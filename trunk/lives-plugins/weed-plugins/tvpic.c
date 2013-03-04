// tvpic.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// simulate a TV display

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
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_UTILS
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

int tvpic_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dest=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int pal=weed_get_int_value(in_channel,"current_palette",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int offs=(pal==WEED_PALETTE_RGB24)?3:4;
  unsigned char *end=src+height*irowstride;
  register int i;
  int colrd=1,col;
  int pc=0;

  width*=offs;

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    int offset=weed_get_int_value(out_channel,"offset",&error);
    int dheight=weed_get_int_value(out_channel,"height",&error);

    src+=offset*irowstride;
    dest+=offset*orowstride;
    end=src+dheight*irowstride;
  }

  for (;src<end;src+=irowstride) {
    col=colrd=!colrd;
    if (!col) pc=0; // red
    else pc=1; // green
    for (i=0;i<width;i+=offs) {
      if (!col) dest[i+2]=dest[i+1]=dest[i]=0;
      else {
	if (pc==0) {
	  dest[i]=src[i];
	  dest[i+1]=dest[i+2]=0;
	}
	else if (pc==1) {
	  dest[i+1]=src[i+1];
	  dest[i]=dest[i+2]=0;
	}
	else {
	  dest[i+2]=src[i+2];
	  dest[i]=dest[i+1]=0;
	}
      }
      if (pal==WEED_PALETTE_RGBA32) dest[i+3]=src[i+3];
      if (!(col=!col)) {
	pc+=pc<2?1:-2;
      }
      src+=offs;
      dest+=offs;
    }
    dest+=orowstride;
  } 
  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("tvpic","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&tvpic_process,NULL,in_chantmpls,out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

