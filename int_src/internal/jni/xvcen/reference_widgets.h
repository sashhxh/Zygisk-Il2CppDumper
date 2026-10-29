#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// Reconstructed from the named ARM64 helpers. Geometry is tested against
// isolated original machine-code traces, not against a hand-written mock.
namespace xvcen_reference {
inline float smooth(float t){return t*t*(3-2*t);}
inline ImU32 rgba(float r,float g,float b,float a=1){
    auto byte=[](float v){return unsigned(std::clamp(v,0.f,1.f)*255.f+.5f);};
    return byte(r)|(byte(g)<<8)|(byte(b)<<16)|(byte(a)<<24);
}
inline ImU32 border(float d,float alpha=1){return rgba(.82f-.56f*d,.82f-.56f*d,.84f-.5499999523162842f*d,alpha);}
inline ImU32 accent(float d,float alpha=1){return rgba(.039f*d,.478f+.04000002f*d,1,alpha);}
inline ImU32 track(float d,float alpha=1){return rgba(.776f-.545f*d,.776f-.545f*d,.8f-.549f*d,alpha);}
template<class Draw> struct Widgets {
    Draw& draw;ImFont* font;float font_size,dark,scale=1,alpha=1;
    void card(ImVec2 p,float width,float height,bool separators,int flags=0){ // 0x6c710
        draw.AddRectFilled({p.x+16*scale,p.y},{p.x+width-16*scale,p.y+height*scale},rgba(1-.827f*dark,1-.827f*dark,1-.82f*dark),24*scale,flags);
        if(separators)draw.AddRect({p.x+16*scale,p.y},{p.x+width-16*scale,p.y+height*scale},border(dark),24*scale,flags,1.2f*scale);
    }
    void switch_at(ImVec2 p,float t){ // 0x6f6a0: receives eased progress
        const float off=.776f-.545f*dark;
        // The original switch truncates channels; other helpers round via ImGui.
        const ImU32 color=unsigned((off+t*(dark*.039f-off))*255.f)|
            (unsigned((off+t*((dark*.04000002f+.478f)-off))*255.f)<<8)|
            (unsigned(((.8f-dark*.549f)+t*(dark*.549f+.19999999f))*255.f)<<16)|0xff000000u;
        draw.AddRectFilled({p.x,p.y-21*scale},{p.x+72*scale,p.y+21*scale},color,21*scale,0);
        const float x=p.x+(23+26*t)*scale;
        draw.AddCircleFilled({x+.5f*scale,p.y+1.5f*scale},18.5f*scale,0x23000000,0);
        draw.AddCircleFilled({x,p.y},17.5f*scale,0xffffffff,0);
    }
    void toggle(ImVec2 p,float width,const char* label,float progress,bool separators,bool last){ // 0x6b7f0
        draw.PushClipRect({p.x+36*scale,p.y},{p.x+width-124*scale,p.y+78*scale},true);
        draw.AddText(font,font_size*1.15f,{p.x+36*scale,p.y+39*scale-font_size*.575f},rgba(dark,dark,dark),label);
        draw.PopClipRect();switch_at({p.x+width-108*scale,p.y+39*scale},smooth(progress));
        if(separators&&!last)draw.AddLine({p.x+36*scale,p.y+77.5f*scale},{p.x+width-36*scale,p.y+77.5f*scale},border(dark),.8f*scale);
    }
    void navigation(ImVec2 p,float width,const char* label){ // 0x6dee0, card handled by caller
        draw.AddText(font,font_size*1.15f,{p.x+36*scale,p.y+39*scale-font_size*.575f},rgba(dark,dark,dark),label);
        const ImVec2 c{p.x+width-48*scale,p.y+39*scale};
        draw.AddCircleFilled({c.x+.5f*scale,c.y+1.5f*scale},23*scale,0x1c000000,48);
        draw.AddCircleFilled(c,22*scale,accent(dark),48);
        draw.AddCircle(c,24.5f*scale,unsigned(dark*255.f)|(unsigned(dark*205.f+50.f)<<8)|(unsigned(dark*85.f+170.f)<<16)|(unsigned(dark*-70.f+130.f)<<24),48,1.5f*scale);
        draw.AddLine({c.x-4.5f*scale,c.y-8*scale},{c.x+5.5f*scale,c.y},0xfaffffff,3*scale);
        draw.AddLine({c.x+5.5f*scale,c.y},{c.x-4.5f*scale,c.y+8*scale},0xfaffffff,3*scale);
    }
    template<class Measure> void counter(ImVec2 center,float value,float size,float width,ImU32 color,Measure measure){ // 0x6dd10 + 0x957a0
        value=std::clamp(value,0.f,999.f);const int lower=int(value);const float fraction=value-lower;
        const float h=measure("0",size).y,travel=h+7;
        draw.PushClipRect({center.x-width*.5f,center.y-h*.62f},{center.x+width*.5f,center.y+h*.62f},true);
        auto paint=[&](int n,float y,float opacity){
            if(opacity<=.002f)return;
            char text[8];std::snprintf(text,sizeof(text),"%d",std::clamp(n,0,999));
            const float x=center.x-measure(text,size).x*.5f,half=std::max(.38f,size*.02f)*.5f;
            draw.AddText(font,size,{x+1,y+1},unsigned(opacity*70)<<24,text);
            const auto c=(color&0xffffffu)|(unsigned(opacity*255+.5f)<<24);
            draw.AddText(font,size,{x-half,y},c,text);draw.AddText(font,size,{x+half,y},c,text);
        };
        if(fraction<=.001f)paint(lower,center.y-h*.5f,1);
        else{paint(lower,center.y-h*.5f-fraction*travel,1-fraction);paint(std::min(lower+1,999),center.y-h*.5f+(1-fraction)*travel,fraction);}
        draw.PopClipRect();
    }
    template<class Measure> void island(ImVec2 p,float width,bool hover,bool dark_target,float pulse,
            const char* enemy_label,const char* bot_label,float enemies,float bots,Measure measure){ // 0x6c9f0
        const float s=scale,d=dark,right=p.x+width,bottom=p.y+66*s,cy=p.y+33*s;
        draw.AddRectFilled({p.x+2,p.y+7},{right+2,bottom+7},0x1d000000,33*s,0);
        draw.AddRectFilled({p.x-2,p.y-2},{right+2,bottom+2},accent(d,hover?.11f:.055f),33*s+2,0);
        draw.AddRectFilled(p,{right,bottom},rgba(1-.827f*d,1-.827f*d,1-.82f*d),33*s,0);
        draw.AddRect(p,{right,bottom},accent(d,hover?.68f:.4f),33*s,0,1.35f);
        draw.AddLine({p.x+23.76f*s,p.y+1},{right-23.76f*s,p.y+1},rgba(d,d,d,dark_target?.09f:.18f),1);
        if(pulse<1){const float t=1-(1-pulse)*(1-pulse)*(1-pulse),expand=s*t*20;
            draw.AddRect({p.x-expand,p.y-expand},{right+expand,bottom+expand},accent(d,.24f-t*.24f),33*s+expand,0,2.f);}
        const float label_size=27*s,numeric=33*s,stroke=std::max(.28f,.324f*s);
        auto paint=[&](const char* label,float x,ImU32 color,float opacity=1){const float y=cy-measure(label,label_size).y*.5f;
            draw.AddText(font,label_size,{x,y},color,label);
            draw.AddText(font,label_size,{x+stroke,y},(color&0xffffffu)|(unsigned(opacity*.34f*255+.5f)<<24),label);};
        const float ew=measure(enemy_label,label_size).x+6*s,bw=measure(bot_label,label_size).x+6*s;
        const float nw=measure("99",numeric).x+4*s,slot=nw+12*s,separator=measure("|",label_size).x;
        float x=p.x+18*s;paint(enemy_label,x,rgba(d,d,d));x+=ew;
        counter({x+nw*.5f,cy},enemies,numeric,nw,rgba(1,.231f+.04f*d,.188f+.039f*d),measure);
        x+=slot;paint("|",x,rgba(.557f,.557f,.576f,.72f),.72f);x+=separator+12*s;
        paint(bot_label,x,rgba(d,d,d));x+=bw;
        counter({x+nw*.5f,cy},bots,numeric,nw,rgba(0,.478f,1),measure);
    }
    void slider(ImVec2 p,float width,const char* label,const char* number,float metric_width,float progress,bool active,bool separators,bool last){ // 0x6c270
        const float value_x=p.x+width-36*scale-metric_width;
        draw.PushClipRect({p.x+36*scale,p.y},{value_x-20*scale,p.y+59.4f*scale},true);
        draw.AddText(font,font_size*1.15f,{p.x+36*scale,p.y+19*scale},rgba(dark,dark,dark,alpha),label);draw.PopClipRect();
        draw.AddText(font,font_size*1.15f,{value_x,p.y+19*scale},accent(dark,alpha),number);
        const float left=p.x+58*scale,right=p.x+width-58*scale,knob=left+progress*(width-116*scale),y=p.y+70*scale;
        draw.AddRectFilled({left,y-5.5f*scale},{right,y+5.5f*scale},track(dark,alpha),11*scale,0);
        draw.AddRectFilled({left,y-5.5f*scale},{knob,y+5.5f*scale},accent(dark,alpha),11*scale,0);
        draw.AddCircleFilled({knob+.5f*scale,y+1.5f*scale},17*scale,unsigned(alpha*30.f)<<24,0);
        draw.AddCircleFilled({knob,y},16*scale,(unsigned(alpha*255.f)<<24)|0xffffff,0);
        if(active)draw.AddCircle({knob,y},20*scale,rgba(0,.478f,1,alpha*.85f),48,3*scale);
        if(separators&&!last)draw.AddLine({p.x+36*scale,p.y+107.5f*scale},{p.x+width-36*scale,p.y+107.5f*scale},border(dark,alpha),.8f*scale);
    }
};
}
