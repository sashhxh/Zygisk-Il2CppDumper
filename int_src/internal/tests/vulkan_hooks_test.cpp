#include "vulkan_hooks.h"
#include "unity/IUnityGraphics.h"
#include "unity/IUnityGraphicsVulkan.h"
#include <cassert>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <cstdio>
#include <string>
namespace {
void* library=nullptr;std::map<void*,void*> wrappers;std::map<void*,int> attempts;bool fail_once=true;unsigned observed=0;
int hook(void* target,void* replacement,void** original){++attempts[target];if(target==dlsym(library,"vkGetInstanceProcAddr")&&fail_once){fail_once=false;return -1;}assert(!wrappers.count(target));wrappers[target]=replacement;*original=target;return 0;}
template<class T>T wrapped(const char* name){return reinterpret_cast<T>(wrappers.at(dlsym(library,name)));}
template<class T>T fixture(const char* name){return reinterpret_cast<T>(dlsym(library,name));}
std::map<std::string,PFN_vkVoidFunction> unity_dispatch;
bool unity_running=false,fail_intercept=true;unsigned intercepts=0,rebuilds=0;
UnityGfxRenderer UNITY_INTERFACE_API renderer(){return unity_running?kUnityGfxRendererVulkan:kUnityGfxRendererNull;}
UnityVulkanInstance UNITY_INTERFACE_API unity_instance(){UnityVulkanInstance i{};if(unity_running){i.instance=(VkInstance)0x101;i.physicalDevice=(VkPhysicalDevice)0x111;i.device=(VkDevice)0x202;i.graphicsQueue=(VkQueue)0x303;}return i;}
PFN_vkVoidFunction UNITY_INTERFACE_API intercept(const char* name,PFN_vkVoidFunction next){
 ++intercepts;if(!std::strcmp(name,"vkDestroySwapchainKHR")&&fail_intercept){fail_intercept=false;return nullptr;}
 auto& current=unity_dispatch.at(name);auto previous=current;current=next;return previous;
}
template<class T>T engine(const char* name){return reinterpret_cast<T>(unity_dispatch.at(name));}
}
int main(){
 library=dlopen("libvulkan.so",RTLD_NOW|RTLD_LOCAL);assert(library);mango_vk::Callbacks c;c.presented=[](uint32_t w,uint32_t h){assert((w==384&&h==256)||(!w&&!h));++observed;};c.log=[](const char* text){if(std::strstr(text,"requesting one"))++rebuilds;};
 assert(!mango_vk::install(hook,nullptr,c));assert(mango_vk::install(hook,nullptr,c));assert(mango_vk::install(hook,nullptr,c));
 for(auto [address,count]:attempts)assert(count==(address==dlsym(library,"vkGetInstanceProcAddr")?2:1));
 auto gipa=wrapped<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");auto gdpa=wrapped<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
 assert(gipa(nullptr,"unsupported")==nullptr);assert(gdpa(nullptr,"unsupported")==nullptr);
 assert(gipa(nullptr,"vkCreateSwapchainKHR")==reinterpret_cast<PFN_vkVoidFunction>(wrapped<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")));
 assert(gdpa(nullptr,"vkQueuePresentKHR")==reinterpret_cast<PFN_vkVoidFunction>(wrapped<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")));
 VkInstance instance{};assert(wrapped<PFN_vkCreateInstance>("vkCreateInstance")(nullptr,nullptr,&instance)==VK_SUCCESS);
 VkDevice device{};assert(wrapped<PFN_vkCreateDevice>("vkCreateDevice")((VkPhysicalDevice)0x111,nullptr,nullptr,&device)==VK_SUCCESS);
 VkQueue queue{};wrapped<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(device,0,0,&queue);assert(queue);
 VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};info.imageFormat=VK_FORMAT_R8G8B8A8_UNORM;info.imageExtent={384,256};info.imageArrayLayers=1;info.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;info.minImageCount=2;info.preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
 VkSwapchainKHR swap{};assert(wrapped<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(device,&info,nullptr,&swap)==VK_SUCCESS);
 assert(info.imageUsage==VK_IMAGE_USAGE_TRANSFER_DST_BIT);assert(reinterpret_cast<VkImageUsageFlags(*)()>(dlsym(library,"fixture_usage"))()==(VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
 unsigned image=0;VkPresentInfoKHR p{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};p.swapchainCount=1;p.pSwapchains=&swap;p.pImageIndices=&image;
 // Missing optional loader functions must leave the game's present untouched.
 assert(wrapped<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")(queue,&p)==VK_SUBOPTIMAL_KHR);assert(observed==1);
 assert(reinterpret_cast<const VkPresentInfoKHR*(*)()>(dlsym(library,"fixture_last_present"))()==&p);
 wrapped<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR")(device,swap,nullptr);wrapped<PFN_vkDestroyDevice>("vkDestroyDevice")(device,nullptr);
 assert(reinterpret_cast<int(*)()>(dlsym(library,"fixture_creates"))()==1);assert(reinterpret_cast<int(*)()>(dlsym(library,"fixture_destroys"))()==1);
 // Simulate dlopen AFTER Unity has cached loader/driver pointers and created a
 // swapchain. Loader exports won't see those calls without runtime attachment.
 // Previous functions may themselves enter our lower loader hooks.
 unity_dispatch["vkCreateSwapchainKHR"]=reinterpret_cast<PFN_vkVoidFunction>(wrapped<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR"));
 unity_dispatch["vkDestroySwapchainKHR"]=reinterpret_cast<PFN_vkVoidFunction>(wrapped<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR"));
 unity_dispatch["vkQueuePresentKHR"]=reinterpret_cast<PFN_vkVoidFunction>(wrapped<PFN_vkQueuePresentKHR>("vkQueuePresentKHR"));
 unity_dispatch["vkDestroyDevice"]=reinterpret_cast<PFN_vkVoidFunction>(wrapped<PFN_vkDestroyDevice>("vkDestroyDevice"));
 IUnityGraphics graphics{};graphics.GetRenderer=renderer;IUnityGraphicsVulkan vulkan{};vulkan.Instance=unity_instance;vulkan.InterceptVulkanAPI=intercept;
 assert(!mango_vk::test_attach_unity(&graphics,&vulkan));assert(intercepts==0);
 unity_running=true;assert(!mango_vk::test_attach_unity(&graphics,&vulkan));assert(intercepts==4);
 assert(mango_vk::test_attach_unity(&graphics,&vulkan));assert(intercepts==5);
 assert(mango_vk::test_attach_unity(&graphics,&vulkan));assert(intercepts==5);
 VkResult individual=VK_SUCCESS;p.pResults=&individual;
 auto present_game=engine<PFN_vkQueuePresentKHR>("vkQueuePresentKHR");
 // Real failures are never replaced and do not spend the one rebuild request.
 fixture<void(*)(VkResult)>("fixture_present_result")(VK_ERROR_DEVICE_LOST);
 assert(present_game(queue,&p)==VK_ERROR_DEVICE_LOST);assert(rebuilds==0);
 fixture<void(*)(VkResult)>("fixture_present_result")(VK_SUCCESS);
 unsigned before=observed;int calls=fixture<int(*)()>("fixture_presents")();
 assert(present_game(queue,&p)==VK_ERROR_OUT_OF_DATE_KHR);assert(individual==VK_ERROR_OUT_OF_DATE_KHR);assert(rebuilds==1);
 assert(observed==before+1&&fixture<int(*)()>("fixture_presents")()==calls+1);
 // Even if Unity delays recreation, don't cause a rebuild loop or double-present.
 assert(present_game(queue,&p)==VK_SUCCESS);assert(individual==VK_SUCCESS);assert(rebuilds==1);
 assert(observed==before+2&&fixture<int(*)()>("fixture_presents")()==calls+2);
 engine<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR")(device,swap,nullptr);
 assert(engine<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(device,&info,nullptr,&swap)==VK_SUCCESS);
 before=observed;calls=fixture<int(*)()>("fixture_presents")();
 assert(present_game(queue,&p)==VK_SUCCESS);assert(individual==VK_SUCCESS);assert(rebuilds==1);
 assert(observed==before+1&&fixture<int(*)()>("fixture_presents")()==calls+1);
 engine<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR")(device,swap,nullptr);
 engine<PFN_vkDestroyDevice>("vkDestroyDevice")(device,nullptr);
 assert(fixture<int(*)()>("fixture_creates")()==2&&fixture<int(*)()>("fixture_destroys")()==3&&fixture<int(*)()>("fixture_device_destroys")()==2);
 std::puts("PASS: Vulkan hook retries, proc-address routing, early capture; late cached Unity dispatch, one rebuild, no recursion/double-present, failure forwarding");
}
