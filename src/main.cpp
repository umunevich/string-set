// Solution for the "set of strings" task, backed by the custom robin-hood
// hash table in include/string_set.hpp.
//
// Input:  lines of "<op> <word>", op in {+,-,?}, terminated by a line "#".
// Output: one "yes"/"no" per "?" op, in order, followed by the
//         variant-specific duplicate report (see TASK.md).
//
// Usage: ./strset [--timing] < input > output
//   --timing prints "read_ms=.. process_ms=.. write_ms=.. total_ms=.. ops=.."
//   to stderr, split into I/O-read / pure-processing / I/O-write phases so
//   the benchmark harness can tell CPU time apart from I/O time.
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "string_set.hpp"

namespace {

using Clock = std::chrono::steady_clock;
double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// Slurp all of stdin into one contiguous buffer.
std::vector<char> readAllStdin() {
    std::vector<char> buf;
    buf.reserve(1 << 20);
    char chunk[1 << 16];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
        buf.insert(buf.end(), chunk, chunk + n);
    }
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    bool timing = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--timing") == 0) timing = true;
    }

    auto t0 = Clock::now();
    std::vector<char> input = readAllStdin();
    double read_ms = ms_since(t0);

    auto t1 = Clock::now();

    strset::StringSet set;
    strset::DuplicateTracker dups;

    std::string out;
    out.reserve(input.size() / 4 + 64);  // rough upper bound for "yes\n"/"no\n" lines

    const char* p = input.data();
    const char* end = p + input.size();
    long ops = 0;

    while (p < end) {
        char op = *p;
        if (op == '#') break;  // end-of-input sentinel; ignore rest of line
        if (op == '\n' || op == '\r') {  // tolerate stray blank lines
            ++p;
            continue;
        }
        ++p;                 // consume op char
        while (p < end && (*p == ' ' || *p == '\t')) ++p;  // skip separator
        const char* wordStart = p;
        while (p < end && *p != '\n' && *p != '\r') ++p;
        const char* wordEnd = p;
        while (wordEnd > wordStart && (wordEnd[-1] == '\r' || wordEnd[-1] == ' ')) --wordEnd;
        while (p < end && (*p == '\n' || *p == '\r')) ++p;  // consume EOL

        size_t len = static_cast<size_t>(wordEnd - wordStart);
        if (len == 0) continue;  // malformed/blank line: nothing to do

        strset::Key key(wordStart, len);
        ++ops;

        switch (op) {
            case '+': {
                bool wasNew = set.add(key);
                if (!wasNew) dups.recordDuplicate(key);
                break;
            }
            case '-':
                set.remove(key);
                break;
            case '?':
                out += set.contains(key) ? "yes\n" : "no\n";
                break;
            default:
                break;  // unknown op: ignore
        }
    }

    // Variant-specific duplicate report: descending by count, then
    // lexicographically ascending as a tie-break.
    auto duplicates = dups.collect();
    std::sort(duplicates.begin(), duplicates.end(),
              [](const auto& a, const auto& b) {
                  if (a.second != b.second) return a.second > b.second;
                  return a.first < b.first;
              });

    out += "---\n";
    char numBuf[16];
    for (const auto& [key, count] : duplicates) {
        out += key.toString();
        out += ' ';
        auto res = std::to_chars(numBuf, numBuf + sizeof(numBuf), count);
        out.append(numBuf, res.ptr);
        out += '\n';
    }

    double process_ms = ms_since(t1);

    auto t2 = Clock::now();
    std::fwrite(out.data(), 1, out.size(), stdout);
    double write_ms = ms_since(t2);

    if (timing) {
        std::fprintf(stderr,
                      "read_ms=%.3f process_ms=%.3f write_ms=%.3f total_ms=%.3f ops=%ld "
                      "set_size=%zu duplicate_strings=%zu\n",
                      read_ms, process_ms, write_ms, read_ms + process_ms + write_ms, ops,
                      set.size(), duplicates.size());
    }
    return 0;
}
