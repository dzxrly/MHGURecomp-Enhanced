// DLSS SR / DLSS 5 NR through the official NGX SDK, on our own Vulkan device.
// Runtime DLLs are supplied separately. No hooks, driver patches or API bridge.
#include <stdlib.h>
#include <string.h>
#include "../../rt.h"
#include "ngx.h"
#include "graphics.h"

#ifndef _WIN32
bool vk_ngx_device_exts(const VkExtensionProperties *a, uint32_t n, const char **e, uint32_t *ne, uint32_t cap) {
    return false;
}
bool vk_ngx_init(void) {
    LOG("[ngx] this DLSS integration currently requires Windows\n");
    return false;
}
bool vk_ngx_render_size(uint32_t ow, uint32_t oh, int q, uint32_t *w, uint32_t *h) { return false; }
bool vk_ngx_sr(const NgxFrame *f) { return false; }
bool vk_ngx_nr(const NgxFrame *f) { return false; }
bool vk_ngx_sr_ready(void) { return false; }
bool vk_ngx_nr_ready(void) { return false; }
void vk_ngx_reset(void) {}
#else
#include <windows.h>
#include <nvsdk_ngx_vk.h>
#include "graphics.h"
#include "temporal/runtime_dll.h"

// The official SDK adapter also keeps NVIDIA's parameter vtables in the Microsoft ABI.
static void (*mhgu_ngx_set_ui)(NVSDK_NGX_Parameter *, const char *, unsigned int);
static void (*mhgu_ngx_set_i)(NVSDK_NGX_Parameter *, const char *, int);
static void (*mhgu_ngx_set_f)(NVSDK_NGX_Parameter *, const char *, float);
static void (*mhgu_ngx_set_p)(NVSDK_NGX_Parameter *, const char *, void *);
static unsigned int (*mhgu_ngx_get_ui)(NVSDK_NGX_Parameter *, const char *, unsigned int *);
static unsigned int (*mhgu_ngx_get_p)(NVSDK_NGX_Parameter *, const char *, void **);
#define UI mhgu_ngx_set_ui
#define INT mhgu_ngx_set_i
#define FLT mhgu_ngx_set_f
#define PTR mhgu_ngx_set_p

typedef NVSDK_NGX_Result (*InitProject)(const char *, NVSDK_NGX_EngineType, const char *, const wchar_t *, VkInstance,
                                        VkPhysicalDevice, VkDevice, PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr,
                                        NVSDK_NGX_Version, const NVSDK_NGX_FeatureCommonInfo *);
typedef NVSDK_NGX_Result (*Parameters)(NVSDK_NGX_Parameter **);
typedef NVSDK_NGX_Result (*Create)(VkDevice, VkCommandBuffer, NVSDK_NGX_Feature, NVSDK_NGX_Parameter *,
                                   NVSDK_NGX_Handle **);
typedef NVSDK_NGX_Result (*Evaluate)(VkCommandBuffer, const NVSDK_NGX_Handle *, const NVSDK_NGX_Parameter *, void *);
typedef NVSDK_NGX_Result (*Release)(NVSDK_NGX_Handle *);
typedef NVSDK_NGX_Result (*Destroy)(NVSDK_NGX_Parameter *);
typedef NVSDK_NGX_Result (*Optimal)(NVSDK_NGX_Parameter *);
typedef NVSDK_NGX_Result (*NrInit)(const wchar_t *, const wchar_t *, VkInstance, VkPhysicalDevice, VkDevice,
                                   PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr, const NVSDK_NGX_Parameter *);

static HMODULE core;
static InitProject init_project;
static Parameters allocate, capabilities;
static Create create_feature;
static Evaluate evaluate;
static NrInit init_nr;
static Create create_nr;
static Evaluate evaluate_nr;
static Release release_feature;
static Release release_nr;
static Destroy destroy_parameters;
static NVSDK_NGX_Parameter *caps;
static bool attempted, initialised, extensions_ok;
static bool nr_direct;
static wchar_t runtime_dir[32768];
static wchar_t nr_dir[32768];

typedef struct {
    NVSDK_NGX_Parameter *p;
    NVSDK_NGX_Handle *handle;
    VkEvent ready;
    bool failed;
    bool creation_retired;
    uint64_t frames;
    uint32_t w, h;
} Model;
static Model sr, nr;

static bool load_core(void) {
    if (core)
        return true;
    // Load our official-SDK adapter by absolute executable-relative path.
    wchar_t path[32768];
    if (!graphics_runtime_path(path, sizeof path / sizeof *path, L"graphics\\nvidia\\ngx\\mhgu-nvngx.dll"))
        return false;
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES &&
        !graphics_runtime_path(path, sizeof path / sizeof *path, L"mhgu-nvngx.dll"))
        return false;
    core = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!core) {
        LOG("[ngx] cannot load installed NGX core (Windows error %lu)\n", GetLastError());
        return false;
    }
#define IMPORT(dst, name)                                                                                              \
    do {                                                                                                               \
        dst = (void *)GetProcAddress(core, name);                                                                      \
        if (!dst) {                                                                                                    \
            LOG("[ngx] core lacks %s\n", name);                                                                        \
            FreeLibrary(core);                                                                                         \
            core = NULL;                                                                                               \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)
    IMPORT(init_project, "mhgu_ngx_init");
    IMPORT(allocate, "mhgu_ngx_allocate");
    IMPORT(capabilities, "mhgu_ngx_capabilities");
    IMPORT(create_feature, "mhgu_ngx_create");
    IMPORT(evaluate, "mhgu_ngx_evaluate");
    IMPORT(init_nr, "mhgu_ngx_nr_init");
    IMPORT(create_nr, "mhgu_ngx_nr_create");
    IMPORT(evaluate_nr, "mhgu_ngx_nr_evaluate");
    IMPORT(release_feature, "mhgu_ngx_release");
    IMPORT(release_nr, "mhgu_ngx_nr_release");
    IMPORT(destroy_parameters, "mhgu_ngx_destroy");
    IMPORT(mhgu_ngx_set_ui, "mhgu_ngx_set_ui");
    IMPORT(mhgu_ngx_set_i, "mhgu_ngx_set_i");
    IMPORT(mhgu_ngx_set_f, "mhgu_ngx_set_f");
    IMPORT(mhgu_ngx_set_p, "mhgu_ngx_set_p");
    IMPORT(mhgu_ngx_get_ui, "mhgu_ngx_get_ui");
    IMPORT(mhgu_ngx_get_p, "mhgu_ngx_get_p");
#undef IMPORT
    return true;
}

bool vk_ngx_device_exts(const VkExtensionProperties *a, uint32_t n, const char **ext, uint32_t *ne, uint32_t capacity) {
    if (!load_core())
        return false;
    static const char *const required[] = {"VK_NVX_binary_import", "VK_NVX_image_view_handle", "VK_KHR_push_descriptor",
                                           "VK_KHR_buffer_device_address"};
    for (uint32_t k = 0; k < sizeof required / sizeof *required; k++) {
        bool found = false;
        for (uint32_t j = 0; j < n; j++)
            found |= !strcmp(a[j].extensionName, required[k]);
        if (!found) {
            LOG("[ngx] unavailable: device lacks %s\n", required[k]);
            return false;
        }
    }
    for (uint32_t k = 0; k < sizeof required / sizeof *required; k++) {
        bool present = false;
        for (uint32_t j = 0; j < *ne; j++)
            present |= !strcmp(ext[j], required[k]);
        if (!present) {
            if (*ne == capacity)
                return false;
            ext[(*ne)++] = required[k];
        }
    }
    return extensions_ok = true;
}

static void log_ngx(const char *message, NVSDK_NGX_Logging_Level level, NVSDK_NGX_Feature feature) {
    LOG("[ngx:%u:%u] %s\n", (unsigned)feature, (unsigned)level, message);
}

bool vk_ngx_init(void) {
    if (attempted)
        return initialised;
    attempted = true;
    // Any early failure must remain visible in F1, rather than waiting forever.
    if (g_graphics.fsr && g_graphics.fsr_upscaler == FSR_UP_DLSS)
        config_graphics_report(false, GFX_UNAVAILABLE);
    if (g_graphics.dlss_nr)
        config_graphics_report(true, GFX_UNAVAILABLE);
    if (!extensions_ok || !load_core()) {
        return false;
    }
    DWORD len = GetEnvironmentVariableW(L"MHGU_NGX_DIR", runtime_dir, sizeof runtime_dir / sizeof *runtime_dir);
    static wchar_t sr_dir[32768], sl_dir[32768];
    const wchar_t *sr_path = runtime_dir;
    const wchar_t *nr_layout = L"dlss\\nr";
    if (!len) {
        len = GetModuleFileNameW(NULL, runtime_dir, sizeof runtime_dir / sizeof *runtime_dir);
        if (!len || len >= sizeof runtime_dir / sizeof *runtime_dir) return false;
        wchar_t *slash = wcsrchr(runtime_dir, L'\\');
        if (!slash)
            return false;
        *slash = 0;
        if (graphics_runtime_path(sr_dir, sizeof sr_dir / sizeof *sr_dir, L"graphics\\nvidia\\ngx") &&
            GetFileAttributesW(sr_dir) != INVALID_FILE_ATTRIBUTES) {
            sr_path = sr_dir;
            nr_layout = L"graphics\\nvidia\\ngx\\nr";
        }
    } else if (len >= sizeof runtime_dir / sizeof *runtime_dir)
        return false;
    if (swprintf(nr_dir, sizeof nr_dir / sizeof *nr_dir, L"%ls\\%ls\\%ls", runtime_dir, nr_layout,
                 g_graphics.nr_runtime == NR_RUNTIME_SM89 ? L"sm89" : L"official") < 0)
        return false;
    static const wchar_t *paths[3];
    paths[0] = nr_dir;
    paths[1] = sr_path;
    static NVSDK_NGX_FeatureCommonInfo info;
    info.PathListInfo.Path = paths;
    info.PathListInfo.Length = 2;
    if (sr_path == sr_dir && graphics_runtime_path(sl_dir, sizeof sl_dir / sizeof *sl_dir, L"graphics\\nvidia\\streamline")) {
        paths[2] = sl_dir;
        info.PathListInfo.Length++;
    }
    info.LoggingInfo.LoggingCallback = log_ngx;
    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
    info.LoggingInfo.DisableOtherLoggingSinks = true;
    // This project's own identifier, rather than borrowing another game's AppID.
    LOG("[ngx] initialising NGX Vulkan project interface\n");
    NVSDK_NGX_Result r = init_project("56b11a1c-a579-4c82-bbf9-9842311eff92", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0.0",
                                      runtime_dir, g_vk.inst, g_vk.phys, g_vk.dev, vkGetInstanceProcAddr,
                                      vkGetDeviceProcAddr, NVSDK_NGX_Version_API, &info);
    if (r != NVSDK_NGX_Result_Success) {
        LOG("[ngx] Vulkan init failed: %#x\n", r);
        return false;
    }
    LOG("[ngx] Vulkan project initialised; querying capabilities\n");
    r = capabilities(&caps);
    if (r != NVSDK_NGX_Result_Success || !caps) {
        LOG("[ngx] capability query failed: %#x\n", r);
        return false;
    }
    unsigned int available = 0;
    mhgu_ngx_get_ui(caps, "SuperSampling.Available", &available);
    sr.failed = !available;
    if (sr.failed) {
        LOG("[ngx] DLSS SR not available on this driver/device/runtime combination\n");
        if (g_graphics.fsr && g_graphics.fsr_upscaler == FSR_UP_DLSS)
            config_graphics_report(false, GFX_UNAVAILABLE);
    }
    LOG("[ngx] initialised on the existing Vulkan device; SR available %u; NR requested %d\n", available,
        g_graphics.dlss_nr);
    initialised = true;
    if (!sr.failed && g_graphics.fsr && g_graphics.fsr_upscaler == FSR_UP_DLSS)
        config_graphics_report(false, GFX_WAITING);
    {
        wchar_t dll[32768];
        if (swprintf(dll, sizeof dll / sizeof *dll, L"%ls\\nvngx_dlssnr.dll", nr_dir) < 0 ||
            GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES) {
            nr.failed = true;
            config_graphics_report(true, GFX_UNAVAILABLE);
            LOG("[ngx] selected NR runtime is missing under %ls; NR disabled, no runtime substitution\n", nr_dir);
        } else {
            config_graphics_report(true, GFX_WAITING);
            LOG("[ngx] selected NR runtime: %s (explicit selection; no automatic switch)\n",
                g_graphics.nr_runtime == NR_RUNTIME_SM89 ? "community SM89 / RTX 40" : "official / RTX 50");
            if (g_graphics.nr_runtime == NR_RUNTIME_SM89) {
                r = init_nr(dll, runtime_dir, g_vk.inst, g_vk.phys, g_vk.dev, vkGetInstanceProcAddr,
                            vkGetDeviceProcAddr, caps);
                nr_direct = r == NVSDK_NGX_Result_Success;
                nr.failed = !nr_direct;
                if (nr.failed)
                    config_graphics_report(true, GFX_UNAVAILABLE);
                LOG("[ngx] explicit SM89 native Vulkan snippet init: %#x\n", r);
            }
        }
    }
    return true;
}

static int quality_mode(int q) {
    static const int modes[] = {NVSDK_NGX_PerfQuality_Value_DLAA, NVSDK_NGX_PerfQuality_Value_MaxQuality,
                                NVSDK_NGX_PerfQuality_Value_Balanced, NVSDK_NGX_PerfQuality_Value_MaxPerf,
                                NVSDK_NGX_PerfQuality_Value_UltraPerformance};
    return modes[q >= FSR_NATIVE && q <= FSR_ULTRA_PERFORMANCE ? q : FSR_NATIVE];
}

bool vk_ngx_render_size(uint32_t ow, uint32_t oh, int quality, uint32_t *w, uint32_t *h) {
    if (!vk_ngx_init() || sr.failed)
        return false;
    if (quality == FSR_NATIVE) {
        *w = ow;
        *h = oh;
        return true;
    }
    void *fn = NULL;
    if (mhgu_ngx_get_p(caps, "DLSSOptimalSettingsCallback", &fn) != NVSDK_NGX_Result_Success || !fn) {
        LOG("[ngx] DLSS optimal-settings callback missing\n");
        return false;
    }
    UI(caps, "Width", ow);
    UI(caps, "Height", oh);
    INT(caps, "PerfQualityValue", quality_mode(quality));
    NVSDK_NGX_Result r = ((Optimal)fn)(caps);
    if (r != NVSDK_NGX_Result_Success || mhgu_ngx_get_ui(caps, "OutWidth", w) != NVSDK_NGX_Result_Success ||
        mhgu_ngx_get_ui(caps, "OutHeight", h) != NVSDK_NGX_Result_Success || !*w || !*h || *w > ow || *h > oh) {
        LOG("[ngx] DLSS render-size query failed: %#x\n", r);
        return false;
    }
    LOG("[ngx] DLSS recommended input %ux%u for %ux%u\n", *w, *h, ow, oh);
    return true;
}

static NVSDK_NGX_Resource_VK resource(NgxImage i) {
    NVSDK_NGX_Resource_VK r = {0};
    r.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
    r.ReadWrite = i.storage;
    r.Resource.ImageViewInfo = (NVSDK_NGX_ImageViewInfo_VK){i.view,   i.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                                                            i.format, i.width, i.height};
    return r;
}

static void nr_controls(NVSDK_NGX_Parameter *p) {
    FLT(p, "DLSSNR.Intensity", g_graphics.nr_intensity);
    UI(p, "DLSSNR.Style", g_graphics.nr_style);
    FLT(p, "DLSSNR.LocalStructureStrength", g_graphics.nr_structure);
    FLT(p, "DLSSNR.LocalToneStrength", g_graphics.nr_tone);
    FLT(p, "DLSSNR.SkinStructureStrength", g_graphics.nr_skin);
    UI(p, "DLSSNR.UseAutoMask", g_graphics.nr_auto_mask);
}

static bool model_ready(Model *m, const NgxFrame *f, bool neural) {
    if (!initialised || m->failed)
        return false;
    if (m->handle) {
        if (!m->creation_retired)
            m->creation_retired = vkGetEventStatus(g_vk.dev, m->ready) == VK_EVENT_SET;
        return m->creation_retired;
    }
    NVSDK_NGX_Result r = allocate(&m->p);
    if (r != NVSDK_NGX_Result_Success || !m->p)
        goto fail;
    NVSDK_NGX_Parameter *p = m->p;
    if (neural) {
        UI(p, "Width", f->color.width);
        UI(p, "Height", f->color.height);
        UI(p, "OutWidth", f->output.width);
        UI(p, "OutHeight", f->output.height);
        UI(p, "DLSSNR.Enabled", 1);
        UI(p, "DLSSNR.Width", f->color.width);
        UI(p, "DLSSNR.Height", f->color.height);
        UI(p, "DLSSNR.OutputWidth", f->output.width);
        UI(p, "DLSSNR.OutputHeight", f->output.height);
        INT(p, "DLSSNR.Hint.Render.Preset", 1); // Original FP16 model, including community SM89 runtimes.
        UI(p, "DLSSNR.UICorrection", 0);
        nr_controls(p);
    } else {
        UI(p, "Width", f->color.width);
        UI(p, "Height", f->color.height);
        UI(p, "OutWidth", f->output.width);
        UI(p, "OutHeight", f->output.height);
        INT(p, "PerfQualityValue", quality_mode(g_graphics.fsr_quality));
        INT(p, "DLSS.Feature.Create.Flags",
            NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure);
        UI(p, "DLSS.Enable.Output.Subrects", 0);
    }
    UI(p, "CreationNodeMask", 1);
    UI(p, "VisibilityNodeMask", 1);
    r = (neural && nr_direct ? create_nr : create_feature)(
        g_vk.dev, f->cb, neural ? NVSDK_NGX_Feature_Reserved18 : NVSDK_NGX_Feature_SuperSampling, p, &m->handle);
    if (r != NVSDK_NGX_Result_Success || !m->handle)
        goto fail;
    VkEventCreateInfo event = {.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
    VK_CHECK(vkCreateEvent(g_vk.dev, &event, NULL, &m->ready));
    vkCmdSetEvent(f->cb, m->ready, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    m->w = f->color.width;
    m->h = f->color.height;
    LOG("[ngx] %s feature created (%ux%u -> %ux%u); evaluation after creation retires\n", neural ? "NR" : "SR", m->w,
        m->h, f->output.width, f->output.height);
    if (neural)
        LOG("[ngx] NR controls: intensity %.2f, style %d, structure %.2f, tone %.2f, skin %.2f, auto mask %s; "
            "effect layers %d, one model evaluation per scene frame\n",
            g_graphics.nr_intensity, g_graphics.nr_style, g_graphics.nr_structure, g_graphics.nr_tone, g_graphics.nr_skin,
            g_graphics.nr_auto_mask ? "on" : "off", g_graphics.nr_layers);
    return false;
fail:
    m->failed = true;
    config_graphics_report(neural, r == NVSDK_NGX_Result_FAIL_OutOfDate ? GFX_DRIVER_OLD : GFX_UNAVAILABLE);
    LOG("[ngx] %s feature creation failed: %#x; %s\n", neural ? "NR" : "SR", r,
        neural ? "NR disabled for this session (check native Vulkan / SM89 runtime support)"
               : "DLSS disabled; scaling the scene without temporal reconstruction");
    // An unsuccessful create may have recorded commands; retire its allocations at process exit.
    return false;
}

static bool run(const NgxFrame *f, bool neural) {
    Model *m = neural ? &nr : &sr;
    if (!model_ready(m, f, neural))
        return false;
    NVSDK_NGX_Parameter *p = m->p;
    NVSDK_NGX_Resource_VK color = resource(f->color), depth = resource(f->depth), motion = resource(f->motion),
                          out = resource(f->output);
    if (neural) {
        PTR(p, "DLSSNR.Color", &color);
        PTR(p, "DLSSNR.Depth", &depth);
        PTR(p, "DLSSNR.MVec", &motion);
        PTR(p, "DLSSNR.Output", &out);
        UI(p, "DLSSNR.Enabled", 1);
        UI(p, "DLSSNR.Width", f->color.width);
        UI(p, "DLSSNR.Height", f->color.height);
        UI(p, "DLSSNR.Reset", f->reset || !m->frames);
        UI(p, "DLSSNR.DepthInverted", 0);
        FLT(p, "DLSSNR.MVecScaleX", f->motion_scale[0]);
        FLT(p, "DLSSNR.MVecScaleY", f->motion_scale[1]);
        UI(p, "DLSSNR.ColorSubrectBaseX", 0);
        UI(p, "DLSSNR.ColorSubrectBaseY", 0);
        UI(p, "DLSSNR.ColorSubrectWidth", f->color.width);
        UI(p, "DLSSNR.ColorSubrectHeight", f->color.height);
        UI(p, "DLSSNR.OutputSubrectBaseX", 0);
        UI(p, "DLSSNR.OutputSubrectBaseY", 0);
        UI(p, "DLSSNR.OutputSubrectWidth", f->output.width);
        UI(p, "DLSSNR.OutputSubrectHeight", f->output.height);
        UI(p, "DLSSNR.DepthSubrectBaseX", 0);
        UI(p, "DLSSNR.DepthSubrectBaseY", 0);
        UI(p, "DLSSNR.DepthSubrectWidth", f->depth.width);
        UI(p, "DLSSNR.DepthSubrectHeight", f->depth.height);
        UI(p, "DLSSNR.MVecSubrectBaseX", 0);
        UI(p, "DLSSNR.MVecSubrectBaseY", 0);
        UI(p, "DLSSNR.MVecSubrectWidth", f->motion.width);
        UI(p, "DLSSNR.MVecSubrectHeight", f->motion.height);
        nr_controls(p);
        PTR(p, "DLSSNR.ControlMask", NULL);
    } else {
        PTR(p, "Color", &color);
        PTR(p, "Depth", &depth);
        PTR(p, "MotionVectors", &motion);
        PTR(p, "Output", &out);
        FLT(p, "Jitter.Offset.X", f->jitter[0]);
        FLT(p, "Jitter.Offset.Y", f->jitter[1]);
        FLT(p, "MV.Scale.X", f->motion_scale[0]);
        FLT(p, "MV.Scale.Y", f->motion_scale[1]);
        INT(p, "Reset", f->reset || !m->frames);
        UI(p, "DLSS.Render.Subrect.Dimensions.Width", f->color.width);
        UI(p, "DLSS.Render.Subrect.Dimensions.Height", f->color.height);
        FLT(p, "DLSS.Pre.Exposure", 1.0f);
        FLT(p, "DLSS.Exposure.Scale", 1.0f);
        FLT(p, "FrameTimeDeltaInMsec", f->frame_ms);
        PTR(p, "ExposureTexture", NULL);
        PTR(p, "BiasCurrentColorMask", NULL);
    }
    vk_full_barrier(f->cb);
    NVSDK_NGX_Result r = (neural && nr_direct ? evaluate_nr : evaluate)(f->cb, m->handle, p, NULL);
    vk_full_barrier(f->cb);
    if (r != NVSDK_NGX_Result_Success) {
        m->failed = true;
        config_graphics_report(neural, r == NVSDK_NGX_Result_FAIL_OutOfDate ? GFX_DRIVER_OLD : GFX_UNAVAILABLE);
        LOG("[ngx] %s evaluate failed: %#x; %s\n", neural ? "NR" : "SR", r,
            neural ? "keeping the scene without NR"
                   : "DLSS disabled; scaling the scene without temporal reconstruction");
        return false;
    }
    if (++m->frames == 1) {
        config_graphics_report(neural, neural ? GFX_NR : GFX_DLSS);
        LOG("[ngx] first successful %s evaluation recorded\n", neural ? "NR" : "SR");
    }
    return true;
}

bool vk_ngx_sr(const NgxFrame *f) { return run(f, false); }
void vk_ngx_reset(void) {
    Model *models[] = {&sr, &nr};
    for (int i = 0; i < 2; i++) {
        Model *m = models[i];
        if (m->handle) {
            NVSDK_NGX_Result result = (i && nr_direct ? release_nr : release_feature)(m->handle);
            if (result != NVSDK_NGX_Result_Success) FATAL("NGX feature release failed: %#x\n", result);
        }
        if (m->p) destroy_parameters(m->p);
        if (m->ready) vkDestroyEvent(g_vk.dev, m->ready, NULL);
        bool failed = m->failed && !m->handle;
        *m = (Model){.failed = failed};
    }
}
bool vk_ngx_nr(const NgxFrame *f) { return run(f, true); }
bool vk_ngx_sr_ready(void) { return initialised && !sr.failed; }
bool vk_ngx_nr_ready(void) { return initialised && !nr.failed; }
#endif
