#include "custom_avatar.h"
#include <cassert>
#include <map>
#include <memory>
#include <string>
using namespace mango_camera;
namespace {
struct Object;struct Managed{char header[16]{};Object* native=nullptr;int32_t id=0;};
struct Object{Managed managed;int transform=0,go=0,component=0;std::vector<int> children;bool active=false,enabled=true;V3 position{};Quaternion rotation{};};
std::map<int,std::unique_ptr<Object>> objects;std::map<int,int> roots,target_by_human;
int queries=0,bones=0,clones=0,destroys=0,next_id=0;bool missing=false,missing_meshes=false,fail_clone=false;
Object& make(int id){auto o=std::make_unique<Object>();o->managed.id=id;o->managed.native=o.get();auto* p=o.get();objects[id]=std::move(o);return *p;}
void* find(int id){return objects.count(id)?&objects.at(id)->managed:nullptr;}
void* ret(int id){void* p=find(id);return p?reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)|1):nullptr;}
Object& obj(void* p){return *static_cast<Object*>(p);}
void* decode(void* p){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)&~uintptr_t(1));}
bool read(uintptr_t p,void* out,size_t n){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),n);return true;}
int node(){int tr=next_id++,go=next_id++;make(tr).go=go;make(go).transform=tr;return tr;}
int path(int root,unsigned depth,const unsigned char* steps){
 int current=root;for(unsigned i=0;i<depth;++i){auto& children=objects.at(current)->children;while(children.size()<=steps[i])children.push_back(node());current=children[steps[i]];}return current;
}
void* clone(void*){
 ++clones;if(fail_clone)return nullptr;int base=clones*10000;make(base).transform=base+1;make(base+1).go=base;next_id=base+2;target_by_human.clear();
 for(auto b:mango_avatar::bindings)target_by_human[b.human]=path(base+1,b.depth,b.steps);
 for(auto b:mango_avatar::mesh_bindings){int t=path(base+1,b.depth,b.steps),renderer=next_id++;make(renderer);objects.at(objects.at(t)->go)->component=renderer;}
 return ret(base);
}
}
int main(){
 mango_chams::Api o;o.loaded=true;o.find=find;o.decode=decode;o.read=read;o.clone=clone;
 o.retain=[](int id,uintptr_t){if(id)++roots[id];};o.release=[](int id,uintptr_t){if(id){assert(roots[id]>0);--roots[id];}};
 o.set_flags=[](void*,int){};o.destroy=[](void* p,float){++destroys;obj(p).managed.native=nullptr;};
 o.transform=[](void* p){return ret(obj(p).transform);};o.game_object=[](void* p){return ret(obj(p).go);};
 o.child_count=[](void* p){++queries;return int(obj(p).children.size());};o.child=[](void* p,int i){++queries;return ret(obj(p).children.at(i));};
 o.component=[](void* p,mango_chams::Span* type)->void*{++queries;assert(std::u16string(type->data,type->length)==u"SkinnedMeshRenderer");return missing_meshes?nullptr:ret(obj(p).component);};
 Api c;c.loaded=true;c.objects=&o;c.game_transform=o.transform;
 c.position=[](void* p,V3* v){*v=obj(p).position;};c.set_position=[](void* p,V3* v){obj(p).position=*v;};
 c.rotation=[](void* p,Quaternion* v){*v=obj(p).rotation;};c.set_rotation=[](void* p,Quaternion* q){obj(p).rotation=*q;};
 c.enabled=[](void* p){return obj(p).enabled;};c.set_enabled=[](void* p,bool v){obj(p).enabled=v;};
 c.active_self=[](void* p){return obj(p).active;};c.set_active=[](void* p,bool v){obj(p).active=v;};
 c.bone=[](void*,int id)->void*{++bones;return missing&&id==17?nullptr:find(1000+id);};
 mango_avatar::Api api;assert(api.load([](const char*)->void*{return nullptr;},c)); // no FindRelativeTransform dependency
 auto& animator=make(1);animator.transform=2;auto& source_root=make(2);source_root.position={4,0,5};make(3);
 for(auto b:mango_avatar::bindings){auto& t=make(1000+b.human);t.position={float(b.human),2,3};}
 mango_avatar::Controller model;
 assert(!model.update(api,find(1),3,false,true,0)&&clones==0);
 assert(model.update(api,find(1),3,true,true,1)&&clones==1&&model.status==3&&model.visible&&model.mesh_count==22);
 int lookups=queries,original_bones=bones;
 for(int i=0;i<120;++i)assert(model.pose(api));assert(queries==lookups&&bones==original_bones);
 for(auto b:mango_avatar::bindings){auto& src=*objects.at(1000+b.human);auto& dst=*objects.at(target_by_human.at(b.human));assert(length(sub(src.position,dst.position))<.00001f);assert(mango_avatar::valid(dst.rotation));}
 model.update(api,find(1),3,false,true,2);assert(!model.visible&&destroys==0);
 assert(model.update(api,find(1),3,true,true,3)&&clones==1&&queries==lookups);
 assert(!model.update(api,find(1),3,true,false,4)&&!model.visible);
 assert(model.update(api,find(1),3,true,true,5));
 objects.at(target_by_human.at(17))->managed.native=nullptr;assert(!model.pose(api)&&!model.visible);
 model.reset(api);for(auto [id,n]:roots)assert(n==0);
 // A late avatar must recover even after the old three-attempt cutoff.
 missing=true;for(int i=0;i<20;++i)assert(!model.update(api,find(1),3,true,true,6+i));
 assert(model.status==4&&!model.visible);missing=false;assert(model.update(api,find(1),3,true,true,30));
 model.reset(api);for(auto [id,n]:roots)assert(n==0);
 missing=true;assert(!model.update(api,find(1),3,true,true,31));lookups=queries;
 for(int i=0;i<30;++i)model.update(api,find(1),3,true,true,31.01+i/60.);assert(queries==lookups);
 missing=false;model.update(api,find(1),3,false,true,31.6);assert(model.update(api,find(1),3,true,true,31.7)); // explicit toggle retries immediately
 model.reset(api);for(auto [id,n]:roots)assert(n==0);
 missing_meshes=true;assert(!model.update(api,find(1),3,true,true,40)&&model.status==-3&&!model.visible);
 missing_meshes=false;assert(model.update(api,find(1),3,true,true,42));
 model.reset(api);for(auto [id,n]:roots)assert(n==0);
 auto& second=make(4);second.transform=2;assert(model.update(api,find(1),3,true,true,43));int before=destroys;
 assert(model.update(api,find(4),3,true,true,44)&&destroys==before+1);
 source_root.position.x=NAN;assert(!model.pose(api)&&!model.visible);source_root.position.x=4;
 model.reset(api);for(auto [id,n]:roots)assert(n==0);
 fail_clone=true;int previous_clones=clones;
 assert(!model.update(api,find(1),3,true,true,50));
 for(int i=0;i<90;++i)model.update(api,find(1),3,true,true,50.01+i/60.);
 assert(clones==previous_clones+1);fail_clone=false;
 assert(model.update(api,find(1),3,true,true,52.1));
 model.reset(api);for(auto [id,n]:roots)assert(n==0);
}
