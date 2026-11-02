#ifndef MHGU_HUD_LAYOUT_H
#define MHGU_HUD_LAYOUT_H

#include <stdbool.h>

// Layer 103 shares the upper-left status HUD with the lower-right item/ammo selector.
// HP/stamina occupy the top strip (y 20/33); their end caps move beyond x 800 as
// their maxima increase. The selector's expanded rows are below this strip.
static inline bool hud_status_is_top_left(float x, float y) {
    return y < 100.0f || (x < 800.0f && y < 500.0f);
}

#endif
