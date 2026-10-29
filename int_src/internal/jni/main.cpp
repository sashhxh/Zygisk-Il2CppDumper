// MANGOcheat / Oxide 1.16.11978 ARM64. Unity APIs stay on Unity main thread.
#include <android/log.h>
#include <android/input.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
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
#include <unordered_map>
#include "dobby.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "vulkan_hooks.h"
#include "ui_coordinates.h"
#include "cf_locale_font.h"
#include "menu.h"
#include "reference_esp.h"
#include "settings.h"
#include "shot_target.h"
#include "resource_esp.h"
#include "aim_prediction.h"
#include "automation_input.h"
#include "proximity_farm.h"
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
#define LOG(...) __android_log_print(ANDROID_LOG_INFO,"MANGOcheat",__VA_ARGS__)
namespace mango {
constexpr const char* package="com.catsbit.oxidesurvivalisland";
// Identifies the loaded library in the startup log.
const char revision_marker[]="MANGOCHEAT_REV=r42-reference-ui";
constexpr size_t max_players=256, bone_count=17;
struct V2{float x=0,y=0;};
struct V3{float x=0,y=0,z=0;};
struct Ray{V3 origin,direction;};
static_assert(sizeof(Ray)==24,"UnityEngine.Ray ARM64 layout");
struct RaycastHit {V3 point,normal;uint32_t face=0;float distance=0;V2 uv;int32_t collider=0;};
static_assert(sizeof(RaycastHit)==44&&offsetof(RaycastHit,distance)==0x1c&&offsetof(RaycastHit,collider)==0x28,
              "Unity 6000.3 RaycastHit ABI verified at D57E24C / D57E1A0");
struct Method;using Obj=void;
double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool finite(V3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
float distance(V3 a,V3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
using Config=mango_config::Settings;Config cfg;
std::atomic<bool> want_esp{true},want_skeleton{false},game_ready{false};
std::atomic<float> esp_range{300};
std::atomic<bool> want_visibility{true};
std::atomic<bool> want_chams{false};
std::atomic<float> chams_alpha{.28f},chams_range{80};
std::atomic<int> chams_limit{16},chams_status{0};
std::mutex chams_appearance_mutex;mango_chams::Appearance published_chams_appearance;
std::atomic<bool> want_custom_model{false};std::atomic<int> custom_model_status{0};
std::atomic<bool> want_third_person{false};std::atomic<float> camera_distance{2.5f};
std::atomic<int> camera_status{0};
std::atomic<int> shader_stage{0};
std::atomic<uintptr_t> third_camera_transform{0};std::atomic<pid_t> unity_camera_thread{0};
std::atomic<bool> want_no_recoil{false},want_no_spread{false},want_no_weapon_motion{false};
std::atomic<bool> want_magic_bullet{false},want_magic_ads_only{false},want_magic_team_filter{true};
std::atomic<bool> want_auto_shoot{false};
std::atomic<double> auto_hold_until{0},automation_input_at{0},last_projectile_at{0};
std::atomic<int> automation_mode{0};
std::mutex extra_settings_mutex;mango_extra::Settings published_extra;
bool aim_requested(){return want_magic_bullet.load()||want_auto_shoot.load();}
mango_extra::Settings extra_config(){std::lock_guard<std::mutex> lock(extra_settings_mutex);return published_extra;}
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
std::atomic<unsigned> frames{0};
std::atomic<int> render_width{0},render_height{0};
std::atomic<double> first_graphics_frame_at{0};
std::atomic<int> input_backend{0}; // 1 legacy, 2 new Input System
std::atomic<bool> native_touch_seen{false},native_touch_down{false};
std::atomic<bool> pointer_down_state{false};
std::atomic<float> native_touch_x{0},native_touch_y{0};
struct Player{
    V3 feet,head;std::array<V3,bone_count> bones{};uint32_t valid=0;
    float meters=0;
    float health_fraction=-1;
    std::array<char,257> name{};
    mango_esp::Visibility visibility=mango_esp::Visibility::Unknown;
};
struct Snapshot{
    struct Resource {V3 point;float meters=0;int kind=0;};
    std::array<Resource,128> resources{};size_t resource_count=0;
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
        resource_count=std::min(other.resource_count,resources.size());for(size_t i=0;i<resource_count;++i)resources[i]=other.resources[i];
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

// One verified offset path; the old metadata/heap-enumeration fallback was
// unreachable after selecting this exact game image.
struct Api {
    bool loaded=false;uintptr_t base=0;
    mango_runtime::Resolver resolver;
    bool load(){
        if(loaded)return true;
        if(!resolver.select_named_image("libil2cpp.so",mango_offsets::build_id,sizeof(mango_offsets::build_id),
                mango_offsets::anchors,sizeof(mango_offsets::anchors)/sizeof(mango_offsets::anchors[0])))return false;
        base=resolver.base();loaded=true;
        LOG("Verified Oxide 1.16.11978 ARM64 runtime ready");return true;
    }
}api;
constexpr int bone_ids[bone_count]={0,7,8,9,10,11,13,15,12,14,16,1,3,5,2,4,6};
constexpr int links[][2]={{0,1},{1,2},{2,3},{3,4},{2,5},{5,6},{6,7},{2,8},{8,9},{9,10},{0,11},{11,12},{12,13},{0,14},{14,15},{15,16}};
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
std::atomic<int> extra_write_fd{-1};
bool extra_write(uintptr_t at,const void* data,size_t size){
    if(!at||size>64||size>UINTPTR_MAX-at)return false;
    int fd=extra_write_fd.load();if(fd<0){fd=open("/proc/self/mem",O_RDWR|O_CLOEXEC);if(fd<0)return false;int expected=-1;if(!extra_write_fd.compare_exchange_strong(expected,fd)){close(fd);fd=expected;}}
    return pwrite(fd,data,size,off_t(at))==ssize_t(size);
}
const mango_extra::Memory extra_memory{raw_read,extra_write};
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
    mango_prediction::Motion motion;
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
mango_avatar::Storage avatar_storage;
extern "C" const unsigned char cfesp_avatar_bundle[],cfesp_avatar_bundle_end[];
void write_avatar_file(){
    const size_t size=reinterpret_cast<uintptr_t>(cfesp_avatar_bundle_end)-reinterpret_cast<uintptr_t>(cfesp_avatar_bundle);
    avatar_storage.flush(cfesp_avatar_bundle,size);
}
void report_avatar(int stage){
    custom_model_status.store(stage);
    static int previous=-999,old_file=-999;
    const int file=avatar_storage.state.load();
    if(stage!=previous||file!=old_file){
        previous=stage;old_file=file;
        if(stage<0)LOG("Avatar: stage=%d %s asset=%d file=%d human=%u target=%u meshes=%u active=%d",stage,mango_avatar::status_text(stage),avatar_asset.status,file,avatar_controller.human_count,avatar_controller.target_count,avatar_controller.mesh_count,avatar_controller.visible);
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
    }catch(...){LOG("Avatar: cache path unavailable");}
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
    prepare_avatar_file(time);const int file=avatar_storage.state.load(std::memory_order_acquire);
    if(file!=3){fail(file<0?-3:2);return;}
    avatar_asset.retry(time,rising);
    try{
        if(avatar_asset.status>=0&&avatar_asset.status!=3&&avatar_asset.bind(safe_graphics_icall)){
            Obj* object=chams_api.decode(chams_api.game_object(chams_api.native(view.animator)));
            Obj* type=raw_type_named(object,"GameObject")?raw_fn<Obj*(*)(Obj*,const Method*)>(0xDB15300)(object,nullptr):nullptr;
            if(!raw_type_named(type,"RuntimeType")){fail(-4);return;}
            const auto& path=avatar_storage.path();mango_chams::Span span{path.data(),int32_t(path.size())};
            avatar_asset.update(chams_api,span,type,time);
        }
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
bool unity_pointer(V2 position,int source_width,int source_height,Pointer& s){
    const int w=render_width.load(),h=render_height.load();mango_input::Point point;
    if(!mango_input::to_ui(position.x,position.y,source_width,source_height,w,h,
        mango_input::Origin::BottomLeft,point)||!menu_hit(point.x,point.y))return false;
    s.position={point.x/w,1.f-point.y/h};s.down=true;return true;
}
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
    if(!unity_pointer(position,w,h,s))return false;
    input_backend.store(2);
    return true;
}
void raw_input(Pointer& s){
    if(!frames.load())return;
    int w=render_width.load(),h=render_height.load();
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

    using Current=Obj*(*)(const Method*);
    Obj* touchscreen=raw_fn<Current>(0xD079374)(nullptr);

    if(touchscreen){
        // Touchscreen.touches is a ReadOnlyArray: array pointer, start, length.
        Obj* array=raw_field<Obj*>(touchscreen,0x1C0);
        const int32_t start=raw_field<int32_t>(touchscreen,0x1C8);
        const int32_t count=raw_field<int32_t>(touchscreen,0x1CC);
        const size_t length=raw_field<size_t>(array,0x18);

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

    if(count>0&&count<=16){
        for(int i=0;i<count;++i){
            LegacyTouch touch=raw_fn<GetTouch>(0xD54E1C4)(i,nullptr);
            if(touch.phase<0||touch.phase>2||
               !std::isfinite(touch.position.x)||!std::isfinite(touch.position.y))continue;
            if(!unity_pointer(touch.position,touch_w,touch_h,s))continue;
            input_backend.store(1);
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
            if(finite(p)&&unity_pointer({p.x,p.y},touch_w,touch_h,s)){
                input_backend.store(4);
                return;
            }
        }
    }
    if(native_touch_seen.load()&&menu_hit(native_touch_x.load(),native_touch_y.load())){
        s.position={native_touch_x.load()/w,1.f-native_touch_y.load()/h};
        s.down=native_touch_down.load();input_backend.store(3);

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
                observation.human=it->human;observation.motion=it->motion;
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
    if(aim_requested()&&(!observation.team[0]||sample_time-observation.team_at>5)){
        raw_team_tag(raw_field<Obj*>(self,0x2B0),observation.team,sizeof(observation.team));
        observation.team_at=sample_time;
    }
    if(!mine&&(want_esp.load()||aim_requested()||want_chams.load())){
        Player& player=observation.world;
        const float collection_range=std::max((want_esp.load()||aim_requested())?esp_range.load():0.f,want_chams.load()?chams_range.load():0.f);
        if(raw_position(self,player.feet)&&(!origin||distance(*origin,player.feet)<=collection_range+3.f)){
            V3 probe{};V3 probe_head=player.feet;probe_head.y+=1.4f;
            const bool onscreen=!projection||!projection->valid||
                (projection->project(probe_head,probe)&&probe.x>=-.1f&&probe.x<=1.1f&&probe.y>=-.2f&&probe.y<=1.2f);
            if(!onscreen&&!aim_requested()){
                observation.visible=false;
            }else{
            if(native_animator){
                V3 visual_root{};
                if(raw_position(animator,visual_root)&&distance(visual_root,player.feet)<3.f)player.feet=visual_root;
            }
            observation.motion.update({player.feet.x,player.feet.y,player.feet.z},sample_time);
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
}
bool player_alive(Obj* player){
    Obj* health=raw_field<Obj*>(raw_field<Obj*>(player,0x78),0x98);float value=raw_field<float>(health,0x20);
    return health&&std::isfinite(value)&&value>.01f;
}
void capture_reference_labels(Obj* object,Player& player){
    player.name[0]=0;player.health_fraction=-1;
    Obj* health=raw_field<Obj*>(raw_field<Obj*>(object,0x78),0x98);
    float value=0;
    if(health&&raw_read(reinterpret_cast<uintptr_t>(health)+0x20,&value,sizeof(value))&&
       std::isfinite(value)&&value>=0&&value<=1000)
        player.health_fraction=std::clamp(value/100.f,0.f,1.f);
    for(size_t offset:{size_t(0xB8),size_t(0xA8),size_t(0xB0)}){
        Obj* candidate=raw_field<Obj*>(object,offset);
        if(!raw_type_named(candidate,"String"))continue;
        const int32_t length=raw_field<int32_t>(candidate,0x10);
        if(length<=0||length>64)continue;
        uint16_t units[64]{};
        if(raw_read(reinterpret_cast<uintptr_t>(candidate)+0x14,units,size_t(length)*2)&&
           mango_reference::utf16_name(units,size_t(length),player.name.data(),player.name.size()))break;
    }
}
Obj* held_weapon(Obj* player){return raw_field<Obj*>(raw_field<Obj*>(player,0x90),0x50);}
mango_extra::MotionApi motion_api;
mango_extra::WeaponOffset weapon_offset;
using MotionUpdate=void(*)(Obj*,const Method*);MotionUpdate original_motion_update=nullptr;
void hook_motion_update(Obj* self,const Method* method){
    auto settings=extra_config();
    if(!settings.weapon){weapon_offset.restore(motion_api);if(original_motion_update)original_motion_update(self,method);return;}
    Obj* weapon=raw_field<Obj*>(self,0xD0);Obj* local=static_cast<Obj*>(local_player.load());
    const bool mine=local&&weapon&&raw_field<Obj*>(weapon,0xC0)==local;
    if(mine)weapon_offset.restore(motion_api);
    if(original_motion_update)original_motion_update(self,method);
    if(!mine)return;
    if(want_third_person.load()||!player_alive(local))return;
    Obj* aim=raw_field<Obj*>(raw_field<Obj*>(local,0x78),0x2B0);
    if(!settings.weapon_ads&&raw_field<uint8_t>(aim,0x10))return;
    if(!motion_api.position){
        motion_api.memory=extra_memory;
        motion_api.position=reinterpret_cast<decltype(motion_api.position)>(safe_graphics_icall("UnityEngine.Transform::get_localPosition_Injected(System.IntPtr,UnityEngine.Vector3&)"));
        motion_api.set_position=reinterpret_cast<decltype(motion_api.set_position)>(safe_graphics_icall("UnityEngine.Transform::set_localPosition_Injected(System.IntPtr,UnityEngine.Vector3&)"));
        motion_api.rotation=reinterpret_cast<decltype(motion_api.rotation)>(safe_graphics_icall("UnityEngine.Transform::get_localRotation_Injected(System.IntPtr,UnityEngine.Quaternion&)"));
        motion_api.set_rotation=reinterpret_cast<decltype(motion_api.set_rotation)>(safe_graphics_icall("UnityEngine.Transform::set_localRotation_Injected(System.IntPtr,UnityEngine.Quaternion&)"));
    }
    weapon_offset.apply(motion_api,raw_field<uintptr_t>(self,0x20),settings);
}
struct ResourceNode {
    mango_extra::Ref ref,root,transform;V3 world;int kind=0;unsigned purpose=0;bool grouped=false;double seen=0,position_at=0;mango_farm::ClientHealth health;
};
struct StoneGroup {mango_extra::Ref ref;mango_resources::StoneCursor cursor;double seen=0;};
struct CachedStone {mango_extra::Ref root,component;};
std::unordered_map<uintptr_t,ResourceNode> resource_nodes;
std::vector<StoneGroup> stone_groups;std::unordered_map<uintptr_t,CachedStone> stone_components;
size_t stone_group_cursor=0;
mango_extra::ResourceCursor resource_cursor;mango_resources::Types resource_types;double resource_next=0;
std::atomic<int> resource_status{0},farm_status{0},auto_status{0};
struct FarmFocus {mango_extra::Ref ref,root,transform;Obj* weapon=nullptr;V3 point{};bool grouped=false;double time=0;};
FarmFocus farm_focus;std::mutex farm_focus_mutex;
mango_farm::Cadence farm_cadence;
mango_farm::ToolSession farm_tool_session;
std::atomic<unsigned> farm_requests{0},farm_weakspot_hits{0},farm_starter_hits{0};
mango_input::Controller automation_input;
bool resource_root_active(const mango_extra::Ref& root){return root.valid(extra_memory)&&raw_fn<bool(*)(Obj*,const Method*)>(0xD4BBA44)(reinterpret_cast<Obj*>(root.object),nullptr);}
bool resource_alive(const mango_extra::Ref& ref,const mango_extra::Ref& root,bool grouped){
    if(!ref.valid(extra_memory)||!resource_root_active(root))return false;
    const float health=raw_field<float>(reinterpret_cast<Obj*>(ref.object),0x78);
    const auto it=resource_nodes.find(ref.object);
    if(it!=resource_nodes.end()&&it->second.ref.native==ref.native&&it->second.ref.klass==ref.klass)
        return it->second.health.available(health,grouped,true);
    mango_farm::ClientHealth initial;
    return initial.available(health,grouped,true);
}
void clear_farm(){
    {std::lock_guard<std::mutex> lock(farm_focus_mutex);farm_focus={};}
}
Obj* stone_component(Obj* root){
    const uintptr_t address=reinterpret_cast<uintptr_t>(root);auto it=stone_components.find(address);
    if(it!=stone_components.end()&&it->second.root.valid(extra_memory)&&it->second.component.valid(extra_memory))return reinterpret_cast<Obj*>(it->second.component.object);
    const auto ref=mango_extra::Ref::capture(extra_memory,address);if(!ref.valid(extra_memory))return nullptr;
    Obj* transform=raw_fn<Obj*(*)(Obj*,const Method*)>(0xD4BB438)(root,nullptr);
    if(!raw_field<Obj*>(transform,0x10)||raw_fn<int(*)(Obj*,const Method*)>(0xD4D343C)(transform,nullptr)<1)return nullptr;
    Obj* first=raw_fn<Obj*(*)(Obj*,int,const Method*)>(0xD4D42D4)(transform,0,nullptr);
    const Method* method=raw_field<const Method*>(raw_field<Obj*>(reinterpret_cast<Obj*>(api.base+mango_offsets::stone_component_method_got),0),0);
    if(!first||!method)return nullptr;
    // The same typed Component.GetComponent<MineableStone> used by kaV.Awake.
    Obj* component=raw_fn<Obj*(*)(Obj*,const Method*)>(0x8A29324)(first,method);
    const auto object=mango_extra::Ref::capture(extra_memory,reinterpret_cast<uintptr_t>(component));
    if(!object.valid(extra_memory)||!(resource_types.flags(extra_memory,object.object)&mango_resources::Stone))return nullptr;
    if(stone_components.size()<16384)stone_components[address]={ref,object};return component;
}
void resource_capture(Obj* local,Obj* camera,V3 origin,Snapshot& snapshot,double time){
    snapshot.resource_count=0;auto settings=extra_config();
    if((!settings.resource_esp&&!settings.farm)||!local||!camera||!player_alive(local)){
        resource_nodes.clear();stone_groups.clear();stone_components.clear();stone_group_cursor=0;resource_cursor.reset();resource_next=0;clear_farm();resource_status.store(0);return;
    }
    if(time>=resource_next){
        resource_next=time+.05;
        auto collect=[&](uintptr_t object,Obj* known_root,bool grouped){
            const unsigned flags=resource_types.flags(extra_memory,object);if(!(flags&mango_resources::Mineable))return;
            const int kind=mango_resources::kind(extra_memory,object,flags);if(kind<0){resource_nodes.erase(object);return;}
            auto ref=mango_extra::Ref::capture(extra_memory,object);if(!ref.valid(extra_memory)){resource_nodes.erase(object);return;}
            auto it=resource_nodes.find(object);Obj* component=reinterpret_cast<Obj*>(object);
            if(it==resource_nodes.end()||it->second.ref.native!=ref.native||it->second.ref.klass!=ref.klass){
                if(resource_nodes.size()>=8192)return;
                Obj* root=known_root?known_root:raw_fn<Obj*(*)(Obj*,const Method*)>(0xD4B6594)(component,nullptr);
                const auto root_ref=mango_extra::Ref::capture(extra_memory,reinterpret_cast<uintptr_t>(root));if(!resource_alive(ref,root_ref,grouped))return;
                V3 position;if(!raw_position(component,position))return;
                if(distance(position,origin)>std::max(settings.resource_esp?settings.resource_range:0.f,settings.farm?settings.farm_range:0.f)+50)return;
                Obj* transform=raw_fn<Obj*(*)(Obj*,const Method*)>(0xD4BB438)(root,nullptr);
                unsigned purpose=raw_field<unsigned>(component,0x70);if(!purpose)purpose=kind==0?1:2;
                resource_nodes[object]={ref,root_ref,mango_extra::Ref::capture(extra_memory,reinterpret_cast<uintptr_t>(transform)),position,kind,purpose,grouped,time,time};
            }else{
                if(!resource_alive(it->second.ref,it->second.root,it->second.grouped)){resource_nodes.erase(it);return;}
                it->second.seen=time;it->second.kind=kind;
                if(time-it->second.position_at>2){V3 position;if(raw_position(component,position)){it->second.world=position;it->second.position_at=time;}}
            }
        };
        const uintptr_t client_fields=reinterpret_cast<uintptr_t>(raw_static_fields(mango_offsets::network_client_type_got));
        const bool valid=mango_resources::visit_client(extra_memory,resource_cursor,client_fields,[&](uintptr_t object,uintptr_t){
            const unsigned flags=resource_types.flags(extra_memory,object);
            if(flags&mango_resources::Mineable)collect(object,nullptr,false);
            if(flags&mango_resources::StoneGroup){
                const auto ref=mango_extra::Ref::capture(extra_memory,object);if(!ref.valid(extra_memory))return;
                auto it=std::find_if(stone_groups.begin(),stone_groups.end(),[&](const StoneGroup& g){return g.ref.object==object;});
                if(it!=stone_groups.end()){if(it->ref.native!=ref.native)*it={ref,{},time};else it->seen=time;}
                else if(stone_groups.size()<512)stone_groups.push_back({ref,{},time});
            }
        });
        unsigned budget=128;const size_t passes=stone_groups.empty()?0:4;
        for(size_t pass=0;pass<passes&&budget&&!stone_groups.empty();++pass){
            if(stone_group_cursor>=stone_groups.size())stone_group_cursor=0;
            auto& group=stone_groups[stone_group_cursor++];if(!group.ref.valid(extra_memory)||time-group.seen>30)continue;
            budget-=group.cursor.step(extra_memory,group.ref.object,[&](uintptr_t root){
                const auto ref=mango_extra::Ref::capture(extra_memory,root);if(!resource_root_active(ref))return;
                Obj* component=stone_component(reinterpret_cast<Obj*>(root));if(component)collect(reinterpret_cast<uintptr_t>(component),reinterpret_cast<Obj*>(root),true);
            },std::min(budget,32u));
        }
        if(!valid){resource_nodes.clear();stone_groups.clear();stone_components.clear();}
        for(auto it=resource_nodes.begin();it!=resource_nodes.end();)if(time-it->second.seen>30)it=resource_nodes.erase(it);else ++it;
        resource_status.store(valid?int(resource_nodes.size()):-1);
    }
    static std::vector<const ResourceNode*> nearby;nearby.clear();
    Obj* weapon=held_weapon(local);const bool tool=raw_type_named(weapon,"FPTool")&&raw_field<Obj*>(weapon,0xC0)==local;
    const unsigned purpose=tool?raw_field<unsigned>(weapon,0x168):0;
    FarmFocus focus{};float nearest=settings.farm_range,matching_distance=1000000;bool any_selected=false;
    V3 player_position=origin;raw_position(local,player_position);const auto projection=raw_engine.capture_camera(camera);
    for(const auto& entry:resource_nodes){
        const auto& node=entry.second;if(!(settings.resources&(1u<<node.kind)))continue;
        const float meters=distance(node.world,player_position);any_selected=true;
        if(settings.resource_esp&&meters<=settings.resource_range){V3 point{},world=node.world;world.y+=.6f;
            if((projection.valid?projection.project(world,point):raw_project(camera,world,point))&&mango_resources::on_screen(point))nearby.push_back(&node);}
        if(tool&&(node.purpose&purpose)){
            matching_distance=std::min(matching_distance,meters);
            if(meters<nearest&&resource_alive(node.ref,node.root,node.grouped)&&node.transform.valid(extra_memory)){
                nearest=meters;focus={node.ref,node.root,node.transform,weapon,{node.world.x,node.world.y+.8f,node.world.z},node.grouped,time};}
        }
    }
    if(!settings.farm||!tool||!focus.ref.object||menu_is_open.load()){
        clear_farm();farm_status.store(!settings.farm?0:menu_is_open.load()?5:!settings.resources?8:!tool?4:matching_distance<1000000?6:any_selected?7:1);
    }else{
        {std::lock_guard<std::mutex> lock(farm_focus_mutex);farm_focus=focus;}
        farm_status.store(2);
    }
    if(settings.resource_esp){
        const auto limit=std::min(size_t(settings.resource_limit),nearby.size());
        std::partial_sort(nearby.begin(),nearby.begin()+limit,nearby.end(),[&](const ResourceNode* a,const ResourceNode* b){return distance(a->world,origin)<distance(b->world,origin);});
        for(size_t i=0;i<limit;++i){const auto& node=*nearby[i];V3 point{},world=node.world;world.y+=.6f;
            if(!resource_alive(node.ref,node.root,node.grouped))continue;
            if(projection.valid?projection.project(world,point):raw_project(camera,world,point))snapshot.resources[snapshot.resource_count++]={point,distance(player_position,node.world),node.kind};}
    }
}
void automation_tick(Obj*,Obj*,V3,double);
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
        resource_nodes.clear();stone_groups.clear();stone_components.clear();stone_group_cursor=0;resource_cursor.reset();clear_farm();farm_cadence.reset();farm_tool_session.reset();automation_input.reset();auto_hold_until.store(0);weapon_offset.restore(motion_api);
        avatar_controller.reset(avatar_api);grip_controller.reset(camera_api);
        third_camera_transform.store(0);camera_controller.reset(camera_api);
        local_player.store(nullptr);local_player_seen.store(0);
        LOG("Session %llu rebound: list=%p local=%p camera=%p",static_cast<unsigned long long>(raw_session.generation()),list,local,camera);
    }
    local_player.store(local);local_player_seen.store(local?time:0);
    raw_camera_seen.store(camera!=nullptr);
    Obj* body_info=(want_third_person.load()||want_chams.load())?raw_body_info(local):nullptr;
    prepare_graphics_types(body_info,time);
    if(!want_third_person.load()||!local||!camera){
        avatar_controller.suspend(avatar_api);avatar_asset.retry(time,true);grip_controller.reset(camera_api);report_avatar(0);
        third_camera_transform.store(0);camera_controller.reset(camera_api);
        local_weapon_view.reset();
        if(view_setup_api.read)view_setup.update(view_setup_api,local,false,time);
    }else if(phase>0){
        if(prepare_camera()){
            camera_collision.local=local;
            view_setup.update(view_setup_api,local,true,time);
            Obj* fp=raw_field<Obj*>(local,0x90);
            if(!raw_type_named(fp,"FPManager"))fp=nullptr;
            Obj* weapon=raw_field<Obj*>(fp,0x50);
            if(!raw_type_named(weapon,"FPObject"))weapon=nullptr;
            const auto view=mango_camera::player_view(local,raw_read,raw_type_named);
            local_weapon_view.update(weapon_view_api,local,view,true,time);
            camera_controller.update(camera_api,camera,native_local,raw_field<void*>(fp,0x10),
                raw_field<void*>(weapon,0x10),true,camera_distance.load(),frame,time,view.animator,view.info,view.render_controller,view.reserved_layers,view.local,local_weapon_view.visual);
            update_avatar(local,view,time);
            third_camera_transform.store(camera_controller.native_transform());
        }else camera_controller.status=-1;
    }
    camera_status.store(camera_controller.status);
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
            if(!want_esp.load()&&!aim_requested()&&!want_chams.load())break;
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
        const float range=std::max((want_esp.load()||aim_requested())?esp_range.load():0.f,want_chams.load()?chams_range.load():0.f);
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
            capture_reference_labels(entry.address,player);
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
        {std::lock_guard<std::mutex> lock(chams_appearance_mutex);options.appearance=published_chams_appearance;}
        chams_controller.update(chams_api,options,candidates,time);
        chams_status.store(chams_controller.status);
    }
    resource_capture(local,camera,origin,next,time);
    automation_tick(local,camera,origin,time);
    if(!extra_config().weapon||want_third_person.load())weapon_offset.restore(motion_api);
    {std::lock_guard<std::mutex> lock(snapshot_mutex);published.assign(next);}
    player_features_tick(time);
}

bool ray_to_point(V3 origin,V3 point,V3& direction){
    if(!finite(origin)||!finite(point))return false;
    const V3 delta{point.x-origin.x,point.y-origin.y,point.z-origin.z};
    const float length=distance(origin,point);
    if(!(length>.01f)||!std::isfinite(length))return false;
    direction={delta.x/length,delta.y/length,delta.z/length};
    return finite(direction);
}
bool magic_target(Obj* camera,V3 origin,V3& point,V3* velocity=nullptr){
    const double time=now();
    const int width=render_width.load(),height=render_height.load();
    Obj* local=static_cast<Obj*>(local_player.load());
    if(!camera||!raw_field<Obj*>(camera,0x10)||width<180||height<180||!finite(origin)||
       !local||time-local_player_seen.load()>1.0)return false;
    static thread_local Obj* team_owner=nullptr;
    static thread_local double team_at=0;
    static thread_local char own_team[48]{};
    if(team_owner!=local||time-team_at>5){
        raw_team_tag(raw_field<Obj*>(local,0x2B0),own_team,sizeof(own_team));
        team_owner=local;team_at=time;
    }
    const bool team_filter=want_magic_team_filter.load(),automatic=want_auto_shoot.load();
    const int point_mode=want_magic_bullet.load()?0:magic_point.load();
    const float max_distance=esp_range.load(),fov=magic_fov.load();
    static thread_local double selected_at=0;
    static thread_local Obj *selected=nullptr,*selected_camera=nullptr,*selected_local=nullptr;
    static thread_local float selected_range=0,selected_fov=0;
    static thread_local int selected_mode=-1;
    static thread_local bool selected_team=false,selected_auto=false;
    const bool reuse=selected&&selected_camera==camera&&selected_local==local&&
        selected_range==max_distance&&selected_fov==fov&&selected_mode==point_mode&&
        selected_team==team_filter&&selected_auto==automatic&&time-selected_at<1.0/45;
    RawObservation chosen{};bool found=false;float best=fov*fov;
    {
        std::lock_guard<std::mutex> lock(raw_observations_mutex);
        for(const auto& entry:raw_observations){
            if(entry.local||!entry.visible||time-entry.stamp>.25||entry.human!=1)continue;
            if(automatic&&(entry.visibility!=mango_esp::Visibility::Visible||time-entry.visibility_at>.3))continue;
            if(team_filter&&own_team[0]&&entry.team[0]&&!std::strcmp(own_team,entry.team))continue;
            if(reuse&&entry.address!=selected)continue;
            const V3 world=point_mode==1&&entry.chest_valid?entry.chest:entry.world.head;
            if(distance(origin,world)>max_distance)continue;
            V3 view{};if(!raw_project(camera,world,view))continue;
            const float x=(view.x-.5f)*float(width),y=(view.y-.5f)*float(height),score=x*x+y*y;
            if(score>=best)continue;
            if(!player_alive(entry.address)||raw_field<Obj*>(entry.address,0x10)!=entry.native_address||
               raw_field<Obj*>(entry.address,0x1B0)!=entry.animator||
               raw_field<Obj*>(entry.animator,0x10)!=entry.native_animator)continue;
            best=score;chosen=entry;found=true;
        }
    }
    if(!found){selected=nullptr;magic_targets.store(0);return false;}
    if(!reuse){selected_at=time;selected_camera=camera;selected_local=local;selected_range=max_distance;
        selected_fov=fov;selected_mode=point_mode;selected_team=team_filter;selected_auto=automatic;selected=chosen.address;}
    // Cache only the selected identity. Re-read its animated head on every ray
    // and firing call, outside the observations mutex.
    mango_aim::Target target{mango_extra::Ref::capture(extra_memory,reinterpret_cast<uintptr_t>(chosen.address)),
        mango_extra::Ref::capture(extra_memory,reinterpret_cast<uintptr_t>(chosen.animator)),chosen.human==1};
    if(target.player.native!=reinterpret_cast<uintptr_t>(chosen.native_address)||
       target.animator.native!=reinterpret_cast<uintptr_t>(chosen.native_animator)){selected=nullptr;magic_targets.store(0);return false;}
    static const mango_aim::Api live{extra_memory,
        [](uintptr_t animator,int bone)->uintptr_t{return reinterpret_cast<uintptr_t>(raw_fn<Obj*(*)(Obj*,int,const Method*)>(0xD40FE38)(reinterpret_cast<Obj*>(animator),bone,nullptr));},
        [](uintptr_t transform,mango_extra::V3& out)->bool{V3 v=raw_fn<V3(*)(Obj*,const Method*)>(0xD4CFB20)(reinterpret_cast<Obj*>(transform),nullptr);out={v.x,v.y,v.z};return finite(v);},
        [](uintptr_t player,mango_extra::V3& out)->bool{V3 v{};if(!raw_position(reinterpret_cast<Obj*>(player),v))return false;out={v.x,v.y,v.z};return true;},
        [](uintptr_t player)->bool{return player_alive(reinterpret_cast<Obj*>(player));}};
    mango_extra::V3 current{};V3 view{};
    if(!mango_aim::live_point(live,target,point_mode,current)){selected=nullptr;magic_targets.store(0);return false;}
    point={current.x,current.y,current.z};
    if(distance(origin,point)>max_distance||!raw_project(camera,point,view)){
        selected=nullptr;magic_targets.store(0);return false;
    }
    const float x=(view.x-.5f)*float(width),y=(view.y-.5f)*float(height);
    found=x*x+y*y<fov*fov;magic_targets.store(found?1:0);if(!found)selected=nullptr;
    if(found&&velocity){const auto speed=chosen.motion.velocity(time);*velocity={speed.x,speed.y,speed.z};}
    return found;
}
struct ShotPhysics {mango_extra::Ref weapon;V3 inherited{};double at=0;};
ShotPhysics shot_physics;std::mutex shot_physics_mutex;
using BallisticFire=void(*)(Obj*,const void*,Obj*,const Method*);
BallisticFire original_ballistic_fire=nullptr;
void hook_ballistic_fire(Obj* self,const void* shot,Obj* owner,const Method* method){
    Obj* local=static_cast<Obj*>(local_player.load());
    if(local&&shot&&raw_type_named(owner,"FPHitscan")&&raw_field<Obj*>(owner,0xC0)==local){
        last_projectile_at.store(now());
        V3 inherited{};
        if(raw_read(reinterpret_cast<uintptr_t>(shot)+0x24,&inherited,sizeof(inherited))&&finite(inherited)){
            const auto ref=mango_extra::Ref::capture(extra_memory,reinterpret_cast<uintptr_t>(owner));
            std::lock_guard<std::mutex> lock(shot_physics_mutex);shot_physics={ref,inherited,now()};
        }
    }
    if(original_ballistic_fire)original_ballistic_fire(self,shot,owner,method);
}
bool predicted_direction(Obj* weapon,V3 origin,V3 target,V3 velocity,V3& direction){
    Obj* config=raw_field<Obj*>(weapon,0x130);Obj* profile=raw_field<Obj*>(config,0x60);
    if(!raw_type_named(config,"HitscanWeaponConfig")||!raw_type_named(profile,"BallisticProfile"))return ray_to_point(origin,target,direction);
    const float speed=raw_field<float>(config,0x30); // BallisticShot.MuzzleSpeed, not LOZ() (MaxRange).
    mango_prediction::Ballistics ballistics;ballistics.speed=speed;
    ballistics.drag=raw_field<float>(profile,origin.y<0?0x18:0x14);
    // Predict target movement without adding gravity compensation.
    ballistics.acceleration={};
    {std::lock_guard<std::mutex> lock(shot_physics_mutex);if(shot_physics.weapon.object==reinterpret_cast<uintptr_t>(weapon)&&
        shot_physics.weapon.valid(extra_memory)&&now()-shot_physics.at<.5)ballistics.inherited={shot_physics.inherited.x,shot_physics.inherited.y,shot_physics.inherited.z};}
    mango_prediction::Solution solution;
    if(!mango_prediction::solve({origin.x,origin.y,origin.z},{target.x,target.y,target.z},{velocity.x,velocity.y,velocity.z},ballistics,solution))
        return ray_to_point(origin,target,direction);
    return ray_to_point(origin,{solution.point.x,solution.point.y,solution.point.z},direction);
}
using AttackHeld=bool(*)(Obj*,const Method*);AttackHeld original_attack_held=nullptr;
bool hook_attack_held(Obj* self,const Method* method){
    const bool physical=original_attack_held&&original_attack_held(self,method);if(physical)return true;
    Obj* local=static_cast<Obj*>(local_player.load());
    return game_ready.load()&&local&&raw_field<Obj*>(self,0x30)==local&&
        mango_prediction::hold(want_auto_shoot.load(),menu_is_open.load(),auto_hold_until.load(),now());
}
using InputUpdate=void(*)(Obj*,const Method*);InputUpdate original_input_update=nullptr;
void hook_input_update(Obj* self,const Method* method){
    Obj* local=static_cast<Obj*>(local_player.load());
    if(!self||!local||raw_field<Obj*>(self,0x30)!=local){if(original_input_update)original_input_update(self,method);return;}
    const double time=now();automation_input_at.store(time);Obj* weapon=held_weapon(local);
    const bool physical=original_attack_held&&original_attack_held(self,method);
    const bool desired=game_ready.load()&&player_alive(local)&&raw_field<Obj*>(weapon,0xC0)==local&&
        mango_prediction::hold(want_auto_shoot.load(),menu_is_open.load(),auto_hold_until.load(),time);
    const int mode=automation_mode.load();
    auto press=[&](bool pressed){
        if(raw_field<Obj*>(self,0x38))raw_fn<void(*)(Obj*,bool,const Method*)>(0x6CF26B4)(self,pressed,nullptr);
    };
    automation_input.update(raw_field<uintptr_t>(weapon,0x10),desired,physical,mode==2,time,press);
    if(original_input_update)original_input_update(self,method);
    if(desired&&automation_input.owned()&&mode==2){
        if(!raw_field<uint8_t>(self,0x68))press(true);
        Obj* action=raw_field<Obj*>(raw_field<Obj*>(self,0x38),0x1F0);
        if(raw_type_named(action,"kAx"))raw_fn<bool(*)(Obj*,const Method*)>(0x6D28158)(action,nullptr);
    }
}
// Resolve metadata explicitly: these slots can still be encoded before the
// player's first attack. Do not borrow the cached camera ray or a prior hit.
Obj* farm_metadata(uintptr_t slot_rva){
    Obj* slot=reinterpret_cast<Obj*>(api.base+slot_rva);
    raw_fn<void(*)(Obj*)>(mango_offsets::metadata_init)(slot);
    return raw_field<Obj*>(slot,0);
}
struct FarmWeakspot {bool active=false;V3 point{},forward{},up{};};
FarmWeakspot farm_weakspot(const FarmFocus& focus){
    FarmWeakspot result;
    Obj* resource=reinterpret_cast<Obj*>(focus.ref.object);
    Obj* object=raw_fn<Obj*(*)(Obj*,const Method*)>(0xD4B6594)(resource,nullptr);
    const Method* method=reinterpret_cast<const Method*>(farm_metadata(mango_offsets::farm_extensions_method_slot));
    if(!object||!method)return result;
    Obj* components=raw_fn<Obj*(*)(Obj*,const Method*)>(mango_offsets::farm_get_extensions)(object,method);
    const uintptr_t count=raw_field<uintptr_t>(components,0x18);if(!components||count>128)return result;
    std::array<Obj*,128> pending{},visited{};size_t size=0,seen=0;
    for(size_t i=0;i<count;++i)pending[size++]=raw_field<Obj*>(components,0x20+i*sizeof(Obj*));
    while(size&&seen<visited.size()){
        Obj* extension=pending[--size];
        if(!extension||std::find(visited.begin(),visited.begin()+seen,extension)!=visited.begin()+seen)continue;
        visited[seen++]=extension;
        if(!raw_field<Obj*>(extension,0x10))continue;
        if(raw_type_named(extension,"MineableObjectExtension_Bypass")){
            Obj* list=raw_field<Obj*>(extension,0x20);Obj* array=raw_field<Obj*>(list,0x10);
            const int n=raw_field<int>(list,0x18);const uintptr_t capacity=raw_field<uintptr_t>(array,0x18);
            if(n>0&&n<=128&&uintptr_t(n)<=capacity)for(int i=0;i<n&&size<pending.size();++i)
                pending[size++]=raw_field<Obj*>(array,0x20+i*sizeof(Obj*));
            continue;
        }
        Obj* marker=nullptr;Obj* transform=nullptr;
        if(raw_type_named(extension,"MineableObjectExtension_TreeHitstreaks")){
            marker=raw_field<Obj*>(extension,0x50);
            if(raw_type_named(marker,"MineableObjectExtension_HitMarkerItem"))transform=raw_field<Obj*>(marker,0x38);
        }else if(raw_type_named(extension,"MineableObjectExtension_OreHitstreaks")){
            marker=raw_field<Obj*>(extension,0x30);
            if(!raw_type_named(marker,"MineableObjectExtension_OreHitstreaksMarker"))marker=nullptr;
        }
        if(!marker||!raw_field<Obj*>(marker,0x10))continue;
        Obj* marker_object=raw_fn<Obj*(*)(Obj*,const Method*)>(0xD4B6594)(marker,nullptr);
        if(!marker_object||!raw_fn<bool(*)(Obj*,const Method*)>(0xD4BBA44)(marker_object,nullptr))continue;
        if(!transform)transform=raw_fn<Obj*(*)(Obj*,const Method*)>(0xD4B63F0)(marker,nullptr);
        if(!raw_field<Obj*>(transform,0x10)||!raw_position(transform,result.point))continue;
        result.forward=raw_fn<V3(*)(Obj*,const Method*)>(mango_offsets::farm_transform_forward)(transform,nullptr);
        result.up=raw_fn<V3(*)(Obj*,const Method*)>(mango_offsets::farm_transform_up)(transform,nullptr);
        result.active=finite(result.forward)&&finite(result.up);if(result.active)return result;
    }
    return {};
}
// Trace to this resource rather than to the viewing direction. Once a bonus
// marker exists, never replace it with a fixed trunk/rock point.
bool farm_surface(Obj* local,const FarmFocus& focus,V3 resource,const FarmWeakspot& weakspot,V3& origin,RaycastHit& result){
    V3 feet{};if(!raw_position(local,feet))return false;
    origin={feet.x,feet.y+1.35f,feet.z};
    std::array<uint64_t,2> scene{};if(!raw_engine.capture_scene(local,scene))return false;
    void* native_local=raw_field<void*>(local,0x10);
    void* own=native_local?raw_engine.unwrap_native(raw_engine.transform(native_local)):nullptr;
    void* root=reinterpret_cast<void*>(focus.transform.native);
    auto cast=[&](V3 from,V3 point,float extra,RaycastHit& result)->int{
        V3 direction{};if(!ray_to_point(from,point,direction))return 0;
        Ray ray{from,direction};float remaining=distance(from,point)+extra;
        for(unsigned pass=0;pass<4&&remaining>.01f;++pass){
            RaycastHit hit{};
            if(!raw_engine.raycast(scene.data(),&ray,remaining,&hit,~(1<<2),1))return 0;
            if(!hit.collider||!std::isfinite(hit.distance)||hit.distance<0||hit.distance>remaining+.05f)return -1;
            void* collider=raw_engine.unwrap_native(raw_engine.collider(&hit.collider));
            void* transform=collider?raw_engine.unwrap_native(raw_engine.transform(collider)):nullptr;
            if(!transform)return -1;
            if(transform==root||raw_engine.is_child(transform,root)){
                if(!finite(hit.point)||!finite(hit.normal))return -1;
                result=hit;return 1;
            }
            if(!own||(transform!=own&&!raw_engine.is_child(transform,own)))return -1;
            const float step=hit.distance+.02f;remaining-=step;
            ray.origin={ray.origin.x+direction.x*step,ray.origin.y+direction.y*step,ray.origin.z+direction.z*step};
        }
        return 0;
    };
    return mango_farm::surface(origin,resource,weakspot,result,cast,
        [](V3 a,V3 b){return distance(a,b);});
}
int send_farm_request(Obj* local,Obj* weapon,const FarmFocus& focus,V3 resource){
    Obj* ray_class=farm_metadata(mango_offsets::farm_ray_type_slot);if(!ray_class)return 11;
    const FarmWeakspot weakspot=farm_weakspot(focus);
    V3 origin{};RaycastHit hit{};
    if(!farm_surface(local,focus,resource,weakspot,origin,hit))return 9;
    Obj* data=raw_fn<Obj*(*)(Obj*)>(mango_offsets::object_new)(ray_class);
    if(!data||!raw_type_named(data,"kAm"))return 11;
    raw_fn<void(*)(Obj*,RaycastHit,V3,const Method*)>(mango_offsets::farm_ray_ctor)(data,hit,origin,nullptr);
    // FPTool.On_Hit prepares LastLocalHitPoint/Time, invokes the resource's
    // QJg handlers (including the real HitStreak), and dispatches the normal
    // harvest command. It does not call AttackOnce or start the swing animator.
    raw_fn<void(*)(Obj*,Obj*,const Method*)>(mango_offsets::farm_tool_hit)(weapon,data,nullptr);
    farm_requests.fetch_add(1,std::memory_order_relaxed);
    (weakspot.active?farm_weakspot_hits:farm_starter_hits).fetch_add(1,std::memory_order_relaxed);
    return 3;
}
void proximity_farm_tick(Obj* local,Obj* weapon,bool available,const mango_extra::Settings& settings,double time){
    if(!settings.farm||!available||!raw_type_named(weapon,"FPTool")){farm_tool_session.reset();return;}
    if(menu_is_open.load()){farm_status.store(5);return;}
    const bool prepared=farm_tool_session.ready(reinterpret_cast<uintptr_t>(weapon),raw_field<uintptr_t>(weapon,0x10),raw_field<uintptr_t>(weapon,0x40),time,[&](){
        Obj* network=raw_field<Obj*>(weapon,0x98);
        if(!raw_type_named(network,"PlayerWeapon")||raw_field<Obj*>(network,0x160)!=local||!raw_fn<bool(*)(Obj*,const Method*)>(mango_offsets::farm_tool_ready)(weapon,nullptr))return false;
        raw_fn<void(*)(Obj*,const Method*)>(mango_offsets::farm_activate_tool)(network,nullptr);
        farm_cadence.reset();LOG("Farm: activated equipped tool=%p item=%p",weapon,raw_field<Obj*>(weapon,0x40));return true;
    });
    if(!prepared){farm_status.store(10);return;}
    FarmFocus focus;{std::lock_guard<std::mutex> lock(farm_focus_mutex);focus=farm_focus;}
    if(!available||!raw_type_named(weapon,"FPTool")||focus.weapon!=weapon||!focus.ref.object)return;
    if(time<focus.time||time-focus.time>=.3||!resource_alive(focus.ref,focus.root,focus.grouped)||!focus.transform.valid(extra_memory)||!raw_engine.ready){farm_status.store(1);return;}
    V3 player{},resource{};
    if(!raw_position(local,player)||!raw_position(reinterpret_cast<Obj*>(focus.ref.object),resource)||distance(player,resource)>settings.farm_range){farm_status.store(6);return;}
    float period=raw_field<float>(weapon,0x138),cycle=raw_field<float>(weapon,0x124);
    if(!std::isfinite(period)||period<=0)period=1;
    if(std::isfinite(cycle)&&cycle>period)period=cycle;
    farm_cadence.tick(true,time,period,[&](){
        const int status=send_farm_request(local,weapon,focus,resource);farm_status.store(status);return status==3;
    });
}
void automation_tick(Obj* local,Obj* camera,V3 origin,double time){
    auto settings=extra_config();
    if(!settings.auto_shoot&&!settings.farm){auto_hold_until.store(0);auto_status.store(0);return;}
    Obj* weapon=held_weapon(local);Obj* events=raw_field<Obj*>(weapon,0xC8);
    if(!events)events=raw_field<Obj*>(local,0x78);
    // Let the game's attack handler check readiness, draw/reload and cooldowns.
    const bool available=local&&camera&&events&&player_alive(local)&&raw_field<Obj*>(weapon,0xC0)==local;
    const bool gun_type=raw_type_named(weapon,"FPHitscan");
    auto bound_at=[&](size_t offset){Obj* action=raw_field<Obj*>(events,offset);return raw_type_named(action,"kAx")&&(raw_field<Obj*>(action,0x10)||raw_field<Obj*>(action,0x18));};
    const int mode=gun_type?raw_field<int>(weapon,0x138):2;
    const bool bound=bound_at(0x1E8)||(mode==2&&bound_at(0x1F0));
    V3 target{};const bool gun=available&&gun_type&&(magic_hook_state.load()&28);
    Obj* aim=raw_field<Obj*>(events,0x2B0);const bool ads=!want_magic_ads_only.load()||raw_field<uint8_t>(aim,0x10);
    const bool shoot_ready=settings.auto_shoot&&gun&&bound&&ads&&!menu_is_open.load()&&magic_target(camera,origin,target);
    const bool recent_input=time-automation_input_at.load()<.5;
    auto_status.store(!settings.auto_shoot?0:menu_is_open.load()?10:!gun_type?4:!available?5:!(magic_hook_state.load()&28)?7:!bound?8:!ads?6:!recent_input?11:
                      shoot_ready?(time-last_projectile_at.load()<.25?3:2):1);
    proximity_farm_tick(local,weapon,available,settings,time);
    automation_mode.store(shoot_ready?mode:2);auto_hold_until.store(shoot_ready?time+.12:0);
    static double last_report=0;
    if(time-last_report>=10){
        last_report=time;unsigned counts[4]{};for(const auto& node:resource_nodes)if(node.second.kind>=0&&node.second.kind<4)++counts[node.second.kind];
        LOG("Automation: auto=%d farm=%d harvest_calls=%u weakspots=%u starters=%u request=%d input_age=%.2f shot_age=%.2f legacy_ready=%d targets=%d resources=%u/%u/%u/%u groups=%zu",
            auto_status.load(),farm_status.load(),farm_requests.load(),farm_weakspot_hits.load(),farm_starter_hits.load(),int(shoot_ready),std::clamp(time-automation_input_at.load(),0.,99.),
            std::clamp(time-last_projectile_at.load(),0.,99.),int(raw_field<uint8_t>(events,0x1F9)),magic_targets.load(),counts[0],counts[1],counts[2],counts[3],stone_groups.size());
    }
}
using RawRay=Ray(*)(Obj*,const Method*);
RawRay original_ray_lsb=nullptr,original_ray_zzc=nullptr;
Ray magic_ray(Obj* self,const Method* method,RawRay original){
    Ray ray=original?original(self,method):Ray{};
    if(want_third_person.load()&&syscall(SYS_gettid)==unity_camera_thread.load()){
        const auto origin=camera_controller.undo_ray_origin({ray.origin.x,ray.origin.y,ray.origin.z});
        ray.origin={origin.x,origin.y,origin.z};
    }
    if(menu_is_open.load()||!game_ready.load()||!self)return ray;
    Obj* local=static_cast<Obj*>(local_player.load());
    if(!local||raw_field<Obj*>(self,0x20)!=local)return ray;
    if(!aim_requested())return ray;
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
thread_local uintptr_t shot_dispersion=0;
using Dispersion=float(*)(Obj*,bool,const Method*);
Dispersion original_dispersion=nullptr;
float hook_dispersion(Obj* self,bool ads,const Method* method){
    if(mango_aim::suppress_spread(shot_dispersion,reinterpret_cast<uintptr_t>(self)))return 0;
    return original_dispersion?original_dispersion(self,ads,method):0;
}
void magic_hit_ray(Obj* self,Ray ray,const Method* method,HitRay original){
    uintptr_t precise_dispersion=0;
    if(want_third_person.load()&&syscall(SYS_gettid)==unity_camera_thread.load()){
        const auto origin=camera_controller.undo_ray_origin({ray.origin.x,ray.origin.y,ray.origin.z});
        ray.origin={origin.x,origin.y,origin.z};
    }
    magic_shot_calls.fetch_add(1,std::memory_order_relaxed);
    if(aim_requested()&&!menu_is_open.load()&&game_ready.load()&&self){
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
                V3 target{},direction{},velocity{};
                if(magic_target(camera,ray.origin,target,&velocity)&&
                   predicted_direction(self,ray.origin,target,velocity,direction)){
                    ray.direction=direction;
                    precise_dispersion=reinterpret_cast<uintptr_t>(raw_field<Obj*>(self,0x1F8));
                    magic_shot_rewrites.fetch_add(1,std::memory_order_relaxed);
                }
            }
        }
    }
    mango_aim::DispersionScope scope(shot_dispersion,precise_dispersion);
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
    static bool motion_installed=false,dispersion_installed=false,input_hold_installed=false,input_update_installed=false,ballistic_fire_installed=false;
    int player=0,canvas=0,lsb=0,zzc=0;
    int zer=0,zmm=0,loa=0;
    if(!input_update_installed)input_update_installed=hook(reinterpret_cast<void*>(api.base+0x6CF6AD0),reinterpret_cast<void*>(hook_input_update),reinterpret_cast<void**>(&original_input_update))==0;
    if(!input_hold_installed)input_hold_installed=hook(reinterpret_cast<void*>(api.base+0x6CF6B8C),reinterpret_cast<void*>(hook_attack_held),reinterpret_cast<void**>(&original_attack_held))==0;
    if(!ballistic_fire_installed)ballistic_fire_installed=hook(reinterpret_cast<void*>(api.base+0x7BF3C1C),reinterpret_cast<void*>(hook_ballistic_fire),reinterpret_cast<void**>(&original_ballistic_fire))==0;
    if(!dispersion_installed)dispersion_installed=hook(reinterpret_cast<void*>(api.base+0x7BDD914),reinterpret_cast<void*>(hook_dispersion),reinterpret_cast<void**>(&original_dispersion))==0;
    if(!motion_installed)motion_installed=hook(reinterpret_cast<void*>(api.base+0x6D6B890),reinterpret_cast<void*>(hook_motion_update),reinterpret_cast<void**>(&original_motion_update))==0;
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
    raw_hooks_complete.store(player_installed&&canvas_installed&&before_installed&&begin_camera_installed&&motion_installed&&
                            input_update_installed&&input_hold_installed&&ballistic_fire_installed&&dispersion_installed&&lsb_installed&&zzc_installed&&zer_installed&&zmm_installed&&loa_installed);
    LOG("Raw callbacks: Player=%d Canvas=%d Before=%d BeginCamera=%d Ray=%d/%d Hitscan=%d/%d/%d",
        player,canvas,before_installed,begin_camera_installed,lsb,zzc,zer,zmm,loa);
    if(player_installed||canvas_installed||before_installed)return true;
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


mango_ui::State menu_state;
float scale=1;double last_render=0;std::atomic<bool> ui_config_ready{false},vk_hooks_ready{false};
std::mutex config_queue_mutex;std::array<char,2048> queued_config{};bool config_queued=false;
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
            if(std::isfinite(alpha))cfg.chams_alpha=std::clamp(alpha,.05f,1.f);
            if(std::isfinite(chams_distance))cfg.chams_range=std::clamp(chams_distance,15.f,150.f);
            cfg.chams_limit=std::clamp(limit,4,32);
        }
        int third=0;float distance=2.5f;
        if(std::fscanf(f,"%d %f",&third,&distance)==2){
            cfg.third_person=third!=0;
            if(std::isfinite(distance))cfg.camera_distance=std::clamp(distance,.8f,5.f);
        }
        int model=0;if(std::fscanf(f,"%d",&model)==1)cfg.custom_model=model!=0;
        mango_chams::load_appearance(f,cfg.chams_appearance);
        mango_extra::load(f,cfg.extra);
        mango_reference::load_options(f,cfg.reference);
    }
    std::fclose(f);
}
void save_config(){
    std::array<char,2048> text{};
    std::snprintf(text.data(),text.size(),"MANGO_ESP_V1 %d %d %d %d %d %d %d %.1f %.3f %.3f %.3f %d %.0f %d %d %d %d %d %d %d %d %d %.3f %.1f %d %d %.2f %d\n",
        cfg.enabled,cfg.box,cfg.distances,cfg.lines,cfg.skeleton,cfg.fill,cfg.corners,
        cfg.range,cfg.color[0],cfg.color[1],cfg.color[2],cfg.magic_bullet,cfg.magic_fov,
        cfg.magic_ads_only,cfg.magic_team_filter,cfg.magic_point,
        cfg.no_recoil,cfg.no_spread,cfg.no_weapon_motion,cfg.visibility_check,cfg.visible_only,
        cfg.chams,cfg.chams_alpha,cfg.chams_range,cfg.chams_limit,cfg.third_person,cfg.camera_distance,cfg.custom_model);
    const size_t used=std::strlen(text.data());
    if(!mango_chams::save_appearance(text.data()+used,text.size()-used,cfg.chams_appearance))return;
    const size_t extra_offset=std::strlen(text.data());
    if(!mango_extra::save(text.data()+extra_offset,text.size()-extra_offset,cfg.extra))return;
    const size_t reference_offset=std::strlen(text.data());
    if(!mango_reference::save_options(text.data()+reference_offset,text.size()-reference_offset,cfg.reference))return;
    std::lock_guard<std::mutex> lock(config_queue_mutex);queued_config=text;config_queued=true;
}
void flush_config(){
    std::array<char,2048> text{};
    {std::lock_guard<std::mutex> lock(config_queue_mutex);if(!config_queued)return;text=queued_config;config_queued=false;}
    char path[256];config_path(path,sizeof(path));FILE* file=std::fopen(path,"w");
    if(!file){std::lock_guard<std::mutex> lock(config_queue_mutex);config_queued=true;return;}
    const size_t size=std::strlen(text.data());const bool complete=std::fwrite(text.data(),1,size,file)==size;
    const bool closed=std::fclose(file)==0;
    if(!complete||!closed){std::lock_guard<std::mutex> lock(config_queue_mutex);config_queued=true;}
}
void init_vulkan_ui(uint32_t w,uint32_t h){
    if(ImGui::GetCurrentContext())return;
    ImGui::CreateContext();ImGuiIO& io=ImGui::GetIO();io.IniFilename=nullptr;
    scale=std::clamp(std::min(w/1100.f,h/720.f),.8f,2.4f);
    menu_state=mango_ui::State{};menu_state.scale=mango_ui::FitScale(w,h);
    mango_ui::ApplyStyle(menu_state.scale);mango_ui::LoadFont(io,menu_state.scale);
    mango_ui::BakeAssets(io);
    LOG("Vulkan UI initialized: %ux%u",w,h);
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
        if(hi.x<0||lo.x>w||hi.y<0||lo.y>h){
            if(cfg.reference.target_arrow)mango_reference::esp_arrow(draw,{center,top},color);
            continue;
        }
        const auto& appearance=cfg.reference;
        if(cfg.box){
            if(cfg.fill)draw->AddRectFilled(lo,hi,ImGui::ColorConvertFloat4ToU32({rgba.x,rgba.y,rgba.z,.1f}));
            mango_reference::esp_box(draw,lo,hi,appearance.thickness,color,cfg.corners);
        }
        auto option_color=[](const float* c){return ImGui::ColorConvertFloat4ToU32({c[0],c[1],c[2],c[3]});};
        if(cfg.skeleton)for(const auto& link:links)if((p.valid&(1u<<link[0]))&&(p.valid&(1u<<link[1]))){
            const auto a=pixel(p.bones[link[0]],w,h),b=pixel(p.bones[link[1]],w,h);
            const float thickness=std::clamp(appearance.thickness,.7f,3.2f);
            draw->AddLine(a,b,mango_reference::esp_shadow,thickness+1.4f);
            draw->AddLine(a,b,option_color(appearance.skeleton_color),thickness);
        }
        const float text_size=std::clamp(28.6f*menu_state.scale*.62f,16.f,19.5f);
        if(appearance.names)mango_reference::esp_text(draw,center,top-4-text_size,text_size,
            option_color(appearance.name_color),p.name.data());
        if(appearance.health)mango_reference::esp_health(draw,lo,hi,p.health_fraction);
        if(cfg.distances){
            char label[32];std::snprintf(label,sizeof(label),"%.0fm",p.meters);
            mango_reference::esp_text(draw,center,hi.y+3,std::max(14.5f,text_size-1.25f),
                option_color(appearance.distance_color),label);
        }
        if(cfg.lines)mango_reference::esp_tracer(draw,{center,top},appearance.thickness,
            option_color(appearance.tracer_color));
    }
}
void sync_config(){
    if(cfg.magic_bullet)cfg.magic_point=0;
    want_esp.store(cfg.enabled);want_skeleton.store(cfg.enabled&&cfg.skeleton);esp_range.store(cfg.range);
    want_visibility.store(cfg.extra.auto_shoot||(cfg.visibility_check&&(cfg.enabled||cfg.chams)));
    mango_extra::sanitize(cfg.extra);
    want_auto_shoot.store(cfg.extra.auto_shoot);
    {std::lock_guard<std::mutex> lock(extra_settings_mutex);published_extra=cfg.extra;}
    want_chams.store(cfg.chams);
    want_third_person.store(cfg.third_person);want_custom_model.store(cfg.custom_model);camera_distance.store(cfg.camera_distance);
    chams_alpha.store(cfg.chams_alpha);chams_range.store(cfg.chams_range);chams_limit.store(cfg.chams_limit);
    {std::lock_guard<std::mutex> lock(chams_appearance_mutex);published_chams_appearance=cfg.chams_appearance;}
    want_magic_bullet.store(cfg.magic_bullet);magic_fov.store(cfg.magic_fov);
    want_magic_ads_only.store(cfg.magic_ads_only);want_magic_team_filter.store(cfg.magic_team_filter);
    magic_point.store(cfg.magic_point);
    want_no_recoil.store(cfg.no_recoil);want_no_spread.store(cfg.no_spread);want_no_weapon_motion.store(cfg.no_weapon_motion);
}
void draw_menu(int w,int h,int players=-1){
    const auto result=mango_ui::Draw(cfg,menu_state,w,h,players,chams_status.load(),camera_status.load(),shader_stage.load(),custom_model_status.load(),auto_status.load(),farm_status.load(),resource_status.load());
    menu_is_open.store(result.open);menu_popup_open.store(result.popup);
    menu_x0.store(result.min.x);menu_y0.store(result.min.y);
    menu_x1.store(result.max.x);menu_y1.store(result.max.y);
    bar_x0.store(result.bar_min.x);bar_y0.store(result.bar_min.y);
    bar_x1.store(result.bar_max.x);bar_y1.store(result.bar_max.y);
    sync_config();if(result.changed)save_config();
}
ImDrawData* draw_vulkan_ui(uint32_t w,uint32_t h){
    if(!ui_config_ready.load(std::memory_order_acquire))return nullptr;
    render_width.store(w);render_height.store(h);
    static Snapshot frame_snapshot;   // render thread only
    {std::lock_guard<std::mutex> lock(snapshot_mutex);frame_snapshot.assign(published);}
    const Snapshot& s=frame_snapshot;
    Pointer pointer;{std::lock_guard<std::mutex> lock(pointer_mutex);pointer=published_pointer;}
    // Android events can operate the menu even before Unity's player/input
    // callbacks publish their first snapshot. They already use window axes.
    if(native_touch_seen.load()&&
       menu_hit(native_touch_x.load(),native_touch_y.load())){
        pointer.position={native_touch_x.load()/float(w),1.f-native_touch_y.load()/float(h)};
        pointer.down=native_touch_down.load();pointer.stamp=now();input_backend.store(3);
    }
    ImGuiIO& io=ImGui::GetIO();double time=now();io.DisplaySize={float(w),float(h)};
    menu_state.scale=mango_ui::FitScale(w,h);
    ImGui::GetStyle().FontScaleMain=menu_state.scale;
    io.DeltaTime=last_render>0?std::clamp(float(time-last_render),.001f,.25f):1.f/60;last_render=time;
    const bool fresh=time-pointer.stamp<.5;
    pointer_down_state.store(fresh&&pointer.down);
    if(fresh)io.AddMousePosEvent(pointer.position.x*w,(1-pointer.position.y)*h);
    io.AddMouseButtonEvent(0,fresh&&pointer.down);
    ImGui::NewFrame();
    draw_players(s,w,h);
    if(cfg.extra.resource_esp&&s.stamp>0&&time-s.stamp<.5){
        static constexpr ImU32 colors[]={IM_COL32(130,235,140,255),IM_COL32(158,164,171,255),IM_COL32(190,108,72,255),IM_COL32(255,188,28,255)};
        auto* draw=ImGui::GetBackgroundDrawList();for(size_t i=0;i<s.resource_count;++i){const auto& resource=s.resources[i];
            if(resource.kind<0||resource.kind>3||!(cfg.extra.resources&(1u<<resource.kind))||resource.point.x<0||resource.point.x>1||resource.point.y<0||resource.point.y>1)continue;
            ImVec2 p=pixel(resource.point,w,h);char text[64];std::snprintf(text,sizeof(text),"%s %.0fm",mango_extra::resource_name(resource.kind),resource.meters);
            draw->AddQuadFilled({p.x,p.y-6},{p.x+6,p.y},{p.x,p.y+6},{p.x-6,p.y},IM_COL32(2,4,7,210));
            draw->AddQuadFilled({p.x,p.y-5},{p.x+5,p.y},{p.x,p.y+5},{p.x-5,p.y},colors[resource.kind]);
            mango_reference::esp_text(draw,p.x,p.y+8,std::clamp(28.6f*menu_state.scale*.61f,15.5f,18.5f),colors[resource.kind],text);
        }
    }
    const int players=s.stamp>0&&time-s.stamp<.5?int(s.total)-(s.local&&s.total?1:0):-1;
    draw_menu(w,h,players);
    if((cfg.magic_bullet||cfg.extra.auto_shoot)&&!menu_is_open.load()){
        ImGui::GetBackgroundDrawList()->AddCircle({w*.5f,h*.5f},cfg.magic_fov,
            IM_COL32(189,123,255,170),64,1.5f);
    }
    ImGui::Render();
    return ImGui::GetDrawData();
}
void vulkan_presented(uint32_t w,uint32_t h){
    if(w&&h){render_width.store(w);render_height.store(h);}
    if(!frames.load())first_graphics_frame_at.store(now());
    if(frames.fetch_add(1)==0)LOG("Vulkan first present");
}
bool game_process(){char cmd[256]{};FILE* f=std::fopen("/proc/self/cmdline","r");if(!f)return false;std::fread(cmd,1,sizeof(cmd)-1,f);std::fclose(f);return !std::strcmp(cmd,package);}
void install_vulkan_early(){
    if(vk_hooks_ready.load())return;
    mango_vk::Callbacks callbacks{init_vulkan_ui,draw_vulkan_ui,vulkan_presented,[](const char* message){LOG("%s",message);}};
    if(mango_vk::install(hook,raw_read,callbacks)){vk_hooks_ready.store(true);LOG("Vulkan hooks installed");}
}
void boot(){
    char cmd[256]{};FILE* f=std::fopen("/proc/self/cmdline","r");if(f){std::fread(cmd,1,sizeof(cmd)-1,f);std::fclose(f);}
    if(std::strcmp(cmd,package)){LOG("Wrong process: %s",cmd);return;}
    load_config();sync_config();ui_config_ready.store(true,std::memory_order_release);write_shader_file();write_avatar_file();
    raw_observations.reserve(max_players);
    LOG("MANGOcheat ESP %s build %s %s package=%s",revision_marker,__DATE__,__TIME__,package);
    for(unsigned attempts=0;;++attempts){
        if(!shader_file_ready.load()&&attempts%10==0)write_shader_file();
        flush_config();write_avatar_file();
        if(!vk_hooks_ready.load())install_vulkan_early();
        mango_vk::poll_unity();
        const double first_frame=first_graphics_frame_at.load();
        const bool graphics_stable=first_frame>0&&now()-first_frame>=4.0;
        if(graphics_stable)install_android_input_hook();
        if((!game_ready.load()||!raw_hooks_complete.load())&&api.load()&&graphics_stable){
            if(install_raw_hooks()){
                game_ready.store(true);
                LOG("Oxide raw player and touch capture installed");
            }
        }
        static bool wait_logged=false;
        if(api.loaded&&!graphics_stable&&!wait_logged){
            wait_logged=true;LOG("Callbacks deferred until Vulkan presents and Unity startup settles");
        }
        if(attempts==10&&!frames.load())LOG("No Vulkan presents captured: hooks=%d. Check the r30 launcher report.",int(vk_hooks_ready.load()));
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
            if(!mango::game_process())return;
            LOG("boot: pid=%d uid=%d revision=%s",int(getpid()),int(getuid()),mango::revision_marker);
            // Install while dlopen still owns the recursive linker lock, before
            // other threads finish graphics-driver loading and create swapchains.
            mango::install_vulkan_early();
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
