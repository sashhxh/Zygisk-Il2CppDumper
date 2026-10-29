#include <vulkan/vulkan.h>
#include <cstring>
namespace {const VkPresentInfoKHR* last=nullptr;int creates=0,destroys=0,presents=0,device_destroys=0;VkImageUsageFlags usage=0;VkResult present_result=VK_SUBOPTIMAL_KHR;}
extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo*,const VkAllocationCallbacks*,VkInstance* out){*out=(VkInstance)0x101;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice,const VkDeviceCreateInfo*,const VkAllocationCallbacks*,VkDevice* out){*out=(VkDevice)0x202;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(VkDevice,uint32_t,uint32_t,VkQueue* out){*out=(VkQueue)0x303;}
VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue2(VkDevice,const VkDeviceQueueInfo2*,VkQueue* out){*out=(VkQueue)0x303;}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice,uint32_t* count,VkQueueFamilyProperties* p){if(p){p[0]={};p[0].queueCount=1;p[0].queueFlags=VK_QUEUE_GRAPHICS_BIT;}*count=1;}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice,const VkSwapchainCreateInfoKHR* info,const VkAllocationCallbacks*,VkSwapchainKHR* out){++creates;usage=info->imageUsage;*out=(VkSwapchainKHR)0x404;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice,VkSurfaceKHR,VkSurfaceCapabilitiesKHR* out){*out={};out->supportedUsageFlags=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice,VkSwapchainKHR,const VkAllocationCallbacks*){++destroys;}
VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue,const VkPresentInfoKHR* p){++presents;last=p;if(p&&p->pResults)for(uint32_t i=0;i<p->swapchainCount;++i)p->pResults[i]=present_result;return present_result;}
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice,const VkAllocationCallbacks*){++device_destroys;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device,const char* name){return vkGetInstanceProcAddr((VkInstance)device,name);}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance,const char* name){
#define EXPORT(name_) if(!std::strcmp(name,"vk" #name_))return reinterpret_cast<PFN_vkVoidFunction>(vk##name_);
 EXPORT(GetInstanceProcAddr);EXPORT(GetDeviceProcAddr);EXPORT(CreateInstance);EXPORT(CreateDevice);EXPORT(GetDeviceQueue);EXPORT(GetDeviceQueue2);EXPORT(GetPhysicalDeviceQueueFamilyProperties);EXPORT(GetPhysicalDeviceSurfaceCapabilitiesKHR);EXPORT(CreateSwapchainKHR);EXPORT(DestroySwapchainKHR);EXPORT(QueuePresentKHR);EXPORT(DestroyDevice);
#undef EXPORT
 return nullptr;
}
const VkPresentInfoKHR* fixture_last_present(){return last;}
int fixture_creates(){return creates;}int fixture_destroys(){return destroys;}
int fixture_presents(){return presents;}int fixture_device_destroys(){return device_destroys;}
void fixture_present_result(VkResult result){present_result=result;}
VkImageUsageFlags fixture_usage(){return usage;}
}
