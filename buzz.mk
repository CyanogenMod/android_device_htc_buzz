#
# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#



# Live wallpaper packages
PRODUCT_PACKAGES := \
    LiveWallpapers \
    LiveWallpapersPicker \
    MagicSmokeWallpapers \
    VisualizationWallpapers \
    librs_jni

# Publish that we support the live wallpaper feature.
PRODUCT_COPY_FILES := \
    packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:/system/etc/permissions/android.software.live_wallpaper.xml


#Gallery 2d
PRODUCT_PACKAGES += Gallery

# Support files
PRODUCT_COPY_FILES += \
    frameworks/base/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/base/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/base/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/base/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/base/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/base/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/base/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/base/data/etc/android.hardware.touchscreen.multitouch.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.xml

# Kernel modules
#PRODUCT_COPY_FILES += \

ifeq ($(TARGET_PREBUILT_KERNEL),)
LOCAL_KERNEL := device/htc/buzz/prebuilt/kernel
else
LOCAL_KERNEL := $(TARGET_PREBUILT_KERNEL)
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_KERNEL):kernel

PRODUCT_COPY_FILES += \
    device/htc/buzz/prebuilt/bcm4329.ko:system/lib/modules/bcm4329.ko

PRODUCT_PROPERTY_OVERRIDES := \
    ro.media.dec.jpeg.memcap=10000000
# RIL Proprties

PRODUCT_PROPERTY_OVERRIDES += \
    rild.libpath=/system/lib/libhtc_ril.so \
    ro.ril.ecc.HTC-WWE=999 \
    ro.ril.ecc.HTC-ELL=92,93,94 \
    ro.ril.enable.a52.HTC-ITA=1 \
    ro.ril.enable.a53.HTC-ITA=1 \
    ro.ril.enable.a52=0 \
    ro.ril.enable.a53=1 \
    ro.ril.vmail.23415=1571,BT \
    ro.ril.enable.dtm = 1 \
    ro.ril.hsdpa.category = 8 \
    ro.ril.htcmaskw1.bitmask = 4294967295 \
    ro.ril.htcmaskw1 = 14449 \
    ro.ril.def.agps.mode = 2

# Time between scans in seconds. Keep it high to minimize battery drain.
# This only affects the case in which there are remembered access points,
# but none are in range.
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface = eth0 \
    wifi.supplicant_scan_interval=45

# density in DPI of the LCD of this board. This is used to scale the UI
# appropriately. If this property is not defined, the default value is 120 dpi. 
PRODUCT_PROPERTY_OVERRIDES += \
    ro.sf.lcd_density=120


# View configuration for QVGA
PRODUCT_PROPERTY_OVERRIDES += \
    view.fading_edge_length=8 \
    view.touch_slop=15 \
    view.minimum_fling_velocity=25 \
    view.scroll_friction=0.008
    

PRODUCT_PROPERTY_OVERRIDES += \
    ro.secure=0 \
    ro.tether.denied=false

# Default network type
# 0 => WCDMA Preferred.
PRODUCT_PROPERTY_OVERRIDES += \
    ro.telephony.default_network=0

PRODUCT_PROPERTY_OVERRIDES += \
    ro.opengles.version=65536

PRODUCT_PROPERTY_OVERRIDES += \
    media.stagefright.enable-record=true

# Disable JIT by default
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.execution-mode=int:fast
    
PRODUCT_PROPERTY_OVERRIDES += \
    ro.com.google.locationfeatures=1

# media configuration xml file
PRODUCT_COPY_FILES += \
    device/htc/buzz/media_profiles.xml:/system/etc/media_profiles.xml

PRODUCT_PACKAGES += \
    librs_jni \
    lights.buzz \
    sensors.buzz \
    gralloc.msm7k \
    gps.buzz \
    copybit.msm7k

# stuff common to all HTC phones
$(call inherit-product, device/htc/common/common.mk)

$(call inherit-product, build/target/product/full_base.mk)

$(call inherit-product, device/common/gps/gps_eu_supl.mk)

PRODUCT_NAME := generic_buzz
PRODUCT_DEVICE := buzz
