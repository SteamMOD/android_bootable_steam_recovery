#ifndef __STEAM_OEM_H
#define __STEAM_OEM_H

// starts post-install (afterburner) config menu.
int do_steam_afterburner(int mountopt);

// starts installation
int do_steam_install();

// starts upgrade
int do_steam_upgrade();

// starts uninstallation
int do_steam_uninstall();

// starts recovery chooser if avialable
int do_steam_install_fromcache();

#endif
