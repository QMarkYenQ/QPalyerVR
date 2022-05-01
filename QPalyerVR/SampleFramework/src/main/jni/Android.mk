LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := sampleframework

IMPORT_PATH := $(LOCAL_PATH)/../../../..
include $(IMPORT_PATH)/cflags.mk



# full speed arm instead of thumb
LOCAL_ARM_MODE := arm
# compile with neon support enabled
LOCAL_ARM_NEON := true


LOCAL_CFLAGS += -Wno-invalid-offsetof
LOCAL_C_INCLUDES := \
  $(IMPORT_PATH)/SampleCommon\src\main\jni\Src \
  $(LOCAL_PATH)/Src \
  $(IMPORT_PATH)/VrApi/Include \
  $(IMPORT_PATH)/1stParty/OVR/Include \
  $(IMPORT_PATH)/1stParty/utilities/include \
  $(IMPORT_PATH)/3rdParty/stb/src \

LOCAL_SHARED_LIBRARIES += minizip stb android_native_app_glue vrapi samplecommon
LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog

LOCAL_SRC_FILES := \
	Src/Appl.cpp \
	Src/Input/HandMaskRenderer.cpp \
	Src/Input/HandModel.cpp \
	Src/Input/HandRenderer.cpp \
	Src/Platform/Android/Android.cpp \
	Src/Render/Framebuffer.c \
	Src/SurfaceRenderApp.cpp \
	Src/Input/Simpleinput.cpp \
# start building based on everything since CLEAR_VARS
include $(BUILD_STATIC_LIBRARY)

IMPORT_PATH := $(LOCAL_PATH)/../../../..
$(call import-add-path, $(IMPORT_PATH))

$(call import-module,android/native_app_glue)
$(call import-module,3rdParty/minizip/build/android/jni)
$(call import-module,3rdParty/stb/build/android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,SampleCommon/src/main/jni)