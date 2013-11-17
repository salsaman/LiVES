// plugins.h
// LiVES
// (c) G. Finch 2003-2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PLUGINS_H
#define HAS_LIVES_PLUGINS_H

#include <gmodule.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>


// generic plugins

GList *get_plugin_list (const gchar *plugin_type, boolean allow_nonex, const gchar *plugdir, const gchar *filter_ext);
#define PLUGIN_ENCODERS "encoders"
#define PLUGIN_DECODERS "decoders"
#define PLUGIN_VID_PLAYBACK "playback/video"
#define PLUGIN_AUDIO_STREAM "playback/audiostream"

/// smogrify handles the directory differently for themes
#define PLUGIN_THEMES "themes"

/// uses WEED_PLUGIN_PATH
#define PLUGIN_EFFECTS_WEED "weed"
#define PLUGIN_WEED_FX_BUILTIN "effects/realtime/weed"


GList *plugin_request (const gchar *plugin_type, const gchar *plugin_name, const gchar *request);
GList *plugin_request_with_blanks (const gchar *plugin_type, const gchar *plugin_name, const gchar *request);
GList *plugin_request_by_line (const gchar *plugin_type, const gchar *plugin_name, const gchar *request);
GList *plugin_request_by_space (const gchar *plugin_type, const gchar *plugin_name, const gchar *request);
GList *plugin_request_common (const gchar *plugin_type, const gchar *plugin_name, const gchar *request, const gchar *delim, boolean allow_blanks);

#ifndef  __WEED_EFFECTS_H__
typedef weed_plant_t *(*weed_bootstrap_f) (weed_default_getter_f *value, int num_versions, int *plugin_versions);
#endif

/// video playback plugins
typedef boolean (*plugin_keyfunc) (boolean down, guint16 unicode, guint16 keymod);


typedef struct {
  // playback
  gchar name[64];
  void *handle;

  // mandatory
  const char *(*module_check_init)(void);
  const char *(*version) (void);
  const char *(*get_description) (void);

  int *(*get_palette_list) (void);
  boolean (*set_palette) (int palette);
  guint64 (*get_capabilities) (int palette);

  boolean (*render_frame) (int hsize, int vsize, int64_t timecode, void **pixel_data, void **return_data,
			    weed_plant_t **play_params);

  // optional
  boolean (*init_screen) (int width, int height, boolean fullscreen, uint64_t window_id, int argc, gchar **argv);
  void (*exit_screen) (guint16 mouse_x, guint16 mouse_y);
  void (*module_unload) (void);
  const gchar *(*get_fps_list) (int palette);
  boolean (*set_fps) (double fps);
  
  const char *(*get_init_rfx) (void);

  ///< optional (but should return a weed plantptr array of paramtmpl and chantmpl, NULL terminated)
  const weed_plant_t **(*get_play_params) (weed_bootstrap_f f); 
  
  // only for display plugins
  boolean (*send_keycodes) (plugin_keyfunc);

  // optional for YUV palettes
  int *(*get_yuv_palette_sampling) (int palette);
  int *(*get_yuv_palette_clamping) (int palette);
  int *(*get_yuv_palette_subspace) (int palette);
  int (*set_yuv_palette_sampling) (int palette);
  int (*set_yuv_palette_clamping) (int palette);
  int (*set_yuv_palette_subspace) (int palette);

  // audio streaming
  int *(*get_audio_fmts)(void);

  guint32 audio_codec;
  // must match with the "acodec" GList in interface.c
  // and bitmaps in the encder plugins, with this one addition:

  guint64 capabilities;

#define VPP_CAN_RESIZE    1<<0
#define VPP_CAN_RETURN    1<<1
#define VPP_LOCAL_DISPLAY 1<<2

  int fwidth,fheight;

  int palette;
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;

  int fixed_fps_numer;
  int fixed_fps_denom;
  double fixed_fpsd;

  int extra_argc;
  gchar **extra_argv;

  const weed_plant_t **play_paramtmpls;
  weed_plant_t **play_params;
  weed_plant_t **alpha_chans;
  int num_play_params;
  int num_alpha_chans;

} _vid_playback_plugin;


#define DEF_VPP_HSIZE 320.
#define DEF_VPP_VSIZE 240.

_vid_playback_plugin *open_vid_playback_plugin (const gchar *name, boolean in_use);
void vid_playback_plugin_exit (void);
void close_vid_playback_plugin(_vid_playback_plugin *);
int64_t get_best_audio(_vid_playback_plugin *);
void save_vpp_defaults(_vid_playback_plugin *, gchar *file);
void load_vpp_defaults(_vid_playback_plugin *, gchar *file);


const weed_plant_t *pp_get_param(weed_plant_t **pparams, int idx);
const weed_plant_t *pp_get_chan(weed_plant_t **pparams, int idx);

// encoder plugins

void do_plugin_encoder_error(const gchar *plugin_name_or_null);

GList *filter_encoders_by_img_ext(GList *encoders, const gchar *img_ext);

typedef struct {
  gchar name[51];
  guint32 audio_codec;
  // match with bitmaps in the encoder plugins
  // and also anames array in plugins.c (see below)

#define AUDIO_CODEC_MP3 0
#define AUDIO_CODEC_PCM 1
#define AUDIO_CODEC_MP2 2
#define AUDIO_CODEC_VORBIS 3
#define AUDIO_CODEC_AC3 4
#define AUDIO_CODEC_AAC 5
#define AUDIO_CODEC_AMR_NB 6
#define AUDIO_CODEC_RAW 7       // reserved
#define AUDIO_CODEC_WMA2 8


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
  int of_allowed_acodecs;
  gchar of_restrict[1024];
  gchar of_def_ext[16];
}
_encoder;


// defined in plugins.c for the whole app
extern const char *anames[AUDIO_CODEC_MAX];


// decoder plugins

// seek_flags is a bitmap

  /// good
#define LIVES_SEEK_FAST (1<<0)

  /// not so good
#define LIVES_SEEK_NEEDS_CALCULATION (1<<1)
#define LIVES_SEEK_QUALITY_LOSS (1<<2)


// must be exactly the same as in decplugin.h

typedef struct {
  gchar *URI; ///< the URI of this cdata

  int nclips; ///< number of clips (titles) in container
  gchar container_name[512]; ///< name of container, e.g. "ogg" or NULL

  char title[256];
  char author[256];
  char comment[256];

  /// plugin should init this to 0 if URI changes
  int current_clip; ///< current clip number in container (starts at 0, MUST be <= nclips) [rw host]

  // video data
  int width; // width and height of picture in frame
  int height;
  int64_t nframes;
  lives_interlace_t interlace;

  /// x and y offsets of picture within frame
  /// for primary pixel plane
  int offs_x;
  int offs_y;
  int frame_width; // frame width and height are the size of the outer frame
  int frame_height;

  float par; ///< pixel aspect ratio

  float video_start_time;

  float fps;

  int *palettes;

  /// plugin should init this to palettes[0] if URI changes
  int current_palette;  ///< current palette [rw host]; must be contained in palettes
  
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;
  gchar video_name[512]; ///< name of video codec, e.g. "theora" or NULL

  /* audio data */
  int arate;
  int achans;
  int asamps;
  boolean asigned;
  boolean ainterleaf;
  gchar audio_name[512]; ///< name of audio codec, e.g. "vorbis" or NULL

  int seek_flag;

#define SYNC_HINT_AUDIO_TRIM_START (1<<0)
#define SYNC_HINT_AUDIO_PAD_START (1<<1)
#define SYNC_HINT_AUDIO_TRIM_END (1<<2)
#define SYNC_HINT_AUDIO_PAD_END (1<<3)

#define SYNC_HINT_VIDEO_PAD_START (1<<4)
#define SYNC_HINT_VIDEO_PAD_END (1<<5)

  int sync_hint;


  void *priv; ///< private data for demuxer/decoder - host should not touch this

} lives_clip_data_t;



typedef struct {
  // playback
  gchar *name; ///< plugin name
  void *handle; ///< may be shared between several instances

  // mandatory
  const char *(*version) (void);

  /// call first time with NULL cdata
  /// subsequent calls should re-use cdata
  /// set cdata->current_clip > 0 to get data for clip n (0 <= n < cdata->nclips)
  /// we can also set cdata->current_palette (must be in list cdata->palettes[])
  ///
  /// if URI changes, current_clip and current_palette are reset by plugin
  ///
  /// to get a clone of cdata, pass in NULL URI and cdata
  ///
  lives_clip_data_t *(*get_clip_data)(char *URI, const lives_clip_data_t *cdata);

  /// frame starts at 0 in these functions; height is height of primary plane
  boolean (*get_frame)(const lives_clip_data_t *, int64_t frame, int *rowstrides, int height, void **pixel_data);

  /// call this for each cdata before unloading the module
  void (*clip_data_free)(lives_clip_data_t *);

  // optional
  const char *(*module_check_init)(void);
  boolean (*set_palette)(lives_clip_data_t *);
  int64_t (*rip_audio) (const lives_clip_data_t *, const char *fname, int64_t stframe, int64_t nframes, 
			unsigned char **abuff);
  void (*rip_audio_cleanup) (const lives_clip_data_t *cdata);
  void (*module_unload)(void);

} lives_decoder_sys_t;




typedef struct {
  const lives_decoder_sys_t *decoder;
  lives_clip_data_t *cdata;
} lives_decoder_t;





const lives_clip_data_t *get_decoder_cdata(file *sfile, GList *disabled, const lives_clip_data_t *fake_cdata);
void close_decoder_plugin (lives_decoder_t *);
lives_decoder_sys_t *open_decoder_plugin(const gchar *plname);
void get_mime_type(gchar *text, int maxlen, const lives_clip_data_t *);
void unload_decoder_plugins(void);

boolean decplugin_supports_palette (const lives_decoder_t *dplug, int palette);

lives_clip_data_t *clone_cdata(int fileno);


// RFX plugins


/// external rendered fx plugins (RFX plugins)
#define PLUGIN_RENDERED_EFFECTS_BUILTIN "effects/rendered/"

/// in the home directory
#define PLUGIN_RENDERED_EFFECTS_CUSTOM "plugins/effects/rendered/custom/"
#define PLUGIN_RENDERED_EFFECTS_TEST "plugins/effects/rendered/test/"

/// rfx scripts for the SDK
#define PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS "effects/RFXscripts/"

/// in the home directory
#define PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS "plugins/effects/RFXscripts/custom/"
#define PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS "plugins/effects/RFXscripts/test/"

/// scraps are passed between programs to generate param windows
#define PLUGIN_RFX_SCRAP ""


/// max number of display widgets per parameter (currently 5 for RGBA spinbuttons + colorbutton)
#define MAX_PARAM_WIDGETS 5

/// length of max string (not including terminating NULL) for LiVES-perl
#define RFX_MAXSTRINGLEN 1024


typedef enum {

  LIVES_PARAM_UNKNOWN=0,
  LIVES_PARAM_NUM,
  LIVES_PARAM_BOOL,
  LIVES_PARAM_COLRGB24,
  LIVES_PARAM_STRING,
  LIVES_PARAM_STRING_LIST,
  LIVES_PARAM_COLRGBA32,

  LIVES_PARAM_UNDISPLAYABLE=65536
  
} lives_param_type_t;


typedef enum {
  LIVES_RFX_SOURCE_RFX=0,
  LIVES_RFX_SOURCE_WEED
} lives_rfx_source_t;


typedef enum {
  LIVES_PARAM_SPECIAL_TYPE_NONE=0, // normal parameter type

  // framedraw types
  LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK,  ///< type may be used in framedraw
  LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT,  ///< type may be used in framedraw
  LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT,  ///< type may be used in framedraw
  LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT,  ///< type may be used in framedraw

  // text widget types
  LIVES_PARAM_SPECIAL_TYPE_FILEREAD,
  LIVES_PARAM_SPECIAL_TYPE_PASSWORD,

  // misc types
  LIVES_PARAM_SPECIAL_TYPE_MERGEALIGN,
  LIVES_PARAM_SPECIAL_TYPE_ASPECT_RATIO

} lives_param_special_t;


typedef struct {
  // weed style part
  gchar *name;
  gchar *desc;

  gchar *label;
  int flags;
  boolean use_mnemonic;
  fn_ptr interp_func;
  fn_ptr display_func;
  int hidden;

  // reason(s) for hiding [bitmap]
#define HIDDEN_GUI (1<<0)
#define HIDDEN_MULTI (1<<1)
#define HIDDEN_NEEDS_REINIT (1<<2)
#define HIDDEN_COMPOUND_INTERNAL (1<<3)

  double step_size;
  //int copy_to;
  boolean transition;
  boolean reinit;

  boolean wrap;
  int group;
  lives_param_type_t type;

  int dp;  ///<decimals, 0 for int and bool
  void *value;  ///< current value(s)

  double min;
  double max; ///< for string this is max characters

  void *def; ///< default values
  GList *list; ///< for string list (choices)

  /// multivalue type - single value, multi value, or per channel
  short multi;
#define PVAL_MULTI_NONE 0
#define PVAL_MULTI_ANY 1
#define PVAL_MULTI_PER_CHANNEL 2

  //--------------------------------------------------
  // extras for LiVES

  /// TODO - change to GtkWidget **widgets, terminated with a NULL
  GtkWidget *widgets[MAX_PARAM_WIDGETS]; ///< widgets which hold value/RGBA settings
  boolean onchange; ///< is there a trigger ?

  boolean changed;

  boolean change_blocked;

  void *source;

  lives_rfx_source_t source_type;

  // this may change
  lives_param_special_t special_type; // the visual modification type (see paramspecial.h)
  int special_type_index; // index within special_type (e.g for DEMASK, 0==left, 1==top, 2==width, 3==height)

} lives_param_t;


typedef enum {
  RFX_STATUS_BUILTIN=0, ///< factory presets
  RFX_STATUS_CUSTOM=1, ///< custom effects in the custom menu
  RFX_STATUS_TEST=2, ///< test effects in the advanced menu
  RFX_STATUS_ANY=3, ///< indicates free choice of statuses
  RFX_STATUS_WEED=4, ///< indicates an internal RFX, created from a weed instance
  RFX_STATUS_SCRAP=5, ///< used for parsing RFX scraps from external apps

  // these are only used when prompting for a name
  RFX_STATUS_COPY=128, ///< indicates a copy operation to test
  RFX_STATUS_RENAME=129 ///< indicates a copy operation to test
} lives_rfx_status_t;




typedef struct {
  gchar *name;  ///< the name of the executable (so we can run it !)
  gchar *menu_text; ///< for Weed, this is the filter_class "name"
  gchar *action_desc; ///< for Weed "Applying $s"
  int min_frames; ///< for Weed, 1
  int num_in_channels;
  lives_rfx_status_t status;


  guint32 props;
#define RFX_PROPS_SLOW        0x0001  ///< hint to GUI
#define RFX_PROPS_MAY_RESIZE  0x0002 ///< is a tool
#define RFX_PROPS_BATCHG      0x0004 ///< is a batch generator


#define RFX_PROPS_RESERVED1   0x1000
#define RFX_PROPS_RESERVED2   0x2000
#define RFX_PROPS_RESERVED3   0x4000
#define RFX_PROPS_AUTO_BUILT  0x8000

  GtkWidget *menuitem;  ///< the menu item which activates this effect
  int num_params;
  lives_param_t *params;
  lives_rfx_source_t source_type;
  void *source;  ///< points to the source (e.g. a weed_plant_t)
  void *extra;  ///< for future use
  gchar delim[2];
  boolean is_template;

} lives_rfx_t;


boolean check_rfx_for_lives (lives_rfx_t *);

void do_rfx_cleanup(lives_rfx_t *);

void render_fx_get_params (lives_rfx_t *, const gchar *plugin_name, short status);

void sort_rfx_array (lives_rfx_t *in_array, int num_elements);

int find_rfx_plugin_by_name (const gchar *name, short status);

void rfx_copy (lives_rfx_t *src, lives_rfx_t *dest, boolean full);

void rfx_params_free(lives_rfx_t *rfx);

void rfx_free(lives_rfx_t *rfx);

void rfx_free_all (void);

void param_copy (lives_param_t *src, lives_param_t *dest, boolean full);


typedef struct {
  GList *list; ///< list of filter_idx from which user can delegate
  int delegate; ///< offset in list of current delegate
  gulong func; ///< menuitem activation function for current delegate
  lives_rfx_t *rfx; ///< pointer to rfx for current delegate (or NULL)
} lives_fx_candidate_t;

// filter types which can have candidates
#define FX_CANDIDATE_AUDIO_VOL 0
#define FX_CANDIDATE_RESIZER 1
#define FX_CANDIDATE_DEINTERLACE 2

#define MAX_FX_CANDIDATE_TYPES 3





boolean get_bool_param(void *value);
int get_int_param(void *value);
double get_double_param(void *value);
void get_colRGB24_param(void *value, lives_colRGB24_t *rgb);
void get_colRGBA32_param(void *value, lives_colRGBA32_t *rgba);

void set_bool_param(void *value, boolean );
void set_int_param(void *value, int );
void set_double_param(void *value, double );
void set_colRGB24_param(void *value, short red, short green, short blue);
void set_colRGBA32_param(void *value, short red, short green, short blue, short alpha);

/// return an array of parameter values
void **store_rfx_params (lives_rfx_t *);
void set_rfx_params_from_store (lives_rfx_t *rfx, void **store);
void rfx_params_store_free (lives_rfx_t *, void **store);

GList *array_to_string_list (gchar **array, int offset, int len);

lives_rfx_t *weed_to_rfx (weed_plant_t *plant, boolean show_reinits);
lives_param_t *weed_params_to_rfx(int npar, weed_plant_t *instance, boolean show_reinits);

gchar *plugin_run_param_window(const gchar *get_com, GtkVBox *vbox, lives_rfx_t **ret_rfx);

//////////////////////////////////////////////////////////////////////////////////////////
/// video playback plugin window - fixed part
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
void on_decplug_advanced_clicked (GtkButton *button, gpointer user_data);


/// for realtime effects, see effects-weed.h

#endif
