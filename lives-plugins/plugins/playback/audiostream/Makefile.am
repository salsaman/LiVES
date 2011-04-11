## Process this with automake to create Makefile.in
#
#ls -1 | grep -v Makefile.am  | sed "s/$/ \\\\/g" >> Makefile.am

astreamdir = $(libdir)/$(PACKAGE)/plugins/playback/audiostream
astream_DATA = pulsestreamer.pl \
	jackstreamer.pl \
	soxstreamer.pl \
	mplayerstreamer.pl


install-data-hook:
	chmod +x $(DESTDIR)$(astreamdir)/*

EXTRA_DIST = $(astream_DATA)
