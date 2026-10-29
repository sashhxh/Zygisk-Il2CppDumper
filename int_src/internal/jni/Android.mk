LOCAL_PATH := $(call my-dir)

IMGUI := imgui

MANGO_JNI_PATH := $(LOCAL_PATH)
include $(LOCAL_PATH)/dobby-source/Android.mk
LOCAL_PATH := $(MANGO_JNI_PATH)

include $(CLEAR_VARS)
LOCAL_MODULE     := cfesp
LOCAL_SRC_FILES  := main.cpp vulkan_overlay.cpp vulkan_hooks.cpp model_bundle.S \
    $(IMGUI)/imgui.cpp $(IMGUI)/imgui_draw.cpp $(IMGUI)/imgui_tables.cpp $(IMGUI)/imgui_widgets.cpp \
    $(IMGUI)/backends/imgui_impl_vulkan.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/dobby-source/include $(LOCAL_PATH)/$(IMGUI) $(LOCAL_PATH)/$(IMGUI)/backends
LOCAL_CPPFLAGS   := -std=c++17 -fexceptions -fvisibility=hidden -O2 -DVK_NO_PROTOTYPES -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
LOCAL_ASFLAGS    := -I$(LOCAL_PATH)
LOCAL_STATIC_LIBRARIES := dobby
LOCAL_LDLIBS     := -llog -ldl -lz
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE     := cfinject
LOCAL_SRC_FILES  := inject.cpp
LOCAL_CPPFLAGS   := -std=c++17 -O2
LOCAL_LDLIBS     := -ldl
include $(BUILD_EXECUTABLE)
