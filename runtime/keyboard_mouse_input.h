// Fixed keyboard/mouse preset. These are ordinary controller inputs, not timed combat actions.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_mouse.h>
#include "config.h"
#include "hle/input.h"

#define KBM_BUTTON(b) (1ull << (b))

static inline uint64_t kbm_key(SDL_Scancode sc, int layout) {
    switch (sc) {
    case SDL_SCANCODE_SPACE: return KBM_BUTTON(BTN_B);
    case SDL_SCANCODE_X: return KBM_BUTTON(BTN_X);
    case SDL_SCANCODE_V: return KBM_BUTTON(BTN_A);
    case SDL_SCANCODE_R: return KBM_BUTTON(layout == KEYBOARD_MELEE ? BTN_R : BTN_X);
    case SDL_SCANCODE_F: return KBM_BUTTON(BTN_Y);
    case SDL_SCANCODE_C: return KBM_BUTTON(BTN_X) | KBM_BUTTON(BTN_A);
    case SDL_SCANCODE_Q: return KBM_BUTTON(BTN_L);
    case SDL_SCANCODE_E: return KBM_BUTTON(layout == KEYBOARD_MELEE ? BTN_Y : BTN_R);
    // Shift is the game's ordinary R/run input, never B/Power Run.
    case SDL_SCANCODE_LSHIFT: return KBM_BUTTON(BTN_R);
    case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return KBM_BUTTON(BTN_PLUS);
    case SDL_SCANCODE_TAB: return KBM_BUTTON(BTN_ZL);
    case SDL_SCANCODE_Z: case SDL_SCANCODE_BACKSPACE: return KBM_BUTTON(BTN_MINUS);
    case SDL_SCANCODE_T: return KBM_BUTTON(BTN_ZR);
    case SDL_SCANCODE_1: case SDL_SCANCODE_UP: return KBM_BUTTON(BTN_UP);
    case SDL_SCANCODE_2: case SDL_SCANCODE_RIGHT: return KBM_BUTTON(BTN_RIGHT);
    case SDL_SCANCODE_3: case SDL_SCANCODE_DOWN: return KBM_BUTTON(BTN_DOWN);
    case SDL_SCANCODE_4: case SDL_SCANCODE_LEFT: return KBM_BUTTON(BTN_LEFT);
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return KBM_BUTTON(BTN_LSTICK);
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return KBM_BUTTON(BTN_RSTICK);
    default: return 0;
    }
}

static inline int kbm_mouse(unsigned button, int layout) {
    switch (button) {
    case SDL_BUTTON_LEFT: return layout == KEYBOARD_MELEE ? BTN_X : BTN_A;
    case SDL_BUTTON_RIGHT: return layout == KEYBOARD_MELEE ? BTN_A : BTN_R;
    case SDL_BUTTON_MIDDLE: return BTN_ZL;
    case SDL_BUTTON_X1: return BTN_Y;
    case SDL_BUTTON_X2: return layout == KEYBOARD_MELEE ? BTN_R : -1;
    default: return -1;
    }
}

typedef struct {
    int pending, direction, phase;
    float fraction;
    uint64_t until;
} KbmWheel;

static inline void kbm_wheel_reset(KbmWheel *w) { *w = (KbmWheel){0}; }

static inline void kbm_wheel_add(KbmWheel *w, float amount) {
    w->fraction += amount;
    int steps = (int)w->fraction;
    w->fraction -= steps;
    w->pending += steps;
}

// L down -> X/B tap -> X/B up -> L up. A >=40 ms phase remains visible at 30 fps as well as high frame rates.
// Queue wheel notches instead of repeating a held X/B, which the game would treat as one selection.
static inline uint64_t kbm_wheel_buttons(KbmWheel *w, uint64_t now, int layout) {
    if (w->phase && now >= w->until) {
        w->phase = (w->phase + 1) % 5;
        w->until = now + 40000000ull;
    }
    if (!w->phase && w->pending) {
        w->direction = w->pending > 0 ? 1 : -1;
        w->pending -= w->direction;
        w->phase = 1;
        w->until = now + 40000000ull;
    }
    if (w->phase == 1 || w->phase == 3) return KBM_BUTTON(BTN_L);
    if (w->phase == 2) {
        int button = layout == KEYBOARD_MELEE ? (w->direction > 0 ? BTN_Y : BTN_A) : (w->direction > 0 ? BTN_X : BTN_B);
        return KBM_BUTTON(BTN_L) | KBM_BUTTON(button);
    }
    return 0;
}
