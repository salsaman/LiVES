## Process this with automake to create Makefile.in
#
#ls -1 | grep -v Makefile.am  | sed "s/$/ \\\\/g" >> Makefile.am

encoderdir = $(libdir)/$(PACKAGE)/plugins/encoders
encoder_DATA = encodedv_encoder \
ffmpeg_encoder \
mencoder_encoder \
mjpegtools_encoder \
multi_encoder \
multi_encoder3 \
qtutils_encoder \
sswf_encoder \
pdf_encoder \
x264_encoder

install-data-hook:
	chmod +x $(DESTDIR)$(encoderdir)/*

EXTRA_DIST = $(encoder_DATA)
