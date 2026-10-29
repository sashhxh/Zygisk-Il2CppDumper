#include "main.h"
#include "widgets.h"
#include "ImGui/Font/Font.h"
#include <math.h>
#include <string>

static ImFont* g_fontMain = nullptr;
static ImFont* g_fontBig  = nullptr;

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg; cfg.PixelSnapH = true;
    g_fontMain = io.Fonts->AddFontFromMemoryCompressedTTF(RobotoFont_compressed_data,
                                                         RobotoFont_compressed_size, 18.f, &cfg);
    g_fontBig  = io.Fonts->AddFontFromMemoryCompressedTTF(RobotoFont_compressed_data,
                                                         RobotoFont_compressed_size, 24.f, &cfg);
    io.FontDefault = g_fontMain;
}

void SetupStyle() {
    auto& s = ImGui::GetStyle();
    s.WindowRounding = 24; s.FrameRounding = 14;
    s.GrabRounding   = 14; s.PopupRounding = 14;
    s.ChildRounding  = 18;
    s.WindowPadding  = {18, 56};
    s.FramePadding   = {14, 10};
    s.ItemSpacing    = {10, 10};
    s.ScrollbarSize  = 8;
    s.WindowBorderSize = 1; s.FrameBorderSize = 0;

    auto& c = s.Colors;
    c[ImGuiCol_WindowBg]        = {0.09f, 0.09f, 0.11f, 0.98f};
    c[ImGuiCol_ChildBg]         = {0.12f, 0.12f, 0.14f, 1.00f};
    c[ImGuiCol_Border]          = {0.18f, 0.18f, 0.22f, 1.00f};
    c[ImGuiCol_FrameBg]         = {0.14f, 0.14f, 0.17f, 1.00f};
    c[ImGuiCol_FrameBgHovered]  = {0.18f, 0.18f, 0.22f, 1.00f};
    c[ImGuiCol_FrameBgActive]   = {0.20f, 0.20f, 0.24f, 1.00f};
    c[ImGuiCol_Button]          = {0.18f, 0.18f, 0.22f, 1.00f};
    c[ImGuiCol_ButtonHovered]   = {0.24f, 0.24f, 0.30f, 1.00f};
    c[ImGuiCol_ButtonActive]    = {0.14f, 0.14f, 0.17f, 1.00f};
    c[ImGuiCol_Separator]       = {0.20f, 0.20f, 0.24f, 1.00f};
    c[ImGuiCol_SliderGrab]      = {0.36f, 0.78f, 0.98f, 1.00f};
    c[ImGuiCol_SliderGrabActive]= {0.46f, 0.84f, 1.00f, 1.00f};
    c[ImGuiCol_CheckMark]       = {0.36f, 0.78f, 0.98f, 1.00f};
    c[ImGuiCol_Text]            = {0.94f, 0.95f, 0.98f, 1.00f};
    c[ImGuiCol_TextDisabled]    = {0.55f, 0.58f, 0.65f, 1.00f};
}

struct Config {
    bool  esp_box = true;
    bool  esp_name = false;
    bool  esp_dist = true;
    bool  esp_hp = true;
    bool  esp_skel = false;
    float esp_range = 300.f;
    float esp_thick = 1.4f;

    bool  aim_on = false;
    bool  aim_vis = true;
    int   aim_bone = 1;
    float aim_fov = 90.f;
    float aim_smooth = 2.5f;

    bool  misc_speed = false;
    float misc_speed_mult = 1.5f;
    bool  misc_fly = false;
    bool  misc_norecoil = false;
    int   misc_fps_cap = 60;
};
static Config g_cfg;

static ImVec2 g_winPos  = {-1, -1};
static ImVec2 g_winSize = {380, 460};
static ImVec2 g_winSizeOpen = {380, 460};
static ImVec2 g_dragStart{}, g_winStart{};
static bool   g_drag = false;
static int    g_tab = 0;
static float  g_openAnim = 1.f;
static uint32_t g_ori = 0xFFFFFFFF;

static float smoothstep(float t) { return t*t*(3.f - 2.f*t); }

static void TabVisual() {
    ui::Section("ESP");
    ui::Toggle("Box",       &g_cfg.esp_box);
    ui::Toggle("Name",      &g_cfg.esp_name);
    ui::Toggle("Distance",  &g_cfg.esp_dist);
    ui::Toggle("Health bar",&g_cfg.esp_hp);
    ui::Toggle("Skeleton",  &g_cfg.esp_skel);
    ui::Section("Render");
    ui::Slider("Range",      &g_cfg.esp_range, 20.f, 1200.f, "%.0f m");
    ui::Slider("Line width", &g_cfg.esp_thick, 0.5f, 4.f);
}

static void TabCombat() {
    ui::Section("Aim");
    ui::Toggle("Enable",       &g_cfg.aim_on);
    ui::Toggle("Visible only", &g_cfg.aim_vis);
    const char* bones[] = {"Pelvis","Head","Neck","Chest"};
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::Combo("Bone", &g_cfg.aim_bone, bones, IM_ARRAYSIZE(bones));
    ImGui::PopItemWidth();
    ui::Slider("FOV",    &g_cfg.aim_fov,    5.f, 180.f, "%.0f");
    ui::Slider("Smooth", &g_cfg.aim_smooth, 0.5f, 10.f);
}

static void TabMisc() {
    ui::Section("Movement");
    ui::Toggle("Speed",  &g_cfg.misc_speed);
    ui::Slider("Multiplier", &g_cfg.misc_speed_mult, 1.f, 5.f);
    ui::Toggle("Fly",    &g_cfg.misc_fly);
    ui::Section("Weapon");
    ui::Toggle("No recoil", &g_cfg.misc_norecoil);
    ui::Section("Perf");
    ui::SliderInt("FPS cap", &g_cfg.misc_fps_cap, 30, 240);
}

static void TabSettings() {
    ui::Section("Menu");
    static float sz = 1.f;
    if (ui::Slider("Scale", &sz, 0.75f, 1.5f)) {
        ImGui::GetIO().FontGlobalScale = sz;
    }
    ImGui::Dummy({0, 6});
    if (ImGui::Button("Exit", {ImGui::GetContentRegionAvail().x, 40.f}))
        main_thread_flag.store(false);
}

void RenderMenu() {
    float sw = (float)displayInfo.width;
    float sh = (float)displayInfo.height;

    if (g_winPos.x < 0 || displayInfo.orientation != g_ori) {
        g_ori = displayInfo.orientation;
        g_winPos = {sw * 0.5f - g_winSize.x * 0.5f, sh * 0.08f};
    }

    float pillW = 180.f, pillH = 34.f;
    ImVec2 pillCenter{g_winPos.x + g_winSize.x * 0.5f, g_winPos.y + pillH * 0.5f};
    ui::NotchPill(pillCenter, pillW, pillH, &menu_open, &g_dragStart, &g_winStart, &g_winPos, &g_drag);

    float target = menu_open ? 1.f : 0.f;
    float k = ImClamp(ImGui::GetIO().DeltaTime * 10.f, 0.f, 1.f);
    g_openAnim = g_openAnim + (target - g_openAnim) * k;
    float eased = smoothstep(ImClamp(g_openAnim, 0.f, 1.f));
    if (eased < 0.01f) return;

    g_winSize.y = g_winSizeOpen.y * eased;
    g_winSize.x = g_winSizeOpen.x;

    ImGui::SetNextWindowPos({g_winPos.x, g_winPos.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(g_winSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, eased);
    ImGui::Begin("##menu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav);

    ImGui::Dummy({0, 2});
    if (g_fontBig) ImGui::PushFont(g_fontBig);
    ImVec4 accent(0.36f, 0.78f, 0.98f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::TextUnformatted("Crez");
    ImGui::PopStyleColor();
    if (g_fontBig) ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("  //  menu");

    ImGui::Dummy({0, 2});
    static const char* tabs[] = {"Visual", "Combat", "Misc", "Settings"};
    ui::TabBarAnimated(tabs, IM_ARRAYSIZE(tabs), &g_tab);

    ImGui::BeginChild("##body", {0, 0}, false,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysUseWindowPadding);
    switch (g_tab) {
        case 0: TabVisual();   break;
        case 1: TabCombat();   break;
        case 2: TabMisc();     break;
        case 3: TabSettings(); break;
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}
