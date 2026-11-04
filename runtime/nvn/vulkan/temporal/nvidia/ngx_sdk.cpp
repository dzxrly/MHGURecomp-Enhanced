// C ABI boundary around NVIDIA's official SDK. Compiled with MSVC and linked
// against nvsdk_ngx_s.lib; the renderer keeps its own Vulkan device and queue.
// This software contains source code provided by NVIDIA Corporation.
#include <vulkan/vulkan.h>
#include <nvsdk_ngx_vk.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define API extern "C" __declspec(dllexport)
API NVSDK_NGX_Result mhgu_ngx_init(const char *id, NVSDK_NGX_EngineType engine, const char *version,
                                   const wchar_t *data, VkInstance instance, VkPhysicalDevice physical, VkDevice device,
                                   PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa, NVSDK_NGX_Version sdk,
                                   const NVSDK_NGX_FeatureCommonInfo *info) {
    return NVSDK_NGX_VULKAN_Init_with_ProjectID(id, engine, version, data, instance, physical, device, gipa, gdpa, info,
                                                sdk);
}
API NVSDK_NGX_Result mhgu_ngx_allocate(NVSDK_NGX_Parameter **p) { return NVSDK_NGX_VULKAN_AllocateParameters(p); }
API NVSDK_NGX_Result mhgu_ngx_capabilities(NVSDK_NGX_Parameter **p) {
    return NVSDK_NGX_VULKAN_GetCapabilityParameters(p);
}
API NVSDK_NGX_Result mhgu_ngx_create(VkDevice d, VkCommandBuffer cb, NVSDK_NGX_Feature f, NVSDK_NGX_Parameter *p,
                                     NVSDK_NGX_Handle **h) {
    return NVSDK_NGX_VULKAN_CreateFeature1(d, cb, f, p, h);
}
API NVSDK_NGX_Result mhgu_ngx_evaluate(VkCommandBuffer cb, const NVSDK_NGX_Handle *h, const NVSDK_NGX_Parameter *p,
                                       void *) {
    return NVSDK_NGX_VULKAN_EvaluateFeature(cb, h, p, nullptr);
}
API NVSDK_NGX_Result mhgu_ngx_release(NVSDK_NGX_Handle *h) { return NVSDK_NGX_VULKAN_ReleaseFeature(h); }
API NVSDK_NGX_Result mhgu_ngx_destroy(NVSDK_NGX_Parameter *p) { return NVSDK_NGX_VULKAN_DestroyParameters(p); }
API void mhgu_ngx_set_ui(NVSDK_NGX_Parameter *p, const char *k, unsigned int v) { p->Set(k, v); }
API void mhgu_ngx_set_i(NVSDK_NGX_Parameter *p, const char *k, int v) { p->Set(k, v); }
API void mhgu_ngx_set_f(NVSDK_NGX_Parameter *p, const char *k, float v) { p->Set(k, v); }
API void mhgu_ngx_set_p(NVSDK_NGX_Parameter *p, const char *k, void *v) { p->Set(k, v); }
API unsigned int mhgu_ngx_get_ui(NVSDK_NGX_Parameter *p, const char *k, unsigned int *v) { return p->Get(k, v); }
API unsigned int mhgu_ngx_get_p(NVSDK_NGX_Parameter *p, const char *k, void **v) { return p->Get(k, v); }

// The explicit community runtime path calls the Vulkan snippet itself. Older
// driver cores can reject feature 18 before dispatching it, even when the snippet
// runs on the same device. See AlrikOlson/bevy_dlss5 src/ngx.rs. The snippet also
// checks its caller module's name, so this adapter is named mhgu-nvngx.dll.
// Keep a real return address in this module: tail calls would expose the host.
using NrInit = NVSDK_NGX_Result (*)(unsigned long long, const wchar_t *, VkInstance, VkPhysicalDevice, VkDevice,
                                    PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr, unsigned int, const void *);
using NrCreate = NVSDK_NGX_Result (*)(VkDevice, VkCommandBuffer, NVSDK_NGX_Feature, NVSDK_NGX_Parameter *,
                                      NVSDK_NGX_Handle **);
using NrEvaluate = NVSDK_NGX_Result (*)(VkCommandBuffer, const NVSDK_NGX_Handle *, const NVSDK_NGX_Parameter *, void *);
static HMODULE nr_module;
static NrCreate nr_create;
static NrEvaluate nr_evaluate;
static NVSDK_NGX_Result (*nr_release)(NVSDK_NGX_Handle *);

API NVSDK_NGX_Result mhgu_ngx_nr_init(const wchar_t *path, const wchar_t *data, VkInstance instance,
                                      VkPhysicalDevice physical, VkDevice device, PFN_vkGetInstanceProcAddr gipa,
                                      PFN_vkGetDeviceProcAddr gdpa, const NVSDK_NGX_Parameter *caps) {
    nr_module = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!nr_module)
        return NVSDK_NGX_Result_FAIL_FeatureNotFound;
    auto init = (NrInit)GetProcAddress(nr_module, "NVSDK_NGX_VULKAN_Init_Ext2");
    nr_create = (NrCreate)GetProcAddress(nr_module, "NVSDK_NGX_VULKAN_CreateFeature1");
    nr_evaluate = (NrEvaluate)GetProcAddress(nr_module, "NVSDK_NGX_VULKAN_EvaluateFeature");
    nr_release = (decltype(nr_release))GetProcAddress(nr_module, "NVSDK_NGX_VULKAN_ReleaseFeature");
    if (!init || !nr_create || !nr_evaluate || !nr_release)
        return NVSDK_NGX_Result_FAIL_FeatureNotFound;
    volatile auto result = init(0, data, instance, physical, device, gipa, gdpa, NVSDK_NGX_Version_API, caps);
    return result;
}
API NVSDK_NGX_Result mhgu_ngx_nr_create(VkDevice d, VkCommandBuffer cb, NVSDK_NGX_Feature f, NVSDK_NGX_Parameter *p,
                                        NVSDK_NGX_Handle **h) {
    volatile auto result = nr_create(d, cb, f, p, h);
    return result;
}
API NVSDK_NGX_Result mhgu_ngx_nr_evaluate(VkCommandBuffer cb, const NVSDK_NGX_Handle *h, const NVSDK_NGX_Parameter *p,
                                          void *callback) {
    volatile auto result = nr_evaluate(cb, h, p, callback);
    return result;
}
API NVSDK_NGX_Result mhgu_ngx_nr_release(NVSDK_NGX_Handle *h) {
    volatile auto result = nr_release(h);
    return result;
}
