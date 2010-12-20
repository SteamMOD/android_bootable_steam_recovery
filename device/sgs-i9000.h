
#define EFS_BLOCK_NAME "/dev/block/stl3"
#define CACHE_BLOCK_NAME "/dev/block/stl11"
#define DATA_BLOCK_NAME "/dev/block/mmcblk0p2"
#define SYSTEM_BLOCK_NAME "/dev/block/stl9"
#define DBDATA_BLOCK_NAME "/dev/block/stl10"

#define SDCARD_BLOCK_NAME "/dev/block/mmcblk0p1"
#define SDCARD2_BLOCK_NAME "/dev/block/mmcblk1p1"

// for the partition reformatter
#define SDCARD_EXT_BLOCK_NAME "/dev/block/mmcblk1X"

#define LIBLIGHTS_SOURCEDIR "/res/misc"
#define LIBLIGHTS_DESTDIR "/system/lib/hw"
#define LIBLIGHTS_NAME "lights.s5pc110.so"
#define LIBLIGHTS_DEST LIBLIGHTS_DESTDIR "/" LIBLIGHTS_NAME
#define LIBLIGHTS_SRC LIBLIGHTS_SOURCEDIR "/" LIBLIGHTS_NAME

#define HAS_TWO_SDCARDS 1

#define MOUNTABLE_COUNT 7
#define MTD_COUNT 4
#define MMC_COUNT 3

typedef char* string;
extern string mounts[MOUNTABLE_COUNT][3];
extern string mtds[MTD_COUNT][2];
extern string mmcs[MMC_COUNT][3];

#define SDCARD_LUN_FILE "/sys/devices/platform/s3c-usbgadget/gadget/lun1"
#define SDCARD2_LUN_FILE "/sys/devices/platform/s3c-usbgadget/gadget/lun2"
#define SDCARD_LUN_CONTENT SDCARD_BLOCK_NAME
#define SDCARD2_LUN_CONTENT SDCARD2_BLOCK_NAME

#ifndef HAS_DATADATA
#define HAS_DATADATA 1
#endif

#ifndef BOARD_HAS_PHONE_CONTROLLER
#define BOARD_HAS_PHONE_CONTROLLER 1
#endif

#ifndef BOARD_HAS_NO_MISC_PARTITION
#define BOARD_HAS_NO_MISC_PARTITION 1
#endif

#ifndef BOARD_RECOVERY_IGNORE_BOOTABLES
#define BOARD_RECOVERY_IGNORE_BOOTABLES 1
#endif

#ifndef BOARD_HAS_SMALL_RECOVERY
#define BOARD_HAS_SMALL_RECOVERY 1
#endif

#define KEY_DEVICE_VOLUP   KEY_CAPSLOCK
#define KEY_DEVICE_VOLDOWN KEY_LEFTSHIFT
#define KEY_DEVICE_POWER   KEY_LEFTBRACE
#define KEY_DEVICE_HOME    KEY_M
#define KEY_DEVICE_BACK    KEY_ENTER
#define KEY_DEVICE_MENU    KEY_BACK
#define KEY_DEVICE_SEARCH  -1


