#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace mango_unity {
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
