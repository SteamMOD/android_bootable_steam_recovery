#include "config.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "pthread.h"
#include "sys/stat.h"

static pthread_mutex_t gMutex1 = PTHREAD_MUTEX_INITIALIZER,gMutex2 = PTHREAD_MUTEX_INITIALIZER,gMutex3 = PTHREAD_MUTEX_INITIALIZER,gWrite = PTHREAD_MUTEX_INITIALIZER,gRead = PTHREAD_MUTEX_INITIALIZER;
static int numreaders = 0, numwriters = 0;

struct confel;
struct confel {
  char key[KEY_MAX_LENGTH];
  char value[VALUE_MAX_LENGTH];
  struct confel* next;
};
static struct confel firstconf = {"","",NULL};

// thread safe reading
static int read_conf(const char* key, char* value)
{
  pthread_mutex_lock(&gMutex3);
  pthread_mutex_lock(&gRead);
  pthread_mutex_lock(&gMutex1);
  numreaders++;
  if (numreaders==1) pthread_mutex_lock(&gWrite);
  pthread_mutex_unlock(&gMutex1);
  pthread_mutex_unlock(&gRead);
  pthread_mutex_unlock(&gMutex3);

  int found = 0;
  struct confel* n = firstconf.next;
  while (n) {
    if (strcmp(key,n->key)==0) {
      if (value) {
        strcpy(value,n->value);
      }
      found = 1;
      break;
    }
    n = n->next;
  }

  pthread_mutex_lock(&gMutex1);
  numreaders--;
  if (numreaders==0) pthread_mutex_unlock(&gWrite);
  pthread_mutex_unlock(&gMutex1);

  return found;
}

// thread safe writing
static int write_conf(const char* key, const char* value)
{
  pthread_mutex_lock(&gMutex2);
  numwriters++;
  if (numwriters==1) pthread_mutex_lock(&gRead);
  pthread_mutex_unlock(&gMutex2);

  pthread_mutex_lock(&gWrite);

  int found = 0;
  struct confel* n = firstconf.next;
  struct confel* p = &firstconf;
  while (n) {
    if (strcmp(key,n->key)==0) {
      if (value) {
        strcpy(n->value,value);
        found = 1;
      } else {
        p->next = n->next;
        free(n);
      }
      break;
    }
    if (!n->next) {
      if (value) {
        n->next = malloc(sizeof(struct confel));
        n->next->next = NULL;
        strcpy(n->next->key,key);
      }
    }
    n = n->next;
    p = p->next;
  }

  struct stat s;
  if (stat(CONFIG_SYSTEM,&s)==0) {
    FILE* f = fopen(CONFIG_SYSTEM,"w+");
    if (f) {
      n = firstconf.next;
      while (n) {
        fprintf(f,"%s=%s\n",n->key,n->value);
        n = n->next;
      }
      fclose(f);
      sync();
    }
  }

  pthread_mutex_unlock(&gWrite);

  pthread_mutex_lock(&gMutex2);
  numwriters--;
  if (numwriters==0) pthread_mutex_unlock(&gRead);
  pthread_mutex_unlock(&gMutex2);

  return found;
}

int get_conf_ro_from(const char* filename, const char* key, char* value)
{
  FILE* f = fopen(filename,"r");
  int found = 0;
  char r[KEY_MAX_LENGTH+VALUE_MAX_LENGTH+3];
  if (f) {
    while (fgets(r, KEY_MAX_LENGTH+VALUE_MAX_LENGTH+3, f)) {
      if (r[0]=='#') continue;
      char* fp = strchr(r,'=');
      if (fp) {
        if (fp[strlen(fp)-1]=='\n') fp[strlen(fp)-1]='\0';
        fp[0] = '\0';
        fp++;
        if (strlen(r)<KEY_MAX_LENGTH && strlen(fp)<VALUE_MAX_LENGTH)
        if (strcmp(r,key)==0) {
          strcpy(value,fp);
          found = 1;
          break;
        }
      }
    }
    fclose(f);
  }
  return found;
}

int get_conf_ro(const char* key, char* value)
{
  return get_conf_ro_from(CONFIG_SBIN,key,value);
}

int init_conf(void)
{
  FILE* f = fopen(CONFIG_SYSTEM,"r");
  int found = 0;
  char r[KEY_MAX_LENGTH+VALUE_MAX_LENGTH+3];
  firstconf.next = NULL;
  struct confel* n = &firstconf;
  if (f) {
    while (fgets(r, KEY_MAX_LENGTH+VALUE_MAX_LENGTH+3, f)) {
      if (r[0]=='#') continue;
      char* fp = strchr(r,'=');
      if (fp) {
        if (fp[strlen(fp)-1]=='\n') fp[strlen(fp)-1]='\0';
        fp[0] = '\0';
        fp++;
        if (strlen(r)<KEY_MAX_LENGTH && strlen(fp)<VALUE_MAX_LENGTH) {
          if (!read_conf(r,NULL)) {
            n->next = malloc(sizeof(struct confel));
            n = n->next;
            n->next = NULL;
            strcpy(n->key,r);
            strcpy(n->value,fp);
            found++;
          }
        }
      }
    }
    fclose(f);
  }
  return found;
}

char* get_conf_def(const char* key, char* value, const char* def)
{
  int has = get_conf(key, value);
  if (!has && value) strcpy(value,def);
  return value;
}

int get_conf(const char* key, char* value)
{
  int r = read_conf(key, value);
  if (!r)
    return get_conf_ro(key,value);
  else
    return r;
}

int set_conf(const char* key, const char* value)
{
  return write_conf(key, value);
}

int get_capability(const char* key, char* value)
{
  if (strstr(key,"fs.support.")==key) {
    char* fsname = key+strlen("fs.support.");
    if (call_busybox("grep",fsname,"/proc/filesystems",NULL)==0) {
      if (value) {
        strcpy(value,"1");
      }
      return 1;
    }
  }
  return 0;
}
