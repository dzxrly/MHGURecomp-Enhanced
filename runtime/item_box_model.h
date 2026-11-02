// Pure inventory planning. No guest memory is changed until the complete plan fits.
#pragma once
#include "item_box.h"
#include <string.h>

typedef struct ItemBoxSlot { uint16_t id, count; } ItemBoxSlot;

// Nonnegative input is an absolute total; a minus sign subtracts from the displayed total.
// Bound every digit before multiplying, including pasted values too large for an integer.
static inline const char *item_box_quantity(const char *text, uint32_t current, unsigned capacity,
                                            uint32_t *result) {
    bool subtract = *text == '-';
    const char *digits = text + (subtract || *text == '+' ? 1 : 0);
    if (!*digits) return "Enter a whole number. Use a negative number to subtract.";
    for (const char *p = digits; *p; ++p)
        if (*p < '0' || *p > '9') return "Enter a whole number. Use a negative number to subtract.";
    if (!capacity || capacity > ITEM_BOX_SLOT_COUNT) return "The requested total exceeds the item box capacity.";
    uint32_t limit = subtract ? current : 99u * capacity, value = 0;
    for (const char *p = digits; *p; ++p) {
        unsigned digit = (unsigned)(*p - '0');
        if (value > limit / 10 || (value == limit / 10 && digit > limit % 10))
            return subtract ? "The resulting quantity cannot be below zero."
                            : "The requested total exceeds the item box capacity.";
        value = value * 10 + digit;
    }
    *result = subtract ? current - value : value;
    return NULL;
}

static inline bool item_box_totals(const ItemBoxSlot *slots, unsigned capacity,
                                  uint32_t *totals, unsigned *used) {
    if (!capacity || capacity > ITEM_BOX_SLOT_COUNT) return false;
    memset(totals, 0, ITEM_BOX_ITEM_COUNT * sizeof *totals);
    *used = 0;
    for (unsigned i = 0; i < capacity; i++) {
        unsigned id = slots[i].id, n = slots[i].count;
        if (id >= ITEM_BOX_ITEM_COUNT || n > 99 || (!id && n) || (id && !n)) return false;
        if (id) { totals[id] += n; ++*used; }
    }
    return true;
}

// Returns a stable localization key on failure. On failure out is scratch only;
// callers must not commit any of it. Unedited records retain their exact positions.
static inline const char *item_box_plan(const ItemBoxSlot *before, unsigned capacity,
                                       const bool *addable, const ItemBoxChange *changes,
                                       size_t count, ItemBoxSlot *out) {
    uint32_t totals[ITEM_BOX_ITEM_COUNT], left[ITEM_BOX_ITEM_COUNT] = {0};
    bool edited[ITEM_BOX_ITEM_COUNT] = {false}, seen[ITEM_BOX_ITEM_COUNT] = {false};
    unsigned used;
    if (!item_box_totals(before, capacity, totals, &used)) return "The item box contains invalid records.";
    if (count > ITEM_BOX_ITEM_COUNT || (count && !changes)) return "Invalid item changes.";
    for (size_t i = 0; i < count; i++) {
        unsigned id = changes[i].id;
        if (!id || id >= ITEM_BOX_ITEM_COUNT || seen[id]) return "Invalid item changes.";
        seen[id] = true;
        if (changes[i].total > 99u * capacity) return "The requested total exceeds the item box capacity.";
        if (!addable[id] && changes[i].total > totals[id]) return "This item cannot be added to the item box.";
        // A no-op must not rearrange existing stacks or consume extra capacity.
        if (changes[i].total == totals[id]) continue;
        edited[id] = true;
        left[id] = changes[i].total;
    }
    memcpy(out, before, ITEM_BOX_SLOT_COUNT * sizeof *out);
    // Release all edited records first, so a batch can spend space freed by another row.
    for (unsigned i = 0; i < capacity; i++)
        if (edited[out[i].id]) out[i] = (ItemBoxSlot){0, 0};
    // Reuse each item's original locations before using the remaining empty slots.
    for (unsigned i = 0; i < capacity; i++) {
        unsigned id = before[i].id;
        if (edited[id] && left[id]) {
            unsigned n = left[id] > 99 ? 99 : left[id];
            out[i] = (ItemBoxSlot){(uint16_t)id, (uint16_t)n};
            left[id] -= n;
        }
    }
    unsigned free_slot = 0;
    for (unsigned id = 1; id < ITEM_BOX_ITEM_COUNT; id++) {
        while (left[id]) {
            while (free_slot < capacity && out[free_slot].id) ++free_slot;
            if (free_slot == capacity) return "Not enough free item box slots.";
            unsigned n = left[id] > 99 ? 99 : left[id];
            out[free_slot++] = (ItemBoxSlot){(uint16_t)id, (uint16_t)n};
            left[id] -= n;
        }
    }
    return NULL;
}
