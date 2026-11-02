// Exercise real ImDrawList batching, including a callback sharing the UI's final command.
#include "forge_canvas.h"
#include "imgui_internal.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static bool near(float a, float b) { return std::fabs(a - b) < .0001f; }
static void position(ImVec2 p, float x, float y) { assert(near(p.x, x) && near(p.y, y)); }
static void clip(ImVec4 r, float x, float y, float z, float w) {
    assert(near(r.x, x) && near(r.y, y) && near(r.z, z) && near(r.w, w));
}

struct Fixture {
    ImDrawList list{ImGui::GetDrawListSharedData()};
    Fixture() {
        list._ResetForNewFrame();
        list.Flags = ImDrawListFlags_None;
        list.PushClipRect(ImVec2(0, 0), ImVec2(800, 600));
    }
    void rect(float x = 10, float y = 20) {
        list.AddRectFilled(ImVec2(x, y), ImVec2(x + 30, y + 40), IM_COL32_WHITE);
    }
};

static void merged_command() {
    Fixture f;
    f.rect();
    auto before = forge_canvas::checkpoint(&f.list);
    std::vector<ImDrawVert> ui(f.list.VtxBuffer.begin(), f.list.VtxBuffer.end());
    f.rect(1, 2);
    assert(f.list.CmdBuffer.Size == before.commands);
    ImDrawCmd old = f.list.CmdBuffer.back();
    old.ElemCount = before.elements;
    std::vector<ImDrawIdx> indices(f.list.IdxBuffer.begin(), f.list.IdxBuffer.end());

    forge_canvas::transform(&f.list, before, forge_canvas::compute(800, 600, 4.f / 3, 400, 300));
    assert(f.list.CmdBuffer.Size == before.commands + 1);
    assert(!std::memcmp(&old, &f.list.CmdBuffer[0], sizeof old));
    assert(!std::memcmp(ui.data(), f.list.VtxBuffer.Data, ui.size() * sizeof ui[0]));
    assert(!std::memcmp(indices.data(), f.list.IdxBuffer.Data, indices.size() * sizeof indices[0]));
    const ImDrawCmd &plugin = f.list.CmdBuffer[1];
    assert(plugin.ElemCount == 6 && plugin.IdxOffset == old.IdxOffset + old.ElemCount);
    assert(plugin.VtxOffset == old.VtxOffset);
    assert(plugin.IdxOffset + plugin.ElemCount == (unsigned)f.list.IdxBuffer.Size);
    position(f.list.VtxBuffer[before.vertices].pos, 2, 4);
    puts("PASS shared UI/plugin command splits without changing UI vertices, commands or indices");
}

static void independent_command() {
    Fixture f;
    f.rect();
    auto before = forge_canvas::checkpoint(&f.list);
    ImDrawCmd old = f.list.CmdBuffer.back();
    f.list.PushClipRect(ImVec2(10, 20), ImVec2(200, 120));
    f.rect(50, 60);
    f.list.PopClipRect();  // creates the usual empty tail command
    int commands = f.list.CmdBuffer.Size;

    forge_canvas::transform(&f.list, before, forge_canvas::compute(800, 600, 4.f / 3, 400, 300));
    assert(f.list.CmdBuffer.Size == commands);
    assert(!std::memcmp(&old, &f.list.CmdBuffer[0], sizeof old));
    const ImDrawCmd &plugin = f.list.CmdBuffer[1];
    clip(plugin.ClipRect, 20, 40, 400, 240);
    assert(plugin.IdxOffset == old.ElemCount && plugin.ElemCount == 6);
    assert(plugin.IdxOffset + plugin.ElemCount == (unsigned)f.list.IdxBuffer.Size);
    position(f.list.VtxBuffer[before.vertices].pos, 100, 120);
    puts("PASS independent plugin command and clipping preserve earlier UI");
}

static void empty_tail() {
    Fixture f;
    f.rect();
    ImDrawCmd old = f.list.CmdBuffer.back();
    f.list.AddDrawCmd();
    auto before = forge_canvas::checkpoint(&f.list);
    assert(before.elements == 0);
    f.rect(3, 4);
    int commands = f.list.CmdBuffer.Size;
    forge_canvas::transform(&f.list, before, forge_canvas::compute(800, 600, 4.f / 3, 400, 300));
    assert(f.list.CmdBuffer.Size == commands);
    assert(!std::memcmp(&old, &f.list.CmdBuffer[0], sizeof old));
    assert(f.list.CmdBuffer[1].IdxOffset == 6 && f.list.CmdBuffer[1].ElemCount == 6);
    position(f.list.VtxBuffer[before.vertices].pos, 6, 8);
    puts("PASS an empty tail becomes plugin geometry without an extra split");
}

static void restored_empty_tail() {
    Fixture f;
    f.rect();
    auto before = forge_canvas::checkpoint(&f.list);
    ImDrawCmd old = f.list.CmdBuffer.back();
    f.list.PushClipRect(ImVec2(0, 0), ImVec2(1600, 1200));
    f.rect(200, 300);
    f.list.PopClipRect();
    ImDrawCmd empty = f.list.CmdBuffer.back();
    assert(!empty.ElemCount && !empty.UserCallback);
    forge_canvas::transform(&f.list, before, forge_canvas::compute(800, 600, 4.f / 3, 1600, 1200));
    assert(!std::memcmp(&old, &f.list.CmdBuffer[0], sizeof old));
    assert(!std::memcmp(&empty, &f.list.CmdBuffer.back(), sizeof empty));
    clip(f.list.CmdBuffer[1].ClipRect, 0, 0, 800, 600);
    position(f.list.VtxBuffer[before.vertices].pos, 100, 150);
    puts("PASS restored empty tail retains physical clipping for subsequent host UI");
}

static void removed_empty_tail() {
    Fixture f;
    f.rect();
    ImDrawCmd old = f.list.CmdBuffer.back();
    f.list.PushClipRect(ImVec2(1, 2), ImVec2(700, 500));
    auto before = forge_canvas::checkpoint(&f.list);
    assert(!before.elements && before.commands == 2);
    std::vector<ImDrawVert> ui(f.list.VtxBuffer.begin(), f.list.VtxBuffer.end());
    f.list.PopClipRect();  // ImGui removes the old empty command and merges with the UI's command
    assert(f.list.CmdBuffer.Size == 1);
    f.rect(1, 2);
    std::vector<ImDrawIdx> indices(f.list.IdxBuffer.begin(), f.list.IdxBuffer.end());
    forge_canvas::transform(&f.list, before, forge_canvas::compute(800, 600, 4.f / 3, 400, 300));
    assert(f.list.CmdBuffer.Size == 2);
    assert(!std::memcmp(&old, &f.list.CmdBuffer[0], sizeof old));
    assert(!std::memcmp(ui.data(), f.list.VtxBuffer.Data, ui.size() * sizeof ui[0]));
    assert(!std::memcmp(indices.data(), f.list.IdxBuffer.Data, indices.size() * sizeof indices[0]));
    assert(f.list.CmdBuffer[1].IdxOffset == 6 && f.list.CmdBuffer[1].ElemCount == 6);
    position(f.list.VtxBuffer[before.vertices].pos, 2, 4);
    puts("PASS a removed empty checkpoint tail still splits at the original index boundary");
}

static void new_and_reset_lists() {
    Fixture f;
    f.list.PushClipRect(ImVec2(0, 0), ImVec2(500, 500));
    f.rect();
    auto map = forge_canvas::compute(1000, 1000, 2, 500, 500);
    assert(near(map.sx, 2) && near(map.sy, 1) && near(map.ox, 0) && near(map.oy, 250));
    forge_canvas::transform(&f.list, {}, map);
    position(f.list.VtxBuffer[0].pos, 20, 270);
    clip(f.list.CmdBuffer.back().ClipRect, 0, 250, 1000, 750);
    assert(f.list.CmdBuffer.back().IdxOffset == 0 && f.list.CmdBuffer.back().ElemCount == 6);

    f.list._ResetForNewFrame();
    f.list.Flags = ImDrawListFlags_None;
    f.list.PushClipRect(ImVec2(-200, -100), ImVec2(400, 900));
    f.rect();
    map = forge_canvas::compute(1000, 500, 1, 250, 500);
    assert(near(map.sx, 2) && near(map.sy, 1) && near(map.ox, 250) && near(map.oy, 0));
    forge_canvas::transform(&f.list, {}, map);
    position(f.list.VtxBuffer[0].pos, 270, 20);
    clip(f.list.CmdBuffer.back().ClipRect, 0, 0, 1000, 500);
    assert(f.list.CmdBuffer.back().IdxOffset == 0 && f.list.CmdBuffer.back().ElemCount == 6);
    puts("PASS new/reset lists, letterbox/pillarbox, anisotropic mapping and window clipping");
}

int main() {
    ImGuiContext *context = ImGui::CreateContext();
    merged_command();
    independent_command();
    empty_tail();
    restored_empty_tail();
    removed_empty_tail();
    new_and_reset_lists();
    ImGui::DestroyContext(context);
}
