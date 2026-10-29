#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
namespace xvcen_assets {
// Original 0xc5160 initializes four bilinear weights from the grab point.
// 0xc4ec0 integrates at 8 substeps, k=120+80*weight, damping=8.
// 0xc3da0 constrains the grabbed point, then restores the rectangle in 0.15s.
struct JellyMotion {
    ImVec2 corner[4]{},velocity[4]{},released[4]{};
    float weight[4]{};bool active=false,returning=false;float phase=0;
    static ImVec2 target(ImVec2 p,ImVec2 size,int i){return {p.x+(i&1?size.x:0),p.y+(i&2?size.y:0)};}
    void begin(ImVec2 p,ImVec2 size,ImVec2 mouse){
        const float u=std::clamp((mouse.x-p.x)/size.x,0.f,1.f),v=std::clamp((mouse.y-p.y)/size.y,0.f,1.f);
        weight[0]=(1-v)*(1-u);weight[1]=(1-v)*u;weight[2]=v*(1-u);weight[3]=v*u;
        for(int i=0;i<4;++i){corner[i]=target(p,size,i);velocity[i]={};}
        active=true;returning=false;phase=0;
    }
    void integrate(ImVec2 p,ImVec2 size,float dt){
        const float step=dt/8;
        for(int sub=0;sub<8;++sub)for(int i=0;i<4;++i){const auto t=target(p,size,i);const float k=std::fma(weight[i],80.f,120.f);
            velocity[i].x=std::fma(std::fma(-velocity[i].x,8.f,(t.x-corner[i].x)*k),step,velocity[i].x);
            velocity[i].y=std::fma(std::fma(-velocity[i].y,8.f,(t.y-corner[i].y)*k),step,velocity[i].y);
            corner[i].x=std::fma(velocity[i].x,step,corner[i].x);corner[i].y=std::fma(velocity[i].y,step,corner[i].y);
        }
    }
    void update(ImVec2 p,ImVec2 size,float dt,bool held){
        if(active&&held){
            integrate(p,size,dt);float square=0;ImVec2 error{};
            for(int i=0;i<4;++i){const auto t=target(p,size,i);square+=weight[i]*weight[i];error.x+=weight[i]*(t.x-corner[i].x);error.y+=weight[i]*(t.y-corner[i].y);}
            if(square>0)for(int i=0;i<4;++i){corner[i].x+=error.x*weight[i]/square;corner[i].y+=error.y*weight[i]/square;}
        }else if(active){active=false;returning=true;phase=0;for(int i=0;i<4;++i)released[i]=corner[i];}
        if(returning){
            phase+=dt/.15f;
            if(phase>=1){returning=false;for(int i=0;i<4;++i)corner[i]=target(p,size,i);return;}
            const float ease=1-(1-phase)*(1-phase);
            for(int i=0;i<4;++i){const auto t=target(p,size,i);corner[i]={released[i].x+(t.x-released[i].x)*ease,released[i].y+(t.y-released[i].y)*ease};}
        }
    }
    bool visible()const{return active||returning;}
    // Original 0xc5920: repeated cubic controls along each edge, 20x20 grid.
    ImVec2 point(float u,float v)const{
        const float su=u*u*(3-2*u),sv=v*v*(3-2*v);
        const float left=corner[0].x+(corner[2].x-corner[0].x)*sv,right=corner[1].x+(corner[3].x-corner[1].x)*sv;
        const float top=corner[0].y+(corner[1].y-corner[0].y)*su,bottom=corner[2].y+(corner[3].y-corner[2].y)*su;
        return {left+(right-left)*u,top+(bottom-top)*v};
    }
};
}
