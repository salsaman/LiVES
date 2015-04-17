/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001-2005 FUKUCHI Kentaro
 *
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

int revtv_process(weed_plant_t *inst, weed_timecode_t timecode) {
  weed_plant_t *in_channel,*out_channel,**in_params;
  unsigned char *src,*dest;

  short val;

  int yval;

  int offset;

  int width,height,irow,orow,pal;
  int error;

  int red=0,green=1,blue=2;
  int psize=4;

  int linespace;
  double vscale;

  register int x, y;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  src=(unsigned char *)weed_get_voidptr_value(in_channel,"pixel_data",&error);
  dest=(unsigned char *)weed_get_voidptr_value(out_channel,"pixel_data",&error);

  width = weed_get_int_value(in_channel,"width",&error);
  height = weed_get_int_value(in_channel,"height",&error);

  pal = weed_get_int_value(in_channel,"current_palette",&error);

  irow = weed_get_int_value(in_channel,"rowstrides",&error);
  orow = weed_get_int_value(out_channel,"rowstrides",&error);

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  linespace=weed_get_int_value(in_params[0],"value",&error);
  vscale=weed_get_double_value(in_params[1],"value",&error);
  vscale=vscale*vscale/200.;
  weed_free(in_params);

  if (pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_BGRA32) {
    red=2;
    blue=0;
  } else if (pal==WEED_PALETTE_ARGB32) {
    red=1;
    green=2;
    blue=3;
  }

  if (pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_YUV888) {
    psize=3;
  }

  width*=psize;

  irow*=linespace;

  for (y=0; y<height; y+=linespace) {
    for (x=0; x<width; x+=psize) {
      if (pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888) val=src[0]*7;
      else val=(short)((src[red]<<1)+(src[green]<<2)+src[blue]);
      yval=y-val*vscale;
      if ((offset=yval*orow+x)>=0) weed_memcpy(&dest[offset],src,psize);
      src+=psize;
    }
    src+=irow-width;
  }

  return WEED_NO_ERROR;
}







weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_RGB24,WEED_PALETTE_BGR24,WEED_PALETTE_ARGB32,WEED_PALETTE_YUV888,WEED_PALETTE_YUVA8888,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *in_params[]= {weed_integer_init("lspace","_Line spacing",6,1,16),weed_float_init("vscale","_Vertical scale factor",2.,0.,4.),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("revTV","effectTV",1,0,NULL,&revtv_process,NULL,in_chantmpls,out_chantmpls,in_params,
                               NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}


