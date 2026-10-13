#!/bin/sh
set -eu
binary=${1:-./build/samosa-inference-setup}
case "$binary" in /*) ;; *) binary="$(pwd)/$binary";; esac
fixture=$(mktemp -d /tmp/samosa-setup.XXXXXX)
umask 077
openssl_bin=${OPENSSL_BIN:-openssl}
case "$("$openssl_bin" version)" in LibreSSL*) if [ -x /opt/homebrew/bin/openssl ]; then openssl_bin=/opt/homebrew/bin/openssl; fi;; esac
server_pid=
worker=
cleanup(){ if [ -n "$worker" ]; then kill "$worker" 2>/dev/null || true; wait "$worker" 2>/dev/null || true; fi; if [ -n "$server_pid" ]; then kill "$server_pid" 2>/dev/null || true; wait "$server_pid" 2>/dev/null || true; fi; }
trap cleanup EXIT HUP INT TERM
case "$(uname -s)-$(uname -m)" in Darwin-arm64) platform=macos-arm64; other=linux-arm64;; Darwin-x86_64) platform=macos-x86_64; other=linux-arm64;; Linux-aarch64) platform=linux-arm64; other=macos-arm64;; Linux-x86_64) platform=linux-x86_64; other=macos-arm64;; *) exit 1;; esac
mkdir "$fixture/web"
printf abc > "$fixture/web/artifact.bin"
"$openssl_bin" req -x509 -newkey rsa:2048 -nodes -keyout "$fixture/key.pem" -out "$fixture/cert.pem" -days 1 \
    -subj /CN=localhost -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1' > "$fixture/cert.log" 2>&1
port=${SAMOSA_SETUP_QA_PORT:-53667}
(cd "$fixture/web";exec "$openssl_bin" s_server -quiet -ign_eof -WWW -accept "127.0.0.1:$port" -cert ../cert.pem -key ../key.pem) > "$fixture/server.log" 2>&1 &
server_pid=$!
sleep 1
kill -0 "$server_pid"
sha=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
catalog(){
    cat <<CATALOG
{"catalog_version":1,"revision":"setup-1","entries":[{"id":"fixture","version":"1","url":"https://127.0.0.1:$port/artifact.bin","license":"Controlled fixture; no production distribution license","platform":"$1","minimum_os":"$2","runtime":"$3","recipe":"fixture-1","sha256":"$4","size":3}]}
CATALOG
}
catalog "$platform" 1 system-runtime-v1 "$sha" > "$fixture/catalog.json"
checks=0
expect_error(){
    expected=$1;request=$2;shift 2
    result=$(printf '%s' "$request" | "$binary" "$@") && rc=0 || rc=$?
    [ "$rc" = 1 ];case "$result" in *"\"code\":\"$expected\""*) ;; *) printf '%s\n' "$result" >&2;exit 1;; esac
    checks=$((checks+1))
}
install='{"contract_version":1,"op":"provider_install","provider_id":"fixture"}'
status='{"contract_version":1,"op":"provider_status","provider_id":"fixture"}'
list='{"contract_version":1,"op":"provider_catalog"}'
result=$(printf '%s' "$list" | "$binary" --catalog-file "$fixture/catalog.json")
case "$result" in *'"supported":true'*) ;; *) exit 1;; esac
case "$result" in *https://*) exit 1;; esac
[ ! -e "$fixture/state" ];checks=$((checks+3))
expect_error artifact_tls_verification_failed "$install" --catalog-file "$fixture/catalog.json" --state "$fixture/untrusted" --byte-ceiling 3 --disk-ceiling 96
printf private-setup-secret > "$fixture/bearer-key"
printf '%s' "$install" | "$binary" --catalog-file "$fixture/catalog.json" --state "$fixture/state" --byte-ceiling 3 --disk-ceiling 96 --ca-file "$fixture/cert.pem" --bearer-key-file "$fixture/bearer-key" > "$fixture/verified"
grep -q '"status":"verified"' "$fixture/verified"
grep -q '"activation":"not_requested"' "$fixture/verified"
grep -q '"readiness":"unqualified"' "$fixture/verified"
if grep -q private-setup-secret "$fixture/verified"; then exit 1; fi
checks=$((checks+4))
printf '%s' "$status" | "$binary" --catalog-file "$fixture/catalog.json" --state "$fixture/state" --byte-ceiling 3 --disk-ceiling 96 > "$fixture/status"
cmp "$fixture/verified" "$fixture/status";checks=$((checks+1))
expect_error invalid_request '{"contract_version":1,"op":"provider_install","provider_id":"fixture","url":"https://override.invalid"}' --catalog-file "$fixture/catalog.json"
expect_error invalid_request '{"contract_version":1,"op":"provider_catalog","provider_id":"fixture"}' --catalog-file "$fixture/catalog.json"
expect_error invalid_request '{"contract_version":1,"op":"provider_install"}' --catalog-file "$fixture/catalog.json"
expect_error unsupported_operation '{"contract_version":1,"op":"provider_activate","provider_id":"fixture"}' --catalog-file "$fixture/catalog.json"
expect_error unsupported_contract '{"contract_version":2,"op":"provider_catalog"}' --catalog-file "$fixture/catalog.json"
expect_error invalid_json '{"contract_version":1,"op":"provider_catalog","op":"provider_install"}' --catalog-file "$fixture/catalog.json"
expect_error unknown_provider '{"contract_version":1,"op":"provider_install","provider_id":"other"}' --catalog-file "$fixture/catalog.json"
expect_error invalid_catalog_file "$list" --catalog-file relative
chmod 0644 "$fixture/catalog.json"
expect_error invalid_catalog_file "$list" --catalog-file "$fixture/catalog.json"
chmod 0600 "$fixture/catalog.json"
catalog "$other" 1 system-runtime-v1 "$sha" > "$fixture/other.json"
expect_error unsupported_provider_platform "$install" --catalog-file "$fixture/other.json" --state "$fixture/other"
[ ! -e "$fixture/other" ];checks=$((checks+1))
catalog "$platform" 65535 system-runtime-v1 "$sha" > "$fixture/future.json"
expect_error unsupported_provider_os "$install" --catalog-file "$fixture/future.json" --state "$fixture/future"
[ ! -e "$fixture/future" ];checks=$((checks+1))
catalog "$platform" 1 unavailable-runtime "$sha" > "$fixture/runtime.json"
expect_error unsupported_provider_runtime "$install" --catalog-file "$fixture/runtime.json" --state "$fixture/runtime"
[ ! -e "$fixture/runtime" ];checks=$((checks+1))
expect_error invalid_artifact_configuration "$install" --catalog-file "$fixture/catalog.json" --state "$fixture/quota" --byte-ceiling 2 --disk-ceiling 96
expect_error invalid_artifact_configuration "$install" --catalog-file "$fixture/catalog.json" --state "$fixture/quota" --byte-ceiling 3 --disk-ceiling 5
[ ! -e "$fixture/quota" ];checks=$((checks+1))
mkdir "$fixture/foreign";chmod 0755 "$fixture/foreign"
printf retain > "$fixture/foreign/keep"
expect_error unsafe_artifact_state "$install" --catalog-file "$fixture/catalog.json" --state "$fixture/foreign" --byte-ceiling 3 --disk-ceiling 96
[ "$(cat "$fixture/foreign/keep")" = retain ];checks=$((checks+1))
expect_error invalid_provider_credentials "$install" --catalog-file "$fixture/catalog.json" --state "$fixture/credentials" --byte-ceiling 3 --disk-ceiling 96 --bearer-key-file "$fixture/missing"
[ ! -e "$fixture/credentials" ];checks=$((checks+1))
printf '%s' "$list" | "$binary" --catalog-file "$fixture/catalog.json" --catalog-file "$fixture/catalog.json" > "$fixture/duplicate" && rc=0 || rc=$?
[ "$rc" = 2 ];checks=$((checks+1))
catalog "$platform" 1 system-runtime-v1 0000000000000000000000000000000000000000000000000000000000000000 > "$fixture/corrupt.json"
expect_error artifact_checksum_mismatch "$install" --catalog-file "$fixture/corrupt.json" --state "$fixture/state" --byte-ceiling 3 --disk-ceiling 96 --ca-file "$fixture/cert.pem"
printf '%s' "$status" | "$binary" --catalog-file "$fixture/catalog.json" --state "$fixture/state" --byte-ceiling 3 --disk-ceiling 96 > "$fixture/last-good"
cmp "$fixture/verified" "$fixture/last-good";checks=$((checks+1))
kill "$server_pid";wait "$server_pid" || true;server_pid=
printf '%s' "$install" | "$binary" --catalog-file "$fixture/catalog.json" --state "$fixture/state" --byte-ceiling 3 --disk-ceiling 96 > "$fixture/retry"
cmp "$fixture/verified" "$fixture/retry";checks=$((checks+1))
mkfifo "$fixture/server-input"
"$openssl_bin" s_server -quiet -ign_eof -accept "127.0.0.1:$port" -cert "$fixture/cert.pem" -key "$fixture/key.pem" < "$fixture/server-input" > "$fixture/stall-server.log" 2>&1 &
server_pid=$!
exec 3> "$fixture/server-input"
sleep 1
kill -0 "$server_pid"
printf '%s' "$install" > "$fixture/request"
"$binary" --catalog-file "$fixture/catalog.json" --state "$fixture/cancel" --byte-ceiling 3 --disk-ceiling 96 --ca-file "$fixture/cert.pem" < "$fixture/request" > "$fixture/cancelled" &
worker=$!
attempt=0
while ! grep -q 'GET /artifact.bin' "$fixture/stall-server.log"; do
    attempt=$((attempt+1));[ "$attempt" -lt 300 ];kill -0 "$worker";sleep 0.01
done
printf 'HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na' >&3
attempt=0
while [ ! -f "$fixture/cancel/$sha/stage" ] || [ "$(wc -c < "$fixture/cancel/$sha/stage")" -lt 1 ]; do
    attempt=$((attempt+1));[ "$attempt" -lt 300 ];kill -0 "$worker";sleep 0.01
done
kill -TERM "$worker"
attempt=0
while kill -0 "$worker" 2>/dev/null; do
    attempt=$((attempt+1));[ "$attempt" -lt 400 ];sleep 0.01
done
wait "$worker" && rc=0 || rc=$?
worker=
[ "$rc" = 1 ];grep -q '"code":"artifact_interrupted"' "$fixture/cancelled";checks=$((checks+2))
printf '%s' "$status" | "$binary" --catalog-file "$fixture/catalog.json" --state "$fixture/cancel" --byte-ceiling 3 --disk-ceiling 96 > "$fixture/interrupted"
grep -q '"status":"interrupted"' "$fixture/interrupted";checks=$((checks+1))
exec 3>&-
kill "$server_pid";wait "$server_pid" || true;server_pid=
(cd "$fixture/web";exec "$openssl_bin" s_server -quiet -ign_eof -WWW -accept "127.0.0.1:$port" -cert ../cert.pem -key ../key.pem) > "$fixture/resume-server.log" 2>&1 &
server_pid=$!
sleep 1
expect_error artifact_resume_required "$install" --catalog-file "$fixture/catalog.json" --state "$fixture/cancel" --byte-ceiling 3 --disk-ceiling 96 --ca-file "$fixture/cert.pem"
printf '%s' '{"contract_version":1,"op":"provider_resume","provider_id":"fixture"}' | "$binary" --catalog-file "$fixture/catalog.json" --state "$fixture/cancel" --byte-ceiling 3 --disk-ceiling 96 --ca-file "$fixture/cert.pem" > "$fixture/resumed"
cmp "$fixture/verified" "$fixture/resumed";checks=$((checks+1))
printf 'PASS setup CLI: %s checks; catalog isolation, HTTPS, quota/foreign refusal and last-good/offline retry; QA %s\n' "$checks" "$fixture"
