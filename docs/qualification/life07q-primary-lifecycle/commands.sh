#!/bin/sh
# Run from Chat. Scoped native recipes only; broad historical aggregates may use Python.
set -eu
task_root=$PWD
task_module="$task_root/components/samosa-inference"
task_q="$task_module/docs/qualification/life07q-primary-lifecycle"
task_guard="$task_root/tools/without_python.sh"
case ${1:-} in
  native) "$task_guard" make -C "$task_module" test-backend-lifecycle sanitize-backend-lifecycle test-backend-process sanitize-backend-process ;;
  standalone) "$task_guard" make -C "$(cat "$task_q/standalone-source.txt")" test-backend-lifecycle sanitize-backend-lifecycle ;;
  assembled)
    "$task_guard" make samosa-gateway test_fake_openai_backend test_fake_native_summarizer test-prompt-budget
    "$task_guard" sh tests/test_model_selection.sh
    "$task_guard" "$task_module/build/test-backend-process" "$task_module/build/test-backend-process" --gateway "$task_root/build/samosa-gateway"
    "$task_guard" "$task_module/build/test-backend-process" "$task_module/build/test-backend-process" --gateway-crash "$task_root/build/samosa-gateway"
    ;;
  package) "$task_guard" sh tests/test_package_pdfium.sh ;;
  install) "$task_guard" sh tests/test_runtime_only_release.sh ;;
  actual)
    "$task_guard" node "$task_module/docs/qualification/life07p-backend-json/qualify-qwen.mjs" "$task_root/build/samosa-gateway" /private/tmp/samosa-life07h-real.2wwUok/qwen36b /private/tmp/samosa-life07h-real.2wwUok/model "$task_root/tools/run_memory_guarded.sh" "$task_q/actual-source.txt" "$task_module/build/test-backend-json"
    ;;
  baseline)
    task_tmp=$(mktemp -d /private/tmp/samosa-life07q-baseline.XXXXXX)
    trap 'rm -rf "$task_tmp"' EXIT HUP INT TERM
    git -C "$task_module" show db99bf7:src/backend_process.c > "$task_tmp/owner.c"
    "$task_guard" cc -O2 -g -std=c11 -Wall -Wextra -Werror -Wpedantic -pthread -I"$task_module/include" -I"$task_module/src" "$task_module/tests/test_backend_lifecycle.c" "$task_tmp/owner.c" "$task_module/src/llama_launch.c" "$task_module/src/engine_process.c" -o "$task_tmp/baseline"
    task_result=0
    "$task_guard" "$task_tmp/baseline" "$task_tmp/baseline" > "$task_tmp/result.log" 2>&1 || task_result=$?
    cat "$task_tmp/result.log"
    [ "$task_result" -eq 1 ] && grep -q '!lease_available(c.marker)' "$task_tmp/result.log"
    ;;
  audit) "$task_guard" node "$task_q/audit-reuse.mjs" ;;
  *) printf 'usage: sh %s native|standalone|assembled|package|install|actual|baseline|audit\n' "$0" >&2;exit 2 ;;
esac
