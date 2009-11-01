## Process this with automake to create Makefile.in
#

marcosencodersdir = $(bindir)
marcosencoders_DATA = dirac_encoder.py \
avi_encoder.py \
gif_encoder.py \
mkv_encoder.py \
mng_encoder.py \
mpeg_encoder.py \
ogm_encoder.py \
theora_encoder.py

install-data-hook:
	for encoder in $(marcosencoders_DATA) ; do chmod a+rx $(DESTDIR)$(marcosencodersdir)/$${encoder} ; done

EXTRA_DIST = $(marcosencoders_DATA) README.multi_encoder
