#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace mango_unity {
template<class Read>int camera_matrix_auto_offset(uintptr_t setter,Read read){
    uint32_t branch=0;if(!setter||!read(setter,&branch,sizeof(branch))||(branch&0xfc000000)!=0x14000000)return -1;
    int64_t displacement=branch&0x03ffffff;if(displacement&(1<<25))displacement-=int64_t(1)<<26;
    const uintptr_t target=uintptr_t(int64_t(setter)+displacement*4);uint32_t body[7]{};
    if(!read(target,body,sizeof(body)))return -1;
    const uint32_t view[]={0xad410021,0xad400c22,0x3914081f,0xad048001,0xad038c02,0xd65f03c0};
    const uint32_t cull[]={0xad410021,0x910bf008,0xad400c22,0x3914441f,0xad010101,0xad000d02,0xd65f03c0};
    if(!std::memcmp(body,view,sizeof(view)))return 0x502;
    if(!std::memcmp(body,cull,sizeof(cull)))return 0x511;
    return -1;
}
// Verified Renderer_CUSTOM_set_forceRenderingOff and its callee in the supplied
// Unity 6000.3.18f1 ARM64 player. Reject other implementations before reading
// native Renderer state; all writes still go through Unity's public setter.
template<class Read>int force_rendering_offset(uintptr_t setter,Read read){
    uint32_t entry[3]{};
    if(!setter||!read(setter,entry,sizeof(entry)))return -1;
    if(entry[0]!=0x72001c3f||entry[1]!=0x1a9f07e1||(entry[2]&0xfc000000)!=0x14000000)return -1;
    int64_t displacement=int64_t(entry[2]&0x03ffffff);
    if(displacement&(1<<25))displacement-=int64_t(1)<<26;
    const uintptr_t target=uintptr_t(int64_t(setter+8)+displacement*4);
    const uint32_t expected[]={0xa9bf4ffe,0x3944e009,0x12000028,0x6b08013f,0x54000300,0xb9419809,0xaa0003f3,0x3904e008};
    uint32_t body[8]{};
    return read(target,body,sizeof(body))&&!std::memcmp(body,expected,sizeof(body))?0x138:-1;
}
}
