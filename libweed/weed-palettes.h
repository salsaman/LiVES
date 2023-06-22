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

/* (C) G. Finch, 2005 - 2023 */

#ifndef __WEED_PALETTES_H__
#define __WEED_PALETTES_H__

#define WEED_PALETTES_VERSION 201

/* Palette types */

// special values
#define WEED_PALETTE_ANY -1

#define WEED_PALETTE_NONE 0
#define WEED_PALETTE_END WEED_PALETTE_NONE
#define WEED_PALETTE_INVALID WEED_PALETTE_NONE

/* RGB palettes */
#define WEED_PALETTE_RGB24 1
#define WEED_PALETTE_RGB888 WEED_PALETTE_RGB24
#define WEED_PALETTE_BGR24 2
#define WEED_PALETTE_BGR888 WEED_PALETTE_BGR24
#define WEED_PALETTE_RGBA32 3
#define WEED_PALETTE_RGBA8888 WEED_PALETTE_RGBA32
#define WEED_PALETTE_BGRA32 4
#define WEED_PALETTE_BGRA8888 WEED_PALETTE_BGRA32
#define WEED_PALETTE_ARGB32 5
#define WEED_PALETTE_ARGB8888 WEED_PALETTE_ARGB32

#define WEED_PALETTE_RGBFLOAT 64
#define WEED_PALETTE_RGBAFLOAT  65

/* YUV palettes */
// planar
#define WEED_PALETTE_YUV420P 512
#define WEED_PALETTE_I420 WEED_PALETTE_YUV420P
#define WEED_PALETTE_IYUV WEED_PALETTE_YUV420P
#define WEED_PALETTE_YU12 WEED_PALETTE_YUV420P

#define WEED_PALETTE_YVU420P 513
#define WEED_PALETTE_YV12 WEED_PALETTE_YVU420P

#define WEED_PALETTE_YUV422P 522
#define WEED_PALETTE_P422 WEED_PALETTE_YUV422P

#define WEED_PALETTE_YUV444P 544

#define WEED_PALETTE_YUVA4444P 545

// packed
#define WEED_PALETTE_UYVY 564
#define WEED_PALETTE_UYVY8888 WEED_PALETTE_UYVY
#define WEED_PALETTE_UYVY422 WEED_PALETTE_UYVY
#define WEED_PALETTE_Y422 WEED_PALETTE_UYVY
#define WEED_PALETTE_HDYC WEED_PALETTE_UYVY /// UYVY with  bt,709 subspace

#define WEED_PALETTE_YUYV 565
#define WEED_PALETTE_YUYV8888 WEED_PALETTE_YUYV
#define WEED_PALETTE_YUYV422 WEED_PALETTE_YUYV
#define WEED_PALETTE_YUY2 WEED_PALETTE_YUYV

#define WEED_PALETTE_YUV888 588
#define WEED_PALETTE_IYU2 WEED_PALETTE_YUV888

#define WEED_PALETTE_YUVA8888 589

#define WEED_PALETTE_YUV411 595
#define WEED_PALETTE_IYU1 WEED_PALETTE_YUV411

/* Alpha palettes */
#define WEED_PALETTE_A8 1024
#define WEED_PALETTE_A1 1025
#define WEED_PALETTE_AFLOAT 1064

#define WEED_PALETTE_FIRST_CUSTOM 8192

#define ALL_RGB_PALETTES {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, \
      WEED_PALETTE_ARGB32, WEED_PALETTE_END}

#define ALL_RGBX_PALETTES \
  {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_END}

#define ALL_24BIT_PALETTES {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_YUV888, WEED_PALETTE_END}

#define ALL_32BIT_PALETTES {WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_ARGB32, WEED_PALETTE_YUVA8888,\
      WEED_PALETTE_END}

#define _ALL_ALPHA_PALETTES WEED_PALETTE_AFLOAT, WEED_PALETTE_A8, WEED_PALETTE_A1

#define ALL_ALPHA_PALETTES {_ALL_ALPHA_PALETTES. WEED_PALETTE_END}

/*( omits WEED_PALETTE_YUV411, WEED_PALETTE_UYVY, WEED_PALETTE_YUYV, WEED_PALETTE_RGB_FLOAT
   and WEED_PALETTE_RGBA_FLOAT as well as the alpha palettes
  WEED_PALETTE_A1, WEED_PALETTE_A8 and WEED_PALETTE_AFLOAT also WEED_PALETTE_ARGB32 so that alpha is always last*/
#define ALL_PACKED_PALETTES q{WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32, \
      WEED_PALETTE_BGRA32, WEED_PALETTE_YUV888, WEED_PALETTE_YUVA8888, WEED_PALETTE_END}

/* adds WEED_PALETTE_ARGB32, WEED_PALETTE_UYVY8888. WEED_PALETTE_YUYV8888 */
#define _ALL_PACKED_PALETTES_PLUS WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32, \
      WEED_PALETTE_BGRA32, WEED_PALETTE_ARGB32, WEED_PALETTE_YUV888, WEED_PALETTE_YUVA8888, WEED_PALETTE_UYVY, \
      WEED_PALETTE_YUYV

#define ALL_PACKED_PALETTES_PLUS {_ALL_PACKED_PALETTES_PLUS, WEED_PALETTE_END}

#define _ALL_PLANAR_PALETTES WEED_PALETTE_YUV444P, WEED_PALETTE_YUVA4444P, WEED_PALETTE_YUV422P, \
                        WEED_PALETTE_YUV420P, WEED_PALETTE_YVU420P

#define ALL_PLANAR_PALETTES {_ALL_PLANAR_PALETTES,  WEED_PALETTE_END}

#define ALL_STANDARD_PALETTES {_ALL_PACKED_PALETTES_PLUS, _ALL_PLANAR_PALETTES, WEED_PALETTE_END}

#define ALL_WEED_PALETTES {_ALL_PACKED_PALETTES_PLUS, WEED_PALETTE_YUV411, WEED_PALETTE_RGBFLOAT,\
			   WEED_PALETTE_RGBAFLOAT, WEED_PALETTE_YUV411, _ALL_PLANAR_PALETTES, \
			   _ALL_ALPHA_PALETTES, WEED_PALETTE_END}
/* YUV sampling types */
// see http://www.mir.com/DMG/chroma.html
#define WEED_YUV_SAMPLING_DEFAULT   0
#define WEED_YUV_SAMPLING_JPEG  0   ///< jpeg/mpeg1 - samples centered horizontally: 0.5, 2.5 etc.
#define WEED_YUV_SAMPLING_MPEG   1 ///< mpeg2 - samples aligned horizontally: left 0,2,4 etc;
#define WEED_YUV_SAMPLING_DVPAL  2 ///< separated Cb and Cr
#define WEED_YUV_SAMPLING_DVNTSC  3 ///< not used - only for 411 planar

#define WEED_YUV_SAMPLING_FIRST_CUSTOM 512

/* YUV clamping types */
#define WEED_YUV_CLAMPING_CLAMPED 0
#define WEED_YUV_CLAMPING_MPEG WEED_YUV_CLAMPING_CLAMPED
#define WEED_YUV_CLAMPING_UNCLAMPED 1
#define WEED_YUV_CLAMPING_JPEG WEED_YUV_CLAMPING_UNCLAMPED

#define WEED_YUV_CLAMPING_FIRST_CUSTOM 512

/* YUV subspace types */
#define WEED_YUV_SUBSPACE_YUV 0
#define WEED_YUV_SUBSPACE_YCBCR 1
#define WEED_YUV_SUBSPACE_BT709 2
#define WEED_YUV_SUBSPACE_ITU709 WEED_YUV_SUBSPACE_BT709

#define WEED_YUV_SUBSPACE_FIRST_CUSTOM 512

/* GAMMA Values */
/* API version 200 */
#define WEED_GAMMA_UNKNOWN 0
#define WEED_GAMMA_LINEAR -1
#define WEED_GAMMA_SRGB 1
#define WEED_GAMMA_BT709 2

#define WEED_GAMMA_FIRST_CUSTOM 512

/// advanced palettes extension (experimental)

#ifdef WEED_ADVANCED_PALETTES
#define WEED_ADVANCED_PALETTES_VERSION 100

// if desired, the "palette_list" for a filter or channel_template may include WEED_PALETTE_ADVANCED.
// In this case, an additional filter_class leaf "adv_pal_list",
// an array of type WEED_SEED_VOIDPTR should be set for the filter_class or channel_template
// the values should point to statically allocated weed_macropixel_t structs.
// the usual rules about channel_templates overriding filter_class, and palette matching / conversion
// still apply.
//
// in response, the host may set the "current_palette" for a channel to WEED_PALETTE_ADVANCED,
// and set the additional leaf "adv_palette" with type WEED_SEED_VOIDPTR to point to the
// weed_macropixel_t struct selected.

#define WEED_PALETTE_ADVANCED 8191

#define WEED_LEAF_ADVANCED_PALETTE_LIST "adv_pal_list"
#define WEED_LEAF_ADVANCED_PALETTE "adv_palette"

#ifndef MAXPPLANES
#define MAXPPLANES 8
#endif

#define WEED_VCHAN_end 		0

#define WEED_VCHAN_red 		1
#define WEED_VCHAN_green      	2
#define WEED_VCHAN_blue	       	3

#define WEED_VCHAN_Y			512
#define WEED_VCHAN_U			513
#define WEED_VCHAN_V			514

#define WEED_VCHAN_alpha		1024

#define WEED_VCHAN_FIRST_CUSTOM		8192

#define WEED_VCHAN_DESC_PLANAR		(1 << 0) ///< planar type
#define WEED_VCHAN_DESC_FP		(1 << 1) ///< floating point type
#define WEED_VCHAN_DESC_BE		(1 << 1) ///< pixel data is big endian (within each component)

#define WEED_VCHAN_DESC_FIRST_CUSTOM	(1 << 16) ///< floating point type

typedef struct {
  uint16_t ext_ref;  ///< link to an enumerated type
  uint16_t chantype[MAXPPLANES]; ///  e.g. {WEED_VCHAN_U, WEED_VCHAN_Y, WEED_VCHAN_V, WEED_VCHAN_Y)
  uint32_t flags; /// bitmap of flags, eg. WEED_VCHAN_DESC_FP | WEED_VCHAN_DESC_PLANAR
  uint8_t  hsub[MAXPPLANES];  /// horiz. subsampling, 0 or 1 means no subsampling, 2 means halved etc. (planar only)
  uint8_t  vsub[MAXPPLANES];  /// vert subsampling
  uint8_t npixels; ///< npixels per macropixel: {0, 1} == 1
  uint8_t bitsize[MAXPPLANES];
  void *extended; ///< pointer to app defined data
} weed_macropixel_t;
#endif

#endif
