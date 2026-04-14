// DynBitsetTest.cpp — standalone unit tests for DynBitset
// Compile: g++ -std=c++17 -I src tests/DynBitsetTest.cpp -o build/DynBitsetTest

#include "DynBitset.h"
#include <cassert>
#include <cstdio>
#include <utility>

static int passed = 0;

#define TEST(name) static void name()
#define RUN(name) do { name(); ++passed; std::printf("  PASS  %s\n", #name); } while(0)

// ---------------------------------------------------------------------------
// singleBit — inline mode (bits 0..63)
// ---------------------------------------------------------------------------

TEST(singleBit_0) {
    auto b = DynBitset::singleBit(0);
    assert(b.test(0));
    assert(!b.test(1));
    assert(!b.test(63));
    assert(b.any());
}

TEST(singleBit_1) {
    auto b = DynBitset::singleBit(1);
    assert(!b.test(0));
    assert(b.test(1));
    assert(!b.test(2));
}

TEST(singleBit_31) {
    auto b = DynBitset::singleBit(31);
    assert(b.test(31));
    assert(!b.test(30));
    assert(!b.test(32));
}

TEST(singleBit_32) {
    auto b = DynBitset::singleBit(32);
    assert(b.test(32));
    assert(!b.test(31));
    assert(!b.test(33));
}

TEST(singleBit_63) {
    auto b = DynBitset::singleBit(63);
    assert(b.test(63));
    assert(!b.test(0));
    assert(!b.test(62));
}

// ---------------------------------------------------------------------------
// singleBit — heap mode (bits 64+)
// ---------------------------------------------------------------------------

TEST(singleBit_64) {
    auto b = DynBitset::singleBit(64);
    assert(b.test(64));
    assert(!b.test(0));
    assert(!b.test(63));
    assert(!b.test(65));
}

TEST(singleBit_65) {
    auto b = DynBitset::singleBit(65);
    assert(b.test(65));
    assert(!b.test(64));
    assert(!b.test(0));
}

TEST(singleBit_127) {
    auto b = DynBitset::singleBit(127);
    assert(b.test(127));
    assert(!b.test(126));
    assert(!b.test(0));
    assert(!b.test(64));
}

TEST(singleBit_128) {
    auto b = DynBitset::singleBit(128);
    assert(b.test(128));
    assert(!b.test(127));
    assert(!b.test(0));
    assert(!b.test(129));
}

// ---------------------------------------------------------------------------
// operator& — inline & inline
// ---------------------------------------------------------------------------

TEST(and_inline_inline) {
    auto a = DynBitset::singleBit(5);
    a.set(10);
    auto b = DynBitset::singleBit(5);
    b.set(20);
    auto c = a & b;
    assert(c.test(5));
    assert(!c.test(10));
    assert(!c.test(20));
}

TEST(and_inline_inline_disjoint) {
    auto a = DynBitset::singleBit(0);
    auto b = DynBitset::singleBit(1);
    auto c = a & b;
    assert(c.none());
}

// ---------------------------------------------------------------------------
// operator& — inline & heap
// ---------------------------------------------------------------------------

TEST(and_inline_heap) {
    // a is inline (bit 5)
    auto a = DynBitset::singleBit(5);
    // b is heap (bit 5 + bit 100)
    auto b = DynBitset::singleBit(100);
    b.set(5);
    auto c = a & b;
    assert(c.test(5));
    assert(!c.test(100));
}

TEST(and_heap_inline) {
    // a is heap (bit 5 + bit 100)
    auto a = DynBitset::singleBit(100);
    a.set(5);
    // b is inline (bit 5)
    auto b = DynBitset::singleBit(5);
    auto c = a & b;
    assert(c.test(5));
    assert(!c.test(100));
}

// ---------------------------------------------------------------------------
// operator& — heap & heap
// ---------------------------------------------------------------------------

TEST(and_heap_heap) {
    auto a = DynBitset::singleBit(64);
    a.set(100);
    a.set(5);
    auto b = DynBitset::singleBit(64);
    b.set(200);
    b.set(5);
    auto c = a & b;
    assert(c.test(64));
    assert(c.test(5));
    assert(!c.test(100));
    assert(!c.test(200));
}

TEST(and_heap_heap_disjoint) {
    auto a = DynBitset::singleBit(64);
    auto b = DynBitset::singleBit(65);
    auto c = a & b;
    assert(c.none());
}

TEST(and_heap_asymmetric_sizes) {
    // a spans 2 words (bits up to 127), b spans 4 words (bits up to 255).
    // n = min(2,4) = 2, so only the first 2 words are ANDed.
    auto a = DynBitset::singleBit(64);   // heap, word 1
    a.set(5);                             // word 0
    auto b = DynBitset::singleBit(200);  // heap, word 3
    b.set(64);                            // word 1
    b.set(5);                             // word 0
    b.set(100);                           // word 1
    auto c = a & b;
    assert(c.test(5));       // both have it in word 0
    assert(c.test(64));      // both have it in word 1
    assert(!c.test(100));    // only in b's word 1 (a lacks it)
    assert(!c.test(200));    // beyond a's word count, not included
}

// ---------------------------------------------------------------------------
// operator& — n==1 edge case (one heap, one inline, overlap in word 0 only)
// ---------------------------------------------------------------------------

TEST(and_n1_edge_case) {
    // a is inline with bit 3 set
    auto a = DynBitset::singleBit(3);
    // b is heap-allocated but only has 1 word of overlap with a
    // (constructed with >64 bits capacity but only word 0 matters)
    DynBitset b(128); // heap-allocated, 2 words
    b.set(3);
    b.set(100);
    // wordCount for a = 1, wordCount for b = 2, so n = min(1,2) = 1
    // This exercises the n<=1 branch in operator&
    auto c = a & b;
    assert(c.test(3));
    assert(!c.test(100));
    // Result should be inline (n<=1 branch) not heap
    assert(c.any());
}

TEST(and_heap_heap_word0_only) {
    // Both operands are heap-allocated (capacity > 64 bits) but all set bits
    // fall within word 0, so n = min(wordCount, wordCount) > 1 but only
    // word 0 matters. This exercises the general heap-heap AND loop where
    // higher words are all zero.
    DynBitset a(128); // heap: 2 words, all zero
    a.set(7);
    a.set(30);
    DynBitset b(128); // heap: 2 words, all zero
    b.set(7);
    b.set(50);
    auto c = a & b;
    assert(c.test(7));       // common bit survives
    assert(!c.test(30));     // only in a
    assert(!c.test(50));     // only in b
    assert(!c.test(64));     // word 1 is zero in both
    assert(c.any());
}

// ---------------------------------------------------------------------------
// intersectsAny — all mode combinations
// ---------------------------------------------------------------------------

TEST(intersectsAny_inline_inline_yes) {
    auto a = DynBitset::singleBit(7);
    auto b = DynBitset::singleBit(7);
    assert(a.intersectsAny(b));
}

TEST(intersectsAny_inline_inline_no) {
    auto a = DynBitset::singleBit(7);
    auto b = DynBitset::singleBit(8);
    assert(!a.intersectsAny(b));
}

TEST(intersectsAny_inline_heap_yes) {
    auto a = DynBitset::singleBit(7);
    auto b = DynBitset::singleBit(100);
    b.set(7);
    assert(a.intersectsAny(b));
}

TEST(intersectsAny_inline_heap_no) {
    auto a = DynBitset::singleBit(7);
    auto b = DynBitset::singleBit(100);
    assert(!a.intersectsAny(b));
}

TEST(intersectsAny_heap_heap_yes) {
    auto a = DynBitset::singleBit(100);
    auto b = DynBitset::singleBit(100);
    assert(a.intersectsAny(b));
}

TEST(intersectsAny_heap_heap_no) {
    auto a = DynBitset::singleBit(64);
    auto b = DynBitset::singleBit(65);
    assert(!a.intersectsAny(b));
}

TEST(intersectsAny_empty) {
    DynBitset a;
    DynBitset b;
    assert(!a.intersectsAny(b));
}

// ---------------------------------------------------------------------------
// containsAll
// ---------------------------------------------------------------------------

TEST(containsAll_inline_subset) {
    auto a = DynBitset::singleBit(3);
    a.set(5);
    a.set(10);
    auto mask = DynBitset::singleBit(3);
    mask.set(5);
    assert(a.containsAll(mask));
}

TEST(containsAll_inline_not_subset) {
    auto a = DynBitset::singleBit(3);
    auto mask = DynBitset::singleBit(3);
    mask.set(5);
    assert(!a.containsAll(mask));
}

TEST(containsAll_heap_subset) {
    auto a = DynBitset::singleBit(64);
    a.set(100);
    a.set(200);
    auto mask = DynBitset::singleBit(100);
    assert(a.containsAll(mask));
}

TEST(containsAll_heap_not_subset) {
    auto a = DynBitset::singleBit(64);
    auto mask = DynBitset::singleBit(64);
    mask.set(100);
    assert(!a.containsAll(mask));
}

TEST(containsAll_mixed_subset) {
    // a is heap, mask is inline — mask bits are a subset of a's word 0
    auto a = DynBitset::singleBit(100);
    a.set(5);
    auto mask = DynBitset::singleBit(5);
    assert(a.containsAll(mask));
}

TEST(containsAll_inline_this_heap_mask) {
    // this is inline (bit 5 only), mask is heap (bit 5 + bit 100).
    // this lacks bit 100, so containsAll must return false.
    auto a = DynBitset::singleBit(5);
    auto mask = DynBitset::singleBit(100);
    mask.set(5);
    assert(!a.containsAll(mask));
}

TEST(containsAll_empty_mask) {
    auto a = DynBitset::singleBit(10);
    DynBitset mask;
    assert(a.containsAll(mask));
}

TEST(containsAll_self) {
    auto a = DynBitset::singleBit(64);
    a.set(100);
    a.set(3);
    assert(a.containsAll(a));
}

// ---------------------------------------------------------------------------
// operator|= — mixing inline and heap modes
// ---------------------------------------------------------------------------

TEST(orAssign_inline_inline) {
    auto a = DynBitset::singleBit(3);
    auto b = DynBitset::singleBit(10);
    a |= b;
    assert(a.test(3));
    assert(a.test(10));
}

TEST(orAssign_inline_heap) {
    auto a = DynBitset::singleBit(3);
    auto b = DynBitset::singleBit(100);
    a |= b;
    assert(a.test(3));
    assert(a.test(100));
}

TEST(orAssign_heap_inline) {
    auto a = DynBitset::singleBit(100);
    auto b = DynBitset::singleBit(3);
    a |= b;
    assert(a.test(100));
    assert(a.test(3));
}

TEST(orAssign_heap_heap) {
    auto a = DynBitset::singleBit(64);
    auto b = DynBitset::singleBit(200);
    a |= b;
    assert(a.test(64));
    assert(a.test(200));
}

TEST(orAssign_preserves_existing_bits) {
    auto a = DynBitset::singleBit(5);
    a.set(10);
    auto b = DynBitset::singleBit(100);
    b.set(5);
    a |= b;
    assert(a.test(5));
    assert(a.test(10));
    assert(a.test(100));
}

// ---------------------------------------------------------------------------
// Copy/move semantics — heap mode
// ---------------------------------------------------------------------------

TEST(copy_construct_heap) {
    // Create a heap-mode bitset with several bits set
    auto original = DynBitset::singleBit(100);
    original.set(5);
    original.set(64);
    original.set(200);

    // Copy-construct
    DynBitset copy(original);

    // Verify the copy has the same bits
    assert(copy.test(5));
    assert(copy.test(64));
    assert(copy.test(100));
    assert(copy.test(200));
    assert(copy == original);

    // Mutate the copy
    copy.set(300);
    assert(copy.test(300));

    // Verify original is untouched
    assert(!original.test(300));
    assert(original.test(5));
    assert(original.test(64));
    assert(original.test(100));
    assert(original.test(200));
    assert(copy != original);
}

TEST(copy_assign_heap) {
    // Create a heap-mode bitset
    auto original = DynBitset::singleBit(100);
    original.set(5);
    original.set(64);

    // Copy-assign into a different bitset
    DynBitset copy;
    copy = original;

    // Verify the copy has the same bits
    assert(copy.test(5));
    assert(copy.test(64));
    assert(copy.test(100));
    assert(copy == original);

    // Mutate the copy
    copy.set(250);
    assert(copy.test(250));

    // Verify original is untouched
    assert(!original.test(250));
    assert(original.test(5));
    assert(original.test(64));
    assert(original.test(100));
    assert(copy != original);
}

TEST(move_construct_heap) {
    // Create a heap-mode bitset
    auto original = DynBitset::singleBit(100);
    original.set(5);
    original.set(64);
    original.set(200);

    // Move-construct
    DynBitset moved(std::move(original));

    // Verify the moved-to bitset has all the bits
    assert(moved.test(5));
    assert(moved.test(64));
    assert(moved.test(100));
    assert(moved.test(200));

    // Verify the source is in a valid moved-from state (empty).
    // After std::move on a vector member, the source vector is empty,
    // so the bitset falls back to inline mode with m_inline == 0.
    assert(original.none());
    assert(!original.test(5));
    assert(!original.test(100));
}

TEST(move_assign_heap) {
    // Create a heap-mode bitset
    auto original = DynBitset::singleBit(100);
    original.set(5);
    original.set(64);

    // Move-assign into a different bitset
    DynBitset moved;
    moved = std::move(original);

    // Verify the moved-to bitset has all the bits
    assert(moved.test(5));
    assert(moved.test(64));
    assert(moved.test(100));

    // Verify the source is in a valid moved-from state (empty)
    assert(original.none());
    assert(!original.test(5));
    assert(!original.test(100));
}

// ---------------------------------------------------------------------------
// Copy/move semantics — inline mode
// ---------------------------------------------------------------------------

TEST(copy_construct_inline) {
    // Create an inline-mode bitset (all bits < 64)
    auto original = DynBitset::singleBit(5);
    original.set(10);
    original.set(63);

    // Copy-construct
    DynBitset copy(original);

    // Verify the copy has the same bits
    assert(copy.test(5));
    assert(copy.test(10));
    assert(copy.test(63));
    assert(copy == original);

    // Mutate the copy
    copy.set(0);
    assert(copy.test(0));

    // Verify original is untouched
    assert(!original.test(0));
    assert(original.test(5));
    assert(original.test(10));
    assert(original.test(63));
    assert(copy != original);
}

TEST(copy_assign_inline) {
    // Create an inline-mode bitset (all bits < 64)
    auto original = DynBitset::singleBit(7);
    original.set(20);
    original.set(50);

    // Copy-assign into a different bitset
    DynBitset copy;
    copy = original;

    // Verify the copy has the same bits
    assert(copy.test(7));
    assert(copy.test(20));
    assert(copy.test(50));
    assert(copy == original);

    // Mutate the copy
    copy.set(0);
    assert(copy.test(0));

    // Verify original is untouched
    assert(!original.test(0));
    assert(original.test(7));
    assert(original.test(20));
    assert(original.test(50));
    assert(copy != original);
}

TEST(move_construct_inline) {
    // Create an inline-mode bitset (all bits < 64)
    auto original = DynBitset::singleBit(3);
    original.set(15);
    original.set(62);

    // Move-construct
    DynBitset moved(std::move(original));

    // Verify the moved-to bitset has all the bits
    assert(moved.test(3));
    assert(moved.test(15));
    assert(moved.test(62));

    // In inline mode, m_inline is a scalar (uint64_t), so std::move just
    // copies it — the source retains its value. This is the expected
    // "valid but unspecified" moved-from state for trivial types.
    // The key invariant: source is still a valid DynBitset we can query.
    assert(original.test(3));
    assert(original.test(15));
    assert(original.test(62));
}

TEST(move_assign_inline) {
    // Create an inline-mode bitset (all bits < 64)
    auto original = DynBitset::singleBit(1);
    original.set(30);
    original.set(63);

    // Move-assign into a different bitset
    DynBitset moved;
    moved = std::move(original);

    // Verify the moved-to bitset has all the bits
    assert(moved.test(1));
    assert(moved.test(30));
    assert(moved.test(63));

    // In inline mode, m_inline is a scalar (uint64_t), so move-assign
    // just copies it — the source retains its value. This is the expected
    // "valid but unspecified" moved-from state for trivial types.
    assert(original.test(1));
    assert(original.test(30));
    assert(original.test(63));
}

// ---------------------------------------------------------------------------
// Assignment operator edge cases — overwrite, cross-mode, self-assign
// ---------------------------------------------------------------------------

TEST(copy_assign_inline_overwrites_existing) {
    // Destination already has bits set — copy-assign must replace them
    auto a = DynBitset::singleBit(10);
    a.set(20);
    auto b = DynBitset::singleBit(30);
    b.set(40);
    a = b;
    // a should now have exactly bits 30 and 40
    assert(a.test(30));
    assert(a.test(40));
    assert(!a.test(10)); // old bit cleared
    assert(!a.test(20)); // old bit cleared
    assert(a == b);
    // Mutate a, verify b unchanged
    a.set(50);
    assert(a.test(50));
    assert(!b.test(50));
}

TEST(copy_assign_heap_overwrites_existing) {
    // Destination already has heap-mode bits — copy-assign must replace them
    auto a = DynBitset::singleBit(100);
    a.set(200);
    auto b = DynBitset::singleBit(150);
    b.set(250);
    a = b;
    // a should now have exactly bits 150 and 250
    assert(a.test(150));
    assert(a.test(250));
    assert(!a.test(100)); // old bit cleared
    assert(!a.test(200)); // old bit cleared
    assert(a == b);
    // Mutate a, verify b unchanged
    a.set(300);
    assert(a.test(300));
    assert(!b.test(300));
}

TEST(copy_assign_heap_to_inline_dest) {
    // Destination is inline, source is heap — must promote destination
    auto a = DynBitset::singleBit(5);
    auto b = DynBitset::singleBit(100);
    b.set(5);
    a = b;
    assert(a.test(5));
    assert(a.test(100));
    assert(a == b);
    // Mutate a, verify b unchanged
    a.set(200);
    assert(!b.test(200));
}

TEST(copy_assign_inline_to_heap_dest) {
    // Destination is heap, source is inline — must shrink/replace
    auto a = DynBitset::singleBit(100);
    a.set(200);
    auto b = DynBitset::singleBit(5);
    b.set(10);
    a = b;
    assert(a.test(5));
    assert(a.test(10));
    assert(!a.test(100)); // old heap bit cleared
    assert(!a.test(200)); // old heap bit cleared
    assert(a == b);
    // Mutate a, verify b unchanged
    a.set(20);
    assert(!b.test(20));
}

TEST(move_assign_inline_overwrites_existing) {
    // Destination already has bits set — move-assign must replace them
    auto a = DynBitset::singleBit(10);
    a.set(20);
    auto b = DynBitset::singleBit(30);
    b.set(40);
    a = std::move(b);
    // a should now have exactly bits 30 and 40
    assert(a.test(30));
    assert(a.test(40));
    assert(!a.test(10)); // old bit cleared
    assert(!a.test(20)); // old bit cleared
    // In inline mode, source retains value (scalar copy)
    assert(b.test(30));
    assert(b.test(40));
}

TEST(move_assign_heap_overwrites_existing) {
    // Destination already has heap-mode bits — move-assign must replace them
    auto a = DynBitset::singleBit(100);
    a.set(200);
    auto b = DynBitset::singleBit(150);
    b.set(250);
    a = std::move(b);
    // a should now have exactly bits 150 and 250
    assert(a.test(150));
    assert(a.test(250));
    assert(!a.test(100)); // old bit cleared
    assert(!a.test(200)); // old bit cleared
    // Heap move leaves source empty
    assert(b.none());
}

TEST(move_assign_heap_to_inline_dest) {
    // Destination is inline, source is heap
    auto a = DynBitset::singleBit(5);
    auto b = DynBitset::singleBit(100);
    b.set(5);
    a = std::move(b);
    assert(a.test(5));
    assert(a.test(100));
    // Heap move leaves source empty
    assert(b.none());
}

TEST(move_assign_inline_to_heap_dest) {
    // Destination is heap, source is inline
    auto a = DynBitset::singleBit(100);
    a.set(200);
    auto b = DynBitset::singleBit(5);
    b.set(10);
    a = std::move(b);
    assert(a.test(5));
    assert(a.test(10));
    assert(!a.test(100)); // old heap bit cleared
    assert(!a.test(200)); // old heap bit cleared
    // Inline move leaves source intact (scalar copy)
    assert(b.test(5));
    assert(b.test(10));
}

TEST(copy_assign_self_inline) {
    auto a = DynBitset::singleBit(5);
    a.set(10);
    a.set(63);
    const auto& ref = a;
    a = ref; // self-assignment
    // All bits must survive
    assert(a.test(5));
    assert(a.test(10));
    assert(a.test(63));
}

TEST(copy_assign_self_heap) {
    auto a = DynBitset::singleBit(100);
    a.set(5);
    a.set(200);
    const auto& ref = a;
    a = ref; // self-assignment
    // All bits must survive
    assert(a.test(5));
    assert(a.test(100));
    assert(a.test(200));
}

// ---------------------------------------------------------------------------
// Additional edge cases
// ---------------------------------------------------------------------------

TEST(default_constructed_is_empty) {
    DynBitset b;
    assert(b.none());
    assert(!b.any());
    assert(!b.test(0));
    assert(!b.test(63));
    assert(!b.test(64));
}

TEST(equality_inline) {
    auto a = DynBitset::singleBit(5);
    auto b = DynBitset::singleBit(5);
    assert(a == b);
    assert(!(a != b));
}

TEST(equality_heap) {
    auto a = DynBitset::singleBit(100);
    a.set(5);
    auto b = DynBitset::singleBit(100);
    b.set(5);
    assert(a == b);
}

TEST(inequality_inline_heap) {
    auto a = DynBitset::singleBit(5);
    auto b = DynBitset::singleBit(100);
    b.set(5);
    // a has bit 5, b has bits 5 and 100 — not equal
    assert(a != b);
}

TEST(explicit_size_constructor_inline) {
    DynBitset b(64); // wordsNeeded(64) = 1, stays inline
    b.set(0);
    b.set(63);
    assert(b.test(0));
    assert(b.test(63));
}

TEST(explicit_size_constructor_heap) {
    DynBitset b(65); // wordsNeeded(65) = 2, goes to heap
    b.set(0);
    b.set(64);
    assert(b.test(0));
    assert(b.test(64));
}

TEST(promotion_from_empty_inline_to_heap) {
    // Default-constructed DynBitset: m_inline == 0, m_heap is empty (inline mode).
    // Calling set(200) must promote from truly empty inline state to heap
    // allocation via promoteIfNeeded, since word index 200/64 == 3 > 0.
    DynBitset b;
    assert(b.none());
    b.set(200);
    assert(b.test(200));
    assert(!b.test(0));
    assert(!b.test(63));
    assert(!b.test(64));
    assert(!b.test(199));
    assert(!b.test(201));
    assert(b.any());
}

// ---------------------------------------------------------------------------
// clear() — zeroes all bits without changing capacity
// ---------------------------------------------------------------------------

TEST(clear_inline) {
    auto b = DynBitset::singleBit(5);
    b.set(10);
    b.set(63);
    assert(b.any());

    b.clear();
    assert(b.none());
    assert(!b.test(5));
    assert(!b.test(10));
    assert(!b.test(63));

    // Can still set bits after clear
    b.set(20);
    assert(b.test(20));
    assert(b.any());
}

TEST(clear_heap) {
    auto b = DynBitset::singleBit(200);
    b.set(5);
    b.set(64);
    assert(b.any());

    b.clear();
    assert(b.none());
    assert(!b.test(5));
    assert(!b.test(64));
    assert(!b.test(200));

    // Capacity preserved: can set bit 200 again without realloc
    b.set(200);
    assert(b.test(200));
    assert(b.any());
}

// ---------------------------------------------------------------------------
// reset() — returns to default-constructed empty state
// ---------------------------------------------------------------------------

TEST(reset_heap) {
    auto b = DynBitset::singleBit(200);
    b.set(5);
    b.set(64);
    assert(b.any());

    b.reset();
    assert(b.none());
    assert(!b.test(5));
    assert(!b.test(64));
    assert(!b.test(200));

    // After reset, should behave like a default-constructed bitset
    DynBitset empty;
    assert(b == empty);
}

TEST(reset_inline) {
    auto b = DynBitset::singleBit(10);
    b.set(30);
    assert(b.any());

    b.reset();
    assert(b.none());
    assert(!b.test(10));
    assert(!b.test(30));

    DynBitset empty;
    assert(b == empty);
}

// ---------------------------------------------------------------------------

int main() {
    std::printf("Running DynBitset tests...\n\n");

    // singleBit inline
    RUN(singleBit_0);
    RUN(singleBit_1);
    RUN(singleBit_31);
    RUN(singleBit_32);
    RUN(singleBit_63);

    // singleBit heap
    RUN(singleBit_64);
    RUN(singleBit_65);
    RUN(singleBit_127);
    RUN(singleBit_128);

    // operator& inline/inline
    RUN(and_inline_inline);
    RUN(and_inline_inline_disjoint);

    // operator& inline/heap
    RUN(and_inline_heap);
    RUN(and_heap_inline);

    // operator& heap/heap
    RUN(and_heap_heap);
    RUN(and_heap_heap_disjoint);
    RUN(and_heap_asymmetric_sizes);

    // operator& n==1 edge case
    RUN(and_n1_edge_case);
    RUN(and_heap_heap_word0_only);

    // intersectsAny
    RUN(intersectsAny_inline_inline_yes);
    RUN(intersectsAny_inline_inline_no);
    RUN(intersectsAny_inline_heap_yes);
    RUN(intersectsAny_inline_heap_no);
    RUN(intersectsAny_heap_heap_yes);
    RUN(intersectsAny_heap_heap_no);
    RUN(intersectsAny_empty);

    // containsAll
    RUN(containsAll_inline_subset);
    RUN(containsAll_inline_not_subset);
    RUN(containsAll_heap_subset);
    RUN(containsAll_heap_not_subset);
    RUN(containsAll_mixed_subset);
    RUN(containsAll_inline_this_heap_mask);
    RUN(containsAll_empty_mask);
    RUN(containsAll_self);

    // operator|= mixed modes
    RUN(orAssign_inline_inline);
    RUN(orAssign_inline_heap);
    RUN(orAssign_heap_inline);
    RUN(orAssign_heap_heap);
    RUN(orAssign_preserves_existing_bits);

    // Copy/move semantics — heap mode
    RUN(copy_construct_heap);
    RUN(copy_assign_heap);
    RUN(move_construct_heap);
    RUN(move_assign_heap);

    // Copy/move semantics — inline mode
    RUN(copy_construct_inline);
    RUN(copy_assign_inline);
    RUN(move_construct_inline);
    RUN(move_assign_inline);

    // Assignment operator edge cases — overwrite, cross-mode, self-assign
    RUN(copy_assign_inline_overwrites_existing);
    RUN(copy_assign_heap_overwrites_existing);
    RUN(copy_assign_heap_to_inline_dest);
    RUN(copy_assign_inline_to_heap_dest);
    RUN(move_assign_inline_overwrites_existing);
    RUN(move_assign_heap_overwrites_existing);
    RUN(move_assign_heap_to_inline_dest);
    RUN(move_assign_inline_to_heap_dest);
    RUN(copy_assign_self_inline);
    RUN(copy_assign_self_heap);

    // Additional edge cases
    RUN(default_constructed_is_empty);
    RUN(equality_inline);
    RUN(equality_heap);
    RUN(inequality_inline_heap);
    RUN(explicit_size_constructor_inline);
    RUN(explicit_size_constructor_heap);
    RUN(promotion_from_empty_inline_to_heap);

    // clear()
    RUN(clear_inline);
    RUN(clear_heap);

    // reset()
    RUN(reset_heap);
    RUN(reset_inline);

    std::printf("\nAll %d tests passed.\n", passed);
    return 0;
}
