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
    { "EFS:", EFS_BLOCK_NAME, NULL, "efs", "/efs", "rfs", "nosuid,nodev,check=no" },
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

