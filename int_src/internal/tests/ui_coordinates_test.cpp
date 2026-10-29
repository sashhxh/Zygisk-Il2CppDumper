#include "ui_coordinates.h"
#include <cassert>
#include <limits>
int main(){
    mango_input::Point point;
    using mango_input::Origin;using mango_input::to_ui;
    assert(to_ui(100,24,2800,1260,2800,1260,Origin::TopLeft,point)&&std::fabs(point.x-100)<.001f&&std::fabs(point.y-24)<.001f);
    assert(to_ui(200,2472,5600,2520,2800,1260,Origin::BottomLeft,point)&&std::fabs(point.x-100)<.001f&&std::fabs(point.y-24)<.001f);
    assert(to_ui(0,1260,2800,1260,2800,1260,Origin::BottomLeft,point)&&point.x==0&&point.y==0);
    assert(to_ui(2800,0,2800,1260,2800,1260,Origin::BottomLeft,point)&&point.x==2800&&point.y==1260);
    // Invalid dimensions/values must not move the pointer or capture a finger.
    const auto prior=point;
    assert(!to_ui(10,20,0,100,200,200,Origin::TopLeft,point));
    assert(!to_ui(10,20,100,100,0,200,Origin::TopLeft,point));
    assert(!to_ui(std::numeric_limits<float>::quiet_NaN(),20,100,100,200,200,Origin::TopLeft,point));
    assert(!to_ui(10,std::numeric_limits<float>::infinity(),100,100,200,200,Origin::BottomLeft,point));
    assert(point.x==prior.x&&point.y==prior.y);
}
