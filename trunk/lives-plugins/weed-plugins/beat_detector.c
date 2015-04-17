// beat_detector.c
// weed plugin to do sample and hold beat detection
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-effects.h"
#endif


///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

#include <string.h>

#include <fftw3.h>
#include <math.h>

#define NSLICES 54

#define STIME 1000.f // milliseconds to buffer values for

#define BUFMAX 16384

typedef struct {
  int totsamps;
  int bufidx;
  int bufsize[BUFMAX];
  double av[NSLICES];
  float buf[NSLICES][BUFMAX];
} _sdata;

static size_t sizf=sizeof(float);

float freq[NSLICES]= {25.,50.,75.,100.,150.,200.,250.,300.,
                      400.,500.,600.,
                      700.,800.,900.,1000.,1100.,1200.,1300.,1400.,
                      1600.,1800.,2000.,2200.,2400.,2600.,2800.,3000.,
                      3200.,3600.,4000.,4400.,4800,5200.,5600.,6000.,
                      6400.,6800.,
                      7400.,8000.,8600.,9200.,9800.,10400.,11000.,11600.,
                      12400.,13200,14000.,14800.,15600.,16400.,17600.,18800.,
                      20000.
                     };


#define MAXPLANS 18

#define TWO_PI 2.*M_PI

static float *ins[MAXPLANS];
static fftwf_complex *outs[MAXPLANS];
static fftwf_plan plans[MAXPLANS];

static int rndlog2(int i) {
  // return (int)log2(i) - 1
  int x=2,val=-1;

  while (x<=i) {
    x*=2;
    val++;
  }
  return val;
}


static int twopow(int i) {
  // return 2**(i+1)
  register int j,x=2;

  for (j=0; j<i; j++) x*=2;

  return x;
}


static int create_plans(void) {
  register int i,nsamps;

  for (i=0; i<MAXPLANS; i++) {
    // create fftw plan
    nsamps=twopow(i);

    ins[i] = (float *) fftwf_malloc(nsamps*sizeof(float));
    if (ins[i]==NULL) {
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    outs[i] = (fftwf_complex *) fftwf_malloc(nsamps*sizeof(fftwf_complex));
    if (outs[i]==NULL) {
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    plans[i] = fftwf_plan_dft_r2c_1d(nsamps, ins[i], outs[i], i<13?FFTW_MEASURE:FFTW_ESTIMATE);
  }
  return WEED_NO_ERROR;
}

/////////////////////////////////////////////////////////////


int beat_init(weed_plant_t *inst) {
  _sdata *sdata;

  register int i,j;

  sdata=(_sdata *)weed_malloc(sizeof(_sdata));
  if (sdata==NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (i=0; i<NSLICES; i++) {
    sdata->av[i]=0.;
    for (j=0; j<BUFMAX; j++) {
      sdata->buf[i][j]=0.;
    }
  }

  for (j=0; j<BUFMAX; j++) {
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
    weed_free(sdata);
  }

  return WEED_NO_ERROR;

}



int beat_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  int chans,nsamps,onsamps,base,inter,rate,k;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  float *src=(float *)weed_get_voidptr_value(in_channel,"audio_data",&error);

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);

  int reset=weed_get_boolean_value(in_params[0],"value",&error);
  double avlim=weed_get_double_value(in_params[1],"value",&error);
  double varlim=weed_get_double_value(in_params[2],"value",&error);
  int hamming=weed_get_boolean_value(in_params[3],"value",&error);

  int beat_pulse=WEED_FALSE,beat_hold=weed_get_boolean_value(out_params[1],"value",&error);

  int has_data=WEED_FALSE;

  int kmin,kmax,okmin,rkmin,rkmax;

  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_data",&error);

  double var,av;

  float tot=0.,totx;

  register int i,j,s;

  weed_free(in_params);

  if (beat_hold==WEED_TRUE) beat_hold=!reset;

  onsamps=weed_get_int_value(in_channel,"audio_data_length",&error);

  if (onsamps<2) {
    beat_pulse=beat_hold=WEED_FALSE;
    goto done;
  }


  rate=weed_get_int_value(in_channel,"audio_rate",&error);

  chans=weed_get_int_value(in_channel,"audio_channels",&error);
  inter=weed_get_boolean_value(in_channel,"audio_interleaf",&error);

  // have we buffered enough data ?
  if ((float)sdata->totsamps/(float)rate*1000.>=STIME) {
    sdata->totsamps-=sdata->bufsize[0];
    // shift all values up

    for (i=0; i<NSLICES; i++) {
      sdata->av[i]=0.;

      for (j=0; j<sdata->bufidx; j++) {
        sdata->buf[i][j]=sdata->buf[i][j+1];
        if (sdata->buf[i][j]!=-1.) sdata->av[i]+=(double)sdata->buf[i][j];
      }
    }

    has_data=WEED_TRUE;
  } else {
    sdata->bufidx++;
    if (sdata->bufidx==BUFMAX) {
      //fprintf(stderr,"OVERFLOW\n");
      sdata->bufidx--;
    }
  }

  sdata->totsamps+=onsamps;
  sdata->bufsize[sdata->bufidx]=onsamps;

  for (s=0; s<NSLICES; s++) {
    sdata->buf[s][sdata->bufidx]=0.;
  }

  base=rndlog2(onsamps);
  nsamps=twopow(base);

  for (i=0; i<chans; i++) {
    // do transform for each channel

    // copy in data to sdata->in
    if (inter==WEED_FALSE) {
      // non-interleaved
      if (hamming==WEED_TRUE) {
        for (j=0; j<nsamps; j++) {
          ins[base][j]=src[j]*(0.54f - 0.46f * cosf(TWO_PI*(float)j/(float)(nsamps-1.)));
        }
      } else {
        weed_memcpy(ins[base],src,nsamps*sizf);
      }
      src+=onsamps;
    } else {
      // interleaved
      for (j=0; j<nsamps; j++) {
        if (hamming==WEED_TRUE) {
          ins[base][j]=src[j*chans]*(0.54f - 0.46f * cosf(TWO_PI*(float)j/(float)(nsamps-1.)));
        } else {
          ins[base][j]=src[j*chans];
        }
      }
      src++;
    }




    //fprintf(stderr,"executing plan of size %d\n",sdata->size);
    fftwf_execute(plans[base]);

    okmin=kmin=0;

    for (s=0; s<NSLICES; s++) {
      // which element do we want for output ?
      // out array goes from 0 to (nsamps/2 + 1) [div b y 2 rounded down]

      // nyquist freq is rate / 2
      // so the freq. of the kth element is: f  =  k/nsamps * rate
      // therefore k = f/rate * nsamps
      tot=0.;

      kmax = freq[s]/(double)rate*(double)nsamps;

      if (kmax>=(nsamps>>1)) {
        // frequency invalid - too high for this sample packet
        tot=-1.;
        sdata->buf[s][sdata->bufidx]=tot;
      } else {

        // use an overlap
        rkmin=kmin-((kmin-okmin)>>1);
        if (s<NSLICES-1) {
          rkmax=kmax+(freq[s+1]-freq[s])/2./(double)rate*(double)nsamps;
          if (rkmax>=(nsamps>>1)) {
            rkmax=kmax;
          }
        } else rkmax=kmax;

        totx=0.;

        for (k=rkmin; k<=rkmax; k++) {
          // sum values over range
          // average over range
          totx+=sqrtf(outs[base][k][0]*outs[base][k][0]+outs[base][k][1]*outs[base][k][1]);
        }

        // average over bandwidth
        totx/=((float)rkmax-(float)rkmin+1.);

        // boost lower freq
        totx/=((float)rkmax-(float)rkmin+1.);

        // store this value in the buffer
        sdata->buf[s][sdata->bufidx]+=totx/(float)chans;

        okmin=kmin;
        kmin=kmax;
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

#define ONSET_WINDOW 5

  var=0.;

  for (i=0; i<NSLICES; i++) {
    // for the variance:
    //av=sdata->av[i]/(double)sdata->bufidx;

    if (sdata->bufidx>ONSET_WINDOW) {
      float val1,val2,varx;
      for (j=sdata->bufidx-ONSET_WINDOW; j<=sdata->bufidx; j++) {
        if (
          (val1=sdata->buf[i][j])!=-1.&&
          (val2=sdata->buf[i][j-1])!=-1.
        ) {
          varx=(val1-val2);
          if (varx<0.) varx=0.;
          var+=(double)varx/(double)ONSET_WINDOW;
        }
      }
    }
  }


  //if (i==0) fprintf(stderr,"%f %f %f  ",var,av,sdata->buf[i][sdata->bufidx]);

  var/=(double)NSLICES;


  for (i=0; i<NSLICES; i++) {
    av=sdata->av[i]/(double)sdata->bufidx;
    if (var>=varlim && sdata->buf[i][sdata->bufidx] >= (avlim*av)) {
      // got a beat !
      beat_pulse=beat_hold=WEED_TRUE;
      //fprintf(stderr,"PULSE !\n");
      break;
    }

  }

  //fprintf(stderr,"\n\n");

done:
  weed_set_boolean_value(out_params[0],"value",beat_pulse);
  weed_set_int64_value(out_params[0],"timecode",timestamp);
  weed_set_boolean_value(out_params[1],"value",beat_hold);
  weed_set_int64_value(out_params[1],"timecode",timestamp);

  weed_free(out_params);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info;
  if (create_plans()!=WEED_NO_ERROR) return NULL;
  plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    weed_plant_t *in_chantmpls[]= {weed_audio_channel_template_init("in channel 0",0),NULL};
    weed_plant_t *in_params[]= {weed_switch_init("reset","_Reset hold",WEED_FALSE),weed_float_init("avlim","_Average threshold",3.,0.,40.),
                                weed_float_init("varlim","_Variance threshold",0.5,0.,10.),weed_switch_init("hamming","Use _Hamming",WEED_TRUE),NULL
                               };
    weed_plant_t *out_params[]= {weed_out_param_switch_init("beat pulse",WEED_FALSE),weed_out_param_switch_init("beat hold",WEED_FALSE),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("beat detector","salsaman",1,0,&beat_init,&beat_process,
                               &beat_deinit,in_chantmpls,NULL,in_params,out_params);

    weed_plant_t *gui=weed_parameter_template_get_gui(in_params[0]);
    weed_set_boolean_value(gui,"hidden",WEED_TRUE);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}


void weed_desetup(void) {
  register int i;
  for (i=0; i<MAXPLANS; i++) {
    fftwf_destroy_plan(plans[i]);
    fftwf_free(ins[i]);
    fftwf_free(outs[i]);
  }
}
