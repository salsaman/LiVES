// ccorect.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
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

int ccorect_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error)*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  unsigned char *end=src+height*irowstride;
  int palette=weed_get_int_value(in_channel,"current_palette",&error);
  register int i;
  weed_plant_t **params=weed_get_plantptr_array(inst,"in_parameters",&error);

  double red=weed_get_double_value(params[0],"value",&error);
  double green=weed_get_double_value(params[1],"value",&error);
  double blue=weed_get_double_value(params[2],"value",&error);

  unsigned int r,g,b;

  for (;src<end;src+=irowstride) {
    for (i=0;i<width;i+=3) {
      if (palette==WEED_PALETTE_BGR24) {
	b=blue*src[i]+.5;
	g=green*src[i+1]+.5;
	r=red*src[i+2]+.5;
	src[i]=b>255?(unsigned char)255:(unsigned char)b;
	src[i+1]=g>255?(unsigned char)255:(unsigned char)g;
	src[i+2]=r>255?(unsigned char)255:(unsigned char)r;
      }
      else {
	b=blue*src[i+2]+.5;
	g=green*src[i+1]+.5;
	r=red*src[i]+.5;
	src[i+2]=b>255?(unsigned char)255:(unsigned char)b;
	src[i+1]=g>255?(unsigned char)255:(unsigned char)g;
	src[i]=r>255?(unsigned char)255:(unsigned char)r;
      }
    }
    dst+=orowstride;
  } 
  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};

    weed_plant_t *in_params[]={weed_float_init("red","_Red factor",1.0,0.0,2.0),weed_float_init("green","_Green factor",1.0,0.0,2.0),weed_float_init("blue","_Blue factor",1.0,0.0,2.0),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("colour correction","salsaman",1,WEED_FILTER_HINT_IS_POINT_EFFECT,NULL,&ccorect_process,NULL,in_chantmpls,out_chantmpls,in_params,NULL);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

