#include "aim_prediction.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace mango_prediction;
// Independent vector simulation follows the verified game assembly's state
// update, and interpolates the collision segment at the solved arrival time.
static V3 simulate(V3 position,V3 velocity,V3 acceleration,float drag,float time){
    constexpr float step=1.f/32;
    while(time>0){V3 next=mul(add(velocity,mul(acceleration,step)),std::clamp(1-drag*step,0.f,1.f));
        velocity=mul(add(velocity,next),.5f);float dt=std::min(time,step);position=add(position,mul(velocity,dt));time-=dt;}
    return position;
}
int main(){
    Motion motion;motion.update({0,0,0},1);motion.update({.2f,0,0},1.04);assert(std::fabs(motion.velocity(1.04).x-5)<.001f);
    motion.update({.4f,0,0},1.08);assert(std::fabs(motion.velocity(1.08).x-5)<.001f);
    motion.update({100,0,0},1.1);assert(length(motion.velocity(1.1))==0);assert(length(motion.velocity(2))==0);
    motion.update({100.2f,0,0},1.14);motion.update({100.2f,0,0},1.18);motion.update({100.2f,0,0},1.22);assert(length(motion.velocity(1.22))==0);
    for(float acceleration:{0.f,-9.81f})for(float meters:{50.f,150.f,300.f,600.f})for(float drag:{0.f,.05f,.3f})for(V3 velocity:{V3{},V3{5,0,0},V3{0,0,7},V3{-3,2,0}}){
        V3 origin{0,2,0},target{0,2,meters};Ballistics b{550,drag,{0,acceleration,0},{2,0,0}};Solution solution;
        assert(solve(origin,target,velocity,b,solution));
        if(acceleration==0&&velocity.y==0)assert(std::fabs(solution.point.y-target.y)<.0001f);
        V3 direction=sub(solution.point,origin);direction=mul(direction,1/length(direction));
        V3 hit=simulate(origin,add(mul(direction,b.speed),b.inherited),b.acceleration,b.drag,solution.seconds);
        V3 future=add(target,mul(velocity,solution.seconds));
        if(length(sub(hit,future))>.02f)std::fprintf(stderr,"prediction error %.4f at %.0fm drag %.2f\n",length(sub(hit,future)),meters,drag);
        assert(length(sub(hit,future))<.02f);
    }
    Solution solution;Ballistics straight{500,0,{},{}};
    assert(solve({0,0,0},{0,0,100},{10,0,0},straight,solution));assert(solution.point.x>2&&solution.seconds>.2f);
    assert(!solve({0,0,0},{0,0,100},{0,0,50},Ballistics{10,0,{},{}},solution));
    assert(!solve({},{0,0,100},{},Ballistics{0,0,{},{}},solution));
    assert(!solve({},{0,0,100},{},Ballistics{500,std::numeric_limits<float>::quiet_NaN(),{},{}},solution));
    assert(attack_offset(0)==0x1E8&&attack_offset(1)==0x1E8&&attack_offset(2)==0x1F0);
    AutomaticAttack input;int presses=0,holds=0;
    auto invoke=[&](size_t offset){if(offset==0x1E8)++presses;else {assert(offset==0x1F0);++holds;}return true;};
    assert(input.fire(1,2,invoke)&&presses==1&&holds==1);
    assert(input.fire(1,2,invoke)&&presses==1&&holds==2);
    input.reset();assert(input.fire(1,2,invoke)&&presses==2&&holds==3);
    assert(input.fire(2,2,invoke)&&presses==3&&holds==4);
    assert(input.fire(2,0,invoke)&&presses==4&&holds==4);
    assert(hold(true,false,10.1,10)&&!hold(true,true,10.1,10)&&!hold(false,false,10.1,10)&&!hold(true,false,10.1,10.2));
    std::puts("aim_prediction: PASS (96 trajectories <2cm; zero drop gives no extra vertical aim offset, motion reset, full-auto and pause gates)");
}
