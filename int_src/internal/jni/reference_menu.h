#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "reference_atlas.h"
#include "reference_options.h"
#include "xvcen/color_sheet.h"
#include "xvcen/drag_motion.h"
#include "settings.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mango_ui {
inline bool russian_labels=false;
struct State {
    float scale=1;
    ImVec2 previous_display{};
    bool open=true,chams_settings=false,weapon_settings=false;
    int active_tab=0,previous_tab=-1;
    float slide_offset=0,underline_width=0;
    float selection_y=226.4f,selection_velocity=0;
    double animation_time=0;
    bool dragging_content=false;
    xvcen_assets::Island island;
    xvcen_assets::Notification notification;
    xvcen_assets::ColorSheet colors;
    bool colors_are_chams=false;
    xvcen_assets::MenuMotion menu_motion;
    xvcen_assets::JellyMotion jelly;
    bool tracking_window=false,dragging_window=false;
    ImVec2 drag_pointer{},drag_origin{};
    xvcen_assets::Spring content_slide;
    float content_alpha=1;
};
struct Result {
    bool changed=false,open=true,popup=false;
    ImVec2 min{},max{},bar_min{},bar_max{};
};
inline float FitScale(int width,int height) {
    return std::max(.1f,std::min({1.f,(width-16.f)/840.f,(height-16.f)/720.f}));
}
inline void LoadFont(ImGuiIO& io,float scale) {
    xvcen_assets::atlas.load(io,scale);
    mango_reference::atlas.text=xvcen_assets::atlas.text;
    mango_reference::atlas.pixel=xvcen_assets::atlas.text;
}
inline void BakeAssets(ImGuiIO& io) {xvcen_assets::atlas.bake(io);}
inline void ApplyStyle(float scale) {
    auto& style=ImGui::GetStyle();style=ImGuiStyle{};
    style.FontSizeBase=28;style.FontScaleMain=scale;
    style.WindowPadding={0,0};style.FramePadding={0,0};style.ItemSpacing={0,0};
    style.WindowBorderSize=style.ChildBorderSize=style.PopupBorderSize=0;
    style.WindowRounding=24*scale;style.ChildRounding=24*scale;
    style.ScrollbarSize=8*scale;style.ScrollbarRounding=4*scale;
    style.Colors[ImGuiCol_WindowBg]={0,0,0,0};
    style.Colors[ImGuiCol_ChildBg]={0,0,0,0};
    style.Colors[ImGuiCol_Border]={0,0,0,0};
    style.Colors[ImGuiCol_ScrollbarBg]={0,0,0,0};
}
inline ImU32 Color(float r,float g,float b,float a=1) {
    return ImGui::ColorConvertFloat4ToU32({r,g,b,a});
}
inline float Smooth(float value){return value*value*(3-2*value);}
inline float Animate(float value,float target,float speed=12.f) {
    const float dt=std::clamp(ImGui::GetIO().DeltaTime,.0005f,.05f);
    const float factor=std::min(1-std::exp(-speed*dt),std::max(.085f,dt*6));
    float out=value+(target-value)*factor;
    return std::fabs(out-target)<.002f?target:out;
}
inline void Text(ImDrawList* draw,ImVec2 at,const char* label,float size,ImU32 color) {
    draw->AddText(xvcen_assets::atlas.text,size,at,color,xvcen_assets::translate(label,russian_labels));
}
inline ImVec2 TextSize(const char* label,float size) {
    return xvcen_assets::atlas.text->CalcTextSizeA(size,FLT_MAX,0,xvcen_assets::translate(label,russian_labels));
}
inline bool Tapped() {
    const auto delta=ImGui::GetMouseDragDelta(0);
    return delta.x*delta.x+delta.y*delta.y<100.f;
}
inline void Notify(State& state,const char* label,bool enabled) {
    state.notification.push(xvcen_assets::translate(label,russian_labels),enabled);
}
struct Rows {
    State& state;Result& result;mango_reference::Options& options;
    float dark,s;
    bool first_card=true;
    ImU32 ink()const{return Color(dark,dark,dark);}
    ImU32 muted()const{return Color(.557f,.557f,.576f);}
    ImU32 accent()const{return Color(.039f*dark,.478f+.04000002f*dark,1);}
    float width()const{return ImGui::GetContentRegionAvail().x;}
    void card(const char* title,int rows,float row_height=78) {
        if(title&&*title){
            ImGui::Dummy({1,18*s});const auto at=ImGui::GetCursorScreenPos();
            const float font=ImGui::GetFontSize()*1.15f;
            Text(ImGui::GetWindowDrawList(),{at.x+32*s,at.y},title,font,muted());
            ImGui::Dummy({1,TextSize(title,font).y});ImGui::Dummy({1,8*s});
        }else if(!first_card)ImGui::Dummy({1,8*s});
        first_card=false;if(rows<=0)return;
        const auto p=ImGui::GetCursorScreenPos();
        xvcen_reference::Widgets<ImDrawList>{*ImGui::GetWindowDrawList(),xvcen_assets::atlas.text,ImGui::GetFontSize(),dark,s}.card(p,width(),rows*row_height,options.separators);
    }
    bool toggle(const char* label,bool& value,bool last=false,float* color=nullptr,int components=4) {
        auto* window=ImGui::GetCurrentWindow();const auto p=ImGui::GetCursorScreenPos();const float w=width();
        const ImGuiID id=window->GetID(label);
        bool clicked=ImGui::InvisibleButton(label,{w,78*s})&&Tapped()&&!state.dragging_content;
        const float font=ImGui::GetFontSize()*1.15f;
        const ImVec2 dot{p.x+63*s+TextSize(label,font).x,p.y+39*s};
        const auto mouse=ImGui::GetIO().MousePos;
        const bool color_clicked=color&&ImGui::IsItemHovered()&&ImGui::IsMouseReleased(0)&&Tapped()&&
            std::fabs(mouse.x-dot.x)<20*s&&std::fabs(mouse.y-dot.y)<20*s;
        if(color_clicked){clicked=false;state.colors_are_chams=false;state.colors.request(label,color,components);}
        if(clicked){value=!value;result.changed=true;Notify(state,label,value);}
        float* animation=window->StateStorage.GetFloatRef(id^0x735421u,value?1.f:0.f);
        *animation=Animate(*animation,value?1.f:0.f);
        auto* draw=ImGui::GetWindowDrawList();
        xvcen_reference::Widgets<ImDrawList>{*draw,xvcen_assets::atlas.text,ImGui::GetFontSize(),dark,s}.toggle(p,w,xvcen_assets::translate(label,russian_labels),*animation,options.separators,last);
        if(color) {
            draw->AddCircleFilled(dot,13*s,Color(color[0],color[1],color[2],components==4?color[3]:1),32);
            draw->AddCircle(dot,15*s,IM_COL32(0,0,0,80),32,2*s);
            draw->AddCircle(dot,14*s,IM_COL32(255,255,255,180),32,1.2f*s);
        }
        return clicked;
    }
    bool action(const char* label,const char* value=nullptr,float height=78,float font_override=0,ImU32 value_color=0,ImU32 label_color=0) {
        const auto p=ImGui::GetCursorScreenPos();const float w=width();
        const bool clicked=ImGui::InvisibleButton(label,{w,height*s})&&Tapped()&&!state.dragging_content;
        auto* draw=ImGui::GetWindowDrawList();const float font=font_override>0?font_override:ImGui::GetFontSize()*1.15f;
        Text(draw,{p.x+36*s,p.y+height*.5f*s-font*.5f},label,font,label_color?label_color:ink());
        if(value){const auto size=TextSize(value,font);Text(draw,{p.x+w-36*s-size.x,p.y+height*.5f*s-font*.5f},value,font,value_color?value_color:accent());}
        return clicked;
    }
    bool edit_color(const char* label,float* value) {
        ImGui::PushID("color-action");ImGui::PushID(label);
        if(action(label)){state.colors_are_chams=true;state.colors.request(label,value,3);}
        ImGui::PopID();ImGui::PopID();return false;
    }
    void slider(const char* label,float& value,float lo,float hi,const char* format="%.0f") {
        const auto p=ImGui::GetCursorScreenPos();const float w=width();
        ImGui::PushID("slider");
        const bool tapped=ImGui::InvisibleButton(label,{w,108*s})&&Tapped();
        const auto drag=ImGui::GetMouseDragDelta(0);
        const bool horizontal=ImGui::IsItemActive()&&ImGui::IsMouseDragging(0)&&std::fabs(drag.x)>std::fabs(drag.y);
        if((tapped||horizontal)&&!state.dragging_content){
            float next=lo+(hi-lo)*std::clamp((ImGui::GetIO().MousePos.x-p.x-58*s)/(w-116*s),0.f,1.f);
            if(next!=value){value=next;result.changed=true;}
        }
        auto* draw=ImGui::GetWindowDrawList();
        char text[48];std::snprintf(text,sizeof(text),format,double(value));
        const auto ts=TextSize(text,ImGui::GetFontSize());
        const float t=std::clamp((value-lo)/(hi-lo),0.f,1.f);
        auto* store=ImGui::GetStateStorage();const auto id=ImGui::GetID(label);
        float* position=store->GetFloatRef(id^0x4411u,-1);
        if(*position<0)*position=t;
        store->GetFloatRef(id^0x4412u,0);
        position=store->GetFloatRef(id^0x4411u,t);
        float* velocity=store->GetFloatRef(id^0x4412u,0);
        const float dt=ImGui::GetIO().DeltaTime;
        *velocity+=dt*((t-*position)*(horizontal?900.f:280.f)-*velocity*(horizontal?60.f:28.f));
        *position=std::clamp(*position+dt*(*velocity),0.f,1.f);
        if(std::fabs(t-*position)<.0002f&&std::fabs(*velocity)<.001f){*position=t;*velocity=0;}
        xvcen_reference::Widgets<ImDrawList>{*draw,xvcen_assets::atlas.text,ImGui::GetFontSize(),dark,s}.slider(p,w,xvcen_assets::translate(label,russian_labels),text,ts.x,*position,horizontal,options.separators,true);
        ImGui::PopID();
    }
};
inline void TransformMenu(ImGuiWindow* menu,ImVec2 target_min,ImVec2 target_max,float amount,const xvcen_assets::JellyMotion* jelly=nullptr) {
    const ImVec2 origin=menu->Pos,size=menu->Size;
    auto point=[&](ImVec2 p){return jelly?jelly->point((p.x-origin.x)/size.x,(p.y-origin.y)/size.y):xvcen_assets::menu_warp(p,origin,size,target_min,target_max,amount);};
    auto blend=[](const ImDrawVert& a,const ImDrawVert& b,float t){
        ImDrawVert v;v.pos={a.pos.x+(b.pos.x-a.pos.x)*t,a.pos.y+(b.pos.y-a.pos.y)*t};
        v.uv={a.uv.x+(b.uv.x-a.uv.x)*t,a.uv.y+(b.uv.y-a.uv.y)*t};v.col=0;
        for(int shift:{0,8,16,24})v.col|=ImU32(float((a.col>>shift)&255)+(float((b.col>>shift)&255)-float((a.col>>shift)&255))*t+.5f)<<shift;
        return v;
    };
    auto clip=[&](std::vector<ImDrawVert> polygon,int axis,float edge,bool lower){
        std::vector<ImDrawVert> out;if(polygon.empty())return out;
        auto coordinate=[&](const ImDrawVert& v){return axis?v.pos.y:v.pos.x;};
        auto previous=polygon.back();bool was=lower?coordinate(previous)>=edge:coordinate(previous)<=edge;
        for(const auto& current:polygon){
            const bool inside=lower?coordinate(current)>=edge:coordinate(current)<=edge;
            if(inside!=was)out.push_back(blend(previous,current,(edge-coordinate(previous))/(coordinate(current)-coordinate(previous))));
            if(inside)out.push_back(current);previous=current;was=inside;
        }
        return out;
    };
    for(auto* window:GImGui->Windows){
        if(!window->Active||(window->RootWindow!=menu&&window->RootWindowPopupTree!=menu))continue;
        auto* list=window->DrawList;
        ImVector<ImDrawVert> vertices;ImVector<ImDrawIdx> indices;ImVector<ImDrawCmd> commands;
        unsigned vertex_base=0;
        for(auto command:list->CmdBuffer){
            if(command.UserCallback){commands.push_back(command);continue;}
            const auto original=command;
            command.IdxOffset=indices.Size;command.VtxOffset=vertex_base;command.ElemCount=0;
            command.ClipRect={0,0,ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y};
            for(unsigned i=0;i<original.ElemCount;i+=3){
                std::vector<ImDrawVert> polygon;
                for(unsigned j=0;j<3;++j)polygon.push_back(list->VtxBuffer[original.VtxOffset+list->IdxBuffer[original.IdxOffset+i+j]]);
                polygon=clip(polygon,0,std::max(origin.x,original.ClipRect.x),true);
                polygon=clip(polygon,0,std::min(origin.x+size.x,original.ClipRect.z),false);
                polygon=clip(polygon,1,std::max(origin.y,original.ClipRect.y),true);
                polygon=clip(polygon,1,std::min(origin.y+size.y,original.ClipRect.w),false);
                if(polygon.size()<3)continue;
                float lo=polygon[0].pos.y,hi=lo;
                for(const auto& v:polygon){lo=std::min(lo,v.pos.y);hi=std::max(hi,v.pos.y);}
                const int slices=std::max(1,int(std::ceil((hi-lo)/std::max(4.f,size.y/48))));
                for(int band=0;band<slices;++band){
                    auto slice=clip(polygon,1,lo+(hi-lo)*band/slices,true);
                    slice=clip(slice,1,lo+(hi-lo)*(band+1)/slices,false);
                    for(size_t triangle=1;triangle+1<slice.size();++triangle){
                        if(vertices.Size-command.VtxOffset>65532){commands.push_back(command);
                            vertex_base=vertices.Size;command.IdxOffset=indices.Size;command.VtxOffset=vertex_base;command.ElemCount=0;}
                        for(size_t index:{size_t(0),triangle,triangle+1}){
                            auto v=slice[index];v.pos=point(v.pos);
                            indices.push_back(ImDrawIdx(vertices.Size-command.VtxOffset));vertices.push_back(v);
                        }
                        command.ElemCount+=3;
                    }
                }
            }
            commands.push_back(command);
        }
        list->VtxBuffer.swap(vertices);list->IdxBuffer.swap(indices);list->CmdBuffer.swap(commands);
        list->_CmdHeader.VtxOffset=list->CmdBuffer.Size?list->CmdBuffer.back().VtxOffset:0;
        list->_VtxCurrentIdx=list->VtxBuffer.Size-list->_CmdHeader.VtxOffset;
        list->_VtxWritePtr=list->VtxBuffer.Data+list->VtxBuffer.Size;
        list->_IdxWritePtr=list->IdxBuffer.Data+list->IdxBuffer.Size;
    }
}
template<class Settings> Result Draw(Settings& cfg,State& state,int width,int height,int players=-1,
    int chams_state=0,int camera_state=0,int shader_stage=0,int model_state=0,
    int auto_state=0,int farm_state=0,int resource_state=0) {
    russian_labels=cfg.reference.russian;
    Result result;
    const auto bar=state.island.draw(state.open,cfg.reference.dark,cfg.reference.fps,cfg.reference.russian,players);
    result.bar_min=bar.Min;result.bar_max=bar.Max;
    state.notification.draw(xvcen_assets::smoothstep(state.island.theme));
    state.menu_motion.update(state.open,ImGui::GetIO().DeltaTime);
    result.open=state.open;if(!state.open&&state.menu_motion.phase<=0)return result;
    const float s=state.scale,dt=std::clamp(ImGui::GetIO().DeltaTime,.0005f,.05f);
    state.animation_time+=dt;state.active_tab=std::clamp(state.active_tab,0,5);
    const float menu_w=840*s,menu_h=720*s,sidebar=265*s;
    ImGui::SetNextWindowSize({menu_w,menu_h});
    const bool resized=state.previous_display.x!=width||state.previous_display.y!=height;
    ImVec2 window_pos{(width-menu_w)*.5f,(height-menu_h)*.5f};
    if(auto* prior=ImGui::FindWindowByName("##MangoMenu"))if(!resized)window_pos=prior->Pos;
    auto& input=ImGui::GetIO();
    const bool blocked=!state.menu_motion.settled(true)||GImGui->OpenPopupStack.Size>0;
    if(resized){state.tracking_window=state.dragging_window=false;state.jelly={};}
    if(input.MouseClicked[0]&&!blocked){
        const auto mouse=input.MousePos;const float x=mouse.x-window_pos.x,y=mouse.y-window_pos.y;
        state.tracking_window=x>=0&&x<menu_w&&y>=0&&y<(x<265*s?120:66)*s;
        state.drag_pointer=mouse;state.drag_origin=window_pos;
    }
    if(state.tracking_window&&input.MouseDown[0]&&!blocked){
        const ImVec2 delta{input.MousePos.x-state.drag_pointer.x,input.MousePos.y-state.drag_pointer.y};
        if(!state.dragging_window&&std::fabs(delta.x)+std::fabs(delta.y)>8*s){
            state.dragging_window=true;state.jelly.begin(state.drag_origin,{menu_w,menu_h},state.drag_pointer);
        }
        if(state.dragging_window){window_pos={std::clamp(state.drag_origin.x+delta.x,0.f,std::max(0.f,width-menu_w)),std::clamp(state.drag_origin.y+delta.y,0.f,std::max(0.f,height-menu_h))};ImGui::ClearActiveID();}
    }
    if(!input.MouseDown[0]||blocked){state.tracking_window=false;state.dragging_window=false;}
    state.jelly.update(window_pos,{menu_w,menu_h},input.DeltaTime,state.dragging_window);
    ImGui::SetNextWindowPos(window_pos,ImGuiCond_Always);
    state.previous_display={float(width),float(height)};
    const auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoMove;
    ImGui::Begin("##MangoMenu",nullptr,flags);
    auto* menu_window=ImGui::GetCurrentWindow();
    ImGui::BeginDisabled(!state.menu_motion.settled(true));
    const auto pos=ImGui::GetWindowPos();result.min=pos;result.max={pos.x+menu_w,pos.y+menu_h};
    auto* draw=ImGui::GetWindowDrawList();
    const float dark=Smooth(state.island.theme);
    draw->AddRectFilled(pos,result.max,Color(.949f-.799f*dark,.949f-.799f*dark,.969f-.809f*dark),24*s);
    draw->AddRectFilled(pos,{pos.x+sidebar,pos.y+menu_h},Color(.969f-.832f*dark,.969f-.832f*dark,.98f-.835f*dark),
        24*s,ImDrawFlags_RoundCornersLeft);
    const ImU32 accent=Color(.039f*dark,.478f+.04000002f*dark,1),ink=Color(dark,dark,dark),muted=Color(.557f,.557f,.576f);
    const ImU32 lights[]={IM_COL32(255,95,87,255),IM_COL32(255,189,46,255),IM_COL32(40,200,64,255)};
    for(int i=0;i<3;++i){
        const ImVec2 center{pos.x+(28+34*i)*s,pos.y+32*s};
        draw->AddCircleFilled(center,13*s,lights[i],48);
        draw->AddCircle(center,14.2f*s,Color(dark,dark,dark,.706f+.118f*dark),48,1.5f*s);
        ImGui::SetCursorPos({(8+34*i)*s,12*s});ImGui::PushID(i);
        if(ImGui::InvisibleButton("##window-light",{40*s,40*s})&&Tapped())state.open=false;
        ImGui::PopID();
    }
    draw->AddCircleFilled({pos.x+134.5f*s,pos.y+137.2f*s},74.2f*s,IM_COL32(0,0,0,20),64);
    draw->AddCircleFilled({pos.x+132.5f*s,pos.y+134.2f*s},74.2f*s,Color(1-.827f*dark,1-.827f*dark,1-.82f*dark),64);
    const int frame=int(state.animation_time*30)%74,row=frame/8,column=frame%8;
    xvcen_assets::atlas.image(draw,xvcen_assets::Avatar,{pos.x+58.3f*s,pos.y+60*s},{pos.x+206.7f*s,pos.y+208.4f*s},
        {column*.125f,row*.1f},{(column+1)*.125f,(row+1)*.1f},IM_COL32_WHITE,74.2f*s);
    draw->AddCircle({pos.x+132.5f*s,pos.y+134.2f*s},76.7f*s,Color(.039f*dark,.478f+.04000002f*dark,1,.72f+.18f*dark),64);
    draw->AddLine({pos.x+20*s,pos.y+224.4f*s},{pos.x+245*s,pos.y+224.4f*s},
        Color(.82f-.56f*dark,.82f-.56f*dark,.84f-.55f*dark),.8f*s);
    const char* tabs[]={"Home","Aimbot","Visuals","Memory","Other","Settings"};
    const xvcen_assets::Asset icons[]={xvcen_assets::Home,xvcen_assets::Aim,xvcen_assets::Visuals,
        xvcen_assets::Memory,xvcen_assets::Other,xvcen_assets::Settings};
    const float tab_target=226.4f+70*state.active_tab;
    state.selection_velocity+=dt*(180*(tab_target-state.selection_y)-26*state.selection_velocity);
    state.selection_y+=dt*state.selection_velocity;
    draw->AddRectFilled({pos.x+8*s,pos.y+(state.selection_y+2)*s},{pos.x+257*s,pos.y+(state.selection_y+68)*s},
        Color(.82f-.56f*dark,.82f-.56f*dark,.84f-.55f*dark),16*s);
    draw->AddRect({pos.x+8*s,pos.y+(state.selection_y+2)*s},{pos.x+257*s,pos.y+(state.selection_y+68)*s},accent,16*s,0,4*s);
    for(int i=0;i<6;++i){
        const float y=(226.4f+70*i)*s;ImGui::SetCursorPos({0,y});ImGui::PushID(i);
        if(ImGui::InvisibleButton("##tab",{sidebar,70*s})&&Tapped()){state.active_tab=i;state.dragging_content=false;}
        ImGui::PopID();
        xvcen_assets::atlas.image(draw,icons[i],{pos.x+18*s,pos.y+y+9*s},{pos.x+70*s,pos.y+y+61*s},
            {0,0},{1,1},IM_COL32_WHITE,10*s);
        const float font=ImGui::GetFontSize()*1.15f;
        Text(draw,{pos.x+86*s,pos.y+y+35*s-font*.5f},tabs[i],font,state.active_tab==i?accent:muted);
    }
    const float title_font=ImGui::GetFontSize()*1.45f;const auto title_size=TextSize(tabs[state.active_tab],title_font);
    Text(draw,{pos.x+sidebar+(menu_w-sidebar-title_size.x)*.5f,pos.y+33*s-title_size.y*.5f},tabs[state.active_tab],title_font,ink);
    if(state.active_tab==5){
        const ImVec2 selector{pos.x+menu_w-144*s,pos.y+16*s};
        draw->AddRectFilled(selector,{selector.x+128*s,selector.y+48*s},
            Color(.82f-.56f*dark,.82f-.56f*dark,.84f-.55f*dark),16*s);
        const float sx=selector.x+(cfg.reference.russian?64:0)*s;
        draw->AddRectFilled({sx+3*s,selector.y+3*s},{sx+61*s,selector.y+45*s},accent,14*s);
        for(int i=0;i<2;++i){
            ImGui::SetCursorScreenPos({selector.x+i*64*s,selector.y});ImGui::PushID(i);
            if(ImGui::InvisibleButton("##language",{64*s,48*s})&&Tapped()){
                cfg.reference.russian=i!=0;result.changed=true;
            }
            ImGui::PopID();Text(draw,{selector.x+(i*64+12)*s,selector.y+9*s},i?"RU":"EN",24*s,
                cfg.reference.russian==(i!=0)?IM_COL32_WHITE:muted);
        }
    }
    ImGui::SetCursorPos({sidebar,70*s});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,Color(.557f,.557f,.576f,.55f));
    ImGui::BeginChild("##ReferenceContent",{menu_w-sidebar,menu_h-82*s},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    if(state.previous_tab!=state.active_tab){
        ImGui::SetScrollY(0);
        if(state.previous_tab>=0){state.content_alpha=0;state.content_slide={50,0};}
        state.previous_tab=state.active_tab;
    }
    state.content_alpha+=dt*(1-state.content_alpha)*5;
    if(state.content_alpha>.999f)state.content_alpha=1;
    state.content_slide.step(0,dt,180,26);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY()+state.content_slide.value*s);
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem|ImGuiHoveredFlags_ChildWindows)&&ImGui::IsMouseDown(0)){
        const auto delta=ImGui::GetMouseDragDelta(0);
        if(std::fabs(delta.y)>10*s&&std::fabs(delta.y)>std::fabs(delta.x))state.dragging_content=true;
        if(state.dragging_content){ImGui::SetScrollY(ImGui::GetScrollY()-ImGui::GetIO().MouseDelta.y);ImGui::ClearActiveID();}
    }
    Rows rows{state,result,cfg.reference,dark,s};
    if(state.active_tab==0){
        rows.card("Home",3);rows.action("Game","Oxide");rows.action("Version","r40");rows.action("Status","Active");
        rows.card("Configs",3);
        if(rows.action("Save")){result.changed=true;Notify(state,"Config saved",true);}
        if(rows.action("Reset appearance")){
            cfg.reference=mango_reference::Options{};cfg.color[0]=cfg.color[1]=cfg.color[2]=1;result.changed=true;
        }
        if(rows.action("Reset Defaults")){cfg=Settings{};result.changed=true;}
    }else if(state.active_tab==1){
        rows.card("Aimbot",3);rows.toggle("Enable aimbot",cfg.magic_bullet);
        rows.toggle("Visibility check",cfg.visibility_check);rows.toggle("Triggerbot",cfg.extra.auto_shoot,true);
        rows.card("Aimbot FOV",2,93);rows.slider("FOV radius",cfg.magic_fov,25,800);rows.toggle("In scope",cfg.magic_ads_only,true);
        rows.card("Advanced settings",1);rows.toggle("Ignore teammates",cfg.magic_team_filter,true);
        rows.card("Target",2,93);
        if(rows.action("Target Point",cfg.magic_point?"Body":"Head")&&!cfg.magic_bullet){cfg.magic_point=cfg.magic_point?0:1;result.changed=true;}
        rows.slider("Range",cfg.range,25,1000,"%.0f m");
    }else if(state.active_tab==2){
        rows.card("ESP",9);rows.toggle("Box",cfg.box,false,cfg.color,3);
        rows.toggle("Names",cfg.reference.names,false,cfg.reference.name_color);
        rows.toggle("Skeleton",cfg.skeleton,false,cfg.reference.skeleton_color);
        rows.toggle("Health",cfg.reference.health);rows.toggle("Tracers",cfg.lines,false,cfg.reference.tracer_color);
        rows.toggle("Distance",cfg.distances,false,cfg.reference.distance_color);
        rows.toggle("Target arrow",cfg.reference.target_arrow);rows.toggle("Visibility check",cfg.visibility_check);
        rows.toggle("Visible only",cfg.visible_only,true);
        rows.card("Ore ESP",4);rows.toggle("Resource ESP",cfg.extra.resource_esp);
        const char* ore_labels[]={"Sulfur ore","Iron ore","Stone ore"};const unsigned ore_masks[]={8,4,2};
        for(int i=0;i<3;++i){bool enabled=cfg.extra.resources&ore_masks[i];
            if(rows.toggle(ore_labels[i],enabled,i==2))cfg.extra.resources=enabled?cfg.extra.resources|ore_masks[i]:cfg.extra.resources&~ore_masks[i];}
        rows.card("Appearance",2,108);rows.slider("Thickness",cfg.reference.thickness,.5f,4.f,"%.2f");rows.slider("Distance",cfg.range,25,1000,"%.0f m");
    }else if(state.active_tab==3){
        rows.card("Memory",4);rows.toggle("No recoil",cfg.no_recoil);rows.toggle("No spread",cfg.no_spread);
        rows.toggle("No weapon motion",cfg.no_weapon_motion);rows.toggle("Third person",cfg.third_person,true);
        rows.card("Camera",1,108);rows.slider("Distance",cfg.camera_distance,.8f,5.f,"%.2f m");
        rows.card("Model",1);const bool previous_model=cfg.custom_model;
        rows.toggle("Custom model",cfg.custom_model,true);if(cfg.custom_model&&!previous_model)cfg.third_person=true;
    }else if(state.active_tab==4){
        rows.card("Resources",6);rows.toggle("Resource ESP",cfg.extra.resource_esp);rows.toggle("Magic Farm",cfg.extra.farm);
        const char* res_labels[]={"Wood","Stone","Iron","Sulfur"};
        for(int i=0;i<4;++i){bool enabled=cfg.extra.resources&(1u<<i);
            if(rows.toggle(res_labels[i],enabled,i==3))cfg.extra.resources=enabled?cfg.extra.resources|(1u<<i):cfg.extra.resources&~(1u<<i);}
        rows.card("Resource settings",3,108);
        rows.slider("ESP distance",cfg.extra.resource_range,25,500,"%.0f m");
        float res_limit=float(cfg.extra.resource_limit);rows.slider("ESP labels",res_limit,8,128);
        cfg.extra.resource_limit=int(res_limit);rows.slider("Farm distance",cfg.extra.farm_range,2,20,"%.1f m");
        rows.card("Chams",1);rows.toggle("Chams",cfg.chams,true);
        const char* presets[]={"Classic","Ice","Toxic","Ghost","Solid","Pulse","Rainbow","Custom"};
        const char* styles[]={"Flat","Soft","Pulse","Rainbow"};
        rows.card("Chams appearance",2);
        if(rows.action("Preset",presets[std::clamp(cfg.chams_appearance.preset,0,7)])){
            mango_chams::apply_preset(cfg.chams_appearance,cfg.chams_alpha,(cfg.chams_appearance.preset+1)%8);result.changed=true;
        }
        if(rows.action("Style",styles[std::clamp(cfg.chams_appearance.style,0,3)])){
            cfg.chams_appearance.style=(cfg.chams_appearance.style+1)%4;
            cfg.chams_appearance.preset=mango_chams::custom_preset;result.changed=true;
        }
        rows.card("Chams colors",3);
        const char* color_labels[]={"Visible","Occluded","Unknown"};
        for(int i=0;i<3;++i){
            ImGui::BeginDisabled(cfg.chams_appearance.style==mango_chams::Rainbow&&i<2);
            if(rows.edit_color(color_labels[i],cfg.chams_appearance.rgb[i]))cfg.chams_appearance.preset=mango_chams::custom_preset;
            ImGui::EndDisabled();
        }
        rows.card("Chams settings",4,108);
        rows.slider("Opacity",cfg.chams_alpha,.05f,1.f,"%.2f");
        rows.slider("Speed",cfg.chams_appearance.speed,.25f,3.f,"%.2fx");
        rows.slider("Range",cfg.chams_range,15,150,"%.0f m");
        float chams_models=float(cfg.chams_limit);rows.slider("Models",chams_models,4,32);cfg.chams_limit=int(chams_models);
        rows.card("Weapon",2);rows.toggle("Weapon offset",cfg.extra.weapon);
        rows.toggle("Apply while aiming",cfg.extra.weapon_ads,true);
        rows.card("Weapon position",6,108);
        rows.slider("X",cfg.extra.position.x,-.5f,.5f,"%.2f m");
        rows.slider("Y",cfg.extra.position.y,-.5f,.5f,"%.2f m");
        rows.slider("Z",cfg.extra.position.z,-.5f,.5f,"%.2f m");
        rows.slider("Pitch",cfg.extra.rotation.x,-60,60,"%.0f deg");
        rows.slider("Yaw",cfg.extra.rotation.y,-60,60,"%.0f deg");
        rows.slider("Roll",cfg.extra.rotation.z,-60,60,"%.0f deg");
        rows.card("Reset",1);
        if(rows.action("Reset weapon position")){cfg.extra.position={};cfg.extra.rotation={};result.changed=true;}
    }else{
        rows.card("Interface",3);rows.toggle("Dark theme",cfg.reference.dark);rows.toggle("Row separators",cfg.reference.separators);
        rows.toggle("Show fps",cfg.reference.fps,true);
        rows.card("ESP appearance",3);rows.toggle("Corner box",cfg.corners);rows.toggle("Fill",cfg.fill);rows.toggle("ESP enabled",cfg.enabled,true);
        rows.card("Menu",1);if(rows.action("Close menu"))state.open=false;
    }
    ImGui::Dummy({0,20*s});
    if(state.content_alpha<.999f){
        const auto* content=ImGui::GetCurrentWindow();
        content->DrawList->AddRectFilled(content->InnerRect.Min,content->InnerRect.Max,
            Color(.949f-.799f*dark,.949f-.799f*dark,.969f-.809f*dark,1-state.content_alpha));
    }
    ImGui::EndChild();ImGui::PopStyleColor();
    const bool color_changed=state.colors.draw(pos,{menu_w,menu_h},s,dark,cfg.reference.russian);
    if(color_changed&&state.colors_are_chams)cfg.chams_appearance.preset=mango_chams::custom_preset;
    result.changed|=color_changed;
    if(ImGui::IsMouseReleased(0))state.dragging_content=false;
    result.popup|=GImGui->OpenPopupStack.Size>0;
    ImGui::EndDisabled();ImGui::End();result.open=state.open;
    if(!state.menu_motion.settled(true)||state.jelly.visible()){
        const float radius=std::min(66.f*state.island.scale(),height*.12f);
        const ImVec2 target_min{width*.5f-radius,float(height)},target_max{width*.5f+radius,height+radius*2};
        const float amount=state.menu_motion.position();
        TransformMenu(menu_window,target_min,target_max,amount,state.jelly.visible()&&state.menu_motion.settled(true)?&state.jelly:nullptr);
        const auto a=xvcen_assets::menu_warp(pos,pos,{menu_w,menu_h},target_min,target_max,amount);
        const auto b=xvcen_assets::menu_warp(result.max,pos,{menu_w,menu_h},target_min,target_max,amount);
        result.min={std::clamp(a.x,0.f,float(width)),std::clamp(a.y,0.f,float(height))};
        result.max={std::clamp(b.x,0.f,float(width)),std::clamp(b.y,0.f,float(height))};
    }
    (void)chams_state;(void)camera_state;(void)shader_stage;(void)model_state;
    (void)auto_state;(void)farm_state;(void)resource_state;return result;
}
}
