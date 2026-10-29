#pragma once
#include "extra_features.h"
#include <unordered_map>
namespace mango_resources {
// Mirror.NetworkClient.spawned contains client-visible NetworkIdentity objects.
// MineableObject.XCb is populated by OnStartServer and is empty on remote clients.
template<class Visit>bool visit_client(const mango_extra::Memory& memory,mango_extra::ResourceCursor& cursor,
                                     uintptr_t fields,Visit visit,unsigned budget=32){
    const uintptr_t dictionary=memory.field<uintptr_t>(fields,0x30);
    return cursor.step(memory,dictionary,[&](uintptr_t identity){
        if(!memory.field<uintptr_t>(identity,0x10)||!memory.field<uint8_t>(identity,0x4C)||
           memory.field<uint8_t>(identity,0x6D))return;
        const uintptr_t array=memory.field<uintptr_t>(identity,0x80),count=memory.field<uintptr_t>(array,0x18);
        std::array<uintptr_t,64> behaviours{};
        if(!array||!count||count>behaviours.size()||!memory.read||!memory.read(array+0x20,behaviours.data(),count*sizeof(uintptr_t)))return;
        for(size_t i=0;i<count;++i)if(behaviours[i])visit(behaviours[i],identity);
    },budget);
}
enum TypeFlag : unsigned {Mineable=1,Stone=2,Tree=4,Composite=8,StoneGroup=16};
class Types {
    std::unordered_map<uintptr_t,unsigned> known_;
public:
    unsigned flags(const mango_extra::Memory& memory,uintptr_t object){
        const uintptr_t start=memory.field<uintptr_t>(object);if(!start)return 0;
        const auto found=known_.find(start);if(found!=known_.end())return found->second;
        unsigned result=0;
        for(uintptr_t type=start,depth=0;type&&depth<16;++depth){
            char name[80]{};const uintptr_t text=memory.field<uintptr_t>(type,0x10);
            if(text&&memory.read&&memory.read(text,name,sizeof(name)-1)){
                if(!std::strcmp(name,"MineableObject"))result|=Mineable;
                else if(!std::strcmp(name,"MineableStone"))result|=Stone;
                else if(!std::strcmp(name,"MineableTree"))result|=Tree;
                else if(!std::strcmp(name,"MineableCompositeStone"))result|=Composite;
                else if(!std::strcmp(name,"kaV"))result|=StoneGroup;
            }
            const uintptr_t parent=memory.field<uintptr_t>(type,0x58);if(parent==type)break;type=parent;
        }
        if(known_.size()<2048)known_[start]=result;return result;
    }
    bool mineable(const mango_extra::Memory& memory,uintptr_t object){return (flags(memory,object)&Mineable)!=0;}
};
class StoneCursor {
    uintptr_t array_=0;unsigned cursor_=0;
public:
    template<class Visit>unsigned step(const mango_extra::Memory& m,uintptr_t manager,Visit visit,unsigned budget=32){
        const uintptr_t array=m.field<uintptr_t>(manager,0x68),count=m.field<uintptr_t>(array,0x18);
        if(!array||count>65536){array_=0;cursor_=0;return 0;}
        if(array_!=array){array_=array;cursor_=0;}if(cursor_>=count)cursor_=0;
        const unsigned amount=std::min({budget,32u,unsigned(count)-cursor_});std::array<uintptr_t,32> roots{};
        if(!amount||!m.read||!m.read(array+0x20+cursor_*sizeof(uintptr_t),roots.data(),amount*sizeof(uintptr_t)))return 0;
        cursor_+=amount;for(unsigned i=0;i<amount;++i)if(roots[i])visit(roots[i]);return amount;
    }
};
inline int item_kind(const mango_extra::Memory& m,uintptr_t text){
    const int length=m.field<int>(text,0x10);if(length<1||length>128||!m.read)return -1;
    uint16_t units[128]{};char name[129]{};
    if(!m.read(text+0x14,units,size_t(length)*2))return -1;
    for(int i=0;i<length;++i){char c=units[i]<128?char(units[i]):' ';name[i]=c>='A'&&c<='Z'?char(c+32):c;}
    if(std::strstr(name,"sulfur")||std::strstr(name,"sulphur"))return 3;
    if(std::strstr(name,"iron")||std::strstr(name,"metal"))return 2;
    if(std::strstr(name,"stone")||std::strstr(name,"rock"))return 1;
    if(std::strstr(name,"wood"))return 0;
    return -1;
}
inline int kind(const mango_extra::Memory& m,uintptr_t object,unsigned flags){
    const int entity=mango_extra::resource_kind(m.field<int>(object,0xD8));if(entity>=0)return entity;
    const uintptr_t list=m.field<uintptr_t>(object,0xA0),array=m.field<uintptr_t>(list,0x10),capacity=m.field<uintptr_t>(array,0x18);
    const int count=m.field<int>(list,0x18);
    if(array&&count>0&&count<=128&&uintptr_t(count)<=capacity){
        for(int i=0;i<std::min(count,16);++i){const uintptr_t loot=m.field<uintptr_t>(array,0x20+i*sizeof(uintptr_t));
            const int value=item_kind(m,m.field<uintptr_t>(loot,0x10));if(value>=0)return value;}
    }
    return flags&Tree?0:flags&Stone?1:-1;
}
inline bool alive(float health,bool grouped,bool active){return active&&(grouped||(std::isfinite(health)&&health>0));}
template<class V>bool on_screen(V point){return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z)&&
    point.z>.01f&&point.x>=0&&point.x<=1&&point.y>=0&&point.y<=1;}
}
