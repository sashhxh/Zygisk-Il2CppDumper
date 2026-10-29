#pragma once
#include <cstdint>
#include <cstring>

namespace mango_graphics {
struct Metadata {
    using Read=bool(*)(uintptr_t,void*,size_t);
    using Lookup=void*(*)(const char*);
    Read read=nullptr;
    const void*(*methods)(void*,void**)=nullptr;
    const char*(*name)(const void*)=nullptr;
    uint32_t(*flags)(const void*,uint32_t*)=nullptr;
    bool(*executable)(uintptr_t)=nullptr;
    bool load(Lookup lookup,Read reader){
        read=reader;
        methods=reinterpret_cast<decltype(methods)>(lookup("mono_class_get_methods"));
        name=reinterpret_cast<decltype(name)>(lookup("mono_method_get_name"));
        flags=reinterpret_cast<decltype(flags)>(lookup("mono_method_get_flags"));
        return methods&&name&&flags&&read;
    }
    template<class T>T field(void* p,size_t offset)const{T v{};if(p)read(reinterpret_cast<uintptr_t>(p)+offset,&v,sizeof(v));return v;}
    void* call(void* instance,const char* wanted)const{
        if(!methods||!instance)return nullptr;
        try{
        void* klass=field<void*>(instance,0);
        for(unsigned depth=0;klass&&depth<32;++depth){
            void* iterator=nullptr;
            for(unsigned i=0;i<512;++i){
                const void* method=methods(klass,&iterator);if(!method)break;
                const char* text=name(method);char buffer[64]{};const size_t n=std::strlen(wanted)+1;
                if(!text||n>sizeof(buffer)||!read(reinterpret_cast<uintptr_t>(text),buffer,n)||std::memcmp(buffer,wanted,n))continue;
                uint32_t implementation=0;if(flags(method,&implementation)&(0x10|0x400))return nullptr;
                const uintptr_t code=field<uintptr_t>(const_cast<void*>(method),0);
                if(!code||!executable||!executable(code))return nullptr;
                return reinterpret_cast<void*(*)(void*,const void*)>(code)(instance,method);
            }
            void* parent=field<void*>(klass,0x58);if(parent==klass)break;klass=parent;
        }
        }catch(...){return nullptr;}
        return nullptr;
    }
    void* type_of(void* object)const{return call(object,"GetType");}
    void* base_type(void* type)const{return call(type,"get_BaseType");}
};
}
