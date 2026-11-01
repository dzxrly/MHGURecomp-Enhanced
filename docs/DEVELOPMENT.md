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
| `VERSION`, `UPSTREAM_VERSION` | Enhanced-edition version and reviewed original-project baseline |

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

It downloads the pinned public SDK library when absent and builds `build/mhgu-nvngx.dll`. The public SDK headers and license are under `third_party/ngx/`. Runtime binary setup and source references are in [NGX integration](../third_party/ngx/README.md).

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

`VERSION` contains the enhanced-edition semantic version, starting at `0.1.0`. `UPSTREAM_VERSION` identifies the reviewed MHGURecomp baseline, currently `0.12.0`; it is separate from the prepared game-code ABI and is changed when a new baseline is adopted.

The selective 0.12.0 sync adopts archive decompression and interpreter optimizations, Forge font/instruction fixes, Intel front-face translation, menu/cutscene/reward-picture anti-aliasing, FSR 4 fallback reasons, settings themes/dragging/resizing/quick save, and live cheat switches including shared-code restoration. Sources are [the 0.12.0 history](https://github.com/morerokk/MHGURecomp/compare/0.11.7...0.12.0), including `e5bb73e`, `0ad44dd`, `8bf48f9`, `4d35207`, `4f6b2fd`, `8c2ff2b`, `7bf00c0`, `504b9ed`, `192b435` and PRs #114/#134/#143/#144. Original-input rebinding, the alternative mouse camera/item bar, Discord and Linux build changes are excluded. Native input, the prepared game-library interface, independent updater and graphics hot switching remain this edition's implementations.

| Item | Format | Example |
| --- | --- | --- |
| Git tag / updater version | Enhanced-edition version | `0.2.0` |
| Release title | Enhanced version + `-` + original-project version | `0.2.0-0.12.0` |
| Windows archive | Fixed filename | `MHGURecomp-Enhancement-win_x86-64.zip` |

Titles contain no prefix, suffix or descriptive words. The startup log reports both versions. Development builds may set `VERSION=0.1.0-dev`; the updater does not automatically replace a nonnumeric development build.

### Prepare and publish

Releases are built locally for Windows. Publication consumes a complete, checked archive; it does not run game-code generation or create prompt resources from a dump.

1. Update `VERSION` and review `UPSTREAM_VERSION`.
2. Build the host, updater and NGX adapter. Supply a matching prepared game library; check it with `--check-gamelib`.
3. Use a clean release folder containing the programs, actual dependency DLLs, `pipelines.list`, licenses and all four files listed in [NGX runtime layout](../third_party/ngx/README.md#bundled-runtimes). The packer requires the NGX adapter, official SR runtime, official NR runtime and pinned community SM89 NR runtime; it stops if any is missing and includes only the two declared NR paths.
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

The renderer separates the scene from the HUD. Shared preparation supplies standard-Z R32F depth and unjittered camera/object motion in render pixels. Temporal reconstruction uses jitter while a valid scene is active. NR alone does not add projection jitter. Scene changes, camera cuts and gaps reset temporal history. MSAA is disabled while temporal input preparation is active; FXAA is disabled when temporal reconstruction owns anti-aliasing.

### FSR

FSR 3.1 uses the vendored FidelityFX Vulkan implementation, runtime-compiled shader permutations and the common scene/depth/motion inputs. Object velocities, reactive masks and animated-surface handling reduce history errors. FSR 4 uses a compatible AMD driver provider when enabled by the build; failure selects FSR 3.1 and the menu reports the actual backend. AMD driver binaries are not distributed.

`FSR4_DRIVER_PATCH=1` enables the optional driver-provider route. It is not an additional Vulkan device or graphics-API bridge. Its hardware/driver availability is distinct from FSR 3.1.

### DLSS SR and DLAA

`ngx.c` controls resources and NGX state; `ngx_sdk.cpp` isolates the official SDK's Microsoft C++ ABI behind a C interface compiled into `mhgu-nvngx.dll`. It reuses the existing Vulkan instance, device and command buffers.

The five selectable qualities map to the NVIDIA performance-quality enums: Ultra Performance, Performance, Balanced, Quality and DLAA. The SDK chooses host render dimensions. Guest dimensions stay fixed for the process; host screen targets, viewport/scissor and fragment coordinates adapt to live size changes. NVIDIA App overrides may change the selected model/dimensions. F1 shows actual backend and dimensions, not only the requested provider. DLSS failure is explicit and never silently invokes FSR.

Creation uploads retire before the first evaluation, using a GPU event instead of a per-frame CPU device-idle wait. Successful Save publishes a graphics snapshot to the render thread. Changes to providers, quality or dimensions retire GPU work and recreate affected resources; strength/style edits update evaluation parameters and reset history where needed. Display changes apply on the window thread. Only the offline NR runtime selection requires restarting.

### DLSS 5 NR

```ini
[Graphics]
TemporalUpscaling = On
Upscaler = DLSS
UpscaleQuality = Quality
NeuralRendering = Off
NRRuntime = Official
NRIntensity = 0.5
NRStyle = Standard
NRLocalStructure = 1
NRLocalTone = 0
NRSkinStructure = 0
NRAutoMask = On
NRLayers = 1
```

Official is the default RTX 50 runtime path. Both runtime files ship in one ZIP. Players select SM89 explicitly by editing `[Graphics] NRRuntime` with the game closed; F1 has no runtime selector and its Reset/Save actions preserve the value. Neither GPU detection nor failure switches runtime. The original FP16 model is selected for the pinned compatibility runtime. Official uses public SDK dispatch. SM89 directly invokes the native Vulkan snippet through the adapter, retaining the caller-module name and return address the snippet checks. The host executable is not renamed and the driver is not modified.

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

F1 has eight pages: Graphics, Audio, Controls, Network, Misc, Mods, Cheats and Menu Options. Draft edits are separate from saved/live state. Save validates and atomically replaces the INI; restart-only changes remain staged. Live values are published atomically. Unknown INI keys and user comments survive saving, while graphical keys are consolidated into `[Graphics]`. Missing NR controls gain documented defaults. Menu Options selects Gold, Blue or Hunter (default), with a 50–200% scale; dragging the tabs moves the menu, its corner resizes it, and Attach menu restores the attached layout. Ctrl+S or the top controller face button saves from any tab.

Keyboard, mouse and controller hints follow active input. Directional/Tab navigation, numeric editing, dropdowns, explicit Save/Reset and dirty-discard behavior share the same row model. The help area stays pinned while long lists scroll. `MenuConfirm` configures the host menu independently of game/Forge mappings.

Mod order is deterministic, with later conflicting entries taking priority. Disabling the Auto prompt pack explicitly is respected. Cheat files in `extras/cheats/` are optional and should not be silently enabled by a release. The positive-only Skill Points x2 variant preserves negative skill points; select the intended variant. The Valor Sheathe cheat changes damage reduction through the game's native tables. Fixed-address changes rely on the supported game compatibility checks.

The Cheats page saves disabled entries in `cheats/_off.txt` and applies them at the next cheat update. Master codes remain enabled. Code words shared by several cheats retain one original value and are restored after the last writer is disabled; already changed gameplay data is not rolled back. Reward screenshots take temporal anti-aliasing output before Neural Rendering, preserving the game's screenshot colours. Live graphics rebuilds discard menu anti-aliasing captures, screenshot images and previous menu-camera draw counts. FSR 4 fallback reasons use atomic status alongside the actual backend, rather than startup-only configuration. Before temporal output is created, its source is narrowed from Smooth Gradients to RGBA8; stand-in caches also check the actual Vulkan format. This keeps an MSAA-to-FSR switch during character selection from retaining an RGBA16 storage output.

## Testing

Run the checks corresponding to the changed areas:

```sh
make test-settings test-settings-panel test-settings-locale test-cheats test-interpreter
make test-camera-aspect test-forge-draw-vertices
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
