// messaging.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _MESSAGING_H_
#define _MESSAGING_H_

#include "support.h"

#define _lives_printerrX(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define _lives_printerr(fmt, ...) _lives_printerrX(fmt "%s", __VA_ARGS__)
#define lives_printerr(...) _lives_printerr(__VA_ARGS__, "")

#define lives_print(fmt, ...) _DW0(char *tmp; lives_printerr((tmp = _(fmt)), __VA_ARGS__); str_free(tmp);)

#define lives_fprintf(stream, fmt, ...) fprintf(stream, fmt, __VA_ARGS__)

#define LIVES_LEAF_MESSAGE_STRING "msg_string"

// flagbits for prefs->msg_routing

#define MSG_ROUTE_UNDEFINED			0
#define MSG_ROUTE_CACHE				(1 << 0)
#define MSG_ROUTE_STORE				(1 << 1)
#define MSG_ROUTE_STDERR			(1 << 2)
#define MSG_ROUTE_LOGFILE			(1 << 3)

#define MSG_ROUTE_DISPLAY			(1 << 8)
#define MSG_ROUTE_FANCY				(1 << 9)

#define MSG_ROUTE_BLOCK				(1 << 16)


#define MSGMODE_SAVE _DW0(if(prefs && future_prefs \
			     && future_prefs->msg_routing == MSG_ROUTE_UNDEFINED) \
			    future_prefs->msg_routing = prefs->msg_routing;)

#define MSGMODE_RESTORE _DW0(if(prefs && future_prefs			\
				&& future_prefs->msg_routing != MSG_ROUTE_UNDEFINED) { \
			       prefs->msg_routing = future_prefs->msg_routing; \
			       future_prefs->msg_routing = MSG_ROUTE_UNDEFINED;})

#define MSGMODE_INIT(pmr)		_DW0(pmr |= (MSG_ROUTE_CACHE);)

#define MSGMODE_NOSTORE(pmr)		_DW0(pmr &= ~(MSG_ROUTE_CACHE | MSG_ROUTE_STORE);)
#define MSGMODE_CONSOLE(pmr)		_DW0(MSGMODE_NOSTORE(pmr); MSGMODE_SET(STDERR);)

#define MSGMODE_NODISPLAY(pmr)		_DW0(pmr &= ~(MSG_ROUTE_DISPLAY);)

#define MSGMODE_NODISPLAY_NOSTORE(pmr)	_DW0(MSGMODE_NOSTORE(pmr); MSGMODE_NODISPLAY(pmr);)

#define MSGMODE_DEBUG_LOG(pmr)		_DW0(pmr |= (MSG_ROUTE_STDERR | MSG_ROUTE_LOGFILE);)

#define MSGMODE_SET(VAL) _DW0(if (prefs) MSGMODE_##VAL(prefs->msg_routing);)

#define MSGMODE_HAS(VAL) (prefs && (prefs->msg_routing & MSG_ROUTE_##VAL))
#define MSGMODE_HAS_NOT(VAL) (!prefs || !(prefs->msg_routing & MSG_ROUTE_##VAL))

#define MSGMODE_ON(VAL) _DW0(if (prefs) prefs->msg_routing |= MSG_ROUTE_##VAL;)
#define MSGMODE_OFF(VAL) _DW0(if (prefs) prefs->msg_routing &= ~MSG_ROUTE_##VAL;)

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

