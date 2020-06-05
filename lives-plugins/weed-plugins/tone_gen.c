// tone_gen.c
// weed plugin to generate simple audio tones
// (c) G. Finch (salsaman) 2012
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

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <stdio.h>

static int resample(weed_plant_t *inst, float **inbuf, float **outbuf, int nsamps, int nchans, int irate, int orate) {
  // resample (time stretch) nsamps samples from inbuf at irate to outbuf at outrate
  // return how many samples in in were consumed

  // we maintain the same number of channels

  register float src_offset_f = weed_get_double_value(inst, "plugin_remainder", NULL);
  register int src_offset_i = 0;
  register int i, j;
  register double scale;

  double rem, endv;

  scale = (double)irate / (double)orate;

  for (i = 0; i < nsamps; i++) {
    // process each sample
    for (j = 0; j < nchans; j++) {
      outbuf[j][i] = inbuf[j][src_offset_i];
    }
    // resample on the fly
    src_offset_i = (int)((src_offset_f += scale));
  }

  endv = outbuf[0][nsamps - 1];
  rem = src_offset_f - (double)src_offset_i;
  if (rem < scale) {
    endv = -endv;
  }
  weed_set_double_value(inst, "plugin_stval", endv);
  weed_set_double_value(inst, "plugin_remainder", rem);
  return src_offset_i;
}

/////////////////////////////////////////////////////////////

static weed_error_t tonegen_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  float **dst = weed_channel_get_audio_data(out_channel, NULL), **buff;

  double freq = weed_param_get_value_double(in_params[0]);
  double mult = weed_param_get_value_double(in_params[1]);
  double trate = freq * mult;
  double stval = weed_get_double_value(inst, "plugin_stval", NULL);

  int chans = weed_channel_get_naudchans(out_channel);
  int nsamps = weed_channel_get_audio_length(out_channel);
  int rate = weed_channel_get_audio_rate(out_channel);

  int nrsamps, i, j;

  weed_free(in_params); /// because we got an array

  if (trate < 0.) trate = -trate;
  if (trate < 50.) trate = 50.;

  if (stval != -1.) stval = 1.;

  nrsamps = ((double)nsamps * (double)rate / (double)trate);
  nrsamps = (((nrsamps + 63) >> 6) << 4);    /// divide by 64, mply by 16 gives the rounded size in sizeof(float)
  buff = (float **)weed_calloc(chans, sizeof(float *));
  for (i = 0; i < chans; i++) {
    buff[i] = (float *)weed_calloc(nrsamps,
                                   sizeof(float));    /// sizeof(float) == 4 so this is the original val rounded up to n * 64
  }

  // set a square wave at freq MYRATE, then resample it to trate
  for (i = 0; i < nrsamps; i += 2) {
    for (j = 0; j < chans; j++) {
      buff[j][i] = stval;
      if (i < nrsamps - 1) {
        buff[j][i + 1] = -stval;
      }
    }
  }

  resample(inst, buff, dst, nsamps, chans, trate, rate);

  for (i = 0; i < chans; i++) {
    if (buff[i] != NULL) weed_free(buff[i]);
  }

  if (buff) weed_free(buff);
  if (dst) weed_free(dst);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *out_chantmpls[] = {weed_audio_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_float_init("freq", "_Frequency", 75., 50., 2000.0),
                               weed_float_init("multiplier", "Frequency _Multiplier", 10., 1., 10.), NULL
                              };
  weed_plant_t *filter_class = weed_filter_class_init("tone generator", "salsaman", 1, 0, NULL,
                               NULL, tonegen_process, NULL, NULL, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


