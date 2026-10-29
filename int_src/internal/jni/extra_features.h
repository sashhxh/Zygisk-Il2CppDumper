#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
namespace mango_extra {
struct V3 {float x=0,y=0,z=0;};
struct Q {float x=0,y=0,z=0,w=1;};
inline bool finite(V3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool finite(Q p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.w);}
inline bool near(V3 a,V3 b){return finite(a)&&finite(b)&&std::fabs(a.x-b.x)+std::fabs(a.y-b.y)+std::fabs(a.z-b.z)<.00003f;}
inline bool near(Q a,Q b){return finite(a)&&finite(b)&&std::fabs(a.x-b.x)+std::fabs(a.y-b.y)+std::fabs(a.z-b.z)+std::fabs(a.w-b.w)<.00003f;}
inline Q multiply(Q a,Q b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline Q euler(V3 degrees){
    const float k=.00872664626f,x=degrees.x*k,y=degrees.y*k,z=degrees.z*k;
    return multiply(multiply({0,std::sin(y),0,std::cos(y)},{std::sin(x),0,0,std::cos(x)}),{0,0,std::sin(z),std::cos(z)});
}
struct Settings {
    bool weapon=false,weapon_ads=false,auto_shoot=false,resource_esp=false,farm=false;
    V3 position{},rotation{};unsigned resources=15;float resource_range=150,farm_range=6;int resource_limit=48;
};
inline void sanitize(Settings& s){
    auto bound=[](float v,float d,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):d;};
    s.position={bound(s.position.x,0,-.5f,.5f),bound(s.position.y,0,-.5f,.5f),bound(s.position.z,0,-.5f,.5f)};
    s.rotation={bound(s.rotation.x,0,-60,60),bound(s.rotation.y,0,-60,60),bound(s.rotation.z,0,-60,60)};
    s.resources&=15;s.resource_range=bound(s.resource_range,150,25,500);s.farm_range=bound(s.farm_range,6,2,20);s.resource_limit=std::clamp(s.resource_limit,8,128);
}
inline bool load(FILE* f,Settings& out){
    Settings s;int weapon,ads,shoot,esp,farm;char tag[16]{};
    const long start=std::ftell(f);
    if(std::fscanf(f," %15s",tag)!=1|| (std::strcmp(tag,"EXTRAS_V1")&&std::strcmp(tag,"EXTRAS_V2")&&std::strcmp(tag,"EXTRAS_V3")&&std::strcmp(tag,"EXTRAS_V4"))){if(start>=0)std::fseek(f,start,SEEK_SET);return false;}
    if(std::fscanf(f," %d %d %d %d %d %f %f %f %f %f %f %u %f %f %d",&weapon,&ads,&shoot,&esp,&farm,
        &s.position.x,&s.position.y,&s.position.z,&s.rotation.x,&s.rotation.y,&s.rotation.z,&s.resources,&s.resource_range,&s.farm_range,&s.resource_limit)!=15)return false;
    if(!std::strcmp(tag,"EXTRAS_V2")||!std::strcmp(tag,"EXTRAS_V3")){int retired=0;if(std::fscanf(f," %d",&retired)!=1)return false;} // Consume the removed option from older records.
    s.weapon=weapon!=0;s.weapon_ads=ads!=0;s.auto_shoot=shoot!=0;s.resource_esp=esp!=0;s.farm=farm!=0;sanitize(s);out=s;return true;
}
inline bool save(char* text,size_t size,Settings s){
    sanitize(s);int n=std::snprintf(text,size,"EXTRAS_V4 %d %d %d %d %d %.4f %.4f %.4f %.2f %.2f %.2f %u %.1f %.1f %d\n",s.weapon,s.weapon_ads,s.auto_shoot,s.resource_esp,s.farm,
        s.position.x,s.position.y,s.position.z,s.rotation.x,s.rotation.y,s.rotation.z,s.resources,s.resource_range,s.farm_range,s.resource_limit);return n>=0&&size_t(n)<size;
}
struct Memory {
    bool(*read)(uintptr_t,void*,size_t)=nullptr;bool(*write)(uintptr_t,const void*,size_t)=nullptr;
    template<class T>T field(uintptr_t p,size_t offset=0)const{T v{};if(p&&read&&offset<=UINTPTR_MAX-p&&read(p+offset,&v,sizeof(v)))return v;return {};}
};
struct Ref {
    uintptr_t object=0,klass=0,native=0;
    static Ref capture(const Memory& m,uintptr_t p){return {p,m.field<uintptr_t>(p),m.field<uintptr_t>(p,0x10)};}
    bool valid(const Memory& m)const{return object&&klass&&native&&m.field<uintptr_t>(object)==klass&&m.field<uintptr_t>(object,0x10)==native;}
};
struct MotionApi {
    Memory memory;
    void(*position)(void*,V3*)=nullptr;void(*set_position)(void*,V3*)=nullptr;
    void(*rotation)(void*,Q*)=nullptr;void(*set_rotation)(void*,Q*)=nullptr;
};
class WeaponOffset {
    Ref transform_{};V3 original_p_{},written_p_{};Q original_q_{},written_q_{};bool pending_=false;
public:
    void restore(MotionApi& api){
        if(pending_&&transform_.valid(api.memory)&&api.position&&api.rotation&&api.set_position&&api.set_rotation){
            V3 p;Q q;api.position(reinterpret_cast<void*>(transform_.native),&p);api.rotation(reinterpret_cast<void*>(transform_.native),&q);
            if(near(p,written_p_))api.set_position(reinterpret_cast<void*>(transform_.native),&original_p_);
            if(near(q,written_q_))api.set_rotation(reinterpret_cast<void*>(transform_.native),&original_q_);
        }
        pending_=false;transform_={};
    }
    void apply(MotionApi& api,uintptr_t transform,const Settings& input){
        if(!input.weapon||!api.position||!api.rotation||!api.set_position||!api.set_rotation)return;
        Settings s=input;sanitize(s);if(near(s.position,{})&&near(s.rotation,{}))return;
        const auto ref=Ref::capture(api.memory,transform);if(!ref.valid(api.memory))return;
        V3 p;Q q;api.position(reinterpret_cast<void*>(ref.native),&p);api.rotation(reinterpret_cast<void*>(ref.native),&q);if(!finite(p)||!finite(q))return;
        transform_=ref;original_p_=p;original_q_=q;written_p_={p.x+s.position.x,p.y+s.position.y,p.z+s.position.z};written_q_=multiply(q,euler(s.rotation));
        api.set_position(reinterpret_cast<void*>(ref.native),&written_p_);api.set_rotation(reinterpret_cast<void*>(ref.native),&written_q_);pending_=true;
    }
};
class FloatPatch {
    Ref ref_{};size_t offset_=0;float original_=0,written_=0;bool pending_=false;
public:
    void restore(const Memory& m){
        if(pending_&&ref_.valid(m)&&m.write){float value=m.field<float>(ref_.object,offset_);if(std::isfinite(value)&&std::fabs(value-written_)<.00001f)m.write(ref_.object+offset_,&original_,sizeof(original_));}
        pending_=false;ref_={};
    }
    void extend(const Memory& m,uintptr_t object,size_t offset,float value){
        auto ref=Ref::capture(m,object);if(!ref.valid(m)||!m.write||!std::isfinite(value)||value<0||value>500){restore(m);return;}
        if(pending_&&(ref_.object!=object||ref_.native!=ref.native||ref_.klass!=ref.klass||offset_!=offset))restore(m);
        const float current=m.field<float>(object,offset);if(!std::isfinite(current)||current<0||current>500)return;
        if(pending_&&std::fabs(current-written_)>.00001f)pending_=false;
        if(!pending_){ref_=ref;offset_=offset;original_=current;}
        const float desired=std::max(original_,value);if(std::fabs(desired-current)<.00001f)return;
        written_=desired;pending_=m.write(object+offset,&written_,sizeof(written_));
    }
};
// Per-feature attempts are bounded; the game's event handler owns ammo,
// reload, damage, attack cadence and server validation.
class Trigger {
    uintptr_t owner_=0;double next_=0;
public:
    void reset(){owner_=0;next_=0;}
    template<class Fire>bool tick(uintptr_t owner,bool enabled,bool menu,bool ready,double time,Fire fire){
        if(!owner||!enabled||menu||!std::isfinite(time)){reset();return false;}
        if(owner_!=owner){reset();owner_=owner;}if(!ready||time<next_)return false;
        next_=time+.05;return fire();
    }
};
inline int resource_kind(int entity){return entity>=8&&entity<=11?entity-8:-1;}
inline const char* resource_name(int kind){static const char* names[]={"Wood","Stone","Iron","Sulfur"};return kind>=0&&kind<4?names[kind]:"Resource";}
class ResourceCursor {
    uintptr_t dictionary_=0,array_=0;uint32_t cursor_=0;
public:
    void reset(){dictionary_=array_=0;cursor_=0;}
    template<class Visit>bool step(const Memory& m,uintptr_t dictionary,Visit visit,unsigned budget=64){
        const uintptr_t array=m.field<uintptr_t>(dictionary,0x18),capacity=m.field<uintptr_t>(array,0x18);
        const int count=m.field<int>(dictionary,0x20);
        if(!dictionary||!array||capacity>1048576||count<0||uintptr_t(count)>capacity){reset();return false;}
        if(dictionary_!=dictionary||array_!=array){dictionary_=dictionary;array_=array;cursor_=0;}
        if(!count){cursor_=0;return true;}if(cursor_>=unsigned(count))cursor_=0;
        struct Entry {uint32_t hash;int32_t next;uint32_t key,padding;uintptr_t value;};static_assert(sizeof(Entry)==24);
        std::array<Entry,64> entries{};const unsigned amount=std::min({budget,64u,unsigned(count)-cursor_});
        if(amount&&!m.read(array+0x20+cursor_*sizeof(Entry),entries.data(),amount*sizeof(Entry)))return false;
        cursor_+=amount;for(unsigned i=0;i<amount;++i)if(entries[i].value&&entries[i].hash!=UINT32_MAX&&entries[i].next>=-1)visit(entries[i].value);
        return true;
    }
};
}
