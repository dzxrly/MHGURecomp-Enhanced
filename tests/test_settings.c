// Host-only persistence regression test. No game files or live save are used.
// clang -Iruntime tests/test_settings.c runtime/config.c runtime/config_store.c -lSDL3 -o test-settings.exe
#include "config.h"
#include "menu_confirm.h"
#include "settings_locale.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define set_env(key, value) _putenv_s(key, value)
#else
#define set_env(key, value) setenv(key, value, 1)
#endif

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb"); assert(f);
    fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
    char *text = malloc(size + 1); assert(text);
    assert(fread(text, 1, size, f) == (size_t)size); text[size] = 0;
    fclose(f); return text;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    Config preset = config_defaults();
    assert(preset.language == MENU_LANG_GAME);
    assert(config_language() == MENU_LANG_ENGLISH);
    assert(config_resolve_language(MENU_LANG_GAME) == MENU_LANG_ENGLISH);
    const char *language_names[] = {"English", "Spanish", "French", "German", "Italian",
                                   "SimplifiedChinese", "TraditionalChinese", "Game"};
    const char *language_aliases[] = {"en", "es", "fr", "de", "it", "zh-CN", "zh-TW", "auto"};
    for (int language = MENU_LANG_ENGLISH; language <= MENU_LANG_GAME; ++language) {
        char formatted[64];
        assert(config_parse(config_setting("Language"), language_names[language], &preset));
        assert(preset.language == language);
        config_format(config_setting("Language"), &preset, formatted, sizeof formatted);
        assert(!strcmp(formatted, language_names[language]));
        assert(config_parse(config_setting("Language"), language_aliases[language], &preset));
        assert(preset.language == language);
    }
    config_set_game_language(MENU_LANG_CHINESE_SIMPLIFIED);
    assert(config_resolve_language(MENU_LANG_GAME) == MENU_LANG_CHINESE_SIMPLIFIED);
    assert(config_resolve_language(MENU_LANG_SPANISH) == MENU_LANG_SPANISH);
    config_set_game_language(100);
    assert(config_resolve_language(MENU_LANG_GAME) == MENU_LANG_CHINESE_SIMPLIFIED);
    config_set_game_language(MENU_LANG_ENGLISH);
    assert(preset.menu_theme == THEME_HUNTER);
    assert(!preset.dlss_nr && preset.nr_runtime == NR_RUNTIME_OFFICIAL);
    assert(fabsf(preset.nr_intensity - .5f) < .001f);
    assert(preset.nr_auto_mask && preset.nr_skin == 0 && preset.nr_layers == 1);
    assert(preset.nr_style == 0 && preset.nr_structure == 1 && preset.nr_tone == 0);
    assert(config_parse(config_setting("NeuralRendering"), "On", &preset));
    assert(preset.dlss_nr && !preset.fsr && preset.nr_runtime == NR_RUNTIME_OFFICIAL);
    assert(config_parse(config_setting("NRRuntime"), "SM89", &preset));
    assert(preset.nr_runtime == NR_RUNTIME_SM89);
    assert(config_parse(config_setting("NRRuntime"), "Official", &preset));
    assert(!config_parse(config_setting("NRRuntime"), "Auto", &preset));
    assert(config_parse(config_setting("Upscaler"), "DLSS", &preset));
    assert(preset.fsr_upscaler == FSR_UP_DLSS);
    assert(config_parse(config_setting("UpscaleQuality"), "DLAA", &preset));
    assert(preset.fsr_quality == FSR_NATIVE);
    char quality_name[32];
    config_format(config_setting("UpscaleQuality"), &preset, quality_name, sizeof quality_name);
    assert(!strcmp(quality_name, "DLAA"));
    assert(config_parse(config_setting("NRIntensity"), "1.1", &preset));
    assert(!config_parse(config_setting("NRIntensity"), "2.1", &preset));
    assert(config_parse(config_setting("NRStyle"), "Cinematic", &preset) && preset.nr_style == 2);
    assert(!config_parse(config_setting("NRStyle"), "3", &preset));
    assert(!config_parse(config_setting("NRLayers"), "0", &preset));
    assert(!config_parse(config_setting("NRLayers"), "4", &preset));
    assert(!config_parse(config_setting("NRSkinStructure"), "-0.1", &preset));
    assert(!config_parse(config_setting("NRLocalTone"), "nan", &preset));
    preset = config_defaults();
    assert(preset.input_mode == INPUT_ORIGINAL);
    assert(preset.ask_input == 1);
    assert(config_parse(config_setting("AskInputOnStartup"), "Off", &preset) && !preset.ask_input);
    assert(!config_parse(config_setting("AskInputOnStartup"), "Invalid", &preset));
    assert(preset.ldn_server[0] == '\0' && preset.ldn_key[0] == '\0');
    assert(config_parse(config_setting("Server"), "example.org:39501", &preset));
    assert(!strcmp(preset.ldn_server, "example.org:39501"));
    assert(config_parse(config_setting("Key"), "friends", &preset));
    assert(!strcmp(preset.ldn_key, "friends"));
    const ConfigSetting *mode_setting = config_setting("InputMode");
    assert(mode_setting && mode_setting->restart);
    assert(config_parse(mode_setting, "Keyboard_Mouse_Input", &preset));
    assert(preset.input_mode == INPUT_KEYBOARD_MOUSE && config_valid(mode_setting, &preset));
    assert(!config_parse(mode_setting, "Unknown", &preset));
    // Presets are selected from the actual equipped weapon, never from a separate INI choice.
    assert(config_setting("KeyboardLayout") == NULL);
    const char *mouse_keys[] = {"MinWindowMs", "FullStickSpeed", "DeadzoneCompensation", "LowSpeedCutoff", "FrameTimeSmoothing", "AimDeadzoneCompensation"};
    const char *mouse_valid[] = {"12", "2000", "0.45", "0", "0.5", "0.55"};
    const char *mouse_invalid[] = {"51", "100001", "0.96", "0.11", "1.01", "0.96"};
    for(unsigned i=0;i<sizeof mouse_keys/sizeof *mouse_keys;i++) {
        const ConfigSetting *s=config_setting(mouse_keys[i]);
        assert(s && s->restart && !strcmp(s->section,"MouseConversion"));
        assert(config_parse(s,mouse_valid[i],&preset) && config_valid(s,&preset));
        assert(!config_parse(s,mouse_invalid[i],&preset));
        assert(!config_parse(s,"-1",&preset) && !config_parse(s,"nan",&preset) && !config_parse(s,"inf",&preset));
    }
    assert(config_parse(config_setting("LowSpeedCutoff")," 2e-3 ",&preset));
    assert(fabsf(preset.mouse_conversion.low_speed_cutoff - 0.002f)<1e-7f);
    assert(!config_parse(config_setting("FullStickSpeed"),"-1500",&preset));
    const char *path = argv[1];
    FILE *f = fopen(path, "wb"); assert(f);
    fputs("; Keep this comment\r\n[Display]\r\nMode = Borderless Window\r\nResolution = Desktop\r\nRenderResolution = Auto\r\n"
          "AspectRatio = 21:9\r\nFOV = 1.1 ; camera comment\r\n"
          "[Performance]\r\nFPS = 60\r\nFrameRate = 60\r\n"
          "[Upscaler]\r\nNeuralRendering = Off\r\nNRIntensity = 0.50 ; keep NR note\r\nNRRuntime = Official\r\n"
          "[Monster HP]\r\nQuest101 = 0.6\r\nQuest102 = 1, 2, 3, 4\r\n"
          "[Future section]\r\nFutureOption = preserve-me", f);
    fclose(f);
    set_env("MHGU_CONFIG", path);
    set_env("MHGU_WINDOW", "900x600");
    set_env("MHGU_FOV", "1.6");
    set_env("MHGU_FRAMERATE", "144.5");
    Config initial = g_config;
    config_load();
    Config c = config_editable();
    assert(c.width == 0 && c.height == 0 && c.render_w == 0 && c.render_h == 0);
    assert(fabsf(c.fov - 1.1f) < 0.001f);
    assert(g_config.width == 900 && fabsf(g_config.fov - 1.6f) < 0.001f);
    assert(c.hp.quests == 2 && c.language == MENU_LANG_GAME && c.button_icons == 0);
    assert(!config_restart_required(&c)); // Raw Auto/Desktop survives environment resolution.
    Config prompts=c; prompts.input_mode=INPUT_KEYBOARD_MOUSE;
    prompts.mouse_conversion=(MouseConversion){12,2000,0.45f,0.001f,0.5f,0.55f};
    char prompt_error[256];
    assert(config_restart_required(&prompts) && config_save(&prompts,prompt_error,sizeof prompt_error));
    char *prompt_text=read_file(path); assert(strstr(prompt_text,"InputMode = Keyboard_Mouse_Input") && !strstr(prompt_text,"KeyboardLayout ="));
    assert(strstr(prompt_text,"[MouseConversion]") && strstr(prompt_text,"MinWindowMs = 12") && strstr(prompt_text,"FullStickSpeed = 2000"));
    assert(strstr(prompt_text,"DeadzoneCompensation = 0.45") && strstr(prompt_text,"LowSpeedCutoff = 0.001") && strstr(prompt_text,"FrameTimeSmoothing = 0.5"));
    assert(strstr(prompt_text,"AimDeadzoneCompensation = 0.55"));
    free(prompt_text);
    assert(g_config.mouse_conversion.min_window_ms == 8 && g_config.mouse_conversion.full_stick_speed == 1500);
    assert(g_config.mouse_conversion.aim_deadzone_compensation == 0.5f && g_config.mouse_conversion.low_speed_cutoff == 0);
    assert(g_config.input_mode == INPUT_ORIGINAL); // Saving must not pretend the mounted RomFS changed live.
    assert(config_save(&c,prompt_error,sizeof prompt_error) && !config_restart_required(&c));
    assert(fabsf(config_live_fov()-1.6f)<0.001f && fabsf(config_live_fps()-144.5f)<0.001f);
    Config volume_only=c; volume_only.volume=35;
    char error[256];
    assert(config_save(&volume_only,error,sizeof error));
    assert(fabsf(config_live_fov()-1.6f)<0.001f && fabsf(config_live_fps()-144.5f)<0.001f);
    float camera_before=camera_fovy(0.5f,g_render_aspect);
    c.menu_confirm = MENU_CONFIRM_SOUTH; c.menu_scale = 170; c.fps = 120; c.fov = 1.2f; c.volume = 25; c.confirm_exit = 0; c.language = 1; c.button_icons = 2;
    c.fxaa=0; c.msaa=4; c.vsync=0; c.face_buttons=FACE_LABEL; c.background_input=0; c.mouse_sensitivity=2.5f;
    assert(config_save(&c,error,sizeof error) && !config_restart_required(&c));
    assert(config_live_fps()==120 && fabsf(config_live_fov()-1.2f)<0.001f);
    assert(config_live_msaa()==4 && !config_live_vsync() && !config_live_fxaa());
    assert(config_live_face_buttons()==FACE_LABEL && !config_live_background_input());
    assert(config_live_mouse_sensitivity()==2.5f && camera_fovy(0.5f,g_render_aspect)<camera_before);
    assert(camera_fovy(0.5f,g_render_aspect*1.002f)<camera_before && camera_fovy(0.5f,g_render_aspect*1.1f)==0.5f); // (pixel rounding)
    float aspect_was=g_render_aspect; uint32_t size_was[2]={g_render_size[0],g_render_size[1]};
    g_render_aspect=3.57778f, g_render_size[0]=426, g_render_size[1]=118; // (FSR Ultra Performance at 1280x720: 0.9% off)
    assert(camera_fovy(0.5f,426.0f/118.0f)!=0.5f && camera_fovy(0.5f,3.3f)==0.5f);
    g_render_aspect=aspect_was, g_render_size[0]=size_was[0], g_render_size[1]=size_was[1];
    assert(g_config.fps==60 && g_config.fxaa==1 && g_config.face_buttons==FACE_POSITION); // Boot snapshot is immutable.
    c.hp.quest[0] = c.hp.quest[1]; c.hp.quests = 1;
    assert(config_save(&c, error, sizeof error));
    assert(fabsf(config_audio_gain() - 0.25f) < 0.001f && !config_confirm_exit());
    assert(config_language() == 1);
    assert(config_restart_required(&c));
    Config restored=c; restored.hp=initial.hp; // Reconstruct the loaded quest overrides, not compiled defaults.
    restored.hp.quests=2; restored.hp.quest[0].quest=101;
    for(int i=0;i<4;i++) restored.hp.quest[0].scale[i]=0.6f;
    restored.hp.quest[1]=c.hp.quest[0];
    assert(!config_restart_required(&restored));
    assert(!strcmp(settings_text(1, "Language"), "Idioma"));
    assert(!strcmp(settings_text(0, "Language"), "Language"));
    assert(!strcmp(settings_text(MENU_LANG_FRENCH, "Language"), "Langue"));
    assert(!strcmp(settings_text(MENU_LANG_GERMAN, "Language"), "Sprache"));
    assert(!strcmp(settings_text(MENU_LANG_ITALIAN, "Language"), "Lingua"));
    assert(!strcmp(settings_text(MENU_LANG_CHINESE_SIMPLIFIED, "Language"), "语言"));
    assert(!strcmp(settings_text(MENU_LANG_CHINESE_TRADITIONAL, "Language"), "語言"));
    assert(!strcmp(settings_text(MENU_LANG_GAME, "Language"), "Language"));
    assert(!strcmp(settings_text(-1, "Language"), "Language"));
    assert(!strcmp(settings_text(99, "Language"), "Language"));
    assert(!strcmp(settings_text(1, "Duplicate quest ID."), "ID de misión repetido."));
    char *text = read_file(path);
    assert(!strncmp(text, "[Graphics]\r\n", 12));
    assert(!strstr(text, "[Display]") && !strstr(text, "[Anti-aliasing]"));
    assert(!strstr(text, "[Performance]") && !strstr(text, "[Upscaler]"));
    assert(!strstr(text + 1, "[Graphics]"));
    assert(strstr(text, "TemporalUpscaling = Off") && strstr(text, "NRRuntime = Official"));
    assert(strstr(text, "NRAutoMask = On") && strstr(text, "NRSkinStructure = 0"));
    assert(strstr(text, "NRLayers = 1") && strstr(text, "; One model evaluation per scene frame"));
    assert(strstr(text, "; Overall strength requested from the NR model") && strstr(text, "NRIntensity = 0.50 ; keep NR note"));
    assert(strstr(text, "; Keep this comment\r\n"));
    assert(strstr(text, "; camera comment"));
    assert(strstr(text, "FutureOption = preserve-me"));
    assert(strstr(text, "Resolution = Desktop") && strstr(text, "RenderResolution = Auto"));
    // Unchanged values keep the player's spelling; changed numbers are written short.
    assert(strstr(text, "Mode = Borderless Window\r\n") && strstr(text, "AspectRatio = 21:9\r\n"));
    assert(strstr(text, "FOV = 1.2 ; camera comment") && strstr(text, "MouseSensitivity = 2.5\r\n"));
    assert(!strstr(text, "Quest101 ="));
    assert(strstr(text, "Quest102 = 1, 2, 3, 4"));
    assert(strstr(text, "FPS = 120") && strstr(text, "FrameRate = 120"));
    assert(strstr(text, "Volume = 25") && strstr(text, "ConfirmExit = Off"));
    assert(strstr(text, "Language = Spanish") && strstr(text, "ButtonIcons = PlayStation"));
    // Invalid edits must leave both the file and the live audio setting intact.
    Config bad=c;bad.menu_confirm=3;assert(!config_save(&bad,error,sizeof error));
    bad=c;bad.mouse_conversion.low_speed_cutoff=NAN;assert(!config_save(&bad,error,sizeof error));
    bad=c;bad.menu_scale=49;assert(!config_save(&bad,error,sizeof error));
    bad.menu_scale=201;assert(!config_save(&bad,error,sizeof error));
    bad = c; bad.button_icons = 3;
    assert(!config_save(&bad, error, sizeof error));
    bad = c; bad.language = MENU_LANG_GAME + 1;
    assert(!config_save(&bad, error, sizeof error));
    bad = c; bad.fov = NAN;
    assert(!config_save(&bad, error, sizeof error));
    bad = c; bad.ldn_port = 0; assert(!config_save(&bad, error, sizeof error));
    bad = c; strcpy(bad.ldn_join, "host\nFPS=30"); assert(!config_save(&bad, error, sizeof error));
    bad = c; bad.hp.quests = 2; bad.hp.quest[1] = bad.hp.quest[0];
    assert(!config_save(&bad, error, sizeof error));
    char *after = read_file(path); assert(!strcmp(text, after)); free(after);
    set_env("MHGU_CONFIG", "missing-settings-parent/settings.ini");
    bad = c; bad.volume = 90; bad.language = 0; bad.fps=30; bad.fov=2; bad.msaa=1; bad.vsync=1; bad.fxaa=1; bad.face_buttons=FACE_POSITION; bad.background_input=1; bad.mouse_sensitivity=9;
    assert(!config_save(&bad, error, sizeof error));
    assert(fabsf(config_audio_gain() - 0.25f) < 0.001f);
    assert(config_live_fps()==120 && config_live_msaa()==4 && !config_live_vsync() && !config_live_fxaa());
    assert(config_live_face_buttons()==FACE_LABEL && !config_live_background_input() && config_live_mouse_sensitivity()==2.5f);
    assert(fabsf(config_live_fov()-1.2f)<0.001f);
    assert(config_language() == 1); // Failed writes must not change the UI language.
    set_env("MHGU_CONFIG", path);
    set_env("MHGU_WINDOW", ""); set_env("MHGU_FOV", ""); set_env("MHGU_FRAMERATE", "");
    g_config = initial; config_load();
    Config loaded = config_editable();
    assert(loaded.language == 1 && config_language() == 1 && loaded.button_icons == 2);
    assert(loaded.menu_scale == 170 && loaded.menu_confirm == MENU_CONFIRM_SOUTH);
    assert(config_live_menu_confirm() == MENU_CONFIRM_SOUTH);
    assert(loaded.fps == 120 && loaded.volume == 25 && loaded.hp.quests == 1);
    assert(loaded.hp.quest[0].quest == 102 && loaded.hp.quest[0].scale[3] == 4);
    assert(loaded.width == 0 && loaded.render_w == 0);
    assert(!loaded.confirm_exit && fabsf(loaded.fov - 1.2f) < 0.001f);
    assert(!config_restart_required(&loaded));
    assert(config_live_fps()==120 && config_live_msaa()==4 && config_live_face_buttons()==FACE_LABEL);
    assert(!config_restart_key("RenderResolution") && !config_restart_key("FOV"));
    assert(!config_restart_key("MSAA") && !config_restart_key("VSync") && !config_restart_key("FaceButtons"));
    for (int i = 0; i < g_nsettings; i++)
        if (!strcmp(g_settings[i].section, "Graphics"))
            assert(g_settings[i].restart == !strcmp(g_settings[i].key, "NRRuntime"));
    assert(config_restart_key("framerate") == config_restart_key("FPS") && !config_restart_key("NoSuchKey"));
    // The menu's help: the default ini's comments, wrapped lines joined, the default last (none when empty)
    char help[2048];
    assert(config_help("FPS", help, sizeof help) && !strcmp(help, "Framerate cap\nBetween 30 and 240\nDefault: 60"));
    assert(config_help("3Players", help, sizeof help) && !strncmp(help, "Playercount-specific HP multipliers", 35));
    assert(config_help("Server", help, sizeof help) && strstr(help, " and Search Hubs shows") && !strstr(help, "Default"));
    assert(config_help("Key", help, sizeof help) && !strncmp(help, "Matchmaking key:", 16));
    assert(config_help("DrawDistance", help, sizeof help) && strstr(help, "twice as far, 0.5 half") && strstr(help, "\nBetween 0"));
    assert(!config_help("NoSuchKey", help, sizeof help));
    // A second save must not append duplicate sections or change resolved Auto settings.
    assert(config_save(&loaded, error, sizeof error));
    after = read_file(path);
    if (strcmp(text, after)) {
        size_t at=0; while(text[at] && text[at]==after[at]) at++;
        fprintf(stderr,"INI idempotence at %zu: before %.100s | after %.100s\n",at,text+at,after+at);
    }
    assert(!strcmp(text, after)); free(text); free(after);
    loaded.language = 0; loaded.button_icons = 1;
    assert(config_save(&loaded, error, sizeof error) && config_language() == 0);
    g_config = initial; config_load();
    assert(config_editable().language == 0 && config_language() == 0 && config_editable().button_icons == 1);
    bool waiting=true;
    assert(menu_release_buttons(0x41,0xf,&waiting)==0x40 && waiting);
    assert(menu_release_buttons(0x42,0xf,&waiting)==0x40 && waiting);
    assert(menu_release_buttons(0x40,0xf,&waiting)==0x40 && !waiting);
    assert(menu_release_buttons(0x42,0xf,&waiting)==0x42);
    int confirm, cancel;
    menu_confirm_buttons(MENU_CONFIRM_ORIGINAL,1,0,1,0,&confirm,&cancel);assert(confirm&&!cancel);
    menu_confirm_buttons(MENU_CONFIRM_SOUTH,1,0,0,1,&confirm,&cancel);assert(confirm&&!cancel);
    menu_confirm_buttons(MENU_CONFIRM_SOUTH,1,0,1,0,&confirm,&cancel);assert(!confirm&&cancel);
    menu_confirm_buttons(MENU_CONFIRM_EAST,0,1,0,1,&confirm,&cancel);assert(confirm&&!cancel);
    assert(menu_confirm_position(MENU_CONFIRM_SOUTH,1,0,0)==0);
    assert(menu_confirm_position(MENU_CONFIRM_SOUTH,1,0,1)==1);
    Config fsr_config=config_editable();
    assert(config_defaults().sync_small_monsters==0);
    assert(config_defaults().shader_prewarm==1);
    assert(config_defaults().draw_distance==1.0f);
    assert(config_defaults().mute_in_background==0);
    assert(config_defaults().combo_window==COMBO_LENIENT);
    assert(config_defaults().fxaa_mode==FXAA_ORIGINAL);
    assert(config_defaults().smooth_gradients==0);
    fsr_config.fsr=1;fsr_config.fsr_upscaler=FSR_UP_4;fsr_config.fsr_quality=FSR_BALANCED;
    fsr_config.fsr_sharpness=.65f;fsr_config.fsr_objmotion=0;fsr_config.fsr_reactive=.35f;fsr_config.fsr_animsurf=0;
    fsr_config.msaa=8;fsr_config.fxaa=1;
    assert(config_save(&fsr_config,error,sizeof error));assert(!config_restart_required(&fsr_config));
    text = read_file(path);
    assert(strstr(text, "Sharpness = 0.65\r\n") && strstr(text, "Reactive = 0.35\r\n") && strstr(text, "Upscaler = FSR4\r\n"));
    free(text);
    g_config=initial;config_load();Config fsr_saved=config_editable();
    assert(fsr_saved.fsr && fsr_saved.fsr_upscaler==FSR_UP_4 && fsr_saved.fsr_quality==FSR_BALANCED);
    assert(fabsf(fsr_saved.fsr_sharpness-.65f)<.001f && fabsf(fsr_saved.fsr_reactive-.35f)<.001f);
    assert(!fsr_saved.fsr_objmotion && !fsr_saved.fsr_animsurf);
    assert(config_live_msaa()==1 && config_live_fxaa()==0);
    fsr_saved.volume=37;assert(config_save(&fsr_saved,error,sizeof error));
    assert(config_editable().msaa==8 && config_editable().fxaa==1);
    assert(config_live_msaa()==1 && config_live_fxaa()==0); // Saving cannot undo FSR's AA override.
    bad=fsr_saved;bad.fsr_quality=5;assert(!config_save(&bad,error,sizeof error));
    bad=fsr_saved;bad.fsr_upscaler=3;assert(!config_save(&bad,error,sizeof error));
    bad=fsr_saved;bad.fsr_sharpness=NAN;assert(!config_save(&bad,error,sizeof error));
    bad=fsr_saved;bad.fsr_reactive=1.1f;assert(!config_save(&bad,error,sizeof error));
    bad=fsr_saved;bad.fsr_objmotion=2;assert(!config_save(&bad,error,sizeof error));
    fsr_saved.hp.quests=HP_QUESTS_MAX;
    for(int i=0;i<HP_QUESTS_MAX;i++){fsr_saved.hp.quest[i].quest=50000+i;for(int j=0;j<4;j++)fsr_saved.hp.quest[i].scale[j]=1;}
    for(int quality=FSR_NATIVE;quality<=FSR_ULTRA_PERFORMANCE;quality++){
        fsr_saved.fsr_quality=quality;assert(config_save(&fsr_saved,error,sizeof error));
        g_config=initial;config_load();assert(config_editable().fsr_quality==quality);
        assert(config_editable().hp.quests==HP_QUESTS_MAX && config_editable().hp.quest[255].quest==50255);
    }
    fsr_saved.fsr_upscaler=FSR_UP_DLSS;
    static const char *const dlss_modes[]={"DLAA","Quality","Balanced","Performance","UltraPerformance"};
    for(int quality=FSR_NATIVE;quality<=FSR_ULTRA_PERFORMANCE;quality++){
        assert(config_parse(config_setting("UpscaleQuality"),dlss_modes[quality],&fsr_saved));
        assert(config_save(&fsr_saved,error,sizeof error));
        text=read_file(path);
        char expected[80];snprintf(expected,sizeof expected,"UpscaleQuality = %s\r\n",dlss_modes[quality]);
        assert(strstr(text,expected));free(text);
        g_config=initial;config_load();
        assert(config_editable().fsr_upscaler==FSR_UP_DLSS && config_editable().fsr_quality==quality);
    }
    Config nr_saved=config_editable();nr_saved.dlss_nr=1;nr_saved.nr_layers=3;nr_saved.nr_style=2;
    nr_saved.nr_intensity=1.5f;nr_saved.nr_structure=.75f;nr_saved.nr_tone=.25f;nr_saved.nr_skin=0;nr_saved.nr_auto_mask=1;
    assert(config_save(&nr_saved,error,sizeof error) && !config_restart_required(&nr_saved));
    g_config=initial;config_load();Config nr_loaded=config_editable();
    assert(nr_loaded.nr_layers==3 && nr_loaded.nr_style==2 && nr_loaded.nr_auto_mask && nr_loaded.nr_skin==0);
    assert(nr_loaded.nr_intensity==1.5f && nr_loaded.nr_structure==.75f && nr_loaded.nr_tone==.25f);
    assert(!config_restart_key("NRLayers") && !config_restart_key("NRAutoMask"));
    bad=nr_loaded;bad.nr_layers=4;assert(!config_save(&bad,error,sizeof error));
    bad=nr_loaded;bad.nr_skin=-.1f;assert(!config_save(&bad,error,sizeof error));
    bad=nr_loaded;bad.nr_structure=NAN;assert(!config_save(&bad,error,sizeof error));
    bad=nr_loaded;bad.nr_auto_mask=2;assert(!config_save(&bad,error,sizeof error));
    fsr_saved.fsr=0;assert(config_save(&fsr_saved,error,sizeof error));
    assert(config_live_msaa()==8 && config_live_fxaa()==1); // Saving disables temporal AA immediately.
    g_config=initial;config_load();assert(config_live_msaa()==8 && config_live_fxaa()==1);
    // SmoothGradients is live, persisted, and independent of native-input defaults.
    Config smooth=config_editable();smooth.smooth_gradients=1;
    assert(config_save(&smooth,error,sizeof error) && !config_restart_required(&smooth));
    assert(config_live_smooth_gradients()==1);
    uint64_t revision = 0; Config graphics;
    assert(config_graphics_snapshot(&graphics, &revision));
    assert(graphics.smooth_gradients == 1);
    assert(!config_graphics_snapshot(&graphics, &revision));
    Config dimensions = config_defaults();
    dimensions.display_mode = DISPLAY_WINDOWED; dimensions.width = 1920; dimensions.height = 1080;
    dimensions.fsr = 1; dimensions.fsr_quality = FSR_QUALITY;
    uint32_t render[2], output[2]; float aspect, anchor;
    config_graphics_resolve(&dimensions, render, output, &aspect, &anchor);
    assert(render[0] == 1280 && render[1] == 720 && output[0] == 1920 && output[1] == 1080);
    dimensions.render_w = dimensions.render_h = 0; dimensions.fsr = 0; dimensions.dlss_nr = 1;
    config_graphics_resolve(&dimensions, render, output, &aspect, &anchor);
    assert(render[0] == 1920 && render[1] == 1080 && output[0] == 1920 && output[1] == 1080);
    text=read_file(path);assert(strstr(text,"SmoothGradients = On"));free(text);
    g_config=initial;config_load();assert(config_editable().smooth_gradients==1);
    set_env("MHGU_RT16","0");
    g_config=initial;config_load();smooth=config_editable();
    assert(smooth.smooth_gradients==1 && g_config.smooth_gradients==0 && !config_live_smooth_gradients());
    smooth.volume=38;assert(config_save(&smooth,error,sizeof error) && !config_live_smooth_gradients());
    set_env("MHGU_RT16","");
    g_config=initial;config_load();assert(config_live_smooth_gradients()==1);
    smooth=config_editable();smooth.smooth_gradients=0;
    assert(config_save(&smooth,error,sizeof error) && !config_live_smooth_gradients());
    bad=smooth;bad.smooth_gradients=2;assert(!config_save(&bad,error,sizeof error));
    Config full = config_editable();
    full.hp.quests = HP_QUESTS_MAX;
    for (int i = 0; i < HP_QUESTS_MAX; i++) {
        full.hp.quest[i].quest = 1000 + i;
        for (int j = 0; j < 4; j++) full.hp.quest[i].scale[j] = 1 + j;
    }
    full.menu_theme = THEME_GOLD; full.menu_scale = 50;
    assert(config_save(&full, error, sizeof error));
    g_config = initial; config_load(); full = config_editable();
    assert(full.hp.quests == HP_QUESTS_MAX && full.hp.quest[255].quest == 1255);
    assert(full.hp.quest[255].scale[3] == 4 && full.menu_theme == THEME_GOLD && full.menu_scale == 50);
    assert(!config_restart_required(&full));
    assert(!config_parse(config_setting("MenuScale"), "49", &full));
    assert(!config_parse(config_setting("Theme"), "invalid", &full));
    config_graphics_fallback(GFX_FALLBACK_FSR4_GPU);
    assert(strstr(config_graphics_reason(), "GPU"));
    config_graphics_fallback(GFX_FALLBACK_FSR4_START);
    assert(strstr(config_graphics_reason(), "could not start"));
    config_graphics_fallback(GFX_FALLBACK_NONE);
    assert(config_graphics_reason() == NULL);
    // Round-trip every supported language, including the live game-language selection.
    Config translated = config_editable();
    config_set_game_language(MENU_LANG_CHINESE_TRADITIONAL);
    for (int language = MENU_LANG_ENGLISH; language <= MENU_LANG_GAME; ++language) {
        translated.language = language;
        assert(config_save(&translated, error, sizeof error));
        g_config = initial;
        config_load();
        assert(config_editable().language == language);
        assert(config_language() == (language == MENU_LANG_GAME ? MENU_LANG_CHINESE_TRADITIONAL : language));
    }
    config_set_game_language(MENU_LANG_ENGLISH);
    // Exercise the real help formatter, including joined lines, rather than a second formatter in Python.
    for (int setting = 0; setting < g_nsettings; ++setting) {
        char help[2048], key[128];
        snprintf(key, sizeof key, "%.*s", (int)strcspn(g_settings[setting].key, "|"), g_settings[setting].key);
        if (!config_help(key, help, sizeof help)) continue;
        for (char *line = help; *line;) {
            char *end = strchr(line, '\n');
            if (end) *end = 0;
            for (int language = MENU_LANG_SPANISH; language <= MENU_LANG_CHINESE_TRADITIONAL; ++language) {
                // A catalog hit returns static storage even when a brand/number needs no translation.
                if (settings_text(language, line) == line) {
                    fprintf(stderr, "Missing help translation (%d): %s\n", language, line);
                    assert(0);
                }
            }
            if (!end) break;
            line = end + 1;
        }
    }
    remove(path);
    puts("PASS: INI round trip, Auto/Desktop, environment overrides, comments/unknown keys, duplicate aliases, quest removal, invalid input, atomic write failure, live volume, seven languages and game-language following, localized help, icons, hot settings and restart classification, full quest table, themes and fallback reasons.");
    return 0;
}
