#include "local_weapon_view.h"
#include "weapon_grip.h"
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct Object{std::array<unsigned char,1024> data{};std::string type;};
std::vector<std::unique_ptr<Object>> objects;
void* make(const char* name,bool unity=false){auto p=std::make_unique<Object>();p->type=name;void* v=p->data.data();objects.push_back(std::move(p));if(unity)std::memcpy(static_cast<char*>(v)+0x10,&v,sizeof(v));return v;}
template<class T>void put(void* p,size_t at,T value){std::memcpy(static_cast<char*>(p)+at,&value,sizeof(value));}
bool read(uintptr_t at,void* out,size_t n){for(auto& o:objects){auto start=reinterpret_cast<uintptr_t>(o->data.data());if(at>=start&&at-start<=o->data.size()&&n<=o->data.size()-(at-start)){std::memcpy(out,reinterpret_cast<void*>(at),n);return true;}}return false;}
bool type(void* p,const char* name){for(auto& o:objects)if(o->data.data()==p)return o->type==name;return false;}
int loads=0,states=0;bool fail=false;
mango_camera::WeaponViewApi api;
mango_camera::WeaponPiece last{};
void load(void* v,const mango_camera::WeaponPiece* p){++loads;last=*p;if(fail)throw std::runtime_error("load");put(v,0x58,*p);put<void*>(v,0x50,nullptr);}
void state(void*,int){++states;if(fail)throw std::runtime_error("state");}
struct Fixture{
    void* player=make("PlayerManager",true),*lease=make("Yf"),*view=make("YU"),*weapon=make("PlayerWeapon",true),*animator=make("Animator",true);
    mango_camera::PlayerView body{};mango_camera::WeaponPiece piece{};
    Fixture(){void* ref=make("InterfaceReference");put(player,0xF0,ref);put(ref,0x18,weapon);put(weapon,0x160,player);put(lease,0x20,view);put(view,0x18,animator);put(view,0x10,make("PlayerModelInfo",true));body.local=true;body.lease=lease;body.info=api.ptr(view,0x10);body.animator=animator;piece.enabled=1;piece.number=3;piece.skin=5;piece.loaded=1;piece.mods=make("Mods");apply();}
    void apply(){put(weapon,0x130,piece);}
    void* complete(){void* primary=make("WeaponBase",true);put(view,0x50,primary);return primary;}
};
}
int main(){
    api.read=read;api.is_type=type;api.load=load;api.state=state;
    Fixture f;mango_camera::LocalWeaponView mirror;
    mirror.update(api,f.player,f.body,false,0);assert(loads==0&&mirror.status==0);
    mirror.update(api,f.player,f.body,true,1);assert(loads==1&&mirror.status==2&&last.number==3&&last.skin==5&&last.mods==f.piece.mods&&last.loaded==1);
    for(int i=0;i<300;++i)mirror.update(api,f.player,f.body,true,1.01+i/60.);
    assert(loads==1&&states==0); // pending asynchronous load is not restarted
    void* primary=f.complete();mirror.update(api,f.player,f.body,true,7);
    assert(loads==1&&states==1&&mirror.status==3&&mirror.visual==reinterpret_cast<uintptr_t>(primary));
    for(int i=0;i<600;++i)mirror.update(api,f.player,f.body,true,7.01+i/60.);
    assert(loads==1&&states==1); // no repeated parenting or loading
    put(f.weapon,0x14C,1);put(f.weapon,0x148,2);mirror.update(api,f.player,f.body,true,18);
    assert(states==2);
    // Loaded changes do not rebuild the same model on every magazine change.
    f.piece.loaded=0;f.apply();mirror.update(api,f.player,f.body,true,19);assert(loads==1);
    f.piece.number=7;f.apply();mirror.update(api,f.player,f.body,true,20);assert(loads==2&&mirror.status==2);
    f.complete();mirror.update(api,f.player,f.body,true,21);assert(states==3&&mirror.status==3);
    // Turning the camera off/on reuses the loaded view instead of reloading.
    mirror.update(api,f.player,f.body,false,22);mirror.update(api,f.player,f.body,true,23);assert(loads==2&&mirror.status==3);
    f.piece.enabled=0;f.apply();mirror.update(api,f.player,f.body,true,24);assert(loads==3);
    mirror.update(api,f.player,f.body,true,25);assert(mirror.status==3&&mirror.visual==0);
    // A late game-owned primary is reparented once, without reloading assets.
    f.piece.enabled=1;f.apply();mirror.update(api,f.player,f.body,true,26);assert(loads==4);
    f.complete();mirror.update(api,f.player,f.body,true,27);int before=states;
    f.complete();mirror.update(api,f.player,f.body,true,28);assert(states==before+1&&loads==4);
    // Skip the mirror if the native weapon system already uses this YU.
    put(f.weapon,0x100,f.view);f.piece.number=8;f.apply();mirror.update(api,f.player,f.body,true,29);assert(loads==4);
    put<void*>(f.weapon,0x100,nullptr);
    // Destroyed wrappers, remote fallbacks and mismatched owners are ignored.
    put<void*>(f.weapon,0x10,nullptr);mirror.update(api,f.player,f.body,true,30);assert(loads==4&&mirror.status==1);
    put(f.weapon,0x10,f.weapon);put<void*>(f.weapon,0x160,nullptr);mirror.update(api,f.player,f.body,true,31);assert(loads==4);
    put(f.weapon,0x160,f.player);f.body.local=false;mirror.update(api,f.player,f.body,true,32);assert(loads==4);f.body.local=true;
    // A new session/view binds its own weapon; no cached old view is invoked.
    Fixture second;mirror.update(api,second.player,second.body,true,33);assert(loads==5);
    for(int i=0;i<900;++i)mirror.update(api,second.player,second.body,true,33.01+i/60.);
    assert(loads==6); // one bounded retry after 10 seconds
    mirror.update(api,second.player,second.body,true,54);assert(loads==6&&mirror.status==-2);
    // Completion after timeout is still accepted without another asset load.
    second.complete();mirror.update(api,second.player,second.body,true,55);assert(mirror.status==3&&loads==6);
    // Cosmetic method failures also stop after three attempts.
    fail=true;put(second.weapon,0x14C,2);before=states;
    for(int i=0;i<30;++i)mirror.update(api,second.player,second.body,true,56+i*.1);
    assert(states==before+3&&mirror.status==-3);fail=false;
    second.piece.enabled=2;second.apply();mirror.update(api,second.player,second.body,true,60);assert(loads==6&&mirror.status==1);
    mirror.update(api,nullptr,{},false,61);assert(mirror.status==0&&mirror.visual==0);
    // Grip eligibility is independent of movement state: idle and jumping use
    // the same authored target, while reload/draw/holster retain free hands.
    second.piece.enabled=1;second.apply();
    void* held=second.complete(),*config=make("WeaponAnimationConfig",true),*pivot=make("Transform",true);
    put(held,0x68,config);put(held,0x98,pivot);put<uint8_t>(held,0x88,1);put<uint8_t>(config,0x28,1);
    for(int state:{0,2,5}){put(second.weapon,0x14C,state);assert(mango_camera::left_hand_grip(api,second.player,second.body)==pivot);}
    for(int state:{1,3,4,6}){put(second.weapon,0x14C,state);assert(!mango_camera::left_hand_grip(api,second.player,second.body));}
    put(second.weapon,0x14C,0);put<uint8_t>(config,0x28,0);assert(!mango_camera::left_hand_grip(api,second.player,second.body));
    put<uint8_t>(config,0x28,1);put<void*>(pivot,0x10,nullptr);assert(!mango_camera::left_hand_grip(api,second.player,second.body));
}
