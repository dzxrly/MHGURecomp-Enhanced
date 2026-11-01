// Exercise the real cheat VM and persistence in an isolated guest-memory page.
#include "rt.h"
#include "cheats.h"
#include "mods.h"
#include "hle/input.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#endif

Module g_modules[MAX_MODULES];
int g_nmodules = 1;
volatile uint64_t g_present_count;
static unsigned interpreted;
bool kernel_mapped(uint64_t a, uint32_t n) { return a >= MAIN_BASE && a + n <= MAIN_BASE + 0x10000; }
uint32_t dispatch_function_at(uint32_t a, uint32_t *next) { (void)a; *next = MAIN_BASE + 0x100; return MAIN_BASE; }
int dispatch_interpret(uint32_t entry, uint32_t end) { (void)entry; (void)end; interpreted++; return 1; }
void interp_add_range(uint32_t lo, uint32_t hi) { (void)lo; (void)hi; }
void input_get(uint64_t *b, int32_t s[4]) { *b = 0; memset(s, 0, 4 * sizeof *s); }
static void tick(void) { g_present_count++; cheats_frame(); }

int main(int argc, char **argv) {
    assert(argc == 2 || argc == 3);
#ifdef _WIN32
    assert(VirtualAlloc(G2H(MAIN_BASE), 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == G2H(MAIN_BASE));
    _mkdir(argv[1]);
    _putenv_s("MHGU_CHEATS_DIR", argv[1]);
#else
    assert(mmap(G2H(MAIN_BASE), 0x10000, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == G2H(MAIN_BASE));
    mkdir(argv[1], 0755);
    setenv("MHGU_CHEATS_DIR", argv[1], 1);
#endif
    g_modules[0] = (Module){.name = "test", .base = MAIN_BASE, .text_size = 0x1000, .ro_off = 0x1000};
    char path[1024], off[1024], error[1200];
    snprintf(path, sizeof path, "%s/test.txt", argv[1]);
    snprintf(off, sizeof off, "%s/_off.txt", argv[1]);
    ST32(MAIN_BASE, 0xe1a00000);
    if (argc == 2) {
        remove(off);
        FILE *f = fopen(path, "wb"); assert(f);
        fputs("[A]\n04000000 00000000 11111111\n[B]\n04000000 00000000 22222222\n"
              "[Money]\n04000000 00002000 00000063\n{Master}\n04000000 00003000 00000077\n", f);
        assert(!fclose(f));
    }
    cheats_init();
    assert(cheats_count() == 4 && cheats_master(3) && cheats_on(3));
    assert(LD32(MAIN_BASE + 0x3000) == 0x77);
    if (argc == 3) {
        assert(!cheats_on(0) && !cheats_on(1) && !cheats_on(2));
        assert(LD32(MAIN_BASE) == 0xe1a00000 && LD32(MAIN_BASE + 0x2000) == 0);
        remove(path); remove(off);
        puts("cheats: persisted disabled entries reloaded; master remains on");
        return 0;
    }
    assert(LD32(MAIN_BASE) == 0x22222222 && LD32(MAIN_BASE + 0x2000) == 99 && interpreted);
    bool on[] = {true, false, true, false};
    assert(cheats_set(on, 4, error, sizeof error)); tick();
    assert(LD32(MAIN_BASE) == 0x11111111 && cheats_on(3));
    on[0] = false;
    assert(cheats_set(on, 4, error, sizeof error)); tick();
    assert(LD32(MAIN_BASE) == 0xe1a00000);
    on[1] = true;
    assert(cheats_set(on, 4, error, sizeof error)); tick();
    assert(LD32(MAIN_BASE) == 0x22222222);
    on[0] = true; on[1] = false;
    assert(cheats_set(on, 4, error, sizeof error)); tick();
    assert(LD32(MAIN_BASE) == 0x11111111);
    on[1] = true;
    assert(cheats_set(on, 4, error, sizeof error)); tick();
    on[0] = on[1] = on[2] = false;
    assert(cheats_set(on, 4, error, sizeof error)); tick();
    assert(LD32(MAIN_BASE) == 0xe1a00000 && LD32(MAIN_BASE + 0x2000) == 99);
    assert(cheats_on(3) && LD32(MAIN_BASE + 0x3000) == 0x77);
    puts("cheats: shared code restored in both orders and one save; data changes persist; master protected");
    return 0;
}
