# Cancellation acknowledgement ordering

Hosted qualification of `3a9bd92` failed the public speech cancellation assertion
on macOS 14 ([run 38754646060](https://github.com/deepanwadhwa/samosa-inference/actions/runs/38754646060));
Ubuntu qualification passed. The native marker was written before the database
acknowledgement. A worker could observe cancellation and commit status `cancelled`
before the subsequent `UPDATE ... WHERE status='running'`, leaving the public
response with `cancel_requested:false`.

`operation_public` now commits the scoped request flag first, then exposes and
syncs the native marker only if that update matched a running operation. Already
terminal operations keep their existing flag. Marker I/O failures still return
an error; this ordering does not claim that every accepted request interrupts a
worker before it completes.

`tests/test_operations_cancel.c` uses the actual public API and an independent
worker SQLite connection. Only the test copy of `operations.c` redirects its
explicit sync call. At marker visibility, the worker checks that the request is
already committed, then immediately commits its cancellation result. The public
response must retain the acknowledgement; a repeated cancellation must preserve
it without sending another marker. No hook enters the production API.

The regression fails against the preceding implementation at the committed-flag
assertion, then passes 31 checks normally and under ASan/UBSan with the fix.
Existing operation gates pass 832 checks in both builds, and native public speech
gates pass 175 checks in both builds. Commands:

```sh
make test-operations-cancel sanitize-operations-cancel test-operations
make test-speech-public sanitize-speech-public
```

The existing operations sanitizer harness was also compiled/run with the same
flags as the umbrella `sanitize` target. Logs:

- `/private/tmp/samosa-cancel-ordering-before.log`
- `/private/tmp/samosa-cancel-ordering.log`
- `/private/tmp/samosa-cancel-existing-sanitize.log`
- `/private/tmp/samosa-cancel-public-speech.log`

This repairs existing qualification behavior. Installation remains the active
implementation component; the later service-controls component is not opened.
