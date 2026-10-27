#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <cmath>
#include <vector>
#include <deque>
#include <unordered_set>
#include <utility>
#include "unity_renderer_state.h"

namespace mango_chams {
struct Span {const char16_t* data;int32_t length;};
template<size_t N>Span span(const char16_t (&text)[N]){return {text,int32_t(N-1)};}
struct Color {float r,g,b,a;};
struct Candidate {uintptr_t owner=0,root=0;float meters=0;int visibility=2;uintptr_t model=0;};
struct Options {bool enabled=false,through_walls=false;float alpha=.6f,range=80;unsigned limit=16;};
struct Api {
    using Resolve=void*(*)(const char*);
    using Unary=void*(*)(void*);
    Unary game_object=nullptr,transform=nullptr,clone=nullptr,materials=nullptr;
    Unary get_shader=nullptr;
    bool(*has_property)(void*,int)=nullptr;
    void*(*component)(void*,Span*)=nullptr;
    int(*child_count)(void*)=nullptr;
    void*(*child)(void*,int)=nullptr;
    void*(*shader)(Span*)=nullptr;        // Shader.FindBuiltin: builtin resources only
    void*(*find_shader)(Span*)=nullptr;   // ResourcesAPIInternal.FindShaderByName: what Shader.Find calls
    int(*property)(Span*)=nullptr;
    void(*set_shader)(void*,void*)=nullptr;
    void(*set_float)(void*,int,float)=nullptr;
    void(*set_color)(void*,int,Color*)=nullptr;
    void(*set_texture)(void*,int,void*)=nullptr;
    void(*set_queue)(void*,int)=nullptr;
    void(*keyword_on)(void*,Span*)=nullptr;
    void(*keyword_off)(void*,Span*)=nullptr;
    void(*set_pass)(void*,Span*,bool)=nullptr;
    void(*set_flags)(void*,int)=nullptr;
    void(*destroy)(void*,float)=nullptr;
    void(*set_materials)(void*,void*,int)=nullptr;
    bool(*occlusion)(void*)=nullptr;void(*set_occlusion)(void*,bool)=nullptr;
    void(*set_force_off)(void*,bool)=nullptr;int force_off_offset=-1;
    void*(*named_ref)(void*,const char*)=nullptr;
    // Host integration: decode an object return, resolve a managed instance ID,
    // safe memory read, and a managed reference store with the GC write barrier.
    void*(*decode)(void*)=nullptr;
    void*(*find)(int32_t)=nullptr;
    bool(*read)(uintptr_t,void*,size_t)=nullptr;
    void(*store_ref)(uintptr_t,void*)=nullptr;
    // Monotonic seconds. Null means "no wall-clock limit" (host tests).
    double(*clock)()=nullptr;
    void(*log)(const char*)=nullptr; // diagnostics only
    // Every pointer handed to field() was returned by Unity a moment ago
    // (FindObjectFromInstanceID, a decoded handle, a freshly allocated array),
    // so it can be dereferenced instead of paying a syscall for each read.
    // Implausible addresses still go through the fault-safe read().
    bool direct=false;
    bool loaded=false;
    int native_id_offset=-1;
    mutable unsigned id_failures=0;
    // GetComponentByName spelling that resolved last time: 0 unknown,
    // 1 short ("SkinnedMeshRenderer"), 2 full ("UnityEngine.SkinnedMeshRenderer").
    // Without this every hierarchy node paid for two failed type lookups.
    mutable int8_t renderer_spelling[2]{0,0};
    bool load(Resolve resolve){
        if(loaded)return true;
#define CH_BIND(member,name) member=reinterpret_cast<decltype(member)>(resolve(name));if(!member){if(log)log("missing icall: " name);return false;}
        CH_BIND(game_object,"UnityEngine.Component::get_gameObject_Injected(System.IntPtr)");
        CH_BIND(transform,"UnityEngine.Component::get_transform_Injected(System.IntPtr)");
        CH_BIND(component,"UnityEngine.GameObject::GetComponentByName_Injected(System.IntPtr,UnityEngine.Bindings.ManagedSpanWrapper&)");
        CH_BIND(child_count,"UnityEngine.Transform::get_childCount_Injected(System.IntPtr)");
        CH_BIND(child,"UnityEngine.Transform::GetChild_Injected(System.IntPtr,System.Int32)");
        CH_BIND(clone,"UnityEngine.Object::Internal_CloneSingle_Injected(System.IntPtr)");
        CH_BIND(materials,"UnityEngine.Renderer::GetSharedMaterialArray_Injected(System.IntPtr)");
        CH_BIND(set_materials,"UnityEngine.Renderer::SetMaterialArray_Injected(System.IntPtr,UnityEngine.Material[],System.Int32)");
        CH_BIND(occlusion,"UnityEngine.Renderer::get_allowOcclusionWhenDynamic_Injected(System.IntPtr)");
        CH_BIND(set_occlusion,"UnityEngine.Renderer::set_allowOcclusionWhenDynamic_Injected(System.IntPtr,System.Boolean)");
        CH_BIND(set_force_off,"UnityEngine.Renderer::set_forceRenderingOff_Injected(System.IntPtr,System.Boolean)");
        CH_BIND(shader,"UnityEngine.Shader::FindBuiltin_Injected(UnityEngine.Bindings.ManagedSpanWrapper&)");
        CH_BIND(property,"UnityEngine.Shader::PropertyToID_Injected(UnityEngine.Bindings.ManagedSpanWrapper&)");
        CH_BIND(set_shader,"UnityEngine.Material::set_shader_Injected(System.IntPtr,System.IntPtr)");
        CH_BIND(get_shader,"UnityEngine.Material::get_shader_Injected(System.IntPtr)");
        CH_BIND(has_property,"UnityEngine.Material::HasProperty_Injected(System.IntPtr,System.Int32)");
        CH_BIND(set_float,"UnityEngine.Material::SetFloatImpl_Injected(System.IntPtr,System.Int32,System.Single)");
        CH_BIND(set_color,"UnityEngine.Material::SetColorImpl_Injected(System.IntPtr,System.Int32,UnityEngine.Color&)");
        CH_BIND(set_texture,"UnityEngine.Material::SetTextureImpl_Injected(System.IntPtr,System.Int32,System.IntPtr)");
        CH_BIND(set_queue,"UnityEngine.Material::set_renderQueue_Injected(System.IntPtr,System.Int32)");
        CH_BIND(keyword_on,"UnityEngine.Material::EnableKeyword_Injected(System.IntPtr,UnityEngine.Bindings.ManagedSpanWrapper&)");
        CH_BIND(keyword_off,"UnityEngine.Material::DisableKeyword_Injected(System.IntPtr,UnityEngine.Bindings.ManagedSpanWrapper&)");
        CH_BIND(set_pass,"UnityEngine.Material::SetShaderPassEnabled_Injected(System.IntPtr,UnityEngine.Bindings.ManagedSpanWrapper&,System.Boolean)");
        CH_BIND(set_flags,"UnityEngine.Object::set_hideFlags_Injected(System.IntPtr,UnityEngine.HideFlags)");
        CH_BIND(destroy,"UnityEngine.Object::Destroy_Injected(System.IntPtr,System.Single)");
        find_shader=reinterpret_cast<decltype(find_shader)>(resolve("UnityEngine.ResourcesAPIInternal::FindShaderByName_Injected(UnityEngine.Bindings.ManagedSpanWrapper&)"));
        if(!find_shader&&log)log("missing icall: ResourcesAPIInternal::FindShaderByName (falling back to FindBuiltin)");
        auto get_id_offset=reinterpret_cast<int(*)()>(resolve("UnityEngine.Object::GetOffsetOfInstanceIDInCPlusPlusObject()"));
        if(!get_id_offset){if(log)log("missing icall: GetOffsetOfInstanceIDInCPlusPlusObject");return false;}
        const int offset=get_id_offset();native_id_offset=offset>=0&&offset<=256&&offset%4==0?offset:-1;
        if(read)force_off_offset=mango_unity::force_rendering_offset(reinterpret_cast<uintptr_t>(set_force_off),read);
#undef CH_BIND
        loaded=decode&&find&&read&&store_ref;return loaded;
    }
    double deadline(double seconds)const{return clock?clock()+seconds:1e300;}
    bool expired(double deadline)const{return clock&&clock()>=deadline;}
    // Shader.Find(name) semantics (all shaders in the player), then builtin resources.
    void* shader_by_name(Span* name)const{
        void* result=find_shader?find_shader(name):nullptr;
        if(!result&&shader)result=shader(name);
        return result;
    }
    template<class T>T field(void* object,size_t offset)const{
        T value{};if(!object)return value;
        const uintptr_t at=reinterpret_cast<uintptr_t>(object)+offset;
        constexpr uintptr_t align=sizeof(T)>8?8:sizeof(T);
        if(direct&&at>=0x10000&&at<(uintptr_t(1)<<47)&&!(at&(align-1)))std::memcpy(&value,reinterpret_cast<const void*>(at),sizeof(T));
        else read(at,&value,sizeof(value));
        return value;
    }
    // Renderer component of a GameObject (0 when it has none).
    int32_t renderer_id(void* object,bool skinned)const{
        Span shortName=skinned?span(u"SkinnedMeshRenderer"):span(u"MeshRenderer");
        Span fullName=skinned?span(u"UnityEngine.SkinnedMeshRenderer"):span(u"UnityEngine.MeshRenderer");
        // UnityEngine types share one naming convention, so a hit for either
        // renderer type settles the spelling for both. Characters usually have
        // no MeshRenderer at all and would otherwise never stop probing it.
        const int8_t spelling=renderer_spelling[skinned?0:1];
        int32_t result=0;
        if(spelling!=2){result=returned_id(component(object,&shortName));if(result){renderer_spelling[0]=renderer_spelling[1]=1;return result;}}
        if(spelling!=1){result=returned_id(component(object,&fullName));if(result)renderer_spelling[0]=renderer_spelling[1]=2;}
        return result;
    }
    void* native(void* managed)const{return field<void*>(managed,0x10);}
    void* native_return(void* value)const{return native(decode(value));}
    int32_t id(void* managed)const{
        if(!managed||!native(managed))return 0;
        void* object=native(managed);
        const int32_t result=field<int32_t>(managed,0x18);
        if(result&&native(find(result))==object)return result;
        if(native_id_offset>=0){
            const int32_t native_id=field<int32_t>(object,size_t(native_id_offset));
            if(native_id&&native(find(native_id))==object)return native_id;
        }
        ++id_failures;return 0;
    }
    int32_t returned_id(void* value)const{return id(decode(value));}
    void* live(int32_t id)const{return id?native(find(id)):nullptr;}
    int count(void* array)const{const uintptr_t n=field<uintptr_t>(array,0x18);return n<=32?int(n):-1;}
    void* item(void* array,int i)const{return field<void*>(array,0x20+size_t(i)*8);}
    void put(void* array,int i,int32_t id)const{store_ref(reinterpret_cast<uintptr_t>(array)+0x20+size_t(i)*8,find(id));}
};

class Controller {
    struct Slot {int32_t original=0,backup=0;};
    struct Renderer {int32_t id=0;uintptr_t native=0;std::vector<Slot> slots;int state=-1;double next_check=0;bool occlusion=true,occlusion_owned=false,force_off=false,force_owned=false;};
    struct Actor {uintptr_t owner=0,root=0,model=0;std::deque<int32_t> queue;std::unordered_set<int32_t> visited;std::vector<Renderer> renderers;double retry=0;};
    struct Backup {int32_t original=0,copy=0;unsigned refs=0;bool adopted=false;};
    std::vector<Actor> actors_;
    std::vector<Backup> backups_;
    std::array<int32_t,3> tint_{};
    std::array<int,16> props_{};
    size_t discovery_cursor_=0;
    float alpha_=-1;bool xray_=false;double last_tick_=-1;
    int32_t shader_=0;
    bool shader_attempted_=false;
    bool requested_xray_=false,shader_logged_=false;
    double shader_retry_=0;
    bool owned(int32_t id)const{return id&&(id==tint_[0]||id==tint_[1]||id==tint_[2]);}
    void remove_material(Api& api,int32_t id){if(void* p=api.live(id))api.destroy(p,0);}
    int32_t clone_owned(Api& api,void* original){
        void* object=api.decode(api.clone(original));void* native=api.native(object);
        if(!native||native==original)return 0;
        const int32_t id=api.id(object);
        if(!id){api.destroy(native,0);return 0;}
        ++material_creations;api.set_flags(native,32);return id;
    }
    int32_t backup(Api& api,int32_t original){
        auto old=std::find_if(backups_.begin(),backups_.end(),[&](const Backup& b){return b.original==original;});
        if(old!=backups_.end()){++old->refs;return old->copy;}
        void* material=api.live(original);if(!material||backups_.size()>=256)return 0;
        const int32_t copy=clone_owned(api,material);if(!copy)return 0;
        backups_.push_back({original,copy,1,false});return copy;
    }
    void release(Api& api,const Slot& slot,bool adopted){
        auto it=std::find_if(backups_.begin(),backups_.end(),[&](const Backup& b){return b.copy==slot.backup;});
        if(it==backups_.end())return;
        it->adopted|=adopted;
        if(--it->refs==0){
            if(it->adopted){if(void* p=api.live(it->copy))api.set_flags(p,0);}
            else remove_material(api,it->copy);
            backups_.erase(it);
        }
    }
    void restore(Api& api,Renderer& renderer){
        void* native=api.live(renderer.id);if(reinterpret_cast<uintptr_t>(native)!=renderer.native)native=nullptr;
        if(native&&renderer.occlusion_owned&&api.occlusion&&!api.occlusion(native))api.set_occlusion(native,renderer.occlusion);
        if(native&&renderer.force_owned&&api.field<uint8_t>(native,size_t(api.force_off_offset))==0)api.set_force_off(native,renderer.force_off);
        void* array=native?api.materials(native):nullptr;
        const int count=array?api.count(array):0;bool changed=false;
        for(size_t i=0;i<renderer.slots.size();++i){
            const Slot slot=renderer.slots[i];bool adopted=false;
            if(int(i)<count&&owned(api.id(api.item(array,int(i))))){
                int32_t original=api.live(slot.original)?slot.original:slot.backup;
                if(api.live(original)){api.put(array,int(i),original);changed=true;adopted=original==slot.backup;}
                else ++restore_errors;
            }
            release(api,slot,adopted);
        }
        if(changed)api.set_materials(native,array,count);
        renderer.slots.clear();
    }
    void restore(Api& api,Actor& actor){for(auto& r:actor.renderers)restore(api,r);actor.renderers.clear();}
    // Hidden/Internal-Colored declares _ZTest/_ZWrite/_Cull/_SrcBlend/_DstBlend as
    // ordinary properties, so ZTest Always is honoured per material. UI/Default
    // reads unity_GUIZTestMode instead and is only a fallback. FindBuiltin cannot
    // be used: it only knows the builtin resource table and returned null for both.
    bool acquire_xray_shader(Api& api){
        if(shader_)return true;
        shader_attempted_=true;
        const char16_t* const names[]={u"Hidden/Internal-Colored",u"UI/Default"};
        const int kinds[]={5,4};
        for(int i=0;i<2&&!shader_;++i){
            Span name{names[i],int32_t(std::char_traits<char16_t>::length(names[i]))};
            shader_=api.returned_id(api.shader_by_name(&name));
            if(shader_)shader_kind=kinds[i];
        }
        if(api.log&&!shader_logged_){
            shader_logged_=true;char note[96];std::snprintf(note,sizeof(note),"Chams shader: kind=%d id=%d (5 Internal-Colored, 4 UI/Default, 0 none)",shader_kind,int(shader_));api.log(note);
        }
        return shader_!=0;
    }
    bool prepare_tints(Api& api,int32_t source){
        if(tint_[0]&&tint_[1]&&tint_[2])return true;
        if(!shader_){
            shader_attempted_=true;
            if(requested_xray_){
                if(!acquire_xray_shader(api))return false;
            }else{
            auto name=span(u"Universal Render Pipeline/Unlit");shader_=api.returned_id(api.shader(&name));
            if(shader_)shader_kind=1;
            else{auto particles=span(u"Universal Render Pipeline/Particles/Unlit");shader_=api.returned_id(api.shader(&particles));if(shader_)shader_kind=2;}
            if(!shader_&&api.get_shader&&api.has_property){
                void* original=api.live(source);
                auto base=span(u"_BaseColor"),surface=span(u"_Surface"),src=span(u"_SrcBlend"),dst=span(u"_DstBlend"),depth=span(u"_ZWrite");
                bool compatible=original!=nullptr;
                for(Span* property:{&base,&surface,&src,&dst,&depth})compatible=compatible&&api.has_property(original,api.property(property));
                if(compatible){shader_=api.returned_id(api.get_shader(original));if(shader_)shader_kind=3;}
            }
            }
        }
        if(!shader_)return false;
        auto p0=span(u"_BaseColor"),p1=span(u"_Color"),p2=span(u"_Surface"),p3=span(u"_SrcBlend"),p4=span(u"_DstBlend"),p5=span(u"_ZWrite"),p6=span(u"_ZTest"),p7=span(u"_BaseMap"),p8=span(u"_MainTex");
        auto p9=span(u"_SrcBlendAlpha"),p10=span(u"_DstBlendAlpha"),p11=span(u"_AlphaClip"),p12=span(u"_AlphaToMask"),p13=span(u"_Blend"),p14=span(u"_Cull"),p15=span(u"_SampleGI");
        const std::array<Span*,16> names{&p0,&p1,&p2,&p3,&p4,&p5,&p6,&p7,&p8,&p9,&p10,&p11,&p12,&p13,&p14,&p15};
        for(size_t i=0;i<names.size();++i)props_[i]=api.property(names[i]);
        void* material=api.live(source);void* shader=api.live(shader_);if(!material||!shader)return false;
        for(auto& id:tint_)if(!id){
            id=clone_owned(api,material);void* copy=api.live(id);if(!copy)return false;
            api.set_shader(copy,shader);
            if(shader_kind==5){
                // The clone still carries the game's opaque blend/cull/zwrite
                // values under the same property names; overwrite every one.
                api.set_queue(copy,4000);
                auto bias=span(u"_ZBias");
                api.set_float(copy,props_[3],5);api.set_float(copy,props_[4],10);
                api.set_float(copy,props_[5],0);api.set_float(copy,props_[6],8);
                api.set_float(copy,props_[14],0);api.set_float(copy,api.property(&bias),0);
                continue;
            }
            if(shader_kind==4){
                api.set_queue(copy,5000);
                auto depth=span(u"unity_GUIZTestMode"),comp=span(u"_StencilComp"),stencil=span(u"_Stencil"),op=span(u"_StencilOp"),write=span(u"_StencilWriteMask"),read=span(u"_StencilReadMask"),mask=span(u"_ColorMask"),clip=span(u"_UseUIAlphaClip");
                for(auto setting:std::array<std::pair<Span*,float>,8>{{{&depth,8},{&comp,8},{&stencil,0},{&op,0},{&write,0},{&read,255},{&mask,15},{&clip,0}}})api.set_float(copy,api.property(setting.first),setting.second);
                auto rect=span(u"UNITY_UI_CLIP_RECT"),alpha=span(u"UNITY_UI_ALPHACLIP");api.keyword_off(copy,&rect);api.keyword_off(copy,&alpha);
                if(api.has_property(copy,props_[8]))api.set_texture(copy,props_[8],nullptr);
                continue;
            }
            api.set_queue(copy,3100);api.set_float(copy,props_[2],1);api.set_float(copy,props_[3],5);api.set_float(copy,props_[4],10);api.set_float(copy,props_[5],0);
            const float alpha_settings[]={1,10,0,0,0,2,0};
            for(size_t i=9;i<props_.size();++i)if(api.has_property(copy,props_[i]))api.set_float(copy,props_[i],alpha_settings[i-9]);
            if(api.has_property(copy,props_[7]))api.set_texture(copy,props_[7],nullptr);
            if(api.has_property(copy,props_[8]))api.set_texture(copy,props_[8],nullptr);
            auto transparent=span(u"_SURFACE_TYPE_TRANSPARENT"),clip=span(u"_ALPHATEST_ON"),premult=span(u"_ALPHAPREMULTIPLY_ON"),shadow=span(u"ShadowCaster"),depth=span(u"DepthOnly"),normals=span(u"DepthNormals");
            api.keyword_on(copy,&transparent);api.keyword_off(copy,&clip);api.keyword_off(copy,&premult);
            auto modulate=span(u"_ALPHAMODULATE_ON");api.keyword_off(copy,&modulate);
            api.set_pass(copy,&shadow,false);api.set_pass(copy,&depth,false);api.set_pass(copy,&normals,false);
        }
        alpha_=-1;return true;
    }
    void colors(Api& api,const Options& options){
        if(alpha_==options.alpha&&xray_==options.through_walls)return;
        const std::array<Color,3> colors{{{.12f,.83f,1,options.alpha},{1,.18f,.28f,options.alpha},{.65f,.68f,.78f,options.alpha}}};
        for(size_t i=0;i<tint_.size();++i)if(void* material=api.live(tint_[i])){
            Color color=colors[i];
            if(api.has_property(material,props_[0]))api.set_color(material,props_[0],&color);
            if(api.has_property(material,props_[1]))api.set_color(material,props_[1],&color);
            if(api.has_property(material,props_[6]))api.set_float(material,props_[6],options.through_walls?8:4);
        }
        alpha_=options.alpha;xray_=options.through_walls;
    }
    bool add_renderer(Api& api,Actor& actor,int32_t id){
        if(!id||actor.renderers.size()>=48||std::any_of(actor.renderers.begin(),actor.renderers.end(),[&](const Renderer& r){return r.id==id;}))return false;
        void* native=api.live(id);if(!native){++diag.no_native;return false;}
        void* array=api.materials(native);const int count=api.count(array);if(count<=0||count>16){++diag.bad_slots;return false;}
        Renderer renderer;renderer.id=id;renderer.native=reinterpret_cast<uintptr_t>(native);
        for(int i=0;i<count;++i){
            const int32_t original=api.id(api.item(array,i));
            if(!original||owned(original)||!prepare_tints(api,original)){
                ++diag.no_tint;for(const auto& slot:renderer.slots)release(api,slot,false);return false;
            }
            const int32_t copy=backup(api,original);
            if(!copy){++diag.no_backup;for(const auto& slot:renderer.slots)release(api,slot,false);return false;}
            renderer.slots.push_back({original,copy});
        }
        if(requested_xray_&&api.occlusion){renderer.occlusion=api.occlusion(native);renderer.occlusion_owned=true;api.set_occlusion(native,false);}
        if(requested_xray_&&api.force_off_offset>=0){renderer.force_owned=true;renderer.force_off=api.field<uint8_t>(native,size_t(api.force_off_offset))!=0;api.set_force_off(native,false);}
        actor.renderers.push_back(std::move(renderer));++diag.found;return true;
    }
    bool rebind(Api& api,Renderer& renderer,void* array,int count){
        if(count<=0||count>16)return false;
        std::vector<Slot> slots;std::vector<bool> kept(renderer.slots.size(),false),acquired;
        for(int i=0;i<count;++i){
            const int32_t id=api.id(api.item(array,i));
            if(owned(id)&&size_t(i)<renderer.slots.size()){slots.push_back(renderer.slots[i]);kept[i]=true;acquired.push_back(false);continue;}
            const int32_t copy=id&&!owned(id)?backup(api,id):0;
            if(!copy){for(size_t j=0;j<slots.size();++j)if(acquired[j])release(api,slots[j],false);return false;}
            slots.push_back({id,copy});acquired.push_back(true);
        }
        for(size_t i=0;i<renderer.slots.size();++i)if(!kept[i])release(api,renderer.slots[i],false);
        renderer.slots=std::move(slots);return true;
    }
public:
    int status=0,shader_kind=0;unsigned renderer_count=0,material_creations=0,restore_errors=0;
    // Why nothing is painted: counted per failure reason, logged by the host.
    struct Diag {unsigned actors=0,nodes=0,found=0,no_native=0,bad_slots=0,no_tint=0,no_backup=0,swaps=0,state_visible=0,state_occluded=0,state_unknown=0;} diag;
    void reset(Api& api){
        if(!api.loaded)return;
        for(auto& actor:actors_)restore(api,actor);actors_.clear();
        for(int32_t id:tint_)remove_material(api,id);tint_={};
        for(auto& b:backups_)if(!b.adopted)remove_material(api,b.copy);backups_.clear();
        shader_=0;shader_kind=0;shader_attempted_=false;shader_logged_=false;shader_retry_=0;alpha_=-1;renderer_count=0;status=0;last_tick_=-1;discovery_cursor_=0;
    }
    bool due(double time)const{return time-last_tick_>=.1;}
    void update(Api& api,const Options& options,std::vector<Candidate>& candidates,double time){
        if(!options.enabled){if(!actors_.empty()||tint_[0])reset(api);return;}
        if(!api.loaded){status=-1;return;}
        if(requested_xray_!=options.through_walls){reset(api);requested_xray_=options.through_walls;}
        if(time-last_tick_<.1)return;last_tick_=time;
        // No usable shader: do not scan hierarchies for nothing; retry slowly.
        if(requested_xray_&&!shader_){
            if(time<shader_retry_){status=-2;return;}
            if(!acquire_xray_shader(api)){shader_retry_=time+5;status=-2;return;}
        }
        candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const Candidate& c){return !c.owner||!c.root||!std::isfinite(c.meters)||c.meters>options.range;}),candidates.end());
        std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){return a.meters<b.meters;});
        if(candidates.size()>std::min(options.limit,32u))candidates.resize(std::min(options.limit,32u));
        diag.state_visible=diag.state_occluded=diag.state_unknown=0;
        for(const auto& c:candidates)++(c.visibility==0?diag.state_visible:c.visibility==1?diag.state_occluded:diag.state_unknown);
        for(auto it=actors_.begin();it!=actors_.end();){
            if(std::none_of(candidates.begin(),candidates.end(),[&](const Candidate& c){return c.owner==it->owner&&c.root==it->root&&c.model==it->model;})){
                restore(api,*it);it=actors_.erase(it);
            }else ++it;
        }
        // Hierarchy discovery is limited by wall-clock time, not by node count: a
        // node costs several type-name lookups and a crowded server used to
        // keep this loop busy for most of every frame.
        const double scan_deadline=api.deadline(.0012);
        unsigned nodes=256,new_renderers=6;renderer_count=0;
        for(size_t index=0;index<candidates.size();++index){
            const auto& c=candidates[(discovery_cursor_+index)%candidates.size()];
            auto it=std::find_if(actors_.begin(),actors_.end(),[&](const Actor& a){return a.owner==c.owner&&a.root==c.root;});
            if(it==actors_.end()){actors_.push_back({});it=actors_.end()-1;it->owner=c.owner;it->root=c.root;it->model=c.model;}
            Actor& actor=*it;
            if(actor.queue.empty()&&time>=actor.retry&&!api.expired(scan_deadline)){
                const int32_t root=api.returned_id(api.transform(reinterpret_cast<void*>(c.root)));
                if(root)actor.queue.push_back(root);actor.visited.clear();
                // A model that already has renderers only needs an occasional
                // look for newly equipped armor; an empty one is retried soon.
                actor.retry=time+(actor.renderers.empty()?.5:3.);
                if(c.model){const int32_t model=api.returned_id(api.transform(reinterpret_cast<void*>(c.model)));if(model&&model!=root)actor.queue.push_front(model);}
            }
            while(nodes&&new_renderers&&!actor.queue.empty()&&actor.visited.size()<768&&!api.expired(scan_deadline)){
                --nodes;++diag.nodes;const int32_t node=actor.queue.front();actor.queue.pop_front();
                if(!actor.visited.insert(node).second)continue;
                void* transform=api.live(node);if(!transform)continue;
                void* object=api.native_return(api.game_object(transform));
                if(object){
                    if(add_renderer(api,actor,api.renderer_id(object,true)))--new_renderers;
                    if(new_renderers&&add_renderer(api,actor,api.renderer_id(object,false)))--new_renderers;
                }
                const int count=std::clamp(api.child_count(transform),0,128);
                for(int i=0;i<count&&actor.queue.size()+actor.visited.size()<768;++i){const int32_t child=api.returned_id(api.child(transform,i));if(child)actor.queue.push_back(child);}
            }
            for(auto r=actor.renderers.begin();r!=actor.renderers.end();){
                void* native=api.live(r->id);if(!native||reinterpret_cast<uintptr_t>(native)!=r->native){for(auto slot:r->slots)release(api,slot,false);r=actor.renderers.erase(r);continue;}
                ++renderer_count;const int state=std::clamp(c.visibility,0,2);
                if((r->state!=state||time>=r->next_check)&&tint_[state]){
                    if(r->occlusion_owned&&api.occlusion(native))api.set_occlusion(native,false);
                    if(r->force_owned&&api.field<uint8_t>(native,size_t(api.force_off_offset)))api.set_force_off(native,false);
                    void* array=api.materials(native);const int count=api.count(array);
                    if(count<=0||count>16){restore(api,*r);r=actor.renderers.erase(r);continue;}
                    bool changed=count!=int(r->slots.size()),game_changed=changed;
                    for(int i=0;i<count;++i){const int32_t id=api.id(api.item(array,i));if(id!=tint_[state])changed=true;if(!owned(id)&&(size_t(i)>=r->slots.size()||id!=r->slots[i].original))game_changed=true;}
                    if(game_changed&&!rebind(api,*r,array,count)){++restore_errors;++r;continue;}
                    if(changed){for(int i=0;i<count;++i)api.put(array,i,tint_[state]);api.set_materials(native,array,count);++diag.swaps;}
                    // GetSharedMaterialArray allocates a managed array on every call;
                    // a visibility change is handled immediately above, so the
                    // periodic re-check only has to notice a game-side re-skin.
                    r->state=state;r->next_check=time+.75;
                }
                ++r;
            }
        }
        if(!candidates.empty())discovery_cursor_=(discovery_cursor_+1)%candidates.size();
        diag.actors=unsigned(actors_.size());
        colors(api,options);status=renderer_count?1:(shader_attempted_&&!shader_?-2:0);
    }
};
} // namespace mango_chams
