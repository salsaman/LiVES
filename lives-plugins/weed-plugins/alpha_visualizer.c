/////////////////////////////////////////////////////////////////////////////
// Weed alpha_visualizer plugin, version 1
// Compiled with Builder version 4.0,0-pre
// autogenerated from script alpha_visualizer.script

// released under the GNU GPL version 3 or higher
// see file COPYING or www.gnu.org for details

// (c) salsaman 2023
/////////////////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

/////////////////////////////////////////////////////////////////////////////

#define _UNIQUE_ID_ "0X670BBDECB23F3DBE"

#define NEED_PALETTE_UTILS
#include <weed/weed-plugin-utils.h>
//////////////////////////////////////////////////////////////////


static int verbosity = WEED_VERBOSITY_ERROR;
enum {
  P_r,
  P_g,
  P_b,
  P_fmin,
  P_fmax,
};

static int getbit(uint8_t val, int bit) {
  int x = 1;
  for (int i = 0; i < bit; i++) x *= 2;
  return val & x;
}

/////////////////////////////////////////////////////////////////////////////

static weed_error_t alpha_visualizer_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int inplace = (src == dst);
  const int pal = (const int)weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int iheight = weed_channel_get_height(in_chan);
  int is_threading = weed_is_threading(inst);
  int offset = 0;
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  const int psize = (const int)pixel_size((int)pal);
  int r = weed_param_get_value_boolean(in_params[P_r]);
  int g = weed_param_get_value_boolean(in_params[P_g]);
  int b = weed_param_get_value_boolean(in_params[P_b]);
  double fmin = weed_param_get_value_double(in_params[P_fmin]);
  double fmax = weed_param_get_value_double(in_params[P_fmax]);
  weed_free(in_params);

  if (is_threading) {
    offset = weed_channel_get_offset(out_chan);
    src += offset * irow;
  }

  do {
    width *= psize;
    for (int j = 0; j < height; j++) {
      for (int i = 0; i < width; i += 3) {
        if (pal == WEED_PALETTE_ARGB32) {
          dst[orow * j + i] = src[irow * j + i];
          i++;
        }
        for (int k = 0; k < 3; k++) dst[orow * j + i + k] = 0xFF ^ src[irow * j + i + k];
        if (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32) {
          dst[orow * j + i + 3] = src[irow * j + i + 3];
          i++;
        }
      }
    }
  } while (0);

  return WEED_SUCCESS;
}


WEED_SETUP_START(203, 202) {
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  weed_plant_t *filter_class;
  uint64_t unique_id;
  int palette_list[] = ALL_RGB_PALETTES;
  weed_plant_t *in_chantmpls[] = {
      weed_channel_template_init("in_channel0", 0),
      NULL};
  weed_plant_t *out_chantmpls[] = {
      weed_channel_template_init("out_channel0", WEED_CHANNEL_CAN_DO_INPLACE),
      NULL};
  weed_plant_t *in_paramtmpls[] = {
      weed_switch_init("r", "_Red", WEED_TRUE),
      weed_switch_init("g", "_Green", WEED_TRUE),
      weed_switch_init("b", "_Blue", WEED_TRUE),
      weed_float_init("fmin", "Float Mi_n", 0., -1000000., 1000000.),
      weed_float_init("fmax", "Float Ma_x", 1., -1000000., 1000000.),
      NULL};
  weed_plant_t *pgui;
  int filter_flags = WEED_FILTER_HINT_MAY_THREAD;

  verbosity = weed_get_host_verbosity(host_info);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_fmin]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 1);
  weed_set_double_value(pgui, WEED_LEAF_STEP_SIZE, 1.);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_fmax]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 1);
  weed_set_double_value(pgui, WEED_LEAF_STEP_SIZE, 1.);

  filter_class = weed_filter_class_init("alpha_visualizer", "salsaman", 1, filter_flags, palette_list,
    NULL, alpha_visualizer_process, NULL, in_chantmpls, out_chantmpls, in_paramtmpls, NULL);

  weed_filter_set_description(filter_class, "Alpha visualizer can transform a separated alpha layer into RGB(A).\n"
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

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  if (!sscanf(_UNIQUE_ID_, "0X%lX", &unique_id) || !sscanf(_UNIQUE_ID_, "0x%lx", &unique_id)) {
    weed_set_int64_value(plugin_info, WEED_LEAF_UNIQUE_ID, unique_id);
  }

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

