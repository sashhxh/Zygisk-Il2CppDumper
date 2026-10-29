#include "resource_esp.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
static unsigned char bytes[131072]{};static size_t largest_read=0;
static bool read_mem(uintptr_t p,void* out,size_t n){largest_read=std::max(largest_read,n);if(p<1||p>sizeof(bytes)||n>sizeof(bytes)-p)return false;std::memcpy(out,bytes+p,n);return true;}
template<class T>void put(uintptr_t p,T value){assert(p+sizeof(T)<=sizeof(bytes));std::memcpy(bytes+p,&value,sizeof(value));}
static void string(uintptr_t p,const char* text){int n=int(std::strlen(text));put(p+0x10,n);for(int i=0;i<n;++i)put<uint16_t>(p+0x14+i*2,uint8_t(text[i]));}
static void klass(uintptr_t at,uintptr_t name,const char* text,uintptr_t parent=0){put(at+0x10,name);std::strcpy(reinterpret_cast<char*>(bytes+name),text);put(at+0x58,parent);}
int main(){
    const mango_extra::Memory m{read_mem,nullptr};mango_resources::Types types;
    klass(65536,70000,"MineableObject");klass(66048,70256,"MineableTree",65536);klass(66560,70512,"MineableStone",65536);klass(67072,70768,"kaV");
    const uintptr_t group=4096,array=8192;put(group,uintptr_t(67072));put<uintptr_t>(group+0x10,10);put(group+0x68,array);put<uintptr_t>(array+0x18,4);
    std::vector<int> kinds;
    for(int i=0;i<3;++i){uintptr_t root=12288+i*512,ore=24576+i*512,list=32768+i*512,items=40960+i*512,loot=49152+i*256,text=73728+i*512;
        put(array+0x20+i*8,root);put<uintptr_t>(root+0x10,99+i);put(root+0x20,ore);put<uint8_t>(root+0x40,1);
        put(ore,uintptr_t(66560));put<uintptr_t>(ore+0x10,200+i);put<float>(ore+0x78,0);put(ore+0xA0,list);
        put(list+0x10,items);put<int>(list+0x18,1);put<uintptr_t>(items+0x18,1);put(items+0x20,loot);put(loot+0x10,text);
        string(text,i==0?"stone":i==1?"iron_ore":"sulfur_ore");}
    assert(types.flags(m,group)==mango_resources::StoneGroup);
    mango_resources::StoneCursor cursor;
    auto visit=[&](uintptr_t root){if(!m.field<uint8_t>(root,0x40))return;auto ore=m.field<uintptr_t>(root,0x20);auto flags=types.flags(m,ore);assert(flags&mango_resources::Mineable);
        assert(mango_resources::alive(m.field<float>(ore,0x78),true,true));kinds.push_back(mango_resources::kind(m,ore,flags));};
    assert(cursor.step(m,group,visit,2)==2&&kinds==std::vector<int>({1,2}));
    assert(cursor.step(m,group,visit,2)==2&&kinds==std::vector<int>({1,2,3}));
    kinds.clear();put<uint8_t>(12288+0x40,0);cursor.step(m,group,visit,4);assert(kinds==std::vector<int>({2,3}));
    // Standalone trees still use the serialized entity type and positive health.
    put<uintptr_t>(56320,66048);put<int>(56320+0xD8,8);assert(mango_resources::kind(m,56320,types.flags(m,56320))==0);
    assert(!mango_resources::alive(0,false,true)&&mango_resources::alive(100,false,true));
    assert(!mango_resources::alive(100,true,false)&&mango_resources::alive(0,true,true));
    // Replacing a chunk resets traversal; corrupt lengths never cause an unbounded read.
    put(group+0x68,uintptr_t(86016));put<uintptr_t>(86016+0x18,1);put<uintptr_t>(86016+0x20,13312);kinds.clear();assert(cursor.step(m,group,visit)==1&&kinds==std::vector<int>({3}));
    put<uintptr_t>(86016+0x18,65537);assert(cursor.step(m,group,visit)==0);assert(largest_read<=256);
    std::puts("resource_groups: PASS (stone/iron/sulfur chunk arrays, zero unsynchronized health, loot fallback, inactive roots, bounded traversal, chunk replacement, trees)");
}
