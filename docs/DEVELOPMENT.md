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

Use MSYS2 UCRT64 with Clang, LLD, Make, Python, SDL3, shaderc, Vulkan headers/loader, VulkanMemoryAllocator, zlib and NASM. The default prepared game library requires AVX2/FMA.

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

The current upstream baseline is 0.15.4. The selective sync includes the Forge vertex cursor fix (`06fa605`). For earlier integrations and deferred changes, see the release notes and commit history.

| Item | Format | Example |
| --- | --- | --- |
| Git tag / updater version | Enhanced-edition version | `0.3.0` |
| Release title | Enhanced version + `-` + original-project version | `0.3.0-0.13.0` |
| Windows archive | Fixed filename | `MHGURecomp-Enhancement-win_x86-64.zip` |

Titles contain no prefix, suffix or descriptive words. The startup log reports both versions. Development builds may set `VERSION=0.1.0-dev`; the updater does not automatically replace a nonnumeric development build.

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

The Windows packer uses ZIP Deflate at level 9. It includes the user guides and screenshots, developer and runtime notes, licenses, version files and `release-info.json`. Keep the ZIP filename and format compatible with existing updaters. Verify the archive contents and CRCs before distributing it. The packer writes a separate `.sha256` file.

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

Successful Save publishes a graphics snapshot to the render thread. Changes to upscalers, quality or dimensions retire GPU work and recreate affected resources. Strength and style edits update evaluation parameters and reset history where needed. Display changes apply on the window thread. Changes to the frame generation backend, multiplier, NR runtime or NR model preset require restarting.

### DLSS 5 NR

```ini
[Graphics]
TemporalUpscaling = On
Upscaler = DLSS
UpscaleQuality = Quality
NeuralRendering = Off
NRRuntime = Auto
NRPreset = A
NRIntensity = 0.5
NRStyle = Standard
NRLocalStructure = 1
NRLocalTone = 0
NRSkinStructure = 0
NRAutoMask = On
NRLayers = 1
```

With `NRRuntime = Auto`, the Windows host matches the Vulkan adapter to the CUDA device by LUID. For the bundled runtimes it selects Official on RTX Blackwell (compute capability 12.0) or SM89 on Ada (8.9). If neither applies, F1 disables Neural Rendering and reports GPU unsupported. Explicit `Official` and `SM89` entries take precedence, including entries in older INI files. If Official is rejected by NGX, Neural Rendering stays disabled for the session and the game shows how to try SM89 on the next launch. Official uses public SDK dispatch. SM89 directly invokes the native Vulkan snippet through the adapter, retaining the caller-module name and return address the snippet checks.

Both bundled `310.9.16.0` DLLs contain models A and B. `NRPreset` defaults to `A` even in an older INI without the key. F1 can save a new model choice for the next launch and shows the effective model once evaluation succeeds. The host checks the selected DLL for the `WEIGHTS_CC_SMALL_FP8` resource when B is requested. If it is missing, the host passes preset A to NGX and keeps the saved preference. Reset restores the model choice to A and preserves the offline runtime selection.

NR receives an opaque SDR scene, depth and motion, and runs before the HUD. Detailed strengths/style/mask controls are passed at creation and evaluation. Auto Mask defaults on and Skin structure defaults to zero. Zero requests no skin-structure enhancement, rather than protection from all neural changes. Each setting's range and restart requirement are defined by `g_settings` and exposed as INI comments/F1 help.

Effect layers use the residual-transfer approach documented by [DLSSNR-Cost-Scaler](https://github.com/xenmods/DLSSNR-Cost-Scaler/blob/main/shaders.hlsl).

> In the following formula, $B$ is the opaque SDR input, $N$ is one NR result, $O$ is the final scene and $L$ is an integer effect-strength level from 1 to 5. The residual gain $G=1+0.25(L-1)$ ranges from 1 to 2 in steps of 0.25. The minimum and maximum apply per channel, limiting the output to 0–1.

$$
O = \min(1, \max(0, B + G(N-B))).
$$

Level 1 uses the direct result blit. Levels 2–5 add one compute composition over the existing images, with 1.25x to 2.00x residual gain. All settings issue one model evaluation per scene frame and reuse the same model and history buffers. The next frame uses the unamplified NR history.

The same API-domain residual extrapolation is used by [Overglaze's composition shader](https://github.com/HANsoA-KevinO/overglaze/blob/dd2e1d9925f76c62f9e153230458b62e2225e673/source/native/runtime/include/lab_prepost_ratio_shader.hpp), before its separate HDR ratio transfer. It amplifies every model edit, including softened detail, edge reshaping and local contrast changes. It has no edge or temporal-confidence guard, so increasing layers can produce halos or ghost-like contours even when the single NR result looks acceptable.

Higher residual gains can make contours more pronounced. Level 1 uses the neural output directly.

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

Enable engine modules in `engine_modules/enabled.txt` using one module folder name per line. On Windows, the binary is `engine_modules/<name>/<name>.dll`. See the [example](../extras/example-engine-module) and [host interface](../runtime/engine_module_api.h). The host checks the interface version before loading and calls the shutdown handler after GPU work drains.

`SaveLocation` points to the folder containing `save_0` and applies on restart. `MHGU_SAVE_DIR` takes precedence when set.

F1 has nine pages: Graphics, Audio, Controls, Network, Misc, Mods, Cheats, Menu Options and Item Box. Host setting drafts are separate from saved/live state. Save validates and atomically replaces the INI; restart-only changes remain staged. Live values are published atomically. Unknown INI keys and user comments survive saving, while graphical keys are consolidated into `[Graphics]`. Missing NR controls gain documented defaults. Menu Options selects Gold, Blue or Hunter (default), with a 50–200% scale; dragging the tabs moves the menu, its corner resizes it, and Attach menu restores the attached layout. Ctrl+S or the top controller face button saves host settings; Item Box uses its own explicit Apply quantity action.

Menu language supports the game's seven languages, with Game as the new default. Existing English/Spanish selections keep their meanings. The game thread publishes the actual game language for automatic UI selection; item names always follow that game language. Noto Sans CJK SC supplies missing glyphs alongside a custom Forge font and is included with its OFL license in releases.

The live item box is separate from configuration persistence. It exposes item totals, including zero for valid unowned items, and distributes each applied total over stacks of at most 99 within unlocked capacity. The backend binds edits to the loaded character session and inventory revision, rejects loading/saving or stale edits before writing, and preserves obtained-history flags when a quantity falls to zero. The game's save process persists applied inventory changes. Search accepts current-language names or item IDs; the list is paged to bound layout work.

Keyboard, mouse and controller hints follow active input. Directional/Tab navigation, numeric editing, dropdowns, explicit Save/Reset and dirty-discard behavior share the same row model. The help area stays pinned while long lists scroll. `MenuConfirm` configures the host menu independently of game/Forge mappings.

Mod order is deterministic, with later conflicting entries taking priority. Disabling the Auto prompt pack explicitly is respected. Cheat files in `extras/cheats/` are optional and should not be silently enabled by a release. The positive-only Skill Points x2 variant preserves negative skill points; select the intended variant. The Valor Sheathe cheat changes damage reduction through the game's native tables. Fixed-address changes rely on the supported game compatibility checks.

The Cheats page saves disabled entries in `cheats/_off.txt` and applies them at the next cheat update. Master codes stay enabled. The last cheat to release a shared code word restores its original value. Changes to gameplay data persist.

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

For graphics changes, verify both the saved configuration and the active backend in F1 or the log. On RTX 4080, the 0.3.5 checks exercised default model A, model B, fallback to A with a missing B resource, and a live switch from one to five effect layers. Use the checks above for menu navigation, renderer geometry and package contents.

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
