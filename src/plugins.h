// plugins.h
// LiVES
// (c) G. Finch 2003-2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PLUGINS_H
#define HAS_LIVES_PLUGINS_H

#ifdef GUI_GTK
#include <gmodule.h>
#endif

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

// generic plugins

#define PLUGIN_SUBTYPE_DLL 		"dll"
#define PLUGIN_SUBTYPE_BINARY 		"exe"
#define PLUGIN_SUBTYPE_SCRIPT 		"script"

#define PLUGIN_TYPE_DECODER			"decoder"
#define PLUGIN_TYPE_ENCODER			"encoder"
#define PLUGIN_TYPE_FILTER			"filter"
#define PLUGIN_TYPE_SOURCE			"source"
#define PLUGIN_TYPE_PLAYER 			"player"

#define PLUGIN_CHANNEL_NONE	0ul
#define PLUGIN_CHANNEL_VIDEO	(1<<0)ul
#define PLUGIN_CHANNEL_AUDIO	(1<<1)ul
#define PLUGIN_CHANNEL_TEXT	(1<<2)ul

#define PLUGIN_CHANNEL_DATA    		(1<<32)ul
#define PLUGIN_CHANNEL_STREAM    	(1<<33)ul
#define PLUGIN_CHANNEL_TTY    		(1<<34)ul
#define PLUGIN_CHANNEL_FILE    		(1<<35)ul

typedef enum {
  LIVES_INTENTION_UNKNOWN,

  // video players
  LIVES_INTENTION_PLAY,
  LIVES_INTENTION_STREAM,
  LIVES_INTENTION_TRANSCODE,  // encode / data in
  LIVES_INTENTION_RENDER,

  //LIVES_INTENTION_ENCODE, // encode / file in
  LIVES_INTENTION_BACKUP,
  LIVES_INTENTION_RESTORE,
  LIVES_INTENTION_DOWNLOAD,
  LIVES_INTENTION_UPLOAD,
  LIVES_INTENTION_EFFECT,
  LIVES_INTENTION_EFFECT_REALTIME, // or make cap ?
  LIVES_INTENTION_ANALYSE,
  LIVES_INTENTION_CONVERT,
  LIVES_INTENTION_MIX,
  LIVES_INTENTION_SPLIT,
  LIVES_INTENTION_DUPLICATE,
  LIVES_INTENTION_OTHER = 65536
} lives_intention_t;

/// type sepcific caps
// vpp
#define VPP_CAN_RESIZE    (1<<0)
#define VPP_CAN_RETURN    (1<<1)
#define VPP_LOCAL_DISPLAY (1<<2)
#define VPP_LINEAR_GAMMA  (1<<3)
#define VPP_CAN_RESIZE_WINDOW          		(1<<4)   /// can resize the image to fit the play window
#define VPP_CAN_LETTERBOX                  	(1<<5)
#define VPP_CAN_CHANGE_PALETTE			(1<<6)

typedef struct {
  uint64_t intent;
  uint64_t in_chan_types; ///< channel types accepted
  uint64_t out_chan_types; ///< channel types produced
  uint64_t intents; ///<
  uint64_t capabilities; ///< type specific capabilities
} lives_intentcaps_t;

typedef struct {
  char type[16];  ///< e.g. "decoder"
  char subtype[16];  ///< e.g. "dll"
  int api_version_major; ///< version of interface API
  int api_version_minor;
  char name[64];  ///< e.g. "mkv_decoder"
  int pl_version_major; ///< version of plugin
  int pl_version_minor;
  lives_intentcaps_t *capabilities;  ///< for future use
} lives_plugin_id_t;

LiVESList *get_plugin_list(const char *plugin_type, boolean allow_nonex,
                           const char *plugdir, const char *filter_ext);

// directory locations
#define PLUGIN_ENCODERS "encoders"
#define PLUGIN_DECODERS "decoders"
#define PLUGIN_VID_PLAYBACK "playback/video"
#define PLUGIN_AUDIO_STREAM "playback/audiostream"

#define AUDIO_STREAMER_NAME "audiostreamer.pl"

/// smogrify handles the directory differently for themes
#define PLUGIN_THEMES "themes"
#define PLUGIN_THEMES_CUSTOM "custom/themes"

/// uses WEED_PLUGIN_PATH
#define PLUGIN_EFFECTS_WEED "weed"
#define PLUGIN_WEED_FX_BUILTIN "effects/realtime/weed"

LiVESList *get_plugin_result(const char *command, const char *delim, boolean allow_blanks, boolean strip);
LiVESList *plugin_request(const char *plugin_type, const char *plugin_name, const char *request);
LiVESList *plugin_request_with_blanks(const char *plugin_type, const char *plugin_name, const char *request);
LiVESList *plugin_request_by_line(const char *plugin_type, const char *plugin_name, const char *request);
LiVESList *plugin_request_by_space(const char *plugin_type, const char *plugin_name, const char *request);
LiVESList *plugin_request_common(const char *plugin_type, const char *plugin_name, const char *request, const char *delim,
                                 boolean allow_blanks);

#define VPP_DEFS_FILE "vpp_defaults"

typedef struct {
  // playback
  char name[64];
  void *handle;

  // mandatory
  const char *(*module_check_init)(void);
  const char *(*version)(void);
  const char *(*get_description)(void);

  int *(*get_palette_list)(void);
  boolean(*set_palette)(int palette);
  uint64_t (*get_capabilities)(int palette);

  boolean(*render_frame)(int hsize, int vsize, ticks_t timecode, void **pixel_data, void **return_data,
                         weed_plant_t **play_params);

  boolean(*play_frame)(weed_layer_t *frame, ticks_t tc, weed_layer_t *ret);

  // optional
  weed_plant_t *(*weed_setup)(weed_bootstrap_f);
  boolean(*init_screen)(int width, int height, boolean fullscreen,
                        uint64_t window_id, int argc, char **argv);
  void (*exit_screen)(uint16_t mouse_x, uint16_t mouse_y);
  void (*module_unload)(void);
  const char *(*get_fps_list)(int palette);
  boolean(*set_fps)(double fps);

  const char *(*get_init_rfx)(int intention);

#ifdef __WEED_EFFECTS_H__
  ///< optional (but should return a weed plantptr array of paramtmpl and chantmpl, NULL terminated)
  const weed_plant_t **(*get_play_params)(weed_bootstrap_f f);
#endif

  // optional for YUV palettes
  int *(*get_yuv_palette_sampling)(int palette);
  int *(*get_yuv_palette_clamping)(int palette);
  int *(*get_yuv_palette_subspace)(int palette);
  int (*set_yuv_palette_sampling)(int sampling_type);
  int (*set_yuv_palette_clamping)(int clamping_type);
  int (*set_yuv_palette_subspace)(int subspace_type);

  // audio streaming (deprecated, use init_audio(), render_audio_frame())
  int *(*get_audio_fmts)(void);

  uint32_t audio_codec; //(deprecated, use init_audio(), render_audio_frame())
  // must match with the "acodec" LiVESList in interface.c
  // and bitmaps in the encder plugins

  // optional audio packeting
  boolean(*init_audio)(int in_sample_rate, int in_nchans, int argc, char **argv);
  boolean(*render_audio_frame_float)(float **audio, int nsamps);

  uint64_t capabilities;

  int fwidth, fheight; /// width in pixels, but converted to macropixels for the player

  int palette;
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;

  int fixed_fps_numer;
  int fixed_fps_denom;
  double fixed_fpsd;

  int extra_argc;
  char **extra_argv;

  const weed_plant_t **play_paramtmpls;
  weed_plant_t **play_params;
  weed_plant_t **alpha_chans;
  int num_play_params;
  int num_alpha_chans;
} _vid_playback_plugin;

_vid_playback_plugin *open_vid_playback_plugin(const char *name, boolean in_use);
void vid_playback_plugin_exit(void);
void close_vid_playback_plugin(_vid_playback_plugin *);
int64_t get_best_audio(_vid_playback_plugin *);
void save_vpp_defaults(_vid_playback_plugin *, char *file);
void load_vpp_defaults(_vid_playback_plugin *, char *file);

boolean vpp_try_match_palette(_vid_playback_plugin *vpp, weed_layer_t *layer);

#define DEFAULT_VPP "openGL"

#define DEF_VPP_HSIZE DEF_FRAME_HSIZE_UNSCALED
#define DEF_VPP_VSIZE DEF_FRAME_VSIZE_UNSCALED

const weed_plant_t *pp_get_param(weed_plant_t **pparams, int idx);
const weed_plant_t *pp_get_chan(weed_plant_t **pparams, int idx);

// encoder plugins

#define FFMPEG_ENCODER_NAME "ffmpeg_encoder"

#define MULTI_ENCODER_NAME "multi_encoder"
#define MULTI_ENCODER3_NAME "multi_encoder3"

#define HI_THEORA_FORMAT "hi-theora"
#define HI_MPEG_FORMAT "hi-mpg"
#define HI_H_MKV_FORMAT "hi_h-mkv"
#define HI_H_AVI_FORMAT "hi_h-avi"

void do_plugin_encoder_error(const char *plugin_name_or_null);

LiVESList *filter_encoders_by_img_ext(LiVESList *encoders, const char *img_ext);

typedef struct {
  char name[64];
  uint32_t audio_codec;
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
#define AUDIO_CODEC_OPUS 9

#define AUDIO_CODEC_MAX 31
  //
#define AUDIO_CODEC_NONE 32
#define AUDIO_CODEC_UNKNOWN 33

  uint32_t capabilities;

#define HAS_RFX (1<<0)

#define CAN_ENCODE_PNG (1<<2)
#define ENCODER_NON_NATIVE (1<<3)

  // current output format
  char of_name[64];
  char of_desc[128];
  int of_allowed_acodecs;
  char of_restrict[1024];
  char of_def_ext[16];
  char ptext[512];
}
_encoder;

typedef struct {
  /// a ctiming_ratio of 0. indicates that none of the other values are set.
  // a value > 0. indicates some values are set. Unset values may be left as 0., or
  /// A value < 0. means that the value is known to be non-zero, but cannot be accurately measured.
  /// In this case, calculations involving this quantity should be avoided, as the result cannot be determined.

  double ctiming_ratio; // dynamic multiplier for timing info, depends on machine load and other factors.
  double idecode_time; /// avg time to decode inter frame
  double kdecode_time; /// avg time to decode keyframe
  double buffer_flush_time; /// time to flush buffers after a seek
  double kframe_nseek_time; /// avg time to seek to following keyframe (const)
  double kframe_delay_time; /// avg extra time per iframe to arrive at following kframe

  double kframe_kframe_time; /// avg time to seek from keyframe to keyframe (const) :: default == 0. (use kframe_nseek_time)
  double kframe_inter_time; /// extra time to seek from kframe to kframe per iframe between them :: default == kframe_delay_time
  double kframe_extra_time; /// extra time to seek from kframe to kframe per kframe between them :: default == kframe_inter_time

  // examples:
  // iframe to next kframe with decode: kframe_nseek_time + n * kframe_delay_time + buffer_flush_tome + kdecode_time
  // where n is the number of iframes skipped over

  // seek from iframe to another iframe, passing over several kframes, decoding frames from final kframe to target

  /// kframe_nseek_time + A * kframe_delay_time + kframe_kframe_time + B * kframe_inter_time * C * kframe_extra_time +
  /// kdecode_time + D * idecode_time
  /// where A == nframes between origin and next kframe, B == iframes between kframse, C == kframes between kframes,
  /// D = iframes after target kframe
  /// this can approximated as: kframe_nseek_time + (A + B + C) * kframe_delay_time + kdecode_time + D * idecode_time

  double xvals[64];  /// extra values which may be
} adv_timing_t;

// defined in plugins.c for the whole app
extern const char *const anames[AUDIO_CODEC_MAX];

// decoder plugins

// seek_flags is a bitmap

/// good
#define LIVES_SEEK_FAST (1<<0)

/// not so good
#define LIVES_SEEK_NEEDS_CALCULATION (1<<1)
#define LIVES_SEEK_QUALITY_LOSS (1<<2)

typedef struct _lives_clip_data {
  // fixed part
  lives_struct_def_t lsd;
  lives_plugin_id_t plugin_id;

  malloc_f  *ext_malloc;
  free_f    *ext_free;
  memcpy_f  *ext_memcpy;
  memset_f  *ext_memset;
  memmove_f *ext_memmove;
  realloc_f *ext_realloc;
  calloc_f  *ext_calloc;

  void *priv;

  char *URI; ///< the URI of this cdata

  int nclips; ///< number of clips (titles) in container
  char container_name[512]; ///< name of container, e.g. "ogg" or NULL

  char title[1024];
  char author[1024];
  char comment[1024];

  /// plugin should init this to 0 if URI changes
  int current_clip; ///< current clip number in container (starts at 0, MUST be <= nclips) [rw host]

  // video data
  int width;
  int height;
  int64_t nframes;
  lives_interlace_t interlace;
  int *rec_rowstrides; ///< if non-NULL, plugin can set recommended vals, pointer to single value set by host

  /// x and y offsets of picture within frame
  /// for primary pixel plane
  int offs_x;
  int offs_y;
  int frame_width;  ///< frame is the surrounding part, including any black border (>=width)
  int frame_height;

  float par; ///< pixel aspect ratio (sample width / sample height)

  float video_start_time;

  float fps;

  /// optional info ////////////////
  float max_decode_fps; ///< theoretical value with no memcpy
  int64_t fwd_seek_time;
  int64_t jump_limit; ///< for internal use

  int64_t kframe_start; /// frame number of first keyframe (usually 0)
  int64_t kframe_dist; /// number forames from one keyframe to the next, 0 if unknown
  //////////////////////////////////

  int *palettes; ///< list of palettes which the format supports, terminated with WEED_PALETTE_END

  /// plugin should init this to palettes[0] if URI changes
  int current_palette;  ///< current palette [rw host]; must be contained in palettes

  /// plugin can change per frame
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;
  int frame_gamma; ///< values WEED_GAMMA_UNKNOWN (0), WEED_GAMMA_SRGB (1), WEED_GAMMA_LINEAR (2)

  char video_name[512]; ///< name of video codec, e.g. "theora" or NULL

  /* audio data */
  int arate;
  int achans;
  int asamps;
  boolean asigned;
  boolean ainterleaf;
  char audio_name[512]; ///< name of audio codec, e.g. "vorbis" or NULL

  /// plugin can change per frame
  int seek_flag; ///< bitmap of seek properties

#define SYNC_HINT_AUDIO_TRIM_START (1<<0)
#define SYNC_HINT_AUDIO_PAD_START (1<<1)
#define SYNC_HINT_AUDIO_TRIM_END (1<<2)
#define SYNC_HINT_AUDIO_PAD_END (1<<3)

#define SYNC_HINT_VIDEO_PAD_START (1<<4)
#define SYNC_HINT_VIDEO_PAD_END (1<<5)

  int sync_hint;

} lives_clip_data_t;


typedef struct {
  // playback
  const char *name; ///< plugin name
  void *handle; ///< may be shared between several instances

  // mandatory
  const char *(*version)(void);

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
  boolean(*get_frame)(const lives_clip_data_t *, int64_t frame, int *rowstrides, int height, void **pixel_data);

  /// free buffers when we arent playing sequentially / on standby
  boolean(*chill_out)(const lives_clip_data_t *);

  /// call this for each cdata before unloading the module
  void (*clip_data_free)(lives_clip_data_t *);

  // optional
  const char *(*module_check_init)(void);
  boolean(*set_palette)(lives_clip_data_t *);
  int64_t (*rip_audio)(const lives_clip_data_t *, const char *fname, int64_t stframe, int64_t nframes,
                       unsigned char **abuff);
  void (*rip_audio_cleanup)(const lives_clip_data_t *cdata);
  void (*module_unload)(void);
} lives_decoder_sys_t;

typedef struct {
  const lives_decoder_sys_t *decoder;
  lives_clip_data_t *cdata;
  int refs;
} lives_decoder_t;

LiVESList *load_decoders(void);
boolean chill_decoder_plugin(int fileno);
boolean decoder_plugin_move_to_first(const char *name);
const lives_clip_data_t *get_decoder_cdata(int fileno, LiVESList *disabled, const lives_clip_data_t *fake_cdata);
void close_decoder_plugin(lives_decoder_t *);
void close_clip_decoder(int clipno);
lives_decoder_sys_t *open_decoder_plugin(const char *plname);
void get_mime_type(char *text, int maxlen, const lives_clip_data_t *);
void unload_decoder_plugins(void);
lives_decoder_t *clone_decoder(int fileno);

// RFX plugins

/// external rendered fx plugins (RFX plugins)
#define PLUGIN_RENDERED_EFFECTS_BUILTIN "effects/rendered/"

/// in the config directory
#define PLUGIN_RENDERED_EFFECTS_CUSTOM "plugins/effects/rendered/custom/"
#define PLUGIN_RENDERED_EFFECTS_TEST "plugins/effects/rendered/test/"

/// rfx scripts for the SDK
#define PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS "effects/RFXscripts/"

/// in the config directory
#define PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS "plugins/effects/RFXscripts/custom/"
#define PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS "plugins/effects/RFXscripts/test/"

/// scraps are passed between programs to generate param windows
#define PLUGIN_RFX_SCRAP ""

/// max number of display widgets per parameter (currently 7 for transition param with mergealign -
/// spin + label + knob + scale + in + out + dummy
// TODO : use enum for widget type
#define MAX_PARAM_WIDGETS 128

#define RFX_MAX_NORM_WIDGETS 16

/// special widgets
#define WIDGET_RB_IN 			16
#define WIDGET_RB_OUT 		17
#define WIDGET_RB_DUMMY 		18

/// length of max string (not including terminating NULL) for LiVES-perl
#define RFX_MAXSTRINGLEN (PATH_MAX - 1)

typedef enum {
  LIVES_PARAM_UNKNOWN = 0,
  LIVES_PARAM_NUM,
  LIVES_PARAM_BOOL,
  LIVES_PARAM_COLRGB24,
  LIVES_PARAM_STRING,
  LIVES_PARAM_STRING_LIST,
  LIVES_PARAM_COLRGBA32,

  LIVES_PARAM_UNDISPLAYABLE = 65536
} lives_param_type_t;

typedef enum {
  LIVES_RFX_SOURCE_RFX = 0,
  LIVES_RFX_SOURCE_WEED,
  LIVES_RFX_SOURCE_NEWCLIP
} lives_rfx_source_t;

typedef enum {
  LIVES_PARAM_SPECIAL_TYPE_NONE = 0, // normal parameter type

  // framedraw types
  LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK,  ///< type may be used in framedraw
  LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT,  ///< type may be used in framedraw
  LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT,  ///< type may be used in framedraw
  LIVES_PARAM_SPECIAL_TYPE_SCALEDPOINT,  ///< type may be used in framedraw

  // text widget types
  LIVES_PARAM_SPECIAL_TYPE_FILEREAD,
  LIVES_PARAM_SPECIAL_TYPE_FILEWRITE,
  LIVES_PARAM_SPECIAL_TYPE_PASSWORD,
  LIVES_PARAM_SPECIAL_TYPE_FONT_CHOOSER,

  // misc types
  LIVES_PARAM_SPECIAL_TYPE_MERGEALIGN,
  LIVES_PARAM_SPECIAL_TYPE_ASPECT_RATIO
} lives_param_special_t;

typedef struct {
  // weed style part
  char *name;
  char *desc;

  char *label;
  int flags;
  boolean use_mnemonic;
  int hidden;

  // reason(s) for hiding [bitmap]
#define HIDDEN_GUI (1<<0)
#define HIDDEN_MULTI (1<<1)
#define HIDDEN_NEEDS_REINIT (1<<2)
#define HIDDEN_COMPOUND_INTERNAL (1<<3)

  double step_size;
  //int copy_to;
  boolean transition;

#define REINIT_FUNCTIONAL 	1
#define REINIT_VISUAL 		2

  int reinit;

  boolean wrap;
  int group;
  lives_param_type_t type;

  int dp;  ///<decimals, 0 for int and bool
  void *value;  ///< current value(s)

  double min;
  double max; ///< for string this is max characters

  void *def; ///< default values
  LiVESList *list; ///< for string list (choices)

  /// multivalue type - single value, multi value, or per channel
  short multi;
#define PVAL_MULTI_NONE 0
#define PVAL_MULTI_ANY 1
#define PVAL_MULTI_PER_CHANNEL 2

  //--------------------------------------------------
  // extras for LiVES

  /// TODO - change to LiVESWidget **widgets, terminated with a NULL
  LiVESWidget *widgets[MAX_PARAM_WIDGETS]; ///< widgets which hold value/RGBA settings
  boolean onchange; ///< is there a trigger ?

  boolean changed;
  boolean edited;

  boolean change_blocked;

  void *source;

  lives_rfx_source_t source_type;

  // this may change
  lives_param_special_t special_type; // the visual modification type (see paramspecial.h)
  int special_type_index; // index within special_type (e.g for DEMASK, 0==left, 1==top, 2==width, 3==height)
} lives_param_t;

typedef enum {
  RFX_STATUS_BUILTIN = 0, ///< factory presets
  RFX_STATUS_CUSTOM = 1, ///< custom effects in the custom menu
  RFX_STATUS_TEST = 2, ///< test effects in the advanced menu
  RFX_STATUS_ANY = 3, ///< indicates free choice of statuses
  RFX_STATUS_WEED = 4, ///< indicates an internal RFX, created from a weed instance
  RFX_STATUS_SCRAP = 5, ///< used for parsing RFX scraps from external apps
  RFX_STATUS_INTERNAL = 6, ///< used for parsing RFX scraps generated internally (will possiblky replace SCRAP)

  // these are only used when prompting for a name
  RFX_STATUS_COPY = 128, ///< indicates a copy operation to test
  RFX_STATUS_RENAME = 129 ///< indicates a copy operation to test
} lives_rfx_status_t;

typedef struct {
  char *name;  ///< the name of the executable (so we can run it !)
  char *menu_text; ///< for Weed, this is the filter_class "name"
  char *action_desc; ///< for Weed "Applying $s"
  int min_frames; ///< for Weed, 1
  int num_in_channels;
  lives_rfx_status_t status;

  uint32_t props;
#define RFX_PROPS_SLOW        0x0001  ///< hint to GUI
#define RFX_PROPS_MAY_RESIZE  0x0002 ///< is a tool
#define RFX_PROPS_BATCHG      0x0004 ///< is a batch generator

#define RFX_PROPS_RESERVED1   0x1000
#define RFX_PROPS_RESERVED2   0x2000
#define RFX_PROPS_RESERVED3   0x4000
#define RFX_PROPS_AUTO_BUILT  0x8000

  LiVESWidget *menuitem;  ///< the menu item which activates this effect
  int num_params;
  uint32_t flags; /// internal use
#define RFX_FLAGS_NO_SLIDERS 	0x0001
#define RFX_FLAGS_NO_RESET 	0x0002

  lives_param_t *params;
  lives_rfx_source_t source_type;
  void *source;  ///< points to the source (e.g. a weed_plant_t)
  char delim[2];
  char rfx_version[64];
  LiVESList *gui_strings;  ///< rfxscript for constructing the params, param window and onchange triggers
  LiVESList *onchange_strings;  ///< rfxscript for constructing the params, param window and onchange triggers
  boolean is_template;
  int needs_reinit;
} lives_rfx_t;

boolean check_rfx_for_lives(lives_rfx_t *);

void do_rfx_cleanup(lives_rfx_t *);

void render_fx_get_params(lives_rfx_t *, const char *plugin_name, short status);

void sort_rfx_array(lives_rfx_t *in_array, int num_elements);

int find_rfx_plugin_by_name(const char *name, short status);

void rfx_copy(lives_rfx_t *dest, lives_rfx_t *src, boolean full);

void rfx_params_free(lives_rfx_t *);

void rfx_free(lives_rfx_t *);

void rfx_free_all(void);

void param_copy(lives_param_t *dest, lives_param_t *src, boolean full);

lives_param_t *find_rfx_param_by_name(lives_rfx_t *, const char *name);

boolean set_rfx_param_by_name_string(lives_rfx_t *, const char *name, const char *value, boolean update_visual);

boolean get_rfx_param_by_name_string(lives_rfx_t *rfx, const char *name, char **return_value);

typedef struct {
  LiVESList *list; ///< list of filter_idx from which user can delegate
  int delegate; ///< offset in list of current delegate
  ulong func; ///< menuitem activation function for current delegate
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
void get_colRGB24_param(void *value, lives_colRGB48_t *rgb);
void get_colRGBA32_param(void *value, lives_colRGBA64_t *rgba);

void set_bool_param(void *value, boolean);
void set_int_param(void *value, int);
void set_double_param(void *value, double);
void set_colRGB24_param(void *value, short red, short green, short blue);
void set_colRGBA32_param(void *value, short red, short green, short blue, short alpha);

/// return an array of parameter values
void **store_rfx_params(lives_rfx_t *);
void set_rfx_params_from_store(lives_rfx_t *rfx, void **store);
void rfx_params_store_free(lives_rfx_t *, void **store);

LiVESList *array_to_string_list(char **array, int offset, int len);

lives_rfx_t *weed_to_rfx(weed_plant_t *plant, boolean show_reinits);
lives_param_t *weed_params_to_rfx(int npar, weed_plant_t *instance, boolean show_reinits);

void rfx_clean_exe(lives_rfx_t *rfx);

char *plugin_run_param_window(const char *scrap_text, LiVESVBox *vbox, lives_rfx_t **ret_rfx);

//////////////////////////////////////////////////////////////////////////////////////////
/// video playback plugin window - fixed part

typedef struct {
  _vid_playback_plugin *plugin;
  LiVESWidget *dialog;
  LiVESWidget *spinbuttonh;
  LiVESWidget *spinbuttonw;
  LiVESWidget *apply_fx;
  LiVESWidget *fps_entry;
  LiVESWidget *pal_entry;
  lives_rfx_t *rfx;
  boolean keep_rfx;
  int intention;
} _vppaw;

_vppaw *on_vpp_advanced_clicked(LiVESButton *, livespointer);

void on_decplug_advanced_clicked(LiVESButton *button, livespointer user_data);

LiVESList *get_external_window_hints(lives_rfx_t *rfx);
boolean check_encoder_restrictions(boolean get_extension, boolean user_audio, boolean save_all);

/// for realtime effects, see effects-weed.h

#endif
