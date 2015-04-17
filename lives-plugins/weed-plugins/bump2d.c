// Based on code Copyright (C) 2002 W.P. van Paassen - peter@paassen.tmfweb.nl
// This effect was inspired by an article by Sqrt(-1) */
// 08-22-02 Optimized by WP
// bump2d.c - weed plugin
// (c) G. Finch (salsaman) 2006
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]= {131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////
#include <math.h>

static short aSin[512];
static uint8_t reflectionmap[256][256];

typedef struct {
  uint16_t sin_index;
  uint16_t sin_index2;
} _sdata;

typedef struct {
  short x, y;
} BUMP;


#define ABS(a)           (((a) < 0) ? -(a) : (a))

/* precomputed tables */
#define FP_BITS 16

static int Y_R[256];
static int Y_G[256];
static int Y_B[256];


static int myround(double n) {
  if (n >= 0)
    return (int)(n + 0.5);
  else
    return (int)(n - 0.5);
}


static void init_RGB_to_YCbCr_tables(void) {
  int i;

  /*
   * Q_Z[i] =   (coefficient * i
   *             * (Q-excursion) / (Z-excursion) * fixed-pogint-factor)
   *
   * to one of each, add the following:
   *             + (fixed-pogint-factor / 2)         --- for rounding later
   *             + (Q-offset * fixed-pogint-factor)  --- to add the offset
   *
   */
  for (i = 0; i < 256; i++) {
    Y_R[i] = myround(0.299 * (double)i
                     * 219.0 / 255.0 * (double)(1<<FP_BITS));
    Y_G[i] = myround(0.587 * (double)i
                     * 219.0 / 255.0 * (double)(1<<FP_BITS));
    Y_B[i] = myround((0.114 * (double)i
                      * 219.0 / 255.0 * (double)(1<<FP_BITS))
                     + (double)(1<<(FP_BITS-1))
                     + (16.0 * (double)(1<<FP_BITS)));

  }
}


static inline uint8_t
calc_luma(uint8_t *pixel) {
  return (Y_R[pixel[2]] + Y_G[pixel[1]]+ Y_B[pixel[0]]) >> FP_BITS;
}


int bumpmap_init(weed_plant_t *inst) {
  _sdata *sdata=(_sdata *)weed_malloc(sizeof(_sdata));
  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->sin_index=0;
  sdata->sin_index2=80;
  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


int bumpmap_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata!=NULL) {
    weed_free(sdata);
    weed_set_voidptr_value(inst,"plugin_internal",NULL);
  }

  return WEED_NO_ERROR;
}


int bumpmap_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int width3=width*3;
  _sdata *sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  uint16_t lightx, lighty, temp;
  short normalx, normaly, x, y;
  uint8_t *s1;

  BUMP bumpmap[width][height];

  /* create bump map */
  for (x = 0; x < width - 1; x++) {
    for (y = 1; y < height - 1; y++) {
      bumpmap[x][y].x = calc_luma(&src[y*irowstride+x*3+3])-calc_luma(&src[y*irowstride+x*3]);
      bumpmap[x][y].y = calc_luma(&src[y*irowstride+x*3])-calc_luma(&src[(y-1)*irowstride+x*3]);
    }
  }

  lightx = aSin[sdata->sin_index];
  lighty = aSin[sdata->sin_index2];

  weed_memset(dst,0,orowstride);
  s1 = dst + orowstride;

  orowstride-=width3-3;

  for (y = 1; y < height - 1; ++y) {
    temp = lighty - y;
    weed_memset(s1,0,3);
    s1+=3;

    for (x = 1; x < width - 1; x++) {
      normalx = bumpmap[x][y].x + lightx - x;
      normaly = bumpmap[x][y].y + temp;

      if (normalx < 0)
        normalx = 0;
      else if (normalx > 255)
        normalx = 0;
      if (normaly < 0)
        normaly = 0;
      else if (normaly > 255)
        normaly = 0;

      weed_memset(s1,reflectionmap[normalx][normaly],3);
      s1+=3;
    }
    weed_memset(s1,0,3);
    s1+=orowstride;
  }

  weed_memset(s1,0,orowstride+width3-3);

  sdata->sin_index += 3;
  sdata->sin_index &= 511;
  sdata->sin_index2 += 5;
  sdata->sin_index2 &= 511;

  return WEED_NO_ERROR;
}


void bumpmap_x_init(void) {
  int i;
  short x, y;
  float rad;

  /*create sin lookup table */
  for (i = 0; i < 512; i++) {
    rad = (float)i * 0.0174532 * 0.703125;
    aSin[i] = (short)((sin(rad) * 100.0) + 256.0);
  }

  /* create reflection map */

  for (x = 0; x < 256; ++x) {
    for (y = 0; y < 256; ++y) {
      float X = (x - 128) / 128.0;
      float Y = (y - 128) / 128.0;
      float Z =  1.0 - sqrt(X * X + Y * Y);
      Z *= 255.0;
      if (Z < 0.0)
        Z = 0.0;
      reflectionmap[x][y] = Z;
    }
  }

}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("bumpmap","salsaman",1,0,&bumpmap_init,&bumpmap_process,&bumpmap_deinit,in_chantmpls,
                               out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

    bumpmap_x_init();
    init_RGB_to_YCbCr_tables();
  }
  return plugin_info;
}

