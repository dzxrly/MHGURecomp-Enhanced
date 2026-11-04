#include "../updater/paths.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(!update_kept("mods/Keyboard_Mouse_Input_Auto/romfs/keyboard_mouse_prompts.bin"));
    assert(!update_kept("mods/Keyboard_Mouse_Input_Auto/romfs/nativeNX/eng/table/TitleMsg_eng.gmd"));
    assert(update_kept("mods/Keyboard_Mouse_Input_Auto/.disabled"));
    assert(update_kept("mods/Keyboard_Mouse_Input_Auto/notes.txt"));
    assert(update_kept("mods/Keyboard_Mouse_Input_Auto_other/romfs/file"));
    assert(update_kept("mods/other/romfs/file"));
    assert(update_kept("mods/load_order.txt"));
    assert(update_kept("data/exefs/main") && update_kept("save/save_0/system"));
    assert(update_kept("mhgurecomp.ini") && update_kept("chat_box.txt"));
    assert(!update_kept("mhgurecomp.exe"));
    assert(!update_kept("nvngx_dlss.dll"));
    assert(!update_kept("dlss/nr/official/nvngx_dlssnr.dll"));
    assert(!update_kept("dlss/nr/sm89/nvngx_dlssnr.dll"));
    assert(!update_kept("graphics/nvidia/ngx/nr/sm89/nvngx_dlssnr.dll"));
    assert(!update_kept("graphics/intel/xess/libxess.dll"));
    assert(!update_kept("graphics/amd/fsr/amd_fidelityfx_loader_vk.dll"));
    assert(!update_kept("fonts/NotoSansCJKsc-Regular.otf"));
    assert(!update_kept("licenses/NotoSansCJK-OFL.txt"));
    assert(!update_kept("licenses/NotoSansCJK-NOTICE.txt"));
    puts("Updater: bundled prompts, fonts and DLSS runtimes replaceable; other mods, disabled state, ROM, saves and settings protected");
}
