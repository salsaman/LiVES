// gdk_fast_resize.c
// weed plugin - resize using gdk
// (c) G. Finch (salsaman) 2007 - 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_RANDOM
#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "../weed-plugin-utils.c" // optional

//////////////////////////////////////////////////////////////////

#include <stdio.h>

typedef struct _sdata {
  unsigned char *bgbuf;
  int count;
  int idxno;
  int dir;
} sdata;

#include <gdk/gdk.h>

static GdkPixbuf *pl_channel_to_pixbuf(weed_plant_t *channel) {
  int palette = weed_channel_get_palette(channel);
  int width = weed_channel_get_width(channel);
  int height = weed_channel_get_height(channel);
  int irowstride = weed_channel_get_stride(channel);
  guchar *pixel_data = (guchar *)weed_channel_get_pixel_data(channel);
  GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, weed_palette_has_alpha_channel(palette), 8, width, height);
  guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
  int orowstride = gdk_pixbuf_get_rowstride(pixbuf);
  int psize = pixel_size(palette);
  width *= psize;

  for (int i = 0; i < height; i++) {
    weed_memcpy(pixels + i * orowstride, pixel_data + i * irowstride, width);
  }
  return pixbuf;
}


static weed_error_t videowall_init(weed_plant_t *inst) {
  int width, xwidth, height, palette;
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);

  struct _sdata *sdata = weed_malloc(sizeof(struct _sdata));
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

  palette = weed_channel_get_palette(out_channel);
  width = weed_channel_get_width(out_channel);
  height = weed_channel_get_height(out_channel);
  sdata->bgbuf = weed_calloc((width * height * pixel_size(palette) + 67) >> 2, 4);

  if (!sdata->bgbuf) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  // set a black background
  xwidth = width * pixel_size(palette);
  blank_frame((void **)&sdata->bgbuf, width, height, &xwidth, palette, weed_channel_get_yuv_clamping(out_channel));

  sdata->count = 0;
  sdata->dir = 0;
  sdata->idxno = -1;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t videowall_deinit(weed_plant_t *inst) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    if (sdata->bgbuf) weed_free(sdata->bgbuf);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t videowall_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0), *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  int palette = weed_channel_get_palette(in_channel);
  int width = weed_channel_get_width(in_channel);
  int height = weed_channel_get_height(in_channel);
  GdkPixbuf *in_pixbuf = pl_channel_to_pixbuf(in_channel), *out_pixbuf;
  int down_interp = GDK_INTERP_BILINEAR;
  uint64_t fastrand_val = fastrand(0);
  int psize, prow, orow, pwidth, pheight;
  unsigned char *bdst, *rpix;
  int ofh = 0, ofw = 0;
  int row, col, idxno, offs_x, offs_y;
  int xwid = weed_param_get_value_int(in_params[0]);
  int xht = weed_param_get_value_int(in_params[1]);
  int mode = weed_param_get_value_int(in_params[2]);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  int i;

  weed_free(in_params);

  pwidth = width / xwid;
  pheight = height / xht;
  pwidth = (pwidth >> 1) << 1;
  pheight = (pheight >> 1) << 1;
  if (pwidth == 0 || pheight == 0) return WEED_SUCCESS;

  offs_x = (width - pwidth * xwid) >> 1;
  offs_y = (height - pheight * xht) >> 1;

  out_pixbuf = gdk_pixbuf_scale_simple(in_pixbuf, pwidth, pheight, down_interp);
  g_object_unref(in_pixbuf);
  if (!out_pixbuf) return WEED_SUCCESS;

  psize = pixel_size(palette);

  switch (mode) {
  case 0:
    // l to r, top to bottom
    idxno = sdata->count;
    break;
  case 1:
    // pseudo-random
    idxno = ((fastrand_val = fastrand(fastrand_val)) >> 24) % (xwid * xht);
    break;
  case 2:
    // spiral
    idxno = sdata->idxno;
    if (idxno == -1) {
      idxno = 0;
      sdata->dir = 0;
      break;
    }

    row = (int)((float)idxno / (float)xwid);
    col = idxno - (row * xwid);

    if (sdata->dir == 0) {
      if (col >= xwid - 1 - row) sdata->dir = 1; // time to go down
      // go right
      else idxno++;
    }
    if (sdata->dir == 1) {
      if (row >= col - (xwid - xht)) sdata->dir = 2; // time to go left
      // go down
      else idxno += xwid;
    }
    if (sdata->dir == 2) {
      if (col <= (xwid - row - 1) - (xwid - xht)) {
        sdata->dir = 3; // time to go up
        if (row <= col + 1) {
          idxno = 0;
          sdata->dir = 0;
          break;
        }
      }
      // go left
      else idxno--;
    }
    if (sdata->dir == 3) {
      if (row <= col + 1) {
        sdata->dir = 0; // time to go right
        if (col < xwid - 1 - row) idxno++;
      }
      // go up
      else idxno -= xwid;
    }
    if (idxno == sdata->idxno) {
      idxno = 0;
      sdata->dir = 0;
    }
    break;
  default:
    idxno = 0;
  }

  idxno %= (xwid * xht);
  sdata->idxno = idxno;

  row = (int)((float)idxno / (float)xwid);
  col = idxno - (row * xwid);
  ofh = offs_y + pheight * row;
  ofw = (offs_x + pwidth * col) * psize;

  width *= psize;
  bdst = sdata->bgbuf + ofh * width + ofw;
  prow = gdk_pixbuf_get_rowstride(out_pixbuf);
  rpix = gdk_pixbuf_get_pixels(out_pixbuf);
  pwidth = gdk_pixbuf_get_width(out_pixbuf) * psize;
  pheight = gdk_pixbuf_get_height(out_pixbuf);

  for (i = 0; i < pheight; i++) {
    // copy pixel_data to bgbuf
    weed_memcpy(bdst + i * width, rpix + i * prow, pwidth);
  }

  g_object_unref(out_pixbuf);

  if (++sdata->count == xwid * xht) sdata->count = 0;
  orow = weed_channel_get_stride(out_channel);

  if (orow == width) weed_memcpy(dst, sdata->bgbuf, width * height);
  else for (i = 0; i < height; i++) {
      weed_memcpy(dst + i * orow, sdata->bgbuf + i * width, width);
    }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  const char *modes[] = {"Scanner", "Random", "Spiral", NULL};
  int palette_list[] = ALL_PACKED_PALETTES;
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0",
                                   WEED_CHANNEL_REINIT_ON_SIZE_CHANGE | WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE),
                                   NULL
                                  };

  weed_plant_t *in_params[] = {weed_integer_init("r", "Number of _rows", 3, 1, 256),
                               weed_integer_init("c", "Number of _Columns", 3, 1, 256),
                               weed_string_list_init("m", "Stepping Mode", 0, modes),
                               NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("videowall", "salsaman", 1, 0, palette_list,
                               &videowall_init, &videowall_process, &videowall_deinit,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

