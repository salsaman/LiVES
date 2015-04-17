// sc_audio_gen.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-effects.h>
#include <weed/weed-plugin.h>
#else
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-effects.h"
#include "../../../libweed/weed-plugin.h"
#endif


///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h> // optional
#include <weed/weed-effects.h>
#else
#include "../../../libweed/weed.h" // optional
#include "../../../libweed/weed-palettes.h" // optional
#include "../../../libweed/weed-effects.h"
#endif

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../../libweed/weed-plugin.h" // optional
#endif

#include "../weed-utils-code.c" // optional
#include "../weed-plugin-utils.c" // optional

#include <string.h>
#include <stdio.h>

#define SCSYNTH_PORT 51000

/////////////////////////////////////////////////////////////

static int resample(float **inbuf, float *outbuf, int nsamps, int nchans, int inter, int irate, int orate) {
  // resample (time stretch) nsamps samples from inbuf at irate to outbuf at outrate
  // return how many samples in in were consumed

  // we maintain the same number of channels and interleave if necessary

  register size_t offs=0;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int i,j;
  register double scale;

  scale=(double)irate/(double)orate;

  for (i=0; i<nsamps; i++) {
    // process each sample

    if (inter) {
      for (j=0; j<nchans; j++) {
        outbuf[offs]=inbuf[j][src_offset_i];
        offs++;
      }
    } else {
      for (j=0; j<nchans; j++) {
        outbuf[offs+(j*nsamps)]=inbuf[j][src_offset_i];
      }
      offs++;
    }

    // resample on the fly
    src_offset_i=(int)(src_offset_f+=scale);

  }

  return src_offset_i;
}



/////////////////////////////////////////////////////////////

int scgen_deinit(weed_plant_t *inst) {
  // tell scsynth to quit
  //osc_send("/quit");

  // killall -9 jack_rec

  // terminate pthread_reader




  return WEED_NO_ERROR;
}


int scgen_init(weed_plant_t *inst) {
  char com[256];

  int retval,error,chans;

  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  // check our audio player is jack
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);
  weed_plant_t *pinfo=weed_get_plantptr_value(filter,"plugin_info",&error);
  weed_plant_t *hinfo=weed_get_plantptr_value(pinfo,"host_info",&error);

  if (weed_plant_has_leaf(hinfo,"host_audio_player")) {
    char *hap=weed_get_string_value(hinfo,"host_audio_player",&error);
    if (strcmp(hap,"jack")) {
      weed_free(hap);
      return WEED_ERROR_HARDWARE;
    }
    weed_free(hap);
  }

  // jack should be running, so we can init scsynth in the background

  chans=weed_get_int_value(out_channel,"audio_channels",&error);

  snprintf(com,256,"scsynth -u %d -i 0 -o %d &",SCSYNTH_PORT,chans);
  //retval=system(com);

  // get audio rate from jack
  //fd=popen("jack_samplerate 2>/dev/null","r");



  // now we want to connect the jack client to a jack filewriter to a fifo

  //retval=mkfifo(fifofile);

  //snprintf(com,256,"jack_rec -f %s -b 32 -d 1000000000",fifofile);

  //for (i=0;i<nchans;i++) {
  //snprintf(com,256,"% SuperCollider:out_%d",i);
  //}

  //snprintf(com,256,"%s 2>/dev/null &",com);
  //retval=system(com);

  // we will read from fifo and buffer it in 3 ringbuffers (2 write and one read)

  // start pthread



  return WEED_NO_ERROR;
}



int scgen_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  // here we will send OSC to scsynth
  // and read from ring buffer

  int error;
  int chans,nsamps,inter,rate,nrsamps;

  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  float *dst=weed_get_voidptr_value(out_channel,"audio_data",&error);

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  double freq=weed_get_double_value(in_params[0],"value",&error);
  double mult=weed_get_double_value(in_params[1],"value",&error);
  double trate;

  float **buff;

  register int i,j;

  weed_free(in_params);

  chans=weed_get_int_value(out_channel,"audio_channels",&error);
  nsamps=weed_get_int_value(out_channel,"audio_data_length",&error);
  inter=weed_get_boolean_value(out_channel,"audio_interleaf",&error);
  rate=weed_get_int_value(out_channel,"audio_rate",&error);



#define TEST
#ifdef TEST
  // fill with audio at TRATE
  trate=freq*mult;

  if (trate<0.) trate=-trate;

  if (trate==0.) {
    memset(dst,0,nsamps*chans*sizeof(float));
    return WEED_NO_ERROR;
  }

  nrsamps=((double)nsamps/(double)rate*trate+.5);
  buff=weed_malloc(chans*sizeof(float *));
  for (i=0; i<chans; i++) {
    buff[i]=weed_malloc(nrsamps*sizeof(float));
  }

  for (i=0; i<nrsamps; i++) {
    for (j=0; j<chans; j++) {
      buff[j][i]=1.;
    }
    i++;
    if (i<nrsamps) {
      for (j=0; j<chans; j++) {
        buff[j][i]=-1.;
      }
    }
  }

  resample(buff,dst,nsamps,chans,inter,trate,rate);

  for (i=0; i<chans; i++) {
    weed_free(buff[i]);
  }

  weed_free(buff);

#endif

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    weed_plant_t *out_chantmpls[]= {weed_audio_channel_template_init("out channel 0",0),NULL};
    weed_plant_t *in_params[]= {weed_float_init("freq","_Frequency",7500.,0.0,48000.0),weed_float_init("multiplier","Frequency _Multiplier",1.,.01,1000.),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("supercollider audiogen","salsaman",1,0,&scgen_init,&scgen_process,NULL,NULL,
                               out_chantmpls,in_params,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

