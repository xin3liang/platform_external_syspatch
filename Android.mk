LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := syspatch_host
LOCAL_SRC_FILES := syspatch.c main.c
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += external/xdelta3
LOCAL_C_INCLUDES += external/lzma/xz-embedded
LOCAL_STATIC_LIBRARIES := libxz_host libxdelta3_host
include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := libsyspatch
LOCAL_SRC_FILES := syspatch.c
LOCAL_C_INCLUDES += external/xdelta3
LOCAL_C_INCLUDES += external/lzma/xz-embedded
LOCAL_STATIC_LIBRARIES := libxz libxdelta3
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
#LOCAL_ADDRESS_SANITIZER := true
LOCAL_MODULE := syspatch
LOCAL_SRC_FILES := main.c
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libsyspatch libxz libxdelta3
include $(BUILD_EXECUTABLE)
