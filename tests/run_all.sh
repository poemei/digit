#!/bin/sh
# Run every Core test, preserve each report, and print a final aggregate.
set -u

total=0
failed=0
suites=0
suite_failures=0

for name in agent_root runtime module_manager logging; do
    suites=$((suites + 1))
    output=$(./bin/test_"$name" 2>&1)
    status=$?
    printf '%s\n' "./bin/test_$name" "$output"

    summary=$(printf '%s\n' "$output" | sed -n 's/^.* tests: \([0-9][0-9]*\) executed, \([0-9][0-9]*\) failed$/\1 \2/p' | tail -n 1)
    if [ -n "$summary" ]; then
        set -- $summary
        total=$((total + $1))
        failed=$((failed + $2))
    else
        suite_failures=$((suite_failures + 1))
        printf '%s\n' "ERROR: test_$name did not produce a valid summary." >&2
    fi

    if [ "$status" -ne 0 ]; then
        suite_failures=$((suite_failures + 1))
        printf 'ERROR: test_%s exited with status %s.\n' "$name" "$status" >&2
    fi
done

passed=$((total - failed))
printf '\n========================================\n'
printf 'DIGIT CORE TEST SUMMARY\n'
printf 'Suites executed: %s\n' "$suites"
printf 'Assertions executed: %s\n' "$total"
printf 'Assertions passed: %s\n' "$passed"
printf 'Assertions failed: %s\n' "$failed"
printf 'Suite/report errors: %s\n' "$suite_failures"
if [ "$failed" -eq 0 ] && [ "$suite_failures" -eq 0 ]; then
    printf 'OVERALL RESULT: PASS\n'
    exit 0
fi
printf 'OVERALL RESULT: FAIL\n'
exit 1
