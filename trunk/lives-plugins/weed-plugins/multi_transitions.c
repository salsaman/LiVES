// weed plugin
// (c) G. Finch (salsaman) 2009
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

#include <math.h>
#include <stdlib.h>
#include <inttypes.h>

////////////////////////////////////////////////////////////

typedef struct {
  float *mask;
  uint32_t fastrand_val;
} _sdata;



#include <sys/time.h>
static uint32_t fastrand_seed(uint32_t sval) {
  // set random seed from time
  struct timeval otv;
  uint32_t fval;

  gettimeofday(&otv, NULL);
  fval=(otv.tv_sec^otv.tv_usec^sval)&0xFFFFFFFF;
  return fval;
}



static inline uint32_t fastrand(_sdata *sdata) {
#define rand_a 1073741789L
#define rand_c 32749L

  return ((sdata->fastrand_val*=rand_a) + rand_c);
}


int dissolve_init(weed_plant_t *inst) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  _sdata *sdata;
  register int i,j;
  int end=width*height;

  sdata=weed_malloc(sizeof(_sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->mask=(float *)weed_malloc(width*height*sizeof(float));

  if (sdata->mask == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->fastrand_val=fastrand_seed(0x91FD57B4); // random seed

  for (i=0; i<end; i+=width) {
    for (j=0; j<width; j++) {
      sdata->mask[i+j]=(float)((double)(sdata->fastrand_val=fastrand(sdata))/(double)UINT32_MAX);
    }
  }

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}

int dissolve_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata->mask!=NULL) weed_free(sdata->mask);
  return WEED_NO_ERROR;
}



static int common_process(int type, weed_plant_t *inst, weed_timecode_t timecode) {
  int error;
  weed_plant_t **in_channels=weed_get_plantptr_array(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                             &error);
  unsigned char *src1=weed_get_voidptr_value(in_channels[0],"pixel_data",&error);
  unsigned char *src2=weed_get_voidptr_value(in_channels[1],"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channels[0],"width",&error);
  int height=weed_get_int_value(in_channels[0],"height",&error);
  int irowstride1=weed_get_int_value(in_channels[0],"rowstrides",&error);
  int irowstride2=weed_get_int_value(in_channels[1],"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int cpal=weed_get_int_value(out_channel,"current_palette",&error);
  unsigned char *end=src1+height*irowstride1;
  weed_plant_t *in_param;
  size_t psize=4;
  _sdata *sdata=NULL;

  int inplace=(src1==dst);

  float hwidth,hheight=(float)height*0.5f;

  register int i=0,j;

  float bf,bfneg;

  int xx=0,yy=0,ihwidth,ihheight=height>>1;

  float maxradsq=0.,xxf,yyf;

  if (cpal==WEED_PALETTE_RGB24||cpal==WEED_PALETTE_BGR24||cpal==WEED_PALETTE_YUV888) psize=3;

  hwidth=(float)width*0.5f;
  if (type==1) maxradsq=((hheight*hheight)+(hwidth*hwidth));

  width*=psize;

  hwidth=(float)width*0.5f;

  ihwidth=width>>1;

  in_param=weed_get_plantptr_value(inst,"in_parameters",&error);
  bf=weed_get_double_value(in_param,"value",&error);
  bfneg=1.f-bf;

  if (type==3) sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);
  else if (type==2) {
    xx=(int)(hheight*bf+.5)*irowstride1;
    yy=(int)(hwidth/(float)psize*bf+.5)*psize;
  }

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    int offset=weed_get_int_value(out_channel,"offset",&error);
    int dheight=weed_get_int_value(out_channel,"height",&error);

    src1+=offset*irowstride1;
    end=src1+dheight*irowstride1;

    src2+=offset*irowstride2;

    dst+=offset*orowstride;

    i+=offset;
  }

  for (; src1<end; src1+=irowstride1) {
    for (j=0; j<width; j+=psize) {
      switch (type) {
      case 0:
        // iris rectangle
        xx=(int)hwidth*bfneg+.5;
        yy=(int)hheight*bfneg+.5;
        if (j<xx||j>=(width-xx)||i<yy||i>=(height-yy)) {
          if (!inplace) weed_memcpy(&dst[j],&src1[j],psize);
          else {
            if (i>=(height-yy)) {
              j=width;
              src1=end;
              break;
            }
            if (j>=ihwidth) {
              j=width;
              break;
            }
          }
        } else weed_memcpy(&dst[j],&src2[j],psize);
        break;
      case 1:
        //iris circle
        xxf=(float)(i-ihheight);
        yyf=(float)(j-ihwidth)/(float)psize;
        if (sqrt((xxf*xxf+yyf*yyf)/maxradsq)>bf) {
          if (!inplace) weed_memcpy(&dst[j],&src1[j],psize);
          else {
            if (yyf>=0) {
              j=width;
              if (xxf>0&&yyf==0) src1=end;
              break;
            }
          }
        } else weed_memcpy(&dst[j],&src2[j],psize);
        break;
      case 2:
        // four way split
        if (abs(i-hheight)/hheight<bf||abs(j-hwidth)/hwidth<bf||bf==1.f) {
          weed_memcpy(&dst[j],&src2[j],psize);
        } else {
          weed_memcpy(&dst[j],&src1[j+(j>ihwidth?-yy:yy)+(i>ihheight?-xx:xx)],psize);
        }
        break;
      case 3:
        // dissolve
        if (sdata->mask[(i*width+j)/psize]<bf) weed_memcpy(&dst[j],&src2[j],psize);
        else if (!inplace) weed_memcpy(&dst[j],&src1[j],psize);
        break;
      }

    }
    src2+=irowstride2;
    dst+=orowstride;
    i++;
  }
  weed_free(in_channels);
  return WEED_NO_ERROR;
}


int irisr_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(0,inst,timestamp);
}

int irisc_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(1,inst,timestamp);
}

int fourw_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(2,inst,timestamp);
}

int dissolve_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(3,inst,timestamp);
}



weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  weed_plant_t **clone1,**clone2,**clone3;

  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_YUV888,WEED_PALETTE_YUVA8888,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),weed_channel_template_init("in channel 1",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    weed_plant_t *in_params1[]= {weed_float_init("amount","_Transition",0.,0.,1.),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("iris rectangle","salsaman",1,WEED_FILTER_HINT_IS_STATELESS|WEED_FILTER_HINT_MAY_THREAD,
                               NULL,irisr_process,NULL,in_chantmpls,out_chantmpls,in_params1,NULL);

    weed_set_boolean_value(in_params1[0],"transition",WEED_TRUE);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    filter_class=weed_filter_class_init("iris circle","salsaman",1,WEED_FILTER_HINT_IS_STATELESS|WEED_FILTER_HINT_MAY_THREAD,NULL,irisc_process,
                                        NULL,(clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),(clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);


    weed_set_int_value(out_chantmpls[0],"flags",0);

    filter_class=weed_filter_class_init("4 way split","salsaman",1,WEED_FILTER_HINT_IS_STATELESS|WEED_FILTER_HINT_MAY_THREAD,NULL,fourw_process,
                                        NULL,(clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),(clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);


    weed_set_int_value(out_chantmpls[0],"flags",WEED_CHANNEL_CAN_DO_INPLACE|WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);

    filter_class=weed_filter_class_init("dissolve","salsaman",1,WEED_FILTER_HINT_IS_STATELESS|WEED_FILTER_HINT_MAY_THREAD,dissolve_init,
                                        dissolve_process,dissolve_deinit,(clone1=weed_clone_plants(in_chantmpls)),(clone2=weed_clone_plants(out_chantmpls)),
                                        (clone3=weed_clone_plants(in_params1)),NULL);
    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_free(clone1);
    weed_free(clone2);
    weed_free(clone3);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}
