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


typedef struct AVCodecTag {
  int id;
  unsigned int tag;
} AVCodecTag;

#ifndef MKTAG
#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#endif

const AVCodecTag codec_bmp_tags[] = {
  { CODEC_ID_H264,         MKTAG('H', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('h', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('X', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('x', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('a', 'v', 'c', '1') },
  { CODEC_ID_H264,         MKTAG('V', 'S', 'S', 'H') },
  { CODEC_ID_H263,         MKTAG('H', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('X', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('T', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('L', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('V', 'X', '1', 'K') },
  { CODEC_ID_H263,         MKTAG('Z', 'y', 'G', 'o') },
  { CODEC_ID_H263P,        MKTAG('H', '2', '6', '3') },
  { CODEC_ID_H263I,        MKTAG('I', '2', '6', '3') }, /* intel h263 */
  { CODEC_ID_H261,         MKTAG('H', '2', '6', '1') },
  { CODEC_ID_H263P,        MKTAG('U', '2', '6', '3') },
  { CODEC_ID_H263P,        MKTAG('v', 'i', 'v', '1') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'X', '5', '0') },
  { CODEC_ID_MPEG4,        MKTAG('X', 'V', 'I', 'D') },
  { CODEC_ID_MPEG4,        MKTAG('M', 'P', '4', 'S') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'S', '2') },
  { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  0 ) }, /* some broken avi use this */
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', '1') },
  { CODEC_ID_MPEG4,        MKTAG('B', 'L', 'Z', '0') },
  { CODEC_ID_MPEG4,        MKTAG('m', 'p', '4', 'v') },
  { CODEC_ID_MPEG4,        MKTAG('U', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('W', 'V', '1', 'F') },
  { CODEC_ID_MPEG4,        MKTAG('S', 'E', 'D', 'G') },
  { CODEC_ID_MPEG4,        MKTAG('R', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('3', 'I', 'V', '2') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'F', 'D', 'S') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'V', 'F', 'W') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'C', 'O', 'D') },
  { CODEC_ID_MPEG4,        MKTAG('M', 'V', 'X', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('P', 'M', '4', 'V') },
  { CODEC_ID_MPEG4,        MKTAG('S', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'X', 'G', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('V', 'I', 'D', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'T', '3') },
  { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('H', 'D', 'X', '4') }, /* flipped video */
  { CODEC_ID_MPEG4,        MKTAG('D', 'M', 'K', '2') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'G', 'I') },
  { CODEC_ID_MPEG4,        MKTAG('I', 'N', 'M', 'C') },
  { CODEC_ID_MPEG4,        MKTAG('E', 'P', 'H', 'V') }, /* Ephv MPEG-4 */
  { CODEC_ID_MPEG4,        MKTAG('E', 'M', '4', 'A') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'C', 'C') }, /* Divio MPEG-4 */
  { CODEC_ID_MPEG4,        MKTAG('S', 'N', '4', '0') },
  { CODEC_ID_MPEG4,        MKTAG('V', 'S', 'P', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('U', 'L', 'D', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'V') },
  { CODEC_ID_MPEG4,        MKTAG('S', 'I', 'P', 'P') }, /* Samsung SHR-6040 */
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '3') }, /* default signature when using MSMPEG4 */
  { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', '4', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', 'G', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '5') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '6') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '4') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'V', 'X', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('A', 'P', '4', '1') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '1') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '0') },
  { CODEC_ID_MSMPEG4V2,    MKTAG('M', 'P', '4', '2') },
  { CODEC_ID_MSMPEG4V2,    MKTAG('D', 'I', 'V', '2') },
  { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', 'G', '4') },
  { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', '4', '1') },
  { CODEC_ID_WMV1,         MKTAG('W', 'M', 'V', '1') },
  { CODEC_ID_WMV2,         MKTAG('W', 'M', 'V', '2') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'd') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', 'd') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'l') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '2', '5') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '5', '0') },
  { CODEC_ID_DVVIDEO,      MKTAG('c', 'd', 'v', 'c') }, /* Canopus DV */
  { CODEC_ID_DVVIDEO,      MKTAG('C', 'D', 'V', 'H') }, /* Canopus DV */
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', ' ') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', 's') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '1') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '2') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('m', 'p', 'g', '2') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'P', 'E', 'G') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('P', 'I', 'M', '1') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('P', 'I', 'M', '2') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('V', 'C', 'R', '2') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG( 1 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG2VIDEO,   MKTAG( 2 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('D', 'V', 'R', ' ') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'M', 'E', 'S') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('L', 'M', 'P', '2') }, /* Lead MPEG2 in avi */
  { CODEC_ID_MPEG2VIDEO,   MKTAG('s', 'l', 'i', 'f') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('E', 'M', '2', 'V') },
  { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('L', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('d', 'm', 'b', '1') },
  { CODEC_ID_MJPEG,        MKTAG('m', 'j', 'p', 'a') },
  { CODEC_ID_LJPEG,        MKTAG('L', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('J', 'P', 'G', 'L') }, /* Pegasus lossless JPEG */
  { CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - encoder */
  { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - decoder */
  { CODEC_ID_MJPEG,        MKTAG('j', 'p', 'e', 'g') },
  { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'R', 'n') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'C', 'D', 'V') },
  { CODEC_ID_MJPEG,        MKTAG('Q', 'I', 'V', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('S', 'L', 'M', 'J') }, /* SL M-JPEG */
  { CODEC_ID_MJPEG,        MKTAG('C', 'J', 'P', 'G') }, /* Creative Webcam JPEG */
  { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'L', 'V') }, /* Intel JPEG Library Video Codec */
  { CODEC_ID_MJPEG,        MKTAG('M', 'V', 'J', 'P') }, /* Midvid JPEG Video Codec */
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '1') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '2') },
  { CODEC_ID_MJPEG,        MKTAG('M', 'T', 'S', 'J') },
  { CODEC_ID_MJPEG,        MKTAG('Z', 'J', 'P', 'G') }, /* Paradigm Matrix M-JPEG Codec */
  { CODEC_ID_HUFFYUV,      MKTAG('H', 'F', 'Y', 'U') },
  { CODEC_ID_FFVHUFF,      MKTAG('F', 'F', 'V', 'H') },
  { CODEC_ID_CYUV,         MKTAG('C', 'Y', 'U', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG( 0 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_RAWVIDEO,     MKTAG( 3 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_RAWVIDEO,     MKTAG('I', '4', '2', '0') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'Y', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'N', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('u', 'y', 'v', '1') },
  { CODEC_ID_RAWVIDEO,     MKTAG('2', 'V', 'u', '1') },
  { CODEC_ID_RAWVIDEO,     MKTAG('2', 'v', 'u', 'y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('P', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', '1', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'V', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', 'Y', 'U', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('I', 'Y', 'U', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', '8', '0', '0') },
  { CODEC_ID_RAWVIDEO,     MKTAG('H', 'D', 'Y', 'C') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', 'U', '9') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', 'D', 'T', 'Z') }, /* SoftLab-NSK VideoTizer */
  { CODEC_ID_FRWU,         MKTAG('F', 'R', 'W', 'U') },
  { CODEC_ID_R210,         MKTAG('r', '2', '1', '0') },
  { CODEC_ID_V210,         MKTAG('v', '2', '1', '0') },
  { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '1') },
  { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '2') },
  { CODEC_ID_INDEO4,       MKTAG('I', 'V', '4', '1') },
  { CODEC_ID_INDEO5,       MKTAG('I', 'V', '5', '0') },
  { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '1') },
  { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '0') },
  { CODEC_ID_VP5,          MKTAG('V', 'P', '5', '0') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '0') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '1') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '2') },
  { CODEC_ID_VP6F,         MKTAG('V', 'P', '6', 'F') },
  { CODEC_ID_VP6F,         MKTAG('F', 'L', 'V', '4') },
  { CODEC_ID_ASV1,         MKTAG('A', 'S', 'V', '1') },
  { CODEC_ID_ASV2,         MKTAG('A', 'S', 'V', '2') },
  { CODEC_ID_VCR1,         MKTAG('V', 'C', 'R', '1') },
  { CODEC_ID_FFV1,         MKTAG('F', 'F', 'V', '1') },
  { CODEC_ID_XAN_WC4,      MKTAG('X', 'x', 'a', 'n') },
  { CODEC_ID_MIMIC,        MKTAG('L', 'M', '2', '0') },
  { CODEC_ID_MSRLE,        MKTAG('m', 'r', 'l', 'e') },
  { CODEC_ID_MSRLE,        MKTAG( 1 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_MSRLE,        MKTAG( 2 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_MSVIDEO1,     MKTAG('M', 'S', 'V', 'C') },
  { CODEC_ID_MSVIDEO1,     MKTAG('m', 's', 'v', 'c') },
  { CODEC_ID_MSVIDEO1,     MKTAG('C', 'R', 'A', 'M') },
  { CODEC_ID_MSVIDEO1,     MKTAG('c', 'r', 'a', 'm') },
  { CODEC_ID_MSVIDEO1,     MKTAG('W', 'H', 'A', 'M') },
  { CODEC_ID_MSVIDEO1,     MKTAG('w', 'h', 'a', 'm') },
  { CODEC_ID_CINEPAK,      MKTAG('c', 'v', 'i', 'd') },
  { CODEC_ID_TRUEMOTION1,  MKTAG('D', 'U', 'C', 'K') },
  { CODEC_ID_TRUEMOTION1,  MKTAG('P', 'V', 'E', 'Z') },
  { CODEC_ID_MSZH,         MKTAG('M', 'S', 'Z', 'H') },
  { CODEC_ID_ZLIB,         MKTAG('Z', 'L', 'I', 'B') },
  { CODEC_ID_SNOW,         MKTAG('S', 'N', 'O', 'W') },
  { CODEC_ID_4XM,          MKTAG('4', 'X', 'M', 'V') },
  { CODEC_ID_FLV1,         MKTAG('F', 'L', 'V', '1') },
  { CODEC_ID_FLASHSV,      MKTAG('F', 'S', 'V', '1') },
  { CODEC_ID_SVQ1,         MKTAG('s', 'v', 'q', '1') },
  { CODEC_ID_TSCC,         MKTAG('t', 's', 'c', 'c') },
  { CODEC_ID_ULTI,         MKTAG('U', 'L', 'T', 'I') },
  { CODEC_ID_VIXL,         MKTAG('V', 'I', 'X', 'L') },
  { CODEC_ID_QPEG,         MKTAG('Q', 'P', 'E', 'G') },
  { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '0') },
  { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '1') },
  { CODEC_ID_WMV3,         MKTAG('W', 'M', 'V', '3') },
  { CODEC_ID_VC1,          MKTAG('W', 'V', 'C', '1') },
  { CODEC_ID_VC1,          MKTAG('W', 'M', 'V', 'A') },
  { CODEC_ID_LOCO,         MKTAG('L', 'O', 'C', 'O') },
  { CODEC_ID_WNV1,         MKTAG('W', 'N', 'V', '1') },
  { CODEC_ID_AASC,         MKTAG('A', 'A', 'S', 'C') },
  { CODEC_ID_INDEO2,       MKTAG('R', 'T', '2', '1') },
  { CODEC_ID_FRAPS,        MKTAG('F', 'P', 'S', '1') },
  { CODEC_ID_THEORA,       MKTAG('t', 'h', 'e', 'o') },
  { CODEC_ID_TRUEMOTION2,  MKTAG('T', 'M', '2', '0') },
  { CODEC_ID_CSCD,         MKTAG('C', 'S', 'C', 'D') },
  { CODEC_ID_ZMBV,         MKTAG('Z', 'M', 'B', 'V') },
  { CODEC_ID_KMVC,         MKTAG('K', 'M', 'V', 'C') },
  { CODEC_ID_CAVS,         MKTAG('C', 'A', 'V', 'S') },
  { CODEC_ID_JPEG2000,     MKTAG('M', 'J', '2', 'C') },
  { CODEC_ID_VMNC,         MKTAG('V', 'M', 'n', 'c') },
  { CODEC_ID_TARGA,        MKTAG('t', 'g', 'a', ' ') },
  { CODEC_ID_PNG,          MKTAG('M', 'P', 'N', 'G') },
  { CODEC_ID_PNG,          MKTAG('P', 'N', 'G', '1') },
  { CODEC_ID_CLJR,         MKTAG('c', 'l', 'j', 'r') },
  { CODEC_ID_DIRAC,        MKTAG('d', 'r', 'a', 'c') },
  { CODEC_ID_RPZA,         MKTAG('a', 'z', 'p', 'r') },
  { CODEC_ID_RPZA,         MKTAG('R', 'P', 'Z', 'A') },
  { CODEC_ID_RPZA,         MKTAG('r', 'p', 'z', 'a') },
  { CODEC_ID_SP5X,         MKTAG('S', 'P', '5', '4') },
  { CODEC_ID_AURA,         MKTAG('A', 'U', 'R', 'A') },
  { CODEC_ID_AURA2,        MKTAG('A', 'U', 'R', '2') },
  { CODEC_ID_DPX,          MKTAG('d', 'p', 'x', ' ') },
  { CODEC_ID_KGV1,         MKTAG('K', 'G', 'V', '1') },
  { CODEC_ID_NONE,         0 }
};


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
