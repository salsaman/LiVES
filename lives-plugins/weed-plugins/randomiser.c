// dat_unpacker.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// generate a random double when input changes state

//#define DEBUG
#include <stdio.h>

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
static int api_versions[]={131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_UTILS
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#define NVALS 8

typedef struct {
  int vals[NVALS];
} _sdata;




int randomiser_init(weed_plant_t *inst) {
  _sdata *sdata=(_sdata *)weed_malloc(sizeof(_sdata));

  register int i;

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  for (i=0;i<NVALS;i++) {
    sdata->vals[i]=WEED_FALSE;
  }

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;

}


int nnprog_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (sdata!=NULL) {
    weed_free(sdata);
  }
  return WEED_NO_ERROR;
}



int randomiser_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);

  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  double nrand=1.;

  int iv;

  register int i;

  for (i=0;i<NVALS;i++) {
    iv=weed_get_boolean_value(in_params[i],"value",&error);
    if (iv!=sdata->vals[i]) {
      weed_set_double_value(out_params[i],"value",nrand);
    }
  }

  weed_free(in_params);
  weed_free(out_params);

  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    weed_plant_t *filter_class;

    weed_plant_t *in_params[NVALS+1];
    weed_plant_t *out_params[NVALS+1];

    register int i;

    char name[256];
    char label[256];

    for (i=0;i<NVALS;i++) {
      snprintf(name,256,"input%03d",i);
      snprintf(label,256,"Trigger %03d",i);
      in_params[i]=weed_switch_init(name,label,WEED_FALSE);
      snprintf(name,256,"Output %03d",i);
      out_params[i]=weed_out_param_float_init_nominmax(name,0.);
    }

    in_params[i]=NULL;
    out_params[i]=NULL;

    filter_class=weed_filter_class_init("randomiser","salsaman",1,0,NULL,&randomiser_process,
					NULL,NULL,NULL,in_params,out_params);


    weed_set_string_value(filter_class,"description","Generate a random double when input changes state");

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

