#!/bin/sh
set -e

LLVM_PATH=$("$(dirname "$0")"/../../etc/find-llvm.sh)
OUTPUT_LIBNAME=libplaywgpu_all

if ! [ -d "$1" ]; then
  echo "Usage: $0 <libdir>" >&2
  exit 1
fi

cd "$1"
echo "generating $OUTPUT_LIBNAME.a"
rm -f $OUTPUT_LIBNAME.a
cat << EOF > $OUTPUT_LIBNAME.mri
create $OUTPUT_LIBNAME.a

addlib libplaywgpu.a

addlib $LLVM_PATH/lib/libc++.a
addlib $LLVM_PATH/lib/libc++abi.a

addlib deps/dawn/src/common/libdawn_common.a
addlib deps/dawn/src/dawn/libdawn_headers.a
addlib deps/dawn/src/dawn/libdawn_proc.a
addlib deps/dawn/src/dawn/libdawncpp.a
addlib deps/dawn/src/dawn/libdawncpp_headers.a
addlib deps/dawn/src/dawn_native/libdawn_native.a
addlib deps/dawn/src/dawn_platform/libdawn_platform.a
addlib deps/dawn/src/dawn_wire/libdawn_wire.a
addlib deps/dawn/src/utils/libdawn_utils.a
addlib deps/dawn/third_party/abseil/absl/base/libabsl_base.a
addlib deps/dawn/third_party/abseil/absl/base/libabsl_log_severity.a
addlib deps/dawn/third_party/abseil/absl/base/libabsl_raw_logging_internal.a
addlib deps/dawn/third_party/abseil/absl/base/libabsl_spinlock_wait.a
addlib deps/dawn/third_party/abseil/absl/base/libabsl_throw_delegate.a
addlib deps/dawn/third_party/abseil/absl/numeric/libabsl_int128.a
addlib deps/dawn/third_party/abseil/absl/strings/libabsl_str_format_internal.a
addlib deps/dawn/third_party/abseil/absl/strings/libabsl_strings.a
addlib deps/dawn/third_party/abseil/absl/strings/libabsl_strings_internal.a
addlib deps/dawn/third_party/glfw/src/libglfw3.a
addlib deps/dawn/third_party/spirv-tools/source/libSPIRV-Tools.a
addlib deps/dawn/third_party/spirv-tools/source/opt/libSPIRV-Tools-opt.a
addlib deps/dawn/third_party/tint/src/libtint.a

save
end
EOF

"$LLVM_PATH"/bin/llvm-ar -M <$OUTPUT_LIBNAME.mri
rm $OUTPUT_LIBNAME.mri
ls -lh $OUTPUT_LIBNAME.a

