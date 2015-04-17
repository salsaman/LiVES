/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001-2005 FUKUCHI Kentaro
 *
 * RippleTV - Water ripple effect.
 * Copyright (C) 2001-2002 FUKUCHI Kentaro
 *
 */

/* modified for Weed by G. Finch (salsaman)
   modifications (c) G. Finch */

// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


//#define DEBUG

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

#define MAGIC_THRESHOLD 70

typedef unsigned int RGB32;


// TODO - handle rowstrides

// TODO

static int sqrtable[256];

static const int point = 16;
static const int impact = 2;
static const int decay = 8;
static const int loopnum = 2;
static int period = 0;
static int rain_stat = 0;
static unsigned int drop_prob = 0;
static int drop_prob_increment = 0;
static int drops_per_frame_max = 0;
static int drops_per_frame = 0;
static int drop_power = 0;

/////////////



struct _sdata {
  int *map;
  int *map1;
  int *map2;
  int *map3;
  int bgIsSet;
  signed char *vtable;
  short *background;
  unsigned char *diff;
  int threshold;
  uint32_t fastrand_val;
};




static void image_bgset_y(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
  register int i,j;
  int R, G, B;
  RGB32 *p;
  short *q;

  rowstride-=width;

  p = src;
  q = sdata->background;
  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      R = ((*p)&0xff0000)>>(16-1);
      G = ((*p)&0xff00)>>(8-2);
      B = (*p)&0xff;
      *q = (short)(R + G + B);
      p++;
      q++;
    }
    p+=rowstride;
  }
}


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


/////////////////////////////////////////////////////

static inline uint32_t fastrand(struct _sdata *sdata) {
#define rand_a 1073741789L
#define rand_c 32749L

  return ((sdata->fastrand_val= (rand_a*sdata->fastrand_val + rand_c)));
}


static void setTable(void) {
  int i;

  for (i=0; i<128; i++) {
    sqrtable[i] = i*i;
  }
  for (i=1; i<=128; i++) {
    sqrtable[256-i] = -i*i;
  }
}

static int setBackground(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
  image_bgset_y(src, width, height, rowstride, sdata);
  sdata->bgIsSet = 1;

  return 0;
}


/////////////////////////////////////////////////////////////

int ripple_init(weed_plant_t *inst) {
  int map_h;
  int map_w;
  struct _sdata *sdata;
  //char *list[2]={"ripples","rain"};
  weed_plant_t *in_channel;
  int error;

  sdata=weed_malloc(sizeof(struct _sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  map_h=weed_get_int_value(in_channel,"height",&error);
  map_w=weed_get_int_value(in_channel,"width",&error);

  sdata->map = (int *)weed_malloc(map_h*map_w*3*sizeof(int));
  if (sdata->map == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->vtable = (signed char *)weed_malloc(map_h*map_w*2*sizeof(signed char));
  if (sdata->vtable == NULL) {
    weed_free(sdata->map);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->background = (short *)weed_malloc(map_h*map_w*sizeof(short));
  if (sdata->background == NULL) {
    weed_free(sdata->vtable);
    weed_free(sdata->map);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->diff = (unsigned char *)weed_malloc(map_h*map_w * 4 * sizeof(unsigned char));
  if (sdata->diff == NULL) {
    weed_free(sdata->background);
    weed_free(sdata->vtable);
    weed_free(sdata->map);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->map, 0, map_h*map_w*3*sizeof(int));
  weed_memset(sdata->vtable, 0, map_h*map_w*2*sizeof(signed char));
  weed_memset(sdata->diff, 0, map_h*map_w*4*sizeof(unsigned char));
  sdata->map1 = sdata->map;
  sdata->map2 = sdata->map + map_h*map_w;
  sdata->map3 = sdata->map + map_w * map_h * 2;
  sdata->bgIsSet = 0;
  sdata->threshold=MAGIC_THRESHOLD*7;
  sdata->fastrand_val=0;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


int ripple_deinit(weed_plant_t *inst) {
  int error;
  struct _sdata *sdata;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata != NULL) {
    weed_free(sdata->diff);
    weed_free(sdata->background);
    weed_free(sdata->vtable);
    weed_free(sdata->map);
    weed_free(sdata);
    weed_set_voidptr_value(inst,"plugin_internal",NULL);
  }
  return WEED_NO_ERROR;
}


static void motiondetect(RGB32 *src, int width, int height, int rowstride, struct _sdata *sdata) {
  unsigned char *diff;
  int *p, *q;
  register int x, y;
  int h;

  if (!sdata->bgIsSet) {
    setBackground(src,width,height,rowstride,sdata);
  }
  image_bgsubtract_update_y(src,width,height,rowstride,sdata);

  diff = sdata->diff;
  p = sdata->map1+width+1;
  q = sdata->map2+width+1;
  diff += width+2;

  for (y=height-2; y>0; y--) {
    for (x=width-2; x>0; x--) {
      h = (int)*diff + (int)*(diff+1) + (int)*(diff+width) + (int)*(diff+width+1);
      if (h>0) {
        *p = h<<(point + impact - 8);
        *q = *p;
      }
      p++;
      q++;
      diff += 2;
    }
    diff += width+2;
    p+=2;
    q+=2;
  }
}



static inline void drop(int power, int width, int height, struct _sdata *sdata) {
  int x, y;
  int *p, *q;

  x = fastrand(sdata)%(width-4)+2;
  y = fastrand(sdata)%(height-4)+2;
  p = sdata->map1 + y*width + x;
  q = sdata->map2 + y*width + x;
  *p = power;
  *q = power;
  *(p-width) = *(p-1) = *(p+1) = *(p+width) = power/2;
  *(p-width-1) = *(p-width+1) = *(p+width-1) = *(p+width+1) = power/4;
  *(q-width) = *(q-1) = *(q+1) = *(q+width) = power/2;
  *(q-width-1) = *(q-width+1) = *(q+width-1) = *(p+width+1) = power/4;
}

static void raindrop(int width, int height, struct _sdata *sdata) {

  int i;

  if (period == 0) {
    switch (rain_stat) {
    case 0:
      period = (fastrand(sdata)>>23)+100;
      drop_prob = 0;
      drop_prob_increment = 0x00ffffff/period;
      drop_power = (-(fastrand(sdata)>>28)-2)<<point;
      drops_per_frame_max = 2<<(fastrand(sdata)>>30); // 2,4,8 or 16
      rain_stat = 1;
      break;
    case 1:
      drop_prob = 0x00ffffff;
      drops_per_frame = 1;
      drop_prob_increment = 1;
      period = (drops_per_frame_max - 1) * 16;
      rain_stat = 2;
      break;
    case 2:
      period = (fastrand(sdata)>>22)+1000;
      drop_prob_increment = 0;
      rain_stat = 3;
      break;
    case 3:
      period = (drops_per_frame_max - 1) * 16;
      drop_prob_increment = -1;
      rain_stat = 4;
      break;
    case 4:
      period = (fastrand(sdata)>>24)+60;
      drop_prob_increment = -(drop_prob/period);
      rain_stat = 5;
      break;
    case 5:
    default:
      period = (fastrand(sdata)>>23)+500;
      drop_prob = 0;
      rain_stat = 0;
      break;
    }
  }
  switch (rain_stat) {
  default:
  case 0:
    break;
  case 1:
  case 5:
    if ((fastrand(sdata)>>8)<drop_prob) {
      drop(drop_power,width,height,sdata);
    }
    drop_prob += drop_prob_increment;
    break;
  case 2:
  case 3:
  case 4:
    for (i=drops_per_frame/16; i>0; i--) {
      drop(drop_power,width,height,sdata);
    }
    drops_per_frame += drop_prob_increment;
    break;
  }
  period--;
}



int ripple_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  register int x, y, i;
  int dx, dy;
  int h, v;
  int width, height;
  int *p, *q, *r;
  signed char *vp;
  struct _sdata *sdata;
  RGB32 *src,*dest;
  int mode;
  weed_plant_t *in_channel,*out_channel,*in_param;
  int error;
  int irowstride,orowstride,orowstridex;

  mode=0;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  dest=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  width = weed_get_int_value(in_channel,"width",&error);
  height = weed_get_int_value(in_channel,"height",&error);

  irowstride=weed_get_int_value(in_channel,"rowstrides",&error)/4;
  orowstridex=orowstride=weed_get_int_value(out_channel,"rowstrides",&error)/4;

  //if (width%2!=0) orowstridex--;

  sdata->fastrand_val=timestamp&0x0000FFFF;
  in_param=weed_get_plantptr_value(inst,"in_parameters",&error);
  mode=weed_get_int_value(in_param,"value",&error);

  /* impact from the motion or rain drop */
  if (mode) {
    raindrop(width,height,sdata);
  } else {
    motiondetect(src,width,height,irowstride,sdata);
  }

  /* simulate surface wave */

  /* This function is called only 30 times per second. To increase a speed
   * of wave, iterates this loop several times. */
  for (i=loopnum; i>0; i--) {
    /* wave simulation */
    p = sdata->map1 + width + 1;
    q = sdata->map2 + width + 1;
    r = sdata->map3 + width + 1;
    for (y=height-2; y>0; y--) {
      for (x=width-2; x>0; x--) {
        h = *(p-width-1) + *(p-width+1) + *(p+width-1) + *(p+width+1)
            + *(p-width) + *(p-1) + *(p+1) + *(p+width) - (*p)*9;
        h = h >> 3;
        v = *p - *q;
        v += h - (v >> decay);
        *r = v + *p;
        p++;
        q++;
        r++;
      }
      p += 2;
      q += 2;
      r += 2;
    }

    /* low pass filter */
    p = sdata->map3 + width + 1;
    q = sdata->map2 + width + 1;
    for (y=height-2; y>0; y--) {
      for (x=width-2; x>0; x--) {
        h = *(p-width) + *(p-1) + *(p+1) + *(p+width) + (*p)*60;
        *q = h >> 6;
        p++;
        q++;
      }
      p+=2;
      q+=2;
    }

    p = sdata->map1;
    sdata->map1 = sdata->map2;
    sdata->map2 = p;
  }

  vp = sdata->vtable;
  p = sdata->map1;
  for (y=height-1; y>0; y--) {
    for (x=width-1; x>0; x--) {
      /* difference of the height between two voxel. They are twiced to
       * emphasise the wave. */
      vp[0] = sqrtable[((p[0] - p[1])>>(point-1))&0xff];
      vp[1] = sqrtable[((p[0] - p[width])>>(point-1))&0xff];
      p++;
      vp+=2;
    }
    p++;
    vp+=2;
  }

  vp = sdata->vtable;

  /* draw refracted image. The vector table is stretched. */
  for (y=0; y<height-2; y+=2) {
    for (x=0; x<width; x+=2) {
      h = (int)vp[0];
      v = (int)vp[1];
      dx = x + h;
      dy = y + v;
      if (dx<0) dx=0;
      if (dy<0) dy=0;
      if (dx>=width) dx=width-1;
      if (dy>=height) dy=height-1;
      dest[0] = src[dy*irowstride+dx];

      i = dx;

      dx = x + 1 + (h+(int)vp[2])/2;
      if (dx<0) dx=0;
      if (dx>=width) dx=width-1;
      dest[1] = src[dy*irowstride+dx];

      dy = y + 1 + (v+(int)vp[width*2+1])/2;
      if (dy<0) dy=0;
      if (dy>=height) dy=height-1;
      dest[orowstride] = src[dy*irowstride+i];

      dest[orowstride+1] = src[dy*irowstride+dx];
      dest+=2;
      vp+=2;
    }
    dest += orowstridex;
    vp += 2;
  }
  return WEED_NO_ERROR;
}


/////////////////////////////////////////////////////////////////////////


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    const char *modes[]= {"ripples","rain",NULL};
    int palette_list[]= {WEED_PALETTE_RGBA32,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *in_params[]= {weed_string_list_init("mode","Ripple _mode",0,modes),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("rippleTV","effectTV",1,0,&ripple_init,&ripple_process,&ripple_deinit,in_chantmpls,
                               out_chantmpls,in_params,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

    setTable();
  }
  return plugin_info;
}

