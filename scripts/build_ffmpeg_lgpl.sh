#!/usr/bin/env bash
set -euo pipefail

FFMPEG_VERSION="${FFMPEG_VERSION:-8.0.1}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/.ffmpeg_build"
SRC_ARCHIVE="${BUILD_DIR}/ffmpeg-${FFMPEG_VERSION}.tar.xz"
SRC_DIR="${BUILD_DIR}/ffmpeg-${FFMPEG_VERSION}"
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"

ANDROID_NDK="${ANDROID_NDK:-${ANDROID_NDK_HOME:-${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}/ndk/28.2.13676358}}"
ANDROID_API="${ANDROID_API:-24}"

COMMON_FLAGS=(
  --disable-gpl
  --disable-nonfree
  --enable-shared
  --disable-static
  --disable-programs
  --disable-doc
  --disable-debug
  --disable-autodetect
  --disable-avdevice
  --disable-network
  --disable-everything
  --enable-avcodec
  --enable-avformat
  --enable-avutil
  --enable-swscale
  --enable-swresample
  --enable-zlib
  --enable-encoder=png
  --enable-muxer=image2
  --enable-protocol=file
  --enable-demuxer=mov
  --enable-demuxer=matroska
  --enable-demuxer=webm_dash_manifest
  --enable-parser=aac
  --enable-parser=h264
  --enable-parser=hevc
  --enable-parser=vp8
  --enable-parser=vp9
  --enable-decoder=aac
  --enable-decoder=h264
  --enable-decoder=hevc
  --enable-decoder=mpeg4
  --enable-decoder=vp8
  --enable-decoder=vp9
  --disable-asm
)

FFMPEG_LIBS=(
  avcodec
  avformat
  avutil
  swscale
  swresample
)

copy_ffmpeg_headers() {
  local source_include_dir="$1"
  local target_include_dir="$2"
  local include_dir

  rm -rf "${target_include_dir}"
  mkdir -p "${target_include_dir}"
  for include_dir in libavcodec libavformat libavutil libswscale libswresample; do
    cp -R "${source_include_dir}/${include_dir}" "${target_include_dir}/"
  done
}

download_source() {
  mkdir -p "${BUILD_DIR}"
  if [[ ! -f "${SRC_ARCHIVE}" ]]; then
    curl -L --fail "${FFMPEG_URL}" -o "${SRC_ARCHIVE}"
  fi
  if [[ ! -d "${SRC_DIR}" ]]; then
    tar -xf "${SRC_ARCHIVE}" -C "${BUILD_DIR}"
  fi
}

build_android_arm64() {
  local target_dir="${ROOT_DIR}/android/src/main/jniLibs/arm64-v8a"
  local prefix="${BUILD_DIR}/android-arm64"
  local toolchain="${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64"
  if [[ ! -d "${toolchain}" ]]; then
    toolchain="${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-arm64"
  fi
  if [[ ! -d "${toolchain}" ]]; then
    echo "Android NDK toolchain not found under ${ANDROID_NDK}" >&2
    exit 1
  fi

  pushd "${SRC_DIR}" >/dev/null
  make distclean >/dev/null 2>&1 || true
  ./configure \
    --prefix="${prefix}" \
    --target-os=android \
    --arch=aarch64 \
    --cpu=armv8-a \
    --cross-prefix="${toolchain}/bin/aarch64-linux-android-" \
    --cc="${toolchain}/bin/aarch64-linux-android${ANDROID_API}-clang" \
    --cxx="${toolchain}/bin/aarch64-linux-android${ANDROID_API}-clang++" \
    --strip="${toolchain}/bin/llvm-strip" \
    --extra-ldflags="-Wl,-z,max-page-size=16384" \
    "${COMMON_FLAGS[@]}"
  make -j"$(sysctl -n hw.ncpu)"
  make install
  popd >/dev/null

  mkdir -p "${target_dir}"
  rm -f "${target_dir}"/lib*.so
  local lib
  for lib in "${FFMPEG_LIBS[@]}"; do
    cp "${prefix}/lib/lib${lib}.so" "${target_dir}/"
  done
  copy_ffmpeg_headers "${prefix}/include" "${ROOT_DIR}/android/src/main/cpp/include"
}

build_ios_arch() {
  local sdk="$1"
  local arch="$2"
  local platform="$3"
  local prefix="${BUILD_DIR}/ios-${platform}-${arch}"
  local sdk_path
  sdk_path="$(xcrun --sdk "${sdk}" --show-sdk-path)"
  local cc
  cc="$(xcrun --sdk "${sdk}" --find clang)"

  pushd "${SRC_DIR}" >/dev/null
  make distclean >/dev/null 2>&1 || true
  env -u CPATH -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH -u LIBRARY_PATH -u PKG_CONFIG_PATH ./configure \
    --prefix="${prefix}" \
    --target-os=darwin \
    --arch="${arch}" \
    --enable-cross-compile \
    --cc="${cc}" \
    --sysroot="${sdk_path}" \
    --extra-cflags="-arch ${arch} -m${platform}-version-min=13.0" \
    --extra-ldflags="-arch ${arch} -m${platform}-version-min=13.0" \
    --install-name-dir='@rpath' \
    "${COMMON_FLAGS[@]}"
  make -j"$(sysctl -n hw.ncpu)"
  make install
  popd >/dev/null
}

# 返回 FFmpeg 动态库的版本化文件名。
ios_dylib_file() {
  local lib="$1"

  local dylib_link="${BUILD_DIR}/ios-iphoneos-arm64/lib/lib${lib}.dylib"
  if [[ ! -L "${dylib_link}" ]]; then
    echo "Missing LGPL FFmpeg dylib symlink: ${dylib_link}" >&2
    exit 1
  fi

  readlink "${dylib_link}"
}

# 写入 iOS 动态 framework 所需的最小 Info.plist。
write_ios_framework_plist() {
  local framework_path="$1"
  local lib="$2"

  cat >"${framework_path}/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>lib${lib}</string>
  <key>CFBundleIdentifier</key>
  <string>org.ffmpeg.lib${lib}</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleName</key>
  <string>lib${lib}</string>
  <key>CFBundlePackageType</key>
  <string>FMWK</string>
  <key>CFBundleShortVersionString</key>
  <string>${FFMPEG_VERSION}</string>
  <key>CFBundleVersion</key>
  <string>${FFMPEG_VERSION}</string>
  <key>MinimumOSVersion</key>
  <string>13.0</string>
</dict>
</plist>
PLIST
}

# 创建 CocoaPods 支持嵌入和签名的动态 framework slice。
create_ios_framework_slice() {
  local lib="$1"
  local source_prefix="$2"
  local output_dir="$3"
  local dylib_file
  dylib_file="$(ios_dylib_file "${lib}")"
  local framework_path="${output_dir}/lib${lib}.framework"

  rm -rf "${framework_path}"
  mkdir -p "${framework_path}/Headers" "${framework_path}/Modules"
  cp "${source_prefix}/lib/${dylib_file}" "${framework_path}/lib${lib}"
  copy_ffmpeg_headers "${source_prefix}/include" "${framework_path}/Headers"
  write_ios_framework_plist "${framework_path}" "${lib}"
  cat >"${framework_path}/Modules/module.modulemap" <<MODULEMAP
framework module lib${lib} {
  umbrella "Headers"
  export *
  module * { export * }
}
MODULEMAP

  install_name_tool -id "@rpath/lib${lib}.framework/lib${lib}" "${framework_path}/lib${lib}"
}

# 将 FFmpeg 动态库之间的依赖改为 framework rpath。
rewrite_ios_framework_dependencies() {
  local binary_path="$1"
  local dep_lib
  for dep_lib in "${FFMPEG_LIBS[@]}"; do
    local dep_dylib
    dep_dylib="$(ios_dylib_file "${dep_lib}")"
    local major_dylib="${dep_dylib%.*.*.dylib}.dylib"
    install_name_tool \
      -change "@rpath/${major_dylib}" "@rpath/lib${dep_lib}.framework/lib${dep_lib}" \
      "${binary_path}" 2>/dev/null || true
    install_name_tool \
      -change "@rpath/${dep_dylib}" "@rpath/lib${dep_lib}.framework/lib${dep_lib}" \
      "${binary_path}" 2>/dev/null || true
    install_name_tool \
      -change "${dep_dylib}" "@rpath/lib${dep_lib}.framework/lib${dep_lib}" \
      "${binary_path}" 2>/dev/null || true
  done
}

# 校验生成结果仍为动态 framework，而不是静态链接产物。
verify_ios_framework() {
  local lib="$1"
  local framework_path="$2"
  local binary_path="${framework_path}/lib${lib}"

  if [[ ! -f "${binary_path}" ]]; then
    echo "Missing LGPL FFmpeg framework binary: ${binary_path}" >&2
    exit 1
  fi

  if ! otool -D "${binary_path}" | grep -F -q "@rpath/lib${lib}.framework/lib${lib}"; then
    echo "Unexpected LGPL FFmpeg install name for ${binary_path}" >&2
    exit 1
  fi
}

# 将每个 LGPL FFmpeg shared library 包装成动态 framework 再生成 XCFramework。
build_ios_library_xcframework() {
  local lib="$1"
  local framework_dir="$2"
  local staging_dir="${BUILD_DIR}/ios-frameworks/lib${lib}"
  local device_framework="${staging_dir}/iphoneos/lib${lib}.framework"
  local simulator_framework="${staging_dir}/ios-simulator/lib${lib}.framework"

  create_ios_framework_slice "${lib}" "${BUILD_DIR}/ios-iphoneos-arm64" "${staging_dir}/iphoneos"
  create_ios_framework_slice "${lib}" "${BUILD_DIR}/ios-ios-simulator-arm64" "${staging_dir}/ios-simulator"
  rewrite_ios_framework_dependencies "${device_framework}/lib${lib}"
  rewrite_ios_framework_dependencies "${simulator_framework}/lib${lib}"
  verify_ios_framework "${lib}" "${device_framework}"
  verify_ios_framework "${lib}" "${simulator_framework}"

  rm -rf "${framework_dir}/lib${lib}.xcframework"
  xcodebuild -create-xcframework \
    -framework "${device_framework}" \
    -framework "${simulator_framework}" \
    -output "${framework_dir}/lib${lib}.xcframework"
}

package_ios_xcframeworks() {
  local framework_dir="${ROOT_DIR}/ios/Frameworks"
  mkdir -p "${framework_dir}"
  rm -rf "${BUILD_DIR}/ios-frameworks"

  for lib in "${FFMPEG_LIBS[@]}"; do
    build_ios_library_xcframework "${lib}" "${framework_dir}"
  done
}

build_ios_xcframework() {
  build_ios_arch iphoneos arm64 iphoneos
  build_ios_arch iphonesimulator arm64 ios-simulator
  package_ios_xcframeworks
}

write_compliance_files() {
  printf "ffmpeg-%s\n" "${FFMPEG_VERSION}" >"${ROOT_DIR}/third_party/ffmpeg/VERSION"
  printf "%s\n" "${FFMPEG_URL}" >"${ROOT_DIR}/third_party/ffmpeg/source-url.txt"
  cat >"${ROOT_DIR}/third_party/ffmpeg/configure_args.txt" <<'ARGS'
Required LGPL-oriented configure flags used by scripts/build_ffmpeg_lgpl.sh:

--disable-gpl
--disable-nonfree
--enable-shared
--disable-static
--disable-programs
--disable-doc
--disable-debug
--disable-autodetect
--disable-avdevice
--disable-network
--disable-everything
--enable-avcodec
--enable-avformat
--enable-avutil
--enable-swscale
--enable-swresample
--enable-zlib
--enable-encoder=png
--enable-muxer=image2
--enable-protocol=file
--enable-demuxer=mov
--enable-demuxer=matroska
--enable-demuxer=webm_dash_manifest
--enable-parser=aac
--enable-parser=h264
--enable-parser=hevc
--enable-parser=vp8
--enable-parser=vp9
--enable-decoder=aac
--enable-decoder=h264
--enable-decoder=hevc
--enable-decoder=mpeg4
--enable-decoder=vp8
--enable-decoder=vp9
--disable-asm

Forbidden flags and libraries:

--enable-gpl
--enable-nonfree
--enable-libx264
--enable-libx265
--enable-libxvid
--enable-libvidstab
ARGS
  cp "${SRC_DIR}/COPYING.LGPLv2.1" "${ROOT_DIR}/third_party/ffmpeg/LICENSES/LGPL-2.1.txt"
  cp "${SRC_DIR}/COPYING.LGPLv3" "${ROOT_DIR}/third_party/ffmpeg/LICENSES/LGPL-3.0.txt"
  cp "${SRC_DIR}/LICENSE.md" "${ROOT_DIR}/third_party/ffmpeg/LICENSES/FFmpeg-LICENSE.txt"
}

main() {
  if [[ "${PACKAGE_IOS_XCFRAMEWORKS_ONLY:-0}" == "1" ]]; then
    package_ios_xcframeworks
    return
  fi

  download_source
  build_android_arm64
  build_ios_xcframework
  write_compliance_files
}

main "$@"
