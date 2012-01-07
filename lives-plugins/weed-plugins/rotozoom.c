// rotozoom.c
// original code (c) Kentaro Fukuchi (effecTV)
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-plugin.h>
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
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

/////////////////////////////////////////////////////////////

#include <math.h>


typedef unsigned int RGB32;
static int roto[256];
static int roto2[256];

/////////////////////////////////////////////////////////////

static void draw_tile(int stepx, int stepy, int zoom,RGB32 *texture,RGB32 *image, int video_width, int video_height)
{
  int x, y, xd, yd, a, b, sx, sy;
  register int i,j;
  int test;
    
  int video_area=video_width*video_height;
  sx = sy = 0;
  xd = (stepx * zoom) >> 12;
  yd = (stepy * zoom) >> 12;
    
  /* Stepping across and down the screen, each screen row has a
   * starting coordinate in the texture: (sx, sy).  As each screen
   * row is traversed, the current texture coordinate (x, y) is
   * modified by (xd, yd), which are (sin(rot), cos(rot)) multiplied
   * by the current zoom factor.  For each vertical step, (xd, yd)
   * is rotated 90 degrees, to become (-yd, xd).
   *
   * More fun can be had by playing around with x, y, xd, and yd as
   * you move about the image.
   */

  for (j = 0; j < video_height; j++) {
    x = sx; y = sy;   
    for (i = 0; i < video_width; i++) {
      a=((x>>12&255)*video_width)>>8;
      b=((y>>12&255)*video_height)>>8;
      //      a*=video_width/64;
      //      b*=video_height/64;
      *image++=((test=b*video_width+a)<video_area)?texture[test]:0;
      x += xd; y += yd;
    }
    sx -= yd; sy += xd;
  }
}

//////////////////////////////////////////////////////////


int rotozoom_init (weed_plant_t *inst) {
  weed_set_int_value(inst,"plugin_path",0);
  weed_set_int_value(inst,"plugin_zpath",0);

  return WEED_NO_ERROR;
}


int rotozoom_deinit (weed_plant_t *inst) {
  return WEED_NO_ERROR;
}


int rotozoom_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  RGB32 *src,*dst;
  int width,height;
  int zoom;
  weed_plant_t *in_channel,*out_channel;
  int error;
  int path=weed_get_int_value(inst,"plugin_path",&error);
  int zpath=weed_get_int_value(inst,"plugin_zpath",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  int autozoom;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  width = weed_get_int_value(in_channel,"width",&error);
  height = weed_get_int_value(in_channel,"height",&error);

  autozoom=weed_get_boolean_value(in_params[1],"value",&error);

  if (autozoom==WEED_TRUE) {
    weed_set_int_value(inst,"plugin_zpath",(zpath + 1) & 255);
  }
  else {
    zpath=weed_get_int_value(in_params[0],"value",&error);
    weed_set_int_value(inst,"plugin_zpath",zpath);
  }
  zoom=roto2[zpath];

  draw_tile(roto[path], roto[(path + 128) & 0xFF],zoom,src,dst,width,height);
  weed_set_int_value(inst,"plugin_path",(path - 1) & 255);

  weed_free(in_params);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int i;
    int palette_list[]={WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_YUVA8888,WEED_PALETTE_END};
    
    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *in_params[]={weed_integer_init("zoom","_Zoom value",128,0,255),weed_switch_init("autozoom","_Auto zoom",WEED_TRUE),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("rotozoom","effectTV",1,0,&rotozoom_init,&rotozoom_process,&rotozoom_deinit,in_chantmpls,out_chantmpls,in_params,NULL);
    
    weed_plugin_info_add_filter_class (plugin_info,filter_class);
    
    weed_set_int_value(plugin_info,"version",package_version);
    
    // static data for all instances
    for (i = 0; i < 256; i++) {
      float rad =  (float)i * 1.41176 * 0.0174532;
      float c = sin(rad);
      roto[i] = (c + 0.8) * 4096.0;
      roto2[i] = (2.0 * c) * 4096.0;
    }
  }
  return plugin_info;
}



