## Process this with automake to create Makefile.in
#

compoundscriptsdir = "$(datadir)/$(PACKAGE)/plugins/effects/compound"
compoundscripts_DATA = image_stabilizer motion_analyser neural_net

EXTRA_DIST = $(compoundscripts_DATA) data icons

install-data-local:
	cp -rf icons/ "$(DESTDIR)$(compoundscriptsdir)"
	$(MAKE) -i install-data-local-remove-vcs-files

install-data-local-remove-vcs-files:
	rm -rf "$(DESTDIR)$(compoundscriptsdir)/icons/".*
	rm -rf "$(DESTDIR)$(compoundscriptsdir)/data/".*

