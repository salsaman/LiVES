// beat_detector.c
// weed plugin to do sample and hold beat detection
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

#define NSLICES 54

#define STIME 1000.f // milliseconds to buffer values for

#define BUFMAX 1024

typedef struct {
  int size;
  float *in;
  fftwf_complex *out;
  fftwf_plan p;
  int totsamps;
  int bufidx;
  int bufsize[BUFMAX];
  float av[NSLICES];
  float buf[NSLICES][BUFMAX];
} _sdata;

static size_t sizf=sizeof(float);

float freq[NSLICES]={25.,50.,75.,100.,150.,200.,250.,300.,
		     400.,500.,600.,
		     700.,800.,900.,1000.,1100.,1200.,1300.,1400.,
		     1600.,1800.,2000.,2200.,2400.,2600.,2800.,3000.,
		     3200.,3600.,4000.,4400.,4800,5200.,5600.,6000.,
		     6400.,6800.,
		     7400.,8000.,8600.,9200.,9800.,10400.,11000.,11600.,
		     12400.,13200,14000.,14800.,15600.,16400.,17600.,18800.,
		     20000.};


/////////////////////////////////////////////////////////////

static void sdata_free_plan(_sdata *sdata) {
  if (sdata->size!=0) {
    fftwf_destroy_plan(sdata->p);
    fftwf_free(sdata->in);
    fftwf_free(sdata->out);
  }
}


static int sdata_create_plan(_sdata *sdata, int nsamps) {

  sdata->size=nsamps;

  if (nsamps==0) return WEED_NO_ERROR;

  sdata->in = (float*) fftwf_alloc_real(nsamps);
  if (sdata->in==NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->out = (fftwf_complex*) fftwf_alloc_complex(nsamps);
  if (sdata->out==NULL) {
    fftwf_free(sdata->in);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->p = fftwf_plan_dft_r2c_1d(nsamps, sdata->in, sdata->out, FFTW_ESTIMATE);

  return WEED_NO_ERROR;
}  



int beat_init(weed_plant_t *inst) {
  int error;
  int nsamps;
  int retval;

  _sdata *sdata;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  register int i,j;

  sdata=(_sdata *)weed_malloc(sizeof(_sdata));
  if (sdata==NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  nsamps=weed_get_int_value(in_channel,"audio_data_length",&error);

  // create fftw plan
  retval=sdata_create_plan(sdata,nsamps);
  if (retval!=WEED_NO_ERROR) return retval;


  for (i=0;i<NSLICES;i++) {
    sdata->av[i]=0.;
     for (j=0;j<BUFMAX;j++) {
      sdata->buf[i][j]=0.;
    }
  }

  for (j=0;j<BUFMAX;j++) {
    sdata->bufsize[j]=0;
  }

  sdata->totsamps=0;
  sdata->bufidx=-1;

  weed_set_voidptr_value(inst,"plugin_data",sdata);

  return WEED_NO_ERROR;

}



int beat_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_data",&error);

  if (sdata!=NULL) {
    sdata_free_plan(sdata);
    weed_free(sdata);
  }

  return WEED_NO_ERROR;

}



int beat_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error,retval;
  int chans,nsamps,inter,rate,k;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  float *src=(float *)weed_get_voidptr_value(in_channel,"audio_data",&error);

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);

  int reset=weed_get_boolean_value(in_params[0],"value",&error);
  double avlim=weed_get_double_value(in_params[1],"value",&error);
  double varlim=weed_get_double_value(in_params[2],"value",&error);

  int beat_pulse=WEED_FALSE,beat_hold=weed_get_boolean_value(out_params[0],"value",&error);

  int has_data=WEED_FALSE;

  int kmin,kmax;

  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_data",&error);

  float tot,var,varx,av;

  register int i,j,s;

  weed_free(in_params);

  if (beat_hold==WEED_TRUE) beat_hold=!reset;

  nsamps=weed_get_int_value(in_channel,"audio_data_length",&error);

  if (nsamps==0) {
    beat_pulse=beat_hold=WEED_FALSE;
    goto done;
  }

  if (nsamps!=sdata->size) {
    sdata_free_plan(sdata);
    retval=sdata_create_plan(sdata,nsamps);
    if (retval!=WEED_NO_ERROR) return WEED_ERROR_HARDWARE;
  }

  rate=weed_get_int_value(in_channel,"audio_rate",&error);
    
  chans=weed_get_int_value(in_channel,"audio_channels",&error);
  inter=weed_get_boolean_value(in_channel,"audio_interleaf",&error);

  // have we buffered enough data ?
  if ((float)sdata->totsamps/(float)rate*1000.>=STIME) {
    sdata->totsamps-=sdata->bufsize[0];
    // shift all values up

    for (i=0;i<NSLICES;i++) {
      if (sdata->buf[i][0]!=-1.) sdata->av[i]-=sdata->buf[i][0];

      for (j=0;j<sdata->bufidx;j++) {
	sdata->buf[i][j]=sdata->buf[i][j+1];
      }
    }

    has_data=WEED_TRUE;
  }
  else {
    sdata->bufidx++;
    if (sdata->bufidx==BUFMAX) sdata->bufidx--;
  }


  sdata->totsamps+=nsamps;
  sdata->bufsize[sdata->bufidx]=nsamps;


  for (s=0;s<NSLICES;s++) {
    sdata->buf[s][sdata->bufidx]=0.;
  }

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

    kmin=0;

    for (s=0;s<NSLICES;s++) {
      // which element do we want for output ?
      // out array goes from 0 to (nsamps/2 + 1) [div b y 2 rounded down]
	
      // nyquist freq is rate / 2
      // so the freq. of the kth element is: f  =  k/nsamps * rate
      // therefore k = f/rate * nsamps
	
      kmax = freq[s]/(double)rate*(double)nsamps;
	
      if (kmax>(nsamps>>1)) {
	// frequency invalid - too high for this sample packet
	tot=-1.;
	sdata->buf[s][sdata->bufidx]=tot;
      }
      else {
	tot=0.;

	for (k=kmin;k<kmax;k++) {
	  // sum values over range
	  tot+=sdata->out[k][0]*sdata->out[k][0]+sdata->out[k][1]*sdata->out[k][1];
	}
	// average over range
	tot/=(float)kmax-(float)kmin+1.;

	// average over channels
	tot/=(float)chans;

	// store this value in the buffer
	sdata->buf[s][sdata->bufidx]+=tot;
	sdata->av[s]+=tot;
      }
    } // done for all slices
  } // done for all channels


  if (!has_data) {
    // need to buffer more data
    beat_pulse=beat_hold=WEED_FALSE;
    goto done;
  }



  // now we have the current value in sdata->buf, and the buffered total in sdata->av
  // we can calculate the variance, and then use a formula:
  // if curr > C * av && curr > V * var : trigger a beat


  for (i=0;i<NSLICES;i++) {
    // for the variance:
    var=0.;
    av=sdata->av[i]/(float)sdata->bufidx;

    for (j=0;j<sdata->bufidx;j++) {
      varx=(sdata->buf[i][j]-av);
      var+=varx*varx;
    }
    var/=(float)sdata->bufidx;

    //fprintf(stderr,"%f %f %f  ",var,av,sdata->buf[i][sdata->bufidx]);

    varlim*=varlim;

    if (var>varlim && sdata->buf[i][sdata->bufidx]>avlim*av) {
      // got a beat !
      beat_pulse=beat_hold=WEED_TRUE;
      //fprintf(stderr,"PULSE !\n");
      break;
    }

  }

  //fprintf(stderr,"\n\n");

 done:
  weed_set_boolean_value(out_params[0],"value",beat_hold);
  weed_set_boolean_value(out_params[1],"value",beat_pulse);

  weed_free(out_params);
  
  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    weed_plant_t *in_chantmpls[]={weed_audio_channel_template_init("in channel 0",0),NULL};
    weed_plant_t *in_params[]={weed_switch_init("reset","_Reset hold",WEED_FALSE),weed_float_init("avlim","_Average threshold",1.5,1.,10.),weed_float_init("varlim","_Variance threshold",5.,1.,100.),NULL};
    weed_plant_t *out_params[]={weed_out_param_switch_init("beat hold",WEED_FALSE),weed_out_param_switch_init("beat pulse",WEED_FALSE),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("beat detector","salsaman",1,0,&beat_init,&beat_process,
						      &beat_deinit,in_chantmpls,NULL,in_params,out_params);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

