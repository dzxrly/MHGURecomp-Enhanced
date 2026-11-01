// Vector controller glyphs: independent of fonts, textures and platform assets.
#pragma once
#include "imgui.h"
namespace SettingsIcons {
enum { Confirm, Back, Save, Undo, Left, Right, Dpad };
inline float key_width(const char *label, float size) {
    return ImGui::CalcTextSize(label).x + size * .7f;
}
inline void key(ImDrawList *d, ImVec2 p, float size, const char *label) {
    float width = key_width(label, size);
    d->AddRectFilled(p, ImVec2(p.x + width, p.y + size), IM_COL32(23,34,48,255), size * .15f);
    d->AddRect(p, ImVec2(p.x + width, p.y + size), IM_COL32(160,184,211,255), size * .15f);
    ImVec2 text = ImGui::CalcTextSize(label);
    d->AddText(ImVec2(p.x + (width - text.x) * .5f, p.y + (size - text.y) * .5f), IM_COL32(242,246,255,255), label);
}
inline float keyboard_width(float size, int action) {
    if (action == Save) return key_width("Ctrl+S", size);
    if (action == Dpad) return size * 4.45f;
    if (action == Confirm) return key_width("Enter", size) + size * 1.5f;
    return key_width("Esc", size) + key_width("F1", size) + size * .5f;
}
inline void keyboard(ImDrawList *d, ImVec2 p, float size, int action) {
    const ImU32 ink = IM_COL32(242,246,255,255), edge = IM_COL32(160,184,211,255);
    if (action == Dpad) {
        for (int i = 0; i < 4; ++i) {
            ImVec2 q(p.x + i * size * 1.15f, p.y);
            float s = size;
            d->AddRect(q, ImVec2(q.x + s, q.y + s), edge, size * .1f);
            ImVec2 c(q.x + s * .5f, q.y + s * .5f);
            ImVec2 tip(c.x + (i == 2 ? -s * .25f : i == 3 ? s * .25f : 0),
                       c.y + (i == 0 ? -s * .25f : i == 1 ? s * .25f : 0));
            ImVec2 tail(c.x * 2 - tip.x, c.y * 2 - tip.y);
            d->AddLine(tail, tip, ink, 1.5f);
            ImVec2 a = i < 2 ? ImVec2(tip.x - s * .15f, c.y) : ImVec2(c.x, tip.y - s * .15f);
            ImVec2 b = i < 2 ? ImVec2(tip.x + s * .15f, c.y) : ImVec2(c.x, tip.y + s * .15f);
            d->AddLine(a, tip, ink, 1.5f);
            d->AddLine(b, tip, ink, 1.5f);
        }
    } else if (action == Save) {
        key(d, p, size, "Ctrl+S");
    } else if (action == Confirm) {
        key(d, p, size, "Enter");
        p.x += key_width("Enter", size) + size * .5f;
        d->AddRect(p, ImVec2(p.x + size * .7f, p.y + size), edge, size * .3f);
        d->AddRectFilled(ImVec2(p.x + size * .07f, p.y + size * .08f),
                         ImVec2(p.x + size * .33f, p.y + size * .43f), ink, size * .06f);
        d->AddLine(ImVec2(p.x + size * .35f, p.y), ImVec2(p.x + size * .35f, p.y + size * .5f), edge);
    } else {
        key(d, p, size, "Esc");
        p.x += key_width("Esc", size) + size * .5f;
        key(d, p, size, "F1");
    }
}
inline void draw(ImDrawList *d, ImVec2 p, float size, int layout, int action, int physical) {
    const ImU32 ink = IM_COL32(242,246,255,255), edge = IM_COL32(160,184,211,255);
    ImVec2 c(p.x + size*.5f, p.y + size*.5f);
    if (action == Dpad) {
        d->AddRectFilled(ImVec2(c.x-size*.38f,c.y-size*.12f),ImVec2(c.x+size*.38f,c.y+size*.12f),ink,1);
        d->AddRectFilled(ImVec2(c.x-size*.12f,c.y-size*.38f),ImVec2(c.x+size*.12f,c.y+size*.38f),ink,1);
        return;
    }
    if (action == Left || action == Right) {
        const char *name = layout == 0 ? (action == Left ? "LB" : "RB") :
                           layout == 1 ? (action == Left ? "L" : "R") : (action == Left ? "L1" : "R1");
        d->AddRect(p, ImVec2(p.x+size*1.5f,p.y+size), edge, size*.2f, 1.5f, 0);
        ImVec2 t=ImGui::CalcTextSize(name);
        d->AddText(ImVec2(p.x+(size*1.5f-t.x)*.5f,c.y-t.y*.5f), ink, name);
        return;
    }
    d->AddCircleFilled(c, size*.47f, IM_COL32(23,34,48,255));
    d->AddCircle(c, size*.47f, edge, 0, 1.5f);
    if (layout != 2) {
        const char *namesXbox[] = {"A","B","X","Y"}, *namesSwitch[] = {"B","A","Y","X"};
        const char *name = layout == 1 ? namesSwitch[physical] : namesXbox[physical];
        ImVec2 t=ImGui::CalcTextSize(name);
        d->AddText(ImVec2(c.x-t.x*.5f,c.y-t.y*.5f), ink, name);
    } else {
        float r=size*.23f;
        if (physical == 0) {
            d->AddLine(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),IM_COL32(125,198,255,255),2);
            d->AddLine(ImVec2(c.x-r,c.y+r),ImVec2(c.x+r,c.y-r),IM_COL32(125,198,255,255),2);
        } else if (physical == 1) d->AddCircle(c,r,IM_COL32(255,157,158,255),0,2);
        else if (physical == 2) d->AddRect(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),IM_COL32(244,166,230,255),0.0f,2.0f,0);
        else d->AddTriangle(ImVec2(c.x,c.y-r),ImVec2(c.x+r,c.y+r),ImVec2(c.x-r,c.y+r),IM_COL32(139,235,202,255),2);
    }
}
}
