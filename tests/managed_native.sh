#!/bin/sh
# Explicit optional actual-model qualification; never part of model-free tests.
set -eu
[ "$#" = 7 ] || { echo 'usage: managed_native.sh ABSOLUTE_BINARY ABSOLUTE_ENGINE ENGINE_SHA ENGINE_SIZE ABSOLUTE_MODEL MODEL_SHA MODEL_SIZE' >&2; exit 2; }
binary=$1 engine=$2 engine_sha=$3 engine_size=$4 model=$5 model_sha=$6 model_size=$7
fixture=$(mktemp -d /tmp/samosa-managed-native.XXXXXX)
printf 'Actual managed QA root: %s\n' "$fixture"
managed_pid=
registry_work_pid=
cleanup() { if [ -n "$registry_work_pid" ]; then kill "$registry_work_pid" 2>/dev/null || true; wait "$registry_work_pid" 2>/dev/null || true; fi; if [ -n "$managed_pid" ]; then kill "$managed_pid" 2>/dev/null || true; wait "$managed_pid" 2>/dev/null || true; fi; rm -f "$fixture/header" "$fixture/registry-key"; }
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
# Optional combined-interface gate retains every managed lifecycle assertion
# above. The trusted host supplies credentials; public JSON supplies no paths.
registry=${SAMOSA_TEST_REGISTRY_BINARY:-}
if [ -n "$registry" ]; then
    (umask 077; sed 's/^Authorization: Bearer //' "$fixture/header" > "$fixture/registry-key")
    registry_config() {
        registry_owner=$1 registry_endpoint=$2 registry_key=$3
        (umask 077; cat > "$fixture/registry.json" <<EOF
{"contract_version":1,"owner":"$registry_owner","providers":[{"kind":"text","provider_id":"$provider","state":"$fixture/state/admission","endpoint":"$registry_endpoint","model_alias":"public-managed","context_tokens":4096$registry_key},{"kind":"fake_text","provider_id":"contract-peer","state":"$fixture/peer-state"}]}
EOF
        )
    }
    registry_config qa "http://127.0.0.1:$port" ",\"api_key_file\":\"$fixture/registry-key\""
    printf '{"contract_version":1,"op":"describe"}' | "$registry" --config "$fixture/registry.json" > "$fixture/registry-discovery"
    grep -q 'contract-peer' "$fixture/registry-discovery"
    grep -q "$provider" "$fixture/registry-discovery"
    printf '{"contract_version":1,"op":"count_tokens","provider_id":"%s","prompt":"Compute 2 + 2. Answer only the integer."}' "$provider" | "$registry" --config "$fixture/registry.json" > "$fixture/registry-count"
    grep -q '"input_tokens":24' "$fixture/registry-count"
    printf '{"contract_version":1,"op":"generate","operation_id":"registry-generate","provider_id":"%s","prompt":"Compute 2 + 2. Answer only the integer.","max_output_tokens":32}' "$provider" > "$fixture/registry-request"
    "$registry" --config "$fixture/registry.json" < "$fixture/registry-request" > "$fixture/registry-generated"
    grep -q '"answer":"4"' "$fixture/registry-generated"
    grep -q '"quality":"native"' "$fixture/registry-generated"
    printf '{"contract_version":1,"op":"operation_get","provider_id":"%s","operation_id":"registry-generate"}' "$provider" > "$fixture/registry-status-request"
    "$registry" --config "$fixture/registry.json" < "$fixture/registry-status-request" > "$fixture/registry-status"
    grep -q '"status":"completed"' "$fixture/registry-status"
    # Observe real model work before cancelling through the same public route.
    printf '{"contract_version":1,"op":"generate","operation_id":"registry-active-cancel","provider_id":"%s","prompt":"Write every integer from 1 to 10000, separated by spaces. Do not abbreviate or skip any integers.","max_output_tokens":2048}' "$provider" > "$fixture/registry-long-request"
    "$registry" --config "$fixture/registry.json" < "$fixture/registry-long-request" > "$fixture/registry-cancelled" &
    registry_work_pid=$!
    attempt=0
    while :; do
        if curl -fsS --max-time 1 --noproxy '*' -H "@$fixture/header" "http://127.0.0.1:$port/slots" > "$fixture/registry-slots" 2> "$fixture/registry-slot-stderr" && grep -Eq '"is_processing"[[:space:]]*:[[:space:]]*true' "$fixture/registry-slots"; then break; fi
        attempt=$((attempt+1));[ "$attempt" -lt 25 ];kill -0 "$registry_work_pid";sleep 0.1
    done
    printf '{"contract_version":1,"op":"operation_cancel","provider_id":"%s","operation_id":"registry-active-cancel"}' "$provider" | "$registry" --config "$fixture/registry.json" > "$fixture/registry-cancel-control"
    if wait "$registry_work_pid"; then exit 1; else registry_work_rc=$?; fi
    registry_work_pid=
    [ "$registry_work_rc" = 1 ]
    grep -q '"code":"cancelled"' "$fixture/registry-cancelled"
    sed 's/registry-generate/registry-survivor/' "$fixture/registry-request" | "$registry" --config "$fixture/registry.json" > "$fixture/registry-survivor"
    grep -q '"answer":"4"' "$fixture/registry-survivor"
    registry_config foreign "http://127.0.0.1:$port" ",\"api_key_file\":\"$fixture/registry-key\""
    if "$registry" --config "$fixture/registry.json" < "$fixture/registry-status-request" > "$fixture/registry-foreign"; then exit 1; fi
    grep -q '"code":"unknown_operation"' "$fixture/registry-foreign"
    rm "$fixture/registry-key"
fi
rm "$fixture/header"
exec 3>&-
attempt=0
while kill -0 "$managed_pid" 2>/dev/null; do
    attempt=$((attempt+1));[ "$attempt" -lt 10 ];sleep 1
done
wait "$managed_pid"
managed_pid=
native=${SAMOSA_TEST_OFFLINE_BINARY:-$(dirname "$binary")/samosa-inference-llama}
printf '{"contract_version":1,"op":"generate","operation_id":"public-generate","provider_id":"%s","prompt":"Compute 2 + 2. Answer only the integer.","max_output_tokens":32}' "$provider" | "$native" --llama-endpoint "http://127.0.0.1:$port" --provider-id "$provider" --model-alias public-managed --context-tokens 4096 --owner qa --admission-state "$fixture/state/admission" > "$fixture/offline-retry"
sed -n 's/^.*"request_id":"generate","response":\(.*\)}$/\1/p' "$fixture/results" > "$fixture/generated"
cmp "$fixture/generated" "$fixture/offline-retry"
[ -z "$(find "$fixture/state" -name 'key.*' -print)" ]
if [ -n "$registry" ]; then
    # Retained operation identity is independent of the now-stopped endpoint.
    registry_config qa "http://127.0.0.1:1" ''
    "$registry" --config "$fixture/registry.json" < "$fixture/registry-request" > "$fixture/registry-offline"
    cmp "$fixture/registry-generated" "$fixture/registry-offline"
    printf 'PASS actual text registry: combined discovery, native counting/generation, scoped operation status, observed active cancellation, survivor, foreign owner refusal and credential-free offline replay\n'
fi
printf 'PASS actual managed public CLI: correlated requests, observed active in-stream cancellation, survivor, FIFO EOF shutdown, offline exact retry, no retained key\n'
