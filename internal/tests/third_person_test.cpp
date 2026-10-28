#include "third_person.h"
#include <cassert>
#include <map>
#include <memory>
#include <string>

namespace {
using namespace mango_camera;
struct Object;
struct Managed {char header[16]{};Object* native=nullptr;int32_t id=0;};
struct Object {
    Managed managed;int32_t transform=0,game=0,renderer=0,parent=0;
    std::vector<int32_t> children;
    V3 position{};Quaternion rotation{};
    bool enabled=true,active=true,offscreen=false,force_off=true,occlusion=true;uint32_t rlm=1;int shadows=1,layer=0,mask=1,culling=2,model=0,animation=0,animator=0;
    std::map<std::string,int> refs;
};
std::map<int32_t,std::unique_ptr<Object>> objects;
Object& make(int32_t id){auto p=std::make_unique<Object>();p->managed.native=p.get();p->managed.id=id;auto* out=p.get();objects[id]=std::move(p);return *out;}
void* find(int32_t id){auto it=objects.find(id);return it==objects.end()?nullptr:&it->second->managed;}
void* returned(int32_t id){const auto p=reinterpret_cast<uintptr_t>(find(id));return reinterpret_cast<void*>(p?p|1:0);}
void* decode(void* p){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)&~uintptr_t(1));}
bool read(uintptr_t p,void* out,size_t size){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),size);return true;}
Object& obj(void* p){assert(p);return *static_cast<Object*>(p);}
int lookups=0;float limit=10;
float collision(void*,V3,V3,float wanted){return std::min(limit,wanted);}
void same(V3 a,V3 b){assert(length(sub(a,b))<.0001f);}
}
int main(){
    mango_chams::Api objects_api;objects_api.loaded=true;objects_api.direct=true;objects_api.find=find;objects_api.decode=decode;objects_api.read=read;
    objects_api.transform=[](void* p){return returned(obj(p).transform);};
    objects_api.game_object=[](void* p){return returned(obj(p).game);};
    objects_api.component=[](void* p,mango_chams::Span* name)->void*{
        ++lookups;const std::u16string s(name->data,name->length);
        if(s==u"SkinnedMeshRenderer")return returned(obj(p).renderer);
        if(s==u"Transform")return returned(obj(p).transform);
        if(s==u"PlayerModelInfo")return returned(obj(p).model);
        if(s==u"CharacterAnimation")return returned(obj(p).animation);
        if(s==u"Animator")return returned(obj(p).animator);
        return nullptr;
    };
    objects_api.child_count=[](void* p){return int(obj(p).children.size());};
    objects_api.child=[](void* p,int i){return returned(obj(p).children.at(i));};
    Object layout;objects_api.force_off_offset=int(reinterpret_cast<char*>(&layout.force_off)-reinterpret_cast<char*>(&layout));
    objects_api.set_force_off=[](void* p,bool value){obj(p).force_off=value;};
    objects_api.occlusion=[](void* p){return obj(p).occlusion;};objects_api.set_occlusion=[](void* p,bool v){obj(p).occlusion=v;};
    objects_api.named_ref=[](void* managed,const char* name)->void*{auto& refs=obj(static_cast<Managed*>(managed)->native).refs;auto it=refs.find(name);return it==refs.end()?nullptr:find(it->second);};
    Api api;api.loaded=true;api.rendering_layer=[](void* p){return obj(p).rlm;};api.set_rendering_layer=[](void* p,uint32_t v){obj(p).rlm=v;};api.objects=&objects_api;api.collision=collision;
    api.position=[](void* p,V3* out){*out=obj(p).position;};api.set_position=[](void* p,V3* v){obj(p).position=*v;};
    api.rotation=[](void* p,Quaternion* out){*out=obj(p).rotation;};
    api.mask=[](void* p){return obj(p).mask;};api.set_mask=[](void* p,int v){obj(p).mask=v;};
    api.enabled=[](void* p){return obj(p).enabled;};api.set_enabled=[](void* p,bool v){obj(p).enabled=v;};
    api.shadows=[](void* p){return obj(p).shadows;};api.set_shadows=[](void* p,int v){obj(p).shadows=v;};
    api.layer=[](void* p){return obj(p).layer;};api.active=[](void* p){return obj(p).active;};
    api.active_self=[](void* p){return obj(p).active;};api.set_active=[](void* p,bool value){obj(p).active=value;};
    api.parent=[](void* p){return returned(obj(p).parent);};
    api.game_transform=objects_api.transform;
    api.behaviour=api.enabled;api.set_behaviour=api.set_enabled;
    api.culling=[](void* p){return obj(p).culling;};api.set_culling=[](void* p,int value){obj(p).culling=value;};
    api.offscreen=[](void* p){return obj(p).offscreen;};api.set_offscreen=[](void* p,bool value){obj(p).offscreen=value;};
    api.is_child=[](void* p,void* parent){
        int id=obj(p).parent;
        for(int i=0;id&&i<16;++i){if(objects[id].get()==parent)return true;id=objects[id]->parent;}
        return false;
    };
    // Separate body and first-person branches under the local player.
    auto& camera=make(1);camera.transform=2;camera.mask=1|(1<<8);const int mask=camera.mask;
    auto& camera_t=make(2);camera_t.position={10,2,30};camera_t.game=3;make(3).transform=2;
    auto& owner=make(10);owner.transform=11;
    auto& root=make(11);root.game=12;root.children={14};make(12).renderer=13;objects[12]->transform=11;
    auto& body=make(13);body.game=12;body.enabled=false;body.shadows=3;body.rlm=1u<<5;objects[12]->layer=11;
    auto& fp_t=make(14);fp_t.game=15;fp_t.parent=11;
    make(15).renderer=16;objects[15]->transform=14;objects[15]->layer=8;auto& arms=make(16);arms.game=15;
    auto& fp=make(20);fp.transform=14;
    Controller controller;const V3 eye=camera_t.position;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,1,1.);
    assert(controller.status==1&&controller.body_meshes==1&&controller.shifted());
    assert(body.enabled&&body.shadows==1&&body.offscreen&&!body.force_off&&!arms.enabled&&camera.mask==(1|(1<<11)));
    assert(!body.occlusion); // baked occlusion culling must not hide the body from the shifted camera
    assert(body.rlm==1); // moved off the first-person-only rendering layer
    const V3 behind=camera_t.position;same(behind,{10.3f,2.15f,27.5f});
    same(controller.undo_ray_origin(behind),eye);same(controller.undo_ray_origin(eye),eye);
    // Repeated callback in one frame cannot accumulate the offset or rescan.
    const int scans=lookups;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,1,1.01);
    same(camera_t.position,behind);assert(lookups==scans);
    controller.begin_frame(api,2);same(camera_t.position,eye);
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,2,1.02);
    same(camera_t.position,behind);assert(lookups==scans);
    // A wall pulls the camera close and restores first-person render flags.
    controller.begin_frame(api,3);limit=.3f;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,3,1.03);
    assert(length(sub(camera_t.position,eye))<.301f);
    same(controller.undo_ray_origin(camera_t.position),eye);same(controller.undo_ray_origin(eye),eye);
    assert(!body.enabled&&body.shadows==3&&!body.offscreen&&body.force_off&&arms.enabled&&camera.mask==mask);
    assert(body.occlusion);
    assert(body.rlm==(1u<<5));
    controller.reset(api);same(camera_t.position,eye);assert(controller.status==0);
    // Game-authored motion must survive reset. Rotation changes the orbit.
    limit=10;camera_t.rotation={0,1,0,0};
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,4,2.);
    same(camera_t.position,{9.7f,2.15f,32.5f});
    camera_t.position={20,4,50};camera.mask=4;
    controller.reset(api);same(camera_t.position,{20,4,50});assert(camera.mask==4);
    // Destroyed renderer IDs may be recycled; never modify their replacement.
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,5,3.);
    auto old=std::move(objects[13]);auto& replacement=make(13);replacement.enabled=true;replacement.shadows=3;
    controller.update(api,nullptr,nullptr,nullptr,nullptr,false,2.5f,6,3.1);
    assert(replacement.enabled&&replacement.shadows==3&&!controller.shifted());
    controller.update(api,find(1),&owner,&fp,nullptr,true,NAN,7,4.);
    assert(!controller.shifted());
    // A hidden full-body model can be detached from the player root. Reveal
    // only its named body/head targets, keep its real Animator and animation
    // behaviour enabled, and preserve their original flags for disable.
    auto& animator=make(30);animator.transform=32;animator.game=31;animator.enabled=false;
    auto& model_go=make(31);model_go.transform=32;model_go.active=false;model_go.model=33;model_go.animation=34;
    auto& model_t=make(32);model_t.game=31;model_t.children={36};
    auto& info=make(33);info.refs={{"body",36},{"skinnedMeshRenderer",37},{"characterAnimation",34}};
    auto& animation=make(34);animation.enabled=false;
    auto& body_go=make(35);body_go.transform=36;body_go.active=false;body_go.renderer=37;
    auto& mesh_t=make(36);mesh_t.game=35;mesh_t.parent=32;
    auto& mesh=make(37);mesh.game=35;mesh.enabled=false;mesh.shadows=3;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,8,5.,find(30));
    assert(model_go.active&&body_go.active&&animator.enabled&&animation.enabled&&animator.culling==0);
    assert(mesh.enabled&&mesh.offscreen&&!mesh.force_off&&controller.body_meshes==1);
    animator.enabled=false;animation.enabled=false;animator.culling=2;
    controller.before_animation(api);
    assert(animator.enabled&&animation.enabled&&animator.culling==0);
    controller.reset(api);
    assert(!model_go.active&&!body_go.active&&!animator.enabled&&!animation.enabled&&animator.culling==2);
    assert(!mesh.enabled&&!mesh.offscreen&&mesh.force_off&&mesh.shadows==3);
    // If PlayerManager has not published its Animator yet, select the real
    // full-body Animator by PlayerModelInfo, not the first-person arms rig.
    model_go.animator=30;root.children.push_back(32);
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,9,6.);
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,10,6.1);
    assert(animator.enabled&&animation.enabled&&body_go.active&&mesh.enabled&&!arms.enabled);
    controller.reset(api);assert(!animator.enabled&&!animation.enabled&&!body_go.active);
    // The first-person layer alone does not make a mesh a viewmodel: a body
    // drawn from layer 8 inside the character model stays visible, a layer-8
    // mesh elsewhere is still a viewmodel, and the layer is only dropped from
    // the camera mask once no visible body mesh uses it.
    {
        auto& cam2=make(100);cam2.transform=101;cam2.mask=1|(1<<8);
        auto& cam2_t=make(101);cam2_t.position={0,2,0};cam2_t.game=102;make(102).transform=101;
        auto& owner2=make(110);owner2.transform=111;
        auto& root2=make(111);root2.game=112;root2.children={113,140};make(112).transform=111;
        auto& body2_t=make(113);body2_t.game=114;body2_t.parent=111;
        auto& body2_go=make(114);body2_go.transform=113;body2_go.layer=8;body2_go.renderer=115;
        auto& body2=make(115);body2.game=114;body2.enabled=false;
        auto& arms2_t=make(140);arms2_t.game=141;arms2_t.parent=111;
        auto& arms2_go=make(141);arms2_go.transform=140;arms2_go.layer=8;arms2_go.renderer=142;
        auto& arms2=make(142);arms2.game=141;arms2.enabled=true;
        auto& model2=make(130);model2.transform=113;
        auto& fp2=make(120);fp2.transform=121;make(121).game=122;make(122).transform=121;
        (void)root2;(void)body2_t;
        limit=10;
        Controller c2;
        c2.update(api,find(100),&owner2,&fp2,nullptr,true,2.5f,1,1.,find(130));
        assert(c2.status==1&&c2.body_meshes==1&&body2.enabled&&!arms2.enabled&&cam2.mask==(1|(1<<8)));
        const char* report=c2.take_report();
        assert(report&&std::string(report).find("body_meshes=1")!=std::string::npos&&!c2.take_report());
        c2.begin_frame(api,2);body2_go.layer=11;
        c2.update(api,find(100),&owner2,&fp2,nullptr,true,2.5f,2,1.1,find(130));
        assert(cam2.mask==(1|(1<<11))&&body2.enabled&&!arms2.enabled);
        c2.reset(api);assert(cam2.mask==(1|(1<<8))&&arms2.enabled);
    }
}
