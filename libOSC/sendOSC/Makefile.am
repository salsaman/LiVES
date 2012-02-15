if ENABLE_OSC

if IS_MINGW
mingw_CFLAGS = -DIS_MINGW=1
mingw_LIBFLAGS = -lws2_32
endif

sendOSC = sendOSC

LIBOSCDIR = ../client

sendOSCdir = $(bindir)

sendOSC_LDFLAGS = -L$(LIBOSCDIR) -lOSC_client $(mingw_LIBFLAGS)

sendOSC_CFLAGS = -I.. $(mingw_CFLAGS)

sendOSC_SOURCES = htmsocket.c htmsocket.h sendOSC.c

sendOSC_PROGRAMS = sendOSC

install:
	if ! test -f "$(bindir)/$(sendOSC)"; then \
		if ! test -z $(bindir); then \
			mkdir -p -- . "$(DESTDIR)$(bindir)"; \
		fi; \
		$(INSTALL_PROGRAM) $(sendOSC) "$(DESTDIR)$(bindir)"; \
	fi


endif
