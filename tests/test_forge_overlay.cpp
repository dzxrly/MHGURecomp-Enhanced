// Exercise the actual FreeRender integration with headless ImGui frames, without a game or Vulkan device.
#define MHGU_NULL_BACKEND
#define SDL_STATIC_LIB
#include "../runtime/overlay.cpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdlib>

extern "C" {
uint32_t g_render_size[2];
float g_render_aspect;
// These unrelated overlay exports survive MinGW section GC; this test must never call them.
SDL_Window *ui_window(void) { std::abort(); }
bool SDLCALL SDL_ShowCursor(void) { std::abort(); }
bool SDLCALL SDL_HideCursor(void) { std::abort(); }
SDL_WindowFlags SDLCALL SDL_GetWindowFlags(SDL_Window *) { std::abort(); }
Uint64 SDLCALL SDL_GetTicksNS(void) { std::abort(); }
}
void settings_panel_request_close(void) { std::abort(); }
bool settings_panel_dirty(void) { std::abort(); }

static bool near(float a, float b) { return std::fabs(a - b) < .001f; }
static void position(ImVec2 p, float x, float y) { assert(near(p.x, x) && near(p.y, y)); }
static void clip(ImVec4 r, float x, float y, float z, float w) {
    assert(near(r.x, x) && near(r.y, y) && near(r.z, z) && near(r.w, w));
}

struct Case {
    uint32_t width, height, guest_width, guest_height;
    float mouse_y, delta_y, click_y, font_scale;
    ImVec2 background, foreground;
    ImVec4 picture;
    bool existing_foreground;
    float host_font_size;
    int background_vertex, foreground_vertex;
};
static Case *current;
static int calls;

extern "C" void forge_imgui_run(void *, int which) {
    assert(which == FORGE_IMGUI_FREE_RENDER);
    ++calls;
    Case &test = *current;
    ImGuiContext &g = *ImGui::GetCurrentContext();
    ImGuiIO &io = g.IO;
    position(io.DisplaySize, (float)test.guest_width, (float)test.guest_height);
    position(g.Viewports[0]->Size, (float)test.guest_width, (float)test.guest_height);
    position(g.Viewports[0]->WorkSize, (float)test.guest_width, (float)test.guest_height);
    position(g.Viewports[0]->WorkPos, 0, 0);
    position(io.MousePos, 300, test.mouse_y);
    position(io.MouseDelta, 10, test.delta_y);
    for (int i = 0; i < IM_ARRAYSIZE(io.MouseClickedPos); ++i)
        position(io.MouseClickedPos[i], 200, test.click_y);
    assert(near(ImGui::GetFontSize(), test.host_font_size / test.font_scale));
    assert(near(g.Style.FontScaleDpi, 1.5f / test.font_scale));

    ImDrawList *bg = ImGui::GetBackgroundDrawList();
    test.background_vertex = bg->VtxBuffer.Size;
    bg->AddRectFilled(ImVec2(100, 120), ImVec2(140, 160), IM_COL32(255, 0, 0, 255));
    ImDrawList *fg = ImGui::GetForegroundDrawList();
    test.foreground_vertex = fg->VtxBuffer.Size;
    fg->AddRectFilled(ImVec2(200, 100), ImVec2(240, 140), IM_COL32(0, 0, 255, 255));
}

static const ImDrawCmd &command_for(const ImDrawList &list, int vertex) {
    for (const ImDrawCmd &cmd : list.CmdBuffer)
        for (unsigned i = 0; i < cmd.ElemCount; ++i)
            if ((int)(cmd.VtxOffset + list.IdxBuffer[cmd.IdxOffset + i]) == vertex) return cmd;
    assert(false && "Draw command for the real primitive was not found");
    return list.CmdBuffer[0];
}

static void run(Case test) {
    ImGuiContext *context = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2((float)test.width, (float)test.height);
    io.DeltaTime = 1.f / 60;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    ImGui::GetStyle().FontScaleDpi = 1.5f;
    g_render_size[0] = test.guest_width; g_render_size[1] = test.guest_height;
    g_render_aspect = 2;
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(40, 40));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Existing host/F1 window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImDrawList *host_window = ImGui::GetWindowDrawList();
    host_window->AddRectFilled(ImVec2(50, 60), ImVec2(100, 110), IM_COL32_WHITE);
    ImGui::End();
    ImDrawList *bg = ImGui::GetBackgroundDrawList();
    bg->AddRectFilled(ImVec2(30, 40), ImVec2(80, 90), IM_COL32_WHITE);
    ImDrawList *fg = nullptr;
    if (test.existing_foreground) {
        fg = ImGui::GetForegroundDrawList();
        fg->AddRectFilled(ImVec2(100, 80), ImVec2(150, 120), IM_COL32_WHITE);
    }
    std::vector<ImDrawVert> host_window_vertices(host_window->VtxBuffer.begin(), host_window->VtxBuffer.end());
    std::vector<ImDrawVert> host_bg_vertices(bg->VtxBuffer.begin(), bg->VtxBuffer.end());
    std::vector<ImDrawVert> host_fg_vertices;
    if (fg) host_fg_vertices.assign(fg->VtxBuffer.begin(), fg->VtxBuffer.end());

    ImGuiViewportP *viewport = context->Viewports[0];
    viewport->WorkPos = ImVec2(3, 7); viewport->WorkSize = ImVec2(test.width - 6.f, test.height - 14.f);
    io.MousePos = ImVec2(600, 400); io.MouseDelta = ImVec2(20, 40);
    for (ImVec2 &click : io.MouseClickedPos) click = ImVec2(400, 300);
    ImFont *host_font = ImGui::GetFont();
    test.host_font_size = ImGui::GetFontSize();
    float font_base = context->FontSizeBase;
    int font_stack = context->FontStack.Size;
    current = &test;
    int before_calls = calls;
    forge_free_render(nullptr, test.width, test.height);
    assert(calls == before_calls + 1);

    position(io.DisplaySize, (float)test.width, (float)test.height);
    position(viewport->Size, (float)test.width, (float)test.height);
    position(viewport->WorkPos, 3, 7);
    position(viewport->WorkSize, test.width - 6.f, test.height - 14.f);
    position(io.MousePos, 600, 400); position(io.MouseDelta, 20, 40);
    for (ImVec2 click : io.MouseClickedPos) position(click, 400, 300);
    assert(ImGui::GetFont() == host_font && near(ImGui::GetFontSize(), test.host_font_size));
    assert(near(context->FontSizeBase, font_base) && near(context->Style.FontScaleDpi, 1.5f));
    assert(context->FontStack.Size == font_stack);
    assert(!std::memcmp(host_window_vertices.data(), host_window->VtxBuffer.Data,
                        host_window_vertices.size() * sizeof(ImDrawVert)));
    assert(!std::memcmp(host_bg_vertices.data(), bg->VtxBuffer.Data, host_bg_vertices.size() * sizeof(ImDrawVert)));
    fg = ImGui::GetForegroundDrawList();
    if (!host_fg_vertices.empty())
        assert(!std::memcmp(host_fg_vertices.data(), fg->VtxBuffer.Data, host_fg_vertices.size() * sizeof(ImDrawVert)));
    position(bg->VtxBuffer[test.background_vertex].pos, test.background.x, test.background.y);
    position(fg->VtxBuffer[test.foreground_vertex].pos, test.foreground.x, test.foreground.y);
    const ImVec4 &picture = test.picture;
    clip(command_for(*bg, test.background_vertex).ClipRect, picture.x, picture.y, picture.z, picture.w);
    clip(command_for(*fg, test.foreground_vertex).ClipRect, picture.x, picture.y, picture.z, picture.w);

    // A host notice/chat drawn afterward uses physical pixels, including the bottom letterbox band.
    int notice = fg->VtxBuffer.Size;
    fg->AddRectFilled(ImVec2(100, test.height - 30.f), ImVec2(140, test.height - 10.f), IM_COL32_WHITE);
    position(fg->VtxBuffer[notice].pos, 100, test.height - 30.f);
    clip(command_for(*fg, notice).ClipRect, 0, 0, (float)test.width, (float)test.height);
    ImGui::Render();
    assert(ImGui::GetDrawData()->Valid);
    ImGui::DestroyContext(context);
}

int main() {
    run({2000, 1000, 1000, 500, 200, 20, 150, 2, {200, 240}, {400, 200}, {0, 0, 2000, 1000}, false});
    puts("PASS actual FreeRender restores IO/viewport/font and maps new BG/FG without moving host UI");
    run({1600, 1000, 800, 600, 225, 30, 150, 4.f / 3, {200, 260}, {400, 700.f / 3}, {0, 100, 1600, 900}, true});
    puts("PASS actual FreeRender handles letterbox, anisotropic input/geometry and later host foreground clipping");
}
