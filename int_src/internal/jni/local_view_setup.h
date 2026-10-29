#pragma once
#include "local_player_view.h"

namespace mango_camera {
struct ViewSetupApi {
    bool(*read)(uintptr_t,void*,size_t)=nullptr;
    bool(*is_type)(void*,const char*)=nullptr;
    void(*create)(void*)=nullptr;
    void(*bind_animation)(void*,void*)=nullptr;
    void(*bind_equipment)(void*,void*)=nullptr;
    bool(*active)(void*)=nullptr;void(*set_active)(void*,bool)=nullptr;
    template<class T>T field(void* p,size_t offset)const{T v{};if(p&&read)read(reinterpret_cast<uintptr_t>(p)+offset,&v,sizeof(v));return v;}
    void* ptr(void* p,size_t offset)const{return field<void*>(p,offset);}
    bool type(void* p,const char* name)const{return p&&is_type&&is_type(p,name);}
    bool unity(void* p,const char* name)const{return type(p,name)&&ptr(p,0x10);}
    void* kcc(void* player)const{
        void* p=ptr(ptr(player,0xB0),0x18);
        return unity(p,"KCC")||unity(p,"SingleKcc")?p:nullptr;
    }
    size_t driver_offset(void* p)const{return type(p,"KCC")?0xE8:0xC8;}
    void* input(void* p)const{return ptr(p,type(p,"KCC")?0x158:0xC0);}
    bool ready(void* player)const{
        if(!unity(player,"PlayerManager")||!create||!bind_animation||!bind_equipment||!active||!set_active)return false;
        if(!unity(ptr(player,0x150),"GameObject"))return false;
        void* factory=ptr(player,0x340);if(!type(factory,"YX")||!ptr(factory,0x28)||!ptr(factory,0x30))return false;
        if(!unity(ptr(ptr(factory,0x20),0x18),"PlayerModelInfo"))return false;
        void* fp=ptr(player,0x90);if(!unity(fp,"FPManager")||!unity(ptr(fp,0x28),"FirstPersonRenderController"))return false;
        void* events=ptr(player,0x78);if(!type(events,"kAu")||!type(ptr(events,0xC8),"kAP"))return false;
        void* equipment=ptr(player,0xE8);if(!unity(equipment,"kJn")||!ptr(equipment,0xA8))return false;
        void* motor=kcc(player);if(!motor||!type(input(motor),"UN"))return false;
        return ptr(motor,type(motor,"KCC")?0x78:0x80)==player;
    }
};
// The hidden network body is not ticked for an owning client. The game normally
// calls Hjr only behind a feature flag. Create its real local view once, using
// that same path: KCC.vpg connects animation and kJn.Hgv subscribes the outfit.
// No copied poses, hand-written movement states or per-frame outfit reloads.
class LocalViewSetup {
    void* player_=nullptr;void* player_native_=nullptr;
    void* created_lease_=nullptr;void* created_root_native_=nullptr;
    bool attempted_=false;double next_=0;unsigned repairs_=0;
    void* repair_model_=nullptr;
    void hide_owned(ViewSetupApi& api,void* player){
        if(!created_root_native_||api.ptr(player,0x268)!=created_lease_)return;
        void* root=api.ptr(player,0x270);
        if(api.unity(root,"GameObject")&&api.ptr(root,0x10)==created_root_native_&&api.active&&api.set_active&&api.active(created_root_native_))api.set_active(created_root_native_,false);
    }
public:
    int status=0;
    void update(ViewSetupApi& api,void* player,bool enabled,double time){
        void* native=api.type(player,"PlayerManager")?api.ptr(player,0x10):nullptr;
        if(player_!=player||player_native_!=native){player_=player;player_native_=native;created_lease_=created_root_native_=repair_model_=nullptr;attempted_=false;next_=0;repairs_=0;}
        if(!enabled||!native){hide_owned(api,player);status=0;return;}
        if(time<next_)return;next_=time+.5;
        auto view=player_view(player,api.read,api.is_type);
        if(!view.local){
            if(attempted_)return;
            status=1;
            // A partial/stale lease or root belongs to the game. Do not create
            // a second body or register another Respawn callback over it.
            if(api.ptr(player,0x268)||api.ptr(player,0x270)||!api.ready(player))return;
            attempted_=true;status=2;
            try{api.create(player);}catch(...){status=-1;}
            created_lease_=api.ptr(player,0x268);
            void* root=api.ptr(player,0x270);
            if(api.unity(root,"GameObject"))created_root_native_=api.ptr(root,0x10);
            // Let the camera controller own activation. Its ordinary disable /
            // wall / session restore now returns a newly created body to hidden.
            hide_owned(api,player);
            view=player_view(player,api.read,api.is_type);
            if(!view.local){if(status!=-1)status=-2;return;}
        }
        if(repair_model_!=view.info){repair_model_=view.info;repairs_=0;}
        if(!api.ready(player)){status=1;return;}
        void* motor=api.kcc(player);void* equipment=api.ptr(player,0xE8);
        const bool animate=api.ptr(motor,api.driver_offset(motor))!=view.animation||!api.field<uint8_t>(view.animation,0x68)||api.ptr(view.animation,0x78)!=player;
        const bool equip=api.type(view.equipment,"YH")&&api.ptr(view.equipment,0x20)==view.info&&api.ptr(equipment,0xA0)!=view.equipment;
        if((animate||equip)&&repairs_<3){
            ++repairs_;next_=time+1;
            try{if(animate)api.bind_animation(motor,view.animation);if(equip)api.bind_equipment(equipment,view.equipment);}catch(...){status=-3;return;}
        }
        status=api.type(view.equipment,"YH")&&api.ptr(view.equipment,0x20)==view.info&&api.ptr(motor,api.driver_offset(motor))==view.animation&&api.field<uint8_t>(view.animation,0x68)&&api.ptr(equipment,0xA0)==view.equipment?3:1;
    }
};
}
