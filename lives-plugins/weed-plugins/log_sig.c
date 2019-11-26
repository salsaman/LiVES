// nn_programmer.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// converts float values to log_sig values

//#define DEBUG

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

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <stdio.h>
#include <math.h>

#define N_PARAMS 128

static weed_error_t logsig_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  weed_plant_t **out_params = weed_get_plantptr_array(inst, WEED_LEAF_OUT_PARAMETERS, NULL);
  double fval;

  register int i;

  for (i = 0; i < 256; i++) {
    if (weed_plant_has_leaf(in_params[i], WEED_LEAF_VALUE)) {
      fval = weed_get_double_value(in_params[i], WEED_LEAF_VALUE, NULL);
      weed_set_double_value(out_params[i], WEED_LEAF_VALUE, 1. / (1. + exp(-fval)));
    }
  }

  weed_free(in_params);
  weed_free(out_params);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class;

  weed_plant_t *in_params[N_PARAMS + 1];
  weed_plant_t *out_params[N_PARAMS + 1];

  register int i;

  char name[256];
  char label[256];

  for (i = 0; i < N_PARAMS; i++) {
    snprintf(name, N_PARAMS, "input%03d", i);
    snprintf(label, N_PARAMS, "Input %03d", i);
    in_params[i] = weed_float_init(name, label, 0., -1000000000000., 1000000000000.);
    snprintf(name, N_PARAMS, "Output %03d", i);
    out_params[i] = weed_out_param_float_init(name, 0., -1., 1.);
  }

  in_params[i] = NULL;
  out_params[i] = NULL;

  filter_class = weed_filter_class_init("log_sig", "salsaman", 1, 0, NULL,
                                        NULL, logsig_process, NULL, NULL, NULL, in_params, out_params);

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION,
                        "Scales (double) values between -1.0 and 1.0 using a log-sig function\n"
                        "Inputs may be fed from the outputs of the data_processor or data_unpacker plugins, "
                        "for instance.\n");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

