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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2012 */

#ifndef __WEED_COMPAT_H__
#define __WEED_COMPAT_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#ifdef NEED_FOURCC_COMPAT

#ifndef __WEED_PALETTES_H__
#include <weed/weed-palettes.h>
#endif

int fourccp_to_weedp(unsigned int fourcc, int bpp, int *interlaced, int *sampling,
                     int *sspace, int *clamping) {
  // inputs are fourcc and bpp
  // returns int weed_palette

  // optionally sets interlaced (0 = none, 1 = interlaced), sampling, subspace and clamping



  // TODO - this is probably wrong for some formats and needs testing/verifying with various devices
  // fourcc colorcodes are a nasty mess, and should be avoided whenever possible

  // data from http://www.fourcc.org

  if (clamping!=NULL) *clamping=WEED_YUV_CLAMPING_CLAMPED;
  if (interlaced!=NULL) *interlaced=0;
  if (sspace!=NULL) *sspace=WEED_YUV_SUBSPACE_YCBCR;
  if (sampling!=NULL) *sampling=WEED_YUV_SAMPLING_DEFAULT;

  switch (fourcc) {

    // RGB formats

  case 0x32524742: // BGR3
  case 0x33524742: // BGR3 - tested and OK
  case 0x34524742: // BGR4
    if (bpp==24) return WEED_PALETTE_BGR24;
    if (bpp==32) return WEED_PALETTE_BGRA32;
    break;

  case 0x00000000: // BI_RGB - RGB or BGR ???
  case 0x32776172: // raw2 - RGB or BGR ???

  case 0x32424752: // RGB2
  case 0x33424752: // RGB3
  case 0x34424752: // RGB4
    if (bpp==24) return WEED_PALETTE_RGB24;
    if (bpp==32) return WEED_PALETTE_RGBA32;
    break;
  case 0x41424752: // RGBA
    if (bpp==32) return WEED_PALETTE_RGBA32;
    break;


    // YUV packed formats

  case 0x56595549: // IUYV
    if (interlaced!=NULL) *interlaced=1;
    return WEED_PALETTE_UYVY;
  case 0x31555949: // IYU1
  case 0x31313459: // Y411
    return WEED_PALETTE_YUV411;
  case 0x32555949: // IYU2
    return WEED_PALETTE_YUV888;
  case 0x43594448: // HDYC
    if (sspace!=NULL) *sspace=WEED_YUV_SUBSPACE_BT709;
    return WEED_PALETTE_UYVY;
  case 0x564E5955: // UYNV
  case 0x59565955: // UYVY
  case 0x32323459: // Y422
  case 0x76757963: // cyuv - ???
    return WEED_PALETTE_UYVY;
  case 0x32595559: // YUY2
  case 0x56595559: // YUYV - tested and OK
  case 0x564E5559: // YUNV
  case 0x31313259: // Y211 - ???
    return WEED_PALETTE_YUYV;
  case 0x59455247: // grey
    if (clamping!=NULL) *clamping=WEED_YUV_CLAMPING_UNCLAMPED;
  case 0x30303859: // Y800
  case 0x20203859: // Y8
    return WEED_PALETTE_A8;


    // YUV planar formats
  case 0x41565559: // YUVA
    return WEED_PALETTE_YUVA4444P;
    break;
  case 0x34343449: // I444
    return WEED_PALETTE_YUV444P;
    break;
  case 0x50323234: // 422P ??
    return WEED_PALETTE_YUV422P;
    break;
  case 0x32315659: // YV12
    return WEED_PALETTE_YVU420P;
  case 0x30323449: // I420
  case 0x56555949: // IYUV
  case 0x32315559: // YU12 ??
    return WEED_PALETTE_YUV420P;

  case 0x3032344a: // J420
    if (clamping!=NULL) *clamping=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV420P;
  case 0x3232344a: // J422
    if (clamping!=NULL) *clamping=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV422P;
  case 0x3434344a: // J444
    if (clamping!=NULL) *clamping=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV444P;


    // known formats we cannot use
  case 0x50424752: // RGBP - palettised RGB
  case 0x4f424752: // RGB0 - 15 or 16 bit RGB
  case 0x51424752: // RGBQ - 15 or 16 bit RGB
  case 0x52424752: // RGBR - ???

  case 0x3231564e: // NV12 - planar Y, packed UV
  case 0x30313276: // v210 - 10 bit 422, packed

  case 0x39565559: // YUV9 - 410 planar palette
  case 0x30313449: // I410 - 410 planar palette

  case 0x31313449: // I411 - 411 planar palette
  case 0x30343449: // I440 - 440 planar palette
  case 0x30343450: // J440 - 440 planar palette unclamped

    // no match
  default:
    return WEED_PALETTE_END;
  }
  return WEED_PALETTE_END;
}




#endif








#ifdef HAVE_AVCODEC
#ifdef HAVE_AVUTIL

// compatibility with libavcodec

#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>

typedef struct AVCodecTag {
  int id;
  unsigned int tag;
} AVCodecTag;

#ifndef MKTAG
#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#endif


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
#define CODEC_ID_MP3 AV_CODEC_ID_MP2
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
  { CODEC_ID_MPEG4,        MKTAG(4 ,  0 ,  0 ,  0) },   /* some broken avi use this */
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
  { CODEC_ID_MPEG1VIDEO,   MKTAG(1 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG2VIDEO,   MKTAG(2 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG4,        MKTAG(4 ,  0 ,  0 ,  16) },
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
  { CODEC_ID_RAWVIDEO,     MKTAG(0 ,  0 ,  0 ,  0) },
  { CODEC_ID_RAWVIDEO,     MKTAG(3 ,  0 ,  0 ,  0) },
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
  { CODEC_ID_MSRLE,        MKTAG(1 ,  0 ,  0 ,  0) },
  { CODEC_ID_MSRLE,        MKTAG(2 ,  0 ,  0 ,  0) },
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
#if FF_API_SNOW
  { CODEC_ID_SNOW,         MKTAG('S', 'N', 'O', 'W') },
#endif
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

#endif // HAVE_AVUTIL
#endif // HAVE_AVCODEC

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_COMPAT_H__
