// xeffect.c
// livido plugin
// (c) G. Finch (salsaman) 2005
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

#define ABS(a)           (((a) < 0) ? -(a) : (a))


static inline unsigned int
calc_luma(unsigned char *pixel) {
  return (unsigned int)(pixel[0]*30+pixel[1]*59+pixel[2]*11);
}

static inline void
make_black(unsigned char *pixel) {
  pixel[0]=pixel[1]=pixel[2]=(unsigned char)0;
}

static inline void
make_white(unsigned char *pixel) {
  pixel[0]=pixel[1]=pixel[2]=(unsigned char)255;
}


static inline void
nine_fill(unsigned char *new_data, int rowstride, unsigned char *old_data) {
  // fill nine pixels with the centre colour
  new_data[-rowstride-3]=new_data[-rowstride]=new_data[-rowstride+3]=new_data[-3]=new_data[0]=new_data[3]=new_data[rowstride-3]=
                           new_data[rowstride]=new_data[rowstride+3]=old_data[0];
  new_data[-rowstride-2]=new_data[-rowstride+1]=new_data[-rowstride+4]=new_data[-2]=new_data[1]=new_data[4]=new_data[rowstride-2]=
                           new_data[rowstride+1]=new_data[rowstride+4]=old_data[1];
  new_data[-rowstride-1]=new_data[-rowstride+2]=new_data[-rowstride+5]=new_data[-1]=new_data[2]=new_data[5]=new_data[rowstride-1]=
                           new_data[rowstride+2]=new_data[rowstride+5]=old_data[2];
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////


int xeffect_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error)*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  unsigned char *end=src+height*irowstride-irowstride;

  unsigned int myluma;
  unsigned int threshold=10000;
  int nbr;

  register int i,j,k;

  src+=irowstride;
  dst+=orowstride;
  width-=4;

  for (; src<end; src+=irowstride) {
    for (i=3; i<width; i+=3) {
      myluma=calc_luma(&src[i]);
      nbr=0;
      for (j=-irowstride; j<=irowstride; j+=irowstride) {
        for (k=-3; k<4; k+=3) {
          if ((j!=0||k!=0)&&ABS(calc_luma(&src[j+i+k])-myluma)>threshold) nbr++;
        }
      }
      if (nbr<2||nbr>5) {
        nine_fill(&dst[i],orowstride,&src[i]);
      } else {
        if (myluma<12500) {
          make_black(&dst[i]);
        } else {
          if (myluma>20000) {
            make_white(&dst[i]);
          }
        }
      }
    }
    dst+=orowstride;
  }
  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("graphic novel","salsaman",1,0,NULL,&xeffect_process,NULL,in_chantmpls,out_chantmpls,NULL,
                               NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}
