// events.c
// LiVES
// (c) G. Finch 2005 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions/structs for event_lists and events

#include "main.h"

#include "effects.h"
#include "interface.h"
#include "callbacks.h"
#include "resample.h"
#include "audio.h"
#include "cvirtual.h"
#include "rfx-builder.h"
#ifdef LIBAV_TRANSCODE
#include "transcode.h"
#endif

//////////////////////////////////
//#define DEBUG_EVENTS

static int render_choice;
static weed_timecode_t last_rec_start_tc = -1;
static void **pchains[FX_KEYS_MAX]; // each pchain is an array of void *, these are parameter changes used for rendering

///////////////////////////////////////////////////////

//lib stuff
LIVES_GLOBAL_INLINE int weed_event_get_type(weed_event_t *event) {
  if (!event || !WEED_PLANT_IS_EVENT(event)) return WEED_EVENT_TYPE_UNDEFINED;
  return get_event_type(event);
}

LIVES_GLOBAL_INLINE int weed_frame_event_get_tracks(weed_event_t *event,  int **clips, frames64_t **frames) {
  int ntracks = 0, xntracks = 0;
  if (!event || !WEED_EVENT_IS_FRAME(event)) return -1;
  if (clips) *clips = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &ntracks);
  else ntracks = weed_leaf_num_elements(event, WEED_LEAF_CLIPS);
  if (frames) *frames = weed_get_int64_array_counted(event, WEED_LEAF_FRAMES, &xntracks);
  else xntracks = weed_leaf_num_elements(event, WEED_LEAF_FRAMES);

  if (ntracks != xntracks && xntracks * ntracks > 0) {
    if (clips) {
      weed_free(*clips);
      *clips = NULL;
    }
    if (frames) {
      weed_free(*frames);
      *frames = NULL;
    }
    return -2;
  }
  if (ntracks != 0) return ntracks;
  return xntracks;
}

LIVES_GLOBAL_INLINE int weed_frame_event_get_audio_tracks(weed_event_t *event,  int **clips, double **seeks) {
  /// number of actual tracks is actually half of the returned value
  int ntracks = 0, xntracks = 0;
  if (!event || !WEED_EVENT_IS_FRAME(event)) return -1;
  if (clips) *clips = weed_get_int_array_counted(event, WEED_LEAF_AUDIO_CLIPS, &ntracks);
  else ntracks = weed_leaf_num_elements(event, WEED_LEAF_AUDIO_CLIPS);
  if (seeks) *seeks = weed_get_double_array_counted(event, WEED_LEAF_AUDIO_SEEKS, &xntracks);
  else xntracks = weed_leaf_num_elements(event, WEED_LEAF_AUDIO_SEEKS);

  if (ntracks != xntracks && xntracks * ntracks > 0) {
    if (clips) {
      weed_free(*clips);
      *clips = NULL;
    }
    if (seeks) {
      weed_free(*seeks);
      *seeks = NULL;
    }
    return -2;
  }
  if (ntracks != 0) return ntracks;
  return xntracks;
}

LIVES_GLOBAL_INLINE weed_error_t weed_event_set_timecode(weed_event_t *event, weed_timecode_t tc) {
  return weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);
}

LIVES_GLOBAL_INLINE weed_timecode_t weed_event_get_timecode(weed_event_t *event) {
  return get_event_timecode(event);
}


GNU_PURE void ** *get_event_pchains(void) {return pchains;}

#define _get_or_zero(a, b, c) (a ? weed_get_##b##_value(a, c, NULL) : 0)

LIVES_GLOBAL_INLINE weed_timecode_t get_event_timecode(weed_plant_t *plant) {
  return _get_or_zero(plant, int64, WEED_LEAF_TIMECODE);
}


LIVES_GLOBAL_INLINE int get_event_type(weed_plant_t *plant) {
  if (!plant) return 0;
  return weed_get_int_value(plant, WEED_LEAF_EVENT_TYPE, NULL);
}


LIVES_GLOBAL_INLINE weed_plant_t *get_prev_event(weed_plant_t *event) {
  return _get_or_zero(event, voidptr, WEED_LEAF_PREVIOUS);
}


LIVES_GLOBAL_INLINE weed_plant_t *get_next_event(weed_plant_t *event) {
  return _get_or_zero(event, voidptr, WEED_LEAF_NEXT);
}


LIVES_GLOBAL_INLINE weed_plant_t *get_first_event(weed_plant_t *event_list) {
  return _get_or_zero(event_list, voidptr, WEED_LEAF_FIRST);
}


LIVES_GLOBAL_INLINE weed_plant_t *get_last_event(weed_plant_t *event_list) {
  return _get_or_zero(event_list, voidptr, WEED_LEAF_LAST);
}


boolean has_frame_event_at(weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t **shortcut) {
  weed_plant_t *event;
  weed_timecode_t ev_tc;

  if (!shortcut || !*shortcut) event = get_first_frame_event(event_list);
  else event = *shortcut;

  while (event && (ev_tc = get_event_timecode(event)) <= tc) {
    if (ev_tc == tc && WEED_EVENT_IS_FRAME(event)) {
      *shortcut = event;
      return TRUE;
    }
    event = get_next_frame_event(event);
  }
  return FALSE;
}


int get_audio_frame_clip(weed_plant_t *event, int track) {
  int numaclips, aclipnum = -1;
  int *aclips;
  int i;

  if (!WEED_EVENT_IS_AUDIO_FRAME(event)) return -2;
  aclips = weed_get_int_array_counted(event, WEED_LEAF_AUDIO_CLIPS, &numaclips);
  for (i = 0; i < numaclips; i += 2) {
    if (aclips[i] == track) {
      aclipnum = aclips[i + 1];
      break;
    }
  }
  lives_freep((void **)&aclips);
  return aclipnum;
}


double get_audio_frame_vel(weed_plant_t *event, int track) {
  // vel of 0. is OFF
  // warning - check for the clip >0 first
  int *aclips = NULL;
  double *aseeks = NULL, avel = 1.;
  int numaclips;

  if (!WEED_EVENT_IS_AUDIO_FRAME(event)) return -2;
  aclips = weed_get_int_array_counted(event, WEED_LEAF_AUDIO_CLIPS, &numaclips);
  aseeks = weed_get_double_array(event, WEED_LEAF_AUDIO_SEEKS, NULL);
  for (int i = 0; i < numaclips; i += 2) {
    if (aclips[i] == track) {
      avel = aseeks[i + 1];
      break;
    }
  }
  lives_freep((void **)&aseeks);
  lives_freep((void **)&aclips);
  return avel;
}


double get_audio_frame_seek(weed_plant_t *event, int track) {
  // warning - check for the clip >0 first
  int *aclips = NULL;
  double *aseeks = NULL, aseek = 0.;
  int numaclips;

  if (!WEED_EVENT_IS_AUDIO_FRAME(event)) return -1000000.;
  numaclips = weed_leaf_num_elements(event, WEED_LEAF_AUDIO_CLIPS);
  aclips = weed_get_int_array_counted(event, WEED_LEAF_AUDIO_CLIPS, &numaclips);
  aseeks = weed_get_double_array(event, WEED_LEAF_AUDIO_SEEKS, NULL);
  for (int i = 0; i < numaclips; i += 2) {
    if (aclips[i] == track) {
      aseek = aseeks[i];
      break;
    }
  }
  lives_freep((void **)&aseeks);
  lives_freep((void **)&aclips);
  return aseek;
}


int get_frame_event_clip(weed_plant_t *event, int layer) {
  int numclips, clipnum;
  int *clips;
  if (!WEED_EVENT_IS_FRAME(event)) return -2;
  clips = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &numclips);
  if (numclips <= layer) {
    lives_freep((void **)&clips);
    return -3;
  }
  clipnum = clips[layer];
  lives_free(clips);
  return clipnum;
}


frames_t get_frame_event_frame(weed_plant_t *event, int layer) {
  int numframes;
  frames_t framenum;
  frames64_t *frames;
  if (!WEED_EVENT_IS_FRAME(event)) return -2;
  frames = weed_get_int64_array_counted(event, WEED_LEAF_FRAMES, &numframes);
  if (numframes <= layer) {
    lives_freep((void **)&frames);
    return -3;
  }
  framenum = (frames_t)frames[layer];
  lives_free(frames);
  return framenum;
}


weed_event_t *lives_event_list_new(weed_event_t *elist, const char *cdate) {
  weed_event_t *evelist;
  weed_error_t error;
  char *xdate = (char *)cdate;
  char *cversion;

  if (elist) evelist = elist;
  else {
    evelist = weed_plant_new(WEED_PLANT_EVENT_LIST);
    if (!evelist) return NULL;
    error = weed_set_int_value(evelist, WEED_LEAF_WEED_EVENT_API_VERSION, WEED_EVENT_API_VERSION);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error = weed_set_voidptr_value(evelist, WEED_LEAF_FIRST, NULL);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error = weed_set_voidptr_value(evelist, WEED_LEAF_LAST, NULL);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }

  if (!weed_plant_has_leaf(evelist, WEED_LEAF_WEED_API_VERSION))
    error = weed_set_int_value(evelist, WEED_LEAF_WEED_API_VERSION, WEED_API_VERSION);
  if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  if (!weed_plant_has_leaf(evelist, WEED_LEAF_FILTER_API_VERSION))
    error = weed_set_int_value(evelist, WEED_LEAF_FILTER_API_VERSION, WEED_FILTER_API_VERSION);
  if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  if (!xdate) xdate = get_current_timestamp();

  cversion = lives_strdup_printf("LiVES version %s", LiVES_VERSION);

  if (!weed_plant_has_leaf(evelist, WEED_LEAF_LIVES_CREATED_VERSION)) {
    error = weed_set_string_value(evelist, WEED_LEAF_LIVES_CREATED_VERSION, cversion);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }
  if (!weed_plant_has_leaf(evelist, LIVES_LEAF_CREATED_DATE)) {
    error = weed_set_string_value(evelist, LIVES_LEAF_CREATED_DATE, xdate);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }

  if (!weed_plant_has_leaf(evelist, WEED_LEAF_LIVES_EDITED_VERSION)) {
    error = weed_set_string_value(evelist, WEED_LEAF_LIVES_EDITED_VERSION, cversion);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }
  if (!weed_plant_has_leaf(evelist, LIVES_LEAF_EDITED_DATE)) {
    error = weed_set_string_value(evelist, LIVES_LEAF_EDITED_DATE, xdate);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }

  if (xdate != cdate) lives_free(xdate);
  lives_free(cversion);
  return evelist;
}


void unlink_event(weed_plant_t *event_list, weed_plant_t *event) {
  // lives_rm event from event_list
  // don't forget to adjust "timecode" before re-inserting !
  weed_plant_t *prev_event = get_prev_event(event);
  weed_plant_t *next_event = get_next_event(event);

  if (prev_event) weed_set_voidptr_value(prev_event, WEED_LEAF_NEXT, next_event);
  if (next_event) weed_set_voidptr_value(next_event, WEED_LEAF_PREVIOUS, prev_event);

  if (get_first_event(event_list) == event) weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, next_event);
  if (get_last_event(event_list) == event) weed_set_voidptr_value(event_list, WEED_LEAF_LAST, prev_event);
}


void delete_event(weed_plant_t *event_list, weed_plant_t *event) {
  // delete event from event_list
  threaded_dialog_spin(0.);
  unlink_event(event_list, event);
  if (mainw->multitrack) mt_fixup_events(mainw->multitrack, event, NULL);
  weed_plant_free(event);
  threaded_dialog_spin(0.);
}


boolean insert_event_before(weed_plant_t *at_event, weed_plant_t *event) {
  // insert event before at_event : returns FALSE if event is new start of event list
  weed_plant_t *xevent = get_prev_event(at_event);
  if (xevent) weed_set_voidptr_value(xevent, WEED_LEAF_NEXT, event);
  weed_set_voidptr_value(event, WEED_LEAF_NEXT, at_event);
  weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, xevent);
  weed_set_voidptr_value(at_event, WEED_LEAF_PREVIOUS, event);
  if (get_event_timecode(event) > get_event_timecode(at_event))
    lives_printerr("Warning ! Inserted out of order event type %d before %d\n", get_event_type(event), get_event_type(at_event));
  return (xevent != NULL);
}


boolean insert_event_after(weed_plant_t *at_event, weed_plant_t *event) {
  // insert event after at_event : returns FALSE if event is new end of event list
  weed_plant_t *xevent = get_next_event(at_event);
  if (xevent) weed_set_voidptr_value(xevent, WEED_LEAF_PREVIOUS, event);
  weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, at_event);
  weed_set_voidptr_value(event, WEED_LEAF_NEXT, xevent);
  weed_set_voidptr_value(at_event, WEED_LEAF_NEXT, event);
  if (get_event_timecode(event) < get_event_timecode(at_event))
    lives_printerr("Warning ! Inserted out of order event type %d after %d\n", get_event_type(event), get_event_type(at_event));
  return (xevent != NULL);
}


void replace_event(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event) {
  // replace at_event with event; free at_event
  if (mainw->multitrack) mt_fixup_events(mainw->multitrack, at_event, event);
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, get_event_timecode(at_event));
  if (!insert_event_after(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
  delete_event(event_list, at_event);
}


weed_plant_t *get_next_frame_event(weed_plant_t *event) {
  weed_plant_t *next;
  if (!event) return NULL;
  next = get_next_event(event);
  while (next) {
    if (WEED_EVENT_IS_FRAME(next)) return next;
    next = get_next_event(next);
  }
  return NULL;
}


weed_plant_t *get_prev_frame_event(weed_plant_t *event) {
  weed_plant_t *prev;
  if (!event) return NULL;
  prev = get_prev_event(event);
  while (prev) {
    if (WEED_EVENT_IS_FRAME(prev)) return prev;
    prev = get_prev_event(prev);
  }
  return NULL;
}


weed_plant_t *get_next_audio_frame_event(weed_plant_t *event) {
  weed_plant_t *next;
  if (!event) return NULL;
  next = get_next_event(event);
  while (next) {
    if (WEED_EVENT_IS_AUDIO_FRAME(next)) return next;
    next = get_next_event(next);
  }
  return NULL;
}


weed_plant_t *get_prev_audio_frame_event(weed_plant_t *event) {
  weed_plant_t *prev;
  if (!event) return NULL;
  prev = get_prev_event(event);
  while (prev) {
    if (WEED_EVENT_IS_AUDIO_FRAME(prev)) return prev;
    prev = get_prev_event(prev);
  }
  return NULL;
}


weed_plant_t *get_first_frame_event(weed_plant_t *event_list) {
  weed_plant_t *event;

  if (!event_list) return NULL;

  event = get_first_event(event_list);

  while (event) {
    if (WEED_EVENT_IS_FRAME(event)) return event;
    event = get_next_event(event);
  }
  return NULL;
}


weed_plant_t *get_last_frame_event(weed_plant_t *event_list) {
  weed_plant_t *event;

  if (!event_list) return NULL;

  event = get_last_event(event_list);

  while (event) {
    if (WEED_EVENT_IS_FRAME(event)) return event;
    event = get_prev_event(event);
  }
  return NULL;
}


weed_plant_t *get_audio_block_start(weed_plant_t *event_list, int track, weed_timecode_t tc, boolean seek_back) {
  // find any event which starts an audio block on track at timecode tc
  // if seek_back is true we go back in time to find a possible start
  // otherwise just check the current frame event

  weed_plant_t *event = get_frame_event_at_or_before(event_list, tc, NULL);
  if (get_audio_frame_clip(event, track) > -1 && get_audio_frame_vel(event, track) != 0.) return event;
  if (!seek_back) return NULL;

  while ((event = get_prev_frame_event(event)) != NULL) {
    if (get_audio_frame_clip(event, track) > -1 && get_audio_frame_vel(event, track) != 0.) return event;
  }

  return NULL;
}

static LiVESList *trans_list = NULL;

typedef struct {
  weed_event_t *in_event;
  weed_event_t *out_event;
} trans_entry;

static void add_init_to_ttable(weed_event_t *in_event, weed_event_t *out_event) {
  trans_entry *tr_entry = (trans_entry *)lives_malloc(sizeof(trans_entry));
  tr_entry->in_event = in_event;
  tr_entry->out_event = out_event;
  trans_list = lives_list_prepend(trans_list, tr_entry);
}


static weed_event_t *find_init_event_by_id(weed_plant_t *event, boolean remove) {
  LiVESList *list = trans_list;
  for (; list; list = list->next) {
    trans_entry *tr_entry = (trans_entry *)list->data;
    if (tr_entry->in_event == event) {
      weed_event_t *out_event = tr_entry->out_event;
      if (remove) trans_list = lives_list_remove_node(trans_list, list, TRUE);
      return out_event;
    }
  }
  return NULL;
}

void reset_ttable(void) {lives_list_free_all(&trans_list);}


void **append_to_easing_events(void **eevents, int *nev, weed_event_t *init_event) {
  weed_event_t *out_event = find_init_event_by_id(init_event, FALSE);
  for (int i = 0; i < *nev; i++) {
    if (eevents[i] == out_event) return eevents;
  }
  (*nev)++;
  eevents = lives_realloc(eevents, *nev * sizeof(void *));
  eevents[*nev - 1] = out_event;
  return eevents;
}


void remove_frame_from_event(weed_plant_t *event_list, weed_plant_t *event, int track) {
  // TODO - memcheck
  weed_timecode_t tc;

  int *clips;
  frames64_t *frames;

  frames_t numframes;
  int i;

  if (!WEED_EVENT_IS_FRAME(event)) return;

  tc = get_event_timecode(event);

  clips = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &numframes);
  frames = weed_get_int64_array(event, WEED_LEAF_FRAMES, NULL);

  if (track == numframes - 1) numframes--;
  else {
    clips[track] = -1;
    frames[track] = 0;
  }

  // if stack is empty, we will replace with a blank frame
  for (i = 0; i < numframes && clips[i] < 1; i++);
  if (i == numframes) {
    if (event == get_last_event(event_list) && !WEED_EVENT_IS_AUDIO_FRAME(event)) delete_event(event_list, event);
    else event_list = insert_blank_frame_event_at(event_list, tc, &event);
  } else event_list = insert_frame_event_at(event_list, tc, numframes, clips, frames, &event);
  lives_free(frames);
  lives_free(clips);
}


boolean is_blank_frame(weed_plant_t *event, boolean count_audio) {
  int clip, numframes;
  frames64_t frame;

  if (!WEED_EVENT_IS_FRAME(event)) return FALSE;
  if (count_audio && WEED_EVENT_IS_AUDIO_FRAME(event)) {
    int *aclips = weed_get_int_array(event, WEED_LEAF_AUDIO_CLIPS, NULL);
    if (aclips[1] > 0) {
      lives_free(aclips);
      return FALSE;   // has audio seek
    }
    lives_free(aclips);
  }
  numframes = weed_leaf_num_elements(event, WEED_LEAF_CLIPS);
  if (numframes > 1) return FALSE;
  clip = weed_get_int_value(event, WEED_LEAF_CLIPS, NULL);
  frame = weed_get_int64_value(event, WEED_LEAF_FRAMES, NULL);

  if (clip < 0 || frame <= 0) return TRUE;
  return FALSE;
}


void remove_end_blank_frames(weed_plant_t *event_list, boolean remove_filter_inits) {
  // remove blank frames from end of event list
  weed_plant_t *event = get_last_event(event_list), *prevevent;
  while (event) {
    prevevent = get_prev_event(event);
    if (!WEED_EVENT_IS_FRAME(event) && !WEED_EVENT_IS_FILTER_INIT(event)) {
      event = prevevent;
      continue;
    }
    if (remove_filter_inits && WEED_EVENT_IS_FILTER_INIT(event)) remove_filter_from_event_list(event_list, event);
    else {
      if (!is_blank_frame(event, TRUE)) break;
      delete_event(event_list, event);
    }
    event = prevevent;
  }
}


weed_timecode_t get_next_paramchange(void **pchange_next, weed_timecode_t end_tc) {
  weed_timecode_t min_tc = end_tc;
  register int i = 0;
  if (!pchange_next) return end_tc;
  for (; pchange_next[i]; i++) if (get_event_timecode((weed_plant_t *)pchange_next[i]) < min_tc)
      min_tc = get_event_timecode((weed_plant_t *)pchange_next[i]);
  return min_tc;
}


weed_timecode_t get_prev_paramchange(void **pchange_prev, weed_timecode_t start_tc) {
  weed_timecode_t min_tc = start_tc;
  register int i = 0;
  if (!pchange_prev) return start_tc;
  for (; pchange_prev[i]; i++) if (get_event_timecode((weed_plant_t *)pchange_prev[i]) < min_tc)
      min_tc = get_event_timecode((weed_plant_t *)pchange_prev[i]);
  return min_tc;
}


boolean is_init_pchange(weed_plant_t *init_event, weed_plant_t *pchange_event) {
  // a PARAM_CHANGE is an init_pchange iff both events have the same tc, and there is no frame event between the two events
  // normally we could check the "in_params" of the init_event for a match, but here we may be rebuilding the event list
  // so the values will not confer
  weed_plant_t *event = init_event;
  weed_timecode_t tc = get_event_timecode(event);
  if (tc != get_event_timecode(pchange_event)) return FALSE;

  while (event && event != pchange_event) {
    if (WEED_EVENT_IS_FRAME(event)) return FALSE;
    event = get_next_event(event);
  }
  return TRUE;
}


/**
   @brief copy (duplicate) in_event and append it to event_list, changing the timecode to out_tc
   this is called during quantisation

   copy an event and insert it in event_list
   events must be copied in time order, since filter_deinit,
   filter_map and param_change events MUST refer to prior filter_init events

   when we copy a filter_init, we add a new field to the copy "event_id".
   This contains the value of the pointer to original event
   we use this to locate the effect_init event in the new event_list

   for effect_deinit, effect_map, param_change, we change the "init_event(s)" property to point to our copy effect_init

   we don't need to make pchain array here, provided we later call event_list_rectify()
   (this only applies to multitrack, since we interpolate parameters there; in clip editor the parameter changes are applied
   as recorded, with no interpolation)

   we check for memory allocation errors here, because we could be building a large new event_list
   on mem error we return NULL, caller should free() event_list in that case
*/
weed_plant_t *event_copy_and_insert(weed_plant_t *in_event, weed_timecode_t out_tc, weed_plant_t *event_list,
                                    weed_event_t **ret_event) {
  void **in_pchanges;

  weed_plant_t *event;
  weed_plant_t *event_after = NULL;
  static weed_plant_t *event_before = NULL;
  weed_plant_t *filter;

  void *init_event, *new_init_event, **init_events;
  char *filter_hash;

  weed_error_t error;

  int etype;
  int num_events;
  int idx, num_params;

  int i;

  if (!in_event) return event_list;

  if (!event_list) {
    event_list = lives_event_list_new(NULL, NULL);
    if (!event_list) return NULL;
    event_before = NULL;
  } else {
    if (event_before)
      while (event_before && get_event_timecode(event_before) <= out_tc) event_before = get_next_event(event_before);
    if (!event_before) event_before = get_last_event(event_list);

    while (event_before) {
      weed_timecode_t tc = get_event_timecode(event_before);
      if (tc < out_tc || (tc == out_tc && (!WEED_EVENT_IS_FRAME(event_before) ||
                                           WEED_EVENT_IS_FILTER_DEINIT(in_event)))) break;
      event_before = get_prev_event(event_before);
    }
  }

  event = weed_plant_copy(in_event);
  weed_event_set_timecode(event, out_tc);

  // need to repoint our avol_init_event
  if (mainw->multitrack) mt_fixup_events(mainw->multitrack, in_event, event);
  if (!event) return NULL;

  if (!event_before) {
    event_after = get_first_event(event_list);
    error = weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    if (event_after == event) event_after = NULL;
  } else {
    event_after = get_next_event(event_before);
    error = weed_set_voidptr_value(event_before, WEED_LEAF_NEXT, event);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }

  error = weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, event_before);
  if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  error = weed_set_voidptr_value(event, WEED_LEAF_NEXT, event_after);
  if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  if (!event_after) error = weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
  else error = weed_set_voidptr_value(event_after, WEED_LEAF_PREVIOUS, event);
  if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  etype = get_event_type(in_event);
  switch (etype) {
  case WEED_EVENT_TYPE_FILTER_INIT:
    /* weed_leaf_delete(event, WEED_LEAF_EVENT_ID); */
    /* error = weed_set_voidptr_value(event, WEED_LEAF_EVENT_ID, (void *)in_event); */
    /* if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL; */
    add_init_to_ttable(in_event, event);
    filter_hash = weed_get_string_value(event, WEED_LEAF_FILTER, &error);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    if ((idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
      filter = get_weed_filter(idx);
      if ((num_params = num_in_params(filter, FALSE, FALSE)) > 0) {
        in_pchanges = (void **)lives_malloc((num_params + 1) * sizeof(void *));
        if (!in_pchanges) return NULL;
        for (i = 0; i < num_params; i++) in_pchanges[i] = NULL;
        error = weed_set_voidptr_array(event, WEED_LEAF_IN_PARAMETERS, num_params,
                                       in_pchanges); // set all to NULL, we will re-fill as we go along
        lives_free(in_pchanges);
        if (error == WEED_ERROR_MEMORY_ALLOCATION) {
          lives_free(filter_hash);
          return NULL;
        }
      }
      lives_free(filter_hash);
    }
    break;
  case WEED_EVENT_TYPE_FILTER_DEINIT:
    init_event = weed_get_voidptr_value(in_event, WEED_LEAF_INIT_EVENT, NULL);
    new_init_event = find_init_event_by_id(init_event, TRUE);
    error = weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENT, new_init_event);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    /* weed_leaf_delete((weed_plant_t *)new_init_event, WEED_LEAF_EVENT_ID); */
    /* error = weed_set_voidptr_value((weed_plant_t *)new_init_event, WEED_LEAF_EVENT_ID, */
    /*                                (void *)new_init_event);  // useful later for event_list_rectify */
    weed_leaf_delete((weed_plant_t *)new_init_event,
                     WEED_LEAF_DEINIT_EVENT); // delete since we assign a placeholder with int64 type
    weed_set_plantptr_value((weed_plant_t *)new_init_event, WEED_LEAF_DEINIT_EVENT, event);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    break;
  case WEED_EVENT_TYPE_FILTER_MAP:
    // set WEED_LEAF_INIT_EVENTS property
    init_events = weed_get_voidptr_array_counted(in_event, WEED_LEAF_INIT_EVENTS, &num_events);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    for (i = 0; i < num_events; i++) {
      init_events[i] = find_init_event_by_id(init_events[i], FALSE);
    }
    error = weed_set_voidptr_array(event, WEED_LEAF_INIT_EVENTS, num_events, init_events);
    lives_free(init_events);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    /*
      // remove any prior FILTER_MAPs at the same timecode
      event_before = get_prev_event(event);
      while (event_before) {
      weed_plant_t *event_before_event_before = get_prev_event(event_before);
      weed_timecode_t tc = get_event_timecode(event_before);
      if (tc < out_tc) break;
      if (tc == out_tc && (WEED_EVENT_IS_FILTER_MAP(event_before))) delete_event(event_list, event_before);
      event_before = event_before_event_before;
      }*/
    break;
  case WEED_EVENT_TYPE_PARAM_CHANGE:
    init_event = weed_get_voidptr_value(in_event, WEED_LEAF_INIT_EVENT, &error);
    new_init_event = find_init_event_by_id(init_event, FALSE);
    error = weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENT, new_init_event);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    weed_set_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, NULL);
    weed_set_voidptr_value(event, WEED_LEAF_PREV_CHANGE, NULL);
    break;
  default:
    break;
  }

  if (ret_event) *ret_event = event;
  return event_list;
}


boolean frame_event_has_frame_for_track(weed_plant_t *event, int track) {
  int *clips, numclips;
  frames64_t *frames;

  clips = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &numclips);
  if (numclips <= track) return FALSE;
  frames = weed_get_int64_array(event, WEED_LEAF_FRAMES, NULL);

  if (clips[track] > 0 && frames[track] > 0) {
    lives_free(clips);
    lives_free(frames);
    return TRUE;
  }
  lives_free(clips);
  lives_free(frames);
  return FALSE;
}


weed_plant_t *get_frame_event_at(weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t *shortcut, boolean exact) {
  // if exact is FALSE, we can get a frame event just after tc
  weed_plant_t *event, *next_event;
  weed_timecode_t xtc, next_tc = 0;

  if (!event_list) return NULL;
  if (shortcut) event = shortcut;
  else event = get_first_frame_event(event_list);
  while (event) {
    next_event = get_next_event(event);
    if (next_event) next_tc = get_event_timecode(next_event);
    xtc = get_event_timecode(event);
    if ((labs(tc - xtc) <= 10 || ((next_tc > tc || !next_event) && !exact)) &&
        WEED_EVENT_IS_FRAME(event)) {
      return event;
    }
    if (xtc > tc) return NULL;
    event = next_event;
  }
  return NULL;
}


boolean filter_map_after_frame(weed_plant_t *fmap) {
  // return TRUE if filter_map follows frame at same timecode
  weed_plant_t *frame = get_prev_frame_event(fmap);

  if (frame && get_event_timecode(frame) == get_event_timecode(fmap)) return TRUE;
  return FALSE;
}


weed_plant_t *get_frame_event_at_or_before(weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t *shortcut) {
  weed_plant_t *frame_event = get_frame_event_at(event_list, tc, shortcut, FALSE);
  while (frame_event && get_event_timecode(frame_event) > tc) {
    frame_event = get_prev_frame_event(frame_event);
  }
  return frame_event;
}


weed_event_t *get_filter_map_after(weed_event_t *event, int ctrack) {
  // get filter_map following event; if ctrack!=LIVES_TRACK_ANY then we ignore filter maps with no in_track/out_track == ctrack
  void **init_events;
  weed_event_t *init_event;
  int num_init_events;

  while (event) {
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      if (ctrack == LIVES_TRACK_ANY) return event;
      if (!weed_plant_has_leaf(event, WEED_LEAF_INIT_EVENTS)) {
        event = get_next_event(event);
        continue;
      }
      init_events = weed_get_voidptr_array_counted(event, WEED_LEAF_INIT_EVENTS, &num_init_events);
      if (!init_events[0]) {
        lives_free(init_events);
        event = get_next_event(event);
        continue;
      }
      for (int i = 0; i < num_init_events; i++) {
        init_event = (weed_event_t *)init_events[i];

        if (init_event_is_relevant(init_event, ctrack)) {
          lives_free(init_events);
          return event;
        }

      }
      lives_freep((void **)&init_events);
    }
    event = get_next_event(event);
  }
  return NULL;
}


boolean init_event_is_relevant(weed_plant_t *init_event, int ctrack) {
  // see if init_event mentions ctrack as an in_track or an out_track

  int *in_tracks, *out_tracks;
  int num_tracks;
  int j;

  //if (init_event_is_process_last(init_event)) return FALSE;

  if (weed_plant_has_leaf(init_event, WEED_LEAF_IN_TRACKS)) {
    in_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &num_tracks);
    for (j = 0; j < num_tracks; j++) {
      if (in_tracks[j] == ctrack) {
        lives_free(in_tracks);
        return TRUE;
      }
    }
    lives_freep((void **)&in_tracks);
  }

  if (weed_plant_has_leaf(init_event, WEED_LEAF_OUT_TRACKS)) {
    out_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_OUT_TRACKS, &num_tracks);
    for (j = 0; j < num_tracks; j++) {
      if (out_tracks[j] == ctrack) {
        lives_free(out_tracks);
        return TRUE;
      }
    }
    lives_freep((void **)&out_tracks);
  }

  return FALSE;
}


weed_event_t *get_filter_map_before(weed_event_t *event, int ctrack, weed_event_t *stop_event) {
  // get filter_map preceding event; if ctrack!=LIVES_TRACK_ANY then we ignore
  // filter maps with no in_track/out_track == ctrack

  // we will stop searching when we reach stop_event; if it is NULL we will search back to
  // start of event list

  void **init_events;
  weed_event_t *init_event;
  int num_init_events;

  while (event != stop_event && event) {
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      if (ctrack == LIVES_TRACK_ANY) return event;
      if (!weed_plant_has_leaf(event, WEED_LEAF_INIT_EVENTS)) {
        event = get_prev_event(event);
        continue;
      }
      init_events = weed_get_voidptr_array_counted(event, WEED_LEAF_INIT_EVENTS, &num_init_events);
      if (!num_init_events || !init_events[0]) {
        lives_freep((void **)&init_events);
        event = get_prev_event(event);
        continue;
      }
      for (int i = 0; i < num_init_events; i++) {
        init_event = (weed_event_t *)init_events[i];
        if (init_event_is_relevant(init_event, ctrack)) {
          lives_free(init_events);
          return event;
        }
      }
      lives_freep((void **)&init_events);
    }
    event = get_prev_event(event);
  }
  return event;
}


weed_event_t **get_init_events_before(weed_event_t *event, weed_event_t *init_event, boolean add) {
  // find previous FILTER_MAP event, and append or delete new init_event
  weed_event_t **init_events = NULL, **new_init_events;
  int error, num_init_events = 0;
  register int i, j = 0;

  while (event) {
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      if ((init_events =
             (weed_event_t **)weed_get_voidptr_array_counted(event, WEED_LEAF_INIT_EVENTS, &num_init_events)) != NULL) {
        if (add) new_init_events = (weed_event_t **)lives_malloc((num_init_events + 2) * sizeof(weed_event_t *));
        else new_init_events = (weed_event_t **)lives_malloc((num_init_events + 1) * sizeof(weed_event_t *));

        for (i = 0; i < num_init_events; i++) if ((add || (init_event && (init_events[i] != init_event))) &&
              init_events[0]) {
            new_init_events[j++] = init_events[i];
            if (add && init_events[i] == init_event) add = FALSE; // don't add twice
          }

        if (add) {
          char *fhash;
          weed_plant_t *filter;
          int k, l, tflags;
          // add before any "process_last" events
          k = j;
          while (k > 0) {
            k--;
            if (mainw->multitrack && init_events[k] == mainw->multitrack->avol_init_event) {
              // add before the audio mixer
              continue;
            }
            fhash = weed_get_string_value(init_events[k], WEED_LEAF_FILTER, &error);
            filter = get_weed_filter(weed_get_idx_for_hashname(fhash, TRUE));
            lives_free(fhash);
            if (weed_plant_has_leaf(filter, WEED_LEAF_FLAGS)) {
              tflags = weed_get_int_value(filter, WEED_LEAF_FLAGS, &error);
              if (tflags & WEED_FILTER_HINT_PROCESS_LAST) {
                // add before any "process_last" filters
                continue;
              }
            }
            k++;
            break;
          }
          // insert new event at slot k
          // make gap for new filter
          for (l = j - 1; l >= k; l--) {
            new_init_events[l + 1] = new_init_events[l];
          }
          new_init_events[k] = init_event;
          j++;
        }

        new_init_events[j] = NULL;
        if (init_events) lives_free(init_events);
        return new_init_events;
      }
      if (init_events) lives_free(init_events);
    }
    event = get_prev_event(event);
  }
  // no previous init_events found
  if (add) {
    new_init_events = (weed_event_t **)lives_malloc(2 * sizeof(weed_event_t *));
    new_init_events[0] = init_event;
    new_init_events[1] = NULL;
  } else {
    new_init_events = (weed_event_t **)lives_malloc(sizeof(weed_event_t *));
    new_init_events[0] = NULL;
  }
  return new_init_events;
}


void update_filter_maps(weed_event_t *event, weed_event_t *end_event, weed_event_t **init_events, int ninits) {
  // append init_event(s) to all FILTER_MAPS between event and end_event

  while (event && event != end_event) {
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      for (int i = 0; i < ninits; i++) {
        add_init_event_to_filter_map(event, init_events[i], NULL);
      }
    }
    event = get_next_event(event);
  }
}


void insert_filter_init_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event) {
  // insert event as first event at same timecode as (FRAME_EVENT) at_event
  weed_timecode_t tc = get_event_timecode(at_event);
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);

  while (at_event) {
    at_event = get_prev_event(at_event);
    if (!at_event) break;
    if (get_event_timecode(at_event) < tc) {
      if (!insert_event_after(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);

      return;
    }
  }

  // event is first
  at_event = get_first_event(event_list);
  insert_event_before(at_event, event);
  weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
}


void insert_filter_deinit_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event) {
  // insert event as last at same timecode as (FRAME_EVENT) at_event
  weed_timecode_t tc = get_event_timecode(at_event);
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);

  while (at_event) {
    if (WEED_EVENT_IS_FRAME(at_event)) {
      if (!insert_event_after(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
      return;
    }
    if (get_event_timecode(at_event) > tc) {
      if (!insert_event_before(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
      return;
    }
    at_event = get_next_event(at_event);
  }
  // event is last
  at_event = get_last_event(event_list);
  insert_event_after(at_event, event);
  weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
}


boolean insert_filter_map_event_at(weed_plant_t *event_list, weed_plant_t *at_event,
                                   weed_plant_t *event, boolean before_frames) {
  // insert event as last event at same timecode as (FRAME_EVENT) at_event
  weed_timecode_t tc = get_event_timecode(at_event);
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);

  if (before_frames) {
    while (at_event) {
      at_event = get_prev_event(at_event);
      if (!at_event) break;
      if (WEED_EVENT_IS_FILTER_MAP(at_event)) {
        // found an existing FILTER_MAP, we can simply replace it
        if (mainw->filter_map == at_event) mainw->filter_map = event;
        replace_event(event_list, at_event, event);
        return TRUE;
      }
      if (WEED_EVENT_IS_FILTER_INIT(at_event) || get_event_timecode(at_event) < tc) {
        if (!insert_event_after(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
        return TRUE;
      }
    }
    // event is first
    at_event = get_first_event(event_list);
    insert_event_before(at_event, event);
    weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
  } else {
    // insert after frame events
    while (at_event) {
      at_event = get_next_event(at_event);
      if (!at_event) break;
      if (WEED_EVENT_IS_FILTER_MAP(at_event)) {
        // found an existing FILTER_MAP, we can simply replace it
        if (mainw->filter_map == at_event) mainw->filter_map = event;
        replace_event(event_list, at_event, event);
        return TRUE;
      }
      if (get_event_timecode(at_event) > tc) {
        if (!insert_event_before(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
        return TRUE;
      }
    }
    // event is last
    at_event = get_last_event(event_list);
    insert_event_after(at_event, event);
    weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
  }
  return TRUE;
}


void insert_param_change_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event) {
  // insert event as last at same timecode as (FRAME_EVENT) at_event, before FRAME event
  weed_timecode_t tc = get_event_timecode(at_event);
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);

  //weed_add_plant_flags(event, WEED_LEAF_READONLY_PLUGIN); // protect it for interpolation

  while (at_event) {
    if (get_event_timecode(at_event) < tc) {
      if (!insert_event_after(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
      return;
    }
    if (WEED_EVENT_IS_FILTER_INIT(at_event)) {
      if (!insert_event_after(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
      return;
    }
    if (WEED_EVENT_IS_FRAME(at_event)) {
      if (!insert_event_before(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
      return;
    }
    at_event = get_prev_event(at_event);
  }
  at_event = get_first_event(event_list);
  insert_event_before(at_event, event);
  weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
}


weed_plant_t *insert_frame_event_at(weed_plant_t *event_list, weed_timecode_t tc, int numframes, int *clips,
                                    frames64_t *frames, weed_plant_t **shortcut) {
  // we will insert a FRAME event at timecode tc, after any other events (except for deinit events) at timecode tc
  // if there is an existing FRAME event at tc, we replace it with the new frame

  // shortcut can be a nearest guess of where the frame should be

  // returns NULL on memory error

  weed_plant_t *event = NULL, *new_event, *prev;
  weed_plant_t *new_event_list, *xevent_list;
  weed_timecode_t xtc;
  weed_error_t error;

  if (!event_list || !get_first_frame_event(event_list)) {
    // no existing event list, or no frames,  append
    event_list = append_frame_event(event_list, tc, numframes, clips, frames);
    if (!event_list) return NULL; // memory error
    if (shortcut) *shortcut = get_last_event(event_list);
    return event_list;
  }

  // skip the next part if we know we have to add at end
  if (tc <= get_event_timecode(get_last_event(event_list))) {
    if (shortcut && *shortcut) {
      event = *shortcut;
    } else event = get_first_event(event_list);

    if (get_event_timecode(event) > tc) {
      // step backwards until we get to a frame before where we want to add
      while (event && get_event_timecode(event) > tc) event = get_prev_frame_event(event);
      // event can come out NULL (add before first frame event), in which case we fall through
    } else {
      while (event && get_event_timecode(event) < tc) event = get_next_frame_event(event);

      // we reached the end, so we will add after last frame event
      if (!event) event = get_last_frame_event(event_list);
    }

    while (event && (((xtc = get_event_timecode(event)) < tc) || (xtc == tc && (!WEED_EVENT_IS_FILTER_DEINIT(event))))) {
      if (shortcut) *shortcut = event;
      if (xtc == tc && WEED_EVENT_IS_FRAME(event)) {
        error = weed_set_int_array(event, WEED_LEAF_CLIPS, numframes, clips);
        if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
        error = weed_set_int64_array(event, WEED_LEAF_FRAMES, numframes, frames);
        if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
        return event_list;
      }
      event = get_next_event(event);
    }

    // we passed all events in event_list; there was one or more at tc, but none were deinits or frames
    if (!event) {
      event = get_last_event(event_list);
      // event is after last event, append it
      if (!(xevent_list = append_frame_event(event_list, tc, numframes, clips, frames))) return NULL;
      event_list = xevent_list;
      if (shortcut) *shortcut = get_last_event(event_list);
      return event_list;
    }
  } else {
    // event is after last event, append it
    if (!(xevent_list = append_frame_event(event_list, tc, numframes, clips, frames))) return NULL;
    event_list = xevent_list;
    if (shortcut) *shortcut = get_last_event(event_list);
    return event_list;
  }

  // add frame before "event"

  if (!(new_event_list = append_frame_event(NULL, tc, numframes, clips, frames))) return NULL;
  // new_event_list is now an event_list with one frame event. We will steal its event and prepend it !

  new_event = get_first_event(new_event_list);

  prev = get_prev_event(event);

  if (prev) {
    error = weed_set_voidptr_value(prev, WEED_LEAF_NEXT, new_event);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }
  error = weed_set_voidptr_value(new_event, WEED_LEAF_PREVIOUS, prev);
  if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  error = weed_set_voidptr_value(new_event, WEED_LEAF_NEXT, event);
  if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  error = weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, new_event);
  if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  if (get_first_event(event_list) == event) {
    error = weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, new_event);
    if (error == WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }

  weed_plant_free(new_event_list);

  if (shortcut) *shortcut = new_event;
  return event_list;
}


void insert_audio_event_at(weed_plant_t *event, int track, int clipnum, double seek, double vel) {
  // insert/update audio event at (existing) frame event
  int *new_aclips;
  double *new_aseeks;
  double arv; // vel needs rounding to four dp (i don't know why, but otherwise we get some weird rounding errors)

  arv = (double)(myround(vel * 10000.)) / 10000.;

  if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
    int *aclips = NULL, i;
    double *aseeks = NULL;
    int num_aclips = weed_frame_event_get_audio_tracks(event, &aclips, &aseeks);

    for (i = 0; i < num_aclips; i += 2) {
      if (aclips[i] == track) {
        if (clipnum <= 0) {
          if (num_aclips <= 2) {
            weed_leaf_delete(event, WEED_LEAF_AUDIO_CLIPS);
            weed_leaf_delete(event, WEED_LEAF_AUDIO_SEEKS);
            lives_freep((void **)&aseeks);
            lives_freep((void **)&aclips);
            return;
          } else {
            int *new_aclips = (int *)lives_malloc((num_aclips - 2) * sizint);
            double *new_aseeks = (double *)lives_malloc((num_aclips - 2) * sizdbl);
            int j, k = 0;
            for (j = 0; j < num_aclips; j += 2) {
              if (j != i) {
                new_aclips[k] = aclips[j];
                new_aclips[k + 1] = aclips[j + 1];
                new_aseeks[k] = aseeks[j];
                new_aseeks[k + 1] = aseeks[j + 1];
                k += 2;
              }
            }

            weed_set_int_array(event, WEED_LEAF_AUDIO_CLIPS, num_aclips - 2, new_aclips);
            weed_set_double_array(event, WEED_LEAF_AUDIO_SEEKS, num_aclips - 2, new_aseeks);
            lives_free(new_aclips);
            lives_free(new_aseeks);
            lives_freep((void **)&aseeks);
            lives_freep((void **)&aclips);
            return;
          }
        }

        // update existing values
        aclips[i + 1] = clipnum;
        aseeks[i] = seek;
        aseeks[i + 1] = arv;

        weed_set_int_array(event, WEED_LEAF_AUDIO_CLIPS, num_aclips, aclips);
        weed_set_double_array(event, WEED_LEAF_AUDIO_SEEKS, num_aclips, aseeks);
        lives_freep((void **)&aseeks);
        lives_freep((void **)&aclips);
        return;
      }
    }

    if (clipnum <= 0) {
      lives_freep((void **)&aseeks);
      lives_freep((void **)&aclips);
      return;
    }

    // append
    new_aclips = (int *)lives_malloc((num_aclips + 2) * sizint);
    for (i = 0; i < num_aclips; i++) new_aclips[i] = aclips[i];
    new_aclips[i++] = track;
    new_aclips[i] = clipnum;

    new_aseeks = (double *)lives_malloc((num_aclips + 2) * sizdbl);
    for (i = 0; i < num_aclips; i++) new_aseeks[i] = aseeks[i];
    new_aseeks[i++] = seek;
    new_aseeks[i++] = arv;

    weed_set_int_array(event, WEED_LEAF_AUDIO_CLIPS, i, new_aclips);
    weed_set_double_array(event, WEED_LEAF_AUDIO_SEEKS, i, new_aseeks);

    lives_free(new_aclips);
    lives_free(new_aseeks);

    lives_freep((void **)&aseeks);
    lives_freep((void **)&aclips);
    return;
  }
  // create new values

  new_aclips = (int *)lives_malloc(2 * sizint);
  new_aclips[0] = track;
  new_aclips[1] = clipnum;

  new_aseeks = (double *)lives_malloc(2 * sizdbl);
  new_aseeks[0] = seek;
  new_aseeks[1] = arv;

  weed_set_int_array(event, WEED_LEAF_AUDIO_CLIPS, 2, new_aclips);
  weed_set_double_array(event, WEED_LEAF_AUDIO_SEEKS, 2, new_aseeks);

  lives_free(new_aclips);
  lives_free(new_aseeks);
}


void remove_audio_for_track(weed_plant_t *event, int track) {
  // delete audio for a FRAME_EVENT with audio for specified track
  // if nothing left, delete the audio leaves
  int num_atracks;
  int *aclip_index = weed_get_int_array_counted(event, WEED_LEAF_AUDIO_CLIPS, &num_atracks);
  double *aseek_index = weed_get_double_array(event, WEED_LEAF_AUDIO_SEEKS, NULL);
  int *new_aclip_index = (int *)lives_malloc(num_atracks * sizint);
  double *new_aseek_index = (double *)lives_malloc(num_atracks * sizdbl);

  int j = 0;

  for (int i = 0; i < num_atracks; i += 2) {
    if (aclip_index[i] == track) continue;
    new_aclip_index[j] = aclip_index[i];
    new_aclip_index[j + 1] = aclip_index[i + 1];
    new_aseek_index[j] = aseek_index[i];
    new_aseek_index[j + 1] = aseek_index[i + 1];
    j += 2;
  }
  if (j == 0) {
    weed_leaf_delete(event, WEED_LEAF_AUDIO_CLIPS);
    weed_leaf_delete(event, WEED_LEAF_AUDIO_SEEKS);
  } else {
    weed_set_int_array(event, WEED_LEAF_AUDIO_CLIPS, j, new_aclip_index);
    weed_set_double_array(event, WEED_LEAF_AUDIO_SEEKS, j, new_aseek_index);
  }
  lives_free(aclip_index);
  lives_free(aseek_index);
  lives_free(new_aclip_index);
  lives_free(new_aseek_index);
}


static weed_error_t set_event_ptrs(weed_event_list_t *event_list, weed_event_t *event) {
  weed_event_t *prev;
  weed_error_t error;

  if (!get_first_event(event_list)) {
    error = weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
    if (error != WEED_SUCCESS) return error;
    error = weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, NULL);
    if (error != WEED_SUCCESS) return error;
  } else {
    error = weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, get_last_event(event_list));
    if (error != WEED_SUCCESS) return error;
  }
  //weed_add_plant_flags(event, WEED_LEAF_READONLY_PLUGIN);
  prev = get_prev_event(event);
  if (prev) {
    error = weed_set_voidptr_value(prev, WEED_LEAF_NEXT, event);
    if (error != WEED_SUCCESS) return error;
  }
  error = weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
  return error;
}


weed_event_list_t *append_marker_event(weed_event_list_t *event_list,
                                       weed_timecode_t tc, int marker_type) {
  weed_event_t *event;

  if (!event_list) {
    event_list = lives_event_list_new(NULL, NULL);
    if (!event_list) return NULL;
  }

  event = weed_plant_new(WEED_PLANT_EVENT);
  weed_set_voidptr_value(event, WEED_LEAF_NEXT, NULL);

  // TODO - error check
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);
  weed_set_int_value(event, WEED_LEAF_EVENT_TYPE, WEED_EVENT_TYPE_MARKER);

  weed_set_int_value(event, WEED_LEAF_LIVES_TYPE, marker_type);

#ifdef DEBUG_EVENTS
  g_print("adding marker event %p at tc %"PRId64"\n", event, tc);
#endif

  if (set_event_ptrs(event_list, event) != WEED_SUCCESS) return NULL;

  return event_list;
}


weed_plant_t *insert_marker_event_at(weed_plant_t *event_list, weed_plant_t *at_event, int marker_type, livespointer data) {
  // insert marker event as first event at same timecode as (FRAME_EVENT) at_event
  weed_timecode_t tc = get_event_timecode(at_event);
  weed_plant_t *event = weed_plant_new(WEED_PLANT_EVENT);
  register int i;

  weed_set_int_value(event, WEED_LEAF_EVENT_TYPE, WEED_EVENT_TYPE_MARKER);
  weed_set_int_value(event, WEED_LEAF_LIVES_TYPE, marker_type);
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);

  if (marker_type == EVENT_MARKER_BLOCK_START || marker_type == EVENT_MARKER_BLOCK_UNORDERED) {
    weed_set_int_value(event, WEED_LEAF_TRACKS, LIVES_POINTER_TO_INT(data));
  }
  //weed_add_plant_flags(event, WEED_LEAF_READONLY_PLUGIN);

  while (at_event) {
    at_event = get_prev_event(at_event);
    if (!at_event) break;
    switch (marker_type) {
    case EVENT_MARKER_BLOCK_START:
    case EVENT_MARKER_BLOCK_UNORDERED:
      if (WEED_EVENT_IS_MARKER(at_event) && (weed_get_int_value(at_event, WEED_LEAF_LIVES_TYPE, NULL) == marker_type)) {
        // add to existing event
        int num_tracks;
        int *tracks = weed_get_int_array_counted(at_event, WEED_LEAF_TRACKS, &num_tracks);
        int *new_tracks = (int *)lives_malloc((num_tracks + 1) * sizint);
        for (i = 0; i < num_tracks; i++) {
          new_tracks[i] = tracks[i];
        }
        new_tracks[i] = LIVES_POINTER_TO_INT(data); // add new track
        weed_set_int_array(at_event, WEED_LEAF_TRACKS, num_tracks + 1, new_tracks);
        lives_free(new_tracks);
        lives_free(tracks);
        weed_plant_free(event); // new event not used
        return event;
      }
      if (get_event_timecode(at_event) < tc) {
        // create new event
        if (!insert_event_after(at_event, event)) weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
        return event;
      }
      break;
    }
  }

  // event is first
  at_event = get_first_event(event_list);
  insert_event_before(at_event, event);
  weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);

  return event;
}


LIVES_GLOBAL_INLINE
weed_event_list_t *insert_blank_frame_event_at(weed_event_list_t *event_list, weed_timecode_t tc,
    weed_event_t **shortcut) {
  int clip = -1;
  frames64_t frame = 0;
  return insert_frame_event_at(event_list, tc, 1, &clip, &frame, shortcut);
}


void remove_filter_from_event_list(weed_plant_t *event_list, weed_plant_t *init_event) {
  int error;
  weed_plant_t *deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, &error);
  weed_plant_t *event = init_event;
  weed_plant_t *filter_map = get_filter_map_before(init_event, LIVES_TRACK_ANY, NULL);
  weed_event_t **new_init_events;
  weed_timecode_t deinit_tc = get_event_timecode(deinit_event);
  weed_plant_t *event_next;

  int i;

  while (event && get_event_timecode(event) <= deinit_tc) {
    event_next = get_next_event(event);
    // update filter_maps
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      new_init_events = get_init_events_before(event, init_event, FALSE);
      for (i = 0; new_init_events[i]; i++);
      if (i == 0) weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENTS, NULL);
      else weed_set_voidptr_array(event, WEED_LEAF_INIT_EVENTS, i, (void **)new_init_events);
      lives_free(new_init_events);

      if ((!filter_map && i == 0) || (filter_map && compare_filter_maps(filter_map, event, LIVES_TRACK_ANY)))
        delete_event(event_list, event);
      else filter_map = event;
    }
    event = event_next;
  }

  // remove param_changes
  if (weed_plant_has_leaf(init_event, WEED_LEAF_IN_PARAMETERS)) {
    void *pchain_next;
    int num_params;
    void **pchain = weed_get_voidptr_array_counted(init_event, WEED_LEAF_IN_PARAMETERS, &num_params);
    for (i = 0; i < num_params; i++) {
      while (pchain[i]) {
        pchain_next = weed_get_voidptr_value((weed_plant_t *)pchain[i], WEED_LEAF_NEXT_CHANGE, NULL);
        delete_event(event_list, (weed_plant_t *)pchain[i]);
        pchain[i] = pchain_next;
      }
    }
    lives_free(pchain);
  }

  delete_event(event_list, init_event);
  delete_event(event_list, deinit_event);
}


static boolean remove_event_from_filter_map(weed_plant_t *fmap, weed_plant_t *event) {
  // return FALSE if result is NULL filter_map
  int num_inits;
  void **init_events = weed_get_voidptr_array_counted(fmap, WEED_LEAF_INIT_EVENTS, &num_inits);
  void **new_init_events;

  int i, j = 0;

  new_init_events = (void **)lives_malloc(num_inits * sizeof(void *));
  for (i = 0; i < num_inits; i++) {
    if (init_events[i] != event) new_init_events[j++] = init_events[i];
  }

  if (j == 0 || (j == 1 && (!event || !init_events[0]))) weed_set_voidptr_value(fmap, WEED_LEAF_INIT_EVENTS, NULL);
  else weed_set_voidptr_array(fmap, WEED_LEAF_INIT_EVENTS, j, new_init_events);
  if (init_events) lives_free(init_events);
  if (new_init_events) lives_free(new_init_events);

  return (!(j == 0 || (j == 1 && !event)));
}


LIVES_GLOBAL_INLINE boolean init_event_in_list(void **init_events, int num_inits, weed_plant_t *event) {
  if (!init_events || !init_events[0]) return FALSE;
  for (int i = 0; i < num_inits; i++) {
    if (init_events[i] == (void **)event) return TRUE;
  }
  return FALSE;
}


boolean filter_map_has_event(weed_plant_t *fmap, weed_plant_t *event) {
  int num_inits;
  void **init_events = weed_get_voidptr_array_counted(fmap, WEED_LEAF_INIT_EVENTS, &num_inits);
  boolean ret = init_event_in_list(init_events, num_inits, event);

  lives_free(init_events);
  return ret;
}


boolean filter_init_has_owner(weed_plant_t *init_event, int track) {
  int num_owners;
  int *owners = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &num_owners);
  if (!num_owners) return FALSE;

  for (int i = 0; i < num_owners; i++) {
    if (owners[i] == track) {
      lives_free(owners);
      return TRUE;
    }
  }
  lives_free(owners);
  return FALSE;
}


void backup_host_tags(weed_plant_t *event_list, weed_timecode_t curr_tc) {
  // when redrawing the current frame during rendering (in multitrack mode)
  // host keys will change (see backup_weed_instances() in effects-weed.c)
  // here we backup the host_tag (which maps a filter_init to a "key" and thus to an instance)

  weed_plant_t *event = get_first_event(event_list);
  weed_timecode_t tc;

  while (event && (tc = get_event_timecode(event)) <= curr_tc) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) weed_leaf_copy(event, WEED_LEAF_HOST_TAG_COPY, event, WEED_LEAF_HOST_TAG);
    event = get_next_event(event);
  }
}


void restore_host_tags(weed_plant_t *event_list, weed_timecode_t curr_tc) {
  // when redrawing the current frame during rendering (in multitrack mode)
  // host keys will change (see backup_weed_instances() in effects-weed.c)
  // here we restore the host_tag (which maps a filter_init to a "key" and thus to an instance)

  weed_plant_t *event = get_first_event(event_list);
  weed_timecode_t tc;

  while (event && (tc = get_event_timecode(event)) <= curr_tc) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      weed_leaf_copy(event, WEED_LEAF_HOST_TAG, event, WEED_LEAF_HOST_TAG_COPY);
      weed_leaf_delete(event, WEED_LEAF_HOST_TAG_COPY);
    }
    event = get_next_event(event);
  }
}


void delete_param_changes_after_deinit(weed_plant_t *event_list, weed_plant_t *init_event) {
  // delete parameter value changes following the filter_deinit
  // this can be called when a FILTER_DEINIT is moved
  void **init_events;
  weed_plant_t *deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, NULL);
  weed_timecode_t deinit_tc = get_event_timecode(deinit_event);
  weed_timecode_t pchain_tc;

  void *pchain, *pchain_next;

  int i, num_inits;

  if (!weed_plant_has_leaf(init_event, WEED_LEAF_IN_PARAMETERS)) return;

  init_events = weed_get_voidptr_array_counted(init_event, WEED_LEAF_IN_PARAMETERS, &num_inits);

  for (i = 0; i < num_inits; i++) {
    pchain = init_events[i];
    while (pchain) {
      pchain_tc = get_event_timecode((weed_plant_t *)pchain);
      if (!weed_plant_has_leaf((weed_plant_t *)pchain, WEED_LEAF_NEXT_CHANGE)) pchain_next = NULL;
      else pchain_next = weed_get_voidptr_value((weed_plant_t *)pchain, WEED_LEAF_NEXT_CHANGE, NULL);
      if (pchain_tc > deinit_tc) delete_event(event_list, (weed_plant_t *)pchain);
      pchain = pchain_next;
    }
  }
  lives_free(init_events);
}


void rescale_param_changes(weed_event_list_t *event_list, weed_event_t *init_event,
                           weed_timecode_t new_init_tc,
                           weed_event_t *deinit_event, weed_timecode_t new_deinit_tc, double fps) {
  // rescale parameter value changes along the time axis
  // this can be called when a FILTER_INIT or FILTER_DEINIT is moved
  // fps is used for quantisation; may be 0. for no quant.

  void **init_events;

  weed_timecode_t old_init_tc = get_event_timecode(init_event);
  weed_timecode_t old_deinit_tc = get_event_timecode(deinit_event);
  weed_timecode_t pchain_tc, new_tc;

  void *pchain;
  weed_plant_t *event;

  int num_inits, i;

  if (!weed_plant_has_leaf(init_event, WEED_LEAF_IN_PARAMETERS)) return;

  init_events = weed_get_voidptr_array_counted(init_event, WEED_LEAF_IN_PARAMETERS, &num_inits);

  if (!init_events) num_inits = 0;

  for (i = 0; i < num_inits; i++) {
    pchain = init_events[i];
    while (pchain) {
      pchain_tc = get_event_timecode((weed_plant_t *)pchain);
      new_tc = (weed_timecode_t)((double)(pchain_tc - old_init_tc) / (double)(old_deinit_tc - old_init_tc) *
                                 (double)(new_deinit_tc - new_init_tc)) + new_init_tc;
      if (new_tc < 0) break;
      if (fps > 0.) new_tc = q_gint64(new_tc, fps);
      if (new_tc == pchain_tc) {
        if (!weed_plant_has_leaf((weed_plant_t *)pchain, WEED_LEAF_NEXT_CHANGE)) pchain = NULL;
        else pchain = weed_get_voidptr_value((weed_plant_t *)pchain, WEED_LEAF_NEXT_CHANGE, NULL);
        continue;
      }
      event = (weed_plant_t *)pchain;
      if (new_tc < pchain_tc) {
        while (event && get_event_timecode(event) > new_tc) event = get_prev_event(event);
      } else {
        while (event && get_event_timecode(event) < new_tc) event = get_next_event(event);
      }

      if (event) {
        unlink_event(event_list, (weed_plant_t *)pchain);
        insert_param_change_event_at(event_list, event, (weed_plant_t *)pchain);
      }

      if (!weed_plant_has_leaf((weed_plant_t *)pchain, WEED_LEAF_NEXT_CHANGE)) pchain = NULL;
      else pchain = weed_get_voidptr_value((weed_plant_t *)pchain, WEED_LEAF_NEXT_CHANGE, NULL);
    }
  }

  if (init_events) lives_free(init_events);
}


static boolean is_in_hints(weed_plant_t *event, void **hints) {
  register int i;
  if (!hints) return FALSE;
  for (i = 0; hints[i]; i++) {
    if (hints[i] == event) return TRUE;
  }
  return FALSE;
}


boolean init_event_is_process_last(weed_plant_t *event) {
  weed_plant_t *filter;
  char *hashname;

  if (!event) return FALSE;

  hashname = weed_get_string_value(event, WEED_LEAF_FILTER, NULL);
  filter = get_weed_filter(weed_get_idx_for_hashname(hashname, TRUE));
  lives_free(hashname);
  return weed_filter_is_process_last(filter);
}


void add_init_event_to_filter_map(weed_plant_t *fmap, weed_plant_t *event, void **hints) {
  // TODO - try to add at same position as in hints ***

  // init_events are the events we are adding to
  // event is what we are adding

  // hints is the init_events from the previous filter_map

  void **init_events, **new_init_events = NULL;

  boolean added = FALSE, plast = FALSE, mustadd = FALSE;
  int num_inits, i, j = 0;

  remove_event_from_filter_map(fmap, event);

  init_events = weed_get_voidptr_array_counted(fmap, WEED_LEAF_INIT_EVENTS, &num_inits);

  if (num_inits <= 1 && (!init_events || !init_events[0])) {
    weed_set_voidptr_value(fmap, WEED_LEAF_INIT_EVENTS, event);
    if (init_events) lives_free(init_events);
    return;
  }

  if (init_event_is_process_last(event)) plast = TRUE;

  new_init_events = (void **)lives_calloc((num_inits + 1), sizeof(void *));

  for (i = 0; i < num_inits; i++) {
    mustadd = FALSE;
    if (!added && init_event_is_process_last((weed_plant_t *)init_events[i])) mustadd = TRUE;

    if (mustadd || (!plast && !added && is_in_hints((weed_plant_t *)init_events[i], hints))) {
      new_init_events[j++] = event;
      added = TRUE;
    }
    if (init_events[i] == event) {
      if (!added) {
        added = TRUE;
        new_init_events[j++] = event;
      }
    } else {
      new_init_events[j++] = init_events[i];
    }
  }
  if (!added) {
    new_init_events[j++] = event;
  }

  weed_set_voidptr_array(fmap, WEED_LEAF_INIT_EVENTS, j, new_init_events);
  lives_freep((void **)&init_events);
  lives_free(new_init_events);
}


void move_filter_init_event(weed_plant_t *event_list, weed_timecode_t new_tc, weed_plant_t *init_event, double fps) {
  int error, i, j = 0;
  weed_timecode_t tc = get_event_timecode(init_event);
  weed_plant_t *deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, &error);
  weed_timecode_t deinit_tc = get_event_timecode(deinit_event);
  weed_plant_t *event = init_event, *event_next;
  weed_plant_t *filter_map, *copy_filter_map;
  weed_event_t **init_events;
  int num_inits;
  void **event_types = NULL;
  boolean is_null_filter_map;

  rescale_param_changes(event_list, init_event, new_tc, deinit_event, deinit_tc, fps);

  if (new_tc > tc) {
    // moving right
    filter_map = get_filter_map_before(event, LIVES_TRACK_ANY, NULL);
    while (get_event_timecode(event) < new_tc) {
      event_next = get_next_event(event);
      if (WEED_EVENT_IS_FILTER_MAP(event)) {
        is_null_filter_map = !remove_event_from_filter_map(event, init_event);
        if ((!filter_map && is_null_filter_map) || (filter_map &&
            compare_filter_maps(filter_map, event, LIVES_TRACK_ANY)))
          delete_event(event_list, event);
        else filter_map = event;
      }
      event = event_next;
    }
    unlink_event(event_list, init_event);
    insert_filter_init_event_at(event_list, event, init_event);

    event = get_next_frame_event(init_event);

    init_events = get_init_events_before(event, init_event, TRUE);
    event_list = append_filter_map_event(event_list, new_tc, (weed_event_t **)init_events);
    lives_free(init_events);

    filter_map = get_last_event(event_list);
    unlink_event(event_list, filter_map);
    insert_filter_map_event_at(event_list, event, filter_map, TRUE);
  } else {
    // moving left
    // see if event is switched on at start
    boolean is_on = FALSE;
    boolean adding = FALSE;
    while (event != deinit_event) {
      if (get_event_timecode(event) > tc) break;
      if (WEED_EVENT_IS_FILTER_MAP(event) && filter_map_has_event(event, init_event)) {
        if (weed_plant_has_leaf(event, WEED_LEAF_INIT_EVENTS)) {
          init_events = (weed_event_t **)weed_get_voidptr_array_counted(event, WEED_LEAF_INIT_EVENTS, &num_inits);
          if (init_events[0]) {
            event_types = (void **)lives_malloc((num_inits + 1) * sizeof(void *));
            for (i = 0; i < num_inits; i++) {
              if (adding) event_types[j++] = init_events[i];
              if (init_events[i] == init_event) adding = TRUE;
            }
            event_types[j] = NULL;
            is_on = TRUE;
          }
          lives_free(init_events);
        }
        break;
      }
      event = get_next_event(event);
    }
    event = init_event;
    while (get_event_timecode(event) > new_tc) event = get_prev_event(event);
    unlink_event(event_list, init_event);
    insert_filter_init_event_at(event_list, event, init_event);

    if (is_on) {
      event = get_next_frame_event(init_event);
      filter_map = get_filter_map_before(event, LIVES_TRACK_ANY, NULL);

      // insert filter_map at new filter_init
      if (filter_map) {
        copy_filter_map = weed_plant_copy(filter_map);
        add_init_event_to_filter_map(copy_filter_map, init_event, event_types);
        filter_map = copy_filter_map;
      } else {
        init_events = (weed_event_t **)lives_malloc(2 * sizeof(weed_event_t *));
        init_events[0] = init_event;
        init_events[1] = NULL;
        event_list = append_filter_map_event(event_list, new_tc, init_events);
        lives_free(init_events);
        filter_map = get_last_event(event_list);
        unlink_event(event_list, filter_map);
      }

      insert_filter_map_event_at(event_list, event, filter_map, TRUE);
      event = get_next_event(filter_map);

      // ensure filter remains on until repositioned FILTER_INIT
      while (event && get_event_timecode(event) <= tc) {
        event_next = get_next_event(event);
        if (WEED_EVENT_IS_FILTER_MAP(event)) {
          add_init_event_to_filter_map(filter_map, init_event, event_types);
          if (compare_filter_maps(filter_map, event, LIVES_TRACK_ANY)) delete_event(event_list, event);
          else filter_map = event;
        }
        event = event_next;
      }
      if (event_types) lives_free(event_types);
    }
  }
}


void move_filter_deinit_event(weed_plant_t *event_list, weed_timecode_t new_tc, weed_plant_t *deinit_event,
                              double fps, boolean rescale_pchanges) {
  // move a filter_deinit from old pos to new pos, remove mention of it from filter maps,
  // possibly add/update filter map before frame at new_tc, remove duplicate filter_maps, update param_change events
  int error, i, j = 0;
  weed_timecode_t tc = get_event_timecode(deinit_event);
  weed_plant_t *init_event = (weed_plant_t *)weed_get_voidptr_value(deinit_event, WEED_LEAF_INIT_EVENT, &error);
  weed_timecode_t init_tc = get_event_timecode(init_event);
  weed_plant_t *event = deinit_event, *event_next;
  weed_plant_t *filter_map, *copy_filter_map;
  weed_plant_t *xevent;
  void **init_events;
  int num_inits;
  void **event_types = NULL;
  boolean is_null_filter_map;

  if (new_tc == tc) return;

  if (rescale_pchanges) rescale_param_changes(event_list, init_event, init_tc, deinit_event, new_tc, fps);

  if (new_tc < tc) {
    // moving left
    //find last event at new_tc, we are going to insert deinit_event after this

    // first find filter_map before new end position, copy it with filter removed
    while (get_event_timecode(event) > new_tc) event = get_prev_event(event);
    filter_map = get_filter_map_before(event, LIVES_TRACK_ANY, NULL);
    if (filter_map) {
      if (get_event_timecode(filter_map) != get_event_timecode(event)) {
        copy_filter_map = weed_plant_copy(filter_map);
        if (!WEED_EVENT_IS_FRAME(event)) event = get_prev_frame_event(event);
        if (!event) event = get_first_event(event_list);
        insert_filter_map_event_at(event_list, event, copy_filter_map, FALSE);
      } else copy_filter_map = filter_map;
      remove_event_from_filter_map(copy_filter_map, init_event);
      if (filter_map != copy_filter_map && compare_filter_maps(filter_map, copy_filter_map, LIVES_TRACK_ANY))
        delete_event(event_list, copy_filter_map);
      else filter_map = copy_filter_map;
    }

    while (!WEED_EVENT_IS_FRAME(event)) event = get_prev_event(event);
    xevent = event;
    filter_map = get_filter_map_before(event, LIVES_TRACK_ANY, NULL);

    // remove from following filter_maps

    while (event && get_event_timecode(event) <= tc) {
      event_next = get_next_event(event);
      if (WEED_EVENT_IS_FILTER_MAP(event)) {
        // found a filter map, so remove the event from it
        is_null_filter_map = !remove_event_from_filter_map(event, init_event);
        if ((!filter_map && is_null_filter_map) || (filter_map &&
            compare_filter_maps(filter_map, event, LIVES_TRACK_ANY)))
          delete_event(event_list, event);
        else filter_map = event;
      }
      event = event_next;
    }
    unlink_event(event_list, deinit_event);
    insert_filter_deinit_event_at(event_list, xevent, deinit_event);
    if (!rescale_pchanges) delete_param_changes_after_deinit(event_list, init_event);
  } else {
    // moving right
    // see if event is switched on at end
    boolean is_on = FALSE;
    boolean adding = FALSE;

    xevent = get_prev_event(deinit_event);

    // get event_types so we can add filter back at guess position
    filter_map = get_filter_map_before(deinit_event, LIVES_TRACK_ANY, NULL);
    if (filter_map && filter_map_has_event(filter_map, init_event)) {
      init_events = weed_get_voidptr_array_counted(filter_map, WEED_LEAF_INIT_EVENTS, &num_inits);
      event_types = (void **)lives_malloc((num_inits + 1) * sizeof(void *));
      for (i = 0; i < num_inits; i++) {
        if (adding) {
          event_types[j++] = init_events[i];
        }
        if (init_events[i] == init_event) adding = TRUE;
      }
      event_types[j] = NULL;
      is_on = TRUE;
      lives_free(init_events);
    }

    // move deinit event
    event = deinit_event;
    while (event && get_event_timecode(event) < new_tc) event = get_next_event(event);

    unlink_event(event_list, deinit_event);

    if (!event) return;

    insert_filter_deinit_event_at(event_list, event, deinit_event);

    if (is_on) {
      // ensure filter remains on until new position
      event = xevent;
      while (event != deinit_event) {
        if (get_event_timecode(event) == new_tc && WEED_EVENT_IS_FRAME(event)) break;
        event_next = get_next_event(event);
        if (WEED_EVENT_IS_FILTER_MAP(event)) {
          add_init_event_to_filter_map(event, init_event, event_types);
          if (compare_filter_maps(filter_map, event, LIVES_TRACK_ANY)) delete_event(event_list, event);
          else filter_map = event;
        }
        event = event_next;
      }
      if (event_types) lives_free(event_types);

      // find last FILTER_MAP before deinit_event
      event = deinit_event;
      while (event && get_event_timecode(event) == new_tc) event = get_next_event(event);
      if (!event) event = get_last_event(event_list);
      filter_map = get_filter_map_before(event, LIVES_TRACK_ANY, NULL);

      if (filter_map && filter_map_has_event(filter_map, init_event)) {
        // if last FILTER_MAP before deinit_event mentions init_event, remove init_event,
        // insert FILTER_MAP after deinit_event
        copy_filter_map = weed_plant_copy(filter_map);

        remove_event_from_filter_map(copy_filter_map, init_event);
        insert_filter_map_event_at(event_list, deinit_event, copy_filter_map, FALSE);
        event = get_next_event(copy_filter_map);
        while (event) {
          // remove next FILTER_MAP if it is a duplicate
          if (WEED_EVENT_IS_FILTER_MAP(event)) {
            if (compare_filter_maps(copy_filter_map, event, LIVES_TRACK_ANY)) delete_event(event_list, event);
            break;
          }
          event = get_next_event(event);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
}


boolean move_event_right(weed_plant_t *event_list, weed_plant_t *event, boolean can_stay, double fps) {
  // move a filter_init or param_change to the right
  // this can happen for two reasons: - we are rectifying an event_list, or a block was deleted or moved

  // if can_stay is FALSE, event is forced to move. This is used only during event list rectification.

  weed_timecode_t tc = get_event_timecode(event), new_tc = tc;
  weed_plant_t *xevent = event;

  int *owners = NULL;

  boolean all_ok = FALSE;

  int num_owners = 0, num_clips, i;

  if (WEED_EVENT_IS_FILTER_INIT(event)) {
    owners = weed_get_int_array_counted(event, WEED_LEAF_IN_TRACKS, &num_owners);
  } else if (!WEED_EVENT_IS_PARAM_CHANGE(event)) return TRUE;

  if (num_owners > 0) {
    while (xevent) {
      if (WEED_EVENT_IS_FRAME(xevent)) {
        if ((new_tc = get_event_timecode(xevent)) > tc || (can_stay && new_tc == tc)) {
          all_ok = TRUE;
          num_clips = weed_leaf_num_elements(xevent, WEED_LEAF_CLIPS);
          // find timecode of next event which has valid frames at all owner tracks
          for (i = 0; i < num_owners; i++) {
            if (owners[i] < 0) continue; // ignore audio owners
            if (num_clips <= owners[i] || get_frame_event_clip(xevent, owners[i]) < 0 || get_frame_event_frame(xevent, owners[i]) < 1) {
              all_ok = FALSE;
              break; // blank frame, or not enough frames
            }
          }
          if (all_ok) break;
        }
      }
      xevent = get_next_event(xevent);
    }
    lives_free(owners);
  } else {
    if (can_stay) return TRUE; // bound to timeline, and allowed to stay
    xevent = get_next_frame_event(event); // bound to timeline, move to next frame event
    new_tc = get_event_timecode(xevent);
  }

  if (can_stay && (new_tc == tc) && all_ok) return TRUE;

  // now we have xevent, new_tc

  if (WEED_EVENT_IS_FILTER_INIT(event)) {
    weed_plant_t *deinit_event = weed_get_plantptr_value(event, WEED_LEAF_DEINIT_EVENT, NULL);
    if (!xevent || get_event_timecode(deinit_event) < new_tc) {
      // if we are moving a filter_init past its deinit event, remove it, remove deinit, remove param_change events,
      // remove from all filter_maps, and check for duplicate filter maps
      remove_filter_from_event_list(event_list, event);
      return FALSE;
    }
    move_filter_init_event(event_list, new_tc, event, fps);
  } else {
    // otherwise, for a param_change, just insert it at new_tc
    weed_plant_t *init_event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);
    weed_plant_t *deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, NULL);
    if (!xevent || get_event_timecode(deinit_event) < new_tc) {
      delete_event(event_list, event);
      return FALSE;
    }
    unlink_event(event_list, event);
    insert_param_change_event_at(event_list, xevent, event);
  }
  return FALSE;
}


boolean move_event_left(weed_plant_t *event_list, weed_plant_t *event, boolean can_stay, double fps) {
  // move a filter_deinit to the left
  // this can happen for two reasons: - we are rectifying an event_list, or a block was deleted or moved

  // if can_stay is FALSE, event is forced to move. This is used only during event list rectification.

  weed_timecode_t tc = get_event_timecode(event), new_tc = tc;
  weed_plant_t *xevent = event;
  weed_plant_t *init_event;

  int *owners;

  boolean all_ok = FALSE;

  int num_owners = 0, num_clips, i;

  if (WEED_EVENT_IS_FILTER_DEINIT(event))
    init_event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);
  else return TRUE;

  owners = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &num_owners);

  if (num_owners > 0) {
    while (xevent) {
      if (WEED_EVENT_IS_FRAME(xevent)) {
        if ((new_tc = get_event_timecode(xevent)) < tc || (can_stay && new_tc == tc)) {
          all_ok = TRUE;
          // find timecode of previous event which has valid frames at all owner tracks
          for (i = 0; i < num_owners; i++) {
            if (owners[i] < 0) continue; // ignore audio owners
            num_clips = weed_leaf_num_elements(xevent, WEED_LEAF_CLIPS);
            if (num_clips <= owners[i] || get_frame_event_clip(xevent, owners[i]) < 0 || get_frame_event_frame(xevent, owners[i]) < 1) {
              all_ok = FALSE;
              break; // blank frame
            }
          }
          if (all_ok) break;
        }
      }
      xevent = get_prev_event(xevent);
    }
    lives_freep((void **)&owners);
  } else {
    if (can_stay) return TRUE; // bound to timeline, and allowed to stay
    while (xevent) {
      // bound to timeline, just move to previous tc
      if ((new_tc = get_event_timecode(xevent)) < tc) break;
      xevent = get_prev_event(xevent);
    }
  }
  // now we have new_tc

  if (can_stay && (new_tc == tc) && all_ok) return TRUE;

  if (get_event_timecode(init_event) > new_tc) {
    // if we are moving a filter_deinit past its init event, remove it, remove init, remove param_change events,
    // remove from all filter_maps, and check for duplicate filter maps
    remove_filter_from_event_list(event_list, init_event);
    return FALSE;
  }

  // otherwise, go from old pos to new pos, remove mention of it from filter maps, possibly add/update filter map as last event at new_tc,
  // remove duplicate filter_maps, update param_change events

  move_filter_deinit_event(event_list, new_tc, event, fps, TRUE);

  return FALSE;
}


//////////////////////////////////////////////////////
// rendering

void set_render_choice(LiVESToggleButton * togglebutton, livespointer choice) {
  if (lives_toggle_button_get_active(togglebutton)) render_choice = LIVES_POINTER_TO_INT(choice);
}


void set_render_choice_button(LiVESButton * button, livespointer choice) {
  render_choice = LIVES_POINTER_TO_INT(choice);
}


LiVESWidget *events_rec_dialog(void) {
  LiVESWidget *e_rec_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *radiobutton;
  LiVESWidget *okbutton;
  LiVESWidget *cancelbutton;
  LiVESSList *radiobutton_group = NULL;

  render_choice = RENDER_CHOICE_PREVIEW;

  e_rec_dialog = lives_standard_dialog_new(_("Events Recorded"), FALSE, -1, -1);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(e_rec_dialog));

  vbox = lives_vbox_new(FALSE, widget_opts.packing_height * 4);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, TRUE, 0);

  label = lives_standard_label_new(_("Events were recorded. What would you like to do with them ?"));

  lives_box_pack_start(LIVES_BOX(vbox), label, TRUE, TRUE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  radiobutton = lives_standard_radio_button_new(_("_Preview events"), &radiobutton_group, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(set_render_choice),
                            LIVES_INT_TO_POINTER(RENDER_CHOICE_PREVIEW));

  if (!mainw->clip_switched && CURRENT_CLIP_IS_NORMAL && !mainw->recording_recovered
      && (last_rec_start_tc == -1
          || (double)last_rec_start_tc / TICKS_PER_SECOND_DBL
          < (cfile->frames - 1.) / cfile->fps)) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

    radiobutton = lives_standard_radio_button_new(_("Render events to _same clip"), &radiobutton_group, LIVES_BOX(hbox), NULL);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(set_render_choice),
                              LIVES_INT_TO_POINTER(RENDER_CHOICE_SAME_CLIP));
  }

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  radiobutton = lives_standard_radio_button_new(_("Render events to _new clip"), &radiobutton_group, LIVES_BOX(hbox), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(set_render_choice),
                            LIVES_INT_TO_POINTER(RENDER_CHOICE_NEW_CLIP));

#ifdef LIBAV_TRANSCODE
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  radiobutton = lives_standard_radio_button_new(_("Quick transcode to video clip"), &radiobutton_group, LIVES_BOX(hbox), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(set_render_choice),
                            LIVES_INT_TO_POINTER(RENDER_CHOICE_TRANSCODE));
#endif

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  radiobutton = lives_standard_radio_button_new(_("View/edit events in _multitrack window (test)"), &radiobutton_group,
                LIVES_BOX(hbox), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(set_render_choice),
                            LIVES_INT_TO_POINTER(RENDER_CHOICE_MULTITRACK));

  if (mainw->stored_event_list) lives_widget_set_no_show_all(hbox, TRUE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  radiobutton = lives_standard_radio_button_new(_("View/edit events in _event window"), &radiobutton_group, LIVES_BOX(hbox),
                NULL);

  add_fill_to_box(LIVES_BOX(vbox));

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(set_render_choice),
                            LIVES_INT_TO_POINTER(RENDER_CHOICE_EVENT_LIST));

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(e_rec_dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(set_render_choice_button),
                            LIVES_INT_TO_POINTER(RENDER_CHOICE_DISCARD));

  lives_window_add_escape(LIVES_WINDOW(e_rec_dialog), cancelbutton);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(e_rec_dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  return e_rec_dialog;
}


static void event_list_free_events(weed_plant_t *event_list) {
  weed_plant_t *event, *next_event;
  event = get_first_event(event_list);

  while (event) {
    next_event = get_next_event(event);
    if (mainw->multitrack && event_list == mainw->multitrack->event_list) mt_fixup_events(mainw->multitrack, event, NULL);
    weed_plant_free(event);
    event = next_event;
  }
}


void event_list_free(weed_plant_t *event_list) {
  if (!event_list) return;
  event_list_free_events(event_list);
  weed_plant_free(event_list);
}


void event_list_replace_events(weed_plant_t *event_list, weed_plant_t *new_event_list) {
  if (!event_list || !new_event_list) return;
  if (event_list == new_event_list) return;
  event_list_free_events(event_list);
  weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, get_first_event(new_event_list));
  weed_set_voidptr_value(event_list, WEED_LEAF_LAST, get_last_event(new_event_list));
}


boolean event_list_to_block(weed_plant_t *event_list, int num_events) {
  // translate our new event_list to older event blocks
  // by now we should have eliminated clip switches and param settings

  // first we count the frame events
  weed_plant_t *event;
  char *what;
  LiVESResponseType response;
  int i = 0;

  if (!event_list) return TRUE;
  what = (_("memory for the reordering operation"));
  // then we create event_frames
  do {
    response = LIVES_RESPONSE_OK;
    if (!create_event_space(num_events)) {
      response = do_memory_error_dialog(what, num_events * sizeof(resample_event));
    }
  } while (response == LIVES_RESPONSE_RETRY);
  lives_free(what);
  if (response == LIVES_RESPONSE_CANCEL) {
    return FALSE;
  }

  event = get_first_event(event_list);

  while (event) {
    if (WEED_EVENT_IS_FRAME(event)) {
      cfile->resample_events[i++].value = (int)weed_get_int64_value(event, WEED_LEAF_FRAMES, NULL);
    }
    event = get_next_event(event);
  }
  return TRUE;
}


static weed_event_t *check_noq_filter_maps(weed_event_t *event, weed_event_t *filter_map) {
  static weed_event_t *old_fmap = NULL;
  if (!event) {
    // reset
    old_fmap = filter_map;
    return old_fmap;
  }
  if (WEED_EVENT_IS_FILTER_MAP(event) || WEED_EVENT_IS_FILTER_DEINIT(event)) {
    if (filter_map) {
      int ninits;
      weed_event_t *fmap = filter_map;
      // merge events from previous audio filter map into video filter maps
      // stopping at current event
      weed_event_t **init_events = (weed_event_t **)weed_get_voidptr_array_counted
                                   (filter_map, WEED_LEAF_INIT_EVENTS, &ninits);
      if (init_events) {
        update_filter_maps(filter_map, event, init_events, ninits);
        lives_free(init_events);
      }
      // merge events from prior video filter map into audio filter map
      if (!old_fmap) {
        // list has not been checked yet, last map could be anywhere
        while (fmap) {
          fmap = get_prev_event(fmap);
          if (WEED_EVENT_IS_FILTER_MAP(fmap)
              && weed_get_boolean_value(fmap, LIVES_LEAF_NOQUANT, NULL) == WEED_FALSE) break;
        }
        // next time we can go fwd from here
        if (fmap) old_fmap = fmap;
        else old_fmap = filter_map;
      } else {
        // we have checked, we only need to as far back as old_fmap
        fmap = old_fmap;
        while (fmap != filter_map) {
          fmap = get_next_event(fmap);
          if (fmap == filter_map) break;
          if (WEED_EVENT_IS_FILTER_MAP(fmap)
              && weed_get_boolean_value(fmap, LIVES_LEAF_NOQUANT, NULL) == WEED_FALSE) old_fmap = fmap;
        }
      }
      if (old_fmap != filter_map) {
        init_events = (weed_event_t **)weed_get_voidptr_array_counted
                      (old_fmap, WEED_LEAF_INIT_EVENTS, &ninits);
        if (init_events) {
          // we nned to prune the events from the video filter map and remove the audio added events
          weed_event_t **new_init_events = lives_calloc(ninits, sizeof(weed_event_t *));
          int nninits = 0;
          for (int i = 0; i < ninits; i++) {
            if (weed_get_boolean_value(init_events[i], LIVES_LEAF_NOQUANT, NULL) == WEED_FALSE) {
              new_init_events[nninits++] = init_events[i];
            }
          }
          if (nninits) {
            update_filter_maps(filter_map, get_next_event(filter_map), new_init_events, nninits);
          }
          lives_free(init_events);
          lives_free(new_init_events);
        }
      }
    }
    if (WEED_EVENT_IS_FILTER_MAP(event)) filter_map = event;
    else filter_map = NULL;
  }
  return filter_map;
}


static void event_list_reorder_noquant(weed_event_t *event_list) {
  // some events created in the audio thread should not be quantised, as they need to happen in the audio renderer
  // however they may end up out of order -
  // here we put them in correct chronological order
  // having done thie, we can now merge the filter maps for audio and video effects
  // which were added separately during recording
  weed_event_t *event, *prev_event = NULL, *next_event, *filter_map = NULL;
  if (!event_list) return;

  event = get_first_event(event_list);
  if (!event) return;

  next_event = get_next_event(event);
  if (!next_event) return;

  // reset
  check_noq_filter_maps(NULL, NULL);

  while (event) {
    weed_event_t *xnext_event = next_event = get_next_event(event);
    if (weed_get_boolean_value(event, LIVES_LEAF_NOQUANT, NULL) == WEED_TRUE) {
      weed_timecode_t tc = get_event_timecode(event), noquant_tc = tc, next_tc;

      if (WEED_EVENT_IS_PARAM_CHANGE(event)) {
        void *init_event = weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);
        weed_event_t *deinit_event = weed_get_plantptr_value((weed_plant_t *)init_event,
                                     WEED_LEAF_DEINIT_EVENT, NULL);
        if (get_event_timecode(deinit_event) < tc) {
          delete_event(event_list, event);
          event = next_event;
          continue;
        }
      }
      if (prev_event) {
        weed_timecode_t prev_tc = get_event_timecode(prev_event);
        if (prev_tc > tc) {
          while (prev_event && prev_tc > tc) {
            prev_tc = get_event_timecode(prev_event);
            prev_event = get_prev_event(prev_event);
          }
          while (1) {
            if (!prev_event && tc < prev_tc) {
              weed_event_set_timecode(event, prev_tc);
            }

            // unlink event
            unlink_event(event_list, event);
            //g_print("move back\n");

            // insert after prev_event, at timecode tc
            insert_event_after(prev_event ? prev_event : get_first_event(event_list), event);

            // - update filter maps
            filter_map = check_noq_filter_maps(event, filter_map);
            event = next_event;
            if (!next_event) break;
            if (weed_get_boolean_value(event, LIVES_LEAF_NOQUANT, NULL) == WEED_FALSE) break;
            tc = get_event_timecode(event);
            if (tc > noquant_tc) break;
            next_event = get_next_event(next_event);
          }
          continue;
        }
      }
      while (xnext_event && weed_get_boolean_value(xnext_event, LIVES_LEAF_NOQUANT, NULL) == WEED_TRUE) {
        xnext_event = get_next_event(xnext_event);
      }
      next_tc = get_event_timecode(xnext_event);
      if (next_tc < tc) {
        while (xnext_event) {
          xnext_event = get_next_event(xnext_event);
          if (weed_get_boolean_value(xnext_event, LIVES_LEAF_NOQUANT, NULL) == WEED_TRUE) continue;
          next_tc = get_event_timecode(xnext_event);
          if (next_tc > tc) break;
        }
        // insert before next_event
        while (1) {
          // unlink event
          unlink_event(event_list, event);
          //g_print("move fwd\n");

          if (xnext_event) {
            // insert before xnext_event
            insert_event_before(xnext_event, event);
            // - update filter maps
            filter_map = check_noq_filter_maps(event, filter_map);
          }
          event = next_event;
          next_event = get_next_event(next_event);
          if (weed_get_boolean_value(event, LIVES_LEAF_NOQUANT, NULL) == WEED_FALSE) break;
          tc = get_event_timecode(event);
          if (tc > noquant_tc) break;
        }
        continue;
      }
      // if it's a filter map, note it, when we get a deinit, another map, or reach the end, add its events to all
      // subsequent maps, then add events from prior map to it
      filter_map = check_noq_filter_maps(event, filter_map);
    } else prev_event = event;
    event = next_event;
  }
  if (filter_map) {
    if (weed_get_boolean_value(filter_map, LIVES_LEAF_NOQUANT, NULL) == WEED_FALSE) abort();
    event = get_filter_map_before(get_last_event(event_list), LIVES_TRACK_ANY, NULL);
    if (event && event != filter_map) {
      check_noq_filter_maps(event, filter_map);
    }
  }
}


static void event_list_close_gaps(weed_event_t *event_list, frames_t play_start) {
  // close gap at start of event list, and between record_end and record_start markers
  weed_event_t *event, *next_event, *first_event;
  weed_timecode_t tc = 0, tc_delta = 0, rec_end_tc = 0, tc_start = 0, tc_offs = 0, last_tc = 0;
  int marker_type;

  if (!event_list) return;

  if (!mainw->clip_switched) {
    /// offset of recording in clip
    tc_start = calc_time_from_frame(mainw->current_file, play_start) * TICKS_PER_SECOND_DBL;
  }

  event = get_first_event(event_list);
  if (WEED_PLANT_IS_EVENT_LIST(event)) event = get_next_event(event);

  first_event = event;

  if (event) tc_offs = get_event_timecode(event);
  tc_start += tc_offs;

  while (event) {
    next_event = get_next_event(event);
    last_tc = tc;
    tc = get_event_timecode(event) - tc_offs;
    if (tc < 0) tc = 0;

    if (weed_plant_has_leaf(event, WEED_LEAF_TC_ADJUSTMENT)) {
      // handle the case where we crashed partway thru
      if (next_event) {
        if (!weed_plant_has_leaf(next_event, WEED_LEAF_TC_ADJUSTMENT)) {
          weed_timecode_t ntc = get_event_timecode(next_event);
          tc_offs = weed_get_int64_value(event, WEED_LEAF_TC_ADJUSTMENT, NULL);
          if (tc + tc_offs > ntc) {
            tc -= tc_offs;
          }
        }
      } else if (last_tc + tc_offs <= tc) tc -= tc_offs;
    }

    if (WEED_EVENT_IS_MARKER(event)) {
      marker_type = weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, NULL);
      if (marker_type == EVENT_MARKER_RECORD_END) {
        rec_end_tc = tc;
        delete_event(event_list, event);
        event = next_event;
        continue;
      } else if (marker_type == EVENT_MARKER_RECORD_START) {
        // squash gaps in recording, but we note the gap for output to same clip
        tc_delta += tc - rec_end_tc;

        if (!mainw->clip_switched) {
          /// if this is > clip length we cannot render to same clip
          last_rec_start_tc = tc + tc_start;
          // if rendering to same clip, we will pick up this value and add it to the out frame tc
          weed_set_int64_value(event, WEED_LEAF_TCDELTA, tc_delta + tc_start);
        }
      }
    }

    /// subtract delta to close gaps, we already subtracted tc_offs
    tc -= tc_delta;
    weed_set_int64_value(event, WEED_LEAF_TC_ADJUSTMENT, tc_delta + tc_offs);
    weed_event_set_timecode(event, tc);
    event = next_event;
  }
  for (event = first_event; event; event = get_next_event(event)) {
    weed_leaf_delete(event, WEED_LEAF_TC_ADJUSTMENT);
  }
}


void add_track_to_avol_init(weed_plant_t *filter, weed_plant_t *event, int nbtracks, boolean behind) {
  // added a new video track - now we need to update our audio volume and pan effect
  weed_plant_t **in_ptmpls;
  void **pchainx, *pchange;

  int *new_in_tracks;
  int *igns, *nigns;

  int num_in_tracks;
  int nparams, numigns;

  int bval, i, j;

  // add a new value to in_tracks
  num_in_tracks = weed_leaf_num_elements(event, WEED_LEAF_IN_TRACKS) + 1;
  new_in_tracks = (int *)lives_malloc(num_in_tracks * sizint);
  for (i = 0; i < num_in_tracks; i++) {
    new_in_tracks[i] = i - nbtracks;
  }
  weed_set_int_array(event, WEED_LEAF_IN_TRACKS, num_in_tracks, new_in_tracks);
  lives_free(new_in_tracks);

  weed_set_int_value(event, WEED_LEAF_IN_COUNT, weed_get_int_value(event, WEED_LEAF_IN_COUNT, NULL) + 1);

  // update all param_changes

  pchainx = weed_get_voidptr_array_counted(event, WEED_LEAF_IN_PARAMETERS, &nparams);

  in_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);

  for (i = 0; i < nparams; i++) {
    pchange = (weed_plant_t *)pchainx[i];
    bval = WEED_FALSE;
    while (pchange) {
      fill_param_vals_to((weed_plant_t *)pchange, in_ptmpls[i], behind ? num_in_tracks - 1 : 1);
      if (weed_plant_has_leaf((weed_plant_t *)pchange, WEED_LEAF_IGNORE)) {
        igns = weed_get_boolean_array_counted((weed_plant_t *)pchange, WEED_LEAF_IGNORE, &numigns);
        nigns = (int *)lives_malloc(++numigns * sizint);

        for (j = 0; j < numigns; j++) {
          if (behind) {
            if (j < numigns - 1) nigns[j] = igns[j];
            else nigns[j] = bval;
          } else {
            if (j == 0) nigns[j] = igns[j];
            else if (j == 1) nigns[j] = bval;
            else nigns[j] = igns[j - 1];
          }
        }
        weed_set_boolean_array((weed_plant_t *)pchange, WEED_LEAF_IGNORE, numigns, nigns);
        lives_free(igns);
        lives_free(nigns);
      }
      pchange = weed_get_voidptr_value((weed_plant_t *)pchange, WEED_LEAF_NEXT_CHANGE, NULL);
      bval = WEED_TRUE;
    }
  }

  lives_free(in_ptmpls);
}


void event_list_add_track(weed_plant_t *event_list, int layer) {
  // in this function we insert a new track before existing tracks
  // TODO - memcheck
  weed_plant_t *event;

  int *clips, *newclips, i;
  frames64_t *frames, *newframes;
  int *in_tracks, *out_tracks;

  int num_in_tracks, num_out_tracks;
  int numframes;

  if (!event_list) return;

  event = get_first_event(event_list);
  while (event) {
    switch (get_event_type(event)) {
    case WEED_EVENT_TYPE_FRAME:
      clips = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &numframes);
      frames = weed_get_int64_array(event, WEED_LEAF_FRAMES, NULL);
      if (numframes == 1 && clips[0] == -1 && frames[0] == 0) {
        // for blank frames, we don't do anything
        lives_free(clips);
        lives_free(frames);
        break;
      }

      newclips = (int *)lives_malloc((numframes + 1) * sizint);
      newframes = (int64_t *)lives_malloc((numframes + 1) * 8);

      newclips[layer] = -1;
      newframes[layer] = 0;
      for (i = 0; i < numframes; i++) {
        if (i < layer) {
          newclips[i] = clips[i];
          newframes[i] = frames[i];
        } else {
          newclips[i + 1] = clips[i];
          newframes[i + 1] = frames[i];
        }
      }
      numframes++;

      weed_set_int_array(event, WEED_LEAF_CLIPS, numframes, newclips);
      weed_set_int64_array(event, WEED_LEAF_FRAMES, numframes, newframes);

      lives_free(newclips);
      lives_free(newframes);
      lives_free(clips);
      lives_free(frames);

      if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
        int *aclips = NULL;
        int atracks = weed_frame_event_get_audio_tracks(event, &aclips, NULL);
        for (i = 0; i < atracks; i += 2) {
          if (aclips[i] >= 0) aclips[i]++;
        }
        weed_set_int_array(event, WEED_LEAF_AUDIO_CLIPS, atracks, aclips);
        lives_free(aclips);
      }
      break;

    case WEED_EVENT_TYPE_FILTER_INIT:
      in_tracks = weed_get_int_array_counted(event, WEED_LEAF_IN_TRACKS, &num_in_tracks);
      if (num_in_tracks) {
        for (i = 0; i < num_in_tracks; i++) {
          if (in_tracks[i] >= layer) in_tracks[i]++;
        }
        weed_set_int_array(event, WEED_LEAF_IN_TRACKS, num_in_tracks, in_tracks);
        lives_free(in_tracks);
      }
      out_tracks = weed_get_int_array_counted(event, WEED_LEAF_OUT_TRACKS, &num_out_tracks);
      if (num_out_tracks) {
        for (i = 0; i < num_out_tracks; i++) {
          if (out_tracks[i] >= layer) out_tracks[i]++;
        }
        weed_set_int_array(event, WEED_LEAF_OUT_TRACKS, num_out_tracks, out_tracks);
        lives_free(out_tracks);
      }
      break;
    }
    event = get_next_event(event);
  }
}


static weed_event_t *create_frame_event(weed_event_t *event, weed_timecode_t tc,
                                        int numframes, int *clips, frames64_t *frames) {
  weed_error_t error = weed_event_set_timecode(event, tc);
  if (error != WEED_SUCCESS) return NULL;
  error = weed_set_int_value(event, WEED_LEAF_EVENT_TYPE, WEED_EVENT_TYPE_FRAME);
  if (error != WEED_SUCCESS) return NULL;

  error = weed_set_int_array(event, WEED_LEAF_CLIPS, numframes, clips);
  if (error != WEED_SUCCESS) return NULL;
  error = weed_set_int64_array(event, WEED_LEAF_FRAMES, numframes, frames);
  if (error != WEED_SUCCESS) return NULL;

  return event;
}


static weed_event_t *add_new_event(weed_event_list_t **pevent_list) {
  // append a generic event to an event_list, and return it, possibly updating event_list
  // returns NULL on memory or other error
  // caller should check if event_list is NULL on return, and if so, ignore event and exit with error
  weed_error_t error;
  weed_event_t *event;
  if (!*pevent_list) {
    *pevent_list = lives_event_list_new(NULL, NULL);
    if (!*pevent_list) return NULL;
  }

  event = weed_plant_new(WEED_PLANT_EVENT);
  if (!event) return NULL;

  error = weed_set_voidptr_value(event, WEED_LEAF_NEXT, NULL);
  if (error != WEED_SUCCESS) return NULL;

  error = weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, NULL);
  if (error != WEED_SUCCESS) return NULL;

  return event;
}


weed_event_list_t *append_frame_event(weed_event_list_t *event_list,
                                      weed_timecode_t tc, int numframes, int *clips, frames64_t *frames) {
  // append a frame event to an event_list
  // returns NULL on memory or other error
  weed_event_t *event;

  event = add_new_event(&event_list);
  if (!event_list) return NULL;

  event = create_frame_event(event, tc, numframes, clips, frames);
  if (!event) return NULL;

  if (set_event_ptrs(event_list, event) != WEED_SUCCESS) return NULL;
  //////////////////////////////////////

  return event_list;
}


void **filter_init_add_pchanges(weed_plant_t *event_list, weed_plant_t *plant, weed_plant_t *init_event, int ntracks,
                                int leave) {
  // add the initial values for all parameters when we insert a filter_init event
  weed_plant_t **in_params = NULL, **in_ptmpls;

  void **pchain = NULL;
  void **in_pchanges = NULL;

  weed_plant_t *filter = plant, *in_param;

  weed_timecode_t tc = get_event_timecode(init_event);

  boolean is_inst = FALSE;
  boolean noquant = FALSE;

  int num_params, i;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) {
    filter = weed_instance_get_filter(plant, TRUE);
    is_inst = TRUE;
  }

  // add param_change events and set "in_params"
  if (!weed_get_plantptr_value(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL)) return NULL;

  if (weed_get_boolean_value(init_event, LIVES_LEAF_NOQUANT, NULL) == WEED_TRUE) noquant = TRUE;

  in_ptmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &num_params);

  pchain = (void **)lives_malloc((num_params + 1) * sizeof(void *));
  pchain[num_params] = NULL;

  if (!is_inst) in_params = weed_params_create(filter, TRUE);

  if (leave > 0) {
    in_pchanges = weed_get_voidptr_array_counted(init_event, WEED_LEAF_IN_PARAMETERS, &num_params);
    if (leave > num_params) leave = num_params;
  }

  for (i = num_params - 1; i >= 0; i--) {
    if (i < leave && in_pchanges && in_pchanges[i]) {
      // maintain existing values
      pchain[i] = in_pchanges[i];
      continue;
    }

    pchain[i] = weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value((weed_plant_t *)pchain[i], WEED_LEAF_EVENT_TYPE, WEED_EVENT_TYPE_PARAM_CHANGE);
    weed_set_int64_value((weed_plant_t *)pchain[i], WEED_LEAF_TIMECODE, tc);

    if (!is_inst) in_param = in_params[i];
    else in_param = weed_inst_in_param(plant, i, FALSE, FALSE);

    if (is_perchannel_multiw(in_param)) {
      // if the parameter is element-per-channel, fill up to number of channels
      fill_param_vals_to(in_param, in_ptmpls[i], ntracks - 1);
    }

    if (weed_param_value_irrelevant(in_param))
      weed_leaf_copy((weed_plant_t *)pchain[i], WEED_LEAF_VALUE,
                     weed_param_get_template(in_param), WEED_LEAF_DEFAULT);
    else weed_leaf_dup((weed_plant_t *)pchain[i], in_param, WEED_LEAF_VALUE);

    weed_set_int_value((weed_plant_t *)pchain[i], WEED_LEAF_INDEX, i);
    weed_set_voidptr_value((weed_plant_t *)pchain[i], WEED_LEAF_INIT_EVENT, init_event);
    weed_set_voidptr_value((weed_plant_t *)pchain[i], WEED_LEAF_NEXT_CHANGE, NULL);
    weed_set_voidptr_value((weed_plant_t *)pchain[i], WEED_LEAF_PREV_CHANGE, NULL);
    weed_set_boolean_value((weed_plant_t *)pchain[i], WEED_LEAF_IS_DEF_VALUE, WEED_TRUE);

    insert_param_change_event_at(event_list, init_event, (weed_plant_t *)pchain[i]);
    if (noquant) weed_set_boolean_value(pchain[i], LIVES_LEAF_NOQUANT, WEED_TRUE);
  }

  if (in_pchanges) lives_free(in_pchanges);

  if (!is_inst) {
    weed_in_params_free(in_params, num_params);
    lives_free(in_params);
  } else {
    lives_free(in_params);
  }
  lives_free(in_ptmpls);

  weed_set_voidptr_array(init_event, WEED_LEAF_IN_PARAMETERS, num_params, pchain);

  return pchain;
}


weed_event_list_t *append_filter_init_event(weed_event_list_t *event_list,
    weed_timecode_t tc, int filter_idx, int num_in_tracks, int key, weed_plant_t *inst) {
  weed_plant_t **ctmpl;
  weed_event_t *event;
  weed_plant_t *filter, *chan;

  char *tmp;

  int e_in_channels, e_out_channels, e_ins, e_outs;
  int total_in_channels = 0;
  int total_out_channels = 0;
  int my_in_tracks = 0;

  int i;

  event = add_new_event(&event_list);
  if (!event_list) return NULL;

  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);
  weed_set_int_value(event, WEED_LEAF_EVENT_TYPE, WEED_EVENT_TYPE_FILTER_INIT);
  weed_set_string_value(event, WEED_LEAF_FILTER,
                        (tmp = make_weed_hashname(filter_idx, TRUE, FALSE, 0, FALSE)));
  lives_free(tmp);

  if (weed_plant_has_leaf(inst, WEED_LEAF_RANDOM_SEED)) {
    weed_set_int64_value(event, WEED_LEAF_RANDOM_SEED,
                         weed_get_int64_value(inst, WEED_LEAF_RANDOM_SEED, NULL));
  }

  filter = get_weed_filter(filter_idx);

  ctmpl = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, &total_in_channels);

  if (total_in_channels > 0) {
    int count[total_in_channels];
    for (i = 0; i < total_in_channels; i++) {
      if (weed_get_boolean_value(ctmpl[i], WEED_LEAF_HOST_DISABLED, NULL) == WEED_FALSE) {
        count[i] = 1;
        my_in_tracks++;
        weed_set_int_value(ctmpl[i], WEED_LEAF_HOST_REPEATS, 1);
      } else count[i] = 0;
    }

    // TODO ***
    if (my_in_tracks < num_in_tracks) {
      int repeats;
      // we need to use some repeated channels
      for (i = 0; i < total_in_channels; i++) {
        if (weed_plant_has_leaf(ctmpl[i], WEED_LEAF_MAX_REPEATS)
            && (count[i] > 0 || has_usable_palette(filter, ctmpl[i]))) {
          repeats = weed_get_int_value(ctmpl[i], WEED_LEAF_MAX_REPEATS, NULL);
          if (repeats == 0) {
            count[i] += num_in_tracks - my_in_tracks;

            /*
                  weed_set_int_value(ctmpl[i],WEED_LEAF_HOST_REPEATS,count[i]);
                  weed_set_boolean_value(ctmpl[i],WEED_LEAF_HOST_DISABLED,WEED_FALSE);
            */

            break;
          }
          count[i] += num_in_tracks - my_in_tracks >= repeats - 1 ?
                      repeats - 1 : num_in_tracks - my_in_tracks;
          my_in_tracks += count[i] - 1;
          if (my_in_tracks == num_in_tracks) break;
        }
      }
    }
    weed_set_int_array(event, WEED_LEAF_IN_COUNT, total_in_channels, count);
    lives_free(ctmpl);
  }

  ctmpl = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &total_out_channels);

  if (total_out_channels > 0) {
    int count[total_out_channels];
    ctmpl = weed_get_plantptr_array(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, NULL);
    for (i = 0; i < total_out_channels; i++) {
      if (!weed_plant_has_leaf(ctmpl[i], WEED_LEAF_HOST_DISABLED) ||
          weed_get_boolean_value(ctmpl[i], WEED_LEAF_HOST_DISABLED, NULL) != WEED_TRUE) count[i] = 1;
      else count[i] = 0;
    }
    lives_free(ctmpl);

    weed_set_int_array(event, WEED_LEAF_OUT_COUNT, total_out_channels, count);
  }

  e_ins = e_in_channels = enabled_in_channels(get_weed_filter(filter_idx), FALSE);
  e_outs = e_out_channels = enabled_out_channels(get_weed_filter(filter_idx), FALSE);

  // discount alpha_channels (in and out)
  if (inst) {
    for (i = 0; i < e_ins; i++) {
      chan = get_enabled_channel(inst, i, TRUE);
      if (weed_palette_is_alpha(weed_layer_get_palette(chan))) e_in_channels--;
    }

    // handling for compound fx
    while (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE)) inst = weed_get_plantptr_value(inst,
          WEED_LEAF_HOST_NEXT_INSTANCE, NULL);

    for (i = 0; i < e_outs; i++) {
      chan = get_enabled_channel(inst, i, FALSE);
      if (weed_palette_is_alpha(weed_layer_get_palette(chan))) e_out_channels--;
    }
  }

  // here we map our tracks to channels
  if (e_in_channels != 0) {
    if (e_in_channels == 1) {
      weed_set_int_value(event, WEED_LEAF_IN_TRACKS, 0);
    } else {
      int *tracks = (int *)lives_malloc(2 * sizint);
      tracks[0] = 0;
      tracks[1] = 1;
      weed_set_int_array(event, WEED_LEAF_IN_TRACKS, 2, tracks);
      lives_free(tracks);
    }
  }

  if (e_out_channels > 0) {
    weed_set_int_value(event, WEED_LEAF_OUT_TRACKS, 0);
  }

  if (key > -1) {
    weed_set_int_value(event, WEED_LEAF_HOST_KEY, key);
    weed_set_int_value(event, WEED_LEAF_HOST_MODE, rte_key_getmode(key));
  }


  ///////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG_EVENTS
  g_print("adding init event at tc %"PRId64"\n", tc);
#endif

  if (set_event_ptrs(event_list, event) != WEED_SUCCESS) return NULL;

  return event_list;
}


weed_event_list_t *append_filter_deinit_event(weed_event_list_t *event_list,
    weed_timecode_t tc, void *init_event, void **pchain) {
  weed_event_t *event = add_new_event(&event_list);
  if (!event_list) return NULL;

  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);
  weed_set_int_value(event, WEED_LEAF_EVENT_TYPE, WEED_EVENT_TYPE_FILTER_DEINIT);
  weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENT, init_event);

  weed_leaf_delete((weed_plant_t *)init_event, WEED_LEAF_DEINIT_EVENT); // delete since we assign a placeholder with int64 type

  weed_set_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, (void *)event);
  if (pchain) {
    int num_params = 0;
    while (pchain[num_params]) num_params++;
    weed_set_voidptr_array(event, WEED_LEAF_IN_PARAMETERS, num_params, pchain);
  }

  if (set_event_ptrs(event_list, event) != WEED_SUCCESS) return NULL;

  return event_list;
}


weed_event_list_t *append_param_change_event(weed_event_list_t *event_list,
    weed_timecode_t tc, int pnum, weed_plant_t *param, void *init_event, void **pchain) {
  weed_event_t *event, *xevent;
  weed_event_t *last_pchange_event;

  event = add_new_event(&event_list);
  if (!event_list) return NULL;

  // TODO - error check
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);
  weed_set_int_value(event, WEED_LEAF_EVENT_TYPE, WEED_EVENT_TYPE_PARAM_CHANGE);
  weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENT, init_event);
  weed_set_int_value(event, WEED_LEAF_INDEX, pnum);
  weed_leaf_copy(event, WEED_LEAF_VALUE, param, WEED_LEAF_VALUE);

  if (pchain) {
    last_pchange_event = (weed_plant_t *)pchain[pnum];
    while ((xevent = (weed_plant_t *)weed_get_voidptr_value(last_pchange_event,
                     WEED_LEAF_NEXT_CHANGE, NULL)) != NULL)
      last_pchange_event = xevent;

    if (weed_event_get_timecode(last_pchange_event) == tc && !is_init_pchange(init_event, last_pchange_event)) {
      weed_event_t *dup_event = last_pchange_event;
      last_pchange_event = (weed_plant_t *)weed_get_voidptr_value(last_pchange_event,
                           WEED_LEAF_PREV_CHANGE, NULL);
      delete_event(event_list, dup_event);
    }

    weed_set_voidptr_value(last_pchange_event, WEED_LEAF_NEXT_CHANGE, event);
    weed_set_voidptr_value(event, WEED_LEAF_PREV_CHANGE, last_pchange_event);
    weed_set_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, NULL);
  }
  if (set_event_ptrs(event_list, event) != WEED_SUCCESS) return NULL;

  return event_list;
}


weed_event_list_t *append_filter_map_event(weed_event_list_t *event_list,
    weed_timecode_t tc, weed_event_t **init_events) {
  weed_event_t *event;
  int i = 0;

  event = add_new_event(&event_list);
  if (!event_list) return NULL;

  // TODO - error check
  weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);
  weed_set_int_value(event, WEED_LEAF_EVENT_TYPE, WEED_EVENT_TYPE_FILTER_MAP);

  if (init_events) for (i = 0; init_events[i]; i++);

  if (i == 0) weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENTS, NULL);
  else weed_set_voidptr_array(event, WEED_LEAF_INIT_EVENTS, i, (void **)init_events);

#ifdef DEBUG_EVENTS
  g_print("adding map event %p with %d init_events at tc %"PRId64"\n", event, i, tc);
#endif

  if (set_event_ptrs(event_list, event) != WEED_SUCCESS) return NULL;

  return event_list;
}


void get_active_track_list(int *clip_index, int num_tracks, weed_plant_t *filter_map) {
  // replace entries in clip_index with 0 if the track is not either the front track or an input to a filter

  // TODO *** we should ignore any filter which does not eventually output to the front track,
  // this involves examining the filter map in reverse order and mapping out_tracks back to in_tracks
  // marking those which we cover

  weed_event_t **init_events;
  weed_plant_t *filter;

  char *filter_hash;

  int *in_tracks, *out_tracks;
  int ninits, nintracks, nouttracks;
  int idx;
  int front = -1;

  int i, j;

  /// if we are previewing a solo effect in multitrack, then we ignore the usual rules for the front frame, and instead
  /// use the (first) output track from the filter instance
  if (mainw->multitrack && mainw->multitrack->solo_inst && mainw->multitrack->init_event && !LIVES_IS_PLAYING) {
    weed_event_t *ievent = mainw->multitrack->init_event;
    front = weed_get_int_value(ievent, WEED_LEAF_OUT_TRACKS, NULL);
  }

  for (i = 0; i < num_tracks; i++) {
    if ((front == -1 || front == i) && clip_index[i] > 0) {
      mainw->active_track_list[i] = clip_index[i];
      front = i;
    } else mainw->active_track_list[i] = 0;
  }

  if (!filter_map || !weed_plant_has_leaf(filter_map, WEED_LEAF_INIT_EVENTS)) return;
  init_events = (weed_event_t **)weed_get_voidptr_array_counted(filter_map, WEED_LEAF_INIT_EVENTS, &ninits);
  if (!init_events) return;

  for (i = ninits - 1; i >= 0; i--) {
    // get the filter and make sure it has video chans out, which feed to an active track
    if (!weed_plant_has_leaf(init_events[i], WEED_LEAF_OUT_TRACKS)
        || !weed_plant_has_leaf(init_events[i], WEED_LEAF_IN_TRACKS)) continue;
    if (mainw->multitrack && mainw->multitrack->solo_inst && mainw->multitrack->init_event
        && mainw->multitrack->init_event != init_events[i] && !LIVES_IS_PLAYING) continue;
    filter_hash = weed_get_string_value(init_events[i], WEED_LEAF_FILTER, NULL);
    if ((idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
      filter = get_weed_filter(idx);
      if (has_video_chans_in(filter, FALSE) && has_video_chans_out(filter, FALSE)) {
        boolean is_valid = FALSE;
        out_tracks = weed_get_int_array_counted(init_events[i], WEED_LEAF_OUT_TRACKS, &nouttracks);
        for (j = 0; j < nouttracks; j++) {
          if (j >= mainw->num_tracks) break;
          if (mainw->active_track_list[out_tracks[j]] != 0) {
            is_valid = TRUE;
            break;
          }
        }
        lives_free(out_tracks);
        if (is_valid) {
          in_tracks = weed_get_int_array_counted(init_events[i], WEED_LEAF_IN_TRACKS, &nintracks);
          for (j = 0; j < nintracks; j++) {
            if (j >= mainw->num_tracks) break;
            mainw->active_track_list[in_tracks[j]] = clip_index[in_tracks[j]];
          }
          lives_free(in_tracks);
        }
      }
    }
    lives_free(filter_hash);
  }

  lives_free(init_events);
}


weed_event_t *process_events(weed_event_t *next_event, boolean process_audio, weed_timecode_t curr_tc) {
  // here we play back (preview) with an event_list
  // we process all events, but drop frames (unless mainw->nodrop is set)

  static weed_timecode_t aseek_tc = 0;
  weed_timecode_t tc, next_tc;

  static double stored_avel = 0.;

  static int dframes = 0, spare_cycles = 0;

  int *in_count = NULL;

  void *init_event, **eevents;

  weed_event_t *next_frame_event, *return_event;
  weed_plant_t *filter;
  weed_plant_t *inst, *orig_inst;

  weed_plant_t **citmpl = NULL, **cotmpl = NULL;
  weed_plant_t **bitmpl = NULL, **botmpl = NULL;
  weed_plant_t **source_params, **in_params;

  char *filter_name;
  char *key_string;

  int current_file;

  int num_params, offset = 0, nparams;
  int num_in_count = 0;
  int num_in_channels = 0, num_out_channels = 0;
  int new_file;
  int etype;
  int key, idx;
  int nev;

  int i;

  if (!next_event) {
    aseek_tc = 0;
    dframes = 0;
    return NULL;
  }

  if (mainw->multitrack && LIVES_IS_PLAYING) prefs->pb_quality = future_prefs->pb_quality;

  tc = get_event_timecode(next_event);

  if (mainw->playing_file != -1 && tc > curr_tc) {
    // next event is in our future
    if (mainw->multitrack && mainw->last_display_ticks > 0) {
      if ((mainw->fixed_fpsd > 0. && (curr_tc - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL >= 1. / mainw->fixed_fpsd) ||
          (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback &&
           (curr_tc - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL >= 1. / mainw->vpp->fixed_fpsd)) {
        // ...but playing at fixed fps, which is faster than mt fps
        mainw->pchains = pchains;
        if (prefs->pbq_adaptive) {
          if (dframes > 0) update_effort(dframes, TRUE);
          else update_effort(spare_cycles, FALSE);
          dframes = 0;
          spare_cycles = 0;
        }
        load_frame_image(cfile->last_frameno >= 1 ? cfile->last_frameno : cfile->start);
        if (prefs->show_player_stats) {
          mainw->fps_measure++;
        }
        if (mainw->last_display_ticks == 0) mainw->last_display_ticks = curr_tc;
        else {
          if (mainw->vpp && mainw->ext_playback && mainw->vpp->fixed_fpsd > 0.)
            mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->vpp->fixed_fpsd;
          else if (mainw->fixed_fpsd > 0.)
            mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->fixed_fpsd;
          else mainw->last_display_ticks = curr_tc;
        }
        mainw->pchains = NULL;
      } else spare_cycles++;
    }
    return next_event;
  }

  if (mainw->cevent_tc != -1)
    aseek_tc += (weed_timecode_t)((double)(tc - mainw->cevent_tc) * stored_avel);
  mainw->cevent_tc = tc;

  return_event = get_next_event(next_event);
  etype = get_event_type(next_event);
  switch (etype) {
  case WEED_EVENT_TYPE_FRAME:

#ifdef DEBUG_EVENTS
    g_print("event: frame event at tc %"PRId64" curr_tc=%"PRId64"\n", tc, curr_tc);
#endif

    eevents = weed_get_voidptr_array_counted(next_event, LIVES_LEAF_EASING_EVENTS, &nev);
    if (eevents) {
      weed_plant_t *gui;
      for (i = 0; i < nev; i++) {
        init_event = (weed_plant_t *)eevents[i];
        if (init_event) {
          key_string = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, NULL);
          key = atoi(key_string);
          lives_free(key_string);
          inst = rte_keymode_get_instance(key + 1, 0);
          weed_set_boolean_value(inst, LIVES_LEAF_AUTO_EASING, WEED_TRUE);
          gui = weed_instance_get_gui(inst, FALSE);
          if (gui) {
            int easeval = weed_get_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, NULL);
            if (easeval) weed_set_int_value(gui, WEED_LEAF_EASE_OUT, easeval);
          }
        }
      }
      lives_free(eevents);
    }

    if (!mainw->multitrack && is_realtime_aplayer(prefs->audio_player) && WEED_EVENT_IS_AUDIO_FRAME(next_event)) {
      // keep track of current seek position, for animating playback pointers
      int *aclips = weed_get_int_array(next_event, WEED_LEAF_AUDIO_CLIPS, NULL);
      double *aseeks = weed_get_double_array(next_event, WEED_LEAF_AUDIO_SEEKS, NULL);

      if (aclips[1] > 0) {
        aseek_tc = aseeks[0] * TICKS_PER_SECOND_DBL;
        stored_avel = aseeks[1];
      }

      lives_freep((void **)&aseeks);
      lives_freep((void **)&aclips);
    }

    if ((next_frame_event = get_next_frame_event(next_event))) {
      next_tc = get_event_timecode(next_frame_event);
      // drop frame if it is too far behind
      if (LIVES_IS_PLAYING && next_tc <= curr_tc) {
        if (prefs->pbq_adaptive) dframes++;
        if (!prefs->noframedrop) break;
      }
      if (!mainw->fs && !prefs->hide_framebar && !mainw->multitrack) {
        lives_signal_handler_block(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), cfile->pb_fps);
        lives_signal_handler_unblock(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
      }
    }

    lives_freep((void **)&mainw->clip_index);
    lives_freep((void **)&mainw->frame_index);

    mainw->clip_index = weed_get_int_array_counted(next_event, WEED_LEAF_CLIPS, &mainw->num_tracks);
    mainw->frame_index = weed_get_int64_array(next_event, WEED_LEAF_FRAMES, NULL);

    if (mainw->scrap_file != -1) {
      int nclips = mainw->num_tracks;
      for (i = 0; i < nclips; i++) {
        if (mainw->clip_index[i] == mainw->scrap_file) {
          int64_t offs = weed_get_int64_value(next_event, WEED_LEAF_HOST_SCRAP_FILE_OFFSET, NULL);
          if (!mainw->files[mainw->scrap_file]->ext_src) load_from_scrap_file(NULL, -1);
          lives_lseek_buffered_rdonly_absolute(LIVES_POINTER_TO_INT(mainw->files[mainw->scrap_file]->ext_src), offs);
        }
      }
    }

    // if we are in multitrack mode, we will just set up NULL layers and let the effects pull our frames
    if (mainw->multitrack) {

      if (!LIVES_IS_PLAYING || ((mainw->fixed_fpsd <= 0. && (!mainw->vpp || mainw->vpp->fixed_fpsd <= 0. || !mainw->ext_playback))
                                || (mainw->fixed_fpsd > 0. && (curr_tc - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL >= 1.
                                    / mainw->fixed_fpsd) ||
                                (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback &&
                                 (curr_tc - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL >= 1. / mainw->vpp->fixed_fpsd))) {
        mainw->pchains = pchains;

        if (LIVES_IS_PLAYING) {
          if (prefs->pbq_adaptive) {
            update_effort(dframes, TRUE);
            dframes = 0;
            spare_cycles = 0;
          }
        }

        load_frame_image(cfile->frameno);

        if (LIVES_IS_PLAYING) {
          if (prefs->show_player_stats) {
            mainw->fps_measure++;
          }
          if (mainw->last_display_ticks == 0) mainw->last_display_ticks = curr_tc;
          else {
            if (mainw->vpp && mainw->ext_playback && mainw->vpp->fixed_fpsd > 0.)
              mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->vpp->fixed_fpsd;
            else if (mainw->fixed_fpsd > 0.)
              mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->fixed_fpsd;
            else mainw->last_display_ticks = curr_tc;
          }
        }
        mainw->pchains = NULL;
      } else spare_cycles++;
    } else {
      if (mainw->num_tracks > 1) {
        if (mainw->blend_file != mainw->clip_index[1]) {
          track_decoder_free(1, mainw->blend_file, mainw->clip_index[1]);
          mainw->blend_file = mainw->clip_index[1];
        }
        if (IS_VALID_CLIP(mainw->blend_file)) mainw->files[mainw->blend_file]->frameno = mainw->frame_index[1];
      } else {
        track_decoder_free(1, mainw->blend_file, -1);
        mainw->blend_file = -1;
      }
      new_file = -1;
      for (i = 0; i < mainw->num_tracks && new_file == -1; i++) {
        new_file = mainw->clip_index[i];
      }
      if (i == 2) {
        track_decoder_free(1, mainw->blend_file, -1);
        mainw->blend_file = -1;
      }

#ifdef DEBUG_EVENTS
      g_print("event: front frame is %"PRId64" tc %"PRId64" curr_tc=%"PRId64"\n", mainw->frame_index[0], tc, curr_tc);
#endif

      // handle case where new_file==-1: we must somehow create a blank frame in load_frame_image
      if (new_file == -1) new_file = mainw->current_file;
      if (prefs->pbq_adaptive) {
        if (dframes > 0) update_effort(dframes, TRUE);
        else update_effort(spare_cycles, FALSE);
        dframes = 0;
      }
      if (!mainw->urgency_msg && weed_plant_has_leaf(next_event, WEED_LEAF_OVERLAY_TEXT)) {
        mainw->urgency_msg = weed_get_string_value(next_event, WEED_LEAF_OVERLAY_TEXT, NULL);
      }

      if (new_file != mainw->current_file) {
        mainw->files[new_file]->frameno = mainw->frame_index[i - 1];
        if (new_file != mainw->scrap_file) {
          // switch to a new file
          do_quick_switch(new_file);
          cfile->next_event = return_event;
          return_event = NULL;
        } else {
          /// load a frame from the scrap file
          mainw->files[new_file]->hsize = cfile->hsize; // set size of scrap file
          mainw->files[new_file]->vsize = cfile->vsize;
          current_file = mainw->current_file;
          mainw->current_file = new_file;
          mainw->aframeno = (double)(aseek_tc / TICKS_PER_SECOND_DBL) * cfile->fps;
          mainw->pchains = pchains;
          load_frame_image(cfile->frameno);
          if (prefs->show_player_stats) {
            mainw->fps_measure++;
          }
          mainw->pchains = NULL;
          mainw->current_file = current_file;
        }
        break;
      } else {
        cfile->frameno = mainw->frame_index[i - 1];
        mainw->aframeno = (double)(aseek_tc / TICKS_PER_SECOND_DBL) * cfile->fps;
        mainw->pchains = pchains;
        load_frame_image(cfile->frameno);
        mainw->pchains = NULL;
      }
    }
    cfile->next_event = get_next_event(next_event);
    break;
  case WEED_EVENT_TYPE_FILTER_INIT:
    // effect init
    //  bind the weed_fx to next free key/0
    filter_name = weed_get_string_value(next_event, WEED_LEAF_FILTER, NULL);
    idx = weed_get_idx_for_hashname(filter_name, TRUE);
    lives_free(filter_name);

    if (idx != -1) {
      filter = get_weed_filter(idx);

      if (!process_audio && is_pure_audio(filter, FALSE)) {
        //if (weed_plant_has_leaf(next_event, WEED_LEAF_HOST_TAG)) weed_leaf_delete(next_event, WEED_LEAF_HOST_TAG);
        break; // audio effects are processed in the audio renderer
      }

      if (process_audio && !is_pure_audio(filter, FALSE)) break;

      key = get_next_free_key();
      weed_add_effectkey_by_idx(key + 1, idx);

      key_string = lives_strdup_printf("%d", key);
      weed_set_string_value(next_event, WEED_LEAF_HOST_TAG, key_string);
      lives_free(key_string);

#ifdef DEBUG_EVENTS
      g_print("event: init effect (%d) on key %d at tc %"PRId64" curr_tc=%"PRId64"\n", idx, key, tc, curr_tc);
#endif
      if (weed_plant_has_leaf(next_event, WEED_LEAF_IN_COUNT)) {
        in_count = weed_get_int_array_counted(next_event, WEED_LEAF_IN_COUNT, &num_in_count);
      }

      citmpl = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, &num_in_channels);
      if (num_in_channels > 0) {
        bitmpl = (weed_plant_t **)lives_malloc(num_in_channels * sizeof(weed_plant_t *));
        if (num_in_channels != num_in_count) LIVES_ERROR("num_in_count != num_in_channels");
        for (i = 0; i < num_in_channels; i++) {
          bitmpl[i] = weed_plant_copy(citmpl[i]);
          if (in_count[i] > 0) {
            weed_set_boolean_value(citmpl[i], WEED_LEAF_HOST_DISABLED, WEED_FALSE);
            weed_set_int_value(citmpl[i], WEED_LEAF_HOST_REPEATS, in_count[i]);
          } else weed_set_boolean_value(citmpl[i], WEED_LEAF_HOST_DISABLED, WEED_TRUE);
        }
      }

      lives_freep((void **)&in_count);

      cotmpl = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &num_out_channels);
      if (num_out_channels > 0) {
        botmpl = (weed_plant_t **)lives_malloc(num_out_channels * sizeof(weed_plant_t *));
        for (i = 0; i < num_out_channels; i++) {
          botmpl[i] = weed_plant_copy(cotmpl[i]);
        }
      }
      THREADVAR(random_seed) = weed_get_int64_value(next_event, WEED_LEAF_RANDOM_SEED, NULL);

      weed_init_effect(key);

      // restore channel state / number from backup

      if (num_in_channels > 0) {
        for (i = 0; i < num_in_channels; i++) {
          weed_leaf_copy_or_delete(citmpl[i], WEED_LEAF_HOST_DISABLED, bitmpl[i]);
          weed_leaf_copy_or_delete(citmpl[i], WEED_LEAF_HOST_REPEATS, bitmpl[i]);
          weed_plant_free(bitmpl[i]);
        }
        lives_free(bitmpl);
        lives_free(citmpl);
      }

      if (num_out_channels > 0) {
        for (i = 0; i < num_out_channels; i++) {
          weed_leaf_copy_or_delete(cotmpl[i], WEED_LEAF_HOST_DISABLED, botmpl[i]);
          weed_leaf_copy_or_delete(cotmpl[i], WEED_LEAF_HOST_REPEATS, botmpl[i]);
          weed_plant_free(botmpl[i]);
        }
        lives_free(botmpl);
        lives_free(cotmpl);
      }

      // reinit effect with saved parameters
      orig_inst = inst = rte_keymode_get_instance(key + 1, 0);

      if (weed_plant_has_leaf(next_event, WEED_LEAF_IN_PARAMETERS)) {
        void **xpchains = weed_get_voidptr_array_counted(next_event, WEED_LEAF_IN_PARAMETERS, &nparams);
        pchains[key] = (void **)lives_realloc(pchains[key], (nparams + 1) * sizeof(void *));
        for (i = 0; i < nparams; i++) pchains[key][i] = xpchains[i];
        pchains[key][nparams] = NULL;
        lives_free(xpchains);
      } else pchains[key] = NULL;

filterinit1:

      num_params = num_in_params(inst, FALSE, FALSE);

      if (num_params > 0) {
        weed_call_deinit_func(inst);
        if (weed_plant_has_leaf(next_event, WEED_LEAF_IN_PARAMETERS)) {
          source_params = (weed_plant_t **)pchains[key];
          if (source_params) {
            in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
            for (i = 0; i < num_params && i < nparams; i++) {
              if (source_params[i + offset] && is_init_pchange(next_event, source_params[i + offset]))
                weed_leaf_dup(in_params[i], source_params[i + offset], WEED_LEAF_VALUE);
            }
            lives_free(in_params);
          }
        }
        offset += num_params;
        weed_call_init_func(inst);
      }

      if (weed_plant_has_leaf(next_event, WEED_LEAF_HOST_KEY)) {
        // mt events will not have this;
        // it is used to connect params and alpha channels during rendering
        // holds our original key/mode values

        // index into pchains...

        int hostkey = weed_get_int_value(next_event, WEED_LEAF_HOST_KEY, NULL);
        int hostmode = weed_get_int_value(next_event, WEED_LEAF_HOST_MODE, NULL);

        weed_set_int_value(inst, WEED_LEAF_HOST_KEY, hostkey);
        weed_set_int_value(inst, WEED_LEAF_HOST_MODE, hostmode);
      }

      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE)) {
        // handle compound fx
        inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
        goto filterinit1;
      }
      weed_instance_unref(orig_inst);
    }
    THREADVAR(random_seed) = 0;
    break;

  case WEED_EVENT_TYPE_FILTER_DEINIT:
    init_event = weed_get_voidptr_value((weed_plant_t *)next_event, WEED_LEAF_INIT_EVENT, NULL);
    if (!init_event) break;
    if (weed_plant_has_leaf((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG)) {
      key_string = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, NULL);
      key = atoi(key_string);
      lives_free(key_string);

      filter_name = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_FILTER, NULL);
      idx = weed_get_idx_for_hashname(filter_name, TRUE);
      lives_free(filter_name);

      filter = get_weed_filter(idx);

      if (!process_audio) {
        if (is_pure_audio(filter, FALSE)) break; // audio effects are processed in the audio renderer
      }

      if (process_audio && !is_pure_audio(filter, FALSE)) break;

      if ((inst = rte_keymode_get_instance(key + 1, 0))) {
        weed_deinit_effect(key);
        weed_delete_effectkey(key, 0);
        weed_instance_unref(inst);
      }
      if (pchains[key]) lives_free(pchains[key]);
      pchains[key] = NULL;
    }
    break;

  case WEED_EVENT_TYPE_FILTER_MAP:
    mainw->filter_map = next_event;
#ifdef DEBUG_EVENTS
    g_print("got new filter map %p\n", mainw->filter_map);
#endif
    break;
  case WEED_EVENT_TYPE_PARAM_CHANGE:
    if (!mainw->multitrack) {
      init_event = weed_get_voidptr_value((weed_plant_t *)next_event, WEED_LEAF_INIT_EVENT, NULL);
      if (weed_plant_has_leaf((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG)) {
        key_string = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, NULL);
        key = atoi(key_string);
        lives_free(key_string);

        filter_name = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_FILTER, NULL);
        idx = weed_get_idx_for_hashname(filter_name, TRUE);
        lives_free(filter_name);

        filter = get_weed_filter(idx);

        if (!process_audio) {
          if (is_pure_audio(filter, FALSE)) break; // audio effects are processed in the audio renderer
        }

        if (process_audio && !is_pure_audio(filter, FALSE)) break;

        if ((inst = rte_keymode_get_instance(key + 1, 0))) {
          int pnum = weed_get_int_value(next_event, WEED_LEAF_INDEX, NULL);
          weed_plant_t *param = weed_inst_in_param(inst, pnum, FALSE, FALSE);
          weed_leaf_dup(param, next_event, WEED_LEAF_VALUE);
          weed_instance_unref(inst);
        }
      }
    }
    break;
  }
  return return_event;
}


static char *set_proc_label(xprocess * proc, const char *label, boolean copy_old) {
  char *blabel = NULL;
  if (!proc) return NULL;
  if (copy_old) blabel = lives_strdup(lives_label_get_text(LIVES_LABEL(proc->label)));
  lives_label_set_text(LIVES_LABEL(proc->label), label);
  lives_widget_queue_draw(proc->processing);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  return blabel;
}


/**
   @brief render mainw->event_list to a clip

   the first time, this should be called with reset = TRUE, then always with reset = FALSE for the rest of the rendering
   each subsequent call will handle one event in the event list (the current event is stored internally and updated on each call).

   Video and audio are rendered separately, audio is rendered only when:
   - a new "audio_frame" is reached
   - the end of the list is reached
   - or mainw->flush_audio_tc is set to non-zero,

   The first two cases happen automatically, the latter case may be used if a preview is required, to ensure that audio is rendered
   up to the preview end point.

   the resulting output may be to an existing clip or a new one. When rendering to an existing clip (cfile->old_frames > 0)
   each call may overwrite an existing frame or extend it by (at most) one frame (measured in output fps). On this case rec_tc_delta
   is used to offset input timecodes to output. When outputting to a new clip, rec_tc_delta is ignored and each frame is appended ar
   the next frame timecode

   The event_list is resampled in place, so that out frames coincide with the output frame rate (frames may be dropped or duplicated)
   - generally the list would have been pre-quantised using quantise_events() so that this process is smoothed out. In multitrack, in and
   out frame rates always match so this is not a concern there.

   When rendering to a new clip, the frame width, height, fps and audio values (channels, rate etc) (if rendering audio)
   must be set first.

   the event_list is rendered as-is, with the exception that param changes are not applied if rendering in multitrack, since these are
   interpolated separately. Otherwise, effect inits / deinits, param changes and filter maps are all updated. Separate filter map pointers
   are maintained for video and audio and only filters for that stream are updated.

   When rendering audio it is most efficient to render
   as large blocks as possible. These will be broken into smaller chunks internally and the audio filters are applied and updated using
   these smaller chunks. If rendering audio, the final call should be with mainw->flush_audio_tc set to
   the event_list length + (1. / cfile->fps) * TICKS_PER_SECOND.
   If audio ends before this then it will padded to the end with silence.
   When rendering to an existing clip, the behaviour of audio rendering is undefined.

   The following values (lives_render_error_t) may be returned:
   LIVES_RENDER_READY - returned after the initial call with reset set to TRUE

   LIVES_RENDER_PROCESSING - the next video event / audio block was handled successfully

   LIVES_RENDER_EFFECTS_PAUSED - processing is paused, the internal state is unchanged

   LIVES_RENDER_COMPLETE - the final event in the list was reached

   LIVES_RENDER_ERROR_WRITE_FRAME - the output frame could not be written, and the user declined to retr

   LIVES_RENDER_ERROR_WRITE_AUDIO
   LIVES_RENDER_ERROR_READ_AUDIO
*/
lives_render_error_t render_events(boolean reset, boolean rend_video, boolean rend_audio) {
#define SAVE_THREAD
#ifdef SAVE_THREAD
  static savethread_priv_t *saveargs = NULL;
  static lives_thread_t *saver_thread = NULL;
#else
  char oname[PATH_MAX];
  char *tmp;
  LiVESError *error;
#endif
  static weed_timecode_t rec_delta_tc, atc;
  static weed_plant_t *event, *eventnext;
  static boolean r_audio, r_video;

  weed_timecode_t tc, next_out_tc = 0l, out_tc, dtc = atc, ztc;
  void *init_event, **eevents;

  LiVESPixbuf *pixbuf = NULL;

  weed_plant_t *filter;
  weed_plant_t **citmpl = NULL, **cotmpl = NULL;
  weed_plant_t **bitmpl = NULL, **botmpl = NULL;
  weed_plant_t *inst, *orig_inst;
  weed_plant_t *next_frame_event = NULL;

  int *in_count = NULL;

  weed_plant_t **source_params, **in_params;
  weed_layer_t **layers, *layer = NULL;

  weed_error_t weed_error;
  LiVESResponseType retval;

  int key, idx;
  int etype;
  int layer_palette;
  int num_params, offset = 0;
  int num_in_count = 0;
  int num_in_channels = 0, num_out_channels = 0;
  int mytrack;
  int scrap_track = -1;
  int nev;

  static int progress;
  static int xaclips[MAX_AUDIO_TRACKS];
  static int out_frame;
  static int frame;
  static frames64_t old_scrap_frame;
  static int natracks, nbtracks;
  int blend_file = mainw->blend_file;

  boolean is_blank = TRUE;
  boolean completed = FALSE;
  boolean intimg = FALSE;

  double dt = 0.;
  static double chvols[MAX_AUDIO_TRACKS];
  static boolean xaon[MAX_AUDIO_TRACKS];
  static double xaseek[MAX_AUDIO_TRACKS], xavel[MAX_AUDIO_TRACKS];
  static double atime;

#ifdef VFADE_RENDER
  static weed_timecode_t vfade_in_end;
  static weed_timecode_t vfade_out_start;
  static lives_colRGBA64_t vfade_in_col;
  static lives_colRGBA64_t vfade_out_col;
#endif

  static lives_render_error_t read_write_error;

  static char nlabel[128];

  char *blabel = NULL;
  char *key_string, *com;
  char *filter_name;

  int i, nparams;

  if (reset) {
    LiVESList *list = NULL;
    // reset pchains
    for (i = 0; i < FX_KEYS_MAX; i++) pchains[i] = NULL;

    r_audio = rend_audio;
    r_video = rend_video;
    progress = frame = 1;
    rec_delta_tc = 0;
    event = cfile->next_event;
    if (WEED_EVENT_IS_MARKER(event)) {
      if (weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, &weed_error) == EVENT_MARKER_RECORD_START) {
        if (cfile->old_frames > 0) {
          /// tc delta is only used if we are rendering to an existing clip; otherwise resampling should have removed the event
          /// but just in case, we ignore it
          rec_delta_tc = weed_get_int64_value(event, WEED_LEAF_TCDELTA, NULL);
        }
      }
    }

    /// set audio and video start positions
    atc = q_gint64(get_event_timecode(event) + rec_delta_tc, cfile->fps);
    atime = (double)atc / TICKS_PER_SECOND_DBL;
    out_frame = calc_frame_from_time4(mainw->current_file, atime);

    /// set the highest quality palette conversions
    init_conversions(LIVES_INTENTION_RENDER);

    if (cfile->frames < out_frame) out_frame = cfile->frames + 1;
    cfile->undo_start = out_frame;

    // store these, because if the user previews and there is no audio file yet, values may get reset
    cfile->undo_achans = cfile->achans;
    cfile->undo_arate = cfile->arate;
    cfile->undo_arps = cfile->arps;
    cfile->undo_asampsize = cfile->asampsize;

    // we may have set this to TRUE to stop the audio being clobbered; now we must reset it to get the correct audio filename
    cfile->opening = FALSE;

    clear_mainw_msg();
    mainw->filter_map = NULL;
    mainw->afilter_map = NULL;
    mainw->audio_event = event;
    old_scrap_frame = -2;
    rec_delta_tc = 0;
    /* end_tc */
    /*   = get_event_timecode(get_last_frame_event(cfile->event_list)) */
    /*   + TICKS_PER_SECOND_DBL / cfile->fps; */

#ifdef VFADE_RENDER
    if (r_video) {
      if (mainw->vfade_in_secs > 0.) {
        vfade_in_end = q_gint64(mainw->vfade_in_secs * TICKS_PER_SECOND_DBL, cfile->fps);
        vfade_in_col = mainw->vfade_in_col;
      } else vfade_in_end = 0;
      if (mainw->vfade_out_secs > 0.) {
        vfade_out_start = q_gint64(end_tc - mainw->vfade_out_secs * TICKS_PER_SECOND_DBL, cfile->fps);
        vfade_out_col = mainw->vfade_out_col;
      } else vfade_out_start = end_tc;
    }
#endif

    if (r_audio) {
      /// define the number of backing audio tracks (i.e tracks with audio but no video)
      natracks = nbtracks = 0;
      if (mainw->multitrack && mainw->multitrack->audio_vols) {
        list = mainw->multitrack->audio_vols;
        nbtracks = mainw->multitrack->opts.back_audio_tracks;
      }

      /// set (fixed) volume levels for input audio tracks
      for (i = 0; i < MAX_AUDIO_TRACKS; i++) {
        xaclips[i] = -1;
        xaseek[i] = xavel[i] = 0.;
        xaon[i >> 1] = TRUE;
        if (list) {
          natracks++;
          chvols[i] = (double)LIVES_POINTER_TO_INT(list->data) / 1000000.;
          list = list->next;
        } else chvols[i] = 0.;
      }

      if (!mainw->multitrack) {
        natracks = 1;
        chvols[0] = 1.;
      }
      /// alt label text for when we are rendering audio parts
      lives_snprintf(nlabel, 128, "%s", _("Rendering audio..."));
      read_write_error = LIVES_RENDER_ERROR_NONE;
      audio_free_fnames();
    }
    return LIVES_RENDER_READY;
  }

  if (mainw->effects_paused) return LIVES_RENDER_EFFECTS_PAUSED;

#ifdef USE_LIBPNG
  if (r_video)
    // use internal image saver if we can
    if (cfile->img_type == IMG_TYPE_PNG) intimg = TRUE;
#endif

  if (event && !r_video && r_audio && !mainw->flush_audio_tc) {
    while (event && !(WEED_EVENT_IS_MARKER(event) || WEED_EVENT_IS_AUDIO_FRAME(event))) {
      event = get_next_frame_event(event);
    }
  }

  if (mainw->flush_audio_tc != 0 || event) {
    if (event) etype = get_event_type(event);
    else etype = WEED_EVENT_TYPE_FRAME;
    if (mainw->flush_audio_tc == 0) {
      if (!(!mainw->multitrack && mainw->is_rendering && cfile->old_frames > 0 && out_frame <= cfile->frames)) {
        is_blank = FALSE;
      }
      eventnext = get_next_event(event);
    } else {
      if (etype != WEED_EVENT_TYPE_MARKER)
        etype = WEED_EVENT_TYPE_FRAME;
    }

    if (!r_video && etype != WEED_EVENT_TYPE_FRAME && etype != WEED_EVENT_TYPE_MARKER) etype = WEED_EVENT_TYPE_UNDEFINED;

    switch (etype) {
    case WEED_EVENT_TYPE_MARKER: {
      int marker_type = weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, &weed_error);
      if (marker_type == EVENT_MARKER_RECORD_START) {
        /// tc delta is only used if we are rendering to an existing clip; otherwise resampling should have removed the event
        /// but just in case, we ignore it
        if (cfile->old_frames > 0) {
          ticks_t ztc;
          rec_delta_tc = weed_get_int64_value(event, WEED_LEAF_TCDELTA, NULL);
          ztc = q_gint64(get_event_timecode(event) + rec_delta_tc, cfile->fps);
          atime = (double)ztc / TICKS_PER_SECOND_DBL;
          out_frame = calc_frame_from_time4(mainw->current_file, atime);
        }
      }
    }
    break;
    case WEED_EVENT_TYPE_FRAME:
      eevents = weed_get_voidptr_array_counted(event, LIVES_LEAF_EASING_EVENTS, &nev);
      if (eevents) {
        weed_plant_t *gui;
        for (i = 0; i < nev; i++) {
          init_event = (weed_plant_t *)eevents[i];
          if (init_event) {
            key_string = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, NULL);
            key = atoi(key_string);
            lives_free(key_string);
            inst = rte_keymode_get_instance(key + 1, 0);
            weed_set_boolean_value(inst, LIVES_LEAF_AUTO_EASING, WEED_TRUE);
            gui = weed_instance_get_gui(inst, FALSE);
            if (gui) {
              int easeval = weed_get_int_value(gui, WEED_LEAF_EASE_OUT_FRAMES, NULL);
              if (easeval) weed_set_int_value(gui, WEED_LEAF_EASE_OUT, easeval);
            }
          }
        }
        lives_free(eevents);
      }

      out_tc = (weed_timecode_t)((out_frame - 1) / cfile->fps
                                 * TICKS_PER_SECOND_DBL - rec_delta_tc); // calculate tc of next out frame */
      out_tc = q_gint64(out_tc, cfile->fps);
      next_out_tc = (weed_timecode_t)(out_frame / cfile->fps
                                      * TICKS_PER_SECOND_DBL - rec_delta_tc); // calculate tc of next out frame */
      layer = NULL;
      next_out_tc = q_gint64(next_out_tc, cfile->fps);
      if (mainw->flush_audio_tc == 0) {
        tc = get_event_timecode(event);
        if (r_video && !(!mainw->clip_switched && cfile->hsize * cfile->vsize == 0)) {
          lives_freep((void **)&mainw->clip_index);
          lives_freep((void **)&mainw->frame_index);

          mainw->clip_index = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &mainw->num_tracks);
          mainw->frame_index = weed_get_int64_array(event, WEED_LEAF_FRAMES, &weed_error);

          if (mainw->scrap_file != -1) {
            for (i = 0; i < mainw->num_tracks; i++) {
              if (mainw->clip_index[i] != mainw->scrap_file) {
                scrap_track = -1;
                break;
              }
              if (scrap_track == -1) scrap_track = i;
            }
          }
          if (scrap_track != -1) {
            int64_t offs;
            if (mainw->frame_index[scrap_track] == old_scrap_frame) {
              if (intimg) {
                if (mainw->scrap_layer) {
                  layer = mainw->scrap_layer;
                }
              } else {
                if (mainw->scrap_pixbuf) {
                  pixbuf = mainw->scrap_pixbuf;
                }
              }
            }

            if (!layer && !pixbuf) {
              if (mainw->scrap_layer) {
                if (mainw->scrap_layer != mainw->transrend_layer &&
                    (!saveargs || mainw->scrap_layer != saveargs->layer))
                  weed_layer_free(mainw->scrap_layer);
                mainw->scrap_layer = NULL;
              }
              if (mainw->scrap_pixbuf) {
                lives_widget_object_unref(mainw->scrap_pixbuf);
                mainw->scrap_pixbuf = NULL;
              }
              old_scrap_frame = mainw->frame_index[scrap_track];

              layer = lives_layer_new_for_frame(mainw->clip_index[scrap_track], old_scrap_frame);
              offs = weed_get_int64_value(event, WEED_LEAF_HOST_SCRAP_FILE_OFFSET, &weed_error);
              if (!mainw->files[mainw->scrap_file]->ext_src) load_from_scrap_file(NULL, -1);
              else {
                lives_lseek_buffered_rdonly_absolute(LIVES_POINTER_TO_INT(mainw->files[mainw->clip_index[scrap_track]]->ext_src),
                                                     offs);
                if (!pull_frame(layer, get_image_ext_for_type(cfile->img_type), tc)) {
                  weed_layer_free(layer);
                  layer = NULL;
                }
              }
            }
          } else {
            int oclip, nclip;
            layers = (weed_plant_t **)lives_malloc((mainw->num_tracks + 1) * sizeof(weed_plant_t *));
            lives_memset(mainw->ext_src_used, 0, MAX_FILES * sizint);

            // get list of active tracks from mainw->filter map
            get_active_track_list(mainw->clip_index, mainw->num_tracks, mainw->filter_map);

            for (i = 0; i < mainw->num_tracks; i++) {
              oclip = mainw->old_active_track_list[i];
              if (oclip > 0 && oclip == (nclip = mainw->active_track_list[i])) {
                // check if ext_src survives old->new
                if (mainw->track_decoders[i] == mainw->files[oclip]->ext_src) {
                  mainw->ext_src_used[oclip] = TRUE;
                }
              }
            }

            for (i = 0; i < mainw->num_tracks; i++) {
              if (mainw->clip_index[i] > 0 && mainw->frame_index[i] > 0 && mainw->multitrack) {
                is_blank = FALSE;
              }
              layers[i] = lives_layer_new_for_frame(mainw->clip_index[i], mainw->frame_index[i]);
              //weed_layer_nullify_pixel_data(layers[i]);

              weed_layer_ref(layers[i]);
              weed_set_int_value(layers[i], WEED_LEAF_CURRENT_PALETTE, (mainw->clip_index[i] == -1 ||
                                 mainw->files[mainw->clip_index[i]]->img_type ==
                                 IMG_TYPE_JPEG) ? WEED_PALETTE_RGB24 : WEED_PALETTE_RGBA32);

              if ((oclip = mainw->old_active_track_list[i]) != (nclip = mainw->active_track_list[i])) {
                // now using threading, we want to start pulling all pixel_data for all active layers here
                // however, we may have more than one copy of the same clip - in this case we want to
                // create clones of the decoder plugin
                // this is to prevent constant seeking between different frames in the clip

                // check if ext_src survives old->new

                ////
                if (mainw->track_decoders[i]) track_decoder_free(i, oclip, nclip);

                if (IS_VALID_CLIP(nclip)) {
                  lives_clip_t *sfile = mainw->files[nclip];
                  if (sfile->clip_type == CLIP_TYPE_FILE) {
                    if (!mainw->ext_src_used[nclip]) {
                      mainw->track_decoders[i] = (lives_decoder_t *)sfile->ext_src;
                    }
                    if (mainw->track_decoders[i] == (lives_decoder_t *)sfile->ext_src) {
                      mainw->ext_src_used[nclip] = TRUE;
                    } else {
                      // add new clone for nclip
                      int nsrcs = sfile->n_altsrcs;
                      mainw->track_decoders[i] = clone_decoder(nclip);
                      g_print("CLONING\n");
                      sfile->alt_srcs = lives_realloc(sfile->alt_srcs, (nsrcs + 1) * sizeof(void *));
                      sfile->alt_srcs[nsrcs] = mainw->track_decoders[i];
                      sfile->alt_src_types = lives_realloc(sfile->alt_src_types, (nsrcs + 1) * sizint);
                      sfile->alt_src_types[nsrcs] = LIVES_EXT_SRC_DECODER;
                      sfile->n_altsrcs++;
                    }
                  }
                  // set alt src in layer
                  weed_set_voidptr_value(layers[i], WEED_LEAF_HOST_DECODER,
                                         (void *)mainw->track_decoders[i]);
                } else weed_layer_pixel_data_free(layers[i]);
                mainw->old_active_track_list[i] = mainw->active_track_list[i];
              }
            }

            layers[i] = NULL;

            if (weed_plant_has_leaf(event, LIVES_LEAF_FAKE_TC))
              ztc = weed_get_int64_value(event, LIVES_LEAF_FAKE_TC, NULL);
            else ztc = tc;

            layer = weed_apply_effects(layers, mainw->filter_map, ztc,
                                       cfile->hsize, cfile->vsize, pchains);

            for (i = 0; layers[i]; i++) {
              weed_layer_unref(layers[i]);
              if (layer != layers[i]) {
                check_layer_ready(layers[i]);
                weed_layer_free(layers[i]);
              }
            }
            lives_free(layers);
          }

#ifdef VFADE_RENDER
          if (layer) {
            double fadeamt;
            if (out_tc < vfade_in_end) {
              fadeamt = (double)(vfade_in_end - out_tc) / (double)vfade_in_end;
              weed_set_int_value(layer, "red_adjust", (double)vfade_in_col.red / 255.);
              weed_set_int_value(layer, "green_adjust", (double)vfade_in_col.green / 255.);
              weed_set_int_value(layer, "blue_adjust", (double)vfade_in_col.blue / 255.);
              weed_set_double_value(layer, "colorize", fadeamt);
            }
            if (out_tc > vfade_out_start) {
              fadeamt = (double)(out_tc - vfade_out_start) / (double)(end_tc - vfade_out_start);
              weed_set_int_value(layer, "red_adjust", (double)vfade_in_col.red / 255.);
              weed_set_int_value(layer, "green_adjust", (double)vfade_in_col.green / 255.);
              weed_set_int_value(layer, "blue_adjust", (double)vfade_in_col.blue / 255.);
              weed_set_double_value(layer, "colorize", fadeamt);
            }
          }
#endif
          if (layer && weed_layer_get_width(layer) > 0) {
            int lpal, width, height;
            boolean was_lbox = FALSE;
            if (THREAD_INTENTION == LIVES_INTENTION_TRANSCODE) {
              if (lives_proc_thread_check_finished(mainw->transrend_proc)) return LIVES_RENDER_ERROR;

              lives_nanosleep_while_true(mainw->transrend_ready);
              if (lives_proc_thread_check_finished(mainw->transrend_proc)) return LIVES_RENDER_ERROR;

              if (mainw->transrend_layer) {
                if (mainw->transrend_layer != mainw->scrap_layer)
                  weed_layer_free(mainw->transrend_layer);
                mainw->transrend_layer = NULL;
              }

              if (scrap_track != -1) {
                if (layer == mainw->scrap_layer) {
                  // hmmm
                } else {
                  check_layer_ready(layer);
                  if (prefs->enc_letterbox) {
                    int lb_width = weed_layer_get_width_pixels(layer);
                    int lb_height = weed_layer_get_height(layer);
                    calc_maxspect(cfile->hsize, cfile->vsize, &lb_width, &lb_height);
                    lb_width = (lb_width >> 2) << 2;
                    letterbox_layer(layer, cfile->hsize, cfile->vsize, lb_width, lb_height,
                                    LIVES_INTERP_BEST, WEED_PALETTE_NONE, 0);
                  } else resize_layer(layer, cfile->hsize, cfile->vsize, LIVES_INTERP_BEST, WEED_PALETTE_NONE, 0);
                }
                // if our pixbuf came from scrap file, and next frame is also from scrap file with same frame number,
                next_frame_event = get_next_frame_event(event);
                if (weed_get_int_value(next_frame_event, WEED_LEAF_CLIPS, NULL) == mainw->clip_index[0]
                    && weed_get_int64_value(next_frame_event, WEED_LEAF_FRAMES, NULL)
                    == mainw->frame_index[0]) {
                  // save the layer
                  if (!mainw->scrap_layer) {
                    // NOT WORKING....TODO ???
                    //mainw->scrap_layer = weed_layer_copy(NULL, layer);
                  }
                }
              }
              weed_leaf_dup(layer, event, LIVES_LEAF_FAKE_TC);
              if (prefs->twater_type == TWATER_TYPE_DIAGNOSTICS) {
                if (weed_plant_has_leaf(event, WEED_LEAF_OVERLAY_TEXT)) {
                  char *texto = weed_get_string_value(event, WEED_LEAF_OVERLAY_TEXT, NULL);
                  render_text_overlay(layer, texto, DEF_OVERLAY_SCALING);
                  lives_free(texto);
                }
              }
              mainw->transrend_layer = layer;
              mainw->transrend_ready = TRUE;
              // sig_progress...
              lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%d", progress++);
              break;
            }
            check_layer_ready(layer);
            width = weed_layer_get_width(layer) * weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(layer));
            height = weed_layer_get_height(layer);
            lpal = layer_palette = weed_layer_get_palette(layer);
#ifndef ALLOW_PNG24
            if (cfile->img_type == IMG_TYPE_JPEG && layer_palette != WEED_PALETTE_RGB24
                && layer_palette != WEED_PALETTE_RGBA32)
              layer_palette = WEED_PALETTE_RGB24;

            else if (cfile->img_type == IMG_TYPE_PNG && layer_palette != WEED_PALETTE_RGBA32)
              layer_palette = WEED_PALETTE_RGBA32;
#else
            layer_palette = WEED_PALETTE_RGB24;
#endif
            // TODO - enc lb
            if ((mainw->multitrack && prefs->letterbox_mt) || (prefs->letterbox && !mainw->multitrack)) {
              calc_maxspect(cfile->hsize, cfile->vsize, &width, &height);
              if (layer_palette != lpal && (cfile->hsize > width || cfile->vsize > height)) {
                convert_layer_palette(layer, layer_palette, 0);
              }
              letterbox_layer(layer, cfile->hsize, cfile->vsize, width, height, LIVES_INTERP_BEST, layer_palette, 0);
              was_lbox = TRUE;
            } else {
              resize_layer(layer, cfile->hsize, cfile->vsize, LIVES_INTERP_BEST, layer_palette, 0);
            }

            convert_layer_palette(layer, layer_palette, 0);

            // we have a choice here, we can either render with the same gamma tf as cfile, or force it to sRGB
            if (!was_lbox)
              gamma_convert_layer(cfile->gamma_type, layer);
            else {
              gamma_convert_sub_layer(cfile->gamma_type, 1.0, layer, (cfile->hsize - width) / 2,
                                      (cfile->vsize - height) / 2,
                                      width, height, TRUE);
            }
            if (weed_plant_has_leaf(event, WEED_LEAF_OVERLAY_TEXT)) {
              char *texto = weed_get_string_value(event, WEED_LEAF_OVERLAY_TEXT, NULL);
              render_text_overlay(layer, texto, DEF_OVERLAY_SCALING);
              lives_free(texto);
            }
            if (!intimg) {
              pixbuf = layer_to_pixbuf(layer, TRUE, FALSE);
              weed_layer_free(layer);
              layer = NULL;
            }
          }
          track_decoder_free(1, mainw->blend_file, blend_file);
          mainw->blend_file = blend_file;
        }
        next_frame_event = get_next_frame_event(event);
        tc = get_event_timecode(event);
      } else tc = mainw->flush_audio_tc;

      if (r_audio && (!next_frame_event || WEED_EVENT_IS_AUDIO_FRAME(event)
                      || (mainw->flush_audio_tc != 0 && tc > mainw->flush_audio_tc)) && tc >= atc) {
        int auditracks;
        for (auditracks = 0; auditracks < MAX_AUDIO_TRACKS; auditracks++) {
          // see if we have any audio to render
          if (xavel[auditracks] != 0.) break;
        }

        cfile->achans = cfile->undo_achans;
        cfile->arate = cfile->undo_arate;
        cfile->arps = cfile->undo_arps;
        cfile->asampsize = cfile->undo_asampsize;

        blabel = set_proc_label(mainw->proc_ptr, nlabel, TRUE);

        lives_freep((void **)&THREADVAR(read_failed_file));

        if (mainw->flush_audio_tc != 0) dtc = mainw->flush_audio_tc;
        else {
          if (r_video && !next_frame_event && is_blank) tc -= 1. / cfile->fps * TICKS_PER_SECOND_DBL;
          dtc = q_gint64(tc + rec_delta_tc, cfile->fps);
        }

        if (auditracks < MAX_AUDIO_TRACKS) {
          // render audio
          render_audio_segment(natracks, xaclips, mainw->multitrack != NULL ? mainw->multitrack->render_file :
                               mainw->current_file, xavel, xaseek, atc, dtc, chvols, 1., 1., NULL);
        } else {
          // render silence
          render_audio_segment(1, NULL, mainw->multitrack != NULL ? mainw->multitrack->render_file : mainw->current_file,
                               NULL, NULL, atc, dtc, chvols, 0., 0., NULL);
        }

        dt = (dtc - atc) / TICKS_PER_SECOND_DBL;
        atc = dtc;

        if (THREADVAR(write_failed)) {
          int outfile = (mainw->multitrack ? mainw->multitrack->render_file : mainw->current_file);
          char *outfilename = lives_get_audio_file_name(outfile);
          do_write_failed_error_s(outfilename, NULL);
          lives_free(outfilename);
          read_write_error = LIVES_RENDER_ERROR_WRITE_AUDIO;
        }

        if (THREADVAR(read_failed)) {
          do_read_failed_error_s(THREADVAR(read_failed_file), NULL);
          read_write_error = LIVES_RENDER_ERROR_READ_AUDIO;
        }

        set_proc_label(mainw->proc_ptr, blabel, FALSE);
        lives_freep((void **)&blabel);

        if (mainw->flush_audio_tc != 0) {
          if (read_write_error) return read_write_error;
          return LIVES_RENDER_COMPLETE;
        } else {
          int *aclips = NULL;
          double *aseeks = NULL;
          int num_aclips = weed_frame_event_get_audio_tracks(event, &aclips, &aseeks);
          boolean nointer = FALSE;

          if (weed_get_int_value(event, LIVES_LEAF_SCRATCH, NULL) != SCRATCH_NONE) nointer = TRUE;

          for (i = 0; i < num_aclips; i += 2) {
            if (aclips[i + 1] > 0) { // clipnum
              double mult = 1.0;
              mytrack = aclips[i] + nbtracks;
              if (mytrack < 0) mytrack = 0;
              //g_print("del was %f\n", xaseek[mytrack] - aseeks[i]);
              if (!nointer && prefs->rr_super && prefs->rr_ramicro
                  && weed_get_boolean_value(event, LIVES_LEAF_ALLOW_JUMP, NULL) != WEED_TRUE) {
                /// smooth out audio by ignoring tiny seek differences
                xaseek[mytrack] += xavel[mytrack] * dt;
                if (xavel[mytrack] * aseeks[i + 1] < 0.) mult *= AUD_DIFF_REVADJ;
                if (xaclips[mytrack] != aclips[i + 1] || xaon[i >> 1]
                    || fabs(xaseek[mytrack] - aseeks[i]) > AUD_DIFF_MIN * mult) {
                  xaseek[mytrack] = aseeks[i];
                }
              } else xaseek[mytrack] = aseeks[i];
              if (xaclips[mytrack] != aclips[i + 1]) xaon[mytrack] = TRUE;
              else xaon[mytrack] = TRUE;
              xaclips[mytrack] = aclips[i + 1];
              xavel[mytrack] = aseeks[i + 1];
            }
            lives_freep((void **)&aseeks);
            lives_freep((void **)&aclips);
          }
        }
      }

      if (!r_video) break;
      /////////////////
      if (intimg) {
        if (!layer) {
          break_me("miss lay");
          break;
        }
      } else {
        if (!pixbuf) {
          break_me("miss pix");
          break;
        }
      }
      ////////////////////////

      if (!next_frame_event && is_blank) {
        next_out_tc = out_tc;
        break; // don't render final blank frame
      }

      if (next_frame_event) {
        weed_timecode_t next_tc = get_event_timecode(next_frame_event);
        if (next_tc < next_out_tc || next_tc - next_out_tc < next_out_tc - tc) break;
      } else if (next_out_tc > tc) break;

#ifndef SAVE_THREAD
      if (cfile->old_frames > 0) {
        tmp = make_image_file_name(cfile, out_frame, LIVES_FILE_EXT_MGK);
      } else {
        tmp = make_image_file_name(cfile, out_frame, get_image_ext_for_type(cfile->img_type));
      }
      lives_snprintf(oname, PATH_MAX, "%s", tmp);
      lives_free(tmp);

      do {
        retval = LIVES_RESPONSE_NONE;
        lives_pixbuf_save(pixbuf, oname, cfile->img_type, 100 - prefs->ocp, cfile->hsize, cfile->vsize, NULL);

        if (error) {
          retval = do_write_failed_error_s_with_retry(oname, error->message, NULL);
          lives_error_free(error);
          error = NULL;
          if (retval != LIVES_RESPONSE_RETRY) read_write_error = LIVES_RENDER_ERROR_WRITE_FRAME;
        }
      } while (retval == LIVES_RESPONSE_RETRY);

#else

      if (!saver_thread) {
        if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE) {
          saveargs = (savethread_priv_t *)lives_calloc(1, sizeof(savethread_priv_t));
          saveargs->img_type = cfile->img_type;
          saveargs->compression = 100 - prefs->ocp;
          saveargs->width = cfile->hsize;
          saveargs->height = cfile->vsize;
          saver_thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
        }
      } else {
        lives_thread_join(*saver_thread, NULL);
        while (saveargs->error || THREADVAR(write_failed)) {
          if (saveargs->error) {
            retval = do_write_failed_error_s_with_retry(saveargs->fname, saveargs->error->message);
            lives_error_free(saveargs->error);
            saveargs->error = NULL;
          } else {
            retval = do_write_failed_error_s_with_retry(saveargs->fname, NULL);
          }
          THREADVAR(write_failed) = 0;
          if (retval != LIVES_RESPONSE_RETRY) {
            read_write_error = LIVES_RENDER_ERROR_WRITE_FRAME;
            break;
          }
          if (intimg) save_to_png_threaded((void *)saveargs);
          else {
            lives_pixbuf_save(saveargs->pixbuf, saveargs->fname, saveargs->img_type, saveargs->compression,
                              saveargs->width, saveargs->height, &saveargs->error);
          }
        }
        if (saveargs->layer && saveargs->layer != layer) {
          if (saveargs->layer != mainw->scrap_layer)
            weed_layer_free(saveargs->layer);
          saveargs->layer = NULL;
        }
        if (saveargs->pixbuf && saveargs->pixbuf != pixbuf) {
          if (saveargs->pixbuf == mainw->scrap_pixbuf) mainw->scrap_pixbuf = NULL;
          lives_widget_object_unref(saveargs->pixbuf);
          saveargs->pixbuf = NULL;
        }
        lives_free(saveargs->fname);
        saveargs->fname = NULL;
      }
      if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE) {
        if (cfile->old_frames > 0) {
          saveargs->fname = make_image_file_name(cfile, out_frame, LIVES_FILE_EXT_MGK);
        } else {
          saveargs->fname = make_image_file_name(cfile, out_frame, get_image_ext_for_type(cfile->img_type));
        }

        if (intimg) {
          saveargs->layer = layer;
          lives_thread_create(saver_thread, LIVES_THRDATTR_NONE, save_to_png_threaded, saveargs);
        } else {
          saveargs->pixbuf = pixbuf;
          lives_thread_create(saver_thread, LIVES_THRDATTR_NONE, lives_pixbuf_save_threaded, saveargs);
        }
      }
#endif

      // sig_progress...
      lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%d", progress++);

      if (cfile->undo_start == -1) cfile->undo_start = out_frame;
      cfile->undo_end = out_frame;
      if (out_frame > cfile->frames) cfile->frames = out_frame;
      if (out_frame > cfile->end) cfile->end = out_frame;
      if (cfile->start == 0) cfile->start = 1;
      out_frame++;

      if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE) {
        // if our pixbuf came from scrap file, and next frame is also from scrap file with same frame number,
        // save the pixbuf and re-use it
        if (scrap_track != -1) {
          if (intimg) {
            mainw->scrap_layer = layer;
          } else {
            mainw->scrap_pixbuf = pixbuf;
          }
        } else {
          if (mainw->scrap_layer) {
            weed_layer_free(mainw->scrap_layer);
            mainw->scrap_layer = NULL;
          }
          if (mainw->scrap_pixbuf) {
            lives_widget_object_unref(saveargs->pixbuf);
            mainw->scrap_pixbuf = NULL;
          }
        }
      }
      break;

    case WEED_EVENT_TYPE_FILTER_INIT:
      // effect init
      //  bind the weed_fx to next free key/0

      filter_name = weed_get_string_value(event, WEED_LEAF_FILTER, &weed_error);
      // for now, assume we can find hashname
      idx = weed_get_idx_for_hashname(filter_name, TRUE);
      lives_free(filter_name);

      filter = get_weed_filter(idx);
      if (is_pure_audio(filter, FALSE)) {
        //if (weed_plant_has_leaf(event, WEED_LEAF_HOST_TAG)) weed_leaf_delete(event, WEED_LEAF_HOST_TAG);
        break; // audio effects are processed in the audio renderer
      }

      key = get_next_free_key();
      weed_add_effectkey_by_idx(key + 1, idx);
      key_string = lives_strdup_printf("%d", key);
      weed_set_string_value(event, WEED_LEAF_HOST_TAG, key_string);
      lives_free(key_string);

      if (weed_plant_has_leaf(event, WEED_LEAF_IN_COUNT)) {
        in_count = weed_get_int_array_counted(event, WEED_LEAF_IN_COUNT, &num_in_count);
      }

      citmpl = weed_get_plantptr_array_counted(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, &num_in_channels);
      if (num_in_channels != num_in_count) {
        LIVES_ERROR("num_in_count != num_in_channels");
      } else {
        if (num_in_channels > 0) {
          bitmpl = (weed_plant_t **)lives_malloc(num_in_channels * sizeof(weed_plant_t *));
          for (i = 0; i < num_in_channels; i++) {
            bitmpl[i] = weed_plant_copy(citmpl[i]);
            if (in_count[i] > 0) {
              weed_set_boolean_value(citmpl[i], WEED_LEAF_HOST_DISABLED, WEED_FALSE);
              weed_set_int_value(citmpl[i], WEED_LEAF_HOST_REPEATS, in_count[i]);
            } else weed_set_boolean_value(citmpl[i], WEED_LEAF_HOST_DISABLED, WEED_TRUE);
          }
        }
      }

      lives_freep((void **)&in_count);

      cotmpl = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &num_out_channels);
      if (num_out_channels > 0) {
        botmpl = (weed_plant_t **)lives_malloc(num_out_channels * sizeof(weed_plant_t *));
        for (i = 0; i < num_out_channels; i++) {
          botmpl[i] = weed_plant_copy(cotmpl[i]);
          if (!weed_plant_has_leaf(cotmpl[i], WEED_LEAF_HOST_DISABLED))
            weed_set_boolean_value(cotmpl[i], WEED_LEAF_HOST_DISABLED, WEED_FALSE);
        }
      }

      THREADVAR(random_seed) = weed_get_int64_value(event, WEED_LEAF_RANDOM_SEED, NULL);

      weed_init_effect(key);

      // restore channel state / number from backup

      if (num_in_channels > 0) {
        for (i = 0; i < num_in_channels; i++) {
          weed_leaf_copy_or_delete(citmpl[i], WEED_LEAF_HOST_DISABLED, bitmpl[i]);
          weed_leaf_copy_or_delete(citmpl[i], WEED_LEAF_HOST_REPEATS, bitmpl[i]);
          weed_plant_free(bitmpl[i]);
        }
        lives_free(bitmpl);
        lives_free(citmpl);
      }

      if (num_out_channels > 0) {
        for (i = 0; i < num_out_channels; i++) {
          weed_leaf_copy_or_delete(cotmpl[i], WEED_LEAF_HOST_DISABLED, botmpl[i]);
          weed_leaf_copy_or_delete(cotmpl[i], WEED_LEAF_HOST_REPEATS, botmpl[i]);
          weed_plant_free(botmpl[i]);
        }
        lives_free(botmpl);
        lives_free(cotmpl);
      }

      // reinit effect with saved parameters
      orig_inst = inst = rte_keymode_get_instance(key + 1, 0);

      if (weed_plant_has_leaf(event, WEED_LEAF_IN_PARAMETERS)) {
        void **xpchains = weed_get_voidptr_array_counted(event, WEED_LEAF_IN_PARAMETERS, &nparams);
        pchains[key] = (void **)lives_realloc(pchains[key], (nparams + 1) * sizeof(void *));
        for (i = 0; i < nparams; i++) pchains[key][i] = xpchains[i];
        pchains[key][nparams] = NULL;
        lives_free(xpchains);
      } else pchains[key] = NULL;

filterinit2:

      num_params = num_in_params(inst, FALSE, FALSE);

      if (num_params > 0) {
        weed_call_deinit_func(inst);
        if (weed_plant_has_leaf(event, WEED_LEAF_IN_PARAMETERS)) {
          source_params = (weed_plant_t **)pchains[key];
          if (source_params) {
            in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
            for (i = 0; i < num_params && i < nparams; i++) {
              if (source_params && source_params[i + offset]
                  && is_init_pchange(event, source_params[i + offset]))
                weed_leaf_copy(in_params[i], WEED_LEAF_VALUE, source_params[i + offset], WEED_LEAF_VALUE);
            }
            lives_free(in_params);
          }
        }
        offset += num_params;
        weed_call_init_func(inst);
      }

      if (weed_plant_has_leaf(event, WEED_LEAF_HOST_KEY)) {
        // mt events will not have this;
        // it is used to connect params and alpha channels during rendering
        // holds our original key/mode values

        int hostkey = weed_get_int_value(event, WEED_LEAF_HOST_KEY, &weed_error);
        int hostmode = weed_get_int_value(event, WEED_LEAF_HOST_MODE, &weed_error);

        weed_set_int_value(inst, WEED_LEAF_HOST_KEY, hostkey);
        weed_set_int_value(inst, WEED_LEAF_HOST_MODE, hostmode);
      }

      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE)) {
        // handle compound fx
        inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, &weed_error);
        goto filterinit2;
      }

      weed_instance_unref(orig_inst);
      THREADVAR(random_seed) = 0;

      break;
    case WEED_EVENT_TYPE_FILTER_DEINIT:
      init_event = weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, &weed_error);
      if (!init_event) break;
      filter_name = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_FILTER, &weed_error);
      // for now, assume we can find hashname
      idx = weed_get_idx_for_hashname(filter_name, TRUE);
      lives_free(filter_name);

      filter = get_weed_filter(idx);
      if (is_pure_audio(filter, FALSE)) break; // audio effects are processed in the audio renderer

      key_string = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, &weed_error);
      key = atoi(key_string);
      lives_free(key_string);
      if ((inst = rte_keymode_get_instance(key + 1, 0))) {
        weed_deinit_effect(key);
        weed_delete_effectkey(key, 0);
        weed_instance_unref(inst);
      }
      if (pchains[key]) lives_free(pchains[key]);
      pchains[key] = NULL;
      break;
    case WEED_EVENT_TYPE_PARAM_CHANGE:
      if (!mainw->multitrack) {
        init_event = weed_get_voidptr_value((weed_plant_t *)event, WEED_LEAF_INIT_EVENT, NULL);
        if (weed_plant_has_leaf((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG)) {
          key_string = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, NULL);
          key = atoi(key_string);
          lives_free(key_string);

          filter_name = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_FILTER, NULL);
          idx = weed_get_idx_for_hashname(filter_name, TRUE);
          lives_free(filter_name);

          filter = get_weed_filter(idx);

          if (is_pure_audio(filter, FALSE)) break; // audio effects are processed in the audio renderer

          if ((inst = rte_keymode_get_instance(key + 1, 0))) {
            int pnum = weed_get_int_value(event, WEED_LEAF_INDEX, NULL);
            weed_plant_t *param = weed_inst_in_param(inst, pnum, FALSE, FALSE);
            weed_leaf_dup(param, event, WEED_LEAF_VALUE);
            weed_instance_unref(inst);
          }
        }
      }
      break;
    case WEED_EVENT_TYPE_FILTER_MAP:
#ifdef DEBUG_EVENTS
      g_print("got new effect map\n");
#endif
      mainw->filter_map = event;
      break;
    default: break;
    }
    event = eventnext;
  } else {
    /// no more events or audio to flush, rendering complete
#ifdef SAVE_THREAD
    if (saver_thread) {
      lives_thread_join(*saver_thread, NULL);
      while (saveargs->error) {
        retval = do_write_failed_error_s_with_retry(saveargs->fname, saveargs->error->message);
        lives_error_free(saveargs->error);
        saveargs->error = NULL;
        if (retval != LIVES_RESPONSE_RETRY) read_write_error = LIVES_RENDER_ERROR_WRITE_FRAME;
        else {
          if (intimg) save_to_png_threaded((void *)saveargs);
          else {
            lives_pixbuf_save(saveargs->pixbuf, saveargs->fname, saveargs->img_type, saveargs->compression,
                              saveargs->width, saveargs->height, &saveargs->error);
          }
        }
      }
      if (saveargs->layer && saveargs->layer != mainw->transrend_layer) {
        weed_layer_free(saveargs->layer);
        if (saveargs->layer == mainw->scrap_layer) mainw->scrap_layer = NULL;
      }
      if (saveargs->pixbuf) {
        lives_widget_object_unref(saveargs->pixbuf);
        if (saveargs->pixbuf == mainw->scrap_pixbuf) mainw->scrap_pixbuf = NULL;
      }
      lives_freep((void **)&saveargs->fname);
      lives_free(saveargs);
      lives_free(saver_thread);
      saver_thread = NULL;
      saveargs = NULL;
    }
#endif

    if (mainw->transrend_layer) {
      lives_nanosleep_while_true(mainw->transrend_ready);
      weed_layer_free(mainw->transrend_layer);
      mainw->transrend_layer = NULL;
    }

    if (cfile->old_frames == 0) cfile->undo_start = cfile->undo_end = 0;
    if (r_video) {
      com = lives_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\"", prefs->backend, cfile->handle, cfile->undo_start,
                                cfile->undo_end, get_image_ext_for_type(cfile->img_type));

      lives_rm(cfile->info_file);
      mainw->error = FALSE;
      mainw->cancelled = CANCEL_NONE;

      lives_system(com, FALSE);
      lives_free(com);

      mainw->is_rendering = mainw->internal_messaging = FALSE;

      if (THREADVAR(com_failed)) {
        read_write_error = LIVES_RENDER_ERROR_WRITE_FRAME;
        // cfile->may_be_damaged = TRUE;
      } else lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "completed");
    }

    if (r_audio) {
      next_out_tc = q_gint64(get_event_timecode(get_last_frame_event(mainw->event_list)) + rec_delta_tc,
                             cfile->fps);
      render_audio_segment(natracks, xaclips, mainw->multitrack != NULL ? mainw->multitrack->render_file :
                           mainw->current_file, xavel, xaseek, atc, next_out_tc, chvols, 1., 1., NULL);
      cfile->afilesize = reget_afilesize_inner(mainw->current_file);
    }
    mainw->filter_map = NULL;
    mainw->afilter_map = NULL;
    completed = TRUE;
  }

  if (read_write_error) return read_write_error;
  if (completed) return LIVES_RENDER_COMPLETE;
  return LIVES_RENDER_PROCESSING;
}


lives_render_error_t render_events_cb(boolean dummy) {
  /// values ignored if first param is FALSE
  return render_events(FALSE, FALSE, FALSE);
}


static void *do_xdg_opt(lives_object_t *obj, void *data) {
  LiVESToggleButton *cb = (LiVESToggleButton *)data;
  if (lives_toggle_button_get_active(cb)) {
    char *tmp, *com = lives_strdup_printf("%s \"%s\"", EXEC_XDG_OPEN,
                                          (tmp = U82L(cfile->save_file_name)));
    lives_system(com, TRUE);
    lives_free(com); lives_free(tmp);
    lives_widget_object_unref(cb);
  }
  return NULL;
}

static void *add_xdg_opt(lives_object_t *obj, livespointer data) {
  if (check_for_executable(&capable->has_xdg_open, EXEC_XDG_OPEN) == PRESENT) {
    LiVESWidget *cb = lives_standard_check_button_new(_("Preview in default video player afterwards"),
                      FALSE, LIVES_BOX(widget_opts.last_container), NULL);
    lives_widget_object_ref(cb);
    lives_hook_append(THREADVAR(hook_closures), TX_DONE_HOOK, HOOK_CB_SINGLE_SHOT, do_xdg_opt, cb);
  }
  return NULL;
}


boolean start_render_effect_events(weed_plant_t *event_list, boolean render_vid, boolean render_aud) {
  // this is called to begin rendering effect events from an event_list into cfile
  // it will do a reorder/resample/resize/effect apply all in one pass

  // return FALSE in case of serious error

  double old_pb_fps = cfile->pb_fps;

  int oundo_start = cfile->undo_start;
  int oundo_end = cfile->undo_end;

  char *com;

  if (!event_list || (!render_vid && !render_aud)) return TRUE; //oh, that was easy !

  mainw->is_rendering = mainw->internal_messaging = TRUE;
  cfile->next_event = get_first_event(event_list);

  mainw->effort = -EFFORT_RANGE_MAX;

  mainw->progress_fn = &render_events_cb;
  render_events(TRUE, render_vid, render_aud);

  cfile->progress_start = 1;
  cfile->progress_end = count_resampled_events(event_list, cfile->fps);

  cfile->pb_fps = 1000000.;

  cfile->redoable = cfile->undoable = FALSE;
  lives_widget_set_sensitive(mainw->redo, FALSE);
  lives_widget_set_sensitive(mainw->undo, FALSE);

  cfile->undo_action = UNDO_RENDER;

  // clear up any leftover old files
  com = lives_strdup_printf("%s clear_tmp_files \"%s\"", prefs->backend, cfile->handle);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE)
    mainw->disk_mon = MONITOR_QUOTA;
  else cfile->nopreview = TRUE;

  if (cfile->old_frames > 0) cfile->nopreview = TRUE; /// FIXME...

  if (THREAD_INTENTION == LIVES_INTENTION_TRANSCODE && render_vid) {
    lives_hook_append(THREADVAR(hook_closures), TX_START_HOOK, HOOK_CB_SINGLE_SHOT, add_xdg_opt, NULL);
  }

  // play back the file as fast as possible, each time calling render_events()
  if ((!do_progress_dialog(TRUE, TRUE, render_vid ? (THREAD_INTENTION == LIVES_INTENTION_RENDER ? _("Rendering")
                           : _("Transcoding")) : _("Pre-rendering audio"))
       && mainw->cancelled != CANCEL_KEEP) || mainw->error ||
      mainw->render_error >= LIVES_RENDER_ERROR
     ) {
    mainw->disk_mon = 0;
    mainw->cancel_type = CANCEL_KILL;
    mainw->cancelled = CANCEL_NONE;
    cfile->nopreview = FALSE;

    if (mainw->error) {
      widget_opts.non_modal = TRUE;
      do_error_dialog(mainw->msg);
      widget_opts.non_modal = FALSE;
      d_print_failed();
    } else if (mainw->render_error >= LIVES_RENDER_ERROR) d_print_failed();
    cfile->undo_start = oundo_start;
    cfile->undo_end = oundo_end;
    cfile->pb_fps = old_pb_fps;
    cfile->frames = cfile->old_frames;
    mainw->internal_messaging = FALSE;
    mainw->resizing = FALSE;
    cfile->next_event = NULL;
    return FALSE;
  }

  cfile->nopreview = FALSE;
  mainw->disk_mon = 0;
  mainw->cancel_type = CANCEL_KILL;
  mainw->cancelled = CANCEL_NONE;
  cfile->changed = TRUE;
  reget_afilesize(mainw->current_file);
  get_total_time(cfile);

  if (CLIP_TOTAL_TIME(mainw->current_file) == 0.) {
    d_print(_("nothing rendered.\n"));
    return FALSE;
  }

  lives_widget_set_sensitive(mainw->undo, TRUE);
  cfile->undoable = TRUE;
  cfile->pb_fps = old_pb_fps;
  lives_widget_set_sensitive(mainw->select_last, TRUE);
  set_undoable(_("rendering"), TRUE);
  cfile->next_event = NULL;
  return TRUE;
}


int count_events(weed_plant_t *event_list, boolean all_events, weed_timecode_t start_tc, weed_timecode_t end_tc) {
  weed_plant_t *event;
  weed_timecode_t tc;
  int i = 0;

  if (!event_list) return 0;
  event = get_first_event(event_list);

  while (event) {
    tc = get_event_timecode(event);
    if ((all_events || (WEED_EVENT_IS_FRAME(event) && !WEED_EVENT_IS_AUDIO_FRAME(event))) &&
        (end_tc == 0 || (tc >= start_tc && tc < end_tc))) i++;
    event = get_next_event(event);
  }
  return i;
}


frames_t count_resampled_events(weed_plant_t *event_list, double fps) {
  weed_plant_t *event;
  weed_timecode_t tc, seg_start_tc = 0, seg_end_tc = 0;

  frames_t rframes = 0;
  int etype, marker_type;

  boolean seg_start = FALSE;

  if (!event_list) return 0;
  event = get_first_event(event_list);

  while (event) {
    etype = get_event_type(event);
    if (etype == WEED_EVENT_TYPE_FRAME) {
      tc = get_event_timecode(event);
      if (!seg_start) {
        seg_start_tc = seg_end_tc = tc;
        seg_start = TRUE;
      } else {
        seg_end_tc = tc;
      }
    } else {
      if (etype == WEED_EVENT_TYPE_MARKER) {
        marker_type = weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, NULL);
        if (marker_type == EVENT_MARKER_RECORD_END) {
          // add (resampled) frames for one recording stretch
          if (seg_start) rframes += 1 + ((double)(seg_end_tc - seg_start_tc)) / TICKS_PER_SECOND_DBL * fps;
          seg_start = FALSE;
        }
      }
    }
    event = get_next_event(event);
  }

  if (seg_start) rframes += 1 + ((double)(seg_end_tc - seg_start_tc)) / TICKS_PER_SECOND_DBL * fps;

  return rframes;
}


weed_timecode_t event_list_get_end_tc(weed_plant_t *event_list) {
  if (!event_list  || !get_last_event(event_list)) return 0.;
  return get_event_timecode(get_last_event(event_list));
}


double event_list_get_end_secs(weed_plant_t *event_list) {
  return (event_list_get_end_tc(event_list) / TICKS_PER_SECOND_DBL);
}


weed_timecode_t event_list_get_start_tc(weed_plant_t *event_list) {
  if (!event_list || !get_first_event(event_list)) return 0.;
  return get_event_timecode(get_first_event(event_list));
}


double event_list_get_start_secs(weed_plant_t *event_list) {
  return (event_list_get_start_tc(event_list) / TICKS_PER_SECOND_DBL);
}


boolean has_audio_frame(weed_plant_t *event_list) {
  weed_plant_t *event = get_first_frame_event(event_list);
  while (event) {
    if (WEED_EVENT_IS_AUDIO_FRAME(event)) return TRUE;
    event = get_next_frame_event(event);
  }
  return FALSE;
}


///////////////////////////////////////////////////////////////////

boolean render_to_clip(boolean new_clip) {
  // this function is called to actually start rendering mainw->event_list to a new/current clip
  char *pname = NULL;
  char *com, *tmp, *clipname = NULL;
  double old_fps = 0.;
  double afade_in_secs = 0., afade_out_secs = 0.;
#ifdef VFADE_RENDER
  double vfade_in_secs = 0., vfade_out_secs = 0.;
  LiVESWidgetColor fadecol;
  lives_colRGBA64_t vfade_rgb;
#endif
  LiVESResponseType response;
  boolean retval = TRUE, rendaud = TRUE, rendvid = TRUE;
  boolean norm_after = FALSE;
  boolean trans_sel = TRUE; // TODO
  boolean enc_lb = prefs->enc_letterbox;
  int xachans = 0, xarate = 0, xasamps = 0, xse = 0;
  int current_file = mainw->current_file;
  int pbq = prefs->pb_quality;

  prefs->pb_quality = PB_QUALITY_HIGH;

  if (new_clip) {
    if (prefs->render_prompt) {
      //set file details
      rdet = create_render_details(THREAD_INTENTION == LIVES_INTENTION_TRANSCODE ? 5 : 2);
      if (!has_audio_frame(mainw->event_list)) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton), FALSE);
        lives_widget_set_sensitive(resaudw->aud_checkbutton, FALSE);
      }
      rdet->enc_changed = FALSE;
      do {
        rdet->suggestion_followed = FALSE;
        response = lives_dialog_run(LIVES_DIALOG(rdet->dialog));
        if (response == LIVES_RESPONSE_OK && rdet->enc_changed) {
          check_encoder_restrictions(FALSE, TRUE, TRUE);
        }
      } while (rdet->suggestion_followed || response == LIVES_RESPONSE_RETRY || response == LIVES_RESPONSE_RESET);

      xarate = rdet->arate;
      xachans = rdet->achans;
      xasamps = rdet->asamps;

      // do we render audio ?
      rendaud = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton));
      norm_after = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rdet->norm_after));
      if (lives_widget_is_sensitive(rdet->afade_in))
        afade_in_secs = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rdet->afade_in));
      if (lives_widget_is_sensitive(rdet->afade_out))
        afade_out_secs = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rdet->afade_out));

      /* if (lives_widget_is_sensitive(rdet->vfade_in)) */
      /* 	vfade_in_secs = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rdet->vfade_in)); */
      /* if (lives_widget_is_sensitive(rdet->vfade_out)) */
      /* 	vfade_out_secs = lives_spin_button_get_value(LIVES_SPIN_BUTTON(rdet->vfade_out)); */

      /* lives_color_button_get_color(LIVES_COLOR_BUTTON(rdet->vfade_col), &fadecol); */
      /* widget_color_to_lives_rgba(&vfade_rgb, &fadecol); */

      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
        xse = AFORM_UNSIGNED;
      } else xse = AFORM_SIGNED;
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
        xse |= AFORM_BIG_ENDIAN;
      } else xse |= AFORM_LITTLE_ENDIAN;

      if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE) {
        clipname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rdet->clipname_entry)));
        tmp = get_untitled_name(mainw->untitled_number);
        if (!strcmp(clipname, tmp)) mainw->untitled_number++;
        lives_free(tmp);
      } else {
        clipname = lives_strdup("transcode");
        if (rdet->enc_lb) prefs->enc_letterbox = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rdet->enc_lb));
      }

      lives_widget_destroy(rdet->dialog);

      if (response == LIVES_RESPONSE_CANCEL) {
        if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE)
          lives_free(rdet->encoder_name);
        lives_free(clipname);
        lives_freep((void **)&rdet);
        lives_freep((void **)&resaudw);
        prefs->pb_quality = pbq;
        prefs->enc_letterbox = enc_lb;
        return FALSE;
      }
    } else {
      lives_capacity_t *caps = THREAD_CAPACITIES;
      if (0 && caps) {
        rendvid = lives_capacity_get(caps, LIVES_CAPACITY_VIDEO);
        rendaud = lives_capacity_get(caps, LIVES_CAPACITY_AUDIO);
      } else {
        if (mainw->multitrack) rendaud = mainw->multitrack->opts.render_audp;
        else rendaud = FALSE;
      }
      // TODO: prompt just for clip name
    }

    if (!(prefs->rec_opts & REC_AUDIO)) rendaud = FALSE;

    // create new file
    mainw->current_file = mainw->first_free_file;

    if (!get_new_handle(mainw->current_file, clipname)) {
      mainw->current_file = current_file;
      if (prefs->mt_enter_prompt) {
        if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE)
          lives_free(rdet->encoder_name);
        lives_freep((void **)&rdet);
        lives_freep((void **)&resaudw);
      }
      lives_free(clipname);
      prefs->enc_letterbox = enc_lb;
      prefs->pb_quality = pbq;
      return FALSE; // show dialog again
    }

    migrate_from_staging(mainw->current_file);

    lives_freep((void **)&clipname);

    cfile->opening = TRUE; // prevent audio from getting clobbered, it will be reset during rendering

    if (weed_plant_has_leaf(mainw->event_list, WEED_LEAF_FPS))
      old_fps = weed_get_double_value(mainw->event_list, WEED_LEAF_FPS, NULL);

    // RDET invalid after this
    if (prefs->render_prompt) {
      cfile->hsize = rdet->width;
      cfile->vsize = rdet->height;
      cfile->pb_fps = cfile->fps = rdet->fps;
      cfile->ratio_fps = rdet->ratio_fps;

      cfile->arps = cfile->arate = xarate;
      cfile->achans = xachans;
      cfile->asampsize = xasamps;
      cfile->signed_endian = xse;

      if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE) {
        lives_free(rdet->encoder_name);
      }
      lives_freep((void **)&rdet);
      lives_freep((void **)&resaudw);
    } else {
      cfile->hsize = prefs->mt_def_width;
      cfile->vsize = prefs->mt_def_height;
      cfile->pb_fps = cfile->fps = prefs->mt_def_fps;
      cfile->ratio_fps = FALSE;
      cfile->arate = cfile->arps = prefs->mt_def_arate;
      cfile->achans = prefs->mt_def_achans;
      cfile->asampsize = prefs->mt_def_asamps;
      cfile->signed_endian = prefs->mt_def_signed_endian;
    }

    if (old_fps != 0.) {
      cfile->pb_fps = cfile->fps = old_fps;
      cfile->ratio_fps = FALSE;
    }

    if (!rendaud) cfile->achans = cfile->arate = cfile->asampsize = 0;

    cfile->bpp = cfile->img_type == IMG_TYPE_JPEG ? 24 : 32;
    cfile->is_loaded = TRUE;
    if (prefs->btgamma) {
      if (IS_VALID_CLIP(current_file)) cfile->gamma_type = mainw->files[current_file]->gamma_type;
    }
    show_playbar_labels(mainw->current_file);
  } else if (!mainw->multitrack) {
    // back up audio to audio.back (in case we overwrite it)
    if (rendaud) {
      do_threaded_dialog(_("Backing up audio..."), FALSE);
      com = lives_strdup_printf("%s backup_audio \"%s\"", prefs->backend_sync, cfile->handle);
      mainw->error = FALSE;
      mainw->cancelled = CANCEL_NONE;
      lives_rm(cfile->info_file);
      lives_system(com, FALSE);
      lives_free(com);
      if (THREADVAR(com_failed)) {
        prefs->enc_letterbox = enc_lb;
        prefs->pb_quality = pbq;
        return FALSE;
      }
    } else {
      do_threaded_dialog(_("Clearing up clip..."), FALSE);
      com = lives_strdup_printf("%s clear_tmp_files \"%s\"", prefs->backend_sync, cfile->handle);
      lives_system(com, FALSE);
      lives_free(com);
    }
    end_threaded_dialog();
  }

  if (mainw->event_list && (!mainw->multitrack || mainw->unordered_blocks)) {
    if (old_fps == 0) {
      weed_plant_t *qevent_list;
      lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
      lives_widget_context_update();

      qevent_list = quantise_events(mainw->event_list, cfile->fps, !new_clip);

      if (qevent_list) {
        event_list_replace_events(mainw->event_list, qevent_list);
        weed_set_double_value(mainw->event_list, WEED_LEAF_FPS, cfile->fps);
      }
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    }
  }

  cfile->old_frames = cfile->frames;
  cfile->changed = TRUE;
  mainw->effects_paused = FALSE;

  if (new_clip) cfile->img_type = IMG_TYPE_BEST; // override the pref
  mainw->vfade_in_secs = mainw->vfade_out_secs = 0.;

#ifdef LIBAV_TRANSCODE
  if (THREAD_INTENTION == LIVES_INTENTION_TRANSCODE) {
    if (!transcode_prep()) {
      THREAD_INTENTION = LIVES_INTENTION_NOTHING;
      if (!mainw->multitrack)
        close_current_file(current_file);
      prefs->enc_letterbox = enc_lb;
      prefs->pb_quality = pbq;
      return FALSE;
    } else {
      lives_capacity_t *caps = lives_capacities_new();
      lives_capacity_set_int(caps, LIVES_CAPACITY_AUDIO_RATE, cfile->arate);
      lives_capacity_set_int(caps, LIVES_CAPACITY_AUDIO_CHANS, cfile->achans);
      lives_capacity_set_readonly(caps, LIVES_CAPACITY_AUDIO_RATE, TRUE);
      lives_capacity_set_readonly(caps, LIVES_CAPACITY_AUDIO_CHANS, TRUE);

      THREAD_CAPACITIES = caps;

      if (!transcode_get_params(&pname)) {
        lives_capacities_free(caps);
        THREAD_CAPACITIES = NULL;
        transcode_cleanup(mainw->vpp);
        if (!mainw->multitrack)
          close_current_file(current_file);
        prefs->enc_letterbox = enc_lb;
        prefs->pb_quality = pbq;
        return FALSE;
      }

      lives_capacities_free(caps);
      THREAD_CAPACITIES = NULL;
    }

    cfile->nopreview = TRUE;

    mainw->transrend_layer = NULL;
    mainw->transrend_ready = FALSE;

    mainw->transrend_proc = lives_proc_thread_create(LIVES_THRDATTR_NONE,
                            (lives_funcptr_t)transcode_clip,
                            WEED_SEED_BOOLEAN, "iibV", 1, 0, TRUE, pname);

    lives_nanosleep_while_false(mainw->transrend_ready);

    if (rendaud) {
      // pre-render audio
      d_print(_("Pre-rendering audio..."));
      if (!start_render_effect_events(mainw->event_list, FALSE, TRUE)) {
        mainw->transrend_ready = FALSE;
        lives_proc_thread_cancel(mainw->transrend_proc, FALSE);
        lives_proc_thread_join(mainw->transrend_proc);
        mainw->transrend_proc = NULL;
        if (!mainw->multitrack)
          close_current_file(current_file);
        retval = FALSE;
        goto rtc_done;
      }
      if (norm_after) on_normalise_audio_activate(NULL, NULL);
      if (afade_in_secs > 0.) {
        if (afade_in_secs > cfile->laudio_time) afade_in_secs = cfile->laudio_time;
        cfile->undo1_int = 0; // fade in

        cfile->undo2_dbl = trans_sel ?  calc_time_from_frame(mainw->current_file, cfile->start) : 0.;
        if (cfile->undo2_dbl + afade_in_secs > cfile->laudio_time) {
          afade_in_secs -= cfile->undo2_dbl + afade_in_secs - cfile->laudio_time;
        }
        cfile->undo1_dbl = cfile->undo2_dbl + afade_in_secs;

        on_fade_audio_activate(NULL, LIVES_INT_TO_POINTER(0));
      }
      if (afade_out_secs > 0.) {
        double entime;
        cfile->undo1_int = 1; // fade out

        if (!mainw->event_list) {
          cfile->undo1_dbl = calc_time_from_frame(mainw->current_file, cfile->end);
          if (cfile->undo1_dbl > cfile->laudio_time) cfile->undo1_dbl = cfile->laudio_time;
          if (trans_sel) entime = calc_time_from_frame(mainw->current_file, cfile->end);
          else entime = cfile->video_time;
          if (cfile->undo1_dbl > entime) cfile->undo1_dbl = entime;
        } else cfile->undo1_dbl = cfile->laudio_time;
        cfile->undo2_dbl = cfile->undo1_dbl - afade_out_secs;
        if (cfile->undo2_dbl < 0.) cfile->undo2_dbl = 0.;
        on_fade_audio_activate(NULL, LIVES_INT_TO_POINTER(1));
      }
      d_print_done();
      rendaud = FALSE;
    }
#ifdef VFADE_RENDER
    // temp fix until a better system emerges
    if (vfade_in_secs > 0.) {
      mainw->vfade_in_secs = vfade_in_secs;
      mainw->vfade_in_col = vfade_rgb;
    }
    // temp fix until a better system emerges
    if (vfade_out_secs > 0.) {
      mainw->vfade_out_secs = vfade_out_secs;
      mainw->vfade_out_col = vfade_rgb;
    }
#endif
    mainw->transrend_ready = FALSE;
  }
#endif

  if (mainw->multitrack && !rendaud && !mainw->multitrack->opts.render_vidp) {
    prefs->enc_letterbox = enc_lb;
    prefs->pb_quality = pbq;
    return FALSE;
  }

  if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE)
    d_print(_("Rendering..."));
  else d_print(_("Transcoding..."));

  // set all current track -> clips to 0
  init_track_decoders();

  if (THREAD_INTENTION == LIVES_INTENTION_TRANSCODE) {
    cfile->progress_start = 0;
    cfile->progress_end = cfile->frames;
    //cfile->asampsize = xasamps;
  }

  if (start_render_effect_events(mainw->event_list, rendvid, rendaud)) { // re-render, applying effects
    // and reordering/resampling/resizing if necessary
    if (THREAD_INTENTION != LIVES_INTENTION_TRANSCODE) {
      if (!mainw->multitrack && mainw->event_list) {
        if (!new_clip) {
          // this is needed in case we render to same clip, and then undo ///////
          if (cfile->event_list_back) event_list_free(cfile->event_list_back);
          cfile->event_list_back = mainw->event_list;
          ///////////////////////////////////////////////////////////////////////
        } else event_list_free(mainw->event_list);
      }
      mainw->event_list = NULL;
    }
    if (mainw->scrap_layer) {
      weed_layer_free(mainw->scrap_layer);
      mainw->scrap_layer = NULL;
    }
    if (mainw->scrap_pixbuf) {
      lives_widget_object_unref(mainw->scrap_pixbuf);
      mainw->scrap_pixbuf = NULL;
    }
    if (new_clip) {
      char *tmp;
      int old_file = current_file;

      if (THREAD_INTENTION == LIVES_INTENTION_TRANSCODE) {
        mainw->transrend_ready = TRUE;
        lives_proc_thread_cancel(mainw->transrend_proc, FALSE);
        lives_proc_thread_join(mainw->transrend_proc);
        mainw->transrend_proc = NULL;
        if (!mainw->multitrack)
          close_current_file(old_file);
        goto rtc_done;
      }

      if (rendaud) {
        if (norm_after) on_normalise_audio_activate(NULL, NULL);
        if (afade_in_secs > 0.) {
          if (afade_in_secs > cfile->laudio_time) afade_in_secs = cfile->laudio_time;
          cfile->undo1_int = 0; // fade in
          cfile->undo2_dbl = 0.;
          cfile->undo1_dbl = afade_in_secs;
          on_fade_audio_activate(NULL, LIVES_INT_TO_POINTER(0));
        }
        if (afade_out_secs > 0.) {
          if (afade_out_secs > cfile->laudio_time) afade_out_secs = cfile->laudio_time;
          cfile->undo1_int = 1; // fade out
          cfile->undo2_dbl = cfile->laudio_time - afade_out_secs;
          cfile->undo1_dbl = cfile->laudio_time;
          on_fade_audio_activate(NULL, LIVES_INT_TO_POINTER(1));
        }
      }

      cfile->start = cfile->frames ? 1 : 0;
      cfile->end = cfile->frames;

      set_undoable(NULL, FALSE);
      add_to_clipmenu();
      current_file = mainw->current_file;
      if (!save_clip_values(current_file)) {
        close_current_file(old_file);
        d_print_failed();
        retval = FALSE;
        goto rtc_done;
      }
      migrate_from_staging(current_file);
      if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
      if (!mainw->multitrack) {
        switch_clip(1, current_file, TRUE);
      }
      lives_rm(cfile->info_file);
      d_print((tmp = lives_strdup_printf(_("rendered %d frames to new clip.\n"), cfile->frames)));
      lives_free(tmp);
      mainw->pre_src_file = mainw->current_file; // if a generator started playback, we will switch back to this file after
      lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
    } else {
      // rendered to same clip - update number of frames
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FRAMES, &cfile->frames))
        do_header_write_error(mainw->current_file);
    }

    if (cfile->clip_type == CLIP_TYPE_FILE) {
      if (cfile->undo_start == 1 && cfile->undo_end == cfile->frames) {
        cfile->clip_type = CLIP_TYPE_DISK;
        lives_freep((void **)&cfile->frame_index_back);
        cfile->frame_index_back = cfile->frame_index;  // save for undo :: TODO
        del_frame_index(mainw->current_file);
      } else {
        char *what = (_("a new file index"));
        LiVESResponseType response;
        lives_freep((void **)&cfile->frame_index_back);

        do {
          response = LIVES_RESPONSE_OK;
          cfile->frame_index_back = cfile->frame_index;  // save for undo :: TODO
          cfile->frame_index = NULL;
          create_frame_index(mainw->current_file, FALSE, 0, cfile->frames);
          if (!cfile->frame_index) {
            cfile->frame_index = cfile->frame_index_back;
            cfile->frame_index_back = NULL;
            response = do_memory_error_dialog(what, cfile->frames * 4);
          }
        } while (response == LIVES_RESPONSE_RETRY);
        lives_free(what);

        if (response == LIVES_RESPONSE_CANCEL) {
          if (!mainw->multitrack) {
            if (new_clip) { // check
              close_current_file(current_file);
            } else {
              cfile->frame_index = cfile->frame_index_back;
              cfile->frame_index_back = NULL;
            }
          }
          prefs->enc_letterbox = enc_lb;
          prefs->pb_quality = pbq;
          return FALSE; /// will reshow the dialog
        }

        lives_memcpy(cfile->frame_index, cfile->frame_index_back, cfile->undo_start * sizeof(frames_t));

        for (int i = cfile->undo_start - 1; i < cfile->undo_end; i++) {
          cfile->frame_index[i] = -1;
        }

        lives_memcpy(&cfile->frame_index[cfile->undo_end], &cfile->frame_index_back[cfile->undo_end],
                     (cfile->frames - cfile->undo_end) * sizeof(frames_t));

        save_frame_index(mainw->current_file);
      }
    }
    if (!new_clip) d_print_done();
  } else {
    retval = FALSE; // cancelled or error, so show the dialog again
    if (THREAD_INTENTION == LIVES_INTENTION_TRANSCODE) {
      mainw->transrend_ready = TRUE;
      lives_proc_thread_cancel(mainw->transrend_proc, FALSE);
      lives_proc_thread_join(mainw->transrend_proc);
      mainw->transrend_proc = NULL;
    }
    if (THREAD_INTENTION == LIVES_INTENTION_TRANSCODE
        || (new_clip && !mainw->multitrack)) {
      // for mt we are rendering to the actual mt file, so we cant close it (CHECK: did we delete all images ?)
      close_current_file(current_file);
    }
  }

rtc_done:
  mainw->effects_paused = FALSE;
  free_track_decoders();
  deinit_render_effects();
  audio_free_fnames();
  mainw->vfade_in_secs = mainw->vfade_out_secs = 0.;
  prefs->pb_quality = pbq;
  prefs->enc_letterbox = enc_lb;
  return retval;
}


LIVES_INLINE void dprint_recneg(void) {d_print(_("nothing recorded.\n"));}

boolean backup_recording(char **esave_file, char **asave_file) {
  char *x, *y;
  LiVESList *clist = mainw->cliplist;
  double vald = 0.;
  int fd, i, hdlsize;

  if (!esave_file) esave_file = &x;
  if (!asave_file) asave_file = &y;

  *esave_file = lives_strdup_printf("%s/recorded-%s.%d.%d.%d.%s", prefs->workdir, LAYOUT_FILENAME, lives_getuid(), lives_getgid(),
                                    capable->mainpid, LIVES_FILE_EXT_LAYOUT);
  THREADVAR(write_failed) = FALSE;
  fd = lives_create_buffered(*esave_file, DEF_FILE_PERMS);
  if (fd >= 0) {
    save_event_list_inner(NULL, fd, mainw->event_list, NULL);
    lives_close_buffered(fd);
  }
  if (fd < 0 || THREADVAR(write_failed)) {
    if (mainw->is_exiting) return FALSE;
    THREADVAR(write_failed) = FALSE;
    lives_freep((void **)esave_file);
    *asave_file = NULL;
    return FALSE;
  }

  *asave_file = lives_strdup_printf("%s/recorded-%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME, lives_getuid(),
                                    lives_getgid(),
                                    capable->mainpid);

  fd = lives_create_buffered(*asave_file, DEF_FILE_PERMS);
  if (fd >= 0) {
    while (!THREADVAR(write_failed) && clist) {
      i = LIVES_POINTER_TO_INT(clist->data);
      if (IS_NORMAL_CLIP(i)) {
        lives_write_le_buffered(fd, &i, 4, TRUE);
        lives_write_le_buffered(fd, &vald, 8, TRUE);
        hdlsize = strlen(mainw->files[i]->handle);
        lives_write_le_buffered(fd, &hdlsize, 4, TRUE);
        lives_write_buffered(fd, (const char *)&mainw->files[i]->handle, hdlsize, TRUE);
      }
      clist = clist->next;
    }
    lives_close_buffered(fd);
  }

  if (fd < 0 || THREADVAR(write_failed)) {
    if (mainw->is_exiting) return FALSE;
    THREADVAR(write_failed) = FALSE;
    lives_rm(*esave_file);
    if (fd >= 0) lives_rm(*asave_file);
    lives_freep((void **)esave_file);
    lives_freep((void **)asave_file);
    return FALSE;
  }
  return TRUE;
}


static LiVESResponseType _show_rc_dlg(void) {
  LiVESResponseType resp;
  LiVESWidget *e_rec_dialog = events_rec_dialog();
  resp = lives_dialog_run(LIVES_DIALOG(e_rec_dialog));
  lives_widget_destroy(e_rec_dialog);
  return resp;
}


static LiVESResponseType show_rc_dlg(void) {
  LiVESResponseType resp;
  main_thread_execute((lives_funcptr_t)_show_rc_dlg, WEED_SEED_INT, &resp, "");
  return resp;
}


void event_list_add_end_events(weed_event_t *event_list, boolean is_final) {
  // for realtime recording, add filter deinit events and switch off audio
  // this occurs either when recording is paused (is_final == FALSE) or when playback ends (is_final == TRUE)
  if (event_list) {
    pthread_mutex_t *event_list_mutex = NULL;
    if (event_list == mainw->event_list) event_list_mutex = &mainw->event_list_mutex;

    if (prefs->rec_opts & REC_EFFECTS) {
      // add deinit events for all active effects
      // this will lock the event _list itself
      add_filter_deinit_events(event_list);
    }

    if (is_final) {
      // switch audio off
#ifdef RT_AUDIO
      if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)
          && mainw->agen_key == 0 && !mainw->agen_needs_reinit && !mainw->mute) {
        weed_plant_t *last_frame = get_last_event(event_list);
        event_list = insert_blank_frame_event_at(event_list, mainw->currticks, &last_frame);
        if (last_frame) {
#ifdef ENABLE_JACK
          if (prefs->audio_player == AUD_PLAYER_JACK) {
            if (prefs->audio_src == AUDIO_SRC_INT) {
              if (mainw->jackd) jack_get_rec_avals(mainw->jackd);
            } else {
              if (mainw->jackd_read) jack_get_rec_avals(mainw->jackd_read);
            }
          }
#endif
#ifdef HAVE_PULSE_AUDIO
          if (prefs->audio_player == AUD_PLAYER_PULSE) {
            if (prefs->audio_src == AUDIO_SRC_INT) {
              if (mainw->pulsed) pulse_get_rec_avals(mainw->pulsed);
            } else {
              if (mainw->pulsed_read) pulse_get_rec_avals(mainw->pulsed_read);
            }
#endif
#if 0
            if (prefs->audio_player == AUD_PLAYER_NONE) {
              nullaudio_get_rec_avals();
            }
#endif
            insert_audio_event_at(last_frame, -1, mainw->rec_aclip, mainw->rec_aseek, 0.);
	    // *INDENT-OFF*
	  }}}
      // *INDENT-ON*
#endif
    } else {
      // write a RECORD_END marker
      weed_timecode_t tc;
      if (event_list_mutex) pthread_mutex_lock(event_list_mutex);
      tc = get_event_timecode(get_last_event(event_list));
      event_list = append_marker_event(event_list, tc, EVENT_MARKER_RECORD_END); // mark record end
      if (event_list_mutex) pthread_mutex_unlock(event_list_mutex);
    }
  }
}


boolean deal_with_render_choice(boolean add_deinit) {
  // this is called from saveplay.c after record/playback ends
  // here we deal with the user's wishes as to how to deal with the recorded events

  // mainw->osc_block should be TRUE during all of this, so we don't have to contend with
  // any incoming network messages

  // return TRUE if we rendered to a new clip
  lives_proc_thread_t info = NULL;
  lives_capacity_t *caps = NULL;

  LiVESWidget *elist_dialog;

  double df;

  char *esave_file = NULL, *asave_file = NULL;

  boolean new_clip = FALSE;

  int dh, dw, dar, das, dac, dse;
  frames_t oplay_start = 1;

  render_choice = RENDER_CHOICE_NONE;

  if (!CURRENT_CLIP_IS_VALID) {
    /// user may have recorded a  generator with no other clips loaded
    if (mainw->scrap_file != -1)
      mainw->current_file = mainw->scrap_file;
    else if (mainw->ascrap_file != -1)
      mainw->current_file = mainw->ascrap_file;
    if (CURRENT_CLIP_IS_VALID) {
      cfile->hsize = DEF_GEN_WIDTH;
      cfile->vsize = DEF_GEN_HEIGHT;
    }
  }

  if (!CURRENT_CLIP_IS_VALID) render_choice = RENDER_CHOICE_MULTITRACK;

  if (count_events(mainw->event_list, FALSE, 0, 0) == 0) {
    event_list_free(mainw->event_list);
    mainw->event_list = NULL;
  }

  if (!mainw->event_list) {
    close_scrap_file(TRUE);
    close_ascrap_file(TRUE);
    dprint_recneg();
    return FALSE;
  }

  last_rec_start_tc = -1;

  if (!mainw->recording_recovered) {
    // need to retain play_start for rendering to same clip
    oplay_start = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
    if (mainw->playing_sel && (oplay_start < cfile->start || oplay_start > cfile->end)) {
      oplay_start = cfile->start;
    }
  }

  event_list_reorder_noquant(mainw->event_list);
  event_list_close_gaps(mainw->event_list, oplay_start);

  check_storage_space(-1, FALSE);

  if (prefs->gui_monitor == 0) {
    // avoid an annoyance
    pref_factory_int(PREF_SEPWIN_TYPE, (int *)&prefs->sepwin_type, SEPWIN_TYPE_NON_STICKY, FALSE);
  }

  // crash recovery -> backup the event list
  if (prefs->crash_recovery && prefs->rr_crash) {
    info = lives_proc_thread_create(LIVES_THRDATTR_NO_GUI | LIVES_THRDATTR_PRIORITY,
                                    (lives_funcptr_t)backup_recording, -1, "vv",
                                    &esave_file, &asave_file);
  }
  mainw->no_interp = TRUE;
  do {
    prefs->event_window_show_frame_events = TRUE;
    if (render_choice == RENDER_CHOICE_NONE || render_choice == RENDER_CHOICE_PREVIEW)
      if (show_rc_dlg() == LIVES_RESPONSE_CANCEL) render_choice = RENDER_CHOICE_DISCARD;
    switch (render_choice) {
    case RENDER_CHOICE_DISCARD:
      if ((mainw->current_file == mainw->scrap_file || mainw->current_file == mainw->ascrap_file)
          && !mainw->clips_available) {
        mainw->current_file = -1;
        lives_ce_update_timeline(0, 0.);
      } else if (CURRENT_CLIP_IS_VALID) cfile->redoable = FALSE;
      close_scrap_file(TRUE);
      close_ascrap_file(TRUE);
      sensitize();
      break;
    case RENDER_CHOICE_PREVIEW:
      // preview
      cfile->next_event = get_first_event(mainw->event_list);
      mainw->is_rendering = TRUE;
      mainw->preview_rendering = TRUE;
      if (prefs->audio_src == AUDIO_SRC_EXT) {
        pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, FALSE);
      }
      on_preview_clicked(NULL, NULL);
      if (future_prefs->audio_src == AUDIO_SRC_EXT) {
        pref_factory_bool(PREF_REC_EXT_AUDIO, TRUE, FALSE);
      }
      free_track_decoders();
      deinit_render_effects();
      mainw->preview_rendering = FALSE;
      mainw->is_processing = mainw->is_rendering = FALSE;
      cfile->next_event = NULL;
      break;
    case RENDER_CHOICE_TRANSCODE:
      THREAD_INTENTION = LIVES_INTENTION_TRANSCODE;
    case RENDER_CHOICE_NEW_CLIP:
      if (render_choice == RENDER_CHOICE_NEW_CLIP) {
        caps = lives_capacities_new();
        THREAD_INTENTION = LIVES_INTENTION_RENDER;
        lives_capacity_set(caps, LIVES_CAPACITY_VIDEO);
        lives_capacity_set(caps, LIVES_CAPACITY_AUDIO);
        THREAD_CAPACITIES = caps;
      }
      dw = prefs->mt_def_width;
      dh = prefs->mt_def_height;
      df = prefs->mt_def_fps;
      dar = prefs->mt_def_arate;
      dac = prefs->mt_def_achans;
      das = prefs->mt_def_asamps;
      dse = prefs->mt_def_signed_endian;
      if (!mainw->clip_switched && prefs->render_prompt && mainw->current_file > -1) {
        if (cfile->hsize > 0) prefs->mt_def_width = cfile->hsize;
        if (cfile->vsize > 0) prefs->mt_def_height = cfile->vsize;
        prefs->mt_def_fps = cfile->fps;
        if (cfile->achans * cfile->arate * cfile->asampsize > 0) {
          prefs->mt_def_arate = cfile->arate;
          prefs->mt_def_asamps = cfile->asampsize;
          prefs->mt_def_achans = cfile->achans;
          prefs->mt_def_signed_endian = cfile->signed_endian;
        }
      }
      mainw->play_start = 1; ///< new clip frames always start  at 1
      if (info) {
        //lives_nanosleep_until_nonzero(weed_get_boolean_value(info, WEED_LEAF_DONE, NULL));
        lives_proc_thread_join(info);
        info = NULL;
      }

      if (!render_to_clip(TRUE) || render_choice == RENDER_CHOICE_TRANSCODE)
        render_choice = RENDER_CHOICE_PREVIEW;
      else {
        close_scrap_file(TRUE);
        close_ascrap_file(TRUE);
        prefs->mt_def_width = dw;
        prefs->mt_def_height = dh;
        prefs->mt_def_fps = df;
        prefs->mt_def_arate = dar;
        prefs->mt_def_achans = dac;
        prefs->mt_def_asamps = das;
        prefs->mt_def_signed_endian = dse;
        new_clip = TRUE;
      }
      mainw->is_rendering = FALSE;
      THREAD_INTENTION = LIVES_INTENTION_NOTHING;
      if (caps) {
        lives_capacities_free(caps);
        caps = THREAD_CAPACITIES = NULL;
      }
      break;
    case RENDER_CHOICE_SAME_CLIP:
      //cfile->undo_end = cfile->undo_start = oplay_start; ///< same clip frames start where recording started
      cfile->undo_end = cfile->undo_start = -1;
      if (info) {
        lives_proc_thread_join(info);
        info = NULL;
      }
      caps = lives_capacities_new();
      THREAD_INTENTION = LIVES_INTENTION_RENDER;
      lives_capacity_set(caps, LIVES_CAPACITY_VIDEO);
      lives_capacity_set(caps, LIVES_CAPACITY_AUDIO);
      THREAD_CAPACITIES = caps;
      if (!render_to_clip(FALSE)) render_choice = RENDER_CHOICE_PREVIEW;
      else {
        close_scrap_file(TRUE);
        close_ascrap_file(TRUE);
      }
      mainw->is_rendering = FALSE;
      THREAD_INTENTION = LIVES_INTENTION_NOTHING;
      if (caps) {
        lives_capacities_free(caps);
        caps = THREAD_CAPACITIES = NULL;
      }
      break;
    case RENDER_CHOICE_MULTITRACK:
      if (mainw->stored_event_list && mainw->stored_event_list_changed) {
        if (!check_for_layout_del(NULL, FALSE)) {
          render_choice = RENDER_CHOICE_PREVIEW;
          break;
        }
      }
      if (mainw->stored_event_list || mainw->sl_undo_mem) {
        recover_layout_cancelled(FALSE);
        stored_event_list_free_all(TRUE);
      }
      mainw->unordered_blocks = TRUE;
      pref_factory_int(PREF_SEPWIN_TYPE, (int *)&prefs->sepwin_type, future_prefs->sepwin_type, FALSE);
      if (info) {
        lives_proc_thread_join(info);
        info = NULL;
      }
      prefs->letterbox_mt = prefs->letterbox;
      if (on_multitrack_activate(NULL, (weed_plant_t *)mainw->event_list)) {
        prefs->letterbox_mt = future_prefs->letterbox_mt;
        mainw->event_list = NULL;
        new_clip = TRUE;
      } else render_choice = RENDER_CHOICE_PREVIEW;
      mainw->unordered_blocks = FALSE;
      break;
    case RENDER_CHOICE_EVENT_LIST:
      if (count_events(mainw->event_list, prefs->event_window_show_frame_events, 0, 0) > 1000) {
        if (!do_event_list_warning()) {
          render_choice = RENDER_CHOICE_PREVIEW;
          break;
        }
      }
      elist_dialog = create_event_list_dialog(mainw->event_list, 0, 0);
      lives_dialog_run(LIVES_DIALOG(elist_dialog));
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
      lives_widget_context_update();
      render_choice = RENDER_CHOICE_PREVIEW;
      break;
    }

    if (CURRENT_CLIP_IS_VALID) cfile->next_event = NULL;

    if (IS_VALID_CLIP(mainw->scrap_file)) {
      // rewind scrap file to beginning
      if (!mainw->files[mainw->scrap_file]->ext_src) load_from_scrap_file(NULL, -1);
      lives_lseek_buffered_rdonly_absolute(LIVES_POINTER_TO_INT(mainw->files[mainw->scrap_file]->ext_src), 0);
    }
  } while (render_choice == RENDER_CHOICE_PREVIEW);

  mainw->no_interp = FALSE;

  if (info) {
    lives_proc_thread_join(info);
    info = NULL;
  }

  if (esave_file) lives_rm(esave_file);
  if (asave_file) lives_rm(asave_file);

  if (mainw->event_list) {
    event_list_free(mainw->event_list);
    mainw->event_list = NULL;
  }

  /// multitrack will set this itself
  if (render_choice != RENDER_CHOICE_MULTITRACK)
    pref_factory_int(PREF_SEPWIN_TYPE, (int *)&prefs->sepwin_type, future_prefs->sepwin_type, FALSE);

  sensitize();

  check_storage_space(mainw->current_file, FALSE);

  return new_clip;
}


/**
   @brief calculate the "visibility" of each track at timecode tc

     that is to say, only the front track is visible, except if we have a transition and WEED_LEAF_HOST_AUDIO_TRANSITION is set
     - in which case the track visibility is proportional to the transition parameter

     to do this, we need a filter map and a frame/clip stack

     if bleedthru is TRUE, all values are set to 1.0 */
double *get_track_visibility_at_tc(weed_plant_t *event_list, int ntracks, int nbtracks,
                                   weed_timecode_t tc, weed_plant_t **shortcut, boolean bleedthru) {
  static weed_plant_t *stored_fmap;

  weed_plant_t *frame_event, *fmap;

  double *vis;
  double *matrix[ntracks + nbtracks];

  int *clips = NULL;
  frames64_t *frames = NULL;

  int nxtracks;
  int got = -1;

  int i, j;

  ntracks += nbtracks;

  if (!shortcut || !*shortcut) stored_fmap = NULL;

  if (shortcut) *shortcut = frame_event = get_frame_event_at_or_before(event_list, tc, *shortcut);
  else frame_event = get_frame_event_at_or_before(event_list, tc, NULL);

  nxtracks = weed_leaf_num_elements(frame_event, WEED_LEAF_CLIPS);

  vis = (double *)lives_malloc(ntracks * sizeof(double));

  if (bleedthru) {
    for (i = 0; i < ntracks; i++) {
      vis[i] = 1.;
    }
    return vis;
  }

  clips = weed_get_int_array(frame_event, WEED_LEAF_CLIPS, NULL);
  frames = weed_get_int64_array(frame_event, WEED_LEAF_FRAMES, NULL);

  if (nbtracks > 0) vis[0] = 1.;

  if (!stored_fmap) stored_fmap = fmap = get_filter_map_before(frame_event, LIVES_TRACK_ANY, NULL);
  else {
    fmap = get_filter_map_before(frame_event, LIVES_TRACK_ANY, *shortcut);
    if (fmap == *shortcut) fmap = stored_fmap;
  }

  for (i = 0; i < ntracks; i++) {
    matrix[i] = (double *)lives_malloc(ntracks * sizeof(double));
    for (j = 0; j < ntracks; j++) {
      matrix[i][j] = 0.;
    }
    matrix[i][i] = 1.;
  }

  if (fmap) {
    // here we look at all init_events in fmap. If any have WEED_LEAF_HOST_AUDIO_TRANSITION set, then
    // we we look at the 2 in channels. We first multiply matrix[t0][...] by trans - 1
    // then we add matrix[t1][...] * (trans) to matrix[t3][...]
    // where trans is the normalised value of the transition parameter
    // t3 is the output channel, (this is usually the same track as t0)
    // thus each row in the matrix represents the contribution from each layer (track)
    if (weed_plant_has_leaf(fmap, WEED_LEAF_INIT_EVENTS)) {
      int nins;
      weed_plant_t **iev = (weed_plant_t **)weed_get_voidptr_array_counted(fmap, WEED_LEAF_INIT_EVENTS, &nins);
      for (i = 0; i < nins; i++) {
        weed_plant_t *ievent = iev[i];
        if (weed_get_boolean_value(ievent, WEED_LEAF_HOST_AUDIO_TRANSITION, NULL) == WEED_TRUE) {
          int *in_tracks = weed_get_int_array(ievent, WEED_LEAF_IN_TRACKS, NULL);
          int *out_tracks = weed_get_int_array(ievent, WEED_LEAF_OUT_TRACKS, NULL);
          char *filter_hash = weed_get_string_value(ievent, WEED_LEAF_FILTER, NULL);
          int idx;
          if ((idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
            int npch;
            weed_plant_t *filter = get_weed_filter(idx);
            int tparam = get_transition_param(filter, FALSE);
            weed_plant_t *inst = weed_instance_from_filter(filter);
            weed_plant_t **in_params = weed_instance_get_in_params(inst, NULL);
            weed_plant_t *ttmpl = weed_param_get_template(in_params[tparam]);
            void **pchains = weed_get_voidptr_array_counted(ievent, WEED_LEAF_IN_PARAMETERS, &npch);
            double trans;

            if (tparam < npch) interpolate_param(in_params[tparam], pchains[tparam], tc);
            lives_free(pchains);

            if (weed_leaf_seed_type(in_params[tparam], WEED_LEAF_VALUE) == WEED_SEED_DOUBLE) {
              double transd = weed_get_double_value(in_params[tparam], WEED_LEAF_VALUE, NULL);
              double tmin = weed_get_double_value(ttmpl, WEED_LEAF_MIN, NULL);
              double tmax = weed_get_double_value(ttmpl, WEED_LEAF_MAX, NULL);
              trans = (transd - tmin) / (tmax - tmin);
            } else {
              int transi = weed_get_int_value(in_params[tparam], WEED_LEAF_VALUE, NULL);
              int tmin = weed_get_int_value(ttmpl, WEED_LEAF_MIN, NULL);
              int tmax = weed_get_int_value(ttmpl, WEED_LEAF_MAX, NULL);
              trans = (double)(transi - tmin) / (double)(tmax - tmin);
            }
            lives_free(in_params);
            for (j = 0; j < ntracks; j++) {
              /// TODO *** make selectable: linear or non-linear
              /* matrix[in_tracks[1] + nbtracks][j] *= lives_vol_from_linear(trans); */
              /* matrix[in_tracks[0] + nbtracks][j] *= lives_vol_from_linear((1. - trans)); */
              matrix[in_tracks[1] + nbtracks][j] *= lives_vol_from_linear(trans);
              matrix[in_tracks[0] + nbtracks][j] *= lives_vol_from_linear(1. - trans);
              matrix[out_tracks[0] + nbtracks][j] = lives_vol_to_linear(matrix[in_tracks[0] + nbtracks][j]
                                                    + matrix[in_tracks[1] + nbtracks][j]);
            }
            weed_instance_unref(inst);
          }
          lives_free(in_tracks);
          lives_free(out_tracks);
          lives_free(filter_hash);
        }
      }
      lives_free(iev);
    }
  }

  // now we select as visibility, whichever row is the first layer to have a non-blank frame

  for (i = 0; i < nxtracks; i++) {
    if (clips[i] >= 0 && frames[i] > 0) {
      got = i + nbtracks;
      break;
    }
  }
  lives_free(clips);
  lives_free(frames);

  if (got == -1) {
    // all frames blank - backing audio only
    for (i = 0; i < ntracks; i++) {
      if (i >= nbtracks) vis[i] = 0.;
      lives_free(matrix[i]);
    }
    return vis;
  }

  for (i = nbtracks; i < ntracks; i++) {
    vis[i] = matrix[got][i];
  }

  for (i = 0; i < ntracks; i++) {
    lives_free(matrix[i]);
  }
  return vis;
}

//////////////////////
//GUI stuff

enum {
  TITLE_COLUMN,
  KEY_COLUMN,
  VALUE_COLUMN,
  DESC_COLUMN,
  NUM_COLUMNS
};


#if GTK_CHECK_VERSION(3, 0, 0)
static void rowexpand(LiVESWidget * tv, LiVESTreeIter * iter, LiVESTreePath * path, livespointer ud) {
  lives_widget_queue_resize(tv);
  lives_widget_queue_draw(tv);
  lives_widget_process_updates(tv);
}
#endif


static void quant_clicked(LiVESButton * button, livespointer elist) {
  weed_plant_t *ev_list = (weed_plant_t *)elist;
  weed_plant_t *qevent_list = quantise_events(ev_list, cfile->fps, FALSE);
  if (qevent_list) {
    /* reset_renumbering(); */
    /* event_list_rectify(NULL, qevent_list); */
    event_list_replace_events(ev_list, qevent_list);
    weed_set_double_value(ev_list, WEED_LEAF_FPS, cfile->fps);
  }
  lives_general_button_clicked(button, NULL);
}


LiVESWidget *create_event_list_dialog(weed_plant_t *event_list, weed_timecode_t start_tc, weed_timecode_t end_tc) {
  // TODO - some event properties should be editable, e.g. parameter values
  weed_timecode_t tc, tc_secs;

  LiVESTreeStore *treestore;
  LiVESTreeIter iter1, iter2, iter3;
  static size_t inistrlen = 0;

  char **string = NULL;
  int *intval = NULL;
  void **voidval = NULL;
  double *doubval = NULL;
  int64_t *int64val = NULL;

  weed_plant_t *event, *ievent;

  LiVESWidget *event_dialog, *daa;
  LiVESWidget *tree;
  LiVESWidget *table;
  LiVESWidget *top_vbox;
  LiVESWidget *label;
  LiVESWidget *ok_button;
  LiVESWidget *scrolledwindow;

  LiVESCellRenderer *renderer;
  LiVESTreeViewColumn *column;

  char **propnames;

  char *strval = NULL, *desc = NULL;
  char *text, *ltext;
  char *oldval = NULL, *final = NULL;
  char *iname = NULL, *fname = NULL;
  char *tmp;

  int woat = widget_opts.apply_theme;

  int winsize_h;
  int winsize_v;

  int num_elems, seed_type, etype;
  int rows, currow = 0;
  int ie_idx = 0;

  int i, j;

  if (inistrlen == 0) inistrlen = lives_strlen(WEED_LEAF_INIT_EVENT);

  if (prefs->event_window_show_frame_events)
    rows = count_events(event_list, TRUE, start_tc, end_tc);
  else rows = count_events(event_list, TRUE, start_tc, end_tc) - count_events(event_list, FALSE, start_tc, end_tc);

  //event = get_first_event(event_list);
  event = get_first_event(event_list);

  winsize_h = GUI_SCREEN_WIDTH - SCR_WIDTH_SAFETY;
  winsize_v = GUI_SCREEN_HEIGHT - SCR_HEIGHT_SAFETY;

  event_dialog = lives_standard_dialog_new(_("Event List"), FALSE, winsize_h, winsize_v);

  top_vbox = lives_dialog_get_content_area(LIVES_DIALOG(event_dialog));

  table = lives_table_new(rows, 6, FALSE);
  lives_widget_set_valign(table, LIVES_ALIGN_START);

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_context_update();

  while (event) {
    // pass through all events
    tc = get_event_timecode(event);

    if (end_tc > 0) {
      if (tc < start_tc) {
        event = get_next_event(event);
        continue;
      }
      if (tc >= end_tc) break;
    }

    etype = get_event_type(event);
    /* if (WEED_EVENT_IS_FRAME(event)) { */
    /*   event = get_next_event(event); */
    /*   continue; */
    /* } */
    if ((prefs->event_window_show_frame_events || !WEED_EVENT_IS_FRAME(event)) || WEED_EVENT_IS_AUDIO_FRAME(event)) {
      //if (!prefs->event_window_show_frame_events && WEED_EVENT_IS_FRAME(event)) {
      // TODO - opts should be all frames, only audio frames, no frames
      // or even better, filter for any event types
      rows++;
      lives_table_resize(LIVES_TABLE(table), rows, 6);
      //}

      treestore = lives_tree_store_new(NUM_COLUMNS, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_STRING,
                                       LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_STRING);

      lives_tree_store_append(treestore, &iter1, NULL);   /* Acquire an iterator */
      lives_tree_store_set(treestore, &iter1, TITLE_COLUMN, _("Properties"), -1);

      // get list of keys (property) names for this event
      propnames = weed_plant_list_leaves(event, NULL);

      for (i = 0; propnames[i]; i++) {
        if (!strcmp(propnames[i], WEED_LEAF_TYPE) || !strcmp(propnames[i], WEED_LEAF_EVENT_TYPE) ||
            !lives_strcmp(propnames[i], WEED_LEAF_TIMECODE) || !strncmp(propnames[i], "host_", 5)) {
          lives_free(propnames[i]);
          continue;
        }
        lives_tree_store_append(treestore, &iter2, &iter1);   /* Acquire a child iterator */

        lives_freep((void **)&oldval);
        lives_freep((void **)&final);

        num_elems = weed_leaf_num_elements(event, propnames[i]);
        seed_type = weed_leaf_seed_type(event, propnames[i]);

        switch (seed_type) {
        // get the value
        case WEED_SEED_INT:
          intval = weed_get_int_array(event, propnames[i], NULL);
          break;
        case WEED_SEED_INT64:
          int64val = weed_get_int64_array(event, propnames[i], NULL);
          break;
        case WEED_SEED_BOOLEAN:
          intval = weed_get_boolean_array(event, propnames[i], NULL);
          break;
        case WEED_SEED_STRING:
          string = weed_get_string_array(event, propnames[i], NULL);
          break;
        case WEED_SEED_DOUBLE:
          doubval = weed_get_double_array(event, propnames[i], NULL);
          break;
        case WEED_SEED_VOIDPTR:
          voidval = weed_get_voidptr_array(event, propnames[i], NULL);
          break;
        case WEED_SEED_PLANTPTR:
          voidval = (void **)weed_get_plantptr_array(event, propnames[i], NULL);
          break;
        }

        ievent = NULL;

        for (j = 0; j < num_elems; j++) {
          if (etype == WEED_EVENT_TYPE_PARAM_CHANGE && (!strcmp(propnames[i], WEED_LEAF_INDEX))
              && seed_type == WEED_SEED_INT) {
            char *pname = NULL; // want the parameter name for the index
            weed_plant_t *ptmpl = NULL;
            ievent = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);
            if (ievent) {
              lives_freep((void **)&iname);
              iname = weed_get_string_value(ievent, WEED_LEAF_FILTER, NULL);
              if (iname) {
                ie_idx = weed_get_idx_for_hashname(iname, TRUE);
              }
              lives_freep((void **)&iname);
              ptmpl = weed_filter_in_paramtmpl(get_weed_filter(ie_idx), intval[j], TRUE);
            }
            if (ptmpl)
              pname = weed_get_string_value(ptmpl, WEED_LEAF_NAME, NULL);
            else pname = lives_strdup("???");
            strval = lives_strdup_printf("%d", intval[j]);
            desc = lives_strdup_printf("(%s)", pname);
            lives_freep((void **)&pname);
          } else {
            if (etype == WEED_EVENT_TYPE_FILTER_INIT && (!strcmp(propnames[i], WEED_LEAF_IN_TRACKS)
                || !strcmp(propnames[i], WEED_LEAF_OUT_TRACKS))) {
              if (mainw->multitrack) {
                iname = weed_get_string_value(event, WEED_LEAF_FILTER, NULL);
                if (iname) {
                  ie_idx = weed_get_idx_for_hashname(iname, TRUE);
                }
                lives_freep((void **)&iname);
                strval = lives_strdup_printf("%d", intval[j]);
                desc = lives_strdup_printf("(%s)",
                                           (tmp = get_track_name(mainw->multitrack, intval[j],
                                                  is_pure_audio(get_weed_filter(ie_idx), FALSE))));
                lives_free(tmp);
              } else {
                strval = lives_strdup_printf("%d", intval[j]);
                desc = lives_strdup_printf("(%s)", intval[j] == 0 ? _("foreground clip")
                                           : _("background_clip"));
              }
            } else {
              if (0);
              /* if (etype == WEED_EVENT_TYPE_FILTER_INIT && (!strcmp(propnames[i], WEED_LEAF_IN_COUNT) */
              /* 						  || !strcmp(propnames[i], WEED_LEAF_OUT_COUNT))) { */
              /* 	iname = weed_get_string_value(event, WEED_LEAF_FILTER, NULL); */
              /* 	if (iname) { */
              /* 	  ie_idx = weed_get_idx_for_hashname(iname, TRUE); */
              /* 	} */
              /* 	strval = lives_strdup_printf("%d		(%s X %d)", intval[j], weed_chantmpl_get_name(...etc)); */
              /* 	lives_freep((void **)&iname); */
              /* } */
              else {
                switch (seed_type) {
                // format each element of value
                case WEED_SEED_INT:
                  strval = lives_strdup_printf("%d", intval[j]);
                  break;
                case WEED_SEED_INT64:
                  strval = lives_strdup_printf("%"PRId64, int64val[j]);
                  break;
                case WEED_SEED_DOUBLE:
                  strval = lives_strdup_printf("%.4f", doubval[j]);
                  break;
                case WEED_SEED_BOOLEAN:
                  if (intval[j] == WEED_TRUE) strval = (_("TRUE"));
                  else strval = (_("FALSE"));
                  break;
                case WEED_SEED_STRING:
                  if (etype == WEED_EVENT_TYPE_FILTER_INIT && (!strcmp(propnames[i], WEED_LEAF_FILTER))) {
                    ie_idx = weed_get_idx_for_hashname(string[j], TRUE);
                    strval = weed_filter_idx_get_name(ie_idx, FALSE, FALSE);
                  } else strval = lives_strdup(string[j]);
                  lives_free(string[j]);
                  break;
                case WEED_SEED_VOIDPTR:
                  if (etype == WEED_EVENT_TYPE_FILTER_DEINIT || etype == WEED_EVENT_TYPE_FILTER_MAP
                      || etype == WEED_EVENT_TYPE_PARAM_CHANGE) {
                    if (!(lives_strncmp(propnames[i], WEED_LEAF_INIT_EVENT, inistrlen))) {
                      ievent = (weed_plant_t *)voidval[j];
                      if (ievent) {
                        lives_freep((void **)&iname);
                        iname = weed_get_string_value(ievent, WEED_LEAF_FILTER, NULL);
                        if (iname) {
                          ie_idx = weed_get_idx_for_hashname(iname, TRUE);
                          fname = weed_filter_idx_get_name(ie_idx, FALSE, FALSE);
                          strval = lives_strdup_printf("%p", voidval[j]);
                          desc = lives_strdup_printf("(%s)", fname);
                          lives_freep((void **)&fname);
                        }
                        lives_freep((void **)&iname);
                      }
                    }
                  }
                  if (!strval) {
                    if (voidval[j]) strval = lives_strdup_printf("%p", voidval[j]);
                    else strval = lives_strdup(" - ");
                  }
                  break;
                case WEED_SEED_PLANTPTR:
                  strval = lives_strdup_printf("%p", voidval[j]);
                  break;
                default:
                  strval = lives_strdup("???");
                  break;
		  // *INDENT-OFF*
                }}}}
	  // *INDENT-ON*

          // attach to treestore
          if (j == 0) {
            if (num_elems == 1) {
              lives_tree_store_set(treestore, &iter2, KEY_COLUMN, propnames[i], VALUE_COLUMN, strval, -1);
              lives_tree_store_set(treestore, &iter2, DESC_COLUMN, desc, -1);
            } else {
              lives_tree_store_set(treestore, &iter2, KEY_COLUMN, propnames[i], VALUE_COLUMN, "", -1);
              lives_tree_store_append(treestore, &iter3, &iter2);
              lives_tree_store_set(treestore, &iter3, VALUE_COLUMN, strval, -1);
              lives_tree_store_set(treestore, &iter3, DESC_COLUMN, desc, -1);
            }
          } else {
            lives_tree_store_append(treestore, &iter3, &iter2);
            lives_tree_store_set(treestore, &iter3, VALUE_COLUMN, strval, -1);
            lives_tree_store_set(treestore, &iter3, DESC_COLUMN, desc, -1);
          }
          lives_freep((void **)&desc);
          lives_freep((void **)&strval);
        }

        switch (seed_type) {
        // free temp memory
        case WEED_SEED_INT:
        case WEED_SEED_BOOLEAN:
          lives_free(intval);
          break;
        case WEED_SEED_INT64:
          lives_free(int64val);
          break;
        case WEED_SEED_DOUBLE:
          lives_free(doubval);
          break;
        case WEED_SEED_STRING:
          lives_free(string);
          break;
        case WEED_SEED_VOIDPTR:
        case WEED_SEED_PLANTPTR:
          lives_free(voidval);
          break;
        default: break;
        }
        lives_free(propnames[i]);
      }

      lives_free(propnames);

      // now add the new treeview

      lives_free(final);

      // timecode
      tc_secs = tc / TICKS_PER_SECOND;
      tc -= tc_secs * TICKS_PER_SECOND;
      text = lives_strdup_printf(_("Timecode=%" PRId64 ".%.08" PRId64), tc_secs, tc);
      label = lives_standard_label_new(text);
      lives_free(text);
      lives_widget_set_valign(label, LIVES_ALIGN_START);

      lives_table_attach(LIVES_TABLE(table), label, 0, 1, currow, currow + 1,
                         (LiVESAttachOptions)(LIVES_EXPAND), (LiVESAttachOptions)(0), 0, 0);

      if (WEED_PLANT_IS_EVENT_LIST(event))
        ltext = "Event list";
      else {
        // event type
        switch (etype) {
        case WEED_EVENT_TYPE_FRAME:
          if (WEED_EVENT_IS_AUDIO_FRAME(event))
            ltext = "Frame with audio";
          else
            ltext = "Frame";
          break;
        case WEED_EVENT_TYPE_FILTER_INIT:
          ltext = "Filter on"; break;
        case WEED_EVENT_TYPE_FILTER_DEINIT:
          ltext = "Filter off"; break;
        case WEED_EVENT_TYPE_PARAM_CHANGE:
          ltext = "Parameter change"; break;
        case WEED_EVENT_TYPE_FILTER_MAP:
          ltext = "Filter map"; break;
        case WEED_EVENT_TYPE_MARKER:
          ltext = "Marker"; break;
        default:
          ltext = lives_strdup_printf("unknown event type %d", etype);
          label = lives_standard_label_new(ltext);
          ltext = NULL;
        }
      }
      if (ltext) {
        text = lives_strdup_printf("<big><b>%s</b></big>", ltext);
        widget_opts.use_markup = TRUE;
        label = lives_standard_label_new(text);
        widget_opts.use_markup = FALSE;
        lives_free(text);
        lives_widget_set_valign(label, LIVES_ALIGN_START);
      }

      lives_table_attach(LIVES_TABLE(table), label, 1, 2, currow, currow + 1,
                         (LiVESAttachOptions)(LIVES_EXPAND), (LiVESAttachOptions)(0), 0, 0);

      // event id
      text = lives_strdup_printf(_("Event id=%p"), (void *)event);
      label = lives_standard_label_new(text);
      lives_free(text);
      lives_widget_set_valign(label, LIVES_ALIGN_START);

      lives_table_attach(LIVES_TABLE(table), label, 2, 3, currow, currow + 1,
                         (LiVESAttachOptions)(LIVES_EXPAND),
                         (LiVESAttachOptions)(0), 0, 0);

      // properties
      tree = lives_tree_view_new_with_model(LIVES_TREE_MODEL(treestore));

      if (palette->style & STYLE_1) {
        lives_widget_set_base_color(tree, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
        lives_widget_set_text_color(tree, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
      }

      renderer = lives_cell_renderer_text_new();
      column = lives_tree_view_column_new_with_attributes(NULL,
               renderer, LIVES_TREE_VIEW_COLUMN_TEXT, TITLE_COLUMN, NULL);

      lives_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
      lives_tree_view_append_column(LIVES_TREE_VIEW(tree), column);

      renderer = lives_cell_renderer_text_new();
      GValue gval = G_VALUE_INIT;
      g_value_init(&gval, G_TYPE_INT);
      g_value_set_int(&gval, 12);
      gtk_cell_renderer_set_padding(renderer, widget_opts.packing_width, 0);
      column = lives_tree_view_column_new_with_attributes(_("Keys"),
               renderer, LIVES_TREE_VIEW_COLUMN_TEXT, KEY_COLUMN, NULL);
      lives_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
      gtk_tree_view_column_set_expand(column, TRUE);
      lives_tree_view_append_column(LIVES_TREE_VIEW(tree), column);

      renderer = lives_cell_renderer_text_new();
      gtk_cell_renderer_set_padding(renderer, widget_opts.packing_width, 0);
      column = lives_tree_view_column_new_with_attributes(_("Values"),
               renderer, LIVES_TREE_VIEW_COLUMN_TEXT, VALUE_COLUMN, NULL);
      lives_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
      gtk_tree_view_column_set_expand(column, TRUE);
      lives_tree_view_append_column(LIVES_TREE_VIEW(tree), column);

      renderer = lives_cell_renderer_text_new();
      gtk_cell_renderer_set_padding(renderer, widget_opts.packing_width, 0);
      column = lives_tree_view_column_new_with_attributes(_("Description"),
               renderer, LIVES_TREE_VIEW_COLUMN_TEXT, DESC_COLUMN, NULL);
      lives_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
      gtk_tree_view_column_set_expand(column, TRUE);
      lives_tree_view_append_column(LIVES_TREE_VIEW(tree), column);

      lives_table_attach(LIVES_TABLE(table), tree, 3, 6, currow, currow + 1,
                         (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                         (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND), 0, 0);

#if GTK_CHECK_VERSION(3, 0, 0)
      lives_signal_sync_connect(LIVES_GUI_OBJECT(tree), LIVES_WIDGET_ROW_EXPANDED_SIGNAL,
                                LIVES_GUI_CALLBACK(rowexpand), NULL);

      lives_widget_set_size_request(tree, -1, TREE_ROW_HEIGHT);
#endif
      currow++;
      gtk_tree_view_set_fixed_height_mode(LIVES_TREE_VIEW(tree), TRUE);
    }
    if (event == event_list) event = get_first_event(event_list);
    else event = get_next_event(event);
  }

  lives_freep((void **)&iname);

  widget_opts.apply_theme = 0;
  scrolledwindow = lives_standard_scrolled_window_new(winsize_h, winsize_v, table);
  widget_opts.apply_theme = woat;

#if !GTK_CHECK_VERSION(3, 0, 0)
  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(top_vbox, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_fg_color(top_vbox, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
    lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(scrolledwindow)), LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
  }
#endif

  lives_box_pack_start(LIVES_BOX(top_vbox), scrolledwindow, TRUE, TRUE, widget_opts.packing_height);

  if (prefs->show_dev_opts) {
    if (CURRENT_CLIP_IS_VALID) {
      char *tmp = lives_strdup_printf("Quantise to %.4f fps", cfile->fps);
      LiVESWidget *qbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(event_dialog), NULL, tmp,
                             LIVES_RESPONSE_OK);
      lives_free(tmp);
      lives_signal_sync_connect(LIVES_GUI_OBJECT(qbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(quant_clicked), (livespointer)event_list);
    }
  }
  ok_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(event_dialog), LIVES_STOCK_CLOSE, _("_Close Window"),
              LIVES_RESPONSE_OK);

  lives_button_grab_default_special(ok_button);

  lives_window_add_escape(LIVES_WINDOW(event_dialog), ok_button);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(ok_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);

  if (prefs->gui_monitor != 0) {
    lives_window_center(LIVES_WINDOW(event_dialog));
  }

  daa = lives_dialog_get_action_area(LIVES_DIALOG(event_dialog));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(daa), LIVES_BUTTONBOX_SPREAD);

  if (prefs->open_maximised) {
    lives_window_unmaximize(LIVES_WINDOW(event_dialog));
    lives_window_maximize(LIVES_WINDOW(event_dialog));
  }

  lives_widget_show_all(event_dialog);
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  return event_dialog;
}


void rdetw_spinh_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  render_details *rdet = (render_details *)user_data;
  rdet->height = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
}


void rdetw_spinw_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  render_details *rdet = (render_details *)user_data;
  rdet->width = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
}


void rdetw_spinf_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  render_details *rdet = (render_details *)user_data;
  rdet->fps = lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton));
}


LiVESWidget *add_video_options(LiVESWidget **spwidth, int defwidth, LiVESWidget **spheight, int defheight,
                               LiVESWidget **spfps, double deffps, LiVESWidget **spframes, int defframes,
                               LiVESWidget **as_lock, LiVESWidget * extra) {
  // add video options to multitrack enter, etc
  LiVESWidget *vbox, *hbox, *layout;
  LiVESWidget *frame = lives_standard_frame_new(_("Video"), 0., FALSE);

  double width_step = 4.;
  double height_step = 4.;

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);

  layout = lives_layout_new(LIVES_BOX(vbox));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  if (spframes) {
    *spframes = lives_standard_spin_button_new
                (_("_Number of frames"), defframes, 1., 100000, 1., 5., 0, LIVES_BOX(hbox), NULL);
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  }

  *spfps = lives_standard_spin_button_new
           (_("_Frames per second"), deffps, 1., FPS_MAX, .1, 1., 3, LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  *spwidth = lives_standard_spin_button_new
             (_("_Width"), defwidth, width_step, MAX_FRAME_WIDTH, -width_step, width_step, 0, LIVES_BOX(hbox), NULL);
  lives_spin_button_set_snap_to_multiples(LIVES_SPIN_BUTTON(*spwidth), width_step);
  lives_spin_button_update(LIVES_SPIN_BUTTON(*spwidth));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  *spheight = lives_standard_spin_button_new
              (_("_Height"), defheight, height_step, MAX_FRAME_WIDTH, -height_step, height_step, 0, LIVES_BOX(hbox), NULL);
  lives_spin_button_set_snap_to_multiples(LIVES_SPIN_BUTTON(*spheight), height_step);
  lives_spin_button_update(LIVES_SPIN_BUTTON(*spheight));

  // add aspect button ?
  if (as_lock) {
    if (CURRENT_CLIP_IS_VALID) {
      const lives_special_aspect_t *aspect;
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
      aspect = add_aspect_ratio_button(LIVES_SPIN_BUTTON(*spwidth), LIVES_SPIN_BUTTON(*spheight), LIVES_BOX(hbox));
      *as_lock = aspect->lockbutton;
    } else *as_lock = NULL;
  }

  if (extra) lives_box_pack_start(LIVES_BOX(vbox), extra, FALSE, FALSE, widget_opts.packing_height);

  return frame;
}


static void add_fade_elements(render_details * rdet, LiVESWidget * hbox, boolean is_video, double maxtime, boolean def_on) {
  LiVESWidget *cb;
  LiVESWidget *vbox = NULL;
  double def_fade_time;
  if (!is_video) def_fade_time = DEF_AFADE_SECS;
  else def_fade_time = DEF_VFADE_SECS;
  if (maxtime > 0.) {
    if (def_fade_time > maxtime / 4.) {
      def_fade_time = maxtime / 4.;
      if (def_fade_time > 1.) def_fade_time = (double)((int)def_fade_time);
    }
  }

  if (is_video) {
    vbox = lives_vbox_new(FALSE, widget_opts.packing_height >> 1);
    lives_box_pack_start(LIVES_BOX(hbox), vbox, FALSE, TRUE, 0);
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, 0);
  }
  add_fill_to_box(LIVES_BOX(hbox));

  cb = lives_standard_check_button_new(_("Fade in over"), FALSE, LIVES_BOX(hbox), NULL);

  widget_opts.swap_label = TRUE;
  if (!is_video) {
    rdet->afade_in = lives_standard_spin_button_new(_("seconds"),
                     def_fade_time, 0., 1000., 1., 1., 2,
                     LIVES_BOX(hbox), NULL);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(cb), rdet->afade_in, FALSE);
  } else {
    rdet->vfade_in = lives_standard_spin_button_new(_("seconds"),
                     def_fade_time, 0., 1000., 1., 1., 2,
                     LIVES_BOX(hbox), NULL);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(cb), rdet->vfade_in, FALSE);
  }
  widget_opts.swap_label = FALSE;

  add_fill_to_box(LIVES_BOX(hbox));

  cb = lives_standard_check_button_new(_("Fade out over"), def_on, LIVES_BOX(hbox), NULL);

  widget_opts.swap_label = TRUE;
  if (!is_video) {
    rdet->afade_out = lives_standard_spin_button_new(_("seconds"),
                      def_fade_time, 0., 1000., 1., 1., 2,
                      LIVES_BOX(hbox), NULL);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(cb), rdet->afade_out, FALSE);
  } else {
    rdet->vfade_out = lives_standard_spin_button_new(_("seconds"),
                      def_fade_time, 0., 1000., 1., 1., 2,
                      LIVES_BOX(hbox), NULL);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(cb), rdet->vfade_out, FALSE);
  }
  widget_opts.swap_label = FALSE;

  add_fill_to_box(LIVES_BOX(hbox));

  if (is_video) {
    lives_colRGBA64_t rgba;
    LiVESWidget *sp_red, *sp_green, *sp_blue;
    rgba.red = rgba.green = rgba.blue = 0;
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, 0);
    add_fill_to_box(LIVES_BOX(hbox));
    rdet->vfade_col = lives_standard_color_button_new(LIVES_BOX(hbox), _("Fade Color"),
                      FALSE, &rgba, &sp_red, &sp_green, &sp_blue, NULL);
  }
}


LiVESWidget *add_audio_options(LiVESWidget **cbbackaudio, LiVESWidget **cbpertrack) {
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);

  //*cbbackaudio = lives_standard_check_button_new(_("Enable _backing audio track"), FALSE, LIVES_BOX(hbox), NULL);
  *cbbackaudio = lives_standard_check_button_new(_("Enable _backing audio track"), TRUE, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  *cbpertrack = lives_standard_check_button_new(_("Audio track _per video track"), FALSE, LIVES_BOX(hbox), NULL);

  return hbox;
}


static void rdet_use_current(LiVESButton * button, livespointer user_data) {
  render_details *rdet = (render_details *)user_data;
  char *arate, *achans, *asamps;
  int aendian;

  if (!CURRENT_CLIP_IS_VALID) return;

  if (CURRENT_CLIP_HAS_VIDEO) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_width), (double)cfile->hsize);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_height), (double)cfile->vsize);
    if (lives_widget_get_sensitive(rdet->spinbutton_fps))
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_fps), cfile->fps);
    lives_spin_button_update(LIVES_SPIN_BUTTON(rdet->spinbutton_width));

    if (rdet->as_lock) {
      lives_lock_button_set_locked(LIVES_BUTTON(rdet->as_lock), TRUE);
      lives_widget_show_all(rdet->as_lock);
    }
    rdet->ratio_fps = cfile->ratio_fps;
  }

  if (cfile->achans > 0) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton), TRUE);

    arate = lives_strdup_printf("%d", cfile->arate);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_arate), arate);
    lives_free(arate);

    achans = lives_strdup_printf("%d", cfile->achans);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_achans), achans);
    lives_free(achans);

    asamps = lives_strdup_printf("%d", cfile->asampsize);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_asamps), asamps);
    lives_free(asamps);

    aendian = cfile->signed_endian;

    if (aendian & AFORM_UNSIGNED) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed), TRUE);
    }

    if (aendian & AFORM_BIG_ENDIAN) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_littleend), TRUE);
    }
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton), FALSE);
  }
}


static void check_lb_conflict(LiVESToggleButton * b, livespointer p) {
  char *msg = NULL;
  if (lives_toggle_button_get_active(b)) {
    if (!prefs->letterbox) msg = _("Letterboxing was not selected during recording.");
  } else {
    if (prefs->letterbox) msg = _("Letterboxing was enabled during recording.");
  }
  if (msg) {
    char *msg2 = lives_strdup_printf(_("%s.\nUsing identical settings for recording "
                                       "and encoding is recommended.\n"
                                       "(The default value for this can be changed in Preferences / Encoding.)"), msg);
    show_warn_image(LIVES_WIDGET(b), msg2);
    lives_free(msg); lives_free(msg2);
  } else hide_warn_image(LIVES_WIDGET(b));
}


render_details *create_render_details(int type) {
  // type == 1 :: pre-save (specified)
  // type == 2 :: render to new clip (!specified)
  // type == 3 :: enter multitrack (!specified)
  // type == 4 :: change during multitrack (!specified)
  // type == 5 :: transcode clip (!specified) -> becomes type 2

  LiVESWidget *label;
  LiVESWidget *top_vbox;
  LiVESWidget *dialog_vbox;
  LiVESWidget *scrollw = NULL;
  LiVESWidget *hbox;
  LiVESWidget *layout;
  LiVESWidget *frame;
  LiVESWidget *cancelbutton;
  LiVESWidget *daa;
  LiVESWidget *cb_letter;
  LiVESWidget *spillover;

  LiVESList *ofmt_all = NULL;
  LiVESList *ofmt = NULL;
  LiVESList *encoders = NULL;

  char **array;

  char *tmp, *tmp2, *tmp3;
  char *title;

  boolean needs_new_encoder = FALSE;
  boolean is_transcode = FALSE;

  int width, height, dwidth, dheight, spht, maxwidth, maxheight;

  int scrw = GUI_SCREEN_WIDTH;
  int scrh = GUI_SCREEN_HEIGHT;
  int dbw;

  int i;

  if (type == 5) {
    is_transcode = TRUE;
    type = 2;
  }
  mainw->no_context_update = TRUE;

  rdet = (render_details *)lives_calloc(1, sizeof(render_details));

  rdet->is_encoding = FALSE;

  if (type == 3 || (mainw->multitrack && type != 4)
      || !CURRENT_CLIP_IS_VALID || mainw->current_file == mainw->scrap_file) {
    rdet->width = prefs->mt_def_width;
    rdet->height = prefs->mt_def_height;
    rdet->fps = prefs->mt_def_fps;
    rdet->ratio_fps = FALSE;

    rdet->arate = prefs->mt_def_arate;
    rdet->achans = prefs->mt_def_achans;
    rdet->asamps = prefs->mt_def_asamps;
    rdet->aendian = prefs->mt_def_signed_endian;
  } else {
    int player_arate = get_aplay_rate();
    rdet->width = DEF_FRAME_HSIZE_UNSCALED;
    rdet->height = DEF_FRAME_VSIZE_UNSCALED;
    rdet->fps = prefs->default_fps;
    rdet->ratio_fps = FALSE;

    if (type == 4 || player_arate == -1)
      rdet->arate = cfile->arps ? cfile->arps : DEFAULT_AUDIO_RATE;
    else rdet->arate = player_arate;
    rdet->achans = DEFAULT_AUDIO_CHANS;
    rdet->asamps = DEFAULT_AUDIO_SAMPS;
    rdet->aendian = CURRENT_CLIP_HAS_AUDIO ? cfile->signed_endian
                    : capable->hw.byte_order == LIVES_BIG_ENDIAN;
  }

  rdet->enc_changed = FALSE;

  if (type == 3 || type == 4) {
    title = (_("Multitrack Details"));
  } else if (type == 1) title = (_("Encoding Details"));
  else {
    if (!is_transcode) title = (_("New Clip Details"));
    else title = (_("Quick Transcode"));
  }

  maxwidth = width = scrw - SCR_WIDTH_SAFETY;
  maxheight = height = scrh - SCR_HEIGHT_SAFETY;

  if (type == 1) {
    width >>= 1;
    height >>= 1;
  }

  if (type == 3) width = (width * 3) >> 2;

  widget_opts.expand = LIVES_EXPAND_NONE;
  rdet->dialog = lives_standard_dialog_new(title, FALSE, width, height);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(rdet->dialog));

  top_vbox = lives_vbox_new(FALSE, 0);

  if (type != 1) {
    dbw = widget_opts.border_width;
    widget_opts.border_width = 0;
    // need to set a large enough default here
    scrollw = lives_standard_scrolled_window_new(width * .8, height * .5, top_vbox);
    widget_opts.border_width = dbw;
    lives_box_pack_start(LIVES_BOX(dialog_vbox), scrollw, FALSE, TRUE, 0);
  } else lives_box_pack_start(LIVES_BOX(dialog_vbox), top_vbox, FALSE, TRUE, 0);

  lives_container_set_border_width(LIVES_CONTAINER(top_vbox), 0);

  daa = lives_dialog_get_action_area(LIVES_DIALOG(rdet->dialog));

  rdet->always_checkbutton = lives_standard_check_button_new((tmp = (_("_Always use these values"))), FALSE,
                             LIVES_BOX(daa),
                             (tmp2 = lives_strdup(
                                       H_("Check this button to always use these values when entering "
                                          "multitrack mode. "
                                          "Choice can be re-enabled from Preferences / Multitrack"))));
  lives_button_box_make_first(LIVES_BUTTON_BOX(daa), widget_opts.last_container);

  rdet->always_hbox = widget_opts.last_container;
  if (type == 1 || type == 2) lives_widget_set_no_show_all(rdet->always_hbox, TRUE);

  lives_free(tmp); lives_free(tmp2);

  if (type == 4) {
    hbox = lives_hbox_new(FALSE, 0);
    cb_letter = lives_standard_check_button_new(_("Apply letterboxing"), prefs->letterbox_mt,
                LIVES_BOX(hbox), (tmp = H_("Defines whether black borders will be added when resizing frames\n"
                                           "in order to preserve the original aspect ratio")));
    lives_free(tmp);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(cb_letter), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(toggle_sets_pref), (livespointer)PREF_LETTERBOX_MT);
  } else hbox = NULL;

  if (type == 3) {
    widget_opts.use_markup = TRUE;
    label = lives_standard_label_new(_("<b>Before entering multitrack mode with an empty event list,"
                                       "you need to define the format to used for rendering and previews</b>\n"
                                       "The majority of these values can be amended later if so desired"));
    widget_opts.use_markup = FALSE;
    lives_box_pack_start(LIVES_BOX(top_vbox), label, FALSE, TRUE, 0);
  }
  if (is_transcode) {
    hbox = lives_hbox_new(FALSE, 0);
    rdet->enc_lb = lives_standard_check_button_new(_("Use letterboxing to fill frame"),
                   prefs->enc_letterbox, LIVES_BOX(hbox), NULL);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(rdet->enc_lb), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(check_lb_conflict), NULL);
    check_lb_conflict(rdet->enc_lb, NULL);
  }

  frame = add_video_options(&rdet->spinbutton_width, rdet->width, &rdet->spinbutton_height, rdet->height, &rdet->spinbutton_fps,
                            rdet->fps, NULL, 0., &rdet->as_lock, hbox);
  if (rdet->as_lock)
    lives_lock_button_set_locked(LIVES_BUTTON(rdet->as_lock), FALSE);
  lives_box_pack_start(LIVES_BOX(top_vbox), frame, FALSE, TRUE, 0);

  if (type == 1) lives_widget_set_no_show_all(frame, TRUE);

  if (type == 2) {
    if (mainw->event_list && weed_plant_has_leaf(mainw->event_list, WEED_LEAF_FPS)) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_fps),
                                  weed_get_double_value(mainw->event_list, WEED_LEAF_FPS, NULL));
      lives_widget_set_sensitive(rdet->spinbutton_fps, FALSE);
    }

    if (!is_transcode) {
      // add clip name entry
      rdet->clipname_entry = lives_standard_entry_new((tmp = (_("New clip name"))),
                             (tmp2 = get_untitled_name(mainw->untitled_number)),
                             MEDIUM_ENTRY_WIDTH, 256, LIVES_BOX(top_vbox),
                             (tmp3 = (_("The name to give the clip in the Clips menu"))));
      lives_free(tmp); lives_free(tmp2); lives_free(tmp3);
    }

    /* add_fill_to_box(LIVES_BOX(lives_bin_get_child(LIVES_BIN(frame)))); */
    /* hbox = lives_hbox_new(FALSE, 0); */
    /* lives_container_add(LIVES_CONTAINER(lives_bin_get_child(LIVES_BIN(frame))), hbox); */
    /* add_fill_to_box(LIVES_BOX(hbox)); */
    /* add_fade_elements(rdet, hbox, TRUE); */
  }
  // call these here since adding the widgets may have altered their values
  rdetw_spinw_changed(LIVES_SPIN_BUTTON(rdet->spinbutton_width), (livespointer)rdet);
  rdetw_spinh_changed(LIVES_SPIN_BUTTON(rdet->spinbutton_height), (livespointer)rdet);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rdet->spinbutton_width), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(rdetw_spinw_changed), rdet);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rdet->spinbutton_height), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(rdetw_spinh_changed), rdet);

  if (type == 4 && mainw->multitrack->event_list) lives_widget_set_sensitive(rdet->spinbutton_fps, FALSE);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rdet->spinbutton_fps), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(rdetw_spinf_changed), rdet);

  rdet->pertrack_checkbutton = lives_check_button_new();
  rdet->backaudio_checkbutton = lives_check_button_new();

  if (is_transcode) rdet->asamps = -32;

  ////// add audio part
  resaudw = NULL;
  if (type == 3 || type == 2) resaudw = create_resaudw(3, rdet, top_vbox); // enter mt, render to clip
  else if (type == 4 && cfile->achans != 0) {
    resaudw = create_resaudw(10, rdet, top_vbox); // change during mt. Channels fixed.
  }

  if (type == 2) {
    double max_time = 0.;
    add_fill_to_box(LIVES_BOX(resaudw->vbox));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(resaudw->vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    rdet->norm_after = lives_standard_check_button_new(_("_Normalise audio after rendering"),
                       TRUE, LIVES_BOX(hbox), NULL);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton), rdet->norm_after, FALSE);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(resaudw->vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    if (mainw->event_list) max_time = (double)(get_event_timecode(get_last_frame_event(mainw->event_list))
                                        - get_event_timecode(get_first_frame_event(mainw->event_list)))
                                        / TICKS_PER_SECOND_DBL + 1. / lives_spin_button_get_value(LIVES_SPIN_BUTTON(rdet->spinbutton_fps));
    add_fade_elements(rdet, hbox, FALSE, max_time, is_transcode);
  }

  if (type == 3) {
    // extra opts
    label = lives_standard_label_new(_("Options"));
    lives_box_pack_start(LIVES_BOX(top_vbox), label, FALSE, FALSE, widget_opts.packing_height);
    hbox = add_audio_options(&rdet->backaudio_checkbutton, &rdet->pertrack_checkbutton);
    lives_box_pack_start(LIVES_BOX(top_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rdet->backaudio_checkbutton), prefs->mt_backaudio > 0);

    lives_widget_set_sensitive(rdet->backaudio_checkbutton,
                               lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rdet->pertrack_checkbutton), prefs->mt_pertrack_audio);

    lives_widget_set_sensitive(rdet->pertrack_checkbutton,
                               lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)));
  }

  if (!is_transcode) {
    if (capable->has_encoder_plugins == PRESENT) {
      encoders = lives_list_copy_strings(capable->plugins_list[PLUGIN_TYPE_ENCODER]);
      if (type != 1) encoders = filter_encoders_by_img_ext(encoders, prefs->image_ext);
      else {
        LiVESList *encs = encoders = filter_encoders_by_img_ext(encoders, get_image_ext_for_type(cfile->img_type));
        needs_new_encoder = TRUE;
        if (*prefs->encoder.name) {
          while (encs) {
            if (!strcmp((char *)encs->data, prefs->encoder.name)) {
              needs_new_encoder = FALSE;
              break;
            }
            encs = encs->next;
	    // *INDENT-OFF*
	  }}}}
    // *INDENT-ON*

    if (type != 1) {
      add_hsep_to_box(LIVES_BOX(top_vbox));
      if (type != 3) {
        label = lives_standard_label_new(_("Options"));
        lives_box_pack_start(LIVES_BOX(top_vbox), label, FALSE, FALSE, widget_opts.packing_height);
      }
    }

    /////////////////////// add expander ////////////////////////////

    if (type != 1) {
      LiVESWidget *expd;
      hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(top_vbox), hbox, FALSE, FALSE, 0);
      layout = lives_layout_new(NULL);
      expd = lives_standard_expander_new(_("_Encoder preferences (optional)"), LIVES_BOX(hbox), layout);
      lives_widget_set_tooltip_text(expd, H_("Setting these options will not affect the output directly\n"
                                             "but a small number of formats may prefer specific video and audio\n"
                                             "configurations, which may be used to adjust the values above"));
    } else layout = lives_layout_new(LIVES_BOX(top_vbox));

    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
    lives_layout_add_row(LIVES_LAYOUT(layout));

    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    label = lives_layout_add_label(LIVES_LAYOUT(layout), _("Target encoder"), FALSE);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

    if (type != 1) {
      rdet->encoder_name = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);
      encoders = lives_list_prepend(encoders, lives_strdup(rdet->encoder_name));
    } else {
      rdet->encoder_name = lives_strdup(prefs->encoder.name);
    }

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    rdet->encoder_combo = lives_standard_combo_new(NULL, encoders, LIVES_BOX(hbox), NULL);
    lives_widget_set_halign(rdet->encoder_combo, LIVES_ALIGN_CENTER);

    rdet->encoder_name_fn = lives_signal_sync_connect_after(LIVES_COMBO(rdet->encoder_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_encoder_entry_changed), rdet);

    lives_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
    lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), rdet->encoder_name);
    lives_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);

    lives_list_free_all(&encoders);

    if (type != 1) {
      ofmt = lives_list_append(ofmt, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));
    } else {
      if (capable->has_encoder_plugins == PRESENT) {
        // request formats from the encoder plugin
        if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS, prefs->encoder.name, "get_formats")) != NULL) {
          for (i = 0; i < lives_list_length(ofmt_all); i++) {
            if (get_token_count((char *)lives_list_nth_data(ofmt_all, i), '|') > 2) {
              array = lives_strsplit((char *)lives_list_nth_data(ofmt_all, i), "|", -1);
              if (!strcmp(array[0], prefs->encoder.of_name)) {
                prefs->encoder.of_allowed_acodecs = atoi(array[2]);
              }
              ofmt = lives_list_append(ofmt, lives_strdup(array[1]));
              lives_strfreev(array);
            }
          }
          lives_list_free_all(&ofmt_all);
        } else {
          future_prefs->encoder.of_allowed_acodecs = 0;
        }
      }
    }

    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Output format"), FALSE);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    rdet->ofmt_combo = lives_standard_combo_new(NULL, ofmt, LIVES_BOX(hbox), NULL);
    lives_widget_set_halign(rdet->ofmt_combo, LIVES_ALIGN_CENTER);

    lives_combo_populate(LIVES_COMBO(rdet->ofmt_combo), ofmt);
    lives_list_free_all(&ofmt);

    rdet->encoder_ofmt_fn = lives_signal_sync_connect_after(LIVES_COMBO(rdet->ofmt_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_encoder_ofmt_changed), rdet);

    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Audio format"), FALSE);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

    if (type != 1) {
      // add "Any" string
      lives_list_free_all(&prefs->acodec_list);

      prefs->acodec_list = lives_list_append(prefs->acodec_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));

      hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

      rdet->acodec_combo = lives_standard_combo_new(NULL, prefs->acodec_list, LIVES_BOX(hbox), NULL);
      lives_widget_set_halign(rdet->acodec_combo, LIVES_ALIGN_CENTER);
    } else {
      lives_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
      lives_combo_set_active_string(LIVES_COMBO(rdet->ofmt_combo), prefs->encoder.of_desc);
      lives_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);

      hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

      rdet->acodec_combo = lives_standard_combo_new(NULL, NULL, LIVES_BOX(hbox), NULL);
      lives_widget_set_halign(rdet->acodec_combo, LIVES_ALIGN_CENTER);

      check_encoder_restrictions(TRUE, FALSE, TRUE);
      future_prefs->encoder.of_allowed_acodecs = prefs->encoder.of_allowed_acodecs;
      set_acodec_list_from_allowed(NULL, rdet);
    }
    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  }

  //////////////// end expander section ///////////////////
  rdet->debug = NULL;

  if (type == 1 && prefs->show_dev_opts) {
    hbox = lives_hbox_new(FALSE, 0);
    //add_spring_to_box(LIVES_BOX(hbox), 0);

    rdet->debug = lives_standard_check_button_new((tmp = (_("Debug Mode"))), FALSE,
                  LIVES_BOX(hbox), (tmp2 = (_("Output diagnostic information to STDERR "
                                            "instead of to the GUI."))));
    lives_free(tmp); lives_free(tmp2);

    lives_box_pack_start(LIVES_BOX(top_vbox), hbox, FALSE, FALSE, 0);
    //lives_widget_set_halign(rdet->acodec_combo, LIVES_ALIGN_CENTER);
    add_spring_to_box(LIVES_BOX(hbox), 0);
  }

  add_fill_to_box(LIVES_BOX(dialog_vbox));
  cancelbutton = NULL;

  if (!(prefs->startup_interface == STARTUP_MT && !mainw->is_ready)) {
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(rdet->dialog), LIVES_STOCK_CANCEL, NULL,
                   LIVES_RESPONSE_CANCEL);
  } else if (LIVES_IS_BOX(daa)) add_fill_to_box(LIVES_BOX(daa));

  if (!is_transcode && !(prefs->startup_interface == STARTUP_MT && !mainw->is_ready)) {
    if (type == 2 || type == 3) {
      if (CURRENT_CLIP_HAS_VIDEO && mainw->current_file != mainw->scrap_file && mainw->current_file != mainw->ascrap_file) {
        widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_EXTRA_WIDTH;
        rdet->usecur_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(rdet->dialog),
                              NULL, _("_Set to current clip values"), LIVES_RESPONSE_RESET);
        widget_opts.expand = LIVES_EXPAND_DEFAULT;
        lives_signal_sync_connect(rdet->usecur_button, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(rdet_use_current),
                                  (livespointer)rdet);
      }
    }
  }

  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT;
  if (type != 1 && !is_transcode) {
    rdet->okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(rdet->dialog), LIVES_STOCK_OK, NULL, LIVES_RESPONSE_OK);
  } else  {
    rdet->okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(rdet->dialog), LIVES_STOCK_GO_FORWARD, _("_Next"),
                     LIVES_RESPONSE_OK);
  }
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  lives_button_grab_default_special(rdet->okbutton);

  lives_window_add_escape(LIVES_WINDOW(rdet->dialog), cancelbutton);

  if (!is_transcode) {
    if (needs_new_encoder) {
      lives_widget_set_sensitive(rdet->okbutton, FALSE);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET); // force showing of transient window
      if (*prefs->encoder.name) do_encoder_img_fmt_error(rdet);
    }

    lives_signal_sync_connect_after(LIVES_COMBO(rdet->acodec_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(rdet_acodec_changed), rdet);
  }

  mainw->no_context_update = FALSE;

  if (type != 1) {
    // shrinkwrap to minimum
    spillover = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(top_vbox), spillover, TRUE, TRUE, 0); // mop up extra height
    lives_widget_show_all(rdet->dialog);
    lives_widget_context_update();

    height = lives_widget_get_allocation_height(scrollw) - (spht = lives_widget_get_allocation_height(spillover));
    width = lives_widget_get_allocation_width(scrollw);

    dheight = lives_widget_get_allocation_height(rdet->dialog) - spht;
    dwidth = lives_widget_get_allocation_width(rdet->dialog);

    if (dwidth > maxwidth) dwidth = maxwidth;
    if (dheight > maxheight) dheight = maxheight;

    if (width > dwidth) width = dwidth;
    if (height > dheight) height = dheight;

    lives_widget_destroy(spillover); // remove extra height
    lives_widget_process_updates(rdet->dialog);
    lives_widget_context_update();

    if (width > 0) lives_scrolled_window_set_min_content_width(LIVES_SCROLLED_WINDOW(scrollw), width);
    if (height > 0) lives_scrolled_window_set_min_content_height(LIVES_SCROLLED_WINDOW(scrollw), height);
    //lives_widget_set_size_request(scrollw, width, height);
    lives_widget_set_maximum_size(scrollw, width, height);

    if (dwidth < width + 6. * widget_opts.border_width) dwidth = width + 6. * widget_opts.border_width;
    if (dheight < height + 6. * widget_opts.border_width) dheight = height + 6. * widget_opts.border_width;

    lives_widget_set_size_request(rdet->dialog, dwidth, dheight);
    lives_widget_set_maximum_size(rdet->dialog, dwidth, dheight);

    // for expander, need to make it resizable
    lives_window_set_resizable(LIVES_WINDOW(rdet->dialog), TRUE);
  }

  lives_widget_show_all(rdet->dialog);
  return rdet;
}

