ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_CFLAGS := -Os $(STEAM_FUNCTIONS)

#LOCAL_CFLAGS += -DDEVICE_NS_ON_SGS
#LOCAL_CFLAGS += -DSTEAM_LANGUAGE_HUNGARIAN

commands_recovery_local_path := $(LOCAL_PATH)

LOCAL_SRC_FILES := \
	extendedcommands.c \
	nandroid.c \
	legacy.c \
	commands.c \
	recovery.c \
	graphsh.c \
	install.c \
	truncate.c \
	steamext.c \
	roots.c \
	ui.c \
	verifier.c \
	init.c \
	system.c \
	device.c \
	config.c \
	oem.c

LOCAL_SRC_FILES += \
    reboot.c \
    setprop.c

LOCAL_CFLAGS += -Dmain=steam_recovery_main

ifndef BOARD_HAS_NO_MISC_PARTITION
    LOCAL_SRC_FILES += \
        firmware.c \
        bootloader.c
else
    LOCAL_CFLAGS += -DBOARD_HAS_NO_MISC_PARTITION
endif

ifdef BOARD_RECOVERY_IGNORE_BOOTABLES
    LOCAL_CFLAGS += -DBOARD_RECOVERY_IGNORE_BOOTABLES
endif

ifdef BOARD_HIJACK_RECOVERY_PATH
    LOCAL_CFLAGS += -DBOARD_HIJACK_RECOVERY_PATH=\"$(BOARD_HIJACK_RECOVERY_PATH)\"
endif

LOCAL_SRC_FILES += test_roots.c


RECOVERY_VERSION := $(STEAM_VERSION)
LOCAL_CFLAGS += -DRECOVERY_VERSION="$(RECOVERY_VERSION)"
RECOVERY_API_VERSION := 3
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)

ifeq ($(BOARD_HAS_NO_SELECT_BUTTON),true)
  LOCAL_CFLAGS += -DKEY_POWER_IS_SELECT_ITEM
endif

ifdef BOARD_SDCARD_DEVICE_PRIMARY
  LOCAL_CFLAGS += -DSDCARD_DEVICE_PRIMARY=\"$(BOARD_SDCARD_DEVICE_PRIMARY)\"
endif

ifdef BOARD_SDCARD_DEVICE_SECONDARY
  LOCAL_CFLAGS += -DSDCARD_DEVICE_SECONDARY=\"$(BOARD_SDCARD_DEVICE_SECONDARY)\"
endif

ifdef BOARD_SDEXT_DEVICE
  LOCAL_CFLAGS += -DSDEXT_DEVICE=\"$(BOARD_SDEXT_DEVICE)\"
endif

ifdef BOARD_SDEXT_FILESYSTEM
  LOCAL_CFLAGS += -DSDEXT_FILESYSTEM=\"$(BOARD_SDEXT_FILESYSTEM)\"
endif

ifdef BOARD_DATA_DEVICE
  LOCAL_CFLAGS += -DDATA_DEVICE=\"$(BOARD_DATA_DEVICE)\"
endif

ifdef BOARD_DATA_FILESYSTEM
  LOCAL_CFLAGS += -DDATA_FILESYSTEM=\"$(BOARD_DATA_FILESYSTEM)\"
endif

ifdef BOARD_DATADATA_DEVICE
  LOCAL_CFLAGS += -DDATADATA_DEVICE=\"$(BOARD_DATADATA_DEVICE)\"
endif

ifdef BOARD_DATADATA_FILESYSTEM
  LOCAL_CFLAGS += -DDATADATA_FILESYSTEM=\"$(BOARD_DATADATA_FILESYSTEM)\"
endif

ifdef BOARD_CACHE_DEVICE
  LOCAL_CFLAGS += -DCACHE_DEVICE=\"$(BOARD_CACHE_DEVICE)\"
endif

ifdef BOARD_CACHE_FILESYSTEM
  LOCAL_CFLAGS += -DCACHE_FILESYSTEM=\"$(BOARD_CACHE_FILESYSTEM)\"
endif

ifdef BOARD_SYSTEM_DEVICE
  LOCAL_CFLAGS += -DSYSTEM_DEVICE=\"$(BOARD_SYSTEM_DEVICE)\"
endif

ifdef BOARD_SYSTEM_FILESYSTEM
  LOCAL_CFLAGS += -DSYSTEM_FILESYSTEM=\"$(BOARD_SYSTEM_FILESYSTEM)\"
endif

ifdef BOARD_HAS_DATADATA
  LOCAL_CFLAGS += -DHAS_DATADATA
endif

ifdef BOARD_DATA_FILESYSTEM_OPTIONS
  LOCAL_CFLAGS += -DDATA_FILESYSTEM_OPTIONS=\"$(BOARD_DATA_FILESYSTEM_OPTIONS)\"
endif

ifdef BOARD_DATADATA_FILESYSTEM_OPTIONS
  LOCAL_CFLAGS += -DDATADATA_FILESYSTEM_OPTIONS=\"$(BOARD_DATADATA_FILESYSTEM_OPTIONS)\"
endif

ifdef BOARD_CACHE_FILESYSTEM_OPTIONS
  LOCAL_CFLAGS += -DCACHE_FILESYSTEM_OPTIONS=\"$(BOARD_CACHE_FILESYSTEM_OPTIONS)\"
endif

ifdef BOARD_SYSTEM_FILESYSTEM_OPTIONS
  LOCAL_CFLAGS += -DSYSTEM_FILESYSTEM_OPTIONS=\"$(BOARD_SYSTEM_FILESYSTEM_OPTIONS)\"
endif

ifdef BOARD_HAS_MTD_CACHE
  LOCAL_CFLAGS += -DBOARD_HAS_MTD_CACHE
endif

ifdef BOARD_USES_BMLUTILS
  LOCAL_CFLAGS += -DBOARD_USES_BMLUTILS
  LOCAL_STATIC_LIBRARIES += libsteam_bmlutils
endif

ifdef BOARD_HAS_SMALL_RECOVERY
  LOCAL_CFLAGS += -DBOARD_HAS_SMALL_RECOVERY
endif

# This binary is in the recovery ramdisk, which is otherwise a copy of root.
# It gets copied there in config/Makefile.  LOCAL_MODULE_TAGS suppresses
# a (redundant) copy of the binary in /system/bin for user builds.
# TODO: Build the ramdisk image in a more principled way.

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libsteam_recovery
LOCAL_STATIC_LIBRARIES :=

LOCAL_C_INCLUDES := bootable/steam

LOCAL_STATIC_LIBRARIES += libsteam_busybox libsteam_clearsilverregex libsteam_mkyaffs2image libsteam_unyaffs libsteam_erase_image libsteam_dump_image libsteam_flash_image libsteam_mtdutils
LOCAL_STATIC_LIBRARIES += libsteam_amend
LOCAL_STATIC_LIBRARIES += libsteam_minzip libunz libsteam_mtdutils libsteam_mmcutils libmincrypt
LOCAL_STATIC_LIBRARIES += libsteam_minui libpixelflinger_static libpng libcutils
LOCAL_STATIC_LIBRARIES += libstdc++ libc

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CFLAGS := -Os

LOCAL_SRC_FILES := verifier_test.c verifier.c
LOCAL_MODULE := steam_verifier_test
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_TAGS := tests
LOCAL_STATIC_LIBRARIES := libmincrypt libcutils libstdc++ libc

include $(BUILD_EXECUTABLE)

commands_recovery_local_path :=

endif   # TARGET_ARCH == arm
endif    # !TARGET_SIMULATOR

