// Host-only regression: include the actual bridge and call its real generated ImGui entry points.
#include "rt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

_Alignas(16) static unsigned char guest_memory[4 << 20];
#undef G2H
#define G2H(a) ((void *)(guest_memory + (uint32_t)(a)))
#include "../runtime/forge_imgui.c"

static uint32_t allocated = 0x1000, callback_drawlist;
bool kernel_mapped(uint64_t addr, uint32_t size) {
    return addr && addr < sizeof guest_memory && size <= sizeof guest_memory - addr;
}
uint32_t hle_alloc(uint32_t size) {
    uint32_t at = allocated;
    allocated += (size + 63) & ~63u;
    assert(allocated < sizeof guest_memory);
    return at;
}
void rt_fatal_trace(void) { abort(); }
// Unrelated entry points retained by the Forge export section must never run in this host-only fixture.
uint32_t hle_register(const char *name, HostFn fn) { (void)name; (void)fn; abort(); }
uint32_t guest_sym(const char *name) { (void)name; abort(); }
void guest_call(Cpu *c, uint32_t a) { (void)c; (void)a; abort(); }
const char *romfs_host_path(const char *p) { (void)p; abort(); }
void forge_format(char *out, size_t n, const char *fmt, Cpu *c, int reg, uint32_t stk) {
    (void)out; (void)n; (void)fmt; (void)c; (void)reg; (void)stk; abort();
}
void forge_plugins_imgui(Cpu *c, int which, uint32_t a, uint32_t b, uint32_t d) {
    (void)c; (void)which; (void)a; (void)b; (void)d;
    ImDrawVert *v = G2H(LD32(callback_drawlist + 32));
    v[0].pos.x += 17;
}

int main(void) {
    ImGuiContext *ctx = igCreateContext(NULL);
    ImDrawList *list = ImDrawList_ImDrawList(igGetDrawListSharedData());
    ImDrawList__ResetForNewFrame(list);
    ImDrawList_PrimReserve(list, 0, 4);
    for (int i = 0; i < 4; i++) list->VtxBuffer.Data[i] = (ImDrawVert){.pos = {10+i, 20+i}};
    uint32_t object = m_out(list, T_ImDrawList), data = LD32(object + 32);
    assert(data && LD32(object + 24) == 4);
    ImDrawVert *g = G2H(data), *host_before = list->VtxBuffer.Data;
    g[1].pos.x += 50; g[2].pos.y -= 20;
    assert(m_in(object, T_ImDrawList) == list);
    assert(list->VtxBuffer.Data == host_before);
    assert(list->VtxBuffer.Data[1].pos.x == 61 && list->VtxBuffer.Data[2].pos.y == 2);
    puts("PASS nonzero guest Data, direct position edits, original host storage");

    callback_drawlist = object;
    Cpu cpu = {0};
    forge_imgui_run(&cpu, FORGE_IMGUI_FREE_RENDER);
    assert(list->VtxBuffer.Data[0].pos.x == 27);
    puts("PASS final direct vertex edit flushed by the actual callback boundary");

    g[3].pos.x += 30;
    cpu.r[0] = object; cpu.r[1] = 0; cpu.r[2] = 512;
    ig_ImDrawList_PrimReserve(&cpu);
    assert(list->VtxBuffer.Size == 516 && list->VtxBuffer.Data != host_before);
    assert(list->VtxBuffer.Data[3].pos.x == 43);
    data = LD32(object + 32); g = G2H(data);
    assert(data && LD32(object + 24) == 516 && g[3].pos.x == 43);
    puts("PASS actual bridge API flushes pending edits before host vector growth and refreshes the guest view");

    list->VtxBuffer.Data[0].pos.y = 999;
    g[0].pos.x = 123;
    hot_push();
    assert(list->VtxBuffer.Data[0].pos.x == 123 && list->VtxBuffer.Data[0].pos.y == 999);
    puts("PASS unrelated host vertex edits survive guest difference writeback");

    g[0].pos.x = -500;
    ctx->FrameCount++;
    ImDrawList__ResetForNewFrame(list);
    ImDrawList_PrimReserve(list, 0, 4);
    list->VtxBuffer.Data[0].pos.x = 300;
    hot_push();
    assert(list->VtxBuffer.Data[0].pos.x == 300);
    m_after(object);
    assert(((ImDrawVert *)G2H(LD32(object + 32)))[0].pos.x == 300);
    puts("PASS next-frame host vertices reject stale guest edits and refresh normally");

    uint32_t triangle = hle_alloc(3 * sizeof(ImVec2_c));
    ImVec2_c *p = G2H(triangle);
    p[0] = (ImVec2_c){10, 20}; p[1] = (ImVec2_c){110, 20}; p[2] = (ImVec2_c){60, 90};
    list->Flags = 0;
    int before = list->VtxBuffer.Size;
    cpu.r[0] = object; cpu.r[1] = triangle; cpu.r[2] = 3; cpu.r[3] = 0xffffffffu;
    ig_ImDrawList_AddConvexPolyFilled(&cpu);
    assert(list->VtxBuffer.Size == before + 3);
    for (int i = 0; i < 3; i++) {
        assert(list->VtxBuffer.Data[before+i].pos.x == p[i].x);
        assert(list->VtxBuffer.Data[before+i].pos.y == p[i].y);
    }
    before = list->VtxBuffer.Size;
    cpu.r[0] = object; cpu.r[1] = sizeof guest_memory - sizeof(ImVec2_c); cpu.r[2] = 3;
    ig_ImDrawList_AddConvexPolyFilled(&cpu);
    assert(list->VtxBuffer.Size == before);
    puts("PASS polygon bridge reads every guest point and rejects a truncated array");

    cpu.r[0] = object;
    ig_ImDrawList_destroy(&cpu);
    assert(ndraw_lists == 0);
    igDestroyContext(ctx);
    puts("PASS actual destroy bridge removes the vertex mirror");
}
