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

#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

#include "system.h"


// This was pulled from bionic: The default system command always looks
// for shell in /system/bin/sh. This is bad.

#ifdef STEAM_HAS_BUSYBOX
// if busybox is compiled in, use the base app, as that's probably avialable
#define _PATH_BSHELL "/sbin/steam"
#else
// else use a busybox in /sbin. (having busybox there usually has a higher chance than having sh there)
#define _PATH_BSHELL "/sbin/busybox"
#endif

extern char **environ;
int
__system(const char *command)
{
  pid_t pid;
    sig_t intsave, quitsave;
    sigset_t mask, omask;
    int pstat;
    char *argp[] = {"sh", "-c", NULL, NULL};

    if (!command)        /* just checking... */
        return(1);

    argp[2] = (char *)command;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &omask);
    switch (pid = vfork()) {
    case -1:            /* error */
        sigprocmask(SIG_SETMASK, &omask, NULL);
        return(-1);
    case 0:                /* child */
        sigprocmask(SIG_SETMASK, &omask, NULL);
        execve(_PATH_BSHELL, argp, environ);
    _exit(127);
  }

    intsave = (sig_t)  bsd_signal(SIGINT, SIG_IGN);
    quitsave = (sig_t) bsd_signal(SIGQUIT, SIG_IGN);
    pid = waitpid(pid, (int *)&pstat, 0);
    sigprocmask(SIG_SETMASK, &omask, NULL);
    (void)bsd_signal(SIGINT, intsave);
    (void)bsd_signal(SIGQUIT, quitsave);
    return (pid == -1 ? -1 : (WIFEXITED(pstat) ? WEXITSTATUS(pstat) : pstat));
}

// This is a popen3 like implementation using file descriptors
// if fd's are NULL they are not used
// if fd's are not NULL, and they are below 0, they will be created, and the fd will be put back
// if fd's are not NULL, and they are above 0, they will be used inside the new process, to allow piping
// flags:
//   POPEN_JOINSTDERR: joins stdout and stderr

#define POPEN_JOINSTDERR 1
static void system_call_func(const char* command)
{
  execl(_PATH_BSHELL,"sh","-c",command,(char*)NULL);
}


pid_t popen3(int *stdin_fd, int* stdout_fd, int *stderr_fd, int flags, const char * command)
{
  return popen3func(stdin_fd,stdout_fd,stderr_fd,flags,command,system_call_func);
}

pid_t popen3func(int *stdin_fd, int* stdout_fd, int *stderr_fd, int flags, const char * command, void (*func)(const char* command))
{
  pid_t pid;
  int stdin_pipe[2];
  int stdout_pipe[2];
  int stderr_pipe[2];

  if ((flags&POPEN_JOINSTDERR) && stderr_fd) {
    errno = EINVAL;
    return -1;
  }

  if (pipe(stdin_pipe)<0) {
    return -1;
  }

  if (pipe(stdout_pipe)<0) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    return -1;
  }

  if (pipe(stderr_pipe)<0) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return -1;
  }

  if (flags&POPEN_JOINSTDERR) {
    dup2(stdout_pipe[0],stderr_pipe[0]);
    dup2(stdout_pipe[1],stderr_pipe[1]);
  }
  switch (pid = fork()) {
    case -1:
      close(stdin_pipe[0]);close(stdout_pipe[0]);close(stderr_pipe[0]);
      close(stdin_pipe[1]);close(stdout_pipe[1]);close(stderr_pipe[1]);
      return -1;
    case 0:
      {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        if (stdin_fd) {
          if (*stdin_fd >= 0) {
            dup2(*stdin_fd,stdin_pipe[0]);
          }
          close(*stdin_fd);
        }
        if (stdout_fd) {
          if (*stdout_fd >= 0) {
            dup2(*stdout_fd,stdout_pipe[1]);
          }
          close(*stdout_fd);
        }
        if (stderr_fd) {
          if (*stderr_fd >= 0) {
            dup2(*stderr_fd,stderr_pipe[1]);
          }
          close(*stderr_fd);
        }
        if (stdin_pipe[0]!=STDIN_FILENO) {
          dup2(stdin_pipe[0],STDIN_FILENO);
          close(stdin_pipe[0]);
        }
        if (stdout_pipe[1]!=STDOUT_FILENO) {
          dup2(stdout_pipe[1],STDOUT_FILENO);
          close(stdout_pipe[1]);
        }
        if (stderr_pipe[1]!=STDERR_FILENO) {
          dup2(stderr_pipe[1],STDERR_FILENO);
          close(stderr_pipe[1]);
        }
        func(command);
        _exit(127);
      }
  }

  close(stdin_pipe[0]);
  close(stdout_pipe[1]);
  close(stderr_pipe[1]);
  if (stdin_fd) {
    if (*stdin_fd >= 0) {
      dup2(*stdin_fd,stdin_pipe[1]);
    }
    *stdin_fd = stdin_pipe[1];
  } else {
    close(stdin_pipe[1]);
  }
  if (stdout_fd) {
    if (*stdout_fd >= 0) {
      dup2(*stdout_fd,stdout_pipe[0]);
    }
    *stdout_fd = stdout_pipe[0];
  } else {
    close(stdout_pipe[0]);
  }
  if (stderr_fd) {
    if (*stderr_fd >= 0) {
      dup2(*stderr_fd,stderr_pipe[0]);
    }
    *stderr_fd = stderr_pipe[0];
  } else {
    close(stderr_pipe[0]);
  }

  return pid;
}

// should be called with the same values as for popen3
// signal: what signal to send to the child.
int pclose3(pid_t pid, int* stdin_fd, int* stdout_fd, int* stderr_fd, int signal)
{
  pid_t p;
  int pstat;

  if (stdin_fd) close(*stdin_fd);
  if (stdout_fd) close(*stdout_fd);
  if (stderr_fd) close(*stderr_fd);
  if (signal) {
    kill(pid,signal);
  }

  do {
    p = waitpid(pid, &pstat, 0);
  } while (p == -1 && errno == EINTR);

  return (p == -1 ? -1 : (WIFEXITED(pstat) ? WEXITSTATUS(pstat) : pstat));
}

int sh(char* command) {
  return call_busybox("sh","-c",command,NULL);
}

