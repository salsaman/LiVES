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
theora_encoder.py \
dirac_encoder3.py \
avi_encoder3.py \
gif_encoder3.py \
mkv_encoder3.py \
mng_encoder3.py \
mpeg_encoder3.py \
ogm_encoder3.py \
theora_encoder3.py

install-data-hook:
	for encoder in $(marcosencoders_DATA) ; do chmod a+rx $(DESTDIR)$(marcosencodersdir)/$${encoder} ; done

EXTRA_DIST = $(marcosencoders_DATA) README.multi_encoder
