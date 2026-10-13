#!/bin/sh
set -eu
binary=${1:-./build/samosa-inference-download}
fixture=$(mktemp -d /tmp/samosa-remote-key-cli.XXXXXX)
umask 077
checks=0
request(){
    result=$(printf '%s' '{"contract_version":1,"op":"not-real"}' | "$binary" --state "$fixture/state" --url https://example.invalid/artifact \
        --sha256 ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad --size 3 --byte-ceiling 3 --disk-ceiling 96 --bearer-key-file "$1") && rc=0 || rc=$?
    checks=$((checks+1));[ "$rc" = "$2" ] || exit 1
    checks=$((checks+1));if [ "$2" = 2 ]; then [ -z "$result" ]; else case "$result" in *unsupported_operation*) ;; *) exit 1;; esac;fi
    checks=$((checks+1));case "$result" in *private-secret*) exit 1;; esac
}
printf private-secret > "$fixture/key"
request "$fixture/key" 1
printf 'private-secret\n' > "$fixture/key"
request "$fixture/key" 1
chmod 644 "$fixture/key"
request "$fixture/key" 2
chmod 600 "$fixture/key"
ln -s "$fixture/key" "$fixture/link"
request "$fixture/link" 2
ln "$fixture/key" "$fixture/alias"
request "$fixture/key" 2
rm "$fixture/alias"
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
rm "$fixture/key"
printf 'PASS remote key CLI: %s checks; private single-link input, strict bytes, no secret output; QA %s\n' "$checks" "$fixture"
