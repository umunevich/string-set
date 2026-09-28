# Task: String Set Data Structure

## Description

Implement a data structure representing a **set of strings**, supporting fast
insertion, removal, and membership queries, along with a variant-specific
analysis feature: detecting and reporting duplicate insertion attempts.

### Domain constraints

- A string is a **non-empty sequence of lowercase Latin letters (`a`–`z`)**,
  at most **15 characters** long.
- The set holds at most **10^6** distinct strings at any time.
- The test/benchmark data set used to validate the implementation must contain
  **at least 10^4–10^5** strings (to demonstrate behavior at scale, not just
  correctness on a handful of inputs).

### Supported operations

| Op  | Meaning                                    |
|-----|---------------------------------------------|
| `+ word` | Add `word` to the set. Not guaranteed to be absent beforehand — adding an already-present string must be a safe no-op (in terms of set membership), but see "Variant-specific part" below for what must additionally happen. |
| `- word` | Remove `word` from the set. Not guaranteed to be present beforehand — removing an absent string must be a safe no-op. |
| `? word` | Query whether `word` is currently in the set. |

## Input format

- Input is read line by line.
- Each line encodes one operation: a single-character operation type, a
  space, then the string operand — e.g. `+ hello`, `- world`, `? foo`.
- The stream terminates with a line containing only `#` (end-of-input
  sentinel). Do not treat `#` as an operation; stop reading immediately upon
  encountering it.
- The total number of operations in the input (excluding the terminating
  `#`) is **at most 10^6**.

## Output format

- For every `?` operation, output exactly one line: `yes` if the string is
  currently in the set, `no` otherwise.
- `+` and `-` operations produce no direct output (their effect is only
  observable through subsequent `?` queries and the duplicate report below).
- Output must be flushed/produced in the same order as the corresponding
  query operations appear in the input.

## Variant-specific requirement: duplicate detection & grouping

In addition to the base set semantics, the program must track **duplicate
insertion attempts** — i.e., every time a `+` operation is issued for a
string that is *already present in the set* at the time of the call.

Requirements:

1. After processing the full input (upon reaching `#`), find all strings
   that were the target of at least one duplicate `+` (a `+` issued while
   the string was already a member).
2. Group these strings and, for each one, report the **count of
   occurrences** — i.e., how many times a redundant `+` was issued for that
   string while it was already present.
3. Output one line per duplicated string, in a clearly specified, stable
   order (recommend: descending by count, then lexicographic ascending as a
   tie-break — state and follow whichever order is chosen), in the format:

   ```
   <word> <count>
   ```

4. Strings that were only ever added once (no redundant `+`), or that were
   removed and later re-added exactly once, must **not** appear in this
   report.
5. This report is produced once, after all operations have been consumed,
   as a distinct final output section — separate from the per-query
   `yes`/`no` stream.

### Clarifying edge cases to handle explicitly

- A string added, then removed, then added again is **not** a duplicate at
  the second add (it was absent at the time), unless it is added a further
  third time while still present.
- Duplicate tracking must be counted independently of removals: removing and
  re-adding resets "presence" but should be considered when deciding whether
  a given `+` counts as a duplicate.
- Duplicate counts must be tracked for the lifetime of the whole run, not
  reset between operations.

## Performance requirements & evaluation

This task is explicitly a **data-structures-and-performance** exercise, not
just a correctness exercise. The submission must include a **detailed
execution time evaluation**, covering:

1. **Complexity analysis** (written, in the report):
   - Expected/average and worst-case time complexity for `+`, `-`, and `?`
     given the chosen underlying structure (e.g., hash table, trie,
     balanced BST) and how string length (≤15) and alphabet size (26)
     factor in.
   - Memory complexity / overhead per stored string.

2. **Empirical benchmarking**:
   - Generate synthetic input files at multiple scales: at minimum
     10^4, 10^5, and up to 10^6 operations, with a realistic mix of `+`,
     `-`, and `?` (including a deliberate fraction of duplicate `+`s and
     of "presence checks that don't exist"/negative `?` lookups, to avoid
     benchmarking only best-case branches).
   - Measure and report wall-clock time (and ideally throughput,
     ops/sec) for each scale, isolating:
     - pure processing time (excluding I/O) vs.
     - end-to-end time (including reading input / writing output), since
       I/O is often the actual bottleneck at 10^6 lines.
   - Present results in a table and/or chart showing how time scales with
     `N` (should be near-linear / O(N) or O(N·L) for a good hash-based
     implementation — flag and explain any super-linear behavior found).
   - Compare against at least one naive baseline (e.g., a plain
     linear-scan list or an unordered structure without hashing by length
     bucket) to demonstrate the chosen structure's advantage — this is
     optional but strongly recommended to "show quality."

3. **Correctness verification alongside performance**:
   - Automated tests (unit tests) covering: empty string set is never
     valid to insert (input guarantees non-empty, but the implementation
     should assert/reject this defensively — decide and document); string
     length exactly 1 and exactly 15 (boundary lengths); repeated add/remove
     cycles; duplicate-report correctness on a small hand-verified example;
     large randomized fuzz test cross-checked against a reference
     (e.g., Python `set()` or C++ `std::unordered_set`) for the same
     operation sequence.
   - A stress test at the 10^6-operation scale that asserts the program
     completes within a stated time budget (define the budget, e.g. a few
     seconds, based on the benchmarking above) — this doubles as a
     regression guard for future changes.

## Deliverables

1. Source code implementing the set (language of choice — a hash-set-backed
   implementation, e.g., `std::unordered_set<string>` in C++ or an
   equivalent hash table, is a natural fit given fixed max length 15 and
   lowercase-only alphabet; a trie is an acceptable alternative if it's
   used to also solve the duplicate-report requirement more elegantly).
2. Input generator script for producing synthetic benchmark inputs at
   10^4 / 10^5 / 10^6 scale with configurable duplicate/negative-query
   ratios.
3. Test suite (unit + fuzz + stress).
4. A short written report (can be part of this repo's README or a
   separate `REPORT.md`) containing:
   - Chosen data structure and justification.
   - Complexity analysis (as above).
   - Benchmark methodology, environment (CPU, RAM, compiler/runtime
     version, flags), and results (table/chart).
   - Discussion of any bottlenecks found (e.g., I/O, string hashing,
     memory allocation churn) and how they were mitigated.

## Definition of done

- All operations (`+`, `-`, `?`) behave correctly per spec, including on
  the edge cases listed above.
- Duplicate-grouping report is correct and appears in the specified format
  and order.
- Program handles the maximum scale (10^6 operations, up to 10^6 distinct
  strings) without excessive memory use or crashing.
- Benchmark results and complexity analysis are documented and support the
  claim that the chosen data structure meets the performance needs implied
  by the constraints (i.e., it must not be, e.g., an O(N) linear scan per
  operation at this scale).
- Test suite passes, including the large-scale stress test within its
  declared time budget.
