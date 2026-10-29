#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace mango_reference {
inline float unit(float value){return std::clamp(value,0.f,1.f);}
inline float smoothstep(float value){value=unit(value);return value*value*(3-2*value);}
struct Spring {
    float value=0,velocity=0;
    void step(float target,float dt,float stiffness,float damping) {
        dt=std::clamp(dt,0.f,.032f);
        velocity+=dt*((target-value)*stiffness-velocity*damping);
        value+=dt*velocity;
        if(std::fabs(value-target)<.001f&&std::fabs(velocity)<.01f){value=target;velocity=0;}
    }
    void critical(float target,float dt,float frequency=11) {
        dt=std::clamp(dt,0.f,.05f);
        const float distance=value-target,accumulated=velocity+frequency*distance;
        const float attenuation=std::exp(-frequency*dt);
        value=target+(distance+dt*accumulated)*attenuation;
        velocity=(velocity-dt*frequency*accumulated)*attenuation;
        if(std::fabs(value-target)<.001f&&std::fabs(velocity)<.01f){value=target;velocity=0;}
    }
};
struct RollingCounter {
    int candidate=0,accepted=0;
    float stable=0;Spring number;
    void update(int input,float dt) {
        input=std::clamp(input,0,999);dt=std::clamp(dt,0.f,.05f);
        if(input==candidate){stable+=dt;if(stable>=.05f)accepted=input;}
        else{candidate=input;stable=0;}
        number.critical(float(accepted),dt);
        number.value=std::clamp(number.value,0.f,999.f);
    }
};
struct MenuMotion {
    float phase=0;
    void update(bool open,float dt){phase=unit(phase+(open?1:-1)*std::clamp(dt,0.f,.1f)/.4f);}
    float position() const{return smoothstep(phase);}
    bool settled(bool open) const{return open?phase>=1:phase<=0;}
};
// The donor's bottom-anchor transition has two stages: the lower edge reaches
// the anchor during the first 30%, then the upper edge follows after 25%.
// x follows a cubic Bezier with repeated end controls; y remains linear.
inline ImVec2 menu_warp(ImVec2 p,ImVec2 origin,ImVec2 size,ImVec2 anchor_min,
                       ImVec2 anchor_max,float visible) {
    const float closing=1-unit(visible),reach=unit(closing/.3f);
    const float distance=std::max(1.f,anchor_min.y-origin.y);
    const float original_end=size.y/distance;
    const float start=unit((closing-.25f)/.75f);
    const float end=original_end+(1-original_end)*reach;
    const float y=start+(end-start)*(p.y-origin.y)/size.y;
    const float curve=y*y*(3-2*y);
    const float left=origin.x+(anchor_min.x-origin.x)*reach*curve;
    const float right=origin.x+size.x+(anchor_max.x-origin.x-size.x)*reach*curve;
    return {left+(right-left)*(p.x-origin.x)/size.x,origin.y+distance*y};
}
}
