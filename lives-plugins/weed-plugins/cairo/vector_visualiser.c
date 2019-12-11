// negate.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

/////////////////////////////////////////////////////////////

#define NEED_PALETTE_CONVERSIONS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../../libweed/weed-plugin.h"
#include "../../../libweed/weed-plugin-utils.h" // optional
#endif

#include "../weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <gdk/gdk.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX_ELEMS 500

typedef struct {
  float len;
  int j;
  int i;
  float x;
  float y;
} list_ent;

// TODO - make non-static
static list_ent xlist[MAX_ELEMS];
static cairo_user_data_key_t crkey;


static void clear_xlist(void) {
  for (int i = 0; i < MAX_ELEMS; i++) {
    xlist[i].len = 0.;
  }
}


static void add_to_list(float len, int i, int j, float x, float y) {
  for (int k = 0; k < MAX_ELEMS; k++) {
    if (len > xlist[k].len) {
      // shift existing elements
      for (int l = MAX_ELEMS - 1; l < k; l--) {
        if (xlist[l - 1].len > 0.) {
          xlist[l].len = xlist[l - 1].len;
          xlist[l].i = xlist[l - 1].i;
          xlist[l].j = xlist[l - 1].j;
          xlist[l].x = xlist[l - 1].x;
          xlist[l].y = xlist[l - 1].y;
        }
      }
      xlist[k].len = len;
      xlist[k].i = i;
      xlist[k].j = j;
      xlist[k].x = x;
      xlist[k].y = y;
      break;
    }
  }
}


static void pdfree(void *data) {
  weed_free(data);
}


static cairo_t *channel_to_cairo(weed_plant_t *channel) {
  // convert a weed channel to cairo

  int irowstride, orowstride;
  int width, widthx;
  int height, pal;
  int error;

  register int i;

  guchar *src, *dst;

  cairo_surface_t *surf;
  cairo_t *cairo = NULL;
  cairo_format_t cform;

  void *pixel_data;

  width = weed_get_int_value(channel, "width", &error);

  pal = weed_get_int_value(channel, "current_palette", &error);
  if (pal == WEED_PALETTE_A8) {
    cform = CAIRO_FORMAT_A8;
    widthx = width;
  } else if (pal == WEED_PALETTE_A1) {
    cform = CAIRO_FORMAT_A1;
    widthx = width >> 3;
  } else {
    cform = CAIRO_FORMAT_ARGB32;
    widthx = width << 2;
  }

  height = weed_get_int_value(channel, "height", &error);

  irowstride = weed_get_int_value(channel, "rowstrides", &error);

  orowstride = cairo_format_stride_for_width(cform, width);

  src = (guchar *)weed_get_voidptr_value(channel, "pixel_data", &error);

  dst = pixel_data = (guchar *)weed_malloc(height * orowstride);
  if (pixel_data == NULL) return NULL;

  if (irowstride == orowstride) {
    weed_memcpy(dst, src, height * irowstride);
  } else {
    for (i = 0; i < height; i++) {
      weed_memcpy(dst, src, widthx);
      weed_memset(dst + widthx, 0, widthx - orowstride);
      dst += orowstride;
      src += irowstride;
    }
  }

  if (cform == CAIRO_FORMAT_ARGB32) {
    alpha_premult(dst, width, height, orowstride, WEED_PALETTE_ARGB32, FALSE);
  }

  surf = cairo_image_surface_create_for_data(pixel_data,
         cform,
         width, height,
         orowstride);

  cairo = cairo_create(surf);
  cairo_surface_destroy(surf);

  cairo_set_user_data(cairo, &crkey, pixel_data, pdfree);

  return cairo;
}


static gboolean cairo_to_channel(cairo_t *cairo, weed_plant_t *channel) {
  // updates a weed_channel from a cairo_t
  // unlike doing this the other way around
  // the cairo is not destroyed (data is copied)
  void *dst, *src;

  int width, height, irowstride, orowstride, widthx, pal;

  cairo_surface_t *surface = cairo_get_target(cairo);

  register int i;

  // flush to ensure all writing to the image was done
  cairo_surface_flush(surface);

  dst = weed_get_voidptr_value(channel, "pixel_data", NULL);
  if (dst == NULL) return FALSE;

  src = cairo_image_surface_get_data(surface);
  height = cairo_image_surface_get_height(surface);
  width = cairo_image_surface_get_width(surface);
  irowstride = cairo_image_surface_get_stride(surface);

  orowstride = weed_get_int_value(channel, "rowstrides", NULL);
  pal = weed_get_int_value(channel, "current_palette", NULL);

  if (irowstride == orowstride) {
    weed_memcpy(dst, src, height * orowstride);
  } else {
    widthx = width * 4;
    if (pal == WEED_PALETTE_A8) widthx = width;
    else if (pal == WEED_PALETTE_A1) widthx = width >> 3;

    for (i = 0; i < height; i++) {
      weed_memcpy(dst, src, widthx);
      weed_memset(dst + widthx, 0, orowstride - widthx);
      dst += orowstride;
      src += irowstride;
    }
  }

  if (pal == WEED_PALETTE_ARGB32) {
    alpha_premult(dst, width, height, orowstride, WEED_PALETTE_ARGB32, TRUE);
  }
  return TRUE;
}


/////////////////////////////////////////////////////////////

enum {
  MD_GRID,
  MD_LARGEST
};

static void draw_arrow(cairo_t *cr, int i, int j, float x, float y) {
  // draw arrow from point i-x,j-y to i, j
  int stx = i - (x + .5);
  int sty = j - (y + .5);

  int len = sqrt(x * x + y * y);

  cairo_set_line_width(cr, 4.);
  cairo_set_source_rgb(cr, 1., 0., 0.);

  cairo_move_to(cr, stx, sty);
  cairo_line_to(cr, i, j);

  cairo_arc(cr, i, j, len / 4., 0, M_PI * 2.);
  cairo_stroke(cr);
}


static weed_error_t vector_visualiser_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  cairo_t *cr;
  weed_plant_t **in_channels = weed_get_plantptr_array(inst, "in_channels", NULL);
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, "out_channels", NULL);

  float *alpha0 = (float *)weed_get_voidptr_value(in_channels[1], "pixel_data", NULL);
  float *alpha1 = (float *)weed_get_voidptr_value(in_channels[2], "pixel_data", NULL);

  float x, y, scale = 1.;

  int mode = MD_GRID;

  int irow0 = weed_get_int_value(in_channels[1], "rowstrides", NULL) >> 2;
  int irow1 = weed_get_int_value(in_channels[2], "rowstrides", NULL) >> 2;

  int width = weed_get_int_value(out_channel, "width", NULL);
  int height = weed_get_int_value(out_channel, "height", NULL);
  register int i, j;

  cr = channel_to_cairo(in_channels[0]);

  switch (mode) {
  case MD_GRID: {
    int smwidth = width / 20;
    int smheight = height / 20;

    if (smwidth < 1) smwidth = 1;
    if (smheight < 1) smheight = 1;

    for (i = smheight; i < height; i += smheight * 2) {
      for (j = smwidth; j < width; j += smwidth * 2) {
        x = alpha0[i * irow0 + j];
        y = alpha1[i * irow1 + j];
        draw_arrow(cr, j, i, x * scale, y * scale);
      }
    }
  }

  break;

  case MD_LARGEST: {
    float len;
    clear_xlist();

    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        x = alpha0[i * irow0 + j];
        y = alpha1[i * irow1 + j];
        len = sqrt(x * x + y * y);
        if (len > xlist[MAX_ELEMS - 1].len) add_to_list(len, i, j, x, y);
      }
    }

    for (i = 0; i < MAX_ELEMS; i++) {
      if (xlist[i].len > 0.) draw_arrow(cr, xlist[i].j, xlist[i].i, xlist[i].x * scale, xlist[i].y * scale);
    }
  }
  break;

  default:
    break;
  }

  cairo_to_channel(cr, out_channel);
  cairo_destroy(cr);

  weed_free(in_channels);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int apalette_list[] = {WEED_PALETTE_AFLOAT, WEED_PALETTE_END};
  int vpalette_list[] = {WEED_PALETTE_BGRA32, WEED_PALETTE_END};
  char desc[1024];

  weed_plant_t *in_chantmpls[] = {
    weed_channel_template_init("video in", 0),
    weed_channel_template_init("X-plane", 0),
    weed_channel_template_init("Y-plane", 0), NULL
  };

  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("video out", WEED_CHANNEL_CAN_DO_INPLACE), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("cairo vector visualiser", "salsaman", 1, WEED_FILTER_PALETTES_MAY_VARY,
                               NULL, NULL, vector_visualiser_process, NULL,
                               in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_array(in_chantmpls[0], WEED_LEAF_PALETTE_LIST, sizeof(vpalette_list) / 4, vpalette_list);
  weed_set_int_array(in_chantmpls[1], WEED_LEAF_PALETTE_LIST, sizeof(apalette_list) / 4, apalette_list);
  weed_set_int_array(in_chantmpls[2], WEED_LEAF_PALETTE_LIST, sizeof(apalette_list) / 4, apalette_list);
  weed_set_int_array(out_chantmpls[0], WEED_LEAF_PALETTE_LIST, sizeof(vpalette_list) / 4, vpalette_list);

  if (is_big_endian()) {
    weed_set_int_value(in_chantmpls[0], WEED_LEAF_PALETTE_LIST, WEED_PALETTE_ARGB32);
    weed_set_int_value(out_chantmpls[0], WEED_LEAF_PALETTE_LIST, WEED_PALETTE_ARGB32);
  }

  snprintf(desc, 1024,
           "This plugin takes as input 1 video frame and two equally sized (width * height) float alpha frames.\n"
           "The plugin treats each alpha frame as a 2 dimensional float array, with the first frame containing x values"
           "and the second, corresponging y values\n\n"
           "The plugin has two modes of operation, the choice of which is fixed at compile time (PATCHME !)"
           "In 'grid' mode, the frames are divided into 20 X 20 equally sized rectangles\n"
           "The centre of each rectangle forms the base of an arrow which vector_visualiser overlays on the video frame.\n"
           "The x and y coordinates of the arrowhead are derived from the corresponding values\n"
           "held at the rectangle centers in the x and y arrays\n\n"
           "In 'max' mode, the plugin will first compile a list of the %d largest values in the alpha planes"
           "where the value is defined as the Pythagorean product: sqrt (x * x + y * y).\n"
           "Then for each entry in the list, vector_visualiser will overlay an  arrow with base coordinates "
           "equal to the location (in real x and y coordinates) where the values lie in the alpha planes,\n"
           "and with the arrowhead located at an offset dependant on the scalar values at that point.\n"
           ,  MAX_ELEMS);

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);
  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;

