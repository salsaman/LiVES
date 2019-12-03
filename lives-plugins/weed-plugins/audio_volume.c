// audio_volume.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2008
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_AUDIO

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdio.h>

/**
   Provides a multi stream mixer with volume per stream, pan,
   and the option to swap left/righ channels (with or without also swapping the pan).
   The first in channel can be inplaced with the out channel.

   The plugin utilizes a few special features of Weed. First it sets the filter flag bit:
   WEED_FILTER_IS_CONVERTER. This allows it to mix multipl input streams into one output,
   and also permits remapping of channels within the layout (i.e. swap left / right).

   It also sets the filter flag WEED_FILTER_PROCESS last, so the host should place it as near to the end of
   the audio filter chain as possible.

   Second, only one input channel is created, however the "max_repeats" is set to 0, which means the host may create an
   unlimited number of copies of the in channel.

   Finally, one of the parameters in marked "is_volume_master". This, combined with being a converter,
   allows the plugin to adjust the volume levels between in and out. Additionally the parameter is marked with
   WEED_PARAMETER_VALUE_PER_CHANNEL | WEED_PARAMETER_VARIABLE_SIZE, which hints the host that the values
   are mapped one-one to in channels. The other parameters also set these flags, so the host knows the values are also per channel.

   As an added nicety, the plugin will hint the host to hide the panning and swap controls if the input is mono.
*/
static weed_error_t  avol_init(weed_plant_t *inst) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t *gui = weed_param_get_gui(in_params[1]);
  weed_plant_t *gui2 = weed_param_get_gui(in_params[2]);
  weed_plant_t *gui3 = weed_param_get_gui(in_params[3]);
  int achans = weed_channel_get_naudchans(in_chan);

  weed_free(in_params);

  // hide the "pan" and "swap" controls if we are using mono audio
  if (achans != 2) {
    weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
    weed_set_boolean_value(gui2, WEED_LEAF_HIDDEN, WEED_TRUE);
    weed_set_boolean_value(gui3, WEED_LEAF_HIDDEN, WEED_TRUE);
  } else {
    weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_FALSE);
    weed_set_boolean_value(gui2, WEED_LEAF_HIDDEN, WEED_FALSE);
    weed_set_boolean_value(gui3, WEED_LEAF_HIDDEN, WEED_FALSE);
  }

  return WEED_SUCCESS;
}


static weed_error_t  avol_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int ntracks = 0, chans = 0;
  weed_plant_t **in_channels = weed_get_in_channels(inst, &ntracks);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  float **odst = weed_channel_get_audio_data(out_channel, &chans);
  float **dst = weed_channel_get_audio_data(out_channel, NULL);
  float **src = weed_channel_get_audio_data(in_channels[0], NULL);
  int nsamps = weed_channel_get_audio_length(out_channel), orig_nsamps = nsamps;
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);

  // this is marked as WEED_LEAF_IS_VOLUME_MASTER in setup
  // therefore it must linearly adjust volume between 0.0 and 1.0 for all audio streams
  double *vol = weed_param_get_array_double(in_params[0],  NULL);
  double *pan = weed_param_get_array_double(in_params[1], NULL);
  int *swapchans = weed_param_get_array_boolean(in_params[2], NULL);
  int *swappan = weed_param_get_array_boolean(in_params[3], NULL);

  double voll, volr;
  float tmp;
  int i;

  weed_free(in_params);
  voll = volr = vol[0];

  if (chans == 2) {
    if (pan[0] < 0.) volr *= (1. + pan[0]);
    else voll *= (1. - pan[0]);
    if (swapchans[0] == WEED_FALSE) {
      while (nsamps--) {
        *(dst[0]++) = voll * (*(src[0]++));
        *(dst[1]++) = volr * (*(src[1]++));
      }
    } else {
      if (swappan[0]) {
        tmp = voll;
        voll = volr;
        volr = tmp;
      }
      while (nsamps--) {
        tmp = volr * (*(src[0]++)); // in case inplace, src[0] will become dst[0]
        *(dst[0]++) = voll * (*(src[1]++));
        *(dst[1]++) = tmp;
      }
    }
  } else if (chans == 1) {
    while (nsamps--) {
      *(dst[0]++) = vol[0] * (*(src[0]++));
    }
  }

  for (i = 1; i < ntracks; i++) {
    if (in_channels[i] == NULL) continue;
    if (weed_channel_is_disabled(in_channels[i])) continue;
    if (vol[i] == 0.) continue;
    dst[0] = odst[0];
    chans = weed_get_int_value(in_channels[i], WEED_LEAF_AUDIO_CHANNELS, NULL);
    if (chans == 2) dst[1] = odst[1];

    nsamps = orig_nsamps = weed_get_int_value(in_channels[i], WEED_LEAF_AUDIO_DATA_LENGTH, NULL);
    if (src != NULL) weed_free(src);
    src = (float **)weed_get_voidptr_array(in_channels[i], WEED_LEAF_AUDIO_DATA, NULL);

    voll = volr = vol[i];

    if (chans == 2) {
      if (pan[i] < 0.) volr *= (1. + pan[i]);
      else voll *= (1. - pan[i]);
      if (swapchans[i] == WEED_FALSE) {
        while (nsamps--) {
          *(dst[0]++) += voll * (*(src[0]++));
          *(dst[1]++) += volr * (*(src[1]++));
        }
      } else {
        if (swappan[i]) {
          tmp = voll;
          voll = volr;
          volr = tmp;
        }
        while (nsamps--) {
          tmp = volr * (*(src[0]++)); // in case inplace, src[0] will become dst[0]
          *(dst[0]++) += voll * (*(src[1]++));
          *(dst[1]++) += tmp;
        }
      }
    } else if (chans == 1) {
      while (nsamps--) {
        *(dst[0]++) += vol[0] * (*(src[0]++));
      }
    }
  }

  if (odst) weed_free(odst);
  if (swapchans) weed_free(swapchans);
  if (swappan) weed_free(swappan);
  if (dst) weed_free(dst);
  if (src) weed_free(src);
  if (vol) weed_free(vol);
  if (pan) weed_free(pan);
  if (in_channels) weed_free(in_channels);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("in channel 0",
                                  WEED_CHANNEL_REINIT_ON_AUDIO_LAYOUT_CHANGE), NULL
                                 };
  weed_plant_t *out_chantmpls[] = {weed_audio_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *in_params[] = {weed_float_init("volume", "_Volume", 1.0, 0.0, 1.0), weed_float_init("pan", "_Pan", 0., -1., 1.),
                               weed_switch_init("swap", "_Swap left and right channels", WEED_FALSE),
                               weed_switch_init("swappan", "_Swap panning when channels swap", WEED_TRUE),
                               NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("audio volume and pan", "salsaman", 2,
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
  weed_set_boolean_value(in_params[2], WEED_LEAF_NEW_DEFAULT, WEED_FALSE);
  weed_set_int_value(in_params[3], WEED_LEAF_FLAGS, WEED_PARAMETER_VARIABLE_SIZE | WEED_PARAMETER_VALUE_PER_CHANNEL);
  weed_set_boolean_value(in_params[3], WEED_LEAF_NEW_DEFAULT, WEED_TRUE);

  weed_set_boolean_value(in_params[0], WEED_LEAF_IS_VOLUME_MASTER, WEED_TRUE);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

