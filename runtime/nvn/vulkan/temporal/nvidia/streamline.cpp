#include "temporal/frame_generation.h"

#ifdef _WIN32
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "temporal/runtime_dll.h"
#include <vulkan/vulkan_win32.h>
#include "sl.h"
#include "sl_dlss.h"
#include "sl_dlss_g.h"
#include "sl_pcl.h"
#include "sl_reflex.h"
#include "sl_security.h"

namespace {
HMODULE library;
PFN_vkGetInstanceProcAddr instance_proc;
PFN_vkGetDeviceProcAddr device_proc;
PFN_vkCreateSwapchainKHR create_swapchain;
PFN_vkDestroySwapchainKHR destroy_swapchain;
PFun_slIsFeatureSupported *feature_supported;
PFun_slGetFeatureFunction *feature_function;
PFun_slGetNewFrameToken *new_frame;
PFun_slSetConstants *set_constants;
PFun_slSetTagForFrame *set_tags;
PFun_slEvaluateFeature *evaluate;
PFun_slFreeResources *free_resources;
PFun_slDLSSSetOptions *set_sr_options;
PFun_slDLSSGetOptimalSettings *get_optimal;
PFun_slDLSSGSetOptions *set_options;
PFun_slDLSSGGetState *get_state;
PFun_slPCLSetMarker *set_marker;
PFun_slReflexSleep *reflex_sleep;
sl::FrameToken *token;
const sl::ViewportHandle viewport(0);
uint32_t minimum_size;
bool enabled, wants_fg, supports_sr, supports_fg, constants_sent, sr_allocated;
std::atomic<bool> present_error;

void log_message(sl::LogType, const char *message) {
    std::fprintf(stderr, "[streamline] %s\n", message);
}

void api_error(const sl::APIError &error) {
    if (error.vkRes != VK_ERROR_OUT_OF_DATE_KHR && error.vkRes != VK_SUBOPTIMAL_KHR)
        present_error.store(true, std::memory_order_relaxed);
}

template<typename T> bool get_feature(sl::Feature feature, const char *name, T *&fn) {
    void *address = nullptr;
    if (feature_function(feature, name, address) != sl::Result::eOk) return false;
    fn = reinterpret_cast<T *>(address);
    return fn != nullptr;
}

sl::Resource resource(const FgImage &image) {
    sl::Resource r(sl::ResourceType::eTex2d, image.image, nullptr, image.view, VK_IMAGE_LAYOUT_GENERAL);
    r.width = image.width;
    r.height = image.height;
    r.nativeFormat = image.format;
    r.mipLevels = 1;
    r.arrayLayers = 1;
    r.usage = image.usage;
    return r;
}

void mark(sl::PCLMarker marker) {
    if (token && set_marker) set_marker(marker, *token);
}
}

bool vk_sl_load(bool frame_generation) {
    wants_fg = frame_generation;
    wchar_t path[32768];
    if (!graphics_runtime_path(path, sizeof path / sizeof *path, L"graphics\\nvidia\\streamline\\sl.interposer.dll") ||
        GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return false;
    if (!sl::security::verifyEmbeddedSignature(path)) {
        std::fprintf(stderr, "[dlss-fg] Streamline signature validation failed\n");
        return false;
    }
    library = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!library) return false;
    auto init = reinterpret_cast<PFun_slInit *>(GetProcAddress(library, "slInit"));
#define LOAD(slot, type, name) slot = reinterpret_cast<type>(GetProcAddress(library, name))
    LOAD(instance_proc, PFN_vkGetInstanceProcAddr, "vkGetInstanceProcAddr");
    LOAD(device_proc, PFN_vkGetDeviceProcAddr, "vkGetDeviceProcAddr");
    LOAD(feature_supported, PFun_slIsFeatureSupported *, "slIsFeatureSupported");
    LOAD(feature_function, PFun_slGetFeatureFunction *, "slGetFeatureFunction");
    LOAD(new_frame, PFun_slGetNewFrameToken *, "slGetNewFrameToken");
    LOAD(set_constants, PFun_slSetConstants *, "slSetConstants");
    LOAD(set_tags, PFun_slSetTagForFrame *, "slSetTagForFrame");
    LOAD(evaluate, PFun_slEvaluateFeature *, "slEvaluateFeature");
    LOAD(free_resources, PFun_slFreeResources *, "slFreeResources");
#undef LOAD
    if (!init || !instance_proc || !device_proc || !feature_supported || !feature_function ||
        !new_frame || !set_constants || !set_tags || !evaluate || !free_resources) return false;
    *wcsrchr(path, L'\\') = 0;
    const wchar_t *plugins[] = {path};
    const sl::Feature features[] = {sl::kFeatureDLSS, sl::kFeatureDLSS_G, sl::kFeatureReflex, sl::kFeaturePCL};
    sl::Preferences pref{};
    pref.pathsToPlugins = plugins;
    pref.numPathsToPlugins = 1;
    pref.featuresToLoad = features;
    pref.numFeaturesToLoad = wants_fg ? 4 : 1;
    pref.renderAPI = sl::RenderAPI::eVulkan;
    pref.engine = sl::EngineType::eCustom;
    pref.engineVersion = "1.0.0";
    pref.projectId = "56b11a1c-a579-4c82-bbf9-9842311eff92";
    pref.logMessageCallback = log_message;
    pref.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eUseFrameBasedResourceTagging |
                 sl::PreferenceFlags::eDisableCLStateTracking;
    const sl::Result result = init(pref, sl::kSDKVersion);
    std::fprintf(stderr, "[streamline] Vulkan init: %d (SR with%s FG)\n", (int)result, wants_fg ? "" : "out");
    return result == sl::Result::eOk;
}

PFN_vkCreateInstance vk_dlss_fg_instance_proxy(void) {
    return reinterpret_cast<PFN_vkCreateInstance>(instance_proc(VK_NULL_HANDLE, "vkCreateInstance"));
}

PFN_vkEnumeratePhysicalDevices vk_dlss_fg_enumerate_proxy(VkInstance instance) {
    return reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(instance_proc(instance, "vkEnumeratePhysicalDevices"));
}

bool vk_sl_window_surface(VkInstance instance, void *window, VkSurfaceKHR *surface) {
    auto create = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(instance_proc(instance, "vkCreateWin32SurfaceKHR"));
    if (!create || !window) return false;
    const auto hwnd = static_cast<HWND>(window);
    VkWin32SurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.hinstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
    info.hwnd = hwnd;
    return create(instance, &info, nullptr, surface) == VK_SUCCESS;
}

PFN_vkCreateDevice vk_dlss_fg_device_proxy(VkInstance instance, VkPhysicalDevice physical) {
    sl::AdapterInfo adapter{};
    adapter.vkPhysicalDevice = physical;
    supports_sr = feature_supported(sl::kFeatureDLSS, adapter) == sl::Result::eOk;
    supports_fg = wants_fg && feature_supported(sl::kFeatureDLSS_G, adapter) == sl::Result::eOk;
    std::fprintf(stderr, "[streamline] feature support: SR %d, FG %d\n", supports_sr, supports_fg);
    if (!supports_sr && !supports_fg) return nullptr;
    return reinterpret_cast<PFN_vkCreateDevice>(instance_proc(instance, "vkCreateDevice"));
}

static int attach_fg() {
    if (!get_feature(sl::kFeatureDLSS_G, "slDLSSGSetOptions", set_options) ||
        !get_feature(sl::kFeatureDLSS_G, "slDLSSGGetState", get_state) ||
        !get_feature(sl::kFeaturePCL, "slPCLSetMarker", set_marker) ||
        !get_feature(sl::kFeatureReflex, "slReflexSleep", reflex_sleep)) return 0;
    PFun_slReflexSetOptions *set_reflex = nullptr;
    if (!get_feature(sl::kFeatureReflex, "slReflexSetOptions", set_reflex)) return 0;
    sl::ReflexOptions reflex{};
    reflex.mode = sl::ReflexMode::eLowLatency;
    if (set_reflex(reflex) != sl::Result::eOk) return 0;
    sl::DLSSGState state{};
    if (get_state(viewport, state, nullptr) != sl::Result::eOk || !state.numFramesToGenerateMax) return 0;
    minimum_size = state.minWidthOrHeight;
    return static_cast<int>(std::min(5u, state.numFramesToGenerateMax));
}

bool vk_sl_attach(VkDevice device, FgSwapchainApi *api, int *max_frames) {
    create_swapchain = reinterpret_cast<PFN_vkCreateSwapchainKHR>(device_proc(device, "vkCreateSwapchainKHR"));
    destroy_swapchain = reinterpret_cast<PFN_vkDestroySwapchainKHR>(device_proc(device, "vkDestroySwapchainKHR"));
    *api = {
        reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(device_proc(device, "vkGetSwapchainImagesKHR")),
        reinterpret_cast<PFN_vkAcquireNextImageKHR>(device_proc(device, "vkAcquireNextImageKHR")),
        reinterpret_cast<PFN_vkQueuePresentKHR>(device_proc(device, "vkQueuePresentKHR")),
    };
    if (!create_swapchain || !destroy_swapchain || !api->images || !api->acquire || !api->present) return false;
    supports_sr = supports_sr && get_feature(sl::kFeatureDLSS, "slDLSSSetOptions", set_sr_options) &&
                  get_feature(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", get_optimal);
    *max_frames = supports_fg ? attach_fg() : 0;
    if (!*max_frames) reflex_sleep = nullptr;
    return supports_sr || *max_frames;
}

PFN_vkCreateSwapchainKHR vk_dlss_fg_swapchain_create(void) { return create_swapchain; }
PFN_vkDestroySwapchainKHR vk_dlss_fg_swapchain_destroy(void) { return destroy_swapchain; }

void vk_dlss_fg_begin(uint32_t frame) {
    token = nullptr;
    constants_sent = false;
    if (new_frame(token, &frame) == sl::Result::eOk && token && reflex_sleep) reflex_sleep(*token);
}

static bool frame_constants(const FgFrame &f) {
    if (constants_sent) return true;
    sl::Constants c{};
    std::memcpy(&c.cameraViewToClip, f.projection, sizeof f.projection);
    std::memcpy(&c.clipToCameraView, f.inverse_projection, sizeof f.inverse_projection);
    std::memcpy(&c.clipToPrevClip, f.clip_to_previous, sizeof f.clip_to_previous);
    std::memcpy(&c.prevClipToClip, f.previous_to_clip, sizeof f.previous_to_clip);
    c.cameraPos = {f.camera_position[0], f.camera_position[1], f.camera_position[2]};
    c.cameraUp = {f.camera_up[0], f.camera_up[1], f.camera_up[2]};
    c.cameraRight = {f.camera_right[0], f.camera_right[1], f.camera_right[2]};
    c.cameraFwd = {f.camera_forward[0], f.camera_forward[1], f.camera_forward[2]};
    c.cameraNear = f.near_plane;
    c.cameraFar = f.far_plane;
    c.cameraFOV = f.fov;
    c.cameraAspectRatio = float(f.depth.width) / float(f.depth.height);
    c.cameraPinholeOffset = {0.0f, 0.0f};
    c.jitterOffset = {f.jitter[0], f.jitter[1]};
    c.mvecScale = {1.0f / f.motion.width, 1.0f / f.motion.height};
    c.depthInverted = c.motionVectors3D = sl::Boolean::eFalse;
    c.cameraMotionIncluded = sl::Boolean::eTrue;
    c.reset = f.reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    constants_sent = set_constants(c, *token, viewport) == sl::Result::eOk;
    return constants_sent;
}

bool vk_dlss_fg_tag(VkCommandBuffer cb, const FgFrame *frame, int generated_frames, uint32_t backbuffers) {
    if (!token || !frame || present_error.load(std::memory_order_relaxed) ||
        frame->color.width < minimum_size || frame->color.height < minimum_size) {
        vk_dlss_fg_pause();
        return false;
    }
    const FgFrame &f = *frame;
    sl::Resource color = resource(f.color), depth = resource(f.depth), motion = resource(f.motion);
    constexpr auto life = sl::ResourceLifecycle::eValidUntilPresent;
    const sl::ResourceTag tags[] = {{&color, sl::kBufferTypeHUDLessColor, life},
                                    {&depth, sl::kBufferTypeDepth, life},
                                    {&motion, sl::kBufferTypeMotionVectors, life}};
    if (!frame_constants(f) ||
        set_tags(*token, viewport, tags, 3, reinterpret_cast<sl::CommandBuffer *>(cb)) != sl::Result::eOk) {
        vk_dlss_fg_pause();
        return false;
    }
    sl::DLSSGOptions options{};
    options.mode = sl::DLSSGMode::eOn;
    options.numFramesToGenerate = generated_frames;
    options.numBackBuffers = backbuffers;
    options.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;
    options.onErrorCallback = api_error;
    enabled = set_options(viewport, options) == sl::Result::eOk;
    return enabled;
}

void vk_dlss_fg_submit_marker(bool start) {
    mark(start ? sl::PCLMarker::eRenderSubmitStart : sl::PCLMarker::eRenderSubmitEnd);
}
void vk_dlss_fg_present_marker(bool start) { mark(start ? sl::PCLMarker::ePresentStart : sl::PCLMarker::ePresentEnd); }

int64_t vk_dlss_fg_presented(void) {
    sl::DLSSGState state{};
    const sl::Result result = get_state(viewport, state, nullptr);
    if (enabled && (result != sl::Result::eOk || state.status != sl::DLSSGStatus::eOk))
        std::fprintf(stderr, "[dlss-fg] frame status %#x (query %d)\n", (unsigned)state.status, (int)result);
    if (result != sl::Result::eOk) return -1;
    return state.numFramesActuallyPresented;
}

void vk_dlss_fg_pause(void) {
    if (!enabled) return;
    sl::DLSSGOptions options{};
    options.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;
    set_options(viewport, options);
    enabled = false;
}

bool vk_sl_sr_available(void) { return supports_sr && get_optimal; }

bool vk_sl_sr_size(uint32_t width, uint32_t height, int quality, uint32_t *render_w, uint32_t *render_h) {
    if (!vk_sl_sr_available()) return false;
    constexpr sl::DLSSMode modes[] = {sl::DLSSMode::eDLAA, sl::DLSSMode::eMaxQuality, sl::DLSSMode::eBalanced,
                                      sl::DLSSMode::eMaxPerformance, sl::DLSSMode::eUltraPerformance};
    sl::DLSSOptions options{};
    options.mode = modes[quality];
    options.outputWidth = width;
    options.outputHeight = height;
    options.colorBuffersHDR = sl::Boolean::eFalse;
    sl::DLSSOptimalSettings optimal{};
    if (get_optimal(options, optimal) != sl::Result::eOk || !optimal.optimalRenderWidth || !optimal.optimalRenderHeight ||
        set_sr_options(viewport, options) != sl::Result::eOk) return false;
    *render_w = optimal.optimalRenderWidth;
    *render_h = optimal.optimalRenderHeight;
    std::fprintf(stderr, "[dlss-sr] Streamline: %ux%u -> %ux%u\n", *render_w, *render_h, width, height);
    return true;
}

bool vk_sl_sr_dispatch(VkCommandBuffer cb, const FgFrame *frame, const FgImage *input, const FgImage *output) {
    if (!token || !frame || !input->view || !output->view || !frame_constants(*frame)) return false;
    sl::Resource color = resource(*input), target = resource(*output);
    sl::Resource depth = resource(frame->depth), motion = resource(frame->motion);
    constexpr auto life = sl::ResourceLifecycle::eOnlyValidNow;
    const sl::ResourceTag tags[] = {{&color, sl::kBufferTypeScalingInputColor, life},
                                    {&target, sl::kBufferTypeScalingOutputColor, life},
                                    {&depth, sl::kBufferTypeDepth, life},
                                    {&motion, sl::kBufferTypeMotionVectors, life}};
    auto *cmd = reinterpret_cast<sl::CommandBuffer *>(cb);
    const sl::BaseStructure *inputs[] = {&viewport};
    sl::Result result = set_tags(*token, viewport, tags, 4, cmd);
    if (result == sl::Result::eOk) {
        sr_allocated = true;
        result = evaluate(sl::kFeatureDLSS, *token, inputs, 1, cmd);
    }
    if (result != sl::Result::eOk) std::fprintf(stderr, "[dlss-sr] Streamline dispatch failed: %d\n", (int)result);
    return result == sl::Result::eOk;
}

void vk_sl_sr_reset(void) {
    if (sr_allocated) free_resources(sl::kFeatureDLSS, viewport);
    sr_allocated = false;
}
#else
bool vk_sl_load(bool) { return false; }
PFN_vkCreateInstance vk_dlss_fg_instance_proxy(void) { return nullptr; }
PFN_vkEnumeratePhysicalDevices vk_dlss_fg_enumerate_proxy(VkInstance) { return nullptr; }
bool vk_sl_window_surface(VkInstance, void *, VkSurfaceKHR *) { return false; }
PFN_vkCreateDevice vk_dlss_fg_device_proxy(VkInstance, VkPhysicalDevice) { return nullptr; }
bool vk_sl_attach(VkDevice, FgSwapchainApi *, int *) { return false; }
PFN_vkCreateSwapchainKHR vk_dlss_fg_swapchain_create(void) { return nullptr; }
PFN_vkDestroySwapchainKHR vk_dlss_fg_swapchain_destroy(void) { return nullptr; }
void vk_dlss_fg_begin(uint32_t) {}
bool vk_dlss_fg_tag(VkCommandBuffer, const FgFrame *, int, uint32_t) { return false; }
void vk_dlss_fg_submit_marker(bool) {}
void vk_dlss_fg_present_marker(bool) {}
int64_t vk_dlss_fg_presented(void) { return -1; }
void vk_dlss_fg_pause(void) {}
bool vk_sl_sr_available(void) { return false; }
bool vk_sl_sr_size(uint32_t, uint32_t, int, uint32_t *, uint32_t *) { return false; }
bool vk_sl_sr_dispatch(VkCommandBuffer, const FgFrame *, const FgImage *, const FgImage *) { return false; }
void vk_sl_sr_reset(void) {}
#endif
