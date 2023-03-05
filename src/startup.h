// startup.h.
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// startup functionss -- the functions that were previoulsy in this file have been moved to setup,

#ifndef _HAS_STARTUP_H
#define _HAS_STARTUP_H

int run_the_program(int argc, char *argv[], pthread_t *gtk_thread, ulong id);

void startup_message_fatal(char *msg) LIVES_NORETURN;

boolean startup_message_choice(const char *msg, int msgtype);

boolean startup_message_nonfatal(const char *msg);

boolean startup_message_info(const char *msg);

boolean startup_message_nonfatal_dismissable(const char *msg, uint64_t warning_mask);

void print_opthelp(LiVESTextBuffer *, const char *extracmds_file1, const char *extracmds_file2);

capabilities *get_capabilities(void);

// checking for executables
void get_location(const char *exe, char *val, int maxlen);
boolean check_for_executable(lives_checkstatus_t *cap, const char *exec);

// pacjage mgmt

char *get_install_cmd(const char *distro, const char *exe);
char *get_install_lib_cmd(const char *distro, const char *libname);

boolean check_snap(const char *prog);

boolean lazy_startup_checks(void);
//
void replace_with_delegates(void);

#endif
