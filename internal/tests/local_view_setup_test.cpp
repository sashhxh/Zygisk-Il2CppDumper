#include "local_view_setup.h"
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct Object {std::array<unsigned char,0x480> data{};std::string type;bool active=true;};
std::vector<std::unique_ptr<Object>> objects;
Object& obj(void* p){for(auto& o:objects)if(o->data.data()==p)return *o;throw std::runtime_error("invalid object");}
void* make(const char* type,bool unity=false){auto o=std::make_unique<Object>();o->type=type;void* p=o->data.data();objects.push_back(std::move(o));if(unity)std::memcpy(static_cast<char*>(p)+0x10,&p,sizeof(p));return p;}
void ptr(void* p,size_t offset,void* value){std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value));}
void byte(void* p,size_t offset,uint8_t value){std::memcpy(static_cast<char*>(p)+offset,&value,1);}
bool read(uintptr_t at,void* out,size_t n){for(auto& o:objects){auto first=reinterpret_cast<uintptr_t>(o->data.data());if(at>=first&&at-first<=o->data.size()&&n<=o->data.size()-(at-first)){std::memcpy(out,reinterpret_cast<void*>(at),n);return true;}}return false;}
bool is_type(void* p,const char* type){if(!p)return false;return obj(p).type==type;}
mango_camera::ViewSetupApi api;
int creates=0,animation_binds=0,equipment_binds=0;bool partial=false,fail_bind=false;
void* make_local(void* player){
    void* info=make("PlayerModelInfo",true),*animation=make("CharacterAnimation",true),*animator=make("Animator",true),*lease=make("Yf"),*equipment=make("YH");
    ptr(lease,0x10,info);ptr(lease,0x18,equipment);ptr(equipment,0x20,info);ptr(info,0x60,animation);ptr(animation,0x20,animator);ptr(player,0x268,lease);
    return lease;
}
void create(void* player){
    ++creates;void* root=make("GameObject",true);ptr(player,0x270,root);
    if(partial)throw std::runtime_error("partial factory failure");
    void* lease=make_local(player),*info=api.ptr(lease,0x10),*animation=api.ptr(info,0x60),*motor=api.kcc(player);
    ptr(motor,api.driver_offset(motor),animation);byte(animation,0x68,1);ptr(animation,0x78,player);
    ptr(api.ptr(player,0xE8),0xA0,api.ptr(lease,0x18));
}
void bind_animation(void* motor,void* animation){
    ++animation_binds;if(fail_bind)return;
    ptr(motor,api.driver_offset(motor),animation);byte(animation,0x68,1);ptr(animation,0x78,api.ptr(motor,is_type(motor,"KCC")?0x78:0x80));
}
void bind_equipment(void* equipment,void* view){++equipment_binds;ptr(equipment,0xA0,view);}
void* fixture(bool single=false){
    void* player=make("PlayerManager",true),*factory=make("YX"),*holder=make("PlayerViewReferenceHolder"),*fp=make("FPManager",true),*events=make("kAu"),*equipment=make("kJn",true);
    ptr(player,0x150,make("GameObject",true));ptr(player,0x340,factory);ptr(factory,0x20,holder);ptr(factory,0x28,make("Yg"));ptr(factory,0x30,make("YJ"));ptr(holder,0x18,make("PlayerModelInfo",true));
    ptr(player,0x90,fp);ptr(fp,0x28,make("FirstPersonRenderController",true));ptr(player,0x78,events);ptr(events,0xC8,make("kAP"));ptr(player,0xE8,equipment);ptr(equipment,0xA8,make("SyncList"));
    void* reference=make("InterfaceReference"),*motor=make(single?"SingleKcc":"KCC",true);ptr(player,0xB0,reference);ptr(reference,0x18,motor);ptr(motor,single?0x80:0x78,player);ptr(motor,single?0xC0:0x158,make("UN"));
    return player;
}
}
int main(){
    api.read=read;api.is_type=is_type;api.create=create;api.bind_animation=bind_animation;api.bind_equipment=bind_equipment;
    api.active=[](void* native){return obj(native).active;};api.set_active=[](void* native,bool value){obj(native).active=value;};
    mango_camera::LocalViewSetup setup;void* player=fixture();assert(api.ready(player));
    setup.update(api,player,false,0);assert(creates==0);
    void* motor=api.kcc(player),*input=api.input(motor);ptr(motor,0x158,nullptr);
    setup.update(api,player,true,1);assert(creates==0&&setup.status==1);
    ptr(motor,0x158,input);setup.update(api,player,true,2);assert(creates==1&&setup.status==3);
    void* root=api.ptr(player,0x270);assert(!obj(root).active);
    obj(root).active=true; // activation belongs to the third-person controller
    for(int i=0;i<120;++i)setup.update(api,player,true,2.01+i/60.);
    assert(creates==1&&animation_binds==0&&equipment_binds==0&&obj(root).active);
    setup.update(api,player,false,5);assert(!obj(root).active&&setup.status==0);
    obj(root).active=true; // the game can reactivate it during Respawn
    setup.update(api,player,false,5.1);assert(!obj(root).active);
    setup.update(api,player,true,6);assert(creates==1&&setup.status==3);
    // Late movement/equipment initialization is repaired once, then the game
    // owns all input ticks and outfit-change callbacks without repeated calls.
    auto view=mango_camera::player_view(player,read,is_type);
    ptr(motor,0xE8,nullptr);byte(view.animation,0x68,0);ptr(api.ptr(player,0xE8),0xA0,nullptr);
    setup.update(api,player,true,7);assert(animation_binds==1&&equipment_binds==1&&setup.status==3);
    setup.update(api,player,true,9);assert(animation_binds==1&&equipment_binds==1);
    // A game-owned replacement is repaired without another factory call, and
    // it is not hidden as if it were the old instance created by us.
    make_local(player);void* game_root=make("GameObject",true);ptr(player,0x270,game_root);
    setup.update(api,player,true,10);assert(creates==1&&animation_binds==2&&equipment_binds==2&&setup.status==3);
    setup.update(api,player,false,11);assert(obj(game_root).active);
    // A new player after reconnect gets exactly one new view and binding.
    void* second=fixture(true);setup.update(api,second,true,12);assert(creates==2&&setup.status==3);
    auto second_view=mango_camera::player_view(second,read,is_type);ptr(api.kcc(second),0xC8,nullptr);byte(second_view.animation,0x68,0);
    setup.update(api,second,true,13);assert(animation_binds==3&&setup.status==3);
    // Partial creation must not repeatedly allocate roots or add callbacks.
    partial=true;void* broken=fixture();setup.update(api,broken,true,14);assert(creates==3&&setup.status==-1&&!obj(api.ptr(broken,0x270)).active);
    for(int i=0;i<20;++i)setup.update(api,broken,true,15+i);assert(creates==3&&setup.status==-1);
    partial=false;
    // Leave a pre-existing partial body alone rather than duplicating it.
    void* pending=fixture();ptr(pending,0x270,make("GameObject",true));setup.update(api,pending,true,40);assert(creates==3);
    // Repairs that fail to initialize are bounded, not attempted every frame.
    void* unready=fixture();make_local(unready);fail_bind=true;
    for(int i=0;i<20;++i)setup.update(api,unready,true,41+i);
    assert(creates==3&&animation_binds==6&&setup.status==1);
    setup.update(api,nullptr,false,99);assert(setup.status==0);
}
