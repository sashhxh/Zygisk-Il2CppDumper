#include "proximity_farm.h"
#include <cassert>
#include <limits>
#include <cstdio>

int main(){
    mango_farm::Cadence cadence;int requests=0;
    auto send=[&](){++requests;return true;};
    assert(!cadence.tick(false,1,.5f,send)); // Menu, lost resource or wrong tool.
    assert(cadence.tick(true,1,.5f,send));
    for(int i=1;i<30;++i)assert(!cadence.tick(true,1+i/60.,.5f,send));
    assert(requests==1);
    assert(cadence.tick(true,1.5,.5f,send));
    // Changing targets or briefly leaving range must not bypass the interval.
    assert(!cadence.tick(false,1.6,.5f,send));
    assert(!cadence.tick(true,1.61,.5f,send));
    assert(!cadence.tick(true,1.62,.01f,send));
    assert(cadence.tick(true,2,.01f,send));
    assert(!cadence.tick(true,2.1,.01f,send));
    assert(cadence.tick(true,2.25,.01f,send));
    // A blocked path or missing binding must not cause retries every frame.
    int failures=0;auto blocked=[&](){++failures;return false;};
    assert(!cadence.tick(true,3,.5f,blocked));
    for(int i=1;i<30;++i)assert(!cadence.tick(true,3+i/60.,.5f,blocked));
    assert(failures==1);
    const float nan=std::numeric_limits<float>::quiet_NaN();
    assert(!cadence.tick(true,10,nan,send));
    assert(!cadence.tick(true,nan,.5f,send));
    assert(!cadence.tick(true,10,0,send));
    assert(!cadence.tick(true,10,-1,send));
    cadence.reset();assert(cadence.tick(true,1,1,send));
    const int before=requests;
    for(int i=0;i<60;++i)assert(!cadence.tick(false,10+i/60.,1,send));
    assert(requests==before);
    // No catch-up burst after a long pause.
    assert(cadence.tick(true,20,1,send));
    assert(!cadence.tick(true,20.001,1,send));
    mango_farm::ToolSession tool;int activations=0;
    auto activate=[&](){++activations;return true;};
    assert(!tool.ready(1,11,100,1,activate));assert(activations==1);
    assert(!tool.ready(1,11,100,1.01,activate));
    assert(tool.ready(1,11,100,1.06,activate));assert(activations==1);
    // The same FP model can be reused for a newly equipped inventory item.
    assert(!tool.ready(1,11,200,2,activate));assert(activations==2);
    assert(tool.ready(1,11,200,2.06,activate));
    assert(!tool.ready(1,11,100,3,activate));assert(activations==3);
    assert(!tool.ready(1,12,100,4,activate));assert(activations==4);
    tool.reset();assert(!tool.ready(1,12,100,5,[](){return false;}));
    assert(!tool.ready(1,12,100,5.1,activate));assert(activations==5);
    assert(tool.ready(1,12,100,5.2,activate));
    assert(!tool.ready(1,12,0,6,activate));
    assert(!tool.ready(1,12,100,7,activate));assert(activations==6);
    assert(!tool.ready(1,12,100,std::numeric_limits<double>::quiet_NaN(),activate));
    std::puts("proximity_farm: PASS (cadence, pauses, target switches, blockers, invalid timing, no bursts)");
}
