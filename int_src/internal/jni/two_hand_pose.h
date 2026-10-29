#pragma once
#include "third_person.h"
namespace mango_grip {
using namespace mango_camera;
inline float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Quaternion mul(Quaternion a,Quaternion b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline bool valid(Quaternion q){float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return std::isfinite(n)&&n>.5f&&n<1.5f;}
inline Quaternion between(V3 a,V3 b){
 float la=length(a),lb=length(b);if(la<1e-6f||lb<1e-6f)return {};
 a=scale(a,1/la);b=scale(b,1/lb);float d=std::clamp(dot(a,b),-1.f,1.f);
 V3 c=cross(a,b);float w=1+d,n=std::sqrt(dot(c,c)+w*w);
 if(n<1e-6f){V3 axis=cross(a,{1,0,0});if(length(axis)<.01f)axis=cross(a,{0,1,0});axis=scale(axis,1/length(axis));return {axis.x,axis.y,axis.z,0};}
 return {c.x/n,c.y/n,c.z/n,w/n};
}
struct Solution{Quaternion upper{},lower{};V3 elbow{},hand{};bool valid=false;};
inline Solution solve(V3 a,V3 b,V3 c,V3 target){
 Solution out;if(!finite(a)||!finite(b)||!finite(c)||!finite(target))return out;
 float l1=length(sub(b,a)),l2=length(sub(c,b)),distance=length(sub(target,a));
 if(l1<.03f||l2<.03f||l1>1||l2>1||distance<1e-5f)return out;
 V3 direction=scale(sub(target,a),1/distance);float d=std::clamp(distance,std::fabs(l1-l2)+.0001f,l1+l2-.0001f);
 V3 bend=sub(sub(b,a),scale(direction,dot(sub(b,a),direction)));
 if(length(bend)<.0001f){bend=cross(direction,{0,1,0});if(length(bend)<.0001f)bend=cross(direction,{1,0,0});}
 bend=scale(bend,1/length(bend));float x=(l1*l1-l2*l2+d*d)/(2*d),y=std::sqrt(std::max(0.f,l1*l1-x*x));
 out.elbow=add(a,add(scale(direction,x),scale(bend,y)));out.hand=add(a,scale(direction,d));
 out.upper=between(sub(b,a),sub(out.elbow,a));out.lower=between(sub(c,b),sub(out.hand,out.elbow));out.valid=valid(out.upper)&&valid(out.lower);return out;
}
// A late two-bone correction also works when a controller has no Animator IK
// pass. It changes only the selected local arm, and restores owned rotations
// before the next game animation tick. No bone translations or scales change.
class Controller{
 struct Ref{int32_t id=0;uintptr_t native=0;};
 Ref animator_{},refs_[3]{};Quaternion before_[3]{},applied_[3]{};
 bool owned_=false,bound_=false;int frame_=-1;double retry_=0;unsigned attempts_=0;
 void* live(Api& a,Ref r){return a.objects->live_native(r.id,r.native);}
 void drop(Api& a,Ref r){if(a.objects&&a.objects->release)a.objects->release(r.id,r.native);}
 Ref ref(Api& a,void* managed){Ref r{a.objects->id(managed),reinterpret_cast<uintptr_t>(a.objects->native(managed))};if(a.objects->retain)a.objects->retain(r.id,r.native);return r;}
 static bool same(Quaternion a,Quaternion b){return std::fabs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w)>.99999f;}
public:
 void restore(Api& a){
  if(owned_&&a.loaded&&a.set_rotation){
   void* targets[3]{};Quaternion current[3]{};bool owned[3]{};
   // Capture ownership before rotating a parent, which also changes its
   // children's world rotations. Then restore parent first.
   for(int i=0;i<3;++i)if(void* p=live(a,refs_[i])){targets[i]=p;a.rotation(p,&current[i]);owned[i]=same(current[i],applied_[i]);}
   bool parent_restored=false;
   for(int i=0;i<3;++i)if(targets[i]){
    if(owned[i]){a.set_rotation(targets[i],&before_[i]);parent_restored=true;}
    else if(parent_restored)a.set_rotation(targets[i],&current[i]);
   }
  }
  owned_=false;
 }
 void begin_frame(Api& a,int frame){if(frame!=frame_)restore(a);}
 void reset(Api& a){restore(a);for(auto r:refs_)drop(a,r);drop(a,animator_);animator_={};for(auto& r:refs_)r={};bound_=false;attempts_=0;retry_=0;frame_=-1;}
 bool apply(Api& a,void* managed_animator,void* pivot,int frame,double time){
  if(!a.loaded||!a.bone||!a.set_rotation||!pivot)return false;
  auto native=reinterpret_cast<uintptr_t>(a.objects->native(managed_animator));
  if(native!=animator_.native){reset(a);animator_=ref(a,managed_animator);}
  if(!native)return false;
  if(!bound_&&time>=retry_&&attempts_<3){
   for(auto& r:refs_){drop(a,r);r={};}++attempts_;retry_=time+1;
   const int ids[]={13,15,17};bound_=true;for(int i=0;i<3;++i){refs_[i]=ref(a,a.bone(managed_animator,ids[i]));if(!refs_[i].native)bound_=false;}
  }
  if(!bound_)return false;if(owned_&&frame==frame_)return true;
  void* p[3]{};V3 positions[3]{},goal{};Quaternion target{};
  for(int i=0;i<3;++i){p[i]=live(a,refs_[i]);if(!p[i])return false;a.position(p[i],&positions[i]);a.rotation(p[i],&before_[i]);if(!valid(before_[i]))return false;}
  a.position(pivot,&goal);a.rotation(pivot,&target);if(!finite(goal)||!valid(target))return false;
  if(length(sub(goal,positions[2]))<.015f)return true;
  Solution s=solve(positions[0],positions[1],positions[2],goal);if(!s.valid)return false;
  applied_[0]=mul(s.upper,before_[0]);applied_[1]=mul(s.lower,before_[1]);applied_[2]=target;
  for(int i=0;i<3;++i)a.set_rotation(p[i],&applied_[i]);owned_=true;frame_=frame;return true;
 }
};
}
