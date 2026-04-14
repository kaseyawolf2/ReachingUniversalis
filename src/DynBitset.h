#pragma once
// DynBitset: a lightweight, dynamically-sized bitset for profession bitmasks.
// Replaces the old uint32_t bitmasks so we can support 32+ professions.

#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>

class DynBitset {
public:
    DynBitset() = default;

    // Construct a zeroed bitset that can hold at least `numBits` bits.
    explicit DynBitset(size_t numBits)
        : m_words(wordsNeeded(numBits), 0) {}

    // Set bit at position `pos`.
    void set(size_t pos) {
        size_t w = pos / 64;
        if (w >= m_words.size()) m_words.resize(w + 1, 0);
        m_words[w] |= (uint64_t(1) << (pos % 64));
    }

    // Test whether bit at position `pos` is set.
    bool test(size_t pos) const {
        size_t w = pos / 64;
        if (w >= m_words.size()) return false;
        return (m_words[w] & (uint64_t(1) << (pos % 64))) != 0;
    }

    // Returns true if no bits are set.
    bool none() const {
        for (auto w : m_words)
            if (w) return false;
        return true;
    }

    // Returns true if any bit is set.
    bool any() const { return !none(); }

    // Bitwise OR-assign: *this |= other.
    DynBitset& operator|=(const DynBitset& other) {
        if (other.m_words.size() > m_words.size())
            m_words.resize(other.m_words.size(), 0);
        for (size_t i = 0; i < other.m_words.size(); ++i)
            m_words[i] |= other.m_words[i];
        return *this;
    }

    // Bitwise AND: returns a new bitset = *this & other.
    DynBitset operator&(const DynBitset& other) const {
        size_t n = std::min(m_words.size(), other.m_words.size());
        DynBitset result;
        result.m_words.resize(n, 0);
        for (size_t i = 0; i < n; ++i)
            result.m_words[i] = m_words[i] & other.m_words[i];
        return result;
    }

    // Equality comparison.
    bool operator==(const DynBitset& other) const {
        size_t n = std::max(m_words.size(), other.m_words.size());
        for (size_t i = 0; i < n; ++i) {
            uint64_t a = (i < m_words.size()) ? m_words[i] : 0;
            uint64_t b = (i < other.m_words.size()) ? other.m_words[i] : 0;
            if (a != b) return false;
        }
        return true;
    }

    bool operator!=(const DynBitset& other) const { return !(*this == other); }

    // Check if this bitset contains all bits of `mask` (i.e., (this & mask) == mask).
    bool containsAll(const DynBitset& mask) const {
        for (size_t i = 0; i < mask.m_words.size(); ++i) {
            uint64_t m = mask.m_words[i];
            uint64_t a = (i < m_words.size()) ? m_words[i] : 0;
            if ((a & m) != m) return false;
        }
        return true;
    }

    // Build a single-bit DynBitset with bit `pos` set.
    static DynBitset singleBit(size_t pos) {
        DynBitset b;
        b.set(pos);
        return b;
    }

private:
    std::vector<uint64_t> m_words;

    static size_t wordsNeeded(size_t bits) {
        return (bits + 63) / 64;
    }
};
