// edge.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2015
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

#include <string.h>
#include <math.h>

/////////////////////////////////////////////

typedef unsigned int RGB32;
#define PIXEL_SIZE (sizeof(RGB32))

typedef struct {
  RGB32 *map;
} static_data;

//static int video_width_margin;


int edge_init(weed_plant_t *inst) {
  weed_plant_t *in_channel;
  int error,height,width;

  static_data *sdata=(static_data *)weed_malloc(sizeof(static_data));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  height=weed_get_int_value(in_channel,"height",&error);
  width=weed_get_int_value(in_channel,"width",&error);

  sdata->map=weed_malloc(width * height * PIXEL_SIZE * 2);
  if (sdata->map == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  weed_memset(sdata->map, 0, width * height * PIXEL_SIZE * 2);

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


int edge_deinit(weed_plant_t *inst) {
  static_data *sdata;
  int error;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata != NULL) {
    weed_free(sdata->map);
    weed_free(sdata);
  }

  return WEED_NO_ERROR;
}


static inline RGB32 copywalpha(RGB32 *dest, RGB32 *src, size_t ioffs, size_t ooffs, RGB32 val) {
  // copy alpha from src, and RGB from val; return val
  dest[ooffs]=(src[ioffs]&0xff000000)|(val&0xffffff);
  return val;
}


int edge_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  RGB32 *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  RGB32 *dest=weed_get_voidptr_value(out_channel,"pixel_data",&error),*odest=dest;
  int video_width=weed_get_int_value(in_channel,"width",&error);
  int video_height=weed_get_int_value(in_channel,"height",&error);
  int r,g,b;
  static_data *sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  RGB32 *map=sdata->map;

  int map_width=video_width/4;
  int map_height=video_height;

  register int x, y;
  RGB32 p, q;
  RGB32 v0, v1, v2, v3;

  //int video_width_margin = video_width - video_width/4 * 4;
  //int row=video_width*3+8+video_width_margin;

  int irow=weed_get_int_value(in_channel,"rowstrides",&error)/4;
  int irowx = irow - video_width + 2;
  int orow=weed_get_int_value(out_channel,"rowstrides",&error)/4;
  int orowx = orow - video_width + 2;

  src += video_width+1;
  dest += video_width+1;

  for (y=1; y<map_height-1; y++) {
    for (x=1; x<map_width-1; x++) {
      p = *src;
      q = *(src - 4);

      /* difference between the current pixel and right neighbor. */
      r = ((int)(p & 0xff0000) - (int)(q & 0xff0000))>>16;
      g = ((int)(p & 0x00ff00) - (int)(q & 0x00ff00))>>8;
      b = ((int)(p & 0x0000ff) - (int)(q & 0x0000ff));
      r *= r; /* Multiply itself and divide it with 16, instead of */
      g *= g; /* using abs(). */
      b *= b;
      r >>=5; /* To lack the lower bit for saturated addition,  */
      g >>=5; /* devide the value with 32, instead of 16. It is */
      b >>=4; /* same as `v2 &= 0xfefeff' */
      if (r>127) r = 127;
      if (g>127) g = 127;
      if (b>255) b = 255;
      v2 = (r<<17)|(g<<9)|b;

      /* difference between the current pixel and upper neighbor. */
      q = *(src - irow);
      r = ((int)(p & 0xff0000) - (int)(q & 0xff0000))>>16;
      g = ((int)(p & 0x00ff00) - (int)(q & 0x00ff00))>>8;
      b = ((int)(p & 0x0000ff) - (int)(q & 0x0000ff));
      r *= r;
      g *= g;
      b *= b;
      r>>=5;
      g>>=5;
      b>>=4;
      if (r>127) r = 127;
      if (g>127) g = 127;
      if (b>255) b = 255;
      v3 = (r<<17)|(g<<9)|b;

      map[y*map_width*2+x*2+1] = copywalpha(dest,src,2,2,copywalpha(dest,src,3,3,copywalpha(dest,src,irow+2,orow+2,
                                            copywalpha(dest,src,irow+3,orow+3,
                                                v3))));
      map[y*map_width*2+x*2] = copywalpha(dest,src,irow*2,orow*2,copywalpha(dest,src,irow*2+1,orow*2+1,
                                          copywalpha(dest,src,irow*3,orow*3,
                                              copywalpha(dest,src,irow*3+1,orow*3+1,v2))));

      v0 = map[(y-1)*map_width*2+x*2];
      v1 = map[y*map_width*2+(x-1)*2+1];

      g = (r=v0+v1) & 0x01010100;
      copywalpha(dest,src,0,0,r | (g - (g>>8)));
      g = (r=v0+v3) & 0x01010100;
      copywalpha(dest,src,1,1,r | (g - (g>>8)));
      g = (r=v2+v1) & 0x01010100;
      copywalpha(dest,src,irow,orow,r | (g - (g>>8)));
      g = (r=v2+v3) & 0x01010100;
      copywalpha(dest,src,irow+1,orow+1,r | (g - (g>>8)));

      src += 4;
      dest += 4;
    }
    src += irowx;
    dest += orowx;
  }

  weed_memset(dest,0,orow*4-4);
  weed_memset(odest,0,orow*4+4);

  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGRA32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("edge detect","effectTV",1,0,&edge_init,&edge_process,&edge_deinit,in_chantmpls,
                               out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

