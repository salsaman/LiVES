// events.h
// LiVES
// (c) G. Finch 2005 - 2016 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions/structs for event_lists and events

#ifndef HAS_LIVES_EVENTS_H
#define HAS_LIVES_EVENTS_H

#define DEF_AFADE_SECS 10.
#define DEF_VFADE_SECS 10.

/// for backwards compat.
#define WEED_LEAF_HINT "hint"

/// parts of this may eventually become libweed-events

/// this struct is used only when physically resampling frames on the disk
/// we create an array of these and write them to the disk
typedef struct {
  int value;
  int64_t reltime;
} resample_event;

typedef struct {
  int afile;
  double seek;
  double vel;
} lives_audio_track_state_t;

// event_list
#define LIVES_LEAF_CREATED_DATE "host_created_date"
#define LIVES_LEAF_EDITED_DATE "host_edited_date"

// internal
// event_list
#define WEED_LEAF_NEXT "next"
#define WEED_LEAF_PREVIOUS "previous"
#define WEED_LEAF_FIRST "first"
#define WEED_LEAF_LAST "last"
#define WEED_LEAF_NEEDS_SET "needs_set" // oops, should have been host_needs_set
#define WEED_LEAF_GAMMA_ENABLED "host_gamma_enabled"
#define WEED_LEAF_TC_ADJUSTMENT "tc_adj_val"

// param change
#define WEED_LEAF_NEXT_CHANGE  "next_change"
#define WEED_LEAF_PREV_CHANGE  "prev_change"
#define WEED_LEAF_IS_DEF_VALUE "host_is_def_value"

// init_event
#define WEED_LEAF_DEINIT_EVENT "deinit_event"

// marker
#define WEED_LEAF_LIVES_TYPE "lives_type"
#define WEED_LEAF_TRACKS "tracks"
#define WEED_LEAF_TCDELTA "tc_delta"

// misc
#define WEED_LEAF_PTRSIZE "ptrsize" ///< deprecated

#define WEED_LEAF_HOST_AUDIO_TRANSITION "host_audio_transition"

#define WEED_LEAF_HOST_TAG_COPY "host_tag_copy"

#define WEED_LEAF_OVERLAY_TEXT "overlay_text"

#define LIVES_LEAF_FAKE_TC "fake_tc"
#define LIVES_LEAF_SCRATCH "scratch_val"
#define LIVES_LEAF_ALLOW_JUMP "allow_jump"

#define LIVES_TRACK_ANY -1000000

#define AUD_DIFF_MIN 0.2  ///< ignore audio seek differences < than this (seconds)
#define AUD_DIFF_REVADJ 8. ///< allow longer seek differences when audio plauback direction reverses (multiplying factor)

#ifndef HAS_EVENT_TYPEDEFS
typedef weed_plant_t weed_event_t;
typedef weed_plant_t weed_event_list_t;
#endif

/// various return conditions from rendering (multitrack or after recording)
typedef enum {
  LIVES_RENDER_ERROR_NONE = 0,
  LIVES_RENDER_READY,
  LIVES_RENDER_PROCESSING,
  LIVES_RENDER_EFFECTS_PAUSED,
  LIVES_RENDER_COMPLETE,
  LIVES_RENDER_WARNING,
  LIVES_RENDER_WARNING_READ_FRAME,
  LIVES_RENDER_ERROR,
  LIVES_RENDER_ERROR_READ_AUDIO,
  LIVES_RENDER_ERROR_WRITE_AUDIO,
  LIVES_RENDER_ERROR_WRITE_FRAME,
  LIVES_RENDER_ERROR_MEMORY
} lives_render_error_t;

weed_event_list_t *append_frame_event(weed_event_list_t *, ticks_t tc, int numframes,
                                      int *clips, frames64_t *frames) WARN_UNUSED;
weed_event_list_t *append_filter_init_event(weed_event_list_t *, ticks_t tc,
    int filter_idx, int num_in_tracks, int key, weed_plant_t *inst) WARN_UNUSED;
weed_event_list_t *append_filter_deinit_event(weed_event_list_t *, ticks_t tc,
    void *init_event, void **pchain) WARN_UNUSED;
weed_event_list_t *append_filter_map_event(weed_event_list_t *, ticks_t tc, weed_event_t **init_events) WARN_UNUSED;
weed_event_list_t *append_param_change_event(weed_event_list_t *, ticks_t tc, int pnum,
    weed_plant_t *param, void *init_event, void **pchain) WARN_UNUSED;
weed_event_list_t *append_marker_event(weed_event_list_t *, ticks_t tc, int marker_type) WARN_UNUSED;

/** will either insert or replace */
weed_event_list_t *insert_frame_event_at(weed_event_list_t *, ticks_t tc, int numframes,
    int *clips, frames64_t *frames, weed_event_t **shortcut) WARN_UNUSED;
void insert_audio_event_at(weed_event_t *event, int track, int clipnum, double time, double vel);
void remove_audio_for_track(weed_event_t *event, int track);
weed_event_list_t *insert_blank_frame_event_at(weed_event_list_t *, ticks_t tc,
    weed_event_t **shortcut) WARN_UNUSED;

void remove_frame_from_event(weed_event_list_t *, weed_event_t *event, int track);
void remove_end_blank_frames(weed_event_list_t *, boolean remove_filter_inits);
void remove_filter_from_event_list(weed_event_list_t *, weed_event_t *init_event);

void rescale_param_changes(weed_event_list_t *, weed_event_t *init_event, weed_timecode_t new_init_tc,
                           weed_event_t *deinit_event, weed_timecode_t new_deinit_tc, double fps);

weed_event_t *process_events(weed_event_t *next_event, boolean process_audio, ticks_t curr_tc);  ///< RT playback
void event_list_close_start_gap(weed_event_list_t *);
void event_list_add_track(weed_event_list_t *, int layer);
void add_track_to_avol_init(weed_plant_t *filter, weed_event_t *event, int nbtracks, boolean behind);
void event_list_free(weed_event_list_t *);

void event_list_add_end_events(weed_event_list_t *, boolean is_final);


/// lib-ish stuff
weed_event_list_t *lives_event_list_new(weed_event_list_t *elist, const char *cdate);
int weed_event_get_type(weed_event_t *event);
weed_error_t weed_event_set_timecode(weed_event_t *, weed_timecode_t tc);
weed_timecode_t weed_event_get_timecode(weed_event_t *);

int weed_frame_event_get_tracks(weed_event_t *event,  int **clips, frames64_t **frames); // returns ntracks
int weed_frame_event_get_audio_tracks(weed_event_t *event,  int **aclips, double **aseeks); // returns natracks

/// replace events in event_list with events in new_event_list
void event_list_replace_events(weed_event_list_t *event_list, weed_event_list_t *new_event_list);

/// called during quantisation
weed_event_list_t *event_copy_and_insert(weed_event_t *in_event, weed_timecode_t tc, weed_event_list_t *,
    weed_event_t **ret_event);
void reset_ttable(void);

/// if all_events is FALSE we only count FRAME events
int count_events(weed_event_list_t *, boolean all_events, ticks_t start_tc, ticks_t end_tc);

frames_t count_resampled_events(weed_event_list_t *, double fps);

boolean backup_recording(char **esave_file, char **asave_file);

boolean event_list_to_block(weed_event_list_t *, int num_events);
double event_list_get_end_secs(weed_event_list_t *);
double event_list_get_start_secs(weed_event_list_t *);
ticks_t event_list_get_end_tc(weed_event_list_t *);
ticks_t event_list_get_start_tc(weed_event_list_t *);

weed_event_t *get_last_frame_event(weed_event_list_t *);
weed_event_t *get_first_frame_event(weed_event_list_t *);

weed_event_t *get_next_frame_event(weed_event_t *event);
weed_event_t *get_prev_frame_event(weed_event_t *event);

weed_event_t *get_next_audio_frame_event(weed_event_t *event);
weed_event_t *get_prev_audio_frame_event(weed_event_t *event);

weed_event_t *get_frame_event_at(weed_event_list_t *, ticks_t tc, weed_event_t *shortcut, boolean exact);
weed_event_t *get_frame_event_at_or_before(weed_event_list_t *, ticks_t tc, weed_event_t *shortcut);

weed_event_t *get_audio_block_start(weed_event_list_t *, int track, ticks_t tc, boolean seek_back);

boolean filter_map_after_frame(weed_event_t *fmap);
boolean init_event_is_relevant(weed_event_t *init_event, int ctrack);

// definitions in events.c
weed_event_t *get_first_event(weed_event_list_t *);
weed_event_t *get_last_event(weed_event_list_t *);
weed_event_t *get_prev_event(weed_event_t *event);
weed_event_t *get_next_event(weed_event_t *event);

//////////////////////////////////////////////////////////

ticks_t get_event_timecode(weed_event_t *);
int get_event_type(weed_event_t *);
boolean is_blank_frame(weed_event_t *, boolean count_audio);
boolean has_audio_frame(weed_event_list_t *);
int get_frame_event_clip(weed_event_t *, int layer);
frames_t get_frame_event_frame(weed_event_t *, int layer);
boolean frame_event_has_frame_for_track(weed_event_t *event, int track);
double *get_track_visibility_at_tc(weed_event_list_t *, int ntracks, int n_back_tracks,
                                   ticks_t tc, weed_event_t **shortcut, boolean bleedthru);
void get_active_track_list(int *clip_index, int num_tracks, weed_event_t *filter_map);

//////////////////////////////////////////////////////////
///// render details //////////

typedef struct {
  int width;
  int height;
  double fps;
  boolean ratio_fps;
  LiVESWidget *dialog;
  LiVESWidget *okbutton;
  LiVESWidget *usecur_button;
  LiVESWidget *clipname_entry;
  LiVESWidget *encoder_combo;
  LiVESWidget *ofmt_combo;
  LiVESWidget *acodec_combo;
  LiVESWidget *acodec_entry;
  LiVESWidget *spinbutton_width;
  LiVESWidget *spinbutton_height;
  LiVESWidget *as_lock;
  LiVESWidget *spinbutton_fps;
  LiVESWidget *pertrack_checkbutton;
  LiVESWidget *backaudio_checkbutton;
  LiVESWidget *always_checkbutton;
  LiVESWidget *always_hbox;
  LiVESWidget *debug;
  LiVESWidget *norm_after;
  LiVESWidget *afade_in;
  LiVESWidget *afade_out;
  LiVESWidget *vfade_in;
  LiVESWidget *vfade_out;
  LiVESWidget *vfade_col;
  LiVESWidget *enc_lb;
  ulong encoder_name_fn;
  ulong encoder_ofmt_fn;
  boolean enc_changed;
  char *encoder_name;
  boolean suggestion_followed;

  boolean is_encoding;

  int arate;
  int achans;
  int asamps;
  int aendian;
} render_details;

render_details *rdet;

////////////////////////////////////////////////////////
//// UI stuff ///////

LiVESWidget *events_rec_dialog(void);

#define RENDER_CHOICE_NONE 0
#define RENDER_CHOICE_DISCARD 1
#define RENDER_CHOICE_PREVIEW 2
#define RENDER_CHOICE_SAME_CLIP 3
#define RENDER_CHOICE_NEW_CLIP 4
#define RENDER_CHOICE_MULTITRACK 5
#define RENDER_CHOICE_EVENT_LIST 6
#define RENDER_CHOICE_TRANSCODE 7

boolean deal_with_render_choice(boolean add_deinit);

// *experimental - not all opts impleted / may change*
#define EL_OPTS_SHOWALL			0 ///< show all
#define EL_OPT_BLKSTARTS		(1ull << 0) ///< show block starts
#define EL_OPT_BLKENDS			(1ull << 1) ///< show block ends
#define EL_OPT_TRNS			(1ull << 2) ///< show transitions
#define EL_OPT_FX			(1ull << 3) ///< show effects

#define EL_OPT_TRKAUDIO			(1ull << 16) ///< show track audio details
#define EL_OPT_BACKAUDIO		(1ull << 17) ///< show backing audio details
#define EL_OPT_SUBS			(1ull << 18) ///< merge with subtitles

#define EL_OPT_PREVIEW			(1ull << 32) ///< add preview play buttons
#define EL_OPT_EDITABLE			(1ull << 33) ///< user changeable values
#define EL_OPT_COMPACT			(1ull << 34) ///< show fewer details

LiVESWidget *create_event_list_dialog(weed_event_list_t *, ticks_t start_tc, ticks_t end_tc, uint64_t opts);

// create the dialog which prompts for render parameters, e.g. width, height, fps
render_details *create_render_details(int type);

// widget sets which can be added in dialogs
LiVESWidget *add_video_options(LiVESWidget **spwidth, int defwidth, LiVESWidget **spheight, int defheight,
                               LiVESWidget **spfps, double deffps, LiVESWidget **spframes, int defframes,
                               LiVESWidget **as_lock, LiVESWidget *extra);

LiVESWidget *add_audio_options(LiVESWidget **cbbackaudio, LiVESWidget **cbpertrack);

////////////////////////////////////////////////////////////////
/// rendering

boolean render_to_clip(boolean new_clip);
boolean start_render_effect_events(weed_event_list_t *, boolean render_vid, boolean render_aud);

lives_render_error_t render_events(boolean reset, boolean rend_video, boolean rend_audio);
lives_render_error_t render_events_cb(boolean dummy);

// effect insertion/updating
void insert_filter_init_event_at(weed_event_list_t *, weed_event_t *at_event, weed_event_t *event);
void **filter_init_add_pchanges(weed_event_list_t *, weed_plant_t *filter, weed_event_t *init_event, int ntracks,
                                int leave);
void insert_filter_deinit_event_at(weed_event_list_t *, weed_event_t *at_event, weed_event_t *event);
boolean insert_filter_map_event_at(weed_event_list_t *, weed_event_t *at_event, weed_event_t *event,
                                   boolean before_frames);
weed_event_t *get_filter_map_before(weed_event_t *event, int ctrack, weed_event_t *stop_event);
weed_event_t *get_filter_map_after(weed_event_t *event, int ctrack);
weed_event_t **get_init_events_before(weed_event_t *event, weed_event_t *init_event, boolean add);
void update_filter_maps(weed_event_t *event, weed_event_t *end_event, weed_event_t **init_event, int ninits);
void insert_param_change_event_at(weed_event_list_t *, weed_event_t *at_event, weed_event_t *event);
weed_event_list_t *insert_marker_event_at(weed_event_list_t *, weed_event_t *at_event, int marker_type, livespointer data);

void add_init_event_to_filter_map(weed_event_t *fmap, weed_event_t *event, void **hints);
boolean init_event_in_list(void **init_events, int num_inits, weed_event_t *event);
boolean filter_init_has_owner(weed_event_t *init_event, int track);
boolean init_event_is_process_last(weed_event_t *event);

void **append_to_easing_events(void **eevents, int *nev, weed_event_t *init_event);

// effect deletion/moving
boolean move_event_right(weed_event_list_t *, weed_event_t *event, boolean can_stay, double fps);
boolean move_event_left(weed_event_list_t *, weed_event_t *event, boolean can_stay, double fps);

void move_filter_init_event(weed_event_list_t *, ticks_t new_tc, weed_event_t *init_event, double fps);
void move_filter_deinit_event(weed_event_list_t *, ticks_t new_tc, weed_event_t *deinit_event,
                              double fps, boolean rescale_pchanges);

// event deletion
void unlink_event(weed_event_list_t *, weed_event_t *event);
void delete_event(weed_event_list_t *, weed_event_t *event);

// event replacement
void replace_event(weed_event_list_t *, weed_event_t *at_event, weed_event_t *event);

// event insertion
boolean insert_event_before(weed_event_t *at_event, weed_event_t *event);
boolean insert_event_after(weed_event_t *at_event, weed_event_t *event);

// param changes
void ** *get_event_pchains(void);
ticks_t get_next_paramchange(void **pchange_next, ticks_t end_tc);
ticks_t get_prev_paramchange(void **pchange_next, ticks_t start_tc);
boolean is_init_pchange(weed_event_t *init_event, weed_event_t *pchange_event);
void free_pchains(int key);

// audio
/// returns clip number for track (track==-1 is backing audio)
int get_audio_frame_clip(weed_event_t *event, int track);

/// returns velocity for track (track==-1 is backing audio)
double get_audio_frame_vel(weed_event_t *event, int track);

/// returns velocity for track (track==-1 is backing audio)
double get_audio_frame_seek(weed_event_t *event, int track);

// playback

void backup_host_tags(weed_event_list_t *, ticks_t curr_tc);
void restore_host_tags(weed_event_list_t *, ticks_t curr_tc);

boolean has_frame_event_at(weed_event_list_t *, ticks_t tc, weed_event_t **shortcut);

#define EVENT_MARKER_BLOCK_START 1
#define EVENT_MARKER_BLOCK_UNORDERED 512
#define EVENT_MARKER_RECORD_START 1024
#define EVENT_MARKER_RECORD_END 1025

#define WEED_PLANT_IS_EVENT(plant) ((plant != NULL && weed_get_plant_type(plant) == WEED_PLANT_EVENT) ? 1 : 0)
#define WEED_PLANT_IS_EVENT_LIST(plant) ((plant != NULL && weed_get_plant_type(plant) == WEED_PLANT_EVENT_LIST) ? 1 : 0)

#define WEED_EVENT_IS_FRAME(event) (get_event_type(event) == WEED_EVENT_TYPE_FRAME ? 1 : 0)
#define WEED_EVENT_IS_AUDIO_FRAME(event) ((get_event_type(event) == WEED_EVENT_TYPE_FRAME \
					   && weed_plant_has_leaf(event, WEED_LEAF_AUDIO_CLIPS)) ? 1 : 0)
#define WEED_EVENT_IS_FILTER_INIT(event) (get_event_type(event) == WEED_EVENT_TYPE_FILTER_INIT ? 1 : 0)
#define WEED_EVENT_IS_FILTER_DEINIT(event) (get_event_type(event) == WEED_EVENT_TYPE_FILTER_DEINIT ? 1 : 0)
#define WEED_EVENT_IS_FILTER_MAP(event) (get_event_type(event) == WEED_EVENT_TYPE_FILTER_MAP ? 1 : 0)
#define WEED_EVENT_IS_PARAM_CHANGE(event) (get_event_type(event) == WEED_EVENT_TYPE_PARAM_CHANGE ? 1 : 0)
#define WEED_EVENT_IS_MARKER(event) (get_event_type(event) == WEED_EVENT_TYPE_MARKER ? 1 : 0)

#endif // HAS_LIVES_EVENTS_H
