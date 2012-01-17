## Process this with automake to create Makefile.in
#
#ls -1 | grep -v Makefile.am  | sed "s/$/ \\\\/g" >> Makefile.am

install-exec-am: 
	"$(top_srcdir)/mkinstalldirs" \
	"$(DESTDIR)$(libdir)/$(PACKAGE)/plugins/effects/rendered"

RFXscriptsdir = "$(datadir)/$(PACKAGE)/plugins/effects/RFXscripts"
RFXscripts_DATA = $(srcdir)/*.script

EXTRA_DIST = $(RFXscripts_DATA)
