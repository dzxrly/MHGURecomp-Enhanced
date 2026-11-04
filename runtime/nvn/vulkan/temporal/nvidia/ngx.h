// Optional DLSS SR and DLSS 5 NR on the renderer's existing Vulkan device.
#pragma once
#include "vk.h"

typedef struct {
    VkImage image;
    VkImageView view;
    VkFormat format;
    uint32_t width, height;
    bool storage;
} NgxImage;

typedef struct {
    VkCommandBuffer cb;
    NgxImage color, depth, motion, output;
    float jitter[2], motion_scale[2];
    float frame_ms;
    bool reset;
} NgxFrame;

// Called before vkCreateDevice. Enables only advertised NGX extensions.
bool vk_ngx_device_exts(const VkExtensionProperties *available, uint32_t count, const char **ext, uint32_t *n_ext,
                        uint32_t capacity);
bool vk_ngx_init(void);
void vk_ngx_reset(void); // release features only; device/selected runtime stay loaded, GPU must be idle
// Query before the game creates render targets; quality uses the FSR_* menu values.
bool vk_ngx_render_size(uint32_t out_w, uint32_t out_h, int quality, uint32_t *w, uint32_t *h);
bool vk_ngx_sr(const NgxFrame *frame);
bool vk_ngx_nr(const NgxFrame *frame);
bool vk_ngx_sr_ready(void);
bool vk_ngx_nr_ready(void);
