// alpha_means.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// calculate n X m mean values for alpha channel

// values are output from left to right and top to bottom, eg. for 2 X 2 grid:

// val 1 | val 2
// ------+------
// val 3 | val 4

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


static weed_error_t alpham_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  weed_plant_t *out_param = weed_get_plantptr_value(inst, WEED_LEAF_OUT_PARAMETERS, NULL);

  float *alpha = (float *)weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);

  int irow = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL) - width * sizeof(float);

  int n = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
  int m = weed_get_int_value(in_params[1], WEED_LEAF_VALUE, NULL);
  int xdiv = weed_get_boolean_value(in_params[2], WEED_LEAF_VALUE, NULL);
  int ydiv = weed_get_boolean_value(in_params[3], WEED_LEAF_VALUE, NULL);
  int abs = weed_get_boolean_value(in_params[4], WEED_LEAF_VALUE, NULL);
  double scale = weed_get_double_value(in_params[5], WEED_LEAF_VALUE, NULL);

  int idx = 0, nidx;

  float nf = (float)width / (float)n; // x pixels per quad
  float mf = (float)height / (float)m; // y pixels per quad
  float nm = (float)(nf * mf); // pixels per quad

  double *vals;

  register int i, j, x;

  weed_free(in_params);
  vals = (double *)weed_malloc(n * m * sizeof(double));
  if (vals == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  for (i = 0; i < n * m; i++) vals[i] = 0.;
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      if (idx > n * m) continue;
      vals[idx] += (double) * alpha;
      // check val of idx for next j
      if (j + 1 < width) {
        nidx = (int)((float)(j + 1.) / nf + .5);
        if (nidx > idx + 1) {
          // too many vals, copy...
          for (x = idx + 1; x < nidx; x++) {
            vals[x] = vals[idx];
          }
        }
        idx = nidx;
      }
      alpha++;
    }
    alpha += irow;

    nidx = (int)((float)(m * (i + 1)) / mf + .5);

    if (nidx > idx + 1) {
      for (x = idx + 1; x < nidx; x++) {
        if (x < n * m)
          vals[x] = vals[x - m];
      }
    }
    idx = nidx;
  }

  if (nm < 1.) nm = 1.;

  for (i = 0; i < n * m; i++) {
    vals[i] /= (double)nm; // get average val
    if (xdiv) vals[i] /= (double)width;
    if (ydiv) vals[i] /= (double)height;
    if (abs && vals[i] < 0.) vals[i] = -vals[i];
    vals[i] *= scale;
  }

  weed_set_double_array(out_param, WEED_LEAF_VALUE, n * m, vals);

  weed_free(vals);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int apalette_list[] = {WEED_PALETTE_AFLOAT, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("alpha float", 0), NULL};

  weed_plant_t *in_params[] = {weed_integer_init("x divisions", "_X divisions", 1, 1, 256),
                               weed_integer_init("y divisions", "_Y divisions", 1, 1, 256),
                               weed_switch_init("xdiv", "Divide by _width", WEED_FALSE),
                               weed_switch_init("ydiv", "Divide by _height", WEED_FALSE),
                               weed_switch_init("abs", "Return _absolute values", WEED_FALSE),
                               weed_float_init("scale", "_Scale by", 1.0, 0.1, 1000000.), NULL
                              };

  weed_plant_t *out_params[] = {weed_out_param_float_init_nominmax("mean values", 0.), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("alpha_means", "salsaman", 1, 0, apalette_list,
                               NULL, alpham_process, NULL,
                               in_chantmpls, NULL,
                               in_params, out_params);

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION,
                        "Calculate n X m mean values for (float) alpha channel\n"
                        "values are output from left to right and top to bottom, eg. for 2 X 2 grid:\n\n"
                        "val 1 | val 2\n------+------\nval 3 | val 4");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  //number of output values depends on size of grid
  weed_set_int_value(out_params[0], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);

}
WEED_SETUP_END;

