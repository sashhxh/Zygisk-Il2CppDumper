#include "vulkan_hooks.h"
#include "runtime_loader.h"
#include "unity/IUnityGraphics.h"
#include "unity/IUnityGraphicsVulkan.h"
#include <dlfcn.h>
#include <cstring>
#include <atomic>
#include <mutex>

namespace mango_vk {
namespace {
#define HOOKED(X) X(GetInstanceProcAddr) X(GetDeviceProcAddr) X(CreateInstance) X(CreateDevice) X(GetDeviceQueue) X(GetDeviceQueue2) X(CreateSwapchainKHR) X(DestroySwapchainKHR) X(QueuePresentKHR) X(DestroyDevice)
struct Hooks {
    std::recursive_mutex mutex;Hook hook=nullptr;Read read=nullptr;Callbacks callbacks{};void* library=nullptr;Overlay* overlay=nullptr;Device current{};
    mango_runtime::Resolver unity;IUnityGraphics* graphics=nullptr;IUnityGraphicsVulkan* vulkan=nullptr;bool unity_ready=false;
    // Unity can cache driver entry points before dlopen. Its runtime API swaps
    // those cached pointers as well; loader-export hooks alone cannot do that.
    std::atomic<PFN_vkCreateSwapchainKHR> unity_create{nullptr};
    std::atomic<PFN_vkDestroySwapchainKHR> unity_destroy{nullptr};
    std::atomic<PFN_vkQueuePresentKHR> unity_present{nullptr};
    std::atomic<PFN_vkDestroyDevice> unity_destroy_device{nullptr};
    bool bound_create=false,bound_destroy=false,bound_present=false,bound_device=false;
    bool recreate_requested=false;
#define FIELD(name) PFN_vk##name name=nullptr;void* target_##name=nullptr;
    HOOKED(FIELD)
#undef FIELD
};
Hooks& state(){static Hooks* value=new Hooks;return *value;}
thread_local unsigned unity_forwarding=0;
struct UnityForward {UnityForward(){++unity_forwarding;}~UnityForward(){--unity_forwarding;}};
void log(const char* message){auto callback=state().callbacks.log;if(callback)callback(message);}
template<class T>T field(uintptr_t p){T out{};auto& s=state();if(s.read&&p)s.read(p,&out,sizeof(out));return out;}
void publish(){auto& s=state();s.current.instance_proc=s.GetInstanceProcAddr;s.current.device_proc=s.GetDeviceProcAddr;if(s.overlay&&s.current.device&&s.current.queue)s.overlay->device(s.current);}
void unity_device(VkQueue queue){
    auto& s=state();std::lock_guard<std::recursive_mutex> lock(s.mutex);if(s.current.device&&s.current.queue==queue&&s.current.instance){publish();return;}
    if(!s.unity_ready||!s.graphics||!s.vulkan||s.graphics->GetRenderer()!=kUnityGfxRendererVulkan)return;
    const auto instance=s.vulkan->Instance();
    if(instance.instance&&instance.physicalDevice&&instance.device&&instance.graphicsQueue){if(s.current.device!=instance.device)s.recreate_requested=false;s.current={instance.instance,instance.physicalDevice,instance.device,instance.graphicsQueue,instance.queueFamilyIndex,s.GetInstanceProcAddr,s.GetDeviceProcAddr};publish();}
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL instance_proc(VkInstance,const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL device_proc(VkDevice,const char*);
VKAPI_ATTR VkResult VKAPI_CALL create_instance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* allocation,VkInstance* output){
    auto& s=state();VkResult result=s.CreateInstance(info,allocation,output);try{if(result==VK_SUCCESS&&output){std::lock_guard<std::recursive_mutex> lock(s.mutex);s.current.instance=*output;s.current.instance_proc=s.GetInstanceProcAddr;s.current.device_proc=s.GetDeviceProcAddr;}}catch(...){}return result;
}
VKAPI_ATTR VkResult VKAPI_CALL create_device(VkPhysicalDevice physical,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* allocation,VkDevice* output){
    auto& s=state();VkResult result=s.CreateDevice(physical,info,allocation,output);try{if(result==VK_SUCCESS&&output){std::lock_guard<std::recursive_mutex> lock(s.mutex);s.current.physical=physical;s.current.device=*output;s.current.queue=VK_NULL_HANDLE;s.current.instance_proc=s.GetInstanceProcAddr;s.current.device_proc=s.GetDeviceProcAddr;}}catch(...){}return result;
}
void queue_seen(VkDevice device,uint32_t family,VkQueue queue){
    auto& s=state();std::lock_guard<std::recursive_mutex> lock(s.mutex);
    if(s.current.device!=device||!queue||!s.current.instance||!s.current.physical||s.current.queue)return;
    auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(s.GetInstanceProcAddr(s.current.instance,"vkGetPhysicalDeviceQueueFamilyProperties"));if(!properties)return;
    uint32_t count=0;properties(s.current.physical,&count,nullptr);if(count>64||family>=count)return;
    VkQueueFamilyProperties families[64]{};properties(s.current.physical,&count,families);if(!(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT))return;
    s.current.queue=queue;s.current.family=family;publish();
}
VKAPI_ATTR void VKAPI_CALL get_queue(VkDevice device,uint32_t family,uint32_t index,VkQueue* output){auto& s=state();s.GetDeviceQueue(device,family,index,output);try{if(output)queue_seen(device,family,*output);}catch(...){}}
VKAPI_ATTR void VKAPI_CALL get_queue2(VkDevice device,const VkDeviceQueueInfo2* info,VkQueue* output){auto& s=state();s.GetDeviceQueue2(device,info,output);try{if(output&&info)queue_seen(device,info->queueFamilyIndex,*output);}catch(...){}}
VkResult create_swapchain_impl(VkDevice device,const VkSwapchainCreateInfoKHR* info,const VkAllocationCallbacks* allocation,VkSwapchainKHR* output,PFN_vkCreateSwapchainKHR original){
    auto& s=state();VkSwapchainCreateInfoKHR adjusted{};const VkSwapchainCreateInfoKHR* selected=info;
    try{if(info&&!(info->imageUsage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)){
        std::lock_guard<std::recursive_mutex> lock(s.mutex);
        auto caps=s.current.device==device&&s.GetInstanceProcAddr&&s.current.instance?reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(s.GetInstanceProcAddr(s.current.instance,"vkGetPhysicalDeviceSurfaceCapabilitiesKHR")):nullptr;
        VkSurfaceCapabilitiesKHR supported{};
        if(caps&&s.current.physical&&caps(s.current.physical,info->surface,&supported)==VK_SUCCESS&&(supported.supportedUsageFlags&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)){
            adjusted=*info;adjusted.imageUsage|=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;selected=&adjusted;
        }
    }}catch(...){selected=info;}
    VkResult result=original(device,selected,allocation,output);try{if(result==VK_SUCCESS&&selected&&output&&s.overlay){s.overlay->created(device,*output,*selected);log("Vulkan swapchain captured");}}catch(...){}return result;
}
void destroy_swapchain_impl(VkDevice device,VkSwapchainKHR swapchain,const VkAllocationCallbacks* allocation,PFN_vkDestroySwapchainKHR original){auto& s=state();try{if(s.overlay)s.overlay->destroyed(device,swapchain);}catch(...){}original(device,swapchain,allocation);}
VkResult present_impl(VkQueue queue,const VkPresentInfoKHR* info,PFN_vkQueuePresentKHR original){
    auto& s=state();bool recreate=false;
    try{
        unity_device(queue);
        std::lock_guard<std::recursive_mutex> lock(s.mutex);
        // A library injected after startup cannot query the old swapchain's
        // format/extent. Ask Unity to recreate it once, using the normal WSI
        // out-of-date path. Never guess VkImage metadata or loop on this error.
        recreate=s.bound_create&&s.bound_present&&!s.recreate_requested&&
            s.current.device&&s.current.queue==queue&&s.overlay&&info&&
            info->swapchainCount==1&&info->pSwapchains&&info->pImageIndices&&
            !s.overlay->knows(s.current.device,info->pSwapchains[0]);
    }catch(...){}
    // Always perform the real presentation first: it consumes the original
    // wait semaphores, so a successful frame is never left pending.
    VkResult result;
    try{result=s.overlay?s.overlay->present(queue,info,original):original(queue,info);}
    catch(...){result=original(queue,info);}
    if(recreate&&(result==VK_SUCCESS||result==VK_SUBOPTIMAL_KHR)){
        bool request=false;
        try{std::lock_guard<std::recursive_mutex> lock(s.mutex);if(!s.recreate_requested){s.recreate_requested=true;request=true;}}catch(...){}
        if(request){if(info->pResults)info->pResults[0]=VK_ERROR_OUT_OF_DATE_KHR;log("Vulkan: requesting one swapchain rebuild after late attachment");return VK_ERROR_OUT_OF_DATE_KHR;}
    }
    return result;
}
void destroy_device_impl(VkDevice device,const VkAllocationCallbacks* allocation,PFN_vkDestroyDevice original){
    auto& s=state();try{if(s.overlay)s.overlay->device_destroyed(device);}catch(...){}original(device,allocation);try{std::lock_guard<std::recursive_mutex> lock(s.mutex);if(s.current.device==device){s.current.device=VK_NULL_HANDLE;s.current.physical=VK_NULL_HANDLE;s.current.queue=VK_NULL_HANDLE;s.recreate_requested=false;}}catch(...){}
}
// Loader wrappers are also reachable through Unity's previous function pointer.
// Skip a second overlay pass in that case, but still call the real driver once.
VKAPI_ATTR VkResult VKAPI_CALL create_swapchain(VkDevice d,const VkSwapchainCreateInfoKHR* i,const VkAllocationCallbacks* a,VkSwapchainKHR* o){auto p=state().CreateSwapchainKHR;return unity_forwarding?p(d,i,a,o):create_swapchain_impl(d,i,a,o,p);}
VKAPI_ATTR void VKAPI_CALL destroy_swapchain(VkDevice d,VkSwapchainKHR s,const VkAllocationCallbacks* a){auto p=state().DestroySwapchainKHR;if(unity_forwarding)p(d,s,a);else destroy_swapchain_impl(d,s,a,p);}
VKAPI_ATTR VkResult VKAPI_CALL present(VkQueue q,const VkPresentInfoKHR* i){auto p=state().QueuePresentKHR;return unity_forwarding?p(q,i):present_impl(q,i,p);}
VKAPI_ATTR void VKAPI_CALL destroy_device(VkDevice d,const VkAllocationCallbacks* a){auto p=state().DestroyDevice;if(unity_forwarding)p(d,a);else destroy_device_impl(d,a,p);}
VKAPI_ATTR VkResult VKAPI_CALL unity_create_swapchain(VkDevice d,const VkSwapchainCreateInfoKHR* i,const VkAllocationCallbacks* a,VkSwapchainKHR* o){UnityForward scope;return create_swapchain_impl(d,i,a,o,state().unity_create.load(std::memory_order_acquire));}
VKAPI_ATTR void VKAPI_CALL unity_destroy_swapchain(VkDevice d,VkSwapchainKHR s,const VkAllocationCallbacks* a){UnityForward scope;destroy_swapchain_impl(d,s,a,state().unity_destroy.load(std::memory_order_acquire));}
VKAPI_ATTR VkResult VKAPI_CALL unity_present(VkQueue q,const VkPresentInfoKHR* i){UnityForward scope;return present_impl(q,i,state().unity_present.load(std::memory_order_acquire));}
VKAPI_ATTR void VKAPI_CALL unity_destroy_device(VkDevice d,const VkAllocationCallbacks* a){UnityForward scope;destroy_device_impl(d,a,state().unity_destroy_device.load(std::memory_order_acquire));}
bool attach_unity(){
    auto& s=state();
    if(!s.unity_ready||!s.graphics||!s.vulkan||!s.vulkan->InterceptVulkanAPI||
       !s.CreateSwapchainKHR||!s.DestroySwapchainKHR||!s.QueuePresentKHR||!s.DestroyDevice||
       s.graphics->GetRenderer()!=kUnityGfxRendererVulkan)return false;
    if(s.bound_create&&s.bound_destroy&&s.bound_present&&s.bound_device)return true;
    const auto instance=s.vulkan->Instance();
    if(!instance.instance||!instance.physicalDevice||!instance.device||!instance.graphicsQueue)return false;
    if(s.current.device!=instance.device)s.recreate_requested=false;
    s.current={instance.instance,instance.physicalDevice,instance.device,instance.graphicsQueue,instance.queueFamilyIndex,s.GetInstanceProcAddr,s.GetDeviceProcAddr};publish();
    // Publish a valid fallback before changing the table: another Unity thread
    // may enter the wrapper as soon as InterceptVulkanAPI writes its pointer.
#define BIND(api,field,bound,wrapper) \
    if(!s.bound){s.field.store(s.api,std::memory_order_release); \
        auto previous=s.vulkan->InterceptVulkanAPI("vk" #api,reinterpret_cast<PFN_vkVoidFunction>(wrapper)); \
        if(previous){if(previous!=reinterpret_cast<PFN_vkVoidFunction>(wrapper))s.field.store(reinterpret_cast<PFN_vk##api>(previous),std::memory_order_release);s.bound=true;}}
    BIND(CreateSwapchainKHR,unity_create,bound_create,unity_create_swapchain);
    BIND(DestroySwapchainKHR,unity_destroy,bound_destroy,unity_destroy_swapchain);
    BIND(QueuePresentKHR,unity_present,bound_present,unity_present);
    BIND(DestroyDevice,unity_destroy_device,bound_device,unity_destroy_device);
#undef BIND
    const bool ready=s.bound_create&&s.bound_destroy&&s.bound_present&&s.bound_device;
    if(ready)log("Unity Vulkan cached entry points attached");
    return ready;
}
PFN_vkVoidFunction replacement(const char* name,PFN_vkVoidFunction real){
    if(!real||!name)return real;auto& s=state();
#define MAP(api,wrapper) if(!std::strcmp(name,"vk" #api)&&s.api)return reinterpret_cast<PFN_vkVoidFunction>(wrapper);
    MAP(GetInstanceProcAddr,instance_proc);MAP(GetDeviceProcAddr,device_proc);MAP(CreateInstance,create_instance);MAP(CreateDevice,create_device);MAP(GetDeviceQueue,get_queue);MAP(GetDeviceQueue2,get_queue2);MAP(CreateSwapchainKHR,create_swapchain);MAP(DestroySwapchainKHR,destroy_swapchain);MAP(QueuePresentKHR,present);MAP(DestroyDevice,destroy_device);
#undef MAP
    return real;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL instance_proc(VkInstance instance,const char* name){auto& s=state();return replacement(name,s.GetInstanceProcAddr(instance,name));}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL device_proc(VkDevice device,const char* name){auto& s=state();return replacement(name,s.GetDeviceProcAddr(device,name));}
}
bool install(Hook hook,Read read,Callbacks callbacks){
    auto& s=state();std::lock_guard<std::recursive_mutex> lock(s.mutex);s.hook=hook;s.read=read;s.callbacks=callbacks;if(!s.overlay)s.overlay=new Overlay(callbacks);
    if(!s.library)s.library=dlopen("libvulkan.so",RTLD_NOW|RTLD_LOCAL);if(!s.library)return false;
    bool ok=true;
#define INSTALL(api,wrapper,required) if(!s.api){s.target_##api=dlsym(s.library,"vk" #api);if(s.target_##api){if(hook(s.target_##api,reinterpret_cast<void*>(wrapper),reinterpret_cast<void**>(&s.api))!=0)s.api=nullptr;}if(required&&!s.api)ok=false;}
    // Resolve ordinary exports first, proc-address wrappers last. All calls
    // forward via Dobby's original trampolines, never back to patched entries.
    INSTALL(CreateInstance,create_instance,true);INSTALL(CreateDevice,create_device,true);
    INSTALL(GetDeviceQueue,get_queue,true);INSTALL(GetDeviceQueue2,get_queue2,false);
    INSTALL(CreateSwapchainKHR,create_swapchain,true);INSTALL(DestroySwapchainKHR,destroy_swapchain,true);
    INSTALL(QueuePresentKHR,present,true);INSTALL(DestroyDevice,destroy_device,true);
    INSTALL(GetDeviceProcAddr,device_proc,true);INSTALL(GetInstanceProcAddr,instance_proc,true);
#undef INSTALL
    return ok;
}
void poll_unity(){
    auto& s=state();std::lock_guard<std::recursive_mutex> lock(s.mutex);if(s.unity_ready){attach_unity();return;}if(!s.read)return;
    // Exact supplied Unity 6000.3.18f1 ARM64 image. The getter is the same one
    // passed by Unity to native UnityPluginLoad callbacks, not a heap scan.
    static constexpr unsigned char build[]={0xf3,0x0d,0xff,0xb7,0x4b,0xa1,0x8b,0x4b};
    static constexpr unsigned char getter[]={0x40,0x7f,0x00,0xd0,0x00,0x60,0x06,0x91,0xc0,0x03,0x5f,0xd6};
    const mango_runtime::Resolver::Signature signature{0x8722d0,getter,sizeof(getter)};
    if(!s.unity.select_named_image("libunity.so",build,sizeof(build),&signature,1))return;
    auto* interfaces=reinterpret_cast<IUnityInterfaces*(*)()>(s.unity.base()+0x8722d0)();
    auto get=field<decltype(IUnityInterfaces::GetInterface)>(reinterpret_cast<uintptr_t>(interfaces));if(!get||!s.unity.executable(reinterpret_cast<uintptr_t>(get)))return;
    s.graphics=static_cast<IUnityGraphics*>(get(GetUnityInterfaceGUID<IUnityGraphics>()));s.vulkan=static_cast<IUnityGraphicsVulkan*>(get(GetUnityInterfaceGUID<IUnityGraphicsVulkan>()));
    if(!s.vulkan)s.vulkan=reinterpret_cast<IUnityGraphicsVulkan*>(get(GetUnityInterfaceGUID<IUnityGraphicsVulkanV2>())); // Shared prefix through Instance().
    if(!s.graphics||!s.vulkan)return;
    auto renderer=field<decltype(IUnityGraphics::GetRenderer)>(reinterpret_cast<uintptr_t>(s.graphics));auto instance=field<decltype(IUnityGraphicsVulkan::Instance)>(reinterpret_cast<uintptr_t>(s.vulkan)+3*sizeof(void*));auto intercept=field<decltype(IUnityGraphicsVulkan::InterceptVulkanAPI)>(reinterpret_cast<uintptr_t>(s.vulkan)+sizeof(void*));
    s.unity_ready=renderer&&instance&&intercept&&s.unity.executable(reinterpret_cast<uintptr_t>(renderer))&&s.unity.executable(reinterpret_cast<uintptr_t>(instance))&&s.unity.executable(reinterpret_cast<uintptr_t>(intercept));
    if(s.unity_ready)attach_unity();
}
#ifdef MANGO_VULKAN_TESTS
bool test_attach_unity(IUnityGraphics* graphics,IUnityGraphicsVulkan* vulkan){auto& s=state();std::lock_guard<std::recursive_mutex> lock(s.mutex);s.graphics=graphics;s.vulkan=vulkan;s.unity_ready=true;return attach_unity();}
#endif
}
