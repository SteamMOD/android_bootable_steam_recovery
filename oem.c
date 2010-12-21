/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

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

#include "../steam_main/steam.h"
#include "system.h"
#include "minzip/Zip.h"

#include "extendedcommands.h"
#include "commands.h"
#include "steamext.h"

extern char** environ;

static int reboot_into_recovery() {
  __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "recovery");
  return 0;
}

struct aftConfig;
struct aftConfig {
  char* id;
  char* name;
  char* help;
  char** commands;
  int commnum;
  int selected;
  struct aftConfig* next;
} initialConfig;

int do_steam_afterburner(int mountopt)
{
  if (ensure_root_path_mounted("SDCARD:")) {
    return 0;
  }
  struct stat s;
  char path[PATH_MAX];
  char value[VALUE_MAX_LENGTH];
  char value2[VALUE_MAX_LENGTH];
  int found = 1;
  initialConfig.next = NULL;
  sprintf(path,"/mnt/sdcard/steam/afterburner_%s_%s.zip",get_conf_def("steam.variant",value,"steamkrnl"),get_conf_def("steam.version",value2,"0"));
  if (stat(path,&s)) {
    sprintf(path,"/mnt/sdcard/steam/afterburner_%s.zip",get_conf_def("steam.variant",value,"steamkrnl"));
    if (stat(path,&s)) {
      sprintf(path,"/mnt/sdcard/steam/afterburner.zip");
      if (stat(path,&s)) {
        found = 0;
      }
    }
  }
  if (!found && mountopt) {
    if (mountopt==2 || strcmp(get_conf_def("afterburner.askuser",value,"0"),"1")==0) {
      show_mount_usb_storage_menu(OEM_AFTERBURNER_ASKUSER);
      return do_steam_afterburner(0);
    }
    return 0;
  } else {
    ZipArchive zip;
    int err = mzOpenZipArchive(path, &zip);
    if (err != 0) {
      LOGE(OEM_AFTERBURNER_ERROR, path, err != -1 ? strerror(err) : "bad");
      return 0;
    }
    ZipEntry *config_file = mzFindZipEntry(&zip, "afterburner.conf");
    if (config_file) {
      int r[2];
      pipe(r);
      pid_t pid;
      if ((pid = fork())==0) {
        close(r[0]);
        mzExtractZipEntryToFile(&zip, config_file, r[1]);
        close(r[1]);
        _exit(0);
      } else {
        if (pid>0) {
          close(r[1]);
          struct aftConfig* curr = &initialConfig;
          FILE* f = fdopen(r[0],"r");
          if (f) {
            char command[1024+1];
            while (fgets(command,1024+1,f)) {
              int l = strlen(command);
              if (l>0) {
                if (command[l-1]=='\n') { command[l-1]='\0'; l--; }
                if (command[l-1]=='\r') { command[l-1]='\0'; l--; }
                if (strstr(command,"---[ID:")==command) {
                  curr->next = malloc(sizeof(struct aftConfig));
                  curr = curr->next;
                  curr->next = NULL;
                  curr->id = malloc(l-6);
                  strcpy(curr->id,command+7);
                  curr->name = NULL;
                  curr->help = NULL;
                  curr->commands = NULL;
                  curr->commnum = 0;
                  curr->selected = 0;
                } else if (strstr(command,"---[NAME:")==command) {
                  curr->name = malloc(l-8);
                  strcpy(curr->name,command+9);
                } else if (strstr(command,"---[HELP:")==command) {
                  curr->help = malloc(l-8);
                  strcpy(curr->help,command+9);
                } else if (strstr(command,"---[SELECTED:1")==command) {
                  curr->selected = 1;
                } else {
                  if (!curr->commands) {
                    curr->commands = calloc(1,sizeof(char*));
                    curr->commnum=1;
                  } else {
                    curr->commnum++;
                    curr->commands = realloc(curr->commands,curr->commnum*sizeof(char*));
                  }
                  curr->commands[curr->commnum-1] = malloc(l+1);
                  strcpy(curr->commands[curr->commnum-1],command);
                }
              }
            }
            fclose(f);
          }
        }
      }

      int chosen_item = 1;
      if (initialConfig.next) {
        for (;;) {
          ui_start_menu_ext();
          ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,OEM_AFTERBURNER_HEADER,NULL);
          ui_add_menu(0,-1,MENU_TYPE_ELEMENT,OEM_AFTERBURNER_DOINSTALL,OEM_AFTERBURNER_DOINSTALL_HELP);
          struct aftConfig* s = initialConfig.next;
          while (s) {
            ui_add_menu(s->selected?(int)s:0,(int)s,MENU_TYPE_CHECKBOX,s->name,s->help);
            s = s->next;
          }
          struct menuElement me;
          chosen_item = get_menu_selection_ext(chosen_item,&me);
          if (chosen_item==GO_BACK) {chosen_item = 0; ui_end_menu(); break; }
          if (me.group_id==-1) {
            if (confirm_selection(OEM_AFTERBURNER_CONFIRM,OEM_AFTERBURNER_YES)) {
              chosen_item = 1;
              break;
            }
          } else {
            if (me.group_id) {
              if (me.id==me.group_id) {
                ((struct aftConfig *)me.group_id)->selected = 0;
              } else {
                ((struct aftConfig *)me.group_id)->selected = 1;
              }
            }
          }
        }
        if (chosen_item) {
          struct aftConfig* s = initialConfig.next;
          while (s) {
            if (s->selected) {
              ui_print(OEM_AFTERBURNER_INSTALLING,s->id);
              int i;
              char path[PATH_MAX];
              strcpy(path,"/");
              for (i=0; i<s->commnum; i++) {
                if (strstr(s->commands[i],"set_dir ")==s->commands[i]) {
                  strcpy(path,s->commands[i]+8);
                } else if (strstr(s->commands[i],"extract ")==s->commands[i]) {
                  mzExtractRecursive(&zip,s->commands[i]+8,path,0,NULL,NULL,NULL);
                } else if (strstr(s->commands[i],"ui_print ")==s->commands[i]) {
                  ui_print("%s\n",s->commands[i]+9);
                } else if (strstr(s->commands[i],"setconf ")==s->commands[i]) {
                  char* key = s->commands[i]+8;
                  char* value = strchr(key,'=');
                  if (value) {
                    value[0]='\0';
                    value++;
                    set_conf(key,value);
                  }
                } else if (strstr(s->commands[i],"install_package ")==s->commands[i]) {
                  install_package(s->commands[i]+16);
                } else if (strstr(s->commands[i],"run_program ")==s->commands[i]) {
                  call_busybox("chmod","755",s->commands[i]+12,NULL);
                  call_busybox("sh","-c",s->commands[i]+12,NULL);
                } else if (strstr(s->commands[i],"mkdir ")==s->commands[i]) {
                  call_busybox("mkdir",s->commands[i]+6,NULL);
                } else if (strstr(s->commands[i],"rm ")==s->commands[i]) {
                  call_busybox("rm","-rf",s->commands[i]+3,NULL);
                } else if ((s->commands[i][0]!='\0') && (s->commands[i][0]!='#')) {
                  LOGW("Unknown command %s\n",s->commands[i]);
                }
              }
            }
            s = s->next;
          }
        }
      }
    }
    mzCloseZipArchive(&zip);
    return 1;
  }
}

int do_steam_install()
{
  // remove ULTK config files
  char value[VALUE_MAX_LENGTH];
  char value2[VALUE_MAX_LENGTH];
  call_busybox("rm","/system/etc/lagfix.conf",NULL);
  call_busybox("rm","/system/etc/lagfix.conf.old",NULL);
  call_busybox("rm","/system/etc/tweaks.conf",NULL);
  struct stat s;
  if (ensure_root_path_mounted("SYSTEM:")) {
    return 0;
  }
  if (ensure_root_path_mounted("DATADATA:")) {
    return 0;
  }
  if (ensure_root_path_mounted("SDCARD:")) {
    return 0;
  }
  // we can flash 128MB worth of afterburner.zip inside the dbdata
  if (stat("/dbdata/afterburner",&s)==0) {
    char path[PATH_MAX];
    call_busybox("mkdir","/mnt/sdcard/steam",NULL);
    sprintf(path,"/dbdata/afterburner/afterburner_%s_%s.zip",get_conf_def("steam.variant",value,"steamkrnl"),get_conf_def("steam.version",value2,"0"));
    call_busybox("cp",path,"/mnt/sdcard/steam",NULL);
    sprintf(path,"/dbdata/afterburner/afterburner_%s.zip",get_conf_def("steam.variant",value,"steamkrnl"));
    call_busybox("cp",path,"/mnt/sdcard/steam",NULL);
    sprintf(path,"/dbdata/afterburner/afterburner.zip");
    call_busybox("cp",path,"/mnt/sdcard/steam",NULL);
  }
  do_steam_afterburner(1);
  set_conf("steam.installation",NULL);
  __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "recovery");
  return 0;
}

int do_steam_upgrade()
{
  set_conf("steam.upgrade",NULL);
  return 0;
}

int do_steam_uninstall()
{
  char value[VALUE_MAX_LENGTH];
  int fsokay = 1;
  // fs should be rfs
  if (strcmp(get_conf_def("fs.data.type",value,"0"),"1")) fsokay = 0;
  if (strcmp(get_conf_def("fs.dbdata.type",value,"0"),"1")) fsokay = 0;
  if (strcmp(get_conf_def("fs.cache.type",value,"0"),"1")) fsokay = 0;
  if (strcmp(get_conf_def("fs.system.type",value,"0"),"1")) fsokay = 0;
  if (!fsokay) {
    set_conf("fs.data.convertto","1");
    set_conf("fs.dbdata.convertto","1");
    set_conf("fs.cache.convertto","1");
    set_conf("fs.system.convertto","1");
    reboot_into_recovery();
    return 1;
  }
  // remove BLN
  struct stat s;
  if (stat(LIBLIGHTS_DEST".backup",&s)==0) {
    call_busybox("rm",LIBLIGHTS_DEST,NULL);
    call_busybox("mv",LIBLIGHTS_DEST".backup",LIBLIGHTS_DEST,NULL);
    call_busybox("rm","/system/etc/bln.conf",NULL);
  }
  // remove config
  call_busybox("rm","/system/etc/steam.conf",NULL);
  reboot_into_recovery();
  return 0;
}

int do_steam_install_fromcache()
{
  // checks for rec2e and rec3e plugins. If found, asking the user
  // what to do and redirecting the installation to them.
  struct stat s;
  if (ensure_root_path_mounted("SYSTEM:")) {
    return 0;
  }
  int has_rec2e = 0;
  int has_rec3e = 0;
  char value[VALUE_MAX_LENGTH];
  if (stat("/system/etc/steam/rec2e/sbin/recovery",&s)==0) {
    has_rec2e = 1;
  }
  if (stat("/system/etc/steam/rec3e/sbin/recovery",&s)==0) {
    has_rec3e = 1;
  }

  if (strcmp(get_conf_def("modules.allowsd",value,"0"),"1")==0) {
    if (ensure_root_path_mounted("SDCARD:")==0) {
      if (stat("/mnt/sdcard/steam/rec2e/sbin/recovery",&s)==0) {
        has_rec2e = 2;
      }
      if (stat("/mnt/sdcard/steam/rec3e/sbin/recovery",&s)==0) {
        has_rec3e = 2;
      }
    }
  }
  if (!has_rec2e && !has_rec3e) return 0;
  struct menuElement me;
  ui_start_menu_ext();
  ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,OEM_RECOVERY_HEADER,NULL);
  if (has_rec2e) {
    ui_add_menu(1,1,MENU_TYPE_ELEMENT,OEM_RECOVERY_2E,OEM_RECOVERY_2E_HELP);
  }
  if (has_rec3e) {
    ui_add_menu(2,1,MENU_TYPE_ELEMENT,OEM_RECOVERY_3E,OEM_RECOVERY_3E_HELP);
  }
  ui_add_menu(3,1,MENU_TYPE_ELEMENT,OEM_RECOVERY_STEAM,OEM_RECOVERY_STEAM_HELP);
  int chosen_item = get_menu_selection_ext(1,&me);
  if (chosen_item == GO_BACK) { ui_end_menu(); return 0; }
  chosen_item = me.id;
  ui_end_menu();
  // they might need it
  call_busybox("cp","/sbin/fat.format","/system/bin/fat.format",NULL);
  if (chosen_item==1) {
    ui_done();
    call_busybox("rm","/sbin/recovery",NULL);
    if (has_rec2e==1) {
      sh("cp -Rf /system/etc/steam/rec2e/* /");
      call_busybox("chmod","755","/sbin/recovery",NULL);
    } else {
      sh("cp -Rf /mnt/sdcard/steam/rec2e/* /");
      call_busybox("chmod","755","/sbin/recovery",NULL);
    }
    char* argp[] = { "recovery", NULL };
    execve("/sbin/recovery",argp,environ);
    reboot_into_recovery();
    ui_init();
    return 1;
  } else if (chosen_item==2) {
    ui_done();
    call_busybox("rm","/sbin/recovery",NULL);
    if (has_rec2e==1) {
      sh("cp -Rf /system/etc/steam/rec3e/* /");
      call_busybox("chmod","755","/sbin/recovery",NULL);
    } else {
      sh("cp -Rf /mnt/sdcard/steam/rec3e/* /");
      call_busybox("chmod","755","/sbin/recovery",NULL);
    }
    char* argp[] = { "recovery", NULL };
    execve("/sbin/recovery",argp,environ);
    reboot_into_recovery();
    ui_init();
    return 1;
  }
  return 0;
}
