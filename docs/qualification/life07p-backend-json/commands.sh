#!/bin/sh
# Run from Chat. These scoped recipes/scripts were reviewed for no Python.
set -eu
task_root=$PWD
task_module="$task_root/components/samosa-inference"
task_evidence="$task_module/docs/qualification/life07p-backend-json"
task_guard="$task_root/tools/without_python.sh"
case ${1:-} in
  native) "$task_guard" make -C "$task_module" test-backend-json sanitize-backend-json ;;
  assembled)
    "$task_guard" make samosa-gateway test_fake_openai_backend test_fake_native_summarizer test-prompt-budget
    "$task_guard" sh tests/test_web_search.sh
    "$task_guard" sh tests/test_model_selection.sh
    ;;
  standalone)
    task_source=$(cat "$task_evidence/standalone-source.txt")
    "$task_guard" make -C "$task_source" test-backend-json sanitize-backend-json
    ;;
  package) "$task_guard" sh tests/test_package_pdfium.sh ;;
  install) "$task_guard" sh tests/test_runtime_only_release.sh ;;
  actual|actual-sanitize)
    task_client="$task_module/build/test-backend-json"
    task_record="$task_evidence/actual-source.txt"
    if [ "$1" = actual-sanitize ]; then task_client="$task_client-sanitize"; task_record="$task_evidence/actual-sanitize-source.txt"; fi
    "$task_guard" node "$task_evidence/qualify-qwen.mjs" "$task_root/build/samosa-gateway" /private/tmp/samosa-life07h-real.2wwUok/qwen36b /private/tmp/samosa-life07h-real.2wwUok/model "$task_root/tools/run_memory_guarded.sh" "$task_record" "$task_client"
    ;;
  audit) "$task_guard" node "$task_evidence/audit-reuse.mjs" ;;
  *) printf 'usage: sh %s native|assembled|standalone|package|install|actual|actual-sanitize|audit\n' "$0" >&2; exit 2 ;;
esac
