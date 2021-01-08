// events.h
// LiVES
// (c) G. Finch 2005 - 2016 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions/structs for event_lists and events

#ifndef HAS_LIVES_EVENTS_H
#define HAS_LIVES_EVENTS_H

/// for backwards compat.
#define WEED_LEAF_HINT "hint"

/// parts of this may eventually become libweed-events

// event_list
#define WEED_LEAF_WEED_EVENT_API_VERSION "weed_event_api_version"
#define WEED_LEAF_AUDIO_SIGNED "audio_signed"
#define WEED_LEAF_AUDIO_ENDIAN "audio_endian"
#define WEED_LEAF_AUDIO_SAMPLE_SIZE "audio_sample_size"
#define WEED_LEAF_AUDIO_VOLUME_TRACKS "audio_volume_tracks"
#define WEED_LEAF_AUDIO_VOLUME_VALUES "audio_volume_values"
#define WEED_LEAF_TRACK_LABEL_TRACKS "track_label_tracks"
#define WEED_LEAF_TRACK_LABEL_VALUES "track_label_values"

#define WEED_LEAF_AUTHOR "author"
#define WEED_LEAF_TITLE "title"
#define WEED_LEAF_COMMENTS "comments"

#define WEED_LEAF_LIVES_CREATED_VERSION "created_version"
#define WEED_LEAF_LIVES_EDITED_VERSION "edited_version"

#define WEED_LEAF_CREATED_DATE "host_created_date"
#define WEED_LEAF_EDITED_DATE "host_edited_date"

// frame event
#define WEED_LEAF_FRAMES "frames"
#define WEED_LEAF_CLIPS "clips"
#define WEED_LEAF_AUDIO_CLIPS "audio_clips"
#define WEED_LEAF_AUDIO_SEEKS "audio_seeks"

// init_event
#define WEED_LEAF_FILTER "filter"
#define WEED_LEAF_IN_COUNT "in_count"
#define WEED_LEAF_OUT_COUNT "out_count"
#define WEED_LEAF_IN_TRACKS "in_tracks"
#define WEED_LEAF_OUT_TRACKS "out_tracks"
#define WEED_LEAF_EVENT_ID "event_id"

// deinit
#define WEED_LEAF_INIT_EVENT "init_event"

// filter map
#define WEED_LEAF_INIT_EVENTS "init_events"

// param change
#define WEED_LEAF_INDEX "index"

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

#define LIVES_TRACK_ANY -1000000

#define AUD_DIFF_MIN 0.05  ///< ignore audio seek differences < than this (seconds)
#define AUD_DIFF_REVADJ 8. ///< allow longer seek differences when audio plauback direction reverses (multiplying factor)

typedef weed_plant_t weed_event_t;

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

weed_event_t *append_frame_event(weed_event_t *event_list, ticks_t tc, int numframes,
                                 int *clips, int64_t *frames) WARN_UNUSED;
weed_event_t *append_filter_init_event(weed_event_t *event_list, ticks_t tc,
                                       int filter_idx, int num_in_tracks, int key, weed_plant_t *inst) WARN_UNUSED;
weed_event_t *append_filter_deinit_event(weed_event_t *event_list, ticks_t tc,
    void *init_event, void **pchain) WARN_UNUSED;
weed_event_t *append_filter_map_event(weed_event_t *event_list, ticks_t tc, void **init_events) WARN_UNUSED;
weed_event_t *append_param_change_event(weed_event_t *event_list, ticks_t tc, int pnum,
                                        weed_plant_t *param, void *init_event, void **pchain) WARN_UNUSED;
weed_event_t *append_marker_event(weed_event_t *event_list, ticks_t tc, int marker_type) WARN_UNUSED;

/** will either insert or replace */
weed_event_t *insert_frame_event_at(weed_event_t *event_list, ticks_t tc, int numframes,
                                    int *clips, int64_t *frames, weed_event_t **shortcut) WARN_UNUSED;
void insert_audio_event_at(weed_event_t *event, int track, int clipnum, double time, double vel);
void remove_audio_for_track(weed_event_t *event, int track);
weed_event_t *insert_blank_frame_event_at(weed_event_t *event_list, ticks_t tc,
    weed_event_t **shortcut) WARN_UNUSED;

void remove_frame_from_event(weed_event_t *event_list, weed_event_t *event, int track);
void remove_end_blank_frames(weed_event_t *event_list, boolean remove_filter_inits);
void remove_filter_from_event_list(weed_event_t *event_list, weed_event_t *init_event);

weed_event_t *process_events(weed_event_t *next_event, boolean process_audio, ticks_t curr_tc);  ///< RT playback
void event_list_close_start_gap(weed_event_t *event_list);
void event_list_add_track(weed_event_t *event_list, int layer);
void add_track_to_avol_init(weed_plant_t *filter, weed_event_t *event, int nbtracks, boolean behind);
void event_list_free(weed_event_t *event_list);

void event_list_add_end_events(weed_event_t *event_list, boolean is_final);


/// lib-ish stuff
weed_event_t *lives_event_list_new(weed_event_t *elist, const char *cdate);
int weed_event_get_type(weed_event_t *event);
weed_timecode_t weed_event_set_timecode(weed_event_t *, weed_timecode_t tc);
weed_timecode_t weed_event_get_timecode(weed_event_t *);

int weed_frame_event_get_tracks(weed_event_t *event,  int **clips, int64_t **frames); // returns ntracks
int weed_frame_event_get_audio_tracks(weed_event_t *event,  int **aclips, double **aseeks); // returns natracks

/// replace events in event_list with events in new_event_list
void event_list_replace_events(weed_event_t *event_list, weed_event_t *new_event_list);

/// called during quantisation
weed_event_t *event_copy_and_insert(weed_event_t *in_event, weed_timecode_t tc, weed_event_t *event_list,
                                    weed_event_t **ret_event);
void reset_ttable(void);

/// if all_events is FALSE we only count FRAME events
int count_events(weed_event_t *event_list, boolean all_events, ticks_t start_tc, ticks_t end_tc);

frames_t count_resampled_events(weed_event_t *event_list, double fps);

boolean backup_recording(char **esave_file, char **asave_file);

boolean event_list_to_block(weed_event_t *event_list, int num_events);
double event_list_get_end_secs(weed_event_t *event_list);
double event_list_get_start_secs(weed_event_t *event_list);
ticks_t event_list_get_end_tc(weed_event_t *event_list);
ticks_t event_list_get_start_tc(weed_event_t *event_list);

weed_event_t *get_last_frame_event(weed_event_t *event_list);
weed_event_t *get_first_frame_event(weed_event_t *event_list);

weed_event_t *get_next_frame_event(weed_event_t *event);
weed_event_t *get_prev_frame_event(weed_event_t *event);

weed_event_t *get_next_audio_frame_event(weed_event_t *event);
weed_event_t *get_prev_audio_frame_event(weed_event_t *event);

weed_event_t *get_frame_event_at(weed_event_t *event_list, ticks_t tc, weed_event_t *shortcut, boolean exact);
weed_event_t *get_frame_event_at_or_before(weed_event_t *event_list, ticks_t tc, weed_event_t *shortcut);

weed_event_t *get_audio_block_start(weed_event_t *event_list, int track, ticks_t tc, boolean seek_back);

boolean filter_map_after_frame(weed_event_t *fmap);
boolean init_event_is_relevant(weed_event_t *init_event, int ctrack);

// definitions in events.c
weed_event_t *get_first_event(weed_event_t *event_list);
weed_event_t *get_last_event(weed_event_t *event_list);
weed_event_t *get_prev_event(weed_event_t *event);
weed_event_t *get_next_event(weed_event_t *event);

//////////////////////////////////////////////////////////

ticks_t get_event_timecode(weed_event_t *);
int get_event_type(weed_event_t *);
boolean is_blank_frame(weed_event_t *, boolean count_audio);
boolean has_audio_frame(weed_event_t *event_list);
int get_frame_event_clip(weed_event_t *, int layer);
frames_t get_frame_event_frame(weed_event_t *, int layer);
boolean frame_event_has_frame_for_track(weed_event_t *event, int track);
double *get_track_visibility_at_tc(weed_event_t *event_list, int ntracks, int n_back_tracks,
                                   ticks_t tc, weed_event_t **shortcut, boolean bleedthru);
void get_active_track_list(int *clip_index, int num_tracks, weed_plant_t *filter_map);

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
boolean deal_with_render_choice(boolean add_deinit);

#define RENDER_CHOICE_NONE 0
#define RENDER_CHOICE_DISCARD 1
#define RENDER_CHOICE_PREVIEW 2
#define RENDER_CHOICE_SAME_CLIP 3
#define RENDER_CHOICE_NEW_CLIP 4
#define RENDER_CHOICE_MULTITRACK 5
#define RENDER_CHOICE_EVENT_LIST 6
#define RENDER_CHOICE_TRANSCODE 7

LiVESWidget *create_event_list_dialog(weed_plant_t *event_list, ticks_t start_tc, ticks_t end_tc);
render_details *create_render_details(int type);

LiVESWidget *add_video_options(LiVESWidget **spwidth, int defwidth, LiVESWidget **spheight, int defheight,
                               LiVESWidget **spfps, double deffps, LiVESWidget **spframes, int defframes,
                               boolean add_aspect, LiVESWidget *extra);

LiVESWidget *add_audio_options(LiVESWidget **cbbackaudio, LiVESWidget **cbpertrack);

////////////////////////////////////////////////////////////////
/// rendering

boolean render_to_clip(boolean new_clip, boolean transcode);
boolean start_render_effect_events(weed_plant_t *event_list, boolean render_vid, boolean render_aud);

lives_render_error_t render_events(boolean reset, boolean rend_video, boolean rend_audio);
lives_render_error_t render_events_cb(boolean dummy);

// effect insertion/updating
void insert_filter_init_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event);
void **filter_init_add_pchanges(weed_plant_t *event_list, weed_plant_t *filter, weed_plant_t *init_event, int ntracks,
                                int leave);
void insert_filter_deinit_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event);
boolean insert_filter_map_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event,
                                   boolean before_frames);
weed_plant_t *get_filter_map_before(weed_plant_t *event, int ctrack, weed_plant_t *stop_event);
weed_plant_t *get_filter_map_after(weed_plant_t *event, int ctrack);
void **get_init_events_before(weed_plant_t *event, weed_plant_t *init_event, boolean add);
void update_filter_maps(weed_plant_t *event, weed_plant_t *end_event, weed_plant_t *init_event);
void insert_param_change_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event);
weed_plant_t *insert_marker_event_at(weed_plant_t *event_list, weed_plant_t *at_event, int marker_type, livespointer data);

void add_init_event_to_filter_map(weed_plant_t *fmap, weed_plant_t *event, void **hints);
boolean init_event_in_list(void **init_events, int num_inits, weed_plant_t *event);
boolean filter_init_has_owner(weed_plant_t *init_event, int track);
boolean init_event_is_process_last(weed_plant_t *event);

// effect deletion/moving
boolean move_event_right(weed_plant_t *event_list, weed_plant_t *event, boolean can_stay, double fps);
boolean move_event_left(weed_plant_t *event_list, weed_plant_t *event, boolean can_stay, double fps);

void move_filter_init_event(weed_plant_t *event_list, ticks_t new_tc, weed_plant_t *init_event, double fps);
void move_filter_deinit_event(weed_plant_t *event_list, ticks_t new_tc, weed_plant_t *deinit_event,
                              double fps, boolean rescale_pchanges);

// event deletion
void unlink_event(weed_plant_t *event_list, weed_plant_t *event);
void delete_event(weed_plant_t *event_list, weed_plant_t *event);

// event replacement
void replace_event(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event);

// event insertion
boolean insert_event_before(weed_plant_t *at_event, weed_plant_t *event);
boolean insert_event_after(weed_plant_t *at_event, weed_plant_t *event);

// param changes
void ** *get_event_pchains(void);
ticks_t get_next_paramchange(void **pchange_next, ticks_t end_tc);
ticks_t get_prev_paramchange(void **pchange_next, ticks_t start_tc);
boolean is_init_pchange(weed_plant_t *init_event, weed_plant_t *pchange_event);
void free_pchains(int key);

// audio
/// returns clip number for track (track==-1 is backing audio)
int get_audio_frame_clip(weed_plant_t *event, int track);

/// returns velocity for track (track==-1 is backing audio)
double get_audio_frame_vel(weed_plant_t *event, int track);

/// returns velocity for track (track==-1 is backing audio)
double get_audio_frame_seek(weed_plant_t *event, int track);

// playback

void backup_host_tags(weed_plant_t *event_list, ticks_t curr_tc);
void restore_host_tags(weed_plant_t *event_list, ticks_t curr_tc);

boolean has_frame_event_at(weed_plant_t *event_list, ticks_t tc, weed_plant_t **shortcut);

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
