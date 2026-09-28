// Lightweight unit tests for strset::StringSet / DuplicateTracker.
// No external test framework: a small CHECK macro tallies pass/fail and
// main() returns non-zero (and prints a summary) if anything failed.
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>

#include "string_set.hpp"

namespace {
int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

using strset::DuplicateTracker;
using strset::Key;
using strset::StringSet;

void test_boundary_lengths() {
    StringSet set;
    CHECK(set.add(Key("a", 1)) == true);
    CHECK(set.contains(Key("a", 1)) == true);
    CHECK(set.add(Key("a", 1)) == false);  // already present

    std::string s15(15, 'z');
    CHECK(set.add(Key(s15)) == true);
    CHECK(set.contains(Key(s15)) == true);
    CHECK(set.size() == 2);
}

void test_rejects_out_of_range_lengths() {
    bool threwEmpty = false;
    try {
        Key k("", 0);
        (void)k;
    } catch (const std::invalid_argument&) {
        threwEmpty = true;
    }
    CHECK(threwEmpty);

    bool threwTooLong = false;
    std::string s16(16, 'a');
    try {
        Key k(s16);
        (void)k;
    } catch (const std::invalid_argument&) {
        threwTooLong = true;
    }
    CHECK(threwTooLong);
}

void test_add_remove_cycles() {
    StringSet set;
    Key k("hello", 5);
    CHECK(set.add(k) == true);
    CHECK(set.contains(k) == true);
    CHECK(set.remove(k) == true);
    CHECK(set.contains(k) == false);
    CHECK(set.remove(k) == false);  // removing absent string is a no-op
    CHECK(set.add(k) == true);      // re-add after removal is NOT a duplicate
    CHECK(set.contains(k) == true);
    CHECK(set.size() == 1);
}

void test_many_distinct_strings_survive_resizes() {
    StringSet set;
    const int N = 5000;
    for (int i = 0; i < N; ++i) {
        std::string s = "k" + std::to_string(i);
        CHECK(set.add(Key(s)) == true);
    }
    CHECK(set.size() == static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        std::string s = "k" + std::to_string(i);
        CHECK(set.contains(Key(s)) == true);
    }
    // Remove every other one, verify survivors are intact (backward-shift
    // deletion must not corrupt probe chains of neighboring entries).
    for (int i = 0; i < N; i += 2) {
        std::string s = "k" + std::to_string(i);
        CHECK(set.remove(Key(s)) == true);
    }
    CHECK(set.size() == static_cast<size_t>(N / 2));
    for (int i = 0; i < N; ++i) {
        std::string s = "k" + std::to_string(i);
        bool expected = (i % 2 != 0);
        CHECK(set.contains(Key(s)) == expected);
    }
}

// Hand-verified duplicate-report example, mirroring the sequence documented
// in TASK.md's "clarifying edge cases" section.
void test_duplicate_report_hand_verified() {
    StringSet set;
    DuplicateTracker dups;

    auto apply_add = [&](const char* w) {
        Key k(w, std::strlen(w));
        if (!set.add(k)) dups.recordDuplicate(k);
    };
    auto apply_remove = [&](const char* w) { set.remove(Key(w, std::strlen(w))); };

    // "cat": + + +  -> present after 1st, duplicate at 2nd and 3rd => count 2
    apply_add("cat");
    apply_add("cat");
    apply_add("cat");

    // "dog": + - +  -> re-added after removal, NOT a duplicate => no entry
    apply_add("dog");
    apply_remove("dog");
    apply_add("dog");

    // "owl": + - + +  -> re-added (not dup), then duplicated once => count 1
    apply_add("owl");
    apply_remove("owl");
    apply_add("owl");
    apply_add("owl");

    // "ant": single add, never duplicated => no entry
    apply_add("ant");

    std::map<std::string, uint32_t> got;
    for (const auto& [k, c] : dups.collect()) got[k.toString()] = c;

    CHECK(got.size() == 2);
    CHECK(got.count("cat") == 1 && got["cat"] == 2);
    CHECK(got.count("owl") == 1 && got["owl"] == 1);
    CHECK(got.count("dog") == 0);
    CHECK(got.count("ant") == 0);

    CHECK(set.contains(Key("cat", 3)) == true);
    CHECK(set.contains(Key("dog", 3)) == true);
    CHECK(set.contains(Key("owl", 3)) == true);
    CHECK(set.contains(Key("ant", 3)) == true);
}

}  // namespace

int main() {
    test_boundary_lengths();
    test_rejects_out_of_range_lengths();
    test_add_remove_cycles();
    test_many_distinct_strings_survive_resizes();
    test_duplicate_report_hand_verified();

    std::printf("%d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
