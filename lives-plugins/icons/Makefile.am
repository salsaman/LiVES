## Process this with automake to create Makefile.in
# some icons by Andrian Valeanu under cc3.0 [loop.png, lock.png, unlock.png]
# https://creativecommons.org/licenses/by/3.0/legalcode


install-exec-am:
	/bin/rm -rf "$(DESTDIR)$(datadir)/$(PACKAGE)/"*

iconsdir = "$(datadir)/$(PACKAGE)/icons"
icons_DATA = \
audio.png \
fullscreen.png\
loop.png \
playsel.png \
sepwin.png \
volume_mute.png \
zoom-in.png \
zoom-out.png \
locked.png \
unlocked.png \
pref_decoding.png \
pref_directory.png \
pref_effects.png \
pref_encoding.png \
pref_gui.png \
pref_jack.png \
pref_midi.png \
pref_misc.png \
pref_net.png \
pref_playback.png \
pref_record.png \
pref_themes.png \
pref_warning.png \
pref_multitrack.png

EXTRA_DIST = $(icons_DATA)
