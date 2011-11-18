/* WEED is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   Weed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this source code; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA


   Weed is developed by:

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

   mainly based on LiViDO, which is developed by:


   Niels Elburg - http://veejay.sf.net

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

   Denis "Jaromil" Rojo - http://freej.dyne.org

   Tom Schouten - http://zwizwa.fartit.com

   Andraz Tori - http://cvs.cinelerra.org

   reviewed with suggestions and contributions from:

   Silvano "Kysucix" Galliani - http://freej.dyne.org

   Kentaro Fukuchi - http://megaui.net/fukuchi

   Jun Iio - http://www.malib.net

   Carlo Prelz - http://www2.fluido.as:8080/

*/

/* (C) Gabriel "Salsaman" Finch, 2005 - 2011 */

#ifndef __WEED_COMPAT_H__
#define __WEED_COMPAT_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef HAVE_AVCODEC

  // compatibility with libavcodec

#include <libavcodec/avcodec.h>

#ifndef __WEED_PALETTES_H__
#include <weed/weed-palettes.h>
#endif

int avi_pix_fmt_to_weed_palette(enum PixelFormat pix_fmt, int *clamped) {
  // clamped may be set to NULL if you are not interested in the value

  switch (pix_fmt) {
  case PIX_FMT_RGB24:
    return WEED_PALETTE_RGB24;
  case PIX_FMT_BGR24:
    return WEED_PALETTE_BGR24;
  case PIX_FMT_RGBA:
    return WEED_PALETTE_RGBA32;
  case PIX_FMT_BGRA:
    return WEED_PALETTE_BGRA32;
  case PIX_FMT_ARGB:
    return WEED_PALETTE_ARGB32;
  case PIX_FMT_YUV444P:
    return WEED_PALETTE_YUV444P;
  case PIX_FMT_YUV422P:
    return WEED_PALETTE_YUV422P;
  case PIX_FMT_YUV420P:
    return WEED_PALETTE_YUV420P;
  case PIX_FMT_YUYV422:
    return WEED_PALETTE_YUYV;
  case PIX_FMT_UYVY422:
    return WEED_PALETTE_UYVY;
  case PIX_FMT_UYYVYY411:
    return WEED_PALETTE_YUV411;
  case PIX_FMT_GRAY8:
  case PIX_FMT_Y400A:
    return WEED_PALETTE_A8;
  case PIX_FMT_MONOWHITE:
  case PIX_FMT_MONOBLACK:
    return WEED_PALETTE_A1;
  case PIX_FMT_YUVJ422P:
    if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV422P;
  case PIX_FMT_YUVJ444P:
    if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV444P;
  case PIX_FMT_YUVJ420P:
    if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV420P;

  default:
    return WEED_PALETTE_END;
  }
}




enum PixelFormat weed_palette_to_avi_pix_fmt(int pal, int *clamped) {

  switch (pal) {
  case WEED_PALETTE_RGB24:
    return PIX_FMT_RGB24;
  case WEED_PALETTE_BGR24:
    return PIX_FMT_BGR24;
  case WEED_PALETTE_RGBA32:
    return PIX_FMT_RGBA;
  case WEED_PALETTE_BGRA32:
    return PIX_FMT_BGRA;
  case WEED_PALETTE_ARGB32:
    return PIX_FMT_ARGB;
  case WEED_PALETTE_YUV444P:
    if (clamped && *clamped==WEED_YUV_CLAMPING_UNCLAMPED)
      return PIX_FMT_YUVJ444P;
    return PIX_FMT_YUV444P;
  case WEED_PALETTE_YUV422P:
    if (clamped && *clamped==WEED_YUV_CLAMPING_UNCLAMPED)
      return PIX_FMT_YUVJ422P;
    return PIX_FMT_YUV422P;
  case WEED_PALETTE_YUV420P:
    if (clamped && *clamped==WEED_YUV_CLAMPING_UNCLAMPED)
      return PIX_FMT_YUVJ420P;
    return PIX_FMT_YUV420P;
  case WEED_PALETTE_YUYV:
    return PIX_FMT_YUYV422;
  case WEED_PALETTE_UYVY:
    return PIX_FMT_UYVY422;
  case WEED_PALETTE_YUV411:
    return PIX_FMT_UYYVYY411;

  case WEED_PALETTE_A8:
    return PIX_FMT_GRAY8;
  case WEED_PALETTE_A1:
    return PIX_FMT_MONOBLACK;

  default:
    return PIX_FMT_NONE;
  }

  return PIX_FMT_NONE;

}

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_COMPAT_H__
