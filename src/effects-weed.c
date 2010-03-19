// effects-weed.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2010 (salsaman@xs4all.nl)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#include <dlfcn.h>

#include "../libweed/weed.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-host.h"

#include "main.h"
#include "effects-weed.h"

///////////////////////////////////

#include "callbacks.h"
#include "support.h"
#include "rte_window.h"
#include "resample.h"
#include "audio.h"

////////////////////////////////////////////////////////////////////////

#define OIL_MEMCPY_MAX_BYTES 1024 // this can be tuned to provide optimal performance

#ifdef ENABLE_OIL
inline void *w_memcpy  (void *dest, const void *src, size_t n) {
  if (n>=32&&n<=OIL_MEMCPY_MAX_BYTES) {
    oil_memcpy(dest,src,n);
    return dest;
  }
  return memcpy(dest,src,n);
}
#else
inline void *w_memcpy  (void *dest, const void *src, size_t n) {return memcpy(dest,src,n);}
#endif

G_GNUC_MALLOC void * lives_weed_malloc(size_t size) {
  return malloc(size);
}

void lives_weed_free(void *ptr) {
  free(ptr);
}

void *lives_weed_memset(void *s, int c, size_t n) {
  return memset(s,c,n);
}

void *lives_weed_memcpy(void *dest, const void *src, size_t n) {
  return w_memcpy(dest,src,n);
}

////////////////////////////////////////////////////////////////////////////

void weed_add_plant_flags (weed_plant_t *plant, int flags) {
  char **leaves=weed_plant_list_leaves(plant);
  int i,currflags;

  for (i=0;leaves[i]!=NULL;i++) {
    currflags=flags;
    if (flags&WEED_LEAF_READONLY_PLUGIN&&(!strncmp(leaves[i],"plugin_",7))) currflags^=WEED_LEAF_READONLY_PLUGIN;
    weed_leaf_set_flags(plant,leaves[i],weed_leaf_get_flags(plant,leaves[i])|currflags);
    weed_free(leaves[i]);
  }
  weed_free(leaves);
}


static void weed_clear_plant_flags (weed_plant_t *plant, int flags) {
  char **leaves=weed_plant_list_leaves(plant);
  int i;

  for (i=0;leaves[i]!=NULL;i++) {
    weed_leaf_set_flags(plant,leaves[i],(weed_leaf_get_flags(plant,leaves[i])|flags)^flags);
    weed_free(leaves[i]);
  }
  weed_free(leaves);
}


static int match_highest_version (int *hostv, int hostn, int *plugv, int plugn) {
  int hmatch=0;
  int i,j;

  for (i=0;i<plugn;i++) {
    for (j=0;j<hostn;j++) {
      if (hostv[j]>plugv[i]) break;
      if (hostv[j]==plugv[i]&&plugv[i]>hmatch) {
	hmatch=plugv[i];
	break;
      }
    }
  }
  return hmatch;
}

// symbols (function pointers) which are exported to plugins
weed_default_getter_f wdg;

weed_leaf_get_f wlg;
weed_plant_new_f wpn;
weed_plant_list_leaves_f wpll;
weed_leaf_num_elements_f wlne;
weed_leaf_element_size_f wles;
weed_leaf_seed_type_f wlst;
weed_leaf_get_flags_f wlgf;
weed_leaf_set_f wls;
weed_malloc_f weedmalloc;
weed_free_f weedfree;
weed_memcpy_f weedmemcpy;
weed_memset_f weedmemset;


weed_plant_t *weed_bootstrap_func (weed_default_getter_f *value, int num_versions, int *plugin_versions) {
  int host_api_versions_supported[]={131}; // must be ordered in ascending order
  int host_api_version;
  weed_plant_t *host_info=weed_plant_new(WEED_PLANT_HOST_INFO);


  // these functions are defined in weed-host.h and set in weed_init()
  wdg=weed_default_get;

  wlg=weed_leaf_get;
  wpn=weed_plant_new;
  wpll=weed_plant_list_leaves;
  wlne=weed_leaf_num_elements;
  wles=weed_leaf_element_size;
  wlst=weed_leaf_seed_type;
  wlgf=weed_leaf_get_flags;

  wls=weed_leaf_set_plugin; // we pass the plugin's version to the plugin - an example of overloading with Weed
  weedmalloc=weed_malloc;
  weedfree=weed_free;

  weedmemcpy=weed_memcpy;
  weedmemset=weed_memset;

  if (num_versions<1) return NULL;
  if ((host_api_version=match_highest_version(host_api_versions_supported,3,plugin_versions,num_versions))==0) return NULL;
  switch (host_api_version) {
  case 100:
  case 110:
  case 120:
  case 130:
  case 131:
    value[0]=wdg; // bootstrap weed_get_get (the plugin's default_getter)

    weed_set_int_value(host_info,"api_version",host_api_version);

    // here we set (void *)&fn_ptr
    weed_set_voidptr_value(host_info,"weed_leaf_get_func",(void *)&wlg);
    weed_set_voidptr_value(host_info,"weed_leaf_set_func",&wls);
    weed_set_voidptr_value(host_info,"weed_plant_new_func",&wpn);
    weed_set_voidptr_value(host_info,"weed_plant_list_leaves_func",&wpll);
    weed_set_voidptr_value(host_info,"weed_leaf_num_elements_func",&wlne);
    weed_set_voidptr_value(host_info,"weed_leaf_element_size_func",&wles);
    weed_set_voidptr_value(host_info,"weed_leaf_seed_type_func",&wlst);
    weed_set_voidptr_value(host_info,"weed_leaf_get_flags_func",&wlgf);
    weed_set_voidptr_value(host_info,"weed_malloc_func",&weedmalloc);
    weed_set_voidptr_value(host_info,"weed_free_func",&weedfree);
    weed_set_voidptr_value(host_info,"weed_memset_func",&weedmemset);
    weed_set_voidptr_value(host_info,"weed_memcpy_func",&weedmemcpy);
    
    weed_add_plant_flags(host_info,WEED_LEAF_READONLY_PLUGIN);
    break;
  default:
    return NULL;
  }
  return host_info;
}

//////////////////////////////////////////////////////////////////////////////
// filter library functions


gint weed_filter_categorise (weed_plant_t *pl, int in_channels, int out_channels) {
  weed_plant_t *filt=pl;
  int filter_flags,error;
  if (WEED_PLANT_IS_FILTER_INSTANCE(pl)) filt=weed_get_plantptr_value(pl,"template",&error);
  filter_flags=weed_get_int_value(filt,"flags",&error);
  if (filter_flags&WEED_FILTER_IS_CONVERTER) return 8;
  if (in_channels==0&&out_channels>0) return 1; // generator
  if (out_channels>1) return 7; // splitter : use optional non-alpha out_channels
  if (in_channels>2&&out_channels==1) return 5; // compositor : use optional non-alpha in_channels
  if (in_channels==2&&out_channels==1) return 2; // transition
  if (in_channels==1&&out_channels==1) return 3; // filter
  if (in_channels>0&&out_channels==0) return 6; // tap : use optional non-alpha in_channels
  if (in_channels==0&&out_channels==0) return 4; // utility
  return 0;
}


gint weed_filter_subcategorise (weed_plant_t *pl, int category, gboolean count_opt) {
  weed_plant_t *filt=pl;
  gboolean has_video_chansi;
  int error;

  if (WEED_PLANT_IS_FILTER_INSTANCE(pl)) filt=weed_get_plantptr_value(pl,"template",&error);

  has_video_chansi=has_video_chans_in(filt,count_opt);

  if (category==2) {
    if (get_transition_param(filt)!=-1) {
      if (!has_video_chansi) return 11;
      return 9;
    }
    return 10;
  }

  if (category==5&&!has_video_chansi) return 12;
  if (category==1&&!has_video_chansi) return 13;
  if (category==8&&!has_video_chansi) return 14;

  return 0;
}

/////////////////////////////////////////////////////////////////////////
gchar *weed_category_to_text(int cat, gboolean plural) {
  // return value should be free'd after use
  switch (cat) {

    // main categories
  case 1:
    if (!plural) return (g_strdup(_("generator")));
    else return (g_strdup(_("Generators")));
  case 2:
    if (!plural) return (g_strdup(_("transition")));
    else return (g_strdup(_("Transitions")));
  case 3:
    if (!plural) return (g_strdup(_("effect")));
    else return (g_strdup(_("Effects")));
  case 4:
    if (!plural) return (g_strdup(_("utility")));
    else return (g_strdup(_("Utilities")));
  case 5:
    if (!plural) return (g_strdup(_("compositor")));
    else return (g_strdup(_("Compositors")));
  case 6:
    if (!plural) return (g_strdup(_("tap")));
    else return (g_strdup(_("Taps")));
  case 7:
    if (!plural) return (g_strdup(_("splitter")));
    else return (g_strdup(_("Splitters")));
  case 8:
    if (!plural) return (g_strdup(_("converter")));
    else return (g_strdup(_("Converters")));


    // subcategories
  case 9:
    if (!plural) return (g_strdup(_("audio/video")));
    else return (g_strdup(_("Audio/Video Transitions")));
  case 10:
    if (!plural) return (g_strdup(_("video only")));
    else return (g_strdup(_("Video only Transitions")));
  case 11:
    if (!plural) return (g_strdup(_("audio only")));
    else return (g_strdup(_("Audio only Transitions")));
  case 12:
    if (!plural) return (g_strdup(_("audio")));
    else return (g_strdup(_("Audio Mixers")));
  case 13:
    if (!plural) return (g_strdup(_("audio")));
    else return (g_strdup(_("Audio Effects")));
  case 14:
    if (!plural) return (g_strdup(_("audio volume controller")));
    else return (g_strdup(_("Audio Volume Controllers")));


  default:
    return (g_strdup(_("unknown")));
  }
}


////////////////////////////////////////////////////////////////////////

#define MAX_WEED_FILTERS 65536
#define MAX_WEED_INSTANCES 65536

#define MAX_MODES_PER_KEY 8

// store keys so we now eg, which rte mask entry to xor when deiniting
static int fg_generator_key;
static int bg_generator_key;

// store modes too
static int fg_generator_mode;
static int bg_generator_mode;

// generators to start on playback, because of a problem in libvisual 
// - we must start generators after starting audio
static int bg_gen_to_start;
static int fg_gen_to_start;

// store the clip, this can sometimes get lost
static gint fg_generator_clip;


//////////////////////////////////////////////////////////////////////

static weed_plant_t *weed_filters[MAX_WEED_FILTERS]; // array of filter_classes

static weed_plant_t *weed_instances[MAX_WEED_INSTANCES]; // array of filter_instances
static weed_plant_t *weed_instances_copy[MAX_WEED_INSTANCES]; // array of filter_instances - copy for preview during rendering


// each 'hotkey' controls n instances, selectable as 'modes' or banks
static int key_to_instance[FX_KEYS_MAX][MAX_MODES_PER_KEY];
static int key_to_instance_copy[FX_KEYS_MAX][1]; // copy for preview during rendering

static int key_to_fx[FX_KEYS_MAX][MAX_MODES_PER_KEY];
static int key_modes[FX_KEYS_MAX];

static int next_free_instance=0; // we use an arena of instances

// count of how many filters we have loaded
static gint num_weed_filters;

static gchar *hashnames[MAX_WEED_FILTERS];


/////////////////// LiVES event system /////////////////

static void *init_events[FX_KEYS_MAX_VIRTUAL];
static void **pchains[FX_KEYS_MAX]; // parameter changes, used during recording (not for rendering)
static void *filter_map[FX_KEYS_MAX+2];
static int next_free_key;
static int key; // for assigning loaded filters

////////////////////////////////////////////////////////////////////


void backup_weed_instances(void) {
  // this is called during multitrack rendering. We are rendering, but we want to display the current frame in the preview window
  // thus we backup our rendering instances, apply the current frame instances, and then restore the rendering instances
  register int i;

  for (i=0;i<MAX_WEED_INSTANCES;i++) {
    weed_instances_copy[i]=weed_instances[i];
    weed_instances[i]=NULL;
  }

  for (i=FX_KEYS_MAX_VIRTUAL;i<FX_KEYS_MAX;i++) {
    key_to_instance_copy[i][0]=key_to_instance[i][0];
    key_to_instance[i][0]=-1;
  }
}


void restore_weed_instances(void) {
  register int i;

  for (i=0;i<MAX_WEED_INSTANCES;i++) {
    weed_instances[i]=weed_instances_copy[i];
  }

  for (i=FX_KEYS_MAX_VIRTUAL;i<FX_KEYS_MAX;i++) {
    key_to_instance[i][0]=key_to_instance_copy[i][0];
  }
}



inline gint step_val(gint val, gint step) {
  gint ret=(gint)(val/step+.5)*step;
  return ret==0?step:ret;
}


gchar *weed_filter_get_type(gint idx) {
  // return value should be free'd after use
  return weed_category_to_text(weed_filter_categorise(weed_filters[idx],enabled_in_channels(weed_filters[idx],FALSE),enabled_out_channels(weed_filters[idx],FALSE)),FALSE);
}

void update_host_info (weed_plant_t *inst) {
  // set "host_audio_plugin" in the host_info
  int error;
  weed_plant_t *filter,*pinfo,*hinfo;

  filter=weed_get_plantptr_value(inst,"filter_class",&error);
  pinfo=weed_get_plantptr_value(filter,"plugin_info",&error);
  hinfo=weed_get_plantptr_value(pinfo,"host_info",&error);

  switch (prefs->audio_player) {
  case AUD_PLAYER_MPLAYER:
    weed_set_string_value(hinfo,"host_audio_player","mplayer");
    break;
  case AUD_PLAYER_SOX:
    weed_set_string_value(hinfo,"host_audio_player","sox");
    break;
  case AUD_PLAYER_JACK:
    weed_set_string_value(hinfo,"host_audio_player","jack");
    break;
  case AUD_PLAYER_PULSE:
    weed_set_string_value(hinfo,"host_audio_player","pulseaudio");
    break;
  }
}


weed_plant_t *get_enabled_channel (weed_plant_t *inst, gint which, gboolean is_in) {
  // plant is a filter_instance
  // "which" starts at 0
  int i=0,error;
  weed_plant_t **channels;
  weed_plant_t *retval;

  if (!WEED_PLANT_IS_FILTER_INSTANCE(inst)) return NULL;

  if (is_in) channels=weed_get_plantptr_array(inst,"in_channels",&error);
  else channels=weed_get_plantptr_array(inst,"out_channels",&error);

  if (channels==NULL) return NULL;
  while (which>-1) {
    if (!weed_plant_has_leaf(channels[i],"disabled")||weed_get_boolean_value(channels[i],"disabled",&error)==WEED_FALSE) which--;
    i++;
  }
  retval=channels[i-1];
  weed_free(channels);
  return retval;
}


weed_plant_t *get_mandatory_channel (weed_plant_t *filter, gint which, gboolean is_in) {
  // plant is a filter_class
  // "which" starts at 0
  int i=0,error;
  weed_plant_t **ctmpls;
  weed_plant_t *retval;

  if (!WEED_PLANT_IS_FILTER_CLASS(filter)) return NULL;

  if (is_in) ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  else ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);

  if (ctmpls==NULL) return NULL;
  while (which>-1) {
    if (!weed_plant_has_leaf(ctmpls[i],"optional")) which--;
    i++;
  }
  retval=ctmpls[i-1];
  weed_free(ctmpls);
  return retval;
}


gboolean weed_filter_is_resizer(weed_plant_t *filt) {
  int error;
  int filter_flags=weed_get_int_value(filt,"flags",&error);
  if (filter_flags&WEED_FILTER_IS_CONVERTER) {
    weed_plant_t *first_out=get_mandatory_channel(filt,0,FALSE);
    if (first_out!=NULL) {
      int tmpl_flags=weed_get_int_value(first_out,"flags",&error);
      if (tmpl_flags&WEED_CHANNEL_SIZE_CAN_VARY) return TRUE;
    }
  }
  return FALSE;
}



gboolean weed_instance_is_resizer(weed_plant_t *inst) {
  int error;
  weed_plant_t *ftmpl=weed_get_plantptr_value(inst,"filter_class",&error);
  return weed_filter_is_resizer(ftmpl);
}



gboolean is_audio_channel_in(weed_plant_t *inst, int chnum) {
  int error,nchans=weed_leaf_num_elements(inst,"in_channels");
  weed_plant_t **in_chans;
  weed_plant_t *ctmpl;

  if (nchans<=chnum) return FALSE;

  in_chans=weed_get_plantptr_array(inst,"in_channels",&error);
  ctmpl=weed_get_plantptr_value(in_chans[chnum],"template",&error);
  weed_free(in_chans);

  if (weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE) {
    return TRUE;
  }
  return FALSE;
}


gboolean has_video_chans_in(weed_plant_t *filter, gboolean count_opt) {
  int error,nchans=weed_leaf_num_elements(filter,"in_channel_templates");
  weed_plant_t **in_ctmpls;
  int i;

  if (nchans==0) return FALSE;

  in_ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  for (i=0;i<nchans;i++) {
    if (!count_opt&&weed_plant_has_leaf(in_ctmpls[i],"optional")&&weed_get_boolean_value(in_ctmpls[i],"optional",&error)==WEED_TRUE) continue;
    if (weed_plant_has_leaf(in_ctmpls[i],"is_audio")&&weed_get_boolean_value(in_ctmpls[i],"is_audio",&error)==WEED_TRUE) continue;
    weed_free(in_ctmpls);
    return TRUE;
  }
  weed_free(in_ctmpls);

  return FALSE;
}



gboolean has_audio_chans_in(weed_plant_t *filter, gboolean count_opt) {
  int error,nchans=weed_leaf_num_elements(filter,"in_channel_templates");
  weed_plant_t **in_ctmpls;
  int i;

  if (nchans==0) return FALSE;

  in_ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  for (i=0;i<nchans;i++) {
    if (!count_opt&&weed_plant_has_leaf(in_ctmpls[i],"optional")&&weed_get_boolean_value(in_ctmpls[i],"optional",&error)==WEED_TRUE) continue;
    if (!weed_plant_has_leaf(in_ctmpls[i],"is_audio")||weed_get_boolean_value(in_ctmpls[i],"is_audio",&error)==WEED_FALSE) continue;
    weed_free(in_ctmpls);
    return TRUE;
  }
  weed_free(in_ctmpls);

  return FALSE;
}


gboolean is_audio_channel_out(weed_plant_t *inst, int chnum) {
  int error,nchans=weed_leaf_num_elements(inst,"out_channels");
  weed_plant_t **out_chans;
  weed_plant_t *ctmpl;

  if (nchans<=chnum) return FALSE;

  out_chans=weed_get_plantptr_array(inst,"out_channels",&error);
  ctmpl=weed_get_plantptr_value(out_chans[chnum],"template",&error);
  weed_free(out_chans);

  if (weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE) {
    return TRUE;
  }
  return FALSE;
}


gboolean has_video_chans_out(weed_plant_t *filter, gboolean count_opt) {
  int error,nchans=weed_leaf_num_elements(filter,"out_channel_templates");
  weed_plant_t **out_ctmpls;
  int i;

  if (nchans==0) return FALSE;

  out_ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
  for (i=0;i<nchans;i++) {
    if (!count_opt&&weed_plant_has_leaf(out_ctmpls[i],"optional")&&weed_get_boolean_value(out_ctmpls[i],"optional",&error)==WEED_TRUE) continue;
    if (weed_plant_has_leaf(out_ctmpls[i],"is_audio")&&weed_get_boolean_value(out_ctmpls[i],"is_audio",&error)==WEED_TRUE) continue;
    weed_free(out_ctmpls);
    return TRUE;
  }
  weed_free(out_ctmpls);

  return FALSE;
}



gboolean has_audio_chans_out(weed_plant_t *filter, gboolean count_opt) {
  int error,nchans=weed_leaf_num_elements(filter,"out_channel_templates");
  weed_plant_t **out_ctmpls;
  int i;

  if (nchans==0) return FALSE;

  out_ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
  for (i=0;i<nchans;i++) {
    if (!count_opt&&weed_plant_has_leaf(out_ctmpls[i],"optional")&&weed_get_boolean_value(out_ctmpls[i],"optional",&error)==WEED_TRUE) continue;
    if (!weed_plant_has_leaf(out_ctmpls[i],"is_audio")||weed_get_boolean_value(out_ctmpls[i],"is_audio",&error)==WEED_FALSE) continue;
    weed_free(out_ctmpls);
    return TRUE;
  }
  weed_free(out_ctmpls);

  return FALSE;
}




static void create_filter_map (void) {
  // here we create an effect map which defines the order in which effects are applied to a frame stack
  // this is done during recording, the keymap is from mainw->rte which is a bitmap of effect keys
  // keys are applied here from smallest (ctrl-1) to largest (virtual key ctrl-FX_KEYS_MAX_VIRTUAL)

  // this is transformed into filter_map which holds init_events 
  // these pointers are then stored in a filter_map event

  // what we actually point to are the init_events for the effects. The init_events are stored when we 
  // init an effect

  // during rendering we read the filter_map event, and retrieve the new key, which is at that time 
  // held in the 
  // "host_tag" property of the init_event, and we apply our effects
  // (which are then bound to virtual keys >=FX_KEYS_MAX_VIRTUAL)

  // [note] that we can do cool things, like mapping the same instance multiple times (though it will always
  // apply itself to the same in/out tracks

  // we don't need to worry about free()ing init_events, since they will be free'd 
  // when the instance is deinited

  int count=0,i;
  int idx;

  for (i=0;i<FX_KEYS_MAX_VIRTUAL;i++) if (mainw->rte&(GU641<<i)&&(idx=key_to_instance[i][key_modes[i]])!=-1&&enabled_in_channels (weed_instances[idx],FALSE)>0) filter_map[count++]=init_events[i];
  filter_map[count]=NULL; // marks the end of the effect map
}


weed_plant_t *add_filter_deinit_events (weed_plant_t *event_list) {
  // during rendering we use the "keys" FX_KEYS_MAX_VIRTUAL -> FX_KEYS_MAX
  // here we add effect_deinit events to an event_list
  int i;
  gboolean needs_filter_map=FALSE;
  weed_timecode_t last_tc=0;

  if (event_list!=NULL) last_tc=get_event_timecode(get_last_event(event_list));

  for (i=0;i<FX_KEYS_MAX_VIRTUAL;i++) {
    if (init_events[i]!=NULL) {
      event_list=append_filter_deinit_event (event_list,last_tc,init_events[i],pchains[i]);
      init_events[i]=NULL;
      if (pchains[i]!=NULL) g_free(pchains[i]);
      needs_filter_map=TRUE;
    }
  }
  // add an empty filter_map event (in case more frames are added)
  create_filter_map(); // we create filter_map event_t * array with ordered effects

  if (needs_filter_map) event_list=append_filter_map_event (mainw->event_list,last_tc,filter_map);
  return event_list;
}


weed_plant_t *add_filter_init_events (weed_plant_t *event_list, weed_timecode_t tc) {
  // during rendering we use the "keys" FX_KEYS_MAX_VIRTUAL -> FX_KEYS_MAX
  // here we are about to start playback, and we add init events for every effect which is switched on
  // we add the init events with a timecode of 0
  int i;
  gint idx;
  int fx_idx,ntracks;

  for (i=0;i<FX_KEYS_MAX_VIRTUAL;i++) {
    if ((idx=key_to_instance[i][key_modes[i]])!=-1&&enabled_in_channels (weed_instances[idx],FALSE)>0) {
      event_list=append_filter_init_event (event_list,tc,(fx_idx=key_to_fx[i][key_modes[i]]),-1);
      init_events[i]=(void *)get_last_event(event_list);
      ntracks=weed_leaf_num_elements(init_events[i],"in_tracks");
      pchains[i]=filter_init_add_pchanges(event_list,weed_instances[key_to_instance[i][key_modes[i]]],init_events[i],ntracks);
    }
  }
  // add an empty filter_map event (in case more frames are added)
  create_filter_map(); // we create filter_map event_t * array with ordered effects
  if (filter_map[0]!=NULL) event_list=append_filter_map_event (event_list,tc,filter_map);
  return event_list;
}


// check if palette is in the palette_list
// if not, return next best palette to use, using a heuristic method
// num_palettes is the size of the palette list
int check_weed_palette_list (int *palette_list, int num_palettes, int palette) {
  int i;
  int best_palette=WEED_PALETTE_END;

  for (i=0;i<num_palettes;i++) {
    if (palette_list[i]==palette) {
      // exact match - return it
      return palette;
    }
    // pass 1, see if we can find same or higher quality in same colorspace
    if (weed_palette_is_alpha_palette(palette)) {
      if (palette_list[i]==WEED_PALETTE_A8) best_palette=palette_list[i];
      if (palette_list[i]==WEED_PALETTE_A1&&(best_palette==WEED_PALETTE_AFLOAT||best_palette==WEED_PALETTE_END)) best_palette=palette_list[i];
      if (palette_list[i]==WEED_PALETTE_AFLOAT&&best_palette==WEED_PALETTE_END) best_palette=palette_list[i];
    }
    else if (weed_palette_is_rgb_palette(palette)) {
      if (palette_list[i]==WEED_PALETTE_RGBAFLOAT&&(palette==WEED_PALETTE_RGBFLOAT||best_palette==WEED_PALETTE_END)) best_palette=palette_list[i];
      if (palette_list[i]==WEED_PALETTE_RGBFLOAT&&best_palette==WEED_PALETTE_END) best_palette=palette_list[i];
      if (palette_list[i]==WEED_PALETTE_RGBA32||palette_list[i]==WEED_PALETTE_BGRA32||palette_list[i]==WEED_PALETTE_ARGB32) {
	if ((best_palette==WEED_PALETTE_END||best_palette==WEED_PALETTE_RGBFLOAT||best_palette==WEED_PALETTE_RGBAFLOAT)||weed_palette_has_alpha_channel(palette)) best_palette=palette_list[i];
      }
      if (!weed_palette_has_alpha_channel(palette)||(best_palette==WEED_PALETTE_END||best_palette==WEED_PALETTE_RGBFLOAT||best_palette==WEED_PALETTE_RGBAFLOAT)) {
	if (palette_list[i]==WEED_PALETTE_RGB24||palette_list[i]==WEED_PALETTE_BGR24) {
	  best_palette=palette_list[i];
	}
      }
    }
    else {
      // yuv
      if (palette==WEED_PALETTE_YUV411&&(palette_list[i]==WEED_PALETTE_YUV422P)) best_palette=palette_list[i];
      if (palette==WEED_PALETTE_YUV411&&best_palette!=WEED_PALETTE_YUV422P&&(palette_list[i]==WEED_PALETTE_YUV420P||palette_list[i]==WEED_PALETTE_YVU420P)) best_palette=palette_list[i];
      if (palette==WEED_PALETTE_YUV420P&&palette_list[i]==WEED_PALETTE_YVU420P) best_palette=palette_list[i];
      if (palette==WEED_PALETTE_YVU420P&&palette_list[i]==WEED_PALETTE_YUV420P) best_palette=palette_list[i];
      
      if (((palette==WEED_PALETTE_YUV420P&&best_palette!=WEED_PALETTE_YVU420P)||(palette==WEED_PALETTE_YVU420P&&best_palette!=WEED_PALETTE_YUV420P))&&(palette_list[i]==WEED_PALETTE_YUV422P||palette_list[i]==WEED_PALETTE_UYVY8888||palette_list[i]==WEED_PALETTE_YUYV8888)) best_palette=palette_list[i];
      
      if ((palette==WEED_PALETTE_YUV422P||palette==WEED_PALETTE_UYVY8888||palette==WEED_PALETTE_YUYV8888)&&(palette_list[i]==WEED_PALETTE_YUV422P||palette_list[i]==WEED_PALETTE_UYVY8888||palette_list[i]==WEED_PALETTE_YUYV8888)) best_palette=palette_list[i];
      
      if (palette_list[i]==WEED_PALETTE_YUVA8888||palette_list[i]==WEED_PALETTE_YUVA4444P) {
	if (best_palette==WEED_PALETTE_END||weed_palette_has_alpha_channel(palette)) best_palette=palette_list[i];
      }
      if (best_palette==WEED_PALETTE_END||((best_palette==WEED_PALETTE_YUVA8888||best_palette==WEED_PALETTE_YUVA4444P)&&!weed_palette_has_alpha_channel(palette))) {
	if (palette_list[i]==WEED_PALETTE_YUV888||palette_list[i]==WEED_PALETTE_YUV444P) {
	  best_palette=palette_list[i];
	}
      }
    }
  }
  
  // pass 2:
  // if we had to drop alpha, see if we can preserve it in the other colorspace
  for (i=0;i<num_palettes;i++) {
    if (weed_palette_has_alpha_channel(palette)&&(best_palette==WEED_PALETTE_END||!weed_palette_has_alpha_channel(best_palette))) {
      if (weed_palette_is_rgb_palette(palette)) {
	if (palette_list[i]==WEED_PALETTE_YUVA8888||palette_list[i]==WEED_PALETTE_YUVA4444P) best_palette=palette_list[i];
      }
      else {
	if (palette_list[i]==WEED_PALETTE_RGBA32||palette_list[i]==WEED_PALETTE_BGRA32||palette_list[i]==WEED_PALETTE_ARGB32) best_palette=palette_list[i];
      }
    }
  }

  // pass 3: no alpha; switch colorspaces, try to find same or higher quality
  if (best_palette==WEED_PALETTE_END) {
    for (i=0;i<num_palettes;i++) {
      if (best_palette==WEED_PALETTE_END&&(palette_list[i]==WEED_PALETTE_RGBA32||palette_list[i]==WEED_PALETTE_BGRA32||palette_list[i]==WEED_PALETTE_ARGB32)) {
	best_palette=palette_list[i];
      }
      if (best_palette==WEED_PALETTE_END&&(palette_list[i]==WEED_PALETTE_YUVA8888||palette_list[i]==WEED_PALETTE_YUVA4444P)) {
	best_palette=palette_list[i];
      }
      if ((weed_palette_is_rgb_palette(palette)||best_palette==WEED_PALETTE_END)&&(palette_list[i]==WEED_PALETTE_RGB24||palette_list[i]==WEED_PALETTE_BGR24)) {
	best_palette=palette_list[i];
      }
      if ((weed_palette_is_yuv_palette(palette)||best_palette==WEED_PALETTE_END)&&(palette_list[i]==WEED_PALETTE_YUV888||palette_list[i]==WEED_PALETTE_YUV444P)) {
	best_palette=palette_list[i];
      }
    }
  }
  
  // pass 4: switch to YUV, try to find highest quality
  if (best_palette==WEED_PALETTE_END) {
    for (i=0;i<num_palettes;i++) {
      if (palette_list[i]==WEED_PALETTE_UYVY8888||palette_list[i]==WEED_PALETTE_YUYV8888||palette_list[i]==WEED_PALETTE_YUV422P) {
	best_palette=palette_list[i];
      }
      if ((best_palette==WEED_PALETTE_END||best_palette==WEED_PALETTE_YUV411)&&(palette_list[i]==WEED_PALETTE_YUV420P||palette_list[i]==WEED_PALETTE_YVU420P)) {
	best_palette=palette_list[i];
      }
      if (best_palette==WEED_PALETTE_END&&palette_list[i]==WEED_PALETTE_YUV411) best_palette=palette_list[i];
    }
  }

  // pass 5: tweak results to use (probably) most common colourspaces
  for (i=0;i<num_palettes;i++) {
    if (palette_list[i]==WEED_PALETTE_RGBA32&&(best_palette==WEED_PALETTE_BGRA32||best_palette==WEED_PALETTE_ARGB32)) {
      best_palette=palette_list[i];
    }
    if (palette_list[i]==WEED_PALETTE_RGB24&&best_palette==WEED_PALETTE_BGR24) {
      best_palette=palette_list[i];
    }
    if (palette_list[i]==WEED_PALETTE_YUV420P&&best_palette==WEED_PALETTE_YVU420P) {
      best_palette=palette_list[i];
    }
    if (palette_list[i]==WEED_PALETTE_UYVY8888&&best_palette==WEED_PALETTE_YUYV8888) {
      best_palette=palette_list[i];
    }
  }

#ifdef DEBUG_PALETTES
  g_printerr("Debug: best palette for %d is %d\n",palette,best_palette);
#endif
    
  return best_palette;
}

static void set_channel_size (weed_plant_t *channel, gint width, gint height, int numplanes, int *rowstrides) {
  int error;
  int max;
  weed_plant_t *chantmpl=weed_get_plantptr_value(channel,"template",&error);
  // note: rowstrides is just a guess, we will set the actual value when we come to process the effect


  if (weed_plant_has_leaf(chantmpl,"width")&&weed_get_int_value(chantmpl,"width",&error)!=0) width=weed_get_int_value(chantmpl,"width",&error);
  else if (weed_plant_has_leaf(chantmpl,"host_width")) width=weed_get_int_value(chantmpl,"host_width",&error);
  if (weed_plant_has_leaf(chantmpl,"hstep")) width=step_val(width,weed_get_int_value(chantmpl,"hstep",&error));
  if (weed_plant_has_leaf(chantmpl,"maxwidth")) {
    max=weed_get_int_value(chantmpl,"maxwidth",&error);
    if (width>max) width=max;
  }
  weed_set_int_value(channel,"width",width);

  if (weed_plant_has_leaf(chantmpl,"height")&&weed_get_int_value(chantmpl,"height",&error)!=0) height=weed_get_int_value(chantmpl,"height",&error);
  else if (weed_plant_has_leaf(chantmpl,"host_height")) height=weed_get_int_value(chantmpl,"host_height",&error);
  if (weed_plant_has_leaf(chantmpl,"vstep")) height=step_val(height,weed_get_int_value(chantmpl,"vstep",&error));
  if (weed_plant_has_leaf(chantmpl,"maxheight")) {
    max=weed_get_int_value(chantmpl,"maxheight",&error);
    if (height>max) height=max;
  }
  weed_set_int_value(channel,"height",height);

  if (rowstrides!=NULL) weed_set_int_array(channel,"rowstrides",numplanes,rowstrides);
  
}


static gboolean rowstrides_differ(int n1, int *n1_array, int n2, int *n2_array) {
  // returns TRUE if the rowstrides differ
  int i;

  if (n1!=n2) return TRUE;
  for (i=0;i<n1;i++) if (n1_array[i]!=n2_array[i]) return TRUE;
  return FALSE;
}


static gboolean align (void **pixel_data, size_t alignment, int numplanes, int height, int *rowstrides) {
  // returns TRUE on success
  int i;
  for (i=0;i<numplanes;i++) {
    if (((gulong)(pixel_data[i]))%alignment==0) continue;
#ifdef HAVE_POSIX_MEMALIGN
    else {
      int memerror;
      void *new_pixel_data;
      if ((memerror=posix_memalign(&new_pixel_data,alignment,height*rowstrides[i]))) return FALSE;
      memcpy(new_pixel_data,pixel_data[i],height*rowstrides[i]);
      g_free(pixel_data[i]);
      pixel_data[i]=new_pixel_data;
    }
#else
    return FALSE;
#endif
  }
  return TRUE;
}


void set_param_gui_readonly (weed_plant_t *inst) {
  int num_params,error,i;
  weed_plant_t **params,*gui,*ptmpl;

  num_params=weed_leaf_num_elements(inst,"in_parameters");
  if (num_params>0) {
    params=weed_get_plantptr_array(inst,"in_parameters",&error);
    for (i=0;i<num_params;i++) {
      ptmpl=weed_get_plantptr_value(params[i],"template",&error);
      if (weed_plant_has_leaf(ptmpl,"gui")) {
	gui=weed_get_plantptr_value(ptmpl,"gui",&error);
	weed_add_plant_flags(gui,WEED_LEAF_READONLY_PLUGIN);
      }
    }
    weed_free(params);
  }

  num_params=weed_leaf_num_elements(inst,"out_parameters");
  if (num_params>0) {
    params=weed_get_plantptr_array(inst,"out_parameters",&error);
    for (i=0;i<num_params;i++) {
      ptmpl=weed_get_plantptr_value(params[i],"template",&error);
      if (weed_plant_has_leaf(ptmpl,"gui")) {
	gui=weed_get_plantptr_value(ptmpl,"gui",&error);
	weed_add_plant_flags(gui,WEED_LEAF_READONLY_PLUGIN);
      }
    }
    weed_free(params);
  }
}

void set_param_gui_readwrite (weed_plant_t *inst) {
  int num_params,error,i;
  weed_plant_t **params,*gui,*ptmpl;

  num_params=weed_leaf_num_elements(inst,"in_parameters");
  if (num_params>0) {
    params=weed_get_plantptr_array(inst,"in_parameters",&error);
    for (i=0;i<num_params;i++) {
      ptmpl=weed_get_plantptr_value(params[i],"template",&error);
      if (weed_plant_has_leaf(ptmpl,"gui")) {
	gui=weed_get_plantptr_value(ptmpl,"gui",&error);
	weed_clear_plant_flags(gui,WEED_LEAF_READONLY_PLUGIN);
      }
    }
    weed_free(params);
  }

  num_params=weed_leaf_num_elements(inst,"out_parameters");
  if (num_params>0) {
    params=weed_get_plantptr_array(inst,"out_parameters",&error);
    for (i=0;i<num_params;i++) {
      ptmpl=weed_get_plantptr_value(params[i],"template",&error);
      if (weed_plant_has_leaf(ptmpl,"gui")) {
	gui=weed_get_plantptr_value(ptmpl,"gui",&error);
	weed_clear_plant_flags(gui,WEED_LEAF_READONLY_PLUGIN);
      }
    }
    weed_free(params);
  }
}



gint weed_reinit_effect (weed_plant_t *inst) {
  int error;
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);

  weed_call_deinit_func(inst);

  if (weed_plant_has_leaf(filter,"init_func")) {
    weed_init_f *init_func_ptr_ptr;
    weed_init_f init_func;
    weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
    init_func=init_func_ptr_ptr[0];
    if (init_func!=NULL) {
      lives_rfx_t *rfx;
      set_param_gui_readwrite(inst);
      update_host_info(inst);
      if ((*init_func)(inst)!=WEED_NO_ERROR) return FILTER_ERROR_COULD_NOT_REINIT;
      set_param_gui_readonly(inst);
      if (fx_dialog[1]!=NULL) {
	// redraw GUI if necessary
	rfx=g_object_get_data(G_OBJECT(fx_dialog[1]),"rfx");
	if (!rfx->is_template) {
	  gint keyw=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
	  gint modew=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
	  close_pwindow(keyw,modew,TRUE);
	}
	else close_pwindow(0,0,TRUE);
      }
    }
    return FILTER_INFO_REINITED;
  }
  return FILTER_NO_ERROR;
}

void weed_reinit_all(void) {
  // reinit all effects on playback start
  int i;
  weed_plant_t *instance;

  for (i=0;i<FX_KEYS_MAX_VIRTUAL;i++) {
    if (rte_key_valid(i+1,TRUE)) {
      if (mainw->rte&(GU641<<i)) {
	mainw->osc_block=TRUE;
	if (key_to_instance[i][key_modes[i]]==-1) continue;
	instance=weed_instances[key_to_instance[i][key_modes[i]]];
	if (instance==NULL) continue;
	if (enabled_in_channels(instance,FALSE)==0) continue;
	weed_reinit_effect(instance);
      }
    }
  }
  mainw->osc_block=FALSE;
}



gint weed_apply_instance (weed_plant_t *inst, weed_plant_t *init_event, weed_plant_t **layers, int opwidth, int opheight, weed_timecode_t tc) {
  // here we:
  // get our in_tracks and out_tracks that map filter_instance channels to layers

  // clear "disabled" if we have non-zero frame and there is no "disabled" in template
  // if we have a zero frame, set "disabled" if "optional", otherwise we cannot apply the filter

  // set channel timecodes

  // pull pixel_data (unless it is there already)

  // set each channel width,height to match largest of in layers

  // if width and height are wrong, resize in the layer

  // if palette is wrong, first we try to change the plugin channel palette, if not possible we convert palette in the layer

  // apply the effect, put result in output layer, set layer palette, width, height, rowstrides

  // if filter does not support inplace, we must create a new pixel_data; this will then replace the original layer



  // WARNING: output layer may need resizing, and its palette may need adjusting - should be checked by the caller

  // opwidth and opheight limit the maximum frame size, they can either be set to 0,0 or to max display size; however if all sources are smaller than this
  // then the output will be smaller also and need resizing by the caller

  // TODO ** - handle return errors
  int num_in_tracks,num_out_tracks;
  int *in_tracks,*out_tracks;
  int error,i,j;
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);
  weed_plant_t *layer;
  weed_plant_t **in_channels,**out_channels,*channel,*chantmpl;
  int frame;
  int inwidth,inheight,inpalette,outpalette,channel_flags,palette,cpalette;
  int outwidth,outheight;
  gboolean needs_reinit=FALSE,inplace=FALSE;
  int incwidth,incheight,numplanes=0,width,height;
  int *rowstrides;
  void **pixel_data;
  weed_process_f *process_func_ptr_ptr;
  weed_process_f process_func;
  weed_plant_t *def_channel=NULL;
  gboolean rowstrides_changed;
  int nchr;
  int *layer_rows=NULL,*channel_rows;
  gint retval=FILTER_NO_ERROR;
  int *mand;
  int maxinwidth=4,maxinheight=4;
  int oclamping,iclamping;
  int clip;
  int num_ctmpl,num_inc;
  weed_plant_t **in_ctmpls;

  gboolean def_disabled=FALSE;

  gint lcount=0;


  // here, in_tracks and out_tracks map our layers to in_channels and out_channels in the filter
  if (!weed_plant_has_leaf(inst,"in_channels")||(in_channels=weed_get_plantptr_array(inst,"in_channels",&error))==NULL) return FILTER_ERROR_NO_IN_CHANNELS;

  if (get_enabled_channel(inst,0,TRUE)==NULL) {
    // we process generators elsewhere
    weed_free(in_channels);
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (!has_video_chans_in(filter,TRUE)||!has_video_chans_out(filter,TRUE)) {
    weed_free(in_channels);
    return FILTER_ERROR_IS_AUDIO; // we process audio effects elsewhere
  }

  if (init_event==NULL) {
    num_in_tracks=enabled_in_channels(inst,FALSE);
    in_tracks=weed_malloc(2*sizint);
    in_tracks[0]=0;
    in_tracks[1]=1;
    num_out_tracks=1;
    out_tracks=weed_malloc(sizint);
    out_tracks[0]=0;
  }
  else {
    num_in_tracks=weed_leaf_num_elements(init_event,"in_tracks");
    in_tracks=weed_get_int_array(init_event,"in_tracks",&error);
    num_out_tracks=weed_leaf_num_elements(init_event,"out_tracks");
    out_tracks=weed_get_int_array(init_event,"out_tracks",&error);
  }

  out_channels=weed_get_plantptr_array(inst,"out_channels",&error);

  // handle case where in_tracks[i] > than num layers
  // either we temporarily disable the channel, or we can't apply the filter
  num_inc=weed_leaf_num_elements(inst,"in_channels");

  if (num_in_tracks>num_inc) num_in_tracks=num_inc;

  if (num_inc>num_in_tracks) {
    for (i=num_in_tracks;i<num_inc;i++) {
      if (!weed_plant_has_leaf(in_channels[i],"disabled")||weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_FALSE) weed_set_boolean_value(in_channels[i],"temp_disabled",WEED_TRUE);
      else weed_set_boolean_value(in_channels[i],"temp_disabled",WEED_FALSE);
    }
  }

  while (layers[lcount++]!=NULL);

  for (i=0;i<num_in_tracks;i++) {
    if (in_tracks[i]<0) {
      weed_free(in_tracks);
      weed_free(out_tracks);
      weed_free(in_channels);
      weed_free(out_channels);
      return FILTER_ERROR_INVALID_TRACK; // probably audio
    }
    channel=in_channels[i];
    weed_set_boolean_value(channel,"temp_disabled",WEED_FALSE);
    if (in_tracks[i]>=lcount) {
      for (j=i;j<num_in_tracks;j++) {
	channel=in_channels[j];
	chantmpl=weed_get_plantptr_value(channel,"template",&error);
	if (weed_plant_has_leaf(chantmpl,"max_repeats")) weed_set_boolean_value(channel,"temp_disabled",WEED_TRUE);
	else {
	  weed_free(in_tracks);
	  weed_free(out_tracks);
	  weed_free(in_channels);
	  weed_free(out_channels);
	  return FILTER_ERROR_MISSING_LAYER;
	}
      }
      break;
    }
    layer=layers[in_tracks[i]];
    if (weed_get_voidptr_value(layer,"pixel_data",&error)==NULL) {
      frame=weed_get_int_value(layer,"frame",&error);
      if (frame==0) {
	// temp disable channels if we can
	chantmpl=weed_get_plantptr_value(channel,"template",&error);
	if (weed_plant_has_leaf(chantmpl,"max_repeats")) weed_set_boolean_value(channel,"temp_disabled",WEED_TRUE);
	else {
	  weed_free(in_tracks);
	  weed_free(out_tracks);
	  weed_free(in_channels);
	  weed_free(out_channels);
	  return FILTER_ERROR_BLANK_FRAME;
	}
      }
    }
  }

  // ensure all chantmpls not marked "optional" have at least one corresponding enabled channel
  // e.g. we could have disabled all channels from a template with "max_repeats" that is not "optional"
  num_ctmpl=weed_leaf_num_elements(filter,"in_channel_templates");
  mand=g_malloc(num_ctmpl*sizint);
  for (j=0;j<num_ctmpl;j++) mand[j]=0;
  in_ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  for (i=0;i<num_inc;i++) {
    if ((weed_plant_has_leaf(in_channels[i],"disabled")&&weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_TRUE)||(weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE)) continue;
    chantmpl=weed_get_plantptr_value(in_channels[i],"template",&error);
    for (j=0;j<num_ctmpl;j++) {
      if (chantmpl==in_ctmpls[j]) {
	mand[j]=1;
	break;
      }
    }
  }
  for (j=0;j<num_ctmpl;j++) if (mand[j]==0&&(!weed_plant_has_leaf(in_ctmpls[j],"optional")||weed_get_boolean_value(in_ctmpls[j],"optional",&error)==WEED_FALSE)) {
    weed_free(in_ctmpls);
    weed_free(in_tracks);
    weed_free(out_tracks);
    weed_free(in_channels);
    weed_free(out_channels);
    g_free(mand);
    return FILTER_ERROR_MISSING_LAYER;
  }
  weed_free(in_ctmpls);
  g_free(mand);
  
  // pull frames for tracks

  for (i=0;i<num_in_tracks;i++) {
    if (weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE) continue;
    layer=layers[in_tracks[i]];
    clip=weed_get_int_value(layer,"clip",&error);

    if (!weed_plant_has_leaf(layer,"pixel_data")||weed_get_voidptr_value(layer,"pixel_data",&error)==NULL) {
      // pull_frame will set pixel_data,width,height,current_palette and rowstrides
      if (!pull_frame(layer,mainw->files[clip]->img_type==IMG_TYPE_JPEG?"jpg":"png",tc)) return FILTER_ERROR_MISSING_FRAME;
    }

    // we only apply transitions and compositors to the scrap file
    if (clip==mainw->scrap_file&&num_in_tracks==1&&num_out_tracks==1) return FILTER_ERROR_IS_SCRAP_FILE;

    // use comparative widths - in RGB(A) pixels
    palette=weed_get_int_value(layer,"current_palette",&error);
    if ((inwidth=(weed_get_int_value(layer,"width",&error)*weed_palette_get_pixels_per_macropixel(palette)))>maxinwidth) maxinwidth=inwidth;
    if ((inheight=weed_get_int_value(layer,"height",&error))>maxinheight) maxinheight=inheight;

  }

  if (maxinwidth<opwidth||opwidth==0) opwidth=maxinwidth;
  if (maxinheight<opheight||opheight==0) opheight=maxinheight;

  // first we resize if necessary; then we change the palette
  for (i=0;i<num_in_tracks;i++) {
    if (weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE) continue;
    
    layer=layers[in_tracks[i]];
    channel=get_enabled_channel(inst,i,TRUE);
    chantmpl=weed_get_plantptr_value(channel,"template",&error);
    
    if (def_channel==NULL) def_channel=channel;

    palette=weed_get_int_value(layer,"current_palette",&error);
    
    width=opwidth;
    height=opheight;
    
    channel_flags=0;
    if (weed_plant_has_leaf(chantmpl,"flags")) channel_flags=weed_get_int_value(chantmpl,"flags",&error);
    
    incwidth=weed_get_int_value(channel,"width",&error);
    incheight=weed_get_int_value(channel,"height",&error);

    inwidth=weed_get_int_value(layer,"width",&error);
    inheight=weed_get_int_value(layer,"height",&error);
    
    if (channel_flags&WEED_CHANNEL_SIZE_CAN_VARY) {
      width=inwidth;
      height=inheight;
    }

    cpalette=weed_get_int_value(channel,"current_palette",&error);

    width/=weed_palette_get_pixels_per_macropixel(cpalette); // width is in macropixels

    // try to set our target width height - the channel may have restrictions
    set_channel_size(channel,width,height,0,NULL);
    width=weed_get_int_value(channel,"width",&error)*weed_palette_get_pixels_per_macropixel(cpalette)/weed_palette_get_pixels_per_macropixel(palette);
    height=weed_get_int_value(channel,"height",&error);

    // restore channel to original size for now
    set_channel_size(channel,incwidth,incheight,0,NULL);

    // check if we need to resize
    if ((inwidth!=width)||(inheight!=height)) {
     // layer needs resizing
      if (prefs->pb_quality==PB_QUALITY_HIGH||opwidth==0||opheight==0) {
	resize_layer(layer,width,height,GDK_INTERP_HYPER);
      }
      else {
	if (prefs->pb_quality==PB_QUALITY_MED) {
	  resize_layer(layer,width,height,GDK_INTERP_BILINEAR);
	}
	else {
	  resize_layer(layer,width,height,GDK_INTERP_NEAREST);
	}
      }

      inwidth=weed_get_int_value(layer,"width",&error);
      inheight=weed_get_int_value(layer,"height",&error);

      if ((inwidth!=width)||(inheight!=height)) {
	weed_free(in_tracks);
	weed_free(out_tracks);
	weed_free(in_channels);
	weed_free(out_channels);
	return FILTER_ERROR_UNABLE_TO_RESIZE;
      }
    }
    
    // check palette again in case it changed during resize
    palette=weed_get_int_value(layer,"current_palette",&error);
    inpalette=weed_get_int_value(channel,"current_palette",&error);
    
    // try to match palettes with first enabled in channel: TODO ** - we should see which palette causes the least palette conversions
    if (i>0&&!(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY)) palette=weed_get_int_value(def_channel,"current_palette",&error);
    
    if (palette!=inpalette) {
      // palette change needed; first try to change channel palette
      int num_palettes=weed_leaf_num_elements(chantmpl,"palette_list");
      int *palettes=weed_get_int_array(chantmpl,"palette_list",&error);
      if (check_weed_palette_list(palettes,num_palettes,palette)==palette) {
	weed_set_int_value(channel,"current_palette",palette);
	if (channel_flags&WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE) needs_reinit=TRUE;
	weed_set_int_value(channel,"width",incwidth/weed_palette_get_pixels_per_macropixel(palette)*weed_palette_get_pixels_per_macropixel(inpalette));
	nchr=weed_leaf_num_elements(channel,"rowstrides");
	channel_rows=weed_get_int_array(channel,"rowstrides",&error);
	for (j=0;j<nchr;j++) {
	  if (weed_palette_get_plane_ratio_horizontal(inpalette,j)!=0.) channel_rows[j]*=weed_palette_get_plane_ratio_horizontal(palette,j)/weed_palette_get_plane_ratio_horizontal(inpalette,j);
	}
	weed_set_int_array(channel,"rowstrides",nchr,channel_rows);
	weed_free(channel_rows);
      }
      weed_free(palettes);
    }
  }

  // now we do a second pass, and we change the palettes of in layers to match the channel, if necessary

  for (i=0;i<num_in_tracks;i++) {
    if (weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE) continue;
    
    layer=layers[in_tracks[i]];
    channel=get_enabled_channel(inst,i,TRUE);
    chantmpl=weed_get_plantptr_value(channel,"template",&error);

    inpalette=weed_get_int_value(channel,"current_palette",&error);

    channel_flags=0;
    if (weed_plant_has_leaf(chantmpl,"flags")) channel_flags=weed_get_int_value(chantmpl,"flags",&error);

    if (weed_plant_has_leaf(chantmpl,"YUV_clamping")) oclamping=(weed_get_int_value(chantmpl,"YUV_clamping",&error));
    else oclamping=WEED_YUV_CLAMPING_CLAMPED;

    if (weed_plant_has_leaf(layer,"YUV_clamping")) iclamping=(weed_get_int_value(layer,"YUV_clamping",&error));
    else iclamping=WEED_YUV_CLAMPING_CLAMPED;

    if (weed_get_int_value(layer,"current_palette",&error)!=inpalette||oclamping!=iclamping) {
      if (!convert_layer_palette(layer,inpalette,oclamping)) {
	weed_free(in_tracks);
	weed_free(out_tracks);
	weed_free(in_channels);
	weed_free(out_channels);
	return FILTER_ERROR_INVALID_PALETTE_CONVERSION;
      }
    }

    if (weed_plant_has_leaf(layer,"YUV_clamping")) {
      oclamping=(weed_get_int_value(layer,"YUV_clamping",&error));
      weed_set_int_value(channel,"YUV_clamping",oclamping);
    }
    else weed_leaf_delete(channel,"YUV_clamping");
    
    if (weed_plant_has_leaf(layer,"YUV_sampling")) weed_set_int_value(channel,"YUV_sampling",weed_get_int_value(layer,"YUV_sampling",&error));
    else weed_leaf_delete(channel,"YUV_sampling");

    if (weed_plant_has_leaf(layer,"YUV_subspace")) weed_set_int_value(channel,"YUV_subspace",weed_get_int_value(layer,"YUV_subspace",&error));
    else weed_leaf_delete(channel,"YUV_subspace");

    incwidth=weed_get_int_value(channel,"width",&error);
    incheight=weed_get_int_value(channel,"height",&error);
    
    nchr=weed_leaf_num_elements(channel,"rowstrides");
    channel_rows=weed_get_int_array(channel,"rowstrides",&error);

    // after all resizing and palette conversions, we set the width, height and rowstrides with their final values

    width=weed_get_int_value(layer,"width",&error);
    height=weed_get_int_value(layer,"height",&error);

    numplanes=weed_leaf_num_elements(layer,"rowstrides");
    rowstrides=weed_get_int_array(layer,"rowstrides",&error);
    
    set_channel_size(channel,width,height,numplanes,rowstrides);
    
    // check layer rowstrides against previous settings
    rowstrides_changed=rowstrides_differ(numplanes,rowstrides,nchr,channel_rows);
    weed_free(channel_rows);

    if (((rowstrides_changed&&(channel_flags&WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE))||(((incwidth!=width)||(incheight!=height))&&(channel_flags&WEED_CHANNEL_REINIT_ON_SIZE_CHANGE)))) needs_reinit=TRUE;
  
    weed_set_int64_value(channel,"timecode",tc);
    pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
 
    // align memory if necessary
    if (weed_plant_has_leaf(chantmpl,"alignment")) {
      int alignment=weed_get_int_value(chantmpl,"alignment",&error);
      align(pixel_data,alignment,numplanes,height,rowstrides);
      weed_set_voidptr_array(layer,"pixel_data",numplanes,pixel_data);
    }

    weed_free(rowstrides);
    weed_set_voidptr_array(channel,"pixel_data",numplanes,pixel_data);
    weed_free(pixel_data);
  }

  // we may need to disable some channels for the plugin
  for (i=0;i<num_in_tracks;i++) {
    if (weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE) weed_set_boolean_value(in_channels[i],"disabled",WEED_TRUE);
  }

  // set up our out channels
  for (i=0;i<num_out_tracks;i++) {
    if (out_tracks[i]<0) {
      weed_free(in_tracks);
      weed_free(out_tracks);
      weed_free(in_channels);
      weed_free(out_channels);
      return FILTER_ERROR_INVALID_TRACK; // probably audio
    }

    channel=get_enabled_channel(inst,i,FALSE);

    outwidth=weed_get_int_value(channel,"width",&error);
    outheight=weed_get_int_value(channel,"height",&error);

    weed_set_int64_value(channel,"timecode",tc);
    outpalette=weed_get_int_value(channel,"current_palette",&error);
    chantmpl=weed_get_plantptr_value(channel,"template",&error);

    channel_flags=0;
    if (weed_plant_has_leaf(chantmpl,"flags")) channel_flags=weed_get_int_value(chantmpl,"flags",&error);

    nchr=weed_leaf_num_elements(channel,"rowstrides");
    channel_rows=weed_get_int_array(channel,"rowstrides",&error);

    if (def_channel!=NULL&&i==0&&(in_tracks[0]==out_tracks[0])) {
      if (channel_flags&WEED_CHANNEL_CAN_DO_INPLACE) {
	// ah, good, inplace
	int num_palettes=weed_leaf_num_elements(chantmpl,"palette_list");
	int *palettes=weed_get_int_array(chantmpl,"palette_list",&error);
	palette=weed_get_int_value(def_channel,"current_palette",&error);
	if (check_weed_palette_list(palettes,num_palettes,palette)==palette) {
	  weed_set_int_value(channel,"current_palette",palette);
	  if (outpalette!=palette&&(channel_flags&WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)) needs_reinit=TRUE;
	  width=weed_get_int_value(def_channel,"width",&error);
	  height=weed_get_int_value(def_channel,"height",&error);
	  weed_set_int_value(channel,"width",width);
	  weed_set_int_value(channel,"height",height);
	  weed_set_int_value(channel,"current_palette",palette);
	  if (weed_plant_has_leaf(def_channel,"YUV_clamping")) {
	    oclamping=(weed_get_int_value(def_channel,"YUV_clamping",&error));
	    weed_set_int_value(channel,"YUV_clamping",oclamping);
	  }
	  else weed_leaf_delete(channel,"YUV_clamping");
	  
	  if (weed_plant_has_leaf(def_channel,"YUV_sampling")) weed_set_int_value(channel,"YUV_sampling",weed_get_int_value(def_channel,"YUV_sampling",&error));
	  else weed_leaf_delete(channel,"YUV_sampling");
	  
	  if (weed_plant_has_leaf(def_channel,"YUV_subspace")) weed_set_int_value(channel,"YUV_subspace",weed_get_int_value(def_channel,"YUV_subspace",&error));
	  else weed_leaf_delete(channel,"YUV_subspace");
	  numplanes=weed_leaf_num_elements(def_channel,"rowstrides");
	  layer_rows=weed_get_int_array(def_channel,"rowstrides",&error);
	  weed_set_int_array(channel,"rowstrides",numplanes,layer_rows);
	  pixel_data=weed_get_voidptr_array(def_channel,"pixel_data",&error);
	  weed_set_voidptr_array(channel,"pixel_data",numplanes,pixel_data);
	  weed_free(pixel_data);
	  weed_set_boolean_value(channel,"inplace",WEED_TRUE);
	  inplace=TRUE;
	}
	weed_free(palettes);
      }
    }

    if (def_channel==NULL) def_channel=get_enabled_channel(inst,0,FALSE);

    if (weed_get_boolean_value(def_channel,"temp_disabled",&error)==WEED_TRUE) def_disabled=TRUE; 

    if (!inplace||def_disabled) {

      if (!def_disabled) {
	// try to match palettes with first enabled in channel
	palette=weed_get_int_value(def_channel,"current_palette",&error);
	
	if (palette!=outpalette) {
	  // palette change needed; try to change channel palette
	  int num_palettes=weed_leaf_num_elements(chantmpl,"palette_list");
	  int *palettes=weed_get_int_array(chantmpl,"palette_list",&error);
	  if (check_weed_palette_list(palettes,num_palettes,palette)==palette) {
	    weed_set_int_value(channel,"current_palette",palette);
	    if (channel_flags&WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE) needs_reinit=TRUE;
	  }
	  else {
	    weed_free(in_tracks);
	    weed_free(out_tracks);
	    weed_free(in_channels);
	    weed_free(out_channels);
	    weed_free(channel_rows);
	    return FILTER_ERROR_INVALID_PALETTE_SETTINGS; // plugin author messed up...
	  }
	  weed_free(palettes);
	}
	
	if (weed_plant_has_leaf(def_channel,"YUV_clamping")) {
	  oclamping=(weed_get_int_value(def_channel,"YUV_clamping",&error));
	  weed_set_int_value(channel,"YUV_clamping",oclamping);
	}
	else weed_leaf_delete(channel,"YUV_clamping");
	
	if (weed_plant_has_leaf(def_channel,"YUV_sampling")) weed_set_int_value(channel,"YUV_sampling",weed_get_int_value(def_channel,"YUV_sampling",&error));
	else weed_leaf_delete(channel,"YUV_sampling");
	
	if (weed_plant_has_leaf(def_channel,"YUV_subspace")) weed_set_int_value(channel,"YUV_subspace",weed_get_int_value(def_channel,"YUV_subspace",&error));
	else weed_leaf_delete(channel,"YUV_subspace");
      }


      palette=weed_get_int_value(channel,"current_palette",&error);

      set_channel_size(channel,opwidth/weed_palette_get_pixels_per_macropixel(palette),opheight,1,NULL);

      create_empty_pixel_data(channel); // this will look at width, height, current_palette, and create an empty pixel_data and set rowstrides
      // and update width and height if necessary

      numplanes=weed_leaf_num_elements(channel,"rowstrides");
      layer_rows=weed_get_int_array(channel,"rowstrides",&error);
      // align memory if necessary
      if (weed_plant_has_leaf(chantmpl,"alignment")) {
	int alignment=weed_get_int_value(chantmpl,"alignment",&error);
	pixel_data=weed_get_voidptr_array(channel,"pixel_data",&error);
	height=weed_get_int_value(channel,"height",&error);
	align(pixel_data,alignment,numplanes,height,layer_rows);
	weed_set_voidptr_array(channel,"pixel_data",numplanes,pixel_data);
	weed_free(pixel_data);
      }
      weed_set_boolean_value(channel,"inplace",WEED_FALSE);
    }

    // check old rowstrides against current rowstrides
    rowstrides_changed=rowstrides_differ(nchr,channel_rows,numplanes,layer_rows);

    weed_free(channel_rows);
    weed_free(layer_rows);
      
    width=weed_get_int_value(channel,"width",&error);
    height=weed_get_int_value(channel,"height",&error);
    if ((rowstrides_changed&&(channel_flags&WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE))||(((outwidth!=width)||(outheight!=height))&&(channel_flags&WEED_CHANNEL_REINIT_ON_SIZE_CHANGE))) needs_reinit=TRUE;
  }

  if (needs_reinit) if ((retval=weed_reinit_effect(inst))==FILTER_ERROR_COULD_NOT_REINIT) {
    weed_free(in_tracks);
    weed_free(out_tracks);
    weed_free(in_channels);
    weed_free(out_channels);
    return retval;
  }

  weed_set_double_value(inst,"fps",cfile->pb_fps);

  //...finally we are ready to apply the filter
  weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
  process_func=process_func_ptr_ptr[0];
  if ((*process_func)(inst,tc)==WEED_ERROR_PLUGIN_INVALID) {
    weed_free(in_tracks);
    weed_free(out_tracks);
    weed_free(in_channels);
    weed_free(out_channels);
    return FILTER_ERROR_MUST_RELOAD;
  }

  // TODO - handle process errors (WEED_ERROR_PLUGIN_INVALID)

  // now we write our out channels back to layers, leaving the palettes and sizes unchanged
  for (i=0;i<num_out_tracks;i++) {
    channel=get_enabled_channel(inst,i,FALSE);
    if (weed_get_boolean_value(channel,"inplace",&error)==WEED_TRUE) continue;
    layer=layers[out_tracks[i]];
    numplanes=weed_leaf_num_elements(channel,"rowstrides");
    rowstrides=weed_get_int_array(channel,"rowstrides",&error);
    weed_set_int_array(layer,"rowstrides",numplanes,rowstrides);
    weed_free(rowstrides);
    numplanes=weed_leaf_num_elements(layer,"pixel_data");
    pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
    for (j=0;j<numplanes;j++) g_free(pixel_data[j]);
    weed_free(pixel_data);
    numplanes=weed_leaf_num_elements(channel,"pixel_data");
    pixel_data=weed_get_voidptr_array(channel,"pixel_data",&error);
    weed_set_voidptr_array(layer,"pixel_data",numplanes,pixel_data);
    weed_free(pixel_data);

    weed_set_int_value(layer,"current_palette",weed_get_int_value(channel,"current_palette",&error));
    weed_set_int_value(layer,"width",weed_get_int_value(channel,"width",&error));
    weed_set_int_value(layer,"height",weed_get_int_value(channel,"height",&error));

    if (weed_plant_has_leaf(channel,"YUV_clamping")) {
      oclamping=(weed_get_int_value(channel,"YUV_clamping",&error));
      weed_set_int_value(layer,"YUV_clamping",oclamping);
    }
    else weed_leaf_delete(layer,"YUV_clamping");
    
    if (weed_plant_has_leaf(channel,"YUV_sampling")) weed_set_int_value(layer,"YUV_sampling",weed_get_int_value(channel,"YUV_sampling",&error));
    else weed_leaf_delete(layer,"YUV_sampling");
    
    if (weed_plant_has_leaf(channel,"YUV_subspace")) weed_set_int_value(layer,"YUV_subspace",weed_get_int_value(channel,"YUV_subspace",&error));
    else weed_leaf_delete(layer,"YUV_subspace");
  }

  for (i=0;i<num_inc;i++) {
    if (weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE) {
      weed_set_boolean_value(in_channels[i],"disabled",WEED_FALSE);
      weed_set_boolean_value(in_channels[i],"temp_disabled",WEED_FALSE);
    }
  }
  // done...

  weed_free(in_tracks);
  weed_free(out_tracks);
  weed_free(in_channels);
  weed_free(out_channels);

  return retval;
}



static gint weed_apply_audio_instance_inner (weed_plant_t *inst, weed_plant_t *init_event, weed_plant_t **layers, weed_timecode_t tc, int nbtracks) {
  int num_in_tracks,num_out_tracks;
  int *in_tracks,*out_tracks;
  int error,i,j;
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);
  weed_plant_t *layer;
  weed_plant_t **in_channels,**out_channels,*channel,*chantmpl;
  //gboolean needs_reinit=FALSE;
  gboolean inplace=FALSE;
  weed_process_f *process_func_ptr_ptr;
  weed_process_f process_func;
  weed_plant_t *def_channel=NULL;
  gint retval=FILTER_NO_ERROR;
  int *mand;
  int channel_flags;

  int num_ctmpl,num_inc;
  weed_plant_t **in_ctmpls;

  int nchans=0;
  int nsamps=0;
  void *adata=NULL;

  if (!weed_plant_has_leaf(inst,"in_channels")||(in_channels=weed_get_plantptr_array(inst,"in_channels",&error))==NULL) return FILTER_ERROR_NO_IN_CHANNELS;

  if (get_enabled_channel(inst,0,TRUE)==NULL) {
    // we process generators elsewhere
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (init_event==NULL) {
    num_in_tracks=enabled_in_channels(inst,FALSE);
    in_tracks=weed_malloc(2*sizint);
    in_tracks[0]=0;
    in_tracks[1]=1;
    num_out_tracks=1;
    out_tracks=weed_malloc(sizint);
    out_tracks[0]=0;
  }
  else {
    num_in_tracks=weed_leaf_num_elements(init_event,"in_tracks");
    in_tracks=weed_get_int_array(init_event,"in_tracks",&error);
    num_out_tracks=weed_leaf_num_elements(init_event,"out_tracks");
    out_tracks=weed_get_int_array(init_event,"out_tracks",&error);
  }

  out_channels=weed_get_plantptr_array(inst,"out_channels",&error);

  // handle case where in_tracks[i] > than num layers
  // either we temporarily disable the channel, or we can't apply the filter
  num_inc=weed_leaf_num_elements(inst,"in_channels");

  if (num_in_tracks>num_inc) num_in_tracks=num_inc;

  if (num_inc>num_in_tracks) {
    for (i=num_in_tracks;i<num_inc;i++) {
      if (!weed_plant_has_leaf(in_channels[i],"disabled")||weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_FALSE) weed_set_boolean_value(in_channels[i],"temp_disabled",WEED_TRUE);
      else weed_set_boolean_value(in_channels[i],"temp_disabled",WEED_FALSE);
    }
  }

  for (i=0;i<num_in_tracks;i++) {
    channel=in_channels[i];
    weed_set_boolean_value(channel,"temp_disabled",WEED_FALSE);
    layer=layers[in_tracks[i]+nbtracks];
    if (layer==NULL) {
      for (j=i;j<num_in_tracks;j++) {
	channel=in_channels[j];
	chantmpl=weed_get_plantptr_value(channel,"template",&error);
	if (weed_plant_has_leaf(chantmpl,"max_repeats")) weed_set_boolean_value(channel,"temp_disabled",WEED_TRUE);
	else {
	  weed_free(in_tracks);
	  weed_free(out_tracks);
	  weed_free(in_channels);
	  weed_free(out_channels);
	  return FILTER_ERROR_MISSING_LAYER;
	}
      }
      break;
    }
    if (weed_get_voidptr_value(layer,"audio_data",&error)==NULL) {
      chantmpl=weed_get_plantptr_value(channel,"template",&error);
      if (weed_plant_has_leaf(chantmpl,"max_repeats")) weed_set_boolean_value(channel,"temp_disabled",WEED_TRUE);
    }
  }

  // ensure all chantmpls not marked "optional" have at least one corresponding enabled channel
  // e.g. we could have disabled all channels from a template with "max_repeats" that is not "optional"
  num_ctmpl=weed_leaf_num_elements(filter,"in_channel_templates");
  mand=g_malloc(num_ctmpl*sizint);
  for (j=0;j<num_ctmpl;j++) mand[j]=0;
  in_ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  for (i=0;i<num_inc;i++) {
    if ((weed_plant_has_leaf(in_channels[i],"disabled")&&weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_TRUE)||(weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE)) continue;
    chantmpl=weed_get_plantptr_value(in_channels[i],"template",&error);
    for (j=0;j<num_ctmpl;j++) {
      if (chantmpl==in_ctmpls[j]) {
	mand[j]=1;
	break;
      }
    }
  }
  for (j=0;j<num_ctmpl;j++) if (mand[j]==0&&(!weed_plant_has_leaf(in_ctmpls[j],"optional")||weed_get_boolean_value(in_ctmpls[j],"optional",&error)==WEED_FALSE)) {
    weed_free(in_ctmpls);
    weed_free(in_tracks);
    weed_free(out_tracks);
    weed_free(in_channels);
    weed_free(out_channels);
    g_free(mand);
    return FILTER_ERROR_MISSING_LAYER;
  }
  weed_free(in_ctmpls);
  g_free(mand);

  for (i=0;i<num_in_tracks;i++) {
    if (weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE) continue;

    layer=layers[in_tracks[i]+nbtracks];
    channel=get_enabled_channel(inst,i,TRUE);
    chantmpl=weed_get_plantptr_value(channel,"template",&error);

    weed_set_int64_value(channel,"timecode",tc);
    adata=weed_get_voidptr_value(layer,"audio_data",&error);

    // nchans and nsamps needed for inplace
    nchans=weed_get_int_value(channel,"audio_channels",&error);
    nsamps=weed_get_int_value(channel,"audio_data_length",&error);
    weed_set_voidptr_value(channel,"audio_data",adata);
  }

  // we may need to disable some channels for the plugin
  for (i=0;i<num_in_tracks;i++) {
    if (weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE) weed_set_boolean_value(in_channels[i],"disabled",WEED_TRUE);
  }

  // set up our out channels
  for (i=0;i<num_out_tracks;i++) {
    if (out_tracks[i]>(nbtracks>0?-1:0)) {
      weed_free(in_tracks);
      weed_free(out_tracks);
      weed_free(in_channels);
      weed_free(out_channels);
      return FILTER_ERROR_INVALID_TRACK; // can't yet mix audio and video
      // as a special exception, we allow our mixer to generate audio to layer 0, if and 
      // only if we have no backing audio
    }

    channel=get_enabled_channel(inst,i,FALSE);

    weed_set_int64_value(channel,"timecode",tc);
    chantmpl=weed_get_plantptr_value(channel,"template",&error);
    channel_flags=weed_get_int_value(chantmpl,"flags",&error);
    
    if (def_channel!=NULL&&i==0&&(in_tracks[0]==out_tracks[0])) {
      if (channel_flags&WEED_CHANNEL_CAN_DO_INPLACE) {
	// ah, good, inplace
	weed_set_boolean_value(channel,"inplace",WEED_TRUE);
	inplace=TRUE;
      }
    }

    if (!inplace) {
      float *abuf=g_malloc(nchans*nsamps*sizeof(float));
      weed_set_int_value(channel,"audio_data_length",nsamps);
      weed_set_voidptr_value(channel,"audio_data",abuf);
    }
    else {
      weed_set_int_value(channel,"audio_data_length",nsamps);
      weed_set_voidptr_value(channel,"audio_data",adata);
    }
    weed_set_boolean_value(channel,"inplace",WEED_FALSE);
  }

  weed_set_double_value(inst,"fps",cfile->pb_fps);

  //...finally we are ready to apply the filter
  pthread_mutex_lock(&mainw->interp_mutex); // stop video thread from possibly interpolating our audio effects
  weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
  process_func=process_func_ptr_ptr[0];

  if ((*process_func)(inst,tc)==WEED_ERROR_PLUGIN_INVALID) {
    pthread_mutex_unlock(&mainw->interp_mutex);
    weed_free(in_tracks);
    weed_free(out_tracks);
    weed_free(in_channels);
    weed_free(out_channels);
    return FILTER_ERROR_MUST_RELOAD;
  }
  pthread_mutex_unlock(&mainw->interp_mutex);

  // TODO - handle process errors (WEED_ERROR_PLUGIN_INVALID)

  // now we write our out channels back to layers, leaving the palettes and sizes unchanged
  for (i=0;i<num_out_tracks;i++) {
    channel=get_enabled_channel(inst,i,FALSE);
    if (weed_get_boolean_value(channel,"inplace",&error)==WEED_TRUE) continue;
    layer=layers[out_tracks[i]+nbtracks];
    if (weed_plant_has_leaf(channel,"audio_data")) {
      float *audio_data=(float *)weed_get_voidptr_value(layer,"audio_data",&error);
      if (audio_data!=NULL) weed_free(audio_data);
    }
    weed_set_voidptr_value(layer,"audio_data",weed_get_voidptr_value(channel,"audio_data",&error));
  }

  for (i=0;i<num_inc;i++) {
    if (weed_plant_has_leaf(in_channels[i],"temp_disabled")&&weed_get_boolean_value(in_channels[i],"temp_disabled",&error)==WEED_TRUE) {
      weed_set_boolean_value(in_channels[i],"disabled",WEED_FALSE);
      weed_set_boolean_value(in_channels[i],"temp_disabled",WEED_FALSE);
    }
  }
  // done...

  weed_free(in_tracks);
  weed_free(out_tracks);
  weed_free(in_channels);
  weed_free(out_channels);


  return retval;

}



gint weed_apply_audio_instance (weed_plant_t *init_event, float **abuf, int nbtracks, int nchans, long nsamps, gdouble arate, weed_timecode_t tc, double *vis) {
  void *in_abuf,*out_abuf;
  int i,j,error;
  weed_plant_t **layers;
  weed_plant_t *instance,*filter;
  size_t nsf=nsamps*sizeof(float);
  void **ev_pchains;
  gint retval=FILTER_NO_ERROR;
  weed_plant_t *channel=NULL;
  int aint=WEED_FALSE;               // use non-interlaced - TODO
  gboolean can_reinit=TRUE;
  weed_plant_t *ctmpl;

  int flags=0;

  int xnsamps,xnchans,xarate,xaint;

  int ntracks=weed_leaf_num_elements(init_event,"in_tracks");

  // check instance exists, and interpolate parameters
  if (weed_plant_has_leaf(init_event,"host_tag")) {

    gchar *keystr=weed_get_string_value(init_event,"host_tag",&error);
    int key=atoi(keystr);
    weed_free(keystr);
    if (rte_key_valid (key+1,FALSE)) {
      if (key_to_instance[key][key_modes[key]]==-1) return FILTER_ERROR_INVALID_INSTANCE;
      instance=weed_instances[key_to_instance[key][key_modes[key]]];
      if (instance==NULL) return FILTER_ERROR_INVALID_INSTANCE;
      if (weed_plant_has_leaf(init_event,"in_parameters")) {
	ev_pchains=weed_get_voidptr_array(init_event,"in_parameters",&error);
	if (ev_pchains[0]!=NULL) {
	  if (!pthread_mutex_trylock(&mainw->interp_mutex)) { // try to minimise thread locking
	    pthread_mutex_unlock(&mainw->interp_mutex);
	    if (!interpolate_params(instance,ev_pchains,tc)) {
	      weed_free(ev_pchains);
	      return FILTER_ERROR_INTERPOLATION_FAILED;
	    }
	  }
	}
	weed_free(ev_pchains);
      }
    }
    else return FILTER_ERROR_INVALID_FILTER;
  }
  else return FILTER_ERROR_INVALID_INIT_EVENT;

  if (weed_plant_has_leaf(instance,"in_channels")) channel=weed_get_plantptr_value(instance,"in_channels",&error);
  if (channel==NULL) channel=weed_get_plantptr_value(instance,"out_channels",&error);

  filter=weed_get_plantptr_value(instance,"filter_class",&error);

  if (nsamps!=weed_get_int_value(channel,"audio_data_length",&error)||nchans!=weed_get_int_value(channel,"audio_channels",&error)||arate!=weed_get_int_value(channel,"audio_rate",&error)||weed_get_boolean_value(channel,"audio_interleaf",&error)!=aint) {
    // audio mismatch with channel values - adjust channels or resample audio

    // TODO - check if we can reinit
    weed_plant_t **in_channels,**out_channels;
    int numchans;

    // reset channel values
    in_channels=weed_get_plantptr_array(instance,"in_channels",&error);
    numchans=weed_leaf_num_elements(instance,"in_channels");
    for (i=0;i<numchans;i++) {
      if ((channel=in_channels[i])!=NULL) {
	ctmpl=weed_get_plantptr_value(channel,"template",&error);
	if ((weed_plant_has_leaf(ctmpl,"audio_data_length")&&weed_get_int_value(ctmpl,"audio_data_length",&error)!=nsamps)||(weed_plant_has_leaf(ctmpl,"audio_channels")&&weed_get_int_value(ctmpl,"audio_channels",&error)!=nchans)||(weed_plant_has_leaf(ctmpl,"audio_rate")&&weed_get_int_value(ctmpl,"audio_rate",&error)!=arate)||(weed_plant_has_leaf(ctmpl,"audio_interleaf")&&weed_get_boolean_value(channel,"audio_interleaf",&error)!=aint)) {
	  can_reinit=FALSE;
	  xnsamps=weed_get_int_value(channel,"audio_data_length",&error);
	  xnchans=weed_get_int_value(channel,"audio_channels",&error);
	  xarate=weed_get_int_value(channel,"audio_rate",&error);
	  xaint=weed_get_boolean_value(channel,"audio_interleaf",&error);
	  break;
	}
      }
    }
    weed_free(in_channels);
      
    if (can_reinit) {
      out_channels=weed_get_plantptr_array(instance,"out_channels",&error);
      numchans=weed_leaf_num_elements(instance,"out_channels");
      for (i=0;i<numchans;i++) {
	if ((channel=out_channels[i])!=NULL) {
	  ctmpl=weed_get_plantptr_value(channel,"template",&error);
	  if ((weed_plant_has_leaf(ctmpl,"audio_data_length")&&weed_get_int_value(ctmpl,"audio_data_length",&error)!=nsamps)||(weed_plant_has_leaf(ctmpl,"audio_channels")&&weed_get_int_value(ctmpl,"audio_channels",&error)!=nchans)||(weed_plant_has_leaf(ctmpl,"audio_rate")&&weed_get_int_value(ctmpl,"audio_rate",&error)!=arate)||(weed_plant_has_leaf(ctmpl,"audio_interleaf")&&weed_get_boolean_value(channel,"audio_interleaf",&error)!=aint)) {
	    can_reinit=FALSE;
	    xnsamps=weed_get_int_value(channel,"audio_data_length",&error);
	    xnchans=weed_get_int_value(channel,"audio_channels",&error);
	    xarate=weed_get_int_value(channel,"audio_rate",&error);
	    xaint=weed_get_boolean_value(channel,"audio_interleaf",&error);
	    break;
	  }
	}
	weed_free(out_channels);
      }
    }

    if (can_reinit) {
      // - deinit inst
      weed_call_deinit_func(instance);

      // reset channel values
      in_channels=weed_get_plantptr_array(instance,"in_channels",&error);
      numchans=weed_leaf_num_elements(instance,"in_channels");
      for (i=0;i<numchans;i++) {
	if ((channel=in_channels[i])!=NULL) {
	  weed_set_int_value(channel,"audio_data_length",nsamps);
	  weed_set_int_value(channel,"audio_channels",nchans);
	  weed_set_int_value(channel,"audio_rate",arate);
	  weed_set_boolean_value(channel,"audio_interleaf",aint);
	}
      }
      weed_free(in_channels);
      
      out_channels=weed_get_plantptr_array(instance,"out_channels",&error);
      numchans=weed_leaf_num_elements(instance,"out_channels");
      for (i=0;i<numchans;i++) {
	if ((channel=out_channels[i])!=NULL) {
	  weed_set_int_value(channel,"audio_data_length",nsamps);
	  weed_set_int_value(channel,"audio_channels",nchans);
	  weed_set_int_value(channel,"audio_rate",arate);
	  weed_set_boolean_value(channel,"audio_interleaf",aint);
	}
      }
      weed_free(out_channels);
      
      // - init inst
      if (weed_plant_has_leaf(filter,"init_func")) {
	weed_init_f *init_func_ptr_ptr;
	weed_init_f init_func;
	weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
	init_func=init_func_ptr_ptr[0];
	if (init_func!=NULL) {
	  set_param_gui_readwrite(instance);
	  update_host_info(instance);
	  if ((*init_func)(instance)!=WEED_NO_ERROR) {
	    weed_instances[key_to_instance[key][key_modes[key]]]=NULL;
	    key_to_instance[key][key_modes[key]]=-1;
	    return FILTER_ERROR_COULD_NOT_REINIT;
	  }
	  set_param_gui_readonly(instance);
	}
      }
      retval=FILTER_INFO_REINITED;
    }
    else {
      // else resample if we can - TODO
    }
  }

  if (!weed_plant_has_leaf(filter,"flags")) weed_set_int_value(filter,"flags",0);
  else flags=weed_get_int_value(filter,"flags",&error);

  // apply visibility mask to volume values
  if (vis!=NULL&&(flags&WEED_FILTER_IS_CONVERTER)) {
    int vmaster=get_master_vol_param(filter);
    if (vmaster!=-1) {
      weed_plant_t **in_params=weed_get_plantptr_array(instance,"in_parameters",&error);
      int nvals=weed_leaf_num_elements(in_params[vmaster],"value");
      double *fvols=weed_get_double_array(in_params[vmaster],"value",&error);
      int *in_tracks=weed_get_int_array(init_event,"in_tracks",&error);
      for (i=0;i<nvals;i++) {
	fvols[i]=fvols[i]*vis[in_tracks[i]+nbtracks];
      }
      weed_set_double_array(in_params[vmaster],"value",nvals,fvols);
      weed_free(fvols);
      weed_free(in_params);
      weed_free(in_tracks);
    }
  }

  layers=g_malloc((ntracks+1)*sizeof(weed_plant_t *));

  for (i=0;i<ntracks;i++) {
    layers[i]=weed_plant_new(WEED_PLANT_CHANNEL);
    in_abuf=g_malloc(nchans*nsf);
    for (j=0;j<nchans;j++) {
      w_memcpy((char *)in_abuf+(j*nsf),abuf[i*nchans+j],nsf);
    }
    weed_set_voidptr_value(layers[i],"audio_data",in_abuf);
    weed_set_int_value(layers[i],"audio_data_length",nsamps);
    weed_set_int_value(layers[i],"audio_channels",nchans);
    weed_set_int_value(layers[i],"audio_rate",arate);
    weed_set_boolean_value(layers[i],"audio_interleaf",WEED_FALSE);
  }

  layers[i]=NULL;

  weed_apply_audio_instance_inner(instance,init_event,layers,tc,nbtracks);

  out_abuf=weed_get_voidptr_value(layers[0],"audio_data",&error);

  if (!can_reinit) {
    // resample back


  }

  for (i=0;i<nchans;i++) {
    w_memcpy(abuf[i],(char *)out_abuf+(i*nsf),nsf);
  }

  for (i=0;i<ntracks;i++) {
    in_abuf=weed_get_voidptr_value(layers[i],"audio_data",&error);
    g_free(in_abuf);

    weed_plant_free(layers[i]);
  }
  g_free(layers);

  return retval;
}


static void weed_apply_filter_map (weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc, void ***pchains) {
  int i,key,num_inst,error;
  weed_plant_t *instance;
  gchar *keystr;
  void **init_events;
  int filter_error;
  weed_plant_t *init_event,*deinit_event;

  // this is called during rendering - we will have previously received a filter_map event and now we apply this to layers


  // layers will be a NULL terminated array of channels, each with two extra leaves: clip and frame
  // clip corresponds to a LiVES file in mainw->files. "pixel_data" will initially be NULL, we will pull this as necessary
  // and the effect output is written back to the layers

  // a frame number of 0 indicates a blank frame; if an effect gets one of these it will not process (except if the channel is optional, in which case the channel is disabled); the same is true for invalid track numbers - hence disabled channels which do not have disabled in the template are re-enabled when a frame is available

  // after all processing, we generally display/output the first non-NULL layer. If all layers are NULL we generate a blank frame

  // size and palettes of resulting layers may change during this

  // the channel sizes are set by the filter: all channels are all set to the size of the largest input layer. (We attempt to do this, but some channels have fixed sizes).

  if (filter_map==NULL||!weed_plant_has_leaf(filter_map,"init_events")||(weed_get_voidptr_value(filter_map,"init_events",&error)==NULL)) return;
  if ((num_inst=weed_leaf_num_elements(filter_map,"init_events"))>0) {
    init_events=weed_get_voidptr_array(filter_map,"init_events",&error);
    for (i=0;i<num_inst;i++) {
      init_event=init_events[i];
      if (mainw->playing_file==-1&&mainw->multitrack!=NULL&&mainw->multitrack->current_rfx!=NULL&&mainw->multitrack->init_event!=NULL) {
	deinit_event=(weed_plant_t *)weed_get_voidptr_value(mainw->multitrack->init_event,"deinit_event",&error);
	if (tc>=get_event_timecode(mainw->multitrack->init_event)&&tc<=get_event_timecode(deinit_event)) {
	  // we are previewing an effect in multitrack - use current unapplied values
	  // and display only the currently selected filter
	  void **pchain=mt_get_pchain();
	  instance=mainw->multitrack->current_rfx->source;
	  // interpolation can be switched of by setting mainw->no_interp
	  if (!mainw->no_interp&&pchain!=NULL) {
	    interpolate_params(instance,pchain,tc); // interpolate parameters for preview
	  }
	  filter_error=weed_apply_instance (instance,mainw->multitrack->init_event,layers,0,0,tc);
	  break;
	}
      }

      if (weed_plant_has_leaf(init_event,"host_tag")) {
	keystr=weed_get_string_value(init_event,"host_tag",&error);
	key=atoi(keystr);
	weed_free(keystr);
	if (rte_key_valid (key+1,FALSE)) {
	  if (key_to_instance[key][key_modes[key]]==-1) continue;
	  instance=weed_instances[key_to_instance[key][key_modes[key]]];
	  if (instance==NULL) continue;
	  if (pchains!=NULL&&pchains[key]!=NULL) {
	    interpolate_params(instance,pchains[key],tc); // interpolate parameters during playback
	  }
	  filter_error=weed_apply_instance (instance,(weed_plant_t *)init_events[i],layers,0,0,tc);
	  //if (filter_error!=FILTER_NO_ERROR) g_printerr("Render error was %d\n",filter_error);
	}
      }
    }
    weed_free(init_events);
  }
}




weed_plant_t *weed_apply_effects (weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc, int opwidth, int opheight, void ***pchains) {
  // given a stack of layers, a filter map, a timecode and possibly paramater chains
  // apply the effects in the filter map, and return a single layer as the result

  // if all goes wrong we return a blank 4x4 RGB24 layer (TODO - return a NULL ?) 

  // returned layer can be of any width,height,palette
  // caller should free all input layers, "pixel_data" of all non-returned layers is free()d here

  // TODO - update the param window with any out_param values which changed.

  int i,error;
  weed_plant_t *instance,*layer;
  int filter_error;
  int output=-1;
  int clip;
  void *pdata;
  gboolean got_pdata=TRUE;

  if (mainw->is_rendering&&!(cfile->proc_ptr!=NULL&&mainw->preview)) {
    // rendering from multitrack
    if (filter_map!=NULL&&layers[0]!=NULL) {
      weed_apply_filter_map(layers, filter_map, tc, pchains);
    }
  }

  // free playback: we will have here only one or two layers, and no filter_map.
  // Effects are applied in key order, in tracks are 0 and 1, out track is 0
  else {
    for (i=0;i<FX_KEYS_MAX_VIRTUAL;i++) {
      if (rte_key_valid(i+1,TRUE)) {
	if (mainw->rte&(GU641<<i)) {
	  mainw->osc_block=TRUE;
	  if (key_to_instance[i][key_modes[i]]==-1) continue;
	  instance=weed_instances[key_to_instance[i][key_modes[i]]];
	  if (instance==NULL) continue;
	  if (mainw->pchains!=NULL&&mainw->pchains[key]!=NULL) {
	    interpolate_params(instance,mainw->pchains[key],tc); // interpolate parameters during preview
	  }
	  filter_error=weed_apply_instance (instance,NULL,layers,opwidth,opheight,tc);
	  if (filter_error==FILTER_INFO_REINITED) close_pwindow(i,key_modes[i],TRUE); // redraw our paramwindow
#ifdef DEBUG_RTE
	  if (filter_error!=FILTER_NO_ERROR) g_printerr("Render error was %d\n",filter_error);
#endif
	}
      }
    }
  }
  if (!mainw->is_rendering) mainw->osc_block=FALSE;

  // caller should free all layers, but here we will free all other pixel_data

  for (i=0;layers[i]!=NULL;i++) {
    if ((pdata=weed_get_voidptr_value(layers[i],"pixel_data",&error))!=NULL||(weed_get_int_value(layers[i],"frame",&error)!=0&&(mainw->playing_file>-1||mainw->multitrack==NULL||mainw->multitrack->current_rfx==NULL||(mainw->multitrack->init_event==NULL||tc<get_event_timecode(mainw->multitrack->init_event)||tc>get_event_timecode((weed_plant_t *)weed_get_voidptr_value(mainw->multitrack->init_event,"deinit_event",&error)))))) {
      if (output!=-1) {
	void **pixel_data=weed_get_voidptr_array(layers[i],"pixel_data",&error);
	if (pixel_data!=NULL) {
	  int j;
	  int numplanes=weed_leaf_num_elements(layers[i],"pixel_data");
	  for (j=0;j<numplanes;j++) if (pixel_data[j]!=NULL) g_free(pixel_data[j]);
	  weed_free(pixel_data);
	}
	else got_pdata=FALSE;
      }
      else output=i;
    }
  }

  if (output==-1||!got_pdata) {
    weed_plant_t *layer=weed_layer_new(opwidth>4?opwidth:4,opheight>4?opheight:4,NULL,WEED_PALETTE_RGB24);
    create_empty_pixel_data(layer);
    return layer;
  }

  layer=layers[output];
  clip=weed_get_int_value(layer,"clip",&error);

  // frame is pulled uneffected here. TODO: Try to pull at target output palette
  if (!weed_plant_has_leaf(layer,"pixel_data")||weed_get_voidptr_value(layer,"pixel_data",&error)==NULL) if (!pull_frame_at_size(layer,mainw->files[clip]->img_type==IMG_TYPE_JPEG?"jpg":"png",tc,opwidth,opheight,WEED_PALETTE_END)) {
      weed_set_int_value(layer,"current_palette",mainw->files[clip]->img_type==IMG_TYPE_JPEG?WEED_PALETTE_RGB24:WEED_PALETTE_RGBA32);
      weed_set_int_value(layer,"width",opwidth);
      weed_set_int_value(layer,"height",opheight);
      create_empty_pixel_data(layer);
      g_printerr("weed_apply_effects created empty pixel_data\n");
    }
  
  return layer;
}




void weed_apply_audio_effects (weed_plant_t *filter_map, float **abuf, int nbtracks, int nchans, long nsamps, gdouble arate, weed_timecode_t tc, double *vis) {
  int i,num_inst,error;
  void **init_events;
  int filter_error;
  weed_plant_t *init_event,*filter;
  gchar *fhash;

  // this is called during rendering - we will have previously received a filter_map event and now we apply this to audio (abuf)
  // abuf will be a NULL terminated array of float audio

  // the results of abuf[0] and abuf[1] (for stereo) will be written to fileno

  if (filter_map==NULL||!weed_plant_has_leaf(filter_map,"init_events")||(weed_get_voidptr_value(filter_map,"init_events",&error)==NULL)) {
    return;
  }
  if ((num_inst=weed_leaf_num_elements(filter_map,"init_events"))>0) {
    init_events=weed_get_voidptr_array(filter_map,"init_events",&error);
    for (i=0;i<num_inst;i++) {
      init_event=init_events[i];
      fhash=weed_get_string_value(init_event,"filter",&error);
      filter=get_weed_filter(weed_get_idx_for_hashname(fhash,TRUE));
      weed_free(fhash);
      if (has_audio_chans_in(filter,FALSE)) filter_error=weed_apply_audio_instance(init_event,abuf,nbtracks,nchans,nsamps,arate,tc,vis);
    }
    weed_free(init_events);
  }
}







/////////////////////////////////////////////////////////////////////////


static int check_weed_plugin_info (weed_plant_t *plugin_info) {
  // verify the plugin_info returned from the plugin
  // TODO - print descriptive errors
  weed_plant_t *host_info;
  int error;

  if (!weed_plant_has_leaf(plugin_info,"host_info")) return -1;
  if (!weed_plant_has_leaf(plugin_info,"version")) return -2;
  if (!weed_plant_has_leaf(plugin_info,"filters")) return -3;
  host_info=weed_get_plantptr_value(plugin_info,"host_info",&error);

  return weed_leaf_num_elements(plugin_info,"filters");
}


gint num_in_params(weed_plant_t *plant, gboolean count_reinits, gboolean count_variable) {
  weed_plant_t **params=NULL;
  gint counted=0;
  int num_params,i,error;
  gboolean is_template=(WEED_PLANT_IS_FILTER_CLASS(plant));
  int flags;
  weed_plant_t *ptmpl;

  if (is_template) {
    if (!weed_plant_has_leaf(plant,"in_parameter_templates")||weed_get_plantptr_value(plant,"in_parameter_templates",&error)==NULL) return 0;
    num_params=weed_leaf_num_elements(plant,"in_parameter_templates");
  }
  else {
    if (!weed_plant_has_leaf(plant,"in_parameters")) return 0;
    if (weed_get_plantptr_value(plant,"in_parameters",&error)==NULL) return 0;
    num_params=weed_leaf_num_elements(plant,"in_parameters");
  }
  if (count_reinits&&count_variable) return num_params;

  if (is_template) params=weed_get_plantptr_array(plant,"in_parameter_templates",&error);
  else params=weed_get_plantptr_array(plant,"in_parameters",&error);

  for (i=0;i<num_params;i++) {
    if (is_template) ptmpl=params[i];
    else ptmpl=weed_get_plantptr_value(params[i],"template",&error);
    if (weed_plant_has_leaf(ptmpl,"flags")) {
      flags=weed_get_int_value(ptmpl,"flags",&error);
      if ((count_reinits||!(flags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE))&&(count_variable||!(flags&WEED_PARAMETER_VARIABLE_ELEMENTS))) counted++;
    }
    else counted++;
  }

  weed_free(params);
  return counted;
}

gboolean has_usable_palette(weed_plant_t *chantmpl) {
  int error;
  int palette=weed_get_int_value(chantmpl,"current_palette",&error);
  // currently only integer RGB palettes are usable
  if (palette==5||palette==6) return FALSE;
  if (palette>0&&palette<=7) return TRUE;
  return FALSE;
}



gint enabled_in_channels (weed_plant_t *plant, gboolean count_repeats) {
  weed_plant_t **channels=NULL;
  gint enabled=0;
  int num_channels,i,error;
  gboolean is_template=(weed_get_int_value(plant,"type",&error)==WEED_PLANT_FILTER_CLASS);

  if (is_template) {
    if (!weed_plant_has_leaf(plant,"in_channel_templates")) return 0;
    num_channels=weed_leaf_num_elements(plant,"in_channel_templates");
    if (num_channels>0) channels=weed_get_plantptr_array(plant,"in_channel_templates",&error);
  }
  else {
    if (!weed_plant_has_leaf(plant,"in_channels")) return 0;
    num_channels=weed_leaf_num_elements(plant,"in_channels");
    if (num_channels>0) channels=weed_get_plantptr_array(plant,"in_channels",&error);
  }

  for (i=0;i<num_channels;i++) {
    if (!weed_plant_has_leaf(channels[i],"disabled")||weed_get_boolean_value(channels[i],"disabled",&error)!=WEED_TRUE) enabled++;
    if (count_repeats) {
      // count repeated channels
      weed_plant_t *chantmpl;
      int repeats;
      if (is_template) chantmpl=channels[i];
      else chantmpl=weed_get_plantptr_value(channels[i],"template",&error);
      if (weed_plant_has_leaf(channels[i],"max_repeats")) {
	if (weed_plant_has_leaf(channels[i],"disabled")&&weed_get_boolean_value(channels[i],"disabled",&error)==WEED_TRUE&&!has_usable_palette(chantmpl)) continue; // channel was disabled because palette is unusable
	repeats=weed_get_int_value(channels[i],"max_repeats",&error)-1;
	if (repeats==-1) repeats=1000000;
	enabled+=repeats;
      }
    }
  }

  if (channels!=NULL) weed_free(channels);

  return enabled;
}


gint enabled_out_channels (weed_plant_t *plant, gboolean count_repeats) {
  weed_plant_t **channels=NULL;
  gint enabled=0;
  int num_channels,i,error;
  gboolean is_template=(weed_get_int_value(plant,"type",&error)==WEED_PLANT_FILTER_CLASS);

  if (is_template) {
    num_channels=weed_leaf_num_elements(plant,"out_channel_templates");
    if (num_channels>0) channels=weed_get_plantptr_array(plant,"out_channel_templates",&error);
  }
  else {
    num_channels=weed_leaf_num_elements(plant,"out_channels");
    if (num_channels>0) channels=weed_get_plantptr_array(plant,"out_channels",&error);
  }

  for (i=0;i<num_channels;i++) {
    if (!weed_plant_has_leaf(channels[i],"disabled")||weed_get_boolean_value(channels[i],"disabled",&error)!=WEED_TRUE) enabled++;
    if (count_repeats) {
      // count repeated channels
      weed_plant_t *chantmpl;
      int repeats;
      if (is_template) chantmpl=channels[i];
      else chantmpl=weed_get_plantptr_value(channels[i],"template",&error);
      if (weed_plant_has_leaf(channels[i],"max_repeats")) {
	if (weed_plant_has_leaf(channels[i],"disabled")&&weed_get_boolean_value(channels[i],"disabled",&error)==WEED_TRUE&&!has_usable_palette(chantmpl)) continue; // channel was disabled because palette is unusable
	repeats=weed_get_int_value(channels[i],"max_repeats",&error)-1;
	if (repeats==-1) repeats=1000000;
	enabled+=repeats;
      }
    }
  }

  if (channels!=NULL) weed_free(channels);

  return enabled;
}






/////////////////////////////////////////////////////////////////////



static gint check_for_lives(weed_plant_t *filter, int filter_idx) {
  // for LiVES, currently:
  // all filters must take 0, 1 or 2 mandatory/optional inputs and provide 
  // 1 mandatory output or >1 optional outputs (for now)

  // all channels used must support a limited range of palettes (for now)

  gint chans_in_mand=0; // number of mandatory channels
  gint chans_in_opt_max=0; // number of usable (by LiVES) optional channels
  gint chans_out_mand=0;
  gint chans_out_opt_max=0;
  gint achans_in_mand=0,achans_out_mand=0;
  gboolean is_generator;
  gboolean is_audio=FALSE;

  int error,flags=0;
  int num_elements,i;
  weed_plant_t **array=NULL;

  // TODO - check seed types

  if (!weed_plant_has_leaf(filter,"name")) return 1;
  if (!weed_plant_has_leaf(filter,"author")) return 2;
  if (!weed_plant_has_leaf(filter,"version")) return 3;
  if (!weed_plant_has_leaf(filter,"process_func")) return 4;

  if (!weed_plant_has_leaf(filter,"flags")) weed_set_int_value(filter,"flags",0);
  else flags=weed_get_int_value(filter,"flags",&error);

  // for now we will only load realtime effects
  if (flags&WEED_FILTER_NON_REALTIME) return 5;

  // count number of mandatory and optional in_channels
  if (!weed_plant_has_leaf(filter,"in_channel_templates")) num_elements=0;
  else num_elements=weed_leaf_num_elements(filter,"in_channel_templates");

  if (num_elements>0) array=weed_get_plantptr_array(filter,"in_channel_templates",&error);

  for (i=0;i<num_elements;i++) {
#ifndef HAVE_POSIX_MEMALIGN
    if (weed_plant_has_leaf(array[i],"alignment")) return 12;
#endif
    if (weed_plant_has_leaf(array[i],"is_audio")&&weed_get_boolean_value(array[i],"is_audio",&error)==WEED_TRUE) is_audio=TRUE;
    if (!weed_plant_has_leaf(array[i],"name")||(!weed_plant_has_leaf(array[i],"palette_list")&&!is_audio)) {
      weed_free(array);
      return 6;
    }

    if (!weed_plant_has_leaf(array[i],"flags")) weed_set_int_value(array[i],"flags",0);

    if (weed_plant_has_leaf(array[i],"optional")&&weed_get_boolean_value(array[i],"optional",&error)==WEED_TRUE) {
      // is optional
      chans_in_opt_max++;
      weed_set_boolean_value(array[i],"disabled",WEED_TRUE);
    }
    else {
      if (!is_audio) chans_in_mand++;
      else achans_in_mand++;
    }
  }
  if (num_elements>0) weed_free(array);
  if (chans_in_mand>2) return 8; // we dont handle mixers yet...
  if (achans_in_mand>0&&chans_in_mand>0) return 13; // can't yet handle effects that need both audio and video

  is_generator=(chans_in_mand==0);

  // count number of mandatory and optional out_channels
  if (!weed_plant_has_leaf(filter,"out_channel_templates")) num_elements=0;
  else num_elements=weed_leaf_num_elements(filter,"out_channel_templates");

  if (num_elements>0) array=weed_get_plantptr_array(filter,"out_channel_templates",&error);

  for (i=0;i<num_elements;i++) {
#ifndef HAVE_POSIX_MEMALIGN
    if (weed_plant_has_leaf(array[i],"alignment")) return 12;
#endif
    if (weed_plant_has_leaf(array[i],"is_audio")&&weed_get_boolean_value(array[i],"is_audio",&error)==WEED_TRUE) is_audio=TRUE;
    if (!weed_plant_has_leaf(array[i],"name")||(!weed_plant_has_leaf(array[i],"palette_list")&&!is_audio)) {
      weed_free(array);
      return 9;
    }

    if (!weed_plant_has_leaf(array[i],"flags")) weed_set_int_value(array[i],"flags",0);

    if (weed_plant_has_leaf(array[i],"optional")&&weed_get_boolean_value(array[i],"optional",&error)==WEED_TRUE) {
      // is optional
      chans_out_opt_max++;
      weed_set_boolean_value(array[i],"disabled",WEED_TRUE);
    }
    else {
      // is mandatory
      if (!is_audio) chans_out_mand++;
      else achans_out_mand++;
    }
  }
  if (num_elements>0) weed_free(array);

  if (chans_out_mand>1||(chans_out_mand+chans_out_opt_max+achans_out_mand<1)) return 11;
  if (achans_out_mand>1||(achans_out_mand==1&&chans_out_mand>0)) return 14;
  if ((achans_in_mand==1&&achans_out_mand==0)||(achans_in_mand==0&&achans_out_mand==1)) return 15;

  weed_add_plant_flags(filter,WEED_LEAF_READONLY_PLUGIN);
  if (weed_plant_has_leaf(filter,"gui")) {
    weed_plant_t *gui=weed_get_plantptr_value(filter,"gui",&error);
    weed_add_plant_flags(gui,WEED_LEAF_READONLY_PLUGIN);
  }

  if (flags&WEED_FILTER_IS_CONVERTER) {
    if (is_audio) {
      weed_set_boolean_value(filter,"host_menu_hide",WEED_TRUE);
      if (enabled_in_channels(filter,TRUE)>=1000000) {
	// this is a candidate for audio volume
	lives_fx_candidate_t *cand=&mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL];
	cand->list=g_list_append(cand->list,GINT_TO_POINTER(filter_idx));
	cand->delegate=0;
      }
    }
    else {
      weed_set_boolean_value(filter,"host_menu_hide",WEED_TRUE);
      if (chans_in_mand==1&&chans_out_mand==1) {
	weed_plant_t *fstout=get_mandatory_channel(filter,0,FALSE);
	int ochan_flags=weed_get_int_value(fstout,"flags",&error);
	if (ochan_flags&WEED_CHANNEL_SIZE_CAN_VARY) {
	  // this is a candidate for resize
	  lives_fx_candidate_t *cand=&mainw->fx_candidates[FX_CANDIDATE_RESIZER];
	  cand->list=g_list_append(cand->list,GINT_TO_POINTER(filter_idx));
	  cand->delegate=0;
	}
      }
    }
  }

  return 0;
}



static gboolean set_in_channel_palettes (gint idx, gint num_channels) {
  // set in channel palettes for filter[idx]
  // we also enable optional channels if we have to
  // in this case we fill first the mandatory channels,
  // then if necessary the optional channels

  // we return FALSE if we could not satisfy the request
  weed_plant_t **chantmpls=NULL;
  int def_palette;

  int num_elements,i,error;

  if (!weed_plant_has_leaf(weed_filters[idx],"in_channel_templates")) {
    if (num_channels>0) return FALSE;
    return TRUE;
  }

  num_elements=weed_leaf_num_elements(weed_filters[idx],"in_channel_templates");

  if (num_elements<num_channels) return FALSE;

  if (num_elements==0) return TRUE;

  chantmpls=weed_get_plantptr_array(weed_filters[idx],"in_channel_templates",&error);

  // our start state is with all optional channels disabled

  // fill mandatory channels first; these palettes may change later if we get a frame in a different palette
  for (i=0;i<num_elements;i++) {
    if (!weed_plant_has_leaf(chantmpls[i],"is_audio")||weed_get_boolean_value(chantmpls[i],"is_audio",&error)!=WEED_TRUE) {
      int num_palettes=weed_leaf_num_elements(chantmpls[i],"palette_list");
      int *palettes=weed_get_int_array(chantmpls[i],"palette_list",&error);
      if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_RGB24)==WEED_PALETTE_RGB24) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGB24);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_BGR24)==WEED_PALETTE_BGR24) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_BGR24);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_RGBA32)==WEED_PALETTE_RGBA32) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGBA32);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_BGRA32)==WEED_PALETTE_BGRA32) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_BGRA32);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_ARGB32)==WEED_PALETTE_ARGB32) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_ARGB32);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV888)==WEED_PALETTE_YUV888) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV888);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV444P)==WEED_PALETTE_YUV444P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV444P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUVA8888)==WEED_PALETTE_YUVA8888) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUVA8888);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUVA4444P)==WEED_PALETTE_YUVA4444P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUVA4444P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_UYVY8888)==WEED_PALETTE_UYVY8888) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_UYVY8888);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUYV8888)==WEED_PALETTE_YUYV8888) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUYV8888);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV422P)==WEED_PALETTE_YUV422P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV422P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV420P)==WEED_PALETTE_YUV420P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV420P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YVU420P)==WEED_PALETTE_YVU420P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YVU420P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV411)==WEED_PALETTE_YUV411) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV411);
      else if (!weed_plant_has_leaf(chantmpls[i],"optional")) {
	if (chantmpls!=NULL) weed_free(chantmpls);
	weed_free(palettes);
	return FALSE; // mandatory channel; we don't yet handle alpha or float
      }
      /*      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_A8)==WEED_PALETTE_A8) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_A8);
	      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_A1)==WEED_PALETTE_A1) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_A1);
	      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_AFLOAT)==WEED_PALETTE_AFLOAT) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_AFLOAT);
	      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_RGBFLOAT)==WEED_PALETTE_RGBFLOAT) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGBFLOAT);
	      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_ARGBFLOAT)==WEED_PALETTE_ARGBFLOAT) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_ARGBFLOAT);
      */
      weed_free(palettes);
    }
    if (!weed_plant_has_leaf(chantmpls[i],"optional")) num_channels--; // mandatory channel
  }

  if (num_channels>0) {
    // OK we need to use some optional channels
    for (i=0;i<num_elements&&num_channels>0;i++) if (weed_plant_has_leaf(chantmpls[i],"optional")) {
      weed_set_boolean_value(chantmpls[i],"disabled",WEED_FALSE);
      num_channels--;
    }
  }
  if (num_channels>0) {
    if (chantmpls!=NULL) weed_free(chantmpls);
    return FALSE;
  }

  // now we set match channels
  if (!weed_plant_has_leaf(chantmpls[0],"is_audio")||weed_get_boolean_value(chantmpls[0],"is_audio",&error)!=WEED_TRUE) {
    def_palette=weed_get_int_value(chantmpls[0],"current_palette",&error);
    for (i=1;i<num_elements;i++) {
      int channel_flags=weed_get_int_value(chantmpls[i],"flags",&error);
      if (!(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY)) {
	int num_palettes=weed_leaf_num_elements(chantmpls[i],"palette_list");
	int *palettes=weed_get_int_array(chantmpls[i],"palette_list",&error);
	if (check_weed_palette_list(palettes,num_palettes,def_palette)==def_palette) weed_set_int_value(chantmpls[i],"current_palette",def_palette);
	else {
	  if (chantmpls!=NULL) weed_free(chantmpls);
	  weed_free(palettes);
	  return FALSE;
	}
	weed_free(palettes);
      }
    }
  }
  if (chantmpls!=NULL) weed_free(chantmpls);
  return TRUE;
}



static gboolean set_out_channel_palettes (gint idx, gint num_channels) {
  // set in channel palettes for filter[idx]
  // we also enable optional channels if we have to
  // in this case we fill first the mandatory channels,
  // then if necessary the optional channels

  // we return FALSE if we could not satisfy the request
  weed_plant_t **chantmpls=NULL;
  weed_plant_t **in_chantmpls=NULL;

  int num_elements=weed_leaf_num_elements(weed_filters[idx],"out_channel_templates"),i,error;
  int num_in_elements=weed_leaf_num_elements(weed_filters[idx],"in_channel_templates");

  int def_palette=WEED_PALETTE_END;

  if (num_elements<num_channels) return FALSE;

  chantmpls=weed_get_plantptr_array(weed_filters[idx],"out_channel_templates",&error);
  // our start state is with all optional channels disabled

  // fill mandatory channels first; these palettes may change later if we get a frame in a different palette
  for (i=0;i<num_elements;i++) {
    if (!weed_plant_has_leaf(chantmpls[i],"is_audio")||weed_get_boolean_value(chantmpls[i],"is_audio",&error)!=WEED_TRUE) {
      int num_palettes=weed_leaf_num_elements(chantmpls[i],"palette_list");
      int *palettes=weed_get_int_array(chantmpls[i],"palette_list",&error);
      if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_RGB24)==WEED_PALETTE_RGB24) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGB24);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_BGR24)==WEED_PALETTE_BGR24) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_BGR24);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_RGBA32)==WEED_PALETTE_RGBA32) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGBA32);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_BGRA32)==WEED_PALETTE_BGRA32) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_BGRA32);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_ARGB32)==WEED_PALETTE_ARGB32) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_ARGB32);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV888)==WEED_PALETTE_YUV888) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV888);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV444P)==WEED_PALETTE_YUV444P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV444P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUVA8888)==WEED_PALETTE_YUVA8888) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUVA8888);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUVA4444P)==WEED_PALETTE_YUVA4444P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUVA4444P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_UYVY8888)==WEED_PALETTE_UYVY8888) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_UYVY8888);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUYV8888)==WEED_PALETTE_YUYV8888) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUYV8888);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV422P)==WEED_PALETTE_YUV422P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV422P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV420P)==WEED_PALETTE_YUV420P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV420P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YVU420P)==WEED_PALETTE_YVU420P) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YVU420P);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_YUV411)==WEED_PALETTE_YUV411) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV411);
      else if (!weed_plant_has_leaf(chantmpls[i],"optional")) {
	if (chantmpls!=NULL) weed_free(chantmpls);
	weed_free(palettes);
	return FALSE; // mandatory channel; we don't yet handle alpha or float
      }

      /*else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_A8)==WEED_PALETTE_A8) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_A8);
	else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_A1)==WEED_PALETTE_A1) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_A1);
	else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_AFLOAT)==WEED_PALETTE_AFLOAT) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_AFLOAT);
	else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_RGBFLOAT)==WEED_PALETTE_RGBFLOAT) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGBFLOAT);
	else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_ARGBFLOAT)==WEED_PALETTE_ARGBFLOAT) weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_ARGBFLOAT);
      */
      weed_free(palettes);
    }
    if (!weed_plant_has_leaf(chantmpls[i],"optional")) num_channels--; // mandatory channel
  }

  if (num_channels>0) {
    // OK we need to use some optional channels
    for (i=0;i<num_elements&&num_channels>0;i++) if (weed_plant_has_leaf(chantmpls[i],"optional")) {
      weed_set_boolean_value(chantmpls[i],"disabled",WEED_FALSE);
      num_channels--;
    }
  }

  if (num_channels>0) {
    if (chantmpls!=NULL) weed_free(chantmpls);
    return FALSE;
  }

  // now we set match channels
  if (num_in_elements) {
    in_chantmpls=weed_get_plantptr_array(weed_filters[idx],"in_channel_templates",&error);
    if (!weed_plant_has_leaf(in_chantmpls[0],"is_audio")||weed_get_boolean_value(in_chantmpls[0],"is_audio",&error)!=WEED_TRUE) {
      def_palette=weed_get_int_value(in_chantmpls[0],"current_palette",&error);
    }
    weed_free(in_chantmpls);
  }
  else if (!weed_plant_has_leaf(chantmpls[0],"is_audio")||weed_get_boolean_value(chantmpls[0],"is_audio",&error)!=WEED_TRUE) {
    def_palette=weed_get_int_value(chantmpls[0],"current_palette",&error);
  }
  if (def_palette!=WEED_PALETTE_END) {
    for (i=0;i<num_elements;i++) {
      int channel_flags=weed_get_int_value(chantmpls[i],"flags",&error);
      if (!(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY)) {
	int num_palettes=weed_leaf_num_elements(chantmpls[i],"palette_list");
	int *palettes=weed_get_int_array(chantmpls[i],"palette_list",&error);
	if (check_weed_palette_list(palettes,num_palettes,def_palette)==def_palette) weed_set_int_value(chantmpls[i],"current_palette",def_palette);
	else {
	  if (chantmpls!=NULL) weed_free(chantmpls);
	  weed_free(palettes);
	  return FALSE;
	}
	weed_free(palettes);
      }
    }
  }
  if (chantmpls!=NULL) weed_free(chantmpls);
  return TRUE;
}




static void load_weed_plugin (gchar *plugin_name, gchar *plugin_path) {
  gchar *filter_name;
  weed_setup_f setup_fn;
  weed_bootstrap_f bootstrap=(weed_bootstrap_f)&weed_bootstrap_func;
  
  int error,reason,idx=num_weed_filters;

  weed_plant_t *plugin_info,**filters;
  void *handle;
  int filters_in_plugin;
  int mode=-1,kmode=0;
  int i;
  gchar *string,*filter_type;
  GtkWidget *menuitem;

  key++;

  // walk list and create fx structures

#ifdef DEBUG_WEED
  g_printerr("Checking plugin %s\n",plugin_path);
#endif

  if ((handle=dlopen(plugin_path,RTLD_LAZY))) {
    dlerror(); // clear existing errors

    if ((setup_fn=dlsym(handle,"weed_setup"))==NULL) {
      g_printerr(_("Error: plugin %s has no weed_setup() function.\n"),plugin_path);
    }
    else {

      // here we call the plugin's setup_fn, passing in our bootstrap function
      // the plugin will call our bootstrap function to get the correct versions of the core weed functions and bootstrap itself
      
      // if we use the plugin, we must not free the plugin_info, since the plugin has a reference to this
      
      plugin_info=(*setup_fn)(bootstrap);
      if (plugin_info==NULL||(filters_in_plugin=check_weed_plugin_info(plugin_info))<1) {
	g_printerr ("error loading plugin %s\n",plugin_path);
	if (plugin_info!=NULL) weed_plant_free(plugin_info);
	dlclose (handle);
	return;
      }
    
      weed_set_voidptr_value(plugin_info,"handle",handle);
      weed_set_string_value(plugin_info,"name",plugin_name); // for hashname
      weed_add_plant_flags(plugin_info,WEED_LEAF_READONLY_PLUGIN);
      
      filters=weed_get_plantptr_array(plugin_info,"filters",&error);
      
      while (idx<MAX_WEED_FILTERS&&mode<filters_in_plugin-1) {
	mode++;
	filter_name=weed_get_string_value(filters[mode],"name",&error);
	if (!(reason=check_for_lives(filters[mode],idx))) {
	  gboolean dup=FALSE;
	  weed_filters[idx]=filters[mode];
	  num_weed_filters++;
	  hashnames[idx]=make_weed_hashname(idx);
	  for (i=0;i<idx;i++) {
	    if (!strcmp(hashnames[idx],hashnames[i])) {
	      // skip dups
	      g_free(hashnames[idx]);
	      hashnames[idx]=NULL;
	      weed_filters[idx]=NULL;
	      num_weed_filters--;
	      dup=TRUE;
	      break;
	    }
	  }
	  if (!dup) {
#ifdef DEBUG_WEED
	    if (key<FX_KEYS_PHYSICAL) d_print(g_strdup_printf("Loaded filter \"%s\" in plugin \"%s\"; assigned to key ctrl-%d, mode %d.\n",filter_name,plugin_name,key+1,kmode+1));
	    else d_print(g_strdup_printf("Loaded filter \"%s\" in plugin \"%s\", no key assigned\n",filter_name,plugin_name));
#endif
	    
	    // we start with all optional channels disabled (unless forced to use them)
	    set_in_channel_palettes (idx,enabled_in_channels(weed_filters[idx],FALSE));
	    set_out_channel_palettes (idx,1);
	    
	    if (!weed_plant_has_leaf(weed_filters[idx],"host_menu_hide")) {
	      if (key<FX_KEYS_PHYSICAL) {
		key_to_fx[key][kmode]=idx;
	      }
	      filter_type=weed_filter_get_type(idx);
	      string=g_strdup_printf("%s (%s)",filter_name,filter_type);
	      menuitem=gtk_menu_item_new_with_label (string);
	      gtk_widget_show(menuitem);
	      g_free(string);
	      g_free(filter_type);
	      gtk_container_add (GTK_CONTAINER (mainw->rte_defs), menuitem);
	      
	      g_signal_connect (GTK_OBJECT (menuitem), "activate",
				G_CALLBACK (rte_set_defs_activate),
				GINT_TO_POINTER(idx));
	      
	      kmode++;
	    }
	    idx++;
	  }
	}
#ifdef DEBUG_WEED
	else g_printerr(g_strdup_printf("Unsuitable filter \"%s\" in plugin \"%s\", reason code %d\n",filter_name,plugin_name,reason));
#endif
	weed_free(filter_name);
      }
      weed_free(filters);
    }
  }
  else g_printerr(_("Info: Unable to load plugin %s\nError was: %s\n"),plugin_path,dlerror());

  // TODO - add any rendered effects to fx submenu
}



void weed_memory_init(void) {
  weed_init(110,(weed_malloc_f)lives_weed_malloc,(weed_free_f)lives_weed_free,(weed_memcpy_f)lives_weed_memcpy,(weed_memset_f)lives_weed_memset);
}


void weed_load_all (void) {
// get list of plugins from directory and create our fx
  int i,j,plugin_idx,subdir_idx;

  GList *weed_plugin_list,*weed_plugin_sublist;

  gchar *lives_weed_plugin_path,*weed_plugin_path,*weed_p_path;
  gchar *subdir_path,*subdir_name,*plugin_path,*plugin_name;
  gint numdirs;
  gchar **dirs;
  gchar *msg;

  gint listlen;

  key=-1;

  num_weed_filters=0;

  pthread_mutex_lock(&mainw->gtk_mutex);
  lives_weed_plugin_path=g_strdup_printf("%s%s%s",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_WEED_FX_BUILTIN);

#ifdef DEBUG_WEED
  g_printerr("In weed init\n");
#endif
  pthread_mutex_unlock(&mainw->gtk_mutex);

  // danger Will Robinson !
  fg_gen_to_start=fg_generator_key=fg_generator_clip=fg_generator_mode=-1;
  bg_gen_to_start=bg_generator_key=bg_generator_mode=-1;

  // reset all instances to NULL
  for (i=0;i<MAX_WEED_INSTANCES;i++) weed_instances[i]=NULL;

  for (i=0;i<FX_KEYS_MAX;i++) {
    key_modes[i]=0;  // current active mode of each key
    filter_map[i]=NULL; // maps effects in order of application for multitrack rendering
    for (j=0;j<MAX_MODES_PER_KEY;j++) {
      key_to_instance[i][j]=key_to_fx[i][j]=-1;
    }
  }
  filter_map[FX_KEYS_MAX+1]=NULL;
  for (i=0;i<FX_KEYS_MAX_VIRTUAL;i++) {
    init_events[i]=NULL;
  }

  next_free_key=FX_KEYS_MAX_VIRTUAL;

  for (i=0;i<MAX_WEED_FILTERS;i++) {
    weed_filters[i]=NULL;
    hashnames[i]=NULL;
  }

  pthread_mutex_lock(&mainw->gtk_mutex);
  weed_p_path=getenv("WEED_PLUGIN_PATH");
  if (weed_p_path==NULL) weed_p_path=g_strdup("");
  weed_plugin_path=g_strdup(weed_p_path);
  g_free(weed_p_path);
  if (weed_plugin_path==NULL) weed_plugin_path=g_strdup("");
  if (strstr(weed_plugin_path,lives_weed_plugin_path)==NULL) {
    gchar *tmp=g_strconcat(strcmp(weed_plugin_path,"")?":":"",lives_weed_plugin_path,NULL);
    g_free(weed_plugin_path);
    weed_plugin_path=tmp;
    setenv("WEED_PLUGIN_PATH",weed_plugin_path,1);
  }
  g_free(lives_weed_plugin_path);

  // first we parse the weed_plugin_path
  numdirs=get_token_count(weed_plugin_path,':');
  dirs=g_strsplit(weed_plugin_path,":",-1);
  pthread_mutex_unlock(&mainw->gtk_mutex);

  for (i=0;i<numdirs;i++) {
    // get list of all files
    weed_plugin_list=get_plugin_list(PLUGIN_EFFECTS_WEED,TRUE,dirs[i],"so");
    listlen=g_list_length(weed_plugin_list);
    
    // parse twice, first we get the plugins, then 1 level of subdirs
    for (plugin_idx=0;plugin_idx<listlen;plugin_idx++) {
      pthread_mutex_lock(&mainw->gtk_mutex);
      plugin_name=g_list_nth_data(weed_plugin_list,plugin_idx);
      plugin_path=g_strdup_printf("%s/%s",dirs[i],plugin_name);
      load_weed_plugin(plugin_name,plugin_path);
      g_free(g_list_nth_data(weed_plugin_list,plugin_idx));
      weed_plugin_list=g_list_delete_link(weed_plugin_list,g_list_nth(weed_plugin_list,plugin_idx));
      plugin_idx--;
      listlen--;
      g_free(plugin_path);
      pthread_mutex_unlock(&mainw->gtk_mutex);

    }
    
    // get 1 level of subdirs
    for (subdir_idx=0;subdir_idx<listlen;subdir_idx++) {
      pthread_mutex_lock(&mainw->gtk_mutex);
      subdir_name=g_list_nth_data(weed_plugin_list,subdir_idx);
      subdir_path=g_strdup_printf("%s/%s",dirs[i],subdir_name);
      if (!g_file_test(subdir_path, G_FILE_TEST_IS_DIR)||!strcmp(subdir_name,"icons")||!strcmp(subdir_name,"data")) {
	g_free(subdir_name);
	g_free(subdir_path);
	pthread_mutex_unlock(&mainw->gtk_mutex);
	continue;
      }
      g_free(subdir_name);
      weed_plugin_sublist=get_plugin_list(PLUGIN_EFFECTS_WEED,TRUE,subdir_path,"so");
      
      for (plugin_idx=0;plugin_idx<g_list_length(weed_plugin_sublist);plugin_idx++) {
	plugin_name=g_list_nth_data(weed_plugin_sublist,plugin_idx);
	plugin_path=g_strdup_printf("%s/%s",subdir_path,plugin_name);
	load_weed_plugin(plugin_name,plugin_path);
	g_free(plugin_name);
	g_free(plugin_path);
      }
      if (weed_plugin_sublist!=NULL) {
	g_list_free_strings(weed_plugin_sublist);
	g_list_free(weed_plugin_sublist);
      }
      g_free(subdir_path);
      pthread_mutex_unlock(&mainw->gtk_mutex);
    }
    if (weed_plugin_list!=NULL) {
      pthread_mutex_lock(&mainw->gtk_mutex);
      g_list_free_strings(weed_plugin_list);
      g_list_free(weed_plugin_list);
      pthread_mutex_unlock(&mainw->gtk_mutex);
    }
  }
  pthread_mutex_lock(&mainw->gtk_mutex);
  g_strfreev(dirs);
  g_free(weed_plugin_path);

  msg=g_strdup_printf(_ ("Successfully loaded %d Weed filters\n"),num_weed_filters);
  d_print(msg);
  g_free(msg);
  pthread_mutex_unlock(&mainw->gtk_mutex);

}


void weed_filter_free(weed_plant_t *filter) {
  int nitems,error,i;
  weed_plant_t **plants,*gui;
  void *func;

  if (weed_plant_has_leaf(filter,"init_func")) {
    func=weed_get_voidptr_value(filter,"init_func",&error);
    if (func!=NULL) weed_free(func);
  }

  if (weed_plant_has_leaf(filter,"deinit_func")) {
    func=weed_get_voidptr_value(filter,"deinit_func",&error);
    if (func!=NULL) weed_free(func);
  }

  if (weed_plant_has_leaf(filter,"process_func")) {
    func=weed_get_voidptr_value(filter,"process_func",&error);
    if (func!=NULL) weed_free(func);
  }


  // free in_channel_templates
  if (weed_plant_has_leaf(filter,"in_channel_templates")) {
    nitems=weed_leaf_num_elements(filter,"in_channel_templates");
    if (nitems>0) {
      plants=weed_get_plantptr_array(filter,"in_channel_templates",&error);
      pthread_mutex_lock(&mainw->gtk_mutex);
      for (i=0;i<nitems;i++) weed_plant_free(plants[i]);
      weed_free(plants);
      pthread_mutex_unlock(&mainw->gtk_mutex);
    }
  }


  // free out_channel_templates
  if (weed_plant_has_leaf(filter,"out_channel_templates")) {
    nitems=weed_leaf_num_elements(filter,"out_channel_templates");
    if (nitems>0) {
      plants=weed_get_plantptr_array(filter,"out_channel_templates",&error);
      pthread_mutex_lock(&mainw->gtk_mutex);
      for (i=0;i<nitems;i++) weed_plant_free(plants[i]);
      weed_free(plants);
      pthread_mutex_unlock(&mainw->gtk_mutex);
    }
  }

  // free in_param_templates
  if (weed_plant_has_leaf(filter,"in_parameter_templates")) {
    nitems=weed_leaf_num_elements(filter,"in_parameter_templates");
    if (nitems>0) {
      plants=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
      pthread_mutex_lock(&mainw->gtk_mutex);
      for (i=0;i<nitems;i++) {
	if (weed_plant_has_leaf(plants[i],"gui")) {
	  gui=(weed_get_plantptr_value(plants[i],"gui",&error));
	  if (weed_plant_has_leaf(gui,"display_func")) {
	    func=weed_get_voidptr_value(gui,"display_func",&error);
	    if (func!=NULL) weed_free(func);
	  }
	  weed_plant_free(gui);
	}
	if (weed_plant_has_leaf(filter,"interpolate_func")) {
	  func=weed_get_voidptr_value(filter,"interpolate_func",&error);
	  if (func!=NULL) weed_free(func);
	}
	weed_plant_free(plants[i]);
      }
      weed_free(plants);
      pthread_mutex_unlock(&mainw->gtk_mutex);
    }
  }


  // free out_param_templates
  if (weed_plant_has_leaf(filter,"out_parameter_templates")) {
    nitems=weed_leaf_num_elements(filter,"out_parameter_templates");
    if (nitems>0) {
      plants=weed_get_plantptr_array(filter,"out_parameter_templates",&error);
      pthread_mutex_lock(&mainw->gtk_mutex);
      for (i=0;i<nitems;i++) {
	if (weed_plant_has_leaf(plants[i],"gui")) weed_plant_free(weed_get_plantptr_value(plants[i],"gui",&error));
	weed_plant_free(plants[i]);
      }
      weed_free(plants);
      pthread_mutex_unlock(&mainw->gtk_mutex);
    }
  }


  // free gui
  pthread_mutex_lock(&mainw->gtk_mutex);
  if (weed_plant_has_leaf(filter,"gui")) weed_plant_free(weed_get_plantptr_value(filter,"gui",&error));


  // free filter
  weed_plant_free(filter);
  pthread_mutex_unlock(&mainw->gtk_mutex);
}


void weed_unload_all(void) {
  int i,error;
  weed_plant_t *filter,*plugin_info,*host_info;
  void *handle;
  weed_desetup_f desetup_fn;
  GList *pinfo=NULL,*xpinfo;

  pthread_mutex_lock(&mainw->gtk_mutex);
  mainw->num_tr_applied=0;
  weed_deinit_all();
  for (i=0;i<num_weed_filters;i++) {
    filter=weed_filters[i];
    plugin_info=weed_get_plantptr_value(filter,"plugin_info",&error);

    if (pinfo==NULL||g_list_index(pinfo,plugin_info)==-1) pinfo=g_list_append(pinfo,plugin_info);

    handle=weed_get_voidptr_value(plugin_info,"handle",&error);

    if (handle!=NULL&&prefs->startup_phase==0) {
      if ((desetup_fn=dlsym (handle,"weed_desetup"))!=NULL) {
	// call weed_desetup()
	(*desetup_fn)();
      }

      pthread_mutex_unlock(&mainw->gtk_mutex);
      dlclose(handle);
      pthread_mutex_lock(&mainw->gtk_mutex);
      handle=NULL;
      weed_set_voidptr_value(plugin_info,"handle",handle);
    }
    weed_filter_free(filter);
    pthread_mutex_unlock(&mainw->gtk_mutex);
    pthread_mutex_lock(&mainw->gtk_mutex);
  }

  xpinfo=pinfo;

  while (pinfo!=NULL) {
    host_info=weed_get_plantptr_value((weed_plant_t *)pinfo->data,"host_info",&error);
    weed_plant_free(host_info);
    weed_plant_free((weed_plant_t *)pinfo->data);
    pinfo=pinfo->next;
  }

  if (xpinfo!=NULL) g_list_free(xpinfo);

  for (i=0;i<MAX_WEED_FILTERS;i++) {
    if (hashnames[i]!=NULL) g_free(hashnames[i]);
  }

  pthread_mutex_unlock(&mainw->gtk_mutex);

}


static void weed_channels_free (weed_plant_t *inst) {
  weed_plant_t **channels;
  int i,error;
  int num_channels;

  num_channels=weed_leaf_num_elements(inst,"in_channels");
  channels=weed_get_plantptr_array(inst,"in_channels",&error);
  for (i=0;i<num_channels;i++) weed_plant_free(channels[i]);
  weed_free(channels);

  num_channels=weed_leaf_num_elements(inst,"out_channels");
  channels=weed_get_plantptr_array(inst,"out_channels",&error);
  for (i=0;i<num_channels;i++) weed_plant_free(channels[i]);
  weed_free(channels);
}

static void weed_gui_free (weed_plant_t *plant) {
  weed_plant_t *gui;
  int error;

  if (weed_plant_has_leaf(plant,"gui")) {
    gui=weed_get_plantptr_value(plant,"gui",&error);
    weed_plant_free(gui);
  }
}


static void weed_parameters_free (weed_plant_t *inst) {
  weed_plant_t **parameters;
  int i,error;
  int num_parameters;

  num_parameters=weed_leaf_num_elements(inst,"in_parameters");
  parameters=weed_get_plantptr_array(inst,"in_parameters",&error);
  for (i=0;i<num_parameters;i++) {
    if (parameters[i]==mainw->rte_textparm) mainw->rte_textparm=NULL;
    weed_gui_free(parameters[i]);
    weed_plant_free(parameters[i]);
  }
  weed_free(parameters);

  num_parameters=weed_leaf_num_elements(inst,"out_parameters");
  parameters=weed_get_plantptr_array(inst,"out_parameters",&error);
  for (i=0;i<num_parameters;i++) {
    weed_gui_free(parameters[i]);
    weed_plant_free(parameters[i]);
  }
  weed_free(parameters);
}



void weed_free_instance (weed_plant_t *inst) {
  weed_channels_free(inst);
  weed_parameters_free(inst);
  weed_plant_free(inst);
}



void weed_generator_end (weed_plant_t *inst) {
  // generator has stopped for one of the following reasons:
  // efect was de-inited; clip (bg/fg) was changed; playback stopped with fg
  gboolean is_bg=FALSE;
  gint current_file=mainw->current_file,pre_src_file=mainw->pre_src_file;
  gboolean clip_switched=mainw->clip_switched;
  weed_plant_t *filter;
  int error;

  if (inst==NULL) {
    g_printerr("  WARNING: inst was NULL !    ");
    return;
  }

  if (mainw->blend_file!=-1&&mainw->blend_file!=current_file&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->ext_src==inst) is_bg=TRUE;
  else mainw->new_blend_file=mainw->blend_file;

  if (!is_bg&&mainw->whentostop==STOP_ON_VID_END&&mainw->playing_file>0) {
    // we will close the file after playback stops
    mainw->cancelled=CANCEL_GENERATOR_END;
    return;
  }
  
  if (rte_window!=NULL) {
    // update real time effects window if we are showing it
    if (!is_bg) {
      rtew_set_param_button (fg_generator_key,fg_generator_mode,FALSE);
      rtew_set_keych(fg_generator_key,FALSE);
    }
    else {
      rtew_set_param_button (bg_generator_key,bg_generator_mode,FALSE);
      rtew_set_keych(bg_generator_key,FALSE);
    }
  }

  if (!is_bg) close_pwindow(fg_generator_key,fg_generator_mode,FALSE);
  else close_pwindow(bg_generator_key,bg_generator_mode,FALSE);

  filter=weed_get_plantptr_value(inst,"filter_class",&error);
  weed_call_deinit_func(inst);
  weed_free_instance(inst);

  if (is_bg) {
    weed_instances[key_to_instance[bg_generator_key][bg_generator_mode]]=NULL;
    key_to_instance[bg_generator_key][bg_generator_mode]=-1;
    if (mainw->rte&(GU641<<bg_generator_key)) mainw->rte^=(GU641<<bg_generator_key);
    bg_gen_to_start=bg_generator_key=bg_generator_mode=-1;
    pre_src_file=mainw->pre_src_file;
    mainw->pre_src_file=mainw->current_file;
    mainw->current_file=mainw->blend_file;
  }
  else {
    weed_instances[key_to_instance[fg_generator_key][fg_generator_mode]]=NULL;
    key_to_instance[fg_generator_key][fg_generator_mode]=-1;
    if (mainw->rte&(GU641<<fg_generator_key)) mainw->rte^=(GU641<<fg_generator_key);
    fg_gen_to_start=fg_generator_key=fg_generator_clip=fg_generator_mode=-1;
    if (mainw->blend_file==mainw->current_file) mainw->blend_file=-1;
  }

  if (!is_bg&&cfile->achans>0&&cfile->clip_type==CLIP_TYPE_GENERATOR) {
    // we started playing from an audio clip
    cfile->frames=cfile->start=cfile->end=0;
    cfile->ext_src=NULL;
    cfile->clip_type=CLIP_TYPE_DISK;
    cfile->hsize=cfile->vsize=0;
    cfile->pb_fps=cfile->fps=prefs->default_fps;
    return;
  }



  if (mainw->new_blend_file!=-1&&is_bg) {
    mainw->blend_file=mainw->new_blend_file;
    mainw->new_blend_file=-1;
    // close generator file and switch to original file if possible
    close_current_file(mainw->pre_src_file);
  }
  else {
    // close generator file and switch to original file if possible
    close_current_file(mainw->pre_src_file);
    if (mainw->current_file==current_file) mainw->clip_switched=clip_switched;
  }

  if (is_bg) {
    mainw->current_file=current_file;
    mainw->pre_src_file=pre_src_file;
  }

  if (mainw->current_file==-1) mainw->cancelled=CANCEL_GENERATOR_END;

}


static weed_plant_t **weed_channels_create (weed_plant_t *filter, gboolean in) {
  weed_plant_t **channels,**chantmpls;
  int num_channels;
  int i,j,error,pal;
  int ccount=0;
  int num_repeats;
  if (in) num_channels=weed_leaf_num_elements(filter,"in_channel_templates");
  else num_channels=weed_leaf_num_elements(filter,"out_channel_templates");

  if (num_channels==0) return NULL;
  if (in) chantmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  else chantmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);

  for (i=0;i<num_channels;i++) {
    if (weed_plant_has_leaf(chantmpls[i],"repeats")) ccount+=weed_get_int_value(chantmpls[i],"repeats",&error);
    else ccount+=1;
  }

  channels=g_malloc((ccount+1)*sizeof(weed_plant_t *));

  ccount=0;

  for (i=0;i<num_channels;i++) {
    if (weed_plant_has_leaf(chantmpls[i],"repeats")) num_repeats=weed_get_int_value(chantmpls[i],"repeats",&error);
    else num_repeats=1;
    for (j=0;j<num_repeats;j++) {
      channels[ccount]=weed_plant_new(WEED_PLANT_CHANNEL);
      weed_set_plantptr_value(channels[ccount],"template",chantmpls[i]);
      weed_set_int_value(channels[ccount],"current_palette",(pal=weed_get_int_value(chantmpls[i],"current_palette",&error)));
      if (weed_plant_has_leaf(chantmpls[i],"disabled")) (weed_set_boolean_value(channels[ccount],"disabled",weed_get_boolean_value(chantmpls[i],"disabled",&error)));
      weed_add_plant_flags(channels[ccount],WEED_LEAF_READONLY_PLUGIN);
      ccount++;
    }
  }
  channels[ccount]=NULL;
  weed_free(chantmpls);
  return channels;
}


weed_plant_t **weed_params_create (weed_plant_t *filter, gboolean in) {
  // return set of parameters with default/host_default values
  // in==TRUE create in parameters for filter
  // in==FALSE create out parameters for filter (untested)

  weed_plant_t **params,**paramtmpls;
  int num_params;
  int i,error;

  if (in) num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  else num_params=weed_leaf_num_elements(filter,"out_parameter_templates");

  if (num_params==0) return NULL;
  if (in) paramtmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  else paramtmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  params=g_malloc((num_params+1)*sizeof(weed_plant_t *));

  for (i=0;i<num_params;i++) {
    params[i]=weed_plant_new(WEED_PLANT_PARAMETER);
    weed_set_plantptr_value(params[i],"template",paramtmpls[i]);
    if (weed_plant_has_leaf(paramtmpls[i],"host_default")) {
      weed_leaf_copy(params[i],"value",paramtmpls[i],"host_default");
    }
    else weed_leaf_copy(params[i],"value",paramtmpls[i],"default");
    weed_add_plant_flags(params[i],WEED_LEAF_READONLY_PLUGIN);
  }
  params[num_params]=NULL;
  weed_free(paramtmpls);
  return params;
}



static void set_default_channel_sizes (weed_plant_t **in_channels, weed_plant_t **out_channels) {
  // set some reasonable default channel sizes when we first init the effect
  int error,i,j;
  weed_plant_t *channel,*chantmpl;
  void **pixel_data;
  int numplanes,width,height;
  int *rowstrides,def_rowstride;
  gboolean is_gen=TRUE;

  for (i=0;in_channels!=NULL&&in_channels[i]!=NULL&&!(weed_plant_has_leaf(in_channels[i],"disabled")&&weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_TRUE);i++) {
    channel=in_channels[i];
    chantmpl=weed_get_plantptr_value(channel,"template",&error);
    if (!weed_plant_has_leaf(chantmpl,"is_audio")||weed_get_boolean_value(chantmpl,"is_audio",&error)==WEED_FALSE) {
      is_gen=FALSE;
      width=height=4;
      weed_set_int_value(channel,"width",width);
      weed_set_int_value(channel,"height",height);
      def_rowstride=width*3;
      // try to set channel size first
      set_channel_size(channel,320,240,1,&def_rowstride);
      
      // create empty data for the palette and get the actual sizes
      create_empty_pixel_data(channel);
      width=weed_get_int_value(channel,"width",&error);
      height=weed_get_int_value(channel,"height",&error);
      numplanes=weed_leaf_num_elements(channel,"rowstrides");
      rowstrides=weed_get_int_array(channel,"rowstrides",&error);
      set_channel_size(channel,width,height,numplanes,rowstrides);
      weed_free(rowstrides);
      pixel_data=weed_get_voidptr_array(channel,"pixel_data",&error);
      for (j=0;j<numplanes;j++) g_free(pixel_data[j]);
      weed_free(pixel_data);
    }
    else {
      if (mainw->current_file==-1) {
	weed_set_int_value(channel,"audio_channels",DEFAULT_AUDIO_CHANS);
	weed_set_int_value(channel,"audio_rate",DEFAULT_AUDIO_RATE);
      }
      else {
	weed_set_int_value(channel,"audio_channels",cfile->achans);
	weed_set_int_value(channel,"audio_rate",cfile->arate);
      }
      weed_set_boolean_value(channel,"audio_interleaf",WEED_FALSE);
      weed_set_int_value(channel,"audio_data_length",0);
      weed_set_voidptr_value(channel,"audio_data",NULL);
    }
  }

  for (i=0;out_channels!=NULL&&out_channels[i]!=NULL;i++) {
    channel=out_channels[i];
    chantmpl=weed_get_plantptr_value(channel,"template",&error);
    if (!weed_plant_has_leaf(chantmpl,"is_audio")||weed_get_boolean_value(chantmpl,"is_audio",&error)==WEED_FALSE) {
      width=is_gen?DEF_GEN_WIDTH:320;
      height=is_gen?DEF_GEN_HEIGHT:240;
      weed_set_int_value(channel,"width",width);
      weed_set_int_value(channel,"height",height);
      def_rowstride=width*3;
      // try to set channel size first
      set_channel_size(channel,width,height,1,&def_rowstride);
      
      // create empty data for the palette and get the actual sizes
      create_empty_pixel_data(channel);
      width=weed_get_int_value(channel,"width",&error);
      height=weed_get_int_value(channel,"height",&error);
      numplanes=weed_leaf_num_elements(channel,"rowstrides");
      rowstrides=weed_get_int_array(channel,"rowstrides",&error);
      set_channel_size(channel,width,height,numplanes,rowstrides);
      weed_free(rowstrides);
      pixel_data=weed_get_voidptr_array(channel,"pixel_data",&error);
      for (j=0;j<numplanes;j++) g_free(pixel_data[j]);
      weed_free(pixel_data);
    }
    else {
      if (mainw->current_file==-1) {
	weed_set_int_value(channel,"audio_channels",DEFAULT_AUDIO_CHANS);
	weed_set_int_value(channel,"audio_rate",DEFAULT_AUDIO_RATE);
      }
      else {
	weed_set_int_value(channel,"audio_channels",cfile->achans);
	weed_set_int_value(channel,"audio_rate",cfile->arate);
      }
      weed_set_boolean_value(channel,"audio_interleaf",WEED_FALSE);
      weed_set_int_value(channel,"audio_data_length",0);
      weed_set_voidptr_value(channel,"audio_data",NULL);
    }
  }
}

static inline int array_count(weed_plant_t **array, gboolean set_readonly) {
  int i=0;
  while (array[i]!=NULL) {
    if (set_readonly) weed_add_plant_flags(array[i],WEED_LEAF_READONLY_PLUGIN);
    i++;
  }
  return i;
}


static weed_plant_t *weed_create_instance (weed_plant_t *filter, weed_plant_t **inc, weed_plant_t **outc, weed_plant_t **inp, weed_plant_t **outp) {
  // here we create a new filter_instance from the ingredients: filter_class, in_channels, out_channels, in_parameters, out_parameters
  weed_plant_t *inst=weed_plant_new(WEED_PLANT_FILTER_INSTANCE);
  weed_set_plantptr_value(inst,"filter_class",filter);
  if (inc!=NULL) weed_set_plantptr_array(inst,"in_channels",array_count(inc,TRUE),inc);
  if (outc!=NULL) weed_set_plantptr_array(inst,"out_channels",array_count(outc,TRUE),outc);
  if (inp!=NULL) weed_set_plantptr_array(inst,"in_parameters",array_count(inp,TRUE),inp);
  if (outp!=NULL) weed_set_plantptr_array(inst,"out_parameters",array_count(outp,TRUE),outp);

  weed_add_plant_flags(inst,WEED_LEAF_READONLY_PLUGIN);
  return inst;
}


static inline int get_next_free_instance(void) {
  int i;
  for (i=next_free_instance+1;i!=next_free_instance&&!(next_free_instance==-1&&i==MAX_WEED_INSTANCES);i++) if (weed_instances[(i=(i==MAX_WEED_INSTANCES?0:i))]==NULL) return i;
  return -1;
}



weed_plant_t *weed_instance_from_filter(weed_plant_t *filter) {
  // return an instance from a filter
  weed_plant_t *new_instance;
  weed_plant_t **inc,**outc,**inp,**outp;

  // create channels from channel_templates
  inc=weed_channels_create (filter,TRUE);
  outc=weed_channels_create (filter,FALSE);

  set_default_channel_sizes (inc,outc); // we set the initial channel sizes to some reasonable defaults

  // create parameters from parameter_templates
  inp=weed_params_create(filter,TRUE);
  outp=weed_params_create(filter,FALSE);

  new_instance=weed_create_instance(filter,inc,outc,inp,outp);
 
  if (inc!=NULL) weed_free(inc);
  if (outc!=NULL) weed_free(outc);
  if (inp!=NULL) weed_free(inp);
  if (outp!=NULL) weed_free(outp);

  return new_instance;
}




gboolean weed_init_effect(int hotkey) {
  // mainw->osc_block should be set to TRUE before calling this function !
  weed_plant_t *filter;
  weed_plant_t *new_instance;
  gint num_tr_applied;
  gboolean fg_modeswitch=FALSE,is_trans=FALSE,gen_start=FALSE,is_modeswitch=FALSE;
  gint rte_keys=mainw->rte_keys;
  gint inc_count;
  weed_plant_t *event_list;
  gint ntracks;
  int error;

  if (hotkey<0) {
    is_modeswitch=TRUE;
    hotkey=-hotkey-1;
  }

  if (hotkey==fg_generator_key) {
    fg_modeswitch=TRUE;
  }

  if (!rte_key_valid (hotkey+1,FALSE)) {
    return FALSE;
  }

  if (next_free_instance==-1) {
    d_print(_ ("Warning - no more effect slots left !\n"));
    return FALSE;
  }

  inc_count=enabled_in_channels(weed_filters[key_to_fx[hotkey][key_modes[hotkey]]],FALSE);

  // TODO - block template channel changes
  // we must stop any old generators

  if (inc_count==0&&hotkey!=fg_generator_key&&mainw->num_tr_applied>0&&mainw->blend_file!=-1&&mainw->blend_file!=mainw->current_file&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR&&inc_count==0) {
    if (bg_gen_to_start==-1) {
      weed_generator_end(mainw->files[mainw->blend_file]->ext_src);
    }
    bg_gen_to_start=bg_generator_key=bg_generator_mode=-1;
    mainw->blend_file=-1;
  }

  if (mainw->current_file>0&&cfile->clip_type==CLIP_TYPE_GENERATOR&&(fg_modeswitch||(inc_count==0&&mainw->num_tr_applied==0))) {
    if (mainw->noswitch||mainw->is_processing||mainw->preview) return FALSE; // stopping fg gen will cause clip to switch
    if (mainw->playing_file>-1&&mainw->whentostop==STOP_ON_VID_END&&inc_count!=0) {
      mainw->cancelled=CANCEL_GENERATOR_END;
    }
    else {
      if (inc_count==0&&mainw->whentostop==STOP_ON_VID_END) mainw->whentostop=NEVER_STOP;
      weed_generator_end (cfile->ext_src);
      fg_generator_key=fg_generator_clip=fg_generator_mode=-1;
      if (mainw->current_file>-1&&(cfile->achans==0||cfile->frames>0)) {
	// in case we switched to bg clip, and bg clip was gen
	// otherwise we will get killed in generator_start
	mainw->current_file=-1;  
      }
    }
  }

  if (inc_count==2) {
    mainw->num_tr_applied++; // increase trans count
    if (mainw->num_tr_applied==1&&!is_modeswitch) {
      mainw->blend_file=mainw->current_file;
    }
  }
  else if (inc_count==0) {
    // aha - a generator
     if (mainw->playing_file==-1) {
      // if we are not playing, we will postpone creating the instance
      // this is a workaround for a problem in libvisual
      fg_gen_to_start=hotkey;
      fg_generator_key=hotkey;
      fg_generator_mode=key_modes[hotkey];
      gen_start=TRUE;
    }
     else if (!fg_modeswitch&&mainw->num_tr_applied==0&&(mainw->noswitch||mainw->is_processing||mainw->preview)) return FALSE;
  }

  filter=weed_filters[key_to_fx[hotkey][key_modes[hotkey]]];

  // TODO - unblock template channel changes

  new_instance=weed_instances[next_free_instance]=weed_instance_from_filter(filter);
  key_to_instance[hotkey][key_modes[hotkey]]=next_free_instance;

  update_host_info(new_instance);

  if (!gen_start) {
    if (weed_plant_has_leaf(filter,"init_func")) {
      weed_init_f *init_func_ptr_ptr;
      weed_init_f init_func;
      weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
      init_func=init_func_ptr_ptr[0];
      if (init_func!=NULL&&(error=(*init_func)(new_instance))!=WEED_NO_ERROR) {
	gint weed_error;
	gchar *filter_name=weed_get_string_value(filter,"name",&weed_error),*tmp;
	d_print ((tmp=g_strdup_printf (_ ("Failed to start instance %s, error code %d\n"),filter_name,error)));
	g_free(tmp);
	weed_free(filter_name);
	weed_free_instance(new_instance);
	weed_instances[key_to_instance[hotkey][key_modes[hotkey]]]=NULL;
	key_to_instance[hotkey][key_modes[hotkey]]=-1;
	if (is_trans) mainw->num_tr_applied--;
	return FALSE;
      }
    }
    set_param_gui_readonly(new_instance);
  }

  if (inc_count==0) {
    // generator start
    if (mainw->num_tr_applied>0&&!fg_modeswitch&&mainw->current_file>-1&&mainw->playing_file>-1) {
      // transition is on, make into bg clip
      bg_generator_key=hotkey;
      bg_generator_mode=key_modes[hotkey];
    }
    else {
      // no transition, make fg (or kb was grabbed for fg generator)
      fg_generator_key=hotkey;
      fg_generator_mode=key_modes[hotkey];
    }

    // start the generator and maybe start playing
    num_tr_applied=mainw->num_tr_applied;
    if (fg_modeswitch) mainw->num_tr_applied=0; // force to fg

    // TODO - be more descriptive with error
    if (!weed_generator_start(new_instance)) {
      int weed_error;
      gchar *filter_name=weed_get_string_value(filter,"name",&weed_error),*tmp;
      d_print ((tmp=g_strdup_printf (_ ("Unable to start generator %s\n"),filter_name)));
      g_free(tmp);
      weed_free(filter_name);
      if (mainw->num_tr_applied&&mainw->current_file>-1) {
	bg_gen_to_start=bg_generator_key=bg_generator_mode=-1;
      }
      else {
	fg_generator_key=fg_generator_clip=fg_generator_mode=-1;
      }
      if (fg_modeswitch) mainw->num_tr_applied=num_tr_applied;
      return FALSE;
    }

    // TODO - problem if modeswitch triggers playback
    if (fg_modeswitch) mainw->num_tr_applied=num_tr_applied;
    if (fg_generator_key!=-1) {
      mainw->rte|=(GU641<<fg_generator_key);
      mainw->clip_switched=TRUE;
    }
    if (bg_generator_key!=-1&&!fg_modeswitch) {
      mainw->rte|=(GU641<<bg_generator_key);
      if (rte_window!=NULL&&hotkey<prefs->rte_keys_virtual) rtew_set_keych(bg_generator_key,TRUE);
    }
  }

  if (rte_keys==hotkey) {
    mainw->rte_keys=rte_keys;
    mainw->blend_factor=weed_get_blend_factor(rte_keys);
  }
  if (rte_window!=NULL&&hotkey<prefs->rte_keys_virtual) rtew_set_param_button (hotkey,key_modes[hotkey],TRUE);

  next_free_instance=get_next_free_instance();

  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)&&inc_count>0) {
    // place this synchronous with the preceding frame
    event_list=append_filter_init_event (mainw->event_list,mainw->currticks-mainw->origticks,key_to_fx[hotkey][key_modes[hotkey]],-1);
    if (mainw->event_list==NULL) mainw->event_list=event_list;
    init_events[hotkey]=(void *)get_last_event(mainw->event_list);
    ntracks=weed_leaf_num_elements(init_events[hotkey],"in_tracks");
    pchains[hotkey]=filter_init_add_pchanges(mainw->event_list,get_weed_filter(key_to_fx[hotkey][key_modes[hotkey]]),init_events[hotkey],ntracks);
    create_filter_map(); // we create filter_map event_t * array with ordered effects
    mainw->event_list=append_filter_map_event (mainw->event_list,mainw->currticks-mainw->origticks,filter_map);
    weed_set_int_value(new_instance,"host_hotkey",hotkey);
  }

  return TRUE;
}


void weed_call_deinit_func(weed_plant_t *instance) {
  int error;
  weed_plant_t *filter=weed_get_plantptr_value(instance,"filter_class",&error);
  if (weed_plant_has_leaf(filter,"deinit_func")) {
    weed_deinit_f *deinit_func_ptr_ptr;
    weed_deinit_f deinit_func;
    weed_leaf_get(filter,"deinit_func",0,(void *)&deinit_func_ptr_ptr);
    deinit_func=deinit_func_ptr_ptr[0];
    if (deinit_func!=NULL) (*deinit_func)(instance);
  }
}




void weed_deinit_effect(int hotkey) {
  // mainw->osc_block should be set before calling this function !
  // caller should also handle mainw->rte

  weed_plant_t *instance;
  gboolean is_modeswitch=FALSE;
  gboolean was_transition=FALSE;
  gint num_in_chans;
  gint idx;

  if (hotkey<0) {
    is_modeswitch=TRUE;
    hotkey=-hotkey-1;
  }

  if (hotkey>FX_KEYS_MAX) return;

  if ((idx=key_to_instance[hotkey][key_modes[hotkey]])==-1) return;

  instance=weed_instances[idx];
  if (instance==NULL) return;

  num_in_chans=enabled_in_channels(weed_instances[idx],FALSE);

  if (num_in_chans==0) {
    // is generator
    if (mainw->playing_file>-1&&mainw->whentostop==STOP_ON_VID_END&&(hotkey!=bg_generator_key)) {
      mainw->cancelled=CANCEL_GENERATOR_END;
    }
    else {
      weed_generator_end (instance);
    }
    return;
  }
  else {
    if (rte_window!=NULL&&hotkey<prefs->rte_keys_virtual) rtew_set_param_button (hotkey,key_modes[hotkey],FALSE);
    close_pwindow(hotkey,key_modes[hotkey],FALSE);
    weed_call_deinit_func(instance);
  }

  if (num_in_chans==2) {
    was_transition=TRUE;
    mainw->num_tr_applied--;
  }

  weed_free_instance(instance);

  weed_instances[idx]=NULL;
  key_to_instance[hotkey][key_modes[hotkey]]=-1;

  if (was_transition&&!is_modeswitch) {
    if (mainw->num_tr_applied<1) {
       if (bg_gen_to_start!=-1) bg_gen_to_start=-1;
      if (mainw->blend_file!=-1&&mainw->blend_file!=mainw->current_file&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR) {
	// all transitions off, so end the bg generator
	weed_generator_end (mainw->files[mainw->blend_file]->ext_src);
      }
      mainw->blend_file=-1;
    }
  }
  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&init_events[hotkey]!=NULL&&(prefs->rec_opts&REC_EFFECTS)&&num_in_chans>0) {
    // place this synchronous with the preceding frame
    mainw->event_list=append_filter_deinit_event (mainw->event_list,mainw->currticks-mainw->origticks,init_events[hotkey],pchains[hotkey]);
    init_events[hotkey]=NULL;
    if (pchains[hotkey]!=NULL) g_free(pchains[hotkey]);
    create_filter_map(); // we create filter_map event_t * array with ordered effects
    mainw->event_list=append_filter_map_event (mainw->event_list,mainw->currticks-mainw->origticks,filter_map);
  }
}


void deinit_render_effects (void) {
  // during rendering/render preview, we use the "keys" FX_KEYS_MAX_VIRTUAL -> FX_KEYS_MAX
  // here we deinit all active ones, similar to weed_deinit_all, but we use higher keys
  register int i;

  for (i=FX_KEYS_MAX_VIRTUAL;i<FX_KEYS_MAX;i++) {
    if (key_to_instance[i][0]!=-1) {
      weed_deinit_effect (i);
      if (mainw->multitrack!=NULL&&mainw->multitrack->is_rendering) g_free(pchains[i]);
    }
  }
}


void weed_deinit_all(void) {
  // deinit all except generators
  // this is called on ctrl-0 or on shutdown
  int i;
  gint idx;

  mainw->osc_block=TRUE;
  if (mainw->playing_file==-1&&rte_window!=NULL) {
    rtew_set_keygr(-1);
    mainw->rte_keys=-1;
    mainw->last_grabable_effect=-1;
  }

  for (i=0;rte_key_valid (i+1,TRUE);i++) {
    if (mainw->playing_file==-1&&rte_window!=NULL) rtew_set_keych(i,FALSE);
    if ((mainw->rte&(GU641<<i))) {
      if ((idx=key_to_instance[i][key_modes[i]])!=-1&&weed_instances[idx]!=NULL) {
	if (enabled_in_channels(weed_instances[idx],FALSE)>0) {
	  weed_deinit_effect(i);
	  if (mainw->rte&(GU641<<i)) mainw->rte^=(GU641<<i);
	  if (rte_window!=NULL) {
	    rtew_set_keych(i,FALSE);
	    if (i==mainw->rte_keys) {
	      rtew_set_keygr(-1);
	    }
	  }
	  if (i==mainw->last_grabable_effect) mainw->last_grabable_effect=-1;
	}
      }
    }
  }
  mainw->rte_keys=-1;
  mainw->osc_block=FALSE;
}


/////////////////////
// special handling for generators (sources)


weed_plant_t *weed_layer_new_from_generator (weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *channel,**out_channels;
  int num_channels;
  weed_plant_t *filter;
  weed_process_f *process_func_ptr_ptr;
  weed_process_f process_func;
  int error;
  int palette;
  weed_plant_t *chantmpl;

  if (inst==NULL) return NULL;
  if ((num_channels=weed_leaf_num_elements(inst,"out_channels"))==0) return NULL;
  out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
  if ((channel=get_enabled_channel(inst,0,FALSE))==NULL) {
    weed_free(out_channels);
    return NULL;
  }
  weed_free(out_channels);

  chantmpl=weed_get_plantptr_value(channel,"template",&error);
  palette=weed_get_int_value(chantmpl,"current_palette",&error);
  weed_set_int_value(channel,"current_palette",palette);

  create_empty_pixel_data(channel);

  // align memory if necessary
  if (weed_plant_has_leaf(chantmpl,"alignment")) {
    int alignment=weed_get_int_value(chantmpl,"alignment",&error);
    void **pixel_data=weed_get_voidptr_array(channel,"pixel_data",&error);
    int numplanes=weed_leaf_num_elements(channel,"pixel_data");
    int height=weed_get_int_value(channel,"height",&error);
    int *rowstrides=weed_get_int_array(channel,"rowstrides",&error);
    align(pixel_data,alignment,numplanes,height,rowstrides);
    weed_set_voidptr_array(channel,"pixel_data",numplanes,pixel_data);
    weed_free(pixel_data);
    weed_free(rowstrides);
  }

  weed_set_double_value(inst,"fps",cfile->pb_fps);

  filter=weed_get_plantptr_value(inst,"filter_class",&error);
  weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
  process_func=process_func_ptr_ptr[0];
  process_func(inst,tc);

  return channel;
}


gboolean weed_generator_start (weed_plant_t *inst) {
  // make an "ephemeral clip"

  // cf. yuv4mpeg.c
  // start "playing" but receive frames from a plugin

  weed_plant_t **out_channels,*channel,*filter;
  gchar *filter_name;
  int error,num_channels;
  gint old_file=mainw->current_file,blend_file=mainw->blend_file;
  int palette;

  // create a virtual clip
  gint new_file=0;

  gboolean is_bg=FALSE;

  if (mainw->current_file<1||cfile->frames>0) {
    new_file=mainw->first_free_file;

    if (new_file==-1) {
      too_many_files();
      return FALSE;
    }
    
    if (mainw->current_file>-1&&cfile!=NULL&&cfile->clip_type==CLIP_TYPE_GENERATOR&&mainw->num_tr_applied==0) {
      close_current_file(0);
      old_file=-1;
    }
  }

  if (mainw->is_processing&&!mainw->preview) return FALSE;

  if ((mainw->preview||(mainw->current_file>-1&&cfile!=NULL&&cfile->opening))&&(mainw->num_tr_applied==0||mainw->blend_file==-1||mainw->blend_file==mainw->current_file)) return FALSE;

  if (mainw->playing_file==-1) mainw->pre_src_file=mainw->current_file;

  if (old_file!=-1&&mainw->blend_file!=-1&&mainw->blend_file!=mainw->current_file&&mainw->num_tr_applied>0&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR) {
    weed_generator_end(mainw->files[mainw->blend_file]->ext_src);
    mainw->current_file=mainw->blend_file;
  }

  // old_file can also be -1 if we are doing a fg_modeswitch
  if (old_file>-1&&mainw->playing_file>-1&&mainw->num_tr_applied>0) is_bg=TRUE;

  filter=weed_get_plantptr_value(inst,"filter_class",&error);

  if (mainw->current_file>0&&cfile->frames==0) {
    // audio clip - we will init the generator as fg video in the same clip
    // otherwise known as "showoff mode"
    new_file=mainw->current_file;
    cfile->frames=1;
    cfile->start=cfile->end=1;
    cfile->clip_type=CLIP_TYPE_GENERATOR;
    cfile->frameno=1;
    mainw->play_start=mainw->play_end=1;
    mainw->startticks=mainw->currticks;
  }

  if (new_file!=mainw->current_file) {
    mainw->current_file=new_file;

    cfile=(file *)(g_malloc(sizeof(file)));
    g_snprintf (cfile->handle,256,"ephemeral%d",mainw->current_file);
    create_cfile();
    cfile->clip_type=CLIP_TYPE_GENERATOR;
    get_next_free_file();
    
    filter_name=weed_get_string_value(filter,"name",&error);
    g_snprintf(cfile->type,40,"generator:%s",filter_name);
    g_snprintf(cfile->file_name,256,"generator: %s",filter_name);
    g_snprintf(cfile->name,256,"generator: %s",filter_name);
    weed_free(filter_name);
    cfile->achans=0;
    cfile->asampsize=0;

    // open as a clip with 1 frame
    cfile->start=cfile->end=cfile->frames=1;
    cfile->arps=cfile->arate=0;
    cfile->changed=FALSE;
  }

  cfile->ext_src=inst;

  if (is_bg) mainw->blend_file=mainw->current_file;

  if (!is_bg||old_file==-1||old_file==new_file) fg_generator_clip=new_file;

  if (weed_plant_has_leaf(inst,"target_fps")) {
    // if plugin sets "target_fps" for the instance we assume there is some special reason
    // and use that
    cfile->pb_fps=cfile->fps=weed_get_double_value(inst,"target_fps",&error);
  }
  else {
    if (weed_plant_has_leaf(filter,"host_fps")) cfile->pb_fps=cfile->fps=weed_get_double_value(filter,"host_fps",&error);
    else if (weed_plant_has_leaf(filter,"target_fps")) cfile->pb_fps=cfile->fps=weed_get_double_value(filter,"target_fps",&error);
    else {
      if (!mainw->record||!(prefs->rec_opts&REC_EFFECTS)) cfile->fps=cfile->pb_fps=FPS_MAX; // play as fast as we can
      else cfile->pb_fps=cfile->fps=prefs->default_fps; // unless we are recording
    }
  }

  if ((num_channels=weed_leaf_num_elements(inst,"out_channels"))==0) {
    if (is_bg&&old_file!=-1) mainw->current_file=old_file;
    return FALSE;
  }
  out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
  if ((channel=get_enabled_channel(inst,0,FALSE))==NULL) {
    weed_free(out_channels);
    if (is_bg&&old_file!=-1) mainw->current_file=old_file;
    return FALSE;
  }
  weed_free(out_channels);

  cfile->hsize=weed_get_int_value(channel,"width",&error);
  cfile->vsize=weed_get_int_value(channel,"height",&error);

  //if (mainw->play_window!=NULL&&!is_bg&&!mainw->fs) resize_play_window();

  palette=weed_get_int_value(channel,"current_palette",&error);
  if (palette==WEED_PALETTE_RGBA32||palette==WEED_PALETTE_ARGB32||palette==WEED_PALETTE_BGRA32) cfile->bpp=32;
  else cfile->bpp=24;

  cfile->opening=FALSE;
  cfile->proc_ptr=NULL;

  // allow clip switching
  cfile->is_loaded=TRUE;

  // if not playing, start playing
  if (mainw->playing_file==-1) {
    if (!is_bg||old_file==-1||old_file==new_file) {
      switch_to_file((mainw->current_file=old_file),new_file);
      set_main_title(cfile->file_name,0);
      mainw->play_start=1;
      mainw->play_end=INT_MAX;
      if (is_bg) {
	mainw->blend_file=mainw->current_file;
	if (old_file!=-1) mainw->current_file=old_file;
      }
    }
    else {
      mainw->blend_file=mainw->current_file;
      mainw->current_file=old_file;
      mainw->play_start=cfile->start;
      mainw->play_end=cfile->end;
      mainw->playing_sel=FALSE;
    }

    if (mainw->play_window!=NULL&&old_file==-1) {
      // usually preview or load_preview_frame would do this
      g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=TRUE;
    }

    play_file();


  }
  else {
    // already playing

    if (old_file!=-1&&mainw->files[old_file]!=NULL) {
      if (mainw->files[old_file]->clip_type==CLIP_TYPE_DISK||mainw->files[old_file]->clip_type==CLIP_TYPE_FILE) mainw->pre_src_file=old_file;
      mainw->current_file=old_file;
    }

    if (!is_bg||old_file==-1||old_file==new_file) {
      if (mainw->current_file==-1) mainw->current_file=new_file;

      if (new_file!=old_file) {
	do_quick_switch (new_file);
	if (mainw->files[mainw->new_blend_file]!=NULL) mainw->blend_file=mainw->new_blend_file;
	if (!is_bg&&blend_file!=-1&&mainw->files[blend_file]!=NULL) mainw->blend_file=blend_file;
	mainw->new_blend_file=-1;
      }    
      else {
	gtk_widget_show(mainw->playframe);
	resize(1);
      }
      //if (old_file==-1) mainw->whentostop=STOP_ON_VID_END;
    }
    else {
      if (mainw->current_file==-1) mainw->current_file=new_file;
      else mainw->blend_file=new_file;
    }

    if (mainw->cancelled==CANCEL_GENERATOR_END) mainw->cancelled=CANCEL_NONE;
    if (old_file==-1&&mainw->play_window!=NULL) {
      g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=TRUE;
    }
  }

  return TRUE;
}



void weed_bg_generator_end (weed_plant_t *inst) {
  // when we stop with a bg generator we want it to be restarted next time
  // i.e we will need a new clip for it
  int bg_gen_key=bg_generator_key;
  weed_generator_end (inst);
  bg_gen_to_start=bg_gen_key;
}



gboolean weed_playback_gen_start (void) {
  // init generators on pb. We have to do this after audio startup
  weed_plant_t *inst=NULL,*filter;
  int error=WEED_NO_ERROR;
  int weed_error;
  gchar *filter_name;
  gint bgs=bg_gen_to_start;
  gboolean was_started=FALSE;

  if (mainw->is_rendering) return TRUE;

  if (fg_gen_to_start==bg_gen_to_start) bg_gen_to_start=-1;

  if (cfile->frames==0&&fg_gen_to_start==-1&&bg_gen_to_start!=-1) {
    fg_gen_to_start=bg_gen_to_start;
    bg_gen_to_start=-1;
  }

  mainw->osc_block=TRUE;

  if (fg_gen_to_start!=-1) {
    if (enabled_in_channels(weed_filters[key_to_fx[fg_gen_to_start][key_modes[fg_gen_to_start]]],FALSE)==0) { // check is still gen
      inst=weed_instances[key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]]];
      if (inst!=NULL) {
	filter=weed_get_plantptr_value(inst,"filter_class",&weed_error);
	if (weed_plant_has_leaf(filter,"init_func")) {
	  weed_init_f *init_func_ptr_ptr;
	  weed_init_f init_func;
	  weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
	  init_func=init_func_ptr_ptr[0];
	  update_host_info(inst);
	  if (init_func!=NULL) error=(*init_func)(inst);
	}
      }
      if (error!=WEED_NO_ERROR) {
	if (inst!=NULL) {
	  gchar *tmp;
	  filter=weed_get_plantptr_value(inst,"filter_class",&weed_error);
	  filter_name=weed_get_string_value(filter,"name",&weed_error);
	  d_print ((tmp=g_strdup_printf (_ ("Failed to start generator %s\n"),filter_name)));
	  g_free(tmp);
	  weed_free(filter_name);
	  weed_call_deinit_func(inst);
	  weed_free_instance(inst);
	}
	weed_instances[key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]]]=NULL;
	key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]]=-1;
	fg_gen_to_start=-1;
	cfile->ext_src=NULL;
	mainw->osc_block=FALSE;
	return FALSE;
      }
      if (weed_plant_has_leaf(inst,"target_fps")) {
	gint current_file=mainw->current_file;
	mainw->current_file=fg_generator_clip;
	cfile->fps=weed_get_double_value(inst,"target_fps",&error);
	set_main_title(cfile->file_name,0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->fps);
	mainw->current_file=current_file;
      }
      mainw->clip_switched=TRUE;
      cfile->ext_src=inst;
      next_free_instance=get_next_free_instance();
      if (rte_window!=NULL) rtew_set_param_button (fg_gen_to_start,key_modes[fg_gen_to_start],TRUE);
    }
    fg_gen_to_start=-1;
  }


  if (bg_gen_to_start!=-1) {
    if (enabled_in_channels(weed_filters[key_to_fx[bg_gen_to_start][key_modes[bg_gen_to_start]]],FALSE)==0) { // check is still gen
      if (key_to_instance[bg_gen_to_start][key_modes[bg_gen_to_start]]==-1) {
	// restart bg generator
	if (!weed_init_effect(bg_gen_to_start)) return TRUE;
	was_started=TRUE;
      }
      inst=weed_instances[key_to_instance[bgs][key_modes[bgs]]];
      
      if (inst==NULL) {
	// 2nd playback
	gint playing_file=mainw->playing_file;
	mainw->playing_file=-100; //kludge to stop playing a second time
	if (!weed_init_effect (bg_gen_to_start)) {
	  error++;
	}
	mainw->playing_file=playing_file;
	inst=weed_instances[key_to_instance[bg_gen_to_start][key_modes[bg_gen_to_start]]];
      }
      else {
	if (!was_started) {
	  filter=weed_get_plantptr_value(inst,"filter_class",&weed_error);
	  if (weed_plant_has_leaf(filter,"init_func")) {
	    weed_init_f *init_func_ptr_ptr;
	    weed_init_f init_func;
	    weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
	    init_func=init_func_ptr_ptr[0];
	    update_host_info(inst);
	    if (init_func!=NULL) error=(*init_func)(inst);
	  }
	}
      }
      
      if (error!=WEED_NO_ERROR) {
	if (inst!=NULL) {
	  gchar *tmp;
	  filter=weed_get_plantptr_value(inst,"filter_class",&weed_error);
	  filter_name=weed_get_string_value(filter,"name",&weed_error);
	  d_print ((tmp=g_strdup_printf (_ ("Failed to start generator %s, error %d\n"),filter_name,error)));
	  g_free(tmp);
	  weed_free(filter_name);
	  weed_call_deinit_func(inst);
	  weed_free_instance(inst);
	}
	weed_instances[key_to_instance[bg_gen_to_start][key_modes[bg_gen_to_start]]]=NULL;
	key_to_instance[bg_gen_to_start][key_modes[bg_gen_to_start]]=-1;
	bg_gen_to_start=-1;
	mainw->blend_file=-1;
	if (mainw->rte&(GU641<<ABS(bg_gen_to_start))) mainw->rte^=(GU641<<ABS(bg_gen_to_start));
	mainw->osc_block=FALSE;
	return FALSE;
      }
      mainw->files[mainw->blend_file]->ext_src=inst;
      next_free_instance=get_next_free_instance();
      if (rte_window!=NULL) rtew_set_param_button (bg_gen_to_start,key_modes[bg_gen_to_start],TRUE);
    }
    bg_gen_to_start=-1;
  }

  if (inst!=NULL) set_param_gui_readonly(inst);

  mainw->osc_block=FALSE;

  return TRUE;
}
 
//////////////////////////////////////////////////////////////////////////////
// weed parameter functions


gboolean is_hidden_param(weed_plant_t *plant, int i) {
  // find out if in_param i is visible or not for plant. Plant can be an instance or a filter
  gboolean visible=TRUE;
  weed_plant_t **wtmpls;
  int error,flags=0;
  weed_plant_t *filter,*gui=NULL;
  int num_params=0;
  weed_plant_t *wtmpl;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) filter=weed_get_plantptr_value(plant,"filter_class",&error);
  else filter=plant;

  if (weed_plant_has_leaf(filter,"in_parameter_templates")) num_params=weed_leaf_num_elements(filter,"in_parameter_templates");

  if (num_params==0) return TRUE;

  wtmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  wtmpl=wtmpls[i];

  if (weed_plant_has_leaf(wtmpl,"flags")) flags=weed_get_int_value(wtmpl,"flags",&error);
  if (weed_plant_has_leaf(wtmpl,"gui")) gui=weed_get_plantptr_value(wtmpl,"gui",&error);
  if (!(flags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE)&&(gui==NULL||(!weed_plant_has_leaf(gui,"hidden")))) {
    if (gui!=NULL) {
      if (weed_plant_has_leaf(gui,"copy_value_to")) {
	int copyto=weed_get_int_value(gui,"copy_value_to",&error);
	int flags2=0,param_hint,param_hint2;
	weed_plant_t *wtmpl2;
	if (copyto==i||copyto<0) copyto=-1;
	if (copyto>-1) {
	  visible=FALSE;
	  wtmpl2=wtmpls[copyto];
	  if (weed_plant_has_leaf(wtmpl2,"flags")) flags2=weed_get_int_value(wtmpl2,"flags",&error);
	  param_hint=weed_get_int_value(wtmpl,"hint",&error);
	  param_hint2=weed_get_int_value(wtmpl2,"hint",&error);
	  if (param_hint==param_hint2&&((flags2&WEED_PARAMETER_VARIABLE_ELEMENTS)||weed_leaf_num_elements(wtmpl,"default")==weed_leaf_num_elements(wtmpl2,"default"))) {
	    if (!(flags2&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE)) {
	      visible=TRUE;
	    }}}}}}

  weed_free(wtmpls);
  return !visible;
}


int get_transition_param(weed_plant_t *filter) {
  int error,num_params,i;
  weed_plant_t **in_ptmpls;

  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return -1; // has no in_parameters

  num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  for (i=0;i<num_params;i++) {
    if (weed_plant_has_leaf(in_ptmpls[i],"transition")&&weed_get_boolean_value(in_ptmpls[i],"transition",&error)==WEED_TRUE) {
      weed_free(in_ptmpls);
      return i;
    }
  }
  weed_free(in_ptmpls);
  return -1;
}


int get_master_vol_param(weed_plant_t *filter) {
  int error,num_params,i;
  weed_plant_t **in_ptmpls;

  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return -1; // has no in_parameters

  num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  for (i=0;i<num_params;i++) {
    if (weed_plant_has_leaf(in_ptmpls[i],"is_volume_master")&&weed_get_boolean_value(in_ptmpls[i],"is_volume_master",&error)==WEED_TRUE) {
      weed_free(in_ptmpls);
      return i;
    }
  }
  weed_free(in_ptmpls);
  return -1;
}


gboolean is_perchannel_multiw(weed_plant_t *param) {
  // updated for weed spec 1.1
  int error;
  int flags=0;
  weed_plant_t *ptmpl;
  if (WEED_PLANT_IS_PARAMETER(param)) ptmpl=weed_get_plantptr_value(param,"template",&error);
  else ptmpl=param;
  if (weed_plant_has_leaf(ptmpl,"flags")) flags=weed_get_int_value(ptmpl,"flags",&error);
  if (flags&WEED_PARAMETER_ELEMENT_PER_CHANNEL) return TRUE;
  return FALSE;
}



gboolean has_perchannel_multiw(weed_plant_t *filter) {
  int error,nptmpl,i;
  weed_plant_t **ptmpls;

  if (!weed_plant_has_leaf(filter,"in_parameter_templates")||(nptmpl=weed_leaf_num_elements(filter,"in_parameter_templates"))==0) return FALSE;

  ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  for (i=0;i<nptmpl;i++) {
    if (is_perchannel_multiw(ptmpls[i])) {
      weed_free(ptmpls);
      return TRUE;
    }
  }

  weed_free(ptmpls);
  return FALSE;
}





weed_plant_t *weed_inst_in_param (weed_plant_t *inst, int param_num, gboolean skip_hidden) {
  weed_plant_t **in_params,*param=NULL;
  int error,num_params;

  if (!weed_plant_has_leaf(inst,"in_parameters")) return NULL; // has no in_parameters

  num_params=weed_leaf_num_elements(inst,"in_parameters");
  if (num_params<=param_num) return NULL; // invalid parameter number

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  if (!skip_hidden) param=in_params[param_num];

  else {
    gint count=0;
    int i;
    
    for (i=0;i<num_params;i++) {
	if (!is_hidden_param(inst,i)) {
	  if (count==param_num) {
	    param=in_params[count];
	    break;
	  }
	  count++;
	}
      }
    }

  weed_free(in_params);
  return param;
}

char *get_weed_display_string (weed_plant_t *inst, int pnum) {
  // TODO - for setting defaults, we will need to create params
  char *disp_string;
  weed_plant_t *param=weed_inst_in_param(inst,pnum,FALSE);
  weed_plant_t *ptmpl,*gui;
  int error;
  weed_display_f display_func;

  if (param==NULL) return NULL;

  ptmpl=weed_get_plantptr_value(param,"template",&error);
  if (!weed_plant_has_leaf(ptmpl,"gui")) return NULL;
  gui=weed_get_plantptr_value(ptmpl,"gui",&error);
  if (!weed_plant_has_leaf(gui,"display_func")) return NULL;

  display_func=weed_get_voidptr_value(gui,"display_func",&error);

  weed_leaf_set_flags(gui,"display_value",(weed_leaf_get_flags(gui,"display_value")|WEED_LEAF_READONLY_PLUGIN)^WEED_LEAF_READONLY_PLUGIN);
  (*display_func)(param);
  weed_leaf_set_flags(gui,"display_value",(weed_leaf_get_flags(gui,"display_value")|WEED_LEAF_READONLY_PLUGIN));

  if (!weed_plant_has_leaf(gui,"display_value")) return NULL;
  if (weed_leaf_seed_type(gui,"display_value")!=WEED_SEED_STRING) return NULL;
  disp_string=weed_get_string_value(gui,"display_value",&error);

  return disp_string;
}



void rec_param_change(weed_plant_t *inst, int pnum) {
  int error;
  weed_timecode_t tc=get_event_timecode(get_last_event(mainw->event_list));
  int key=weed_get_int_value(inst,"host_hotkey",&error);
  weed_plant_t *in_param=weed_inst_in_param(inst,pnum,FALSE);

  mainw->event_list=append_param_change_event(mainw->event_list,tc,pnum,in_param,init_events[key],pchains[key]);
}



#define KEYSCALE 255.


void weed_set_blend_factor(int hotkey) {
  // mainw->osc_block should be set to TRUE before calling this function !
  weed_plant_t *inst,*in_param,*in_param2=NULL,*paramtmpl;
  int idx=key_to_instance[hotkey][key_modes[hotkey]],error;
  int vali,mini,maxi;
  double vald,mind,maxd;
  GList *list=NULL;
  int param_hint;
  int copyto=-1;
  weed_plant_t *gui=NULL;
  weed_plant_t **all_params;
  int pnum=0;
  gboolean copy_ok=FALSE;
  weed_timecode_t tc=0;
  gint inc_count;

  if (hotkey<0) return;
  idx=key_to_instance[hotkey][key_modes[hotkey]];

  if (idx==-1||(inst=weed_instances[idx])==NULL) return;

  in_param=weed_inst_in_param(inst,0,TRUE); // - skip hidden params
  if (in_param==NULL) return;

  all_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  while (all_params[pnum]!=in_param) pnum++;

  paramtmpl=weed_get_plantptr_value(in_param,"template",&error);
  param_hint=weed_get_int_value(paramtmpl,"hint",&error);

  if (weed_plant_has_leaf(paramtmpl,"gui")) gui=weed_get_plantptr_value(paramtmpl,"gui",&error);

  if (gui!=NULL) {
    if (weed_plant_has_leaf(gui,"copy_value_to")) {
      int param_hint2,flags2=0;
      weed_plant_t *paramtmpl2;
      copyto=weed_get_int_value(gui,"copy_value_to",&error);
      //if (copyto==in_param||copyto<0) copyto=-1;
      if (copyto>-1) {
	copy_ok=FALSE;
	paramtmpl2=weed_get_plantptr_value(all_params[copyto],"template",&error);
	if (weed_plant_has_leaf(paramtmpl2,"flags")) flags2=weed_get_int_value(paramtmpl2,"flags",&error);
	param_hint2=weed_get_int_value(paramtmpl2,"hint",&error);
	if (param_hint==param_hint2&&((flags2&WEED_PARAMETER_VARIABLE_ELEMENTS)||weed_leaf_num_elements(paramtmpl,"default")==weed_leaf_num_elements(paramtmpl2,"default"))) {
	  if (!(flags2&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE)) {
	    copy_ok=TRUE;
	  }}}}}

  if (!copy_ok) copyto=-1;
  else (in_param2=all_params[copyto]);

  weed_free(all_params);
  inc_count=enabled_in_channels(inst,FALSE);

  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)&&inc_count>0) {
    tc=get_event_timecode(get_last_event(mainw->event_list));
    mainw->event_list=append_param_change_event(mainw->event_list,tc,pnum,in_param,init_events[hotkey],pchains[hotkey]);
    if (copyto>-1) {
      mainw->event_list=append_param_change_event(mainw->event_list,tc,copyto,in_param2,init_events[hotkey],pchains[hotkey]);
    }
  }

  switch (param_hint) {
  case WEED_HINT_INTEGER:
    vali=weed_get_int_value(in_param,"value",&error);
    mini=weed_get_int_value(paramtmpl,"min",&error);
    maxi=weed_get_int_value(paramtmpl,"max",&error);

    weed_set_int_value (in_param,"value",(int)((gdouble)mini+(mainw->blend_factor/KEYSCALE*(gdouble)(maxi-mini))+.5));
    vali=weed_get_int_value (in_param,"value",&error);

    list=g_list_append(list,g_strdup_printf("%d",vali));
    list=g_list_append(list,g_strdup_printf("%d",mini));
    list=g_list_append(list,g_strdup_printf("%d",maxi));
    update_pwindow(hotkey,0,list);
    g_list_free_strings(list);
    g_list_free(list);

    break;
  case WEED_HINT_FLOAT:
    vald=weed_get_double_value(in_param,"value",&error);
    mind=weed_get_double_value(paramtmpl,"min",&error);
    maxd=weed_get_double_value(paramtmpl,"max",&error);

    weed_set_double_value (in_param,"value",mind+(mainw->blend_factor/KEYSCALE*(maxd-mind)));
    vald=weed_get_double_value (in_param,"value",&error);

    list=g_list_append(list,g_strdup_printf("%.4f",vald));
    list=g_list_append(list,g_strdup_printf("%.4f",mind));
    list=g_list_append(list,g_strdup_printf("%.4f",maxd));
    update_pwindow(hotkey,0,list);
    g_list_free_strings(list);
    g_list_free(list);

    break;
  case WEED_HINT_SWITCH:
    vali=!!(int)mainw->blend_factor;
    weed_set_boolean_value (in_param,"value",vali);
    vali=weed_get_boolean_value (in_param,"value",&error);
    mainw->blend_factor=(gdouble)vali;

    list=g_list_append(list,g_strdup_printf("%d",vali));
    update_pwindow(hotkey,0,list);
    g_list_free_strings(list);
    g_list_free(list);

    break;
  }

  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)&&inc_count>0) {
    mainw->event_list=append_param_change_event(mainw->event_list,tc,pnum,in_param,init_events[hotkey],pchains[hotkey]);
    if (copyto>-1) {
      weed_leaf_copy(in_param2,"value",in_param,"value");
      mainw->event_list=append_param_change_event(mainw->event_list,tc,copyto,in_param2,init_events[hotkey],pchains[hotkey]);
    }
  }


}



gint weed_get_blend_factor(int hotkey) {
  // mainw->osc_block should be set to TRUE before calling this function !

  weed_plant_t *inst,*in_param,*paramtmpl;
  int idx,error;
  int vali,mini,maxi;
  double vald,mind,maxd;
  int weed_hint;

  if (hotkey<0) return 0;
  idx=key_to_instance[hotkey][key_modes[hotkey]];

  if (idx==-1||(inst=weed_instances[idx])==NULL) return 0;

  if (!weed_plant_has_leaf(inst,"in_parameters")) return 0;
  in_param=weed_get_plantptr_value(inst,"in_parameters",&error);
  if (in_param==NULL) return 0;

  paramtmpl=weed_get_plantptr_value(in_param,"template",&error);
  weed_hint=weed_get_int_value(paramtmpl,"hint",&error);

  switch (weed_hint) {
  case WEED_HINT_INTEGER:
    vali=weed_get_int_value(in_param,"value",&error);
    mini=weed_get_int_value(paramtmpl,"min",&error);
    maxi=weed_get_int_value(paramtmpl,"max",&error);
    return (gdouble)(vali-mini)/(gdouble)(maxi-mini)*KEYSCALE;
  case WEED_HINT_FLOAT:
    vald=weed_get_double_value(in_param,"value",&error);
    mind=weed_get_double_value(paramtmpl,"min",&error);
    maxd=weed_get_double_value(paramtmpl,"max",&error);
    return (vald-mind)/(maxd-mind)*KEYSCALE;
  case WEED_HINT_SWITCH:
    vali=weed_get_boolean_value(in_param,"value",&error);
    return vali;
  }

  return 0;
}




















////////////////////////////////////////////////////////////////////////


gchar *weed_instance_get_type(gint idx) {
  // return value should be free'd after use
  return weed_category_to_text(weed_filter_categorise(weed_instances[idx],enabled_in_channels(weed_instances[idx],FALSE),enabled_out_channels(weed_instances[idx],FALSE)),FALSE);
}




gchar *rte_keymode_get_type (gint key, gint mode) {
  // return value should be free'd after use
  gchar *type=g_strdup ("");
  weed_plant_t *filter;
  gint idx,inst_idx;

  key--;
  if (!rte_keymode_valid(key+1,mode,TRUE)) return g_strdup("");

  if ((idx=key_to_fx[key][mode])==-1) return type;
  if ((filter=weed_filters[idx])==NULL) return type;

  mainw->osc_block=TRUE;

  if ((inst_idx=key_to_instance[key][mode])!=-1) {
    // return details for instance
    g_free(type);
    type=weed_instance_get_type(inst_idx);
  }
  else type=weed_filter_get_type(idx);

  mainw->osc_block=FALSE;
  return type;
}

///////////////////////////////////////////////////////////////////////////////

int get_next_free_key(void) {
  // 0 based
  int i,free_key;
  free_key=next_free_key;
  for (i=free_key+1;i<FX_KEYS_MAX;i++) {
    if (key_to_fx[i][0]==-1) {
      next_free_key=i;
      break;
    }
  }
  if (i==FX_KEYS_MAX) next_free_key=-1;
  return free_key;
}


gboolean weed_delete_effectkey (gint key, gint mode) {
  // delete the effect binding for key/mode and move higher numbered slots down
  // also moves the active mode if applicable
  // returns FALSE if there was no effect bound to key/mode

  int oldkeymode=key_modes[--key];
  gint orig_mode=mode;
  gboolean was_started=FALSE;
  gint modekey=key;
  gchar *tmp;

  if (key_to_fx[key][mode]==-1) return FALSE;

  for (;mode<MAX_MODES_PER_KEY;mode++) {

    mainw->osc_block=TRUE;
    if (mode==MAX_MODES_PER_KEY-1||key_to_fx[key][mode+1]==-1) {
      if (key_to_instance[key][mode]!=-1) {
	was_started=TRUE;
	if (key_modes[key]==mode) modekey=-key-1;
	else key_modes[key]=mode;
	weed_deinit_effect(modekey);
	key_modes[key]=oldkeymode;
      }

      key_to_fx[key][mode]=-1;

      if (mode==orig_mode&&key_modes[key]==mode) {
	key_modes[key]=0;
	if (was_started) {
	  if (key_to_fx[key][0]!=-1) weed_init_effect(modekey);
	  else if (mainw->rte&(GU641<<key)) mainw->rte^=(GU641<<key);
	}
      }
      break; // quit the loop
    }
    else {
      rte_switch_keymode(key+1,mode,(tmp=make_weed_hashname(key_to_fx[key+1][mode+1])));
      g_free(tmp);
    }
  }

  if (key>=FX_KEYS_MAX_VIRTUAL&&key<next_free_key) next_free_key=key;

  mainw->osc_block=FALSE;
  if (key_modes[key]>orig_mode) key_modes[key]--;

  return TRUE;
}








/////////////////////////////////////////////////////////////////////////////

gboolean rte_key_valid (int key, gboolean is_userkey) {
  key--;
  if (key<0||(is_userkey&&key>=FX_KEYS_MAX_VIRTUAL)||key>=FX_KEYS_MAX) return FALSE;
  if (key_to_fx[key][key_modes[key]]==-1) return FALSE;
  return TRUE;
}

gboolean rte_keymode_valid (gint key, gint mode, gboolean is_userkey) {
  if (key<1||(is_userkey&&key>FX_KEYS_MAX_VIRTUAL)||key>FX_KEYS_MAX||mode<0||mode>=MAX_MODES_PER_KEY) return FALSE;
  if (key_to_fx[--key][mode]==-1) return FALSE;
  return TRUE;
}

gint rte_keymode_get_filter_idx(gint key, gint mode) {
  return (key_to_fx[--key][mode]);
}

int rte_key_getmode (gint key) {
  return key_modes[--key];
}

int rte_key_getmaxmode (gint key) {
  register int i;

  key--;

  for (i=0;i<MAX_MODES_PER_KEY;i++) {
    if (key_to_fx[key][i]==-1) return i;
  }
  return 0;
}

weed_plant_t *rte_keymode_get_instance(gint key, gint mode) {
  weed_plant_t *inst;

  key--;
  if (!rte_keymode_valid(key+1,mode,FALSE)) return NULL;
  mainw->osc_block=TRUE;
  if (key_to_instance[key][mode]==-1) {
    mainw->osc_block=FALSE;
    return NULL;
  }
  inst=weed_instances[key_to_instance[key][mode]];
  mainw->osc_block=FALSE;
  return inst;
}


weed_plant_t *rte_keymode_get_filter(gint key, gint mode) {
  key--;
  if (!rte_keymode_valid(key+1,mode,FALSE)) return NULL;
  return weed_filters[key_to_fx[key][mode]];
}


gchar *weed_filter_get_name(gint idx) {
  // return value should be g_free'd after use
  weed_plant_t *filter;
  int error;
  gchar *filter_name,*retval;

  if (idx==-1) return g_strdup("");
  if ((filter=weed_filters[idx])==NULL) return g_strdup("");
  filter_name=weed_get_string_value(filter,"name",&error);
  retval=g_strdup(filter_name); // copy so we can use g_free() instead of weed_free()
  weed_free(filter_name);
  return retval;
}


gchar *rte_keymode_get_filter_name (gint key, gint mode) {
  // return value should be g_free'd after use
  key--;
  if (!rte_keymode_valid(key+1,mode,TRUE)) return g_strdup("");
  return (weed_filter_get_name(key_to_fx[key][mode]));
}


gchar *rte_keymode_get_plugin_name(gint key, gint mode) {
  // return value should be g_free'd after use
  weed_plant_t *filter,*plugin_info;
  gchar *name;
  int error;
  gchar *retval;

  key--;
  if (!rte_keymode_valid(key+1,mode,TRUE)) return g_strdup("");

  filter=weed_filters[key_to_fx[key][mode]];
  plugin_info=weed_get_plantptr_value(filter,"plugin_info",&error);
  name=weed_get_string_value(plugin_info,"name",&error);
  retval=g_strdup(name);
  weed_free(name);
  return retval;
}




G_GNUC_PURE int rte_getmodespk (void) {
  return MAX_MODES_PER_KEY;
}

G_GNUC_PURE gint rte_bg_gen_key (void) {
  return bg_generator_key;
}

G_GNUC_PURE gint rte_fg_gen_key (void) {
  return fg_generator_key;
}

G_GNUC_PURE gint rte_bg_gen_mode (void) {
  return bg_generator_mode;
}

G_GNUC_PURE gint rte_fg_gen_mode (void) {
  return fg_generator_mode;
}



weed_plant_t *get_textparm() {
  // for rte textmode, get first string parameter for current key/mode instance
  // we will then forward all keystrokes to this parm "value" until the exit key (TAB)
  // is pressed

  weed_plant_t *inst,**in_params,*ptmpl,*ret;

  int key=mainw->rte_keys,mode,error,i,idx,hint;

  if (key==-1) return NULL;

  mode=rte_key_getmode(key+1);

  if ((idx=key_to_instance[key][mode])!=-1&&(inst=weed_instances[idx])!=NULL){
    int nparms;

    if (!weed_plant_has_leaf(inst,"in_parameters")||(nparms=weed_leaf_num_elements(inst,"in_parameters"))==0) return NULL;

    in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
    
    for (i=0;i<nparms;i++) {
      ptmpl=weed_get_plantptr_value(in_params[0],"template",&error);

      hint=weed_get_int_value(ptmpl,"hint",&error);

      if (hint==WEED_HINT_TEXT) {
	ret=in_params[i];
	weed_free(in_params);
	return ret;
      }

    }

    weed_free(in_params);

  }

  return NULL;

}



gboolean rte_key_setmode (gint key, gint newmode) {
  // newmode has two special values, -1 = cycle forwards, -2 = cycle backwards

  gint oldmode;
  gint blend_file;
  gshort whentostop=mainw->whentostop;
  gboolean was_started=FALSE;
  gint idx,real_key;

  if (key==0) {
    if ((key=mainw->rte_keys)==-1) return FALSE;
  }
  else key--;

  real_key=key;

  oldmode=key_modes[key];

  if (key_to_fx[key][0]==-1) return FALSE; // nothing is mapped to effect key

  if (newmode==-1) {
    // cycle forwards
    if (oldmode==MAX_MODES_PER_KEY-1||key_to_fx[key][oldmode+1]==-1) {
      newmode=0;
    }
    else {
      newmode=key_modes[key]+1;
    }
  }

  if (newmode==-2) {
    // cycle backwards
    newmode=key_modes[key]-1;
    if (newmode<0) {
      for (newmode=MAX_MODES_PER_KEY-1;newmode>=0;newmode--) {
	if (key_to_fx[key][newmode]!=-1) break;
      }
    }
  }

  if (newmode<0||newmode>=MAX_MODES_PER_KEY) return FALSE;

  if (key_to_fx[key][newmode]==-1) return FALSE;

  if (rte_window!=NULL) rtew_set_mode_radio(key,newmode);

  mainw->osc_block=TRUE;

  // TODO - block template channel changes

  if ((idx=key_to_instance[key][oldmode])!=-1&&weed_instances[idx]!=NULL){
    was_started=TRUE;
    if (enabled_in_channels(weed_instances[idx],FALSE)==2&&enabled_in_channels(weed_filters[key_to_fx[key][newmode]],FALSE)==2) {
      // transition --> transition, allow any bg generators to survive
      key=-key-1;
    }
  }

  if (oldmode!=newmode) {
    blend_file=mainw->blend_file;

    if (was_started&&enabled_in_channels(weed_instances[idx],FALSE)>0) {
      // not a generator
      weed_deinit_effect (key);
    }
    else if (enabled_in_channels(weed_filters[key_to_fx[key][newmode]],FALSE)==0) mainw->whentostop=NEVER_STOP; // when gen->gen, dont stop pb
    
    key_modes[real_key]=newmode;

    mainw->blend_file=blend_file;

    if (was_started) {
      if (!weed_init_effect(key)) {
	// TODO - unblock template channel changes
	mainw->whentostop=whentostop;
	key=real_key;
	if (mainw->rte&(GU641<<key)) mainw->rte^=(GU641<<key);
	mainw->osc_block=FALSE;
	return FALSE;
       }
    }
    // TODO - unblock template channel changes
    mainw->whentostop=whentostop;
  }

  mainw->osc_block=FALSE;
  return TRUE;
}


int weed_add_effectkey_by_idx (gint key, int idx) {
  // we will add a filter_class at the next free slot for key, and return the slot number
  // if idx is -1 (probably meaning the filter was not found), we return -1
  // if all slots are full, we return -3
  // currently, generators and non-generators cannot be mixed on the same key (causes problems if the mode is switched)
  // in this case a -2 is returned
  gboolean has_gen=FALSE;
  gboolean has_non_gen=FALSE;

  int i;

  if (idx==-1) return -1;

  key--;

  for (i=0;i<MAX_MODES_PER_KEY;i++) {
    if (key_to_fx[key][i]!=-1) {
      if (enabled_in_channels(weed_filters[key_to_fx[key][i]],FALSE)==0) has_gen=TRUE;
      else has_non_gen=TRUE;
    }
    else {
      if ((enabled_in_channels(weed_filters[idx],FALSE)==0&&has_non_gen)||(enabled_in_channels(weed_filters[idx],FALSE)>0&&has_gen)) return -2;
      key_to_fx[key][i]=idx;
      return i;
    }
  }
  return -3;
}


int weed_add_effectkey (gint key, const gchar *hashname, gboolean fullname) {
  // add a filter_class by hashname to an effect_key
  int idx=weed_get_idx_for_hashname(hashname,fullname);
  return weed_add_effectkey_by_idx (key,idx);
}



gint rte_switch_keymode (gint key, gint mode, const gchar *hashname) {
  // this is called when we switch the filter_class bound to an effect_key/mode

  int oldkeymode=key_modes[--key];
  int id=weed_get_idx_for_hashname (hashname,TRUE);
  gboolean osc_block;
  gboolean has_gen=FALSE,has_non_gen=FALSE;

  gint test=(mode==0?1:0);

  // effect not found
  if (id==-1) return -1;

  if (key_to_fx[key][test]!=-1) {
    if (enabled_in_channels(weed_filters[key_to_fx[key][test]],FALSE)==0) has_gen=TRUE;
    else has_non_gen=TRUE;
  }

  if ((enabled_in_channels(weed_filters[id],FALSE)==0&&has_non_gen)||(enabled_in_channels(weed_filters[id],FALSE)>0&&has_gen)) return -2;

  osc_block=mainw->osc_block;
  mainw->osc_block=TRUE;
  if (key_to_instance[key][mode]!=-1) {
    key_modes[key]=mode;
    weed_deinit_effect(-key-1); // set is_modeswitch
    key_to_fx[key][mode]=id;
    weed_init_effect(-key-1);
    key_modes[key]=oldkeymode;
  }
  else key_to_fx[key][mode]=id;

  mainw->osc_block=osc_block;
  return 0;
}



void rte_swap_fg_bg (void) {
  gint key=fg_generator_key;
  gint mode=fg_generator_mode;

  if (key!=-1) {
    fg_generator_clip=-1;
  }
  fg_generator_key=bg_generator_key;
  fg_generator_mode=bg_generator_mode;
  if (fg_generator_key!=-1) {
    fg_generator_clip=mainw->current_file;
  }
  bg_generator_key=key;
  bg_generator_mode=mode;
}



GList *weed_get_all_names (gshort list_type) {
  // remember to free list after use, if non-NULL
  GList *list=NULL;
  int i,error;
  gchar *filter_name,*filter_type,*hashname,*string;

  for (i=0;i<num_weed_filters;i++) {
    filter_name=weed_get_string_value(weed_filters[i],"name",&error);
    switch (list_type) {
    case 1:
      // just name
      string=g_strdup(filter_name);
      list=g_list_append(list,(gpointer)string);
      break;
    case 2:
      // name and type
      filter_type=weed_filter_get_type(i);
      string=g_strdup_printf("%s (%s)",filter_name,filter_type);
      list=g_list_append(list,(gpointer)string);
      g_free(filter_type);
      break;
    case 3:
      // hashnames
      hashname=make_weed_hashname(i);
      list=g_list_append(list,(gpointer)hashname);
      break;
    }
    weed_free(filter_name);
  }
  return list;
}


gint rte_get_numfilters(void) {
  return num_weed_filters;
}


///////////////////
// parameter interpolation

void fill_param_vals_to (weed_plant_t *paramtmpl, weed_plant_t *param, int pnum, int hint, int index) {
  // for a multi valued parameter or pchange, we will fill "value" up to element index with "new_default"

  int i,error;
  int num_vals=weed_leaf_num_elements(param,"value");
  int new_defi,*valis,*nvalis;
  double new_defd,*valds,*nvalds;
  gchar *new_defs,**valss,**nvalss;
  int cspace;
  int *colsis,*coli;
  double *colsds,*cold;


  switch (hint) {
  case WEED_HINT_INTEGER:
    new_defi=weed_get_int_value(paramtmpl,"new_default",&error);
    valis=weed_get_int_array(param,"value",&error);
    nvalis=g_malloc((index+1)*sizint);
    for (i=0;i<=index;i++) {
      if (i<num_vals) nvalis[i]=valis[i];
      else nvalis[i]=new_defi;
    }
    weed_set_int_array(param,"value",index+1,nvalis);
    weed_free(valis);
    g_free(nvalis);
    break;
  case WEED_HINT_FLOAT:
    new_defd=weed_get_double_value(paramtmpl,"new_default",&error);
    valds=weed_get_double_array(param,"value",&error);
    nvalds=g_malloc((index+1)*sizdbl);
    for (i=0;i<=index;i++) {
      if (i<num_vals) nvalds[i]=valds[i];
      else nvalds[i]=new_defd;
    }
    weed_set_double_array(param,"value",index+1,nvalds);

    weed_free(valds);
    g_free(nvalds);
    break;
  case WEED_HINT_SWITCH:
    new_defi=weed_get_boolean_value(paramtmpl,"new_default",&error);
    valis=weed_get_boolean_array(param,"value",&error);
    nvalis=g_malloc((index+1)*sizint);
    for (i=0;i<=index;i++) {
      if (i<num_vals) nvalis[i]=valis[i];
      else nvalis[i]=new_defi;
    }
    weed_set_boolean_array(param,"value",index+1,nvalis);
    weed_free(valis);
    g_free(nvalis);
    break;
  case WEED_HINT_TEXT:
    new_defs=weed_get_string_value(paramtmpl,"new_default",&error);
    valss=weed_get_string_array(param,"value",&error);
    nvalss=g_malloc((index+1)*sizeof(gchar *));
    for (i=0;i<=index;i++) {
      if (i<num_vals) {
	nvalss[i]=g_strdup(valss[i]);
	weed_free(valss[i]);
      }
      else nvalss[i]=g_strdup(new_defs);
    }
    weed_set_string_array(param,"value",index+1,nvalss);

    for (i=0;i<index;i++) {
      g_free(nvalss[i]);
    }

    weed_free(new_defs);
    weed_free(valss);
    g_free(nvalss);
    break;
  case WEED_HINT_COLOR:
    cspace=weed_get_int_value(paramtmpl,"colorspace",&error);
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      index*=3;
      if (weed_leaf_seed_type(paramtmpl,"new_default")==WEED_SEED_INT) {
	colsis=weed_get_int_array(param,"value",&error);
	if (weed_leaf_num_elements(paramtmpl,"new_default")==1) {
	  coli=weed_malloc(3*sizint);
	  coli[0]=coli[1]=coli[2]=weed_get_int_value(paramtmpl,"new_default",&error);
	}
	else coli=weed_get_int_array(paramtmpl,"new_default",&error);
	valis=weed_get_int_array(param,"value",&error);
	nvalis=g_malloc((index+3)*sizint);
	for (i=0;i<=index;i+=3) {
	  if (i<num_vals) {
	    nvalis[i]=valis[i];
	    nvalis[i+1]=valis[i+1];
	    nvalis[i+2]=valis[i+2];
	  }
	  else {
	    nvalis[i]=coli[0];
	    nvalis[i+1]=coli[1];
	    nvalis[i+2]=coli[2];
	  }
	}
	weed_set_int_array(param,"value",index+3,nvalis);
	weed_free(valis);
	weed_free(colsis);
	g_free(nvalis);
      }
      else {
	colsds=weed_get_double_array(param,"value",&error);
	if (weed_leaf_num_elements(paramtmpl,"new_default")==1) {
	  cold=weed_malloc(3*sizdbl);
	  cold[0]=cold[1]=cold[2]=weed_get_double_value(paramtmpl,"new_default",&error);
	}
	else cold=weed_get_double_array(paramtmpl,"new_default",&error);
	valds=weed_get_double_array(param,"value",&error);
	nvalds=g_malloc((index+3)*sizdbl);
	for (i=0;i<=index;i+=3) {
	  if (i<num_vals) {
	    nvalds[i]=valds[i];
	    nvalds[i+1]=valds[i+1];
	    nvalds[i+2]=valds[i+2];
	  }
	  else {
	    nvalds[i]=cold[0];
	    nvalds[i+1]=cold[1];
	    nvalds[i+2]=cold[2];
	  }
	}
	weed_set_double_array(param,"value",index+3,nvalds);
	weed_free(valds);
	weed_free(colsds);
	g_free(nvalds);
      }
    }
    break;
  }
}




static weed_plant_t **void_ptrs_to_plant_array(weed_plant_t *tmpl, void *pchain, int num) {
  // return value should be free'd after use
  weed_plant_t **param_array;
  weed_plant_t *pchange;
  int i=0,error;

  if (num==-1) {
    // count pchain entries
    num=0;
    pchange=pchain;
    while (pchange!=NULL) {
      pchange=weed_get_voidptr_value(pchange,"next_change",&error);
      num++;
    }
  }

  param_array=g_malloc((num+1)*sizeof(weed_plant_t *));
  pchange=pchain;
  while (pchange!=NULL) {
    param_array[i]=weed_plant_new(WEED_PLANT_PARAMETER);
    weed_set_plantptr_value(param_array[i],"template",tmpl);
    weed_leaf_copy(param_array[i],"timecode",pchange,"timecode");
    weed_leaf_copy(param_array[i],"value",pchange,"value");
    weed_add_plant_flags(param_array[i],WEED_LEAF_READONLY_PLUGIN);
    pchange=weed_get_voidptr_value(pchange,"next_change",&error);
    i++;
  }
  param_array[i]=NULL;
  return param_array;
}


static int get_default_element_int (weed_plant_t *param, int idx, int mpy, int add) {
  int *valsi,val;
  int error;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"host_default")&&weed_leaf_num_elements(ptmpl,"host_default")>idx*mpy+add) {
    valsi=weed_get_int_array(ptmpl,"host_default",&error);
    val=valsi[idx*mpy+add];
    weed_free(valsi);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl,"default")&&weed_leaf_num_elements(ptmpl,"default")>idx*mpy+add) {
    valsi=weed_get_int_array(ptmpl,"default",&error);
    val=valsi[idx*mpy+add];
    weed_free(valsi);
    return val;
  }
  if (weed_leaf_num_elements(ptmpl,"new_default")==mpy) {
    valsi=weed_get_int_array(ptmpl,"default",&error);
    val=valsi[add];
    weed_free(valsi);
    return val;
  }
  return weed_get_int_value(ptmpl,"new_default",&error);
}


static double get_default_element_double (weed_plant_t *param, int idx, int mpy, int add) {
  double *valsd,val;
  int error;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"host_default")&&weed_leaf_num_elements(ptmpl,"host_default")>idx*mpy+add) {
    valsd=weed_get_double_array(ptmpl,"host_default",&error);
    val=valsd[idx*mpy+add];
    weed_free(valsd);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl,"default")&&weed_leaf_num_elements(ptmpl,"default")>idx*mpy+add) {
    valsd=weed_get_double_array(ptmpl,"default",&error);
    val=valsd[idx*mpy+add];
    weed_free(valsd);
    return val;
  }
  if (weed_leaf_num_elements(ptmpl,"new_default")==mpy) {
    valsd=weed_get_double_array(ptmpl,"default",&error);
    val=valsd[add];
    weed_free(valsd);
    return val;
  }
   return weed_get_double_value(ptmpl,"new_default",&error);
}


static int get_default_element_bool (weed_plant_t *param, int idx) {
  int *valsi,val;
  int error;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"host_default")&&weed_leaf_num_elements(ptmpl,"host_default")>idx) {
    valsi=weed_get_boolean_array(ptmpl,"host_default",&error);
    val=valsi[idx];
    weed_free(valsi);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl,"default")&&weed_leaf_num_elements(ptmpl,"default")>idx) {
    valsi=weed_get_boolean_array(ptmpl,"default",&error);
    val=valsi[idx];
    weed_free(valsi);
    return val;
  }
  return weed_get_boolean_value(ptmpl,"new_default",&error);
}


static gchar *get_default_element_string (weed_plant_t *param, int idx) {
  gchar **valss,*val,*val2;
  int error,i;
  int numvals;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"host_default")&&(numvals=weed_leaf_num_elements(ptmpl,"host_default"))>idx) {
    valss=weed_get_string_array(ptmpl,"host_default",&error);
    val=g_strdup(valss[idx]);
    for (i=0;i<numvals;i++) weed_free(valss[i]);
    weed_free(valss);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl,"default")&&(numvals=weed_leaf_num_elements(ptmpl,"default"))>idx) {
    valss=weed_get_string_array(ptmpl,"host_default",&error);
    val=g_strdup(valss[idx]);
    for (i=0;i<numvals;i++) weed_free(valss[i]);
    weed_free(valss);
    return val;
  }
  val=weed_get_string_value(ptmpl,"new_default",&error);
  val2=g_strdup(val);
  weed_free(val);
  return val2;
}






gboolean interpolate_param(weed_plant_t *inst, int i, void *pchain, weed_timecode_t tc) {
  // return FALSE if param has no "value" - this can happen during realtime audio processing, if the effect is inited, but no "value" has been set yet
  weed_plant_t **param_array;
  int error,j;
  weed_plant_t *pchange=pchain,*last_pchange=NULL;
  weed_plant_t *wtmpl;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_timecode_t tc_diff=0,tc_diff2;
  int hint,cspace=0;
  weed_plant_t *gui=NULL;
  weed_plant_t *param=in_params[i];
  double *last_valuesd,*next_valuesd;
  int *last_valuesi,*next_valuesi;
  void **lpc,**npc;
  int *ign;
  int got_npc;
  double *valds=NULL,*nvalds,last_valued;
  int *valis=NULL,*nvalis,last_valuei;
  int num_values,xnum,num_pvals;
  int k;
  int last_valueir,last_valueig,last_valueib;
  double last_valuedr,last_valuedg,last_valuedb;
  gchar **valss,**nvalss;
  int num_ign=0;

  if (pchange==NULL) {
    weed_free(in_params);
    return TRUE;
  }

  if (!weed_plant_has_leaf(param,"value")||weed_leaf_num_elements(param,"value")==0) {
    weed_free(in_params);
    return FALSE;  // do not apply effect
  }

  while (pchange!=NULL&&get_event_timecode(pchange)<=tc) {
    last_pchange=pchange;
    pchange=weed_get_voidptr_value(pchange,"next_change",&error);
  }

  // we need to single thread here, because it's possible to have a conflict - if the audio and video threads are
  // both doing simultaneous interpolation of the same parameter
  pthread_mutex_lock(&mainw->interp_mutex);

  // if plugin wants to do its own interpolation, we let it
  wtmpl=weed_get_plantptr_value(param,"template",&error);
  if (weed_plant_has_leaf(wtmpl,"interpolate_func")) {
    gboolean needs_more;
    gboolean more_available;
    weed_interpolate_f interpolate_func;
    weed_plant_t *calc_param=weed_plant_new(WEED_PLANT_PARAMETER);

    // setup our calc_param (return result)
    weed_set_plantptr_value(calc_param,"template",wtmpl);
    weed_set_int64_value(calc_param,"timecode",tc);

    // try first with just the two surrounding values
    if (pchange==last_pchange&&pchange==NULL) {
      param_array=void_ptrs_to_plant_array(wtmpl,pchange,0);
      more_available=FALSE;
    }
    else if (last_pchange==NULL) {
      param_array=void_ptrs_to_plant_array(wtmpl,pchange,1);
      more_available=FALSE;
    }
    else if (pchange==NULL) {
      param_array=void_ptrs_to_plant_array(wtmpl,last_pchange,1);
      more_available=FALSE;
    }
    else {
      param_array=void_ptrs_to_plant_array(wtmpl,last_pchange,2);
      more_available=TRUE; // maybe...
    }
    
    weed_add_plant_flags(calc_param,WEED_LEAF_READONLY_PLUGIN);
    interpolate_func=weed_get_voidptr_value(wtmpl,"interpolate_func",&error);
    needs_more=(*interpolate_func)(param_array,calc_param);
    
    if (needs_more==WEED_FALSE||!more_available) {
      // got an accurate result from 2 points
      weed_leaf_copy(param,"value",calc_param,"value");
      weed_plant_free(calc_param);
      weed_free(in_params);
      for(i=0;param_array[i]!=NULL;i++) weed_plant_free(param_array[i]);
      g_free(param_array);
      return TRUE;
    }
    // try to pass more values
    g_free(param_array);
    param_array=void_ptrs_to_plant_array(wtmpl,pchain,-1);
    
    (*interpolate_func)(param_array,calc_param);
    
    weed_leaf_copy(param,"value",calc_param,"value");
    weed_plant_free(calc_param);
    weed_free(in_params);
    for(i=0;param_array[i]!=NULL;i++) weed_plant_free(param_array[i]);
    g_free(param_array);
    return TRUE;
  }

  num_values=weed_leaf_num_elements(param,"value");

  if ((num_pvals=weed_leaf_num_elements(pchain,"value"))>num_values) num_values=num_pvals; // init a multivalued param

  lpc=(void **)g_malloc(num_values*sizeof(void *));
  npc=(void **)g_malloc(num_values*sizeof(void *));

  if (num_values==1) {
    lpc[0]=last_pchange;
    npc[0]=pchange;
  }
  else {
    pchange=pchain;
    
    for (j=0;j<num_values;j++) npc[j]=lpc[j]=NULL;

    while (pchange!=NULL) {
      num_pvals=weed_leaf_num_elements(pchange,"value");
      if (num_pvals>num_values) num_pvals=num_values;
      if (weed_plant_has_leaf(pchange,"ignore")) {
	num_ign=weed_leaf_num_elements(pchange,"ignore");
	ign=weed_get_boolean_array(pchange,"ignore",&error);
      }
      else ign=NULL;
      if (get_event_timecode(pchange)<=tc) {
	for (j=0;j<num_pvals;j++) if (ign==NULL||j>=num_ign||ign[j]==WEED_FALSE) lpc[j]=pchange;
      }
      else {
	for (j=0;j<num_pvals;j++) {
	  if (npc[j]==NULL&&(ign==NULL||j>=num_ign||ign[j]==WEED_FALSE)) npc[j]=pchange;
	}
	got_npc=0;
	for (j=0;j<num_values;j++) {
	  if (npc[j]!=NULL) got_npc++;
	}
	if (got_npc==num_values) {
	  if (ign!=NULL) weed_free(ign);
	  break;
	}
      }
      pchange=weed_get_voidptr_value(pchange,"next_change",&error);
      if (ign!=NULL) weed_free(ign);
    }
  }


  hint=weed_get_int_value(wtmpl,"hint",&error);
  switch (hint) {
  case WEED_HINT_FLOAT:
    valds=g_malloc(num_values*(sizeof(double)));
    break;
  case WEED_HINT_COLOR:
    cspace=weed_get_int_value(wtmpl,"colorspace",&error);
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (num_values%3!=0) return TRUE;
      if (weed_leaf_seed_type(wtmpl,"default")==WEED_SEED_INT) {
	valis=g_malloc(num_values*sizint);
      }
      else {
	valds=g_malloc(num_values*(sizeof(double)));
      }
      break;
    }
    break;
  case WEED_HINT_SWITCH:
  case WEED_HINT_INTEGER:
    valis=g_malloc(num_values*sizint);
    break;
  }



  for (j=0;j<num_values;j++) {
    // must interpolate - we use linear interpolation
    if (lpc[j]==NULL&&npc[j]==NULL) continue;
    if (lpc[j]!=NULL&&npc[j]!=NULL) tc_diff=weed_get_int64_value(npc[j],"timecode",&error)-weed_get_int64_value(lpc[j],"timecode",&error);
    switch (hint) {
    case WEED_HINT_FLOAT:
      if (lpc[j]==NULL) {
      // before first change
	valds[j]=get_default_element_double(param,j,1,0);
	continue;
      }
      if (npc[j]==NULL) {
	// after last change
	xnum=weed_leaf_num_elements(lpc[j],"value");
	if (xnum>j) {
	  nvalds=weed_get_double_array(lpc[j],"value",&error);
	  valds[j]=nvalds[j];
	  weed_free(nvalds);
	}
	else valds[j]=get_default_element_double(param,j,1,0);
	continue;
      }
          
      next_valuesd=weed_get_double_array(npc[j],"value",&error);
      last_valuesd=weed_get_double_array(lpc[j],"value",&error);
      xnum=weed_leaf_num_elements(lpc[j],"value");
      if (xnum>j) last_valued=last_valuesd[j];
      else last_valued=get_default_element_double(param,j,1,0);
      
      valds[j]=last_valued+(gdouble)(next_valuesd[j]-last_valued)/(gdouble)(tc_diff/U_SEC)*(gdouble)((tc-weed_get_int64_value(lpc[j],"timecode",&error))/U_SEC);

      weed_free(last_valuesd);
      weed_free(next_valuesd);
      break;
    case WEED_HINT_COLOR:
      if (num_values!=weed_leaf_num_elements(last_pchange,"value")) break; // no interp possible
      
      switch (cspace) {
      case WEED_COLORSPACE_RGB:
	k=j*3;
	if (weed_leaf_seed_type(wtmpl,"default")==WEED_SEED_INT) {
	  if (lpc[j]==NULL) {
	    // before first change
	    valis[k]=get_default_element_int(param,j,3,0);
	    valis[k+1]=get_default_element_int(param,j,3,1);
	    valis[k+2]=get_default_element_int(param,j,3,2);
	    j+=3;
	    continue;
	  }
	  if (npc[j]==NULL) {
	    // after last change
	    xnum=weed_leaf_num_elements(lpc[j],"value");
	    if (xnum>k) {
	      nvalis=weed_get_int_array(lpc[j],"value",&error);
	      valis[k]=nvalis[k];
	      valis[k+1]=nvalis[k+1];
	      valis[k+2]=nvalis[k+2];
	      weed_free(nvalis);
	    }
	    else {
	      valis[k]=get_default_element_int(param,j,3,0);
	      valis[k+1]=get_default_element_int(param,j,3,1);
	      valis[k+2]=get_default_element_int(param,j,3,2);
	    }
	    j+=3;
	    continue;
	  }
	  
	  next_valuesi=weed_get_int_array(npc[j],"value",&error);
	  last_valuesi=weed_get_int_array(lpc[j],"value",&error);
	  xnum=weed_leaf_num_elements(lpc[j],"value");
	  if (xnum>k) {
	    last_valueir=last_valuesi[k];
	    last_valueig=last_valuesi[k+1];
	    last_valueib=last_valuesi[k+2];
	  }
	  else {
	    last_valueir=get_default_element_int(param,j,3,0);
	    last_valueig=get_default_element_int(param,j,3,1);
	    last_valueib=get_default_element_int(param,j,3,2);
	  }

	  if (next_valuesi==NULL) continue; // can happen if we recorded a param change

	  valis[k]=last_valueir+(next_valuesi[k]-last_valueir)/(tc_diff/U_SEC)*((tc_diff2=(tc-weed_get_int64_value(lpc[j],"timecode",&error)))/U_SEC)+.5;
	  valis[k+1]=last_valueig+(next_valuesi[k+1]-last_valueig)/(tc_diff/U_SEC)*(tc_diff2/U_SEC)+.5;
	  valis[k+2]=last_valueib+(next_valuesi[k+2]-last_valueib)/(tc_diff/U_SEC)*(tc_diff2/U_SEC)+.5;
	  
	  weed_free(last_valuesi);
	  weed_free(next_valuesi);
	}
	else {
	  if (lpc[j]==NULL) {
	    // before first change
	    valds[k]=get_default_element_double(param,j,3,0);
	    valds[k+1]=get_default_element_double(param,j,3,1);
	    valds[k+2]=get_default_element_double(param,j,3,2);
	    j+=3;
	    continue;
	  }
	  if (npc[j]==NULL) {
	    // after last change
	    xnum=weed_leaf_num_elements(lpc[j],"value");
	    if (xnum>k) {
	      nvalds=weed_get_double_array(lpc[j],"value",&error);
	      valds[k]=nvalds[k];
	      valds[k+1]=nvalds[k+1];
	      valds[k+2]=nvalds[k+2];
	      weed_free(nvalds);
	    }
	    else {
	      valds[k]=get_default_element_double(param,j,3,0);
	      valds[k+1]=get_default_element_double(param,j,3,1);
	      valds[k+2]=get_default_element_double(param,j,3,2);
	    }
	    j+=3;
	    continue;
	  }
	  
	  next_valuesd=weed_get_double_array(npc[j],"value",&error);
	  last_valuesd=weed_get_double_array(lpc[j],"value",&error);
	  xnum=weed_leaf_num_elements(lpc[j],"value");
	  if (xnum>k) {
	    last_valuedr=last_valuesd[k];
	    last_valuedg=last_valuesd[k+1];
	    last_valuedb=last_valuesd[k+2];
	  }
	  else {
	    last_valuedr=get_default_element_double(param,j,3,0);
	    last_valuedg=get_default_element_double(param,j,3,1);
	    last_valuedb=get_default_element_double(param,j,3,2);
	  }
	  valds[k]=last_valuedr+(next_valuesd[k]-last_valuedr)/(tc_diff/U_SEC)*((tc_diff2=(tc-weed_get_int64_value(lpc[j],"timecode",&error)))/U_SEC);
	  valds[k+1]=last_valuedg+(next_valuesd[k+1]-last_valuedg)/(tc_diff/U_SEC)*(tc_diff2/U_SEC)+.5;
	  valds[k+2]=last_valuedb+(next_valuesd[k+2]-last_valuedb)/(tc_diff/U_SEC)*(tc_diff2/U_SEC)+.5;

	  weed_free(last_valuesd);
	  weed_free(next_valuesd);
	}
	j+=3;
	break;
	// TODO - other colorspaces (e.g. RGBA32)
      } // cspace
      break; // color
    case WEED_HINT_INTEGER:
      // get gui
      if (weed_plant_has_leaf(wtmpl,"gui")) gui=weed_get_plantptr_value(wtmpl,"gui",&error);
      if (gui!=NULL&&weed_plant_has_leaf(gui,"choices")) {
	// no interpolation
	if (npc[j]!=NULL&&get_event_timecode(npc[j])==tc) {
	  nvalis=weed_get_int_array(npc[j],"value",&error);
	  valis[j]=nvalis[j];
	  weed_free(nvalis);
	  continue;
	}
	else {
	  // use last_pchange value
	  xnum=weed_leaf_num_elements(lpc[j],"value");
	  if (xnum>j) {
	    nvalis=weed_get_int_array(lpc[j],"value",&error);
	    valis[j]=nvalis[j];
	    weed_free(nvalis);
	  }
	  else valis[j]=get_default_element_int(param,j,1,0);
	  continue;
	}
      }
      else {
	if (lpc[j]==NULL) {
	  // before first change
	  valis[j]=get_default_element_int(param,j,1,0);
	  continue;
	}
	if (npc[j]==NULL) {
	  // after last change
	  xnum=weed_leaf_num_elements(lpc[j],"value");
	  if (xnum>j) {
	    nvalis=weed_get_int_array(lpc[j],"value",&error);
	    valis[j]=nvalis[j];
	    weed_free(nvalis);
	  }
	  else valis[j]=get_default_element_int(param,j,1,0);
	  continue;
	}
	
	next_valuesi=weed_get_int_array(npc[j],"value",&error);
	last_valuesi=weed_get_int_array(lpc[j],"value",&error);
	xnum=weed_leaf_num_elements(lpc[j],"value");
	if (xnum>j) last_valuei=last_valuesi[j];
	else last_valuei=get_default_element_int(param,j,1,0);
	
	valis[j]=last_valuei+(next_valuesi[j]-last_valuei)/(tc_diff/U_SEC)*((tc-weed_get_int64_value(lpc[j],"timecode",&error))/U_SEC)+.5;

	weed_free(last_valuesi);
	weed_free(next_valuesi);
	break;
      }
    case WEED_HINT_SWITCH:
      // no interpolation
      if (npc[j]!=NULL&&get_event_timecode(npc[j])==tc) {
	nvalis=weed_get_boolean_array(npc[j],"value",&error);
	valis[j]=nvalis[j];
	weed_free(nvalis);
	continue;
      }
      else {
	// use last_pchange value
	xnum=weed_leaf_num_elements(lpc[j],"value");
	if (xnum>j) {
	  nvalis=weed_get_boolean_array(lpc[j],"value",&error);
	  valis[j]=nvalis[j];
	  weed_free(nvalis);
	}
	else valis[j]=get_default_element_bool(param,j);
	continue;
      }
      break;
    case WEED_HINT_TEXT:
      // no interpolation
      valss=weed_get_string_array(param,"value",&error);

      if (npc[j]!=NULL&&get_event_timecode(npc[j])==tc) {
	nvalss=weed_get_string_array(npc[j],"value",&error);
	valss[j]=g_strdup(nvalss[j]);
	for (k=0;k<num_values;k++) weed_free(nvalss[k]);
	weed_free(nvalss);
	weed_set_string_array(param,"value",num_values,valss);
	for (k=0;k<num_values;k++) weed_free(valss[k]);
	weed_free(valss);
	continue;
      }
      else {
	// use last_pchange value
	xnum=weed_leaf_num_elements(lpc[j],"value");
	if (xnum>j) {
	  nvalss=weed_get_string_array(lpc[j],"value",&error);
	  valss[j]=g_strdup(nvalss[j]);
	  for (k=0;k<num_values;k++) weed_free(nvalss[k]);
	  weed_free(nvalss);
	}
	else valss[j]=get_default_element_string(param,j);
	weed_set_string_array(param,"value",num_values,valss);
	for (k=0;k<num_values;k++) weed_free(valss[k]);
	weed_free(valss);
	continue;
      }
      break;
    } // parameter hint
  } // j


  switch (hint) {
  case WEED_HINT_FLOAT:
    weed_set_double_array(param,"value",num_values,valds);
    g_free(valds);
    break;
  case WEED_HINT_COLOR:
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (weed_leaf_seed_type(wtmpl,"default")==WEED_SEED_INT) {
	weed_set_int_array(param,"value",num_values,valis);
	g_free(valis);
      }
      else {
	weed_set_double_array(param,"value",num_values,valds);
	g_free(valds);
      }
      break;
    }
    break;
  case WEED_HINT_INTEGER:
    weed_set_int_array(param,"value",num_values,valis);
    g_free(valis);
    break;
  case WEED_HINT_SWITCH:
    weed_set_boolean_array(param,"value",num_values,valis);
    g_free(valis);
    break;
  }

  pthread_mutex_unlock(&mainw->interp_mutex);

  g_free(npc);
  g_free(lpc);
  weed_free(in_params);
  return TRUE;
}


gboolean interpolate_params(weed_plant_t *inst, void **pchains, weed_timecode_t tc) {
  // interpolate all in_parameters for filter_instance inst, using void **pchain, which is an array of param_change events in temporal order
  // values are calculated for timecode tc. We skip "hidden" parameters
  int i;
  void *pchain;
  int num_params;

  if (!weed_plant_has_leaf(inst,"in_parameters")||pchains==NULL) return TRUE;

  num_params=weed_leaf_num_elements(inst,"in_parameters");

  if (num_params==0) return TRUE; // no in_parameters ==> do nothing

  for (i=0;i<num_params;i++) {
    if (!is_hidden_param(inst,i)) {
      pchain=pchains[i];
      if (!interpolate_param(inst,i,pchain,tc)) return FALSE; // FALSE if param is not ready
    }
  }
  return TRUE;
}




///////////////////////////////////////////////////////////
////// hashnames

gchar *make_weed_hashname(int filter_idx) {
  weed_plant_t *filter,*plugin_info;
  gchar *plugin_name,*filter_name,*filter_author,*filter_version,*hashname;
  int error,version;
  gchar plugin_fname[256];

  if (filter_idx<0||filter_idx>=num_weed_filters) return g_strdup("");

  if (hashnames[filter_idx]!=NULL) return g_strdup(hashnames[filter_idx]);

  filter=weed_filters[filter_idx];

  plugin_info=weed_get_plantptr_value(filter,"plugin_info",&error);

  plugin_name=weed_get_string_value(plugin_info,"name",&error);

  g_snprintf(plugin_fname,256,"%s",plugin_name);
  weed_free(plugin_name);
  get_filename(plugin_fname);

  filter_name=weed_get_string_value(filter,"name",&error);
  filter_author=weed_get_string_value(filter,"author",&error);
  version=weed_get_int_value(filter,"version",&error);
  filter_version=g_strdup_printf("%d",version);

  hashname=g_strconcat(plugin_fname,filter_name,filter_author,filter_version,NULL);

  weed_free(filter_name);
  weed_free(filter_author);
  g_free(filter_version);

  return hashname;
}



int weed_get_idx_for_hashname (const gchar *hashname, gboolean fullname) {
  int i;
  gchar *chashname;

  for (i=0;i<num_weed_filters;i++) {
    chashname=make_weed_hashname(i);
    if ((fullname&&!strcmp(hashname,chashname))||(!fullname&&!strncmp(hashname,chashname,strlen(hashname)))) {
      g_free(chashname);
      return i;
    }
    g_free(chashname);
  }
  return -1;
}

weed_plant_t *get_weed_filter(int idx) {
  if (idx>-1&&idx<num_weed_filters) return weed_filters[idx];
  return NULL;
}



static void weed_leaf_serialise (int fd, weed_plant_t *plant, char *key, gboolean write_all, unsigned char **mem) {
  void *value;
  guint32 vlen;
  int st,ne;
  int j;
  guint32 i=(guint32)strlen(key);

  if (write_all) {
    if (mem==NULL) {
      dummyvar=write(fd,&i,sizint);
      dummyvar=write(fd,key,(size_t)i);
    }
    else {
      w_memcpy(*mem,&i,sizint);
      *mem+=sizint;
      w_memcpy(*mem,key,(size_t)i);
      *mem+=i;
    }
  }
  st=weed_leaf_seed_type(plant,key);
  if (mem==NULL) dummyvar=write(fd,&st,sizint);
  else {
    w_memcpy(*mem,&st,sizint);
    *mem+=sizint;
  }
  ne=weed_leaf_num_elements(plant,key);
  if (mem==NULL) dummyvar=write(fd,&ne,sizint);
  else {
    w_memcpy(*mem,&ne,sizint);
    *mem+=sizint;
  }
  for (j=0;j<ne;j++) {
    vlen=(guint32)weed_leaf_element_size(plant,key,j);
    if (st!=WEED_SEED_STRING) {
      value=g_malloc((size_t)vlen);
      weed_leaf_get(plant,key,j,value);
    }
    else {
      value=g_malloc((size_t)(vlen+1));
      weed_leaf_get(plant,key,j,&value);
    }
    if (mem==NULL) {
      dummyvar=write(fd,&vlen,sizint); // actually should be size_t
      dummyvar=write(fd,value,(size_t)vlen);
    }
    else {
      w_memcpy(*mem,&vlen,sizint);  // actually should be size_t
      *mem+=sizint;
      w_memcpy(*mem,value,(size_t)vlen);
      *mem+=vlen;
    }
    g_free(value);
  }
}



gboolean weed_plant_serialise(int fd, weed_plant_t *plant, unsigned char **mem) {
  int i=0;
  char **proplist=weed_plant_list_leaves(plant);
  char *prop;
  for (prop=proplist[0];(prop=proplist[i])!=NULL;i++);

  if (mem==NULL) dummyvar=write(fd,&i,sizint); // write number of leaves
  else {
    w_memcpy(*mem,&i,sizint);
    *mem+=sizint;
  }

  weed_leaf_serialise(fd,plant,"type",TRUE,mem);
  i=0;

  for (prop=proplist[0];(prop=proplist[i])!=NULL;i++) {
    // write each leaf and key
    if (strcmp(prop,"type")) weed_leaf_serialise(fd,plant,prop,TRUE,mem);
    weed_free(prop);
  }
  weed_free(proplist);
  return TRUE;
}



static gint weed_leaf_deserialise(int fd, weed_plant_t *plant, gchar *key, unsigned char **mem) {
  // if plant is NULL, returns type
  // "host_default" sets key; otherwise NULL
  size_t bytes;
  guint32 len,vlen;
  int st; // seed type
  int ne; // num elems
  void **values;
  int i,j;

  int *ints;
  double *dubs;
  gint64 *int64s;
  
  int type=0;

  gboolean free_key=FALSE;

  if (key==NULL) {
    if (mem==NULL) {
      bytes=read(fd,&len,sizint);
      if (bytes<sizint) {
	return -4;
      }
    }
    else {
      w_memcpy(&len,*mem,sizint);
      *mem+=sizint;
    }
  
    key=g_try_malloc((size_t)len+1);
    if (key==NULL) return -5;

    if (mem==NULL) {
      bytes=read(fd,key,(size_t)len);
      if (bytes<len) {
	g_free(key);
	return -6;
      }
    }
    else {
      w_memcpy(key,*mem,(size_t)len);
      *mem+=len;
    }
    memset(key+(size_t)len,0,1);
    free_key=TRUE;
  }
  if (mem==NULL) {
    bytes=read(fd,&st,sizint);
    if (bytes<sizint) {
      if (free_key) g_free(key);
      return -7;
    }
  }
  else {
    w_memcpy(&st,*mem,sizint);
    *mem+=sizint;
  }

  if (mem==NULL) {
    bytes=read(fd,&ne,sizint);
    if (bytes<sizint) {
      if (free_key) g_free(key);
      return -8;
    }
  }
  else {
    w_memcpy(&ne,*mem,sizint);
    *mem+=sizint;
  }

  if (ne>0) values=g_malloc(ne*sizeof(void *));
  else values=NULL;

  for (i=0;i<ne;i++) {
    if (mem==NULL) {
      bytes=read(fd,&vlen,sizint);
      if (bytes<sizint) {
	for (--i;i>=0;g_free(values[i--]));
	g_free(values);
	if (free_key) g_free(key);
	return -9;
      }
    }
    else {
      w_memcpy(&vlen,*mem,sizint);
      *mem+=sizint;
    }
      
    if (st==WEED_SEED_STRING) {
      values[i]=g_malloc((size_t)vlen+1);
    }
    else values[i]=g_malloc((size_t)vlen);

    if (mem==NULL) {
      bytes=read(fd,values[i],vlen);
      if (bytes<vlen) {
	for (--i;i>=0;g_free(values[i--]));
	g_free(values);
	if (free_key) g_free(key);
	return -10;
      }
    }
    else {
      w_memcpy(values[i],*mem,vlen);
      *mem+=vlen;
    }
    if (st==WEED_SEED_STRING) {
      memset((char *)values[i]+vlen,0,1);
    }
  }

  // if plant was NULL, we should return a single int "type"
  if (plant==NULL) {
    if (strcmp(key,"type")) {
      if (free_key) g_free(key);
      return -1;
    }
    if (st!=WEED_SEED_INT) {
      if (free_key) g_free(key);
      return -2;
    }
    if (ne!=1) {
      if (free_key) g_free(key);
      return -3;
    }
    type=*(int*)(values[0]);
  }
  else {
    if (values==NULL) weed_leaf_set(plant,key,st,0,NULL);
    else {
      switch (st) {
      case WEED_SEED_INT:
	// fallthrough
      case WEED_SEED_BOOLEAN:
	ints=g_malloc(ne*sizint);
	for (j=0;j<ne;j++) ints[j]=*(int *)values[j];
	weed_leaf_set (plant, key, st, ne, (void *)ints);
	g_free(ints);
	break;
      case WEED_SEED_DOUBLE:
	dubs=g_malloc(ne*sizdbl);
	for (j=0;j<ne;j++) dubs[j]=*(double *)values[j];
	weed_leaf_set (plant, key, st, ne, (void *)dubs);
	g_free(dubs);
	break;
      case WEED_SEED_INT64:
	int64s=g_malloc(ne*8);
	for (j=0;j<ne;j++) int64s[j]=*(int64_t *)values[j];
	weed_leaf_set (plant, key, st, ne, (void *)int64s);
	g_free(int64s);
	break;
      case WEED_SEED_STRING:
	weed_leaf_set (plant, key, st, ne, (void *)values);
	break;
      default:
	if (plant!=NULL) {
	  void **voids=g_malloc(ne*sizeof(void *));
	  for (j=0;j<ne;j++) voids[j]=*(void **)values[j];
	  weed_leaf_set (plant, key, st, ne, (void *)voids);
	  g_free(voids);
	}
      }
    }
  }
  if (values!=NULL) {
    for (i=0;i<ne;i++) g_free(values[i]);
    g_free(values);
  }
  if (free_key) g_free(key);
  if (plant==NULL) return type;
  return 0;
}



weed_plant_t *weed_plant_deserialise(int fd, unsigned char **mem) {
  // desrialise a plant from file fd
  weed_plant_t *plant;
  int numleaves;
  size_t bytes;
  int err;

  if (mem==NULL) {
    if ((bytes=read(fd,&numleaves,sizint))<sizint) return NULL;
  }
  else {
    w_memcpy(&numleaves,*mem,sizint);
    *mem+=sizint;
  }

  plant=weed_plant_new(WEED_PLANT_UNKNOWN);
  weed_leaf_set_flags (plant,"type",0);

  while (numleaves--) {
    if ((err=weed_leaf_deserialise(fd,plant,NULL,mem))) {
      weed_plant_free(plant);
      return NULL;
    }
  }
  if (weed_get_plant_type(plant)==WEED_PLANT_UNKNOWN) {
    weed_plant_free(plant);
    return NULL;
  }
  weed_leaf_set_flags (plant,"type",WEED_LEAF_READONLY_PLUGIN|WEED_LEAF_READONLY_HOST);

  return plant;
}




void write_filter_defaults (int fd, int idx) {
  gchar *hashname;
  weed_plant_t *filter=weed_filters[idx],**ptmpls;
  int num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  int i,error;
  gboolean wrote_hashname=FALSE;
  size_t vlen;
  int ntowrite=0;

  if (num_params==0) return;
  ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  for (i=0;i<num_params;i++) {
    if (weed_plant_has_leaf(ptmpls[i],"host_default")) {
      ntowrite++;
    }
  }

  for (i=0;i<num_params;i++) {
    if (weed_plant_has_leaf(ptmpls[i],"host_default")) {
      if (!wrote_hashname) {
	hashname=make_weed_hashname(idx);
	vlen=strlen(hashname);
	
	dummyvar=write(fd,&vlen,sizeof(size_t));
	dummyvar=write(fd,hashname,vlen);
	g_free(hashname);
	wrote_hashname=TRUE;
	dummyvar=write(fd,&ntowrite,sizint);
      }
      dummyvar=write(fd,&i,sizint);
      weed_leaf_serialise(fd,ptmpls[i],"host_default",FALSE,NULL);
    }
  }
  if (wrote_hashname) dummyvar=write(fd,"\n",1);

  if (ptmpls!=NULL) weed_free(ptmpls);

}




void read_filter_defaults(int fd) {
  gboolean eof=FALSE;
  void *buf;
  ssize_t bytes;
  size_t vlen;
  int i,error,pnum;
  weed_plant_t *filter,**ptmpls;
  int num_params=0;
  gchar *tmp;
  int ntoread;

  while (!eof) {
    bytes=read(fd,&vlen,sizeof(size_t));
    if (bytes<sizeof(size_t)) {
      eof=TRUE;
      break;
    }

    buf=g_malloc(vlen+1);
    bytes=read(fd,buf,vlen);
    if (bytes<vlen) {
      eof=TRUE;
      break;
    }
    memset((char *)buf+vlen,0,1);
    for (i=0;i<num_weed_filters;i++) {
      if (!strcmp(buf,(tmp=make_weed_hashname(i)))) {
	g_free(tmp);
	break;
      }
      g_free(tmp);
    }

    g_free(buf);

    if (i>=num_weed_filters) continue;

    ptmpls=NULL;

    filter=weed_filters[i];
 
    num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
    if (num_params>0) ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

    bytes=read(fd,&ntoread,sizint);
    if (bytes<sizint) {
      eof=TRUE;
      break;
    }
    for (i=0;i<ntoread;i++) {
      bytes=read(fd,&pnum,sizint);
      if (bytes<sizint) {
	eof=TRUE;
	break;
      }
      if (pnum<num_params) {
	weed_leaf_deserialise(fd,ptmpls[pnum],"host_default",NULL);
      }
    }
    buf=g_malloc(strlen("\n"));
    dummyvar=read(fd,buf,strlen("\n"));
    g_free(buf);
    if (ptmpls!=NULL) weed_free(ptmpls);
  }
}




void write_generator_sizes (int fd, int idx) {
  // TODO - handle optional channels
  gchar *hashname;
  weed_plant_t *filter,**ctmpls;
  int num_channels;
  int i,error;
  size_t vlen;
  gboolean wrote_hashname=FALSE;

  num_channels=enabled_in_channels(weed_filters[idx],FALSE);
  if (num_channels!=0) return;

  filter=weed_filters[idx];

  num_channels=weed_leaf_num_elements(filter,"out_channel_templates");
  if (num_channels==0) return;

  ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);

  for (i=0;i<num_channels;i++) {
    if (weed_plant_has_leaf(ctmpls[i],"host_width")||weed_plant_has_leaf(ctmpls[i],"host_height")||(!wrote_hashname&&weed_plant_has_leaf(filter,"host_fps"))) {
      if (!wrote_hashname) {
	hashname=make_weed_hashname(idx);
	vlen=strlen(hashname);
	dummyvar=write(fd,&vlen,sizeof(size_t));
	dummyvar=write(fd,hashname,vlen);
	g_free(hashname);
	wrote_hashname=TRUE;

	if (weed_plant_has_leaf(filter,"host_fps")) {
	  int j=-1;
	  dummyvar=write(fd,&j,sizint);
	  weed_leaf_serialise(fd,filter,"host_fps",FALSE,NULL);
	}
      }
  
      dummyvar=write(fd,&i,sizint);
      if (weed_plant_has_leaf(ctmpls[i],"host_width")) weed_leaf_serialise(fd,ctmpls[i],"host_width",FALSE,NULL);
      else weed_leaf_serialise(fd,ctmpls[i],"width",FALSE,NULL);
      if (weed_plant_has_leaf(ctmpls[i],"host_height")) weed_leaf_serialise(fd,ctmpls[i],"host_height",FALSE,NULL);
      else weed_leaf_serialise(fd,ctmpls[i],"height",FALSE,NULL);
    }
  }
  if (wrote_hashname) dummyvar=write(fd,"\n",1);
}




void read_generator_sizes(int fd) {
  gboolean eof=FALSE;
  void *buf;
  size_t bytes;
  size_t vlen;
  int i,error;
  weed_plant_t *filter,**ctmpls;
  int num_chans=0;
  int cnum;
  gchar *tmp;

  while (!eof) {
    bytes=read(fd,&vlen,sizeof(size_t));
    if (bytes<sizeof(size_t)) {
      eof=TRUE;
      break;
    }

    buf=g_malloc(vlen+1);
    bytes=read(fd,buf,vlen);
    if (bytes<vlen) {
      eof=TRUE;
      break;
    }
    memset((char *)buf+vlen,0,1);

    for (i=0;i<num_weed_filters;i++) {
      if (!strcmp(buf,(tmp=make_weed_hashname(i)))) {
	g_free(tmp);
	break;
      }
      g_free(tmp);
    }

    g_free(buf);
    ctmpls=NULL;

    if (i<num_weed_filters) {
      filter=weed_filters[i];
      num_chans=weed_leaf_num_elements(filter,"out_channel_templates");
      if (num_chans>0) ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);

      bytes=read(fd,&cnum,sizint);
      if (bytes<sizint) {
	eof=TRUE;
	break;
      }
      
      if (cnum<num_chans&&cnum>=0) {
	weed_leaf_deserialise(fd,ctmpls[cnum],"host_width",NULL);
	weed_leaf_deserialise(fd,ctmpls[cnum],"host_height",NULL);
	if (weed_get_int_value(ctmpls[cnum],"host_width",&error)==0) weed_set_int_value(ctmpls[cnum],"host_width",DEF_GEN_WIDTH);
	if (weed_get_int_value(ctmpls[cnum],"host_height",&error)==0) weed_set_int_value(ctmpls[cnum],"host_height",DEF_GEN_HEIGHT);
      }
      else if (cnum==-1) {
	weed_leaf_deserialise(fd,filter,"host_fps",NULL);
      }
    }

    if (ctmpls!=NULL) weed_free(ctmpls);
    buf=g_malloc(strlen("\n"));
    dummyvar=read(fd,buf,strlen("\n"));
    g_free(buf);
  }
}



void reset_frame_and_clip_index(void) {
  if (mainw->clip_index==NULL) {
    mainw->clip_index=weed_malloc(sizint);
    mainw->clip_index[0]=-1;
    }
  if (mainw->frame_index==NULL) {
    mainw->frame_index=weed_malloc(sizint);
    mainw->frame_index[0]=0;
  }
}
