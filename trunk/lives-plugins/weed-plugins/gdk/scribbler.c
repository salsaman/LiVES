// scribbler.c
// weed plugin
// (c) A. Penkov (salsaman) 2010
// cloned and modified from livetext.c (author G. Finch aka salsaman)
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-palettes.h"
#include "weed/weed-effects.h"
#include "weed/weed-plugin.h"
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=2; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed-utils.h" // optional
#include "weed/weed-plugin-utils.h" // optional
#else
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

/////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>

#include <gdk/gdk.h>
#include <pango/pangocairo.h>

// defines for configure dialog elements
enum DlgControls {
  P_TEXT=0,
  P_MODE,
  P_FONT,
  P_FOREGROUND,
  P_BACKGROUND,
  P_FGALPHA,
  P_BGALPHA,
  P_FONTSIZE,
  P_CENTER,
  P_RISE,
  P_TOP,
  P_END
};

typedef struct {
  int red;
  int green;
  int blue;
} rgb_t;


/////////////////////////////////////////////


static GdkPixbuf *pl_channel_to_pixbuf (weed_plant_t *channel);
static gboolean pl_pixbuf_to_channel(weed_plant_t *channel, GdkPixbuf *pixbuf);
static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height, guchar *buf);

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

static char **fonts_available = NULL;
static int num_fonts_available = 0;

static weed_plant_t *weed_parameter_get_gui(weed_plant_t *param) {
  int error;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);
  return weed_parameter_template_get_gui(ptmpl);
}


int scribbler_init(weed_plant_t *inst) {
  int error;

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t *pgui;
  int mode=weed_get_int_value(in_params[P_MODE],"value",&error);

  pgui = weed_parameter_get_gui(in_params[P_BGALPHA]);
  if (mode==0) weed_set_boolean_value(pgui, "hidden", WEED_TRUE);
  else weed_set_boolean_value(pgui, "hidden", WEED_FALSE);

  pgui = weed_parameter_get_gui(in_params[P_BACKGROUND]);
  if (mode==0) weed_set_boolean_value(pgui, "hidden", WEED_TRUE);
  else weed_set_boolean_value(pgui, "hidden", WEED_FALSE);

  pgui = weed_parameter_get_gui(in_params[P_FGALPHA]);
  if (mode==2) weed_set_boolean_value(pgui, "hidden", WEED_TRUE);
  else weed_set_boolean_value(pgui, "hidden", WEED_FALSE);

  pgui = weed_parameter_get_gui(in_params[P_FOREGROUND]);
  if (mode==2) weed_set_boolean_value(pgui, "hidden", WEED_TRUE);
  else weed_set_boolean_value(pgui, "hidden", WEED_FALSE);

  weed_free(in_params);

  return WEED_NO_ERROR;
}

static void getxypos(PangoLayout *layout, double *px, double *py, int width, int height, int cent, double *pw, double *ph)
{
  int w_, h_;
  double d;
  pango_layout_get_size(layout, &w_, &h_);
  if(pw)
    *pw = ((double)w_)/PANGO_SCALE;
  if(ph)
    *ph = ((double)h_)/PANGO_SCALE;

  if(cent) {
    d = ((double)w_)/PANGO_SCALE;
    d /= 2.0;
    d = (width>>1) - d;
  }
  else
    d = 0.0;
  if(px)
    *px = d;
  
  d = ((double)h_)/PANGO_SCALE;
  d = height - d;
  if(py)
    *py = d;
}

static void fill_bckg(cairo_t *cr, double x, double y, double dx, double dy) {
  cairo_rectangle(cr, x, y, dx, dy);
  cairo_fill(cr);
}

//
//
// now text is drawn with pixbuf/pango
//
//

int scribbler_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  char *text;

  rgb_t *fg,*bg;

  int cent,rise;
  int alpha_threshold = 0;
  int fontnum;
  int error,mode;

  double f_alpha, b_alpha;
  double dwidth, dheight;
  double font_size, top;

  GdkPixbuf *pixbuf = NULL;
  GdkPixbuf *pixbuf_new = NULL;

  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  weed_plant_t *in_channel=NULL;

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  int width=weed_get_int_value(out_channel,"width",&error);
  int height=weed_get_int_value(out_channel,"height",&error);

  int palette=weed_get_int_value(out_channel,"current_palette",&error);

  if (weed_plant_has_leaf(inst,"in_channels")) {
    in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  }

  text=weed_get_string_value(in_params[P_TEXT],"value",&error);
  mode=weed_get_int_value(in_params[P_MODE],"value",&error);
  fontnum=weed_get_int_value(in_params[P_FONT],"value",&error);
  
  fg=(rgb_t *)weed_get_int_array(in_params[P_FOREGROUND],"value",&error);
  bg=(rgb_t *)weed_get_int_array(in_params[P_BACKGROUND],"value",&error);

  f_alpha=weed_get_double_value(in_params[P_FGALPHA],"value",&error);
  b_alpha=weed_get_double_value(in_params[P_BGALPHA],"value",&error);
  font_size=weed_get_double_value(in_params[P_FONTSIZE],"value",&error);

  cent=weed_get_boolean_value(in_params[P_CENTER],"value",&error);
  rise=weed_get_boolean_value(in_params[P_RISE],"value",&error);
  top=weed_get_double_value(in_params[P_TOP],"value",&error);


  if (palette==WEED_PALETTE_BGR24/*||palette==WEED_PALETTE_BGRA32*/) {
    int tmp=fg->red;
    fg->red=fg->blue;
    fg->blue=tmp;

    tmp=bg->red;
    bg->red=bg->blue;
    bg->blue=tmp;
  }


  weed_free(in_params); // must weed free because we got an array

  if (in_channel!=out_channel&&in_channel!=NULL) {
    // if not inplace, copy in pixel_data to out pixel_data
    void *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
    void *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
    int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
    int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
    if (irowstride==orowstride&&irowstride==width*3) {
      weed_memcpy(dst,src,width*3*height);
    }
    else {
      register int i;
      for (i=0;i<height;i++) {
	weed_memcpy(dst,src,width*3);
	dst+=orowstride;
	src+=irowstride;
      }
    }
  }

  // THINGS TO TO WITH TEXTS AND PANGO
  if((!in_channel) || (in_channel == out_channel))
    pixbuf = pl_channel_to_pixbuf(out_channel);
  else
    pixbuf = pl_channel_to_pixbuf(in_channel);

  if(pixbuf) {
    // do cairo and pango things
    GdkPixmap *pixmap = NULL;
    gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, NULL, alpha_threshold);
    if(pixmap) {
      cairo_t *cairo;
      cairo = gdk_cairo_create(pixmap);
      if(cairo) {
        PangoLayout *layout;
        layout = pango_cairo_create_layout(cairo);
        if(layout) { 
          PangoFontDescription *font;
          double x_pos, y_pos;
          double x_text, y_text;
          gboolean result;

          font = pango_font_description_new();
          if((num_fonts_available) && (fontnum >= 0) && (fontnum < num_fonts_available) && (fonts_available[fontnum]))
            pango_font_description_set_family(font, fonts_available[fontnum]);

          pango_font_description_set_size(font, font_size*PANGO_SCALE);

          pango_layout_set_font_description(layout, font);
          pango_layout_set_text(layout, text, -1);
          getxypos(layout, &x_pos, &y_pos, width, height, cent, &dwidth, &dheight);

          if(!rise)
            y_pos = y_text = height*top;

          x_text = x_pos;
          y_text = y_pos;
          if (cent) pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	  else pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

          cairo_move_to(cairo, x_text, y_text);

          switch(mode) {
          case 1:
                 cairo_set_source_rgba(cairo,bg->red/255.0, bg->green/255.0, bg->blue/255.0, b_alpha);
                 fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
                 cairo_move_to(cairo, x_text, y_text);
                 cairo_set_source_rgba(cairo,fg->red/255.0, fg->green/255.0, fg->blue/255.0, f_alpha);
                 pango_layout_set_text(layout, text, -1);
                 break;
          case 2:
                 cairo_set_source_rgba(cairo,bg->red/255.0, bg->green/255.0, bg->blue/255.0, b_alpha);
                 fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
                 cairo_move_to(cairo, x_pos, y_pos);
                 cairo_set_source_rgba(cairo,fg->red/255.0, fg->green/255.0, fg->blue/255.0, f_alpha);
                 pango_layout_set_text(layout, "", -1);
                 break;
          case 0:
          default:
                 cairo_set_source_rgba(cairo,fg->red/255.0, fg->green/255.0, fg->blue/255.0, f_alpha);
                 break;
          }

          pango_cairo_show_layout(cairo, layout);

          // and finally convert backwards
          pixbuf_new = gdk_pixbuf_get_from_drawable(pixbuf, pixmap, NULL,\
              0, 0,\
              0, 0,\
              -1, -1);
          result = pl_pixbuf_to_channel(out_channel, pixbuf_new);
          g_object_unref(pixbuf_new);
          g_object_unref(layout);
          pango_font_description_free(font);
        }
        cairo_destroy(cairo);
      }
      g_object_unref(pixmap);
    }
  }

  weed_free(text);
  weed_free(fg);
  weed_free(bg);

  return WEED_NO_ERROR;
}


inline static int font_compare(const void *p1, const void *p2) {  
  const char *s1 = (const char *)(*(char **)p1);
  const char *s2 = (const char *)(*(char **)p2);
  return(strcasecmp(s1, s2));
}

weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  static char *def_fonts[] = {"serif", NULL};
  if (plugin_info!=NULL) {
    char *modes[]={"foreground only","foreground and background","background only",NULL};
    // removed palettes with alpha channel
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    weed_plant_t *in_params[P_END+1],*pgui;
    weed_plant_t *filter_class;
    PangoContext *ctx;
    int flags=0,error;

    // this section contains code
    // for configure fonts available
    num_fonts_available = 0;
    fonts_available = NULL;

    ctx = gdk_pango_context_get();
    if(ctx) {
      PangoFontMap *pfm = pango_context_get_font_map(ctx);
      if(pfm) {
        int num = 0;
        PangoFontFamily **pff = NULL;
        pango_font_map_list_families(pfm, &pff, &num);
        if(num > 0) {
          // we should reserve num+1 for a final NULL pointer
          fonts_available = (char **)weed_malloc((num+1)*sizeof(char *));
          if(fonts_available) {
            register int i;
            num_fonts_available = num;
            for(i = 0; i < num; ++i) {
              fonts_available[i] = strdup(pango_font_family_get_name(pff[i]));
            }
            // don't forget this thing
            fonts_available[num] = NULL;
            // also we sort fonts in alphabetical order
            qsort(fonts_available, num, sizeof(char *), font_compare);
          }
        }
        g_free(pff);
      } 
      g_object_unref(ctx);
    }


    in_params[P_TEXT]=weed_text_init("text","_Text","");
    in_params[P_MODE]=weed_string_list_init("mode","Colour _mode",0,modes);
    if (weed_plant_has_leaf(in_params[P_MODE],"flags")) 
      flags=weed_get_int_value(in_params[P_MODE],"flags",&error);
    flags|=WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
    weed_set_int_value(in_params[P_MODE],"flags",flags);

    if(fonts_available)
      in_params[P_FONT]=weed_string_list_init("font","_Font",0,fonts_available);
    else
      in_params[P_FONT]=weed_string_list_init("font","_Font",0, def_fonts);
    in_params[P_FOREGROUND]=weed_colRGBi_init("foreground","_Foreground",255,255,255);
    in_params[P_BACKGROUND]=weed_colRGBi_init("background","_Background",0,0,0);
    in_params[P_FGALPHA]=weed_float_init("fr_alpha","_Alpha _Foreground",1.0,0.0,1.0);
    in_params[P_BGALPHA]=weed_float_init("bg_alpha","_Alpha _Background",1.0,0.0,1.0);
    in_params[P_FONTSIZE]=weed_float_init("fontsize","_Font Size",20.0,10.0,128.0);
    in_params[P_CENTER]=weed_switch_init("center","_Center text",1);
    in_params[P_RISE]=weed_switch_init("rising","_Rising text",1);
    in_params[P_TOP]=weed_float_init("top","_Top",0.0,0.0,1.0);
    in_params[P_END]=NULL;
    
    pgui=weed_parameter_template_get_gui(in_params[P_TEXT]);
    weed_set_int_value(pgui,"maxchars",65536);

    pgui=weed_parameter_template_get_gui(in_params[P_FGALPHA]);
    weed_set_int_value(pgui,"copy_value_to",P_BGALPHA);

    filter_class=weed_filter_class_init("scribbler","Aleksej Penkov",1,0,&scribbler_init,&scribbler_process,NULL,in_chantmpls,out_chantmpls,in_params,NULL);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    filter_class=weed_filter_class_init("scribbler_generator","Aleksej Penkov",1,0,&scribbler_init,&scribbler_process,NULL,NULL,weed_clone_plants(out_chantmpls),weed_clone_plants(in_params),NULL);
    
    weed_plugin_info_add_filter_class (plugin_info,filter_class);
    weed_set_double_value(filter_class,"target_fps",25.); // set reasonable default fps

    weed_set_int_value(plugin_info,"version",package_version);

  }

  return plugin_info;
}



void weed_desetup(void) {
  // clean up what we reserve for font family names
  if(num_fonts_available && fonts_available) {
    int i;
    for(i = 0; i < num_fonts_available; ++i) {
      free((void *)fonts_available[i]);
    }
    weed_free((void *)fonts_available);
  }    
  num_fonts_available = 0;
  fonts_available = NULL;
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
  case WEED_PALETTE_YUV888:
    if (irowstride==pl_gdk_rowstride_value(width*3)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat=TRUE;
    }
    else pixbuf=gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels=3;
    break;
//  case WEED_PALETTE_RGBA32:
//  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32:
  case WEED_PALETTE_YUVA8888:
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

  if(dst == in_pixel_data)
    return TRUE;

  if (rowstride==pl_gdk_last_rowstride_value(width,n_channels)&&rowstride==out_rowstride) {
    weed_memcpy(dst,in_pixel_data,rowstride*height);
    return FALSE;
  }

  for (i=0;i<height;i++) {
    if (i==height-1) rowstride=pl_gdk_last_rowstride_value(width,n_channels);
    weed_memcpy(dst,in_pixel_data,rowstride);
    in_pixel_data+=rowstride;
    dst+=out_rowstride;
  }

  return FALSE;
}

static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height, guchar *buf) {
  // we can cheat if our buffer is correctly sized
  int channels=has_alpha?4:3;
  int rowstride=pl_gdk_rowstride_value(width*channels);
  return gdk_pixbuf_new_from_data (buf, colorspace, has_alpha, bits_per_sample, width, height, rowstride, plugin_free_buffer, NULL);
}

