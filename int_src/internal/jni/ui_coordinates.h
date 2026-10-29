#pragma once
#include <cmath>
namespace mango_input {
struct Point {float x=0,y=0;};
enum class Origin {TopLeft,BottomLeft};
// Android/ImGui use top-left coordinates; Unity Input uses bottom-left and
// may report a different resolution. Hit testing must use UI pixels as well.
inline bool to_ui(float x,float y,int source_width,int source_height,
                  int ui_width,int ui_height,Origin origin,Point& out){
    if(source_width<=0||source_height<=0||ui_width<=0||ui_height<=0||
       !std::isfinite(x)||!std::isfinite(y))return false;
    const float nx=x/float(source_width),ny=y/float(source_height);
    Point value{nx*ui_width,(origin==Origin::BottomLeft?1.f-ny:ny)*ui_height};
    if(!std::isfinite(value.x)||!std::isfinite(value.y))return false;
    out=value;return true;
}
}
