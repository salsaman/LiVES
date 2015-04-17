// alpha_means.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// calculate n X m mean values for alpha channel

// values are output from left to right and top to bottom, eg. for 2 X 2 grid:

// val 1 | val 2
// ------+------
// val 3 | val 4

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


int alpham_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t *out_param=weed_get_plantptr_value(inst,"out_parameters",&error);

  float *alpha=(float *)weed_get_voidptr_value(in_channel,"pixel_data",&error);

  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);

  int irow=weed_get_int_value(in_channel,"rowstrides",&error)-width*sizeof(float);


  int n=weed_get_int_value(in_params[0],"value",&error);
  int m=weed_get_int_value(in_params[1],"value",&error);
  int xdiv=weed_get_boolean_value(in_params[2],"value",&error);
  int ydiv=weed_get_boolean_value(in_params[3],"value",&error);
  int abs=weed_get_boolean_value(in_params[4],"value",&error);
  double scale=weed_get_double_value(in_params[5],"value",&error);

  int idx=0,nidx;

  float nf=(float)width/(float)n; // x pixels per quad
  float mf=(float)height/(float)m; // y pixels per quad
  float nm=(float)(nf*mf); // pixels per quad

  double *vals;

  register int i,j,x;

  weed_free(in_params);

  vals=(double *)weed_malloc(n*m*sizeof(double));

  if (vals==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  for (i=0; i<n*m; i++) vals[i]=0.;

  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      if (idx>n*m) continue;

      vals[idx]+=(double)*alpha;

      // check val of idx for next j
      if (j+1<width) {
        nidx=(int)((float)(j+1.)/nf+.5);
        if (nidx>idx+1) {
          // too many vals, copy...
          for (x=idx+1; x<nidx; x++) {
            vals[x]=vals[idx];
          }

        }
        idx=nidx;
      }
      alpha++;
    }
    alpha+=irow;

    nidx=(int)((float)(m*(i+1))/mf+.5);

    if (nidx>idx+1) {
      for (x=idx+1; x<nidx; x++) {
        if (x<n*m)
          vals[x]=vals[x-m];
      }
    }

    idx=nidx;

  }

  if (nm<1.) nm=1.;

  for (i=0; i<n*m; i++) {
    vals[i]/=(double)nm; // get average val
    if (xdiv) vals[i]/=(double)width;
    if (ydiv) vals[i]/=(double)height;
    if (abs&&vals[i]<0.) vals[i]=-vals[i];
    vals[i]*=scale;
  }

  weed_set_double_array(out_param,"value",n*m,vals);

  weed_free(vals);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {

    int apalette_list[]= {WEED_PALETTE_AFLOAT,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("alpha float",0,apalette_list),NULL};

    weed_plant_t *in_params[]= {weed_integer_init("x divisions","_X divisions",1,1,256),weed_integer_init("y divisions","_Y divisions",1,1,256),weed_switch_init("xdiv","Divide by _width",WEED_FALSE),weed_switch_init("ydiv","Divide by _height",WEED_FALSE),weed_switch_init("abs","Return _absolute values",WEED_FALSE),weed_float_init("scale","_Scale by",1.0,0.1,1000000.),NULL};

    weed_plant_t *out_params[]= {weed_out_param_float_init_nominmax("mean values",0.),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("alpha_means","salsaman",1,0,
                               NULL,&alpham_process,NULL,
                               in_chantmpls,NULL,
                               in_params,out_params);

    weed_set_string_value(filter_class,"description",
                          "Calculate n X m mean values for (float) alpha channel\nvalues are output from left to right and top to bottom, eg. for 2 X 2 grid:\n\nval 1 | val 2\n------+------\nval 3 | val 4");


    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    //number of output values depends on size of grid
    weed_set_int_value(out_params[0],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS);

    weed_set_int_value(plugin_info,"version",package_version);

  }

  return plugin_info;
}

