// effects-weed.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2021 <salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_EFFECTS_WEED_H
#define HAS_LIVES_EFFECTS_WEED_H

#ifndef WEED_EFFECT_HAS_PARAM_FLAGBITS
#define WEED_PARAM_FLAG_READ_ONLY     		(1 << 0)
#define WEED_PARAM_FLAG_VALUE_CHANGED           (1 << 1)
#endif

#define MAX_WEED_STRLEN 65535 // soft limit for LiVES
#define MAX_WEED_ELEMENTS 65535 // soft limit for LiVES

#define MAX_EASE_SECS 5. // sanity check for plugin easing feature

// signature for serialisation / deserialisation
#define WEED_LAYER_MARKER 0x44454557;

/// filter apply errors
typedef enum {
  FILTER_SUCCESS = 0,
  FILTER_ERROR_MISSING_LAYER,
  FILTER_ERROR_BLANK_FRAME,
  FILTER_ERROR_MISSING_FRAME,
  FILTER_ERROR_INVALID_PALETTE_CONVERSION,
  FILTER_ERROR_UNABLE_TO_RESIZE,
  FILTER_ERROR_COULD_NOT_REINIT,
  FILTER_ERROR_INVALID_PLUGIN,
  FILTER_ERROR_NEEDS_REINIT,   // TODO
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
  FILTER_ERROR_COPYING_FAILED,
  FILTER_ERROR_BUSY,

  /// values >= 512 are info
  FILTER_INFO_REINITED = 512,
  FILTER_INFO_REDRAWN
} lives_filter_error_t;

#define FILTER_ERROR_IS_INFO(err) (err >= 512)

typedef enum {
  FX_LIST_NAME, // just name
  FX_LIST_EXTENDED_NAME, // name + author (if dupe) + subcat + observations
  FX_LIST_HASHNAME, // hashnames - (packagefilterauthor) author and not extra_authors
} lives_fx_list_t;

// set some custom Weed values

#define WEED_FLAG_HOST_READONLY (1 << 16)
#define WEED_FLAG_FREE_ON_DELETE (1 << 17)

boolean weed_leaf_autofree(weed_plant_t *plant, const char *key);

weed_error_t weed_leaf_set_autofree(weed_plant_t *, const char *key, boolean state);
void  weed_plant_autofree(weed_plant_t *);

// for copy and cleanup
boolean pixdata_nullify_leaf(const char *key);
// for sterilzing "dormant" plants
boolean no_copy_leaf(const char *key);
void weed_plant_sanitize(weed_plant_t *plant, boolean sterilize);

// plugin specific values
#define WEED_LEAF_HOST_SUSPICIOUS "host_suspicious" // plugin is badly behaved

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
#define WEED_LEAF_HOST_TEMP_DISABLED "host_temp_dis" // channel is temp disabled
#define WEED_LEAF_HOST_REPEATS "host_repeats" // host channel repeats
#define WEED_LEAF_HOST_INITED "host_inited" // inited or not
#define WEED_LEAF_HOST_PLUGIN_PATH "host_plugin_path" // plugin path
#define WEED_LEAF_HOST_HANDLE "host_handle" // dll handle
#define WEED_LEAF_HOST_FILTER_LIST "host_filter_list" // host usable filters
#define WEED_LEAF_HOST_NORECORD "host_norecord" // do not record parameter changes for this instance
#define WEED_LEAF_DUPLICATE "host_duplicate"
#define WEED_LEAF_VALUE_BACK "host_value_back"
#define WEED_LEAF_HOST_REINITING "host_reiniting"
#define WEED_LEAF_HOST_CHKSTRG "host_chkstrchoices"

#define WEED_LEAF_HOST_INSTANCE "host_instance" // special value for text widgets
#define WEED_LEAF_HOST_IDX "host_idx" // special value for text widgets

#define WEED_LEAF_FREED_PLANTS "host_freed_plants" // list of freed pointers to avoid freeing dupes during unload

#define WEED_LEAF_HOST_SCRAP_FILE_OFFSET "scrap_offset" // special value for scrap_file

#define WEED_LEAF_HOST_IDENTIFIER "host_unique_id"

#define LIVES_LEAF_AUTO_EASING "host_auto_ease"
#define LIVES_LEAF_EASING_EVENTS "host_eevents"

#define WEED_LEAF_RFX_STRINGS "layout_rfx_strings"
#define WEED_LEAF_RFX_DELIM "layout_rfx_delim"

#define WEED_LEAF_HOST_PLUGIN_NAME "host_plugin_name"

#define LIVES_LEAF_IGNORE_STATE_UPDATES "host_ign_state_upd"

#define LIVES_LEAF_SOFT_DEINIT "host_soft_deinit"

#define LIVES_LEAF_NOQUANT "host_noquant"

// compound plugins
#define WEED_LEAF_HOST_INTERNAL_CONNECTION "host_internal_connection" // for chain plugins
#define WEED_LEAF_HOST_INTERNAL_CONNECTION_AUTOSCALE "host_internal_connection_autoscale" // for chain plugins
#define WEED_LEAF_HOST_NEXT_INSTANCE "host_next_instance" // for chain plugins
#define WEED_LEAF_HOST_COMPOUND_CLASS "host_compound_class" // for chain plugins
#define WEED_LEAF_HOST_CHANNEL_CONNECTION "host_channel_connection" // special value for text widgets

// custom leaf flags, may evolve
#define LIVES_FLAG_MAINTAIN_VALUE (1 << 16) ///< soft flag, like immutable / deletable for host

weed_plant_t *get_weed_filter(int filter_idx); // TODO: make const
char *weed_filter_idx_get_package_name(int filter_idx) WARN_UNUSED;
char *weed_filter_idx_get_name(int idx, boolean add_subcats, boolean add_notes) WARN_UNUSED;
char *weed_instance_get_filter_name(weed_plant_t *inst, boolean get_compound_parent) WARN_UNUSED;
char *make_weed_hashname(int filter_idx, boolean fullname,
                         boolean use_extra_authors, char sep, boolean spc_to_underscore) WARN_UNUSED;  ///< fullname includes author and version
int weed_get_idx_for_hashname(const char *hashname, boolean fullname) LIVES_CONST;  ///< fullname includes author and version
int *weed_get_indices_from_template(const char *package_name, const char *filter_name, const char *author, int version);
int weed_filter_highest_version(const char *pkg, const char *fxname, const char *auth, int *return_version);
int enabled_in_channels(weed_plant_t *plant, boolean count_repeats);
int enabled_out_channels(weed_plant_t *plant, boolean count_repeats);
weed_plant_t *get_enabled_channel(weed_plant_t *inst, int which, boolean is_in);  ///< for FILTER_INST
weed_plant_t *get_enabled_audio_channel(weed_plant_t *inst, int which, boolean is_in);  ///< for FILTER_INST
weed_plant_t *get_mandatory_channel(weed_plant_t *filter, int which, boolean is_in);  ///< for FILTER_CLASS
boolean weed_instance_is_resizer(weed_plant_t *filt);
weed_plant_t *weed_instance_get_filter(weed_plant_t *inst, boolean get_compound_parent);

#define COMPOUND_LITERAL "compound"

#define PLUGIN_COMPOUND_EFFECTS_BUILTIN EFFECTS_LITERAL LIVES_DIR_SEP COMPOUND_LITERAL
#define PLUGIN_COMPOUND_EFFECTS_CUSTOM PLUGINS_LITERAL LIVES_DIR_SEP EFFECTS_LITERAL LIVES_DIR_SEP COMPOUND_LITERAL

weed_plant_t *get_next_compound_inst(weed_plant_t *inst);

int num_compound_fx(weed_plant_t
                    *plant); ///< return number of filters in a compound fx (1 if it is not compound) - works for filter or inst

boolean has_non_alpha_palette(weed_plant_t *ctmpl, weed_plant_t *filter);
boolean has_alpha_palette(weed_plant_t *ctmpl, weed_plant_t *filter);

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

boolean has_usable_palette(weed_plant_t *filter, weed_plant_t *chantmpl);
int best_palette_match(int *palete_list, int num_palettes, int palette);
int check_filter_chain_palettes(boolean is_bg, int *palette_list, int npals);

// instances
weed_error_t weed_call_init_func(weed_plant_t *instance);
weed_error_t weed_call_deinit_func(weed_plant_t *instance);
lives_filter_error_t run_process_func(weed_plant_t *instance, weed_timecode_t tc);

char *cd_to_plugin_dir(weed_plant_t *filter);
boolean weed_init_effect(int hotkey); ///< hotkey starts at 1
boolean  weed_deinit_effect(int hotkey); ///< hotkey starts at 1
weed_plant_t *weed_instance_from_filter(weed_plant_t *filter);
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

void **get_easing_events(int *nev);

/// parameter interpolation
boolean interpolate_param(weed_plant_t *param, void *pchain, ticks_t tc);
boolean interpolate_params(weed_plant_t *inst, void **pchains, ticks_t tc);

int filter_mutex_lock(int key);  // 0 based key
int filter_mutex_trylock(int key);  // 0 based key
int filter_mutex_unlock(int key); // 0 based key


size_t weed_plant_serialise(int fd, weed_plant_t *plant, unsigned char **mem);
weed_plant_t *weed_plant_deserialise(int fd, unsigned char **mem, weed_plant_t *plant);

/// record a parameter value change in our event_list
void rec_param_change(weed_plant_t *inst, int pnum);

// copy values for "copy_value_to" params
int set_copy_to(weed_plant_t *inst, int pnum, lives_rfx_t *rfx, boolean update);

weed_plant_t *get_textparm();

void weed_set_blend_factor(int hotkey);  // 0 based key
int weed_get_blend_factor(int hotkey); // 0 based key

void weed_functions_init(void); ///< call weed_init() to set our weed core functions

void weed_load_all(void);  ///< load effects
void weed_unload_all(void); ///< unload all effects
int get_next_free_key(void); ///< next free "key" for the multitrack system

void weed_deinit_all(boolean shutdown); ///< deinit all active effects

weed_plant_t *weed_apply_effects(weed_plant_t **layers, weed_plant_t *filter_map, ticks_t tc, int opwidth, int opheight,
                                 void ***pchains);
lives_filter_error_t weed_apply_instance(weed_plant_t *inst, weed_plant_t *init_event, weed_plant_t **layers,
    int opwidth, int opheight, ticks_t tc);
void weed_apply_audio_effects(weed_plant_t *filter_map, weed_layer_t **, int nbtracks, int nchans, int64_t nsamps, double arate,
                              ticks_t tc, double *vis);
void weed_apply_audio_effects_rt(weed_layer_t *alayer, ticks_t tc, boolean analysers_only, boolean is_audio_thread);

boolean fill_audio_channel(weed_plant_t *filter, weed_plant_t *achan, boolean is_vid);
int register_audio_client(boolean is_vid);
int unregister_audio_client(boolean is_vid);

int register_aux_audio_channels(int nchannels);
int unregister_aux_audio_channels(int nchannels);
boolean fill_audio_channel_aux(weed_plant_t *achan);

lives_filter_error_t weed_apply_audio_instance(weed_plant_t *init_event, weed_layer_t **layers, int nbtracks, int nchans,
    int64_t nsamps,
    double arate, ticks_t tc, double *vis);

int weed_generator_start(weed_plant_t *inst, int key);  // 0 based key
void weed_generator_end(weed_plant_t *inst);
boolean weed_playback_gen_start(void);
void weed_bg_generator_end(weed_plant_t *inst);
void wge_inner(weed_plant_t *inst); ///< deinit and instance(s) for generator, reset instance mapping

// layers
weed_error_t weed_leaf_copy_or_delete(weed_layer_t *dlayer, const char *key, weed_layer_t *slayer);
weed_plant_t *weed_layer_create_from_generator(weed_plant_t *inst, ticks_t tc, int clipno);

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
char *rte_keymode_get_filter_name(int key, int mode,
                                  boolean add_notes) WARN_UNUSED;  ///< returns name of filter_class bound to key/mode (or "")
char *rte_keymode_get_plugin_name(int key,
                                  int mode) WARN_UNUSED; ///< returns name of plugin package containing filter_class (or "")
char *rte_keymode_get_type(int key, int mode) WARN_UNUSED;  ///< returns a string filter/instance type (or "")

#ifdef HAS_LIVES_EFFECTS_H
lives_fx_cat_t rte_keymode_get_category(int key, int mode);
#endif

weed_plant_t *rte_keymode_get_instance(int key, int mode); ///< returns refcounted filter_instance bound to key/mode (or NULL)
weed_plant_t *rte_keymode_get_filter(int key, int mode); ///< returns filter_class bound to key/mode (or NULL)

boolean weed_delete_effectkey(int key, int mode);  ///< unbinds a filter_class from a key/mode
int weed_add_effectkey(int key, const char *hashname,
                       boolean fullname);  ///< bind a filter_class to key/mode using its hashname

int weed_add_effectkey_by_idx(int key, int idx);  ///< see description

int rte_key_getmode(int key);  ///< returns current active mode for a key (or -1)
int rte_key_getmaxmode(int key); ///< returns highest mode which is set

int rte_key_num_modes(int key); ///< max, set or not

weed_plant_t *get_new_inst_for_keymode(int key, int mode); ///< get new refcounted inst (during recording playback)

boolean rte_key_setmode(int key, int newmode);  ///< set mode for a given key; if key==0 then the active key is used

///< returns -1 if the filter is not found; it will match the first name found - returns -2 if you try to switch a generator/non-generator
int rte_switch_keymode(int key, int mode, const char *hashname);

boolean set_autotrans(int clip);
void set_trans_amt(int key, int mode, double *amt);

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

int rte_bg_gen_key(void) LIVES_PURE;

int rte_fg_gen_key(void) LIVES_PURE;

int rte_bg_gen_mode(void) LIVES_PURE;
int rte_fg_gen_mode(void) LIVES_PURE;

////////////////////////////////////////////////////////////////////////

char *get_weed_display_string(weed_plant_t *inst, int pnum);

weed_plant_t *add_filter_deinit_events(weed_plant_t *event_list);
weed_plant_t *add_filter_init_events(weed_plant_t *event_list, ticks_t tc);

boolean record_filter_init(int key);
boolean record_filter_deinit(int key);

void deinit_render_effects(void);
void deinit_easing_effects(void);

boolean write_filter_defaults(int fd, int idx);
boolean read_filter_defaults(int fd);

boolean write_generator_sizes(int fd, int idx);
boolean read_generator_sizes(int fd);

//void set_param_gui_readwrite(weed_plant_t *inst);
//void set_param_gui_readonly(weed_plant_t *inst);

void update_all_host_info(void);

/// add default filler values to a parameter or pchange.
void fill_param_vals_to(weed_plant_t *param, weed_plant_t *ptmpl, int fill_slot);

//#define DEBUG_FILTER_MUTEXES
#ifdef DEBUG_FILTER_MUTEXES
#define filter_mutex_lock(key) {g_print ("lock %d at line %d in file %s\n",key,__LINE__,__FILE__); \
  if (key >= 0 && key < FX_KEYS_MAX) {if (pthread_mutex_trylock(&mainw->fx_mutex[key])) { \
      g_print("BLOCKED\n"); pthread_mutex_lock(&mainw->fx_mutex[key]);} \
    g_print("done\n");}}

#define filter_mutex_unlock(key) {g_print ("unlock %d at line %d in file %s\n\n",key,__LINE__,__FILE__); \
    if (key >= 0 && key < FX_KEYS_MAX) pthread_mutex_unlock(&mainw->fx_mutex[key]); g_print("done\n");}
#endif

int check_ninstrefs(void);

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

weed_plant_t *host_info_cb(weed_plant_t *xhost_info, void *data);

weed_error_t weed_leaf_set_host(weed_plant_t *plant, const char *key, uint32_t seed_type, weed_size_t num_elems, void *value);
weed_error_t weed_leaf_delete_host(weed_plant_t *plant, const char *key);
weed_error_t weed_plant_free_host(weed_plant_t *plant);
weed_plant_t *weed_plant_new_host(int type);
//weed_error_t weed_leaf_get_monitor(weed_plant_t *plant, const char *key, int32_t idx, void *value);

void show_weed_stats(weed_plant_t *statsplant);

#endif
