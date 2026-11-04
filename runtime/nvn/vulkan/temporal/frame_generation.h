#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    VkImage image;
    VkImageView view;
    VkFormat format;
    VkImageUsageFlags usage;
    uint32_t width, height;
} FgImage;

typedef struct {
    FgImage color, depth, motion;
    float jitter[2], camera_position[3], camera_right[3], camera_up[3], camera_forward[3];
    float projection[16], inverse_projection[16], clip_to_previous[16], previous_to_clip[16];
    float near_plane, far_plane, fov, frame_ms;
    bool reset;
} FgFrame;

typedef struct {
    PFN_vkGetSwapchainImagesKHR images;
    PFN_vkAcquireNextImageKHR acquire;
    PFN_vkQueuePresentKHR present;
} FgSwapchainApi;

bool vk_sl_load(bool frame_generation, const wchar_t *official_nr_path);
bool vk_sl_active(void);
PFN_vkCreateInstance vk_dlss_fg_instance_proxy(void);
PFN_vkEnumeratePhysicalDevices vk_dlss_fg_enumerate_proxy(VkInstance instance);
bool vk_sl_window_surface(VkInstance instance, void *hwnd, VkSurfaceKHR *surface);
PFN_vkCreateDevice vk_dlss_fg_device_proxy(VkInstance instance, VkPhysicalDevice physical);
bool vk_sl_attach(VkDevice device, FgSwapchainApi *api, int *max_frames);
PFN_vkCreateSwapchainKHR vk_dlss_fg_swapchain_create(void);
PFN_vkDestroySwapchainKHR vk_dlss_fg_swapchain_destroy(void);
void vk_dlss_fg_begin(uint32_t frame);
bool vk_dlss_fg_tag(VkCommandBuffer cb, const FgFrame *frame, int generated_frames, uint32_t backbuffers);
void vk_dlss_fg_submit_marker(bool start);
void vk_dlss_fg_present_marker(bool start);
int64_t vk_dlss_fg_presented(void); // Since the last query. -1 means unavailable.
void vk_dlss_fg_pause(void);
bool vk_sl_sr_available(void);
bool vk_sl_sr_size(uint32_t width, uint32_t height, int quality, uint32_t *render_w, uint32_t *render_h);
bool vk_sl_sr_dispatch(VkCommandBuffer cb, const FgFrame *frame, const FgImage *input, const FgImage *output);
void vk_sl_sr_reset(void);

bool vk_fsr_fg_load(void);
void vk_fsr_fg_device(VkDevice device, VkPhysicalDevice physical, VkQueue game, uint32_t game_family,
                       VkQueue present, uint32_t present_family, VkQueue acquire, uint32_t acquire_family);
VkResult vk_fsr_fg_create(const VkSwapchainCreateInfoKHR *info, VkSwapchainKHR *swapchain, FgSwapchainApi *api);
void vk_fsr_fg_wait(void);
void vk_fsr_fg_destroy(VkSwapchainKHR swapchain);
bool vk_fsr_fg_prepare(VkCommandBuffer cb, const FgFrame *frame, uint64_t frame_id, bool enable);
bool vk_fsr_fg_present_count(VkSwapchainKHR swapchain, uint64_t *count);

// Accessed on the renderer's locked graphics/present queue.
bool vk_fg_available(void);
void vk_fg_begin_scene(void);
void vk_fg_save_scene(const FgFrame *frame);
void vk_fg_clear_scene(void);
void vk_fg_wait(void);

#ifdef __cplusplus
}
#endif
