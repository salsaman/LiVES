/////////////////////////////////////////////////////////////////////////////
// Weed xeffect plugin, version
// Compiled with Builder version 3.2.1-pre
// autogenerated from script by salsaman
/////////////////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

/////////////////////////////////////////////////////////////////////////////
#define NEED_PALETTE_UTILS
#define NEED_PALETTE_CONVERSIONS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"
//////////////////////////////////////////////////////////////////

typedef struct {
  uint8_t *map;
} sdata_t;

static int verbosity = WEED_VERBOSITY_ERROR;

/////////////////////////////////////////////////////////////////////////////

static inline void make_white(unsigned char *pixel) {
  weed_memset(pixel, 255, 3);
}

static inline void nine_fill(unsigned char *new_data, int row, unsigned char *o) {
  // fill nine pixels with the centre colour
  new_data[-row - 3] = new_data[-row] = new_data[-row + 3] = new_data[-3] = new_data[0] =
                                          new_data[3] = new_data[row - 3] = new_data[row] = new_data[row + 3] = o[0];
  new_data[-row - 2] = new_data[-row + 1] = new_data[-row + 4] = new_data[-2] = new_data[1] =
                         new_data[4] = new_data[row - 2] = new_data[row + 1] = new_data[row + 4] = o[1];
  new_data[-row - 1] = new_data[-row + 2] = new_data[-row + 5] = new_data[-1] = new_data[2] =
                         new_data[5] = new_data[row - 1] = new_data[row + 2] = new_data[row + 5] = o[2];
}

/////////////////////////////////////////////////////////////////////////////

static weed_error_t xeffect_init(weed_plant_t *inst) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);
  sdata_t *sdata;
  if (!(weed_instance_get_flags(inst) & WEED_INSTANCE_UPDATE_GUI_ONLY)) {
    sdata = (sdata_t *)weed_calloc(1, sizeof(sdata_t));
    if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;
    weed_set_voidptr_value(inst, "plugin_internal", sdata);

    if (1) {
      sdata->map = weed_malloc(width * height);
      if (!sdata->map) return WEED_ERROR_MEMORY_ALLOCATION;
    }
  } else sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  return WEED_SUCCESS;
}


static weed_error_t xeffect_deinit(weed_plant_t *inst) {
  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (sdata) {
    if (sdata->map) weed_free(sdata->map);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);

  return WEED_SUCCESS;
}


static weed_error_t xeffect_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int inplace = (src == dst);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);
  sdata_t *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (!sdata) return WEED_ERROR_REINIT_NEEDED;

  if (1) {
    int widthp = (width - 1) * psize, nbr;
    unsigned int myluma, threshold = 10000;
    if (!sdata->map) return WEED_ERROR_REINIT_NEEDED;

    for (int h = 0; h < height; h++) {
      for (int i = 0; i < width; i ++) {
        sdata->map[h * width + i] = calc_luma(&src[h * irow  + i * psize], pal, 0);
      }
    }
    src += irow;
    dst += orow;
    for (int h = 1; h < height - 2; h++) {
      for (int i = psize; i < widthp; i += psize) {
        myluma = sdata->map[h * width + i / psize];
        nbr = 0;
        for (int j = h - 1; j <= h + 1; j++) {
          for (int k = -1; k < 2; k++) {
            if ((j != h || k != 0) &&
                ABS(sdata->map[j * width + i / psize + k] - myluma) > threshold) nbr++;
          }
        }
        if (nbr < 2 || nbr > 5) {
          nine_fill(&dst[h * orow + i], orow, &src[h * irow + i]);
        } else {
          if (myluma < 12500) {
            blank_pixel(&dst[h * orow + i], pal, 0, NULL);
          } else {
            if (myluma > 20000) {
              make_white(&dst[h * orow + i]);
            }
          }
        }
      }
    }
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  weed_plant_t *filter_class;
  int palette_list[] = ALL_RGB_PALETTES;
  weed_plant_t *in_chantmpls[] = {
    weed_channel_template_init("in_channel0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE),
    NULL
  };
  weed_plant_t *out_chantmpls[] = {
    weed_channel_template_init("out_channel0", WEED_CHANNEL_CAN_DO_INPLACE),
    NULL
  };
  int filter_flags = 0;

  verbosity = weed_get_host_verbosity(host_info);

  filter_class = weed_filter_class_init("graphic novel", "salsaman", 1, filter_flags, palette_list,
                                        xeffect_init, xeffect_process, xeffect_deinit, in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

