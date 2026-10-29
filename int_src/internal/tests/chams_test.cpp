#include "chams.h"
#include <cassert>
#include <map>
#include <memory>
#include <string>

namespace {
struct Object;
struct Managed {char header[16]{};Object* native=nullptr;int32_t id=0;};
struct Object {Managed managed;int32_t native_id=0,transform=0,game=0,renderer=0;std::vector<int32_t> children,materials;int flags=0,queue=0;bool unlit=false,occlusion=true,force_off=false,enabled=true;std::map<int,float> floats;std::map<int,mango_chams::Color> tints;};
struct Array {void* klass=nullptr,*monitor=nullptr,*bounds=nullptr;uintptr_t size=0;void* items[32]{};};
std::map<int32_t,std::unique_ptr<Object>> objects;
std::vector<std::unique_ptr<Array>> arrays;
int next_id=1000,clones=0,sets=0,lookups=0,colors=0;bool shader_available=true,compatible_original=false;
std::map<std::u16string,int> properties;
Object& make(int32_t id){auto p=std::make_unique<Object>();p->managed.native=p.get();p->managed.id=id;p->native_id=id;auto* out=p.get();objects[id]=std::move(p);return *out;}
void* find(int32_t id){auto it=objects.find(id);return it==objects.end()?nullptr:&it->second->managed;}
void* returned(int32_t id){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(find(id))|uintptr_t(find(id)?1:0));}
void* decode(void* p){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)&~uintptr_t(1));}
void* game(void* p){auto& o=*static_cast<Object*>(p);return returned(o.game?o.game:o.transform?objects[o.transform]->game:0);}
void* transform(void* p){return returned(static_cast<Object*>(p)->transform);}
void* component(void* p,mango_chams::Span* name){auto text=std::u16string(name->data,name->length);++lookups;if(text==u"SkinnedMeshRenderer"||text==u"UnityEngine.SkinnedMeshRenderer")return returned(static_cast<Object*>(p)->renderer);assert(text==u"MeshRenderer"||text==u"UnityEngine.MeshRenderer");return nullptr;}
int child_count(void* p){return int(static_cast<Object*>(p)->children.size());}
void* child(void* p,int i){return returned(static_cast<Object*>(p)->children.at(i));}
void* clone(void* source){auto& copy=make(++next_id);copy.materials=static_cast<Object*>(source)->materials;copy.unlit=static_cast<Object*>(source)->unlit;++clones;return returned(next_id);}
void* materials(void* renderer){auto a=std::make_unique<Array>();auto& slots=static_cast<Object*>(renderer)->materials;a->size=slots.size();for(size_t i=0;i<slots.size();++i)a->items[i]=find(slots[i]);auto* p=a.get();arrays.push_back(std::move(a));return p;}
void set_materials(void* renderer,void* data,int n){auto* a=static_cast<Array*>(data);assert(a->size==uintptr_t(n));auto& slots=static_cast<Object*>(renderer)->materials;slots.clear();for(int i=0;i<n;++i)slots.push_back(static_cast<Managed*>(a->items[i])->id);++sets;}
void* shader(mango_chams::Span* name){auto text=std::u16string(name->data,name->length);assert(text==u"Universal Render Pipeline/Unlit"||text==u"Universal Render Pipeline/Particles/Unlit"||text==u"UI/Default");return shader_available?returned(30):nullptr;}
int property(mango_chams::Span* name){const std::u16string s(name->data,name->length);auto [it,inserted]=properties.emplace(s,int(properties.size())+1);return it->second;}
void flags(void* p,int flags){assert(static_cast<Object*>(p)->managed.id>1000);static_cast<Object*>(p)->flags=flags;}
void destroy(void* p,float){assert(static_cast<Object*>(p)->managed.id>1000);objects.erase(static_cast<Object*>(p)->managed.id);}
bool read(uintptr_t p,void* out,size_t size){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),size);return true;}
void store(uintptr_t p,void* value){std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value));}
int queries=0;bool xray_ready=true;
void* collect(void* p,void* type,bool typed,bool recursive,bool inactive,bool reverse,void* list){
    assert(type&&typed&&recursive&&!inactive&&!reverse&&!list);++queries;
    auto a=std::make_unique<Array>();std::vector<Object*> pending{static_cast<Object*>(p)};
    while(!pending.empty()){
        auto* go=pending.back();pending.pop_back();if(go->renderer){assert(a->size<32);a->items[a->size++]=find(go->renderer);}
        for(auto& entry:objects)if(entry.second->game==go->native_id)for(int child:entry.second->children){auto* tr=objects[child].get();if(tr->game)pending.push_back(objects[tr->game].get());}
    }
    auto* out=a.get();arrays.push_back(std::move(a));return out;
}
mango_chams::Api api(){
    mango_chams::Api a;a.loaded=true;a.decode=decode;a.find=find;a.read=read;a.store_ref=store;
    a.game_object=game;a.transform=transform;a.component=component;a.child_count=child_count;a.child=child;
    a.clone=clone;a.materials=materials;a.set_materials=set_materials;a.shader=shader;a.property=property;a.set_flags=flags;a.destroy=destroy;
    a.set_shader=[](void* p,void*){assert(static_cast<Object*>(p)->managed.id>1000);static_cast<Object*>(p)->unlit=true;};a.set_float=[](void* p,int id,float value){assert(static_cast<Object*>(p)->managed.id>1000);static_cast<Object*>(p)->floats[id]=value;};
    a.set_color=[](void* p,int property,mango_chams::Color* color){assert(static_cast<Object*>(p)->managed.id>1000);static_cast<Object*>(p)->tints[property]=*color;++colors;};
    a.get_shader=[](void*)->void*{return returned(30);};a.has_property=[](void* p,int){return static_cast<Object*>(p)->unlit||compatible_original;};
    a.set_texture=[](void*,int,void*){};a.set_queue=[](void* p,int value){static_cast<Object*>(p)->queue=value;};a.keyword_on=[](void*,mango_chams::Span*){};a.keyword_off=a.keyword_on;a.set_pass=[](void*,mango_chams::Span*,bool){};
    a.occlusion=[](void* p){return static_cast<Object*>(p)->occlusion;};a.set_occlusion=[](void* p,bool value){static_cast<Object*>(p)->occlusion=value;};
    Object layout;a.force_off_offset=int(reinterpret_cast<char*>(&layout.force_off)-reinterpret_cast<char*>(&layout));a.set_force_off=[](void* p,bool value){static_cast<Object*>(p)->force_off=value;};
    a.renderer_type=reinterpret_cast<void*>(1);a.components=collect;a.renderer_enabled=[](void* p){return static_cast<Object*>(p)->enabled;};a.is_mesh=[](void*){return true;};
    a.xray_shader=[](){return xray_ready?30:0;};a.shader_state=[](){return xray_ready?3:1;};
    return a;
}
}
int main(){
    auto a=api();make(1).transform=2;make(2).game=3;make(3).renderer=4;make(4).materials={10,11};make(10);make(11);make(30);
    // Release players can omit the managed instance-ID cache. The native ID
    // location is supplied by Unity, not guessed from the managed wrapper.
    Object layout;a.native_id_offset=int(reinterpret_cast<char*>(&layout.native_id)-reinterpret_cast<char*>(&layout));
    for(int id:{1,2,3,4})objects[id]->managed.id=0;
    assert(a.returned_id(returned(4))==4&&a.id_failures==0);
    std::vector<mango_chams::Candidate> candidates{{1,0,10,0}};
    // The root is a native component, not its managed wrapper or encoded handle.
    candidates[0].root=reinterpret_cast<uintptr_t>(objects[1].get());
    mango_chams::Options options;options.enabled=true;
    mango_chams::Controller controller;controller.update(a,options,candidates,1.);
    assert(controller.renderer_count==1&&clones==5&&sets==1);
    assert(objects[4]->materials[0]>1000&&objects[4]->materials[1]==objects[4]->materials[0]);
    const int created=clones,changed=sets,painted=colors;const auto allocations=arrays.size();
    controller.update(a,options,candidates,1.11);controller.update(a,options,candidates,1.22);
    assert(clones==created&&sets==changed&&arrays.size()==allocations&&colors==painted);
    candidates[0].visibility=1;controller.update(a,options,candidates,1.33);
    assert(sets==changed+1&&clones==created);
    options.enabled=false;controller.update(a,options,candidates,1.34);
    assert((objects[4]->materials==std::vector<int32_t>{10,11}));
    assert(controller.renderer_count==0&&objects.size()==7);
    // Unloaded originals are restored from the untouched backup, without
    // destroying the backup after it has been adopted by a live renderer.
    options.enabled=true;controller.update(a,options,candidates,2.);
    objects.erase(10);options.enabled=false;controller.update(a,options,candidates,2.01);
    assert(objects[4]->materials[0]>1000&&objects.count(objects[4]->materials[0]));
    assert(objects[4]->materials[1]==11);
    // An unavailable compatible shader must leave the game's materials alone.
    shader_available=false;options.enabled=true;const auto before=objects[4]->materials;
    controller.update(a,options,candidates,3.);assert(objects[4]->materials==before&&controller.renderer_count==0);
    options.enabled=false;controller.update(a,options,candidates,3.01);
    compatible_original=true;options.enabled=true;controller.update(a,options,candidates,4.);
    assert(controller.renderer_count==1&&controller.shader_kind==3);
    options.enabled=false;controller.update(a,options,candidates,4.01);
    assert(objects[4]->materials==before);
    // Large skeletons must not consume the cap before a shallow mesh sibling.
    make(40).transform=41;make(41).game=42;objects[41]->children={43,44};make(42);
    make(43).game=45;make(45).renderer=46;make(46).materials={11};
    make(44).game=47;make(47);int parent=44;
    for(int id=100;id<240;++id){make(id);objects[parent]->children.push_back(id);parent=id;}
    candidates={{40,reinterpret_cast<uintptr_t>(objects[40].get()),5,0}};
    options.enabled=true;controller.update(a,options,candidates,5.);
    assert(controller.renderer_count==1&&objects[46]->materials[0]!=11);
    options.enabled=false;controller.update(a,options,candidates,5.01);
    assert(objects[46]->materials[0]==11);
    shader_available=true;options.through_walls=true;options.enabled=true;
    controller.update(a,options,candidates,6.);
    const int tint=objects[46]->materials[0];assert(controller.shader_kind==5&&tint>1000);
    assert(objects[tint]->queue==4000);
    assert(!objects[46]->occlusion&&!objects[46]->force_off);
    // Equipping armor can replace and resize the array. Retain the Renderer,
    // capture the new originals, and reapply without a two-second rediscovery.
    make(51);make(52);objects[46]->materials={51,52};objects[46]->force_off=true;
    controller.update(a,options,candidates,6.4);
    assert(controller.renderer_count==1&&objects[46]->materials==std::vector<int32_t>({tint,tint})&&objects[46]->force_off);
    options.enabled=false;controller.update(a,options,candidates,6.41);
    assert(objects[46]->materials==std::vector<int32_t>({51,52})&&objects[46]->occlusion&&objects[46]->force_off);
    // Body, helmet, armor, and LODs can exceed the former 12-Renderer cap.
    make(60).transform=61;make(61).game=62;make(62);
    for(int i=0;i<16;++i){const int tr=300+i*3;make(tr).game=tr+1;make(tr+1).renderer=tr+2;make(tr+2).materials={11};objects[61]->children.push_back(tr);}
    candidates={{60,reinterpret_cast<uintptr_t>(objects[60].get()),5,0}};options.enabled=true;
    for(int i=0;i<6;++i)controller.update(a,options,candidates,7.+i*.12);
    assert(controller.renderer_count==16);
    for(int i=0;i<16;++i)assert(objects[302+i*3]->materials[0]>1000&&!objects[302+i*3]->force_off);
    options.enabled=false;controller.update(a,options,candidates,8.);
    for(int i=0;i<16;++i)assert(objects[302+i*3]->materials[0]==11&&!objects[302+i*3]->force_off);
    xray_ready=false;options.enabled=true;const int before_queries=queries;
    for(int i=0;i<200;++i)controller.update(a,options,candidates,9.+i*.1);
    assert(controller.status==2&&queries==before_queries);
    xray_ready=true;
    for(int i=0;i<6;++i)controller.update(a,options,candidates,31.+i*.12);
    assert(controller.renderer_count==16);
    const int shared_tint=objects[302]->materials[0],stable_clones=clones,stable_sets=sets;
    const int base_color=properties.at(u"_BaseColor");
    for(int preset=0;preset<mango_chams::custom_preset;++preset){
        mango_chams::apply_preset(options.appearance,options.alpha,preset);
        controller.update(a,options,candidates,33.+preset*.12);
        assert(clones==stable_clones&&sets==stable_sets);
        assert(objects[302]->materials[0]==shared_tint&&objects[shared_tint]->tints.count(base_color));
        assert(objects[11]->tints.empty());
    }
    options.appearance.style=mango_chams::Flat;options.appearance.rgb[0][0]=.42f;
    controller.update(a,options,candidates,35.);assert(objects[shared_tint]->tints[base_color].r==.42f);
    const int static_colors=colors;controller.update(a,options,candidates,35.12);assert(colors==static_colors);
    options.appearance.style=mango_chams::Rainbow;controller.update(a,options,candidates,35.24);
    const auto rainbow=objects[shared_tint]->tints[base_color];controller.update(a,options,candidates,35.36);
    const auto next=objects[shared_tint]->tints[base_color];assert(rainbow.r!=next.r||rainbow.g!=next.g||rainbow.b!=next.b);
    assert(clones==stable_clones&&sets==stable_sets);
    options.enabled=false;controller.update(a,options,candidates,36.);
    for(int i=0;i<16;++i)assert(objects[302+i*3]->materials[0]==11);
}
