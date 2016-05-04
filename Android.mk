ifeq ($(BOARD_HAVE_QCOM_FM),true)

LOCAL_PATH:= $(call my-dir)
LOCAL_DIR_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

#ifneq ($(TARGET_USES_AOSP),true)

LOCAL_SRC_FILES := $(call all-java-files-under, qcom/fmradio)
LOCAL_JNI_SHARED_LIBRARIES := libqcomfm_jni

LOCAL_MODULE:= qcom.fmradio

LOCAL_ADDITIONAL_DEPENDENCIES := qcom.fmradio.xml

include $(BUILD_JAVA_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := qcom.fmradio.xml
LOCAL_SRC_FILES := qcom/fmradio/$(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/permissions

include $(BUILD_PREBUILT)


include $(LOCAL_PATH)/jni/Android.mk
LOCAL_PATH := $(LOCAL_DIR_PATH)
include $(LOCAL_PATH)/fmapp2/Android.mk
#LOCAL_PATH := $(LOCAL_DIR_PATH)
#include $(LOCAL_PATH)/FMRecord/Android.mk

#endif # Not (TARGET_USES_AOSP)

LOCAL_PATH := $(LOCAL_DIR_PATH)
include $(LOCAL_PATH)/libfm_jni/Android.mk

endif # BOARD_HAVE_QCOM_FM
