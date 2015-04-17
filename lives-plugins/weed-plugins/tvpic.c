// tvpic.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// simulate a TV display

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
static void set_black(unsigned char *dst, unsigned char *src, size_t alpha, int psize) {
  weed_memset(dst,0,psize);
  if (alpha!=-1) dst[alpha]=src[alpha];
}

static void set_avg(unsigned char *dst, unsigned char *src1, unsigned char *src2, size_t col, size_t alpha, int psize) {
  unsigned char avg=(src1[col]+src2[col])/2;
  weed_memset(dst,0,psize);
  dst[col]=avg;
  if (alpha!=-1) dst[alpha]=src1[alpha];
}


static int tvpic_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);

  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dest=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  int width=weed_get_int_value(in_channel,"width",&error);
  int pal=weed_get_int_value(in_channel,"current_palette",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int psize=(pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24)?3:4;
  int offset=0,dheight=height;
  int odd=0;
  int rem=width%6; // modulo 6
  int lbord,rbord;

  size_t red,green,blue,alpha;

  register int x,y;

  if (height<2) return WEED_NO_ERROR;

  width*=psize;

  lbord=(rem>>1)*psize;
  rbord=width-lbord;

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    offset=weed_get_int_value(out_channel,"offset",&error);
    dheight=weed_get_int_value(out_channel,"height",&error);
    dheight+=offset;

    src+=offset*irowstride;
    dest+=offset*orowstride;
    odd=offset%2;
  }

  switch (pal) {
  case WEED_PALETTE_RGB24:
    red=0;
    green=1;
    blue=2;
    alpha=-1;
    break;
  case WEED_PALETTE_RGBA32:
    red=0;
    green=1;
    blue=2;
    alpha=3;
    break;
  case WEED_PALETTE_BGR24:
    blue=0;
    green=1;
    red=2;
    alpha=-1;
    break;
  case WEED_PALETTE_BGRA32:
    blue=0;
    green=1;
    red=2;
    alpha=4;
    break;
  default:
    // ARGB32
    alpha=0;
    red=1;
    green=2;
    blue=3;
  }


  for (y=offset; y<dheight; y++) {
    x=0;
    while (x<width) {
      if (x<lbord||x>rbord) {
        set_black(&dest[x],&src[x],alpha,psize);
        x+=psize;
      } else if (y==height-1) {
        // bottom row, 2 possibilities
        // if odd, rgb from row, rgb from row/row-1
        if (odd) {
          weed_memcpy(&dest[x],&src[x],psize);
          x+=psize;
          weed_memcpy(&dest[x],&src[x],psize);
          x+=psize;
          weed_memcpy(&dest[x],&src[x],psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],red,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],green,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],blue,alpha,psize);
          x+=psize;
        } else {
          // if even, rgb from row/row-1, 3 black
          set_avg(&dest[x],&src[x],&src[x-irowstride],red,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],green,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],blue,alpha,psize);
          x+=psize;
          set_black(&dest[x],&src[x],alpha,psize);
          x+=psize;
          set_black(&dest[x],&src[x],alpha,psize);
          x+=psize;
          set_black(&dest[x],&src[x],alpha,psize);
          x+=psize;
        }
      } else if (y==0) {
        // top row has 3 black, rgb from row/row+1, 3 black, etc
        set_black(&dest[x],&src[x],alpha,psize);
        x+=psize;
        set_black(&dest[x],&src[x],alpha,psize);
        x+=psize;
        set_black(&dest[x],&src[x],alpha,psize);
        x+=psize;
        set_avg(&dest[x],&src[x],&src[x+irowstride],red,alpha,psize);
        x+=psize;
        set_avg(&dest[x],&src[x],&src[x+irowstride],green,alpha,psize);
        x+=psize;
        set_avg(&dest[x],&src[x],&src[x+irowstride],blue,alpha,psize);
        x+=psize;
      } else {
        // normal row, 2 possibilities
        if (odd) {
          // if odd, rgb from row/row+1, rgb from row/row-1
          set_avg(&dest[x],&src[x],&src[x+irowstride],red,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x+irowstride],green,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x+irowstride],blue,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],red,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],green,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],blue,alpha,psize);
          x+=psize;
        } else {
          // if even, rgb from row/row-1, rgb from row/row+1
          set_avg(&dest[x],&src[x],&src[x-irowstride],red,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],green,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x-irowstride],blue,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x+irowstride],red,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x+irowstride],green,alpha,psize);
          x+=psize;
          set_avg(&dest[x],&src[x],&src[x+irowstride],blue,alpha,psize);
          x+=psize;
        }
      }
    }
    dest+=orowstride;
    src+=irowstride;
    odd=!odd;
  }
  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("tvpic","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&tvpic_process,NULL,in_chantmpls,
                               out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

