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
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../../libweed/weed-plugin.h"
#include "../../../libweed/weed-plugin-utils.h" // optional
#endif

#include "../weed-plugin-utils.c" // optional

//////////////////////////////////////////////////////////////////

typedef struct _sdata {
  unsigned char *bgbuf;
  int count;
  int idxno;
  int dir;
} sdata;

#include <gdk/gdk.h>

inline G_GNUC_CONST int pl_gdk_rowstride_value(int rowstride) {
  // from gdk-pixbuf.c
  /* Always align rows to 32-bit boundaries */
  return (rowstride + 3) & ~3;
}


inline int G_GNUC_CONST pl_gdk_last_rowstride_value(int width, int nchans) {
  // from gdk pixbuf docs
  return width * (((nchans << 3) + 7) >> 3);
}


static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height,
                                      guchar *buf) {
  // we can cheat if our buffer is correctly sized
  int channels = has_alpha ? 4 : 3;
  int rowstride = pl_gdk_rowstride_value(width * channels);
  return gdk_pixbuf_new_from_data(buf, colorspace, has_alpha, bits_per_sample, width, height, rowstride, NULL, NULL);
}


static GdkPixbuf *pl_channel_to_pixbuf(weed_plant_t *channel) {
  GdkPixbuf *pixbuf;
  int palette = weed_get_int_value(channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int width = weed_get_int_value(channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(channel, WEED_LEAF_HEIGHT, NULL);
  int irowstride = weed_get_int_value(channel, WEED_LEAF_ROWSTRIDES, NULL);
  int rowstride, orowstride;
  guchar *pixel_data = (guchar *)weed_get_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, NULL), *pixels, *end;
  gboolean cheat = FALSE;
  gint n_channels;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_YUV888:
    if (irowstride == pl_gdk_rowstride_value(width * 3)) {
      pixbuf = pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat = TRUE;
    } else pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels = 3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_YUVA8888:
    if (irowstride == pl_gdk_rowstride_value(width * 4)) {
      pixbuf = pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, TRUE, 8, width, height, pixel_data);
      cheat = TRUE;
    } else pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    n_channels = 4;
    break;
  default:
    return NULL;
  }
  pixels = gdk_pixbuf_get_pixels(pixbuf);
  orowstride = gdk_pixbuf_get_rowstride(pixbuf);

  if (irowstride > orowstride) rowstride = orowstride;
  else rowstride = irowstride;
  end = pixels + orowstride * height;

  if (!cheat) {
    gboolean done = FALSE;
    for (; pixels < end && !done; pixels += orowstride) {
      if (pixels + orowstride >= end) {
        orowstride = rowstride = pl_gdk_last_rowstride_value(width, n_channels);
        done = TRUE;
      }
      weed_memcpy(pixels, pixel_data, rowstride);
      if (rowstride < orowstride) weed_memset(pixels + rowstride, 0, orowstride - rowstride);
      pixel_data += irowstride;
    }
  }
  return pixbuf;
}


static weed_error_t videowall_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  weed_plant_t *in_channel;
  int video_height, video_width, video_area;
  int palette;
  int psize;
  register int i, j;

  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);

  sdata = weed_malloc(sizeof(struct _sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  video_height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  video_width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  video_area = video_width * video_height;

  sdata->bgbuf = weed_malloc((psize = video_area * (palette == WEED_PALETTE_RGB24 ? 3 : 4)));

  if (sdata->bgbuf == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  // set a black background
  if (palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_BGR24) {
    weed_memset(sdata->bgbuf, 0, psize);
  } else if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32) {
    unsigned char *ptr = sdata->bgbuf;
    for (i = 0; i < video_height; i++) {
      for (j = 0; j < video_width; j++) {
        weed_memset(ptr, 0, 3);
        weed_memset(ptr + 3, 255, 1);
        ptr += 4;
      }
    }
  }

  if (palette == WEED_PALETTE_YUV888) {
    unsigned char *ptr = sdata->bgbuf;
    for (i = 0; i < video_height; i++) {
      for (j = 0; j < video_width; j++) {
        weed_memset(ptr, 16, 1);
        weed_memset(ptr + 1, 128, 2);
        ptr += 3;
      }
    }
  }

  if (palette == WEED_PALETTE_YUVA8888) {
    unsigned char *ptr = sdata->bgbuf;
    for (i = 0; i < video_height; i++) {
      for (j = 0; j < video_width; j++) {
        weed_memset(ptr, 16, 1);
        weed_memset(ptr + 1, 128, 2);
        weed_memset(ptr + 3, 255, 1);
        ptr += 4;
      }
    }
  }

  sdata->count = 0;
  sdata->dir = 0;
  sdata->idxno = -1;

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t videowall_deinit(weed_plant_t *inst) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    weed_free(sdata->bgbuf);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t videowall_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  int palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  GdkPixbuf *in_pixbuf = pl_channel_to_pixbuf(in_channel);
  GdkPixbuf *out_pixbuf;
  int down_interp = GDK_INTERP_BILINEAR;
  register int i, j;
  struct _sdata *sdata;
  int psize = 4, prow, orow, pwidth, pheight;
  unsigned char *bdst, *dst, *rpix;
  int ofh = 0, ofw = 0;
  int xwid, xht, mode;
  int row, col, idxno, bdstoffs, rpixoffs;
  int offs_x, offs_y;
  uint64_t fastrand_val = fastrand(0);

  xwid = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
  xht = weed_get_int_value(in_params[1], WEED_LEAF_VALUE, NULL);
  mode = weed_get_int_value(in_params[2], WEED_LEAF_VALUE, NULL);

  dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  pwidth = width / xwid;
  pheight = height / xht;

  pwidth = (pwidth >> 1) << 1;
  pheight = (pheight >> 1) << 1;

  if (pwidth == 0 || pheight == 0) return WEED_SUCCESS;

  offs_x = (width - pwidth * xwid) >> 1;
  offs_y = (height - pheight * xht) >> 1;

  out_pixbuf = gdk_pixbuf_scale_simple(in_pixbuf, pwidth, pheight, down_interp);

  g_object_unref(in_pixbuf);

  if (out_pixbuf == NULL) return WEED_SUCCESS;

  if (palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_BGR24 || palette == WEED_PALETTE_YUV888) psize = 3;

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
    // TODO

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

  bdst = sdata->bgbuf + ofh * width * psize + ofw;

  prow = gdk_pixbuf_get_rowstride(out_pixbuf);
  rpix = gdk_pixbuf_get_pixels(out_pixbuf);
  pwidth = gdk_pixbuf_get_width(out_pixbuf);
  pheight = gdk_pixbuf_get_height(out_pixbuf);

  bdstoffs = (width - pwidth) * psize;
  rpixoffs = (prow - pwidth * psize);
  // copy pixel_data to bgbuf

  for (i = 0; i < pheight; i++) {
    for (j = 0; j < pwidth; j++) {
      weed_memcpy(bdst, rpix, psize);
      bdst += psize;
      rpix += psize;
    }
    bdst += bdstoffs;
    rpix += rpixoffs;
  }

  g_object_unref(out_pixbuf);


  if (++sdata->count == xwid * xht) sdata->count = 0;
  orow = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);

  if (orow == width * psize) {
    weed_memcpy(dst, sdata->bgbuf, width * psize * height);
  } else {
    for (i = 0; i < height; i++) {
      weed_memcpy(dst, sdata->bgbuf + i * width * psize, width * psize);
      dst += orow;
    }
  }

  weed_free(in_params);
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
                               weed_integer_init("c", "Number of _Columns", 3, 1, 256), weed_string_list_init("m", "Stepping Mode", 0, modes),
                               NULL
                              };
  weed_plant_t *filter_class = weed_filter_class_init("videowall", "salsaman", 1, 0, palette_list,
                               &videowall_init, &videowall_process, &videowall_deinit,
                               in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

