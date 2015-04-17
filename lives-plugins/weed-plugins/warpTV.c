/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001-2005 FUKUCHI Kentaro
 *
 * From main.c of warp-1.1:
 *
 *      Simple DirectMedia Layer demo
 *      Realtime picture 'gooing'
 *      Released under GPL
 *      by sam lantinga slouken@devolution.com
 */

/* ported to Weed by G. Finch (salsaman@xs4all.nl,salsaman@gmail.com)
   modifications (c) salsaman 2005
*/

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

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

///////////////////////////////////////////////////////////////////

typedef unsigned int RGB32;
typedef int32_t Sint32;
typedef uint32_t Uint32;

/////////////////////////////////

struct _sdata {
  Sint32 *disttable;
  Sint32 ctable[1024];
  Sint32 sintable[1024+256];
  int tval;
};


static void initSinTable(struct _sdata *sdata) {
  Sint32 *tptr, *tsinptr;
  double i;

  tsinptr = tptr = sdata->sintable;

  for (i = 0; i < 1024; i++)
    *tptr++ = (int)(sin(i*M_PI/512) * 32767);

  for (i = 0; i < 256; i++)
    *tptr++ = *tsinptr++;
}




static void initDistTable(struct _sdata *sdata, int width, int height) {
  Sint32 halfw, halfh, *distptr;
#ifdef PS2
  float	x,y,m;
#else
  double x,y,m;
#endif

  halfw = width/2.+.5;
  halfh = height/2.+.5;

  distptr = sdata->disttable;

  m = sqrt((double)(halfw*halfw + halfh*halfh));

  for (y = -halfh; y < halfh; y++)
    for (x= -halfw; x < halfw; x++)
#ifdef PS2
      *distptr++ = ((int)
                    ((sqrtf(x*x+y*y) * 511.100100) / m)) << 1;
#else
      *distptr++ = ((int)
                    ((sqrt(x*x+y*y) * 511.100100) / m)) << 1;
#endif
}


static void doWarp(int xw, int yw, int cw,RGB32 *src,RGB32 *dst, int width, int height, int irow, int orow, struct _sdata *sdata) {
  Sint32 c,i, x,y, dx,dy, maxx, maxy;
  Sint32 skip, *ctptr, *distptr;
  Uint32 *destptr;
  //	Uint32 **offsptr;

  ctptr = sdata->ctable;
  distptr = sdata->disttable;
  destptr = dst;
  skip = orow-width;
  c = 0;
  for (x = 0; x < 512; x++) {
    i = (c >> 3) & 0x3FE;
    *ctptr++ = ((sdata->sintable[i] * yw) >> 15);
    *ctptr++ = ((sdata->sintable[i+256] * xw) >> 15);
    c += cw;
  }
  maxx = width - 2;
  maxy = height - 2;
  /*	printf("Forring\n"); */
  for (y = 0; y < height-1; y++) {
    for (x = 0; x < width; x++) {
      i = *distptr;
      distptr++;
      dx = sdata->ctable [i+1] + x;
      dy = sdata->ctable [i] + y;


      if (dx < 0) dx = 0;
      else if (dx > maxx) dx = maxx;

      if (dy < 0) dy = 0;
      else if (dy > maxy) dy = maxy;
      /* 	   printf("f:%d\n",dy); */
      /*	   printf("%d\t%d\n",dx,dy); */
      *destptr++ = src[dy*irow+dx];
    }
    destptr += skip;
  }

}

/////////////////////////////////////////////////////////////

int warp_init(weed_plant_t *inst) {
  int video_height;
  int video_width;
  int error;
  weed_plant_t *in_channel;
  struct _sdata *sdata;

  sdata=weed_malloc(sizeof(struct _sdata));

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  video_height=weed_get_int_value(in_channel,"height",&error);
  video_width=weed_get_int_value(in_channel,"width",&error);

  video_width=video_width/2.+.5;
  video_width*=2;

  video_height=video_height/2.+.5;
  video_height*=2;

  sdata->disttable = weed_malloc(video_width * video_height * sizeof(int));
  if (sdata->disttable == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  initSinTable(sdata);
  initDistTable(sdata,video_width,video_height);

  sdata->tval=0;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


int warp_deinit(weed_plant_t *inst) {
  int error;
  struct _sdata *sdata;
  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata != NULL) {
    weed_free(sdata->disttable);
    weed_free(sdata);
    weed_set_voidptr_value(inst,"plugin_internal",NULL);
  }
  return WEED_NO_ERROR;
}


int warp_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  RGB32 *src,*dest;
  struct _sdata *sdata;

  int error;
  int xw,yw,cw;

  int width,height,irow,orow;

  weed_plant_t *in_channel,*out_channel;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  dest=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  width = weed_get_int_value(in_channel,"width",&error);
  height = weed_get_int_value(in_channel,"height",&error);

  irow = weed_get_int_value(in_channel,"rowstrides",&error)/4;
  orow = weed_get_int_value(out_channel,"rowstrides",&error)/4;


  xw  = (int)(sin((sdata->tval+100)*M_PI/128) * 30);
  yw  = (int)(sin((sdata->tval)*M_PI/256) * -35);
  cw  = (int)(sin((sdata->tval-70)*M_PI/64) * 50);
  xw += (int)(sin((sdata->tval-10)*M_PI/512) * 40);
  yw += (int)(sin((sdata->tval+30)*M_PI/512) * 40);

  doWarp(xw,yw,cw,src,dest,width,height,irow,orow,sdata);
  sdata->tval = (sdata->tval+1) &511;

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_RGBA32,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("warpTV","effectTV",1,0,&warp_init,&warp_process,&warp_deinit,in_chantmpls,out_chantmpls,
                               NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }

  return plugin_info;
}


