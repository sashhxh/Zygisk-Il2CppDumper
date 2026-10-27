#pragma once
#include <cstdint>
namespace mango_unity {
// Unity 6000.3 object-return bindings return a tagged ScriptingObjectHandle,
// not a native Unity Object*. This mirrors D3EC4A0-D3EC4BC in the pinned image.
template<class ResolveRef,class ResolveTagged,class ReadPointer>
uintptr_t decode_object(uintptr_t handle,ResolveRef&& ref,ResolveTagged&& tagged,ReadPointer&& read){
    if(!handle)return 0;
    return handle&1?tagged(handle):read(ref(handle));
}
struct RayBudget {
    unsigned remaining=8;
    bool exhausted=false;
    bool take(){if(!remaining){exhausted=true;return false;}--remaining;return true;}
};
} // namespace mango_unity
