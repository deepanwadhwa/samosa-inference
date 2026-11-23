#!/bin/sh
# Build the pinned local Whisper.cpp command-line runtime used by Samosa voice.
# This is intentionally a separate, explicit bootstrap from the weight
# download: the source is checksum-verified, the result lives under Samosa's
# own home, and neither the model nor audio ever leaves the computer at use
# time. The gateway serializes invocations of this script.

set -eu
umask 077

HOME_DIR=${SAMOSA_HOME:-"$HOME/.samosa"}
RUNTIME_DIR="$HOME_DIR/voice/runtime"
CLI="$RUNTIME_DIR/whisper-cli"
PIN="$RUNTIME_DIR/whisper-cli.pin"
COMMIT="f049fff95a089aa9969deb009cdd4892b3e74916"
SOURCE_URL="https://api.github.com/repos/ggml-org/whisper.cpp/tarball/$COMMIT"
SOURCE_SHA256="d8cd961352377b1cc612224016a9ebdfe0ae508dc2b2f9ef514b341d672e3fdc"

sha256_file() {
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{print $1}';
  elif command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}';
  else return 1; fi
}

file_metadata() {
  if [ "$(uname -s)" = Darwin ]; then
    stat -f '%u %Lp %l %z' "$1"
  else
    stat -c '%u %a %h %s' "$1"
  fi
}

runtime_pin() {
  [ -f "$1" ] && [ -x "$1" ] && [ ! -L "$1" ] || return 1
  metadata=$(file_metadata "$1") || return 1
  read -r owner mode links bytes <<EOF
$metadata
EOF
  [ "$owner" = "$(id -u)" ] && [ "$links" = 1 ] &&
    [ "$((0$mode & 022))" -eq 0 ] && [ "$bytes" -ge 64 ] &&
    [ "$bytes" -le 67108864 ] || return 1
  digest=$(sha256_file "$1") || return 1
  [ "${#digest}" -eq 64 ] || return 1
  printf 'whisper-runtime-v1\t%s\t%s\n' "$digest" "$bytes"
}

pin_matches() {
  [ -f "$PIN" ] && [ ! -L "$PIN" ] || return 1
  metadata=$(file_metadata "$PIN") || return 1
  read -r owner mode links bytes <<EOF
$metadata
EOF
  [ "$owner" = "$(id -u)" ] && [ "$links" = 1 ] &&
    [ "$((0$mode & 077))" -eq 0 ] && [ "$bytes" -le 100 ] &&
    [ "$(cat "$PIN")" = "$1" ]
}

case "${1:-}" in
  ''|--pin-existing) ;;
  *) exit 2 ;;
esac
if [ -x "$CLI" ] && observed_pin=$(runtime_pin "$CLI"); then
  if pin_matches "$observed_pin"; then
    exit 0
  fi
  if [ ! -e "$PIN" ] && [ ! -L "$PIN" ]; then
    # Adopt the installed local runtime once. Inference still verifies its
    # native loader, the pinned bytes and actual speech before reporting ready.
    staged_pin=$(mktemp "$RUNTIME_DIR/.whisper-pin.XXXXXX")
    trap 'rm -f "$staged_pin"' EXIT HUP INT TERM
    printf '%s\n' "$observed_pin" >"$staged_pin"
    mv -n "$staged_pin" "$PIN"
    pin_matches "$observed_pin"
    exit 0
  fi
fi
[ "${1:-}" != --pin-existing ] || exit 1
[ ! -d "$PIN" ] && [ ! -L "$PIN" ] && [ ! -d "$CLI" ] && [ ! -L "$CLI" ] || {
  echo "The Whisper runtime paths must name local files." >&2
  exit 1
}
command -v curl >/dev/null 2>&1 || { echo "curl is required to prepare local speech recognition." >&2; exit 1; }
command -v tar >/dev/null 2>&1 || { echo "tar is required to prepare local speech recognition." >&2; exit 1; }

# launchd services receive a deliberately small PATH. Homebrew's CMake lives
# outside it on a normal Apple Silicon install, even though it is installed.
# Honour an explicit override first, then resolve the two standard locations.
if [ -n "${SAMOSA_CMAKE:-}" ] && [ -x "$SAMOSA_CMAKE" ]; then
  CMAKE="$SAMOSA_CMAKE"
elif command -v cmake >/dev/null 2>&1; then
  CMAKE=$(command -v cmake)
elif [ -x /opt/homebrew/bin/cmake ]; then
  CMAKE=/opt/homebrew/bin/cmake
elif [ -x /usr/local/bin/cmake ]; then
  CMAKE=/usr/local/bin/cmake
else
  echo "CMake is required to set up local voice recognition. Install it with: brew install cmake" >&2
  exit 1
fi

mkdir -p "$RUNTIME_DIR"
TMP=$(mktemp -d "$RUNTIME_DIR/.whisper-build.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

curl -fsSL --retry 3 --retry-delay 2 "$SOURCE_URL" -o "$TMP/source.tar.gz"
[ "$(sha256_file "$TMP/source.tar.gz")" = "$SOURCE_SHA256" ] || {
  echo "The Whisper.cpp source archive did not match its pinned checksum." >&2
  exit 1
}

mkdir "$TMP/source"
tar -xzf "$TMP/source.tar.gz" -C "$TMP/source" --strip-components 1
"$CMAKE" -S "$TMP/source" -B "$TMP/build" \
  -DBUILD_SHARED_LIBS=OFF -DWHISPER_BUILD_EXAMPLES=ON \
  -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_SERVER=OFF
"$CMAKE" --build "$TMP/build" --target whisper-cli --parallel 2
[ -x "$TMP/build/bin/whisper-cli" ] || { echo "Whisper.cpp did not produce whisper-cli." >&2; exit 1; }

cp "$TMP/build/bin/whisper-cli" "$TMP/whisper-cli"
chmod 700 "$TMP/whisper-cli"
runtime_pin "$TMP/whisper-cli" >"$TMP/whisper-cli.pin"
mv -f "$TMP/whisper-cli" "$CLI"
mv -f "$TMP/whisper-cli.pin" "$PIN"
