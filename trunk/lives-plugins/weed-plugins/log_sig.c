// nn_programmer.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// converts float values to log_sig values

//#define DEBUG
#include <stdio.h>
#include <math.h>

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

/////////////////////////////////////////////////////////////


int logsig_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);
  double fval;

  register int i;

  for (i=0; i<256; i++) {
    if (weed_plant_has_leaf(in_params[i],"value")) {
      fval=weed_get_double_value(in_params[i],"value",&error);
      weed_set_double_value(out_params[i],"value",1./(1.+exp(-fval)));
    }
  }

  weed_free(in_params);
  weed_free(out_params);

  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    weed_plant_t *filter_class;

    weed_plant_t *in_params[257];
    weed_plant_t *out_params[257];

    register int i;

    char name[256];
    char label[256];

    for (i=0; i<256; i++) {
      snprintf(name,256,"input%03d",i);
      snprintf(label,256,"Input %03d",i);
      in_params[i]=weed_float_init(name,label,0.,-1000000000000.,1000000000000.);
      snprintf(name,256,"Output %03d",i);
      out_params[i]=weed_out_param_float_init(name,0.,-1.,1.);
    }

    in_params[i]=NULL;
    out_params[i]=NULL;

    filter_class=weed_filter_class_init("log_sig","salsaman",1,0,NULL,&logsig_process,
                                        NULL,NULL,NULL,in_params,out_params);

    weed_set_string_value(filter_class,"description","Scales float values between -1.0 and 1.0 using a log-sig function");

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

