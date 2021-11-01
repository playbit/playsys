// enum sys_opcode
const sys_op_test   = 1 // sys_opcode op
const sys_op_exit   = 2 // int status_code
const sys_op_openat = 3 // const char* path, usize flags, isize mode
const sys_op_close  = 4 // sys_fd fd
const sys_op_read   = 5 // sys_fd fd, void* data, usize size
const sys_op_write  = 6 // sys_fd fd, const void* data, usize size
const sys_op_sleep  = 7 // usize seconds, usize nanoseconds

// enum sys_err
const sys_err_none          = 0
const sys_err_badfd         = 1
const sys_err_invalid       = 2 // invalid data or argument
const sys_err_sys_op        = 3 // invalid syscall op or syscall op data
const sys_err_bad_name      = 4
const sys_err_not_found     = 5
const sys_err_name_too_long = 6
const sys_err_canceled      = 7
const sys_err_not_supported = 8
const sys_err_exists        = 9
const sys_err_end           = 10
const sys_err_access        = 11

// enum sys_open_flags
const sys_open_ronly  = 0
const sys_open_wonly  = 1
const sys_open_rw     = 2
const sys_open_append = 1 << 2
const sys_open_create = 1 << 3
const sys_open_trunc  = 1 << 4
const sys_open_excl   = 1 << 5

function assert(cond) {
  if (!cond)
    throw new Error(`assertion failed`)
}


function sys_syscall(proc, op, arg1, arg2, arg3, arg4, arg5) { // -> sys_ret
  // console.log("sys_syscall", {proc, op, arg1, arg2, arg3, arg4, arg5})
  if (proc.suspended)
    return proc.resumeFinalize()
  switch (op) {
    case sys_op_test:   return sys_syscall_test(proc, arg1);
    case sys_op_exit:   return sys_syscall_exit(proc, arg1);
    case sys_op_openat: return sys_syscall_openat(proc, arg1, arg2, arg3, arg4);
    case sys_op_close:  return sys_syscall_close(proc, arg1);
    case sys_op_read:   return sys_syscall_read(proc, arg1, arg2, arg3);
    case sys_op_write:  return sys_syscall_write(proc, arg1, arg2, arg3);
    case sys_op_sleep:  return sys_syscall_sleep(proc, arg1, arg2);
  }
  console.error("invalid sys_syscall op", {proc, op, arg1, arg2, arg3, arg4, arg5})
  return -sys_err_sys_op;
}

function sys_syscall_test(proc, supports_op) {
  if (supports_op > sys_op_write)
    return -sys_err_not_supported
  return 0
}

function sys_syscall_exit(proc, status) {
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
    return -sys_err_not_supported

  const handle = await (
    (flags & sys_open_create) ? showSaveFilePicker() :
                                showOpenFilePicker().then(v => v[0]) )
  let rstream = null
  let wstream = null
  if ((flags & 3) != sys_open_wonly) {
    const file = await handle.getFile()
    rstream = file.stream().getReader()
  }
  if ((flags & 3) != sys_open_ronly) {
    wstream = await handle.createWritable()
    if (flags & sys_open_trunc)
      await wstream.write({ type: "truncate", size: 0 })
  }
  const fd = proc.allocFd()
  proc.files[fd] = new WebFSFile(proc, fd, flags, handle, rstream, wstream)
  return fd
}


function sys_syscall_openat(proc, basefd, pathptr, flags, mode) {
  const path = proc.cstr(pathptr)
  if (path.length == 0)
    return -sys_err_invalid

  let fent = fake_fs.findEntry(path)
  if (fent) {
    if (flags & sys_open_excl)
      return -sys_err_exists
    const fd = proc.allocFd()
    const file = new MemoryFile(proc, fd, flags, fent)
    proc.files[fd] = file
    if (flags & sys_open_trunc) {
      const r = file.truncate(0)
      if (r < 0) {
        proc.freeFd(fd)
        return r
      }
    }
    if (flags & sys_open_append) {
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
    const code = (err.name == "AbortError") ? sys_err_canceled : sys_err_invalid
    proc.resume(-code)
  })
}

function sys_syscall_close(proc, fd) {
  const file = proc.files[fd]
  if (!file)
    return -sys_err_badfd
  proc.freeFd(fd)
  return file.close()
}

function sys_syscall_read(proc, fd, dataptr, datalen) {
  const file = proc.files[fd]
  if (!file)
    return -sys_err_badfd
  if (!file.isReadable())
    return -sys_err_invalid // not writable
  const result = file.read(dataptr, datalen)
  if (!(result instanceof Promise))
    return result
  proc.suspend()
  result
    .then(result => proc.resume(result))
    .catch(err => {
      // console.error("sys_syscall_read caught async exception:", err, {err})
      proc.resume(-sys_err_canceled)
    })
}

function sys_syscall_write(proc, fd, dataptr, datalen) {
  const file = proc.files[fd]
  if (!file)
    return -sys_err_badfd
  if (!file.isWritable())
    return -sys_err_invalid // not writable
  return file.write(dataptr, datalen)
}

function sys_syscall_sleep(proc, seconds, nanoseconds) {
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

  isReadable() { return (this.flags & 3) != sys_open_wonly }
  isWritable() { return (this.flags & 3) != sys_open_ronly }

  read(dstptr, cap) { return -sys_err_invalid }
  write(srcptr, len) { return -sys_err_invalid }
  seek(offset, whence) { return -sys_err_invalid }
  truncate(size) { return -sys_err_invalid }

  flush(){ return -sys_err_invalid }
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
      return -sys_err_invalid
    }
    if (this.pos > this.size()) {
      this.pos = this.size()
      return -sys_err_invalid
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
        return -sys_err_access // Permission is not granted.
      return -sys_err_canceled
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
  write(srcptr, len) { // -> sys_ret nbytes_written
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
      return -sys_err_end
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
  write(ptr, len) { // -> sys_ret nbytes_written
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
      1: /*stdout*/ new LineWriterFile(this, 1, sys_open_wonly, s => this.onStdoutLine(s)),
      2: /*stderr*/ new LineWriterFile(this, 2, sys_open_wonly, s => this.onStderrLine(s)),
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
      sys_syscall: sys_syscall.bind(null, proc),
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
//     return -sys_err_not_supported

//   // allocate new file
//   file = new HostFile(proc, proc.files.length, flags)
//   file.promise = (
//     flags & sys_open_create ? showSaveFilePicker() :
//     showOpenFilePicker()
//   ).catch(err => {
//     console.error("syscall open", err.message, {path,mode,flags})
//   })
// }
