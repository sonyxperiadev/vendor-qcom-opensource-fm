/*
 * Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *            notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *            notice, this list of conditions and the following disclaimer in the
 *            documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *            the names of its contributors may be used to endorse or promote
 *            products derived from this software without specific prior written
 *            permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "android_hardware_fm"

#include "jni.h"
#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"
#include "utils/Log.h"
#include "utils/misc.h"
#include "FmIoctlsInterface.h"
#include "ConfigFmThs.h"
#include <cutils/properties.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <media/tavarua.h>
#include <linux/videodev2.h>
#include <math.h>

#define RADIO "/dev/radio0"
#define FM_JNI_SUCCESS 0L
#define FM_JNI_FAILURE -1L
#define SEARCH_DOWN 0
#define SEARCH_UP 1
#define HIGH_BAND 2
#define LOW_BAND  1
#define CAL_DATA_SIZE 23
#define V4L2_CTRL_CLASS_USER 0x00980000
#define V4L2_CID_PRIVATE_IRIS_SET_CALIBRATION           (V4L2_CTRL_CLASS_USER + 0x92A)
#define V4L2_CID_PRIVATE_TAVARUA_ON_CHANNEL_THRESHOLD   (V4L2_CTRL_CLASS_USER + 0x92B)
#define V4L2_CID_PRIVATE_TAVARUA_OFF_CHANNEL_THRESHOLD  (V4L2_CTRL_CLASS_USER + 0x92C)
#define V4L2_CID_PRIVATE_IRIS_SET_SPURTABLE             (V4L2_CTRL_CLASS_USER + 0x92D)
#define TX_RT_LENGTH       63
#define WAIT_TIMEOUT 200000 /* 200*1000us */
#define TX_RT_DELIMITER    0x0d
#define PS_LEN    9
enum search_dir_t {
    SEEK_UP,
    SEEK_DN,
    SCAN_UP,
    SCAN_DN
};


using namespace android;

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_acquireFdNative
        (JNIEnv* env, jobject thiz, jstring path)
{
    int fd;
    int i, retval=0, err;
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    char versionStr[40] = {'\0'};
    int init_success = 0;
    jboolean isCopy;
    v4l2_capability cap;
    const char* radio_path = env->GetStringUTFChars(path, &isCopy);
    if(radio_path == NULL){
        return FM_JNI_FAILURE;
    }
    fd = open(radio_path, O_RDONLY, O_NONBLOCK);
    if(isCopy == JNI_TRUE){
        env->ReleaseStringUTFChars(path, radio_path);
    }
    if(fd < 0){
        return FM_JNI_FAILURE;
    }
    //Read the driver verions
    err = ioctl(fd, VIDIOC_QUERYCAP, &cap);

    ALOGD("VIDIOC_QUERYCAP returns :%d: version: %d \n", err , cap.version );

    if( err >= 0 ) {
       ALOGD("Driver Version(Same as ChipId): %x \n",  cap.version );
       /*Conver the integer to string */
       sprintf(versionStr, "%d", cap.version );
       property_set("hw.fm.version", versionStr);
    } else {
       return FM_JNI_FAILURE;
    }

    property_get("qcom.bluetooth.soc", value, NULL);

    ALOGD("BT soc is %s\n", value);

    if (strcmp(value, "rome") != 0)
    {
       /*Set the mode for soc downloader*/
       property_set("hw.fm.mode", "normal");
       /* Need to clear the hw.fm.init firstly */
       property_set("hw.fm.init", "0");
       property_set("ctl.start", "fm_dl");
       sched_yield();
       for(i=0; i<45; i++) {
         property_get("hw.fm.init", value, NULL);
         if (strcmp(value, "1") == 0) {
            init_success = 1;
            break;
         } else {
            usleep(WAIT_TIMEOUT);
         }
       }
       ALOGE("init_success:%d after %f seconds \n", init_success, 0.2*i);
       if(!init_success) {
         property_set("ctl.stop", "fm_dl");
         // close the fd(power down)
         close(fd);
         return FM_JNI_FAILURE;
       }
    }
    return fd;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_closeFdNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int i = 0;
    int cleanup_success = 0;
    char retval =0;
    char value[PROPERTY_VALUE_MAX] = {'\0'};

    property_get("qcom.bluetooth.soc", value, NULL);

    ALOGD("BT soc is %s\n", value);

    if (strcmp(value, "rome") != 0)
    {
       property_set("ctl.stop", "fm_dl");
    }
    close(fd);
    return FM_JNI_SUCCESS;
}

/********************************************************************
 * Current JNI
 *******************************************************************/

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getFreqNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;
    long freq;

    if (fd >= 0) {
        err = FmIoctlsInterface :: get_cur_freq(fd, freq);
        if(err < 0) {
           err = FM_JNI_FAILURE;
           ALOGE("%s: get freq failed\n", LOG_TAG);
        } else {
           err = freq;
        }
    } else {
        ALOGE("%s: get freq failed because fd is negative, fd: %d\n",
              LOG_TAG, fd);
        err = FM_JNI_FAILURE;
    }
    return err;
}

/*native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setFreqNative
    (JNIEnv * env, jobject thiz, jint fd, jint freq)
{
    int err;

    if ((fd >= 0) && (freq > 0)) {
        err = FmIoctlsInterface :: set_freq(fd, freq);
        if (err < 0) {
            ALOGE("%s: set freq failed, freq: %d\n", LOG_TAG, freq);
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        ALOGE("%s: set freq failed because either fd/freq is negative,\
              fd: %d, freq: %d\n", LOG_TAG, fd, freq);
        err = FM_JNI_FAILURE;
    }
    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setControlNative
    (JNIEnv * env, jobject thiz, jint fd, jint id, jint value)
{
    int err;
    ALOGE("id(%x) value: %x\n", id, value);

    if ((fd >= 0) && (id >= 0)) {
        err = FmIoctlsInterface :: set_control(fd, id, value);
        if (err < 0) {
            ALOGE("%s: set control failed, id: %d\n", LOG_TAG, id);
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        ALOGE("%s: set control failed because either fd/id is negavtive,\
               fd: %d, id: %d\n", LOG_TAG, fd, id);
        err = FM_JNI_FAILURE;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_SetCalibrationNative
     (JNIEnv * env, jobject thiz, jint fd, jbyteArray buff)
{

   int err;

   if (fd >= 0) {
       err = FmIoctlsInterface :: set_calibration(fd);
       if (err < 0) {
           ALOGE("%s: set calibration failed\n", LOG_TAG);
           err = FM_JNI_FAILURE;
       } else {
           err = FM_JNI_SUCCESS;
       }
   } else {
       ALOGE("%s: set calibration failed because fd is negative, fd: %d\n",
              LOG_TAG, fd);
       err = FM_JNI_FAILURE;
   }

   return err;
}
/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getControlNative
    (JNIEnv * env, jobject thiz, jint fd, jint id)
{
    int err;
    long val;

    ALOGE("id(%x)\n", id);

    if ((fd >= 0) && (id >= 0)) {
        err = FmIoctlsInterface :: get_control(fd, id, val);
        if (err < 0) {
            ALOGE("%s: get control failed, id: %d\n", LOG_TAG, id);
            err = FM_JNI_FAILURE;
        } else {
            err = val;
        }
    } else {
        ALOGE("%s: get control failed because either fd/id is negavtive,\
               fd: %d, id: %d\n", LOG_TAG, fd, id);
        err = FM_JNI_FAILURE;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_startSearchNative
    (JNIEnv * env, jobject thiz, jint fd, jint dir)
{
    int err;

    if ((fd >= 0) && (dir >= 0)) {
        ALOGD("startSearchNative: Issuing the VIDIOC_S_HW_FREQ_SEEK");
        err = FmIoctlsInterface :: start_search(fd, dir);
        if (err < 0) {
            ALOGE("%s: search failed, dir: %d\n", LOG_TAG, dir);
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        ALOGE("%s: search failed because either fd/dir is negative,\
               fd: %d, dir: %d\n", LOG_TAG, fd, dir);
        err = FM_JNI_FAILURE;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_cancelSearchNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;

    if (fd >= 0) {
        err = FmIoctlsInterface :: set_control(fd, V4L2_CID_PRV_SRCHON, 0);
        if (err < 0) {
            ALOGE("%s: cancel search failed\n", LOG_TAG);
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        ALOGE("%s: cancel search failed because fd is negative, fd: %d\n",
               LOG_TAG, fd);
        err = FM_JNI_FAILURE;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getRSSINative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;
    long rmssi;

    if (fd >= 0) {
        err = FmIoctlsInterface :: get_rmssi(fd, rmssi);
        if (err < 0) {
            ALOGE("%s: get rmssi failed\n", LOG_TAG);
            err = FM_JNI_FAILURE;
        } else {
            err = rmssi;
        }
    } else {
        ALOGE("%s: get rmssi failed because fd is negative, fd: %d\n",
               LOG_TAG, fd);
        err = FM_JNI_FAILURE;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setBandNative
    (JNIEnv * env, jobject thiz, jint fd, jint low, jint high)
{
    int err;

    if ((fd >= 0) && (low >= 0) && (high >= 0)) {
        err = FmIoctlsInterface :: set_band(fd, low, high);
        if (err < 0) {
            ALOGE("%s: set band failed, low: %d, high: %d\n",
                   LOG_TAG, low, high);
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        ALOGE("%s: set band failed because either fd/band is negative,\
               fd: %d, low: %d, high: %d\n", LOG_TAG, fd, low, high);
        err = FM_JNI_FAILURE;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getLowerBandNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;
    ULINT freq;

    if (fd >= 0) {
        err = FmIoctlsInterface :: get_lowerband_limit(fd, freq);
        if (err < 0) {
            ALOGE("%s: get lower band failed\n", LOG_TAG);
            err = FM_JNI_FAILURE;
        } else {
            err = freq;
        }
    } else {
        ALOGE("%s: get lower band failed because fd is negative,\
               fd: %d\n", LOG_TAG, fd);
        err = FM_JNI_FAILURE;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getUpperBandNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;
    ULINT freq;

    if (fd >= 0) {
        err = FmIoctlsInterface :: get_upperband_limit(fd, freq);
        if (err < 0) {
            ALOGE("%s: get lower band failed\n", LOG_TAG);
            err = FM_JNI_FAILURE;
        } else {
            err = freq;
        }
    } else {
        ALOGE("%s: get lower band failed because fd is negative,\
               fd: %d\n", LOG_TAG, fd);
        err = FM_JNI_FAILURE;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_setMonoStereoNative
    (JNIEnv * env, jobject thiz, jint fd, jint val)
{

    int err;

    if (fd >= 0) {
        err = FmIoctlsInterface :: set_audio_mode(fd, (enum AUDIO_MODE)val);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}


/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getBufferNative
 (JNIEnv * env, jobject thiz, jint fd, jbyteArray buff, jint index)
{
    int err;
    jboolean isCopy;
    jbyte *byte_buffer;

    if ((fd >= 0) && (index >= 0)) {
        byte_buffer = env->GetByteArrayElements(buff, &isCopy);
        err = FmIoctlsInterface :: get_buffer(fd,
                                               (char *)byte_buffer,
                                               STD_BUF_SIZE,
                                               index);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        }
        env->ReleaseByteArrayElements(buff, byte_buffer, 0);
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getRawRdsNative
 (JNIEnv * env, jobject thiz, jint fd, jbooleanArray buff, jint count)
{

    return (read (fd, buff, count));

}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setNotchFilterNative(JNIEnv * env, jobject thiz,jint fd, jint id, jboolean aValue)
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int init_success = 0,i;
    char notch[PROPERTY_VALUE_MAX] = {0x00};
    int band;
    int err = 0;

    property_get("qcom.bluetooth.soc", value, NULL);

    ALOGD("BT soc is %s\n", value);

    if (strcmp(value, "rome") != 0)
    {
       /*Enable/Disable the WAN avoidance*/
       property_set("hw.fm.init", "0");
       if (aValue)
          property_set("hw.fm.mode", "wa_enable");
       else
          property_set("hw.fm.mode", "wa_disable");

       property_set("ctl.start", "fm_dl");
       sched_yield();
       for(i=0; i<10; i++) {
          property_get("hw.fm.init", value, NULL);
          if (strcmp(value, "1") == 0) {
             init_success = 1;
             break;
          } else {
             usleep(WAIT_TIMEOUT);
          }
       }
       ALOGE("init_success:%d after %f seconds \n", init_success, 0.2*i);

       property_get("notch.value", notch, NULL);
       ALOGE("Notch = %s",notch);
       if (!strncmp("HIGH",notch,strlen("HIGH")))
           band = HIGH_BAND;
       else if(!strncmp("LOW",notch,strlen("LOW")))
           band = LOW_BAND;
       else
           band = 0;

       ALOGE("Notch value : %d", band);

        if ((fd >= 0) && (id >= 0)) {
            err = FmIoctlsInterface :: set_control(fd, id, band);
            if (err < 0) {
                err = FM_JNI_FAILURE;
            } else {
                err = FM_JNI_SUCCESS;
            }
        } else {
            err = FM_JNI_FAILURE;
        }
    }

    return err;
}


/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setAnalogModeNative(JNIEnv * env, jobject thiz, jboolean aValue)
{
    int i=0;
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    char firmwareVersion[80];

    property_get("qcom.bluetooth.soc", value, NULL);

    ALOGD("BT soc is %s\n", value);

    if (strcmp(value, "rome") != 0)
    {
       /*Enable/Disable Analog Mode FM*/
       property_set("hw.fm.init", "0");
       if (aValue) {
           property_set("hw.fm.isAnalog", "true");
       } else {
           property_set("hw.fm.isAnalog", "false");
       }
       property_set("hw.fm.mode","config_dac");
       property_set("ctl.start", "fm_dl");
       sched_yield();
       for(i=0; i<10; i++) {
          property_get("hw.fm.init", value, NULL);
          if (strcmp(value, "1") == 0) {
             return 1;
          } else {
             usleep(WAIT_TIMEOUT);
          }
       }
    }

    return 0;
}




/*
 * Interfaces added for Tx
*/

/*native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setPTYNative
    (JNIEnv * env, jobject thiz, jint fd, jint pty)
{
    int masked_pty;
    int err;

    ALOGE("->android_hardware_fmradio_FmReceiverJNI_setPTYNative\n");

    if (fd >= 0) {
        masked_pty = pty & MASK_PTY;
        err = FmIoctlsInterface :: set_control(fd,
                                                V4L2_CID_RDS_TX_PTY,
                                                masked_pty);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_setPINative
    (JNIEnv * env, jobject thiz, jint fd, jint pi)
{
    int err;
    int masked_pi;

    ALOGE("->android_hardware_fmradio_FmReceiverJNI_setPINative\n");

    if (fd >= 0) {
        masked_pi = pi & MASK_PI;
        err = FmIoctlsInterface :: set_control(fd,
                                                V4L2_CID_RDS_TX_PI,
                                                masked_pi);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_startRTNative
    (JNIEnv * env, jobject thiz, jint fd, jstring radio_text, jint count )
{
    ALOGE("->android_hardware_fmradio_FmReceiverJNI_startRTNative\n");

    struct v4l2_ext_control ext_ctl;
    struct v4l2_ext_controls v4l2_ctls;
    size_t len = 0;

    int err = 0;
    jboolean isCopy = false;
    char* rt_string1 = NULL;
    char* rt_string = (char*)env->GetStringUTFChars(radio_text, &isCopy);
    if(rt_string == NULL ){
        ALOGE("RT string is not valid \n");
        return FM_JNI_FAILURE;
    }
    len = strlen(rt_string);
    if (len > TX_RT_LENGTH) {
        ALOGE("RT string length more than max size");
        env->ReleaseStringUTFChars(radio_text, rt_string);
        return FM_JNI_FAILURE;
    }
    rt_string1 = (char*) malloc(TX_RT_LENGTH + 1);
    if (rt_string1 == NULL) {
       ALOGE("out of memory \n");
       env->ReleaseStringUTFChars(radio_text, rt_string);
       return FM_JNI_FAILURE;
    }
    memset(rt_string1, 0, TX_RT_LENGTH + 1);
    memcpy(rt_string1, rt_string, len);


    ext_ctl.id     = V4L2_CID_RDS_TX_RADIO_TEXT;
    ext_ctl.string = rt_string1;
    ext_ctl.size   = strlen(rt_string1) + 1;

    /* form the ctrls data struct */
    v4l2_ctls.ctrl_class = V4L2_CTRL_CLASS_FM_TX,
    v4l2_ctls.count      = 1,
    v4l2_ctls.controls   = &ext_ctl;


    err = ioctl(fd, VIDIOC_S_EXT_CTRLS, &v4l2_ctls );
    env->ReleaseStringUTFChars(radio_text, rt_string);
    if (rt_string1 != NULL) {
        free(rt_string1);
        rt_string1 = NULL;
    }
    if(err < 0){
        ALOGE("VIDIOC_S_EXT_CTRLS for start RT returned : %d\n", err);
        return FM_JNI_FAILURE;
    }

    ALOGD("->android_hardware_fmradio_FmReceiverJNI_startRTNative is SUCCESS\n");
    return FM_JNI_SUCCESS;
}

static jint android_hardware_fmradio_FmReceiverJNI_stopRTNative
    (JNIEnv * env, jobject thiz, jint fd )
{
    int err;

    ALOGE("->android_hardware_fmradio_FmReceiverJNI_stopRTNative\n");
    if (fd >= 0) {
        err = FmIoctlsInterface :: set_control(fd,
                                                V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT,
                                                0);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_startPSNative
    (JNIEnv * env, jobject thiz, jint fd, jstring buff, jint count )
{
    ALOGD("->android_hardware_fmradio_FmReceiverJNI_startPSNative\n");

    struct v4l2_ext_control ext_ctl;
    struct v4l2_ext_controls v4l2_ctls;
    int l;
    int err = 0;
    jboolean isCopy = false;
    char *ps_copy = NULL;
    const char *ps_string = NULL;

    ps_string = env->GetStringUTFChars(buff, &isCopy);
    if (ps_string != NULL) {
        l = strlen(ps_string);
        if ((l > 0) && ((l + 1) == PS_LEN)) {
             ps_copy = (char *)malloc(sizeof(char) * PS_LEN);
             if (ps_copy != NULL) {
                 memset(ps_copy, '\0', PS_LEN);
                 memcpy(ps_copy, ps_string, (PS_LEN - 1));
             } else {
                 env->ReleaseStringUTFChars(buff, ps_string);
                 return FM_JNI_FAILURE;
             }
        } else {
             env->ReleaseStringUTFChars(buff, ps_string);
             return FM_JNI_FAILURE;
        }
    } else {
        return FM_JNI_FAILURE;
    }

    env->ReleaseStringUTFChars(buff, ps_string);

    ext_ctl.id     = V4L2_CID_RDS_TX_PS_NAME;
    ext_ctl.string = ps_copy;
    ext_ctl.size   = PS_LEN;

    /* form the ctrls data struct */
    v4l2_ctls.ctrl_class = V4L2_CTRL_CLASS_FM_TX,
    v4l2_ctls.count      = 1,
    v4l2_ctls.controls   = &ext_ctl;

    err = ioctl(fd, VIDIOC_S_EXT_CTRLS, &v4l2_ctls);
    if (err < 0) {
        ALOGE("VIDIOC_S_EXT_CTRLS for Start PS returned : %d\n", err);
        free(ps_copy);
        return FM_JNI_FAILURE;
    }

    ALOGD("->android_hardware_fmradio_FmReceiverJNI_startPSNative is SUCCESS\n");
    free(ps_copy);

    return FM_JNI_SUCCESS;
}

static jint android_hardware_fmradio_FmReceiverJNI_stopPSNative
    (JNIEnv * env, jobject thiz, jint fd)
{

    int err;

    ALOGE("->android_hardware_fmradio_FmReceiverJNI_stopPSNative\n");

    if (fd >= 0) {
        err = FmIoctlsInterface :: set_control(fd,
                                                V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME,
                                                0);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_configureSpurTable
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err;

    ALOGD("->android_hardware_fmradio_FmReceiverJNI_configureSpurTable\n");

    if (fd >= 0) {
        err = FmIoctlsInterface :: set_control(fd,
                                                V4L2_CID_PRIVATE_UPDATE_SPUR_TABLE,
                                                0);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_setPSRepeatCountNative
    (JNIEnv * env, jobject thiz, jint fd, jint repCount)
{
    int masked_ps_repeat_cnt;
    int err;

    ALOGE("->android_hardware_fmradio_FmReceiverJNI_setPSRepeatCountNative\n");

    if (fd >= 0) {
        masked_ps_repeat_cnt = repCount & MASK_TXREPCOUNT;
        err = FmIoctlsInterface :: set_control(fd,
                                                V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT,
                                                masked_ps_repeat_cnt);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}

static jint android_hardware_fmradio_FmReceiverJNI_setTxPowerLevelNative
    (JNIEnv * env, jobject thiz, jint fd, jint powLevel)
{
    int err;

    ALOGE("->android_hardware_fmradio_FmReceiverJNI_setTxPowerLevelNative\n");

    if (fd >= 0) {
        err = FmIoctlsInterface :: set_control(fd,
                                                V4L2_CID_TUNE_POWER_LEVEL,
                                                powLevel);
        if (err < 0) {
            err = FM_JNI_FAILURE;
        } else {
            err = FM_JNI_SUCCESS;
        }
    } else {
        err = FM_JNI_FAILURE;
    }

    return err;
}

static void android_hardware_fmradio_FmReceiverJNI_configurePerformanceParams
    (JNIEnv * env, jobject thiz, jint fd)
{

     ConfigFmThs thsObj;

     thsObj.SetRxSearchAfThs(FM_PERFORMANCE_PARAMS, fd);
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setSpurDataNative
 (JNIEnv * env, jobject thiz, jint fd, jshortArray buff, jint count)
{
    ALOGE("entered JNI's setSpurDataNative\n");
    int err, i = 0;
    struct v4l2_ext_control ext_ctl;
    struct v4l2_ext_controls v4l2_ctls;
    uint8_t *data;
    short *spur_data = env->GetShortArrayElements(buff, NULL);
    if (spur_data == NULL) {
        ALOGE("Spur data is NULL\n");
        return FM_JNI_FAILURE;
    }
    data = (uint8_t *) malloc(count);
    if (data == NULL) {
        ALOGE("Allocation failed for data\n");
        return FM_JNI_FAILURE;
    }
    for(i = 0; i < count; i++)
        data[i] = (uint8_t) spur_data[i];

    ext_ctl.id = V4L2_CID_PRIVATE_IRIS_SET_SPURTABLE;
    ext_ctl.string = (char*)data;
    ext_ctl.size = count;
    v4l2_ctls.ctrl_class = V4L2_CTRL_CLASS_USER;
    v4l2_ctls.count   = 1;
    v4l2_ctls.controls  = &ext_ctl;

    err = ioctl(fd, VIDIOC_S_EXT_CTRLS, &v4l2_ctls );
    if (err < 0){
        ALOGE("Set ioctl failed\n");
        free(data);
        return FM_JNI_FAILURE;
    }
    free(data);
    return FM_JNI_SUCCESS;
}

/*
 * JNI registration.
 */
static JNINativeMethod gMethods[] = {
        /* name, signature, funcPtr */
        { "acquireFdNative", "(Ljava/lang/String;)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_acquireFdNative},
        { "closeFdNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_closeFdNative},
        { "getFreqNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getFreqNative},
        { "setFreqNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setFreqNative},
        { "getControlNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getControlNative},
        { "setControlNative", "(III)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setControlNative},
        { "startSearchNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_startSearchNative},
        { "cancelSearchNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_cancelSearchNative},
        { "getRSSINative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getRSSINative},
        { "setBandNative", "(III)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setBandNative},
        { "getLowerBandNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getLowerBandNative},
        { "getUpperBandNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getUpperBandNative},
        { "getBufferNative", "(I[BI)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getBufferNative},
        { "setMonoStereoNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setMonoStereoNative},
        { "getRawRdsNative", "(I[BI)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getRawRdsNative},
       { "setNotchFilterNative", "(IIZ)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setNotchFilterNative},
        { "startRTNative", "(ILjava/lang/String;I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_startRTNative},
        { "stopRTNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_stopRTNative},
        { "startPSNative", "(ILjava/lang/String;I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_startPSNative},
        { "stopPSNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_stopPSNative},
        { "setPTYNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setPTYNative},
        { "setPINative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setPINative},
        { "setPSRepeatCountNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setPSRepeatCountNative},
        { "setTxPowerLevelNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setTxPowerLevelNative},
       { "setAnalogModeNative", "(Z)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setAnalogModeNative},
        { "SetCalibrationNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_SetCalibrationNative},
        { "configureSpurTable", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_configureSpurTable},
        { "setSpurDataNative", "(I[SI)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setSpurDataNative},
        { "configurePerformanceParams", "(I)V",
             (void*)android_hardware_fmradio_FmReceiverJNI_configurePerformanceParams},
};

int register_android_hardware_fm_fmradio(JNIEnv* env)
{
        return jniRegisterNativeMethods(env, "qcom/fmradio/FmReceiverJNI", gMethods, NELEM(gMethods));
}

jint JNI_OnLoad(JavaVM *jvm, void *reserved)
{
  JNIEnv *e;
  int status;
   ALOGE("FM : loading QCOMM FM-JNI\n");
  
   if(jvm->GetEnv((void **)&e, JNI_VERSION_1_6)) {
       ALOGE("JNI version mismatch error");
      return JNI_ERR;
   }

   if ((status = register_android_hardware_fm_fmradio(e)) < 0) {
       ALOGE("jni adapter service registration failure, status: %d", status);
      return JNI_ERR;
   }
   return JNI_VERSION_1_6;
}
