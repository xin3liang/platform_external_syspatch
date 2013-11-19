LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := syspatch_host
LOCAL_SRC_FILES := syspatch.c
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libxz_host libxdelta3_host
LOCAL_C_INCLUDES += external/xdelta
LOCAL_C_INCLUDES += external/lzma/xz-embedded
include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)
#LOCAL_ADDRESS_SANITIZER := true
LOCAL_MODULE := syspatch
LOCAL_SRC_FILES := syspatch.c
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libxz libxdelta3
LOCAL_C_INCLUDES += external/xdelta
LOCAL_C_INCLUDES += external/lzma/xz-embedded
include $(BUILD_EXECUTABLE)
