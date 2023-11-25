// mirrors.c
// weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

#include <weed/weed-plugin.h>
#include <weed/weed-utils.h>
#include <weed/weed-plugin-utils.h>

#include <weed/weed-plugin-utils/weed-plugin-utils.c>

static int verbosity = WEED_VERBOSITY_ERROR;

/////////////////////////////////////////////////////////////

static weed_error_t mirrorx_process(weed_plant_t *inst, weed_timecode_t timestamp) { 
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int inplace = (src == dst);
  const int pal = (const int)weed_channel_get_palette(in_chan);
  int irow = (const int)weed_channel_get_stride(in_chan);
  const int width = (const int)weed_channel_get_width(out_chan);
  const int height = (const int)weed_channel_get_height(out_chan);
  const int orow = (const int)weed_channel_get_stride(out_chan);
  const int psize = (const int)pixel_size((int)pal);
  int iheight = weed_channel_get_height(in_chan);
  int is_threading = weed_is_threading(inst);
  int offset = 0;

  if (is_threading) {
    offset = weed_channel_get_offset(out_chan);
    src += offset * irow;
    dst += offset * orow;
  }

  do {
    const int xwidth = (width >> 1) * psize;
    if (weed_get_boolean_value(inst, "plugin_combined", NULL)) {
      inplace = 1;
      src = dst;
      irow = orow;
    }
    if (inplace) {
      dst += xwidth << 1;
      for (int i = 0; i < height; i++) {
	const int xrow = i * orow;
	for (int j = 0; j < xwidth; j += psize)
	  weed_memcpy(dst + xrow - j, src + xrow + j, psize);
      }
    }
    else {
      for (int i = 0; i < height; i++) {
	int xrow = i * orow;
	const int yrow = i * irow;
	weed_memcpy(dst + xrow, src + yrow, xwidth);
	xrow += xwidth << 1;
	for (int j = 0; j < xwidth; j += psize)
	  weed_memcpy(dst + xrow - j, src + yrow + j, psize); 
      }
    }
  } while (0);

  return WEED_SUCCESS;
}


static weed_error_t mirrory_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int inplace = (src == dst);
  const int pal = (const int)weed_channel_get_palette(in_chan);
  int irow = (const int)weed_channel_get_stride(in_chan);
  int iheight = weed_channel_get_height(in_chan);
  int is_threading = weed_is_threading(inst);
  int offset = 0;
  const int width = (const int)weed_channel_get_width(out_chan);
  const int height = (const int)weed_channel_get_height(out_chan);
  const int hheight = height >> 1;
  const int orow = (const int)weed_channel_get_stride(out_chan);
  const int psize = (const int)pixel_size((int)pal);
  const int xwidth = width * psize;

  if (!inplace) {
    if (irow == orow) weed_memcpy(dst, src, hheight * irow);
    else for (int i = 0; i < hheight; i++)
	   weed_memcpy(dst + i * orow, src + i * irow, xwidth);
  }

  dst += orow * height;

  for (int i = 0; i < hheight; i++)
    weed_memcpy(dst - orow * i, src + irow * i, xwidth);

  return WEED_SUCCESS;
}


static weed_error_t mirrorxy_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_error_t ret = mirrory_process(inst, timestamp);
  if (ret != WEED_SUCCESS) return ret;
  weed_set_boolean_value(inst, "plugin_combined", WEED_TRUE);
  ret = mirrorx_process(inst, timestamp);
  return ret;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone1, **clone2;
  // all planar palettes
  int palette_list[] = ALL_PACKED_PALETTES_PLUS;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("mirrorx", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               NULL, mirrorx_process, NULL, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  filter_class = weed_filter_class_init("mirrory", "salsaman", 1, 0, palette_list,
                                        NULL, mirrory_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)), NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_free(clone1);
  weed_free(clone2);

  filter_class = weed_filter_class_init("mirrorxy", "salsaman", 1, 0, palette_list,
                                        NULL, mirrorxy_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)), NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_free(clone1);
  weed_free(clone2);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


