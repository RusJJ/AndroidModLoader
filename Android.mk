LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := substrate
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
	LOCAL_SRC_FILES := libsubstrate-armv7a.a # Cydia Substrate
else
	ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
		LOCAL_SRC_FILES := libsubstrate-armv8a.a # And64InlineHook
	endif
endif
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SHARED_LIBRARIES := substrate
LOCAL_MODULE    := armpatch
LOCAL_SRC_FILES := ARMPatch.cpp
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SHARED_LIBRARIES := armpatch
LOCAL_MODULE    := AML
LOCAL_SRC_FILES := main.cpp aml.cpp interface.cpp modslist.cpp icfg.cpp mod/logger.cpp mod/config.cpp
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG -D__AML -std=c++17
LOCAL_C_INCLUDES += ./include
LOCAL_LDLIBS += -llog
# Uncomment these two lines to add IL2CPP support! (NOT WORKING)
#LOCAL_SRC_FILES += il2cpp/gc.cpp il2cpp/functions.cpp
#LOCAL_CFLAGS += -D__IL2CPPUTILS
# Uncomment these two lines to add IL2CPP support! (NOT WORKING)
include $(BUILD_SHARED_LIBRARY)