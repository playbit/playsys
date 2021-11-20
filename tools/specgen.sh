#!/bin/bash
set -e
cd "$(dirname "$0")/.."

QUIET=false
case "$1" in
  -h*|--h*)
    echo "Reads spec.md and generates playsys API definitions"
    echo "usage: $0 [-help | -quiet]"
    exit ;;
  -quiet) QUIET=true ;;
esac

if $QUIET; then tools/build.sh specgen/specgen >/dev/null
           else tools/build.sh specgen/specgen
           fi
exec tools/specgen/specgen "$@"
