#!/bin/sh
set -eu
binary=${1:-./build/samosa-inference-download}
case "$binary" in /*) ;; *) binary="$(pwd)/$binary";; esac
fixture=$(mktemp -d /tmp/samosa-remote-tls.XXXXXX)
openssl_bin=${OPENSSL_BIN:-openssl}
case "$("$openssl_bin" version)" in LibreSSL*) if [ -x /opt/homebrew/bin/openssl ]; then openssl_bin=/opt/homebrew/bin/openssl; fi;; esac
umask 077
server_pid=
cleanup(){ if [ -n "$server_pid" ]; then kill "$server_pid" 2>/dev/null || true; wait "$server_pid" 2>/dev/null || true; fi; }
trap cleanup EXIT HUP INT TERM
mkdir "$fixture/web"
printf abc > "$fixture/web/artifact.bin"
"$openssl_bin" req -x509 -newkey rsa:2048 -nodes -keyout "$fixture/key.pem" -out "$fixture/cert.pem" -days 1 \
    -subj /CN=localhost -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1' > "$fixture/cert.log" 2>&1
port=${SAMOSA_TLS_QA_PORT:-53666}
(cd "$fixture/web";exec "$openssl_bin" s_server -quiet -ign_eof -WWW -accept "127.0.0.1:$port" -cert ../cert.pem -key ../key.pem) > "$fixture/server.log" 2>&1 &
server_pid=$!
sleep 1
kill -0 "$server_pid"
sha=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
request='{"contract_version":1,"op":"artifact_install"}'
result=$(printf '%s' "$request" | "$binary" --state "$fixture/untrusted" --url "https://127.0.0.1:$port/artifact.bin" --sha256 "$sha" --size 3 --byte-ceiling 3 --disk-ceiling 96) && rc=0 || rc=$?
[ "$rc" = 1 ];case "$result" in *artifact_tls_verification_failed*) ;; *) printf '%s\n' "$result" >&2;exit 1;; esac
printf private-secret > "$fixture/bearer-key"
printf '%s' "$request" | "$binary" --state "$fixture/trusted" --url "https://127.0.0.1:$port/artifact.bin" --ca-file "$fixture/cert.pem" --bearer-key-file "$fixture/bearer-key" --sha256 "$sha" --size 3 --byte-ceiling 3 --disk-ceiling 96 > "$fixture/verified"
rm "$fixture/bearer-key"
if grep -q private-secret "$fixture/verified"; then exit 1; fi
grep -q '"status":"verified"' "$fixture/verified"
grep -q '"readiness":"unqualified"' "$fixture/verified"
kill "$server_pid";wait "$server_pid" || true;server_pid=
printf '%s' "$request" | "$binary" --state "$fixture/trusted" --url "https://127.0.0.1:$port/artifact.bin" --ca-file "$fixture/cert.pem" --sha256 "$sha" --size 3 --byte-ceiling 3 --disk-ceiling 96 > "$fixture/retry"
cmp "$fixture/verified" "$fixture/retry"
printf 'PASS remote TLS CLI: verified HTTPS, untrusted certificate refusal, offline exact retry; QA %s\n' "$fixture"
