// Custom hash-based "set of strings" for lowercase strings of length 1..15.
//
// Core container: RobinHoodMap<Value> -- an open-addressing hash map with
// robin-hood hashing (bounded probe sequence lengths) and backward-shift
// deletion (no tombstones, so probe sequences never degrade after removals).
// Keys are stored inline as a fixed 16-byte buffer + length byte, so there
// is no per-string heap allocation and no pointer chasing on lookup -- the
// whole entry (key + metadata) lives in one cache line's worth of table
// slot, which is the main reason this beats std::unordered_set<std::string>
// at this problem's scale (see REPORT.md for benchmarks).
//
// Two thin wrappers sit on top of RobinHoodMap:
//   - StringSet:         membership set for the "+", "-", "?" operations.
//   - DuplicateTracker:  persistent counts of redundant "+" operations,
//                        used for the variant-specific duplicate report.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace strset {

constexpr int kMaxLen = 15;

// Fixed-capacity inline key: up to kMaxLen bytes, no heap allocation.
struct Key {
    std::array<char, kMaxLen + 1> buf{};
    uint8_t len = 0;

    Key() = default;

    Key(const char* s, size_t n) {
        if (n == 0 || n > static_cast<size_t>(kMaxLen)) {
            throw std::invalid_argument("string length must be in [1, 15]");
        }
        len = static_cast<uint8_t>(n);
        std::memcpy(buf.data(), s, n);
    }

    explicit Key(const std::string& s) : Key(s.data(), s.size()) {}

    bool operator==(const Key& o) const noexcept {
        return len == o.len && std::memcmp(buf.data(), o.buf.data(), len) == 0;
    }

    // Lexicographic order matching std::string's operator<, computed
    // directly on the inline bytes -- no std::string construction, which
    // matters when this is used as a sort comparator over ~10^5 keys.
    bool operator<(const Key& o) const noexcept {
        uint8_t n = len < o.len ? len : o.len;
        int cmp = n ? std::memcmp(buf.data(), o.buf.data(), n) : 0;
        if (cmp != 0) return cmp < 0;
        return len < o.len;
    }

    std::string toString() const { return std::string(buf.data(), len); }
};

// FNV-1a over the meaningful bytes, mixed with length so that e.g. "ab" and
// "ab\0" (impossible here since we never store the padding, but as a
// defensive habit) can never collide via padding alone.
struct KeyHash {
    size_t operator()(const Key& k) const noexcept {
        uint64_t h = 1469598103934665603ULL ^
                     (static_cast<uint64_t>(k.len) * 0x9E3779B97F4A7C15ULL);
        for (uint8_t i = 0; i < k.len; ++i) {
            h ^= static_cast<unsigned char>(k.buf[i]);
            h *= 1099511628211ULL;
        }
        h ^= h >> 33;
        return static_cast<size_t>(h);
    }
};

// Open-addressing hash map with robin-hood hashing and backward-shift
// deletion. `Value` must be default-constructible and cheaply copyable.
template <typename Value>
class RobinHoodMap {
public:
    explicit RobinHoodMap(size_t initial_capacity = 16) {
        size_t cap = 1;
        while (cap < initial_capacity) cap <<= 1;
        capacity_ = cap;
        mask_ = cap - 1;
        slots_.assign(capacity_, Slot{});
    }

    // Returns pointer to the stored value, or nullptr if key is absent.
    Value* find(const Key& key) noexcept {
        if (size_ == 0) return nullptr;
        size_t h = KeyHash{}(key);
        size_t idx = h & mask_;
        uint32_t dist = 0;
        while (true) {
            Slot& s = slots_[idx];
            if (s.dist == 0) return nullptr;
            if (s.dist - 1 < dist) return nullptr;  // robin-hood early-out
            if (s.hash == h && s.key == key) return &s.value;
            idx = (idx + 1) & mask_;
            ++dist;
        }
    }

    bool contains(const Key& key) noexcept { return find(key) != nullptr; }

    // Inserts `key` with `defaultValue` if absent. Returns {ptr-to-value,
    // true} if newly inserted, {ptr-to-existing-value, false} if the key
    // was already present (existing value is left untouched).
    std::pair<Value*, bool> insert(const Key& key, const Value& defaultValue = Value{}) {
        // Rehash before crossing load factor 0.7 so the following
        // insert_impl call never has to search an over-full table.
        if ((size_ + 1) * 10 > capacity_ * 7) {
            rehash(capacity_ * 2);
        }
        return insert_impl(key, defaultValue);
    }

    // Backward-shift deletion: no tombstones, keeps probe sequences short.
    bool erase(const Key& key) noexcept {
        if (size_ == 0) return false;
        size_t h = KeyHash{}(key);
        size_t idx = h & mask_;
        uint32_t dist = 0;
        while (true) {
            Slot& s = slots_[idx];
            if (s.dist == 0) return false;
            if (s.dist - 1 < dist) return false;
            if (s.hash == h && s.key == key) {
                size_t cur = idx;
                size_t next = (cur + 1) & mask_;
                while (slots_[next].dist > 1) {
                    slots_[cur] = slots_[next];
                    slots_[cur].dist -= 1;
                    cur = next;
                    next = (next + 1) & mask_;
                }
                slots_[cur] = Slot{};  // dist = 0 => empty
                --size_;
                return true;
            }
            idx = (idx + 1) & mask_;
            ++dist;
        }
    }

    size_t size() const noexcept { return size_; }

    template <typename Fn>
    void forEach(Fn&& fn) const {
        for (const Slot& s : slots_) {
            if (s.dist != 0) fn(s.key, s.value);
        }
    }

private:
    struct Slot {
        Key key{};
        Value value{};
        size_t hash = 0;
        uint32_t dist = 0;  // 0 = empty; else (probe distance from home slot) + 1
    };

    // Precondition: table has enough headroom (caller already rehashed).
    std::pair<Value*, bool> insert_impl(Key key, Value value) {
        size_t h = KeyHash{}(key);
        size_t idx = h & mask_;
        uint32_t dist = 0;
        Value* resultPtr = nullptr;
        bool placed = false;
        bool inserted = false;

        while (true) {
            Slot& s = slots_[idx];
            if (s.dist == 0) {
                s.key = key;
                s.value = value;
                s.hash = h;
                s.dist = dist + 1;
                ++size_;
                if (!placed) {
                    resultPtr = &s.value;
                    inserted = true;
                    placed = true;
                }
                return {resultPtr, inserted};
            }
            if (!placed && s.hash == h && s.key == key) {
                resultPtr = &s.value;
                inserted = false;
                placed = true;
                return {resultPtr, inserted};
            }
            uint32_t existing_dist = s.dist - 1;
            if (existing_dist < dist) {
                Key evKey = s.key;
                Value evValue = s.value;
                size_t evHash = s.hash;
                s.key = key;
                s.value = value;
                s.hash = h;
                s.dist = dist + 1;
                if (!placed) {
                    resultPtr = &s.value;
                    inserted = true;
                    placed = true;
                }
                key = evKey;
                value = evValue;
                h = evHash;
                dist = existing_dist;
            }
            idx = (idx + 1) & mask_;
            ++dist;
        }
    }

    void rehash(size_t newCapacity) {
        std::vector<Slot> old = std::move(slots_);
        slots_.assign(newCapacity, Slot{});
        capacity_ = newCapacity;
        mask_ = newCapacity - 1;
        size_ = 0;
        for (auto& s : old) {
            if (s.dist != 0) {
                insert_impl(s.key, s.value);
            }
        }
    }

    std::vector<Slot> slots_;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    size_t size_ = 0;
};

// Membership set backing the "+", "-", "?" operations.
class StringSet {
public:
    explicit StringSet(size_t initial_capacity = 16) : map_(initial_capacity) {}

    // Returns true if the string was newly added (was absent before this
    // call); false if it was already present (caller uses this to decide
    // whether a "+" is a duplicate).
    bool add(const Key& key) {
        auto result = map_.insert(key, Empty{});
        return result.second;
    }

    // Returns true if the string was present and got removed.
    bool remove(const Key& key) { return map_.erase(key); }

    bool contains(const Key& key) noexcept { return map_.contains(key); }

    size_t size() const noexcept { return map_.size(); }

private:
    struct Empty {};
    RobinHoodMap<Empty> map_;
};

// Persistent counter of redundant "+" operations (a "+" issued while the
// string was already present), independent of later removals -- a string
// removed and re-added keeps accumulating into the same counter if it is
// duplicated again later.
class DuplicateTracker {
public:
    explicit DuplicateTracker(size_t initial_capacity = 16) : map_(initial_capacity) {}

    void recordDuplicate(const Key& key) {
        auto result = map_.insert(key, uint32_t{0});
        ++(*result.first);
    }

    std::vector<std::pair<Key, uint32_t>> collect() const {
        std::vector<std::pair<Key, uint32_t>> out;
        out.reserve(map_.size());
        map_.forEach([&](const Key& k, const uint32_t& c) {
            if (c > 0) out.emplace_back(k, c);
        });
        return out;
    }

private:
    RobinHoodMap<uint32_t> map_;
};

}  // namespace strset
