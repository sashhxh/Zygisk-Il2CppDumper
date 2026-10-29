#include "automation_input.h"
#include <cassert>
#include <cstdio>
struct Game {
    bool held=false,ammo=true,semi=false;int presses=0,releases=0,shots=0;double at=0,next=0;
    void fire(){if(ammo&&at>=next){++shots;next=at+.1;}}
    void press(bool value){if(value){++presses;fire();}else ++releases;held=value;}
    void poll(){if(held&&!semi)fire();}
};
int main(){
    mango_input::Controller input;Game game;
    auto frame=[&](double time,bool desired,bool physical,bool continuous=true,uintptr_t owner=1){
        game.at=time;input.update(owner,desired,physical,continuous,time,[&](bool value){game.press(value);});game.poll();
    };
    // A visible target produces the same press and held-input state as the UI.
    for(int i=0;i<30;++i)frame(1+i*.016,true,false);
    assert(game.presses==1&&game.shots>=4&&input.owned()&&game.held);
    const int shots=game.shots;frame(1.6,false,false);assert(!game.held&&!input.owned()&&game.releases==1);
    frame(2,false,false);assert(game.shots==shots);
    // Closing a target window, opening the menu, and switching weapons release.
    frame(2.1,true,false);frame(2.12,false,false);assert(!game.held);
    frame(2.2,true,false);frame(2.21,true,false,true,2);assert(game.releases==3&&game.held);
    frame(2.22,false,false,true,0);assert(!game.held);
    // Physical input stays in charge, including when automation is paused.
    game.held=true;int releases=game.releases;frame(3,true,true,true,2);frame(3.02,false,true,true,2);
    assert(game.held&&!input.owned()&&game.releases==releases);
    game.held=false;frame(3.1,false,false,true,2);
    // Native ammo/cooldown checks remain effective.
    game.ammo=false;int before=game.shots;for(int i=0;i<20;++i)frame(4+i*.016,true,false,true,2);assert(game.shots==before);
    game.ammo=true;frame(4.5,true,false,true,2);assert(game.shots==before+1);
    frame(4.6,false,false,true,2);game.semi=true;before=game.shots;int presses=game.presses;
    for(int i=0;i<120;++i)frame(5+i/120.0,true,false,false,3);
    assert(game.presses-presses<=21&&game.shots-before>=8&&game.shots-before<=11);
    frame(6.1,false,false,false,3);assert(!game.held);
    // A tool uses held input as well; it does not need a firearm fire-mode flag.
    game.semi=false;before=game.shots;for(int i=0;i<30;++i)frame(7+i*.016,true,false,true,4);
    assert(game.shots>before&&game.held);frame(7.6,false,false,true,4);assert(!game.held);
    std::puts("automation_input: PASS (native press/hold, visible target, tool, release, menu, owner switch, manual input, ammo/cooldown, semi-auto)");
}
