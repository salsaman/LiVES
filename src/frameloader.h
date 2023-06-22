// frameloader.h
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _FRAME_LOADER_H
#define _FRAME_LOADER_H

#define GET_BASE_DEFNS
#include "cliphandler.h"
#undef GET_BASE_DEFNS

// functions that deal with loading (and saving) frames

// create "fake" sources - "real" sources are things like
// clip decoders adn generators,  which operate via plugins.
// we also have "fake" sources: image loader, filebuffer loader (scrap_frames)
// and blank fram generator being examples
// in this latter case there is no associated plugin (at laeast currently)
// and the plugin functions are simulated via function pointers to internal code
// see cliphandler.h for definitons of clip
//
// for clip sources we have this union:
/* union { */
/*     void *source; // pointer to src object (object that fills the pixel_data and has metadata) */
/*     lives_clipsrc_func_t source_func; // or to a function that fills pixel data for a layer */
/*   }; */

lives_result_t lives_img_srcfunc(weed_layer_t *layer);

lives_result_t lives_blankframe_srcfunc(weed_layer_t *layer);

/////////////////// image filennames ////////

/// lives_image_type can be a string, lives_img_type_t is an enumeration
char *make_image_file_name(lives_clip_t *, frames_t frame, const char *img_ext);// /workdir/handle/00000001.png
char *make_image_short_name(lives_clip_t *, frames_t frame, const char *img_ext);// e.g. 00000001.png
const char *get_image_ext_for_type(lives_img_type_t imgtype);
lives_img_type_t lives_image_ext_to_img_type(const char *img_ext);
lives_img_type_t lives_image_type_to_img_type(const char *lives_image_type);
const char *image_ext_to_lives_image_type(const char *img_ext);

///////////

int save_to_scrap_file(weed_layer_t *);
boolean load_from_scrap_file(weed_layer_t *, frames_t frame);
boolean flush_scrap_file(void);

boolean pull_frame(weed_layer_t *, const char *image_ext, ticks_t tc);
void pull_frame_threaded(weed_layer_t *, ticks_t tc, int width, int height);
boolean is_layer_ready(weed_layer_t *);
boolean check_layer_ready(weed_layer_t *);
boolean pull_frame_at_size(weed_layer_t *, const char *image_ext, ticks_t tc,
                           int width, int height, int target_palette);
LiVESPixbuf *pull_lives_pixbuf_at_size(int clip, int frame, const char *image_ext, ticks_t tc,
                                       int width, int height, LiVESInterpType interp, boolean fordisp);
LiVESPixbuf *pull_lives_pixbuf(int clip, int frame, const char *image_ext, ticks_t tc);

boolean weed_layer_create_from_file_progressive(weed_layer_t *, const char *fname, int width,
    int height, int tpalette, const char *img_ext);

void load_start_image(frames_t frame);

void load_end_image(frames_t frame);

void showclipimgs(void);

void load_preview_image(boolean update_always);

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

  // for png this is 'quality' (TODO - use union)
  int compression;
} savethread_priv_t;

boolean pixbuf_to_png(LiVESPixbuf *pixbuf, char *fname, lives_img_type_t imgtype,
                      int quality, int width, int height, LiVESError **gerrorptr);
boolean pixbuf_to_png_threaded(savethread_priv_t *); // deprecated

boolean layer_from_png(int fd, weed_layer_t *layer, int width, int height, int tpalette, boolean prog);
boolean layer_to_png(weed_layer_t *layer, const char *fname, int comp);
boolean layer_to_png_threaded(savethread_priv_t *); // deprecated

#endif// _FRAME_LOADER_H
