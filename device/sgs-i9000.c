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

void fix_init(int isrecovery)
{
  char value[VALUE_MAX_LENGTH];

// TODO: This is designed for an unmodified initramfs, and might break if it's already modified
  call_busybox("sed","-i","/export TMPDIR/ a\\\n    class_start earlyinitclass","init.rc",NULL);
  call_busybox("sed","-i","s/mount rfs/#mount rfs/","init.rc",NULL);
  call_busybox("sed","-i","s|/system/bin/playlogos1|/sbin/steam postinit|g","init.rc",NULL);

  if (get_conf("init.bootanim",value) && strcmp(value,"2")==0) {
     call_busybox("sed","-i","/service playlogos1/ i\\\nservice bootanim /system/bin/bootanimation","init.rc",NULL);
     call_busybox("sed","-i","/service playlogos1/ i\\\n    user graphics","init.rc",NULL);
     call_busybox("sed","-i","/service playlogos1/ i\\\n    group graphics","init.rc",NULL);
     call_busybox("sed","-i","/service playlogos1/ i\\\n    oneshot","init.rc",NULL);
     call_busybox("sed","-i","/service playlogos1/ i\\\n    disabled","init.rc",NULL);
     call_busybox("sed","-i","/service playlogos1/ i\\\n    class nostart\\\n\\\n","init.rc",NULL);
   }
   call_busybox("sed","-i","/service playlogos1/ i\\\nservice earlyinit /sbin/steam earlyinit","init.rc",NULL);
   call_busybox("sed","-i","/service playlogos1/ i\\\n    user root","init.rc",NULL);
   call_busybox("sed","-i","/service playlogos1/ i\\\n    group root","init.rc",NULL);
   call_busybox("sed","-i","/service playlogos1/ i\\\n    oneshot","init.rc",NULL);
   call_busybox("sed","-i","/service playlogos1/ i\\\n    class earlyinitclass\\\n\\\n","init.rc",NULL);

   call_busybox("sed","-i","s|mount tmpfs nodev /tmp||","recovery.rc",NULL);
   call_busybox("sed","-i","s/mount rfs/#mount rfs/","recovery.rc",NULL);
   call_busybox("sed","-i","s|/sbin/adbd recovery|/sbin/adbd|","recovery.rc",NULL);
   call_busybox("sed","-i","s|/system/bin/recovery|/sbin/recovery|","recovery.rc",NULL);
}
