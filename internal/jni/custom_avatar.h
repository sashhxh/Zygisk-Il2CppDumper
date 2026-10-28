#pragma once
#include "third_person.h"
#include "model/avatar_bindings.h"
namespace mango_avatar {
using V3=mango_camera::V3;using Quaternion=mango_camera::Quaternion;
inline Quaternion multiply(Quaternion a,Quaternion b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline bool valid(Quaternion q){float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return std::isfinite(n)&&n>.5f&&n<1.5f;}
struct Api {
    mango_camera::Api* camera=nullptr;
    void(*set_rotation)(void*,Quaternion*)=nullptr;
    bool load(mango_chams::Api::Resolve,mango_camera::Api& c){
        camera=&c;set_rotation=c.set_rotation;
        return c.loaded&&c.objects&&c.objects->child&&c.objects->child_count&&set_rotation;
    }
};
class Controller {
    struct Ref {int32_t id=0;uintptr_t native=0;};
    struct Bone {Ref source,target;Quaternion correction;bool required=false;};
    struct Node {uintptr_t parent;unsigned index;Ref value;};
    std::vector<Bone> bones_;std::vector<Ref> meshes_;
    Ref root_{},transform_{},animator_{};
    unsigned attempts_=0;double retry_=0,next_clone_=0;bool bound_=false;
    Ref ref(Api& a,void* m){auto& o=*a.camera->objects;return {o.id(m),reinterpret_cast<uintptr_t>(o.native(m))};}
    void hold(Api& a,Ref r){auto& o=*a.camera->objects;if(o.retain)o.retain(r.id,r.native);}
    void drop(Api& a,Ref r){auto& o=*a.camera->objects;if(o.release)o.release(r.id,r.native);}
    void* live(Api& a,Ref r){return a.camera->objects->live_native(r.id,r.native);}
    void clear(Api& a){for(auto b:bones_){drop(a,b.source);drop(a,b.target);}for(auto r:meshes_)drop(a,r);bones_.clear();meshes_.clear();bound_=false;human_count=target_count=mesh_count=0;}
    Ref walk(Api& a,unsigned depth,const unsigned char* steps,std::vector<Node>& nodes){
        auto& o=*a.camera->objects;Ref current=transform_;
        if(!depth||depth>32)return {};
        for(unsigned i=0;i<depth;++i){
            void* p=live(a,current);if(!p)return {};
            auto it=std::find_if(nodes.begin(),nodes.end(),[&](const Node& n){return n.parent==current.native&&n.index==steps[i];});
            if(it!=nodes.end()){current=it->value;continue;}
            const int count=o.child_count(p);if(count<0||count>512||steps[i]>=count)return {};
            Ref next=ref(a,o.decode(o.child(p,steps[i])));if(!next.id||!next.native)return {};
            hold(a,next);nodes.push_back({current.native,steps[i],next});current=next;
        }
        return current;
    }
    bool bind(Api& a,void* animator,double time){
        clear(a);++attempts_;retry_=time+std::min(4u,attempts_);
        auto& c=*a.camera;auto& o=*c.objects;std::vector<Node> nodes;
        struct Release {Controller* self;Api& api;std::vector<Node>& nodes;~Release(){for(auto n:nodes)self->drop(api,n.value);}} release{this,a,nodes};
        unsigned source_required=0,target_required=0;
        for(auto b:bindings){
            void* source=c.bone?c.bone(animator,b.human):nullptr;
            const bool required=b.human==0||b.human==10||b.human==17||b.human==18;
            if(!o.native(source))continue;++human_count;if(required)++source_required;
            Ref target=walk(a,b.depth,b.steps,nodes);if(!target.id)continue;
            Ref s=ref(a,source);if(!s.id)continue;
            hold(a,s);hold(a,target);bones_.push_back({s,target,{b.correction[0],b.correction[1],b.correction[2],b.correction[3]},required});
            ++target_count;if(required)++target_required;
        }
        for(auto b:mesh_bindings){
            Ref tr=walk(a,b.depth,b.steps,nodes);void* p=live(a,tr);if(!p)continue;
            void* go=o.native_return(o.game_object(p));if(!go)continue;
            auto type=mango_chams::span(u"SkinnedMeshRenderer");Ref renderer=ref(a,o.decode(o.component(go,&type)));if(!renderer.id)continue;
            hold(a,renderer);meshes_.push_back(renderer);++mesh_count;
        }
        bound_=source_required==4&&target_required==4&&mesh_count==sizeof(mesh_bindings)/sizeof(mesh_bindings[0]);
        status=source_required!=4?4:target_required!=4?5:!bound_?-3:3;
        return bound_;
    }
public:
    int status=0;bool visible=false;unsigned human_count=0,target_count=0,mesh_count=0;
    void hide(Api& a){if(a.camera&&a.camera->loaded)if(void* p=live(a,root_))if(a.camera->active_self(p))a.camera->set_active(p,false);visible=false;}
    void suspend(Api& a){hide(a);if(!bound_){attempts_=0;retry_=next_clone_=0;}status=0;}
    void reset(Api& a){
        if(a.camera&&a.camera->loaded){hide(a);clear(a);if(void* p=live(a,root_))a.camera->objects->destroy(p,0.f);drop(a,root_);drop(a,transform_);drop(a,animator_);}
        root_=transform_=animator_={};attempts_=0;retry_=next_clone_=0;bound_=false;status=0;
    }
    bool pose(Api& a){
        auto& c=*a.camera;auto& o=*c.objects;void* root=live(a,root_),*t=live(a,transform_),*animator=live(a,animator_);
        if(!bound_||!root||!t||!animator){hide(a);status=1;return false;}
        for(auto b:bones_)if(b.required&&(!live(a,b.source)||!live(a,b.target))){hide(a);bound_=false;status=4;return false;}
        for(auto r:meshes_)if(!live(a,r)){hide(a);bound_=false;status=-3;return false;}
        void* source_root=o.native_return(o.transform(animator));if(!source_root){hide(a);status=-4;return false;}
        V3 p{};Quaternion q{};c.position(source_root,&p);c.rotation(source_root,&q);
        if(!mango_camera::finite(p)||!valid(q)){hide(a);status=-4;return false;}
        c.set_position(t,&p);a.set_rotation(t,&q);
        for(auto b:bones_){void* s=live(a,b.source),*target=live(a,b.target);if(!s||!target)continue;c.position(s,&p);c.rotation(s,&q);if(!mango_camera::finite(p)||!valid(q))continue;Quaternion result=multiply(q,b.correction);c.set_position(target,&p);a.set_rotation(target,&result);}
        for(auto r:meshes_)if(void* renderer=live(a,r)){
            if(!c.enabled(renderer))c.set_enabled(renderer,true);
            if(o.force_off_offset>=0&&o.field<uint8_t>(renderer,size_t(o.force_off_offset)))o.set_force_off(renderer,false);
            if(c.render_layers&&c.set_render_layers&&c.render_layers(renderer)!=1)c.set_render_layers(renderer,1);
        }
        if(!c.active_self(root))c.set_active(root,true);visible=true;status=3;return true;
    }
    bool update(Api& a,void* animator,int32_t prefab,bool enabled,bool body_visible,double time){
        if(!a.camera||!a.set_rotation){status=1;return false;}
        if(!enabled){suspend(a);return false;}
        if(!body_visible){hide(a);status=1;return false;}
        if(!root_.id&&time<next_clone_){status=-1;return false;}
        auto& c=*a.camera;auto& o=*c.objects;auto native=reinterpret_cast<uintptr_t>(o.native(animator));
        if(animator_.native!=native||(root_.id&&!live(a,root_)))reset(a);
        if(!native||!prefab){hide(a);status=1;return false;}
        if(!root_.id){
            if(time<next_clone_){status=-1;return false;}next_clone_=time+2;
            void* original=o.live(prefab);if(!original){status=1;return false;}
            root_=ref(a,o.decode(o.clone(original)));if(!root_.id||!root_.native){if(root_.native)o.destroy(reinterpret_cast<void*>(root_.native),0.f);root_={};status=-1;return false;}
            hold(a,root_);o.set_flags(reinterpret_cast<void*>(root_.native),32);
            transform_=ref(a,o.decode(c.game_transform(reinterpret_cast<void*>(root_.native))));animator_=ref(a,animator);hold(a,transform_);hold(a,animator_);hide(a);
            if(!transform_.id){reset(a);next_clone_=time+2;status=-2;return false;}
        }
        if(!bound_&&time>=retry_){status=2;bind(a,animator,time);}
        if(!bound_){hide(a);return false;}
        return pose(a);
    }
};
}
