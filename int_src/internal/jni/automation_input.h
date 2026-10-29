#pragma once
#include <cmath>
#include <cstdint>
namespace mango_input {
// Feed the game's press/release handler, not just its attack event. The game
// also stores its held-input bit and checks it while running weapon actions.
class Controller {
    uintptr_t owner_=0;bool held_=false;double next_press_=0;
public:
    void reset(){owner_=0;held_=false;next_press_=0;}
    bool owned()const{return held_;}
    template<class Press>void update(uintptr_t owner,bool desired,bool physical,bool continuous,double time,Press press){
        if(owner_!=owner){if(held_&&!physical)press(false);reset();owner_=owner;}
        if(!owner||!std::isfinite(time))return;
        if(physical){held_=false;next_press_=time;return;}
        if(!desired){if(held_)press(false);held_=false;next_press_=time;return;}
        if(continuous){if(!held_)press(true);held_=true;return;}
        if(held_){press(false);held_=false;return;}
        if(time>=next_press_){press(true);held_=true;next_press_=time+.05;}
    }
};
}
