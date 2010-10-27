LOCAL_PATH := $(call my-dir)

file := $(TARGET_ROOT_OUT)/init.buzz.rc
ALL_PREBUILT += $(file)
$(file) : $(LOCAL_PATH)/init.buzz.rc | $(ACP)
	$(transform-prebuilt-to-target)

file := $(TARGET_ROOT_OUT)/init.rc
ALL_PREBUILT += $(file)
$(file) : $(LOCAL_PATH)/init.rc | $(ACP)
	$(transform-prebuilt-to-target)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := buzz-keypad.kcm
include $(BUILD_KEY_CHAR_MAP)

# This will install the file in /system/etc
#

include $(CLEAR_VARS)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE := vold.fstab
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

