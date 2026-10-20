#!/usr/bin/env bash
# Builds the C++ core against React Native's Yoga on the host and runs the doctest suite.
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

RN_DIR=$(node -p "require('path').dirname(require.resolve('react-native/package.json'))")
WORKLETS_DIR=$(node -p "require('path').dirname(require.resolve('react-native-worklets/package.json'))")
mkdir -p build
if ! "$CMAKE" -S cpp/tests -B build/host -DCMAKE_BUILD_TYPE=Release \
  -DREACT_NATIVE_DIR="$RN_DIR" -DWORKLETS_DIR="$WORKLETS_DIR" > build/host-configure.log 2>&1; then
  tail -40 build/host-configure.log
  exit 1
fi
if ! "$CMAKE" --build build/host -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc)" > build/host-build.log 2>&1; then
  grep -E -A4 'error:' build/host-build.log | head -80
  exit 1
fi
build/host/flume_tests
