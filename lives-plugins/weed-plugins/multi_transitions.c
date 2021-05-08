// weed plugin
// (c) G. Finch (salsaman) 2009
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
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <math.h>
#include <stdlib.h>
#include <inttypes.h>

////////////////////////////////////////////////////////////

typedef struct {
  volatile int cpy0;
  float *mask;
  uint64_t fastrand_val;
} _sdata;


static weed_error_t common_init(weed_plant_t *inst) {
  _sdata *sdata = weed_calloc(1, sizeof(_sdata));
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;
  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t dissolve_init(weed_plant_t *inst) {
  _sdata *sdata;
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_error_t error;
  int width = weed_channel_get_width(in_channel);
  int height = weed_channel_get_height(in_channel);

  error = common_init(inst);
  if (error != WEED_SUCCESS) return error;

  sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  sdata->mask = weed_calloc(width * height, sizeof(float));
  if (!sdata->mask) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->fastrand_val = fastrand(0); // seed random

  for (int i = 0; i < height; i += width) {
    for (int j = 0; j < width; j++) {
      sdata->mask[width * i + j] = (float)((double)(sdata->fastrand_val = fastrand(sdata->fastrand_val)) / (double)UINT64_MAX);
    }
  }

  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t common_deinit(weed_plant_t *inst) {
  _sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata->mask) {
    if (sdata->mask) weed_free(sdata->mask);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


static weed_error_t common_process(int type, weed_plant_t *inst, weed_timecode_t timecode) {
  _sdata *sdata = NULL;
  weed_plant_t **in_channels = weed_get_in_channels(inst, NULL),
                 *out_channel = weed_get_out_channel(inst, 0);
  unsigned char *src1 = weed_channel_get_pixel_data(in_channels[0]);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);
  int inplace = (src1 == dst);
  int is_threading = weed_is_threading(inst);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  double bfd = weed_param_get_value_double(in_params[0]);
  weed_free(in_params);

  if (type == 3 || type == 4) sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (type == 4) {
    if (is_threading == WEED_TRUE) {
      if (!weed_plant_has_leaf(inst, WEED_LEAF_STATE_UPDATED)) {
        return WEED_ERROR_FILTER_INVALID;
      }
      if (weed_get_boolean_value(inst, WEED_LEAF_STATE_UPDATED, NULL) == WEED_FALSE) {
        if (fastrnd_dbl(1.) >= bfd) sdata->cpy0 = 0;
        else sdata->cpy0 = 1;
        weed_set_boolean_value(inst, WEED_LEAF_STATE_UPDATED, WEED_TRUE);
      }
    }
    if (inplace && !sdata->cpy0) return WEED_SUCCESS;
  }

  if (1) {
    unsigned char *src2 = weed_channel_get_pixel_data(in_channels[1]);
    int width = weed_channel_get_width(in_channels[0]);
    int height = weed_channel_get_height(in_channels[0]);
    int irowstride1 = weed_channel_get_stride(in_channels[0]);
    int irowstride2 = weed_channel_get_stride(in_channels[1]);
    int orowstride = weed_channel_get_stride(out_channel);
    int cpal = weed_channel_get_palette(out_channel);

    size_t psize = pixel_size(cpal);

    int xx = 0, yy = 0, ihwidth, ihheight = height >> 1;

    float bf, bfneg;
    float maxradsq = 0., xxf, yyf;
    float hwidth, hheight = (float)height * 0.5f;

    int i = 0, dheight = height;


    hwidth = (float)width * 0.5f;
    if (type == 1) maxradsq = ((hheight * hheight) + (hwidth * hwidth));

    width *= psize;

    hwidth = (float)width * 0.5f;

    ihwidth = width >> 1;

    bf = (float)bfd;
    bfneg = 1.f - bf;

    if (type == 2) {
      xx = (int)(hheight * bf + .5) * irowstride1;
      yy = (int)(hwidth / (float)psize * bf + .5) * psize;
    }

    if (is_threading) {
      int offset = weed_channel_get_offset(out_channel);
      dheight = weed_channel_get_height(out_channel);
      src1 += offset * irowstride1;
      src2 += offset * irowstride2;
      dst += offset * orowstride;
      i += offset;
    }

    for (int k = 0; k < dheight; k++, i++) {
      for (int j = 0; j < width; j += psize) {
        switch (type) {
        case 0:
          // iris rectangle
          xx = (int)hwidth * bfneg + .5;
          yy = (int)hheight * bfneg + .5;
          if (j < xx || j >= (width - xx) || i < yy || i >= (height - yy)) {
            if (!inplace) weed_memcpy(&dst[orowstride * k + j], &src1[irowstride1 * k + j], psize);
            else {
              if (i >= (height - yy)) {
                j = width;
                k = dheight;
                break;
              }
              if (j >= ihwidth) {
                j = width;
                break;
              }
            }
          } else weed_memcpy(&dst[orowstride * k + j], &src2[irowstride2 * k + j], psize);
          break;
        case 1:
          //iris circle
          xxf = (float)(i - ihheight);
          yyf = (float)(j - ihwidth) / (float)psize;
          if (sqrt((xxf * xxf + yyf * yyf) / maxradsq) > bf) {
            if (!inplace) weed_memcpy(&dst[orowstride * k + j], &src1[irowstride1 * k + j], psize);
            else {
              if (yyf >= 0) {
                j = width;
                if (xxf > 0 && yyf == 0) k = dheight;
                break;
              }
            }
          } else weed_memcpy(&dst[orowstride * k + j], &src2[irowstride2 * k + j], psize);
          break;
        case 2:
          // four way split
          if (fabsf(i - hheight) / hheight < bf || fabsf(j - hwidth) / hwidth < bf || bf == 1.f) {
            weed_memcpy(&dst[orowstride * k + j], &src2[irowstride2 * k + j], psize);
          } else {
            weed_memcpy(&dst[orowstride * k + j], &src1[irowstride1 * k + j + (j > ihwidth ? -yy : yy) + (i > ihheight ? -xx : xx)], psize);
          }
          break;
        case 3:
          // dissolve
          if (sdata->mask[(i * width + j) / psize] < bf) weed_memcpy(&dst[orowstride * k + j], &src2[irowstride2 * k + j], psize);
          else if (!inplace) weed_memcpy(&dst[orowstride * k + j], &src1[irowstride1 * k + j], psize);
          break;
        case 4:
          // random replace :; show either src1 or src2 depending on random value <= trans val
          if (sdata->cpy0)
            weed_memcpy(&dst[orowstride * k], &src2[irowstride2 * k], width);
          else
            weed_memcpy(&dst[orowstride * k], &src1[irowstride1 * k], width);
          j = width;
          break;
        }
      }
    }
    weed_free(in_channels);
  }
  return WEED_SUCCESS;
}


static weed_error_t irisr_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(0, inst, timestamp);
}

static weed_error_t irisc_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(1, inst, timestamp);
}

static weed_error_t fourw_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(2, inst, timestamp);
}

static weed_error_t dissolve_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(3, inst, timestamp);
}

static weed_error_t rreplace_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(4, inst, timestamp);
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone1, **clone2, **clone3;

  int palette_list[] = ALL_PACKED_PALETTES;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0),
                                  weed_channel_template_init("in channel 1", 0), NULL
                                 };

  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0",
                                   WEED_CHANNEL_CAN_DO_INPLACE), NULL
                                  };

  weed_plant_t *in_params[] = {weed_float_init("amount", "_Transition", 0., 0., 1.), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("iris rectangle", "salsaman", 1,
                               WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               NULL, irisr_process, NULL, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_paramtmpl_declare_transition(in_params[0]);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  filter_class = weed_filter_class_init("iris circle", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        NULL, irisc_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params)),
                                        NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  weed_paramtmpl_set_flags(out_chantmpls[0], 0);

  filter_class = weed_filter_class_init("4 way split", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        NULL, fourw_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params)),
                                        NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  weed_set_int_value(out_chantmpls[0], WEED_LEAF_FLAGS,
                     WEED_CHANNEL_CAN_DO_INPLACE | WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);

  filter_class = weed_filter_class_init("dissolve", "salsaman", 1,
                                        WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        dissolve_init, dissolve_process, common_deinit,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params)),
                                        NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  weed_paramtmpl_set_flags(out_chantmpls[0], WEED_CHANNEL_CAN_DO_INPLACE);

  filter_class = weed_filter_class_init("rand replace", "salsaman", 1,
                                        WEED_FILTER_HINT_STATEFUL | WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        common_init, rreplace_process, common_deinit,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params)),
                                        NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);


  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

