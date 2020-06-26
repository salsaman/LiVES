// dat_unpacker.c
// weed plugin
// (c) G. Finch (salsaman) 2012
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
#include "../../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <stdio.h>

#define N_ELEMS 128

static weed_error_t dunpack_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  weed_plant_t **out_params = weed_get_plantptr_array(inst, WEED_LEAF_OUT_PARAMETERS, NULL);
  double *fvals, xval;

  int oidx = 0, nvals;

  register int i, j;

  for (i = 0; i < N_ELEMS; i++) {
    nvals = weed_leaf_num_elements(in_params[i], WEED_LEAF_VALUE);
    if (nvals > 0) {
      fvals = weed_get_double_array(in_params[i], WEED_LEAF_VALUE, NULL);
      for (j = 0; j < nvals; j++) {
        xval = fvals[j];
        if (xval > 1.) xval = 1.;
        if (xval < -1.) xval = -1.;
        weed_set_double_value(out_params[oidx++], WEED_LEAF_VALUE, xval);
        if (oidx == N_ELEMS) break;
      }
      weed_free(fvals);
    }
    if (oidx == N_ELEMS) break;
  }

  weed_free(in_params);
  weed_free(out_params);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class;
  weed_plant_t *in_params[N_ELEMS + 1];
  weed_plant_t *out_params[N_ELEMS + 1];

  char name[256];
  char label[256];
  char desc[256];

  register int i;
  for (i = 0; i < N_ELEMS; i++) {
    snprintf(name, 256, "input%03d", i);
    snprintf(label, 256, "Input %03d", i);
    in_params[i] = weed_float_init(name, label, 0., 0., 1.);
    weed_set_int_value(in_params[i], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE);

    snprintf(name, 256, "Output %03d", i);
    out_params[i] = weed_out_param_float_init(name, 0., -1., 1.);
  }

  in_params[i] = NULL;
  out_params[i] = NULL;

  filter_class = weed_filter_class_init("data_unpacker", "salsaman", 1, 0, NULL,
                                        NULL, dunpack_process, NULL, NULL, NULL, in_params, out_params);

  snprintf(desc, 256, "Unpacks multivalued (array) data in the input parameters into single valued outputs\n"
           "Maximum number of values is %d\n"
           "Output values are clamped between -1.0 and +1.0\n"
           "The outputs are suitable for passing into the inputs of the data_processing plugin\n"
           , N_ELEMS);

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;
