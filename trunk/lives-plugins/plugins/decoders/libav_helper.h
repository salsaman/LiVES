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
