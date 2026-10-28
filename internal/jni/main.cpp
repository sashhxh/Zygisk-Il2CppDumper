// MANGOcheat / Oxide 1.16.11978 ARM64. Unity APIs stay on Unity main thread.
#include <android/log.h>
#include <android/input.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>
#include "dobby.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "cf_locale_font.h"
#include "menu.h"
#include "player_features.h"
#include "runtime_loader.h"
#include "oxide_offsets.h"
#include "esp_capture.h"
#include "unity_return.h"
#include "chams.h"
#include "third_person.h"
#include "local_view_setup.h"
#include "local_weapon_view.h"
#include "weapon_grip.h"
#include "two_hand_pose.h"
#include "custom_avatar.h"
#include "avatar_storage.h"
#include "avatar_status.h"
#include "prefab_asset.h"
#include "unity_fields.h"
#include "graphics_metadata.h"
#include "shader_asset.h"
#include "chams_shader_bundle.h"
#include "graphics_roots.h"
#include "file_log.h"
extern "C" void mango_file_log(const char* tag,const char* line){mango_log::write_file(tag,line);}
extern "C" int oxide_bypass_install(uint32_t);
extern "C" int oxide_bypass_tick();
#define LOG(...) mango_log::log("MANGOcheat",__VA_ARGS__)
namespace mango {
constexpr const char* package="com.catsbit.oxidesurvivalisland";
// Identifies the loaded library in the startup log.
const char revision_marker[]="MANGOCHEAT_REV=r18-avatar-activation-recovery";
constexpr size_t max_players=256, bone_count=17;
struct V2{float x=0,y=0;};
struct V3{float x=0,y=0,z=0;};
struct Ray{V3 origin,direction;};
static_assert(sizeof(Ray)==24,"UnityEngine.Ray ARM64 layout");
struct RaycastHit {V3 point,normal;uint32_t face=0;float distance=0;V2 uv;int32_t collider=0;};
static_assert(sizeof(RaycastHit)==44&&offsetof(RaycastHit,distance)==0x1c&&offsetof(RaycastHit,collider)==0x28,
              "Unity 6000.3 RaycastHit ABI verified at D57E24C / D57E1A0");
struct Method;struct Field;using Obj=void;using Class=void;using Type=void;
double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool finite(V3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
float distance(V3 a,V3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
struct Config{
    bool enabled=true,box=true,distances=true,lines=false,skeleton=false,fill=false,corners=true;
    float range=300,color[3]={1,.62f,.13f};
    bool magic_bullet=false;float magic_fov=280;bool magic_ads_only=false;
    bool magic_team_filter=true;int magic_point=0; // 0 head, 1 body
    bool no_recoil=false,no_spread=false,no_weapon_motion=false;
    bool visibility_check=true,visible_only=false;
    bool chams=false;float chams_alpha=.28f,chams_range=80;int chams_limit=16;
    bool third_person=false;float camera_distance=2.5f;bool custom_model=false;
}cfg;
std::atomic<bool> want_esp{true},want_skeleton{false},game_ready{false};
std::atomic<float> esp_range{300};
std::atomic<bool> want_visibility{true};
std::atomic<bool> want_chams{false};
std::atomic<float> chams_alpha{.28f},chams_range{80};
std::atomic<int> chams_limit{16},chams_status{0};
std::atomic<int> chams_shader{0};
std::atomic<unsigned> chams_renderers{0},chams_creations{0},chams_restore_errors{0};
std::atomic<bool> want_custom_model{false};std::atomic<int> custom_model_status{0};
std::atomic<bool> want_third_person{false};std::atomic<float> camera_distance{2.5f};
std::atomic<int> camera_status{0};std::atomic<unsigned> camera_meshes{0};
std::atomic<int> shader_stage{0};std::atomic<float> chams_cpu_ms{0},camera_cpu_ms{0};
std::atomic<uintptr_t> third_camera_transform{0};std::atomic<pid_t> unity_camera_thread{0};
std::atomic<bool> want_no_recoil{false},want_no_spread{false},want_no_weapon_motion{false};
std::atomic<bool> want_magic_bullet{false},want_magic_ads_only{false},want_magic_team_filter{true};
std::atomic<float> magic_fov{280};std::atomic<int> magic_point{0};
std::atomic<int> magic_hook_state{0},magic_targets{0};
std::atomic<unsigned> magic_rewrites{0},magic_shot_calls{0},magic_shot_rewrites{0};
std::atomic<void*> local_player{nullptr};std::atomic<double> local_player_seen{0};
mango_player::Controller player_features;
void player_features_tick(double time){
    const uintptr_t player=time-local_player_seen.load()<1.0?
        reinterpret_cast<uintptr_t>(local_player.load()):0;
    player_features.update(player,want_no_recoil.load(),want_no_spread.load(),want_no_weapon_motion.load());
}
std::atomic<int> runtime_state{0},bypass_state{-2};
std::atomic<int> runtime_stage{0};
std::atomic<unsigned> frames{0};
std::atomic<int> egl_width{0},egl_height{0};
std::atomic<double> first_egl_frame_at{0};
std::atomic<bool> model_ready{false};
std::atomic<unsigned> capture_frames{0};
std::atomic<unsigned> max_observe_us{0},max_publish_us{0};
std::atomic<unsigned> max_render_us{0},capture_samples{0},render_samples{0};
std::atomic<uint64_t> capture_total_us{0},render_total_us{0};
std::atomic<int> input_backend{0}; // 1 legacy, 2 new Input System
std::atomic<bool> native_touch_seen{false},native_touch_down{false};
std::atomic<bool> pointer_down_state{false};
std::atomic<float> native_touch_x{0},native_touch_y{0};
std::atomic<int> input_devices{0},input_touches{0},input_legacy_count{-1};
std::atomic<float> input_x{0},input_y{0};
std::atomic<int> input_screen_width{0},input_screen_height{0};
struct Player{
    V3 feet,head;std::array<V3,bone_count> bones{};uint32_t valid=0;
    float meters=0;
    mango_esp::Visibility visibility=mango_esp::Visibility::Unknown;
};
struct Snapshot{
    std::array<Player,max_players> players{};
    size_t count=0,total=0;bool local=false,camera=false;
    double stamp=0;
    uint64_t generation=0;
    // Only the used prefix is copied. A full 96-slot copy per frame was the
    // largest fixed cost in the previous revision.
    void assign(const Snapshot& other){
        count=other.count<max_players?other.count:max_players;
        total=other.total;local=other.local;camera=other.camera;stamp=other.stamp;generation=other.generation;
        for(size_t i=0;i<count;++i)players[i]=other.players[i];
    }
};
struct Pointer{V2 position{};bool down=false;double stamp=0;};
// UI bounds are published by the render thread. Input selection never borrows
// a gameplay finger unless that finger is actually over a menu control.
std::atomic<float> menu_x0{0},menu_y0{0},menu_x1{0},menu_y1{0};
std::atomic<float> bar_x0{0},bar_y0{0},bar_x1{0},bar_y1{0};
std::atomic<bool> menu_is_open{true},menu_popup_open{false};
bool menu_hit(float x,float y){
    if(!std::isfinite(x)||!std::isfinite(y))return false;
    if(x>=bar_x0.load()&&x<=bar_x1.load()&&y>=bar_y0.load()&&y<=bar_y1.load())return true;
    if(!menu_is_open.load())return false;
    if(menu_popup_open.load())return true;
    return x>=menu_x0.load()&&x<=menu_x1.load()&&y>=menu_y0.load()&&y<=menu_y1.load();
}
Snapshot published;std::mutex snapshot_mutex;
Pointer published_pointer;std::mutex pointer_mutex;

struct Api{
    bool loaded=false;std::atomic<bool> raw{false};uintptr_t base=0;std::vector<const void*> images;
    mango_runtime::Resolver resolver;
    void*(*domain_get)()=nullptr;
    void*(*thread_attach)(void*)=nullptr;
    const void**(*domain_get_assemblies)(void*,size_t*)=nullptr;
    const void*(*assembly_get_image)(const void*)=nullptr;
    Class*(*class_from_name)(const void*,const char*,const char*)=nullptr;
    const Method*(*class_get_methods)(Class*,void**)=nullptr;
    const char*(*method_get_name)(const Method*)=nullptr;
    uint32_t(*method_get_param_count)(const Method*)=nullptr;
    uint32_t(*method_get_flags)(const Method*,uint32_t*)=nullptr;
    const Type*(*method_get_return_type)(const Method*)=nullptr;
    int(*type_get_type)(const Type*)=nullptr;
    bool(*method_is_generic)(const Method*)=nullptr;
    Field*(*class_get_field_from_name)(Class*,const char*)=nullptr;
    size_t(*field_get_offset)(Field*)=nullptr;
    uint32_t(*field_get_flags)(Field*)=nullptr;
    void(*field_get_value)(Obj*,Field*,void*)=nullptr;
    void(*field_static_get_value)(Field*,void*)=nullptr;
    Class*(*object_get_class)(Obj*)=nullptr;
    Class*(*class_get_parent)(Class*)=nullptr;
    const Type*(*class_get_type)(Class*)=nullptr;
    Obj*(*type_get_object)(const Type*)=nullptr;
    Obj*(*runtime_invoke)(const Method*,Obj*,void**,Obj**)=nullptr;
    void*(*object_unbox)(Obj*)=nullptr;
    uint32_t(*array_length)(Obj*)=nullptr;
    void*(*gchandle_new)(Obj*,bool)=nullptr;
    Obj*(*gchandle_get_target)(void*)=nullptr;
    void(*gchandle_free)(void*)=nullptr;
    bool load(){
        if(loaded)return true;
        if(!resolver.select_named_image("libil2cpp.so",mango_offsets::build_id,sizeof(mango_offsets::build_id),
                mango_offsets::anchors,sizeof(mango_offsets::anchors)/sizeof(mango_offsets::anchors[0]))){
            runtime_stage.store(1);
            LOG("Waiting for verified Oxide 1.16.11978 ARM64 image; unsupported build is not patched");
            return false;
        }
        base=resolver.base();mango_log::module_base()=base;raw=true;loaded=true;runtime_stage.store(2);
        LOG("Offset runtime ready: %s bias=%lx GNU build ID verified, no metadata/scene scan",resolver.path(),static_cast<unsigned long>(base));
        return true;
    }

    bool refresh(){
        void* domain=domain_get();if(!domain)return false;size_t n=0;
        const void** assemblies=domain_get_assemblies(domain,&n);if(!assemblies||!n)return false;
        images.clear();for(size_t i=0;i<n;++i){const void* im=assembly_get_image(assemblies[i]);if(im)images.push_back(im);}
        return !images.empty();
    }
    Class* cls(const char* ns,const char* name)const{
        Class* result=nullptr;
        for(const void* image:images){Class* c=class_from_name(image,ns,name);if(c&&result&&c!=result)return nullptr;if(c)result=c;}
        return result;
    }
    const Method* method(const char* ns,const char* klass,const char* name,uint32_t args,uintptr_t rva,bool stat,int kind=-1)const{
        Class* c=cls(ns,klass);if(!c)return nullptr;
        const Method* result=nullptr;void* it=nullptr;
        while(const Method* m=class_get_methods(c,&it)){
            if(std::strcmp(method_get_name(m),name)||method_get_param_count(m)!=args)continue;
            uint32_t impl=0,flags=method_get_flags(m,&impl);uintptr_t code=0;
            std::memcpy(&code,m,sizeof(code)); // first MethodInfo word in supplied il2cpp.h
            if(method_is_generic(m)||(flags&0x400)||bool(flags&0x10)!=stat||(code&3)||!resolver.executable(code))return nullptr;
            if(kind>=0&&type_get_type(method_get_return_type(m))!=kind)return nullptr;
            // Metadata supplies the current native function. Reference RVAs are
            // diagnostic, not a reason to disable the entire read-only ESP.
            if(code<base||code-base!=rva)LOG("Metadata RVA changed: %s.%s expected=%lx actual=%lx",
                klass,name,static_cast<unsigned long>(rva),static_cast<unsigned long>(code-base));
            if(result)return nullptr;result=m;
        }
        if(!result)LOG("Method mismatch: %s.%s RVA=%lx",klass,name,static_cast<unsigned long>(rva));
        return result;
    }
    Field* field(Class* c,const char* name,size_t offset,bool stat=false)const{
        Field* f=c?class_get_field_from_name(c,name):nullptr;
        if(!f||bool(field_get_flags(f)&0x10)!=stat)return nullptr;
        if(field_get_offset(f)!=offset){
            static std::vector<Field*> reported;
            if(std::find(reported.begin(),reported.end(),f)==reported.end()){
                reported.push_back(f);LOG("Metadata field offset changed: %s expected=%zx actual=%zx",name,offset,field_get_offset(f));
            }
        }
        // field_get_value uses this FieldInfo's actual runtime offset.
        return f;
    }
    template<class T>T read(Obj* obj,Field* f)const{T out{};if(obj&&f)field_get_value(obj,f,&out);return out;}
    Obj* ref(Obj* obj,Field* f)const{return read<Obj*>(obj,f);}
    Obj* stat(Field* f)const{Obj* out=nullptr;if(f)field_static_get_value(f,&out);return out;}
    Obj* call(const Method* m,Obj* obj=nullptr,void** args=nullptr)const{
        if(!m)return nullptr;Obj* ex=nullptr;Obj* out=runtime_invoke(m,obj,args,&ex);
        if(ex){
            static std::vector<const Method*> reported;
            if(std::find(reported.begin(),reported.end(),m)==reported.end()){
                reported.push_back(m);LOG("Managed call failed: %s",method_get_name(m));
            }
        }
        return ex?nullptr:out;
    }
    template<class T>bool value(const Method* m,Obj* obj,T& out,void** args=nullptr)const{
        Obj* boxed=call(m,obj,args);if(!boxed)return false;void* p=object_unbox(boxed);if(!p)return false;
        std::memcpy(&out,p,sizeof(T));return true;
    }
    const Method* inherited(Class* cls,const char* name,uint32_t params)const{
        for(Class* c=cls;c;c=class_get_parent(c)){
            void* it=nullptr;const Method* found=nullptr;
            while(const Method* m=class_get_methods(c,&it)){
                if(std::strcmp(method_get_name(m),name)||method_get_param_count(m)!=params)continue;
                uint32_t impl=0;
                if(method_get_flags(m,&impl)&(0x10|0x400))continue;
                if(method_is_generic(m))continue;
                if(found)return nullptr;
                found=m;
            }
            if(found)return found;
        }
        return nullptr;
    }
}api;
uintptr_t native(const Method* m){uintptr_t p=0;if(m)std::memcpy(&p,m,sizeof(p));return p;}
Field *f_players=nullptr,*f_local=nullptr,*f_cmanager=nullptr,*f_camera=nullptr,*f_animator=nullptr;
const Method *m_transform=nullptr,*m_position=nullptr,*m_project=nullptr,*m_camera=nullptr,*m_local=nullptr,*m_frame=nullptr;
const Method *m_width=nullptr,*m_height=nullptr,*m_touches=nullptr,*m_touch=nullptr,*m_mouse=nullptr,*m_button=nullptr,*m_bone=nullptr,*m_human=nullptr;
const Method *m_find=nullptr,*m_touchscreen=nullptr,*m_primary=nullptr,*m_control_position=nullptr,*m_control_press=nullptr;
Class* player_class=nullptr;
bool resolve_model(){
    if(!api.refresh())return false;
    Class* player=api.cls("Oxide","PlayerManager");player_class=player;
    Class* controller=api.cls("Oxide","GameControllerBase");
    f_players=api.field(player,"clientPlayerList",0x10,true);f_local=api.field(controller,"<KOG>k__BackingField",0x10,true);
    f_cmanager=api.field(controller,"<KOx>k__BackingField",0x38,true);
    f_camera=api.field(api.cls("Oxide","CameraManager"),"m_Camera",0x20);
    f_animator=api.field(player,"animator",0x1B0);
    m_transform=api.method("UnityEngine","Component","get_transform",0,0xD4B63F0,false);
    m_position=api.method("UnityEngine","Transform","get_position",0,0xD4CFB20,false,0x11);
    m_project=api.method("UnityEngine","Camera","WorldToViewportPoint",1,0xD43A0A8,false,0x11);
    m_camera=api.method("UnityEngine","Camera","get_main",0,0xD43AB4C,true);
    m_local=api.method("Mirror","NetworkBehaviour","get_isLocalPlayer",0,0xC8D8774,false,2);
    m_frame=api.method("UnityEngine","Time","get_frameCount",0,0xD4B577C,true,8);
    m_width=api.method("UnityEngine","Screen","get_width",0,0xD44DE3C,true,8);
    m_height=api.method("UnityEngine","Screen","get_height",0,0xD44DE64,true,8);
    m_touches=api.method("UnityEngine","Input","get_touchCount",0,0xD54EAD0,true,8);
    m_touch=api.method("UnityEngine","Input","GetTouch",1,0xD54E1C4,true,0x11);
    m_mouse=api.method("UnityEngine","Input","get_mousePosition",0,0xD54E5F8,true,0x11);
    m_button=api.method("UnityEngine","Input","GetMouseButton",1,0xD54E110,true,2);
    m_bone=api.method("UnityEngine","Animator","GetBoneTransform",1,0xD40FE38,false);
    m_human=api.method("UnityEngine","Animator","get_isHuman",0,0xD4091E0,false,2);
    m_find=api.method("UnityEngine","Object","FindObjectsOfType",2,0xD4C3628,true);
    m_touchscreen=api.method("UnityEngine.InputSystem","Touchscreen","get_current",0,0xD079374,true);
    m_primary=api.method("UnityEngine.InputSystem","Touchscreen","get_primaryTouch",0,0xD0792B4,false);
    m_control_position=api.method("UnityEngine.InputSystem.Controls","TouchControl","get_position",0,0xD007C20,false);
    m_control_press=api.method("UnityEngine.InputSystem.Controls","TouchControl","get_press",0,0xD007BD8,false);
    const bool ok=player&&(f_players||m_find)&&m_transform&&m_position&&m_project&&m_camera;
    LOG("Bindings: model=%d list=%d local=%d cameraField=%d frame=%d legacyInput=%d newInput=%d",
        ok,f_players!=nullptr,f_local!=nullptr,f_camera!=nullptr,m_frame!=nullptr,m_touch!=nullptr,m_touchscreen!=nullptr);
    return ok;
}
bool position(Obj* component,V3& out){
    Obj* tr=api.call(m_transform,component);return tr&&api.value(m_position,tr,out)&&finite(out);
}
bool project(Obj* camera,V3 point,V3& out){void* args[]={&point};return api.value(m_project,camera,out,args)&&finite(out);}
constexpr int bone_ids[bone_count]={0,7,8,9,10,11,13,15,12,14,16,1,3,5,2,4,6};
constexpr int links[][2]={{0,1},{1,2},{2,3},{3,4},{2,5},{5,6},{6,7},{2,8},{8,9},{9,10},{0,11},{11,12},{12,13},{0,14},{14,15},{15,16}};
bool bone_position(Obj* animator,int id,V3& out){void* args[]={&id};Obj* tr=api.call(m_bone,animator,args);return tr&&api.value(m_position,tr,out)&&finite(out);}
struct Touch{int finger;V2 pos,raw,delta;float dt;int taps,phase,type;float pressure,maximum,radius,variance,altitude,azimuth;};
static_assert(sizeof(Touch)==68&&offsetof(Touch,phase)==0x24,"Touch ABI");
void capture_input(Pointer& s){
    int w=0,h=0,n=0;
    if(!api.value(m_width,nullptr,w)||!api.value(m_height,nullptr,h)||w<=0||h<=0)return;
    // The game's new Input System works even when legacy UnityEngine.Input throws.
    Obj* device=api.call(m_touchscreen);
    Obj* primary=device?api.call(m_primary,device):nullptr;
    if(device&&primary){
        Obj* position_control=api.call(m_control_position,primary);
        Obj* press_control=api.call(m_control_press,primary);
        if(position_control&&press_control){
            const Method* read=api.inherited(api.object_get_class(position_control),"ReadValue",0);
            const Method* pressed=api.inherited(api.object_get_class(press_control),"get_isPressed",0);
            V2 pos{};bool down=false;
            if(api.value(read,position_control,pos)&&api.value(pressed,press_control,down)&&
               menu_hit(pos.x,float(h)-pos.y)){
                s.position={pos.x/w,pos.y/h};s.down=down;input_backend.store(2);return;
            }
        }
    }
    static int finger=-1;
    if(api.value(m_touches,nullptr,n)&&n>0&&n<32){
        for(int i=0;i<n;++i){Touch t{};void* args[]={&i};if(!api.value(m_touch,nullptr,t,args))continue;
            if(finger>=0&&finger!=t.finger)continue;
            if(!menu_hit(t.pos.x,float(h)-t.pos.y))continue;
            finger=t.finger;s.position={t.pos.x/w,t.pos.y/h};s.down=t.phase<3;
            if(!s.down)finger=-1;input_backend.store(1);return;}
        finger=-1;return;
    }
    finger=-1;int button=0;bool down=false;V3 mouse{};void* args[]={&button};
    if(api.value(m_button,nullptr,down,args)&&api.value(m_mouse,nullptr,mouse)&&
       menu_hit(mouse.x,float(h)-mouse.y)){
        s.position={mouse.x/w,mouse.y/h};s.down=down;input_backend.store(1);
    }
}
void* fallback_players=nullptr;
Obj* player_array(int& size){
    size=0;
    Obj* list=api.stat(f_players);
    if(list){
        Class* cls=api.object_get_class(list);
        Field* items_field=api.field(cls,"_items",0x10),*size_field=api.field(cls,"_size",0x18);
        Obj* items=api.ref(list,items_field);const int n=api.read<int>(list,size_field);
        if(items&&n>0&&n<=4096&&uint32_t(n)<=api.array_length(items)){size=n;return items;}
    }
    // Some client scenes leave clientPlayerList empty. Enumerate actual scene
    // components instead; only objects already present in this client are read.
    static double last_scan=-1;
    if(now()-last_scan>=0.5){
        last_scan=now();
        Obj* type=player_class?api.type_get_object(api.class_get_type(player_class)):nullptr;
        bool include_inactive=false;
        void* args[]={type,&include_inactive};
        Obj* array=type?api.call(m_find,nullptr,args):nullptr;
        void* next=array?api.gchandle_new(array,false):nullptr;
        if(fallback_players)api.gchandle_free(fallback_players);
        fallback_players=next;
    }
    Obj* array=fallback_players?api.gchandle_get_target(fallback_players):nullptr;
    if(array){const uint32_t n=api.array_length(array);if(n<=4096){size=int(n);return array;}}
    return nullptr;
}
void capture_players(Snapshot& s){
    int size=0;Obj* items=player_array(size);
    s.total=size;
    Obj* local=api.stat(f_local);
    s.local=local!=nullptr;
    if(local){local_player.store(local);local_player_seen.store(now());}
    Obj* camera=api.ref(api.stat(f_cmanager),f_camera);if(!camera)camera=api.call(m_camera);
    s.camera=camera!=nullptr;
    if(!items||!camera||!want_esp.load())return;
    // Camera position is a valid distance origin even before the game's static
    // local-player property becomes populated.
    V3 origin{};
    if(!local||!position(local,origin)){if(!position(camera,origin))return;}
    void* handle=api.gchandle_new(items,false);if(!handle)return;
    for(int i=0;i<size&&s.count<max_players;++i){
        Obj* root=api.gchandle_get_target(handle);if(!root)break;Obj* p=nullptr;
        std::memcpy(&p,reinterpret_cast<unsigned char*>(root)+0x20+i*sizeof(void*),sizeof(p));
        if(!p||p==local)continue;bool mine=false;
        if(api.value(m_local,p,mine)&&mine){s.local=true;local_player.store(p);local_player_seen.store(now());continue;}
        V3 feet{};if(!position(p,feet))continue;Player out{};out.meters=distance(origin,feet);
        if(!std::isfinite(out.meters)||out.meters>2000)continue;
        V3 head=feet;head.y+=1.75f;Obj* animator=api.ref(p,f_animator);bool human=false;
        if(animator&&m_bone&&api.value(m_human,animator,human)&&human){
            V3 actual{};if(bone_position(animator,10,actual)){head=actual;head.y+=.15f;}
            if(want_skeleton.load())for(size_t b=0;b<bone_count;++b){
                V3 world{},screen{};
                if(bone_position(animator,bone_ids[b],world)&&project(camera,world,screen)&&screen.z>.01f){out.bones[b]=screen;out.valid|=1u<<b;}
            }
        }
        if(!project(camera,feet,out.feet)||!project(camera,head,out.head)||out.feet.z<.01f||out.head.z<.01f)continue;
        s.players[s.count++]=out;
    }api.gchandle_free(handle);
}
void pump(){
    // Managed callback only. No Unity method is called from EGL or worker.
    static thread_local bool entered=false;if(entered)return;entered=true;
    static int last_frame=-1;static double last_capture=0,last_bypass=0,last_retry=0,last_pump=0;
    int frame=-1;
    const bool frame_valid=api.value(m_frame,nullptr,frame);
    if(frame_valid&&frame==last_frame){entered=false;return;}
    double time=now();
    if(!frame_valid&&time-last_pump<1.0/240){entered=false;return;}
    last_frame=frame;last_pump=time;capture_frames.fetch_add(1);
    if(!model_ready.load()&&time-last_retry>=5){
        last_retry=time;model_ready.store(resolve_model());
    }
    if(time-last_bypass>=(bypass_state.load()>=0?1:10)){
        last_bypass=time;if(bypass_state.load()<0)bypass_state.store(oxide_bypass_install(1));
        else{int r=oxide_bypass_tick();if(r<0)LOG("ACTk cleanup status=%d",r);}
    }
    Pointer pointer;
    {std::lock_guard<std::mutex> lock(pointer_mutex);pointer=published_pointer;}
    pointer.down=false;
    capture_input(pointer);   // last position is kept through the release frame
    pointer.stamp=time;
    {std::lock_guard<std::mutex> lock(pointer_mutex);published_pointer=pointer;}
    if(time-last_capture>=1.0/30){
        static Snapshot managed_scratch;
        managed_scratch.count=0;managed_scratch.total=0;managed_scratch.local=false;managed_scratch.camera=false;
        if(model_ready.load())capture_players(managed_scratch);
        managed_scratch.stamp=time;last_capture=time;
        std::lock_guard<std::mutex> lock(snapshot_mutex);published.assign(managed_scratch);
    }
    player_features_tick(time);
    entered=false;
}
using Late=void(*)(Obj*,const Method*);using Before=void(*)(const Method*);
Late original_late=nullptr,original_update=nullptr;Before original_before=nullptr,original_canvas=nullptr;
void hook_late(Obj* self,const Method* info){if(original_late)original_late(self,info);pump();}
void hook_update(Obj* self,const Method* info){if(original_update)original_update(self,info);pump();}
void hook_before(const Method* info){if(original_before)original_before(info);pump();}
void hook_canvas(const Method* info){if(original_canvas)original_canvas(info);pump();}
int hook(void* address,void* replacement,void** original){
    return DobbyHook(address,reinterpret_cast<dobby_dummy_func_t>(replacement),reinterpret_cast<dobby_dummy_func_t*>(original));
}

// Oxide 1.16.11978 hides every il2cpp_* export. These native entry points and
// object offsets were checked against the supplied ARM64 libil2cpp.so. The
// callbacks execute on Unity's thread, so no unattached thread calls Unity.
template<class T>T raw_fn(uintptr_t rva){return reinterpret_cast<T>(api.base+rva);}
std::atomic<bool> raw_read_vm{true};
std::atomic<int> raw_read_fd{-1};
bool raw_read(uintptr_t address,void* out,size_t size){
    if(!address||!out||!size||size>4096||size>UINTPTR_MAX-address)return false;
#if defined(SYS_process_vm_readv)
    if(raw_read_vm.load(std::memory_order_relaxed)){
        iovec local{out,size},remote{reinterpret_cast<void*>(address),size};
        const long n=syscall(SYS_process_vm_readv,getpid(),&local,1,&remote,1,0);
        if(n==static_cast<long>(size))return true;
        // EFAULT means a bad game pointer; only a refused call disables this path.
        if(errno==EPERM||errno==ENOSYS||errno==EACCES)raw_read_vm.store(false,std::memory_order_relaxed);
        else return false;
    }
#endif
    // The descriptor is opened once. R5 opened and closed /proc/self/mem on
    // every field read, which is three syscalls per ESP field on kernels that
    // refuse process_vm_readv.
    int fd=raw_read_fd.load(std::memory_order_relaxed);
    if(fd<0){
        fd=open("/proc/self/mem",O_RDONLY|O_CLOEXEC);
        if(fd<0)return false;
        int expected=-1;
        if(!raw_read_fd.compare_exchange_strong(expected,fd)){close(fd);fd=expected;}
    }
    const ssize_t n=pread(fd,out,size,static_cast<off_t>(address));
    return n==static_cast<ssize_t>(size);
}
template<class T>T raw_field(Obj* object,size_t offset){
    T result{};const uintptr_t p=reinterpret_cast<uintptr_t>(object);
    if(p&&offset<=UINTPTR_MAX-p)raw_read(p+offset,&result,sizeof(result));
    return result;
}
void raw_team_tag(Obj* value,char* out,size_t capacity){
    if(!capacity)return;out[0]=0;
    int32_t length=raw_field<int32_t>(value,0x10);
    if(length<=0||length>256)return;
    uint16_t units[64]{};size_t count=std::min(size_t(length),size_t(64));
    const uintptr_t p=reinterpret_cast<uintptr_t>(value);
    if(!p||!raw_read(p+0x14,units,count*sizeof(uint16_t)))return;
    size_t used=0;
    for(size_t i=0;i<count;++i){
        uint32_t cp=units[i];
        if(cp>=0xD800&&cp<=0xDBFF&&i+1<count&&units[i+1]>=0xDC00&&units[i+1]<=0xDFFF){
            cp=0x10000+((cp-0xD800)<<10)+units[++i]-0xDC00;
        }else if(cp>=0xD800&&cp<=0xDFFF)cp=0xFFFD;
        if(cp<0x20||cp==0x7F)continue;
        char bytes[4]{};size_t n=0;
        if(cp<0x80){bytes[0]=char(cp);n=1;}
        else if(cp<0x800){bytes[0]=char(0xC0|(cp>>6));bytes[1]=char(0x80|(cp&63));n=2;}
        else if(cp<0x10000){bytes[0]=char(0xE0|(cp>>12));bytes[1]=char(0x80|((cp>>6)&63));bytes[2]=char(0x80|(cp&63));n=3;}
        else{bytes[0]=char(0xF0|(cp>>18));bytes[1]=char(0x80|((cp>>12)&63));bytes[2]=char(0x80|((cp>>6)&63));bytes[3]=char(0x80|(cp&63));n=4;}
        if(used+n>=capacity)break;std::memcpy(out+used,bytes,n);used+=n;
    }out[used]=0;
}
bool raw_position(Obj* component,V3& value){
    using Transform=Obj*(*)(Obj*,const Method*);
    using Position=V3(*)(Obj*,const Method*);
    if(!component||!raw_field<Obj*>(component,0x10))return false;
    Obj* transform=raw_fn<Transform>(0xD4B63F0)(component,nullptr);
    if(!transform||!raw_field<Obj*>(transform,0x10))return false;
    value=raw_fn<Position>(0xD4CFB20)(transform,nullptr);
    return finite(value);
}
bool raw_project(Obj* camera,V3 world,V3& viewport){
    using Project=V3(*)(Obj*,V3,const Method*);
    viewport=raw_fn<Project>(0xD43A0A8)(camera,world,nullptr);
    return finite(viewport)&&viewport.z>0.01f;
}
struct RawObservation {
    Obj* address=nullptr;Obj* native_address=nullptr;Obj* animator=nullptr;Obj* native_animator=nullptr;Player world{};
    V3 head_offset{0,1.55f,0},chest_offset{0,1.1f,0},chest{};
    char team[48]{};
    double stamp=0,team_at=0;
    double visibility_at=0;
    double head_at=0,bones_at=0,human_at=0;
    mango_esp::Visibility visibility=mango_esp::Visibility::Unknown;
    float head_pad=.22f;
    int human=-1; // -1 unknown, 0 no animator rig, 1 humanoid
    bool visible=false,local=false,chest_valid=false;
};
std::vector<RawObservation> raw_observations;
std::mutex raw_observations_mutex;
std::atomic<bool> raw_camera_seen{false};
std::atomic<double> last_canvas_call{0};
std::atomic<double> last_before_call{0};
std::atomic<int> last_late_frame{-1};
std::atomic<bool> registry_healthy{false};
std::atomic<bool> raw_hooks_complete{false};
mango_esp::Session raw_session;
mango_esp::LivePlayers live_players;

Obj* raw_decode_object(void* value){
    const uintptr_t result=mango_unity::decode_object(reinterpret_cast<uintptr_t>(value),
        [](uintptr_t handle){using Fn=uintptr_t(*)(uintptr_t,const Method*);return raw_fn<Fn>(0xDB1E504)(handle,nullptr);},
        [](uintptr_t handle){using Fn=uintptr_t(*)(uintptr_t,const Method*);return raw_fn<Fn>(0xD9F5710)(handle,nullptr);},
        [](uintptr_t at){return raw_field<uintptr_t>(reinterpret_cast<Obj*>(at),0);});
    return reinterpret_cast<Obj*>(result);
}
void* raw_native_return(void* value){return raw_field<void*>(raw_decode_object(value),0x10);}

struct RawEngine {
    using MatrixGetter=void(*)(void*,mango_esp::Matrix*);
    using RectGetter=void(*)(void*,mango_esp::Rect*);
    using ComponentObject=void*(*)(void*);
    using GetScene=void(*)(void*,void*);
    using GetPhysicsScene=void(*)(void*,void*);
    using SceneValid=bool(*)(void*);
    using Raycast=bool(*)(void*,Ray*,float,RaycastHit*,int,int);
    using Collider=void*(*)(int32_t*);
    using IsChild=bool(*)(void*,void*);
    using Unwrap=void*(*)(void*);
    MatrixGetter view=nullptr,projection=nullptr;
    RectGetter rect=nullptr;ComponentObject game_object=nullptr;
    GetScene scene=nullptr;ComponentObject transform=nullptr;
    GetPhysicsScene physics_scene=nullptr;SceneValid scene_valid=nullptr;
    Raycast raycast=nullptr;Collider collider=nullptr;IsChild is_child=nullptr;
    Unwrap unwrap_native=nullptr;
    // Main-thread writes; worker reads only atomic totals.
    std::array<std::atomic<unsigned>,10> metrics{}; // rays,clear,target,wall,own,scene,binding,badHit,collider,budget
    bool ready=false;
    void load(){
        if(ready)return;
        using Resolve=void*(*)(const char*);
        const auto resolve=raw_fn<Resolve>(mango_offsets::resolve_icall);
        // Exact icall names verified in this APK's libil2cpp and libunity.
        view=reinterpret_cast<MatrixGetter>(resolve("UnityEngine.Camera::get_worldToCameraMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
        projection=reinterpret_cast<MatrixGetter>(resolve("UnityEngine.Camera::get_nonJitteredProjectionMatrix_Injected(System.IntPtr,UnityEngine.Matrix4x4&)"));
        rect=reinterpret_cast<RectGetter>(resolve("UnityEngine.Camera::get_rect_Injected(System.IntPtr,UnityEngine.Rect&)"));
        game_object=reinterpret_cast<ComponentObject>(resolve("UnityEngine.Component::get_gameObject_Injected(System.IntPtr)"));
        scene=reinterpret_cast<GetScene>(resolve("UnityEngine.GameObject::get_scene_Injected(System.IntPtr,UnityEngine.SceneManagement.Scene&)"));
        physics_scene=reinterpret_cast<GetPhysicsScene>(resolve("UnityEngine.PhysicsSceneExtensions::GetPhysicsScene_Internal_Injected(UnityEngine.SceneManagement.Scene&,UnityEngine.PhysicsScene&)"));
        scene_valid=reinterpret_cast<SceneValid>(resolve("UnityEngine.PhysicsScene::IsValid_Internal_Injected(UnityEngine.PhysicsScene&)"));
        raycast=reinterpret_cast<Raycast>(resolve("UnityEngine.PhysicsScene::Internal_Raycast_Injected(UnityEngine.PhysicsScene&,UnityEngine.Ray&,System.Single,UnityEngine.RaycastHit&,System.Int32,UnityEngine.QueryTriggerInteraction)"));
        collider=reinterpret_cast<Collider>(resolve("UnityEngine.Object::FindObjectFromInstanceID_Injected(UnityEngine.EntityId&)"));
        transform=reinterpret_cast<ComponentObject>(resolve("UnityEngine.Component::get_transform_Injected(System.IntPtr)"));
        is_child=reinterpret_cast<IsChild>(resolve("UnityEngine.Transform::IsChildOf_Injected(System.IntPtr,System.IntPtr)"));
        unwrap_native=raw_native_return;
        ready=view&&projection&&rect&&game_object&&scene&&physics_scene&&scene_valid&&raycast&&collider&&transform&&is_child;
        LOG("Verified engine bindings: projection=%d physics=%d",view&&projection&&rect,game_object&&scene&&physics_scene&&scene_valid&&raycast&&collider&&transform&&is_child);
    }
    mango_esp::Projection capture_camera(Obj* camera){
        mango_esp::Projection result;
        void* native=raw_field<void*>(camera,0x10);
        if(native&&view&&projection&&rect){view(native,&result.view);projection(native,&result.projection);rect(native,&result.rect);result.validate();}
        return result;
    }
    bool capture_scene(Obj* camera,std::array<uint64_t,2>& output){
        void* native=raw_field<void*>(camera,0x10);
        if(!native||!game_object||!scene||!physics_scene||!scene_valid||!unwrap_native){metrics[5].fetch_add(1);return false;}
        void* object=unwrap_native(game_object(native));if(!object){metrics[5].fetch_add(1);return false;}
        // Scene is 4 bytes and PhysicsScene 8 bytes in the verified wrappers;
        // padded buffers avoid relying on a managed return-value ABI.
        std::array<uint64_t,2> scene_value{};
        output={};scene(object,scene_value.data());physics_scene(scene_value.data(),output.data());
        const bool valid=scene_valid(output.data());if(!valid)metrics[5].fetch_add(1);return valid;
    }
    mango_esp::Visibility visibility(Obj* target,Obj* local,V3 origin,V3 head,V3 chest,std::array<uint64_t,2>& physics,mango_unity::RayBudget* budget=nullptr){
        using Visibility=mango_esp::Visibility;
        if(!finite(origin))return Visibility::Unknown;
        void* native=raw_field<void*>(target,0x10);
        if(!native||!raycast||!collider||!transform||!is_child||!unwrap_native){metrics[6].fetch_add(1);return Visibility::Unknown;}
        void* target_transform=unwrap_native(transform(native));if(!target_transform){metrics[6].fetch_add(1);return Visibility::Unknown;}
        void* native_local=raw_field<void*>(local,0x10);
        void* local_transform=native_local?unwrap_native(transform(native_local)):nullptr;
        // Keep Default and player layers: recognize the target's collider by
        // its transform hierarchy instead of ignoring everything on that layer.
        constexpr int mask=~(1<<2);
        bool unknown=false,tested=false;
        for(const V3 point:{head,chest}){
            const float length=distance(origin,point);
            if(!finite(point)||!std::isfinite(length)||length<.05f)continue;
            tested=true;
            Ray ray{origin,{(point.x-origin.x)/length,(point.y-origin.y)/length,(point.z-origin.z)/length}};
            float traveled=0;
            for(unsigned pass=0;pass<4;++pass){
                RaycastHit hit{};
                const float remaining=length-traveled;
                if(remaining<=.01f){unknown=true;break;}
                if(budget&&!budget->take()){metrics[9].fetch_add(1);return Visibility::Unknown;}
                metrics[0].fetch_add(1);
                if(!raycast(physics.data(),&ray,remaining,&hit,mask,1)){metrics[1].fetch_add(1);return Visibility::Visible;}
                if(!std::isfinite(hit.distance)||hit.distance<0||hit.distance>remaining+.05f||!hit.collider){metrics[7].fetch_add(1);unknown=true;break;}
                void* hit_collider=unwrap_native(collider(&hit.collider));
                void* hit_transform=hit_collider?unwrap_native(transform(hit_collider)):nullptr;
                if(!hit_transform){metrics[8].fetch_add(1);unknown=true;break;}
                if(hit_transform==target_transform||is_child(hit_transform,target_transform)){metrics[2].fetch_add(1);return Visibility::Visible;}
                if(!local_transform||(hit_transform!=local_transform&&!is_child(hit_transform,local_transform))){metrics[3].fetch_add(1);break;}
                metrics[4].fetch_add(1);
                // A third-person camera can start behind our own character.
                // Step inside that collider; PhysX ignores a collider containing
                // the ray origin. No collider or global physics state is changed.
                const float advance=hit.distance+.02f;traveled+=advance;
                ray.origin={ray.origin.x+ray.direction.x*advance,ray.origin.y+ray.direction.y*advance,ray.origin.z+ray.direction.z*advance};
                if(pass==3)unknown=true;
            }
        }
        return unknown||!tested?Visibility::Unknown:Visibility::Occluded;
    }
}raw_engine;

mango_chams::Api chams_api;
mango_chams::Controller chams_controller;
mango_graphics::Metadata graphics_metadata;
mango_graphics::Roots graphics_roots;
mango_graphics::ShaderAsset shader_asset;
std::u16string shader_file_path;std::atomic<bool> shader_file_ready{false};
void* shader_runtime_type=nullptr;
void* safe_graphics_icall(const char* name){return raw_fn<void*(*)(const char*)>(0x5C8292C)(name);}
bool raw_type_named(Obj*,const char*);
void* raw_named_reference(void* object,const char* name){
    if(!raw_type_named(object,"PlayerModelInfo"))return nullptr;
    const char* expected=nullptr;size_t offset=0;
    if(!std::strcmp(name,"body")){expected="Transform";offset=0x40;}
    else if(!std::strcmp(name,"headModel")){expected="Transform";offset=0x48;}
    else if(!std::strcmp(name,"skinnedMeshRenderer")){expected="Renderer";offset=0x50;}
    else if(!std::strcmp(name,"headRenderer")){expected="Renderer";offset=0x58;}
    else if(!std::strcmp(name,"characterAnimation")){expected="CharacterAnimation";offset=0x60;}
    else if(!std::strcmp(name,"rightWeaponHolder")){expected="Transform";offset=0x28;}
    else if(!std::strcmp(name,"leftWeaponHolder")){expected="Transform";offset=0x30;}
    Obj* value=offset?raw_field<Obj*>(object,offset):nullptr;
    return expected&&raw_type_named(value,expected)&&raw_field<void*>(value,0x10)?value:nullptr;
}
void prepare_chams(){
    if(chams_api.loaded||(!want_chams.load()&&!want_third_person.load()))return;
    static double retry=0;if(now()<retry)return;retry=now()+5;
    chams_api.decode=raw_decode_object;
    chams_api.find=[](int32_t id)->void*{return raw_fn<Obj*(*)(int32_t,const Method*)>(0xD4C4960)(id,nullptr);};
    chams_api.read=raw_read;
    chams_api.named_ref=raw_named_reference;
    chams_api.is_mesh=[](void* object){return raw_type_named(object,"SkinnedMeshRenderer")||raw_type_named(object,"MeshRenderer");};
    chams_api.store_ref=[](uintptr_t at,void* value){
        std::memcpy(reinterpret_cast<void*>(at),&value,sizeof(value));
        raw_fn<void(*)(uintptr_t)>(0x5C3793C)(at);
    };
    chams_api.xray_shader=[](){return shader_asset.id();};
    chams_api.shader_state=[](){return shader_asset.status;};
    chams_api.load(safe_graphics_icall);
    if(chams_api.loaded){
        graphics_roots.init([](const char* name){return api.resolver.get(name);},chams_api);
        chams_api.retain=[](int32_t id,uintptr_t p){graphics_roots.retain(id,p);};chams_api.release=[](int32_t id,uintptr_t p){graphics_roots.release(id,p);};
        chams_api.cached_native=[](int32_t id,uintptr_t p){return graphics_roots.live(id,p);};
    }
    LOG("Chams object API: ready=%d native_id_offset=%d force_offset=%d",chams_api.loaded,chams_api.native_id_offset,chams_api.force_off_offset);
}
Obj* raw_body_info(Obj* player){
    Obj* lease=raw_field<Obj*>(player,0x260);Obj* info=raw_field<Obj*>(lease,0x10);
    return raw_type_named(info,"PlayerModelInfo")&&raw_field<void*>(info,0x10)?info:nullptr;
}
void prepare_graphics_types(Obj* model_info,double time){
    if(!chams_api.loaded)return;
    static double next=0;if(time<next)return;next=time+.25;
    if(!graphics_metadata.methods){
        graphics_metadata.load([](const char* name){return api.resolver.get(name);},raw_read);
        graphics_metadata.executable=[](uintptr_t p){return api.resolver.executable(p);};
    }
    Obj* renderer=static_cast<Obj*>(raw_named_reference(model_info,"skinnedMeshRenderer"));
    if(renderer&&!chams_api.renderer_type){
        void* type=graphics_metadata.type_of(renderer);chams_api.renderer_type=type?graphics_metadata.base_type(type):nullptr;
        if(!raw_type_named(chams_api.renderer_type,"RuntimeType"))chams_api.renderer_type=nullptr;
        graphics_roots.type(0,chams_api.renderer_type);
    }
    if(!want_chams.load())return;
    if(!shader_runtime_type){
        void* source=nullptr;
        if(renderer){void* mats=chams_api.materials(chams_api.native(renderer));if(chams_api.count(mats)>0){void* m=chams_api.native(chams_api.item(mats,0));if(m)source=chams_api.decode(chams_api.get_shader(m));}}
        if(!source){auto name=mango_chams::span(u"Universal Render Pipeline/Unlit");source=chams_api.decode(chams_api.shader(&name));}
        if(source)shader_runtime_type=graphics_metadata.type_of(source);
        if(!raw_type_named(shader_runtime_type,"RuntimeType"))shader_runtime_type=nullptr;
        graphics_roots.type(1,shader_runtime_type);
    }
    if(shader_file_ready.load(std::memory_order_acquire)&&shader_asset.bind(safe_graphics_icall)){
        mango_chams::Span path{shader_file_path.data(),int32_t(shader_file_path.size())};
        shader_asset.update(chams_api,path,shader_runtime_type,time);
    }
    shader_stage.store(!graphics_metadata.methods?-8:!chams_api.renderer_type?-9:!shader_runtime_type?-10:!shader_file_ready.load()?-11:shader_asset.status);
    if(!graphics_metadata.methods||!chams_api.renderer_type||!shader_runtime_type)next=time+2;
}
void write_shader_file(){
    char path[256];std::snprintf(path,sizeof(path),"/data/user/%u/%s/files/cfesp-unlit-r12.bundle",unsigned(getuid())/100000,package);
    const int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0){LOG("Shader file creation failed: %d",errno);return;}
    size_t at=0;while(at<kChamsShaderBundleSize){const ssize_t n=write(fd,kChamsShaderBundle+at,kChamsShaderBundleSize-at);if(n<=0)break;at+=size_t(n);}
    const bool complete=close(fd)==0&&at==kChamsShaderBundleSize;
    if(!complete){unlink(path);return;}
    shader_file_path.clear();for(const unsigned char c:std::string(path))shader_file_path.push_back(char16_t(c));
    shader_file_ready.store(true,std::memory_order_release);
}

mango_camera::Api camera_api;mango_camera::Controller camera_controller;
mango_camera::ViewSetupApi view_setup_api;mango_camera::LocalViewSetup view_setup;
mango_camera::WeaponViewApi weapon_view_api;mango_camera::LocalWeaponView local_weapon_view;
mango_grip::Controller grip_controller;
mango_avatar::Api avatar_api;mango_avatar::Controller avatar_controller;
mango_graphics::PrefabAsset avatar_asset;
mango_avatar::Storage avatar_storage,avatar_storage_files; // Unity cache dir, and the app files dir the shader bundle already loads from
extern "C" const unsigned char cfesp_avatar_bundle[],cfesp_avatar_bundle_end[];
void write_avatar_file(){
    const size_t size=reinterpret_cast<uintptr_t>(cfesp_avatar_bundle_end)-reinterpret_cast<uintptr_t>(cfesp_avatar_bundle);
    avatar_storage.flush(cfesp_avatar_bundle,size);avatar_storage_files.flush(cfesp_avatar_bundle,size);
}
std::string narrow(const std::u16string& text){std::string out;for(char16_t c:text)out.push_back(c<128?char(c):'?');return out;}
// Unity prints why it refuses a bundle to logcat (tag Unity); the app may read its own log.
void capture_unity_log(int stage){
    static int captures=0;if(captures++>=4)return;
    FILE* pipe=popen("/system/bin/logcat -d -t 800 -s Unity:V 2>&1","r");
    if(!pipe){LOG("Unity log unavailable: %s",std::strerror(errno));return;}
    char line[400];unsigned kept=0;
    while(std::fgets(line,sizeof(line),pipe)&&kept<40){
        if(!(std::strstr(line,"ssetBundle")||std::strstr(line,"ssetbundle")||std::strstr(line,"CAB-")||std::strstr(line,"cfesp")||std::strstr(line,"erializ")||std::strstr(line,"orrupt")||std::strstr(line,"annot")||std::strstr(line,"ailed")))continue;
        size_t n=std::strlen(line);while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]=0;
        LOG("Unity says (stage %d): %s",stage,line);++kept;
    }
    pclose(pipe);if(!kept)LOG("Unity log has no bundle messages (stage %d)",stage);
}
void log_avatar_files(){
    static bool logged=false;if(logged)return;
    const int a=avatar_storage.state.load(),b=avatar_storage_files.state.load();if(a<=2&&b<=2)return;logged=true;
    for(const auto* storage:{&avatar_storage,&avatar_storage_files}){
        if(storage->state.load()!=3){LOG("Avatar file state=%d",storage->state.load());continue;}
        const std::string path=narrow(storage->path());struct stat info{};
        const bool ok=::stat(path.c_str(),&info)==0;
        LOG("Avatar file %s size=%lld expected=%zu readable=%d",path.c_str(),ok?(long long)info.st_size:-1LL,
            size_t(reinterpret_cast<uintptr_t>(cfesp_avatar_bundle_end)-reinterpret_cast<uintptr_t>(cfesp_avatar_bundle)),access(path.c_str(),R_OK)==0);
    }
}
void report_avatar(int stage){
    custom_model_status.store(stage);
    static int previous=-999,old_file=-999;
    const int file=avatar_storage.state.load();
    if(stage!=previous||file!=old_file){
        previous=stage;old_file=file;
        LOG("Avatar: stage=%d %s asset=%d attempt=%u file=%d human=%u target=%u meshes=%u active=%d",stage,mango_avatar::status_text(stage),avatar_asset.status,avatar_asset.attempts(),file,avatar_controller.human_count,avatar_controller.target_count,avatar_controller.mesh_count,avatar_controller.visible);
        log_avatar_files();
        if(stage<=-11&&stage>=-17)capture_unity_log(stage);
    }
}
void prepare_avatar_file(double time){
    static double retry=0;
    if(avatar_storage.state.load()>0||time<retry)return;retry=time+5;
    try{
        Obj* path=raw_fn<Obj*(*)(const Method*)>(0xD42F9F8)(nullptr);
        const int count=raw_field<int>(path,0x10);if(count<=0||count>400)return;
        std::array<char16_t,401> text{};if(!raw_read(reinterpret_cast<uintptr_t>(path)+0x14,text.data(),size_t(count)*2))return;
        std::string directory;for(int i=0;i<count;++i){if(!text[i]||text[i]>127)return;directory.push_back(char(text[i]));}
        avatar_storage.request(directory);
        char files[256];std::snprintf(files,sizeof(files),"/data/user/%u/com.catsbit.oxidesurvivalisland/files",unsigned(getuid())/100000);avatar_storage_files.request(files);
    }catch(...){LOG("Avatar: cache path unavailable");}
}
extern "C" const unsigned char cfesp_probe_0[],cfesp_probe_0_end[],cfesp_probe_1[],cfesp_probe_1_end[],cfesp_probe_2[],cfesp_probe_2_end[],cfesp_probe_3[],cfesp_probe_3_end[],
    cfesp_probe_4[],cfesp_probe_4_end[],cfesp_probe_5[],cfesp_probe_5_end[],cfesp_probe_6[],cfesp_probe_6_end[],cfesp_probe_7[],cfesp_probe_7_end[];
// Diagnostic: shows which serialized type makes Unity refuse the avatar bundle.
void probe_bundles(double time){
    static const unsigned char* const begin[8]={cfesp_probe_0,cfesp_probe_1,cfesp_probe_2,cfesp_probe_3,cfesp_probe_4,cfesp_probe_5,cfesp_probe_6,cfesp_probe_7};
    static const unsigned char* const end[8]={cfesp_probe_0_end,cfesp_probe_1_end,cfesp_probe_2_end,cfesp_probe_3_end,cfesp_probe_4_end,cfesp_probe_5_end,cfesp_probe_6_end,cfesp_probe_7_end};
    static const char* const parts[8]={"AssetBundle","+GameObject","+Transform","+Shader","+Material","+Texture2D","+Mesh","+SkinnedMeshRenderer"};
    static int index=0;static bool running=false;static std::u16string path;
    if(index>=8)return;
    if(!running){
        char name[300];std::snprintf(name,sizeof(name),"/data/user/%u/com.catsbit.oxidesurvivalisland/files/cfesp-probe-%d.bundle",unsigned(getuid())/100000,index);
        const int fd=open(name,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
        const size_t size=size_t(end[index]-begin[index]);bool ok=fd>=0;
        for(size_t at=0;ok&&at<size;){ssize_t n=write(fd,begin[index]+at,size-at);if(n<=0&&errno!=EINTR)ok=false;else if(n>0)at+=size_t(n);}
        if(fd>=0)close(fd);
        if(!ok){LOG("Probe %d (%s): write failed %s",index,parts[index],std::strerror(errno));++index;return;}
        path.clear();for(const char* c=name;*c;++c)path.push_back(char16_t(*c));
        const int result=avatar_asset.probe(chams_api,mango_chams::Span{path.data(),int32_t(path.size())},time,true);
        if(result<0){LOG("Probe %d (%s): start failed %d",index,parts[index],result);++index;return;}
        running=true;return;
    }
    const int result=avatar_asset.probe(chams_api,mango_chams::Span{path.data(),int32_t(path.size())},time,false);
    if(!result)return;
    LOG("Probe %d (%s): %s (%d)",index,parts[index],result>0?"ACCEPTED":"REJECTED",result);
    running=false;++index;if(index>=8)capture_unity_log(-99);
}
void update_avatar(Obj* local,const mango_camera::PlayerView& view,double time){
    static bool previously_enabled=false;
    if(!want_custom_model.load()){
        previously_enabled=false;avatar_asset.retry(time,true);avatar_controller.suspend(avatar_api);camera_controller.replace_body(camera_api,false);report_avatar(0);return;
    }
    const bool rising=!previously_enabled;previously_enabled=true;
    auto fail=[&](int stage){avatar_controller.hide(avatar_api);camera_controller.replace_body(camera_api,false);report_avatar(stage);};
    // The already displayed body is a usable source while the dedicated local
    // view initializes. Switching to it later rebinds the visual automatically.
    if(!view.info||!view.animator){fail(-1);return;}
    if(!avatar_api.load(safe_graphics_icall,camera_api)){fail(-2);return;}
    prepare_avatar_file(time);
    const int cache_state=avatar_storage.state.load(std::memory_order_acquire),files_state=avatar_storage_files.state.load(std::memory_order_acquire);
    // Prefer the files directory (the shader bundle loads from there); alternate on retries.
    const bool use_files=files_state==3&&(cache_state!=3||(avatar_asset.attempts()%2)==0);
    const auto& chosen=use_files?avatar_storage_files:avatar_storage;
    if(chosen.state.load(std::memory_order_acquire)!=3){fail(cache_state<0&&files_state<0?-3:2);return;}
    avatar_asset.retry(time,rising);
    try{
        if(avatar_asset.status>=0&&avatar_asset.status!=3&&avatar_asset.bind(safe_graphics_icall)){
            Obj* object=chams_api.decode(chams_api.game_object(chams_api.native(view.animator)));
            Obj* type=raw_type_named(object,"GameObject")?raw_fn<Obj*(*)(Obj*,const Method*)>(0xDB15300)(object,nullptr):nullptr;
            if(!raw_type_named(type,"RuntimeType")){fail(-4);return;}
            const auto& path=chosen.path();mango_chams::Span span{path.data(),int32_t(path.size())};
            avatar_asset.update(chams_api,span,type,time);
        }
        if(avatar_asset.status==-5&&avatar_asset.attempts()>=3)probe_bundles(time);
        if(avatar_asset.status<0){fail(avatar_asset.status-10);return;}
        if(avatar_asset.status!=3){fail(avatar_asset.status==2?4:3);return;}
        const bool visible=avatar_controller.update(avatar_api,view.animator,avatar_asset.id(),true,camera_controller.body_visible(),time);
        camera_controller.replace_body(camera_api,visible);
        const int pose=avatar_controller.status;
        report_avatar(visible?6:!camera_controller.body_visible()?9:pose==4?7:pose==5?8:pose<0?pose-20:5);
    }catch(...){fail(-25);}
}
using SphereCast=bool(*)(void*,mango_camera::V3*,float,mango_camera::V3*,float,RaycastHit*,int,int);
SphereCast camera_sphere=nullptr;
struct CameraCollision {Obj* local=nullptr,*previous=nullptr;double at=-1;float distance=0,length=0;mango_camera::V3 origin{},direction{};};CameraCollision camera_collision;
float camera_collision_distance(void* context,mango_camera::V3 origin,mango_camera::V3 direction,float max_distance){
    auto* ctx=static_cast<CameraCollision*>(context);
    const double time=now();
    if(ctx->local==ctx->previous&&time-ctx->at<.05&&mango_camera::length(mango_camera::sub(origin,ctx->origin))<.1f&&mango_camera::length(mango_camera::sub(direction,ctx->direction))<.1f&&std::fabs(max_distance-ctx->length)<.05f)return std::min(ctx->distance,max_distance);
    struct Remember {CameraCollision* ctx;mango_camera::V3 origin,direction;float length;double time;float result=0;~Remember(){ctx->previous=ctx->local;ctx->origin=origin;ctx->direction=direction;ctx->length=length;ctx->at=time;ctx->distance=result;}} remember{ctx,origin,direction,max_distance,time};
    std::array<uint64_t,2> scene{};
    if(!ctx->local||!raw_engine.capture_scene(ctx->local,scene))return 0;
    void* owner=raw_field<void*>(ctx->local,0x10);
    void* owner_transform=owner?raw_engine.unwrap_native(raw_engine.transform(owner)):nullptr;
    float traveled=0;
    for(unsigned attempt=0;attempt<2;++attempt){
        RaycastHit hit{};const float remaining=max_distance-traveled;
        if(remaining<=.05f)return remember.result=std::max(0.f,traveled-.15f);
        bool found=false;
        constexpr int mask=~((1<<2)|(1<<3)|(1<<8)|(1<<9)|(1<<10)|(1<<26));
        if(camera_sphere)found=camera_sphere(scene.data(),&origin,.14f,&direction,remaining,&hit,mask,1);
        else{Ray ray{{origin.x,origin.y,origin.z},{direction.x,direction.y,direction.z}};found=raw_engine.raycast(scene.data(),&ray,remaining,&hit,mask,1);}
        if(!found)return remember.result=max_distance;
        if(!std::isfinite(hit.distance)||hit.distance<0||hit.distance>remaining+.05f)return remember.result=std::max(0.f,traveled-.15f);
        void* collider=raw_engine.unwrap_native(raw_engine.collider(&hit.collider));
        void* transform=collider?raw_engine.unwrap_native(raw_engine.transform(collider)):nullptr;
        if(transform&&owner_transform&&(transform==owner_transform||raw_engine.is_child(transform,owner_transform))){
            const float advance=hit.distance+.3f;origin=mango_camera::add(origin,mango_camera::scale(direction,advance));traveled+=advance;continue;
        }
        return remember.result=std::max(0.f,traveled+hit.distance-.15f);
    }
    return remember.result=std::max(0.f,std::min(max_distance,traveled-.15f));
}
bool prepare_camera(){
    if(!want_third_person.load())return camera_api.loaded;
    if(!chams_api.loaded)return false;
    const auto resolve=safe_graphics_icall;
    if(!camera_api.load(resolve,chams_api))return false;
    camera_api.bone=[](void* animator,int human)->void*{
        try{
            if(!raw_fn<bool(*)(Obj*,const Method*)>(0xD4091E0)(animator,nullptr))return nullptr;
            return raw_fn<Obj*(*)(Obj*,int,const Method*)>(0xD40FE38)(animator,human,nullptr);
        }catch(...){return nullptr;}
    };
    view_setup_api.read=raw_read;view_setup_api.is_type=raw_type_named;
    view_setup_api.create=[](void* player){raw_fn<void(*)(Obj*,const Method*)>(0x6EFF164)(player,nullptr);};
    view_setup_api.bind_animation=[](void* motor,void* animation){
        const uintptr_t address=raw_type_named(motor,"KCC")?0x5F278E4:0x7CB4AD8;
        raw_fn<void(*)(Obj*,Obj*,const Method*)>(address)(motor,animation,nullptr);
    };
    view_setup_api.bind_equipment=[](void* equipment,void* view){raw_fn<void(*)(Obj*,Obj*,const Method*)>(0x6ED8FB8)(equipment,view,nullptr);};
    view_setup_api.active=camera_api.active_self;view_setup_api.set_active=camera_api.set_active;
    weapon_view_api.read=raw_read;weapon_view_api.is_type=raw_type_named;
    weapon_view_api.load=[](void* view,const mango_camera::WeaponPiece* piece){
        raw_fn<void(*)(Obj*,const mango_camera::WeaponPiece*,Obj*,const Method*)>(0x5F62D0C)(view,piece,nullptr,nullptr);
    };
    weapon_view_api.state=[](void* view,int state){raw_fn<void(*)(Obj*,int,const Method*)>(0x5F6209C)(view,state,nullptr);};
    camera_api.collision=camera_collision_distance;camera_api.collision_context=&camera_collision;
    if(!camera_sphere)camera_sphere=reinterpret_cast<SphereCast>(resolve("UnityEngine.PhysicsScene::Query_SphereCast_Injected(UnityEngine.PhysicsScene&,UnityEngine.Vector3&,System.Single,UnityEngine.Vector3&,System.Single,UnityEngine.RaycastHit&,System.Int32,UnityEngine.QueryTriggerInteraction)"));
    unity_camera_thread.store(pid_t(syscall(SYS_gettid)));return true;
}
bool raw_type_named(Obj* object,const char* wanted){
    Obj* klass=raw_field<Obj*>(object,0);
    for(unsigned depth=0;klass&&depth<16;++depth){
        const uintptr_t name=raw_field<uintptr_t>(klass,0x10);char text[80]{};
        if(name&&raw_read(name,text,sizeof(text)-1)&&!std::strcmp(text,wanted))return true;
        Obj* parent=raw_field<Obj*>(klass,0x58);if(parent==klass)break;klass=parent;
    }
    return false;
}

Obj* raw_static_fields(uintptr_t type_got){
    Obj* slot=raw_field<Obj*>(reinterpret_cast<Obj*>(api.base+type_got),0);
    Obj* klass=raw_field<Obj*>(slot,0);
    return raw_field<Obj*>(klass,0xB8);
}
bool raw_player_type(Obj* player,Obj* expected=nullptr){
    if(!expected){Obj* slot=raw_field<Obj*>(reinterpret_cast<Obj*>(api.base+mango_offsets::player_type_got),0);expected=raw_field<Obj*>(slot,0);}
    Obj* klass=raw_field<Obj*>(player,0);
    for(size_t depth=0;expected&&klass&&depth<16;++depth){
        if(klass==expected)return true;
        Obj* parent=raw_field<Obj*>(klass,0x58);if(parent==klass)break;klass=parent;
    }
    return false;
}
struct LegacyTouch {int32_t id;V2 position;uint8_t rest[24];int32_t phase;uint8_t tail[28];};
static_assert(sizeof(LegacyTouch)==68,"UnityEngine.Touch ARM64 layout");
bool raw_input_system_touch(Obj* touch,int w,int h,Pointer& s){
    if(!touch)return false;
    Obj* position_control=raw_field<Obj*>(touch,0x188);
    Obj* press_control=raw_field<Obj*>(touch,0x170);
    if(!position_control||!press_control)return false;
    const uint32_t optimized=raw_field<uint32_t>(position_control,0xD8);
    const uint32_t format=raw_field<uint32_t>(position_control,0x10);
    if(optimized!=0x56454332&&format!=0x56454332)return false;
    using State=void*(*)(Obj*,const Method*);
    using Pressed=bool(*)(Obj*,const Method*);
    void* state=raw_fn<State>(0xD023404)(position_control,nullptr);
    const uint32_t offset=raw_field<uint32_t>(position_control,0x14);
    V2 position{};const uintptr_t p=reinterpret_cast<uintptr_t>(state);
    if(!p||offset>=0x100000||offset>UINTPTR_MAX-p||
       !raw_read(p+offset,&position,sizeof(position))||
       !std::isfinite(position.x)||!std::isfinite(position.y))return false;
    if(!raw_fn<Pressed>(0xD0059F8)(press_control,nullptr))return false;
    if(!menu_hit(position.x,float(h)-position.y))return false;
    s.position={position.x/w,position.y/h};s.down=true;
    input_x.store(position.x);input_y.store(position.y);input_backend.store(2);
    return true;
}
void raw_input(Pointer& s){
    if(!frames.load())return;
    int w=egl_width.load(),h=egl_height.load();
    if(w<=0||h<=0||w>20000||h>20000)return;
    int touch_w=w,touch_h=h;
    uintptr_t cached_width=0,cached_height=0;
    raw_read(api.base+0xEA6DEA0,&cached_width,sizeof(cached_width));
    raw_read(api.base+0xEA6DEA8,&cached_height,sizeof(cached_height));
    if(cached_width&&cached_height){
        using Size=int(*)(const Method*);
        int screen_w=raw_fn<Size>(0xD44DE3C)(nullptr);
        int screen_h=raw_fn<Size>(0xD44DE64)(nullptr);
        if(screen_w>=180&&screen_h>=180&&screen_w<=20000&&screen_h<=20000){
            touch_w=screen_w;touch_h=screen_h;
        }
    }
    input_screen_width.store(touch_w);input_screen_height.store(touch_h);
    using Current=Obj*(*)(const Method*);
    Obj* touchscreen=raw_fn<Current>(0xD079374)(nullptr);
    input_devices.store(touchscreen?1:0);
    if(touchscreen){
        // Touchscreen.touches is a ReadOnlyArray: array pointer, start, length.
        Obj* array=raw_field<Obj*>(touchscreen,0x1C0);
        const int32_t start=raw_field<int32_t>(touchscreen,0x1C8);
        const int32_t count=raw_field<int32_t>(touchscreen,0x1CC);
        const size_t length=raw_field<size_t>(array,0x18);
        input_touches.store(count>=0&&count<=32?count:0);
        if(array&&start>=0&&count>0&&count<=32&&length<=32&&size_t(start)+size_t(count)<=length){
            // Only a finger over the menu may control ImGui. Gameplay fingers
            // remain with the game, including simultaneous movement and look.
            for(int32_t i=count-1;i>=0;--i){
                Obj* finger=raw_field<Obj*>(array,0x20+size_t(start+i)*sizeof(void*));
                if(raw_input_system_touch(finger,touch_w,touch_h,s))return;
            }
        }
        Obj* primary=raw_field<Obj*>(touchscreen,0x1B8);
        if(raw_input_system_touch(primary,touch_w,touch_h,s))return;
    }
    // Oxide also ships the legacy Unity Input API. Its Touch value is a
    // 68-byte ARM64 return-by-address struct; call only after startup settles.
    using TouchCount=int(*)(const Method*);
    using GetTouch=LegacyTouch(*)(int,const Method*);
    // The generated wrapper caches its Unity icall at this versioned slot.
    // Require the game to have used it first; a disabled legacy backend can
    // otherwise raise an unhandled managed exception from a direct call.
    uintptr_t cached_touch_count=0;
    raw_read(api.base+0xEA72F00,&cached_touch_count,sizeof(cached_touch_count));
    int count=cached_touch_count?raw_fn<TouchCount>(0xD54EAD0)(nullptr):-2;
    input_legacy_count.store(count);
    if(count>0&&count<=16){
        for(int i=0;i<count;++i){
            LegacyTouch touch=raw_fn<GetTouch>(0xD54E1C4)(i,nullptr);
            if(touch.phase<0||touch.phase>2||
               !std::isfinite(touch.position.x)||!std::isfinite(touch.position.y))continue;
            if(!menu_hit(touch.position.x,float(touch_h)-touch.position.y))continue;
            s.position={touch.position.x/touch_w,touch.position.y/touch_h};s.down=true;
            input_x.store(touch.position.x);input_y.store(touch.position.y);input_backend.store(1);
            return;
        }
    }
    uintptr_t cached_mouse=0,cached_button=0;
    raw_read(api.base+0xEA72F38,&cached_mouse,sizeof(cached_mouse));
    raw_read(api.base+0xEA72EA0,&cached_button,sizeof(cached_button));
    if(cached_mouse&&cached_button){
        using Mouse=V3(*)(const Method*);
        using Button=bool(*)(int,const Method*);
        if(raw_fn<Button>(0xD54E110)(0,nullptr)){
            V3 p=raw_fn<Mouse>(0xD54E5F8)(nullptr);
            if(finite(p)&&menu_hit(p.x,float(touch_h)-p.y)){
                s.position={p.x/touch_w,p.y/touch_h};s.down=true;
                input_x.store(p.x);input_y.store(p.y);input_backend.store(4);
                return;
            }
        }
    }
    if(native_touch_seen.load()&&menu_hit(native_touch_x.load(),native_touch_y.load())){
        s.position={native_touch_x.load()/w,1.f-native_touch_y.load()/h};
        s.down=native_touch_down.load();input_backend.store(3);
        input_x.store(native_touch_x.load());input_y.store(native_touch_y.load());
    }else{s.down=false;}
}
void raw_observe(Obj* self,const V3* origin=nullptr,const mango_esp::Projection* projection=nullptr,unsigned* bone_budget=nullptr){
    if(!self||!frames.load())return;
    Obj* native=raw_field<Obj*>(self,0x10);if(!native)return;
    Obj* animator=raw_field<Obj*>(self,0x1B0);
    Obj* native_animator=raw_field<Obj*>(animator,0x10);
    const double sample_time=now();
    using Human=bool(*)(Obj*,const Method*);
    using Bone=Obj*(*)(Obj*,int,const Method*);
    using Position=V3(*)(Obj*,const Method*);
    const bool mine=self==local_player.load();
    V3 previous_feet{};
    RawObservation observation{};observation.address=self;observation.stamp=sample_time;observation.local=mine;
    observation.native_address=native;observation.animator=animator;observation.native_animator=native_animator;
    {   // Preserve the rig classification and latest valid head offset.
        std::lock_guard<std::mutex> lock(raw_observations_mutex);
        auto it=std::find_if(raw_observations.begin(),raw_observations.end(),
            [self](const RawObservation& entry){return entry.address==self;});
        if(it!=raw_observations.end()&&it->native_address==native){
            if(it->animator==animator&&it->native_animator==native_animator){
                observation.human=it->human;
                observation.head_offset=it->head_offset;
                observation.head_pad=it->head_pad;
                observation.chest_offset=it->chest_offset;observation.chest_valid=it->chest_valid;
                observation.head_at=it->head_at;observation.bones_at=it->bones_at;observation.human_at=it->human_at;
                observation.world.bones=it->world.bones;observation.world.valid=it->world.valid;previous_feet=it->world.feet;
            }
            observation.team_at=it->team_at;std::memcpy(observation.team,it->team,sizeof(observation.team));
            observation.visibility=it->visibility;observation.visibility_at=it->visibility_at;
        }
    }
    if(mine){local_player.store(self);local_player_seen.store(sample_time);}
    if(want_magic_bullet.load()&&(!observation.team[0]||sample_time-observation.team_at>5)){
        raw_team_tag(raw_field<Obj*>(self,0x2B0),observation.team,sizeof(observation.team));
        observation.team_at=sample_time;
    }
    if(!mine&&(want_esp.load()||want_magic_bullet.load()||want_chams.load())){
        Player& player=observation.world;
        const float collection_range=std::max((want_esp.load()||want_magic_bullet.load())?esp_range.load():0.f,want_chams.load()?chams_range.load():0.f);
        if(raw_position(self,player.feet)&&(!origin||distance(*origin,player.feet)<=collection_range+3.f)){
            V3 probe{};V3 probe_head=player.feet;probe_head.y+=1.4f;
            const bool onscreen=!projection||!projection->valid||
                (projection->project(probe_head,probe)&&probe.x>=-.1f&&probe.x<=1.1f&&probe.y>=-.2f&&probe.y<=1.2f);
            if(!onscreen&&!want_magic_bullet.load()){
                observation.visible=false;
            }else{
            if(native_animator){
                V3 visual_root{};
                if(raw_position(animator,visual_root)&&distance(visual_root,player.feet)<3.f)player.feet=visual_root;
            }
            if(native_animator&&(observation.human<0||(observation.human==0&&sample_time-observation.human_at>.5))){
                observation.human=raw_fn<Human>(0xD4091E0)(animator,nullptr)?1:0;observation.human_at=sample_time;
            }
            if(player.valid){
                const V3 delta{player.feet.x-previous_feet.x,player.feet.y-previous_feet.y,player.feet.z-previous_feet.z};
                for(size_t b=0;b<bone_count;++b)if(player.valid&(1u<<b)){
                    player.bones[b].x+=delta.x;player.bones[b].y+=delta.y;player.bones[b].z+=delta.z;
                }
            }
            if(native_animator&&observation.human==1){
                // Bones are stored as an offset from the transform position. A
                // culled or late animator cannot leave the box top behind
                // the player; the offset is applied to this frame's
                // networked position.
                if(sample_time-observation.head_at>=.05){
                observation.head_at=sample_time;
                Obj* head=raw_fn<Bone>(0xD40FE38)(animator,10,nullptr);
                if(head){
                    const V3 world=raw_fn<Position>(0xD4CFB20)(head,nullptr);
                    const V3 offset{world.x-player.feet.x,world.y-player.feet.y,world.z-player.feet.z};
                    if(finite(world)&&offset.y>-.2f&&offset.y<2.6f&&
                       std::fabs(offset.x)<1.5f&&std::fabs(offset.z)<1.5f){
                        observation.head_offset=offset;observation.head_pad=.14f;
                    }
                }
                if(magic_point.load()==1||want_skeleton.load()||want_visibility.load()){
                    Obj* chest=raw_fn<Bone>(0xD40FE38)(animator,8,nullptr);
                    if(chest){
                        const V3 world=raw_fn<Position>(0xD4CFB20)(chest,nullptr);
                        const V3 offset{world.x-player.feet.x,world.y-player.feet.y,world.z-player.feet.z};
                        if(finite(world)&&offset.y>.2f&&offset.y<2.2f&&
                           std::fabs(offset.x)<1.5f&&std::fabs(offset.z)<1.5f){
                            observation.chest_offset=offset;observation.chest_valid=true;
                        }
                    }
                }
                }
                if(want_skeleton.load()&&onscreen&&(!origin||distance(*origin,player.feet)<=std::min(esp_range.load(),150.f)+3.f)&&
                   (!bone_budget||*bone_budget)&&sample_time-observation.bones_at>=.05){
                    if(bone_budget)--*bone_budget;observation.bones_at=sample_time;player.valid=0;
                    for(size_t b=0;b<bone_count;++b){
                    Obj* transform=raw_fn<Bone>(0xD40FE38)(animator,bone_ids[b],nullptr);
                    if(!transform)continue;
                    V3 world=raw_fn<Position>(0xD4CFB20)(transform,nullptr);
                    if(finite(world)){player.bones[b]=world;player.valid|=1u<<b;}
                }
                }
            }
            player.head={player.feet.x+observation.head_offset.x,
                         player.feet.y+observation.head_offset.y,
                         player.feet.z+observation.head_offset.z};
            observation.chest={player.feet.x+observation.chest_offset.x,
                               player.feet.y+observation.chest_offset.y,
                               player.feet.z+observation.chest_offset.z};
            observation.visible=true;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(raw_observations_mutex);
        auto it=std::find_if(raw_observations.begin(),raw_observations.end(),
            [self](const RawObservation& entry){return entry.address==self;});
        if(it!=raw_observations.end())*it=observation;
        else if(raw_observations.size()<max_players)raw_observations.push_back(observation);
    }
    unsigned cost=unsigned(std::max(0.0,(now()-sample_time)*1000000));
    unsigned old=max_observe_us.load();while(old<cost&&!max_observe_us.compare_exchange_weak(old,cost)){}
}
Snapshot publish_scratch; // Unity callbacks only
void raw_publish(int phase=0){
    static thread_local bool entered=false;if(entered)return;entered=true;
    struct Leave {bool& entered;~Leave(){entered=false;}} leave{entered};
    static mango_esp::FrameGate frame_gate;
    const int frame=raw_fn<int(*)(const Method*)>(0xD4B577C)(nullptr);
    if(!frame_gate.accept(frame,phase))return;
    if(phase>0)last_late_frame.store(frame);
    const double time=now();
    raw_engine.load();
    prepare_chams();
    static double last_input=0;
    if(time-last_input>=1.0/120){
        last_input=time;Pointer pointer;
        {std::lock_guard<std::mutex> lock(pointer_mutex);pointer=published_pointer;}
        pointer.down=false;raw_input(pointer);pointer.stamp=time;
        {std::lock_guard<std::mutex> lock(pointer_mutex);published_pointer=pointer;}
    }
    Obj* controller=raw_static_fields(mango_offsets::controller_type_got);
    Obj* local=raw_field<Obj*>(controller,0x10);
    Obj* native_local=raw_field<Obj*>(local,0x10);
    if(!native_local)local=nullptr;
    const bool static_local=local!=nullptr;
    static std::vector<uintptr_t> callback_players;
    callback_players.clear();double newest_local=-1;
    live_players.visit(time,[&](const mango_esp::LivePlayers::Entry& entry){
        Obj* player=reinterpret_cast<Obj*>(entry.address);
        if(raw_field<uintptr_t>(player,0x10)!=entry.native)return;
        callback_players.push_back(entry.address);
        if(!static_local&&entry.local&&entry.stamp>newest_local){
            newest_local=entry.stamp;local=player;native_local=reinterpret_cast<Obj*>(entry.native);
        }
    });
    Obj* manager=raw_field<Obj*>(controller,0x38);
    Obj* camera=raw_field<Obj*>(manager,0x20);
    if(local&&!raw_field<Obj*>(camera,0x10)){
        using Camera=Obj*(*)(const Method*);
        camera=raw_fn<Camera>(0xD43AB4C)(nullptr);
    }
    Obj* native_camera=raw_field<Obj*>(camera,0x10);
    if(want_third_person.load()){
        Obj* main=raw_fn<Obj*(*)(const Method*)>(0xD43AB4C)(nullptr);
        if(raw_field<Obj*>(main,0x10)){camera=main;native_camera=raw_field<Obj*>(main,0x10);}
    }
    if(!native_camera||!local)camera=nullptr;
    Obj* list=raw_field<Obj*>(raw_static_fields(mango_offsets::player_type_got),0x10);
    const mango_esp::SessionKey key{reinterpret_cast<uintptr_t>(list),reinterpret_cast<uintptr_t>(local),
        reinterpret_cast<uintptr_t>(camera),reinterpret_cast<uintptr_t>(native_local),
        camera?reinterpret_cast<uintptr_t>(native_camera):0};
    if(raw_session.update(key)){
        {std::lock_guard<std::mutex> lock(raw_observations_mutex);raw_observations.clear();}
        live_players.clear();callback_players.clear();
        chams_controller.reset(chams_api);
        avatar_controller.reset(avatar_api);grip_controller.reset(camera_api);
        third_camera_transform.store(0);camera_controller.reset(camera_api);
        local_player.store(nullptr);local_player_seen.store(0);
        LOG("Session %llu rebound: list=%p local=%p camera=%p",static_cast<unsigned long long>(raw_session.generation()),list,local,camera);
    }
    local_player.store(local);local_player_seen.store(local?time:0);
    raw_camera_seen.store(camera!=nullptr);
    Obj* body_info=(want_third_person.load()||want_chams.load())?raw_body_info(local):nullptr;
    const double graphics_start=now();prepare_graphics_types(body_info,time);
    chams_cpu_ms.store(float((now()-graphics_start)*1000)*.2f+chams_cpu_ms.load()*.8f);
    const double camera_start=now();
    if(!want_third_person.load()||!local||!camera){
        avatar_controller.suspend(avatar_api);avatar_asset.retry(time,true);grip_controller.reset(camera_api);report_avatar(0);
        third_camera_transform.store(0);camera_controller.reset(camera_api);
        local_weapon_view.reset();
        if(view_setup_api.read)view_setup.update(view_setup_api,local,false,time);
    }else if(phase>0){
        if(prepare_camera()){
            camera_collision.local=local;
            view_setup.update(view_setup_api,local,true,time);
            static int previous_setup=-100;
            if(view_setup.status!=previous_setup){previous_setup=view_setup.status;LOG("Third person setup: %d (3=live animation and equipment bound)",view_setup.status);}
            Obj* fp=raw_field<Obj*>(local,0x90);
            if(!raw_type_named(fp,"FPManager"))fp=nullptr;
            Obj* weapon=raw_field<Obj*>(fp,0x50);
            if(!raw_type_named(weapon,"FPObject"))weapon=nullptr;
            const auto view=mango_camera::player_view(local,raw_read,raw_type_named);
            local_weapon_view.update(weapon_view_api,local,view,true,time);
            static int previous_weapon=-100;
            if(local_weapon_view.status!=previous_weapon){previous_weapon=local_weapon_view.status;LOG("Third person weapon: %d (2=loading, 3=ready)",previous_weapon);}
            static void* previous_model=nullptr;
            if(view.info!=previous_model){previous_model=view.info;LOG("Third person model: local=%d info=%p animator=%p fp_pass=%p layers=%08x",view.local,view.info,view.animator,view.render_controller,view.reserved_layers);}
            camera_controller.update(camera_api,camera,native_local,raw_field<void*>(fp,0x10),
                raw_field<void*>(weapon,0x10),true,camera_distance.load(),frame,time,view.animator,view.info,view.render_controller,view.reserved_layers,view.local,local_weapon_view.visual);
            update_avatar(local,view,time);
            third_camera_transform.store(camera_controller.native_transform());
        }else camera_controller.status=-1;
    }
    camera_status.store(camera_controller.status);camera_meshes.store(camera_controller.body_meshes);
    camera_cpu_ms.store(float((now()-camera_start)*1000)*.2f+camera_cpu_ms.load()*.8f);
    Snapshot& next=publish_scratch;
    next.count=0;next.total=0;next.local=local!=nullptr;next.camera=camera!=nullptr;
    next.generation=raw_session.generation();next.stamp=time;
    V3 origin{};
    const bool camera_position=camera&&raw_position(camera,origin);
    static std::vector<uintptr_t> current;
    current.clear();
    bool list_valid=local&&camera_position&&mango_esp::visit_list(reinterpret_cast<uintptr_t>(list),
        [](uintptr_t at,void* out,size_t size){return raw_read(at,out,size);},
        [&](uintptr_t object){if(current.size()<4096)current.push_back(object);});
    registry_healthy.store(list_valid&&!current.empty());
    if(local&&camera_position&&(!list_valid||current.empty())&&!callback_players.empty()){
        current=callback_players;list_valid=true;
    }
    if(list_valid){
        next.total=current.size();
        std::sort(current.begin(),current.end());
        current.erase(std::unique(current.begin(),current.end()),current.end());
        {
            std::lock_guard<std::mutex> lock(raw_observations_mutex);
            raw_observations.erase(std::remove_if(raw_observations.begin(),raw_observations.end(),
                [&](const RawObservation& old){return !std::binary_search(current.begin(),current.end(),reinterpret_cast<uintptr_t>(old.address));}),raw_observations.end());
        }
        // Re-read visual transforms after the game's updates, instead of
        // projecting positions retained by the previous Update callback.
        const auto projection=raw_engine.capture_camera(camera);
        Obj* type_slot=raw_field<Obj*>(reinterpret_cast<Obj*>(api.base+mango_offsets::player_type_got),0);
        Obj* expected_type=raw_field<Obj*>(type_slot,0);
        unsigned bone_budget=10;
        if(raw_player_type(local,expected_type))raw_observe(local,&origin,&projection,&bone_budget);
        size_t sampled=0;
        for(uintptr_t address:current){
            Obj* player=reinterpret_cast<Obj*>(address);
            if(!want_esp.load()&&!want_magic_bullet.load()&&!want_chams.load())break;
            if(player==local||!raw_player_type(player,expected_type))continue;
            if(sampled++>=max_players-1)break;
            raw_observe(player,&origin,&projection,&bone_budget);
        }
        auto project_current=[&](V3 world,V3& out){
            if(projection.valid)return projection.project(world,out);
            if(!raw_project(camera,world,out))return false;
            const auto& rect=projection.rect;
            out.x=rect.x+out.x*rect.w;out.y=rect.y+out.y*rect.h;
            return finite(out);
        };
        std::array<uint64_t,2> physics{};
        const bool physics_valid=want_visibility.load()&&
            (raw_engine.capture_scene(local,physics)||raw_engine.capture_scene(camera,physics));
        const float range=std::max(want_esp.load()?esp_range.load():0.f,want_chams.load()?chams_range.load():0.f);
        const bool skeleton=want_skeleton.load(),draw=want_esp.load();
        std::lock_guard<std::mutex> lock(raw_observations_mutex);
        // Refresh the oldest visibility results first; the budget is shared
        // by all players, so a crowded server cannot starve later entries.
        static std::vector<RawObservation*> checks;checks.clear();
        if(physics_valid)for(auto& entry:raw_observations){
            if(!entry.local&&entry.visible&&time-entry.stamp<.1&&
               distance(entry.world.feet,origin)<=range&&time-entry.visibility_at>=.1)checks.push_back(&entry);
        }
        std::sort(checks.begin(),checks.end(),[](const RawObservation* a,const RawObservation* b){return a->visibility_at<b->visibility_at;});
        static int budget_frame=-1;static mango_unity::RayBudget ray_budget;
        if(budget_frame!=frame){budget_frame=frame;ray_budget={};}
        const double visibility_start=now();
        for(size_t i=0;i<checks.size()&&ray_budget.remaining&&now()-visibility_start<.00035;++i){
            auto& entry=*checks[i];
            const auto result=raw_engine.visibility(entry.address,local,origin,entry.world.head,entry.chest,physics,&ray_budget);
            if(!ray_budget.exhausted){entry.visibility=result;entry.visibility_at=time;}
        }
        unsigned skeletons=0;
        for(const auto& entry:raw_observations){
            if(!draw||entry.local||!entry.visible||time-entry.stamp>.1||next.count>=max_players)continue;
            Player& player=next.players[next.count];
            player.meters=distance(entry.world.feet,origin);
            if(!std::isfinite(player.meters)||player.meters>range+1.f)continue;
            V3 top=entry.world.head;top.y+=entry.head_pad;
            if(!project_current(entry.world.feet,player.feet)||!project_current(top,player.head))continue;
            player.visibility=physics_valid&&time-entry.visibility_at<.5?entry.visibility:mango_esp::Visibility::Unknown;
            player.valid=0;
            if(skeleton&&entry.world.valid&&skeletons<10&&player.meters<=150.f){
                ++skeletons;
                for(size_t b=0;b<bone_count;++b){
                    if(!(entry.world.valid&(1u<<b)))continue;
                    V3 screen{};
                    if(project_current(entry.world.bones[b],screen)){player.bones[b]=screen;player.valid|=1u<<b;}
                }
            }
            ++next.count;
        }
    }else{
        std::lock_guard<std::mutex> lock(raw_observations_mutex);raw_observations.clear();
    }
    if((want_chams.load()||chams_api.loaded)&&(!want_chams.load()||chams_controller.due(time))){
        static std::vector<mango_chams::Candidate> candidates;candidates.clear();
        if(want_chams.load()&&camera_position){
            std::lock_guard<std::mutex> lock(raw_observations_mutex);
            for(const auto& entry:raw_observations)if(!entry.local&&time-entry.stamp<.2){
                const float meters=distance(entry.world.feet,origin);
                const auto visibility=time-entry.visibility_at<.5?entry.visibility:mango_esp::Visibility::Unknown;
                const int state=!want_visibility.load()||visibility==mango_esp::Visibility::Visible?0:
                    (visibility==mango_esp::Visibility::Occluded?1:2);
                if(std::isfinite(meters)&&meters<=chams_range.load())candidates.push_back({
                    reinterpret_cast<uintptr_t>(entry.address),reinterpret_cast<uintptr_t>(entry.native_address),meters,state,reinterpret_cast<uintptr_t>(entry.native_animator)});
            }
        }
        mango_chams::Options options{want_chams.load(),true,chams_alpha.load(),chams_range.load(),unsigned(chams_limit.load())};
        const double start=now();
        chams_controller.update(chams_api,options,candidates,time);
        chams_cpu_ms.store(float((now()-start)*1000)*.2f+chams_cpu_ms.load()*.8f);
        chams_status.store(chams_controller.status);chams_renderers.store(chams_controller.renderer_count);
        chams_shader.store(chams_controller.shader_kind);
        chams_creations.store(chams_controller.material_creations);chams_restore_errors.store(chams_controller.restore_errors);
    }
    {std::lock_guard<std::mutex> lock(snapshot_mutex);published.assign(next);}
    player_features_tick(time);
    unsigned cost=unsigned(std::max(0.0,(now()-time)*1000000));
    capture_samples.fetch_add(1);capture_total_us.fetch_add(cost);
    unsigned old=max_publish_us.load();while(old<cost&&!max_publish_us.compare_exchange_weak(old,cost)){}
    capture_frames.fetch_add(1);
}

bool ray_to_point(V3 origin,V3 point,V3& direction){
    if(!finite(origin)||!finite(point))return false;
    const V3 delta{point.x-origin.x,point.y-origin.y,point.z-origin.z};
    const float length=distance(origin,point);
    if(!(length>.01f)||!std::isfinite(length))return false;
    direction={delta.x/length,delta.y/length,delta.z/length};
    return finite(direction);
}
bool magic_target(Obj* camera,V3 origin,V3& point){
    const double time=now();
    static thread_local double cached_at=0;
    static thread_local Obj* cached_camera=nullptr;
    static thread_local bool cached_found=false;
    static thread_local V3 cached_point{};
    if(camera==cached_camera&&time-cached_at<1.0/45){
        if(cached_found)point=cached_point;
        return cached_found;
    }
    cached_at=time;cached_camera=camera;cached_found=false;
    const int width=egl_width.load(),height=egl_height.load();
    if(!camera||width<180||height<180||!finite(origin))return false;
    Obj* local=static_cast<Obj*>(local_player.load());
    if(!local||time-local_player_seen.load()>1.0)return false;
    static thread_local Obj* team_owner=nullptr;
    static thread_local double team_at=0;
    static thread_local char own_team[48]{};
    if(team_owner!=local||time-team_at>5){
        raw_team_tag(raw_field<Obj*>(local,0x2B0),own_team,sizeof(own_team));
        team_owner=local;team_at=time;
    }
    const bool team_filter=want_magic_team_filter.load();
    const int point_mode=magic_point.load();
    const float max_distance=esp_range.load();
    const float fov=magic_fov.load();
    float best=fov*fov;
    {
        std::lock_guard<std::mutex> lock(raw_observations_mutex);
        for(const auto& entry:raw_observations){
            if(entry.local||!entry.visible||time-entry.stamp>.25)continue;
            if(team_filter&&own_team[0]&&entry.team[0]&&!std::strcmp(own_team,entry.team))continue;
            const V3 world=point_mode==1&&entry.chest_valid?entry.chest:entry.world.head;
            if(distance(origin,world)>max_distance)continue;
            V3 view{};if(!raw_project(camera,world,view))continue;
            const float x=(view.x-.5f)*float(width),y=(view.y-.5f)*float(height);
            const float score=x*x+y*y;
            if(score>=best)continue;
            best=score;cached_point=world;cached_found=true;
        }
    }
    magic_targets.store(cached_found?1:0);
    if(cached_found)point=cached_point;
    return cached_found;
}
using RawRay=Ray(*)(Obj*,const Method*);
RawRay original_ray_lsb=nullptr,original_ray_zzc=nullptr;
Ray magic_ray(Obj* self,const Method* method,RawRay original){
    Ray ray=original?original(self,method):Ray{};
    if(want_third_person.load()&&syscall(SYS_gettid)==unity_camera_thread.load()){
        const auto origin=camera_controller.undo_ray_origin({ray.origin.x,ray.origin.y,ray.origin.z});
        ray.origin={origin.x,origin.y,origin.z};
    }
    if(!want_magic_bullet.load()||menu_is_open.load()||!game_ready.load()||!self)return ray;
    Obj* local=static_cast<Obj*>(local_player.load());
    if(!local||raw_field<Obj*>(self,0x20)!=local)return ray;
    if(want_magic_ads_only.load()){
        Obj* events=raw_field<Obj*>(local,0x78);
        Obj* action=events?raw_field<Obj*>(events,0x2B0):nullptr;
        if(!action||!raw_field<uint8_t>(action,0x10))return ray;
    }
    Obj* camera=raw_field<Obj*>(self,0x30);
    V3 target{},direction{};
    if(!magic_target(camera,ray.origin,target)||
       !ray_to_point(ray.origin,target,direction))return ray;
    ray.direction=direction;
    magic_rewrites.fetch_add(1,std::memory_order_relaxed);
    return ray;
}
Ray raw_hook_ray_lsb(Obj* self,const Method* method){
    return magic_ray(self,method,original_ray_lsb);
}
Ray raw_hook_ray_zzc(Obj* self,const Method* method){
    return magic_ray(self,method,original_ray_zzc);
}
using HitRay=void(*)(Obj*,Ray,const Method*);
HitRay original_hit_zer=nullptr,original_hit_zmm=nullptr,original_hit_loa=nullptr;
void magic_hit_ray(Obj* self,Ray ray,const Method* method,HitRay original){
    if(want_third_person.load()&&syscall(SYS_gettid)==unity_camera_thread.load()){
        const auto origin=camera_controller.undo_ray_origin({ray.origin.x,ray.origin.y,ray.origin.z});
        ray.origin={origin.x,origin.y,origin.z};
    }
    magic_shot_calls.fetch_add(1,std::memory_order_relaxed);
    if(want_magic_bullet.load()&&!menu_is_open.load()&&game_ready.load()&&self){
        Obj* local=static_cast<Obj*>(local_player.load());
        if(local&&raw_field<Obj*>(self,0xC0)==local){ // FPObject.Player
            Obj* events=raw_field<Obj*>(local,0x78);
            Obj* action=events?raw_field<Obj*>(events,0x2B0):nullptr;
            const bool ads=action&&raw_field<uint8_t>(action,0x10)!=0;
            if(!want_magic_ads_only.load()||ads){
                Obj* camera=raw_field<Obj*>(self,0x270); // FPHitscan.KLj
                if(!camera){
                    Obj* raycast=raw_field<Obj*>(self,0x90); // FPObject.Kuk
                    camera=raycast?raw_field<Obj*>(raycast,0x30):nullptr;
                }
                V3 target{},direction{};
                if(magic_target(camera,ray.origin,target)&&
                   ray_to_point(ray.origin,target,direction)){
                    ray.direction=direction;
                    magic_shot_rewrites.fetch_add(1,std::memory_order_relaxed);
                }
            }
        }
    }
    if(original)original(self,ray,method);
}
void raw_hook_hit_zer(Obj* self,Ray ray,const Method* method){
    magic_hit_ray(self,ray,method,original_hit_zer);
}
void raw_hook_hit_zmm(Obj* self,Ray ray,const Method* method){
    magic_hit_ray(self,ray,method,original_hit_zmm);
}
void raw_hook_hit_loa(Obj* self,Ray ray,const Method* method){
    magic_hit_ray(self,ray,method,original_hit_loa);
}
using RawCallback=void(*)(Obj*,const Method*);
using RawCanvas=void(*)(const Method*);
RawCallback original_raw_update=nullptr;
RawCanvas original_raw_canvas=nullptr;
RawCanvas original_raw_before=nullptr;
using RawBeginCamera=void(*)(void*,Obj*,const Method*);
RawBeginCamera original_raw_begin_camera=nullptr;
void raw_hook_begin_camera(void* context,Obj* camera,const Method* method){
    if(original_raw_begin_camera)original_raw_begin_camera(context,camera,method);
    // URP calls this after its beginCameraRendering subscribers and before
    // culling. Reassert cached local render state only for the selected camera.
    static thread_local const pid_t callback_thread=pid_t(syscall(SYS_gettid));
    if(want_third_person.load(std::memory_order_relaxed)&&callback_thread==unity_camera_thread.load()&&camera_controller.shifted()&&camera_controller.selected(camera_api,camera)){
        camera_controller.before_cull(camera_api,camera);
        if(camera_controller.body_visible()){
            Obj* player=static_cast<Obj*>(local_player.load());const auto view=mango_camera::player_view(player,raw_read,raw_type_named);
            Obj* pivot=mango_camera::left_hand_grip(weapon_view_api,player,view);
            if(pivot)grip_controller.apply(camera_api,view.animator,raw_field<void*>(pivot,0x10),camera_controller.frame(),now());
            else grip_controller.restore(camera_api);
        }else grip_controller.restore(camera_api);
        if(want_custom_model.load()&&avatar_controller.visible&&camera_controller.body_visible()){
            const bool visible=avatar_controller.pose(avatar_api);camera_controller.replace_body(camera_api,visible);
        }
    }
}
void raw_hook_update(Obj* self,const Method* method){
    static thread_local const pid_t callback_thread=pid_t(syscall(SYS_gettid));
    if(third_camera_transform.load(std::memory_order_relaxed)&&callback_thread==unity_camera_thread.load()&&camera_controller.shifted()){
        const int frame=raw_fn<int(*)(const Method*)>(0xD4B577C)(nullptr);
        grip_controller.begin_frame(camera_api,frame);camera_controller.begin_frame(camera_api,frame);
    }
    if(original_raw_update)original_raw_update(self,method);
    if(self&&!registry_healthy.load()){
        const auto native=raw_field<uintptr_t>(self,0x10);
        if(native){
            using IsLocal=bool(*)(Obj*,const Method*);
            live_players.remember({reinterpret_cast<uintptr_t>(self),native,now(),raw_fn<IsLocal>(0xC8D8774)(self,nullptr)});
        }
    }
    // Keep publishing if the post-LateUpdate canvas callback is unavailable.
    const double time=now();
    if(time-last_canvas_call.load()>.03&&time-last_before_call.load()>.03){
        const int frame=raw_fn<int(*)(const Method*)>(0xD4B577C)(nullptr),late=last_late_frame.load();
        if(frame<late||frame-late>2)raw_publish(0);
    }
}
void raw_hook_canvas(const Method* method){
    if(original_raw_canvas)original_raw_canvas(method);
    last_canvas_call.store(now());
    raw_publish(2);
}
void raw_hook_before(const Method* method){
    if(original_raw_before)original_raw_before(method);
    const double time=now();last_before_call.store(time);
    raw_publish(1);
}
bool install_raw_hooks(){
    static bool player_installed=false,canvas_installed=false,before_installed=false;
    static bool lsb_installed=false,zzc_installed=false;
    static bool zer_installed=false,zmm_installed=false,loa_installed=false;
    static bool begin_camera_installed=false;
    int player=0,canvas=0,lsb=0,zzc=0;
    int zer=0,zmm=0,loa=0;
    if(!player_installed){
        player=hook(reinterpret_cast<void*>(api.base+0x6F01500),
            reinterpret_cast<void*>(raw_hook_update),reinterpret_cast<void**>(&original_raw_update));
        player_installed=player==0;
    }
    if(!canvas_installed){
        canvas=hook(reinterpret_cast<void*>(api.base+0xD844580),
            reinterpret_cast<void*>(raw_hook_canvas),reinterpret_cast<void**>(&original_raw_canvas));
        canvas_installed=canvas==0;
    }
    if(!before_installed){
        const int result=hook(reinterpret_cast<void*>(api.base+0xD431BF8),
            reinterpret_cast<void*>(raw_hook_before),reinterpret_cast<void**>(&original_raw_before));
        before_installed=result==0;
    }
    if(!begin_camera_installed){
        const int result=hook(reinterpret_cast<void*>(api.base+0xD4FA514),reinterpret_cast<void*>(raw_hook_begin_camera),reinterpret_cast<void**>(&original_raw_begin_camera));
        begin_camera_installed=result==0;
    }
    if(!lsb_installed){
        lsb=hook(reinterpret_cast<void*>(api.base+0x6D8F10C),
            reinterpret_cast<void*>(raw_hook_ray_lsb),reinterpret_cast<void**>(&original_ray_lsb));
        lsb_installed=lsb==0;
    }
    if(!zzc_installed){
        zzc=hook(reinterpret_cast<void*>(api.base+0x6D8F698),
            reinterpret_cast<void*>(raw_hook_ray_zzc),reinterpret_cast<void**>(&original_ray_zzc));
        zzc_installed=zzc==0;
    }
    if(!zer_installed){
        zer=hook(reinterpret_cast<void*>(api.base+0x6D44094),
            reinterpret_cast<void*>(raw_hook_hit_zer),reinterpret_cast<void**>(&original_hit_zer));
        zer_installed=zer==0;
    }
    if(!zmm_installed){
        zmm=hook(reinterpret_cast<void*>(api.base+0x6D4CD20),
            reinterpret_cast<void*>(raw_hook_hit_zmm),reinterpret_cast<void**>(&original_hit_zmm));
        zmm_installed=zmm==0;
    }
    if(!loa_installed){
        loa=hook(reinterpret_cast<void*>(api.base+0x6D4A230),
            reinterpret_cast<void*>(raw_hook_hit_loa),reinterpret_cast<void**>(&original_hit_loa));
        loa_installed=loa==0;
    }
    magic_hook_state.store((lsb_installed?1:0)|(zzc_installed?2:0)|
                           (zer_installed?4:0)|(zmm_installed?8:0)|(loa_installed?16:0));
    raw_hooks_complete.store(player_installed&&canvas_installed&&before_installed&&begin_camera_installed);
    LOG("Raw callbacks: Player=%d Canvas=%d Before=%d BeginCamera=%d Ray=%d/%d Hitscan=%d/%d/%d",
        player,canvas,before_installed,begin_camera_installed,lsb,zzc,zer,zmm,loa);
    if(player_installed||canvas_installed||before_installed){model_ready.store(true);return true;}
    return false;
}

using GetAndroidEvent=int32_t(*)(AInputQueue*,AInputEvent**);
using AndroidType=int32_t(*)(const AInputEvent*);
using AndroidAction=int32_t(*)(const AInputEvent*);
using AndroidPosition=float(*)(const AInputEvent*,size_t);
GetAndroidEvent original_get_android_event=nullptr;
AndroidType android_type=nullptr;
AndroidAction android_action=nullptr;
AndroidPosition android_x=nullptr,android_y=nullptr;
int32_t hook_get_android_event(AInputQueue* queue,AInputEvent** event){
    int32_t result=original_get_android_event?original_get_android_event(queue,event):-1;
    if(result<0||!event||!*event||!android_type||!android_action||!android_x||!android_y||
       android_type(*event)!=AINPUT_EVENT_TYPE_MOTION)return result;
    const int32_t action=android_action(*event)&AMOTION_EVENT_ACTION_MASK;
    if(action!=AMOTION_EVENT_ACTION_DOWN&&action!=AMOTION_EVENT_ACTION_MOVE&&
       action!=AMOTION_EVENT_ACTION_UP&&action!=AMOTION_EVENT_ACTION_CANCEL&&
       action!=AMOTION_EVENT_ACTION_POINTER_DOWN&&action!=AMOTION_EVENT_ACTION_POINTER_UP)return result;
    const float x=android_x(*event,0),y=android_y(*event,0);
    if(std::isfinite(x)&&std::isfinite(y)){native_touch_x.store(x);native_touch_y.store(y);}
    if(action==AMOTION_EVENT_ACTION_DOWN||action==AMOTION_EVENT_ACTION_MOVE||
       action==AMOTION_EVENT_ACTION_POINTER_DOWN)native_touch_down.store(true);
    if(action==AMOTION_EVENT_ACTION_UP||action==AMOTION_EVENT_ACTION_CANCEL||
       (action==AMOTION_EVENT_ACTION_POINTER_UP&&
        ((android_action(*event)&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT)==0))
        native_touch_down.store(false);
    native_touch_seen.store(true);
    return result;
}
void install_android_input_hook(){
    static bool installed=false;if(installed)return;
    static void* android=dlopen("libandroid.so",RTLD_NOW|RTLD_LOCAL);
    if(!android)return;
    auto target=dlsym(android,"AInputQueue_getEvent");
    android_type=reinterpret_cast<AndroidType>(dlsym(android,"AInputEvent_getType"));
    android_action=reinterpret_cast<AndroidAction>(dlsym(android,"AMotionEvent_getAction"));
    android_x=reinterpret_cast<AndroidPosition>(dlsym(android,"AMotionEvent_getX"));
    android_y=reinterpret_cast<AndroidPosition>(dlsym(android,"AMotionEvent_getY"));
    if(!target||!android_type||!android_action||!android_x||!android_y)return;
    int result=hook(target,reinterpret_cast<void*>(hook_get_android_event),
        reinterpret_cast<void**>(&original_get_android_event));
    if(result==0){installed=true;LOG("Android motion hook: AInputQueue_getEvent");}
}

std::mutex render_mutex;EGLContext context=EGL_NO_CONTEXT;
mango_ui::State menu_state;ImFont* esp_font=nullptr;
bool gl_ready=false;float scale=1;double last_render=0;
std::mutex config_queue_mutex;std::array<char,512> queued_config{};bool config_queued=false;
void config_path(char* path,size_t n){std::snprintf(path,n,"/data/user/%u/%s/files/mango_esp.cfg",unsigned(getuid())/100000,package);}
void load_config(){
    char path[256];config_path(path,sizeof(path));FILE* f=std::fopen(path,"r");if(!f)return;
    int enabled,box,distances,lines,skeleton,fill,corners;
    float range,r,green,blue;
    // The format tag prevents older settings from shifting into unrelated fields.
    if(std::fscanf(f,"MANGO_ESP_V1 %d %d %d %d %d %d %d %f %f %f %f",
            &enabled,&box,&distances,&lines,&skeleton,&fill,&corners,&range,&r,&green,&blue)==11){
        cfg.enabled=enabled!=0;cfg.box=box!=0;cfg.distances=distances!=0;
        cfg.lines=lines!=0;cfg.skeleton=skeleton!=0;cfg.fill=fill!=0;cfg.corners=corners!=0;
        if(std::isfinite(range))cfg.range=std::clamp(range,25.f,1000.f);
        if(std::isfinite(r)&&std::isfinite(green)&&std::isfinite(blue)){
            cfg.color[0]=std::clamp(r,0.f,1.f);
            cfg.color[1]=std::clamp(green,0.f,1.f);
            cfg.color[2]=std::clamp(blue,0.f,1.f);
        }
        int enabled_magic=0,ads=0,team=1,point=0;float fov=280;
        if(std::fscanf(f,"%d %f %d %d %d",&enabled_magic,&fov,&ads,&team,&point)==5){
            cfg.magic_bullet=enabled_magic!=0;cfg.magic_ads_only=ads!=0;
            cfg.magic_team_filter=team!=0;cfg.magic_point=point==1?1:0;
            if(std::isfinite(fov))cfg.magic_fov=std::clamp(fov,25.f,800.f);
            int recoil=0,spread=0,motion=0;
            if(std::fscanf(f,"%d %d %d",&recoil,&spread,&motion)==3){
                cfg.no_recoil=recoil!=0;cfg.no_spread=spread!=0;cfg.no_weapon_motion=motion!=0;
            }
        }
        int visibility=1,visible_only=0;
        if(std::fscanf(f,"%d %d",&visibility,&visible_only)==2){
            cfg.visibility_check=visibility!=0;cfg.visible_only=visible_only!=0;
        }
        int chams=0,limit=16;float alpha=.28f,chams_distance=80;
        if(std::fscanf(f,"%d %f %f %d",&chams,&alpha,&chams_distance,&limit)==4){
            cfg.chams=chams!=0;
            if(std::isfinite(alpha))cfg.chams_alpha=std::clamp(alpha,.05f,.65f);
            if(std::isfinite(chams_distance))cfg.chams_range=std::clamp(chams_distance,15.f,150.f);
            cfg.chams_limit=std::clamp(limit,4,32);
        }
        int third=0;float distance=2.5f;
        if(std::fscanf(f,"%d %f",&third,&distance)==2){
            cfg.third_person=third!=0;
            if(std::isfinite(distance))cfg.camera_distance=std::clamp(distance,.8f,5.f);
        }
        int model=0;if(std::fscanf(f,"%d",&model)==1)cfg.custom_model=model!=0;
    }
    std::fclose(f);
}
void save_config(){
    std::array<char,512> text{};
    std::snprintf(text.data(),text.size(),"MANGO_ESP_V1 %d %d %d %d %d %d %d %.1f %.3f %.3f %.3f %d %.0f %d %d %d %d %d %d %d %d %d %.3f %.1f %d %d %.2f %d\n",
        cfg.enabled,cfg.box,cfg.distances,cfg.lines,cfg.skeleton,cfg.fill,cfg.corners,
        cfg.range,cfg.color[0],cfg.color[1],cfg.color[2],cfg.magic_bullet,cfg.magic_fov,
        cfg.magic_ads_only,cfg.magic_team_filter,cfg.magic_point,
        cfg.no_recoil,cfg.no_spread,cfg.no_weapon_motion,cfg.visibility_check,cfg.visible_only,
        cfg.chams,cfg.chams_alpha,cfg.chams_range,cfg.chams_limit,cfg.third_person,cfg.camera_distance,cfg.custom_model);
    std::lock_guard<std::mutex> lock(config_queue_mutex);queued_config=text;config_queued=true;
}
void flush_config(){
    std::array<char,512> text{};
    {std::lock_guard<std::mutex> lock(config_queue_mutex);if(!config_queued)return;text=queued_config;config_queued=false;}
    char path[256];config_path(path,sizeof(path));FILE* file=std::fopen(path,"w");
    if(!file){std::lock_guard<std::mutex> lock(config_queue_mutex);config_queued=true;return;}
    const size_t size=std::strlen(text.data());const bool complete=std::fwrite(text.data(),1,size,file)==size;
    const bool closed=std::fclose(file)==0;
    if(!complete||!closed){std::lock_guard<std::mutex> lock(config_queue_mutex);config_queued=true;}
}
void init_gl(EGLContext ctx,int w,int h,EGLDisplay d,EGLSurface surface){
    if(gl_ready&&ctx==context)ImGui_ImplOpenGL3_Shutdown();
    if(ImGui::GetCurrentContext())ImGui::DestroyContext();
    ImGui::CreateContext();ImGuiIO& io=ImGui::GetIO();io.IniFilename=nullptr;
    scale=std::clamp(std::min(w/1100.f,h/720.f),.8f,2.4f);
    menu_state=mango_ui::State{};menu_state.scale=mango_ui::FitScale(w,h);
    mango_ui::ApplyStyle(menu_state.scale);mango_ui::LoadFont(io,menu_state.scale);
    static const ImWchar ranges[]={0x20,0xFF,0x11E,0x11F,0x130,0x131,0x15E,0x15F,0x400,0x45F,0};
    ImFontConfig font;font.FontDataOwnedByAtlas=false;
    esp_font=io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(kCFLocaleFont),kCFLocaleFontSize,19*scale,&font,ranges);
    gl_ready=ImGui_ImplOpenGL3_Init("#version 300 es");context=ctx;EGLint colorspace=0;
    ImGui_ImplOpenGL3_SetSrgbFramebuffer(eglQuerySurface(d,surface,0x309D,&colorspace)&&colorspace==0x3089);
    LOG("EGL initialized: %dx%d GLES=%s",w,h,glGetString(GL_VERSION));
}
ImVec2 pixel(V3 p,int w,int h){return{p.x*w,(1-p.y)*h};}
void centered(ImDrawList* draw,ImVec2 p,ImU32 color,const char* text){
    p.x-=ImGui::CalcTextSize(text).x/2;draw->AddText({p.x+1,p.y+1},IM_COL32(0,0,0,230),text);draw->AddText(p,color,text);
}
void draw_players(const Snapshot& s,int w,int h){
    if(!cfg.enabled||!s.camera||now()-s.stamp>.5)return;
    ImDrawList* draw=ImGui::GetBackgroundDrawList();
    for(size_t i=0;i<s.count;++i){
        const Player& p=s.players[i];if(p.meters>cfg.range)continue;ImVec2 feet=pixel(p.feet,w,h),head=pixel(p.head,w,h);
        if(cfg.visibility_check&&cfg.visible_only&&p.visibility!=mango_esp::Visibility::Visible)continue;
        ImVec4 rgba{cfg.color[0],cfg.color[1],cfg.color[2],1};
        if(cfg.visibility_check){
            if(p.visibility==mango_esp::Visibility::Occluded)rgba={1.f,.22f,.22f,1.f};
            else if(p.visibility==mango_esp::Visibility::Unknown)rgba={.65f,.65f,.68f,1.f};
        }
        ImU32 color=ImGui::ColorConvertFloat4ToU32(rgba);
        // min/max keeps a box on prone and ragdolled players, where the head
        // point can project below the feet point.
        const float top=std::min(feet.y,head.y),bottom=std::max(feet.y,head.y);
        float height=bottom-top;if(height<4||height>h*3)continue;
        float width=height*.46f,center=(feet.x+head.x)*.5f;ImVec2 lo(center-width*.5f,top),hi(center+width*.5f,bottom);
        if(hi.x<0||lo.x>w||hi.y<0||lo.y>h)continue;
        if(cfg.box){
            if(cfg.fill)draw->AddRectFilled(lo,hi,ImGui::ColorConvertFloat4ToU32({rgba.x,rgba.y,rgba.z,.1f}));
            if(cfg.corners){
                float x=width*.28f,y=height*.2f;
                ImVec2 segments[][2]={{{lo.x,lo.y+y},lo},{lo,{lo.x+x,lo.y}},{{hi.x-x,lo.y},{hi.x,lo.y}},{{hi.x,lo.y},{hi.x,lo.y+y}},
                    {{lo.x,hi.y-y},{lo.x,hi.y}},{{lo.x,hi.y},{lo.x+x,hi.y}},{{hi.x-x,hi.y},hi},{hi,{hi.x,hi.y-y}}};
                for(auto& line:segments){draw->AddLine(line[0],line[1],IM_COL32(0,0,0,220),4);draw->AddLine(line[0],line[1],color,1.6f);}
            }else{draw->AddRect(lo,hi,IM_COL32(0,0,0,230),0,0,4);draw->AddRect(lo,hi,color,0,0,1.6f);}
        }
        if(cfg.skeleton)for(const auto& link:links)if((p.valid&(1u<<link[0]))&&(p.valid&(1u<<link[1])))draw->AddLine(pixel(p.bones[link[0]],w,h),pixel(p.bones[link[1]],w,h),color,1.6f);
        if(cfg.distances){
            char label[32];std::snprintf(label,sizeof(label),"%.0f m",p.meters);
            centered(draw,{center,hi.y+3},color,label);
        }
        if(cfg.lines)draw->AddLine({w*.5f,0.f},{center,top},color,1.2f);
    }
}
void sync_config(){
    want_esp.store(cfg.enabled);want_skeleton.store(cfg.enabled&&cfg.skeleton);esp_range.store(cfg.range);
    want_visibility.store(cfg.visibility_check&&(cfg.enabled||cfg.chams));
    want_chams.store(cfg.chams);
    want_third_person.store(cfg.third_person);want_custom_model.store(cfg.custom_model);camera_distance.store(cfg.camera_distance);
    chams_alpha.store(cfg.chams_alpha);chams_range.store(cfg.chams_range);chams_limit.store(cfg.chams_limit);
    want_magic_bullet.store(cfg.magic_bullet);magic_fov.store(cfg.magic_fov);
    want_magic_ads_only.store(cfg.magic_ads_only);want_magic_team_filter.store(cfg.magic_team_filter);
    magic_point.store(cfg.magic_point);
    want_no_recoil.store(cfg.no_recoil);want_no_spread.store(cfg.no_spread);want_no_weapon_motion.store(cfg.no_weapon_motion);
}
void draw_menu(int w,int h,int players=-1){
    const auto result=mango_ui::Draw(cfg,menu_state,w,h,players,chams_status.load(),chams_renderers.load(),camera_status.load(),camera_meshes.load(),shader_stage.load(),chams_cpu_ms.load(),camera_cpu_ms.load(),custom_model_status.load());
    menu_is_open.store(result.open);menu_popup_open.store(result.popup);
    menu_x0.store(result.min.x);menu_y0.store(result.min.y);
    menu_x1.store(result.max.x);menu_y1.store(result.max.y);
    bar_x0.store(result.bar_min.x);bar_y0.store(result.bar_min.y);
    bar_x1.store(result.bar_max.x);bar_y1.store(result.bar_max.y);
    sync_config();if(result.changed)save_config();
}
void render(EGLDisplay d,EGLSurface surface){
    EGLContext ctx=eglGetCurrentContext();if(ctx==EGL_NO_CONTEXT)return;EGLint w=0,h=0;
    if(!eglQuerySurface(d,surface,EGL_WIDTH,&w)||!eglQuerySurface(d,surface,EGL_HEIGHT,&h)||w<180||h<180)return;
    const char* version=reinterpret_cast<const char*>(glGetString(GL_VERSION));if(!version||!std::strstr(version,"OpenGL ES 3"))return;
    if(!gl_ready||ctx!=context)init_gl(ctx,w,h,d,surface);if(!gl_ready)return;
    egl_width.store(w);egl_height.store(h);
    static Snapshot frame_snapshot;   // render thread only
    {std::lock_guard<std::mutex> lock(snapshot_mutex);frame_snapshot.assign(published);}
    const Snapshot& s=frame_snapshot;
    Pointer pointer;{std::lock_guard<std::mutex> lock(pointer_mutex);pointer=published_pointer;}
    if(api.raw&&native_touch_seen.load()&&input_backend.load()==3&&
       menu_hit(native_touch_x.load(),native_touch_y.load())){
        pointer.position={native_touch_x.load()/float(w),1.f-native_touch_y.load()/float(h)};
        pointer.down=native_touch_down.load();pointer.stamp=now();
    }
    ImGuiIO& io=ImGui::GetIO();double time=now();io.DisplaySize={float(w),float(h)};
    io.DeltaTime=last_render>0?std::clamp(float(time-last_render),.001f,.25f):1.f/60;last_render=time;
    const bool fresh=time-pointer.stamp<.5;
    pointer_down_state.store(fresh&&pointer.down);
    if(fresh)io.AddMousePosEvent(pointer.position.x*w,(1-pointer.position.y)*h);
    io.AddMouseButtonEvent(0,fresh&&pointer.down);
    ImGui_ImplOpenGL3_NewFrame();ImGui::NewFrame();
    if(esp_font)ImGui::PushFont(esp_font);
    draw_players(s,w,h);
    if(esp_font)ImGui::PopFont();
    const int players=s.stamp>0&&time-s.stamp<.5?int(s.total)-(s.local&&s.total?1:0):-1;
    draw_menu(w,h,players);
    if(cfg.magic_bullet&&!menu_is_open.load()){
        ImGui::GetBackgroundDrawList()->AddCircle({w*.5f,h*.5f},cfg.magic_fov,
            IM_COL32(189,123,255,170),64,1.5f);
    }
    ImGui::Render();
    GLint draw=0,read=0;GLboolean mask[4];glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glColorMask(mask[0],mask[1],mask[2],mask[3]);
    if(!frames.load())first_egl_frame_at.store(now());
    if(frames.fetch_add(1)==0)LOG("first frame %dx%d",w,h);
}
std::mutex render_gate;thread_local unsigned depth=0;
using Swap=EGLBoolean(*)(EGLDisplay,EGLSurface);using Damage=EGLBoolean(*)(EGLDisplay,EGLSurface,EGLint*,EGLint);
Swap orig_swap=nullptr;Damage orig_khr=nullptr,orig_ext=nullptr;
struct SwapScope{SwapScope(){++depth;}~SwapScope(){--depth;}};
void on_swap(EGLDisplay d,EGLSurface s){
    if(depth>1)return;char name[32]{};prctl(PR_GET_NAME,name,0,0,0);if(std::strstr(name,"RenderThread"))return;
    std::unique_lock<std::mutex> lock(render_gate,std::try_to_lock);if(lock.owns_lock()){
        const double start=now();render(d,s);
        const unsigned cost=unsigned(std::max(0.0,(now()-start)*1000000));
        render_total_us.fetch_add(cost);render_samples.fetch_add(1);
        unsigned old=max_render_us.load();while(old<cost&&!max_render_us.compare_exchange_weak(old,cost)){}
    }
}
EGLBoolean hook_swap(EGLDisplay d,EGLSurface s){SwapScope guard;on_swap(d,s);return orig_swap?orig_swap(d,s):EGL_FALSE;}
EGLBoolean hook_khr(EGLDisplay d,EGLSurface s,EGLint* r,EGLint n){SwapScope guard;on_swap(d,s);return orig_khr?orig_khr(d,s,r,n):EGL_FALSE;}
EGLBoolean hook_ext(EGLDisplay d,EGLSurface s,EGLint* r,EGLint n){SwapScope guard;on_swap(d,s);return orig_ext?orig_ext(d,s,r,n):EGL_FALSE;}
void install_render_hooks(){
    static std::vector<void*> installed;
    struct Entry{const char* name;void* replacement;void** original;};
    Entry entries[]={{"eglSwapBuffers",reinterpret_cast<void*>(hook_swap),reinterpret_cast<void**>(&orig_swap)},
        {"eglSwapBuffersWithDamageKHR",reinterpret_cast<void*>(hook_khr),reinterpret_cast<void**>(&orig_khr)},
        {"eglSwapBuffersWithDamageEXT",reinterpret_cast<void*>(hook_ext),reinterpret_cast<void**>(&orig_ext)}};
    for(auto& entry:entries){
        void* target=dlsym(RTLD_DEFAULT,entry.name);if(!target)target=reinterpret_cast<void*>(eglGetProcAddress(entry.name));
        if(!target||std::find(installed.begin(),installed.end(),target)!=installed.end())continue;
        int r=hook(target,entry.replacement,entry.original);if(r==0){installed.push_back(target);LOG("Render hook: %s",entry.name);}
    }
}
void boot(){
    char cmd[256]{};FILE* f=std::fopen("/proc/self/cmdline","r");if(f){std::fread(cmd,1,sizeof(cmd)-1,f);std::fclose(f);}
    if(std::strcmp(cmd,package)){LOG("Wrong process: %s",cmd);return;}
    LOG("boot: pid=%d uid=%u revision=%s",int(getpid()),unsigned(getuid()),revision_marker);
    mango_log::install_crash_handler();
    load_config();sync_config();write_shader_file();write_avatar_file();
    raw_observations.reserve(max_players);
    LOG("MANGOcheat ESP %s build %s %s package=%s",revision_marker,__DATE__,__TIME__,package);void* attached=nullptr;
    for(unsigned attempts=0;;++attempts){
        if(!shader_file_ready.load()&&attempts%10==0)write_shader_file();
        flush_config();write_avatar_file();
        install_render_hooks();
        const double first_frame=first_egl_frame_at.load();
        const bool graphics_stable=first_frame>0&&now()-first_frame>=4.0;
        if(graphics_stable)install_android_input_hook();
        if((!game_ready.load()||!raw_hooks_complete.load())&&api.load()&&api.raw&&graphics_stable){
            if(install_raw_hooks()){
                runtime_stage.store(6);runtime_state.store(1);game_ready.store(true);
                LOG("Oxide raw player and touch capture installed");
            }else{runtime_stage.store(5);runtime_state.store(-1);}
        }
        static bool wait_logged=false;
        if(api.raw&&!graphics_stable&&!wait_logged){
            wait_logged=true;LOG("Raw callbacks deferred until EGL has rendered and Unity startup settles");
        }
        if(!game_ready.load()&&api.loaded&&!api.raw&&api.domain_get()){
            if(!attached){attached=api.thread_attach(api.domain_get());if(!attached)runtime_stage.store(3);}
            if(attached&&api.refresh()){
                // Input/main-thread delivery must not depend on every ESP
                // binding being available or a player having spawned already.
                model_ready.store(resolve_model());
                const Method* late=api.method("Oxide","PlayerManager","LateUpdate",0,0x6F270CC,false,1);
                const Method* update=api.method("Oxide","PlayerManager","Update",0,0x6F01500,false,1);
                const Method* before=api.method("UnityEngine","Application","InvokeOnBeforeRender",0,0xD431BF8,true,1);
                const Method* canvas=api.method("UnityEngine","Canvas","SendWillRenderCanvases",0,0xD844580,true,1);
                bool installed=false;
                struct Candidate{const char* name;const Method* method;void* replacement;void** original;};
                Candidate candidates[]={
                    {"Canvas.SendWillRenderCanvases",canvas,reinterpret_cast<void*>(hook_canvas),reinterpret_cast<void**>(&original_canvas)},
                    {"PlayerManager.Update",update,reinterpret_cast<void*>(hook_update),reinterpret_cast<void**>(&original_update)},
                    {"Application.InvokeOnBeforeRender",before,reinterpret_cast<void*>(hook_before),reinterpret_cast<void**>(&original_before)},
                    {"PlayerManager.LateUpdate",late,reinterpret_cast<void*>(hook_late),reinterpret_cast<void**>(&original_late)}};
                for(const auto& c:candidates)if(c.method){
                    const int result=hook(reinterpret_cast<void*>(native(c.method)),c.replacement,c.original);
                    LOG("Main callback %s status=%d",c.name,result);
                    if(result==0)installed=true;
                }
                if(installed){runtime_stage.store(6);runtime_state.store(1);game_ready.store(true);LOG("Unity main-thread capture installed");}
                else {runtime_stage.store(5);runtime_state.store(-1);}
            }else if(attempts>3)runtime_state.store(-1);
        }
        if(game_ready.load()&&(attempts%15)==0){
            Snapshot s;{std::lock_guard<std::mutex> lock(snapshot_mutex);s=published;}
            Pointer pointer;{std::lock_guard<std::mutex> lock(pointer_mutex);pointer=published_pointer;}
            LOG("Flow: capture=%u draw=%u input=%d down=%d players=%zu/%zu camera=%d local=%d model=%d age=%.2f legacy=%d sys=%d/%d touch=%.0f,%.0f size=%dx%d cost=%u/%u us",
                capture_frames.load(),frames.load(),input_backend.load(),pointer.down,s.count,s.total,s.camera,s.local,
                model_ready.load(),s.stamp?now()-s.stamp:-1.0,input_legacy_count.load(),input_devices.load(),input_touches.load(),input_x.load(),input_y.load(),input_screen_width.load(),input_screen_height.load(),max_observe_us.exchange(0),max_publish_us.exchange(0));
            const unsigned count=capture_samples.exchange(0),draws=render_samples.exchange(0);
            const uint64_t capture_time=capture_total_us.exchange(0),render_time=render_total_us.exchange(0);
            LOG("Perf: capture_avg=%llu us render_avg=%llu us render_max=%u us samples=%u/%u chams=%d shader=%d meshes=%u material_creations=%u restore_errors=%u camera=%d body_meshes=%u",
                static_cast<unsigned long long>(count?capture_time/count:0),static_cast<unsigned long long>(draws?render_time/draws:0),max_render_us.exchange(0),count,draws,
                chams_status.load(),chams_shader.load(),chams_renderers.load(),chams_creations.load(),chams_restore_errors.load(),camera_status.load(),camera_meshes.load());
            LOG("Visibility: rays=%u clear=%u target=%u world=%u own=%u scene_fail=%u binding_fail=%u invalid_hit=%u collider_fail=%u budget=%u",
                raw_engine.metrics[0].exchange(0),raw_engine.metrics[1].exchange(0),raw_engine.metrics[2].exchange(0),raw_engine.metrics[3].exchange(0),
                raw_engine.metrics[4].exchange(0),raw_engine.metrics[5].exchange(0),raw_engine.metrics[6].exchange(0),raw_engine.metrics[7].exchange(0),raw_engine.metrics[8].exchange(0),raw_engine.metrics[9].exchange(0));
        }
        if(attempts==10&&!frames.load())LOG("No GLES frames; Vulkan unsupported. Launch with Unity -force-gles30.");
        std::this_thread::sleep_for(std::chrono::seconds(game_ready.load()?1:2));
    }
}
} // namespace mango
// Ordered C++ initialization: this object is declared after all runtime state
// in this translation unit, so dlopen cannot start the worker before its vectors
// and mutexes have been initialized.
namespace {
struct MangoBootstrap {
    MangoBootstrap() noexcept {
        try {
            std::thread([]{
                try {mango::boot();}
                catch(const std::exception& error){LOG("Worker exception: %s",error.what());}
                catch(...){LOG("Worker exception: unknown");}
            }).detach();
        }catch(const std::exception& error){LOG("Worker startup failed: %s",error.what());}
        catch(...){LOG("Worker startup failed");}
    }
}mango_bootstrap;
}
