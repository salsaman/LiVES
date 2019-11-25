// freenect.c
// weed plugin
// (c) G. Finch (salsaman) 2014
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

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

#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include "libfreenect.h"

typedef struct {
  freenect_context *f_ctx;
  freenect_device *f_dev;
  uint16_t *depth_front;
  uint16_t *depth_back;
  uint8_t *rgb_front;
  uint8_t *rgb_back;
  pthread_mutex_t backbuf_mutex;
  pthread_t usb_thread;
  int die;
} _sdata;


static void *idle_loop(void *user_data) {
  _sdata *sd = (_sdata *)(user_data);
  int res;

  while (!sd->die) {
    res = freenect_process_events(sd->f_ctx);
    if (res < 0 && res != -10) {
      fprintf(stderr, "\nFreenect - Error %d received from libusb - aborting.\n", res);
      break;
    }
  }
  return NULL;
}


static void rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp) {
  _sdata *sd = (_sdata *)freenect_get_user(dev);

  pthread_mutex_lock(&sd->backbuf_mutex);

  // swap buffers
  sd->rgb_back = sd->rgb_front;
  freenect_set_video_buffer(dev, sd->rgb_back);
  sd->rgb_front = (uint8_t *)rgb;

  //got_rgb++;
  pthread_mutex_unlock(&sd->backbuf_mutex);
}


static void depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp) {
  _sdata *sd = (_sdata *)freenect_get_user(dev);

  pthread_mutex_lock(&sd->backbuf_mutex);

  // swap buffers
  sd->depth_back = sd->depth_front;
  freenect_set_depth_buffer(dev, sd->depth_back);
  sd->depth_front = (uint16_t *)v_depth;

  pthread_mutex_unlock(&sd->backbuf_mutex);
}


static int lives_freenect_prep(_sdata *sdata) {
  int nr_devices;
  int user_device_number = 0;

  if (freenect_init(&sdata->f_ctx, NULL) < 0) {
    fprintf(stderr, "freenect_init() failed\n");
    return 1;
  }

  freenect_set_log_level(sdata->f_ctx, FREENECT_LOG_WARNING);
  freenect_select_subdevices(sdata->f_ctx, (freenect_device_flags)(FREENECT_DEVICE_CAMERA));

  nr_devices = freenect_num_devices(sdata->f_ctx);
  fprintf(stderr, "Freenect: Number of devices found: %d\n", nr_devices);

  /*  if (argc > 1)
    user_device_number = atoi(argv[1]);
  */

  if (nr_devices < 1) {
    freenect_shutdown(sdata->f_ctx);
    return 0;
  }

  if (freenect_open_device(sdata->f_ctx, &sdata->f_dev, user_device_number) < 0) {
    fprintf(stderr, "Freenect: Could not open device\n");
    freenect_shutdown(sdata->f_ctx);
    return 0;
  }

  freenect_set_user(sdata->f_dev, sdata);

  return 1;
}


static weed_error_t lives_freenect_init(weed_plant_t *inst) {
  _sdata *sd = (_sdata *)weed_malloc(sizeof(_sdata));
  if (sd == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sd->depth_back = (uint16_t *)weed_malloc(640 * 480 * 2);
  if (sd->depth_back == NULL) {
    weed_free(sd);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sd->depth_front = (uint16_t *)weed_malloc(640 * 480 * 2);
  if (sd->depth_front == NULL) {
    weed_free(sd->depth_back);
    weed_free(sd);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sd->rgb_back = (uint8_t *)weed_malloc(640 * 480 * 3);
  if (sd->rgb_back == NULL) {
    weed_free(sd->depth_back);
    weed_free(sd->depth_front);
    weed_free(sd);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sd->rgb_front = (uint8_t *)weed_malloc(640 * 480 * 3);
  if (sd->rgb_front == NULL) {
    weed_free(sd->depth_back);
    weed_free(sd->depth_front);
    weed_free(sd->rgb_back);
    weed_free(sd);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_set_voidptr_value(inst, "plugin_internal", sd);

  if (!lives_freenect_prep(sd)) {
    weed_free(sd->depth_back);
    weed_free(sd->depth_front);
    weed_free(sd->rgb_back);
    weed_free(sd->rgb_front);
    weed_free(sd);
    return WEED_ERROR_PLUGIN_INVALID;
  }

  pthread_mutex_init(&sd->backbuf_mutex, NULL);

  freenect_set_depth_callback(sd->f_dev, depth_cb);
  freenect_set_video_callback(sd->f_dev, rgb_cb);

  freenect_set_video_mode(sd->f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
  freenect_set_depth_mode(sd->f_dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_REGISTERED));

  freenect_set_video_buffer(sd->f_dev, sd->rgb_back);
  freenect_set_depth_buffer(sd->f_dev, sd->depth_back);

  freenect_start_depth(sd->f_dev);
  freenect_start_video(sd->f_dev);

  // kick off a thread to do usb stuff
  sd->die = 0;
  pthread_create(&sd->usb_thread, NULL, idle_loop, sd);

  return WEED_SUCCESS;
}


static weed_error_t lives_freenect_deinit(weed_plant_t *inst) {
  _sdata *sd = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  // kill usb thread
  sd->die = 1;
  pthread_join(sd->usb_thread, NULL);

  if (sd->f_dev != NULL) {
    freenect_stop_depth(sd->f_dev);
    freenect_stop_video(sd->f_dev);
    freenect_close_device(sd->f_dev);
  }

  if (sd->f_ctx != NULL) freenect_shutdown(sd->f_ctx);

  weed_free(sd->depth_front);
  weed_free(sd->depth_back);
  weed_free(sd->rgb_back);
  weed_free(sd->rgb_front);
  weed_free(sd);
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t lives_freenect_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  weed_plant_t **outs = weed_get_plantptr_array(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t *out_channel = outs[0];
  weed_plant_t *out_alpha = outs[1];

  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dsta = weed_get_voidptr_value(out_alpha, WEED_LEAF_PIXEL_DATA, NULL);

  _sdata *sd = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  int width = weed_get_int_value(out_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);
  int pal = weed_get_int_value(out_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int rowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);

  int cmin, cmax, *ccol;

  int offs, psize = 3;

  int red = 0, green = 1, blue = 2, alpha = 3;

  unsigned char *rgb = sd->rgb_front;
  uint16_t *depth = sd->depth_front;

  register int i, j;

  cmin = weed_get_int_value(in_params[0], WEED_LEAF_VALUE, NULL);
  cmax = weed_get_int_value(in_params[1], WEED_LEAF_VALUE, NULL);
  ccol = weed_get_int_array(in_params[2], WEED_LEAF_VALUE, NULL);

  if (pal != WEED_PALETTE_RGB24 && pal != WEED_PALETTE_BGR24) psize = 4;

  if (pal == WEED_PALETTE_BGR24 || pal == WEED_PALETTE_BGRA32) {
    red = 2;
    blue = 0;
  }

  if (pal == WEED_PALETTE_ARGB32) {
    alpha = 0;
    red = 1;
    green = 2;
    blue = 3;
  }

  offs = rowstride - width * psize;

  //fprintf(stderr, "min %d max %d\n", cmin, cmax);

  pthread_mutex_lock(&sd->backbuf_mutex);
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      if (*depth >= cmax || *depth < cmin) {
        dst[red] = ccol[0];
        dst[green] = ccol[1];
        dst[blue] = ccol[2];
        if (psize == 4) dst[alpha] = 0;
      } else {
        dst[red] = rgb[0];
        dst[green] = rgb[1];
        dst[blue] = rgb[2];
        if (psize == 4) dst[alpha] = 255;
      }

      dst += psize;
      rgb += 3;

      if (dsta != NULL) {
        *(dsta++) = (float) * depth;
      }

      depth++;
    }
    dst += offs;
  }

  pthread_mutex_unlock(&sd->backbuf_mutex);

  weed_free(outs);
  weed_free(ccol);
  weed_free(in_params);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32,
                        WEED_PALETTE_BGRA32, WEED_PALETTE_ARGB32, WEED_PALETTE_END
                       };
  int apalette_list[] = {WEED_PALETTE_AFLOAT, WEED_PALETTE_END};

  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0),
                                   weed_channel_template_init("depth", WEED_CHANNEL_OPTIONAL), NULL
                                  };

  weed_plant_t *in_params[] = {weed_integer_init("minthresh", "Cut depth (cm) <", 0, 0, 65535),
                               weed_integer_init("maxthresh", "Cut depth (cm) >=", 65536, 0, 65536),
                               weed_colRGBi_init("ccol", "_Replace with colour", 0, 0, 0),
                               NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("freenect", "salsaman", 1, WEED_FILTER_PALETTES_MAY_VARY, NULL,
                               lives_freenect_init,
                               lives_freenect_process,
                               lives_freenect_deinit,
                               NULL, out_chantmpls, in_params, NULL);

  weed_set_int_value(out_chantmpls[0], WEED_LEAF_WIDTH, 640);
  weed_set_int_value(out_chantmpls[0], WEED_LEAF_HEIGHT, 480);
  weed_set_int_array(out_chantmpls[0], WEED_LEAF_PALETTE_LIST, sizeof(palette_list), palette_list);
  weed_set_int_array(out_chantmpls[0], WEED_LEAF_PALETTE_LIST, sizeof(apalette_list), apalette_list);

  weed_set_double_value(filter_class, WEED_LEAF_TARGET_FPS, 25.);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;

