// ccorect.c
// weed plugin
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

typedef struct _sdata {
  double ored;
  double ogreen;
  double oblue;

  unsigned char r[256];
  unsigned char g[256];
  unsigned char b[256];
} _sdata;


/////////////////////////////////////////////////////////////
static void make_table(unsigned char *tab, double val) {
  unsigned int ival;
  register int i;
  for (i=0; i<256; i++) {
    ival=(val*i+.5);
    tab[i]=ival>255?(unsigned char)255:(unsigned char)ival;
  }
}



static int ccorrect_init(weed_plant_t *inst) {
  _sdata *sdata;

  register int i;

  sdata=weed_malloc(sizeof(_sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  for (i=0; i<256; i++) {
    sdata->r[i]=sdata->g[i]=sdata->b[i]=0;
  }
  sdata->ored=sdata->ogreen=sdata->oblue=0.;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


static int ccorrect_deinit(weed_plant_t *inst) {
  _sdata *sdata;
  int error;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (sdata != NULL) weed_free(sdata);

  return WEED_NO_ERROR;
}


static int ccorrect_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  _sdata *sdata;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  int width=weed_get_int_value(in_channel,"width",&error)*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int psize=4;

  unsigned char *end=src+height*irowstride;
  int inplace=(dst==src);
  int offs=0;
  int palette=weed_get_int_value(in_channel,"current_palette",&error);
  weed_plant_t **params=weed_get_plantptr_array(inst,"in_parameters",&error);

  double red=weed_get_double_value(params[0],"value",&error);
  double green=weed_get_double_value(params[1],"value",&error);
  double blue=weed_get_double_value(params[2],"value",&error);

  register int i;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (red!=sdata->ored) {
    make_table(sdata->r,red);
    sdata->ored=red;
  }

  if (green!=sdata->ogreen) {
    make_table(sdata->g,green);
    sdata->ogreen=green;
  }

  if (blue!=sdata->oblue) {
    make_table(sdata->b,blue);
    sdata->oblue=blue;
  }

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    int offset=weed_get_int_value(out_channel,"offset",&error);
    int dheight=weed_get_int_value(out_channel,"height",&error);

    src+=offset*irowstride;
    dst+=offset*orowstride;
    end=src+dheight*irowstride;
  }

  if (palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_BGR24) psize=3;
  if (palette==WEED_PALETTE_ARGB32) offs=1;

  for (; src<end; src+=irowstride) {
    for (i=0; i<width; i+=psize) {
      if (palette==WEED_PALETTE_BGR24||palette==WEED_PALETTE_BGRA32) {
        dst[i]=sdata->b[src[i]];
        dst[i+1]=sdata->g[src[i+1]];
        dst[i+2]=sdata->r[src[i+2]];
        if (!inplace&&palette==WEED_PALETTE_BGRA32) dst[i+3]=src[i+3];
      } else {
        if (!inplace&&palette==WEED_PALETTE_ARGB32) dst[i]=src[i];
        dst[i+offs]=sdata->r[src[i+offs]];
        dst[i+1+offs]=sdata->g[src[i+1+offs]];
        dst[i+2+offs]=sdata->b[src[i+2+offs]];
        if (!inplace&&palette==WEED_PALETTE_RGBA32) dst[i+3]=src[i+3];
      }
    }
    dst+=orowstride;
  }
  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};

    weed_plant_t *in_params[]= {weed_float_init("red","_Red factor",1.0,0.0,2.0),weed_float_init("green","_Green factor",1.0,0.0,2.0),weed_float_init("blue","_Blue factor",1.0,0.0,2.0),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("colour correction","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,&ccorrect_init,
                               &ccorrect_process,&ccorrect_deinit,in_chantmpls,out_chantmpls,in_params,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

