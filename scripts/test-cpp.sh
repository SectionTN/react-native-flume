#!/usr/bin/env bash
# Builds the C++ core against React Native's Yoga on the host and runs the doctest suites.
set -euo pipefail
cd "$(dirname "$0")/.."

CMAKE=$(command -v cmake || true)
if [ -z "$CMAKE" ]; then
  # No cmake on PATH: fall back to the newest one in the Android SDK.
  SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
  CMAKE=$(ls -d "$SDK"/cmake/*/bin/cmake 2>/dev/null | sort -V | tail -1)
fi
if [ -z "$CMAKE" ]; then
  echo "cmake not found on PATH or in the Android SDK" >&2
  exit 1
fi
# Ninja tracks header dependencies per object; Makefiles left stale objects behind a failed build.
NINJA=$(command -v ninja || ls "$(dirname "$CMAKE")/ninja" 2>/dev/null || true)
GENERATOR="Unix Makefiles"
GEN_ARGS=()
if [ -n "$NINJA" ]; then
  GENERATOR="Ninja"
  GEN_ARGS=(-G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA")
fi
if [ -f build/host/CMakeCache.txt ] && ! grep -q "CMAKE_GENERATOR:INTERNAL=$GENERATOR\$" build/host/CMakeCache.txt; then
  rm -rf build/host
fi

RN_DIR=$(node -p "require('path').dirname(require.resolve('react-native/package.json'))")
WORKLETS_DIR=$(node -p "require('path').dirname(require.resolve('react-native-worklets/package.json'))")
# The jsi suite links React Native's macOS Hermes framework, so it runs on macOS only.
HERMES_ARG=()
if [ "$(uname -s)" = "Darwin" ]; then
  HERMES_DIR=$(scripts/fetch-hermes.sh)
  HERMES_ARG=(-DHERMES_DIR="$PWD/$HERMES_DIR")
fi
mkdir -p build
if ! "$CMAKE" -S cpp/tests -B build/host -DCMAKE_BUILD_TYPE=Release "${GEN_ARGS[@]}" \
  -DREACT_NATIVE_DIR="$RN_DIR" -DWORKLETS_DIR="$WORKLETS_DIR" "${HERMES_ARG[@]}" > build/host-configure.log 2>&1; then
  tail -40 build/host-configure.log
  exit 1
fi
if ! "$CMAKE" --build build/host -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc)" > build/host-build.log 2>&1; then
  grep -E -A4 'error:' build/host-build.log | head -80
  exit 1
fi
build/host/flume_tests
if [ -x build/host/flume_jsi_tests ]; then
  build/host/flume_jsi_tests
else
  echo "jsi suite skipped (macOS only)"
fi
