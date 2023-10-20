// messaging.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _MESSAGING_H_
#define _MESSAGING_H_

#define LIVES_LEAF_MESSAGE_STRING "msg_string"

// flagbits for prefs->msg_routing
#define MSG_ROUTE_CACHE				0

#define MSG_ROUTE_STORE				(1 << 0)
#define MSG_ROUTE_DISPLAY			(1 << 1)
#define MSG_ROUTE_FANCY				(1 << 2)
#define MSG_ROUTE_STDERR			(1 << 3)
#define MSG_ROUTE_LOGFILE			(1 << 4)

#define MSG_ROUTE_BLOCKED			(1 << 8)
#define MSG_ROUTE_DEBUG_LOG			(1 << 9)

// message collection
void d_print(const char *fmt, ...);
char *_dump_messages(int start, int end);

void dump_messages(FILE *stream);

weed_plant_t *get_nth_info_message(int n);
weed_error_t add_message_to_list(const char *text);
weed_error_t add_message_first(const char *text);
weed_error_t free_n_msgs(int frval);

#define msg_print(...) d_print(__VA_ARGS__)

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

