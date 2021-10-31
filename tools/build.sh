#!/bin/sh
set -e
cd "$(dirname "$0")"

cat << _EOF > build.ninja
ninja_required_version = 1.3

cflags = $
  -std=c11 $
  -g $
  -fcolor-diagnostics $
  -Wall $
  -Wextra $
  -Wvla $
  -Wimplicit-fallthrough $
  -Wno-missing-field-initializers $
  -Wno-unused-parameter $
  -Werror=implicit-function-declaration $
  -Wcovered-switch-default $
  -Wunused
  #-O1 -march=native

rule cexe
  command = clang \$cflags -o \$out \$in

_EOF

ALL=
# COMMON_SRC=$(echo $(ls *.c))

for d in *; do
  case "$d" in
    .*) continue ;; # skip dot files
  esac
  [ -d "$d" ] || continue # skip non-dirs
  if ! [ -f "$d/$d.c" ]; then # skip dirs withoud dir/dir.c file
    echo "skipping dir $d (does not contain a file $d.c)"
    continue
  fi
  echo "build $d/$d: cexe $(echo $(ls "$d"/*.c))" >> build.ninja
  ALL="$ALL $d/$d"
done

echo "default$ALL" >> build.ninja
ninja "$@"
