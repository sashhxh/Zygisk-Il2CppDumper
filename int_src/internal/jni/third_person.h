#pragma once
#include "chams.h"
#include "local_player_view.h"
#include <cmath>

namespace mango_camera {
struct V3 {float x=0,y=0,z=0;};
struct Quaternion {float x=0,y=0,z=0,w=1;};
struct Matrix {float v[16]{};};
inline bool finite(Matrix m){for(float v:m.v)if(!std::isfinite(v))return false;return true;}
inline bool near(Matrix a,Matrix b){if(!finite(a)||!finite(b))return false;for(unsigned i=0;i<16;++i)if(std::fabs(a.v[i]-b.v[i])>.002f+std::fabs(b.v[i])*.00001f)return false;return true;}
inline Matrix translate_view(Matrix m,V3 delta){for(unsigned row=0;row<4;++row)m.v[12+row]-=m.v[row]*delta.x+m.v[4+row]*delta.y+m.v[8+row]*delta.z;return m;}
inline V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V3 scale(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float length(V3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
inline bool finite(V3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline V3 rotate(Quaternion q,V3 v){
    const V3 t{2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
    return {v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
struct Api {
    mango_chams::Api* objects=nullptr;
    void(*position)(void*,V3*)=nullptr;
    void(*set_position)(void*,V3*)=nullptr;
    void(*rotation)(void*,Quaternion*)=nullptr;void(*set_rotation)(void*,Quaternion*)=nullptr;
    void(*local_scale)(void*,V3*)=nullptr;void(*set_local_scale)(void*,V3*)=nullptr;
    void(*local_position)(void*,V3*)=nullptr;void(*set_local_position)(void*,V3*)=nullptr;
    void*(*bone)(void*,int)=nullptr; // managed Animator -> managed Transform
    int(*mask)(void*)=nullptr;void(*set_mask)(void*,int)=nullptr;
    bool(*enabled)(void*)=nullptr;void(*set_enabled)(void*,bool)=nullptr;
    int(*shadows)(void*)=nullptr;void(*set_shadows)(void*,int)=nullptr;
    uint32_t(*render_layers)(void*)=nullptr;void(*set_render_layers)(void*,uint32_t)=nullptr;
    int(*layer)(void*)=nullptr;bool(*active)(void*)=nullptr;
    bool(*is_child)(void*,void*)=nullptr;
    void*(*parent)(void*)=nullptr;
    void*(*game_transform)(void*)=nullptr;
    bool(*active_self)(void*)=nullptr;void(*set_active)(void*,bool)=nullptr;
    bool(*behaviour)(void*)=nullptr;void(*set_behaviour)(void*,bool)=nullptr;
    int(*culling)(void*)=nullptr;void(*set_culling)(void*,int)=nullptr;
    bool(*offscreen)(void*)=nullptr;void(*set_offscreen)(void*,bool)=nullptr;
    void(*view)(void*,Matrix*)=nullptr;void(*set_view)(void*,Matrix*)=nullptr;
    void(*cull_matrix)(void*,Matrix*)=nullptr;void(*set_cull_matrix)(void*,Matrix*)=nullptr;
    int view_auto_offset=-1,cull_auto_offset=-1;
    float(*collision)(void*,V3,V3,float)=nullptr;void* collision_context=nullptr;
    bool loaded=false;
    bool load(mango_chams::Api::Resolve resolve,mango_chams::Api& api){
        if(loaded)return true;objects=&api;
#define TP_BIND(member,name) member=reinterpret_cast<decltype(member)>(resolve(name));if(!member)return false
        TP_BIND(position,"UnityEngine.Transform::get_position_Injected(System.IntPtr,UnityEngine.Vector3&)");
        TP_BIND(set_position,"UnityEngine.Transform::set_position_Injected(System.IntPtr,UnityEngine.Vector3&)");
        TP_BIND(rotation,"UnityEngine.Transform::get_rotation_Injected(System.IntPtr,UnityEngine.Quaternion&)");
        TP_BIND(set_rotation,"UnityEngine.Transform::set_rotation_Injected(System.IntPtr,UnityEngine.Quaternion&)");
        TP_BIND(local_scale,"UnityEngine.Transform::get_localScale_Injected(System.IntPtr,UnityEngine.Vector3&)");
        TP_BIND(set_local_scale,"UnityEngine.Transform::set_localScale_Injected(System.IntPtr,UnityEngine.Vector3&)");
        TP_BIND(local_position,"UnityEngine.Transform::get_localPosition_Injected(System.IntPtr,UnityEngine.Vector3&)");
        TP_BIND(set_local_position,"UnityEngine.Transform::set_localPosition_Injected(System.IntPtr,UnityEngine.Vector3&)");
        TP_BIND(mask,"UnityEngine.Camera::get_cullingMask_Injected(System.IntPtr)");
        TP_BIND(set_mask,"UnityEngine.Camera::set_cullingMask_Injected(System.IntPtr,System.Int32)");
        TP_BIND(enabled,"UnityEngine.Renderer::get_enabled_Injected(System.IntPtr)");
        TP_BIND(set_enabled,"UnityEngine.Renderer::set_enabled_Injected(System.IntPtr,System.Boolean)");
        TP_BIND(shadows,"UnityEngine.Renderer::get_shadowCastingMode_Injected(System.IntPtr)");
        TP_BIND(set_shadows,"UnityEngine.Renderer::set_shadowCastingMode_Injected(System.IntPtr,UnityEngine.Rendering.ShadowCastingMode)");
        TP_BIND(render_layers,"UnityEngine.Renderer::get_renderingLayerMask_Injected(System.IntPtr)");
        TP_BIND(set_render_layers,"UnityEngine.Renderer::set_renderingLayerMask_Injected(System.IntPtr,System.UInt32)");
        TP_BIND(layer,"UnityEngine.GameObject::get_layer_Injected(System.IntPtr)");
        TP_BIND(active,"UnityEngine.GameObject::get_activeInHierarchy_Injected(System.IntPtr)");
        TP_BIND(is_child,"UnityEngine.Transform::IsChildOf_Injected(System.IntPtr,System.IntPtr)");
        TP_BIND(parent,"UnityEngine.Transform::GetParent_Injected(System.IntPtr)");
        TP_BIND(game_transform,"UnityEngine.GameObject::get_transform_Injected(System.IntPtr)");
        TP_BIND(active_self,"UnityEngine.GameObject::get_activeSelf_Injected(System.IntPtr)");
        TP_BIND(set_active,"UnityEngine.GameObject::SetActive_Injected(System.IntPtr,System.Boolean)");
        TP_BIND(behaviour,"UnityEngine.Behaviour::get_enabled_Injected(System.IntPtr)");
        TP_BIND(set_behaviour,"UnityEngine.Behaviour::set_enabled_Injected(System.IntPtr,System.Boolean)");
        TP_BIND(culling,"UnityEngine.Animator::get_cullingMode_Injected(System.IntPtr)");
        TP_BIND(set_culling,"UnityEngine.Animator::set_cullingMode_Injected(System.IntPtr,UnityEngine.AnimatorCullingMode)");
        TP_BIND(offscreen,"UnityEngine.SkinnedMeshRenderer::get_updateWhenOffscreen_Injected(System.IntPtr)");
        TP_BIND(set_offscreen,"UnityEngine.SkinnedMeshRenderer::set_updateWhenOffscreen_Injected(System.IntPtr,System.Boolean)");
#undef TP_BIND
        view=reinterpret_cast<decltype(view)>(resolve("UnityEngine.Camera::get_worldToCameraMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
        set_view=reinterpret_cast<decltype(set_view)>(resolve("UnityEngine.Camera::set_worldToCameraMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
        cull_matrix=reinterpret_cast<decltype(cull_matrix)>(resolve("UnityEngine.Camera::get_cullingMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
        set_cull_matrix=reinterpret_cast<decltype(set_cull_matrix)>(resolve("UnityEngine.Camera::set_cullingMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
        view_auto_offset=mango_unity::camera_matrix_auto_offset(reinterpret_cast<uintptr_t>(set_view),api.read);
        cull_auto_offset=mango_unity::camera_matrix_auto_offset(reinterpret_cast<uintptr_t>(set_cull_matrix),api.read);
        loaded=objects->loaded;return loaded;
    }
};
// PlayerView_Local ships with zero-scale neck/arm bones, stretched spine bones,
// and a LateUpdate component that pushes the spine away from a first-person
// camera. Restore those presentation changes without changing animated poses.
class LocalBodyGeometry {
    struct Ref {int32_t id=0;uintptr_t native=0;};
    struct Bone {Ref ref;V3 original{};bool owned=false;};
    struct Offset {Ref bone;unsigned index=0;};
    std::vector<Bone> bones_;std::vector<Offset> offsets_;
    Ref component_{};bool shown_=false,component_owned_=false,component_enabled_=false;
    unsigned endpoints_=0;
    static bool near(V3 a,V3 b){return finite(a)&&finite(b)&&length(sub(a,b))<.00001f;}
    static bool first_person_scale(V3 v){
        return near(v,{0,0,0})||near(v,{1,1.2f,1})||near(v,{.8f,1.2f,1});
    }
    Ref ref(Api& api,void* managed){return {api.objects->id(managed),reinterpret_cast<uintptr_t>(api.objects->native(managed))};}
    void hold(Api& api,Ref r){if(api.objects->retain)api.objects->retain(r.id,r.native);}
    void drop(Api& api,Ref r){if(api.objects->release)api.objects->release(r.id,r.native);}
    void* live(Api& api,Ref r){return api.objects->live_native(r.id,r.native);}
    void undo_offsets(Api& api){
        auto& o=*api.objects;void* managed=o.find(component_.id);
        if(reinterpret_cast<uintptr_t>(o.native(managed))!=component_.native||!o.field<uint8_t>(managed,0x38)||!api.local_position||!api.set_local_position)return;
        void* before=o.field<void*>(managed,0x28);void* after=o.field<void*>(managed,0x30);
        const auto n_before=o.field<uintptr_t>(before,0x18),n_after=o.field<uintptr_t>(after,0x18);
        if(n_before>16||n_after>16)return;
        for(auto offset:offsets_){
            if(offset.index>=n_before||offset.index>=n_after)continue;
            void* p=live(api,offset.bone);if(!p)continue;
            V3 a=o.field<V3>(before,0x20+offset.index*sizeof(V3)),b=o.field<V3>(after,0x20+offset.index*sizeof(V3)),current{};
            api.local_position(p,&current);
            // Animator may already have replaced the position. Only undo a
            // displacement still matching the component's recorded result.
            if(finite(a)&&near(current,b))api.set_local_position(p,&a);
        }
    }
public:
    bool complete()const{return endpoints_==3;}
    void show(Api& api,bool visible){
        if(visible&&!shown_){
            if(void* p=live(api,component_)){
                component_enabled_=api.behaviour(p);component_owned_=component_enabled_;
                if(component_owned_){api.set_behaviour(p,false);undo_offsets(api);}
            }
        }
        if(visible&&api.local_scale&&api.set_local_scale){
            for(auto& bone:bones_)if(void* p=live(api,bone.ref)){
                V3 current{};api.local_scale(p,&current);
                if(first_person_scale(current)){
                    if(!bone.owned)bone.original=current;
                    V3 unit{1,1,1};api.set_local_scale(p,&unit);bone.owned=true;
                }else if(bone.owned&&!near(current,{1,1,1}))bone.owned=false;
            }
        }
        if(!visible){
            for(auto& bone:bones_)if(bone.owned){
                if(void* p=live(api,bone.ref)){V3 current{};api.local_scale(p,&current);if(near(current,{1,1,1}))api.set_local_scale(p,&bone.original);}
                bone.owned=false;
            }
            if(component_owned_)if(void* p=live(api,component_))if(!api.behaviour(p))api.set_behaviour(p,component_enabled_);
            component_owned_=false;
        }
        shown_=visible;
    }
    void reset(Api& api){
        show(api,false);for(auto b:bones_)drop(api,b.ref);for(auto o:offsets_)drop(api,o.bone);drop(api,component_);
        bones_.clear();offsets_.clear();component_={};shown_=component_owned_=component_enabled_=false;endpoints_=0;
    }
    void bind(Api& api,void* animator){
        reset(api);if(!api.bone||!api.local_scale||!api.set_local_scale)return;
        auto& o=*api.objects;void* native=o.native(animator);if(!native)return;
        void* stop=o.native_return(o.transform(native));
        // Head and both hands cover neck, shoulders, upper arms and every
        // spine ancestor, including spine bones absent from the humanoid map.
        for(int human:{10,17,18}){
            void* managed=api.bone(animator,human);
            if(o.native(managed))++endpoints_;
            for(unsigned depth=0;managed&&depth<16&&bones_.size()<24;++depth){
                Ref r=ref(api,managed);void* p=o.native(managed);if(!r.id||!p||p==stop)break;
                if(std::any_of(bones_.begin(),bones_.end(),[&](const Bone& b){return b.ref.id==r.id&&b.ref.native==r.native;}))break;
                V3 original{};api.local_scale(p,&original);if(!finite(original))break;
                hold(api,r);bones_.push_back({r,original,false});managed=o.decode(api.parent(p));
            }
        }
        void* go=o.native_return(o.game_object(native));if(!go||!o.component)return;
        auto name=mango_chams::span(u"HyperHug.Games.Oxide.Features.Player.FirstPersonBodyOffsets");
        void* managed=o.decode(o.component(go,&name));
        if(!o.native(managed)){auto short_name=mango_chams::span(u"FirstPersonBodyOffsets");managed=o.decode(o.component(go,&short_name));}
        component_=ref(api,managed);if(!component_.id)return;hold(api,component_);
        void* array=o.field<void*>(managed,0x20);const auto count=o.field<uintptr_t>(array,0x18);if(count>16)return;
        for(unsigned i=0;i<count;++i){
            Ref r=ref(api,o.field<void*>(array,0x20+i*24));if(!r.id)continue;
            hold(api,r);offsets_.push_back({r,i});
        }
    }
};
class LocalModel {
    struct Ref {int32_t id=0;uintptr_t native=0;};
    struct Active {Ref object;bool original=false;};
    struct Behaviour {Ref object;bool original=false;};
    std::vector<Active> active_;
    std::vector<Behaviour> behaviours_;
    std::vector<Ref> targets_,primary_;
    Ref animator_{};int original_culling_=0;bool visible_=false;
    LocalBodyGeometry geometry_;bool local_=false;
    unsigned geometry_attempts_=0;double next_geometry_=0;
    double retry_=0,next_show_=0;void* info_=nullptr;
    Ref ref(Api& api,void* managed){return {api.objects->id(managed),reinterpret_cast<uintptr_t>(api.objects->native(managed))};}
    void* live(Api& api,Ref r){return api.objects->live_native(r.id,r.native);}
    void hold(Api& api,Ref r){if(api.objects->retain)api.objects->retain(r.id,r.native);}
    void drop(Api& api,Ref r){if(api.objects->release)api.objects->release(r.id,r.native);}
    void activate(Api& api,void* go){
        auto& o=*api.objects;
        for(unsigned depth=0;go&&depth<32;++depth){
            if(!api.active_self(go)){
                void* transform=o.native_return(api.game_transform(go));
                if(!transform)break;
                const Ref r=ref(api,o.decode(o.game_object(transform)));
                // GetComponent returns the Transform; obtain its GameObject's
                // managed identity rather than treating a native pointer as one.
                if(!r.id)break;
                if(std::none_of(active_.begin(),active_.end(),[&](const Active& a){return a.object.id==r.id&&a.object.native==r.native;})){hold(api,r);active_.push_back({r,false});}
                api.set_active(go,true);
            }
            if(api.active(go))break;
            void* transform=o.native_return(api.game_transform(go));
            void* parent=transform?o.native_return(api.parent(transform)):nullptr;
            go=parent?o.native_return(o.game_object(parent)):nullptr;
        }
    }
    void behaviour(Api& api,void* managed){
        Ref r=ref(api,managed);void* p=live(api,r);if(!p)return;
        if(std::none_of(behaviours_.begin(),behaviours_.end(),[&](const Behaviour& b){return b.object.id==r.id&&b.object.native==r.native;})){hold(api,r);behaviours_.push_back({r,api.behaviour(p)});}
    }
    void inspect(Api& api,void* managed,void* model,double time){
        auto& o=*api.objects;void* native=o.native(managed);if(!native)return;
        void* go=o.native_return(o.game_object(native));if(!go)return;
        const Ref root=ref(api,o.decode(o.game_object(native)));if(root.id){hold(api,root);targets_.push_back(root);}
        behaviour(api,managed);
        if(local_){geometry_.bind(api,managed);geometry_attempts_=1;next_geometry_=time+1;}
        if(model&&o.named_ref){
            for(const char* name:{"body","headModel"}){
                void* transform=o.native(o.named_ref(model,name));
                Ref r=ref(api,transform?o.decode(o.game_object(transform)):nullptr);if(r.id){hold(api,r);targets_.push_back(r);}
            }
            for(const char* name:{"skinnedMeshRenderer","headRenderer"}){Ref r=ref(api,o.named_ref(model,name));if(r.id){hold(api,r);primary_.push_back(r);}}
            behaviour(api,o.named_ref(model,"characterAnimation"));
        }
        retry_=time+1;
    }
public:
    void reset(Api& api){
        show(api,false);geometry_.reset(api);local_=false;geometry_attempts_=0;next_geometry_=0;for(auto a:active_)drop(api,a.object);for(auto b:behaviours_)drop(api,b.object);for(auto r:targets_)drop(api,r);for(auto r:primary_)drop(api,r);
        active_.clear();behaviours_.clear();targets_.clear();primary_.clear();animator_={};retry_=next_show_=0;info_=nullptr;
    }
    void show(Api& api,bool visible){
        if(!api.loaded)return;
        if(!visible){
            geometry_.show(api,false);
            for(auto& b:behaviours_)if(void* p=live(api,b.object))if(api.behaviour(p))api.set_behaviour(p,b.original);
            for(auto it=active_.rbegin();it!=active_.rend();++it)if(void* go=live(api,it->object))if(api.active_self(go))api.set_active(go,it->original);
        }else{
            for(auto r:targets_)if(void* go=live(api,r))activate(api,go);
            for(auto b:behaviours_)if(void* p=live(api,b.object))if(!api.behaviour(p))api.set_behaviour(p,true);
            geometry_.show(api,true);
        }
        visible_=visible;
    }
    void update(Api& api,void* managed,void* info,bool visible,double time,bool local=false){
        const uintptr_t native=reinterpret_cast<uintptr_t>(api.objects->native(managed));
        const Ref r=native==animator_.native&&info_==info?animator_:ref(api,managed);
        if(r.id!=animator_.id||r.native!=animator_.native||info_!=info||local_!=local){reset(api);animator_=r;info_=info;local_=local;if(r.id)inspect(api,managed,info,time);}
        // A model's equipment / script may finish initialization after Animator.
        else if(r.id&&primary_.empty()&&time>=retry_){for(auto r:targets_)drop(api,r);targets_.clear();inspect(api,managed,info,time);}
        else if(r.id&&local_&&!geometry_.complete()&&geometry_attempts_<3&&time>=next_geometry_){
            geometry_.bind(api,managed);++geometry_attempts_;next_geometry_=time+1;
        }
        if(visible!=visible_||time>=next_show_){show(api,visible);next_show_=time+.2;}
    }
    void before_animation(Api&){ }
    const auto& primary()const{return primary_;}
    bool has_primary()const{return !primary_.empty();}
};
class Controller {
    struct Renderer {int32_t id=0;uintptr_t native=0;bool enabled=false,fp=false,primary=false,force_off=false;int shadows=0,layer=0;uint32_t render_layers=0,applied_layers=0;bool layers_owned=false,occlusion=false,weapon=false,avatar_hidden=false;};
    std::vector<Renderer> renderers_;
    LocalModel model_;
    int32_t camera_=0,transform_=0;uintptr_t camera_native_=0,transform_native_=0,owner_=0,fp_=0,weapon_=0,animator_=0;
    V3 baseline_{},applied_{};int applied_frame_=-1;bool shifted_=false,mask_owned_=false;
    uint32_t added_camera_layers_=0;double next_refresh_=0,next_flags_=0;bool body_view_=false;
    Matrix original_view_{},applied_view_{},original_cull_{},applied_cull_{};bool view_owned_=false,cull_owned_=false;
    int32_t pass_id_=0;uintptr_t pass_native_=0;bool pass_enabled_=false,pass_owned_=false;
    uint32_t reserved_layers_=0;void* model_info_=nullptr;uintptr_t weapon_visual_=0;bool replacement_=false;
    void camera_layers(Api& api,void* camera,uint32_t required){
        // The camera belongs to the game. Remove only bits we added, then add
        // the local body/avatar bits; never clear a scene/world layer.
        const uint32_t current=uint32_t(api.mask(camera));
        const uint32_t baseline=current&~added_camera_layers_;
        added_camera_layers_=required&~baseline;
        const uint32_t next=baseline|required;
        if(current!=next)api.set_mask(camera,int(next));
        mask_owned_=added_camera_layers_!=0;
    }
    void restore_pass(Api& api){
        if(pass_owned_)if(void* p=api.objects->live_native(pass_id_,pass_native_))if(!api.behaviour(p))api.set_behaviour(p,pass_enabled_);
        if(api.objects->release)api.objects->release(pass_id_,pass_native_);
        pass_id_=0;pass_native_=0;pass_owned_=false;
    }
    void update_pass(Api& api,void* managed,bool visible){
        auto& o=*api.objects;const auto native=reinterpret_cast<uintptr_t>(o.native(managed));
        if(native!=pass_native_){restore_pass(api);pass_id_=o.id(managed);pass_native_=native;if(pass_id_&&o.retain)o.retain(pass_id_,native);}
        void* p=o.live_native(pass_id_,pass_native_);if(!p)return;
        if(visible){if(!pass_owned_){pass_enabled_=api.behaviour(p);pass_owned_=true;}if(api.behaviour(p))api.set_behaviour(p,false);}
        else if(pass_owned_){if(!api.behaviour(p))api.set_behaviour(p,pass_enabled_);pass_owned_=false;}
    }
    void restore_renderer(Api& api,Renderer& r){
        void* p=api.objects->live_native(r.id,r.native);if(!p)return;
        if(api.enabled(p)==(!r.fp&&!r.avatar_hidden))api.set_enabled(p,r.enabled);
        r.avatar_hidden=false;
        if(!r.fp&&r.shadows==3&&api.shadows(p)==1)api.set_shadows(p,r.shadows);
        if(r.layers_owned&&api.render_layers(p)==r.applied_layers)api.set_render_layers(p,r.render_layers);
        r.layers_owned=false;
        if(!r.fp&&api.objects->occlusion&&!api.objects->occlusion(p))api.objects->set_occlusion(p,r.occlusion);
        if(r.primary&&api.objects->force_off_offset>=0&&!api.objects->field<uint8_t>(p,size_t(api.objects->force_off_offset)))api.objects->set_force_off(p,r.force_off);
    }
    void add_renderer(Api& api,int32_t id,bool first_person,bool primary){
        auto& o=*api.objects;
        if(!id||renderers_.size()>=32||std::any_of(renderers_.begin(),renderers_.end(),[&](const Renderer& r){return r.id==id;}))return;
        void* p=o.live(id);if(!p)return;void* go=o.native_return(o.game_object(p));if(!go)return;
        const bool enabled=api.enabled(p);const bool force=o.force_off_offset>=0&&o.field<uint8_t>(p,size_t(o.force_off_offset));
        if(!primary&&!first_person&&(!enabled||force||!api.active(go)))return;
        const int layer=api.layer(go);
        // An explicitly bound body renderer takes precedence over a layer that
        // was assigned by the game's first-person setup.
        renderers_.push_back({id,reinterpret_cast<uintptr_t>(p),enabled,!primary&&first_person,primary,force,api.shadows(p),layer});
        auto& r=renderers_.back();r.render_layers=api.render_layers?api.render_layers(p):0;
        r.occlusion=o.occlusion&&o.occlusion(p);
        if(!r.fp&&model_info_&&o.named_ref&&api.is_child){
            void* tr=o.native_return(o.transform(p));
            for(const char* name:{"rightWeaponHolder","leftWeaponHolder"}){
                void* holder=o.native(o.named_ref(model_info_,name));
                if(tr&&holder&&(tr==holder||api.is_child(tr,holder)))r.weapon=true;
            }
        }
        if(o.retain)o.retain(id,reinterpret_cast<uintptr_t>(p));
    }
    void collect(Api& api,void* root,bool fp,bool layer_only=false){
        auto& o=*api.objects;void* array=o.collect(root);const uintptr_t count=o.field<uintptr_t>(array,0x18);
        if(count>256)return;
        for(size_t i=0;i<count;++i){void* object=o.item(array,int(i));if(!object||(o.is_mesh&&!o.is_mesh(object)))continue;
            if(layer_only){void* p=o.native(object);void* go=p?o.native_return(o.game_object(p)):nullptr;if(!go||api.layer(go)!=8)continue;}
            add_renderer(api,o.id(object),fp,false);}
    }
public:
    int status=0;unsigned body_meshes=0;
    uintptr_t native_transform()const{return transform_native_;}
    int frame()const{return applied_frame_;}
    bool shifted()const{return shifted_;}
    bool body_visible()const{return body_view_;}
    bool selected(Api& api,void* camera)const{return reinterpret_cast<uintptr_t>(api.objects->native(camera))==camera_native_;}
    void replace_body(Api& api,bool enabled){
        replacement_=enabled;
        if(!body_view_||!api.loaded)return;
        uint32_t layers=0;
        for(auto& r:renderers_)if(!r.fp)if(void* p=api.objects->live_native(r.id,r.native)){
            r.avatar_hidden=replacement_&&!r.weapon;
            if(api.enabled(p)==r.avatar_hidden)api.set_enabled(p,!r.avatar_hidden);
            if(!r.avatar_hidden&&r.layer>=0&&r.layer<32)layers|=uint32_t(1)<<r.layer;
        }
        if(void* camera=api.objects->live_native(camera_,camera_native_)){
            camera_layers(api,camera,layers|(replacement_?1u:0u));
        }
    }
    void before_cull(Api& api,void* managed_camera){
        if(!api.loaded||!body_view_||!shifted_||reinterpret_cast<uintptr_t>(api.objects->native(managed_camera))!=camera_native_)return;
        model_.show(api,true);
        for(auto& r:renderers_)if(void* p=api.objects->live_native(r.id,r.native)){
            r.avatar_hidden=!r.fp&&replacement_&&!r.weapon;
            const bool visible=!r.fp&&!r.avatar_hidden;
            if(api.enabled(p)!=visible)api.set_enabled(p,visible);
            if(r.fp)continue;
            // Unity's ShadowsOnly is 3; 2 is TwoSided. Apply to equipment too.
            if(api.shadows(p)==3)api.set_shadows(p,1);
            if(r.primary&&api.objects->force_off_offset>=0&&api.objects->field<uint8_t>(p,size_t(api.objects->force_off_offset)))api.objects->set_force_off(p,false);
            if(api.objects->occlusion&&api.objects->occlusion(p))api.objects->set_occlusion(p,false);
            if(api.render_layers&&api.set_render_layers){
                const uint32_t current=api.render_layers(p);
                if(!r.layers_owned||current!=r.applied_layers)r.render_layers=current;
                r.applied_layers=(current&~reserved_layers_)|1u;
                if(current!=r.applied_layers)api.set_render_layers(p,r.applied_layers);
                r.layers_owned=true;
            }
        }
    }
    void before_animation(Api&){ }
    V3 offset()const{return sub(applied_,baseline_);}
    V3 undo_ray_origin(V3 origin)const{const float d=length(sub(origin,applied_));return shifted_&&d<.1f&&d<length(sub(origin,baseline_))?sub(origin,offset()):origin;}
    void begin_frame(Api& api,int frame){
        if(!shifted_||frame==applied_frame_||!api.loaded)return;
        if(void* p=api.objects->live_native(transform_,transform_native_)){
            V3 current{};api.position(p,&current);if(finite(current)&&length(sub(current,applied_))<.003f)api.set_position(p,&baseline_);
        }
        if(void* camera=api.objects->live_native(camera_,camera_native_)){
            Matrix current{};
            if(view_owned_&&api.view&&api.set_view){api.view(camera,&current);if(near(current,applied_view_))api.set_view(camera,&original_view_);}
            if(cull_owned_&&api.cull_matrix&&api.set_cull_matrix){api.cull_matrix(camera,&current);if(near(current,applied_cull_))api.set_cull_matrix(camera,&original_cull_);}
        }
        view_owned_=cull_owned_=false;
        shifted_=false;
    }
    void reset(Api& api){
        if(api.loaded){
            begin_frame(api,applied_frame_-1);
            if(void* p=api.objects->live_native(camera_,camera_native_))if(mask_owned_)camera_layers(api,p,0);
            for(auto& r:renderers_){restore_renderer(api,r);if(api.objects->release)api.objects->release(r.id,r.native);}model_.reset(api);
            restore_pass(api);
            if(api.objects->release){api.objects->release(camera_,camera_native_);api.objects->release(transform_,transform_native_);}
        }
        renderers_.clear();camera_=transform_=0;camera_native_=transform_native_=owner_=fp_=weapon_=animator_=0;
        shifted_=mask_owned_=body_view_=view_owned_=cull_owned_=false;next_refresh_=next_flags_=0;status=0;body_meshes=0;
        added_camera_layers_=reserved_layers_=0;model_info_=nullptr;weapon_visual_=0;replacement_=false;
    }
    void update(Api& api,void* managed_camera,void* owner,void* fp,void* weapon,bool enabled,float distance,int frame,double time,void* managed_animator=nullptr,void* model_info=nullptr,void* render_controller=nullptr,uint32_t reserved_layers=0,bool local_model=false,uintptr_t weapon_visual=0){
        if(!enabled||!api.loaded||!managed_camera||!owner){reset(api);return;}
        if(!std::isfinite(distance)){reset(api);status=-2;return;}
        auto& o=*api.objects;void* camera=o.native(managed_camera);void* animator=o.native(managed_animator);
        if(!animator||!model_info){reset(api);status=-3;return;}
        if(camera_native_!=reinterpret_cast<uintptr_t>(camera)||owner_!=reinterpret_cast<uintptr_t>(owner)){
            reset(api);camera_=o.id(managed_camera);camera_native_=reinterpret_cast<uintptr_t>(camera);owner_=reinterpret_cast<uintptr_t>(owner);
            void* t=o.decode(o.transform(camera));transform_=o.id(t);transform_native_=reinterpret_cast<uintptr_t>(o.native(t));
            if(!camera_||!transform_){reset(api);status=-1;return;}
            if(o.retain){o.retain(camera_,camera_native_);o.retain(transform_,transform_native_);}
        }
        begin_frame(api,frame);if(shifted_&&applied_frame_==frame)return;
        if(animator_!=reinterpret_cast<uintptr_t>(animator)||fp_!=reinterpret_cast<uintptr_t>(fp)||weapon_!=reinterpret_cast<uintptr_t>(weapon)||model_info_!=model_info||weapon_visual_!=weapon_visual){
            for(auto& r:renderers_){restore_renderer(api,r);if(o.release)o.release(r.id,r.native);}renderers_.clear();next_refresh_=next_flags_=0;
            animator_=reinterpret_cast<uintptr_t>(animator);fp_=reinterpret_cast<uintptr_t>(fp);weapon_=reinterpret_cast<uintptr_t>(weapon);
            model_info_=model_info;
            weapon_visual_=weapon_visual;
        }
        reserved_layers_=reserved_layers;
        void* transform=o.live_native(transform_,transform_native_);if(!transform){reset(api);status=-1;return;}
        V3 eye{};Quaternion q{};api.position(transform,&eye);api.rotation(transform,&q);
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        if(!finite(eye)||!std::isfinite(norm)||norm<.5f||norm>1.5f){reset(api);status=-2;return;}
        const float inv=1.f/std::sqrt(norm);q={q.x*inv,q.y*inv,q.z*inv,q.w*inv};
        const V3 delta=add(scale(rotate(q,{0,0,1}),-std::clamp(distance,.8f,5.f)),add(scale(rotate(q,{1,0,0}),.3f),scale(rotate(q,{0,1,0}),.15f)));
        const float wanted=length(delta);const V3 direction=scale(delta,1.f/wanted);
        float allowed=api.collision?api.collision(api.collision_context,eye,direction,wanted):wanted;
        if(!std::isfinite(allowed))allowed=0;const float travel=std::clamp(allowed,0.f,wanted);const bool body_view=travel>.65f;
        // OnDisable restores temporary origin shifts. Read the eye again after
        // switching; do not toggle the pass twice per frame next to a wall.
        update_pass(api,render_controller,body_view);
        api.position(transform,&eye);if(!finite(eye)){reset(api);status=-2;return;}
        model_.update(api,managed_animator,model_info,body_view,time,local_model);
        for(auto r:model_.primary())if(o.live_native(r.id,r.native))add_renderer(api,r.id,false,true);
        if(time>=next_refresh_){
            collect(api,animator,false);
            // Query the separate view-model / weapon branches only. Never walk
            // every child of the player and enable every LOD.
            void* owner_t=o.native_return(o.transform(owner));void* fp_t=fp?o.native_return(o.transform(fp)):nullptr;
            if(fp&&fp_t!=owner_t)collect(api,fp,true);
            collect(api,owner,true,true);
            if(weapon)collect(api,weapon,true);
            next_refresh_=time+2;
        }
        if(body_view!=body_view_||time>=next_flags_){
            body_meshes=0;uint32_t layers=0;
            for(auto it=renderers_.begin();it!=renderers_.end();){
                void* p=o.live_native(it->id,it->native);if(!p){if(o.release)o.release(it->id,it->native);it=renderers_.erase(it);continue;}
                if(!body_view){restore_renderer(api,*it);++it;continue;}
                it->avatar_hidden=!it->fp&&replacement_&&!it->weapon;
                const bool visible=!it->fp&&!it->avatar_hidden;
                if(api.enabled(p)!=visible)api.set_enabled(p,visible);
                if(visible){
                    if(it->primary&&o.force_off_offset>=0&&o.field<uint8_t>(p,size_t(o.force_off_offset)))o.set_force_off(p,false);
                    if(it->layer>=0&&it->layer<32)layers|=uint32_t(1)<<it->layer;++body_meshes;
                }
                ++it;
            }
            camera_layers(api,camera,body_view?(layers|(replacement_?1u:0u)):0);
            body_view_=body_view;next_flags_=time+.2;
        }
        baseline_=eye;applied_=add(eye,scale(direction,travel));
        const bool use_view=travel>.01f&&api.view&&api.set_view&&api.view_auto_offset>=0&&o.field<uint8_t>(camera,size_t(api.view_auto_offset))==0;
        const bool use_cull=travel>.01f&&api.cull_matrix&&api.set_cull_matrix&&api.cull_auto_offset>=0&&o.field<uint8_t>(camera,size_t(api.cull_auto_offset))==0;
        if(use_view)api.view(camera,&original_view_);if(use_cull)api.cull_matrix(camera,&original_cull_);
        api.set_position(transform,&applied_);
        // A custom game view/culling matrix does not follow Transform.position.
        // Adjust only matrices that failed to follow automatically; restore
        // their exact prior values next frame, preserving external changes.
        const V3 offset=sub(applied_,baseline_);Matrix current{};
        if(use_view&&finite(original_view_)){
            applied_view_=translate_view(original_view_,offset);api.view(camera,&current);
            if(!near(current,applied_view_)){api.set_view(camera,&applied_view_);view_owned_=true;}
        }
        if(use_cull&&finite(original_cull_)){
            applied_cull_=translate_view(original_cull_,offset);api.cull_matrix(camera,&current);
            if(!near(current,applied_cull_)){api.set_cull_matrix(camera,&applied_cull_);cull_owned_=true;}
        }
        shifted_=true;applied_frame_=frame;status=1;
        before_cull(api,managed_camera);
    }
};
} // namespace mango_camera
