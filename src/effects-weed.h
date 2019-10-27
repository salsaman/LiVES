// effects-weed.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2016 <salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_EFFECTS_WEED_H
#define HAS_LIVES_EFFECTS_WEED_H

/// filter apply errors
typedef enum {
  FILTER_NO_ERROR = 0,
  FILTER_ERROR_MISSING_LAYER,
  FILTER_ERROR_BLANK_FRAME,
  FILTER_ERROR_MISSING_FRAME,
  FILTER_ERROR_INVALID_PALETTE_CONVERSION,
  FILTER_ERROR_UNABLE_TO_RESIZE,
  FILTER_ERROR_INVALID_PALETTE_SETTINGS,
  FILTER_ERROR_COULD_NOT_REINIT,
  FILTER_ERROR_MUST_RELOAD,
  FILTER_ERROR_NO_IN_CHANNELS,
  FILTER_ERROR_INVALID_TRACK,
  FILTER_ERROR_INTERPOLATION_FAILED,
  FILTER_ERROR_INVALID_INSTANCE,
  FILTER_ERROR_INVALID_FILTER,
  FILTER_ERROR_INVALID_INIT_EVENT,
  FILTER_ERROR_IS_AUDIO,
  FILTER_ERROR_IS_SCRAP_FILE,
  FILTER_ERROR_MISSING_CHANNEL,
  FILTER_ERROR_TEMPLATE_MISMATCH,
  FILTER_ERROR_MEMORY_ERROR,
  FILTER_ERROR_DONT_THREAD,

  /// values >= 512 are info
  FILTER_INFO_REINITED = 512
} lives_filter_error_t;

typedef enum {
  FX_LIST_NAME, // just name
  FX_LIST_EXTENDED_NAME, // name + author (if dupe) + subcat + observations
  FX_LIST_HASHNAME, // hashnames - (packagefilterauthor) author and not extra_authors
} lives_fx_list_t;

#ifndef WEED_PLANT_LAYER
#define WEED_PLANT_LAYER WEED_PLANT_CHANNEL
#endif

#define WEED_LEAF_TYPE "type"
#define WEED_LEAF_PLUGIN_INFO "plugin_info"
#define WEED_LEAF_FILTERS "filters"
#define WEED_LEAF_MAINTAINER "maintainer"
#define WEED_LEAF_HOST_INFO "host_info"
#define WEED_LEAF_HOST_PLUGIN_NAME "host_plugin_name"
#define WEED_LEAF_PACKAGE_NAME "package_name"

// host info
#define WEED_LEAF_API_VERSION "api_version"
#define WEED_LEAF_GET_FUNC "weed_leaf_get_func"
#define WEED_LEAF_SET_FUNC "weed_leaf_set_func"
#define WEED_PLANT_NEW_FUNC "weed_plant_new_func"
#define WEED_PLANT_LIST_LEAVES_FUNC "weed_plant_list_leaves_func"
#define WEED_LEAF_NUM_ELEMENTS_FUNC "weed_leaf_num_elements_func"
#define WEED_LEAF_ELEMENT_SIZE_FUNC "weed_leaf_element_size_func"
#define WEED_LEAF_SEED_TYPE_FUNC "weed_leaf_seed_type_func"
#define WEED_LEAF_GET_FLAGS_FUNC "weed_leaf_get_flags_func"
#define WEED_LEAF_MALLOC_FUNC "weed_malloc_func"
#define WEED_LEAF_FREE_FUNC "weed_free_func"
#define WEED_LEAF_MEMSET_FUNC "weed_memset_func"
#define WEED_LEAF_MEMCPY_FUNC "weed_memcpy_func"

// filter_class
#define WEED_LEAF_INIT_FUNC "init_func"
#define WEED_LEAF_DEINIT_FUNC "deinit_func"
#define WEED_LEAF_PROCESS_FUNC "process_func"
#define WEED_LEAF_DISPLAY_FUNC "display_func"
#define WEED_LEAF_INTERPOLATE_FUNC "interpolate_func"
#define WEED_LEAF_TARGET_FPS "target_fps"
#define WEED_LEAF_GUI "gui"
#define WEED_LEAF_DESCRIPTION "description"
#define WEED_LEAF_AUTHOR "author"
#define WEED_LEAF_EXTRA_AUTHORS "extra_authors"
#define WEED_LEAF_URL "url"
#define WEED_LEAF_ICON "icon"
#define WEED_LEAF_LICENSE "license"
#define WEED_LEAF_COPYRIGHT "copyright"
#define WEED_LEAF_VERSION "version"

// instance
#define WEED_LEAF_FILTER_CLASS "filter_class"
#define WEED_LEAF_TIMECODE "timecode"
#define WEED_LEAF_FPS "fps"

// channels / chan template
#define WEED_LEAF_PIXEL_DATA "pixel_data"
#define WEED_LEAF_WIDTH "width"
#define WEED_LEAF_HEIGHT "height"
#define WEED_LEAF_PALETTE_LIST "palette_list"
#define WEED_LEAF_CURRENT_PALETTE "current_palette"
#define WEED_LEAF_ROWSTRIDES "rowstrides"
#define WEED_LEAF_YUV_SUBSPACE "YUV_subspace"
#define WEED_LEAF_YUV_SAMPLING "YUV_sampling"
#define WEED_LEAF_YUV_CLAMPING "YUV_clamping"
#define WEED_LEAF_IN_CHANNELS "in_channels"
#define WEED_LEAF_OUT_CHANNELS "out_channels"
#define WEED_LEAF_IN_CHANNEL_TEMPLATES "in_channel_templates"
#define WEED_LEAF_OUT_CHANNEL_TEMPLATES "out_channel_templates"
#define WEED_LEAF_OFFSET "offset"
#define WEED_LEAF_HSTEP "hstep"
#define WEED_LEAF_VSTEP "vstep"
#define WEED_LEAF_MAXWIDTH "maxwidth"
#define WEED_LEAF_MAXHEIGHT "maxheight"
#define WEED_LEAF_OPTIONAL "optional"
#define WEED_LEAF_DISABLED "disabled"
#define WEED_LEAF_ALIGNMENT "alignment"
#define WEED_LEAF_TEMPLATE "template"
#define WEED_LEAF_PIXEL_ASPECT_RATIO "pixel_aspect_ratio"
#define WEED_LEAF_ROWSTRIDE_ALIGNMENT_HINT "rowstride_alignment_hint"
#define WEED_LEAF_MAX_REPEATS "max_repeats"
#define WEED_LEAF_GAMMA_TYPE "gamma_type"

#define WEED_GAMMA_BT709 1024
#define WEED_GAMMA_MONITOR 1025

// params / param tmpl
#define WEED_LEAF_IN_PARAMETERS "in_parameters"
#define WEED_LEAF_OUT_PARAMETERS "out_parameters"
#define WEED_LEAF_VALUE "value"
#define WEED_LEAF_FLAGS "flags"
#define WEED_LEAF_HINT "hint"
#define WEED_LEAF_GROUP "group"
#define WEED_LEAF_NAME "name"
#define WEED_LEAF_DEFAULT "default"
#define WEED_LEAF_MIN "min"
#define WEED_LEAF_MAX "max"
#define WEED_LEAF_IGNORE "ignore"
#define WEED_LEAF_NEW_DEFAULT "new_default"
#define WEED_LEAF_COLORSPACE "colorspace"
#define WEED_LEAF_IN_PARAMETER_TEMPLATES "in_parameter_templates"
#define WEED_LEAF_OUT_PARAMETER_TEMPLATES "out_parameter_templates"
#define WEED_LEAF_TRANSITION "transition"
#define WEED_LEAF_IS_VOLUME_MASTER "is_volume_master"

// audio
#define WEED_LEAF_IS_AUDIO "is_audio"
#define WEED_LEAF_AUDIO_DATA "audio_data"
#define WEED_LEAF_AUDIO_DATA_LENGTH "audio_data_length"
#define WEED_LEAF_AUDIO_RATE "audio_rate"
#define WEED_LEAF_AUDIO_CHANNELS "audio_channels"
#define WEED_LEAF_AUDIO_INTERLEAF "audio_interleaf"

// param gui
#define WEED_LEAF_WRAP "wrap"
#define WEED_LEAF_MAXCHARS "maxchars"
#define WEED_LEAF_LABEL "label"
#define WEED_LEAF_DECIMALS "decimals"
#define WEED_LEAF_STEP_SIZE "step_size"
#define WEED_LEAF_CHOICES "choices"
#define WEED_LEAF_USE_MNEMONIC "use_mnemonic"
#define WEED_LEAF_HIDDEN "hidden"
#define WEED_LEAF_DISPLAY_VALUE "display_value"
#define WEED_LEAF_COPY_VALUE_TO "copy_value_to"

// plugin gui: layout
#define WEED_LEAF_LAYOUT_SCHEME "layout_scheme"
#define WEED_LEAF_RFX_STRINGS "rfx_strings"
#define WEED_LEAF_RFX_DELIM "rfx_delim"

// plugin specific values
#define WEED_LEAF_PLUGIN_UNSTABLE "plugin_unstable" // plugin hint to host

// internal values
#define WEED_LEAF_HOST_AUDIO_PLAYER "host_audio_player" // exported to plugins

#define WEED_LEAF_HOST_ORIG_PDATA "host_orig_pdata" // set if we "steal" an alpha channel to chain
#define WEED_LEAF_HOST_MENU_HIDE "host_menu_hide" // hide from menus
#define WEED_LEAF_HOST_DEFAULT "host_default" // user set default
#define WEED_LEAF_HOST_WIDTH "host_width" // user set width
#define WEED_LEAF_HOST_HEIGHT "host_height" // user set height
#define WEED_LEAF_HOST_FPS "host_fps" // user set fps
#define WEED_LEAF_HOST_TAG "host_tag" // internal key mapping (for higher keys)
#define WEED_LEAF_HOST_KEY "host_key" // internal key mapping
#define WEED_LEAF_HOST_MODE "host_mode" // internal mode mapping
#define WEED_LEAF_HOST_INPLACE "host_inplace" // inplace effect
#define WEED_LEAF_HOST_DISABLED "host_disabled" // channel is disabled
#define WEED_LEAF_HOST_TEMP_DISABLED "host_temp_disabled" // channel is temp disabled
#define WEED_LEAF_HOST_REFS "host_refs" // host ref counting
#define WEED_LEAF_HOST_REPEATS "host_repeats" // host channel repeats
#define WEED_LEAF_HOST_INITED "host_inited" // inited or not
#define WEED_LEAF_HOST_PLUGIN_PATH "host_plugin_path" // plugin path
#define WEED_LEAF_HOST_HANDLE "host_handle" // dll handle
#define WEED_LEAF_HOST_FILTER_LIST "host_filter_list" // host usable filters
#define WEED_LEAF_HOST_NORECORD "host_norecord" // do not record parameter changes for this instance

#define WEED_LEAF_HOST_INSTANCE "host_instance" // special value for text widgets
#define WEED_LEAF_HOST_IDX "host_idx" // special value for text widgets

#define WEED_LEAF_HOST_SCRAP_FILE_OFFSET "scrap_file_offset" // special value for scrap_file

// compound plugins
#define WEED_LEAF_HOST_INTERNAL_CONNECTION "host_internal_connection" // for chain plugins
#define WEED_LEAF_HOST_INTERNAL_CONNECTION_AUTOSCALE "host_internal_connection_autoscale" // for chain plugins
#define WEED_LEAF_HOST_NEXT_INSTANCE "host_next_instance" // for chain plugins
#define WEED_LEAF_HOST_COMPOUND_CLASS "host_compound_class" // for chain plugins
#define WEED_LEAF_HOST_CHANNEL_CONNECTION "host_channel_connection" // special value for text widgets

// layer only values

#define WEED_LEAF_CLIP "clip"
#define WEED_LEAF_FRAME "frame"

/// bootstrap function for weed plugins
weed_plant_t *weed_bootstrap_func(weed_default_getter_f *value, int num_versions, int *plugin_versions);

weed_plant_t *get_weed_filter(int filter_idx); // TODO: make const
char *weed_filter_idx_get_package_name(int filter_idx) WARN_UNUSED;
char *weed_get_package_name(weed_plant_t *filter_or_instance) WARN_UNUSED;
char *weed_filter_idx_get_name(int idx, boolean add_subcats, boolean mark_dupes, boolean add_notes) WARN_UNUSED;
char *weed_instance_get_filter_name(weed_plant_t *inst, boolean get_compound_parent) WARN_UNUSED;
char *make_weed_hashname(int filter_idx, boolean fullname,
                         boolean use_extra_authors, char sep) WARN_UNUSED;  ///< fullname includes author and version
int weed_get_idx_for_hashname(const char *hashname, boolean fullname) GNU_CONST;  ///< fullname includes author and version
int *weed_get_indices_from_template(const char *package_name, const char *filter_name, const char *author, int version);
int weed_filter_highest_version(const char *pkg, const char *fxname, const char *auth, int *return_version);
int enabled_in_channels(weed_plant_t *plant, boolean count_repeats);
int enabled_out_channels(weed_plant_t *plant, boolean count_repeats);
weed_plant_t *get_enabled_channel(weed_plant_t *inst, int which, boolean is_in);  ///< for FILTER_INST
weed_plant_t *get_enabled_audio_channel(weed_plant_t *inst, int which, boolean is_in);  ///< for FILTER_INST
weed_plant_t *get_mandatory_channel(weed_plant_t *filter, int which, boolean is_in);  ///< for FILTER_CLASS
boolean weed_filter_is_resizer(weed_plant_t *filt);
boolean weed_instance_is_resizer(weed_plant_t *filt);
weed_plant_t *weed_instance_get_filter(weed_plant_t *inst, boolean get_compound_parent);

#define PLUGIN_COMPOUND_EFFECTS_BUILTIN "effects/compound/"
#define PLUGIN_COMPOUND_EFFECTS_CUSTOM "plugins/effects/compound/"

int num_compound_fx(weed_plant_t
                    *plant); ///< return number of filters in a compound fx (1 if it is not compound) - works for filter or inst

boolean has_non_alpha_palette(weed_plant_t *ctmpl);
boolean has_alpha_palette(weed_plant_t *ctmpl);

boolean is_audio_channel_in(weed_plant_t *inst, int chnum);
boolean has_video_chans_in(weed_plant_t *filter, boolean count_opt);
boolean has_audio_chans_in(weed_plant_t *filter, boolean count_opt);
boolean is_audio_channel_out(weed_plant_t *inst, int chnum);
boolean has_video_chans_out(weed_plant_t *filter, boolean count_opt);
boolean has_audio_chans_out(weed_plant_t *filter, boolean count_opt);
boolean is_pure_audio(weed_plant_t *filter_or_instance, boolean count_opt); ///< TRUE if audio in or out and no vid in/out

boolean has_video_filters(boolean analysers_only);

#ifdef HAS_LIVES_EFFECTS_H
lives_fx_cat_t weed_filter_categorise(weed_plant_t *pl, int in_channels, int out_channels);
lives_fx_cat_t weed_filter_subcategorise(weed_plant_t *pl, lives_fx_cat_t category, boolean count_opt);
boolean has_audio_filters(lives_af_t af_type);
#endif

char *weed_seed_type_to_text(int seed_type);
char *weed_error_to_text(int error);

boolean has_usable_palette(weed_plant_t *chantmpl);
int check_weed_palette_list(int *palette_list, int num_palettes, int palette);

int weed_call_init_func(weed_plant_t *instance);
int weed_call_deinit_func(weed_plant_t *instance);

char *cd_to_plugin_dir(weed_plant_t *filter);
boolean weed_init_effect(int hotkey); ///< hotkey starts at 1
void weed_deinit_effect(int hotkey); ///< hotkey starts at 1
weed_plant_t *weed_instance_from_filter(weed_plant_t *filter);
int _wood_instance_ref(weed_plant_t *inst);
int _wood_instance_unref(weed_plant_t *inst);
weed_plant_t *_wood_instance_obtain(int line, char *file, int key, int mode);
void weed_in_parameters_free(weed_plant_t *inst);
void weed_in_params_free(weed_plant_t **parameters, int num_parameters);
void add_param_connections(weed_plant_t *inst);
lives_filter_error_t weed_reinit_effect(weed_plant_t *inst, boolean reinit_compound);
void weed_reinit_all(void);

int weed_flagset_array_count(weed_plant_t **array, boolean set_readonly);

int num_alpha_channels(weed_plant_t *filter, boolean out);

int num_in_params(weed_plant_t *, boolean skip_hidden, boolean skip_internal);
int num_out_params(weed_plant_t *);
weed_plant_t *weed_inst_in_param(weed_plant_t *inst, int param_num, boolean skip_hidden, boolean skip_internal);
weed_plant_t *weed_inst_out_param(weed_plant_t *inst, int param_num);
weed_plant_t *weed_filter_in_paramtmpl(weed_plant_t *filter, int param_num, boolean skip_internal);
weed_plant_t *weed_filter_out_paramtmpl(weed_plant_t *filter, int param_num);
boolean is_hidden_param(weed_plant_t *, int i);
int get_nth_simple_param(weed_plant_t *, int pnum);
int count_simple_params(weed_plant_t *);
weed_plant_t **weed_params_create(weed_plant_t *filter, boolean in);
int get_transition_param(weed_plant_t *filter, boolean skip_internal);
int get_master_vol_param(weed_plant_t *filter, boolean skip_internal);
boolean is_perchannel_multiw(weed_plant_t *param);
boolean has_perchannel_multiw(weed_plant_t *filter);
boolean weed_parameter_has_variable_elements_strict(weed_plant_t *inst, weed_plant_t *ptmpl);

/// parameter interpolation
boolean interpolate_param(weed_plant_t *inst, int i, void *pchain, weed_timecode_t tc);
boolean interpolate_params(weed_plant_t *inst, void **pchains, weed_timecode_t tc);

int filter_mutex_lock(int key);  // 0 based key
int filter_mutex_trylock(int key);  // 0 based key
int filter_mutex_unlock(int key); // 0 based key

size_t weed_plant_serialise(int fd, weed_plant_t *plant, unsigned char **mem);
weed_plant_t *weed_plant_deserialise(int fd, unsigned char **mem, weed_plant_t *plant);

/// record a parameter value change in our event_list
void rec_param_change(weed_plant_t *inst, int pnum);

// copy values for "copy_value_to" params
int set_copy_to(weed_plant_t *inst, int pnum, boolean update);

weed_plant_t *get_textparm();

void weed_set_blend_factor(int hotkey);  // 0 based key
int weed_get_blend_factor(int hotkey); // 0 based key

void weed_memory_init(void); ///< call weed_init() with mem functions

void weed_load_all(void);  ///< load effects
void weed_unload_all(void); ///< unload all effects
int get_next_free_key(void); ///< next free "key" for the multitrack system

void weed_deinit_all(boolean shutdown); ///< deinit all active effects

weed_plant_t *weed_apply_effects(weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc, int opwidth, int opheight,
                                 void ***pchains);
lives_filter_error_t weed_apply_instance(weed_plant_t *inst, weed_plant_t *init_event, weed_plant_t **layers,
    int opwidth, int opheight, weed_timecode_t tc);
void weed_apply_audio_effects(weed_plant_t *filter_map, float **abuf, int nbtracks, int nchans, int64_t nsamps, double arate,
                              weed_timecode_t tc, double *vis);
void weed_apply_audio_effects_rt(float **abuf, int nchans, int64_t nsamps, double arate, weed_timecode_t tc, boolean analysers_only);

lives_filter_error_t weed_apply_audio_instance(weed_plant_t *init_event, float **abuf, int nbtracks, int nchans, int64_t nsamps,
    double arate, weed_timecode_t tc, double *vis);

boolean weed_generator_start(weed_plant_t *inst, int key);  // 0 based key
void weed_generator_end(weed_plant_t *inst);
boolean weed_playback_gen_start(void);
void weed_bg_generator_end(weed_plant_t *inst);
void wge_inner(weed_plant_t *inst); ///< deinit and instance(s) for generator, reset instance mapping

// layers
weed_plant_t *weed_layer_create_from_generator(weed_plant_t *inst, weed_timecode_t tc);
weed_plant_t *weed_layer_new();
weed_plant_t *weed_layer_new_for_frame();
void **weed_layer_get_pixel_data(weed_plant_t *layer);
int *weed_layer_get_rowstrides(weed_plant_t *layer);
int weed_layer_get_width(weed_plant_t *layer);
int weed_layer_get_height(weed_plant_t *layer);
int weed_layer_current_palette(weed_plant_t *layer);

/// for multitrack
void backup_weed_instances(void);
void restore_weed_instances(void);

//////////////////////////////////////////////////////////
// WARNING !! "key" here starts at 1, "mode" starts at 0

boolean rte_key_valid(int key, boolean is_userkey);  ///< returns TRUE if there is a filter bound to active mode of hotkey
boolean rte_keymode_valid(int key, int mode,
                          boolean is_userkey);  ///< returns TRUE if a filter_class is bound to key/mode, is_userkey should be
///< set to TRUE
int rte_keymode_get_filter_idx(int key, int mode); ///< returns filter_class index of key/mode (or -1 if no filter bound)
char *rte_keymode_get_filter_name(int key, int mode, boolean mark_dupes,
                                  boolean add_notes) WARN_UNUSED;  ///< returns name of filter_class bound to key/mode (or "")
char *rte_keymode_get_plugin_name(int key, int mode) WARN_UNUSED; ///< returns name of plugin package containing filter_class (or "")
char *rte_keymode_get_type(int key, int mode) WARN_UNUSED;  ///< returns a string filter/instance type (or "")

#ifdef HAS_LIVES_EFFECTS_H
lives_fx_cat_t rte_keymode_get_category(int key, int mode);
#endif

weed_plant_t *rte_keymode_get_instance(int key, int mode); ///< returns refcounted filter_instance bound to key/mode (or NULL)
weed_plant_t *rte_keymode_get_filter(int key, int mode); ///< returns filter_class bound to key/mode (or NULL)

boolean weed_delete_effectkey(int key, int mode);  ///< unbinds a filter_class from a key/mode
int weed_add_effectkey(int key, const char *hashname, boolean fullname);  ///< bind a filter_class to key/mode using its hashname

int weed_add_effectkey_by_idx(int key, int idx);  ///< see description

int rte_key_getmode(int key);  ///< returns current active mode for a key (or -1)
int rte_key_getmaxmode(int key); ///< returns highest mode which is set

weed_plant_t *get_new_inst_for_keymode(int key, int mode); ///< get new refcounted inst (during recording playback)

boolean rte_key_setmode(int key, int newmode);  ///< set mode for a given key; if key==0 then the active key is used

///< returns -1 if the filter is not found; it will match the first name found - returns -2 if you try to switch a generator/non-generator
int rte_switch_keymode(int key, int mode, const char *hashname);

/////////////////////////////////////////////////////////////

LiVESList *weed_get_all_names(lives_fx_list_t list_type);
int rte_get_numfilters(void);
int weed_get_sorted_filter(int i);

/////////////////////////////////////////////////////////
// key starts at 0

void free_key_defaults(int key, int mode);
void apply_key_defaults(weed_plant_t *inst, int key, int mode);
void write_key_defaults(int fd, int key, int mode);
boolean read_key_defaults(int fd, int nparams, int key, int mode, int version);
void set_key_defaults(weed_plant_t *inst, int key, int mode);
boolean has_key_defaults(void);

//////////////////////////////////////////////////////
// 0 based keys
void rte_swap_fg_bg(void);

int rte_bg_gen_key(void) GNU_PURE;

int rte_fg_gen_key(void) GNU_PURE;

int rte_bg_gen_mode(void) GNU_PURE;
int rte_fg_gen_mode(void) GNU_PURE;

////////////////////////////////////////////////////////////////////////

char *get_weed_display_string(weed_plant_t *inst, int pnum);
weed_plant_t *add_filter_deinit_events(weed_plant_t *event_list);
weed_plant_t *add_filter_init_events(weed_plant_t *event_list, weed_timecode_t tc);
void deinit_render_effects(void);

boolean write_filter_defaults(int fd, int idx);
boolean read_filter_defaults(int fd);

boolean write_generator_sizes(int fd, int idx);
boolean read_generator_sizes(int fd);

int step_val(int val, int step);

void set_param_gui_readwrite(weed_plant_t *inst);
void set_param_gui_readonly(weed_plant_t *inst);

void weed_add_plant_flags(weed_plant_t *plant, int flags);

void update_host_info(weed_plant_t *inst);

/// add default filler values to a parameter or pchange.
void fill_param_vals_to(weed_plant_t *param, weed_plant_t *ptmpl, int fill_slot);

// some general utilities

#define WEED_PLANT_IS_PLUGIN_INFO(plant) (weed_get_plant_type(plant) == WEED_PLANT_PLUGIN_INFO ? 1 : 0)
#define WEED_PLANT_IS_HOST_INFO(plant) (weed_get_plant_type(plant) == WEED_PLANT_HOST_INFO ? 1 : 0)
#define WEED_PLANT_IS_FILTER_CLASS(plant) (weed_get_plant_type(plant) == WEED_PLANT_FILTER_CLASS ? 1 : 0)
#define WEED_PLANT_IS_FILTER_INSTANCE(plant) (weed_get_plant_type(plant) == WEED_PLANT_FILTER_INSTANCE ? 1 : 0)
#define WEED_PLANT_IS_CHANNEL(plant) (weed_get_plant_type(plant) == WEED_PLANT_CHANNEL ? 1 : 0)
#define WEED_PLANT_IS_CHANNEL_TEMPLATE(plant) (weed_get_plant_type(plant) == WEED_PLANT_CHANNEL_TEMPLATE ? 1 : 0)
#define WEED_PLANT_IS_PARAMETER(plant) (weed_get_plant_type(plant) == WEED_PLANT_PARAMETER ? 1 : 0)
#define WEED_PLANT_IS_PARAMETER_TEMPLATE(plant) (weed_get_plant_type(plant) == WEED_PLANT_PARAMETER_TEMPLATE ? 1 : 0)
#define WEED_PLANT_IS_GUI(plant) (weed_get_plant_type(plant) == WEED_PLANT_GUI ? 1 : 0)

int weed_general_error;

//#define DEBUG_FILTER_MUTEXES
#ifdef DEBUG_FILTER_MUTEXES
#define filter_mutex_lock(key) {g_print ("lock %d at line %d in file %s\n",key,__LINE__,__FILE__); if (key >= 0 && key < FX_KEYS_MAX) pthread_mutex_lock(&mainw->fx_mutex[key]); g_print("done\n");}
#define filter_mutex_unlock(key) {g_print ("unlock %d at line %d in file %s\n\n",key,__LINE__,__FILE__); if (key >= 0 && key < FX_KEYS_MAX) pthread_mutex_unlock(&mainw->fx_mutex[key]); g_print("done\n");}
#endif

//#define DEBUG_REFCOUNT
#ifdef DEBUG_REFCOUNT
#define weed_instance_ref(a) {g_print ("ref %p at line %d in file %s\n",a,__LINE__,__FILE__); _weed_instance_ref(a);}
#define weed_instance_unref(a) {g_print ("unref %p at line %d in file %s\n",a,__LINE__,__FILE__); _weed_instance_unref(a);}
#define weed_instance_obtain(a,b) _weed_instance_obtain(__LINE__, __FILE__, a, b)
#endif

int _weed_instance_ref(weed_plant_t *inst);
int _weed_instance_unref(weed_plant_t *inst);
weed_plant_t *_weed_instance_obtain(int line, char *file, int key, int mode);

#ifndef DEBUG_REFCOUNT
int weed_instance_ref(weed_plant_t *inst);
int weed_instance_unref(weed_plant_t *inst);
weed_plant_t *weed_instance_obtain(int key, int mode);
#endif

#endif
