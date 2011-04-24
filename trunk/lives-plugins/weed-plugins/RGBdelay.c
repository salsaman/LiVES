// RGBdelay.c
// weed plugin
// (c) G. Finch (salsaman) 2011
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


#include <stdio.h>

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-palettes.h"
#include "weed/weed-effects.h"
#include "weed/weed-plugin.h"
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed-utils.h" // optional
#include "weed/weed-plugin-utils.h" // optional
#else
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

/////////////////////////////////////////////////////////////




int RGBd_init(weed_plant_t *inst) {
  int error;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error),*gui,*ptmpl;
  int ncache=weed_get_int_value(in_params[0],"value",&error);
  register int i;

  ncache=ncache*4+4;

  for (i=0;i<205;i++) {
    ptmpl=weed_get_plantptr_value(in_params[i],"template",&error);
    gui=weed_parameter_template_get_gui(ptmpl);
    weed_set_boolean_value(gui,"hidden",i>ncache?WEED_TRUE:WEED_FALSE);
  }

  weed_free(in_params);
  return WEED_NO_ERROR;
}



int RGBd_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error)*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  unsigned char *end=src+height*irowstride;
  register int i;

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    int offset=weed_get_int_value(out_channel,"offset",&error);
    int dheight=weed_get_int_value(out_channel,"height",&error);

    src+=offset*irowstride;
    dst+=offset*orowstride;
    end=src+dheight*irowstride;
  }

  for (;src<end;src+=irowstride) {
    for (i=0;i<width;i++) {
      dst[i]=src[i]^0xFF;
    }
    dst+=orowstride;
  } 
  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    weed_plant_t *filter_class,*gui;

    weed_plant_t *in_params[206];
    char *rfx_strings[54];
    char label[256];
    int i,j;

    in_params[0]=weed_integer_init("fcsize","Frame _Cache Size",20,0,50);
    weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

    for (i=1;i<205;i+=4) {
      for (j=0;j<3;j++) {
	if (j==2) snprintf(label,256,"        Frame -%-2d       ",(i-1)/4);
	else weed_memset(label,0,1);
	in_params[i+j]=weed_switch_init("",label,i<4?WEED_TRUE:WEED_FALSE);
      }
      in_params[i+j]=weed_float_init("","",1.,0.,1.);

      if (i>=80) {
	gui=weed_parameter_template_get_gui(in_params[i]);
	weed_set_boolean_value(gui,"hidden",WEED_TRUE);
	gui=weed_parameter_template_get_gui(in_params[i+1]);
	weed_set_boolean_value(gui,"hidden",WEED_TRUE);
	gui=weed_parameter_template_get_gui(in_params[i+2]);
	weed_set_boolean_value(gui,"hidden",WEED_TRUE);
	gui=weed_parameter_template_get_gui(in_params[i+3]);
	weed_set_boolean_value(gui,"hidden",WEED_TRUE);
      }
    }

    in_params[205]=NULL;

    filter_class=weed_filter_class_init("RGBdelay","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,&RGBd_init,&RGBd_process,NULL,in_chantmpls,out_chantmpls,in_params,NULL);

    gui=weed_filter_class_get_gui(filter_class);
    rfx_strings[0]="layout|p0";
    rfx_strings[1]="layout|hseparator|";
    rfx_strings[2]="layout|\"  R\"|\"           G \"|\"           B \"|fill|\"Blend Strength\"|fill|";

    for(i=3;i<54;i++) {
      rfx_strings[i]=weed_malloc(1024);
      snprintf(rfx_strings[i],1024,"layout|p%d|p%d|p%d|p%d|",(i-3)*4+1,(i-3)*4+2,(i-3)*4+3,(i-2)*4);
    }

    weed_set_string_value(gui,"layout_scheme","RFX");
    weed_set_string_value(gui,"rfx_delim","|");
    weed_set_string_array(gui,"rfx_strings",54,rfx_strings);

    for(i=3;i<54;i++) weed_free(rfx_strings[i]);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

