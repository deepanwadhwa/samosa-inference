#!/bin/sh
set -eu
binary=${1:-./build/samosa-inference-artifacts}
fixture=$(mktemp -d /tmp/samosa-artifacts-cli.XXXXXX)
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
printf abc > "$fixture/input"
sha=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
checks=0
request() {
    result=$(printf '%s' "$1" | "$binary" --state "$fixture/state" --source "$fixture/input" \
        --sha256 "$sha" --size 3 --byte-ceiling 3 --disk-ceiling 96) && rc=0 || rc=$?
    checks=$((checks+1))
    [ "$rc" = "$2" ] || { printf 'FAIL exit: %s\n' "$result" >&2; exit 1; }
    checks=$((checks+1))
    case "$result" in *"$3"*) ;; *) printf 'FAIL response: %s\n' "$result" >&2; exit 1;; esac
}
request '{"contract_version":1,"op":"artifact_status"}' 0 '"status":"absent"'
request '{"contract_version":1,"op":"artifact_install"}' 0 '"status":"verified"'
request '{"contract_version":1,"op":"artifact_status"}' 0 '"readiness":"unqualified"'
rm "$fixture/input"
request '{"contract_version":1,"op":"artifact_install"}' 0 '"activation":"not_requested"'
request '{"contract_version":1,"op":"artifact_resume"}' 0 '"status":"verified"'
request '{"contract_version":1,"op":"artifact_install","path":"/foreign"}' 1 '"code":"invalid_request"'
request '{"contract_version":2,"op":"artifact_status"}' 1 '"code":"unsupported_contract"'
request '{"contract_version":1,"op":"activate"}' 1 '"code":"unsupported_operation"'
request '{"contract_version":1,"op":"artifact_status","op":"artifact_install"}' 1 '"code":"invalid_json"'
request '{"contract_version":1,"op":"artifact_install","artifact_capacity":1024}' 1 '"code":"invalid_request"'
result=$(printf '%s' '{"contract_version":1,"op":"artifact_install"}' | "$binary" \
    --state "$fixture/state" --source "$fixture/input" --sha256 "$sha" --size 3 \
    --byte-ceiling 3 --disk-ceiling 96 --artifact-capacity 1024)
checks=$((checks+1))
case "$result" in *'"status":"verified"'*) ;; *) exit 1;; esac
for capacity in 0 1025 -1 1.5; do
    if printf '%s' '{"contract_version":1,"op":"artifact_status"}' | "$binary" \
        --state "$fixture/invalid-capacity" --sha256 "$sha" --size 3 \
        --byte-ceiling 3 --disk-ceiling 96 --artifact-capacity "$capacity"; then exit 1; else rc=$?; fi
    checks=$((checks+1));[ "$rc" = 2 ]
done
if printf '%s' '{"contract_version":1,"op":"artifact_status"}' | "$binary" \
    --state "$fixture/invalid-capacity" --sha256 "$sha" --size 3 \
    --byte-ceiling 3 --disk-ceiling 96 --artifact-capacity 17 --artifact-capacity 18; then exit 1; else rc=$?; fi
checks=$((checks+1));[ "$rc" = 2 ];[ ! -e "$fixture/invalid-capacity" ]
printf 'PASS artifact public CLI: %s checks; host config, verified/unqualified, offline retry, strict refusal\n' "$checks"
