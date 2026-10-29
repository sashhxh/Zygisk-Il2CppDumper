#pragma once
#include "imgui.h"
#include "resources_data.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <zlib.h>

namespace xvcen_assets {
// All images share ImGui's existing texture. This keeps the Vulkan renderer's
// resource lifetime, resize handling and descriptor ownership unchanged.
struct Atlas {
    ImGuiContext* owner=nullptr;
    std::array<ImFontAtlasRectId,AssetCount> rects{};
    ImFont* text=nullptr;
    ImFont* pixel=nullptr;
    float base_scale=1;
    void load(ImGuiIO& io,float scale) {
        base_scale=scale;
        owner=ImGui::GetCurrentContext();
        io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
        io.Fonts->TexMinWidth=io.Fonts->TexMaxWidth=4096;
        io.Fonts->TexMinHeight=io.Fonts->TexMaxHeight=4096;
        io.Fonts->TexDesiredFormat=ImTextureFormat_RGBA32;
        ImFontConfig cfg;cfg.FontDataOwnedByAtlas=false;
        // Exact registration at original RVA 0xbaf54..0xbaf74: raw Roboto
        // Medium, 159296 bytes, 28 px, default ImFontConfig. No Proggy merge.
        text=io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(font_menu),
            sizeof(font_menu),28.f,&cfg);
        pixel=text;
        if(!text||!pixel)throw std::runtime_error("Reference fonts could not be loaded");
        io.FontDefault=text;
        for(int i=0;i<AssetCount;++i)
            rects[i]=io.Fonts->AddCustomRect(images[i].width,images[i].height);
    }
    void bake(ImGuiIO& io) {
        if(owner!=ImGui::GetCurrentContext())
            throw std::runtime_error("Reference atlas belongs to another ImGui context");
        unsigned char* pixels;int width,height;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        if(!pixels)throw std::runtime_error("Reference atlas could not be built");
        std::vector<unsigned char> decoded;
        for(int i=0;i<AssetCount;++i) {
            const auto& image=images[i];
            ImFontAtlasRect rect;
            if(!io.Fonts->GetCustomRect(rects[i],&rect)||rect.x+image.width>width||rect.y+image.height>height)
                throw std::runtime_error("Reference image does not fit the font atlas");
            const std::size_t stride=std::size_t(image.width)*4;
            decoded.resize(stride*image.height);
            uLongf bytes=decoded.size();
            if(uncompress(decoded.data(),&bytes,image.data,image.size)!=Z_OK||bytes!=decoded.size())
                throw std::runtime_error("Reference image decompression failed");
            for(int row=0;row<image.height;++row)
                std::memcpy(pixels+(std::size_t(rect.y+row)*width+rect.x)*4,
                    decoded.data()+std::size_t(row)*stride,stride);
        }
        io.Fonts->TexPixelsUseColors=true;
        io.Fonts->TexData->UseColors=true;
    }
    void image(ImDrawList* draw,Asset asset,ImVec2 min,ImVec2 max,
               ImVec2 uv0={0,0},ImVec2 uv1={1,1},ImU32 tint=IM_COL32_WHITE,float rounding=0) const {
        auto* atlas=ImGui::GetIO().Fonts;
        if(owner!=ImGui::GetCurrentContext())return;
        ImFontAtlasRect rect;
        if(!atlas->GetCustomRect(rects[asset],&rect))return;
        const ImVec2 lo=rect.uv0,hi=rect.uv1;
        const ImVec2 a{lo.x+(hi.x-lo.x)*uv0.x,lo.y+(hi.y-lo.y)*uv0.y};
        const ImVec2 b{lo.x+(hi.x-lo.x)*uv1.x,lo.y+(hi.y-lo.y)*uv1.y};
        if(rounding>0)draw->AddImageRounded(atlas->TexID,min,max,a,b,tint,rounding);
        else draw->AddImage(atlas->TexID,min,max,a,b,tint);
    }
};
inline Atlas atlas;
}
