// paramspecial.h
// LiVES
// (c) G. Finch 2004 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// dynamic window generation from parameter arrays :-)
// special widgets

// TODO - refactor all of this using lives_special_t, use a union

#ifndef HAS_LIVES_PARAMSPECIAL_H
#define HAS_LIVES_PARAMSPECIAL_H

typedef struct {
  lives_rfx_t *rfx;
  boolean added;
  lives_param_special_t type;
  lives_param_t *xstart_param;
  lives_param_t *ystart_param;
  lives_param_t *scale_param;
  lives_param_t *xend_param;
  lives_param_t *yend_param;

  int stdwidgets; // 2 for singlepoint, 3 for scaledpoint, 4 for demask, multirect
  int *extra_params;
  int num_extra;
  LiVESWidget **extra_widgets;
} lives_special_framedraw_rect_t;

typedef struct {
  lives_param_t *height_param;
  lives_param_t *width_param;
  LiVESWidget *lockbutton;
  ulong width_func;
  ulong height_func;
  double ratio;
  int nwidgets;
  boolean no_reset;
} lives_special_aspect_t;

typedef struct {
  lives_param_t *font_param;
  lives_param_t *size_param;
  ulong size_paramfunc;
  ulong entry_func;
  int nwidgets;
} lives_special_fontchooser_t;

typedef struct {
  lives_rfx_t *rfx;
  lives_param_t *start_param;
  lives_param_t *end_param;
} lives_special_mergealign_t;

////////////////////////////////

#define WEED_LEAF_HOST_VALUE_SPECIAL "host_valspec"
#define LIVES_VALUE_LETTERBOX_OFFSX (1 << 0) // def val. == param_max * lb_offs_x / width
#define LIVES_VALUE_LETTERBOX_OFFSY (1 << 1)
#define LIVES_VALUE_LETTERBOX_WIDTH (1 << 2) // def val == param_max * lb_width / width
#define LIVES_VALUE_LETTERBOX_HEIGHT (1 << 3)

#include "multitrack.h"

void init_special(void);

void add_to_special(const char *special_string, lives_rfx_t *);

void check_for_special(lives_rfx_t *, lives_param_t *, LiVESBox *);
void check_for_special_type(lives_rfx_t *, lives_param_t *, LiVESBox *);

void reset_framedraw_preview(void);

void fd_connect_spinbutton(lives_rfx_t *);

void fd_tweak(lives_rfx_t *);

void after_aspect_width_changed(LiVESSpinButton *, livespointer);

void after_aspect_height_changed(LiVESToggleButton *, livespointer);

const lives_special_aspect_t *paramspecial_get_aspect(void);

boolean check_filewrite_overwrites(void);

boolean special_cleanup(boolean is_ok);

void setmergealign(void);

void set_aspect_ratio_widgets(lives_param_t *, lives_param_t *);

boolean is_perchannel_multi(lives_rfx_t *, int pnum);

LiVESPixbuf *mt_framedraw(lives_mt *, weed_layer_t *);

lives_special_mergealign_t mergealign;

#endif
