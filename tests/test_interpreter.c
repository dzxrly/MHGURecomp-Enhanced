// ARM unsigned parallel arithmetic: expected register/GE values checked with Unicorn 2.1.4.
#include "interp.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void rt_dispatch(Cpu *c) { (void)c; abort(); }
void rt_svc(Cpu *c, uint32_t imm, uint32_t pc) { (void)c; (void)imm; (void)pc; abort(); }
void rt_bkpt(Cpu *c, uint32_t imm, uint32_t pc) { (void)c; (void)imm; (void)pc; abort(); }
void rt_udf(Cpu *c, uint32_t pc) { (void)c; (void)pc; abort(); }
void rt_thumb(Cpu *c, uint32_t pc) { (void)c; (void)pc; abort(); }
uint64_t rt_ticks(void) { abort(); }
void rt_fatal_trace(void) { abort(); }
uint32_t kernel_heap_size(void) { return 0; }
uint8_t g_fnhook[0x03000000u >> 5];
bool fnhook_entry_word(uint32_t pc, uint32_t *word) { (void)pc; (void)word; return false; }
uint64_t plat_now_ns(void) { return 0; }
bool kernel_mapped(uint64_t addr, uint32_t size) { (void)addr; (void)size; return false; }
void forge_guest_fault(uint32_t pc, uint32_t addr, int size, int write, const char *why) {
    (void)pc; (void)addr; (void)size; (void)write; (void)why; abort();
}

int main(void) {
    const struct { uint32_t word, a, b, result, ge; } cases[] = {
        {0xe6600ff1u, 0x10203040u, 0x20100850u, 0x00102800u, 0},
        {0xe6600ff1u, 0xffffffffu, 0x01010101u, 0xfefefefeu, 0},
        {0xe6500f91u, 0x10203040u, 0x20100850u, 0x30303890u, 0},
        {0xe6500f91u, 0xffffffffu, 0x01010101u, 0x00000000u, 15},
        {0xe6600f91u, 0x10203040u, 0x20100850u, 0x30303890u, 0},
        {0xe6600f91u, 0xffffffffu, 0x01010101u, 0xffffffffu, 0},
        {0xe6700ff1u, 0x10203040u, 0x20100850u, 0xf80814f8u, 0},
        {0xe6700ff1u, 0xffffffffu, 0x01010101u, 0x7f7f7f7fu, 0}
    };
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
        Cpu c = {0}; int link;
        c.r[0] = cases[i].a; c.r[1] = cases[i].b;
        assert(interp_exec(&c, 0x200000, cases[i].word, &link) == 0x200004);
        assert(!link && c.r[0] == cases[i].result && c.ge == cases[i].ge);
    }
    puts("interpreter: UADD8/UQADD8/UQSUB8/UHSUB8 overflow, saturation and GE flags passed");
}
