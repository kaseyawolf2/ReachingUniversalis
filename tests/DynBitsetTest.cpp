// DynBitsetTest.cpp — standalone unit tests for DynBitset
// Compile: g++ -std=c++17 -I src tests/DynBitsetTest.cpp -o build/DynBitsetTest

#include "DynBitset.h"
#include <cassert>
#include <cstdio>

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

TEST(and_n1_both_heap_single_word) {
    // Both heap but with exactly 1 word each
    DynBitset a(65); // needs 2 words but let's use a different approach
    // Actually DynBitset(65) = wordsNeeded(65) = 2, so that's 2 words.
    // We need exactly 1-word heap. Let's construct via set() on bit <64
    // after promoting.
    // Actually the n==1 edge case is: min(wordCount(), other.wordCount()) == 1
    // which happens when at least one operand has wordCount()==1 (inline)
    // and the other is heap. Let's test the exact scenario from the code comment.

    // Inline (wordCount=1) & heap with bits only in word 0
    auto a2 = DynBitset::singleBit(10);
    DynBitset b2(128);
    b2.set(10);
    b2.set(20);
    auto c2 = a2 & b2;
    assert(c2.test(10));
    assert(!c2.test(20));
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

    // operator& n==1 edge case
    RUN(and_n1_edge_case);
    RUN(and_n1_both_heap_single_word);

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
    RUN(containsAll_empty_mask);
    RUN(containsAll_self);

    // operator|= mixed modes
    RUN(orAssign_inline_inline);
    RUN(orAssign_inline_heap);
    RUN(orAssign_heap_inline);
    RUN(orAssign_heap_heap);
    RUN(orAssign_preserves_existing_bits);

    // Additional edge cases
    RUN(default_constructed_is_empty);
    RUN(equality_inline);
    RUN(equality_heap);
    RUN(inequality_inline_heap);
    RUN(explicit_size_constructor_inline);
    RUN(explicit_size_constructor_heap);

    std::printf("\nAll %d tests passed.\n", passed);
    return 0;
}
