// gdk_fast_resize.c
// weed plugin - resize using gdk
// (c) G. Finch (salsaman) 2007 - 2012
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

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../../libweed/weed-plugin.h" // optional
#endif

#include "../weed-utils-code.c" // optional
#include "../weed-plugin-utils.c" // optional


/////////////////////////////////////////////////////////////

typedef struct _sdata {
  unsigned char *bgbuf;
  int count;
  int idxno;
  int dir;
  uint32_t fastrand_val;
} sdata;

static inline uint32_t fastrand(struct _sdata *sdata) {
#define rand_a 1073741789L
#define rand_c 32749L

  return ((sdata->fastrand_val= (rand_a*sdata->fastrand_val + rand_c)));
}



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




int videowall_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int video_height,video_width,video_area;
  int palette;
  int error;
  int psize;
  weed_plant_t *in_channel;
  register int i,j;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  sdata=weed_malloc(sizeof(struct _sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  palette=weed_get_int_value(in_channel,"current_palette",&error);
  video_height=weed_get_int_value(in_channel,"height",&error);
  video_width=weed_get_int_value(in_channel,"width",&error);
  video_area=video_width*video_height;


  sdata->bgbuf=weed_malloc((psize=video_area*(palette==WEED_PALETTE_RGB24?3:4)));

  if (sdata->bgbuf == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  // set a black background
  if (palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_BGR24) {
    weed_memset(sdata->bgbuf,0,psize);
  } else if (palette==WEED_PALETTE_RGBA32||palette==WEED_PALETTE_BGRA32) {
    unsigned char *ptr=sdata->bgbuf;
    for (i=0; i<video_height; i++) {
      for (j=0; j<video_width; j++) {
        weed_memset(ptr,0,3);
        weed_memset(ptr+3,255,1);
        ptr+=4;
      }
    }
  }
  if (palette==WEED_PALETTE_YUV888) {
    unsigned char *ptr=sdata->bgbuf;
    for (i=0; i<video_height; i++) {
      for (j=0; j<video_width; j++) {
        weed_memset(ptr,16,1);
        weed_memset(ptr+1,128,2);
        ptr+=3;
      }
    }
  }
  if (palette==WEED_PALETTE_YUVA8888) {
    unsigned char *ptr=sdata->bgbuf;
    for (i=0; i<video_height; i++) {
      for (j=0; j<video_width; j++) {
        weed_memset(ptr,16,1);
        weed_memset(ptr+1,128,2);
        weed_memset(ptr+3,255,1);
        ptr+=4;
      }
    }
  }


  sdata->count=0;
  sdata->fastrand_val=0;
  sdata->dir=0;
  sdata->idxno=-1;

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






int videowall_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  int palette=weed_get_int_value(in_channel,"current_palette",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);

  GdkPixbuf *in_pixbuf=pl_channel_to_pixbuf(in_channel);
  GdkPixbuf *out_pixbuf;

  int down_interp=GDK_INTERP_BILINEAR;

  register int i,j;
  struct _sdata *sdata;

  int psize=4,prow,orow,pwidth,pheight;

  unsigned char *bdst,*dst,*rpix;

  int ofh=0,ofw=0;
  int xwid,xht,mode;

  int row,col,idxno,bdstoffs,rpixoffs;

  int offs_x,offs_y;

  xwid=weed_get_int_value(in_params[0],"value",&error);
  xht=weed_get_int_value(in_params[1],"value",&error);
  mode=weed_get_int_value(in_params[2],"value",&error);

  dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  pwidth=width/xwid;
  pheight=height/xht;

  pwidth=(pwidth>>1)<<1;
  pheight=(pheight>>1)<<1;

  if (pwidth==0||pheight==0) return WEED_NO_ERROR;

  offs_x=(width-pwidth*xwid)>>1;
  offs_y=(height-pheight*xht)>>1;

  out_pixbuf=gdk_pixbuf_scale_simple(in_pixbuf,pwidth,pheight,down_interp);

  g_object_unref(in_pixbuf);

  if (out_pixbuf==NULL) return WEED_NO_ERROR;

  if (palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_BGR24||palette==WEED_PALETTE_YUV888) psize=3;

  switch (mode) {
  case 0:
    // l to r, top to bottom
    idxno=sdata->count;
    break;
  case 1:
    // pseudo-random
    idxno=(fastrand(sdata)>>24)%(xwid*xht);
    break;
  case 2:
    // spiral
    // TODO

    idxno=sdata->idxno;

    if (idxno==-1) {
      idxno=0;
      sdata->dir=0;
      break;
    }

    row=(int)((float)idxno/(float)xwid);
    col=idxno-(row*xwid);

    if (sdata->dir==0) {
      if (col>=xwid-1-row) sdata->dir=1; // time to go down
      // go right
      else idxno++;
    }
    if (sdata->dir==1) {
      if (row>=col-(xwid-xht)) sdata->dir=2; // time to go left
      // go down
      else idxno+=xwid;
    }
    if (sdata->dir==2) {
      if (col<=(xwid-row-1)-(xwid-xht)) {
        sdata->dir=3; // time to go up
        if (row<=col+1) {
          idxno=0;
          sdata->dir=0;
          break;
        }
      }
      // go left
      else idxno--;
    }
    if (sdata->dir==3) {
      if (row<=col+1) {
        sdata->dir=0; // time to go right
        if (col<xwid-1-row) idxno++;
      }
      // go up
      else idxno-=xwid;
    }
    if (idxno==sdata->idxno) {
      idxno=0;
      sdata->dir=0;
    }
    break;
  default:
    idxno=0;

  }

  idxno%=(xwid*xht);

  sdata->idxno=idxno;

  row=(int)((float)idxno/(float)xwid);
  col=idxno-(row*xwid);

  ofh=offs_y+pheight*row;
  ofw=(offs_x+pwidth*col)*psize;

  bdst=sdata->bgbuf+ofh*width*psize+ofw;

  prow=gdk_pixbuf_get_rowstride(out_pixbuf);
  rpix=gdk_pixbuf_get_pixels(out_pixbuf);
  pwidth=gdk_pixbuf_get_width(out_pixbuf);
  pheight=gdk_pixbuf_get_height(out_pixbuf);

  bdstoffs=(width-pwidth)*psize;
  rpixoffs=(prow-pwidth*psize);
  // copy pixel_data to bgbuf

  for (i=0; i<pheight; i++) {
    for (j=0; j<pwidth; j++) {
      weed_memcpy(bdst,rpix,psize);
      bdst+=psize;
      rpix+=psize;
    }
    bdst+=bdstoffs;
    rpix+=rpixoffs;
  }

  g_object_unref(out_pixbuf);


  if (++sdata->count==xwid*xht) sdata->count=0;
  orow=weed_get_int_value(out_channel,"rowstrides",&error);

  if (orow==width*psize) {
    weed_memcpy(dst,sdata->bgbuf,width*psize*height);
  } else {
    for (i=0; i<height; i++) {
      weed_memcpy(dst,sdata->bgbuf+i*width*psize,width*psize);
      dst+=orow;
    }
  }

  weed_free(in_params);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    const char *modes[]= {"Scanner","Random","Spiral",NULL};
    //const char *modes[]={"Scanner","Random","Spiral",NULL};
    int palette_list[]= {WEED_PALETTE_RGB24,WEED_PALETTE_BGR24,WEED_PALETTE_YUV888,WEED_PALETTE_YUVA8888,WEED_PALETTE_BGRA32,WEED_PALETTE_RGBA32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE|WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE,palette_list),NULL};
    weed_plant_t *in_params[]= {weed_integer_init("r","Number of _rows",3,1,256),weed_integer_init("c","Number of _Columns",3,1,256),weed_string_list_init("m","Stepping Mode",0,modes),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("videowall","salsaman",1,0,&videowall_init,&videowall_process,&videowall_deinit,
                               in_chantmpls,out_chantmpls,in_params,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }
  return plugin_info;
}

