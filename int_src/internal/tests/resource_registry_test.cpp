#include "resource_esp.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
static unsigned char bytes[32768]{};static size_t max_read=0,reads=0;
static bool read_mem(uintptr_t p,void* out,size_t n){++reads;max_read=std::max(max_read,n);if(p<1||p>sizeof(bytes)||n>sizeof(bytes)-p)return false;std::memcpy(out,bytes+p,n);return true;}
template<class T>void put(uintptr_t p,T value){assert(p+sizeof(T)<=sizeof(bytes));std::memcpy(bytes+p,&value,sizeof(value));}
static void entry(int index,uintptr_t value,int32_t next=-1,uint32_t hash=1){uintptr_t p=2048+0x20+index*24;put(p,hash);put(p+4,next);put(p+16,value);}
static void identity(uintptr_t p,uintptr_t array){put<uintptr_t>(p+0x10,99);put<uint8_t>(p+0x4C,1);put<uintptr_t>(p+0x80,array);}
int main(){
    const mango_extra::Memory memory{read_mem,nullptr};mango_extra::ResourceCursor cursor;mango_resources::Types types;
    const uintptr_t fields=128,dict=1024,array=2048;
    put(fields+0x30,dict);put(dict+0x18,array);put<int>(dict+0x20,5);put<uintptr_t>(array+0x18,5);
    identity(4096,6144);identity(4608,6656);identity(5120,7168);
    put<uint8_t>(5120+0x4C,0);
    put<uintptr_t>(6144+0x18,2);put<uintptr_t>(6144+0x20,8192);put<uintptr_t>(6144+0x28,8704);
    put<uintptr_t>(6656+0x18,1);put<uintptr_t>(6656+0x20,9216);
    put<uintptr_t>(7168+0x18,1);put<uintptr_t>(7168+0x20,9728);
    const uintptr_t ordinary=16384,base=16640,derived=16896;
    std::strcpy(reinterpret_cast<char*>(bytes+18432),"PlayerManager");std::strcpy(reinterpret_cast<char*>(bytes+18688),"MineableObject");std::strcpy(reinterpret_cast<char*>(bytes+18944),"MineableCompositeStone");
    put(ordinary+0x10,uintptr_t(18432));put(base+0x10,uintptr_t(18688));put(derived+0x10,uintptr_t(18944));put(derived+0x58,base);
    put(8192,ordinary);put(8704,base);put(9216,derived);put(9728,base);
    entry(0,4096);entry(1,4608);entry(2,5120);entry(3,4096,-3);entry(4,4096,-1,UINT32_MAX);
    std::vector<uintptr_t> resources;
    auto visit=[&](uintptr_t object,uintptr_t owner){assert(owner==4096||owner==4608);if(types.mineable(memory,object))resources.push_back(object);};
    assert(mango_resources::visit_client(memory,cursor,fields,visit)&&resources==std::vector<uintptr_t>({8704,9216}));
    // A remote client's empty server-only registry must not affect discovery.
    put<int>(11000+0x20,0);put<uintptr_t>(11000+0x18,11520);put<uintptr_t>(11520+0x18,0);
    mango_extra::ResourceCursor server;unsigned server_count=0;assert(server.step(memory,11000,[&](uintptr_t){++server_count;}));assert(server_count==0);
    resources.clear();assert(mango_resources::visit_client(memory,cursor,fields,visit)&&resources.size()==2);
    // Destroyed/unstarted/oversized component arrays are skipped.
    put<uint8_t>(4096+0x6D,1);put<uintptr_t>(6656+0x18,65);resources.clear();
    assert(mango_resources::visit_client(memory,cursor,fields,visit)&&resources.empty());
    put<uint8_t>(4096+0x6D,0);put<uintptr_t>(6656+0x18,1);
    cursor.reset();resources.clear();assert(mango_resources::visit_client(memory,cursor,fields,visit,1)&&resources==std::vector<uintptr_t>({8704}));
    assert(mango_resources::visit_client(memory,cursor,fields,visit,1)&&resources==std::vector<uintptr_t>({8704,9216}));
    // Legitimate large registries still read a bounded slice, never the full array.
    put<int>(dict+0x20,50000);put<uintptr_t>(array+0x18,50000);cursor.reset();resources.clear();
    const size_t before=reads;assert(mango_resources::visit_client(memory,cursor,fields,visit,1)&&reads-before<30&&max_read<=1536);
    put<int>(dict+0x20,1);put<uintptr_t>(array+0x18,1);cursor.reset();
    assert(!mango_resources::visit_client(memory,cursor,0,visit));
    // Replacing the client dictionary after reconnect resets its traversal.
    put(fields+0x30,uintptr_t(12000));put<uintptr_t>(12000+0x18,12544);put<int>(12000+0x20,1);put<uintptr_t>(12544+0x18,1);
    put<int32_t>(12544+0x24,-1);put<uintptr_t>(12544+0x30,4608);resources.clear();
    assert(mango_resources::visit_client(memory,cursor,fields,visit)&&resources==std::vector<uintptr_t>({9216}));
    using V=mango_extra::V3;assert(mango_resources::on_screen(V{.5f,.5f,10}));
    assert(!mango_resources::on_screen(V{1.1f,.5f,1})&&!mango_resources::on_screen(V{.5f,-.1f,1})&&!mango_resources::on_screen(V{.5f,.5f,-1}));
    assert(!mango_resources::on_screen(V{std::numeric_limits<float>::quiet_NaN(),.5f,1}));
    std::puts("resource_registry: PASS (remote client, derived resources, holes, lifecycle, budgets, reconnect, screen filter)");
}
