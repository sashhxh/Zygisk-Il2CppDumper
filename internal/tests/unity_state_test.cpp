#include "unity_renderer_state.h"
#include "unity_fields.h"
#include <array>
#include <cassert>
#include <vector>
#include <utility>
#include <string>

int main(){
    std::array<uint32_t,40> code{};
    code[0]=0x72001c3f;code[1]=0x1a9f07e1;code[2]=0x1400000e;
    const uint32_t body[]={0xa9bf4ffe,0x3944e009,0x12000028,0x6b08013f,0x54000300,0xb9419809,0xaa0003f3,0x3904e008};
    std::memcpy(code.data()+16,body,sizeof(body));
    const uintptr_t start=reinterpret_cast<uintptr_t>(code.data());
    auto read_code=[&](uintptr_t at,void* out,size_t n){if(at<start||n>sizeof(code)||at-start>sizeof(code)-n)return false;std::memcpy(out,reinterpret_cast<void*>(at),n);return true;};
    assert(mango_unity::force_rendering_offset(start,read_code)==0x138);
    code[16]^=1;assert(mango_unity::force_rendering_offset(start,read_code)==-1);code[16]^=1;
    code[2]=0x17ffffff;assert(mango_unity::force_rendering_offset(start,read_code)==-1);
    assert(mango_unity::force_rendering_offset(start+10000,read_code)==-1);
    // Live Class fields may be absent, inherited, or corrupt. Invalid
    // offsets / owners must not become an unchecked Unity API argument.
    std::array<unsigned char,0x130> klass{},parent{};
    std::array<uintptr_t,8> object{};
    struct Field {uintptr_t name,type,parent;int32_t offset;uint32_t token;};
    std::array<Field,2> fields{};
    const std::string name="body";
    std::vector<std::pair<uintptr_t,size_t>> ranges;
    auto allow=[&](const void* p,size_t n){ranges.emplace_back(reinterpret_cast<uintptr_t>(p),n);};
    allow(klass.data(),klass.size());allow(parent.data(),parent.size());allow(object.data(),sizeof(object));allow(fields.data(),sizeof(fields));allow(name.c_str(),name.size()+1);
    auto read=[&](uintptr_t at,void* out,size_t n){for(auto r:ranges)if(at>=r.first&&n<=r.second&&at-r.first<=r.second-n){std::memcpy(out,reinterpret_cast<void*>(at),n);return true;}return false;};
    auto put=[](void* into,const auto& v){std::memcpy(into,&v,sizeof(v));};
    const uintptr_t cls=reinterpret_cast<uintptr_t>(klass.data()),base=reinterpret_cast<uintptr_t>(parent.data()),table=reinterpret_cast<uintptr_t>(fields.data()),obj=reinterpret_cast<uintptr_t>(object.data());
    object[0]=cls;object[5]=0x12345678;
    put(klass.data()+0x58,base);put(parent.data()+0x80,table);const uint32_t instance_size=sizeof(object);put(parent.data()+0xf8,instance_size);
    fields[0]={reinterpret_cast<uintptr_t>(name.c_str()),1,base,40,0};
    assert(mango_unity::reference_field(obj,"body",read)==object[5]);
    assert(mango_unity::reference_field(obj,"headModel",read)==0);
    fields[0].offset=-1;assert(mango_unity::reference_field(obj,"body",read)==0);
    fields[0].offset=0x10000;assert(mango_unity::reference_field(obj,"body",read)==0);
    fields[0].offset=40;fields[0].parent=cls;assert(mango_unity::reference_field(obj,"body",read)==0);
    put(parent.data()+0x58,base);assert(mango_unity::reference_field(obj,"body",read)==0);
}
