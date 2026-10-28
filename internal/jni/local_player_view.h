#pragma once
#include <cstdint>
#include <cstddef>
#include <initializer_list>

namespace mango_camera {
// PlayerManager has a network body (0x260) and a separate animated local body
// (0x268). Hjr creates the latter; Player.Update drives its Animator directly.
// The first-person render controller is FPManager.m_FPRenderController (0x28).
struct PlayerView {void* info=nullptr;void* animator=nullptr;void* render_controller=nullptr;uint32_t reserved_layers=0;bool local=false;void* lease=nullptr;void* animation=nullptr;void* equipment=nullptr;};
template<class Read,class IsType>
PlayerView player_view(void* player,Read read,IsType is_type){
    auto ptr=[&](void* p,size_t offset){void* v=nullptr;if(p)read(reinterpret_cast<uintptr_t>(p)+offset,&v,sizeof(v));return v;};
    PlayerView result;
    for(size_t offset:{size_t(0x268),size_t(0x260)}){
        void* lease=ptr(player,offset);if(!is_type(lease,"Yf"))continue;
        void* info=ptr(lease,0x10);if(!is_type(info,"PlayerModelInfo")||!ptr(info,0x10))continue;
        void* animation=ptr(info,0x60);if(!is_type(animation,"CharacterAnimation")||!ptr(animation,0x10))continue;
        void* animator=ptr(animation,0x20);if(!is_type(animator,"Animator")||!ptr(animator,0x10))continue;
        result.info=info;result.animator=animator;result.local=offset==0x268;result.lease=lease;result.animation=animation;result.equipment=ptr(lease,0x18);break;
    }
    void* fp=ptr(player,0x90);if(!is_type(fp,"FPManager"))return result;
    void* controller=ptr(fp,0x28);if(!is_type(controller,"FirstPersonRenderController")||!ptr(controller,0x10))return result;
    result.render_controller=controller;
    void* fields=ptr(ptr(controller,0),0xB8);
    uint32_t masks[2]{};if(fields&&read(reinterpret_cast<uintptr_t>(fields)+0xC,masks,sizeof(masks)))result.reserved_layers=masks[0]|masks[1];
    return result;
}
}
