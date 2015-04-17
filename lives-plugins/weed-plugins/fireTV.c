/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 * FireTV - clips incoming objects and burn them.
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 * Fire routine is taken from Frank Jan Sorensen's demo program.
 */

/* modified for Weed by G. Finch (salsaman)
   modifications (c) G. Finch */

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

#include <stdio.h>
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

//////////////////////////////////////////////

typedef unsigned int RGB32;

static RGB32 palette[256];

#define MaxColor 120
#define Decay 15
#define MAGIC_THRESHOLD 50


struct _sdata {
  unsigned char *buffer;
  short *background;
  unsigned char *diff;
  int threshold;
  uint32_t fastrand_val;
};


//////////////////////////////////////////////////////

inline uint32_t fastrand(struct _sdata *sdata) {
#define rand_a 1073741789L
#define rand_c 32749L

  return ((sdata->fastrand_val = (rand_a * sdata->fastrand_val + rand_c)));
}



static void HSItoRGB(double H, double S, double I, int *r, int *g, int *b) {
  double T,Rv,Gv,Bv;

  T=H;
  Rv=1+S*sin(T-2*M_PI/3);
  Gv=1+S*sin(T);
  Bv=1+S*sin(T+2*M_PI/3);
  T=255.1009*I/2;
  *r=trunc(Rv*T);
  *g=trunc(Gv*T);
  *b=trunc(Bv*T);
}


static void makePalette() {
  int i, r, g, b;

  for (i=0; i<MaxColor; i++) {
    HSItoRGB(4.6-1.5*i/MaxColor, (double)i/MaxColor, (double)i/MaxColor, &r, &g, &b);
    palette[i] = ((r<<16)|(g<<8)|b)&0xffffff;
  }
  for (i=MaxColor; i<256; i++) {
    if (r<255)r++;
    if (r<255)r++;
    if (r<255)r++;
    if (g<255)g++;
    if (g<255)g++;
    if (b<255)b++;
    if (b<255)b++;
    palette[i] = ((r<<16)|(g<<8)|b)&0xffffff;
  }
}


static void image_bgsubtract_y(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
  register int i,j;
  int R, G, B;
  RGB32 *p;
  short *q;
  unsigned char *r;
  int v;

  rowstride-=width;

  p = src;
  q = sdata->background;
  r = sdata->diff;
  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      R = ((*p)&0xff0000)>>(16-1);
      G = ((*p)&0xff00)>>(8-2);
      B = (*p)&0xff;
      v = (R + G + B) - (int)(*q);
      *q = (short)(R + G + B);
      *r = ((v + sdata->threshold)>>24) | ((sdata->threshold - v)>>24);

      p++;
      q++;
      r++;
    }
    p+=rowstride;
  }


  /* The origin of subtraction function is;
   * diff(src, dest) = (abs(src - dest) > threshold) ? 0xff : 0;
   *
   * This functions is transformed to;
   * (threshold > (src - dest) > -threshold) ? 0 : 0xff;
   *
   * (v + threshold)>>24 is 0xff when v is less than -threshold.
   * (v - threshold)>>24 is 0xff when v is less than threshold.
   * So, ((v + threshold)>>24) | ((threshold - v)>>24) will become 0xff when
   * abs(src - dest) > threshold.
   */
}



int fire_init(weed_plant_t *inst) {
  int error;
  int map_h;
  int map_w;
  struct _sdata *sdata;

  weed_plant_t *in_channel;

  sdata=weed_malloc(sizeof(struct _sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  map_h=weed_get_int_value(in_channel,"height",&error);
  map_w=weed_get_int_value(in_channel,"width",&error);

  sdata->buffer = (unsigned char *)weed_malloc(map_h*map_w*sizeof(unsigned char));
  if (sdata->buffer == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->background = (short *)weed_malloc(map_h*map_w*sizeof(short));
  if (sdata->background == NULL) {
    weed_free(sdata->buffer);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->diff = (unsigned char *)weed_malloc(map_h*map_w * sizeof(unsigned char));
  if (sdata->diff == NULL) {
    weed_free(sdata->background);
    weed_free(sdata->buffer);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->threshold=MAGIC_THRESHOLD*7;
  weed_memset(sdata->buffer, 0, map_h*map_w*sizeof(unsigned char));
  sdata->fastrand_val=0;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);
  return WEED_NO_ERROR;
}


int fire_deinit(weed_plant_t *inst) {
  int error;
  struct _sdata *sdata;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata != NULL) {
    weed_free(sdata->buffer);
    weed_free(sdata->diff);
    weed_free(sdata->background);
    weed_free(sdata);
    weed_set_voidptr_value(inst,"plugin_internal",NULL);
  }
  return WEED_NO_ERROR;
}


int fire_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  struct _sdata *sdata;

  unsigned char v;
  weed_plant_t *in_channel,*out_channel;

  RGB32 *src,*dest;

  int video_width,video_height,irow,orow;
  int video_area;
  int error;

  register int i, x, y;


  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  dest=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  video_width = weed_get_int_value(in_channel,"width",&error);
  video_height = weed_get_int_value(in_channel,"height",&error);

  irow = weed_get_int_value(in_channel,"rowstrides",&error)/4;
  orow = weed_get_int_value(out_channel,"rowstrides",&error)/4;

  video_area=video_width*video_height;
  sdata->fastrand_val=timestamp&0x0000FFFF;

  image_bgsubtract_y(src,video_width,video_height,irow,sdata);

  for (i=0; i<video_area-video_width; i++) {
    sdata->buffer[i] |= sdata->diff[i];
  }
  for (x=1; x<video_width-1; x++) {
    i = video_width + x;
    for (y=1; y<video_height; y++) {
      v = sdata->buffer[i];
      if (v<Decay)
        sdata->buffer[i-video_width] = 0;
      else
        sdata->buffer[i-video_width+fastrand(sdata)%3-1] = v - (fastrand(sdata)&Decay);
      i += video_width;
    }
  }
  for (y=0; y<video_height; y++) {
    for (x=1; x<video_width-1; x++) {
      dest[y*orow+x] = (src[y*irow+x]&0xff000000)|palette[sdata->buffer[y*video_width+x]];
    }
  }

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGRA32,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("fireTV","effectTV",1,0,&fire_init,&fire_process,&fire_deinit,in_chantmpls,out_chantmpls,
                               NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

    makePalette();
  }
  return plugin_info;
}

