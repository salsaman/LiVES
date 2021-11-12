// LiVES - videodev input
// (c) G. Finch 2010 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef _VIDEODEV_H
#define _VIDEODEV_H

#ifdef HAVE_UNICAP

#include <unicap/unicap.h>

typedef struct {
  unicap_handle_t handle;
  int fileno; ///< lives clip number
  int buffer_type; ///< system or user
  volatile int buffer_ready;
  unicap_data_buffer_t buffer1;
  unicap_data_buffer_t buffer2;
  int current_palette;
  int YUV_sampling;
  int YUV_subspace;
  int YUV_clamping;
  boolean is_really_grey; ///< for greyscale we lie and say it is YUV444P (i.e we add U and V planes)
} lives_vdev_t;

#define MAX_DEVICES 1024
#define MAX_FORMATS 1024
#define MAX_PROPS 1024

boolean on_open_vdev_activate(LiVESMenuItem *, const char *devname);
boolean weed_layer_set_from_lvdev(weed_layer_t *layer, lives_clip_t *sfile, double timeoutsecs);
void lives_vdev_free(lives_vdev_t *);

#endif
#endif
