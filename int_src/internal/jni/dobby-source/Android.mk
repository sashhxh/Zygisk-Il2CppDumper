# Official Dobby 21f07a5ce184cda9346afb8009c07fe7e47390d0.
# ndk-build port: compile Dobby with the same NDK/libc++ as the application.
# Source list and includes mirror upstream CMake's verified arm64 static target.
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := dobby
LOCAL_SRC_FILES := \
    source/core/arch/CpuFeature.cc \
    source/core/arch/CpuRegister.cc \
    source/core/assembler/assembler.cc \
    source/core/assembler/assembler-arm.cc \
    source/core/assembler/assembler-arm64.cc \
    source/core/assembler/assembler-ia32.cc \
    source/core/assembler/assembler-x64.cc \
    source/core/codegen/codegen-arm.cc \
    source/core/codegen/codegen-arm64.cc \
    source/core/codegen/codegen-ia32.cc \
    source/core/codegen/codegen-x64.cc \
    source/MemoryAllocator/CodeBuffer/CodeBufferBase.cc \
    source/MemoryAllocator/AssemblyCodeBuilder.cc \
    source/MemoryAllocator/MemoryAllocator.cc \
    source/InstructionRelocation/arm/InstructionRelocationARM.cc \
    source/InstructionRelocation/arm64/InstructionRelocationARM64.cc \
    source/InstructionRelocation/x86/InstructionRelocationX86.cc \
    source/InstructionRelocation/x86/InstructionRelocationX86Shared.cc \
    source/InstructionRelocation/x64/InstructionRelocationX64.cc \
    source/InstructionRelocation/x86/x86_insn_decode/x86_insn_decode.c \
    source/InterceptRouting/InterceptRouting.cpp \
    source/TrampolineBridge/Trampoline/arm/trampoline_arm.cc \
    source/TrampolineBridge/Trampoline/arm64/trampoline_arm64.cc \
    source/TrampolineBridge/Trampoline/x86/trampoline_x86.cc \
    source/TrampolineBridge/Trampoline/x64/trampoline_x64.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/common_bridge_handler.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/arm/helper_arm.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/arm/closure_bridge_arm.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/arm/ClosureTrampolineARM.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/arm64/helper_arm64.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/arm64/ClosureTrampolineARM64.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/x86/helper_x86.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/x86/closure_bridge_x86.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/x86/ClosureTrampolineX86.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/x64/helper_x64.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/x64/closure_bridge_x64.cc \
    source/TrampolineBridge/ClosureTrampolineBridge/x64/ClosureTrampolineX64.cc \
    source/InterceptRouting/Routing/InstructionInstrument/InstructionInstrument.cc \
    source/InterceptRouting/Routing/InstructionInstrument/RoutingImpl.cc \
    source/InterceptRouting/Routing/InstructionInstrument/instrument_routing_handler.cc \
    source/InterceptRouting/Routing/FunctionInlineHook/FunctionInlineHook.cc \
    source/InterceptRouting/Routing/FunctionInlineHook/RoutingImpl.cc \
    source/InterceptRouting/RoutingPlugin/RoutingPlugin.cc \
    source/dobby.cpp \
    source/Interceptor.cpp \
    source/InterceptEntry.cpp \
    source/Backend/UserMode/PlatformUtil/Linux/ProcessRuntimeUtility.cc \
    source/Backend/UserMode/UnifiedInterface/platform-posix.cc \
    source/Backend/UserMode/ExecMemory/code-patch-tool-posix.cc \
    source/Backend/UserMode/ExecMemory/clear-cache-tool-all.c \
    source/InterceptRouting/RoutingPlugin/NearBranchTrampoline/near_trampoline_arm64.cc \
    source/InterceptRouting/RoutingPlugin/NearBranchTrampoline/NearBranchTrampoline.cc \
    source/MemoryAllocator/NearMemoryAllocator.cc \
    external/logging/logging.cc \
    builtin-plugin/SymbolResolver/elf/dobby_symbol_resolver.cc
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/source \
    $(LOCAL_PATH)/source/dobby \
    $(LOCAL_PATH)/external \
    $(LOCAL_PATH)/external/logging \
    $(LOCAL_PATH)/builtin-plugin \
    $(LOCAL_PATH)/source/Backend/UserMode \
    $(LOCAL_PATH)/builtin-plugin/SymbolResolver
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CPP_FEATURES := exceptions
LOCAL_CFLAGS := -O3 -DANDROID -DNDEBUG -fomit-frame-pointer \
    -ffunction-sections -fdata-sections -fvisibility=hidden \
    -D__DOBBY_BUILD_VERSION__=\"Dobby-21f07a5-ndk-source\"
LOCAL_CPPFLAGS := -fno-rtti -fvisibility-inlines-hidden
LOCAL_EXPORT_LDLIBS := -llog -ldl
include $(BUILD_STATIC_LIBRARY)
