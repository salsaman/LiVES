/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 * 1DTV - scans line by line and generates amazing still image.
 * Copyright (C) 2001 FUKUCHI Kentarou
 *
 */


////////////////////////////////////////////////

/* modified for Livido by G. Finch (salsaman)
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

typedef unsigned int RGB32;

static int is_big_endian() {
  int32_t testint = 0x12345678;
  char *pMem;
  pMem = (char *) &testint;
  if (pMem[0] == 0x78) return 0;
  return 1;
}

struct _sdata {
  int line;
  RGB32 *linebuf;
};

////////////////////////////////////////////

int oned_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int map_w,map_h;

  weed_plant_t *in_channel;
  int error;

  sdata=weed_malloc(sizeof(struct _sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  map_h=weed_get_int_value(in_channel,"height",&error);
  map_w=weed_get_int_value(in_channel,"width",&error);

  sdata->linebuf = weed_malloc(map_w*map_w*sizeof(RGB32));
  if (sdata->linebuf == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->linebuf, 0, map_w*map_h*sizeof(RGB32));


  sdata->line = 0;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}



int oned_deinit(weed_plant_t *inst) {
  struct _sdata *sdata;
  int error;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata != NULL) {
    weed_free(sdata->linebuf);
    weed_free(sdata);
  }
  return WEED_NO_ERROR;
}



static void blitline(RGB32 *src, RGB32 *dest, int video_width, int irow, struct _sdata *sdata) {
  src += irow  * sdata->line;
  dest += video_width * sdata->line;
  weed_memcpy(dest, src, sizeof(RGB32) * video_width);
}


int oned_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel,*out_channel;
  struct _sdata *sdata;
  RGB32 *src,*odest,*dest;

  size_t offs=0;

  int width,height,irow,orow;
  int error;

  register int i;


  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  odest=dest=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  width = weed_get_int_value(in_channel,"width",&error);
  height = weed_get_int_value(in_channel,"height",&error);

  irow = weed_get_int_value(in_channel,"rowstrides",&error)/4;
  orow = weed_get_int_value(out_channel,"rowstrides",&error)/4;

  blitline(src,sdata->linebuf,width,irow,sdata);

  sdata->line++;
  if (sdata->line >= height)
    sdata->line = 0;

  for (i=0; i<height; i++) {
    weed_memcpy(dest,sdata->linebuf+offs,width*4);
    dest+=orow;
    offs+=width;
  }

  dest = odest + orow * sdata->line;
  for (i=0; i<width; i++) {
    dest[i] = 0xff00ff00;
  }
  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_RGBA32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("onedTV","effectTV",1,0,&oned_init,&oned_process,&oned_deinit,in_chantmpls,out_chantmpls,
                               NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

