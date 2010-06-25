DEBUG_CFLAGS = 

SUBDIRS = . client sendOSC

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/libOSC -I$(includedir)

OSC_LIB_FILE = libOSC.la 
ALL_LIB_OPTS = -export-dynamic

noinst_LTLIBRARIES = $(OSC_LIB_FILE)

if IS_SOLARIS
solaris_CFLAGS=-DIS_SOLARIS=1
endif

libOSC_la_CFLAGS = $(DEBUG_CFLAGS) $(solaris_CFLAGS) -Wstrict-aliasing=0

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

libOSC_la_LDFLAGS = $(ALL_LIB_OPTS) -DDYNAMIC -O2 -Wall -rdynamic

