DEBUG_CFLAGS = 

SUBDIRS = . client sendOSC

OSC_LIB_FILE = libOSC.la

lib_LTLIBRARIES = $(OSC_LIB_FILE)

if IS_SOLARIS
solaris_CFLAGS=-DIS_SOLARIS=1
endif

if IS_MINGW
mingw_CFLAGS=-DIS_MINGW=1
mingw_LIBFLAGS = -lwsock32 -lws2_32
AM_CPPFLAGS = -I "$(top_srcdir)" -I "$(top_srcdir)/libOSC"
else
AM_CPPFLAGS = -I "$(top_srcdir)" -I "$(top_srcdir)/libOSC" -I "$(includedir)"
endif

libOSC_la_CFLAGS = $(DEBUG_CFLAGS) $(solaris_CFLAGS) $(mingw_CFLAGS)

AM_CPPFLAGS += -fPIC -DPIC
AM_LDFLAGS = -fPIC -DPIC

if ENABLE_OSC
libOSC_la_SOURCES = OSC-address-space.c OSC-address-space.h\
		 OSC-callbacklist.c OSC-callbacklist.h\
		 OSC-drop.c OSC-drop.h\
		 OSC-pattern-match.c OSC-pattern-match.h\
		 OSC-priority-queue.c OSC-priority-queue.h\
		 OSC-receive.c OSC-receive.h\
		 OSC-string-help.c OSC-string-help.h\
		 OSC-common.c OSC-common.h\
		 OSC-timetag.c OSC-timetag.h\
		 NetworkReturnAddress.c NetworkReturnAddress.h \
		 libosc.h \
		 OSC-client.h \
		 OSC-internal-messages.h \
		 NetworkUDP.h \
		 OSC-struct.h \
		 OSC-dispatch.h
endif

libOSC_la_LDFLAGS = $(ALL_LIB_OPTS) $(mingw_LIBFLAGS) -O2

