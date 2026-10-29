#include "extra_features.h"
#include <cassert>
#include <vector>
#include <limits>
namespace {
struct Block {std::array<unsigned char,512> bytes{};};
std::vector<Block*> blocks;unsigned writes=0;
bool read(uintptr_t p,void* out,size_t n){for(auto* b:blocks){const auto start=reinterpret_cast<uintptr_t>(b);if(p>=start&&p+n<=start+sizeof(Block)){std::memcpy(out,reinterpret_cast<void*>(p),n);return true;}}return false;}
bool write(uintptr_t p,const void* out,size_t n){for(auto* b:blocks){const auto start=reinterpret_cast<uintptr_t>(b);if(p>=start&&p+n<=start+sizeof(Block)){++writes;std::memcpy(reinterpret_cast<void*>(p),out,n);return true;}}return false;}
template<class T>void put(Block& b,size_t offset,T v){assert(offset+sizeof(T)<=b.bytes.size());std::memcpy(b.bytes.data()+offset,&v,sizeof(T));}
uintptr_t address(Block& b){return reinterpret_cast<uintptr_t>(&b);}
mango_extra::V3 position{1,2,3};mango_extra::Q rotation{};
FILE* input(const char* text){FILE* f=std::tmpfile();assert(f);std::fputs(text,f);std::rewind(f);return f;}
}
int main(){
    using namespace mango_extra;Memory m{read,write};Block object,native,dictionary,array;blocks={&object,&native,&dictionary,&array};
    put(object,0,uintptr_t(0x1234));put(object,0x10,address(native));put(object,0x130,2.f);
    FloatPatch patch;patch.extend(m,address(object),0x130,10);assert(m.field<float>(address(object),0x130)==10);
    unsigned saved=writes;patch.extend(m,address(object),0x130,10);assert(writes==saved);
    patch.restore(m);assert(m.field<float>(address(object),0x130)==2);
    patch.extend(m,address(object),0x130,10);put(object,0x130,12.f);patch.restore(m);assert(m.field<float>(address(object),0x130)==12);
    patch.extend(m,address(object),0x130,20);put(object,0x10,uintptr_t(0x5555));patch.restore(m);assert(m.field<float>(address(object),0x130)==20);
    put(object,0x10,address(native));
    MotionApi api;api.memory=m;api.position=[](void*,V3* p){*p=position;};api.rotation=[](void*,Q* q){*q=rotation;};api.set_position=[](void*,V3* p){position=*p;};api.set_rotation=[](void*,Q* q){rotation=*q;};
    Settings settings;settings.weapon=true;settings.position={.1f,-.2f,.3f};settings.rotation={0,0,30};WeaponOffset weapon;
    for(int frame=0;frame<200;++frame){weapon.restore(api);assert(near(position,{1,2,3})&&near(rotation,Q{}));weapon.apply(api,address(object),settings);assert(near(position,{1.1f,1.8f,3.3f}));}
    weapon.restore(api);assert(near(position,{1,2,3})&&near(rotation,Q{}));
    weapon.apply(api,address(object),settings);position={7,8,9};weapon.restore(api);assert(near(position,{7,8,9}));
    Trigger trigger;int calls=0;auto fire=[&]{++calls;return true;};
    assert(!trigger.tick(1,true,true,true,1,fire)&&calls==0);
    assert(trigger.tick(1,true,false,true,1,fire)&&calls==1);
    assert(!trigger.tick(1,true,false,true,1.01,fire)&&calls==1);
    assert(!trigger.tick(1,true,false,false,1.02,fire)&&calls==1);
    assert(!trigger.tick(1,true,false,true,1.03,fire)&&calls==1);
    assert(trigger.tick(1,true,false,true,1.06,fire)&&calls==2);
    assert(!trigger.tick(1,false,false,true,1.1,fire));assert(trigger.tick(2,true,false,true,1.11,fire));
    assert(!trigger.tick(2,true,false,true,std::numeric_limits<double>::quiet_NaN(),fire));
    ResourceCursor cursor;put(dictionary,0x18,address(array));put(dictionary,0x20,int(6));put(array,0x18,uintptr_t(6));
    for(unsigned i=0;i<6;++i){put(array,0x20+i*24+4,int32_t(i==2?-3:-1));put(array,0x20+i*24+16,uintptr_t(100+i));}
    std::vector<uintptr_t> found;auto visit=[&](uintptr_t p){found.push_back(p);};
    assert(cursor.step(m,address(dictionary),visit,2)&&found==std::vector<uintptr_t>({100,101}));
    assert(cursor.step(m,address(dictionary),visit,2)&&found==std::vector<uintptr_t>({100,101,103}));
    assert(cursor.step(m,address(dictionary),visit,2)&&found.back()==105);
    assert(cursor.step(m,address(dictionary),visit,1)&&found.back()==100);
    put(dictionary,0x20,int(100));assert(!cursor.step(m,address(dictionary),visit));
    assert(resource_kind(8)==0&&resource_kind(11)==3&&resource_kind(19)==-1);
    Settings record=settings;record.auto_shoot=true;record.farm=true;record.resource_esp=true;record.resources=5;record.farm_range=10;
    char text[512];assert(save(text,sizeof(text),record));assert(std::strstr(text,"EXTRAS_V4")==text);Settings loaded;FILE* f=input(text);assert(load(f,loaded));std::fclose(f);
    assert(loaded.auto_shoot&&loaded.farm&&loaded.resource_esp&&loaded.resources==5&&loaded.farm_range==10&&near(loaded.position,settings.position));
    f=input("EXTRAS_V1 0 0 0 0 1 0 0 0 0 0 0 15 150 6 48\n");Settings legacy;
    assert(load(f,legacy)&&legacy.farm);std::fclose(f);
    f=input("EXTRAS_V2 0 0 0 0 1 0 0 0 0 0 0 15 150 6 48 1\n");Settings previous;
    assert(load(f,previous)&&previous.farm);std::fclose(f);
    f=input("EXTRAS_V3 0 0 0 0 1 0 0 0 0 0 0 15 150 6 48 1 NEXT\n");Settings retired;
    assert(load(f,retired)&&retired.farm);char next[16]{};assert(std::fscanf(f," %15s",next)==1&&!std::strcmp(next,"NEXT"));std::fclose(f);
    f=input("EXTRAS_V2 0 0 0 0 1 0 0 0 0 0 0 15 150 6 48\n");Settings truncated;
    assert(!load(f,truncated));std::fclose(f);
    for(const char* old:{"","EXTRAS_V1 1 1","CHAMS_STYLE_V1"}){Settings d;f=input(old);assert(!load(f,d));std::fclose(f);assert(!d.weapon&&!d.auto_shoot&&!d.resource_esp&&!d.farm);}
    std::puts("PASS: viewmodel restoration/nonaccumulation, owned reach restoration, trigger gates, bounded registry cursor, typed resources, settings compatibility");
}
