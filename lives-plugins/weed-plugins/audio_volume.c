// audio_volume.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2008
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////


static weed_error_t  avol_init(weed_plant_t *inst) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  int chans = weed_get_int_value(in_channel, WEED_LEAF_AUDIO_CHANNELS, NULL);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  weed_plant_t *paramt = weed_get_plantptr_value(in_params[1], WEED_LEAF_TEMPLATE, NULL);
  weed_plant_t *gui = weed_parameter_template_get_gui(paramt);
  weed_plant_t *paramt2 = weed_get_plantptr_value(in_params[2], WEED_LEAF_TEMPLATE, NULL);
  weed_plant_t *gui2 = weed_parameter_template_get_gui(paramt2);

  weed_free(in_params);

  // hide the "pan" and "swap" controls if we are using mono audio
  if (chans != 2) {
    weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
    weed_set_boolean_value(gui2, WEED_LEAF_HIDDEN, WEED_TRUE);
  } else {
    weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_FALSE);
    weed_set_boolean_value(gui2, WEED_LEAF_HIDDEN, WEED_FALSE);
  }

  return WEED_SUCCESS;
}


static weed_error_t  avol_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL),
                 *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  float *src;
  float *odst = weed_get_voidptr_value(out_channel, WEED_LEAF_AUDIO_DATA, NULL), *dst = odst;

  register int nsamps;

  int orig_nsamps;
  int chans;
  int inter;

  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  double voll, volr;

  // this is marked as WEED_LEAF_IS_VOLUME_MASTER in setup
  // therefore it must linearly adjust volume between 0.0 and 1.0 for all sub-channels
  double *vol = weed_get_double_array(in_params[0], WEED_LEAF_VALUE, NULL);
  double *pan = weed_get_double_array(in_params[1], WEED_LEAF_VALUE, NULL);
  int swapchans = weed_get_boolean_value(in_params[2], WEED_LEAF_VALUE, NULL);

  int ntracks = weed_leaf_num_elements(inst, WEED_LEAF_IN_CHANNELS);

  register int i;

  weed_free(in_params);

  chans = weed_get_int_value(in_channels[0], WEED_LEAF_AUDIO_CHANNELS, NULL);

  voll = volr = vol[0];

  if (chans == 2) {
    if (pan[0] < 0.) volr *= (1. + pan[0]);
    else voll *= (1. - pan[0]);
  }

  nsamps = orig_nsamps = weed_get_int_value(in_channels[0], WEED_LEAF_AUDIO_DATA_LENGTH, NULL);
  src = weed_get_voidptr_value(in_channels[0], WEED_LEAF_AUDIO_DATA, NULL);
  inter = weed_get_boolean_value(in_channels[0], WEED_LEAF_AUDIO_INTERLEAF, NULL);

  if (chans == 2) {
    if (swapchans == WEED_FALSE) {
      while (nsamps--) {
        *(dst++) = voll * (*(src++));
        if (inter) *(dst++) = volr * (*(src++));
      }
      if (!inter) {
        nsamps = orig_nsamps;
        while (nsamps--) {
          *(dst++) = volr * (*(src++));
        }
      }
    } else {
      // swap l/r channels
      if (!inter) src += nsamps;
      else src++;
      while (nsamps--) {
        if (inter) {
          *(dst++) = voll * (*(src--));
          *(dst++) = volr * (*(src++));
          src++;
        } else *(dst++) = voll * (*(src++));
      }
      if (!inter) {
        nsamps = orig_nsamps;
        src -= nsamps * 2;
        while (nsamps--) {
          *(dst++) = volr * (*(src++));
        }
      }
    }
  } else if (chans == 1) {
    while (nsamps--) {
      *(dst++) = vol[0] * (*(src++));
    }
  }

  for (i = 1; i < ntracks; i++) {
    if (weed_plant_has_leaf(in_channels[i], WEED_LEAF_DISABLED) &&
        weed_get_boolean_value(in_channels[i], WEED_LEAF_DISABLED, NULL) == WEED_TRUE) continue;
    if (vol[i] == 0.) continue;
    dst = odst;
    nsamps = orig_nsamps = weed_get_int_value(in_channels[i], WEED_LEAF_AUDIO_DATA_LENGTH, NULL);
    src = weed_get_voidptr_value(in_channels[i], WEED_LEAF_AUDIO_DATA, NULL);

    inter = weed_get_boolean_value(in_channels[i], WEED_LEAF_AUDIO_INTERLEAF, NULL);

    chans = weed_get_int_value(in_channels[i], WEED_LEAF_AUDIO_CHANNELS, NULL);

    voll = volr = vol[i];

    if (chans == 2) {
      if (pan[i] < 0.) volr *= (1. + pan[i]);
      else voll *= (1. - pan[i]);
    }

    if (chans == 2) {
      while (nsamps--) {
        *(dst++) += voll * (*(src++));
        if (inter) *(dst++) += volr * (*(src++));
      }
      if (!inter) {
        nsamps = orig_nsamps;
        while (nsamps--) {
          *(dst++) += volr * (*(src++));
        }
      }
    } else if (chans == 1) {
      while (nsamps--) {
        *(dst++) += vol[i] * (*(src++));
      }
    }
  }

  weed_free(vol);
  weed_free(pan);
  weed_free(in_channels);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_audio_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *in_params[] = {weed_float_init("volume", "_Volume", 1.0, 0.0, 1.0), weed_float_init("pan", "_Pan", 0., -1., 1.),
                               weed_switch_init("swap", "_Swap left and right channels", WEED_FALSE), NULL
                              };
  weed_plant_t *filter_class = weed_filter_class_init("audio volume and pan", "salsaman", 1,
                               WEED_FILTER_IS_CONVERTER | WEED_FILTER_PROCESS_LAST, NULL,
                               avol_init,
                               avol_process,
                               NULL, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_set_int_value(in_chantmpls[0], WEED_LEAF_MAX_REPEATS, 0); // set optional repeats of this channel

  weed_set_int_value(in_params[0], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);
  weed_set_double_value(in_params[0], WEED_LEAF_NEW_DEFAULT, 1.0);
  weed_set_int_value(in_params[1], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);
  weed_set_double_value(in_params[1], WEED_LEAF_NEW_DEFAULT, 0.0);
  weed_set_int_value(in_params[2], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);
  weed_set_double_value(in_params[2], WEED_LEAF_NEW_DEFAULT, WEED_FALSE);

  // set is_volume_master from weedaudio spec 110
  weed_set_boolean_value(in_params[0], WEED_LEAF_IS_VOLUME_MASTER, WEED_TRUE);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

