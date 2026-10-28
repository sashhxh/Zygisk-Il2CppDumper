#include "two_hand_pose.h"
#include <cassert>
#include <random>
#include <map>
using namespace mango_camera;
using namespace mango_grip;
namespace {
struct Managed {char header[16]{};void* native=nullptr;int id=0;};
struct Object{Managed managed;V3 position{};Quaternion rotation{};};
std::map<int,Object> objects;std::map<int,int> roots;int queries=0,writes=0;
void* find(int id){return objects.count(id)?&objects.at(id).managed:nullptr;}
Object& make(int id,V3 p={}){auto& o=objects[id];o.managed.native=&o;o.managed.id=id;o.position=p;return o;}
}
int main(){
 const V3 a{0,0,0},b{.3f,0,0},c{.5f,.1f,0};
 std::mt19937 rng(42);std::uniform_real_distribution<float> d(-.5,.5);
 for(int i=0;i<2000;++i){V3 target{d(rng),d(rng),d(rng)};auto s=solve(a,b,c,target);assert(s.valid);
  V3 elbow=add(a,rotate(s.upper,sub(b,a))),hand=add(elbow,rotate(s.lower,sub(c,b)));
  assert(length(sub(elbow,s.elbow))<.0001f&&length(sub(hand,s.hand))<.0001f);
  assert(std::fabs(length(sub(elbow,a))-length(sub(b,a)))<.0001f);
  assert(std::fabs(length(sub(hand,elbow))-length(sub(c,b)))<.0001f);
  if(length(target)<length(b)+length(sub(c,b))-.001f&&length(target)>std::fabs(length(b)-length(sub(c,b)))+.001f)assert(length(sub(hand,target))<.0001f);
 }
 assert(!solve(a,a,c,b).valid&&!solve(a,b,c,{NAN,0,0}).valid);
 assert(valid(between({1,0,0},{-1,0,0})));
 mango_chams::Api o;o.loaded=true;o.find=find;o.read=[](uintptr_t p,void* out,size_t n){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),n);return true;};
 o.retain=[](int id,uintptr_t){if(id)++roots[id];};o.release=[](int id,uintptr_t){if(id){assert(roots[id]>0);--roots[id];}};
 Api api;api.loaded=true;api.objects=&o;
 api.bone=[](void*,int id)->void*{++queries;return find(id);};
 api.position=[](void* p,V3* v){*v=static_cast<Object*>(p)->position;};api.rotation=[](void* p,Quaternion* q){*q=static_cast<Object*>(p)->rotation;};
 api.set_rotation=[](void* p,Quaternion* q){++writes;static_cast<Object*>(p)->rotation=*q;};
 make(1);make(13,a);make(15,b);make(17,c);auto& pivot=make(2,{.2,.2,.2});
 mango_grip::Controller grip;assert(grip.apply(api,find(1),&pivot,1,0));assert(queries==3&&writes==3);
 for(int i=0;i<100;++i)assert(grip.apply(api,find(1),&pivot,1,0));assert(queries==3&&writes==3);
 grip.begin_frame(api,2);assert(writes==6);for(int id:{13,15,17})assert(std::fabs(objects[id].rotation.w-1)<.0001f);
 assert(grip.apply(api,find(1),&pivot,2,1));objects[15].rotation={0,1,0,0};
 grip.restore(api);assert(objects[15].rotation.y==1); // preserve external changes
 grip.reset(api);for(auto [id,n]:roots)assert(n==0);
 Api cold;grip.reset(cold); // initial session may reset before camera API binds
}
