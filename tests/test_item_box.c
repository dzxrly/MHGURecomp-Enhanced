#include "item_box_model.h"
#include <assert.h>
#include <stdio.h>

static ItemBoxSlot before[ITEM_BOX_SLOT_COUNT], after[ITEM_BOX_SLOT_COUNT];
static bool allowed[ITEM_BOX_ITEM_COUNT];

static void reset(void) {
    memset(before, 0, sizeof before);
    memset(after, 0, sizeof after);
    memset(allowed, 1, sizeof allowed);
    allowed[0] = false;
}
static void expect_total(unsigned capacity, unsigned id, unsigned total) {
    uint32_t sums[ITEM_BOX_ITEM_COUNT];
    unsigned used;
    assert(item_box_totals(after, capacity, sums, &used));
    assert(sums[id] == total);
}

static void fixed_cases(void) {
    reset();
    before[0] = (ItemBoxSlot){1, 99};
    before[1] = (ItemBoxSlot){1, 7};
    before[2] = (ItemBoxSlot){2, 15};
    before[8] = (ItemBoxSlot){3, 66}; // locked space is never allocated or rewritten
    ItemBoxChange grow[] = {{1, 200}};
    assert(!item_box_plan(before, 8, allowed, grow, 1, after));
    expect_total(8, 1, 200);
    assert(after[0].count == 99 && after[1].count == 99 && after[3].count == 2);
    assert(!memcmp(before + 2, after + 2, sizeof *before));
    assert(!memcmp(before + 8, after + 8, sizeof before - 8 * sizeof *before));

    ItemBoxChange remove[] = {{1, 0}};
    assert(!item_box_plan(before, 8, allowed, remove, 1, after));
    expect_total(8, 1, 0);
    assert(!after[0].id && !after[1].id && after[2].count == 15);

    ItemBoxChange split[] = {{3, 100}};
    assert(!item_box_plan(before, 8, allowed, split, 1, after));
    expect_total(8, 3, 100);
    assert(after[3].count == 99 && after[4].count == 1);

    reset();
    before[0] = (ItemBoxSlot){1, 99};
    before[1] = (ItemBoxSlot){2, 99};
    ItemBoxChange full[] = {{1, 100}};
    assert(item_box_plan(before, 2, allowed, full, 1, after));
    assert(before[0].count == 99 && before[1].count == 99);
    ItemBoxChange exchange[] = {{1, 198}, {2, 0}};
    assert(!item_box_plan(before, 2, allowed, exchange, 2, after));
    expect_total(2, 1, 198);
    expect_total(2, 2, 0);

    allowed[1] = false;
    assert(item_box_plan(before, 2, allowed, exchange, 2, after));
    ItemBoxChange lower[] = {{1, 1}};
    assert(!item_box_plan(before, 2, allowed, lower, 1, after));
    expect_total(2, 1, 1);

    ItemBoxChange duplicate[] = {{2, 99}, {2, 1}};
    assert(item_box_plan(before, 2, allowed, duplicate, 2, after));
    ItemBoxChange invalid[] = {{ITEM_BOX_ITEM_COUNT, 1}};
    assert(item_box_plan(before, 2, allowed, invalid, 1, after));
    invalid[0] = (ItemBoxChange){0, 0};
    assert(item_box_plan(before, 2, allowed, invalid, 1, after));
    invalid[0] = (ItemBoxChange){2, UINT32_MAX};
    assert(item_box_plan(before, 2, allowed, invalid, 1, after));
    before[0].count = 100;
    assert(item_box_plan(before, 2, allowed, NULL, 0, after));
    before[0] = (ItemBoxSlot){0, 1};
    assert(item_box_plan(before, 2, allowed, NULL, 0, after));

    reset();
    before[0] = (ItemBoxSlot){1, 1};
    before[1] = (ItemBoxSlot){1, 1};
    ItemBoxChange noop[] = {{1, 2}};
    assert(!item_box_plan(before, 2, allowed, noop, 1, after));
    assert(!memcmp(before, after, sizeof before));
}

static uint32_t seed = 0xa57bd019;
static unsigned random_u32(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}

static void varied_batches(void) {
    // Compare complete plans against an independently counted capacity bound.
    // Covers fragmented duplicates, simultaneous removals and additions, and no-ops.
    for (unsigned trial = 0; trial < 1000; trial++) {
        reset();
        unsigned capacity = 1 + random_u32() % 30;
        uint32_t old[ITEM_BOX_ITEM_COUNT], sums[ITEM_BOX_ITEM_COUNT];
        unsigned used;
        for (unsigned i = 0; i < capacity; i++) if (random_u32() % 4) {
            before[i].id = (uint16_t)(1 + random_u32() % 8);
            before[i].count = (uint16_t)(1 + random_u32() % 99);
        }
        before[capacity] = (ItemBoxSlot){21, 42};
        assert(item_box_totals(before, capacity, old, &used));
        ItemBoxChange changes[4];
        bool edited[9] = {false};
        unsigned required = 0;
        for (unsigned i = 0; i < 4; i++) {
            unsigned id = 2 * i + 1;
            changes[i] = (ItemBoxChange){(uint16_t)id, random_u32() % (capacity * 99 + 1)};
            if (trial % 5 == 0) changes[i].total = 0;
            if (trial % 11 == 0) changes[i].total = old[id];
            edited[id] = changes[i].total != old[id];
            if (edited[id]) required += (changes[i].total + 98) / 99;
        }
        for (unsigned i = 0; i < capacity; i++)
            if (before[i].id && !edited[before[i].id]) ++required;
        const char *error = item_box_plan(before, capacity, allowed, changes, 4, after);
        assert((error == NULL) == (required <= capacity));
        if (error) continue;
        assert(item_box_totals(after, capacity, sums, &used));
        for (unsigned id = 1; id <= 8; id++) {
            unsigned expected = id % 2 ? changes[id / 2].total : old[id];
            assert(sums[id] == expected);
        }
        for (unsigned i = 0; i < capacity; i++)
            if (before[i].id && !edited[before[i].id]) assert(!memcmp(before + i, after + i, sizeof *before));
        assert(!memcmp(before + capacity, after + capacity, sizeof before - capacity * sizeof *before));
    }
}

int main(void) {
    uint32_t quantity = 123;
    assert(!item_box_quantity("-20", 100, 1000, &quantity) && quantity == 80);
    assert(!item_box_quantity("-100", 100, 1000, &quantity) && quantity == 0);
    assert(!item_box_quantity("-0", 100, 1000, &quantity) && quantity == 100);
    assert(!item_box_quantity("0", 100, 1000, &quantity) && quantity == 0);
    assert(!item_box_quantity("250", 100, 1000, &quantity) && quantity == 250);
    assert(!item_box_quantity("+250", 100, 1000, &quantity) && quantity == 250);
    assert(!item_box_quantity("+0", 100, 1000, &quantity) && quantity == 0);
    assert(!item_box_quantity("227700", 0, 2300, &quantity) && quantity == 227700);
    assert(item_box_quantity("-101", 100, 1000, &quantity));
    assert(item_box_quantity("99001", 0, 1000, &quantity));
    assert(item_box_quantity("-1", 0, 1000, &quantity));
    assert(item_box_quantity("99999999999999999999999999999", 100, 1000, &quantity));
    assert(item_box_quantity("-99999999999999999999999999999", 100, 1000, &quantity));
    assert(item_box_quantity("", 100, 1000, &quantity));
    assert(item_box_quantity("-", 100, 1000, &quantity));
    assert(item_box_quantity("2.5", 100, 1000, &quantity));
    assert(item_box_quantity("12abc", 100, 1000, &quantity));
    const char *invalid[] = {"+", "++1", "--1", "+-1", "-+1", "1e3", " 1", "1 ", "1\t", "1\n", "1 2", "0x10", "NaN", "１２", "−1"};
    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        quantity = 123;
        assert(item_box_quantity(invalid[i], 100, 1000, &quantity));
        assert(quantity == 123); // Invalid content never supplies a quantity to the writer.
    }
    fixed_cases();
    varied_batches();
    puts("item box planning tests passed");
    return 0;
}
