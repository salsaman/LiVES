// audio_fft.c
// weed plugin to do audio fft sampling
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-utils.h"
#endif

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // optional


///// plugin internal functions  ///////

#include <string.h>

#include <fftw3.h>
#include <math.h>

#define MAXPLANS 18

static size_t sizf = sizeof(float);

static float *ins[MAXPLANS];
static fftwf_complex *outs[MAXPLANS];
static fftwf_plan plans[MAXPLANS];

static int rndlog2(int i) {
  // return (int)log2(i) - 1
  int x = 2, val = -1;

  while (x <= i) {
    x *= 2;
    val++;
  }
  return val;
}


static int twopow(int i) {
  // return 2**(i+1)
  register int j, x = 2;

  for (j = 0; j < i; j++) x *= 2;

  return x;
}


static int create_plans(void) {
  register int i, nsamps;

  for (i = 0; i < MAXPLANS; i++) {
    // create fftw plan
    nsamps = twopow(i);

    ins[i] = (float *) fftwf_malloc(nsamps * sizeof(float));
    if (ins[i] == NULL) {
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    outs[i] = (fftwf_complex *) fftwf_malloc(nsamps * sizeof(fftwf_complex));
    if (outs[i] == NULL) {
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    plans[i] = fftwf_plan_dft_r2c_1d(nsamps, ins[i], outs[i], i < 13 ? FFTW_MEASURE : FFTW_ESTIMATE);
  }
  return WEED_NO_ERROR;
}


/////////////////////////////////////////////////////////////

static int fftw_process(weed_plant_t *inst, weed_timecode_t tc) {
  int error;
  int chans, nsamps, onsamps, base, inter, rate, k;

  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", &error);
  float *src = (float *)weed_get_voidptr_value(in_channel, "audio_data", &error);

  weed_plant_t **in_params = weed_get_plantptr_array(inst, "in_parameters", &error);
  weed_plant_t *out_param = weed_get_plantptr_value(inst, "out_parameters", &error);

  double freq = weed_get_double_value(in_params[0], "value", &error);

  float tot = 0.;

  register int i, j;

  weed_free(in_params);

  onsamps = weed_get_int_value(in_channel, "audio_data_length", &error);

  if (onsamps < 2) {
    weed_set_double_value(out_param, "value", 0.);
    weed_set_int64_value(out_param, "timecode", tc);
    return WEED_NO_ERROR;
  }

  base = rndlog2(onsamps);
  nsamps = twopow(base);

  rate = weed_get_int_value(in_channel, "audio_rate", &error);

  // which element do we want for output ?
  // out array goes from 0 to (nsamps/2 + 1) [div b y 2 rounded down]

  // nyquist freq is rate / 2
  // so the freq. of the kth element is: f  =  k/nsamps * rate
  // therefore k = f/rate * nsamps

  k = freq / (double)rate * (double)nsamps;

  if (k > (nsamps >> 1)) {
    weed_set_double_value(out_param, "value", 0.);
    weed_set_int64_value(out_param, "timecode", tc);
    return WEED_NO_ERROR;
  }

  chans = weed_get_int_value(in_channel, "audio_channels", &error);
  inter = weed_get_boolean_value(in_channel, "audio_interleaf", &error);

  for (i = 0; i < chans; i++) {
    // do transform for each channel

    // copy in data to sdata->in
    if (inter == WEED_FALSE) {
      // non-interleaved
      weed_memcpy(ins[base], src, nsamps * sizf);
      src += onsamps;
    } else {
      // interleaved
      for (j = 0; j < nsamps; j++) {
        ins[base][j] = src[j * chans];
      }
      src++;
    }

    //fprintf(stderr,"executing plan of size %d\n",sdata->size);
    fftwf_execute(plans[base]);

    tot += sqrtf(outs[base][k][0] * outs[base][k][0] + outs[base][k][1] * outs[base][k][1]);
  }

  //fprintf(stderr,"tot is %f\n",tot);

  // average over all audio channels
  weed_set_double_value(out_param, "value", (double)(tot / (float)chans));
  weed_set_int64_value(out_param, "timecode", tc);

  return WEED_NO_ERROR;
}


WEED_SETUP_START(200, 200) {
  if (create_plans() != WEED_NO_ERROR) return NULL;
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_float_init("freq", "_Frequency", 2000., 0.0, 22000.0), NULL};
  weed_plant_t *out_params[] = {weed_out_param_float_init("value", 0., 0., 1.), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("audio fft analyser", "salsaman", 1, 0, NULL, &fftw_process,
                               NULL, in_chantmpls, NULL, in_params, out_params);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_string_value(filter_class, "description", "Fast Fourier Transform for audio");

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;


WEED_DESETUP_START {
  register int i;
  for (i = 0; i < MAXPLANS; i++) {
    fftwf_destroy_plan(plans[i]);
    fftwf_free(ins[i]);
    fftwf_free(outs[i]);
  }
}
WEED_DESETUP_END;

