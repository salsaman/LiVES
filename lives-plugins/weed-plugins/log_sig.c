// log_sig.c
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
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <stdio.h>
#include <math.h>

#define N_PARAMS 128

static weed_error_t logsig_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);
  double fval;

  for (int i = 0; i < N_PARAMS; i++) {
    if (weed_param_has_value(in_params[i])) {
      fval = weed_param_get_value_double(in_params[i]);
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

  int i;

  char name[256];
  char label[256];

  for (i = 0; i < N_PARAMS; i++) {
    snprintf(name, 256, "input%03d", i);
    snprintf(label, 256, "Input %03d", i);
    in_params[i] = weed_float_init(name, label, 0., -1000000000000., 1000000000000.);
    snprintf(name, 256, "Output %03d", i);
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
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

