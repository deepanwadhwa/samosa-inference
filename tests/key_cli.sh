#!/bin/sh
set -eu
binary=${1:-./build/samosa-inference-llama}
fixture=$(mktemp -d /tmp/samosa-key-cli.XXXXXX)
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
checks=0
request() {
    result=$(printf '%s' '{"contract_version":1,"op":"describe"}' | "$binary" \
        --llama-endpoint http://127.0.0.1:1 --provider-id key-test --model-alias key-test \
        --admission-state "$fixture/admission" --api-key-file "$1") && rc=0 || rc=$?
    checks=$((checks+1))
    [ "$rc" = "$2" ] || { printf 'FAIL key CLI exit: %s\n' "$rc" >&2; exit 1; }
    checks=$((checks+1))
    if [ "$2" = 2 ]; then [ -z "$result" ]; else
        case "$result" in *'key-test'*) ;; *) exit 1;; esac
    fi
    checks=$((checks+1))
    case "$result" in *'private-secret'*) exit 1;; esac
}
printf private-secret > "$fixture/key"
chmod 600 "$fixture/key"
request "$fixture/key" 0
printf 'private-secret\n' > "$fixture/key"
request "$fixture/key" 0
chmod 644 "$fixture/key"
request "$fixture/key" 2
chmod 600 "$fixture/key"
ln -s "$fixture/key" "$fixture/link"
request "$fixture/link" 2
request relative-key 2
request "$fixture/missing" 2
mkfifo "$fixture/fifo"
request "$fixture/fifo" 2
request "$fixture" 2
printf 'private-secret\000suffix' > "$fixture/key"
request "$fixture/key" 2
printf 'private-secret\r\n' > "$fixture/key"
request "$fixture/key" 2
: > "$fixture/key"
request "$fixture/key" 2
printf '%0129d' 0 > "$fixture/key"
request "$fixture/key" 2
printf 'private-secret\nextra' > "$fixture/key"
request "$fixture/key" 2
printf 'PASS native key CLI: %s checks; private regular input, strict bytes, no secret output\n' "$checks"
