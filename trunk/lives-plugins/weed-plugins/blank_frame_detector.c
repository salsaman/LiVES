// blank_frame_detector.c
// weed plugin
// (c) G. Finch (salsaman) 2012
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

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order (most preferred first)

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
  int count;
} _sdata;

static unsigned short Y_R[256];
static unsigned short Y_G[256];
static unsigned short Y_B[256];

static unsigned short unclamp_luma[256];


static void init_luma_arrays(void) {
  register int i;

  for (i=0; i<256; i++) {
    Y_R[i]=.299*(float)i*256.;
    Y_G[i]=.587*(float)i*256.;
    Y_B[i]=.114*(float)i*256.;
  }

  for (i=0; i<17; i++) {
    unclamp_luma[i]=0;
  }

  for (i=17; i<235; i++) {
    unclamp_luma[i]=(int)((float)(i-16.)/219.*255.+.5);
  }

  for (i=235; i<256; i++) {
    unclamp_luma[i]=255;
  }

}



int calc_luma(int red, int green, int blue) {
  return (Y_R[red]+Y_G[green]+Y_B[blue])>>8;
}


////////////////////////////////////////////////////////////


int bfd_init(weed_plant_t *inst) {
  int error;
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);
  weed_plant_t *blank=out_params[0];
  _sdata *sdata;

  weed_set_boolean_value(blank,"value",WEED_FALSE);

  sdata=(_sdata *)weed_malloc(sizeof(_sdata));

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->count=0;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  weed_free(out_params);

  return WEED_NO_ERROR;
}



int bfd_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (sdata!=NULL) weed_free(sdata);

  return WEED_NO_ERROR;
}



int bfd_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  unsigned char *src=(unsigned char *)weed_get_voidptr_value(in_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int pal=weed_get_int_value(in_channel,"current_palette",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);
  weed_plant_t *blank=out_params[0];
  int threshold=weed_get_int_value(in_params[0],"value",&error);
  int fcount=weed_get_int_value(in_params[1],"value",&error);
  int psize=4;
  int luma;
  int clamped=0;
  int start=0;
  unsigned char *end=src+height*irowstride;
  register int i;

  if (pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_YUV422P||
      pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_UYVY||pal==WEED_PALETTE_YUYV||
      pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888) {

    if (weed_plant_has_leaf(in_channel,"YUV_clamping")&&
        (weed_get_int_value(in_channel,"YUV_clamping",&error)==WEED_YUV_CLAMPING_CLAMPED))
      clamped=1;
  }

  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_YUV888) psize=3;
  if (pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_YUV422P||
      pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P) psize=1;

  width*=psize;

  // yes this is right to set this here (1 macropixel == 4 bytes == 2 pixels)
  if (pal==WEED_PALETTE_UYVY||pal==WEED_PALETTE_YUYV) psize=2;

  if (pal==WEED_PALETTE_UYVY) start=1;

  for (; src<end; src+=irowstride) {
    for (i=start; i<width; i+=psize) {
      if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_RGBA32) luma=calc_luma(src[i],src[i+1],src[i+2]);
      else if (pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_BGRA32) luma=calc_luma(src[i+2],src[i+1],src[i]);
      else if (pal==WEED_PALETTE_ARGB32) luma=calc_luma(src[i+1],src[i+2],src[i+3]);
      else luma=(clamped?unclamp_luma[src[i]]:src[i]);

      if (luma>threshold) {
        // is not blank
        sdata->count=-1;
        break;
      }

    }
  }

  // is blank, but we need to check count

  sdata->count++;

  if (sdata->count>=fcount) weed_set_boolean_value(blank,"value",WEED_TRUE);
  else weed_set_boolean_value(blank,"value",WEED_FALSE);

  weed_free(in_params);
  weed_free(out_params);

  return WEED_NO_ERROR;
}





weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,
                         WEED_PALETTE_ARGB32,WEED_PALETTE_YUVA8888,WEED_PALETTE_YUV888,WEED_PALETTE_YUV444P,
                         WEED_PALETTE_YUVA4444P,WEED_PALETTE_YUV422P,WEED_PALETTE_YUV420P,WEED_PALETTE_YVU420P,
                         WEED_PALETTE_UYVY,WEED_PALETTE_YUYV,
                         WEED_PALETTE_END
                        };
    weed_plant_t *out_params[]= {weed_out_param_switch_init("blank",WEED_FALSE),NULL};
    weed_plant_t *in_params[]= {weed_integer_init("threshold","Luma _threshold",0,0,255),
                                weed_integer_init("fcount","Frame _count",1,1,1000),NULL
                               };
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("blank_frame_detector","salsaman",1,0,&bfd_init,
                               &bfd_process,&bfd_deinit,in_chantmpls,NULL,in_params,out_params);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

    init_luma_arrays();
  }
  return plugin_info;
}

