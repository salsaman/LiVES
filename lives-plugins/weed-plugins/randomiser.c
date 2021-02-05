// randomiser.c
// weed plugin
// (c) G. Finch (salsaman) 2012 - 2021
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// generate a random double when input changes state

//#define DEBUG

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_RANDOM

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

#include <stdlib.h>
#include <stdio.h>

#define NVALS 32

typedef struct {
  int vals[NVALS];
} _sdata;


static double getrand(double min, double max, weed_plant_t *inst) {
  return min + fastrand_dbl_re(max - min, inst, WEED_LEAF_PLUGIN_RANDOM_SEED);
}


static weed_error_t randomiser_init(weed_plant_t *inst) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);

  _sdata *sdata = (_sdata *)weed_calloc(sizeof(_sdata), 1);

  double nrand, min, max;

  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

  // enable repeatable randomness
  weed_set_int64_value(inst, WEED_LEAF_PLUGIN_RANDOM_SEED,
                       weed_get_int64_value(inst, WEED_LEAF_RANDOM_SEED, NULL));

  for (int i = 0; i < NVALS; i++) {
    sdata->vals[i] = weed_param_get_value_boolean(in_params[i]);

    min = weed_param_get_value_double(in_params[NVALS + i * 4]);
    max = weed_param_get_value_double(in_params[NVALS + i * 4 + 1]);
    nrand = min + (max - min) / 2.;
    weed_set_double_value(out_params[i], WEED_LEAF_VALUE, nrand);
  }

  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  weed_free(in_params);
  weed_free(out_params);
  return WEED_SUCCESS;
}


int randomiser_deinit(weed_plant_t *inst) {
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) weed_free(sdata);
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t randomiser_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  double nrand, min, max;
  int iv, trigt, trigf;

  for (int i = 0; i < NVALS; i++) {
    iv = weed_param_get_value_boolean(in_params[i]);
    if (iv != sdata->vals[i]) {
      trigt = weed_param_get_value_boolean(in_params[NVALS + i * 4 + 2]);
      trigf = weed_param_get_value_boolean(in_params[NVALS + i * 4 + 3]);
      if ((iv == WEED_TRUE && trigt == WEED_TRUE) || (iv == WEED_FALSE && trigf == WEED_FALSE)) {
        min = weed_param_get_value_double(in_params[NVALS + i * 4]);
        max = weed_param_get_value_double(in_params[NVALS + i * 4 + 1]);
        nrand = getrand(min, max, inst);
        weed_set_double_value(out_params[i], WEED_LEAF_VALUE, nrand);
      }
      sdata->vals[i] = iv;
    }
  }

  weed_free(in_params);
  weed_free(out_params);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class, *gui;

  weed_plant_t *in_params[NVALS * 5 + 1];
  weed_plant_t *out_params[NVALS + 1];

  int count = 0, i;

  char name[256];
  char label[256];
  char desc[256];

  for (i = 0; i < NVALS; i++) {
    snprintf(name, 256, "input%03d", i);
    snprintf(label, 256, "Trigger %03d", i);
    in_params[i] = weed_switch_init(name, label, WEED_FALSE);
    gui = weed_paramtmpl_get_gui(in_params[i]);
    weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
    snprintf(name, 256, "Output %03d", i);
    out_params[i] = weed_out_param_float_init_nominmax(name, 0.);
  }

  out_params[i] = NULL;

  for (i = NVALS; i < NVALS * 5; i += 4) {
    snprintf(name, 256, "min%03d", i);
    snprintf(label, 256, "Min value for output %03d", count);
    in_params[i] = weed_float_init(name, label, 0., -1000000., 1000000.);

    snprintf(name, 256, "max%03d", i);
    snprintf(label, 256, "Max value for output %03d", count);
    in_params[i + 1] = weed_float_init(name, label, 1., -1000000., 1000000.);

    snprintf(name, 256, "trigt%03d", i);
    snprintf(label, 256, "Trigger FALSE->TRUE");
    in_params[i + 2] = weed_switch_init(name, label, WEED_TRUE);

    snprintf(name, 256, "trigf%03d", i);
    snprintf(label, 256, "Trigger TRUE->FALSE");
    in_params[i + 3] = weed_switch_init(name, label, WEED_FALSE);

    count++;
  }

  in_params[i] = NULL;

  filter_class = weed_filter_class_init("randomiser", "salsaman", 1, 0, NULL,
                                        randomiser_init, randomiser_process, randomiser_deinit, NULL, NULL, in_params, out_params);

  snprintf(desc, 256, "For each of the %d outputs,\n"
           "produce a random double when the corresponding boolean input changes state\n"
           "min and max may be set for each out parameter, as may the trigger direction(s) for each in parameter.\n"
           , NVALS);
  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

