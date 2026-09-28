// Baseline implementation of the same task using plain std::unordered_set /
// std::unordered_map<std::string, ...>. Same I/O and timing harness as
// main.cpp so the two can be benchmarked head-to-head (see REPORT.md).
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

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

    std::unordered_set<std::string> set;
    std::unordered_map<std::string, uint32_t> dupCounts;

    std::string out;
    out.reserve(input.size() / 4 + 64);

    const char* p = input.data();
    const char* end = p + input.size();
    long ops = 0;

    while (p < end) {
        char op = *p;
        if (op == '#') break;
        if (op == '\n' || op == '\r') {
            ++p;
            continue;
        }
        ++p;
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
        const char* wordStart = p;
        while (p < end && *p != '\n' && *p != '\r') ++p;
        const char* wordEnd = p;
        while (wordEnd > wordStart && (wordEnd[-1] == '\r' || wordEnd[-1] == ' ')) --wordEnd;
        while (p < end && (*p == '\n' || *p == '\r')) ++p;

        size_t len = static_cast<size_t>(wordEnd - wordStart);
        if (len == 0) continue;

        std::string word(wordStart, wordEnd);
        ++ops;

        switch (op) {
            case '+': {
                auto result = set.insert(word);
                if (!result.second) ++dupCounts[word];
                break;
            }
            case '-':
                set.erase(word);
                break;
            case '?':
                out += set.count(word) ? "yes\n" : "no\n";
                break;
            default:
                break;
        }
    }

    std::vector<std::pair<std::string, uint32_t>> duplicates(dupCounts.begin(), dupCounts.end());
    std::sort(duplicates.begin(), duplicates.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    out += "---\n";
    char numBuf[16];
    for (const auto& [word, count] : duplicates) {
        out += word;
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
