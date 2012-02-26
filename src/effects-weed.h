// effects-weed.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012 <salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#ifndef HAS_LIVES_EFFECTS_WEED_H
#define HAS_LIVES_EFFECTS_WEED_H

/// filter apply errors
typedef enum {
  FILTER_NO_ERROR=0,
  FILTER_ERROR_MISSING_LAYER=1,
  FILTER_ERROR_BLANK_FRAME=2,
  FILTER_ERROR_MISSING_FRAME=3,
  FILTER_ERROR_INVALID_PALETTE_CONVERSION=4,
  FILTER_ERROR_UNABLE_TO_RESIZE=5,
  FILTER_ERROR_INVALID_PALETTE_SETTINGS=6,
  FILTER_ERROR_COULD_NOT_REINIT=7,
  FILTER_ERROR_MUST_RELOAD=8,
  FILTER_ERROR_NO_IN_CHANNELS=9,
  FILTER_ERROR_INVALID_TRACK=10,
  FILTER_ERROR_INTERPOLATION_FAILED=11,
  FILTER_ERROR_INVALID_INSTANCE=12,
  FILTER_ERROR_INVALID_FILTER=13,
  FILTER_ERROR_INVALID_INIT_EVENT=14,
  FILTER_ERROR_IS_AUDIO=15,
  FILTER_ERROR_IS_SCRAP_FILE=16,

  FILTER_INFO_REINITED=512
} lives_filter_error_t;


// bootstrap function for weed plugins
weed_plant_t *weed_bootstrap_func (weed_default_getter_f *value, int num_versions, int *plugin_versions);

weed_plant_t *get_weed_filter(int filter_idx);
gchar *weed_filter_get_name(int filter_idx);
gchar *make_weed_hashname(int filter_idx, gboolean fullname) WARN_UNUSED;  ///< fullname includes author and version
int weed_get_idx_for_hashname (const gchar *hashname, gboolean fullname); ///< fullname includes author and version
gint enabled_in_channels (weed_plant_t *plant, gboolean count_repeats);
gint enabled_out_channels (weed_plant_t *plant, gboolean count_repeats);
weed_plant_t *get_enabled_channel (weed_plant_t *inst, gint which, gboolean is_in); ///< for FILTER_INST
weed_plant_t *get_mandatory_channel (weed_plant_t *filter, gint which, gboolean is_in); ///< for FILTER_CLASS
gboolean weed_filter_is_resizer(weed_plant_t *filt);
gboolean weed_instance_is_resizer(weed_plant_t *filt);

gboolean is_audio_channel_in(weed_plant_t *inst, int chnum);
gboolean has_video_chans_in(weed_plant_t *filter, gboolean count_opt);
gboolean has_audio_chans_in(weed_plant_t *filter, gboolean count_opt);
gboolean is_audio_channel_out(weed_plant_t *inst, int chnum);
gboolean has_video_chans_out(weed_plant_t *filter, gboolean count_opt);
gboolean has_audio_chans_out(weed_plant_t *filter, gboolean count_opt);
gboolean is_pure_audio(weed_plant_t *filter_or_instance, gboolean count_opt); ///< TRUE if audio in or out and no vid in/out

lives_fx_cat_t weed_filter_categorise (weed_plant_t *pl, int in_channels, int out_channels);
lives_fx_cat_t weed_filter_subcategorise (weed_plant_t *pl, lives_fx_cat_t category, gboolean count_opt);
gboolean has_usable_palette(weed_plant_t *chantmpl);
int check_weed_palette_list (int *palette_list, int num_palettes, int palette);

void weed_call_init_func(weed_plant_t *instance);
void weed_call_deinit_func(weed_plant_t *instance);

gchar *cd_to_plugin_dir(weed_plant_t *filter);
gboolean weed_init_effect(int hotkey); ///< hotkey starts at 1
void weed_deinit_effect(int hotkey); ///< hotkey starts at 1
weed_plant_t *weed_instance_from_filter(weed_plant_t *filter);
void weed_instance_ref(weed_plant_t *inst);
void weed_instance_unref(weed_plant_t *inst);
void weed_in_parameters_free (weed_plant_t *inst);
lives_filter_error_t weed_reinit_effect (weed_plant_t *inst, gboolean deinit_first);
void weed_reinit_all(void);


int weed_flagset_array_count(weed_plant_t **array, gboolean set_readonly);

int num_in_params(weed_plant_t *, gboolean count_reinits, gboolean count_variable);
weed_plant_t *weed_inst_in_param (weed_plant_t *inst, int param_num, gboolean skip_hidden);
gboolean is_hidden_param(weed_plant_t *plant, int i);
int get_nth_simple_param(weed_plant_t *plant, int pnum);
int count_simple_params(weed_plant_t *plant);
weed_plant_t **weed_params_create (weed_plant_t *filter, gboolean in);
int get_transition_param(weed_plant_t *filter);
int get_master_vol_param(weed_plant_t *filter);
gboolean is_perchannel_multiw(weed_plant_t *param);
gboolean has_perchannel_multiw(weed_plant_t *filter);
gboolean weed_parameter_has_variable_elements_strict(weed_plant_t *inst, weed_plant_t *ptmpl);


/// parameter interpolation
gboolean interpolate_param(weed_plant_t *inst, int i, void *pchain, weed_timecode_t tc);
gboolean interpolate_params(weed_plant_t *inst, void **pchains, weed_timecode_t tc);

void update_visual_params(lives_rfx_t *rfx, gboolean update_hidden);

gboolean weed_plant_serialise(int fd, weed_plant_t *plant, unsigned char **mem);
weed_plant_t *weed_plant_deserialise(int fd, unsigned char **mem);


/// record a parameter value change in our event_list
void rec_param_change(weed_plant_t *inst, int pnum);

weed_plant_t *get_textparm();

void weed_set_blend_factor(int hotkey);
gint weed_get_blend_factor(int hotkey);

void weed_memory_init(void); ///< call weed_init() with mem functions

void weed_load_all (void); ///< load effects
void weed_unload_all(void); ///< unload all effects
int get_next_free_key(void); ///< next free "key" for the multitrack system

void weed_deinit_all(gboolean shutdown); ///< deinit all active effects

weed_plant_t *weed_apply_effects (weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc, int opwidth, int opheight, void ***pchains);
lives_filter_error_t weed_apply_instance (weed_plant_t *inst, weed_plant_t *init_event, weed_plant_t **layers, int opwidth, int opheight, weed_timecode_t tc);
void weed_apply_audio_effects (weed_plant_t *filter_map, float **abuf, int nbtracks, int nchans, int64_t nsamps, gdouble arate, weed_timecode_t tc, double *vis);
lives_filter_error_t weed_apply_audio_instance (weed_plant_t *init_event, float **abuf, int nbtracks, int nchans, int64_t nsamps, gdouble arate, weed_timecode_t tc, double *vis);

gboolean weed_generator_start (weed_plant_t *inst);
weed_plant_t *weed_layer_new_from_generator (weed_plant_t *inst, weed_timecode_t tc);
void weed_generator_end (weed_plant_t *inst);
gboolean weed_playback_gen_start (void);
void weed_bg_generator_end (weed_plant_t *inst);


/// for multitrack
void backup_weed_instances(void);
void restore_weed_instances(void);


//////////////////////////////////////////////////////////
// WARNING !! "key" here starts at 1, "mode" starts at 0

gboolean rte_key_valid (gint key, gboolean is_userkey); ///< returns TRUE if there is a filter bound to active mode of hotkey
gboolean rte_keymode_valid (gint key, gint mode, gboolean is_userkey); ///< returns TRUE if a filter_class is bound to key/mode
gint rte_keymode_get_filter_idx(gint key, gint mode); ///< returns filter_class index of key/mode (or -1 if no filter bound)
gchar *rte_keymode_get_filter_name (gint key, gint mode); ///< returns name of filter_class bound to key/mode (or "")
gchar *rte_keymode_get_plugin_name(gint key, gint mode); ///< returns name of plugin package containing filter_class (or "")
gchar *rte_keymode_get_type (gint key, gint mode, gboolean get_subtype); ///< returns a string filter/instance type (or "")
lives_fx_cat_t rte_keymode_get_category (gint key, gint mode);
weed_plant_t *rte_keymode_get_instance(gint key, gint mode); ///< returns filter_instance bound to key/mode (or NULL)
weed_plant_t *rte_keymode_get_filter(gint key, gint mode); ///< returns filter_class bound to key/mode (or NULL)

gboolean weed_delete_effectkey (gint key, gint mode); ///< unbinds a filter_class from a key/mode
int weed_add_effectkey (gint key, const gchar *hashname, gboolean fullname); ///< bind a filter_class to key/mode using its hashname

int weed_add_effectkey_by_idx (gint key, int idx); ///< see description

int rte_key_getmode (gint key); ///< returns current active mode for a key (or -1)
int rte_key_getmaxmode(gint key); ///< returns highest mode which is set

gboolean rte_key_setmode (gint key, gint newmode); ///< set mode for a given key; if key==0 then the active key is used

///< returns -1 if the filter is not found; it will match the first name found - returns -2 if you try to switch a generator/non-generator
gint rte_switch_keymode (gint key, gint mode, const gchar *hashname);




/////////////////////////////////////////////////////////////

int rte_getmodespk (void);
GList *weed_get_all_names (gshort list_type);
gint rte_get_numfilters(void);

/////////////////////////////////////////////////////////
// key starts at 0

void free_key_defaults(gint key, gint mode);
void apply_key_defaults(weed_plant_t *inst, gint key, gint mode);
void write_key_defaults(int fd, gint key, gint mode);
gboolean read_key_defaults(int fd, int nparams, int key, int mode, int version);
void set_key_defaults(weed_plant_t *inst, gint key, gint mode);
gboolean has_key_defaults(void);


//////////////////////////////////////////////////////

void rte_swap_fg_bg (void);


gint rte_bg_gen_key (void);
gint rte_fg_gen_key (void);

gint rte_bg_gen_mode (void);
gint rte_fg_gen_mode (void);



////////////////////////////////////////////////////////////////////////

char *get_weed_display_string (weed_plant_t *inst, int pnum);
weed_plant_t *add_filter_deinit_events (weed_plant_t *event_list);
weed_plant_t *add_filter_init_events (weed_plant_t *event_list, weed_timecode_t tc);
void deinit_render_effects (void);

gboolean write_filter_defaults (int fd, int idx);
gboolean read_filter_defaults(int fd);

gboolean write_generator_sizes (int fd, int idx);
gboolean read_generator_sizes(int fd);

gint step_val(gint val, gint step);

void set_param_gui_readwrite (weed_plant_t *inst);
void set_param_gui_readonly (weed_plant_t *inst);

void weed_add_plant_flags (weed_plant_t *plant, int flags);

void update_host_info (weed_plant_t *inst);

/// add default filler values to a parameter or pchange.
void fill_param_vals_to (weed_plant_t *paramtmpl, weed_plant_t *param, int pnum, int hint, int index);


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
