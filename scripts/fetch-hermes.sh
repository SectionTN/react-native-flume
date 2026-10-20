#!/usr/bin/env bash
# Downloads the Hermes release tarball React Native pins and prints its directory.
set -euo pipefail
cd "$(dirname "$0")/.."

TAG=$(tr -d '[:space:]' < node_modules/react-native/sdks/.hermesv1version)
VERSION=${TAG#hermes-v}
DIR=build/hermes/$VERSION
if [ ! -d "$DIR/destroot" ]; then
  mkdir -p "$DIR"
  URL="https://repo1.maven.org/maven2/com/facebook/hermes/hermes-ios/$VERSION/hermes-ios-$VERSION-hermes-ios-release.tar.gz"
  curl -fsSL "$URL" -o "$DIR/hermes.tar.gz"
  tar -xzf "$DIR/hermes.tar.gz" -C "$DIR"
  rm "$DIR/hermes.tar.gz"
fi
echo "$DIR"
