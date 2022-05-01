/*******************************************************************************


*******************************************************************************/

//
#include <cstdint>
#include <cstdio>
#include <stdlib.h> // for exit()
#include <unistd.h> // for sleep()

#include "Platform/Android/Android.h"

#include <android/window.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>

#include "VrApi.h"

#include "Appl.h"
#include "kernel.h"



extern "C" {

OVRFWQ::ovrQPlayerAppl *appPtr = nullptr;
//-----------------------------------------------------------
long Java_com_yen_vrqplayer_MainActivity_nativeSetAppInterface(
        JNIEnv* jni,
        jclass clazz,
        jobject activity) {
    ALOG("nativeSetAppInterface %p", appPtr);
    return reinterpret_cast<jlong>(appPtr);
}

void Java_com_yen_vrqplayer_MainActivity_nativeReloadPhoto(
        JNIEnv* jni,
        jclass clazz,
        jlong interfacePtr) {
    ALOG("nativeReloadPhoto interfacePtr=%p appPtr=%p", interfacePtr, appPtr);
    if (appPtr && interfacePtr) {
       // videoPlayer->ReloadPhoto();
    } else {
        ALOG("nativeReloadPhoto %p NULL ptr", appPtr);
    }
}

void Java_com_yen_vrqplayer_MainActivity_nativeSetVideoSize(
    JNIEnv *jni,
    jclass clazz,
    jlong interfacePtr, int width, int height)
{//
    ALOG("nativeSetVideoSize %p", interfacePtr);
    OVRFWQ::ovrQPlayerAppl *videoPlayer = appPtr;
    if (videoPlayer && interfacePtr) {
         videoPlayer->SetVideoSize(width, height);
    } else {
        ALOG("nativeSetVideoSize %p cinema == NULL", videoPlayer);
    }

}


jobject Java_com_yen_vrqplayer_MainActivity_nativePrepareNewVideo(
        JNIEnv* jni,
        jclass clazz,
        jlong interfacePtr)
{
    ALOG("nativePrepareNewVideo interfacePtr=%p appPtr=%p", interfacePtr, appPtr);
    jobject surfaceTexture = nullptr;
    OVRFWQ::ovrQPlayerAppl* videoPlayer = appPtr;
    if (videoPlayer && interfacePtr) {
         videoPlayer->GetScreenSurface(surfaceTexture);
    } else {
        ALOG("nativePrepareNewVideo %p NULL ptr", videoPlayer);
    }
    return surfaceTexture;
}

void Java_com_yen_vrqplayer_MainActivity_nativeVideoCompletion(
        JNIEnv* jni,
        jclass clazz,
        jlong interfacePtr) {
    ALOG("nativeVideoCompletion interfacePtr=%p appPtr=%p", interfacePtr, appPtr);
    OVRFWQ::ovrQPlayerAppl* videoPlayer = appPtr;
    if (videoPlayer && interfacePtr) {
         videoPlayer->VideoEnded();
    } else {
        ALOG("nativeVideoCompletion %p NULL ptr", videoPlayer);
    }
}


}// extern "C"
//==============================================================
// android_main
//==============================================================
void android_main(struct android_app* app)
{
    appPtr = nullptr;
    std::unique_ptr<OVRFWQ::ovrQPlayerAppl> appl = std::unique_ptr<OVRFWQ::ovrQPlayerAppl>(
                new OVRFWQ::ovrQPlayerAppl(0, 0, 0, 0));
    appPtr = appl.get();
    appl->Run(app);
}