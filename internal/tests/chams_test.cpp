#include "chams.h"
#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace {
struct Object;
struct Managed {char header[16]{};Object* native=nullptr;int32_t id=0;};
struct Object {Managed managed;int32_t native_id=0,transform=0,game=0,renderer=0;std::vector<int32_t> children,materials;int flags=0,queue=0;bool unlit=false,occlusion=true,force_off=true;std::map<int,float> floats;};
struct Array {void* klass=nullptr,*monitor=nullptr,*bounds=nullptr;uintptr_t size=0;void* items[32]{};};
std::map<int32_t,std::unique_ptr<Object>> objects;
std::vector<std::unique_ptr<Array>> arrays;
int next_id=1000,clones=0,sets=0,lookups=0,full_lookups=0,colors=0;double fake_time=0;double fake_clock(){return fake_time+=.0005;}bool shader_available=true,compatible_original=false;
std::set<std::u16string> unavailable;
std::map<std::u16string,int> properties;
Object& make(int32_t id){auto p=std::make_unique<Object>();p->managed.native=p.get();p->managed.id=id;p->native_id=id;auto* out=p.get();objects[id]=std::move(p);return *out;}
void* find(int32_t id){auto it=objects.find(id);return it==objects.end()?nullptr:&it->second->managed;}
void* returned(int32_t id){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(find(id))|uintptr_t(find(id)?1:0));}
void* decode(void* p){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)&~uintptr_t(1));}
void* game(void* p){return returned(static_cast<Object*>(p)->game);}
void* transform(void* p){return returned(static_cast<Object*>(p)->transform);}
void* component(void* p,mango_chams::Span* name){auto text=std::u16string(name->data,name->length);++lookups;if(text.rfind(u"UnityEngine.",0)==0)++full_lookups;if(text==u"SkinnedMeshRenderer"||text==u"UnityEngine.SkinnedMeshRenderer")return returned(static_cast<Object*>(p)->renderer);assert(text==u"MeshRenderer"||text==u"UnityEngine.MeshRenderer");return nullptr;}
int child_count(void* p){return int(static_cast<Object*>(p)->children.size());}
void* child(void* p,int i){return returned(static_cast<Object*>(p)->children.at(i));}
void* clone(void* source){auto& copy=make(++next_id);copy.materials=static_cast<Object*>(source)->materials;copy.unlit=static_cast<Object*>(source)->unlit;++clones;return returned(next_id);}
void* materials(void* renderer){auto a=std::make_unique<Array>();auto& slots=static_cast<Object*>(renderer)->materials;a->size=slots.size();for(size_t i=0;i<slots.size();++i)a->items[i]=find(slots[i]);auto* p=a.get();arrays.push_back(std::move(a));return p;}
void set_materials(void* renderer,void* data,int n){auto* a=static_cast<Array*>(data);assert(a->size==uintptr_t(n));auto& slots=static_cast<Object*>(renderer)->materials;slots.clear();for(int i=0;i<n;++i)slots.push_back(static_cast<Managed*>(a->items[i])->id);++sets;}
void* shader(mango_chams::Span* name){auto text=std::u16string(name->data,name->length);assert(text==u"Universal Render Pipeline/Unlit"||text==u"Universal Render Pipeline/Particles/Unlit"||text==u"UI/Default"||text==u"Hidden/Internal-Colored");return shader_available&&!unavailable.count(text)?returned(30):nullptr;}
int property(mango_chams::Span* name){const std::u16string s(name->data,name->length);auto [it,inserted]=properties.emplace(s,int(properties.size())+1);return it->second;}
void flags(void* p,int flags){assert(static_cast<Object*>(p)->managed.id>1000);static_cast<Object*>(p)->flags=flags;}
void destroy(void* p,float){assert(static_cast<Object*>(p)->managed.id>1000);objects.erase(static_cast<Object*>(p)->managed.id);}
bool read(uintptr_t p,void* out,size_t size){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),size);return true;}
void store(uintptr_t p,void* value){std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value));}
mango_chams::Api api(){
    mango_chams::Api a;a.loaded=true;a.direct=true;a.decode=decode;a.find=find;a.read=read;a.store_ref=store;
    a.game_object=game;a.transform=transform;a.component=component;a.child_count=child_count;a.child=child;
    a.find_shader=shader;a.clone=clone;a.materials=materials;a.set_materials=set_materials;a.shader=shader;a.property=property;a.set_flags=flags;a.destroy=destroy;
    a.set_shader=[](void* p,void*){assert(static_cast<Object*>(p)->managed.id>1000);static_cast<Object*>(p)->unlit=true;};a.set_float=[](void* p,int id,float value){assert(static_cast<Object*>(p)->managed.id>1000);static_cast<Object*>(p)->floats[id]=value;};
    a.set_color=[](void* p,int,mango_chams::Color*){assert(static_cast<Object*>(p)->managed.id>1000);++colors;};
    a.get_shader=[](void*)->void*{return returned(30);};a.has_property=[](void* p,int){return static_cast<Object*>(p)->unlit||compatible_original;};
    a.set_texture=[](void*,int,void*){};a.set_queue=[](void* p,int value){static_cast<Object*>(p)->queue=value;};a.keyword_on=[](void*,mango_chams::Span*){};a.keyword_off=a.keyword_on;a.set_pass=[](void*,mango_chams::Span*,bool){};
    a.occlusion=[](void* p){return static_cast<Object*>(p)->occlusion;};a.set_occlusion=[](void* p,bool value){static_cast<Object*>(p)->occlusion=value;};
    Object layout;a.force_off_offset=int(reinterpret_cast<char*>(&layout.force_off)-reinterpret_cast<char*>(&layout));a.set_force_off=[](void* p,bool value){static_cast<Object*>(p)->force_off=value;};
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
    // Hidden/Internal-Colored declares its depth state as plain properties.
    assert(objects[tint]->floats[properties[u"_ZTest"]]==8&&objects[tint]->floats[properties[u"_ZWrite"]]==0);
    assert(objects[tint]->floats[properties[u"_Cull"]]==0&&objects[tint]->floats[properties[u"_SrcBlend"]]==5&&objects[tint]->floats[properties[u"_DstBlend"]]==10);
    assert(objects[tint]->queue==4000);
    assert(!objects[46]->occlusion&&!objects[46]->force_off);
    // Equipping armor can replace and resize the array. Retain the Renderer,
    // capture the new originals, and reapply without a two-second rediscovery.
    make(51);make(52);objects[46]->materials={51,52};objects[46]->force_off=true;
    controller.update(a,options,candidates,6.8);
    assert(controller.renderer_count==1&&objects[46]->materials==std::vector<int32_t>({tint,tint})&&!objects[46]->force_off);
    options.enabled=false;controller.update(a,options,candidates,6.81);
    assert(objects[46]->materials==std::vector<int32_t>({51,52})&&objects[46]->occlusion&&objects[46]->force_off);
    // Body, helmet, armor, and LODs can exceed the former 12-Renderer cap.
    make(60).transform=61;make(61).game=62;make(62);
    for(int i=0;i<16;++i){const int tr=300+i*3;make(tr).game=tr+1;make(tr+1).renderer=tr+2;make(tr+2).materials={11};objects[61]->children.push_back(tr);}
    candidates={{60,reinterpret_cast<uintptr_t>(objects[60].get()),5,0}};options.enabled=true;
    for(int i=0;i<6;++i)controller.update(a,options,candidates,7.+i*.12);
    assert(controller.renderer_count==16);
    for(int i=0;i<16;++i)assert(objects[302+i*3]->materials[0]>1000&&!objects[302+i*3]->force_off);
    options.enabled=false;controller.update(a,options,candidates,8.);
    for(int i=0;i<16;++i)assert(objects[302+i*3]->materials[0]==11&&objects[302+i*3]->force_off);
    // The first hit settles the type-name spelling; later nodes, and the
    // MeshRenderer lookups of a character that has none, never probe the
    // other spelling again. The two failures on the root node are the only
    // full-name lookups.
    {
        auto b=api();mango_chams::Controller fresh;full_lookups=0;options.enabled=true;
        for(int i=0;i<8;++i)fresh.update(b,options,candidates,20.+i*.12);
        assert(fresh.renderer_count==16&&full_lookups==2);
        options.enabled=false;fresh.update(b,options,candidates,21.5);
    }
    // Without Hidden/Internal-Colored the previous UI shader is still used.
    {
        unavailable.insert(u"Hidden/Internal-Colored");
        auto b=api();mango_chams::Controller fallback;options.enabled=true;
        for(int i=0;i<8;++i)fallback.update(b,options,candidates,30.+i*.12);
        const int tint=objects[302]->materials[0];
        assert(fallback.shader_kind==4&&tint>1000&&objects[tint]->queue==5000);
        assert(objects[tint]->floats[properties[u"unity_GUIZTestMode"]]==8);
        options.enabled=false;fallback.update(b,options,candidates,31.5);
        unavailable.clear();
    }
    // Hierarchy discovery stops at its wall-clock budget and resumes next tick,
    // instead of walking every node in one frame.
    {
        auto b=api();b.clock=fake_clock;mango_chams::Controller sliced;options.enabled=true;
        sliced.update(b,options,candidates,40.);
        assert(sliced.renderer_count<16);
        for(int i=1;i<60&&sliced.renderer_count<16;++i)sliced.update(b,options,candidates,40.+i*.12);
        assert(sliced.renderer_count==16);
        options.enabled=false;sliced.update(b,options,candidates,50.);
    }
}
