#include "avatar_status.h"
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "imgui_internal.h"
#include "menu_font.h"

// GUI widgets, colors and animation curves ported from the supplied reference.
namespace mango_ui {
struct State {
    float scale=1;
    bool open=true;
    int active_tab=0,previous_tab=-1;
    float slide_offset=0,underline_width=0;
};
struct Result {
    bool changed=false,open=true,popup=false;
    ImVec2 min{},max{},bar_min{},bar_max{};
};
inline float FitScale(int w,int h){
    return std::max(.1f,std::min(w/2400.f,h/1080.f));
}
inline void LoadFont(ImGuiIO& io,float scale){
    static const ImWchar ranges[]={0x0020,0x00FF,0x0100,0x017F,0};
    ImFontConfig config;
    config.OversampleH=2;config.OversampleV=2;config.FontDataOwnedByAtlas=false;
    io.FontDefault=io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(menu_font_data),int(menu_font_size),22*scale,&config,ranges);
    if(!io.FontDefault){
        ImFontConfig fallback;fallback.SizePixels=22*scale;
        io.FontDefault=io.Fonts->AddFontDefault(&fallback);
    }
}
static bool ToggleSwitch(const char* label, bool* v, float scale)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext&     g     = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID     id    = window->GetID(label);
    const float trackH  = g.FontSize * 1.1f;
    const float trackW  = trackH * 2.1f;
    const float padX    =  3.0f * scale;
    const float padY    =  3.0f * scale;
    const float knobH   = trackH - padY * 2.0f;
    const float knobW   = knobH * 1.35f;
    const float knobRnd = knobH * 0.48f;
    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos,
              ImVec2(pos.x + trackW + style.ItemInnerSpacing.x
                             + ImGui::CalcTextSize(label, NULL, true).x,
                     pos.y + trackH));
    ImGui::ItemSize(bb, (trackH - g.FontSize) * 0.5f);
    if (!ImGui::ItemAdd(bb, id)) return false;
    bool hovered, held;
    bool clicked = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (clicked) { *v = !(*v); ImGui::MarkItemEdited(id); }
    float* pAnim = window->StateStorage.GetFloatRef(id, *v ? 1.0f : 0.0f);
    float  spd   = ImGui::GetIO().DeltaTime * 10.0f;
    *pAnim += (*v ? spd : -spd);
    *pAnim  = ImClamp(*pAnim, 0.0f, 1.0f);
    float t = *pAnim;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto lu = [](int a, int b, float tt){ return (int)(a + (b-a)*tt); };
    ImU32 colTrack = IM_COL32(lu(229,52,t), lu(229,199,t), lu(234,89,t), 255);
    ImVec2 tMin = pos;
    ImVec2 tMax = ImVec2(pos.x + trackW, pos.y + trackH);
    float  tRnd = trackH * 0.5f;
    dl->AddRectFilled(tMin, tMax, colTrack, tRnd);
    dl->AddRectFilled(
        ImVec2(tMin.x + 2.f * scale, tMin.y + 2.f * scale),
        ImVec2(tMax.x - 2.f * scale, tMax.y - 2.f * scale),
        IM_COL32(255,255,255,20), tRnd - 2.f * scale);
    float minKX = tMin.x + padX + knobW * 0.5f;
    float maxKX = tMax.x - padX - knobW * 0.5f;
    float kCX   = minKX + (maxKX - minKX) * t;
    float kCY   = tMin.y + trackH * 0.5f;
    ImVec2 kMin = ImVec2(kCX - knobW * 0.5f, kCY - knobH * 0.5f);
    ImVec2 kMax = ImVec2(kCX + knobW * 0.5f, kCY + knobH * 0.5f);
    dl->AddRectFilled(
        ImVec2(kMin.x,        kMin.y + 2.f * scale),
        ImVec2(kMax.x,        kMax.y + 2.f * scale),
        IM_COL32(0,0,0,35), knobRnd + 1.f * scale);
    dl->AddRectFilled(kMin, kMax, IM_COL32(255,255,255,255), knobRnd);
    if (t > 0.01f) {
        dl->AddRectFilled(kMin, kMax,
            IM_COL32(180,255,200,(int)(35*t)), knobRnd);
    }
    dl->AddRect(kMin, kMax, IM_COL32(0,0,0,18), knobRnd, 0, 1.2f * scale);
    ImVec2 labelPos = ImVec2(tMax.x + style.ItemInnerSpacing.x,
                             pos.y + (trackH - g.FontSize) * 0.5f);
    const char* labelEnd = label;
    while (*labelEnd) {
        if (labelEnd[0] == '#' && labelEnd[1] == '#') break;
        ++labelEnd;
    }
    dl->AddText(labelPos, ImGui::GetColorU32(ImGuiCol_Text), label, labelEnd);
    return clicked;
}
inline void ApplyStyle(float scale)
{
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle{};
    ImGui::StyleColorsDark();
    s.ScaleAllSizes(2.5f);
    s.WindowRounding    = 15.0f;
    s.ChildRounding     = 12.0f;
    s.FrameRounding     =  4.0f;
    s.GrabRounding      =  3.0f;
    s.PopupRounding     =  0.0f;
    s.ScrollbarRounding =  0.0f;
    s.WindowBorderSize  =  0.0f;
    s.ChildBorderSize   =  0.0f;
    s.FrameBorderSize   =  0.0f;
    s.PopupBorderSize   =  0.0f;
    s.WindowPadding     = ImVec2(0.0f, 0.0f);
    s.FramePadding      = ImVec2(6.0f, 4.0f);
    s.ItemSpacing       = ImVec2(8.0f, 4.0f);
    s.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    s.IndentSpacing     = 14.0f;
    s.ScrollbarSize     =  8.0f;
    s.GrabMinSize       = 10.0f;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.105f, 0.105f, 0.110f, 0.98f);
    c[ImGuiCol_Header]               = ImVec4(0.17f, 0.17f, 0.18f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.29f, 0.29f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.26f, 0.26f, 0.28f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.32f, 0.32f, 0.34f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.36f, 0.36f, 0.38f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.80f, 0.80f, 0.82f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.78f, 0.78f, 0.82f, 1.00f);
    c[ImGuiCol_Separator]            = ImVec4(0.48f, 0.48f, 0.50f, 0.75f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.105f, 0.105f, 0.11f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.28f, 0.29f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.36f, 0.37f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.43f, 0.43f, 0.44f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.13f, 0.13f, 0.14f, 0.98f);
    c[ImGuiCol_Button]               = ImVec4(0.16f, 0.16f, 0.17f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.22f, 0.23f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.26f, 0.26f, 0.27f, 1.00f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f,  0.0f,  0.0f,  0.20f);
    c[ImGuiCol_Border]               = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    s.ScaleAllSizes(scale);
}

inline void Section(const char* title,float pad,float scale,bool settings=false){
    ImGui::SetCursorPos({pad,5*scale});
    ImGui::TextColored(settings?ImVec4(.88f,.88f,.90f,1):ImVec4(1,1,1,1),"%s",title);
    ImGui::SetCursorPos({pad,32*scale});ImGui::Separator();
    ImGui::SetCursorPos({pad,42*scale});
}
inline void Label(const char* label,float pad){
    ImGui::SetCursorPosX(pad);ImGui::TextDisabled("%s",label);ImGui::SetCursorPosX(pad);
}
inline ImRect PlayersBar(State& state,int players){
    const float s=state.scale;char text[48];
    if(players<0)std::snprintf(text,sizeof(text),"Players: --");
    else std::snprintf(text,sizeof(text),"Players: %d",players);
    const ImVec2 size=ImGui::CalcTextSize(text);
    const float width=size.x+32*s,height=size.y+14*s;
    const ImVec2 min{(ImGui::GetIO().DisplaySize.x-width)*.5f,12*s};
    const ImRect rect(min,{min.x+width,min.y+height});
    ImDrawList* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled(rect.Min,rect.Max,IM_COL32(12,12,12,200),8*s);
    draw->AddText({min.x+16*s,min.y+7*s},IM_COL32(240,240,240,255),text);
    if(ImGui::GetIO().MouseClicked[0]&&rect.Contains(ImGui::GetIO().MousePos)){
        state.open=!state.open;ImGui::ClearActiveID();
        if(!state.open&&GImGui->OpenPopupStack.Size)ImGui::ClosePopupToLevel(0,true);
    }
    return rect;
}
template<class Settings> Result Draw(Settings& cfg,State& state,int w,int h,int players=-1,int chams_state=0,unsigned chams_meshes=0,int camera_state=0,unsigned body_meshes=0,int shader_stage=0,float chams_cpu=0,float camera_cpu=0,int model_state=0){
    Result result;const ImRect bar=PlayersBar(state,players);
    result.bar_min=bar.Min;result.bar_max=bar.Max;result.open=state.open;
    if(!state.open)return result;
    const float s=state.scale;
    const float menuW=748*s,menuH=506*s;
    const float innerPad=14*s,tabSpacing=24*s,radius=8*s,tlGap=22*s;
    ImGui::SetNextWindowPos({std::min(w*.574f,w-menuW-12*s),std::min(h*.224f,h-menuH-12*s)},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({menuW,menuH});
    const auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    const bool visible=ImGui::Begin("##MangoMenu",nullptr,flags);
    ImGui::PopStyleVar();
    if(visible){
        const char* labels[]={"visual","aim","player","misc"};
        float totalW=tabSpacing*3;
        for(const char* label:labels)totalW+=ImGui::CalcTextSize(label).x;
        ImGui::PushStyleColor(ImGuiCol_Button,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_Text,{1,1,1,1});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{tabSpacing,0});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,6*s});
        ImGui::SetCursorPos({menuW-totalW-innerPad-8*s,6*s});
        ImVec2 tabMin[4],tabMax[4];
        for(int i=0;i<4;++i){
            if(i)ImGui::SameLine();
            if(ImGui::Button(labels[i]))state.active_tab=i;
            tabMin[i]=ImGui::GetItemRectMin();tabMax[i]=ImGui::GetItemRectMax();
        }
        ImGui::PopStyleVar(2);ImGui::PopStyleColor(4);
        const float contentY=ImGui::GetCursorPosY()+8*s;
        ImDrawList* draw=ImGui::GetWindowDrawList();
        const ImVec2 pos=ImGui::GetWindowPos();
        const float centerY=pos.y+12*s+ImGui::GetTextLineHeight()*.5f;
        const float startX=pos.x+innerPad+10*s;
        const ImU32 colors[]={IM_COL32(255,96,89,255),IM_COL32(255,189,46,255),IM_COL32(39,200,63,255)};
        for(int i=0;i<3;++i){
            const ImVec2 center{startX+tlGap*i,centerY};
            draw->AddCircleFilled(center,radius,colors[i]);
            draw->AddCircle(center,radius,IM_COL32(0,0,0,30),24,1*s);
        }
        const float targetX=tabMin[state.active_tab].x;
        const float targetW=tabMax[state.active_tab].x-targetX;
        if(state.previous_tab<0){state.underline_width=targetW;state.previous_tab=state.active_tab;}
        else if(state.previous_tab!=state.active_tab){
            state.slide_offset=tabMin[state.previous_tab].x-targetX;state.previous_tab=state.active_tab;
        }
        const float anim=1.f-std::exp(-ImGui::GetIO().DeltaTime*16.f);
        state.slide_offset+=(0-state.slide_offset)*anim;
        state.underline_width+=(targetW-state.underline_width)*anim;
        const float lineY=tabMax[state.active_tab].y+1*s;
        draw->AddLine({targetX+state.slide_offset,lineY},
            {targetX+state.slide_offset+state.underline_width,lineY},IM_COL32(170,95,255,255),2.5f*s);
        {
            const float gapX=12*s,colW=(menuW-innerPad*2-gapX)*.5f;
            const float contentH=menuH-contentY-12*s,rPad=12*s,itemW=colW-rPad*2;
            ImGui::SetCursorPos({innerPad,contentY});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8*s,6*s});
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,9*s);
            ImGui::PushStyleColor(ImGuiCol_ChildBg,{.145f,.145f,.15f,1});
            if(ImGui::BeginChild("##LeftColumn",{colW,contentH},ImGuiChildFlags_Borders)){
                auto toggle=[&](const char* label,bool& value){
                    ImGui::SetCursorPosX(innerPad);result.changed|=ToggleSwitch(label,&value,s);
                };
                if(state.active_tab==0){
                    Section("Visual",innerPad,s);
                    toggle("ESP Enable",cfg.enabled);toggle("Line",cfg.lines);toggle("Box",cfg.box);
                    toggle("Corner Box",cfg.corners);toggle("Distance",cfg.distances);
                    toggle("Fill",cfg.fill);toggle("Skeleton",cfg.skeleton);
                    toggle("Visibility Check",cfg.visibility_check);toggle("Visible Only",cfg.visible_only);
                    toggle("Chams",cfg.chams);
                }else if(state.active_tab==1){
                    Section("Magic Bullet",innerPad,s);
                    toggle("Enable",cfg.magic_bullet);toggle("ADS Only",cfg.magic_ads_only);
                    toggle("Team Filter",cfg.magic_team_filter);
                }else if(state.active_tab==2){
                    Section("Player",innerPad,s);
                    toggle("No Recoil",cfg.no_recoil);toggle("No Spread",cfg.no_spread);
                    toggle("No Weapon Motion",cfg.no_weapon_motion);
                    toggle("Third Person",cfg.third_person);
                    const bool previous_model=cfg.custom_model;
                    toggle("Custom Model",cfg.custom_model);
                    if(cfg.custom_model&&!previous_model)cfg.third_person=true;
                }else{
                    Section("Misc",innerPad,s);
                    ImGui::PushItemWidth(colW-innerPad*2);
                    const char* channels[]={"Red","Green","Blue"};
                    for(int i=0;i<3;++i){
                        Label(channels[i],innerPad);ImGui::PushID(i);
                        result.changed|=ImGui::SliderFloat("##channel",&cfg.color[i],0,1,"%.2f");
                        ImGui::PopID();
                    }
                    ImGui::PopItemWidth();ImGui::Spacing();ImGui::SetCursorPosX(innerPad);
                    ImGui::PushStyleColor(ImGuiCol_Button,{.80f,.10f,.10f,1});
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.95f,.15f,.15f,1});
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,{.60f,.05f,.05f,1});
                    if(ImGui::Button("Reset Defaults")){cfg=Settings{};result.changed=true;}
                    ImGui::PopStyleColor(3);
                }
            }
            ImGui::EndChild();ImGui::SameLine(0,gapX);
            if(ImGui::BeginChild("##RightColumn",{colW,contentH},ImGuiChildFlags_Borders)){
                Section("Settings",rPad,s,true);ImGui::PushItemWidth(itemW);
                if(state.active_tab==0){
                    Label("Range",rPad);result.changed|=ImGui::SliderFloat("##esp_range",&cfg.range,25,1000,"%.0f m");
                    ImGui::Spacing();ImGui::SetCursorPosX(rPad);ImGui::TextDisabled("Occluded: red");
                    ImGui::SetCursorPosX(rPad);ImGui::TextDisabled("Unknown: gray");
                    Label("Chams Opacity",rPad);result.changed|=ImGui::SliderFloat("##chams_alpha",&cfg.chams_alpha,.05f,.65f,"%.2f");
                    Label("Chams Range",rPad);result.changed|=ImGui::SliderFloat("##chams_range",&cfg.chams_range,15,150,"%.0f m");
                    Label("Chams Models",rPad);result.changed|=ImGui::SliderInt("##chams_limit",&cfg.chams_limit,4,32);
                    if(cfg.chams){
                        ImGui::SetCursorPosX(rPad);
                        if(chams_state==-1)ImGui::TextDisabled("Chams: binding unavailable");
                        else if(chams_state==-2)ImGui::TextDisabled("Shader failed: %d",shader_stage);
                        else if(chams_state==2)ImGui::TextDisabled("Shader loading: %d",shader_stage);
                        else ImGui::TextDisabled("Chams meshes: %u",chams_meshes);
                        ImGui::SetCursorPosX(rPad);ImGui::TextDisabled("Through walls / armor");
                        ImGui::SetCursorPosX(rPad);ImGui::TextDisabled("Chams CPU: %.2f ms",chams_cpu);
                    }
                }else if(state.active_tab==1){
                    Label("Target Point",rPad);
                    result.changed|=ImGui::Combo("##magic_point",&cfg.magic_point,"Head\0Body\0");
                    Label("Distance",rPad);result.changed|=ImGui::SliderFloat("##magic_range",&cfg.range,25,1000,"%.0f m");
                    Label("FOV",rPad);result.changed|=ImGui::SliderFloat("##magic_fov",&cfg.magic_fov,25,800,"%.0f px");
                }else if(state.active_tab==2){
                    Label("Camera Distance",rPad);result.changed|=ImGui::SliderFloat("##camera_distance",&cfg.camera_distance,.8f,5.f,"%.1f m");
                    if(cfg.third_person){
                        ImGui::SetCursorPosX(rPad);ImGui::TextDisabled("Camera: %s",camera_state==1?"active":camera_state==-3?"full body missing":camera_state<0?"unavailable":"waiting");
                        ImGui::SetCursorPosX(rPad);ImGui::TextDisabled("Body meshes: %u",body_meshes);
                        ImGui::SetCursorPosX(rPad);ImGui::TextDisabled("Camera CPU: %.2f ms",camera_cpu);
                        if(cfg.custom_model){ImGui::SetCursorPosX(rPad);ImGui::TextWrapped("Model: %s",mango_avatar::status_text(model_state));if(model_state<0){ImGui::SetCursorPosX(rPad);ImGui::TextDisabled("Code: %d",model_state);}}
                    }
                }else if(state.active_tab==3){
                    Label("Color",rPad);result.changed|=ImGui::ColorEdit3("##esp_color",cfg.color,ImGuiColorEditFlags_NoInputs);
                }
                ImGui::PopItemWidth();
            }
            ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar(2);
        }
    }
    result.min=ImGui::GetWindowPos();const ImVec2 size=ImGui::GetWindowSize();
    result.max={result.min.x+size.x,result.min.y+size.y};
    result.open=state.open;
    ImGui::End();
    result.popup=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup);
    return result;
}
} // namespace mango_ui
