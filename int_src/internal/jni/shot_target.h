#pragma once
#include "extra_features.h"

namespace mango_aim {
using V3=mango_extra::V3;
struct Target {mango_extra::Ref player,animator;bool humanoid=false;};
struct Api {
    mango_extra::Memory memory;
    uintptr_t(*bone)(uintptr_t,int)=nullptr;
    bool(*position)(uintptr_t,V3&)=nullptr;
    bool(*root)(uintptr_t,V3&)=nullptr;
    bool(*alive)(uintptr_t)=nullptr;
};
// Resolve the current animated bone for every request. Never substitute an ESP
// height estimate for a missing head or retain the last shot's world position.
inline bool live_point(const Api& api,const Target& target,int mode,V3& out){
    auto valid=[&](){return target.humanoid&&target.player.valid(api.memory)&&target.animator.valid(api.memory)&&
        api.memory.field<uintptr_t>(target.player.object,0x1B0)==target.animator.object;};
    if(!api.bone||!api.position||!api.root||!api.alive||!valid()||!api.alive(target.player.object))return false;
    V3 root{},point{};
    if(!api.root(target.player.object,root)||!mango_extra::finite(root))return false;
    uintptr_t transform=api.bone(target.animator.object,mode==1?8:10);
    if(!transform||!api.memory.field<uintptr_t>(transform,0x10)||!api.position(transform,point)||!mango_extra::finite(point))return false;
    const float x=point.x-root.x,y=point.y-root.y,z=point.z-root.z;
    if(x*x+y*y+z*z>16||!valid())return false;
    out=point;return true;
}
// Only the local shot's dispersion instance is affected, for the duration of
// the original firing call. Nested calls restore the enclosing scope.
class DispersionScope {
    uintptr_t& slot_;uintptr_t previous_;
public:
    DispersionScope(uintptr_t& slot,uintptr_t owner):slot_(slot),previous_(slot){slot_=owner;}
    ~DispersionScope(){slot_=previous_;}
    DispersionScope(const DispersionScope&)=delete;
    DispersionScope& operator=(const DispersionScope&)=delete;
};
inline bool suppress_spread(uintptr_t scope,uintptr_t instance){return scope&&scope==instance;}
}
