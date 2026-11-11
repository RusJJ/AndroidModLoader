LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
	LOCAL_MODULE := modtemplate
else
	LOCAL_MODULE := modtemplate64
endif
LOCAL_SRC_FILES := main.cpp mod/logger.cpp mod/config.cpp mod/flogger.cpp
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG -std=c++17
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_LDLIBS += -llog # logger.cpp needs it
include $(BUILD_SHARED_LIBRARY)