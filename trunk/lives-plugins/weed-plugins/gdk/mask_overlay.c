// mask_overlay.c
// weed plugin - resize using gdk
// (c) G. Finch (salsaman) 2011
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-palettes.h"
#include "weed/weed-effects.h"
#include "weed/weed-plugin.h"
#else
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-palettes.h"
#include "../../../libweed/weed-effects.h"
#include "../../../libweed/weed-plugin.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed-utils.h" // optional
#include "weed/weed-plugin-utils.h" // optional
#else
#include "../../../libweed/weed-utils.h" // optional
#include "../../../libweed/weed-plugin-utils.h" // optional
#endif

/////////////////////////////////////////////////////////////

typedef struct _sdata {
  int *xmap; // x offset in src0 to map to point (or -1 to map src1)
  int *ymap; // x offset in src0 to map to point (or -1 to map src1)
} sdata;



#include <gdk/gdk.h>



//inline int calc_luma(unsigned char *pt) {
  // return luma 0<=x<=256
//  return 0.21*(float)pt[0]+0.587*(float)pt[1]+0.114*(float)pt[2];
//}



static void make_mask(GdkPixbuf *pbuf, int mode, int owidth, int oheight, int *xmap, int *ymap) {
  int iwidth=gdk_pixbuf_get_width(pbuf);
  int iheight=gdk_pixbuf_get_height(pbuf);
  gboolean has_alpha=gdk_pixbuf_get_has_alpha(pbuf);
  int stride=gdk_pixbuf_get_rowstride(pbuf);
  guchar *pdata=gdk_pixbuf_get_pixels(pbuf);

  double xscale=(double)iwidth/(double)owidth;
  double yscale=(double)iheight/(double)oheight;

  int psize=3;

  register int i,j;

  if (has_alpha) psize=4;

  for (i=0;i<oheight;i++) {
    for (j=0;j<owidth;j++) {
      if ( *(pdata + (int)(i * yscale) * stride + (int)(j * xscale) * psize + 1) == 0) {
	// map front frame

	if (mode==0) {
	  // no re-mapping of front frame
	  xmap[i*owidth+j]=j;
	  ymap[i*owidth+j]=i;
	}

      }
      else {
	// map back frame
	xmap[i*owidth+j]=ymap[i*owidth+j]=-1;
      }

    }
  }

}








int masko_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int video_height,video_width,video_area;
  int error;
  weed_plant_t *in_channel,**in_params;
  GdkPixbuf *pbuf;
  GError *gerr=NULL;
  char *mfile;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  sdata=weed_malloc(sizeof(struct _sdata));

  if (sdata == NULL ) return WEED_ERROR_MEMORY_ALLOCATION;

  video_height=weed_get_int_value(in_channel,"height",&error);
  video_width=weed_get_int_value(in_channel,"width",&error);
  video_area=video_width*video_height;


  sdata->xmap=(int *)weed_malloc(video_area*sizeof(int));

  if (sdata->xmap == NULL ) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->ymap=(int *)weed_malloc(video_area*sizeof(int));

  if (sdata->ymap == NULL ) {
    weed_free(sdata->xmap);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  // load image, then get luma values and scale
  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  mfile=weed_get_string_value(in_params[0],"value",&error);

  pbuf=gdk_pixbuf_new_from_file(mfile,&gerr);


  make_mask(pbuf,0,video_width,video_height,sdata->xmap,sdata->ymap);

  gdk_pixbuf_unref(pbuf);
  weed_free(mfile);
  weed_free(in_params);

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;

}


int masko_deinit(weed_plant_t *inst) {
  int error;
  struct _sdata *sdata;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  weed_free(sdata->xmap);
  weed_free(sdata->ymap);
  weed_free(sdata);

  return WEED_NO_ERROR;

}






int masko_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t **in_channels=weed_get_plantptr_array(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  int palette=weed_get_int_value(out_channel,"current_palette",&error);
  int width=weed_get_int_value(out_channel,"width",&error);
  int height=weed_get_int_value(out_channel,"height",&error);
  int offset=0;

  register int i,j,pos;
  struct _sdata *sdata;

  int psize=3;

  unsigned char *dst,*src0,*src1;
  int orow,irow0,irow1;

  if (palette==WEED_PALETTE_RGBA32||palette==WEED_PALETTE_BGRA32||palette==WEED_PALETTE_ARGB32||palette==WEED_PALETTE_YUVA8888) psize=4;


  dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  src0=weed_get_voidptr_value(in_channels[0],"pixel_data",&error);
  src1=weed_get_voidptr_value(in_channels[1],"pixel_data",&error);

  orow=weed_get_int_value(out_channel,"rowstrides",&error);
  irow0=weed_get_int_value(in_channels[0],"rowstrides",&error);
  irow1=weed_get_int_value(in_channels[1],"rowstrides",&error);

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    offset=weed_get_int_value(out_channel,"offset",&error);
    int dheight=weed_get_int_value(out_channel,"height",&error);
    height=offset+dheight;
    dst+=offset*orow;
    src1+=offset*irow1;
  }

  pos=offset*width;
  orow-=width*psize;
  irow1-=width*psize;

  for (i=offset;i<height;i++) {
    for (j=0;j<width;j++) {
      if (sdata->xmap[pos]==-1||sdata->ymap[pos]==-1) {
	// map bg pixel to dst
	weed_memcpy(dst,src1,psize);
      }
      else {
	// remap fg pixel
	weed_memcpy(dst,src0+sdata->ymap[pos]*irow0+sdata->xmap[pos]*psize,psize);
      }
      dst+=psize;
      src1+=psize;
      pos++;
    }
    dst+=orow;
    src1+=irow1;
  }

  weed_free(in_channels);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_YUV888,WEED_PALETTE_YUVA8888,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),weed_channel_template_init("in channel 1",0,palette_list),NULL};

    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *filter_class;
    weed_plant_t *in_params[3],*gui;
    char *rfx_strings[]={"special|fileread|0|"};
    char *modes[]={"normal","stretch",NULL};

    char *defmaskfile=g_build_filename(g_get_home_dir(), "mask.png", NULL);
    int flags,error;

    in_params[0]=weed_text_init("maskfile","_Mask file (.png or .jpg)",defmaskfile);
    flags=0;
    if (weed_plant_has_leaf(in_params[0],"flags")) 
      flags=weed_get_int_value(in_params[0],"flags",&error);
    flags|=WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
    weed_set_int_value(in_params[0],"flags",flags);

    in_params[1]=weed_string_list_init("mode","Effect _mode",0,modes);
    flags=0;
    if (weed_plant_has_leaf(in_params[1],"flags")) 
      flags=weed_get_int_value(in_params[1],"flags",&error);
    flags|=WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
    weed_set_int_value(in_params[1],"flags",flags);
    in_params[2]=NULL;

    g_free(defmaskfile);

    filter_class=weed_filter_class_init("mask_overlay","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,&masko_init,&masko_process,&masko_deinit,in_chantmpls,out_chantmpls,in_params,NULL);
    
    gui=weed_filter_class_get_gui(filter_class);
    weed_set_string_value(gui,"layout_scheme","RFX");
    weed_set_string_value(gui,"rfx_delim","|");
    weed_set_string_array(gui,"rfx_strings",1,rfx_strings);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }
  return plugin_info;
}

