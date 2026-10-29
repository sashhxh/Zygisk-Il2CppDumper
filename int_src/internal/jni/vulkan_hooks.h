#pragma once
#include "vulkan_overlay.h"
#ifdef MANGO_VULKAN_TESTS
struct IUnityGraphics;
struct IUnityGraphicsVulkan;
#endif
namespace mango_vk {
using Hook=int(*)(void*,void*,void**);
using Read=bool(*)(uintptr_t,void*,size_t);
bool install(Hook hook,Read read,Callbacks callbacks);
void poll_unity();
#ifdef MANGO_VULKAN_TESTS
bool test_attach_unity(IUnityGraphics* graphics,IUnityGraphicsVulkan* vulkan);
#endif
}
