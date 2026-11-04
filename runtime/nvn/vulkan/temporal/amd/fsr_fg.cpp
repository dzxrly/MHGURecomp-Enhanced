#include "temporal/frame_generation.h"

#ifdef _WIN32
#include <cstdio>
#include <cstring>
#include "temporal/runtime_dll.h"
#include "framegeneration/include/ffx_framegeneration.h"
#include "framegeneration/include/vk/ffx_api_framegeneration_vk.h"

namespace {
HMODULE library;
PfnFfxCreateContext create;
PfnFfxDestroyContext destroy;
PfnFfxConfigure configure;
PfnFfxQuery query;
PfnFfxDispatch dispatch;
ffxContext swap_context, frame_context;
ffxQueryDescSwapchainReplacementFunctionsVK functions{};
VkDevice vk_device;
VkPhysicalDevice vk_physical;
VkQueueInfoFFXAPI game_queue, present_queue, acquire_queue;
VkSwapchainKHR chain;
VkFormat swap_format;
VkExtent2D extent;
bool active;

FfxApiResource resource(const FgImage &image) {
    FfxApiResourceDescription desc{};
    desc.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
    desc.format = ffxApiGetSurfaceFormatVK(image.format);
    desc.width = image.width;
    desc.height = image.height;
    desc.depth = desc.mipCount = 1;
    desc.usage = FFX_API_RESOURCE_USAGE_READ_ONLY;
    if (image.usage & VK_IMAGE_USAGE_STORAGE_BIT) desc.usage |= FFX_API_RESOURCE_USAGE_UAV;
    return ffxApiGetResourceVK(image.image, desc, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
}

ffxReturnCode_t generate(ffxDispatchDescFrameGeneration *desc, void *) {
    return dispatch(&frame_context, &desc->header);
}

bool configure_frame(bool enabled, uint64_t id, const FgImage *color) {
    ffxConfigureDescFrameGeneration config{};
    config.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
    config.swapChain = chain;
    config.frameGenerationEnabled = enabled;
    config.frameGenerationCallback = generate;
    config.generationRect = {0, 0, static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height)};
    config.frameID = id;
    if (color) config.HUDLessColor = resource(*color);
    ffxReturnCode_t rc = configure(&frame_context, &config.header);
    if (rc == FFX_API_RETURN_OK) active = enabled;
    return rc == FFX_API_RETURN_OK;
}

bool start_context() {
    ffxCreateBackendVKDesc backend{};
    backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
    backend.deviceContext = {vk_device, vk_physical, vkGetDeviceProcAddr};
    ffxCreateContextDescFrameGenerationHudless hudless{};
    hudless.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_HUDLESS;
    hudless.header.pNext = &backend.header;
    hudless.hudlessBackBufferFormat = FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
    ffxCreateContextDescFrameGenerationVersion version{};
    version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_VERSION;
    version.header.pNext = &hudless.header;
    version.version = FFX_FRAMEGENERATION_VERSION;
    ffxCreateContextDescFrameGeneration desc{};
    desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
    desc.header.pNext = &version.header;
    desc.displaySize = desc.maxRenderSize = {extent.width, extent.height};
    desc.backBufferFormat = ffxApiGetSurfaceFormatVK(swap_format);
    ffxReturnCode_t rc = create(&frame_context, &desc.header, nullptr);
    if (rc != FFX_API_RETURN_OK)
        std::fprintf(stderr, "[fsr-fg] generation context failed: %u\n", (unsigned)rc);
    return rc == FFX_API_RETURN_OK;
}
}

bool vk_fsr_fg_load(void) {
    wchar_t path[32768];
    if (!graphics_runtime_path(path, sizeof path / sizeof *path,
                              L"graphics\\amd\\fsr\\amd_fidelityfx_loader_vk.dll") ||
        GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return false;
    library = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!library) return false;
    create = reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(library, "ffxCreateContext"));
    destroy = reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(library, "ffxDestroyContext"));
    configure = reinterpret_cast<PfnFfxConfigure>(GetProcAddress(library, "ffxConfigure"));
    query = reinterpret_cast<PfnFfxQuery>(GetProcAddress(library, "ffxQuery"));
    dispatch = reinterpret_cast<PfnFfxDispatch>(GetProcAddress(library, "ffxDispatch"));
    return create && destroy && configure && query && dispatch;
}

void vk_fsr_fg_device(VkDevice device, VkPhysicalDevice physical, VkQueue game, uint32_t game_family,
                       VkQueue present, uint32_t present_family, VkQueue acquire, uint32_t acquire_family) {
    vk_device = device;
    vk_physical = physical;
    game_queue = {game, game_family, nullptr};
    present_queue = {present, present_family, nullptr};
    acquire_queue = {acquire, acquire_family, nullptr};
}

VkResult vk_fsr_fg_create(const VkSwapchainCreateInfoKHR *info, VkSwapchainKHR *swapchain, FgSwapchainApi *api) {
    extent = info->imageExtent;
    swap_format = info->imageFormat;
    if (!start_context()) return VK_ERROR_INITIALIZATION_FAILED;
    ffxCreateContextDescFrameGenerationSwapChainVersionVK version{};
    version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_VERSION_VK;
    version.version = FFX_FRAMEGENERATION_SWAPCHAIN_VK_VERSION;
    ffxCreateContextDescFrameGenerationSwapChainVK desc{};
    desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_VK;
    desc.header.pNext = &version.header;
    desc.device = vk_device;
    desc.physicalDevice = vk_physical;
    desc.swapchain = &chain;
    desc.createInfo = *info;
    desc.gameQueue = game_queue;
    desc.presentQueue = present_queue;
    desc.imageAcquireQueue = acquire_queue;
    ffxReturnCode_t rc = create(&swap_context, &desc.header, nullptr);
    if (rc != FFX_API_RETURN_OK) {
        std::fprintf(stderr, "[fsr-fg] swapchain creation failed: %u\n", (unsigned)rc);
        destroy(&frame_context, nullptr);
        frame_context = nullptr;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    functions = {};
    functions.header.type = FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_FUNCTIONS_VK;
    rc = query(&swap_context, &functions.header);
    if (rc != FFX_API_RETURN_OK || !functions.pOutGetSwapchainImagesKHR || !functions.pOutAcquireNextImageKHR ||
        !functions.pOutQueuePresentKHR || !functions.pOutDestroySwapchainFFXAPI) {
        std::fprintf(stderr, "[fsr-fg] swapchain function query failed: %u\n", (unsigned)rc);
        vk_fsr_fg_destroy(chain);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *api = {functions.pOutGetSwapchainImagesKHR, functions.pOutAcquireNextImageKHR, functions.pOutQueuePresentKHR};
    *swapchain = chain;
    return VK_SUCCESS;
}

void vk_fsr_fg_wait(void) {
    if (!swap_context) return;
    ffxDispatchDescFrameGenerationSwapChainWaitForPresentsVK wait{};
    wait.header.type = FFX_API_DISPATCH_DESC_TYPE_FGSWAPCHAIN_WAIT_FOR_PRESENTS_VK;
    dispatch(&swap_context, &wait.header);
}

void vk_fsr_fg_destroy(VkSwapchainKHR swapchain) {
    vk_fsr_fg_wait();
    if (active) configure_frame(false, 0, nullptr);
    if (frame_context) destroy(&frame_context, nullptr);
    if (swap_context) destroy(&swap_context, nullptr);
    if (swapchain && functions.pOutDestroySwapchainFFXAPI)
        functions.pOutDestroySwapchainFFXAPI(vk_device, swapchain, nullptr, swapchain);
    frame_context = swap_context = nullptr;
    chain = VK_NULL_HANDLE;
    active = false;
}

bool vk_fsr_fg_prepare(VkCommandBuffer cb, const FgFrame *frame, uint64_t frame_id, bool enable) {
    if (!frame_context) return false;
    if (!frame || !enable || frame->depth.width > extent.width || frame->depth.height > extent.height) {
        if (active) configure_frame(false, frame_id, nullptr);
        return false;
    }
    const FgFrame &f = *frame;
    ffxDispatchDescFrameGenerationPrepareV2 prep{};
    prep.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2;
    prep.commandList = cb;
    prep.frameID = frame_id;
    prep.renderSize = {f.depth.width, f.depth.height};
    prep.jitterOffset = {f.jitter[0], f.jitter[1]};
    prep.motionVectorScale = {1.0f, 1.0f};
    prep.frameTimeDelta = f.frame_ms;
    prep.reset = f.reset || !active;
    prep.cameraNear = f.near_plane;
    prep.cameraFar = f.far_plane;
    prep.cameraFovAngleVertical = f.fov;
    prep.viewSpaceToMetersFactor = 0.01f;
    prep.depth = resource(f.depth);
    prep.motionVectors = resource(f.motion);
    std::memcpy(prep.cameraPosition, f.camera_position, sizeof f.camera_position);
    std::memcpy(prep.cameraUp, f.camera_up, sizeof f.camera_up);
    std::memcpy(prep.cameraRight, f.camera_right, sizeof f.camera_right);
    std::memcpy(prep.cameraForward, f.camera_forward, sizeof f.camera_forward);
    ffxReturnCode_t rc = dispatch(&frame_context, &prep.header);
    if (rc == FFX_API_RETURN_OK && configure_frame(true, frame_id, &f.color)) return true;
    std::fprintf(stderr, "[fsr-fg] frame preparation failed: %u\n", (unsigned)rc);
    if (active) configure_frame(false, frame_id, nullptr);
    return false;
}

bool vk_fsr_fg_present_count(VkSwapchainKHR swapchain, uint64_t *count) {
    if (!functions.pOutGetLastPresentCountFFXAPI) return false;
    *count = functions.pOutGetLastPresentCountFFXAPI(swapchain);
    return true;
}
#else
bool vk_fsr_fg_load(void) { return false; }
void vk_fsr_fg_device(VkDevice, VkPhysicalDevice, VkQueue, uint32_t, VkQueue, uint32_t, VkQueue, uint32_t) {}
VkResult vk_fsr_fg_create(const VkSwapchainCreateInfoKHR *, VkSwapchainKHR *, FgSwapchainApi *) { return VK_ERROR_INITIALIZATION_FAILED; }
void vk_fsr_fg_wait(void) {}
void vk_fsr_fg_destroy(VkSwapchainKHR) {}
bool vk_fsr_fg_prepare(VkCommandBuffer, const FgFrame *, uint64_t, bool) { return false; }
bool vk_fsr_fg_present_count(VkSwapchainKHR, uint64_t *) { return false; }
#endif
