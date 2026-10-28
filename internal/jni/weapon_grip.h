#pragma once
#include "local_weapon_view.h"
namespace mango_camera {
// Return the local gun's authored left-hand target only during ordinary hold
// states. Reload/draw/holster keep their original animation and hand movement.
inline void* left_hand_grip(const WeaponViewApi& api,void* player,const PlayerView& view){
    if(!view.local||!api.type(view.lease,"Yf"))return nullptr;
    void* weapon=api.ptr(api.ptr(player,0xF0),0x18);
    if(!api.unity(weapon,"PlayerWeapon")||api.ptr(weapon,0x160)!=player)return nullptr;
    const int state=api.field<int>(weapon,0x14C);if(state!=0&&state!=2&&state!=5)return nullptr;
    const WeaponPiece piece=api.field<WeaponPiece>(weapon,0x130);if(!piece.enabled||piece.empty_hands)return nullptr;
    void* held=api.ptr(api.ptr(view.lease,0x20),0x50);
    if(!api.unity(held,"WeaponBase")||!api.field<uint8_t>(held,0x88))return nullptr;
    void* config=api.ptr(held,0x68);
    if(!api.unity(config,"WeaponAnimationConfig"))return nullptr;
    const bool firearm=api.type(held,"WeaponMachineGun")||api.type(held,"WeaponBow")||api.type(held,"WeaponCrossbow")||api.type(held,"WeaponRocketLauncher");
    if(!api.field<uint8_t>(config,0x28)&&!firearm)return nullptr;
    void* pivot=api.ptr(held,0x98);return api.unity(pivot,"Transform")?pivot:nullptr;
}
}
