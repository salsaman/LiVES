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

#include <stdlib.h>
#include <sys/time.h>

/////////////////////////////////////////////////////////////

#define NVALS 8

typedef struct {
  int vals[NVALS];
} _sdata;


static double drand(double max) {
  double denom=(double)(2ul<<30)/max;
  double num=(double)lrand48();
  return (double)(num/denom);
}


static void seed_rand(void) {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  srand48(tv.tv_sec);
}


static double getrand(double min, double max) {
  return min+(min==max?0.:drand(max-min));
}


int randomiser_init(weed_plant_t *inst) {
  int error;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);

  _sdata *sdata=(_sdata *)weed_malloc(sizeof(_sdata));

  double nrand,min,max;

  register int i;

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  for (i=0; i<NVALS; i++) {
    sdata->vals[i]=weed_get_boolean_value(in_params[i],"value",&error);

    min=weed_get_double_value(in_params[NVALS+i*4],"value",&error);
    max=weed_get_double_value(in_params[NVALS+i*4+1],"value",&error);
    nrand=min+(max-min)/2.;
    weed_set_double_value(out_params[i],"value",nrand);
  }

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;

}


int randomiser_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (sdata!=NULL) {
    weed_free(sdata);
  }
  return WEED_NO_ERROR;
}



int randomiser_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);

  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  double nrand,min,max;

  int iv,trigt,trigf;

  register int i;

  for (i=0; i<NVALS; i++) {
    iv=weed_get_boolean_value(in_params[i],"value",&error);

    if (iv!=sdata->vals[i]) {
      trigt=weed_get_boolean_value(in_params[NVALS+i*4+2],"value",&error);
      trigf=weed_get_boolean_value(in_params[NVALS+i*4+3],"value",&error);
      if ((iv==WEED_TRUE&&trigt==WEED_TRUE) || (iv==WEED_FALSE&&trigf==WEED_FALSE)) {
        min=weed_get_double_value(in_params[NVALS+i*4],"value",&error);
        max=weed_get_double_value(in_params[NVALS+i*4+1],"value",&error);
        nrand=getrand(min,max);
        weed_set_double_value(out_params[i],"value",nrand);
      }
      sdata->vals[i]=iv;
    }
  }

  weed_free(in_params);
  weed_free(out_params);

  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    weed_plant_t *filter_class,*gui;

    weed_plant_t *in_params[NVALS*5+1];
    weed_plant_t *out_params[NVALS+1];

    int count=0;

    register int i;

    char name[256];
    char label[256];

    for (i=0; i<NVALS; i++) {
      snprintf(name,256,"input%03d",i);
      snprintf(label,256,"Trigger %03d",i);
      in_params[i]=weed_switch_init(name,label,WEED_FALSE);
      gui=weed_parameter_template_get_gui(in_params[i]);
      weed_set_boolean_value(gui,"hidden",WEED_TRUE);
      snprintf(name,256,"Output %03d",i);
      out_params[i]=weed_out_param_float_init_nominmax(name,0.);
    }

    out_params[i]=NULL;

    for (i=NVALS; i<NVALS*5; i+=4) {
      snprintf(name,256,"min%03d",i);
      snprintf(label,256,"Min value for output %03d",count);
      in_params[i]=weed_float_init(name,label,0.,-1000000.,1000000.);

      snprintf(name,256,"max%03d",i);
      snprintf(label,256,"Max value for output %03d",count);
      in_params[i+1]=weed_float_init(name,label,1.,-1000000.,1000000.);

      snprintf(name,256,"trigt%03d",i);
      snprintf(label,256,"Trigger FALSE->TRUE");
      in_params[i+2]=weed_switch_init(name,label,WEED_TRUE);

      snprintf(name,256,"trigf%03d",i);
      snprintf(label,256,"Trigger TRUE->FALSE");
      in_params[i+3]=weed_switch_init(name,label,WEED_FALSE);

      count++;
    }

    in_params[i]=NULL;

    filter_class=weed_filter_class_init("randomiser","salsaman",1,0,&randomiser_init,&randomiser_process,
                                        &randomiser_deinit,NULL,NULL,in_params,out_params);


    weed_set_string_value(filter_class,"description","Generate a random double when input changes state");

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

