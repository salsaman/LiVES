// messaging.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _MESSAGING_H_
#define _MESSAGING_H_

// message collection
void d_print(const char *fmt, ...);
char *dump_messages(int start, int end); // utils.c
weed_plant_t *get_nth_info_message(int n); // utils.c
int add_messages_to_list(const char *text);
int free_n_msgs(int frval);

FILE *open_logfile(const char *logfilename);
void close_logfile(FILE *logfile);

// d_print shortcuts
void d_print_cancelled(void);
void d_print_failed(void);
void d_print_done(void);
void d_print_enough(int frames);
void d_print_file_error_failed(void);

boolean d_print_urgency(double timeout_seconds, const char *fmt, ...);
boolean d_print_overlay(double timeout_seconds, const char *fmt, ...);

#endif

