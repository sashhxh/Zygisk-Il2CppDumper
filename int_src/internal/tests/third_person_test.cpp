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
    std::vector<int32_t> children,batch;
    V3 position{};Quaternion rotation{};
    bool enabled=true,active=true,offscreen=false,force_off=true;int shadows=1,layer=0,mask=1,culling=2,model=0,animation=0,animator=0;
    std::map<std::string,int> refs;
    bool manual_view=false,manual_cull=false;uint8_t view_auto=1,cull_auto=1;Matrix view{},cull{};
    uint32_t render_layers=1;bool occlusion=true;
};
std::map<int32_t,std::unique_ptr<Object>> objects;
std::map<int32_t,int> rooted;
Object& make(int32_t id){auto p=std::make_unique<Object>();p->managed.native=p.get();p->managed.id=id;auto* out=p.get();objects[id]=std::move(p);return *out;}
void* find(int32_t id){auto it=objects.find(id);return it==objects.end()?nullptr:&it->second->managed;}
void* returned(int32_t id){const auto p=reinterpret_cast<uintptr_t>(find(id));return reinterpret_cast<void*>(p?p|1:0);}
void* decode(void* p){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)&~uintptr_t(1));}
bool read(uintptr_t p,void* out,size_t size){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),size);return true;}
Object& obj(void* p){assert(p);return *static_cast<Object*>(p);}
struct Array {void* klass=nullptr,*monitor=nullptr,*bounds=nullptr;uintptr_t length=0;void* items[64]{};};
std::vector<std::unique_ptr<Array>> arrays;int queries=0;
void* collect(void* go,void* type,bool typed,bool recursive,bool inactive,bool reverse,void* list){
 assert(type&&typed&&recursive&&!inactive&&!reverse&&!list);++queries;
 auto a=std::make_unique<Array>();a->length=obj(go).batch.size();
 for(size_t i=0;i<a->length;++i)a->items[i]=find(obj(go).batch[i]);
 auto p=a.get();arrays.push_back(std::move(a));return p;
}
int lookups=0;float limit=10;
float collision(void*,V3,V3,float wanted){return std::min(limit,wanted);}
void same(V3 a,V3 b){assert(length(sub(a,b))<.0001f);}
}
int main(){
    mango_chams::Api objects_api;objects_api.loaded=true;objects_api.find=find;objects_api.decode=decode;objects_api.read=read;
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
    objects_api.named_ref=[](void* managed,const char* name)->void*{auto& refs=obj(static_cast<Managed*>(managed)->native).refs;auto it=refs.find(name);return it==refs.end()?nullptr:find(it->second);};
    objects_api.renderer_type=reinterpret_cast<void*>(1);
    objects_api.components=collect;
    objects_api.is_mesh=[](void*){return true;};
    objects_api.retain=[](int32_t id,uintptr_t){if(id)++rooted[id];};
    objects_api.release=[](int32_t id,uintptr_t){if(id){assert(rooted[id]>0);--rooted[id];}};
    Api api;api.loaded=true;api.objects=&objects_api;api.collision=collision;
    api.position=[](void* p,V3* out){*out=obj(p).position;};api.set_position=[](void* p,V3* v){obj(p).position=*v;};
    api.rotation=[](void* p,Quaternion* out){*out=obj(p).rotation;};
    api.mask=[](void* p){return obj(p).mask;};api.set_mask=[](void* p,int v){obj(p).mask=v;};
    api.enabled=[](void* p){return obj(p).enabled;};api.set_enabled=[](void* p,bool v){obj(p).enabled=v;};
    api.shadows=[](void* p){return obj(p).shadows;};api.set_shadows=[](void* p,int v){obj(p).shadows=v;};
    api.render_layers=[](void* p){return obj(p).render_layers;};api.set_render_layers=[](void* p,uint32_t v){obj(p).render_layers=v;};
    objects_api.occlusion=[](void* p){return obj(p).occlusion;};objects_api.set_occlusion=[](void* p,bool v){obj(p).occlusion=v;};
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
    auto& camera=make(1);camera.transform=2;camera.mask=1|(1<<8);const int original_mask=camera.mask;
    auto& camera_t=make(2);camera_t.position={10,2,30};camera_t.game=3;make(3).transform=2;
    auto& owner=make(10);owner.transform=11;owner.game=12;auto& root=make(11);root.game=12;make(12).transform=11;
    auto& fp=make(20);fp.transform=14;fp.game=15;auto& fp_t=make(14);fp_t.game=15;fp_t.parent=11;
    make(15).transform=14;objects[15]->batch={16};auto& arms=make(16);arms.game=15;objects[15]->layer=8;
    objects[12]->batch={16};
    auto& animator=make(30);animator.transform=32;animator.game=31;animator.enabled=false;
    auto& model_go=make(31);model_go.transform=32;model_go.active=true;model_go.batch={37,41};
    auto& model_t=make(32);model_t.game=31;model_t.parent=11;
    auto& info=make(33);info.refs={{"body",36},{"skinnedMeshRenderer",37},{"characterAnimation",34}};
    auto& animation=make(34);animation.enabled=false;
    auto& body_go=make(35);body_go.transform=36;body_go.active=false;body_go.layer=11;
    auto& mesh_t=make(36);mesh_t.game=35;mesh_t.parent=32;
    auto& mesh=make(37);mesh.game=35;mesh.enabled=false;mesh.shadows=3;mesh.render_layers=4|32;
    auto& hidden_lod=make(41);hidden_lod.game=35;hidden_lod.enabled=false;
    Controller controller;const V3 eye=camera_t.position;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,1,1.);
    assert(controller.status==-3&&!controller.shifted()&&queries==0);
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,2,1.1,find(30),find(33));
    assert(controller.status==1&&controller.body_meshes==1&&body_go.active&&mesh.enabled&&!mesh.force_off);
    assert(mesh.shadows==1&&!mesh.occlusion);
    assert(animator.enabled&&animation.enabled&&animator.culling==2&&!mesh.offscreen);
    assert(!hidden_lod.enabled&&hidden_lod.force_off&&!arms.enabled&&camera.mask==(original_mask|(1<<11)));
    same(camera_t.position,{10.3f,2.15f,27.5f});same(controller.undo_ray_origin(camera_t.position),eye);
    const int initial_queries=queries;
    for(int frame=3;frame<103;++frame)controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,frame,1.1+(frame-2)/60.,find(30),find(33));
    same(camera_t.position,{10.3f,2.15f,27.5f});assert(queries==initial_queries&&lookups==0);
    limit=.3f;controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,103,2.9,find(30),find(33));
    assert(length(sub(camera_t.position,eye))<.301f&&!mesh.enabled&&mesh.force_off&&!body_go.active&&arms.enabled&&camera.mask==original_mask);
    assert(mesh.shadows==3&&mesh.occlusion&&mesh.render_layers==(4|32));
    controller.reset(api);same(camera_t.position,eye);assert(!animator.enabled&&!animation.enabled);
    limit=10;controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,104,3.,find(30),find(33));
    camera_t.position={20,4,50};camera.mask=4;controller.reset(api);same(camera_t.position,{20,4,50});assert(camera.mask==4);
    controller.update(api,find(1),&owner,&fp,nullptr,true,NAN,105,4.,find(30),find(33));assert(!controller.shifted());
    // A game may provide custom view/culling matrices. Changing Transform
    // alone then moves the logical camera but never changes the rendered view.
    camera.manual_view=camera.manual_cull=true;
    camera.view_auto=camera.cull_auto=0;
    api.view_auto_offset=int(reinterpret_cast<char*>(&camera.view_auto)-reinterpret_cast<char*>(&camera));
    api.cull_auto_offset=int(reinterpret_cast<char*>(&camera.cull_auto)-reinterpret_cast<char*>(&camera));
    camera.view.v[0]=camera.view.v[5]=camera.view.v[10]=camera.view.v[15]=1;
    camera.view.v[12]=-camera_t.position.x;camera.view.v[13]=-camera_t.position.y;camera.view.v[14]=-camera_t.position.z;
    camera.cull=camera.view;camera.cull.v[0]=2;const Matrix original_view=camera.view,original_cull=camera.cull;
    api.view=[](void* p,Matrix* out){*out=obj(p).view;};api.set_view=[](void* p,Matrix* in){assert(obj(p).manual_view);obj(p).view=*in;};
    api.cull_matrix=[](void* p,Matrix* out){*out=obj(p).cull;};api.set_cull_matrix=[](void* p,Matrix* in){assert(obj(p).manual_cull);obj(p).cull=*in;};
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,106,5.,find(30),find(33));
    assert(!near(camera.view,original_view)&&near(camera.view,translate_view(original_view,controller.offset())));
    assert(near(camera.cull,translate_view(original_cull,controller.offset())));
    controller.begin_frame(api,107);assert(near(camera.view,original_view)&&near(camera.cull,original_cull));
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,107,5.1,find(30),find(33));
    camera.view.v[12]=333;camera.cull.v[12]=444;controller.reset(api);
    assert(camera.view.v[12]==333&&camera.cull.v[12]==444);
    camera.view_auto=camera.cull_auto=1;camera.manual_view=camera.manual_cull=false;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,108,6.,find(30),find(33));controller.reset(api);
    assert(camera.view_auto==1&&camera.cull_auto==1&&camera.view.v[12]==333&&camera.cull.v[12]==444);
    Matrix invalid{};invalid.v[0]=NAN;assert(!near(invalid,invalid));
    // Live local body and armor are masked out by the first-person render pass.
    // The pass is disabled while showing the body, and only its reserved bits
    // are removed. Equipment also needs ShadowsOnly -> On, not TwoSided -> On.
    auto& pass=make(50);pass.enabled=true;
    auto& armor=make(51);armor.game=35;armor.shadows=3;armor.force_off=false;armor.render_layers=4|64;
    model_go.batch={37,41,51};mesh.shadows=3;mesh.render_layers=4|32;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,109,7.,find(30),find(33),find(50),4|8);
    assert(!pass.enabled&&mesh.shadows==1&&armor.shadows==1);
    assert(mesh.render_layers==(1|32)&&armor.render_layers==(1|64)&&!armor.occlusion);
    const int cached_queries=queries;
    // Simulate a game callback hiding the live model just before rendering.
    mesh.shadows=3;mesh.enabled=false;mesh.render_layers=4|32;body_go.active=false;
    controller.before_cull(api,find(3));assert(!mesh.enabled); // unrelated camera
    controller.before_cull(api,find(1));assert(mesh.enabled&&mesh.shadows==1&&body_go.active&&mesh.render_layers==(1|32));
    assert(queries==cached_queries&&!hidden_lod.enabled);
    controller.reset(api);assert(pass.enabled&&mesh.shadows==3&&armor.shadows==3&&mesh.render_layers==(4|32)&&armor.render_layers==(4|64)&&armor.occlusion);
    // A two-sided renderer was already visible; preserve that mode exactly.
    mesh.shadows=2;pass.enabled=false;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,110,8.,find(30),find(33),find(50),4|8);
    assert(mesh.shadows==2&&!pass.enabled);controller.reset(api);assert(mesh.shadows==2&&!pass.enabled);
    // An explicitly bound body on layer 8 is not a view-model arm mesh.
    body_go.layer=8;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,111,9.,find(30),find(33),find(50),4|8);
    assert(mesh.enabled&&!arms.enabled&&(camera.mask&(1<<8)));controller.reset(api);
    for(auto [id,count]:rooted)assert(count==0);
    body_go.layer=0;
    // A respawn can replace PlayerModelInfo without replacing the Animator.
    auto& second_info=make(60);second_info.refs={{"body",36},{"skinnedMeshRenderer",61},{"characterAnimation",34}};
    auto& second_mesh=make(61);second_mesh.game=35;second_mesh.enabled=false;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,112,10.,find(30),find(33),find(50),12);
    assert(mesh.enabled);
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,113,10.1,find(30),find(60),find(50),12);
    assert(!mesh.enabled&&second_mesh.enabled);
    controller.reset(api);assert(!second_mesh.enabled);for(auto [id,count]:rooted)assert(count==0);
    // Async held-weapon completion invalidates the mesh cache immediately,
    // including world-pass flags. Subsequent frames keep the cache bounded.
    auto& gun=make(70);gun.game=35;gun.force_off=false;gun.shadows=3;gun.render_layers=4|128;
    model_go.batch={37,41,51};
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,114,11.,find(30),find(33),find(50),4|8,false,0);
    int before_weapon_queries=queries;
    model_go.batch.push_back(70);
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,115,11.1,find(30),find(33),find(50),4|8,false,70);
    assert(queries>before_weapon_queries&&gun.shadows==1&&gun.render_layers==(1|128)&&!gun.occlusion);
    before_weapon_queries=queries;
    for(int i=0;i<60;++i)controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,116+i,11.11+i/60.,find(30),find(33),find(50),4|8,false,70);
    assert(queries==before_weapon_queries);
    // Swapping the cosmetic body keeps the real weapon visible. Reclassify
    // its branch once; no live hierarchy scan occurs during toggle/culling.
    auto& weapon_holder=make(80);weapon_holder.transform=81;auto& weapon_t=make(81);weapon_t.parent=32;
    auto& weapon_child=make(82);weapon_child.parent=81;weapon_child.game=83;
    auto& weapon_go=make(83);weapon_go.transform=82;
    gun.game=83;gun.transform=82;info.refs["rightWeaponHolder"]=81;
    controller.reset(api);
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,180,13.,find(30),find(33),find(50),4|8,false,70);
    before_weapon_queries=queries;
    controller.replace_body(api,true);assert(!mesh.enabled&&!armor.enabled&&gun.enabled&&!arms.enabled);
    controller.before_cull(api,find(1));assert(!mesh.enabled&&gun.enabled&&queries==before_weapon_queries);
    controller.replace_body(api,false);assert(mesh.enabled&&armor.enabled&&gun.enabled&&!arms.enabled);
    controller.replace_body(api,true);
    controller.reset(api);assert(gun.shadows==3&&gun.render_layers==(4|128)&&gun.occlusion);
    // An avatar uses Default layer even when the game's body uses Entity.
    body_go.layer=weapon_go.layer=11;camera.mask=(1<<11)|(1<<8);
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,181,14.,find(30),find(33),find(50),4|8,false,70);
    assert(!(camera.mask&1));controller.replace_body(api,true);assert(camera.mask&1);
    controller.replace_body(api,false);assert(!(camera.mask&1));
    controller.reset(api);assert(camera.mask==((1<<11)|(1<<8)));
    // Scene layers and game changes survive body/avatar toggles and shutdown.
    // Only the added local-body/default bits belong to the camera controller.
    body_go.layer=weapon_go.layer=11;
    camera.mask=(1<<8)|(1<<12)|(1<<14);const int world_mask=camera.mask;
    controller.update(api,find(1),&owner,&fp,nullptr,true,2.5f,182,15.,find(30),find(33),find(50),4|8,false,70);
    assert((camera.mask&world_mask)==world_mask&&!arms.enabled);
    controller.replace_body(api,true);assert((camera.mask&world_mask)==world_mask&&(camera.mask&1));
    camera.mask|=1<<15; // The game enables terrain while the visual is active.
    controller.replace_body(api,false);assert((camera.mask&world_mask)==world_mask&&(camera.mask&(1<<15))&&!(camera.mask&1));
    controller.reset(api);assert(camera.mask==(world_mask|(1<<15)));
    for(auto [id,count]:rooted)assert(count==0);
}
