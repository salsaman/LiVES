// frameloader.h
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _FRAME_LOADER_H
#define _FRAME_LOADER_H

// functions that deal with loading (and saving) frames

boolean pull_frame(weed_layer_t *, const char *image_ext, ticks_t tc);
void pull_frame_threaded(weed_layer_t *, const char *img_ext, ticks_t tc, int width, int height);
boolean is_layer_ready(weed_layer_t *);
boolean check_layer_ready(weed_layer_t *);
boolean pull_frame_at_size(weed_layer_t *, const char *image_ext, ticks_t tc,
                           int width, int height, int target_palette);
LiVESPixbuf *pull_lives_pixbuf_at_size(int clip, int frame, const char *image_ext, ticks_t tc,
                                       int width, int height, LiVESInterpType interp, boolean fordisp);
LiVESPixbuf *pull_lives_pixbuf(int clip, int frame, const char *image_ext, ticks_t tc);

boolean weed_layer_create_from_file_progressive(weed_layer_t *, const char *fname, int width,
    int height, int tpalette, const char *img_ext);

boolean lives_pixbuf_save(LiVESPixbuf *, char *fname, lives_img_type_t imgtype, int quality,
                          int width, int height, LiVESError **gerrorptr);

void set_drawing_area_from_pixbuf(LiVESWidget *darea, LiVESPixbuf *, lives_painter_surface_t *);

void load_start_image(frames_t frame);

void load_end_image(frames_t frame);

void showclipimgs(void);

void load_preview_image(boolean update_always);

void close_current_file(int file_to_switch_to);   ///< close current file, and try to switch to file_to_switch_to

void switch_to_file(int old_file, int new_file);

void do_quick_switch(int new_file);

boolean switch_audio_clip(int new_file, boolean activate);

void resize(double scale);

void set_record(void);

typedef struct {
  char *fname;

  // for pixbuf (e.g. jpeg)
  LiVESPixbuf *pixbuf;
  LiVESError *error;
  lives_img_type_t img_type;
  int width, height;

  // for layer (e.g. png) (may also set TGREADVAR(write_failed)
  weed_layer_t *layer;
  boolean success;

  int compression;
} savethread_priv_t;

void *lives_pixbuf_save_threaded(void *saveargs);

#ifdef USE_LIBPNG
boolean layer_from_png(int fd, weed_layer_t *layer, int width, int height, int tpalette, boolean prog);
boolean save_to_png(weed_layer_t *layer, const char *fname, int comp);
void *save_to_png_threaded(void *args);
#endif

#endif// _FRAME_LOADER_H
