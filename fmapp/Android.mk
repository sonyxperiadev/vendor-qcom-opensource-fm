ifeq ($(BOARD_HAVE_QCOM_FM),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src/com/codeaurora/fmradio) \
        src/com/codeaurora/fmradio/IFMRadioServiceCallbacks.aidl \
        src/com/codeaurora/fmradio/IFMRadioService.aidl \
        src/com/codeaurora/fmradio/IFMTransmitterServiceCallbacks.aidl \
        src/com/codeaurora/fmradio/IFMTransmitterService.aidl \


ifeq ($(call is-platform-sdk-version-at-least,11),true)
LOCAL_SRC_FILES +=  $(call all-java-files-under, src/com/codeaurora/hc_utils)
else
LOCAL_SRC_FILES +=  $(call all-java-files-under, src/com/codeaurora/utils)
endif
LOCAL_PACKAGE_NAME := FM
LOCAL_CERTIFICATE := platform
LOCAL_JNI_SHARED_LIBRARIES := libqcomfm_jni
LOCAL_JAVA_LIBRARIES := qcom.fmradio
include $(BUILD_PACKAGE)

endif
