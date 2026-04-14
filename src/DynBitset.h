#pragma once
// DynBitset: a lightweight, dynamically-sized bitset for profession bitmasks.
// Replaces the old uint32_t bitmasks so we can support 32+ professions.
//
// Small-buffer optimization: uses an inline uint64_t for <= 64 bits (covers
// any realistic profession count), only falling back to heap allocation for
// 65+ bits. This makes singleBit(), set(), test(), operator|=, etc.
// allocation-free in the common case.

#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>

class DynBitset {
public:
    DynBitset() = default;

    // Construct a zeroed bitset that can hold at least `numBits` bits.
    explicit DynBitset(size_t numBits) {
        size_t n = wordsNeeded(numBits);
        if (n <= 1) {
            m_inline = 0;
        } else {
            m_heap.resize(n, 0);
            m_inline = 0;
        }
    }

    // Set bit at position `pos`.
    void set(size_t pos) {
        size_t w = pos / 64;
        if (w == 0 && !usingHeap()) {
            m_inline |= (uint64_t(1) << (pos % 64));
        } else {
            promoteIfNeeded(w);
            heapWord(w) |= (uint64_t(1) << (pos % 64));
        }
    }

    // Test whether bit at position `pos` is set.
    bool test(size_t pos) const {
        size_t w = pos / 64;
        if (w == 0 && !usingHeap()) {
            return (m_inline & (uint64_t(1) << (pos % 64))) != 0;
        }
        if (usingHeap() && w < m_heap.size()) {
            return (m_heap[w] & (uint64_t(1) << (pos % 64))) != 0;
        }
        return false;
    }

    // Returns true if no bits are set.
    bool none() const {
        if (!usingHeap()) return m_inline == 0;
        for (auto w : m_heap)
            if (w) return false;
        return true;
    }

    // Returns true if any bit is set.
    bool any() const { return !none(); }

    // Bitwise OR-assign: *this |= other.
    DynBitset& operator|=(const DynBitset& other) {
        if (!other.usingHeap() && !usingHeap()) {
            // Fast path: both inline
            m_inline |= other.m_inline;
            return *this;
        }
        // Slow path: at least one uses heap
        promoteIfNeeded(0); // ensure *this is on heap
        size_t otherSize = other.wordCount();
        if (otherSize > m_heap.size())
            m_heap.resize(otherSize, 0);
        if (other.usingHeap()) {
            for (size_t i = 0; i < other.m_heap.size(); ++i)
                m_heap[i] |= other.m_heap[i];
        } else {
            m_heap[0] |= other.m_inline;
        }
        return *this;
    }

    // Bitwise AND: returns a new bitset = *this & other.
    DynBitset operator&(const DynBitset& other) const {
        if (!usingHeap() && !other.usingHeap()) {
            // Fast path: both inline
            DynBitset result;
            result.m_inline = m_inline & other.m_inline;
            return result;
        }
        // Slow path: at least one uses heap
        size_t n = std::min(wordCount(), other.wordCount());
        if (n <= 1) {
            // Only one word of overlap — stay in inline mode to avoid
            // heapWord() on an empty vector (DynBitset(64) is inline).
            DynBitset result;
            result.m_inline = wordAt(0) & other.wordAt(0);
            return result;
        }
        DynBitset result(n * 64);
        for (size_t i = 0; i < n; ++i)
            result.heapWord(i) = wordAt(i) & other.wordAt(i);
        return result;
    }

    // Returns true if (*this & other) has any bit set, without allocating a temporary.
    bool intersectsAny(const DynBitset& other) const {
        if (!usingHeap() && !other.usingHeap()) {
            return (m_inline & other.m_inline) != 0;
        }
        size_t n = std::min(wordCount(), other.wordCount());
        for (size_t i = 0; i < n; ++i) {
            if ((wordAt(i) & other.wordAt(i)) != 0)
                return true;
        }
        return false;
    }

    // Equality comparison.
    bool operator==(const DynBitset& other) const {
        if (!usingHeap() && !other.usingHeap())
            return m_inline == other.m_inline;
        size_t n = std::max(wordCount(), other.wordCount());
        for (size_t i = 0; i < n; ++i) {
            if (wordAt(i) != other.wordAt(i))
                return false;
        }
        return true;
    }

    bool operator!=(const DynBitset& other) const { return !(*this == other); }

    // Check if this bitset contains all bits of `mask` (i.e., (this & mask) == mask).
    bool containsAll(const DynBitset& mask) const {
        if (!usingHeap() && !mask.usingHeap()) {
            return (m_inline & mask.m_inline) == mask.m_inline;
        }
        size_t n = mask.wordCount();
        for (size_t i = 0; i < n; ++i) {
            uint64_t m = mask.wordAt(i);
            uint64_t a = wordAt(i);
            if ((a & m) != m) return false;
        }
        return true;
    }

    // Zero all bits without changing capacity.
    // Inline mode: set m_inline to 0. Heap mode: fill the vector with zeros.
    void clear() {
        if (!usingHeap()) {
            m_inline = 0;
        } else {
            std::fill(m_heap.begin(), m_heap.end(), uint64_t(0));
        }
    }

    // Return to default-constructed empty state (inline mode, zero capacity).
    void reset() {
        m_inline = 0;
        m_heap.clear();
        m_heap.shrink_to_fit();
    }

    // Build a single-bit DynBitset with bit `pos` set.
    // For pos < 64 this is allocation-free thanks to the inline buffer.
    static DynBitset singleBit(size_t pos) {
        DynBitset b;
        b.set(pos);
        return b;
    }

private:
    // Small-buffer optimization: m_inline stores the first word directly.
    // When m_heap is empty, we use m_inline only (supports bits 0..63).
    // When m_heap is non-empty, m_inline is unused and all words are in m_heap.
    uint64_t m_inline = 0;
    std::vector<uint64_t> m_heap;

    static size_t wordsNeeded(size_t bits) {
        return (bits + 63) / 64;
    }

    bool usingHeap() const { return !m_heap.empty(); }

    size_t wordCount() const {
        return usingHeap() ? m_heap.size() : 1;
    }

    // Read word at index i (works for both inline and heap modes).
    uint64_t wordAt(size_t i) const {
        if (!usingHeap()) return (i == 0) ? m_inline : 0;
        return (i < m_heap.size()) ? m_heap[i] : 0;
    }

    // Writable reference to heap word at index i. Must already be on heap.
    uint64_t& heapWord(size_t i) {
        return m_heap[i];
    }

    // Promote from inline to heap representation if needed for word index w.
    void promoteIfNeeded(size_t w) {
        if (!usingHeap()) {
            // Move inline value to heap
            size_t needed = std::max(size_t(1), w + 1);
            m_heap.resize(needed, 0);
            m_heap[0] = m_inline;
            m_inline = 0;
        } else if (w >= m_heap.size()) {
            m_heap.resize(w + 1, 0);
        }
    }
};
