#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstddef>
namespace mango_chams {
struct Color {float r,g,b,a;};
enum Style {Flat=0,Soft,Pulse,Rainbow};
inline constexpr int custom_preset=7;
inline constexpr const char* preset_names="Classic\0Ice\0Toxic\0Ghost\0Solid\0Pulse\0Rainbow\0Custom\0";
inline constexpr const char* style_names="Flat\0Soft\0Pulse\0Rainbow\0";
struct Appearance {
    int preset=0,style=Flat;
    float speed=1;
    float rgb[3][3]={{.12f,.83f,1},{1,.18f,.28f},{.65f,.68f,.78f}};
};
inline float bounded(float value,float fallback,float lo,float hi){return std::isfinite(value)?std::clamp(value,lo,hi):fallback;}
inline void sanitize(Appearance& a){
    a.preset=a.preset>=0&&a.preset<=custom_preset?a.preset:custom_preset;
    a.style=a.style>=Flat&&a.style<=Rainbow?a.style:Flat;
    a.speed=bounded(a.speed,1,.25f,3.f);
    const Appearance defaults;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)a.rgb[i][j]=bounded(a.rgb[i][j],defaults.rgb[i][j],0,1);
}
inline void apply_preset(Appearance& a,float& opacity,int preset){
    if(preset<0||preset>=custom_preset){a.preset=custom_preset;return;}
    a=Appearance{};a.preset=preset;opacity=.28f;
    auto palette=[&](std::array<float,3> visible,std::array<float,3> hidden){
        for(unsigned i=0;i<3;++i){a.rgb[0][i]=visible[i];a.rgb[1][i]=hidden[i];}
    };
    switch(preset){
        case 1:palette({.35f,.85f,1},{.65f,.35f,1});opacity=.38f;break;
        case 2:palette({.35f,1,.10f},{1,.35f,.06f});opacity=.45f;break;
        case 3:palette({.72f,.88f,1},{.85f,.60f,1});a.style=Soft;opacity=.45f;break;
        case 4:palette({.16f,.90f,.68f},{1,.24f,.38f});opacity=1;break;
        case 5:a.style=Pulse;opacity=.60f;break;
        case 6:a.style=Rainbow;opacity=.50f;break;
        default:break;
    }
}
inline Color hue(float h,float alpha){
    const float v=h*6;const int sector=int(v);const float f=v-sector,p=.15f,q=1-f*.85f,t=.15f+f*.85f;
    switch(sector%6){
        case 0:return {1,t,p,alpha};case 1:return {q,1,p,alpha};case 2:return {p,1,t,alpha};
        case 3:return {p,q,1,alpha};case 4:return {t,p,1,alpha};default:return {1,p,q,alpha};
    }
}
inline std::array<Color,3> palette(const Appearance& source,float opacity,double time){
    Appearance a=source;sanitize(a);float alpha=bounded(opacity,.28f,.05f,1);
    if(!std::isfinite(time))time=0;
    if(a.style==Soft)alpha*=.45f;
    if(a.style==Pulse)alpha*=float(.8+.2*std::sin(std::fmod(time,1.0/a.speed)*6.283185307179586*a.speed));
    std::array<Color,3> colors{};
    for(unsigned i=0;i<3;++i)colors[i]={a.rgb[i][0],a.rgb[i][1],a.rgb[i][2],alpha};
    if(a.style==Rainbow){
        float h=float(std::fmod(time*.12*a.speed,1.0));if(h<0)h+=1;
        colors[0]=hue(h,alpha);colors[1]=hue(std::fmod(h+.5f,1.f),alpha);
        // Unknown visibility stays identifiable, rather than guessing a state.
    }
    return colors;
}
inline bool same(const std::array<Color,3>& a,const std::array<Color,3>& b){
    for(unsigned i=0;i<3;++i)if(a[i].r!=b[i].r||a[i].g!=b[i].g||a[i].b!=b[i].b||a[i].a!=b[i].a)return false;
    return true;
}
// Appended tagged record keeps existing MANGO_ESP_V1 files compatible.
inline bool load_appearance(FILE* file,Appearance& out){
    Appearance a;
    if(std::fscanf(file," CHAMS_STYLE_V1 %d %d %f %f %f %f %f %f %f %f %f %f",
        &a.preset,&a.style,&a.speed,&a.rgb[0][0],&a.rgb[0][1],&a.rgb[0][2],
        &a.rgb[1][0],&a.rgb[1][1],&a.rgb[1][2],&a.rgb[2][0],&a.rgb[2][1],&a.rgb[2][2])!=12)return false;
    sanitize(a);out=a;return true;
}
inline bool save_appearance(char* text,size_t size,const Appearance& source){
    Appearance a=source;sanitize(a);
    const int n=std::snprintf(text,size,"CHAMS_STYLE_V1 %d %d %.3f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
        a.preset,a.style,a.speed,a.rgb[0][0],a.rgb[0][1],a.rgb[0][2],
        a.rgb[1][0],a.rgb[1][1],a.rgb[1][2],a.rgb[2][0],a.rgb[2][1],a.rgb[2][2]);
    return n>=0&&size_t(n)<size;
}
}
