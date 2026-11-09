#include "keyboard_mouse_input.h"
#include "keymap.h"
#include <assert.h>
#include <stdio.h>

Config g_config;

static void wheel_at_fps(unsigned fps, int notches, int layout) {
    KbmWheel w = {0};
    kbm_wheel_add(&w, (float)notches);
    uint64_t prev = 0, step = 1000000000ull / fps;
    int taps = 0;
    for (uint64_t now = 0; now < 2000000000ull; now += step) {
        uint64_t b = kbm_wheel_buttons(&w, now, layout);
        uint64_t selection = KBM_BUTTON(layout == KEYBOARD_MELEE ? (notches > 0 ? BTN_Y : BTN_A) : (notches > 0 ? BTN_X : BTN_B));
        if ((b & selection) && !(prev & selection)) {
            assert(prev == KBM_BUTTON(BTN_L));
            assert(b & KBM_BUTTON(BTN_L));
            taps++;
        }
        if ((prev & selection) && !(b & selection)) assert(b == KBM_BUTTON(BTN_L));
        prev = b;
    }
    assert(taps == (notches > 0 ? notches : -notches));
    assert(!w.pending && !w.phase);
}

int main(void) {
    g_config.input_mode = INPUT_KEYBOARD_MOUSE;
    assert(key_in_use(SDL_SCANCODE_F) && key_in_use(SDL_SCANCODE_C));
    assert(key_in_use(SDL_SCANCODE_1) && key_in_use(SDL_SCANCODE_F2));
    assert(key_in_use(SDL_SCANCODE_X) && key_in_use(SDL_SCANCODE_V));
    assert(key_in_use(SDL_SCANCODE_Z) && key_in_use(SDL_SCANCODE_4));
    g_config.input_mode = INPUT_ORIGINAL;
    assert(!key_in_use(SDL_SCANCODE_F2));
    assert(!key_in_use(SDL_SCANCODE_X) && !key_in_use(SDL_SCANCODE_V));
    assert(!key_in_use(SDL_SCANCODE_Z) && !key_in_use(SDL_SCANCODE_4));
    const int layouts[] = {KEYBOARD_MELEE, KEYBOARD_RANGED};
    const struct { SDL_Scancode left, alternate; int button; } aliases[] = {
        {SDL_SCANCODE_1, SDL_SCANCODE_UP, BTN_UP},
        {SDL_SCANCODE_2, SDL_SCANCODE_RIGHT, BTN_RIGHT},
        {SDL_SCANCODE_3, SDL_SCANCODE_DOWN, BTN_DOWN},
        {SDL_SCANCODE_4, SDL_SCANCODE_LEFT, BTN_LEFT},
        {SDL_SCANCODE_Z, SDL_SCANCODE_BACKSPACE, BTN_MINUS},
    };
    for (unsigned i = 0; i < sizeof layouts / sizeof *layouts; i++) {
        int layout = layouts[i];
        assert(kbm_key(SDL_SCANCODE_X, layout) == KBM_BUTTON(BTN_X));
        assert(kbm_key(SDL_SCANCODE_V, layout) == KBM_BUTTON(BTN_A));
        for (unsigned j = 0; j < sizeof aliases / sizeof *aliases; j++) {
            assert(kbm_key(aliases[j].left, layout) == KBM_BUTTON(aliases[j].button));
            assert(kbm_key(aliases[j].alternate, layout) == KBM_BUTTON(aliases[j].button));
        }
    }
    assert(kbm_key(SDL_SCANCODE_R, KEYBOARD_RANGED) == KBM_BUTTON(BTN_X));
    assert(kbm_key(SDL_SCANCODE_F, KEYBOARD_RANGED) == KBM_BUTTON(BTN_Y));
    assert(kbm_key(SDL_SCANCODE_SPACE, KEYBOARD_RANGED) == KBM_BUTTON(BTN_B));
    assert(kbm_key(SDL_SCANCODE_LSHIFT, KEYBOARD_RANGED) == KBM_BUTTON(BTN_R));
    assert(!(kbm_key(SDL_SCANCODE_LSHIFT, KEYBOARD_RANGED) & KBM_BUTTON(BTN_B)));
    assert(kbm_key(SDL_SCANCODE_E, KEYBOARD_RANGED) == KBM_BUTTON(BTN_R));
    assert(kbm_key(SDL_SCANCODE_C, KEYBOARD_RANGED) == (KBM_BUTTON(BTN_X) | KBM_BUTTON(BTN_A)));
    assert(kbm_key(SDL_SCANCODE_ESCAPE, KEYBOARD_RANGED) == KBM_BUTTON(BTN_PLUS));
    assert(kbm_key(SDL_SCANCODE_TAB, KEYBOARD_RANGED) == KBM_BUTTON(BTN_ZL));
    assert(kbm_key(SDL_SCANCODE_F2, KEYBOARD_RANGED) == 0);
    assert(kbm_mouse(SDL_BUTTON_LEFT, KEYBOARD_RANGED) == BTN_A);
    assert(kbm_mouse(SDL_BUTTON_RIGHT, KEYBOARD_RANGED) == BTN_R);
    assert(kbm_mouse(SDL_BUTTON_X1, KEYBOARD_RANGED) == BTN_Y);
    assert(kbm_mouse(SDL_BUTTON_MIDDLE, KEYBOARD_RANGED) == BTN_ZL);
    assert(kbm_mouse(SDL_BUTTON_X2, KEYBOARD_RANGED) == -1);
    for (unsigned fps = 30; fps <= 240; fps *= 2) {
        wheel_at_fps(fps, 3, KEYBOARD_RANGED);
        wheel_at_fps(fps, 3, KEYBOARD_MELEE);
        wheel_at_fps(fps, -3, KEYBOARD_RANGED);
        wheel_at_fps(fps, -3, KEYBOARD_MELEE);
    }
    assert(kbm_key(SDL_SCANCODE_R, KEYBOARD_MELEE) == KBM_BUTTON(BTN_R));
    assert(kbm_key(SDL_SCANCODE_F, KEYBOARD_MELEE) == KBM_BUTTON(BTN_Y));
    assert(kbm_key(SDL_SCANCODE_E, KEYBOARD_MELEE) == KBM_BUTTON(BTN_Y));
    assert(kbm_key(SDL_SCANCODE_LSHIFT, KEYBOARD_MELEE) == KBM_BUTTON(BTN_R));
    assert(kbm_mouse(SDL_BUTTON_LEFT, KEYBOARD_MELEE) == BTN_X);
    assert(kbm_mouse(SDL_BUTTON_RIGHT, KEYBOARD_MELEE) == BTN_A);
    assert(kbm_mouse(SDL_BUTTON_X2, KEYBOARD_MELEE) == BTN_R);
    KbmWheel w = {0};
    kbm_wheel_add(&w, 0.5f); kbm_wheel_add(&w, 0.5f);
    assert(w.pending == 1 && w.fraction == 0);
    assert(kbm_wheel_buttons(&w, 0, KEYBOARD_RANGED) == KBM_BUTTON(BTN_L));
    kbm_wheel_reset(&w);  // focus loss / opening an overlay must cancel queued selections and held modifiers
    assert(!kbm_wheel_buttons(&w, 1000000000ull, KEYBOARD_RANGED));
    puts("Melee/Ranged bindings and item/ammo wheel: passed (30/60/120/240 fps)");
}
