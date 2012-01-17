if ENABLE_OSC

sendOSC = sendOSC

LIBOSCDIR = ../client

sendOSCdir = $(bindir)

sendOSC_LDFLAGS = -L$(LIBOSCDIR) -lOSC_client

sendOSC_CFLAGS = -I..

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
