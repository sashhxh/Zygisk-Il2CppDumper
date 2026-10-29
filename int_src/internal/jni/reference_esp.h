#pragma once
#include "imgui.h"
#include "reference_atlas.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mango_reference {
constexpr ImU32 esp_shadow=IM_COL32(3,5,8,190);
inline void esp_text(ImDrawList* draw,float center,float top,float size,ImU32 color,const char* text) {
    if(!text||!*text)return;
    auto* font=atlas.text;const auto extent=font->CalcTextSizeA(size,FLT_MAX,0,text);
    const auto screen=ImGui::GetIO().DisplaySize;
    const ImVec2 at{std::clamp(center-extent.x*.5f,2.f,std::max(2.f,screen.x-extent.x-2)),
        std::clamp(top,2.f,std::max(2.f,screen.y-extent.y-2))};
    const float weight=std::max(.34f,size*.024f)*.5f;
    draw->AddText(font,size,{at.x+1,at.y+1},esp_shadow,text);
    draw->AddText(font,size,{at.x-weight,at.y},color,text);
    draw->AddText(font,size,{at.x+weight,at.y},color,text);
}
inline void esp_box(ImDrawList* draw,ImVec2 lo,ImVec2 hi,float thickness,ImU32 color,bool corners=true) {
    if(hi.x-lo.x<2||hi.y-lo.y<2)return;
    thickness=std::clamp(thickness,.5f,4.f);
    if(!corners){
        draw->AddRect(lo,hi,esp_shadow,0,0,thickness+2);
        draw->AddRect(lo,hi,(color&0xffffffu)|0x4d000000u,0,0,thickness+1.2f);
        draw->AddRect(lo,hi,color,0,0,thickness);return;
    }
    const float length=std::clamp(std::min(hi.x-lo.x,hi.y-lo.y)*.28f,2.f,30.f);
    const ImVec2 segments[][2]={
        {lo,{lo.x+length,lo.y}},{lo,{lo.x,lo.y+length}},
        {{hi.x,lo.y},{hi.x-length,lo.y}},{{hi.x,lo.y},{hi.x,lo.y+length}},
        {{lo.x,hi.y},{lo.x+length,hi.y}},{{lo.x,hi.y},{lo.x,hi.y-length}},
        {hi,{hi.x-length,hi.y}},{hi,{hi.x,hi.y-length}}};
    for(const auto& line:segments)draw->AddLine(line[0],line[1],esp_shadow,thickness+2);
    for(const auto& line:segments)draw->AddLine(line[0],line[1],(color&0xffffffu)|0x4d000000u,thickness+1.2f);
    for(const auto& line:segments)draw->AddLine(line[0],line[1],color,thickness);
    for(ImVec2 p:{lo,ImVec2(hi.x,lo.y),ImVec2(lo.x,hi.y),hi})
        draw->AddCircleFilled(p,thickness*.6f,color,16);
}
inline void esp_health(ImDrawList* draw,ImVec2 lo,ImVec2 hi,float fraction) {
    if(!std::isfinite(fraction)||fraction<0)return;
    fraction=std::clamp(fraction,0.f,1.f);
    draw->AddRectFilled({lo.x-8,lo.y-1},{lo.x-3,hi.y+1},IM_COL32(2,4,7,205),1);
    const ImVec4 color{1-.8f*fraction,.2f+.65f*fraction,.1f+.25f*fraction,1};
    draw->AddRectFilled({lo.x-7,hi.y+(lo.y-hi.y)*fraction},{lo.x-4,hi.y},
        ImGui::ColorConvertFloat4ToU32(color),.8f);
}
inline void esp_tracer(ImDrawList* draw,ImVec2 target,float thickness,ImU32 color) {
    const ImVec2 start{ImGui::GetIO().DisplaySize.x*.5f,1};
    float dx=target.x-start.x,dy=target.y-start.y;const float length=std::sqrt(dx*dx+dy*dy);
    if(length<8)return;
    dx/=length;dy/=length;thickness=std::clamp(thickness,.65f,2.f);
    const ImVec2 end{target.x-dx*7,target.y-dy*7};
    draw->AddLine(start,end,esp_shadow,thickness+1.2f);draw->AddLine(start,end,color,thickness);
    draw->AddTriangleFilled({target.x,target.y+.6f},{end.x-dy*3.2f,end.y+dx*3.2f},
        {end.x+dy*3.2f,end.y-dx*3.2f},esp_shadow);
    draw->AddTriangleFilled({target.x-dx*.9f,target.y-dy*.9f},{end.x-dy*2.3f,end.y+dx*2.3f},
        {end.x+dy*2.3f,end.y-dx*2.3f},color);
}
inline void esp_arrow(ImDrawList* draw,ImVec2 target,ImU32 color) {
    const auto size=ImGui::GetIO().DisplaySize;
    const ImVec2 center{size.x*.5f,size.y*.5f};
    float dx=target.x-center.x,dy=target.y-center.y;
    const float length=std::sqrt(dx*dx+dy*dy);if(length<1)return;
    dx/=length;dy/=length;
    const float radius=std::min((size.x*.5f-36)/std::max(.001f,std::fabs(dx)),
        (size.y*.5f-36)/std::max(.001f,std::fabs(dy)));
    const ImVec2 tip{center.x+dx*radius,center.y+dy*radius};
    draw->AddTriangleFilled(tip,{tip.x-dx*18-dy*8,tip.y-dy*18+dx*8},
        {tip.x-dx*18+dy*8,tip.y-dy*18-dx*8},esp_shadow);
    draw->AddTriangleFilled({tip.x-dx*2,tip.y-dy*2},
        {tip.x-dx*15-dy*5,tip.y-dy*15+dx*5},{tip.x-dx*15+dy*5,tip.y-dy*15-dx*5},color);
}
// Copy only valid UTF-16 strings. A failed/mismatched field never becomes a
// fabricated player name; the caller simply leaves that label absent.
inline bool utf16_name(const uint16_t* input,std::size_t length,char* output,std::size_t capacity) {
    if(!input||!output||capacity==0||length==0||length>64)return false;
    std::size_t pos=0;
    for(std::size_t i=0;i<length;++i){
        uint32_t point=input[i];
        if(point>=0xd800&&point<=0xdbff){
            if(i+1>=length||input[i+1]<0xdc00||input[i+1]>0xdfff){output[0]=0;return false;}
            point=0x10000+((point-0xd800)<<10)+(input[++i]-0xdc00);
        }else if(point>=0xdc00&&point<=0xdfff){output[0]=0;return false;}
        if(point<32||point==127){output[0]=0;return false;}
        unsigned bytes=point<0x80?1:point<0x800?2:point<0x10000?3:4;
        if(pos+bytes>=capacity){output[0]=0;return false;}
        if(bytes==1)output[pos++]=char(point);
        else {
            output[pos++]=char((bytes==2?0xc0:bytes==3?0xe0:0xf0)|(point>>(6*(bytes-1))));
            for(unsigned j=bytes-1;j>0;--j)output[pos++]=char(0x80|((point>>(6*(j-1)))&63));
        }
    }
    output[pos]=0;return true;
}
}
