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
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <math.h>
#include <stdlib.h>
#include <inttypes.h>

////////////////////////////////////////////////////////////

typedef struct {
  float *mask;
  uint64_t fastrand_val;
} _sdata;


static weed_error_t dissolve_init(weed_plant_t *inst) {
  _sdata *sdata;
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int end = width * height;
  register int i, j;

  sdata = weed_malloc(sizeof(_sdata));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->mask = (float *)weed_malloc(width * height * sizeof(float));
  if (sdata->mask == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->fastrand_val = fastrand(0); // seed random

  for (i = 0; i < end; i += width) {
    for (j = 0; j < width; j++) {
      sdata->mask[i + j] = (float)((double)(sdata->fastrand_val = fastrand(sdata->fastrand_val)) / (double)UINT64_MAX);
    }
  }

  weed_set_voidptr_value(inst, "plugin_internal", sdata);
  return WEED_SUCCESS;
}


static weed_error_t dissolve_deinit(weed_plant_t *inst) {
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata->mask) {
    if (sdata->mask) weed_free(sdata->mask);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


static weed_error_t common_process(int type, weed_plant_t *inst, weed_timecode_t timecode) {
  _sdata *sdata = NULL;
  weed_plant_t *in_param;
  weed_plant_t **in_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL),
                 *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  unsigned char *src1 = weed_get_voidptr_value(in_channels[0], WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *src2 = weed_get_voidptr_value(in_channels[1], WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  int width = weed_get_int_value(in_channels[0], WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(in_channels[0], WEED_LEAF_HEIGHT, NULL);
  int irowstride1 = weed_get_int_value(in_channels[0], WEED_LEAF_ROWSTRIDES, NULL);
  int irowstride2 = weed_get_int_value(in_channels[1], WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int cpal = weed_get_int_value(out_channel, WEED_LEAF_CURRENT_PALETTE, NULL);

  unsigned char *end = src1 + height * irowstride1;

  size_t psize = 4;

  int inplace = (src1 == dst);
  int xx = 0, yy = 0, ihwidth, ihheight = height >> 1;

  float bf, bfneg;
  float maxradsq = 0., xxf, yyf;
  float hwidth, hheight = (float)height * 0.5f;

  register int i = 0, j;

  if (cpal == WEED_PALETTE_RGB24 || cpal == WEED_PALETTE_BGR24 || cpal == WEED_PALETTE_YUV888) psize = 3;

  hwidth = (float)width * 0.5f;
  if (type == 1) maxradsq = ((hheight * hheight) + (hwidth * hwidth));

  width *= psize;

  hwidth = (float)width * 0.5f;

  ihwidth = width >> 1;

  in_param = weed_get_plantptr_value(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  bf = weed_get_double_value(in_param, WEED_LEAF_VALUE, NULL);
  bfneg = 1.f - bf;

  if (type == 3) sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  else if (type == 2) {
    xx = (int)(hheight * bf + .5) * irowstride1;
    yy = (int)(hwidth / (float)psize * bf + .5) * psize;
  }

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);

    src1 += offset * irowstride1;
    end = src1 + dheight * irowstride1;

    src2 += offset * irowstride2;
    dst += offset * orowstride;
    i += offset;
  }

  for (; src1 < end; src1 += irowstride1) {
    for (j = 0; j < width; j += psize) {
      switch (type) {
      case 0:
        // iris rectangle
        xx = (int)hwidth * bfneg + .5;
        yy = (int)hheight * bfneg + .5;
        if (j < xx || j >= (width - xx) || i < yy || i >= (height - yy)) {
          if (!inplace) weed_memcpy(&dst[j], &src1[j], psize);
          else {
            if (i >= (height - yy)) {
              j = width;
              src1 = end;
              break;
            }
            if (j >= ihwidth) {
              j = width;
              break;
            }
          }
        } else weed_memcpy(&dst[j], &src2[j], psize);
        break;
      case 1:
        //iris circle
        xxf = (float)(i - ihheight);
        yyf = (float)(j - ihwidth) / (float)psize;
        if (sqrt((xxf * xxf + yyf * yyf) / maxradsq) > bf) {
          if (!inplace) weed_memcpy(&dst[j], &src1[j], psize);
          else {
            if (yyf >= 0) {
              j = width;
              if (xxf > 0 && yyf == 0) src1 = end;
              break;
            }
          }
        } else weed_memcpy(&dst[j], &src2[j], psize);
        break;
      case 2:
        // four way split
        if (fabsf(i - hheight) / hheight < bf || fabsf(j - hwidth) / hwidth < bf || bf == 1.f) {
          weed_memcpy(&dst[j], &src2[j], psize);
        } else {
          weed_memcpy(&dst[j], &src1[j + (j > ihwidth ? -yy : yy) + (i > ihheight ? -xx : xx)], psize);
        }
        break;
      case 3:
        // dissolve
        if (sdata->mask[(i * width + j) / psize] < bf) weed_memcpy(&dst[j], &src2[j], psize);
        else if (!inplace) weed_memcpy(&dst[j], &src1[j], psize);
        break;
      }
    }
    src2 += irowstride2;
    dst += orowstride;
    i++;
  }
  weed_free(in_channels);
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
                               WEED_FILTER_HINT_IS_STATELESS | WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               NULL, irisr_process, NULL, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_set_boolean_value(in_params[0], WEED_LEAF_IS_TRANSITION, WEED_TRUE);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  filter_class = weed_filter_class_init("iris circle", "salsaman", 1,
                                        WEED_FILTER_HINT_IS_STATELESS | WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        NULL, irisc_process, NULL,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params)),
                                        NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  weed_set_int_value(out_chantmpls[0], WEED_LEAF_FLAGS, 0);

  filter_class = weed_filter_class_init("4 way split", "salsaman", 1,
                                        WEED_FILTER_HINT_IS_STATELESS | WEED_FILTER_HINT_MAY_THREAD, palette_list,
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
                                        WEED_FILTER_HINT_IS_STATELESS | WEED_FILTER_HINT_MAY_THREAD, palette_list,
                                        dissolve_init, dissolve_process, dissolve_deinit,
                                        (clone1 = weed_clone_plants(in_chantmpls)),
                                        (clone2 = weed_clone_plants(out_chantmpls)),
                                        (clone3 = weed_clone_plants(in_params)),
                                        NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_free(clone1);
  weed_free(clone2);
  weed_free(clone3);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

