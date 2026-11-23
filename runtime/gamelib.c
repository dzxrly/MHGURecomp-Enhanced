// Loads the recompiled game code library (runtime/gamelib.h).
#include "gamelib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "rt.h"

static const GameLibTables *g_tables;

#ifdef _WIN32
// A partial extraction can leave the PE headers intact but cut off a section's on-disk data.
// Check only after LoadLibrary fails so that its normal DLL search and success paths stay authoritative.
static bool pe_file_truncated(FILE *file) {
    if (_fseeki64(file, 0, SEEK_END)) return false;
    int64_t size = _ftelli64(file);
    if (size < 0 || _fseeki64(file, 0, SEEK_SET)) return false;
    IMAGE_DOS_HEADER dos;
    if (size < (int64_t)sizeof dos) return true;
    if (fread(&dos, sizeof dos, 1, file) != 1 || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < (LONG)sizeof dos) return false;

    DWORD signature;
    IMAGE_FILE_HEADER header;
    int64_t coff_end = (int64_t)dos.e_lfanew + sizeof signature + sizeof header;
    if (coff_end > size) return true;
    if (_fseeki64(file, dos.e_lfanew, SEEK_SET) ||
        fread(&signature, sizeof signature, 1, file) != 1 || signature != IMAGE_NT_SIGNATURE ||
        fread(&header, sizeof header, 1, file) != 1) return false;

    int64_t section_start = coff_end + header.SizeOfOptionalHeader;
    if (section_start + header.NumberOfSections * (int64_t)sizeof(IMAGE_SECTION_HEADER) > size) return true;
    if (_fseeki64(file, section_start, SEEK_SET)) return false;
    for (unsigned i = 0; i < header.NumberOfSections; i++) {
        IMAGE_SECTION_HEADER section;
        if (fread(&section, sizeof section, 1, file) != 1) return false;
        if (section.SizeOfRawData && (int64_t)section.PointerToRawData + section.SizeOfRawData > size) return true;
    }
    return false;
}
#endif

bool gamelib_load(char *why, size_t why_size) {
    if (g_tables) return true;
    char path[1100];
    const char *env = getenv("MHGU_GAME_LIB");
    if (env && *env) {
        snprintf(path, sizeof path, "%s", env);
    } else {
        char exe[1024];
        if (!plat_exe_path(exe, sizeof exe)) exe[0] = 0;
        char *slash = strrchr(exe, '/'), *bs = strrchr(exe, '\\');
        if (bs > slash) slash = bs;
        if (slash) slash[1] = 0; else exe[0] = 0;
        snprintf(path, sizeof path, "%s%s", exe, GAMELIB_NAME);
    }
#ifdef _WIN32
    HMODULE h = LoadLibraryA(path);
    if (!h) {
        DWORD error = GetLastError();
        FILE *file = fopen(path, "rb");
        bool truncated = file && pe_file_truncated(file);
        if (file) fclose(file);
        if (truncated)
            snprintf(why, why_size, "%s is incomplete: download and unpack the Windows release again", path);
        else
            snprintf(why, why_size, "can't load %s (error %lu): run `make gen` and `make` first", path, error);
        return false;
    }
    GameLibEntry entry = (GameLibEntry)(void *)GetProcAddress(h, GAMELIB_ENTRY);
#else
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) { snprintf(why, why_size, "%s", dlerror()); return false; }
    GameLibEntry entry = (GameLibEntry)dlsym(h, GAMELIB_ENTRY);
#endif
    if (!entry) { snprintf(why, why_size, "%s: not game code", path); return false; }
    const GameLibTables *t = entry();
    if (!t || t->abi != GAMELIB_ABI || !t->id || strcmp(t->id, GAMELIB_ID)) {
        snprintf(why, why_size, "%s was built for another version of the runtime: rebuild it (make)", path);
        return false;
    }
    g_tables = t;
    LOG("[gamelib] %s (%u + %u functions)\n", path, *t->main_count, *t->sdk_count);
    return true;
}

const GameLibTables *gamelib_tables(void) { return g_tables; }
