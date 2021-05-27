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
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////
#define N_PARAMS 16

typedef struct {
  int counts[N_PARAMS];
  int ovals[N_PARAMS];
} sdata_t;


static weed_error_t dcount_init(weed_plant_t *inst) {
  sdata_t *sdata = (sdata_t *)weed_malloc(sizeof(sdata_t));
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;
  weed_memset(sdata->counts, 0, N_PARAMS * sizeof(int));
  weed_memset(sdata->ovals, 0, N_PARAMS * sizeof(int));
  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t dcount_deinit(weed_plant_t *inst) {
  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) weed_free(sdata);
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t dcount_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);
  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  for (int i = 0, j = 0; i < N_PARAMS * 7; i += 7, j++) {
    if (weed_param_has_value(in_params[i])) {
      int cval = weed_param_get_value_boolean(in_params[i]);
      if (!weed_param_has_value(out_params[j]))
        weed_set_boolean_value(out_params[j], WEED_LEAF_VALUE, WEED_FALSE);
      if (cval != sdata->ovals[j]) {
        if (weed_param_get_value_boolean(out_params[j]) == WEED_FALSE) {
          if (cval == WEED_TRUE) {
            // F -> T
            if (weed_param_get_value_boolean(in_params[i + 1]) == WEED_TRUE) {
              sdata->counts[j]++;
            }
          } else {
            // T -> F
            if (weed_param_get_value_boolean(in_params[i + 2]) == WEED_TRUE) {
              sdata->counts[j]++;
            }
          }
          if (sdata->counts[j] >= weed_param_get_value_int(in_params[i + 3])) {
            weed_set_boolean_value(out_params[j], WEED_LEAF_VALUE, WEED_TRUE);
            sdata->counts[j] = 0;
          }
        } else {
          if (cval == WEED_TRUE) {
            // F -> T
            if (weed_param_get_value_boolean(in_params[i + 4]) == WEED_TRUE) {
              sdata->counts[j]++;
            }
          } else {
            // T -> F
            if (weed_param_get_value_boolean(in_params[i + 5]) == WEED_TRUE) {
              sdata->counts[j]++;
            }
          }
          if (sdata->counts[j] >= weed_param_get_value_int(in_params[i + 6])) {
            weed_set_boolean_value(out_params[j], WEED_LEAF_VALUE, WEED_FALSE);
            sdata->counts[j] = 0;
          }
        }
      }
      sdata->ovals[j] = cval;
    }
  }

  weed_free(in_params);
  weed_free(out_params);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class;
  weed_plant_t *gui;
  weed_plant_t *in_params[N_PARAMS * 7 + 1];
  weed_plant_t *out_params[N_PARAMS + 1];

  int i, j = 0;

  char name[256];
  char label[256];

  char *rfxstrings[N_PARAMS * 4];

  for (i = 0; i < N_PARAMS * 7; i += 7, j++) {
    snprintf(name, 256, "input%02d", j);
    snprintf(label, 256, "Input %02d", j);
    in_params[i] = weed_switch_init(name, label, WEED_FALSE);

    in_params[i + 1] = weed_switch_init("ttrue", "Count FALSE -> TRUE", WEED_TRUE);
    in_params[i + 2] = weed_switch_init("tfalse", "Count TRUE -> FALSE", WEED_FALSE);
    in_params[i + 3] = weed_integer_init("oncount", "Count to flip to TRUE", 8, 1, 256);

    in_params[i + 4] = weed_switch_init("txtrue", "Count FALSE -> TRUE", WEED_TRUE);
    in_params[i + 5] = weed_switch_init("txfalse", "Count TRUE -> FALSE", WEED_FALSE);
    in_params[i + 6] = weed_integer_init("offcount", "Count to flip back to FALSE", 8, 1, 256);

    gui = weed_paramtmpl_get_gui(in_params[i]);
    weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);

    rfxstrings[j * 4] = weed_malloc(256);
    rfxstrings[j * 4 + 1] = weed_malloc(256);
    rfxstrings[j * 4 + 2] = weed_malloc(256);
    rfxstrings[j * 4 + 3] = weed_malloc(256);

    snprintf(rfxstrings[j * 4], 256, "layout|fill|\n");
    snprintf(rfxstrings[j * 4 + 1], 256, "layout|\"Output %d\"|\n", j);
    snprintf(rfxstrings[j * 4 + 2], 256, "layout|p%d|fill|p%d|p%d|\n", i + 3, i + 1, i + 2);
    snprintf(rfxstrings[j * 4 + 3], 256, "layout|p%d|fill|p%d|p%d|\n", i + 6, i + 4, i + 5);

    snprintf(name, 256, "Output %02d", j);
    out_params[j] = weed_out_param_switch_init(name, WEED_FALSE);
  }

  in_params[i] = NULL;
  out_params[j] = NULL;

  filter_class = weed_filter_class_init("data_counter", "salsaman", 1, 0, NULL,
                                        dcount_init, dcount_process, dcount_deinit, NULL, NULL, in_params, out_params);

  gui = weed_filter_get_gui(filter_class);

  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", N_PARAMS * 4, rfxstrings);

  for (int i = 0; i < N_PARAMS * 4; i++) weed_free(rfxstrings[i]);

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION,
                        "Each input can be set to count n trigger events, and once this happens, "
                        "the corresponding output will flip from FALSE to TRUE.\n"
                        "Then after a different set of trigger events, it will flip back to FALSE");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

