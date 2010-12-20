#ifndef __STEAM_CONFIG_H
#define __STEAM_CONFIG_H

#define KEY_MAX_LENGTH 96
#define VALUE_MAX_LENGTH 96

#define CONFIG_SBIN "/sbin/steam.conf"
#define CONFIG_SYSTEM "/system/etc/steam.conf"

// get a value from the key store or the read-only store
int get_conf(const char* key, char* value);
// set a value inside the key store
int set_conf(const char* key, const char* value);
// gets a value from the key store or set a default value
char* get_conf_def(const char* key, char* value, const char* def);
// gets a value from the specified file
int get_conf_ro_from(const char* file, const char* key, char* value);
// gets a value from the read-only (sbin) store
int get_conf_ro(const char* key, char* value);
// inits the configuration store from the rw (system) store
int init_conf();
// gets realtime information about the environment
int get_capability(const char* key, char* value);
#endif
