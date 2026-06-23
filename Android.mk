LOCAL_PATH := $(call my-dir)


# ARMPatch

include $(CLEAR_VARS)
LOCAL_MODULE := armpatch
LOCAL_SRC_FILES := obj/local/$(TARGET_ARCH_ABI)/libarmpatch.a
include $(PREBUILT_STATIC_LIBRARY)


# ThirdParty libraries

include $(CLEAR_VARS)
LOCAL_MODULE := gloss
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libGlossHook.a
include $(PREBUILT_STATIC_LIBRARY)


# AML libraries

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION     := .cpp .cc
LOCAL_SHARED_LIBRARIES  := armpatch gloss
LOCAL_MODULE            := AML
LOCAL_SRC_FILES         := main.cpp src/interface.cpp src/aml.cpp src/modpaks.cpp src/signal.cpp \
                           src/modslist.cpp src/icfg.cpp src/vtable_hooker.cpp src/alog.cpp src/mls.cpp \
                           mod/logger.cpp mod/config.cpp

 ## FLAGS ##
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG -D__AML -DNO_HOOKDEFINES -DFASTMAN92_CODE -std=c17 -mthumb
LOCAL_CXXFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG -D__AML -DNO_HOOKDEFINES -DFASTMAN92_CODE -std=c++17 -mthumb -fexceptions
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include $(LOCAL_PATH)/AML_PrecompiledLibs/include
LOCAL_LDLIBS += -llog -ldl -landroid

# Uncomment these two lines to add IL2CPP support! (NOT WORKING)
#    LOCAL_SRC_FILES += il2cpp/gc.cpp il2cpp/functions.cpp
#    LOCAL_CFLAGS += -D__IL2CPPUTILS
# Uncomment these two lines to add IL2CPP support! (NOT WORKING)

 ## BUILD ##
include $(BUILD_SHARED_LIBRARY)