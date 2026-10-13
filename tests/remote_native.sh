#!/bin/sh
# Explicit actual provider QA; not a production download catalog or model-free gate.
set -eu
[ "$#" = 8 ] || { echo 'usage: remote_native.sh ABSOLUTE_SETUP ABSOLUTE_MANAGED ABSOLUTE_ENGINE ENGINE_SHA ENGINE_SIZE ABSOLUTE_MODEL MODEL_SHA MODEL_SIZE' >&2; exit 2; }
setup=$1 managed=$2 engine=$3 engine_sha=$4 engine_size=$5 model=$6 model_sha=$7 model_size=$8
[ "$engine_sha" = 1634265c6be962401fb2d2419b371b5eedef20957bc603fb3bbde62ed8389ccf ] && [ "$engine_size" = 20513984 ] || { echo 'This actual recipe qualifies only the recorded static engine pin' >&2; exit 2; }
fixture=$(mktemp -d /tmp/samosa-remote-native.XXXXXX)
umask 077
server_pid=
cleanup(){ if [ -n "$server_pid" ]; then kill "$server_pid" 2>/dev/null || true; wait "$server_pid" 2>/dev/null || true; fi; }
trap cleanup EXIT HUP INT TERM
case "$(uname -s)-$(uname -m)" in Darwin-arm64) platform=macos-arm64;minimum=26.0;; *) echo 'Actual artifact platform unqualified' >&2;exit 2;; esac
openssl_bin=${OPENSSL_BIN:-openssl}
case "$("$openssl_bin" version)" in LibreSSL*) if [ -x /opt/homebrew/bin/openssl ]; then openssl_bin=/opt/homebrew/bin/openssl; fi;; esac
mkdir "$fixture/web"
cp "$engine" "$fixture/web/engine.bin"
"$openssl_bin" req -x509 -newkey rsa:2048 -nodes -keyout "$fixture/key.pem" -out "$fixture/cert.pem" -days 1 \
    -subj /CN=localhost -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1' > "$fixture/cert.log" 2>&1
port=${SAMOSA_REMOTE_NATIVE_QA_PORT:-53668}
cat > "$fixture/catalog.json" <<CATALOG
{"catalog_version":1,"revision":"actual-static-qa-1","entries":[{"id":"actual-static-qa","version":"1","url":"https://127.0.0.1:$port/engine.bin","license":"Controlled local QA delivery of an explicitly configured engine; see its owner-provided package notices. No production distribution declaration.","platform":"$platform","minimum_os":"$minimum","runtime":"system-runtime-v1","recipe":"static-system-9fcaed763ccda38ea81068ad9d7f991aaddca451","sha256":"$engine_sha","size":$engine_size}]}
CATALOG
(cd "$fixture/web";exec "$openssl_bin" s_server -quiet -ign_eof -WWW -accept "127.0.0.1:$port" -cert ../cert.pem -key ../key.pem) > "$fixture/server.log" 2>&1 &
server_pid=$!
sleep 1
kill -0 "$server_pid"
request='{"contract_version":1,"op":"provider_install","provider_id":"actual-static-qa"}'
printf '%s' "$request" | "$setup" --catalog-file "$fixture/catalog.json" --state "$fixture/artifacts" --byte-ceiling "$engine_size" --disk-ceiling "$((engine_size*2))" --ca-file "$fixture/cert.pem" > "$fixture/installed"
grep -q '"status":"verified"' "$fixture/installed"
kill "$server_pid";wait "$server_pid" || true;server_pid=
printf '%s' "$request" | "$setup" --catalog-file "$fixture/catalog.json" --state "$fixture/artifacts" --byte-ceiling "$engine_size" --disk-ceiling "$((engine_size*2))" > "$fixture/offline"
cmp "$fixture/installed" "$fixture/offline"
artifacts=$(dirname "$setup")/samosa-inference-artifacts
printf '%s' '{"contract_version":1,"op":"artifact_make_executable"}' | "$artifacts" --state "$fixture/artifacts" --sha256 "$engine_sha" --size "$engine_size" --byte-ceiling "$engine_size" --disk-ceiling "$((engine_size*2))" > "$fixture/executable"
sh tests/managed_native.sh "$managed" "$fixture/artifacts/$engine_sha/artifact" "$engine_sha" "$engine_size" "$model" "$model_sha" "$model_size" > "$fixture/managed.log" 2>&1
cat "$fixture/managed.log"
printf 'PASS actual remote setup: pinned HTTPS engine acquisition, offline retry, native executable qualification, owned actual provider; QA %s\n' "$fixture"
