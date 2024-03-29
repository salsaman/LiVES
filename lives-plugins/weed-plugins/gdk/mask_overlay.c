// mask_overlay.c
// weed plugin - resize using gdk
// (c) G. Finch (salsaman) 2011
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

#include <weed/weed-plugin-utils.h>

/////////////////////////////////////////////////////////////

typedef struct _sdata {
  int *xmap; // x offset in src0 to map to point (or -1 to map src1)
  int *ymap; // x offset in src0 to map to point (or -1 to map src1)
} sdata;

#include <gdk/gdk.h>


static void make_mask(GdkPixbuf *pbuf, int mode, int owidth, int oheight, int *xmap, int *ymap) {
  int iwidth = gdk_pixbuf_get_width(pbuf);
  int iheight = gdk_pixbuf_get_height(pbuf);
  int stride = gdk_pixbuf_get_rowstride(pbuf);
  guchar *pdata = gdk_pixbuf_get_pixels(pbuf);
  gboolean has_alpha = gdk_pixbuf_get_has_alpha(pbuf);

  double xscale = (double)iwidth / (double)owidth;
  double yscale = (double)iheight / (double)oheight;

  double xscale2 = xscale, yscale2 = yscale;

  int psize = 3;

  int top = -1, bot = -1, left = -1, right = -1, tline = 0, xwidth = 0;
  double xpos = 0., ypos = 0.;

  register int i, j;

  if (has_alpha) psize = 4;

  if (mode == 1) {
    // get bounds

    for (i = 0; i < oheight; i++) {
      for (j = 0; j < owidth; j++) {
        if (*(pdata + (int)(i * yscale) * stride + (int)(j * xscale) * psize + 1) == 0) {
          if (top == -1) top = i;
          if (j < left || left == -1) left = j;
          if (j > right) right = j;
          if (i > bot) bot = i;
        }
      }
    }

    // get width (ignoring non-black)

    tline = (top + bot) >> 1;

    for (j = 0; j < owidth; j++) {
      if (*(pdata + (int)(tline * yscale) * stride + (int)(j * xscale) * psize + 1) == 0) xwidth++;
    }

    yscale2 = (double)oheight / (double)(bot - top);
    xscale2 = (double)owidth / (double)xwidth;

    // map center row as template for other rows
    for (j = 0; j < owidth; j++) {
      if (*(pdata + (int)(tline * yscale) * stride + (int)(j * xscale) * psize + 1) == 0) {
        // map front frame
        xmap[tline * owidth + j] = (int)xpos;
        xpos += xscale2;
      } else {
        xmap[tline * owidth + j] = -1;
      }
    }
  }

  for (i = 0; i < oheight; i++) {
    for (j = 0; j < owidth; j++) {
      if (*(pdata + (int)(i * yscale) * stride + (int)(j * xscale) * psize + 1) == 0) {
        // map front frame

        if (mode == 0) {
          // no re-mapping of front frame
          xmap[i * owidth + j] = j;
          ymap[i * owidth + j] = i;
        } else {
          xmap[i * owidth + j] = xmap[tline * owidth + j];
          ymap[i * owidth + j] = (int)ypos;
        }

      } else {
        // map back frame
        xmap[i * owidth + j] = ymap[i * owidth + j] = -1;
      }
    }
    if (i >= top) ypos += yscale2;
  }
}


static weed_error_t masko_init(weed_plant_t *inst) {
  struct _sdata *sdata;
  int video_height, video_width, video_area;
  int mode;
  weed_plant_t *in_channel, **in_params;
  GdkPixbuf *pbuf;
  GError *gerr = NULL;
  char *mfile;

  in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);

  sdata = weed_malloc(sizeof(struct _sdata));

  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  video_height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  video_width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  video_area = video_width * video_height;

  sdata->xmap = (int *)weed_malloc(video_area * sizeof(int));

  if (sdata->xmap == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->ymap = (int *)weed_malloc(video_area * sizeof(int));

  if (sdata->ymap == NULL) {
    weed_free(sdata->xmap);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  // load image, then get luma values and scale
  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  mfile = weed_get_string_value(in_params[0], WEED_LEAF_VALUE, NULL);
  mode = weed_get_int_value(in_params[1], WEED_LEAF_VALUE, NULL);

  pbuf = gdk_pixbuf_new_from_file(mfile, &gerr);

  if (gerr != NULL) {
    weed_free(sdata->xmap);
    weed_free(sdata->ymap);
    //g_object_unref(gerr);
    sdata->xmap = sdata->ymap = NULL;
  } else {
    make_mask(pbuf, mode, video_width, video_height, sdata->xmap, sdata->ymap);
    g_object_unref(pbuf);
  }

  weed_free(mfile);
  weed_free(in_params);

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t masko_deinit(weed_plant_t *inst) {
  struct _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    if (sdata->xmap != NULL) weed_free(sdata->xmap);
    if (sdata->ymap != NULL) weed_free(sdata->ymap);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t masko_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL),
                 *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);

  int palette = weed_get_int_value(out_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int width = weed_get_int_value(out_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);
  int offset = 0;

  register int i, j, pos;
  struct _sdata *sdata;

  int psize = 3;

  unsigned char *dst, *src0, *src1;
  int orow, irow0, irow1;

  if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32 || palette == WEED_PALETTE_ARGB32 ||
      palette == WEED_PALETTE_YUVA8888) psize = 4;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (sdata->xmap == NULL || sdata->ymap == NULL) return WEED_SUCCESS;

  dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  src0 = weed_get_voidptr_value(in_channels[0], WEED_LEAF_PIXEL_DATA, NULL);
  src1 = weed_get_voidptr_value(in_channels[1], WEED_LEAF_PIXEL_DATA, NULL);

  orow = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  irow0 = weed_get_int_value(in_channels[0], WEED_LEAF_ROWSTRIDES, NULL);
  irow1 = weed_get_int_value(in_channels[1], WEED_LEAF_ROWSTRIDES, NULL);

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);
    height = offset + dheight;
    src1 += offset * irow1;
  }

  pos = offset * width;
  orow -= width * psize;
  irow1 -= width * psize;

  for (i = offset; i < height; i++) {
    for (j = 0; j < width; j++) {
      if (sdata->xmap[pos] == -1 || sdata->ymap[pos] == -1) {
        // map bg pixel to dst
        weed_memcpy(dst, src1, psize);
      } else {
        // remap fg pixel
        weed_memcpy(dst, src0 + sdata->ymap[pos]*irow0 + sdata->xmap[pos]*psize, psize);
      }
      dst += psize;
      src1 += psize;
      pos++;
    }
    dst += orow;
    src1 += irow1;
  }

  weed_free(in_channels);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_PACKED_PALETTES;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0),
                                  weed_channel_template_init("in channel 1", 0), NULL
                                 };

  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *filter_class;
  weed_plant_t *in_params[3], *gui;
  char *rfx_strings[] = {"special|fileread|0|"};
  const char *modes[] = {"normal", "stretch", NULL};

  char *defmaskfile = g_build_filename(g_get_home_dir(), "mask.png", NULL);
  int flags;

  in_params[0] = weed_text_init("maskfile", "_Mask file (.png or .jpg)", defmaskfile);
  gui = weed_paramtmpl_get_gui(in_params[0]);
  weed_set_int_value(gui, WEED_LEAF_MAXCHARS, 80); // for display only - fileread will override this
  flags = 0;
  if (weed_plant_has_leaf(in_params[0], WEED_LEAF_FLAGS))
    flags = weed_get_int_value(in_params[0], WEED_LEAF_FLAGS, NULL);
  flags |= WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
  weed_set_int_value(in_params[0], WEED_LEAF_FLAGS, flags);

  in_params[1] = weed_string_list_init("mode", "Effect _mode", 0, modes);
  flags = 0;
  if (weed_plant_has_leaf(in_params[1], WEED_LEAF_FLAGS))
    flags = weed_get_int_value(in_params[1], WEED_LEAF_FLAGS, NULL);
  flags |= WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
  weed_set_int_value(in_params[1], WEED_LEAF_FLAGS, flags);
  in_params[2] = NULL;

  g_free(defmaskfile);

  filter_class = weed_filter_class_init("mask_overlay", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        masko_init, masko_process, masko_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

  gui = weed_filter_get_gui(filter_class);
  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 1, rfx_strings);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


