#pragma once
#include "extra_features.h"
namespace mango_prediction {
using V3=mango_extra::V3;
inline V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V3 mul(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float length(V3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
class Motion {
    V3 point_{},velocity_{};double at_=0,still_=0;bool ready_=false;
public:
    void update(V3 p,double at){
        if(!mango_extra::finite(p)||!std::isfinite(at))return;
        const double dt=at-at_;const V3 delta=sub(p,point_);
        if(!ready_||dt<=0||dt>.3||length(delta)>float(dt)*50){point_=p;velocity_={};at_=at;still_=0;ready_=true;return;}
        if(dt<.001)return;
        if(length(delta)<.0001f)still_+=dt;else still_=0;
        const V3 speed=mul(delta,float(1/dt));const float alpha=std::min(1.f,float(dt/.04));
        velocity_=still_>.06?V3{}:add(velocity_,mul(sub(speed,velocity_),alpha));point_=p;at_=at;
    }
    V3 velocity(double at)const{return ready_&&at>=at_&&at-at_<.25?velocity_:V3{};}
};
struct Ballistics {float speed=0,drag=0;V3 acceleration{},inherited{};};
struct Solution {V3 point{};float seconds=0;};
// These coefficients match the game's 1/32 s BallisticIntegrator: apply
// acceleration/drag, average old and new velocity, then advance position.
inline bool coefficients(float time,float drag,float& travel,float& fall){
    constexpr float dt=1.f/32;travel=fall=0;float v=1,g=0;
    const float q=std::clamp(1-drag*dt,0.f,1.f),decay=(1+q)*.5f;
    for(unsigned i=0;time>0&&i<321;++i){
        v*=decay;g=g*decay+q*dt*.5f;const float step=std::min(time,dt);
        travel+=v*step;fall+=g*step;time-=step;
    }
    return time<=.00001f&&travel>.000001f;
}
inline bool solve(V3 origin,V3 target,V3 velocity,const Ballistics& b,Solution& out){
    if(!mango_extra::finite(origin)||!mango_extra::finite(target)||!mango_extra::finite(velocity)||
       !mango_extra::finite(b.acceleration)||!mango_extra::finite(b.inherited)||
       !std::isfinite(b.speed)||b.speed<10||b.speed>5000||!std::isfinite(b.drag)||b.drag<0||b.drag>100||
       length(velocity)>50||length(b.inherited)>100||length(b.acceleration)>200)return false;
    const float meters=length(sub(target,origin));if(meters<.01f||meters>2000)return false;
    auto required=[&](float time,V3& initial){
        float travel,fall;if(!coefficients(time,b.drag,travel,fall))return false;
        initial=sub(mul(sub(sub(add(target,mul(velocity,time)),origin),mul(b.acceleration,fall)),1/travel),b.inherited);
        return mango_extra::finite(initial);
    };
    float lo=.00001f,hi=std::max(.001f,meters/b.speed*.5f);V3 initial{};bool bracket=false;
    for(unsigned i=0;i<64&&hi<=10;++i){
        if(!required(hi,initial))return false;
        if(length(initial)<=b.speed){bracket=true;break;}
        lo=hi;hi=std::min(10.f,hi*1.2f+.001f);if(lo==hi)break;
    }
    if(!bracket)return false;
    for(unsigned i=0;i<22;++i){float mid=(lo+hi)*.5f;if(!required(mid,initial))return false;if(length(initial)>b.speed)lo=mid;else hi=mid;}
    if(!required(hi,initial))return false;
    out.point=add(origin,mul(initial,hi));out.seconds=hi;return mango_extra::finite(out.point);
}
inline size_t attack_offset(int mode){return mode==2?0x1F0:0x1E8;}
class AutomaticAttack {
    uintptr_t owner_=0;bool pressed_=false;
public:
    void reset(){owner_=0;pressed_=false;}
    template<class Invoke>bool fire(uintptr_t owner,int mode,Invoke invoke){
        if(!owner)return false;
        if(owner!=owner_){owner_=owner;pressed_=false;}
        if(mode!=2){pressed_=false;return invoke(size_t(0x1E8));}
        const bool started=!pressed_&&invoke(size_t(0x1E8));pressed_=pressed_||started;
        const bool held=invoke(size_t(0x1F0));return started||held;
    }
};
inline bool hold(bool enabled,bool menu,double deadline,double time){return enabled&&!menu&&std::isfinite(time)&&time<deadline&&deadline-time<=.15;}
}
