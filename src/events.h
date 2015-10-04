// events.h
// LiVES
// (c) G. Finch 2005 - 2010 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions/structs for event_lists and events

#ifndef HAS_LIVES_EVENTS_H
#define HAS_LIVES_EVENTS_H

weed_plant_t *append_frame_event(weed_plant_t *event_list, weed_timecode_t tc, int numframes,
                                 int *clips, int *frames) WARN_UNUSED;
weed_plant_t *append_filter_init_event(weed_plant_t *event_list, weed_timecode_t tc,
                                       int filter_idx, int num_in_tracks, int key, weed_plant_t *inst) WARN_UNUSED;
weed_plant_t *append_filter_deinit_event(weed_plant_t *event_list, weed_timecode_t tc,
    void *init_event, void **pchain) WARN_UNUSED;
weed_plant_t *append_filter_map_event(weed_plant_t *event_list, weed_timecode_t tc, void **init_events) WARN_UNUSED;
weed_plant_t *append_param_change_event(weed_plant_t *event_list, weed_timecode_t tc, int pnum,
                                        weed_plant_t *param, void *init_event, void **pchain) WARN_UNUSED;
weed_plant_t *append_marker_event(weed_plant_t *event_list, weed_timecode_t tc, int marker_type) WARN_UNUSED;

/** will either insert or replace */
weed_plant_t *insert_frame_event_at(weed_plant_t *event_list, weed_timecode_t tc, int numframes,
                                    int *clips, int *frames, weed_plant_t **shortcut) WARN_UNUSED;
void insert_audio_event_at(weed_plant_t *event_list,weed_plant_t *event, int track, int clipnum,
                           double time, double vel);
void remove_audio_for_track(weed_plant_t *event, int track);
weed_plant_t *insert_blank_frame_event_at(weed_plant_t *event_list, weed_timecode_t tc,
    weed_plant_t **shortcut) WARN_UNUSED;

void remove_frame_from_event(weed_plant_t *event_list, weed_plant_t *event, int track);
void remove_end_blank_frames(weed_plant_t *event_list, boolean remove_filter_inits);
void remove_filter_from_event_list(weed_plant_t *event_list, weed_plant_t *init_event);

weed_plant_t *process_events(weed_plant_t *next_event, boolean process_audio, weed_timecode_t curr_tc);  ///< RT playback
void event_list_close_start_gap(weed_plant_t *event_list);
void event_list_add_track(weed_plant_t *event_list, int layer);
void add_track_to_avol_init(weed_plant_t *filter, weed_plant_t *event, int nbtracks, boolean behind);
void event_list_free(weed_plant_t *event_list);

/// replace events in event_list with events in new_event_list
void event_list_replace_events(weed_plant_t *event_list, weed_plant_t *new_event_list);

/// called during quantisation
weed_plant_t *event_copy_and_insert(weed_plant_t *in_event, weed_plant_t *event_list);

/// if all_events is FALSE we only count FRAME events
int count_events(weed_plant_t *event_list, boolean all_events, weed_timecode_t start_tc, weed_timecode_t end_tc);

int count_resampled_events(weed_plant_t *event_list, double fps);

boolean event_list_to_block(weed_plant_t *event_list, int num_events);
double event_list_get_end_secs(weed_plant_t *event_list);
double event_list_get_start_secs(weed_plant_t *event_list);
weed_timecode_t event_list_get_end_tc(weed_plant_t *event_list);
weed_timecode_t event_list_get_start_tc(weed_plant_t *event_list);

weed_plant_t *get_last_frame_event(weed_plant_t *event_list);
weed_plant_t *get_first_frame_event(weed_plant_t *event_list);

weed_plant_t *get_next_frame_event(weed_plant_t *event);
weed_plant_t *get_prev_frame_event(weed_plant_t *event);

weed_plant_t *get_next_audio_frame_event(weed_plant_t *event);
weed_plant_t *get_prev_audio_frame_event(weed_plant_t *event);

weed_plant_t *get_frame_event_at(weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t *shortcut, boolean exact);
weed_plant_t *get_frame_event_at_or_before(weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t *shortcut);

weed_plant_t *get_audio_block_start(weed_plant_t *event_list, int track, weed_timecode_t tc, boolean seek_back);

boolean filter_map_after_frame(weed_plant_t *fmap);
boolean init_event_is_relevant(weed_plant_t *init_event, int ctrack);


// definitions in events.c
weed_plant_t *get_first_event(weed_plant_t *event_list);
weed_plant_t *get_last_event(weed_plant_t *event_list);
weed_plant_t *get_prev_event(weed_plant_t *event);
weed_plant_t *get_next_event(weed_plant_t *event);




//////////////////////////////////////////////////////////
#if HAVE_SYSTEM_WEED
#include <weed/weed-utils.h>
#else
#include "../libweed/weed-utils.h"
#endif

weed_timecode_t get_event_timecode(weed_plant_t *);
int get_event_hint(weed_plant_t *);
boolean is_blank_frame(weed_plant_t *, boolean count_audio);
boolean has_audio_frame(weed_plant_t *event_list);
int get_frame_event_clip(weed_plant_t *, int layer);
int get_frame_event_frame(weed_plant_t *, int layer);
boolean frame_event_has_frame_for_track(weed_plant_t *event, int track);
double *get_track_visibility_at_tc(weed_plant_t *event_list, int ntracks, int n_back_tracks,
                                   weed_timecode_t tc, weed_plant_t **shortcut, boolean bleedthru);
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


LiVESWidget *events_rec_dialog(boolean allow_mt);
int get_render_choice(void);
boolean deal_with_render_choice(boolean add_deinit);

#define RENDER_CHOICE_DISCARD 0
#define RENDER_CHOICE_PREVIEW 1
#define RENDER_CHOICE_SAME_CLIP 2
#define RENDER_CHOICE_NEW_CLIP 3
#define RENDER_CHOICE_MULTITRACK 4
#define RENDER_CHOICE_EVENT_LIST 5

LiVESWidget *create_event_list_dialog(weed_plant_t *event_list, weed_timecode_t start_tc, weed_timecode_t end_tc);
render_details *create_render_details(int type);

////////////////////////////////////////////////////////////////
/// rendering

boolean render_to_clip(boolean new_clip);  ///< render to clip
boolean start_render_effect_events(weed_plant_t *event_list);  ///< render to clip


// effect insertion/updating
void insert_filter_init_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event);
void **filter_init_add_pchanges(weed_plant_t *event_list, weed_plant_t *filter, weed_plant_t *init_event, int ntracks, int leave);
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

void move_filter_init_event(weed_plant_t *event_list, weed_timecode_t new_tc, weed_plant_t *init_event, double fps);
void move_filter_deinit_event(weed_plant_t *event_list, weed_timecode_t new_tc, weed_plant_t *deinit_event,
                              double fps, boolean rescale_pchanges);

// event deletion
void unlink_event(weed_plant_t *event_list, weed_plant_t *event);
void delete_event(weed_plant_t *event_list, weed_plant_t *event);

// event replacement
void replace_event(weed_plant_t *event_list, weed_plant_t *at_event,weed_plant_t *event);

// event insertion
boolean insert_event_before(weed_plant_t *at_event,weed_plant_t *event);
boolean insert_event_after(weed_plant_t *at_event,weed_plant_t *event);

// param changes
void ** *get_event_pchains(void);
weed_timecode_t get_next_paramchange(void **pchange_next,weed_timecode_t end_tc);
weed_timecode_t get_prev_paramchange(void **pchange_next,weed_timecode_t start_tc);
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

void backup_host_tags(weed_plant_t *event_list, weed_timecode_t curr_tc);
void restore_host_tags(weed_plant_t *event_list, weed_timecode_t curr_tc);

boolean has_frame_event_at(weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t **shortcut);


#define EVENT_MARKER_BLOCK_START 1
#define EVENT_MARKER_BLOCK_UNORDERED 512
#define EVENT_MARKER_RECORD_START 1024
#define EVENT_MARKER_RECORD_END 1025

#define WEED_PLANT_IS_EVENT(plant) ((plant!=NULL&&weed_get_plant_type(plant)==WEED_PLANT_EVENT)?1:0)
#define WEED_PLANT_IS_EVENT_LIST(plant) ((plant!=NULL&&weed_get_plant_type(plant)==WEED_PLANT_EVENT_LIST)?1:0)

#define WEED_EVENT_IS_FRAME(event) (get_event_hint(event)==WEED_EVENT_HINT_FRAME?1:0)
#define WEED_EVENT_IS_AUDIO_FRAME(event) ((get_event_hint(event)==WEED_EVENT_HINT_FRAME&&weed_plant_has_leaf(event,"audio_clips"))?1:0)
#define WEED_EVENT_IS_FILTER_INIT(event) (get_event_hint(event)==WEED_EVENT_HINT_FILTER_INIT?1:0)
#define WEED_EVENT_IS_FILTER_DEINIT(event) (get_event_hint(event)==WEED_EVENT_HINT_FILTER_DEINIT?1:0)
#define WEED_EVENT_IS_FILTER_MAP(event) (get_event_hint(event)==WEED_EVENT_HINT_FILTER_MAP?1:0)
#define WEED_EVENT_IS_PARAM_CHANGE(event) (get_event_hint(event)==WEED_EVENT_HINT_PARAM_CHANGE?1:0)
#define WEED_EVENT_IS_MARKER(event) (get_event_hint(event)==WEED_EVENT_HINT_MARKER?1:0)

#endif // HAS_LIVES_EVENTS_H
