/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include <linux/input.h>

#include "ui.h"
#include "extendedcommands.h"
#include "device.h"
#include "locale.h"
#include "roots.h"

char* MENU_HEADERS[] = { NULL };

char* MENU_ITEMS[] = { MENU_REBOOT_RECOVERY,
                       MENU_APPLY_SDCARD,
                       MENU_WIPE_ALL,
                       MENU_WIPE_CACHE,
                       MENU_INSTALL_SDCARD,
                       MENU_BACKUP,
                       MENU_ADV_ULK,
                       MENU_AFTERBURNER,
                       NULL };

const char g_mtd_device[] = "@\0g_mtd_device";
const char g_mmc_device[] = "@\0g_mmc_device";
const char g_raw[] = "@\0g_raw";
const char g_package_file[] = "@\0g_package_file";
const char g_auto[] = "@\0g_auto";

#include "device/sgs-i9000.c"

int get_num_roots() {
  return NUM_ROOTS;
}

int device_recovery_start() {
    return 0;
}

int device_toggle_display(volatile char* key_pressed, int key_code) {
    return get_allow_toggle_display() && key_code == KEY_DEVICE_POWER;
}

int device_reboot_now(volatile char* key_pressed, int key_code) {
    return 0;
}

int device_handle_key(int key_code, int visible) {
    if (visible) {
        switch (key_code) {
            case KEY_DEVICE_VOLUP:
                return HIGHLIGHT_DOWN;
            case KEY_DEVICE_VOLDOWN:
                return HIGHLIGHT_UP;
            case KEY_DEVICE_POWER:
                break;
            case KEY_DEVICE_HOME:
                return SELECT_ITEM;
            case BTN_MOUSE:
                return SELECT_ITEM_MOUSE;
            case KEY_DEVICE_BACK:
                return GO_BACK;
            case KEY_DEVICE_MENU:
                break;
        }
    }

    return NO_ACTION;
}

int device_perform_action(int which) {
    return which;
}

int device_wipe_data() {
    return 0;
}
