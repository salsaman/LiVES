DEBUG_CFLAGS = 

OSC_CLIENT_LIB_FILE = libOSC_client.la

ALL_LIB_OPTS = -export-dynamic

noinst_LTLIBRARIES = $(OSC_CLIENT_LIB_FILE)

libOSC_client_la_CFLAGS = -I "$(srcdir)/.." $(DEBUG_CFLAGS)

if ENABLE_OSC
libOSC_client_la_SOURCES = OSC-client.c OSC-timetag.c
endif

libOSC_client_la_LDFLAGS = $(ALL_LIB_OPTS) -DDYNAMIC -O2 -Wall -rdynamic

