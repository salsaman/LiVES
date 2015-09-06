// effects-weed.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2014 <salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#ifndef HAS_LIVES_EFFECTS_WEED_H
#define HAS_LIVES_EFFECTS_WEED_H

/// filter apply errors
typedef enum {
  FILTER_NO_ERROR=0,
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
  FILTER_INFO_REINITED=512
} lives_filter_error_t;


typedef enum {
  FX_LIST_NAME,
  FX_LIST_NAME_AND_TYPE,
  FX_LIST_HASHNAME,
} lives_fx_list_t;


/// bootstrap function for weed plugins
weed_plant_t *weed_bootstrap_func(weed_default_getter_f *value, int num_versions, int *plugin_versions);

weed_plant_t *get_weed_filter(int filter_idx);
char *weed_filter_idx_get_name(int filter_idx) WARN_UNUSED;
char *weed_instance_get_filter_name(weed_plant_t *inst, boolean get_compound_parent) WARN_UNUSED;
char *make_weed_hashname(int filter_idx, boolean fullname,
                         boolean use_extra_authors) WARN_UNUSED;  ///< fullname includes author and version
int weed_get_idx_for_hashname(const char *hashname, boolean fullname);  ///< fullname includes author and version
int *weed_get_indices_from_template(const char *package_name, const char *filter_name, const char *author, int version);
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
void load_compound_fx(void);

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

boolean has_usable_palette(weed_plant_t *chantmpl);
int check_weed_palette_list(int *palette_list, int num_palettes, int palette);

int weed_call_init_func(weed_plant_t *instance);
int weed_call_deinit_func(weed_plant_t *instance);

char *cd_to_plugin_dir(weed_plant_t *filter);
boolean weed_init_effect(int hotkey); ///< hotkey starts at 1
void weed_deinit_effect(int hotkey); ///< hotkey starts at 1
weed_plant_t *weed_instance_from_filter(weed_plant_t *filter);
void weed_instance_ref(weed_plant_t *inst);
void weed_instance_unref(weed_plant_t *inst);
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

void filter_mutex_lock(int key);  // 0 based key
void filter_mutex_unlock(int key); // 0 based key

boolean weed_plant_serialise(int fd, weed_plant_t *plant, unsigned char **mem);
weed_plant_t *weed_plant_deserialise(int fd, unsigned char **mem);


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
weed_plant_t *weed_layer_new_from_generator(weed_plant_t *inst, weed_timecode_t tc);
void weed_generator_end(weed_plant_t *inst);
boolean weed_playback_gen_start(void);
void weed_bg_generator_end(weed_plant_t *inst);
void wge_inner(weed_plant_t *inst, boolean unref); ///< deinit instance(s) for generator

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
char *rte_keymode_get_filter_name(int key, int mode) WARN_UNUSED;  ///< returns name of filter_class bound to key/mode (or "")
char *rte_keymode_get_plugin_name(int key, int mode) WARN_UNUSED; ///< returns name of plugin package containing filter_class (or "")
char *rte_keymode_get_type(int key, int mode) WARN_UNUSED;  ///< returns a string filter/instance type (or "")

#ifdef HAS_LIVES_EFFECTS_H
lives_fx_cat_t rte_keymode_get_category(int key, int mode);
#endif

weed_plant_t *rte_keymode_get_instance(int key, int mode); ///< returns filter_instance bound to key/mode (or NULL)
weed_plant_t *rte_keymode_get_filter(int key, int mode); ///< returns filter_class bound to key/mode (or NULL)

boolean weed_delete_effectkey(int key, int mode);  ///< unbinds a filter_class from a key/mode
int weed_add_effectkey(int key, const char *hashname, boolean fullname);  ///< bind a filter_class to key/mode using its hashname

int weed_add_effectkey_by_idx(int key, int idx);  ///< see description

int rte_key_getmode(int key);  ///< returns current active mode for a key (or -1)
int rte_key_getmaxmode(int key); ///< returns highest mode which is set

weed_plant_t *get_new_inst_for_keymode(int key, int mode); ///< get new inst (during recording playback)


boolean rte_key_setmode(int key, int newmode);  ///< set mode for a given key; if key==0 then the active key is used

///< returns -1 if the filter is not found; it will match the first name found - returns -2 if you try to switch a generator/non-generator
int rte_switch_keymode(int key, int mode, const char *hashname);

boolean rte_key_is_enabled(int key);



/////////////////////////////////////////////////////////////

int rte_getmodespk(void);
LiVESList *weed_get_all_names(lives_fx_list_t list_type);
int rte_get_numfilters(boolean inc_dupes);

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


int rte_bg_gen_key(void);
int rte_fg_gen_key(void);

int rte_bg_gen_mode(void);
int rte_fg_gen_mode(void);



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

#define WEED_PLANT_IS_PLUGIN_INFO(plant) (weed_get_plant_type(plant)==WEED_PLANT_PLUGIN_INFO?1:0)
#define WEED_PLANT_IS_HOST_INFO(plant) (weed_get_plant_type(plant)==WEED_PLANT_HOST_INFO?1:0)
#define WEED_PLANT_IS_FILTER_CLASS(plant) (weed_get_plant_type(plant)==WEED_PLANT_FILTER_CLASS?1:0)
#define WEED_PLANT_IS_FILTER_INSTANCE(plant) (weed_get_plant_type(plant)==WEED_PLANT_FILTER_INSTANCE?1:0)
#define WEED_PLANT_IS_CHANNEL(plant) (weed_get_plant_type(plant)==WEED_PLANT_CHANNEL?1:0)
#define WEED_PLANT_IS_CHANNEL_TEMPLATE(plant) (weed_get_plant_type(plant)==WEED_PLANT_CHANNEL_TEMPLATE?1:0)
#define WEED_PLANT_IS_PARAMETER(plant) (weed_get_plant_type(plant)==WEED_PLANT_PARAMETER?1:0)
#define WEED_PLANT_IS_PARAMETER_TEMPLATE(plant) (weed_get_plant_type(plant)==WEED_PLANT_PARAMETER_TEMPLATE?1:0)
#define WEED_PLANT_IS_GUI(plant) (weed_get_plant_type(plant)==WEED_PLANT_GUI?1:0)


#endif
