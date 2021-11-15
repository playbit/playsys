#!/bin/sh
set -e
cd "$(dirname "$0")"

ERRSTYLE="\e[1m\e[31m" # bold red
OKSTYLE="\e[2m" # dim
RUN=
CMD=

while [[ $# -gt 0 ]]; do case "$1" in
  -h*|--help)
    echo "usage: $0 [options] [arg to ninja ...]"
    echo "  -h, -help   Show help on stdout and exit"
    echo "  -w, -watch  Rebuild when source files changes"
    echo "  -run        Run <arg1 to ninja> after successful build"
    echo "  -run=<cmd>  Run <cmd> after successful build"
    exit 0
    ;;
  -w|--?watch) CMD=watch; shift ;;
  -run=*)      RUN=${1:5}; shift ;;
  -run)        RUN=1; shift ;;
  *)           break ;;
esac; done

[ "$RUN" != "1" ] || RUN=$1
echo "x $@"

LLVM_PATH=$(../../etc/find-llvm.sh)
echo "Using LLVM at $LLVM_PATH"
export PATH=$LLVM_PATH/bin:$PATH

_run() {
  set +e
  printf "\e[2m> run ${RUN[@]}\e[m\n"
  printf "\e[1m" # bold command output
  "${RUN[@]}"
  local status=$?
  printf "\e[0m"
  local style=$OKSTYLE
  [ $status -eq 0 ] || style=$ERRSTYLE
  printf "${style}> $(basename ${RUN[0]}) exited $status\e[m\n"
  set -e
}

if [ "$CMD" = "watch" ]; then
  trap exit SIGINT  # make sure we can ctrl-c in the while loop
  command -v fswatch >/dev/null || # must have fswatch
    { echo "fswatch not found in PATH" >&2 ; exit 1; }

  while true; do
    echo -e "\x1bc"  # clear screen ("scroll to top" style)
    if ninja "$@"; then
      [ -z "$RUN" ] || _run
    fi
    printf "\e[2m> watching files for changes...\e[m\n"
    fswatch --one-event --extended --latency=0.2 \
            --exclude='.*' --include='\.(c|h|syms|ninja|a|dylib|so)$' \
            . \
            ../../backends/wgpu/out/debug \
            ../../backends/wgpu/include \
            ../../backends/wgpu/src/*wasm* \
            ../../backends/posix/*.c \
            ../../backends/js/*.c \
            ../../include
  done
else
  ninja "$@"
  [ -z "$RUN" ] || _run
fi
