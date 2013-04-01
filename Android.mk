ifeq ($(call is-vendor-board-platform,QCOM),true)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_JNI_SHARED_LIBRARIES := libqcomfm_jni

LOCAL_MODULE:= qcom.fmradio

include $(BUILD_JAVA_LIBRARY)

include $(LOCAL_PATH)/jni/Android.mk
endif # is-vendor-board-platform
