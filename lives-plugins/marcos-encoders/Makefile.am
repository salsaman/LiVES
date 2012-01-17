## Process this with automake to create Makefile.in
#

marcosencodersdir = $(bindir)
marcosencoders_DATA = lives_dirac_encoder \
lives_avi_encoder \
lives_gif_encoder \
lives_mkv_encoder \
lives_mng_encoder \
lives_mpeg_encoder \
lives_ogm_encoder \
lives_theora_encoder \
lives_dirac_encoder3 \
lives_avi_encoder3 \
lives_gif_encoder3 \
lives_mkv_encoder3 \
lives_mng_encoder3 \
lives_mpeg_encoder3 \
lives_ogm_encoder3 \
lives_theora_encoder3

install-data-hook:
	for encoder in $(marcosencoders_DATA) ; do chmod a+rx "$(DESTDIR)$(marcosencodersdir)/$${encoder}" ; done

EXTRA_DIST = $(marcosencoders_DATA) README.multi_encoder
