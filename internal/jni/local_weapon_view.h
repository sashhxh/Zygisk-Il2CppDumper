#pragma once
#include "local_player_view.h"
#include <cmath>

namespace mango_camera {
// WeaponPiece is a 24-byte IL2CPP value type. ARM64 passes this size indirectly;
// Mods is a managed reference copied from the current PlayerWeapon snapshot.
struct WeaponPiece {
    uint8_t enabled=0,padding0=0;int16_t number=0,skin=0;
    uint8_t skin_level=0,loaded=0;void* mods=nullptr;
    uint8_t empty_hands=0,padding1[7]{};
    bool same_model(const WeaponPiece& b)const{
        return enabled==b.enabled&&number==b.number&&skin==b.skin&&skin_level==b.skin_level&&mods==b.mods&&empty_hands==b.empty_hands;
    }
};
static_assert(sizeof(WeaponPiece)==24&&offsetof(WeaponPiece,mods)==8&&offsetof(WeaponPiece,empty_hands)==16);
struct WeaponViewApi {
    bool(*read)(uintptr_t,void*,size_t)=nullptr;
    bool(*is_type)(void*,const char*)=nullptr;
    void(*load)(void*,const WeaponPiece*)=nullptr;
    void(*state)(void*,int)=nullptr;
    template<class T>T field(void* p,size_t offset)const{T v{};if(p&&read)read(reinterpret_cast<uintptr_t>(p)+offset,&v,sizeof(v));return v;}
    void* ptr(void* p,size_t offset)const{return field<void*>(p,offset);}
    bool type(void* p,const char* name)const{return p&&is_type&&is_type(p,name);}
    bool unity(void* p,const char* name)const{return type(p,name)&&ptr(p,0x10);}
};
// A cosmetic mirror of the held item, using the local view's own YU loader.
// Leave PlayerWeapon's network/FP references, callbacks and gameplay untouched.
// YU owns asynchronous loads, cancellation, skin assets and hand parenting.
class LocalWeaponView {
    void* player_=nullptr;void* player_native_=nullptr;void* lease_=nullptr;void* view_=nullptr;
    void* weapon_=nullptr;void* weapon_native_=nullptr;void* primary_=nullptr;
    WeaponPiece requested_{};bool requested_valid_=false;unsigned attempts_=0;
    int state_=-1;unsigned failures_=0;double next_load_=0,next_check_=0;
public:
    // 0 off, 1 waiting, 2 asynchronous load, 3 ready; negative = bounded failure.
    int status=0;uintptr_t visual=0;
    void reset(){*this=LocalWeaponView{};}
    void update(WeaponViewApi& api,void* player,const PlayerView& body,bool enabled,double time){
        if(!enabled||!std::isfinite(time)){reset();return;}
        if(time<next_check_)return;
        if(!api.read||!api.is_type||!api.load||!api.state){status=1;return;}
        void* native=api.unity(player,"PlayerManager")?api.ptr(player,0x10):nullptr;
        if(!native||!body.local||!body.info||!body.lease){reset();next_check_=time+.05;status=1;return;}
        void* view=api.ptr(body.lease,0x20);
        void* weapon=api.ptr(api.ptr(player,0xF0),0x18);
        if(!api.type(view,"YU")||api.ptr(view,0x18)!=body.animator||!api.ptr(view,0x10)||!api.unity(weapon,"PlayerWeapon")||api.ptr(weapon,0x160)!=player){reset();next_check_=time+.05;status=1;return;}
        if(player_!=player||player_native_!=native||lease_!=body.lease||view_!=view||weapon_!=weapon||weapon_native_!=api.ptr(weapon,0x10)){
            reset();player_=player;player_native_=native;lease_=body.lease;view_=view;weapon_=weapon;weapon_native_=api.ptr(weapon,0x10);
        }
        next_check_=time+.05;
        WeaponPiece piece{};
        if(!api.read(reinterpret_cast<uintptr_t>(weapon)+0x130,&piece,sizeof(piece))||piece.enabled>1||piece.loaded>1||piece.empty_hands>1){status=1;return;}
        // Never duplicate a loader already driven by the game itself.
        if(api.ptr(weapon,0x100)==view){visual=reinterpret_cast<uintptr_t>(api.ptr(api.ptr(view,0x50),0x10));status=3;return;}
        if(!requested_valid_||!piece.same_model(requested_)){
            requested_=piece;requested_valid_=true;attempts_=failures_=0;next_load_=0;primary_=nullptr;state_=-1;
        }
        WeaponPiece current=api.field<WeaponPiece>(view,0x58);
        void* primary=api.ptr(view,0x50);void* primary_native=api.unity(primary,"WeaponBase")?api.ptr(primary,0x10):nullptr;
        const bool ready=piece.same_model(current)&&(!piece.enabled||primary_native);
        visual=reinterpret_cast<uintptr_t>(primary_native);
        if(!ready){
            // QGB records the requested piece before its asynchronous load.
            // Wait for completion; do not cancel/restart it at the poll rate.
            if(time>=next_load_&&attempts_<2){
                ++attempts_;next_load_=time+10;status=2;
                try{api.load(view,&piece);}catch(...){status=-1;}
            }else if(attempts_>=2&&time>=next_load_)status=-2;
            return;
        }
        status=3;
        const int state=api.field<int>(weapon,0x14C);
        if(primary_!=primary){primary_=primary;state_=-1;failures_=0;}
        if(failures_>=3){status=-3;return;}
        try{
            // Reapply once after async completion even if the held state did
            // not change: the new meshes still need attaching to the hands.
            if(state>=0&&state<=6&&state_!=state){api.state(view,state);state_=state;}
        }catch(...){++failures_;status=-3;}
    }
};
}
