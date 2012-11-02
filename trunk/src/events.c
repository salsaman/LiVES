// events.c
// LiVES
// (c) G. Finch 2005 - 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions/structs for event_lists and events

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"
#include "effects.h"
#include "support.h"
#include "callbacks.h"
#include "resample.h"
#include "paramwindow.h" // for DEF_BUTTON_WIDTH
#include "audio.h"
#include "cvirtual.h"

//////////////////////////////////
//#define DEBUG_EVENTS


static gint render_choice;

static void **pchains[FX_KEYS_MAX]; // each pchain is an array of void *, these are parameter changes used for rendering
///////////////////////////////////////////////////////

G_GNUC_PURE void ***get_event_pchains(void) {
  return pchains;
}


LIVES_INLINE weed_timecode_t get_event_timecode (weed_plant_t *plant) {
  weed_timecode_t tc;
  int error;
  tc=weed_get_int64_value (plant,"timecode",&error);
  return tc;
}

LIVES_INLINE int get_event_hint (weed_plant_t *plant) {
  int hint;
  int error;
  hint=weed_get_int_value (plant,"hint",&error);
  return hint;
}


LIVES_INLINE weed_plant_t *get_prev_event(weed_plant_t *event) {
  int error;
  if (!weed_plant_has_leaf(event,"previous")) return NULL;
  return (weed_plant_t *)weed_get_voidptr_value(event,"previous",&error);
}

LIVES_INLINE weed_plant_t *get_next_event(weed_plant_t *event) {
  int error;
  if (!weed_plant_has_leaf(event,"next")) return NULL;
  return (weed_plant_t *)weed_get_voidptr_value(event,"next",&error);
}

LIVES_INLINE weed_plant_t *get_first_event(weed_plant_t *event_list) {
  int error;
  if (!weed_plant_has_leaf(event_list,"first")) return NULL;
  return (weed_plant_t *)weed_get_voidptr_value(event_list,"first",&error);
}

LIVES_INLINE weed_plant_t *get_last_event(weed_plant_t *event_list) {
  int error;
  if (!weed_plant_has_leaf(event_list,"last")) return NULL;
  return (weed_plant_t *)weed_get_voidptr_value(event_list,"last",&error);
}


gboolean has_frame_event_at(weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t **shortcut) {
  weed_plant_t *event;
  weed_timecode_t ev_tc;

  if (shortcut==NULL||*shortcut==NULL) event=get_first_event(event_list);
  else event=*shortcut;

  while ((ev_tc=get_event_timecode(event))<=tc) {
    if (ev_tc==tc&&WEED_EVENT_IS_FRAME(event)) {
      *shortcut=event;
      return TRUE;
    }
    event=get_next_event(event);
  }
  return FALSE;
}


int get_audio_frame_clip (weed_plant_t *event, int layer) {
  int numaclips,aclipnum=0;
  int *aclips,error,i;
  if (get_event_hint (event)!=WEED_EVENT_HINT_FRAME) return -2;
  numaclips=weed_leaf_num_elements (event,"audio_clips");
  aclips=weed_get_int_array(event,"audio_clips",&error);
  for (i=0;i<numaclips;i+=2) {
    if (aclips[i]==layer) {
      aclipnum=aclips[i+1];
      break;
    }
  }
  weed_free(aclips);
  return aclipnum;
}


double get_audio_frame_vel (weed_plant_t *event, int layer) {
  int numaclips;
  int *aclips,error,i;
  double *aseeks,avel=1.;

  if (get_event_hint (event)!=WEED_EVENT_HINT_FRAME) return -2;
  numaclips=weed_leaf_num_elements (event,"audio_clips");
  aclips=weed_get_int_array(event,"audio_clips",&error);
  aseeks=weed_get_double_array(event,"audio_seeks",&error);
  for (i=0;i<numaclips;i+=2) {
    if (aclips[i]==layer) {
      avel=aseeks[i+1];
      break;
    }
  }
  weed_free(aclips);
  weed_free(aseeks);
  return avel;
}


double get_audio_frame_seek (weed_plant_t *event, int layer) {
  int numaclips;
  int *aclips,error,i;
  double *aseeks,aseek=0.;

  if (get_event_hint (event)!=WEED_EVENT_HINT_FRAME) return -2;
  numaclips=weed_leaf_num_elements (event,"audio_clips");
  aclips=weed_get_int_array(event,"audio_clips",&error);
  aseeks=weed_get_double_array(event,"audio_seeks",&error);
  for (i=0;i<numaclips;i+=2) {
    if (aclips[i]==layer) {
      aseek=aseeks[i];
      break;
    }
  }
  weed_free(aclips);
  weed_free(aseeks);
  return aseek;
}


int get_frame_event_clip (weed_plant_t *event, int layer) {
  int numclips,clipnum;
  int *clips,error;
  if (get_event_hint (event)!=WEED_EVENT_HINT_FRAME) return -2;
  numclips=weed_leaf_num_elements (event,"clips");
  clips=weed_get_int_array(event,"clips",&error);
  if (numclips<=layer) return -3;
  clipnum=clips[layer];
  weed_free(clips);
  return clipnum;
}

int get_frame_event_frame (weed_plant_t *event, int layer) {
  int numframes,framenum;
  int *frames,error;
  if (get_event_hint (event)!=WEED_EVENT_HINT_FRAME) return -2;
  numframes=weed_leaf_num_elements (event,"frames");
  frames=weed_get_int_array(event,"frames",&error);
  if (numframes<=layer) return -3;
  framenum=frames[layer];
  weed_free(frames);
  return framenum;
}

void unlink_event(weed_plant_t *event_list, weed_plant_t *event) {
  // unlink event from event_list
  // don't forget to adjust "timecode" before re-inserting !
  weed_plant_t *prev_event=get_prev_event(event);
  weed_plant_t *next_event=get_next_event(event);

  if (prev_event!=NULL) weed_set_voidptr_value(prev_event,"next",next_event);
  if (next_event!=NULL) weed_set_voidptr_value(next_event,"previous",prev_event);

  if (get_first_event(event_list)==event) weed_set_voidptr_value(event_list,"first",next_event);
  if (get_last_event(event_list)==event) weed_set_voidptr_value(event_list,"last",prev_event);

}

void delete_event(weed_plant_t *event_list, weed_plant_t *event) {
  // delete event from event_list
  threaded_dialog_spin();
  unlink_event(event_list,event);
  if (mainw->multitrack!=NULL) mt_fixup_events(mainw->multitrack,event,NULL);
  weed_plant_free(event);
  threaded_dialog_spin();
}


gboolean insert_event_before(weed_plant_t *at_event,weed_plant_t *event) {
  // insert event before at_event : returns FALSE if event is new start of event list
  weed_plant_t *xevent=get_prev_event(at_event);
  if (xevent!=NULL) weed_set_voidptr_value(xevent,"next",event);
  weed_set_voidptr_value(event,"next",at_event);
  weed_set_voidptr_value(event,"previous",xevent);
  weed_set_voidptr_value(at_event,"previous",event);
  if (get_event_timecode(event)>get_event_timecode(at_event)) 
    g_printerr("Warning ! Inserted out of order event type %d before %d\n",get_event_hint(event),get_event_hint(at_event));
  return (xevent!=NULL);
}


gboolean insert_event_after(weed_plant_t *at_event,weed_plant_t *event) {
  // insert event after at_event : returns FALSE if event is new end of event list
  weed_plant_t *xevent=get_next_event(at_event);
  if (xevent!=NULL) weed_set_voidptr_value(xevent,"previous",event);
  weed_set_voidptr_value(event,"previous",at_event);
  weed_set_voidptr_value(event,"next",xevent);
  weed_set_voidptr_value(at_event,"next",event);
  if (get_event_timecode(event)<get_event_timecode(at_event)) 
    g_printerr("Warning ! Inserted out of order event type %d after %d\n",get_event_hint(event),get_event_hint(at_event));
  return (xevent!=NULL);
}


void replace_event(weed_plant_t *event_list, weed_plant_t *at_event,weed_plant_t *event) {
  // replace at_event with event; free at_event
  if (mainw->multitrack!=NULL) mt_fixup_events(mainw->multitrack,at_event,event);
  weed_set_int64_value(event,"timecode",get_event_timecode(at_event));
  if (!insert_event_after(at_event,event)) weed_set_voidptr_value(event_list,"last",event);
  delete_event(event_list,at_event);
}

weed_plant_t *get_next_frame_event (weed_plant_t *event) {
  weed_plant_t *next;
  if (event==NULL) return NULL;
  next=get_next_event(event);
  while (next!=NULL) {
    if (WEED_EVENT_IS_FRAME(next)) return next;
    next=get_next_event(next);
  }
  return NULL;
}


weed_plant_t *get_prev_frame_event (weed_plant_t *event) {
  weed_plant_t *prev;
  if (event==NULL) return NULL;
  prev=get_prev_event(event);
  while (prev!=NULL) {
    if (WEED_EVENT_IS_FRAME(prev)) return prev;
    prev=get_prev_event(prev);
  }
  return NULL;
}


weed_plant_t *get_first_frame_event (weed_plant_t *event_list) {
  weed_plant_t *event;

  if (event_list==NULL) return NULL;

  event=get_first_event(event_list);

  while (event!=NULL) {
    if (WEED_EVENT_IS_FRAME(event)) return event;
    event=get_next_event(event);
  }
  return NULL;
}

weed_plant_t *get_last_frame_event (weed_plant_t *event_list) {
  weed_plant_t *event;

  if (event_list==NULL) return NULL;

  event=get_last_event(event_list);

  while (event!=NULL) {
    if (WEED_EVENT_IS_FRAME(event)) return event;
    event=get_prev_event(event);
  }
  return NULL;
}


void *find_init_event_by_id(void *init_event, weed_plant_t *event) {
  void *event_id;
  int error;

  while (event!=NULL) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      event_id=weed_get_voidptr_value (event,"event_id",&error);
      if (event_id==init_event) break;
    }
    event=get_prev_event(event);
  }
  return (void *)event;
}


void remove_frame_from_event (weed_plant_t *event_list, weed_plant_t *event, gint track) {
  // TODO - memcheck
  int *clips;
  int *frames;
  weed_timecode_t tc;
  int numframes;
  int i,error;

  if (!WEED_EVENT_IS_FRAME(event)) return;

  tc=get_event_timecode(event);

  numframes=weed_leaf_num_elements(event,"clips");
  clips=weed_get_int_array(event,"clips",&error);
  frames=weed_get_int_array(event,"frames",&error);

  if (track==numframes-1) numframes--;
  else {
    clips[track]=-1;
    frames[track]=0;
  }

  // if stack is empty, we will replace with a blank frame
  for (i=0;i<numframes&&clips[i]<1;i++);
  if (i==numframes) {
    if (event==get_last_event(event_list)&&!weed_plant_has_leaf(event,"audio_clips")) delete_event(event_list,event);
    else event_list=insert_blank_frame_event_at (event_list,tc,&event);
  }
  else event_list=insert_frame_event_at (event_list,tc,numframes,clips,frames,&event);
  weed_free(frames);
  weed_free(clips);
}



gboolean is_blank_frame (weed_plant_t *event, gboolean count_audio) {
  int clip,frame,numframes,error;
  if (!WEED_EVENT_IS_FRAME(event)) return FALSE;
  if (count_audio&&weed_plant_has_leaf(event,"audio_clips")) {
    int *aclips=weed_get_int_array(event,"audio_clips",&error);
    if (aclips[1]>0) {
      weed_free(aclips);
      return FALSE;   // has audio seek
    }
    weed_free(aclips);
  }
  numframes=weed_leaf_num_elements(event,"clips");
  if (numframes>1) return FALSE;
  clip=weed_get_int_value(event,"clips",&error);
  frame=weed_get_int_value(event,"frames",&error);

  if (clip<0||frame<=0) return TRUE;
  return FALSE;
}


void remove_end_blank_frames (weed_plant_t *event_list) {
  // remove blank frames from end of event list
  weed_plant_t *event=get_last_event(event_list),*prevevent;
  while (event!=NULL) {
    prevevent=get_prev_event(event);
    if (!WEED_EVENT_IS_FRAME(event)&&!WEED_EVENT_IS_FILTER_INIT(event)) {
      event=prevevent;
      continue;
    }
    if (WEED_EVENT_IS_FILTER_INIT(event)) remove_filter_from_event_list(event_list,event);
    else {
      if (!is_blank_frame(event,TRUE)) break;
      delete_event(event_list,event);
    }
    event=prevevent;
  }
}



weed_timecode_t get_next_paramchange(void **pchange_next,weed_timecode_t end_tc) {
  weed_timecode_t min_tc=end_tc;
  int i=0;
  if (pchange_next==NULL) return end_tc;
  for (;pchange_next[i]!=NULL;i++) if (get_event_timecode((weed_plant_t *)pchange_next[i])<min_tc) 
				     min_tc=get_event_timecode((weed_plant_t *)pchange_next[i]);
  return min_tc;
}


weed_timecode_t get_prev_paramchange(void **pchange_prev,weed_timecode_t start_tc) {
  weed_timecode_t min_tc=start_tc;
  int i=0;
  if (pchange_prev==NULL) return start_tc;
  for (;pchange_prev[i]!=NULL;i++) if (get_event_timecode((weed_plant_t *)pchange_prev[i])<min_tc) 
				     min_tc=get_event_timecode((weed_plant_t *)pchange_prev[i]);
  return min_tc;
}


gboolean is_init_pchange(weed_plant_t *init_event, weed_plant_t *pchange_event) {
  // a PARAM_CHANGE is an init_pchange iff both events have the same tc, and there is no frame event between the two events
  weed_plant_t *event=init_event;
  weed_timecode_t tc=get_event_timecode(event);
  if (tc!=get_event_timecode(pchange_event)) return FALSE;

  while (event!=NULL&&event!=pchange_event) {
    if (WEED_EVENT_IS_FRAME(event)) return FALSE;
    event=get_next_event(event);
  }
  return TRUE;

}




weed_plant_t *event_copy_and_insert (weed_plant_t *in_event, weed_plant_t *event_list) {
  // this is called during quantisation

  // copy an event and insert it in event_list
  // events must be copied in time order, since filter_deinit, 
  // filter_map and param_change events MUST refer to prior filter_init events

  // when we copy a filter_init, we add a new field to the copy "event_id". 
  // This contains the value of the pointer to original event
  // we use this to locate the effect_init event in the new event_list

  // for effect_deinit, effect_map, param_change, we change the "init_event(s)" property to point to our copy effect_init

  // we don't need to make pchain array here, provided we later call event_list_rectify()

  // we check for memory allocation errors here, because we could be building a large new event_list
  // on mem error we return NULL, caller should free() the event_list in that case

  weed_plant_t *event;
  int hint;
  int num_events,i;
  void *init_event,*new_init_event,**init_events;
  int error;
  weed_plant_t *event_after=NULL;
  weed_plant_t *event_before=NULL;
  gchar *filter_hash;
  int idx,num_params;
  weed_plant_t *filter;
  void **in_pchanges;

  if (in_event==NULL) return event_list;

  if (event_list==NULL) {
    event_list=weed_plant_new(WEED_PLANT_EVENT_LIST);
    if (event_list==NULL) return NULL;
    weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
    error=weed_set_int_value(event_list,"weed_event_list_api",WEED_EVENT_API_VERSION);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error=weed_set_voidptr_value(event_list,"first",NULL);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error=weed_set_voidptr_value(event_list,"last",NULL);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
    event_before=NULL;
  }
  else {
    weed_timecode_t in_tc=get_event_timecode(in_event);
    event_before=get_last_event(event_list);
    while (event_before!=NULL) {
      if (get_event_timecode(event_before)<in_tc||(get_event_timecode(event_before)==in_tc
						   &&(!WEED_EVENT_IS_FRAME(event_before)||
						      WEED_EVENT_IS_FILTER_DEINIT(in_event)))) break;
      event_before=get_prev_event(event_before);
    }
  }

  event=weed_plant_copy(in_event);
  if (event==NULL) return NULL;

  if (event_before==NULL) {
    event_after=get_first_event(event_list);
    error=weed_set_voidptr_value(event_list,"first",event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }
  else {
    event_after=get_next_event(event_before);
    error=weed_set_voidptr_value(event_before,"next",event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }

  error=weed_set_voidptr_value(event,"previous",event_before);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  error=weed_set_voidptr_value(event,"next",event_after);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  if (event_after==NULL) error=weed_set_voidptr_value(event_list,"last",event);
  else error=weed_set_voidptr_value(event_after,"previous",event);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  hint=get_event_hint(in_event);
  switch (hint) {
  case WEED_EVENT_HINT_FILTER_INIT:
    error=weed_set_voidptr_value (event,"event_id",(void *)in_event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    filter_hash=weed_get_string_value(event,"filter",&error);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    if ((idx=weed_get_idx_for_hashname(filter_hash,TRUE))!=-1) {
      filter=get_weed_filter(idx);
      if ((num_params=num_in_params(filter,TRUE,TRUE))>0) {
	in_pchanges=(void **)g_try_malloc(num_params*sizeof(void *));
	if (in_pchanges==NULL) return NULL;
	for (i=0;i<num_params;i++) in_pchanges[i]=NULL;
	error=weed_set_voidptr_array(event,"in_parameters",num_params,in_pchanges); // set all to NULL, we will re-fill as we go along
	g_free(in_pchanges);
	if (error==WEED_ERROR_MEMORY_ALLOCATION) {
	  weed_free(filter_hash);
	  return NULL;
	}
      }
      weed_free(filter_hash);
    }
    break;
  case WEED_EVENT_HINT_FILTER_DEINIT:
    init_event=weed_get_voidptr_value (in_event,"init_event",&error);
    new_init_event=find_init_event_by_id(init_event,event);
    error=weed_set_voidptr_value (event,"init_event",new_init_event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error=weed_set_voidptr_value ((weed_plant_t *)new_init_event,"event_id",(void *)new_init_event); // useful later for event_list_rectify
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    break;
  case WEED_EVENT_HINT_FILTER_MAP:
    // set "init_events" property
    num_events=weed_leaf_num_elements (in_event,"init_events");
    init_events=weed_get_voidptr_array (in_event,"init_events",&error);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    for (i=0;i<num_events;i++) {
      init_events[i]=find_init_event_by_id(init_events[i],event);
    }
    error=weed_set_voidptr_array (event,"init_events",num_events,init_events);
    weed_free(init_events);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    break;
  case WEED_EVENT_HINT_PARAM_CHANGE:
    init_event=weed_get_voidptr_value (in_event,"init_event",&error);
    new_init_event=find_init_event_by_id(init_event,get_last_event(event_list));
    error=weed_set_voidptr_value (event,"init_event",new_init_event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    break;
  }
  
  return event_list;
}



gboolean frame_event_has_frame_for_track (weed_plant_t *event, gint track) {
  int *clips,*frames,numclips;
  int error;

  if (!weed_plant_has_leaf(event,"clips")) return FALSE;
  numclips=weed_leaf_num_elements(event,"clips");
  if (numclips<=track) return FALSE;
  clips=weed_get_int_array(event,"clips",&error);
  frames=weed_get_int_array(event,"frames",&error);

  if (clips[track]>0&&frames[track]>0) {
    weed_free(clips);
    weed_free(frames);
    return TRUE;
  }
  weed_free(clips);
  weed_free(frames);
  return FALSE;

}



weed_plant_t *get_frame_event_at (weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t *shortcut, gboolean exact) {
  // if exact is FALSE, we can get a frame event just after tc
  weed_plant_t *event,*next_event;
  weed_timecode_t xtc,next_tc=0;

  if (event_list==NULL) return NULL;
  if (shortcut!=NULL) event=shortcut;
  else event=get_first_frame_event(event_list);
  while (event!=NULL) {
    next_event=get_next_event(event);
    if (next_event!=NULL) next_tc=get_event_timecode(next_event);
    if (((tc==(xtc=get_event_timecode(event)))||((next_tc>tc||next_event==NULL)&&!exact))&&WEED_EVENT_IS_FRAME(event)) return event;
    if (xtc>tc) return NULL;
    event=next_event;
  }
  return NULL;
}



gboolean filter_map_after_frame(weed_plant_t *fmap) {
  // return TRUE if filter_map follows frame at same timecode
  weed_plant_t *frame=get_prev_frame_event(fmap);

  if (frame!=NULL&&get_event_timecode(frame)==get_event_timecode(fmap)) return TRUE;
  return FALSE;
}






weed_plant_t *get_frame_event_at_or_before (weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t *shortcut) {
  weed_plant_t *frame_event=get_frame_event_at(event_list,tc,shortcut,FALSE);
  if (get_event_timecode(frame_event)>tc) {
    frame_event=get_prev_frame_event(frame_event);
  }
  return frame_event;
}



weed_plant_t *get_filter_map_after(weed_plant_t *event, gint ctrack) {
  // get filter_map following event; if ctrack!=-1000000 then we ignore filter maps with no in_track/out_track == ctrack
  void **init_events;
  int error,num_init_events,i;
  weed_plant_t *init_event;

  while (event!=NULL) {
    if (get_event_hint(event)==WEED_EVENT_HINT_FILTER_MAP) {
      if (ctrack==-1000000) return event;
      if (!weed_plant_has_leaf(event,"init_events")) {
	event=get_next_event(event);
	continue;
      }
      init_events=weed_get_voidptr_array(event,"init_events",&error);
      if (init_events[0]==NULL) {
	weed_free(init_events);
	event=get_next_event(event);
	continue;
      }
      num_init_events=weed_leaf_num_elements(event,"init_events");
      for (i=0;i<num_init_events;i++) {
	init_event=(weed_plant_t *)init_events[i];

	if (init_event_is_relevant(init_event,ctrack)) {
	  weed_free(init_events);
	  return event;
	}

      }
      weed_free(init_events);
    }
    event=get_next_event(event);
  }
  return NULL;
}





gboolean init_event_is_relevant(weed_plant_t *init_event, gint ctrack) {
  // see if init_event mentions ctrack as an in_track or an out_track
  // ignore any process_last filters

  register int j;
  int *in_tracks,*out_tracks,error;
  int num_tracks;

  if (init_event_is_process_last(init_event)) return FALSE;
  if (weed_plant_has_leaf(init_event,"in_tracks")) {
    in_tracks=weed_get_int_array(init_event,"in_tracks",&error);
    num_tracks=weed_leaf_num_elements(init_event,"in_tracks");
    for (j=0;j<num_tracks;j++) {
      if (in_tracks[j]==ctrack) {
	weed_free(in_tracks);
	return TRUE;
      }
    }
    weed_free(in_tracks);
  }

  if (weed_plant_has_leaf(init_event,"out_tracks")) {
    out_tracks=weed_get_int_array(init_event,"out_tracks",&error);
    num_tracks=weed_leaf_num_elements(init_event,"out_tracks");
    for (j=0;j<num_tracks;j++) {
      if (out_tracks[j]==ctrack) {
	weed_free(out_tracks);
	return TRUE;
      }
    }
    weed_free(out_tracks);
  }

  return FALSE;
}






weed_plant_t *get_filter_map_before(weed_plant_t *event, gint ctrack, weed_plant_t *stop_event) {
  // get filter_map preceding event; if ctrack!=-1000000 then we ignore 
  // filter maps with no in_track/out_track == ctrack


  // we will stop searching when we reach stop_event; if it is NULL we will search back to
  // start of event list

  void **init_events;
  int error,num_init_events,i;
  weed_plant_t *init_event;

  while (event!=stop_event&&event!=NULL) {
    if (get_event_hint(event)==WEED_EVENT_HINT_FILTER_MAP) {
      if (ctrack==-1000000) return event;
      if (!weed_plant_has_leaf(event,"init_events")) {
	event=get_prev_event(event);
	continue;
      }
      init_events=weed_get_voidptr_array(event,"init_events",&error);
      if (init_events[0]==NULL) {
	weed_free(init_events);
	event=get_prev_event(event);
	continue;
      }
      num_init_events=weed_leaf_num_elements(event,"init_events");
      for (i=0;i<num_init_events;i++) {
	init_event=(weed_plant_t *)init_events[i];
	if (init_event_is_relevant(init_event,ctrack)) {
	  weed_free(init_events);
	  return event;
	}
      }
      weed_free(init_events);
    }
    event=get_prev_event(event);
  }
  return event;
}


void **get_init_events_before(weed_plant_t *event, weed_plant_t *init_event, gboolean add) {
  // find previous FILTER_MAP event, and append or delete new init_event
  void **init_events=NULL,**new_init_events;
  int error,num_init_events=0,i,j=0;

  while (event!=NULL) {
    if (get_event_hint(event)==WEED_EVENT_HINT_FILTER_MAP) {
      if (weed_plant_has_leaf(event,"init_events")&&
	  (init_events=weed_get_voidptr_array(event,"init_events",&error))!=NULL) {
	num_init_events=weed_leaf_num_elements(event,"init_events");
	if (add) new_init_events=(void **)g_malloc((num_init_events+2)*sizeof(void *));
	else new_init_events=(void **)g_malloc((num_init_events+1)*sizeof(void *));

	for (i=0;i<num_init_events;i++) if ((add||(init_event!=NULL&&(init_events[i]!=(void *)init_event)))&&
					    init_events[0]!=NULL) {
	  new_init_events[j++]=init_events[i];
	  if (add&&init_events[i]==(void *)init_event) add=FALSE; // don't add twice
	}

	if (add) {
	  gchar *fhash;
	  weed_plant_t *filter;
	  int k,l,tflags;
	  // add before any "process_last" events
	  k=j;
	  while (k>0) {
	    k--;
	    if (mainw->multitrack!=NULL&&init_events[k]==mainw->multitrack->avol_init_event) {
	      // add before the audio mixer
	      continue;
	    }
	    fhash=weed_get_string_value((weed_plant_t *)init_events[k],"filter",&error);
	    filter=get_weed_filter(weed_get_idx_for_hashname(fhash,TRUE));
	    weed_free(fhash);
	    if (weed_plant_has_leaf(filter,"flags")) {
	      tflags=weed_get_int_value(filter,"flags",&error);
	      if (tflags&WEED_FILTER_PROCESS_LAST) {
		// add before any "process_last" filters
		continue;
	      }
	    }
	    k++;
	    break;
	  }
	  // insert new event at slot k
	  // make gap for new filter
	  for (l=j-1;l>=k;l--) {
	    new_init_events[l+1]=new_init_events[l];
	  }
	  new_init_events[k]=(void *)init_event;
	  j++;
	}

	new_init_events[j]=NULL;
	if (init_events!=NULL) weed_free(init_events);
	return new_init_events;
      }
      if (init_events!=NULL) weed_free(init_events);
    }
    event=get_prev_event(event);
  }
  // no previous init_events found
  if (add) {
    new_init_events=(void **)g_malloc(2*sizeof(void *));
    new_init_events[0]=(void *)init_event;
    new_init_events[1]=NULL;
  }
  else {
    new_init_events=(void **)g_malloc(sizeof(void *));
    new_init_events[0]=NULL;
  }
  return new_init_events;
}


void update_filter_maps (weed_plant_t *event, weed_plant_t *end_event, weed_plant_t *init_event) {
  // append init_event to all FILTER_MAPS between event and end_event

  while (event!=end_event) {
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      add_init_event_to_filter_map(event, init_event, NULL);
    }
    event=get_next_event(event);
  }
}



void insert_filter_init_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event) {
  // insert event as first event at same timecode as (FRAME_EVENT) at_event
  weed_timecode_t tc=get_event_timecode(at_event);
  weed_set_int64_value(event,"timecode",tc);

  while (at_event!=NULL) {
    at_event=get_prev_event(at_event);
    if (at_event==NULL) break;
    if (get_event_timecode(at_event)<tc) {
      if (!insert_event_after(at_event,event)) weed_set_voidptr_value(event_list,"last",event);
      
      return;
    }
  }

  // event is first
  at_event=get_first_event(event_list);
  insert_event_before(at_event,event);
  weed_set_voidptr_value(event_list,"first",event);

}



void insert_filter_deinit_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event) {
  // insert event as last at same timecode as (FRAME_EVENT) at_event
  weed_timecode_t tc=get_event_timecode(at_event);
  weed_set_int64_value(event,"timecode",tc);

  while (at_event!=NULL) {
    if (WEED_EVENT_IS_FRAME(at_event)) {
      if (!insert_event_after(at_event,event)) weed_set_voidptr_value(event_list,"last",event);
      return;
    }
    if (get_event_timecode(at_event)>tc) {
      if (!insert_event_before(at_event,event)) weed_set_voidptr_value(event_list,"first",event);
      return;
    }
    at_event=get_next_event(at_event);
  }
  // event is last
  at_event=get_last_event(event_list);
  insert_event_after(at_event,event);
  weed_set_voidptr_value(event_list,"last",event);
}



gboolean insert_filter_map_event_at(weed_plant_t *event_list, weed_plant_t *at_event, 
				    weed_plant_t *event, gboolean before_frames) {
  // insert event as last event at same timecode as (FRAME_EVENT) at_event
  weed_timecode_t tc=get_event_timecode(at_event);
  weed_set_int64_value(event,"timecode",tc);

  if (before_frames) {
    while (at_event!=NULL) {
      at_event=get_prev_event(at_event);
      if (at_event==NULL) break;
      if (WEED_EVENT_IS_FILTER_MAP(at_event)) {
	// found an existing FILTER_MAP, we can simply replace it
	if (mainw->filter_map==at_event) mainw->filter_map=event;
	replace_event(event_list,at_event,event);
	return TRUE;
      }
      if (WEED_EVENT_IS_FILTER_INIT(at_event)||get_event_timecode(at_event)<tc) {
      if (!insert_event_after(at_event,event)) weed_set_voidptr_value(event_list,"last",event);
      return TRUE;
      }
    }
    // event is first
    at_event=get_first_event(event_list);
    insert_event_before(at_event,event);
    weed_set_voidptr_value(event_list,"first",event);
  }
  else {
    // insert after frame events
    while (at_event!=NULL) {
      at_event=get_next_event(at_event);
      if (at_event==NULL) break;
      if (WEED_EVENT_IS_FILTER_MAP(at_event)) {
	// found an existing FILTER_MAP, we can simply replace it
	if (mainw->filter_map==at_event) mainw->filter_map=event;
	replace_event(event_list,at_event,event);
	return TRUE;
      }
      if (get_event_timecode(at_event)>tc) {
	if (!insert_event_before(at_event,event)) weed_set_voidptr_value(event_list,"first",event);
	return TRUE;
      }
    }
    // event is last
    at_event=get_last_event(event_list);
    insert_event_after(at_event,event);
    weed_set_voidptr_value(event_list,"last",event);
  }
  return TRUE;
}




void insert_param_change_event_at(weed_plant_t *event_list, weed_plant_t *at_event, weed_plant_t *event) {
  // insert event as last at same timecode as (FRAME_EVENT) at_event, before FRAME event
  weed_timecode_t tc=get_event_timecode(at_event);
  weed_set_int64_value(event,"timecode",tc);

  weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN); // protect it for interpolation

  while (at_event!=NULL) {
    if (get_event_timecode(at_event)<tc) {
      if (!insert_event_after(at_event,event)) weed_set_voidptr_value(event_list,"last",event);
      return;
    }
    if (WEED_EVENT_IS_FILTER_INIT(at_event)) {
      if (!insert_event_after(at_event,event)) weed_set_voidptr_value(event_list,"last",event);
      return;
    }
    if (WEED_EVENT_IS_FRAME(at_event)) {
      if (!insert_event_before(at_event,event)) weed_set_voidptr_value(event_list,"first",event);
      return;
    }
    at_event=get_prev_event(at_event);
  }
  at_event=get_first_event(event_list);
  insert_event_before(at_event,event);
  weed_set_voidptr_value(event_list,"first",event);
}



 

weed_plant_t *insert_frame_event_at (weed_plant_t *event_list, weed_timecode_t tc, int numframes, int *clips, 
				     int *frames, weed_plant_t **shortcut) {
  // we will insert a FRAME event at timecode tc, after any other events (except for deinit events) at timecode tc
  // if there is an existing FRAME event at tc, we replace it with the new frame

  // shortcut can be a nearest guess of where the frame should be

  // returns NULL on memory error

  weed_plant_t *event=NULL,*new_event,*prev;
  weed_plant_t *new_event_list,*xevent_list;
  weed_timecode_t xtc;

  int error;

  if (event_list==NULL||get_first_frame_event(event_list)==NULL) {
    // no existing event list, or no frames (uh oh !) append
    event_list=append_frame_event(event_list,tc,numframes,clips,frames);
    if (event_list==NULL) return NULL; // memory error
    if (shortcut!=NULL) *shortcut=get_last_event(event_list);
    return event_list;
  }

  // skip the next part if we know we have to add at end
  if (tc<=get_event_timecode(get_last_event(event_list))) {
    if (shortcut!=NULL&&*shortcut!=NULL) {
      event=*shortcut;
    }
    else event=get_first_event(event_list);
    
    if (get_event_timecode(event)>tc) {
      // step backwards until we get to a frame before where we want to add
      while (event!=NULL&&get_event_timecode(event)>tc) event=get_prev_frame_event(event);
      // event can come out NULL (add before first frame event), in which case we fall through
    }
    else {
      while (event!=NULL&&get_event_timecode(event)<tc) event=get_next_frame_event(event);

      // we reached the end, so we will add after last frame event
      if (event==NULL) event=get_last_frame_event(event_list);
    }
    
    while (event!=NULL&&(((xtc=get_event_timecode(event))<tc)||(xtc==tc&&(!WEED_EVENT_IS_FILTER_DEINIT(event))))) {
      if (shortcut!=NULL) *shortcut=event;
      if (xtc==tc&&WEED_EVENT_IS_FRAME(event)) {
	error=weed_set_int_array (event,"clips",numframes,clips);
	if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
	error=weed_set_int_array (event,"frames",numframes,frames);
	if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
	return event_list;
      }
      event=get_next_event(event);
    }
  }
  else {
    // event is after last event, append it
    if ((xevent_list=append_frame_event (event_list,tc,numframes,clips,frames))==NULL) return NULL;
    event_list=xevent_list;
    if (shortcut!=NULL) *shortcut=get_last_event(event_list);
    return event_list;
  }


  // add frame before first frame event
  
  if ((new_event_list=append_frame_event(NULL,tc,numframes,clips,frames))==NULL) return NULL;
  // new_event_list is now an event_list with one frame event. We will steal its event and prepend it !

  new_event=get_first_event(new_event_list);

  prev=get_prev_event(event);

  if (prev!=NULL) {
    error=weed_set_voidptr_value(prev,"next",new_event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }
  error=weed_set_voidptr_value(new_event,"previous",prev);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  error=weed_set_voidptr_value(new_event,"next",event);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  error=weed_set_voidptr_value(event,"previous",new_event);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  if (get_first_event(event_list)==event) {
    error=weed_set_voidptr_value(event_list,"first",new_event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }

  weed_plant_free(new_event_list);

  if (shortcut!=NULL) *shortcut=new_event;
  return event_list;

}



void insert_audio_event_at(weed_plant_t *event_list, weed_plant_t *event, gint track, gint clipnum, 
			   double seek, double vel) {
  // insert/update audio event at (existing) frame event
  int error,i;
  int *new_aclips;
  double *new_aseeks;
  double arv; // vel needs rounding to four dp (i don't know why, but otherwise we get some weird rounding errors)

  if (vel>=0.) arv=(double)(gint)(vel*10000.+.5)/10000.;
  else arv=(double)(gint)(vel*10000.-.5)/10000.;

  if (weed_plant_has_leaf(event,"audio_clips")) {
    int num_aclips=weed_leaf_num_elements(event,"audio_clips");
    int *aclips=weed_get_int_array(event,"audio_clips",&error);
    double *aseeks=weed_get_double_array(event,"audio_seeks",&error);

    for (i=0;i<num_aclips;i+=2) {
      if (aclips[i]==track) {
	if (clipnum<=0&&num_aclips>2) {
	  // ignore - remove track altogether
	  int *new_aclips=(int *)g_malloc((num_aclips-2)*sizint);
	  double *new_aseeks=(double *)g_malloc((num_aclips-2)*sizdbl);
	  int j,k=0;
	  for (j=0;j<num_aclips;j+=2) {
	    if (j!=i) {
	      new_aclips[k]=aclips[j];
	      new_aclips[k+1]=aclips[j+1];
	      new_aseeks[k]=aseeks[j];
	      new_aseeks[k+1]=aseeks[j+1];
	      k+=2;
	    }
	  }

	  weed_set_int_array(event,"audio_clips",num_aclips-2,new_aclips);
	  weed_set_double_array(event,"audio_seeks",num_aclips-2,new_aseeks);
	  g_free(new_aclips);
	  g_free(new_aseeks);
	  weed_free(aseeks);
	  weed_free(aclips);
	  return;
	}

	// update existing values
	aclips[i+1]=clipnum;
	aseeks[i]=seek;
	aseeks[i+1]=arv;

	weed_set_int_array(event,"audio_clips",num_aclips,aclips);
	weed_set_double_array(event,"audio_seeks",num_aclips,aseeks);
	weed_free(aseeks);
	weed_free(aclips);
	return;
      }
    }

    if (clipnum<=0) return;

    // append
    new_aclips=(int *)g_malloc((num_aclips+2)*sizint);
    for (i=0;i<num_aclips;i++) new_aclips[i]=aclips[i];
    new_aclips[i++]=track;
    new_aclips[i]=clipnum;

    new_aseeks=(double *)g_malloc((num_aclips+2)*sizdbl);
    for (i=0;i<num_aclips;i++) new_aseeks[i]=aseeks[i];
    new_aseeks[i++]=seek;
    new_aseeks[i++]=arv;

    weed_set_int_array(event,"audio_clips",i,new_aclips);
    weed_set_double_array(event,"audio_seeks",i,new_aseeks);

    g_free(new_aclips);
    g_free(new_aseeks);

    weed_free(aseeks);
    weed_free(aclips);
    return;
  }
  // create new values

  new_aclips=(int *)g_malloc(2*sizint);
  new_aclips[0]=track;
  new_aclips[1]=clipnum;

  new_aseeks=(double *)g_malloc(2*sizdbl);
  new_aseeks[0]=seek;
  new_aseeks[1]=arv;

  weed_set_int_array(event,"audio_clips",2,new_aclips);
  weed_set_double_array(event,"audio_seeks",2,new_aseeks);
  
  g_free(new_aclips);
  g_free(new_aseeks);
}



void remove_audio_for_track (weed_plant_t *event, int track) {
  // delete audio for a FRAME_EVENT with audio for specified track
  // if nothing left, delete the audio leaves

  int j=0,i,error;
  int num_atracks=weed_leaf_num_elements (event,"audio_clips");
  int *aclip_index=weed_get_int_array(event,"audio_clips",&error);
  double *aseek_index=weed_get_double_array(event,"audio_seeks",&error);
  int *new_aclip_index=(int *)g_malloc(num_atracks*sizint);
  double *new_aseek_index=(double *)g_malloc(num_atracks*sizdbl);

  for (i=0;i<num_atracks;i+=2) {
    if (aclip_index[i]==track) continue;
    new_aclip_index[j]=aclip_index[i];
    new_aclip_index[j+1]=aclip_index[i+1];
    new_aseek_index[j]=aseek_index[i];
    new_aseek_index[j+1]=aseek_index[i+1];
    j+=2;
  }
  if (j==0) {
    weed_leaf_delete(event,"audio_clips");
    weed_leaf_delete(event,"audio_seeks");
  }
  else {
    weed_set_int_array(event,"audio_clips",j,new_aclip_index);
    weed_set_double_array(event,"audio_seeks",j,new_aseek_index);
  }
  weed_free(aclip_index);
  weed_free(aseek_index);
  g_free(new_aclip_index);
  g_free(new_aseek_index);
}


weed_plant_t *append_marker_event (weed_plant_t *event_list, weed_timecode_t tc, int marker_type) {
  weed_plant_t *event,*prev;

  if (event_list==NULL) {
    event_list=weed_plant_new(WEED_PLANT_EVENT_LIST);
    weed_set_int_value(event_list,"weed_event_list_api",WEED_EVENT_API_VERSION);
    weed_set_voidptr_value(event_list,"first",NULL);
    weed_set_voidptr_value(event_list,"last",NULL);
    weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
  }

  event=weed_plant_new (WEED_PLANT_EVENT);
  weed_set_voidptr_value(event,"next",NULL);

  // TODO - error check
  weed_set_int64_value (event,"timecode",tc);
  weed_set_int_value (event,"hint",WEED_EVENT_HINT_MARKER);

  weed_set_int_value (event,"lives_type",marker_type);

#ifdef DEBUG_EVENTS
  g_print ("adding map event %p at tc %"PRId64"\n",init_events[0],tc);
#endif

  if (get_first_event(event_list)==NULL) {
    weed_set_voidptr_value(event_list,"first",event);
    weed_set_voidptr_value(event,"previous",NULL);
  }
  else {
    weed_set_voidptr_value(event,"previous",get_last_event(event_list));
  }
  weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN);
  prev=get_prev_event(event);
  if (prev!=NULL) weed_set_voidptr_value(prev,"next",event);
  weed_set_voidptr_value(event_list,"last",event);

  return event_list;
}



void insert_marker_event_at(weed_plant_t *event_list, weed_plant_t *at_event, int marker_type, gpointer data) {
  // insert marker event as first event at same timecode as (FRAME_EVENT) at_event
  weed_timecode_t tc=get_event_timecode(at_event);
  weed_plant_t *event=weed_plant_new(WEED_PLANT_EVENT);
  int error,i;

  weed_set_int_value (event,"hint",WEED_EVENT_HINT_MARKER);
  weed_set_int_value (event,"lives_type",marker_type);
  weed_set_int64_value (event,"timecode",tc);
 
  if (marker_type==EVENT_MARKER_BLOCK_START||marker_type==EVENT_MARKER_BLOCK_UNORDERED) {
    weed_set_int_value(event,"tracks",GPOINTER_TO_INT(data));
  }
  weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN);

  while (at_event!=NULL) {
    at_event=get_prev_event(at_event);
    if (at_event==NULL) break;
    switch (marker_type) {
    case EVENT_MARKER_BLOCK_START:
    case EVENT_MARKER_BLOCK_UNORDERED:
      if (WEED_EVENT_IS_MARKER(at_event)&&(weed_get_int_value(at_event,"lives_type",&error)==marker_type)) {
	// add to existing event
	int num_tracks=weed_leaf_num_elements(at_event,"tracks");
	int *tracks=weed_get_int_array(at_event,"tracks",&error);
	int *new_tracks=(int *)g_malloc((num_tracks+1)*sizint);
	for (i=0;i<num_tracks;i++) {
	  new_tracks[i]=tracks[i];
	}
	new_tracks[i]=GPOINTER_TO_INT(data); // add new track
	weed_set_int_array(at_event,"tracks",num_tracks+1,new_tracks);
	g_free(new_tracks);
	weed_free(tracks);
	weed_plant_free(event); // new event not used
	return;
      }
      if (get_event_timecode(at_event)<tc) {
	// create new event
	if (!insert_event_after(at_event,event)) weed_set_voidptr_value(event_list,"last",event);
	return;
      }
      break;
    }
  }

  // event is first
  at_event=get_first_event(event_list);
  insert_event_before(at_event,event);
  weed_set_voidptr_value(event_list,"first",event);

}


weed_plant_t *insert_blank_frame_event_at (weed_plant_t *event_list, weed_timecode_t tc, weed_plant_t **shortcut) {
  int clip=-1;
  int frame=0;

  return insert_frame_event_at(event_list,tc,1,&clip,&frame,shortcut);
}


void remove_filter_from_event_list(weed_plant_t *event_list, weed_plant_t *init_event) {
  int error,i;
  weed_plant_t *deinit_event=(weed_plant_t *)weed_get_voidptr_value(init_event,"deinit_event",&error);
  weed_plant_t *event=init_event;
  weed_plant_t *filter_map=get_filter_map_before(init_event,-1000000,NULL);
  void **new_init_events;
  weed_timecode_t deinit_tc=get_event_timecode(deinit_event);
  weed_plant_t *event_next;

  while (event!=NULL&&get_event_timecode(event)<=deinit_tc) {
    event_next=get_next_event(event);
   // update filter_maps
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      new_init_events=get_init_events_before(event,init_event,FALSE);
      for (i=0;new_init_events[i]!=NULL;i++);
      if (i==0) weed_set_voidptr_value(event,"init_events",NULL);
      else weed_set_voidptr_array(event,"init_events",i,new_init_events);
      g_free(new_init_events);

      if ((filter_map==NULL&&i==0)||(filter_map!=NULL&&compare_filter_maps(filter_map,event,-1000000))) 
	delete_event(event_list,event);
      else filter_map=event;
    }
    event=event_next;
  }

  // remove param_changes
  if (weed_plant_has_leaf(init_event,"in_parameters")) {
    void *pchain_next;
    void **pchain=weed_get_voidptr_array(init_event,"in_parameters",&error);
    int num_params=weed_leaf_num_elements(init_event,"in_parameters");
    for (i=0;i<num_params;i++) {
      while (pchain[i]!=NULL) {
	pchain_next=weed_get_voidptr_value((weed_plant_t *)pchain[i],"next_change",&error);
	delete_event(event_list,(weed_plant_t *)pchain[i]);
	pchain[i]=pchain_next;
      }
    }
    weed_free(pchain);
  }

  delete_event(event_list,init_event);
  delete_event(event_list,deinit_event);

}



static gboolean remove_event_from_filter_map(weed_plant_t *fmap, weed_plant_t *event) {
  // return FALSE if result is NULL filter_map
  int error;
  void **init_events=weed_get_voidptr_array(fmap,"init_events",&error);
  void **new_init_events;
  int i,j=0;
  int num_inits;

  num_inits=weed_leaf_num_elements(fmap,"init_events");
  new_init_events=(void **)g_malloc(num_inits*sizeof(void *));
  for (i=0;i<num_inits;i++) {
    if (init_events[i]!=event) new_init_events[j++]=init_events[i];
  }
    
  if (j==0||(j==1&&(event==NULL||init_events[0]==NULL))) weed_set_voidptr_value(fmap,"init_events",NULL);
  else weed_set_voidptr_array(fmap,"init_events",j,new_init_events);
  weed_free(init_events);
  g_free(new_init_events);

  return (!(j==0||(j==1&&event==NULL)));
}

LIVES_INLINE gboolean init_event_in_list(void **init_events, int num_inits, weed_plant_t *event) {
  int i;
  if (init_events==NULL||init_events[0]==NULL) return FALSE;
  for (i=0;i<num_inits;i++) {
    if (init_events[i]==(void **)event) return TRUE;
  }
  return FALSE;
}


gboolean filter_map_has_event(weed_plant_t *fmap, weed_plant_t *event) {
  int error;
  void **init_events=weed_get_voidptr_array(fmap,"init_events",&error);
  int num_inits=weed_leaf_num_elements(fmap,"init_events");
  gboolean ret=init_event_in_list(init_events,num_inits,event);

  weed_free(init_events);
  return ret;

}


gboolean filter_init_has_owner(weed_plant_t *init_event, int track) {
  int i,error,num_owners;
  int *owners;
  if (!weed_plant_has_leaf(init_event,"in_tracks")) return FALSE;

  owners=weed_get_int_array(init_event,"in_tracks",&error);
  num_owners=weed_leaf_num_elements(init_event,"in_tracks");

  for (i=0;i<num_owners;i++) {
    if (owners[i]==track) {
      weed_free(owners);
      return TRUE;
    }
  }
  weed_free(owners);
  return FALSE;
}



void backup_host_tags(weed_plant_t *event_list, weed_timecode_t curr_tc) {
  // when redrawing the current frame during rendering (in multitrack mode)
  // host keys will change (see backup_weed_instances() in effects-weed.c)
  // here we backup the host_tag (which maps a filter_init to a "key" and thus to an instance)

  weed_plant_t *event=get_first_event(event_list);
  weed_timecode_t tc;

  while (event!=NULL&&(tc=get_event_timecode(event))<=curr_tc) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) weed_leaf_copy(event,"host_tag_copy",event,"host_tag");
    event=get_next_event(event);
  }
}


void restore_host_tags(weed_plant_t *event_list, weed_timecode_t curr_tc) {
  // when redrawing the current frame during rendering (in multitrack mode)
  // host keys will change (see backup_weed_instances() in effects-weed.c)
  // here we restore the host_tag (which maps a filter_init to a "key" and thus to an instance)

  weed_plant_t *event=get_first_event(event_list);
  weed_timecode_t tc;

  while (event!=NULL&&(tc=get_event_timecode(event))<=curr_tc) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      weed_leaf_copy(event,"host_tag",event,"host_tag_copy");
      weed_leaf_delete(event,"host_tag_copy");
    }
    event=get_next_event(event);
  }
}



void delete_param_changes_after_deinit(weed_plant_t *event_list, weed_plant_t *init_event) {
  // delete parameter value changes following the filter_deinit
  // this can be called when a FILTER_DEINIT is moved
  int error,i;
  void **init_events;
  int num_inits;
  weed_plant_t *deinit_event=(weed_plant_t *)weed_get_voidptr_value(init_event,"deinit_event",&error);
  weed_timecode_t deinit_tc=get_event_timecode(deinit_event);

  weed_timecode_t pchain_tc;
  
  void *pchain,*pchain_next;

  if (!weed_plant_has_leaf(init_event,"in_parameters")) return;

  num_inits=weed_leaf_num_elements(init_event,"in_parameters");
  init_events=weed_get_voidptr_array(init_event,"in_parameters",&error);

  for (i=0;i<num_inits;i++) {
    pchain=init_events[i];
    while (pchain!=NULL) {
      pchain_tc=get_event_timecode((weed_plant_t *)pchain);
      if (!weed_plant_has_leaf((weed_plant_t *)pchain,"next_change")) pchain_next=NULL;
      else pchain_next=weed_get_voidptr_value((weed_plant_t *)pchain,"next_change",&error);
      if (pchain_tc>deinit_tc) delete_event(event_list,(weed_plant_t *)pchain);
      pchain=pchain_next;
    }
  }
  weed_free(init_events);
}



static void rescale_param_changes(weed_plant_t *event_list, weed_plant_t *init_event, weed_timecode_t new_init_tc, 
				  weed_plant_t *deinit_event, weed_timecode_t new_deinit_tc, gdouble fps) {
  // rescale parameter value changes along the time axis
  // this can be called when a FILTER_INIT or FILTER_DEINIT is moved

  int error,i;
  void **init_events;
  int num_inits;

  weed_timecode_t old_init_tc=get_event_timecode(init_event);
  weed_timecode_t old_deinit_tc=get_event_timecode(deinit_event);

  weed_timecode_t pchain_tc,new_tc;
  
  void *pchain;
  weed_plant_t *event;

  if (!weed_plant_has_leaf(init_event,"in_parameters")) return;

  num_inits=weed_leaf_num_elements(init_event,"in_parameters");
  init_events=weed_get_voidptr_array(init_event,"in_parameters",&error);

  if (init_events==NULL) num_inits=0;

  for (i=0;i<num_inits;i++) {
    pchain=init_events[i];
    while (pchain!=NULL) {
      pchain_tc=get_event_timecode((weed_plant_t *)pchain);
      new_tc=(weed_timecode_t)((gdouble)(pchain_tc-old_init_tc)/(gdouble)(old_deinit_tc-old_init_tc)*
			       (gdouble)(new_deinit_tc-new_init_tc))+new_init_tc;
      new_tc=q_gint64(new_tc,fps);
      if (new_tc==pchain_tc) {
	if (!weed_plant_has_leaf((weed_plant_t *)pchain,"next_change")) pchain=NULL;
	else pchain=weed_get_voidptr_value((weed_plant_t *)pchain,"next_change",&error);
	continue;
      }
      event=(weed_plant_t *)pchain;
      if (new_tc<pchain_tc) {
	while (event!=NULL&&get_event_timecode(event)>new_tc) event=get_prev_event(event);
      }
      else {
	while (event!=NULL&&get_event_timecode(event)<new_tc) event=get_next_event(event);
      }
      
      if (event!=NULL) {
	unlink_event(event_list,(weed_plant_t *)pchain);
	insert_param_change_event_at(event_list,event,(weed_plant_t *)pchain);
      }
      
      if (!weed_plant_has_leaf((weed_plant_t *)pchain,"next_change")) pchain=NULL;
      else pchain=weed_get_voidptr_value((weed_plant_t *)pchain,"next_change",&error);
    }
  }

  if (init_events!=NULL) weed_free(init_events);
}


static gboolean is_in_hints(weed_plant_t *event,void **hints) {
  int i;
  if (hints==NULL) return FALSE;
  for (i=0;hints[i]!=NULL;i++) {
    if (hints[i]==event) return TRUE;
  }
  return FALSE;
}




gboolean init_event_is_process_last(weed_plant_t *event) {
  int error;
  gboolean res=FALSE;
  gchar *hashname;
  weed_plant_t *filter;

  if (event==NULL) return FALSE;

  hashname=weed_get_string_value(event,"filter",&error);
  filter=get_weed_filter(weed_get_idx_for_hashname(hashname,TRUE));
  if (weed_plant_has_leaf(filter,"flags")) {
      int fflags=weed_get_int_value(filter,"flags",&error);
      if (fflags&WEED_FILTER_PROCESS_LAST) {
	res=TRUE;
      }
  }
  
  weed_free(hashname);
  return res;
}




void add_init_event_to_filter_map(weed_plant_t *fmap, weed_plant_t *event, void **hints) {
  // TODO - try to add at same position as in hints ***

  // init_events are the events we are adding to
  // event is what we are adding

  // hints is the init_events from the previous filter_map


  int i,error,j=0;
  void **init_events;
  int num_inits;
  void **new_init_events;
  gboolean added=FALSE,plast=FALSE,mustadd=FALSE;

  remove_event_from_filter_map(fmap,event);

  init_events=weed_get_voidptr_array(fmap,"init_events",&error);
  num_inits=weed_leaf_num_elements(fmap,"init_events");

  if (num_inits==1&&(init_events==NULL||init_events[0]==NULL)) {
    weed_set_voidptr_value(fmap,"init_events",event);
    weed_free(init_events);
    return;
  }

  if (init_event_is_process_last(event)) plast=TRUE;

  new_init_events=(void **)g_malloc((num_inits+1)*sizeof(void *));

  for (i=0;i<num_inits;i++) {
    if (init_event_is_process_last((weed_plant_t *)init_events[i])) mustadd=TRUE;

    if (mustadd||(!plast&&!added&&is_in_hints((weed_plant_t *)init_events[i],hints))) {
      new_init_events[j++]=event;
      added=TRUE;
    }
    new_init_events[j++]=init_events[i];
    if (init_events[i]==event) added=TRUE;
  }
  if (!added) new_init_events[j++]=event;

  weed_set_voidptr_array(fmap,"init_events",j,new_init_events);
  weed_free(init_events);
  g_free(new_init_events);
}





void move_filter_init_event(weed_plant_t *event_list, weed_timecode_t new_tc, weed_plant_t *init_event, gdouble fps) {
  int error,i,j=0;
  weed_timecode_t tc=get_event_timecode(init_event);
  weed_plant_t *deinit_event=(weed_plant_t *)weed_get_voidptr_value(init_event,"deinit_event",&error);
  weed_timecode_t deinit_tc=get_event_timecode(deinit_event);
  weed_plant_t *event=init_event,*event_next;
  weed_plant_t *filter_map,*copy_filter_map;
  void **init_events;
  int num_inits;
  void **event_hints=NULL;
  gboolean is_null_filter_map;

  rescale_param_changes(event_list,init_event,new_tc,deinit_event,deinit_tc,fps);

  if (new_tc>tc) {
    // moving right
    filter_map=get_filter_map_before(event,-1000000,NULL);
    while (get_event_timecode(event)<new_tc) {
      event_next=get_next_event(event);
      if (WEED_EVENT_IS_FILTER_MAP(event)) {
	is_null_filter_map=!remove_event_from_filter_map(event,init_event);
	if ((filter_map==NULL&&is_null_filter_map)||(filter_map!=NULL&&compare_filter_maps(filter_map,event,-1000000))) 
	  delete_event(event_list,event);
	else filter_map=event;
      }
      event=event_next;
    }
    unlink_event(event_list,init_event);
    insert_filter_init_event_at(event_list, event, init_event);

    event=get_next_frame_event(init_event);

    init_events=get_init_events_before(event,init_event,TRUE);
    event_list=append_filter_map_event(event_list,new_tc,init_events);
    g_free(init_events);

    filter_map=get_last_event(event_list);
    unlink_event(event_list,filter_map);
    insert_filter_map_event_at(event_list,event,filter_map,TRUE);
  }
  else {
    // moving left
    // see if event is switched on at start
    gboolean is_on=FALSE;
    gboolean adding=FALSE;
    while (event!=deinit_event) {
      if (get_event_timecode(event)>tc) break;
      if (WEED_EVENT_IS_FILTER_MAP(event)&&filter_map_has_event(event,init_event)) {
	if (weed_plant_has_leaf(event,"init_events")) {
	  init_events=weed_get_voidptr_array(event,"init_events",&error);
	  if (init_events[0]!=NULL) {
	    num_inits=weed_leaf_num_elements(event,"init_events");
	    event_hints=(void **)g_malloc((num_inits+1)*sizeof(void *));
	    for (i=0;i<num_inits;i++) {
	      if (adding) event_hints[j++]=init_events[i];
	      if (init_events[i]==init_event) adding=TRUE;
	    }
	    event_hints[j]=NULL;
	    is_on=TRUE;
	  }
	  weed_free(init_events);
	}
	break;
      }
      event=get_next_event(event);
    }
    event=init_event;
    while (get_event_timecode(event)>new_tc) event=get_prev_event(event);
    unlink_event(event_list,init_event);
    insert_filter_init_event_at(event_list,event,init_event);

    if (is_on) {
      event=get_next_frame_event(init_event);
      filter_map=get_filter_map_before(event,-1000000,NULL);

      // insert filter_map at new filter_init
      if (filter_map!=NULL) {
	copy_filter_map=weed_plant_copy(filter_map);
	add_init_event_to_filter_map(copy_filter_map,init_event,event_hints);
	filter_map=copy_filter_map;
      }
      else {
	init_events=(void **)g_malloc(2*sizeof(void *));
	init_events[0]=init_event;
	init_events[1]=NULL;
	event_list=append_filter_map_event (event_list, new_tc, init_events);
	g_free(init_events);
	filter_map=get_last_event(event_list);
	unlink_event(event_list,filter_map);
      }
      
      insert_filter_map_event_at(event_list,event,filter_map,TRUE);
      event=get_next_event(filter_map);

      // ensure filter remains on until repositioned FILTER_INIT
      while (event!=NULL&&get_event_timecode(event)<=tc) {
	event_next=get_next_event(event);
	if (WEED_EVENT_IS_FILTER_MAP(event)) {
	  add_init_event_to_filter_map(filter_map,init_event,event_hints);
	  if (compare_filter_maps(filter_map,event,-1000000)) delete_event(event_list,event);
	  else filter_map=event;
	}
	event=event_next;
      }
      if (event_hints!=NULL) g_free(event_hints);
    }
  }

}



void move_filter_deinit_event(weed_plant_t *event_list, weed_timecode_t new_tc, weed_plant_t *deinit_event, 
			      gdouble fps, gboolean rescale_pchanges) {
  // move a filter_deinit from old pos to new pos, remove mention of it from filter maps, 
  // possibly add/update filter map before frame at new_tc, remove duplicate filter_maps, update param_change events
  int error,i,j=0;
  weed_timecode_t tc=get_event_timecode(deinit_event);
  weed_plant_t *init_event=(weed_plant_t *)weed_get_voidptr_value(deinit_event,"init_event",&error);
  weed_timecode_t init_tc=get_event_timecode(init_event);
  weed_plant_t *event=deinit_event,*event_next;
  weed_plant_t *filter_map,*copy_filter_map;
  weed_plant_t *xevent;
  void **init_events;
  int num_inits;
  void **event_hints=NULL;
  gboolean is_null_filter_map;

  if (new_tc==tc) return;

  if (rescale_pchanges) rescale_param_changes(event_list,init_event,init_tc,deinit_event,new_tc,fps);

  if (new_tc<tc) {
    // moving left
    //find last event at new_tc, we are going to insert deinit_event after this


    // first find filter_map before new end position, copy it with filter removed
    while (get_event_timecode(event)>new_tc) event=get_prev_event(event);
    filter_map=get_filter_map_before(event,-1000000,NULL);
    if (filter_map!=NULL) {
      if (get_event_timecode(filter_map)!=get_event_timecode(event)) {
	copy_filter_map=weed_plant_copy(filter_map);
	if (!WEED_EVENT_IS_FRAME(event)) event=get_prev_frame_event(event);
	if (event==NULL) event=get_first_event(event_list);
	insert_filter_map_event_at(event_list,event,copy_filter_map,FALSE);
      }
      else copy_filter_map=filter_map;
      remove_event_from_filter_map(copy_filter_map,init_event);
      if (filter_map!=copy_filter_map&&compare_filter_maps(filter_map,copy_filter_map,-1000000)) 
	delete_event(event_list,copy_filter_map);
      else filter_map=copy_filter_map;
    }

    while (!WEED_EVENT_IS_FRAME(event)) event=get_prev_event(event);
    xevent=event;
    filter_map=get_filter_map_before(event,-1000000,NULL);

    // remove from following filter_maps

    while (event!=NULL&&get_event_timecode(event)<=tc) {
      event_next=get_next_event(event);
      if (WEED_EVENT_IS_FILTER_MAP(event)) {
	// found a filter map, so remove the event from it
	is_null_filter_map=!remove_event_from_filter_map(event,init_event);
	if ((filter_map==NULL&&is_null_filter_map)||(filter_map!=NULL&&compare_filter_maps(filter_map,event,-1000000))) 
	  delete_event(event_list,event);
	else filter_map=event;
      }
      event=event_next;
    }
    unlink_event(event_list,deinit_event);
    insert_filter_deinit_event_at(event_list,xevent,deinit_event);
    if (!rescale_pchanges) delete_param_changes_after_deinit(event_list,init_event);
  }
  else {
    // moving right
    // see if event is switched on at end
    gboolean is_on=FALSE;
    gboolean adding=FALSE;

    xevent=get_prev_event(deinit_event);

    // get event_hints so we can add filter back at guess position
    filter_map=get_filter_map_before(deinit_event,-1000000,NULL);
    if (filter_map!=NULL&&filter_map_has_event(filter_map,init_event)) {
      init_events=weed_get_voidptr_array(filter_map,"init_events",&error);
      num_inits=weed_leaf_num_elements(filter_map,"init_events");
      event_hints=(void **)g_malloc((num_inits+1)*sizeof(void *));
      for (i=0;i<num_inits;i++) {
	if (adding) {
	  event_hints[j++]=init_events[i];
	}
	if (init_events[i]==init_event) adding=TRUE;
      }
      event_hints[j]=NULL;
      is_on=TRUE;
      weed_free(init_events);
    }
    
    // move deinit event
    event=deinit_event;
    while (event!=NULL&&get_event_timecode(event)<new_tc) event=get_next_event(event);

    unlink_event(event_list,deinit_event);

    if (event==NULL) return;

    insert_filter_deinit_event_at(event_list,event,deinit_event);

    if (is_on) {
      // ensure filter remains on until new position
      event=xevent;
      while (event!=deinit_event) {
	if (get_event_timecode(event)==new_tc&&WEED_EVENT_IS_FRAME(event)) break;
	event_next=get_next_event(event);
	if (WEED_EVENT_IS_FILTER_MAP(event)) {
	  add_init_event_to_filter_map(event,init_event,event_hints);
	  if (compare_filter_maps(filter_map,event,-1000000)) delete_event(event_list,event);
	  else filter_map=event;
	}
	event=event_next;
      }
      if (event_hints!=NULL) g_free(event_hints);

      // find last FILTER_MAP before deinit_event
      event=deinit_event;
      while (event!=NULL&&get_event_timecode(event)==new_tc) event=get_next_event(event);
      if (event==NULL) event=get_last_event(event_list);
      filter_map=get_filter_map_before(event,-1000000,NULL);

      if (filter_map!=NULL&&filter_map_has_event(filter_map,init_event)) {
	// if last FILTER_MAP before deinit_event mentions init_event, remove init_event, 
	// insert FILTER_MAP after deinit_event
	copy_filter_map=weed_plant_copy(filter_map);

	remove_event_from_filter_map(copy_filter_map,init_event);
	insert_filter_map_event_at(event_list,deinit_event,copy_filter_map,FALSE);
	event=get_next_event(copy_filter_map);
	while (event!=NULL) {
	  // remove next FILTER_MAP if it is a duplicate
	  if (WEED_EVENT_IS_FILTER_MAP(event)) {
	    if (compare_filter_maps(copy_filter_map,event,-1000000)) delete_event(event_list,event);
	    break;
	  }
	  event=get_next_event(event);
	}
      }
    }
  }
}





gboolean move_event_right(weed_plant_t *event_list, weed_plant_t *event, gboolean can_stay, gdouble fps) {
  // move a filter_init or param_change to the right
  // this can happen for two reasons: - we are rectifying an event_list, or a block was deleted or moved

  int *owners;
  int error;
  int num_owners=0,num_clips;
  weed_timecode_t tc=get_event_timecode(event),new_tc=tc;
  weed_plant_t *xevent=event;
  gboolean all_ok=FALSE;
  int i;
  
  if (WEED_EVENT_IS_FILTER_INIT(event)) {
    if (weed_plant_has_leaf(event,"in_tracks")) num_owners=weed_leaf_num_elements(event,"in_tracks");
  }
  else if (!WEED_EVENT_IS_PARAM_CHANGE(event)) return TRUE;
 
  if (num_owners>0) {
    owners=weed_get_int_array(event,"in_tracks",&error);

    while (xevent!=NULL) {
      if (WEED_EVENT_IS_FRAME(xevent)) {
	if ((new_tc=get_event_timecode(xevent))>tc||(can_stay&&new_tc==tc)) {
	  all_ok=TRUE;
	  num_clips=weed_leaf_num_elements(xevent,"clips");
	  // find timecode of next event which has valid frames at all owner tracks
	  for (i=0;i<num_owners;i++) {
	    if (owners[i]<0) continue; // ignore audio owners
	    if (num_clips<=owners[i]||get_frame_event_clip(xevent,owners[i])<0||get_frame_event_frame(xevent,owners[i])<1) {
	      all_ok=FALSE;
	      break; // blank frame, or not enough frames
	    }
	  }
	  if (all_ok) break;
	}
      }
      xevent=get_next_event(xevent);
    }
    weed_free(owners);
  }
  else {
    if (can_stay) return TRUE; // bound to timeline, and allowed to stay
    xevent=get_next_frame_event(event); // bound to timeline, move to next frame event
    new_tc=get_event_timecode(xevent);
  }

  if (can_stay&&(new_tc==tc)&&all_ok) return TRUE;

  // now we have xevent, new_tc

  if (WEED_EVENT_IS_FILTER_INIT(event)) {
    weed_plant_t *deinit_event=(weed_plant_t *)weed_get_voidptr_value(event,"deinit_event",&error);
    if (xevent==NULL||get_event_timecode(deinit_event)<new_tc) {
      // if we are moving a filter_init past its deinit event, remove it, remove deinit, remove param_change events, remove from all filter_maps, and check for duplicate filter maps
      remove_filter_from_event_list(event_list,event);
      return FALSE;
    }
    move_filter_init_event(event_list,new_tc,event,fps);
  }
  else {
    // otherwise, for a param_change, just insert it at new_tc
    weed_plant_t *init_event=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);
    weed_plant_t *deinit_event=(weed_plant_t *)weed_get_voidptr_value(init_event,"deinit_event",&error);
    if (xevent==NULL||get_event_timecode(deinit_event)<new_tc) {
      delete_event(event_list,event);
      return FALSE;
    }
    unlink_event(event_list,event);
    insert_param_change_event_at(event_list,xevent,event);
  }
  return FALSE;
}



gboolean move_event_left(weed_plant_t *event_list, weed_plant_t *event, gboolean can_stay, gdouble fps) {
  // move a filter_deinit to the left
  // this can happen for two reasons: - we are rectifying an event_list, or a block was deleted or moved

  int *owners;
  int error;
  int num_owners=0,num_clips;
  weed_timecode_t tc=get_event_timecode(event),new_tc=tc;
  weed_plant_t *xevent=event;
  weed_plant_t *init_event;
  gboolean all_ok=FALSE;
  int i;

  if (WEED_EVENT_IS_FILTER_DEINIT(event)) init_event=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);
  else return TRUE;

  if (weed_plant_has_leaf(init_event,"in_tracks")) num_owners=weed_leaf_num_elements(init_event,"in_tracks");

  if (num_owners>0) {
    owners=weed_get_int_array(init_event,"in_tracks",&error);
    while (xevent!=NULL) {
      if (WEED_EVENT_IS_FRAME(xevent)) {
	if ((new_tc=get_event_timecode(xevent))<tc||(can_stay&&new_tc==tc)) {
	  all_ok=TRUE;
	  // find timecode of previous event which has valid frames at all owner tracks
	  for (i=0;i<num_owners;i++) {
	    if (owners[i]<0) continue; // ignore audio owners
	    num_clips=weed_leaf_num_elements(xevent,"clips");
	    if (num_clips<=owners[i]||get_frame_event_clip(xevent,owners[i])<0||get_frame_event_frame(xevent,owners[i])<1) {
	      all_ok=FALSE;
	      break; // blank frame
	    }
	  }
	  if (all_ok) break;
	}
      }
      xevent=get_prev_event(xevent);
    }
    weed_free(owners);
  }
  else {
    if (can_stay) return TRUE; // bound to timeline, and allowed to stay
    while (xevent!=NULL) {
      // bound to timeline, just move to previous tc
      if ((new_tc=get_event_timecode(xevent))<tc) break;
      xevent=get_prev_event(xevent);
    }
  }
  // now we have new_tc

  if (can_stay&&(new_tc==tc)&&all_ok) return TRUE;

  if (get_event_timecode(init_event)>new_tc) {
    // if we are moving a filter_deinit past its init event, remove it, remove init, remove param_change events, remove from all filter_maps, and check for duplicate filter maps
    remove_filter_from_event_list(event_list,init_event);
    return FALSE;
  }

  // otherwise, go from old pos to new pos, remove mention of it from filter maps, possibly add/update filter map as last event at new_tc, remove duplicate filter_maps, update param_change events

  move_filter_deinit_event(event_list,new_tc,event,fps,TRUE);

  return FALSE;
}


//////////////////////////////////////////////////////
// rendering

void
set_render_choice (GtkToggleButton *togglebutton, gpointer choice) {
  if (gtk_toggle_button_get_active(togglebutton)) render_choice=GPOINTER_TO_INT (choice);
}

void
set_render_choice_button (GtkButton *button, gpointer choice) {
  render_choice=GPOINTER_TO_INT (choice);
}


gint get_render_choice (void) {
  return render_choice;
}




GtkWidget *events_rec_dialog (gboolean allow_mt) {
  GtkWidget *e_rec_dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *eventbox;
  GtkWidget *label;
  GtkWidget *radiobutton;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;
  GSList *radiobutton_group = NULL;
  GtkAccelGroup *accel_group;

  render_choice=RENDER_CHOICE_PREVIEW;

  e_rec_dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (e_rec_dialog), _("LiVES: - Events recorded"));
  gtk_window_set_position (GTK_WINDOW (e_rec_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (e_rec_dialog), TRUE);
  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(e_rec_dialog),FALSE);
    gtk_widget_modify_bg (e_rec_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (prefs->show_gui) gtk_window_set_transient_for(GTK_WINDOW(e_rec_dialog),GTK_WINDOW(mainw->LiVES));
  gtk_window_set_modal (GTK_WINDOW (e_rec_dialog), TRUE);

  dialog_vbox = GTK_DIALOG (e_rec_dialog)->vbox;
  gtk_widget_show (dialog_vbox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);

  label = gtk_label_new (_("   Events were recorded. What would you like to do with them ?    "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 20);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 20);

  radiobutton = gtk_radio_button_new (NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("_Preview events"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton),TRUE);

  GTK_WIDGET_SET_FLAGS (radiobutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  g_signal_connect (GTK_OBJECT (radiobutton), "toggled",
		    G_CALLBACK (set_render_choice),
		    GINT_TO_POINTER (RENDER_CHOICE_PREVIEW));

  if (!mainw->clip_switched&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 20);

    radiobutton = gtk_radio_button_new (NULL);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton), radiobutton_group);
    radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
    
    gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);
    
    label=gtk_label_new_with_mnemonic (_ ("Render events to _same clip"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);
    
    eventbox=gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      radiobutton);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

    GTK_WIDGET_SET_FLAGS (radiobutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
    g_signal_connect (GTK_OBJECT (radiobutton), "toggled",
                      G_CALLBACK (set_render_choice),
                      GINT_TO_POINTER (RENDER_CHOICE_SAME_CLIP));
  }


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 20);

  radiobutton = gtk_radio_button_new (NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
    
  gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);
    
  label=gtk_label_new_with_mnemonic (_ ("Render events to _new clip"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);
    
  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  
  GTK_WIDGET_SET_FLAGS (radiobutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  g_signal_connect (GTK_OBJECT (radiobutton), "toggled",
		    G_CALLBACK (set_render_choice),
		    GINT_TO_POINTER (RENDER_CHOICE_NEW_CLIP));
  


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 20);

  radiobutton = gtk_radio_button_new (NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
    
  gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);
    
  label=gtk_label_new_with_mnemonic (_("View/edit events in _multitrack window (test)"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);
    
  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

  GTK_WIDGET_SET_FLAGS (radiobutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  g_signal_connect (GTK_OBJECT (radiobutton), "toggled",
		    G_CALLBACK (set_render_choice),
		    GINT_TO_POINTER (RENDER_CHOICE_MULTITRACK));

  if (!allow_mt) gtk_widget_set_sensitive(radiobutton,FALSE);



  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 20);

  radiobutton = gtk_radio_button_new (NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
    
  gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);

  if (mainw->stored_event_list!=NULL) gtk_widget_set_sensitive(radiobutton,FALSE);
    
  label=gtk_label_new_with_mnemonic (_("View/edit events in _event window"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);
    
  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

  GTK_WIDGET_SET_FLAGS (radiobutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  g_signal_connect (GTK_OBJECT (radiobutton), "toggled",
		    G_CALLBACK (set_render_choice),
		    GINT_TO_POINTER (RENDER_CHOICE_EVENT_LIST));

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_dialog_add_action_widget (GTK_DIALOG (e_rec_dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (set_render_choice_button),
		    GINT_TO_POINTER (RENDER_CHOICE_DISCARD));

  accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  gtk_window_add_accel_group (GTK_WINDOW (e_rec_dialog), accel_group);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (e_rec_dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default (okbutton);
  gtk_widget_show_all (e_rec_dialog);

  return e_rec_dialog;

}



static void event_list_free_events (weed_plant_t *event_list) {
  weed_plant_t *event,*next_event;
  event=get_first_event(event_list);

  while (event!=NULL) {
    next_event=get_next_event(event);
    if (mainw->multitrack!=NULL&&event_list==mainw->multitrack->event_list) mt_fixup_events(mainw->multitrack,event,NULL);
    weed_plant_free (event);
    event=next_event;
  }
}

void event_list_free (weed_plant_t *event_list) {
  if (event_list==NULL) return;
  event_list_free_events(event_list);
  weed_plant_free (event_list);
}

void event_list_replace_events (weed_plant_t *event_list, weed_plant_t *new_event_list) {
  if (event_list==NULL) return;
  event_list_free_events(event_list);
  weed_set_voidptr_value(event_list,"first",get_first_event(new_event_list));
  weed_set_voidptr_value(event_list,"last",get_last_event(new_event_list));
}




gboolean event_list_to_block (weed_plant_t *event_list, gint num_events) {
  // translate our new event_list to older event blocks
  // by now we should have eliminated clip switches and param settings

  // first we count the frame events
  int i=0,error;
  weed_plant_t *event;

  if (event_list==NULL) return TRUE;

  // then we create event_frames

  if (!create_event_space(num_events)) {
    do_memory_error_dialog();
    return FALSE;
  }

  event=get_first_event(event_list);

  while (event!=NULL) {
    if (get_event_hint(event)==WEED_EVENT_HINT_FRAME) {
      (cfile->events[0]+i++)->value=weed_get_int_value (event,"frames",&error);
    }
    event=get_next_event(event);
  }
  return TRUE;
}



void event_list_close_gaps (weed_plant_t *event_list) {
  // close gap at start of event list, and between record_end and record_start markers
  weed_plant_t *event,*next_event;
  weed_timecode_t tc,tc_start,rec_end_tc=0;
  int marker_type,error;

  if (event_list==NULL) return;
  event=get_first_event(event_list);
  tc_start=get_event_timecode(event);

  while (event!=NULL) {
    next_event=get_next_event(event);

    tc=get_event_timecode (event)-tc_start;
    weed_set_int64_value (event,"timecode",tc);

    if (WEED_EVENT_IS_MARKER(event)) {
      marker_type=weed_get_int_value(event,"lives_type",&error);
      if (marker_type==EVENT_MARKER_RECORD_END) {
	rec_end_tc=tc;
	delete_event(event_list,event);
      }
      else if (marker_type==EVENT_MARKER_RECORD_START) {
	tc_start+=tc-rec_end_tc;
	delete_event(event_list,event);
      }
    }
    event=next_event;
  }
}



void add_track_to_avol_init(weed_plant_t *filter, weed_plant_t *event, int nbtracks, boolean behind) {
  // added a new video track - now we need to update our audio volume and pan effect
  weed_plant_t **in_ptmpls;

  int *new_in_tracks;
  int *igns,*nigns;

  int num_in_tracks,x=-nbtracks;
  int error,nparams,numigns;

  int bval;

  void **pchainx,*pchange;

  register int i,j;

  // add a new value to in_tracks
  num_in_tracks=weed_leaf_num_elements(event,"in_tracks")+1;
  new_in_tracks=g_malloc(num_in_tracks*sizint);
  for (i=0;i<num_in_tracks;i++) {
    new_in_tracks[i]=x++;
  }
  weed_set_int_array(event,"in_tracks",num_in_tracks,new_in_tracks);
  g_free(new_in_tracks);

  weed_set_int_value(event,"in_count",weed_get_int_value(event,"in_count",&error)+1);

  // update all param_changes

  nparams=weed_leaf_num_elements(event,"in_parameters");
  pchainx=weed_get_voidptr_array(event,"in_parameters",&error);

  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  for (i=0;i<nparams;i++) {
    pchange=(weed_plant_t *)pchainx[i];
    bval=WEED_FALSE;
    while (pchange!=NULL) {
      fill_param_vals_to(pchange,in_ptmpls[i],behind?num_in_tracks-1:1);
      if (weed_plant_has_leaf(pchange,"ignore")) {
	numigns=weed_leaf_num_elements(pchange,"ignore")+1;
	igns=weed_get_boolean_array(pchange,"ignore",&error);
	nigns=g_malloc(numigns*sizint);

	for (j=0;j<numigns;j++) {
	  if (behind) {
	    if (j<numigns-1) nigns[j]=igns[j];
	    else nigns[j]=bval;
	  }
	  else {
	    if (j==0) nigns[j]=igns[j];
	    else if (j==1) nigns[j]=bval;
	    else nigns[j]=igns[j-1];
	  }
	}
	weed_set_boolean_array(pchange,"ignore",numigns,nigns);
	weed_free(igns);
	g_free(nigns);
      }
      pchange=weed_get_voidptr_value(pchange,"next_change",&error);
      bval=WEED_TRUE;
    }
  }

  weed_free(in_ptmpls);
}



void event_list_add_track (weed_plant_t *event_list, gint layer) {
  // in this function we insert a new track before existing tracks
  // TODO - memcheck
  weed_plant_t *event;
  gint numframes;
  int *clips,*frames,*newclips,*newframes,i,error;
  int num_in_tracks,num_out_tracks;
  int *in_tracks,*out_tracks;

  if (event_list==NULL) return;

  event=get_first_event(event_list);
  while (event!=NULL) {
    switch (get_event_hint(event)) {
    case WEED_EVENT_HINT_FRAME:
      numframes=weed_leaf_num_elements(event,"clips");
      clips=weed_get_int_array(event,"clips",&error);
      frames=weed_get_int_array(event,"frames",&error);
      if (numframes==1&&clips[0]==-1&&frames[0]==0) {
	// for blank frames, we don't do anything
	weed_free(clips);
	weed_free(frames);
	break;
      }

      newclips=(int *)g_malloc((numframes+1)*sizint);
      newframes=(int *)g_malloc((numframes+1)*sizint);
      
      newclips[layer]=-1;
      newframes[layer]=0;
      for (i=0;i<numframes;i++) {
	if (i<layer) {
	  newclips[i]=clips[i];
	  newframes[i]=frames[i];
	}
	else {
	  newclips[i+1]=clips[i];
	  newframes[i+1]=frames[i];
	}
      }
      numframes++;

      weed_set_int_array(event,"clips",numframes,newclips);
      weed_set_int_array(event,"frames",numframes,newframes);
      
      g_free(newclips);
      g_free(newframes);
      weed_free(clips);
      weed_free(frames);
      break;
    case WEED_EVENT_HINT_FILTER_INIT:
      if (weed_plant_has_leaf(event,"in_tracks")&&(num_in_tracks=weed_leaf_num_elements(event,"in_tracks"))>0) {
	in_tracks=weed_get_int_array(event,"in_tracks",&error);
	for (i=0;i<num_in_tracks;i++) {
	  if (in_tracks[i]>=layer) in_tracks[i]++;
	}
	weed_set_int_array(event,"in_tracks",num_in_tracks,in_tracks);
	weed_free(in_tracks);
      }
      if (weed_plant_has_leaf(event,"out_tracks")&&(num_out_tracks=weed_leaf_num_elements(event,"out_tracks"))>0) {
	out_tracks=weed_get_int_array(event,"out_tracks",&error);
	for (i=0;i<num_out_tracks;i++) {
	  if (out_tracks[i]>=layer) out_tracks[i]++;
	}
	weed_set_int_array(event,"out_tracks",num_out_tracks,out_tracks);
	weed_free(out_tracks);
      }
      break;
    }
    event=get_next_event(event);
  }
}


weed_plant_t *append_frame_event (weed_plant_t *event_list, weed_timecode_t tc, int numframes, int *clips, int *frames) {
  // append a frame event to an event_list
  weed_plant_t *event,*prev;
  int error;
  // returns NULL on memory error

  ///////////// TODO - to func //////////////
  if (event_list==NULL) {
    event_list=weed_plant_new(WEED_PLANT_EVENT_LIST);
    if (event_list==NULL) return NULL;
    error=weed_set_int_value(event_list,"weed_event_list_api",WEED_EVENT_API_VERSION);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error=weed_set_voidptr_value(event_list,"first",NULL);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error=weed_set_voidptr_value(event_list,"last",NULL);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
  }

  event=weed_plant_new (WEED_PLANT_EVENT);
  if (event==NULL) return NULL;
  error=weed_set_voidptr_value(event,"next",NULL);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;



  ////////////////////////////////////////

  error=weed_set_int64_value (event,"timecode",tc);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  error=weed_set_int_value (event,"hint",WEED_EVENT_HINT_FRAME);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  error=weed_set_int_array (event,"clips",numframes,clips);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  error=weed_set_int_array (event,"frames",numframes,frames);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;

  if (get_first_event(event_list)==NULL) {
    error=weed_set_voidptr_value(event_list,"first",event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error=weed_set_voidptr_value(event,"previous",NULL);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }
  else {
    error=weed_set_voidptr_value(event,"previous",get_last_event(event_list));
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }
  weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN);
  prev=get_prev_event(event);
  if (prev!=NULL) {
    error=weed_set_voidptr_value(prev,"next",event);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  }
  error=weed_set_voidptr_value(event_list,"last",event);
  if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
  //////////////////////////////////////

  return event_list;
}


void **filter_init_add_pchanges (weed_plant_t *event_list, weed_plant_t *plant, weed_plant_t *init_event, int ntracks) {
  int i,error;
  weed_plant_t **in_params,**in_ptmpls;
  int num_params;
  void **pchain=NULL;

  weed_plant_t *filter=plant;
  gboolean is_inst=FALSE;

  weed_timecode_t tc=get_event_timecode(init_event);

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) {
    filter=weed_instance_get_filter(plant,TRUE);
    is_inst=TRUE;
  }

  // add param_change events and set "in_params"
  if (!weed_plant_has_leaf(filter,"in_parameter_templates")||weed_get_plantptr_value(filter,"in_parameter_templates",&error)==NULL) return NULL;

  num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  pchain=(void **)g_malloc(num_params*sizeof(void *));

  if (!is_inst) in_params=weed_params_create(filter,TRUE);
  else in_params=weed_get_plantptr_array(plant,"in_parameters",&error);

  for (i=num_params-1;i>=0;i--) {
    
    pchain[i]=weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value((weed_plant_t *)pchain[i],"hint",WEED_EVENT_HINT_PARAM_CHANGE);
    weed_set_int64_value((weed_plant_t *)pchain[i],"timecode",tc);

    if (is_perchannel_multiw(in_params[i])) {
      // if the parameter is element-per-channel, fill up to number of channels
      fill_param_vals_to(in_params[i],in_ptmpls[i],ntracks-1);
    }

    weed_leaf_copy((weed_plant_t *)pchain[i],"value",in_params[i],"value");

    weed_set_int_value((weed_plant_t *)pchain[i],"index",i);
    weed_set_voidptr_value((weed_plant_t *)pchain[i],"init_event",init_event);
    weed_set_voidptr_value((weed_plant_t *)pchain[i],"next_change",NULL);
    weed_set_voidptr_value((weed_plant_t *)pchain[i],"prev_change",NULL);
    weed_add_plant_flags((weed_plant_t *)pchain[i],WEED_LEAF_READONLY_PLUGIN);

    insert_param_change_event_at(event_list,init_event,(weed_plant_t *)pchain[i]);

  }

  weed_free(in_params);
  weed_free(in_ptmpls);

  weed_set_voidptr_array(init_event,"in_parameters",num_params,pchain);
  return pchain;
}


weed_plant_t *append_filter_init_event (weed_plant_t *event_list, weed_timecode_t tc, int filter_idx, 
					int num_in_tracks, int key, weed_plant_t *inst) {
  weed_plant_t *event,*prev,*filter,*chan;
  gint e_in_channels,e_out_channels,e_ins,e_outs;
  gint total_in_channels=0;
  gint total_out_channels=0;
  int i,error;
  weed_plant_t **ctmpl;
  int my_in_tracks=0;
  gchar *tmp;

  if (event_list==NULL) {
    event_list=weed_plant_new(WEED_PLANT_EVENT_LIST);
    if (event_list==NULL) return NULL;
    error=weed_set_int_value(event_list,"weed_event_list_api",WEED_EVENT_API_VERSION);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error=weed_set_voidptr_value(event_list,"first",NULL);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    error=weed_set_voidptr_value(event_list,"last",NULL);
    if (error==WEED_ERROR_MEMORY_ALLOCATION) return NULL;
    weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
  }

  event=weed_plant_new (WEED_PLANT_EVENT);
  weed_set_voidptr_value(event,"next",NULL);

  weed_set_int64_value (event,"timecode",tc);
  weed_set_int_value (event,"hint",WEED_EVENT_HINT_FILTER_INIT);
  weed_set_string_value (event,"filter",(tmp=make_weed_hashname (filter_idx,TRUE)));
  g_free(tmp);

  filter=get_weed_filter(filter_idx);

  if (weed_plant_has_leaf(filter,"in_channel_templates")) 
    total_in_channels=weed_leaf_num_elements(filter,"in_channel_templates");
  if (total_in_channels>0) {
    int count[total_in_channels];
    ctmpl=weed_get_plantptr_array(filter,"in_channel_templates",&error);
    for (i=0;i<total_in_channels;i++) {
      if (!weed_plant_has_leaf(ctmpl[i],"disabled")||weed_get_boolean_value(ctmpl[i],"disabled",&error)!=WEED_TRUE) {
	count[i]=1;
	my_in_tracks++;
	weed_set_int_value(ctmpl[i],"repeats",1);
      }
      else count[i]=0;
    }

    if (my_in_tracks<num_in_tracks) {
      int repeats;
      // we need to use some repeated channels
      for (i=0;i<total_in_channels;i++) {
	if (weed_plant_has_leaf(ctmpl[i],"max_repeats")&&(count[i]>0||has_usable_palette(ctmpl[i]))) {
	  repeats=weed_get_int_value(ctmpl[i],"max_repeats",&error);
	  if (repeats==0) {
	    count[i]+=num_in_tracks-my_in_tracks;
	    weed_set_int_value(ctmpl[i],"repeats",count[i]);
	    weed_set_boolean_value(ctmpl[i],"disabled",WEED_FALSE);
	    break;
	  }
	  count[i]+=num_in_tracks-my_in_tracks>=repeats-1?repeats-1:num_in_tracks-my_in_tracks;
	  weed_set_int_value(ctmpl[i],"repeats",count[i]);
	  weed_set_boolean_value(ctmpl[i],"disabled",WEED_FALSE);
	  my_in_tracks+=count[i]-1;
	  if (my_in_tracks==num_in_tracks) break;
	}
      }
    }
    weed_set_int_array(event,"in_count",total_in_channels,count);
    weed_free(ctmpl);
  }

  if (weed_plant_has_leaf(filter,"out_channel_templates")) 
    total_out_channels=weed_leaf_num_elements(filter,"out_channel_templates");
  if (total_out_channels>0) {
    int count[total_out_channels];
    ctmpl=weed_get_plantptr_array(filter,"out_channel_templates",&error);
    for (i=0;i<total_out_channels;i++) {
      if (!weed_plant_has_leaf(ctmpl[i],"disabled")||weed_get_boolean_value(ctmpl[i],"disabled",&error)!=WEED_TRUE) count[i]=1;
      else count[i]=0;
    }
    weed_set_int_array(event,"out_count",total_out_channels,count);
    weed_free(ctmpl);
  }


  e_ins=e_in_channels=enabled_in_channels (get_weed_filter(filter_idx),FALSE);
  e_outs=e_out_channels=enabled_out_channels (get_weed_filter(filter_idx),FALSE);

  // discount alpha_channels (in and out)
  if (inst!=NULL) {
    for (i=0;i<e_ins;i++) {
      chan=get_enabled_channel(inst,i,TRUE);
      if (weed_palette_is_alpha_palette(weed_layer_get_palette(chan))) e_in_channels--;
    }
    
    for (i=0;i<e_outs;i++) {
      chan=get_enabled_channel(inst,i,FALSE);
      if (weed_palette_is_alpha_palette(weed_layer_get_palette(chan))) e_out_channels--;
    }
  }

  // here we map our tracks to channels
  if (e_in_channels!=0) {
    if (e_in_channels==1) {
      weed_set_int_value (event,"in_tracks",0);
    }
    else {
      int *tracks=(int *)g_malloc (2*sizint);
      tracks[0]=0;
      tracks[1]=1;
      weed_set_int_array (event,"in_tracks",2,tracks);
      g_free (tracks);
    }
  }

  if (e_out_channels>0) {
    weed_set_int_value (event,"out_tracks",0);
  }

  if (key>-1) {
    weed_set_int_value(event,"host_key",key);
    weed_set_int_value(event,"host_mode",rte_key_getmode(key));
  }


  ///////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG_EVENTS
  g_print ("adding init event at tc %"PRId64"\n",tc);
#endif

  if (get_first_event(event_list)==NULL) {
    weed_set_voidptr_value(event_list,"first",event);
    weed_set_voidptr_value(event,"previous",NULL);
  }
  else {
    weed_set_voidptr_value(event,"previous",get_last_event(event_list));
  }
  weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN);
  prev=get_prev_event(event);
  if (prev!=NULL) weed_set_voidptr_value(prev,"next",event);
  weed_set_voidptr_value(event_list,"last",event);

  return event_list;
}


weed_plant_t *append_filter_deinit_event (weed_plant_t *event_list, weed_timecode_t tc, void *init_event, void **pchain) {
  weed_plant_t *event,*prev;

  if (event_list==NULL) {
    event_list=weed_plant_new(WEED_PLANT_EVENT_LIST);
    weed_set_int_value(event_list,"weed_event_list_api",WEED_EVENT_API_VERSION);
    weed_set_voidptr_value(event_list,"first",NULL);
    weed_set_voidptr_value(event_list,"last",NULL);
    weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
  }

  event=weed_plant_new (WEED_PLANT_EVENT);
  weed_set_voidptr_value(event,"next",NULL);

  // TODO - error check
  weed_set_int64_value (event,"timecode",tc);
  weed_set_int_value (event,"hint",WEED_EVENT_HINT_FILTER_DEINIT);
  weed_set_voidptr_value (event,"init_event",init_event);
  weed_set_voidptr_value ((weed_plant_t *)init_event,"deinit_event",event);

  if (pchain!=NULL) {
    int error;
    gchar *filter_hash=weed_get_string_value((weed_plant_t *)init_event,"filter",&error);
    int idx=weed_get_idx_for_hashname(filter_hash,TRUE);
    weed_plant_t *filter=get_weed_filter(idx);
    int num_params=num_in_params(filter,TRUE,TRUE);
    weed_set_voidptr_array(event,"in_parameters",num_params,pchain);
    weed_free(filter_hash);
  }

  if (get_first_event(event_list)==NULL) {
    weed_set_voidptr_value(event_list,"first",event);
    weed_set_voidptr_value(event,"previous",NULL);
  }
  else {
    weed_set_voidptr_value(event,"previous",get_last_event(event_list));
  }
  weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN);
  prev=get_prev_event(event);
  if (prev!=NULL) weed_set_voidptr_value(prev,"next",event);
  weed_set_voidptr_value(event_list,"last",event);

  return event_list;
}



weed_plant_t *append_param_change_event (weed_plant_t *event_list, weed_timecode_t tc, gint pnum, 
					 weed_plant_t *param, void *init_event, void **pchain) {
  weed_plant_t *event,*prev,*xevent;
  weed_plant_t *last_pchange_event;
  int error;

  if (event_list==NULL) {
    event_list=weed_plant_new(WEED_PLANT_EVENT_LIST);
    weed_set_int_value(event_list,"weed_event_list_api",WEED_EVENT_API_VERSION);
    weed_set_voidptr_value(event_list,"first",NULL);
    weed_set_voidptr_value(event_list,"last",NULL);
    weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
  }

  event=weed_plant_new (WEED_PLANT_EVENT);
  weed_set_voidptr_value(event,"next",NULL);

  // TODO - error check
  weed_set_int64_value (event,"timecode",tc);
  weed_set_int_value (event,"hint",WEED_EVENT_HINT_PARAM_CHANGE);
  weed_set_voidptr_value (event,"init_event",init_event);
  weed_set_int_value (event, "index", pnum);
  weed_leaf_copy (event,"value",param,"value");

  last_pchange_event=(weed_plant_t *)pchain[pnum];
  while ((xevent=(weed_plant_t *)weed_get_voidptr_value(last_pchange_event,"next_change",&error))!=NULL) 
    last_pchange_event=xevent;

  weed_set_voidptr_value(last_pchange_event,"next_change",event);
  weed_set_voidptr_value(event,"prev_change",last_pchange_event);
  weed_set_voidptr_value(event,"next_change",NULL);

  if (get_first_event(event_list)==NULL) {
    weed_set_voidptr_value(event_list,"first",event);
    weed_set_voidptr_value(event,"previous",NULL);
  }
  else {
    weed_set_voidptr_value(event,"previous",get_last_event(event_list));
  }
  weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN);
  prev=get_prev_event(event);
  if (prev!=NULL) weed_set_voidptr_value(prev,"next",event);
  weed_set_voidptr_value(event_list,"last",event);

  return event_list;
}




weed_plant_t *append_filter_map_event (weed_plant_t *event_list, weed_timecode_t tc, void **init_events) {
  weed_plant_t *event,*prev;
  int i=0;

  if (event_list==NULL) {
    event_list=weed_plant_new(WEED_PLANT_EVENT_LIST);
    weed_set_int_value(event_list,"weed_event_list_api",WEED_EVENT_API_VERSION);
    weed_set_voidptr_value(event_list,"first",NULL);
    weed_set_voidptr_value(event_list,"last",NULL);
    weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
  }

  event=weed_plant_new (WEED_PLANT_EVENT);
  weed_set_voidptr_value(event,"next",NULL);

  // TODO - error check
  weed_set_int64_value (event,"timecode",tc);
  weed_set_int_value (event,"hint",WEED_EVENT_HINT_FILTER_MAP);

  if (init_events!=NULL) for (i=0;init_events[i]!=NULL;i++);

  if (i==0) weed_set_voidptr_value(event,"init_events",NULL);
  else weed_set_voidptr_array (event,"init_events",i,init_events);

#ifdef DEBUG_EVENTS
  g_print ("adding map event %p at tc %"PRId64"\n",init_events[0],tc);
#endif

  if (get_first_event(event_list)==NULL) {
    weed_set_voidptr_value(event_list,"first",event);
    weed_set_voidptr_value(event,"previous",NULL);
  }
  else {
    weed_set_voidptr_value(event,"previous",get_last_event(event_list));
  }
  weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN);
  prev=get_prev_event(event);
  if (prev!=NULL) weed_set_voidptr_value(prev,"next",event);
  weed_set_voidptr_value(event_list,"last",event);

  return event_list;
}



weed_plant_t *process_events (weed_plant_t *next_event, boolean process_audio, weed_timecode_t curr_tc) {
  // here we play back (preview) with an event_list
  // we process all events, but drop frames (unless mainw->nodrop is set)

  static weed_timecode_t aseek_tc=0;
  static double stored_avel=0.;

  weed_timecode_t tc,next_tc;
  int hint;
  int key,idx;
  char *key_string;
  void *init_event;
  weed_plant_t *next_frame_event,*return_event;
  int i;
  char *filter_name;
  int new_file;
  gint current_file;
  int error;
  weed_plant_t *filter;
  int num_channels;
  int *in_count=NULL;
  weed_plant_t **ctmpl;
  int num_params;
  weed_plant_t *inst;
  weed_plant_t **source_params,**in_params;
  int num_in_count=0;


  if (next_event==NULL) return NULL;

  tc=get_event_timecode (next_event);

  if (mainw->playing_file!=-1&&tc>curr_tc) {
    // next event is in our future
    if (mainw->multitrack!=NULL&&mainw->last_display_ticks>0) {
      if ((mainw->fixed_fpsd>0.&&(curr_tc-mainw->last_display_ticks)/U_SEC>=1./mainw->fixed_fpsd)||
	  (mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.&&mainw->ext_playback&&
	   (curr_tc-mainw->last_display_ticks)/U_SEC>=1./mainw->vpp->fixed_fpsd)) {
	// ...but playing at fixed fps, which is faster than mt fps
	mainw->pchains=pchains;
	load_frame_image (cfile->last_frameno>=1?cfile->last_frameno:cfile->start);
	if (mainw->last_display_ticks==0) mainw->last_display_ticks=curr_tc;
	else {
	  if (mainw->vpp!=NULL&&mainw->ext_playback&&mainw->vpp->fixed_fpsd>0.) 
	    mainw->last_display_ticks+=U_SEC/mainw->vpp->fixed_fpsd;
	  else if (mainw->fixed_fpsd>0.) 
	    mainw->last_display_ticks+=U_SEC/mainw->fixed_fpsd;
	  else mainw->last_display_ticks=curr_tc;
	}
	mainw->pchains=NULL;
      }
    }

    return next_event;
  }

  aseek_tc+=(weed_timecode_t)((gdouble)(tc-mainw->cevent_tc)*stored_avel);
  mainw->cevent_tc=tc;

  return_event=get_next_event(next_event);
  hint=get_event_hint (next_event);
  switch (hint) {
  case WEED_EVENT_HINT_FRAME:

#ifdef DEBUG_EVENTS
    g_print ("event: frame event at tc %"PRId64" curr_tc=%"PRId64"\n",tc,curr_tc);
#endif


  if (mainw->multitrack==NULL&&prefs->audio_player==AUD_PLAYER_JACK&&weed_plant_has_leaf(next_event,"audio_clips")) {
    // keep track of current seek position, for animating playback pointers
    int *aclips=weed_get_int_array(next_event,"audio_clips",&error);
    double *aseeks=weed_get_double_array(next_event,"audio_seeks",&error);

    if (aclips[1]>0) {
      aseek_tc=aseeks[0]*U_SEC;
      stored_avel=aseeks[1];
    }
    
    weed_free(aclips);
    weed_free(aseeks);
  }


  if ((next_frame_event=get_next_frame_event (next_event))!=NULL) {
    next_tc=get_event_timecode (next_frame_event);
    // drop frame if it is too far behind
    if (mainw->playing_file>-1&&!mainw->noframedrop&&next_tc<=curr_tc) break;
    if (!mainw->fs&&prefs->show_framecount) {
      g_signal_handler_block(mainw->spinbutton_pb_fps,mainw->pb_fps_func);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->pb_fps);
      g_signal_handler_unblock(mainw->spinbutton_pb_fps,mainw->pb_fps_func);
    }
  }

  mainw->num_tracks=weed_leaf_num_elements (next_event,"clips");
    
  if (mainw->clip_index!=NULL) weed_free(mainw->clip_index);
  if (mainw->frame_index!=NULL) weed_free(mainw->frame_index);

  mainw->clip_index=weed_get_int_array (next_event,"clips",&error);
  mainw->frame_index=weed_get_int_array (next_event,"frames",&error);

  // if we are in multitrack mode, we will just set up NULL layers and let the effects pull our frames
  if (mainw->multitrack!=NULL) {
    if ((mainw->fixed_fpsd<=0.&&(mainw->vpp==NULL||mainw->vpp->fixed_fpsd<=0.||!mainw->ext_playback))
	||(mainw->fixed_fpsd>0.&&(curr_tc-mainw->last_display_ticks)/U_SEC>=1./mainw->fixed_fpsd)||
	(mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.&&mainw->ext_playback&&
	 (curr_tc-mainw->last_display_ticks)/U_SEC>=1./mainw->vpp->fixed_fpsd)) {
      mainw->pchains=pchains;
      load_frame_image (cfile->frameno);
      if (mainw->last_display_ticks==0) mainw->last_display_ticks=curr_tc;
      else {
	if (mainw->vpp!=NULL&&mainw->ext_playback&&mainw->vpp->fixed_fpsd>0.) 
	  mainw->last_display_ticks+=U_SEC/mainw->vpp->fixed_fpsd;
	else if (mainw->fixed_fpsd>0.) 
	  mainw->last_display_ticks+=U_SEC/mainw->fixed_fpsd;
	else mainw->last_display_ticks=curr_tc;
      }
      mainw->pchains=NULL;
    }
  }
  else {
    if (mainw->num_tracks>1) {
      mainw->blend_file=mainw->clip_index[1];
      if (mainw->blend_file>-1) mainw->files[mainw->blend_file]->frameno=mainw->frame_index[1];
    }
    else mainw->blend_file=-1;

    new_file=-1;
    for (i=0;i<mainw->num_tracks&&new_file==-1;i++) new_file=mainw->clip_index[i];
    if (i==2) mainw->blend_file=-1;

#ifdef DEBUG_EVENTS
    g_print ("event: front frame is %d tc %"PRId64" curr_tc=%"PRId64"\n",mainw->frame_index[0],tc,curr_tc);
#endif

    // handle case where new_file==-1: we must somehow create a blank frame in load_frame_image
    if (new_file==-1) new_file=mainw->current_file;
      
    if (new_file!=mainw->current_file) {
      mainw->files[new_file]->frameno=mainw->frame_index[i-1];
	
      if (new_file!=mainw->scrap_file) {
	// switch to a new file
	mainw->noswitch=FALSE;
	do_quick_switch (new_file);
	mainw->noswitch=TRUE;
	cfile->next_event=return_event;
	return_event=NULL;
      }
      else {
	mainw->files[new_file]->hsize=cfile->hsize; // set size of scrap file
	mainw->files[new_file]->vsize=cfile->vsize;
	current_file=mainw->current_file;
	mainw->current_file=new_file;
	mainw->aframeno=(gdouble)(aseek_tc/U_SEC)*cfile->fps;
	mainw->pchains=pchains;
	load_frame_image (cfile->frameno);
	mainw->pchains=NULL;
	if (mainw->playing_file>-1) while (g_main_context_iteration (NULL,FALSE));
	mainw->current_file=current_file;
      }
      //weed_free(mainw->clip_index);
      //weed_free(mainw->frame_index);
      //mainw->clip_index=mainw->frame_index=NULL;
      break;
    }
    else {
      if (mainw->multitrack!=NULL&&new_file==mainw->multitrack->render_file) {
	cfile->frameno=0; // will force blank frame creation
      }
      else cfile->frameno=mainw->frame_index[i-1];
      mainw->aframeno=(gdouble)(aseek_tc/U_SEC)*cfile->fps;
      mainw->pchains=pchains;
      load_frame_image (cfile->frameno);
      mainw->pchains=NULL;
    }
  }
  //weed_free(mainw->clip_index);
  //weed_free(mainw->frame_index);
  //mainw->clip_index=mainw->frame_index=NULL;
  if (mainw->playing_file>-1) while (g_main_context_iteration(NULL,FALSE));
  cfile->next_event=get_next_event(next_event);
  break;
  case WEED_EVENT_HINT_FILTER_INIT:
    // effect init
    //  bind the weed_fx to next free key/0
    filter_name=weed_get_string_value (next_event,"filter",&error);
    idx=weed_get_idx_for_hashname (filter_name,TRUE);
    weed_free (filter_name);

    if (idx!=-1) {
      filter=get_weed_filter(idx);

      if (!process_audio&&is_pure_audio(filter,FALSE)) {
	if (weed_plant_has_leaf(next_event,"host_tag")) weed_leaf_delete(next_event,"host_tag");
	break; // audio effects are processed in the audio renderer
      }

      if (process_audio&&!is_pure_audio(filter,FALSE)) break;

      key=get_next_free_key();
      weed_add_effectkey_by_idx (key+1,idx);
      key_string=g_strdup_printf ("%d",key);
      weed_set_string_value (next_event,"host_tag",key_string);
      g_free (key_string);
    
#ifdef DEBUG_EVENTS
      g_print ("event: init effect on key %d at tc %"PRId64" curr_tc=%"PRId64"\n",key,tc,curr_tc);
#endif
    
      if (weed_plant_has_leaf(next_event,"in_count")) {
	num_in_count=weed_leaf_num_elements(next_event,"in_count");
	in_count=weed_get_int_array(next_event,"in_count",&error);
      }

      if (weed_plant_has_leaf(filter,"in_channel_templates")) {
	if ((num_channels=weed_leaf_num_elements(filter,"in_channel_templates"))>0) {
	  ctmpl=weed_get_plantptr_array(filter,"in_channel_templates",&error);
	  if (num_channels!=num_in_count) LIVES_ERROR("num_in_count != num_in_channels");
	  for (i=0;i<num_channels;i++) {
	    if (in_count[i]>0) {
	      weed_set_boolean_value(ctmpl[i],"disabled",WEED_FALSE);
	      weed_set_int_value(ctmpl[i],"repeats",in_count[i]);
	    }
	    else weed_set_boolean_value(ctmpl[i],"disabled",WEED_TRUE);
	  }
	  weed_free(ctmpl);
	}
      }

      if (in_count!=NULL) weed_free(in_count);
	
      if (weed_plant_has_leaf(filter,"out_channel_templates")) {
	if ((num_channels=weed_leaf_num_elements(filter,"out_channel_templates"))>0) {
	  ctmpl=weed_get_plantptr_array(filter,"out_channel_templates",&error);
	  for (i=0;i<num_channels;i++) {
	    if (!weed_plant_has_leaf(ctmpl[i],"disabled")||weed_get_boolean_value(ctmpl[i],"disabled",&error)!=WEED_TRUE) 
	      weed_set_boolean_value(ctmpl[i],"disabled",WEED_FALSE);
	    else weed_set_boolean_value(ctmpl[i],"disabled",WEED_TRUE);
	  }
	  weed_free(ctmpl);
	}
      }

      weed_init_effect (key);

      // reinit effect with saved parameters
      inst=rte_keymode_get_instance(key+1,0);
	// TODO *** - handle compound fx

      num_params=num_in_params(inst,TRUE,TRUE);
      if (num_params>0) {
	weed_call_deinit_func(inst);
	if (weed_plant_has_leaf(next_event,"in_parameters")) {
	  source_params=(weed_plant_t **)weed_get_voidptr_array(next_event,"in_parameters",&error);
	  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

	  pchains[key]=weed_get_voidptr_array(next_event,"in_parameters",&error);
	  for (i=0;i<num_params;i++) {
	    if (source_params!=NULL&&source_params[i]!=NULL&&is_init_pchange(next_event,source_params[i])) 
	      weed_leaf_copy(in_params[i],"value",source_params[i],"value");
	  }
	  if (source_params!=NULL) weed_free(source_params);
	  weed_free(in_params);
	}
	else pchains[key]=NULL;

	if (weed_plant_has_leaf(filter,"init_func")) {
	  weed_deinit_f *init_func_ptr_ptr;
	  weed_init_f init_func;
	  weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
	  init_func=init_func_ptr_ptr[0];
	  set_param_gui_readwrite(inst);
	  update_host_info(inst);
	  if (init_func!=NULL) {
	    gchar *cwd=cd_to_plugin_dir(filter);
	    (*init_func)(inst);
	    lives_chdir(cwd,FALSE);
	    g_free(cwd);
	  }
	  set_param_gui_readonly(inst);
	}
	weed_set_boolean_value(inst,"host_inited",WEED_TRUE);
      }
      else pchains[key]=NULL;

      if (weed_plant_has_leaf(next_event,"host_key")) {
	// mt events will not have this;
	// it is used to connect params and alpha channels during rendering
	// holds our original key/mode values
	
	int hostkey=weed_get_int_value(next_event,"host_key",&error);
	int hostmode=weed_get_int_value(next_event,"host_mode",&error);
	
	weed_set_int_value(inst,"host_key",hostkey);
	weed_set_int_value(inst,"host_mode",hostmode);
	
      }

    }
    break;
      
  case WEED_EVENT_HINT_FILTER_DEINIT:
    init_event=weed_get_voidptr_value ((weed_plant_t *)next_event,"init_event",&error);
    if (weed_plant_has_leaf((weed_plant_t *)init_event,"host_tag")) {
      key_string=weed_get_string_value ((weed_plant_t *)init_event,"host_tag",&error);
      key=atoi (key_string);
      weed_free (key_string);

      filter_name=weed_get_string_value (init_event,"filter",&error);
      idx=weed_get_idx_for_hashname (filter_name,TRUE);
      weed_free (filter_name);
      
      filter=get_weed_filter(idx);

      if (!process_audio) {
	if (is_pure_audio(filter,FALSE)) break; // audio effects are processed in the audio renderer
      }
      
      if (process_audio&&!is_pure_audio(filter,FALSE)) break;

      if (rte_keymode_get_instance(key+1,0)!=NULL) {
	weed_deinit_effect (key);
	weed_delete_effectkey (key+1,0);
      }
      if (pchains[key]!=NULL) weed_free(pchains[key]);
      pchains[key]=NULL;
    }
    break;

  case WEED_EVENT_HINT_FILTER_MAP:
    mainw->filter_map=next_event;
#ifdef DEBUG_EVENTS
    g_print ("got new effect map\n");
#endif
    break;
  case WEED_EVENT_HINT_PARAM_CHANGE:
    break;
  }

  return return_event;
}


lives_render_error_t render_events (gboolean reset) {
  // this is called repeatedly when we are rendering effect changes and/or clip switches
  // if we have clip switches we will resize and build a new clip
  
  static gint progress;
  static weed_plant_t *event,*eventnext;
  gint blend_file=mainw->blend_file;
  static gint frame;
  int hint;
  gchar oname[PATH_MAX];
  GError *error=NULL;
  weed_timecode_t tc;
  int key,idx;
  gchar *key_string,*com;
  gchar *filter_name;
  void *init_event;
  weed_plant_t *next_frame_event;
  weed_timecode_t next_tc=0,next_out_tc;
  static gint out_frame;
  GdkPixbuf *pixbuf=NULL;
  int num_tracks;
  int i;
  int *clip_index,*frame_index;
  int weed_error;
  weed_plant_t **layers,*layer;
  int layer_palette;
  weed_plant_t *filter;
  int num_channels;
  weed_plant_t **ctmpl;
  int *in_count=NULL;
  int num_params;
  int retval;
  weed_plant_t *inst;
  weed_plant_t **source_params,**in_params;
  int num_in_count=0;
  gboolean is_blank=TRUE;
  gboolean firstframe=TRUE;
  gboolean completed=FALSE;
  gdouble chvols[MAX_AUDIO_TRACKS];

  static gint xaclips[MAX_AUDIO_TRACKS];
  static gdouble xaseek[MAX_AUDIO_TRACKS],xavel[MAX_AUDIO_TRACKS],atime; // TODO **
  static gboolean has_audio;
  static lives_render_error_t read_write_error;

  int track,mytrack;

  gchar *blabel=NULL;
  gchar *nlabel;

  if (reset) {
    progress=frame=1;
    event=cfile->next_event;
    out_frame=(gint)((gdouble)(get_event_timecode (event)/U_SECL)*cfile->fps+mainw->play_start);
    if (cfile->frames<out_frame) out_frame=cfile->frames+1;
    cfile->undo_start=out_frame;
    // store this, because if the user previews and there is no audio file yet, achans will get reset
    cfile->undo_achans=cfile->achans;
    cfile->undo_arate=cfile->arate;
    cfile->undo_arps=cfile->arps;
    cfile->undo_asampsize=cfile->asampsize;

    clear_mainw_msg();
    mainw->filter_map=NULL;
    mainw->afilter_map=NULL;
    mainw->audio_event=event;

    for (i=0;i<MAX_AUDIO_TRACKS;i++) {
      xaclips[i]=-1;
      xaseek[i]=xavel[i]=0;
    }
    atime=(gdouble)(out_frame-1.)/cfile->fps;
    has_audio=FALSE;
    read_write_error=LIVES_RENDER_ERROR_NONE;
    return LIVES_RENDER_READY;
  }

  if (mainw->effects_paused) return LIVES_RENDER_EFFECTS_PAUSED;

  nlabel=g_strdup(_("Rendering audio..."));

  if (event!=NULL) {
    eventnext=get_next_event(event);

    hint=get_event_hint (event);
    switch (hint) {
    case WEED_EVENT_HINT_FRAME:
      tc=get_event_timecode (event);

      if ((mainw->multitrack==NULL||(mainw->multitrack->render_vidp&&!mainw->multitrack->pr_audio))&&
	  !(!mainw->clip_switched&&cfile->hsize*cfile->vsize==0)) {
	gint scrap_track=-1;
	  
	num_tracks=weed_leaf_num_elements (event,"clips");
	clip_index=weed_get_int_array (event,"clips",&weed_error);
	frame_index=weed_get_int_array (event,"frames",&weed_error);
	
	if (mainw->scrap_file!=-1) {
	  for (i=0;i<num_tracks;i++) {
	    if (clip_index[i]!=mainw->scrap_file) {
	      scrap_track=-1;
	      break;
	    }
	    if (scrap_track==-1) scrap_track=i;
	  }
	}

	if (scrap_track!=-1) {
	  // do not apply fx, just pull frame
	  layer=weed_plant_new(WEED_PLANT_CHANNEL);
	  weed_set_int_value(layer,"clip",clip_index[scrap_track]);
	  weed_set_int_value(layer,"frame",frame_index[scrap_track]);
	  if (!pull_frame(layer,prefs->image_ext,tc)) {
	    weed_plant_free(layer);
	    layer=NULL;
	  }
	}
	else {
	  layers=(weed_plant_t **)g_malloc((num_tracks+1)*sizeof(weed_plant_t *));
	  for (i=0;i<num_tracks;i++) {
	    if (clip_index[i]>0&&frame_index[i]>0&&mainw->multitrack!=NULL) is_blank=FALSE;
	    layers[i]=weed_plant_new(WEED_PLANT_CHANNEL);
	    weed_set_int_value(layers[i],"clip",clip_index[i]);
	    weed_set_int_value(layers[i],"frame",frame_index[i]);
	    weed_set_voidptr_value(layers[i],"pixel_data",NULL);
	  }
	  layers[i]=NULL;
	  
	  layer=weed_apply_effects(layers,mainw->filter_map,tc,0,0,pchains);
	  
	  for (i=0;layers[i]!=NULL;i++) if (layer!=layers[i]) weed_plant_free(layers[i]);
	  g_free(layers);
	}

	weed_free(clip_index);
	weed_free(frame_index);

	if (layer!=NULL) {
	  layer_palette=weed_layer_get_palette(layer);
	  if (cfile->img_type==IMG_TYPE_JPEG&&layer_palette!=WEED_PALETTE_RGB24&&layer_palette!=WEED_PALETTE_RGBA32) 
	    convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
	  else if (cfile->img_type==IMG_TYPE_PNG&&layer_palette!=WEED_PALETTE_RGBA32) 
	    convert_layer_palette(layer,WEED_PALETTE_RGBA32,0);

	  resize_layer(layer,cfile->hsize,cfile->vsize,GDK_INTERP_HYPER);
	  pixbuf=layer_to_pixbuf(layer);
	  weed_plant_free(layer);
	}

	mainw->blend_file=blend_file;
      }

      next_frame_event=get_next_frame_event (event);

      // get tc of next frame event
      if (next_frame_event!=NULL) next_tc=get_event_timecode (next_frame_event);
      else {
	if (has_audio&&!weed_plant_has_leaf(event,"audio_clips")) {
	  // pad to end with silence
	  cfile->achans=cfile->undo_achans;
	  cfile->arate=cfile->undo_arate;
	  cfile->arps=cfile->undo_arps;
	  cfile->asampsize=cfile->undo_asampsize;
	  if (cfile->proc_ptr!=NULL) {
	    blabel=g_strdup(gtk_label_get_text(GTK_LABEL(cfile->proc_ptr->label)));
	    gtk_label_set_text(GTK_LABEL(cfile->proc_ptr->label),nlabel);
	    gtk_widget_queue_draw(cfile->proc_ptr->processing);
	    while (g_main_context_iteration(NULL,FALSE));
	  }

	  mainw->read_failed=mainw->write_failed=FALSE;
	  if (mainw->read_failed_file!=NULL) g_free(mainw->read_failed_file);
	  mainw->read_failed_file=NULL;

	  render_audio_segment(0, NULL, mainw->multitrack!=NULL?mainw->multitrack->render_file:mainw->current_file,
			       NULL, NULL, atime*U_SEC, q_gint64(tc+(U_SEC/cfile->fps*!is_blank),cfile->fps), 
			       chvols, 1., 1., NULL);
	  
	  if (mainw->write_failed) {
	    int outfile=(mainw->multitrack!=NULL?mainw->multitrack->render_file:mainw->current_file);
	    gchar *outfilename=g_build_filename(prefs->tmpdir,mainw->files[outfile]->handle,"audio",NULL);
	    do_write_failed_error_s(outfilename,NULL);
	    read_write_error=LIVES_RENDER_ERROR_WRITE_AUDIO;
	  }

	  if (mainw->read_failed) {
	    do_read_failed_error_s(mainw->read_failed_file,NULL);
	    read_write_error=LIVES_RENDER_ERROR_READ_AUDIO;
	  }

	  if (cfile->proc_ptr!=NULL) {
	    gtk_label_set_text(GTK_LABEL(cfile->proc_ptr->label),blabel);
	    g_free(blabel);
	    gtk_widget_queue_draw(cfile->proc_ptr->processing);
	    while (g_main_context_iteration(NULL,FALSE));
	  }
	}
      }

      while (cfile->fps>0.) {
	if ((mainw->multitrack==NULL&&prefs->render_audio)||(mainw->multitrack!=NULL&&mainw->multitrack->render_audp)) {
	  if (firstframe) {
	    // see if audio needs appending
	    if (weed_plant_has_leaf(event,"audio_clips")) {
	      int num_aclips=weed_leaf_num_elements(event,"audio_clips");
	      int *aclips=weed_get_int_array(event,"audio_clips",&weed_error);
	      double *aseeks=weed_get_double_array(event,"audio_seeks",&weed_error);
	      gint natracks=1,nbtracks=0;

	      if (mainw->multitrack!=NULL) {
		natracks=weed_leaf_num_elements(mainw->multitrack->avol_init_event,"in_tracks");
		nbtracks=mainw->multitrack->opts.back_audio_tracks;
	      }
	      else {
		natracks=1;
		nbtracks=1;
	      }

	      has_audio=TRUE;

	      if (mainw->multitrack!=NULL) {
		for (track=0;track<natracks;track++) {
		  // insert audio up to tc
		  if (mainw->multitrack->audio_vols!=NULL) {
		    chvols[track]=(gdouble)GPOINTER_TO_INT(g_list_nth_data(mainw->multitrack->audio_vols,track))/1000000.;
		  }
		}
	      }
	      else {
		chvols[0]=1.;
	      }

	      if (q_gint64(tc,cfile->fps)/U_SEC>atime) {
		cfile->achans=cfile->undo_achans;
		cfile->arate=cfile->undo_arate;
		cfile->arps=cfile->undo_arps;
		cfile->asampsize=cfile->undo_asampsize;

		if (cfile->proc_ptr!=NULL) {
		  blabel=g_strdup(gtk_label_get_text(GTK_LABEL(cfile->proc_ptr->label)));
		  gtk_label_set_text(GTK_LABEL(cfile->proc_ptr->label),nlabel);
		  gtk_widget_queue_draw(cfile->proc_ptr->processing);
		  while (g_main_context_iteration(NULL,FALSE));
		}

		mainw->read_failed=mainw->write_failed=FALSE;
		if (mainw->read_failed_file!=NULL) g_free(mainw->read_failed_file);
		mainw->read_failed_file=NULL;

		render_audio_segment(natracks, xaclips, mainw->multitrack!=NULL?mainw->multitrack->render_file:
				     mainw->current_file, xavel, xaseek, (atime*U_SEC+.5), 
				     q_gint64(tc+(U_SEC/cfile->fps*!is_blank),cfile->fps), chvols, 1., 1., NULL);


		if (mainw->write_failed) {
		  int outfile=(mainw->multitrack!=NULL?mainw->multitrack->render_file:mainw->current_file);
		  gchar *outfilename=g_build_filename(prefs->tmpdir,mainw->files[outfile]->handle,"audio",NULL);
		  do_write_failed_error_s(outfilename,NULL);
		  read_write_error=LIVES_RENDER_ERROR_WRITE_AUDIO;
		}
		
		if (mainw->read_failed) {
		  do_read_failed_error_s(mainw->read_failed_file,NULL);
		  read_write_error=LIVES_RENDER_ERROR_READ_AUDIO;
		}

		if (cfile->proc_ptr!=NULL) {
		  gtk_label_set_text(GTK_LABEL(cfile->proc_ptr->label),blabel);
		  g_free(blabel);
		  gtk_widget_queue_draw(cfile->proc_ptr->processing);
		  while (g_main_context_iteration(NULL,FALSE));
		}

		for (i=0;i<natracks;i++) {
		  if (xaclips[i]>0) {
		    xaseek[i]+=(q_gint64(tc,cfile->fps)/U_SEC+1./cfile->fps-atime)*xavel[i];
		  }
		}
		atime=q_gint64(tc,cfile->fps)/U_SEC+1./cfile->fps;
	      }
	      for (i=0;i<num_aclips;i+=2) {
		if (aclips[i+1]>0) { // clipnum
		  mytrack=aclips[i]+nbtracks;
		  xaclips[mytrack]=aclips[i+1];
		  xaseek[mytrack]=aseeks[i];
		  xavel[mytrack]=aseeks[i+1];
		}
	      }
	      weed_free(aclips);
	      weed_free(aseeks);
	    }
	    firstframe=FALSE;
	  }
	}

	if (mainw->multitrack!=NULL&&mainw->multitrack->pr_audio) break;

	if (pixbuf==NULL) break;
	if (next_frame_event==NULL&&is_blank) break; // don't render final blank frame
	next_out_tc=(weed_timecode_t)((out_frame-mainw->play_start)/cfile->fps*U_SEC); // calculate tc of next out frame
	
	if (next_frame_event!=NULL) {
	  if (next_tc<next_out_tc||next_tc-next_out_tc<next_out_tc-tc) break;
	}
	else if (next_out_tc>tc) break;

	// TODO - dirsep

	if (cfile->old_frames>0) g_snprintf(oname,PATH_MAX,"%s/%s/%08d.mgk",prefs->tmpdir,cfile->handle,out_frame);
	// sig_progress...
	g_snprintf (mainw->msg,256,"%d",progress++);

	if (prefs->ocp==-1) prefs->ocp=get_int_pref ("open_compression_percent");

	if (cfile->img_type==IMG_TYPE_JPEG) {
	  if (cfile->old_frames==0) g_snprintf(oname,PATH_MAX,"%s/%s/%08d.jpg",prefs->tmpdir,cfile->handle,out_frame);
	}
	else if (cfile->img_type==IMG_TYPE_PNG) {
	  if (cfile->old_frames==0) g_snprintf(oname,PATH_MAX,"%s/%s/%08d.png",prefs->tmpdir,cfile->handle,out_frame);
	}

	do {
	  retval=0;
	  lives_pixbuf_save (pixbuf, oname, cfile->img_type, 100-prefs->ocp, TRUE, &error);

	  if (error!=NULL) {
	    retval=do_write_failed_error_s_with_retry(oname,error->message,NULL);
	    g_error_free(error);
	    error=NULL;
	    if (retval!=LIVES_RETRY) read_write_error=LIVES_RENDER_ERROR_WRITE_FRAME;
	  }
	} while (retval==LIVES_RETRY);

	cfile->undo_end=out_frame;
	if (out_frame>cfile->frames) cfile->frames=out_frame;
	if (out_frame>cfile->end) cfile->end=out_frame;
	if (cfile->start==0) cfile->start=1;
	out_frame++;
      }
      if (pixbuf!=NULL) lives_object_unref (pixbuf);

      break;
    case WEED_EVENT_HINT_FILTER_INIT:
      // effect init
      //  bind the weed_fx to next free key/0

      filter_name=weed_get_string_value (event,"filter",&weed_error);
      // for now, assume we can find hashname
      idx=weed_get_idx_for_hashname (filter_name,TRUE);
      weed_free (filter_name);

      filter=get_weed_filter(idx);
      if (is_pure_audio(filter,FALSE)) {
	if (weed_plant_has_leaf(event,"host_tag")) weed_leaf_delete(event,"host_tag");
	break; // audio effects are processed in the audio renderer
      }

      key=get_next_free_key();
      weed_add_effectkey_by_idx (key+1,idx);
      key_string=g_strdup_printf ("%d",key);
      weed_set_string_value (event,"host_tag",key_string);
      g_free (key_string);


      if (weed_plant_has_leaf(event,"in_count")) {
	num_in_count=weed_leaf_num_elements(event,"in_count");
	in_count=weed_get_int_array(event,"in_count",&weed_error);
      }
      
      if (weed_plant_has_leaf(filter,"in_channel_templates")) {
	if ((num_channels=weed_leaf_num_elements(filter,"in_channel_templates"))>0) {
	  ctmpl=weed_get_plantptr_array(filter,"in_channel_templates",&weed_error);
	  if (num_channels!=num_in_count) g_printerr("num_in_count != num_in_channels !!\n");
	  for (i=0;i<num_channels;i++) {
	    if (in_count[i]>0) {
	      weed_set_boolean_value(ctmpl[i],"disabled",WEED_FALSE);
	      weed_set_int_value(ctmpl[i],"repeats",in_count[i]);
	    }
	    else weed_set_boolean_value(ctmpl[i],"disabled",WEED_TRUE);
	  }
	  weed_free(ctmpl);
	}
      }
      
      if (in_count!=NULL) weed_free(in_count);
      
      if (weed_plant_has_leaf(filter,"out_channel_templates")) {
	if ((num_channels=weed_leaf_num_elements(filter,"out_channel_templates"))>0) {
	  ctmpl=weed_get_plantptr_array(filter,"out_channel_templates",&weed_error);
	  for (i=0;i<num_channels;i++) {
	    if (!weed_plant_has_leaf(ctmpl[i],"disabled")||
		weed_get_boolean_value(ctmpl[i],"disabled",&weed_error)!=WEED_TRUE) 
	      weed_set_boolean_value(ctmpl[i],"disabled",WEED_FALSE);
	    else weed_set_boolean_value(ctmpl[i],"disabled",WEED_TRUE);
	  }
	  weed_free(ctmpl);
	}
      }

      weed_init_effect (key);

      // reinit effect with saved parameters
      inst=rte_keymode_get_instance(key+1,0);


      if (weed_plant_has_leaf(event,"host_key")) {
	// mt events will not have this;
	// it is used to connect params and alpha channels during rendering
	// holds our original key/mode values
	
	int hostkey=weed_get_int_value(event,"host_key",&weed_error);
	int hostmode=weed_get_int_value(event,"host_mode",&weed_error);
	
	weed_set_int_value(inst,"host_key",hostkey);
	weed_set_int_value(inst,"host_mode",hostmode);
	
      }

	// TODO *** - handle compound fx

      num_params=num_in_params(inst,TRUE,TRUE);
      if (num_params>0) {
	weed_call_deinit_func(inst);
	source_params=(weed_plant_t **)weed_get_voidptr_array(event,"in_parameters",&weed_error);
	in_params=weed_get_plantptr_array(inst,"in_parameters",&weed_error);
	for (i=0;i<num_params;i++) {
	  if (source_params[i]!=NULL&&is_init_pchange(event,source_params[i])) 
	    weed_leaf_copy(in_params[i],"value",source_params[i],"value");
	}
	
	if (weed_plant_has_leaf(filter,"init_func")) {
	  weed_init_f *init_func_ptr_ptr;
	  weed_init_f init_func;
	  weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
	  init_func=init_func_ptr_ptr[0];
	  set_param_gui_readwrite(inst);
	  update_host_info(inst);
	  if (init_func!=NULL) {
	    gchar *cwd=cd_to_plugin_dir(filter);
	    (*init_func)(inst);
	    lives_chdir(cwd,FALSE);
	    g_free(cwd);
	  }
	  set_param_gui_readonly(inst);
	}
	weed_set_boolean_value(inst,"host_inited",WEED_TRUE);
	if (weed_plant_has_leaf(event,"in_parameters")) {
	  pchains[key]=weed_get_voidptr_array(event,"in_parameters",&weed_error);
	}
	else pchains[key]=NULL;

	weed_free(source_params);
	weed_free(in_params);
      }
      else pchains[key]=NULL;
      break;
    case WEED_EVENT_HINT_FILTER_DEINIT:
      init_event=weed_get_voidptr_value (event,"init_event",&weed_error);

      filter_name=weed_get_string_value (init_event,"filter",&weed_error);
      // for now, assume we can find hashname
      idx=weed_get_idx_for_hashname (filter_name,TRUE);
      weed_free (filter_name);

      filter=get_weed_filter(idx);
      if (is_pure_audio(filter,FALSE)) break; // audio effects are processed in the audio renderer

      key_string=weed_get_string_value ((weed_plant_t *)init_event,"host_tag",&weed_error);
      key=atoi (key_string);
      weed_free (key_string);
      if (rte_keymode_get_instance(key+1,0)!=NULL) {
	weed_deinit_effect (key);
	weed_delete_effectkey (key+1,0);
      }
      if (pchains[key]!=NULL) weed_free(pchains[key]);
      pchains[key]=NULL;
      break;
    case WEED_EVENT_HINT_PARAM_CHANGE:
      break;
    case WEED_EVENT_HINT_FILTER_MAP:
#ifdef DEBUG_EVENTS
      g_print ("got new effect map\n");
#endif
      mainw->filter_map=event;
      break;
    }
    event=eventnext;
  }
  else {
    lives_mt *multi;

    if (cfile->old_frames==0) cfile->undo_start=cfile->undo_end=0;
    if (mainw->multitrack==NULL||!mainw->multitrack->pr_audio) {
      com=g_strdup_printf ("%s mv_mgk \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,cfile->undo_start,
			   cfile->undo_end,cfile->img_type==IMG_TYPE_JPEG?"jpg":"png");
      unlink(cfile->info_file);
      mainw->error=FALSE;
      mainw->com_failed=FALSE;
      mainw->cancelled=CANCEL_NONE;

      lives_system (com,FALSE);
      g_free (com);
      mainw->is_rendering=mainw->internal_messaging=FALSE;

      if (mainw->com_failed) {
	read_write_error=LIVES_RENDER_ERROR_WRITE_FRAME;
	//	cfile->may_be_damaged=TRUE;
      }
    }
    else g_snprintf(mainw->msg,512,"completed");

    multi=mainw->multitrack;
    mainw->multitrack=NULL;  // allow setting of audio filesize now
    reget_afilesize(mainw->current_file);
    mainw->multitrack=multi;
    mainw->filter_map=NULL;
    mainw->afilter_map=NULL;
    completed=TRUE;
  }

  g_free(nlabel);
  if (read_write_error) return read_write_error;
  if (completed) return LIVES_RENDER_COMPLETE;
  return LIVES_RENDER_PROCESSING;
}




gboolean start_render_effect_events (weed_plant_t *event_list) {
  // this is called to begin rendering effect events from an event_list into cfile
  // it will do a reorder/resample/resize/effect apply all in one pass

  // return FALSE in case of serious error

  gdouble old_pb_fps=cfile->pb_fps;
  gint oundo_start=cfile->undo_start;
  gint oundo_end=cfile->undo_end;

  if (event_list==NULL) return TRUE; //oh, that was easy !

  mainw->is_rendering=mainw->internal_messaging=TRUE;
  cfile->next_event=get_first_event(event_list);

  mainw->progress_fn=&render_events;
  mainw->progress_fn (TRUE);

  cfile->progress_start=1;
  cfile->progress_end=count_resampled_events (event_list, cfile->fps);

  cfile->pb_fps=1000000.;

  cfile->redoable=cfile->undoable=FALSE;
  gtk_widget_set_sensitive (mainw->redo, FALSE);
  gtk_widget_set_sensitive (mainw->undo, FALSE);

  cfile->undo_action=UNDO_RENDER;
  // play back the file as fast as possible, each time calling render_events()
  if ((!do_progress_dialog(TRUE,TRUE,"Rendering")&&mainw->cancelled!=CANCEL_KEEP)||mainw->error||
      mainw->render_error>=LIVES_RENDER_ERROR
      ) {
    mainw->cancel_type=CANCEL_KILL;
    mainw->cancelled=CANCEL_NONE;

    if (mainw->error) {
      do_error_dialog (mainw->msg);
      d_print_failed();
    }
    else if (mainw->render_error>=LIVES_RENDER_ERROR) d_print_failed();
    else d_print_cancelled();
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    cfile->pb_fps=old_pb_fps;
    cfile->frames=cfile->old_frames;
    mainw->internal_messaging=FALSE;
    mainw->resizing=FALSE;
    cfile->next_event=NULL;
    return FALSE;
  }

  mainw->cancel_type=CANCEL_KILL;
  mainw->cancelled=CANCEL_NONE;
  cfile->changed=TRUE;
  reget_afilesize(mainw->current_file);
  get_total_time(cfile);

  if (cfile->total_time==0) {
    d_print (_ ("nothing rendered.\n"));
    return FALSE;
  }

  gtk_widget_set_sensitive (mainw->undo, TRUE);
  cfile->undoable=TRUE;
  cfile->pb_fps=old_pb_fps;
  gtk_widget_set_sensitive (mainw->select_last, TRUE);
  set_undoable (_ ("rendering"),TRUE);
  cfile->next_event=NULL;
  return TRUE;
}



gint count_events (weed_plant_t *event_list, gboolean all_events, weed_timecode_t start_tc, weed_timecode_t end_tc) {
  weed_plant_t *event;
  weed_timecode_t tc;
  gint i=0;

  if (event_list==NULL) return 0;
  event=get_first_event(event_list);

  while (event!=NULL) {
    tc=get_event_timecode(event);
    if ((all_events||(get_event_hint(event)==WEED_EVENT_HINT_FRAME&&
		      !weed_plant_has_leaf(event,"audio_clips")))&&
	(end_tc==0||(tc>=start_tc&&tc<end_tc))) i++;
    event=get_next_event(event);
  }
  return i;
}



int count_resampled_events (weed_plant_t *event_list, gdouble fps) {
  weed_plant_t *event;
  weed_timecode_t tc,seg_start_tc=0,seg_end_tc=0;

  int rframes=0,hint,marker_type,error;
  
  gboolean seg_start=FALSE;

  if (event_list==NULL) return 0;
  event=get_first_event(event_list);

  while (event!=NULL) {
    hint=get_event_hint(event);
    if (hint==WEED_EVENT_HINT_FRAME) {
      tc=get_event_timecode(event);
      if (!seg_start) {
	seg_start_tc=seg_end_tc=tc;
	seg_start=TRUE;
      }
      else {
	seg_end_tc=tc;
      }
    }
    else {
      if (hint==WEED_EVENT_HINT_MARKER) {
	marker_type=weed_get_int_value(event,"lives_type",&error);
	if (marker_type==EVENT_MARKER_RECORD_END) {
	  // add (resampled) frames for one recording stretch
	  if (seg_start) rframes+=1+((gdouble)(seg_end_tc-seg_start_tc))/U_SEC*fps;
	  seg_start=FALSE;
	}
      }
    }
    event=get_next_event(event);
  }

  if (seg_start) rframes+=1+((gdouble)(seg_end_tc-seg_start_tc))/U_SEC*fps;

  return rframes;
}


weed_timecode_t event_list_get_end_tc (weed_plant_t *event_list) {
  if (event_list==NULL||get_last_event(event_list)==NULL) return 0.;
  return get_event_timecode (get_last_event(event_list));
}

gdouble event_list_get_end_secs (weed_plant_t *event_list) {
  return (event_list_get_end_tc(event_list)/U_SEC);
}


weed_timecode_t event_list_get_start_tc (weed_plant_t *event_list) {
  if (event_list==NULL||get_first_event(event_list)==NULL) return 0.;
  return get_event_timecode (get_first_event(event_list));
}

gdouble event_list_get_start_secs (weed_plant_t *event_list) {
  return (event_list_get_start_tc(event_list)/U_SEC);
}



gboolean has_audio_frame(weed_plant_t *event_list) {
  weed_plant_t *event=get_first_frame_event(event_list);
  while (event!=NULL) {
    if (weed_plant_has_leaf(event,"audio_clips")) return TRUE;
    event=get_next_frame_event(event);
  }
  return FALSE;
}


///////////////////////////////////////////////////////////////////


gboolean render_to_clip (gboolean new_clip) {
  // this function is called to actually start rendering mainw->event_list to a new/current clip
  gboolean retval=TRUE;
  gint current_file=mainw->current_file;
  gboolean response;
  gint xachans=0,xarate=0,xasamps=0,xse=0;
  gboolean rendaud=TRUE;
  gchar *com;

  if (mainw->multitrack!=NULL&&mainw->multitrack->pr_audio) d_print(_("Pre-rendering audio..."));
  else d_print(_("Rendering..."));

  if (new_clip) {

    if (prefs->render_prompt) {
      //set file details
      rdet=create_render_details(2);

      if (!has_audio_frame(mainw->event_list)) {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resaudw->aud_checkbutton), FALSE);
	gtk_widget_set_sensitive(resaudw->aud_checkbutton,FALSE);
      }
      rdet->enc_changed=FALSE;
      do {
	rdet->suggestion_followed=FALSE;
	if ((response=gtk_dialog_run(GTK_DIALOG(rdet->dialog)))==GTK_RESPONSE_OK) if (rdet->enc_changed) {
	    check_encoder_restrictions(FALSE,TRUE,TRUE);
	}
      } while (rdet->suggestion_followed);

      xarate=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_arate)));
      xachans=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_achans)));
      xasamps=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_asamps)));

      rendaud=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton));

      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
	xse=AFORM_UNSIGNED;;
      }
      else xse=AFORM_SIGNED;
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_bigend))) {
	xse|=AFORM_BIG_ENDIAN;
      }
      else xse|=AFORM_LITTLE_ENDIAN;

      gtk_widget_destroy (rdet->dialog);
      
      if (response==GTK_RESPONSE_CANCEL) {
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
	return FALSE;
      }

    }
  
    if (mainw->current_file>-1&&cfile!=NULL&&cfile->clip_type==CLIP_TYPE_GENERATOR) {
      weed_generator_end ((weed_plant_t *)cfile->ext_src);
    }

    // create new file
    mainw->current_file=mainw->first_free_file;
    
    if (!get_new_handle(mainw->current_file,NULL)) {
      // bummer...
      mainw->current_file=current_file;
      
      if (prefs->mt_enter_prompt) {
	g_free(rdet->encoder_name);
	g_free(rdet);
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
	rdet=NULL;
      }
      return FALSE; // show dialog again
    }
    
    if (prefs->render_prompt) {
      cfile->hsize=rdet->width;
      cfile->vsize=rdet->height;
      cfile->pb_fps=cfile->fps=rdet->fps;
      cfile->ratio_fps=rdet->ratio_fps;

      cfile->arps=cfile->arate=xarate;
      cfile->achans=xachans;
      cfile->asampsize=xasamps;
      cfile->signed_endian=xse;

      g_free(rdet->encoder_name);
      g_free(rdet);
      rdet=NULL;
      if (resaudw!=NULL) g_free(resaudw);
      resaudw=NULL;
    }
    else {
      cfile->hsize=prefs->mt_def_width;
      cfile->vsize=prefs->mt_def_height;
      cfile->pb_fps=cfile->fps=prefs->mt_def_fps;
      cfile->ratio_fps=FALSE;
      cfile->arate=cfile->arps=prefs->mt_def_arate;
      cfile->achans=prefs->mt_def_achans;
      cfile->asampsize=prefs->mt_def_asamps;
      cfile->signed_endian=prefs->mt_def_signed_endian;
    }
    
    cfile->bpp=cfile->img_type==IMG_TYPE_JPEG?24:32;
    cfile->is_loaded=TRUE;
  }
  else if (mainw->multitrack==NULL) {
    // back up audio to audio.back (in case we overwrite it)
    if (prefs->rec_opts&REC_AUDIO) {
      do_threaded_dialog(_("Backing up audio..."),FALSE);
      com=g_strdup_printf("%s backup_audio \"%s\"",prefs->backend_sync,cfile->handle);
      mainw->com_failed=FALSE;
      mainw->error=FALSE;
      mainw->cancelled=CANCEL_NONE;
      unlink (cfile->info_file);
      lives_system(com,FALSE);
      g_free(com);
      if (mainw->com_failed) return FALSE;

    }
    else {
      do_threaded_dialog(_("Clearing up clip..."),FALSE);
      com=g_strdup_printf("%s clear_tmp_files \"%s\"",prefs->backend_sync,cfile->handle);
      lives_system(com,FALSE);
      g_free(com);
    }
    end_threaded_dialog();
  }

  cfile->old_frames=cfile->frames;
  cfile->changed=TRUE;
  mainw->effects_paused=FALSE;
  prefs->render_audio=rendaud;

  if (start_render_effect_events(mainw->event_list)) { // re-render, applying effects 
    // and reordering/resampling/resizing if necessary

    if (mainw->multitrack==NULL&&mainw->event_list!=NULL) {
      if (!new_clip) {
	// this is needed in case we render to same clip, and then undo ///////
	if (cfile->event_list_back!=NULL) event_list_free (cfile->event_list_back);
	cfile->event_list_back=mainw->event_list;
	///////////////////////////////////////////////////////////////////////
      }
      else event_list_free(mainw->event_list);
    }
    mainw->event_list=NULL;
    if (new_clip) {
      gchar *tmp;
      gint old_file=current_file;
      cfile->start=1;
      cfile->end=cfile->frames;
      set_undoable (NULL,FALSE);
      add_to_winmenu();
      current_file=mainw->current_file;
      if (!save_clip_values(current_file)) {
	close_current_file(old_file);
	mainw->effects_paused=FALSE;
	deinit_render_effects();
	audio_free_fnames();
	return FALSE;
      }
      if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
      switch_to_file ((mainw->current_file=0),current_file);
      d_print ((tmp=g_strdup_printf (_ ("rendered %d frames to new clip.\n"),cfile->frames)));
      g_free(tmp);
      mainw->pre_src_file=mainw->current_file; // if a generator started playback, we will switch back to this file after
#ifdef ENABLE_OSC
      lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif
    }
    else {
      // rendered to same clip - update number of frames
      save_clip_value(mainw->current_file,CLIP_DETAILS_FRAMES,&cfile->frames);
      if (mainw->com_failed||mainw->write_failed) do_header_write_error(mainw->current_file);
    }

    if (cfile->clip_type==CLIP_TYPE_FILE) {
      if (cfile->frame_index_back!=NULL) g_free(cfile->frame_index_back);
      cfile->frame_index_back=cfile->frame_index;
      cfile->frame_index=NULL;

      if (cfile->undo_start==1&&cfile->undo_end==cfile->frames) {
	cfile->clip_type=CLIP_TYPE_DISK;
	del_frame_index(cfile);
      }
      else {
	register int i;
	create_frame_index(mainw->current_file,FALSE,0,cfile->frames);
	for (i=0;i<cfile->undo_start-1;i++) {
	  cfile->frame_index[i]=cfile->frame_index_back[i];
	}
	for (i=cfile->undo_start-1;i<cfile->undo_end;i++) {
	  cfile->frame_index[i]=-1;
	}
	for (i=cfile->undo_end;i<cfile->frames;i++) {
	  cfile->frame_index[i]=cfile->frame_index_back[i];
	}
	save_frame_index(mainw->current_file);
      }
    }

  }
  else {
    retval=FALSE; // cancelled or error, so show the dialog again
    if (new_clip&&mainw->multitrack==NULL) {
      close_current_file(current_file);
    }
  }

  mainw->effects_paused=FALSE;
  deinit_render_effects();
  audio_free_fnames();
  return retval;
}



LIVES_INLINE void dprint_recneg(void) {
  d_print (_ ("nothing recorded.\n"));
}


gboolean deal_with_render_choice (gboolean add_deinit) {
  // this is called from saveplay.c after record/playback ends
  // here we deal with the user's wishes as to how to deal with the recorded events

  // mainw->osc_block should be TRUE during all of this, so we don't have to contend with
  // any incoming network messages

  // return TRUE if we rendered to a new clip

  GtkWidget *e_rec_dialog;
  GtkWidget *elist_dialog;

  gint dh,dw,dar,das,dac,dse;
  gint oplay_start;
  gdouble df;

  gboolean new_clip=FALSE;
  gboolean was_paused=mainw->record_paused;

  // record end
  mainw->record=FALSE;
  mainw->record_paused=FALSE;

  g_signal_handler_block(mainw->record_perf,mainw->record_perf_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->record_perf),FALSE);
  g_signal_handler_unblock(mainw->record_perf,mainw->record_perf_func);

  if (count_events(mainw->event_list,FALSE,0,0)==0) {
    event_list_free(mainw->event_list);
    mainw->event_list=NULL;
  }
  
  if (mainw->event_list==NULL) {
    dprint_recneg();
    return FALSE;
  }

  // add deinit events for any effects that are left on
  if (add_deinit&&!was_paused) mainw->event_list=add_filter_deinit_events (mainw->event_list);

  if (add_deinit) event_list_close_gaps (mainw->event_list);

  // need to retain play_start for rendering to same clip
  oplay_start=mainw->play_start;

  do {
    e_rec_dialog=events_rec_dialog(!was_paused);
    gtk_widget_show (e_rec_dialog);
    gtk_dialog_run (GTK_DIALOG (e_rec_dialog));
    gtk_widget_destroy (e_rec_dialog);
    while (g_main_context_iteration (NULL,FALSE));
    switch (render_choice) {
    case RENDER_CHOICE_DISCARD:
      if (mainw->current_file>-1) cfile->redoable=FALSE;
      close_scrap_file();
      close_ascrap_file();
      sensitize();
      break;
    case RENDER_CHOICE_PREVIEW:
      // preview
      cfile->next_event=get_first_event(mainw->event_list);
      mainw->is_rendering=TRUE;
      on_preview_clicked (NULL,NULL);
      deinit_render_effects();
      mainw->is_processing=mainw->is_rendering=FALSE;
      cfile->next_event=NULL;
      break;
    case RENDER_CHOICE_NEW_CLIP:
      dw=prefs->mt_def_width;
      dh=prefs->mt_def_height;
      df=prefs->mt_def_fps;
      dar=prefs->mt_def_arate;
      dac=prefs->mt_def_achans;
      das=prefs->mt_def_asamps;
      dse=prefs->mt_def_signed_endian;
      if (!mainw->clip_switched&&prefs->render_prompt&&mainw->current_file>-1) {
	if (cfile->hsize>0) prefs->mt_def_width=cfile->hsize;
	if (cfile->vsize>0) prefs->mt_def_height=cfile->vsize;
	prefs->mt_def_fps=cfile->fps;
	if (cfile->achans*cfile->arate*cfile->asampsize>0) {
	  prefs->mt_def_arate=cfile->arate;
	  prefs->mt_def_asamps=cfile->asampsize;
	  prefs->mt_def_achans=cfile->achans;
	  prefs->mt_def_signed_endian=cfile->signed_endian;
	}
      }
      mainw->play_start=1;   ///< new clip frames always start  at 1
      if (!render_to_clip (TRUE)) render_choice=RENDER_CHOICE_PREVIEW;
      else {
	close_scrap_file();
	close_ascrap_file();
      }
      prefs->mt_def_width=dw;
      prefs->mt_def_height=dh;
      prefs->mt_def_fps=df;
      prefs->mt_def_arate=dar;
      prefs->mt_def_achans=dac;
      prefs->mt_def_asamps=das;
      prefs->mt_def_signed_endian=dse;
      mainw->is_rendering=FALSE;
      new_clip=TRUE;
      break;
    case RENDER_CHOICE_SAME_CLIP:
      mainw->play_start=oplay_start;  ///< same clip frames start where recording started
      if (!render_to_clip (FALSE)) render_choice=RENDER_CHOICE_PREVIEW;
      else {
	close_scrap_file();
	close_ascrap_file();
	d_print_done();
      }
      mainw->is_rendering=FALSE;
      break;
    case RENDER_CHOICE_MULTITRACK:
      if (mainw->stored_event_list!=NULL&&mainw->stored_event_list_changed) {
	if (!check_for_layout_del(NULL,FALSE)) {
	  render_choice=RENDER_CHOICE_PREVIEW;
	  break;
	}
      }
      if (mainw->stored_event_list!=NULL||mainw->sl_undo_mem!=NULL) {
	recover_layout_cancelled(NULL,NULL);
	stored_event_list_free_all(TRUE);
      }
      mainw->unordered_blocks=TRUE;
      if (on_multitrack_activate (NULL, (weed_plant_t *)mainw->event_list)) {
	mainw->event_list=NULL;
	new_clip=TRUE;
      }
      else render_choice=RENDER_CHOICE_PREVIEW;
     break;
    case RENDER_CHOICE_EVENT_LIST:
      if (count_events(mainw->event_list,prefs->event_window_show_frame_events,0,0)>1000) if (!do_event_list_warning()) {
	render_choice=RENDER_CHOICE_PREVIEW;
	break;
      }
      elist_dialog=create_event_list_dialog(mainw->event_list,0,0);
      gtk_dialog_run(GTK_DIALOG(elist_dialog));
      gtk_widget_destroy(elist_dialog);
      render_choice=RENDER_CHOICE_PREVIEW;
      break;
    }
    if 
      (mainw->current_file>0&&mainw->files[mainw->current_file]!=NULL) {
      cfile->next_event=NULL;
    }
  } while (render_choice==RENDER_CHOICE_PREVIEW);


  if (mainw->event_list!=NULL) {
    event_list_free(mainw->event_list);
    mainw->event_list=NULL;
  }

  return new_clip;
}



gdouble *get_track_visibility_at_tc(weed_plant_t *event_list, gint ntracks, gint nbtracks, 
				    weed_timecode_t tc, weed_plant_t **shortcut, gboolean bleedthru) {
  // calculate the "visibility" of each track at timecode tc
  // that is to say, only the front track is visible, except if we have a transition and "host_audio_transition" is set
  // - in which case the track visibilty is proportional to the transition parameter

  // to do this, we need a filter map and a frame/clip stack

  // if bleedthru is TRUE, all values are set to 1.0

  static weed_plant_t *stored_fmap;
  weed_plant_t *frame_event,*fmap;
  gdouble *vis;
  gint nxtracks;
  gint got=-1;

  int *clips=NULL,*frames=NULL;

  int error;
  register int i,j;

  double *matrix[ntracks+nbtracks];

  ntracks+=nbtracks;

  if (shortcut==NULL||*shortcut==NULL) stored_fmap=NULL;

  if (shortcut!=NULL) *shortcut=frame_event=get_frame_event_at_or_before(event_list,tc,*shortcut);
  else frame_event=get_frame_event_at_or_before(event_list,tc,NULL);

  nxtracks=weed_leaf_num_elements(frame_event,"clips");

  vis=(gdouble *)g_malloc(ntracks*sizeof(gdouble));

  if (bleedthru) {
    for (i=0;i<ntracks;i++) {
      vis[i]=1.;
    }
    return vis;
  }

  clips=weed_get_int_array(frame_event,"clips",&error);
  frames=weed_get_int_array(frame_event,"frames",&error);

  if (nbtracks>0) vis[0]=1.;

  if (stored_fmap==NULL) stored_fmap=fmap=get_filter_map_before(frame_event,-1000000,NULL);
  else {
    fmap=get_filter_map_before(frame_event,-1000000,*shortcut);
    if (fmap==*shortcut) fmap=stored_fmap;
  }

  for (i=0;i<ntracks;i++) {
    matrix[i]=(double *)g_malloc(ntracks*sizeof(double));
    for (j=0;j<ntracks;j++) {
      matrix[i][j]=0.;
    }
    matrix[i][i]=1.;
  }


  if (fmap!=NULL) {
    // here we look at all init_events in fmap. If any have "host_audio_transition" set, then
    // we we look at the 2 in channels. We first multiply matrix[t0][...] by trans
    // then we add matrix[t1][...]*(1.0 - trans) to matrix[t3][...]
    // where trans is the normalised value of the transition parameter
    // t3 is the output channel, this is usually the same track as t0
    // thus each row in the matrix represents the contribution from each layer (track)
    if (weed_plant_has_leaf(fmap,"init_events")) {
      weed_plant_t **iev=(weed_plant_t **)weed_get_voidptr_array(fmap,"init_events",&error);
      gint nins=weed_leaf_num_elements(fmap,"init_events");
      for (i=0;i<nins;i++) {
	weed_plant_t *ievent=iev[i];
	if (weed_plant_has_leaf(ievent,"host_audio_transition")&&
	    weed_get_boolean_value(ievent,"host_audio_transition",&error)==WEED_TRUE) {
	  int *in_tracks=weed_get_int_array(ievent,"in_tracks",&error);
	  int *out_tracks=weed_get_int_array(ievent,"out_tracks",&error);
	  gchar *filter_hash=weed_get_string_value(ievent,"filter",&error);
	  int idx;
	  if ((idx=weed_get_idx_for_hashname(filter_hash,TRUE))!=-1) {
	    weed_plant_t *filter=get_weed_filter(idx);
	    int tparam=get_transition_param(filter);
	    weed_plant_t *inst=weed_instance_from_filter(filter);
	    weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
	    weed_plant_t *ttmpl=weed_get_plantptr_value(in_params[tparam],"template",&error);
	    double trans;
	    void **pchains=weed_get_voidptr_array(ievent,"in_parameters",&error);

	    interpolate_param(inst,tparam,pchains[tparam],tc);

	    if (weed_leaf_seed_type(in_params[tparam],"value")==WEED_SEED_DOUBLE) {
	      double transd=weed_get_double_value(in_params[tparam],"value",&error);
	      double tmin=weed_get_double_value(ttmpl,"min",&error);
	      double tmax=weed_get_double_value(ttmpl,"max",&error);
	      trans=(transd-tmin)/(tmax-tmin);
	    }
	    else {
	      int transi=weed_get_int_value(in_params[tparam],"value",&error);
	      int tmin=weed_get_int_value(ttmpl,"min",&error);
	      int tmax=weed_get_int_value(ttmpl,"max",&error);
	      trans=(double)(transi-tmin)/(double)(tmax-tmin);
	    }
	    weed_free(in_params);
	    for (j=0;j<ntracks;j++) {
	      matrix[in_tracks[1]+nbtracks][j]*=trans;
	      matrix[in_tracks[0]+nbtracks][j]*=(1.-trans);
	      matrix[out_tracks[0]+nbtracks][j]=matrix[in_tracks[0]+nbtracks][j]+matrix[in_tracks[1]+nbtracks][j];
	    }
	    weed_free(pchains);
	    weed_plant_free(inst);
	  }
	  weed_free(in_tracks);
	  weed_free(out_tracks);
	  weed_free(filter_hash);
	}
      }
      weed_free(iev);
    }
  }
  

  // now we select as visibility, whichever row is the first layer to have a non-blank frame

  for (i=0;i<nxtracks;i++) {
    if (clips[i]>=0&&frames[i]>0) {
      got=i+nbtracks;
      break;
    }
  }

  weed_free(clips);
  weed_free(frames);

  if (got==-1) {
    // all frames blank - backing audio only
    for (i=0;i<ntracks;i++) {
      if (i>=nbtracks) vis[i]=0.;
      g_free(matrix[i]);
    }
    return vis;
  }

  for (i=nbtracks;i<ntracks;i++) {
    vis[i]=matrix[got][i];
  }

  for (i=0;i<ntracks;i++) {
    g_free(matrix[i]);
  }

  return vis;
}





//////////////////////
//GUI stuff



enum {
  TITLE_COLUMN,
  KEY_COLUMN,
  VALUE_COLUMN,
  NUM_COLUMNS
};



GtkWidget *create_event_list_dialog (weed_plant_t *event_list, weed_timecode_t start_tc, weed_timecode_t end_tc) {
  // TODO - some event properties should be editable, e.g. parameter values

 GtkTreeStore *gtkstore;
 GtkTreeIter iter1,iter2,iter3;

 gchar **string=NULL;
 int *intval=NULL;
 void **voidval=NULL;
 double *doubval=NULL;
 gint64 *int64val=NULL;
 weed_plant_t *event;

 GtkWidget *event_dialog;
 GtkWidget *tree;
 GtkWidget *table;
 GtkWidget *top_vbox;
 GtkWidget *label;
 GtkWidget *ok_button;
 GtkWidget *hbuttonbox;
 GtkWidget *scrolledwindow;
 GtkCellRenderer *renderer;
 GtkTreeViewColumn *column;

 gint winsize_h,scr_width=mainw->scr_width;
 gint winsize_v,scr_height=mainw->scr_height;

 int i,j,num_elems,seed_type,hint,error;
 weed_timecode_t tc,tc_secs;
 gchar *strval=NULL;
 gchar *text;

 char **propnames;
 gchar *oldval=NULL,*final=NULL;

 gint rows,currow=0;

 if (prefs->event_window_show_frame_events) rows=count_events(event_list,TRUE,start_tc,end_tc);
 else rows=count_events(event_list,TRUE,start_tc,end_tc)-count_events(event_list,FALSE,start_tc,end_tc);

 event=get_first_event(event_list);

 if (prefs->gui_monitor!=0) {
   scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
   scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
 }

 winsize_h=scr_width-100;
 winsize_v=scr_height-100;
 
 event_dialog = gtk_dialog_new ();

 gtk_widget_modify_bg(event_dialog, GTK_STATE_NORMAL, &palette->menu_and_bars);
 gtk_window_set_title (GTK_WINDOW (event_dialog), _("LiVES: Event list"));
 gtk_window_add_accel_group (GTK_WINDOW (event_dialog), mainw->accel_group);
 
 top_vbox=GTK_DIALOG(event_dialog)->vbox;

 table = gtk_table_new (rows, 6, TRUE);
 gtk_widget_show (table);

 while (event!=NULL) {
   tc=get_event_timecode (event);

   if (end_tc>0) {
     if (tc<start_tc) {
       event=get_next_event(event);
       continue;
     }
     if (tc>=end_tc) break;
   }

   if ((prefs->event_window_show_frame_events||!WEED_EVENT_IS_FRAME(event))||weed_plant_has_leaf(event,"audio_clips")) {
     if (!prefs->event_window_show_frame_events&&WEED_EVENT_IS_FRAME(event)) {
       // TODO - opts should be all frames, only audio frames, no frames
       // or even better, filter for any event types
       rows++;
       gtk_table_resize(GTK_TABLE(table),rows,6);
     }
			
     gtkstore = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

     gtk_tree_store_append (gtkstore, &iter1, NULL);  /* Acquire an iterator */
     gtk_tree_store_set (gtkstore, &iter1, TITLE_COLUMN, "Properties", -1);

     propnames=weed_plant_list_leaves (event);
   
     for (i=0;propnames[i]!=NULL;i++) {
       if (!strcmp(propnames[i],"type")||!strcmp(propnames[i],"hint")||!strcmp(propnames[i],"timecode")) {
	 weed_free(propnames[i]);
	 continue;
       }
       gtk_tree_store_append (gtkstore, &iter2, &iter1);  /* Acquire a child iterator */
       
       if (oldval!=NULL) {
	 g_free(oldval);
	 oldval=NULL;
       }
       
       if (final!=NULL) {
	 g_free(final);
	 final=NULL;
       }
       
       num_elems=weed_leaf_num_elements (event,propnames[i]);
       seed_type=weed_leaf_seed_type (event,propnames[i]);
       
       switch (seed_type) {
       case WEED_SEED_INT:
	 intval=weed_get_int_array (event,propnames[i],&error);
	 break;
       case WEED_SEED_INT64:
	 int64val=weed_get_int64_array (event,propnames[i],&error);
	 break;
       case WEED_SEED_BOOLEAN:
	 intval=weed_get_boolean_array (event,propnames[i],&error);
	 break;
       case WEED_SEED_STRING:
	 string=weed_get_string_array (event,propnames[i],&error);
	 break;
       case WEED_SEED_DOUBLE:
	 doubval=weed_get_double_array (event,propnames[i],&error);
	 break;
       case WEED_SEED_VOIDPTR:
	 voidval=weed_get_voidptr_array (event,propnames[i],&error);
	 break;
       case WEED_SEED_PLANTPTR:
	 voidval=(void **)weed_get_plantptr_array (event,propnames[i],&error);
	 break;
       }
       
       
       for (j=0;j<num_elems;j++) {
	 switch (seed_type) {
	 case WEED_SEED_INT:
	   strval=g_strdup_printf("%d",intval[j]);
	   break;
	 case WEED_SEED_INT64:
	   strval=g_strdup_printf("%"PRId64,int64val[j]);
	   break;
	 case WEED_SEED_DOUBLE:
	   strval=g_strdup_printf("%.4f",doubval[j]);
	   break;
	 case WEED_SEED_BOOLEAN:
	   if (intval[j]==WEED_TRUE) strval=g_strdup(_("TRUE"));
	   else strval=g_strdup(_("FALSE"));
	   break;
	 case WEED_SEED_STRING:
	   strval=g_strdup(string[j]);
	   weed_free(string[j]);
	   break;
	 case WEED_SEED_VOIDPTR:
	   if (!(strncmp(propnames[i],"init_event",10))) {
	     weed_plant_t *ievent=(weed_plant_t *)voidval[j];
	     if (ievent!=NULL) {
	       gchar *iname=weed_get_string_value(ievent,"filter",&error);
	       if (iname!=NULL) {
		 gchar *fname=weed_filter_get_name(weed_get_idx_for_hashname(iname,TRUE));
		 strval=g_strdup_printf("%p (%s)",voidval[j],fname);
		 weed_free(fname);
	       }
	       weed_free(iname);
	     }
	   }
	   if (strval==NULL) strval=g_strdup_printf("%p",voidval[j]);
	   break;
	 case WEED_SEED_PLANTPTR:
	   strval=g_strdup_printf("-->%p",voidval[j]);
	   break;
	 default:
	   strval=g_strdup("???");
	   break;
	 }
	 if (j==0) {
	   if (num_elems==1) {
	     gtk_tree_store_set (gtkstore, &iter2, KEY_COLUMN, propnames[i], VALUE_COLUMN, strval, -1);
	   }
	   else {
	     gtk_tree_store_set (gtkstore, &iter2, KEY_COLUMN, propnames[i], VALUE_COLUMN, "", -1);
	     gtk_tree_store_append (gtkstore, &iter3, &iter2);
	     gtk_tree_store_set (gtkstore, &iter3, VALUE_COLUMN, strval, -1);
	   }
       }
	 else {
	   gtk_tree_store_append (gtkstore, &iter3, &iter2);
	   gtk_tree_store_set (gtkstore, &iter3, VALUE_COLUMN, strval, -1);
	 }
	 if (strval!=NULL) g_free (strval);
	 strval=NULL;
       }

       switch (seed_type) {
       case WEED_SEED_INT:
       case WEED_SEED_BOOLEAN:
	 weed_free(intval);
	 break;
       case WEED_SEED_INT64:
	 weed_free(int64val);
	 break;
       case WEED_SEED_DOUBLE:
	 weed_free(doubval);
	 break;
       case WEED_SEED_STRING:
	 weed_free(string);
	 break;
       case WEED_SEED_VOIDPTR:
       case WEED_SEED_PLANTPTR:
	 weed_free(voidval);
	 break;
       default:
	 break;
       }
       weed_free(propnames[i]);
     }

     weed_free (propnames);

     // now add the new treeview
     
     g_free(final);

     // timecode
     tc_secs=tc/U_SECL;
     tc-=tc_secs*U_SECL;
     text=g_strdup_printf(_("Timecode=%"PRId64".%"PRId64),tc_secs,tc);
     label = gtk_label_new (text);
     g_free(text);
     
     gtk_widget_show (label);

     if (palette->style&STYLE_1) {
       gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
     }

     
     gtk_table_attach (GTK_TABLE (table), label, 0, 1, currow, currow+1,
		       (GtkAttachOptions) (GTK_EXPAND),
		       (GtkAttachOptions) (0), 0, 0);
     
     // event type
     hint=get_event_hint (event);
     switch (hint) {
     case WEED_EVENT_HINT_FRAME:
       label = gtk_label_new ("Frame");
       break;
     case WEED_EVENT_HINT_FILTER_INIT:
       label = gtk_label_new ("Filter on");
       break;
     case WEED_EVENT_HINT_FILTER_DEINIT:
       label = gtk_label_new ("Filter off");
       break;
     case WEED_EVENT_HINT_PARAM_CHANGE:
       label = gtk_label_new ("Parameter change");
       break;
     case WEED_EVENT_HINT_FILTER_MAP:
       label = gtk_label_new ("Filter map");
       break;
     case WEED_EVENT_HINT_MARKER:
       label = gtk_label_new ("Marker");
       break;
     default:
       text=g_strdup_printf("unknown event hint %d",hint);
       label = gtk_label_new (text);
       g_free(text);
     }
     
     if (palette->style&STYLE_1) {
       gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
     }

     gtk_widget_show (label);
     gtk_table_attach (GTK_TABLE (table), label, 1, 2, currow, currow+1,
		     (GtkAttachOptions) (GTK_EXPAND),
		     (GtkAttachOptions) (0), 0, 0);
     
     // event id
     text=g_strdup_printf(("Event id=%p"),(void *)event);
     label = gtk_label_new (text);
     g_free(text);
     gtk_widget_show (label);
     if (palette->style&STYLE_1) {
       gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
     }
     gtk_table_attach (GTK_TABLE (table), label, 2, 3, currow, currow+1,
		       (GtkAttachOptions) (GTK_EXPAND),
		       (GtkAttachOptions) (0), 0, 0);
     
     // properties
     tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (gtkstore));
     gtk_widget_modify_base(tree, GTK_STATE_NORMAL, &palette->menu_and_bars);
     
     renderer = gtk_cell_renderer_text_new ();
     column = gtk_tree_view_column_new_with_attributes (NULL,
							renderer,
							"text", TITLE_COLUMN,
							NULL);

     gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
     
     renderer = gtk_cell_renderer_text_new ();
     column = gtk_tree_view_column_new_with_attributes ("Keys",
							renderer,
							"text", KEY_COLUMN,
							NULL);
     gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
     
     
     renderer = gtk_cell_renderer_text_new ();
     column = gtk_tree_view_column_new_with_attributes ("Values",
							renderer,
							"text", VALUE_COLUMN,
							NULL);
     gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
     gtk_widget_show (tree);
     
     gtk_table_attach (GTK_TABLE (table), tree, 3, 6, currow, currow+1,
		       (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		       (GtkAttachOptions) (0), 0, 0);
     
     currow++;
   }
   event=get_next_event(event);
 }

 scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
 gtk_widget_show (scrolledwindow);
 gtk_box_pack_start (GTK_BOX (top_vbox), scrolledwindow, TRUE, TRUE, 0);

 gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
 gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), table);
 gtk_widget_set_size_request (scrolledwindow, winsize_h, winsize_v);
 
 if (palette->style&STYLE_1) {
   gtk_widget_modify_bg(gtk_bin_get_child (GTK_BIN (scrolledwindow)), GTK_STATE_NORMAL, &palette->normal_back);
 }
 
 gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolledwindow))),GTK_SHADOW_IN);

 hbuttonbox = gtk_hbutton_box_new ();
 gtk_widget_show (hbuttonbox);

 gtk_box_pack_start (GTK_BOX (top_vbox), hbuttonbox, TRUE, TRUE, 0);

 gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox), DEF_BUTTON_WIDTH, -1);
 gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_SPREAD);

 ok_button = gtk_button_new_with_mnemonic (_("Close _window"));
 gtk_widget_show (ok_button);
 gtk_container_add (GTK_CONTAINER (hbuttonbox), ok_button);

 GTK_WIDGET_SET_FLAGS (ok_button, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
 gtk_widget_grab_default (ok_button);
 
 g_signal_connect (GTK_OBJECT (ok_button), "clicked",
		   G_CALLBACK (response_ok),
		   NULL);

 if (prefs->gui_monitor!=0) {
   gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+
     (mainw->mgeom[prefs->gui_monitor-1].width-event_dialog->allocation.width)/2;
   gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+
     (mainw->mgeom[prefs->gui_monitor-1].height-event_dialog->allocation.height)/2;
   gtk_window_set_screen(GTK_WINDOW(event_dialog),mainw->mgeom[prefs->gui_monitor-1].screen);
   gtk_window_move(GTK_WINDOW(event_dialog),xcen,ycen);
 }

  if (prefs->open_maximised) {
    gtk_window_maximize (GTK_WINDOW(event_dialog));
  }

 return event_dialog;
}



void rdetw_spinh_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  render_details *rdet=(render_details *)user_data;
  rdet->height=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
}


void rdetw_spinw_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  render_details *rdet=(render_details *)user_data;
  rdet->width=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
}


void rdetw_spinf_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  render_details *rdet=(render_details *)user_data;
  rdet->fps=gtk_spin_button_get_value(GTK_SPIN_BUTTON(spinbutton));
}



render_details *create_render_details (gint type) {

  // type == 1 :: pre-save (specified)
  // type == 2 :: render to new clip (!specified)
  // type == 3 :: enter multitrack (!specified)
  // type == 4 :: change during multitrack (!specified)

  GtkWidget *label;
  GtkWidget *top_vbox;
  GtkWidget *dialog_vbox;
  GtkWidget *scrollw;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *cancelbutton;
  GtkWidget *alabel;
  GtkWidget *hsep;

  GObject *spinbutton_adj;

  GtkAccelGroup *rdet_accel_group;

  GList *ofmt_all=NULL;
  GList *ofmt=NULL;
  GList *encoders=NULL;

  gboolean specified=FALSE;
  gboolean needs_new_encoder=FALSE;

  int i;
  int scrw,scrh;

  gchar **array;

  gchar *tmp,*tmp2;

  if (type==1) specified=TRUE;

  rdet=(render_details *)g_malloc(sizeof(render_details));

  rdet->is_encoding=FALSE;

  if (!specified&&type!=4) {
    rdet->height=prefs->mt_def_height;
    rdet->width=prefs->mt_def_width;
    rdet->fps=prefs->mt_def_fps;
    rdet->ratio_fps=FALSE;

    rdet->arate=prefs->mt_def_arate;
    rdet->achans=prefs->mt_def_achans;
    rdet->asamps=prefs->mt_def_asamps;
    rdet->aendian=prefs->mt_def_signed_endian;
  }
  else {
    rdet->height=cfile->vsize;
    rdet->width=cfile->hsize;
    rdet->fps=cfile->fps;
    rdet->ratio_fps=cfile->ratio_fps;

    rdet->arate=cfile->arate;
    rdet->achans=cfile->achans;
    rdet->asamps=cfile->asampsize;
    rdet->aendian=cfile->signed_endian;
  }

  rdet->enc_changed=FALSE;
  rdet->dialog = gtk_dialog_new ();
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(rdet->dialog, GTK_STATE_NORMAL, &palette->normal_back);
    if (type==1) gtk_dialog_set_has_separator(GTK_DIALOG(rdet->dialog),FALSE);
  }

  if (type==3||type==4) gtk_window_set_title (GTK_WINDOW (rdet->dialog), _("LiVES: Multitrack details"));
  else if (type==1) gtk_window_set_title (GTK_WINDOW (rdet->dialog), _("LiVES: Encoding details"));
  else gtk_window_set_title (GTK_WINDOW (rdet->dialog), _("LiVES: New clip details"));

  gtk_container_set_border_width (GTK_CONTAINER (rdet->dialog), 10);
  gtk_window_set_default_size (GTK_WINDOW (rdet->dialog), 300, 200);
  if (prefs->show_gui&&mainw->is_ready) gtk_window_set_transient_for(GTK_WINDOW(rdet->dialog),GTK_WINDOW(mainw->LiVES));
  gtk_window_set_modal (GTK_WINDOW (rdet->dialog), TRUE);
  rdet_accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (rdet->dialog), rdet_accel_group);

  dialog_vbox = GTK_DIALOG(rdet->dialog)->vbox;


  scrollw = gtk_scrolled_window_new (NULL, NULL);
   
  if (prefs->gui_monitor!=0) {
    scrw=mainw->mgeom[prefs->gui_monitor-1].width;
    scrh=mainw->mgeom[prefs->gui_monitor-1].height;
  }
  else {
    scrw=mainw->scr_width;
    scrh=mainw->scr_height;
  }

  gtk_widget_set_size_request (scrollw, scrw*.8, scrh*.75);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollw), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  top_vbox = gtk_vbox_new (FALSE, 0);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrollw), top_vbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), scrollw, TRUE, TRUE, 0);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(scrollw)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(scrollw)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_widget_show (top_vbox);
  gtk_widget_show (scrollw);
  gtk_container_set_border_width (GTK_CONTAINER (top_vbox), 0);

  frame = gtk_frame_new (NULL);
  gtk_widget_show (frame);

  if (type!=1) gtk_box_pack_start (GTK_BOX (top_vbox), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg (frame, GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  label = gtk_label_new (_("Video"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  hbox = gtk_hbox_new (FALSE, 50);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
  
  label = gtk_label_new_with_mnemonic (_ ("_Width"));
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  spinbutton_adj = (GObject *)gtk_adjustment_new (rdet->width, 2., 100000., 1, 16, 0);
  
  rdet->spinbutton_width = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (rdet->spinbutton_width),TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (rdet->spinbutton_width), TRUE);
  
  g_signal_connect_after (GTK_OBJECT (rdet->spinbutton_width), "value_changed",
			  G_CALLBACK (rdetw_spinw_changed),
			  rdet);
  
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), rdet->spinbutton_width, FALSE, FALSE, 10);
  
  label = gtk_label_new_with_mnemonic (_ ("_Height"));
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  spinbutton_adj = (GObject *)gtk_adjustment_new (rdet->height, 2., 10000., 1, 16, 0);
  
  rdet->spinbutton_height = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (rdet->spinbutton_height),TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (rdet->spinbutton_height), TRUE);
  
  g_signal_connect_after (GTK_OBJECT (rdet->spinbutton_height), "value_changed",
			  G_CALLBACK (rdetw_spinh_changed),
			  rdet);
  

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), rdet->spinbutton_height, FALSE, FALSE, 10);

  // add aspect button
  if (type==1) {
    lives_param_t param;
    // add "aspectratio" widget
    init_special();
    add_to_special("aspect|-100|-101|",NULL);
    param.widgets[0]=rdet->spinbutton_width;
    check_for_special (&param,-100,GTK_BOX(vbox),NULL);
    param.widgets[0]=rdet->spinbutton_height;
    check_for_special (&param,-101,GTK_BOX(vbox),NULL);
  }


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
  
  label = gtk_label_new_with_mnemonic (_ ("_Frames per second"));
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  spinbutton_adj = (GObject *)gtk_adjustment_new (rdet->fps, 1., FPS_MAX, 1, 10, 0);
  
  rdet->spinbutton_fps = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 3);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (rdet->spinbutton_fps),TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (rdet->spinbutton_fps), TRUE);
  
  if (type==4&&mainw->multitrack->event_list!=NULL) gtk_widget_set_sensitive(rdet->spinbutton_fps,FALSE);
  
  g_signal_connect_after (GTK_OBJECT (rdet->spinbutton_fps), "value_changed",
			  G_CALLBACK (rdetw_spinf_changed),
			  rdet);
  
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), rdet->spinbutton_fps, FALSE, FALSE, 10);
  
  rdet->pertrack_checkbutton = gtk_check_button_new ();
  rdet->backaudio_checkbutton = gtk_check_button_new ();
  
  if (type==3) resaudw=create_resaudw(3,rdet,top_vbox);
  else if (type!=1) resaudw=create_resaudw(10,rdet,top_vbox);

  if (type==3) {
    // extra opts
    
    label= lives_standard_label_new (_("Options"));
    
    gtk_box_pack_start (GTK_BOX (top_vbox), label, FALSE, FALSE, 10);
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (top_vbox), hbox, FALSE, FALSE, 10);

    rdet->backaudio_checkbutton=lives_standard_check_button_new(_("Enable _backing audio track"),TRUE,LIVES_BOX(hbox),NULL);

    add_fill_to_box(LIVES_BOX(hbox));

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rdet->backaudio_checkbutton), prefs->mt_backaudio>0);
    
    gtk_widget_set_sensitive(rdet->backaudio_checkbutton, 
			     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

    rdet->pertrack_checkbutton=lives_standard_check_button_new(_("Audio track _per video track"),TRUE,LIVES_BOX(hbox),NULL);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rdet->pertrack_checkbutton), prefs->mt_pertrack_audio);

    gtk_widget_set_sensitive(rdet->pertrack_checkbutton, 
			     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  }
  
  
#ifndef IS_MINGW
  if (capable->has_encoder_plugins) encoders=get_plugin_list (PLUGIN_ENCODERS,FALSE,NULL,NULL);
#else
  if (capable->has_encoder_plugins) encoders=get_plugin_list (PLUGIN_ENCODERS,TRUE,NULL,NULL);
#endif
  
  if (!specified) encoders=filter_encoders_by_img_ext(encoders,prefs->image_ext);
  else {
    GList *encs=encoders=filter_encoders_by_img_ext(encoders,cfile->img_type==IMG_TYPE_JPEG?"jpg":"png");
    needs_new_encoder=TRUE;
    while (encs!=NULL) {
      if (!strcmp((char *)encs->data,prefs->encoder.name)) {
	needs_new_encoder=FALSE;
	break;
      }
      encs=encs->next;
    }

  }

  hsep = gtk_hseparator_new ();
  label= lives_standard_label_new ("");
  if (!specified) {
    gtk_box_pack_start (GTK_BOX (top_vbox), hsep, TRUE, TRUE, 0);
    if (type!=3) gtk_label_set_text(GTK_LABEL(label),_ ("Options"));
  }

  gtk_box_pack_start (GTK_BOX (top_vbox), label, FALSE, FALSE, 10);

  label = lives_standard_label_new (_("      Target Encoder           "));

  gtk_box_pack_start (GTK_BOX (top_vbox), label, FALSE, FALSE, 0);

  if (!specified) {
    rdet->encoder_name=g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);
    encoders=g_list_prepend(encoders,g_strdup(rdet->encoder_name));
  }
  else {
    rdet->encoder_name=g_strdup(prefs->encoder.name);
  }

  rdet->encoder_combo = lives_combo_new();
  lives_combo_populate(LIVES_COMBO(rdet->encoder_combo),encoders);
  gtk_widget_show(rdet->encoder_combo);

  gtk_box_pack_start (GTK_BOX (top_vbox), rdet->encoder_combo, FALSE, FALSE, 10);


  rdet->encoder_name_fn = g_signal_connect_after(GTK_COMBO_BOX(rdet->encoder_combo), "changed",
						 G_CALLBACK(on_encoder_entry_changed), rdet);

  g_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
  lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), rdet->encoder_name);
  g_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);


  if (encoders!=NULL) {
    g_list_free_strings (encoders);
    g_list_free (encoders);
  }

  encoders=NULL;
  
  if (!specified) {
    ofmt=g_list_append(ofmt,g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));
  }
  else {
    if (capable->has_encoder_plugins) {
      // reqest formats from the encoder plugin
      if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))!=NULL) {
	for (i=0;i<g_list_length(ofmt_all);i++) {
	  if (get_token_count ((gchar *)g_list_nth_data (ofmt_all,i),'|')>2) {
	    array=g_strsplit ((gchar *)g_list_nth_data (ofmt_all,i),"|",-1);
	    if (!strcmp(array[0],prefs->encoder.of_name)) {
	      prefs->encoder.of_allowed_acodecs=atoi(array[2]);
	    }
	    ofmt=g_list_append(ofmt,g_strdup(array[1]));
	    g_strfreev (array);
	  }
	}
	g_list_free_strings(ofmt_all);
	g_list_free(ofmt_all);
      }
      else {
	future_prefs->encoder.of_allowed_acodecs=0;
      }
    }
  }

  label = lives_standard_label_new (_("    Output format           "));
  rdet->ofmt_combo = lives_combo_new();

  lives_combo_populate(LIVES_COMBO(rdet->ofmt_combo), ofmt);

  gtk_combo_box_set_active(GTK_COMBO_BOX(rdet->ofmt_combo), 0);

  g_list_free_strings(ofmt);
  g_list_free(ofmt);
  
  rdet->encoder_ofmt_fn=g_signal_connect_after (GTK_COMBO_BOX(rdet->ofmt_combo), "changed", 
						G_CALLBACK (on_encoder_ofmt_changed), rdet);
  gtk_box_pack_start (GTK_BOX (top_vbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (top_vbox), rdet->ofmt_combo, FALSE, FALSE, 10);
  
  rdet->acodec_combo = lives_combo_new ();
  alabel = lives_standard_label_new (_("    Audio format           "));

  gtk_box_pack_start (GTK_BOX (top_vbox), alabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (top_vbox), rdet->acodec_combo, FALSE, FALSE, 10);
  
  if (!specified) {
    // add "Any" string
    if (prefs->acodec_list!=NULL) {
      g_list_free_strings (prefs->acodec_list);
      g_list_free (prefs->acodec_list);
      prefs->acodec_list=NULL;
    }
    prefs->acodec_list=g_list_append(prefs->acodec_list,g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));
    lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
    gtk_combo_box_set_active(GTK_COMBO_BOX(rdet->acodec_combo), 0);
  }
  else {
    g_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
    lives_combo_set_active_string(LIVES_COMBO(rdet->ofmt_combo), prefs->encoder.of_desc);
    g_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);

    check_encoder_restrictions(TRUE,FALSE,TRUE);
    future_prefs->encoder.of_allowed_acodecs=prefs->encoder.of_allowed_acodecs;
    set_acodec_list_from_allowed(NULL,rdet);
    
    gtk_window_set_default_size (GTK_WINDOW (rdet->dialog), 300, 400);
    
  }


  rdet->always_checkbutton = gtk_check_button_new ();
  
  rdet->always_hbox = gtk_hbox_new (TRUE, 20);


  rdet->always_checkbutton=lives_standard_check_button_new ((tmp=g_strdup(_("_Always use these values"))),TRUE,
							    LIVES_BOX(rdet->always_hbox),
							    (tmp2=g_strdup( _("Check this button to always use these values when entering multitrack mode. Choice can be re-enabled from Preferences."))));

  g_free(tmp);
  g_free(tmp2);

  add_fill_to_box(LIVES_BOX(rdet->always_hbox));

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(rdet->dialog)->action_area), rdet->always_hbox, TRUE, FALSE, 20);


  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  if (!(prefs->startup_interface==STARTUP_MT&&!mainw->is_ready)) {
    gtk_dialog_add_action_widget (GTK_DIALOG (rdet->dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  }
  else add_fill_to_box(GTK_BOX (GTK_DIALOG(rdet->dialog)->action_area));

  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_FOCUS);


  rdet->okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (rdet->dialog), rdet->okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (rdet->okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default (rdet->okbutton);


  gtk_widget_add_accelerator (cancelbutton, "activate", rdet_accel_group,
                              GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0);


  gtk_widget_show_all (rdet->dialog);
  gtk_widget_hide(rdet->always_hbox);

  if (type==4) gtk_widget_hide(resaudw->aud_hbox);
  
  if (needs_new_encoder) {
    gtk_widget_set_sensitive(rdet->okbutton,FALSE);
    while (g_main_context_iteration(NULL,FALSE)); // force showing of transient window
    do_encoder_img_ftm_error(rdet);
  }

  g_signal_connect_after(GTK_COMBO_BOX(rdet->acodec_combo), "changed", G_CALLBACK (rdet_acodec_changed), rdet);

  return rdet;
}

