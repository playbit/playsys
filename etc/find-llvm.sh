#!/bin/sh

if command -v wasm-ld >/dev/null; then
  echo "$(dirname $(dirname "$(command -v wasm-ld)"))"
  exit 0
fi

test_paths=()
if (command -v clang >/dev/null); then
  X=$(clang -print-search-dirs | grep programs: | awk '{print $2}' | sed 's/^=//')
  if [[ "$X" != "" ]]; then
    test_paths+=( "$(dirname "$X")" )
  fi
fi

case $(uname -s) in
  Darwin*)
    test_paths+=( /usr/local/opt/llvm )
    test_paths+=( /Library/Developer/CommandLineTools/usr )
    ;;
esac

for X in ${test_paths[@]}; do
  # echo "try $X/bin/wasm-ld" >&2
  if [[ -f "$X/bin/wasm-ld" ]]; then
    echo $X
    exit 0
  fi
done

echo "LLVM with WASM support not found in PATH. Also searched:" >&2
for X in ${test_paths[@]}; do
  echo "  $X" >&2
done
echo "Please set PATH to include LLVM with WASM backend" >&2
exit 1
