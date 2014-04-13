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
#define CODEC_ID_WMV1 AV_CODEC_ID_WMV1
#define CODEC_ID_WMV2 AV_CODEC_ID_WMV2
#define CODEC_ID_WMV3 AV_CODEC_ID_WMV3
#define CODEC_ID_DVVIDEO AV_CODEC_ID_DVVIDEO
#define CODEC_ID_MPEG4 AV_CODEC_ID_MPEG4
#define CODEC_ID_H264 AV_CODEC_ID_H264
#define CODEC_ID_MPEG1VIDEO AV_CODEC_ID_MPEG1VIDEO
#define CODEC_ID_MPEG2VIDEO AV_CODEC_ID_MPEG2VIDEO
#define CODEC_ID_FLV1 AV_CODEC_ID_FLV1
#define CODEC_ID_FLASHSV AV_CODEC_ID_FLASHSV
#define CODEC_ID_FLASHSV2 AV_CODEC_ID_FLASHSV2
#define CODEC_ID_VP6A AV_CODEC_ID_VP6A
#define CODEC_ID_VP6F AV_CODEC_ID_VP6F
#define CODEC_ID_H264 AV_CODEC_ID_H264
#define CODEC_ID_TEXT AV_CODEC_ID_TEXT
#define CODEC_ID_SSA AV_CODEC_ID_SSA
#define CODEC_ID_VP8 AV_CODEC_ID_VP8
#define CODEC_ID_THEORA AV_CODEC_ID_THEORA
#define CODEC_ID_SNOW AV_CODEC_ID_SNOW
#define CODEC_ID_DIRAC AV_CODEC_ID_DIRAC
#define CODEC_ID_MJPEG AV_CODEC_ID_MJPEG
#define CODEC_ID_MSMPEG4V3 AV_CODEC_ID_MSMPEG4V3
#define CODEC_ID_RV10 AV_CODEC_ID_RV10
#define CODEC_ID_RV20 AV_CODEC_ID_RV20
#define CODEC_ID_RV30 AV_CODEC_ID_RV30
#define CODEC_ID_RV40 AV_CODEC_ID_RV40
#define CODEC_ID_RAWVIDEO AV_CODEC_ID_RAWVIDEO
#define CODEC_ID_MP3 AV_CODEC_ID_MP3
#define CODEC_ID_AAC AV_CODEC_ID_AAC
#define CODEC_ID_VC1 AV_CODEC_ID_VC1
#define CODEC_ID_PCM_BLURAY AV_CODEC_ID_PCM_BLURAY
#define CODEC_ID_AC3 AV_CODEC_ID_AC3
#define CODEC_ID_EAC3 AV_CODEC_ID_EAC3
#define CODEC_ID_DTS AV_CODEC_ID_DTS
#define CODEC_ID_TRUEHD AV_CODEC_ID_TRUEHD
#define CODEC_ID_S302M AV_CODEC_ID_S302M
#define CODEC_ID_DVB_TELETEXT AV_CODEC_ID_DVB_TELETEXT
#define CODEC_ID_DVB_SUBTITLE AV_CODEC_ID_DVB_SUBTITLE
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

#if !HAVE_AV_SET_PTS_INFO
#if HAVE_AVFORMAT_INTERNAL_H

#include <avformat/internal.h>
#define av_set_pts_info(a,b,c,d) avpriv_set_pts_info(a,b,c,d)

#else
static void av_set_pts_info(AVStream *s, int pts_wrap_bits,
			      unsigned int pts_num, unsigned int pts_den)
{
    AVRational new_tb;
    if(av_reduce(&new_tb.num, &new_tb.den, pts_num, pts_den, INT_MAX)){
        if(new_tb.num != pts_num)
            av_log(NULL, AV_LOG_DEBUG, "st:%d removing common factor %d from timebase\n", s->index, pts_num/new_tb.num);
    }else
        av_log(NULL, AV_LOG_WARNING, "st:%d has too large timebase, reducing\n", s->index);

    if(new_tb.num <= 0 || new_tb.den <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Ignoring attempt to set invalid timebase %d/%d for st:%d\n", new_tb.num, new_tb.den, s->index);
        return;
    }
    s->time_base = new_tb;
    av_codec_set_pkt_timebase(s->codec, new_tb);
    s->pts_wrap_bits = pts_wrap_bits;
}

#endif
#endif

#endif // HAVE_LIBAV_LIBS

#endif // HAVE_LIBAV_HELPER_H
