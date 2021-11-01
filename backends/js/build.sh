#!/bin/sh
set -e
cd "$(dirname "$0")"

ESBUILD_ARGS=( \
  --bundle \
  --format=esm \
  --platform=browser --define:JS_PLATFORM=browser \
)

BUILD_MODE=dev

while [[ $# -gt 0 ]]; do case "$1" in
  -h|-help|--help)
    echo "usage: $0 [options] [arg to esbuild ...]"
    echo "  -opt        Create optimized build"
    echo "  -dev        Create development build (default)"
    echo "  -w, -watch  Rebuild when source files changes"
    echo "  -h, -help   Show help on stdout and exit"
    echo "See esbuild --help for available esbuild options."
    exit 0
    ;;
  -opt)      BUILD_MODE=opt; shift ;;
  -dev)      BUILD_MODE=dev; shift ;;
  -w|-watch) ESBUILD_ARGS+=( --watch ); shift ;;
  *)         ESBUILD_ARGS+=( "$1" ); shift ;;
  --)        break ;; # no shift; make sure "--" is passed on to esbuild
esac; done

if [ "$BUILD_MODE" = "opt" ]; then
  ESBUILD_ARGS+=( --minify --sourcemap --outfile=out/playsys.js )
else
  ESBUILD_ARGS+=( --sourcemap=inline --outfile=out/playsys.dev.js )
fi

exec esbuild "${ESBUILD_ARGS[@]}" "$@" src/web/playsys.ts
