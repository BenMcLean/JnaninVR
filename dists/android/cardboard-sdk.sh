#!/bin/sh
# Builds the Cardboard SDK and lays out what the ScummVM Android build needs:
#   OUT/sdk-release.aar          for Gradle:    -PcardboardAar=OUT/sdk-release.aar
#   OUT/<abi>/include, OUT/<abi>/lib   for configure: --enable-cardboard --with-cardboard-prefix=OUT/<abi>
# Usage: cardboard-sdk.sh OUT_DIR [ABI ...]   (default ABI: arm64-v8a)
# Needs git, the Android SDK (ANDROID_HOME) and a JDK.
set -e

COMMIT=8bd424d   # Cardboard SDK v1.36.0
OUT="$1"
[ -n "$OUT" ] || { echo "usage: $0 OUT_DIR [ABI ...]" >&2; exit 1; }
shift
[ $# -gt 0 ] || set -- arm64-v8a

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
if [ ! -d "$OUT/cardboard" ]; then
	git clone https://github.com/googlevr/cardboard.git "$OUT/cardboard"
fi
cd "$OUT/cardboard"
git checkout "$COMMIT"
./gradlew :sdk:assembleRelease

cp sdk/build/outputs/aar/sdk-release.aar "$OUT/sdk-release.aar"
for abi in "$@"; do
	mkdir -p "$OUT/$abi/include" "$OUT/$abi/lib"
	cp sdk/include/cardboard.h "$OUT/$abi/include/"
	unzip -o -j "$OUT/sdk-release.aar" "jni/$abi/libGfxPluginCardboard.so" -d "$OUT/$abi/lib"
done
