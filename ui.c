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

#include <linux/input.h>

//these are included in the original kernel's linux/input.h but are missing from AOSP

#ifndef SYN_MT_REPORT
#define SYN_MT_REPORT 2
#define ABS_MT_TOUCH_MAJOR  0x30  /* Major axis of touching ellipse */
#define ABS_MT_WIDTH_MAJOR  0x32  /* Major axis of approaching ellipse */
#define ABS_MT_POSITION_X 0x35  /* Center X ellipse position */
#define ABS_MT_POSITION_Y 0x36  /* Center Y ellipse position */
#define ABS_MT_TRACKING_ID 0x39  /* Center Y ellipse position */
#endif

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include "system.h"

#include "ui.h"
#include "minui/minui.h"
#include "device.h"

#ifdef KEY_POWER_IS_SELECT_ITEM
static int gShowBackButton = 1;
#else
static int gShowBackButton = 0;
#endif

#define MAX_COLS 64
#define MAX_ROWS 45

#define MENU_MAX_COLS 64
#define MENU_MAX_ROWS 250

#define CHAR_WIDTH 10
#define CHAR_HEIGHT 18

#define PROGRESSBAR_INDETERMINATE_STATES 6
#define PROGRESSBAR_INDETERMINATE_FPS 24

static pthread_mutex_t gUpdateMutex = PTHREAD_MUTEX_INITIALIZER;
static gr_surface gBackgroundIcon[NUM_BACKGROUND_ICONS];
static gr_surface gProgressBarIndeterminate[PROGRESSBAR_INDETERMINATE_STATES];
static gr_surface gProgressBarEmpty;
static gr_surface gProgressBarFill;

static gr_surface gBackgroundSave = NULL;

static int ui_has_initialized = 0;

static const struct { gr_surface* surface; const char *name; } BITMAPS[] = {
    { &gBackgroundIcon[BACKGROUND_ICON_INSTALLING], "icon_steam" },
    { &gBackgroundIcon[BACKGROUND_ICON_ERROR],      "icon_steam" },
    { &gProgressBarIndeterminate[0],    "indeterminate1" },
    { &gProgressBarIndeterminate[1],    "indeterminate2" },
    { &gProgressBarIndeterminate[2],    "indeterminate3" },
    { &gProgressBarIndeterminate[3],    "indeterminate4" },
    { &gProgressBarIndeterminate[4],    "indeterminate5" },
    { &gProgressBarIndeterminate[5],    "indeterminate6" },
    { &gProgressBarEmpty,               "progress_empty" },
    { &gProgressBarFill,                "progress_fill" },
    { NULL,                             NULL },
};

static gr_surface gCurrentIcon = NULL;

static enum ProgressBarType {
    PROGRESSBAR_TYPE_NONE,
    PROGRESSBAR_TYPE_INDETERMINATE,
    PROGRESSBAR_TYPE_NORMAL,
} gProgressBarType = PROGRESSBAR_TYPE_NONE;

// Progress bar scope of current operation
static float gProgressScopeStart = 0, gProgressScopeSize = 0, gProgress = 0;
static time_t gProgressScopeTime, gProgressScopeDuration;

// Set to 1 when both graphics pages are the same (except for the progress bar)
static int gPagesIdentical = 0;

// Log text overlay, displayed when a magic key is pressed.
static struct textContainer {
  char text[MAX_ROWS][MAX_COLS];
  int text_cols, text_rows;
  int text_col, text_row, text_top;
} logs[4], *activeLog;
// whether to show the main screen, or the log screen
static int show_text = 0;
// which log to show. 0:mainLog, 1:stdoutLog, 2:dmesgLog, 3:logcatLog
static int current_page = 0;
// whether to show the mouse testing screen or not
static int enable_mouse_test = 0;
// whether to show the console screen or not
static int enable_console_screen = 0;
// whether to show the key swipe screen or not
static int enable_secret_screen = 0;

// old style menu interface
static char menu[MENU_MAX_ROWS][MENU_MAX_COLS];
static int show_menu = 0;
static int menu_items = 0, menu_sel = 0;

// Key event input queue
static pthread_mutex_t key_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t key_queue_cond = PTHREAD_COND_INITIALIZER;
static int key_queue[256], key_queue_len = 0;
static volatile char key_pressed[KEY_MAX + 1];

// Threads
static pthread_t pt_ui_thread;
static pthread_t pt_input_thread;
static pthread_t logreaders[NUM_TEXTCONTAINERS-1]; //we don't need one for the main thread
static volatile int pt_ui_thread_active = 1;
static volatile int pt_input_thread_active = 1;
static volatile int pt_logreaders_active = 1; //one switch for all

// Desire/Nexus and similar have 2, SGS has 5, SGT has 10, we take the max as it's cool. We'll only use 1 however
#define MAX_MT_POINTS 10

// Struct to store mouse events
static struct mousePosStruct {
  int x;
  int y;
  int pressure; // 0:up or 40:down
  int size;
  int num;
  int length; // length of the line drawn while in touch state
} actPos, grabPos, oldMousePos[MAX_MT_POINTS], mousePos[MAX_MT_POINTS];

// Data to store the ui information for the menu
static int menu_max_height = 480;
static int menu_pos = 0;
static int menu_start_pos = 0;
static float menu_velocity = 0;
static int menu_pressed = 0;
static int menu_sel_mouse_valid = 0;

// console specific stuff
#define NUM_KEYBOARD_PAGES 4
#define KEYBOARD_PAGE_qwe 0
#define KEYBOARD_PAGE_QWE 1
#define KEYBOARD_PAGE_sym 2
#define KEYBOARD_PAGE_123 3

#define KEYBOARD_CHAR_SHIFT 1
#define KEYBOARD_CHAR_DEL 2
#define KEYBOARD_CHAR_SWITCH 3
static char console_cmd[256];
static int keyboard_page = KEYBOARD_PAGE_qwe;
static int console_length = 0;
static char* keyboard_line[NUM_KEYBOARD_PAGES][4] = {
  {"qwertyuiop","asdfghjkl","\001zxcvbnm\002","\003/.,    \n\n"},
  {"QWERTYUIOP","ASDFGHJKL","\001ZXCVBNM\002","\003/.,    \n\n"},
  {"1234567890","!@#$%^&*()","_+{}:\"|<>?","\003-=[];'\\~`"},
  {"123.","456-","789\002","\0030 \n"}
};

static struct menuElement* menu_first = NULL;

// this will delete menu elements recursively
static void deleteMenu(struct menuElement* e) {
  if (e->next) {
    deleteMenu(e->next);
    free(e->next);
  }
  if (e->text) {
    free(e->text);
  }
  if (e->help) {
    free(e->help);
  }
  e->help = NULL;
  e->next = NULL;
  e->text = NULL;
}

// Clear the screen and draw the currently selected background icon (if any).
// Should only be called with gUpdateMutex locked.
static void draw_background_locked(gr_surface icon)
{
  gPagesIdentical = 0;
  if (gBackgroundSave) {
    gr_color(255, 255, 255, 255);
    gr_blend(0);
    gr_blit(gBackgroundSave, 0, 0, gr_fb_width(), gr_fb_height(), 0, 0);
    gr_blend(1);
  } else {
    gr_color(0, 0, 0, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    if (icon) {
        int iconWidth = gr_get_width(icon);
        int iconHeight = gr_get_height(icon);
        int iconX = (gr_fb_width() - iconWidth) / 2;
        int iconY = (gr_fb_height() - iconHeight) / 2;
        gr_blit(icon, 0, 0, iconWidth, iconHeight, iconX, iconY);
    }
    gr_color(0, 0, 0, 222);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    gBackgroundSave = gr_create_surface();
    gr_save_active_surface(gBackgroundSave);
  }
}

// Draw the progress bar (if any) on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
static void draw_progress_locked()
{
    if (gProgressBarType == PROGRESSBAR_TYPE_NONE) return;
    if (enable_console_screen) return;

    int iconHeight = gr_get_height(gBackgroundIcon[BACKGROUND_ICON_INSTALLING]);
    int width = gr_get_width(gProgressBarEmpty);
    int height = gr_get_height(gProgressBarEmpty);

    int dx = (gr_fb_width() - width)/2;
    int dy = (gr_fb_height() - 2*height);

    // Erase behind the progress bar (in case this was a progress-only update)
    gr_color(0, 0, 0, 255);
    gr_fill(dx, dy, width, height);

    if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL) {
        float progress = gProgressScopeStart + gProgress * gProgressScopeSize;
        int pos = (int) (progress * width);

        if (pos > 0) {
          gr_blit(gProgressBarFill, 0, 0, pos, height, dx, dy);
        }
        if (pos < width-1) {
          gr_blit(gProgressBarEmpty, pos, 0, width-pos, height, dx+pos, dy);
        }
    }

    if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE) {
        static int frame = 0;
        gr_blit(gProgressBarIndeterminate[frame], 0, 0, width, height, dx, dy);
        frame = (frame + 1) % PROGRESSBAR_INDETERMINATE_STATES;
    }
}

static void draw_text_line(int row, const char* t) {
  if (enable_console_screen) {
    if (t[0] != '\0') {
      gr_text(0, (row+1-17)*CHAR_HEIGHT-1, t);
    }
  } else if (gProgressBarType == PROGRESSBAR_TYPE_NONE) {
    if (t[0] != '\0') {
      gr_text(0, (row+1)*CHAR_HEIGHT-1, t);
    }
  } else {
    if (t[0] != '\0') {
      gr_text(0, (row+1-4)*CHAR_HEIGHT-1, t);
    }
  }
}

#define MENU_TEXT_COLOR 7, 133, 74, 255
#define MENU_TEXT_COLOR_BACK 7, 133, 74, 128
#define MENU_TEXT_COLOR_BACK_PRESS 7, 133, 74, 196
#define NORMAL_TEXT_COLOR 200, 200, 200, 255
#define HEADER_TEXT_COLOR NORMAL_TEXT_COLOR

// 4 times the character length is enough for touch input. 3 is too small, 5 takes too much space
#define MENU_ELEMENT_HEIGHT (CHAR_HEIGHT*4)

static void draw_menu(void)
{
    if (show_text) {
        if (enable_console_screen) {
          activeLog = &logs[TEXTCONTAINER_STDOUT];
        } else {
          activeLog = &logs[TEXTCONTAINER_MAIN];
        }
        int i = 0;
        int j = 0;
        int row = 0;            // current row that we are drawing on

        int menu_height = menu_items*MENU_ELEMENT_HEIGHT;
        if (menu_height>menu_max_height) {
          menu_height = menu_max_height;
        }
        if (!show_menu) menu_height = 0;

        // first draw the text
        gr_color(NORMAL_TEXT_COLOR);
        row = menu_height/CHAR_HEIGHT;
        for (; row < activeLog->text_rows; ++row) {
            draw_text_line(row, activeLog->text[(row+activeLog->text_top) % activeLog->text_rows]);
        }

        // then fill it with the menu
        if (show_menu) {
            // mouse handling part
            if (grabPos.pressure) {
            } else {
              menu_start_pos = INT_MAX;
              if (abs(menu_velocity)>1) {
                menu_pos += menu_velocity;
                menu_velocity = menu_velocity/4*3;
              } else {
                menu_velocity = 0;
              }
              int menu_height = menu_items*MENU_ELEMENT_HEIGHT;
              int menu_all_height = menu_height;
              if (menu_height>menu_max_height) {
                menu_height = menu_max_height;
              }
              if (menu_pos>0) { menu_pos = 0; menu_velocity = 0; }
              if (menu_pos<-menu_all_height+menu_height) { menu_pos = -menu_all_height+menu_height; menu_velocity = 0; }
            }

            gr_color(MENU_TEXT_COLOR);
            gr_fill(0,menu_height,gr_fb_width(),menu_height+2);

            gr_crop(0, 0, gr_fb_width(), menu_height);
            draw_background_locked(gCurrentIcon);

            struct menuElement* curr = menu_first;
            int spos = -1;
            int top = menu_pos;
            while (curr!=NULL) {
              spos++;
              if (spos==menu_sel && curr->type!=MENU_TYPE_GLOBAL_HEADER && curr->type!=MENU_TYPE_GROUP_HEADER) {
                if (mousePos[0].length < 15) {
                  gr_color(MENU_TEXT_COLOR_BACK_PRESS);
                } else {
                  gr_color(MENU_TEXT_COLOR_BACK);
                }
                if (curr->help) {
                  if (grabPos.pressure) {
                    if (grabPos.x<gr_fb_width()-60) {
                      gr_fill(0, top, gr_fb_width()-60, top+MENU_ELEMENT_HEIGHT);
                    } else {
                      gr_fill(gr_fb_width()-60, top, gr_fb_width(), top+MENU_ELEMENT_HEIGHT);
                    }
                  } else {
                    gr_fill(0, top, gr_fb_width()-60, top+MENU_ELEMENT_HEIGHT);
                  }
                } else {
                  gr_fill(0, top, gr_fb_width(), top+MENU_ELEMENT_HEIGHT);
                }
                gr_color(NORMAL_TEXT_COLOR);
              }
              if (curr->type==MENU_TYPE_GLOBAL_HEADER) {
                gr_color(HEADER_TEXT_COLOR);
                gr_text_align(gr_fb_width()/2,top+MENU_ELEMENT_HEIGHT/2,GR_TEXT_ALIGN_CENTER,GR_TEXT_ALIGN_CENTER,0,curr->text);
              } else if (curr->type==MENU_TYPE_GROUP_HEADER) {
                gr_color(HEADER_TEXT_COLOR);
                gr_text_align(20,top+MENU_ELEMENT_HEIGHT-10,GR_TEXT_ALIGN_LEFT,GR_TEXT_ALIGN_BOTTOM,0,curr->text);
              } else {
                if (spos==menu_sel) {
                  gr_color(HEADER_TEXT_COLOR);
                } else {
                  gr_color(MENU_TEXT_COLOR);
                }
                gr_text_align(60,top+MENU_ELEMENT_HEIGHT/2,GR_TEXT_ALIGN_LEFT,GR_TEXT_ALIGN_CENTER,0,curr->text);
                if (curr->type==MENU_TYPE_CHECKBOX) {
                  gr_fill(20,top+MENU_ELEMENT_HEIGHT/2-10,40,top+MENU_ELEMENT_HEIGHT/2+10);
                  gr_color(0, 0, 0, 255);
                  gr_fill(22,top+MENU_ELEMENT_HEIGHT/2-8,38,top+MENU_ELEMENT_HEIGHT/2+8);
                  if (curr->id==curr->group_id) {
                    gr_color(255, 255, 255, 255);
                    gr_fill(24,top+MENU_ELEMENT_HEIGHT/2-6,36,top+MENU_ELEMENT_HEIGHT/2+6);
                  }
                }
                if (curr->type==MENU_TYPE_RADIOBOX) {
                  gr_point(30,top+MENU_ELEMENT_HEIGHT/2,20);
                  gr_color(0, 0, 0, 255);
                  gr_point(30,top+MENU_ELEMENT_HEIGHT/2,18);
                  if (curr->id==curr->group_id) {
                    gr_color(255, 255, 255, 255);
                    gr_point(30,top+MENU_ELEMENT_HEIGHT/2,16);
                  }
                }
              }
              gr_color(MENU_TEXT_COLOR);
              gr_fill(0,top+MENU_ELEMENT_HEIGHT-1,gr_fb_width(),top+MENU_ELEMENT_HEIGHT);
             
              if (curr->help) {
                gr_color(NORMAL_TEXT_COLOR);
                gr_text_align(gr_fb_width()-30,top+MENU_ELEMENT_HEIGHT/2,GR_TEXT_ALIGN_CENTER,GR_TEXT_ALIGN_CENTER,0,"?");
              }
              
              curr = curr->next;
              top+=MENU_ELEMENT_HEIGHT;
            }

            gr_crop(0, 0, 0, 0);
        }
    }
}

static void draw_logs()
{
    if (!show_text) {
      activeLog = &logs[current_page];
      int row = 0;
      gr_color(NORMAL_TEXT_COLOR);
      for (row = 0; row < activeLog->text_rows; ++row) {
          draw_text_line(row, activeLog->text[(row+activeLog->text_top) % activeLog->text_rows]);
      }
      gr_color(MENU_TEXT_COLOR);
      gr_fill(0,0,gr_fb_width(),40);
      gr_color(0, 0, 0, 255);
      for (row=0; row<NUM_TEXTCONTAINERS; row++) {
        if (row!=current_page) {
          gr_fill(row*(gr_fb_width()/NUM_TEXTCONTAINERS)+1,1,
                  (row+1)*(gr_fb_width()/NUM_TEXTCONTAINERS)-1,39);
        }
      }
      gr_color(NORMAL_TEXT_COLOR);
      for (row=0; row<NUM_TEXTCONTAINERS; row++) {
        char s[2];
        s[0]='0'+row;
        s[1]='\0';
        gr_text(row*(gr_fb_width()/NUM_TEXTCONTAINERS)+
                    (gr_fb_width()/NUM_TEXTCONTAINERS)/2-
                    CHAR_WIDTH/2,30,s);
      }
    }
}

static void draw_mouse_text()
{
    if (enable_mouse_test) {
      int i,i2;

      for (i=0; i<MAX_MT_POINTS; i++) {
        if (mousePos[i].pressure) {
          for (i2=i+1; i2<MAX_MT_POINTS; i2++) {
            if (mousePos[i2].pressure) {
              gr_color(0,128,0,128);
              gr_line(mousePos[i].x,mousePos[i].y,mousePos[i2].x,mousePos[i2].y,5);
            }
          }
          gr_color(128+(i%2)*128,128+(i%4<2?0:1)*128,128+(i%8<4?0:1)*128,255);
          gr_point(mousePos[i].x,mousePos[i].y,mousePos[i].size*16);
          gr_color(0,0,0,255);
          gr_point(mousePos[i].x,mousePos[i].y,mousePos[i].size*14);
          char d[2];
          d[0] = i+'0';
          d[1] = '\0';
          gr_color(255,255,255,192);
          gr_text(mousePos[i].x-mousePos[i].size*8-16,mousePos[i].y-mousePos[i].size*8-16,d);
        }
      }
    }
}

static void draw_console(void)
{
    if (enable_console_screen) {
      int i,i2,sl;
      int height = gr_fb_height() - CHAR_HEIGHT*14;
      int width;
      char* str;
      char s[2];
      s[1] = '\0';
      for (i=0; i<4; i++) {
        str = keyboard_line[keyboard_page][i];
        sl = strlen(str);
        width = gr_fb_width()/sl;
        for (i2=0; i2<sl; i2++) {
          s[0] = str[i2];
          switch (s[0]) {
            case KEYBOARD_CHAR_SHIFT:
              gr_text(i2*width + width/2-CHAR_WIDTH*2,height,"^^^");
              break;
            case KEYBOARD_CHAR_DEL:
              gr_text(i2*width + width/2-CHAR_WIDTH*2,height,"<==");
              break;
            case KEYBOARD_CHAR_SWITCH:
              gr_text(i2*width + width/2-CHAR_WIDTH*2,height,"123");
              break;
            case '\n':
              gr_text(i2*width + width/2-CHAR_WIDTH*3,height,"EEEEE");
              break;
            case ' ':
              gr_text(i2*width + width/2-CHAR_WIDTH*3,height,"_____");
              break;
            default:
              gr_text(i2*width + width/2,height,s);
          }
        }
        height += CHAR_HEIGHT*4;
      }
      gr_text(0,height-CHAR_HEIGHT*18,console_cmd);
    }
}

static void draw_secret(void)
{
  if (enable_secret_screen) {
    char* c = strrchr(console_cmd,'_');
    if (c == NULL) {
      c = console_cmd;
    } else {
      c = c + 1;
    }
    int x,y;
    int width = gr_fb_width()/4;
    int height = (gr_fb_height()-50)/6;
    int len = strlen(c);
    char s[2];
    s[1] = '\0';
    for (x=0; x<len-1; x++) {
      gr_color(0,255,0,192);
      int ax = (c[x]-65)%4;
      int ay = (c[x]-65)/4;
      int bx = (c[x+1]-65)%4;
      int by = (c[x+1]-65)/4;
      gr_line(ax*width+width/2,ay*height+height/2,bx*width+width/2,by*height+height/2,20);
    }
    if (len>0) {
      if (grabPos.pressure) {
        gr_color(0,255,0,128);
        int ax = (c[len-1]-65)%4;
        int ay = (c[len-1]-65)/4;
        gr_line(ax*width+width/2,ay*height+height/2,mousePos[0].x,mousePos[0].y,20);
      }
    }
    for (x=0; x<4; x++) {
      for (y=0; y<6; y++) {
        s[0] = 65+(y*4)+x;
        if (strrchr(c,s[0])) {
          gr_color(0,255,0,255);
        } else {
          gr_color(0,0,255,255);
        }
        gr_point(x*width+width/2,y*height+height/2,70);
        gr_color(0,0,0,255);
        gr_point(x*width+width/2,y*height+height/2,50);
        gr_color(255,255,255,255);
        gr_text_align(x*width+width/2,y*height+height/2,GR_TEXT_ALIGN_CENTER,GR_TEXT_ALIGN_CENTER,0,s);
      }
    }
    for (x=0; x<strlen(console_cmd); x++) {
      gr_text(x*CHAR_WIDTH,CHAR_HEIGHT,"*");
    }
  }
}

// Redraw everything on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
static void draw_screen_locked(void)
{
    if (!ui_has_initialized) return;
    draw_background_locked(gCurrentIcon);
    draw_progress_locked();

    draw_menu();
    draw_logs();
    draw_mouse_text();
    draw_console();
    draw_secret();
}

// Redraw everything on the screen and flip the screen (make it visible).
// Should only be called with gUpdateMutex locked.
static void update_screen_locked(void)
{
    if (!ui_has_initialized) return;
    draw_screen_locked();
    gr_flip();
}

void ui_print_to(struct textContainer* tc, const char *fmt, ...);

// Reads a log file, and splits it out
static void *log_reader(void *cookie)
{
  int type = (int)cookie;
  int fd = -1;
  pid_t pid = -1;
  int move,i;
  int is_popened = 0;
  char buf[256+1];
  char filename[128];
  struct pollfd fds;
  if (type==TEXTCONTAINER_STDOUT) {
    sprintf(filename,TEMPORARY_LOG_FILE);
  } else if (type==TEXTCONTAINER_DMESG) {
    sprintf(filename,"/proc/kmsg");
  } else if (type==TEXTCONTAINER_LOGCAT) {
    sprintf(filename,"/system/bin/logcat"); //should be improved...
    is_popened = 1;
  }

  int rbytes;
  struct stat sbuf, fsbuf;
  while (pt_logreaders_active) {
    if (!is_popened) {
      if ((fd<0) || (fstat(fd,&fsbuf)<0) || (stat(filename,&sbuf)<0)
          || (fsbuf.st_dev!=sbuf.st_dev) || (fsbuf.st_ino!=sbuf.st_ino)) {
        if (fd>=0) {
          close(fd);
        }
        fd = open(filename,O_RDONLY,0666);
      }
    } else {
      // we should also check if the process exited and probably restart it
      if (pid<0) {
        fd = -1;
        pid = popen3(NULL, &fd, NULL, POPEN_JOINSTDERR, filename);
        if (pid==-1) {
          fd = -1;
          usleep(1000000/PROGRESSBAR_INDETERMINATE_FPS);
        }
      }
    }
    fds.fd = fd;
    fds.events = POLLIN|POLLRDNORM;
    if (fd>=0) {
      do {
        rbytes = poll(&fds, 1, 1000/PROGRESSBAR_INDETERMINATE_FPS);
        if (rbytes>0 && !(fds.revents&POLLERR) && !(fds.revents&POLLNVAL) && !(fds.revents&POLLHUP)) {
          rbytes = read(fd, buf, sizeof(buf)-1);
          if (rbytes>0) {
            buf[rbytes] = '\0';
            move=0;
            for (i=0; i<rbytes; i++) {
              if ((buf[i]!='\n') && ((buf[i]<' ') || (buf[i]>'~'))) {
                move++;
              } else {
                buf[i-move] = buf[i];
              }
            }
            buf[rbytes-move] = '\0';
            ui_print_to(&logs[type],"%s",buf);
          }
        } else {
          rbytes = 0;
        }
      } while(rbytes>0);
    }
  }
  if (fd>=0) {
    if (is_popened) {
      pclose3(pid, NULL, &fd, NULL, SIGKILL);
    } else {
      close(fd);
    }
  }
  pthread_exit((void*)type);
  return (void*)type;
}

// Keeps the ui updated (this only updated the progressbar)
static void *ui_thread(void *cookie)
{
    while (pt_ui_thread_active) {
        usleep(1000000 / PROGRESSBAR_INDETERMINATE_FPS);
        pthread_mutex_lock(&gUpdateMutex);

        // move the progress bar forward on timed intervals, if configured
        int duration = gProgressScopeDuration;
        if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && duration > 0) {
            int elapsed = time(NULL) - gProgressScopeTime;
            float progress = 1.0 * elapsed / duration;
            if (progress > 1.0) progress = 1.0;
            if (progress > gProgress) {
                gProgress = progress;
            }
        }

        update_screen_locked();
        pthread_mutex_unlock(&gUpdateMutex);
    }
    pthread_exit(NULL);
    return NULL;
}

// handle the user input events (mainly the touch events) inside the ui handler
static int ui_handle_mouse_input(struct input_event* ev)
{
  // ui move menu
  if (show_menu && show_text) {
    if (grabPos.pressure) {
      if (grabPos.y<menu_max_height) {
        if (menu_start_pos == INT_MAX) {
          menu_start_pos = menu_pos;
          int nsel = (-menu_pos+grabPos.y)/MENU_ELEMENT_HEIGHT;
          if ((nsel>=0) && (nsel<menu_items)) { int opos = menu_pos; int s = ui_menu_select(nsel); menu_pos = opos; if (s==nsel) menu_sel_mouse_valid = 1; } else { menu_sel_mouse_valid = 0; }
        } else {
          int old_pos = menu_pos;
          menu_pos = menu_start_pos - (grabPos.y - mousePos[0].y);
          menu_velocity = (menu_pos - old_pos)*1.6;
        }
      } else {
        menu_sel_mouse_valid = 0;
      }
    }
    if (ev->code == BTN_MOUSE) {
      if (mousePos[0].x>gr_fb_width()-60) {
        struct menuElement el;
        ui_get_selected_ext(&el);
        if (el.help) {
          ui_print("\n%s\n",el.help);
          ev->code = BTN_GEAR_UP;
        }
      }
    }
  }
  if (enable_secret_screen) {
    if (grabPos.pressure) {
      int width = gr_fb_width()/4;
      int height = (gr_fb_height()-50)/6;
      char* c = strrchr(console_cmd,'_');
      if (c == NULL) {
        c = console_cmd;
      } else {
        c = c + 1;
      }
      char s;
      int x,y;
      for (x=0; x<4; x++) {
        for (y=0; y<6; y++) {
          s = 65+(y*4)+x;
          if (abs(mousePos[0].x-(x*width+width/2))<30 && abs(mousePos[0].y-(y*height+height/2))<30) {
            if (!strrchr(c,s)) {
              int len = strlen(console_cmd);
              if (len<255) {
                console_cmd[len] = s;
                console_cmd[len+1] = '\0';
              }
            }
          }
        }
      }
    } else if (ev->code == BTN_GEAR_UP || ev->code == BTN_MOUSE) {
      int len = strlen(console_cmd);
      if (len<255 && len>0 && console_cmd[len-1]!='_') {
        console_cmd[len] = '_';
        console_cmd[len+1] = '\0';
      }
    }
  }
  if (enable_console_screen) {
    if (ev->code == BTN_MOUSE) {
      pthread_mutex_lock(&gUpdateMutex);
      int height = gr_fb_height() - CHAR_HEIGHT*14;
      int width;
      int i,i2,sl;
      char* str;
      for (i=0;i<4;i++) {
        str = keyboard_line[keyboard_page][i];
        sl = strlen(str);
        width = gr_fb_width()/sl;
        for (i2=0; i2<sl; i2++) {
          if ((mousePos[0].x>i2*width) && (mousePos[0].x<(i2+1)*width) &&
             (mousePos[0].y>height-CHAR_HEIGHT*2) && (mousePos[0].y<height+CHAR_HEIGHT*2)) {
            switch(str[i2]) {
              case KEYBOARD_CHAR_SHIFT:
                if (keyboard_page==KEYBOARD_PAGE_QWE) keyboard_page=KEYBOARD_PAGE_qwe;
                else if (keyboard_page==KEYBOARD_PAGE_qwe) keyboard_page=KEYBOARD_PAGE_QWE;
                break;
              case KEYBOARD_CHAR_DEL:
                if (console_length>0) console_length--;
                console_cmd[console_length] = '\0';
                break;
              case KEYBOARD_CHAR_SWITCH:
                keyboard_page++;
                if (keyboard_page==KEYBOARD_PAGE_QWE) keyboard_page++;
                if (keyboard_page>=NUM_KEYBOARD_PAGES) keyboard_page=0;
                break;
              default:
                if (keyboard_page!=KEYBOARD_PAGE_123) keyboard_page = KEYBOARD_PAGE_qwe;
                console_cmd[console_length] = str[i2];
                console_length++;
                console_cmd[console_length] = '\0';
            }
          }
        }
        height += CHAR_HEIGHT*4;
      }
      pthread_mutex_unlock(&gUpdateMutex);
    }
  }
  if (!show_text) {
    if (ev->code == BTN_MOUSE) {
      pthread_mutex_lock(&gUpdateMutex);
      if (mousePos[0].y<50) {
        current_page = mousePos[0].x/(gr_fb_width()/NUM_TEXTCONTAINERS);
        if (current_page>=NUM_TEXTCONTAINERS) current_page=NUM_TEXTCONTAINERS-1;
        if (current_page<0) current_page=0;
        activeLog = &logs[current_page];
      }
      pthread_mutex_unlock(&gUpdateMutex);
      ev->code = BTN_GEAR_UP; // hide it from the main stuff
    }
  }
  return 1;
}

// Reads input events, handles special hot keys, and adds to the key queue.
static void *input_thread(void *cookie)
{
    int rel_sum_x = 0;
    int rel_sum_y = 0;
    int fake_key = 0;
    int got_data = 0;
    while (pt_input_thread_active) {
        // wait for the next key event
        struct input_event ev;
        do {
          do {
            got_data = ev_get(&ev, 1000/PROGRESSBAR_INDETERMINATE_FPS);
            if (!pt_input_thread_active) {
              pthread_exit(NULL);
              return NULL;
            }
          } while (got_data==-1);

            if (ev.type == EV_SYN) {
                // end of a multitouch point
                if (ev.code == SYN_MT_REPORT) {
                  if (actPos.num>=0 && actPos.num<MAX_MT_POINTS) {
                    // create a fake keyboard event. We will use BTN_WHEEL, BTN_GEAR_DOWN and BTN_GEAR_UP key events to fake
                    // TOUCH_MOVE, TOUCH_DOWN and TOUCH_UP in this order
                    int type = BTN_WHEEL;
                    // new and old pressure state are not consistent --> we have touch down or up event
                    if ((mousePos[actPos.num].pressure!=0) != (actPos.pressure!=0)) {
                      if (actPos.pressure == 0) {
                        type = BTN_GEAR_UP;
                        if (actPos.num==0) {
                          if (mousePos[0].length<15) {
                            // consider this a mouse click
                            type = BTN_MOUSE;
                          }
                          memset(&grabPos,0,sizeof(grabPos));
                        }
                      } else if (actPos.pressure != 0) {
                        type == BTN_GEAR_DOWN;
                        if (actPos.num==0) {
                          grabPos = actPos;
                        }
                      }
                    }
                    fake_key = 1;
                    ev.type = EV_KEY;
                    ev.code = type;
                    ev.value = actPos.num+1;

                    // this should be locked, but that causes ui events to get dropped, as the screen drawing takes too much time
                    // this should be solved by making the critical section inside the drawing much much smaller
                    if (actPos.pressure) {
                      if (mousePos[actPos.num].pressure) {
                        actPos.length = mousePos[actPos.num].length + abs(mousePos[actPos.num].x-actPos.x) + abs(mousePos[actPos.num].y-actPos.y);
                      } else {
                        actPos.length = 0;
                      }
                    } else {
                      actPos.length = 0;
                    }
                    oldMousePos[actPos.num] = mousePos[actPos.num];
                    mousePos[actPos.num] = actPos;
                    ui_handle_mouse_input(&ev);
                  }

                  memset(&actPos,0,sizeof(actPos));
                } else {
                  continue;
                }
            } else if (ev.type == EV_ABS) {
              // multitouch records are sent as ABS events. Well at least on the SGS-i9000
              if (ev.code == ABS_MT_POSITION_X) {
                actPos.x = MT_X(ev.value);
              } else if (ev.code == ABS_MT_POSITION_Y) {
                actPos.y = MT_Y(ev.value);
              } else if (ev.code == ABS_MT_TOUCH_MAJOR) {
                actPos.pressure = ev.value; // on SGS-i9000 this is 0 for not-pressed and 40 for pressed
              } else if (ev.code == ABS_MT_WIDTH_MAJOR) {
                // num is stored inside the high byte of width. Well at least on SGS-i9000
                if (actPos.num==0) {
                  // only update if it was not already set. On a normal device MT_TRACKING_ID is sent
                  actPos.num = ev.value >> 8;
                }
                actPos.size = ev.value & 0xFF;
              } else if (ev.code == ABS_MT_TRACKING_ID) {
                // on a normal device, the num is got from this value
                actPos.num = ev.value;
              }
            } else if (ev.type == EV_REL) {
                // accumulate the up or down motion reported by
                // the trackball.  When it exceeds a threshold
                // (positive or negative), fake an up/down
                // key event.
                if (ev.code == REL_Y) {
                    rel_sum_y += ev.value;
                    if (rel_sum_y > 3) { fake_key = 1; ev.type = EV_KEY; ev.code = KEY_DOWN; ev.value = 1; rel_sum_y = 0;
                    } else if (rel_sum_y < -3) { fake_key = 1; ev.type = EV_KEY; ev.code = KEY_UP; ev.value = 1; rel_sum_y = 0;
                    }
                }
                // do the same for the X axis
                if (ev.code == REL_X) {
                    rel_sum_x += ev.value;
                    if (rel_sum_x > 3) { fake_key = 1; ev.type = EV_KEY; ev.code = KEY_RIGHT; ev.value = 1; rel_sum_x = 0;
                    } else if (rel_sum_x < -3) { fake_key = 1; ev.type = EV_KEY; ev.code = KEY_LEFT; ev.value = 1; rel_sum_x = 0;
                    }
                }
            } else {
                rel_sum_y = 0;
                rel_sum_x = 0;
            }
        } while (ev.type != EV_KEY || ev.code > KEY_MAX);

        pthread_mutex_lock(&key_queue_mutex);
        if (!fake_key) {
            // our "fake" keys only report a key-down event (no
            // key-up), so don't record them in the key_pressed
            // table.
            key_pressed[ev.code] = ev.value;
        }
        fake_key = 0;
        const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
        if (ev.value > 0 && key_queue_len < queue_max) {
          // we don't want to pollute the queue with mouse move events
          if (ev.code!=BTN_WHEEL || key_queue_len==0 || key_queue[key_queue_len-1]!=BTN_WHEEL) {
            key_queue[key_queue_len++] = ev.code;
          }
          pthread_cond_signal(&key_queue_cond);
        }
        pthread_mutex_unlock(&key_queue_mutex);

        if (ev.value > 0 && device_toggle_display(key_pressed, ev.code)) {
            pthread_mutex_lock(&gUpdateMutex);
            show_text = !show_text;
            if (show_text) activeLog = &logs[TEXTCONTAINER_MAIN];
//            update_screen_locked();
            pthread_mutex_unlock(&gUpdateMutex);
        }

        if (ev.value > 0 && device_reboot_now(key_pressed, ev.code)) {
            reboot(RB_AUTOBOOT);
        }
    }
    pthread_exit(NULL);
    return NULL;
}

// initialize ui
void ui_init(void)
{
    int i;
    gBackgroundSave = NULL;
    ui_has_initialized = 1;
    gr_init();
    ev_init();


    for (i=0; i< NUM_TEXTCONTAINERS; i++) {
      memset(&logs[i],0,sizeof(logs[i]));
      logs[i].text_col = logs[i].text_row = 0;
      logs[i].text_rows = gr_fb_height() / CHAR_HEIGHT;
      if (logs[i].text_rows > MAX_ROWS) logs[i].text_rows = MAX_ROWS;
      logs[i].text_top = 1;
      logs[i].text_cols = gr_fb_width() / CHAR_WIDTH;
      if (logs[i].text_cols > MAX_COLS - 1) logs[i].text_cols = MAX_COLS - 1;
    }

    for (i = 0; BITMAPS[i].name != NULL; ++i) {
        int result = res_create_surface(BITMAPS[i].name, BITMAPS[i].surface);
        if (result < 0) {
            if (result == -2) {
                LOGI("Bitmap %s missing header\n", BITMAPS[i].name);
            } else {
                LOGE("Missing bitmap %s\n(Code %d)\n", BITMAPS[i].name, result);
            }
            *BITMAPS[i].surface = NULL;
        }
    }

    memset(&actPos, 0, sizeof(actPos));
    memset(&grabPos, 0, sizeof(grabPos));
    memset(mousePos, 0, sizeof(mousePos));
    memset(oldMousePos, 0, sizeof(oldMousePos));

    pt_ui_thread_active = 1;
    pt_input_thread_active = 1;
    pt_logreaders_active = 1;
    activeLog = &logs[TEXTCONTAINER_MAIN];

    pthread_create(&pt_ui_thread, NULL, ui_thread, NULL);
    pthread_create(&pt_input_thread, NULL, input_thread, NULL);

    for (i=1; i<NUM_TEXTCONTAINERS; i++) {
      pthread_create(&logreaders[i-1],NULL, log_reader, (void*)i);
    }

    ui_set_background(BACKGROUND_ICON_INSTALLING);
}

// shut down ui
void ui_done(void)
{
  int i;
  pt_logreaders_active = 0;
  pt_input_thread_active = 0;
  pt_ui_thread_active = 0;
  for (i=1; i<NUM_TEXTCONTAINERS; i++) {
    pthread_join(logreaders[i-1],NULL);
  }
  pthread_join(pt_input_thread,NULL);
  pthread_join(pt_ui_thread,NULL);
  draw_background_locked(gCurrentIcon);
  ev_exit();
  gr_exit();
  gr_free_surface(gBackgroundSave);
  gBackgroundSave = NULL;
  ui_has_initialized = 0;
}

char *ui_copy_image(int icon, int *width, int *height, int *bpp) {
    pthread_mutex_lock(&gUpdateMutex);
    draw_background_locked(gBackgroundIcon[icon]);
    *width = gr_fb_width();
    *height = gr_fb_height();
    *bpp = sizeof(gr_pixel) * 8;
    int size = *width * *height * sizeof(gr_pixel);
    char *ret = malloc(size);
    if (ret == NULL) {
        LOGE("Can't allocate %d bytes for image\n", size);
    } else {
        memcpy(ret, gr_fb_data(), size);
    }
    pthread_mutex_unlock(&gUpdateMutex);
    return ret;
}

void ui_set_background(int icon)
{
    pthread_mutex_lock(&gUpdateMutex);
    gCurrentIcon = gBackgroundIcon[icon];
//    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_indeterminate_progress()
{
    pthread_mutex_lock(&gUpdateMutex);
    if (gProgressBarType != PROGRESSBAR_TYPE_INDETERMINATE) {
        gProgressBarType = PROGRESSBAR_TYPE_INDETERMINATE;
//        update_progress_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_progress(float portion, int seconds)
{
    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NORMAL;
    gProgressScopeStart += gProgressScopeSize;
    gProgressScopeSize = portion;
    gProgressScopeTime = time(NULL);
    gProgressScopeDuration = seconds;
    gProgress = 0;
//    update_progress_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_set_progress(float fraction)
{
    pthread_mutex_lock(&gUpdateMutex);
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && fraction > gProgress) {
        // Skip updates that aren't visibly different.
        int width = gr_get_width(gProgressBarIndeterminate[0]);
        float scale = width * gProgressScopeSize;
        if ((int) (gProgress * scale) != (int) (fraction * scale)) {
            gProgress = fraction;
//            update_progress_locked();
        }
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_reset_progress()
{
    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NONE;
    gProgressScopeStart = gProgressScopeSize = 0;
    gProgressScopeTime = gProgressScopeDuration = 0;
    gProgress = 0;
//    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void vui_print_to(struct textContainer* tc, const char* fmt, va_list argp)
{
    char buf[513];
    vsnprintf(buf, 513, fmt, argp);

    // This can get called before ui_init(), so be careful.
    // we only want to lock if this is the active screen. Otherwise we don't care
    int waslocked = (activeLog==tc);
    if (waslocked) pthread_mutex_lock(&gUpdateMutex);
    if (tc->text_rows > 0 && tc->text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || tc->text_col >= tc->text_cols) {
                tc->text[tc->text_row][tc->text_col] = '\0';
                tc->text_col = 0;
                tc->text_row = (tc->text_row + 1) % tc->text_rows;
                if (tc->text_row == tc->text_top) tc->text_top = (tc->text_top + 1) % tc->text_rows;
            }
            if (*ptr != '\n') tc->text[tc->text_row][tc->text_col++] = *ptr;
        }
        tc->text[tc->text_row][tc->text_col] = '\0';
//        update_screen_locked();
    }
    if (waslocked) pthread_mutex_unlock(&gUpdateMutex);
}

void ui_print_to(struct textContainer* tc, const char *fmt, ...)
{
  va_list argp;
  va_start(argp, fmt);
  vui_print_to(tc,fmt, argp);
  va_end(argp);
}

void ui_print(const char *fmt, ...)
{
  va_list argp;
  va_start(argp, fmt);
  char buf[513];
  vsnprintf(buf, 513, fmt, argp);
  va_end(argp);
  fputs(buf,stderr);
  ui_print_to(&logs[TEXTCONTAINER_MAIN],"%s", buf);
}

void ui_clear_screen(struct textContainer* tc) {
    pthread_mutex_lock(&gUpdateMutex);
    memset(tc,0,sizeof(*tc));
    tc->text_col = tc->text_row = 0;
    tc->text_rows = gr_fb_height() / CHAR_HEIGHT;
    if (tc->text_rows > MAX_ROWS) tc->text_rows = MAX_ROWS;
    tc->text_top = 1;
    tc->text_cols = gr_fb_width() / CHAR_WIDTH;
    if (tc->text_cols > MAX_COLS - 1) tc->text_cols = MAX_COLS - 1;
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_clear_num_screen(int i) {
  ui_clear_screen(&logs[i]);
}

void ui_reset_text_col()
{
    pthread_mutex_lock(&gUpdateMutex);
    logs[TEXTCONTAINER_MAIN].text_col = 0;
    pthread_mutex_unlock(&gUpdateMutex);
}

#define MENU_ITEM_HEADER ""
#define MENU_ITEM_HEADER_LENGTH strlen(MENU_ITEM_HEADER)

int ui_start_menu(char** headers, char** items) {
    int i, menu_top;
    pthread_mutex_lock(&gUpdateMutex);
    if (menu_first) {
      deleteMenu(menu_first);
      free(menu_first);
    }
    menu_first = malloc(sizeof(struct menuElement));
    memset(menu_first,0,sizeof(struct menuElement));

    struct menuElement* curr = NULL;
    i = 0;
    while (headers[i]) {
      if (!curr) curr = menu_first;
      if (curr->text) {
        if (strlen(headers[i])) {
          int len = strlen(curr->text);
          curr->text = realloc(curr->text,len+1+strlen(headers[i])+1);
          strcpy(curr->text + len + 1, headers[i]);
          curr->text[len]='\n';
        }
      } else {
        curr->text = malloc(strlen(headers[i])+1);
        strcpy(curr->text,headers[i]);
      }
      curr->id = -1;
      curr->group_id = 0;
      curr->type = MENU_TYPE_GLOBAL_HEADER;
      i++;
    }

    i = i>0?1:0;
    menu_top = i;


    while(items[i-menu_top]) {
      if (curr) { curr->next = malloc(sizeof(struct menuElement));memset(curr->next,0,sizeof(struct menuElement));curr = curr->next; } else curr = menu_first;
      char *help = strrchr(items[i-menu_top],'\001');
      if (help) {
        help = help+1;
        int hlen = strlen(help);
        int tlen = strlen(items[i-menu_top]);
        curr->help = malloc(hlen+1);
        strcpy(curr->help,help);
        curr->text = malloc(tlen-hlen+1);
        strncpy(curr->text,items[i-menu_top],tlen-hlen);
        curr->text[tlen-hlen]='\0';
      } else {
        curr->text = malloc(strlen(items[i-menu_top])+1);
        strcpy(curr->text,items[i-menu_top]);
      }
      curr->id = i-menu_top;
      curr->group_id = 0;
      curr->type = MENU_TYPE_ELEMENT;
      i++;
    }

/*    if (gShowBackButton) {
        strcpy(menu[i], "+++++Go Back+++++");
        ++i;
    }*/

    menu_items = i;
    show_menu = 1;
    menu_sel = menu_top;
    menu_pos = 0;

    pthread_mutex_unlock(&gUpdateMutex);
    if (gShowBackButton) {
        return menu_items - 1;
    }
    return menu_items;
}

int ui_menu_select(int sel) {
    int old_sel,i;
    struct menuElement* act;
    pthread_mutex_lock(&gUpdateMutex);
    show_text = 1;
    show_menu = 1;
    if (show_menu > 0) {
        old_sel = menu_sel;
        menu_sel = sel;

        int direction=(old_sel<=menu_sel);
        int okay;

        // do not allow selection of invalid element
        do {
          okay = 1;
          if (menu_sel < 0) menu_sel = menu_items + menu_sel;
          if (menu_sel >= menu_items) menu_sel = menu_sel - menu_items;
          act = menu_first;
          int i=0;
          while(i!=menu_sel) {
            i++;
            act = act->next;
          }
          if (act->type==MENU_TYPE_GLOBAL_HEADER || act->type==MENU_TYPE_GROUP_HEADER) {
            if (direction) {
              menu_sel++;
            } else {
              menu_sel--;
            }
            okay = 0;
          }
        } while(!okay);

//        update_screen_locked();
        sel = menu_sel;

        int menu_height = menu_items*MENU_ELEMENT_HEIGHT;
        int menu_all_height = menu_height;
        if (menu_height>menu_max_height) {
          menu_height = menu_max_height;
        }
        menu_pos = -(sel * MENU_ELEMENT_HEIGHT - menu_height / 2 + MENU_ELEMENT_HEIGHT/2);
        if (menu_pos>0) menu_pos = 0;
        if (menu_pos<-menu_all_height+menu_height) menu_pos = -menu_all_height+menu_height;
    }
    pthread_mutex_unlock(&gUpdateMutex);
    return sel;
}

void ui_end_menu() {
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    show_menu = 0;
    if (menu_first) {
      deleteMenu(menu_first);
      free(menu_first);
    }
    menu_first = NULL;
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_start_menu_ext()
{
  pthread_mutex_lock(&gUpdateMutex);
  if (menu_first) {
    deleteMenu(menu_first);
    free(menu_first);
  }
  menu_first = NULL;
  menu_items = 0;
  menu_sel = 0;
  menu_pos = 0;
  pthread_mutex_unlock(&gUpdateMutex);
}

int ui_add_menu(int id, int group_id, int type, char* text, char* help)
{
  struct menuElement me;
  me.id = id;
  me.group_id = group_id;
  me.type = type;
  if (text) {
    me.text = malloc(strlen(text)+1);
    strcpy(me.text,text);
  } else me.text = NULL;
  if (help) {
    me.help = malloc(strlen(help)+1);
    strcpy(me.help,help);
  } else me.help = NULL;
  return ui_add_menu_ext(me);
}

int ui_add_menu_ext(struct menuElement el)
{
  pthread_mutex_lock(&gUpdateMutex);
  struct menuElement *act_menu = menu_first;
  if (act_menu) {
    while (act_menu->next) {
      act_menu = act_menu->next;
    }
    act_menu->next = malloc(sizeof(struct menuElement));
    memset(act_menu->next,0,sizeof(struct menuElement));
    *act_menu->next = el;
    act_menu->next->next = NULL;
  } else {
    menu_first = malloc(sizeof(struct menuElement));
    memset(menu_first,0,sizeof(struct menuElement));
    *menu_first = el;
    menu_first->next = NULL;
  }
  menu_items++;
  pthread_mutex_unlock(&gUpdateMutex);
  return menu_items;
}

int ui_get_selected_ext(struct menuElement* el)
{
  pthread_mutex_lock(&gUpdateMutex);
  int i=0;
  struct menuElement *act_menu = menu_first;
  while(i!=menu_sel) {
    i++;
    act_menu = act_menu->next;
  }
  pthread_mutex_unlock(&gUpdateMutex);
  *el = *act_menu;
  return i;
}

int ui_set_page(int page)
{
  pthread_mutex_lock(&gUpdateMutex);
  if (page>=NUM_TEXTCONTAINERS) page=NUM_TEXTCONTAINERS-1;
  if (page<0) page = 0;
  current_page = page;
  activeLog = &logs[current_page];
  pthread_mutex_unlock(&gUpdateMutex);
  return page;
}

static int allow_display_toggle = 1;

int get_allow_toggle_display() {
    return allow_display_toggle;
}

int
get_menu_selection_ext(int initial_selection, struct menuElement* el)
{
    allow_display_toggle = 1;
    ui_clear_key_queue();
    ui_set_showing_back_button(0);
    int selected = initial_selection;
    int chosen_item = -1;
    int wrap_count = 0;
    ui_menu_select(selected);
    while (chosen_item < 0 && chosen_item != GO_BACK) {
        int key = ui_wait_key();
        int visible = ui_text_visible();

        int action = device_handle_key(key, visible);
        if (action < 0) {
          int old_selected = selected;
          selected = ui_get_selected_ext(el);
          switch (action) {
            case HIGHLIGHT_UP:
              --selected;
              selected = ui_menu_select(selected);
              break;
            case HIGHLIGHT_DOWN:
              ++selected;
              selected = ui_menu_select(selected);
              break;
            case SELECT_ITEM_MOUSE:
              if (ui_valid_mouse_select()) chosen_item = selected;
              break;
            case SELECT_ITEM:
              chosen_item = selected; //-1 for the header
              break;
            case NO_ACTION:
              break;
            case GO_BACK:
              chosen_item = GO_BACK;
              break;
          }
        } else {
          chosen_item = action;
        }
    }
    ui_clear_key_queue();
    allow_display_toggle = 0;
    ui_set_show_text(0);
    return chosen_item;
}

int
get_menu_selection(char** headers, char** items, int menu_only) {
    // throw away keys pressed previously, so user doesn't
    // accidentally trigger menu items.
    allow_display_toggle = 1;
    ui_clear_key_queue();

    ui_set_showing_back_button(0);

    int item_count = ui_start_menu(headers, items);
    int selected = 0;
    int chosen_item = -1;

    // Some users with dead enter keys need a way to turn on power to select.
    // Jiggering across the wrapping menu is one "secret" way to enable it.
    // We can't rely on /cache or /sdcard since they may not be available.
    int wrap_count = 0;
    ui_menu_select(1);
    while (chosen_item < 0 && chosen_item != GO_BACK) {
        int key = ui_wait_key();
        int visible = ui_text_visible();

        int action = device_handle_key(key, visible);
        if (action < 0) {
          int old_selected = selected;
          selected = ui_get_selected();
          switch (action) {
            case HIGHLIGHT_UP:
              --selected;
              selected = ui_menu_select(selected);
              break;
            case HIGHLIGHT_DOWN:
              ++selected;
              selected = ui_menu_select(selected);
              break;
            case SELECT_ITEM_MOUSE:
              if (ui_valid_mouse_select()) chosen_item = selected-1;
              break;
            case SELECT_ITEM:
              chosen_item = selected-1; //-1 for the header
              break;
            case NO_ACTION:
              break;
            case GO_BACK:
              chosen_item = GO_BACK;
              break;
          }
        } else if (!menu_only) {
          chosen_item = action;
        }
    }
    ui_end_menu();
    ui_clear_key_queue();
    allow_display_toggle = 0;
    ui_set_show_text(0);
    return chosen_item;
}


int ui_text_visible()
{
    pthread_mutex_lock(&gUpdateMutex);
    int visible = show_text;
    pthread_mutex_unlock(&gUpdateMutex);
    return visible;
}

int ui_wait_key()
{
    pthread_mutex_lock(&key_queue_mutex);
    while (key_queue_len == 0) {
        pthread_cond_wait(&key_queue_cond, &key_queue_mutex);
    }
    int key = key_queue[0];
    memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
    pthread_mutex_unlock(&key_queue_mutex);
    return key;
}

int ui_key_pressed(int key)
{
    // This is a volatile static array, don't bother locking
    return key_pressed[key];
}

void ui_clear_key_queue() {
    pthread_mutex_lock(&key_queue_mutex);
    key_queue_len = 0;
    pthread_mutex_unlock(&key_queue_mutex);
}

void ui_set_show_text(int value) {
    pthread_mutex_lock(&gUpdateMutex);
    show_text = value;
    if (show_text) activeLog = &logs[TEXTCONTAINER_MAIN];
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_set_mouse_test(int value) {
    enable_mouse_test = value;
}

void ui_set_console(int value) {
    enable_console_screen = value;
}

void set_console_cmd(const char* str) {
    strcpy(console_cmd,str);
    console_length = strlen(str);
}

const char* get_console_cmd() {
    return console_cmd;
}

void ui_set_showing_back_button(int showBackButton) {
    gShowBackButton = showBackButton;
}

int ui_get_showing_back_button() {
    return gShowBackButton;
}

int ui_get_selected() {
    return menu_sel;
}

int ui_valid_mouse_select() {
    return menu_sel_mouse_valid && show_text;
}

void ui_set_secret_screen(int val) {
  enable_secret_screen = val;
}

int get_ui_state() {
  return ui_has_initialized;
}
