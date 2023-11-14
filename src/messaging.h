// messaging.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _MESSAGING_H_
#define _MESSAGING_H_

#include "support.h"

#define LIVES_STDERR stderr
#define LIVES_STDOUT stdout
#define LIVES_STDIN stdin

#define lives_fprintf(stream, fmt, ...) fprintf(stream, fmt, __VA_ARGS__)
#define _lives_printerrX(fmt, ...) lives_fprintf(LIVES_STDERR, fmt, __VA_ARGS__)
#define _lives_printerr(fmt, ...) _lives_printerrX(fmt "%s", __VA_ARGS__)

// append an extra %s and corresponding "". This prevents warnings if fmt is just a plain string.
#define lives_printerr(...) _lives_printerr(__VA_ARGS__, "")

// print with translation
#define _lives_print(fmt, ...) _DW0(char *tmp = lives_strdup_printf("%s%%s", _(fmt)), \
				    *tmp2 = lives_strdup_printf(tmp, __VA_ARGS_ ); \
				    _lives_printerrX(tmp2, __VA_ARGS__); \
				    lives_free(tmp); lives_free(tmp2);)

#define lives_print(...) _lives_print(__VA_ARGS__, "")

#define d_printerr(...) _lives_printerr(__VA_ARGS__)

void _cache_msg(const char *fmt, ...);

#define Xcache_msg(fmt, ...) _DW0(char *tmp = lives_strdup_printf("%s%%s", (fmt)), \
				  *tmp2 = lives_strdup_printf(tmp, __VA_ARGS__);	\
				  _cache_msg(tmp2, __VA_ARGS__); \
				  lives_free(tmp); lives_free(tmp2);)

#define cache_msg(...) Xcache_msg(__VA_ARGS__, "")

#define LIVES_LEAF_MESSAGE_STRING "msg_string"

// flagbits for prefs->msg_routing

#define MSG_ROUTE_UNDEFINED			0
#define MSG_ROUTE_CACHE				(1 << 0)
#define MSG_ROUTE_STORE				(1 << 1)
#define MSG_ROUTE_STDERR			(1 << 2)
#define MSG_ROUTE_LOGFILE			(1 << 3)
#define MSG_ROUTE_SOCKET			(1 << 4)

#define MSG_ROUTE_DISPLAY			(1 << 8)
#define MSG_ROUTE_FANCY				(1 << 9)

#define MSG_ROUTE_BLOCK				(1 << 16)
#define MSG_ROUTE_DEBUG				(1 << 17)

#define MSGVAR_LOCAL THREADVAR(msgmode)
#define MSGVAR_GLOBAL prefs->msg_routing
#define MSGVAR_POINTER THREADVAR(pmsgmode)

#define MSGMODE_LOCAL _DW0(int *iptr = &MSGVAR_LOCAL; *iptr = MSGVAR_GLOBAL; MSGVAR_POINTER = iptr;)

#define MSGMODE_GLOBAL MSGVAR_POINTER = &MSGVAR_GLOBAL

#define MSGMODE_SOCKET(var)		_DW0((var) = MSG_ROUTE_SOCKET;)
#define MSGMODE_NOSTORE(var)		_DW0((var) &= ~(MSG_ROUTE_CACHE | MSG_ROUTE_STORE);)
#define MSGMODE_CONSOLE(var)		_DW0(MSGMODE_NOSTORE(var); MSGMODE_SET(STDERR);)
#define MSGMODE_NODISPLAY(var)		_DW0((var) &= ~(MSG_ROUTE_DISPLAY);)
#define MSGMODE_NODISPLAY_NOSTORE(var)	_DW0(MSGMODE_NOSTORE(var); MSGMODE_NODISPLAY(var);)
#define MSGMODE_DEBUG_LOG(var)		_DW0((var) |= (MSG_ROUTE_STDERR | MSG_ROUTE_LOGFILE);)

#define _MSGMODE_SET(var, VAL) _DW0(MSGMODE_##VAL(var);)
#define MSGMODE_INIT(var) _DW0((var) = MSG_ROUTE_CACHE; if (mainw->debug) _MSGMODE_SET(var, DEBUG_LOG);)
#define MSGMODE_SET(VAL) _DW0(MSGMODE_##VAL(*MSGVAR_POINTER);)

#define MSGMODE_HAS(VAL) ((*MSGVAR_POINTER & MSG_ROUTE_##VAL) == MSG_ROUTE_##VAL)

#define MSGMODE_ON(VAL) _DW0(*MSGVAR_POINTER |= MSG_ROUTE_##VAL;)
#define MSGMODE_OFF(VAL) _DW0(*MSGVAR_POINTER &= ~MSG_ROUTE_##VAL;)

void d_print_utility(const char *);

#define d_print_cancelled()						\
  _DW0(d_print_utility(_("cancelled.\n")); lives_notify(LIVES_OSC_NOTIFY_CANCELLED, "");)

#define d_print_failed() \
  _DW0(d_print_utility(_("failed.\n")); lives_notify(LIVES_OSC_NOTIFY_FAILED, "");)

// message collection
boolean d_print(const char *fmt, ...);
char *_dump_messages(int start, int end);

void dump_messages(FILE *stream);

weed_plant_t *get_nth_info_message(int n);
weed_error_t add_message_to_list(const char *text);
weed_error_t add_message_first(const char *text);
weed_error_t free_n_msgs(int frval);

#define msg_print(...) d_print(__VA_ARGS__)

FILE *open_logfile(const char *logfilename);
void close_logfile(FILE *logfile);

void d_print_debug(const char *fmt, ...);

// d_print shortcuts
void d_print_done(void);
void d_print_enough(int frames);
void d_print_file_error_failed(void);

boolean d_print_urgency(double timeout_seconds, const char *fmt, ...);
boolean d_print_overlay(double timeout_seconds, const char *fmt, ...);

#endif

