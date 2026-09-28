// Randomized fuzz test: cross-checks strset::StringSet / DuplicateTracker
// against a reference built from std::unordered_set<std::string> /
// std::unordered_map<std::string,uint32_t> over long random operation
// sequences. Any divergence is a correctness bug in the custom hash table.
#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "string_set.hpp"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

std::string randomWord(std::mt19937& rng, int poolSize) {
    // Small pool of pre-generated words with varied lengths (including the
    // boundary lengths 1 and 15) so duplicates and removals are frequent.
    static std::vector<std::string> pool;
    if (pool.empty()) {
        std::mt19937 seedRng(12345);
        std::uniform_int_distribution<int> lenDist(1, strset::kMaxLen);
        std::uniform_int_distribution<int> chDist('a', 'z');
        for (int i = 0; i < poolSize; ++i) {
            int len = lenDist(seedRng);
            std::string s(len, 'a');
            for (auto& c : s) c = static_cast<char>(chDist(seedRng));
            pool.push_back(s);
        }
    }
    std::uniform_int_distribution<int> pick(0, static_cast<int>(pool.size()) - 1);
    return pool[pick(rng)];
}

void runFuzzRun(unsigned seed, long numOps, int poolSize) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> opDist(0, 99);  // weighted op choice

    strset::StringSet set;
    strset::DuplicateTracker dups;

    std::unordered_set<std::string> refSet;
    std::unordered_map<std::string, uint32_t> refDup;

    for (long i = 0; i < numOps; ++i) {
        std::string word = randomWord(rng, poolSize);
        strset::Key key(word);
        int r = opDist(rng);

        if (r < 45) {  // 45% add
            bool refNew = refSet.insert(word).second;
            if (!refNew) ++refDup[word];
            bool testNew = set.add(key);
            if (!testNew) dups.recordDuplicate(key);
            CHECK(refNew == testNew);
        } else if (r < 75) {  // 30% remove
            bool refHad = refSet.erase(word) > 0;
            bool testHad = set.remove(key);
            CHECK(refHad == testHad);
        } else {  // 25% query
            bool refHas = refSet.count(word) > 0;
            bool testHas = set.contains(key);
            CHECK(refHas == testHas);
        }
    }

    CHECK(set.size() == refSet.size());
    auto collected = dups.collect();
    CHECK(collected.size() == refDup.size());
    for (const auto& [key, count] : collected) {
        auto it = refDup.find(key.toString());
        CHECK(it != refDup.end() && it->second == count);
    }
}

}  // namespace

int main() {
    // Multiple seeds/pool sizes: small pools stress heavy collision/reuse
    // paths, larger pools stress resizing and sparse duplicate tracking.
    struct Run {
        unsigned seed;
        long ops;
        int poolSize;
    };
    std::vector<Run> runs = {
        {1, 200000, 20},    {2, 200000, 500},   {3, 300000, 5},
        {4, 300000, 50000}, {5, 150000, 100000},
    };

    for (const auto& run : runs) {
        runFuzzRun(run.seed, run.ops, run.poolSize);
    }

    if (g_failures == 0) {
        std::printf("fuzz OK: %zu runs, no divergence from reference\n", runs.size());
    } else {
        std::printf("fuzz FAILED: %d divergences\n", g_failures);
    }
    return g_failures == 0 ? 0 : 1;
}
