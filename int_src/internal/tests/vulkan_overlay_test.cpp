#undef VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "vulkan_overlay.h"
#include "ui_coordinates.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef REFERENCE_UI_FIXTURE
#include "settings.h"
#include "reference_menu.h"
mango_ui::State reference_state;
mango_config::Settings reference_settings;
#endif

namespace {
int errors=0,draws=0,presents=0,clicks=0;unsigned input_phase=0;bool reject=false;int color_rect=-1;
VKAPI_ATTR VkBool32 VKAPI_CALL debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void*){
    if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){++errors;std::fprintf(stderr,"VALIDATION: %s\n",data->pMessage);}return VK_FALSE;
}
void ok(VkResult r){assert(r==VK_SUCCESS);}
struct Target {VkImage image{};VkDeviceMemory memory{};VkBuffer buffer{};VkDeviceMemory host{};void* mapped=nullptr;VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};VkSemaphore game{};bool first=true;};
VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family=0;
VkExtent2D extent{384,256};std::vector<Target> targets;
uint32_t memory(uint32_t bits,VkMemoryPropertyFlags flags){VkPhysicalDeviceMemoryProperties p;vkGetPhysicalDeviceMemoryProperties(physical,&p);for(uint32_t i=0;i<p.memoryTypeCount;++i)if((bits&(1u<<i))&&(p.memoryTypes[i].propertyFlags&flags)==flags)return i;assert(false);return 0;}
void allocate(VkMemoryRequirements r,VkMemoryPropertyFlags flags,VkDeviceMemory& out){VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=r.size;a.memoryTypeIndex=memory(r.memoryTypeBits,flags);ok(vkAllocateMemory(device,&a,nullptr,&out));}
void begin(Target& t){ok(vkResetCommandPool(device,t.pool,0));VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};ok(vkBeginCommandBuffer(t.command,&b));}
void barrier(VkCommandBuffer command,VkImage image,VkImageLayout old,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags source,VkPipelineStageFlags destination){VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=old;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;b.srcAccessMask=src;b.dstAccessMask=dst;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(command,source,destination,0,0,nullptr,0,nullptr,1,&b);}
void game_frame(unsigned index){
    auto& t=targets[index];ok(vkWaitForFences(device,1,&t.fence,VK_TRUE,UINT64_MAX));begin(t);
    barrier(t.command,t.image,t.first?VK_IMAGE_LAYOUT_UNDEFINED:VK_IMAGE_LAYOUT_GENERAL,t.first?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkClearColorValue color{{.2f,.3f,.4f,1.f}};VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdClearColorImage(t.command,t.image,VK_IMAGE_LAYOUT_GENERAL,&color,1,&range);
    barrier(t.command,t.image,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_MEMORY_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    ok(vkEndCommandBuffer(t.command));VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&t.command;s.signalSemaphoreCount=1;s.pSignalSemaphores=&t.game;ok(vkQueueSubmit(queue,1,&s,VK_NULL_HANDLE));ok(vkQueueWaitIdle(queue));t.first=false;
}
VKAPI_ATTR VkResult VKAPI_CALL fake_present(VkQueue q,const VkPresentInfoKHR* p){
    assert(q==queue&&p&&p->swapchainCount==1);++presents;if(reject)return VK_ERROR_OUT_OF_DATE_KHR;
    auto& t=targets.at(p->pImageIndices[0]);begin(t);
    barrier(t.command,t.image,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={extent.width,extent.height,1};vkCmdCopyImageToBuffer(t.command,t.image,VK_IMAGE_LAYOUT_GENERAL,t.buffer,1,&copy);
    VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=t.buffer;host.size=VK_WHOLE_SIZE;vkCmdPipelineBarrier(t.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
    ok(vkEndCommandBuffer(t.command));VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.waitSemaphoreCount=p->waitSemaphoreCount;s.pWaitSemaphores=p->pWaitSemaphores;s.pWaitDstStageMask=&stage;s.commandBufferCount=1;s.pCommandBuffers=&t.command;ok(vkResetFences(device,1,&t.fence));ok(vkQueueSubmit(q,1,&s,t.fence));ok(vkWaitForFences(device,1,&t.fence,VK_TRUE,UINT64_MAX));return VK_SUCCESS;
}
void init_ui(uint32_t,uint32_t){if(!ImGui::GetCurrentContext()){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
    #ifdef REFERENCE_UI_FIXTURE
    mango_ui::ApplyStyle(1);mango_ui::LoadFont(io,1);
    color_rect=io.Fonts->AddCustomRect(8,8);mango_ui::BakeAssets(io);
    #else
    io.Fonts->AddFontDefault();color_rect=io.Fonts->AddCustomRectRegular(8,8);
    #endif
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ImFontAtlasRect rect;assert(io.Fonts->GetCustomRect(color_rect,&rect));
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){auto* pixel=pixels+((rect.y+y)*w+rect.x+x)*4;pixel[0]=128;pixel[1]=64;pixel[2]=32;pixel[3]=255;}
}}
ImDrawData* draw_ui(uint32_t w,uint32_t h){
    ++draws;auto& io=ImGui::GetIO();io.DisplaySize={float(w),float(h)};io.DeltaTime=1.f/60;
    // Alternate Android window touches and Unity bottom-left touches at twice
    // the UI resolution. Both must hit the button at the displayed location.
    mango_input::Point touch;
    if(input_phase/4%2)assert(mango_input::to_ui(200,(float(h)-24)*2,w*2,h*2,w,h,mango_input::Origin::BottomLeft,touch));
    else assert(mango_input::to_ui(100,24,w,h,w,h,mango_input::Origin::TopLeft,touch));
    io.AddMousePosEvent(touch.x,touch.y);io.AddMouseButtonEvent(0,input_phase%4==1);
    #ifdef REFERENCE_UI_FIXTURE
    reference_state.scale=mango_ui::FitScale(w,h);
    ImGui::GetStyle().FontScaleMain=2.2f*reference_state.scale;
    #endif
    ImGui::NewFrame();
    #ifdef REFERENCE_UI_FIXTURE
    reference_state.active_tab=(draws/10)%6;
    reference_state.menu_motion.phase=float(draws%24)/24;
    reference_settings.reference.dark=draws%40>20;
    reference_settings.reference.fps=draws%30>15;
    ImGui::BeginDisabled();mango_ui::Draw(reference_settings,reference_state,w,h,draws%18);ImGui::EndDisabled();
    #endif
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled({8,8},{64,64},IM_COL32(128,32,64,255));
    draw->AddRectFilled({10,10},{22,22},IM_COL32(255,0,0,255));
    draw->AddRectFilled({10,40},{22,52},IM_COL32(0,255,0,255));
    draw->AddRectFilled({40,10},{52,22},IM_COL32(0,0,255,255));
    ImVec2 uv0,uv1;io.Fonts->CalcCustomRectUV(io.Fonts->GetCustomRectByIndex(color_rect),&uv0,&uv1);
    draw->AddImage(io.Fonts->TexID,{220,16},{252,48},uv0,uv1);
    ImGui::SetNextWindowPos({80,8});ImGui::SetNextWindowSize({120,48});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("touch",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    if(ImGui::Button("Click",{120,48}))++clicks;
    ImGui::End();ImGui::PopStyleVar();ImGui::Render();return ImGui::GetDrawData();
}
std::array<unsigned,4> pixel(unsigned image,unsigned x,unsigned y){auto* p=static_cast<unsigned char*>(targets[image].mapped)+(y*extent.width+x)*4;return {p[0],p[1],p[2],p[3]};}
std::array<unsigned,4> displayed_pixel(unsigned image,unsigned x,unsigned y,unsigned transform){
    // Independent presentation oracle: image origin and basis vectors for each
    // specified clockwise pre-rotation (mirror first). Do not reuse production
    // transform_draw_data, or an inverted renderer would validate itself.
    static constexpr float basis[8][6]={
        {0,0,1,0,0,1},{1,0,0,1,-1,0},{1,1,-1,0,0,-1},{0,1,0,-1,1,0},
        {1,0,-1,0,0,1},{1,1,0,-1,-1,0},{0,1,1,0,0,-1},{0,0,0,1,1,0}};
    unsigned index=0;while((1u<<index)!=transform){++index;assert(index<8);}
    const bool quarter=transform==2||transform==8||transform==32||transform==128;
    float u=(float(x)+.5f)/(quarter?extent.height:extent.width),v=(float(y)+.5f)/(quarter?extent.width:extent.height);
    const auto& b=basis[index];unsigned ix=unsigned((b[0]+b[2]*u+b[4]*v)*extent.width),iy=unsigned((b[1]+b[3]*u+b[5]*v)*extent.height);
    assert(ix<extent.width&&iy<extent.height);return pixel(image,ix,iy);
}
void create_targets(VkFormat format){
    targets.resize(3);for(auto& t:targets){
        VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=format;i.extent={extent.width,extent.height,1};i.mipLevels=i.arrayLayers=1;i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;ok(vkCreateImage(device,&i,nullptr,&t.image));VkMemoryRequirements r;vkGetImageMemoryRequirements(device,t.image,&r);allocate(r,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,t.memory);ok(vkBindImageMemory(device,t.image,t.memory,0));
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=extent.width*extent.height*4;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;ok(vkCreateBuffer(device,&b,nullptr,&t.buffer));vkGetBufferMemoryRequirements(device,t.buffer,&r);allocate(r,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,t.host);ok(vkBindBufferMemory(device,t.buffer,t.host,0));ok(vkMapMemory(device,t.host,0,VK_WHOLE_SIZE,0,&t.mapped));
        VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=family;ok(vkCreateCommandPool(device,&pool,nullptr,&t.pool));VkCommandBufferAllocateInfo c{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};c.commandPool=t.pool;c.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;c.commandBufferCount=1;ok(vkAllocateCommandBuffers(device,&c,&t.command));VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};f.flags=VK_FENCE_CREATE_SIGNALED_BIT;ok(vkCreateFence(device,&f,nullptr,&t.fence));VkSemaphoreCreateInfo s{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};ok(vkCreateSemaphore(device,&s,nullptr,&t.game));
    }
}
void destroy_targets(){ok(vkQueueWaitIdle(queue));for(auto& t:targets){vkDestroySemaphore(device,t.game,nullptr);vkDestroyFence(device,t.fence,nullptr);vkDestroyCommandPool(device,t.pool,nullptr);vkUnmapMemory(device,t.host);vkDestroyBuffer(device,t.buffer,nullptr);vkFreeMemory(device,t.host,nullptr);vkDestroyImage(device,t.image,nullptr);vkFreeMemory(device,t.memory,nullptr);}targets.clear();}
VKAPI_ATTR VkResult VKAPI_CALL fail_shader(VkDevice,const VkShaderModuleCreateInfo*,const VkAllocationCallbacks*,VkShaderModule*){return VK_ERROR_OUT_OF_HOST_MEMORY;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL fail_shader_proc(VkDevice d,const char* name){if(!std::strcmp(name,"vkCreateShaderModule"))return reinterpret_cast<PFN_vkVoidFunction>(fail_shader);return vkGetDeviceProcAddr(d,name);}
}
int main(){
    const char* layers[]={"VK_LAYER_KHRONOS_validation"};const char* extensions[]={VK_EXT_DEBUG_UTILS_EXTENSION_NAME,VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,VK_KHR_SURFACE_EXTENSION_NAME};VkValidationFeatureEnableEXT sync=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};features.enabledValidationFeatureCount=1;features.pEnabledValidationFeatures=&sync;
    VkDebugUtilsMessengerCreateInfoEXT messages{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};messages.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;messages.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;messages.pfnUserCallback=debug;features.pNext=&messages;
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};application.apiVersion=VK_API_VERSION_1_1;VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pNext=&features;info.pApplicationInfo=&application;info.enabledLayerCount=1;info.ppEnabledLayerNames=layers;info.enabledExtensionCount=3;info.ppEnabledExtensionNames=extensions;ok(vkCreateInstance(&info,nullptr,&instance));
    auto create_debug=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");VkDebugUtilsMessengerEXT messenger;ok(create_debug(instance,&messages,nullptr,&messenger));
    uint32_t count=1;ok(vkEnumeratePhysicalDevices(instance,&count,&physical));assert(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);std::vector<VkQueueFamilyProperties> queues(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,queues.data());for(;family<count;++family)if(queues[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)break;assert(family<count);
    float priority=1;VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};q.queueFamilyIndex=family;q.queueCount=1;q.pQueuePriorities=&priority;const char* swapchain_ext=VK_KHR_SWAPCHAIN_EXTENSION_NAME;VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};d.queueCreateInfoCount=1;d.pQueueCreateInfos=&q;d.enabledExtensionCount=1;d.ppEnabledExtensionNames=&swapchain_ext;ok(vkCreateDevice(physical,&d,nullptr,&device));vkGetDeviceQueue(device,family,0,&queue);
    mango_vk::Overlay overlay({init_ui,draw_ui,nullptr,[](const char* s){std::printf("%s\n",s);}});overlay.device({instance,physical,device,queue,family,vkGetInstanceProcAddr,vkGetDeviceProcAddr});
    unsigned sequence=0;
    for(auto format:{VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8G8B8A8_SRGB})for(unsigned transform:{1u,2u,4u,8u,16u,32u,64u,128u}){
        create_targets(format);VkSwapchainKHR swap=(VkSwapchainKHR)uintptr_t(++sequence);VkSwapchainCreateInfoKHR s{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};s.imageFormat=format;s.imageExtent=extent;s.imageArrayLayers=1;s.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;s.minImageCount=2;s.preTransform=VkSurfaceTransformFlagBitsKHR(transform);overlay.created(device,swap,s);VkImage images[]={targets[0].image,targets[1].image,targets[2].image};overlay.test_images(swap,images,3,VK_IMAGE_LAYOUT_GENERAL);
        // Let ImGui publish the window's initial layout before the first press.
        const int prior_clicks=clicks;
        for(int frame=-2;frame<12;++frame){input_phase=frame<0?0:unsigned(frame);unsigned serial=unsigned(frame+2),index=(serial*serial+serial/3)%3;game_frame(index);VkPresentInfoKHR p{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};p.waitSemaphoreCount=1;p.pWaitSemaphores=&targets[index].game;p.swapchainCount=1;p.pSwapchains=&swap;p.pImageIndices=&index;ok(overlay.present(queue,&p,fake_present));
            const unsigned sw=mango_vk::rotated(s.preTransform)?extent.height:extent.width,sh=mango_vk::rotated(s.preTransform)?extent.width:extent.height;
            #ifdef REFERENCE_UI_FIXTURE
            auto background=displayed_pixel(index,1,sh-1,transform);
            #else
            auto background=displayed_pixel(index,sw/2,sh/2,transform);
            #endif
            assert(background[3]==255);
            auto color=displayed_pixel(index,30,30,transform);assert(std::abs(int(color[0])-128)<=3&&std::abs(int(color[1])-32)<=3&&std::abs(int(color[2])-64)<=3);assert(color!=background);
            auto red=displayed_pixel(index,12,12,transform),green=displayed_pixel(index,12,44,transform),blue=displayed_pixel(index,44,12,transform);
            assert(red[0]>250&&red[1]<3&&red[2]<3);assert(green[0]<3&&green[1]>250&&green[2]<3);assert(blue[0]<3&&blue[1]<3&&blue[2]>250);
            auto atlas_color=displayed_pixel(index,236,32,transform);
            assert(std::abs(int(atlas_color[0])-128)<=2&&std::abs(int(atlas_color[1])-64)<=2&&std::abs(int(atlas_color[2])-32)<=2);
            assert(displayed_pixel(index,100,24,transform)!=background); // Visible button under the test touch.
            #ifndef REFERENCE_UI_FIXTURE
            auto edge=displayed_pixel(index,sw-20,sh-20,transform);assert(edge==background); // Existing game color survives LOAD/STORE.
            #endif
        }
        assert(clicks==prior_clicks+3);
        // WSI may return out-of-date before consuming the overlay semaphore.
        input_phase=0;game_frame(0);unsigned index=0;VkPresentInfoKHR p{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};p.waitSemaphoreCount=1;p.pWaitSemaphores=&targets[0].game;p.swapchainCount=1;p.pSwapchains=&swap;p.pImageIndices=&index;reject=true;assert(overlay.present(queue,&p,fake_present)==VK_ERROR_OUT_OF_DATE_KHR);reject=false;
        overlay.destroyed(device,swap);destroy_targets();
    }
    overlay.device_destroyed(device);
    // A failed UI pipeline must forward the original game frame, clean partial
    // backend resources and leave the device usable.
    {mango_vk::Overlay failure({init_ui,draw_ui,nullptr,nullptr});failure.device({instance,physical,device,queue,family,vkGetInstanceProcAddr,fail_shader_proc});create_targets(VK_FORMAT_R8G8B8A8_UNORM);VkSwapchainKHR swap=(VkSwapchainKHR)uintptr_t(1000);VkSwapchainCreateInfoKHR s{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};s.imageFormat=VK_FORMAT_R8G8B8A8_UNORM;s.imageExtent=extent;s.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;s.imageArrayLayers=1;s.minImageCount=2;s.preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;failure.created(device,swap,s);VkImage images[]={targets[0].image,targets[1].image,targets[2].image};failure.test_images(swap,images,3,VK_IMAGE_LAYOUT_GENERAL);game_frame(0);unsigned index=0;VkPresentInfoKHR p{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};p.waitSemaphoreCount=1;p.pWaitSemaphores=&targets[0].game;p.swapchainCount=1;p.pSwapchains=&swap;p.pImageIndices=&index;ok(failure.present(queue,&p,fake_present));assert(pixel(0,16,16)==pixel(0,100,100));failure.device_destroyed(device);destroy_targets();assert(!ImGui::GetIO().BackendRendererUserData);}
    ImGui::DestroyContext();vkDestroyDevice(device,nullptr);auto destroy_debug=(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");destroy_debug(instance,messenger,nullptr);vkDestroyInstance(instance,nullptr);
    if(draws!=240||presents!=241||clicks!=48)std::fprintf(stderr,"Unexpected counts: draws=%d presents=%d clicks=%d\n",draws,presents,clicks);
    assert(draws==240&&presents==241&&clicks==48);assert(errors==0);std::printf("PASS: Vulkan overlay %d frames, 8 display orientations, 48 UI clicks, scaled Android/Unity coordinates, UNORM/sRGB, resize/out-of-date, allocation-failure forwarding; zero validation errors\n",draws);
}
