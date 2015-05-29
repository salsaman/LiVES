// alien_overlay.c
// Weed plugin
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

typedef struct {
  uint8_t *inited;
  unsigned char *old_pixel_data;
} static_data;


//////////////////////////////////////////////

int alien_over_init(weed_plant_t *inst) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  int height=weed_get_int_value(in_channel,"height",&error);
  int width=weed_get_int_value(in_channel,"width",&error)*3;

  static_data *sdata=(static_data *)weed_malloc(sizeof(static_data));

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->old_pixel_data=(unsigned char *)weed_malloc(height*width);
  if (sdata->old_pixel_data==NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->inited=(uint8_t *)weed_malloc(height);
  if (sdata->inited==NULL) {
    weed_free(sdata);
    weed_free(sdata->old_pixel_data);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->inited,0,height);

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


int alien_over_deinit(weed_plant_t *inst) {
  int error;
  static_data *sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata!=NULL) {
    weed_free(sdata->inited);
    weed_free(sdata->old_pixel_data);
    weed_free(sdata);
    weed_set_voidptr_value(inst,"plugin_internal",NULL);
  }

  return WEED_NO_ERROR;
}


int alien_over_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error)*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int inplace=(src==dst);
  unsigned char val;

  unsigned char *old_pixel_data;
  unsigned char *end=src+height*irowstride;
  static_data *sdata;
  register int j,i=0;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  old_pixel_data=sdata->old_pixel_data;

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    int offset=weed_get_int_value(out_channel,"offset",&error);
    int dheight=weed_get_int_value(out_channel,"height",&error);

    src+=offset*irowstride;
    dst+=offset*orowstride;
    end=src+dheight*irowstride;
    old_pixel_data+=width*offset;
    i=offset;
  }


  for (; src<end; src+=irowstride) {
    for (j=0; j<width; j++) {
      if (sdata->inited[i]) {
        if (!inplace) {
          dst[j]=((char)(old_pixel_data[j])+(char)(src[j]))>>1;
          old_pixel_data[j]=src[j];
        } else {
          val=((char)(old_pixel_data[j])+(char)(src[j]))>>1;
          old_pixel_data[j]=src[j];
          dst[j]=val;
        }
      } else old_pixel_data[j]=dst[j]=src[j];
    }
    sdata->inited[i++]=1;
    dst+=orowstride;
    old_pixel_data+=width;
  }

  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("alien overlay","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,&alien_over_init,
                               &alien_over_process,&alien_over_deinit,in_chantmpls,out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }

  return plugin_info;
}

