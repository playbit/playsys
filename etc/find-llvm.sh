#!/bin/bash

# if LLVM is installed through some package manager such as aptitute,
# the command wasm-ld is wasm-ld-XX where XX is a LLVM version.
possible_llvm_vs=(wasm-ld wasm-ld-13 wasm-ld-12 wasm-ld-11 wasm-ld-10)
has_llvm_wasmld=false
for pwasm in ${possible_llvm_vs[@]}
do
	if (command -v ${pwasm} >/dev/null); then
  has_llvm_wasmld=true
	echo "$(dirname $(dirname "$(command -v ${pwasm})"))"
	fi
done

if [[ has_llvm_wasmld == false ]]; then
  echo "LLVM with WASM support not found in PATH."
  exit 1
fi

# the clang LLVM compiler will as well be linked to something such as
# clang-XX where XX is a LLVM version.
# this step checks if the user has or not clang.
test_paths=()
has_clang=false
possible_clang_vs=(clang clang-13 clang-12 clang-11 clang-10)
for pclang in ${possible_clang_vs[@]}
do
	if (command -v ${pclang} >/dev/null); then
    X=$(${pclang} -print-search-dirs | grep programs: | awk '{print $2}' | sed 's/^=//')
    if [[ "$X" != "" ]]; then
      has_clang=true
      test_paths+=( "$(dirname "$X")" )
    fi
  fi
done

if [[ has_clang == false ]]; then
  echo "CLANG not found in PATH."
  exit 1
fi

# for better cross-os identification of LLVM test paths.
UNAME=$(uname -s)
if [ "$UNAME" == Linux* ] ; then
	test_paths+=( \
    /usr/lib/llvm-13 \
    /usr/lib/llvm-12 \
    /usr/lib/llvm-11 \
    /usr/lib/llvm-10 \
  )
elif [ "$UNAME" == Darwin* ] ; then
	test_paths+=( \
    /opt/homebrew/opt/llvm \
    /usr/local/opt/llvm \
    /Library/Developer/CommandLineTools/usr \
  )
elif [[ "$UNAME" == CYGWIN* || "$UNAME" == MINGW* ]] ; then
	# TODO! add windows test_paths for LLVM.
  echo ""
fi

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
