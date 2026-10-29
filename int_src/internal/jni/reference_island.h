#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "reference_atlas.h"
#include "reference_labels.h"
#include "reference_motion.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace mango_reference {
inline ImU32 rgba(float r,float g,float b,float a=1){return ImGui::ColorConvertFloat4ToU32({r,g,b,a});}
inline ImVec2 measure(const char* text,float size){return atlas.text->CalcTextSizeA(size,FLT_MAX,0,text);}
inline void label(ImDrawList* draw,ImVec2 p,float size,ImU32 color,const char* text) {
    draw->AddText(atlas.text,size,p,color,text);
}
struct Island {
    RollingCounter enemies,bots;
    float theme=0,pulse=0,fps_blend=0,refresh=60;
    ImRect bounds{};
    float scale() const {
        const auto screen=ImGui::GetIO().DisplaySize;
        return std::clamp(std::min({screen.x/1920.f,screen.y/1080.f,screen.x/720.f}),.55f,1.08f);
    }
    void digits(ImDrawList* draw,ImVec2 center,float value,float size,float width,ImU32 color) {
        value=std::clamp(value,0.f,999.f);
        int lower=int(value);float fraction=value-lower;
        const float h=measure("0",size).y,travel=h+7;
        draw->PushClipRect({center.x-width*.5f,center.y-h*.62f},{center.x+width*.5f,center.y+h*.62f},true);
        auto paint=[&](int number,float top,float alpha){
            char text[8];std::snprintf(text,sizeof(text),"%d",std::clamp(number,0,999));
            const float x=center.x-measure(text,size).x*.5f;
            const auto c=(color&0xffffffu)|(unsigned(float(color>>24)*alpha)<<24);
            label(draw,{x,top},size,c,text);
            label(draw,{x+.33f*scale(),top},size,(c&0xffffffu)|(unsigned(float(c>>24)*.34f)<<24),text);
        };
        if(fraction<=.001f)paint(lower,center.y-h*.5f,1);
        else{
            paint(lower,center.y-h*.5f-fraction*travel,1-fraction);
            paint(std::min(lower+1,999),center.y-h*.5f+(1-fraction)*travel,fraction);
        }
        draw->PopClipRect();
    }
    ImRect draw(bool& open,bool dark,bool show_fps,bool russian,int player_count,int bot_count=0) {
        auto& io=ImGui::GetIO();const float dt=std::clamp(io.DeltaTime,0.f,.1f);
        const float factor=1-std::exp(-6*std::clamp(dt,.0005f,.05f));
        theme+=(float(dark)-theme)*factor;if(std::fabs(theme-float(dark))<.002f)theme=float(dark);
        const float d=smoothstep(theme),s=scale(),font=27*s,numeric=33*s;
        enemies.update(player_count,dt);bots.update(bot_count,dt);pulse+=dt*3.5f;
        const char* enemy_label=translate("Enemies:",russian);
        const char* bot_label=translate("Bots:",russian);
        const float ew=measure(enemy_label,font).x+6*s,bw=measure(bot_label,font).x+6*s;
        const float separator=measure("|",font).x,number_width=measure("99",numeric).x+4*s;
        const float slot=number_width+12*s;
        const float width=36*s+ew+bw+2*slot+separator;
        const float left=(io.DisplaySize.x-width)*.5f,top=std::clamp(io.DisplaySize.y*.035f,26.f,42.f);
        bounds=ImRect({left,top},{left+width,top+66*s});
        const bool hover=bounds.Contains(io.MousePos);
        if(hover&&io.MouseClicked[0]){
            open=!open;pulse=0;ImGui::ClearActiveID();
            if(!open&&GImGui->OpenPopupStack.Size)ImGui::ClosePopupToLevel(0,true);
        }
        auto* draw=ImGui::GetForegroundDrawList();const auto accent=rgba(.039f*d,.478f+.04000002f*d,1);
        draw->AddRectFilled({left+2,top+7},{bounds.Max.x+2,bounds.Max.y+7},IM_COL32(0,0,0,29),33*s);
        draw->AddRectFilled({left-2,top-2},{bounds.Max.x+2,bounds.Max.y+2},
            (accent&0xffffffu)|(unsigned((hover?.11f:.055f)*255)<<24),33*s+2);
        draw->AddRectFilled(bounds.Min,bounds.Max,rgba(1-.827f*d,1-.827f*d,1-.82f*d),33*s);
        draw->AddRect(bounds.Min,bounds.Max,(accent&0xffffffu)|(unsigned((hover?.68f:.4f)*255)<<24),33*s,0,1.35f);
        draw->AddLine({left+23.76f*s,top+1},{bounds.Max.x-23.76f*s,top+1},
            rgba(d,d,d,dark?.09f:.18f));
        if(pulse<1){
            const float t=1-std::pow(1-pulse,3.f),expand=s*t*20;
            draw->AddRect({left-expand,top-expand},{bounds.Max.x+expand,bounds.Max.y+expand},
                (accent&0xffffffu)|(unsigned((.24f-t*.24f)*255)<<24),33*s+expand,2.f,ImDrawFlags_None);
        }
        const float cy=top+33*s,weight=std::max(.28f,.324f*s);
        auto paint_label=[&](const char* text,float x,ImU32 color){
            const float y=cy-measure(text,font).y*.5f;
            label(draw,{x,y},font,color,text);
            label(draw,{x+weight,y},font,(color&0xffffffu)|(unsigned(float(color>>24)*.34f)<<24),text);
        };
        float x=left+18*s;
        paint_label(enemy_label,x,rgba(d,d,d));x+=ew;
        digits(draw,{x+number_width*.5f,cy},enemies.number.value,numeric,number_width,
            rgba(1,.231f+.04f*d,.188f+.039f*d));
        x+=slot;paint_label("|",x,rgba(.557f,.557f,.576f,.72f));x+=separator+12*s;
        paint_label(bot_label,x,rgba(d,d,d));x+=bw;
        digits(draw,{x+number_width*.5f,cy},bots.number.value,numeric,number_width,rgba(0,.478f,1));
        const float sample=io.Framerate>=1?io.Framerate:60;float nearest=sample,distance=1e9f;
        for(float rate:{60.f,90.f,120.f,144.f,165.f,180.f,240.f})
            if(std::fabs(sample-rate)<distance){distance=std::fabs(sample-rate);nearest=rate;}
        const float target=distance<=8?nearest:sample;
        const float next=refresh+(target-refresh)*std::min(dt,.05f)*8;
        refresh=next>=1?next:target;
        fps_blend=unit(fps_blend+std::min(dt,.032f)*(show_fps?3.6f:-3.6f));
        if(fps_blend>.004f){
            const float caption=19*s,caption_w=measure("FPS",caption).x;
            const float value_w=measure("240",font).x;
            const float full=caption_w+8*s+28*s+value_w,t=fps_blend-1;
            const float expanded=full*(1+t*t*(2.2f*t+1.2f));
            if(expanded>1){
                const float fx=bounds.Max.x+11*s;
                draw->PushClipRect({fx-1,top-6},{fx+expanded+2,bounds.Max.y+6},true);
                draw->AddRectFilled({fx+1,top+5},{fx+expanded+1,bounds.Max.y+5},IM_COL32(0,0,0,23),33*s);
                draw->AddRectFilled({fx,top},{fx+expanded,bounds.Max.y},rgba(1-.827f*d,1-.827f*d,1-.82f*d),33*s);
                draw->AddRect({fx,top},{fx+expanded,bounds.Max.y},
                    (accent&0xffffffu)|(unsigned((1-std::pow(1-fps_blend,3.f))*.35f*255)<<24),33*s);
                const float a=unit(fps_blend/.42f);
                label(draw,{fx+14*s,cy-measure("FPS",caption).y*.5f},caption,rgba(.557f,.557f,.576f,a),"FPS");
                char text[16];std::snprintf(text,sizeof(text),"%.0f",double(refresh));
                const float fade=1-std::pow(1-unit(fps_blend/.42f-.71428573f),3.f);
                label(draw,{fx+14*s+caption_w+8*s+(value_w-measure(text,font).x)*.5f+(1-fade)*16,
                    cy-measure(text,font).y*.5f},font,rgba(.039f*d,.478f+.04000002f*d,1,fade),text);
                draw->PopClipRect();
            }
        }
        return bounds;
    }
};

struct Notification {
    bool active=false,closing=false,queued=false;
    std::string text,pending;
    float age=0,opacity=0;
    Spring position,progress{1,0},width;
    void push(const char* message,bool enabled) {
        const std::string next=std::string(message)+"|"+(enabled?"ON":"OFF");
        age=0;
        if(!active||closing){
            text=next;active=true;closing=false;queued=false;opacity=0;
            position={};progress={1,0};width={};
        }else{pending=next;queued=true;}
    }
    void draw(float dark) {
        if(!active)return;
        const float dt=std::clamp(ImGui::GetIO().DeltaTime,0.f,.1f);age+=dt;
        if(queued&&opacity<.04f){text=pending;pending.clear();queued=false;progress={1,0};}
        if(age>2.6f&&!queued)closing=true;
        position.step(closing?0.f:1.f,dt,120,22);
        position.value=unit(position.value);
        if(closing&&position.value<=.001f){active=false;position={};return;}
        opacity=unit(opacity+((queued?0.f:1.f)-opacity)*dt*14);
        progress.step(queued?1.f:std::max(0.f,1-age/2.6f),dt,220,30);progress.value=unit(progress.value);
        const auto split=text.find('|');
        const std::string name=text.substr(0,split),status=split==std::string::npos?"":text.substr(split+1);
        const float font=ImGui::GetFontSize()*1.55f;
        const auto extent=measure(name.c_str(),font);const float separator=measure(" - ",font).x;
        const float target=extent.x+56+(status.empty()?0:separator+measure(status.c_str(),font).x);
        if(width.value<8)width.value=target;
        width.step(target,dt,220,28);
        if(std::fabs(target-width.value)<.6f&&std::fabs(width.velocity)<4)width={target,0};
        const float phase=smoothstep(position.value),travel=(-70-extent.y)*(1-phase);
        const float right=ImGui::GetIO().DisplaySize.x-16,left=right-width.value;
        const float top=travel+16,bottom=top+extent.y+50;
        auto* draw=ImGui::GetForegroundDrawList();
        const ImU32 accent=rgba(.039f*dark,.478f+.04000002f*dark,1);
        draw->AddRectFilled({left+2,top+4},{right+2,bottom+4},IM_COL32(0,0,0,int(phase*26)),26);
        draw->AddRectFilled({left,top},{right,bottom},rgba(1-.827f*dark,1-.827f*dark,1-.82f*dark,phase*245/255.f),26);
        draw->AddRect({left,top},{right,bottom},(accent&0xffffffu)|(unsigned(phase*.32f*255)<<24),26,0,1.3f);
        draw->PushClipRect({left+1,top+1},{right-1,bottom-1},true);
        const float text_alpha=opacity*phase;float x=left+28;
        label(draw,{x,travel+36},font,rgba(dark,dark,dark,text_alpha),name.c_str());x+=extent.x;
        if(!status.empty()){
            label(draw,{x,travel+36},font,rgba(.557f,.557f,.576f,text_alpha*170/255.f)," - ");x+=separator;
            const ImU32 status_color=status=="ON"?IM_COL32(49,214,74,int(text_alpha*255)):IM_COL32(255,58,47,int(text_alpha*255));
            label(draw,{x,travel+36},font,status_color,status.c_str());
        }
        const float progress_x=left+22+(width.value-28)*progress.value;
        if(progress_x>left+32)draw->AddRectFilled({left+22,bottom-18},{progress_x,bottom-8},
            (accent&0xffffffu)|(unsigned(phase*215)<<24),5);
        draw->PopClipRect();
    }
};
}
