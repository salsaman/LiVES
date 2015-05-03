if ENABLE_OSC

if IS_MINGW
mingw_CFLAGS = -DIS_MINGW=1 -I/usr/i686-pc-mingw32/sys-root/mingw/include
mingw_LIBFLAGS = -lws2_32
endif

sendOSC = sendOSC

LIBOSCDIR = ../client

sendOSCdir = $(bindir)

sendOSC_LDFLAGS = $(mingw_LIBFLAGS) $(top_builddir)/libOSC/client/.libs/libOSC_client.a

sendOSC_CFLAGS = -I.. $(mingw_CFLAGS)

sendOSC_SOURCES = htmsocket.c htmsocket.h sendOSC.c

sendOSC_PROGRAMS = sendOSC

installdirs:
	$(MKDIR_P) "$(DESTDIR)$(bindir)"

install-exec-local:
	cp -f $(sendOSC) "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/$(sendOSC)"


endif
