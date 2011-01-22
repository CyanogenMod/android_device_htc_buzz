# config.mk
#
# Product-specific compile-time definitions.
#
 
# WARNING: This line must come *before* including the proprietary
# variant, so that it gets overwritten by the parent (which goes
# against the traditional rules of inheritance).

USE_CAMERA_STUB := true
 
# inherit from the proprietary version
-include vendor/htc/buzz/BoardConfigVendor.mk
 
TARGET_BOARD_PLATFORM := msm7k
TARGET_ARCH_VARIANT := armv6j
ARCH_ARM_HAVE_NEON := false

JS_ENGINE:=v8

TARGET_CPU_ABI := armeabi
 
TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true

BOARD_USE_FROYO_LIBCAMERA := true

# Wifi related defines
BOARD_WPA_SUPPLICANT_DRIVER := WEXT
WPA_SUPPLICANT_VERSION      := VER_0_6_X
BOARD_WLAN_DEVICE           := bcm4329
WIFI_DRIVER_MODULE_PATH     := "/system/lib/modules/bcm4329.ko"
WIFI_DRIVER_FW_STA_PATH     := "/system/etc/firmware/fw_bcm4329.bin"
WIFI_DRIVER_FW_AP_PATH      := "/system/etc/firmware/fw_bcm4329_apsta.bin"
WIFI_DRIVER_MODULE_ARG      := "firmware_path=/system/etc/firmware/fw_bcm4329.bin nvram_path=/proc/calibration"
WIFI_DRIVER_MODULE_NAME     := "bcm4329"

BOARD_LDPI_RECOVERY := true

TARGET_PROVIDES_INIT_TARGET_RC := true

BOARD_KERNEL_CMDLINE := no_console_suspend=1 console=null
BOARD_KERNEL_BASE := 0x02E00000

BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true

BOARD_HAS_LIMITED_EGL:=true

BOARD_VENDOR_USE_AKMD := akmd

#BOARD_VENDOR_QCOM_AMSS_VERSION := 1355

TARGET_HARDWARE_3D := false

#BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE := buzz
#BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION := 3200

BOARD_GPS_LIBRARIES := libgps librpc

BOARD_USES_GPSSHIM:=true

BOARD_GPS_NEEDS_XTRA := true

BOARD_USES_QCOM_LIBS := true

BOARD_USES_QCOM_HARDWARE := true

TARGET_BOOTLOADER_BOARD_NAME := buzz
 
TARGET_RELEASETOOLS_EXTENSIONS := device/htc/common
 
TARGET_OTA_ASSERT_DEVICE := buzz

BOARD_NO_RGBX_8888 := true

TARGET_LIBAGL_USE_GRALLOC_COPYBITS := true

BOARD_USE_HTC_USB_FUNCTION_SWITCH := true

BOARD_USE_USB_MASS_STORAGE_SWITCH := true

BOARD_USE_NEW_LIBRIL_HTC := true
 
# # cat /proc/mtd
# dev:    size   erasesize  name
# mtd0: 00040000 00020000 "misc"
# mtd1: 00500000 00020000 "recovery"
# mtd2: 00280000 00020000 "boot"
# mtd3: 05a00000 00020000 "system"
# mtd4: 05000000 00020000 "cache"
# mtd5: 127c0000 00020000 "userdata"
# Changed for wildfire
BOARD_BOOTIMAGE_MAX_SIZE := $(call image-size-from-data-size,0x00280000)
BOARD_RECOVERYIMAGE_MAX_SIZE := $(call image-size-from-data-size,0x00500000)
BOARD_SYSTEMIMAGE_MAX_SIZE := $(call image-size-from-data-size,0x09600000)
BOARD_USERDATAIMAGE_MAX_SIZE := $(call image-size-from-data-size,0x0a520000)
 
# The size of a block that can be marked bad.
BOARD_FLASH_BLOCK_SIZE := 153600
 
# Stagefright fully enabled
BUILD_WITH_FULL_STAGEFRIGHT := true
 
 
# JIT built in, but disabled by default
#WITH_JIT := true
#ENABLE_JSC_JIT := true
 
# Stop compiling test_* binaries for eng tag
#STOP_TEST_BINS := true

TARGET_PREBUILT_KERNEL := device/htc/buzz/prebuilt/kernel
LOCAL_KERNEL := device/htc/buzz/prebuilt/kernel
