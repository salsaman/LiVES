// alpha_visualizer.c
// weed plugin
// (c) G. Finch (salsaman) 2016
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// convert alpha values to (R) (G) (B) (A)

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdio.h>

static int getbit(uint8_t val, int bit) {
  int x = 1;
  register int i;
  for (i = 0; i < bit; i++) x *= 2;
  return val & x;
}


static weed_error_t alphav_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  float *alphaf;
  uint8_t *alphau;

  uint8_t *dst = (uint8_t *)weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  uint8_t valu;

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL);
  int irow = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orow = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);

  int ipal = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int opal = weed_get_int_value(out_channel, WEED_LEAF_CURRENT_PALETTE, NULL);

  int r = weed_get_boolean_value(in_params[0], WEED_LEAF_VALUE, NULL);
  int g = weed_get_boolean_value(in_params[1], WEED_LEAF_VALUE, NULL);
  int b = weed_get_boolean_value(in_params[2], WEED_LEAF_VALUE, NULL);

  int psize = 4;

  double fmin = weed_get_double_value(in_params[3], WEED_LEAF_VALUE, NULL);
  double fmax = weed_get_double_value(in_params[4], WEED_LEAF_VALUE, NULL);

  register int i, j, k;

  weed_free(in_params);

  if (opal == WEED_PALETTE_RGB24 || opal == WEED_PALETTE_BGR24) psize = 3;

  orow = orow - width * psize;

  if (ipal == WEED_PALETTE_AFLOAT) {
    irow /= sizeof(float);
    alphaf = (float *)weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        if (fmax > fmin) {
          valu = (int)((alphaf[j] - fmin / (fmax - fmin)) * 255. + .5);
          valu = valu < 0 ? 0 : valu > 255 ? 255 : valu;
        } else valu = 0;
        switch (opal) {
        case WEED_PALETTE_RGBA32:
          dst[3] = 0xFF;
        case WEED_PALETTE_RGB24:
          dst[0] = (r == WEED_TRUE ? valu : 0);
          dst[1] = (g == WEED_TRUE ? valu : 0);
          dst[2] = (b == WEED_TRUE ? valu : 0);
          dst += psize;
          break;
        case WEED_PALETTE_BGRA32:
          dst[3] = 0xFF;
        case WEED_PALETTE_BGR24:
          dst[0] = (b == WEED_TRUE ? valu : 0);
          dst[1] = (g == WEED_TRUE ? valu : 0);
          dst[2] = (r == WEED_TRUE ? valu : 0);
          dst += psize;
          break;
        case WEED_PALETTE_ARGB32:
          dst[0] = 0xFF;
          dst[1] = (r == WEED_TRUE ? valu : 0);
          dst[2] = (g == WEED_TRUE ? valu : 0);
          dst[3] = (b == WEED_TRUE ? valu : 0);
          dst += psize;
          break;
        default:
          break;
        }
      }
      alphaf += irow;
      dst += orow;
    }
  } else if (ipal == WEED_PALETTE_A8) {
    irow -= width;
    alphau = (uint8_t *)weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        valu = alphau[j];
        switch (opal) {
        case WEED_PALETTE_RGBA32:
          dst[j + 3] = 0xFF;
        case WEED_PALETTE_RGB24:
          dst[j] = (r == WEED_TRUE ? valu : 0);
          dst[j + 1] = (g == WEED_TRUE ? valu : 0);
          dst[j + 2] = (b == WEED_TRUE ? valu : 0);
          dst += psize;
          break;
        case WEED_PALETTE_BGRA32:
          dst[j + 3] = 0xFF;
        case WEED_PALETTE_BGR24:
          dst[j] = (b == WEED_TRUE ? valu : 0);
          dst[j + 1] = (g == WEED_TRUE ? valu : 0);
          dst[j + 2] = (r == WEED_TRUE ? valu : 0);
          dst += 3;
          break;
        case WEED_PALETTE_ARGB32:
          dst[j] = 0xFF;
          dst[j + 1] = (r == WEED_TRUE ? valu : 0);
          dst[j + 2] = (g == WEED_TRUE ? valu : 0);
          dst[j + 3] = (b == WEED_TRUE ? valu : 0);
          dst += psize;
          break;
        default:
          break;
        }
      }
      alphau += irow;
      dst += orow;
    }
  } else if (ipal == WEED_PALETTE_A1) {
    width >>= 3;
    irow -= width;
    alphau = (uint8_t *)weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        for (k = 0; k < 8; k++) {
          valu = getbit(alphau[j], k) * 255;
          switch (opal) {
          case WEED_PALETTE_RGBA32:
            dst[j + 3] = 0xFF;
          case WEED_PALETTE_RGB24:
            dst[j] = (r == WEED_TRUE ? valu : 0);
            dst[j + 1] = (g == WEED_TRUE ? valu : 0);
            dst[j + 2] = (b == WEED_TRUE ? valu : 0);
            dst += psize;
            break;
          case WEED_PALETTE_BGRA32:
            dst[j + 3] = 0xFF;
          case WEED_PALETTE_BGR24:
            dst[j] = (b == WEED_TRUE ? valu : 0);
            dst[j + 1] = (g == WEED_TRUE ? valu : 0);
            dst[j + 2] = (r == WEED_TRUE ? valu : 0);
            dst += psize;
            break;
          case WEED_PALETTE_ARGB32:
            dst[j] = 0xFF;
            dst[j + 1] = (r == WEED_TRUE ? valu : 0);
            dst[j + 2] = (g == WEED_TRUE ? valu : 0);
            dst[j + 3] = (b == WEED_TRUE ? valu : 0);
            dst += psize;
            break;
          default:
            break;
          }
        }
      }

      alphau += irow;
      dst += orow;
    }
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  char desc[1024];
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32,
                        WEED_PALETTE_BGRA32, WEED_PALETTE_ARGB32, WEED_PALETTE_END
                       };
  int apalette_list[] = {WEED_PALETTE_AFLOAT, WEED_PALETTE_A8, WEED_PALETTE_A1, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("alpha input", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("output", 0), NULL};

  weed_plant_t *in_params[] = {weed_switch_init("red", "_Red", WEED_TRUE),
                               weed_switch_init("green", "_Green", WEED_TRUE),
                               weed_switch_init("blue", "_Blue", WEED_TRUE),
                               weed_float_init("fmin", "Float Min", 0., -1000000., 1000000.),
                               weed_float_init("fmax", "Float Max", 1., -1000000., 1000000.),
                               NULL
                              };

  weed_plant_t *filter_class;

  weed_set_int_array(in_chantmpls[0], WEED_LEAF_PALETTE_LIST, sizeof(apalette_list) / 4, apalette_list);
  weed_set_int_array(out_chantmpls[0], WEED_LEAF_PALETTE_LIST, sizeof(palette_list) / 4, palette_list);

  filter_class = weed_filter_class_init("alpha_visualizer", "salsaman", 1, WEED_FILTER_PALETTES_MAY_VARY |
                                        WEED_FILTER_IS_CONVERTER,
                                        NULL, NULL, alphav_process, NULL,
                                        in_chantmpls, out_chantmpls,
                                        in_params, NULL);

  snprintf(desc, 1024, "Alpha visualizer can transform a separated alpha layer into RGB(A).\n"
           "It has one input channel which accepts frames containing 1 bit, 8 bit or float data\n"
           "The parameters red, green and blue define which channels in the generated output are set.\n"
           "Setting them all on produces black/white or greyscale output.\n\n"
           "For 1 bit input, the values of the selected output colour channels are set to maximum for a 1 in the input,\n"
           "and to minimum (0) for a 0 in the input.\n"
           "For 8 bit input, the values of the colours in the output are scaled in proportion [0 - 255] in the input.\n"
           "For float alpha input, the 4th and 5th parameters define minimum and maximum for the range of the floats\n"
           "The input values are clamped to within this range, and the output is scaled in proportion to\n"
           "(input - min) / (max - min). If max is <= min then a blank (black) frame is output.\n\n"
           "If the output has alpha, it will be set to opaque.\n");

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

