// example plugin

// include header files
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // to be replaced by libweed-plugin-utils


///// plugin internal functions  ///////









///// plugin standard functions ////////////////////


// OPTIONAL FUNCTION
static weed_error_t myplugin_init(weed_plant_t *inst) {
  // example:

  weed_plant_t *in_channel;
  int height, width;

  // alloc somewhere to store static data for the instance
  // static_data should be a struct to hold static values for the instance
  static_data *sdata = (static_data *)weed_malloc(sizeof(static_data));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  // get width X height of in_channel
  in_channel = weed_get_plantptr_value(inst, "in_channels", NULL);
  height = weed_get_int_value(in_channel, "height", NULL);
  width = weed_get_int_value(in_channel, "width", NULL);

  // create some example data
  sdata->map = weed_calloc(width * height, PIXEL_SIZE * 2);
  if (sdata->map == NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_set_voidptr_value(inst, "plugin_internal", sdata); // we can use any leaf with a key starting "plugin_"

  return WEED_SUCCESS;
}




// OPTIONAL FUNCTION
static weed_error_t myplugin_deinit(weed_plant_t *inst) {
  // clean up anything we allocated
  static_data *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sdata != NULL) {
    weed_free(sdata->map);
    weed_free(sdata);
  }

  return WEED_SUCCESS;
}





static weed_error_t myplugin_process(weed_plant_t *inst, weed_timecode_t tc) {

  // get the in channel(s)
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", NULL);

  // get the out channel(s)
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, "out_channels", NULL);

  // get the pixel_data
  unsigned char *src = weed_get_voidptr_value(in_channel, "pixel_data", NULL), *osrc = src;
  unsigned char *dst = weed_get_voidptr_value(out_channel, "pixel_data", NULL);

  // input palette
  int pal = weed_get_int_value(in_channel, "current_palette", NULL);

  // pixel size of in frame
  int width = weed_get_int_value(in_channel, "width", NULL), widthx;
  int height = weed_get_int_value(in_channel, "height", NULL);

  // rowstrides
  int irowstride = weed_get_int_value(in_channel, "rowstrides", NULL);
  int orowstride = weed_get_int_value(out_channel, "rowstrides", NULL);


  // for an audio channel:
  float *src = (float *)weed_get_voidptr_value(in_channel, "audio_data", NULL);

  // in and out parameter arrays
  weed_plant_t **in_params = weed_get_plantptr_array(inst, "in_parameters", NULL);
  weed_plant_t *out_param = weed_get_plantptr_value(inst, "out_parameters", NULL);

  // value of first in parameter
  double freq = weed_get_double_value(in_params[0], "value", NULL);

  // etc.

  // process one frame or one audio block...

  //...

  weed_free(in_params);
  weed_free(out_params);

  return WEED_SUCCESS;
}





// video setup example:

WEED_SETUP_START(200, 200) {
  // list the palettes we can use
  int palette_list1[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_END};

  int palette_list2[] = {WEED_PALETTE_BGR24, WEED_PALETTE_RGB24, WEED_PALETTE_YUV888, WEED_PALETTE_YUVA8888,
                         WEED_PALETTE_RGBA32, WEED_PALETTE_ARGB32, WEED_PALETTE_BGRA32, WEED_PALETTE_UYVY,
                         WEED_PALETTE_YUYV, WEED_PALETTE_END
                        };


  // set up channel templates
  int chan_flags = 0;
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", chan_flags, palette_list), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", chan_flags, palette_list), NULL};

  weed_plant_t *in_params[4];
  weed_plant_t **out_params = NULL;

  weed_plant_t *filter_class;

  int filter_version = 1;
  int filter_flags = 0;


  // set some parameter templates
  in_params[0] = weed_integer_init("threshold", "Pixel _threshold", 162, 0, 255);
  in_params[1] = weed_string_list_init("mode", "Colour _mode", 0, modes);
  in_params[2] = weed_string_list_init("font", "_Font", 0, fonts);
  in_params[3] = NULL;

  // etc...



  // add the filter class
  filter_class = weed_filter_class_init("plugin name", "author", filter_version, filter_flags, myplugin_init,
                                        myplugin_process, myplugin_deinit, in_chantmpls, out_chantmpls, in_params,
                                        out_params);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;




// audio setup example:

WEED_SETUP_START(200, 200) {
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("in channel 0", 0), NULL};

  weed_plant_t *in_params[] = {weed_float_init("freq", "_Frequency", 2000., 0.0, 22000.0), NULL};
  weed_plant_t *out_params[] = {weed_out_param_float_init("value", 0., 0., 1.), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("audio fft analyser", "salsaman", 1, 0, NULL, &fftw_process,
                               NULL, in_chantmpls, NULL, in_params, out_params);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_string_value(filter_class, "description", "Fast Fourier Transform for audio");

  weed_set_int_value(plugin_info, "version", package_version);
}
WEED_SETUP_END;




// OPTIONAL FUNCTION
WEED_DESETUP_START {
  // do any cleanup for the whole plugin here.
}
WEED_DESETUP_END;

