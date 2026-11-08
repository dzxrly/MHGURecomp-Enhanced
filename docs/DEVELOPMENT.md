# MHGURecomp Enhanced — Development

MHGURecomp Enhanced is an independently versioned Windows edition of [MHGURecomp](https://github.com/morerokk/MHGURecomp), adding native keyboard/mouse input and icon support, DLSS Super Resolution and DLSS 5 Neural Rendering.

## Contents

- [Source layout](#source-layout)
- [Build](#build)
- [Versions and release](#versions-and-release)
- [Runtime architecture](#runtime-architecture)
- [Graphics](#graphics)
- [Native input](#native-input)
- [Settings and mods](#settings-and-mods)
- [Testing](#testing)
- [Hub server](#hub-server)
- [Licenses](#licenses)

## Source layout

| Path | Responsibility |
| --- | --- |
| `runtime/` | Loader, services, interpreter, input, configuration and renderer |
| `runtime/nvn/vulkan/` | Shader translation, Vulkan execution, shared temporal inputs, FSR and NGX |
| `runtime/settings_panel.cpp` | F1 interface and its editing workflow |
| `runtime/config.c`, `runtime/config_store.c` | Schema, defaults, validation, persistence and live values |
| `tools/` | Recompilation, resource preparation, packaging and publication |
| `updater/` | Archive validation, download, installation and recovery |
| `extras/keyboard-mouse/prebuilt/` | Fixed seven-language Auto prompt pack included in releases |
| `extras/cheats/` | Optional MHGU 1.4.0 cheat files |
| `server/` | Hub discovery and relay server |
| `VERSION`, `UPSTREAM_VERSION` | Enhanced-edition version and the upstream stable release corresponding to the selective integration |

## Build

### Windows toolchain

Use MSYS2 UCRT64 with Clang, LLD, Make, Python, SDL3, shaderc, Vulkan headers/loader, VulkanMemoryAllocator, zlib and NASM. The default prepared game library requires AVX2/FMA. The client does not currently have a Linux build/test environment.

```sh
pacman -S --needed make git diffutils xz \
  mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-lld \
  mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-python \
  mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-shaderc \
  mingw-w64-ucrt-x86_64-vulkan-headers mingw-w64-ucrt-x86_64-vulkan-loader \
  mingw-w64-ucrt-x86_64-vulkan-memory-allocator mingw-w64-ucrt-x86_64-nasm \
  mingw-w64-ucrt-x86_64-zlib
```

Clone this repository and initialize its dependencies:

```sh
git clone --recursive https://github.com/dzxrly/MHGURecomp-Enhanced.git
cd MHGURecomp-Enhanced
```

The optional NGX adapter uses Visual Studio C++ x64 tools. Build it from PowerShell:

```powershell
./tools/build_ngx.ps1 -VulkanInclude 'C:/path/to/Vulkan-Headers/include'
```

It downloads the pinned public SDK library when absent and builds `build/graphics/nvidia/ngx/mhgu-nvngx.dll`. The public SDK headers and license are under `third_party/ngx/`. Runtime binary setup and source references are in [NGX integration](../third_party/ngx/README.md).

### Host and game code

The host and updater can be built independently of a game dump:

```sh
make -j8 build/mhgurecomp.exe build/mhgurecomp-updater.exe pipelist licenses
```

A matching prepared `mhgugame.dll` is required to play. Check its interface without loading a game:

```sh
build/mhgurecomp.exe --check-gamelib
```

To regenerate game code for development, use the owner's complete 1.4.0 dump as a read-only input:

```sh
make gen GAME=/c/path/to/own-dump
make -j8
```

Generated game sources, dumps, saves and extracted resources remain outside Git. Base `exefs` and `romfs`, including hard-linked build copies, must remain unchanged. Mods are overlays.

The default build compiles a minimal pinned FFmpeg 9.0.2 with MP4/MOV demuxing, H.264 and AAC decoding, no networking and no GPL components. `FFMPEG=system` is available for development; a release must carry notices matching the libraries actually linked. The Vulkan loader comes from the graphics driver and is not bundled.

## Versions and release

### Version contract

`VERSION` contains the enhanced-edition semantic version, starting at `0.1.0`. `UPSTREAM_VERSION` identifies the stable MHGURecomp release corresponding to the selective integration. When a maintenance release includes validated upstream changes and the review covers a newer stable release, use that upstream release number. Record the actual adopted commits and deferred changes in the release notes. The prepared game library has its own compatibility check.

The selective 0.12.0 sync adopts archive decompression and interpreter optimizations, Forge font/instruction fixes, Intel front-face translation, menu/cutscene/reward-picture anti-aliasing, FSR 4 fallback reasons, settings themes/dragging/resizing/quick save, and live cheat switches including shared-code restoration. Sources are [the 0.12.0 history](https://github.com/morerokk/MHGURecomp/compare/0.11.7...0.12.0), including `e5bb73e`, `0ad44dd`, `8bf48f9`, `4d35207`, `4f6b2fd`, `8c2ff2b`, `7bf00c0`, `504b9ed`, `192b435` and PRs #114/#134/#143/#144. Original-input rebinding, the alternative mouse camera/item bar, Discord and Linux build changes are excluded. Native input, the prepared game-library interface, independent updater and graphics hot switching remain this edition's implementations.

The selective 0.13.0 sync adds shared vertex-pool copies (`0633029`, `69b47f2`), menu-preview anti-aliasing optimizations (`3ec8491`), whole-window ultrawide anchoring and Character Select backdrop tracking (`1e502fe`, `83f148f`), rounded-aspect FOV fixes (`4ac1916`, `ace6ed8`), complete Forge point-array reads (`5bc3695`), and window/menu fixes (`11cea9d`, `b04067f`, `dfe556b`). The preview optimizations retain this edition's output-size mapping and graphics-reset handling. Android, Discord, new cheat-entry/FPS-counter features and the later removal of FSR 4 are not included.

The selective 0.15.0 sync ports Windows thread-stack allocation, controller rumble, FXAA and render-thread optimizations, FSR 3.1 history sampling, AMD depth bias, Forge crash fixes and the mod interpreter. It also adds partial archives, high-frame-rate monster fixes, shared urgent credit, Village quest maps, settings changes that apply during play, startup progress, a save-folder picker, and Windows executable icons. The stall-timer fix changes generated guest code. Generate and build a matching game library for this baseline.

An engine module is a native library enabled explicitly in `engine_modules/enabled.txt`. List the module folder name on its own line and put the library at `engine_modules/<name>/<name>.dll` on Windows. The example and `runtime/engine_module_api.h` show the loader interface. The host validates the interface version before calling the entry point and calls the registered shutdown function after GPU work drains. `make test-engine-modules` exercises successful loading and rejection cases. Engine modules run native code in the game process.

The `SaveLocation` setting takes effect at restart and points to the folder containing `save_0`. The explicit `MHGU_SAVE_DIR` environment setting takes precedence. The folder picker updates the settings draft. Save it before restarting.

The 0.2.1 candidate also fixes the expanded item/ammo selector on layer 103: its upper rows and R prompt share the lower selector's right/bottom anchor, while the status bars stay left/top. Checked with an isolated save at the exact 3440:1440 picture ratio, including DLSS Quality and NR intensity 1.00. The local 2560x1440 desktop required a fitted window for the ultrawide case; the physical ultrawide borderless setup is not locally validated. Damage Numbers r36 is unchanged, and this candidate contains no new damage-number-specific fix.

Version 0.2.2 additionally maps Forge's game-world drawings from the guest's boot render canvas to the window's fitted game picture. During that drawing callback, plugin input/display state and viewport dimensions use the same guest canvas; afterward only newly added vertices and draw-command clips are transformed. Shared commands are split at the prior index boundary. Mouse coordinates, fonts and viewport state are restored before host notices or other UI draw. This fixes r36's displaced, undersized numbers with temporal upscaling without modifying the plugin or the game's shared viewport data. F1 and the plugins' settings remain in window pixels.

Version 0.3.0 adds the live item-box editor and seven-language F1 menu described below. It also keeps the whole top status strip on layer 103 left/top anchored: HP and stamina end caps can pass x 800 as the bars grow, so the selector's horizontal cutoff must not apply to that strip. Expanded item/ammo rows below it remain right/bottom anchored.

| Item | Format | Example |
| --- | --- | --- |
| Git tag / updater version | Enhanced-edition version | `0.3.0` |
| Release title | Enhanced version + `-` + original-project version | `0.3.0-0.13.0` |
| Windows archive | Fixed filename | `MHGURecomp-Enhancement-win_x86-64.zip` |

Titles contain no prefix, suffix or descriptive words. The startup log reports both versions. Development builds may set `VERSION=0.1.0-dev`; the updater does not automatically replace a nonnumeric development build.

Version 0.3.3 selectively adopts upstream `06fa605` (PR #310): Forge's moving vertex-buffer write cursor is data, not a persistent ImGui object. This prevents cursor movement from consuming object-mirror slots. Enhanced's direct vertex editing and fitted-picture coordinate mapping remain in use. The Forge vertex regression now checks real cursor writes without mirror growth, and the same test target checks the generator's cursor and shared-state classifications.

The 2026-11-09 maintenance review covered upstream `81d5a93..51373a5`, including stable releases through 0.15.4 and the 0.15.5 prerelease. Version 0.3.4 corrects the metadata for this integration to 0.15.4, whose stable tag contains `06fa605`. CPU-feature exports, preview-camera and three-button changes require a new prepared game library and are deferred. HDR needs validation with Enhanced's SDR neural-rendering path. Crossplay, direct UDP and server changes need multi-client network verification. Other 16:10 and shader fixes need targeted display and driver checks. Parallel archive checks depend on loading progress and ordered overlay tests. Part-HP, Village-demo and fast-transition hooks need gameplay and network fixtures. The upstream mouse/keybind stack and platform-specific changes remain outside this integration.

### Prepare and publish

Releases are built locally for Windows. Publication consumes a complete, checked archive; it does not run game-code generation or create prompt resources from a dump.

1. Update `VERSION` and set `UPSTREAM_VERSION` to the corresponding reviewed upstream stable release for the selected changes.
2. Build the host, updater and NGX adapter. Supply a matching prepared game library; check it with `--check-gamelib`.
3. Use a clean release folder containing the programs, actual dependency DLLs, `pipelines.list`, licenses and the complete [graphics runtime layout](#graphics-runtime-layout). The packer stops if any required file is missing.
4. Run the focused checks below and pack the folder. The checked-in Auto prompt pack is the release input; `BONUS` is empty unless a donor has explicitly been approved for distribution.

```sh
python tools/dist.py build-release dist windows "$(cat VERSION)" \
  --keyboard-mouse-mod extras/keyboard-mouse/prebuilt/Keyboard_Mouse_Input_Auto
```

The packer uses the maximum compression level, 9: Deflate for the Windows ZIP and gzip for the Linux archive. Keep the Windows ZIP filename and Deflate format compatible with already released updaters. It adds the three user guides, this developer guide, runtime setup notes, legal notices, version files and `release-info.json`. It excludes the build's game data, settings, saves, unrelated mods, caches and logs. It writes one `.sha256` alongside the archive. Verify archive entries and CRCs before upload.

Commit the prepared source, create the numeric version tag at that commit and push the branch/tag to `origin`. Write the release notes in English and supply the file to the publisher:

```powershell
./tools/release.ps1
./tools/release.ps1 -Publish -NotesFile '.agents/release/notes.md'
```

Without `-Publish`, it shows the resolved title and archive. Publication requires a clean worktree and a version tag at HEAD. It uploads a draft, checks the uploaded ZIP's size and GitHub SHA-256 against the prepared checksum, then publishes it as Latest with the exact numeric title. It uses the repository selected by `origin` and the user's authenticated GitHub CLI.

The updater queries this repository and compares enhanced-edition tags. It preserves game data, saves, configuration and other mods; the bundled Auto prompt `romfs/` and DLSS runtime paths are release-owned. Files it replaces are backed up under `update/backup/`. Installing a release must not write to `%APPDATA%` saves.

## Runtime architecture

The loader checks the complete executable Build IDs and selected base-resource fingerprints/file count before installing fixed-address hooks. These checks identify the supported game layout; they do not prove whole-dump integrity. `--check-rom <directory>` performs the compatibility check without entering gameplay.

The prepared game library supplies compiled ARM functions. Unsupported or mod-added executable code uses the interpreter. Kernel, IPC, service, audio and filesystem implementations provide the platform services the game expects. The NVN backend translates shaders and rendering state to Vulkan on one instance/device.

> A subsdk is an additional executable module provided by a mod as `exefs/subsdk0` through `exefs/subsdk9`.

Executable patches, subsdk modules, RomFS replacement, merged ARC resources and Forge plugins are supported. The runtime loads Forge plugins itself and skips Forge's duplicate subsdk loader. Input and graphics hooks must be evaluated against the supported game layout, not merely a matching file name.

## Graphics

### Configuration and frame flow

All graphical settings are in `[Graphics]`. F1's Graphics page exposes the graphical controls except `NRRuntime`, which is an offline INI-only setting. Audio is separate. The unified anti-aliasing/upscaling selector maps to the existing `MSAA`, `FXAA`, `TemporalUpscaling` and `Upscaler` keys and displays only applicable parameters. `NeuralRendering` is independent. Existing accepted key aliases remain parser details; examples use the canonical schema.

The renderer separates the scene from the HUD. Shared preparation under `runtime/nvn/vulkan/temporal/` supplies standard-Z R32F depth and unjittered camera/object motion in render pixels. Temporal reconstruction uses jitter while a valid scene is active. NR alone and frame generation alone use no projection jitter. Scene changes, camera cuts and gaps reset temporal history. MSAA is disabled while temporal input preparation is active. FXAA is disabled when temporal reconstruction owns anti-aliasing.

### Graphics runtime layout

| Directory relative to the executable | Contents |
| --- | --- |
| `graphics/amd/fsr/` | Vulkan loader, upscaler and frame generation DLLs from [AMD FSR SDK 2.4.0](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v2.4.0). |
| `graphics/intel/xess/` | `libxess.dll` from [Intel XeSS SDK 3.0.2](https://github.com/intel/xess/releases/tag/v3.0.2). The public headers come from Intel's [MIT-licensed header update](https://github.com/intel/xess/commit/de0fb9c1c510661c571164e1418ceca8101dab69). |
| `graphics/nvidia/streamline/` | Production x64 [NVIDIA Streamline 2.14.16](https://github.com/NVIDIAGameWorks/Streamline/releases/tag/v2.14.16) core, interposer, DLSS SR, DLSS FG, Reflex and PC Latency plugins, plus their runtime DLLs. |
| `graphics/nvidia/ngx/` | The project's NGX adapter and the separate `nr/official/` and `nr/sm89/` runtime directories. |

See `GRAPHICS_RUNTIME_GROUPS` in [the packer](../tools/dist.py) for the exact file list. Stage these DLLs locally. The source repository carries the SDK headers and license files. The new layout takes precedence over the legacy NGX and FSR paths when present. Existing installations can retain their old files after an updater run. Fresh archives contain the new paths only.

### Vulkan frame generation

`FrameGeneration` and `DLSSGeneratedFrames` take effect after restarting. The latter counts generated frames per real frame, so a value of 1 selects 2×. The `FPS` setting limits real game frames before interpolation. The renderer uses one swapchain backend at a time and pauses generation without a valid scene or when the scene and swapchain sizes differ. NVIDIA's Vulkan integration uses immediate presentation and exposes fixed multipliers up to the reported device limit. This Streamline version exposes Dynamic Multi Frame Generation through Direct3D 12. The new SDK integrations in this project use Windows Vulkan runtimes.

### FSR

FSR 3.1 uses the vendored FidelityFX Vulkan implementation, runtime-compiled shader permutations and the common scene/depth/motion inputs. Object velocities, reactive masks and animated-surface handling reduce history errors. FSR 4 uses a compatible AMD driver provider when enabled by the build; failure selects FSR 3.1 and the menu reports the actual backend. AMD driver binaries are not distributed.

`FSR4_DRIVER_PATCH=1` enables the optional driver-provider route. It is not an additional Vulkan device or graphics-API bridge. Its hardware/driver availability is distinct from FSR 3.1.

### DLSS SR and DLAA

`temporal/nvidia/streamline.cpp` integrates DLSS SR and DLAA on the existing Vulkan device. It loads the SR feature even with DLSS frame generation off so F1 can switch upscalers during play. When DLSS frame generation is selected, the same initialization loads frame generation and Reflex. `temporal/nvidia/ngx.c` retains the direct NGX path for NR and for SR when the AMD frame-generation proxy owns presentation or Streamline is unavailable.

The five selectable qualities map to the NVIDIA performance-quality enums: Ultra Performance, Performance, Balanced, Quality and DLAA. The SDK chooses host render dimensions. Guest dimensions stay fixed for the process; host screen targets, viewport/scissor and fragment coordinates adapt to live size changes. NVIDIA App overrides may change the selected model/dimensions. F1 shows actual backend and dimensions, not only the requested provider. DLSS failure is explicit and never silently invokes FSR.

Successful Save publishes a graphics snapshot to the render thread. Changes to upscalers, quality or dimensions retire GPU work and recreate affected resources. Strength and style edits update evaluation parameters and reset history where needed. Display changes apply on the window thread. Frame generation backend and multiplier changes and the offline NR runtime selection require restarting.

### DLSS 5 NR

```ini
[Graphics]
TemporalUpscaling = On
Upscaler = DLSS
UpscaleQuality = Quality
NeuralRendering = Off
NRRuntime = Auto
NRIntensity = 0.5
NRStyle = Standard
NRLocalStructure = 1
NRLocalTone = 0
NRSkinStructure = 0
NRAutoMask = On
NRLayers = 1
```

With `NRRuntime = Auto`, the Windows host matches the Vulkan adapter to the CUDA device by LUID. For the bundled runtimes it selects Official on RTX Blackwell (compute capability 12.0) or SM89 on Ada (8.9). If neither applies, F1 disables Neural Rendering and reports GPU unsupported. Explicit `Official` and `SM89` entries take precedence, including entries in older INI files. F1 Reset and Save preserve the entry. If Official is rejected by NGX, Neural Rendering stays disabled for the session and the game shows how to try SM89 on the next launch. Official uses public SDK dispatch. SM89 directly invokes the native Vulkan snippet through the adapter with the original FP16 model, retaining the caller-module name and return address the snippet checks.

NR receives an opaque SDR scene, depth and motion, and runs before the HUD. Detailed strengths/style/mask controls are passed at creation and evaluation. Auto Mask defaults on and Skin structure defaults to zero. Zero requests no skin-structure enhancement, rather than protection from all neural changes. Each setting's range and restart requirement are defined by `g_settings` and exposed as INI comments/F1 help.

Effect layers use the residual-transfer approach documented by [DLSSNR-Cost-Scaler](https://github.com/xenmods/DLSSNR-Cost-Scaler/blob/main/shaders.hlsl).

> In the following formula, $B$ is the opaque SDR input, $N$ is one NR result, $O$ is the final scene and $L$ is an integer effect-strength level from 1 to 3. The residual gain $G=1+0.25(L-1)$ is 1, 1.25 or 1.5. The minimum and maximum apply per channel, limiting the output to 0–1.

$$
O = \min(1, \max(0, B + G(N-B))).
$$

Level 1 uses the direct result blit. Levels 2/3 add one small compute composition over the existing images, with 1.25x/1.5x residual gain. All settings still issue one model evaluation per scene frame, with no additional model/history allocation. The amplified scene is not fed back into NR history. This approximates effect strength; it is not sequential neural inference.

The same API-domain residual extrapolation is used by [Overglaze's composition shader](https://github.com/HANsoA-KevinO/overglaze/blob/dd2e1d9925f76c62f9e153230458b62e2225e673/source/native/runtime/include/lab_prepost_ratio_shader.hpp), before its separate HDR ratio transfer. It amplifies every model edit, including softened detail, edge reshaping and local contrast changes. It has no edge or temporal-confidence guard, so increasing layers can produce halos or ghost-like contours even when the single NR result looks acceptable.

In 1080p RTX 4080 / SM89 tests, same-frame GPU readbacks confirmed unchanged NR input and exact 8-bit agreement with the original 3x residual formula, with NR alone, FSR 3.1 and DLSS SR. Zero model strength/structure produced an unchanged scene. Stronger contours also remained with NR history reset on every frame and with FP16 NR input/output buffers. These samples identify residual extrapolation as the direct amplifier; input overwrite, composition misalignment, accumulated composed history and 8-bit precision alone do not explain the reproduced contours. They do not establish the absence of all temporal artifacts in other scenes or the Official runtime. The adopted 1x/1.25x/1.5x mapping reproduced its intended formula within 8-bit rounding in six GPU capture sets, reducing overdriven contours while retaining some extra contrast. It attenuates all edits uniformly and does not provide edge-specific protection; level 1 remains the conservative setting.

`MHGU_FSR_LOG` enables development statistics/readback work and is unset for normal runs. Temporary timing wrappers are excluded from release executables. Use isolated diagnostics rather than leaving them enabled during gameplay.

## Native input

On Windows, startup offers Controller or Keyboard/Mouse before creating the game window. Start saves `InputMode` and the inverse of the "Don't ask again" checkbox as `[Controls] AskInputOnStartup`; Cancel does not start the game. Automated control/hidden runs skip this chooser. `InputMode = Keyboard_Mouse_Input` activates the native hooks. The local character's actual equipment selects Ranged for Light Bowgun, Heavy Bowgun and Bow, and Melee for other Hunter weapons/Prowler. Changing equipment releases held inputs and queued wheel steps. Controllers remain available.

Relative mouse counts expire after 100 ms and are consumed during valid game input stages. Menus, focus, capture and original action gates discard blocked motion. Ordinary camera changes occur before recenter, pitch and terrain logic. Bowgun/Bow aiming updates authoritative aim before camera synchronization; original aiming delays, recovery, pitch bounds and action constraints remain active. Ordinary movement with a ranged weapon leaves mouse look available unless the game's aim checks are actually active.

While moving in a following Quick Aim camera, horizontal mouse input starts from the camera's current target yaw. The original movement code also writes the player's aim yaw; reusing that movement direction for each mouse update would feed it back into the camera and cause repeated large turns. Scope input and Bow's independent Quick Aim Camera Type 2 retain their original reference directions.

Bow mouse buttons follow the selected Bow control type and R/ZR swap. Village cameras, fixed Ballista and Prowler special aiming use the stick-compatible path. Their turn limits remain in effect.

`MouseSensitivity` is 0.1–10. Direct input uses relative counts, with no extra acceleration curve. Camera Speed and Reticle Speed apply independently: Slow 0.5×, Default 1×, Fast 1.5×. At sensitivity 1, 1,000 horizontal counts correspond to approximately 20°/40°/60°. Camera, Quick Aim and Scope inversion remain independent.

Only the stick-compatible path uses these restart-only settings:

```ini
[MouseConversion]
MinWindowMs = 8
FullStickSpeed = 1500
DeadzoneCompensation = 0.40
AimDeadzoneCompensation = 0.50
LowSpeedCutoff = 0
FrameTimeSmoothing = 0.25
```

The Auto prompt pack carries Melee/Ranged text and atlas variants for seven game languages. Runtime message selection and Vulkan texture uploads follow the active equipment preset. The fixed pack is under `extras/keyboard-mouse/prebuilt/Keyboard_Mouse_Input_Auto`; release packaging reads it directly. Resource-generation tools and artwork prompts are maintainer inputs, not player installation requirements.

## Settings and mods

F1 has nine pages: Graphics, Audio, Controls, Network, Misc, Mods, Cheats, Menu Options and Item Box. Host setting drafts are separate from saved/live state. Save validates and atomically replaces the INI; restart-only changes remain staged. Live values are published atomically. Unknown INI keys and user comments survive saving, while graphical keys are consolidated into `[Graphics]`. Missing NR controls gain documented defaults. Menu Options selects Gold, Blue or Hunter (default), with a 50–200% scale; dragging the tabs moves the menu, its corner resizes it, and Attach menu restores the attached layout. Ctrl+S or the top controller face button saves host settings; Item Box uses its own explicit Apply quantity action.

Menu language supports the game's seven languages, with Game as the new default. Existing English/Spanish selections keep their meanings. The game thread publishes the actual game language for automatic UI selection; item names always follow that game language. Noto Sans CJK SC supplies missing glyphs alongside a custom Forge font and is included with its OFL license in releases.

The live item box is separate from configuration persistence. It exposes item totals, including zero for valid unowned items, and distributes each applied total over stacks of at most 99 within unlocked capacity. The backend binds edits to the loaded character session and inventory revision, rejects loading/saving or stale edits before writing, and preserves obtained-history flags when a quantity falls to zero. The game's save process persists applied inventory changes. Search accepts current-language names or item IDs; the list is paged to bound layout work.

Keyboard, mouse and controller hints follow active input. Directional/Tab navigation, numeric editing, dropdowns, explicit Save/Reset and dirty-discard behavior share the same row model. The help area stays pinned while long lists scroll. `MenuConfirm` configures the host menu independently of game/Forge mappings.

Mod order is deterministic, with later conflicting entries taking priority. Disabling the Auto prompt pack explicitly is respected. Cheat files in `extras/cheats/` are optional and should not be silently enabled by a release. The positive-only Skill Points x2 variant preserves negative skill points; select the intended variant. The Valor Sheathe cheat changes damage reduction through the game's native tables. Fixed-address changes rely on the supported game compatibility checks.

The Cheats page saves disabled entries in `cheats/_off.txt` and applies them at the next cheat update. Master codes remain enabled. Code words shared by several cheats retain one original value and are restored after the last writer is disabled; already changed gameplay data is not rolled back. Reward screenshots take temporal anti-aliasing output before Neural Rendering, preserving the game's screenshot colours. Live graphics rebuilds discard menu anti-aliasing captures, screenshot images and previous menu-camera draw counts. FSR 4 fallback reasons use atomic status alongside the actual backend, rather than startup-only configuration. Before temporal output is created, its source is narrowed from Smooth Gradients to RGBA8; stand-in caches also check the actual Vulkan format. This keeps an MSAA-to-FSR switch during character selection from retaining an RGBA16 storage output.

## Testing

Run the checks corresponding to the changed areas:

```sh
make test-settings test-settings-panel test-settings-locale test-item-box test-cheats test-interpreter
make test-camera-aspect test-hud-layout test-forge-draw-vertices
make test-forge-canvas test-forge-overlay
make test-keyboard-mouse test-keyboard-mouse-assets
make test-rom-version test-release-packaging
```

Settings and packaging fixtures do not need a ROM. The latter checks prompt allowlisting and exclusion of private game data/mods, release metadata and checksums. `--check-gamelib` checks the prepared library independently; `--check-rom` checks the owner's supported dump. The standalone updater ownership check is `tests/test_update_paths.c` (compile it with Clang and run the resulting host executable).

Runtime tests use isolated configuration, save and mod directories, and copies of player saves. They must not write experimental equipment/save changes back to the original save or mutate base game resources. `MHGU_CONTROL` accepts scripted input and frame/screenshot requests; `stopinput` clears held test inputs. Diagnostics such as native-mouse traces are explicit opt-ins.

The 0.12.0 integration was checked with host settings/menu/cheat/interpreter regressions and isolated Windows Vulkan runs: menu MSAA/FSR, FSR 4 fallback, resolution/backend hot switching and DLSS SR with NR. The menu-to-town MSAA-to-FSR transition was reproduced and checked after the RGBA8 output fix. Task-completion screenshots, Intel Gen9 hardware and real CJK Forge plugins have not been revalidated locally.

Current validation scope:

| Area | Evidence and bounds |
| --- | --- |
| Hunter camera | All 14 weapon classes, six standing/movement states; 84 checks of accepted mouse counts and actual camera angles |
| Ranged controls | All three ranged classes, Quick Aim/scope/projectiles, both Bow control types and movement-camera behavior |
| Prowler | Standing/four-way movement camera, charged-boomerang precision aim, exit and selected guard paths |
| Selected actions | Insect Glaive aim/Kinsect launch, fixed Ballista aim, Long Sword movement/draw, Lance thrust/guard, Charge Blade guard, Great Sword/Sword and Shield selected mouse actions |
| Temporal graphics | RTX 4080 / driver 610.88: DLAA at 1280×720, SR at SDK-selected 742×418 → 1280×720, native NR and SR+NR with pinned SM89 SF 310.9.16 |
| NR layers | 600 successful three-layer scene frames each recorded one NR call; sampled interval p99 about 17.6 ms and maximum about 19.7 ms at a 60 FPS cap |
| Ultrawide | Same-camera 16:9/21:9/3440x1440 town captures retain the central scene scale and extend the sides; repeated live ratio switches from an Edges ultrawide boot checked, plus HUD Center/Edges and DLSS SR with SM89 NR. Rendered at up to 3440x1440 in a 1720x720 window; no physical ultrawide monitor test |
| Ultrawide movies | The 960x540, 16:9 opening movie (`movie06.mp4`) plays as 1280x720 with 220-pixel black bars on each side in a 1720x720 window. The realtime village meal sequence also played in the wide scene; other movies/event sequences were not individually checked |
| Forge Damage Numbers r36 | The unchanged supplied plugin was loaded on the published 0.1.5 and reproduced null-page accesses when editing text vertices. The host DrawList vertex mirror fixes those accesses; real Heavy Bowgun hits displayed Wilds-style numbers at 16:9 and the exact 3440x1440 aspect. The user also confirmed positions by manual observation. Other styles, damage sources and mismatched window/picture ratios were not exhaustively tested |
| Forge overlay canvas | 0.2.2 checks real draw-list command splitting, existing host geometry, input/font/viewport restoration and fitted-picture offsets in headless integration tests. An unchanged r36 plugin displayed numbers at the hit location after a 3440x1440 DLSS Quality + SM89 NR boot (NR intensity 1), presented in a fitted 2484x1039 window. The prior DLSS/NR displacement was reproduced first; physical ultrawide borderless hardware is not available locally |
| HP/stamina end caps | 0.3.0 regression tests exercise the production anchor classifier across the former x 800 cutoff, including long bars and expanded ammo rows. In-game checks cover ordinary-length bars at 43:18, 16:9 and 16:10, Center/Edges and expanded ammo selection. The user subsequently confirmed the fix on their reported setup |

These are exercised states, not all moves/styles/ammo combinations. Selected FPS settings do not prove every delivered frame rate or physical polling rate. Guard-pose tests do not establish damage blocking. Bow-specific prompts/handbook art, actual Prowler support-move activation, variable scope zoom, broad multiplayer contexts, RTX 20/30/50 and long-session NR quality remain open validation areas.

The moving Quick Aim regression was reproduced in a borderless window and checked with 48 automated in-game angle scenarios at a 60 FPS cap: all three ranged classes across six movement directions with stationary or moving mouse input, vertical/diagonal input, Bow's second control type and independent camera, and the installed program in windowed mode with Quick Aim Camera Type 5. Scope shots and arrows from both Bow control types were also exercised. The angle checks sampled 8,663 frames; measured horizontal turns matched accepted mouse counts to within one game angle unit (360/65,536 degrees).

## Hub server

The optional server provides discovery and relay for compatible clients. LAN play can use direct discovery; direct hosts need a reachable configured TCP/UDP port (default 39500). Internet clients configure the same `[Multiplayer] Server` and optional `Key`.

```sh
cd server
go build -o mhgurecomp-hubserver .
go test ./...
./mhgurecomp-hubserver -addr :39510
```

Go 1.22 or newer is required. The server can run on Linux independently of the Windows client. `GOOS=linux GOARCH=amd64 go build` cross-builds it. A Dockerfile is provided in `server/`; expose TCP and UDP on the chosen port. Public-server availability is separate from client compatibility.

The server pairs compatible protocol versions and matchmaking-key hashes, then relays the games' own link traffic. Its controls include packet/JSON bounds, rate/connection limits, timeouts and destination restrictions. Protocol details are in `server/server.go`.

## Licenses

Retain the [project license](../LICENSE), [third-party notices](../THIRD_PARTY_NOTICES), dependency license files and source-offer records in the distribution. Vendored dependency provenance/license records describe their sources and must remain intact. NGX components and NR model binaries retain NVIDIA's separate terms and source notices; the community SM89 binary is identified separately from the unmodified official runtime. AMD driver binaries are not distributed. The project license does not cover Nintendo/Capcom game data or generated game code.
