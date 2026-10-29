#include "graphics_metadata.h"
#include <array>
#include <cassert>
#include <map>
#include <vector>
struct Method {uintptr_t code;const char* name;uint32_t flags=0;};
struct Object {void* klass;};
std::array<unsigned char,0x60> leaf{},base{},runtime{};
std::map<void*,std::vector<Method*>> table;
Object type{runtime.data()},parent_type{runtime.data()};
void* get_type(void* self,const void*){assert(self);return &type;}
void* get_base(void* self,const void*){assert(self==&type);return &parent_type;}
const void* methods(void* cls,void** iterator){const uintptr_t i=reinterpret_cast<uintptr_t>(*iterator);auto& list=table[cls];if(i>=list.size())return nullptr;*iterator=reinterpret_cast<void*>(i+1);return list[i];}
const char* name(const void* m){return static_cast<const Method*>(m)->name;}
uint32_t flags(const void* m,uint32_t*){return static_cast<const Method*>(m)->flags;}
int main(){
    void* parent=base.data();std::memcpy(leaf.data()+0x58,&parent,sizeof(parent));
    Method type_method{reinterpret_cast<uintptr_t>(get_type),"GetType"},base_method{reinterpret_cast<uintptr_t>(get_base),"get_BaseType"};
    table[base.data()]={&type_method};table[runtime.data()]={&base_method};Object source{leaf.data()};
    mango_graphics::Metadata api;
    assert(api.load([](const char* n)->void*{if(!std::strcmp(n,"mono_class_get_methods"))return reinterpret_cast<void*>(methods);if(!std::strcmp(n,"mono_method_get_name"))return reinterpret_cast<void*>(name);return reinterpret_cast<void*>(flags);},[](uintptr_t p,void* out,size_t n){if(!p)return false;std::memcpy(out,reinterpret_cast<void*>(p),n);return true;}));
    api.executable=[](uintptr_t p){return p==reinterpret_cast<uintptr_t>(get_type)||p==reinterpret_cast<uintptr_t>(get_base);};
    assert(api.type_of(&source)==&type&&api.base_type(&type)==&parent_type);
    type_method.flags=0x400;assert(!api.type_of(&source));type_method.flags=0;type_method.code=1;assert(!api.type_of(&source));
    assert(!api.type_of(nullptr));
}
