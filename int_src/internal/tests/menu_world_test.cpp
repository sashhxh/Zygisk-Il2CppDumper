#include "settings.h"
#include "menu_r40.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
struct Item {ImGuiID id;ImRect rect;std::string window;};
static std::vector<Item> items;
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx,ImGuiID id,const ImRect& bb,const ImGuiLastItemData*){
    if(id==ctx->CurrentWindow->ID)return;
    const char* name=ctx->CurrentWindow->Name;
    if(std::strstr(name,"LeftColumn")||std::strstr(name,"RightColumn"))items.push_back({id,bb,name});
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID,const char*,ImGuiItemStatusFlags){}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...){}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID){return nullptr;}
static void verify(float scale){
    for(size_t i=0;i<items.size();++i)for(size_t j=i+1;j<items.size();++j){
        if(items[i].window!=items[j].window)continue;
        const auto a=items[i].rect,b=items[j].rect;
        const float x=std::min(a.Max.x,b.Max.x)-std::max(a.Min.x,b.Min.x);
        const float y=std::min(a.Max.y,b.Max.y)-std::max(a.Min.y,b.Min.y);
        if(x>.5f*scale&&y>.5f*scale){
            std::fprintf(stderr,"OVERLAP %s: item %zu [%.1f %.1f %.1f %.1f] and %zu [%.1f %.1f %.1f %.1f]\n",items[i].window.c_str(),i,a.Min.x,a.Min.y,a.Max.x,a.Max.y,j,b.Min.x,b.Min.y,b.Max.x,b.Max.y);
            assert(false&&"World elements overlap");
        }
    }
    for(auto* w:GImGui->Windows)if(w->Active&&(std::strstr(w->Name,"LeftColumn")||std::strstr(w->Name,"RightColumn"))){
        assert(w->ScrollMax.y<=.5f&&w->ScrollMax.x<=.5f);
        for(const auto& item:items)if(item.window==w->Name){
            assert(item.rect.Min.x>=w->InnerRect.Min.x-.5f);
            assert(item.rect.Max.x<=w->InnerRect.Max.x+.5f);
            assert(item.rect.Min.y>=w->InnerRect.Min.y-.5f);
            assert(item.rect.Max.y<=w->InnerRect.Max.y+.5f);
        }
    }
}
static void dump(const char* prefix){
    if(!prefix)return;
    std::string stem(prefix);FILE* f=std::fopen((stem+".mesh").c_str(),"wb");assert(f);
    auto* d=ImGui::GetDrawData();
    std::fprintf(f,"%d %d %d\n",int(d->DisplaySize.x),int(d->DisplaySize.y),d->CmdListsCount);
    for(const auto* list:d->CmdLists){
        std::fprintf(f,"%d %d %d\n",list->VtxBuffer.Size,list->IdxBuffer.Size,list->CmdBuffer.Size);
        for(const auto& v:list->VtxBuffer)std::fprintf(f,"%.4f %.4f %.7f %.7f %u\n",v.pos.x,v.pos.y,v.uv.x,v.uv.y,v.col);
        for(const auto& idx:list->IdxBuffer)std::fprintf(f,"%u ",unsigned(idx));std::fputc('\n',f);
        for(const auto& c:list->CmdBuffer)std::fprintf(f,"%u %u %u %.4f %.4f %.4f %.4f\n",c.ElemCount,c.IdxOffset,c.VtxOffset,c.ClipRect.x,c.ClipRect.y,c.ClipRect.z,c.ClipRect.w);
    }
    std::fclose(f);
    unsigned char* pixels;int w,h;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    f=std::fopen((stem+".atlas").c_str(),"wb");assert(f);std::fprintf(f,"%d %d\n",w,h);std::fwrite(pixels,4,w*h,f);std::fclose(f);
}
int main(int argc,char** argv){
    const int sizes[][2]={{2800,1260},{2400,1080},{1920,1080},{1280,720},{1080,2400}};
    for(const auto& size:sizes){
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={float(size[0]),float(size[1])};io.DeltaTime=1.f/60;
        GImGui->TestEngineHookItems=true;
        mango_ui::State state;state.scale=mango_ui::FitScale(size[0],size[1]);state.active_tab=3;
        mango_ui::ApplyStyle(state.scale);mango_ui::LoadFont(io,state.scale);
        unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        mango_config::Settings cfg;cfg.extra.resource_esp=cfg.extra.farm=true;
        auto frame=[&](int farm=4,int resource=-1){items.clear();ImGui::NewFrame();mango_ui::Draw(cfg,state,size[0],size[1],9,0,0,0,0,0,farm,resource);ImGui::Render();};
        for(int i=0;i<3;++i)frame();
        for(int status=0;status<=11;++status)for(int resources:{-1,0,8192}){frame(status,resources);verify(state.scale);}
        ImGuiWindow* left=nullptr;for(auto* window:GImGui->Windows)if(std::strstr(window->Name,"LeftColumn"))left=window;
        assert(left);
        for(const auto& item:items)assert(item.id!=left->GetID("Auto Tool")&&item.id!=left->GetID("Auto Run"));
        auto click=[&](const char* label){
            const ImGuiID id=left->GetID(label);ImVec2 center{};bool found=false;
            for(const auto& item:items)if(item.id==id){center={(item.rect.Min.x+item.rect.Max.x)*.5f,(item.rect.Min.y+item.rect.Max.y)*.5f};found=true;break;}
            assert(found);io.AddMousePosEvent(center.x,center.y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();verify(state.scale);
        };
        click("Resource ESP");assert(!cfg.extra.resource_esp&&cfg.extra.farm&&cfg.extra.resources==15);
        click("Magic Farm");assert(!cfg.extra.resource_esp&&!cfg.extra.farm&&cfg.extra.resources==15);
        const char* types[]={"Wood","Stone","Iron","Sulfur"};unsigned mask=15;
        for(unsigned i=0;i<4;++i){click(types[i]);mask&=~(1u<<i);assert(cfg.extra.resources==mask&&!cfg.extra.resource_esp&&!cfg.extra.farm);}
        for(unsigned i=0;i<4;++i){click(types[i]);mask|=1u<<i;assert(cfg.extra.resources==mask);}
        cfg.extra.resource_esp=cfg.extra.farm=true;
        for(int i=0;i<20;++i)frame(2,18);verify(state.scale);
        if(size[0]==2800&&argc>1)dump(argv[1]);
        ImGui::DestroyContext();std::printf("PASS World %dx%d: no intersections/clipping; 10 independent toggle clicks; all farm statuses\n",size[0],size[1]);
    }
}
