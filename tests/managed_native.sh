#!/bin/sh
# Explicit optional actual-model qualification; never part of model-free tests.
set -eu
[ "$#" = 7 ] || { echo 'usage: managed_native.sh ABSOLUTE_BINARY ABSOLUTE_ENGINE ENGINE_SHA ENGINE_SIZE ABSOLUTE_MODEL MODEL_SHA MODEL_SIZE' >&2; exit 2; }
binary=$1 engine=$2 engine_sha=$3 engine_size=$4 model=$5 model_sha=$6 model_size=$7
fixture=$(mktemp -d /tmp/samosa-managed-native.XXXXXX)
printf 'Actual managed QA root: %s\n' "$fixture"
managed_pid=
cleanup() { if [ -n "$managed_pid" ]; then kill "$managed_pid" 2>/dev/null || true; wait "$managed_pid" 2>/dev/null || true; fi; rm -f "$fixture/header"; }
trap cleanup EXIT HUP INT TERM
# Host port can be explicitly selected; authentication rejects an unrelated
# listener. Failure to bind/start refuses rather than reporting false readiness.
port=${SAMOSA_QA_PORT:-53558}
cat > "$fixture/host.json" <<EOF
{"contract_version":1,"profile":"llama-static-system-v1","state":"$fixture/state","engine":"$engine","engine_sha256":"$engine_sha","engine_size":$engine_size,"model":"$model","model_sha256":"$model_sha","model_size":$model_size,"context_tokens":4096,"model_alias":"public-managed","owner":"qa","port":$port,"expected_revision":0,"gpu_layers":99,"threads":4}
EOF
chmod 600 "$fixture/host.json"
mkfifo "$fixture/requests"
"$binary" --config "$fixture/host.json" < "$fixture/requests" > "$fixture/results" 2> "$fixture/stderr" &
managed_pid=$!
exec 3> "$fixture/requests"
attempt=0
while ! grep -q provider_started "$fixture/results"; do
    if [ ! -f "$fixture/header" ] && [ -d "$fixture/state" ]; then
        key_file=$(find "$fixture/state" -maxdepth 1 -name 'key.*' -print | head -1)
        if [ -n "$key_file" ]; then
            key=$(cat "$key_file")
            (umask 077;printf 'Authorization: Bearer %s\n' "$key" > "$fixture/header")
            unset key
        fi
    fi
    attempt=$((attempt+1));[ "$attempt" -lt 150 ];kill -0 "$managed_pid";sleep 1
done
provider=$(sed -n 's/.*"provider_id":"\(managed-text-[a-f0-9]*\)".*/\1/p' "$fixture/results" | head -1)
[ -n "$provider" ]
printf '{"contract_version":1,"request_id":"describe","request":{"contract_version":1,"op":"describe"}}\n{"contract_version":1,"request_id":"status","request":{"contract_version":1,"op":"provider_status","provider_id":"%s"}}\n{"contract_version":1,"request_id":"count","request":{"contract_version":1,"op":"count_tokens","operation_id":"public-count","provider_id":"%s","prompt":"Compute 2 + 2. Answer only the integer."}}\n' "$provider" "$provider" >&3
attempt=0
while ! grep -q '"operation_id":"public-count"' "$fixture/results"; do
    attempt=$((attempt+1));[ "$attempt" -lt 60 ];kill -0 "$managed_pid";sleep 1
done
printf '{"contract_version":1,"request_id":"generate","request":{"contract_version":1,"op":"generate","operation_id":"public-generate","provider_id":"%s","prompt":"Compute 2 + 2. Answer only the integer.","max_output_tokens":32}}\n' "$provider" >&3
attempt=0
while ! grep -q '"operation_id":"public-generate"' "$fixture/results"; do
    attempt=$((attempt+1));[ "$attempt" -lt 60 ];kill -0 "$managed_pid";sleep 1
done
grep -q '"ready":true' "$fixture/results"
grep -q '"answer":"4"' "$fixture/results"
[ -f "$fixture/header" ]
printf '{"contract_version":1,"request_id":"long","request":{"contract_version":1,"op":"generate","operation_id":"public-active-cancel","provider_id":"%s","prompt":"Write every integer from 1 to 10000, separated by spaces. Do not abbreviate or skip any integers.","max_output_tokens":2048}}\n' "$provider" >&3
attempt=0
while :; do
    if curl -fsS --max-time 1 --noproxy '*' -H "@$fixture/header" "http://127.0.0.1:$port/slots" > "$fixture/slots" 2> "$fixture/slot-stderr" && grep -Eq '"is_processing"[[:space:]]*:[[:space:]]*true' "$fixture/slots"; then break; fi
    attempt=$((attempt+1));[ "$attempt" -lt 15 ];kill -0 "$managed_pid";sleep 0.1
done
printf '{"contract_version":1,"request_id":"cancel","request":{"contract_version":1,"op":"operation_cancel","operation_id":"public-active-cancel"}}\n' >&3
attempt=0
while ! grep -q '"request_id":"long"' "$fixture/results"; do
    attempt=$((attempt+1));[ "$attempt" -lt 6 ];kill -0 "$managed_pid";sleep 1
done
grep -q '"code":"cancelled"' "$fixture/results"
grep -q '"request_id":"cancel"' "$fixture/results"
printf '{"contract_version":1,"request_id":"survivor","request":{"contract_version":1,"op":"generate","operation_id":"public-survivor","provider_id":"%s","prompt":"Compute 2 + 2. Answer only the integer.","max_output_tokens":32}}\n' "$provider" >&3
attempt=0
while ! grep -q '"operation_id":"public-survivor"' "$fixture/results"; do
    attempt=$((attempt+1));[ "$attempt" -lt 60 ];kill -0 "$managed_pid";sleep 1
done
grep '"request_id":"survivor"' "$fixture/results" | grep -q '"answer":"4"'
rm "$fixture/header"
exec 3>&-
attempt=0
while kill -0 "$managed_pid" 2>/dev/null; do
    attempt=$((attempt+1));[ "$attempt" -lt 10 ];sleep 1
done
wait "$managed_pid"
managed_pid=
native=$(dirname "$binary")/samosa-inference-llama
printf '{"contract_version":1,"op":"generate","operation_id":"public-generate","provider_id":"%s","prompt":"Compute 2 + 2. Answer only the integer.","max_output_tokens":32}' "$provider" | "$native" --llama-endpoint "http://127.0.0.1:$port" --provider-id "$provider" --model-alias public-managed --context-tokens 4096 --owner qa --admission-state "$fixture/state/admission" > "$fixture/offline-retry"
sed -n 's/^.*"request_id":"generate","response":\(.*\)}$/\1/p' "$fixture/results" > "$fixture/generated"
cmp "$fixture/generated" "$fixture/offline-retry"
[ -z "$(find "$fixture/state" -name 'key.*' -print)" ]
printf 'PASS actual managed public CLI: correlated requests, observed active in-stream cancellation, survivor, FIFO EOF shutdown, offline exact retry, no retained key\n'
