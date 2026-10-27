LOCAL_PATH := $(call my-dir)

IMGUI := imgui

MANGO_JNI_PATH := $(LOCAL_PATH)
include $(LOCAL_PATH)/dobby-source/Android.mk
LOCAL_PATH := $(MANGO_JNI_PATH)

include $(CLEAR_VARS)
LOCAL_MODULE     := cfesp
LOCAL_SRC_FILES  := main.cpp bypass.cpp \
    $(IMGUI)/imgui.cpp $(IMGUI)/imgui_draw.cpp $(IMGUI)/imgui_tables.cpp $(IMGUI)/imgui_widgets.cpp \
    $(IMGUI)/backends/imgui_impl_opengl3.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/dobby-source/include $(LOCAL_PATH)/$(IMGUI) $(LOCAL_PATH)/$(IMGUI)/backends
LOCAL_CPPFLAGS   := -std=c++17 -fexceptions -fvisibility=hidden -O2 -DIMGUI_IMPL_OPENGL_ES3
LOCAL_STATIC_LIBRARIES := dobby
LOCAL_LDLIBS     := -llog -lEGL -lGLESv3 -ldl
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE     := cfinject
LOCAL_SRC_FILES  := inject.cpp
LOCAL_CPPFLAGS   := -std=c++17 -O2
LOCAL_LDLIBS     := -ldl
include $(BUILD_EXECUTABLE)
