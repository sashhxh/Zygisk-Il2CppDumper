#include "proximity_farm.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
struct Point {float x=0,y=0,z=0;};
struct Spot {bool active=false;Point point{},forward{1,0,0},up{0,1,0};};
struct Hit {Point point;};
float distance(Point a,Point b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
int main(){
    mango_farm::ClientHealth health;
    assert(health.available(0,false,true)); // Untouched remote resource: no manual hit.
    assert(health.available(0,false,true));
    assert(!health.available(0,false,false));
    assert(health.available(80,false,true));
    assert(!health.available(0,false,true)); // Observed death is not an untouched resource.
    assert(health.available(0,true,true)); // Chunk ores have unsynchronized health.
    assert(!health.available(0,true,false));
    assert(!health.available(std::numeric_limits<float>::quiet_NaN(),false,true));
    Point origin{2,1.35f,0},resource{};Spot spot;Hit hit{};int calls=0;
    auto cast=[&](Point from,Point target,float,Hit& out){++calls;out.point=target;return 1;};
    assert(mango_farm::surface(origin,resource,spot,hit,cast,distance));
    assert(calls==1&&hit.point.y==.8f); // Automatic starter, no marker yet.
    spot.active=true;spot.point={.5f,1.7f,.2f};
    assert(mango_farm::surface(origin,resource,spot,hit,cast,distance));
    assert(distance(hit.point,spot.point)==0);
    // Both a moving tree cross and a replacement ore glow are re-read each hit.
    for(Point next:{Point{-.4f,1.4f,.3f},Point{.7f,.6f,-.2f}}){
        spot.point=next;assert(mango_farm::surface(origin,resource,spot,hit,cast,distance));
        assert(distance(hit.point,next)==0);
    }
    calls=0;
    auto blocked=[&](Point,Point,float,Hit&){++calls;return -1;};
    assert(!mango_farm::surface(origin,resource,spot,hit,blocked,distance));assert(calls==1);
    calls=0;
    auto wrong_point=[&](Point,Point target,float,Hit& out){
        ++calls;assert(distance(target,spot.point)==0);out.point={99,99,99};return 1;
    };
    assert(!mango_farm::surface(origin,resource,spot,hit,wrong_point,distance));
    assert(calls==5); // No fixed-height fallback when a live marker exists.
    calls=0;
    auto far_side=[&](Point from,Point target,float,Hit& out){
        ++calls;out.point=calls==1?resource:target;return 1;
    };
    assert(mango_farm::surface(origin,resource,spot,hit,far_side,distance));
    assert(calls==2&&distance(hit.point,spot.point)==0);
    std::puts("farm_weakspots: PASS (untouched resources, death, starter, moving marks, far-side surface, blockers, no fixed-point fallback)");
}
