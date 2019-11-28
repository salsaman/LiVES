// tone_gen.c
// weed plugin to generate simple audio tones
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

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

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

static int resample(float **inbuf, float **outbuf, int nsamps, int nchans, int irate, int orate) {
  // resample (time stretch) nsamps samples from inbuf at irate to outbuf at outrate
  // return how many samples in in were consumed

  // we maintain the same number of channels

  register float src_offset_f = 0.f;
  register int src_offset_i = 0;
  register int i, j;
  register double scale;

  scale = (double)irate / (double)orate;

  for (i = 0; i < nsamps; i++) {
    // process each sample
    for (j = 0; j < nchans; j++) {
      outbuf[j][i] = inbuf[j][src_offset_i];
    }
    // resample on the fly
    src_offset_i = (int)(src_offset_f += scale);
  }
  return src_offset_i;
}


/////////////////////////////////////////////////////////////

static weed_error_t tonegen_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int chans, nsamps, rate, nrsamps;

  weed_plant_t *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  float **dst = (float **)weed_get_voidptr_array(out_channel, WEED_LEAF_AUDIO_DATA, NULL);

  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  double freq = weed_get_double_value(in_params[0], WEED_LEAF_VALUE, NULL);
  double mult = weed_get_double_value(in_params[1], WEED_LEAF_VALUE, NULL);
  double trate;

  float **buff;

  register int i, j;

  weed_free(in_params);

  chans = weed_get_int_value(out_channel, WEED_LEAF_AUDIO_CHANNELS, NULL);
  nsamps = weed_get_int_value(out_channel, WEED_LEAF_AUDIO_DATA_LENGTH, NULL);
  rate = weed_get_int_value(out_channel, WEED_LEAF_AUDIO_RATE, NULL);

  // fill with audio at TRATE
  trate = freq * mult;

  if (trate < 0.) trate = -trate;

  if (trate == 0.) {
    weed_memset(dst, 0, nsamps * chans * sizeof(float));
    return WEED_SUCCESS;
  }

  nrsamps = ((double)nsamps / (double)rate * trate + .5);
  buff = weed_malloc(chans * sizeof(float *));
  for (i = 0; i < chans; i++) {
    buff[i] = weed_malloc(nrsamps * sizeof(float));
  }

  for (i = 0; i < nrsamps; i++) {
    for (j = 0; j < chans; j++) {
      buff[j][i] = 1.;
    }
    i++;
    if (i < nrsamps) {
      for (j = 0; j < chans; j++) {
        buff[j][i] = -1.;
      }
    }
  }

  resample(buff, dst, nsamps, chans, trate, rate);

  for (i = 0; i < chans; i++) {
    if (buff[i] != NULL) weed_free(buff[i]);
  }

  if (buff) weed_free(buff);
  if (dst) weed_free(dst);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *out_chantmpls[] = {weed_audio_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_float_init("freq", "_Frequency", 7500., 0.0, 48000.0),
                               weed_float_init("multiplier", "Frequency _Multiplier", 1., .01, 1000.), NULL
                              };
  weed_plant_t *filter_class = weed_filter_class_init("tone generator", "salsaman", 1, 0, NULL,
                               NULL, tonegen_process, NULL, NULL, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


