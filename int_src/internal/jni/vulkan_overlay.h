#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include "imgui.h"

namespace mango_vk {
struct Device {
    VkInstance instance=VK_NULL_HANDLE;
    VkPhysicalDevice physical=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkQueue queue=VK_NULL_HANDLE;
    uint32_t family=0;
    PFN_vkGetInstanceProcAddr instance_proc=nullptr;
    PFN_vkGetDeviceProcAddr device_proc=nullptr;
};
struct Callbacks {
    void(*initialize_ui)(uint32_t,uint32_t)=nullptr;
    ImDrawData*(*draw_ui)(uint32_t,uint32_t)=nullptr;
    void(*presented)(uint32_t,uint32_t)=nullptr;
    void(*log)(const char*)=nullptr;
};
// Own command buffers, render pass, fences and per-swapchain-image semaphores.
// Never record into Unity's command buffers or change its rendering state.
class Overlay {
    struct State;State* state_;
public:
    explicit Overlay(Callbacks callbacks);
    ~Overlay();
    void device(const Device& device);
    void created(VkDevice device,VkSwapchainKHR swapchain,const VkSwapchainCreateInfoKHR& info);
    bool knows(VkDevice device,VkSwapchainKHR swapchain);
    void destroyed(VkDevice device,VkSwapchainKHR swapchain);
    void device_destroyed(VkDevice device);
    VkResult present(VkQueue queue,const VkPresentInfoKHR* info,PFN_vkQueuePresentKHR original);
    // Also used by the offscreen Vulkan regression fixture; production images
    // are queried using vkGetSwapchainImagesKHR and always use PRESENT layout.
    void test_images(VkSwapchainKHR swapchain,const VkImage* images,uint32_t count,VkImageLayout boundary_layout);
};
void transform_draw_data(ImDrawData* data,VkExtent2D image_extent,VkSurfaceTransformFlagBitsKHR transform);
bool rotated(VkSurfaceTransformFlagBitsKHR transform);
}
