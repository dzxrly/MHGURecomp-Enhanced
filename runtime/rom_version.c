#include "rom_version.h"
#include "rom_version_data.h"
#include "../updater/updater.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// The virtual RomFS accepts alternate capitalization for nativeNX only.
static bool resource_path(const char *root, const char *relative, char *path, size_t size) {
    int used = snprintf(path, size, "%s/romfs", root);
    if (used < 0 || (size_t)used >= size) return false;
    for (const char *part = relative; *part;) {
        size_t length = strcspn(part, "/");
        DIR *dir = opendir(path);
        if (!dir) return false;
        char match[256] = "";
        for (const struct dirent *e; (e = readdir(dir));) {
            if (strlen(e->d_name) != length || length >= sizeof match) continue;
            if (!strncmp(e->d_name, part, length)) {
                memcpy(match, e->d_name, length + 1);
                break;
            }
            if (part == relative && !strncasecmp(e->d_name, part, length))
                memcpy(match, e->d_name, length + 1);
        }
        closedir(dir);
        if (!*match || (size_t)used + 1 + length >= size) return false;
        path[used++] = '/';
        memcpy(path + used, match, length + 1);
        used += (int)length;
        part += length + (part[length] == '/');
    }
    return true;
}

bool rom_version_check(const char *root, char *why, size_t size) {
    char path[1600];
    for (size_t i = 0; i < sizeof rom_modules / sizeof *rom_modules; i++) {
        const RomModule *m = &rom_modules[i];
        int n = snprintf(path, sizeof path, "%s/exefs/%s", root, m->name);
        if (n < 0 || (size_t)n >= sizeof path) {
            snprintf(why, size, "ROM path is too long: %s", root);
            return false;
        }
        FILE *f = fopen(path, "rb");
        unsigned char header[0x100];
        bool ok = f && fread(header, 1, sizeof header, f) == sizeof header && !memcmp(header, "NSO0", 4);
        if (f) fclose(f);
        char actual[65] = "missing, truncated or invalid NSO";
        if (ok) {
            for (unsigned j = 0; j < 32; j++) snprintf(actual + j * 2, 3, "%02x", header[0x40 + j]);
            ok = !strcmp(actual, m->build_id);
        }
        if (!ok) {
            snprintf(why, size, "ROM compatibility warning: startup stopped before loading game code.\n\n"
                     "Supported: Nintendo Switch MHGU 1.4.0.\nFile: %s\nExpected Build ID: %s\nFound: %s\n\n"
                     "Export exefs with the 1.4.0 update applied, and merge its romfs over the base game.",
                     path, m->build_id, actual);
            return false;
        }
    }
    for (size_t i = 0; i < sizeof rom_resources / sizeof *rom_resources; i++) {
        const RomResource *r = &rom_resources[i];
        int n = snprintf(path, sizeof path, "%s/romfs/%s", root, r->name);
        if (n < 0 || (size_t)n >= sizeof path) {
            snprintf(why, size, "ROM path is too long: %s", root);
            return false;
        }
        char resolved[sizeof path];
        FILE *f = resource_path(root, r->name, resolved, sizeof resolved) ? fopen(resolved, "rb") : NULL;
        char actual[65] = "missing, unreadable, or incorrect filename capitalization";
        bool ok = false;
        if (f) {
            Sha256 hash;
            sha256_init(&hash);
            unsigned char buf[65536];
            size_t got;
            uint64_t total = 0;
            while ((got = fread(buf, 1, sizeof buf, f))) {
                sha256_update(&hash, buf, got);
                total += got;
            }
            bool error = ferror(f) != 0;
            fclose(f);
            sha256_hex(&hash, actual);
            ok = !error && total == r->size && !strcmp(actual, r->sha256);
        }
        if (!ok) {
            snprintf(why, size, "ROM compatibility warning: startup stopped before installing fixed-address hooks.\n\n"
                     "The base romfs does not match the supported Switch MHGU 1.4.0 reference.\nFile: %s\n"
                     "Expected SHA-256: %s\nFound: %s\n\n"
                     "The files may be from another version, incompletely merged, damaged or modified. "
                     "Restore the original merged romfs; put modifications in mods/ instead. "
                     "These resource fingerprints are not a whole-ROM integrity check.", path, r->sha256, actual);
            return false;
        }
    }
    if (size) *why = 0;
    return true;
}
