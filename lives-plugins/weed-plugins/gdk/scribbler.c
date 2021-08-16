// scribbler.c
// weed plugin
// (c) A. Penkov (salsaman) 2010 - 2019
// cloned and modified from livetext.c (author G. Finch aka salsaman)
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_CONVERSIONS
#define NEED_FONT_UTILS

#include <pango/pango-font.h>

#define NEED_PANGO_COMPAT

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#include <weed/weed-compat.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#include "../../../libweed/weed-compat.h" // optional
#endif

#include "../weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <pango/pangocairo.h>
#include <gdk/gdk.h>

// defines for configure dialog elements
enum DlgControls {
  P_TEXT = 0,
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

typedef struct {
  int fontnum;
  double font_size;
  PangoFontDescription *pfd;
} _sdata;


/////////////////////////////////////////////

static cairo_t *channel_to_cairo(weed_plant_t *channel) {
  // convert a weed channel to cairo
  // the channel shares pixel_data with cairo
  // so it should be copied before the cairo is destroyed

  // WEED_LEAF_WIDTH,WEED_LEAF_ROWSTRIDES and WEED_LEAF_CURRENT_PALETTE of channel may all change

  int irowstride, orowstride;
  int width, widthx;
  int height;
  int pal;
  int error;

  int i;

  guchar *src, *dst, *pixel_data;

  cairo_surface_t *surf;
  cairo_t *cairo;
  cairo_format_t cform = CAIRO_FORMAT_ARGB32;

  width = weed_get_int_value(channel, WEED_LEAF_WIDTH, &error);
  height = weed_get_int_value(channel, WEED_LEAF_HEIGHT, &error);
  pal = weed_get_int_value(channel, WEED_LEAF_CURRENT_PALETTE, &error);
  irowstride = weed_get_int_value(channel, WEED_LEAF_ROWSTRIDES, &error);

  widthx = width * 4;

  orowstride = cairo_format_stride_for_width(cform, width);

  src = (guchar *)weed_get_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, &error);

  pixel_data = (guchar *)weed_malloc(height * orowstride);

  if (pixel_data == NULL) return NULL;

  if (irowstride == orowstride) {
    weed_memcpy((void *)pixel_data, (void *)src, irowstride * height);
  } else {
    dst = pixel_data;
    for (i = 0; i < height; i++) {
      weed_memcpy((void *)dst, (void *)src, widthx);
      dst += orowstride;
      src += irowstride;
    }
  }

  // pre-multiply alpha for cairo
  if (weed_get_boolean_value(channel, WEED_LEAF_ALPHA_PREMULTIPLIED, NULL) == WEED_FALSE)
    alpha_premult(pixel_data, widthx, height, orowstride, pal, WEED_FALSE);

  surf = cairo_image_surface_create_for_data(pixel_data, cform, width, height, orowstride);

  if (!surf) {
    weed_free(pixel_data);
    return NULL;
  }

  cairo = cairo_create(surf);
  return cairo;
}


static void cairo_to_channel(cairo_t *cairo, weed_plant_t *channel) {
  // updates a weed_channel from a cairo_t
  cairo_surface_t *surface = cairo_get_target(cairo);
  guchar *osrc, *src, *dst, *pixel_data = (guchar *)weed_channel_get_pixel_data(channel);

  int width = weed_channel_get_width(channel), widthx = width * 4;
  int height = weed_channel_get_height(channel), cheight;
  int irowstride, orowstride = weed_channel_get_stride(channel);

  int i;

  // flush to ensure all writing to the image was done
  cairo_surface_flush(surface);

  osrc = src = cairo_image_surface_get_data(surface);
  irowstride = cairo_image_surface_get_stride(surface);
  cheight = cairo_image_surface_get_height(surface);

  if (cheight < height) height = cheight;

  if (irowstride == orowstride) {
    weed_memcpy((void *)pixel_data, (void *)src, irowstride * height);
  } else {
    dst = pixel_data;
    for (i = 0; i < height; i++) {
      weed_memcpy((void *)dst, (void *)src, widthx);
      dst += orowstride;
      src += irowstride;
    }
  }

  if (weed_get_boolean_value(channel, WEED_LEAF_ALPHA_PREMULTIPLIED, NULL) == WEED_FALSE) {
    int pal = weed_channel_get_palette(channel);
    // un-premultiply the alpha
    alpha_premult(pixel_data, widthx, height, orowstride, pal, TRUE);
  }
  cairo_surface_flush(surface);
  weed_free(osrc);
  cairo_surface_destroy(surface);
}


static const char **fonts_available = NULL;
static int num_fonts_available = 0;


static weed_error_t scribbler_deinit(weed_plant_t *inst) {
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    if (sdata->pfd) pango_font_description_free(sdata->pfd);
    weed_free(sdata);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


static weed_error_t scribbler_init(weed_plant_t *inst) {
  weed_plant_t **in_params;
  weed_plant_t *pgui;
  int mode;
  weed_plant_t *filter = weed_instance_get_filter(inst);
  int version = weed_filter_get_version(filter);

  if (version > 1) {
    _sdata *sdata = (_sdata *)weed_calloc(1, sizeof(_sdata));
    if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;
    weed_set_voidptr_value(inst, "plugin_internal", sdata);
  }

  in_params = weed_get_in_params(inst, NULL);
  mode = weed_param_get_value_int(in_params[P_MODE]);

  pgui = weed_param_get_gui(in_params[P_BGALPHA]);
  if (mode == 0) weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_FALSE);

  pgui = weed_param_get_gui(in_params[P_BACKGROUND]);
  if (mode == 0) weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_FALSE);

  pgui = weed_param_get_gui(in_params[P_FGALPHA]);
  if (mode == 2) weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_FALSE);

  pgui = weed_param_get_gui(in_params[P_FOREGROUND]);
  if (mode == 2) weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_FALSE);

  weed_free(in_params);

  return WEED_SUCCESS;
}


static void getxypos(PangoLayout *layout, double *px, double *py, int width, int height, int cent, double *pw, double *ph) {
  double d;
  PangoRectangle logic;
  pango_layout_get_pixel_extents(layout, NULL, &logic);

  if (cent) {
    d = (double)logic.width / 2. + logic.x;
    d = (width >> 1) - d;
  } else d = 0.0;
  if (px) *px = d;

  if (pw) *pw = (double)(logic.width - logic.x);
  if (ph) *ph = (double)(logic.height - logic.y);

  d = (double)logic.height;
  d = height - d;
  if (py) *py = d;
}


static void fill_bckg(cairo_t *cr, double x, double y, double dx, double dy) {
  cairo_rectangle(cr, x, y, dx, dy);
  cairo_fill(cr);
}


static int font_compare(const void *s1, const void *s2) {
  size_t sz1 = strlen(s1);
  size_t sz2 = strlen(s2);
  if (sz1 == sz2) {
    char *u1 = g_utf8_casefold(s1, sz1);
    char *u2 = g_utf8_casefold(s2, sz1);
    int ret = strcmp(u1, u2);
    g_free(u1); g_free(u2);
    return ret;
  }
  return 1;
}


static weed_error_t scribbler_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  _sdata *sdata = NULL;
  PangoFontDescription *pfd = NULL;
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0); // maye be NULL
  weed_plant_t *filter;
  rgb_t *fg, *bg;
  cairo_t *cairo;

  double f_alpha, b_alpha, dwidth, dheight, font_size, top;
  double sx = 1., sy = 1.;

  char *text, *xtext, *fontstr;
  char *family;

  size_t tlen = 0;

  int cent, rise;
  int fontnum = 0;
  int mode;
  int version;
  int width = weed_channel_get_width(out_channel);
  int height = weed_channel_get_height(out_channel);
  int pal = weed_channel_get_palette(out_channel);
  int nwidth = width, nheight = height;
  int i, j = 0;
  int nlcnt = 0;

  xtext = text = weed_param_get_value_string(in_params[P_TEXT]);
  font_size = weed_param_get_value_double(in_params[P_FONTSIZE]);

  filter = weed_instance_get_filter(inst);
  version = weed_filter_get_version(filter);
  if (!version) version = 1;

  if (version == 1) fontnum = weed_param_get_value_int(in_params[P_FONT]);
  else {
    sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
    fontstr = weed_param_get_value_string(in_params[P_FONT]);
    pfd = pango_font_description_from_string(fontstr);
    pango_font_description_set_size(pfd, font_size_to_pango_size(font_size));

    if (sdata->pfd && font_size != sdata->font_size)
      pango_font_description_set_size(sdata->pfd, font_size_to_pango_size(font_size));

    if (!sdata->pfd || !pango_font_description_equal(pfd, sdata->pfd)) {
      if (sdata->pfd) {
        pango_font_description_free(sdata->pfd);
        sdata->pfd = NULL;
      }
      weed_parse_font_string(fontstr, &family, NULL, NULL, NULL, NULL);
      for (i = 0; i < num_fonts_available; ++i) {
        if (!font_compare((const void *)fonts_available[i], (const void *)family)) {
          sdata->fontnum = i;
          break;
        }
      }
      weed_free(family);
      if (i == num_fonts_available) {
        sdata->fontnum = -1;
        pango_font_description_free(pfd);
      } else sdata->pfd = pfd;
    }
    weed_free(fontstr);
    sdata->font_size = font_size;
    fontnum = sdata->fontnum;
  }

  mode = weed_param_get_value_int(in_params[P_MODE]);

  fg = (rgb_t *)weed_param_get_array_int(in_params[P_FOREGROUND], NULL);
  bg = (rgb_t *)weed_param_get_array_int(in_params[P_BACKGROUND], NULL);

  f_alpha = weed_param_get_value_double(in_params[P_FGALPHA]);
  b_alpha = weed_param_get_value_double(in_params[P_BGALPHA]);

  cent = weed_param_get_value_boolean(in_params[P_CENTER]);
  rise = weed_param_get_value_boolean(in_params[P_RISE]);
  top = weed_param_get_value_double(in_params[P_TOP]);

  weed_free(in_params); // must weed free because we got an array

  for (i = 0; text[i]; i++) {
    if (text[i] == '\n') {
      if (!text[i + 1] || text[i + 1] == '\n') nlcnt++;
    }
  }
  tlen = i;
  if (nlcnt) {
    tlen += nlcnt;
    xtext = weed_malloc(tlen + 1);
    for (i = 0; text[i]; i++) {
      xtext[j++] = text[i];
      if (text[i] == '\n') {
        if (!text[i + 1] || text[i + 1] == '\n') xtext[j++] = ' ';
      }
    }
    xtext[j] = 0;
  }

  if (!in_channel || (in_channel == out_channel))
    cairo = channel_to_cairo(out_channel);
  else
    cairo = channel_to_cairo(in_channel);

  // create a cairo surface using natural_size
  if (weed_plant_has_leaf(in_channel, WEED_LEAF_NATURAL_SIZE)) {
    int *nsize = weed_get_int_array(in_channel, WEED_LEAF_NATURAL_SIZE, NULL);
    nwidth = nsize[0];
    nheight = nsize[1];
    sx = (double)width / (double)nwidth;
    sy = (double)height / (double)nheight;
    cairo_scale(cairo, sx, sy);
  }

  if (cairo) {
    if (xtext && *xtext) {
      // do cairo and pango things
      PangoLayout *layout = pango_cairo_create_layout(cairo);
      if (layout) {
        double x_pos, y_pos;
        double x_text, y_text;

        if (version == 1) {
          pfd = pango_font_description_new();
          if ((num_fonts_available) && (fontnum >= 0) && (fontnum < num_fonts_available) && (fonts_available[fontnum])) {
            pango_font_description_set_family(pfd, fonts_available[fontnum]);
          }
        } else {
          if (sdata->pfd) pfd = sdata->pfd;
          else pfd = pango_font_description_new();
        }

        pango_font_description_set_size(pfd, font_size_to_pango_size(font_size));
        pango_layout_set_font_description(layout, pfd);

        pango_layout_set_text(layout, xtext, tlen);
        getxypos(layout, &x_pos, &y_pos, nwidth, nheight, cent, &dwidth, &dheight);

        if (!rise) y_pos = nheight * top;

        x_text = x_pos;
        y_text = y_pos;
        if (cent) pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
        else pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

        cairo_move_to(cairo, x_text, y_text);

        switch (mode) {
        case 1:
          if (pal == WEED_PALETTE_RGBA32)
            cairo_set_source_rgba(cairo, (double)bg->blue / 255.0, (double)bg->green / 255.0, (double)bg->red / 255.0, b_alpha);
          else
            cairo_set_source_rgba(cairo, (double)bg->red / 255.0, (double)bg->green / 255.0, (double)bg->blue / 255.0, b_alpha);
          fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
          cairo_move_to(cairo, x_text, y_text);
          if (pal == WEED_PALETTE_RGBA32)
            cairo_set_source_rgba(cairo, (double)fg->blue / 255.0, (double)fg->green / 255.0, (double)fg->red / 255.0, f_alpha);
          else
            cairo_set_source_rgba(cairo, (double)fg->red / 255.0, (double)fg->green / 255.0, (double)fg->blue / 255.0, f_alpha);
          pango_layout_set_text(layout, text, -1);
          break;
        case 2:
          if (pal == WEED_PALETTE_RGBA32)
            cairo_set_source_rgba(cairo, (double)bg->blue / 255.0, (double)bg->green / 255.0, (double)bg->red / 255.0, b_alpha);
          else
            cairo_set_source_rgba(cairo, (double)bg->red / 255.0, (double)bg->green / 255.0, (double)bg->blue / 255.0, b_alpha);
          fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
          cairo_move_to(cairo, x_pos, y_pos);
          if (pal == WEED_PALETTE_RGBA32)
            cairo_set_source_rgba(cairo, (double)fg->blue / 255.0, (double)fg->green / 255.0, (double)fg->red / 255.0, f_alpha);
          else
            cairo_set_source_rgba(cairo, (double)fg->red / 255.0, (double)fg->green / 255.0, (double)fg->blue / 255.0, f_alpha);
          pango_layout_set_text(layout, "", -1);
          break;
        case 0:
        default:
          if (pal == WEED_PALETTE_RGBA32)
            cairo_set_source_rgba(cairo, (double)fg->blue / 255.0, (double)fg->green / 255.0, (double)fg->red / 255.0, f_alpha);
          else
            cairo_set_source_rgba(cairo, (double)fg->red / 255.0, (double)fg->green / 255.0, (double)fg->blue / 255.0, f_alpha);
          break;
        }

        pango_cairo_show_layout(cairo, layout);
        g_object_unref(layout);
        if (version == 1 || pfd != sdata->pfd) pango_font_description_free(pfd);
      }
    }

    cairo_to_channel(cairo, out_channel);
    cairo_destroy(cairo);
  }

  if (xtext != text) weed_free(xtext);
  weed_free(text);
  weed_free(fg);
  weed_free(bg);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  const char *def_fonts[] = {"serif", NULL};
  const char *modes[] = {"foreground only", "foreground and background", "background only", NULL};
  char rfxstring0[256];
  char rfxstring1[256];
  char rfxstring2[256];
  char *rfxstrings[3] = {rfxstring0, rfxstring1, rfxstring2};

  int palette_list[3];
  weed_plant_t **clone0, **clone1, **clone2;
  weed_plant_t *in_chantmpls[2];
  weed_plant_t *out_chantmpls[2];
  weed_plant_t *in_params[P_END + 1], *pgui;
  weed_plant_t *filter_class, *gui;
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  PangoContext *ctx;
  int filter_flags = weed_host_supports_premultiplied_alpha(host_info) ? WEED_FILTER_PREF_PREMULTIPLIED_ALPHA : 0;
  int param_flags = 0;
  int version = 2;

  if (is_big_endian()) {
    palette_list[0] = WEED_PALETTE_ARGB32;
    palette_list[1] = palette_list[2] = WEED_PALETTE_END;
  } else {
    palette_list[0] = WEED_PALETTE_RGBA32;
    palette_list[1] = WEED_PALETTE_BGRA32;
    palette_list[2] = WEED_PALETTE_END;
  }

  in_chantmpls[0] = weed_channel_template_init("in channel 0", WEED_CHANNEL_NEEDS_NATURAL_SIZE);
  in_chantmpls[1] = NULL;

  out_chantmpls[0] = weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE);
  out_chantmpls[1] = NULL;

  init_unal();

  // this section contains code
  // for configure fonts available
  num_fonts_available = 0;
  fonts_available = NULL;

  ctx = gdk_pango_context_get();
  if (ctx) {
    PangoFontMap *pfm = pango_context_get_font_map(ctx);
    if (pfm) {
      int num = 0;
      PangoFontFamily **pff = NULL;
      pango_font_map_list_families(pfm, &pff, &num);
      if (num > 0) {
        // we should reserve num+1 for a final NULL pointer
        fonts_available = (const char **)weed_malloc((num + 1) * sizeof(char *));
        if (fonts_available) {
          num_fonts_available = num;
          for (register int i = 0; i < num; ++i) {
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

  in_params[P_TEXT] = weed_text_init("text", "_Text", "");
  in_params[P_MODE] = weed_string_list_init("mode", "Colour _mode", 0, modes);
  param_flags = weed_paramtmpl_get_flags(in_params[P_MODE]);
  param_flags |= WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
  weed_paramtmpl_set_flags(in_params[P_MODE], param_flags);
  gui = weed_paramtmpl_get_gui(in_params[P_MODE]);
  weed_gui_set_flags(gui, WEED_GUI_REINIT_ON_VALUE_CHANGE);

  snprintf(rfxstrings[0], 256, "layout|p%d|", P_TEXT);
  snprintf(rfxstrings[1], 256, "layout|p%d|p%d|", P_FONT, P_FONTSIZE);
  snprintf(rfxstrings[2], 256, "special|fontchooser|%d|%d|", P_FONT, P_FONTSIZE);

  if (version == 1) {
    if (fonts_available)
      in_params[P_FONT] = weed_string_list_init("font", "_Font", 0, fonts_available);
    else
      in_params[P_FONT] = weed_string_list_init("font", "_Font", 0, def_fonts);
  } else in_params[P_FONT] = weed_text_init("fontname", "_Font Name", "");

  in_params[P_FOREGROUND] = weed_colRGBi_init("foreground", "_Foreground", 255, 255, 255);
  in_params[P_BACKGROUND] = weed_colRGBi_init("background", "_Background", 0, 0, 0);
  in_params[P_FGALPHA] = weed_float_init("fr_alpha", "Alpha _Foreground", 1.0, 0.0, 1.0);
  in_params[P_BGALPHA] = weed_float_init("bg_alpha", "Alpha _Background", 1.0, 0.0, 1.0);
  in_params[P_FONTSIZE] = weed_float_init("fontsize", "_Font Size", 32.0, 10.0, 128.0);
  in_params[P_CENTER] = weed_switch_init("center", "_Center text", WEED_TRUE);
  in_params[P_RISE] = weed_switch_init("rising", "_Rising text", WEED_TRUE);
  in_params[P_TOP] = weed_float_init("top", "_Top", 0.0, 0.0, 1.0);
  in_params[P_END] = NULL;

  pgui = weed_paramtmpl_get_gui(in_params[P_TEXT]);
  weed_set_int_value(pgui, WEED_LEAF_MAXCHARS, 65536);

  pgui = weed_paramtmpl_get_gui(in_params[P_FONTSIZE]);
  weed_set_double_value(pgui, WEED_LEAF_STEP_SIZE, 1.);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 0);

  weed_set_int_value(in_params[P_FGALPHA], WEED_LEAF_COPY_VALUE_TO, P_BGALPHA);

  for (int i = 1; i < 3; i++) {
    if (i == 1) {
      filter_class = weed_filter_class_init("scribbler", "Aleksej Penkov", 1, filter_flags, palette_list,
                                            scribbler_init, scribbler_process, NULL,
                                            in_chantmpls, out_chantmpls, in_params, NULL);
      weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, "salsaman");
    } else {
      clone2 = weed_clone_plants(in_params);
      weed_plant_free(clone2[P_FONT]);
      clone2[P_FONT] = weed_text_init("font", "_Font",  "");
      filter_class = weed_filter_class_init("scribbler", "Aleksej Penkov", 2, filter_flags, palette_list,
                                            scribbler_init, scribbler_process, scribbler_deinit,
                                            (clone0 = weed_clone_plants(in_chantmpls)),
                                            (clone1 = weed_clone_plants(out_chantmpls)),
                                            clone2, NULL);
      weed_free(clone0); weed_free(clone1); weed_free(clone2);

      gui = weed_filter_get_gui(filter_class);
      weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
      weed_set_string_value(gui, "layout_rfx_delim", "|");
      weed_set_string_array(gui, "layout_rfx_strings", 3, (char **)rfxstrings);
      weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, "salsaman");
    }

    weed_plugin_info_add_filter_class(plugin_info, filter_class);

    clone2 = weed_clone_plants(in_params);
    weed_plant_free(clone2[P_FONT]);
    clone2[P_FONT] = weed_text_init("font", "_Font",  "");

    filter_class = weed_filter_class_init("scribbler_generator", "Aleksej Penkov", i, filter_flags, palette_list,
                                          scribbler_init, scribbler_process, i == 1 ? NULL : scribbler_deinit, NULL,
                                          (clone1 = weed_clone_plants(out_chantmpls)), clone2, NULL);
    weed_free(clone1); weed_free(clone2);
    weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, "salsaman");

    if (i > 1) {
      gui = weed_filter_get_gui(filter_class);
      weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
      weed_set_string_value(gui, "layout_rfx_delim", "|");
      weed_set_string_array(gui, "layout_rfx_strings", 3, (char **)rfxstrings);
    }
    weed_plugin_info_add_filter_class(plugin_info, filter_class);
  }
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;


WEED_DESETUP_START {
  // clean up what we reserve for font family names
  if (num_fonts_available && fonts_available) {
    for (int i = 0; i < num_fonts_available; ++i) {
      free((void *)fonts_available[i]);
    }
    weed_free((void *)fonts_available);
  }
  num_fonts_available = 0;
  fonts_available = NULL;
}
WEED_DESETUP_END;
