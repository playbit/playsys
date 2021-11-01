#!/bin/bash
cd "$(dirname "$0")"
set -e

_err() { echo "$0: $1" >&2 ; exit 1 ; }

SRC_DIR=$PWD
OUT_DIR=
BUILD_MODE=debug
OPT_CONFIG=false
OPT_WATCH=false
OPT_RUN=
OPT_HELP=false

while [[ $# -gt 0 ]]; do case "$1" in
  -config)         OPT_CONFIG=true; shift ;;
  -opt)            BUILD_MODE=opt; shift ;;
  -w|-watch)       OPT_WATCH=true; shift ;;
  -run=*)          OPT_RUN="${1:5}"; shift ;;
  -outdir=*)       OUT_DIR="${1:8}"; shift ;;
  -h|-help|--help) OPT_HELP=true; shift ;;
  --)              shift; break ;;
  # -*)              _err "Unknown option $1 (see $0 --help)" ;;
  *)               break ;;
esac; done
if $OPT_HELP; then
  cat <<- _EOF_ >&2
usage: $0 [options] [--] [<ninja-arg> ...]
options:
  -config        Reconfigure even if config seems up to date
  -opt           Build optimized build instead of debug build
  -w, -watch     Rebuild when source files change
  -run=<cmd>     Run <cmd> after building
  -outdir=<dir>  Use <dir> instead of out/{debug,opt}
  -h, -help      Show help and exit
_EOF_
  exit 1
fi

[ -n "$OUT_DIR" ] || OUT_DIR=out/$BUILD_MODE

LLVM_PATH=$(../../find-llvm.sh)
echo "Using LLVM at $LLVM_PATH"
export PATH=$LLVM_PATH/bin:$PATH
export CC=$LLVM_PATH/bin/clang
export CXX=$LLVM_PATH/bin/clang++
export LD=$LLVM_PATH/bin/ld.lld
export LDFLAGS="-L$LLVM_PATH/lib -Wl,-rpath,$LLVM_PATH/lib"
export CPPFLAGS="-I$LLVM_PATH/include"

if $OPT_CONFIG || [ ! -f "$OUT_DIR/build.ninja" ]; then
  mkdir -p "$OUT_DIR"
  cd "$OUT_DIR"
  CMAKE_BUILD_TYPE=Debug
  [ "$BUILD_MODE" = "debug" ] || CMAKE_BUILD_TYPE=Release
  cmake -G Ninja -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE "$SRC_DIR"
else
  cd "$OUT_DIR"
fi

_pidfile_kill() {
  local pidfile="$1"
  # echo "_pidfile_kill $1"
  if [ -f "$pidfile" ]; then
    local pid=$(cat "$pidfile" 2>/dev/null)
    # echo "_pidfile_kill pid=$pid"
    [ -z "$pid" ] || kill $pid 2>/dev/null || true
    rm -f "$pidfile"
  fi
}

RUN_PIDFILE="$PWD/run-${$}.pid"
BUILD_LOCKFILE=$PWD/.build.lock

__atexit() {
  set +e
  rm -rf "$BUILD_LOCKFILE"
  _pidfile_kill "$RUN_PIDFILE"
}
trap __atexit EXIT

_onsigint() {
  echo
  exit
}
trap _onsigint SIGINT

_run_after_build() {
  _pidfile_kill "$RUN_PIDFILE"
  set +e
  pushd "$SRC_DIR" >/dev/null
  ( $OPT_RUN &
    echo $! > "$RUN_PIDFILE"
    if [ -n "$RSMS_DEV_SETUP" ]; then
      sleep 0.1
      osascript -e 'activate application "Sublime Text"'
    fi
    wait
    rm -rf "$RUN_PIDFILE" ) &
  popd >/dev/null
  set -e
}

_build() {
  local printed_msg=false
  while true; do
    if { set -C; 2>/dev/null >"$BUILD_LOCKFILE"; }; then
      break
    else
      if ! $printed_msg; then
        printed_msg=true
        # is lockfile stale? (older than 15 minutes)
        if ! find "$BUILD_LOCKFILE" -mmin +15 >/dev/null 2>&1; then
          echo "trashing stale build lock ($OUT_DIR/.build.lock)"
          rm -f "$BUILD_LOCKFILE"
          continue
        fi
        echo "waiting for build lock... ($OUT_DIR/.build.lock)"
      fi
      sleep 0.5
    fi
  done
  ninja "$@"
  local result=$?
  rm -rf "$BUILD_LOCKFILE"
  return $result
}

if ! $OPT_WATCH; then
  _build "$@"
  [ -z "$OPT_RUN" ] || $OPT_RUN
  exit 0
fi

FSWATCH_ARGS=( \
  --exclude='.*' \
  --include='\.(c|cc|cpp|h|hh|hpp|s|S|m|mm)$' \
  "$SRC_DIR"/src \
)

while true; do
  printf "\x1bc"  # clear screen ("scroll to top" style)
  if _build "$@" && [ -n "$OPT_RUN" ]; then
    _run_after_build
  fi
  echo "watching files for changes ..."
  fswatch \
    --one-event \
    --latency=0.2 \
    --extended \
    --recursive \
    "${FSWATCH_ARGS[@]}"
  echo "———————————————————— restarting ————————————————————"
  _pidfile_kill "$RUN_PIDFILE"
done
