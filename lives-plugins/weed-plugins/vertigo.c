/////////////////////////////////////////////////////////////////////////////
// Weed vertigo plugin, version 1
// Compiled with Builder version 3.2.1-pre
// autogenerated from script by effecTV
/////////////////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

/////////////////////////////////////////////////////////////////////////////

#define _UNIQUE_ID_ "0XC3E0EEF9299266BD"

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

#include "weed-plugin-utils.c"

//////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdio.h>

static int verbosity = WEED_VERBOSITY_ERROR;
typedef unsigned int RGB32;

typedef struct {
  int dx, dy, sx, sy;
  RGB32 *buffer;
  RGB32 *current_buffer, *alt_buffer;
  double phase;
} sdata_t;

enum {
  P_pinc,
  P_zoom,
};

static void setParams(int video_width, int video_height, sdata_t *sdata,
    double phase_increment, double zoom) {
  double zoomrate = 1. + zoom / 100.;
  double vx, vy, vvx, vvy, ang;
  double dizz = sin(sdata->phase) * 10 + sin(sdata->phase * 1.9 + 5) * 5;
  double x = video_width / 2.;
  double y = video_height / 2.;
  double t = (x * x + y * y) * zoomrate;

  if (video_width > video_height) {
    if (dizz >= 0) {
      if (dizz > x) dizz = x;
      vvx = (x * (x - dizz) + y * y) / t;
    } else {
      if (dizz < -x) dizz = -x;
      vvx = (x * (x + dizz) + y * y) / t;
    }
    vvy = (dizz * y) / t;
  } else {
    if (dizz >= 0) {
      if (dizz > y) dizz = y;
      vvx = (x * x + y * (y - dizz)) / t;
    } else {
      if (dizz < -y) dizz = -y;
      vvx = (x * x + y * (y + dizz)) / t;
    }
    vvy = (dizz * x) / t;
  }

  ang = sin(sdata->phase / 10.) / (100. / zoom / phase_increment);
  vx = cos(ang) * vvx + sin(ang) * vvy;
  vy = cos(ang) * vvy + sin(ang) * vvx;

  sdata->dx = vx * 65536.;
  sdata->dy = vy * 65536.;

  sdata->sx = (-vx * x + vy * y + x) * 65536.;
  sdata->sy = (-vx * y - vy * x + y) * 65536.;

  sdata->phase += phase_increment;
  if (sdata->phase > 5700000.) sdata->phase = 0.;
}

/////////////////////////////////////////////////////////////////////////////

static weed_error_t vertigo_init(weed_plant_t *inst) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);
  sdata_t *sdata;

  double pinc = weed_param_get_value_double(in_params[P_pinc]);
  double zoom = weed_param_get_value_double(in_params[P_zoom]);
  weed_free(in_params);

  if (!(weed_instance_get_flags(inst) & WEED_INSTANCE_UPDATE_GUI_ONLY)) {
    sdata = (sdata_t *)weed_calloc(1, sizeof(sdata_t));
    if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

    if (1) {
      int video_area = width * height;
      size_t psize = pixel_size(pal) * 2;
      
      sdata->buffer = (RGB32 *)weed_calloc(video_area, psize);
      if (!sdata->buffer) {
        weed_free(sdata);
        return WEED_ERROR_MEMORY_ALLOCATION;
      }
      sdata->current_buffer = sdata->buffer;
      sdata->alt_buffer = sdata->buffer + video_area;
    }
    weed_set_voidptr_value(inst, "plugin_internal", sdata);
  }
  else sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  return WEED_SUCCESS;
}


static weed_error_t vertigo_deinit(weed_plant_t *inst) {
  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (sdata) {
    if (sdata->buffer) weed_free(sdata->buffer);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);

  return WEED_SUCCESS;
}


static weed_error_t vertigo_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_chan = weed_get_in_channel(inst, 0);
  weed_plant_t *out_chan = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  unsigned char *src = weed_channel_get_pixel_data(in_chan);
  unsigned char *dst = weed_channel_get_pixel_data(out_chan);
  int pal = weed_channel_get_palette(in_chan);
  int irow = weed_channel_get_stride(in_chan);
  int width = weed_channel_get_width(out_chan);
  int height = weed_channel_get_height(out_chan);
  int orow = weed_channel_get_stride(out_chan);
  int psize = pixel_size(pal);
  sdata_t *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (!sdata) return WEED_ERROR_REINIT_NEEDED;
  else {
    double pinc = weed_param_get_value_double(in_params[P_pinc]);
    double zoom = weed_param_get_value_double(in_params[P_zoom]);
    weed_free(in_params);

    if (1) {
      RGB32 v, z, *p = sdata->alt_buffer, *psrc = (RGB32 *)src;
      
      int video_area = width * height;
      int y, ox, oy, x, i;
      int widthx = width * pixel_size(pal);
      
      setParams(width, height, sdata, pinc, zoom);
      irow /= psize;
      
      for (y = 0; y < height; y++) {
        ox = sdata->sx;
        oy = sdata->sy;
        for (x = 0; x < width; x++) {
          if ((i = (oy >> 16) * width + (ox >> 16)) < 0) i = 0;
          if (i >= video_area) i = video_area;
          z = psrc[irow * y + x];
          v = ((sdata->current_buffer[i] & 0xfcfcff) * 3) + (z & 0xfcfcff);
          p[width * y + x] = (v >> 2) | (z & 0xff000000);
          ox += sdata->dx;
          oy += sdata->dy;
        }
        sdata->sx -= sdata->dy;
        sdata->sy += sdata->dx;
      }
      
      for (y = 0; y < height; y++) {
        weed_memcpy(&dst[orow * y], &sdata->alt_buffer[width * y], widthx);
      }
      
      p = sdata->current_buffer;
      sdata->current_buffer = sdata->alt_buffer;
      sdata->alt_buffer = p;
    }
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  weed_plant_t *filter_class;
  uint64_t unique_id;
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_END};
  weed_plant_t *in_chantmpls[] = {
      weed_channel_template_init("in_channel0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE),
      NULL};
  weed_plant_t *out_chantmpls[] = {
      weed_channel_template_init("out_channel0", 0),
      NULL};
  weed_plant_t *in_paramtmpls[] = {
      weed_float_init("pinc", "_Phase increment", .2, 0., 1.),
      weed_float_init("zoom", "_Zoom", 1., 0., 10.),
      NULL};
  weed_plant_t *pgui;
  int filter_flags = 0;

  verbosity = weed_get_host_verbosity(host_info);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_pinc]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);

  pgui = weed_paramtmpl_get_gui(in_paramtmpls[P_zoom]);
  weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);

  filter_class = weed_filter_class_init("vertigo", "effecTV", 1, filter_flags, palette_list,
    vertigo_init, vertigo_process, vertigo_deinit, in_chantmpls, out_chantmpls, in_paramtmpls, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  if (!sscanf(_UNIQUE_ID_, "0X%lX", &unique_id) || !sscanf(_UNIQUE_ID_, "0x%lx", &unique_id)) {
    weed_set_int64_value(plugin_info, WEED_LEAF_UNIQUE_ID, unique_id);
  }

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

