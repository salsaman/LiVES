// audio_fft.c
// weed plugin to do audio fft sampling
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-effects.h>
#include <weed/weed-plugin.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"
#endif


///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]={131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-palettes.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed.h" // optional
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-palettes.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include <string.h>
#include <stdio.h>

#include <fftw3.h>
#include <math.h>

typedef struct {
  int size;
  float *in;
  fftwf_complex *out;
  fftwf_plan p;
} _sdata;

static size_t sizf=sizeof(float);

/////////////////////////////////////////////////////////////


int fftw_init(weed_plant_t *inst) {
  int error;
  int nsamps;

  _sdata *sdata;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  sdata=(_sdata *)weed_malloc(sizeof(_sdata));
  if (sdata==NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  nsamps=weed_get_int_value(in_channel,"audio_data_length",&error);

  // create fftw plan

  sdata->in = (float*) fftwf_malloc(sizf * nsamps);
  if (sdata->in==NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * nsamps);
  if (sdata->out==NULL) {
    fftwf_free(sdata->in);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->p = fftwf_plan_dft_r2c_1d(nsamps, sdata->in, sdata->out, FFTW_ESTIMATE);

  sdata->size=nsamps;

  weed_set_voidptr_value(inst,"plugin_data",sdata);

  return WEED_NO_ERROR;

}


int fftw_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_data",&error);

  if (sdata!=NULL) {
    fftwf_destroy_plan(sdata->p);
    fftwf_free(sdata->in);
    fftwf_free(sdata->out);
    
    weed_free(sdata);
  }

  return WEED_NO_ERROR;

}



int fftw_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  int chans,nsamps,inter,rate,k;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  float *src=(float *)weed_get_voidptr_value(in_channel,"audio_data",&error);

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t *out_param=weed_get_plantptr_value(inst,"out_parameters",&error);

  double freq=weed_get_double_value(in_params[0],"value",&error);

  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_data",&error);

  float tot=0.;

  register int i,j;

  weed_free(in_params);

  nsamps=weed_get_int_value(in_channel,"audio_data_length",&error);

  if (nsamps==0) {
    weed_set_double_value(out_param,"value",0.);
    return WEED_NO_ERROR;
  }

  if (nsamps!=sdata->size) {
    fftw_deinit(inst);
    fftw_init(inst);
  }

  rate=weed_get_int_value(in_channel,"audio_rate",&error);

  // which element do we want for output ?
  // out array goes from 0 to (nsamps/2 + 1) [div b y 2 rounded down]

  // nyquist freq is rate / 2
  // so the freq. of the kth element is: f  =  k/nsamps * rate
  // therefore k = f/rate * nsamps

  k = freq/(double)rate*(double)nsamps;

  if (k>(nsamps>>1)) {
    weed_set_double_value(out_param,"value",0.);
    return WEED_NO_ERROR;
  }

  chans=weed_get_int_value(in_channel,"audio_channels",&error);
  inter=weed_get_boolean_value(in_channel,"audio_interleaf",&error);

  for (i=0;i<chans;i++) {
    // do transform for each channel

    // copy in data to sdata->in
    if (inter==WEED_FALSE) {
      // non-interleaved
      weed_memcpy(sdata->in,src,nsamps*sizf);
      src+=nsamps;
    }
    else {
      // interleaved
      for (j=0;j<nsamps;j++) {
	sdata->in[j]=src[j*chans];
      }
      src++;
    }

    //fprintf(stderr,"executing plan of size %d\n",sdata->size);
    fftwf_execute(sdata->p);

    tot+=sqrtf(sdata->out[k][0]*sdata->out[k][0]+sdata->out[k][1]*sdata->out[k][1]);

  }

  //fprintf(stderr,"tot is %f\n",tot);

  // average over all audio channels
  weed_set_double_value(out_param,"value",(double)(tot/(float)chans));


  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    weed_plant_t *in_chantmpls[]={weed_audio_channel_template_init("in channel 0",0),NULL};
    weed_plant_t *in_params[]={weed_float_init("freq","_Frequency",2000.,0.0,22000.0),NULL};
    weed_plant_t *out_params[]={weed_out_param_float_init("value",0.,0.,1.),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("audio fft analyser","salsaman",1,0,&fftw_init,&fftw_process,
						      &fftw_deinit,in_chantmpls,NULL,in_params,out_params);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

