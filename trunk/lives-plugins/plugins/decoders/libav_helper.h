// LiVES - asf decoder plugin
// (c) G. Finch 2011 - 2014 <salsaman@gmail.com>

/*
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * LiVES is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with LiVES; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


// various workarounds to provide backwards compatibility for libav

// "If you libav developers could just stop breaking backwards compatibility..."
// "THAT'D BE GREAT"

#ifndef HAVE_LIBAV_HELPER_H
#define HAVE_LIBAV_HELPER_H

#ifdef HAVE_LIBAV_LIBS

#if (LIBAVCODEC_VERSION_MAJOR > 54)
#define CodecID AVCodecID

#define CODEC_ID_NONE AV_CODEC_ID_NONE
#define CODEC_ID_H264 AV_CODEC_ID_H264
#define CODEC_ID_H263 AV_CODEC_ID_H263
#define CODEC_ID_H263P AV_CODEC_ID_H263P
#define CODEC_ID_H263I AV_CODEC_ID_H263I
#define CODEC_ID_H261 AV_CODEC_ID_H261
#define CODEC_ID_MPEG4 AV_CODEC_ID_MPEG4
#define CODEC_ID_MSMPEG4V3 AV_CODEC_ID_MSMPEG4V3
#define CODEC_ID_MSMPEG4V2 AV_CODEC_ID_MSMPEG4V2
#define CODEC_ID_MSMPEG4V1 AV_CODEC_ID_MSMPEG4V1
#define CODEC_ID_WMV1 AV_CODEC_ID_WMV1
#define CODEC_ID_WMV2 AV_CODEC_ID_WMV2
#define CODEC_ID_DVVIDEO AV_CODEC_ID_DVVIDEO
#define CODEC_ID_MPEG1VIDEO AV_CODEC_ID_MPEG1VIDEO
#define CODEC_ID_MPEG2VIDEO AV_CODEC_ID_MPEG2VIDEO
#define CODEC_ID_MJPEG AV_CODEC_ID_MJPEG
#define CODEC_ID_LJPEG AV_CODEC_ID_LJPEG
#define CODEC_ID_JPEGLS AV_CODEC_ID_JPEGLS
#define CODEC_ID_HUFFYUV AV_CODEC_ID_HUFFYUV
#define CODEC_ID_FFVHUFF AV_CODEC_ID_FFVHUFF
#define CODEC_ID_CYUV AV_CODEC_ID_CYUV
#define CODEC_ID_RAWVIDEO AV_CODEC_ID_RAWVIDEO
#define CODEC_ID_INDEO2 AV_CODEC_ID_INDEO2
#define CODEC_ID_INDEO3 AV_CODEC_ID_INDEO3
#define CODEC_ID_INDEO4 AV_CODEC_ID_INDEO4
#define CODEC_ID_INDEO5 AV_CODEC_ID_INDEO5
#define CODEC_ID_VP3 AV_CODEC_ID_VP3
#define CODEC_ID_VP5 AV_CODEC_ID_VP5
#define CODEC_ID_VP6 AV_CODEC_ID_VP6
#define CODEC_ID_VP6F AV_CODEC_ID_VP6F
#define CODEC_ID_ASV1 AV_CODEC_ID_ASV1
#define CODEC_ID_ASV2 AV_CODEC_ID_ASV2
#define CODEC_ID_VCR1 AV_CODEC_ID_VCR1
#define CODEC_ID_FFV1 AV_CODEC_ID_FFV1
#define CODEC_ID_XAN_WC4 AV_CODEC_ID_XAN_WC4
#define CODEC_ID_MIMIC AV_CODEC_ID_MIMIC
#define CODEC_ID_MSRLE AV_CODEC_ID_MSRLE
#define CODEC_ID_MSVIDEO1 AV_CODEC_ID_MSVIDEO1
#define CODEC_ID_CINEPAK AV_CODEC_ID_CINEPAK
#define CODEC_ID_TRUEMOTION1 AV_CODEC_ID_TRUEMOTION1
#define CODEC_ID_TRUEMOTION2 AV_CODEC_ID_TRUEMOTION2
#define CODEC_ID_MSZH AV_CODEC_ID_MSZH
#define CODEC_ID_ZLIB AV_CODEC_ID_ZLIB

#if FF_API_SNOW
#define CODEC_ID_SNOW AV_CODEC_ID_SNOW
#endif

#define CODEC_ID_4XM AV_CODEC_ID_4XM
#define CODEC_ID_FLV1 AV_CODEC_ID_FLV1
#define CODEC_ID_FLASHSV AV_CODEC_ID_FLASHSV
#define CODEC_ID_SVQ1 AV_CODEC_ID_SVQ1
#define CODEC_ID_TSCC AV_CODEC_ID_TSCC
#define CODEC_ID_ULTI AV_CODEC_ID_ULTI
#define CODEC_ID_VIXL AV_CODEC_ID_VIXL
#define CODEC_ID_QPEG AV_CODEC_ID_QPEG
#define CODEC_ID_WMV3 AV_CODEC_ID_WMV3
#define CODEC_ID_VC1 AV_CODEC_ID_VC1
#define CODEC_ID_LOCO AV_CODEC_ID_LOCO
#define CODEC_ID_WNV1 AV_CODEC_ID_WNV1
#define CODEC_ID_AASC AV_CODEC_ID_AASC
#define CODEC_ID_FRAPS AV_CODEC_ID_FRAPS
#define CODEC_ID_THEORA AV_CODEC_ID_THEORA
#define CODEC_ID_CSCD AV_CODEC_ID_CSCD
#define CODEC_ID_ZMBV AV_CODEC_ID_ZMBV
#define CODEC_ID_KMVC AV_CODEC_ID_KMVC
#define CODEC_ID_CAVS AV_CODEC_ID_CAVS
#define CODEC_ID_JPEG2000 AV_CODEC_ID_JPEG2000
#define CODEC_ID_VMNC AV_CODEC_ID_VMNC
#define CODEC_ID_TARGA AV_CODEC_ID_TARGA
#define CODEC_ID_PNG AV_CODEC_ID_PNG
#define CODEC_ID_GIF AV_CODEC_ID_GIF
#define CODEC_ID_TIFF AV_CODEC_ID_TIFF
#define CODEC_ID_CLJR AV_CODEC_ID_CLJR
#define CODEC_ID_DIRAC AV_CODEC_ID_DIRAC
#define CODEC_ID_RPZA AV_CODEC_ID_RPZA
#define CODEC_ID_SP5X AV_CODEC_ID_SP5X

#define CODEC_ID_FLASHSV2 AV_CODEC_ID_FLASHSV2
#define CODEC_ID_TEXT AV_CODEC_ID_TEXT
#define CODEC_ID_SSA AV_CODEC_ID_SSA
#define CODEC_ID_SSA AV_CODEC_ID_SRT
#define CODEC_ID_VP8 AV_CODEC_ID_VP8
#define CODEC_ID_RV10 AV_CODEC_ID_RV10
#define CODEC_ID_RV20 AV_CODEC_ID_RV20
#define CODEC_ID_RV30 AV_CODEC_ID_RV30
#define CODEC_ID_RV40 AV_CODEC_ID_RV40
#define CODEC_ID_MP3 AV_CODEC_ID_MP3
#define CODEC_ID_MP2 AV_CODEC_ID_MP2
#define CODEC_ID_AAC AV_CODEC_ID_AAC
#define CODEC_ID_PCM_BLURAY AV_CODEC_ID_PCM_BLURAY
#define CODEC_ID_AC3 AV_CODEC_ID_AC3
#define CODEC_ID_VORBIS AV_CODEC_ID_VORBIS
#define CODEC_ID_EAC3 AV_CODEC_ID_EAC3
#define CODEC_ID_DTS AV_CODEC_ID_DTS
#define CODEC_ID_TRUEHD AV_CODEC_ID_TRUEHD
#define CODEC_ID_S302M AV_CODEC_ID_S302M
#define CODEC_ID_DVB_TELETEXT AV_CODEC_ID_DVB_TELETEXT
#define CODEC_ID_DVB_SUBTITLE AV_CODEC_ID_DVB_SUBTITLE
#define CODEC_ID_DVD_SUBTITLE AV_CODEC_ID_DVD_SUBTITLE

#define CODEC_ID_MOV_TEXT AV_CODEC_ID_MOV_TEXT
#define CODEC_ID_MP4ALS AV_CODEC_ID_MP4ALS
#define CODEC_ID_QCELP AV_CODEC_ID_QCELP
#define CODEC_ID_MPEG4SYSTEMS AV_CODEC_ID_MPEG4SYSTEMS

#define CODEC_ID_MPEG2TS AV_CODEC_ID_MPEG2TS
#define CODEC_ID_AAC_LATM AV_CODEC_ID_AAC_LATM
#define CODEC_ID_HDMV_PGS_SUBTITLE AV_CODEC_ID_HDMV_PGS_SUBTITLE

#define CODEC_ID_FLAC AV_CODEC_ID_FLAC
#define CODEC_ID_MLP AV_CODEC_ID_MLP

#define CODEC_ID_PCM_F32LE AV_CODEC_ID_PCM_F32LE
#define CODEC_ID_PCM_F64LE AV_CODEC_ID_PCM_F64LE

#define CODEC_ID_PCM_S16BE AV_CODEC_ID_PCM_S16BE
#define CODEC_ID_PCM_S24BE AV_CODEC_ID_PCM_S24BE
#define CODEC_ID_PCM_S32BE AV_CODEC_ID_PCM_S32BE

#define CODEC_ID_PCM_S16LE AV_CODEC_ID_PCM_S16LE
#define CODEC_ID_PCM_S24LE AV_CODEC_ID_PCM_S24LE
#define CODEC_ID_PCM_S32LE AV_CODEC_ID_PCM_S32LE

#define CODEC_ID_PCM_U8 AV_CODEC_ID_PCM_U8

#define CODEC_ID_PCM_QDM2 AV_CODEC_ID_QDM2
#define CODEC_ID_RA_144 AV_CODEC_ID_RA_144
#define CODEC_ID_RA_288 AV_CODEC_ID_RA_288
#define CODEC_ID_ATRAC3 AV_CODEC_ID_ATRAC3
#define CODEC_ID_COOK AV_CODEC_ID_COOK
#define CODEC_ID_SIPR AV_CODEC_ID_SIPR
#define CODEC_ID_TTA AV_CODEC_ID_TTA
#define CODEC_ID_WAVPACK AV_CODEC_ID_WAVPACK

#define CODEC_ID_TTF AV_CODEC_ID_TTF

#endif

#if ((LIBAVUTIL_VERSION_MAJOR < 51) || (LIBAVUTIL_VERSION_MAJOR == 51) && (LIBAVUTIL_VERSION_MINOR < 22))
#define AV_OPT_TYPE_INT FF_OPT_TYPE_INT
#endif

#ifndef offsetof
#define offsetof(T, F) ((unsigned int)((char *)&((T *)0)->F))
#endif

#if !HAVE_AVCODEC_OPEN2
#define avcodec_open2(a, b, c) avcodec_open(a, b)
#endif

#if !HAVE_AVCODEC_ALLOC_CONTEXT3
#define avcodec_alloc_context3(a) avcodec_alloc_context()
#endif

#if !HAVE_AVFORMAT_FIND_STREAM_INFO
#define avformat_find_stream_info(a, b) av_find_stream_info(a)
#endif

#if HAVE_AVFORMAT_NEW_STREAM
#define av_new_stream(a, b) avformat_new_stream(a, NULL)
#endif

#if HAVE_AVFORMAT_CLOSE_INPUT
#define av_close_input_file(a) avformat_close_input(&a)
#endif


#if !HAVE_AV_SET_PTS_INFO
#if HAVE_AVFORMAT_INTERNAL_H

#include <avformat/internal.h>
#define av_set_pts_info(a,b,c,d) avpriv_set_pts_info(a,b,c,d)

#else
static void av_set_pts_info(AVStream *s, int pts_wrap_bits,
                            unsigned int pts_num, unsigned int pts_den) {
  AVRational new_tb;
  if (av_reduce(&new_tb.num, &new_tb.den, pts_num, pts_den, INT_MAX)) {
    if (new_tb.num != pts_num)
      av_log(NULL, AV_LOG_DEBUG, "st:%d removing common factor %d from timebase\n", s->index, pts_num/new_tb.num);
  } else
    av_log(NULL, AV_LOG_WARNING, "st:%d has too large timebase, reducing\n", s->index);

  if (new_tb.num <= 0 || new_tb.den <= 0) {
    av_log(NULL, AV_LOG_ERROR, "Ignoring attempt to set invalid timebase %d/%d for st:%d\n", new_tb.num, new_tb.den, s->index);
    return;
  }
  s->time_base = new_tb;
#ifdef HAVE_AV_CODEC_SET_PKT_TIMEBASE
  av_codec_set_pkt_timebase(s->codec, new_tb);
#endif
  s->pts_wrap_bits = pts_wrap_bits;
}

#endif
#endif


#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc  avcodec_alloc_frame
#define av_frame_free  avcodec_free_frame
#endif



#endif // HAVE_LIBAV_LIBS

#endif // HAVE_LIBAV_HELPER_H
