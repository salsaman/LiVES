if ENABLE_OSC

if IS_LINUX_MUSL
musl_CFLAGS = -DIS_LINUX_MUSL=1 @TIRPC_CFLAGS@
musl_LIBFLAGS = @TIRPC_LIBS@
endif

sendOSC = sendOSC

LIBOSCDIR = ../client

sendOSCdir = $(bindir)

sendOSC_LDFLAGS = $(mingw_LIBFLAGS) $(musl_LIBFLAGS) $(top_builddir)/libOSC/client/.libs/libOSC_client.a

sendOSC_CFLAGS = -I.. $(mingw_CFLAGS) $(musl_CFLAGS)

sendOSC_SOURCES = htmsocket.c htmsocket.h sendOSC.c

sendOSC_PROGRAMS = sendOSC

install-exec-local:
	$(MKDIR_P) "$(DESTDIR)$(bindir)"
	cp -f $(sendOSC) "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/$(sendOSC)"

endif
