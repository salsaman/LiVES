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

typedef struct _param_t lives_param_t;
typedef weed_plant_t weed_param_t;

#include "intents.h"

// generic objects / plugins

#define DLL_EXT "so"

// perhaps intentions could be used instead, ie. each plugin just provides a set of intentcaps
// and an interface for each one ?
#define PLUGIN_TYPE_DECODER		256//		"decoder"
#define PLUGIN_TYPE_ENCODER		257//		"encoder"
#define PLUGIN_TYPE_FILTER		258//		"filter"
#define PLUGIN_TYPE_SOURCE		259//		"source"
#define PLUGIN_TYPE_PLAYER      	260//		"player"

#define PLUGIN_TYPE_BASE_OFFSET		256 // subtract from types to get 0 base
#define PLUGIN_TYPE_MAX_BUILTIN		1023

// subtypes (in future derived from intentcaps)
#define PLUGIN_SUBTYPE_VIDEO_PLAYER     32768

#define PLUGIN_TYPE_FIRST_CUSTOM	65536

#define PLUGIN_PKGTYPE_DYNAMIC 		128//	dynamic library
#define PLUGIN_PKGTYPE_EXE 		129//	binary executable
#define PLUGIN_PKGTYPE_SCRIPT 		130//	interpreted script

#define PLUGIN_CHANNEL_NONE	0ull
#define PLUGIN_CHANNEL_VIDEO	(1ull << 0)
#define PLUGIN_CHANNEL_AUDIO	(1ull << 1)
#define PLUGIN_CHANNEL_TEXT	(1ull << 2)

#define PLUGIN_CHANNEL_DATA    		(1ull << 32)
#define PLUGIN_CHANNEL_STREAM    	(1ull << 33)
#define PLUGIN_CHANNEL_TTY    		(1ull << 34)
#define PLUGIN_CHANNEL_FILE    		(1ull << 35)

#define LIVES_DEVSTATE_NORMAL 0 // normal development status, presumed bug free
#define LIVES_DEVSTATE_RECOMMENDED 1 // recommended, suitable for default use
#define LIVES_DEVSTATE_CUSTOM 2 // plugin is not normally shipped with base package
#define LIVES_DEVSTATE_TESTING 3 // waring - plugin is being tested and may not function correctly

#define LIVES_DEVSTATE_UNSTABLE -1 // warning - may be unstable in specific circumstances
#define LIVES_DEVSTATE_BROKEN -2 // WARNING - plugin is know to function incorrectly
#define LIVES_DEVSTATE_AVOID -3 // WARNING - plugin should be completely ignored

// in future, all plugins will have only a single mandatory function, get_plugin_id
// which should return a lives_plugin_id_t

typedef struct {
  uint64_t uid; // fixed enumeration
  char name[32];  ///< e.g. "mkv_decoder"
  int pl_version_major; ///< version of plugin
  int pl_version_minor;
  int devstate; // e.g. LIVES_DEVSTATE_UNSTABLE

  // the following would normally be filled via a type specific header:
  uint64_t type;  ///< e.g. "decoder"
  int api_version_major; ///< version of interface API
  int api_version_minor;
  uint64_t pkgtype;  ///< e.g. dynamic
  char script_lang[32];  ///< for scripted types only, the script interpreter, e.g. "perl", "python3"

  // if intentcaps is set, then type, api_version_* become optional
  lives_intentcap_t *intentcaps;  /// array of intentcaps (NULL terminated)
} lives_plugin_id_t;

typedef struct {
  lives_plugin_id_t *pl_id;
  void *pl_priv;
} lives_plugin_t;

typedef struct {
  void *parent;
  LiVESList *offspring;
} lives_relation_t;

LiVESList *get_plugin_list(const char *plugin_type, boolean allow_nonex,
                           const char *plugdir, const char *filter_ext);

#define PLUGINS_LITERAL "plugins"
#define EFFECTS_LITERAL "effects"
#define THEMES_LITERAL "themes"

#define CUSTOM_LITERAL "custom"
#define TEST_LITERAL "test"

#define N_PLUGIN_SUBDIRS 4 // decoders, encoders, effects, playback

boolean check_for_plugins(const char *dirn, boolean check_only);
boolean find_prefix_dir(const char *predirn, boolean check_only);

// directory locations
#define ENCODERS_LITERAL "encoders"
#define DECODERS_LITERAL "decoders"

#define PLUGIN_ENCODERS ENCODERS_LITERAL
#define PLUGIN_DECODERS DECODERS_LITERAL

#define PLAYBACK_LITERAL "playback"

#define PLUGIN_VID_PLAYBACK PLAYBACK_LITERAL LIVES_DIR_SEP  "video"
#define PLUGIN_AUDIO_STREAM PLAYBACK_LITERAL LIVES_DIR_SEP "audiostream"

#define AUDIO_STREAMER_NAME "audiostreamer.pl"

/// smogrify handles the directory differently for themes
#define PLUGIN_THEMES THEMES_LITERAL
#define PLUGIN_THEMES_CUSTOM CUSTOM_LITERAL LIVES_DIR_SEP THEMES_LITERAL

#define REALTIME_LITERAL "realtime"

/// uses WEED_PLUGIN_PATH
#define WEED_LITERAL "weed"
#define PLUGIN_EFFECTS_WEED WEED_LITERAL
#define PLUGIN_WEED_FX_BUILTIN EFFECTS_LITERAL LIVES_DIR_SEP REALTIME_LITERAL LIVES_DIR_SEP WEED_LITERAL

LiVESList *get_plugin_result(const char *command, const char *delim, boolean allow_blanks, boolean strip);
LiVESList *plugin_request(const char *plugin_type, const char *plugin_name, const char *request);
LiVESList *plugin_request_with_blanks(const char *plugin_type, const char *plugin_name, const char *request);
LiVESList *plugin_request_by_line(const char *plugin_type, const char *plugin_name, const char *request);
LiVESList *plugin_request_by_space(const char *plugin_type, const char *plugin_name, const char *request);
LiVESList *plugin_request_common(const char *plugin_type, const char *plugin_name, const char *request, const char *delim,
                                 boolean allow_blanks);

// descriptive text for version string formatting
#define DECPLUG_VER_DESC " engine v "

#define VPP_DEFS_FILE "vpp_defaults"

typedef struct {
  // mandatory
  const lives_plugin_id_t *(*get_plugin_id)(void);
  const lives_plugin_id_t *id;

  // playback
  char *soname; ///< plugin name (soname without .so)
  void *handle;

  // mandatory
  const char *(*module_check_init)(void);
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

  const char *(*get_init_rfx)(lives_intentcap_t *icaps);

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
  uint64_t *(*get_audio_fmts)(void);

  uint64_t audio_codec; //(deprecated, use init_audio(), render_audio_frame())
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
uint64_t get_best_audio(_vid_playback_plugin *);
void save_vpp_defaults(_vid_playback_plugin *, char *file);
void load_vpp_defaults(_vid_playback_plugin *, char *file);

boolean vpp_try_match_palette(_vid_playback_plugin *, weed_layer_t *);
int get_best_vpp_palette_for(_vid_playback_plugin *, int palette);

#if HAVE_OPENGL && HAVE_X11 && HAVE_XRENDER
#define DEFAULT_VPP_NAME "openGL"
#define DEFAULT_VPP_UID 0XA6750BDAC53DF23F
#endif

#define DEF_VPP_HSIZE DEF_FRAME_HSIZE_UNSCALED
#define DEF_VPP_VSIZE DEF_FRAME_VSIZE_UNSCALED

const weed_plant_t *pp_get_param(weed_plant_t **pparams, int idx);
const weed_plant_t *pp_get_chan(weed_plant_t **pparams, int idx);

// encoder plugins

// TODO - use plugin UIDs
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
  uint64_t audio_codec;
  // match with bitmaps in the encoder plugins
  // and also anames array in plugins.c (see below)

  uint32_t capabilities;

  // current output format
  char of_name[64];
  char of_desc[128];
  uint64_t of_allowed_acodecs;
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
  double const_time; /// avg const time apart from seek / decode (e.g. memcpy)
  double ib_time; /// avg time to decode inter / b frame
  double k_time; /// avg time to decode keyframe not following seek (if we can distinguish from ib_time. else use ib_time)
  double ks_time; /// avg time to seek and decode kframe
  double kb_time; /// avg time to seek / decode backwd kframe
  double blockread_time; /// avg time to read . parse a data block
  double seekback_time; // unused

  double xvals[64];  /// extra values which may be stored depending on codec
} adv_timing_t;

// decoder plugins

// seek_flags is a bitmap

typedef struct _lives_memfuncs {
  malloc_f  *ext_malloc;
  free_f    *ext_free;
  memcpy_f  *ext_memcpy;
  memset_f  *ext_memset;
  memmove_f *ext_memmove;
  realloc_f *ext_realloc;
  calloc_f  *ext_calloc;
} ext_memfuncs_t;

typedef struct _lives_clip_data {
  // fixed part
  lives_struct_def_t *lsd;

  // TODO - replace with ext_memfuncs_t
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
  char container_name[512]; ///< name of container, e.g. "ogg" (if known)

  char title[1024];
  char author[1024];
  char comment[1024];

  /// plugin should init this to 0 if URI changes
  int current_clip; ///< current clip number in container (starts at 0, MUST be <= nclips) [rw host]

  // video data
  int width, height;
  int64_t nframes; // number of frames in current clip
  lives_interlace_t interlace; // frame interlacing (if any)

  // the host may initialise this by creating an array of n ints, where n is the number of planes in the current palette
  // the plugin may then fill the n values with its own rowstride values. The host can then use the values on the
  // subsequent call to get_frame(). The plugin MUST set the values each time a frame is returned.
  int *rec_rowstrides;

  /// x and y offsets of picture within frame
  /// for primary pixel plane
  int offs_x, offs_y;
  ///< frame is the surrounding part, including any blank border ( >= width, height )
  int frame_width, frame_height;

  float par; ///< pixel aspect ratio (sample width / sample height) (default of 0. implies square pixels)

  float video_start_time; // if the clip is a chapter, thhen this can be set to the chapter start time, info only

  float fps; // playback frame rate (variable rates not supported currently)

  int *palettes; ///< list of palettes which the format supports, terminated with WEED_PALETTE_END

  /// plugin should init this to palettes[0] if URI changes
  int current_palette;  ///< current palette [rw host]; must be contained in palettes

  /// plugin can change per frame
  int YUV_sampling, YUV_clamping, YUV_subspace;
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

  int sync_hint; ///< hint to host how to correct in case of audio / video stream misalignments

  /// decoder details /////

  int64_t last_frame_decoded; // last frame read / decoded from video stream

  /// optional info ////////////////

  //< estimate of the forward frame difference beyond which it always becomes
  // quicker to re-seek rathere than decode sequntially
  // 0 means no estimate, it is suggested to set this either to kframe_dist if valid, else to some other measured value
  int64_t jump_limit;

  float max_decode_fps; ///< theoretical value with no memcpy
  int64_t fwd_seek_time; // deprecated

  // handling for keyframes / seek points
  // these values are intended for use with delay estimation
  // (it is ASSUMED that we can jump to any keyframe and begin decoding forward from there)


  boolean kframes_complete; /// TRUE if all keyframes have been mapped (e.g read from index)

  // Otherwise,
  // if keyframes are regularly spaced, then this information can be used to guess the positions
  // of as yet unmapped keyframes.

  int64_t kframe_dist;
  //////////////////////////////////

  adv_timing_t adv_timing;

  boolean debug;
} lives_clip_data_t;

typedef struct {
  // mandatory
  const lives_plugin_id_t *(*get_plugin_id)(void);
  const lives_plugin_id_t *id;

  // playback
  const char *soname; ///< plugin name (soname without .so)
  void *handle; ///< may be shared between several instances

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

  /// free buffers when we aren't playing sequentially / on standby
  boolean(*chill_out)(const lives_clip_data_t *);

  void (*clip_data_free)(lives_clip_data_t *);

  // optional
  const char *(*module_check_init)(void);
  boolean(*set_palette)(lives_clip_data_t *);
  int64_t (*rip_audio)(const lives_clip_data_t *, const char *fname, int64_t stframe, int64_t nframes,
                       unsigned char **abuff);
  void (*rip_audio_cleanup)(const lives_clip_data_t *);
  void (*module_unload)(void);
  double (*estimate_delay)(const lives_clip_data_t *, int64_t tframe);
  double (*estimate_delay_full)(const lives_clip_data_t *, int64_t tframe,  int64_t last_frame,
                                double *confidence);
} lives_decoder_sys_t;

typedef struct {
  const lives_decoder_sys_t *dpsys;
  lives_clip_data_t *cdata;
  double timing_ratio;
  double timing_const;
  pthread_mutex_t mutex;
  lives_relation_t relations;
} lives_decoder_t;

lives_clip_data_t *get_clip_cdata(int clipno);
LiVESList *locate_decoders(LiVESList *);
LiVESList *load_decoders(void);
boolean chill_decoder_plugin(int fileno);
boolean decoder_plugin_move_to_first(const char *name, uint64_t uid);
const lives_clip_data_t *get_decoder_cdata(int fileno, LiVESList *disabled, const lives_clip_data_t *fake_cdata);
void close_decoder_plugin(lives_decoder_sys_t *);
void close_clip_decoder(int clipno);
lives_decoder_sys_t *open_decoder_plugin(const char *plname);
void get_mime_type(char *text, int maxlen, const lives_clip_data_t *);
void unload_decoder_plugins(void);
void clip_decoder_free(lives_decoder_t *);

lives_decoder_t *clone_decoder(int fileno);
lives_decoder_t *add_decoder_clone(int nclip);
lives_decoder_t *get_decoder_clone(int nclip);
boolean swap_decoder_clone(int nclip, lives_decoder_t *);
boolean free_decoder_clone(int nclip, lives_decoder_t *);

void propogate_timing_data(lives_decoder_t *);

// RFX plugins

#define RENDERED_LITERAL "rendered"
#define RFX_SCRIPTS_LITERAL "RFXscripts"

/// external rendered fx plugins (RFX plugins)
#define RFX_PLUG_DIR EFFECTS_LITERAL LIVES_DIR_SEP RENDERED_LITERAL
#define PLUGIN_RENDERED_EFFECTS_BUILTIN RFX_PLUG_DIR

/// in the config directory
#define PLUGIN_RENDERED_EFFECTS_CUSTOM PLUGINS_LITERAL LIVES_DIR_SEP RFX_PLUG_DIR LIVES_DIR_SEP CUSTOM_LITERAL
#define PLUGIN_RENDERED_EFFECTS_TEST PLUGINS_LITERAL LIVES_DIR_SEP RFX_PLUG_DIR LIVES_DIR_SEP TEST_LITERAL

/// rfx scripts for the SDK
#define RFX_SCRIPT_DIR EFFECTS_LITERAL LIVES_DIR_SEP RFX_SCRIPTS_LITERAL
#define PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS RFX_SCRIPT_DIR

/// in the config directory
#define PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS PLUGINS_LITERAL LIVES_DIR_SEP RFX_SCRIPT_DIR LIVES_DIR_SEP CUSTOM_LITERAL
#define PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS PLUGINS_LITERAL LIVES_DIR_SEP RFX_SCRIPT_DIR LIVES_DIR_SEP TEST_LITERAL

/// scraps are passed between programs to generate param windows
#define PLUGIN_RFX_SCRAP ""

/// max number of display widgets per parameter
/// (currently 7 in ppractice for transition param with mergealign -
/// spin + label + knob + scale + in + out + dummy
// TODO : use enum for widget type
#define MAX_PARAM_WIDGETS 128

#define RFX_MAX_NORM_WIDGETS 16

/// special widgets
#define WIDGET_RB_IN 			16
#define WIDGET_RB_OUT	 		17
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

#define LIVES_PARAM_TYPE_UNDEFINED LIVES_PARAM_UNKNOWN

typedef enum {
  LIVES_RFX_SOURCE_RFX = 0,
  LIVES_RFX_SOURCE_WEED,
  LIVES_RFX_SOURCE_OBJECT,
  LIVES_RFX_SOURCE_NEWCLIP,
  LIVES_RFX_SOURCE_EXTERNAL
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

/// parameter is for display only
#define PARAM_FLAG_READONLY 	0x00000001
// parameter is "optional"
#define PARAM_FLAG_OPTIONAL 	0x10000000
// the 'value' has been set (after being initialised to "default"
#define PARAM_FLAG_VALUE_SET	0x20000000

struct _param_t {
  // weed style part
  char *name;
  char *desc;

  char *label;

  uint64_t flags;

  boolean use_mnemonic;
  uint32_t hidden;

  // reason(s) for hiding [bitmap]
  /// structural (permanent)
#define HIDDEN_UNDISPLAYABLE		(1 << 0)
#define HIDDEN_GUI_PERM			(1 << 1)
#define HIDDEN_MULTI			(1 << 2)

#define HIDDEN_STRUCTURAL (0x00FF)

  /// non-structural (temporary)
#define HIDDEN_NEEDS_REINIT 		(1 << 16)
#define HIDDEN_GUI_TEMP			(1 << 17)

#define HIDDEN_TEMPORARY (0xFF00)

  double step_size;
  boolean snap_to_step;

  boolean transition;

#define REINIT_FUNCTIONAL 	1
#define REINIT_VISUAL 		2

  int reinit;

  boolean wrap; // if TRUE value wraps from max -> min and min -> max (eg radians)
  int group; // radiobutton group for SWITCH (ignore if <= 0)

  lives_param_type_t type;

  int dp;  ///<decimals, 0 for int and bool

  int nvalues; // if > 0, value is an array of n itemss, for colRG24i int[n][3], etc.

  void *value;  ///< current value(s); may be cast to poiner of appropriate type

  // TODO - use union {}
  double min; //< display len for string
  double max; ///< for string this is max characters

  void *def; ///< default value(s)

  LiVESList *list; ///< for string list (choices), DISPLAYED values (list->data os const char *)

  ///< for string list, the ACTUAL (not display) values
  /// if they differ from idx value, otherwise NULL
  // if this points to list, then vlist_type is LIVES_PARAM_STRING, and vlist_dp is 0
  LiVESList *vlist;

  // type and decimals of vlist items
  lives_param_type_t vlist_type;
  int vlist_dp;

  /// multivalue type - single value, multi value, or per channel
  short multi;
#define PVAL_MULTI_NONE 0
#define PVAL_MULTI_ANY 1
#define PVAL_MULTI_PER_CHANNEL 2

  //--------------------------------------------------
  // extras for LiVES

  char *units; // optional display detail (eg. "Hz", "sec."


  LiVESWidget *widgets[MAX_PARAM_WIDGETS]; ///< widgets which hold value/RGBA settings
  int nwidgets;

  boolean onchange; ///< is there a trigger ?

  boolean change_blocked;

  void *source;

  lives_rfx_source_t source_type;

  // this may change
  lives_param_special_t special_type; // the visual modification type (see paramspecial.h)
  int special_type_index; // index within special_type (e.g for DEMASK, 0==left, 1==top, 2==width, 3==height)
};

typedef enum {
  RFX_STATUS_BUILTIN, ///< factory presets
  RFX_STATUS_CUSTOM, ///< custom effects in the custom menu
  RFX_STATUS_TEST, ///< test effects in the advanced menu
  RFX_STATUS_ANY, ///< indicates free choice of statuses (e.g. during plugin loading)
  RFX_STATUS_WEED, ///< indicates an internal RFX, created from a weed instance
  RFX_STATUS_SCRAP, ///< used for parsing RFX scraps from external apps
  RFX_STATUS_INTERNAL, ///< used for parsing RFX scraps generated internally (will possiblky replace SCRAP)
  RFX_STATUS_INTERFACE, ///< indicates a "dumb" interface, used just to display and collect param values
  RFX_STATUS_OBJECT, ///< created from a lives_object

  // these are only used when prompting for a name
  RFX_STATUS_COPY = 128, ///< indicates a copy operation to test
  RFX_STATUS_RENAME = 129 ///< indicates a copy operation to test
} lives_rfx_status_t;

#define RFX_FLAGS_NO_SLIDERS 	0x10000001
#define RFX_FLAGS_NO_RESET 	0x10000002
#define RFX_FLAGS_UPD_FROM_GUI 	0x10000004
#define RFX_FLAGS_UPD_FROM_VAL 	0x10000008

typedef struct {
  char *name;  ///< the name of the executable (so we can run it !)
  char *menu_text; ///< for Weed, this is the filter_class "name"
  char *action_desc; ///< for Weed "Applying $s"
  int min_frames; ///< for Weed, 1
  int num_in_channels;
  lives_rfx_status_t status;

  uint32_t props;

  LiVESWidget *menuitem;  ///< the menu item which activates this effect
  LiVESWidget *interface; // TODO
  int num_params;
  uint64_t flags; /// internal use

  lives_param_t *params;
  lives_rfx_source_t source_type;
  void *source;  ///< points to the source (e.g. a weed_plant_t)
  char delim[2];
  char rfx_version[64];
  LiVESList *gui_strings;  ///< rfxscript hints for constructing the param window
  LiVESList *onchange_strings;  ///< rfxscript for constructing onchange triggers
  boolean is_template;
  int needs_reinit;
} lives_rfx_t;

boolean check_rfx_for_lives(lives_rfx_t *);

void do_rfx_cleanup(lives_rfx_t *);

void render_fx_get_params(lives_rfx_t *, const char *plugin_name, short status);

void sort_rfx_array(lives_rfx_t **in_array, int num_elements);

int find_rfx_plugin_by_name(const char *name, short status);

void free_rfx_params(lives_param_t *params, int num_params);

void rfx_params_free(lives_rfx_t *);

void rfx_free(lives_rfx_t *); // calls rfx_params_free()

void rfx_free_all(void);

lives_rfx_t *rfx_init(int status, int src_type, void *src);

lives_param_t *rfx_init_params(lives_rfx_t *, int nparams);

lives_param_t *lives_param_string_init(lives_param_t *, const char *name, const char *def,
                                       int disp_len, int maxlen);

lives_param_t *lives_param_double_init(lives_param_t *, const char *name, double def,
                                       double min, double max);

lives_param_t *lives_param_integer_init(lives_param_t *, const char *name, int def,
                                        int min, int max);

lives_param_t *lives_param_boolean_init(lives_param_t *, const char *name, int def);

void param_copy(lives_param_t *dest, lives_param_t *src, boolean full);

lives_param_t *find_rfx_param_by_name(lives_rfx_t *, const char *name);

lives_param_t *rfx_param_from_name(lives_param_t *, int nparams, const char *name);

boolean set_rfx_value_by_name_string(lives_rfx_t *, const char *name,
                                     const char *value, boolean update_visual);

boolean get_rfx_value_by_name_string(lives_rfx_t *, const char *name, char **return_value);

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

char *get_string_param(void *value);
boolean get_bool_param(void *value);
int get_int_param(void *value);
double get_double_param(void *value);
float get_float_param(void *value);
void get_colRGB24_param(void *value, lives_colRGB48_t *rgb);
void get_colRGBA32_param(void *value, lives_colRGBA64_t *rgba);

void set_string_param(void **value_ptr, const char *_const, size_t maxlen);
void set_bool_param(void *value, boolean);
void set_int_param(void *value, int);
void set_double_param(void *value, double);
void set_float_param(void *value, float);
void set_colRGB24_param(void *value, short red, short green, short blue);
void set_colRGBA32_param(void *value, short red, short green, short blue, short alpha);

void set_float_array_param(void **value_ptr, float *values, int nvals);

/// return an array of parameter values
void **store_rfx_params(lives_rfx_t *);
void set_rfx_params_from_store(lives_rfx_t *, void **store);
void rfx_params_store_free(lives_rfx_t *, void **store);

lives_rfx_t *weed_to_rfx(weed_plant_t *filter_or_inst, boolean show_reinits);
lives_param_t *weed_params_to_rfx(int npar, weed_plant_t *instance, boolean show_reinits);

void rfx_clean_exe(lives_rfx_t *rfx);

LiVESWidget *rfx_make_param_dialog(lives_rfx_t *rfx, const char *title, boolean add_cancel);

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
  LiVESWidget *overlay_combo;
  lives_rfx_t *rfx;
  boolean keep_rfx;
  lives_intention intention;
} _vppaw;

_vppaw *on_vpp_advanced_clicked(LiVESButton *, livespointer);

void on_decplug_advanced_clicked(LiVESButton *, livespointer user_data);

LiVESResponseType on_vppa_ok_clicked(boolean direct, _vppaw *);

LiVESList *get_external_window_hints(lives_rfx_t *);
boolean check_encoder_restrictions(boolean get_extension, boolean user_audio, boolean save_all);

/// for realtime effects, see effects-weed.h
/// CAPACITIES - flavours

// ^TODO -> object properties
#define LIVES_CAPACITY_AUDIO_RATE "audio_rate"			// int value
#define LIVES_CAPACITY_AUDIO_CHANS "audio_channels"		// int value

/// CAPACITIES - varieties

// vpp
//"general"
#define VPP_CAN_RESIZE				(1 << 0)
#define VPP_CAN_RETURN				(1 << 1)
#define VPP_LOCAL_DISPLAY			(1 << 2)
#define VPP_LINEAR_GAMMA			(1 << 3)
#define VPP_CAN_RESIZE_WINDOW          		(1 << 4)   /// can resize the image to fit the play window
#define VPP_CAN_LETTERBOX                  	(1 << 5)
#define VPP_CAN_CHANGE_PALETTE			(1 << 6)

// encoder
//"general"
#define HAS_RFX (1 << 0)
#define CAN_ENCODE_PNG (1 << 2)
#define ENCODER_NON_NATIVE (1 << 3)

//"acodecs"
#define AUDIO_CODEC_NONE	0

#define AUDIO_CODEC_MP3		1
#define AUDIO_CODEC_PCM		2
#define AUDIO_CODEC_MP2		3
#define AUDIO_CODEC_VORBIS	4
#define AUDIO_CODEC_AC3		5
#define AUDIO_CODEC_AAC		6
#define AUDIO_CODEC_AMR_NB	7
#define AUDIO_CODEC_RAW		8
#define AUDIO_CODEC_WMA2	9
#define AUDIO_CODEC_OPUS	10

#define AUDIO_CODEC_MAX		63
#define AUDIO_CODEC_UNKNOWN	-1

// decoders

// "sync_hint"
#define SYNC_HINT_AUDIO_TRIM_START	(1 << 0)
#define SYNC_HINT_AUDIO_PAD_START	(1 << 1)
#define SYNC_HINT_AUDIO_TRIM_END 	(1 << 2)
#define SYNC_HINT_AUDIO_PAD_END 	(1 << 3)

#define SYNC_HINT_VIDEO_PAD_START 	(1 << 4)
#define SYNC_HINT_VIDEO_PAD_END		(1 << 5)

//"seek_flag"
/// good
#define LIVES_SEEK_FAST (1 << 0)
#define LIVES_SEEK_FAST_REV (1 << 1)

/// not so good
#define LIVES_SEEK_NEEDS_CALCULATION (1 << 2)
#define LIVES_SEEK_QUALITY_LOSS (1 << 3)

// rendered effects
// "general"
#define RFX_PROPS_SLOW        0x0001  ///< hint to GUI
#define RFX_PROPS_MAY_RESIZE  0x0002 ///< is a tool (can only be applied to entire clip)
#define RFX_PROPS_BATCHG      0x0004 ///< is a batch generator
#define RFX_PROPS_NO_PREVIEWS 0x0008 ///< no previews possible (e.g. effect has long prep. time)

#define RFX_PROPS_RESERVED1   0x1000
#define RFX_PROPS_RESERVED2   0x2000
#define RFX_PROPS_RESERVED3   0x4000
#define RFX_PROPS_AUTO_BUILT  0x8000

// defined in plugins.c for the whole app
extern const char *const anames[AUDIO_CODEC_MAX];

lives_rfx_t *obj_attrs_to_rfx(lives_object_t *, boolean readonly);

#define LIVES_LEAF_RPAR "host_rpar"

#endif
