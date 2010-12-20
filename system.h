#ifndef __LIBSTEAM_SYSTEM_H
#define __LIBSTEAM_SYSTEM_H

#include <sys/types.h>

#define POPEN_JOINSTDERR 1

int __system(const char *command);
pid_t popen3func(int *stdin_fd, int* stdout_fd, int *stderr_fd, int flags, const char * command, void (*func)(const char* command));
pid_t popen3(int *stdin_fd, int* stdout_fd, int *stderr_fd, int flags, const char * command);
int pclose3(pid_t pid, int *stdin_fd, int *stdout_fd, int *stderr_fd, int signal);
int sh(char* command);

#endif
