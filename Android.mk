LOCAL_PATH := $(my-dir)

ifeq ($(TARGET_DEVICE),buzz)
    subdir_makefiles := \
        $(LOCAL_PATH)/libcopybit/Android.mk \
        $(LOCAL_PATH)/libgralloc/Android.mk \
        $(LOCAL_PATH)/liblights/Android.mk \
        $(LOCAL_PATH)/libsensors/Android.mk \
        $(LOCAL_PATH)/recovery/offmode/Android.mk

    include $(subdir_makefiles)
endif
