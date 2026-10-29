#include "settings.h"
#include "menu.h"
#include "reference_esp.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct Item {ImGuiID id;ImRect rect;std::string label,window;};
static std::vector<Item> items;
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx,ImGuiID id,const ImRect& rect,const ImGuiLastItemData*){
    if(id!=ctx->CurrentWindow->ID)items.push_back({id,rect,"",ctx->CurrentWindow->Name});
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags){
    for(auto it=items.rbegin();it!=items.rend();++it)if(it->id==id){it->label=label;break;}
}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...){}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID){return nullptr;}

static void dump(const std::string& prefix){
    FILE* f=std::fopen((prefix+".mesh").c_str(),"wb");assert(f);
    auto* d=ImGui::GetDrawData();
    std::fprintf(f,"%d %d %d\n",int(d->DisplaySize.x),int(d->DisplaySize.y),d->CmdListsCount);
    for(const auto* list:d->CmdLists){
        std::fprintf(f,"%d %d %d\n",list->VtxBuffer.Size,list->IdxBuffer.Size,list->CmdBuffer.Size);
        for(const auto& v:list->VtxBuffer)std::fprintf(f,"%.4f %.4f %.7f %.7f %u\n",v.pos.x,v.pos.y,v.uv.x,v.uv.y,v.col);
        for(auto i:list->IdxBuffer)std::fprintf(f,"%u ",unsigned(i));std::fputc('\n',f);
        for(const auto& c:list->CmdBuffer)std::fprintf(f,"%u %u %u %.4f %.4f %.4f %.4f\n",c.ElemCount,c.IdxOffset,c.VtxOffset,c.ClipRect.x,c.ClipRect.y,c.ClipRect.z,c.ClipRect.w);
    }
    std::fclose(f);unsigned char* pixels;int w,h;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    f=std::fopen((prefix+".atlas").c_str(),"wb");assert(f);
    std::fprintf(f,"%d %d\n",w,h);std::fwrite(pixels,4,size_t(w)*h,f);std::fclose(f);
}
static void verify_atlas(){
    auto& atlas=mango_reference::atlas;auto* fonts=ImGui::GetIO().Fonts;
    unsigned char* pixels;int width,height;fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    assert(width==4096&&height<=8192);
    std::vector<unsigned char> decoded;
    for(int i=0;i<mango_reference::AssetCount;++i){
        const auto& image=mango_reference::images[i];
        ImFontAtlasRect rect;assert(fonts->GetCustomRect(atlas.rects[i],&rect));
        decoded.resize(size_t(image.width)*image.height*4);uLongf length=decoded.size();
        assert(uncompress(decoded.data(),&length,image.data,image.size)==Z_OK);
        assert(length==decoded.size());
        for(int y=0;y<image.height;++y)
            assert(!std::memcmp(pixels+(size_t(rect.y+y)*width+rect.x)*4,
                decoded.data()+size_t(y)*image.width*4,size_t(image.width)*4));
    }
    assert(atlas.text->GetFontBaked(28.6f)->FindGlyphNoFallback('A'));
    assert(atlas.text->GetFontBaked(28.6f)->FindGlyphNoFallback(0x0416));
    assert(!std::strcmp(IMGUI_VERSION,"1.92.9b"));
    const auto* source=atlas.text->Sources[0];
    assert(source->PixelSnapH&&source->OversampleH==1&&source->OversampleV==1);
    assert(std::fabs(source->ExtraSizeScale-1.015f)<1e-6f);
    assert(std::fabs(source->GlyphOffset.y-.40625f)<1e-6f);
    assert(source->Flags&ImFontFlags_ImplicitRefSize);
    for(float size:{17.732f,28.6f,32.89f,41.47f,44.33f}){
        auto* baked=atlas.text->GetFontBaked(size);
        assert(std::fabs(baked->Size-std::floor(size+.5f))<.01f&&baked->FindGlyphNoFallback('A'));
    }
    std::printf("PASS: all seven reference images retain every RGBA pixel in %dx%d atlas\n",width,height);
}
static void verify_motion(){
    using namespace mango_reference;
    Spring a,b;
    for(int i=0;i<60;++i)a.critical(73,1.f/60);
    for(int i=0;i<120;++i)b.critical(73,1.f/120);
    assert(std::fabs(a.value-b.value)<.001f&&std::fabs(a.velocity-b.velocity)<.01f);
    RollingCounter count;
    count.update(9,.016f);count.update(9,.016f);count.update(9,.016f);
    assert(count.accepted==0&&count.number.value==0);
    count.update(9,.016f);count.update(9,.016f);
    assert(count.accepted==9&&count.number.value>0&&count.number.value<9);
    count.update(9999,.05f);count.update(9999,.05f);assert(count.accepted==999);
    MenuMotion menu;
    for(int i=0;i<12;++i)menu.update(true,1.f/60);
    assert(std::fabs(menu.position()-.5f)<.001f&&!menu.settled(true));
    menu.update(false,1.f/60);assert(menu.position()<.5f);
    for(int i=0;i<25;++i)menu.update(true,1.f/60);
    assert(menu.settled(true));
    for(int i=0;i<25;++i)menu.update(false,1.f/60);
    assert(menu.settled(false));
    const ImVec2 origin{780,180},size{840,720},anchor{1134,1080},anchor_end{1266,1212};
    for(const ImVec2 p:{origin,ImVec2(1620,900),ImVec2(1200,540)}){
        const auto full=menu_warp(p,origin,size,anchor,anchor_end,1);
        assert(std::fabs(full.x-p.x)<.001f&&std::fabs(full.y-p.y)<.001f);
        const auto closed=menu_warp(p,origin,size,anchor,anchor_end,0);
        assert(closed.y==anchor.y&&closed.x>=anchor.x&&closed.x<=anchor_end.x);
    }
    const auto middle=menu_warp({780,540},origin,size,anchor,anchor_end,.5f);
    const auto top=menu_warp(origin,origin,size,anchor,anchor_end,.5f);
    assert(middle.x>top.x&&middle.y>top.y);
    std::puts("PASS: exact-time counter spring, input debounce, 0.4s menu motion and reversal");
}
static void verify_options(){
    mango_reference::Options a;a.dark=a.separators=a.fps=a.names=a.health=a.target_arrow=a.russian=true;
    a.thickness=2.75f;a.name_color[1]=.25f;
    char text[1024]{};assert(mango_reference::save_options(text,sizeof(text),a));
    FILE* file=std::tmpfile();assert(file);std::fputs(text,file);std::rewind(file);
    mango_reference::Options b;mango_reference::load_options(file,b);std::fclose(file);
    assert(b.dark&&b.separators&&b.fps&&b.names&&b.health&&b.target_arrow&&b.russian);
    assert(b.thickness==2.75f&&b.name_color[1]==.25f);
    file=std::tmpfile();assert(file);std::fputs("REFERENCE_UI_V1 0 0 0 0 0 0 0 nan\n",file);std::rewind(file);
    mango_reference::load_options(file,b);std::fclose(file);assert(b.thickness==2.75f&&b.dark);
    assert(!mango_reference::save_options(text,8,a));
    uint16_t valid[]={0x0041,0x0416,0xd83d,0xde00};char name[32];
    assert(mango_reference::utf16_name(valid,4,name,sizeof(name)));
    assert(!std::strcmp(name,"A\xd0\x96\xf0\x9f\x98\x80"));
    uint16_t broken[]={0xd800};assert(!mango_reference::utf16_name(broken,1,name,sizeof(name)));
    assert(!mango_reference::utf16_name(valid,4,name,3));
    std::puts("PASS: appearance settings round-trip, invalid settings and malformed names rejected");
}
static void exercise(int width,int height,const char* preview){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.DisplaySize={float(width),float(height)};io.DeltaTime=1.f/60;
    GImGui->TestEngineHookItems=true;
    mango_ui::State state;state.scale=mango_ui::FitScale(width,height);
    mango_ui::ApplyStyle(state.scale);mango_ui::LoadFont(io,state.scale);mango_ui::BakeAssets(io);
    io.Fonts->SetTexID(ImTextureID(1));
    io.Fonts->TexData->SetStatus(ImTextureStatus_OK);
    if(width==2400)verify_atlas();
    mango_config::Settings cfg;
    auto frame=[&](){
        items.clear();ImGui::NewFrame();auto r=mango_ui::Draw(cfg,state,width,height,9);ImGui::Render();
        for(auto* tex:ImGui::GetPlatformIO().Textures){
            if(tex->Status==ImTextureStatus_WantCreate||tex->Status==ImTextureStatus_WantUpdates){tex->SetTexID(1);tex->SetStatus(ImTextureStatus_OK);}
            else if(tex->Status==ImTextureStatus_WantDestroy){tex->SetTexID(ImTextureID_Invalid);tex->SetStatus(ImTextureStatus_Destroyed);}
        }
        if(state.open){assert(r.min.x>=0&&r.min.y>=0);assert(r.max.x<=width&&r.max.y<=height);}
        for(const auto* list:ImGui::GetDrawData()->CmdLists)for(const auto& v:list->VtxBuffer){
            assert(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y)&&std::isfinite(v.uv.x)&&std::isfinite(v.uv.y));
        }
        return r;
    };
    auto settle=[&](int count=60){for(int i=0;i<count;++i)frame();};settle();
    assert(state.menu_motion.settled(true));
    assert(std::fabs(state.island.bounds.Min.y-std::clamp(height*.035f,26.f,42.f))<.01f);
    assert(std::fabs(state.island.bounds.GetHeight()-66*state.island.scale())<.01f);
    auto click_at=[&](ImVec2 p){io.AddMousePosEvent(p.x,p.y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
    auto click_label=[&](const char* label){
        for(const auto& item:items)if(item.label==label&&item.window.find("ReferenceContent")!=std::string::npos){
            auto* content=ImGui::FindWindowByName(item.window.c_str());
            if(item.rect.Min.y<content->InnerRect.Min.y||item.rect.Max.y>content->InnerRect.Max.y)continue;
            click_at({item.rect.Max.x-72*state.scale,(item.rect.Min.y+item.rect.Max.y)*.5f});return;
        }
        std::fprintf(stderr,"Missing visible item: %s\n",label);assert(false);
    };
    for(int tab=0;tab<6;++tab){
        state.active_tab=tab;settle(60);
        assert(ImGui::GetDrawData()->TotalVtxCount>0);
        if(width==2400&&preview&&tab==2)dump(std::string(preview)+"-visuals-light");
    }
    state.active_tab=2;settle();
    const bool box=cfg.box;click_label("Box");assert(cfg.box!=box);
    for(const auto& item:items)if(item.label=="Box"){
        const float font=28.6f*state.scale*1.15f;
        const ImVec2 dot{item.rect.Min.x+63*state.scale+mango_ui::TextSize("Box",font).x,
            (item.rect.Min.y+item.rect.Max.y)*.5f};
        const bool prior=cfg.box;click_at(dot);assert(cfg.box==prior);
        assert(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup));
        settle(60);assert(state.colors.opening.value>.99f);
        click_at(state.colors.swatches[3].GetCenter());
        assert(std::fabs(cfg.color[0]-1)<.001f&&std::fabs(cfg.color[1]-.6f)<.001f&&std::fabs(cfg.color[2]-.1f)<.001f);
        settle(60);
        if(width==2400&&preview)dump(std::string(preview)+"-color-sheet");
        click_at(state.colors.done.GetCenter());assert(state.colors.closing);
        assert(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup));
        settle(90);assert(!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup));break;
    }
    click_label("Names");assert(cfg.reference.names);
    click_label("Health");assert(cfg.reference.health);
    auto* content=ImGui::FindWindowByName("##MangoMenu/##ReferenceContent_2A70BE74");
    if(!content)for(auto* w:GImGui->Windows)if(std::strstr(w->Name,"ReferenceContent"))content=w;
    assert(content&&content->ScrollMax.y>0);
    const auto p=content->InnerRect.Min;const bool before=cfg.skeleton;
    io.AddMousePosEvent(p.x+100*state.scale,p.y+100*state.scale);frame();
    io.AddMouseButtonEvent(0,true);frame();
    io.AddMousePosEvent(p.x+100*state.scale,p.y+20*state.scale);frame();
    io.AddMouseButtonEvent(0,false);settle();
    if(cfg.skeleton!=before||content->Scroll.y<=0)
        std::fprintf(stderr,"Drag failed: skeleton=%d before=%d scroll=%.1f max=%.1f drag=%d\n",
            cfg.skeleton,before,content->Scroll.y,content->ScrollMax.y,state.dragging_content);
    assert(cfg.skeleton==before&&content->Scroll.y>0);
    state.active_tab=5;settle();click_label("Dark theme");assert(cfg.reference.dark);
    click_label("Row separators");assert(cfg.reference.separators);
    settle(120);
    if(width==2400&&preview)dump(std::string(preview)+"-settings-dark");
    auto* parent=ImGui::FindWindowByName("##MangoMenu");assert(parent);
    click_at({parent->Pos.x+(840-144+96)*state.scale,parent->Pos.y+40*state.scale});
    assert(cfg.reference.russian);settle();
    if(width==2400&&preview)dump(std::string(preview)+"-settings-russian");
    click_at({parent->Pos.x+(840-144+32)*state.scale,parent->Pos.y+40*state.scale});
    assert(!cfg.reference.russian);
    state.active_tab=4;settle();
    click_label("Resource ESP");assert(cfg.extra.resource_esp);
    click_label("Magic Farm");assert(cfg.extra.farm);
    const unsigned mask=cfg.extra.resources;click_label("Wood");assert(cfg.extra.resources==(mask^1u));
    // A real sidebar click must switch tabs without changing a feature.
    auto* menu=ImGui::FindWindowByName("##MangoMenu");assert(menu);
    click_at({menu->Pos.x+100*state.scale,menu->Pos.y+(226.4f+70+35)*state.scale});
    assert(state.active_tab==1&&cfg.extra.farm);
    settle();
    for(const auto& item:items)if(item.label=="FOV radius"){
        const ImVec2 start{item.rect.Min.x+100*state.scale,item.rect.Min.y+76*state.scale};
        const float prior=cfg.magic_fov;
        io.AddMousePosEvent(start.x,start.y);frame();io.AddMouseButtonEvent(0,true);frame();
        io.AddMousePosEvent(start.x,start.y-80*state.scale);frame();
        io.AddMouseButtonEvent(0,false);settle();assert(cfg.magic_fov==prior);break;
    }
    state.previous_tab=-1;settle();click_label("FOV radius");assert(cfg.magic_fov>600);
    click_at({menu->Pos.x+28*state.scale,menu->Pos.y+32*state.scale});assert(!state.open);
    assert(state.menu_motion.phase>0);settle(30);assert(state.menu_motion.settled(false));
    assert(!ImGui::FindWindowByName("##MangoMenu")->Active);
    auto r=frame();io.AddMousePosEvent((r.bar_min.x+r.bar_max.x)*.5f,(r.bar_min.y+r.bar_max.y)*.5f);frame();
    io.AddMouseButtonEvent(0,true);frame();assert(state.open&&state.island.pulse==0);
    assert(state.menu_motion.phase>0&&state.menu_motion.phase<1);
    if(width==2400&&preview){settle(10);dump(std::string(preview)+"-menu-opening");}
    io.AddMouseButtonEvent(0,false);frame();assert(state.open);settle(60);
    cfg.reference.fps=true;settle(30);assert(state.island.fps_blend==1);
    state.notification.push("Box",true);settle(90);
    assert(state.notification.active&&!state.notification.closing&&state.notification.opacity>.99f);
    if(width==2400&&preview)dump(std::string(preview)+"-island-notification");
    state.notification.push("Skeleton",false);assert(state.notification.queued);settle(60);
    assert(!state.notification.queued&&state.notification.text=="Skeleton|OFF");
    settle(180);assert(!state.notification.active);
    cfg.reference.fps=false;settle(30);assert(state.island.fps_blend==0);
    if(width==2400)verify_atlas();
    if(width==2400&&preview){
        ImGui::NewFrame();auto* draw=ImGui::GetBackgroundDrawList();
        const ImVec2 lo{900,300},hi{1060,640};
        mango_reference::esp_box(draw,lo,hi,1,IM_COL32_WHITE);
        mango_reference::esp_health(draw,lo,hi,.7f);
        mango_reference::esp_tracer(draw,{980,300},1,IM_COL32_WHITE);
        mango_reference::esp_text(draw,980,274,17.732f,IM_COL32_WHITE,"Preview player");
        mango_reference::esp_text(draw,980,643,16.482f,IM_COL32_WHITE,"125m");
        const int before=draw->VtxBuffer.Size;
        mango_reference::esp_box(draw,{20,20},{19,30},1,IM_COL32_WHITE);
        assert(draw->VtxBuffer.Size==before);
        mango_ui::Text(draw,{24,24},"ESP render fixture",20,IM_COL32_WHITE);
        ImGui::Render();dump(std::string(preview)+"-esp");
    }
    ImGui::DestroyContext();std::printf("PASS %dx%d: six tabs, live toggles, touch scrolling, close/reopen, finite geometry\n",width,height);
}
int main(int argc,char** argv){
    verify_options();verify_motion();
    for(const auto size:{ImVec2(2400,1080),ImVec2(1280,720),ImVec2(720,1280)})
        exercise(int(size.x),int(size.y),argc>1?argv[1]:nullptr);
}
