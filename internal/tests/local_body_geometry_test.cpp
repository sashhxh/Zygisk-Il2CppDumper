#include "third_person.h"
#include <cassert>
#include <map>
#include <memory>
#include <string>
namespace {
using namespace mango_camera;
struct Object {
    std::array<unsigned char,0x100> managed{};int id=0,parent=0,game=0,transform=0,component=0;
    V3 scale{1,1,1},position{};bool enabled=true;
};
std::map<int,std::unique_ptr<Object>> objects;std::map<int,int> roots;std::map<int,int> humans;
void ptr(void* p,size_t offset,void* value){std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value));}
Object& make(int id,int parent=0){auto o=std::make_unique<Object>();o->id=id;o->parent=parent;Object* p=o.get();ptr(o->managed.data(),0x10,p);std::memcpy(o->managed.data()+0x18,&id,sizeof(id));objects[id]=std::move(o);return *p;}
void* find(int id){auto it=objects.find(id);return it==objects.end()?nullptr:it->second->managed.data();}
void* returned(int id){return find(id);}
Object& native(void* p){assert(p);return *static_cast<Object*>(p);}
bool read(uintptr_t p,void* out,size_t size){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),size);return true;}
int lookups=0,bone_calls=0,scale_writes=0;
void equal(V3 a,V3 b){assert(length(sub(a,b))<.00001f);}
struct Array {void* klass=nullptr,*monitor=nullptr,*bounds=nullptr;uintptr_t count=0;std::array<unsigned char,512> data{};};
void vector(Array& a,unsigned i,V3 v){std::memcpy(a.data.data()+i*sizeof(V3),&v,sizeof(v));}
}
int main(){
    mango_chams::Api o;o.loaded=true;o.find=find;o.read=read;o.decode=[](void* p){return p;};
    o.game_object=[](void* p){return returned(native(p).game);};o.transform=[](void* p){return returned(native(p).transform);};
    o.retain=[](int id,uintptr_t){if(id)++roots[id];};o.release=[](int id,uintptr_t){if(id){assert(roots[id]>0);--roots[id];}};
    o.component=[](void* go,mango_chams::Span* name)->void*{++lookups;assert(std::u16string(name->data,name->length)==u"HyperHug.Games.Oxide.Features.Player.FirstPersonBodyOffsets");return returned(native(go).component);};
    Api api;api.loaded=true;api.objects=&o;
    api.parent=[](void* p){return returned(native(p).parent);};
    api.bone=[](void*,int human){++bone_calls;return returned(humans[human]);};
    api.local_scale=[](void* p,V3* v){*v=native(p).scale;};api.set_local_scale=[](void* p,V3* v){++scale_writes;native(p).scale=*v;};
    api.local_position=[](void* p,V3* v){*v=native(p).position;};api.set_local_position=[](void* p,V3* v){native(p).position=*v;};
    api.behaviour=[](void* p){return native(p).enabled;};api.set_behaviour=[](void* p,bool enabled){native(p).enabled=enabled;};
    auto& animator=make(1);animator.game=2;animator.transform=3;auto& go=make(2);go.component=4;go.transform=3;
    auto& root=make(3);root.scale={2.54f,2.54f,2.54f};auto& offsets=make(4);
    // Same ancestor relationships and eight malformed scales as the actual
    // PlayerView_Local prefab. Import scale 100 belongs to a separate ancestor.
    make(5,3).scale={100,100,100};make(6,5); // armature, hips
    make(7,6).scale={1,1.2f,1};make(8,7).scale={.8f,1.2f,1};make(9,8).scale={.8f,1.2f,1};
    make(10,9).scale={0,0,0};make(11,10); // neck, head
    make(12,9).scale={0,0,0};make(13,12).scale={0,0,0};make(14,13);make(15,14); // left arm
    make(16,9).scale={0,0,0};make(17,16).scale={0,0,0};make(18,17);make(19,18); // right arm
    humans={{10,11},{17,15},{18,19}};
    Array entries,before,after;entries.count=before.count=after.count=3;
    for(unsigned i=0;i<3;++i){void* bone=find(7+i);std::memcpy(entries.data.data()+i*24,&bone,sizeof(bone));V3 a{float(i),float(i+1),0},b=a;b.z-=.05f*(i+1);vector(before,i,a);vector(after,i,b);objects[7+i]->position=b;}
    ptr(offsets.managed.data(),0x20,&entries);ptr(offsets.managed.data(),0x28,&before);ptr(offsets.managed.data(),0x30,&after);offsets.managed[0x38]=1;
    // Animator already updated Spine1; don't replace this new animated pose.
    objects[8]->position={4,5,6};
    LocalBodyGeometry geometry;geometry.bind(api,find(1));assert(bone_calls==3&&lookups==1);
    geometry.show(api,true);assert(!offsets.enabled&&scale_writes==8);
    for(int id:{7,8,9,10,12,13,16,17})equal(objects[id]->scale,{1,1,1});
    equal(objects[5]->scale,{100,100,100});equal(root.scale,{2.54f,2.54f,2.54f});
    equal(objects[7]->position,{0,1,0});equal(objects[8]->position,{4,5,6});equal(objects[9]->position,{2,3,0});
    // Frames change animated positions normally. No repeat component/bone
    // lookup, accumulated offsets or redundant scale setters are needed.
    for(int frame=0;frame<120;++frame){objects[11]->position={1,float(frame),3};geometry.show(api,true);equal(objects[11]->position,{1,float(frame),3});}
    assert(bone_calls==3&&lookups==1&&scale_writes==8);
    objects[12]->scale={0,0,0};geometry.show(api,true);equal(objects[12]->scale,{1,1,1});assert(scale_writes==9);
    // Preserve an unrelated game scale change instead of restoring over it.
    objects[10]->scale={1.1f,1.1f,1.1f};geometry.show(api,true);
    geometry.show(api,false);assert(offsets.enabled);equal(objects[10]->scale,{1.1f,1.1f,1.1f});equal(objects[12]->scale,{0,0,0});equal(objects[8]->scale,{.8f,1.2f,1});
    // Returning to third person uses the newly recorded first-person offset,
    // not an old snapshot from before animation advanced.
    for(unsigned i=0;i<3;++i){V3 a{10+float(i),20,30},b=a;b.z-=.05f*(i+1);vector(before,i,a);vector(after,i,b);objects[7+i]->position=b;}
    geometry.show(api,true);equal(objects[7]->position,{10,20,30});equal(objects[9]->position,{12,20,30});
    // A destroyed managed wrapper must not cause access to freed native bones.
    ptr(objects[17]->managed.data(),0x10,nullptr);geometry.show(api,false);geometry.reset(api);
    for(auto [id,count]:roots)assert(count==0);
    // Respect a game-disabled offsets component and invalid/incomplete arrays.
    offsets.enabled=false;entries.count=99;
    geometry.bind(api,find(1));geometry.show(api,true);geometry.reset(api);assert(!offsets.enabled);
    for(auto [id,count]:roots)assert(count==0);
    // Non-finite scales are ignored; traversal remains bounded.
    objects[11]->scale={NAN,1,1};geometry.bind(api,find(1));geometry.show(api,true);assert(std::isnan(objects[11]->scale.x));geometry.reset(api);
    for(auto [id,count]:roots)assert(count==0);
    // A cloned Animator can expose its avatar one frame after activation.
    // Retry that initialization without a per-frame bone/hierarchy search.
    objects[11]->scale={1,1,1};ptr(objects[17]->managed.data(),0x10,objects[17].get());entries.count=3;
    make(30).game=2;
    o.named_ref=[](void*,const char* name)->void*{return std::string(name)=="skinnedMeshRenderer"?find(30):nullptr;};
    api.active_self=[](void* p){return native(p).enabled;};api.active=api.active_self;api.set_active=api.set_behaviour;api.game_transform=o.transform;
    humans.clear();LocalModel model;model.update(api,find(1),find(30),true,0,true);
    assert(model.has_primary());const int first_attempt=bone_calls;
    humans={{10,11},{17,15},{18,19}};model.update(api,find(1),find(30),true,.5,true);assert(bone_calls==first_attempt);
    model.update(api,find(1),find(30),true,1.1,true);assert(bone_calls==first_attempt+3);equal(objects[12]->scale,{1,1,1});
    model.update(api,find(1),find(30),true,2.2,true);assert(bone_calls==first_attempt+3);
    model.reset(api);for(auto [id,count]:roots)assert(count==0);
    // Some Unity class resolvers accept the short script name only.
    o.component=[](void* p,mango_chams::Span* name)->void*{
        ++lookups;const std::u16string text(name->data,name->length);
        if(text==u"HyperHug.Games.Oxide.Features.Player.FirstPersonBodyOffsets")return nullptr;
        assert(text==u"FirstPersonBodyOffsets");return returned(native(p).component);
    };
    const int previous_lookups=lookups;geometry.bind(api,find(1));assert(lookups==previous_lookups+2);geometry.show(api,true);geometry.reset(api);
    for(auto [id,count]:roots)assert(count==0);
}
