#include "shot_target.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>
using namespace mango_aim;
static unsigned char data[4096]{};
static bool read_mem(uintptr_t p,void* out,size_t n){if(p<1||p>sizeof(data)||n>sizeof(data)-p)return false;std::memcpy(out,data+p,n);return true;}
template<class T>void put(uintptr_t p,T value){assert(p+sizeof(T)<=sizeof(data));std::memcpy(data+p,&value,sizeof(value));}
static V3 root_point{0,0,10},bone_point{0,1.7f,10};
static bool living=true,good_position=true;static int requests=0,last_bone=0;
static uintptr_t bone(uintptr_t a,int id){assert(a==1024);++requests;last_bone=id;return 2048;}
static bool position(uintptr_t t,V3& p){assert(t==2048);p=bone_point;return good_position;}
static bool root(uintptr_t a,V3& p){assert(a==128);p=root_point;return true;}
static bool alive(uintptr_t a){assert(a==128);return living;}
int main(){
    const mango_extra::Memory memory{read_mem,nullptr};
    put<uintptr_t>(128,77);put<uintptr_t>(144,700);put<uintptr_t>(128+0x1B0,1024);
    put<uintptr_t>(1024,88);put<uintptr_t>(1040,800);put<uintptr_t>(2064,900);
    Target target{mango_extra::Ref::capture(memory,128),mango_extra::Ref::capture(memory,1024),true};
    const Api api{memory,bone,position,root,alive};V3 p{};
    assert(live_point(api,target,0,p)&&p.y==1.7f&&last_bone==10);
    // Moving and crouching between consecutive bullets must not reuse a point.
    for(int i=0;i<200;++i){root_point.x=i*.01f;bone_point.x=root_point.x;bone_point.y=i%2?1.1f:1.7f;
        assert(live_point(api,target,0,p)&&p.x==bone_point.x&&p.y==bone_point.y);}
    assert(requests==201);assert(live_point(api,target,1,p)&&last_bone==8);
    living=false;int before=requests;assert(!live_point(api,target,0,p)&&requests==before);living=true;
    target.humanoid=false;assert(!live_point(api,target,0,p));target.humanoid=true;
    put<uintptr_t>(1040,801);assert(!live_point(api,target,0,p));put<uintptr_t>(1040,800);
    put<uintptr_t>(128+0x1B0,1025);assert(!live_point(api,target,0,p));put<uintptr_t>(128+0x1B0,1024);
    put<uintptr_t>(2064,0);assert(!live_point(api,target,0,p));put<uintptr_t>(2064,900);
    good_position=false;assert(!live_point(api,target,0,p));good_position=true;
    bone_point.x=std::numeric_limits<float>::quiet_NaN();assert(!live_point(api,target,0,p));
    bone_point={root_point.x,20,10};assert(!live_point(api,target,0,p));
    uintptr_t scope=0;
    {DispersionScope shot(scope,500);assert(suppress_spread(scope,500)&&!suppress_spread(scope,501));
        {DispersionScope nested(scope,600);assert(suppress_spread(scope,600)&&!suppress_spread(scope,500));}
        assert(suppress_spread(scope,500));
        {DispersionScope unrelated(scope,0);assert(!suppress_spread(scope,500));}
        assert(suppress_spread(scope,500));}
    assert(scope==0&&!suppress_spread(0,0));
    std::puts("live head/body, moving/crouching targets, destroyed rigs, no fallback, scoped dispersion: PASS");
}
