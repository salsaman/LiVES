/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 * LifeTV - Play John Horton Conway's `Life' game with video input.
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 * This idea is stolen from Nobuyuki Matsushita's installation program of
 * ``HoloWall''. (See http://www.csl.sony.co.jp/person/matsu/index.html)
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

#include <strings.h>

typedef unsigned int RGB32;

#define PIXEL_SIZE (sizeof(RGB32))

struct _sdata {
  unsigned char *field;
  unsigned char *field1;
  unsigned char *field2;
  short *background;
  unsigned char *diff;
  unsigned char *diff2;
  int threshold;
};



/* Background image is refreshed every frame */
static void image_bgsubtract_update_y(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
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
}


/* noise filter for subtracted image. */
void image_diff_filter(struct _sdata *sdata, int width, int height) {
  int x, y;
  unsigned char *src, *dest;
  unsigned int count;
  unsigned int sum1, sum2, sum3;

  src = sdata->diff;
  dest = sdata->diff2 + width +1;
  for (y=1; y<height-1; y++) {
    sum1 = src[0] + src[width] + src[width*2];
    sum2 = src[1] + src[width+1] + src[width*2+1];
    src += 2;
    for (x=1; x<width-1; x++) {
      sum3 = src[0] + src[width] + src[width*2];
      count = sum1 + sum2 + sum3;
      sum1 = sum2;
      sum2 = sum3;
      *dest++ = (0xff*3 - count)>>24;
      src++;
    }
    dest += 2;
  }
}

static void clear_field(struct _sdata *sdata, int video_area) {
  bzero(sdata->field1, video_area);
}

/////////////////////////////////////////////////////////////

int lifetv_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int height,width,video_area;
  weed_plant_t *in_channel;
  int error;

  sdata=weed_malloc(sizeof(struct _sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  height=weed_get_int_value(in_channel,"height",&error);
  width=weed_get_int_value(in_channel,"width",&error);

  video_area=width*height;

  sdata->field = (unsigned char *)weed_malloc(video_area*2);
  if (sdata->field == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->diff = (unsigned char *)weed_malloc(video_area*sizeof(unsigned char));

  if (sdata->diff == NULL) {
    weed_free(sdata->field);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->diff2 = (unsigned char *)weed_malloc(video_area*sizeof(unsigned char));

  if (sdata->diff2 == NULL) {
    weed_free(sdata->diff);
    weed_free(sdata->field);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }


  sdata->background = (short *)weed_malloc(video_area*sizeof(short));
  if (sdata->background == NULL) {
    weed_free(sdata->field);
    weed_free(sdata->diff);
    weed_free(sdata->diff2);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  if (sdata->background == NULL) {
    weed_free(sdata->field);
    weed_free(sdata->diff);
    weed_free(sdata->diff2);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->diff, 0, video_area*sizeof(unsigned char));

  sdata->threshold=280;
  sdata->field1 = sdata->field;
  sdata->field2 = sdata->field + video_area;
  clear_field(sdata,video_area);
  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


int lifetv_deinit(weed_plant_t *inst) {
  struct _sdata *sdata;
  int error;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata != NULL) {
    weed_free(sdata->background);
    weed_free(sdata->diff);
    weed_free(sdata->diff2);
    weed_free(sdata->field);
    weed_free(sdata);
  }

  return WEED_NO_ERROR;
}


int lifetv_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  struct _sdata *sdata;

  weed_plant_t *in_channel,*out_channel;

  unsigned char *p, *q, v;
  unsigned char sum, sum1, sum2, sum3;

  RGB32 *src,*dest;

  RGB32 pix;

  int width,height,video_area,irow,orow;
  int error;
  register int x, y;


  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  dest=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  width = weed_get_int_value(in_channel,"width",&error);
  height = weed_get_int_value(in_channel,"height",&error);

  irow = weed_get_int_value(in_channel,"rowstrides",&error)/4;
  orow = weed_get_int_value(out_channel,"rowstrides",&error)/4-width;

  video_area=width*height;

  image_bgsubtract_update_y(src,width,height,irow,sdata);
  image_diff_filter(sdata,width,height);
  p=sdata->diff2;

  irow-=width;

  for (x=0; x<video_area; x++) {
    sdata->field1[x] |= p[x];
  }

  p = sdata->field1 + 1;
  q = sdata->field2 + width + 1;
  dest += width + 1;
  src += width + 1;
  /* each value of cell is 0 or 0xff. 0xff can be treated as -1, so
   * following equations treat each value as negative number. */
  for (y=1; y<height-1; y++) {
    sum1 = 0;
    sum2 = p[0] + p[width] + p[width*2];
    for (x=1; x<width-1; x++) {
      sum3 = p[1] + p[width+1] + p[width*2+1];
      sum = sum1 + sum2 + sum3;
      v = 0 - ((sum==0xfd)|((p[width]!=0)&(sum==0xfc)));
      *q++ = v;
      pix = (signed char)v;
      //			pix = pix >> 8;
      *dest++ = pix | *src++;
      sum1 = sum2;
      sum2 = sum3;
      p++;
    }
    p += 2;
    q += 2;
    src += 2+irow;
    dest += 2+orow;
  }
  p = sdata->field1;
  sdata->field1 = sdata->field2;
  sdata->field2 = p;

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("lifeTV","effectTV",1,0,&lifetv_init,&lifetv_process,&lifetv_deinit,in_chantmpls,
                               out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

