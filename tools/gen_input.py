#!/usr/bin/env python3
"""Synthetic input generator for the "set of strings" task.

Simulates the target's present/absent state while generating so the
resulting file deliberately exercises every interesting branch instead of
being purely random noise:

  - '+' on an already-present word           -> a genuine duplicate
  - '+' on an absent word                    -> a fresh insert
  - '-' on a present word                    -> a real removal
  - '-' on an absent word                    -> a documented no-op
  - '?' on a present word                    -> "yes"
  - '?' on a word never in the vocabulary     -> guaranteed "no"

With --emit-expected, also writes a `<out>.expected` file containing the
exact expected stdout (yes/no stream + duplicate report, same format and
tie-break order as src/main.cpp), so generated files can be used for
end-to-end verification, not just performance timing.
"""
import argparse
import random
import string
import sys


class RandomSet:
    """Dynamic string set supporting O(1) add / discard / membership /
    uniform-random pick (swap-with-last on removal). A plain Python set
    doesn't support O(1) random choice, and converting it to a tuple on
    every '+'/'-'/'?' call would make generation O(ops * |present|)."""

    __slots__ = ("items", "index")

    def __init__(self):
        self.items = []
        self.index = {}

    def __contains__(self, w):
        return w in self.index

    def __len__(self):
        return len(self.items)

    def add(self, w):
        if w in self.index:
            return False
        self.index[w] = len(self.items)
        self.items.append(w)
        return True

    def discard(self, w):
        i = self.index.pop(w, None)
        if i is None:
            return False
        last = self.items.pop()
        if i != len(self.items):
            self.items[i] = last
            self.index[last] = i
        return True

    def random(self, rng):
        return rng.choice(self.items)


def random_word(rng, min_len, max_len):
    n = rng.randint(min_len, max_len)
    return "".join(rng.choice(string.ascii_lowercase) for _ in range(n))


def build_pool(rng, size, min_len, max_len, exclude=None):
    exclude = exclude or frozenset()
    pool = set()
    attempts = 0
    max_attempts = size * 50 + 1000
    while len(pool) < size and attempts < max_attempts:
        w = random_word(rng, min_len, max_len)
        if w not in exclude:
            pool.add(w)
        attempts += 1
    return list(pool)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--ops", type=int, required=True,
                     help="number of operations to generate (excluding the '#' terminator)")
    ap.add_argument("--vocab-size", type=int, default=None,
                     help="number of distinct candidate strings (default: max(100, ops//20))")
    ap.add_argument("--min-len", type=int, default=1)
    ap.add_argument("--max-len", type=int, default=15)
    ap.add_argument("--add-frac", type=float, default=0.5)
    ap.add_argument("--remove-frac", type=float, default=0.2)
    ap.add_argument("--query-frac", type=float, default=0.3)
    ap.add_argument("--dup-add-frac", type=float, default=0.3,
                     help="fraction of '+' ops that deliberately target an already-present word")
    ap.add_argument("--stale-remove-frac", type=float, default=0.3,
                     help="fraction of '-' ops that deliberately target a (likely) absent word")
    ap.add_argument("--miss-query-frac", type=float, default=0.3,
                     help="fraction of '?' ops that target a word never in the vocabulary at all")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--out", required=True)
    ap.add_argument("--emit-expected", action="store_true",
                     help="also write <out>.expected with the exact expected stdout")
    args = ap.parse_args()

    if not (1 <= args.min_len <= args.max_len <= 15):
        ap.error("require 1 <= min-len <= max-len <= 15")

    rng = random.Random(args.seed)
    vocab_size = args.vocab_size or max(100, args.ops // 20)

    vocab = build_pool(rng, vocab_size, args.min_len, args.max_len)
    miss_pool = build_pool(rng, max(50, vocab_size // 10), args.min_len, args.max_len,
                            exclude=set(vocab))

    total_frac = args.add_frac + args.remove_frac + args.query_frac
    add_frac = args.add_frac / total_frac
    remove_frac = args.remove_frac / total_frac

    present = RandomSet()
    dup_counts = {}
    lines = []
    expected_lines = []

    for _ in range(args.ops):
        r = rng.random()
        if r < add_frac:
            op = "+"
        elif r < add_frac + remove_frac:
            op = "-"
        else:
            op = "?"

        if op == "+":
            if len(present) and rng.random() < args.dup_add_frac:
                word = present.random(rng)
            else:
                word = rng.choice(vocab)
            if word in present:
                dup_counts[word] = dup_counts.get(word, 0) + 1
            else:
                present.add(word)
        elif op == "-":
            if len(present) and rng.random() >= args.stale_remove_frac:
                word = present.random(rng)
            else:
                word = rng.choice(vocab)
            present.discard(word)
        else:  # '?'
            if len(present) and rng.random() >= args.miss_query_frac:
                word = present.random(rng)
            elif miss_pool:
                word = rng.choice(miss_pool)
            else:
                word = rng.choice(vocab)
            expected_lines.append("yes" if word in present else "no")

        lines.append(f"{op} {word}")

    lines.append("#")

    with open(args.out, "w") as f:
        f.write("\n".join(lines))
        f.write("\n")

    if args.emit_expected:
        expected_lines.append("---")
        for word, count in sorted(dup_counts.items(), key=lambda kv: (-kv[1], kv[0])):
            expected_lines.append(f"{word} {count}")
        with open(args.out + ".expected", "w") as f:
            f.write("\n".join(expected_lines))
            f.write("\n")

    print(
        f"wrote {args.ops} ops to {args.out} "
        f"(vocab={len(vocab)}, distinct present at end={len(present)}, "
        f"duplicated strings={len(dup_counts)})",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
