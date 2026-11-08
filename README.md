<h1 align="center">MHGURecomp Enhanced</h1>

<p align="center"><strong>English</strong> · <a href="docs/README.zh-CN.md">简体中文</a> · <a href="docs/README.zh-TW.md">繁體中文</a></p>

Built on [morerokk/MHGURecomp](https://github.com/morerokk/MHGURecomp), this enhanced Windows edition adds **native keyboard/mouse input and icon support, ultrawide support, DLSS Super Resolution and DLSS 5 Neural Rendering**.

The enhanced edition has its own version in [VERSION](VERSION). [UPSTREAM_VERSION](UPSTREAM_VERSION) records the upstream stable release corresponding to the changes selected for integration. Release titles contain those two numbers only, for example **0.3.4-0.15.4**. Git tags and updater comparisons use the enhanced-edition version, starting at **0.1.0**. The release notes list the upstream changes included in each release.

## Requirements and installation

- Windows 10/11, 64-bit; AVX2/FMA-capable CPU; Vulkan 1.2-capable GPU; at least 4 GB RAM and 1 GB VRAM for the base renderer. DLSS and NR have additional GPU, driver and memory requirements.
- Your own complete **Nintendo Switch MHGU 1.4.0** game files, with the update merged. The 3DS game is not supported.
- The supported client release is Windows. A Linux client build and test environment is not available.

Download `MHGURecomp-Enhancement-win_x86-64.zip` from [Releases](https://github.com/dzxrly/MHGURecomp-Enhanced/releases), extract it, then place both complete game folders here:

```text
Windows-MHGURecomp/
  mhgurecomp.exe
  mhgugame.dll
  mhgurecomp-updater.exe
  data/
    exefs/
    romfs/
  mods/Keyboard_Mouse_Input_Auto/
```

Keep all program dependencies from the archive. Start `mhgurecomp.exe`; the first start creates `mhgurecomp.ini`. Startup checks the supported game version. Keep the base game files unchanged and install changes through mods.

## Ultrawide support

In **F1 → Graphics → Aspect ratio**, choose **Auto**, 16:10, 16:9, 21:9, 32:10, 32:9 or **Custom**. Auto follows the selected screen resolution. The resolution-labelled presets use exact pixel ratios:

| Screen resolution | Exact preset |
| --- | --- |
| 2560×1080 | 64:27 |
| 3440×1440 | 43:18 |
| 3840×1600 | 24:10 |

Choose Auto or the matching preset for these screens. Wider ratios keep the vertical field of view and render more of the scene on both sides, without stretching the 16:9 picture. A mismatched screen resolution leaves bars around the fitted picture. **Save applies ratio changes immediately**, including repeated switches from an ultrawide startup configuration.

> HUD (heads-up display) means the health bars, party information, item bar and other information drawn over the scene.

**HUD anchor** is independent of the 3D view: **16:9** (`UIAnchor = Center` in the INI) keeps it in the central area, while **Edges** moves it to the picture's sides. Intermediate 21:9/32:9 anchors are also available. HUD placement applies after Save.

Pre-recorded movies retain their source aspect ratio and use black bars as needed. Samples of real-time cutscenes have also been checked, but not every cutscene or camera. The 21:9 checks include same-camera scene comparisons, HUD placement and movies; 32:9 has implementation and menu-regression coverage, without a physical 32:9 monitor test.

## Graphics and neural rendering

Graphics settings are in the INI's **[Graphics]** section. F1's **Graphics** page exposes the controls except the INI-only `NRRuntime` selection. Save applies display mode, resolution, upscaler and NR controls in the current game. Changing the frame generation backend or multiplier requires restarting. Resource changes may briefly pause rendering. Audio has its own page.

Use **Anti-aliasing / upscaling** to select Off, FXAA, MSAA, MSAA + FXAA, FSR 3.1, FSR 4, XeSS SR or DLSS SR / DLAA. The menu shows the parameters used by your selection: MSAA samples, FXAA mode, or temporal quality and related controls. MSAA choices are unavailable while NR is on. DLSS offers **Ultra Performance → Performance → Balanced → Quality → DLAA**. `UpscaleQuality = DLAA` selects native-resolution anti-aliasing. Render sizes come from the SDK; NVIDIA App overrides can affect the actual size/model, which the menu and log report. FSR 4 needs a compatible build/AMD driver and falls back to FSR 3.1 if unavailable. DLSS failure is reported explicitly.

**Frame generation** selects Off, FSR or DLSS. The **Frame rate limit** is the number of real game frames rendered per second before interpolation. The menu reports rendered and presented frame rates separately when available. DLSS uses a fixed multiplier reported by the driver. XeSS is available as an upscaler.

Enable **Performance overlay** in F1 → Graphics. It shows presentation FPS, rendered FPS and super-resolution status in a single line such as `60 / 30 - SR on`. Choose a screen corner and background transparency, then Save.

NR has a separate switch that defaults to Off and also works with temporal upscaling off. The same ZIP includes both NR runtimes. The default `NRRuntime = Auto` selects Official on RTX 50 or SM89 on RTX 40. To override the selection, close the game and set `NRRuntime` under `[Graphics]` to `Official` or `SM89`. F1 disables Neural Rendering when the GPU supports neither runtime. See [runtime configuration and sources](third_party/ngx/README.md).

```text
graphics/nvidia/ngx/nr/official/nvngx_dlssnr.dll
graphics/nvidia/ngx/nr/sm89/nvngx_dlssnr.dll
```

| NR setting | INI key | Default | Values |
| --- | --- | --- | --- |
| Model strength | `NRIntensity` | 0.5 | 0–2 |
| Style | `NRStyle` | Standard | Standard / Natural / Cinematic |
| Local structure | `NRLocalStructure` | 1 | 0–2 |
| Local tone | `NRLocalTone` | 0 | 0–2 |
| Skin structure | `NRSkinStructure` | **0** | 0–2 |
| Automatic skin mask | `NRAutoMask` | **On** | On / Off |
| Effect layers | `NRLayers` | 1 | **1–3** |

Each scene frame runs NR once. Levels 1/2/3 use **1.00x / 1.25x / 1.50x** of the difference between the NR result and the original scene, composited before the HUD. Level 1 keeps the direct NR result; levels 2/3 add mild enhancement without running the model again. The reduced gains lessen the dark contours caused by the former 2x/3x amplification. Skin 0 requests no skin-structure enhancement, while other model changes may remain. Detailed help appears in F1 and the INI. NR controls apply when you press Save. Basic NR and SR+NR have been exercised on RTX 4080; RTX 50 and other GPU families remain unverified in this project.

## Keyboard and mouse

### Enable and configure

At startup, choose Controller or Keyboard and mouse. The choice is saved to `mhgurecomp.ini`. Check **Don't ask again** to reuse it on later launches. Set `AskInputOnStartup = On` in the INI to show the picker again. The equivalent keyboard/mouse configuration is:

```ini
[Controls]
InputMode = Keyboard_Mouse_Input

[Mouse]
MouseSensitivity = 1.0
```

The default mode is `Original`; use `Keyboard_Mouse_Input` for the controls below. Controllers work in either mode. Light Bowgun, Heavy Bowgun and Bow automatically use **Ranged**; other Hunter weapons and Prowler use **Melee**. Equipment changes switch bindings and prompts without restarting.

### Enable keyboard/mouse prompts

The release includes `mods/Keyboard_Mouse_Input_Auto`. Enable it in **F1 → Mods**, then restart. It follows your equipment and game language automatically. Keep the whole folder; no separate download or generation is needed.

### Bindings

| Input                      | Melee                                                 | Ranged                                                |
| -------------------------- | ----------------------------------------------------- | ----------------------------------------------------- |
| WASD                       | Left stick: move                                      | Left stick: move                                      |
| Mouse movement             | Quest camera                                          | Quest camera / weapon aim                             |
| Left mouse                 | X: primary attack / draw                              | A: fire, confirm / interact                           |
| Right mouse                | A: secondary attack, confirm / interact               | R: aim                                                |
| R                          | R: guard / weapon special action                      | X: reload / draw                                      |
| Mouse side button 5        | R: guard / weapon special action                      | Unbound                                               |
| F / Mouse side button 4    | Y: sheathe / use item                                 | Y: sheathe / use item                                 |
| E                          | Y: sheathe / use item                                 | R: aim                                                |
| Left Shift                 | R: ordinary sprint                                    | R: ordinary sprint                                    |
| Space                      | B: evade, cancel / back                               | B: evade, cancel / back                               |
| C                          | X+A: combined attack                                  | X+A: combined action                                  |
| Wheel up / down            | L+Y / L+A: select item                                | L+X / L+B: select ammo or coating                     |
| Q                          | L: item bar / camera operation                        | L: item bar / camera operation                        |
| Esc / Enter / Numpad Enter | Plus: game menu                                       | Plus: game menu                                       |
| Backspace                  | Minus: secondary menu                                 | Minus: secondary menu                                 |
| Tab / Middle mouse         | ZL: switch target (control type 1)                    | ZL: switch target (control type 1)                    |
| T                          | ZR                                                    | ZR                                                    |
| 1 / 2 / 3                  | D-pad up / right / down: Hunter Arts (control type 1) | D-pad up / right / down: Hunter Arts (control type 1) |
| Arrow keys                 | D-pad                                                 | D-pad                                                 |
| Ctrl / Alt                 | Left / right stick click                              | Left / right stick click                              |
| F1 / F2                    | PC settings / release mouse                           | PC settings / release mouse                           |

Bindings use physical QWERTY key positions. In game menus, confirm with right mouse in Melee or left mouse in Ranged; Space goes back. Original game menus use button navigation rather than pointer clicks. Press F2 to release the mouse; click the window to capture it again.

Press `/` to chat in town or quests, Enter to send and Esc to cancel. Change the chat key in **F1 → Controls**. During text entry, Ctrl+V pastes.

### Bow and Prowler

Bow uses Ranged. With the Bow sheathed, left mouse remains A for interaction; press R (game X) to draw. While drawn:

| Input               | Bow control type 1             | Bow control type 2                          |
| ------------------- | ------------------------------ | ------------------------------------------- |
| Left mouse          | X: charge; release to shoot    | Configured game R: charge; release to shoot |
| Right mouse         | Configured game R: hold to aim | X: click to toggle aim                      |
| Mouse side button 5 | A: secondary Bow action        | A: secondary Bow action                     |

Mouse controls follow the game’s Bow control type and R/ZR swap option. Keyboard controls stay as in the main table; menus use left mouse to confirm. Bow prompts still show the base Ranged layout, so use this table for Bow mouse controls.

Prowler uses Melee: left mouse attacks and right mouse charges a boomerang. While charging, hold R / side button 5 for precision aim. With the weapon out, R guards and R+left mouse performs the overhead attack; with it put away, R+C guards. The wheel selects Support Moves; F uses one while sheathed, R+F while drawn. Original move requirements and gauge costs apply.

### Mouse sensitivity and camera options

Adjust **Mouse sensitivity** in F1, or `MouseSensitivity` in the ini (0.1–10). In the game’s own options, **Camera Speed** adjusts the quest camera and **Reticle Speed** adjusts bowgun/Bow aim and Binoculars; both apply immediately. Camera, Quick Aim and Scope inversion are configured separately in the game.

Original firing recovery, aiming delays and attack-turning restrictions still apply. Village cameras, fixed Ballista and Prowler special aiming retain the game’s turn-speed limits. Some handbook pages still show controller illustrations.

## Settings, files and multiplayer

F1 opens settings. **Menu Options** offers Gold, Blue and Hunter themes and a **50–200%** interface scale. Drag the tabs to move the menu or its bottom-right corner to resize it; **Attach menu** restores the attached layout. Save changes with **Ctrl+S**, the top controller face button, or Save.

Alt+Enter or F11 toggles fullscreen, F12 takes a screenshot and F7 cycles MSAA where available. The PC menu supports English, Spanish, French, German, Italian, Simplified Chinese and Traditional Chinese. New settings default to **Follow game**; choose an independent language in Menu Options if preferred. The bundled Noto font supplies multilingual glyphs while retaining a custom Forge font. Hints follow the last keyboard/mouse or controller input. Use arrows or Tab/Shift+Tab, Enter/left click to confirm, and Esc/F1 to go back.

**F1 → Item Box** edits the active character's live inventory after loading. Search by the game's item names or an item ID; unowned, valid box items show zero. Select an item, enter its total quantity and choose **Apply quantity**. The editor combines duplicate stacks and splits quantities into stacks of up to 99 within your unlocked box capacity. Changes take effect immediately; save in the game to keep them. Host settings Save/Reset does not apply or undo item edits, and returning to the title invalidates an open quantity edit.

Search and quantity use ordinary text fields with a Ctrl+V paste hint. Quantity accepts decimal integers with an optional leading `+` or `-`; empty input, spaces, decimals and other text are rejected. A nonnegative number sets the total; a negative number subtracts from the displayed total (100 with `-20` becomes 80). Results below zero or beyond available box space are rejected. The compact list shows 100 items per page in two columns when space permits, with a single column in narrow windows.

Both the fixed box header and the quantity input page show the currently unlocked slot limit, used and free slots, total item count and remaining capacity. Remaining capacity separates empty slots (99 items each) from space in existing stacks, which can only hold more of the same item.

| Content | Location |
| --- | --- |
| Settings / log | `mhgurecomp.ini` / `log.txt` beside the program |
| Windows save | `%APPDATA%\MHGURecomp\save\save_0\` |
| Mods / cheats | `mods/` / `cheats/` |
| Screenshots / shader cache | `shots/` / `cache/` |

To import a Switch save, back up the destination, then copy your `system` and `system_backup` into `save_0`.

Choose **Local Wireless** in the game. LAN/virtual-LAN players can discover rooms directly. Internet play uses the same compatible client, hub server and optional matchmaking key, configured under **F1 → Network**. Direct hosting requires reachable TCP/UDP port 39500 by default.

Put each mod in its own `mods/` directory. Enable/order it in **F1 → Mods**, then restart; later conflicting mods take priority. Put MHGU 1.4.0 cheat text files in `cheats/`; prefix a filename with `_` to disable it. The included Auto prompt mod follows equipment and game language. Optional monster-HP and Hub/Village settings are under Misc. Bonus downloads can be imported when a separately supplied `bonus/system` donor is present and `BonusContent = On`.

In **F1 → Cheats**, switch individual cheats on or off, then Save to apply without restarting. Master codes stay on. Turning a cheat off restores its code changes after all cheats using the same code are off; money, items and other data already changed remain. Restart after adding cheat files.

### Forge plugins

The PC runtime runs Forge plugins through its own Forge 1.5.1 compatibility layer. Install the plugin's complete `romfs/` tree under its own mod folder. Do not copy the Switch Forge loader's `exefs/` files into the game; this edition supplies the loader functionality. Press **Insert → Plugin UI** to open a plugin's settings.

**Damage Numbers r36** uses the original, unchanged `damage_numbers.nro` plugin and accompanying font/artwork files. Version 0.2.2 also maps the plugin's game-canvas coordinates to the visible picture, correcting displaced, undersized numbers with DLSS and ultrawide output. Install the mod separately under `mods/damage-numbers/`; it is not included in the release archive.

## Updates and troubleshooting

Download releases from this repository or use its included updater. Updates replace program files, bundled DLSS runtimes and Auto prompt resources while preserving game data, settings, saves and other mods.

- **Game-version warning:** use a complete Switch 1.4.0 dump with its update merged; leave its base files unchanged.
- **Controller prompts remain:** select `InputMode = Keyboard_Mouse_Input`, enable the bundled Auto mod and restart.
- **DLSS/NR unavailable.** Check the actual status in F1 and the log. On RTX 40, use `NRRuntime = Auto` or `SM89`, then restart.
- **Frame-time spikes:** start at 60 FPS with VSync and leave development graphics statistics disabled.
- **Report a problem:** include the enhanced-edition version, GPU/driver, graphics settings, reproduction steps and a copy of `log.txt` in [Issues](https://github.com/dzxrly/MHGURecomp-Enhanced/issues).

[Developer documentation](docs/DEVELOPMENT.md) covers architecture, building, testing and release publication.

## License and attribution

Project code is MIT licensed; retain the [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES). NVIDIA components have separate terms. The game and generated game code are not covered by the project's license. This project is not affiliated with Capcom, Nintendo or NVIDIA.
