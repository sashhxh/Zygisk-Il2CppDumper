#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mango_reference {
struct Options {
    bool dark=false,separators=false,fps=false,russian=false;
    bool names=false,health=false,target_arrow=false;
    float thickness=1.f;
    float name_color[4]={1,1,1,1},skeleton_color[4]={1,1,1,1};
    float tracer_color[4]={1,1,1,1},distance_color[4]={1,1,1,1};
};
inline void load_options(FILE* file,Options& out) {
    Options next;int dark,separators,fps,names,health,arrow,russian;
    if(std::fscanf(file," REFERENCE_UI_V1 %d %d %d %d %d %d %d %f",
        &dark,&separators,&fps,&names,&health,&arrow,&russian,&next.thickness)!=8)return;
    next.dark=dark!=0;next.separators=separators!=0;next.fps=fps!=0;
    next.names=names!=0;next.health=health!=0;next.target_arrow=arrow!=0;
    next.russian=russian!=0;
    if(!std::isfinite(next.thickness))return;
    next.thickness=std::clamp(next.thickness,.5f,4.f);
    for(auto* color:{next.name_color,next.skeleton_color,next.tracer_color,next.distance_color})
        for(int i=0;i<4;++i){
            if(std::fscanf(file," %f",color+i)!=1||!std::isfinite(color[i]))return;
            color[i]=std::clamp(color[i],0.f,1.f);
        }
    out=next;
}
inline bool save_options(char* text,std::size_t size,const Options& opt) {
    int used=std::snprintf(text,size,"REFERENCE_UI_V1 %d %d %d %d %d %d %d %.3f",
        opt.dark,opt.separators,opt.fps,opt.names,opt.health,opt.target_arrow,opt.russian,opt.thickness);
    if(used<0||std::size_t(used)>=size)return false;
    for(const auto* color:{opt.name_color,opt.skeleton_color,opt.tracer_color,opt.distance_color})
        for(int i=0;i<4;++i){
            int n=std::snprintf(text+used,size-used," %.4f",color[i]);
            if(n<0||std::size_t(n)>=size-used)return false;
            used+=n;
        }
    if(std::size_t(used)+2>size)return false;
    text[used++]='\n';text[used]=0;return true;
}
}
