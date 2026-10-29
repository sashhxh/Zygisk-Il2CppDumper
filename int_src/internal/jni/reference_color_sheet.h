#pragma once
#include "reference_island.h"
#include "reference_palette.h"

namespace mango_reference {
struct ColorSheet {
    float* color=nullptr;
    int components=4;
    bool requested=false,closing=false;
    std::string title;
    Spring opening,selected_x,selected_y,selection;
    ImRect panel{},swatches[28]{},done{};
    void request(const char* name,float* target,int count) {
        title=name;color=target;components=count;requested=true;closing=false;
        opening={};selection={};
    }
    bool draw(ImVec2 origin,ImVec2 size,float scale,float dark,bool russian) {
        const ImGuiID id=ImGui::GetID("##ReferenceColorSheet");
        if(requested){ImGui::OpenPopupEx(id,ImGuiPopupFlags_None);requested=false;}
        if(!ImGui::IsPopupOpen(id,ImGuiPopupFlags_None)){color=nullptr;return false;}
        const float dt=ImGui::GetIO().DeltaTime;
        opening.step(closing?0.f:1.f,dt,120,22);opening.value=unit(opening.value);
        if(closing&&opening.value<=.001f){
            ImGui::ClosePopupToLevel(GImGui->BeginPopupStack.Size,true);color=nullptr;return false;
        }
        const float t=smoothstep(opening.value);
        ImGui::SetNextWindowPos(origin);ImGui::SetNextWindowSize(size);
        const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
            ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings;
        if(!ImGui::BeginPopupEx(id,flags)){color=nullptr;return false;}
        auto* draw=ImGui::GetWindowDrawList();bool changed=false;
        const float font=ImGui::GetFontSize(),w=size.x/scale,h=size.y/scale;
        auto p=[&](float x,float y){return ImVec2(origin.x+x*scale,origin.y+y*scale);};
        auto box=[&](float x,float y,float x1,float y1){return ImRect(p(x,y),p(x1,y1));};
        draw->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},rgba(0,0,0,.24f*t),24*scale);
        const float left=w*.04f,width=w*.92f,top=h-t*h*.94f,bottom=top+h*.88f;
        panel=box(left,top,left+width,bottom);
        draw->AddRectFilled(panel.Min,panel.Max,rgba(1-.827f*dark,1-.827f*dark,1-.82f*dark),26*scale);
        const float center=left+width*.5f;
        draw->AddRectFilled(p(center-36,top+14),p(center+36,top+21),rgba(.557f,.557f,.576f,.45f*t),3.5f*scale);
        const float heading=font*1.45f;
        const char* translated=translate(title.c_str(),russian);
        label(draw,{origin.x+center*scale-measure(translated,heading).x*.5f,origin.y+(top+28)*scale},
            heading,rgba(dark,dark,dark,t),translated);
        const ImVec2 close=p(left+width-42,top+28+heading/scale*.5f);
        draw->AddCircleFilled(close,26*scale,rgba(.82f-.56f*dark,.82f-.56f*dark,.84f-.55f*dark,t),32);
        for(int direction:{-1,1})draw->AddLine({close.x-9*scale,close.y-direction*9*scale},
            {close.x+9*scale,close.y+direction*9*scale},rgba(.557f,.557f,.576f,t),3.2f*scale);
        const float content=top+52+heading/scale;
        draw->AddLine(p(left+18,top+44+heading/scale),p(left+width-18,top+44+heading/scale),
            rgba(.82f-.56f*dark,.82f-.56f*dark,.84f-.55f*dark,t),2.5f*scale);
        const float end=bottom-72;
        const float cell=std::max(22.f,std::min(w*.13142858f-13.142858f,(end-content)*.25f-11.5f));
        const float grid_h=4*cell+30;
        const float grid_y=std::min(content+(end-content-grid_h)*.5f,bottom-80-grid_h);
        const float grid_x=left+16+(width-92-7*cell)*.5f;
        int selected=-1;float best=1e9f;
        for(int i=0;i<28&&color;++i){
            const auto& c=palette[i];float distance=0;
            for(int j=0;j<3;++j){const float delta=(&c.x)[j]-color[j];distance+=delta*delta;}
            if(distance<best){best=distance;selected=i;}
        }
        if(best>=.02f)selected=-1;
        auto released=[&](const ImRect& rect){
            const auto& io=ImGui::GetIO();const auto delta=ImGui::GetMouseDragDelta(0);
            return !closing&&io.MouseReleased[0]&&delta.x*delta.x+delta.y*delta.y<100&&
                rect.Contains(io.MousePos)&&rect.Contains(io.MouseClickedPos[0]);
        };
        draw->PushClipRect(p(left+8,content),p(left+width-8,bottom-78),true);
        for(int i=0;i<28;++i){
            const float x=grid_x+(i%7)*(cell+10),y=grid_y+(i/7)*(cell+10);
            swatches[i]=box(x,y,x+cell,y+cell);
            draw->AddRectFilled(p(x+1.5f,y+1.5f),p(x+cell+1.5f,y+cell+1.5f),rgba(0,0,0,18/255.f*t),10*scale);
            draw->AddRectFilled(swatches[i].Min,swatches[i].Max,ImGui::ColorConvertFloat4ToU32(palette[i]),10*scale);
            if(color&&released(swatches[i])){
                for(int j=0;j<components;++j)color[j]=(&palette[i].x)[j];selected=i;changed=true;
            }
        }
        if(selected>=0){
            const float x=(selected%7)*(cell+10)+cell*.5f,y=(selected/7)*(cell+10)+cell*.5f;
            if(selection.value<.001f){selected_x={x,0};selected_y={y,0};}
            selected_x.step(x,dt,180,26);selected_y.step(y,dt,180,26);
        }
        selection.step(selected>=0?1.f:0.f,dt,180,26);
        if(selection.value>.001f){
            const float radius=selection.value*(cell*.5f+5);
            const float x=grid_x+selected_x.value,y=grid_y+selected_y.value;
            draw->AddRect(p(x-radius,y-radius),p(x+radius,y+radius),rgba(0,0,0,t*220/255.f*selection.value),
                (10+5*selection.value)*scale,ImDrawFlags_None,4*scale);
            draw->AddRect(p(x-radius+2.5f,y-radius+2.5f),p(x+radius-2.5f,y+radius-2.5f),rgba(1,1,1,t*selection.value),
                (10+2.5f*selection.value)*scale,ImDrawFlags_None,1.5f*scale);
        }
        draw->PopClipRect();
        done=box(left+16,bottom-72,left+width-16,bottom-10);
        draw->AddRectFilled(done.Min,done.Max,rgba(.039f*dark,.478f+.04000002f*dark,1,t),18*scale);
        const char* done_text=translate("Done",russian);const auto extent=measure(done_text,font*1.2f);
        label(draw,{(done.Min.x+done.Max.x-extent.x)*.5f,(done.Min.y+done.Max.y-extent.y)*.5f},
            font*1.2f,rgba(1,1,1,t),done_text);
        const auto close_area=ImRect({close.x-38*scale,close.y-38*scale},{close.x+38*scale,close.y+38*scale});
        if(released(done)||released(close_area)||ImGui::IsKeyPressed(ImGuiKey_Escape))closing=true;
        ImGui::EndPopup();return changed;
    }
};
}
