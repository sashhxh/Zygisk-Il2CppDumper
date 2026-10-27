#include "esp_capture.h"
#include <cassert>
#include <cstring>
#include <limits>
#include <map>
#include <vector>

struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t at,T value) {
        const auto* p=reinterpret_cast<const unsigned char*>(&value);
        for(size_t i=0;i<sizeof(value);++i)bytes[at+i]=p[i];
    }
    bool read(uintptr_t at,void* out,size_t count) {
        for(size_t i=0;i<count;++i){auto it=bytes.find(at+i);if(it==bytes.end())return false;static_cast<unsigned char*>(out)[i]=it->second;}
        return true;
    }
    void list(uintptr_t list,uintptr_t array,std::initializer_list<uintptr_t> values) {
        put(list+0x10,array);put(list+0x18,int32_t(values.size()));put(list+0x1c,int32_t(1));
        put(array+0x18,uintptr_t(values.size()));
        size_t i=0;for(auto object:values)put(array+0x20+8*i++,object);
    }
};
struct V {float x,y,z;};
bool close(float x,float y){return std::fabs(x-y)<1e-5f;}
int main(){
    Memory memory;memory.list(0x1000,0x2000,{0x5000,0x6000});
    auto reader=[&](uintptr_t at,void* out,size_t size){return memory.read(at,out,size);};
    std::vector<uintptr_t> players;
    auto capture=[&](uintptr_t list){players.clear();return mango_esp::visit_list(list,reader,[&](uintptr_t p){players.push_back(p);});};
    assert(capture(0x1000));assert((players==std::vector<uintptr_t>{0x5000,0x6000}));
    // Disconnect with the old objects still allocated, then enter a new server.
    memory.put(0x1018,int32_t(0));assert(capture(0x1000)&&players.empty());
    memory.list(0x3000,0x4000,{0x7000,0x8000});
    assert(capture(0x3000));assert((players==std::vector<uintptr_t>{0x7000,0x8000}));
    memory.put(0x3018,int32_t(-1));assert(!capture(0x3000));
    memory.put(0x3018,int32_t(3));assert(!capture(0x3000));
    memory.put(0x3018,int32_t(2));
    assert(!mango_esp::visit_list(0x3000,reader,[&](uintptr_t){memory.put(0x301c,int32_t(2));}));
    assert(!capture(0x9999));
    // Pointer batches bound kernel reads even on crowded servers.
    memory.put(0xa010,uintptr_t(0xb000));memory.put(0xa018,int32_t(128));memory.put(0xa01c,int32_t(1));
    memory.put(0xb018,uintptr_t(128));
    for(uintptr_t i=0;i<128;++i)memory.put(0xb020+i*8,uintptr_t(0xc000+i*16));
    unsigned batches=0,count=0;
    assert(mango_esp::visit_list(0xa000,[&](uintptr_t at,void* out,size_t size){if(size==512)++batches;return memory.read(at,out,size);},[&](uintptr_t){++count;}));
    assert(count==128&&batches==2);

    mango_esp::Session session;
    const mango_esp::SessionKey first{0x1000,0x5000,0xa000,0xb000,0xc000};
    assert(session.update(first));assert(!session.update(first));
    assert(session.update({}));assert(session.update(first));
    auto replacement=first;replacement.native_local=0xd000;
    assert(session.update(replacement));assert(session.generation()==4);

    mango_esp::LivePlayers live;
    live.remember({1,11,1.,true});live.remember({2,22,1.,false});
    std::vector<uintptr_t> seen;
    live.visit(1.1,[&](const auto& entry){seen.push_back(entry.address);});
    assert((seen==std::vector<uintptr_t>{1,2}));
    seen.clear();live.visit(2.,[&](const auto& entry){seen.push_back(entry.address);});assert(seen.empty());
    live.remember({3,33,2.,true});
    live.visit(2.,[&](const auto& entry){seen.push_back(entry.address);});assert((seen==std::vector<uintptr_t>{3}));
    live.remember({3,44,2.1,true});seen.clear();
    live.visit(2.1,[&](const auto& entry){assert(entry.native==44);seen.push_back(entry.address);});assert(seen.size()==1);
    live.clear();seen.clear();live.visit(2.1,[&](const auto& entry){seen.push_back(entry.address);});assert(seen.empty());
    for(uintptr_t i=1;i<=600;++i)live.remember({i,i+1000,3.+double(i)*.0001,false});
    seen.clear();live.visit(3.06,[&](const auto& entry){seen.push_back(entry.address);});
    assert(seen.size()==512&&std::find(seen.begin(),seen.end(),600)!=seen.end());

    mango_esp::FrameGate gate;
    assert(gate.accept(1,0));assert(!gate.accept(1,0));assert(gate.accept(1,1));
    assert(!gate.accept(1,2));assert(!gate.accept(1,1));assert(!gate.accept(1,2));
    // No fixed 60 Hz time gate: every 120/144 Hz frame is accepted.
    for(int frame=2;frame<146;++frame)assert(gate.accept(frame,2));
    assert(gate.accept(0,2)); // frame counter reset / resumed player loop

    mango_esp::Projection camera;
    camera.view.m[0]=camera.view.m[5]=camera.view.m[15]=1;camera.view.m[10]=-1;
    camera.projection.m[0]=camera.projection.m[5]=1;
    camera.projection.m[10]=-1.02f;camera.projection.m[11]=-1;camera.projection.m[14]=-.2f;
    assert(camera.validate());V out{};
    assert(camera.project(V{0,0,10},out)&&close(out.x,.5f)&&close(out.y,.5f));
    assert(camera.project(V{10,10,10},out)&&close(out.x,1)&&close(out.y,1));
    assert(!camera.project(V{0,0,-10},out));assert(!camera.project(V{0,0,0},out));
    camera.view.m[12]=-1;
    assert(camera.project(V{0,0,10},out)&&close(out.x,.45f));
    camera.view.m[12]=-2;
    assert(camera.project(V{0,0,10},out)&&close(out.x,.4f));
    camera.view.m[12]=0;camera.rect={.25f,.1f,.5f,.8f};
    assert(camera.project(V{10,10,10},out)&&close(out.x,.75f)&&close(out.y,.9f));
    camera.projection.m[0]=std::numeric_limits<float>::quiet_NaN();
    assert(!camera.validate());assert(!camera.project(V{0,0,10},out));
}
