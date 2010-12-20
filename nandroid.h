#ifndef NANDROID_H
#define NANDROID_H

#define BACKUP_SYSTEM 1
#define BACKUP_CACHE 2
#define BACKUP_DATA 4
#define BACKUP_DATADATA 8
#define BACKUP_EFS 16
#define BACKUP_SDEXT 32
#define BACKUP_BOOTABLES 64
#define BACKUP_OTHERS 128
#define BACKUP_ALL 255

#define BACKUP_NOUMOUNT 512
#define BACKUP_NOFORMAT 1024
#define BACKUP_NOMD5 2048

int nandroid_backup_flags(const char* backup_path, int flags);
int nandroid_main(int argc, char** argv);
int nandroid_backup(const char* backup_path);
int nandroid_restore_flags(const char* backup_path, int flags);
int nandroid_restore(const char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache, int restore_sdext);
void nandroid_generate_timestamp_path(char* backup_path);

#endif
