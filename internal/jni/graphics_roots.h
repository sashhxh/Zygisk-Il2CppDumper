#pragma once
#include "chams.h"

namespace mango_graphics {
// Root the managed wrappers while caching native pointers. Unity clears a
// rooted wrapper's m_CachedPtr on destruction; a readable old native ID alone
// is not proof that an allocation is still alive.
class Roots {
    struct Entry {int32_t id=0;uintptr_t native=0;unsigned refs=0;};
    std::array<void*,2048> managed_{};
    std::array<Entry,2048> entries_{};
    mango_chams::Api* api_=nullptr;bool ready_=false;
public:
    bool init(mango_chams::Api::Resolve lookup,mango_chams::Api& api){
        api_=&api;if(ready_)return true;
        auto reg=reinterpret_cast<int(*)(void*,size_t,void*)>(lookup("mono_gc_register_root"));
        ready_=reg&&reg(managed_.data(),sizeof(managed_),nullptr)!=0;return ready_;
    }
    void type(unsigned slot,void* object){if(ready_&&slot<2)api_->store_ref(reinterpret_cast<uintptr_t>(&managed_[slot]),object);}
    void retain(int32_t id,uintptr_t native){
        if(!ready_||!id||!native)return;
        size_t vacant=managed_.size();
        for(size_t i=2;i<entries_.size();++i){
            auto& e=entries_[i];if(e.id==id&&e.native==native&&e.refs){++e.refs;return;}
            if(!e.refs&&vacant==managed_.size())vacant=i;
        }
        if(vacant==managed_.size())return;
        void* managed=api_->find(id);if(reinterpret_cast<uintptr_t>(api_->native(managed))!=native)return;
        entries_[vacant]={id,native,1};api_->store_ref(reinterpret_cast<uintptr_t>(&managed_[vacant]),managed);
    }
    void release(int32_t id,uintptr_t native){
        if(!ready_)return;
        for(size_t i=2;i<entries_.size();++i){auto& e=entries_[i];if(e.id!=id||e.native!=native||!e.refs)continue;
            if(--e.refs==0){api_->store_ref(reinterpret_cast<uintptr_t>(&managed_[i]),nullptr);e={};}return;}
    }
    void* live(int32_t id,uintptr_t native){
        if(ready_)for(size_t i=2;i<entries_.size();++i){const auto& e=entries_[i];if(e.id==id&&e.native==native&&e.refs){void* p=api_->native(managed_[i]);return reinterpret_cast<uintptr_t>(p)==native?p:nullptr;}}
        void* p=api_->live(id);return reinterpret_cast<uintptr_t>(p)==native?p:nullptr;
    }
};
}
