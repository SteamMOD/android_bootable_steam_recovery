#include <ctype.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <cutils/properties.h>
#include "../steam_main/steam.h"
#include "ui.h"
#include "device.h"
#include "system.h"
#include "locale.h"
#include "config.h"
#include "nandroid.h"

#define INIT_LOG_FILE "/tmp/init.log"
#define POSTINIT_LOG_FILE "/tmp/post-init.log"
#define EARLYINIT_LOG_FILE "/tmp/early-init.log"


int crsymlink(char* name) {
  char n[50];
  sprintf(n,"/sbin/%s",name);
  return symlink("/sbin/steam",n);
}

int rmsymlink(char* name) {
  struct stat sbuf;
  char n[50];
  sprintf(n,"/sbin/%s",name);
  if (lstat(n,&sbuf)==0) {
    if (S_ISLNK(sbuf.st_mode)) {
      int r = unlink(n);
      return r;
    }
  }
  errno = EACCES;
  return -1;
}

void reboot_recovery() {
  __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "recovery");
}

extern char **environ;

int steam_postinit_main(int argc, char* argv[]) {
  freopen(POSTINIT_LOG_FILE,"a+",stdout);setbuf(stdout,NULL);
  freopen(POSTINIT_LOG_FILE,"a+",stderr);setbuf(stderr,NULL);
  struct stat sbuf;
  char value[VALUE_MAX_LENGTH];
  init_conf();
  printf("--- POSTINIT ---\n");
  // remount / as it was set to read-only in init
  call_busybox("mount","-o","remount,rw","/",NULL);

  int startanim = 1;
  if (get_conf("init.bootanim",value) && strcmp(value,"0")==0) startanim=0;
  if (get_conf("init.bootanim",value) && strcmp(value,"2")==0) startanim=2;

  if (strcmp(get_conf_def("tweaks.iosched",value,"0"),"1")==0) {
    // tweak cfq io scheduler
    printf(TWEAKS_ENABLE_IOSCHED);
    DIR* d = opendir("/sys/block");
    FILE *f;
    if (d) {
      struct dirent *entry;
      char path[PATH_MAX];
      while ((entry = readdir(d))!=NULL) {
        if ((strstr(entry->d_name,"stl")==entry->d_name) ||
           (strstr(entry->d_name,"mmc")==entry->d_name) ||
           (strstr(entry->d_name,"bml")==entry->d_name) ||
           (strstr(entry->d_name,"tfsr")==entry->d_name)) {
          sprintf(path,"/sys/block/%s/queue/rotational",entry->d_name);
          f = fopen(path,"w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.iosched.rotational",value,"0")); fclose(f); }
          sprintf(path,"/sys/block/%s/queue/iosched/low_latency",entry->d_name);
          f = fopen(path,"w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.iosched.low_latency",value,"1")); fclose(f); }
          sprintf(path,"/sys/block/%s/queue/iosched/back_seek_penalty",entry->d_name);
          f = fopen(path,"w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.iosched.back_seek_penalty",value,"1")); fclose(f); }
          sprintf(path,"/sys/block/%s/queue/iosched/back_seek_max",entry->d_name);
          f = fopen(path,"w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.iosched.back_seek_max",value,"1000000000")); fclose(f); }
          sprintf(path,"/sys/block/%s/queue/iosched/slice_idle",entry->d_name);
          f = fopen(path,"w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.iosched.slice_idle",value,"3")); fclose(f); }
        }
      }
      closedir(d);
    }
  }

  if (strcmp(get_conf_def("tweaks.kernelvm",value,"0"),"1")==0) {
    printf(TWEAKS_ENABLE_KERNELVM);
    FILE*f;
    f = fopen("/proc/sys/vm/swappiness","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.kernelvm.swappiness",value,"0")); fclose(f); }
    f = fopen("/proc/sys/vm/dirty_ratio","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.kernelvm.dirty_ratio",value,"20")); fclose(f); }
    f = fopen("/proc/sys/vm/vfs_cache_pressure","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.kernelvm.vfs_cache_pressure",value,"100")); fclose(f); }
    f = fopen("/proc/sys/vm/min_free_kbytes","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.kernelvm.min_free_kbytes",value,"2746")); fclose(f); }
  }

  if (strcmp(get_conf_def("tweaks.kernelsched",value,"0"),"1")==0) {
    printf(TWEAKS_ENABLE_KERNELSCHED);
    FILE*f;
    f = fopen("/proc/sys/vm/sched_latency_ns","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.kernelsched.sched_latency_ns",value,"20000000")); fclose(f); }
    f = fopen("/proc/sys/vm/sched_wakeup_granularity_ns","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.kernelsched.sched_min_granularity_ns",value,"1000000")); fclose(f); }
    f = fopen("/proc/sys/vm/sched_min_granularity_ns","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.kernelsched.sched_wakeup_granularity_ns",value,"2000000")); fclose(f); }
  }

  if (strcmp(get_conf_def("tweaks.misc",value,"0"),"1")==0) {
    printf(TWEAKS_ENABLE_MISC);
    //property_set("dalvik.vm.startheapsize",get_conf_def("tweaks.misc.heapsize",value,"8m"));
    //property_set("wifi.supplicant_scan_interval",get_conf_def("tweaks.misc.supplicant_scan_interval"));
    FILE*f;
    f = fopen("/proc/sys/vm/dirty_writeback_centisecs","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.misc.dirty_writeback_centisecs",value,"2000")); fclose(f); }
    f = fopen("/proc/sys/vm/dirty_expire_centisecs","w+"); if (f) { fprintf(f,"%s\n",get_conf_def("tweaks.misc.dirty_expire_centisecs",value,"1000")); fclose(f); }
  }

  // BLN
  if (call_busybox("grep","^1$","/system/etc/bln.conf",NULL)==0) {
    FILE* f = fopen("/sys/class/misc/backlightnotification/enabled","w+");
    if (f) { fprintf(f,"1\n");fclose(f); };
  } else {
    FILE* f = fopen("/sys/class/misc/backlightnotification/enabled","w+");
    if (f) { fprintf(f,"0\n");fclose(f); };
  }
  // install liblights
  if (call_busybox("grep","backlightnotification",LIBLIGHTS_DEST,NULL)) {
    // no liblights found
    if (stat(LIBLIGHTS_DEST ".backup",&sbuf)) {
      // no backup found
      call_busybox("cp",LIBLIGHTS_DEST,LIBLIGHTS_DEST ".backup",NULL);
    }
    call_busybox("cp",LIBLIGHTS_SRC,LIBLIGHTS_DEST,NULL);
    call_busybox("chmod","644",LIBLIGHTS_DEST,NULL);
  }
  // put back original fat.format
  call_busybox("cp","/sbin/fat.format","/system/bin/fat.format",NULL);

  time_t t;

  t=time(NULL);printf(INITD_START,ctime(&t));
  struct stat s;
  if (stat("/system/etc/init.d",&s)==0) {
    if (S_ISDIR(s.st_mode)) {
      DIR* d = opendir("/system/etc/init.d");
      struct dirent* entry;
      if (d) {
        while ((entry = readdir(d)) != NULL) {
          if (entry->d_name && entry->d_name[0]=='S') {
            char path[PATH_MAX];
            sprintf(path,"/system/etc/init.d/%s",entry->d_name);
            t=time(NULL);printf(INITD_STARTX,ctime(&t),entry->d_name);
            sh(path);
            t=time(NULL);printf(INITD_DONEX,ctime(&t),entry->d_name);
          }
        }
        closedir(d);
      }
    }
  }
  t=time(NULL);printf(INITD_DONE,ctime(&t));

  if (strcmp(get_conf_def("init.rmsymlinks",value,"1"),"1")==0) {
    char** command = steam_command_list;
    while (*command) {
      rmsymlink(*command);
      command++;
    }
  }

  // remount ro
  call_busybox("mount","-o","remount,ro","/",NULL);
  if (strcmp(get_conf_def("fs.system.ro",value,"1"),"1")==0) {
    // efs hiding needs rw rights
    if (strcmp(get_conf_def("fs.system.efs",value,"0"),"0")==0) {
      call_busybox("mount","-o","remount,ro","/dev/block/stl9","/system",NULL);
    }
  }

  // signal earlyinit that logo playing will start soon
  // so it can disable the display engine if needed
  property_set("dev.defaultclassstarted","1");

  if (startanim==1) {
    char* argp[] = {"playlogos1",NULL};
    property_set("ctl.stop","bootanim");
    execve("/system/bin/playlogos1",argp,environ);
  } else if (startanim==2) {
    // signal init to start bootanimation
    property_set("ctl.start","bootanim");
  } else {
    property_set("ctl.stop","bootanim");
  }
  return 0;
}

int steam_earlyinit_main(int argc, char* argv[]) {
  freopen(EARLYINIT_LOG_FILE,"a+",stdout);setbuf(stdout,NULL);
  freopen(EARLYINIT_LOG_FILE,"a+",stderr);setbuf(stderr,NULL);
  sprintf(TEMPORARY_LOG_FILE,"%s",POSTINIT_LOG_FILE);
  init_conf();
  char value[VALUE_MAX_LENGTH];
  int donepinit = false;
  int shutdownscreen = true;
  int noscreen = true;
  if (strcmp(get_conf_def("earlyinit.graphics",value,"0"),"1")==0) noscreen = false;
  if (strcmp(get_conf_def("init.bootanim",value,"1"),"0")==0) { shutdownscreen = false; noscreen = false; }
  if (!noscreen) {
    ui_init();
    ui_show_progress(1,15);
    ui_set_page(TEXTCONTAINER_LOGCAT);
    ui_print(EARLY_LOGAPP);
    ui_print(EARLY_USAGE);
  }
  time_t t;

  t=time(NULL);printf(INITD_EARLYSTART,ctime(&t));
  struct stat s;
  if (stat("/system/etc/init.d",&s)==0) {
    if (S_ISDIR(s.st_mode)) {
      DIR* d = opendir("/system/etc/init.d");
      struct dirent* entry;
      if (d) {
        while ((entry = readdir(d)) != NULL) {
          if (entry->d_name && entry->d_name[0]=='E') {
            char path[PATH_MAX];
            sprintf(path,"/system/etc/init.d/%s",entry->d_name);
            t=time(NULL);printf(INITD_STARTX,ctime(&t),entry->d_name);
            sh(path);
            t=time(NULL);printf(INITD_DONEX,ctime(&t),entry->d_name);
          }
        }
        closedir(d);
      }
    }
  }
  t=time(NULL);printf(INITD_EARLYDONE,ctime(&t));

  while (true) {
    sleep(1);
    usleep(1000000/15);
    char value[PROPERTY_VALUE_MAX];
    // got the signal from post-init
    property_get("dev.defaultclassstarted",value,"");
    ui_set_progress(0.2);
    if (strcmp(value,"1")==0) {
      if (!donepinit) {
        printf(EARLY_POSTINITSTART);
        if (!noscreen && shutdownscreen) {
          if (get_ui_state()) ui_done();
        }
        if (!shutdownscreen) {
          call_busybox("killall","-9","playlogos1",NULL);
          call_busybox("killall","-9","bootanim",NULL);
          call_busybox("killall","-9","bootanimation",NULL);
        }
      }
      donepinit = true;
    }
    property_get("dev.bootcomplete",value,"");
    if (strcmp(value,"1")==0) {
    ui_set_progress(0.9);
      printf(EARLY_BOOTCOMPLETE);
      if (!noscreen) {
        if (get_ui_state()) ui_done();
      }
      sleep(2);
      ui_set_progress(1.0);
      printf(EARLY_KILLBOOTANIM);
      // if they don't kill themselves, we'll do it for them
      call_busybox("killall","-9","playlogos1",NULL);
      call_busybox("killall","-9","bootanim",NULL);
      call_busybox("killall","-9","bootanimation",NULL);
      printf(EARLY_DONE);
      // rebooting into recovery is broken if it's tryed earlier. I don't know why..., but it's ugly that the reboot command has to be put here, after everything is set up :(
      if (strcmp(get_conf_def("steam.installation",value,"0"),"1") == 0) reboot_recovery();
      if (strcmp(get_conf_def("steam.upgrade",value,"0"),"1") == 0) reboot_recovery();
      if (strcmp(get_conf_def("steam.uninstallation",value,"0"),"1") == 0) reboot_recovery();
      exit(0);
    }
  }
  return 0;
}

int convert_system(int fromfstype, int tofstype)
{
  struct statfs s;
  uint64_t sdcardfree = 0, sysused = 2147483647;
  char value[VALUE_MAX_LENGTH];
  call_busybox("mkdir","/mnt",NULL);
  call_busybox("mkdir","/mnt/sdcard",NULL);
  call_busybox("mkdir","/mnt/external_sd",NULL);
  call_busybox("mount","-t","vfat","-o","utf8",SDCARD_BLOCK_NAME,"/mnt/sdcard",NULL);
  call_busybox("mount","-t","vfat","-o","utf8",SDCARD2_BLOCK_NAME,"/mnt/external_sd",NULL);
  call_busybox("mkdir","/tmp/sys",NULL);
  if (fromfstype==(TYPE_RFS|TYPE_RFS_BAD)) {
    call_busybox("mount","-t","rfs","-o",TYPE_RFS_BAD_DEFAULT_MOUNT,SYSTEM_BLOCK_NAME,"/tmp/sys",NULL);
  } else {
    check_and_mount(fromfstype,SYSTEM_BLOCK_NAME,"loop4","tmp/sys",NULL);
  }
  call_busybox("mkdir","/system/lib",NULL);
  call_busybox("mkdir","/system/bin",NULL);
  call_busybox("mkdir","/system/etc",NULL);
  call_busybox("ln","-s","/system/etc","/etc",NULL);
  call_busybox("cp","/res/misc/mke2fs.conf","/system/etc",NULL); // to make mkfs.ext4 to work
  // these files are needed, as fat.format is dynamically linked
  call_busybox("cp","/tmp/sys/lib/liblog.so","/system/lib",NULL);
  call_busybox("cp","/tmp/sys/lib/libcutils.so","/system/lib",NULL);
  call_busybox("cp","/tmp/sys/lib/libc.so","/system/lib",NULL);
  call_busybox("cp","/tmp/sys/lib/libstdc++.so","/system/lib",NULL);
  call_busybox("cp","/tmp/sys/lib/libm.so","/system/lib",NULL);
  call_busybox("cp","/tmp/sys/lib/libdl.so","/system/lib",NULL);
  call_busybox("cp","/tmp/sys/bin/linker","/system/bin",NULL);
  setenv("LD_LIBRARY_PATH","/system/lib",1); // use the libs from that directory
  while (sdcardfree<sysused) {
    if (statfs("/mnt/sdcard",&s)==0) {
      sdcardfree = s.f_bavail*s.f_bsize / (uint64_t)(1024*1024);
      if (statfs("/system",&s)==0) {
        sysused = (s.f_blocks - s.f_bfree)*s.f_bsize / (uint64_t)(1024*952); // add ~10% to needed space
      }
    }
    if (sdcardfree<sysused) {
      ui_print(CONVERT_NO_SPACE);
      ui_print(CONVERT_SPACE_NEEDS,sysused,sdcardfree);
      if (get_conf("preinit.allowfm",value) && strcmp(value,"1")==0) {
        ui_print(CONVERT_LOAD_FM);
        file_manager();
      } else {
        ui_print(CONVERT_NO_FM);
        ui_print(INIT_HALT);
        while (true) {
          sleep(1);
        }
      }
    }
  }
  char backup_path[PATH_MAX];
  call_busybox("rm","-rf","/mnt/sdcard/.syssave",NULL);
  call_busybox("mkdir","/mnt/sdcard/.syssave",NULL);
  if (call_busybox("tar","-cvpf","/mnt/sdcard/.syssave/save.tar","/tmp/sys",NULL)) {
    ui_print(CONVERT_BACKUP_FAILED);
    ui_print(CONVERT_CONTINUE_ANYWAY);
  }
  unmount_filesystem("/tmp/sys");
  sync();
  filesystem_format(tofstype,SYSTEM_BLOCK_NAME,"loop4","tmp/sys",NULL);
  sync();
  check_and_mount(tofstype,SYSTEM_BLOCK_NAME,"loop4","tmp/sys",NULL);
  if (call_busybox("tar","-xvpf","/mnt/sdcard/.syssave/save.tar",NULL)) {
    ui_print(CONVERT_RESTORE_FAILED);
    if (get_conf("preinit.allowfm",value) && strcmp(value,"1")==0) {
      ui_print(CONVERT_LOAD_FM);
      file_manager();
    } else {
      ui_print(CONVERT_NO_FM);
      ui_print(INIT_HALT);
      call_busybox("rm","-rf","/mnt/sdcard/.syssave",NULL);
      while (true) sleep(1);
    }
  }
  call_busybox("mkdir","/mnt/sdcard/steam",NULL);
  call_busybox("rm","-rf","/mnt/sdcard/steam/sysconv",NULL);
  call_busybox("mkdir","/mnt/sdcard/steam/sysconv",NULL);
  sh("cp /tmp/*.log /mnt/sdcard/steam/sysconv");
  call_busybox("rm","-rf","/mnt/sdcard/.syssave",NULL);
  call_busybox("umount","/mnt/external_sd",NULL);
  call_busybox("umount","/mnt/sdcard",NULL);
  // in case of "faked" bad rfs, remove the flag
  call_busybox("sed","-i","s/^fs.system.rfs=.*/#fs.system.rfs=ok/","/tmp/sys/etc/steam.conf",NULL);
  unmount_filesystem("/tmp/sys");
  printf(CONVERT_WAIT_REBOOT);
  sleep(5);
  reboot_recovery();
  ui_print(INIT_HALT); // if reboot fails
  while (true) sleep(1);
  return 0;
}

int convert_filesystems(int oldcache,int newcache,int olddata, int newdata, int olddbdata, int newdbdata, char* secret)
{
  char* header[] = { CONVERT_FS_HEADER, NULL };
  char* items[] = { CONVERT_FS_FULLBACKUP, CONVERT_FS_ONLYBACKUP, CONVERT_FS_RMBACKUP, CONVERT_FS_RMALL, CONVERT_FS_CANCEL, NULL };
  char value[VALUE_MAX_LENGTH];
  int chosen_item = get_menu_selection(header,items,0);
  if (chosen_item>=0) printf("%s\n",items[chosen_item]);
  if (chosen_item>=0 && chosen_item != 4) {
    char tmp[PATH_MAX];
    nandroid_generate_timestamp_path(tmp);
    if (chosen_item==3) {
      // no backup
    } else {
      int flags = 0;
      call_busybox("mkdir","/mnt",NULL);
      call_busybox("mkdir","/mnt/sdcard",NULL);
      if (newcache && (oldcache!=newcache)) flags = flags | BACKUP_CACHE;
      if (newdata && (olddata!=newdata)) flags = flags | BACKUP_DATA;
      if (newdbdata && (olddbdata!=newdbdata)) flags = flags | BACKUP_DATADATA;
      ui_set_page(TEXTCONTAINER_MAIN);
      call_busybox("mount",NULL);
      if (nandroid_backup_flags(tmp,flags)!=0) {
        ui_set_page(TEXTCONTAINER_STDOUT);
        return -1;
      }
    }
    call_busybox("mount",NULL);
    if (newcache && (oldcache!=newcache)) {
      unmount_filesystem("/cache");
      filesystem_format(newcache,CACHE_BLOCK_NAME,"loop1","cache",secret);
      check_and_mount(newcache,CACHE_BLOCK_NAME,"loop1","cache",secret);
      sprintf(value,"%d",newcache);
      set_conf("fs.cache.type",value);
    }
    if (newdata && (olddata!=newdata)) {
      unmount_filesystem("/data");
      filesystem_format(newdata,DATA_BLOCK_NAME,"loop2","data",secret);
      check_and_mount(newdata,DATA_BLOCK_NAME,"loop2","data",secret);
      sprintf(value,"%d",newdata);
      set_conf("fs.data.type",value);
    }
#ifdef HAS_DATADATA
    if (newdbdata && (olddbdata!=newdbdata)) {
      unmount_filesystem("/dbdata");
      filesystem_format(newdbdata,DBDATA_BLOCK_NAME,"loop3","dbdata",secret);
      check_and_mount(newdbdata,DBDATA_BLOCK_NAME,"loop3","dbdata",secret);
      sprintf(value,"%d",newdbdata);
      set_conf("fs.dbdata.type",value);
    }
#endif
    call_busybox("mount",NULL);
    if (chosen_item==0 || chosen_item==2) {
      nandroid_restore(tmp,0,0,1,1,0);
    }
    if (chosen_item==2) {
      call_busybox("rm","-rf",tmp,NULL);
    }
    ui_set_page(TEXTCONTAINER_STDOUT);
  }
  return 0;
}

int autoload_modules()
{
  char value[VALUE_MAX_LENGTH];
  if (strcmp(get_conf_def("modules.autoload",value,"0"),"1")==0)
  {
    if (strcmp(get_conf_def("modules.allowsd",value,"0"),"1")==0) {
      call_busybox("mkdir","/mnt",NULL);
      call_busybox("mkdir","/mnt/sdcard",NULL);
      call_busybox("mount","-t","vfat","-o","utf8",SDCARD_BLOCK_NAME,"/mnt/sdcard",NULL);
      call_busybox("cp","/mnt/sdcard/steam/ext2/ext2.ko","/lib/modules",NULL);
      call_busybox("cp","/mnt/sdcard/steam/ext4/jbd2.ko","/lib/modules",NULL);
      call_busybox("cp","/mnt/sdcard/steam/ext4/ext4.ko","/lib/modules",NULL);
      call_busybox("cp","/mnt/sdcard/steam/jfs/jfs.ko","/lib/modules",NULL);
      call_busybox("umount","/mnt/sdcard",NULL);
    }

    if (!get_capability("fs.support.ext2",NULL)) {
      call_busybox("insmod","/lib/modules/ext2.ko",NULL);
    }
    if (!get_capability("fs.support.ext4",NULL)) {
      call_busybox("insmod","/lib/modules/jbd2.ko",NULL);
      call_busybox("insmod","/lib/modules/ext4.ko",NULL);
    }
    if (!get_capability("fs.support.jfs",NULL)) {
      call_busybox("insmod","/lib/modules/jfs.ko",NULL);
    }
    return 1;
  }
  return 0;
}

int steam_init_main(int argc, char* argv[]) {
  // STAGE 1: initialize proc, sys and tmp
  // If these fail we're doomed anyway...
  call_busybox("mkdir","/proc",NULL);
  call_busybox("mkdir","/sys",NULL);
  call_busybox("mkdir","/tmp",NULL);
  call_busybox("mount","-t","proc","proc","/proc",NULL);
  call_busybox("mount","-t","sysfs","sys","/sys",NULL);
  call_busybox("mount","-t","tmpfs","tmpfs","/tmp",NULL);

  // tmpfs is up, redirect stdout and stderr to /tmp
  freopen(INIT_LOG_FILE,"a+",stdout);setbuf(stdout,NULL);
  freopen(INIT_LOG_FILE,"a+",stderr);setbuf(stderr,NULL);
  printf("     /--------  /-----     /||        ||\n");
  printf("    / \\------/ //-----    //||\\      /||\n");
  printf("   /     ||   //         // ||\\\\    //||\n");
  printf("  /      ||  //         //  || \\\\  // ||\n");
  printf(" /-----/ || ||-----    //---||  \\\\//  ||\n");
  printf("/-----/  || ||-----   //----||   \\/   ||\n");
  printf("     /   || ||       //     ||        ||\n");
  printf("    /    || ||      //      ||        ||\n");
  printf("   /     || \\\\---- //       ||        ||\n");
  printf("  /      ||  \\\\---//        ||        ||\n");
  printf(" /                                Kernel\n");
  printf("with "EXPAND(RECOVERY_VERSION)"\n\n\n");

  // STAGE 2: load up modules and create initial directory and device system
  printf(INIT_STAGE,2);
  call_busybox("insmod","/modules/pvrsrvkm.ko",NULL);
  call_busybox("insmod","/modules/s3c_lcd.ko",NULL);
  call_busybox("insmod","/modules/s3c_bc.ko",NULL);

  call_busybox("insmod","/lib/modules/fsr.ko",NULL);
  call_busybox("insmod","/lib/modules/fsr_stl.ko",NULL);
  call_busybox("insmod","/lib/modules/rfs_glue.ko",NULL);
  call_busybox("insmod","/lib/modules/rfs_fat.ko",NULL);
  call_busybox("insmod","/lib/modules/j4fs.ko",NULL);
  call_busybox("insmod","/lib/modules/param.ko",NULL);

  call_busybox("mkdir","/dev",NULL);

  call_busybox("mkdir","/dev/block",NULL);
  call_busybox("mkdir","/dev/snd",NULL);

  call_busybox("mknod","/dev/null","c","1","3",NULL);
  call_busybox("mknod","/dev/zero","c","1","5",NULL);
  call_busybox("mknod","/dev/random","c","1","8",NULL);
  call_busybox("mknod","/dev/urandom","c","1","9",NULL);

  // internal and external sd card
  call_busybox("mknod","/dev/block/mmcblk0","b","179","0",NULL);
  call_busybox("mknod","/dev/block/mmcblk0p1","b","179","1",NULL);
  call_busybox("mknod","/dev/block/mmcblk0p2","b","179","2",NULL);

  call_busybox("mknod","/dev/block/mmcblk1","b","179","8",NULL);
  call_busybox("mknod","/dev/block/mmcblk1p1","b","179","9",NULL);

  // ROM blocks
  call_busybox("mknod","/dev/block/stl1","b","138","1",NULL);
  call_busybox("mknod","/dev/block/stl2","b","138","2",NULL);
  call_busybox("mknod","/dev/block/stl3","b","138","3",NULL);
  call_busybox("mknod","/dev/block/stl4","b","138","4",NULL);
  call_busybox("mknod","/dev/block/stl5","b","138","5",NULL);
  call_busybox("mknod","/dev/block/stl6","b","138","6",NULL);
  call_busybox("mknod","/dev/block/stl7","b","138","7",NULL);
  call_busybox("mknod","/dev/block/stl8","b","138","8",NULL);
  call_busybox("mknod","/dev/block/stl9","b","138","9",NULL);
  call_busybox("mknod","/dev/block/stl10","b","138","10",NULL);
  call_busybox("mknod","/dev/block/stl11","b","138","11",NULL);
  call_busybox("mknod","/dev/block/stl12","b","138","12",NULL);

  // loop devices. loop0 will be unused to keep it free for mods
  call_busybox("mknod","/dev/block/loop0","b","7","0",NULL);
  call_busybox("mknod","/dev/block/loop1","b","7","1",NULL);
  call_busybox("mknod","/dev/block/loop2","b","7","2",NULL);
  call_busybox("mknod","/dev/block/loop3","b","7","3",NULL);
  call_busybox("mknod","/dev/block/loop4","b","7","4",NULL);
  call_busybox("mknod","/dev/block/loop5","b","7","5",NULL);
  call_busybox("mknod","/dev/block/loop6","b","7","6",NULL);
  call_busybox("mknod","/dev/block/loop7","b","7","7",NULL);

  // framebuffer support for display
  call_busybox("mkdir","/dev/graphics",NULL);
  call_busybox("mknod","/dev/graphics/fb0","c","29","0",NULL);
  call_busybox("mknod","/dev/graphics/fb1","c","29","1",NULL);
  call_busybox("mknod","/dev/graphics/fb2","c","29","2",NULL);
  call_busybox("mknod","/dev/graphics/fb3","c","29","3",NULL);
  call_busybox("mknod","/dev/graphics/fb4","c","29","4",NULL);

  // touch screen and keyboard support
  call_busybox("mkdir","/dev/input",NULL);
  call_busybox("mknod","/dev/input/event0","c","13","64",NULL);
  call_busybox("mknod","/dev/input/event1","c","13","65",NULL);
  call_busybox("mknod","/dev/input/event2","c","13","66",NULL);
  call_busybox("mknod","/dev/input/event3","c","13","67",NULL);
  call_busybox("mknod","/dev/input/event4","c","13","68",NULL);
  call_busybox("mknod","/dev/input/mice","c","13","63",NULL);
  call_busybox("mknod","/dev/input/mouse0","c","13","32",NULL);

  call_busybox("mkdir","/dev/mapper",NULL);

  printf(INIT_DEVICES_DONE);
  sprintf(TEMPORARY_LOG_FILE,"%s",INIT_LOG_FILE);

  char value[VALUE_MAX_LENGTH];
  int isrecovery = 0;
  FILE* f = fopen("/proc/cmdline","r");
  if (f) {
    int r = fread(value,1,VALUE_MAX_LENGTH-1,f);
    value[r]='\0';
    if (strstr(value,"bootmode=2")) isrecovery = 1;
  }
  int usegraphics = 0;
  if (get_conf("preinit.graphics",value) && strcmp(value,"1")==0) usegraphics = 1;
  if (isrecovery && get_conf("preinit.recovery.graphics",value) && strcmp(value,"1")==0) usegraphics = 1;
  if (usegraphics) {
    printf(INIT_LOAD_GRAPHICS);
    ui_init();
    ui_set_page(TEXTCONTAINER_STDOUT);
    ui_show_progress(1,0);
  }
  if (isrecovery) printf("\n\nRECOVERY MODE\n\n");
  // creating crsymlinks
  char** command = steam_command_list;
  while (*command) {
    crsymlink(*command);
    command++;
  }

  if (usegraphics) ui_set_progress(0.1);
  setenv("PATH","/sbin:/system/sbin:/system/bin:/system/xbin",1);
  if (usegraphics) ui_set_progress(0.2);

  printf(INIT_CREATE_MOUNT);
  autoload_modules();
  call_busybox("mkdir","/cache",NULL);
#ifdef HAS_DATADATA
  call_busybox("mkdir","/dbdata",NULL);
#endif
  call_busybox("mkdir","/data",NULL);
  call_busybox("mkdir","/system",NULL);
  if (usegraphics) ui_set_progress(0.3);

  // STAGE 3: Do everything to get /system mounted
  printf(INIT_STAGE,3);
  // we don't know much about /system, as the config file is stored there4
  int system_type = 0;
  int count = 0;
  while (system_type==0 && count<20) {
    system_type = filesystem_check(MAIN_BLOCK_NAME);
    sleep(1);
    count++;
  }
  if (system_type&TYPE_RFS_BAD) {
    // inconsistent rfs state, asking user to fix it
    if (!usegraphics) { ui_init(); ui_set_page(TEXTCONTAINER_STDOUT); }
    char* headers[] = { INIT_RFS_INCONSISTENT_FIX_HEADER, NULL };
    char* items[] = { INIT_RFS_INCONSISTENT_FIX_TAR, INIT_RFS_INCONSISTENT_FIX_NONE, INIT_RFS_INCONSISTENT_FIX_NONE2, NULL };
    int chosen_item = -1;
    while (chosen_item<0) {
      chosen_item = get_menu_selection(headers,items,0);
    }
    if (chosen_item==0) {
      convert_system(TYPE_RFS|TYPE_RFS_BAD,TYPE_RFS);
    } else if (chosen_item==1) {
      call_busybox("mount","-t","rfs","-o",TYPE_RFS_BAD_DEFAULT_MOUNT,MAIN_BLOCK_NAME,MAIN_BLOCK_MTP,NULL);
    } else if (chosen_item==2) {
      call_busybox("mount","-t","rfs","-o",TYPE_RFS_DEFAULT_MOUNT,MAIN_BLOCK_NAME,MAIN_BLOCK_MTP,NULL);
    }
    if (!usegraphics) ui_done();
  } else {
    check_and_mount(system_type,MAIN_BLOCK_NAME,"loop4",MAIN_BLOCK_LABEL,NULL);
  }
  if (strcmp(MAIN_BLOCK_NAME,SYSTEM_BLOCK_NAME)) {
    // we need to mount system if it's not the main partition
    system_type = mount_from_config_or_autodetect("fs.system.type",SYSTEM_BLOCK_NAME,"loop4","system",NULL);
  }
  // system is now mounted/fixed
  call_busybox("rm","/system/bin/fat.format",NULL);
  call_busybox("ln","-s","/system/etc/","/etc",NULL);
  call_busybox("cp","/res/misc/mke2fs.conf","/etc",NULL);
  if (usegraphics) ui_set_progress(0.4);

  // STAGE 4: check for Steam
  printf(INIT_STAGE,4);
  // check if steam is installed. If not ask the user whether he wants to install it or not.
  struct stat s;
  if (stat(CONFIG_SYSTEM,&s) || s.st_size<10) {
    // nope
    if (!usegraphics) { ui_init(); ui_set_page(TEXTCONTAINER_STDOUT); }
    char* headers[] = { INSTALL_STEAM_HEADER, NULL };
    char* items[] = { INSTALL_STEAM_YES, INSTALL_STEAM_NO, NULL };
    int chosen_item = -1;
    while (chosen_item<0) {
      chosen_item = get_menu_selection(headers,items,0);
    }
    if (chosen_item==0) {
      call_busybox("rm","/system/etc/steam.conf",NULL);
      call_busybox("cp",CONFIG_SBIN,CONFIG_SYSTEM,NULL);
      call_busybox("touch",CONFIG_SYSTEM,NULL);
      init_conf();
      set_conf("steam.installation","1");
    } else {
      init_conf();
    }
    if (!usegraphics) ui_done();
  } else {
    // check version numbers
    int diff = 0;
    char version[VALUE_MAX_LENGTH];
    char variant[VALUE_MAX_LENGTH];
    char varvers[VALUE_MAX_LENGTH];
    version[0]='\0'; get_conf_ro_from(CONFIG_SYSTEM,"steam.version",version);
    value[0]='\0';  get_conf_ro("steam.version",value);
    if (strcmp(value,version)) diff = 1;
    variant[0]='\0';   get_conf_ro_from(CONFIG_SYSTEM,"steam.variant",variant);
    value[0]='\0';  get_conf_ro("steam.variant",value);
    if (strcmp(value,variant)) diff = 1;
    varvers[0]='\0';   get_conf_ro_from(CONFIG_SYSTEM,"steam.variant.version",varvers);
    value[0]='\0';  get_conf_ro("steam.variant.version",value);
    if (strcmp(value,varvers)) diff = 1;
    if (diff) {
      if (!usegraphics) { ui_init(); ui_set_page(TEXTCONTAINER_STDOUT); }
      char* headers[] = { UPGRADE_STEAM_HEADER, NULL };
      char* items[] = { UPGRADE_STEAM_YES, UPGRADE_STEAM_NO, UPGRADE_STEAM_SKIP, NULL };
      int chosen_item = -1;
      while (chosen_item<0) {
        chosen_item = get_menu_selection(headers,items,0);
      }
      if (!usegraphics) ui_done();
      if (chosen_item==0) {
        call_busybox("rm","/system/etc/steam.conf.old",NULL);
        call_busybox("mv","/system/etc/steam.conf","/system/etc/steam.conf.old",NULL);
        sh("/sbin/busybox cat "CONFIG_SBIN" "CONFIG_SYSTEM".old > "CONFIG_SYSTEM);
        call_busybox("touch",CONFIG_SYSTEM,NULL);
        init_conf();
        set_conf("steam.old.version",version);
        set_conf("steam.old.variant",variant);
        set_conf("steam.old.variant.version",varvers);
        set_conf("steam.upgrade","1");
      } else if (chosen_item==1) {
        call_busybox("rm","/system/etc/steam.conf",NULL);
        call_busybox("cp",CONFIG_SBIN,CONFIG_SYSTEM,NULL);
        call_busybox("touch",CONFIG_SYSTEM,NULL);
        init_conf();
        set_conf("steam.installation","1");
      } else {
        init_conf();
      }
    } else {
      init_conf();
    }
  }

  // save the system type
  sprintf(value,"%d",system_type);
  set_conf("fs.system.type",value);

  // we check this again, as the rw config might have set this variable too
  autoload_modules();
  if (strcmp(get_conf_def("preinit.graphics",value,"0"),"1")==0 ||
     (isrecovery && strcmp(get_conf_def("preinit.recovery.graphics",value,"0"),"1")==0)) {
    if (!usegraphics) {
      usegraphics = 1;
      printf(INIT_LOAD_GRAPHICS);
      ui_init();
      ui_set_page(TEXTCONTAINER_STDOUT);
      ui_show_progress(1,0);
      ui_set_progress(0.5); //we're at 50% already, wow! :)
    }
  }

  // STAGE 5: convert filesystem on /system if needed
  printf(INIT_STAGE,5);
  if (get_conf("fs.system.convertto",value)) {
    int newfs = 0;
    if (sscanf(value,"%d",&newfs)!=1) newfs = 0;
    set_conf("fs.system.convertto",NULL);
    if (newfs&TYPE_FSTYPE_MASK) {
      if (system_type!=newfs) {
        if (!usegraphics) ui_init();
        if (confirm_selection(CONVERT_SYSTEM_SURE,CONVERT_SYSTEM_CONFIRM)) {
          unmount_filesystem("/system");
          convert_system(system_type,newfs);
        }
        if (!usegraphics) ui_done();
      }
    }
  }

  if (strcmp(get_conf_def("fs.system.efs",value,"0"),"1")==0) {
    struct stat s;
    if (stat("/system/efs",&s)) {
      // create a copy of /efs inside /system
      call_busybox("mkdir","/efs",NULL);
      call_busybox("mount","-t","rfs","-o",TYPE_RFS_DEFAULT_MOUNT,EFS_BLOCK_NAME,"/efs",NULL);
      call_busybox("cp","-R","/efs","/system",NULL);
      call_busybox("umount","/efs",NULL);
      call_busybox("rmdir","/efs",NULL);
    }
    // and use that
    call_busybox("ln","-s","/system/efs","/efs",NULL);
  } else {
    // else mount /efs normally
    call_busybox("rm","-rf","/system/efs",NULL);
    call_busybox("mkdir","/efs",NULL);
    call_busybox("mount","-t","rfs","-o",TYPE_RFS_DEFAULT_MOUNT,EFS_BLOCK_NAME,"/efs",NULL);
  }

  // STAGE 6: Mount all other filesystems
  printf(INIT_STAGE,6);
  char secret[256];secret[0] = '\0';
  ui_set_progress(0.6);
  int cache_type = mount_from_config_or_autodetect("fs.cache.type",CACHE_BLOCK_NAME,"loop1","cache",secret);
  ui_set_progress(0.7);
  int data_type = mount_from_config_or_autodetect("fs.data.type",DATA_BLOCK_NAME,"loop2","data",secret);
  ui_set_progress(0.8);
#ifdef HAS_DATADATA
  int dbdata_type = mount_from_config_or_autodetect("fs.dbdata.type",DBDATA_BLOCK_NAME,"loop3","dbdata",secret);
#else
  int dbdata_type = 0;
#endif
  ui_set_progress(0.9);
  // STAGE 7: convert filesystems
  printf(INIT_STAGE,7);

  int newcache = 0;
  int newdata = 0;
  int newdbdata = 0;
  if (get_conf("fs.cache.convertto",value) && sscanf(value,"%d",&newcache)==1) {} else newcache=0;
  if (get_conf("fs.data.convertto",value) && sscanf(value,"%d",&newdata)==1) {} else newdata=0;
#ifdef HAS_DATADATA
  if (get_conf("fs.dbdata.convertto",value) && sscanf(value,"%d",&newdbdata)==1) {} else newdbdata=0;
#endif
  set_conf("fs.cache.convertto",NULL);
  set_conf("fs.data.convertto",NULL);
#ifdef HAS_DATADATA
  set_conf("fs.dbdata.convertto",NULL);
#endif
  if (newcache==cache_type) newcache=0;
  if (newdata==data_type) newdata=0;
#ifdef HAS_DATADATA
  if (newdbdata==data_type) newdbdata=0;
#endif
  if (newcache || newdata || newdbdata) {
    // at least one filesystem needs reformatting
    if (!usegraphics) ui_init();
    convert_filesystems(cache_type,newcache,data_type,newdata,dbdata_type,newdbdata,secret);
    if (!usegraphics) ui_done();
  }
  memset(secret,0,sizeof(secret));

  // STAGE 8: modify init.rc and start
  printf(INIT_STAGE,8);
  ui_set_progress(1.0);

  // this is device specific
  fix_init(isrecovery);

  //TODO: if we're on a modified initramfs, these settings won't turn the properties off!
  if (strcmp(get_conf_def("adb.root",value,"0"),"0")) {
    if ((isrecovery && strcmp(value,"1")==0) || (strcmp(value,"3")==0) || (strcmp(value,"2")==0)) {
      call_busybox("sed","-i","s/ro.debuggable=0/ro.debuggable=1/","default.prop",NULL);
      if ((isrecovery && strcmp(value,"1")==0) || (strcmp(value,"3")==0)) {
        call_busybox("sed","-i","s/ro.secure=1/ro.secure=0/","default.prop",NULL);
      }
    }
  }

  if (strcmp(get_conf_def("adb.boot",value,"0"),"0")) {
    if ((isrecovery && strcmp(value,"1")==0) || (strcmp(value,"2")==0)) {
      call_busybox("sed","-i","s/persist.service.adb.enable=0/persist.service.adb.enable=1/","default.prop",NULL);
    }
  }

  if (usegraphics) ui_done();
  // parent will continue, and load up init
  char* argp[] = {"init",NULL};
  execve("/init.original",argp,environ);


  // should never happen!
  ui_init();
  ui_print("This shouldn't happen!\n");
  while (true) {
    file_manager();
  }
  ui_done();
  return 1;
}
