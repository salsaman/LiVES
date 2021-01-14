// dat_unpacker.c
// weed plugin
// (c) G. Finch (salsaman) 2012 - 2021
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

#define N_ELEMS 128

static weed_error_t dunpack_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);
  double *fvals, xval;

  int oidx = 0, nvals = 0;
  int clamp = weed_param_get_value_boolean(in_params[N_ELEMS]);
  double range = weed_param_get_value_double(in_params[N_ELEMS + 1]);

  for (int i = 0; i < N_ELEMS; i++) {
    fvals = weed_param_get_array_double(in_params[i], &nvals);
    if (nvals > 0) {
      for (int j = 0; j < nvals; j++) {
        xval = fvals[j];
        if (clamp == WEED_TRUE) {
          if (xval > range) xval = range;
          if (xval < -range) xval = -range;
        }
        weed_set_double_value(out_params[oidx++], WEED_LEAF_VALUE, xval);
        if (oidx == N_ELEMS) break;
      }
    }
    if (fvals) weed_free(fvals);
    if (oidx == N_ELEMS) break;
  }

  weed_free(in_params);
  weed_free(out_params);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class, *gui;
  weed_plant_t *in_params[N_ELEMS + 3];
  weed_plant_t *out_params[N_ELEMS + 1];

  char rfxstring0[256];
  char rfxstring1[256];
  char *rfxstrings[2] = {rfxstring0, rfxstring1};

  char name[256];
  char label[256];
  char desc[512];

  int i;
  for (i = 0; i < N_ELEMS; i++) {
    snprintf(name, 256, "input%03d", i);
    snprintf(label, 256, "Input %03d", i);
    in_params[i] = weed_float_init(name, label, -1000000000000., 0., 1000000000000.);
    weed_set_int_value(in_params[i], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE);

    snprintf(name, 256, "Output %03d", i);
    out_params[i] = weed_out_param_float_init(name, 0., -1000000000000., 1.000000000000);
  }

  out_params[i] = NULL;

  in_params[i++] = weed_switch_init("clamp", "_Clamp to range", WEED_TRUE);
  in_params[i++] = weed_float_init("range", "_Range (-range to +range)", 1., 0., 1000000000000.);
  in_params[i] = NULL;

  filter_class = weed_filter_class_init("data_unpacker", "salsaman", 1, 0, NULL,
                                        NULL, dunpack_process, NULL, NULL, NULL, in_params, out_params);

  snprintf(desc, 512, "Unpacks multi-valued (array) data in the input parameters into single valued outputs\n"
           "Once an input has been fully unpacked, then the next input is processed and so on,\n"
           "until there are no more inputs or the maximum number of output values (%d) is reached.\n"
           "Output values may optionally be clamped between -range and +range\n"
           "The outputs are suitable as inputs for the data_processing filter for example.\n"
           , N_ELEMS);

  snprintf(rfxstrings[0], 256, "layout|p%d|", N_ELEMS);
  snprintf(rfxstrings[1], 256, "layout|p%d|", N_ELEMS + 1);

  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 3, (char **)rfxstrings);

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;
