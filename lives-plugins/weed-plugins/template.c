a// example plugin


///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

// include header files
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional

#include "weed-plugin-utils.c"


///// plugin internal functions go here  ///////









///// plugin standard functions go here ////////////////////


// OPTIONAL FUNCTION
static weed_error_t myplugin_init(weed_plant_t *inst) {
  // EXAMPLE:

  // get any data needed from in_channels, out_channels, in_params, out_params (see below, myplugin_process() for examples)
  weed_plant_t *in_channel = weed_get_in_channel(inst);
  int palette = weed_channel_get_palette(in_channel);
  size_t pixel_size = pixel_size(palette);

  // alloc somewhere to store static data for the instance
  // static_data should be a struct to hold static values for the instance
  static_data *sdata = (static_data *)weed_malloc(sizeof(static_data));
  if (sdata == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->map = weed_calloc(width * height, pixel_size * 2);
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
  if (sdata) {
    weed_free(sdata->map);
    weed_free(sdata);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}





static weed_error_t myplugin_process(weed_plant_t *inst, weed_timecode_t tc) {

  // get the in_channel (if applicable) /////////////////
  weed_plant_t *in_channel = weed_get_in_channel(inst);

  // OR: if we have multiple in_channels
  weed_size_t num_ins = weed_leaf_num_elements(inst, WEED_LEAF_IN_CHANNELS);
  weed_plant_t **pin_channels = weed_get_plantptr_array(inst, WEED_LEAF_IN_CHANNELS, NULL);
  weed_plant_t *in_channel = pin_channels[0];
  weed_plant_t *in_channel1 = pin_channels[1];
  // etc....for all in_channels


  // get the out_channel (if applicable) /////////////////////////
  weed_plant_t *out_channel = weed_get_out_channel(inst);

  // OR: if we have multiple out_channels
  weed_size_t num_outs = weed_leaf_num_elements(inst, WEED_LEAF_IN_CHANNELS);
  weed_plant_t **pout_channels = weed_get_plantptr_array(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t *out_channel = pout_channels[0];
  weed_plant_t *out_channel1 = pout_channels[1];
  // etc....for all out_channels


  // for VIDEO channels:  ///////////////////////////////////////////////////////////

  // get the palette (see weed-palettes.h) [if we have no in_channels, we can get this from out_channel instead]
  int palette = weed_channel_get_palette(in_channel);
  // palettes for each channel will match unless WEED_CHANNEL_PALETTE_CAN_VARY was set in channel_template flags

  // get the pixel_data ////////////////////////////////////

  // in_channel pixel_data (if we have one in_channel only):
  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  // OR: (if palette is planar, ie. the palette name ends with 'P')
  unsigned char **psrc = weed_get_voidptr_array(in_channel, WEED_LEAF_PIXEL_DATA, NULL); // if we have in_channels

  // out_channel pixel_data (if we have one out_channel only)
  unsigned char *dst = weed_channel_get_pixel_daya(out_channel);
  // OR: (if palette is planar, ie. the palette name ends with 'P')
  unsigned char **pdst = weed_get_voidptr_array(out_channel, WEED_LEAF_PIXEL_DATA, NULL); // if we have out_channels

  // if we have multiple VIDEO channels:
  unsigned char *src1 = weed_channel_get_pixel_data(in_channel1);
  // OR: (if palette is planar, ie. the palette name ends with 'P')
  unsigned char **psrc1 = weed_get_voidptr_array(in_channel1, WEED_LEAF_PIXEL_DATA, NULL); // if we have in_channels
  // out_channel pixel_data:
  unsigned char *dst1 = weed_channel_get_pixel_data(out_channel1);
  // OR: (if palette is planar, ie. the palette name ends with 'P')
  unsigned char **pdst1 = weed_get_voidptr_array(out_channel1, WEED_LEAF_PIXEL_DATA, NULL); // if we have out_channels
  // etc for all VIDEO channels....

  // frame sizes, width X height. If we have no in_channels we can get this out_channel instead
  // channel width in (macro)pixels  [for planar palettes, this is the width of the first plane]
  int width = weed_channel_get_width(in_channel);
  // channel height in rows          [for planar palettes, this is the height of the first plane]
  int height = weed_channel_get_height(in_channel);
  // (sizes for each channel will match unless WEED_CHANNEL_SIZE_CAN_VARY was set in channel_template flags)

  //  rowstrides
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);
  // OR: (if palette is planar, ie. the palette name ends with 'P')
  int *irowstrides = weed_get_int_array(in_channel, WEED_LEAF_ROWSTRIDES, NULL); // if we have in_channels
  int *orowstrides = weed_get_int_array(out_channel, WEED_LEAF_ROWSTRIDES, NULL); // if we have out_channels
  // (rowstrides for each channel will match unless WEED_CHANNEL_SIZE_CAN_VARY or WEED_CHANNEL_PALETTE_CAN_VARY
  // was set in the channel_template)

  // if the palette is WEED_PALETTE_YUV*:   (see weed-palettes.h)
  int clamping = weed_get_int_value(in_channel, WEED_LEAF_YUV_CLAMPING, NULL);
  int sampling = weed_get_int_value(in_channel, WEED_LEAF_YUV_SAMPLING, NULL);
  int subspace = weed_get_int_value(in_channel, WEED_LEAF_YUV_SUBSPACE, NULL);
  // (values for each YUV channel will match unless WEED_CHANNEL_PALETTE_CAN_VARY was set in the channel_template)


  // for AUDIO channels: ////////////////////////////////////
  float *fsrc = (float *)weed_get_voidptr_value(in_channel, WEED_LEAF_AUDIO_DATA, NULL); // if we have audio in_channels
  float *fdst = (float *)weed_get_voidptr_value(out_channel, WEED_LEAF_AUDIO_DATA, NULL); // if we have audio out_channels
  // OR: for multiple AUDIO channels:
  float *fsrc1 = (float *)weed_get_voidptr_value(in_channel1, WEED_LEAF_AUDIO_DATA, NULL); // if we have audio in_channels
  float *fdst1 = (float *)weed_get_voidptr_value(out_channel1, WEED_LEAF_AUDIO_DATA, NULL); // if we have audio out_channels
  // etc for all AUDIO channels....

  int nsamps = weed_get_int_value(in_channel, WEED_LEAF_AUDIO_DATA_LENGTH, NULL); // if no in_channels, use out_channel
  int nchans = weed_get_int_value(in_channel, WEED_LEAF_AUDIO_CHANNELS, NULL); // if no in_channels, use out_channel
  int arate = weed_get_int_value(in_channel, WEED_LEAF_AUDIO_RATE, NULL); // if no in_channels, use out_channel
  int interleaved = weed_get_boolean_value(in_channel, WEED_LEAF_AUDIO_INTERLEAF, NULL);


  // if we set WEED_CHANNEL_CAN_DO_INPLACE on an out_channel, check if it's inplace:
  int inplace = (dst == src);
  // OR: (if palette is planar, ie. the palette name ends with 'P')
  int inplace = (pdst == psrc);

  // if we have multiple out_channels:
  int inplace1 = (dst1 == src1);   // if src1 exists
  // OR: (if palette is planar, ie. the palette name ends with 'P')
  int inplace1 = (psrc1 == pdst1); // if psrc1 exists


  // if we set WEED_FILTER_HINT_MAY_THREAD, check if we are threading:
  int is_master_thread = 1;
  int real_height = height;
  int offset = 0;

  if (weed_lis_threading(inst)) {
    // we are threading...

    // get the height of our "slice"
    height = weed_channel_get_height(out_channel);

    // real height of out_channel if we need it, this is also the src height unless we set WEED_CHANNEL_SIZE_MAY_VARY
    real_height = weed_channel_get_real_height(out_channel);

    // get the offset. We must only write to destination rows between offs and (offs + height - 1)
    offset = weed_channel_get_offset(out_channel);

    // only the master thread may update the plugin internal state
    if (offset > 0) is_master_thread = 0;

    // adjust dst: ////////////////////////////////////////////////////////////////////////
    dst += orowstride * offs;

    // OR: (if palette is planar, ie. the palette name ends with 'P')
    pdst[0] += orowstrides[0] * offs;
    pdst[1] += orowstrides[1] * offs; // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P, use (offs / 2)
    pdst[2] += orowstrides[2] * offs;  // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P, use (offs / 2)
    // for WEED_PALETTE_YUVA4444P only:
    pdst[3] += orowstrides[3] * offs;

    // OR: for multiple out_channels:
    dst[0] += orowstride * offs;
    dst[1] += orowstride * offs;
    // etc for all out_channels...

    // OR: for multiple out_channels (if palette is planar, ie. the palette name ends with 'P')
    pdst[0][0] += orowstrides[0] * offs;
    pdst[0][1] += orowstrides[1] * offs; // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P, use (offs / 2)
    pdst[0][2] += orowstrides[2] * offs; // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P, use (offs / 2)
    // for WEED_PALETTE_YUVA4444P only:
    pdst[0][3] += orowstrides[3] * offs;

    pdst[1][0] += orowstrides[0] * offs;
    pdst[1][1] += orowstrides[1] * offs; // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P, use (offs / 2)
    pdst[1][2] += orowstrides[2] * offs; // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P, use (offs / 2)
    // for WEED_PALETTE_YUVA4444P only:
    pdst[1][3] += orowstrides[3] * offs;
    // etc for all out_channels...
  }


  // in parameter array (if applicable) ////////////////////////////////////
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  // value of first in_parameter (example)
  double freq = weed_get_double_value(in_params[0], WEED_LEAF_VALUE, NULL);

  // etc...


  // out parameter array (if applicable) ////////////////////////////////////
  weed_plant_t **out_params = weed_get_plantptr_array(inst, WEED_LEAF_OUT_PARAMETERS, NULL);
  // etc....
  // we may set WEED_LEAF_VALUE for each out_param (if we are threading, only the master thread may do so)


  // process one frame or one audio block.......

  // if check each channel before processing it:
  if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE) {
    // plugin must ignore this channel
  }
  //else {
  //.....

  // processing examples: //////////////////////////////




  // EXAMPLE1:
  // packed (non-planar) palettes (palette name does NOT end with 'P')
  int x, y;
  unsigned char *d = dst;
  size_t pixel_size = 3;

  // pixel_size should be set to 4 for palettes WITH alpha
  // pixel_size should be set to 6 for WEED_PALETTE_YUV411

  for (d = dst; d < dst + (height - 1) * orowstride; d += orowstride) {
    for (x = 0; x < width * pixel_size; x += pixel_size) {
      // write (pixel_size) bytes to d[x]
    }
  }

  // for multiple out_channels, process dst1, dst2, etc.


  // EXAMPLE2:
  // planar palettes (palette name ends with 'P')
  int x, y;
  unsigned char *d0 = pdst[0];
  unsigned char *d1 = pdst[1];
  unsigned char *d2 = pdst[2];
  // for WEED_PALETTE_YUVA4444P only:
  unsigned char *d3 = pdst[3];

  // pixel_size should be set to 4 for palettes WITH alpha

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      // set (unsigned char) d0[y][x], d1[y][x], d2[y][x] and for WEED_PALETTE_YUVA4444P, d3[y][x]
      // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P:
      //    set d1[y][x>>1] and d2[y][x>>1] instead (may be done only on every other loop)
    }
    d0 += orowstrides[0];
    d1 += orowstrides[1]; // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P do this only if (y % 2) == 0
    d2 += orowstrides[2]; // for WEED_PALETTE_YUV420P or WEED_PALETTE_YVU420P do this only if (y % 2) == 0
    // for WEED_PALETTE_YUVA4444P only:
    d3 += orowstrides[3];
  }

  // for multiple out_channels, process pdst1, pdst2, etc.


  // free any arrays created
  if (in_channels)  weed_free(in_channels);
  if (out_channels) weed_free(out_channels);
  if (irowstrides)  weed_free(irowstrides);
  if (orowstrides)  weed_free(orowstrides);
  if (in_params)    weed_free(in_params);
  if (out_params)   weed_free(out_params);

  if (psrc) weed_free(psrc);
  if (pdst) weed_free(pdst);

  if (psrc1) weed_free(psrc1);
  if (pdst1) weed_free(pdst1);
  // etc...

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
  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", chan_flags), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", chan_flags), NULL};

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
  filter_class = weed_filter_class_init("plugin name", "author", filter_version, filter_flags, palette_list, myplugin_init,
                                        myplugin_process, myplugin_deinit, in_chantmpls, out_chantmpls, in_params, out_params);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;




// audio setup example:

WEED_SETUP_START(200, 200) {
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("in channel 0", 0), NULL};

  weed_plant_t *in_params[] = {weed_float_init("freq", "_Frequency", 2000., 0.0, 22000.0), NULL};
  weed_plant_t *out_params[] = {weed_out_param_float_init(WEED_LEAF_VALUE, 0., 0., 1.), NULL};

  weed_plant_t *filter_class = weed_filter_class_init("audio fft analyser", "salsaman", 1, 0, NULL,
                               NULL, audio_process, NULL, in_chantmpls, NULL, in_params, out_params);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, "Audio example for Weed");

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;




// OPTIONAL FUNCTION
WEED_DESETUP_START {
  // do any cleanup for the whole plugin here.
}
WEED_DESETUP_END;

