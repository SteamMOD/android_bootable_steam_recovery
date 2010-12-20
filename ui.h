/*
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef RECOVERY_COMMON_H
#define RECOVERY_COMMON_H

#include <stdio.h>

#define NUM_TEXTCONTAINERS 4
#define TEXTCONTAINER_MAIN 0
#define TEXTCONTAINER_STDOUT 1
#define TEXTCONTAINER_DMESG 2
#define TEXTCONTAINER_LOGCAT 3

// New style menu interface
#define MENU_TYPE_GLOBAL_HEADER 0
#define MENU_TYPE_ELEMENT 1
#define MENU_TYPE_GROUP_HEADER 2
#define MENU_TYPE_CHECKBOX 3
#define MENU_TYPE_RADIOBOX 4

extern char TEMPORARY_LOG_FILE[255];

// Initialize and destroy the graphics and events system.
void ui_init();
void ui_done();

// Use KEY_* codes from <linux/input.h> or KEY_DREAM_* from "minui/minui.h".
int ui_wait_key();            // waits for a key/button press, returns the code
int ui_key_pressed(int key);  // returns >0 if the code is currently pressed
int ui_text_visible();        // returns >0 if text log is currently visible
void ui_clear_key_queue();

// Write a message to the on-screen log shown with Alt-L (also to stderr).
// The screen is small, and users may need to report these messages to support,
// so keep the output short and not too cryptic.
void ui_print(const char *fmt, ...);

void ui_reset_text_col();
void ui_set_show_text(int value);
int ui_get_show_text();

// Old style menu compatibility interface
// Display some header text followed by a menu of items, which appears
// at the top of the screen (in place of any scrolling ui_print()
// output, if necessary).
int ui_start_menu(char** headers, char** items);
// Old stlye menu compatibility interface with online help
int ui_start_menu_help(char** headers, char** items);
// Set the menu highlight to the given index, and return it (capped to
// the range [0..numitems).
int ui_menu_select(int sel);
// End menu mode, resetting the text overlay so that ui_print()
// statements will be displayed.
void ui_end_menu();

struct menuElement;
struct menuElement {
  int id;
  int group_id;
  int type;
  char *text;
  char *help;
  struct menuElement* next;
};

// New style menu interface
// Initialize menu interface
void ui_start_menu_ext();
// Add an element
int ui_add_menu(int id, int group_id, int type, char* text, char* help);
int ui_add_menu_ext(struct menuElement el);
// Get the selected element
int ui_get_selected_ext(struct menuElement* el);
// end menu is the same

int ui_set_page(int new_page);

int ui_get_showing_back_button();
void ui_set_showing_back_button(int showBackButton);

// gets currently selected element
int ui_get_selected();
// gets whether the selected element could be valid from a mouse click
int ui_valid_mouse_select();

void ui_clear_num_screen(int i);

// Set the icon (normally the only thing visible besides the progress bar).
enum {
  BACKGROUND_ICON_NONE,
  BACKGROUND_ICON_INSTALLING,
  BACKGROUND_ICON_ERROR,
  BACKGROUND_ICON_FIRMWARE_INSTALLING,
  BACKGROUND_ICON_FIRMWARE_ERROR,
  NUM_BACKGROUND_ICONS
};
void ui_set_background(int icon);

// Get a malloc'd copy of the screen image showing (only) the specified icon.
// Also returns the width, height, and bits per pixel of the returned image.
// TODO: Use some sort of "struct Bitmap" here instead of all these variables?
char *ui_copy_image(int icon, int *width, int *height, int *bpp);

// Show a progress bar and define the scope of the next operation:
//   portion - fraction of the progress bar the next operation will use
//   seconds - expected time interval (progress bar moves at this minimum rate)
void ui_show_progress(float portion, int seconds);
void ui_set_progress(float fraction);  // 0.0 - 1.0 within the defined scope

int get_ui_state();
// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;

// Show a rotating "barberpole" for ongoing operations.  Updates automatically.
void ui_show_indeterminate_progress();

// Hide and reset the progress bar.
void ui_reset_progress();

// Console stuff
void ui_set_console(int value);
void ui_set_secret_screen(int value);
void set_console_cmd(const char* str);
const char* get_console_cmd();

#define LOGE(...) ui_print("E:" __VA_ARGS__)
#define LOGW(...) fprintf(stderr, "W:" __VA_ARGS__)
#define LOGI(...) fprintf(stderr, "I:" __VA_ARGS__)

#if 0
#define LOGV(...) fprintf(stderr, "V:" __VA_ARGS__)
#define LOGD(...) fprintf(stderr, "D:" __VA_ARGS__)
#else
#define LOGV(...) do {} while (0)
#define LOGD(...) do {} while (0)
#endif

#define STRINGIFY(x) #x
#define EXPAND(x) STRINGIFY(x)

#endif  // RECOVERY_COMMON_H
