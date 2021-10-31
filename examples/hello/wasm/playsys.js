const ISIZE = 4 // sizeof(isize) (wasm32=4, wasm64=8)

// enum _p_err
const p_err_none          =   0 // no error
const p_err_badfd         =  -1 // invalid file descriptor
const p_err_invalid       =  -2 // invalid data or argument
const p_err_sys_op        =  -3 // invalid syscall op or syscall op data
const p_err_bad_name      =  -4 // invalid or misformed name
const p_err_not_found     =  -5 // resource not found
const p_err_name_too_long =  -6
const p_err_canceled      =  -7 // operation canceled
const p_err_not_supported =  -8 // functionality not supported
const p_err_exists        =  -9 // already exists
const p_err_end           = -10 // end of resource
const p_err_access        = -11 // permission denied

// enum _p_oflag
const p_open_ronly  = 0  // Open for reading only
const p_open_wonly  = 1  // Open for writing only
const p_open_rw     = 2  // Open for both reading and writing
const p_open_append = 4  // Start writing at end (seekable files only)
const p_open_create = 8  // Create file if it does not exist
const p_open_trunc  = 16 // Set file size to zero
const p_open_excl   = 32 // fail if file exists when create and excl are set

// enum _p_sysop
const p_sysop_openat   = 257   // base fd, path cstr, flags oflag, mode usize
const p_sysop_close    = 3     // fd fd
const p_sysop_read     = 0     // fd fd, data mutptr, nbyte usize
const p_sysop_write    = 1     // fd fd, data ptr, nbyte usize
const p_sysop_seek     = 8     // TODO
const p_sysop_statat   = 262   // TODO (newfstatat in linux, alt: statx 332)
const p_sysop_removeat = 263   // base fd, path cstr, flags usize
const p_sysop_renameat = 264   // oldbase fd, oldpath cstr, newbase fd, newpath cstr
const p_sysop_sleep    = 230   // seconds usize, nanoseconds usize
const p_sysop_exit     = 60    // status_code i32
const p_sysop_test     = 10000 // op psysop


function assert(cond) {
  if (!cond)
    throw new Error(`assertion failed`)
}


function p_syscall(proc, op, arg1, arg2, arg3, arg4, arg5) { // -> isize
  // console.log("syscall", {proc, op, arg1, arg2, arg3, arg4, arg5})
  if (proc.suspended)
    return proc.resumeFinalize()
  switch (op) {
    case p_sysop_test:   return syscall_test(proc, arg1, arg2, arg3, arg4, arg5);
    case p_sysop_exit:   return syscall_exit(proc, arg1, arg2, arg3, arg4, arg5);
    case p_sysop_openat: return syscall_openat(proc, arg1, arg2, arg3, arg4, arg5);
    case p_sysop_close:  return syscall_close(proc, arg1, arg2, arg3, arg4, arg5);
    case p_sysop_read:   return syscall_read(proc, arg1, arg2, arg3, arg4, arg5);
    case p_sysop_write:  return syscall_write(proc, arg1, arg2, arg3, arg4, arg5);
    case p_sysop_sleep:  return syscall_sleep(proc, arg1, arg2, arg3, arg4, arg5);
  }
  console.error("invalid syscall op", {proc, op, arg1, arg2, arg3, arg4, arg5})
  return p_err_sys_op;
}


function syscall_test(proc, supports_op) {
  if (supports_op > p_sysop_write)
    return p_err_not_supported
  return 0
}

function syscall_exit(proc, status) {
  const err = new Error("exit " + status)
  err.name = "WasmExit"
  err.status = status
  throw err // only way to interrupt wasm in a JS runtime
  return 0
}


async function open_web_fs(proc, pathptr, flags, mode) {
  // see https://web.dev/file-system-access/
  // see https://developer.mozilla.org/en-US/docs/Web/API/window/showSaveFilePicker
  if (window.showSaveFilePicker === undefined)
    return p_err_not_supported

  const handle = await (
    (flags & p_open_create) ? showSaveFilePicker() :
                                showOpenFilePicker().then(v => v[0]) )
  let rstream = null
  let wstream = null
  if ((flags & 3) != p_open_wonly) {
    const file = await handle.getFile()
    rstream = file.stream().getReader()
  }
  if ((flags & 3) != p_open_ronly) {
    wstream = await handle.createWritable()
    if (flags & p_open_trunc)
      await wstream.write({ type: "truncate", size: 0 })
  }
  const fd = proc.allocFd()
  proc.files[fd] = new WebFSFile(proc, fd, flags, handle, rstream, wstream)
  return fd
}


function syscall_openat(proc, basefd, pathptr, flags, mode) {
  const path = proc.cstr(pathptr)
  if (path.length == 0)
    return p_err_invalid

  let fent = fake_fs.findEntry(path)
  if (fent) {
    if (flags & p_open_excl)
      return p_err_exists
    const fd = proc.allocFd()
    const file = new MemoryFile(proc, fd, flags, fent)
    proc.files[fd] = file
    if (flags & p_open_trunc) {
      const r = file.truncate(0)
      if (r < 0) {
        proc.freeFd(fd)
        return r
      }
    }
    if (flags & p_open_append) {
      const r = file.seek(0, sys_seek_end)
      if (r < 0) {
        proc.freeFd(fd)
        return r
      }
    }
    return fd
  }

  proc.suspend()
  open_web_fs(proc, pathptr, flags, mode).then(fd => {
    proc.resume(fd)
  }).catch(err => {
    // console.error("open_web_fs failed:", err, {err})
    const code = (err.name == "AbortError") ? p_err_canceled : p_err_invalid
    proc.resume(-code)
  })
}

function syscall_close(proc, fd) {
  const file = proc.files[fd]
  if (!file)
    return p_err_badfd
  proc.freeFd(fd)
  return file.close()
}

function syscall_read(proc, fd, dataptr, datalen) {
  const file = proc.files[fd]
  if (!file)
    return p_err_badfd
  if (!file.isReadable())
    return p_err_invalid // not writable
  const result = file.read(dataptr, datalen)
  if (!(result instanceof Promise))
    return result
  proc.suspend()
  result
    .then(result => proc.resume(result))
    .catch(err => {
      // console.error("syscall_read caught async exception:", err, {err})
      proc.resume(-p_err_canceled)
    })
}

function syscall_write(proc, fd, dataptr, datalen) {
  const file = proc.files[fd]
  if (!file)
    return p_err_badfd
  if (!file.isWritable())
    return p_err_invalid // not writable
  return file.write(dataptr, datalen)
}

function syscall_sleep(proc, seconds, nanoseconds) {
  proc.suspend()
  const milliseconds = seconds*1000 + nanoseconds/1000000
  setTimeout(() => { proc.resume(0) }, milliseconds);
}

// -------------------------------------------------------------------------

// note: nodejs has require("util").TextEncoder & .TextDecoder
const txt_enc = new TextEncoder("utf-8")
const txt_dec = new TextDecoder("utf-8")

class FileBase {
  constructor(proc, fd, flags) {
    this.proc = proc
    this.fd = fd
    this.flags = flags
  }

  isReadable() { return (this.flags & 3) != p_open_wonly }
  isWritable() { return (this.flags & 3) != p_open_ronly }

  read(dstptr, cap) { return p_err_invalid }
  write(srcptr, len) { return p_err_invalid }
  seek(offset, whence) { return p_err_invalid }
  truncate(size) { return p_err_invalid }

  flush(){ return p_err_invalid }
  close() { return 0 }
  size() { return -1 }
}

class SeekableFile extends FileBase {
  constructor(proc, fd, flags) {
    super(proc, fd, flags)
    this.pos = 0
  }
  seek(offset, whence) {
    const sys_seek_set = 1
    const sys_seek_current = 1
    const sys_seek_end = 1
    switch (whence) {
      case sys_seek_set:     this.pos = offset; break
      case sys_seek_current: this.pos += offset; break
      case sys_seek_end:     this.pos = this.size(); break
    }
    if (this.pos < 0) {
      this.pos = 0
      return p_err_invalid
    }
    if (this.pos > this.size()) {
      this.pos = this.size()
      return p_err_invalid
    }
    return 0
  }
}

class WebFSFile extends SeekableFile {
  constructor(proc, fd, flags, handle, rstream, wstream) {
    super(proc, fd, flags)
    this.handle = handle   // FileSystemFileHandle
    this.rstream = rstream // ReadableStreamDefaultReader | null
    this.wstream = wstream // FileSystemWritableFileStream | null
    this.rbuf = null       // queued UInt8Array
  }

  async read(dstptr, len) {
    let wptr = dstptr

    const write = (buf) => {
      const rlen = wptr - dstptr // bytes read so far
      const n = len - rlen       // remaining number of bytes to read
      let remainder = null
      if (buf.length > n) {
        remainder = buf.subarray(n)
        buf = buf.subarray(0, n)
      }
      this.proc.mem_u8.set(buf, wptr)
      wptr += buf.length
      return remainder
    }

    if (this.rbuf)
      this.rbuf = write(this.rbuf)

    while (wptr - dstptr < len) {
      const { done, value } = await this.rstream.read() // value is a UInt8Array
      if (done)
        break
      this.rbuf = write(value)
    }
    return wptr - dstptr
  }

  async write(srcptr, len) {
    const data = this.proc.mem_u8.subarray(srcptr, srcptr + len)
    try {
      await this.wstream.write({ type: "write", data })
      return len
    } catch (err) {
      if (err.name == "NotAllowedError")
        return p_err_access // Permission is not granted.
      return p_err_canceled
    }
  }

  async close() {
    if (this.rstream) {
      this.rstream.cancel("file closed")
      this.rstream.releaseLock()
    }
    if (this.wstream)
      await this.wstream.close()
  }
}

class MemoryFile extends SeekableFile {
  constructor(proc, fd, flags, fent) {
    super(proc, fd, flags)
    this.fent = fent
  }
  size() { return this.fent.size }
  write(srcptr, len) { // -> isize nbytes_written
    if (this.pos + len > this.fent.buf.length) {
      const prevbuf = this.fent.buf
      this.fent.buf = new Uint8Array(align2(prevbuf.length + len, 4096))
      this.fent.buf.set(prevbuf.subarray(0, this.pos))
    }
    this.fent.buf.set(this.proc.mem_u8.subarray(srcptr, srcptr + len), this.pos)
    this.pos += len
    this.fent.size = Math.max(this.fent.size, this.pos)
    return len
  }
  read(dstptr, cap) {
    if (this.pos >= this.fent.size)
      return p_err_end
    const len = Math.min(this.fent.size - this.pos, cap)
    const src = this.fent.buf.subarray(this.pos, this.pos + len)
    this.proc.mem_u8.set(src, dstptr)
    this.pos += len
    return len
  }
}

class LineWriterFile extends FileBase {
  constructor(proc, fd, flags, onLine) {
    super(proc, fd, flags)
    this.buf = ""
    this.onLine = onLine
  }
  write(ptr, len) { // -> isize nbytes_written
    this.buf += this.proc.str(ptr, len)
    const nl = this.buf.lastIndexOf("\n")
    if (nl != -1) {
      this.onLine(this.buf.substr(0, nl))
      this.buf = this.buf.substr(nl + 1)
    }
    return len
  }
  flush() {
    const len = this.buf.length
    if (len > 0) {
      this.onLine(this.buf)
      this.buf = ""
    }
    return len
  }
}


// fake file system
class FakeFSEntry {}
class FakeFS {
  constructor() {
    this.entries = Object.create(null)
    const uname = `web-${navigator.appName} 1\n`
    this.createEntry("/sys/uname", 0o444, txt_enc.encode(uname))
  }
  findEntry(path) {
    return this.entries[path]
  }
  createEntry(path, mode, buf) {
    const entry = {
      path,
      mode,
      size: buf ? buf.length : 0,
      mtime: Date.now()/1000,
      buf: buf || new Uint8Array(4096),
    }
    this.entries[path] = entry
    return entry
  }
  deleteEntry(path) {
    if (!this.entries[entry])
      return false
    delete this.entries[entry]
    return true
  }
}

var fake_fs = new FakeFS()


class Process {
  constructor(memory) {
    this.memory = memory
    this.mem_u8 = new Uint8Array(memory.buffer)
    this.mem_i32 = new Int32Array(memory.buffer)
    this.mem_u32 = new Uint32Array(memory.buffer)
    this.instance = null
    this.mainfun = null
    this.main_resolve = null
    this.main_reject = null
    this.cwd = "/"
    this.free_fds = []
    this.max_fd = 2
    this.suspended = false
    this.suspend_data_addr = 16 // where the unwind/rewind data will live
    this.suspend_stack_size = 1024
    this.suspend_result = null // resumed return value
    this.main_args_addr = this.suspend_data_addr + this.suspend_stack_size
    this.onStdoutLine = console.log.bind(console)
    this.onStderrLine = console.error.bind(console)
    this.files = { // open file handles, indexed by fd
      0: /*stdin*/  new FileBase(this, 0, 0), // invalid
      1: /*stdout*/ new LineWriterFile(this, 1, p_open_wonly, s => this.onStdoutLine(s)),
      2: /*stderr*/ new LineWriterFile(this, 2, p_open_wonly, s => this.onStderrLine(s)),
    }
  }

  run(main_args) {
    return new Promise((resolve, reject) => {
      //console.log("wasm exports:\n  " + Object.keys(instance.exports).sort().join("\n  "))
      this.mainfun = this.instance.exports.main || this.instance.exports.__main_argc_argv
      this.main_resolve = resolve
      this.main_reject = reject
      const argc = 0
      const argv = 0
      // TODO: main args
      // const argc = main_args.length
      // const argv = this.main_args_addr // start address of argv
      // let argvp = argv
      // for (let arg of main_args) {
      //   let srcbuf = txt_enc.encode(String(arg))
      //   this.mem_u8.set(srcbuf, argvp)
      //   argvp +=
      // }
      this.callMain(argc, argv)
    })
  }

  callMain(argc, argv) {
    try {
      const retval = this.mainfun(argc, argv)
      const sstate = this.suspendState()
      if (sstate == 0) {
        assert(this.suspendState() == 0)
        this.finalize()
        this.main_resolve(retval)
      } else {
        assert(sstate == 1)
        this.instance.exports.asyncify_stop_unwind()
      }
    } catch (err) {
      this.suspended = false
      // TODO: figure out if we need to clean up asyncify state:
      // const sstate = this.suspendState()
      // if (sstate == 1) {
      //   this.instance.exports.asyncify_stop_unwind()
      // } else if (sstate == 2) {
      //   this.instance.exports.asyncify_stop_rewind()
      // }
      this.finalize()
      if (err.name == "WasmExit") {
        this.main_resolve(err.status)
      } else {
        this.main_reject(err)
      }
    }
  }

  finalize() {
    for (let fd in this.files) {
      const file = this.files[fd]
      if (file.isWritable())
        file.flush()
    }
  }

  allocFd() {
    if (this.free_fds.length)
      return this.free_fds.pop()
    this.max_fd++
    return this.max_fd
  }

  freeFd(fd) {
    delete this.files[fd]
    this.free_fds.push(fd)
  }

  u8(ptr)  { return this.mem_u8[ptr] }

  i32(ptr) { return this.mem_i32[ptr >>> 2] }
  u32(ptr) { return this.mem_u32[ptr >>> 2] >>> 0 }

  setI32(ptr, v) { this.mem_u32[ptr >>> 2] = v }
  setU32(ptr, v) { this.mem_u32[ptr >>> 2] = (v >>> 0) }

  // u64(ptr) {
  //   BigInt ...
  // }

  isize(ptr) { return this.mem_i32[ptr >>> 2] }
  usize(ptr) { return this.mem_u32[ptr >>> 2] >>> 0 }

  str(ptr, len) {
    return txt_dec.decode(new DataView(this.memory.buffer, ptr, len))
  }
  cstr(ptr) {
    const len = this.mem_u8.indexOf(0, ptr) - ptr
    return txt_dec.decode(new DataView(this.memory.buffer, ptr, len))
  }
  setStr(ptr, cap, jsstr) {
    let srcbuf = txt_enc.encode(String(jsstr))
    if (srcbuf.length > cap)
      srcbuf = srcbuf.subarray(0, cap)
    this.mem_u8.set(srcbuf, ptr)
    return srcbuf.length
  }

  // Process suspension via Binaryen asyncify
  // Usage synopsis:
  //   function foo(arg) {
  //     if (proc.suspended) // woke up from resume()
  //       return proc.resumeFinalize()
  //     // Your function may complete immediately if it wants
  //     if (done_imm)
  //       return 123
  //     // Or suspend execution for a while
  //     proc.suspend()
  //     setTimeout(() => {
  //       let result = 123
  //       proc.resume(result)
  //     },100)
  //   }
  //
  // See https://kripken.github.io/blog/wasm/2019/07/16/asyncify.html
  // See https://yingtongli.me/blog/2021/07/28/asyncify-vanilla.html
  //
  suspend() {
    assert(this.suspendState() == 0) // must not be suspended
    // Fill in the data structure. The first value has the stack location,
    // which for simplicity we can start right after the data structure itself
    this.mem_i32[this.suspend_data_addr >> 2] = this.suspend_data_addr + 8
    this.mem_i32[this.suspend_data_addr + 4 >> 2] = this.suspend_stack_size // end of stack
    this.instance.exports.asyncify_start_unwind(this.suspend_data_addr)
    this.suspended = true
  }

  resume(result) {
    assert(this.suspendState() != 2) // must not be rewinding
    this.suspend_result = result
    this.instance.exports.asyncify_start_rewind(this.suspend_data_addr)
    // The code is now ready to rewind; to start the process, enter the
    // first function that should be on the call stack.
    this.callMain()
  }

  resumeFinalize() {
    assert(this.suspendState() == 2) // must be rewinding
    this.instance.exports.asyncify_stop_rewind()
    this.suspended = false
    return this.suspend_result
  }

  // 0 = normal, 1 = unwinding, 2 = rewinding
  suspendState() { return this.instance.exports.asyncify_get_state() }

  mkasync(f) {
    const proc = this
    return function() {
      if (!proc.suspended) {
        proc.suspend()
        f.call(proc, proc.resume.bind(proc), ...arguments)
      } else {
        return proc.resumeFinalize()
      }
    }
  }
}


const wasm_instantiate = (
  WebAssembly.instantiateStreaming || ((res, import_obj) =>
    res.then(r => r.arrayBuffer()).then(buf => WebAssembly.instantiate(buf, import_obj)))
)


export async function wasm_load(url) {
  const fetch_promise = url instanceof Promise ? url : fetch(url)
  const memory = new WebAssembly.Memory({ initial: 32 /*pages*/ })
  const proc = new Process(memory)
  const import_obj = {
    env: {
      memory,
      p_syscall: p_syscall.bind(null, proc),
    },
  }
  const { instance } = await wasm_instantiate(fetch_promise, import_obj)
  proc.instance = instance
  return proc
}


// align2 rounds up unsigned integer u to closest a where a must be a power of two.
// E.g.
//   align(0, 4) => 0
//   align(1, 4) => 4
//   align(2, 4) => 4
//   align(3, 4) => 4
//   align(4, 4) => 4
//   align(5, 4) => 8
//   ...
function align2(u,a) {
  a = (a >>> 0) - 1
  return (((u >>> 0) + a) & ~a) >>> 0
}


// function TODO_open_web_fs() {
//   // see https://web.dev/file-system-access/
//   // see https://developer.mozilla.org/en-US/docs/Web/API/window/showSaveFilePicker

//   if (window.showSaveFilePicker === undefined)
//     return p_err_not_supported

//   // allocate new file
//   file = new HostFile(proc, proc.files.length, flags)
//   file.promise = (
//     flags & p_open_create ? showSaveFilePicker() :
//     showOpenFilePicker()
//   ).catch(err => {
//     console.error("syscall open", err.message, {path,mode,flags})
//   })
// }
