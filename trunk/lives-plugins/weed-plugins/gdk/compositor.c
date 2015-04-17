// simple_blend.c
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

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]= {131,110,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#include "../weed-utils-code.c" // optional
#include "../weed-plugin-utils.c" // optional


/////////////////////////////////////////////////////////////
// gdk stuff for resizing


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



static GdkPixbuf *pl_data_to_pixbuf(int palette, int width, int height, int irowstride, guchar *pixel_data) {
  GdkPixbuf *pixbuf;
  int rowstride,orowstride;
  gboolean cheat=FALSE;
  gint n_channels;
  guchar *pixels,*end;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    if (irowstride==pl_gdk_rowstride_value(width*3)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat=TRUE;
    } else pixbuf=gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels=3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32: // TODO - change to RGBA ??
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


///////////////////////////////////////////////////////



static void paint_pixel(unsigned char *dst, int dof, unsigned char *src, int sof, double alpha) {
  double invalpha;
  dst[dof]=dst[dof]*((invalpha=1.-alpha))+src[sof]*alpha;
  dst[dof+1]=dst[dof+1]*invalpha+src[sof+1]*alpha;
  dst[dof+2]=dst[dof+2]*invalpha+src[sof+2]*alpha;
}


int compositor_process(weed_plant_t *inst, weed_timecode_t timecode) {
  int error;
  weed_plant_t **in_channels=NULL;
  int num_in_channels=0;
  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  unsigned char *src;
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error),*dst2;

  int owidth=weed_get_int_value(out_channel,"width",&error),owidth3=owidth*3;
  int oheight=weed_get_int_value(out_channel,"height",&error);

  int in_width,in_height,out_width,out_height;

  int irowstride;
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  //int palette=weed_get_int_value(out_channel,"current_palette",&error);

  weed_plant_t **in_params;

  int numscalex,numscaley,numoffsx,numoffsy,numalpha;
  int *bgcol;
  double *offsx,*offsy,*scalex,*scaley,*alpha;

  double myoffsx,myoffsy,myscalex,myscaley,myalpha;

  unsigned char *end;

  register int x,y,z;

  GdkPixbuf *in_pixbuf,*out_pixbuf;

  int up_interp=GDK_INTERP_HYPER;
  int down_interp=GDK_INTERP_BILINEAR;



  if (weed_plant_has_leaf(inst,"in_channels")) {
    num_in_channels=weed_leaf_num_elements(inst,"in_channels");
    in_channels=weed_get_plantptr_array(inst,"in_channels",&error);
  }

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  numoffsx=weed_leaf_num_elements(in_params[0],"value");
  offsx=weed_get_double_array(in_params[0],"value",&error);

  numoffsy=weed_leaf_num_elements(in_params[1],"value");
  offsy=weed_get_double_array(in_params[1],"value",&error);

  numscalex=weed_leaf_num_elements(in_params[2],"value");
  scalex=weed_get_double_array(in_params[2],"value",&error);

  numscaley=weed_leaf_num_elements(in_params[3],"value");
  scaley=weed_get_double_array(in_params[3],"value",&error);

  numalpha=weed_leaf_num_elements(in_params[4],"value");
  alpha=weed_get_double_array(in_params[4],"value",&error);

  bgcol=weed_get_int_array(in_params[5],"value",&error);

  // set out frame to bgcol

  end=dst+oheight*orowstride;
  for (dst2=dst; dst2<end; dst2+=orowstride) {
    for (x=0; x<owidth3; x+=3) {
      dst2[x]=bgcol[0];
      dst2[x+1]=bgcol[1];
      dst2[x+2]=bgcol[2];
    }
  }

  weed_free(bgcol);

  // add overlays in reverse order

  for (z=num_in_channels-1; z>=0; z--) {
    // check if host disabled this channel : this is allowed as we have set "max_repeats"
    if (weed_plant_has_leaf(in_channels[z],"disabled")&&weed_get_boolean_value(in_channels[z],"disabled",&error)==WEED_TRUE) continue;

    if (z<numoffsx) myoffsx=(int)(offsx[z]*(double)owidth);
    else myoffsx=0;
    if (z<numoffsy) myoffsy=(int)(offsy[z]*(double)oheight);
    else myoffsy=0;
    if (z<numscalex) myscalex=scalex[z];
    else myscalex=1.;
    if (z<numscaley) myscaley=scaley[z];
    else myscaley=1.;
    if (z<numalpha) myalpha=alpha[z];
    else myalpha=1.;

    out_width=(owidth*myscalex+.5);
    out_height=(oheight*myscaley+.5);

    if (out_width*out_height>0) {
      in_width=weed_get_int_value(in_channels[z],"width",&error);
      in_height=weed_get_int_value(in_channels[z],"height",&error);

      src=weed_get_voidptr_value(in_channels[z],"pixel_data",&error);
      irowstride=weed_get_int_value(in_channels[z],"rowstrides",&error);

      // scale image to new size

      in_pixbuf=pl_data_to_pixbuf(WEED_PALETTE_RGB24, in_width, in_height, irowstride, (guchar *)src);

      if (out_width>in_width||out_height>in_height) {
        out_pixbuf=gdk_pixbuf_scale_simple(in_pixbuf,out_width,out_height,up_interp);
      } else {
        out_pixbuf=gdk_pixbuf_scale_simple(in_pixbuf,out_width,out_height,down_interp);
      }

      g_object_unref(in_pixbuf);

      src=gdk_pixbuf_get_pixels(out_pixbuf);

      out_width=gdk_pixbuf_get_width(out_pixbuf);
      out_height=gdk_pixbuf_get_height(out_pixbuf);
      irowstride=gdk_pixbuf_get_rowstride(out_pixbuf);


      for (y=myoffsy; y<oheight&&y<myoffsy+out_height; y++) {
        for (x=myoffsx; x<owidth&&x<myoffsx+out_width; x++) {
          paint_pixel(dst,y*orowstride+x*3,src,(y-myoffsy)*irowstride+(x-myoffsx)*3,myalpha);
        }
      }
      g_object_unref(out_pixbuf);
    }
  }

  weed_free(offsx);
  weed_free(offsy);
  weed_free(scalex);
  weed_free(scaley);
  weed_free(alpha);

  if (num_in_channels>0) weed_free(in_channels);
  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_RGB24,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",WEED_CHANNEL_SIZE_CAN_VARY,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_SIZE_CAN_VARY,palette_list),NULL};

    weed_plant_t *in_params[]= {weed_float_init("xoffs","_X offset",0.,0.,1.),weed_float_init("yoffs","_Y offset",0.,0.,1.),weed_float_init("scalex","Scale _width",1.,0.,1.),weed_float_init("scaley","Scale _height",1.,0.,1.),weed_float_init("alpha","_Alpha",1.0,0.0,1.0),weed_colRGBi_init("bgcol","_Background color",0,0,0),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("compositor","salsaman",1,0,NULL,&compositor_process,NULL,in_chantmpls,out_chantmpls,
                               in_params,NULL);

    weed_plant_t *gui=weed_filter_class_get_gui(filter_class);

    // define RFX layout
    char *rfx_strings[]= {"layout|p0|p1|","layout|p2|p3|","layout|p4|","layout|hseparator|","layout|p5|","special|framedraw|multrect|0|1|2|3|4|"};

    int api_used=weed_get_api_version(plugin_info);
    // set 0 to infinite repeats
    weed_set_int_value(in_chantmpls[0],"max_repeats",0);
    weed_set_boolean_value(in_chantmpls[0],"optional",WEED_TRUE);

    // this is necessary for the host
    if (api_used==100) {
      weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS);
      weed_set_int_value(in_params[1],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS);
      weed_set_int_value(in_params[2],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS);
      weed_set_int_value(in_params[3],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS);
      weed_set_int_value(in_params[4],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS);
    } else if (api_used>=110) {
      // use WEED_PARAMETER_ELEMENT_PER_CHANNEL from spec 110
      weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS|WEED_PARAMETER_ELEMENT_PER_CHANNEL);
      weed_set_int_value(in_params[1],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS|WEED_PARAMETER_ELEMENT_PER_CHANNEL);
      weed_set_int_value(in_params[2],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS|WEED_PARAMETER_ELEMENT_PER_CHANNEL);
      weed_set_int_value(in_params[3],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS|WEED_PARAMETER_ELEMENT_PER_CHANNEL);
      weed_set_int_value(in_params[4],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS|WEED_PARAMETER_ELEMENT_PER_CHANNEL);
    }

    // set default value for elements added by the host
    weed_set_double_value(in_params[0],"new_default",0.);
    weed_set_double_value(in_params[1],"new_default",0.);
    weed_set_double_value(in_params[2],"new_default",1.);
    weed_set_double_value(in_params[3],"new_default",1.);
    weed_set_double_value(in_params[4],"new_default",1.);

    // set RFX layout
    weed_set_string_value(gui,"layout_scheme","RFX");
    weed_set_string_value(gui,"rfx_delim","|");
    weed_set_string_array(gui,"rfx_strings",6,rfx_strings);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }

  return plugin_info;
}
