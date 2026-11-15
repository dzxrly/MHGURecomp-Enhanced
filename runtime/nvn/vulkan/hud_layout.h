#ifndef MHGU_HUD_LAYOUT_H
#define MHGU_HUD_LAYOUT_H

#include <math.h>
#include <stdbool.h>

// Layer 103 shares the upper-left status HUD with the lower-right item/ammo selector.
// HP/stamina occupy the top strip (y 20/33); their end caps move beyond x 800 as
// their maxima increase. The selector's expanded rows are below this strip.
static inline bool hud_status_is_top_left(float x, float y) {
    return y < 100.0f || (x < 800.0f && y < 500.0f);
}

// Multiple tiles on one backdrop row count as one observation.
static inline bool hud_note_loading_row(unsigned *rows, float y, bool *loading) {
    unsigned row = fabsf(y - 168.0f) < 0.3f ? 1u : fabsf(y - 433.0f) < 0.3f ? 2u
                 : fabsf(y - 570.0f) < 0.3f ? 4u : 0u;
    *rows |= row;
    *loading = (*rows & (*rows - 1u)) != 0;
    return row != 0;
}

#endif
