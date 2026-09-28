# string-set

A hash-set-backed "set of strings" implementation in C++ (lowercase strings,
1-15 chars, supporting add/remove/contains plus a duplicate-add report).

- [`TASK.md`](TASK.md) — full task description and requirements.
- [`REPORT.md`](REPORT.md) — design, complexity analysis, and benchmark results.

Quick start:

```sh
make all         # build bin/strset, bin/strset_naive, and the tests
make test        # unit + fuzz tests
make stress      # 10^6-op correctness + time-budget check
make bench       # regenerate benchmark data and benchmarks/results.csv
```
