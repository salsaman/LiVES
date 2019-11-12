// nn_programmer.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// works in combination with data_processor to make a neural net
//#define DEBUG

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

/////////////////////////////////////////////////////////////

#define NEED_RANDOM

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

typedef struct {
  double *constvals;
  double *vals;
} _sdata;

#define MAXNODES 128
#define MAXSTRLEN 8192

#define NGAUSS 4

//////////////////////////////////////////////////////////////////////////////////

static weed_error_t nnprog_init(weed_plant_t *inst) {
  register int i, j;

  _sdata *sdata = (_sdata *)weed_malloc(sizeof(_sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->vals = weed_malloc(MAXNODES * 2 * MAXNODES * sizeof(double));

  if (sdata->vals == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->constvals = weed_malloc(MAXNODES * sizeof(double));

  if (sdata->constvals == NULL) {
    weed_free(sdata->vals);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (i = 0; i < MAXNODES * 2; i++) {
    if (i < MAXNODES) sdata->constvals[i] = drand(2.) - 1.;
    for (j = 0; j < MAXNODES; j++) {
      sdata->vals[i * MAXNODES + j] = drand(2.) - 1.;
    }
  }

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t nnprog_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_error_t error;
  weed_plant_t **in_params = weed_get_plantptr_array(inst, "in_parameters", &error);
  weed_plant_t **out_params = weed_get_plantptr_array(inst, "out_parameters", &error);
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);

  double fit = (1. - weed_get_double_value(in_params[0], "value", &error)) / (double)NGAUSS;
  double rval;

  int innodes = weed_get_int_value(in_params[1], "value", &error);
  int outnodes = weed_get_int_value(in_params[2], "value", &error);
  int hnodes = weed_get_int_value(in_params[3], "value", &error);

  char *strings[256];
  char tmp[MAXSTRLEN];

  char *ptr;

  int k, idx = 0;

  register int i, j, z;

  weed_free(in_params);

  // adjust values according to fitness

  for (i = 0; i < hnodes + outnodes; i++) {
    if (i < MAXNODES) {
      rval = 0.;
      for (z = 0; z < NGAUSS; z++) {
        rval += (drand(2.) - 1.) * fit;
      }
      if (rval > 0.)
        sdata->constvals[i] += (1. - sdata->constvals[i]) * rval;
      else
        sdata->constvals[i] += (1. + sdata->constvals[i]) * rval;

      if (sdata->constvals[i] < -1.) sdata->constvals[i] = -1.;
      if (sdata->constvals[i] > 1.) sdata->constvals[i] = 1.;
    }
    for (j = 0; j < MAXNODES; j++) {
      rval = 0.;
      for (z = 0; z < NGAUSS; z++) {
        rval += (drand(2.) - 1.) * fit;
      }
      if (rval > 0.)
        sdata->vals[idx] += (1. - sdata->vals[idx]) * rval;
      else
        sdata->vals[idx] += (1. + sdata->vals[idx]) * rval;

      if (sdata->vals[idx] < -1.) sdata->vals[idx] = -1.;
      if (sdata->vals[idx] > 1.) sdata->vals[idx] = 1.;

      idx++;
    }
  }

  // create strings for hidden nodes (s values)

  for (i = 0; i < hnodes; i++) {
    snprintf(tmp, MAXSTRLEN, "s[%d]=%f", i, sdata->constvals[i]);
    ptr = tmp + strlen(tmp);
    for (j = 0; j < innodes; j++) {
      snprintf(ptr, MAXSTRLEN, "+%f*i[%d]", sdata->vals[i * MAXNODES + j], j);
      ptr = tmp + strlen(tmp);
    }
    strings[i] = strdup(tmp);
  }

  k = i;

  // create strings for output nodes (o values)

  for (i = 0; i < outnodes; i++) {
    snprintf(tmp, MAXSTRLEN, "o[%d]=", i);
    ptr = tmp + strlen(tmp);
    for (j = 0; j < hnodes; j++) {
      snprintf(ptr, MAXSTRLEN, "+%f*s[%d]", sdata->vals[(k + i)*MAXNODES + j], j);
      ptr = tmp + strlen(tmp);
    }
    strings[i + k] = strdup(tmp);
  }

  for (i = 0; i < hnodes + outnodes; i++) {
    weed_set_string_value(out_params[i], "value", strings[i]);
#ifdef DEBUG
    if (strlen(strings[i])) printf("eqn %d: %s\n", i, strings[i]);
#endif
    weed_free(strings[i]);
  }

  weed_free(out_params);

  return WEED_SUCCESS;
}


static weed_error_t nnprog_deinit(weed_plant_t *inst) {
  weed_error_t error;
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);

  if (sdata != NULL) {
    if (sdata->constvals != NULL) weed_free(sdata->constvals);
    if (sdata->vals != NULL) weed_free(sdata->vals);
    weed_free(sdata);
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class, *gui;
  char name[256];
  char desc[512];

  weed_plant_t *in_params[] = {weed_float_init("fitness", "_Fitness", 0., 0., 1.),
                               weed_integer_init("innodes", "Number of _Input Nodes", 1, 1, 256),
                               weed_integer_init("outnodes", "Number of _Output Nodes", 1, 1, 128),
                               weed_integer_init("hnodes", "Number of _Hidden Nodes", 1, 1, 128),
                               NULL
                              };
  weed_plant_t *out_params[MAXNODES * 2 + 1];

  register int i;

  for (i = 0; i < MAXNODES * 2; i++) {
    snprintf(name, 256, "Equation%03d", i);
    out_params[i] = weed_out_param_text_init(name, "");
  }

  out_params[i] = NULL;

  filter_class = weed_filter_class_init("nn_programmer", "salsaman", 1, 0, &nnprog_init, &nnprog_process,
                                        &nnprog_deinit, NULL, NULL, in_params, out_params);

  gui = weed_filter_class_get_gui(filter_class);
  weed_set_boolean_value(gui, "hidden", WEED_TRUE);

  for (i = 1; i < 4; i++)
    weed_set_int_value(in_params[i], "flags", WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

  snprintf(desc, 512, "%s", "Runs a neural net.\n"
           "On each cycle, generates string equations for the output nodes,\n"
           "using input nodes and possibly hidden nodes as intermediaries.\n"
           "The resulting output strings may be fed in as equations to the data_processor plugin\n"
           "to generate numerical values from real inputs.\n"
           "Depending on the outputs from the data_processor, the fitness value may be adjusted\n"
           "For the next cycle. A fitness value of 0. (the default) will produce large variations,\n"
           "whereas a fitness value of 1. will produce no variations.\n"
           "A Gaussian randomiser is used to vary the random factors.\n");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;

