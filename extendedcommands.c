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

#include "../steam_main/steam.h"

#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "ui.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "device.h"
#include "locale.h"
#include "config.h"

#include "commands.h"
#include "amend/amend.h"

#include "mtdutils/mtdutils.h"
#include "mtdutils/dump_image.h"
#include "../yaffs2/yaffs2/utils/mkyaffs2image.h"
#include "../yaffs2/yaffs2/utils/unyaffs.h"

#include "extendedcommands.h"
#include "nandroid.h"

int signature_check_enabled = 1;
int script_assert_enabled = 1;
static const char *SDCARD_PACKAGE_FILE = "SDCARD:update.zip";
#ifdef HAS_TWO_SDCARDS
static const char *SDCARD2_PACKAGE_FILE = "SDCARD2:update.zip";
#endif

void
toggle_signature_check()
{
    signature_check_enabled = !signature_check_enabled;
    ui_print("Signature Check: %s\n", signature_check_enabled ? "Enabled" : "Disabled");
}

void toggle_script_asserts()
{
    script_assert_enabled = !script_assert_enabled;
    ui_print("Script Asserts: %s\n", script_assert_enabled ? "Enabled" : "Disabled");
}

int install_zip(const char* packagefilepath)
{
    ui_print("\n-- Installing: %s\n", packagefilepath);
#ifndef BOARD_HAS_NO_MISC_PARTITION
    set_sdcard_update_bootloader_message();
#endif
    int status = install_package(packagefilepath);
    ui_reset_progress();
    if (status != INSTALL_SUCCESS) {
        ui_set_background(BACKGROUND_ICON_ERROR);
        ui_print("Installation aborted.\n");
        return 1;
    } 
#ifndef BOARD_HAS_NO_MISC_PARTITION
    if (firmware_update_pending()) {
        ui_print("\nReboot via menu to complete\ninstallation.\n");
    }
#endif
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_print("\nInstall from sdcard complete.\n");
    return 0;
}

char* INSTALL_MENU_ITEMS[] = {  UPDATE_APPLY_SDCARD,
                                UPDATE_CHOOSE_ZIP, 
                                UPDATE_SIG_CHECK,
                                UPDATE_ASSERTS,
#ifdef HAS_TWO_SDCARDS 
                                UPDATE_APPLY_SDCARD2,
                                UPDATE_CHOOSE_ZIP2,
#endif
                                NULL };
#define ITEM_APPLY_SDCARD1    0
#define ITEM_CHOOSE_ZIP       1
#define ITEM_SIG_CHECK        2
#define ITEM_ASSERTS          3
#define ITEM_APPLY_SDCARD2    4 
#define ITEM_CHOOSE_ZIP2      5 

void show_install_update_menu()
{
    static char* headers[] = {  "Apply update from .zip file on SD card",
                                "",
                                NULL 
    };
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, INSTALL_MENU_ITEMS, 0);
        switch (chosen_item)
        {
            case ITEM_ASSERTS:
                toggle_script_asserts();
                break;
            case ITEM_SIG_CHECK:
                toggle_signature_check();
                break;
            case ITEM_APPLY_SDCARD1:
            {
                if (confirm_selection("Confirm install?", "Yes - Install /mnt/sdcard/update.zip"))
                    install_zip(SDCARD_PACKAGE_FILE);
                break;
            }
#ifdef HAS_TWO_SDCARDS
            case ITEM_APPLY_SDCARD2:
            {
                if (confirm_selection("Confirm install?", "Yes - Install /mnt/external_sd/update.zip"))
                    install_zip(SDCARD2_PACKAGE_FILE);
                break;
            }
#endif
            case ITEM_CHOOSE_ZIP:
                show_choose_zip_menu(0);
                break;
#ifdef HAS_TWO_SDCARDS
            case ITEM_CHOOSE_ZIP2:
                show_choose_zip_menu(1);
                break;
#endif
            default:
                return;
        }
        
    }
}

void free_string_array(char** array)
{
    if (array == NULL)
        return;
    char* cursor = array[0];
    int i = 0;
    while (cursor != NULL)
    {
        free(cursor);
        cursor = array[++i];
    }
    free(array);
}

char** gather_files(const char* directory, const char* fileExtensionOrDirectory, int* numFiles)
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int total = 0;
    int i;
    char** files = NULL;
    int pass;
    *numFiles = 0;
    int dirLen = strlen(directory);

    dir = opendir(directory);
    if (dir == NULL) {
        ui_print("Couldn't open directory.\n");
        return NULL;
    }
  
    int extension_length = 0;
    if (fileExtensionOrDirectory != NULL)
        extension_length = strlen(fileExtensionOrDirectory);
  
    int isCounting = 1;
    i = 0;
    for (pass = 0; pass < 2; pass++) {
        while ((de=readdir(dir)) != NULL) {
            // skip hidden files
            if (de->d_name[0] == '.')
                continue;
            
            // NULL means that we are gathering directories, so skip this
            if (fileExtensionOrDirectory != NULL)
            {
                // make sure that we can have the desired extension (prevent seg fault)
                if (strlen(de->d_name) < extension_length)
                    continue;
                // compare the extension
                if (strcmp(de->d_name + strlen(de->d_name) - extension_length, fileExtensionOrDirectory) != 0)
                    continue;
            }
            else
            {
                struct stat info;
                char fullFileName[PATH_MAX];
                strcpy(fullFileName, directory);
                strcat(fullFileName, de->d_name);
                stat(fullFileName, &info);
                // make sure it is a directory
                if (!(S_ISDIR(info.st_mode)))
                    continue;
            }
            
            if (pass == 0)
            {
                total++;
                continue;
            }
            
            files[i] = (char*) malloc(dirLen + strlen(de->d_name) + 2);
            strcpy(files[i], directory);
            strcat(files[i], de->d_name);
            if (fileExtensionOrDirectory == NULL)
                strcat(files[i], "/");
            i++;
        }
        if (pass == 1)
            break;
        if (total == 0)
            break;
        rewinddir(dir);
        *numFiles = total;
        files = (char**) malloc((total+1)*sizeof(char*));
        files[total]=NULL;
    }

    if(closedir(dir) < 0) {
        LOGE("Failed to close directory.");
    }

    if (total==0) {
        return NULL;
    }

	// sort the result
	if (files != NULL) {
		for (i = 0; i < total; i++) {
			int curMax = -1;
			int j;
			for (j = 0; j < total - i; j++) {
				if (curMax == -1 || strcmp(files[curMax], files[j]) < 0)
					curMax = j;
			}
			char* temp = files[curMax];
			files[curMax] = files[total - i - 1];
			files[total - i - 1] = temp;
		}
	}

    return files;
}

// pass in NULL for fileExtensionOrDirectory and you will get a directory chooser
char* choose_file_menu(const char* directory, const char* fileExtensionOrDirectory, const char* headers[])
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int numFiles = 0;
    int numDirs = 0;
    int i;
    char* return_value = NULL;
    int dir_len = strlen(directory);

    char** files = gather_files(directory, fileExtensionOrDirectory, &numFiles);
    char** dirs = NULL;
    if (fileExtensionOrDirectory != NULL)
        dirs = gather_files(directory, NULL, &numDirs);
    int total = numDirs + numFiles;
    if (total == 0)
    {
        ui_print("No files found.\n");
    }
    else
    {
        char** list = (char**) malloc((total + 1) * sizeof(char*));
        list[total] = NULL;


        for (i = 0 ; i < numDirs; i++)
        {
            list[i] = strdup(dirs[i] + dir_len);
        }

        for (i = 0 ; i < numFiles; i++)
        {
            list[numDirs + i] = strdup(files[i] + dir_len);
        }

        for (;;)
        {
            int chosen_item = get_menu_selection(headers, list, 0);
            if (chosen_item == GO_BACK)
                break;
            static char ret[PATH_MAX];
            if (chosen_item < numDirs)
            {
                char* subret = choose_file_menu(dirs[chosen_item], fileExtensionOrDirectory, headers);
                if (subret != NULL)
                {
                    strcpy(ret, subret);
                    return_value = ret;
                    break;
                }
                continue;
            } 
            strcpy(ret, files[chosen_item - numDirs]);
            return_value = ret;
            break;
        }
        free_string_array(list);
    }

    free_string_array(files);
    free_string_array(dirs);
    return return_value;
}

void show_choose_zip_menu(int type)
{
  if (type==0) {
    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /mnt/sdcard\n");
        return;
    }
  }
#ifdef HAS_TWO_SDCARDS
  if (type==1) {
    if (ensure_root_path_mounted("SDCARD2:") != 0) {
        LOGE ("Can't mount /mnt/external_sd\n");
        return;
    }
  }
#endif

    static char* headers[] = {  "Choose a zip to apply",
                                "",
                                NULL 
    };
    
    char* file;
    if (type==0) file = choose_file_menu("/mnt/sdcard/", ".zip", headers);
#ifdef HAS_TWO_SDCARDS
    if (type==1) file = choose_file_menu("/mnt/external_sd", ".zip", headers);
#endif
    if (file == NULL)
        return;
    char sdcard_package_file[1024];
    if (type==0) strcpy(sdcard_package_file, "SDCARD:");
#ifdef HAS_TWO_SDCARDS
    if (type==1) strcpy(sdcard_package_file, "SDCARD2:");
#endif
    if (type==0) strcat(sdcard_package_file,  file + strlen("/mnt/sdcard/"));
#ifdef HAS_TWO_SDCARDS
    if (type==1) strcat(sdcard_package_file,  file + strlen("/mnt/external_sd/"));
#endif
    static char* confirm_install  = "Confirm install?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Install %s", basename(file));
    if (confirm_selection(confirm_install, confirm))
        install_zip(sdcard_package_file);
}

void show_nandroid_restore_menu()
{
    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /mnt/sdcard\n");
        return;
    }
    
    static char* headers[] = {  NANDROID_HEADER, NULL };

    char* file = choose_file_menu("/mnt/sdcard/clockworkmod/backup/", NULL, headers);
    if (file == NULL)
        return;

    if (confirm_selection(NANDROID_CONFIRM, NANDROID_YES))
        nandroid_restore_flags(file, BACKUP_ALL&BACKUP_NOFORMAT);
}

void show_mount_usb_storage_menu(char* message)
{
    char command[PATH_MAX];
    sprintf(command, "echo %s > %s/file",SDCARD_LUN_CONTENT, SDCARD_LUN_FILE);
    __system(command);
#ifdef HAS_TWO_SDCARDS
    sprintf(command, "echo %s > %s/file",SDCARD2_LUN_CONTENT, SDCARD2_LUN_FILE);
    __system(command);
#endif
    if (message) {
      ui_print(message);
    }
    static char* headers[] = {  MOUNTS_USBMASS_HEADER, NULL };
    
    static char* list[] = { MOUNTS_USBMASS_EXIT, NULL };
    
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0);
        if (chosen_item == GO_BACK || chosen_item == 0)
            break;
    }
    
    sprintf(command, "echo '' > %s/file", SDCARD_LUN_FILE);
    __system(command);
    sprintf(command, "echo 0 > %s/enable", SDCARD_LUN_FILE);
    __system(command);
#ifdef HAS_TWO_SDCARDS
    sprintf(command, "echo '' > %s/file", SDCARD2_LUN_FILE);
    __system(command);
    sprintf(command, "echo 0 > %s/enable", SDCARD2_LUN_FILE);
    __system(command);
#endif
}

int confirm_selection(const char* title, const char* confirm)
{
    struct stat info;
    if (0 == stat("/mnt/sdcard/clockworkmod/.no_confirm", &info))
        return 1;

    char* confirm_headers[]  = {  title, CONFIRM_NOUNDO, "", NULL };
    char* items[] = { CONFIRM_NO,
                      CONFIRM_NO,
                      CONFIRM_NO,
                      CONFIRM_NO,
                      CONFIRM_NO,
                      CONFIRM_NO,
                      CONFIRM_NO,
                      confirm, //" Yes -- wipe partition",   // [7
                      CONFIRM_NO,
                      CONFIRM_NO,
                      CONFIRM_NO,
                      NULL };

    int chosen_item = get_menu_selection(confirm_headers, items, 0);
    return chosen_item == 7;
}

int format_non_mtd_device(const char* root)
{
    // if this is SDEXT:, don't worry about it.
    if (0 == strcmp(root, "SDEXT:"))
    {
        struct stat st;
        if (0 != stat(SDEXT_DEVICE, &st))
        {
            ui_print("No app2sd partition found. Skipping format of /sd-ext.\n");
            return 0;
        }
    }

    char path[PATH_MAX];
    translate_root_path(root, path, PATH_MAX);
    if (0 != ensure_root_path_mounted(root))
    {
        ui_print("Error mounting %s!\n", path);
        ui_print("Skipping format...\n");
        return 0;
    }

    static char tmp[PATH_MAX];
    call_busybox("rm","-rf",path,NULL);
    // if it could delete the mount point, recreate it
    call_busybox("mkdir",path,NULL);

    ensure_root_path_unmounted(root);
    return 0;
}

void show_partition_menu()
{
    static char* headers[] = {  PARTITION_HEADER, NULL };
    static char* confirm_format  = PARTITION_CONFIRM;
    static char* confirm = PARTITION_YES;
        
    for (;;)
    {
        int ismounted[MOUNTABLE_COUNT];
        int i;
        static string options[MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT + 1 + 1]; // mountables, format mtds, format mmcs, usb storage, null
        for (i = 0; i < MOUNTABLE_COUNT; i++)
        {
            ismounted[i] = is_root_path_mounted(mounts[i][2]);
            options[i] = ismounted[i] ? mounts[i][1] : mounts[i][0];
        }
        
        for (i = 0; i < MTD_COUNT; i++)
        {
            options[MOUNTABLE_COUNT + i] = mtds[i][0];
        }
            
        for (i = 0; i < MMC_COUNT; i++)
        {
            options[MOUNTABLE_COUNT + MTD_COUNT + i] = mmcs[i][0];
        }
    
        options[MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT] = PARTITION_USB_STORAGE;
        options[MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT + 1] = NULL;
        
        int chosen_item = get_menu_selection(headers, options, 0);
        if (chosen_item == GO_BACK)
            break;
        if (chosen_item == MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT)
        {
            show_mount_usb_storage_menu(NULL);
        }
        else if (chosen_item < MOUNTABLE_COUNT)
        {
            if (ismounted[chosen_item])
            {
                if (0 != ensure_root_path_unmounted(mounts[chosen_item][2]))
                    ui_print("Error unmounting %s!\n", mounts[chosen_item][2]);
            }
            else
            {
                if (0 != ensure_root_path_mounted(mounts[chosen_item][2]))
                    ui_print("Error mounting %s!\n", mounts[chosen_item][2]);
            }
        }
        else if (chosen_item < MOUNTABLE_COUNT + MTD_COUNT)
        {
            chosen_item = chosen_item - MOUNTABLE_COUNT;
            if (!confirm_selection(confirm_format, confirm))
                continue;
            ui_print("Formatting %s...\n", mtds[chosen_item][1]);
            if (0 != format_root_device(mtds[chosen_item][1]))
                ui_print("Error formatting %s!\n", mtds[chosen_item][1]);
            else
                ui_print("Done.\n");
        }
        else if (chosen_item < MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT)
        {
            chosen_item = chosen_item - MOUNTABLE_COUNT - MTD_COUNT;
            if (!confirm_selection(confirm_format, confirm))
                continue;
            ui_print("Formatting %s...\n", mmcs[chosen_item][1]);
            if (0 != format_non_mtd_device(mmcs[chosen_item][1]))
                ui_print("Error formatting %s!\n", mmcs[chosen_item][1]);
            else
                ui_print("Done.\n");
        }
    }
}

#define EXTENDEDCOMMAND_SCRIPT "/cache/recovery/extendedcommand"

int extendedcommand_file_exists()
{
    struct stat file_info;
    return 0 == stat(EXTENDEDCOMMAND_SCRIPT, &file_info);
}

int run_script_from_buffer(char* script_data, int script_len, char* filename)
{
    /* Parse the script.  Note that the script and parse tree are never freed.
     */
    const AmCommandList *commands = parseAmendScript(script_data, script_len);
    if (commands == NULL) {
        printf("Syntax error in update script\n");
        return 1;
    } else {
        printf("Parsed %.*s\n", script_len, filename);
    }

    /* Execute the script.
     */
    int ret = execCommandList((ExecContext *)1, commands);
    if (ret != 0) {
        int num = ret;
        char *line = NULL, *next = script_data;
        while (next != NULL && ret-- > 0) {
            line = next;
            next = memchr(line, '\n', script_data + script_len - line);
            if (next != NULL) *next++ = '\0';
        }
        printf("Failure at line %d:\n%s\n", num, next ? line : "(not found)");
        return 1;
    }    
    
    return 0;
}

int run_script(char* filename)
{
    struct stat file_info;
    if (0 != stat(filename, &file_info)) {
        printf("Error executing stat on file: %s\n", filename);
        return 1;
    }
    
    int script_len = file_info.st_size;
    char* script_data = (char*)malloc(script_len + 1);
    FILE *file = fopen(filename, "rb");
    fread(script_data, script_len, 1, file);
    // supposedly not necessary, but let's be safe.
    script_data[script_len] = '\0';
    fclose(file);
    LOGI("Running script:\n");
    LOGI("\n%s\n", script_data);

    int ret = run_script_from_buffer(script_data, script_len, filename);
    free(script_data);
    return ret;
}

int run_and_remove_extendedcommand()
{
    char tmp[PATH_MAX];
    sprintf(tmp, "cp %s /tmp/%s", EXTENDEDCOMMAND_SCRIPT, basename(EXTENDEDCOMMAND_SCRIPT));
    __system(tmp);
    remove(EXTENDEDCOMMAND_SCRIPT);
    int i = 0;
    for (i = 20; i > 0; i--) {
        ui_print("Waiting for SD Card to mount (%ds)\n", i);
        if (ensure_root_path_mounted("SDCARD:") == 0) {
            ui_print("SD Card mounted...\n");
            break;
        }
        sleep(1);
    }
    remove("/mnt/sdcard/clockworkmod/.recoverycheckpoint");
    if (i == 0) {
        ui_print("Timed out waiting for SD card... continuing anyways.");
    }
    
    sprintf(tmp, "/tmp/%s", basename(EXTENDEDCOMMAND_SCRIPT));
    return run_script(tmp);
}

int steam_amend_main(int argc, char** argv)
{
    if (argc != 2) 
    {
        printf("Usage: amend <script>\n");
        return 0;
    }

    RecoveryCommandContext ctx = { NULL };
    if (register_update_commands(&ctx)) {
        LOGE("Can't install update commands\n");
    }
    return run_script(argv[1]);
}

void show_nandroid_advanced_backup_menu(const char* backup_path)
{
    int flags = BACKUP_DATA|BACKUP_CACHE|BACKUP_SDEXT|BACKUP_OTHERS;
#ifdef HAS_DATADATA
    flags = flags|BACKUP_DATADATA;
#endif
    int chosen_item = 1;
    struct menuElement me;
    while (true) {
      ui_start_menu_ext();
      ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,NANDROID_BADVANCED,NULL);
      ui_add_menu(0,-1,MENU_TYPE_ELEMENT,NANDROID_DOBACKUP,NANDROID_DOBACKUP_HELP);
      ui_add_menu(flags&BACKUP_BOOTABLES,BACKUP_BOOTABLES,MENU_TYPE_CHECKBOX,NANDROID_BACBOOT,NULL);
      ui_add_menu(flags&BACKUP_SYSTEM,BACKUP_SYSTEM,MENU_TYPE_CHECKBOX,NANDROID_BACSYSTEM,NULL);
      ui_add_menu(flags&BACKUP_DATA,BACKUP_DATA,MENU_TYPE_CHECKBOX,NANDROID_BACDATA,NULL);
#ifdef HAS_DATADATA
      ui_add_menu(flags&BACKUP_DATADATA,BACKUP_DATADATA,MENU_TYPE_CHECKBOX,NANDROID_BACDATADATA,NULL);
#endif
      ui_add_menu(flags&BACKUP_OTHERS,BACKUP_OTHERS,MENU_TYPE_CHECKBOX,NANDROID_BACMISC,NULL);
      ui_add_menu(flags&BACKUP_CACHE,BACKUP_CACHE,MENU_TYPE_CHECKBOX,NANDROID_BACCACHE,NULL);
      ui_add_menu(flags&BACKUP_SDEXT,BACKUP_SDEXT,MENU_TYPE_CHECKBOX,NANDROID_BACSDEXT,NULL);
#ifdef BOARD_HAS_PHONE_CONTROLLER
      ui_add_menu(flags&BACKUP_EFS,BACKUP_EFS,MENU_TYPE_CHECKBOX,NANDROID_BACEFS,NULL);
#endif
      chosen_item = get_menu_selection_ext(chosen_item, &me);
      if (chosen_item == GO_BACK) { ui_end_menu(); return; }
      if (me.group_id==-1) { ui_end_menu(); break; }
      if (me.group_id==BACKUP_BOOTABLES) { if (me.id) flags = flags-BACKUP_BOOTABLES; else flags = flags|BACKUP_BOOTABLES; }
      if (me.group_id==BACKUP_SYSTEM) { if (me.id) flags = flags-BACKUP_SYSTEM; else flags = flags|BACKUP_SYSTEM; }
      if (me.group_id==BACKUP_DATA) { if (me.id) flags = flags-BACKUP_DATA; else flags = flags|BACKUP_DATA; }
      if (me.group_id==BACKUP_DATADATA) { if (me.id) flags = flags-BACKUP_DATADATA; else flags = flags|BACKUP_DATADATA; }
      if (me.group_id==BACKUP_OTHERS) { if (me.id) flags = flags-BACKUP_OTHERS; else flags = flags|BACKUP_OTHERS; }
      if (me.group_id==BACKUP_CACHE) { if (me.id) flags = flags-BACKUP_CACHE; else flags = flags|BACKUP_CACHE; }
      if (me.group_id==BACKUP_SDEXT) { if (me.id) flags = flags-BACKUP_SDEXT; else flags = flags|BACKUP_SDEXT; }
      if (me.group_id==BACKUP_EFS) { if (me.id) flags = flags-BACKUP_EFS; else flags = flags|BACKUP_EFS; }
      ui_end_menu();
    }
    nandroid_backup_flags(backup_path, flags);
}


void show_nandroid_advanced_restore_menu()
{
    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /mnt/sdcard\n");
        return;
    }

    static char* advancedheaders[] = {  NANDROID_HEADER, NULL };

    char* file = choose_file_menu("/mnt/sdcard/clockworkmod/backup/", NULL, advancedheaders);
    if (file == NULL)
        return;

    int flags = BACKUP_DATA|BACKUP_CACHE|BACKUP_SDEXT|BACKUP_OTHERS|BACKUP_NOFORMAT;
#ifdef HAS_DATADATA
    flags = flags|BACKUP_DATADATA;
#endif
    int chosen_item = 1;
    struct menuElement me;
    static char* confirm_restore  = NANDROID_CONFIRM;
    while (true) {
      ui_start_menu_ext();
      ui_add_menu(0,0,MENU_TYPE_GLOBAL_HEADER,NANDROID_ADVANCED,NULL);
      ui_add_menu(0,-1,MENU_TYPE_ELEMENT,NANDROID_DORESTORE,NANDROID_DORESTORE_HELP);
      ui_add_menu(flags&BACKUP_BOOTABLES,BACKUP_BOOTABLES,MENU_TYPE_CHECKBOX,NANDROID_RESBOOT,NULL);
      ui_add_menu(flags&BACKUP_SYSTEM,BACKUP_SYSTEM,MENU_TYPE_CHECKBOX,NANDROID_RESSYSTEM,NULL);
      ui_add_menu(flags&BACKUP_DATA,BACKUP_DATA,MENU_TYPE_CHECKBOX,NANDROID_RESDATA,NULL);
#ifdef HAS_DATADATA
      ui_add_menu(flags&BACKUP_DATADATA,BACKUP_DATADATA,MENU_TYPE_CHECKBOX,NANDROID_RESDATADATA,NULL);
#endif
      ui_add_menu(flags&BACKUP_OTHERS,BACKUP_OTHERS,MENU_TYPE_CHECKBOX,NANDROID_RESMISC,NULL);
      ui_add_menu(flags&BACKUP_CACHE,BACKUP_CACHE,MENU_TYPE_CHECKBOX,NANDROID_RESCACHE,NULL);
      ui_add_menu(flags&BACKUP_SDEXT,BACKUP_SDEXT,MENU_TYPE_CHECKBOX,NANDROID_RESSDEXT,NULL);
      ui_add_menu(flags&BACKUP_NOFORMAT,BACKUP_NOFORMAT,MENU_TYPE_CHECKBOX,NANDROID_NOFORMAT,NANDROID_NOFORMAT_HELP);
      chosen_item = get_menu_selection_ext(chosen_item, &me);
      if (chosen_item == GO_BACK) { ui_end_menu(); return; }
      if (me.group_id==-1) { ui_end_menu(); break; }
      if (me.group_id==BACKUP_BOOTABLES) { if (me.id) flags = flags-BACKUP_BOOTABLES; else flags = flags|BACKUP_BOOTABLES; }
      if (me.group_id==BACKUP_SYSTEM) { if (me.id) flags = flags-BACKUP_SYSTEM; else flags = flags|BACKUP_SYSTEM; }
      if (me.group_id==BACKUP_DATA) { if (me.id) flags = flags-BACKUP_DATA; else flags = flags|BACKUP_DATA; }
      if (me.group_id==BACKUP_DATADATA) { if (me.id) flags = flags-BACKUP_DATADATA; else flags = flags|BACKUP_DATADATA; }
      if (me.group_id==BACKUP_OTHERS) { if (me.id) flags = flags-BACKUP_OTHERS; else flags = flags|BACKUP_OTHERS; }
      if (me.group_id==BACKUP_CACHE) { if (me.id) flags = flags-BACKUP_CACHE; else flags = flags|BACKUP_CACHE; }
      if (me.group_id==BACKUP_SDEXT) { if (me.id) flags = flags-BACKUP_SDEXT; else flags = flags|BACKUP_SDEXT; }
      if (me.group_id==BACKUP_NOFORMAT) { if (me.id) flags = flags-BACKUP_NOFORMAT; else flags = flags|BACKUP_NOFORMAT; }
      ui_end_menu();
    }
    if (confirm_selection(confirm_restore, NANDROID_YES)) nandroid_restore_flags(file, flags);
}

void show_nandroid_menu()
{
    static char* headers[] = {  NANDROID_MAIN_MENU_HEADER, NULL };

    static char* list[] = { NANDROID_MAIN_BACKUP, NANDROID_MAIN_ABACKUP, NANDROID_MAIN_RESTORE, NANDROID_MAIN_ARESTORE, NULL };

    int chosen_item = get_menu_selection(headers, list, 0);
    switch (chosen_item)
    {
        case 0:
        case 1:
            {
                char backup_path[PATH_MAX];
                time_t t = time(NULL);
                struct tm *tmp = localtime(&t);
                if (tmp == NULL)
                {
                    struct timeval tp;
                    gettimeofday(&tp, NULL);
                    sprintf(backup_path, "/mnt/sdcard/clockworkmod/backup/%d", tp.tv_sec);
                }
                else
                {
                    strftime(backup_path, sizeof(backup_path), "/mnt/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
                }
                if (chosen_item==0) {
                  nandroid_backup(backup_path);
                } else {
                  show_nandroid_advanced_backup_menu(backup_path);
                }
            }
            break;
        case 2:
            show_nandroid_restore_menu();
            break;
        case 3:
            show_nandroid_advanced_restore_menu();
            break;
    }
}

void wipe_battery_stats()
{
    ensure_root_path_mounted("DATA:");
    remove("/data/system/batterystats.bin");
    ensure_root_path_unmounted("DATA:");
}

void show_advanced_menu()
{
    static char* headers[] = {  ADV_MENU_HEADER, NULL };

    static char* list[] = { ADV_MENU_WIPE_CACHE,
                            ADV_MENU_WIPE_BATTERY,
                            ADV_MENU_REPORT,
                            ADV_MENU_KEYTEST,
                            ADV_MENU_SCREENTEST,
                            ADV_MENU_SECRETTEST,
#ifndef BOARD_HAS_SMALL_RECOVERY
                            ADV_MENU_PARTITION,
                            ADV_MENU_FIXPERM,
#endif
                            NULL
    };

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
            {
                if (0 != ensure_root_path_mounted("DATA:"))
                    break;
                ensure_root_path_mounted("SDEXT:");
                ensure_root_path_mounted("CACHE:");
                if (confirm_selection( ADV_MENU_WIPE_CACHE_CONFIRM, ADV_MENU_WIPE_CACHE_YES )) {
                    __system("rm -r /data/dalvik-cache");
                    __system("rm -r /cache/dalvik-cache");
                    __system("rm -r /sd-ext/dalvik-cache");
                }
                ensure_root_path_unmounted("DATA:");
                ui_print(ADV_MENU_WIPE_CACHE_DONE);
                break;
            }
            case 1:
            {
                if (confirm_selection( ADV_MENU_WIPE_BATTERY_CONFIRM, ADV_MENU_WIPE_BATTERY_YES ))
                    wipe_battery_stats();
                break;
            }
            case 2:
                handle_failure(1);
                break;
            case 3:
            {
                ui_print("Outputting key codes.\n");
                ui_print("Go back to end debugging.\n");
                int key;
                int action;
                ui_set_show_text(1);
                do
                {
                    key = ui_wait_key();
                    action = device_handle_key(key, 1);
                    ui_print("Key: %d\n", key);
                }
                while (action != GO_BACK);
                ui_set_show_text(0);
                break;
            }
            case 4:
            {
                ui_set_mouse_test(1);
                int chosen_item = -1;
                ui_print("Press any key to exit\n");
                ui_clear_key_queue();
                while (chosen_item<0) {
                  int key = ui_wait_key();
                  if (key!=BTN_WHEEL && key!=BTN_GEAR_UP && key!=BTN_GEAR_DOWN && key!=BTN_MOUSE) chosen_item=1;
                }
                ui_set_mouse_test(0);
                break;
            }
            case 5:
            {
                ui_set_secret_screen(1);
                ui_set_show_text(1);
                int chosen_item = 0;
                ui_clear_num_screen(0);
                ui_print(SECRET_HOWTO_TEST);
                ui_clear_key_queue();
                set_console_cmd("");
                while (!chosen_item) {
                  int key = ui_wait_key();
                  int action = device_handle_key(key, 1);

                  if (action == SELECT_ITEM) { chosen_item = SELECT_ITEM;  };
                  if (action == GO_BACK) { chosen_item = GO_BACK; };
                }
                if (chosen_item==SELECT_ITEM) {
                  ui_print("Entered: %s\n",get_console_cmd());
                }
                ui_set_secret_screen(0);
                ui_set_show_text(0);
                break;             
            }
            case 6:
            {
                static char* ext_sizes[] = { "128M",
                                             "256M",
                                             "512M",
                                             "1024M",
                                             NULL };

                static char* swap_sizes[] = { "0M",
                                              "32M",
                                              "64M",
                                              "128M",
                                              "256M",
                                              NULL };

                static char* ext_headers[] = { "Ext Size", "", NULL };
                static char* swap_headers[] = { "Swap Size", "", NULL };

                int ext_size = get_menu_selection(ext_headers, ext_sizes, 0);
                if (ext_size == GO_BACK)
                    continue;
                 
                int swap_size = get_menu_selection(swap_headers, swap_sizes, 0);
                if (swap_size == GO_BACK)
                    continue;

                char sddevice[256];
#ifdef HAS_TWO_SDCARDS
                // if it has two, we don't want to mess with the internal
                const RootInfo *ri = get_root_info_for_path("SDCARD2:");
#else
                const RootInfo *ri = get_root_info_for_path("SDCARD:");
#endif
                strcpy(sddevice, ri->device);
                // we only want the mmcblk, not the partition
                sddevice[strlen(SDCARD_EXT_BLOCK_NAME)] = NULL;
                char cmd[PATH_MAX];
                setenv("SDPATH", sddevice, 1);
                sprintf(cmd, "sdparted -es %s -ss %s -efs ext3 -s", ext_sizes[ext_size], swap_sizes[swap_size]);
                ui_print("Partitioning SD Card... please wait...\n");
                if (0 == __system(cmd))
                    ui_print("Done!\n");
                else
                    ui_print("An error occured while partitioning your SD Card. Please see /tmp/recovery.log for more details.\n");
                break;
            }
            case 7:
            {
                ensure_root_path_mounted("SYSTEM:");
                ensure_root_path_mounted("DATA:");
                ui_print("Fixing permissions...\n");
                __system("fix_permissions");
                ui_print("Done!\n");
                break;
            }
        }
    }
}

void write_fstab_root(char *root_path, FILE *file)
{
    RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL) {
        LOGW("Unable to get root info for %s during fstab generation!", root_path);
        return;
    }
    MtdPartition *mtd = get_root_mtd_partition(root_path);
    if (mtd != NULL)
    {
        fprintf(file, "/dev/block/mtdblock%d ", mtd->device_index);
    }
    else
    {
        fprintf(file, "%s ", info->device);
    }
    
    fprintf(file, "%s ", info->mount_point);
    if (info->filesystem == g_auto) {
      // TODO: write the real stuff here
      fprintf(file, "auto %s\n", info->filesystem_options == NULL ? "rw" : info->filesystem_options); 
    } else {
      fprintf(file, "%s %s\n", info->filesystem, info->filesystem_options == NULL ? "rw" : info->filesystem_options); 
    }
}

void create_fstab()
{
    __system("touch /etc/mtab");
    FILE *file = fopen("/etc/fstab", "w");
    if (file == NULL) {
        LOGW("Unable to create /etc/fstab!");
        return;
    }
    write_fstab_root("CACHE:", file);
    write_fstab_root("DATA:", file);
#ifdef HAS_DATADATA
    write_fstab_root("DATADATA:", file);
#endif
    write_fstab_root("SYSTEM:", file);
    write_fstab_root("SDCARD:", file);
    write_fstab_root("SDEXT:", file);
    fclose(file);
}

void handle_failure(int ret)
{
    if (ret == 0)
        return;
    if (0 != ensure_root_path_mounted("SDCARD:"))
        return;
    mkdir("/mnt/sdcard/clockworkmod", S_IRWXU);
    __system("cp /tmp/*.log /mnt/sdcard/clockworkmod/");
    ui_print("/tmp/*.log was copied to /mnt/sdcard/clockworkmod.\n");
}
