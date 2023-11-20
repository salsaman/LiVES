// audio_fft.c
// weed plugin to do audio fft sampling
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_AUDIO

#include <weed/weed-plugin.h>
#include <weed/weed-utils.h>
#include <weed/weed-plugin-utils.h>

#include <weed/weed-plugin-utils/weed-plugin-utils.c>

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
  int x = 2;
  for (int j = 0; j < i; j++) x *= 2;
  return x;
}


static int create_plans(void) {
  int nsamps;

  for (int i = 0; i < MAXPLANS; i++) {
    // create fftw plan
    nsamps = twopow(i);

    ins[i] = (float *) fftwf_malloc(nsamps * sizeof(float));
    if (!ins[i]) {
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    outs[i] = (fftwf_complex *) fftwf_malloc(nsamps * sizeof(fftwf_complex));
    if (!outs[i]) {
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    plans[i] = fftwf_plan_dft_r2c_1d(nsamps, ins[i], outs[i], i < 13 ? FFTW_MEASURE : FFTW_ESTIMATE);
  }
  return WEED_SUCCESS;
}


/////////////////////////////////////////////////////////////

static weed_error_t fftw_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  float **src = weed_channel_get_audio_data(in_channel, NULL);

  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t *out_param = weed_get_out_param(inst, 0);

  double freq = weed_param_get_value_double(in_params[0]);

  float tot = 0.;
  int chans, nsamps, onsamps, base, rate, k;

  weed_free(in_params);

  onsamps = weed_channel_get_audio_length(in_channel);

  if (onsamps < 2) {
    weed_set_double_value(out_param, WEED_LEAF_VALUE, 0.);
    return WEED_SUCCESS;
  }

  base = rndlog2(onsamps);
  nsamps = twopow(base);

  rate = weed_channel_get_audio_rate(in_channel);

  // which element do we want for output ?
  // out array goes from 0 to (nsamps/2 + 1) [div b y 2 rounded down]

  // nyquist freq is rate / 2
  // so the freq. of the kth element is: f  =  k/nsamps * rate
  // therefore k = f/rate * nsamps

  k = freq / (double)rate * (double)nsamps;

  if (k > (nsamps >> 1)) {
    weed_set_double_value(out_param, WEED_LEAF_VALUE, 0.);
    return WEED_SUCCESS;
  }

  chans = weed_channel_get_naudchans(in_channel);

  for (int i = 0; i < chans; i++) {
    // do transform for each channel

    // copy in data to sdata->in
    weed_memcpy(ins[base], src[i], nsamps * sizf);

    //fprintf(stderr,"executing plan of size %d\n",sdata->size);
    fftwf_execute(plans[base]);

    tot += sqrtf(outs[base][k][0] * outs[base][k][0] + outs[base][k][1] * outs[base][k][1]);
  }

  //fprintf(stderr,"tot is %f\n",tot);

  // average over all audio channels
  weed_set_double_value(out_param, WEED_LEAF_VALUE, (double)(tot / (float)chans));
  weed_free(src);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  if (create_plans() != WEED_SUCCESS) return NULL;
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_float_init("freq", "_Frequency", 2000., 0.0, 22000.0), NULL};
  weed_plant_t *out_params[] = {weed_out_param_float_init(WEED_LEAF_VALUE, 0., 0., 1.), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("audio fft analyser", "salsaman", 1, 0, NULL, NULL, fftw_process,
                               NULL, in_chantmpls, NULL, in_params, out_params);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, "Fast Fourier Transform for audio");
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;


WEED_DESETUP_START {
  for (int i = 0; i < MAXPLANS; i++) {
    fftwf_destroy_plan(plans[i]);
    fftwf_free(ins[i]);
    fftwf_free(outs[i]);
  }
}
WEED_DESETUP_END;

