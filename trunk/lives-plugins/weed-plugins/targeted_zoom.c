// targetted_zoom.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2009
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

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


int tzoom_process (weed_plant_t *inst, weed_timecode_t timecode) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);

  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  //int palette=weed_get_int_value(out_channel,"current_palette",&error);

  weed_plant_t **in_params;

  double offsx,offsy,scale;

  int dx,dy,dr;

  int sy;

  int offset=0,dheight=height;

  register int x,y;

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  scale=weed_get_double_value(in_params[0],"value",&error);
  offsx=weed_get_double_value(in_params[1],"value",&error);
  offsy=weed_get_double_value(in_params[2],"value",&error);
  weed_free(in_params);

  if (scale<1.) scale=1.;
  if (offsx<0.) offsx=0.;
  if (offsx>1.) offsx=1.;
  if (offsy<0.) offsy=0.;
  if (offsy>1.) offsy=1.;

  offsx*=width;
  offsy*=height;

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    offset=weed_get_int_value(out_channel,"offset",&error);
    dheight=weed_get_int_value(out_channel,"height",&error);
  }

  for (y=offset;y<dheight;y++) {
    dy=(int)((double)y-offsy)/scale+offsy;
    sy=dy*irowstride;
    dr=y*orowstride;

    for (x=0;x<width;x++) {
      dx=(int)((double)x-offsx)/scale+offsx;
      weed_memcpy(dst+dr+x*3,src+sy+dx*3,3);
    }
  }

  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_RGB24,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",0,palette_list),NULL};

    weed_plant_t *in_params[]={weed_float_init("scale","_Scale",1.,1.,16.),weed_float_init("xoffs","_X offset",0.5,0.,1.),weed_float_init("yoffs","_Y offset",0.5,0.,1.),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("targeted zoom","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&tzoom_process,NULL,in_chantmpls,out_chantmpls,in_params,NULL);

    weed_plant_t *gui=weed_filter_class_get_gui(filter_class);

    // define RFX layout
    char *rfx_strings[]={"layout|p0|","layout|p1|p2|","special|framedraw|singlepoint|1|2|"};

    weed_set_string_value(gui,"layout_scheme","RFX");
    weed_set_string_value(gui,"rfx_delim","|");
    weed_set_string_array(gui,"rfx_strings",3,rfx_strings);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }

  return plugin_info;
}
