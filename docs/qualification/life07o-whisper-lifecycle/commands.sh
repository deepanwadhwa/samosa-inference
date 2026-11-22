#!/bin/sh
# Run from the Inference repository. Scoped recipes were reviewed for no Python.
set -eu
task_chat=${SAMOSA_CHAT_ROOT:-$(cd ../.. && pwd)}
task_guard="$task_chat/tools/without_python.sh"
case ${1:-} in
  native)
    "$task_guard" make test-whisper-host sanitize-whisper-host test-engine-process sanitize-engine-process test-activation sanitize-activation test-whisper sanitize-whisper
    ;;
  legacy)
    "$task_guard" make test-backend-process sanitize-backend-process test-whisper-process sanitize-whisper-process test-specialist-launch sanitize-specialist-launch
    ;;
  compatibility)
    "$task_guard" make test-registry-cli sanitize-registry-cli test-registry-lifecycle sanitize-registry-lifecycle test-registry-embedding-lifecycle test-managed-readiness sanitize-managed-readiness test-managed-host sanitize-managed-host test-summary-owned sanitize-summary-owned test-speech-runtime sanitize-speech-runtime test-speech-worker sanitize-speech-worker test-whisper sanitize-whisper test-vision sanitize-vision test-summary sanitize-summary
    ;;
  standalone)
    task_export=$(cat docs/qualification/life07o-whisper-lifecycle/standalone-source.txt)
    # Prepare/refresh only the source files listed in source-files.sha256 first.
    CARGO_NET_OFFLINE=true "$task_guard" make -C "$task_export" test-whisper-host sanitize-whisper-host test-activation sanitize-activation
    ;;
  actual|actual-sanitize)
    task_binary="$PWD/build/test-whisper-host"
    [ "$1" = actual ] || task_binary="$task_binary-sanitize"
    task_audio=$(cat docs/qualification/life07o-whisper-lifecycle/actual-source.txt)
    MAX_FOOTPRINT_MB=6144 MAX_SWAP_DELTA_MB=0 "$task_guard" bash "$task_chat/tools/run_memory_guarded.sh" "$task_binary" /Users/deepanwadhwa/.samosa/voice/runtime/whisper-cli /Users/deepanwadhwa/.samosa/voice/stt-whisper-base-en/ggml-base.en.bin "$task_audio/speech.wav" "$PWD/build/samosa-inference-providers"
    ;;
  audit)
    "$task_guard" node docs/qualification/life07o-whisper-lifecycle/audit-reuse.mjs
    ;;
  *) printf 'usage: sh %s native|legacy|compatibility|standalone|actual|actual-sanitize|audit\n' "$0" >&2; exit 2 ;;
esac
