// audio_volume.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2008
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]= {131,110}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////


int avol_init(weed_plant_t *inst) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  int chans=weed_get_int_value(in_channel,"audio_channels",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  weed_plant_t *paramt=weed_get_plantptr_value(in_params[1],"template",&error);
  weed_plant_t *gui=weed_parameter_template_get_gui(paramt);
  weed_plant_t *paramt2=weed_get_plantptr_value(in_params[2],"template",&error);
  weed_plant_t *gui2=weed_parameter_template_get_gui(paramt2);

  weed_free(in_params);

  // hide the "pan" and "swap" controls if we are using mono audio
  if (chans!=2) {
    weed_set_boolean_value(gui,"hidden",WEED_TRUE);
    weed_set_boolean_value(gui2,"hidden",WEED_TRUE);
  } else {
    weed_set_boolean_value(gui,"hidden",WEED_FALSE);
    weed_set_boolean_value(gui2,"hidden",WEED_FALSE);
  }

  return WEED_NO_ERROR;
}



int avol_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t **in_channels=weed_get_plantptr_array(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                             &error);
  float *src;
  float *odst=weed_get_voidptr_value(out_channel,"audio_data",&error),*dst=odst;

  register int nsamps;

  int orig_nsamps;
  int chans;
  int inter;

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  double voll,volr;

  // this is marked as "is_volume_master" in setup
  // therefore it must linearly adjust volume between 0.0 and 1.0 for all sub-channels
  double *vol=weed_get_double_array(in_params[0],"value",&error);
  double *pan=weed_get_double_array(in_params[1],"value",&error);
  int swapchans=weed_get_boolean_value(in_params[2],"value",&error);

  int ntracks=weed_leaf_num_elements(inst,"in_channels");

  register int i;

  weed_free(in_params);

  chans=weed_get_int_value(in_channels[0],"audio_channels",&error);

  voll=volr=vol[0];

  if (chans==2) {
    if (pan[0]<0.) volr*=(1.+pan[0]);
    else voll*=(1.-pan[0]);
  }

  nsamps=orig_nsamps=weed_get_int_value(in_channels[0],"audio_data_length",&error);
  src=weed_get_voidptr_value(in_channels[0],"audio_data",&error);
  inter=weed_get_boolean_value(in_channels[0],"audio_interleaf",&error);

  if (chans==2) {
    if (swapchans==WEED_FALSE) {
      while (nsamps--) {
        *(dst++)=voll*(*(src++));
        if (inter) *(dst++)=volr*(*(src++));
      }
      if (!inter) {
        nsamps=orig_nsamps;
        while (nsamps--) {
          *(dst++)=volr*(*(src++));
        }
      }
    } else {
      // swap l/r channels
      if (!inter) src+=nsamps;
      else src++;
      while (nsamps--) {
        if (inter) {
          *(dst++)=voll*(*(src--));
          *(dst++)=volr*(*(src++));
          src++;
        } else *(dst++)=voll*(*(src++));
      }
      if (!inter) {
        nsamps=orig_nsamps;
        src-=nsamps*2;
        while (nsamps--) {
          *(dst++)=volr*(*(src++));
        }
      }
    }
  } else if (chans==1) {
    while (nsamps--) {
      *(dst++)=vol[0]*(*(src++));
    }
  }

  for (i=1; i<ntracks; i++) {
    if (weed_plant_has_leaf(in_channels[i],"disabled")&&weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_TRUE) continue;
    if (vol[i]==0.) continue;
    dst=odst;
    nsamps=orig_nsamps=weed_get_int_value(in_channels[i],"audio_data_length",&error);
    src=weed_get_voidptr_value(in_channels[i],"audio_data",&error);

    inter=weed_get_boolean_value(in_channels[i],"audio_interleaf",&error);

    chans=weed_get_int_value(in_channels[i],"audio_channels",&error);


    voll=volr=vol[i];

    if (chans==2) {
      if (pan[i]<0.) volr*=(1.+pan[i]);
      else voll*=(1.-pan[i]);
    }

    if (chans==2) {
      while (nsamps--) {
        *(dst++)+=voll*(*(src++));
        if (inter) *(dst++)+=volr*(*(src++));
      }
      if (!inter) {
        nsamps=orig_nsamps;
        while (nsamps--) {
          *(dst++)+=volr*(*(src++));
        }
      }
    } else if (chans==1) {
      while (nsamps--) {
        *(dst++)+=vol[i]*(*(src++));
      }
    }
  }

  weed_free(vol);
  weed_free(pan);
  weed_free(in_channels);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    weed_plant_t *in_chantmpls[]= {weed_audio_channel_template_init("in channel 0",0),NULL};
    weed_plant_t *out_chantmpls[]= {weed_audio_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE),NULL};
    weed_plant_t *in_params[]= {weed_float_init("volume","_Volume",1.0,0.0,1.0),weed_float_init("pan","_Pan",0.,-1.,1.),weed_switch_init("swap","_Swap left and right channels",WEED_FALSE),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("audio volume and pan","salsaman",1,WEED_FILTER_IS_CONVERTER,&avol_init,&avol_process,
                               NULL,in_chantmpls,out_chantmpls,in_params,NULL);
    int error;
    weed_plant_t *host_info=weed_get_plantptr_value(plugin_info,"host_info",&error);
    int api=weed_get_int_value(host_info,"api_version",&error);

    weed_set_int_value(in_chantmpls[0],"max_repeats",0); // set optional repeats of this channel

    // use WEED_PARAMETER_ELEMENT_PER_CHANNEL from spec 110
    weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS|WEED_PARAMETER_ELEMENT_PER_CHANNEL);
    weed_set_double_value(in_params[0],"new_default",1.0);
    weed_set_int_value(in_params[1],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS|WEED_PARAMETER_ELEMENT_PER_CHANNEL);
    weed_set_double_value(in_params[1],"new_default",0.0);
    weed_set_int_value(in_params[2],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS|WEED_PARAMETER_ELEMENT_PER_CHANNEL);
    weed_set_double_value(in_params[2],"new_default",WEED_FALSE);

    // set is_volume_master from weedaudio spec 110
    weed_set_boolean_value(in_params[0],"is_volume_master",WEED_TRUE);

    if (api>=131) weed_set_int_value(filter_class,"flags",WEED_FILTER_PROCESS_LAST|WEED_FILTER_IS_CONVERTER);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

