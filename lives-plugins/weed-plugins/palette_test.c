// palette_test.c
// weed plugin
// (c) G. Finch (salsaman) 2008
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// test plugin for testing various palettes

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

int ptest_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  void **src=weed_get_voidptr_array(in_channel,"pixel_data",&error);
  void **dst=weed_get_voidptr_array(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);

  if (src[0]!=dst[0]) weed_memcpy(dst[0],src[0],width*height);
  if (src[1]!=dst[1]) weed_memcpy(dst[1],src[1],width*height);
  if (src[2]!=dst[2]) weed_memcpy(dst[2],src[2],width*height);

  weed_free(src);
  weed_free(dst);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions),*gui;
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_YUV444P,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("Palette testing plugin","salsaman",1,0,NULL,&ptest_process,NULL,in_chantmpls,
                               out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(in_chantmpls[0],"YUV_clamping",WEED_YUV_CLAMPING_UNCLAMPED);
    weed_set_int_value(out_chantmpls[0],"YUV_clamping",WEED_YUV_CLAMPING_UNCLAMPED);

    gui=weed_filter_class_get_gui(filter_class);
    weed_set_boolean_value(gui,"hidden",WEED_TRUE);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

