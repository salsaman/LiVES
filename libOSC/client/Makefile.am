DEBUG_CFLAGS = 

if IS_MINGW
mingw_CFLAGS = -DIS_MINGW=1 -I/usr/i686-pc-mingw32/sys-root/mingw/include
mingw_LDFLAGS = -lws2_32
endif

OSC_CLIENT_LIB_FILE = libOSC_client.la

lib_LTLIBRARIES = $(OSC_CLIENT_LIB_FILE)

libOSC_client_la_CFLAGS = -I "$(srcdir)/.." $(DEBUG_CFLAGS) $(mingw_CFLAGS) -fPIC -DPIC

if ENABLE_OSC
libOSC_client_la_SOURCES = OSC-client.c OSC-timetag.c
endif

libOSC_client_la_LDFLAGS = $(ALL_LIB_OPTS) -O2 -Wall $(mingw_LDFLAGS) -fPIC -DPIC

