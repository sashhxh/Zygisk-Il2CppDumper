#include "widgets.h"
#include "ImGui/imgui_internal.h"
#include <math.h>
#include <unordered_map>

namespace ui {

static float lerp(float a, float b, float t) { return a + (b - a) * t; }
static ImVec4 lerp4(ImVec4 a, ImVec4 b, float t) {
    return {lerp(a.x,b.x,t), lerp(a.y,b.y,t), lerp(a.z,b.z,t), lerp(a.w,b.w,t)};
}

static float& Anim(ImGuiID id, float init) {
    static std::unordered_map<ImGuiID, float> m;
    auto it = m.find(id);
    if (it == m.end()) { m[id] = init; return m[id]; }
    return it->second;
}

bool Toggle(const char* label, bool* v) {
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    if (win->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& s = g.Style;
    const ImGuiID id = win->GetID(label);

    float h = ImGui::GetFrameHeight();
    float trackW = h * 1.9f;
    ImVec2 p = win->DC.CursorPos;
    ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, true);

    ImRect total(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + h));
    ImRect track(ImVec2(total.Max.x - trackW, p.y + (h - h*0.7f)*0.5f),
                 ImVec2(total.Max.x,          p.y + (h - h*0.7f)*0.5f + h*0.7f));

    ImGui::ItemSize(total, s.FramePadding.y);
    if (!ImGui::ItemAdd(total, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total, id, &hovered, &held);
    if (pressed) *v = !*v;

    float& a = Anim(id, *v ? 1.f : 0.f);
    float target = *v ? 1.f : 0.f;
    a = lerp(a, target, ImClamp(g.IO.DeltaTime * 12.f, 0.f, 1.f));

    ImU32 off = ImGui::GetColorU32(ImVec4(0.22f,0.22f,0.26f,1.f));
    ImU32 on  = ImGui::GetColorU32(ImVec4(0.36f,0.78f,0.98f,1.f));
    ImU32 col = ImGui::GetColorU32(lerp4({0.22f,0.22f,0.26f,1.f},{0.36f,0.78f,0.98f,1.f}, a));
    (void)off; (void)on;

    float r = (track.Max.y - track.Min.y) * 0.5f;
    win->DrawList->AddRectFilled(track.Min, track.Max, col, r);

    float knobR = r - 3.f;
    float kx = lerp(track.Min.x + r, track.Max.x - r, a);
    win->DrawList->AddCircleFilled({kx, (track.Min.y+track.Max.y)*0.5f}, knobR,
                                   ImGui::GetColorU32(ImVec4(1,1,1,1)), 24);

    ImGui::RenderText(ImVec2(p.x, p.y + (h - labelSize.y)*0.5f), label);
    return pressed;
}

bool Slider(const char* label, float* v, float lo, float hi, const char* fmt) {
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    bool c = ImGui::SliderFloat(label, v, lo, hi, fmt);
    ImGui::PopItemWidth();
    return c;
}

bool SliderInt(const char* label, int* v, int lo, int hi) {
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    bool c = ImGui::SliderInt(label, v, lo, hi);
    ImGui::PopItemWidth();
    return c;
}

void Section(const char* text) {
    ImGui::Dummy({0, 4});
    ImVec4 col(0.55f, 0.58f, 0.65f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy({0, 2});
}

bool NotchPill(ImVec2 center, float width, float height, bool* open,
               ImVec2* dragStart, ImVec2* winStart, ImVec2* winPos, bool* dragging) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 a(center.x - width*0.5f, center.y - height*0.5f);
    ImVec2 b(center.x + width*0.5f, center.y + height*0.5f);
    float r = height * 0.5f;

    ImU32 bg = ImGui::GetColorU32(ImVec4(0.07f, 0.07f, 0.09f, 1.0f));
    dl->AddRectFilled(a, b, bg, r);

    float dotR = height * 0.18f;
    ImU32 dotCol = ImGui::GetColorU32(ImVec4(0.36f, 0.78f, 0.98f, 1.f));
    dl->AddCircleFilled({center.x - r*0.6f, center.y}, dotR, dotCol, 20);

    ImGuiIO& io = ImGui::GetIO();
    bool inside = io.MousePos.x >= a.x && io.MousePos.x <= b.x
               && io.MousePos.y >= a.y && io.MousePos.y <= b.y;
    static bool pressedInside = false;
    static ImVec2 downPos;

    if (!io.MouseDown[0]) {
        if (pressedInside && inside) {
            float dx = io.MousePos.x - downPos.x, dy = io.MousePos.y - downPos.y;
            if (dx*dx + dy*dy < 64.f) *open = !*open;
        }
        pressedInside = false;
        *dragging = false;
    } else if (io.MouseClicked[0] && inside) {
        pressedInside = true;
        downPos = io.MousePos;
        *dragStart = io.MousePos;
        *winStart = *winPos;
    } else if (pressedInside && io.MouseDown[0]) {
        float dx = io.MousePos.x - downPos.x, dy = io.MousePos.y - downPos.y;
        if (dx*dx + dy*dy > 64.f) *dragging = true;
        if (*dragging) {
            winPos->x = winStart->x + (io.MousePos.x - dragStart->x);
            winPos->y = winStart->y + (io.MousePos.y - dragStart->y);
        }
    }
    return pressedInside;
}

bool TabBarAnimated(const char* const* names, int count, int* current) {
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;
    float avail = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetFontSize() + 14.f;
    float tabW = avail / (float)count;
    ImVec2 origin = win->DC.CursorPos;
    bool changed = false;

    for (int i = 0; i < count; ++i) {
        ImGui::PushID(i);
        ImVec2 p(origin.x + i * tabW, origin.y);
        ImRect r(p, {p.x + tabW, p.y + h});
        ImGuiID id = win->GetID(names[i]);
        ImGui::ItemSize(ImVec2(tabW, h));
        if (i < count - 1) ImGui::SameLine(0, 0);
        ImGui::ItemAdd(r, id);
        bool hov, held;
        bool pr = ImGui::ButtonBehavior(r, id, &hov, &held);
        if (pr) { *current = i; changed = true; }

        float& fade = ui::Anim(id, i == *current ? 1.f : 0.f);
        fade = fade + ((i == *current ? 1.f : 0.f) - fade) * ImClamp(g.IO.DeltaTime * 10.f, 0.f, 1.f);

        ImVec4 c = { 0.60f + fade*0.4f, 0.62f + fade*0.38f, 0.68f + fade*0.32f, 1.f };
        ImVec2 ts = ImGui::CalcTextSize(names[i]);
        ImGui::PushStyleColor(ImGuiCol_Text, c);
        ImGui::RenderText({p.x + (tabW - ts.x)*0.5f, p.y + (h - ts.y)*0.5f}, names[i]);
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    static float indX = 0.f, indW = 0.f;
    float tX = origin.x + (*current) * tabW + tabW*0.2f;
    float tW = tabW * 0.6f;
    float k = ImClamp(g.IO.DeltaTime * 14.f, 0.f, 1.f);
    indX = indX + (tX - indX) * k;
    indW = indW + (tW - indW) * k;
    ImU32 col = ImGui::GetColorU32(ImVec4(0.36f, 0.78f, 0.98f, 1.f));
    win->DrawList->AddRectFilled({indX, origin.y + h - 3.f},
                                 {indX + indW, origin.y + h - 1.f}, col, 1.f);
    ImGui::Dummy({0, 6});
    return changed;
}

}
