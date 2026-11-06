// Regression coverage for rejecting unsupported input before any fixed-address hooks.
#include "rom_version.h"
#include "rom_version_data.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

static void module(const char *root, unsigned i, bool corrupt_tail, bool short_file) {
    char path[1024];
    snprintf(path, sizeof path, "%s/exefs/%s", root, rom_modules[i].name);
    unsigned char header[0x100] = {'N', 'S', 'O', '0'};
    for (unsigned j = 0; j < 32; j++) {
        char byte[3] = {rom_modules[i].build_id[j * 2], rom_modules[i].build_id[j * 2 + 1], 0};
        header[0x40 + j] = (unsigned char)strtoul(byte, NULL, 16);
    }
    if (corrupt_tail) header[0x5f] ^= 1;
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(header, 1, short_file ? 0x48 : sizeof header, f) == (short_file ? 0x48 : sizeof header));
    fclose(f);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    char path[1024], why[3000];
    MKDIR(argv[1]);
    snprintf(path, sizeof path, "%s/exefs", argv[1]); MKDIR(path);
    module(argv[1], 0, false, true);
    assert(!rom_version_check(argv[1], why, sizeof why) && strstr(why, "invalid NSO"));
    module(argv[1], 0, true, false);
    assert(!rom_version_check(argv[1], why, sizeof why) && strstr(why, "Expected Build ID"));
    module(argv[1], 0, false, false);
    module(argv[1], 1, true, false);
    assert(!rom_version_check(argv[1], why, sizeof why) && strstr(why, "exefs/sdk"));
    module(argv[1], 1, false, false);
    assert(!rom_version_check(argv[1], why, sizeof why) && strstr(why, "base romfs"));
    const char *dirs[] = {"romfs", "romfs/NativeNX", "romfs/NativeNX/system", "romfs/NativeNX/system/app_shader"};
    for (unsigned i = 0; i < sizeof dirs / sizeof *dirs; i++) {
        snprintf(path, sizeof path, "%s/%s", argv[1], dirs[i]); MKDIR(path);
    }
    snprintf(path, sizeof path, "%s/romfs/NativeNX/system/app_shader/AppShaderPackage.mfx", argv[1]);
    FILE *f = fopen(path, "wb"); assert(f);
    fputs("modified resource", f); fclose(f);
    assert(!rom_version_check(argv[1], why, sizeof why) && strstr(why, "Expected SHA-256"));
    assert(remove(path) == 0);
    snprintf(path, sizeof path, "%s/romfs/NativeNX/system/app_shader/appshaderpackage.mfx", argv[1]);
    f = fopen(path, "wb"); assert(f);
    fputs("incorrectly capitalized resource", f); fclose(f);
    assert(!rom_version_check(argv[1], why, sizeof why) && strstr(why, "incorrect filename capitalization"));
    puts("ROM guard: module identities, missing romfs, alternate NativeNX and case-sensitive resource names");
    return 0;
}
