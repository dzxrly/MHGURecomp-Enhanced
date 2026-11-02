// GU 1.4 (fb08f1d20fd1204fe7070672777f869b), checked by rom_version before hooks.
// Addresses below are guest addresses: the NSO image offsets plus MAIN_BASE.
#include "item_box.h"
#include "item_box_model.h"
#include "config.h"
#include "romfs.h"
#include "rt.h"
#include <stdlib.h>
#include <string.h>

#define G_BOX          0x1a58438u
#define G_ITEM         0x1a583a8u
#define G_USER         0x1a97f78u
#define G_SAVE         0x1a8c76cu
#define G_PLAYER       0x1a85b58u
#define G_CONTROL      0x1a8ca48u
#define G_QUEST        0x1a8ad90u
#define FN_LOAD        0x5e0db8u
#define LOAD_SELECTED  0x5e25a8u
#define FN_TITLE_MAKE  0x209420u
#define FN_TITLE_ENTER 0x8a6bf8u
#define FN_TOWN_IDLE   0x8bd0fcu
#define FN_MAP_CTOR    0x2059c4u
#define FN_MAP_DTOR    0x2059e4u
#define FN_MY_PLAYER   0x476278u
#define FN_STORABLE    0x38e854u

static ItemBoxSnapshot snapshot = {.reason = "Load a character to edit the item box.", .slot = -1};
static ItemBoxEntry catalog[ITEM_BOX_ITEM_COUNT];
static bool addable[ITEM_BOX_ITEM_COUNT], dummy[ITEM_BOX_ITEM_COUNT];
static ItemBoxSlot baseline[ITEM_BOX_SLOT_COUNT];
static char *text_data;
static bool hooks_ok, entered_town, town_seen, town_ready;
static uint32_t town_area, map_area, box_address, item_address, catalog_table;
static Cpu *frame_cpu;
static char catalog_language[4];
static bool trace_state;
static uint32_t checked_player;
static bool checked_player_ready;

static bool mapped(uint32_t p, uint32_t n) { return p && kernel_mapped(p, n); }
static uint32_t object(uint32_t cell, uint32_t size) {
    uint32_t p = LD32(cell);
    return mapped(p, size) ? p : 0;
}

static void invalidate(void) {
    ++snapshot.session;
    ++snapshot.revision;
    snapshot.ready = false;
    snapshot.reason = "Load a character to edit the item box.";
    snapshot.slot = -1;
    snapshot.character[0] = 0;
    snapshot.capacity = snapshot.used_slots = 0;
    memset(snapshot.totals, 0, sizeof snapshot.totals);
    entered_town = town_seen = town_ready = false;
    town_area = map_area = box_address = item_address = 0;
}

static void character_load(Cpu *c) {
    uint32_t manager = c->r[0];
    int selected = c->r[14] == LOAD_SELECTED && mapped(manager, 0x70) ? (int)LD32(manager + 0x40) : -1;
    // Startup previews load every character through this function too. Only the
    // actual confirm-selection call site can establish a new editing session.
    invalidate();
    fnhook_call_original(c, FN_LOAD);
    if (selected >= 0 && selected < 3) snapshot.slot = selected;
}
static void title_make(Cpu *c) { invalidate(); fnhook_call_original(c, FN_TITLE_MAKE); }
static void title_enter(Cpu *c) { invalidate(); fnhook_call_original(c, FN_TITLE_ENTER); }

void item_box_on_town(uint32_t area) {
    town_area = area;
    town_seen = true;
    if (snapshot.slot >= 0) entered_town = true;
}
static void town_idle(Cpu *c) {
    uint32_t area = c->r[0];
    fnhook_call_original(c, FN_TOWN_IDLE);
    item_box_on_town(area);
}
static void map_ctor(Cpu *c) {
    fnhook_call_original(c, FN_MAP_CTOR);
    map_area = c->r[0];
}
static void map_dtor(Cpu *c) {
    if (map_area == c->r[0]) map_area = 0;
    fnhook_call_original(c, FN_MAP_DTOR);
}

static uint32_t u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// Use the same RomFS overlay resolution as the game (including installed mods).
static char *read_gmd(const char *language, char **parts) {
    char path[96];
    snprintf(path, sizeof path, "nativeNX/%s/table/itemData_%s.gmd", language, language);
    const char *host = romfs_host_path(path);
    FILE *f = host ? fopen(host, "rb") : NULL;
    if (!f) return NULL;
    unsigned char header[40];
    if (fread(header, 1, sizeof header, f) != sizeof header || memcmp(header, "GMD\0\x02\x03\x01\0", 8)) {
        fclose(f); return NULL;
    }
    uint32_t labels = u32(header + 20), count = u32(header + 24), bytes = u32(header + 32);
    uint64_t start = 41ull + u32(header + 36) + 20ull * labels + (labels ? 1024 : 0) + u32(header + 28);
    if (count != ITEM_BOX_ITEM_COUNT * 2 || !bytes || bytes > 16u * 1024u * 1024u || start > 16u * 1024u * 1024u) {
        fclose(f); return NULL;
    }
    char *data = malloc(bytes);
    if (!data || fseek(f, (long)start, SEEK_SET) || fread(data, 1, bytes, f) != bytes || fgetc(f) != EOF) {
        free(data); fclose(f); return NULL;
    }
    fclose(f);
    char *p = data, *end = data + bytes;
    for (unsigned i = 0; i < count; i++) {
        parts[i] = p;
        char *zero = memchr(p, 0, (size_t)(end - p));
        if (!zero) { free(data); return NULL; }
        p = zero + 1;
    }
    if (p != end) { free(data); return NULL; }
    return data;
}

static bool refresh_catalog(Cpu *c, uint32_t item) {
    uint32_t resource = LD32(item + 0x1c);
    if (!mapped(resource, 0x70) || LD32(resource + 0x64) != ITEM_BOX_ITEM_COUNT) return false;
    uint32_t table = LD32(resource + 0x6c);
    if (!mapped(table, 56u * ITEM_BOX_ITEM_COUNT)) return false;
    if (text_data && table == catalog_table && !strcmp(catalog_language, snapshot.game_language)) return true;
    char *parts[ITEM_BOX_ITEM_COUNT * 2];
    char *english = read_gmd("eng", parts);
    if (!english) return false;
    for (unsigned id = 0; id < ITEM_BOX_ITEM_COUNT; id++)
        dummy[id] = !*parts[2 * id] || strstr(parts[2 * id], "DUMMY") != NULL;
    free(english);
    char *data = read_gmd(snapshot.game_language, parts);
    if (!data) return false;
    for (unsigned id = 0; id < ITEM_BOX_ITEM_COUNT; id++) {
        catalog[id].id = (uint16_t)id;
        catalog[id].name = parts[2 * id];
        catalog[id].description = parts[2 * id + 1];
    }
    Cpu saved = *c;
    for (unsigned id = 0; id < ITEM_BOX_ITEM_COUNT; id++) {
        c->r[0] = table + 56u * id;
        guest_call(c, FN_STORABLE);  // the same predicate used by the game's move-to-box UI
        addable[id] = id && !dummy[id] && *catalog[id].name && c->r[0] == 1;
        catalog[id].addable = addable[id];
    }
    *c = saved;
    free(text_data);
    text_data = data;
    catalog_table = table;
    snprintf(catalog_language, sizeof catalog_language, "%s", snapshot.game_language);
    snapshot.catalog = catalog;
    snapshot.catalog_count = ITEM_BOX_ITEM_COUNT;
    ++snapshot.revision;
    return true;
}

static unsigned capacity(uint32_t user) {
    uint32_t bits = LD32(user + 0x958), lab = LD8(user + 0x3614) & 7;
    unsigned books = ((bits >> 21) & 1) + ((bits >> 22) & 1);
    unsigned upgrades = (lab & 1) + ((lab >> 1) & 1) + ((lab >> 2) & 1);
    return 1000 + 200 * books + 300 * upgrades;
}

static void game_language(void) {
    static const char *const codes[] = {"eng", "eng", "fre", "spa", "ger", "ita", "eng", "chT", "chS"};
    static const int host[] = {0, 0, 2, 1, 3, 4, 0, 6, 5};
    uint32_t control = object(G_CONTROL, 0xa6);
    unsigned lang = control ? LD8(control + 0xa5) : 0;
    if (lang >= sizeof codes / sizeof *codes) lang = 0;
    snprintf(snapshot.game_language, sizeof snapshot.game_language, "%s", codes[lang]);
    config_set_game_language(host[lang]);
}

static const char *live_context(Cpu *c, uint32_t *box, uint32_t *item, unsigned *cap) {
    checked_player = 0;
    checked_player_ready = false;
    if (!hooks_ok) return "The item box editor is unavailable for this game build.";
    if (snapshot.slot < 0 || !entered_town) return "Load a character to edit the item box.";
    uint32_t save = object(G_SAVE, 0x70);
    if (!save || LD8(save + 0x68)) return "The game is loading or saving. Please wait.";
    // aQuest sets this before loading and clears it only at its terminal state
    // (image 0x5d98 / 0x7d0c); the village load task does the same (0xfc40 / 0x103d8).
    uint32_t control = object(G_CONTROL, 0xa6);
    if (!control || LD8(control + 0xa3)) return "Wait until the current area has finished loading.";
    uint32_t players = object(G_PLAYER, 0x80), q = object(G_QUEST, 0x54);
    if (!players || !q) return "Wait until the current area has finished loading.";
    unsigned state = LD8(q + 0x50);
    // The town idle method changes +b8 before departing. Quest maps override the
    // common resource-loader method with `return 1` (e.g. image 0x1f94), so their
    // +2c stays zero. Use the quest-start state instead: aQuest calls 0x3ad1a8 to
    // set sQuest+50=2, then waits for players and finally clears control+a3.
    bool town = town_ready && mapped(town_area, 0xbc) && LD32(town_area + 0xb8) == 1;
    bool quest = mapped(map_area, 0xb8) && state == 2;
    if (!town && !quest) return "Wait until the current area has finished loading.";
    if (state == 4 || state == 5 || state == 6 || state == 7 || state == 9)
        return "Wait until the current area has finished loading.";
    Cpu saved = *c;
    c->r[0] = players;
    guest_call(c, FN_MY_PLAYER);
    uint32_t player = c->r[0];
    bool ready = false;
    if (mapped(player, 0x13c8)) {
        uint32_t vt = LD32(player);
        if (mapped(vt, 0x8c)) {
            c->r[0] = player;
            guest_call(c, LD32(vt + 0x88)); // aQuest waits for exactly this readiness query before entering play
            ready = c->r[0] == 1;
        }
    }
    *c = saved;
    checked_player = player;
    checked_player_ready = ready;
    if (!ready) return "Wait until the current area has finished loading.";
    uint32_t data = LD32(player + 0x13c4);
    if (!mapped(data, 0x554)) return "The current character is unavailable.";
    memcpy(snapshot.character, G2H(data + 0x514), sizeof snapshot.character - 1);
    snapshot.character[sizeof snapshot.character - 1] = 0;
    uint32_t user = object(G_USER, 0x3618);
    *box = object(G_BOX, 0x1a + ITEM_BOX_SLOT_COUNT * 4);
    *item = object(G_ITEM, 0x9c + 94 * 4);
    if (!user || !*box || !*item) return "The current character is unavailable.";
    *cap = capacity(user);
    return NULL;
}

void item_box_init(void) {
    trace_state = getenv("MHGU_LOG_ITEM_BOX") != NULL;
    hooks_ok = fnhook_set(FN_LOAD, hle_register("item-box:character-load", character_load));
    hooks_ok &= fnhook_set(FN_TITLE_MAKE, hle_register("item-box:title-make", title_make));
    hooks_ok &= fnhook_set(FN_TITLE_ENTER, hle_register("item-box:title-enter", title_enter));
    hooks_ok &= fnhook_set(FN_TOWN_IDLE, hle_register("item-box:town", town_idle));
    hooks_ok &= fnhook_set(FN_MAP_CTOR, hle_register("item-box:map-create", map_ctor));
    hooks_ok &= fnhook_set(FN_MAP_DTOR, hle_register("item-box:map-destroy", map_dtor));
}

void item_box_tick(Cpu *c) {
    frame_cpu = c;
    town_ready = town_seen;
    town_seen = false;
    game_language();
    uint32_t box = 0, item = 0;
    unsigned cap = 0;
    const char *reason = live_context(c, &box, &item, &cap);
    if (!reason && !refresh_catalog(c, item)) reason = "The game's item catalog could not be read.";
    if (!reason) {
        ItemBoxSlot slots[ITEM_BOX_SLOT_COUNT];
        memcpy(slots, G2H(box + 0x1a), sizeof slots);
        if (!item_box_totals(slots, cap, snapshot.totals, &snapshot.used_slots))
            reason = "The item box contains invalid records.";
        else {
            if (box != box_address || item != item_address || cap != snapshot.capacity || memcmp(slots, baseline, sizeof slots))
                ++snapshot.revision;
            memcpy(baseline, slots, sizeof baseline);
            box_address = box;
            item_address = item;
            snapshot.capacity = cap;
        }
    }
    snapshot.ready = !reason;
    snapshot.reason = reason ? reason : "";
    if (trace_state) {
        static uint32_t previous[12];
        static const char *last_reason;
        uint32_t save = object(G_SAVE, 0x70), q = object(G_QUEST, 0x54), control = object(G_CONTROL, 0xa6);
        uint32_t state[] = {(uint32_t)snapshot.slot, entered_town, town_area, town_ready,
            mapped(town_area, 0xbc) ? LD32(town_area + 0xb8) : ~0u, map_area,
            mapped(map_area, 0xb8) ? LD32(map_area + 0x2c) : ~0u,
            save ? LD8(save + 0x68) : ~0u, q ? LD8(q + 0x50) : ~0u,
            checked_player, checked_player_ready, control ? LD8(control + 0xa3) : ~0u};
        if (last_reason != snapshot.reason || memcmp(previous, state, sizeof state)) {
            LOG("[item-box] slot=%d entered=%u town=%08x/%u mode=%u map=%08x phase=%u busy=%u quest=%u player=%08x/%u loading=%u cap=%u lang=%s ready=%u %s\n",
                snapshot.slot, state[1], state[2], state[3], state[4], state[5], state[6], state[7], state[8],
                state[9], state[10], state[11], snapshot.capacity, snapshot.game_language, snapshot.ready, snapshot.reason);
            memcpy(previous, state, sizeof state);
            last_reason = snapshot.reason;
        }
    }
}

const ItemBoxSnapshot *item_box_snapshot(void) { return &snapshot; }

bool item_box_apply(uint64_t session, uint64_t revision, const ItemBoxChange *changes,
                    size_t count, char *error, size_t error_size) {
    const char *reason = NULL;
    uint32_t box = 0, item = 0;
    unsigned cap = 0;
    if (!frame_cpu || !snapshot.ready) reason = snapshot.reason;
    else reason = live_context(frame_cpu, &box, &item, &cap);
    if (!reason && (session != snapshot.session || revision != snapshot.revision || box != box_address ||
                   item != item_address || cap != snapshot.capacity ||
                   memcmp(G2H(box + 0x1a), baseline, sizeof baseline)))
        reason = "The item box changed. Refresh before applying your edits.";
    ItemBoxSlot planned[ITEM_BOX_SLOT_COUNT];
    if (!reason) reason = item_box_plan(baseline, cap, addable, changes, count, planned);
    if (reason) {
        if (error && error_size) snprintf(error, error_size, "%s", reason);
        return false;
    }
    // This function and the F1 panel run on the same game update thread. Planning,
    // conflict checking and the commit happen without yielding to guest code.
    memcpy(G2H(box + 0x1a), planned, cap * sizeof *planned);
    for (size_t i = 0; i < count; i++) if (changes[i].total) {
        unsigned id = changes[i].id;
        uint32_t at = item + 0x9c + 4 * (id >> 5);
        ST32(at, LD32(at) | (1u << (id & 31))); // obtained history is never cleared
    }
    memcpy(baseline, planned, sizeof baseline);
    item_box_totals(baseline, cap, snapshot.totals, &snapshot.used_slots);
    ++snapshot.revision;
    if (error && error_size) *error = 0;
    return true;
}
