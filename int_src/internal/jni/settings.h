#pragma once
#include "chams_appearance.h"
#include "extra_features.h"
#include "reference_options.h"
namespace mango_config {
struct Settings {
    bool enabled=true,box=true,distances=true,lines=false,skeleton=false,fill=false,corners=true;
    float range=300,color[3]={1,1,1};
    bool magic_bullet=false;float magic_fov=280;bool magic_ads_only=false;
    bool magic_team_filter=true;int magic_point=0;
    bool no_recoil=false,no_spread=false,no_weapon_motion=false;
    bool visibility_check=true,visible_only=false;
    bool chams=false;float chams_alpha=.28f,chams_range=80;int chams_limit=16;
    mango_chams::Appearance chams_appearance;
    bool third_person=false;float camera_distance=2.5f;bool custom_model=false;
    mango_extra::Settings extra;
    mango_reference::Options reference;
};
}
