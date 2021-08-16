// kaleidoscope.c
// weed plugin
// (c) G. Finch (salsaman) 2013
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <math.h>

#define FIVE_PI3 5.23598775598f
#define FOUR_PI3 4.18879020479f

#define THREE_PI2 4.71238898038f

#define TWO_PI 6.28318530718f
#define TWO_PI3 2.09439510239f

#define ONE_PI2 1.57079632679f
#define ONE_PI3 1.0471975512f

#define RT2 1.41421356237f
#define RT3  1.73205080757f //sqrt(3)
#define RT32 0.86602540378f //sqrt(3)/2

#define RT322 0.43301270189f

typedef struct {
  float angle;
  weed_timecode_t old_tc;
  int revrot;
  int owidth;
  int oheight;
} sdata;


#define calc_angle(y, x) ((x) > 0. ? ((y) >= 0. ? atanf((y) / (x)) : TWO_PI + atanf((y) / (x))) \
			  : ((x) < 0. ? atanf((y) / (x)) + M_PI : ((y) > 0. ? ONE_PI2 : THREE_PI2)))


#define calc_dist(x, y) (sqrtf((x) * (x) + (y) * (y)))


static void calc_center(float side, float j, float i, float *x, float *y) {
  // find nearest hex center
  int gridx, gridy;

  float secx, secy;

  float sidex = side * RT3; // 2 * side * cos(30)
  float sidey = side * 1.5; // side + side * sin(30)

  float hsidex = sidex / 2., hsidey = sidey / 2.;

  i -= side / FIVE_PI3;

  if (i > 0.) i += hsidey;
  else i -= hsidey;
  if (j > 0.) j += hsidex;
  else j -= hsidex;

  // find the square first
  gridy = i / sidey;
  gridx = j / sidex;

  // center
  *y = gridy * sidey;
  *x = gridx * sidex;

  secy = i - *y;
  secx = j - *x;

  if (secy < 0.) secy += sidey;
  if (secx < 0.) secx += sidex;

  if (!(gridy & 1)) {
    // even row (inverted Y)
    if (secy > (sidey - (hsidex - secx) * RT322)) {
      *y += sidey;
      *x -= hsidex;
    } else if (secy > sidey - (secx - hsidex) * RT322) {
      *y += sidey;
      *x += hsidex;
    }
  } else {
    // odd row, center is left or right (Y)
    if (secx <= hsidex) {
      if (secy < (sidey - secx * RT322)) {
        *x -= hsidex;
      } else *y += sidey;
    } else {
      if (secy < sidey - (sidex - secx) * RT322) {
        *x += hsidex;
      } else *y += sidey;
    }
  }
}


static inline void rotate(float r, float theta, float angle, float *x, float *y) {
  theta += angle;
  if (theta < 0.) theta += TWO_PI;
  else if (theta >= TWO_PI) theta -= TWO_PI;
  *x = r * cosf(theta);
  *y = r * sinf(theta);
}


static int put_pixel(uint8_t *src, uint8_t *dst, int psize, float angle, float theta, float r, int irowstride, int hheight,
                     int hwidth) {
  // dest point is at i,j; r tells us which point to copy, and theta related to angle gives us the transform

  // return 0 if src is oob

  float adif = theta - angle;
  float stheta;

  int sx, sy;

  if (adif < 0.) adif += TWO_PI;
  else if (adif >= TWO_PI) adif -= TWO_PI;

  theta -= angle;

  if (theta < 0.) theta += TWO_PI;
  else if (theta > TWO_PI) theta -= TWO_PI;

  if (adif < ONE_PI3) {
    stheta = theta;
  }

  else if (adif < TWO_PI3) {
    // get coords of src point
    stheta = TWO_PI3 - theta;
  }

  else if (adif < M_PI) {
    // get coords of src point
    stheta = theta - TWO_PI3;
  }

  else if (adif < FOUR_PI3) {
    // get coords of src point
    stheta = FOUR_PI3 - theta;
  }

  else if (adif < FIVE_PI3) {
    // get coords of src point
    stheta = theta - FOUR_PI3;
  } else {
    // get coords of src point
    stheta = TWO_PI - theta;
  }

  stheta += angle;

  sx = r * cosf(stheta) + .5;
  sy = r * sinf(stheta) + .5;

  if (sy < -hheight || sy >= hheight || sx < -hwidth || sx >= hwidth) {
    return 0;
  }

  weed_memcpy(dst, &src[sx * psize - sy * irowstride], psize);
  return 1;
}


static weed_error_t kal_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  float xangle = sdata->angle;
  weed_timecode_t old_tc = sdata->old_tc;

  weed_plant_t *in_channel = weed_get_in_channel(inst, 0),
                *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  unsigned char *src = weed_channel_get_pixel_data(in_channel);
  unsigned char *dst = weed_channel_get_pixel_data(out_channel);

  float theta, r, delta, last_theta = 0., last_r = 0.;

  float x, y, a, b, last_x = 0., last_y = 0.;

  float side, fi, fj;

  float anglerot = 0.;
  double dtime, sfac, angleoffs;

  int width = weed_channel_get_width(in_channel), hwidth = width >> 1;
  int height = weed_channel_get_height(in_channel), hheight = height >> 1;
  int palette = weed_channel_get_palette(in_channel);
  int irowstride = weed_channel_get_stride(in_channel);
  int orowstride = weed_channel_get_stride(out_channel);
  int psize = pixel_size(palette);

  int sizerev;

  int start, end;

  int upd = 1;

  int i, j, jj;

  if (width < height) side = width / 2. / RT32;
  else side = height / 2.;
  sfac = log(weed_param_get_value_double(in_params[0])) / 2.;

  angleoffs = weed_param_get_value_double(in_params[1]);

  if (old_tc != 0 && timestamp > old_tc) {
    anglerot = (float)weed_param_get_value_double(in_params[2]);
    dtime = (double)(timestamp - old_tc) / (double)WEED_TICKS_PER_SECOND;
    anglerot *= (float)dtime;
    while (anglerot >= TWO_PI) anglerot -= TWO_PI;
  }

  if (weed_param_get_value_boolean(in_params[4]) == WEED_FALSE) anglerot = -anglerot;

  sizerev = weed_param_get_value_boolean(in_params[5]);

  weed_free(in_params);

  xangle += (float)angleoffs / 360. * TWO_PI;
  if (xangle >= TWO_PI) xangle -= TWO_PI;

  if (sdata->owidth != width || sdata->oheight != height) {
    if (sizerev == WEED_TRUE && sdata->owidth != 0 && sdata->oheight != 0)
      sdata->revrot = 1 - sdata->revrot;
    sdata->owidth = width;
    sdata->oheight = height;
  }

  if (sdata->revrot) anglerot = -anglerot;

  side *= (float)sfac;

  src += hheight * irowstride + hwidth * psize;

  start = -hheight;
  end = hheight;

  // new threading arch
  if (weed_is_threading(inst)) {
    int offset = weed_channel_get_offset(out_channel);
    int dheight = weed_channel_get_height(out_channel);

    if (offset > 0) upd = 0;

    start += offset;
    dst += offset * orowstride;
    end = start + dheight;
  }

  delta = xangle - ONE_PI2;

  for (i = start; end - i > 0; i++) {
    fi = (float)i;
    jj = orowstride * (i - start);
    last_x = last_y = 0;
    last_theta = last_r = 0.;
    for (j = -hwidth; hwidth - j > 0; j++) {
      // first find the nearest hex center to our input point
      // hexes are rotating about the origin, this is equivalent to the point rotating
      // in the opposite sense
      fj = (float)j;
      theta = calc_angle(fi, fj); // get angle of this point from origin
      r = calc_dist(fi, fj); // get dist of point from origin
      rotate(r, theta, -delta, &a, &b); // rotate point around origin

      // find nearest hex center and angle to it
      calc_center(side, a, b, &x, &y);

      // the hexes turn as they orbit, so calculating the angle to the center, we add the
      // rotation amount to get the final mapping
      if (x == last_x && y == last_y) {
        theta = last_theta;
        r = last_r;
      } else {
        last_x = x;
        last_y = y;
        last_theta = theta = calc_angle(y, x);
        last_r = r = calc_dist(x, y);
      }

      rotate(r, theta, delta, &a, &b);

      theta = calc_angle(fi - b, fj - a);
      r = calc_dist(b - fi, a - fj);

      if (r < 10.) r = 10.;

      if (!put_pixel(src, &dst[jj], psize, xangle, theta, r, irowstride, hheight, hwidth)) {
        if (palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_BGR24) {
          weed_memset(&dst[jj], 0, 3);
        } else if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32) {
          weed_memset(&dst[jj], 0, 3);
          dst[jj + 3] = 255;
        } else if (palette == WEED_PALETTE_ARGB32) {
          dst[jj] = 255;
          weed_memset(&dst[jj + 1], 0, 3);
        }
      }
      jj += psize;
    }
  }

  if (upd) {
    sdata->angle += anglerot * TWO_PI;
    if (sdata->angle >= TWO_PI) sdata->angle -= TWO_PI;
    else if (sdata->angle < 0.) sdata->angle += TWO_PI;
    sdata->old_tc = timestamp;
  }

  return WEED_SUCCESS;
}


static weed_error_t kal_init(weed_plant_t *inst) {
  sdata *sd = (sdata *)weed_malloc(sizeof(sdata));
  if (!sd) return WEED_ERROR_MEMORY_ALLOCATION;

  sd->angle = 0.;
  sd->old_tc = 0;
  sd->revrot = 0;
  sd->owidth = sd->oheight = 0;

  weed_set_voidptr_value(inst, "plugin_internal", sd);

  return WEED_SUCCESS;
}


static weed_error_t kal_deinit(weed_plant_t *inst) {
  sdata *sd = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sd) weed_free(sd);
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_RGB_PALETTES;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_float_init("szln", "_Size (log)", 5.62, 2., 10.),
                               weed_float_init(WEED_LEAF_OFFSET, "_Offset angle", 0., 0., 359.),
                               weed_float_init("rotsec", "_Rotations per second", 0.2, 0., 4.),
                               weed_radio_init("acw", "_Anti-clockwise", WEED_TRUE, 1),
                               weed_radio_init("cw", "_Clockwise", WEED_FALSE, 1),
                               weed_switch_init("szc", "_Switch direction on frame size change", WEED_FALSE),
                               NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("kaleidoscope", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               kal_init, kal_process, kal_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plant_t *gui = weed_paramtmpl_get_gui(in_params[2]);
  weed_set_double_value(gui, WEED_LEAF_STEP_SIZE, .1);

  gui = weed_paramtmpl_get_gui(in_params[1]);
  weed_set_boolean_value(gui, WEED_LEAF_WRAP, WEED_TRUE);

  gui = weed_paramtmpl_get_gui(in_params[0]);
  weed_set_double_value(gui, WEED_LEAF_STEP_SIZE, .1);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END

