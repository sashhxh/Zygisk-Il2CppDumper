#include "settings.h"
#include "menu_r40.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.DisplaySize={2800,1260};io.DeltaTime=1.f/60;
    mango_ui::State state;state.scale=mango_ui::FitScale(2800,1260);state.chams_settings=true;
    mango_ui::ApplyStyle(state.scale);mango_ui::LoadFont(io,state.scale);
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    mango_config::Settings cfg;cfg.chams=true;
    for(int style=mango_chams::Flat;style<=mango_chams::Rainbow;++style){
        cfg.chams_appearance.style=style;
        for(int frame=0;frame<3;++frame){ImGui::NewFrame();auto result=mango_ui::Draw(cfg,state,2800,1260);assert(result.open);ImGui::Render();}
        ImGuiWindow* right=nullptr;for(auto* window:GImGui->Windows)if(std::strstr(window->Name,"RightColumn"))right=window;
        assert(right&&right->Active);
        if(right->ScrollMax.y>.5f)std::fprintf(stderr,"Chams style %d needs %.1f pixels of scrolling\n",style,right->ScrollMax.y);
        assert(right->ScrollMax.y<=.5f);
        assert(ImGui::GetDrawData()->TotalVtxCount>0);
    }
    cfg.extra.auto_shoot=cfg.extra.farm=cfg.extra.resource_esp=cfg.extra.weapon=true;
    state.weapon_settings=true;
    for(int tab=1;tab<5;++tab){state.active_tab=tab;
        for(int frame=0;frame<3;++frame){ImGui::NewFrame();mango_ui::Draw(cfg,state,2800,1260);ImGui::Render();}
        for(auto* window:GImGui->Windows)if(window->Active&&(std::strstr(window->Name,"RightColumn")||std::strstr(window->Name,"LeftColumn"))){
            if(window->ScrollMax.y>.5f)std::fprintf(stderr,"Tab %d needs %.1f pixels of scrolling\n",tab,window->ScrollMax.y);
            assert(window->ScrollMax.y<=.5f);
        }
    }
    ImGui::DestroyContext();std::puts("PASS: chams styles, weapon controls, aim/world tabs render without clipped controls");
}
