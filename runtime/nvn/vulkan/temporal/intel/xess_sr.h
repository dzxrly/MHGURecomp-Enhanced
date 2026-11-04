#pragma once
#include "vk.h"

void vk_xess_instance_exts(const VkExtensionProperties *available, uint32_t count, const char **ext,
                           uint32_t *n_ext, uint32_t capacity, uint32_t api);
bool vk_xess_device_exts(const VkExtensionProperties *available, uint32_t count, const char **ext,
                         uint32_t *n_ext, uint32_t capacity);
bool vk_xess_device_features(void **features);
void vk_xess_disable(void);
bool vk_xess_init(uint32_t width, uint32_t height, int quality, uint32_t *render_w, uint32_t *render_h);
void vk_xess_reset(void);
bool vk_xess_ready(void);
bool vk_xess_dispatch(VkCommandBuffer cb, TexBackend *color, TexBackend *depth, TexBackend *motion,
                      TexBackend *output, const float jitter[2], bool reset);
