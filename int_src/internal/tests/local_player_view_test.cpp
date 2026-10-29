#include "local_player_view.h"
#include <array>
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace {
struct Object {std::array<unsigned char,0x480> bytes{};std::string type;};
std::vector<std::unique_ptr<Object>> storage;
void* make(const char* type){auto o=std::make_unique<Object>();o->type=type;void* p=o->bytes.data();storage.push_back(std::move(o));return p;}
void ptr(void* object,size_t offset,void* value){std::memcpy(static_cast<char*>(object)+offset,&value,sizeof(value));}
bool read(uintptr_t at,void* out,size_t n){for(auto& o:storage){auto start=reinterpret_cast<uintptr_t>(o->bytes.data());if(at>=start&&at-start<=o->bytes.size()&&n<=o->bytes.size()-(at-start)){std::memcpy(out,reinterpret_cast<void*>(at),n);return true;}}return false;}
bool is_type(void* p,const char* name){for(auto& o:storage)if(o->bytes.data()==p)return o->type==name;return false;}
void* model(){void* info=make("PlayerModelInfo"),*animation=make("CharacterAnimation"),*animator=make("Animator");for(void* p:{info,animation,animator})ptr(p,0x10,p);ptr(info,0x60,animation);ptr(animation,0x20,animator);return info;}
void* lease(void* info){void* p=make("Yf");ptr(p,0x10,info);return p;}
}
int main(){
    using mango_camera::player_view;
    void* player=make("PlayerManager"),*world=model(),*local=model();
    ptr(player,0x260,lease(world));ptr(player,0x268,lease(local));
    auto view=player_view(player,read,is_type);assert(view.local&&view.info==local&&view.animator&&!view.render_controller);
    // A not-yet-created local view must fall back to the existing world body.
    ptr(player,0x268,nullptr);view=player_view(player,read,is_type);assert(!view.local&&view.info==world);
    ptr(player,0x268,lease(local));ptr(local,0x10,nullptr);
    view=player_view(player,read,is_type);assert(!view.local&&view.info==world);
    ptr(local,0x10,local);ptr(player,0x268,lease(make("PlayerFPModelInfo")));
    view=player_view(player,read,is_type);assert(view.info==world); // not the arm rig
    // Rebind immediately to a new live model after respawn, using its Animator.
    local=model();ptr(player,0x268,lease(local));
    void* fp=make("FPManager"),*pass=make("FirstPersonRenderController"),*klass=make("class"),*fields=make("statics");
    ptr(player,0x90,fp);ptr(fp,0x28,pass);ptr(pass,0x10,pass);ptr(pass,0,klass);ptr(klass,0xB8,fields);
    uint32_t masks[]={4,8};std::memcpy(static_cast<char*>(fields)+0xC,masks,sizeof(masks));
    view=player_view(player,read,is_type);assert(view.info==local&&view.local&&view.render_controller==pass&&view.reserved_layers==12);
    ptr(fp,0x28,make("Camera"));view=player_view(player,read,is_type);assert(!view.render_controller&&view.reserved_layers==0);
    assert(!player_view(nullptr,read,is_type).info);
}
