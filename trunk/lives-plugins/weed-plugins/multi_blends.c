// multi_blends.c
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

/////////////////////////////////////////////////////////////

/* precomputed tables */
#define FP_BITS 16

static int Y_R[256];
static int Y_G[256];
static int Y_B[256];
static int conv_RY_inited = 0;

static int myround(double n) {
  if (n >= 0)
    return (int)(n + 0.5);
  else
    return (int)(n - 0.5);
}



static void init_RGB_to_YCbCr_tables(void) {
  int i;

  /*
   * Q_Z[i] =   (coefficient * i
   *             * (Q-excursion) / (Z-excursion) * fixed-pogint-factor)
   *
   * to one of each, add the following:
   *             + (fixed-pogint-factor / 2)         --- for rounding later
   *             + (Q-offset * fixed-pogint-factor)  --- to add the offset
   *
   */
  for (i = 0; i < 256; i++) {
    Y_R[i] = myround(0.299 * (double)i
                     * 219.0 / 255.0 * (double)(1<<FP_BITS));
    Y_G[i] = myround(0.587 * (double)i
                     * 219.0 / 255.0 * (double)(1<<FP_BITS));
    Y_B[i] = myround((0.114 * (double)i
                      * 219.0 / 255.0 * (double)(1<<FP_BITS))
                     + (double)(1<<(FP_BITS-1))
                     + (16.0 * (double)(1<<FP_BITS)));

  }
  conv_RY_inited = 1;
}


static inline unsigned char
calc_luma(unsigned char *pixel) {
  // TODO - RGB
  return (Y_R[pixel[2]] + Y_G[pixel[1]]+ Y_B[pixel[0]]) >> FP_BITS;
}

/////////////////////////////////////////////////////////////////////////

int common_init(weed_plant_t *inst) {

  return WEED_NO_ERROR;
}


static int common_process(int type, weed_plant_t *inst, weed_timecode_t timecode) {
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
  unsigned char *end=src1+height*irowstride1;
  weed_plant_t *in_param;
  int intval;

  register int j;

  int bf;
  unsigned char luma1,luma2;

  unsigned char pixel[3];
  unsigned char blend_factor,blend1,blend2,blendneg1,blendneg2;

  in_param=weed_get_plantptr_value(inst,"in_parameters",&error);
  bf=weed_get_int_value(in_param,"value",&error);
  blend_factor=(unsigned char)bf;

  blend1=blend_factor*2;
  blendneg1=255-blend_factor*2;

  blend2=(255-blend_factor)*2;
  blendneg2=(blend_factor-128)*2;

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    int offset=weed_get_int_value(out_channel,"offset",&error);
    int dheight=weed_get_int_value(out_channel,"height",&error);

    src1+=offset*irowstride1;
    end=src1+dheight*irowstride1;

    src2+=offset*irowstride2;

    dst+=offset*orowstride;

  }

  for (; src1<end; src1+=irowstride1) {
    for (j=0; j<width; j+=3) {
      switch (type) {
      case 0:
        // multiply
        pixel[0]=(unsigned char)((src2[j]*src1[j])>>8);
        pixel[1]=(unsigned char)((src2[j+1]*src1[j+1])>>8);
        pixel[2]=(unsigned char)((src2[j+2]*src1[j+2])>>8);
        break;
      case 1:
        // screen
        pixel[0]=(unsigned char)(255-(((255-src2[j])*(255-src1[j]))>>8));
        pixel[1]=(unsigned char)(255-(((255-src2[j+1])*(255-src1[j+1]))>>8));
        pixel[2]=(unsigned char)(255-(((255-src2[j+2])*(255-src1[j+2]))>>8));
        break;
      case 2:
        // darken
        luma1=(unsigned char)(calc_luma(&src1[j]));
        luma2=(unsigned char)(calc_luma(&src2[j]));
        if (luma1<=luma2) weed_memcpy(pixel,&src1[j],3);
        else weed_memcpy(pixel,&src2[j],3);
        break;
      case 3:
        // lighten
        luma1=(unsigned char)(calc_luma(&src1[j]));
        luma2=(unsigned char)(calc_luma(&src2[j]));
        if (luma1>=luma2) weed_memcpy(pixel,&src1[j],3);
        else weed_memcpy(pixel,&src2[j],3);
        break;
      case 4:
        // overlay
        luma1=calc_luma(&src1[j]);
        if (luma1<128) {
          // mpy
          pixel[0]=(unsigned char)((src2[j]*src1[j])>>8);
          pixel[1]=(unsigned char)((src2[j+1]*src1[j+1])>>8);
          pixel[2]=(unsigned char)((src2[j+2]*src1[j+2])>>8);
        } else {
          // screen
          pixel[0]=(unsigned char)(255-(((255-src2[j])*(255-src1[j]))>>8));
          pixel[1]=(unsigned char)(255-(((255-src2[j+1])*(255-src1[j+1]))>>8));
          pixel[2]=(unsigned char)(255-(((255-src2[j+2])*(255-src1[j+2]))>>8));
        }
        break;
      case 5:
        // dodge
        if (src2[j]==255) pixel[0]=255;
        else {
          intval=((int)(src1[j])<<8)/(int)(255-src2[j]);
          pixel[0]=intval>255?255:(unsigned char)intval;
        }
        if (src2[j+1]==255) pixel[1]=255;
        else {
          intval=((int)(src1[j+1])<<8)/(int)(255-src2[j+1]);
          pixel[1]=intval>255?255:(unsigned char)intval;
        }
        if (src2[j+2]==255) pixel[2]=255;
        else {
          intval=((int)(src1[j+2])<<8)/(int)(255-src2[j+2]);
          pixel[2]=intval>255?255:(unsigned char)intval;
        }
        break;
      case 6:
        // burn
        if (src2[j]==0) pixel[0]=0;
        else {
          intval=255-(255-((int)(src1[j])<<8))/(int)(src2[j]);
          pixel[0]=intval<0?0:(unsigned char)intval;
        }
        if (src2[j+1]==0) pixel[1]=0;
        else {
          intval=255-(255-((int)(src1[j+1])<<8))/(int)(src2[j+1]);
          pixel[1]=intval<0?0:(unsigned char)intval;
        }
        if (src2[j+2]==0) pixel[2]=0;
        else {
          intval=255-(255-((int)(src1[j+2])<<8))/(int)(src2[j+2]);
          pixel[2]=intval<0?0:(unsigned char)intval;
        }
        break;
      }
      if (blend_factor<128) {
        dst[j]=(blend1*pixel[0]+blendneg1*src1[j])>>8;
        dst[j+1]=(blend1*pixel[1]+blendneg1*src1[j+1])>>8;
        dst[j+2]=(blend1*pixel[2]+blendneg1*src1[j+2])>>8;
      } else {
        dst[j]=(blend2*pixel[0]+blendneg2*src2[j])>>8;
        dst[j+1]=(blend2*pixel[1]+blendneg2*src2[j+1])>>8;
        dst[j+2]=(blend2*pixel[2]+blendneg2*src2[j+2])>>8;
      }

    }
    src2+=irowstride2;
    dst+=orowstride;
  }
  weed_free(in_channels);
  return WEED_NO_ERROR;
}

int common_deinit(weed_plant_t *filter_instance) {
  return WEED_NO_ERROR;
}



int mpy_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(0,inst,timestamp);
}

int screen_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(1,inst,timestamp);
}

int darken_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(2,inst,timestamp);
}

int lighten_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(3,inst,timestamp);
}

int overlay_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(4,inst,timestamp);
}

int dodge_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(5,inst,timestamp);
}

int burn_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(6,inst,timestamp);
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  weed_plant_t **clone1,**clone2,**clone3;

  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),weed_channel_template_init("in channel 1",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    weed_plant_t *in_params1[]= {weed_integer_init("amount","Blend _amount",128,0,255),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("blend_multiply","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&mpy_process,NULL,
                               in_chantmpls,out_chantmpls,in_params1,NULL);

    weed_set_boolean_value(in_params1[0],"transition",WEED_TRUE);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    filter_class=weed_filter_class_init("blend_screen","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&screen_process,NULL,
                                        (clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),(clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);

    filter_class=weed_filter_class_init("blend_darken","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&darken_process,NULL,
                                        (clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),(clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);

    filter_class=weed_filter_class_init("blend_lighten","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&lighten_process,NULL,
                                        (clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),(clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);

    filter_class=weed_filter_class_init("blend_overlay","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&overlay_process,NULL,
                                        (clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),(clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);

    filter_class=weed_filter_class_init("blend_dodge","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&dodge_process,NULL,
                                        (clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),(clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);

    filter_class=weed_filter_class_init("blend_burn","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,NULL,&burn_process,NULL,
                                        (clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),(clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);

    weed_set_int_value(plugin_info,"version",package_version);

    init_RGB_to_YCbCr_tables();
  }
  return plugin_info;
}
