#include "unity_return.h"
#include <cassert>
int main(){
    int refs=0,tags=0,reads=0;
    auto ref=[&](uintptr_t value){assert(value==0x100);++refs;return uintptr_t(0x200);};
    auto tag=[&](uintptr_t value){assert(value==0x101);++tags;return uintptr_t(0x400);};
    auto read=[&](uintptr_t value){assert(value==0x200);++reads;return uintptr_t(0x300);};
    assert(mango_unity::decode_object(0,ref,tag,read)==0&&refs==0&&tags==0&&reads==0);
    assert(mango_unity::decode_object(0x100,ref,tag,read)==0x300&&refs==1&&reads==1);
    assert(mango_unity::decode_object(0x101,ref,tag,read)==0x400&&tags==1&&reads==1);
    mango_unity::RayBudget budget;
    for(unsigned i=0;i<8;++i)assert(budget.take());
    assert(!budget.take()&&budget.exhausted&&budget.remaining==0);
}
