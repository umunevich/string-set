#!/usr/bin/env bash
# Stress/regression guard: the task's absolute maximum (10^6 operations)
# must complete well within a fixed time budget. Also re-verifies
# correctness at that scale against the Python reference, so this doubles
# as an end-to-end correctness check at the largest supported size.
set -euo pipefail
cd "$(dirname "$0")/.."

BUDGET_MS=${BUDGET_MS:-2000}   # generous vs. the ~70-90ms observed on this machine (see REPORT.md)
INFILE="data/ops_1000000.txt"

make -s all

if [ ! -f "$INFILE" ]; then
  echo "generating $INFILE..." >&2
  python3 tools/gen_input.py --ops 1000000 --vocab-size 125000 --seed 42 \
    --out "$INFILE" --emit-expected
fi

bin/strset < "$INFILE" > /tmp/strset_stress_out.txt
if ! diff -q /tmp/strset_stress_out.txt "${INFILE}.expected" > /dev/null; then
  echo "FATAL: stress test output mismatch at 10^6 ops" >&2
  exit 1
fi
echo "correctness OK at 10^6 ops" >&2

bin/strset --timing < "$INFILE" > /tmp/strset_stress_out.txt 2> /tmp/strset_stress_timing.txt
timing_line=$(cat /tmp/strset_stress_timing.txt)
echo "$timing_line" >&2
total_ms=$(grep -o 'total_ms=[0-9.]*' <<< "$timing_line" | cut -d= -f2)

# Compare as integers (ms rounded down) to avoid depending on bc/awk float compare quirks.
total_ms_int=${total_ms%.*}
if [ "$total_ms_int" -gt "$BUDGET_MS" ]; then
  echo "FATAL: 10^6-op run took ${total_ms}ms, exceeding budget of ${BUDGET_MS}ms" >&2
  exit 1
fi

echo "PASS: 10^6 ops in ${total_ms}ms (budget ${BUDGET_MS}ms)" >&2
