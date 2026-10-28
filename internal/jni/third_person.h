#pragma once
#include "chams.h"
#include <cmath>
#include <cstdio>

namespace mango_camera {
constexpr bool kCameraOcclusionFallback=false;
struct V3 {float x=0,y=0,z=0;};
struct Quaternion {float x=0,y=0,z=0,w=1;};
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
    void(*rotation)(void*,Quaternion*)=nullptr;
    int(*mask)(void*)=nullptr;void(*set_mask)(void*,int)=nullptr;
    bool(*enabled)(void*)=nullptr;void(*set_enabled)(void*,bool)=nullptr;
    int(*shadows)(void*)=nullptr;void(*set_shadows)(void*,int)=nullptr;
    int(*layer)(void*)=nullptr;bool(*active)(void*)=nullptr;
    bool(*is_child)(void*,void*)=nullptr;
    void*(*parent)(void*)=nullptr;
    void*(*game_transform)(void*)=nullptr;
    bool(*active_self)(void*)=nullptr;void(*set_active)(void*,bool)=nullptr;
    bool(*behaviour)(void*)=nullptr;void(*set_behaviour)(void*,bool)=nullptr;
    int(*culling)(void*)=nullptr;void(*set_culling)(void*,int)=nullptr;
    bool(*offscreen)(void*)=nullptr;void(*set_offscreen)(void*,bool)=nullptr;
    float(*collision)(void*,V3,V3,float)=nullptr;void* collision_context=nullptr;
    // Optional, diagnostics only: why an enabled renderer is still not drawn.
    bool(*is_visible)(void*)=nullptr;
    void(*bounds)(void*,float*)=nullptr;   // center xyz, extents xyz
    void(*lossy_scale)(void*,V3*)=nullptr;
    void*(*shared_mesh)(void*)=nullptr;
    bool(*camera_occlusion)(void*)=nullptr;void(*set_camera_occlusion)(void*,bool)=nullptr;
    // A game that sets Camera.cullingMatrix (floating-origin / first-person tricks) makes the
    // engine cull with a frustum that does not follow our shifted camera.
    void(*reset_culling)(void*)=nullptr;
    void(*culling_matrix)(void*,float*)=nullptr,(*view_matrix)(void*,float*)=nullptr,(*projection_matrix)(void*,float*)=nullptr;
    bool loaded=false;
    bool load(mango_chams::Api::Resolve resolve,mango_chams::Api& api){
        if(loaded)return true;objects=&api;
#define TP_BIND(member,name) member=reinterpret_cast<decltype(member)>(resolve(name));if(!member){if(api.log)api.log("missing icall: " name);return false;}
        TP_BIND(position,"UnityEngine.Transform::get_position_Injected(System.IntPtr,UnityEngine.Vector3&)");
        TP_BIND(set_position,"UnityEngine.Transform::set_position_Injected(System.IntPtr,UnityEngine.Vector3&)");
        TP_BIND(rotation,"UnityEngine.Transform::get_rotation_Injected(System.IntPtr,UnityEngine.Quaternion&)");
        TP_BIND(mask,"UnityEngine.Camera::get_cullingMask_Injected(System.IntPtr)");
        TP_BIND(set_mask,"UnityEngine.Camera::set_cullingMask_Injected(System.IntPtr,System.Int32)");
        TP_BIND(enabled,"UnityEngine.Renderer::get_enabled_Injected(System.IntPtr)");
        TP_BIND(set_enabled,"UnityEngine.Renderer::set_enabled_Injected(System.IntPtr,System.Boolean)");
        TP_BIND(shadows,"UnityEngine.Renderer::get_shadowCastingMode_Injected(System.IntPtr)");
        TP_BIND(set_shadows,"UnityEngine.Renderer::set_shadowCastingMode_Injected(System.IntPtr,UnityEngine.Rendering.ShadowCastingMode)");
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
        is_visible=reinterpret_cast<decltype(is_visible)>(resolve("UnityEngine.Renderer::get_isVisible_Injected(System.IntPtr)"));
        bounds=reinterpret_cast<decltype(bounds)>(resolve("UnityEngine.Renderer::get_bounds_Injected(System.IntPtr,UnityEngine.Bounds&)"));
        lossy_scale=reinterpret_cast<decltype(lossy_scale)>(resolve("UnityEngine.Transform::get_lossyScale_Injected(System.IntPtr,UnityEngine.Vector3&)"));
        shared_mesh=reinterpret_cast<decltype(shared_mesh)>(resolve("UnityEngine.SkinnedMeshRenderer::get_sharedMesh_Injected(System.IntPtr)"));
        camera_occlusion=reinterpret_cast<decltype(camera_occlusion)>(resolve("UnityEngine.Camera::get_useOcclusionCulling_Injected(System.IntPtr)"));
        set_camera_occlusion=reinterpret_cast<decltype(set_camera_occlusion)>(resolve("UnityEngine.Camera::set_useOcclusionCulling_Injected(System.IntPtr,System.Boolean)"));
        reset_culling=reinterpret_cast<decltype(reset_culling)>(resolve("UnityEngine.Camera::ResetCullingMatrix_Injected(System.IntPtr)"));
        culling_matrix=reinterpret_cast<decltype(culling_matrix)>(resolve("UnityEngine.Camera::get_cullingMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
        view_matrix=reinterpret_cast<decltype(view_matrix)>(resolve("UnityEngine.Camera::get_worldToCameraMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
        projection_matrix=reinterpret_cast<decltype(projection_matrix)>(resolve("UnityEngine.Camera::get_nonJitteredProjectionMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
#undef TP_BIND
        loaded=objects->loaded;return loaded;
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
    double retry_=0;
    Ref ref(Api& api,void* managed){return {api.objects->id(managed),reinterpret_cast<uintptr_t>(api.objects->native(managed))};}
    void* live(Api& api,Ref r){void* p=api.objects->live(r.id);return reinterpret_cast<uintptr_t>(p)==r.native?p:nullptr;}
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
                if(std::none_of(active_.begin(),active_.end(),[&](const Active& a){return a.object.id==r.id&&a.object.native==r.native;}))active_.push_back({r,false});
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
        if(std::none_of(behaviours_.begin(),behaviours_.end(),[&](const Behaviour& b){return b.object.id==r.id&&b.object.native==r.native;}))behaviours_.push_back({r,api.behaviour(p)});
    }
    void inspect(Api& api,void* managed,double time){
        auto& o=*api.objects;void* native=o.native(managed);if(!native)return;
        void* go=o.native_return(o.game_object(native));if(!go)return;
        Ref r=ref(api,o.decode(o.game_object(native)));if(r.id)targets_.push_back(r);
        behaviour(api,managed);
        auto model_name=mango_chams::span(u"PlayerModelInfo");void* model=o.decode(o.component(go,&model_name));
        if(!model){auto full=mango_chams::span(u"HyperHug.Games.Oxide.Features.Player.PlayerModelInfo");model=o.decode(o.component(go,&full));}
        if(model&&o.named_ref){
            for(const char* name:{"body","headModel"}){
                void* transform=o.native(o.named_ref(model,name));
                Ref r=ref(api,transform?o.decode(o.game_object(transform)):nullptr);if(r.id)targets_.push_back(r);
            }
            for(const char* name:{"skinnedMeshRenderer","headRenderer"}){Ref r=ref(api,o.named_ref(model,name));if(r.id)primary_.push_back(r);}
            behaviour(api,o.named_ref(model,"characterAnimation"));
        }
        auto animation=mango_chams::span(u"CharacterAnimation");void* script=o.decode(o.component(go,&animation));
        if(!script){auto full=mango_chams::span(u"HyperHug.Games.Oxide.Features.Player.CharacterAnimation");script=o.decode(o.component(go,&full));}
        behaviour(api,script);
        retry_=time+1;
    }
public:
    void reset(Api& api){
        show(api,false);active_.clear();behaviours_.clear();targets_.clear();primary_.clear();animator_={};retry_=0;
    }
    void show(Api& api,bool visible){
        if(!api.loaded)return;
        if(!visible){
            for(auto& b:behaviours_)if(void* p=live(api,b.object))if(api.behaviour(p))api.set_behaviour(p,b.original);
            if(void* p=live(api,animator_))if(api.culling(p)==0)api.set_culling(p,original_culling_);
            for(auto it=active_.rbegin();it!=active_.rend();++it)if(void* go=live(api,it->object))if(api.active_self(go))api.set_active(go,it->original);
        }else{
            for(auto r:targets_)if(void* go=live(api,r))activate(api,go);
            for(auto b:behaviours_)if(void* p=live(api,b.object))if(!api.behaviour(p))api.set_behaviour(p,true);
            if(void* p=live(api,animator_))if(api.culling(p)!=0)api.set_culling(p,0);
        }
        visible_=visible;
    }
    void update(Api& api,void* managed,bool visible,double time){
        const Ref r=ref(api,managed);
        if(r.id!=animator_.id||r.native!=animator_.native){reset(api);animator_=r;if(r.id){original_culling_=api.culling(api.objects->native(managed));inspect(api,managed,time);}}
        // A model's equipment / script may finish initialization after Animator.
        else if(r.id&&primary_.empty()&&time>=retry_){targets_.clear();inspect(api,managed,time);}
        show(api,visible);
    }
    void before_animation(Api& api){if(visible_)show(api,true);}
    // Native GameObjects of the model targets (animator root, body, head).
    template<class F>void each_object(Api& api,F f){for(auto r:targets_)if(void* go=live(api,r))f(go);}
    const auto& primary()const{return primary_;}
    bool has_primary()const{return !primary_.empty();}
};
class Controller {
    struct Renderer {
        int32_t id=0;uintptr_t native=0;
        bool enabled=false,fp=false,skinned=false,offscreen=false,force_off=false;
        bool occlusion=true,occlusion_owned=false;
        int shadows=0,layer=0;
        uintptr_t go=0; // a Renderer never changes GameObject; saves 3 calls per frame
    };
    std::vector<Renderer> renderers_;
    LocalModel model_;
    std::deque<int32_t> queue_;std::unordered_set<int32_t> visited_;
    bool fp_root_known_=false,camera_occlusion_owned_=false,camera_occlusion_original_=true;
    double invisible_since_=-1;
    float cull_diff_=-1;unsigned culling_resets_=0;
    char report_[768]{},detail_[1800]{};bool report_ready_=false;double next_report_=0;
    int32_t camera_=0,transform_=0;
    uintptr_t camera_native_=0,transform_native_=0,owner_=0,fp_=0,weapon_=0,animator_=0;
    uintptr_t supplied_animator_=0,fallback_animator_native_=0;int32_t fallback_animator_=0;
    V3 baseline_{},applied_{};
    int applied_frame_=-1;
    bool shifted_=false,mask_owned_=false;
    int original_mask_=0,applied_mask_=0;
    double next_scan_=0;
    void restore_renderer(Api& api,Renderer& r){
        void* p=api.objects->live(r.id);if(reinterpret_cast<uintptr_t>(p)!=r.native)return;
        const bool show=!r.fp;
        if(api.enabled(p)==show)api.set_enabled(p,r.enabled);
        if(!r.fp&&api.shadows(p)==1)api.set_shadows(p,r.shadows);
        if(!r.fp&&r.skinned&&api.offscreen(p))api.set_offscreen(p,r.offscreen);
        if(!r.fp&&r.occlusion_owned&&api.objects->occlusion&&!api.objects->occlusion(p))api.objects->set_occlusion(p,r.occlusion);
        if(!r.fp&&api.objects->force_off_offset>=0&&!api.objects->field<uint8_t>(p,size_t(api.objects->force_off_offset)))api.objects->set_force_off(p,r.force_off);
    }
    void add_renderer(Api& api,int32_t id,bool fp,bool skinned){
        auto& o=*api.objects;
        if(!id||renderers_.size()>=64||std::any_of(renderers_.begin(),renderers_.end(),[&](const Renderer& r){return r.id==id;}))return;
        void* native=o.live(id);if(!native)return;
        void* go=o.native_return(o.game_object(native));if(!go)return;
        const int layer=api.layer(go);
        renderers_.push_back({id,reinterpret_cast<uintptr_t>(native),api.enabled(native),fp,skinned,skinned?api.offscreen(native):false,
            o.force_off_offset>=0?o.field<uint8_t>(native,size_t(o.force_off_offset))!=0:false,
            o.occlusion?o.occlusion(native):true,false,api.shadows(native),layer,reinterpret_cast<uintptr_t>(go)});
    }
    void discover(Api& api,void* owner,void* fp,void* weapon,void* animator,double time){
        auto& objects=*api.objects;
        void* root=objects.native_return(objects.transform(owner));
        void* fp_root=fp?objects.native_return(objects.transform(fp)):nullptr;
        void* weapon_root=weapon?objects.native_return(objects.transform(weapon)):nullptr;
        if(fp_root==root)fp_root=nullptr;
        if(weapon_root==root)weapon_root=nullptr;
        fp_root_known_=fp_root!=nullptr;
        if(queue_.empty()&&time>=next_scan_){
            // A model that already has renderers is rescanned rarely (armor
            // swaps); a scan that found nothing is retried sooner.
            visited_.clear();next_scan_=time+(renderers_.empty()?.75:4.);
            const int32_t id=objects.returned_id(objects.transform(owner));if(id)queue_.push_back(id);
            if(animator){const int32_t id=objects.returned_id(objects.transform(animator));if(id)queue_.push_front(id);}
            // Prioritize the view-model branch on enable / weapon changes.
            if(fp){const int32_t id=objects.returned_id(objects.transform(fp));if(id)queue_.push_front(id);}
            if(weapon){const int32_t id=objects.returned_id(objects.transform(weapon));if(id)queue_.push_front(id);}
        }
        // Wall-clock budget: this runs on the render-critical main thread
        // every frame while a scan is in flight.
        const double deadline=objects.deadline(.0006);
        // Layer 8 alone does not make a mesh a viewmodel: the local body may be
        // drawn from it too. Meshes under the character model are body.
        void* model_root=animator?objects.native_return(objects.transform(animator)):nullptr;
        unsigned budget=64;
        while(budget--&&!queue_.empty()&&visited_.size()<768&&!objects.expired(deadline)){
            const int32_t id=queue_.front();queue_.pop_front();
            if(!visited_.insert(id).second)continue;
            void* t=objects.live(id);if(!t)continue;
            void* go=objects.native_return(objects.game_object(t));if(!go)continue;
            if(!model_.has_primary()&&!fallback_animator_){
                auto animator_name=mango_chams::span(u"Animator");void* value=objects.component(go,&animator_name);
                if(value){
                    auto model_name=mango_chams::span(u"PlayerModelInfo");void* model=objects.component(go,&model_name);
                    if(!model){auto full=mango_chams::span(u"HyperHug.Games.Oxide.Features.Player.PlayerModelInfo");model=objects.component(go,&full);}
                    if(model){fallback_animator_=objects.returned_id(value);fallback_animator_native_=reinterpret_cast<uintptr_t>(objects.native_return(value));}
                }
            }
            const bool in_model=model_root&&(t==model_root||api.is_child(t,model_root));
            const bool first_person=(fp_root&&(t==fp_root||api.is_child(t,fp_root)))||
                (weapon_root&&(t==weapon_root||api.is_child(t,weapon_root)))||
                (!in_model&&api.layer(go)==8);
            for(bool skinned:{true,false})add_renderer(api,objects.renderer_id(go,skinned),first_person,skinned);
            const int n=std::clamp(objects.child_count(t),0,64);
            for(int i=0;i<n&&queue_.size()+visited_.size()<768;++i){const int32_t child=objects.returned_id(objects.child(t,i));if(child)queue_.push_back(child);}
        }
    }
public:
    int status=0;unsigned body_meshes=0;
    uintptr_t native_transform()const{return transform_native_;}
    int frame()const{return applied_frame_;}
    bool shifted()const{return shifted_;}
    void before_animation(Api& api){model_.before_animation(api);}
    V3 offset()const{return sub(applied_,baseline_);}
    V3 undo_ray_origin(V3 origin)const{
        const float from_applied=length(sub(origin,applied_));
        return shifted_&&from_applied<.1f&&from_applied<length(sub(origin,baseline_))?sub(origin,offset()):origin;
    }
    void begin_frame(Api& api,int frame){
        if(!shifted_||frame==applied_frame_||!api.loaded)return;
        void* transform=api.objects->live(transform_);
        if(reinterpret_cast<uintptr_t>(transform)==transform_native_){
            V3 current{};api.position(transform,&current);
            if(finite(current)&&length(sub(current,applied_))<.003f)api.set_position(transform,&baseline_);
        }
        shifted_=false;
    }
    void reset(Api& api){
        if(api.loaded){
            begin_frame(api,applied_frame_-1);
            void* camera=api.objects->live(camera_);
            if(mask_owned_&&reinterpret_cast<uintptr_t>(camera)==camera_native_&&api.mask(camera)==applied_mask_)api.set_mask(camera,original_mask_);
            if(camera_occlusion_owned_&&reinterpret_cast<uintptr_t>(camera)==camera_native_&&api.camera_occlusion&&!api.camera_occlusion(camera))api.set_camera_occlusion(camera,camera_occlusion_original_);
            for(auto& renderer:renderers_)restore_renderer(api,renderer);
            model_.reset(api);
        }
        renderers_.clear();queue_.clear();visited_.clear();camera_=transform_=0;
        camera_native_=transform_native_=owner_=fp_=weapon_=animator_=0;shifted_=mask_owned_=false;next_scan_=0;status=0;body_meshes=0;
        report_ready_=false;next_report_=0;fp_root_known_=false;camera_occlusion_owned_=false;invisible_since_=-1;
        supplied_animator_=fallback_animator_native_=0;fallback_animator_=0;
    }
    void update(Api& api,void* managed_camera,void* owner,void* fp,void* weapon,bool enabled,float distance,int frame,double time,void* managed_animator=nullptr){
        if(!enabled||!api.loaded||!managed_camera||!owner){reset(api);return;}
        if(!std::isfinite(distance)){reset(api);status=-2;return;}
        auto& objects=*api.objects;
        const int32_t camera_id=objects.id(managed_camera);
        void* camera=objects.native(managed_camera);
        if(!camera_id||!camera){reset(api);status=-1;return;}
        void* managed_transform=objects.decode(objects.transform(camera));
        void* transform=objects.native(managed_transform);
        const int32_t transform_id=objects.id(managed_transform);
        if(!camera_id||!transform||!transform_id){reset(api);status=-1;return;}
        if(camera_!=camera_id||owner_!=reinterpret_cast<uintptr_t>(owner))reset(api);
        const uintptr_t supplied=reinterpret_cast<uintptr_t>(objects.native(managed_animator));
        if(supplied!=supplied_animator_){supplied_animator_=supplied;fallback_animator_=0;fallback_animator_native_=0;}
        if(fallback_animator_){
            if(reinterpret_cast<uintptr_t>(objects.live(fallback_animator_))==fallback_animator_native_)managed_animator=objects.find(fallback_animator_);
            else{fallback_animator_=0;fallback_animator_native_=0;}
        }
        begin_frame(api,frame);
        camera_=camera_id;camera_native_=reinterpret_cast<uintptr_t>(camera);transform_=transform_id;
        transform_native_=reinterpret_cast<uintptr_t>(transform);owner_=reinterpret_cast<uintptr_t>(owner);
        if(shifted_&&applied_frame_==frame)return;
        void* animator=objects.native(managed_animator);
        if(fp_!=reinterpret_cast<uintptr_t>(fp)||weapon_!=reinterpret_cast<uintptr_t>(weapon)||animator_!=reinterpret_cast<uintptr_t>(animator)){
            for(auto& r:renderers_)restore_renderer(api,r);renderers_.clear();queue_.clear();visited_.clear();next_scan_=0;
            fp_=reinterpret_cast<uintptr_t>(fp);weapon_=reinterpret_cast<uintptr_t>(weapon);
            animator_=reinterpret_cast<uintptr_t>(animator);
        }
        V3 position{};Quaternion q{};api.position(transform,&position);api.rotation(transform,&q);
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        if(!finite(position)||!std::isfinite(norm)||norm<.5f||norm>1.5f){reset(api);status=-2;return;}
        const float inv=1.f/std::sqrt(norm);q={q.x*inv,q.y*inv,q.z*inv,q.w*inv};
        const V3 forward=rotate(q,{0,0,1}),right=rotate(q,{1,0,0}),up=rotate(q,{0,1,0});
        V3 delta=add(scale(forward,-std::clamp(distance,.8f,5.f)),add(scale(right,.3f),scale(up,.15f)));
        const float wanted=length(delta);const V3 direction=scale(delta,1.f/wanted);
        float allowed=api.collision?api.collision(api.collision_context,position,direction,wanted):wanted;
        if(!std::isfinite(allowed))allowed=0;
        const float travel=std::clamp(allowed,0.f,wanted);
        baseline_=position;applied_=add(position,scale(direction,travel));
        api.set_position(transform,&applied_);shifted_=true;applied_frame_=frame;
        // Measure, then drop any custom culling matrix: cull with the frustum of the pose we render from.
        if(api.culling_matrix&&api.view_matrix&&api.projection_matrix&&time>=next_report_){
            float c[16]{},v[16]{},pr[16]{};api.culling_matrix(camera,c);api.view_matrix(camera,v);api.projection_matrix(camera,pr);
            float worst=0;
            for(int col=0;col<4;++col)for(int row=0;row<4;++row){
                float sum=0;for(int k=0;k<4;++k)sum+=pr[k*4+row]*v[col*4+k];
                worst=std::max(worst,std::fabs(sum-c[col*4+row]));
            }
            cull_diff_=worst;
        }
        if(api.reset_culling){api.reset_culling(camera);++culling_resets_;}
        // Show the body whenever the camera has really left the head; a wall
        // directly behind the player keeps the first-person view.
        const bool body_view=travel>.5f;
        fp_root_known_=fp!=nullptr;
        model_.update(api,managed_animator,body_view,time);
        for(auto r:model_.primary())if(reinterpret_cast<uintptr_t>(objects.live(r.id))==r.native)add_renderer(api,r.id,false,true);
        discover(api,owner,fp,weapon,animator,time);body_meshes=0;
        uint32_t layers=0;bool body_on_fp_layer=false;unsigned drawn=0;
        for(auto it=renderers_.begin();it!=renderers_.end();){
            void* p=objects.live(it->id);
            if(reinterpret_cast<uintptr_t>(p)!=it->native){it=renderers_.erase(it);continue;}
            void* go=reinterpret_cast<void*>(it->go);
            if(!body_view||!api.active(go)){restore_renderer(api,*it);++it;continue;}
            const bool visible=!it->fp;
            if(api.enabled(p)!=visible)api.set_enabled(p,visible);
            if(visible){
                if(objects.force_off_offset>=0&&objects.field<uint8_t>(p,size_t(objects.force_off_offset)))objects.set_force_off(p,false);
                if(api.shadows(p)==3)api.set_shadows(p,1); // ShadowCastingMode: 0 Off, 1 On, 2 TwoSided, 3 ShadowsOnly
                if(it->skinned&&!api.offscreen(p))api.set_offscreen(p,true);
                // The device log showed exactly the renderers with dynamic occlusion
                // enabled (isVisible=0) being culled once the camera left the head, while
                // the ones without it stayed visible: baked occlusion data hides the body
                // from the shifted camera position.
                if(objects.occlusion&&objects.set_occlusion&&objects.occlusion(p)){objects.set_occlusion(p,false);it->occlusion_owned=true;}
                if(api.is_visible&&api.is_visible(p))++drawn;
                // The game may move the model between layers (hidden / shown);
                // use the layer it has right now, not the one seen at discovery.
                it->layer=api.layer(go);
                if(it->layer>=0&&it->layer<32){layers|=uint32_t(1)<<it->layer;body_on_fp_layer|=it->layer==8;}
                ++body_meshes;
            }
            ++it;
        }
        // Per-renderer occlusion is off but the engine still culls every body mesh:
        // fall back to switching occlusion culling off on the camera while this lasts.
        if(api.is_visible&&body_view&&body_meshes){
            if(drawn)invisible_since_=-1;
            else if(invisible_since_<0)invisible_since_=time;
            // Device log: the body stayed unseen with this off as well, so it is not the cause; kept disabled.
            else if(kCameraOcclusionFallback&&time-invisible_since_>1.5&&!camera_occlusion_owned_&&api.camera_occlusion&&api.set_camera_occlusion){
                camera_occlusion_original_=api.camera_occlusion(camera);
                if(camera_occlusion_original_){api.set_camera_occlusion(camera,false);camera_occlusion_owned_=true;}
            }
        }
        const int current=api.mask(camera);
        if(!mask_owned_||current!=applied_mask_)original_mask_=current;
        // The first-person layer stays hidden unless the body itself lives on it.
        const uint32_t hidden=body_on_fp_layer?0u:(uint32_t(1)<<8);
        applied_mask_=int(body_view?(uint32_t(original_mask_)|layers)&~hidden:uint32_t(original_mask_));
        if(current!=applied_mask_)api.set_mask(camera,applied_mask_);mask_owned_=true;status=1;
        if(time>=next_report_){
            next_report_=time+2.;
            int used=std::snprintf(report_,sizeof(report_),
                "TP travel=%.2f wanted=%.2f body_view=%d renderers=%zu body_meshes=%u primary=%zu fallback_animator=%d fp_root=%d mask=%08x->%08x cam_occlusion_off=%d drawn=%u cull_matrix_diff=%.4f head=(%.1f,%.1f,%.1f) cam=(%.1f,%.1f,%.1f) |",
                travel,wanted,body_view,renderers_.size(),body_meshes,model_.primary().size(),fallback_animator_!=0,fp_root_known_,
                uint32_t(original_mask_),uint32_t(applied_mask_),int(camera_occlusion_owned_),drawn,cull_diff_,baseline_.x,baseline_.y,baseline_.z,applied_.x,applied_.y,applied_.z);
            unsigned shown=0;
            for(const auto& r:renderers_){
                if(shown++>=8||used<0||size_t(used)>=sizeof(report_)-80)break;
                void* p=objects.live(r.id);
                const int enabled_now=p&&reinterpret_cast<uintptr_t>(p)==r.native?int(api.enabled(p)):-1;
                const int hidden_now=p&&objects.force_off_offset>=0?int(objects.field<uint8_t>(p,size_t(objects.force_off_offset))):-1;
                used+=std::snprintf(report_+used,sizeof(report_)-size_t(used)," [%d L%d %s en%d off%d sh%d]",
                    r.id,r.layer,r.fp?"fp":"body",enabled_now,hidden_now,enabled_now>=0?api.shadows(p):-1);
            }
            // One line per renderer: is the engine culling it, where is it, how big, has it a mesh.
            int d=0;detail_[0]=0;shown=0;
            for(const auto& r:renderers_){
                if(shown++>=8||d<0||size_t(d)>=sizeof(detail_)-200)break;
                void* p=objects.live(r.id);
                if(!p||reinterpret_cast<uintptr_t>(p)!=r.native)continue;
                float b[6]{};V3 sc{};int vis=-1,mesh=-1;
                if(api.is_visible)vis=api.is_visible(p)?1:0;
                if(api.bounds)api.bounds(p,b);
                if(api.lossy_scale){void* t=objects.native_return(api.game_transform(reinterpret_cast<void*>(r.go)));if(t)api.lossy_scale(t,&sc);}
                if(r.skinned&&api.shared_mesh)mesh=api.shared_mesh(p)?1:0;
                d+=std::snprintf(detail_+d,sizeof(detail_)-size_t(d),"TP renderer %d %s L%d visible=%d occlusion=%d mesh=%d bounds=(%.1f,%.1f,%.1f)+-(%.2f,%.2f,%.2f) scale=(%.2f,%.2f,%.2f)\n",
                    r.id,r.fp?"fp":"body",r.layer,vis,objects.occlusion?int(objects.occlusion(p)):-1,mesh,b[0],b[1],b[2],b[3],b[4],b[5],sc.x,sc.y,sc.z);
            }
            report_ready_=true;
        }
    }
    // One diagnostic line every few seconds while the camera is active.
    const char* take_report(){if(!report_ready_)return nullptr;report_ready_=false;return report_;}
    const char* detail()const{return detail_;}
    // Natives of what the game must not hide while the body is shown:
    // renderers (kind 0) and GameObjects (kind 1).
    template<class F>void each_guarded(Api& api,F f){
        for(const auto& r:renderers_)if(!r.fp){f(0,reinterpret_cast<void*>(r.native));f(1,reinterpret_cast<void*>(r.go));}
        model_.each_object(api,[&](void* go){f(1,go);});
    }
    uintptr_t camera_native()const{return camera_native_;}
};
} // namespace mango_camera
