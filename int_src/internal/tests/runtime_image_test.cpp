#include "runtime_loader.h"
#include <cassert>
#include <dlfcn.h>
#include <limits>
int main(int argc,char** argv){
    assert(argc==2);
    void* library=dlopen(argv[1],RTLD_NOW);assert(library);
    void* code=dlsym(library,"fixture_version");assert(code);
    Dl_info info{};assert(dladdr(code,&info));
    const auto base=reinterpret_cast<uintptr_t>(info.dli_fbase);
    const auto rva=reinterpret_cast<uintptr_t>(code)-base;
    unsigned char bytes[16]{};std::memcpy(bytes,code,sizeof(bytes));
    const mango_runtime::Resolver::Signature anchor{rva,bytes,sizeof(bytes)};
    unsigned char build_id[20]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
    unsigned char wrong_id[20]={};
    const char* name=std::strrchr(argv[1],'/');name=name?name+1:argv[1];
    mango_runtime::Resolver bad_id,bad_name,bad_code,good;
    assert(!bad_id.select_named_image(name,wrong_id,sizeof(wrong_id),&anchor,1));
    assert(!bad_name.select_named_image("not-installed.so",build_id,sizeof(build_id),&anchor,1));
    unsigned char changed[16]{};std::memcpy(changed,bytes,sizeof(bytes));changed[0]^=0xff;
    const mango_runtime::Resolver::Signature corrupted{rva,changed,sizeof(changed)};
    assert(!bad_code.select_named_image(name,build_id,sizeof(build_id),&corrupted,1));
    assert(good.select_named_image(name,build_id,sizeof(build_id),&anchor,1));
    assert(good.base()==base);assert(good.executable(reinterpret_cast<uintptr_t>(code)));
    dlclose(library);
}
