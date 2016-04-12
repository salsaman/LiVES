// alpha_visualizer.c
// weed plugin
// (c) G. Finch (salsaman) 2016
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// convert alpha values to (R) (G) (B) (A)


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
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

#include <stdio.h>

/////////////////////////////////////////////////////////////

static int getbit(uint8_t val, int bit) {
  int x=1;
  register int i;
  for (i=0; i<bit; i++) x*=2;
  return val&x;
}



int alphav_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  float *alphaf;
  uint8_t *alphau;

  uint8_t *dst=(uint8_t *)weed_get_voidptr_value(out_channel,"pixel_data",&error);

  uint8_t valu;

  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int irow=weed_get_int_value(in_channel,"rowstrides",&error);
  int orow=weed_get_int_value(out_channel,"rowstrides",&error);

  int ipal=weed_get_int_value(in_channel,"current_palette",&error);
  int opal=weed_get_int_value(out_channel,"current_palette",&error);

  int r=weed_get_boolean_value(in_params[0],"value",&error);
  int g=weed_get_boolean_value(in_params[1],"value",&error);
  int b=weed_get_boolean_value(in_params[2],"value",&error);

  int psize=4;

  double fmin=weed_get_double_value(in_params[3],"value",&error);
  double fmax=weed_get_double_value(in_params[4],"value",&error);

  register int i,j,k;

  weed_free(in_params);

  if (opal==WEED_PALETTE_RGB24||opal==WEED_PALETTE_BGR24) psize=3;

  orow=orow-width*psize;

  if (ipal==WEED_PALETTE_AFLOAT) {
    irow/=sizeof(float);
    alphaf=(float *)weed_get_voidptr_value(in_channel,"pixel_data",&error);
    for (i=0; i<height; i++) {
      for (j=0; j<width; j++) {
        if (fmax>fmin) {
          valu=(int)((alphaf[j]-fmin/(fmax-fmin))*255.+.5);
          valu=valu<0?0:valu>255?255:valu;
        } else valu=0;
        switch (opal) {
        case WEED_PALETTE_RGBA32:
          dst[3]=0xFF;
        case WEED_PALETTE_RGB24:
          dst[0]=(r==WEED_TRUE?valu:0);
          dst[1]=(g==WEED_TRUE?valu:0);
          dst[2]=(b==WEED_TRUE?valu:0);
          dst+=psize;
          break;
        case WEED_PALETTE_BGRA32:
          dst[3]=0xFF;
        case WEED_PALETTE_BGR24:
          dst[0]=(b==WEED_TRUE?valu:0);
          dst[1]=(g==WEED_TRUE?valu:0);
          dst[2]=(r==WEED_TRUE?valu:0);
          dst+=psize;
          break;
        case WEED_PALETTE_ARGB32:
          dst[0]=0xFF;
          dst[1]=(r==WEED_TRUE?valu:0);
          dst[2]=(g==WEED_TRUE?valu:0);
          dst[3]=(b==WEED_TRUE?valu:0);
          dst+=psize;
          break;
        default:
          break;
        }
      }
      alphaf+=irow;
      dst+=orow;
    }
  } else if (ipal==WEED_PALETTE_A8) {
    irow-=width;
    alphau=(uint8_t *)weed_get_voidptr_value(in_channel,"pixel_data",&error);
    for (i=0; i<height; i++) {
      for (j=0; j<width; j++) {
        valu=alphau[j];
        switch (opal) {
        case WEED_PALETTE_RGBA32:
          dst[j+3]=0xFF;
        case WEED_PALETTE_RGB24:
          dst[j]=(r==WEED_TRUE?valu:0);
          dst[j+1]=(g==WEED_TRUE?valu:0);
          dst[j+2]=(b==WEED_TRUE?valu:0);
          dst+=psize;
          break;
        case WEED_PALETTE_BGRA32:
          dst[j+3]=0xFF;
        case WEED_PALETTE_BGR24:
          dst[j]=(b==WEED_TRUE?valu:0);
          dst[j+1]=(g==WEED_TRUE?valu:0);
          dst[j+2]=(r==WEED_TRUE?valu:0);
          dst+=3;
          break;
        case WEED_PALETTE_ARGB32:
          dst[j]=0xFF;
          dst[j+1]=(r==WEED_TRUE?valu:0);
          dst[j+2]=(g==WEED_TRUE?valu:0);
          dst[j+3]=(b==WEED_TRUE?valu:0);
          dst+=psize;
          break;
        default:
          break;
        }
      }
      alphau+=irow;
      dst+=orow;
    }
  } else if (ipal==WEED_PALETTE_A1) {
    width>>=3;
    irow-=width;
    alphau=(uint8_t *)weed_get_voidptr_value(in_channel,"pixel_data",&error);
    for (i=0; i<height; i++) {
      for (j=0; j<width; j++) {
        for (k=0; k<8; k++) {
          valu=getbit(alphau[j],k)*255;
          switch (opal) {
          case WEED_PALETTE_RGBA32:
            dst[j+3]=0xFF;
          case WEED_PALETTE_RGB24:
            dst[j]=(r==WEED_TRUE?valu:0);
            dst[j+1]=(g==WEED_TRUE?valu:0);
            dst[j+2]=(b==WEED_TRUE?valu:0);
            dst+=psize;
            break;
          case WEED_PALETTE_BGRA32:
            dst[j+3]=0xFF;
          case WEED_PALETTE_BGR24:
            dst[j]=(b==WEED_TRUE?valu:0);
            dst[j+1]=(g==WEED_TRUE?valu:0);
            dst[j+2]=(r==WEED_TRUE?valu:0);
            dst+=psize;
            break;
          case WEED_PALETTE_ARGB32:
            dst[j]=0xFF;
            dst[j+1]=(r==WEED_TRUE?valu:0);
            dst[j+2]=(g==WEED_TRUE?valu:0);
            dst[j+3]=(b==WEED_TRUE?valu:0);
            dst+=psize;
            break;
          default:
            break;
          }
        }
      }
      alphau+=irow;
      dst+=orow;
    }
  }


  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {

    int palette_list[]= {WEED_PALETTE_RGB24,WEED_PALETTE_BGR24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_END};
    int apalette_list[]= {WEED_PALETTE_AFLOAT,WEED_PALETTE_A8,WEED_PALETTE_A1,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("alpha input",0,apalette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("output",0,palette_list),NULL};

    weed_plant_t *in_params[]= {weed_switch_init("red","_Red",WEED_TRUE),
                                weed_switch_init("green","_Green",WEED_TRUE),
                                weed_switch_init("blue","_Blue",WEED_TRUE),
                                weed_float_init("fmin","Float Min",0.,-1000000.,1000000.),
                                weed_float_init("fmax","Float Max",1.,-1000000.,1000000.),
                                NULL
                               };

    weed_plant_t *filter_class;

    weed_set_int_value(out_chantmpls[0],"flags",WEED_CHANNEL_PALETTE_CAN_VARY);

    filter_class=weed_filter_class_init("alpha_visualizer","salsaman",1,0,
                                        NULL,&alphav_process,NULL,
                                        in_chantmpls,out_chantmpls,
                                        in_params,NULL);

    weed_set_string_value(filter_class,"description",
                          "Visualize a separated alpha channel as red / green / blue (grey)");

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }

  return plugin_info;
}

