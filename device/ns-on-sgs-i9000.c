RootInfo g_roots[] = {
// sys stuff
    { "BOOT:", g_mtd_device, NULL, "boot", NULL, g_raw, NULL },
    { "RECOVERY:", g_mtd_device, NULL, "recovery", "/", g_raw, NULL },
    { "MBM:", g_mtd_device, NULL, "mbm", NULL, g_raw, NULL },
#ifndef BOARD_HAS_NO_MISC_PARTITION
    { "MISC:", g_mtd_device, NULL, "misc", NULL, g_raw, NULL },
#endif
// partitions
#ifdef BOARD_HAS_PHONE_CONTROLLER
    { "EFS:", EFS_BLOCK_NAME, NULL, "efs", "/efs", g_auto, NULL },
#endif
    { "CACHE:", CACHE_BLOCK_NAME, NULL, "cache", "/cache", g_auto, NULL },
    { "DATA:", DATA_BLOCK_NAME, NULL, "userdata", "/data", g_auto, NULL },
    { "SYSTEM:", SYSTEM_BLOCK_NAME, NULL, "system", "/system", g_auto, NULL },
#ifdef HAS_DATADATA
    { "DATADATA:", DBDATA_BLOCK_NAME, NULL, "datadata", "/dbdata", g_auto, NULL },
#endif
// sdcard
    { "SDCARD:", SDCARD_BLOCK_NAME, NULL, "sdcard", "/mnt/sdcard", "vfat", NULL },
#ifdef HAS_TWO_SDCARDS
    { "SDCARD2:", SDCARD2_BLOCK_NAME, NULL, "sdcard2", "/mnt/external_sd", "vfat", NULL },
#endif
// apps2sd
    { "SDEXT:", NULL, NULL, NULL, "/sd-ext", SDEXT_FILESYSTEM, NULL },
// virtual
    { "PACKAGE:", NULL, NULL, NULL, NULL, g_package_file, NULL },
    { "TMP:", NULL, NULL, NULL, "/tmp", NULL, NULL },
};
#define NUM_ROOTS (sizeof(g_roots) / sizeof(g_roots[0]))

string mounts[MOUNTABLE_COUNT][3] = {
    { "mount /system", "unmount /system", "SYSTEM:" },
    { "mount /data", "unmount /data", "DATA:" },
    { "mount /cache", "unmount /cache", "CACHE:" },
#ifdef HAS_DATADATA
    { "mount /dbdata", "unmount /dbdata", "DATADATA:" },
#endif
    { "mount /mnt/sdcard", "unmount /mnt/sdcard", "SDCARD:" },
#ifdef HAS_TWO_SDCARDS
    { "mount /mnt/external_sd", "unmount /mnt/external_sd", "SDCARD2:" },
#endif
    { "mount /sd-ext", "unmount /sd-ext", "SDEXT:" }
    };

string mtds[MTD_COUNT][2] = {
    { "format system", "SYSTEM:" },
    { "format data", "DATA:" },
    { "format dbdata", "DATADATA:" },
    { "format cache", "CACHE:" },
};

string mmcs[MMC_COUNT][3] = {
  { "format sdcard", "SDCARD:" },
  { "format external sdcard", "SDCARD2:" },
  { "format sd-ext", "SDEXT:" }
};

void fix_init(int isrecovery)
{
  // fix vold.conf
  call_busybox("sed","-i","s|dev_mount sdcard.*|dev_mount sdcard /mnt/sdcard 1 /devices/platform/s3c-sdhci.0/mmc_host/mmc0/mmc0:0001/block/mmcblk0|","/system/etc/vold.fstab",NULL);

  if (isrecovery) {
    // TODO: hardcoding this _IS_ bad
    call_busybox("sed","-i","306,411d","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\nservice recovery /sbin/steam recovery","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    user root","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    group root","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    oneshot\\\n\\\n","init.rc",NULL);
    call_busybox("sed","-i","s|mount yaffs2 mtd@system /system ro remount|#|","init.rc",NULL);
  } else {
    call_busybox("sed","-i","/export BOOTCLASSPATH/ a\\\n    class_start earlyinitclass","init.rc",NULL);

    call_busybox("sed","-i","/service console/ i\\\nservice earlyinit /sbin/steam earlyinit","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    user root","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    group root","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    oneshot","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    class earlyinitclass\\\n\\\n","init.rc",NULL);

    call_busybox("sed","-i","/service console/ i\\\nservice postinit /sbin/steam postinit","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    user root","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    group root","init.rc",NULL);
    call_busybox("sed","-i","/service console/ i\\\n    oneshot\\\n\\\n","init.rc",NULL);
  }
}
