// haip.c
// weed plugin
// (c) G. Finch (salsaman) 2006 - 2012
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

#define NUM_WRMS 100

typedef struct {
  int x;
  int y;
  uint32_t fastrand_val;
  int *px;
  int *py;
  int *wt;
  int old_width;
  int old_height;
} _sdata;


static inline uint32_t fastrand(_sdata *sdata) {
#define rand_a 1073741789L
#define rand_c 32749L

  return ((sdata->fastrand_val*=rand_a) + rand_c);
}

static int ress[8];

static unsigned short Y_R[256];
static unsigned short Y_G[256];
static unsigned short Y_B[256];


static void init_luma_arrays(void) {
  register int i;

  for (i=0; i<256; i++) {
    Y_R[i]=.299*(float)i*256.;
    Y_G[i]=.587*(float)i*256.;
    Y_B[i]=.114*(float)i*256.;
  }

}


int haip_init(weed_plant_t *inst) {
  _sdata *sdata;
  int i;
  sdata=weed_malloc(sizeof(_sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->x=sdata->y=-1;

  sdata->fastrand_val=0; // TODO - seed with random seed
  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  sdata->px=weed_malloc(NUM_WRMS*sizeof(int));
  sdata->py=weed_malloc(NUM_WRMS*sizeof(int));
  sdata->wt=weed_malloc(NUM_WRMS*sizeof(int));

  for (i=0; i<NUM_WRMS; i++) {
    sdata->px[i]=sdata->py[i]=-1;
  }

  sdata->old_width=sdata->old_height=-1;

  return WEED_NO_ERROR;


}


int haip_deinit(weed_plant_t *inst) {
  _sdata *sdata;
  int error;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  weed_free(sdata->wt);
  weed_free(sdata->px);
  weed_free(sdata->py);
  weed_free(sdata);
  return WEED_NO_ERROR;
}


static inline int calc_luma(unsigned char *pt) {
  return (Y_R[pt[0]]+Y_G[pt[1]]+Y_B[pt[2]])>>8;
}



static int make_eight_table(unsigned char *pt, int row, int luma, int adj) {
  int n=0;

  for (n=0; n<8; n++) ress[n]=-1;

  n=0;

  if (calc_luma(&pt[-row-3])>=(luma-adj)) {
    ress[n]=0;
    n++;
  }
  if (calc_luma(&pt[-row])>=(luma-adj)) {
    ress[n]=1;
    n++;
  }
  if (calc_luma(&pt[-row+3])>=(luma-adj)) {
    ress[n]=2;
    n++;
  }
  if (calc_luma(&pt[-3])>=(luma-adj)) {
    ress[n]=3;
    n++;
  }
  if (calc_luma(&pt[3])>=(luma-adj)) {
    ress[n]=4;
    n++;
  }
  if (calc_luma(&pt[row-3])>=(luma-adj)) {
    ress[n]=5;
    n++;
  }
  if (calc_luma(&pt[row])>=(luma-adj)) {
    ress[n]=6;
    n++;
  }
  if (calc_luma(&pt[row+3])>=(luma-adj)) {
    ress[n]=7;
    n++;
  }

  return n;
}


static int select_dir(_sdata *sdata) {
  int num_choices=1;
  int i;
  int mychoice;

  for (i=0; i<8; i++) {
    if (ress[i]!=-1) num_choices++;
  }

  if (num_choices==0) return 1;

  sdata->fastrand_val=fastrand(sdata);
  mychoice=(int)(((sdata->fastrand_val>>24)/255.*num_choices));

  switch (ress[mychoice]) {
  case 0:
    sdata->x=sdata->x-1;
    sdata->y=sdata->y-1;
    break;
  case 1:
    sdata->y=sdata->y-1;
    break;
  case 2:
    sdata->x=sdata->x+1;
    sdata->y=sdata->y-1;
    break;
  case 3:
    sdata->x=sdata->x-1;
    break;
  case 4:
    sdata->x=sdata->x+1;
    break;
  case 5:
    sdata->x=sdata->x-1;
    sdata->y=sdata->y+1;
    break;
  case 6:
    sdata->y=sdata->y+1;
    break;
  case 7:
    sdata->x=sdata->x+1;
    sdata->y=sdata->y+1;
    break;
  }
  return 0;
}

static inline void
nine_fill(unsigned char *new_data, int row, unsigned char *old_data) {
  // fill nine pixels with the centre colour
  new_data[-row-3]=new_data[-row]=new_data[-row+3]=new_data[-3]=new_data[0]=new_data[3]=new_data[row-3]=new_data[row]=new_data[row+3]=
                                    old_data[0];
  new_data[-row-2]=new_data[-row+1]=new_data[-row+4]=new_data[-2]=new_data[1]=new_data[4]=new_data[row-2]=new_data[row+1]=new_data[row+4]=
                                      old_data[1];
  new_data[-row-1]=new_data[-row+2]=new_data[-row+5]=new_data[-1]=new_data[2]=new_data[5]=new_data[row-1]=new_data[row+2]=new_data[row+5]=
                                      old_data[2];
}

static inline void
black_fill(unsigned char *new_data, int row) {
  // fill nine pixels with the centre colour
  new_data[-row-3]=new_data[-row]=new_data[-row+3]=new_data[-3]=new_data[0]=new_data[3]=new_data[row-3]=new_data[row]=new_data[row+3]=0;
  new_data[-row-2]=new_data[-row+1]=new_data[-row+4]=new_data[-2]=new_data[1]=new_data[4]=new_data[row-2]=new_data[row+1]=new_data[row+4]=0;
  new_data[-row-1]=new_data[-row+2]=new_data[-row+5]=new_data[-1]=new_data[2]=new_data[5]=new_data[row-1]=new_data[row+2]=new_data[row+5]=0;
}

static inline void
white_fill(unsigned char *new_data, int row) {
  // fill nine pixels with the centre colour
  new_data[-row-3]=new_data[-row]=new_data[-row+3]=new_data[-3]=new_data[0]=new_data[3]=new_data[row-3]=new_data[row]=new_data[row+3]=255;
  new_data[-row-2]=new_data[-row+1]=new_data[-row+4]=new_data[-2]=new_data[1]=new_data[4]=new_data[row-2]=new_data[row+1]=new_data[row+4]=255;
  new_data[-row-1]=new_data[-row+2]=new_data[-row+5]=new_data[-1]=new_data[2]=new_data[5]=new_data[row-1]=new_data[row+2]=new_data[row+5]=255;
}


static void proc_pt(unsigned char *dest, unsigned char *src, int x, int y, int orows, int irows, int wt) {
  //nine_fill(&dest[rows*y+x*3],rows,&src[rows*y+x*3]);
  switch (wt) {
  case 0:
    black_fill(&dest[orows*y+x*3],orows);
    break;
  case 1:
    white_fill(&dest[orows*y+x*3],orows);
    break;
  case 2:
    nine_fill(&dest[orows*y+x*3],orows,&src[irows*y+x*3]);
    break;
  }
}

int haip_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  _sdata *sdata;
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error),width3=width*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  register int i;

  float scalex,scaley;

  unsigned char *pt;
  int count;

  int luma,adj;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);


  for (i=0; i<height; i++) {
    weed_memcpy(&dst[i*orowstride],&src[i*irowstride],width3);
  }

  if (sdata->old_width==-1) {
    sdata->old_width=width;
    sdata->old_height=height;
  }

  scalex=(float)width/(float)sdata->old_width;
  scaley=(float)height/(float)sdata->old_height;

  for (i=0; i<NUM_WRMS; i++) {
    count=1000;
    if (sdata->px[i]==-1) {
      sdata->fastrand_val=fastrand(sdata);
      sdata->px[i]=(int)(((sdata->fastrand_val>>24)/255.*(width-2)))+1;
      sdata->fastrand_val=fastrand(sdata);
      sdata->py[i]=(int)(((sdata->fastrand_val>>24)/255.*(height-2)))+1;
      sdata->fastrand_val=fastrand(sdata);
      sdata->wt[i]=(int)(((sdata->fastrand_val>>24)/255.*2));
    }

    sdata->x=(float)sdata->px[i]*scalex;
    sdata->y=(float)sdata->py[i]*scaley;

    while (count>0) {
      if (sdata->x<1) sdata->x++;
      if (sdata->x>width-2) sdata->x=width-2;
      if (sdata->y<1) sdata->y++;
      if (sdata->y>height-2) sdata->y=height-2;

      proc_pt(dst,src,sdata->x,sdata->y,orowstride,irowstride,sdata->wt[i]);

      if (sdata->x<1) sdata->x++;
      if (sdata->x>width-2) sdata->x=width-2;
      if (sdata->y<1) sdata->y++;
      if (sdata->y>height-2) sdata->y=height-2;
      pt=&src[sdata->y*irowstride+sdata->x*3];

      luma=calc_luma(pt);
      adj=0;

      make_eight_table(pt,irowstride,luma,adj);
      if (((count<<7)>>7)==count) select_dir(sdata);
      count--;
    }
    sdata->px[i]=sdata->x;
    sdata->py[i]=sdata->y;
  }

  sdata->old_width=width;
  sdata->old_height=height;

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("haip","salsaman",1,0,&haip_init,&haip_process,&haip_deinit,in_chantmpls,out_chantmpls,
                               NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
    init_luma_arrays();
  }
  return plugin_info;
}

