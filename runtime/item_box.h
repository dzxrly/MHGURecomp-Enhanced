// Live item box, owned by the game/update thread (the F1 panel runs on that thread too).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ITEM_BOX_ITEM_COUNT 2991
#define ITEM_BOX_SLOT_COUNT 2300

typedef struct ItemBoxEntry {
    uint16_t id;
    bool addable;
    const char *name;
    const char *description;
} ItemBoxEntry;

typedef struct ItemBoxSnapshot {
    bool ready;
    const char *reason;  // English localization key, empty while ready.
    uint64_t session, revision;
    int slot;           // 0..2; -1 before an actual character load.
    char character[64];
    char game_language[4];  // eng, fre, spa, ger, ita, chS, chT.
    unsigned capacity, used_slots;
    uint32_t totals[ITEM_BOX_ITEM_COUNT];
    unsigned catalog_count;
    const ItemBoxEntry *catalog;  // indexed by item ID; retained until the language changes.
} ItemBoxSnapshot;

typedef struct ItemBoxChange {
    uint16_t id;
    uint32_t total;
} ItemBoxChange;

struct Cpu;
void item_box_init(void);
void item_box_tick(struct Cpu *cpu);
// Shared by the normal town hook and MHGU_QUICKHUNT's replacement hook.
void item_box_on_town(uint32_t area);
const ItemBoxSnapshot *item_box_snapshot(void);
bool item_box_apply(uint64_t session, uint64_t revision, const ItemBoxChange *changes,
                    size_t count, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif
