#include "shader_asset.h"
#include "prefab_asset.h"
#include <cassert>
#include <string>
struct Managed {void* klass=nullptr,*monitor=nullptr,*native=nullptr;int id=0;};
struct Object {Managed managed;int flags=0;};
struct Operation {bool done=false;};
Object bundle,shader;Operation file_op,asset_op;int releases=0,unloads=0,loads=0;bool fail_bundle=false;
std::u16string expected_asset=u"cfesp/unlit_always";int retains=0;
void* encode(Managed* p){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)|1);}
void* resolve(const char* name){
    const std::string key=name;
    if(key.find("LoadFromFileAsync")!=std::string::npos)return reinterpret_cast<void*>(+[](mango_chams::Span* path,uint32_t crc,uint64_t offset)->void*{assert(path->length&&crc==0&&offset==0);++loads;return &file_op;});
    if(key.find("LoadAssetAsync")!=std::string::npos)return reinterpret_cast<void*>(+[](void* b,mango_chams::Span* name,void* type)->void*{assert(b==&bundle&&std::u16string(name->data,name->length)==expected_asset&&type);++loads;return &asset_op;});
    if(key.find("get_isDone")!=std::string::npos)return reinterpret_cast<void*>(+[](void* p){return static_cast<Operation*>(p)->done;});
    if(key.find("get_assetBundle")!=std::string::npos)return reinterpret_cast<void*>(+[](void*)->void*{return fail_bundle?nullptr:encode(&bundle.managed);});
    if(key.find("GetResult")!=std::string::npos)return reinterpret_cast<void*>(+[](void*)->void*{return encode(&shader.managed);});
    if(key.find("InternalDestroy")!=std::string::npos)return reinterpret_cast<void*>(+[](void*){++releases;});
    if(key.find("Unload_Injected")!=std::string::npos)return reinterpret_cast<void*>(+[](void* b,bool destroy){assert(b==&bundle&&!destroy);++unloads;});
    return nullptr;
}
int main(){
    bundle.managed.native=&bundle;bundle.managed.id=1;shader.managed.native=&shader;shader.managed.id=2;
    mango_chams::Api api;api.read=[](uintptr_t p,void* out,size_t n){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),n);return true;};
    api.decode=[](void* p){return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)&~uintptr_t(1));};
    api.find=[](int id)->void*{return id==1?&bundle.managed:id==2?&shader.managed:nullptr;};
    api.set_flags=[](void* p,int flags){static_cast<Object*>(p)->flags=flags;};
    mango_graphics::ShaderAsset loader;assert(loader.bind(resolve));auto path=mango_chams::span(u"/private/shader.bundle");
    loader.update(api,path,nullptr,1);assert(loads==0);
    loader.update(api,path,&shader,1.2);assert(loader.status==1&&loads==1);
    loader.update(api,path,&shader,1.4);assert(loader.status==1&&releases==0);
    file_op.done=true;loader.update(api,path,&shader,1.6);assert(loader.status==2&&loads==2&&releases==1);
    asset_op.done=true;loader.update(api,path,&shader,1.8);assert(loader.status==3&&loader.id()==2&&releases==2&&unloads==1);
    assert(shader.flags==32);loader.update(api,path,&shader,20);assert(loads==2);
    mango_graphics::ShaderAsset failure;assert(failure.bind(resolve));fail_bundle=true;
    failure.update(api,path,&shader,30);failure.update(api,path,&shader,31);assert(failure.status==-5&&releases==3);
    mango_graphics::ShaderAsset timeout;assert(timeout.bind(resolve));file_op.done=false;
    timeout.update(api,path,&shader,40);timeout.update(api,path,&shader,71);assert(timeout.status==-4&&releases==4);
    // The prefab wrapper must stay rooted after Unload(false), so creating a
    // new avatar after reconnect can reuse it without reloading all textures.
    expected_asset=u"cfesp/avatar";fail_bundle=false;file_op.done=asset_op.done=true;
    api.retain=[](int32_t id,uintptr_t native){assert(id==2&&native==reinterpret_cast<uintptr_t>(&shader));++retains;};
    mango_graphics::PrefabAsset prefab;assert(prefab.bind(resolve));
    prefab.update(api,path,&shader,80);int pending_loads=loads;
    prefab.update(api,path,nullptr,80.5);assert(prefab.status==1&&loads==pending_loads);
    prefab.update(api,path,&shader,81);prefab.update(api,path,&shader,82);
    assert(prefab.status==3&&prefab.id()==2&&retains==1&&unloads==2);
    int previous_loads=loads;for(int i=0;i<120;++i)prefab.update(api,path,&shader,83+i*.1);
    assert(loads==previous_loads&&retains==1);
    mango_graphics::PrefabAsset recovering;assert(recovering.bind(resolve));fail_bundle=true;
    recovering.update(api,path,&shader,200);recovering.update(api,path,&shader,201);assert(recovering.status==-5);
    recovering.retry(202);assert(recovering.status==0);assert(recovering.bind(resolve));
    recovering.update(api,path,&shader,202);recovering.update(api,path,&shader,202.2);assert(recovering.status==-5);
    recovering.retry(202.3);assert(recovering.status==-5); // no per-frame retry loop
    fail_bundle=false;recovering.retry(208);assert(recovering.status==0);assert(recovering.bind(resolve));
    recovering.update(api,path,&shader,209);recovering.update(api,path,&shader,210);recovering.update(api,path,&shader,211);
    assert(recovering.status==3&&retains==2);
}
