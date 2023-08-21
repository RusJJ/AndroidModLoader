LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := armpatch
LOCAL_SRC_FILES := obj/local/$(TARGET_ARCH_ABI)/libarmpatch.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := substrate
LOCAL_SRC_FILES := obj/local/$(TARGET_ARCH_ABI)/libsubstrate.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := dobby
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libdobby.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := gloss
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libGlossHook.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libz
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libz.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := wolfssl
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libwolfssl.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := curl
LOCAL_SHARED_LIBRARIES := libz wolfssl
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libcurl.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION     := .cpp .cc
LOCAL_SHARED_LIBRARIES  := armpatch substrate curl dobby gloss
LOCAL_MODULE            := AML
LOCAL_SRC_FILES         := main.cpp interface.cpp aml.cpp modpaks.cpp \
                           modslist.cpp icfg.cpp vtable_hooker.cpp \
                           mod/logger.cpp mod/config.cpp

 ## FLAGS ##
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG -D__AML -DNO_HOOKDEFINES -std=c17 -mthumb
LOCAL_CXXFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG -D__AML -DNO_HOOKDEFINES -std=c++17 -mthumb -fexceptions
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include $(LOCAL_PATH)/curl $(LOCAL_PATH)/curl/include $(LOCAL_PATH)/wolfssl
LOCAL_LDLIBS += -llog -ldl -landroid

# Uncomment these two lines to add IL2CPP support! (NOT WORKING)
#    LOCAL_SRC_FILES += il2cpp/gc.cpp il2cpp/functions.cpp
#    LOCAL_CFLAGS += -D__IL2CPPUTILS
# Uncomment these two lines to add IL2CPP support! (NOT WORKING)

 ## BUILD ##
include $(BUILD_SHARED_LIBRARY)
