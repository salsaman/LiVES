// audio_transition.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2008
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////


static weed_error_t atrans_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  // this is a bit of a cheat - we simply rely on the fact that the host will automatically adjust volume levels
  // depending on the value of the WEED_LEAF_IS_TRANSITION parameter for transition effects which have audio in / out
  // channels (normally they would also have video channels...)

  // since  we also hinted "inplace", out channel[0] data should be set to in_channel[0] data anyway
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("in channel 0", 0),
                                  weed_audio_channel_template_init("in channel 1", 0), NULL
                                 };
  weed_plant_t *out_chantmpls[] = {weed_audio_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *in_params[] = {weed_float_init("transition", "_Rear track level", 0.0, 0.0, 1.0), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("audio transition", "salsaman", 1, 0, NULL,
                               atrans_process, NULL, in_chantmpls,
                               out_chantmpls,
                               in_params, NULL);

  weed_set_boolean_value(in_params[0], WEED_LEAF_IS_TRANSITION, WEED_TRUE);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

