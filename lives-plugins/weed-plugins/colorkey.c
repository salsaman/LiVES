// colorkey.c
// weed plugin
// (c) G. Finch (salsaman) 2006
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
static int api_versions[]= {131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

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


static int ckey_process(weed_plant_t *inst, weed_timecode_t timecode) {
  int error;
  weed_plant_t **in_channels=weed_get_plantptr_array(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                             &error);
  unsigned char *src1=weed_get_voidptr_value(in_channels[0],"pixel_data",&error);
  unsigned char *src2=weed_get_voidptr_value(in_channels[1],"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channels[0],"width",&error)*3;
  int height=weed_get_int_value(in_channels[0],"height",&error);
  int irowstride1=weed_get_int_value(in_channels[0],"rowstrides",&error);
  int irowstride2=weed_get_int_value(in_channels[1],"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int palette=weed_get_int_value(out_channel,"current_palette",&error);
  unsigned char *end=src1+height*irowstride1;
  int inplace=(src1==dst);
  weed_plant_t **in_params;
  int b_red,b_green,b_blue;
  int red,green,blue;
  int b_red_min,b_green_min,b_blue_min;
  int b_red_max,b_green_max,b_blue_max;
  double delta,opac,opacx;
  int *carray;

  register int j;

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  delta=weed_get_double_value(in_params[0],"value",&error);
  opac=weed_get_double_value(in_params[1],"value",&error);
  carray=weed_get_int_array(in_params[2],"value",&error);
  b_red=carray[0];
  b_green=carray[1];
  b_blue=carray[2];
  weed_free(carray);


  b_red_min=b_red-(int)(b_red*delta+.5);
  b_green_min=b_green-(int)(b_green*delta+.5);
  b_blue_min=b_blue-(int)(b_blue*delta+.5);

  b_red_max=b_red+(int)((255-b_red)*delta+.5);
  b_green_max=b_green+(int)((255-b_green)*delta+.5);
  b_blue_max=b_blue+(int)((255-b_blue)*delta+.5);

  for (; src1<end; src1+=irowstride1) {
    for (j=0; j<width; j+=3) {
      if (palette==WEED_PALETTE_RGB24) {
        red=src1[j];
        green=src1[j+1];
        blue=src1[j+2];
      } else {
        blue=src1[j];
        green=src1[j+1];
        red=src1[j+2];
      }
      if (red>=b_red_min&&red<=b_red_max&&green>=b_green_min&&green<=b_green_max&&blue>=b_blue_min&&blue<=b_blue_max) {
        dst[j]=src1[j]*((opacx=1.-opac))+src2[j]*opac;
        dst[j+1]=src1[j+1]*(opacx)+src2[j+1]*opac;
        dst[j+2]=src1[j+2]*(opacx)+src2[j+2]*opac;
      } else if (!inplace) weed_memcpy(&dst[j],&src1[j],3);
    }
    src2+=irowstride2;
    dst+=orowstride;
  }
  weed_free(in_channels);
  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),weed_channel_template_init("in channel 1",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};

    weed_plant_t *in_params[]= {weed_float_init("delta","_Delta",.2,0.,1.),weed_float_init("opacity","_Opacity",1.,0.,1.),weed_colRGBi_init("col","_Colour",0,0,255),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("colour key","salsaman",1,0,NULL,&ckey_process,NULL,in_chantmpls,out_chantmpls,in_params,
                               NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }
  return plugin_info;
}
