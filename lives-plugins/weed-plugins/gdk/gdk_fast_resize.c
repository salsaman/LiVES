// gdk_fast_resize.c
// weed plugin - resize using gdk
// (c) G. Finch (salsaman) 2007
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-palettes.h"
#include "../../../libweed/weed-effects.h"
#endif

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h>
#else
#include "../../../libweed/weed-plugin.h"
#endif


///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#include "../weed-utils-code.c" // optional
#include "../weed-plugin-utils.c" // optional


/////////////////////////////////////////////////////////////

#include <gdk/gdk.h>

inline G_GNUC_CONST int pl_gdk_rowstride_value(int rowstride) {
  // from gdk-pixbuf.c
  /* Always align rows to 32-bit boundaries */
  return (rowstride + 3) & ~3;
}

inline int G_GNUC_CONST pl_gdk_last_rowstride_value(int width, int nchans) {
  // from gdk pixbuf docs
  return width*(((nchans<<3)+7)>>3);
}

static void plugin_free_buffer(guchar *pixels, gpointer data) {
  return;
}


static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height,
                                      guchar *buf) {
  // we can cheat if our buffer is correctly sized
  int channels=has_alpha?4:3;
  int rowstride=pl_gdk_rowstride_value(width*channels);
  return gdk_pixbuf_new_from_data(buf, colorspace, has_alpha, bits_per_sample, width, height, rowstride, plugin_free_buffer, NULL);
}



static GdkPixbuf *pl_channel_to_pixbuf(weed_plant_t *channel) {
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
  case WEED_PALETTE_YUV888:
    if (irowstride==pl_gdk_rowstride_value(width*3)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat=TRUE;
    } else pixbuf=gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels=3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32:
  case WEED_PALETTE_YUVA8888:
    if (irowstride==pl_gdk_rowstride_value(width*4)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, TRUE, 8, width, height, pixel_data);
      cheat=TRUE;
    } else pixbuf=gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    n_channels=4;
    break;
  default:
    return NULL;
  }
  pixels=gdk_pixbuf_get_pixels(pixbuf);
  orowstride=gdk_pixbuf_get_rowstride(pixbuf);

  if (irowstride>orowstride) rowstride=orowstride;
  else rowstride=irowstride;
  end=pixels+orowstride*height;

  if (!cheat) {
    gboolean done=FALSE;
    for (; pixels<end&&!done; pixels+=orowstride) {
      if (pixels+orowstride>=end) {
        orowstride=rowstride=pl_gdk_last_rowstride_value(width,n_channels);
        done=TRUE;
      }
      weed_memcpy(pixels,pixel_data,rowstride);
      if (rowstride<orowstride) weed_memset(pixels+rowstride,0,orowstride-rowstride);
      pixel_data+=irowstride;
    }
  }
  return pixbuf;
}



static gboolean pl_pixbuf_to_channel(weed_plant_t *channel, GdkPixbuf *pixbuf) {
  // return TRUE if we can use the original pixbuf pixels

  int error;
  int rowstride=gdk_pixbuf_get_rowstride(pixbuf);
  int width=gdk_pixbuf_get_width(pixbuf);
  int height=gdk_pixbuf_get_height(pixbuf);
  int n_channels=gdk_pixbuf_get_n_channels(pixbuf);
  guchar *in_pixel_data=(guchar *)gdk_pixbuf_get_pixels(pixbuf);
  int out_rowstride=weed_get_int_value(channel,"rowstrides",&error);
  guchar *dst=weed_get_voidptr_value(channel,"pixel_data",&error);

  register int i;

  if (rowstride==pl_gdk_last_rowstride_value(width,n_channels)&&rowstride==out_rowstride) {
    weed_memcpy(dst,in_pixel_data,rowstride*height);
    return FALSE;
  }

  for (i=0; i<height; i++) {
    if (i==height-1) rowstride=pl_gdk_last_rowstride_value(width,n_channels);
    weed_memcpy(dst,in_pixel_data,rowstride);
    in_pixel_data+=rowstride;
    dst+=out_rowstride;
  }

  return FALSE;
}






int resize_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);

  int inwidth=weed_get_int_value(in_channel,"width",&error);
  int inheight=weed_get_int_value(in_channel,"height",&error);

  int outwidth=weed_get_int_value(out_channel,"width",&error);
  int outheight=weed_get_int_value(out_channel,"height",&error);

  GdkPixbuf *in_pixbuf=pl_channel_to_pixbuf(in_channel);
  GdkPixbuf *out_pixbuf;

  int up_interp=GDK_INTERP_HYPER;
  int down_interp=GDK_INTERP_BILINEAR;

  if (outwidth>inwidth||outheight>inheight) {
    out_pixbuf=gdk_pixbuf_scale_simple(in_pixbuf,outwidth,outheight,up_interp);
  } else {
    out_pixbuf=gdk_pixbuf_scale_simple(in_pixbuf,outwidth,outheight,down_interp);
  }

  g_object_unref(in_pixbuf);

  pl_pixbuf_to_channel(out_channel,out_pixbuf);
  g_object_unref(out_pixbuf);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_YUV888,WEED_PALETTE_YUVA8888,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_SIZE_CAN_VARY,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("gdk_fast_resize","salsaman",1,WEED_FILTER_IS_CONVERTER,NULL,&resize_process,NULL,
                               in_chantmpls,out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }
  return plugin_info;
}

