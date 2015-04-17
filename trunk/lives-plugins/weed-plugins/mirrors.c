// mirrors.c
// weed plugin
// (c) G. Finch (salsaman) 2005
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

int mirrorx_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int inplace=(src==dst);
  int pal=weed_get_int_value(in_channel,"current_palette",&error);
  int width=weed_get_int_value(in_channel,"width",&error),hwidth;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int psize=4;
  unsigned char *end=src+height*irowstride;
  register int i;

  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_YUV888) psize=3;

  width*=psize;
  hwidth=(width>>2)<<1;

  for (; src<end; src+=irowstride) {
    for (i=0; i<hwidth; i+=psize) {
      weed_memcpy(&dst[width-i-psize],&src[i],psize);
      if (!inplace) weed_memcpy(&dst[i],&src[i],psize);
    }
    dst+=orowstride;
  }
  return WEED_NO_ERROR;
}


int mirrory_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error),*osrc=src;
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error),*odst=dst;
  int inplace=(src==dst);
  int pal=weed_get_int_value(in_channel,"current_palette",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int psize=4;
  unsigned char *end=src+height*irowstride/2;

  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_YUV888) psize=3;

  if (pal==WEED_PALETTE_UYVY||pal==WEED_PALETTE_YUYV) width>>=1; // 2 pixels per macropixel

  width*=psize;

  if (weed_plant_has_leaf(inst,"plugin_combined")&&weed_get_boolean_value(inst,"plugin_combined",&error)==WEED_TRUE) {
    inplace=WEED_TRUE;
    src=dst;
    end=dst+height*orowstride/2;
    irowstride=orowstride;
  }

  if (!inplace) {
    for (; src<end; src+=irowstride) {
      weed_memcpy(dst,src,width);
      dst+=orowstride;
    }
    src=osrc;
    dst=odst;
  }
  dst+=((height-1)*orowstride);

  for (; src<end; src+=irowstride) {
    weed_memcpy(dst,src,width);
    dst-=orowstride;
  }
  return WEED_NO_ERROR;
}


int mirrorxy_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int ret=mirrorx_process(inst,timestamp);
  if (ret!=WEED_NO_ERROR) return ret;
  weed_set_boolean_value(inst,"plugin_combined",WEED_TRUE);
  ret=mirrory_process(inst,timestamp);
  return ret;
}



weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  weed_plant_t **clone1,**clone2;

  if (plugin_info!=NULL) {
    // all planar palettes
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_YUV888,WEED_PALETTE_YUVA8888,WEED_PALETTE_RGBA32,WEED_PALETTE_ARGB32,WEED_PALETTE_BGRA32,WEED_PALETTE_UYVY,WEED_PALETTE_YUYV,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("mirrorx","salsaman",1,0,NULL,&mirrorx_process,NULL,in_chantmpls,out_chantmpls,NULL,NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    filter_class=weed_filter_class_init("mirrory","salsaman",1,0,NULL,&mirrory_process,NULL,(clone1=weed_clone_plants(in_chantmpls)),
                                        (clone2=weed_clone_plants(out_chantmpls)),NULL,NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);

    filter_class=weed_filter_class_init("mirrorxy","salsaman",1,0,NULL,&mirrorxy_process,NULL,(clone1=weed_clone_plants(in_chantmpls)),
                                        (clone2=weed_clone_plants(out_chantmpls)),NULL,NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}


