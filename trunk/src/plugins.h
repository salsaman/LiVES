// plugins.h
// LiVES
// (c) G. Finch 2003-2006 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _HAS_PLUGINS_H
#define _HAS_PLUGINS_H

#include <gmodule.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include "main.h"


// generic plugins

GList *get_plugin_list (gchar *plugin_type, gboolean allow_nonex, gchar *plugdir, gchar *filter_ext);
#define PLUGIN_ENCODERS "encoders"
#define PLUGIN_DECODERS "decoders"
#define PLUGIN_VID_PLAYBACK "playback/video"


// smogrify handles the directory differently for themes
#define PLUGIN_THEMES "themes"

// uses WEED_PLUGIN_PATH
#define PLUGIN_EFFECTS_WEED "weed"
#define PLUGIN_WEED_FX_BUILTIN "effects/realtime/weed"


GList *plugin_request (const gchar *plugin_type, const gchar *plugin_name, const gchar *request);
GList *plugin_request_with_blanks (const gchar *plugin_type, const gchar *plugin_name, const gchar *request);
GList *plugin_request_by_line (const gchar *plugin_type, const gchar *plugin_name, const gchar *request);
GList *plugin_request_by_space (const gchar *plugin_type, const gchar *plugin_name, const gchar *request);
GList *plugin_request_common (const gchar *plugin_type, const gchar *plugin_name, const gchar *request, gchar *delim, gboolean allow_blanks);


// video playback plugins
typedef gboolean (*plugin_keyfunc) (gboolean down, guint16 unicode, guint16 keymod);

typedef struct {
  // playback
  gchar name[64];
  void *handle;

  // mandatory
  const char *(*module_check_init)(void);
  const char *(*version) (void);
  const char *(*get_description) (void);
  const char *(*get_rfx) (void);
  gint *(*get_palette_list) (void);
  gboolean (*set_palette) (int palette);
  guint64 (*get_capabilities) (int palette);
  gboolean (*render_frame) (int hsize, int vsize, int64_t timecode, void *pixel_data, void *return_data);

  // optional
  gboolean (*init_screen) (int width, int height, gboolean fullscreen, guint32 window_id, int argc, gchar **argv);
  void (*exit_screen) (guint16 mouse_x, guint16 mouse_y);
  void (*module_unload) (void);
  const gchar *(*get_fps_list) (int palette);
  gboolean (*set_fps) (gdouble fps);

  // only for display plugins
  gboolean (*send_keycodes) (plugin_keyfunc);

  // optional for YUV palettes
  int *(*get_yuv_palette_sampling) (int palette);
  int *(*get_yuv_palette_clamping) (int palette);
  int *(*get_yuv_palette_subspace) (int palette);
  int (*set_yuv_palette_sampling) (int palette);
  int (*set_yuv_palette_clamping) (int palette);
  int (*set_yuv_palette_subspace) (int palette);

  guint64 capabilities;

#define VPP_CAN_RESIZE    1<<0
#define VPP_CAN_RETURN    1<<1
#define VPP_LOCAL_DISPLAY 1<<2

  gint fwidth,fheight;

  int palette;
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;

  gint fixed_fps_numer;
  gint fixed_fps_denom;
  gdouble fixed_fpsd;

  int extra_argc;
  gchar **extra_argv;


} _vid_playback_plugin;


#define DEF_VPP_HSIZE 320.
#define DEF_VPP_VSIZE 240.

_vid_playback_plugin *open_vid_playback_plugin (const gchar *name, gboolean using);
void vid_playback_plugin_exit (void);
void close_vid_playback_plugin(_vid_playback_plugin *vpp);
void save_vpp_defaults(_vid_playback_plugin *vpp);
void load_vpp_defaults(_vid_playback_plugin *vpp);


// encoder plugins

void do_plugin_encoder_error(const gchar *plugin_name_or_null);

GList *filter_encoders_by_img_ext(GList *encoders, const gchar *img_ext);

typedef struct {
  gchar name[51];
  guint32 audio_codec;
  // must match with the "acodec" GList in interface.c

  // TODO ** - fixme (change to bitmap, add raw pcm)

#define AUDIO_CODEC_MP3 0
#define AUDIO_CODEC_PCM 1
#define AUDIO_CODEC_MP2 2
#define AUDIO_CODEC_VORBIS 3
#define AUDIO_CODEC_AC3 4
#define AUDIO_CODEC_AAC 5
#define AUDIO_CODEC_AMR_NB 6

#define AUDIO_CODEC_MAX 31
  //
#define AUDIO_CODEC_NONE 32
#define AUDIO_CODEC_UNKNOWN 33

  guint capabilities;


#define HAS_RFX 1<<0

#define CAN_ENCODE_PNG 1<<2
#define ENCODER_NON_NATIVE 1<<3

  // current output format
  gchar of_name[51];
  gchar of_desc[128];
  gint of_allowed_acodecs;
  gchar of_restrict[1024];
  gchar of_def_ext[16];
}
_encoder;



// decoder plugins

#define LIVES_INTERLACE_NONE 0
#define LIVES_INTERLACE_BOTTOM_FIRST 1
#define LIVES_INTERLACE_TOP_FIRST 2
#define LIVES_INTERLACE_PROGRESSIVE 3

typedef struct {
  int width;
  int height;
  int nframes;
  int interlace;

  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;
  float fps;
  int *palettes;

  char container_name[512]; // name of container, e.g. "ogg"
  char video_name[512]; // name of video codec, e.g. "theora"
  char audio_name[512]; // name of audio codec, e.g. "vorbis"

  int arate;
  int achans;
  int asamps;
  int asigned;
  int ainterleaf;
} lives_clip_data_t;




typedef struct {
  // playback
  gchar *name; // plugin name
  void *handle;

  // mandatory
  const char *(*module_check_init)(void);
  const char *(*version) (void);
  const char **(*get_formats)(void);
  const lives_clip_data_t *(*get_clip_data)(char *URI);
  gboolean (*rip_audio) (char *URI, char *fname, int stframe, int frames);
  gboolean (*get_frame)(char *URI, int64_t frame, void **pixel_data);

  // optional
  gboolean (*set_palette)(int palette);
  gboolean (*set_audio_fmt)(int audio_fmt);
  void (*module_unload)(void);

  int preferred_palette;
  int current_palette;
  int interlace;
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;

  double gamma_needed;

} _decoder_plugin;


const lives_clip_data_t *get_decoder_plugin(file *sfile);
void close_decoder_plugin (file *sfile, _decoder_plugin *dplug);
_decoder_plugin *open_decoder_plugin(const gchar *plname, file *sfile);
void get_mime_type(gchar *text, int maxlen, const lives_clip_data_t *cdata);


// RFX plugins


// external rendered fx plugins (RFX plugins)
#define PLUGIN_RENDERED_EFFECTS_BUILTIN "effects/rendered/"

// in the home directory
#define PLUGIN_RENDERED_EFFECTS_CUSTOM "plugins/effects/rendered/custom/"
#define PLUGIN_RENDERED_EFFECTS_TEST "plugins/effects/rendered/test/"

// rfx scripts for the SDK
#define PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS "effects/RFXscripts/"

// in the home directory
#define PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS "plugins/effects/RFXscripts/custom/"
#define PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS "plugins/effects/RFXscripts/test/"

// scraps are passed between programs to generate param windows
#define PLUGIN_RFX_SCRAP ""


// max number of display widgets per parameter (currently 5 for RGBA spinbuttons + colorbutton)
#define MAX_PARAM_WIDGETS 5

// length of max string (not including terminating NULL) for LiVES-perl
#define RFX_MAXSTRINGLEN 1024


typedef struct {
  // weed style part
  gchar *name;
  gchar *desc;

  gchar *label;
  gint flags;
  gboolean use_mnemonic;
  gpointer interp_func;
  gpointer display_func;
  gint hidden;

  // reason(s) for hiding [bitmap]
#define HIDDEN_GUI (1<<0)
#define HIDDEN_MULTI (1<<1)
#define HIDDEN_NEEDS_REINIT (1<<2)

  gdouble step_size;
  gint copy_to;
  gboolean transition;
  gboolean reinit;

  gboolean wrap;
  gint group;
  gint type;

#define LIVES_PARAM_UNKNOWN 0
#define LIVES_PARAM_NUM 1
#define LIVES_PARAM_BOOL 2
#define LIVES_PARAM_COLRGB24 3
#define LIVES_PARAM_STRING 4
#define LIVES_PARAM_STRING_LIST 5
#define LIVES_PARAM_COLRGBA32 6


#define LIVES_PARAM_UNDISPLAYABLE 65536
  
  gint dp;  //decimals, 0 for int and bool
  void *value;  // current value(s)

  gdouble min;
  gdouble max; // for string this is max characters

  void *def; // default values
  GList *list; // for string list (choices)

  // multivalue type - single value, multi value, or per channel
  gshort multi;
#define PVAL_MULTI_NONE 0
#define PVAL_MULTI_ANY 1
#define PVAL_MULTI_PER_CHANNEL 2

  //--------------------------------------------------
  // extras for LiVES

  // TODO - change to GtkWidget **widgets, terminated with a NULL
  GtkWidget *widgets[MAX_PARAM_WIDGETS]; // widgets which hold value/RGBA settings
  gboolean onchange; // is there a trigger ?

  gboolean changed;

  gboolean change_blocked;

} lives_param_t;


typedef struct {
  gchar *name;  // the name of the executable (so we can run it !)
  gchar *menu_text; // for Weed, this is the filter_class "name"
  gchar *action_desc; // for Weed "Applying $s"
  gint min_frames; // for Weed, 1
  gint num_in_channels;
  gshort status;
#define RFX_STATUS_BUILTIN 0 // factory presets
#define RFX_STATUS_CUSTOM 1 // custom effects in the custom menu
#define RFX_STATUS_TEST 2 // test effects in the advanced menu
#define RFX_STATUS_ANY 3 // indicates free choice of statuses
#define RFX_STATUS_WEED 4 // indicates an internal RFX, created from a weed instance
#define RFX_STATUS_SCRAP 5 // used for parsing RFX scraps from external apps

  // these are only used when prompting for a name
#define RFX_STATUS_COPY 128 // indicates a copy operation to test
#define RFX_STATUS_RENAME 129 // indicates a copy operation to test

  guint32 props;
#define RFX_PROPS_SLOW        0x0001  // hint to GUI
#define RFX_PROPS_MAY_RESIZE  0x0002 // is a tool
#define RFX_PROPS_BATCHG      0x0004 // is a batch generator


#define RFX_PROPS_RESERVED1   0x1000
#define RFX_PROPS_RESERVED2   0x2000
#define RFX_PROPS_RESERVED3   0x4000
#define RFX_PROPS_AUTO_BUILT  0x8000

  GtkWidget *menuitem;  // the menu item which activates this effect
  gint num_params;
  lives_param_t *params;
  void *source;  // points to the source (e.g. a weed_plant_t)
  void *extra;  // for future use */
  gchar delim[2];
  gboolean is_template;

} lives_rfx_t;

gboolean check_rfx_for_lives (lives_rfx_t *);

void do_rfx_cleanup(lives_rfx_t *);

void render_fx_get_params (lives_rfx_t *, const gchar *plugin_name, gshort status);

void sort_rfx_array (lives_rfx_t *in_array, gint num_elements);

gint find_rfx_plugin_by_name (gchar *name, gshort status);

void rfx_copy (lives_rfx_t *src, lives_rfx_t *dest, gboolean full);

void rfx_free(lives_rfx_t *rfx);

void rfx_free_all (void);

void param_copy (lives_param_t *src, lives_param_t *dest, gboolean full);


typedef struct {
  gshort red;
  gshort green;
  gshort blue;
} lives_colRGB24_t;

typedef struct {
  gshort red;
  gshort green;
  gshort blue;
  gshort alpha;
} lives_colRGBA32_t;


typedef struct {
  GList *list; // list of filter_idx from which user can delegate
  gint delegate; // offset in list of current delegate
  gulong func; // menuitem activation function for current delegate
  lives_rfx_t *rfx; // pointer to rfx for current delegate (or NULL)
} lives_fx_candidate_t;

// filter types which can have candidates
#define FX_CANDIDATE_AUDIO_VOL 0
#define FX_CANDIDATE_RESIZER 1
#define FX_CANDIDATE_DEINTERLACE 2

#define MAX_FX_CANDIDATE_TYPES 3

gboolean get_bool_param(void *value);
gint get_int_param(void *value);
gdouble get_double_param(void *value);
void get_colRGB24_param(void *value, lives_colRGB24_t *rgb);
void get_colRGBA32_param(void *value, lives_colRGBA32_t *rgba);

void set_bool_param(void *value, const gboolean _const);
void set_int_param(void *value, const gint _const);
void set_double_param(void *value, const gdouble _const);
void set_colRGB24_param(void *value, gshort red, gshort green, gshort blue);
void set_colRGBA32_param(void *value, gshort red, gshort green, gshort blue, gshort alpha);

// return an array of parameter values
void **store_rfx_params (lives_rfx_t *);
void set_rfx_params_from_store (lives_rfx_t *rfx, void **store);
void rfx_params_store_free (lives_rfx_t *, void **store);

// 
GList *array_to_string_list (gchar **array, gint offset, gint len);

lives_rfx_t *weed_to_rfx (weed_plant_t *plant, gboolean show_reinits);

gchar *plugin_run_param_window(gchar *get_com, GtkVBox *vbox, lives_rfx_t **ret_rfx);

////////////////////////////////////////////////////////////
// video playback plugin window - fixed part
typedef struct {
  _vid_playback_plugin *plugin;
  GtkWidget *dialog;
  GtkWidget *spinbuttonh;
  GtkWidget *spinbuttonw;
  GtkWidget *fps_entry;
  GtkWidget *pal_entry;
  lives_rfx_t *rfx;
} _vppaw;

_vppaw *on_vpp_advanced_clicked (GtkButton *, gpointer);


// for realtime effects, see effects-weed.h

#endif
