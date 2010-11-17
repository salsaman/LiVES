// gdk_fast_resize.c
// weed plugin - resize using gdk
// (c) G. Finch (salsaman) 2007
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
  unsigned char *bgbuf;
  int pwidth;
  int pheight;
  int count;

} sdata;



#include <gdk/gdk.h>

inline G_GNUC_CONST int pl_gdk_rowstride_value (int rowstride) {
  // from gdk-pixbuf.c
  /* Always align rows to 32-bit boundaries */
  return (rowstride + 3) & ~3;
}

inline int G_GNUC_CONST pl_gdk_last_rowstride_value (int width, int nchans) {
  // from gdk pixbuf docs
  return width*(((nchans<<3)+7)>>3);
}

static void plugin_free_buffer (guchar *pixels, gpointer data) {
  return;
}


static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height, guchar *buf) {
  // we can cheat if our buffer is correctly sized
  int channels=has_alpha?4:3;
  int rowstride=pl_gdk_rowstride_value(width*channels);
  return gdk_pixbuf_new_from_data (buf, colorspace, has_alpha, bits_per_sample, width, height, rowstride, plugin_free_buffer, NULL);
}



static GdkPixbuf *pl_channel_to_pixbuf (weed_plant_t *channel) {
  int error;
  GdkPixbuf *pixbuf;
  int palette=weed_get_int_value(channel,"current_palette",&error);
  int width=weed_get_int_value(channel,"width",&error);
  int height=weed_get_int_value(channel,"height",&error);
  int irowstride=weed_get_int_value(channel,"rowstrides",&error);
  int rowstride,orowstride;
  guchar *pixel_data=(guchar *)weed_get_voidptr_value(channel,"pixel_data",&error),*pixels,*end;
  gboolean cheat=FALSE;
  gint n_channels;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    if (irowstride==pl_gdk_rowstride_value(width*3)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat=TRUE;
    }
    else pixbuf=gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels=3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32: // TODO - change to RGBA ??
    if (irowstride==pl_gdk_rowstride_value(width*4)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, TRUE, 8, width, height, pixel_data);
      cheat=TRUE;
    }
    else pixbuf=gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    n_channels=4;
    break;
  default:
    return NULL;
  }
  pixels=gdk_pixbuf_get_pixels (pixbuf);
  orowstride=gdk_pixbuf_get_rowstride(pixbuf);
  
  if (irowstride>orowstride) rowstride=orowstride;
  else rowstride=irowstride;
  end=pixels+orowstride*height;

  if (!cheat) {
    gboolean done=FALSE;
    for (;pixels<end&&!done;pixels+=orowstride) {
      if (pixels+orowstride>=end) {
	orowstride=rowstride=pl_gdk_last_rowstride_value(width,n_channels);
	done=TRUE;
      }
      weed_memcpy(pixels,pixel_data,rowstride);
      if (rowstride<orowstride) weed_memset (pixels+rowstride,0,orowstride-rowstride);
      pixel_data+=irowstride;
    }
  }
  return pixbuf;
}



int videowall_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int video_height,video_width,video_area;
  int palette;
  int error;
  int psize;
  weed_plant_t *in_channel;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  sdata=weed_malloc(sizeof(struct _sdata));

  if (sdata == NULL ) return WEED_ERROR_MEMORY_ALLOCATION;

  palette=weed_get_int_value(in_channel,"height",&error);
  video_height=weed_get_int_value(in_channel,"height",&error);
  video_width=weed_get_int_value(in_channel,"width",&error);
  video_area=video_width*video_height;


  sdata->bgbuf=weed_malloc((psize=video_area*(palette==WEED_PALETTE_RGB24?3:4)));

  if (sdata->bgbuf == NULL ) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_memset(sdata->bgbuf,0,psize);

  sdata->pwidth=video_width/3;
  sdata->pheight=video_height/3;

  sdata->count=0;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;

}


int videowall_deinit(weed_plant_t *inst) {
  int error;
  struct _sdata *sdata;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  weed_free(sdata->bgbuf);
  weed_free(sdata);

  return WEED_NO_ERROR;

}






int videowall_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  int palette=weed_get_int_value(in_channel,"current_palette",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);

  GdkPixbuf *in_pixbuf=pl_channel_to_pixbuf(in_channel);
  GdkPixbuf *out_pixbuf;

  int down_interp=GDK_INTERP_BILINEAR;

  register int i,j;
  struct _sdata *sdata;

  int psize,prow,orow,pwidth,pheight;

  unsigned char *bdst,*dst,*rpix;

  int ofh=0,ofw=0;

  dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  out_pixbuf=gdk_pixbuf_scale_simple(in_pixbuf,sdata->pwidth,sdata->pheight,down_interp);

  g_object_unref(in_pixbuf);

  psize=(palette==WEED_PALETTE_RGB24?3:4);

  if (sdata->count>2) {
    ofh=sdata->pheight;
  }
  if (sdata->count>5) {
    ofh+=sdata->pheight;
  }

  if (sdata->count==1||sdata->count==4||sdata->count==7) {
    ofw=sdata->pwidth*psize;
  }
  else if (sdata->count==2||sdata->count==5||sdata->count==8) {
    ofw=sdata->pwidth*psize*2;
  }

  bdst=sdata->bgbuf+ofh*width*psize;

  prow=gdk_pixbuf_get_rowstride(out_pixbuf);
  rpix=gdk_pixbuf_get_pixels(out_pixbuf);
  pwidth=gdk_pixbuf_get_width(out_pixbuf);
  pheight=gdk_pixbuf_get_height(out_pixbuf);

  // copy pixel_data to bgbuf

  for (i=0;i<pheight;i++) {
    for (j=0;j<pwidth;j++) {
      weed_memcpy(bdst+ofw,rpix,psize);
      bdst+=psize;
      rpix+=psize;
    }
    bdst+=(width-pwidth)*psize;
    rpix+=(prow-pwidth*psize);
  }
  
  g_object_unref(out_pixbuf);


  if (++sdata->count==9) sdata->count=0;
  orow=weed_get_int_value(out_channel,"rowstrides",&error);

  if (orow==width*psize) {
    weed_memcpy(dst,sdata->bgbuf,width*psize*height);
  }
  else {
    for (i=0;i<height;i++) {
      weed_memcpy(dst,sdata->bgbuf+i*width*psize,width*psize);
      dst+=orow;
    }
  }

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE|WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("videowall","salsaman",1,0,&videowall_init,&videowall_process,&videowall_deinit,in_chantmpls,out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }
  return plugin_info;
}

