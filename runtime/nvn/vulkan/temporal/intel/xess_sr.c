#include "xess_sr.h"
#include "config.h"

#ifdef _WIN32
#include <string.h>
#include "temporal/runtime_dll.h"
#include "xess_vk.h"

static HMODULE library;
static bool instance_ok, device_ok, initialized;
static xess_context_handle_t context;
static __typeof__(&xessVKGetRequiredInstanceExtensions) instance_extensions;
static __typeof__(&xessVKGetRequiredDeviceExtensions) device_extensions;
static __typeof__(&xessVKGetRequiredDeviceFeatures) device_features;
static __typeof__(&xessVKCreateContext) create_context;
static __typeof__(&xessGetOptimalInputResolution) optimal_resolution;
static __typeof__(&xessVKInit) init_context;
static __typeof__(&xessVKExecute) execute;
static __typeof__(&xessDestroyContext) destroy_context;

static bool load_xess(void) {
    wchar_t path[32768];
    if (!graphics_runtime_path(path, sizeof path / sizeof *path, L"graphics\\intel\\xess\\libxess.dll") ||
        GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return false;
    library = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!library) {
        LOG("[xess] cannot load libxess.dll (Windows error %lu)\n", GetLastError());
        return false;
    }
#define LOAD(slot, symbol) do { \
    slot = (void *)GetProcAddress(library, #symbol); \
    if (!slot) { \
        LOG("[xess] runtime lacks %s\n", #symbol); \
        FreeLibrary(library); library = NULL; return false; \
    } \
} while (0)
    LOAD(instance_extensions, xessVKGetRequiredInstanceExtensions);
    LOAD(device_extensions, xessVKGetRequiredDeviceExtensions);
    LOAD(device_features, xessVKGetRequiredDeviceFeatures);
    LOAD(create_context, xessVKCreateContext);
    LOAD(optimal_resolution, xessGetOptimalInputResolution);
    LOAD(init_context, xessVKInit);
    LOAD(execute, xessVKExecute);
    LOAD(destroy_context, xessDestroyContext);
#undef LOAD
    return true;
}

static bool append_extensions(const char *const *required, uint32_t required_count,
                              const VkExtensionProperties *available, uint32_t count,
                              const char **ext, uint32_t *n_ext, uint32_t capacity) {
    uint32_t next = *n_ext;
    for (uint32_t i = 0; i < required_count; i++) {
        uint32_t j = 0;
        while (j < next && strcmp(required[i], ext[j])) j++;
        if (j < next) continue;
        j = 0;
        while (j < count && strcmp(required[i], available[j].extensionName)) j++;
        if (j == count || next == capacity) {
            LOG("[xess] cannot enable required Vulkan extension %s\n", required[i]);
            return false;
        }
        ext[next++] = required[i];
    }
    *n_ext = next;
    return true;
}

void vk_xess_instance_exts(const VkExtensionProperties *available, uint32_t count, const char **ext,
                           uint32_t *n_ext, uint32_t capacity, uint32_t api) {
    if (!load_xess()) return;
    const char *const *required = NULL;
    uint32_t n = 0, min_api = 0;
    instance_ok = instance_extensions(&n, &required, &min_api) == XESS_RESULT_SUCCESS && api >= min_api &&
                  append_extensions(required, n, available, count, ext, n_ext, capacity);
}

bool vk_xess_device_exts(const VkExtensionProperties *available, uint32_t count, const char **ext,
                         uint32_t *n_ext, uint32_t capacity) {
    if (!instance_ok) return false;
    const char *const *required = NULL;
    uint32_t n = 0;
    return device_extensions(g_vk.inst, g_vk.phys, &n, &required) == XESS_RESULT_SUCCESS &&
           append_extensions(required, n, available, count, ext, n_ext, capacity);
}

bool vk_xess_device_features(void **features) {
    device_ok = device_features(g_vk.inst, g_vk.phys, features) == XESS_RESULT_SUCCESS;
    return device_ok;
}

void vk_xess_disable(void) { instance_ok = device_ok = false; }

bool vk_xess_init(uint32_t width, uint32_t height, int quality, uint32_t *render_w, uint32_t *render_h) {
    if (!device_ok) return false;
    static const xess_quality_settings_t qualities[] = {
        XESS_QUALITY_SETTING_AA, XESS_QUALITY_SETTING_QUALITY, XESS_QUALITY_SETTING_BALANCED,
        XESS_QUALITY_SETTING_PERFORMANCE, XESS_QUALITY_SETTING_ULTRA_PERFORMANCE,
    };
    xess_vk_init_params_t params = {.outputResolution = {width, height}, .qualitySetting = qualities[quality],
                                  .initFlags = XESS_INIT_FLAG_LDR_INPUT_COLOR};
    xess_2d_t input = {0}, minimum, maximum;
    xess_result_t rc = create_context(g_vk.inst, g_vk.phys, g_vk.dev, &context);
    if (rc == XESS_RESULT_SUCCESS) rc = init_context(context, &params);
    if (rc >= XESS_RESULT_SUCCESS)
        rc = optimal_resolution(context, &params.outputResolution, params.qualitySetting, &input, &minimum, &maximum);
    if (rc < XESS_RESULT_SUCCESS || !input.x || !input.y) {
        LOG("[xess] context or input size initialization failed (%d)\n", (int)rc);
        vk_xess_reset();
        return false;
    }
    *render_w = input.x;
    *render_h = input.y;
    initialized = true;
    LOG("[xess] Vulkan XeSS ready: %ux%u -> %ux%u\n", input.x, input.y, width, height);
    return true;
}

void vk_xess_reset(void) {
    if (context) destroy_context(context);
    context = NULL;
    initialized = false;
}

bool vk_xess_ready(void) { return initialized; }

static xess_vk_image_view_info image_info(TexBackend *t) {
    static const VkComponentMapping identity = {0};
    return (xess_vk_image_view_info){
        .imageView = vk_image_view(t, VK_IMAGE_VIEW_TYPE_2D, t->format, 0, 1, 0, 1, identity, t->aspect),
        .image = t->image, .format = t->format, .width = t->width, .height = t->height,
        .subresourceRange = {t->aspect, 0, 1, 0, 1},
    };
}

bool vk_xess_dispatch(VkCommandBuffer cb, TexBackend *color, TexBackend *depth, TexBackend *motion,
                      TexBackend *output, const float jitter[2], bool reset) {
    if (!initialized) return false;
    xess_vk_execute_params_t params = {
        .colorTexture = image_info(color), .velocityTexture = image_info(motion),
        .depthTexture = image_info(depth), .outputTexture = image_info(output),
        .jitterOffsetX = jitter[0], .jitterOffsetY = jitter[1], .exposureScale = 1.0f,
        .resetHistory = reset, .inputWidth = color->width, .inputHeight = color->height,
    };
    if (!params.colorTexture.imageView || !params.velocityTexture.imageView || !params.depthTexture.imageView ||
        !params.outputTexture.imageView) return false;
    VkImageMemoryBarrier reads[3];
    const xess_vk_image_view_info *inputs[] = {&params.colorTexture, &params.velocityTexture, &params.depthTexture};
    for (uint32_t i = 0; i < 3; i++)
        reads[i] = (VkImageMemoryBarrier){.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL, .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = inputs[i]->image, .subresourceRange = inputs[i]->subresourceRange};
    vkCmdPipelineBarrier(cb, VK_STAGES_ALL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 3, reads);
    xess_result_t rc = execute(context, cb, &params);
    for (uint32_t i = 0; i < 3; i++) {
        reads[i].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        reads[i].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        reads[i].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        reads[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_STAGES_ALL, 0, 0, NULL, 0, NULL, 3, reads);
    if (rc < XESS_RESULT_SUCCESS) {
        LOG("[xess] dispatch failed (%d)\n", (int)rc);
        initialized = false;
    }
    return initialized;
}
#else
void vk_xess_instance_exts(const VkExtensionProperties *a, uint32_t n, const char **ext, uint32_t *ne,
                           uint32_t cap, uint32_t api) {}
bool vk_xess_device_exts(const VkExtensionProperties *a, uint32_t n, const char **ext, uint32_t *ne, uint32_t cap) { return false; }
bool vk_xess_device_features(void **features) { return false; }
void vk_xess_disable(void) {}
bool vk_xess_init(uint32_t w, uint32_t h, int q, uint32_t *rw, uint32_t *rh) { return false; }
void vk_xess_reset(void) {}
bool vk_xess_ready(void) { return false; }
bool vk_xess_dispatch(VkCommandBuffer cb, TexBackend *c, TexBackend *d, TexBackend *m,
                      TexBackend *o, const float j[2], bool reset) { return false; }
#endif
