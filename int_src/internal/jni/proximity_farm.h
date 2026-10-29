#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mango_farm {
// A remote client's health may stay at its prefab value until the first
// SyncVar update. Zero is only depletion after we observed positive health.
inline bool at_weakspot(float distance){return std::isfinite(distance)&&distance>=0&&distance<=.145f;}
class ClientHealth {
    bool synchronized_=false;
public:
    bool available(float health,bool grouped,bool active){
        if(!active)return false;
        if(grouped)return true;
        if(!std::isfinite(health)||health<0)return false;
        if(health>0)synchronized_=true;
        return !synchronized_||health>0;
    }
};
// The first harvest can seed a marker; every later hit must use the live marker.
// Cast returns -1 for an unrelated blocker, 0 for a miss and 1 for this resource.
template<class Point,class Weakspot,class Hit,class Cast,class Distance>
bool surface(Point origin,Point resource,const Weakspot& weakspot,Hit& hit,Cast cast,Distance distance){
    if(weakspot.active){
        const int direct=cast(origin,weakspot.point,.25f,hit);
        if(direct<0)return false;
        if(direct>0&&at_weakspot(distance(hit.point,weakspot.point)))return true;
        for(Point axis:{weakspot.forward,weakspot.up})for(float sign:{1.f,-1.f}){
            Point from{weakspot.point.x+axis.x*.25f*sign,weakspot.point.y+axis.y*.25f*sign,weakspot.point.z+axis.z*.25f*sign};
            if(cast(from,weakspot.point,.25f,hit)>0&&at_weakspot(distance(hit.point,weakspot.point)))return true;
        }
        return false;
    }
    for(float height:{.8f,.35f,1.2f})if(cast(origin,Point{resource.x,resource.y+height,resource.z},.25f,hit)>0)return true;
    return false;
}
// The network-side tool activation sent by an ordinary air swing is separate
// from the local swing. Re-prepare when either the FP object or equipped item changes.
class ToolSession {
    uintptr_t object_=0,native_=0,item_=0;double prepared_at_=0;bool prepared_=false;
public:
    void reset(){object_=native_=item_=0;prepared_at_=0;prepared_=false;}
    template<class Prepare>bool ready(uintptr_t object,uintptr_t native,uintptr_t item,double time,Prepare prepare){
        if(!object||!native||!item||!std::isfinite(time)){reset();return false;}
        if(object_!=object||native_!=native||item_!=item){reset();object_=object;native_=native;item_=item;}
        if(!prepared_){if(!prepare())return false;prepared_=true;prepared_at_=time;return false;}
        return time>=prepared_at_+.05;
    }
};
// Reserve the next request before invoking game code. Target/weapon changes,
// pauses and failed casts must not produce a burst of harvest requests.
class Cadence {
    double next_=0;
public:
    void reset(){next_=0;}
    template<class Send> bool tick(bool ready,double time,float period,Send send){
        if(!ready||!std::isfinite(time)||!std::isfinite(period)||period<=0||time<next_)return false;
        next_=time+std::max(.25f,period);
        return send();
    }
};
}
