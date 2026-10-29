#include "graphics_roots.h"
#include <cassert>
struct Managed {void* klass=nullptr,*monitor=nullptr,*native=nullptr;int id=0;};
Managed first,second;int finds=0;
bool read(uintptr_t p,void* out,size_t n){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),n);return true;}
void* find(int32_t id){++finds;return id==1?&first:id==2?&second:nullptr;}
int reg(void* p,size_t size,void*){assert(p&&size==2048*sizeof(void*));return 1;}
int main(){
    mango_chams::Api api;api.read=read;api.find=find;api.store_ref=[](uintptr_t at,void* p){std::memcpy(reinterpret_cast<void*>(at),&p,sizeof(p));};
    mango_graphics::Roots roots;assert(roots.init([](const char*){return reinterpret_cast<void*>(reg);},api));
    int native1=1,native2=2;first.native=&native1;second.native=&native2;
    roots.retain(1,reinterpret_cast<uintptr_t>(&native1));roots.retain(1,reinterpret_cast<uintptr_t>(&native1));const int initial=finds;
    for(int i=0;i<200;++i)assert(roots.live(1,reinterpret_cast<uintptr_t>(&native1))==&native1);
    assert(finds==initial);
    // Destroying Unity's native object clears the retained managed wrapper.
    // Old readable native memory must not make a dead object look alive.
    first.native=nullptr;assert(!roots.live(1,reinterpret_cast<uintptr_t>(&native1)));
    roots.release(1,reinterpret_cast<uintptr_t>(&native1));assert(!roots.live(1,reinterpret_cast<uintptr_t>(&native1)));
    roots.release(1,reinterpret_cast<uintptr_t>(&native1));
    roots.retain(2,reinterpret_cast<uintptr_t>(&native2));assert(roots.live(2,reinterpret_cast<uintptr_t>(&native2))==&native2);
    roots.release(2,reinterpret_cast<uintptr_t>(&native2));
}
