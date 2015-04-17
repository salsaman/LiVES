// shift.c
// weed plugin
// (c) G. Finch (salsaman) 2012
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

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

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




static void add_bg_pixel(unsigned char *ptr, int pal, int clamping, int trans) {
  if (trans==WEED_TRUE) trans=0;
  else trans=255;

  switch (pal) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    weed_memset(ptr,0,3);
    break;

  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    weed_memset(ptr,0,3);
    ptr[3]=trans;
    break;

  case WEED_PALETTE_ARGB32:
    weed_memset(ptr+1,0,3);
    ptr[0]=trans;
    break;

  case WEED_PALETTE_YUV888:
    if (clamping!=WEED_YUV_CLAMPING_CLAMPED) ptr[0]=0;
    else ptr[0]=16;
    ptr[1]=ptr[2]=128;
    break;

  case WEED_PALETTE_YUVA8888:
    if (clamping!=WEED_YUV_CLAMPING_CLAMPED) ptr[0]=0;
    else ptr[0]=16;
    ptr[1]=ptr[2]=128;
    ptr[3]=trans;
    break;
  }

}


static void add_bg_row(unsigned char *ptr, int xwidth, int pal, int clamping, int trans) {
  register int i;
  int psize=4;
  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_YUV888) psize=3;
  for (i=0; i<xwidth; i+=psize) add_bg_pixel(ptr+i,pal,clamping,trans);
}



int shift_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  unsigned char *src=(unsigned char *)weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=(unsigned char *)weed_get_voidptr_value(out_channel,"pixel_data",&error);

  int width=weed_get_int_value(in_channel,"width",&error);
  int sheight=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);

  unsigned char *dend;

  size_t send=irowstride*sheight;

  int x=(int)(weed_get_double_value(in_params[0],"value",&error)*(double)width+.5);
  int y=(int)(weed_get_double_value(in_params[1],"value",&error)*(double)sheight+.5)*irowstride;
  int trans=weed_get_boolean_value(in_params[2],"value",&error);

  int offset=0;
  int dheight=weed_get_int_value(out_channel,"height",&error); // may differ because of threading

  int pal=weed_get_int_value(in_channel,"current_palette",&error);

  int psize=4;

  int sx,sy,ypos;

  int istart,iend;

  int clamping=WEED_YUV_CLAMPING_CLAMPED;

  weed_free(in_params);

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    offset=weed_get_int_value(out_channel,"offset",&error);
    dst+=offset*orowstride;
  }

  dend=dst+dheight*orowstride;

  ypos=(offset-1)*irowstride;

  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_YUV888) psize=3;

  if (pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888)
    clamping=weed_get_int_value(in_channel,"YUV_clamping",&error);

  x*=psize;
  width*=psize;

  if (x<0) {
    // shift left
    istart=0;
    iend=width+x;
    if (iend<0) iend=0;
  } else {
    // shift right
    if (x>=width) x=width;
    istart=x;
    iend=width;
  }

  for (; dst<dend; dst+=orowstride) {
    ypos+=irowstride;
    sy=ypos-y;

    if (sy<0||sy>=send) {
      add_bg_row(dst,width,pal,clamping,trans);
      continue;
    }

    if (x>0) {
      add_bg_row(dst,x,pal,clamping,trans);
      sx=0;
    } else sx=-x;

    if (istart<iend) {
      weed_memcpy(&dst[istart],src+sy+sx,iend-istart);
    }

    if (iend<width) add_bg_row(&dst[iend],width-iend,pal,clamping,trans);

  }


  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,
                         WEED_PALETTE_YUV888,WEED_PALETTE_YUVA8888,WEED_PALETTE_END
                        };

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *in_params[]= {weed_float_init("xshift","_X shift (ratio)",0.,-1.,1.),
                                weed_float_init("yshift","_Y shift (ratio)",0.,-1.,1.),
                                weed_switch_init("transbg","_Transparent edges",WEED_FALSE),
                                NULL
                               };

    weed_plant_t *filter_class=weed_filter_class_init("shift","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&shift_process,NULL,
                               in_chantmpls,out_chantmpls,in_params,NULL);


    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

