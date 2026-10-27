/*
 * Oxide 1.15.11978 / Android ARM64 / IL2CPP -- partial client bypass.
 * Targets and ABI come from the user's arm64/dump.cs and il2cpp.h.
 *
 * Integration (Unity MAIN thread, after IL2CPP and scenes are ready):
 *   oxide_bypass_install(1);   // ACTk; returns native patch count, or < 0
 *   oxide_bypass_tick();       // call after scene loads / about once a second
 *   oxide_bypass_restore();   // restore native code; detectors remain disabled
 *
 * Flag 2 additionally suppresses SecurityRenderFeature.AddRenderPasses.
 * Missing render feedback may itself be detected. Flag 2 is NOT the default.
 * No Dobby, hard-coded module-base fallback, constructor or loader is used.
 * The caller must keep this library loaded and serialize calls on Unity's main
 * thread. Install/restore at a quiescent point, with target methods not running.
 *
 * Build with the Android NDK:
 *   $CXX -std=c++17 -O2 -fPIC -shared bypass.cpp -o libbypass.so -ldl -llog
 *
 * This does not bypass server movement/AOI, NativeGuard or Play Integrity.
 * The supplied ZIP has no libil2cpp.so: native bodies and device behavior have
 * NOT been verified. Metadata identity is required before any instruction write.
 */

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <pthread.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include "runtime_loader.h"

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#define EXPORT extern "C" __attribute__((visibility("default")))

namespace oxide {
constexpr uint32_t kActk = 1;
constexpr uint32_t kDepth = 2;
constexpr uint32_t kRet = 0xD65F03C0;  // AArch64: ret x30
constexpr const char* kNamespace = "CodeStage.AntiCheat.Detectors";
enum Error : int {
    Unsupported = -1, NotReady = -2, MetadataMismatch = -3,
    AmbiguousCode = -4, WriteFailed = -5, WrongThread = -6,
    ManagedFailure = -7, BadFlags = -8, Busy = -9
};

void log(const char* format, ...) {
    va_list args;
    va_start(args, format);
#if defined(__ANDROID__)
    __android_log_vprint(ANDROID_LOG_INFO, "OxideBypass", format, args);
#else
    std::fputs("[OxideBypass] ", stderr);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
#endif
    va_end(args);
}

struct MethodInfo;
struct Class;
struct Type;
struct Field;
struct Object;
struct Array;
using GCHandle = void*; // Unity 6 il2cpp-api-types.h: opaque pointer-sized handle.

struct Api {
    bool ready = false;
    mango_runtime::Resolver resolver;
    void* (*domain_get)() = nullptr;
    const void** (*domain_get_assemblies)(void*, size_t*) = nullptr;
    const void* (*assembly_get_image)(const void*) = nullptr;
    Class* (*class_from_name)(const void*, const char*, const char*) = nullptr;
    size_t (*image_get_class_count)(const void*) = nullptr;
    Class* (*image_get_class)(const void*, size_t) = nullptr;
    const MethodInfo* (*class_get_methods)(Class*, void**) = nullptr;
    const char* (*method_get_name)(const MethodInfo*) = nullptr;
    uint32_t (*method_get_param_count)(const MethodInfo*) = nullptr;
    const Type* (*method_get_return_type)(const MethodInfo*) = nullptr;
    const Type* (*method_get_param)(const MethodInfo*, uint32_t) = nullptr;
    uint32_t (*method_get_flags)(const MethodInfo*, uint32_t*) = nullptr;
    bool (*method_is_generic)(const MethodInfo*) = nullptr;
    bool (*method_is_inflated)(const MethodInfo*) = nullptr;
    int (*type_get_type)(const Type*) = nullptr;
    bool (*type_is_byref)(const Type*) = nullptr;
    Class* (*class_from_type)(const Type*) = nullptr;
    const Type* (*class_get_type)(Class*) = nullptr;
    Object* (*type_get_object)(const Type*) = nullptr;
    Object* (*runtime_invoke)(const MethodInfo*, void*, void**, Object**) = nullptr;
    Field* (*class_get_field_from_name)(Class*, const char*) = nullptr;
    size_t (*field_get_offset)(Field*) = nullptr;
    const Type* (*field_get_type)(Field*) = nullptr;
    void (*field_set_value)(Object*, Field*, void*) = nullptr;
    uint32_t (*array_length)(Array*) = nullptr;
    GCHandle (*gchandle_new)(Object*, bool) = nullptr;
    Object* (*gchandle_get_target)(GCHandle) = nullptr;
    void (*gchandle_free)(GCHandle) = nullptr;

    bool load() {
        if (ready) return true;
        if (!resolver.initialize()) { log("Loaded runtime API table unavailable"); return false; }
        bool ok = true;
#define LOAD(name) do { \
        name = reinterpret_cast<decltype(name)>(resolver.get("il2cpp_" #name)); \
        if (!name) { log("Missing export: il2cpp_%s", #name); ok = false; } \
    } while (false)
        LOAD(domain_get); LOAD(domain_get_assemblies); LOAD(assembly_get_image);
        LOAD(class_from_name); LOAD(image_get_class_count); LOAD(image_get_class);
        LOAD(class_get_methods); LOAD(method_get_name); LOAD(method_get_param_count);
        LOAD(method_get_return_type); LOAD(method_get_param); LOAD(method_get_flags);
        LOAD(method_is_generic); LOAD(method_is_inflated); LOAD(type_get_type);
        LOAD(type_is_byref); LOAD(class_from_type); LOAD(class_get_type);
        LOAD(type_get_object); LOAD(runtime_invoke); LOAD(class_get_field_from_name);
        LOAD(field_get_offset); LOAD(field_get_type); LOAD(field_set_value);
        LOAD(array_length); LOAD(gchandle_new); LOAD(gchandle_get_target);
        LOAD(gchandle_free);
#undef LOAD
        if (!ok) return false;
        ready = true;
        return true;
    }
};

struct Segment { uintptr_t begin, end; };
struct Module {
    uintptr_t base = 0;
    std::vector<Segment> executable;
    unsigned matches = 0;
    bool contains(uintptr_t address, size_t length) const {
        for (const auto& s : executable)
            if (address >= s.begin && address < s.end && length <= s.end - address)
                return true;
        return false;
    }
    static int visit(dl_phdr_info* info, size_t, void* context) {
        auto& out = *static_cast<Module*>(context);
        const char* path = info->dlpi_name;
        if (!path) return 0;
        const char* slash = std::strrchr(path, '/');
        const char* name = slash ? slash + 1 : path;
        if (std::strcmp(name, "libil2cpp.so") != 0) return 0;
        ++out.matches;
        out.base = info->dlpi_addr; // ELF load bias, NOT first /proc/maps address.
        for (ElfW(Half) i = 0; i < info->dlpi_phnum; ++i) {
            const auto& h = info->dlpi_phdr[i];
            if (h.p_type == PT_LOAD && (h.p_flags & PF_X) && (h.p_flags & PF_R))
                out.executable.push_back({out.base + h.p_vaddr,
                                          out.base + h.p_vaddr + h.p_memsz});
        }
        return 0;
    }
    bool find() {
        base = 0; matches = 0; executable.clear();
        dl_iterate_phdr(visit, this);
        return matches == 1 && !executable.empty();
    }
};

// First member of MethodInfo is methodPointer in the supplied il2cpp.h:117.
uintptr_t pointer(const MethodInfo* method) {
    uintptr_t result = 0;
    if (method) std::memcpy(&result, method, sizeof(result));
    return result;
}

struct Target {
    const char* ns;
    const char* klass;
    const char* method;
    uintptr_t rva;
    uint32_t feature;
    uint32_t params;
};

// All entries are confirmed INSTANCE void methods in this exact dump.
constexpr Target kTargets[] = {
    {kNamespace, "ACTkDetectorBase", "OnEnable", 0x5E2E2C8, kActk, 0},
    {kNamespace, "ACTkDetectorBase", "Start", 0x5E2EC88, kActk, 0},
    {kNamespace, "InjectionDetector", "jdJ", 0x5E2F630, kActk, 0},
    {kNamespace, "ObscuredCheatingDetector", "jdJ", 0x5E30324, kActk, 0},
    {kNamespace, "SpeedHackDetector", "jdJ", 0x5E32C4C, kActk, 0},
    {kNamespace, "SpeedHackDetector", "Update", 0x5E33800, kActk, 0},
    {kNamespace, "TimeCheatingDetector", "jdJ", 0x5E36EA4, kActk, 0},
    {kNamespace, "TimeCheatingDetector", "Update", 0x5E34F74, kActk, 0},
    {kNamespace, "WallHackDetector", "jdJ", 0x5E45378, kActk, 0},
    {kNamespace, "WallHackDetector", "Update", 0x5E3CE64, kActk, 0},
    {kNamespace, "WallHackDetector", "FixedUpdate", 0x5E3D598, kActk, 0},
    {kNamespace, "WallHackDetector", "ShootRaycastModule", 0x5E40748, kActk, 0},
    {kNamespace, "WallHackDetector", "ShootWireframeModule", 0x5E4293C, kActk, 0},
    {"Oxide.Features.Rendering.Security", "SecurityRenderFeature", "AddRenderPasses",
     0x721531C, kDepth, 2},
};

struct Patch {
    const Target* target;
    const MethodInfo* method;
    uintptr_t entry;
    uintptr_t site;
    uint32_t original;
    bool written = false;
    bool protected_dirty = false;
};

Api api;
Module module;
std::vector<const void*> images;
std::vector<Patch> patches;
pthread_t owner{};
bool has_owner = false;
bool busy = false;
uint32_t active_features = 0;
Field* auto_start = nullptr;
Field* action = nullptr;
Field* event_listener = nullptr;
std::vector<Field*> time_callbacks;
Class* detector_base = nullptr;
const MethodInfo* find_objects = nullptr;
const MethodInfo* stop_coroutines = nullptr;
const MethodInfo* cancel_invokes = nullptr;
const MethodInfo* set_enabled = nullptr;

// These APIs are intentionally single-threaded. Managed OnDisable callbacks may
// reenter; reject reentry instead of holding a mutex across runtime_invoke.
struct CallGuard {
    int error = 0;
    bool entered = false;
    CallGuard() {
        if (has_owner && !pthread_equal(owner, pthread_self())) error = WrongThread;
        else if (busy) error = Busy;
        else { busy = true; entered = true; }
    }
    ~CallGuard() { if (entered) busy = false; }
};

Class* klass(const char* ns, const char* name) {
    Class* found = nullptr;
    for (const void* image : images) {
        Class* candidate = api.class_from_name(image, ns, name);
        if (candidate && found && candidate != found) return nullptr;
        if (candidate) found = candidate;
    }
    return found;
}

const MethodInfo* method(Class* cls, const char* name, uint32_t count) {
    if (!cls) return nullptr;
    const MethodInfo* found = nullptr;
    void* iterator = nullptr;
    while (const MethodInfo* m = api.class_get_methods(cls, &iterator)) {
        const char* candidate = api.method_get_name(m);
        if (candidate && std::strcmp(candidate, name) == 0 &&
            api.method_get_param_count(m) == count) {
            if (found) return nullptr; // Same-arity overloads need explicit types.
            found = m;
        }
    }
    return found;
}

bool valid_method(const MethodInfo* m, uint32_t count, bool is_static,
                  uintptr_t expected_rva, bool returns_void = true) {
    if (!m || api.method_get_param_count(m) != count ||
        api.method_is_generic(m) || api.method_is_inflated(m)) return false;
    uint32_t impl_flags = 0;
    const uint32_t flags = api.method_get_flags(m, &impl_flags);
    if ((flags & 0x400u) || bool(flags & 0x10u) != is_static) return false;
    if (returns_void && api.type_get_type(api.method_get_return_type(m)) != 1)
        return false; // IL2CPP_TYPE_VOID
    uintptr_t entry = pointer(m);
    return entry >= module.base && entry - module.base == expected_rva &&
           !(entry & 3u) && module.contains(entry, 4);
}

bool depth_signature(const MethodInfo* m) {
    const Type* renderer = api.method_get_param(m, 0);
    const Type* data = api.method_get_param(m, 1);
    return renderer && data && !api.type_is_byref(renderer) &&
           api.type_is_byref(data) &&
           api.class_from_type(renderer) ==
               klass("UnityEngine.Rendering.Universal", "ScriptableRenderer") &&
           api.class_from_type(data) ==
               klass("UnityEngine.Rendering.Universal", "RenderingData");
}

Field* field(Class* cls, const char* name, size_t offset, int type) {
    Field* f = cls ? api.class_get_field_from_name(cls, name) : nullptr;
    return f && api.field_get_offset(f) == offset &&
           api.type_get_type(api.field_get_type(f)) == type ? f : nullptr;
}

bool prepare_cleanup() {
    detector_base = klass(kNamespace, "ACTkDetectorBase");
    auto_start = field(detector_base, "autoStart", 0x20, 2); // bool
    action = field(detector_base, "rWR", 0x28, 0x12); // System.Action (class)
    event_listener = field(detector_base, "detectionEventHasListener", 0x38, 2);
    find_objects = method(klass("UnityEngine", "Object"), "FindObjectsOfType", 2);
    stop_coroutines = method(klass("UnityEngine", "MonoBehaviour"), "StopAllCoroutines", 0);
    cancel_invokes = method(klass("UnityEngine", "MonoBehaviour"), "CancelInvoke", 0);
    set_enabled = method(klass("UnityEngine", "Behaviour"), "set_enabled", 1);
    bool ok = auto_start && action && event_listener &&
        valid_method(find_objects, 2, true, 0xD4C3628, false) &&
        valid_method(stop_coroutines, 0, false, 0xD4BEF18) &&
        valid_method(cancel_invokes, 0, false, 0xD4BDF50) &&
        valid_method(set_enabled, 1, false, 0xD4B5BE4);
    if (!ok) return false;
    if (api.type_get_type(api.method_get_return_type(find_objects)) != 0x1D ||
        api.type_get_type(api.method_get_param(find_objects, 1)) != 2 ||
        api.class_from_type(api.method_get_param(find_objects, 0)) !=
            klass("System", "Type") ||
        api.type_get_type(api.method_get_param(set_enabled, 0)) != 2)
        return false;
    time_callbacks.clear();
    Class* time = klass(kNamespace, "TimeCheatingDetector");
    const char* names[] = {"rhh", "rRv", "rRs", "rRy"};
    const size_t offsets[] = {0x40, 0x80, 0x98, 0xA0};
    for (size_t i = 0; i < 4; ++i) {
        Field* f = field(time, names[i], offsets[i], 0x12);
        // Action<ErrorKind> is GENERICINST rather than CLASS in some runtimes.
        if (!f) f = field(time, names[i], offsets[i], 0x15);
        if (!f) return false;
        time_callbacks.push_back(f);
    }
    return true;
}

bool invoke(const MethodInfo* m, Object* obj, void** args, Object** result = nullptr) {
    Object* exception = nullptr;
    Object* value = api.runtime_invoke(m, obj, args, &exception);
    if (exception) {
        log("Managed exception in %s", api.method_get_name(m));
        return false;
    }
    if (result) *result = value;
    return true;
}

// Object[] uses object header + bounds + max_length + aligned pointer vector.
// The supplied il2cpp.h uses 64-bit pointers and il2cpp_array_size_t=uintptr_t.
struct ObjectArrayLayout {
    void* klass;
    void* monitor;
    void* bounds;
    uintptr_t max_length;
};
static_assert(sizeof(ObjectArrayLayout) == 4 * sizeof(void*), "Array ABI");

int cleanup() {
    if (!(active_features & kActk)) return 0;
    Class* time = klass(kNamespace, "TimeCheatingDetector");
    const char* classes[] = {"InjectionDetector", "ObscuredCheatingDetector",
                            "SpeedHackDetector", "TimeCheatingDetector", "WallHackDetector"};
    int stopped = 0;
    for (const char* name : classes) {
        Class* cls = klass(kNamespace, name);
        if (!cls) return ManagedFailure;
        Object* type = api.type_get_object(api.class_get_type(cls));
        if (!type) return ManagedFailure;
        bool include_inactive = true;
        void* arguments[] = {type, &include_inactive};
        Object* result = nullptr;
        if (!invoke(find_objects, nullptr, arguments, &result) || !result)
            return ManagedFailure;
        // Keep the result array alive across managed calls and callbacks.
        const GCHandle handle = api.gchandle_new(result, false);
        if (!handle) return ManagedFailure;
        auto* array = reinterpret_cast<Array*>(result);
        const size_t length = api.array_length(array);
        if (length > 4096) { api.gchandle_free(handle); return ManagedFailure; }
        bool ok = true;
        for (size_t i = 0; i < length; ++i) {
            auto* root = api.gchandle_get_target(handle);
            if (!root) { ok = false; break; }
            Object* obj = nullptr;
            const auto* slot = reinterpret_cast<const unsigned char*>(root) +
                               sizeof(ObjectArrayLayout) + i * sizeof(Object*);
            std::memcpy(&obj, slot, sizeof(obj));
            if (!obj) continue;
            bool disabled = false;
            Object* null_callback = nullptr;
            api.field_set_value(obj, auto_start, &disabled);
            api.field_set_value(obj, event_listener, &disabled);
            api.field_set_value(obj, action, &null_callback);
            if (cls == time)
                for (Field* f : time_callbacks) api.field_set_value(obj, f, &null_callback);
            // Disabling a MonoBehaviour alone does not stop its coroutines.
            ok = invoke(stop_coroutines, obj, nullptr) && ok;
            ok = invoke(cancel_invokes, obj, nullptr) && ok;
            void* disable_args[] = {&disabled};
            ok = invoke(set_enabled, obj, disable_args) && ok;
            ++stopped;
        }
        api.gchandle_free(handle);
        if (!ok) return ManagedFailure;
    }
    return stopped;
}

bool is_bti(uint32_t instruction) {
    return instruction == 0xD503245F || instruction == 0xD503249F ||
           instruction == 0xD50324DF; // bti c / j / jc
}

int prepare(uint32_t features, std::vector<Patch>& plan) {
    for (const Target& target : kTargets) {
        if (!(features & target.feature)) continue;
        const MethodInfo* m = method(klass(target.ns, target.klass), target.method, target.params);
        if (!valid_method(m, target.params, false, target.rva) ||
            (target.feature == kDepth && !depth_signature(m))) {
            log("Metadata/RVA mismatch: %s.%s", target.klass, target.method);
            return MetadataMismatch;
        }
        const uintptr_t entry = pointer(m);
        uint32_t first = 0;
        std::memcpy(&first, reinterpret_cast<const void*>(entry), 4);
        // PACIASP/PACIBSP can also be BTI landing pads. Replacing the landing
        // instruction breaks BTI; preserving it followed by RET leaves LR
        // signed. Do not guess a PAC-aware replacement without native bodies.
        if (first == 0xD503233F || first == 0xD503237F) {
            log("Unsupported PAC entry: %s.%s", target.klass, target.method);
            return MetadataMismatch;
        }
        const uintptr_t site = entry + (is_bti(first) ? 4 : 0);
        if (!module.contains(site, 4)) return MetadataMismatch;
        uint32_t original = 0;
        std::memcpy(&original, reinterpret_cast<const void*>(site), 4);
        if (original == kRet) {
            log("Already returns: %s.%s", target.klass, target.method);
            continue;
        }
        plan.push_back({&target, m, entry, site, original, false, false});
    }
    // Reject aliases and entry points inside a BTI+RET patch range. A four-byte
    // patch never steals a neighboring prologue as a longer detour could.
    for (const void* image : images) {
        const size_t count = api.image_get_class_count(image);
        for (size_t i = 0; i < count; ++i) {
            Class* cls = api.image_get_class(image, i);
            if (!cls) continue;
            void* iterator = nullptr;
            while (const MethodInfo* m = api.class_get_methods(cls, &iterator)) {
                const uintptr_t p = pointer(m);
                for (const Patch& patch : plan) {
                    if (m != patch.method && p >= patch.entry && p < patch.site + 4) {
                        log("Shared/overlapping native entry: %s.%s", patch.target->klass,
                            patch.target->method);
                        return AmbiguousCode;
                    }
                }
            }
        }
    }
    return 0;
}

// Return false on write OR permission restoration failure. 'changed' lets the
// caller track bytes already written even when the final mprotect fails.
bool write_instruction(Patch& patch, uint32_t instruction) {
    const long page_value = sysconf(_SC_PAGESIZE);
    if (page_value <= 0) return false;
    const uintptr_t page_size = static_cast<uintptr_t>(page_value);
    const uintptr_t page = patch.site - (patch.site % page_size);
    // Loaded executable PT_LOAD is expected RX. Refuse other mappings in
    // production rather than inventing their original permissions.
    FILE* maps = std::fopen("/proc/self/maps", "r");
    if (!maps) return false;
    char line[1024];
    bool rx = false;
    while (std::fgets(line, sizeof(line), maps)) {
        unsigned long long start = 0, end = 0;
        char permissions[5]{};
        if (std::sscanf(line, "%llx-%llx %4s", &start, &end, permissions) == 3 &&
            patch.site >= start && patch.site + 4 <= end) {
            rx = permissions[0] == 'r' && permissions[2] == 'x' &&
                 (permissions[1] == '-' || patch.protected_dirty);
            break;
        }
    }
    std::fclose(maps);
    if (!rx) return false;
    // Some Android policies prohibit writable executable mappings. Propagate
    // that failure; no SELinux changes or fallback remote writes are attempted.
    if (mprotect(reinterpret_cast<void*>(page), page_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        log("mprotect writable failed: %d", errno);
        return false;
    }
    patch.protected_dirty = true;
    __atomic_store_n(reinterpret_cast<uint32_t*>(patch.site), instruction, __ATOMIC_SEQ_CST);
    __builtin___clear_cache(reinterpret_cast<char*>(patch.site),
                            reinterpret_cast<char*>(patch.site + 4));
    patch.written = instruction == kRet;
    if (mprotect(reinterpret_cast<void*>(page), page_size, PROT_READ | PROT_EXEC) != 0) {
        log("mprotect RX restore failed: %d", errno);
        return false;
    }
    patch.protected_dirty = false;
    return true;
}

bool rollback() {
    // A partial restore is no longer an installed configuration. Retained patch
    // records block reinstall until restore succeeds, and tick must not run.
    active_features = 0;
    bool ok = true;
    for (auto it = patches.rbegin(); it != patches.rend(); ++it) {
        if (!it->written && !it->protected_dirty) continue;
        uint32_t current = 0;
        std::memcpy(&current, reinterpret_cast<void*>(it->site), 4);
        if (current != kRet && current != it->original) {
            log("Restore conflict: %s.%s", it->target->klass, it->target->method);
            ok = false;
            continue;
        }
        if (!write_instruction(*it, it->original)) ok = false;
    }
    if (ok) { patches.clear(); active_features = 0; }
    return ok;
}
} // namespace oxide

EXPORT int oxide_bypass_install(uint32_t flags) {
    using namespace oxide;
#if !defined(__aarch64__) && !defined(OXIDE_BYPASS_TEST)
    (void)flags;
    return Unsupported;
#else
    CallGuard guard;
    if (guard.error) return guard.error;
    if (!flags) flags = kActk;
    if (flags & ~(kActk | kDepth)) return BadFlags;
    if (active_features) {
        if (active_features != flags) return BadFlags;
        return static_cast<int>(patches.size());
    }
    if (!patches.empty()) return WriteFailed; // restore pending partial writes
    if (!api.load()) return NotReady;
    module.base = api.resolver.base();
    module.executable.clear();
    for (const auto& range : api.resolver.module().loads)
        if (range.executable) module.executable.push_back({range.start, range.end});
    if (module.executable.empty()) return NotReady;
    void* domain = api.domain_get();
    if (!domain) return NotReady;
    size_t count = 0;
    const void** assemblies = api.domain_get_assemblies(domain, &count);
    if (!assemblies || !count) return NotReady;
    images.clear();
    for (size_t i = 0; i < count; ++i) {
        const void* image = api.assembly_get_image(assemblies[i]);
        if (image) images.push_back(image);
    }
    std::vector<Patch> plan;
    if ((flags & kActk) && !prepare_cleanup()) {
        log("Managed cleanup API or field layout mismatch");
        return MetadataMismatch;
    }
    const int prepared = prepare(flags, plan);
    if (prepared < 0) return prepared;
    // All metadata and alias checks complete BEFORE the first mutation.
    owner = pthread_self(); has_owner = true;
    patches = std::move(plan);
    for (Patch& patch : patches) {
        if (!write_instruction(patch, kRet)) {
            log("Patch failed: %s.%s; restoring", patch.target->klass, patch.target->method);
            rollback();
            return WriteFailed;
        }
        log("Patched %s.%s RVA 0x%lx", patch.target->klass, patch.target->method,
            static_cast<unsigned long>(patch.target->rva));
    }
    active_features = flags;
    const int cleaned = cleanup();
    if (cleaned < 0) {
        log("Detector cleanup incomplete; restoring native instructions");
        return rollback() ? ManagedFailure : WriteFailed;
    }
    log("Installed: %zu native patches, %d live ACTk components disabled",
        patches.size(), cleaned);
    return static_cast<int>(patches.size());
#endif
}

EXPORT int oxide_bypass_tick() {
    using namespace oxide;
    CallGuard guard;
    if (guard.error) return guard.error;
    if (!active_features) return NotReady;
    return cleanup();
}

EXPORT int oxide_bypass_restore() {
    using namespace oxide;
    CallGuard guard;
    if (guard.error) return guard.error;
    if (!rollback()) return WriteFailed;
    log("Native instructions restored. Existing component/field changes persist.");
    return 0;
}
