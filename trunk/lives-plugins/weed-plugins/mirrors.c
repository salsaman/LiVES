// mirrors.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "../../libweed/weed.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional

/////////////////////////////////////////////////////////////

int mirrorx_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int inplace=(src==dst);
  int width=weed_get_int_value(in_channel,"width",&error)*3,hwidth=width>>1;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int offset=0;
  unsigned char *end=src+height*irowstride;
  register int i;

  for (;src<end;src+=irowstride) {
    offset=-1;
    for (i=0;i<hwidth;i++) {
      dst[width-1-i+offset*2]=src[i];
      if (!inplace) dst[i]=src[i];
      offset=++offset<2?offset:-1;
    }
    dst+=orowstride;
  } 
  return WEED_NO_ERROR;
}


int mirrory_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error),*osrc=src;
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error),*odst=dst;
  int inplace=(src==dst);
  int width=weed_get_int_value(in_channel,"width",&error)*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  unsigned char *end=src+height*irowstride/2;

  if (weed_plant_has_leaf(inst,"plugin_combined")&&weed_get_boolean_value(inst,"plugin_combined",&error)==WEED_TRUE) {
    inplace=WEED_TRUE;
    src=dst;
    end=dst+height*orowstride/2;
    irowstride=orowstride;
  }

  if (!inplace) {
    for (;src<end;src+=irowstride) {
      weed_memcpy(dst,src,width);
      dst+=orowstride;
    }
    src=osrc;
    dst=odst;
  }
  dst+=((height-1)*orowstride);

  for (;src<end;src+=irowstride) {
    weed_memcpy(dst,src,width);
    dst-=orowstride;
  } 
  return WEED_NO_ERROR;
}


int mirrorxy_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int ret=mirrorx_process(inst,timestamp);
  if (ret!=WEED_NO_ERROR) return ret;
  weed_set_boolean_value(inst,"plugin_combined",WEED_TRUE);
  ret=mirrory_process(inst,timestamp);
  return ret;
}



weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    
    weed_plant_t *filter_class=weed_filter_class_init("mirrorx","salsaman",1,0,NULL,&mirrorx_process,NULL,in_chantmpls,out_chantmpls,NULL,NULL);
    weed_plugin_info_add_filter_class (plugin_info,filter_class);
    
    filter_class=weed_filter_class_init("mirrory","salsaman",1,0,NULL,&mirrory_process,NULL,weed_clone_plants(in_chantmpls),weed_clone_plants(out_chantmpls),NULL,NULL);
    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    filter_class=weed_filter_class_init("mirrorxy","salsaman",1,0,NULL,&mirrorxy_process,NULL,weed_clone_plants(in_chantmpls),weed_clone_plants(out_chantmpls),NULL,NULL);
    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}


