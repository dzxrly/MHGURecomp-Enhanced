#include <math.h>
#include <string.h>
#include "graphics.h"
#include "fsr.h"
#include "ngx.h"

Config g_graphics;
uint32_t g_graphics_render[2], g_graphics_output[2];
static uint64_t revision;

void vk_graphics_init(void) {
    Config request;
    config_graphics_snapshot(&request, &revision);
    g_graphics = g_config;
    memcpy(g_graphics_render, g_render_size, sizeof g_graphics_render);
    for (int i = 0; i < 2; i++) g_graphics_output[i] = g_fsr_output[i] ? g_fsr_output[i] : g_render_size[i];
}

void vk_graphics_target_size(const NvnTexture *t, uint32_t *w, uint32_t *h) {
    // Only screen-shaped render targets. Shadow maps, atlases, cubes, arrays,
    // movies and ordinary CPU textures retain their own dimensions.
    if (!t->addr || t->target != 1 || t->levels != 1 || t->width > g_render_size[0] || t->height > g_render_size[1]) return;
    for (uint32_t divisor = 1; divisor <= 16; divisor *= 2) {
        if (t->width != g_render_size[0] / divisor || t->height != g_render_size[1] / divisor) continue;
        *w = g_graphics_render[0] / divisor; *h = g_graphics_render[1] / divisor;
        if (!*w) *w = 1;
        if (!*h) *h = 1;
        return;
    }
}

void vk_graphics_apply(void) {
    vk_prewarm_apply();
    Config next;
    if (!config_graphics_snapshot(&next, &revision)) return;
    uint32_t render[2], output[2]; float aspect, anchor;
    config_graphics_resolve(&next, render, output, &aspect, &anchor);
    if (next.fsr && next.fsr_upscaler == FSR_UP_DLSS && g_graphics.fsr && g_graphics.fsr_upscaler == FSR_UP_DLSS &&
        next.fsr_quality == g_graphics.fsr_quality && !memcmp(output, g_graphics_output, sizeof output)) {
        memcpy(render, g_graphics_render, sizeof render);
        next.render_w = render[0]; next.render_h = render[1];
    }
    // NRRuntime is deliberately INI-only and immutable until the next launch.
    next.nr_runtime = g_graphics.nr_runtime;
    bool rebuild = memcmp(render, g_graphics_render, sizeof render) || memcmp(output, g_graphics_output, sizeof output) ||
        next.fsr != g_graphics.fsr || next.dlss_nr != g_graphics.dlss_nr || next.fsr_upscaler != g_graphics.fsr_upscaler ||
        next.fsr_quality != g_graphics.fsr_quality || next.fsr_objmotion != g_graphics.fsr_objmotion ||
        next.fsr_animsurf != g_graphics.fsr_animsurf;
    bool reset = next.nr_style != g_graphics.nr_style || next.fov != g_graphics.fov || aspect != g_render_aspect;
    if (rebuild || next.msaa != g_graphics.msaa || next.fxaa != g_graphics.fxaa || next.fxaa_mode != g_graphics.fxaa_mode) {
        vk_end_pass();
        vk_models_reset();
    }
    if (rebuild) {
        vk_end_pass(); vk_submit_extra(VK_NULL_HANDLE, VK_NULL_HANDLE);
        VK_CHECK(vkDeviceWaitIdle(g_vk.dev));
        vk_fsr_reset();
        vk_ngx_reset();
    }
    g_graphics = next;
    memcpy(g_graphics_render, render, sizeof render);
    memcpy(g_graphics_output, output, sizeof output);
    g_render_aspect = aspect; g_ui_anchor_aspect = anchor;
    g_fsr_lod_bias = next.fsr ? log2f((float)render[0] / output[0]) : 0;
    if (rebuild) vk_fsr_init();
    else if (reset) vk_fsr_reset_history();
    config_graphics_dimensions(g_graphics_render, g_graphics_output, true);
    LOG("[graphics] applied save %llu: %ux%u -> %ux%u, upscaler %s, NR %s, layers %d%s\n",
        (unsigned long long)revision, g_graphics_render[0], g_graphics_render[1], output[0], output[1],
        !next.fsr ? "off" : next.fsr_upscaler == FSR_UP_DLSS ? "DLSS" : next.fsr_upscaler == FSR_UP_4 ? "FSR4" : "FSR3",
        next.dlss_nr ? "on" : "off", next.nr_layers, rebuild ? " (resources rebuilt)" : "");
}
