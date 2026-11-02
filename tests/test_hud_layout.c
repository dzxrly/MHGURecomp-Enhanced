#include "../runtime/nvn/vulkan/hud_layout.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    // The body and every cap position must keep the same anchor as the bar grows.
    // 100 points ends at x 742, 110 at 806, and 150 at 1062 in the 1280x720 canvas.
    const float bar_x[] = {102, 742, 799, 800, 806, 1062};
    for (unsigned i = 0; i < sizeof bar_x / sizeof *bar_x; ++i) {
        assert(hud_status_is_top_left(bar_x[i], 20));
        assert(hud_status_is_top_left(bar_x[i], 33));
    }
    // Clock, weapon gauges and party status remain on the upper left.
    assert(hud_status_is_top_left(64, 50));
    assert(hud_status_is_top_left(108, 84));
    assert(hud_status_is_top_left(100, 175));
    // Expanded ammo rows and their R prompt must stay with the bottom-right selector.
    assert(!hud_status_is_top_left(1024, 440));
    assert(!hud_status_is_top_left(1118, 548));
    assert(!hud_status_is_top_left(1024, 555));
    assert(!hud_status_is_top_left(1153, 617));
    assert(!hud_status_is_top_left(1085, 649));
    puts("HUD layout: long HP/stamina caps, status HUD and expanded item/ammo selector OK");
}
