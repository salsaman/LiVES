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
   Gabriel "Salsaman" Finch - http://lives-video.com

   partly based on LiViDO, which is developed by:
   Niels Elburg - http://veejay.sf.net
   Denis "Jaromil" Rojo - http://freej.dyne.org
   Tom Schouten - http://zwizwa.fartit.com
   Andraz Tori - http://cvs.cinelerra.org

   reviewed with suggestions and contributions from:
   Silvano "Kysucix" Galliani - http://freej.dyne.org
   Kentaro Fukuchi - http://megaui.net/fukuchi
   Jun Iio - http://www.malib.net
   Carlo Prelz - http://www2.fluido.as:8080/
*/

/* (C) G. Finch, 2005 - 2019 */

#ifndef __WEED_COMPAT_H__
#define __WEED_COMPAT_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <string.h>

#define WEED_COMPAT_VERSION 0.12.0

#ifdef NEED_FOURCC_COMPAT

#ifndef __WEED_PALETTES_H__
#ifdef NEED_LOCAL_WEED
#include "weed-palettes.h"
#else
#include <weed/weed-palettes.h>
#endif
#endif

#ifndef WEED_FOURCC_COMPAT
#define WEED_FOURCC_COMPAT
#endif

int fourccp_to_weedp(unsigned int fourcc, int bpp, int *interlaced, int *sampling,
                     int *sspace, int *clamping) {
  // inputs are fourcc and bpp
  // returns int weed_palette

  // optionally sets interlaced (0 = none, 1 = interlaced), sampling, subspace and clamping

  // TODO - this is probably wrong for some formats and needs testing/verifying with various devices
  // fourcc colorcodes are a nasty mess, and should be avoided whenever possible

  // data from http://www.fourcc.org

  if (clamping != NULL) *clamping = WEED_YUV_CLAMPING_CLAMPED;
  if (interlaced != NULL) *interlaced = 0;
  if (sspace != NULL) *sspace = WEED_YUV_SUBSPACE_YCBCR;
  if (sampling != NULL) *sampling = WEED_YUV_SAMPLING_DEFAULT;

  switch (fourcc) {
  // RGB formats
  case 0x32524742: // BGR3
  case 0x33524742: // BGR3 - tested and OK
  case 0x34524742: // BGR4
    if (bpp == 24) return WEED_PALETTE_BGR24;
    if (bpp == 32) return WEED_PALETTE_BGRA32;
    break;

  case 0x00000000: // BI_RGB - RGB or BGR ???
  case 0x32776172: // raw2 - RGB or BGR ???

  case 0x32424752: // RGB2
  case 0x33424752: // RGB3
  case 0x34424752: // RGB4
    if (bpp == 24) return WEED_PALETTE_RGB24;
    if (bpp == 32) return WEED_PALETTE_RGBA32;
    break;
  case 0x41424752: // RGBA
    if (bpp == 32) return WEED_PALETTE_RGBA32;
    break;

  // YUV packed formats

  case 0x56595549: // IUYV
    if (interlaced != NULL) *interlaced = 1;
    return WEED_PALETTE_UYVY;
  case 0x31555949: // IYU1
  case 0x31313459: // Y411
    return WEED_PALETTE_YUV411;
  case 0x32555949: // IYU2
    return WEED_PALETTE_YUV888;
  case 0x43594448: // HDYC
    if (sspace != NULL) *sspace = WEED_YUV_SUBSPACE_BT709;
    return WEED_PALETTE_UYVY;
  case 0x564E5955: // UYNV
  case 0x59565955: // UYVY
  case 0x32323459: // Y422
  case 0x76757963: // cyuv - ???
    return WEED_PALETTE_UYVY;
  case 0x32595559: // YUY2
  case 0x56595559: // YUYV
  case 0x564E5559: // YUNV
    return WEED_PALETTE_YUYV;
  case 0x59455247: // grey
    if (clamping != NULL) *clamping = WEED_YUV_CLAMPING_UNCLAMPED;
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
    if (clamping != NULL) *clamping = WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV420P;
  case 0x3232344a: // J422
    if (clamping != NULL) *clamping = WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV422P;
  case 0x3434344a: // J444
    if (clamping != NULL) *clamping = WEED_YUV_CLAMPING_UNCLAMPED;
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
  case 0x31313259: // Y211 - ???

  case 0x31313449: // I411 - 411 planar palette
  case 0x30343449: // I440 - 440 planar palette
  case 0x30343450: // J440 - 440 planar palette unclamped
  case 0x56555941: // - YUVA8888 but with alpha first, ie. AYUV8888
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

#ifndef __WEED_PALETTES_H__
#ifdef NEED_LOCAL_WEED
#include "weed-palettes.h"
#else
#include <weed/weed-palettes.h>
#endif
#endif

typedef struct AVCodecTag {
  int id;
  unsigned int tag;
} AVCodecTag;

#ifndef MKTAG
#define MKTAG(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#endif

#if (LIBAVCODEC_VERSION_MAJOR <= 54)
#define AVCodecID CodecID

#define AV_CODEC_ID_NONE CODEC_ID_NONE
#define AV_CODEC_ID_H264 CODEC_ID_H264
#define AV_CODEC_ID_H263 CODEC_ID_H263
#define AV_CODEC_ID_H263P CODEC_ID_H263P
#define AV_CODEC_ID_H263I CODEC_ID_H263I
#define AV_CODEC_ID_H261 CODEC_ID_H261
#define AV_CODEC_ID_MPEG4 CODEC_ID_MPEG4
#define AV_CODEC_ID_MSMPEG4V3 CODEC_ID_MSMPEG4V3
#define AV_CODEC_ID_MSMPEG4V2 CODEC_ID_MSMPEG4V2
#define AV_CODEC_ID_MSMPEG4V1 CODEC_ID_MSMPEG4V1
#define AV_CODEC_ID_WMV1 CODEC_ID_WMV1
#define AV_CODEC_ID_WMV2 CODEC_ID_WMV2
#define AV_CODEC_ID_DVVIDEO CODEC_ID_DVVIDEO
#define AV_CODEC_ID_MPEG1VIDEO CODEC_ID_MPEG1VIDEO
#define AV_CODEC_ID_MPEG2VIDEO CODEC_ID_MPEG2VIDEO
#define AV_CODEC_ID_MJPEG CODEC_ID_MJPEG
#define AV_CODEC_ID_LJPEG CODEC_ID_LJPEG
#define AV_CODEC_ID_JPEGLS CODEC_ID_JPEGLS
#define AV_CODEC_ID_HUFFYUV CODEC_ID_HUFFYUV
#define AV_CODEC_ID_FFVHUFF CODEC_ID_FFVHUFF
#define AV_CODEC_ID_CYUV CODEC_ID_CYUV
#define AV_CODEC_ID_RAWVIDEO CODEC_ID_RAWVIDEO
#define AV_CODEC_ID_INDEO2 CODEC_ID_INDEO2
#define AV_CODEC_ID_INDEO3 CODEC_ID_INDEO3
#define AV_CODEC_ID_INDEO4 CODEC_ID_INDEO4
#define AV_CODEC_ID_INDEO5 CODEC_ID_INDEO5
#define AV_CODEC_ID_VP3 CODEC_ID_VP3
#define AV_CODEC_ID_VP5 CODEC_ID_VP5
#define AV_CODEC_ID_VP6 CODEC_ID_VP6
#define AV_CODEC_ID_VP6F CODEC_ID_VP6F
#define AV_CODEC_ID_VP6A CODEC_ID_VP6A
#define AV_CODEC_ID_ASV1 CODEC_ID_ASV1
#define AV_CODEC_ID_ASV2 CODEC_ID_ASV2
#define AV_CODEC_ID_VCR1 CODEC_ID_VCR1
#define AV_CODEC_ID_FFV1 CODEC_ID_FFV1
#define AV_CODEC_ID_XAN_WC4 CODEC_ID_XAN_WC4
#define AV_CODEC_ID_MIMIC CODEC_ID_MIMIC
#define AV_CODEC_ID_MSRLE CODEC_ID_MSRLE
#define AV_CODEC_ID_MSVIDEO1 CODEC_ID_MSVIDEO1
#define AV_CODEC_ID_CINEPAK CODEC_ID_CINEPAK
#define AV_CODEC_ID_TRUEMOTION1 CODEC_ID_TRUEMOTION1
#define AV_CODEC_ID_TRUEMOTION2 CODEC_ID_TRUEMOTION2
#define AV_CODEC_ID_MSZH CODEC_ID_MSZH
#define AV_CODEC_ID_ZLIB CODEC_ID_ZLIB

#if FF_API_SNOW
#define AV_CODEC_ID_SNOW CODEC_ID_SNOW
#endif

#define AV_CODEC_ID_4XM CODEC_ID_4XM
#define AV_CODEC_ID_FLV1 CODEC_ID_FLV1
#define AV_CODEC_ID_FLASHSV CODEC_ID_FLASHSV
#define AV_CODEC_ID_SVQ1 CODEC_ID_SVQ1
#define AV_CODEC_ID_TSCC CODEC_ID_TSCC
#define AV_CODEC_ID_ULTI CODEC_ID_ULTI
#define AV_CODEC_ID_VIXL CODEC_ID_VIXL
#define AV_CODEC_ID_QPEG CODEC_ID_QPEG
#define AV_CODEC_ID_WMV3 CODEC_ID_WMV3
#define AV_CODEC_ID_VC1 CODEC_ID_VC1
#define AV_CODEC_ID_LOCO CODEC_ID_LOCO
#define AV_CODEC_ID_WNV1 CODEC_ID_WNV1
#define AV_CODEC_ID_AASC CODEC_ID_AASC
#define AV_CODEC_ID_FRAPS CODEC_ID_FRAPS
#define AV_CODEC_ID_THEORA CODEC_ID_THEORA
#define AV_CODEC_ID_CSCD CODEC_ID_CSCD
#define AV_CODEC_ID_ZMBV CODEC_ID_ZMBV
#define AV_CODEC_ID_KMVC CODEC_ID_KMVC
#define AV_CODEC_ID_CAVS CODEC_ID_CAVS
#define AV_CODEC_ID_JPEG2000 CODEC_ID_JPEG2000
#define AV_CODEC_ID_VMNC CODEC_ID_VMNC
#define AV_CODEC_ID_TARGA CODEC_ID_TARGA
#define AV_CODEC_ID_PNG CODEC_ID_PNG
#define AV_CODEC_ID_GIF CODEC_ID_GIF
#define AV_CODEC_ID_TIFF CODEC_ID_TIFF
#define AV_CODEC_ID_CLJR CODEC_ID_CLJR
#define AV_CODEC_ID_DIRAC CODEC_ID_DIRAC
#define AV_CODEC_ID_RPZA CODEC_ID_RPZA
#define AV_CODEC_ID_SP5X CODEC_ID_SP5X

#define AV_CODEC_ID_FLASHSV2 CODEC_ID_FLASHSV2
#define AV_CODEC_ID_TEXT CODEC_ID_TEXT
#define AV_CODEC_ID_SSA CODEC_ID_SSA
#define AV_CODEC_ID_SRT CODEC_ID_SRT
#define AV_CODEC_ID_VP8 CODEC_ID_VP8
#define AV_CODEC_ID_RV10 CODEC_ID_RV10
#define AV_CODEC_ID_RV20 CODEC_ID_RV20
#define AV_CODEC_ID_RV30 CODEC_ID_RV30
#define AV_CODEC_ID_RV40 CODEC_ID_RV40
#define AV_CODEC_ID_MP3 CODEC_ID_MP3
#define AV_CODEC_ID_MP2 CODEC_ID_MP2
#define AV_CODEC_ID_AAC CODEC_ID_AAC
#define AV_CODEC_ID_PCM_BLURAY CODEC_ID_PCM_BLURAY
#define AV_CODEC_ID_AC3 CODEC_ID_AC3
#define AV_CODEC_ID_VORBIS CODEC_ID_VORBIS
#define AV_CODEC_ID_EAC3 CODEC_ID_EAC3
#define AV_CODEC_ID_DTS CODEC_ID_DTS
#define AV_CODEC_ID_TRUEHD CODEC_ID_TRUEHD
#define AV_CODEC_ID_S302M CODEC_ID_S302M
#define AV_CODEC_ID_DVB_TELETEXT CODEC_ID_DVB_TELETEXT
#define AV_CODEC_ID_DVB_SUBTITLE CODEC_ID_DVB_SUBTITLE
#define AV_CODEC_ID_DVD_SUBTITLE CODEC_ID_DVD_SUBTITLE

#define AV_CODEC_ID_MOV_TEXT CODEC_ID_MOV_TEXT
#define AV_CODEC_ID_MP4ALS CODEC_ID_MP4ALS
#define AV_CODEC_ID_QCELP CODEC_ID_QCELP
#define AV_CODEC_ID_MPEG4SYSTEMS CODEC_ID_MPEG4SYSTEMS

#define AV_CODEC_ID_MPEG2TS CODEC_ID_MPEG2TS
#define AV_CODEC_ID_AAC_LATM CODEC_ID_AAC_LATM
#define AV_CODEC_ID_HDMV_PGS_SUBTITLE CODEC_ID_HDMV_PGS_SUBTITLE

#define AV_CODEC_ID_FLAC CODEC_ID_FLAC
#define AV_CODEC_ID_MLP CODEC_ID_MLP

#define AV_CODEC_ID_PCM_F32LE CODEC_ID_PCM_F32LE
#define AV_CODEC_ID_PCM_F64LE CODEC_ID_PCM_F64LE

#define AV_CODEC_ID_PCM_S16BE CODEC_ID_PCM_S16BE
#define AV_CODEC_ID_PCM_S24BE CODEC_ID_PCM_S24BE
#define AV_CODEC_ID_PCM_S32BE CODEC_ID_PCM_S32BE

#define AV_CODEC_ID_PCM_S16LE CODEC_ID_PCM_S16LE
#define AV_CODEC_ID_PCM_S24LE CODEC_ID_PCM_S24LE
#define AV_CODEC_ID_PCM_S32LE CODEC_ID_PCM_S32LE

#define AV_CODEC_ID_PCM_U8 CODEC_ID_PCM_U8

#define AV_CODEC_ID_QDM2 CODEC_ID_QDM2
#define AV_CODEC_ID_RA_144 CODEC_ID_RA_144
#define AV_CODEC_ID_RA_288 CODEC_ID_RA_288
#define AV_CODEC_ID_ATRAC3 CODEC_ID_ATRAC3
#define AV_CODEC_ID_COOK CODEC_ID_COOK
#define AV_CODEC_ID_SIPR CODEC_ID_SIPR
#define AV_CODEC_ID_TTA CODEC_ID_TTA
#define AV_CODEC_ID_WAVPACK CODEC_ID_WAVPACK

#define AV_CODEC_ID_TTF CODEC_ID_TTF

// from mkv_decoder.h
#define AV_CODEC_ID_R10K CODEC_ID_R10K
#define AV_CODEC_ID_R210 CODEC_ID_R210
#define AV_CODEC_ID_V210 CODEC_ID_V210
#define AV_CODEC_ID_MJPEGB CODEC_ID_MJPEGB
#define AV_CODEC_ID_SVQ3 CODEC_ID_SVQ3
#define AV_CODEC_ID_8BPS CODEC_ID_8BPS
#define AV_CODEC_ID_SMC CODEC_ID_SMC
#define AV_CODEC_ID_QTRLE CODEC_ID_QTRLE
#define AV_CODEC_ID_QDRAW CODEC_ID_QDRAW
#define AV_CODEC_ID_DNXHD CODEC_ID_DNXHD
#define AV_CODEC_ID_SGI CODEC_ID_SGI
#define AV_CODEC_ID_DPX CODEC_ID_DPX
#define AV_CODEC_ID_PRORES CODEC_ID_PRORES

#endif

const AVCodecTag codec_bmp_tags[] = {
  { AV_CODEC_ID_H264,         MKTAG('H', '2', '6', '4') },
  { AV_CODEC_ID_H264,         MKTAG('h', '2', '6', '4') },
  { AV_CODEC_ID_H264,         MKTAG('X', '2', '6', '4') },
  { AV_CODEC_ID_H264,         MKTAG('x', '2', '6', '4') },
  { AV_CODEC_ID_H264,         MKTAG('a', 'v', 'c', '1') },
  { AV_CODEC_ID_H264,         MKTAG('V', 'S', 'S', 'H') },
  { AV_CODEC_ID_H263,         MKTAG('H', '2', '6', '3') },
  { AV_CODEC_ID_H263,         MKTAG('X', '2', '6', '3') },
  { AV_CODEC_ID_H263,         MKTAG('T', '2', '6', '3') },
  { AV_CODEC_ID_H263,         MKTAG('L', '2', '6', '3') },
  { AV_CODEC_ID_H263,         MKTAG('V', 'X', '1', 'K') },
  { AV_CODEC_ID_H263,         MKTAG('Z', 'y', 'G', 'o') },
  { AV_CODEC_ID_H263P,        MKTAG('H', '2', '6', '3') },
  { AV_CODEC_ID_H263I,        MKTAG('I', '2', '6', '3') }, /* intel h263 */
  { AV_CODEC_ID_H261,         MKTAG('H', '2', '6', '1') },
  { AV_CODEC_ID_H263P,        MKTAG('U', '2', '6', '3') },
  { AV_CODEC_ID_H263P,        MKTAG('v', 'i', 'v', '1') },
  { AV_CODEC_ID_MPEG4,        MKTAG('F', 'M', 'P', '4') },
  { AV_CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', 'X') },
  { AV_CODEC_ID_MPEG4,        MKTAG('D', 'X', '5', '0') },
  { AV_CODEC_ID_MPEG4,        MKTAG('X', 'V', 'I', 'D') },
  { AV_CODEC_ID_MPEG4,        MKTAG('M', 'P', '4', 'S') },
  { AV_CODEC_ID_MPEG4,        MKTAG('M', '4', 'S', '2') },
  { AV_CODEC_ID_MPEG4,        MKTAG(4 ,  0 ,  0 ,  0) },   /* some broken avi use this */
  { AV_CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', '1') },
  { AV_CODEC_ID_MPEG4,        MKTAG('B', 'L', 'Z', '0') },
  { AV_CODEC_ID_MPEG4,        MKTAG('m', 'p', '4', 'v') },
  { AV_CODEC_ID_MPEG4,        MKTAG('U', 'M', 'P', '4') },
  { AV_CODEC_ID_MPEG4,        MKTAG('W', 'V', '1', 'F') },
  { AV_CODEC_ID_MPEG4,        MKTAG('S', 'E', 'D', 'G') },
  { AV_CODEC_ID_MPEG4,        MKTAG('R', 'M', 'P', '4') },
  { AV_CODEC_ID_MPEG4,        MKTAG('3', 'I', 'V', '2') },
  { AV_CODEC_ID_MPEG4,        MKTAG('F', 'F', 'D', 'S') },
  { AV_CODEC_ID_MPEG4,        MKTAG('F', 'V', 'F', 'W') },
  { AV_CODEC_ID_MPEG4,        MKTAG('D', 'C', 'O', 'D') },
  { AV_CODEC_ID_MPEG4,        MKTAG('M', 'V', 'X', 'M') },
  { AV_CODEC_ID_MPEG4,        MKTAG('P', 'M', '4', 'V') },
  { AV_CODEC_ID_MPEG4,        MKTAG('S', 'M', 'P', '4') },
  { AV_CODEC_ID_MPEG4,        MKTAG('D', 'X', 'G', 'M') },
  { AV_CODEC_ID_MPEG4,        MKTAG('V', 'I', 'D', 'M') },
  { AV_CODEC_ID_MPEG4,        MKTAG('M', '4', 'T', '3') },
  { AV_CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'X') },
  { AV_CODEC_ID_MPEG4,        MKTAG('H', 'D', 'X', '4') }, /* flipped video */
  { AV_CODEC_ID_MPEG4,        MKTAG('D', 'M', 'K', '2') },
  { AV_CODEC_ID_MPEG4,        MKTAG('D', 'I', 'G', 'I') },
  { AV_CODEC_ID_MPEG4,        MKTAG('I', 'N', 'M', 'C') },
  { AV_CODEC_ID_MPEG4,        MKTAG('E', 'P', 'H', 'V') }, /* Ephv MPEG-4 */
  { AV_CODEC_ID_MPEG4,        MKTAG('E', 'M', '4', 'A') },
  { AV_CODEC_ID_MPEG4,        MKTAG('M', '4', 'C', 'C') }, /* Divio MPEG-4 */
  { AV_CODEC_ID_MPEG4,        MKTAG('S', 'N', '4', '0') },
  { AV_CODEC_ID_MPEG4,        MKTAG('V', 'S', 'P', 'X') },
  { AV_CODEC_ID_MPEG4,        MKTAG('U', 'L', 'D', 'X') },
  { AV_CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'V') },
  { AV_CODEC_ID_MPEG4,        MKTAG('S', 'I', 'P', 'P') }, /* Samsung SHR-6040 */
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '3') }, /* default signature when using MSMPEG4 */
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', '4', '3') },
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', 'G', '3') },
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '5') },
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '6') },
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '4') },
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'V', 'X', '3') },
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('A', 'P', '4', '1') },
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '1') },
  { AV_CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '0') },
  { AV_CODEC_ID_MSMPEG4V2,    MKTAG('M', 'P', '4', '2') },
  { AV_CODEC_ID_MSMPEG4V2,    MKTAG('D', 'I', 'V', '2') },
  { AV_CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', 'G', '4') },
  { AV_CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', '4', '1') },
  { AV_CODEC_ID_WMV1,         MKTAG('W', 'M', 'V', '1') },
  { AV_CODEC_ID_WMV2,         MKTAG('W', 'M', 'V', '2') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'd') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', 'd') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'l') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '2', '5') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '5', '0') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('c', 'd', 'v', 'c') }, /* Canopus DV */
  { AV_CODEC_ID_DVVIDEO,      MKTAG('C', 'D', 'V', 'H') }, /* Canopus DV */
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', ' ') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', 's') },
  { AV_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
  { AV_CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '1') },
  { AV_CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '2') },
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG('m', 'p', 'g', '2') },
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'P', 'E', 'G') },
  { AV_CODEC_ID_MPEG1VIDEO,   MKTAG('P', 'I', 'M', '1') },
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG('P', 'I', 'M', '2') },
  { AV_CODEC_ID_MPEG1VIDEO,   MKTAG('V', 'C', 'R', '2') },
  { AV_CODEC_ID_MPEG1VIDEO,   MKTAG(1 ,  0 ,  0 ,  16) },
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG(2 ,  0 ,  0 ,  16) },
  { AV_CODEC_ID_MPEG4,        MKTAG(4 ,  0 ,  0 ,  16) },
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG('D', 'V', 'R', ' ') },
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'M', 'E', 'S') },
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG('L', 'M', 'P', '2') }, /* Lead MPEG2 in avi */
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG('s', 'l', 'i', 'f') },
  { AV_CODEC_ID_MPEG2VIDEO,   MKTAG('E', 'M', '2', 'V') },
  { AV_CODEC_ID_MJPEG,        MKTAG('M', 'J', 'P', 'G') },
  { AV_CODEC_ID_MJPEG,        MKTAG('L', 'J', 'P', 'G') },
  { AV_CODEC_ID_MJPEG,        MKTAG('d', 'm', 'b', '1') },
  { AV_CODEC_ID_MJPEG,        MKTAG('m', 'j', 'p', 'a') },
  { AV_CODEC_ID_LJPEG,        MKTAG('L', 'J', 'P', 'G') },
  { AV_CODEC_ID_MJPEG,        MKTAG('J', 'P', 'G', 'L') }, /* Pegasus lossless JPEG */
  { AV_CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - encoder */
  { AV_CODEC_ID_MJPEG,        MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - decoder */
  { AV_CODEC_ID_MJPEG,        MKTAG('j', 'p', 'e', 'g') },
  { AV_CODEC_ID_MJPEG,        MKTAG('I', 'J', 'P', 'G') },
  { AV_CODEC_ID_MJPEG,        MKTAG('A', 'V', 'R', 'n') },
  { AV_CODEC_ID_MJPEG,        MKTAG('A', 'C', 'D', 'V') },
  { AV_CODEC_ID_MJPEG,        MKTAG('Q', 'I', 'V', 'G') },
  { AV_CODEC_ID_MJPEG,        MKTAG('S', 'L', 'M', 'J') }, /* SL M-JPEG */
  { AV_CODEC_ID_MJPEG,        MKTAG('C', 'J', 'P', 'G') }, /* Creative Webcam JPEG */
  { AV_CODEC_ID_MJPEG,        MKTAG('I', 'J', 'L', 'V') }, /* Intel JPEG Library Video Codec */
  { AV_CODEC_ID_MJPEG,        MKTAG('M', 'V', 'J', 'P') }, /* Midvid JPEG Video Codec */
  { AV_CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '1') },
  { AV_CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '2') },
  { AV_CODEC_ID_MJPEG,        MKTAG('M', 'T', 'S', 'J') },
  { AV_CODEC_ID_MJPEG,        MKTAG('Z', 'J', 'P', 'G') }, /* Paradigm Matrix M-JPEG Codec */
  { AV_CODEC_ID_HUFFYUV,      MKTAG('H', 'F', 'Y', 'U') },
  { AV_CODEC_ID_FFVHUFF,      MKTAG('F', 'F', 'V', 'H') },
  { AV_CODEC_ID_CYUV,         MKTAG('C', 'Y', 'U', 'V') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG(0 ,  0 ,  0 ,  0) },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG(3 ,  0 ,  0 ,  0) },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('I', '4', '2', '0') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'Y', '2') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '2', '2') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('V', '4', '2', '2') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'N', 'V') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'V') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'Y') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('u', 'y', 'v', '1') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('2', 'V', 'u', '1') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('2', 'v', 'u', 'y') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('P', '4', '2', '2') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', '1', '2') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'V', 'Y') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('V', 'Y', 'U', 'Y') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('I', 'Y', 'U', 'V') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('Y', '8', '0', '0') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('H', 'D', 'Y', 'C') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', 'U', '9') },
  { AV_CODEC_ID_RAWVIDEO,     MKTAG('V', 'D', 'T', 'Z') }, /* SoftLab-NSK VideoTizer */
  { AV_CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '1') },
  { AV_CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '2') },
  { AV_CODEC_ID_INDEO4,       MKTAG('I', 'V', '4', '1') },
  { AV_CODEC_ID_INDEO5,       MKTAG('I', 'V', '5', '0') },
  { AV_CODEC_ID_VP3,          MKTAG('V', 'P', '3', '1') },
  { AV_CODEC_ID_VP3,          MKTAG('V', 'P', '3', '0') },
  { AV_CODEC_ID_VP5,          MKTAG('V', 'P', '5', '0') },
  { AV_CODEC_ID_VP6,          MKTAG('V', 'P', '6', '0') },
  { AV_CODEC_ID_VP6,          MKTAG('V', 'P', '6', '1') },
  { AV_CODEC_ID_VP6,          MKTAG('V', 'P', '6', '2') },
  { AV_CODEC_ID_VP6F,         MKTAG('V', 'P', '6', 'F') },
  { AV_CODEC_ID_VP6F,         MKTAG('F', 'L', 'V', '4') },
  { AV_CODEC_ID_ASV1,         MKTAG('A', 'S', 'V', '1') },
  { AV_CODEC_ID_ASV2,         MKTAG('A', 'S', 'V', '2') },
  { AV_CODEC_ID_VCR1,         MKTAG('V', 'C', 'R', '1') },
  { AV_CODEC_ID_FFV1,         MKTAG('F', 'F', 'V', '1') },
  { AV_CODEC_ID_XAN_WC4,      MKTAG('X', 'x', 'a', 'n') },
  { AV_CODEC_ID_MIMIC,        MKTAG('L', 'M', '2', '0') },
  { AV_CODEC_ID_MSRLE,        MKTAG('m', 'r', 'l', 'e') },
  { AV_CODEC_ID_MSRLE,        MKTAG(1 ,  0 ,  0 ,  0) },
  { AV_CODEC_ID_MSRLE,        MKTAG(2 ,  0 ,  0 ,  0) },
  { AV_CODEC_ID_MSVIDEO1,     MKTAG('M', 'S', 'V', 'C') },
  { AV_CODEC_ID_MSVIDEO1,     MKTAG('m', 's', 'v', 'c') },
  { AV_CODEC_ID_MSVIDEO1,     MKTAG('C', 'R', 'A', 'M') },
  { AV_CODEC_ID_MSVIDEO1,     MKTAG('c', 'r', 'a', 'm') },
  { AV_CODEC_ID_MSVIDEO1,     MKTAG('W', 'H', 'A', 'M') },
  { AV_CODEC_ID_MSVIDEO1,     MKTAG('w', 'h', 'a', 'm') },
  { AV_CODEC_ID_CINEPAK,      MKTAG('c', 'v', 'i', 'd') },
  { AV_CODEC_ID_TRUEMOTION1,  MKTAG('D', 'U', 'C', 'K') },
  { AV_CODEC_ID_TRUEMOTION1,  MKTAG('P', 'V', 'E', 'Z') },
  { AV_CODEC_ID_MSZH,         MKTAG('M', 'S', 'Z', 'H') },
  { AV_CODEC_ID_ZLIB,         MKTAG('Z', 'L', 'I', 'B') },
#if FF_API_SNOW
  { AV_CODEC_ID_SNOW,         MKTAG('S', 'N', 'O', 'W') },
#endif
  { AV_CODEC_ID_4XM,          MKTAG('4', 'X', 'M', 'V') },
  { AV_CODEC_ID_FLV1,         MKTAG('F', 'L', 'V', '1') },
  { AV_CODEC_ID_FLASHSV,      MKTAG('F', 'S', 'V', '1') },
  { AV_CODEC_ID_SVQ1,         MKTAG('s', 'v', 'q', '1') },
  { AV_CODEC_ID_TSCC,         MKTAG('t', 's', 'c', 'c') },
  { AV_CODEC_ID_ULTI,         MKTAG('U', 'L', 'T', 'I') },
  { AV_CODEC_ID_VIXL,         MKTAG('V', 'I', 'X', 'L') },
  { AV_CODEC_ID_QPEG,         MKTAG('Q', 'P', 'E', 'G') },
  { AV_CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '0') },
  { AV_CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '1') },
  { AV_CODEC_ID_WMV3,         MKTAG('W', 'M', 'V', '3') },
  { AV_CODEC_ID_VC1,          MKTAG('W', 'V', 'C', '1') },
  { AV_CODEC_ID_VC1,          MKTAG('W', 'M', 'V', 'A') },
  { AV_CODEC_ID_LOCO,         MKTAG('L', 'O', 'C', 'O') },
  { AV_CODEC_ID_WNV1,         MKTAG('W', 'N', 'V', '1') },
  { AV_CODEC_ID_AASC,         MKTAG('A', 'A', 'S', 'C') },
  { AV_CODEC_ID_INDEO2,       MKTAG('R', 'T', '2', '1') },
  { AV_CODEC_ID_FRAPS,        MKTAG('F', 'P', 'S', '1') },
  { AV_CODEC_ID_THEORA,       MKTAG('t', 'h', 'e', 'o') },
  { AV_CODEC_ID_TRUEMOTION2,  MKTAG('T', 'M', '2', '0') },
  { AV_CODEC_ID_CSCD,         MKTAG('C', 'S', 'C', 'D') },
  { AV_CODEC_ID_ZMBV,         MKTAG('Z', 'M', 'B', 'V') },
  { AV_CODEC_ID_KMVC,         MKTAG('K', 'M', 'V', 'C') },
  { AV_CODEC_ID_CAVS,         MKTAG('C', 'A', 'V', 'S') },
  { AV_CODEC_ID_JPEG2000,     MKTAG('M', 'J', '2', 'C') },
  { AV_CODEC_ID_VMNC,         MKTAG('V', 'M', 'n', 'c') },
  { AV_CODEC_ID_TARGA,        MKTAG('t', 'g', 'a', ' ') },
  { AV_CODEC_ID_PNG,          MKTAG('M', 'P', 'N', 'G') },
  { AV_CODEC_ID_PNG,          MKTAG('P', 'N', 'G', '1') },
  { AV_CODEC_ID_CLJR,         MKTAG('c', 'l', 'j', 'r') },
  { AV_CODEC_ID_DIRAC,        MKTAG('d', 'r', 'a', 'c') },
  { AV_CODEC_ID_RPZA,         MKTAG('a', 'z', 'p', 'r') },
  { AV_CODEC_ID_RPZA,         MKTAG('R', 'P', 'Z', 'A') },
  { AV_CODEC_ID_RPZA,         MKTAG('r', 'p', 'z', 'a') },
  { AV_CODEC_ID_SP5X,         MKTAG('S', 'P', '5', '4') },
  { AV_CODEC_ID_NONE,         0 }
};

#if defined FF_API_PIX_FMT ||  defined AVUTIL_PIXFMT_H

int avi_color_range_to_weed_clamping(enum AVColorRange range) {
  switch (range) {
  case AVCOL_RANGE_MPEG:
    return WEED_YUV_CLAMPING_CLAMPED;
  case AVCOL_RANGE_JPEG:
    return WEED_YUV_CLAMPING_UNCLAMPED;
  default:
    break;
  }
  return WEED_YUV_CLAMPING_CLAMPED;
}

enum AVColorRange weed_clamping_to_avi_color_range(int clamping) {
  switch (clamping) {
  case WEED_YUV_CLAMPING_CLAMPED:
        return AVCOL_RANGE_MPEG;
    case WEED_YUV_CLAMPING_UNCLAMPED:
      return AVCOL_RANGE_JPEG;
    }
    return AVCOL_RANGE_NB;
  }

#ifndef AVUTIL_PIXFMT_H

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
  case PIX_FMT_YUVA444P:
    return WEED_PALETTE_YUVA4444P;
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
    if (clamped) *clamped = WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV422P;
  case PIX_FMT_YUVJ444P:
    if (clamped) *clamped = WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV444P;
  case PIX_FMT_YUVAJ444P:
    if (clamped) *clamped = WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUVA444P;
  case PIX_FMT_YUVJ420P:
    if (clamped) *clamped = WEED_YUV_CLAMPING_UNCLAMPED;
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
      if (clamped && *clamped == WEED_YUV_CLAMPING_UNCLAMPED)
        return PIX_FMT_YUVJ444P;
      return PIX_FMT_YUV444P;
    case WEED_PALETTE_YUVA444P:
      if (clamped && *clamped == WEED_YUV_CLAMPING_UNCLAMPED)
        return PIX_FMT_YUVAJ444P;
      return PIX_FMT_YUVA444P;
    case WEED_PALETTE_YUV422P:
      if (clamped && *clamped == WEED_YUV_CLAMPING_UNCLAMPED)
        return PIX_FMT_YUVJ422P;
      return PIX_FMT_YUV422P;
    case WEED_PALETTE_YUV420P:
      if (clamped && *clamped == WEED_YUV_CLAMPING_UNCLAMPED)
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
  }

#else

  int avi_pix_fmt_to_weed_palette(enum AVPixelFormat pix_fmt, int *clamped) {
  // clamped may be set to NULL if you are not interested in the value
  switch (pix_fmt) {
  case AV_PIX_FMT_RGB24:
    return WEED_PALETTE_RGB24;
  case AV_PIX_FMT_BGR24:
    return WEED_PALETTE_BGR24;
  case AV_PIX_FMT_RGBA:
    return WEED_PALETTE_RGBA32;
  case AV_PIX_FMT_BGRA:
    return WEED_PALETTE_BGRA32;
  case AV_PIX_FMT_ARGB:
    return WEED_PALETTE_ARGB32;
  case AV_PIX_FMT_YUV444P:
    return WEED_PALETTE_YUV444P;
  case AV_PIX_FMT_YUVA444P:
    return WEED_PALETTE_YUVA4444P;
  case AV_PIX_FMT_YUV422P:
    return WEED_PALETTE_YUV422P;
  case AV_PIX_FMT_YUV420P:
    return WEED_PALETTE_YUV420P;
  case AV_PIX_FMT_YUYV422:
    return WEED_PALETTE_YUYV;
  case AV_PIX_FMT_UYVY422:
    return WEED_PALETTE_UYVY;
  case AV_PIX_FMT_UYYVYY411:
    return WEED_PALETTE_YUV411;
  case AV_PIX_FMT_GRAY8:
    return WEED_PALETTE_A8;
  case AV_PIX_FMT_MONOWHITE:
  case AV_PIX_FMT_MONOBLACK:
    return WEED_PALETTE_A1;
  case AV_PIX_FMT_YUVJ422P:
    if (clamped) *clamped = WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV422P;
  case AV_PIX_FMT_YUVJ444P:
    if (clamped) *clamped = WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV444P;
  case AV_PIX_FMT_YUVJ420P:
    if (clamped) *clamped = WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV420P;

  default:
    return WEED_PALETTE_END;
  }
}
enum AVPixelFormat weed_palette_to_avi_pix_fmt(int pal, int *clamped) {
  switch (pal) {
  case WEED_PALETTE_RGB24:
        return AV_PIX_FMT_RGB24;
    case WEED_PALETTE_BGR24:
      return AV_PIX_FMT_BGR24;
    case WEED_PALETTE_RGBA32:
      return AV_PIX_FMT_RGBA;
    case WEED_PALETTE_BGRA32:
      return AV_PIX_FMT_BGRA;
    case WEED_PALETTE_ARGB32:
      return AV_PIX_FMT_ARGB;
    case WEED_PALETTE_YUV444P:
      if (clamped && *clamped == WEED_YUV_CLAMPING_UNCLAMPED)
        return AV_PIX_FMT_YUVJ444P;
      return AV_PIX_FMT_YUV444P;
    case WEED_PALETTE_YUVA4444P:
      return AV_PIX_FMT_YUVA444P;
    case WEED_PALETTE_YUV422P:
      if (clamped && *clamped == WEED_YUV_CLAMPING_UNCLAMPED)
        return AV_PIX_FMT_YUVJ422P;
      return AV_PIX_FMT_YUV422P;
    case WEED_PALETTE_YUV420P:
      if (clamped && *clamped == WEED_YUV_CLAMPING_UNCLAMPED)
        return AV_PIX_FMT_YUVJ420P;
      return AV_PIX_FMT_YUV420P;
    case WEED_PALETTE_YUYV:
      return AV_PIX_FMT_YUYV422;
    case WEED_PALETTE_UYVY:
      return AV_PIX_FMT_UYVY422;
    case WEED_PALETTE_YUV411:
      return AV_PIX_FMT_UYYVYY411;

    case WEED_PALETTE_A8:
      return AV_PIX_FMT_GRAY8;
    case WEED_PALETTE_A1:
      return AV_PIX_FMT_MONOBLACK;

    default:
      return AV_PIX_FMT_NONE;
    }
  }
#endif

  int avi_trc_to_weed_gamma(enum AVColorTransferCharacteristic trc) {
  switch (trc) {
  case AVCOL_TRC_BT709:
    return WEED_GAMMA_BT709;
  case  AVCOL_TRC_LINEAR:
    return WEED_GAMMA_LINEAR;
  case AVCOL_TRC_GAMMA22:
    return WEED_GAMMA_SRGB;
  default:
    break;
  }
  return WEED_GAMMA_UNKNOWN;
}

enum AVColorTransferCharacteristic weed_gamma_to_avi_trc(int gamma_type) {
  switch (gamma_type) {
  case WEED_GAMMA_BT709:
        return AVCOL_TRC_BT709;
    case  WEED_GAMMA_LINEAR:
      return AVCOL_TRC_LINEAR;
    case WEED_GAMMA_SRGB:
      return AVCOL_TRC_GAMMA22;
    default:
      break;
    }
    return AVCOL_TRC_UNSPECIFIED;
  }

#endif

#endif // HAVE_AVUTIL
#endif // HAVE_AVCODEC

#ifndef WEED_AVUTIL_CHANNEL_LAYOUTS

#ifdef USE_AVUTIL_CHANNEL_LAYOUTS
#ifndef AVUTIL_CHANNEL_LAYOUT_H
#ifdef HAVE_AVUTIL
#include <libavutil/channel_layout.h>
#endif
#endif

#ifdef AVUTIL_CHANNEL_LAYOUT_H
#define WEED_CH_FRONT_LEFT	 			AV_CH_FRONT_LEFT
#define WEED_CH_FRONT_RIGHT	 			AV_CH_FRONT_RIGHT
#define WEED_CH_FRONT_CENTER 				AV_CH_FRONT_CENTER
#define WEED_CH_LOW_FREQUENCY 			AV_CH_LOW_FREQUENCY
#define WEED_CH_BACK_LEFT 				AV_CH_BACK_LEFT
#define WEED_CH_BACK_RIGHT				AV_CH_BACK_RIGHT
#define WEED_CH_FRONT_LEFT_OF_CENTER  		AV_CH_FRONT_LEFT_OF_CENTER
#define WEED_CH_FRONT_RIGHT_OF_CENTER 	AV_CH_FRONT_RIGHT_OF_CENTER
#define WEED_CH_BACK_CENTER 				AV_CH_BACK_CENTER
#define WEED_CH_SIDE_LEFT 					AV_CH_SIDE_LEFT
#define WEED_CH_SIDE_RIGHT 				AV_CH_SIDE_RIGHT
#define WEED_CH_TOP_CENTER 				AV_CH_TOP_CENTER
#define WEED_CH_TOP_FRONT_LEFT 			AV_CH_TOP_FRONT_LEFT
#define WEED_CH_TOP_FRONT_CENTER 			AV_CH_TOP_FRONT_CENTER
#define WEED_CH_TOP_FRONT_RIGHT 			AV_CH_TOP_FRONT_RIGHT
#define WEED_CH_TOP_BACK_LEFT 			AV_CH_TOP_BACK_LEFT
#define WEED_CH_TOP_BACK_CENTER 			AV_CH_TOP_BACK_CENTER
#define WEED_CH_TOP_BACK_RIGHT 			AV_CH_TOP_BACK_RIGHT
#define WEED_CH_STEREO_LEFT 				AV_CH_STEREO_LEFT
#define WEED_CH_STEREO_RIGHT 				AV_CH_STEREO_RIGHT
#define WEED_CH_WIDE_LEFT 				AV_CH_WIDE_LEFT
#define WEED_CH_WIDE_RIGHT 				AV_CH_WIDE_RIGHT
#define WEED_CH_SURROUND_DIRECT_LEFT     	AV_CH_SURROUND_DIRECT_LEFT
#define WEED_CH_SURROUND_DIRECT_RIGHT 	AV_CH_SURROUND_DIRECT_RIGHT
#define WEED_CH_LOW_FREQUENCY_2 			AV_CH_LOW_FREQUENCY_2

#ifdef WEED_CH_LAYOUT_MONO
#undef WEED_CH_LAYOUT_MONO
#endif

#ifdef WEED_CH_LAYOUT_STEREO
#undef WEED_CH_LAYOUT_STEREO
#endif

#define WEED_CH_LAYOUT_MONO 					AV_CH_LAYOUT_MONO
#define WEED_CH_LAYOUT_STEREO 				AV_CH_LAYOUT_STEREO
#define WEED_CH_LAYOUT_2POINT1 				AV_CH_LAYOUT_2POINT1
#define WEED_CH_LAYOUT_2_1 					AV_CH_LAYOUT_2_1
#define WEED_CH_LAYOUT_SURROUND 				AV_CH_LAYOUT_SURROUND
#define WEED_CH_LAYOUT_3POINT1 				AV_CH_LAYOUT_3POINT1
#define WEED_CH_LAYOUT_4POINT0 				AV_CH_LAYOUT_4POINT0
#define WEED_CH_LAYOUT_4POINT1 				AV_CH_LAYOUT_4POINT1
#define WEED_CH_LAYOUT_2_2 					AV_CH_LAYOUT_2_2
#define WEED_CH_LAYOUT_QUAD 					AV_CH_LAYOUT_QUAD
#define WEED_CH_LAYOUT_5POINT0 				AV_CH_LAYOUT_5POINT0
#define WEED_CH_LAYOUT_5POINT1 				AV_CH_LAYOUT_5POINT1
#define WEED_CH_LAYOUT_5POINT0_BACK 			AV_CH_LAYOUT_5POINT0_BACK
#define WEED_CH_LAYOUT_5POINT1_BACK 			AV_CH_LAYOUT_5POINT1_BACK
#define WEED_CH_LAYOUT_6POINT0 				AV_CH_LAYOUT_6POINT0
#define WEED_CH_LAYOUT_6POINT0_FRONT 			AV_CH_LAYOUT_6POINT0_FRONT
#define WEED_CH_LAYOUT_HEXAGONAL 				AV_CH_LAYOUT_HEXAGONAL
#define WEED_CH_LAYOUT_6POINT1 				AV_CH_LAYOUT_6POINT1
#define WEED_CH_LAYOUT_6POINT1_BACK 			AV_CH_LAYOUT_6POINT1_BACK
#define WEED_CH_LAYOUT_6POINT1_FRONT 			AV_CH_LAYOUT_6POINT1_FRONT
#define WEED_CH_LAYOUT_7POINT0 				AV_CH_LAYOUT_7POINT0
#define WEED_CH_LAYOUT_7POINT0_FRONT 			AV_CH_LAYOUT_7POINT0_FRONT
#define WEED_CH_LAYOUT_7POINT1 				AV_CH_LAYOUT_7POINT1
#define WEED_CH_LAYOUT_7POINT1_WIDE 			AV_CH_LAYOUT_7POINT1_WIDE
#define WEED_CH_LAYOUT_7POINT1_WIDE_BACK		AV_CH_LAYOUT_7POINT1_WIDE_BACK
#define WEED_CH_LAYOUT_OCTAGONAL 			AV_CH_LAYOUT_OCTAGONAL
#define WEED_CH_LAYOUT_HEXADECAGONAL 		AV_CH_LAYOUT_HEXADECAGONAL
#define WEED_CH_LAYOUT_STEREO_DOWNMIX 		AV_CH_LAYOUT_STEREO_DOWNMIX

#ifndef WEED_CH_LAYOUT_DEFAULT_1
#define WEED_CH_LAYOUT_DEFAULT_1 WEED_CH_LAYOUT_MONO
#endif
#ifndef WEED_CH_LAYOUT_DEFAULT_2
#define WEED_CH_LAYOUT_DEFAULT_2 WEED_CH_LAYOUT_STEREO
#endif
#ifndef WEED_CH_LAYOUTS_DEFAULT
#define WEED_CH_LAYOUTS_DEFAULT (WEED_CH_LAYOUT_DEFAULT_2, WEED_CH_LAYOUT_DEFAULT_1}
#endif
#ifndef WEED_CH_LAYOUTS_DEFAULT_MIN2
#define WEED_CH_LAYOUTS_DEFAULT_MIN2 (WEED_CH_LAYOUT_DEFAULT_2}
#endif

#define WEED_AVUTIL_CHANNEL_LAYOUTS

#ifdef WEED_CHANNEL_LAYOUT_TYPE
#undef WEED_CHANNEL_LAYOUT_TYPE
#endif

#define WEED_CHANNEL_LAYOUT_TYPE "avutil"

#endif
#endif
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_COMPAT_H__
