// ladspa.c
// weed plugin wrapper for LADSPA effects
// (c) G. Finch (salsaman) 2012 - 2020
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_ALPHA_SORT
#define NEED_AUDIO

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

#include "weed-plugin-utils.c" // optional

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <math.h>

#include <limits.h>

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

static int verbosity = WEED_VERBOSITY_ERROR;

#include <ladspa.h>

typedef LADSPA_Descriptor *(*lad_descriptor_f)(unsigned long);
typedef LADSPA_Handle(*lad_instantiate_f)(const struct _LADSPA_Descriptor *Descriptor, unsigned long SampleRate);
typedef void (*lad_activate_f)(LADSPA_Handle Instance);
typedef void (*lad_deactivate_f)(LADSPA_Handle Instance);
typedef void (*lad_connect_port_f)(LADSPA_Handle Instance, unsigned long Port, LADSPA_Data *DataLocation);
typedef void (*lad_run_f)(LADSPA_Handle Instance, unsigned long SampleCount);
typedef void (*lad_cleanup_f)(LADSPA_Handle Instance);

typedef struct {
  LADSPA_Handle handle_l, handle_r;
  int activated_l, activated_r;
  unsigned long inst_rate;
} _sdata;

#define DEF_ARATE 44100ul

//////////////////////////////////////////////////////////////////

static void getenv_piece(char *target, size_t tlen, char *envvar, int num) {
  // get num piece from envvar path and set in target
  char *str1;
  memset(target, 0, 1);

  /* extract first string from string sequence */
  str1 = strtok(envvar, ":");

  /* loop until finishied */
  while (--num >= 0) {
    /* extract string from string sequence */
    str1 = strtok(NULL, ":");

    /* check if there is nothing else to extract */
    if (str1 == NULL) break;
  }

  if (str1 != NULL) snprintf(target, tlen, "%s", str1);
}

/////////////////////////////////////////////////////////////

static weed_error_t ladspa_init(weed_plant_t *inst) {
  weed_plant_t *filter = weed_instance_get_filter(inst);
  fprintf(stderr, "ladspa init\n");
  if (!(weed_instance_get_flags(inst) & WEED_INSTANCE_UPDATE_GUI_ONLY)) {
    lad_instantiate_f lad_instantiate_func =
      (lad_instantiate_f)weed_get_funcptr_value(filter, "plugin_lad_instantiate_func", NULL);
    LADSPA_Descriptor *laddes
      = (LADSPA_Descriptor *)weed_get_voidptr_value(filter, "plugin_lad_descriptor", NULL);
    weed_plant_t *channel = NULL;
    unsigned long rate = 0;

    _sdata *sdata = (_sdata *)weed_malloc(sizeof(_sdata));
    if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;
    weed_set_voidptr_value(inst, "plugin_data", sdata);

    channel = weed_get_in_channel(inst, 0);
    if (!channel) channel = weed_get_out_channel(inst, 0);

    if (channel) rate = (unsigned long)weed_get_int_value(channel, WEED_LEAF_AUDIO_RATE, NULL);
    if (!rate) rate = DEF_ARATE;
    sdata->inst_rate = rate;
    sdata->activated_l = sdata->activated_r = WEED_FALSE;
    sdata->handle_l = (*lad_instantiate_func)(laddes, rate);
    if (weed_get_boolean_value(filter, "plugin_dual", NULL) == WEED_TRUE)
      sdata->handle_r = (*lad_instantiate_func)(laddes, rate);
    else sdata->handle_r = NULL;
  }

  if (weed_get_boolean_value(filter, "plugin_dual", NULL) == WEED_TRUE) {
    int ninps = 0;
    weed_plant_t **in_params = weed_get_in_params(inst, &ninps);
    if (ninps > 0) {
      int link = weed_param_get_value_boolean(in_params[ninps - 1]);
      ninps /= 2;
      for (int i = 0; i < ninps; i++) {
        weed_plant_t *gui = weed_param_get_gui(in_params[i]);
        if (link == WEED_TRUE) {
          weed_set_int_value(gui, WEED_LEAF_COPY_VALUE_TO, i + ninps);
          gui = weed_param_get_gui(in_params[i + ninps]);
          weed_set_int_value(gui, WEED_LEAF_COPY_VALUE_TO, i);
        } else {
          weed_set_int_value(gui, WEED_LEAF_COPY_VALUE_TO, -1);
          gui = weed_param_get_gui(in_params[i + ninps]);
          weed_set_int_value(gui, WEED_LEAF_COPY_VALUE_TO, -1);
        }
      }
    }
    if (in_params) weed_free(in_params);
  }
  return WEED_SUCCESS;
}


static weed_error_t ladspa_deinit(weed_plant_t *inst) {
  fprintf(stderr, "ladspa deinit\n");
  if (!(weed_instance_get_flags(inst) & WEED_INSTANCE_UPDATE_GUI_ONLY)) {
    _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_data", NULL);
    weed_plant_t *filter = weed_instance_get_filter(inst);
    lad_deactivate_f lad_deactivate_func = (lad_activate_f)weed_get_funcptr_value(filter, "plugin_lad_deactivate_func", NULL);
    lad_cleanup_f lad_cleanup_func = (lad_cleanup_f)weed_get_funcptr_value(filter, "plugin_lad_cleanup_func", NULL);
    if (sdata) {
      if (sdata->activated_l == WEED_TRUE) {
        if (lad_deactivate_func) (*lad_deactivate_func)(sdata->handle_l);
      }
      if (lad_cleanup_func) (*lad_cleanup_func)(sdata->handle_l);

      if (sdata->activated_r == WEED_TRUE) {
        if (lad_deactivate_func) (*lad_deactivate_func)(sdata->handle_r);
      }
      if (sdata->handle_r)
	if (lad_cleanup_func) (*lad_cleanup_func)(sdata->handle_r);

      weed_free(sdata);
      weed_set_voidptr_value(inst, "plugin_data", NULL);
    }
  }
  return WEED_SUCCESS;
}


static weed_error_t ladspa_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  unsigned long nsamps = 0;

  int pinc, poutc;
  int iinc = 0, ioutc = 0;
  int iinp = 0, ioutp = 0, pinp = 0, poutp = 0;
  int dual = WEED_FALSE;
  unsigned long rate = DEF_ARATE;
  int srccnt = 0, dstcnt = 0;

  weed_error_t err = WEED_ERROR_REINIT_NEEDED;

  weed_plant_t *in_channel = NULL, *out_channel = NULL;
  weed_plant_t *filter = weed_instance_get_filter(inst);
  weed_plant_t *ptmpl;

  lad_connect_port_f lad_connect_port_func;
  lad_run_f lad_run_func;

  LADSPA_Data **src = NULL, **dst = NULL;
  LADSPA_Data *invals = NULL, *outvals = NULL;

  LADSPA_Descriptor *laddes;
  LADSPA_PortDescriptor ladpdes;
  LADSPA_PortRangeHint ladphint;
  LADSPA_PortRangeHintDescriptor ladphintdes;
  LADSPA_Handle handle;

  weed_plant_t **in_params = NULL;
  weed_plant_t **out_params = NULL;

  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_data", NULL);
  unsigned long i;

  if (!sdata) return WEED_ERROR_REINIT_NEEDED;

  in_channel = weed_get_in_channel(inst, 0);
  if (in_channel) {
    src = (LADSPA_Data **)weed_channel_get_audio_data(in_channel, &iinc);
    if (!src) goto procfunc_done;
    nsamps = (unsigned long)weed_channel_get_audio_length(in_channel);
    rate = (unsigned long)weed_channel_get_audio_rate(in_channel);
  }

  out_channel = weed_get_out_channel(inst, 0);
  if (out_channel) {
    dst = (LADSPA_Data **)weed_channel_get_audio_data(out_channel, &ioutc);
    if (!dst) goto procfunc_done;
    if (!in_channel) {
      nsamps = (unsigned long)weed_channel_get_audio_length(out_channel);
      rate = (unsigned long)weed_channel_get_audio_rate(out_channel);
    }
  }

  if (rate != sdata->inst_rate) goto procfunc_done;

  pinc = weed_get_int_value(filter, "plugin_in_channels", NULL);
  poutc = weed_get_int_value(filter, "plugin_out_channels", NULL);

  if (pinc > 0 && iinc == 0) goto procfunc_done;
  if (poutc > 0 && ioutc == 0) goto procfunc_done;

  if (pinc == 2 && iinc == 1) goto procfunc_done;
  if (poutc == 2 && ioutc == 1) goto procfunc_done;

  /// if the filter is mono and we want to send stereo then we actually run two filter instances,
  /// one for each stereo channel, and set "dual" to WEED_TRUE
  if (pinc == 1 && iinc == 2) dual = WEED_TRUE;
  if (poutc == 1 && ioutc == 2) dual = WEED_TRUE;

  if (sdata->activated_l == WEED_FALSE) {
    lad_activate_f lad_activate_func = (lad_activate_f)weed_get_funcptr_value(filter, "plugin_lad_activate_func", NULL);
    if (lad_activate_func)(*lad_activate_func)(sdata->handle_l);
    sdata->activated_l = WEED_TRUE;
  }

  if (dual && sdata->activated_r == WEED_FALSE) {
    lad_activate_f lad_activate_func = (lad_activate_f)weed_get_funcptr_value(filter, "plugin_lad_activate_func", NULL);
    if (lad_activate_func)(*lad_activate_func)(sdata->handle_r);
    sdata->activated_r = WEED_TRUE;
  }

  laddes = (LADSPA_Descriptor *)weed_get_voidptr_value(filter, "plugin_lad_descriptor", NULL);
  lad_connect_port_func = (lad_connect_port_f)weed_get_funcptr_value(filter, "plugin_lad_connect_port_func", NULL);

  lad_run_func = (lad_run_f)weed_get_funcptr_value(filter, "plugin_lad_run_func", NULL);

  handle = sdata->handle_l;

  if (pinc > 0 || poutc > 0) {
    // connect audio ports

    for (i = 0; i < laddes->PortCount; i++) {
      ladpdes = laddes->PortDescriptors[i];
      if (ladpdes & LADSPA_PORT_AUDIO) {
        // channel
        if (ladpdes & LADSPA_PORT_INPUT) {
          // connect to next instance audio in
          (*lad_connect_port_func)(handle, i, (LADSPA_Data *)src[srccnt++]);
          iinc--;
        } else {
          // connect to next instance audio out
          (*lad_connect_port_func)(handle, i, (LADSPA_Data *)dst[dstcnt++]);
          ioutc--;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  in_params = weed_get_in_params(inst, &iinp);
  if (weed_get_boolean_value(filter, "plugin_dual", NULL) == WEED_TRUE) iinp -= 1;

  out_params = weed_get_out_params(inst, &ioutp);

  if (ioutp > 0) outvals = (LADSPA_Data *)weed_malloc(ioutp * sizeof(LADSPA_Data));
  if (iinp > 0) invals = (LADSPA_Data *)weed_malloc(iinp * sizeof(LADSPA_Data));

  if (iinp > 0 || ioutp > 0) {
    for (i = 0; i < laddes->PortCount; i++) {
      ladpdes = laddes->PortDescriptors[i];

      if (ladpdes & LADSPA_PORT_CONTROL) {
        // control ports
        ladphint = laddes->PortRangeHints[i];
        ladphintdes = ladphint.HintDescriptor;

        if (ladpdes & LADSPA_PORT_INPUT) {
          if (ladphintdes & LADSPA_HINT_TOGGLED) {
            invals[pinp] = (LADSPA_Data)weed_param_get_value_boolean(in_params[pinp]);
          } else if (ladphintdes & LADSPA_HINT_INTEGER) {
            invals[pinp] = (LADSPA_Data)weed_param_get_value_int(in_params[pinp]);
          } else {
            invals[pinp] = (LADSPA_Data)weed_param_get_value_double(in_params[pinp]);
          }
          ptmpl = weed_param_get_template(in_params[pinp]);
          if (weed_get_boolean_value(ptmpl, "plugin_sample_rate", NULL) == WEED_TRUE) invals[pinp] *= (LADSPA_Data)rate;
          (*lad_connect_port_func)(handle, i, (LADSPA_Data *)&invals[pinp++]);
        } else {
          // connect to store for out params
          (*lad_connect_port_func)(handle, i, (LADSPA_Data *)&outvals[poutp++]);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  (*lad_run_func)(handle, nsamps);

  if (sdata->handle_r) {
    handle = sdata->handle_r;
    if (srccnt > pinc) srccnt = 0;

    if (pinc > 0 || poutc > 0) {
      if (ioutc > 0 || iinc > 0) {
	// if the second instance out channel is unconnected; we need to connect it to the second lad instance
	// if we have an unconnected (second) in channel, we connect it here; otherwise we connect both input ports again

	for (i = 0; i < laddes->PortCount; i++) {
	  ladpdes = laddes->PortDescriptors[i];
	  if (ladpdes & LADSPA_PORT_AUDIO) {
	    // channel
	    if (ladpdes & LADSPA_PORT_INPUT) {
	      // connect to next instance audio in
	      (*lad_connect_port_func)(handle, i, (LADSPA_Data *)src[srccnt++]);
	    } else {
	      // connect to next instance audio out
	      (*lad_connect_port_func)(handle, i, (LADSPA_Data *)dst[dstcnt++]);
	      // *INDENT-OFF*
	    }}}}}
  // *INDENT-ON*

      if (iinp > 0 || ioutp > 0) {
	if (pinp < iinp || poutp < ioutp) {
	  // need to use second instance
	  for (i = 0; i < laddes->PortCount; i++) {
	    ladpdes = laddes->PortDescriptors[i];
	    if (ladpdes & LADSPA_PORT_CONTROL) {
	      // control ports
	      ladphint = laddes->PortRangeHints[i];
	      ladphintdes = ladphint.HintDescriptor;

	      if (ladpdes & LADSPA_PORT_INPUT) {
		// connect to next instance in param
		if (ladphintdes & LADSPA_HINT_TOGGLED) {
		  invals[pinp] = (LADSPA_Data)weed_param_get_value_boolean(in_params[pinp]);
		} else if (ladphintdes & LADSPA_HINT_INTEGER) {
		  invals[pinp] = (LADSPA_Data)weed_param_get_value_int(in_params[pinp]);
		} else {
		  invals[pinp] = (LADSPA_Data)weed_param_get_value_double(in_params[pinp]);
		}
		ptmpl = weed_param_get_template(in_params[pinp]);
		if (weed_get_boolean_value(ptmpl, "plugin_sample_rate", NULL) == WEED_TRUE) invals[pinp] *= (LADSPA_Data)rate;
		(*lad_connect_port_func)(handle, i, (LADSPA_Data *)&invals[pinp++]);
	      } else {
		// connect to store for out params
		(*lad_connect_port_func)(handle, i, (LADSPA_Data *)&outvals[poutp++]);
		// *INDENT-OFF*
	      }}}}}}
  // *INDENT-ON*

  if (dual)(*lad_run_func)(handle, nsamps);

  // copy out values to out_params
  for (i = 0; i < ioutp; i++) {
    switch (weed_leaf_seed_type(out_params[i], WEED_LEAF_VALUE)) {
    case WEED_SEED_BOOLEAN:
      weed_set_boolean_value(out_params[i], WEED_LEAF_VALUE, (int)outvals[i]);
      break;
    case WEED_SEED_INT:
      weed_set_int_value(out_params[i], WEED_LEAF_VALUE, (int)outvals[i]);
      break;
    default:
      weed_set_double_value(out_params[i], WEED_LEAF_VALUE, (double)outvals[i]);
      break;
    }
  }

  err = WEED_SUCCESS;

procfunc_done:
  if (invals) weed_free(invals);
  if (dst) weed_free(dst);
  if (src) weed_free(src);
  if (outvals) weed_free(outvals);
  if (in_params) weed_free(in_params);
  if (out_params) weed_free(out_params);

  return err;
}


static weed_error_t ladspa_disp(weed_plant_t *inst, weed_plant_t *param, int inverse) {
  int stype = weed_leaf_seed_type(param, WEED_LEAF_VALUE);
  if (stype == WEED_SEED_INVALID) return WEED_ERROR_NOSUCH_LEAF;
  else {
    weed_plant_t *ptmpl = weed_param_get_template(param);
    weed_plant_t *gui;
    _sdata *sdata = NULL;
    double dval = 0.;
    int arate = 0;
    int is_srate, is_log;
    is_srate = weed_get_boolean_value(ptmpl, "plugin_sample_rate", NULL);
    is_log = weed_get_boolean_value(ptmpl, "plugin_logarithmic", NULL);
    if (is_srate == WEED_TRUE) {
      sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_data", NULL);
      if (!sdata) return WEED_ERROR_REINIT_NEEDED;
      arate = (int)sdata->inst_rate;
    }
    gui = weed_param_get_gui(param);
    if (stype == WEED_SEED_INT) {
      int ival = weed_param_get_value_int(param);
      if (inverse == WEED_FALSE) {
        if (is_log) {
          if (ival <= 0) return WEED_ERROR_NOT_READY;
          ival = (int)(logf((float)ival) + .5);
        }
        if (is_srate) ival *= arate;
      } else {
        if (is_srate) ival = ((double)ival / (double)arate + .5);
        if (is_log) ival = (int)(expf((float)ival) + .5);
      }
      return weed_set_int_value(gui, WEED_LEAF_DISPLAY_VALUE, ival);
    }
    dval = weed_param_get_value_double(param);
    if (inverse == WEED_FALSE) {
      if (is_log) {
        if (dval <= 0.) return WEED_ERROR_NOT_READY;
        dval = log(dval);
      }
      if (is_srate) dval *= (double)arate;
    } else {
      if (is_srate) dval /= (double)arate;
      if (is_log) dval = exp(dval);
    }
    return weed_set_double_value(gui, WEED_LEAF_DISPLAY_VALUE, dval);
  }
}


WEED_SETUP_START(200, 200) {
  dlink_list_t *list = NULL;
  unsigned long num_plugins = 0;
  int num_filters = 0;
  // get LADSPA path

  char *lpp = getenv("LADSPA_PATH");

  char **rfx_strings = NULL;
  weed_plant_t **out_chantmpls = NULL, **in_chantmpls = NULL;
  weed_plant_t **in_params = NULL, **out_params = NULL, *gui;

  struct dirent *vdirent = NULL;
  weed_plant_t *filter_class;

  int vdirval = 0;
  int pnum, wnum;

  LADSPA_Descriptor *laddes;
  LADSPA_Properties ladprop;
  LADSPA_PortDescriptor ladpdes;
  LADSPA_PortRangeHint ladphint;
  LADSPA_PortRangeHintDescriptor ladphintdes;
  LADSPA_Data lbound;
  LADSPA_Data ubound;
  LADSPA_Data defval;

  lad_descriptor_f lad_descriptor_func;
  lad_instantiate_f lad_instantiate_func;
  lad_activate_f lad_activate_func;
  lad_deactivate_f lad_deactivate_func;
  lad_connect_port_f lad_connect_port_func;
  lad_run_f lad_run_func;
  lad_cleanup_f lad_cleanup_func;

  char cname[128];

#define LABSIZE 1024

  char label[LABSIZE];
  char vdir[PATH_MAX];
  char plugin_name[PATH_MAX], weed_name[PATH_MAX], plug1[PATH_MAX * 2];
  DIR *curvdir = NULL;

  void *handle = NULL;

  int cflags;
  int ninps, noutps = 0, ninchs = 0, noutchs = 0;
  int oninps, onoutps = 0, oninchs = 0, onoutchs = 0;
  int cninps, cnoutps;
  int stcount, stcount2;
  int numstr;

  int dual;

  if (!lpp) return NULL;

  verbosity = weed_get_host_verbosity(weed_get_host_info(plugin_info));

  weed_set_string_value(plugin_info, WEED_LEAF_PACKAGE_NAME, "LADSPA");

  while (1) {
    // split by :
    char *lpp_copy = strdup(lpp);
    getenv_piece(vdir, PATH_MAX, lpp, vdirval);
    free(lpp_copy);

    if (curvdir) closedir(curvdir);
    curvdir = NULL;

    if (!strlen(vdir)) break;
    curvdir = opendir(vdir);
    if (!curvdir) break;

    while (1) {
      // check each plugin
      vdirent = readdir(curvdir);
      if (!vdirent) {
        closedir(curvdir);
        curvdir = NULL;
        vdirval++;
        break;
      }

      if (!strncmp(vdirent->d_name, "..", strlen(vdirent->d_name))) continue;

      snprintf(plugin_name, PATH_MAX, "%s", vdirent->d_name);
      snprintf(plug1, PATH_MAX * 2, "%s/%s", vdir, plugin_name);

      if (verbosity == WEED_VERBOSITY_DEBUG)
        fprintf(stderr, "checking %s\n", plug1);
      handle = dlopen(plug1, RTLD_NOW);
      if (!handle) continue;

      if (!(lad_descriptor_func = dlsym(handle, "ladspa_descriptor"))) {
        dlclose(handle);
        handle = NULL;
        continue;
      }

      for (num_plugins = 0; (laddes = ((*lad_descriptor_func)(num_plugins))); num_plugins++) {
        if (!(lad_instantiate_func = laddes->instantiate)) continue;

        if (!laddes->PortCount) continue;

        lad_activate_func = laddes->activate;
        lad_deactivate_func = laddes->deactivate;

        if (!(lad_connect_port_func = laddes->connect_port)) continue;

        if (!(lad_run_func = laddes->run)) continue;

        lad_cleanup_func = laddes->cleanup;

        ladprop = laddes->Properties;
        ninps = noutps = ninchs = noutchs = 0;

        // create the in_parameters, out_parameters, in_channels, out_channels

        for (int i = 0; i < laddes->PortCount; i++) {
          // count number of each type first
          ladpdes = laddes->PortDescriptors[i];
          ladphint = laddes->PortRangeHints[i];

          if (ladpdes & LADSPA_PORT_CONTROL) {
            // parameter
            if (ladpdes & LADSPA_PORT_INPUT) {
              // in_param
              ninps++;
            } else {
              // out_param
              noutps++;
            }
          } else {
            // channel
            if (ladpdes & LADSPA_PORT_INPUT) {
              // in_channel
              ninchs++;
            } else {
              // out_channel
              noutchs++;
            }
          }
        }

        // do we need to double up the channels ?
        // if we have 0, 1 or 2 input channels, 1 outputs
        // 1 input, 0 or 2 outputs

        stcount = 0;
        if ((ninchs < 2 && noutchs < 2 && (ninchs + noutchs > 0)) || noutchs == 1) {
          dual = WEED_TRUE;
          if (ninps > 0) stcount = 1;
        } else {
          dual = WEED_FALSE;
        }

        oninchs = ninchs;
        onoutchs = noutchs;
        oninps = ninps;
        onoutps = noutps;

        if (dual == WEED_TRUE && oninchs == 1) ninchs = 2;
        if (dual == WEED_TRUE && onoutchs == 1) noutchs = 2;

        if (dual == WEED_TRUE) {
          ninps = oninps * 2;
          noutps = onoutps * 2;
        }
        if (ninchs == 0) stcount2 = 3;
        else stcount2 = 4;

	if (ninchs == 2 && !dual) {
	  numstr = 2;
	  stcount = stcount2 = 0;
	}
        else numstr = ninps + stcount2 + stcount;
        rfx_strings = weed_calloc(numstr, sizeof(char *));
        for (pnum = 0; pnum < numstr; pnum++) {
          rfx_strings[pnum] = (char *)weed_calloc(256, 1);
        }

	if (!dual && ninchs == 2) {
          sprintf(rfx_strings[0], "layout|\"Stereo channels\"|");
	  sprintf(rfx_strings[1], "layout|hseparator|");
	}
	else {
	  if (ninchs == 0) {
	    sprintf(rfx_strings[stcount + 1], "layout|\"Left output channel\"|");
	    sprintf(rfx_strings[oninps + 1 + stcount], "layout|hseparator|");
	    sprintf(rfx_strings[oninps + 2 + stcount], "layout|\"Right output channel\"|");
	  } else {
	    if (ninchs == 1) sprintf(rfx_strings[stcount + 1], "layout|\"Left/mono channel\"");
          else sprintf(rfx_strings[stcount + 1], "layout|\"Left channel\"|");
          sprintf(rfx_strings[oninps + 3 + stcount], "layout|\"Right channel\"|");
	  }
	  sprintf(rfx_strings[oninps + stcount2 - 2 + stcount], "layout|hseparator|");
	}

        if (ninps > 0) {
          // add extra in param for "link channels"
          int numinparams;
	  if (ninchs == 2 && !dual) numinparams = ++ninps;
	  else {
	    numinparams = (++ninps + stcount) * (dual == WEED_TRUE ? 1 : 2)
	      - (dual == WEED_TRUE ? 0 : 1);
	  }
          in_params = (weed_plant_t **)weed_calloc(numinparams, sizeof(weed_plant_t *));
          in_params[numinparams - 1] = NULL;
          if (dual == WEED_TRUE) {
            in_params[ninps - 1] = weed_switch_init("link", "_Link left and right parameters", WEED_TRUE);
            weed_paramtmpl_set_flags(in_params[ninps - 1], WEED_PARAMETER_VALUE_IRRELEVANT);
            gui = weed_paramtmpl_get_gui(in_params[ninps - 1]);
            weed_gui_set_flags(gui, WEED_GUI_REINIT_ON_VALUE_CHANGE);
            sprintf(rfx_strings[0], "layout|p%d|", ninps - 1);
            sprintf(rfx_strings[1], "layout|hseparator|");
          }
        } else in_params = NULL;

        if (noutps > 0) {
          out_params = (weed_plant_t **)weed_calloc(++noutps, sizeof(weed_plant_t *));
          out_params[noutps - 1] = NULL;
        } else out_params = NULL;

        if (verbosity == WEED_VERBOSITY_DEBUG)
          fprintf(stderr, "%s %p %p has inc %d and outc %d\n", laddes->Name, laddes, lad_instantiate_func, ninchs, noutchs);

        if (ninchs > 0) {
          in_chantmpls = (weed_plant_t **)weed_malloc(2 * sizeof(weed_plant_t *));
          if (ninchs == 1) sprintf(cname, "Mono channel");
          else if (ninchs == 2) sprintf(cname, "Stereo channel");
          else sprintf(cname, "Multi channel");
          in_chantmpls[0] = weed_audio_channel_template_init(cname, 0);
          cflags = WEED_CHANNEL_REINIT_ON_RATE_CHANGE | WEED_CHANNEL_REINIT_ON_LAYOUT_CHANGE;
          weed_chantmpl_set_flags(in_chantmpls[0], cflags);
          in_chantmpls[1] = NULL;
          weed_set_int_value(in_chantmpls[0], WEED_LEAF_AUDIO_CHANNELS, ninchs);
        } else in_chantmpls = NULL;

        if (noutchs > 0) {
          out_chantmpls = (weed_plant_t **)weed_malloc((noutchs + 1) * sizeof(weed_plant_t *));
          if (noutchs == 1) sprintf(cname, "Mono channel");
          else if (noutchs == 2) sprintf(cname, "Stereo channel");
          else sprintf(cname, "Multi channel");
          out_chantmpls[0] = weed_audio_channel_template_init(cname, 0);
          out_chantmpls[1] = NULL;
          weed_set_int_value(out_chantmpls[0], WEED_LEAF_AUDIO_CHANNELS, noutchs);
          cflags =  WEED_CHANNEL_REINIT_ON_RATE_CHANGE | WEED_CHANNEL_REINIT_ON_LAYOUT_CHANGE;
          if (oninchs == onoutchs &&
              !(ladprop & LADSPA_PROPERTY_INPLACE_BROKEN))
            cflags |= WEED_CHANNEL_CAN_DO_INPLACE;
          weed_chantmpl_set_flags(out_chantmpls[0], cflags);
        } else out_chantmpls = NULL;

        cnoutps = cninps = 0;
        for (int i = 0; i < laddes->PortCount; i++) {
          int is_srate = 0, is_log = 0;
          ladpdes = laddes->PortDescriptors[i];
          ladphint = laddes->PortRangeHints[i];
          ladphintdes = ladphint.HintDescriptor;

          if (ladpdes & LADSPA_PORT_CONTROL) {
            // parameter
            if (ladpdes & LADSPA_PORT_INPUT) {
              // in_param
              snprintf(label, LABSIZE, "_%s", laddes->PortNames[i]);

              if (ladphintdes & LADSPA_HINT_TOGGLED) {
                // boolean type
                if (ladphintdes & LADSPA_HINT_DEFAULT_1) {
                  in_params[cninps] = weed_switch_init(laddes->PortNames[i], label, WEED_TRUE);
                  if (dual == WEED_TRUE) in_params[cninps + oninps] = weed_switch_init(laddes->PortNames[i], label, WEED_TRUE);
                } else {
                  in_params[cninps] = weed_switch_init(laddes->PortNames[i], label, WEED_FALSE);
                  if (dual == WEED_TRUE) in_params[cninps + oninps] = weed_switch_init(laddes->PortNames[i], label, WEED_FALSE);
                }
              } else {
                // int/float type
                lbound = ladphint.LowerBound;
                ubound = ladphint.UpperBound;

                if (ladphintdes & LADSPA_HINT_SAMPLE_RATE) {
                  is_srate = 1;
                }
                if (ladphintdes & LADSPA_HINT_LOGARITHMIC) {
                  is_log = 1;
                }

                if (lbound == 0. && ubound == 0.) {
                  lbound = 0.;
                  ubound = 1.;
                  if (is_log) ubound = log(4.);
                }

                if (dual == WEED_TRUE) {
                  sprintf(rfx_strings[cninps + 2 + stcount], "layout|p%d|", cninps);
                  sprintf(rfx_strings[cninps + oninps + stcount2 + stcount], "layout|p%d|", cninps + oninps);
                }

                defval = lbound;

                if (ladphintdes & LADSPA_HINT_DEFAULT_MAXIMUM) defval = ubound;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_0) defval = 0.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_1) defval = 1.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_100) defval = 100.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_440) defval = 440.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_LOW) defval += (ubound - lbound) / 4.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_MIDDLE) defval += (ubound - lbound) / 2.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_HIGH) defval += (ubound - lbound) * 3. / 4.;

                if (is_log) {
                  defval = exp(defval);
                  lbound = exp(lbound);
                  ubound = exp(ubound);
                }

                if (ladphintdes & LADSPA_HINT_INTEGER) {
                  in_params[cninps] = weed_integer_init(laddes->PortNames[i], label, defval + .5, lbound + .5, ubound + .5);
                  if (dual == WEED_TRUE) in_params[cninps + oninps]
                      = weed_integer_init(laddes->PortNames[i], label, defval + .5, lbound + .5, ubound + .5);
                } else {
                  in_params[cninps] = weed_float_init(laddes->PortNames[i], label, defval, lbound, ubound);
                  if (dual == WEED_TRUE) in_params[cninps + oninps] = weed_float_init(laddes->PortNames[i], label, defval, lbound, ubound);
                }

                if (is_srate) {
                  weed_set_boolean_value(in_params[cninps], "plugin_sample_rate", WEED_TRUE);
                } else weed_set_boolean_value(in_params[cninps], "plugin_sample_rate", WEED_FALSE);

                if (is_log) {
                  weed_set_boolean_value(in_params[cninps], "plugin_logarithmic", WEED_TRUE);
                } else weed_set_boolean_value(in_params[cninps], "plugin_logarithmic", WEED_FALSE);

                if (dual == WEED_TRUE) {
                  if (is_srate) {
                    weed_set_boolean_value(in_params[cninps + oninps], "plugin_sample_rate", WEED_TRUE);
                  } else weed_set_boolean_value(in_params[cninps + oninps],
                                                  "plugin_sample_rate", WEED_FALSE);

                  if (is_log) {
                    weed_set_boolean_value(in_params[cninps + oninps], "plugin_logarithmic", WEED_TRUE);
                  } else weed_set_boolean_value(in_params[cninps + oninps],
                                                  "plugin_logarithmic", WEED_FALSE);
                }

                if (is_log || is_srate) {
                  gui = weed_paramtmpl_get_gui(in_params[cninps]);
                  weed_set_funcptr_value(gui, WEED_LEAF_DISPLAY_VALUE_FUNC,
                                         (weed_funcptr_t)ladspa_disp);
                  if (dual == WEED_TRUE) {
                    gui = weed_paramtmpl_get_gui(in_params[cninps + oninps]);
                    weed_set_funcptr_value(gui, WEED_LEAF_DISPLAY_VALUE_FUNC,
                                           (weed_funcptr_t)ladspa_disp);
                  }
                }
              }
              cninps++;
            } else {
              // out_param
              if (ladphintdes & LADSPA_HINT_TOGGLED) {
                // boolean type
                if (ladphintdes & LADSPA_HINT_DEFAULT_1) {
                  out_params[cnoutps] = weed_out_param_switch_init(laddes->PortNames[i], WEED_TRUE);
                  if (dual == WEED_TRUE) out_params[cnoutps + onoutps] = weed_out_param_switch_init(laddes->PortNames[i], WEED_TRUE);
                } else {
                  out_params[cnoutps] = weed_out_param_switch_init(laddes->PortNames[i], WEED_FALSE);
                  if (dual == WEED_TRUE) out_params[cnoutps + onoutps] = weed_out_param_switch_init(laddes->PortNames[i], WEED_FALSE);
                }
              } else {
                // int/float type
                lbound = ladphint.LowerBound;
                ubound = ladphint.UpperBound;

                if (ladphintdes & LADSPA_HINT_SAMPLE_RATE) {
                  is_srate = 1;
                }
                if (ladphintdes & LADSPA_HINT_LOGARITHMIC) {
                  is_log = 1;
                }

                if (lbound == 0. && ubound == 0.) {
                  lbound = 0.;
                  ubound = 1.;
                  if (is_log) ubound = log(4.);
                }

                defval = lbound;

                if (ladphintdes & LADSPA_HINT_DEFAULT_MAXIMUM) defval = ubound;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_0) defval = 0.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_1) defval = 1.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_100) defval = 100.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_440) defval = 440.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_LOW) defval += (ubound - lbound) / 4.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_MIDDLE) defval += (ubound - lbound) / 2.;
                else if (ladphintdes & LADSPA_HINT_DEFAULT_HIGH) defval += (ubound - lbound) * 3. / 4.;

                if (is_log) {
                  defval = exp(defval);
                  lbound = exp(lbound);
                  ubound = exp(ubound);
                }

                if (ladphintdes & LADSPA_HINT_INTEGER) {
                  out_params[cnoutps] = weed_out_param_integer_init(laddes->PortNames[i], defval + .5,
                                        lbound + .5, ubound + .5);
                  if (dual == WEED_TRUE) out_params[cnoutps + onoutps] =
                      weed_out_param_integer_init(laddes->PortNames[i], defval + .5, lbound + .5,
                                                  ubound + .5);
                } else {
                  out_params[cnoutps] = weed_out_param_float_init(laddes->PortNames[i], defval, lbound, ubound);
                  if (dual == WEED_TRUE) out_params[cnoutps + onoutps] =
                      weed_out_param_float_init(laddes->PortNames[i], defval, lbound, ubound);
                }
              }

              if (is_srate) {
                weed_set_boolean_value(out_params[cnoutps], "plugin_sample_rate", WEED_TRUE);
              } else weed_set_boolean_value(out_params[cnoutps], "plugin_sample_rate", WEED_FALSE);

              if (is_log) {
                weed_set_boolean_value(out_params[cnoutps], "plugin_logarithmic", WEED_TRUE);
              } else weed_set_boolean_value(out_params[cnoutps], "plugin_logarithmic", WEED_FALSE);

              if (dual == WEED_TRUE) {
                if (is_srate) {
                  weed_set_boolean_value(out_params[cnoutps + onoutps], "plugin_sample_rate", WEED_TRUE);
                } else weed_set_boolean_value(out_params[cnoutps + onoutps],
                                                "plugin_sample_rate", WEED_FALSE);

                if (is_log) {
                  weed_set_boolean_value(out_params[cnoutps + onoutps], "plugin_logarithmic", WEED_TRUE);
                } else weed_set_boolean_value(out_params[cnoutps + onoutps],
                                                "plugin_logarithmic", WEED_FALSE);
              }

              cnoutps++;
            }
          }
        }

        snprintf(weed_name, PATH_MAX, "%s", laddes->Name);

        filter_class = weed_filter_class_init(weed_name, "LADSPA developers", 0, 0,
                                              NULL, ladspa_init, ladspa_process, ladspa_deinit,
                                              in_chantmpls, out_chantmpls, in_params, out_params);

        weed_set_int64_value(filter_class, "unique_id", laddes->UniqueID);

        weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, laddes->Maker);
        weed_set_string_value(filter_class, WEED_LEAF_LICENSE, laddes->Copyright);

        if (num_plugins == 0) weed_set_voidptr_value(filter_class, "plugin_handle", handle);

        weed_set_voidptr_value(filter_class, "plugin_lad_descriptor", laddes);
        weed_set_funcptr_value(filter_class, "plugin_lad_instantiate_func", (weed_funcptr_t)lad_instantiate_func);
        weed_set_funcptr_value(filter_class, "plugin_lad_activate_func", (weed_funcptr_t)lad_activate_func);
        weed_set_funcptr_value(filter_class, "plugin_lad_deactivate_func", (weed_funcptr_t)lad_deactivate_func);
        weed_set_funcptr_value(filter_class, "plugin_lad_connect_port_func", (weed_funcptr_t)lad_connect_port_func);
        weed_set_funcptr_value(filter_class, "plugin_lad_run_func", (weed_funcptr_t)lad_run_func);
        weed_set_funcptr_value(filter_class, "plugin_lad_cleanup_func", (weed_funcptr_t)lad_cleanup_func);

	if (dual) weed_set_boolean_value(filter_class, "plugin_dual", WEED_TRUE);
	else weed_set_boolean_value(filter_class, "plugin_dual", WEED_FALSE);

	if (dual == WEED_TRUE || (!dual && ninchs == 2)) {
          if (ninps > 0) {
            gui = weed_filter_get_gui(filter_class);
            weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
            weed_set_string_value(gui, "layout_rfx_delim", "|");

            // subtract 1 from ninps since we incremented it
            weed_set_string_array(gui, "layout_rfx_strings", numstr, rfx_strings);

          }
        }

	if (ninchs == 2 && !dual) {
          weed_free(rfx_strings[0]);
          weed_free(rfx_strings[1]);
	}
	else {
	  for (wnum = 0; wnum < ninps + stcount2 + stcount - 2; wnum++) {
	    weed_free(rfx_strings[wnum]);
	  }
	}
        weed_free(rfx_strings);
        rfx_strings = NULL;

        if (in_params) weed_free(in_params);
        if (out_params) weed_free(out_params);
        if (in_chantmpls) weed_free(in_chantmpls);
        if (out_chantmpls) weed_free(out_chantmpls);
        in_params = out_params = in_chantmpls = out_chantmpls = NULL;

        weed_set_int_value(filter_class, "plugin_in_channels", oninchs);
        weed_set_int_value(filter_class, "plugin_out_channels", onoutchs);
        weed_set_int_value(filter_class, "plugin_in_params", oninps);
        weed_set_int_value(filter_class, "plugin_out_params", onoutps);

        if (*(laddes->Name)) {
          list = add_to_list_sorted(list, filter_class, (!strncmp(laddes->Name, "Mag", 3) ? "foobar" : laddes->Name));
        }
        num_filters++;
        if (verbosity == WEED_VERBOSITY_DEBUG)
          fprintf(stderr, "DONE\n");
      }
    }
  }

  if (!num_filters) {
    if (verbosity >= WEED_VERBOSITY_CRITICAL) {
      fprintf(stderr, "No LADSPA plugins found; if you have them installed please set the LADSPA_PATH"
              "environment variable to point to them.\n");
    }
    return NULL;
  }

  add_filters_from_list(plugin_info, list);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;
