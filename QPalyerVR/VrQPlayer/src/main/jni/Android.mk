LOCAL_PATH := $(call my-dir)
####################################################################################################

include $(CLEAR_VARS)

IMPORT_PATH := $(LOCAL_PATH)/../../../..
include $(IMPORT_PATH)/cflags.mk
LOCAL_MODULE := vrqplayer


LOCAL_C_INCLUDES := \
	  $(IMPORT_PATH)/SampleCommon/src/main/jni/Src\
      $(IMPORT_PATH)/SampleFramework/src/main/jni/Src \
      $(IMPORT_PATH)/VrApi/Include \
      $(IMPORT_PATH)/1stParty/OVR/Include \
      $(IMPORT_PATH)/1stParty/utilities/include \
      $(IMPORT_PATH)/3rdParty/stb/src \

LOCAL_SRC_FILES := \
	main.cpp \
	kernel.cpp\
	kernel_input_device.cpp\
	kernel_gl_tool.c\
	kernel_ui.cpp\


LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog
LOCAL_STATIC_LIBRARIES := sampleframework
LOCAL_SHARED_LIBRARIES := samplecommon
LOCAL_SHARED_LIBRARIES += vrapi


include $(BUILD_SHARED_LIBRARY)
IMPORT_PATH := $(LOCAL_PATH)/../../../..

$(call import-add-path, $(IMPORT_PATH))

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,SampleCommon/src/main/jni)
$(call import-module,SampleFramework/src/main/jni)

