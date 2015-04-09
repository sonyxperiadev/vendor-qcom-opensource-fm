/*
Copyright (c) 2015, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <jni.h>
#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"
#include <utils/Log.h>
#include "FmRadioController.h"
#include "FM_Const.h"

static FmRadioController * pFMRadio;

jboolean OpenFd(JNIEnv *env, jobject thiz)
{
    int ret = 0;
    pFMRadio = new FmRadioController();
    if (pFMRadio)
        ret = pFMRadio->open_dev();
    else
        ret = JNI_FALSE;
    ALOGD("%s, [ret=%d]\n", __func__, ret);
    return ret? JNI_FALSE: JNI_TRUE;
}

jboolean CloseFd(JNIEnv *env, jobject thiz)
{
    int ret = 0;

    if (pFMRadio)
        ret = pFMRadio->close_dev();
    else
        ret = JNI_FALSE;

    ALOGD("%s, [ret=%d]\n", __func__, ret);
    return ret? JNI_FALSE: JNI_TRUE;
}

jboolean TurnOn(JNIEnv *env, jobject thiz, jfloat freq)
{
    int ret = 0;
    int tmp_freq;

    ALOGI("%s, [freq=%d]\n", __func__, (int)freq);
    tmp_freq = (int)(freq * FREQ_MULT);   //Eg, 87.5 * 1000 --> 87500
    if (!pFMRadio) {
        pFMRadio = new FmRadioController();
    }
    if (pFMRadio)
        ret = pFMRadio->Pwr_Up(tmp_freq);
    else
        ret = JNI_FALSE;

    ALOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?JNI_FALSE:JNI_TRUE;
}

jboolean TurnOff(JNIEnv *env, jobject thiz, jint type)
{
    int ret = 0;

    if (pFMRadio)
        ret = pFMRadio->Pwr_Down();
    else
        ret = JNI_FALSE;

    ALOGD("%s, [ret=%d]\n", __func__, ret);
    if (pFMRadio) {
        delete pFMRadio;
        pFMRadio = NULL;
    }
    return ret?JNI_FALSE:JNI_TRUE;
}

jboolean SetFreq(JNIEnv *env, jobject thiz, jfloat freq)
{
    int ret = 0;
    int tmp_freq;

    tmp_freq = (int)(freq * FREQ_MULT);        //Eg, 87.5 * 10 --> 875
    if (pFMRadio)
        ret = pFMRadio->TuneChannel(tmp_freq);
    else
        ret = JNI_FALSE;

    ALOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?JNI_FALSE:JNI_TRUE;
}

jfloat Seek(JNIEnv *env, jobject thiz, jfloat freq, jboolean isUp)
{
    int ret = 0;
    int tmp_freq;
    int ret_freq;
    float val;

    tmp_freq = (int)(freq * FREQ_MULT); //Eg, 87.55 * 100 --> 87550
    if (pFMRadio)
        ret = pFMRadio->Set_mute(true);
    else
        ret = JNI_FALSE;
    if (ret) {
        ALOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    ALOGD("%s, [mute] [ret=%d]\n", __func__, ret);
    if (pFMRadio)
        ret = pFMRadio->Seek((int)isUp);
    else
        ret = JNI_FALSE;
    if (ret < 0) {
        ret_freq = tmp_freq; //seek error, so use original freq
    }

    ALOGD("%s, [freq=%d] [ret=%d]\n", __func__, ret_freq, ret);

    val = (float)ret/FREQ_MULT;   //Eg, 8755 / 100 --> 87.55

    return val;
}

jshortArray ScanList(JNIEnv *env, jobject thiz)
{
    int ret = 0;
    jshortArray scanList;
    int chl_cnt = FM_SCAN_CH_SIZE_MAX;
    uint16_t ScanTBL[FM_SCAN_CH_SIZE_MAX];

    if (pFMRadio)
        ret = pFMRadio->ScanList(ScanTBL, &chl_cnt);
    else
        ret = JNI_FALSE;
    if (ret < 0) {
        ALOGE("scan failed!\n");
        scanList = NULL;
        goto out;
    }
    if (chl_cnt > 0) {
        scanList = env->NewShortArray(chl_cnt);
        env->SetShortArrayRegion(scanList, 0, chl_cnt, (const jshort*)&ScanTBL[0]);
    } else {
        ALOGE("cnt error, [cnt=%d]\n", chl_cnt);
        scanList = NULL;
    }

out:
    ALOGD("%s, [cnt=%d] [ret=%d]\n", __func__, chl_cnt, ret);
    return scanList;
}

jshort GetRdsEvent(JNIEnv *env, jobject thiz)
{
    int ret = JNI_FALSE;

    if (pFMRadio)
        ret = pFMRadio->ReadRDS();

    return ret;
}

jbyteArray GetPsText(JNIEnv *env, jobject thiz)
{
    int ret = 0;
    jbyteArray PS;
    char ps[MAX_PS_LEN];
    int ps_len = 0;

    if (pFMRadio)
        ret = pFMRadio->Get_ps(ps, &ps_len);
    else
        ret = JNI_FALSE;
    if (ret) {
        ALOGE("%s, error, [ret=%d]\n", __func__, ret);
        return NULL;
    }
    PS = env->NewByteArray(ps_len);
    env->SetByteArrayRegion(PS, 0, ps_len, (const jbyte*)ps);
    ALOGD("%s, [ret=%d]\n", __func__, ret);
    return PS;
}

jbyteArray GetRtText(JNIEnv *env, jobject thiz)
{
    int ret = 0;
    jbyteArray RadioText;
    char rt[MAX_RT_LEN];
    int rt_len = 0;

    if (pFMRadio)
        ret = pFMRadio->Get_rt(rt, &rt_len);
    else
        ret = JNI_FALSE;
    if (ret) {
        ALOGE("%s, error, [ret=%d]\n", __func__, ret);
        return NULL;
    }
    RadioText = env->NewByteArray(rt_len);
    env->SetByteArrayRegion(RadioText, 0, rt_len, (const jbyte*)rt);
    ALOGD("%s, [ret=%d]\n", __func__, ret);
    return RadioText;
}

jshort GetAfFreq(JNIEnv *env, jobject thiz)
{
    int ret = 0;
    jshort ret_freq = 0;

    if (pFMRadio)
        ret = pFMRadio->Get_AF_freq((uint16_t*)&ret_freq);
    else
        ret = JNI_FALSE;
    if (ret) {
        ALOGE("%s, error, [ret=%d]\n", __func__, ret);
        return 0;
    }
    ALOGD("%s, [ret_freq=%d]\n", __func__, ret_freq);
    return ret_freq;
}

jint SetRds(JNIEnv *env, jobject thiz, jboolean rdson)
{
    int ret = 0;

    if (pFMRadio)
        ret = pFMRadio->Turn_On_Off_Rds(rdson);
    else
        ret = JNI_FALSE;
    if (ret) {
        ALOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    ALOGD("%s, [rdson=%d] [ret=%d]\n", __func__, rdson, ret);
    return ret?JNI_FALSE:JNI_TRUE;
}

jboolean StopSrch(JNIEnv *env, jobject thiz)
{
    int ret = 0;

    if (pFMRadio)
        ret = pFMRadio->Stop_Scan_Seek();
    else
        ret = JNI_FALSE;
    if (ret) {
        ALOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    ALOGD("%s, [ret=%d]\n", __func__, ret);
    return ret?JNI_FALSE:JNI_TRUE;
}

jint SetMute(JNIEnv *env, jobject thiz, jboolean mute)
{
    int ret = 0;

    if (pFMRadio)
        ret = pFMRadio->Set_mute(mute);
    else
        ret = JNI_FALSE;
    if (ret) {
        ALOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    ALOGD("%s, [mute=%d] [ret=%d]\n", __func__, (int)mute, ret);
    return ret?JNI_FALSE:JNI_TRUE;
}

/******************************************
 * Inquiry if RDS is support in driver.
 * Parameter:
 *      None
 *Return Value:
 *      1: support
 *      0: NOT support
 *      -1: error
 ******************************************/
jint IsRdsSupport(JNIEnv *env, jobject thiz)
{
    int ret = 0;

    if (pFMRadio)
        ret = pFMRadio->IsRds_support();
    else
        ret = JNI_FALSE;
    if (!ret) {
        ALOGE("%s, error, [ret=%d]\n", __func__, ret);
    }
    ALOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

/******************************************
 * SwitchAntenna
 * Parameter:
 *      antenna:
                0 : switch to long antenna
                1: switch to short antenna
 *Return Value:
 *          0: Success
 *          1: Failed
 *          2: Not support
 ******************************************/
jint SetAntenna(JNIEnv *env, jobject thiz, jint antenna)
{
    int ret = 0;
    jint jret = 0;
    int ana = -1;

    if (0 == antenna) {
        ana = FM_LONG_ANA;
    } else if (1 == antenna) {
        ana = FM_SHORT_ANA;
    } else {
        ALOGE("%s:fail, para error\n", __func__);
        jret = JNI_FALSE;
        goto out;
    }
    if (pFMRadio)
        ret = pFMRadio->Antenna_Switch(ana);
    else
        ret = JNI_FALSE;
    if (ret) {
        ALOGE("switchAntenna(), error\n");
        jret = 1;
    } else {
        jret = 0;
    }
out:
    ALOGD("%s: [antenna=%d] [ret=%d]\n", __func__, ana, ret);
    return jret;
}

static const char *classPathNameFM = "com/android/fmradio/FmNative";

static JNINativeMethod gMethods[] = {
    {"openDev",       "()Z",   (void*)OpenFd },
    {"closeDev",      "()Z",   (void*)CloseFd },
    {"powerUp",       "(F)Z",  (void*)TurnOn },
    {"powerDown",     "(I)Z",  (void*)TurnOff },
    {"tune",          "(F)Z",  (void*)SetFreq },
    {"seek",          "(FZ)F", (void*)Seek },
    {"autoScan",      "()[S",  (void*)ScanList },
    {"stopScan",      "()Z",   (void*)StopSrch },
    {"setRds",        "(Z)I",  (void*)SetRds  },
    {"readRds",       "()S",   (void*)GetRdsEvent },
    {"getPs",         "()[B",  (void*)GetPsText },
    {"getLrText",     "()[B",  (void*)GetRtText},
    {"activeAf",      "()S",   (void*)GetAfFreq},
    {"setMute",       "(Z)I",  (void*)SetMute},
    {"isRdsSupport",  "()I",   (void*)IsRdsSupport},
    {"switchAntenna", "(I)I",  (void*)SetAntenna},
};

int register_android_hardware_fm(JNIEnv* env)
{
        return jniRegisterNativeMethods(env, classPathNameFM, gMethods, NELEM(gMethods));
}

jint JNI_OnLoad(JavaVM *jvm, void *reserved)
{
  JNIEnv *e;
  int status;
   ALOGE("FM : loading FM-JNI\n");

   if(jvm->GetEnv((void **)&e, JNI_VERSION_1_6)) {
       ALOGE("JNI version mismatch error");
      return JNI_ERR;
   }

   if ((status = register_android_hardware_fm(e)) < 0) {
       ALOGE("jni adapter service registration failure, status: %d", status);
      return JNI_ERR;
   }
   return JNI_VERSION_1_6;
}

