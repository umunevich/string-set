#!/usr/bin/env bash
# Generates synthetic inputs at increasing scale, runs both implementations
# (custom robin-hood hash set vs std::unordered_set baseline) against each,
# records read/process/write/total timings (parsed from the binaries'
# --timing stderr output) plus an external wall-clock measurement, verifies
# output correctness against a Python-generated ground truth on every scale,
# and writes everything to benchmarks/results.csv for tools/plot_results.py.
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p data benchmarks bin
make -s all

SCALES=(10000 30000 100000 300000 1000000)
REPEATS=5
CSV=benchmarks/results.csv

echo "impl,scale,run,read_ms,process_ms,write_ms,total_ms,wall_ms" > "$CSV"

for scale in "${SCALES[@]}"; do
  infile="data/ops_${scale}.txt"
  vocab=$(( scale / 8 ))
  if [ "$vocab" -lt 100 ]; then vocab=100; fi

  if [ ! -f "$infile" ]; then
    echo "generating $infile (ops=$scale, vocab=$vocab)..." >&2
    python3 tools/gen_input.py --ops "$scale" --vocab-size "$vocab" --seed 42 \
      --out "$infile" --emit-expected
  fi

  for impl in strset strset_naive; do
    bin="bin/${impl}"

    # Correctness check against the independent Python reference (once per
    # impl/scale is enough; timing runs below reuse the same input).
    "$bin" < "$infile" > /tmp/strset_bench_verify.txt
    if ! diff -q /tmp/strset_bench_verify.txt "${infile}.expected" > /dev/null; then
      echo "FATAL: $impl mismatched expected output at scale=$scale" >&2
      diff /tmp/strset_bench_verify.txt "${infile}.expected" | head -20 >&2
      exit 1
    fi

    for run in $(seq 1 "$REPEATS"); do
      t_start=$(date +%s%N)
      "$bin" --timing < "$infile" > /tmp/strset_bench_out.txt 2> /tmp/strset_bench_timing.txt
      t_end=$(date +%s%N)
      wall_ms=$(( (t_end - t_start) / 1000000 ))

      timing_line=$(cat /tmp/strset_bench_timing.txt)
      read_ms=$(grep -o 'read_ms=[0-9.]*' <<< "$timing_line" | cut -d= -f2)
      process_ms=$(grep -o 'process_ms=[0-9.]*' <<< "$timing_line" | cut -d= -f2)
      write_ms=$(grep -o 'write_ms=[0-9.]*' <<< "$timing_line" | cut -d= -f2)
      total_ms=$(grep -o 'total_ms=[0-9.]*' <<< "$timing_line" | cut -d= -f2)

      echo "$impl,$scale,$run,$read_ms,$process_ms,$write_ms,$total_ms,$wall_ms" >> "$CSV"
    done
    echo "  $impl scale=$scale: OK ($REPEATS runs, output verified)" >&2
  done
done

echo "wrote $CSV" >&2
