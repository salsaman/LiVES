// edge.c
// weed plugin
// (c) G. Finch (salsaman) 2005 - 2015
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_CONVERSIONS
#define NEED_PALETTE_UTILS

#include <weed/weed-plugin-utils.h>

/////////////////////////////////////////////////////////////

#include <math.h>

#define SCALEVAL 0.94f /// to get values below 1024
#define TMAX 1017 // sqrt(2 * (255 * 3) ^ 2) * .94

/////////////////////////////////////////////

typedef struct {
  uint8_t *mapl;
  int16_t *maph, *mapv, *map;
  int64_t pr[1024];
} static_data;


static weed_error_t edge_init(weed_plant_t *inst) {
  weed_plant_t *in_channel;
  int width, height;
  static_data *sdata = (static_data *)weed_malloc(sizeof(static_data));
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel = weed_get_in_channel(inst, 0);
  width = weed_channel_get_width(in_channel);
  height = weed_channel_get_height(in_channel);

  sdata->mapl = weed_calloc(height * width, 1);
  if (!sdata->mapl) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->maph = weed_calloc(height * width, 2);
  if (!sdata->maph) {
    weed_free(sdata->mapl);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->mapv = weed_calloc(height * width, 2);
  if (!sdata->mapv) {
    weed_free(sdata->mapl);
    weed_free(sdata->maph);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->map = weed_calloc(height * width, 2);
  if (!sdata->map) {
    weed_free(sdata->mapl);
    weed_free(sdata->maph);
    weed_free(sdata->mapv);
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t edge_deinit(weed_plant_t *inst) {
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata) {
    if (sdata->mapl) weed_free(sdata->mapl);
    if (sdata->maph) weed_free(sdata->maph);
    if (sdata->mapv) weed_free(sdata->mapv);
    if (sdata->map) weed_free(sdata->map);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static inline void copywalpha(uint8_t *dest, size_t doffs, uint8_t *src,
                              size_t offs, uint8_t red, uint8_t green, uint8_t blue, int aoffs, int inplace) {
  // copy alpha from src, and RGB from val; return val
  // red, green, blue are just labels, actual RGB / BGR order is irrelevant
  if (aoffs == 1) {
    if (!inplace) dest[doffs] = src[offs]; // ARGB
    offs++;
    doffs++;
  }
  if (red != 3) {
    if (red == 1)
      dest[doffs] = src[offs];
    else {
      if (red == 0) dest[doffs] = 0;
      else dest[doffs] = 255;
    }
  }
  if (green != 3) {
    if (green == 1)
      dest[doffs + 1] = src[offs + 1];
    else {
      if (green == 0) dest[doffs + 1] = 0;
      else dest[doffs + 1] = 255;
    }
  }
  if (blue != 3) {
    if (blue == 1)
      dest[doffs + 2] = src[offs + 2];
    else {
      if (blue == 0) dest[doffs + 2] = 0;
      else dest[doffs + 2] = 255;
    }
  }
  if (aoffs != 0 || inplace) return;
  dest[doffs + 3] = src[offs + 3]; // RGBA / BGRA
}


static weed_error_t edge_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0),
                *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  uint8_t *src = (uint8_t *)weed_channel_get_pixel_data(in_channel);
  uint8_t *dest = (uint8_t *)weed_channel_get_pixel_data(out_channel);

  int width = weed_channel_get_width(in_channel);
  int height = weed_channel_get_height(in_channel);
  int pal = weed_channel_get_palette(in_channel);
  int irow = weed_channel_get_stride(in_channel);
  int orow = weed_channel_get_stride(out_channel);
  int psize = pixel_size(pal);
  int inplace = (src == dest);
  int x, y, offs = 0;

  int16_t v0, v1;
  uint16_t val, thresh, threshmax = 0;
  uint64_t bh = 0, bl = 0, nbh = 0, nbl = 0, nn;
  double abl = 0., abh = 0., dif = 0., difmax = 0.;
  int mode = 1;

  if (pal == WEED_PALETTE_ARGB32) offs = 1;
  if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) offs = -1;

  mode = weed_param_get_value_int(in_params[0]);
  weed_free(in_params);

  for (int pass = 0; pass < 4; pass++) {
    weed_memset(sdata->pr, 0, 8192);

    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        if (pass == 0)
          sdata->mapl[y * width + x] = calc_luma(&src[y * irow + x * psize], pal, 0);
        else
          sdata->mapl[y * width + x] = src[y * irow + x * psize + pass - 1];
      }
    }

    for (y = 1; y < height - 1; y++) {
      for (x = 1; x < width - 1; x++) {
        sdata->maph[y * width + x] = -sdata->mapl[y * width + x - 1] + sdata->mapl[y * width + x + 1];
        sdata->mapv[y * width + x] = -sdata->mapl[(y - 1) * width + x] + sdata->mapl[(y + 1) * width + x];
      }
    }

    for (y = 2; y < height - 2; y++) {
      for (x = 2; x < width - 2; x++) {
        v0 = sdata->maph[(y - 1) * width + x]
             + sdata->maph[y * width + x] + sdata->maph[(y + 1) * width + x];
        v1 = sdata->mapv[y * width + x - 1]
             + sdata->mapv[y * width + x] + sdata->mapv[y * width + x + 1];
        val = sdata->map[y * width + x] = (uint16_t)(sqrtf((float)(v0 * v0) + (float)(v1 * v1))
                                          * SCALEVAL);
        sdata->pr[val]++;
        bh += val;
        nbh++;
      }
    }

    // use Otsu to find thresh
    abh = (double)(bh) / (double)nbh;
    for (thresh = 0; thresh < TMAX; thresh++) {
      nn = (uint64_t)(sdata->pr[thresh] * thresh);
      bl += nn;
      nbl += sdata->pr[thresh];
      bh -= nn;
      nbh -= sdata->pr[thresh];

      abh = (double)(bh) / (double)nbh;
      abl = (double)(bl) / (double)nbl;

      dif = (double)(nbl * nbh) * (abh - abl) * (abh - abl);

      //fprintf(stderr, "pr[%d] = %d %ld %f |%f|\n", thresh, threshmax, sdata->pr[thresh], dif, difmax);
      if (thresh > 0) {
        if (dif > difmax) {
          difmax = dif;
          threshmax = thresh;
        }
      }
    }
    thresh = threshmax;

    //fprintf(stderr, "thresh = %d, nbl = %ld, nbh = %ld\n", thresh, nbl, nbh);
    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        if (sdata->map[y * width + x] >= thresh) {
          if (pass == 0) {
            if (mode == 1) {
              copywalpha(dest, y * orow + x * psize, src, y * irow + x * psize,
                         2, 2, 2, offs, inplace);
            } else {
              copywalpha(dest, y * orow + x * psize, src, y * irow + x * psize,
                         1, 1, 1, offs, inplace);
            }
          } else if (pass == 1)
            copywalpha(dest, y * orow + x * psize, src, y * irow + x * psize,
                       2, 3, 3, offs, inplace);
          else if (pass == 2)
            copywalpha(dest, y * orow + x * psize, src, y * irow + x * psize,
                       3, 2, 3, offs, inplace);
          else if (pass == 3)
            copywalpha(dest, y * orow + x * psize, src, y * irow + x * psize,
                       3, 3, 2, offs, inplace);
        } else {
          if (pass == 0) {
            copywalpha(dest, y * orow + x * psize, src, y * irow + x * psize,
                       0, 0, 0, offs, inplace);
          }
        }
      }
    }
    if (mode < 2) break;
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  const char *modes[] = {"normal", "monochrome", "supercolour", NULL};
  int palette_list[] = ALL_RGB_PALETTES;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE), NULL};
  weed_plant_t *in_params[] = {weed_string_list_init("mode", "Edge _Mode", 0, modes), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("edge detect", "effectTV", 1, WEED_FILTER_PREF_LINEAR_GAMMA,
                               palette_list, edge_init, edge_process, edge_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

