#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>

namespace mango_esp {
enum class Visibility : int8_t { Unknown=-1, Occluded=0, Visible=1 };
struct SessionKey {
    uintptr_t list=0,local=0,camera=0,native_local=0,native_camera=0;
    bool operator==(const SessionKey& b) const {
        return list==b.list&&local==b.local&&camera==b.camera&&
               native_local==b.native_local&&native_camera==b.native_camera;
    }
};
class Session {
    SessionKey key_{};
    uint64_t generation_=0;
public:
    bool update(const SessionKey& key) {
        if(key==key_)return false;
        key_=key;++generation_;return true;
    }
    uint64_t generation() const {return generation_;}
};
class FrameGate {
    int frame_=-1,phase_=-1;
public:
    bool accept(int frame,int phase) {
        if(frame==frame_&&(phase_>0||phase<=phase_))return false;
        if(frame!=frame_)phase_=-1;
        frame_=frame;phase_=phase;return true;
    }
};
class LivePlayers {
public:
    struct Entry {uintptr_t address=0,native=0;double stamp=0;bool local=false;};
private:
    std::vector<Entry> entries_;
public:
    void clear(){entries_.clear();}
    void remember(Entry entry) {
        if(!entry.address||!entry.native)return;
        entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[&](const Entry& old){return entry.stamp-old.stamp>.2;}),entries_.end());
        auto it=std::find_if(entries_.begin(),entries_.end(),[&](const Entry& old){return old.address==entry.address;});
        if(it!=entries_.end()){*it=entry;return;}
        if(entries_.size()>=512){
            it=std::min_element(entries_.begin(),entries_.end(),[](const Entry& a,const Entry& b){return a.stamp<b.stamp;});
            *it=entry;
        }else entries_.push_back(entry);
    }
    template<class Visit>void visit(double time,Visit&& visit) const {
        for(const auto& entry:entries_)if(time>=entry.stamp&&time-entry.stamp<=.2)visit(entry);
    }
};

struct Matrix {float m[16]{};}; // Unity column-major Matrix4x4
struct Rect {float x=0,y=0,w=1,h=1;};
struct Projection {
    Matrix view{},projection{};Rect rect{};
    bool valid=false;
    bool validate() {
        valid=false;
        for(float x:view.m)if(!std::isfinite(x))return false;
        for(float x:projection.m)if(!std::isfinite(x))return false;
        if(!std::isfinite(rect.x)||!std::isfinite(rect.y)||!std::isfinite(rect.w)||
           !std::isfinite(rect.h)||rect.w<=0||rect.h<=0||rect.w>2||rect.h>2)return false;
        if(std::fabs(projection.m[0])<1e-6f||std::fabs(projection.m[5])<1e-6f)return false;
        valid=true;return true;
    }
    template<class V>bool project(V world,V& out) const {
        if(!valid)return false;
        const float x=view.m[0]*world.x+view.m[4]*world.y+view.m[8]*world.z+view.m[12];
        const float y=view.m[1]*world.x+view.m[5]*world.y+view.m[9]*world.z+view.m[13];
        const float z=view.m[2]*world.x+view.m[6]*world.y+view.m[10]*world.z+view.m[14];
        const float cx=projection.m[0]*x+projection.m[4]*y+projection.m[8]*z+projection.m[12];
        const float cy=projection.m[1]*x+projection.m[5]*y+projection.m[9]*z+projection.m[13];
        const float cw=projection.m[3]*x+projection.m[7]*y+projection.m[11]*z+projection.m[15];
        if(!std::isfinite(cw)||cw<1e-5f||z>=-.01f)return false;
        out={rect.x+(cx/cw*.5f+.5f)*rect.w,rect.y+(cy/cw*.5f+.5f)*rect.h,-z};
        return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z);
    }
};

// Iterate the current managed List<T>, never search heap memory or scene
// objects. Reject incomplete, corrupt or concurrently replaced list storage.
template<class Read,class Visit>
bool visit_list(uintptr_t list,Read&& read,Visit&& visit) {
    uintptr_t items=0,capacity=0;int32_t count=0,version=0;
    if(!list||!read(list+0x10,&items,sizeof(items))||
       !read(list+0x18,&count,sizeof(count))||!read(list+0x1c,&version,sizeof(version))||
       count<0||count>4096)return false;
    if(count==0)return true;
    if(!items||items>UINTPTR_MAX-0x20-uintptr_t(count)*sizeof(uintptr_t)||
       !read(items+0x18,&capacity,sizeof(capacity))||capacity>4096||uintptr_t(count)>capacity)return false;
    std::array<uintptr_t,64> batch{};
    for(size_t first=0;first<size_t(count);first+=batch.size()){
        const size_t n=std::min(batch.size(),size_t(count)-first);
        if(!read(items+0x20+first*sizeof(uintptr_t),batch.data(),n*sizeof(uintptr_t)))return false;
        for(size_t i=0;i<n;++i)if(batch[i])visit(batch[i]);
    }
    uintptr_t after_items=0;int32_t after_count=0,after_version=0;
    return read(list+0x10,&after_items,sizeof(after_items))&&
           read(list+0x18,&after_count,sizeof(after_count))&&
           read(list+0x1c,&after_version,sizeof(after_version))&&
           items==after_items&&count==after_count&&version==after_version;
}
} // namespace mango_esp
