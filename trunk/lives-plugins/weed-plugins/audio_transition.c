// audio_transition.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2008
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
static int api_versions[]= {131,110}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

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


int atrans_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  // do nothing - it is enough for the host that we have a transition parameter
  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    weed_plant_t *in_chantmpls[]= {weed_audio_channel_template_init("in channel 0",0),weed_audio_channel_template_init("in channel 1",0),NULL};
    weed_plant_t *out_chantmpls[]= {weed_audio_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE),NULL};
    weed_plant_t *in_params[]= {weed_float_init("transition","_Rear track level",0.0,0.0,1.0),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("audio transition","salsaman",1,0,NULL,&atrans_process,NULL,in_chantmpls,out_chantmpls,
                               in_params,NULL);

    weed_set_boolean_value(in_params[0],"transition",WEED_TRUE);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

