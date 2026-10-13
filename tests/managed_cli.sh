#!/bin/sh
set -eu
binary=${1:-./build/samosa-inference-managed}
fixture=$(mktemp -d /tmp/samosa-managed-cli.XXXXXX)
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
checks=0
run() {
    result=$("$binary" --config "$1" </dev/null) && rc=0 || rc=$?
    checks=$((checks+1))
    [ "$rc" = 2 ] || { printf 'FAIL managed CLI exit: %s\n' "$rc" >&2; exit 1; }
    checks=$((checks+1))
    case "$result" in *"$2"*) ;; *) printf 'FAIL managed CLI: %s\n' "$result" >&2; exit 1;; esac
}
printf '{}' > "$fixture/config"
chmod 600 "$fixture/config"
run "$fixture/config" invalid_managed_configuration
chmod 644 "$fixture/config"
run "$fixture/config" invalid_managed_configuration
chmod 600 "$fixture/config"
ln -s "$fixture/config" "$fixture/link"
run "$fixture/link" invalid_managed_configuration
mkfifo "$fixture/fifo"
run "$fixture/fifo" invalid_managed_configuration
run relative-config invalid_managed_configuration
run "$fixture/missing" invalid_managed_configuration
printf '%064d' 0 > "$fixture/engine"
printf abc > "$fixture/model"
chmod 600 "$fixture/engine" "$fixture/model"
if command -v sha256sum >/dev/null 2>&1; then sha=$(sha256sum "$fixture/engine"); else sha=$(shasum -a 256 "$fixture/engine"); fi
sha=${sha%% *}
config() {
    cat > "$fixture/config" <<EOF
{"contract_version":1,"profile":"$1","state":"$fixture/state","engine":"$fixture/engine","engine_sha256":"$sha","engine_size":64,"model":"$fixture/model","model_sha256":"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","model_size":3,"context_tokens":4096,"model_alias":"fixture","owner":"qa","port":53555,"expected_revision":0,"gpu_layers":0,"threads":1$2}
EOF
}
config llama-static-system-v1 ''
run "$fixture/config" unsupported_engine_binary
config external ''
run "$fixture/config" invalid_managed_configuration
config llama-static-system-v1 ',"url":"http://foreign"'
run "$fixture/config" invalid_managed_configuration
config llama-static-system-v1 ',"port":53556'
run "$fixture/config" invalid_managed_configuration
config llama-embedding-static-system-v1 ''
run "$fixture/config" invalid_managed_embedding_configuration
config llama-embedding-static-system-v1 ',"embedding_config_file":null'
run "$fixture/config" invalid_managed_embedding_configuration
config llama-static-system-v1 ',"embedding_config_file":null'
run "$fixture/config" invalid_managed_embedding_configuration
config llama-static-system-v1 ',"owner|port":"crafted"'
run "$fixture/config" invalid_managed_configuration
config llama-static-system-v1 ',"owner":"duplicate"'
run "$fixture/config" invalid_managed_configuration
cat > "$fixture/embedding" <<EOF
{"model_id":"candidate","model_revision":"pinned","model_sha256":"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","engine_sha256":"$sha","tokenizer_hash":"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","query_prefix":"task: search result | query: ","document_prefix":"","recipe":"mean-l2-v1","dimensions":768}
EOF
chmod 600 "$fixture/embedding"
config llama-embedding-static-system-v1 ",\"embedding_config_file\":\"$fixture/embedding\""
run "$fixture/config" unsupported_engine_binary
sed 's/"model_sha256":"[^"]*"/"model_sha256":"0000000000000000000000000000000000000000000000000000000000000000"/' "$fixture/embedding" > "$fixture/mismatched-embedding"
chmod 600 "$fixture/mismatched-embedding"
config llama-embedding-static-system-v1 ",\"embedding_config_file\":\"$fixture/mismatched-embedding\""
run "$fixture/config" invalid_managed_embedding_configuration
sed 's/"dimensions":768/"dimensions":0/' "$fixture/embedding" > "$fixture/bad-embedding"
chmod 600 "$fixture/bad-embedding"
config llama-embedding-static-system-v1 ",\"embedding_config_file\":\"$fixture/bad-embedding\""
run "$fixture/config" invalid_managed_embedding_configuration
config llama-static-system-v1 ''
printf '\000extra' >> "$fixture/config"
run "$fixture/config" invalid_managed_configuration
config llama-static-system-v1 ''
result=$(LD_PRELOAD= "$binary" --config "$fixture/config" </dev/null) && rc=0 || rc=$?
checks=$((checks+1));[ "$rc" = 2 ]
checks=$((checks+1));case "$result" in *unsafe_engine_environment*) ;; *) exit 1;; esac
checks=$((checks+1));[ -z "$(find "$fixture/state" -name 'key.*' -print)" ]
printf 'PASS managed CLI: %s checks; private host config, bounded schema, artifact/environment refusal and key cleanup\n' "$checks"
