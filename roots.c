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

#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include <limits.h>

#include "mtdutils/mtdutils.h"
#include "mtdutils/mounts.h"
#include "mmcutils/mmcutils.h"
#include "minzip/Zip.h"
#include "roots.h"
#include "ui.h"
#include "device.h"
#include "extendedcommands.h"
#include "locale.h"
#include "device.h"
#include "config.h"
#include "system.h"
#include "../steam_main/steam.h"

int get_num_roots();
int truncate(const char* path, off_t length);

const RootInfo *
get_root_info_for_path(const char *root_path)
{
    const char *c;

    /* Find the first colon.
     */
    c = root_path;
    while (*c != '\0' && *c != ':') {
        c++;
    }
    if (*c == '\0') {
        return NULL;
    }
    size_t len = c - root_path + 1;
    size_t i;
    for (i = 0; i < get_num_roots(); i++) {
        RootInfo *info = &g_roots[i];
        if (strncmp(info->name, root_path, len) == 0) {
            return info;
        }
    }
    return NULL;
}

static const ZipArchive *g_package = NULL;
static char *g_package_path = NULL;

int
register_package_root(const ZipArchive *package, const char *package_path)
{
    if (package != NULL) {
        package_path = strdup(package_path);
        if (package_path == NULL) {
            return -1;
        }
        g_package_path = (char *)package_path;
    } else {
        free(g_package_path);
        g_package_path = NULL;
    }
    g_package = package;
    return 0;
}

int
is_package_root_path(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    return info != NULL && info->filesystem == g_package_file;
}

const char *
translate_package_root_path(const char *root_path,
        char *out_buf, size_t out_buf_len, const ZipArchive **out_package)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL || info->filesystem != g_package_file) {
        return NULL;
    }

    /* Strip the package root off of the path.
     */
    size_t root_len = strlen(info->name);
    root_path += root_len;
    size_t root_path_len = strlen(root_path);

    if (out_buf_len < root_path_len + 1) {
        return NULL;
    }
    strcpy(out_buf, root_path);
    *out_package = g_package;
    return out_buf;
}

/* Takes a string like "SYSTEM:lib" and turns it into a string
 * like "/system/lib".  The translated path is put in out_buf,
 * and out_buf is returned if the translation succeeded.
 */
const char *
translate_root_path(const char *root_path, char *out_buf, size_t out_buf_len)
{
    if (out_buf_len < 1) {
        return NULL;
    }

    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL || info->mount_point == NULL) {
        return NULL;
    }

    /* Find the relative part of the non-root part of the path.
     */
    root_path += strlen(info->name);  // strip off the "root:"
    while (*root_path != '\0' && *root_path == '/') {
        root_path++;
    }

    size_t mp_len = strlen(info->mount_point);
    size_t rp_len = strlen(root_path);
    if (mp_len + 1 + rp_len + 1 > out_buf_len) {
        return NULL;
    }

    /* Glue the mount point to the relative part of the path.
     */
    memcpy(out_buf, info->mount_point, mp_len);
    if (out_buf[mp_len - 1] != '/') out_buf[mp_len++] = '/';

    memcpy(out_buf + mp_len, root_path, rp_len);
    out_buf[mp_len + rp_len] = '\0';

    return out_buf;
}

static int
internal_root_mounted(const RootInfo *info)
{
    if (info->mount_point == NULL) {
        return -1;
    }
//xxx if TMP: (or similar) just say "yes"

    /* See if this root is already mounted.
     */
    int ret = scan_mounted_volumes();
    if (ret < 0) {
        return ret;
    }
    const MountedVolume *volume;
    volume = find_mounted_volume_by_mount_point(info->mount_point);
    if (volume != NULL) {
        /* It's already mounted.
         */
        return 0;
    }
    return -1;
}

int
is_root_path_mounted(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL) {
        return -1;
    }
    return internal_root_mounted(info) >= 0;
}

static int mount_internal(const char* device, const char* mount_point, const char* filesystem, const char* filesystem_options)
{
    if (filesystem==g_auto) {
      char keyname[KEY_MAX_LENGTH];
      char loopname[10];
      char mtname[10];
      if (strcmp(device,SYSTEM_BLOCK_NAME)==0) {
        strcpy(loopname,"loop4");
        strcpy(mtname,"system");
      } else if (strcmp(device,CACHE_BLOCK_NAME)==0) {
        strcpy(loopname,"loop1");
        strcpy(mtname,"cache");
      } else if (strcmp(device,DBDATA_BLOCK_NAME)==0) {
        strcpy(loopname,"loop3");
        strcpy(mtname,"dbdata");
      } else if (strcmp(device,DATA_BLOCK_NAME)==0) {
        strcpy(loopname,"loop2");
        strcpy(mtname,"data");
      } else {
        strcpy(loopname,"loop5");
        strcpy(mtname,"xxx");
      }
      sprintf(keyname,"fs.%s.type",mtname);
      int fstype = mount_from_config_or_autodetect(keyname,device,loopname,mtname,NULL);
      if (fstype&TYPE_FSTYPE_MASK) {
        return 0;
      } else {
        LOGE("Couldn't mount autodetected filesystem. Parameters: %s; %s; %s\n",keyname,loopname,mtname);
        return -1;
      }
    } else
    if (strcmp(filesystem, "auto") != 0 && filesystem_options == NULL) {
        return mount(device, mount_point, filesystem, MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
    }
    else {
        char mount_cmd[PATH_MAX];
        const char* options = filesystem_options == NULL ? "noatime,nodiratime,nodev" : filesystem_options;
        sprintf(mount_cmd, "mount -t %s -o %s %s %s", filesystem, options, device, mount_point);
        return __system(mount_cmd);
    }
}

int
ensure_root_path_mounted(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL) {
        LOGE("No info found");
        return -1;
    }

    int ret = internal_root_mounted(info);
    if (ret >= 0) {
        /* It's already mounted.
         */
        return 0;
    }

    /* It's not mounted.
     */
    if (info->device == g_mtd_device) {
        if (info->partition_name == NULL) {
            LOGE("Partition name was NULL");
            return -1;
        }
//TODO: make the mtd stuff scan once when it needs to
        mtd_scan_partitions();
        const MtdPartition *partition;
        partition = mtd_find_partition_by_name(info->partition_name);
        if (partition == NULL) {
            LOGE("Partition was NULL");
            return -1;
        }
        return mtd_mount_partition(partition, info->mount_point,
                info->filesystem, 0);
    }

    if (info->device == NULL || info->mount_point == NULL ||
        info->filesystem == NULL ||
        info->filesystem == g_raw ||
        info->filesystem == g_package_file) {
        LOGE("INFO is WRONG");
        return -1;
    }

    mkdir(info->mount_point, 0755);  // in case it doesn't already exist
    if (mount_internal(info->device, info->mount_point, info->filesystem, info->filesystem_options)) {
        if (info->device2 == NULL) {
            LOGE("Can't mount %s to %s with parameters %s %s\n(%s)\n", info->device, info->mount_point, info->filesystem, info->filesystem_options, strerror(errno));
            return -1;
        } else if (mount(info->device2, info->mount_point, info->filesystem,
                MS_NOATIME | MS_NODEV | MS_NODIRATIME, "")) {
            LOGE("Can't mount %s (or %s)\n(%s)\n",
                    info->device, info->device2, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int
ensure_root_path_unmounted(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL) {
        return -1;
    }
    if (info->mount_point == NULL) {
        /* This root can't be mounted, so by definition it isn't.
         */
        return 0;
    }
//xxx if TMP: (or similar) just return error

    /* See if this root is already mounted.
     */
    int ret = scan_mounted_volumes();
    if (ret < 0) {
        return ret;
    }
    const MountedVolume *volume;
    volume = find_mounted_volume_by_mount_point(info->mount_point);
    if (volume == NULL) {
        /* It's not mounted.
         */
        return 0;
    }

    ret = unmount_filesystem(info->mount_point);
    if (ret) {
      return unmount_mounted_volume(volume);
    } else {
      return 0;
    }
}

const MtdPartition *
get_root_mtd_partition(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL || info->device != g_mtd_device ||
            info->partition_name == NULL)
    {
#ifdef BOARD_HAS_MTD_CACHE
        if (strcmp(root_path, "CACHE:") != 0)
            return NULL;
#else
        return NULL;
#endif
    }
    mtd_scan_partitions();
    return mtd_find_partition_by_name(info->partition_name);
}

int
format_root_device(const char *root)
{
    /* Be a little safer here; require that "root" is just
     * a device with no relative path after it.
     */
    const char *c = root;
    while (*c != '\0' && *c != ':') {
        c++;
    }
    /*
    if (c[0] != ':' || c[1] != '\0') {
        LOGW("format_root_device: bad root name \"%s\"\n", root);
        return -1;
    }
    */
   
    const RootInfo *info = get_root_info_for_path(root);
    if (info == NULL || info->device == NULL) {
        LOGW("format_root_device: can't resolve \"%s\"\n", root);
        return -1;
    }
    if (info->mount_point != NULL && info->device == g_mtd_device) {
        /* Don't try to format a mounted device.
         */
        int ret = ensure_root_path_unmounted(root);
        if (ret < 0) {
            LOGW("format_root_device: can't unmount \"%s\"\n", root);
            return ret;
        }
    }

    /* Format the device.
     */
    if (info->device == g_mtd_device) {
        mtd_scan_partitions();
        const MtdPartition *partition;
        partition = mtd_find_partition_by_name(info->partition_name);
        if (partition == NULL) {
            LOGW("format_root_device: can't find mtd partition \"%s\"\n",
                    info->partition_name);
            return -1;
        }
        if (info->filesystem == g_raw || !strcmp(info->filesystem, "yaffs2")) {
            MtdWriteContext *write = mtd_write_partition(partition);
            if (write == NULL) {
                LOGW("format_root_device: can't open \"%s\"\n", root);
                return -1;
            } else if (mtd_erase_blocks(write, -1) == (off_t) -1) {
                LOGW("format_root_device: can't erase \"%s\"\n", root);
                mtd_write_close(write);
                return -1;
            } else if (mtd_write_close(write)) {
                LOGW("format_root_device: can't close \"%s\"\n", root);
                return -1;
            } else {
                return 0;
            }
        }
    }

    //Handle MMC device types
    if(info->device == g_mmc_device) {
        mmc_scan_partitions();
        const MmcPartition *partition;
        partition = mmc_find_partition_by_name(info->partition_name);
        if (partition == NULL) {
            LOGE("format_root_device: can't find mmc partition \"%s\"\n",
                    info->partition_name);
            return -1;
        }
        if (!strcmp(info->filesystem, "ext3")) {
            if(mmc_format_ext3(partition))
                LOGE("\n\"%s\" wipe failed!\n", info->partition_name);
        }
    }

    return format_non_mtd_device(root);
}

//////////////////////////////
// Steam additions

#ifdef STEAM_HAS_CRYPTSETUP
void cryptsetup_popen(const char* command)
{
  char* argv[] = {"cryptsetup","luksOpen","-q",NULL,NULL,NULL};
  char secname[32];
  sprintf(secname,"sec%s",strrchr(command,'/')+1);
  argv[3] = command;
  argv[4] = secname;
  int rv = steam_cryptsetup_main(5,argv);
  _exit(rv);
}

void cryptformat_popen(const char* command)
{
  char* argv[] = {"cryptsetup","luksFormat","-q","-c","aes-plain",NULL,NULL};
  argv[5] = command;
  int rv = steam_cryptsetup_main(6,argv);
  _exit(rv);
}
#endif

int is_encrypted_partition(const char* partition)
{
  int iscrypt = 0;
#ifdef STEAM_HAS_CRYPTSETUP
  if (call_cryptsetup("cryptsetup","isLuks",partition,NULL)==0) iscrypt=TYPE_CRYPT;
#else
  // we might not yet have cryptsetup to load, so we'll check the magic number
  int fd = open(partition,O_RDONLY);
  if (fd>=0) {
    char magic[7];
    read(fd,partition,6);
    magic[6]='\0';
    if (strcmp(magic,"LUKS\xBA\xBE")==0) iscrypt=TYPE_CRYPT;
    close(fd);
  }
#endif
  return iscrypt;
}

int open_encrypted_partition(const char* partition, char* secret)
{
  if (is_encrypted_partition(partition)) {
    if (secret && strlen(secret)>0) {
      // first try with the supplied password
      int sin = -1;
      int sout = -1;
      pid_t pid;
#ifdef STEAM_HAS_CRYPTSETUP
      pid = popen3func(&sin,&sout,NULL,POPEN_JOINSTDERR,partition,cryptsetup_popen);
#else
      char command[255];
      sprintf(command,"/sbin/cryptsetup luksOpen -q %s sec%s",partition,strrchr(partition,'/')+1);
      pid = popen3(&sin,&sout,NULL,POPEN_JOINSTDERR,command);
#endif
      write(sin,secret,strlen(secret));
      write(sin,"\n",1);
      close(sin);
      char d;while(read(sout,&d,1)>0) printf("%c",d);
      int r = pclose3(pid,&sin,&sout,NULL,0);
      if (r==0) return 1;
    }
    struct stat s;
    char path[PATH_MAX];
    sprintf(path,"/dev/mapper/sec%s",strrchr(partition,'/')+1);
    if (stat(path,&s)==0) {
      // already opened
      return 1;
    }
    int was_initialized = get_ui_state();
    if (!was_initialized) ui_init();
    set_console_cmd("");
    ui_clear_num_screen(0);
    ui_set_secret_screen(1);
    ui_set_show_text(1);
    ui_print(SECRET_ENTER_FOR,partition);
    ui_print(SECRET_HOWTO);
    ui_clear_key_queue();
    do {
      int key = ui_wait_key();
      int action = device_handle_key(key, 1);
      if (action == SELECT_ITEM) {
        int sin = -1;
        int sout = -1;
        pid_t pid;
#ifdef STEAM_HAS_CRYPTSETUP
        pid = popen3func(&sin,&sout,NULL,POPEN_JOINSTDERR,partition,cryptsetup_popen);
#else
        char command[255];
        sprintf(command,"/sbin/cryptsetup luksOpen -q %s sec%s",partition,strrchr(partition,'/')+1);
        pid = popen3(&sin,&sout,NULL,POPEN_JOINSTDERR,command);
#endif
        write(sin,get_console_cmd(),strlen(get_console_cmd()));
        write(sin,"\n",1);
        close(sin);
        char d;while(read(sout,&d,1)>0) printf("%c",d);
        int r = pclose3(pid,&sin,&sout,NULL,0);
        if (r==0) {
          if (secret) {
            strcpy(secret,get_console_cmd());
          }
          break;
        } else {
          ui_print(SECRET_FAIL);
          set_console_cmd("");
        }
      }
      if (action == GO_BACK) {
        ui_set_show_text(0);
        ui_set_secret_screen(0);
        if (!was_initialized) ui_done();
        return 0;
      }
    } while (true);
    ui_set_show_text(0);
    ui_set_secret_screen(0);
    if (!was_initialized) ui_done();
    return 1;
  }
  return 0;
}

int filesystem_check(const char* partition) {
  int iscrypt = is_encrypted_partition(partition);
  struct stat s;
  int fstype = 0;
  char frommount[PATH_MAX];
  printf(INIT_AUTODETECT,partition);

  if (iscrypt) {
    if (!open_encrypted_partition(partition,NULL)) {
      // can't autodetect much if the partition is encrypted and we don't know the password
      return TYPE_CRYPT;
    }
    sprintf(frommount,"/dev/mapper/sec%s",strrchr(partition,'/')+1);
  } else {
    strcpy(frommount,partition);
  }
  char* loopmount = "/res/.tmp";
  char path[PATH_MAX];
  call_busybox("mkdir","/res/.tmp",NULL);
  call_busybox("chmod","700","/res/.tmp",NULL);

  if (call_busybox("mount","-t","ext2","-o",TYPE_EXT2_DEFAULT_MOUNT,frommount,loopmount,NULL)==0) {
    // ext3/4 won't mount as ext2, so we can differentiate them by trying ext2 first
    fstype = TYPE_EXT2;
  } else if (call_busybox("mount","-t","ext4","-o",TYPE_EXT4_DEFAULT_MOUNT,frommount,loopmount,NULL)==0) {
    fstype = TYPE_EXT4;
  } else if (call_busybox("mount","-t","rfs","-o",TYPE_RFS_DEFAULT_MOUNT,frommount,loopmount,NULL)==0) {
    fstype = TYPE_RFS;
    char value[VALUE_MAX_LENGTH];
    if ((get_conf_ro("fs.system.rfs",value) && strcmp(value,"bad")==0) ||
        (get_conf_ro_from("/res/.tmp/etc/steam.conf","fs.system.rfs",value) && strcmp(value,"bad")==0)) {
      fstype = TYPE_RFS|TYPE_RFS_BAD;
    }
    // if we found these we will think it's a bad rfs.
    sprintf(path,"%s/BIN",loopmount);      if (stat(path,&s)==0) { fstype = TYPE_RFS|TYPE_RFS_BAD; }
    sprintf(path,"%s/APP",loopmount);      if (stat(path,&s)==0) { fstype = TYPE_RFS|TYPE_RFS_BAD; }
    sprintf(path,"%s/DATA",loopmount);     if (stat(path,&s)==0) { fstype = TYPE_RFS|TYPE_RFS_BAD; }
    sprintf(path,"%s/SYSTEM",loopmount);   if (stat(path,&s)==0) { fstype = TYPE_RFS|TYPE_RFS_BAD; }
    sprintf(path,"%s/RECOVERY",loopmount); if (stat(path,&s)==0) { fstype = TYPE_RFS|TYPE_RFS_BAD; }
  } else {
    // jfs won't mount if dirty without being checked first
    call_fsck_jfs("fsck.jfs","-p",frommount,NULL);
    if (call_busybox("mount","-t","jfs","-o",TYPE_JFS_DEFAULT_MOUNT,frommount,loopmount,NULL)==0) {
      fstype = TYPE_JFS;
    }
  }

  int isloop = 0;
  int isbind = 0;

  if (fstype) {
    sprintf(path,"%s/.extfs",loopmount);
    if (stat(path,&s)==0) isloop = TYPE_LOOP;
    sprintf(path,"%s/.data",loopmount);
    if (stat(path,&s)==0) isbind = TYPE_BIND;
    sync();
    call_busybox("umount","-f",frommount,NULL);
    call_busybox("umount","-f",loopmount,NULL);
  }
  call_busybox("rmdir","/res/.tmp",NULL);

  if (fstype&TYPE_CRYPT) { char secname[32];sprintf(secname,"sec%s",strrchr(partition,'/')+1);call_cryptsetup("cryptsetup","luksClose",secname,NULL); }

  printf(PARTITION_INFORMATION,partition,fstype|iscrypt|isloop|isbind);
  return fstype|iscrypt|isloop|isbind;
}

int check_and_mount(int fstype, const char* partition, const char* loopname, const char* mtname, char* secret) {
  printf(INIT_MOUNTING,fstype,partition);
  if (fstype&TYPE_FSTYPE_MASK) {
    char mtnamec[PATH_MAX];
    char* p;
    strcpy(mtnamec,mtname);
    while ((p = strchr(mtnamec,'/'))) *p = '_';

    char frompath[PATH_MAX];
    char topath[PATH_MAX];
    if (fstype&TYPE_CRYPT) {
      if (!open_encrypted_partition(partition,secret)) {
        return 1;
      }
      sprintf(frompath,"/dev/mapper/sec%s",strrchr(partition,'/')+1);
    } else {
      sprintf(frompath,"%s",partition);
    }
    if (fstype&TYPE_LOOP) {
      sprintf(topath,"/res/.orig_%s",mtnamec);
      call_busybox("mkdir",topath,NULL);
      call_busybox("chmod","700",topath,NULL);
    } else {
      sprintf(topath,"/%s",mtname);
    }
    // mount base fs
    if (fstype&TYPE_RFS) {
      // no fsck. duh
      if (call_busybox("mount","-t","rfs","-o",TYPE_RFS_DEFAULT_MOUNT,frompath,topath,NULL)) {
        if (fstype&TYPE_CRYPT) { char secname[32];sprintf(secname,"sec%s",strrchr(partition,'/')+1);call_cryptsetup("cryptsetup","luksClose",secname,NULL); }
        return 2;
      }
    } else if (fstype&TYPE_EXT2) {
      call_e2fsck("fsck.ext2","-p",frompath,NULL);
      if (call_busybox("mount","-t","ext2","-o",TYPE_EXT2_DEFAULT_MOUNT,frompath,topath,NULL)) {
        if (fstype&TYPE_CRYPT) { char secname[32];sprintf(secname,"sec%s",strrchr(partition,'/')+1);call_cryptsetup("cryptsetup","luksClose",secname,NULL); }
        return 3;
      }
    } else if (fstype&TYPE_EXT4) {
      call_e2fsck("fsck.ext4","-p",frompath,NULL);
      if (call_busybox("mount","-t","ext4","-o",TYPE_EXT4_DEFAULT_MOUNT,frompath,topath,NULL)) {
        if (fstype&TYPE_CRYPT) { char secname[32];sprintf(secname,"sec%s",strrchr(partition,'/')+1);call_cryptsetup("cryptsetup","luksClose",secname,NULL); }
        return 4;
      }
    } else if (fstype&TYPE_JFS) {
      call_fsck_jfs("fsck.jfs","-p",frompath,NULL);
      if (call_busybox("mount","-t","jfs","-o",TYPE_JFS_DEFAULT_MOUNT,frompath,topath,NULL)) {
        if (fstype&TYPE_CRYPT) { char secname[32];sprintf(secname,"sec%s",strrchr(partition,'/')+1);call_cryptsetup("cryptsetup","luksClose",secname,NULL); }
        return 5;
      }
    }
    // mount loop
    if (fstype&TYPE_LOOP) {
      sprintf(frompath,"/res/.orig_%s/.extfs",mtnamec);
      sprintf(topath,"/dev/block/%s",loopname);
      if (call_busybox("losetup",topath,frompath,NULL)) {
        call_busybox("umount","-f",topath,NULL);
        if (fstype&TYPE_CRYPT) { char secname[32];sprintf(secname,"sec%s",strrchr(partition,'/')+1);call_cryptsetup("cryptsetup","luksClose",secname,NULL); }
        return 6;
      }
      sprintf(frompath,"/%s",mtname);
      call_e2fsck("fsck.ext2","-p",topath,NULL);
      if (call_busybox("mount","-t","ext2","-o",TYPE_EXT2_DEFAULT_MOUNT,topath,frompath,NULL)) {
        call_busybox("losetup","-d",topath,NULL);
        sprintf(topath,"/res/.orig_%s",mtnamec);
        call_busybox("umount","-f",topath,NULL);
        if (fstype&TYPE_CRYPT) { char secname[32];sprintf(secname,"sec%s",strrchr(partition,'/')+1);call_cryptsetup("cryptsetup","luksClose",secname,NULL); }
        return 7;
      }
    }
    // mount bind
    // TODO
  } else {
    return -1;
  }
  return 0;
}

int filesystem_format(int fstype, const char* partition, const char* loopname, const char* mtname, char* secret)
{
  char fromname[PATH_MAX];
  strcpy(fromname,partition);
  char ss[256];
  ss[0]='\0';
  printf(INIT_CREATE,fstype,partition);
  if (fstype&TYPE_CRYPT) {
    if (!secret || strlen(secret)==0) {
      int was_initialized = get_ui_state();
      if (!was_initialized) ui_init();
      set_console_cmd("");
      ui_clear_num_screen(0);
      ui_set_secret_screen(1);
      ui_set_show_text(1);
      ui_print(SECRET_ENTER_PASS);
      ui_print(SECRET_HOWTO);
      ui_clear_key_queue();
      int type = 0;
      do {
        int key = ui_wait_key();
        int action = device_handle_key(key, 1);
        if (action == SELECT_ITEM && strlen(get_console_cmd())>0) {
          if (type==0) {
            strcpy(ss,get_console_cmd());
            type = 1;
            ui_clear_key_queue();
            set_console_cmd("");
            ui_print(SECRET_PASSWORD_AGAIN);
          } else {
            if (strcmp(ss,get_console_cmd())==0) {
              break;
            } else {
              ui_print(SECRET_PASSWORD_MISMATCH);
              type = 0;
              ui_clear_key_queue();
              set_console_cmd("");
            }
          }
        }
        if (action == GO_BACK) {
          type = 0;
          set_console_cmd("");
          ui_clear_key_queue();
        }
      } while (true);
      ui_set_secret_screen(0);
      ui_set_show_text(0);
      if (!was_initialized) ui_done();
      if (secret) {
        strcpy(secret,ss);
      }
    } else {
      strcpy(ss,secret);
    }

    int sin = -1;
    int sout = -1;
    pid_t pid;
#ifdef STEAM_HAS_CRYPTSETUP
    pid = popen3func(&sin,&sout,NULL,POPEN_JOINSTDERR,partition,cryptformat_popen);
#else
    char command[255];
    sprintf(command,"/sbin/cryptsetup luksFormat -q -c aes-plain %s",partition);
    pid = popen3(&sin,&sout,NULL,POPEN_JOINSTDERR,command);
#endif
    write(sin,ss,strlen(ss));
    write(sin,"\n",1);
    close(sin);
    char d;while(read(sout,&d,1)>0) printf("%c",d);
    int r = pclose3(pid,&sin,&sout,NULL,0);
    if (!open_encrypted_partition(partition,ss)) {
      printf(CONVERT_CRYPT_FAILED,r);
      return 0;
    }
    sprintf(fromname,"/dev/mapper/sec%s",strrchr(partition,'/')+1);
  }

  if (fstype&TYPE_RFS) {
    char header[255];
    if (strcmp(partition,DATA_BLOCK_NAME)==0) {
      sprintf(header,"/sbin/fat.format -v -F 32 -S 4096 -s 4 %s",fromname);
    } else if (strcmp(partition,DBDATA_BLOCK_NAME)==0) {
      sprintf(header,"/sbin/fat.format -v -F 16 -S 4096 -s 1 %s",fromname);
    } else if (strcmp(partition,CACHE_BLOCK_NAME)==0) {
      sprintf(header,"/sbin/fat.format -v -F 16 -S 4096 -s 1 %s",fromname);
    } else if (strcmp(partition,SYSTEM_BLOCK_NAME)==0) {
      sprintf(header,"/sbin/fat.format -v -F 32 -S 4096 -s 1 %s",fromname);
    } else {
      sprintf(header,"/sbin/fat.format -v %s",fromname);
    }
    sh(header);
  } else if (fstype&TYPE_EXT2) {
    if (strcmp(partition,DATA_BLOCK_NAME)==0) {
      call_mke2fs("mkfs.ext2","-L",mtname,"-b","4096","-m","0","-F",fromname,NULL);
    } else {
      call_mke2fs("mkfs.ext2","-L",mtname,"-b","1024","-m","0","-F",fromname,NULL);
    }
  } else if (fstype&TYPE_EXT4) {
    if (strcmp(partition,DATA_BLOCK_NAME)==0) {
      call_mke2fs("mkfs.ext4","-L",mtname,"-b","4096","-m","0","-F",fromname,NULL);
    } else {
      call_mke2fs("mkfs.ext4","-L",mtname,"-b","1024","-J","size=4","-m","0","-F",fromname,NULL);
    }
  } else if (fstype&TYPE_JFS) {
    call_mkfs_jfs("mkfs.jfs","-q","-L",mtname,fromname,NULL);
  }

  if (fstype&TYPE_LOOP) {
    // for this we have to mount it first...
    call_busybox("mkdir","/res/.tmp",NULL);
    call_busybox("chmod","700","/res/.tmp",NULL);
    if (check_and_mount(fstype&TYPE_FSTYPE_MASK,fromname,"loop0","res/.tmp",ss)==0) {
      int loopsize = 0;
      if (strcmp(partition,DATA_BLOCK_NAME)==0) {
        loopsize = BLOCK_DATA_LOOP_SIZE;
      } else if (strcmp(partition,DBDATA_BLOCK_NAME)==0) {
        loopsize = BLOCK_DBDATA_LOOP_SIZE;
      } else if (strcmp(partition,CACHE_BLOCK_NAME)==0) {
        loopsize = BLOCK_CACHE_LOOP_SIZE;
      }
      if (loopsize) {
        call_busybox("touch","/res/.tmp/.extfs",NULL);
        truncate("/res/.tmp/.extfs",loopsize);
        call_busybox("losetup","/dev/block/loop0","/res/.tmp/.extfs",NULL);
        call_mke2fs("mkfs.ext2","-L",mtname,"-b","4096","-m","0","-F","/dev/block/loop0",NULL);
        call_busybox("losetup","-d","/dev/block/loop0",NULL);
      }
      call_busybox("umount","-f","/res/.tmp/.extfs",NULL);
      call_busybox("umount","-f","/res/.tmp",NULL);
    }
    call_busybox("rm","-rf","/res/.tmp",NULL);
  }
  if (fstype&TYPE_CRYPT) { char secname[32];sprintf(secname,"sec%s",strrchr(partition,'/')+1);call_cryptsetup("cryptsetup","luksClose",secname,NULL); }
  memset(ss,0,sizeof(ss));
  return fstype;
}

int filesystem_create(const char* partition, const char* label) {
  int was_initialized = get_ui_state();
  if (!was_initialized) ui_init();
  char header[255];
  char value[VALUE_MAX_LENGTH];
  sprintf(header,CREATE_PARTITION_HEADER,partition);
  char* headers[] = { header, NULL };
  char* items[] = { CREATE_PARTITION_RFS, CREATE_PARTITION_EXT2, CREATE_PARTITION_EXT4, CREATE_PARTITION_JFS, CREATE_PARTITION_NONE, NULL, NULL };
  if (strcmp(get_conf_def("preinit.allowfm",value,"0"),"1")==0) items[5] = CREATE_PARTITION_FM;
  int chosen_item = -1;
  while (chosen_item<0) {
    chosen_item = get_menu_selection(headers,items,0);
  }
  int fstype = 0;
  if (chosen_item==0) {
    fstype = filesystem_format(TYPE_RFS,partition,NULL,label,NULL);
  } else if (chosen_item==1) {
    fstype = filesystem_format(TYPE_EXT2,partition,NULL,label,NULL);
  } else if (chosen_item==2) {
    fstype = filesystem_format(TYPE_EXT4,partition,NULL,label,NULL);
  } else if (chosen_item==3) {
    fstype = filesystem_format(TYPE_JFS,partition,NULL,label,NULL);
  } else if (chosen_item==5) {
    file_manager();
    if (!was_initialized) ui_done();
    return filesystem_create(partition,label);
  }
  if (!was_initialized) ui_done();
  return fstype;
}

int mount_from_config_or_autodetect(const char* keyname, const char* partition, const char* loopname, const char* mtname, char* secret)
{
  int fstype = 0;
  char value[VALUE_MAX_LENGTH];
  get_conf_def(keyname,value,"0");
  if (sscanf(value,"%d",&fstype)!=1) fstype = 0;
  if (fstype&TYPE_FSTYPE_MASK) {
    int res = check_and_mount(fstype,partition,loopname,mtname,secret);
    if (res==0) return fstype;
  }
  fstype = filesystem_check(partition);
  if (!(fstype&TYPE_FSTYPE_MASK)) fstype = filesystem_create(partition,mtname);
  if (fstype&TYPE_FSTYPE_MASK) {
    sprintf(value,"%d",fstype);
    set_conf(keyname,value);
    int res = check_and_mount(fstype,partition,loopname,mtname,secret);
    if (res==0) return fstype;
  }
  return 0;
}

int unmount_filesystem(const char* partition)
{
  // TODO: this should be way more intelligent
  char loopname[40];
  char blockname[40];
  int found = 0;
  if (strcmp(partition,"/system")==0) {
    strcpy(loopname,"/dev/block/loop4");
    strcpy(blockname,SYSTEM_BLOCK_NAME);
    found = 1;
  } else if (strcmp(partition,"/tmp/sys")==0) {
    strcpy(loopname,"/dev/block/loop4");
    strcpy(blockname,SYSTEM_BLOCK_NAME);
    found = 1;
  } else if (strcmp(partition,"/cache")==0) {
    strcpy(loopname,"/dev/block/loop1");
    strcpy(blockname,CACHE_BLOCK_NAME);
    found = 1;
  } else if (strcmp(partition,"/data")==0) {
    strcpy(loopname,"/dev/block/loop2");
    strcpy(blockname,DATA_BLOCK_NAME);
    found = 1;
  } else if (strcmp(partition,"/dbdata")==0) {
    strcpy(loopname,"/dev/block/loop3");
    strcpy(blockname,DBDATA_BLOCK_NAME);
    found = 1;
  }
  if (found) {
    char cryptname[PATH_MAX];
    call_busybox("umount","-f",loopname,NULL); // unmount loop device
    call_busybox("losetup","-d",loopname,NULL); // disconnect loop file
    call_busybox("umount","-f",blockname,NULL); // unmount original device
    sprintf(cryptname,"/dev/mapper/sec%s",strrchr(blockname,'/')+1);
    call_busybox("umount","-f",cryptname,NULL); // unmount crypt device
    sprintf(cryptname,"sec%s",strrchr(blockname,'/')+1);
    call_cryptsetup("cryptsetup","luksClose",cryptname,NULL); // close crypt device
    return 0;
  } else {
    return -1;
  }
}
