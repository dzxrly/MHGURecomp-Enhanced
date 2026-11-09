// Player settings: mhgurecomp.ini in the game folder (written with the defaults when it is missing).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DISPLAY_WINDOWED, DISPLAY_FULLSCREEN, DISPLAY_BORDERLESS };
enum { FACE_POSITION, FACE_LABEL };
enum { COMBO_LENIENT, COMBO_STRICT };  // [Controls] ComboInputWindow (runtime/hle/combowin.c)
enum { INPUT_ORIGINAL, INPUT_KEYBOARD_MOUSE };  // [Controls] InputMode (restart)
enum { KEYBOARD_MELEE, KEYBOARD_RANGED };        // automatically selected from the equipped weapon
enum { MENU_CONFIRM_ORIGINAL, MENU_CONFIRM_SOUTH, MENU_CONFIRM_EAST };
// Preserve the original English/Spanish values in existing settings files.
enum {
    MENU_LANG_ENGLISH = 0, MENU_LANG_SPANISH = 1, MENU_LANG_FRENCH = 2,
    MENU_LANG_GERMAN = 3, MENU_LANG_ITALIAN = 4, MENU_LANG_CHINESE_SIMPLIFIED = 5,
    MENU_LANG_CHINESE_TRADITIONAL = 6, MENU_LANG_GAME = 7
};
enum { FXAA_ORIGINAL, FXAA_311 };  // [Anti-aliasing] FXAAMode (runtime/nvn/vulkan/exec.c)

// [Monster HP]: factors on the HP multiplier of Hub-type quests' large monsters
#define HP_QUESTS_MAX 256
#define HP_SCALE_MIN 0.05f
#define HP_SCALE_MAX 20.0f
typedef struct {
    float scale[4];  // by the number of hunters (index: hunters - 1)
    float village;   // Village quests posted in the Hub / Pub: this first, then scale ([Village Quests] HubHP)
    int quests;
    struct { uint32_t quest; float scale[4]; } quest[HP_QUESTS_MAX];  // per quest, in place of scale
} HpScale;

// [MouseConversion]: Keyboard_Mouse_Input only; immutable for this process (restart after editing).
typedef struct {
    float min_window_ms;       // minimum displacement averaging window, 1..50 ms
    float full_stick_speed;    // counts/s reaching full stick at MouseSensitivity=1
    float deadzone_compensation; // output stick magnitude added to cross the game's dead zone, 0..0.95
    float low_speed_cutoff;    // normalized speed below which output is zero, 0..0.1
    float frame_time_smoothing; // weight of each new measured frame interval, 0.01..1
    float aim_deadzone_compensation; // Ranged RMB/E: per-axis floor; 0 uses ordinary radial conversion
} MouseConversion;
#define MOUSE_CONVERSION_DEFAULTS {8.0f, 1500.0f, 0.40f, 0.0f, 0.25f, 0.50f}

typedef struct {
    int enabled, corner, transparency;
} PerformanceOverlay;
enum { PERF_TOP_LEFT, PERF_TOP_RIGHT, PERF_BOTTOM_LEFT, PERF_BOTTOM_RIGHT };

typedef struct {
    int display_mode;         // DISPLAY_*
    int width, height;        // screen / window size (Fullscreen: the monitor mode)
    int render_w, render_h;   // the game's 3D resolution (16:9)
    int fps;                  // frame rate cap: 30..240
    int vsync;                // 1: present without tearing (mailbox / FIFO, at most the refresh rate), 0: immediate
    int msaa;                 // samples of the 3D scene: 1 (off), 2, 4, 8
    int fxaa;                 // the game's own FXAA pass: 1 on, 0 off
    int face_buttons;         // FACE_*: controller face buttons by position (the Switch layout) or by their labels
    int ldn_port;             // local play: the port a hosted hub listens on (TCP; UDP for LAN search)
    char ldn_join[256];       // local play: hosts to search besides the LAN (host[:port], comma separated)
    int sync_small_monsters;  // multiplayer: small monsters synced like large ones (runtime/net/emsync.c)
    int bonus_content;        // free downloads copied from a donor save into saves without any (runtime/services/bonus.c)
    int village_in_hub;       // L / R at the Hub's / Pub's quest counter switch to Village quests (runtime/hle/villagehub.c)
    HpScale hp;               // large monster HP in Hub-type quests (runtime/hle/hpscale.c)
    float mouse_sensitivity;  // mouse look: the speed for full right stick tilt is divided by it (runtime/host_input.c)
    float aspect_ratio;       // the picture's shape (width / height), 0: the window's; never below 16:10 (config.c)
    float fov;                // 3D camera field of view factor (1: the game's)
    int ui_anchor;            // where the HUD goes on a picture wider than 16:9: UI_*
    int background_input;     // the controller keeps working while the window is in the background (runtime/ui.c)
    int volume;               // host master volume, 0..100; independent of the game's music / SFX settings
    int button_icons;         // 0: Xbox, 1: Switch, 2: PlayStation (visual guides only)
    int language;             // MENU_LANG_*: host UI language, or follow the game
    int menu_scale;           // host UI text/layout scale, 50..200 percent
    int menu_confirm;         // independent confirmation policy for the host settings menu
    int confirm_exit;         // ask before closing the application
    int fsr;                  // [Graphics] TemporalUpscaling (legacy alias FSR): one FSR / DLSS backend
    int fsr_quality;          // FSR_*: the render scale
    float fsr_sharpness;      // FSR's RCAS sharpening, 0..1
    int fsr_objmotion;        // FSR: motion vectors of moving objects (characters, monsters): 1 on, 0 camera only
    float fsr_reactive;       // FSR: reactive mask strength on the game's moving-object mask, 0 (off) .. 1
    int fsr_animsurf;         // FSR: surfaces animated through their texture (banners, water): 1 detected, 0 off
    int fsr_upscaler;         // [Graphics] Upscaler: FSR_UP_* (FSR 3.1, FSR 4, DLSS SR, XeSS SR)
    int updates;              // [Application] Updates: UPDATES_*, read by the updater (updater/updater.c), not the game
    int shader_prewarm;       // [Graphics] ShaderPrewarm: translate the game's shaders at start-up (nvn/vulkan/exec.c)
    float draw_distance;      // [Graphics] DrawDistance: town NPCs' fade distance factor, 0 no limit (config.c npc_fade_dist2)
    int mute_in_background;   // [Audio] MuteInBackground: no sound while the window is not focused (config_audio_gain)
    int combo_window;         // [Controls] ComboInputWindow: COMBO_*, how long a press waits for a second button
    int input_mode;           // INPUT_*: original controls/prompts or Keyboard_Mouse_Input
    MouseConversion mouse_conversion;
    char ldn_server[256];     // [Multiplayer] Server: hub server for internet hubs (host[:port]); "" the default, "Off" none
    char ldn_key[64];         // [Multiplayer] Key: matchmaking key, hubs seen only by players with the same; "" public
    char chat_key[32];        // [Controls] ChatKey: the key that opens a line of chat (runtime/hle/chat.c), by SDL's
                              // name for it ("/", "Y"...); "" none
    int fxaa_mode;            // [Graphics] FXAAMode: FXAA_*, which FXAA draws while FXAA is on
    int smooth_gradients;     // [Graphics] SmoothGradients: the game's RGBA8 render targets as RGBA16 (nvn/vulkan/textures.c)
    int dlss_nr;              // [Graphics] NeuralRendering: independent, HUD-free DLSS 5 NR
    float nr_intensity;       // [Graphics] NRIntensity: 0..2
    int nr_runtime;           // INI-only NR_RUNTIME_*: Auto or an explicit runtime
    int nr_preset;            // NR_PRESET_*: requested model, applied at startup
    int nr_style;             // 0 Standard, 1 Natural, 2 Cinematic
    float nr_structure;       // local structure strength, 0..2
    float nr_tone;            // local tone strength, 0..2
    float nr_skin;            // skin structure strength, 0..2; zero by default
    int nr_auto_mask;         // the runtime's automatic skin mask, on by default
    int nr_layers;            // 1..5 residual gain levels after one model evaluation
    int ask_input;            // [Controls] AskInputOnStartup: show the input picker before loading the game
    int menu_theme;           // [Application] Theme: THEME_* (runtime/settings_panel.cpp)
    int frame_generation;     // FG_*: selects the presentation backend at launch
    int dlss_frames;          // generated frames per real frame (1..5), at launch
    PerformanceOverlay performance_overlay;
    int menu_chord, share_urgent, loading_indicator;
    char save_location[256];    // optional folder containing save_0, applied at startup
} Config;
// The hub server used when ini Server is empty (runtime/net/ldn.c, server/): the project's public one (an Oracle
// Cloud VM; a DuckDNS name, so it can move without a release). "" = no default.
#define LDN_DEFAULT_SERVER "morerokk-mhgurecomp-ldnserver.duckdns.org"
enum { UPDATES_ASK, UPDATES_AUTO, UPDATES_OFF };
enum { THEME_GOLD, THEME_BLUE, THEME_HUNTER };  // [Application] Theme
enum { UI_CENTER = 1, UI_21_9, UI_32_9, UI_EDGES };  // the middle 16:9, 21:9, 32:9, the picture's sides
enum { FSR_UP_3, FSR_UP_4, FSR_UP_DLSS, FSR_UP_XESS };  // [Graphics] Upscaler
enum { FG_OFF, FG_FSR, FG_DLSS };
enum { NR_RUNTIME_AUTO, NR_RUNTIME_OFFICIAL, NR_RUNTIME_SM89 };
enum { NR_PRESET_A = 1, NR_PRESET_B = 2 };
enum { FSR_NATIVE, FSR_QUALITY, FSR_BALANCED, FSR_PERFORMANCE, FSR_ULTRA_PERFORMANCE };  // output / render: 1, 1.5, 1.7, 2, 3

extern Config g_config;
static inline bool config_temporal(void) { return g_config.fsr || g_config.dlss_nr || g_config.frame_generation != FG_OFF; }
enum { FG_STATUS_OFF, FG_STATUS_WAITING, FG_STATUS_ACTIVE, FG_STATUS_UNAVAILABLE };
// Status and rates describe the same sample. A display rate of -1 means the vendor count was unavailable.
void config_fg_report(int status, int max_multiplier, int render_fps, int display_fps);
void config_fg_status(int *status, int *max_multiplier, int *render_fps, int *display_fps);
PerformanceOverlay config_performance_overlay(void);
extern uint32_t g_render_size[2];  // read by the game code (tools/gen.py PATCHES: its docked render size)
extern float g_render_aspect;      // the picture's shape, 16:10 or wider: the 3D cameras' aspect (tools/gen.py PATCHES)
extern float g_fov_scale;          // boot diagnostic value; camera_fovy reads config_live_fov()
extern uint32_t g_ui_size[2];      // the game's 2D canvas size (tools/gen.py PATCHES: 0xdbcd48, 0xdbcd54)
extern float g_ui_anchor_aspect;   // the HUD's width / height, 16:9 .. g_render_aspect (nvn/vulkan/exec.c)
extern uint32_t g_fsr_output[2];   // FSR on: the size it upscales to (what g_render_size would be without it), else 0
extern float g_fsr_lod_bias;       // FSR on: texture LOD bias for the lower render size, log2(render / output); else 0
float camera_fovy(float fovy, float aspect);  // a 3D camera's vertical field of view for its aspect (gen.py PATCHES)

void config_load(void);  // before the window and the guest start
void config_path(char *buf, size_t size);  // the settings file (MHGU_CONFIG, else mhgurecomp.ini)

// The settings of the file, one row each (config.c g_settings): what reads, writes and checks them, and what the
// settings menu (settings_panel.cpp) marks as needing a restart. Quest<number> entries ([Monster HP]) are apart: hp.quest.
enum { SET_BOOL, SET_INT, SET_FLOAT, SET_FACTOR, SET_CHOICE, SET_SIZE, SET_ASPECT, SET_TEXT };
typedef struct {
    const char *section, *key;  // [section] and key; "FPS|FrameRate": other spellings of the key it reads too
    int kind;                   // SET_*
    size_t field, field2;       // offsetof(Config, ...): the value (SET_SIZE: width; field2 its height)
    float min, max;             // SET_INT, SET_FLOAT, SET_FACTOR: the range
    // SET_CHOICE: the values as the file writes them, ',' between them, each with other spellings it reads after '|'
    // ('*' at the end: anything starting so); SET_SIZE: the spelling of 0x0 ("Desktop|Auto")
    const char *choices;
    const int *values;          // SET_CHOICE: each choice's value (NULL: 0, 1, 2...)
    bool restart;               // takes effect at the next start, not at once
    const char *error;          // config_validate's message for a bad value (English: settings_locale.h translates)
} ConfigSetting;
extern const ConfigSetting g_settings[];
extern const int g_nsettings;
const ConfigSetting *config_setting(const char *key);  // by key or another spelling of it; NULL: none
bool config_parse(const ConfigSetting *s, const char *value, Config *c);  // a value as the file has it into *c
void config_format(const ConfigSetting *s, const Config *c, char *buf, size_t size);  // as the file writes it
bool config_valid(const ConfigSetting *s, const Config *c);
bool config_equal(const ConfigSetting *s, const Config *a, const Config *b);
bool config_restart_key(const char *key);  // the setting takes effect at the next start
bool config_range(const char *key, float *min, float *max);  // a number setting's range
bool config_help(const char *key, char *buf, size_t size);  // what the default ini's comments say of it (the menu's help)

// File values, before environment overrides and automatic resolution are resolved.
void config_store_loaded(const Config *config);
Config config_editable(void);
Config config_defaults(void);
bool config_save(const Config *config, char *error, size_t error_size);
bool config_validate(const Config *config, char *error, size_t error_size);
// Immutable boot settings stay in g_config. These getters are atomic live values.
void config_runtime_init(const Config *effective); // After environment/Auto resolution at startup.
enum { GFX_NATIVE, GFX_WAITING, GFX_FSR3, GFX_FSR4, GFX_DLSS, GFX_UNAVAILABLE, GFX_DRIVER_OLD, GFX_XESS,
       GFX_DLSS_SL, GFX_NR_OFFICIAL_UNAVAILABLE, GFX_NR_PRESET_A, GFX_NR_PRESET_B, GFX_NR_FALLBACK_A };
enum { GFX_FALLBACK_NONE, GFX_FALLBACK_FSR4_GPU, GFX_FALLBACK_FSR4_START };
void config_graphics_fallback(int reason);
const char *config_graphics_reason(void);
void config_graphics_report(bool neural, int status);
const char *config_graphics_status(bool neural);
const char *config_neural_rendering_hint(void);
void config_neural_gpu_support(bool supported);
bool config_neural_gpu_supported(void);
void config_dlss_support(bool sr, bool fg);
bool config_dlss_sr_supported(void);
bool config_dlss_fg_supported(void);
bool config_upscaling_active(void);
float config_live_fps(void);
float config_live_fov(void);
float config_live_mouse_sensitivity(void);
float config_live_draw_distance(void);
int config_live_fxaa(void);
int config_live_msaa(void);
int config_live_vsync(void);
int config_live_face_buttons(void);
int config_live_menu_confirm(void);
int config_live_background_input(void);
int config_live_combo_window(void);
int config_live_chat_key(void);  // [Controls] ChatKey as an SDL scancode (0: none)
int config_live_fxaa_mode(void);
int config_live_smooth_gradients(void);
void config_live_ldn(char *server, size_t server_size, char *key, size_t key_size);  // [Multiplayer] Server, Key
int config_live_ldn_port(void);
void config_live_ldn_join(char *out, size_t size);
bool config_live_sync_small_monsters(void);
bool config_live_village_in_hub(void);
void config_live_hp(HpScale *out);
float config_live_hp_village(void);
bool config_live_bonus_content(void);
bool config_live_share_urgent(void);
bool config_menu_chord(void);
bool config_loading_indicator(void);
bool config_restart_required(const Config *draft); // Raw cold fields vs this process's startup INI values.
float config_audio_gain(void);  // atomic, safe on audio threads; 0 while muted in the background
void config_window_focus(bool focused);  // the window got / lost the focus (runtime/host_input.c)
int config_language(void);     // last saved UI language, resolved and atomic across host threads
int config_resolve_language(int selected); // resolves MENU_LANG_GAME to the current game language
void config_set_game_language(int language); // game thread: host language index 0..6
bool config_confirm_exit(void); // atomic, safe on the host main thread

// Saved graphics are published together. Consumers keep their own revision;
// window operations stay on the host thread, GPU operations under vk_lock.
bool config_graphics_snapshot(Config *out, uint64_t *revision);
void config_graphics_resolve(Config *c, uint32_t render[2], uint32_t output[2], float *aspect, float *anchor);
int config_live_prewarm(void);
void config_graphics_dimensions(uint32_t render[2], uint32_t output[2], bool write);

#ifdef __cplusplus
}
#endif
