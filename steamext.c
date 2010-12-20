/* Copyright (C) 2010 Zsolt Sz Sztupák
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../steam_main/steam.h"

#include "bootloader.h"
#include "ui.h"
#include "cutils/properties.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "device.h"
#include "locale.h"
#include "config.h"

#include "extendedcommands.h"
#include "commands.h"
#include "steamext.h"
#include "nandroid.h"

extern char **environ;

int apply_ln(char* name) {
  char n[128];
  sprintf(n,"/system/xbin/%s",name);
  return symlink("/system/xbin/busybox",n);
}

int apply_rm(char* name) {
  char tmp[128];
  sprintf(tmp,"/system/bin/%s",name);
  return call_busybox("rm",tmp,NULL);
}

void apply_root_to_device(int mode) {
  ensure_root_path_mounted("SYSTEM:");
  ui_print(APPROOT_COPYING);

  ui_print(APPROOT_COPYING_SU);
  call_busybox("rm","/system/bin/su",NULL);
  call_busybox("rm","/system/xbin/su",NULL);
  call_busybox("cp","/res/misc/su","/system/xbin/su",NULL);
  call_busybox("chown","0.0","/system/xbin/su",NULL);
  call_busybox("chmod","4755","/system/xbin/su",NULL);

  ui_print(APPROOT_COPYING_APK);
  call_busybox("rm","/system/app/Superuser.apk",NULL);
  call_busybox("rm","/data/app/Superuser.apk",NULL);
  call_busybox("cp","/res/misc/Superuser.apk","/system/app/Superuser.apk",NULL);
  call_busybox("chown","0.0","/system/app/Superuser.apk",NULL);
  call_busybox("chmod","644","/system/app/Superuser.apk",NULL);

  ui_print(APPROOT_COPYING_BB);
  call_busybox("rm","/system/xbin/busybox",NULL);
  call_busybox("rm","/system/bin/busybox",NULL);
#ifdef STEAM_HAS_BUSYBOX
  call_busybox("cp","/sbin/steam","/system/xbin/busybox",NULL);
#else
  call_busybox("cp","/sbin/busybox","/system/xbin/busybox",NULL);
#endif
  call_busybox("chmod","755","/system/xbin/busybox",NULL);

  ui_print(APPROOT_CRSYMLINK);
  char** command = steam_command_list;
  while (*command) {
    apply_ln(*command);
    command++;
  }

  if (mode>0) {
    ui_print(APPROOT_RMSOME);
    apply_rm("cat");
    apply_rm("chmod");
    apply_rm("chown");
    apply_rm("dd");
    apply_rm("ln");
    apply_rm("ls");
    apply_rm("mkdir");
    apply_rm("mv");
    apply_rm("rm");
    apply_rm("rmdir");
    if (mode>1) {
      ui_print(APPROOT_RMMORE);
      apply_rm("cmp");
      apply_rm("date");
      apply_rm("df");
      apply_rm("dmesg");
      apply_rm("id");
      apply_rm("insmod");
      apply_rm("kill");
      apply_rm("lsmod");
      apply_rm("mount");
      apply_rm("printenv");
      apply_rm("ps");
      apply_rm("renice");
      apply_rm("sleep");
      apply_rm("sync");
      apply_rm("top");
      apply_rm("umount");
    }
  }
  ui_print(APPROOT_DONE);
}

void tweak_menu() {
  int chosen_item = 1;
  for (;;)
  {
    struct menuElement me;
    char value[VALUE_MAX_LENGTH];
    int adbboot = 0;
    int adbroot = 0;
    int bootlog = 0;
    int bootanim = 0;
    int iosched = 0;
    int kernelvm = 0;
    int kernelsched = 0;
    int misc = 0;
    int sysrw = 0;
    if (get_conf("adb.boot",value) && sscanf(value,"%d",&adbboot)==1) {} else adbboot = 0;
    if (get_conf("adb.root",value) && sscanf(value,"%d",&adbroot)==1) {} else adbroot = 0;
    if (strcmp(get_conf_def("preinit.recovery.graphics",value,"0"),"1")==0) bootlog = 1;
    if (strcmp(get_conf_def("preinit.graphics",value,"0"),"1")==0) bootlog = 2;
    if (get_conf("init.bootanim",value) && sscanf(value,"%d",&bootanim)==1) {} else bootanim = 1;
    if (get_conf("tweaks.iosched",value) && sscanf(value,"%d",&iosched)==1) {} else iosched = 0;
    if (get_conf("tweaks.kernelvm",value) && sscanf(value,"%d",&kernelvm)==1) {} else kernelvm = 0;
    if (get_conf("tweaks.kernelsched",value) && sscanf(value,"%d",&kernelsched)==1) {} else kernelsched = 0;
    if (get_conf("tweaks.misc",value) && sscanf(value,"%d",&misc)==1) {} else misc = 0;
    if (get_conf("fs.system.ro",value) && sscanf(value,"%d",&sysrw)==1) {} else sysrw = 1;
    ui_start_menu_ext();
    ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,MENU_TWEAKS_HEADER,NULL);
    
    ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,MENU_TWEAKS_ADB,NULL);
    ui_add_menu(adbboot+1,1,MENU_TYPE_RADIOBOX,MENU_TWEAKS_ADB_DEF,MENU_TWEAKS_ADB_DEF_HELP);
    ui_add_menu(adbboot+1,2,MENU_TYPE_RADIOBOX,MENU_TWEAKS_ADB_REC,MENU_TWEAKS_ADB_REC_HELP);
    ui_add_menu(adbboot+1,3,MENU_TYPE_RADIOBOX,MENU_TWEAKS_ADB_ON,MENU_TWEAKS_ADB_ON_HELP);

    ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,MENU_TWEAKS_ADB_ROOT,NULL);
    ui_add_menu(adbroot+4,4,MENU_TYPE_RADIOBOX,MENU_TWEAKS_ADB_ROOT_OFF,MENU_TWEAKS_ADB_ROOT_OFF_HELP);
    ui_add_menu(adbroot+4,5,MENU_TYPE_RADIOBOX,MENU_TWEAKS_ADB_ROOT_REC,MENU_TWEAKS_ADB_ROOT_REC_HELP);
    ui_add_menu(adbroot+4,6,MENU_TYPE_RADIOBOX,MENU_TWEAKS_ADB_ROOT_ASK,MENU_TWEAKS_ADB_ROOT_ASK_HELP);
    ui_add_menu(adbroot+4,7,MENU_TYPE_RADIOBOX,MENU_TWEAKS_ADB_ROOT_ALWAYS,MENU_TWEAKS_ADB_ROOT_ALWAYS_HELP);

    ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,MENU_TWEAKS_BOOTLOG,NULL);
    ui_add_menu(bootlog+8,8,MENU_TYPE_RADIOBOX,MENU_TWEAKS_BOOTLOG_NO,MENU_TWEAKS_BOOTLOG_NO_HELP);
    ui_add_menu(bootlog+8,9,MENU_TYPE_RADIOBOX,MENU_TWEAKS_BOOTLOG_REC,MENU_TWEAKS_BOOTLOG_REC_HELP);
    ui_add_menu(bootlog+8,10,MENU_TYPE_RADIOBOX,MENU_TWEAKS_BOOTLOG_ALWAYS,MENU_TWEAKS_BOOTLOG_ALWAYS_HELP);

    ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,MENU_TWEAKS_BOOTANIM,NULL);
    ui_add_menu(bootanim+11,11,MENU_TYPE_RADIOBOX,MENU_TWEAKS_BOOTANIM_LOG,MENU_TWEAKS_BOOTANIM_LOG_HELP);
    ui_add_menu(bootanim+11,12,MENU_TYPE_RADIOBOX,MENU_TWEAKS_BOOTANIM_DEF,MENU_TWEAKS_BOOTANIM_DEF_HELP);
    ui_add_menu(bootanim+11,13,MENU_TYPE_RADIOBOX,MENU_TWEAKS_BOOTANIM_ANDROID,MENU_TWEAKS_BOOTANIM_ANDROID_HELP);

    ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,MENU_TWEAKS_TWEAKS,NULL);   
    ui_add_menu(iosched*16,16,MENU_TYPE_CHECKBOX,MENU_TWEAKS_IOSCHED,MENU_TWEAKS_IOSCHED_HELP);
    ui_add_menu(kernelvm*17,17,MENU_TYPE_CHECKBOX,MENU_TWEAKS_KERNELVM,MENU_TWEAKS_KERNELVM_HELP);
    ui_add_menu(kernelsched*18,18,MENU_TYPE_CHECKBOX,MENU_TWEAKS_KERNELSCHED,MENU_TWEAKS_KERNELSCHED_HELP);
    ui_add_menu(misc*19,19,MENU_TYPE_CHECKBOX,MENU_TWEAKS_MISC,MENU_TWEAKS_MISC_HELP);
    ui_add_menu(sysrw*20,20,MENU_TYPE_CHECKBOX,MENU_TWEAKS_SYS_RW,MENU_TWEAKS_SYS_RW_HELP);
    chosen_item = get_menu_selection_ext(chosen_item,&me);
    if (chosen_item == GO_BACK) {
      ui_end_menu();
      break;
    }
    if ((me.group_id>=1) && (me.group_id<=3)) { adbboot = me.group_id-1; sprintf(value,"%d",adbboot); set_conf("adb.boot",value);  }
    if ((me.group_id>=4) && (me.group_id<=7)) { adbroot = me.group_id-4; sprintf(value,"%d",adbroot); set_conf("adb.root",value);  }
    if ((me.group_id>=8) && (me.group_id<=10))  {
      if (me.group_id==8) {
        set_conf("preinit.recovery.graphics","0");
        set_conf("preinit.graphics","0");
      } else if (me.group_id==9) {
        set_conf("preinit.recovery.graphics","1");
        set_conf("preinit.graphics","0");
      } else if (me.group_id==10) {
        set_conf("preinit.recovery.graphics","1");
        set_conf("preinit.graphics","1");
      }
    }
    if ((me.group_id>=11) && (me.group_id<=13)) {
      if (me.group_id==11) {
        set_conf("init.bootanim","0");
        set_conf("earlyinit.graphics","1");
      } else if (me.group_id==12) {
        set_conf("init.bootanim","1");
        set_conf("earlyinit.graphics","0");
      } else if (me.group_id==13) {
        set_conf("init.bootanim","2");
        set_conf("earlyinit.graphics","0");
      }
    }
    if (me.group_id==16) { iosched = iosched?0:1; sprintf(value,"%d",iosched); set_conf("tweaks.iosched",value); }
    if (me.group_id==17) { kernelvm = kernelvm?0:1; sprintf(value,"%d",kernelvm); set_conf("tweaks.kernelvm",value); }
    if (me.group_id==18) { kernelsched = kernelsched?0:1; sprintf(value,"%d",kernelsched); set_conf("tweaks.kernelsched",value); }
    if (me.group_id==19) { misc = misc?0:1; sprintf(value,"%d",misc); set_conf("tweaks.misc",value); }
    if (me.group_id==20) { sysrw = sysrw?0:1; sprintf(value,"%d",sysrw); set_conf("fs.system.ro",value); }
    ui_end_menu();
  }
}

void fsreformat_menu() {
  int chosen_item = 1;
  struct menuElement me;
#define PARTITION_NUMBERS 4
  static char *partitions[PARTITION_NUMBERS] = { "data","dbdata","cache","system" };
  int oldconf[PARTITION_NUMBERS];
  int newconf[PARTITION_NUMBERS];
  int noefs = 0;
  int badrfs = 0;
  char key[KEY_MAX_LENGTH];
  char value[VALUE_MAX_LENGTH];
  int ext2support = get_capability("fs.support.ext2",NULL);
  int ext4support = get_capability("fs.support.ext4",NULL);
  int jfssupport = get_capability("fs.support.jfs",NULL);
  for (;;)
  {
    int i;
    noefs = 0;
    badrfs = 0;
    for (i=0; i<PARTITION_NUMBERS; i++) {
      sprintf(key,"fs.%s.type",partitions[i]);
      if (get_conf(key,value) && sscanf(value,"%d",&oldconf[i])) {} else oldconf[i] = 0;
      sprintf(key,"fs.%s.convertto",partitions[i]);
      if (get_conf(key,value) && sscanf(value,"%d",&newconf[i])) {} else newconf[i] = oldconf[i];
    }
    if (strcmp(get_conf_def("fs.system.efs",value,"0"),"1")==0) {
      noefs = 1;
    }
    if (strcmp(get_conf_def("fs.system.rfs",value,"ok"),"bad")==0) {
      badrfs = 1;
    }
    ui_start_menu_ext();
    ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,MENU_FS_HEADER,NULL);

    ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,MENU_SCHEME_HEADER,NULL);
    ui_add_menu(0,-1,MENU_TYPE_ELEMENT,MENU_SCHEME_DISABLED,MENU_SCHEME_DISABLED_HELP);
    if (ext4support) {
      ui_add_menu(1,-1,MENU_TYPE_ELEMENT,MENU_SCHEME_EXT4,MENU_SCHEME_EXT4_HELP);
      ui_add_menu(2,-1,MENU_TYPE_ELEMENT,MENU_SCHEME_EXT4_NORFS,MENU_SCHEME_EXT4_NORFS_HELP);
      if (ext2support) ui_add_menu(3,-1,MENU_TYPE_ELEMENT,MENU_SCHEME_EXT4_OVERKILL,MENU_SCHEME_EXT4_OVERKILL_HELP);
    }
    if (jfssupport) {
      ui_add_menu(4,-1,MENU_TYPE_ELEMENT,MENU_SCHEME_JFS,MENU_SCHEME_JFS_HELP);
      ui_add_menu(5,-1,MENU_TYPE_ELEMENT,MENU_SCHEME_JFS_NORFS,MENU_SCHEME_JFS_NORFS_HELP);
    }
    if (ext4support) {
      ui_add_menu(6,-1,MENU_TYPE_ELEMENT,MENU_SCHEME_EXT4_CRYPT,MENU_SCHEME_EXT4_CRYPT_HELP);
    }
   
    ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,MENU_FS_ADVANCED,NULL);
    for (i=0; i<PARTITION_NUMBERS; i++) {
      ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,partitions[i],NULL);
      ui_add_menu(i*8192+(newconf[i]&TYPE_RFS),i*8192+TYPE_RFS,MENU_TYPE_RADIOBOX,MENU_FS_RFS,MENU_FS_RFS_HELP);
      if (ext2support) {
        ui_add_menu(i*8192+(newconf[i]&TYPE_EXT2),i*8192+TYPE_EXT2,MENU_TYPE_RADIOBOX,MENU_FS_EXT2,MENU_FS_EXT2_HELP);
      }
      if (ext4support) {
        ui_add_menu(i*8192+(newconf[i]&TYPE_EXT4),i*8192+TYPE_EXT4,MENU_TYPE_RADIOBOX,MENU_FS_EXT4,MENU_FS_EXT4_HELP);
      }
      if (jfssupport) {
        ui_add_menu(i*8192+(newconf[i]&TYPE_JFS),i*8192+TYPE_JFS,MENU_TYPE_RADIOBOX,MENU_FS_JFS,MENU_FS_JFS_HELP);
      }
      if ((strcmp(partitions[i],"system")) || (strcmp(get_conf_def("steam.edge.features",value,"0"),"1")==0)) {
        // we disallow loop and encrypt on system, unless someone wants to try it out...
        ui_add_menu(i*8192+(newconf[i]&TYPE_LOOP),i*8192+TYPE_LOOP,MENU_TYPE_CHECKBOX,MENU_FS_LOOP,MENU_FS_LOOP_HELP);
        ui_add_menu(i*8192+(newconf[i]&TYPE_CRYPT),i*8192+TYPE_CRYPT,MENU_TYPE_CHECKBOX,MENU_FS_CRYPT,MENU_FS_CRYPT_HELP);
      }
    }
    ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,MENU_FS_MISC,NULL);
    ui_add_menu(noefs*-2,-2,MENU_TYPE_CHECKBOX,MENU_FS_NORFS,MENU_FS_NORFS_HELP);
    ui_add_menu(badrfs*-3,-3,MENU_TYPE_CHECKBOX,MENU_FS_BADRFS,MENU_FS_BADRFS_HELP);
    ui_add_menu(0,-4,MENU_TYPE_ELEMENT,MENU_FS_RESET,MENU_FS_RESET_HELP);

    chosen_item = get_menu_selection_ext(chosen_item,&me);
    if (chosen_item == GO_BACK) {
      ui_end_menu();
      break;
    }
    if (me.group_id==-1) {
      switch (me.id) {
        case 0: set_conf("fs.cache.convertto","1");set_conf("fs.data.convertto","1");set_conf("fs.dbdata.convertto","1");set_conf("fs.system.convertto","1");set_conf("fs.system.efs","0");set_conf("fs.system.rfs",NULL);break;
        case 1: set_conf("fs.cache.convertto","4");set_conf("fs.data.convertto","4");set_conf("fs.dbdata.convertto","4");set_conf("fs.system.convertto","1");set_conf("fs.system.efs","0");set_conf("fs.system.rfs",NULL); break;
        case 2: set_conf("fs.cache.convertto","4");set_conf("fs.data.convertto","4");set_conf("fs.dbdata.convertto","4");set_conf("fs.system.convertto","4");set_conf("fs.system.efs","1");set_conf("fs.system.rfs",NULL);break;
        case 3: set_conf("fs.cache.convertto","260");set_conf("fs.data.convertto","260");set_conf("fs.dbdata.convertto","260");set_conf("fs.system.convertto","4");set_conf("fs.system.efs","1");set_conf("fs.system.rfs",NULL);break;
        case 4: set_conf("fs.cache.convertto","8");set_conf("fs.data.convertto","8");set_conf("fs.dbdata.convertto","8");set_conf("fs.system.convertto","1");set_conf("fs.system.efs","0");set_conf("fs.system.rfs",NULL);break;
        case 5: set_conf("fs.cache.convertto","8");set_conf("fs.data.convertto","8");set_conf("fs.dbdata.convertto","8");set_conf("fs.system.convertto","8");set_conf("fs.system.efs","1");set_conf("fs.system.rfs",NULL);break;
        case 6: set_conf("fs.cache.convertto","132");set_conf("fs.data.convertto","132");set_conf("fs.dbdata.convertto","132");set_conf("fs.system.convertto","4");set_conf("fs.system.efs","1");set_conf("fs.system.rfs",NULL);break;
      }
    } else if (me.group_id==-2) {
      if (me.id) {
        set_conf("fs.system.efs","0");
      } else {
        set_conf("fs.system.efs","1");
      }
    } else if (me.group_id==-3) {
      if (me.id) {
        set_conf("fs.system.rfs",NULL);
      } else {
        set_conf("fs.system.rfs","bad");
      }
    } else if (me.group_id==-4) {
      for (i=0;i<PARTITION_NUMBERS;i++) {
        newconf[i] = oldconf[i];
      }
    } else if (me.group_id>0) {
      int ison = (me.group_id==me.id)?1:0;
      int num = me.group_id/8192;
      int val = me.group_id%8192;
      int fstype = 0;
      int loop = newconf[num]&TYPE_LOOP;
      int crypt = newconf[num]&TYPE_CRYPT;
      if (val&TYPE_FSTYPE_MASK) {
        fstype = val;
      } else {
        fstype = newconf[num]&TYPE_FSTYPE_MASK;
      }
      if (val&TYPE_CRYPT) {
        if (!ison) crypt = 1; else crypt = 0;
      }
      if (val&TYPE_LOOP) {
        if (!ison) loop = 1; else loop = 0;
      }
      fstype = fstype | (loop?TYPE_LOOP:0);
      fstype = fstype | (crypt?TYPE_CRYPT:0);
      sprintf(key,"fs.%s.convertto",partitions[num]);
      sprintf(value,"%d",fstype);
      set_conf(key,value);
    }
    ui_end_menu();
  }
}


void run_console()
{
  set_console_cmd("");
  ui_set_console(1);

  int chosen_item = -1;
  ui_print(CONSOLE_BACK);
  ui_clear_key_queue();
  ui_set_show_text(1);
  while (1) {
    int key = ui_wait_key();
    int action = 0;
    if (key==BTN_MOUSE) {
      int sl = strlen(get_console_cmd());
      if (sl>0 && (get_console_cmd()[sl-1]=='\n')) {
        action = SELECT_ITEM;
      }
    }
    int visible = ui_text_visible();
    if (!action) {
      action = device_handle_key(key, visible);
    }
    if (action==GO_BACK) break;
    if (action==SELECT_ITEM) {
      char* cmd = get_console_cmd();
      int len = strlen(cmd);
      char path[PATH_MAX];
      getcwd(path,PATH_MAX);
      if (len>3 && cmd[0]=='c' && cmd[1]=='d' && cmd[2]==' ') {
        if (cmd[len-1]=='\n') cmd[len-1]='\0';
        if (chdir(cmd+3)) {
          ui_print(CONSOLE_BADDIR);
        }
        getcwd(path,PATH_MAX);
        ui_print("%s\n",cmd+3);
      } else {
        if (cmd[len-1]=='\n') cmd[len-1]='\0';
        ui_print("%s\n",cmd);
        __system(cmd);
        ui_print("\n");
      }
      ui_print("%s # ",path);
      set_console_cmd("");
    }
  }
  ui_set_show_text(0);
  ui_set_console(0);
  chdir("/");
}


void bln_menu() {
    for (;;)
    {
      unsigned int bln_enabled = 0;
      struct menuElement me;
      FILE* f = fopen("/system/etc/bln.conf","r");
      if (f) fscanf (f,"%u",&bln_enabled);
      ui_start_menu_ext();
      ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,MENU_BLN_HEADERS,NULL);
      ui_add_menu(bln_enabled,1,MENU_TYPE_CHECKBOX,MENU_BLN_ITEM,MENU_BLN_HELP);
      int chosen_item = get_menu_selection_ext(1,&me);
      if (chosen_item == GO_BACK) {
        ui_end_menu();
        break;
      }
      if (me.group_id==1) {
        bln_enabled = bln_enabled?0:1;
      }
      f = fopen("/system/etc/bln.conf","w+");
      if (f) {
        fprintf(f,"%u\n", bln_enabled);
        fclose(f);
      } else {
        ui_print(MENU_BLN_NOCONFIG);
      }
      ui_end_menu();
    }
}

static int checkclipboard(char** clipboard, int clipboardlen, char* name)
{
  if (clipboardlen) {
    int i;
    for (i=0; i<clipboardlen; i++) {
      if (strcmp(clipboard[i],name)==0) return 1;
    }
  }
  return 0;
}

static void addclipboard(char*** clipboard, int *clipboardlen, char* name)
{
  if (checkclipboard(*clipboard,*clipboardlen,name)) return;
  if (*clipboardlen) {
    *clipboard = realloc(*clipboard,sizeof(char*)*(*clipboardlen+1));
    (*clipboard)[*clipboardlen] = malloc(strlen(name)+1);
    strcpy((*clipboard)[*clipboardlen],name);
    (*clipboardlen)++;
  } else {
    *clipboard = malloc(sizeof(char*));
    (*clipboard)[0] = malloc(strlen(name)+1);
    strcpy((*clipboard)[0],name);
    *clipboardlen = 1;
  }
}

static void removeclipboard(char*** clipboard, int *clipboardlen, char* name)
{
  int i,is;
  is = -1;
  for (i=0; i<*clipboardlen; i++) {
    if (strcmp((*clipboard)[i],name)==0) {
      is = i;
    }
  }
  if (is>=0) {
    free((*clipboard)[is]);
    for (i=is+1; i<*clipboardlen; i++) {
      (*clipboard)[i-1] = (*clipboard)[i];
    }
    (*clipboardlen)--;
    *clipboard = realloc(*clipboard, (*clipboardlen)*sizeof(char*));
  }
}

static void clearclipboard(char*** clipboard, int *clipboardlen)
{
  int i;
  if (*clipboardlen) {
    for (i=0; i<*clipboardlen; i++) {
      free((*clipboard)[i]);
    }
    free(*clipboard);
    *clipboard = NULL;
    *clipboardlen = 0;
  }
}

void file_manager() {
  char path[PATH_MAX] = "/";
  char npath[PATH_MAX];
  char xpath[PATH_MAX];
  char** clipboard = NULL;
  int clipboardlen = 0;
  struct dirent* entry;
  struct stat stres;
  struct menuElement me;
  int i, type, id, group_id;
  int multichoice = 0;
  int oldchoice = 0;
  ui_print(FILEMANAGER_PRESSBACK);
  for (;;) {
    ui_start_menu_ext();
    ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,FILEMANAGER_HEADER,NULL);
    ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,FILEMANAGER_COMMANDS,NULL);
    ui_add_menu(multichoice,1,MENU_TYPE_CHECKBOX,FILEMANAGER_CLIPBOARD_MODE,FILEMANAGER_CLIPBOARD_MODE_HELP);
    ui_add_menu(1,4,MENU_TYPE_ELEMENT,FILEMANAGER_CONSOLE,FILEMANAGER_CONSOLE_HELP);
    ui_add_menu(2,4,MENU_TYPE_ELEMENT,FILEMANAGER_PARTMAN,FILEMANAGER_PARTMAN_HELP);
    ui_add_menu(3,4,MENU_TYPE_ELEMENT,FILEMANAGER_SAVELOG,FILEMANAGER_SAVELOG_HELP);
    if (clipboardlen) {
      ui_add_menu(1,5,MENU_TYPE_ELEMENT,FILEMANAGER_CLIP_MOVE,NULL);
      ui_add_menu(2,5,MENU_TYPE_ELEMENT,FILEMANAGER_CLIP_COPY,NULL);
      ui_add_menu(3,5,MENU_TYPE_ELEMENT,FILEMANAGER_CLIP_RM,NULL);
      ui_add_menu(4,5,MENU_TYPE_ELEMENT,FILEMANAGER_CLIP_CLEAR,NULL);
    }
    me.id = 0; me.group_id = 0; me.type = MENU_TYPE_GROUP_HEADER;
    me.text = malloc(strlen(path)+1); me.help = NULL;
    strcpy(me.text,path);
    ui_add_menu_ext(me);
    ui_add_menu(0,2,MENU_TYPE_ELEMENT,"..",NULL);
    int i = 0;
    DIR* d = opendir(path);
    while ((entry = readdir(d)) != NULL) {
      if (!((strcmp(entry->d_name,"..")==0) || (strcmp(entry->d_name,".")==0))) {
        sprintf(npath,"%s/%s",path,entry->d_name);
        if (realpath(npath,xpath)) {
          int res = stat(xpath,&stres);
          if (res==0) {
            i++;
            me.id = checkclipboard(clipboard,clipboardlen,xpath)?2:3; me.group_id = 2; me.type = multichoice?MENU_TYPE_CHECKBOX:MENU_TYPE_ELEMENT;
            me.text = malloc(strlen(entry->d_name)+2); me.help = NULL;
            if (S_ISDIR(stres.st_mode)) {
              sprintf(me.text,"%s/",entry->d_name);
            } else {
              strcpy(me.text,entry->d_name);
            }
            ui_add_menu_ext(me);
          } else {
            printf("WTF!\n");
          }
        }
      }
    }
    closedir(d);
    if (clipboardlen) {
      ui_add_menu(0,0,MENU_TYPE_GROUP_HEADER,FILEMANAGER_CLIPBOARD,NULL);
      for (i=0; i<clipboardlen; i++) {
        me.id = 3; me.group_id = 3; me.type = MENU_TYPE_CHECKBOX;
        me.text = malloc(strlen(clipboard[i])+1); me.help = NULL;
        strcpy(me.text,clipboard[i]);
        ui_add_menu_ext(me);
      }
    }
    int chosen_item = get_menu_selection_ext(oldchoice,&me);
    oldchoice = 0;
    if (me.group_id==1) {
      multichoice = !multichoice;
      oldchoice = chosen_item;
    } else if (me.group_id==4) {
      if (me.id==1) {
        chdir(path);
        ui_end_menu();
        run_console();
      } else if (me.id==2) {
        ui_end_menu();
        show_partition_menu();
      } else if (me.id==3) {
        ui_end_menu();
        call_busybox("mkdir","/mnt",NULL);
        call_busybox("mkdir","/mnt/sdcard",NULL);
        ensure_root_path_mounted("SDCARD:");
        call_busybox("mkdir","/mnt/sdcard/steam",NULL);
        call_busybox("mkdir","/mnt/sdcard/steam/logs",NULL);
        char backup_path[PATH_MAX];
        time_t t = time(NULL);
        struct tm *tmp = localtime(&t); 
        if (tmp==NULL) {
          struct timeval tp;
          gettimeofday(&tp, NULL);
          sprintf(backup_path, "/mnt/sdcard/steam/logs/%d", tp.tv_sec);
        } else {
          strftime(backup_path, PATH_MAX, "/mnt/sdcard/steam/logs/%F.%H.%M.%S", tmp);
        }
        call_busybox("mkdir",backup_path,NULL);
        char text[PATH_MAX];
        sprintf(text,"cp /tmp/*.log %s",backup_path);
        sh(text);
      }
    } else if (me.group_id==5) {
      char sys[PATH_MAX*2+10];
      if (me.id==2) {
        for (i=0; i<clipboardlen; i++) {
          sprintf(sys,"cp -R %s %s",clipboard[i],path);
          ui_print("%s\n",sys);
          __system(sys);
        }
        clearclipboard(&clipboard,&clipboardlen);
      } else if (me.id==1) {
        for (i=0; i<clipboardlen; i++) {
          sprintf(sys,"mv %s %s",clipboard[i],path);
          ui_print("%s\n",sys);
          __system(sys);
        }
        clearclipboard(&clipboard,&clipboardlen);
      } else if (me.id==3) {
        for (i=0; i<clipboardlen; i++) {
          sprintf(sys,"rm -rf %s",clipboard[i]);
          ui_print("%s\n",sys);
          __system(sys);
        }
        clearclipboard(&clipboard,&clipboardlen);
      } else if (me.id==4) {
        clearclipboard(&clipboard,&clipboardlen);
      }
    } else if (me.group_id==3) {
      removeclipboard(&clipboard,&clipboardlen,me.text);
    } else if (me.group_id==2) {
      if (multichoice && me.id!=0) {
        oldchoice = chosen_item;
        sprintf(npath,"%s/%s",path,me.text);
        if (realpath(npath,xpath)) {
          if (me.id==2) {
            removeclipboard(&clipboard,&clipboardlen,xpath);
            if (clipboardlen==0) oldchoice-=4;
          } else {
            addclipboard(&clipboard,&clipboardlen,xpath);
            if (clipboardlen==1) oldchoice+=4;
          }
        }
      } else {
        sprintf(npath,"%s/%s",path,me.text);
        ui_print(npath);ui_print("\n");
        int res = stat(npath,&stres);
        realpath(npath,xpath);
        if (res==0) {
          if (S_ISDIR(stres.st_mode)) {
            if (!realpath(npath,path)) {
              strcpy(path,npath);
            }
          } else {
            char* fheaders[] = {FILEMANAGER_FILECMD_HEADER,NULL};
            char* flist[] = {FILEMANAGER_FILECMD_CLIPADD,FILEMANAGER_FILECMD_CAT,FILEMANAGER_FILECMD_HEAD,FILEMANAGER_FILECMD_RM,NULL};
            int sel2 = get_menu_selection(fheaders,flist,0);
            if (sel2!=GO_BACK) {
              if (sel2==0) {
                addclipboard(&clipboard,&clipboardlen,xpath);
              } else if (sel2==3) {
                sprintf(npath,"rm %s",xpath);
                ui_print("%s\n",npath);
                __system(npath);
              } else if (sel2==1 || sel2==2) {
                FILE* f = fopen(npath,"r+");
                if (f) {
                  int lines = 0;
                  char buf[257];
                  int rcount;
                  while ((rcount = fread(buf,1,256,f))) {
                    buf[rcount] = '\0';
                    ui_print(buf);
                    lines++;
                    if ((sel2==2) && (lines>6)) break;
                    if (rcount!=256) break;
                  }
                  ui_print("\n");
                  fclose(f);
                }
              }
            } else {
              ui_print(ERROR_NOTOPEN);
            }
          }
        } else {
          ui_print(ERROR_UNKNOWN);
        }
        ui_print(FILEMANAGER_DIR,path);
      }
    }
    ui_end_menu();
    if (chosen_item == GO_BACK) break;
  }
  clearclipboard(&clipboard,&clipboardlen);
}

void apply_root_menu() {
    static char* headers[] = {  APPROOT_HEADER, NULL };
    static char* list[] = { APPROOT_SIMPLE, APPROOT_ADV, APPROOT_EXT, NULL };

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
              if (confirm_selection(APPROOT_CONFIRM_ROOT,APPROOT_CONFIRM_YES)) {
                apply_root_to_device(0);
              }
              break;
            case 1:
              if (confirm_selection(APPROOT_CONFIRM_ROOT,APPROOT_CONFIRM_YES)) {
                apply_root_to_device(1);
              }
              break;
            case 2:
              if (confirm_selection(APPROOT_CONFIRM_ROOT,APPROOT_CONFIRM_YES)) {
                apply_root_to_device(2);
              }
              break;
        }
    }

}


void show_advanced_lfs_menu() {
    static char* headers[] = {  STEAM_MENU_HEADER, NULL };

    static char* list[] = { STEAM_MENU_SU,
                            STEAM_MENU_BOOT,
                            STEAM_MENU_BLN,
                            STEAM_MENU_CWM,
                            STEAM_MENU_LAGFIX,
                            STEAM_MENU_FM,
                            STEAM_MENU_ABOUT,
                            NULL
    };

    struct stat ss;
      for (;;)
      {
          int chosen_item = get_menu_selection(headers, list, 0);
          if (chosen_item == GO_BACK)
              break;
          switch (chosen_item)
          {
              case 0:
              {
                apply_root_menu();
                break;
              }
              case 1:
              {
                struct stat s;
                if (stat("/system/etc/steam.conf",&s)==0) {
                  tweak_menu();
                } else {
                  ui_print(STEAM_NOT_AVAILABLE);
                }
                break;
              }
              case 2:
              {
                struct stat s;
                if (stat("/sys/class/misc/backlightnotification/enabled",&s)==0) {
                  bln_menu();
                } else {
                  ui_print(BLN_NOT_AVAILABLE);
                }
                break;
              }
              case 3: {
                show_advanced_menu();
                break;
              }
              case 4: {
                struct stat s;
                if (stat("/system/etc/steam.conf",&s)==0) {
                  fsreformat_menu();
                } else {
                  ui_print(STEAM_NOT_AVAILABLE);
                }
                break;
              }
              case 5:
              {
                file_manager();
                break;
              }
              case 6:
              {
                ui_print("\n\n\n\n\n\n\n\n");
                ui_print(EXPAND(RECOVERY_VERSION));
                ui_print("\n by SztupY (mail@sztupy.hu)\n\n");
                ui_print("Based on CWM by koush\n\n");
                ui_print("Logo by ZAGE\n\n");
                ui_print("Credits:\n");
                ui_print("  anzo,ChainFire,DocRambone,hardcore,\n");
                ui_print("  koush,nikademus,neldar,newmail,patience,\n");
                ui_print("  PFC,RyanZA,tariattila,XDA,ykk_fice,\n");
                ui_print("  ZAGE,z4ziggy\n\n");
                struct stat s;
                if (stat("/system/etc/steam.conf",&s)==0) {
                  char* headers[] = { STEAM_ABOUT_UNINSTALL, NULL };
                  char* items[] = { STEAM_ABOUT_UNINSTALL, NULL };
                  int chosen_item = get_menu_selection(headers,items,0);
                  if (chosen_item==0 && confirm_selection(STEAM_ABOUT_UNINSTALL_CONFIRM,STEAM_ABOUT_UNINSTALL_YES)) {
                    set_conf("steam.uninstallation","1");
                    ui_print(STEAM_ABOUT_UNINSTALL_OK);
                  }
                }
              }
          }
    }
}
