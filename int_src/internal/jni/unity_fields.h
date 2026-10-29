#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mango_unity {
// Read a named instance field from the live, bounded Il2CppClass field table.
// Used only when a local model changes, never to enumerate the managed heap.
template<class Read>uintptr_t reference_field(uintptr_t object,const char* wanted,Read read){
    auto pointer=[&](uintptr_t at){uintptr_t p=0;if(at)read(at,&p,sizeof(p));return p;};
    uintptr_t klass=object?pointer(object):0;
    const size_t length=std::strlen(wanted);if(length>63)return 0;
    for(unsigned depth=0;klass&&depth<16;++depth){
        const uintptr_t fields=pointer(klass+0x80);uint32_t instance_size=0;
        read(klass+0xf8,&instance_size,sizeof(instance_size));
        if(fields&&instance_size>=0x18&&instance_size<=0x100000){
            struct Field {uintptr_t name,type,parent;int32_t offset;uint32_t token;};
            static_assert(sizeof(Field)==32,"IL2CPP ARM64 FieldInfo");
            for(unsigned i=0;i<256;++i){
                Field f{};if(!read(fields+i*sizeof(Field),&f,sizeof(f))||f.parent!=klass||!f.name||!f.type)break;
                if(f.offset<0x10||uint32_t(f.offset)>instance_size-sizeof(uintptr_t))continue;
                char name[64]{};
                if(read(f.name,name,length+1)&&!std::memcmp(name,wanted,length+1))return pointer(object+size_t(f.offset));
            }
        }
        const uintptr_t parent=pointer(klass+0x58);if(parent==klass)break;klass=parent;
    }
    return 0;
}
}
