// dat_unpacker.c
// weed plugin
// (c) G. Finch (salsaman) 2021
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// unpacks multivalued data into single valued outputs

//#define DEBUG

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <stdio.h>

typedef struct _sdata {
  weed_timecode_t start, reset;
  int was_started, was_reset;
} sdata;


static weed_error_t timer_init(weed_plant_t *inst) {
  struct _sdata *sdata = weed_calloc(sizeof(struct _sdata), 1);
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;
  sdata->was_reset = WEED_FALSE;
  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t timer_deinit(weed_plant_t *inst) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t timer_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);

  double tval = (double)timestamp / (double)WEED_TICKS_PER_SECOND;

  int reset = weed_param_get_value_boolean(in_params[0]);

  if (!sdata->was_started) {
    sdata->reset = sdata->start = timestamp;
    sdata->was_started = 1;
  }
  if (reset == WEED_TRUE) {
    if (sdata->was_reset == WEED_FALSE) {
      sdata->reset = timestamp;
      sdata->was_reset = WEED_TRUE;
    }
  } else sdata->was_reset = WEED_FALSE;

  // absolute
  weed_set_double_value(out_params[1], WEED_LEAF_VALUE, tval);

  // relative
  tval = (double)(timestamp - sdata->start) / (double)WEED_TICKS_PER_SECOND;
  weed_set_double_value(out_params[0], WEED_LEAF_VALUE, tval);

  // since reset
  tval = (double)(timestamp - sdata->reset) / (double)WEED_TICKS_PER_SECOND;
  weed_set_double_value(out_params[2], WEED_LEAF_VALUE, tval);

  // was reset (can be fed back to reset)
  weed_set_boolean_value(out_params[3], WEED_LEAF_VALUE, sdata->was_reset);

  weed_free(in_params);
  weed_free(out_params);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class;
  weed_plant_t *in_params[2];
  weed_plant_t *out_params[5];

  char desc[256];

  out_params[0] = weed_out_param_float_init("relative", 0., -1000000000000., 1.000000000000);
  out_params[1] = weed_out_param_float_init("absolute", 0., -1000000000000., 1.000000000000);
  out_params[2] = weed_out_param_float_init("since_reset", 0., -1000000000000., 1.000000000000);
  out_params[3] = weed_out_param_switch_init("was_reset", WEED_FALSE);
  out_params[4] = NULL;

  in_params[0] = weed_switch_init("reset", "_Reset counter", WEED_FALSE);
  in_params[1] = NULL;

  filter_class = weed_filter_class_init("timer", "salsaman", 1, 0, NULL,
                                        timer_init, timer_process, timer_deinit, NULL, NULL,
                                        in_params, out_params);

  snprintf(desc, 256, "Outputs timing values (relative to playback start, absolute, and since reset).\b"
           "The reset value can be set by flipping the 'reset' input from FALSE to TRUE\n");

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;
