// Atomic INI persistence and live settings. Cold boot state is never mutated;
// each consumer reads hot values atomically on its own thread.
#include "config.h"
#include <SDL3/SDL.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

static Config saved, startup;
static Config graphics;
static uint64_t graphics_revision;
static SDL_SpinLock graphics_lock;
static uint32_t active_render[2], active_output[2];
void config_graphics_dimensions(uint32_t render[2], uint32_t output[2], bool write) {
    SDL_LockSpinlock(&graphics_lock);
    if (write) { memcpy(active_render, render, sizeof active_render); memcpy(active_output, output, sizeof active_output); }
    else { memcpy(render, active_render, sizeof active_render); memcpy(output, active_output, sizeof active_output); }
    SDL_UnlockSpinlock(&graphics_lock);
}
static SDL_AtomicInt live_prewarm, active_fsr, active_temporal;

bool config_graphics_snapshot(Config *out, uint64_t *revision) {
    SDL_LockSpinlock(&graphics_lock);
    bool changed = *revision != graphics_revision;
    if (changed) { *out = graphics; *revision = graphics_revision; }
    SDL_UnlockSpinlock(&graphics_lock);
    return changed;
}
static bool have_saved, runtime_ready;
static SDL_AtomicInt graphics_backend, neural_backend;
static SDL_AtomicInt graphics_fallback;
static SDL_SpinLock fg_lock;
static int fg_status, fg_max_multiplier, fg_render_fps, fg_display_fps;
void config_fg_report(int status, int max_multiplier, int render_fps, int display_fps) {
    SDL_LockSpinlock(&fg_lock);
    fg_status = status;
    fg_max_multiplier = max_multiplier;
    fg_render_fps = render_fps;
    fg_display_fps = display_fps;
    SDL_UnlockSpinlock(&fg_lock);
}
void config_fg_status(int *status, int *max_multiplier, int *render_fps, int *display_fps) {
    SDL_LockSpinlock(&fg_lock);
    *status = fg_status;
    *max_multiplier = fg_max_multiplier;
    *render_fps = fg_render_fps;
    *display_fps = fg_display_fps;
    SDL_UnlockSpinlock(&fg_lock);
}
PerformanceOverlay config_performance_overlay(void) {
    SDL_LockSpinlock(&graphics_lock);
    PerformanceOverlay overlay = graphics.performance_overlay;
    SDL_UnlockSpinlock(&graphics_lock);
    return overlay;
}
void config_graphics_fallback(int reason) { SDL_SetAtomicInt(&graphics_fallback, reason); }
const char *config_graphics_reason(void) {
    switch (SDL_GetAtomicInt(&graphics_fallback)) {
    case GFX_FALLBACK_FSR4_GPU: return "This GPU can't run FSR 4, so FSR 3.1 is used.";
    case GFX_FALLBACK_FSR4_START: return "FSR 4 could not start, so FSR 3.1 is used. The [fsr4] lines in log.txt say why.";
    default: return NULL;
    }
}
void config_graphics_report(bool neural, int status) {
    SDL_SetAtomicInt(neural ? &neural_backend : &graphics_backend, status);
}
bool config_upscaling_active(void) {
    switch (SDL_GetAtomicInt(&graphics_backend)) {
    case GFX_FSR3: case GFX_FSR4: case GFX_DLSS: case GFX_DLSS_SL: case GFX_XESS: {
        uint32_t render[2], output[2];
        config_graphics_dimensions(render, output, false);
        return render[0] && render[1] && (render[0] < output[0] || render[1] < output[1]);
    }
    default:
        return false;
    }
}
const char *config_graphics_status(bool neural) {
    switch (SDL_GetAtomicInt(neural ? &neural_backend : &graphics_backend)) {
    case GFX_WAITING: return "Waiting for a 3D scene";
    case GFX_FSR3: return "FSR 3.1";
    case GFX_FSR4: return "FSR 4";
    case GFX_DLSS: return "DLSS SR / DLAA";
    case GFX_DLSS_SL: return "DLSS SR / DLAA (Streamline)";
    case GFX_XESS: return "XeSS SR";
    case GFX_NR: return "Active";
    case GFX_UNAVAILABLE: return "Unavailable";
    case GFX_DRIVER_OLD: return "Unavailable: NGX driver too old";
    default: return neural ? "Off" : "Native";
    }
}
static SDL_AtomicInt volume = {100}, confirm_exit = {1}, language = {0}, mute_in_background, window_focused = {1};
static SDL_AtomicInt live_fps, live_fov, live_mouse, live_draw = {0x3f800000}, live_fxaa = {1}, live_msaa = {1},
                     live_vsync = {1}, live_face, live_menu_confirm, live_background = {1}, live_combo, live_chat_key,
                     live_fxaa_mode, live_smooth;
static void set_float(SDL_AtomicInt *slot, float value) {
    int bits; _Static_assert(sizeof bits == sizeof value, "32-bit float settings");
    memcpy(&bits, &value, sizeof bits); SDL_SetAtomicInt(slot, bits);
}
static float get_float(SDL_AtomicInt *slot) {
    int bits = SDL_GetAtomicInt(slot); float value; memcpy(&value, &bits, sizeof value); return value;
}
static SDL_SpinLock live_ldn_lock;  // [Multiplayer] Server / Key: read by the network threads (runtime/net/ldn.c)
static char live_ldn_server[sizeof saved.ldn_server], live_ldn_key[sizeof saved.ldn_key];
// A save updates changed hot fields only. Saving the volume must not silently
// remove an unchanged FOV/FPS override supplied by the development environment.
static void publish_live(const Config *c, bool all) {
    SDL_LockSpinlock(&graphics_lock);
    graphics.performance_overlay = c->performance_overlay;
    bool changed = all;
    for (int i = 0; i < g_nsettings; i++) {
        const ConfigSetting *s = &g_settings[i];
        if (strcmp(s->section, "Graphics") || s->restart) continue;
        if (!all && config_equal(s, c, &saved)) continue;
        size_t size = s->kind == SET_FLOAT || s->kind == SET_ASPECT ? sizeof(float) : sizeof(int);
        memcpy((char *)&graphics + s->field, (const char *)c + s->field, size);
        if (s->kind == SET_SIZE) memcpy((char *)&graphics + s->field2, (const char *)c + s->field2, sizeof(int));
        changed = true;
    }
    if (changed) ++graphics_revision;
    SDL_UnlockSpinlock(&graphics_lock);
    SDL_SetAtomicInt(&active_fsr, graphics.fsr);
    SDL_SetAtomicInt(&active_temporal, graphics.fsr || graphics.dlss_nr || g_config.frame_generation != FG_OFF);
    SDL_SetAtomicInt(&live_prewarm, graphics.shader_prewarm);
    if (all || c->fps != saved.fps) set_float(&live_fps, (float)c->fps);
    if (all || c->fov != saved.fov) set_float(&live_fov, c->fov);
    if (all || c->mouse_sensitivity != saved.mouse_sensitivity) set_float(&live_mouse, c->mouse_sensitivity);
    if (all || c->draw_distance != saved.draw_distance) set_float(&live_draw, c->draw_distance);
    if (all || c->fxaa != saved.fxaa) SDL_SetAtomicInt(&live_fxaa, c->fxaa);
    if (all || c->msaa != saved.msaa) SDL_SetAtomicInt(&live_msaa, c->msaa);
    if (all || c->vsync != saved.vsync) SDL_SetAtomicInt(&live_vsync, c->vsync);
    if (all || c->face_buttons != saved.face_buttons) SDL_SetAtomicInt(&live_face, c->face_buttons);
    if (all || c->menu_confirm != saved.menu_confirm) SDL_SetAtomicInt(&live_menu_confirm, c->menu_confirm);
    if (all || c->background_input != saved.background_input) SDL_SetAtomicInt(&live_background, c->background_input);
    if (all || c->combo_window != saved.combo_window) SDL_SetAtomicInt(&live_combo, c->combo_window);
    if (all || strcmp(c->chat_key, saved.chat_key))
        SDL_SetAtomicInt(&live_chat_key, *c->chat_key ? (int)SDL_GetScancodeFromName(c->chat_key) : 0);
    if (all || c->fxaa_mode != saved.fxaa_mode) SDL_SetAtomicInt(&live_fxaa_mode, c->fxaa_mode);
    if (all || c->smooth_gradients != saved.smooth_gradients) SDL_SetAtomicInt(&live_smooth, c->smooth_gradients);
    if (all || strcmp(c->ldn_server, saved.ldn_server) || strcmp(c->ldn_key, saved.ldn_key)) {
        SDL_LockSpinlock(&live_ldn_lock);
        if (all || strcmp(c->ldn_server, saved.ldn_server)) memcpy(live_ldn_server, c->ldn_server, sizeof live_ldn_server);
        if (all || strcmp(c->ldn_key, saved.ldn_key)) memcpy(live_ldn_key, c->ldn_key, sizeof live_ldn_key);
        SDL_UnlockSpinlock(&live_ldn_lock);
    }
}
void config_store_loaded(const Config *c) {
    publish_live(c, !have_saved);
    saved = *c; have_saved = true;
    SDL_SetAtomicInt(&language, c->language);
    SDL_SetAtomicInt(&volume, c->volume);
    SDL_SetAtomicInt(&mute_in_background, c->mute_in_background);
    SDL_SetAtomicInt(&confirm_exit, c->confirm_exit);
}
void config_runtime_init(const Config *effective) {
    startup = saved; runtime_ready = true;
    config_graphics_report(false, effective->fsr ? GFX_WAITING : GFX_NATIVE);
    config_graphics_report(true, effective->dlss_nr ? GFX_WAITING : GFX_NATIVE);
    publish_live(effective, true);
    // Effective boot config suppresses AA while temporal processing is on.
    // Keep the saved preference so switching temporal processing off restores it.
    if (!getenv("MHGU_MSAA")) SDL_SetAtomicInt(&live_msaa, saved.msaa);
    if (!getenv("MHGU_FXAA")) SDL_SetAtomicInt(&live_fxaa, saved.fxaa);
    SDL_LockSpinlock(&graphics_lock);
    graphics = *effective;
    // render_w/h in effective are already divided by the upscale ratio. Keep
    // the player's output-size request so a later save can resolve it again.
    graphics.render_w = saved.render_w; graphics.render_h = saved.render_h;
    if (getenv("MHGU_RENDER")) {
        graphics.render_w = g_fsr_output[0] ? (int)g_fsr_output[0] : effective->render_w;
        graphics.render_h = g_fsr_output[1] ? (int)g_fsr_output[1] : effective->render_h;
    }
    ++graphics_revision;
    SDL_UnlockSpinlock(&graphics_lock);
    const char *override = getenv("MHGU_FRAMERATE");
    if (override && *override) {
        float f = strtof(override, NULL);
        if (!(f >= 30)) f = 30;
        if (f > 240) f = 240;
        set_float(&live_fps, f);
    }
}
float config_live_fps(void) { return get_float(&live_fps); }
float config_live_fov(void) { return get_float(&live_fov); }
float config_live_mouse_sensitivity(void) { return get_float(&live_mouse); }
float config_live_draw_distance(void) { return get_float(&live_draw); }
int config_live_fxaa(void) { return SDL_GetAtomicInt(&active_fsr) ? 0 : SDL_GetAtomicInt(&live_fxaa); }
int config_live_msaa(void) { return SDL_GetAtomicInt(&active_temporal) ? 1 : SDL_GetAtomicInt(&live_msaa); }
int config_live_prewarm(void) { return SDL_GetAtomicInt(&live_prewarm); }
int config_live_vsync(void) { return SDL_GetAtomicInt(&live_vsync); }
int config_live_menu_confirm(void) { return SDL_GetAtomicInt(&live_menu_confirm); }
int config_live_face_buttons(void) { return SDL_GetAtomicInt(&live_face); }
int config_live_background_input(void) { return SDL_GetAtomicInt(&live_background); }
int config_live_combo_window(void) { return SDL_GetAtomicInt(&live_combo); }
int config_live_chat_key(void) { return SDL_GetAtomicInt(&live_chat_key); }
int config_live_fxaa_mode(void) { return SDL_GetAtomicInt(&live_fxaa_mode); }
int config_live_smooth_gradients(void) { return SDL_GetAtomicInt(&live_smooth); }
void config_live_ldn(char *server, size_t server_size, char *key, size_t key_size) {
    SDL_LockSpinlock(&live_ldn_lock);
    if (server) SDL_strlcpy(server, live_ldn_server, server_size);
    if (key) SDL_strlcpy(key, live_ldn_key, key_size);
    SDL_UnlockSpinlock(&live_ldn_lock);
}
static SDL_AtomicInt game_language = {0};
int config_resolve_language(int selected) {
    return selected == MENU_LANG_GAME ? SDL_GetAtomicInt(&game_language) : selected;
}
void config_set_game_language(int value) {
    if (value >= MENU_LANG_ENGLISH && value < MENU_LANG_GAME) SDL_SetAtomicInt(&game_language, value);
}
int config_language(void) { return config_resolve_language(SDL_GetAtomicInt(&language)); }
Config config_editable(void) { return saved; }
float config_audio_gain(void) {
    if (SDL_GetAtomicInt(&mute_in_background) && !SDL_GetAtomicInt(&window_focused)) return 0.0f;
    return SDL_GetAtomicInt(&volume) / 100.0f;
}
void config_window_focus(bool focused) { SDL_SetAtomicInt(&window_focused, focused); }
bool config_confirm_exit(void) { return SDL_GetAtomicInt(&confirm_exit) != 0; }
bool config_restart_required(const Config *c) {
    if (!runtime_ready) return false;
    if (c->hp.quests < 0 || c->hp.quests > HP_QUESTS_MAX) return true;
    for (int i = 0; i < g_nsettings; i++) {
        const ConfigSetting *s = &g_settings[i];
        if (s->restart && (!config_valid(s, c) || !config_equal(s, c, &startup))) return true;
    }
    return c->hp.quests != startup.hp.quests ||
        memcmp(c->hp.quest, startup.hp.quest, sizeof c->hp.quest[0] * (size_t)c->hp.quests);
}

static bool fail(char *error, size_t size, const char *message) {
    if (size) snprintf(error, size, "%s", message);
    return false;
}
static bool factor_valid(float f) { return isfinite(f) && f >= HP_SCALE_MIN && f <= HP_SCALE_MAX; }
bool config_validate(const Config *c, char *error, size_t size) {
    for (int i = 0; i < g_nsettings; i++)
        if (!config_valid(&g_settings[i], c)) return fail(error, size, g_settings[i].error);
    if (c->hp.quests < 0 || c->hp.quests > HP_QUESTS_MAX)
        return fail(error, size, "Invalid village HP or quest override count.");
    for (int i = 0; i < c->hp.quests; i++) {
        if (!c->hp.quest[i].quest) return fail(error, size, "Quest IDs must be greater than zero.");
        for (int j = 0; j < i; j++)
            if (c->hp.quest[j].quest == c->hp.quest[i].quest) return fail(error, size, "Duplicate quest ID.");
        for (int j = 0; j < 4; j++)
            if (!factor_valid(c->hp.quest[i].scale[j])) return fail(error, size, "Quest HP factors must be 0.05-20.");
    }
    if (size) *error = 0;
    return true;
}

// What config_save writes: a setting of the table (setting) or a Quest<number> entry (setting NULL)
typedef struct { const ConfigSetting *setting; const char *section; char key[64], value[256]; bool seen, described; } Entry;
#define ENTRY_MAX (HP_QUESTS_MAX + 128)
static bool nr_entry(const Entry *e) {
    return e->setting && (!strncmp(e->key, "NR", 2) || !strcmp(e->key, "NeuralRendering"));
}
static void nr_help(FILE *out, Entry *e, const char *newline) {
    if (!nr_entry(e) || e->described) return;
    char help[2048];
    if (config_help(e->key, help, sizeof help))
        for (const char *p = help; *p;) {
            size_t length = strcspn(p, "\n");
            fprintf(out, "; %.*s%s", (int)length, p, newline);
            p += length; if (*p) p++;
        }
    e->described = true;
}
static void note_nr_help(Entry *entries, int n, const char *line) {
    while (isspace((unsigned char)*line)) line++;
    if (*line != ';' && *line != '#') return;
    line++; while (isspace((unsigned char)*line)) line++;
    for (int i = 0; i < n; i++) {
        Entry *e = &entries[i];
        if (!nr_entry(e) || e->described) continue;
        char help[2048];
        if (!config_help(e->key, help, sizeof help)) continue;
        size_t length = strcspn(help, "\n");
        if (!strncmp(line, help, length) && (!line[length] || isspace((unsigned char)line[length]))) e->described = true;
    }
}
static void normalize(const char *s, char *out, size_t size) {
    size_t n = 0;
    for (; *s && n + 1 < size; s++)
        if (!isspace((unsigned char)*s) && *s != '_' && *s != '-') out[n++] = (char)tolower((unsigned char)*s);
    out[n] = 0;
}
static void flush_section(FILE *out, Entry *entries, int n, const char *section, const char *newline) {
    for (int i = 0; i < n; i++)
        if (!entries[i].seen && !strcasecmp(entries[i].section, section)) {
            nr_help(out, &entries[i], newline);
            fprintf(out, "%s = %s%s", entries[i].key, entries[i].value, newline);
            entries[i].seen = true;
        }
}
// The entry a "key = value" line is for (NULL: none of them)
static Entry *entry_of(Entry *entries, int n, const char *key) {
    const ConfigSetting *s = config_setting(key);
    char normalized[128], other[128];
    normalize(key, normalized, sizeof normalized);
    for (int i = 0; i < n; i++) {
        if (s ? entries[i].setting == s : !entries[i].setting && (normalize(entries[i].key, other, sizeof other),
                                                                   !strcmp(normalized, other)))
            return &entries[i];
    }
    return NULL;
}
// A line that already says what c has (in its own spelling: "21:9", "1.0", "Borderless Window") stays as it is.
static bool line_says(const Entry *e, const char *eq, const Config *c) {
    if (!e->setting) return false;
    char value[256];
    snprintf(value, sizeof value, "%.*s", (int)strcspn(eq + 1, ";#\r\n"), eq + 1);
    char *v = value, *end = value + strlen(value);
    while (isspace((unsigned char)*v)) v++;
    while (end > v && isspace((unsigned char)end[-1])) *--end = 0;
    Config read = *c;
    return config_parse(e->setting, v, &read) && config_equal(e->setting, &read, c);
}
static bool graphics_section(const char *section) {
    return !strcasecmp(section, "Graphics") || !strcasecmp(section, "Display") ||
        !strcasecmp(section, "Anti-aliasing") || !strcasecmp(section, "Upscaler") || !strcasecmp(section, "Performance");
}
static void write_entry(FILE *out, Entry *e, const char *line, const char *eq, const Config *c, const char *newline) {
    nr_help(out, e, newline);
    const char *comment = strpbrk(eq + 1, ";#");
    if (!strcmp(e->key, "TemporalUpscaling") || !strcmp(e->key, "UpscaleQuality")) {
        fprintf(out, "%s = %s", e->key, e->value);
        if (comment) fprintf(out, " %s", comment); else fputs(newline, out);
    } else if (line_says(e, eq, c)) fputs(line, out);
    else if (comment) fprintf(out, "%.*s %s %s", (int)(eq - line + 1), line, e->value, comment);
    else fprintf(out, "%.*s %s%s", (int)(eq - line + 1), line, e->value, newline);
    e->seen = true;
}
bool config_save(const Config *c, char *error, size_t error_size) {
    if (!config_validate(c, error, error_size)) return false;
    static Entry entries[ENTRY_MAX];  // (big: off the stack)
    int n = 0;
    if (g_nsettings + c->hp.quests > ENTRY_MAX) return fail(error, error_size, "Too many settings to save (ENTRY_MAX).");
    for (int i = 0; i < g_nsettings; i++) {
        Entry *e = &entries[n++];
        e->setting = &g_settings[i], e->section = g_settings[i].section, e->seen = e->described = false;
        snprintf(e->key, sizeof e->key, "%.*s", (int)strcspn(g_settings[i].key, "|"), g_settings[i].key);
        config_format(&g_settings[i], c, e->value, sizeof e->value);
    }
    for (int i = 0; i < c->hp.quests; i++) {
        Entry *e = &entries[n++];
        const float *v = c->hp.quest[i].scale;
        e->setting = NULL, e->section = "Monster HP", e->seen = false;
        snprintf(e->key, sizeof e->key, "Quest%u", c->hp.quest[i].quest);
        snprintf(e->value, sizeof e->value, "%g, %g, %g, %g", (double)v[0], (double)v[1], (double)v[2], (double)v[3]);
    }
    char path[1100];
    config_path(path, sizeof path);
    char temp[1200];
    if (snprintf(temp, sizeof temp, "%s.settings-%llu.tmp", path, (unsigned long long)SDL_GetTicksNS()) >= (int)sizeof temp)
        return fail(error, error_size, "Settings path is too long.");
    FILE *in = fopen(path, "rb");
    if (!in && errno != ENOENT) return fail(error, error_size, "Cannot read the existing settings file.");
    FILE *out = fopen(temp, "wbx");
    if (!out) { if (in) fclose(in); return fail(error, error_size, "Cannot create a temporary settings file."); }
    char line[4096], section[128] = "";
    const char *newline = "\n";
    bool ok = true, ended_line = true;
    // Move all recognised graphics settings into one section. Keep their spelling,
    // inline comments and legacy aliases; unknown keys stay in their original section.
    // A second bounded INI pass avoids rebuilding unrelated sections or dropping comments.
    if (in) {
        while (fgets(line, sizeof line, in)) if (strstr(line, "\r\n")) { newline = "\r\n"; break; }
        rewind(in);
    }
    fprintf(out, "[Graphics]%s", newline);
    while (in && fgets(line, sizeof line, in)) {
        char *s = line; while (isspace((unsigned char)*s)) s++;
        if (*s == '[' && strchr(s, ']')) {
            snprintf(section, sizeof section, "%.*s", (int)(strchr(s, ']') - s - 1), s + 1);
            continue;
        }
        char *eq = *s && *s != ';' && *s != '#' ? strchr(s, '=') : NULL;
        if (eq) {
            char key[128]; snprintf(key, sizeof key, "%.*s", (int)(eq - s), s);
            Entry *e = entry_of(entries, n, key);
            if (e && !strcmp(e->section, "Graphics")) write_entry(out, e, line, eq, c, newline);
        } else if (graphics_section(section)) {
            note_nr_help(entries, n, line);
            fputs(line, out);
        }
    }
    if (in) { if (ferror(in)) ok = false; rewind(in); }
    flush_section(out, entries, n, "Graphics", newline);
    section[0] = 0;
    bool hidden_header = false;
    while (in && fgets(line, sizeof line, in)) {
        size_t len = strlen(line);
        if (!strchr(line, '\n') && !feof(in)) { ok = false; break; } // never truncate an unknown line
        if (strstr(line, "\r\n")) newline = "\r\n";
        char *s = line; while (isspace((unsigned char)*s)) s++;
        if (*s == '[' && strchr(s, ']')) {
            flush_section(out, entries, n, section, newline);
            snprintf(section, sizeof section, "%.*s", (int)(strchr(s, ']') - s - 1), s + 1);
            hidden_header = graphics_section(section);
            if (hidden_header) continue;
        }
        char *eq = (*s && *s != ';' && *s != '#' && *s != '[') ? strchr(s, '=') : NULL;
        bool handled = false;
        if (eq) {
            char key[128];
            snprintf(key, sizeof key, "%.*s", (int)(eq - s), s);
            char *k = key + strlen(key);
            while (k > key && isspace((unsigned char)k[-1])) *--k = 0;
            Entry *e = entry_of(entries, n, key);
            if (e) {
                if (strcmp(e->section, "Graphics")) {
                    if (hidden_header) { fprintf(out, "[%s]%s", section, newline); hidden_header = false; }
                    write_entry(out, e, line, eq, c, newline);
                }
                handled = true; ended_line = true;
            } else {
                // Removing an override must remove its old value, including duplicate entries.
                char normalized[128];
                normalize(key, normalized, sizeof normalized);
                if (!strncmp(normalized, "quest", 5) && isdigit((unsigned char)normalized[5])) handled = true;
            }
        }
        if (!handled) {
            if (!eq && graphics_section(section)) continue; // comments already carried with graphics
            if (hidden_header) { fprintf(out, "[%s]%s", section, newline); hidden_header = false; }
            fputs(line, out); ended_line = len && line[len - 1] == '\n';
        }
    }
    if (in) { if (ferror(in)) ok = false; fclose(in); }
    if (!ended_line) fputs(newline, out);
    flush_section(out, entries, n, section, newline);
    for (int i = 0; i < n; i++) if (!entries[i].seen) {
        fprintf(out, "%s[%s]%s", newline, entries[i].section, newline);
        flush_section(out, entries, n, entries[i].section, newline);
    }
    if (fflush(out) || ferror(out)) ok = false;
#ifdef _WIN32
    if (_commit(_fileno(out))) ok = false;
#else
    if (fsync(fileno(out))) ok = false;
#endif
    if (fclose(out)) ok = false;
    if (ok) {
#ifdef _WIN32
        ok = MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        ok = rename(temp, path) == 0;
#endif
    }
    if (!ok) { remove(temp); return fail(error, error_size, "Could not save settings; the previous file was kept."); }
    config_store_loaded(c);
    return true;
}
