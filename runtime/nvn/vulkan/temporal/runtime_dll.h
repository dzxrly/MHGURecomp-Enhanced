#pragma once

#ifdef _WIN32
#include <stdbool.h>
#include <wchar.h>
#include <windows.h>

// Absolute paths keep runtime selection independent of the working directory.
static inline bool graphics_runtime_path(wchar_t *path, size_t capacity, const wchar_t *relative) {
    DWORD length = GetModuleFileNameW(NULL, path, (DWORD)capacity);
    if (!length || length >= capacity) return false;
    wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash || (size_t)(slash + 1 - path) + wcslen(relative) >= capacity) return false;
    wcscpy(slash + 1, relative);
    return true;
}
#endif
