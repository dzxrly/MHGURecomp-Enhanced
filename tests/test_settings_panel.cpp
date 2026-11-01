// Controller navigation exercised against the real ImGui frame loop.
#include "settings_panel.h"
#include "config.h"
#include "app_actions.h"
#include "overlay.h"
#include "settings_input.h"
#include "modorder.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>
static int requested;
static SettingsInput hint_input = {true, {0, 0}};
extern "C" bool overlay_uses_gamepad(void) { return hint_input.gamepad; }
extern "C" const char *g_game_dir = "obj/settings-test/game";  // (modorder.c compares archives with the game's)
extern "C" void ui_request_action(bool restart) { requested = restart ? 2 : 1; }
extern "C" bool ui_action_failed(void) { return false; }
extern "C" int ldn_hub_status(const char *, const char *, int *, int *) { return 0; }
extern "C" void ldn_hub_check(const char *, const char *) {}
// (cheats.c: two cheats, a master code and one that can be turned off)
static bool cheat_on[2] = {true, true};
extern "C" int cheats_count(void) { return 2; }
extern "C" const char *cheats_name(int i) { return i ? "Test cheat" : "Master"; }
extern "C" const char *cheats_file(int i) { (void)i; return "test.txt"; }
extern "C" bool cheats_master(int i) { return i == 0; }
extern "C" bool cheats_on(int i) { return cheat_on[i]; }
extern "C" bool cheats_set(const bool *on, int n, char *, size_t) {
    for (int i = 0; i < n && i < 2; i++)
        cheat_on[i] = on[i];
    return true;
}
extern "C" int overlay_face_position(int semantic) {
    const int p[] = {1, 0, 3, 2};
    return p[semantic];
}
static bool opened = true;
static void frame() {
    settings_panel_before_frame(opened);
    ImGui::NewFrame();
    settings_panel(&opened);
    ImGui::Render();
}
static void idle(int n = 3) {
    while (n--)
        frame();
}
static void press(ImGuiKey key) {
    ImGui::GetIO().AddKeyEvent(key, true);
    idle();
    ImGui::GetIO().AddKeyEvent(key, false);
    idle();
}
static ImGuiID id(const char *name) {  // (a dropdown's choices are in a window of their own)
    auto *w = ImGui::FindWindowByName(strncmp(name, "choice.", 7) ? "Settings##main" : "Settings##dropdown");
    assert(w);
    return ImHashStr("###row", 0, ImHashStr(name, 0, w->ID));
}
static void selected(const char *name) {
    auto &g = *ImGui::GetCurrentContext();
    if (g.NavId != id(name))
        fprintf(stderr, "focus %s: %x != %x\n", name, g.NavId, id(name));
    assert(g.NavId == id(name));
    assert(g.NavCursorVisible);
}
static float pinned_content_bottom() {
    auto *w = ImGui::FindWindowByName("Settings##main");
    float bottom = 0;
    for (const auto &command : w->DrawList->CmdBuffer) {
        auto clip = command.ClipRect;
        if (command.ElemCount && clip.x == w->InnerClipRect.Min.x && clip.z == w->InnerClipRect.Max.x &&
            clip.w < w->InnerClipRect.Max.y - 1 && clip.w > bottom)
            bottom = clip.w;
    }
    assert(bottom > w->InnerClipRect.Min.y);
    return bottom;
}
static float pinned_content_top() {
    auto *w = ImGui::FindWindowByName("Settings##main");
    float top = 0;
    for (const auto &command : w->DrawList->CmdBuffer) {
        auto clip = command.ClipRect;
        if (command.ElemCount && clip.x == w->InnerClipRect.Min.x && clip.z == w->InnerClipRect.Max.x &&
            clip.w < w->InnerClipRect.Max.y - 1 && clip.y > top)
            top = clip.y;
    }
    assert(top > w->InnerClipRect.Min.y);
    return top;
}
static void down(int n = 1) {
    while (n--)
        press(ImGuiKey_GamepadDpadDown);
}
static void graphics_row(const char *name) {
    press(ImGuiKey_Home);
    for (int i=0; i<60 && ImGui::GetCurrentContext()->NavId!=id(name); i++) down();
    selected(name);
}
static void tab() { press(ImGuiKey_GamepadR1); }
static void back() { press(ImGuiKey_GamepadFaceRight); }
static void confirm() { press(ImGuiKey_GamepadFaceDown); }
static void check_hint_input() {
    SDL_Event event = {};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    settings_input_event(&hint_input, &event); assert(hint_input.gamepad);
    event.type = SDL_EVENT_MOUSE_MOTION; event.motion.xrel = 1;
    settings_input_event(&hint_input, &event); assert(hint_input.gamepad); // Ignore pointer noise.
    event.motion.xrel = 4;
    settings_input_event(&hint_input, &event); assert(!hint_input.gamepad);
    event = {}; event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX; event.gaxis.value = 100;
    settings_input_event(&hint_input, &event); assert(!hint_input.gamepad); // Ignore stick drift.
    event.gaxis.value = 16000;
    settings_input_event(&hint_input, &event); assert(hint_input.gamepad);
    event = {}; event.type = SDL_EVENT_KEY_DOWN; event.key.scancode = SDL_SCANCODE_F1;
    settings_input_event(&hint_input, &event); assert(!hint_input.gamepad);
    event = {}; event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX; event.gaxis.value = 17000;
    settings_input_event(&hint_input, &event); assert(!hint_input.gamepad); // Held stick cannot reclaim hints.
    event.gaxis.value = -16000;
    settings_input_event(&hint_input, &event); assert(hint_input.gamepad);
    event = {}; event.type = SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.button = SDL_BUTTON_LEFT;
    settings_input_event(&hint_input, &event); assert(!hint_input.gamepad);
    event = {}; event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    settings_input_event(&hint_input, &event); assert(hint_input.gamepad);
    event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.y = 1;
    settings_input_event(&hint_input, &event); assert(!hint_input.gamepad);
    hint_input = {true, {0, 0}};
}
int main() {
    check_hint_input();
#ifdef _WIN32
    _putenv_s("MHGU_CONFIG", "obj/settings-test/panel-settings.ini");
#else
    setenv("MHGU_CONFIG", "obj/settings-test/panel-settings.ini", 1);
#endif
    // Mods tab fixtures: Aaa (two files) and Bbb (one) both replace a.bin, Ccc stands alone, Ddd is off ('_')
    namespace fs = std::filesystem;
    fs::remove_all("obj/settings-test/mods");
    auto put = [](const char *path, const char *text) {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream(path, std::ios::binary) << text;
    };
    put("obj/settings-test/mods/Aaa/romfs/a.bin", "1");
    put("obj/settings-test/mods/Aaa/romfs/b.bin", "bb");
    put("obj/settings-test/mods/Bbb/romfs/a.bin", "22");
    put("obj/settings-test/mods/Ccc/romfs/c.bin", "3");
    put("obj/settings-test/mods/_Ddd/romfs/d.bin", "4");
#ifdef _WIN32
    _putenv_s("MHGU_MODS_DIR", "obj/settings-test/mods");
#else
    setenv("MHGU_MODS_DIR", "obj/settings-test/mods", 1);
#endif
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.f / 60;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    Config baseline = config_defaults();
    config_store_loaded(&baseline);
    config_runtime_init(&baseline);

    idle();
    selected("display.mode");
    // A click outside the menu unfocuses every window; the menu takes its focus back (it used to assert in SetNavID)
    io.AddMousePosEvent(4, 4);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    idle();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    idle();
    selected("display.mode");
    confirm();
    selected("choice.2");
    back();
    selected("display.mode");
    down(2);
    selected("display.width");
    confirm();
    selected("display.width");
    assert(opened && !settings_panel_confirming() &&
           !settings_panel_dirty()); // Numeric activation never opens an editor.
    press(ImGuiKey_GamepadDpadRight);
    assert(settings_panel_dirty());
    back();
    selected("dialog.cancel");
    down(2);
    confirm();
    assert(!opened);
    frame();
    opened = true;
    idle();
    selected("display.mode");
    io.AddKeyEvent(ImGuiKey_GamepadR1, true);
    idle(35);
    selected("volume"); // The separate Audio page contains no graphics settings.
    io.AddKeyEvent(ImGuiKey_GamepadR1, false);
    idle();
    press(ImGuiKey_GamepadL1);
    graphics_row("fps");
    // The frame rate: Confirm lists the common ones, left / right go a frame at a time
    confirm();
    selected("choice.1");  // (60, the one it has)
    down();
    confirm();
    selected("fps");
    press(ImGuiKey_GamepadDpadRight);
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    selected("save");
    confirm();
    assert(config_editable().fps == 91);
    graphics_row("fps");
    confirm();
    selected("choice.2");  // (91 is none of them: the one below)
    press(ImGuiKey_GamepadDpadUp);
    confirm();
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    confirm();
    assert(config_editable().fps == 60);
    graphics_row("fps");
    confirm();
    selected("choice.1");
    back();  // (Back closes the dropdown, nothing changed)
    selected("fps");
    assert(!settings_panel_dirty() && opened);
    // Common ratios use the existing hot INI field; exact ultrawide ratios survive Save/reopen.
    const float aspects[] = {0, 16.f/10, 16.f/9, 21.f/9, 64.f/27, 43.f/18, 24.f/10, 32.f/10, 32.f/9};
    for (int preset=1; preset<9; preset++) {
        graphics_row("aspect.auto"); confirm(); press(ImGuiKey_Home); down(preset); confirm();
        down(); selected("fov"); // A preset needs no custom-number row.
        press(ImGuiKey_End); press(ImGuiKey_GamepadDpadUp); selected("save"); confirm();
        float value = config_editable().aspect_ratio;
        assert(value > aspects[preset]-.0001f && value < aspects[preset]+.0001f);
        Config chosen = config_editable(); assert(!config_restart_required(&chosen));
    }
    graphics_row("aspect.auto"); confirm(); press(ImGuiKey_End); confirm();
    assert(!settings_panel_dirty()); // Entering Custom preserves the current ratio.
    down(); selected("aspect"); press(ImGuiKey_GamepadDpadRight);
    press(ImGuiKey_End); press(ImGuiKey_GamepadDpadUp); confirm();
    float custom_aspect = config_editable().aspect_ratio;
    assert(custom_aspect > aspects[8]+.049f && custom_aspect < aspects[8]+.051f);
    opened = false; frame(); opened = true; idle();
    graphics_row("aspect.auto"); confirm(); selected("choice.9"); back();
    down(); selected("aspect");
    assert(config_editable().aspect_ratio == custom_aspect && !settings_panel_dirty());
    graphics_row("aspect.auto"); confirm(); press(ImGuiKey_Home); confirm();
    press(ImGuiKey_End); press(ImGuiKey_GamepadDpadUp); confirm();
    assert(config_editable().aspect_ratio == 0);
    graphics_row("fps");
    tab(); // Audio
    tab();
    selected("face");
    down();
    selected("menu.confirm");
    press(ImGuiKey_GamepadDpadRight);
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    selected("save");
    confirm();
    assert(config_live_menu_confirm() == MENU_CONFIRM_SOUTH);
    assert(config_editable().face_buttons == FACE_POSITION);
    assert(!config_restart_required(&baseline));
    press(ImGuiKey_Home);
    down();
    selected("menu.confirm");
    press(ImGuiKey_GamepadDpadLeft);
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    confirm();
    assert(config_live_menu_confirm() == MENU_CONFIRM_ORIGINAL);
    tab();
    selected("port");
    press(ImGuiKey_GamepadL1);
    selected("face");
    tab();
    tab();
    selected("hp.0");
    tab();
    selected("mod.Aaa");
    // The Mods tab: the conflict scan runs on another thread; Aaa and Bbb both replace a.bin
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    idle();
    down(4);  // (Aaa, Bbb, Ccc, then _Ddd that is off)
    selected("mods.conflicts");  // one row for them all
    down();
    selected("mods.rescan");  // (the suggested order is the one it has: Aaa changes more files, so Bbb comes after)
    press(ImGuiKey_Home);
    down();
    selected("mod.Bbb");
    confirm();
    down();  // Move up
    confirm();
    selected("mod.Bbb");
    assert(settings_panel_dirty());
    press(ImGuiKey_Home);
    selected("mod.Bbb");
    down(5);  // (Bbb, Aaa, Ccc, _Ddd, the conflict)
    selected("mods.suggest");  // Bbb now wins over the bigger Aaa: the order is suggested back
    confirm();
    press(ImGuiKey_Home);
    selected("mod.Aaa");
    down(2);
    selected("mod.Ccc");
    press(ImGuiKey_GamepadDpadLeft);  // off
    assert(settings_panel_dirty());
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    selected("save");
    confirm();
    assert(!settings_panel_dirty());
    {
        std::ifstream in("obj/settings-test/mods/load_order.txt");
        std::string list((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(list.find("\nAaa\nBbb\n") != std::string::npos);
        assert(list.find("\nCcc\n") == std::string::npos && list.find("# off: Ccc\n") != std::string::npos);
        assert(list.find("\n_Ddd\n") == std::string::npos);
    }
    // Choosing which of two conflicting mods is used moves it below the other
    press(ImGuiKey_Home);
    down(4);  // (Aaa, Bbb, then the off ones _Ddd and Ccc)
    selected("mods.conflicts");
    confirm();
    selected("mods.conflict.0");
    confirm();
    selected("choice.1");  // (Use Bbb, the one used now)
    press(ImGuiKey_GamepadDpadUp);
    selected("choice.0");  // Use Aaa
    confirm();
    selected("mods.conflict.0");
    down();
    selected("conflicts.back");
    confirm();
    selected("mods.conflicts");
    assert(settings_panel_dirty());
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    selected("save");
    confirm();
    {
        std::ifstream in("obj/settings-test/mods/load_order.txt");
        std::string list((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(list.find("\nBbb\nAaa\n") != std::string::npos);
    }
    // A mod that is off keeps its place in the list: turned on again, it loads where it was
    {
        ModEntry order[] = {{"Bbb", true}, {"Ccc", false}, {"Aaa", true}, {"_Ddd", false}};
        char error[256];
        assert(mods_save_order(order, 4, error, sizeof error));
        ModEntry list[MODS_MAX];
        assert(mods_installed(list, MODS_MAX) == 4);
        for (int i = 0; i < 4; i++)
            assert(!strcmp(list[i].name, order[i].name) && list[i].on == order[i].on);
    }
    // A mod folder added after the Mods tab saved loads (on), after the ones it lists
    put("obj/settings-test/mods/Eee/romfs/e.bin", "5");
    {
        ModEntry list[MODS_MAX];
        int n = mods_installed(list, MODS_MAX);
        assert(n == 5);
        const char *want[] = {"Bbb", "Ccc", "Aaa", "_Ddd", "Eee"};
        const bool on[] = {true, false, true, false, true};
        for (int i = 0; i < n; i++)
            assert(!strcmp(list[i].name, want[i]) && list[i].on == on[i]);
    }
    fs::remove_all("obj/settings-test/mods/Eee");
    tab();
    selected("cheat.1");
    confirm();
    assert(settings_panel_dirty() && cheat_on[1]); // Draft only, until Save.
    press(ImGuiKey_GamepadFaceUp);
    assert(!settings_panel_dirty() && !cheat_on[1] && cheat_on[0]);
    press(ImGuiKey_Home); selected("cheat.1");
    confirm();
    assert(settings_panel_dirty());
    settings_panel_request_back(); idle();
    selected("dialog.cancel"); down(2); confirm(); // Discard restores the saved off state.
    assert(!opened && !cheat_on[1]); frame(); opened = true; idle();
    selected("cheat.1");
    tab();
    selected("language");
    down(); selected("theme");
    assert(settings_panel_theme() == THEME_HUNTER);
    press(ImGuiKey_GamepadDpadLeft);
    assert(settings_panel_dirty() && settings_panel_theme() == THEME_BLUE);
    assert(config_editable().menu_theme == THEME_HUNTER);
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    press(ImGuiKey_S);
    io.AddKeyEvent(ImGuiMod_Ctrl, false); idle();
    assert(!settings_panel_dirty() && config_editable().menu_theme == THEME_BLUE);
    auto drag = [&](ImVec2 from, ImVec2 to) {
        io.AddMousePosEvent(from.x, from.y); idle();
        io.AddMouseButtonEvent(0, true); idle();
        io.AddMousePosEvent(to.x, to.y); idle();
        io.AddMouseButtonEvent(0, false); idle();
    };
    auto *movable = ImGui::FindWindowByName("Settings##main");
    ImVec2 attached = movable->Pos;
    ImVec2 grab(attached.x + movable->Size.x * .5f, attached.y + 3);
    drag(grab, ImVec2(grab.x - 20, grab.y + 20));
    assert(movable->Pos.y > attached.y + 15);
    ImVec2 original_size = movable->Size;
    ImVec2 grip(movable->Pos.x + movable->Size.x - 2, movable->Pos.y + movable->Size.y - 2);
    drag(grip, ImVec2(grip.x - 40, grip.y - 40));
    assert(movable->Size.x < original_size.x - 30 && movable->Size.y < original_size.y - 30);
    graphics_row("detach"); confirm(); idle(); // Attach resets position and custom size.
    assert(movable->Pos.y == attached.y && movable->Size.x == original_size.x);
    tab();
    selected("display.mode");
    tab();
    selected("volume");
    confirm();
    selected("volume");
    press(ImGuiKey_GamepadDpadLeft);
    assert(settings_panel_dirty());
    settings_panel_request_back();
    idle();
    selected("dialog.cancel");
    back();
    selected("volume");
    settings_panel_request_close();
    idle();
    selected("dialog.cancel");
    assert(!requested);
    back();
    selected("volume");
    // The unused face button still does nothing, including while held.
    io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, true);
    idle(50);
    assert(!ImGui::GetCurrentContext()->NavWindowingTarget);
    io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, false);
    idle();
    assert(settings_panel_dirty());
    press(ImGuiKey_GamepadFaceUp);
    assert(!settings_panel_dirty());
    assert(config_editable().volume == 99);
    press(ImGuiKey_Home);
    selected("volume");
    press(ImGuiKey_GamepadDpadRight);
    back();
    selected("dialog.cancel");
    down(2);
    confirm();
    assert(!opened);
    assert(config_editable().volume == 99);
    frame();
    opened = true;
    idle();
    selected("volume");
    tab();
    tab();
    selected("port");
    confirm();
    selected("port");
    press(ImGuiKey_GamepadDpadRight);
    assert(settings_panel_dirty());
#ifdef _WIN32
    _putenv_s("MHGU_CONFIG", "missing-settings-parent/panel.ini");
#else
    setenv("MHGU_CONFIG", "missing-settings-parent/panel.ini", 1);
#endif
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    selected("save");
    confirm();
    assert(settings_panel_dirty());
    assert(config_editable().ldn_port == 39500);
#ifdef _WIN32
    _putenv_s("MHGU_CONFIG", "obj/settings-test/panel-settings.ini");
#else
    setenv("MHGU_CONFIG", "obj/settings-test/panel-settings.ini", 1);
#endif
    back();
    selected("dialog.cancel");
    down(2);
    confirm();
    assert(!opened);
    frame();
    opened = true;
    idle();
    selected("port");
    down();
    selected("hosts");
    confirm();
    selected("key.0");
    io.AddInputCharactersUTF8("localhost:39500");
    idle();
    settings_panel_request_close();
    idle();
    selected("dialog.cancel");
    assert(settings_panel_dirty());
    back();
    selected("key.0");
    back();
    selected("hosts");
    assert(!settings_panel_dirty());
    settings_panel_request_close();
    idle();
    selected("dialog.cancel");
    down();
    confirm();
    assert(requested == 1);
    back();
    selected("hosts");
    requested = 0;
    tab();
    tab();
    tab();
    tab(); // Cheats is between Mods and Menu Options.
    selected("language");
    down(7); // Theme adds one row to Menu Options.
    selected("restart");
    confirm();
    selected("dialog.cancel");
    back();
    selected("restart");
    confirm();
    down();
    confirm();
    assert(requested == 2);
    back();
    requested = 0;
    opened = false;
    frame();
    baseline = config_editable();
    io.DisplaySize = ImVec2(640, 480);
    baseline.menu_scale = 200;
    baseline.language = 1;
    config_store_loaded(&baseline);
    opened = true;
    idle();
    auto *w = ImGui::FindWindowByName("Settings##main");
    assert(w->Size.x <= io.DisplaySize.x);
    assert(w->ContentSize.x <= w->InnerRect.GetWidth() + 1);
    float top = w->Pos.y;
    press(ImGuiKey_GamepadL1);
    press(ImGuiKey_GamepadL1);
    press(ImGuiKey_GamepadL1);
    selected("hp.0");
    down(5);
    confirm();
    idle();
    assert(w->Pos.y == top);
    press(ImGuiKey_End);
    selected("reset");
    idle(12);
    assert(w->Scroll.y > 0);
    press(ImGuiKey_GamepadDpadUp);
    selected("hp.add");
    confirm();
    assert(settings_panel_dirty());
    down(5);
    confirm();
    selected("hp.add");
    assert(!settings_panel_dirty());
    press(ImGuiKey_Home);
    press(ImGuiKey_GamepadDpadRight);
    press(ImGuiKey_End);
    selected("reset");
    confirm();
    selected("dialog.cancel");
    down();
    confirm();
    selected("reset");
    assert(!settings_panel_dirty());
    io.DisplaySize = ImVec2(640, 480);
    idle(12);
    assert(w->Size.x <= 640);
    assert(w->Size.y <= 480);
    assert(w->ContentSize.x <= w->InnerRect.GetWidth() + 1);
    press(ImGuiKey_GamepadL1);
    press(ImGuiKey_GamepadL1);
    press(ImGuiKey_GamepadL1);
    press(ImGuiKey_GamepadL1);
    // The unified selector maps the existing INI keys and only shows applicable controls.
    graphics_row("aa.method");
    confirm();
    press(ImGuiKey_Home);
    confirm(); // Off, independent of the defaults.
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    selected("save");
    confirm();
    for (int method = 1; method <= 5; method++) {
        graphics_row("aa.method");
        press(ImGuiKey_GamepadDpadRight);
        idle();
        down();
        selected(method == 1 ? "fxaamode" : method <= 3 ? "msaa" : "fsr.quality");
        press(ImGuiKey_End);
        press(ImGuiKey_GamepadDpadUp);
        selected("save");
        confirm();
        Config chosen = config_editable();
        assert(chosen.fsr == (method >= 4));
        if (method < 4) {
            assert(chosen.fxaa == (method != 2));
            assert((chosen.msaa > 1) == (method >= 2));
            if (method == 3) { graphics_row("msaa"); down(); selected("fxaamode"); }
        } else {
            assert(chosen.fsr_upscaler == method - 4);
            graphics_row("fsr.quality"); down(); selected("fsr.sharpness");
        }
        assert(!config_restart_required(&chosen));
    }
    graphics_row("fsr.quality");
    press(ImGuiKey_GamepadDpadRight);
    assert(settings_panel_dirty());
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    selected("save");
    confirm();
    assert(config_editable().fsr == 1 && config_editable().fsr_upscaler == FSR_UP_4);
    assert(config_editable().fsr_quality == FSR_QUALITY);
    Config fsr_saved = config_editable();
    assert(!config_restart_required(&fsr_saved));
    graphics_row("aa.method");
    press(ImGuiKey_GamepadDpadRight); // One selected provider: FSR 4 -> DLSS.
    graphics_row("fsr.quality");
    down(); selected("fsr.surfaces"); // No FSR-only sharpness / reactive rows under DLSS.
    graphics_row("fsr.quality");
    confirm();
    selected("choice.3"); // Quality, in Ultra Performance -> DLAA order.
    press(ImGuiKey_Home);
    for(int mode=0;mode<5;mode++){
        char choice[32];snprintf(choice,sizeof choice,"choice.%d",mode);selected(choice);
        confirm();
        press(ImGuiKey_End);
        press(ImGuiKey_GamepadDpadUp);
        confirm();
        assert(config_editable().fsr_quality==FSR_ULTRA_PERFORMANCE-mode);
        graphics_row("fsr.quality");
        if(mode<4){confirm();down();}
    }
    graphics_row("nr");
    confirm();
    graphics_row("aa.method");
    confirm();
    selected("choice.4"); // NR excludes MSAA and MSAA + FXAA: Off, FXAA, FSR 3, FSR 4, DLSS.
    back();
    graphics_row("nr");
    down();
    selected("nr.intensity"); // Runtime selection is INI-only, with no F1 row.
    graphics_row("nr.style");
    press(ImGuiKey_GamepadDpadRight);
    down();selected("nr.structure");press(ImGuiKey_GamepadDpadRight);
    down();selected("nr.tone");press(ImGuiKey_GamepadDpadRight);
    down();selected("nr.skin");press(ImGuiKey_GamepadDpadRight);
    down();selected("nr.mask");confirm();
    down();selected("nr.layers");
    press(ImGuiKey_GamepadDpadRight);press(ImGuiKey_GamepadDpadRight);press(ImGuiKey_GamepadDpadRight);
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    confirm();
    assert(config_editable().fsr_upscaler==FSR_UP_DLSS && config_editable().dlss_nr);
    assert(config_editable().nr_runtime==NR_RUNTIME_OFFICIAL);
    assert(config_editable().nr_style==1 && config_editable().nr_layers==3 && !config_editable().nr_auto_mask);
    assert(config_editable().nr_structure>1.049f && config_editable().nr_structure<1.051f);
    assert(config_editable().nr_tone>.049f && config_editable().nr_skin>.049f);
    opened = false;
    frame();
    g_config.fsr = 1;
    opened = true;
    idle();
    graphics_row("fps");
    down(3);
    selected("aa.method");
    assert(!settings_panel_dirty()); // The selector reads saved temporal settings without changing them.
    idle(20);
    float pinned_bottom = pinned_content_bottom();
    float pinned_top = pinned_content_top();
    press(ImGuiKey_End);
    idle(40);
    selected("reset");
    auto *root = ImGui::FindWindowByName("Settings##main");
    assert(root->Scroll.y > 0);
    assert(pinned_content_bottom() == pinned_bottom); // Guide position does not follow the scroll.
    assert(pinned_content_top() == pinned_top);
    assert(root->NavRectRel[ImGuiNavLayer_Main].Max.y + root->Pos.y <= pinned_bottom + 1);
    press(ImGuiKey_Home);
    idle(40);
    selected("display.mode");
    assert(pinned_content_bottom() == pinned_bottom);
    assert(pinned_content_top() == pinned_top);
    assert(root->NavRectRel[ImGuiNavLayer_Main].Min.y + root->Pos.y >= pinned_top - 1);
    press(ImGuiKey_End);
    idle(20);
    float font = root->FontRefSize;
    io.AddMousePosEvent(root->Pos.x + ImGui::GetStyle().WindowPadding.x + font * 2.6f,
                        root->Pos.y + ImGui::GetStyle().WindowPadding.y + font * .8f);
    idle();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    idle(8);
    selected("display.mode"); // Pinned tabs remain clickable when the option list is scrolled.
    graphics_row("fps");
    float controller_body_top = pinned_content_top();
    hint_input.gamepad = false;
    idle(20); // Wider keycaps must fit the existing narrow, scaled Spanish layout too.
    selected("fps");
    root = ImGui::FindWindowByName("Settings##main");
    assert(root->Pos.x >= 0 && root->Pos.x + root->Size.x <= io.DisplaySize.x + 1);
    assert(pinned_content_bottom() > pinned_content_top());
    assert(pinned_content_top() == controller_body_top); // Input hints cannot reflow the category tabs.
    press(ImGuiKey_Enter);
    assert(ImGui::FindWindowByName("Settings##dropdown")->Active);
    press(ImGuiKey_Escape);
    selected("fps");
    press(ImGuiKey_Tab);
    selected("vsync"); // Tab still moves between options, not categories.
    press(ImGuiKey_UpArrow);
    selected("fps");
    press(ImGuiKey_Escape);
    assert(!opened);
    opened = false;
    frame();
    assert(!settings_panel_dirty());
    Config offline = config_editable();
    offline.nr_runtime = NR_RUNTIME_SM89;
    offline.dlss_nr = 1;
    config_store_loaded(&offline);
    opened = true;
    idle();
    graphics_row("nr");
    down();
    selected("nr.intensity");
    press(ImGuiKey_End);
    selected("reset");
    confirm();
    selected("dialog.cancel");
    down();
    confirm();
    press(ImGuiKey_End);
    press(ImGuiKey_GamepadDpadUp);
    selected("save");
    confirm();
    assert(config_editable().nr_runtime == NR_RUNTIME_SM89); // F1 Reset/Save preserves the offline selection.
    opened = false;
    frame();
    ImGui::DestroyContext();
    remove("obj/settings-test/panel-settings.ini");
    puts("settings panel: inline numbers, explicit Save/Reset, Back guide, dirty discard, focus and layout "
         "OK");
}
