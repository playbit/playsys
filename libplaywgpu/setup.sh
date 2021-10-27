#!/bin/sh
set -e
cd "$(dirname "$0")"

PROJ_DIR=$PWD
DEPS_DIR=deps

# dep_git_begin <dir> <git-url> <git-hash>
dep_git_begin() {
  local DIR=$1
  local GIT_URL=$2
  local GIT_TREE=$3
  local FORCE=false
  if [ ! -d "$DIR" ]; then
    git clone "$GIT_URL" "$DIR"
    FORCE=true
  fi
  pushd "$DIR" >/dev/null
  git fetch origin --tags
  local GIT_COMMIT=$(git rev-list -n 1 "$GIT_TREE")
  #echo "GIT_COMMIT $GIT_COMMIT"
  #echo "GIT_HEAD   $(git rev-parse HEAD)"
  if $FORCE || [ "$(git rev-parse HEAD)" != "$GIT_COMMIT" ]; then
    git checkout --detach "$GIT_COMMIT" --
    return 0
  fi
  return 1
}

dep_git_end() {
  popd >/dev/null
}

LLVM_PATH=$(./find-llvm.sh)
export PATH=$LLVM_PATH/bin:$PATH
export CC=$LLVM_PATH/bin/clang
export CXX=$LLVM_PATH/bin/clang++
export LD=$LLVM_PATH/bin/ld.lld
export LDFLAGS="-L$LLVM_PATH/lib -Wl,-rpath,$LLVM_PATH/lib"
export CPPFLAGS="-I$LLVM_PATH/include"

echo "PROJ_DIR=$PROJ_DIR"
echo "LLVM_PATH=$LLVM_PATH"

mkdir -p "$DEPS_DIR"
cd "$DEPS_DIR"


echo "---------- depot_tools ----------"
if dep_git_begin depot_tools \
  https://chromium.googlesource.com/chromium/tools/depot_tools.git \
  557588737ad6f7174cf439b14ba2d27ec00a9535
then
  true
else
  echo "depot_tools: up to date"
fi
export PATH=$PWD:$PATH
echo "export PATH=$PWD:\$PATH"
dep_git_end


echo "---------- dawn ----------"
if dep_git_begin dawn \
  https://dawn.googlesource.com/dawn.git 3faa478ed25f3ec1628d30afe78bacbb535dd28b
then
  cp scripts/standalone.gclient .gclient
  echo gclient sync
       gclient sync
  if [ -d ../../out ]; then
    echo rm -rf ../../out/*/deps/dawn
         rm -rf ../../out/*/deps/dawn
  fi
else
  echo "dawn: up to date"
fi
dep_git_end
