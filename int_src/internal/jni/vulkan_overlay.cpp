#include "vulkan_overlay.h"
#include "imgui_impl_vulkan.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace mango_vk {
namespace {
struct Failure {VkResult result;};
void check(VkResult result){if(result<0)throw Failure{result};}
ImVec2 image_point(ImVec2 p,VkExtent2D e,VkSurfaceTransformFlagBitsKHR t){
    const bool mirror=t>=VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR&&t<=VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR;
    unsigned r=t==VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR||t==VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR?1:
        t==VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR||t==VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR?2:
        t==VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR||t==VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR?3:0;
    // preTransform describes rotation ALREADY applied to image content. With
    // top-left Vulkan coordinates, apply the clockwise rotation here (not its
    // inverse). Android displays this pre-rotated buffer in window orientation.
    // The mirror variants specify mirroring before rotation.
    if(mirror)p.x=float(rotated(t)?e.height:e.width)-p.x;
    return r==1?ImVec2(float(e.width)-p.y,p.x):r==2?ImVec2(float(e.width)-p.x,float(e.height)-p.y):r==3?ImVec2(p.y,float(e.height)-p.x):p;
}
bool srgb(VkFormat f){return f==VK_FORMAT_R8G8B8A8_SRGB||f==VK_FORMAT_B8G8R8A8_SRGB||f==VK_FORMAT_A8B8G8R8_SRGB_PACK32;}
void linear_colors(ImDrawData* data){
    static const std::array<unsigned,256> table=[](){std::array<unsigned,256> a{};for(unsigned i=0;i<256;++i){float s=float(i)/255;float v=s<=.04045f?s/12.92f:std::pow((s+.055f)/1.055f,2.4f);a[i]=unsigned(v*255+.5f);}return a;}();
    for(int i=0;i<data->CmdListsCount;++i)for(auto& v:data->CmdLists[i]->VtxBuffer){unsigned c=v.col;v.col=(c&0xff000000u)|table[c&255]|(table[(c>>8)&255]<<8)|(table[(c>>16)&255]<<16);}
}
#define VK_FUNCTIONS(X) \
 X(CreateRenderPass) X(DestroyRenderPass) X(GetSwapchainImagesKHR) X(CreateImageView) X(DestroyImageView) \
 X(CreateFramebuffer) X(DestroyFramebuffer) X(CreateSemaphore) X(DestroySemaphore) X(CreateFence) X(DestroyFence) \
 X(WaitForFences) X(ResetFences) X(CreateCommandPool) X(DestroyCommandPool) X(ResetCommandPool) \
 X(AllocateCommandBuffers) X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdBeginRenderPass) X(CmdEndRenderPass) \
 X(QueueSubmit) X(QueueWaitIdle)
struct Api {
    Device device{};
#define DECLARE(name) PFN_vk##name name=nullptr;
    VK_FUNCTIONS(DECLARE)
#undef DECLARE
    PFN_vkVoidFunction lookup(const char* name){
        auto p=device.device_proc(device.device,name);return p?p:device.instance_proc(device.instance,name);
    }
    bool load(Device d){
        device=d;if(!d.device||!d.instance||!d.physical||!d.queue||!d.instance_proc||!d.device_proc)return false;
#define LOAD(name) name=reinterpret_cast<PFN_vk##name>(lookup("vk" #name));if(!name)return false;
        VK_FUNCTIONS(LOAD)
#undef LOAD
        return true;
    }
};
struct Frame {VkCommandPool pool=VK_NULL_HANDLE;VkCommandBuffer command=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;};
struct Image {VkImage image=VK_NULL_HANDLE;VkImageView view=VK_NULL_HANDLE;VkFramebuffer framebuffer=VK_NULL_HANDLE;VkSemaphore ready=VK_NULL_HANDLE;};
struct Swapchain {
    VkDevice device=VK_NULL_HANDLE;VkSwapchainKHR handle=VK_NULL_HANDLE;VkFormat format=VK_FORMAT_UNDEFINED;VkExtent2D extent{};
    VkSurfaceTransformFlagBitsKHR transform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkImageLayout boundary=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    uint32_t minimum=2;std::vector<VkImage> fixture;std::vector<Image> images;std::vector<Frame> frames;
    VkRenderPass pass=VK_NULL_HANDLE;size_t next=0;bool initialized=false,invalid=false,renderable=true;
};
}
bool rotated(VkSurfaceTransformFlagBitsKHR t){return t==VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR||t==VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR||t==VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR||t==VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR;}
void transform_draw_data(ImDrawData* data,VkExtent2D e,VkSurfaceTransformFlagBitsKHR transform){
    if(!data)return;
    for(int i=0;i<data->CmdListsCount;++i){
        auto* list=data->CmdLists[i];for(auto& v:list->VtxBuffer)v.pos=image_point(v.pos,e,transform);
        for(auto& command:list->CmdBuffer){ImVec2 a=image_point({command.ClipRect.x,command.ClipRect.y},e,transform),b=image_point({command.ClipRect.z,command.ClipRect.w},e,transform);command.ClipRect={std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y)};}
    }
    data->DisplayPos={0,0};data->DisplaySize={float(e.width),float(e.height)};data->FramebufferScale={1,1};
}
struct Overlay::State {
    std::mutex mutex;Callbacks callbacks;Device device{};Api api;std::map<VkSwapchainKHR,std::unique_ptr<Swapchain>> swaps;
    Swapchain* active=nullptr;bool backend=false,disabled=false;int last_status=-999;
    explicit State(Callbacks c):callbacks(c){}
    void log(int status,const char* message){if(status==last_status)return;last_status=status;if(callbacks.log)callbacks.log(message);}
    void release(Swapchain& s){
        if(!s.pass&&s.images.empty()&&s.frames.empty()&&active!=&s)return;
        if(!api.device.device||s.device!=api.device.device)return;
        const VkResult idle=api.QueueWaitIdle(api.device.queue); // Only on resize/device teardown.
        if(idle!=VK_ERROR_DEVICE_LOST)check(idle); // Destruction remains valid after device loss.
        if(active==&s&&backend){ImGui_ImplVulkan_Shutdown();backend=false;active=nullptr;}
        for(auto f:s.frames){if(f.fence)api.DestroyFence(s.device,f.fence,nullptr);if(f.pool)api.DestroyCommandPool(s.device,f.pool,nullptr);}
        for(auto i:s.images){if(i.ready)api.DestroySemaphore(s.device,i.ready,nullptr);if(i.framebuffer)api.DestroyFramebuffer(s.device,i.framebuffer,nullptr);if(i.view)api.DestroyImageView(s.device,i.view,nullptr);}
        if(s.pass)api.DestroyRenderPass(s.device,s.pass,nullptr);
        s.frames.clear();s.images.clear();s.pass=VK_NULL_HANDLE;s.initialized=false;
    }
    bool initialize(Swapchain& s){
        if(s.initialized&&active==&s&&backend)return true;
        if(active&&active!=&s)release(*active);
        if(!api.load(device)||device.device!=s.device)return false;
        if(!s.renderable||s.extent.width<180||s.extent.height<180||s.extent.width>20000||s.extent.height>20000||s.transform==VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR)return false;
        std::vector<VkImage> images=s.fixture;
        if(images.empty()){
            uint32_t count=0;check(api.GetSwapchainImagesKHR(s.device,s.handle,&count,nullptr));if(count<2||count>16)return false;
            images.resize(count);check(api.GetSwapchainImagesKHR(s.device,s.handle,&count,images.data()));images.resize(count);
        }
        if(images.size()<2||images.size()>16)return false;
        VkAttachmentDescription attachment{};attachment.format=s.format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachment.initialLayout=attachment.finalLayout=s.boundary;
        VkAttachmentReference reference{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&reference;
        VkSubpassDependency dependency{};dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;dependency.srcStageMask=dependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};pass.attachmentCount=1;pass.pAttachments=&attachment;pass.subpassCount=1;pass.pSubpasses=&sub;pass.dependencyCount=1;pass.pDependencies=&dependency;check(api.CreateRenderPass(s.device,&pass,nullptr,&s.pass));
        s.images.resize(images.size());s.frames.resize(images.size());
        for(size_t i=0;i<images.size();++i){
            auto& image=s.images[i];image.image=images[i];VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=image.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=s.format;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};check(api.CreateImageView(s.device,&view,nullptr,&image.view));
            VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fb.renderPass=s.pass;fb.attachmentCount=1;fb.pAttachments=&image.view;fb.width=s.extent.width;fb.height=s.extent.height;fb.layers=1;check(api.CreateFramebuffer(s.device,&fb,nullptr,&image.framebuffer));
            VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};check(api.CreateSemaphore(s.device,&semaphore,nullptr,&image.ready));
            auto& frame=s.frames[i];VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pool.queueFamilyIndex=device.family;check(api.CreateCommandPool(s.device,&pool,nullptr,&frame.pool));
            VkCommandBufferAllocateInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};command.commandPool=frame.pool;command.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;command.commandBufferCount=1;check(api.AllocateCommandBuffers(s.device,&command,&frame.command));
            VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fence.flags=VK_FENCE_CREATE_SIGNALED_BIT;check(api.CreateFence(s.device,&fence,nullptr,&frame.fence));
        }
        const uint32_t w=rotated(s.transform)?s.extent.height:s.extent.width,h=rotated(s.transform)?s.extent.width:s.extent.height;
        if(callbacks.initialize_ui)callbacks.initialize_ui(w,h);if(!ImGui::GetCurrentContext())throw Failure{VK_ERROR_INITIALIZATION_FAILED};
        if(!ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_0,[](const char* name,void* userdata){return static_cast<Api*>(userdata)->lookup(name);},&api))throw Failure{VK_ERROR_INITIALIZATION_FAILED};
        ImGui_ImplVulkan_InitInfo info{};info.ApiVersion=VK_API_VERSION_1_0;info.Instance=device.instance;info.PhysicalDevice=device.physical;info.Device=device.device;info.QueueFamily=device.family;info.Queue=device.queue;info.DescriptorPoolSize=8;info.PipelineInfoMain.RenderPass=s.pass;info.MinImageCount=std::min(uint32_t(images.size()),std::max(2u,s.minimum));info.ImageCount=uint32_t(images.size());info.PipelineInfoMain.MSAASamples=VK_SAMPLE_COUNT_1_BIT;info.CheckVkResultFn=check;
        info.FontTextureFormat=srgb(s.format)?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM;
        try{if(!ImGui_ImplVulkan_Init(&info))throw Failure{VK_ERROR_INITIALIZATION_FAILED};}
        catch(...){backend=ImGui::GetIO().BackendRendererUserData!=nullptr;if(backend)active=&s;throw;}
        backend=true;active=&s;
        s.initialized=true;s.next=0;
        char message[160];std::snprintf(message,sizeof(message),"Vulkan overlay ready: image=%ux%u ui=%ux%u preTransform=0x%x",s.extent.width,s.extent.height,w,h,unsigned(s.transform));log(1,message);return true;
    }
};
Overlay::Overlay(Callbacks c):state_(new State(c)){}
Overlay::~Overlay(){try{std::lock_guard<std::mutex> lock(state_->mutex);for(auto& p:state_->swaps)state_->release(*p.second);}catch(...){}delete state_;}
void Overlay::device(const Device& d){std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->device.device||state_->device.device==d.device)state_->device=d;}
void Overlay::created(VkDevice d,VkSwapchainKHR handle,const VkSwapchainCreateInfoKHR& info){
    std::lock_guard<std::mutex> lock(state_->mutex);if(state_->swaps.find(handle)!=state_->swaps.end())return;
    auto slot=std::make_unique<Swapchain>();slot->device=d;slot->handle=handle;slot->format=info.imageFormat;slot->extent=info.imageExtent;slot->transform=info.preTransform;slot->minimum=info.minImageCount;
    slot->renderable=(info.imageUsage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)&&info.imageArrayLayers==1&&!(info.flags&VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR);
    state_->swaps.emplace(handle,std::move(slot));
}
void Overlay::test_images(VkSwapchainKHR handle,const VkImage* images,uint32_t count,VkImageLayout layout){std::lock_guard<std::mutex> lock(state_->mutex);auto it=state_->swaps.find(handle);if(it!=state_->swaps.end()){it->second->fixture.assign(images,images+count);it->second->boundary=layout;}}
bool Overlay::knows(VkDevice device,VkSwapchainKHR handle){std::lock_guard<std::mutex> lock(state_->mutex);auto it=state_->swaps.find(handle);return it!=state_->swaps.end()&&it->second->device==device;}
void Overlay::destroyed(VkDevice d,VkSwapchainKHR handle){std::lock_guard<std::mutex> lock(state_->mutex);auto it=state_->swaps.find(handle);if(it!=state_->swaps.end()&&it->second->device==d){try{state_->release(*it->second);}catch(...){state_->disabled=true;}state_->swaps.erase(it);}}
void Overlay::device_destroyed(VkDevice d){std::lock_guard<std::mutex> lock(state_->mutex);if(state_->device.device==d){for(auto& s:state_->swaps)try{state_->release(*s.second);}catch(...){}state_->swaps.clear();state_->active=nullptr;state_->backend=false;state_->device={};state_->disabled=false;}}
VkResult Overlay::present(VkQueue queue,const VkPresentInfoKHR* info,PFN_vkQueuePresentKHR original){
    if(!original)return VK_ERROR_INITIALIZATION_FAILED;
    std::unique_lock<std::mutex> lock(state_->mutex);auto& st=*state_;Swapchain* swap=nullptr;
    if(info&&info->swapchainCount==1&&info->pSwapchains&&info->pImageIndices){auto it=st.swaps.find(info->pSwapchains[0]);if(it!=st.swaps.end())swap=it->second.get();}
    uint32_t width=0,height=0;if(swap){width=rotated(swap->transform)?swap->extent.height:swap->extent.width;height=rotated(swap->transform)?swap->extent.width:swap->extent.height;}
    if(st.callbacks.presented)st.callbacks.presented(width,height);
    VkPresentInfoKHR rewritten{};VkSemaphore signal=VK_NULL_HANDLE;bool submitted=false;
    try{
        if(!st.disabled&&swap&&!swap->invalid&&queue==st.device.queue&&st.initialize(*swap)&&info->pImageIndices[0]<swap->images.size()){
            auto& frame=swap->frames[swap->next];auto& image=swap->images[info->pImageIndices[0]];
            // A busy overlay frame is skipped; never stall gameplay waiting for it.
            if(st.api.WaitForFences(swap->device,1,&frame.fence,VK_TRUE,2000000)==VK_SUCCESS){
                check(st.api.ResetCommandPool(swap->device,frame.pool,0));ImGui_ImplVulkan_NewFrame();
                ImDrawData* data=st.callbacks.draw_ui?st.callbacks.draw_ui(width,height):nullptr;
                if(data){
                    if(srgb(swap->format))linear_colors(data);transform_draw_data(data,swap->extent,swap->transform);
                    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(st.api.BeginCommandBuffer(frame.command,&begin));
                    VkRenderPassBeginInfo render{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};render.renderPass=swap->pass;render.framebuffer=image.framebuffer;render.renderArea.extent=swap->extent;st.api.CmdBeginRenderPass(frame.command,&render,VK_SUBPASS_CONTENTS_INLINE);ImGui_ImplVulkan_RenderDrawData(data,frame.command);st.api.CmdEndRenderPass(frame.command);check(st.api.EndCommandBuffer(frame.command));
                    std::vector<VkPipelineStageFlags> stages(info->waitSemaphoreCount,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=info->waitSemaphoreCount;submit.pWaitSemaphores=info->pWaitSemaphores;submit.pWaitDstStageMask=stages.data();submit.commandBufferCount=1;submit.pCommandBuffers=&frame.command;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&image.ready;
                    check(st.api.ResetFences(swap->device,1,&frame.fence));check(st.api.QueueSubmit(queue,1,&submit,frame.fence));
                    signal=image.ready;rewritten=*info;rewritten.waitSemaphoreCount=1;rewritten.pWaitSemaphores=&signal;submitted=true;swap->next=(swap->next+1)%swap->frames.size();
                    if(data->TotalVtxCount>0)st.log(3,"Vulkan UI frame submitted");
                }
            }
        }else if(!swap)st.log(2,"Vulkan: waiting for swapchain creation");
    }catch(const Failure& f){char message[96];std::snprintf(message,sizeof(message),"Vulkan overlay disabled after error %d",int(f.result));st.log(-1,message);st.disabled=true;}
    catch(...){st.log(-2,"Vulkan overlay disabled after an exception");st.disabled=true;}
    // Keep the mutex through present: image semaphore lifetime is tied to WSI.
    const VkResult result=original(queue,submitted?&rewritten:info);
    // A failed presentation may not consume its wait semaphore. Do not signal
    // it again; Unity will replace this out-of-date/lost swapchain.
    if(swap&&result<0)swap->invalid=true;
    return result;
}
}
