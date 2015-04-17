// effects-weed.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2014 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#include <dlfcn.h>

#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-host.h"
#endif

#ifdef __cplusplus
#ifdef HAVE_OPENCV
#include "opencv2/core/core.hpp"
using namespace cv;
#endif
#endif

#include "main.h"
#include "effects.h"

///////////////////////////////////

#include "callbacks.h"
#include "support.h"
#include "rte_window.h"
#include "resample.h"
#include "audio.h"
#include "ce_thumbs.h"

////////////////////////////////////////////////////////////////////////


struct _procvals {
  weed_plant_t *inst;
  weed_timecode_t tc;
  int ret;
};



#define OIL_MEMCPY_MAX_BYTES 1024 // this can be tuned to provide optimal performance

#ifdef ENABLE_OIL
livespointer lives_memcpy(livespointer dest, livesconstpointer src, size_t n) {
#ifndef __cplusplus
  if (n>=32&&n<=OIL_MEMCPY_MAX_BYTES) {
    oil_memcpy((uint8_t *)dest,(const uint8_t *)src,n);
    return dest;
  }
#endif
  return memcpy(dest,src,n);
}
#else
livespointer lives_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  return memcpy(dest,src,n);
}
#endif

G_GNUC_MALLOC livespointer _lives_malloc(size_t size) {
#ifdef __cplusplus
#ifdef HAVE_OPENCV
  return fastMalloc(size);
#endif
#endif
  return malloc(size);
}


livespointer  _lives_realloc(livespointer ptr, size_t new_size) {
#ifdef __cplusplus
#ifdef HAVE_OPENCV
  livespointer nptr = _lives_malloc(new_size);
  if (nptr) {
    if (ptr) {
      lives_memcpy(nptr,ptr,new_size);
      _lives_free_normal(ptr);
    }
  }
  return nptr;
#endif
#endif
  return realloc(ptr,new_size);
}


void _lives_free_normal(livespointer ptr) {
#ifdef __cplusplus
#ifdef HAVE_OPENCV
  fastFree(ptr);
  return;
#endif
#endif
  free(ptr);
}


void filter_mutex_lock(int key) {
  // lock a filter before setting the "value" of in_parameter or reading the "value" of an out_parameter
  if (key>=0&&key<FX_KEYS_MAX) pthread_mutex_lock(&mainw->data_mutex[key]);
  //g_print ("lock %d\n",key);
}

void filter_mutex_unlock(int key) {
  if (key>=0&&key<FX_KEYS_MAX) pthread_mutex_unlock(&mainw->data_mutex[key]);
  //g_print ("unlock %d\n",key);
}

// special de-allocators which avoid free()ing mainw->do_not_free...this is necessary to "hack" into gdk-pixbuf
// do not use this directly in code (use lives_free())
void _lives_free_with_check(livespointer ptr) {
  if (ptr==mainw->do_not_free) return;
  _lives_free_normal(ptr);
}

void _lives_free(livespointer ptr) {
  (*mainw->free_fn)(ptr);
}

livespointer lives_memset(livespointer s, int c, size_t n) {
  return memset(s,c,n);
}

#ifdef GUI_GTK
livespointer lives_calloc(size_t nmemb, size_t size) {
#if GTK_CHECK_VERSION(2,24,0)
  return lives_try_malloc0_n(nmemb,size);
#endif
  return calloc(nmemb,size);
}
#endif


////////////////////////////////////////////////////////////////////////////

void weed_add_plant_flags(weed_plant_t *plant, int flags) {
  char **leaves=weed_plant_list_leaves(plant);
  int i,currflags;

  for (i=0; leaves[i]!=NULL; i++) {
    currflags=flags;
    if (flags&WEED_LEAF_READONLY_PLUGIN&&(!strncmp(leaves[i],"plugin_",7))) currflags^=WEED_LEAF_READONLY_PLUGIN;
    weed_leaf_set_flags(plant,leaves[i],weed_leaf_get_flags(plant,leaves[i])|currflags);
    lives_free(leaves[i]);
  }
  lives_free(leaves);
}


static void weed_clear_plant_flags(weed_plant_t *plant, int flags) {
  char **leaves=weed_plant_list_leaves(plant);
  int i;

  for (i=0; leaves[i]!=NULL; i++) {
    weed_leaf_set_flags(plant,leaves[i],(weed_leaf_get_flags(plant,leaves[i])|flags)^flags);
    lives_free(leaves[i]);
  }
  lives_free(leaves);
}


static int match_highest_version(int *hostv, int hostn, int *plugv, int plugn) {
  int hmatch=0;
  int i,j;

  for (i=0; i<plugn; i++) {
    for (j=0; j<hostn; j++) {
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
weed_leaf_set_f wlsp;
weed_malloc_f weedmalloc;
weed_free_f weedfree;
weed_memcpy_f weedmemcpy;
weed_memset_f weedmemset;


weed_plant_t *weed_bootstrap_func(weed_default_getter_f *value, int num_versions, int *plugin_versions) {
  int host_api_versions_supported[]= {131}; // must be ordered in ascending order
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

  wlsp=weed_leaf_set_plugin; // we pass the plugin's version to the plugin - an example of overloading with Weed
  weedmalloc=weed_malloc;
  weedfree=weed_free;

  weedmemcpy=weed_memcpy;
  weedmemset=weed_memset;

  if (num_versions<1) return NULL;
  if ((host_api_version=match_highest_version(host_api_versions_supported,1,plugin_versions,num_versions))==0) return NULL;
  switch (host_api_version) {
  case 100:
  case 110:
  case 120:
  case 130:
  case 131:
    value[0]=wdg; // bootstrap weed_default_get (the plugin's default_getter)

    weed_set_int_value(host_info,"api_version",host_api_version);

    // here we set (void *)&fn_ptr
    weed_set_voidptr_value(host_info,"weed_leaf_get_func",&wlg);
    weed_set_voidptr_value(host_info,"weed_leaf_set_func",&wlsp);
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

int num_compound_fx(weed_plant_t *plant) {
  weed_plant_t *filter;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) filter=weed_instance_get_filter(plant,TRUE);
  else filter=plant;

  if (!weed_plant_has_leaf(filter,"host_filter_list")) return 1;
  return weed_leaf_num_elements(filter,"host_filter_list");

}




boolean has_non_alpha_palette(weed_plant_t *ctmpl) {
  int *plist;
  int error;
  int npals=0;

  register int i;

  if (!weed_plant_has_leaf(ctmpl,"palette_list")) return TRUE; ///< most probably audio
  npals=weed_leaf_num_elements(ctmpl,"palette_list");

  plist=weed_get_int_array(ctmpl,"palette_list",&error);
  for (i=0; i<npals; i++) {
    if (!weed_palette_is_alpha_palette(plist[i])) {
      lives_free(plist);
      return TRUE;
    }
  }
  lives_free(plist);
  return FALSE;
}


boolean has_alpha_palette(weed_plant_t *ctmpl) {
  int *plist;
  int error;
  int npals=0;

  register int i;

  if (!weed_plant_has_leaf(ctmpl,"palette_list")) return TRUE; ///< most probably audio
  npals=weed_leaf_num_elements(ctmpl,"palette_list");

  plist=weed_get_int_array(ctmpl,"palette_list",&error);
  for (i=0; i<npals; i++) {
    if (weed_palette_is_alpha_palette(plist[i])) {
      lives_free(plist);
      return TRUE;
    }
  }
  lives_free(plist);
  return FALSE;
}


weed_plant_t *weed_instance_get_filter(weed_plant_t *inst, boolean get_compound_parent) {
  int error;
  if (get_compound_parent&&
      (weed_plant_has_leaf(inst,"host_compound_class"))) return weed_get_plantptr_value(inst,"host_compound_class",&error);
  return weed_get_plantptr_value(inst,"filter_class",&error);
}


lives_fx_cat_t weed_filter_categorise(weed_plant_t *pl, int in_channels, int out_channels) {
  weed_plant_t *filt=pl;
  int filter_flags,error;
  boolean has_out_params=FALSE;
  boolean has_in_params=FALSE;
  boolean has_mandatory_in=FALSE;
  boolean all_out_alpha=TRUE;
  boolean all_in_alpha=TRUE;
  boolean has_in_alpha=FALSE;

  register int i;

  if (WEED_PLANT_IS_FILTER_INSTANCE(pl)) filt=weed_instance_get_filter(pl,TRUE);

  // check mandatory output chans, see if any are non-alpha
  if (weed_plant_has_leaf(filt,"out_channel_templates")) {
    int nouts=weed_leaf_num_elements(filt,"out_channel_templates");
    if (nouts>0) {
      weed_plant_t **ctmpls=weed_get_plantptr_array(filt,"out_channel_templates",&error);
      for (i=0; i<nouts; i++) {
        if (weed_plant_has_leaf(ctmpls[i],"optional")&&
            weed_get_boolean_value(ctmpls[i],"optional",&error)==WEED_TRUE) continue; ///< ignore optional channels
        if (has_non_alpha_palette(ctmpls[i])) {
          all_out_alpha=FALSE;
          break;
        }
      }
      lives_free(ctmpls);
    }
  }

  // check mandatory input chans, see if any are non-alpha
  if (weed_plant_has_leaf(filt,"in_channel_templates")) {
    int nins=weed_leaf_num_elements(filt,"in_channel_templates");
    if (nins>0) {
      weed_plant_t **ctmpls=weed_get_plantptr_array(filt,"in_channel_templates",&error);
      for (i=0; i<nins; i++) {
        if (weed_plant_has_leaf(ctmpls[i],"optional")&&
            weed_get_boolean_value(ctmpls[i],"optional",&error)==WEED_TRUE) continue; ///< ignore optional channels
        has_mandatory_in=TRUE;
        if (has_non_alpha_palette(ctmpls[i])) {
          all_in_alpha=FALSE;
        } else has_in_alpha=TRUE;
      }
      if (!has_mandatory_in) {
        for (i=0; i<nins; i++) {
          if (has_non_alpha_palette(ctmpls[i])) {
            all_in_alpha=FALSE;
          } else has_in_alpha=TRUE;
        }
      }
      lives_free(ctmpls);
    }
  }

  filter_flags=weed_get_int_value(filt,"flags",&error);
  if (weed_plant_has_leaf(filt,"out_parameter_templates")) has_out_params=TRUE;
  if (weed_plant_has_leaf(filt,"in_parameter_templates")) has_in_params=TRUE;
  if (filter_flags&WEED_FILTER_IS_CONVERTER) return LIVES_FX_CAT_CONVERTER;
  if (in_channels==0&&out_channels>0&&all_out_alpha) return LIVES_FX_CAT_DATA_GENERATOR;
  if (in_channels==0&&out_channels>0) {
    if (!has_audio_chans_out(filt,TRUE)) return LIVES_FX_CAT_VIDEO_GENERATOR;
    else if (has_video_chans_out(filt,TRUE)) return LIVES_FX_CAT_AV_GENERATOR;
    else return LIVES_FX_CAT_AUDIO_GENERATOR;
  }
  if (out_channels>=1&&in_channels>=1&&(all_in_alpha||has_in_alpha)&&!all_out_alpha) return LIVES_FX_CAT_DATA_VISUALISER;
  if (out_channels>=1&&all_out_alpha) return LIVES_FX_CAT_ANALYSER;
  if (out_channels>1) return LIVES_FX_CAT_SPLITTER;

  if (in_channels>2&&out_channels==1) {
    return LIVES_FX_CAT_COMPOSITOR;
  }
  if (in_channels==2&&out_channels==1) return LIVES_FX_CAT_TRANSITION;
  if (in_channels==1&&out_channels==1&&!(has_video_chans_in(filt,TRUE))&&!(has_video_chans_out(filt,TRUE))) return LIVES_FX_CAT_AUDIO_EFFECT;
  if (in_channels==1&&out_channels==1) return LIVES_FX_CAT_EFFECT;
  if (in_channels>0&&out_channels==0&&has_out_params) return LIVES_FX_CAT_ANALYSER;
  if (in_channels>0&&out_channels==0) return LIVES_FX_CAT_TAP;
  if (in_channels==0&&out_channels==0&&has_out_params&&has_in_params) return LIVES_FX_CAT_DATA_PROCESSOR;
  if (in_channels==0&&out_channels==0&&has_out_params) return LIVES_FX_CAT_DATA_SOURCE;
  if (in_channels==0&&out_channels==0) return LIVES_FX_CAT_UTILITY;
  return LIVES_FX_CAT_NONE;
}


lives_fx_cat_t weed_filter_subcategorise(weed_plant_t *pl, lives_fx_cat_t category, boolean count_opt) {
  weed_plant_t *filt=pl;
  boolean has_video_chansi;

  if (WEED_PLANT_IS_FILTER_INSTANCE(pl)) filt=weed_instance_get_filter(pl,TRUE);

  if (category==LIVES_FX_CAT_COMPOSITOR) count_opt=TRUE;

  has_video_chansi=has_video_chans_in(filt,count_opt);

  if (category==LIVES_FX_CAT_TRANSITION) {
    if (get_transition_param(filt,FALSE)!=-1) {
      if (!has_video_chansi) return LIVES_FX_CAT_AUDIO_TRANSITION;
      return LIVES_FX_CAT_AV_TRANSITION;
    }
    return LIVES_FX_CAT_VIDEO_TRANSITION;
  }

  if (category==LIVES_FX_CAT_COMPOSITOR&&!has_video_chansi) return LIVES_FX_CAT_AUDIO_MIXER;
  if (category==LIVES_FX_CAT_EFFECT&&!has_video_chansi) return LIVES_FX_CAT_AUDIO_EFFECT;
  if (category==LIVES_FX_CAT_CONVERTER&&!has_video_chansi) return LIVES_FX_CAT_AUDIO_VOL;

  if (category==LIVES_FX_CAT_ANALYSER) {
    if (!has_video_chansi) return LIVES_FX_CAT_AUDIO_ANALYSER;
    return LIVES_FX_CAT_VIDEO_ANALYSER;
  }

  return LIVES_FX_CAT_NONE;
}


char *weed_seed_type_to_text(int seed_type) {
  switch (seed_type) {
  case WEED_SEED_INT:
    return lives_strdup(_("integer"));
  case WEED_SEED_INT64:
    return lives_strdup(_("int64"));
  case WEED_SEED_BOOLEAN:
    return lives_strdup(_("boolean"));
  case WEED_SEED_DOUBLE:
    return lives_strdup(_("double"));
  case WEED_SEED_STRING:
    return lives_strdup(_("string"));
  default:
    return lives_strdup(_("pointer"));
  }
}

int num_alpha_channels(weed_plant_t *filter, boolean out) {
  // get number of alpha channels (in or out) for filter; including optional

  weed_plant_t **ctmpls;

  int count=0;
  int nchans,error;

  register int i;

  if (out) {
    if (!weed_plant_has_leaf(filter,"out_channel_templates")) return FALSE;
    nchans=weed_leaf_num_elements(filter,"out_channel_templates");
    if (nchans==0) return FALSE;
    ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
    for (i=0; i<nchans; i++) {
      if (has_non_alpha_palette(ctmpls[i])) continue;
      count++;
    }
  } else {
    if (!weed_plant_has_leaf(filter,"in_channel_templates")) return FALSE;
    nchans=weed_leaf_num_elements(filter,"in_channel_templates");
    if (nchans==0) return FALSE;
    ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
    for (i=0; i<nchans; i++) {
      if (has_non_alpha_palette(ctmpls[i])) continue;
      count++;
    }
  }
  lives_free(ctmpls);
  return count;

}



////////////////////////////////////////////////////////////////////////

#define MAX_WEED_INSTANCES 65536

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
static int fg_generator_clip;


//////////////////////////////////////////////////////////////////////

static weed_plant_t **weed_filters; // array of filter_classes
static weed_plant_t **dupe_weed_filters; // array of duplicate filter_classes (only used during loading)

// each 'hotkey' controls n instances, selectable as 'modes' or banks
static weed_plant_t **key_to_instance[FX_KEYS_MAX];
static weed_plant_t **key_to_instance_copy[FX_KEYS_MAX]; // copy for preview during rendering

static int *key_to_fx[FX_KEYS_MAX];
static int key_modes[FX_KEYS_MAX];

static int num_weed_filters; ///< count of how many filters we have loaded
static int num_weed_dupes; ///< number of duplicate filters (with multiple versions)

static char **hashnames;
static char **dupe_hashnames;

// per key/mode parameter defaults
weed_plant_t ** *key_defaults[FX_KEYS_MAX_VIRTUAL];

/////////////////// LiVES event system /////////////////

static weed_plant_t *init_events[FX_KEYS_MAX_VIRTUAL];
static void **pchains[FX_KEYS_MAX]; // parameter changes, used during recording (not for rendering)
static void *filter_map[FX_KEYS_MAX+2];
static int next_free_key;

////////////////////////////////////////////////////////////////////


void backup_weed_instances(void) {
  // this is called during multitrack rendering.
  // We are rendering, but we want to display the current frame in the preview window
  // thus we backup our rendering instances, apply the current frame instances, and then restore the rendering instances
  register int i;

  for (i=FX_KEYS_MAX_VIRTUAL; i<FX_KEYS_MAX; i++) {
    key_to_instance_copy[i][0]=key_to_instance[i][0];
    key_to_instance[i][0]=NULL;
  }
}


void restore_weed_instances(void) {
  register int i;

  for (i=FX_KEYS_MAX_VIRTUAL; i<FX_KEYS_MAX; i++) {
    key_to_instance[i][0]=key_to_instance_copy[i][0];
  }
}



LIVES_INLINE int step_val(int val, int step) {
  int ret=(int)(val/step+.5)*step;
  return ret==0?step:ret;
}


static char *weed_filter_get_type(weed_plant_t *filter, boolean getsub, boolean format) {
  // return value should be lives_free'd after use
  char *tmp1,*tmp2,*ret;
  lives_fx_cat_t cat=weed_filter_categorise(filter,
                     enabled_in_channels(filter,TRUE),
                     enabled_out_channels(filter,TRUE));

  lives_fx_cat_t sub=weed_filter_subcategorise(filter,cat,FALSE);

  if (!getsub||sub==LIVES_FX_CAT_NONE)
    return lives_fx_cat_to_text(cat,FALSE);

  tmp1=lives_fx_cat_to_text(cat,FALSE);
  tmp2=lives_fx_cat_to_text(sub,FALSE);

  if (format) ret=lives_strdup_printf("%s (%s)",tmp1,tmp2);
  else {
    if (cat==LIVES_FX_CAT_TRANSITION) ret=lives_strdup_printf("%s - %s",tmp1,tmp2);
    else ret=lives_strdup_printf("%s",tmp2);
  }

  lives_free(tmp1);
  lives_free(tmp2);

  return ret;

}

void update_host_info(weed_plant_t *inst) {
  // set "host_audio_plugin" in the host_info
  int error;
  weed_plant_t *filter,*pinfo,*hinfo;

  filter=weed_instance_get_filter(inst,FALSE);
  pinfo=weed_get_plantptr_value(filter,"plugin_info",&error);
  hinfo=weed_get_plantptr_value(pinfo,"host_info",&error);

  switch (prefs->audio_player) {
  case AUD_PLAYER_MPLAYER:
    weed_set_string_value(hinfo,"host_audio_player","mplayer");
    break;
  case AUD_PLAYER_MPLAYER2:
    weed_set_string_value(hinfo,"host_audio_player","mplayer2");
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


static weed_plant_t *get_enabled_channel_inner(weed_plant_t *inst, int which, boolean is_in, boolean audio_only) {
  // plant is a filter_instance
  // "which" starts at 0
  weed_plant_t **channels;
  weed_plant_t *retval,*ctmpl=NULL;

  int error,nchans=3;

  register int i=0;

  if (!WEED_PLANT_IS_FILTER_INSTANCE(inst)) return NULL;

  if (is_in) {
    if (!weed_plant_has_leaf(inst,"in_channels")) return NULL;
    channels=weed_get_plantptr_array(inst,"in_channels",&error);
    nchans=weed_leaf_num_elements(inst,"in_channels");
  } else {
    if (!weed_plant_has_leaf(inst,"out_channels")) return NULL;
    channels=weed_get_plantptr_array(inst,"out_channels",&error);
    nchans=weed_leaf_num_elements(inst,"out_channels");
  }

  if (channels==NULL||nchans==0) return NULL;

  while (1) {
    if (!weed_plant_has_leaf(channels[i],"disabled")||weed_get_boolean_value(channels[i],"disabled",&error)==WEED_FALSE) {
      if (audio_only) ctmpl=weed_get_plantptr_value(channels[i],"template",&error);
      if (!audio_only||(audio_only&&weed_plant_has_leaf(ctmpl,"is_audio")&&
                        weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE)) {
        which--;
      }
    }
    if (which<0) break;
    if (++i>=nchans) {
      lives_free(channels);
      return NULL;
    }
  }
  retval=channels[i];
  lives_free(channels);
  return retval;
}


weed_plant_t *get_enabled_channel(weed_plant_t *inst, int which, boolean is_in) {
  // count audio and video channels
  return get_enabled_channel_inner(inst,which,is_in,FALSE);
}


weed_plant_t *get_enabled_audio_channel(weed_plant_t *inst, int which, boolean is_in) {
  // count only audio channels
  return get_enabled_channel_inner(inst,which,is_in,TRUE);
}


weed_plant_t *get_mandatory_channel(weed_plant_t *filter, int which, boolean is_in) {
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
  lives_free(ctmpls);
  return retval;
}


boolean weed_filter_is_resizer(weed_plant_t *filt) {
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



boolean weed_instance_is_resizer(weed_plant_t *inst) {
  weed_plant_t *ftmpl=weed_instance_get_filter(inst,TRUE);
  return weed_filter_is_resizer(ftmpl);
}



boolean is_audio_channel_in(weed_plant_t *inst, int chnum) {
  int error,nchans=weed_leaf_num_elements(inst,"in_channels");
  weed_plant_t **in_chans;
  weed_plant_t *ctmpl;

  if (nchans<=chnum) return FALSE;

  in_chans=weed_get_plantptr_array(inst,"in_channels",&error);
  ctmpl=weed_get_plantptr_value(in_chans[chnum],"template",&error);
  lives_free(in_chans);

  if (weed_plant_has_leaf(ctmpl,"is_audio")&&weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE) {
    return TRUE;
  }
  return FALSE;
}


weed_plant_t *get_audio_channel_in(weed_plant_t *inst, int achnum) {
  // get nth audio channel in (not counting video channels)

  int error,nchans=weed_leaf_num_elements(inst,"in_channels");
  weed_plant_t **in_chans;
  weed_plant_t *ctmpl,*achan;

  register int i;

  if (nchans==0) return NULL;

  in_chans=weed_get_plantptr_array(inst,"in_channels",&error);

  for (i=0; i<nchans; i++) {
    ctmpl=weed_get_plantptr_value(in_chans[i],"template",&error);
    if (weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE) {
      if (achnum--==0) {
        achan=in_chans[i];
        lives_free(in_chans);
        return achan;
      }
    }
  }
  lives_free(in_chans);
  return NULL;
}


boolean has_video_chans_in(weed_plant_t *filter, boolean count_opt) {
  int error,nchans=weed_leaf_num_elements(filter,"in_channel_templates");
  weed_plant_t **in_ctmpls;
  int i;

  if (nchans==0) return FALSE;

  in_ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  for (i=0; i<nchans; i++) {
    if (!count_opt&&weed_plant_has_leaf(in_ctmpls[i],"optional")&&
        weed_get_boolean_value(in_ctmpls[i],"optional",&error)==WEED_TRUE) continue;
    if (weed_plant_has_leaf(in_ctmpls[i],"is_audio")&&weed_get_boolean_value(in_ctmpls[i],"is_audio",&error)==WEED_TRUE)
      continue;
    lives_free(in_ctmpls);
    return TRUE;
  }
  lives_free(in_ctmpls);

  return FALSE;
}



boolean has_audio_chans_in(weed_plant_t *filter, boolean count_opt) {
  int error,nchans=weed_leaf_num_elements(filter,"in_channel_templates");
  weed_plant_t **in_ctmpls;
  int i;

  if (nchans==0) return FALSE;

  in_ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  for (i=0; i<nchans; i++) {
    if (!count_opt&&weed_plant_has_leaf(in_ctmpls[i],"optional")&&
        weed_get_boolean_value(in_ctmpls[i],"optional",&error)==WEED_TRUE) continue;
    if (!weed_plant_has_leaf(in_ctmpls[i],"is_audio")||
        weed_get_boolean_value(in_ctmpls[i],"is_audio",&error)==WEED_FALSE) continue;
    lives_free(in_ctmpls);
    return TRUE;
  }
  lives_free(in_ctmpls);

  return FALSE;
}


boolean is_audio_channel_out(weed_plant_t *inst, int chnum) {
  int error,nchans=weed_leaf_num_elements(inst,"out_channels");
  weed_plant_t **out_chans;
  weed_plant_t *ctmpl;

  if (nchans<=chnum) return FALSE;

  out_chans=weed_get_plantptr_array(inst,"out_channels",&error);
  ctmpl=weed_get_plantptr_value(out_chans[chnum],"template",&error);
  lives_free(out_chans);

  if (weed_plant_has_leaf(ctmpl,"is_audio")&&weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE) {
    return TRUE;
  }
  return FALSE;
}


boolean has_video_chans_out(weed_plant_t *filter, boolean count_opt) {
  int error,nchans=weed_leaf_num_elements(filter,"out_channel_templates");
  weed_plant_t **out_ctmpls;
  int i;

  if (nchans==0) return FALSE;

  out_ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
  for (i=0; i<nchans; i++) {
    if (!count_opt&&weed_plant_has_leaf(out_ctmpls[i],"optional")&&
        weed_get_boolean_value(out_ctmpls[i],"optional",&error)==WEED_TRUE) continue;
    if (weed_plant_has_leaf(out_ctmpls[i],"is_audio")&&
        weed_get_boolean_value(out_ctmpls[i],"is_audio",&error)==WEED_TRUE) continue;
    lives_free(out_ctmpls);
    return TRUE;
  }
  lives_free(out_ctmpls);

  return FALSE;
}



boolean has_audio_chans_out(weed_plant_t *filter, boolean count_opt) {
  int error,nchans=weed_leaf_num_elements(filter,"out_channel_templates");
  weed_plant_t **out_ctmpls;
  int i;

  if (nchans==0) return FALSE;

  out_ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
  for (i=0; i<nchans; i++) {
    if (!count_opt&&weed_plant_has_leaf(out_ctmpls[i],"optional")&&
        weed_get_boolean_value(out_ctmpls[i],"optional",&error)==WEED_TRUE) continue;
    if (!weed_plant_has_leaf(out_ctmpls[i],"is_audio")||
        weed_get_boolean_value(out_ctmpls[i],"is_audio",&error)==WEED_FALSE) continue;
    lives_free(out_ctmpls);
    return TRUE;
  }
  lives_free(out_ctmpls);

  return FALSE;
}


boolean is_pure_audio(weed_plant_t *plant, boolean count_opt) {
  weed_plant_t *filter=plant;
  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) filter=weed_instance_get_filter(plant,FALSE);

  if ((has_audio_chans_in(filter,count_opt) || has_audio_chans_out(filter,count_opt)) &&
      !has_video_chans_in(filter,count_opt) && !has_video_chans_out(filter,count_opt)) return TRUE;
  return FALSE;
}



boolean weed_parameter_has_variable_elements_strict(weed_plant_t *inst, weed_plant_t *ptmpl) {
  /** see if param has variable elements, using the strictest check */
  weed_plant_t **chans,*ctmpl;
  int error,i;
  int flags=weed_get_int_value(ptmpl,"flags",&error);
  int nchans;

  if (flags&WEED_PARAMETER_VARIABLE_ELEMENTS) return TRUE;

  if (inst==NULL) return FALSE;

  if (!(flags&WEED_PARAMETER_ELEMENT_PER_CHANNEL)) return FALSE;

  if (!weed_plant_has_leaf(inst,"in_channels")
      ||(nchans=weed_leaf_num_elements(inst,"in_channels"))==0)
    return FALSE;

  chans=weed_get_plantptr_array(inst,"in_channels",&error);

  for (i=0; i<nchans; i++) {
    if (weed_plant_has_leaf(chans[i],"disabled")&&
        weed_get_boolean_value(chans[i],"disabled",&error)==WEED_TRUE) continue; //ignore disabled channels
    ctmpl=weed_get_plantptr_value(chans[i],"template",&error);
    if (weed_plant_has_leaf(ctmpl,"max_repeats")&&weed_get_int_value(ctmpl,"max_repeats",&error)!=1) {
      lives_free(chans);
      return TRUE;
    }
  }

  lives_free(chans);
  return FALSE;

}


LIVES_INLINE boolean rte_key_is_enabled(int key) {
  key--;
  return (mainw->rte&(GU641<<key));
}


static void create_filter_map(uint64_t rteval) {
  /** here we create an effect map which defines the order in which effects are applied to a frame stack
   * this is done during recording, the keymap is from mainw->rte which is a bitmap of effect keys
   * keys are applied here from smallest (ctrl-1) to largest (virtual key ctrl-FX_KEYS_MAX_VIRTUAL)

   * this is transformed into filter_map which holds init_events
   * these pointers are then stored in a filter_map event

   * what we actually point to are the init_events for the effects. The init_events are stored when we
   * init an effect

   * during rendering we read the filter_map event, and retrieve the new key, which is at that time
   * held in the
   * "host_tag" property of the init_event, and we apply our effects
   * (which are then bound to virtual keys >=FX_KEYS_MAX_VIRTUAL)

   * [note] that we can do cool things, like mapping the same instance multiple times (though it will always
   * apply itself to the same in/out tracks

   * we don't need to worry about free()ing init_events, since they will be free'd
   * when the instance is deinited
   */

  int count=0,i;
  weed_plant_t *inst;

  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++)
    if (rteval&(GU641<<i)&&(inst=key_to_instance[i][key_modes[i]])!=NULL&&
        enabled_in_channels(inst,FALSE)>0) {
      filter_map[count++]=init_events[i];
    }
  filter_map[count]=NULL; // marks the end of the effect map
}


weed_plant_t *add_filter_deinit_events(weed_plant_t *event_list) {
  // during rendering we use the "keys" FX_KEYS_MAX_VIRTUAL -> FX_KEYS_MAX
  // here we add effect_deinit events to an event_list
  int i;
  boolean needs_filter_map=FALSE;
  weed_timecode_t last_tc=0;

  if (event_list!=NULL) last_tc=get_event_timecode(get_last_event(event_list));

  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    if (init_events[i]!=NULL) {
      event_list=append_filter_deinit_event(event_list,last_tc,init_events[i],pchains[i]);
      init_events[i]=NULL;
      if (pchains[i]!=NULL) lives_free(pchains[i]);
      needs_filter_map=TRUE;
    }
  }
  // add an empty filter_map event (in case more frames are added)
  create_filter_map(mainw->rte); // we create filter_map event_t * array with ordered effects

  if (needs_filter_map) event_list=append_filter_map_event(mainw->event_list,last_tc,filter_map);
  return event_list;
}


weed_plant_t *add_filter_init_events(weed_plant_t *event_list, weed_timecode_t tc) {
  // during rendering we use the "keys" FX_KEYS_MAX_VIRTUAL -> FX_KEYS_MAX
  // here we are about to start playback, and we add init events for every effect which is switched on
  // we add the init events with a timecode of 0
  int i;
  weed_plant_t *inst;
  int fx_idx,ntracks;

  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    if ((inst=key_to_instance[i][key_modes[i]])!=NULL&&enabled_in_channels(inst,FALSE)>0) {
      event_list=append_filter_init_event(event_list,tc,(fx_idx=key_to_fx[i][key_modes[i]]),-1,i,inst);
      init_events[i]=get_last_event(event_list);
      ntracks=weed_leaf_num_elements(init_events[i],"in_tracks");
      pchains[i]=filter_init_add_pchanges(event_list,inst,init_events[i],ntracks,0);
    }
  }
  // add an empty filter_map event (in case more frames are added)
  create_filter_map(mainw->rte); // we create filter_map event_t * array with ordered effects
  if (filter_map[0]!=NULL) event_list=append_filter_map_event(event_list,tc,filter_map);
  return event_list;
}


// check if palette is in the palette_list
// if not, return next best palette to use, using a heuristic method
// num_palettes is the size of the palette list
int check_weed_palette_list(int *palette_list, int num_palettes, int palette) {
  int i;
  int best_palette=WEED_PALETTE_END;

  for (i=0; i<num_palettes; i++) {
    if (palette_list[i]==palette) {
      // exact match - return it
      return palette;
    }
    // pass 1, see if we can find same or higher quality in same colorspace
    if (weed_palette_is_alpha_palette(palette)) {
      if (palette_list[i]==WEED_PALETTE_A8) best_palette=palette_list[i];
      if (palette_list[i]==WEED_PALETTE_A1&&(best_palette==WEED_PALETTE_AFLOAT||best_palette==WEED_PALETTE_END))
        best_palette=palette_list[i];
      if (palette_list[i]==WEED_PALETTE_AFLOAT&&best_palette==WEED_PALETTE_END) best_palette=palette_list[i];
    } else if (weed_palette_is_rgb_palette(palette)) {
      if (palette_list[i]==WEED_PALETTE_RGBAFLOAT&&(palette==WEED_PALETTE_RGBFLOAT||best_palette==WEED_PALETTE_END))
        best_palette=palette_list[i];
      if (palette_list[i]==WEED_PALETTE_RGBFLOAT&&best_palette==WEED_PALETTE_END) best_palette=palette_list[i];
      if (palette_list[i]==WEED_PALETTE_RGBA32||
          palette_list[i]==WEED_PALETTE_BGRA32||palette_list[i]==WEED_PALETTE_ARGB32) {
        if ((best_palette==WEED_PALETTE_END||
             best_palette==WEED_PALETTE_RGBFLOAT||best_palette==WEED_PALETTE_RGBAFLOAT)||
            weed_palette_has_alpha_channel(palette)) best_palette=palette_list[i];
      }
      if (!weed_palette_has_alpha_channel(palette)||
          (best_palette==WEED_PALETTE_END||best_palette==WEED_PALETTE_RGBFLOAT||best_palette==WEED_PALETTE_RGBAFLOAT)) {
        if (palette_list[i]==WEED_PALETTE_RGB24||palette_list[i]==WEED_PALETTE_BGR24) {
          best_palette=palette_list[i];
        }
      }
    } else {
      // yuv
      if (palette==WEED_PALETTE_YUV411&&(palette_list[i]==WEED_PALETTE_YUV422P)) best_palette=palette_list[i];
      if (palette==WEED_PALETTE_YUV411&&best_palette!=WEED_PALETTE_YUV422P&&
          (palette_list[i]==WEED_PALETTE_YUV420P||palette_list[i]==WEED_PALETTE_YVU420P)) best_palette=palette_list[i];
      if (palette==WEED_PALETTE_YUV420P&&palette_list[i]==WEED_PALETTE_YVU420P) best_palette=palette_list[i];
      if (palette==WEED_PALETTE_YVU420P&&palette_list[i]==WEED_PALETTE_YUV420P) best_palette=palette_list[i];

      if (((palette==WEED_PALETTE_YUV420P&&best_palette!=WEED_PALETTE_YVU420P)||
           (palette==WEED_PALETTE_YVU420P&&best_palette!=WEED_PALETTE_YUV420P))&&
          (palette_list[i]==WEED_PALETTE_YUV422P||palette_list[i]==WEED_PALETTE_UYVY8888||
           palette_list[i]==WEED_PALETTE_YUYV8888)) best_palette=palette_list[i];

      if ((palette==WEED_PALETTE_YUV422P||palette==WEED_PALETTE_UYVY8888||palette==WEED_PALETTE_YUYV8888)&&
          (palette_list[i]==WEED_PALETTE_YUV422P||palette_list[i]==WEED_PALETTE_UYVY8888||
           palette_list[i]==WEED_PALETTE_YUYV8888)) best_palette=palette_list[i];

      if (palette_list[i]==WEED_PALETTE_YUVA8888||palette_list[i]==WEED_PALETTE_YUVA4444P) {
        if (best_palette==WEED_PALETTE_END||weed_palette_has_alpha_channel(palette)) best_palette=palette_list[i];
      }
      if (best_palette==WEED_PALETTE_END||((best_palette==WEED_PALETTE_YUVA8888||best_palette==WEED_PALETTE_YUVA4444P)&&
                                           !weed_palette_has_alpha_channel(palette))) {
        if (palette_list[i]==WEED_PALETTE_YUV888||palette_list[i]==WEED_PALETTE_YUV444P) {
          best_palette=palette_list[i];
        }
      }
    }
  }

  // pass 2:
  // if we had to drop alpha, see if we can preserve it in the other colorspace
  for (i=0; i<num_palettes; i++) {
    if (weed_palette_has_alpha_channel(palette)&&
        (best_palette==WEED_PALETTE_END||!weed_palette_has_alpha_channel(best_palette))) {
      if (weed_palette_is_rgb_palette(palette)) {
        if (palette_list[i]==WEED_PALETTE_YUVA8888||palette_list[i]==WEED_PALETTE_YUVA4444P) best_palette=palette_list[i];
      } else {
        if (palette_list[i]==WEED_PALETTE_RGBA32||palette_list[i]==WEED_PALETTE_BGRA32||
            palette_list[i]==WEED_PALETTE_ARGB32) best_palette=palette_list[i];
      }
    }
  }

  // pass 3: no alpha; switch colorspaces, try to find same or higher quality
  if (best_palette==WEED_PALETTE_END) {
    for (i=0; i<num_palettes; i++) {
      if ((weed_palette_is_rgb_palette(palette)||best_palette==WEED_PALETTE_END)&&
          (palette_list[i]==WEED_PALETTE_RGB24||palette_list[i]==WEED_PALETTE_BGR24)) {
        best_palette=palette_list[i];
      }
      if ((weed_palette_is_yuv_palette(palette)||best_palette==WEED_PALETTE_END)&&
          (palette_list[i]==WEED_PALETTE_YUV888||palette_list[i]==WEED_PALETTE_YUV444P)) {
        best_palette=palette_list[i];
      }
      if (best_palette==WEED_PALETTE_END&&(palette_list[i]==WEED_PALETTE_RGBA32||
                                           palette_list[i]==WEED_PALETTE_BGRA32||palette_list[i]==WEED_PALETTE_ARGB32)) {
        best_palette=palette_list[i];
      }
      if (best_palette==WEED_PALETTE_END&&
          (palette_list[i]==WEED_PALETTE_YUVA8888||palette_list[i]==WEED_PALETTE_YUVA4444P)) {
        best_palette=palette_list[i];
      }
    }
  }

  // pass 4: switch to YUV, try to find highest quality
  if (best_palette==WEED_PALETTE_END) {
    for (i=0; i<num_palettes; i++) {
      if (palette_list[i]==WEED_PALETTE_UYVY8888||palette_list[i]==WEED_PALETTE_YUYV8888||
          palette_list[i]==WEED_PALETTE_YUV422P) {
        best_palette=palette_list[i];
      }
      if ((best_palette==WEED_PALETTE_END||best_palette==WEED_PALETTE_YUV411)&&
          (palette_list[i]==WEED_PALETTE_YUV420P||palette_list[i]==WEED_PALETTE_YVU420P)) {
        best_palette=palette_list[i];
      }
      if (best_palette==WEED_PALETTE_END&&palette_list[i]==WEED_PALETTE_YUV411) best_palette=palette_list[i];
    }
  }

  // pass 5: tweak results to use (probably) most common colourspaces
  for (i=0; i<num_palettes; i++) {
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
  lives_printerr("Debug: best palette for %d is %d\n",palette,best_palette);
#endif

  return best_palette;
}

static void set_channel_size(weed_plant_t *channel, int width, int height, int numplanes, int *rowstrides) {
  int error;
  int max;
  weed_plant_t *chantmpl=weed_get_plantptr_value(channel,"template",&error);
  // note: rowstrides is just a guess, we will set the actual value when we come to process the effect

  if (weed_plant_has_leaf(chantmpl,"width")&&weed_get_int_value(chantmpl,"width",&error)!=0)
    width=weed_get_int_value(chantmpl,"width",&error);
  else if (weed_plant_has_leaf(chantmpl,"host_width")) width=weed_get_int_value(chantmpl,"host_width",&error);
  if (weed_plant_has_leaf(chantmpl,"hstep")) width=step_val(width,weed_get_int_value(chantmpl,"hstep",&error));
  if (weed_plant_has_leaf(chantmpl,"maxwidth")) {
    max=weed_get_int_value(chantmpl,"maxwidth",&error);
    if (width>max) width=max;
  }

  weed_set_int_value(channel,"width",width);

  if (weed_plant_has_leaf(chantmpl,"height")&&weed_get_int_value(chantmpl,"height",&error)!=0)
    height=weed_get_int_value(chantmpl,"height",&error);
  else if (weed_plant_has_leaf(chantmpl,"host_height")) height=weed_get_int_value(chantmpl,"host_height",&error);
  if (weed_plant_has_leaf(chantmpl,"vstep")) height=step_val(height,weed_get_int_value(chantmpl,"vstep",&error));
  if (weed_plant_has_leaf(chantmpl,"maxheight")) {
    max=weed_get_int_value(chantmpl,"maxheight",&error);
    if (height>max) height=max;
  }
  weed_set_int_value(channel,"height",height);

  if (rowstrides!=NULL) weed_set_int_array(channel,"rowstrides",numplanes,rowstrides);

}


static boolean rowstrides_differ(int n1, int *n1_array, int n2, int *n2_array) {
  // returns TRUE if the rowstrides differ
  int i;

  if (n1!=n2) return TRUE;
  for (i=0; i<n1; i++) if (n1_array[i]!=n2_array[i]) return TRUE;
  return FALSE;
}


static boolean align(void **pixel_data, size_t alignment, int numplanes, int height, int *rowstrides,
                     int *contiguous) {
#ifndef HAVE_POSIX_MEMALIGN
  return FALSE;
#else

  // returns TRUE on success
  int i;
  int memerror;
  boolean needs_change=FALSE;
  uint8_t *npixel_data;
  void **new_pixel_data;
  size_t size,totsize=0;

  for (i=0; i<numplanes; i++) {
    if (((uint64_t)(pixel_data[i]))%alignment==0) continue;
    needs_change=TRUE;
  }

  if (!needs_change) return TRUE;

  for (i=0; i<numplanes; i++) {
    size=height*rowstrides[i];
    totsize+=CEIL(size,32);
  }

  // try contiguous first


  if ((memerror=posix_memalign((void **)&npixel_data,alignment,totsize))) return FALSE;

  new_pixel_data=(void **)lives_malloc(numplanes*(sizeof(void *)));

  // recheck
  needs_change=FALSE;

  for (i=0; i<numplanes; i++) {
    if (((uint64_t)(pixel_data[i]))%alignment==0) continue;
    needs_change=TRUE;
  }

  if (!needs_change) {
    for (i=0; i<numplanes; i++) {
      memcpy(npixel_data,pixel_data[i],height*rowstrides[i]);
      new_pixel_data[i]=npixel_data;
      size=height*rowstrides[i];
      npixel_data+=CEIL(size,32);
    }

    for (i=0; i<numplanes; i++) {
      if (i==0||!(*contiguous))
        lives_free(pixel_data[i]);
      pixel_data[i]=new_pixel_data[i];
    }

    lives_free(new_pixel_data);
    if (numplanes>1) *contiguous=TRUE;
    else *contiguous=FALSE;
    return TRUE;
  }

  lives_free(npixel_data);

  // non-contiguous
  for (i=0; i<numplanes; i++) {
    if ((memerror=posix_memalign((void **)&npixel_data,alignment,height*rowstrides[i]))) return FALSE;
    memcpy(npixel_data,pixel_data[i],height*rowstrides[i]);
    new_pixel_data[i]=npixel_data;
  }

  for (i=0; i<numplanes; i++) {
    if (i==0||!(*contiguous))
      lives_free(pixel_data[i]);
    pixel_data[i]=new_pixel_data[i];
  }

  lives_free(new_pixel_data);
  *contiguous=FALSE;

  return TRUE;
#endif
}


LIVES_INLINE int weed_flagset_array_count(weed_plant_t **array, boolean set_readonly) {
  int i=0;
  while (array[i]!=NULL) {
    if (set_readonly) weed_add_plant_flags(array[i],WEED_LEAF_READONLY_PLUGIN);
    i++;
  }
  return i;
}


void set_param_gui_readonly(weed_plant_t *inst) {
  int num_params,error,i;
  weed_plant_t **params,*gui,*ptmpl;

  num_params=weed_leaf_num_elements(inst,"in_parameters");
  if (num_params>0) {
    params=weed_get_plantptr_array(inst,"in_parameters",&error);
    for (i=0; i<num_params; i++) {
      ptmpl=weed_get_plantptr_value(params[i],"template",&error);
      if (weed_plant_has_leaf(ptmpl,"gui")) {
        gui=weed_get_plantptr_value(ptmpl,"gui",&error);
        weed_add_plant_flags(gui,WEED_LEAF_READONLY_PLUGIN);
      }
    }
    lives_free(params);
  }

}

void set_param_gui_readwrite(weed_plant_t *inst) {
  int num_params,error,i;
  weed_plant_t **params,*gui,*ptmpl;

  num_params=weed_leaf_num_elements(inst,"in_parameters");
  if (num_params>0) {
    params=weed_get_plantptr_array(inst,"in_parameters",&error);
    for (i=0; i<num_params; i++) {
      ptmpl=weed_get_plantptr_value(params[i],"template",&error);
      if (weed_plant_has_leaf(ptmpl,"gui")) {
        gui=weed_get_plantptr_value(ptmpl,"gui",&error);
        weed_clear_plant_flags(gui,WEED_LEAF_READONLY_PLUGIN);
      }
    }
    lives_free(params);
  }

}

/// change directory to plugin installation dir so it can find any data files
///
/// returns copy of current directory (before directory change) which should be freed after use
char *cd_to_plugin_dir(weed_plant_t *filter) {
  char *ret;
  int error;
  weed_plant_t *plugin_info=weed_get_plantptr_value(filter,"plugin_info",&error);
  char *ppath=weed_get_string_value(plugin_info,"plugin_path",&error);
  ret=lives_get_current_dir();
  // allow this to fail -it's not that important - it just means any plugin data files wont be found
  // besides, we dont want to show warnings at 50 fps
  lives_chdir(ppath,TRUE);
  lives_free(ppath);
  return ret;
}

lives_filter_error_t weed_reinit_effect(weed_plant_t *inst, boolean reinit_compound) {
  weed_plant_t *filter;

  lives_filter_error_t filter_error=FILTER_NO_ERROR;

  char *cwd;

  boolean is_audio=FALSE,deinit_first=FALSE;

  int error,retval,key=-1;

reinit:

  if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);

  filter=weed_instance_get_filter(inst,FALSE);

  if (weed_plant_has_leaf(inst,"host_inited")&&weed_get_boolean_value(inst,"host_inited",&error)==WEED_TRUE) deinit_first=TRUE;

  if (deinit_first) {
    if (is_pure_audio(filter,FALSE)) {
      filter_mutex_lock(key);
      is_audio=TRUE;
    }
    weed_call_deinit_func(inst);
  }

  if (weed_plant_has_leaf(filter,"init_func")) {
    weed_init_f *init_func_ptr_ptr;
    weed_init_f init_func;
    weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
    init_func=init_func_ptr_ptr[0];
    cwd=cd_to_plugin_dir(filter);
    if (init_func!=NULL) {
      lives_rfx_t *rfx;
      set_param_gui_readwrite(inst);
      update_host_info(inst);
      retval=(*init_func)(inst);
      if (is_audio) filter_mutex_unlock(key);
      set_param_gui_readonly(inst);
      if (fx_dialog[1]!=NULL) {
        // redraw GUI if necessary
        rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"rfx");
        if (rfx->source_type==LIVES_RFX_SOURCE_WEED&&rfx->source==inst) {
          int keyw=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"key"));
          int modew=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"mode"));

          // do updates from "gui"
          rfx_params_free(rfx);
          lives_free(rfx->params);

          rfx->params=weed_params_to_rfx(rfx->num_params,inst,FALSE);

          redraw_pwindow(keyw,modew);
        }
      }

      if (retval!=WEED_NO_ERROR) {
        lives_chdir(cwd,FALSE);
        lives_free(cwd);
        return FILTER_ERROR_COULD_NOT_REINIT;
      }

      // need to set this before calling deinit
      weed_set_boolean_value(inst,"host_inited",WEED_TRUE);
      // redraw set defs window

    } else if (is_audio) filter_mutex_unlock(key);

    if (!deinit_first) {
      if (is_audio) filter_mutex_lock(key);
      weed_call_deinit_func(inst);
      if (is_audio) filter_mutex_unlock(key);
    }

    lives_chdir(cwd,FALSE);
    lives_free(cwd);
    filter_error=FILTER_INFO_REINITED;
  } else if (is_audio) filter_mutex_unlock(key);


  if (deinit_first) weed_set_boolean_value(inst,"host_inited",WEED_TRUE);
  else weed_set_boolean_value(inst,"host_inited",WEED_FALSE);

  if (reinit_compound&&weed_plant_has_leaf(inst,"host_next_instance")) {
    // handle compound fx
    inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
    goto reinit;
  }

  return filter_error;
}

void weed_reinit_all(void) {
  // reinit all effects on playback start
  weed_plant_t *instance,*last_inst;
  int error;
  register int i;

  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i+1,TRUE)) {
      if (mainw->rte&(GU641<<i)) {
        mainw->osc_block=TRUE;
        if ((instance=key_to_instance[i][key_modes[i]])==NULL) continue;
        last_inst=instance;

        // ignore video generators
        while (weed_plant_has_leaf(last_inst,"host_next_instance")) last_inst=weed_get_plantptr_value(last_inst,"host_next_instance",&error);
        if (enabled_in_channels(instance,FALSE)==0&&enabled_out_channels(last_inst,FALSE)>0&&!is_pure_audio(last_inst,FALSE)) continue;
        weed_reinit_effect(instance,TRUE);
      }
    }
  }
  mainw->osc_block=FALSE;
}


static void *thread_process_func(void *arg) {
  struct _procvals *procvals=(struct _procvals *)arg;
  weed_process_f *process_func_ptr_ptr;
  weed_process_f process_func;

  weed_plant_t *inst=procvals->inst;
  weed_timecode_t tc=procvals->tc;

  weed_plant_t *filter=weed_instance_get_filter(inst,FALSE);

  weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
  process_func=process_func_ptr_ptr[0];

  procvals->ret = (*process_func)(inst,tc);

  return NULL;
}


static lives_filter_error_t process_func_threaded(weed_plant_t *inst, weed_plant_t **out_channels, weed_timecode_t tc) {
  // split output(s) into horizontal slices
  int offset=0;
  int dheight,height;
  int nthreads=0;
  int error;
  boolean got_invalid=FALSE;

  int nchannels=weed_leaf_num_elements(inst,"out_channels"),pal,vrt;
  int retval;
  int minh,xminh;

  int slices,slices_per_thread,to_use;

  struct _procvals *procvals;
  pthread_t *dthreads=NULL;
  weed_plant_t **xinst=NULL;

  weed_plant_t **xchannels;
  weed_plant_t *ctmpl;

  register int i,j;

  height=weed_get_int_value(out_channels[0],"height",&error);
  xminh=1;

  for (i=0; i<nchannels; i++) {
    // min height for slices is 1, unless an out channel has vstep set
    // or using a compressed yuv palette
    ctmpl=weed_get_plantptr_value(out_channels[i],"template",&error);

    pal=weed_get_int_value(out_channels[i],"current_palette",&error);
    vrt=weed_palette_get_plane_ratio_vertical(pal,1);
    if (vrt!=0.&&vrt<1.) if (xminh<(int)(1./vrt)) xminh=(int)(1./vrt);

    if (weed_plant_has_leaf(ctmpl,"vstep")) {
      minh=weed_get_int_value(ctmpl,"vstep",&error);
      if (minh>xminh) xminh=minh;
    }
  }

  if (height%xminh!=0) return FILTER_ERROR_DONT_THREAD;

  slices=height/xminh;
  slices_per_thread=CEIL((double)slices/(double)prefs->nfx_threads,1.);

  to_use=CEIL((double)slices/(double)slices_per_thread,1.);
  if (to_use<2) return FILTER_ERROR_DONT_THREAD;

  dheight=slices_per_thread*xminh;

  for (i=0; i<nchannels; i++) {
    weed_set_int_value(out_channels[i],"offset",0);
    weed_set_int_value(out_channels[i],"host_height",height);
    weed_set_int_value(out_channels[i],"height",dheight);
  }

  procvals=(struct _procvals *)lives_malloc(sizeof(struct _procvals)*to_use);
  if (to_use>1) {
    xinst=(weed_plant_t **)lives_malloc(sizeof(weed_plant_t *)*(to_use-1));
    dthreads=(pthread_t *)calloc(sizeof(pthread_t)*(to_use-1),1);
  }

  for (j=1; j<to_use; j++) {
    // each thread needs its own copy of the output channels, so it can have its own "offset" and "height"
    // therefore it also needs its own copy of inst
    // but note that "pixel_data" always points to the same memory buffer(s)

    xinst[j-1]=weed_plant_copy(inst);
    xchannels=(weed_plant_t **)lives_malloc(nchannels*sizeof(weed_plant_t *));

    for (i=0; i<nchannels; i++) {
      xchannels[i]=weed_plant_copy(out_channels[i]);
      ctmpl=weed_get_plantptr_value(out_channels[i],"template",&error);

      offset=dheight*j;

      if ((height-offset)<dheight) {
        dheight=height-offset;
      }

      weed_set_int_value(xchannels[i],"offset",offset);
      weed_set_int_value(xchannels[i],"height",dheight);
    }

    weed_set_plantptr_array(xinst[j-1],"out_channels",nchannels,xchannels);
    lives_free(xchannels);

    procvals[j].inst=xinst[j-1];
    procvals[j].tc=tc; // use same timecode for all slices

    // start a thread for processing
    pthread_create(&dthreads[j-1],NULL,thread_process_func,&procvals[j]);
    nthreads++; // actual number of threads used
  }


  procvals[0].inst=inst;
  procvals[0].tc=tc;

  // use main thread for first slices
  thread_process_func(&procvals[0]);
  retval=procvals[0].ret;

  if (retval==WEED_ERROR_PLUGIN_INVALID) got_invalid=TRUE;

  for (i=0; i<nchannels; i++) {
    weed_leaf_delete(out_channels[i],"offset");
    weed_set_int_value(out_channels[i],"height",weed_get_int_value(out_channels[i],"host_height",&error));
    weed_leaf_delete(out_channels[i],"host_height");
  }

  // wait for threads to finish
  for (j=0; j<nthreads; j++) {
    retval=WEED_NO_ERROR;

    pthread_join(dthreads[j],NULL);
    retval=procvals[j].ret;

    xchannels=weed_get_plantptr_array(xinst[j],"out_channels",&error);
    for (i=0; i<nchannels; i++) {
      weed_plant_free(xchannels[i]);
    }
    lives_free(xchannels);
    weed_plant_free(xinst[j]);

    if (retval==WEED_ERROR_PLUGIN_INVALID) got_invalid=TRUE;
  }

  lives_free(procvals);
  if (xinst!=NULL) lives_free(xinst);
  if (dthreads!=NULL) lives_free(dthreads);

  if (got_invalid) return FILTER_ERROR_MUST_RELOAD;

  return FILTER_NO_ERROR;
}



lives_filter_error_t weed_apply_instance(weed_plant_t *inst, weed_plant_t *init_event, weed_plant_t **layers,
    int opwidth, int opheight, weed_timecode_t tc) {
  // here we:
  // get our in_tracks and out_tracks that map filter_instance channels to layers

  // clear "disabled" if we have non-zero frame and there is no "disabled" in template
  // if we have a zero frame, set "disabled" if "optional", otherwise we cannot apply the filter

  // set channel timecodes

  // pull pixel_data (unless it is there already)

  // set each channel width,height to match largest of in layers

  // if width and height are wrong, resize in the layer

  // if palette is wrong, first we try to change the plugin channel palette,
  // if not possible we convert palette in the layer

  // apply the effect, put result in output layer, set layer palette, width, height, rowstrides

  // if filter does not support inplace, we must create a new pixel_data; this will then replace the original layer


  // for in/out alpha channels, there is no matching layer. These channels are passed around like data
  // using mainw->cconx as a guide. We will simply free any in alpha channels after processing (unless inplace was used)



  // WARNING: output layer may need resizing, and its palette may need adjusting - should be checked by the caller

  // opwidth and opheight limit the maximum frame size, they can either be set to 0,0 or to max display size;
  // however if all sources are smaller than this
  // then the output will be smaller also and need resizing by the caller

  // TODO ** - handle return errors
  int *in_tracks,*out_tracks;
  int *rowstrides;
  int *layer_rows=NULL,*channel_rows;
  int *mand;

  void **pixel_data;

  void *pdata;

  weed_process_f *process_func_ptr_ptr;
  weed_process_f process_func;
  weed_plant_t *def_channel=NULL;

  weed_plant_t *filter=weed_instance_get_filter(inst,FALSE);
  weed_plant_t *layer=NULL,*orig_layer=NULL;

  weed_plant_t **in_channels=NULL,**out_channels=NULL,*channel,*chantmpl;
  weed_plant_t **in_ctmpls;

  lives_filter_error_t retval=FILTER_NO_ERROR;

  boolean rowstrides_changed;
  boolean ignore_palette;
  boolean did_thread=FALSE;
  boolean needs_reinit=FALSE,inplace=FALSE;
  boolean def_disabled=FALSE;
  boolean all_outs_alpha=TRUE;//,all_ins_alpha=FALSE;

  int num_in_tracks,num_out_tracks;
  int error;
  int frame;
  int inwidth,inheight,inpalette,outpalette,channel_flags,filter_flags=0;
  int palette,cpalette;
  int outwidth,outheight;
  int incwidth,incheight,numplanes=0,width,height;
  int nchr;
  int maxinwidth=4,maxinheight=4;
  int iclamping,isampling,isubspace;
  int clip;
  int num_ctmpl,num_inc,num_outc;
  int osubspace=-1;
  int osampling=-1;
  int oclamping=-1;
  int flags;
  int num_in_alpha=0,num_out_alpha=0;
  int nmandout=0;
  int lcount=0;
  int key=-1;

  register int i,j,k;

  if ((!weed_plant_has_leaf(inst,"out_channels")||(out_channels=weed_get_plantptr_array(inst,"out_channels",&error))==NULL)
      &&(mainw->preview||mainw->is_rendering)&&(num_compound_fx(inst)==1)) {
    if (out_channels!=NULL) lives_free(out_channels);
    return retval;
  }

  if (weed_plant_has_leaf(filter,"flags")) filter_flags=weed_get_int_value(filter,"flags",&error);

  if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);

  // here, in_tracks and out_tracks map our layers to in_channels and out_channels in the filter
  if (!has_video_chans_in(filter,TRUE)||!weed_plant_has_leaf(inst,"in_channels")||
      (in_channels=weed_get_plantptr_array(inst,"in_channels",&error))==NULL) {

    if (out_channels==NULL&&weed_plant_has_leaf(inst,"out_parameters")) {

      // TODO - this is more complex, as we have to check the entire chain of fx

      // run it only if it outputs into effects which have video chans
      //if (!feeds_to_video_filters(key,rte_key_getmode(key+1))) return FILTER_ERROR_NO_IN_CHANNELS;

      // data processing effect; just call the process_func
      weed_set_double_value(inst,"fps",cfile->pb_fps);

      // see if we can multithread
      if ((prefs->nfx_threads=future_prefs->nfx_threads)>1 &&
          filter_flags&WEED_FILTER_HINT_MAY_THREAD) {
        filter_mutex_lock(key);
        retval=process_func_threaded(inst,out_channels,tc);
        filter_mutex_unlock(key);
        if (retval!=FILTER_ERROR_DONT_THREAD) did_thread=TRUE;
      }
      if (!did_thread) {
        // normal single threaded version
        int ret;
        weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
        process_func=process_func_ptr_ptr[0];
        filter_mutex_lock(key);
        ret=(*process_func)(inst,tc);
        filter_mutex_unlock(key);
        if (ret==WEED_ERROR_PLUGIN_INVALID) retval=FILTER_ERROR_MUST_RELOAD;
      }
      return retval;
    }
    if (out_channels!=NULL) lives_free(out_channels);
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (weed_plant_has_leaf(filter,"rowstride_alignment_hint")) {
    int rowstride_alignment_hint=weed_get_int_value(filter,"rowstride_alignment_hint",&error);
    if ((rowstride_alignment_hint==16||
         rowstride_alignment_hint==8||
         rowstride_alignment_hint==4||
         rowstride_alignment_hint==2)
        &&rowstride_alignment_hint>mainw->rowstride_alignment_hint)
      mainw->rowstride_alignment_hint=rowstride_alignment_hint;
  }

  if (get_enabled_channel(inst,0,TRUE)==NULL) {
    // we process generators elsewhere
    lives_free(in_channels);
    if (out_channels!=NULL) lives_free(out_channels);
    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (is_pure_audio(filter,TRUE)) {
    lives_free(in_channels);
    if (out_channels!=NULL) lives_free(out_channels);
    return FILTER_ERROR_IS_AUDIO; // we process audio effects elsewhere
  }

  if (init_event==NULL) {
    num_in_tracks=enabled_in_channels(inst,FALSE);
    in_tracks=(int *)lives_malloc(2*sizint);
    in_tracks[0]=0;
    in_tracks[1]=1;
    num_out_tracks=enabled_out_channels(inst,FALSE);
    out_tracks=(int *)lives_malloc(sizint);
    out_tracks[0]=0;
  } else {
    num_in_tracks=weed_leaf_num_elements(init_event,"in_tracks");
    in_tracks=weed_get_int_array(init_event,"in_tracks",&error);
    num_out_tracks=weed_leaf_num_elements(init_event,"out_tracks");
    out_tracks=weed_get_int_array(init_event,"out_tracks",&error);
  }


  // handle case where in_tracks[i] > than num layers
  // either we temporarily disable the channel, or we can't apply the filter
  num_inc=weed_leaf_num_elements(inst,"in_channels");

  for (i=0; i<num_inc; i++) {
    if (weed_palette_is_alpha_palette(weed_get_int_value(in_channels[i],"current_palette",&error))&&
        !(weed_plant_has_leaf(in_channels[i],"disabled") &&
          weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_TRUE))
      num_in_alpha++;
  }

  //  if (num_inc==num_in_alpha) all_ins_alpha=TRUE;

  num_inc-=num_in_alpha;

  if (num_in_tracks>num_inc) num_in_tracks=num_inc; // for example, compound fx

  if (num_inc>num_in_tracks) {
    for (i=num_in_tracks; i<num_inc+num_in_alpha; i++) {
      if (!weed_palette_is_alpha_palette(weed_get_int_value(in_channels[i],"current_palette",&error))) {
        if (!weed_plant_has_leaf(in_channels[i],"disabled")||
            weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_FALSE)
          weed_set_boolean_value(in_channels[i],"host_temp_disabled",WEED_TRUE);
        else weed_set_boolean_value(in_channels[i],"host_temp_disabled",WEED_FALSE);
      }
    }
  }

  while (layers[lcount++]!=NULL);

  for (k=i=0; i<num_in_tracks; i++) {

    if (in_tracks[i]<0) {
      lives_free(in_tracks);
      lives_free(out_tracks);
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      return FILTER_ERROR_INVALID_TRACK; // probably audio
    }

    while (weed_palette_is_alpha_palette(weed_get_int_value(in_channels[k],"current_palette",&error))) k++;

    channel=in_channels[k];
    weed_set_boolean_value(channel,"host_temp_disabled",WEED_FALSE);

    if (in_tracks[i]>=lcount) {
      for (j=k; j<num_in_tracks+num_in_alpha; j++) {
        if (weed_palette_is_alpha_palette(weed_get_int_value(in_channels[j],"current_palette",&error))) continue;
        channel=in_channels[j];
        chantmpl=weed_get_plantptr_value(channel,"template",&error);
        if (weed_plant_has_leaf(chantmpl,"max_repeats")||(weed_plant_has_leaf(chantmpl,"option")&&
            weed_get_boolean_value(chantmpl,"optional",&error)==WEED_TRUE))
          weed_set_boolean_value(channel,"host_temp_disabled",WEED_TRUE);
        else {
          lives_free(in_tracks);
          lives_free(out_tracks);
          lives_free(in_channels);
          if (out_channels!=NULL) lives_free(out_channels);
          return FILTER_ERROR_MISSING_LAYER;
        }
      }
      break;
    }
    layer=layers[in_tracks[i]];

    check_layer_ready(layer);

    if (weed_get_voidptr_value(layer,"pixel_data",&error)==NULL) {
      frame=weed_get_int_value(layer,"frame",&error);
      if (frame==0) {
        // temp disable channels if we can
        channel=in_channels[k];
        chantmpl=weed_get_plantptr_value(channel,"template",&error);
        if (weed_plant_has_leaf(chantmpl,"max_repeats")||(weed_plant_has_leaf(chantmpl,"option")&&
            weed_get_boolean_value(chantmpl,"optional",&error)==WEED_TRUE)) {
          weed_set_boolean_value(channel,"host_temp_disabled",WEED_TRUE);
        } else {
          lives_free(in_tracks);
          lives_free(out_tracks);
          lives_free(in_channels);
          if (out_channels!=NULL) lives_free(out_channels);
          return FILTER_ERROR_BLANK_FRAME;
        }
      }
    }
    k++;
  }


  // ensure all chantmpls not marked "optional" have at least one corresponding enabled channel
  // e.g. we could have disabled all channels from a template with "max_repeats" that is not "optional"
  num_ctmpl=weed_leaf_num_elements(filter,"in_channel_templates");
  mand=(int *)lives_malloc(num_ctmpl*sizint);
  for (j=0; j<num_ctmpl; j++) mand[j]=0;
  in_ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);

  for (i=0; i<num_inc+num_in_alpha; i++) {
    if ((weed_plant_has_leaf(in_channels[i],"disabled")&&
         weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_TRUE)||
        (weed_plant_has_leaf(in_channels[i],"host_temp_disabled")&&
         weed_get_boolean_value(in_channels[i],"host_temp_disabled",&error)==WEED_TRUE)) continue;
    chantmpl=weed_get_plantptr_value(in_channels[i],"template",&error);
    for (j=0; j<num_ctmpl; j++) {
      if (chantmpl==in_ctmpls[j]) {
        mand[j]=1;
        break;
      }
    }
  }

  for (j=0; j<num_ctmpl; j++) {
    if (mand[j]==0&&(!weed_plant_has_leaf(in_ctmpls[j],"optional")||
                     weed_get_boolean_value(in_ctmpls[j],"optional",&error)==WEED_FALSE)) {
      lives_free(in_ctmpls);
      lives_free(in_tracks);
      lives_free(out_tracks);
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      lives_free(mand);
      return FILTER_ERROR_MISSING_LAYER;
    }
  }

  lives_free(in_ctmpls);
  lives_free(mand);


  num_outc=weed_leaf_num_elements(inst,"out_channels");

  for (i=0; i<num_outc; i++) {
    if (weed_palette_is_alpha_palette(weed_get_int_value(out_channels[i],"current_palette",&error))) {
      if (!(weed_plant_has_leaf(out_channels[i],"disabled") &&
            weed_get_boolean_value(out_channels[i],"disabled",&error)==WEED_TRUE))
        num_out_alpha++;
    } else {
      if ((!weed_plant_has_leaf(out_channels[i],"disabled")||weed_get_boolean_value(out_channels[i],"disabled",&error)==WEED_FALSE)&&
          (!weed_plant_has_leaf(out_channels[i],"disabled")||weed_get_boolean_value(out_channels[i],"disabled",&error)==WEED_FALSE)) {
        nmandout++;
      }
    }
  }

  if (init_event==NULL||num_compound_fx(inst)>1) num_out_tracks-=num_out_alpha;

  if (num_out_tracks<0) num_out_tracks=0;

  if (nmandout>num_out_tracks) {
    // occasionally during recording we get an init_event with no "out_tracks" (probably when an audio effect inits/deinits a video effect)
    // needs more investigation
    lives_free(in_tracks);
    lives_free(out_tracks);
    lives_free(in_channels);
    if (out_channels!=NULL) lives_free(out_channels);
    return FILTER_ERROR_MISSING_CHANNEL;
  }


  // pull frames for tracks

  for (i=0; i<num_out_tracks+num_out_alpha; i++) {
    if (i>=num_outc) continue; // for compound filters, num_out_tracks may not be valid
    channel=out_channels[i];
    palette=weed_get_int_value(channel,"current_palette",&error);
    if (weed_palette_is_alpha_palette(palette)) continue;
    if (weed_plant_has_leaf(channel,"host_temp_disabled")&&
        weed_get_boolean_value(channel,"host_temp_disabled",&error)==WEED_TRUE) continue;
    all_outs_alpha=FALSE;
  }


  for (j=i=0; i<num_in_tracks; i++) {
    while (weed_palette_is_alpha_palette(weed_get_int_value(in_channels[j],"current_palette",&error))) j++;

    if (weed_plant_has_leaf(in_channels[j],"host_temp_disabled")&&
        weed_get_boolean_value(in_channels[j],"host_temp_disabled",&error)==WEED_TRUE) {
      j++;
      continue;
    }
    layer=layers[in_tracks[i]];
    clip=weed_get_int_value(layer,"clip",&error);

    if (!weed_plant_has_leaf(layer,"pixel_data")||weed_get_voidptr_value(layer,"pixel_data",&error)==NULL) {
      // pull_frame will set pixel_data,width,height,current_palette and rowstrides
      if (!pull_frame(layer,get_image_ext_for_type(mainw->files[clip]->img_type),tc)) {
        lives_free(in_tracks);
        lives_free(out_tracks);
        lives_free(in_channels);
        if (out_channels!=NULL) lives_free(out_channels);
        return FILTER_ERROR_MISSING_FRAME;
      }
    }
    // we apply only transitions and compositors to the scrap file
    if (clip==mainw->scrap_file&&num_in_tracks==1&&num_out_tracks==1) {
      lives_free(in_tracks);
      lives_free(out_tracks);
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      return FILTER_ERROR_IS_SCRAP_FILE;
    }
    // use comparative widths - in RGB(A) pixels
    palette=weed_get_int_value(layer,"current_palette",&error);
    if ((inwidth=(weed_get_int_value(layer,"width",&error)*weed_palette_get_pixels_per_macropixel(palette)))>maxinwidth)
      maxinwidth=inwidth;
    if ((inheight=weed_get_int_value(layer,"height",&error))>maxinheight) maxinheight=inheight;
    j++;
  }

  // pixels
  if (maxinwidth<opwidth||opwidth==0) opwidth=maxinwidth;
  if (maxinheight<opheight||opheight==0) opheight=maxinheight;

  // first we resize if necessary; then we change the palette

  for (k=i=0; k<num_inc+num_in_alpha; k++) {

    channel=get_enabled_channel(inst,k,TRUE);
    if (channel==NULL) break;

    if (weed_plant_has_leaf(channel,"host_temp_disabled")&&
        weed_get_boolean_value(channel,"host_temp_disabled",&error)==WEED_TRUE) continue;

    chantmpl=weed_get_plantptr_value(channel,"template",&error);

    if (def_channel==NULL) def_channel=channel;

    if (weed_palette_is_alpha_palette(weed_get_int_value(channel,"current_palette",&error))) {
      if (def_channel==channel) continue;
      palette=weed_get_int_value(channel,"current_palette",&error);
    } else {
      layer=layers[in_tracks[i]];
      palette=weed_get_int_value(layer,"current_palette",&error);
    }

    // values in pixels
    width=opwidth;
    height=opheight;

    channel_flags=0;
    if (weed_plant_has_leaf(chantmpl,"flags")) channel_flags=weed_get_int_value(chantmpl,"flags",&error);

    // (channel macropixels)
    incwidth=weed_get_int_value(channel,"width",&error);
    incheight=weed_get_int_value(channel,"height",&error);

    if (weed_palette_is_alpha_palette(weed_get_int_value(channel,"current_palette",&error))) {
      inwidth=weed_get_int_value(def_channel,"width",&error);
      inheight=weed_get_int_value(def_channel,"height",&error);
    } else {
      // (layer macropixels)
      inwidth=weed_get_int_value(layer,"width",&error);
      inheight=weed_get_int_value(layer,"height",&error);
    }

    if (channel_flags&WEED_CHANNEL_SIZE_CAN_VARY) {
      width=inwidth*weed_palette_get_pixels_per_macropixel(palette); // convert inwidth to pixels
      height=inheight;
    }

    cpalette=weed_get_int_value(channel,"current_palette",&error);
    width/=weed_palette_get_pixels_per_macropixel(cpalette); // convert width to (channel) macropixels

    if (weed_plant_has_leaf(channel,"YUV_clamping")) iclamping=(weed_get_int_value(channel,"YUV_clamping",&error));
    else iclamping=0;

    // try to set our target width height - the channel may have restrictions
    set_channel_size(channel,width,height,0,NULL);

    if (weed_palette_is_alpha_palette(cpalette)) continue;

    width=weed_get_int_value(channel,"width",&error)*weed_palette_get_pixels_per_macropixel(cpalette)/
          weed_palette_get_pixels_per_macropixel(palette);
    height=weed_get_int_value(channel,"height",&error);

    // restore channel to original size for now

    set_channel_size(channel,incwidth,incheight,0,NULL);

    palette=weed_get_int_value(layer,"current_palette",&error);
    inpalette=cpalette;

    // check if we need to resize
    if ((inwidth!=width)||(inheight!=height)) {
      // layer needs resizing

      if (prefs->pb_quality==PB_QUALITY_HIGH||opwidth==0||opheight==0) {
        if (!resize_layer(layer,width,height,LIVES_INTERP_BEST,cpalette,iclamping)) {
          lives_free(in_tracks);
          lives_free(out_tracks);
          lives_free(in_channels);
          if (out_channels!=NULL) lives_free(out_channels);
          return FILTER_ERROR_UNABLE_TO_RESIZE;
        }
      } else {
        if (!resize_layer(layer,width,height,get_interp_value(prefs->pb_quality),cpalette,iclamping)) {
          lives_free(in_tracks);
          lives_free(out_tracks);
          lives_free(in_channels);
          if (out_channels!=NULL) lives_free(out_channels);
          return FILTER_ERROR_UNABLE_TO_RESIZE;
        }
      }
      // check palette again in case it changed during resize
      inpalette=weed_get_int_value(channel,"current_palette",&error);

      inwidth=weed_get_int_value(layer,"width",&error)*weed_palette_get_pixels_per_macropixel(inpalette)/
              weed_palette_get_pixels_per_macropixel(palette);

      inheight=weed_get_int_value(layer,"height",&error);

      if (0&&((inwidth!=width)||(inheight!=height))) {
        lives_free(in_tracks);
        lives_free(out_tracks);
        lives_free(in_channels);
        if (out_channels!=NULL) lives_free(out_channels);
        return FILTER_ERROR_UNABLE_TO_RESIZE;
      }
    }

    i++;


    // try to match palettes with first enabled in channel:
    // TODO ** - we should see which palette causes the least palette conversions
    if (i>0&&!(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY))
      palette=weed_get_int_value(def_channel,"current_palette",&error);

    if (palette!=inpalette) {
      // palette change needed; first try to change channel palette
      int num_palettes=weed_leaf_num_elements(chantmpl,"palette_list");
      int *palettes=weed_get_int_array(chantmpl,"palette_list",&error);
      if ((palette=check_weed_palette_list(palettes,num_palettes,palette))!=inpalette) {
        weed_set_int_value(channel,"current_palette",palette);
        if (channel_flags&WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE) needs_reinit=TRUE;
        weed_set_int_value(channel,"width",incwidth/
                           weed_palette_get_pixels_per_macropixel(palette)*
                           weed_palette_get_pixels_per_macropixel(inpalette));
        nchr=weed_leaf_num_elements(channel,"rowstrides");
        channel_rows=weed_get_int_array(channel,"rowstrides",&error);
        for (j=0; j<nchr; j++) {
          if (weed_palette_get_plane_ratio_horizontal(inpalette,j)!=0.)
            channel_rows[j]*=weed_palette_get_plane_ratio_horizontal(palette,j)/
                             weed_palette_get_plane_ratio_horizontal(inpalette,j);
        }
        weed_set_int_array(channel,"rowstrides",nchr,channel_rows);
        lives_free(channel_rows);
      }
      lives_free(palettes);

      if (weed_palette_is_yuv_palette(palette)) {
        if (!(weed_plant_has_leaf(chantmpl,"YUV_subspace"))||
            weed_get_int_value(chantmpl,"YUV_subspace",&error)==WEED_YUV_SUBSPACE_YUV) {
          // set to default for LiVES
          weed_set_int_value(channel,"YUV_subspace",WEED_YUV_SUBSPACE_YCBCR);
        } else {
          weed_set_int_value(channel,"YUV_subspace",weed_get_int_value(chantmpl,"YUV_subspace",&error));
        }
      }
    }
  }

  // we stored original key/mode to use here
  if (weed_plant_has_leaf(inst,"host_key")) {
    // pull from alpha chain
    int key=weed_get_int_value(inst,"host_key",&error),mode;
    if (weed_plant_has_leaf(inst,"host_mode")) {
      mode=weed_get_int_value(inst,"host_mode",&error);
    } else mode=key_modes[key];

    // need to do this AFTER setting in-channel size
    if (mainw->cconx!=NULL) {
      // chain any alpha channels
      if (cconx_chain_data(key,mode)) needs_reinit=TRUE;
    }
  }

  // make sure we have pixel_data for all mandatory in alpha channels (from alpha chains)
  // if not, if the ctmpl is optnl mark as host_temp_disabled; else return with error

  for (i=0; i<num_inc+num_in_alpha; i++) {
    if (!weed_palette_is_alpha_palette(weed_get_int_value(in_channels[i],"current_palette",&error))) continue;

    if (weed_plant_has_leaf(in_channels[i],"host_internal_connection")) {
      if (cconx_chain_data_internal(in_channels[i])) needs_reinit=TRUE;
    }

    if (weed_get_voidptr_value(in_channels[i],"pixel_data",&error)==NULL) {
      chantmpl=weed_get_plantptr_value(in_channels[i],"template",&error);
      if (weed_plant_has_leaf(chantmpl,"max_repeats")||(weed_plant_has_leaf(chantmpl,"option")&&
          weed_get_boolean_value(chantmpl,"optional",&error)==WEED_TRUE))
        weed_set_boolean_value(in_channels[i],"host_temp_disabled",WEED_TRUE);
      else {
        lives_free(in_tracks);
        lives_free(out_tracks);
        lives_free(in_channels);
        if (out_channels!=NULL) lives_free(out_channels);
        return FILTER_ERROR_MISSING_CHANNEL;
      }
    }
  }


  // now we do a second pass, and we change the palettes of in layers to match the channel, if necessary


  for (j=i=0; i<num_in_tracks; i++) {


    do {
      channel=get_enabled_channel(inst,i,TRUE);
      chantmpl=weed_get_plantptr_value(channel,"template",&error);

      inpalette=weed_get_int_value(channel,"current_palette",&error);

      channel_flags=0;
      if (weed_plant_has_leaf(chantmpl,"flags")) channel_flags=weed_get_int_value(chantmpl,"flags",&error);

      if (weed_palette_is_alpha_palette(inpalette)) {
        if (!(channel_flags&WEED_CHANNEL_SIZE_CAN_VARY)) {
          width=weed_get_int_value(channel,"width",&error);
          height=weed_get_int_value(channel,"height",&error);
          if (width != opwidth || height != opheight) {
            if (!resize_layer(channel,opwidth,opheight,LIVES_INTERP_BEST,WEED_PALETTE_END,0)) {
              lives_free(in_tracks);
              lives_free(out_tracks);
              lives_free(in_channels);
              if (out_channels!=NULL) lives_free(out_channels);
              return FILTER_ERROR_UNABLE_TO_RESIZE;
            }
          }
        }
      }
    } while (weed_palette_is_alpha_palette(weed_get_int_value(channel,"current_palette",&error)));


    if (weed_plant_has_leaf(in_channels[i],"host_temp_disabled")&&
        weed_get_boolean_value(in_channels[i],"host_temp_disabled",&error)==WEED_TRUE) continue;

    layer=layers[in_tracks[i]];

    if (weed_plant_has_leaf(layer,"YUV_clamping")) iclamping=(weed_get_int_value(layer,"YUV_clamping",&error));
    else iclamping=WEED_YUV_CLAMPING_CLAMPED;

    if (oclamping==-1||(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY)) {
      if (weed_plant_has_leaf(chantmpl,"YUV_clamping")) oclamping=(weed_get_int_value(chantmpl,"YUV_clamping",&error));
      else oclamping=iclamping;
    }

    if (weed_plant_has_leaf(layer,"YUV_sampling")) isampling=(weed_get_int_value(layer,"YUV_sampling",&error));
    else isampling=WEED_YUV_SAMPLING_DEFAULT;

    if (osampling==-1||(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY)) {
      /*   if (weed_plant_has_leaf(chantmpl,"YUV_sampling")) osampling=(weed_get_int_value(layer,"YUV_sampling",&error));
      else */

      // cant convert sampling yet
      osampling=isampling;
    }


    if (weed_plant_has_leaf(layer,"YUV_subspace")) isubspace=(weed_get_int_value(layer,"YUV_subspace",&error));
    else isubspace=WEED_YUV_SUBSPACE_YCBCR;

    if (osubspace==-1||(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY)) {
      /*if (weed_plant_has_leaf(chantmpl,"YUV_subspace")) osubspace=(weed_get_int_value(chantmpl,"YUV_subspace",&error));
      else */

      // cant convert subspace yet
      osubspace=isubspace;
    }

    cpalette=weed_get_int_value(layer,"current_palette",&error);

    if (weed_palette_is_rgb_palette(cpalette)&&weed_palette_is_rgb_palette(inpalette)) {
      oclamping=iclamping;
      osubspace=isubspace;
      osampling=isampling;
    }



    if (cpalette!=inpalette||isubspace!=osubspace) {

      if (all_outs_alpha&&(weed_palette_is_lower_quality(inpalette,cpalette)||
                           (weed_palette_is_rgb_palette(inpalette)&&
                            !weed_palette_is_rgb_palette(cpalette))||
                           (weed_palette_is_rgb_palette(cpalette)&&
                            !weed_palette_is_rgb_palette(inpalette)))) {
        // for an analyser (no out channels) we copy the layer if it needs lower quality
        orig_layer=layer;
        layer=weed_layer_copy(NULL,orig_layer);
      }

      if (!convert_layer_palette_full(layer,inpalette,
                                      osampling,oclamping,osubspace)) {
        lives_free(in_tracks);
        lives_free(out_tracks);
        lives_free(in_channels);
        if (out_channels!=NULL) lives_free(out_channels);
        if (orig_layer!=NULL) {
          weed_layer_free(layer);
        }
        return FILTER_ERROR_INVALID_PALETTE_CONVERSION;
      }
    }

    if (weed_palette_is_yuv_palette(inpalette)) {
      if (weed_plant_has_leaf(layer,"YUV_clamping"))
        oclamping=(weed_get_int_value(layer,"YUV_clamping",&error));

      if (weed_plant_has_leaf(layer,"YUV_sampling"))
        osampling=(weed_get_int_value(layer,"YUV_sampling",&error));

      if (weed_plant_has_leaf(layer,"YUV_clamping"))
        osubspace=(weed_get_int_value(layer,"YUV_subspace",&error));

      weed_set_int_value(channel,"YUV_clamping",oclamping);
      weed_set_int_value(channel,"YUV_sampling",osampling);
      weed_set_int_value(channel,"YUV_subspace",osubspace);
    } else {
      weed_leaf_delete(channel,"YUV_clamping");
      weed_leaf_delete(channel,"YUV_sampling");
      weed_leaf_delete(channel,"YUV_subspace");
    }

    incwidth=weed_get_int_value(channel,"width",&error);
    incheight=weed_get_int_value(channel,"height",&error);

    nchr=weed_leaf_num_elements(channel,"rowstrides");
    channel_rows=weed_get_int_array(channel,"rowstrides",&error);

    if (weed_plant_has_leaf(layer,"flags")) flags=weed_get_int_value(layer,"flags",&error);
    else flags=0;
    if (flags!=0) weed_set_int_value(channel,"flags",flags);


    // after all resizing and palette conversions, we set the width, height and rowstrides with their final values

    width=weed_get_int_value(layer,"width",&error);
    height=weed_get_int_value(layer,"height",&error);

    numplanes=weed_leaf_num_elements(layer,"rowstrides");
    rowstrides=weed_get_int_array(layer,"rowstrides",&error);

    set_channel_size(channel,width,height,numplanes,rowstrides);

    // check layer rowstrides against previous settings

    rowstrides_changed=rowstrides_differ(numplanes,rowstrides,nchr,channel_rows);
    lives_free(channel_rows);

    if (((rowstrides_changed&&(channel_flags&WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE))||
         (((incwidth!=width)||(incheight!=height))&&(channel_flags&WEED_CHANNEL_REINIT_ON_SIZE_CHANGE))))
      needs_reinit=TRUE;

    weed_set_int64_value(channel,"timecode",tc);
    pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

    // align memory if necessary
    if (weed_plant_has_leaf(chantmpl,"alignment")) {
      int alignment=weed_get_int_value(chantmpl,"alignment",&error);
      boolean contiguous=FALSE;
      if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous") &&
          weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error)==WEED_TRUE) contiguous=TRUE;
      align(pixel_data,alignment,numplanes,height,rowstrides,(int *)&contiguous);
      weed_set_voidptr_array(layer,"pixel_data",numplanes,pixel_data);
      if (contiguous) weed_set_boolean_value(layer,"host_pixel_data_contiguous",WEED_TRUE);
      else if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous"))
        weed_leaf_delete(layer,"host_pixel_data_contiguous");
    }

    lives_free(rowstrides);
    weed_set_voidptr_array(channel,"pixel_data",numplanes,pixel_data);
    lives_free(pixel_data);
    if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous"))
      weed_set_boolean_value(channel,"host_pixel_data_contiguous",
                             weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error));
    else if (weed_plant_has_leaf(channel,"host_pixel_data_contiguous"))
      weed_leaf_delete(channel,"host_pixel_data_contiguous");

  }

  // we may need to disable some channels for the plugin
  for (i=0; i<num_in_tracks+num_in_alpha; i++) {
    if (weed_plant_has_leaf(in_channels[i],"host_temp_disabled")&&
        weed_get_boolean_value(in_channels[i],"host_temp_disabled",&error)==WEED_TRUE)
      weed_set_boolean_value(in_channels[i],"disabled",WEED_TRUE);
  }

  // set up our out channels
  for (i=0; i<num_out_tracks+num_out_alpha; i++) {
    channel=get_enabled_channel(inst,i,FALSE);
    if (channel==NULL) break; // compound fx

    palette=weed_get_int_value(channel,"current_palette",&error);

    if (!weed_palette_is_alpha_palette(palette)&&out_tracks[i]<0) {
      lives_free(in_tracks);
      lives_free(out_tracks);
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      if (orig_layer!=NULL) {
        weed_layer_free(layer);
      }
      return FILTER_ERROR_INVALID_TRACK; // probably audio
    }

    outwidth=weed_get_int_value(channel,"width",&error);
    outheight=weed_get_int_value(channel,"height",&error);

    weed_set_int64_value(channel,"timecode",tc);
    outpalette=weed_get_int_value(channel,"current_palette",&error);
    chantmpl=weed_get_plantptr_value(channel,"template",&error);

    channel_flags=0;
    if (weed_plant_has_leaf(chantmpl,"flags")) channel_flags=weed_get_int_value(chantmpl,"flags",&error);

    nchr=weed_leaf_num_elements(channel,"rowstrides");
    channel_rows=weed_get_int_array(channel,"rowstrides",&error);

    if (def_channel!=NULL&&i==0&&(weed_palette_is_alpha_palette
                                  (weed_get_int_value(channel,"current_palette",&error)&&
                                   weed_palette_is_alpha_palette
                                   (weed_get_int_value(def_channel,"current_palette",&error)
                                   ))||
                                  (in_tracks!=NULL&&out_tracks!=NULL&&in_tracks[0]==out_tracks[0]))) {

      if (channel_flags&WEED_CHANNEL_CAN_DO_INPLACE) {
        if (!(weed_palette_is_alpha_palette(weed_get_int_value(in_channels[i],"current_palette",&error) &&
                                            weed_plant_has_leaf(channel,"host_orig_pdata") &&
                                            weed_get_boolean_value(channel,"host_orig_pdata",&error)==WEED_TRUE))) {

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
            } else weed_leaf_delete(channel,"YUV_clamping");

            if (weed_plant_has_leaf(def_channel,"YUV_sampling"))
              weed_set_int_value(channel,"YUV_sampling",weed_get_int_value(def_channel,"YUV_sampling",&error));
            else weed_leaf_delete(channel,"YUV_sampling");

            if (weed_plant_has_leaf(def_channel,"YUV_subspace"))
              weed_set_int_value(channel,"YUV_subspace",weed_get_int_value(def_channel,"YUV_subspace",&error));
            else weed_leaf_delete(channel,"YUV_subspace");

            numplanes=weed_leaf_num_elements(def_channel,"rowstrides");
            layer_rows=weed_get_int_array(def_channel,"rowstrides",&error);
            weed_set_int_array(channel,"rowstrides",numplanes,layer_rows);
            pixel_data=weed_get_voidptr_array(def_channel,"pixel_data",&error);
            weed_set_voidptr_array(channel,"pixel_data",numplanes,pixel_data);
            lives_free(pixel_data);
            weed_set_boolean_value(channel,"inplace",WEED_TRUE);
            inplace=TRUE;
            if (weed_plant_has_leaf(def_channel,"host_pixel_data_contiguous"))
              weed_set_boolean_value(channel,"host_pixel_data_contiguous",
                                     weed_get_boolean_value(def_channel,"host_pixel_data_contiguous",&error));
            else if (weed_plant_has_leaf(channel,"host_pixel_data_contiguous"))
              weed_leaf_delete(channel,"host_pixel_data_contiguous");

            if (weed_palette_is_alpha_palette(palette)) {
              // protect our in- channel from being freed()
              weed_set_boolean_value(channel,"host_orig_pdata",WEED_TRUE);
            }
          }
          lives_free(palettes);
        }
      }
    }

    if (def_channel==NULL) def_channel=get_enabled_channel(inst,0,FALSE);

    if (weed_get_boolean_value(def_channel,"host_temp_disabled",&error)==WEED_TRUE) def_disabled=TRUE;

    ignore_palette=FALSE;

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
          } else {
            if (channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY) ignore_palette=TRUE;
            else {
              lives_free(in_tracks);
              lives_free(out_tracks);
              lives_free(in_channels);
              if (out_channels!=NULL) lives_free(out_channels);
              lives_free(channel_rows);
              if (orig_layer!=NULL) {
                weed_layer_free(layer);
              }
              return FILTER_ERROR_INVALID_PALETTE_SETTINGS; // plugin author messed up...
            }
          }
          lives_free(palettes);
        }

        if (!ignore_palette) {

          if (weed_plant_has_leaf(def_channel,"YUV_clamping")) {
            oclamping=(weed_get_int_value(def_channel,"YUV_clamping",&error));
            weed_set_int_value(channel,"YUV_clamping",oclamping);
          } else weed_leaf_delete(channel,"YUV_clamping");

          if (weed_plant_has_leaf(def_channel,"YUV_sampling"))
            weed_set_int_value(channel,"YUV_sampling",weed_get_int_value(def_channel,"YUV_sampling",&error));
          else weed_leaf_delete(channel,"YUV_sampling");

          if (weed_plant_has_leaf(def_channel,"YUV_subspace"))
            weed_set_int_value(channel,"YUV_subspace",weed_get_int_value(def_channel,"YUV_subspace",&error));
          else weed_leaf_delete(channel,"YUV_subspace");
        }
      }

      palette=weed_get_int_value(channel,"current_palette",&error);

      if (weed_plant_has_leaf(channel,"host_width")) {
        width=opwidth=weed_get_int_value(channel,"host_width",&error);
        height=opheight=weed_get_int_value(channel,"host_height",&error);
      } else {
        width=weed_get_int_value(def_channel,"width",&error);
        height=weed_get_int_value(def_channel,"height",&error);
      }

      pdata=weed_get_voidptr_value(channel,"pixel_data",&error);

      if (weed_palette_is_alpha_palette(palette)&&outpalette==palette&&outwidth==width&&outheight==height&&pdata!=NULL) {
        lives_free(channel_rows);
        continue;
      }

      set_channel_size(channel,opwidth/weed_palette_get_pixels_per_macropixel(palette),opheight,1,NULL);

      // this will look at width, height, current_palette, and create an empty pixel_data and set rowstrides
      // and update width and height if necessary
      create_empty_pixel_data(channel,FALSE,TRUE);

      numplanes=weed_leaf_num_elements(channel,"rowstrides");
      layer_rows=weed_get_int_array(channel,"rowstrides",&error);
      // align memory if necessary
      if (weed_plant_has_leaf(chantmpl,"alignment")) {
        int alignment=weed_get_int_value(chantmpl,"alignment",&error);
        boolean contiguous=FALSE;
        if (weed_plant_has_leaf(channel,"host_pixel_data_contiguous") &&
            weed_get_boolean_value(channel,"host_pixel_data_contiguous",&error)==WEED_TRUE) contiguous=TRUE;

        pixel_data=weed_get_voidptr_array(channel,"pixel_data",&error);

        height=weed_get_int_value(channel,"height",&error);

        align(pixel_data,alignment,numplanes,height,layer_rows,(int *)&contiguous);

        weed_set_voidptr_array(channel,"pixel_data",numplanes,pixel_data);
        lives_free(pixel_data);

        if (contiguous) weed_set_boolean_value(channel,"host_pixel_data_contiguous",WEED_TRUE);
        else if (weed_plant_has_leaf(channel,"host_pixel_data_contiguous"))
          weed_leaf_delete(channel,"host_pixel_data_contiguous");
      }
      weed_set_boolean_value(channel,"inplace",WEED_FALSE);
    }

    // check old rowstrides against current rowstrides
    rowstrides_changed=rowstrides_differ(nchr,channel_rows,numplanes,layer_rows);

    lives_free(channel_rows);
    lives_free(layer_rows);

    width=weed_get_int_value(channel,"width",&error);
    height=weed_get_int_value(channel,"height",&error);
    if ((rowstrides_changed&&(channel_flags&WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE))||
        (((outwidth!=width)||(outheight!=height))&&(channel_flags&WEED_CHANNEL_REINIT_ON_SIZE_CHANGE)))
      needs_reinit=TRUE;

  }

  if (needs_reinit) {
    if ((retval=weed_reinit_effect(inst,FALSE))==FILTER_ERROR_COULD_NOT_REINIT) {
      lives_free(in_tracks);
      lives_free(out_tracks);
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      if (orig_layer!=NULL) {
        weed_layer_free(layer);
      }
      return retval;
    }
  }

  weed_set_double_value(inst,"fps",cfile->pb_fps);

  //...finally we are ready to apply the filter

  channel=get_enabled_channel(inst,0,FALSE);

  // see if we can multithread
  if ((prefs->nfx_threads=future_prefs->nfx_threads)>1 &&
      filter_flags&WEED_FILTER_HINT_MAY_THREAD) {
    filter_mutex_lock(key);
    retval=process_func_threaded(inst,out_channels,tc);
    filter_mutex_unlock(key);
    if (retval!=FILTER_ERROR_DONT_THREAD) did_thread=TRUE;
  }
  if (!did_thread) {
    // normal single threaded version
    int ret;
    weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
    process_func=process_func_ptr_ptr[0];
    filter_mutex_lock(key);
    ret=(*process_func)(inst,tc);
    filter_mutex_unlock(key);
    if (ret==WEED_ERROR_PLUGIN_INVALID) retval=FILTER_ERROR_MUST_RELOAD;
  }

  if (retval==FILTER_ERROR_MUST_RELOAD) {
    lives_free(in_tracks);
    lives_free(out_tracks);
    lives_free(in_channels);
    if (out_channels!=NULL) lives_free(out_channels);
    if (orig_layer!=NULL) {
      weed_layer_free(layer);
    }
    return retval;
  }

  for (k=0; k<num_inc+num_in_alpha; k++) {
    channel=get_enabled_channel(inst,k,TRUE);
    if (weed_palette_is_alpha_palette(weed_get_int_value(channel,"current_palette",&error))) {
      // free pdata for all alpha in channels, unless orig pdata was passed from a prior fx

      if (!weed_plant_has_leaf(channel,"host_orig_pdata")||
          weed_get_boolean_value(channel,"host_orig_pdata",&error)!=WEED_TRUE) {
        pdata=weed_get_voidptr_value(channel,"pixel_data",&error);
        if (pdata!=NULL) lives_free(pdata);
      }
      weed_set_voidptr_value(channel,"pixel_data",NULL);
      if (weed_plant_has_leaf(channel,"host_orig_pdata"))
        weed_leaf_delete(channel,"host_orig_pdata");
    }
  }

  // now we write our out channels back to layers, leaving the palettes and sizes unchanged

  for (i=k=0; k<num_out_tracks+num_out_alpha; k++) {

    channel=get_enabled_channel(inst,k,FALSE);
    if (channel==NULL) break; // compound fx

    if (weed_get_boolean_value(channel,"inplace",&error)==WEED_TRUE) continue;

    if (weed_palette_is_alpha_palette(weed_get_int_value(channel,"current_palette",&error))) {
      // out chan data for alpha is freed after all fx proc - in case we need for in chans
      continue;
    }

    numplanes=weed_leaf_num_elements(channel,"rowstrides");
    palette=weed_get_int_value(channel,"current_palette",&error);

    layer=layers[out_tracks[i]];

    check_layer_ready(layer);

    rowstrides=weed_get_int_array(channel,"rowstrides",&error);
    weed_set_int_array(layer,"rowstrides",numplanes,rowstrides);
    lives_free(rowstrides);
    numplanes=weed_leaf_num_elements(layer,"pixel_data");

    pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
    if (pixel_data!=NULL) {
      if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous") &&
          weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error)==WEED_TRUE)
        numplanes=1;

      for (j=0; j<numplanes; j++) if (pixel_data[j]!=NULL) lives_free(pixel_data[j]);
      lives_free(pixel_data);
    }


    numplanes=weed_leaf_num_elements(channel,"pixel_data");
    pixel_data=weed_get_voidptr_array(channel,"pixel_data",&error);
    weed_set_voidptr_array(layer,"pixel_data",numplanes,pixel_data);
    lives_free(pixel_data);

    // set this in case it was a resize plugin
    width=weed_get_int_value(channel,"width",&error);
    height=weed_get_int_value(channel,"height",&error);
    weed_set_int_value(layer,"width",width);
    weed_set_int_value(layer,"height",height);

    i++;

    if (weed_plant_has_leaf(channel,"host_pixel_data_contiguous"))
      weed_set_boolean_value(layer,"host_pixel_data_contiguous",
                             weed_get_boolean_value(channel,"host_pixel_data_contiguous",&error));
    else if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous"))
      weed_leaf_delete(layer,"host_pixel_data_contiguous");

    chantmpl=weed_get_plantptr_value(channel,"template",&error);

    if (weed_plant_has_leaf(chantmpl,"flags")) flags=weed_get_int_value(chantmpl,"flags",&error);
    else flags=0;

    if (weed_plant_has_leaf(channel,"YUV_clamping")) {
      oclamping=(weed_get_int_value(channel,"YUV_clamping",&error));
      weed_set_int_value(layer,"YUV_clamping",oclamping);
    } else weed_leaf_delete(layer,"YUV_clamping");

    if (weed_plant_has_leaf(channel,"YUV_sampling"))
      weed_set_int_value(layer,"YUV_sampling",weed_get_int_value(channel,"YUV_sampling",&error));
    else weed_leaf_delete(layer,"YUV_sampling");

    if (weed_plant_has_leaf(channel,"YUV_subspace"))
      weed_set_int_value(layer,"YUV_subspace",weed_get_int_value(channel,"YUV_subspace",&error));
    else weed_leaf_delete(layer,"YUV_subspace");

  }

  for (i=0; i<num_inc+num_in_alpha; i++) {
    if (weed_plant_has_leaf(in_channels[i],"host_temp_disabled")&&
        weed_get_boolean_value(in_channels[i],"host_temp_disabled",&error)==WEED_TRUE) {
      weed_set_boolean_value(in_channels[i],"disabled",WEED_FALSE);
      weed_set_boolean_value(in_channels[i],"host_temp_disabled",WEED_FALSE);
    }
  }
  // done...

  lives_free(in_tracks);
  lives_free(out_tracks);
  lives_free(in_channels);
  if (out_channels!=NULL) lives_free(out_channels);

  if (orig_layer!=NULL) {
    weed_layer_free(layer);
  }

  return retval;
}



static lives_filter_error_t weed_apply_audio_instance_inner(weed_plant_t *inst, weed_plant_t *init_event,
    weed_plant_t **layers, weed_timecode_t tc, int nbtracks) {
  int *in_tracks,*out_tracks;
  int *mand;

  int error;

  weed_plant_t *filter=weed_instance_get_filter(inst,FALSE);
  weed_plant_t *layer;
  weed_plant_t **in_channels,**out_channels=NULL,*channel,*chantmpl;
  weed_plant_t **in_ctmpls;

  weed_process_f *process_func_ptr_ptr;
  weed_process_f process_func;
  lives_filter_error_t retval=FILTER_NO_ERROR;

  void *adata=NULL,*adata0=NULL;

  boolean inplace=FALSE;
  boolean did_thread=FALSE;

  int channel_flags,filter_flags=0;
  int num_in_tracks,num_out_tracks;

  int num_ctmpl,num_inc;
  int key=-1;

  int nchans=0;
  int nsamps=0;

  register int i,j;

  // TODO - handle the following:
  // input audio_channels are mono, but the plugin NEEDS stereo

  if (weed_plant_has_leaf(filter,"flags")) filter_flags=weed_get_int_value(filter,"flags",&error);

  if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);

  // here, in_tracks and out_tracks map our layers to in_channels and out_channels in the filter
  if (!weed_plant_has_leaf(inst,"in_channels")||(in_channels=weed_get_plantptr_array(inst,"in_channels",&error))==NULL) {
    if (out_channels==NULL&&weed_plant_has_leaf(inst,"out_parameters")) {

      // TODO - need to check the entire chain of effects

      // run it only if it outputs into effects which have audio chans
      if (!feeds_to_audio_filters(key,rte_key_getmode(key+1))) return FILTER_ERROR_NO_IN_CHANNELS;

      // data processing effect; just call the process_func
      weed_set_double_value(inst,"fps",cfile->pb_fps);

      // see if we can multithread
      if ((prefs->nfx_threads=future_prefs->nfx_threads)>1 &&
          filter_flags&WEED_FILTER_HINT_MAY_THREAD) {
        filter_mutex_lock(key);
        retval=process_func_threaded(inst,out_channels,tc);
        filter_mutex_unlock(key);
        if (retval!=FILTER_ERROR_DONT_THREAD) did_thread=TRUE;
      }
      if (!did_thread) {
        // normal single threaded version
        int ret;
        weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
        process_func=process_func_ptr_ptr[0];
        filter_mutex_lock(key);
        ret=(*process_func)(inst,tc);
        filter_mutex_unlock(key);
        if (ret==WEED_ERROR_PLUGIN_INVALID) retval=FILTER_ERROR_MUST_RELOAD;
      }
      return retval;
    }

    return FILTER_ERROR_NO_IN_CHANNELS;
  }

  if (get_enabled_channel(inst,0,TRUE)==NULL) {
    // we process generators elsewhere
    return FILTER_ERROR_NO_IN_CHANNELS;
  }


  if (init_event==NULL) {
    num_in_tracks=enabled_in_channels(inst,FALSE);
    in_tracks=(int *)lives_malloc(2*sizint);
    in_tracks[0]=0;
    in_tracks[1]=1;
    num_out_tracks=1;
    out_tracks=(int *)lives_malloc(sizint);
    out_tracks[0]=0;
  } else {
    num_in_tracks=weed_leaf_num_elements(init_event,"in_tracks");
    in_tracks=weed_get_int_array(init_event,"in_tracks",&error);
    num_out_tracks=weed_leaf_num_elements(init_event,"out_tracks");
    out_tracks=weed_get_int_array(init_event,"out_tracks",&error);
  }

  if (!weed_plant_has_leaf(inst,"out_channels")||(out_channels=weed_get_plantptr_array(inst,"out_channels",&error))==NULL) {
    num_out_tracks=0;
  }

  // handle case where in_tracks[i] > than num layers
  // either we temporarily disable the channel, or we can't apply the filter
  num_inc=weed_leaf_num_elements(inst,"in_channels");

  if (num_in_tracks>num_inc) num_in_tracks=num_inc;

  if (num_inc>num_in_tracks) {
    for (i=num_in_tracks; i<num_inc; i++) {
      if (!weed_plant_has_leaf(in_channels[i],"disabled")||
          weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_FALSE)
        weed_set_boolean_value(in_channels[i],"host_temp_disabled",WEED_TRUE);
      else weed_set_boolean_value(in_channels[i],"host_temp_disabled",WEED_FALSE);
    }
  }


  for (i=0; i<num_in_tracks; i++) {
    channel=in_channels[i];
    weed_set_boolean_value(channel,"host_temp_disabled",WEED_FALSE);
    layer=layers[i];
    if (layer==NULL) {
      for (j=i; j<num_in_tracks; j++) {
        channel=in_channels[j];
        chantmpl=weed_get_plantptr_value(channel,"template",&error);
        if (weed_plant_has_leaf(chantmpl,"max_repeats")) weed_set_boolean_value(channel,"host_temp_disabled",WEED_TRUE);
        else {
          lives_free(in_tracks);
          lives_free(out_tracks);
          lives_free(in_channels);
          if (out_channels!=NULL) lives_free(out_channels);
          return FILTER_ERROR_MISSING_LAYER;
        }
      }
      break;
    }
    if (weed_get_voidptr_value(layer,"audio_data",&error)==NULL) {
      chantmpl=weed_get_plantptr_value(channel,"template",&error);
      if (weed_plant_has_leaf(chantmpl,"max_repeats")) weed_set_boolean_value(channel,"host_temp_disabled",WEED_TRUE);
    }
  }

  // ensure all chantmpls not marked "optional" have at least one corresponding enabled channel
  // e.g. we could have disabled all channels from a template with "max_repeats" that is not "optional"
  num_ctmpl=weed_leaf_num_elements(filter,"in_channel_templates");
  mand=(int *)lives_malloc(num_ctmpl*sizint);
  for (j=0; j<num_ctmpl; j++) mand[j]=0;
  in_ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  for (i=0; i<num_inc; i++) {
    if ((weed_plant_has_leaf(in_channels[i],"disabled")&&
         weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_TRUE)||
        (weed_plant_has_leaf(in_channels[i],"host_temp_disabled")&&
         weed_get_boolean_value(in_channels[i],"host_temp_disabled",&error)==WEED_TRUE)) continue;
    chantmpl=weed_get_plantptr_value(in_channels[i],"template",&error);
    for (j=0; j<num_ctmpl; j++) {
      if (chantmpl==in_ctmpls[j]) {
        mand[j]=1;
        break;
      }
    }
  }
  for (j=0; j<num_ctmpl; j++) if (mand[j]==0&&(!weed_plant_has_leaf(in_ctmpls[j],"optional")||
                                    weed_get_boolean_value(in_ctmpls[j],"optional",&error)==WEED_FALSE)) {
      lives_free(in_ctmpls);
      lives_free(in_tracks);
      lives_free(out_tracks);
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      lives_free(mand);
      return FILTER_ERROR_MISSING_LAYER;
    }
  lives_free(in_ctmpls);
  lives_free(mand);

  for (i=0; i<num_in_tracks; i++) {
    if (weed_plant_has_leaf(in_channels[i],"host_temp_disabled")&&
        weed_get_boolean_value(in_channels[i],"host_temp_disabled",&error)==WEED_TRUE) continue;

    layer=layers[i];
    channel=get_enabled_channel(inst,i,TRUE);
    chantmpl=weed_get_plantptr_value(channel,"template",&error);

    weed_set_int64_value(channel,"timecode",tc);
    adata=weed_get_voidptr_value(layer,"audio_data",&error);
    if (i==0) adata0=adata;

    // nchans and nsamps needed for inplace
    nchans=weed_get_int_value(channel,"audio_channels",&error);
    nsamps=weed_get_int_value(channel,"audio_data_length",&error);

    if (weed_get_boolean_value(channel,"audio_interleaf",&error)==WEED_TRUE) {
      // handle case where plugin NEEDS interleaved
      weed_set_boolean_value(layers[i],"audio_interleaf",WEED_TRUE);
      if (!float_interleave((float *)adata,nsamps,nchans)) {
        lives_free(in_tracks);
        lives_free(out_tracks);
        lives_free(in_channels);
        if (out_channels!=NULL) lives_free(out_channels);
        return FILTER_ERROR_MEMORY_ERROR;
      }
    }

    weed_set_voidptr_value(channel,"audio_data",adata);
  }

  // we may need to disable some channels for the plugin
  for (i=0; i<num_in_tracks; i++) {
    if (weed_plant_has_leaf(in_channels[i],"host_temp_disabled")&&
        weed_get_boolean_value(in_channels[i],"host_temp_disabled",&error)==WEED_TRUE)
      weed_set_boolean_value(in_channels[i],"disabled",WEED_TRUE);
  }

  // set up our out channels
  for (i=0; i<num_out_tracks; i++) {
    if (out_tracks[i]!=in_tracks[i]) {
      lives_free(in_tracks);
      lives_free(out_tracks);
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      return FILTER_ERROR_INVALID_TRACK; // can't yet mix audio and video
    }

    channel=get_enabled_channel(inst,i,FALSE);

    weed_set_int64_value(channel,"timecode",tc);
    chantmpl=weed_get_plantptr_value(channel,"template",&error);
    channel_flags=weed_get_int_value(chantmpl,"flags",&error);

    if (i==0&&(in_tracks[0]==out_tracks[0])&&adata0!=NULL) {
      if (channel_flags&WEED_CHANNEL_CAN_DO_INPLACE) {
        // ah, good, inplace
        inplace=TRUE;
      }
    }

    if (!inplace) {
      float *abuf=(float *)lives_malloc0(nchans*nsamps*sizeof(float));
      weed_set_int_value(channel,"audio_data_length",nsamps);
      weed_set_voidptr_value(channel,"audio_data",abuf);
      inplace=FALSE;
    } else {
      weed_set_int_value(channel,"audio_data_length",nsamps);
      weed_set_voidptr_value(channel,"audio_data",adata0);
      weed_set_boolean_value(layers[i],"audio_interleaf",weed_get_boolean_value(channel,"audio_interleaf",&error));
    }
  }

  weed_set_double_value(inst,"fps",cfile->pb_fps);

  //...finally we are ready to apply the filter
  pthread_mutex_lock(&mainw->interp_mutex); // stop video thread from possibly interpolating our audio effects
  weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
  process_func=process_func_ptr_ptr[0];
  filter_mutex_lock(key);

  if ((*process_func)(inst,tc)==WEED_ERROR_PLUGIN_INVALID) {
    filter_mutex_unlock(key);
    pthread_mutex_unlock(&mainw->interp_mutex);
    lives_free(in_tracks);
    lives_free(out_tracks);
    lives_free(in_channels);
    if (out_channels!=NULL) lives_free(out_channels);
    return FILTER_ERROR_MUST_RELOAD;
  }

  pthread_mutex_unlock(&mainw->interp_mutex);
  filter_mutex_unlock(key);

  // TODO - handle process errors (WEED_ERROR_PLUGIN_INVALID)

  // now we write our out channels back to layers
  for (i=0; i<num_out_tracks; i++) {
    if (i==0&&inplace) continue;

    // TODO - check this...

    channel=get_enabled_channel(inst,i,FALSE);
    layer=layers[i];
    if (weed_plant_has_leaf(channel,"audio_data")) {
      float *audio_data=(float *)weed_get_voidptr_value(layer,"audio_data",&error);
      if (audio_data!=NULL) lives_free(audio_data);
    }
    weed_set_voidptr_value(layer,"audio_data",weed_get_voidptr_value(channel,"audio_data",&error));
  }

  for (i=0; i<num_inc; i++) {
    if (weed_plant_has_leaf(in_channels[i],"host_temp_disabled")&&
        weed_get_boolean_value(in_channels[i],"host_temp_disabled",&error)==WEED_TRUE) {
      weed_set_boolean_value(in_channels[i],"disabled",WEED_FALSE);
      weed_set_boolean_value(in_channels[i],"host_temp_disabled",WEED_FALSE);
    }
  }
  // done...

  lives_free(in_tracks);
  lives_free(out_tracks);
  lives_free(in_channels);
  if (out_channels!=NULL) lives_free(out_channels);


  return retval;

}



lives_filter_error_t weed_apply_audio_instance(weed_plant_t *init_event, float **abuf, int nbtracks, int nchans,
    int64_t nsamps, double arate, weed_timecode_t tc, double *vis) {

  lives_filter_error_t retval=FILTER_NO_ERROR,retval2;

  weed_plant_t **layers=NULL,**in_channels=NULL,**out_channels=NULL;

  weed_plant_t *instance=NULL,*filter;
  weed_plant_t *channel=NULL;
  weed_plant_t *ctmpl;

  float *in_abuf,*out_abuf;
  int *in_tracks=NULL,*out_tracks=NULL;

  size_t nsf=nsamps*sizeof(float);

  boolean needs_reinit=FALSE;
  boolean was_init_event=FALSE;

  int error;

  int flags=0;
  int key=-1;

  int ntracks=1;
  int numinchans=0,numoutchans=0,xnchans,aint;

  register int i,j;

  // caller passes an init_event from event list, or an instance (for realtime mode)

  if (WEED_PLANT_IS_FILTER_INSTANCE(init_event)) {
    // for realtime, we pass a single instance instead of init_event
    instance=init_event;
    // set init_event to NULL before passing it to the inner function
    init_event=NULL;

    in_tracks=(int *)lives_malloc(sizint);
    in_tracks[0]=0;

    out_tracks=(int *)lives_malloc(sizint);
    out_tracks[0]=0;

  } else {

    // when processing an event list, we pass an init_event

    was_init_event=TRUE;
    if (weed_plant_has_leaf(init_event,"host_tag")) {
      char *keystr=weed_get_string_value(init_event,"host_tag",&error);
      key=atoi(keystr);
      lives_free(keystr);
    } else return FILTER_ERROR_INVALID_INIT_EVENT;

    ntracks=weed_leaf_num_elements(init_event,"in_tracks");
    in_tracks=weed_get_int_array(init_event,"in_tracks",&error);

    if (weed_plant_has_leaf(init_event,"out_tracks")) out_tracks=weed_get_int_array(init_event,"out_tracks",&error);

    // check instance exists, and interpolate parameters

    if (rte_key_valid(key+1,FALSE)) {
      if ((instance=key_to_instance[key][key_modes[key]])==NULL) {
        lives_free(in_tracks);
        lives_free(out_tracks);
        return FILTER_ERROR_INVALID_INSTANCE;
      }
      if (mainw->pchains!=NULL&&mainw->pchains[key]!=NULL) {
        if (!pthread_mutex_trylock(&mainw->interp_mutex)) { // try to minimise thread locking
          pthread_mutex_unlock(&mainw->interp_mutex);
          if (!interpolate_params(instance,mainw->pchains[key],tc)) {
            lives_free(in_tracks);
            lives_free(out_tracks);
            return FILTER_ERROR_INTERPOLATION_FAILED;
          }
        }
      }
    } else {
      lives_free(in_tracks);
      lives_free(out_tracks);
      return FILTER_ERROR_INVALID_INIT_EVENT;
    }
  }

audinst1:

  if (weed_plant_has_leaf(instance,"in_channels")) channel=weed_get_plantptr_value(instance,"in_channels",&error);
  if (channel==NULL) {
    if (weed_plant_has_leaf(instance,"out_channels")) {
      retval=FILTER_ERROR_NO_IN_CHANNELS; // audio generators are dealt with by the audio player
      goto audret1;
    }
  } else {
    in_channels=weed_get_plantptr_array(instance,"in_channels",&error);
    numinchans=weed_leaf_num_elements(instance,"in_channels");
  }

  if (weed_plant_has_leaf(instance,"out_channels")) {
    out_channels=weed_get_plantptr_array(instance,"out_channels",&error);
    if (out_channels[0]!=NULL) numoutchans=weed_leaf_num_elements(instance,"out_channels");
  }

  filter=weed_instance_get_filter(instance,FALSE);

  if (!weed_plant_has_leaf(filter,"flags")) weed_set_int_value(filter,"flags",0);
  else flags=weed_get_int_value(filter,"flags",&error);

  if (vis!=NULL&&vis[0]<0.&&in_tracks[0]<=-nbtracks) {
    // first layer comes from ascrap file; do not apply any effects except the audio mixer
    if (numinchans==1&&numoutchans==1&&!(flags&WEED_FILTER_IS_CONVERTER)) {
      retval=FILTER_ERROR_IS_SCRAP_FILE;
      goto audret1;
    }
  }

  for (i=0; i<numinchans; i++) {
    if ((channel=in_channels[i])==NULL) continue;

    aint=WEED_FALSE; // preferred value
    ctmpl=weed_get_plantptr_value(channel,"template",&error);
    if (weed_plant_has_leaf(ctmpl,"audio_interleaf")) {
      aint=weed_get_boolean_value(ctmpl,"audio_interleaf",&error);
      if (weed_get_boolean_value(channel,"audio_interleaf",&error)!=aint) {
        needs_reinit=TRUE;
      }
    }
    xnchans=nchans; // preferred value
    if (weed_plant_has_leaf(ctmpl,"audio_channels")) {
      xnchans=weed_get_int_value(ctmpl,"audio_channels",&error);
      if (weed_get_int_value(channel,"audio_channels",&error)!=xnchans) {
        needs_reinit=TRUE;
      }
    }


    if ((weed_plant_has_leaf(ctmpl,"audio_data_length")&&nsamps!=weed_get_int_value(ctmpl,"audio_data_length",&error))||
        (weed_plant_has_leaf(ctmpl,"audio_arate")&&arate!=weed_get_int_value(ctmpl,"audio_rate",&error))) {
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      retval=FILTER_ERROR_TEMPLATE_MISMATCH;
      goto audret1;
    }

    if (weed_get_int_value(channel,"audio_rate",&error)!=arate) {
      needs_reinit=TRUE;
    }

    weed_set_int_value(channel,"audio_data_length",nsamps);

  }

  for (i=0; i<numoutchans; i++) {
    if ((channel=out_channels[i])==NULL) continue;

    aint=WEED_FALSE; // preferred value
    ctmpl=weed_get_plantptr_value(channel,"template",&error);
    if (weed_plant_has_leaf(ctmpl,"audio_interleaf")) {
      aint=weed_get_boolean_value(ctmpl,"audio_interleaf",&error);
      if (weed_get_boolean_value(channel,"audio_interleaf",&error)!=aint) {
        needs_reinit=TRUE;
      }
    }
    xnchans=nchans; // preferred value
    if (weed_plant_has_leaf(ctmpl,"audio_channels")) {
      xnchans=weed_get_int_value(ctmpl,"audio_channels",&error);
      if (weed_get_int_value(channel,"audio_channels",&error)!=xnchans) {
        needs_reinit=TRUE;
      }
    }

    if ((weed_plant_has_leaf(ctmpl,"audio_data_length")&&nsamps!=weed_get_int_value(ctmpl,"audio_data_length",&error))||
        (weed_plant_has_leaf(ctmpl,"audio_arate")&&arate!=weed_get_int_value(ctmpl,"audio_rate",&error))) {
      lives_free(in_channels);
      if (out_channels!=NULL) lives_free(out_channels);
      retval=FILTER_ERROR_TEMPLATE_MISMATCH;
      goto audret1;
    }

    if (weed_get_int_value(channel,"audio_rate",&error)!=arate) {
      needs_reinit=TRUE;
    }

    weed_set_int_value(channel,"audio_data_length",nsamps);

  }


  if (needs_reinit) {
    // - deinit inst
    weed_call_deinit_func(instance);

    for (i=0; i<numinchans; i++) {
      if ((channel=in_channels[i])!=NULL) {
        aint=WEED_FALSE; // preferred value
        ctmpl=weed_get_plantptr_value(channel,"template",&error);
        if (weed_plant_has_leaf(ctmpl,"audio_interleaf")) {
          aint=weed_get_boolean_value(ctmpl,"audio_interleaf",&error);
        }
        xnchans=nchans; // preferred value
        if (weed_plant_has_leaf(ctmpl,"audio_channels")) {
          xnchans=weed_get_int_value(ctmpl,"audio_channels",&error);
        }

        weed_set_boolean_value(channel,"audio_interleaf",aint);
        weed_set_int_value(channel,"audio_channels",xnchans);
        weed_set_int_value(channel,"audio_rate",arate);
      }
    }

    for (i=0; i<numoutchans; i++) {
      if ((channel=out_channels[i])!=NULL) {
        aint=WEED_FALSE; // preferred value
        ctmpl=weed_get_plantptr_value(channel,"template",&error);
        if (weed_plant_has_leaf(ctmpl,"audio_interleaf")) {
          aint=weed_get_boolean_value(ctmpl,"audio_interleaf",&error);
        }
        xnchans=nchans; // preferred value
        if (weed_plant_has_leaf(ctmpl,"audio_channels")) {
          xnchans=weed_get_int_value(ctmpl,"audio_channels",&error);
        }

        weed_set_boolean_value(channel,"audio_interleaf",aint);
        weed_set_int_value(channel,"audio_channels",xnchans);
        weed_set_int_value(channel,"audio_rate",arate);
      }
    }

    // - init inst
    if (weed_plant_has_leaf(filter,"init_func")) {
      weed_init_f *init_func_ptr_ptr;
      weed_init_f init_func;
      weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
      init_func=init_func_ptr_ptr[0];
      if (init_func!=NULL) {
        char *cwd=cd_to_plugin_dir(filter);
        set_param_gui_readwrite(instance);
        update_host_info(instance);
        if ((*init_func)(instance)!=WEED_NO_ERROR) {
          key_to_instance[key][key_modes[key]]=NULL;
          lives_chdir(cwd,FALSE);
          lives_free(cwd);
          lives_free(in_channels);
          if (out_channels!=NULL) lives_free(out_channels);
          retval=FILTER_ERROR_COULD_NOT_REINIT;
          goto audret1;
        }
        set_param_gui_readonly(instance);
        lives_chdir(cwd,FALSE);
        lives_free(cwd);
      }
    }
    weed_set_boolean_value(instance,"host_inited",WEED_TRUE);
    retval=FILTER_INFO_REINITED;
  }



  if (in_channels!=NULL) lives_free(in_channels);
  if (out_channels!=NULL) lives_free(out_channels);

  // apply visibility mask to volume values
  if (vis!=NULL&&(flags&WEED_FILTER_IS_CONVERTER)) {
    int vmaster=get_master_vol_param(filter,FALSE);
    if (vmaster!=-1) {
      weed_plant_t **in_params=weed_get_plantptr_array(instance,"in_parameters",&error);
      int nvals=weed_leaf_num_elements(in_params[vmaster],"value");
      double *fvols=weed_get_double_array(in_params[vmaster],"value",&error);
      for (i=0; i<nvals; i++) {
        fvols[i]=fvols[i]*vis[in_tracks[i]+nbtracks];
        if (vis[in_tracks[i]+nbtracks]<0.) fvols[i]=-fvols[i];
      }
      filter_mutex_lock(key);
      weed_set_double_array(in_params[vmaster],"value",nvals,fvols);
      filter_mutex_unlock(key);
      set_copy_to(instance,vmaster,TRUE);
      lives_free(fvols);
      lives_free(in_params);
    }
  }

  if (layers==NULL&&numinchans>0) {
    layers=(weed_plant_t **)lives_malloc((ntracks+1)*sizeof(weed_plant_t *));

    for (i=0; i<ntracks; i++) {
      // create audio layers, and copy/combine separated audio into each layer

      layers[i]=weed_plant_new(WEED_PLANT_CHANNEL);

      // copy audio into layer audio
      in_abuf=(float *)lives_malloc(nchans*nsf);

      // non-interleaved
      for (j=0; j<nchans; j++) {
        lives_memcpy(&in_abuf[j*nsamps],abuf[(in_tracks[i]+nbtracks)*nchans+j],nsf);
      }

      weed_set_voidptr_value(layers[i],"audio_data",(void *)in_abuf);
      weed_set_int_value(layers[i],"audio_data_length",nsamps);
      weed_set_int_value(layers[i],"audio_channels",nchans);
      weed_set_int_value(layers[i],"audio_rate",arate);
      weed_set_boolean_value(layers[i],"audio_interleaf",WEED_FALSE);
    }

    layers[i]=NULL;
  }

  retval2=weed_apply_audio_instance_inner(instance,init_event,layers,tc,nbtracks);
  if (retval==FILTER_NO_ERROR) retval=retval2;

  if (retval2==FILTER_NO_ERROR&&was_init_event) {
    // handle compound filters
    if (weed_plant_has_leaf(instance,"host_next_instance")) {
      instance=weed_get_plantptr_value(instance,"host_next_instance",&error);
      goto audinst1;
    }
  }

  out_abuf=(float *)weed_get_voidptr_value(layers[0],"audio_data",&error);

  if (numoutchans>0) {
    // copy processed audio

    // inner function will set this if the plugin accepts/returns interleaved audio only
    if (weed_get_boolean_value(layers[0],"audio_interleaf",&error)==WEED_TRUE) float_deinterleave(out_abuf,nsamps,nchans);

    // non-interleaved
    for (i=0; i<nchans; i++) {
      lives_memcpy(abuf[(out_tracks[0]+nbtracks)*nchans+i],&out_abuf[i*nsamps],nsf);
    }
  }

audret1:

  if (layers!=NULL) {
    for (i=0; i<ntracks; i++) {
      in_abuf=(float *)weed_get_voidptr_value(layers[i],"audio_data",&error);
      lives_free(in_abuf);
      weed_plant_free(layers[i]);
    }
    lives_free(layers);
  }
  lives_free(in_tracks);
  lives_free(out_tracks);

  return retval;
}


static void weed_apply_filter_map(weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc, void ***pchains) {
  weed_plant_t *instance;
  weed_plant_t *init_event,*deinit_event;

  char *keystr;

  void **init_events;

  lives_filter_error_t filter_error;
  int key,num_inst,error;

  boolean needs_reinit;

  register int i;

  // this is called during rendering - we will have previously received a filter_map event and now we apply this to layers

  // layers will be a NULL terminated array of channels, each with two extra leaves: clip and frame
  // clip corresponds to a LiVES file in mainw->files. "pixel_data" will initially be NULL, we will pull this as necessary
  // and the effect output is written back to the layers

  // a frame number of 0 indicates a blank frame;
  // if an effect gets one of these it will not process (except if the channel is optional,
  // in which case the channel is disabled); the same is true for invalid track numbers -
  // hence disabled channels which do not have disabled in the template are re-enabled when a frame is available

  // after all processing, we generally display/output the first non-NULL layer.
  // If all layers are NULL we generate a blank frame

  // size and palettes of resulting layers may change during this

  // the channel sizes are set by the filter: all channels are all set to the size of the largest input layer.
  // (We attempt to do this, but some channels have fixed sizes).

  if (filter_map==NULL||!weed_plant_has_leaf(filter_map,"init_events")||
      (weed_get_voidptr_value(filter_map,"init_events",&error)==NULL)) return;

  if ((num_inst=weed_leaf_num_elements(filter_map,"init_events"))>0) {
    init_events=weed_get_voidptr_array(filter_map,"init_events",&error);
    for (i=0; i<num_inst; i++) {
      init_event=(weed_plant_t *)init_events[i];
      if (mainw->playing_file==-1&&mainw->multitrack!=NULL&&mainw->multitrack->current_rfx!=NULL&&
          mainw->multitrack->init_event!=NULL) {
        if (mainw->multitrack->current_rfx->source_type==LIVES_RFX_SOURCE_WEED&&
            mainw->multitrack->current_rfx->source!=NULL) {

          deinit_event=(weed_plant_t *)weed_get_voidptr_value(mainw->multitrack->init_event,"deinit_event",&error);
          if (tc>=get_event_timecode(mainw->multitrack->init_event)&&tc<=get_event_timecode(deinit_event)) {
            // we are previewing an effect in multitrack - use current unapplied values
            // and display only the currently selected filter (unless it is a "process_last" effect (like audio mixer))
            // TODO - if current fx
            void **pchain=mt_get_pchain();
            instance=(weed_plant_t *)mainw->multitrack->current_rfx->source;

            if (is_pure_audio(instance,FALSE)) continue; // audio effects are applied in the audio renderer

            // interpolation can be switched of by setting mainw->no_interp
            if (!mainw->no_interp&&pchain!=NULL) {
              interpolate_params(instance,pchain,tc); // interpolate parameters for preview
            }




apply_inst1:

            if (weed_plant_has_leaf(instance,"host_next_instance")) {
              // chain any internal data pipelines for compound fx
              needs_reinit=pconx_chain_data_internal(instance);
              if (needs_reinit) {
                weed_reinit_effect(instance,FALSE);
              }
            }

            filter_error=weed_apply_instance(instance,mainw->multitrack->init_event,layers,0,0,tc);
            if (filter_error==WEED_NO_ERROR&&weed_plant_has_leaf(instance,"host_next_instance")) {
              // handling for compound fx
              instance=weed_get_plantptr_value(instance,"host_next_instance",&error);

              goto apply_inst1;
            }

            if (!init_event_is_process_last(mainw->multitrack->init_event)) break;
            continue;
          }
        }
      }

      // else we are previewing or rendering from an event_list
      if (weed_plant_has_leaf(init_event,"host_tag")) {
        keystr=weed_get_string_value(init_event,"host_tag",&error);
        key=atoi(keystr);
        lives_free(keystr);
        if (rte_key_valid(key+1,FALSE)) {
          if ((instance=key_to_instance[key][key_modes[key]])==NULL) continue;

          if (is_pure_audio(instance,FALSE)) continue; // audio effects are applied in the audio renderer

          if (pchains!=NULL&&pchains[key]!=NULL) {
            interpolate_params(instance,pchains[key],tc); // interpolate parameters during playback
          }

          /*
          // might be needed for multitrack ???
          if (mainw->pconx!=NULL) {
          int key=i;
          int mode=key_modes[i];
          if (weed_plant_has_leaf(instance,"host_mode")) {
          key=weed_get_int_value(instance,"host_key",&error);
          mode=weed_get_int_value(instance,"host_mode",&error);
          }
          // chain any data pipelines
          pconx_chain_data(key,mode);
          }*/

apply_inst2:

          if (weed_plant_has_leaf(instance,"host_next_instance")) {
            // chain any internal data pipelines for compound fx
            needs_reinit=pconx_chain_data_internal(instance);
            if (needs_reinit) {
              weed_reinit_effect(instance,FALSE);
            }
          }

          filter_error=weed_apply_instance(instance,init_event,layers,0,0,tc);

          if (filter_error==WEED_NO_ERROR&&weed_plant_has_leaf(instance,"host_next_instance")) {
            // handling for compound fx
            instance=weed_get_plantptr_value(instance,"host_next_instance",&error);

            goto apply_inst2;
          }

          //if (filter_error!=FILTER_NO_ERROR) lives_printerr("Render error was %d\n",filter_error);
        }
      }
    }

    lives_free(init_events);
  }
}




weed_plant_t *weed_apply_effects(weed_plant_t **layers, weed_plant_t *filter_map, weed_timecode_t tc,
                                 int opwidth, int opheight, void ***pchains) {
  // given a stack of layers, a filter map, a timecode and possibly paramater chains
  // apply the effects in the filter map, and return a single layer as the result

  // if all goes wrong we return a blank 4x4 RGB24 layer (TODO - return a NULL ?)

  // returned layer can be of any width,height,palette
  // caller should free all input layers, "pixel_data" of all non-returned layers is free()d here

  weed_plant_t *filter,*instance,*layer;
  lives_filter_error_t filter_error;

  boolean needs_reinit;

  int error;
  int output=-1;
  int clip;

  register int i;

  if (mainw->is_rendering&&!(cfile->proc_ptr!=NULL&&mainw->preview)) {
    // rendering from multitrack
    if (filter_map!=NULL&&layers[0]!=NULL) {
      weed_apply_filter_map(layers, filter_map, tc, pchains);
    }
  }

  // free playback: we will have here only one or two layers, and no filter_map.
  // Effects are applied in key order, in tracks are 0 and 1, out track is 0
  else {
    for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
      if (rte_key_valid(i+1,TRUE)) {
        if (!(mainw->rte&(GU641<<i))) {
          // if anything is connected to ACTIVATE, the fx may be activated
          pconx_chain_data(i,key_modes[i]);
        }
        if (mainw->rte&(GU641<<i)) {
          mainw->osc_block=TRUE;
          if ((instance=key_to_instance[i][key_modes[i]])==NULL) continue;
          if (mainw->pchains!=NULL&&mainw->pchains[i]!=NULL) {
            interpolate_params(instance,mainw->pchains[i],tc); // interpolate parameters during preview
          }

          filter=weed_instance_get_filter(instance,TRUE);

          // TODO *** enable this, and apply pconx to audio gens in audio.c
          if (!is_pure_audio(filter,TRUE)) {
            if (mainw->pconx!=NULL&&!(mainw->preview||mainw->is_rendering)) {
              // chain any data pipelines
              needs_reinit=pconx_chain_data(i,key_modes[i]);

              // if anything is connected to ACTIVATE, the fx may be activated
              if ((instance=key_to_instance[i][key_modes[i]])==NULL) continue;

              if (needs_reinit) {
                weed_reinit_effect(instance,FALSE);
              }
            }
          }

apply_inst3:

          if (weed_plant_has_leaf(instance,"host_next_instance")) {
            // chain any internal data pipelines for compound fx
            needs_reinit=pconx_chain_data_internal(instance);
            if (needs_reinit) {
              weed_reinit_effect(instance,FALSE);
            }
          }

          filter_error=weed_apply_instance(instance,NULL,layers,opwidth,opheight,tc);

          if (filter_error==FILTER_INFO_REINITED) redraw_pwindow(i,key_modes[i]); // redraw our paramwindow
          //#define DEBUG_RTE
#ifdef DEBUG_RTE
          if (filter_error!=FILTER_NO_ERROR) lives_printerr("Render error was %d\n",filter_error);
#endif

          if (filter_error==WEED_NO_ERROR&&weed_plant_has_leaf(instance,"host_next_instance")) {
            // handling for compound fx

            instance=weed_get_plantptr_value(instance,"host_next_instance",&error);

            goto apply_inst3;
          }

        }
      }
    }
  }

  // TODO - set mainw->vpp->play_params from connected out params and out alphas

  if (!mainw->is_rendering) {
    mainw->osc_block=FALSE;
  }

  // caller should free all layers, but here we will free all other pixel_data

  for (i=0; layers[i]!=NULL; i++) {

    check_layer_ready(layers[i]);

    if (layers[i]==mainw->blend_layer) mainw->blend_layer=NULL;

    if ((weed_plant_has_leaf(layers[i],"pixel_data")&&weed_get_voidptr_value(layers[i],"pixel_data",&error)!=NULL)||
        (weed_get_int_value(layers[i],"frame",&error)!=0&&
         (mainw->playing_file>-1||mainw->multitrack==NULL||mainw->multitrack->current_rfx==NULL||
          (mainw->multitrack->init_event==NULL||tc<get_event_timecode(mainw->multitrack->init_event)||
           (mainw->multitrack->init_event==mainw->multitrack->avol_init_event)||
           tc>get_event_timecode((weed_plant_t *)weed_get_voidptr_value
                                 (mainw->multitrack->init_event,"deinit_event",&error)))))) {
      if (output!=-1||weed_get_int_value(layers[i],"clip",&error)==-1) {
        if (!weed_plant_has_leaf(layers[i],"pixel_data")) continue;
        weed_layer_pixel_data_free(layers[i]);
      } else output=i;
    }
  }

  if (output==-1) {
    // blank frame - e.g. for multitrack
    weed_plant_t *layer=weed_layer_new(opwidth>4?opwidth:4,opheight>4?opheight:4,NULL,WEED_PALETTE_RGB24);
    create_empty_pixel_data(layer,TRUE,TRUE);
    return layer;
  }

  layer=layers[output];
  clip=weed_get_int_value(layer,"clip",&error);

  // frame is pulled uneffected here. TODO: Try to pull at target output palette
  if (!weed_plant_has_leaf(layer,"pixel_data")||weed_get_voidptr_value(layer,"pixel_data",&error)==NULL)
    if (!pull_frame_at_size(layer,get_image_ext_for_type(mainw->files[clip]->img_type),tc,opwidth,opheight,
                            WEED_PALETTE_END)) {
      weed_set_int_value(layer,"current_palette",mainw->files[clip]->img_type==IMG_TYPE_JPEG?
                         WEED_PALETTE_RGB24:WEED_PALETTE_RGBA32);
      weed_set_int_value(layer,"width",opwidth);
      weed_set_int_value(layer,"height",opheight);
      create_empty_pixel_data(layer,TRUE,TRUE);
      LIVES_WARN("weed_apply_effects created empty pixel_data");
    }

  return layer;
}




void weed_apply_audio_effects(weed_plant_t *filter_map, float **abuf, int nbtracks, int nchans, int64_t nsamps,
                              double arate, weed_timecode_t tc, double *vis) {
  int i,num_inst,error;
  void **init_events;
  lives_filter_error_t filter_error;
  weed_plant_t *init_event,*filter;
  char *fhash;

  // this is called during rendering - we will have previously received a
  // filter_map event and now we apply this to audio (abuf)
  // abuf will be a NULL terminated array of float audio

  // the results of abuf[0] and abuf[1] (for stereo) will be written to fileno
  if (filter_map==NULL||!weed_plant_has_leaf(filter_map,"init_events")||
      (weed_get_voidptr_value(filter_map,"init_events",&error)==NULL)) {
    return;
  }
  mainw->pchains=get_event_pchains();
  if ((num_inst=weed_leaf_num_elements(filter_map,"init_events"))>0) {
    init_events=weed_get_voidptr_array(filter_map,"init_events",&error);
    for (i=0; i<num_inst; i++) {
      init_event=(weed_plant_t *)init_events[i];
      fhash=weed_get_string_value(init_event,"filter",&error);
      filter=get_weed_filter(weed_get_idx_for_hashname(fhash,TRUE));
      lives_free(fhash);
      if (has_audio_chans_in(filter,FALSE)&&!has_video_chans_in(filter,FALSE)&&!has_video_chans_out(filter,FALSE)) {
        filter_error=weed_apply_audio_instance(init_event,abuf,nbtracks,nchans,nsamps,arate,tc,vis);
        filter_error=filter_error; // stop compiler complaining
      }
      // TODO *** - also run any pure data processing filters which feed into audio filters

    }
    lives_free(init_events);
  }
  mainw->pchains=NULL;
}



void weed_apply_audio_effects_rt(float **abuf, int nchans, int64_t nsamps, double arate, weed_timecode_t tc, boolean analysers_only) {

  weed_plant_t *instance,*filter;

  lives_filter_error_t filter_error;

  int error;

  boolean needs_reinit;

  register int i;

  // free playback: apply any audio filters or analysers (but not generators)
  // Effects are applied in key order


  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i+1,TRUE)) {
      if (!(mainw->rte&(GU641<<i))) {
        // if anything is connected to ACTIVATE, the fx may be activated
        pconx_chain_data(i,key_modes[i]);
      }
      if (mainw->rte&(GU641<<i)) {
        mainw->osc_block=TRUE;

        // filter must not be deinited until we have processed it
        if (pthread_mutex_trylock(&mainw->data_mutex[i])) continue;

        if ((instance=key_to_instance[i][key_modes[i]])==NULL) {
          filter_mutex_unlock(i);
          continue;
        }
        filter=weed_instance_get_filter(instance,FALSE);

        if (!has_audio_chans_in(filter,FALSE)||has_video_chans_in(filter,FALSE)||has_video_chans_out(filter,FALSE)) {
          filter_mutex_unlock(i);
          continue;
        }

        if (analysers_only&&has_audio_chans_out(filter,FALSE)) {
          filter_mutex_unlock(i);
          continue;
        }

        if (mainw->pchains!=NULL&&mainw->pchains[i]!=NULL) {
          interpolate_params(instance,mainw->pchains[i],tc); // interpolate parameters during preview
        }

apply_audio_inst2:

        if (mainw->pconx!=NULL) {
          // chain any data pipelines

          if (!pthread_mutex_trylock(&mainw->data_mutex[i])) {
            needs_reinit=pconx_chain_data(i,key_modes[i]);

            // if anything is connected to ACTIVATE, the fx may be deactivated
            if ((instance=key_to_instance[i][key_modes[i]])==NULL) {
              filter_mutex_unlock(i);
              continue;
            }

            if (needs_reinit) {
              weed_reinit_effect(instance,FALSE);
            }

            filter_mutex_unlock(i);
          }
        }

        //weed_set_int_value(instance,"host_key",i);

        if (weed_plant_has_leaf(instance,"host_next_instance")) {
          // chain any internal data pipelines for compound fx
          if (!pthread_mutex_trylock(&mainw->data_mutex[i])) {
            needs_reinit=pconx_chain_data_internal(instance);
            filter_mutex_unlock(i);
            if (needs_reinit) {
              weed_reinit_effect(instance,FALSE);
            }
          }
        }

        filter_error=weed_apply_audio_instance(instance,abuf,0,nchans,nsamps,arate,tc,NULL);

        if (filter_error==WEED_NO_ERROR&&weed_plant_has_leaf(instance,"host_next_instance")) {
          // handling for compound fx
          instance=weed_get_plantptr_value(instance,"host_next_instance",&error);

          goto apply_audio_inst2;
        }

        filter_mutex_unlock(i);

        if (filter_error==FILTER_INFO_REINITED) redraw_pwindow(i,key_modes[i]); // redraw our paramwindow
#ifdef DEBUG_RTE
        if (filter_error!=FILTER_NO_ERROR) lives_printerr("Render error was %d\n",filter_error);
#endif
      }
    }
  }

}




boolean has_audio_filters(lives_af_t af_type) {
  // do we have any active audio filters (excluding audio generators) ?
  weed_plant_t *instance,*filter;

  register int i;

  if (af_type==AF_TYPE_A&&mainw->audio_frame_buffer!=NULL) return TRUE;

  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i+1,TRUE)) {
      if (mainw->rte&(GU641<<i)) {
        if ((instance=key_to_instance[i][key_modes[i]])==NULL) continue;
        filter=weed_instance_get_filter(instance,FALSE);
        if (has_audio_chans_in(filter,FALSE)&&!has_video_chans_in(filter,FALSE)&&!has_video_chans_out(filter,FALSE)) {
          if ((af_type==AF_TYPE_A&&has_audio_chans_out(filter,FALSE))||  // check for analysers only
              (af_type==AF_TYPE_NONA&&!has_audio_chans_out(filter,FALSE))) // check for non-analysers only
            continue;
          return TRUE;
        }
      }
    }
  }
  return FALSE;
}


boolean has_video_filters(boolean analysers_only) {
  // do we have any active video filters (excluding generators) ?
  weed_plant_t *instance,*filter;

  register int i;

  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i+1,TRUE)) {
      if (mainw->rte&(GU641<<i)) {
        if ((instance=key_to_instance[i][key_modes[i]])==NULL) continue;
        filter=weed_instance_get_filter(instance,FALSE);
        if (has_video_chans_in(filter,FALSE)) {
          if (analysers_only&&has_video_chans_out(filter,FALSE)) continue;
          return TRUE;
        }
      }
    }
  }
  return FALSE;
}


/////////////////////////////////////////////////////////////////////////


static int check_weed_plugin_info(weed_plant_t *plugin_info) {
  // verify the plugin_info returned from the plugin
  // TODO - print descriptive errors
  if (!weed_plant_has_leaf(plugin_info,"host_info")) return -1;
  if (!weed_plant_has_leaf(plugin_info,"version")) return -2;
  if (!weed_plant_has_leaf(plugin_info,"filters")) return -3;
  return weed_leaf_num_elements(plugin_info,"filters");
}


int num_in_params(weed_plant_t *plant, boolean skip_hidden, boolean skip_internal) {
  weed_plant_t **params=NULL;

  weed_plant_t *param;

  int counted=0;
  int num_params,i,error;
  boolean is_template=(WEED_PLANT_IS_FILTER_CLASS(plant));

nip1:

  num_params=0;

  if (is_template) {
    if (!weed_plant_has_leaf(plant,"in_parameter_templates")||
        weed_get_plantptr_value(plant,"in_parameter_templates",&error)==NULL) return 0;
    num_params=weed_leaf_num_elements(plant,"in_parameter_templates");
  } else {
    if (!weed_plant_has_leaf(plant,"in_parameters")) goto nip1done;
    if (weed_get_plantptr_value(plant,"in_parameters",&error)==NULL) goto nip1done;
    num_params=weed_leaf_num_elements(plant,"in_parameters");
  }

  if (!skip_hidden&&!skip_internal) {
    counted+=num_params;
    goto nip1done;
  }

  if (is_template) params=weed_get_plantptr_array(plant,"in_parameter_templates",&error);
  else params=weed_get_plantptr_array(plant,"in_parameters",&error);

  for (i=0; i<num_params; i++) {
    if (skip_hidden&&is_hidden_param(plant,i)) continue;
    param=params[i];
    if (skip_internal&&weed_plant_has_leaf(param,"host_internal_connection")) continue;
    counted++;
  }

  lives_free(params);

nip1done:

  if (!is_template&&skip_internal&&weed_plant_has_leaf(plant,"host_next_instance")) {
    plant=weed_get_plantptr_value(plant,"host_next_instance",&error);
    goto nip1;
  }

  return counted;
}


int num_out_params(weed_plant_t *plant) {
  int num_params,error;
  boolean is_template=(WEED_PLANT_IS_FILTER_CLASS(plant));

  if (is_template) {
    if (!weed_plant_has_leaf(plant,"out_parameter_templates")||
        weed_get_plantptr_value(plant,"out_parameter_templates",&error)==NULL) return 0;
    num_params=weed_leaf_num_elements(plant,"out_parameter_templates");
  } else {
    if (!weed_plant_has_leaf(plant,"out_parameters")) return 0;
    if (weed_get_plantptr_value(plant,"out_parameters",&error)==NULL) return 0;
    num_params=weed_leaf_num_elements(plant,"out_parameters");
  }
  return num_params;
}

boolean has_usable_palette(weed_plant_t *chantmpl) {
  int error;
  int palette=weed_get_int_value(chantmpl,"current_palette",&error);
  // currently only integer RGB palettes are usable
  if (palette==5||palette==6) return FALSE;
  if (palette>0&&palette<=7) return TRUE;
  return FALSE;
}



int enabled_in_channels(weed_plant_t *plant, boolean count_repeats) {
  // count number of non-disabled in channels (video and/or audio) for a filter or instance

  // NOTE: for instances, we do not count optional audio channels, even if they are enabled


  weed_plant_t **channels=NULL;
  boolean is_template=WEED_PLANT_IS_FILTER_CLASS(plant);
  int enabled=0;
  int num_channels,error;

  register int i;

  if (is_template) {
    if (!weed_plant_has_leaf(plant,"in_channel_templates")) return 0;
    num_channels=weed_leaf_num_elements(plant,"in_channel_templates");
    if (num_channels>0) channels=weed_get_plantptr_array(plant,"in_channel_templates",&error);
  } else {
    if (!weed_plant_has_leaf(plant,"in_channels")) return 0;
    num_channels=weed_leaf_num_elements(plant,"in_channels");
    if (num_channels>0) channels=weed_get_plantptr_array(plant,"in_channels",&error);
  }

  for (i=0; i<num_channels; i++) {
    if (!is_template) {
      weed_plant_t *ctmpl=weed_get_plantptr_value(channels[i],"template",&error);
      if (weed_plant_has_leaf(ctmpl,"is_audio")&&weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE) {
        if (weed_plant_has_leaf(ctmpl,"optional")&&weed_get_boolean_value(ctmpl,"optional",&error)==WEED_TRUE) continue;
      }

      if (!weed_plant_has_leaf(channels[i],"disabled")||
          weed_get_boolean_value(channels[i],"disabled",&error)!=WEED_TRUE) enabled++;
    } else {
      if (!weed_plant_has_leaf(channels[i],"host_disabled")||
          weed_get_boolean_value(channels[i],"host_disabled",&error)!=WEED_TRUE) enabled++;
    }

    if (count_repeats) {
      // count repeated channels
      weed_plant_t *chantmpl;
      int repeats;
      if (is_template) chantmpl=channels[i];
      else chantmpl=weed_get_plantptr_value(channels[i],"template",&error);
      if (weed_plant_has_leaf(channels[i],"max_repeats")) {
        if (weed_plant_has_leaf(channels[i],"disabled")&&
            weed_get_boolean_value(channels[i],"disabled",&error)==WEED_TRUE&&
            !has_usable_palette(chantmpl)) continue; // channel was disabled because palette is unusable
        repeats=weed_get_int_value(channels[i],"max_repeats",&error)-1;
        if (repeats==-1) repeats=1000000;
        enabled+=repeats;
      }
    }
  }

  if (channels!=NULL) lives_free(channels);

  return enabled;
}


int enabled_out_channels(weed_plant_t *plant, boolean count_repeats) {
  weed_plant_t **channels=NULL;
  int enabled=0;
  int num_channels,i,error;
  boolean is_template=WEED_PLANT_IS_FILTER_CLASS(plant);

  if (is_template) {
    num_channels=weed_leaf_num_elements(plant,"out_channel_templates");
    if (num_channels>0) channels=weed_get_plantptr_array(plant,"out_channel_templates",&error);
  } else {
    num_channels=weed_leaf_num_elements(plant,"out_channels");
    if (num_channels>0) channels=weed_get_plantptr_array(plant,"out_channels",&error);
  }

  for (i=0; i<num_channels; i++) {
    if (!is_template) {
      if (!weed_plant_has_leaf(channels[i],"disabled")||
          weed_get_boolean_value(channels[i],"disabled",&error)!=WEED_TRUE) enabled++;
    } else {
      if (!weed_plant_has_leaf(channels[i],"host_disabled")||
          weed_get_boolean_value(channels[i],"host_disabled",&error)!=WEED_TRUE) enabled++;
    }
    if (count_repeats) {
      // count repeated channels
      weed_plant_t *chantmpl;
      int repeats;
      if (is_template) chantmpl=channels[i];
      else chantmpl=weed_get_plantptr_value(channels[i],"template",&error);
      if (weed_plant_has_leaf(channels[i],"max_repeats")) {
        if (weed_plant_has_leaf(channels[i],"disabled")&&
            weed_get_boolean_value(channels[i],"disabled",&error)==WEED_TRUE&&
            !has_usable_palette(chantmpl)) continue; // channel was disabled because palette is unusable
        repeats=weed_get_int_value(channels[i],"max_repeats",&error)-1;
        if (repeats==-1) repeats=1000000;
        enabled+=repeats;
      }
    }
  }

  if (channels!=NULL) lives_free(channels);

  return enabled;
}






/////////////////////////////////////////////////////////////////////



static int check_for_lives(weed_plant_t *filter, int filter_idx) {
  // for LiVES, currently:
  // all filters must take 0, 1 or 2 mandatory/optional inputs and provide
  // 1 mandatory output (audio or video) or >1 optional (video only) outputs (for now)

  // or 1 or more mandatory alpha outputs (analysers)

  // filters can also have 1 mandatory input and no outputs and out parameters
  // (video analyzer)

  // they may have no outs provided they have out parameters (data sources or processors)

  // all channels used must support a limited range of palettes (for now)

  // filters can now have any number of mandatory in alphas, the effect will not be run unless the channels are filled
  // (by chaining to output of another fx)


  int chans_in_mand=0; // number of mandatory channels
  int chans_in_opt_max=0; // number of usable (by LiVES) optional channels
  int chans_out_mand=0;
  int chans_out_opt_max=0;
  int achans_in_mand=0,achans_out_mand=0;
  boolean is_audio=FALSE;
  boolean has_out_params=FALSE;
  boolean all_out_alpha=TRUE;
  boolean hidden=FALSE;

  int error,flags=0;
  int num_elements,i;
  int naudins=0,naudouts=0;
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

  for (i=0; i<num_elements; i++) {
#ifndef HAVE_POSIX_MEMALIGN
    if (weed_plant_has_leaf(array[i],"alignment")) {
      lives_free(array);
      return 12;
    }
#endif
    if (weed_plant_has_leaf(array[i],"is_audio")&&weed_get_boolean_value(array[i],"is_audio",&error)==WEED_TRUE) {
      if (weed_plant_has_leaf(array[i],"audio_channels")&&(naudins=weed_get_int_value(array[i],"audio_channels",&error))>2) {
        // currently we only handle mono and stereo audio filters
        lives_free(array);
        return 7;
      }
      is_audio=TRUE;
    }
    if (!weed_plant_has_leaf(array[i],"name")||(!weed_plant_has_leaf(array[i],"palette_list")&&!is_audio)) {
      lives_free(array);
      return 6;
    }

    if (!weed_plant_has_leaf(array[i],"flags")) weed_set_int_value(array[i],"flags",0);

    if (weed_plant_has_leaf(array[i],"optional")&&weed_get_boolean_value(array[i],"optional",&error)==WEED_TRUE) {
      // is optional
      chans_in_opt_max++;
      weed_set_boolean_value(array[i],"host_disabled",WEED_TRUE);
    } else {
      if (has_non_alpha_palette(array[i])) {
        if (!is_audio) chans_in_mand++;
        else achans_in_mand++;
      }
    }
  }
  if (num_elements>0) lives_free(array);
  if (chans_in_mand>2) return 8; // we dont handle mixers yet...
  if (achans_in_mand>0&&chans_in_mand>0) return 13; // can't yet handle effects that need both audio and video

  // count number of mandatory and optional out_channels
  if (!weed_plant_has_leaf(filter,"out_channel_templates")) num_elements=0;
  else num_elements=weed_leaf_num_elements(filter,"out_channel_templates");

  if (num_elements>0) array=weed_get_plantptr_array(filter,"out_channel_templates",&error);

  for (i=0; i<num_elements; i++) {
#ifndef HAVE_POSIX_MEMALIGN
    if (weed_plant_has_leaf(array[i],"alignment")) {
      lives_free(array);
      return 12;
    }
#endif
    if (weed_plant_has_leaf(array[i],"is_audio")&&weed_get_boolean_value(array[i],"is_audio",&error)==WEED_TRUE) {
      if (weed_plant_has_leaf(array[i],"audio_channels")&&(naudouts=weed_get_int_value(array[i],"audio_channels",&error))>2) {
        // currently we only handle mono and stereo audio filters
        lives_free(array);
        return 7;
      }
      if (naudins==1&&naudouts==2) {
        // converting mono to stereo cannot be done like that
        lives_free(array);
        return 7;
      }
      is_audio=TRUE;
    }
    if (!weed_plant_has_leaf(array[i],"name")||(!weed_plant_has_leaf(array[i],"palette_list")&&!is_audio)) {
      lives_free(array);
      return 9;
    }

    if (!weed_plant_has_leaf(array[i],"flags")) weed_set_int_value(array[i],"flags",0);

    if (weed_plant_has_leaf(array[i],"optional")&&weed_get_boolean_value(array[i],"optional",&error)==WEED_TRUE) {
      // is optional
      chans_out_opt_max++;
      weed_set_boolean_value(array[i],"host_disabled",WEED_TRUE);
    } else {
      // is mandatory
      if (!is_audio) {
        chans_out_mand++;
        if (has_non_alpha_palette(array[i])) all_out_alpha=FALSE;
      } else achans_out_mand++;
    }
  }
  if (num_elements>0) lives_free(array);
  if (weed_plant_has_leaf(filter,"out_parameter_templates")) has_out_params=TRUE;

  if ((chans_out_mand>1&&!all_out_alpha)||((chans_out_mand+chans_out_opt_max+achans_out_mand<1)
      &&(!has_out_params))) return 11;
  if (achans_out_mand>1||(achans_out_mand==1&&chans_out_mand>0)) return 14;
  if (achans_in_mand>=1&&achans_out_mand==0&&!(has_out_params)) return 15;

  weed_add_plant_flags(filter,WEED_LEAF_READONLY_PLUGIN);
  if (weed_plant_has_leaf(filter,"gui")) {
    weed_plant_t *gui=weed_get_plantptr_value(filter,"gui",&error);
    weed_add_plant_flags(gui,WEED_LEAF_READONLY_PLUGIN);
    if (weed_plant_has_leaf(gui,"hidden")&&weed_get_boolean_value(gui,"hidden",&error)==WEED_TRUE)
      hidden=TRUE;
  }

  if (hidden||(flags&WEED_FILTER_IS_CONVERTER)) {
    if (is_audio) {
      weed_set_boolean_value(filter,"host_menu_hide",WEED_TRUE);
      if (enabled_in_channels(filter,TRUE)>=1000000) {
        // this is a candidate for audio volume
        lives_fx_candidate_t *cand=&mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL];
        cand->list=lives_list_append(cand->list,LIVES_INT_TO_POINTER(filter_idx));
        cand->delegate=0;
      }
    } else {
      weed_set_boolean_value(filter,"host_menu_hide",WEED_TRUE);
      if (chans_in_mand==1&&chans_out_mand==1) {
        weed_plant_t *fstout=get_mandatory_channel(filter,0,FALSE);
        int ochan_flags=weed_get_int_value(fstout,"flags",&error);
        if (ochan_flags&WEED_CHANNEL_SIZE_CAN_VARY) {
          // this is a candidate for resize
          lives_fx_candidate_t *cand=&mainw->fx_candidates[FX_CANDIDATE_RESIZER];
          cand->list=lives_list_append(cand->list,LIVES_INT_TO_POINTER(filter_idx));
          cand->delegate=0;
        }
      }
    }
  }

  return 0;
}



static boolean set_in_channel_palettes(weed_plant_t *filter, int num_channels) {
  // set in channel palettes for filter
  // we also enable optional channels if we have to
  // in this case we fill first the mandatory channels,
  // then if necessary the optional channels

  // we return FALSE if we could not satisfy the request
  weed_plant_t **chantmpls=NULL;
  int def_palette;

  int num_elements,i,error;

  if (!weed_plant_has_leaf(filter,"in_channel_templates")) {
    if (num_channels>0) return FALSE;
    return TRUE;
  }

  num_elements=weed_leaf_num_elements(filter,"in_channel_templates");

  if (num_elements<num_channels) return FALSE;

  if (num_elements==0) return TRUE;

  chantmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);

  // our start state is with all optional channels disabled

  // fill mandatory channels first; these palettes may change later if we get a frame in a different palette
  for (i=0; i<num_elements; i++) {
    if (!weed_plant_has_leaf(chantmpls[i],"is_audio")||weed_get_boolean_value(chantmpls[i],"is_audio",&error)!=WEED_TRUE) {
      int num_palettes=weed_leaf_num_elements(chantmpls[i],"palette_list");
      int *palettes=weed_get_int_array(chantmpls[i],"palette_list",&error);
      if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_RGB24)==WEED_PALETTE_RGB24)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGB24);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_BGR24)==WEED_PALETTE_BGR24)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_BGR24);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_RGBA32)==WEED_PALETTE_RGBA32)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGBA32);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_BGRA32)==WEED_PALETTE_BGRA32)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_BGRA32);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_ARGB32)==WEED_PALETTE_ARGB32)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_ARGB32);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV888)==WEED_PALETTE_YUV888)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV888);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV444P)==WEED_PALETTE_YUV444P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV444P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUVA8888)==WEED_PALETTE_YUVA8888)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUVA8888);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUVA4444P)==WEED_PALETTE_YUVA4444P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUVA4444P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_UYVY8888)==WEED_PALETTE_UYVY8888)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_UYVY8888);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUYV8888)==WEED_PALETTE_YUYV8888)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUYV8888);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV422P)==WEED_PALETTE_YUV422P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV422P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV420P)==WEED_PALETTE_YUV420P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV420P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YVU420P)==WEED_PALETTE_YVU420P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YVU420P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV411)==WEED_PALETTE_YUV411)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV411);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_AFLOAT)==WEED_PALETTE_AFLOAT)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_AFLOAT);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_A8)==WEED_PALETTE_A8)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_A8);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_A1)==WEED_PALETTE_A1)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_A1);
      else if (!weed_plant_has_leaf(chantmpls[i],"optional")) {
        if (chantmpls!=NULL) lives_free(chantmpls);
        lives_free(palettes);
        return FALSE; // mandatory channel; we don't yet handle rgb float
      }
      /*else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_RGBFLOAT)==WEED_PALETTE_RGBFLOAT)
      weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGBFLOAT);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_ARGBFLOAT)==WEED_PALETTE_ARGBFLOAT)
      weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_ARGBFLOAT);
      */
      lives_free(palettes);
    }

    if (!weed_plant_has_leaf(chantmpls[i],"optional")) num_channels--; // mandatory channel
  }

  if (num_channels>0) {
    // OK we need to use some optional channels
    for (i=0; i<num_elements&&num_channels>0; i++) if (weed_plant_has_leaf(chantmpls[i],"optional")) {
        weed_set_boolean_value(chantmpls[i],"host_disabled",WEED_FALSE);
        num_channels--;
      }
  }
  if (num_channels>0) {
    if (chantmpls!=NULL) lives_free(chantmpls);
    return FALSE;
  }

  // now we set match channels
  if (!weed_plant_has_leaf(chantmpls[0],"is_audio")||weed_get_boolean_value(chantmpls[0],"is_audio",&error)!=WEED_TRUE) {
    def_palette=weed_get_int_value(chantmpls[0],"current_palette",&error);
    for (i=1; i<num_elements; i++) {
      int channel_flags=weed_get_int_value(chantmpls[i],"flags",&error);
      if (!(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY)) {
        int num_palettes=weed_leaf_num_elements(chantmpls[i],"palette_list");
        int *palettes=weed_get_int_array(chantmpls[i],"palette_list",&error);
        if (check_weed_palette_list(palettes,num_palettes,def_palette)==def_palette)
          weed_set_int_value(chantmpls[i],"current_palette",def_palette);
        else {
          if (chantmpls!=NULL) lives_free(chantmpls);
          lives_free(palettes);
          return FALSE;
        }
        lives_free(palettes);
      }
    }
  }
  if (chantmpls!=NULL) lives_free(chantmpls);
  return TRUE;
}



static boolean set_out_channel_palettes(weed_plant_t *filter, int num_channels) {
  // set in channel palettes for filter
  // we also enable optional channels if we have to
  // in this case we fill first the mandatory channels,
  // then if necessary the optional channels

  // we return FALSE if we could not satisfy the request
  weed_plant_t **chantmpls=NULL;
  weed_plant_t **in_chantmpls=NULL;

  int num_elements=weed_leaf_num_elements(filter,"out_channel_templates"),i,error;
  int num_in_elements=weed_leaf_num_elements(filter,"in_channel_templates");

  int def_palette=WEED_PALETTE_END;

  if (num_elements<num_channels) return FALSE;

  chantmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
  // our start state is with all optional channels disabled

  // fill mandatory channels first; these palettes may change later if we get a frame in a different palette
  for (i=0; i<num_elements; i++) {
    if (!weed_plant_has_leaf(chantmpls[i],"is_audio")||weed_get_boolean_value(chantmpls[i],"is_audio",&error)!=WEED_TRUE) {
      int num_palettes=weed_leaf_num_elements(chantmpls[i],"palette_list");
      int *palettes=weed_get_int_array(chantmpls[i],"palette_list",&error);
      if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_RGB24)==WEED_PALETTE_RGB24)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGB24);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_BGR24)==WEED_PALETTE_BGR24)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_BGR24);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_RGBA32)==WEED_PALETTE_RGBA32)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGBA32);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_BGRA32)==WEED_PALETTE_BGRA32)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_BGRA32);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_ARGB32)==WEED_PALETTE_ARGB32)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_ARGB32);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV888)==WEED_PALETTE_YUV888)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV888);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV444P)==WEED_PALETTE_YUV444P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV444P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUVA8888)==WEED_PALETTE_YUVA8888)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUVA8888);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUVA4444P)==WEED_PALETTE_YUVA4444P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUVA4444P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_UYVY8888)==WEED_PALETTE_UYVY8888)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_UYVY8888);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUYV8888)==WEED_PALETTE_YUYV8888)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUYV8888);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV422P)==WEED_PALETTE_YUV422P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV422P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV420P)==WEED_PALETTE_YUV420P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV420P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YVU420P)==WEED_PALETTE_YVU420P)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YVU420P);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_YUV411)==WEED_PALETTE_YUV411)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_YUV411);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_AFLOAT)==WEED_PALETTE_AFLOAT)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_AFLOAT);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_A8)==WEED_PALETTE_A8)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_A8);
      else if (check_weed_palette_list(palettes,num_palettes,WEED_PALETTE_A1)==WEED_PALETTE_A1)
        weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_A1);
      else if (!weed_plant_has_leaf(chantmpls[i],"optional")) {
        if (chantmpls!=NULL) lives_free(chantmpls);
        lives_free(palettes);
        return FALSE; // mandatory channel; we don't yet handle rgb float
      }

      /*	else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_RGBFLOAT)==WEED_PALETTE_RGBFLOAT)
      weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_RGBFLOAT);
      else if (check_weed_palette_list (palettes,num_palettes,WEED_PALETTE_ARGBFLOAT)==WEED_PALETTE_ARGBFLOAT)
      weed_set_int_value(chantmpls[i],"current_palette",WEED_PALETTE_ARGBFLOAT);
      */
      lives_free(palettes);
    }
    if (!weed_plant_has_leaf(chantmpls[i],"optional")) num_channels--; // mandatory channel
  }

  if (num_channels>0) {
    // OK we need to use some optional channels
    for (i=0; i<num_elements&&num_channels>0; i++) if (weed_plant_has_leaf(chantmpls[i],"optional")) {
        weed_set_boolean_value(chantmpls[i],"host_disabled",WEED_FALSE);
        num_channels--;
      }
  }

  if (num_channels>0) {
    if (chantmpls!=NULL) lives_free(chantmpls);
    return FALSE;
  }

  // now we set match channels
  if (num_in_elements) {
    in_chantmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
    if (!weed_plant_has_leaf(in_chantmpls[0],"is_audio")||
        weed_get_boolean_value(in_chantmpls[0],"is_audio",&error)!=WEED_TRUE) {
      def_palette=weed_get_int_value(in_chantmpls[0],"current_palette",&error);
    }
    lives_free(in_chantmpls);
  } else if (!weed_plant_has_leaf(chantmpls[0],"is_audio")||
             weed_get_boolean_value(chantmpls[0],"is_audio",&error)!=WEED_TRUE) {
    def_palette=weed_get_int_value(chantmpls[0],"current_palette",&error);
  }
  if (def_palette!=WEED_PALETTE_END) {
    for (i=0; i<num_elements; i++) {
      int channel_flags=weed_get_int_value(chantmpls[i],"flags",&error);
      if (!(channel_flags&WEED_CHANNEL_PALETTE_CAN_VARY)) {
        int num_palettes=weed_leaf_num_elements(chantmpls[i],"palette_list");
        int *palettes=weed_get_int_array(chantmpls[i],"palette_list",&error);
        if (check_weed_palette_list(palettes,num_palettes,def_palette)==def_palette)
          weed_set_int_value(chantmpls[i],"current_palette",def_palette);
        else {
          if (chantmpls!=NULL) lives_free(chantmpls);
          lives_free(palettes);
          return FALSE;
        }
        lives_free(palettes);
      }
    }
  }
  if (chantmpls!=NULL) lives_free(chantmpls);
  return TRUE;
}




static void load_weed_plugin(char *plugin_name, char *plugin_path, char *dir) {
  weed_setup_f setup_fn;
  weed_bootstrap_f bootstrap=(weed_bootstrap_f)&weed_bootstrap_func;

  weed_plant_t *plugin_info,**filters,*filter;

  void *handle;

  int error,reason,oidx,idx=num_weed_filters;

  int filters_in_plugin;
  int mode=-1,kmode=0;
  int phashes;

  char **phashnames;

  char cwd[PATH_MAX];

  char *pwd;

  char *filter_name;

  register int i;

  static int key=-1;

  pwd=getcwd(cwd,PATH_MAX);

  key++;

  mainw->chdir_failed=FALSE;

  // walk list and create fx structures
  //  #define DEBUG_WEED
#ifdef DEBUG_WEED
  lives_printerr("Checking plugin %s\n",plugin_path);
#endif

  if ((handle=dlopen(plugin_path,RTLD_LAZY))) {
    dlerror(); // clear existing errors

    if ((setup_fn=(weed_setup_f)dlsym(handle,"weed_setup"))==NULL) {
      lives_printerr(_("Error: plugin %s has no weed_setup() function.\n"),plugin_path);
    } else {

      // here we call the plugin's setup_fn, passing in our bootstrap function
      // the plugin will call our bootstrap function to get the correct versions of the core weed functions
      // and bootstrap itself

      // if we use the plugin, we must not free the plugin_info, since the plugin has a reference to this

      // chdir to plugin dir, in case it needs to load data

      lives_chdir(dir,TRUE);
      plugin_info=(*setup_fn)(bootstrap);
      if (plugin_info==NULL||(filters_in_plugin=check_weed_plugin_info(plugin_info))<1) {
        char *msg=lives_strdup_printf(_("No usable filters found in plugin %s\n"),plugin_path);
        LIVES_INFO(msg);
        lives_free(msg);
        if (plugin_info!=NULL) weed_plant_free(plugin_info);
        dlclose(handle);
        lives_chdir(pwd,FALSE);
        return;
      }

      weed_set_voidptr_value(plugin_info,"handle",handle);
      weed_set_string_value(plugin_info,"name",plugin_name); // for hashname
      weed_set_string_value(plugin_info,"plugin_path",dir);
      weed_add_plant_flags(plugin_info,WEED_LEAF_READONLY_PLUGIN);

      filters=weed_get_plantptr_array(plugin_info,"filters",&error);

      oidx=idx;
      phashnames=NULL;
      phashes=0;

      while (mode<filters_in_plugin-1) {
        mode++;
        filter=filters[mode];

        filter_name=weed_get_string_value(filter,"name",&error);
        if (!(reason=check_for_lives(filter,idx))) {
          boolean dup=FALSE,pdup=FALSE;

          num_weed_filters++;
          weed_filters=(weed_plant_t **)lives_realloc(weed_filters,num_weed_filters*sizeof(weed_plant_t *));
          weed_filters[idx]=filter;

          hashnames=(char **)lives_realloc(hashnames,num_weed_filters*sizeof(char *));
          hashnames[idx]=NULL;
          hashnames[idx]=make_weed_hashname(idx,TRUE,FALSE);

          phashes++;
          phashnames=(char **)lives_realloc(phashnames,phashes*sizeof(char *));
          phashnames[phashes-1]=make_weed_hashname(idx,FALSE,FALSE);

          for (i=0; i<idx; i++) {
            if (i>oidx) {
              if (!lives_utf8_strcasecmp(phashnames[phashes-1],phashnames[i-oidx-1])) {
                //g_print("partial dupe: %s and %s\n",phashnames[phashes-1],phashnames[i-oidx-1]);
                // found a partial match: author and/or version differ
                // if in the same plugin, we will use the highest version no., others become dupes
                num_weed_dupes++;
                dupe_weed_filters=(weed_plant_t **)lives_realloc(dupe_weed_filters,num_weed_dupes*sizeof(weed_plant_t *));
                dupe_hashnames=(char **)lives_realloc(dupe_hashnames,num_weed_dupes*sizeof(char *));
                if (weed_get_int_value(filter,"version",&error)<
                    weed_get_int_value(weed_filters[i],"version",&error)) {
                  // add idx to dupe list
                  dupe_weed_filters[num_weed_dupes-1]=filter;
                  dupe_hashnames[num_weed_dupes-1]=hashnames[idx];
                } else {
                  // add i to dupe list
                  dupe_weed_filters[num_weed_dupes-1]=weed_filters[i];
                  dupe_hashnames[num_weed_dupes-1]=hashnames[i];
                  weed_filters[i]=filter;
                  hashnames[i]=hashnames[idx];
                }
                phashes--;
                lives_free(phashnames[phashes]);
                num_weed_filters--;
                weed_filters=(weed_plant_t **)lives_realloc(weed_filters,num_weed_filters*sizeof(weed_plant_t *));
                hashnames=(char **)lives_realloc(hashnames,num_weed_filters*sizeof(char *));
                phashnames=(char **)lives_realloc(phashnames,phashes*sizeof(char *));
                pdup=TRUE;
                break;
              }
            }
            if (!lives_utf8_strcasecmp(hashnames[idx],hashnames[i])) {
              // skip dups
              char *msg=lives_strdup_printf(_("Found duplicate plugin %s"),hashnames[idx]);
              LIVES_INFO(msg);
              lives_free(msg);
              lives_free(hashnames[idx]);
              phashes--;
              lives_free(phashnames[phashes]);
              num_weed_filters--;
              weed_filters=(weed_plant_t **)lives_realloc(weed_filters,num_weed_filters*sizeof(weed_plant_t *));
              hashnames=(char **)lives_realloc(hashnames,num_weed_filters*sizeof(char *));
              phashnames=(char **)lives_realloc(phashnames,phashes*sizeof(char *));
              dup=TRUE;
              break;
            }
          }
          if (dup) {
            lives_free(filter_name);
            continue;
          }

          // we start with all optional channels disabled (unless forced to use them)
          set_in_channel_palettes(filter,enabled_in_channels(filters[mode],FALSE));
          set_out_channel_palettes(filter,1);

          // skip hidden/duplicate filters
          if (!pdup&&key<FX_KEYS_PHYSICAL&&kmode<prefs->max_modes_per_key&&
              (!weed_plant_has_leaf(filter,"host_menu_hide")||
               (weed_get_boolean_value(filter,"host_menu_hide",&error)==WEED_FALSE))) {
            key_to_fx[key][kmode++]=idx;

#ifdef DEBUG_WEED
            if (!pdup&&key<FX_KEYS_PHYSICAL&&kmode<prefs->max_modes_per_key)
              d_print("Loaded filter \"%s\" in plugin \"%s\"; assigned to key ctrl-%d, mode %d.\n",
                      filter_name,plugin_name,key+1,kmode);

          } else {
            d_print("Loaded filter \"%s\" in plugin \"%s\", no key assigned\n",filter_name,plugin_name);
#endif
          }
          if (!pdup) idx++;



        }
#ifdef DEBUG_WEED
        else lives_printerr("Unsuitable filter \"%s\" in plugin \"%s\", reason code %d\n",filter_name,plugin_name,reason);
#endif
        lives_free(filter_name);
      }
      for (i=0; i<phashes; i++) {
        lives_free(phashnames[i]);
      }
      lives_free(phashnames);
      lives_free(filters);
    }

  } else lives_printerr(_("Info: Unable to load plugin %s\nError was: %s\n"),plugin_path,dlerror());

  if (mainw->chdir_failed) {
    char *dirs=lives_strdup(_("Some plugin directories"));
    do_chdir_failed_error(dirs);
    lives_free(dirs);
  }

  lives_chdir(pwd,FALSE);

  // TODO - add any rendered effects to fx submenu
}


void weed_memory_init(void) {
#ifndef IS_LIBLIVES
  weed_init(110,(weed_malloc_f)_lives_malloc,(weed_free_f)_lives_free_normal,(weed_memcpy_f)lives_memcpy,(weed_memset_f)lives_memset);
#endif
}



static void merge_dupes(void) {
  register int i;

  weed_filters=(weed_plant_t **)lives_realloc(weed_filters,(num_weed_filters+num_weed_dupes)*sizeof(weed_plant_t *));
  hashnames=(char **)lives_realloc(hashnames,(num_weed_filters+num_weed_dupes)*sizeof(char *));

  for (i=num_weed_filters; i<num_weed_filters+num_weed_dupes; i++) {
    weed_filters[i]=dupe_weed_filters[i-num_weed_filters];
    hashnames[i]=dupe_hashnames[i-num_weed_filters];
  }

  if (dupe_weed_filters!=NULL) lives_free(dupe_weed_filters);
  if (dupe_hashnames!=NULL) lives_free(dupe_hashnames);

  num_weed_filters+=num_weed_dupes;
}


static void make_fx_defs_menu(void) {
  weed_plant_t *filter;

  LiVESWidget *menuitem,*menu;
  LiVESWidget *pkg_menu;
  LiVESWidget *pkg_submenu=NULL;

  char *string,*filter_type,*filter_name;
  char *pkg=NULL,*pkgstring;

  int pkg_posn=0,error;

  register int i;


  // menu entries for vj/set defs
  for (i=0; i<num_weed_filters-num_weed_dupes; i++) {
    filter=weed_filters[i];

    // skip hidden filters
    if (!weed_plant_has_leaf(filter,"host_menu_hide")||
        (weed_get_boolean_value(filter,"host_menu_hide",&error)==WEED_FALSE)) {

      if ((!has_video_chans_in(filter,FALSE)&&has_video_chans_out(filter,FALSE))||
          num_in_params(filter,TRUE,TRUE)>0) {
        filter_name=weed_get_string_value(filter,"name",&error);
        if ((pkgstring=strstr(filter_name,": "))!=NULL) {
          // package effect
          if (pkg!=NULL&&strncmp(pkg,filter_name,strlen(pkg))) {
            lives_free(pkg);
            pkg=NULL;
            menu=mainw->rte_defs;
          }
          if (pkg==NULL) {
            pkg=filter_name;
            filter_name=lives_strdup(pkg);
            memset(pkgstring,0,1);
            /* TRANSLATORS: example " - LADSPA plugins -" */
            pkgstring=lives_strdup_printf(_(" - %s plugins -"),pkg);
            // create new submenu

            pkg_menu=lives_menu_item_new_with_label(pkgstring);
            lives_container_add(LIVES_CONTAINER(mainw->rte_defs), pkg_menu);
            lives_menu_reorder_child(LIVES_MENU(mainw->rte_defs),pkg_menu,pkg_posn++);

            pkg_submenu=lives_menu_new();
            lives_menu_item_set_submenu(LIVES_MENU_ITEM(pkg_menu), pkg_submenu);

            if (palette->style&STYLE_1) {
              lives_widget_set_bg_color(pkg_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(pkg_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }

            lives_widget_show(pkg_menu);
            lives_widget_show(pkg_submenu);
            lives_free(pkgstring);
          }
          // add to submenu
          menu=pkg_submenu;
        } else {
          if (pkg!=NULL) lives_free(pkg);
          pkg=NULL;
          menu=mainw->rte_defs;
        }

        filter_type=weed_filter_get_type(filter,TRUE,FALSE);
        string=lives_strdup_printf("%s (%s)",filter_name,filter_type);

        menuitem=lives_menu_item_new_with_label(string);
        lives_widget_show(menuitem);
        lives_free(string);
        lives_free(filter_type);

        lives_container_add(LIVES_CONTAINER(menu), menuitem);

        lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                             LIVES_GUI_CALLBACK(rte_set_defs_activate),
                             LIVES_INT_TO_POINTER(i));

        lives_free(filter_name);
      }
    }
  }
}




void weed_load_all(void) {
  // get list of plugins from directory and create our fx
  int i,j,plugin_idx,subdir_idx;

  LiVESList *weed_plugin_list,*weed_plugin_sublist;

  char *lives_weed_plugin_path,*weed_plugin_path,*weed_p_path;
  char *subdir_path,*subdir_name,*plugin_path,*plugin_name;
  int numdirs;
  char **dirs;

  int listlen;

  num_weed_filters=0;
  num_weed_dupes=0;

  weed_filters=dupe_weed_filters=NULL;
  hashnames=dupe_hashnames=NULL;

  threaded_dialog_spin();
  lives_weed_plugin_path=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_WEED_FX_BUILTIN,NULL);

#ifdef DEBUG_WEED
  lives_printerr("In weed init\n");
#endif

  fg_gen_to_start=fg_generator_key=fg_generator_clip=fg_generator_mode=-1;
  bg_gen_to_start=bg_generator_key=bg_generator_mode=-1;

  for (i=0; i<FX_KEYS_MAX; i++) {
    if (i<FX_KEYS_MAX_VIRTUAL) key_to_instance[i]=(weed_plant_t **)
          lives_malloc(prefs->max_modes_per_key*sizeof(weed_plant_t *));
    else key_to_instance[i]=(weed_plant_t **)lives_malloc(sizeof(weed_plant_t *));

    key_to_instance_copy[i]=(weed_plant_t **)lives_malloc(sizeof(weed_plant_t *));

    if (i<FX_KEYS_MAX_VIRTUAL) key_to_fx[i]=(int *)lives_malloc(prefs->max_modes_per_key*sizint);
    else key_to_fx[i]=(int *)lives_malloc(sizint);

    if (i<FX_KEYS_MAX_VIRTUAL)
      key_defaults[i]=(weed_plant_t ** *)lives_malloc(prefs->max_modes_per_key*sizeof(weed_plant_t **));

    key_modes[i]=0;  // current active mode of each key
    filter_map[i]=NULL; // maps effects in order of application for multitrack rendering
    for (j=0; j<prefs->max_modes_per_key; j++) {
      if (i<FX_KEYS_MAX_VIRTUAL||j==0) {
        if (i<FX_KEYS_MAX_VIRTUAL) key_defaults[i][j]=NULL;
        key_to_instance[i][j]=NULL;
        key_to_fx[i][j]=-1;
      } else break;
    }
  }
  filter_map[FX_KEYS_MAX+1]=NULL;
  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    init_events[i]=NULL;
  }

  next_free_key=FX_KEYS_MAX_VIRTUAL;


  threaded_dialog_spin();
  weed_p_path=getenv("WEED_PLUGIN_PATH");
  if (weed_p_path==NULL) weed_plugin_path=lives_strdup("");
  else weed_plugin_path=lives_strdup(weed_p_path);

  if (strstr(weed_plugin_path,lives_weed_plugin_path)==NULL) {
    char *tmp=lives_strconcat(strcmp(weed_plugin_path,"")?":":"",lives_weed_plugin_path,NULL);
    lives_free(weed_plugin_path);
    weed_plugin_path=tmp;
    lives_setenv("WEED_PLUGIN_PATH",weed_plugin_path);
  }
  lives_free(lives_weed_plugin_path);

  // first we parse the weed_plugin_path
#ifndef IS_MINGW
  numdirs=get_token_count(weed_plugin_path,':');
  dirs=lives_strsplit(weed_plugin_path,":",-1);
#else
  numdirs=get_token_count(weed_plugin_path,';');
  dirs=lives_strsplit(weed_plugin_path,";",-1);
#endif
  threaded_dialog_spin();

  for (i=0; i<numdirs; i++) {
    // get list of all files
    weed_plugin_list=get_plugin_list(PLUGIN_EFFECTS_WEED,TRUE,dirs[i],NULL);
    listlen=lives_list_length(weed_plugin_list);

    // parse twice, first we get the plugins, then 1 level of subdirs
    for (plugin_idx=0; plugin_idx<listlen; plugin_idx++) {
      threaded_dialog_spin();
      plugin_name=(char *)lives_list_nth_data(weed_plugin_list,plugin_idx);
#ifndef IS_MINGW
      if (!strncmp(plugin_name+strlen(plugin_name)-3,".so",3))
#else
      if (!strncmp(plugin_name+strlen(plugin_name)-4,".dll",4))
#endif
      {
        plugin_path=lives_build_filename(dirs[i],plugin_name,NULL);
        load_weed_plugin(plugin_name,plugin_path,dirs[i]);
        lives_free(plugin_name);
        lives_free(plugin_path);
        weed_plugin_list=lives_list_delete_link(weed_plugin_list,lives_list_nth(weed_plugin_list,plugin_idx));
        plugin_idx--;
        listlen--;
      }
      threaded_dialog_spin();

    }

    // get 1 level of subdirs
    for (subdir_idx=0; subdir_idx<listlen; subdir_idx++) {
      threaded_dialog_spin();
      subdir_name=(char *)lives_list_nth_data(weed_plugin_list,subdir_idx);
      subdir_path=lives_build_filename(dirs[i],subdir_name,NULL);
      if (!lives_file_test(subdir_path, LIVES_FILE_TEST_IS_DIR)||!strcmp(subdir_name,"icons")||!strcmp(subdir_name,"data")) {
        lives_free(subdir_path);
        continue;
      }
#ifndef IS_MINGW
      weed_plugin_sublist=get_plugin_list(PLUGIN_EFFECTS_WEED,TRUE,subdir_path,"so");
#else
      weed_plugin_sublist=get_plugin_list(PLUGIN_EFFECTS_WEED,TRUE,subdir_path,"dll");
#endif

      for (plugin_idx=0; plugin_idx<lives_list_length(weed_plugin_sublist); plugin_idx++) {
        plugin_name=(char *)lives_list_nth_data(weed_plugin_sublist,plugin_idx);
        plugin_path=lives_build_filename(subdir_path,plugin_name,NULL);
        load_weed_plugin(plugin_name,plugin_path,subdir_path);
        lives_free(plugin_path);
      }
      if (weed_plugin_sublist!=NULL) {
        lives_list_free_strings(weed_plugin_sublist);
        lives_list_free(weed_plugin_sublist);
      }
      lives_free(subdir_path);
      threaded_dialog_spin();
    }
    if (weed_plugin_list!=NULL) {
      lives_list_free_strings(weed_plugin_list);
      lives_list_free(weed_plugin_list);
    }
  }

  lives_strfreev(dirs);
  lives_free(weed_plugin_path);

  d_print(_("Successfully loaded %d Weed filters\n"),num_weed_filters);

  threaded_dialog_spin();
  load_compound_fx();
  threaded_dialog_spin();
  make_fx_defs_menu();
  threaded_dialog_spin();
  if (num_weed_dupes>0) merge_dupes();

}


static void lives_free_if_not_in_list(void *ptr, LiVESList **freed_ptrs) {
  /// avoid duplicate frees when unloading fx plugins
  LiVESList *list=*freed_ptrs;
  while (list!=NULL) {
    if (ptr==(void *)list->data) return;
    list=list->next;
  }
  if (freed_ptrs!=NULL) *freed_ptrs=lives_list_append(*freed_ptrs,(livespointer)ptr);
  lives_free(ptr);
}


static void weed_plant_free_if_not_in_list(weed_plant_t *plant, LiVESList **freed_ptrs) {
  /// avoid duplicate frees when unloading fx plugins
  LiVESList *list=*freed_ptrs;
  while (list!=NULL) {
    if (plant==(weed_plant_t *)list->data) return;
    list=list->next;
  }
  if (freed_ptrs!=NULL) *freed_ptrs=lives_list_append(*freed_ptrs,(livespointer)plant);
  weed_plant_free(plant);
}


static void weed_filter_free(weed_plant_t *filter, LiVESList **freed_ptrs) {
  int nitems,error,i;
  weed_plant_t **plants,*gui;
  void *func;
  boolean is_compound=FALSE;

  if (num_compound_fx(filter)>1) is_compound=TRUE;

  // free in_param_templates
  if (weed_plant_has_leaf(filter,"in_parameter_templates")) {
    nitems=weed_leaf_num_elements(filter,"in_parameter_templates");
    if (nitems>0) {
      plants=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
      for (i=0; i<nitems; i++) {
        if (weed_plant_has_leaf(plants[i],"gui")) {
          gui=(weed_get_plantptr_value(plants[i],"gui",&error));
          if (weed_plant_has_leaf(gui,"display_func")) {
            func=weed_get_voidptr_value(gui,"display_func",&error);
            if (func!=NULL) lives_free_if_not_in_list(func,freed_ptrs);
          }
          weed_plant_free_if_not_in_list(gui,freed_ptrs);
        }
        if (weed_plant_has_leaf(filter,"interpolate_func")) {
          func=weed_get_voidptr_value(filter,"interpolate_func",&error);
          if (func!=NULL) lives_free_if_not_in_list(func,freed_ptrs);
        }
        weed_plant_free_if_not_in_list(plants[i],freed_ptrs);
      }
      lives_free(plants);
    }
  }

  if (is_compound) {
    weed_plant_free(filter);
    return;
  }

  if (weed_plant_has_leaf(filter,"init_func")) {
    func=weed_get_voidptr_value(filter,"init_func",&error);
    if (func!=NULL) lives_free_if_not_in_list(func,freed_ptrs);
  }

  if (weed_plant_has_leaf(filter,"deinit_func")) {
    func=weed_get_voidptr_value(filter,"deinit_func",&error);
    if (func!=NULL) lives_free_if_not_in_list(func,freed_ptrs);
  }

  if (weed_plant_has_leaf(filter,"process_func")) {
    func=weed_get_voidptr_value(filter,"process_func",&error);
    if (func!=NULL) lives_free_if_not_in_list(func,freed_ptrs);
  }


  // free in_channel_templates
  if (weed_plant_has_leaf(filter,"in_channel_templates")) {
    nitems=weed_leaf_num_elements(filter,"in_channel_templates");
    if (nitems>0) {
      plants=weed_get_plantptr_array(filter,"in_channel_templates",&error);
      for (i=0; i<nitems; i++) weed_plant_free_if_not_in_list(plants[i],freed_ptrs);
      lives_free(plants);
    }
  }


  // free out_channel_templates
  if (weed_plant_has_leaf(filter,"out_channel_templates")) {
    nitems=weed_leaf_num_elements(filter,"out_channel_templates");
    if (nitems>0) {
      plants=weed_get_plantptr_array(filter,"out_channel_templates",&error);
      for (i=0; i<nitems; i++) weed_plant_free_if_not_in_list(plants[i],freed_ptrs);
      lives_free(plants);
    }
  }

  // free out_param_templates
  if (weed_plant_has_leaf(filter,"out_parameter_templates")) {
    nitems=weed_leaf_num_elements(filter,"out_parameter_templates");
    if (nitems>0) {
      plants=weed_get_plantptr_array(filter,"out_parameter_templates",&error);
      threaded_dialog_spin();
      for (i=0; i<nitems; i++) {
        if (weed_plant_has_leaf(plants[i],"gui")) weed_plant_free(weed_get_plantptr_value(plants[i],"gui",&error));
        weed_plant_free_if_not_in_list(plants[i],freed_ptrs);
      }
      lives_free(plants);
    }
  }


  // free gui
  if (weed_plant_has_leaf(filter,"gui")) weed_plant_free_if_not_in_list(weed_get_plantptr_value(filter,"gui",&error),freed_ptrs);


  // free filter
  weed_plant_free(filter);
}



static weed_plant_t *create_compound_filter(char *plugin_name, int nfilts, int *filts) {
  weed_plant_t *filter=weed_plant_new(WEED_PLANT_FILTER_CLASS),*xfilter,*gui;

  weed_plant_t **in_params=NULL,**out_params=NULL,**params;
  weed_plant_t **in_chans,**out_chans;

  char *tmp;

  double tgfps=-1.,tfps;

  int count,xcount,error;

  int txparam=-1,tparam,txvolm=-1,tvolm;

  register int i,j,x;


  weed_set_int_array(filter,"host_filter_list",nfilts,filts);

  // create parameter templates - concatenate all sub filters
  count=xcount=0;

  for (i=0; i<nfilts; i++) {
    xfilter=weed_filters[filts[i]];

    if (weed_plant_has_leaf(xfilter,"target_fps")) {
      tfps=weed_get_double_value(xfilter,"target_fps",&error);
      if (tgfps==-1.) tgfps=tfps;
      else if (tgfps!=tfps) {
        d_print((tmp=lives_strdup_printf(_("Invalid compound effect %s - has conflicting target_fps\n"),plugin_name)));
        LIVES_ERROR(tmp);
        lives_free(tmp);
        return NULL;
      }
    }

    // in_parameter templates are copy-by-value, since we may set a different default/host_default value, and a different gui

    // everything else - filter classes, out_param templates and channels are copy by ref.


    if (weed_plant_has_leaf(xfilter,"in_parameter_templates")) {

      tparam=get_transition_param(xfilter,FALSE);

      // TODO *** - ignore transtion params if they are connected to themselves

      if (tparam!=-1) {
        if (txparam!=-1) {
          d_print((tmp=lives_strdup_printf(_("Invalid compound effect %s - has multiple transition parameters\n"),plugin_name)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          return NULL;
        }
        txparam=tparam;
      }

      tvolm=get_master_vol_param(xfilter,FALSE);

      // TODO *** - ignore master vol params if they are connected to themselves

      if (tvolm!=-1) {
        if (txvolm!=-1) {
          d_print((tmp=lives_strdup_printf(_("Invalid compound effect %s - has multiple master volume parameters\n"),plugin_name)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          return NULL;
        }
        txvolm=tvolm;
      }

      count+=weed_leaf_num_elements(xfilter,"in_parameter_templates");
      params=weed_get_plantptr_array(xfilter,"in_parameter_templates",&error);

      in_params=(weed_plant_t **)lives_realloc(in_params,count*sizeof(weed_plant_t *));
      x=0;

      for (j=xcount; j<count; j++) {
        in_params[j]=weed_plant_copy(params[x]);
        if (weed_plant_has_leaf(params[x],"gui")) {
          gui=weed_get_plantptr_value(params[x],"gui",&error);
          weed_set_plantptr_value(in_params[j],"gui",weed_plant_copy(gui));
        }

        if (x==tparam) {
          weed_set_boolean_value(in_params[j],"transition",WEED_TRUE);
        } else if (weed_plant_has_leaf(in_params[j],"transition")) weed_leaf_delete(in_params[j],"transition");

        if (x==tvolm) {
          weed_set_boolean_value(in_params[j],"is_volume_master",WEED_TRUE);
        } else if (weed_plant_has_leaf(in_params[j],"is_volume_master")) weed_leaf_delete(in_params[j],"is_volume_master");

        x++;
      }
      lives_free(params);
      xcount=count;
    }
  }

  if (tgfps!=-1.) weed_set_double_value(filter,"target_fps",tgfps);

  if (count>0) {
    weed_set_plantptr_array(filter,"in_parameter_templates",count,in_params);
    lives_free(in_params);
  }

  count=xcount=0;

  for (i=0; i<nfilts; i++) {
    xfilter=weed_filters[filts[i]];
    if (weed_plant_has_leaf(xfilter,"out_parameter_templates")) {
      count+=weed_leaf_num_elements(xfilter,"out_parameter_templates");
      params=weed_get_plantptr_array(xfilter,"out_parameter_templates",&error);

      out_params=(weed_plant_t **)lives_realloc(out_params,count*sizeof(weed_plant_t *));
      x=0;

      for (j=xcount; j<count; j++) {
        out_params[j]=params[x++];
      }
      if (params!=NULL) lives_free(params);
      xcount=count;
    }
  }

  if (count>0) {
    weed_set_plantptr_array(filter,"out_parameter_templates",count,out_params);
    lives_free(out_params);
  }

  // use in channels from first filter

  xfilter=weed_filters[filts[0]];
  if (weed_plant_has_leaf(xfilter,"in_channel_templates")) {
    count=weed_leaf_num_elements(xfilter,"in_channel_templates");
    in_chans=weed_get_plantptr_array(xfilter,"in_channel_templates",&error);
    weed_set_plantptr_array(filter,"in_channel_templates",count,in_chans);
    lives_free(in_chans);
  }

  // use out channels from last filter

  xfilter=weed_filters[filts[nfilts-1]];
  if (weed_plant_has_leaf(xfilter,"out_channel_templates")) {
    count=weed_leaf_num_elements(xfilter,"out_channel_templates");
    out_chans=weed_get_plantptr_array(xfilter,"out_channel_templates",&error);
    weed_set_plantptr_array(filter,"out_channel_templates",count,out_chans);
    lives_free(out_chans);
  }

  return filter;
}






static void load_compound_plugin(char *plugin_name, char *plugin_path) {
  FILE *cpdfile;

  weed_plant_t *filter=NULL,*ptmpl,*iptmpl,*xfilter;

  char buff[16384];

  char **array,**svals;

  char *author=NULL,*tmp,*key;

  int *filters=NULL,*ivals;

  double *dvals;

  int xvals[4];

  boolean ok=TRUE,autoscale;

  int stage=0,nfilts=0,fnum,line=0,version=0;
  int qvals=1,ntok,xfilt,xfilt2;

  int xconx=0;

  int nparams,nchans,pnum,pnum2,xpnum2,cnum,cnum2,ptype,phint,pcspace,pflags;

  int error;

  register int i;

  if ((cpdfile=fopen(plugin_path,"r"))) {
    while (fgets(buff,16384,cpdfile)) {
      line++;
      lives_strstrip(buff);
      if (!strlen(buff)) {
        if (++stage==5) break;
        if (stage==2) {
          if (nfilts<2) {
            d_print((tmp=lives_strdup_printf(_("Invalid compound effect %s - must have >1 sub filters\n"),plugin_name)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
            ok=FALSE;
            break;
          }
          filter=create_compound_filter(plugin_name,nfilts,filters);
          if (filter==NULL) break;
        }
        continue;
      }
      switch (stage) {
      case 0:
        if (line==1) author=lives_strdup(buff);
        if (line==2) version=atoi(buff);
        break;
      case 1:
        // add filters
        fnum=weed_get_idx_for_hashname(buff,TRUE);
        if (fnum==-1) {
          d_print((tmp=lives_strdup_printf(_("Invalid effect %s found in compound effect %s, line %d\n"),buff,plugin_name,line)));
          LIVES_INFO(tmp);
          lives_free(tmp);
          ok=FALSE;
          break;
        }
        filters=(int *)lives_realloc(filters,++nfilts*sizeof(int));
        filters[nfilts-1]=fnum;
        break;
      case 2:
        // override defaults: format is i|p|d0|d1|...|dn
        // NOTE: for obvious reasons, | may not be used inside string defaults
        ntok=get_token_count(buff,'|');

        if (ntok<2) {
          d_print((tmp=lives_strdup_printf(_("Invalid default found in compound effect %s, line %d\n"),plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          break;
        }

        array=lives_strsplit(buff,"|",ntok);

        xfilt=atoi(array[0]); // sub filter number
        if (xfilt<0||xfilt>=nfilts) {
          d_print((tmp=lives_strdup_printf(_("Invalid filter %d for defaults found in compound effect %s, line %d\n"),xfilt,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }
        xfilter=get_weed_filter(filters[xfilt]);

        nparams=num_in_params(xfilter,FALSE,FALSE);

        pnum=atoi(array[1]);

        if (pnum>=nparams) {
          d_print((tmp=lives_strdup_printf(_("Invalid param %d for defaults found in compound effect %s, line %d\n"),pnum,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }

        // get pnum for compound filter
        for (i=0; i<xfilt; i++) pnum+=num_in_params(get_weed_filter(filters[i]),FALSE,FALSE);

        ptmpl=weed_filter_in_paramtmpl(filter,pnum,FALSE);

        ptype=weed_leaf_seed_type(ptmpl,"default");
        pflags=weed_get_int_value(ptmpl,"flags",&error);
        phint=weed_get_int_value(ptmpl,"hint",&error);

        if (phint==WEED_HINT_COLOR) {
          pcspace=weed_get_int_value(ptmpl,"colorspace",&error);
          qvals=3;
          if (pcspace==WEED_COLORSPACE_RGBA) qvals=4;
        }

        ntok-=2;

        if ((ntok!=weed_leaf_num_elements(ptmpl,"default")&&!(pflags&WEED_PARAMETER_VARIABLE_ELEMENTS)) ||
            ntok%qvals!=0) {
          d_print((tmp=lives_strdup_printf(_("Invalid number of values for defaults found in compound effect %s, line %d\n"),plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }

        // TODO - for INT and DOUBLE, check if default is within min/max bounds

        switch (ptype) {
        case WEED_SEED_INT:
          ivals=(int *)lives_malloc(ntok*sizint);
          for (i=0; i<ntok; i++) {
            ivals[i]=atoi(array[i+2]);
          }
          weed_set_int_array(ptmpl,"default",ntok,ivals);
          lives_free(ivals);
          break;
        case WEED_SEED_DOUBLE:
          dvals=(double *)lives_malloc(ntok*sizdbl);
          for (i=0; i<ntok; i++) {
            dvals[i]=strtod(array[i+2],NULL);
          }
          weed_set_double_array(ptmpl,"default",ntok,dvals);
          lives_free(dvals);
          break;
        case WEED_SEED_BOOLEAN:
          ivals=(int *)lives_malloc(ntok*sizint);
          for (i=0; i<ntok; i++) {
            ivals[i]=atoi(array[i+2]);

            if (ivals[i]!=WEED_TRUE&&ivals[i]!=WEED_FALSE) {
              lives_free(ivals);
              d_print((tmp=lives_strdup_printf(_("Invalid non-boolean value for defaults found in compound effect %s, line %d\n"),
                                               pnum,plugin_name,line)));
              LIVES_ERROR(tmp);
              lives_free(tmp);
              ok=FALSE;
              lives_strfreev(array);
              break;
            }

          }
          weed_set_boolean_array(ptmpl,"default",ntok,ivals);
          lives_free(ivals);
          break;
        default: // string
          svals=(char **)lives_malloc(ntok*sizeof(char *));
          for (i=0; i<ntok; i++) {
            svals[i]=lives_strdup(array[i+2]);
          }
          weed_set_string_array(ptmpl,"default",ntok,svals);
          for (i=0; i<ntok; i++) {
            lives_free(svals[i]);
          }
          lives_free(svals);
          break;
        }
        lives_strfreev(array);
        break;
      case 3:
        // link params: format is f0|p0|autoscale|f1|p1

        ntok=get_token_count(buff,'|');

        if (ntok!=5) {
          d_print((tmp=lives_strdup_printf(_("Invalid param link found in compound effect %s, line %d\n"),plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          break;
        }

        array=lives_strsplit(buff,"|",ntok);

        xfilt=atoi(array[0]); // sub filter number
        if (xfilt<-1||xfilt>=nfilts) {
          d_print((tmp=lives_strdup_printf(_("Invalid out filter %d for link params found in compound effect %s, line %d\n"),
                                           xfilt,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }

        pnum=atoi(array[1]);

        if (xfilt>-1) {
          xfilter=get_weed_filter(filters[xfilt]);

          if (weed_plant_has_leaf(xfilter,"out_parameter_templates"))
            nparams=weed_leaf_num_elements(xfilter,"out_parameter_templates");
          else nparams=0;

          if (pnum>=nparams) {
            d_print((tmp=lives_strdup_printf(_("Invalid out param %d for link params found in compound effect %s, line %d\n"),
                                             pnum,plugin_name,line)));
            LIVES_ERROR(tmp);
            lives_free(tmp);
            ok=FALSE;
            lives_strfreev(array);
            break;
          }
        }

        autoscale=atoi(array[2]);

        if (autoscale!=WEED_TRUE&&autoscale!=WEED_FALSE) {
          d_print((tmp=lives_strdup_printf(_("Invalid non-boolean value for autoscale found in compound effect %s, line %d\n"),
                                           pnum,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }

        xfilt2=atoi(array[3]); // sub filter number
        if (xfilt>=nfilts) {
          d_print((tmp=lives_strdup_printf(_("Invalid in filter %d for link params found in compound effect %s, line %d\n"),
                                           xfilt2,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }
        xfilter=get_weed_filter(filters[xfilt2]);

        nparams=num_in_params(xfilter,FALSE,FALSE);

        pnum2=atoi(array[4]);

        if (pnum2>=nparams) {
          d_print((tmp=lives_strdup_printf(_("Invalid in param %d for link params found in compound effect %s, line %d\n"),
                                           pnum2,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }

        // calc xpnum2
        xpnum2=pnum2;
        for (i=0; i<xfilt2; i++) xpnum2+=num_in_params(get_weed_filter(filters[i]),FALSE,FALSE);

        // must get paramtmpl here from filter (not xfilter)
        iptmpl=weed_filter_in_paramtmpl(filter,xpnum2,FALSE);

        xvals[0]=xfilt;
        xvals[1]=pnum;

        weed_set_int_array(iptmpl,"host_internal_connection",2,xvals);
        if (autoscale==WEED_TRUE) weed_set_boolean_value(iptmpl,"host_internal_connection_autoscale",WEED_TRUE);

        lives_strfreev(array);
        break;
      case 4:
        // link alpha channels: format is f0|c0|f1|c1
        ntok=get_token_count(buff,'|');

        if (ntok!=4) {
          d_print((tmp=lives_strdup_printf(_("Invalid channel link found in compound effect %s, line %d\n"),plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          break;
        }

        array=lives_strsplit(buff,"|",ntok);

        xfilt=atoi(array[0]); // sub filter number
        if (xfilt<0||xfilt>=nfilts) {
          d_print((tmp=lives_strdup_printf(_("Invalid out filter %d for link channels found in compound effect %s, line %d\n"),
                                           xfilt,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }
        xfilter=get_weed_filter(filters[xfilt]);

        nchans=0;
        if (weed_plant_has_leaf(xfilter,"out_channel_templates")) {
          nchans=weed_leaf_num_elements(xfilter,"out_channel_templates");
          if (weed_get_plantptr_value(xfilter,"out_channel_templates",&error)==NULL) nchans=0;
        }

        cnum=atoi(array[1]);

        if (cnum>=nchans) {
          d_print((tmp=lives_strdup_printf(_("Invalid out channel %d for link params found in compound effect %s, line %d\n"),
                                           cnum,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }

        xfilt2=atoi(array[2]); // sub filter number
        if (xfilt2<=xfilt||xfilt>=nfilts) {
          d_print((tmp=lives_strdup_printf(_("Invalid in filter %d for link channels found in compound effect %s, line %d\n"),
                                           xfilt2,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }
        xfilter=get_weed_filter(filters[xfilt2]);

        nchans=0;
        if (weed_plant_has_leaf(xfilter,"in_channel_templates")) {
          nchans=weed_leaf_num_elements(xfilter,"in_channel_templates");
          if (weed_get_plantptr_value(xfilter,"in_channel_templates",&error)==NULL) nchans=0;
        }

        cnum2=atoi(array[3]);

        if (cnum2>=nchans) {
          d_print((tmp=lives_strdup_printf(_("Invalid in channel %d for link params found in compound effect %s, line %d\n"),
                                           cnum2,plugin_name,line)));
          LIVES_ERROR(tmp);
          lives_free(tmp);
          ok=FALSE;
          lives_strfreev(array);
          break;
        }

        xvals[0]=xfilt;
        xvals[1]=cnum;
        xvals[2]=xfilt2;
        xvals[3]=cnum2;

        // unlike with in_param_templates, which are copy by value, in_channel_templates are copy by ref.
        // so we need to add some extra leaves to the compound filter to note channel connections
        // - we will link the actual channels when we create an instance from the filter
        key=lives_strdup_printf("host_channel_connection%d",xconx++);
        weed_set_int_array(filter,key,4,xvals);
        lives_free(key);

        lives_strfreev(array);
        break;
      default:
        break;
      }
      if (!ok) {
        if (filter!=NULL) weed_filter_free(filter,NULL);
        filter=NULL;
        break;
      }
    }
    fclose(cpdfile);
  }

  if (filter!=NULL) {
    int idx;
    char *filter_name=lives_strdup_printf(_("Compound:%s"),plugin_name);
    weed_set_string_value(filter,"name",filter_name);

    weed_set_string_value(filter,"author",author);
    lives_free(author);
    weed_set_int_value(filter,"version",version);
    idx=num_weed_filters++;

    weed_filters=(weed_plant_t **)lives_realloc(weed_filters,num_weed_filters*sizeof(weed_plant_t *));
    weed_filters[idx]=filter;

    hashnames=(char **)lives_realloc(hashnames,num_weed_filters*sizeof(char *));
    hashnames[idx]=NULL;
    hashnames[idx]=make_weed_hashname(idx,TRUE,FALSE);

    lives_free(filter_name);
  }

  if (filters!=NULL) lives_free(filters);

}





void load_compound_fx(void) {
  LiVESList *compound_plugin_list;

  int plugin_idx,onum_filters=num_weed_filters;

  char *lives_compound_plugin_path,*plugin_name,*plugin_path;

  threaded_dialog_spin();

  lives_compound_plugin_path=lives_build_filename(capable->home_dir,PLUGIN_COMPOUND_EFFECTS_CUSTOM,NULL);
  compound_plugin_list=get_plugin_list(PLUGIN_COMPOUND_EFFECTS_CUSTOM,TRUE,lives_compound_plugin_path,NULL);

  for (plugin_idx=0; plugin_idx<lives_list_length(compound_plugin_list); plugin_idx++) {
    plugin_name=(char *)lives_list_nth_data(compound_plugin_list,plugin_idx);
    plugin_path=lives_build_filename(lives_compound_plugin_path,plugin_name,NULL);
    load_compound_plugin(plugin_name,plugin_path);
    lives_free(plugin_path);
    threaded_dialog_spin();
  }

  if (compound_plugin_list!=NULL) {
    lives_list_free_strings(compound_plugin_list);
    lives_list_free(compound_plugin_list);
  }

  threaded_dialog_spin();
  lives_free(lives_compound_plugin_path);

  lives_compound_plugin_path=lives_build_filename(prefs->prefix_dir,PLUGIN_COMPOUND_DIR,PLUGIN_COMPOUND_EFFECTS_BUILTIN,NULL);
  compound_plugin_list=get_plugin_list(PLUGIN_COMPOUND_EFFECTS_BUILTIN,TRUE,lives_compound_plugin_path,NULL);

  for (plugin_idx=0; plugin_idx<lives_list_length(compound_plugin_list); plugin_idx++) {
    plugin_name=(char *)lives_list_nth_data(compound_plugin_list,plugin_idx);
    plugin_path=lives_build_filename(lives_compound_plugin_path,plugin_name,NULL);
    load_compound_plugin(plugin_name,plugin_path);
    lives_free(plugin_path);
    threaded_dialog_spin();
  }

  if (compound_plugin_list!=NULL) {
    lives_list_free_strings(compound_plugin_list);
    lives_list_free(compound_plugin_list);
  }

  if (num_weed_filters>onum_filters) {
    d_print(_("Successfully loaded %d compound filters\n"),num_weed_filters-onum_filters);
  }

  lives_free(lives_compound_plugin_path);
  threaded_dialog_spin();

}





void weed_unload_all(void) {
  weed_plant_t *filter,*plugin_info,*host_info;

  void *handle;

  weed_desetup_f desetup_fn;

  LiVESList *pinfo=NULL,*xpinfo,*freed_ptrs=NULL;

  int error;

  register int i,j;

  mainw->num_tr_applied=0;
  weed_deinit_all(TRUE);

  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    for (j=0; j<prefs->max_modes_per_key; j++) {
      if (key_defaults[i][j]!=NULL) free_key_defaults(i,j);
    }
    lives_free(key_defaults[i]);
  }

  mainw->chdir_failed=FALSE;

  for (i=0; i<num_weed_filters; i++) {

    filter=weed_filters[i];

    if (num_compound_fx(filter)>1) {
      weed_filter_free(filter,&freed_ptrs);
      continue;
    }

    plugin_info=weed_get_plantptr_value(filter,"plugin_info",&error);

    if (pinfo==NULL||lives_list_index(pinfo,plugin_info)==-1) pinfo=lives_list_append(pinfo,plugin_info);

    handle=weed_get_voidptr_value(plugin_info,"handle",&error);

    if (handle!=NULL&&prefs->startup_phase==0) {
      if ((desetup_fn=(weed_desetup_f)dlsym(handle,"weed_desetup"))!=NULL) {
        char *cwd=cd_to_plugin_dir(filter);
        // call weed_desetup()
        (*desetup_fn)();
        lives_chdir(cwd,FALSE);
        lives_free(cwd);
      }

      dlclose(handle);
      threaded_dialog_spin();
      handle=NULL;
      weed_set_voidptr_value(plugin_info,"handle",handle);
    }
    weed_filter_free(filter,&freed_ptrs);
  }

  lives_list_free(freed_ptrs);

  xpinfo=pinfo;

  while (pinfo!=NULL) {
    host_info=weed_get_plantptr_value((weed_plant_t *)pinfo->data,"host_info",&error);
    weed_plant_free(host_info);
    weed_plant_free((weed_plant_t *)pinfo->data);
    pinfo=pinfo->next;
  }

  if (xpinfo!=NULL) lives_list_free(xpinfo);

  for (i=0; i<num_weed_filters; i++) {
    if (hashnames[i]!=NULL) lives_free(hashnames[i]);
  }

  if (hashnames!=NULL) lives_free(hashnames);
  if (weed_filters!=NULL) lives_free(weed_filters);

  for (i=0; i<FX_KEYS_MAX; i++) {
    lives_free(key_to_fx[i]);
    lives_free(key_to_instance[i]);
    lives_free(key_to_instance_copy[i]);
  }

  threaded_dialog_spin();

  if (mainw->chdir_failed) {
    char *dirs=lives_strdup(_("Some plugin directories"));
    do_chdir_failed_error(dirs);
    lives_free(dirs);
  }

}


static void weed_in_channels_free(weed_plant_t *inst) {
  weed_plant_t **channels;
  int i,error;
  int num_channels;

  if (!weed_plant_has_leaf(inst,"in_channels")) return;

  num_channels=weed_leaf_num_elements(inst,"in_channels");
  channels=weed_get_plantptr_array(inst,"in_channels",&error);
  for (i=0; i<num_channels; i++) {
    if (channels[i]!=NULL) {
      if (weed_palette_is_alpha_palette(weed_layer_get_palette(channels[i]))) weed_layer_free(channels[i]);
      else weed_plant_free(channels[i]);
    }
  }
  lives_free(channels);

}

static void weed_out_channels_free(weed_plant_t *inst) {
  weed_plant_t **channels;
  int i,error;
  int num_channels;

  if (!weed_plant_has_leaf(inst,"out_channels")) return;

  num_channels=weed_leaf_num_elements(inst,"out_channels");
  channels=weed_get_plantptr_array(inst,"out_channels",&error);
  for (i=0; i<num_channels; i++) {
    if (channels[i]!=NULL) {
      if (weed_palette_is_alpha_palette(weed_layer_get_palette(channels[i]))) weed_layer_free(channels[i]);
      else weed_plant_free(channels[i]);
    }
  }
  lives_free(channels);
}

static void weed_channels_free(weed_plant_t *inst) {
  weed_in_channels_free(inst);
  weed_out_channels_free(inst);
}


static void weed_gui_free(weed_plant_t *plant) {
  weed_plant_t *gui;
  int error;

  if (weed_plant_has_leaf(plant,"gui")) {
    gui=weed_get_plantptr_value(plant,"gui",&error);
    weed_plant_free(gui);
  }
}


void weed_in_params_free(weed_plant_t **parameters, int num_parameters) {
  register int i;
  for (i=0; i<num_parameters; i++) {
    if (parameters[i]!=NULL) {
      if (parameters[i]==mainw->rte_textparm) mainw->rte_textparm=NULL;
      weed_gui_free(parameters[i]);
      weed_plant_free(parameters[i]);
    }
  }
  lives_free(parameters);
}


void weed_in_parameters_free(weed_plant_t *inst) {
  weed_plant_t **parameters;
  int error;
  int num_parameters;

  if (!weed_plant_has_leaf(inst,"in_parameters")) return;

  num_parameters=weed_leaf_num_elements(inst,"in_parameters");
  parameters=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_in_params_free(parameters,num_parameters);
}

static void weed_out_parameters_free(weed_plant_t *inst) {
  weed_plant_t **parameters;
  int error;
  int num_parameters;
  register int i;

  if (!weed_plant_has_leaf(inst,"out_parameters")) return;

  num_parameters=weed_leaf_num_elements(inst,"out_parameters");
  parameters=weed_get_plantptr_array(inst,"out_parameters",&error);
  for (i=0; i<num_parameters; i++) {
    if (parameters[i]!=NULL) {
      weed_plant_free(parameters[i]);
    }
  }
  lives_free(parameters);
}

static void weed_parameters_free(weed_plant_t *inst) {
  weed_in_parameters_free(inst);
  weed_out_parameters_free(inst);
}

static void lives_free_instance(weed_plant_t *inst) {
  weed_channels_free(inst);
  weed_parameters_free(inst);
  weed_plant_free(inst);
}

void weed_instance_unref(weed_plant_t *inst) {
  int error;
  int nrefs=weed_get_int_value(inst,"host_refs",&error)-1;
  if (nrefs==0) lives_free_instance(inst);
  else weed_set_int_value(inst,"host_refs",nrefs);
}

void weed_instance_ref(weed_plant_t *inst) {
  int error;
  int nrefs=weed_get_int_value(inst,"host_refs",&error)+1;
  weed_set_int_value(inst,"host_refs",nrefs);
}



void wge_inner(weed_plant_t *inst) {
  weed_plant_t *next_inst=NULL;

  int error;

  if (weed_plant_has_leaf(inst,"host_next_instance")) next_inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
  else next_inst=NULL;

  weed_call_deinit_func(inst);
  weed_instance_unref(inst);

  if (next_inst!=NULL) {
    // handle compound fx
    inst=next_inst;
    wge_inner(inst);
  }

}


void weed_generator_end(weed_plant_t *inst) {
  // generator has stopped for one of the following reasons:
  // efect was de-inited; clip (bg/fg) was changed; playback stopped with fg
  lives_whentostop_t wts=mainw->whentostop;
  boolean is_bg=FALSE;
  boolean clip_switched=mainw->clip_switched;
  int current_file=mainw->current_file,pre_src_file=mainw->pre_src_file;

  if (inst==NULL) {
    LIVES_WARN("inst was NULL !");
    return;
  }

  if (mainw->blend_file!=-1&&mainw->blend_file!=current_file&&mainw->files[mainw->blend_file]!=NULL&&
      mainw->files[mainw->blend_file]->ext_src==inst) is_bg=TRUE;
  else mainw->new_blend_file=mainw->blend_file;

  if (!is_bg&&mainw->whentostop==STOP_ON_VID_END&&mainw->playing_file>0) {
    // we will close the file after playback stops
    mainw->cancelled=CANCEL_GENERATOR_END;
    return;
  }

  if (rte_window!=NULL) {
    // update real time effects window if we are showing it
    if (!is_bg) {
      rtew_set_keych(fg_generator_key,FALSE);
    } else {
      rtew_set_keych(bg_generator_key,FALSE);
    }
  }

  if (mainw->ce_thumbs) {
    // update ce_thumbs window if we are showing it
    if (!is_bg) {
      ce_thumbs_set_keych(fg_generator_key,FALSE);
    } else {
      ce_thumbs_set_keych(bg_generator_key,FALSE);
    }
  }

  if (get_audio_channel_in(inst,0)!=NULL) {
    mainw->afbuffer_clients--;
    if (mainw->afbuffer_clients==0) {
      pthread_mutex_lock(&mainw->abuf_frame_mutex);
      free_audio_frame_buffer(mainw->audio_frame_buffer);
      lives_free(mainw->audio_frame_buffer);
      mainw->audio_frame_buffer=NULL;
      pthread_mutex_unlock(&mainw->abuf_frame_mutex);
    }
  }

  if (is_bg) {
    if (mainw->blend_layer!=NULL) check_layer_ready(mainw->blend_layer);
    filter_mutex_lock(bg_generator_key);
    key_to_instance[bg_generator_key][bg_generator_mode]=NULL;
    filter_mutex_unlock(bg_generator_key);
    if (mainw->rte&(GU641<<bg_generator_key)) mainw->rte^=(GU641<<bg_generator_key);
    bg_gen_to_start=bg_generator_key=bg_generator_mode=-1;
    pre_src_file=mainw->pre_src_file;
    mainw->pre_src_file=mainw->current_file;
    mainw->current_file=mainw->blend_file;
  } else {
    if (mainw->frame_layer!=NULL) check_layer_ready(mainw->frame_layer);
    filter_mutex_lock(fg_generator_key);
    key_to_instance[fg_generator_key][fg_generator_mode]=NULL;
    filter_mutex_unlock(fg_generator_key);
    if (mainw->rte&(GU641<<fg_generator_key)) mainw->rte^=(GU641<<fg_generator_key);
    fg_gen_to_start=fg_generator_key=fg_generator_clip=fg_generator_mode=-1;
    if (mainw->blend_file==mainw->current_file) mainw->blend_file=-1;
  }

  wge_inner(inst);

  // if the param window is already open, show any reinits now
  if (fx_dialog[1]!=NULL) {
    if (is_bg) redraw_pwindow(bg_generator_key,bg_generator_mode);
    else redraw_pwindow(fg_generator_key,fg_generator_mode);
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
    if (cfile==NULL||cfile->clip_type!=CLIP_TYPE_GENERATOR) {
      LIVES_WARN("Close non-generator file");
    } else {
      close_current_file(mainw->pre_src_file);
    }
    if (mainw->ce_thumbs&&mainw->active_sa_clips==SCREEN_AREA_BACKGROUND) ce_thumbs_update_current_clip();
  } else {
    // close generator file and switch to original file if possible
    if (cfile==NULL||cfile->clip_type!=CLIP_TYPE_GENERATOR) {
      LIVES_WARN("Close non-generator file");
    } else {
      close_current_file(mainw->pre_src_file);
    }
    if (mainw->current_file==current_file) mainw->clip_switched=clip_switched;
  }

  if (is_bg) {
    mainw->current_file=current_file;
    mainw->pre_src_file=pre_src_file;
    mainw->whentostop=wts;
  }

  if (mainw->current_file==-1) mainw->cancelled=CANCEL_GENERATOR_END;

}


static weed_plant_t **weed_channels_create(weed_plant_t *filter, boolean in) {
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

  for (i=0; i<num_channels; i++) {
    if (weed_plant_has_leaf(chantmpls[i],"host_repeats")) ccount+=weed_get_int_value(chantmpls[i],"host_repeats",&error);
    else ccount+=1;
  }

  channels=(weed_plant_t **)lives_malloc((ccount+1)*sizeof(weed_plant_t *));

  ccount=0;

  for (i=0; i<num_channels; i++) {
    if (weed_plant_has_leaf(chantmpls[i],"host_repeats")) num_repeats=weed_get_int_value(chantmpls[i],"host_repeats",&error);
    else num_repeats=1;
    for (j=0; j<num_repeats; j++) {
      channels[ccount]=weed_plant_new(WEED_PLANT_CHANNEL);
      weed_set_plantptr_value(channels[ccount],"template",chantmpls[i]);

      weed_set_voidptr_value(channels[ccount],"pixel_data",NULL);

      if (weed_plant_has_leaf(chantmpls[i],"current_palette")) {
        // audio only channels dont have a "current_palette" !
        weed_set_int_value(channels[ccount],"current_palette",
                           (pal=weed_get_int_value(chantmpls[i],"current_palette",&error)));

        if (weed_palette_is_yuv_palette(pal)) {
          if (!(weed_plant_has_leaf(chantmpls[i],"YUV_subspace"))||
              weed_get_int_value(chantmpls[i],"YUV_subspace",&error)==WEED_YUV_SUBSPACE_YUV) {
            // set to default for LiVES
            weed_set_int_value(channels[ccount],"YUV_subspace",WEED_YUV_SUBSPACE_YCBCR);
          } else {
            weed_set_int_value(channels[ccount],"YUV_subspace",weed_get_int_value(chantmpls[i],"YUV_subspace",&error));
          }
        }
      }

      if (weed_plant_has_leaf(chantmpls[i],"host_disabled")) {
        weed_set_boolean_value(channels[ccount],"disabled", weed_get_boolean_value(chantmpls[i],"host_disabled",&error));
      }
      weed_add_plant_flags(channels[ccount],WEED_LEAF_READONLY_PLUGIN);
      ccount++;
    }
  }
  channels[ccount]=NULL;
  lives_free(chantmpls);
  return channels;
}


weed_plant_t **weed_params_create(weed_plant_t *filter, boolean in) {
  // return set of parameters with default/host_default values
  // in==TRUE create in parameters for filter
  // in==FALSE create out parameters for filter

  weed_plant_t **params,**paramtmpls;
  int num_params;
  int i,error;

  if (in) num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  else num_params=weed_leaf_num_elements(filter,"out_parameter_templates");

  if (num_params==0) return NULL;
  if (in) paramtmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  else paramtmpls=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  params=(weed_plant_t **)lives_malloc((num_params+1)*sizeof(weed_plant_t *));

  for (i=0; i<num_params; i++) {
    params[i]=weed_plant_new(WEED_PLANT_PARAMETER);
    weed_set_plantptr_value(params[i],"template",paramtmpls[i]);
    if (in) {
      if (weed_plant_has_leaf(paramtmpls[i],"host_default")) {
        weed_leaf_copy(params[i],"value",paramtmpls[i],"host_default");
      } else weed_leaf_copy(params[i],"value",paramtmpls[i],"default");
      weed_add_plant_flags(params[i],WEED_LEAF_READONLY_PLUGIN);
    } else {
      weed_leaf_copy(params[i],"value",paramtmpls[i],"default");
    }
  }
  params[num_params]=NULL;
  lives_free(paramtmpls);

  return params;
}



static void set_default_channel_sizes(weed_plant_t **in_channels, weed_plant_t **out_channels) {
  // set some reasonable default channel sizes when we first init the effect
  weed_plant_t *channel,*chantmpl;

  int *rowstrides;

  boolean is_gen=TRUE;
  boolean has_aud_in_chans=FALSE;

  int def_rowstride;
  int numplanes,width,height;
  int error;

  register int i;

  // ignore filters with no in/out channels (e.g. data processors)
  if ((in_channels==NULL||in_channels[0]==NULL)&&(out_channels==NULL||out_channels[0]==NULL)) return;

  for (i=0; in_channels!=NULL&&in_channels[i]!=NULL&&
       !(weed_plant_has_leaf(in_channels[i],"disabled")&&
         weed_get_boolean_value(in_channels[i],"disabled",&error)==WEED_TRUE); i++) {
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
      create_empty_pixel_data(channel,FALSE,TRUE);
      width=weed_get_int_value(channel,"width",&error);
      height=weed_get_int_value(channel,"height",&error);
      numplanes=weed_leaf_num_elements(channel,"rowstrides");
      rowstrides=weed_get_int_array(channel,"rowstrides",&error);
      set_channel_size(channel,width,height,numplanes,rowstrides);
      lives_free(rowstrides);
      weed_layer_pixel_data_free(channel);
    } else {
      if (mainw->current_file==-1) {
        weed_set_int_value(channel,"audio_channels",DEFAULT_AUDIO_CHANS);
        weed_set_int_value(channel,"audio_rate",DEFAULT_AUDIO_RATE);
      } else {
        weed_set_int_value(channel,"audio_channels",cfile->achans);
        weed_set_int_value(channel,"audio_rate",cfile->arate);
      }
      weed_set_boolean_value(channel,"audio_interleaf",WEED_FALSE);
      weed_set_int_value(channel,"audio_data_length",0);
      weed_set_voidptr_value(channel,"audio_data",NULL);

      has_aud_in_chans=TRUE;
    }
  }

  for (i=0; out_channels!=NULL&&out_channels[i]!=NULL; i++) {
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
      create_empty_pixel_data(channel,FALSE,TRUE);
      width=weed_get_int_value(channel,"width",&error);
      height=weed_get_int_value(channel,"height",&error);
      numplanes=weed_leaf_num_elements(channel,"rowstrides");
      rowstrides=weed_get_int_array(channel,"rowstrides",&error);
      set_channel_size(channel,width,height,numplanes,rowstrides);
      lives_free(rowstrides);
      weed_layer_pixel_data_free(channel);

    } else {
      if (mainw->current_file==-1||!has_aud_in_chans) {
        weed_set_int_value(channel,"audio_channels",DEFAULT_AUDIO_CHANS);
        weed_set_int_value(channel,"audio_rate",DEFAULT_AUDIO_RATE);
      } else {
        weed_set_int_value(channel,"audio_channels",cfile->achans);
        weed_set_int_value(channel,"audio_rate",cfile->arate);
      }
      weed_set_boolean_value(channel,"audio_interleaf",WEED_FALSE);
      weed_set_int_value(channel,"audio_data_length",0);
      weed_set_voidptr_value(channel,"audio_data",NULL);
    }
  }
}


static weed_plant_t *weed_create_instance(weed_plant_t *filter, weed_plant_t **inc, weed_plant_t **outc,
    weed_plant_t **inp, weed_plant_t **outp) {
  // here we create a new filter_instance from the ingredients:
  // filter_class, in_channels, out_channels, in_parameters, out_parameters
  weed_plant_t *inst=weed_plant_new(WEED_PLANT_FILTER_INSTANCE);

  int n;
  int flags=WEED_LEAF_READONLY_PLUGIN;

  register int i;

  weed_set_plantptr_value(inst,"filter_class",filter);
  if (inc!=NULL) weed_set_plantptr_array(inst,"in_channels",weed_flagset_array_count(inc,TRUE),inc);
  if (outc!=NULL) weed_set_plantptr_array(inst,"out_channels",weed_flagset_array_count(outc,TRUE),outc);
  if (inp!=NULL) weed_set_plantptr_array(inst,"in_parameters",weed_flagset_array_count(inp,TRUE),inp);
  if (outp!=NULL) {
    weed_set_plantptr_array(inst,"out_parameters",(n=weed_flagset_array_count(outp,TRUE)),outp);
    for (i=0; i<n; i++) {
      // allow plugins to set out_param "value"
      weed_leaf_set_flags(outp[i],"value",(weed_leaf_get_flags(outp[i],"value")|flags)^flags);
    }
  }

  weed_set_int_value(inst,"host_refs",1);
  weed_set_boolean_value(inst,"host_inited",WEED_FALSE);

  weed_add_plant_flags(inst,WEED_LEAF_READONLY_PLUGIN);
  return inst;
}



void add_param_connections(weed_plant_t *inst) {
  weed_plant_t **in_ptmpls,**outp;
  weed_plant_t *iparam,*oparam,*first_inst=inst,*filter=weed_instance_get_filter(inst,TRUE);

  int *xvals;

  int nptmpls,error;

  register int i;

  if (weed_plant_has_leaf(filter,"in_parameter_templates")) nptmpls=weed_leaf_num_elements(filter,"in_parameter_templates");
  else return;

  if (nptmpls!=0&&weed_get_plantptr_value(filter,"in_parameter_templates",&error)!=NULL) {
    in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
    for (i=0; i<nptmpls; i++) {
      if (weed_plant_has_leaf(in_ptmpls[i],"host_internal_connection")) {
        xvals=weed_get_int_array(in_ptmpls[i],"host_internal_connection",&error);

        iparam=weed_inst_in_param(first_inst,i,FALSE,FALSE);

        if (xvals[0]>-1) {
          inst=first_inst;
          while (--xvals[0]>=0) inst=weed_get_plantptr_value(inst,"host_next_instance",&error);

          outp=weed_get_plantptr_array(inst,"out_parameters",&error);
          oparam=outp[xvals[1]];
          lives_free(outp);
        } else oparam=iparam; // just hide the parameter, but don't pull a value

        weed_set_plantptr_value(iparam,"host_internal_connection",oparam);

        if (weed_plant_has_leaf(in_ptmpls[i],"host_internal_connection_autoscale")&&
            weed_get_boolean_value(in_ptmpls[i],"host_internal_connection_autoscale",&error)==WEED_TRUE)
          weed_set_boolean_value(iparam,"host_internal_connection_autoscale",WEED_TRUE);

        lives_free(xvals);
      }
    }
    lives_free(in_ptmpls);
  }
}



weed_plant_t *weed_instance_from_filter(weed_plant_t *filter) {
  // return an instance from a filter
  weed_plant_t **inc,**outc,**inp,**outp,**xinp;

  weed_plant_t *last_inst=NULL,*first_inst=NULL,*inst,*ofilter=filter;
  weed_plant_t *ochan=NULL;

  int *filters=NULL,*xvals;

  char *key;

  int nfilters,ninpar=0,error;

  int xcnumx=0,x,poffset=0;

  register int i,j;

  if ((nfilters=num_compound_fx(filter))>1) {
    filters=weed_get_int_array(filter,"host_filter_list",&error);
  }

  inp=weed_params_create(filter,TRUE);

  for (i=0; i<nfilters; i++) {

    if (filters!=NULL) {
      filter=weed_filters[filters[i]];
    }

    // create channels from channel_templates
    inc=weed_channels_create(filter,TRUE);
    outc=weed_channels_create(filter,FALSE);

    set_default_channel_sizes(inc,outc);  // we set the initial channel sizes to some reasonable defaults

    // create parameters from parameter_templates
    outp=weed_params_create(filter,FALSE);

    if (nfilters==1) xinp=inp;
    else {
      // for a compound filter, assign only the params which belong to the instance
      ninpar=num_in_params(filter,FALSE,FALSE);
      if (ninpar==0) xinp=NULL;
      else {
        xinp=(weed_plant_t **)lives_malloc((ninpar+1)*sizeof(weed_plant_t *));
        x=0;
        for (j=poffset; j<poffset+ninpar; j++) xinp[x++]=inp[j];
        xinp[x]=NULL;
        poffset+=ninpar;
      }
    }

    inst=weed_create_instance(filter,inc,outc,xinp,outp);

    if (filters!=NULL) weed_set_plantptr_value(inst,"host_compound_class",ofilter);

    if (i>0) {
      weed_set_plantptr_value(last_inst,"host_next_instance",inst);
    } else first_inst=inst;

    last_inst=inst;

    if (inc!=NULL) lives_free(inc);
    if (outc!=NULL) lives_free(outc);
    if (outp!=NULL) lives_free(outp);
    if (xinp!=inp) lives_free(xinp);

    for (j=0; j<ninpar; j++) {
      // copy param values if that was set up
      set_copy_to(inst,j,TRUE);
    }

  }

  if (inp!=NULL) lives_free(inp);

  if (filters!=NULL) {
    lives_free(filters);

    // for compound fx, add param and channel connections
    filter=ofilter;

    // add param connections
    add_param_connections(first_inst);

    while (1) {
      // add channel connections
      key=lives_strdup_printf("host_channel_connection%d",xcnumx++);
      if (weed_plant_has_leaf(filter,key)) {
        xvals=weed_get_int_array(filter,key,&error);

        inst=first_inst;
        while (1) {
          if (xvals[0]--==0) {
            // got the out instance
            outc=weed_get_plantptr_array(inst,"out_channels",&error);
            ochan=outc[xvals[1]];
            lives_free(outc);
          }
          if (xvals[2]--==0) {
            // got the in instance
            inc=weed_get_plantptr_array(inst,"in_channels",&error);
            weed_set_plantptr_value(inc[xvals[3]],"host_internal_connection",ochan);
            lives_free(inc);
            break;
          }

          inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
        }

        lives_free(xvals);
        lives_free(key);

      } else {
        lives_free(key);
        break;
      }

    }

  }


  return first_inst;
}




boolean weed_init_effect(int hotkey) {
  // mainw->osc_block should be set to TRUE before calling this function !
  weed_plant_t *filter;
  weed_plant_t *new_instance,*inst;
  weed_plant_t *event_list;

  boolean fg_modeswitch=FALSE,is_trans=FALSE,gen_start=FALSE,is_modeswitch=FALSE;
  boolean is_audio_gen=FALSE;

  int num_tr_applied;
  int rte_keys=mainw->rte_keys;
  int inc_count,outc_count;
  int ntracks;
  int error;
  int idx;

  mainw->error=FALSE;

  if (hotkey<0) {
    is_modeswitch=TRUE;
    hotkey=-hotkey-1;
  }

  if (hotkey==fg_generator_key) {
    fg_modeswitch=TRUE;
  }

  if (hotkey>=FX_KEYS_MAX) {
    mainw->error=TRUE;
    return FALSE;
  }

  if (!rte_key_valid(hotkey+1,FALSE)) {
    mainw->error=TRUE;
    return FALSE;
  }

  idx=key_to_fx[hotkey][key_modes[hotkey]];

  inc_count=enabled_in_channels(weed_filters[idx],FALSE);
  outc_count=enabled_out_channels(weed_filters[idx],FALSE);

  // check first if it is an audio generator

  if ((inc_count==0||(has_audio_chans_in(weed_filters[idx],FALSE)&&
                      !has_video_chans_in(weed_filters[idx],TRUE)))&&
      has_audio_chans_out(weed_filters[idx],FALSE)&&
      !has_video_chans_out(weed_filters[idx],TRUE)) {

    if (!is_realtime_aplayer(prefs->audio_player)) {
      // audio fx only with realtime players
      char *fxname=weed_filter_idx_get_name(idx);
      d_print(_("Effect %s cannot be used with this audio player.\n"),fxname);
      lives_free(fxname);
      mainw->error=TRUE;
      return FALSE;
    }


    if (inc_count==0) {
      if (mainw->agen_key!=0) {
        // we had an existing audio gen running - stop that one first
        int agen_key=mainw->agen_key-1;
        weed_deinit_effect(agen_key);

        if ((mainw->rte&(GU641<<agen_key))) {
          // need to do this in case starting another audio gen caused us to come here
          mainw->rte^=(GU641<<agen_key);
          if (rte_window!=NULL) rtew_set_keych(agen_key,FALSE);
          if (mainw->ce_thumbs) ce_thumbs_set_keych(agen_key,FALSE);
        }

      }
      is_audio_gen=TRUE;
    }
  }

  // TODO - block template channel changes
  // we must stop any old generators

  if (inc_count==0&&outc_count>0&&hotkey!=fg_generator_key&&mainw->num_tr_applied>0&&mainw->blend_file!=-1&&
      mainw->blend_file!=mainw->current_file&&mainw->files[mainw->blend_file]!=NULL&&
      mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR&&inc_count==0&&!is_audio_gen) {
    if (bg_gen_to_start==-1) {
      weed_generator_end((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
    }
    bg_gen_to_start=bg_generator_key=bg_generator_mode=-1;
    mainw->blend_file=-1;
  }

  if (mainw->current_file>0&&cfile->clip_type==CLIP_TYPE_GENERATOR&&
      (fg_modeswitch||(inc_count==0&&outc_count>0&&mainw->num_tr_applied==0))&&!is_audio_gen) {
    if (mainw->noswitch||mainw->is_processing||mainw->preview) {
      mainw->error=TRUE;
      return FALSE; // stopping fg gen will cause clip to switch
    }
    if (mainw->playing_file>-1&&mainw->whentostop==STOP_ON_VID_END&&inc_count!=0) {
      mainw->cancelled=CANCEL_GENERATOR_END;
    } else {
      if (inc_count==0&&mainw->whentostop==STOP_ON_VID_END) mainw->whentostop=NEVER_STOP;
      weed_generator_end((weed_plant_t *)cfile->ext_src);
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
    if (mainw->active_sa_clips==SCREEN_AREA_FOREGROUND) {
      mainw->active_sa_clips=SCREEN_AREA_BACKGROUND;
    }
    if (mainw->ce_thumbs) ce_thumbs_liberate_clip_area_register(SCREEN_AREA_BACKGROUND);
    if (mainw->num_tr_applied==1&&!is_modeswitch) {
      mainw->blend_file=mainw->current_file;
    }
  } else if (inc_count==0&&outc_count>0&&!is_audio_gen) {
    // aha - a generator
    if (mainw->playing_file==-1) {
      // if we are not playing, we will postpone creating the instance
      // this is a workaround for a problem in libvisual
      fg_gen_to_start=hotkey;
      fg_generator_key=hotkey;
      fg_generator_mode=key_modes[hotkey];
      gen_start=TRUE;
    } else if (!fg_modeswitch&&mainw->num_tr_applied==0&&(mainw->noswitch||mainw->is_processing||mainw->preview))  {
      mainw->error=TRUE;
      return FALSE;
    }
  }

  filter=weed_filters[idx];

  // TODO - unblock template channel changes

  // if the param window is already open, use instance from there
  if (fx_dialog[1]!=NULL&&LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"key"))==hotkey&&
      LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"mode"))==key_modes[hotkey]) {
    lives_rfx_t *rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"rfx");
    new_instance=(weed_plant_t *)rfx->source;

    weed_instance_ref(new_instance);

    // handle compound fx
    inst=new_instance;
    while (weed_plant_has_leaf(inst,"host_next_instance")) {
      inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
      weed_instance_ref(inst);
    }
    redraw_pwindow(hotkey,key_modes[hotkey]);
  } else {
    new_instance=weed_instance_from_filter(filter);
    // if it is a key effect, set key defaults
    if (hotkey<FX_KEYS_MAX_VIRTUAL&&key_defaults[hotkey][key_modes[hotkey]]!=NULL) {
      // TODO - handle compound fx
      weed_reinit_effect(new_instance,FALSE);
      apply_key_defaults(new_instance,hotkey,key_modes[hotkey]);
      weed_reinit_effect(new_instance,FALSE);
    }
  }

  update_host_info(new_instance);

  // record the key so we know whose parameters to record later
  weed_set_int_value(new_instance,"host_key",hotkey);

  // handle compound fx
  inst=new_instance;
  while (weed_plant_has_leaf(inst,"host_next_instance")) {
    inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
    update_host_info(inst);
    weed_set_int_value(inst,"host_key",hotkey);
  }


  if (!gen_start) {
    weed_plant_t *inst,*next_inst=NULL;

    inst=new_instance;

start1:

    filter=weed_instance_get_filter(inst,FALSE);

    if (weed_plant_has_leaf(filter,"init_func")) {
      weed_init_f *init_func_ptr_ptr;
      weed_init_f init_func;
      char *cwd=cd_to_plugin_dir(filter);
      weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
      init_func=init_func_ptr_ptr[0];
      set_param_gui_readwrite(inst);
      if (init_func!=NULL&&(error=(*init_func)(inst))!=WEED_NO_ERROR) {
        int weed_error;
        char *filter_name;
        filter=weed_filters[idx];
        filter_name=weed_get_string_value(filter,"name",&weed_error);
        set_param_gui_readonly(inst);
        d_print(_("Failed to start instance %s, error code %d\n"),filter_name,error);
        lives_free(filter_name);
        filter_mutex_lock(hotkey);
        key_to_instance[hotkey][key_modes[hotkey]]=NULL;
        filter_mutex_unlock(hotkey);

        inst=new_instance;

deinit2:

        if (weed_plant_has_leaf(inst,"host_next_instance")) next_inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
        else next_inst=NULL;

        weed_call_deinit_func(inst);
        weed_instance_unref(inst);

        if (next_inst!=NULL&&weed_get_boolean_value(next_inst,"host_inited",&error)==WEED_TRUE) {
          // handle compound fx
          inst=next_inst;
          goto deinit2;
        }

        if (is_trans) {
          // TODO: - do we need this ? is_trans is always FALSE !
          mainw->num_tr_applied--;
          if (mainw->num_tr_applied==0) {
            if (mainw->ce_thumbs) ce_thumbs_liberate_clip_area_register(SCREEN_AREA_FOREGROUND);
            if (mainw->active_sa_clips==SCREEN_AREA_BACKGROUND) {
              mainw->active_sa_clips=SCREEN_AREA_FOREGROUND;
            }
          }
        }


        lives_chdir(cwd,FALSE);
        lives_free(cwd);
        if (is_audio_gen) mainw->agen_needs_reinit=FALSE;
        mainw->error=TRUE;
        return FALSE;
      }
      set_param_gui_readonly(inst);
      lives_chdir(cwd,FALSE);
      lives_free(cwd);
    }

    weed_set_boolean_value(inst,"host_inited",WEED_TRUE);

    if (weed_plant_has_leaf(inst,"host_next_instance")) {
      inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
      goto start1;
    }

    filter=weed_filters[idx];

  }


  if (inc_count==0&&outc_count>0&&!is_audio_gen) {
    // generator start
    if (mainw->num_tr_applied>0&&!fg_modeswitch&&mainw->current_file>-1&&mainw->playing_file>-1) {
      // transition is on, make into bg clip
      bg_generator_key=hotkey;
      bg_generator_mode=key_modes[hotkey];
    } else {
      // no transition, make fg (or kb was grabbed for fg generator)
      fg_generator_key=hotkey;
      fg_generator_mode=key_modes[hotkey];
    }

    // start the generator and maybe start playing
    num_tr_applied=mainw->num_tr_applied;
    if (fg_modeswitch) mainw->num_tr_applied=0; // force to fg

    key_to_instance[hotkey][key_modes[hotkey]]=new_instance;

    if (!weed_generator_start(new_instance,hotkey)) {
      // TODO - be more descriptive with error
      int weed_error;
      char *filter_name=weed_get_string_value(filter,"name",&weed_error);
      d_print(_("Unable to start generator %s\n"),filter_name);
      lives_free(filter_name);
      if (mainw->num_tr_applied&&mainw->current_file>-1) {
        bg_gen_to_start=bg_generator_key=bg_generator_mode=-1;
      } else {
        fg_generator_key=fg_generator_clip=fg_generator_mode=-1;
      }
      if (fg_modeswitch) mainw->num_tr_applied=num_tr_applied;
      key_to_instance[hotkey][key_modes[hotkey]]=NULL;

      if (mainw->playing_file==-1) {
        int current_file=mainw->current_file;
        switch_to_file(mainw->current_file=0,current_file);
      }

      mainw->error=TRUE;
      return FALSE;
    }

    // weed_generator_start can change the instance
    new_instance=key_to_instance[hotkey][key_modes[hotkey]];

    // TODO - problem if modeswitch triggers playback
    // hence we do not allow mixing of generators and non-gens on the same key
    if (fg_modeswitch) mainw->num_tr_applied=num_tr_applied;
    if (fg_generator_key!=-1) {
      mainw->rte|=(GU641<<fg_generator_key);
      mainw->clip_switched=TRUE;
    }
    if (bg_generator_key!=-1&&!fg_modeswitch) {
      mainw->rte|=(GU641<<bg_generator_key);
      if (hotkey<prefs->rte_keys_virtual) {
        if (rte_window!=NULL) rtew_set_keych(bg_generator_key,TRUE);
        if (mainw->ce_thumbs) ce_thumbs_set_keych(bg_generator_key,TRUE);
      }
    }
  }


  if (rte_keys==hotkey) {
    mainw->rte_keys=rte_keys;
    mainw->blend_factor=weed_get_blend_factor(rte_keys);
  }

  // need to do this *before* calling append_filter_map_event
  key_to_instance[hotkey][key_modes[hotkey]]=new_instance;

  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)&&(inc_count>0||outc_count==0)) {
    uint64_t rteval,new_rte;
    // place this synchronous with the preceding frame
    pthread_mutex_lock(&mainw->event_list_mutex);
    event_list=append_filter_init_event(mainw->event_list,mainw->currticks,
                                        idx,-1,hotkey,new_instance);
    if (mainw->event_list==NULL) mainw->event_list=event_list;
    init_events[hotkey]=get_last_event(mainw->event_list);
    ntracks=weed_leaf_num_elements(init_events[hotkey],"in_tracks");
    pchains[hotkey]=filter_init_add_pchanges(mainw->event_list,new_instance,init_events[hotkey],ntracks,0);
    rteval=mainw->rte;
    new_rte=GU641<<(hotkey);
    if (!(rteval&new_rte)) rteval|=new_rte;
    create_filter_map(rteval); // we create filter_map event_t * array with ordered effects
    mainw->event_list=append_filter_map_event(mainw->event_list,mainw->currticks,filter_map);
    pthread_mutex_unlock(&mainw->event_list_mutex);
  }

  if (is_audio_gen) {
    mainw->agen_key=hotkey+1;
    mainw->agen_needs_reinit=FALSE;

    if (mainw->playing_file>0) {
      if (mainw->whentostop==STOP_ON_AUD_END) mainw->whentostop=STOP_ON_VID_END;
      if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
        if (mainw->jackd_read!=NULL&&mainw->jackd_read->in_use&&
            (mainw->jackd_read->playing_file==-1||mainw->jackd_read->playing_file==mainw->ascrap_file)) {
          // if playing external audio, switch over to internal for an audio gen
          mainw->jackd->audio_ticks=mainw->currticks;
          mainw->jackd->frames_written=0;

          // close the reader
          jack_rec_audio_end(!(prefs->perm_audio_reader&&prefs->audio_src==AUDIO_SRC_EXT),FALSE);
        }
        if (mainw->jackd!=NULL&&(mainw->jackd_read==NULL||!mainw->jackd_read->in_use)) {

          // enable writer
          mainw->jackd->in_use=TRUE;
        }
#endif
      }
      if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
        if (mainw->pulsed_read!=NULL&&mainw->pulsed_read->in_use&&
            (mainw->pulsed_read->playing_file==-1||mainw->pulsed_read->playing_file==mainw->ascrap_file)) {
          // if playing external audio, switch over to internal for an audio gen
          mainw->pulsed->audio_ticks=mainw->currticks;
          mainw->pulsed->frames_written=0;
          pulse_rec_audio_end(!(prefs->perm_audio_reader&&prefs->audio_src==AUDIO_SRC_EXT),FALSE);
        }
        if (mainw->pulsed!=NULL&&(mainw->pulsed_read==NULL||!mainw->pulsed_read->in_use)) {
          mainw->pulsed->in_use=TRUE;
        }
#endif
      }

      if (mainw->playing_file>0&&mainw->record&&!mainw->record_paused&&!prefs->audio_src==AUDIO_SRC_EXT&&(prefs->rec_opts&REC_AUDIO)) {
        // if recording audio, open ascrap file and add audio event
        mainw->record=FALSE;
        on_record_perf_activate(NULL,NULL);
        mainw->record_starting=FALSE;
        mainw->record=TRUE;
        if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
          mainw->pulsed->audio_ticks=mainw->currticks;
          mainw->pulsed->frames_written=0;
#endif
        }
        if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
          mainw->jackd->audio_ticks=mainw->currticks;
          mainw->jackd->frames_written=0;
#endif
        }

      }

    }
  }

  return TRUE;
}





int weed_call_init_func(weed_plant_t *inst) {
  int error=0;

  weed_plant_t *filter=weed_instance_get_filter(inst,FALSE);
  if (weed_plant_has_leaf(filter,"init_func")) {
    weed_init_f *init_func_ptr_ptr;
    weed_init_f init_func;
    weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
    init_func=init_func_ptr_ptr[0];
    update_host_info(inst);
    if (init_func!=NULL) {
      char *cwd=cd_to_plugin_dir(filter);
      set_param_gui_readwrite(inst);
      error=(*init_func)(inst);
      set_param_gui_readonly(inst);
      lives_chdir(cwd,FALSE);
      lives_free(cwd);
    }
  }
  weed_set_boolean_value(inst,"host_inited",WEED_TRUE);
  return error;
}


int weed_call_deinit_func(weed_plant_t *instance) {
  int error=0;

  weed_plant_t *filter=weed_instance_get_filter(instance,FALSE);
  if (weed_plant_has_leaf(instance,"host_inited")&&weed_get_boolean_value(instance,"host_inited",&error)==WEED_FALSE) return 1;
  if (weed_plant_has_leaf(filter,"deinit_func")) {
    weed_deinit_f *deinit_func_ptr_ptr;
    weed_deinit_f deinit_func;
    weed_leaf_get(filter,"deinit_func",0,(void *)&deinit_func_ptr_ptr);
    deinit_func=deinit_func_ptr_ptr[0];
    if (deinit_func!=NULL) {
      char *cwd=cd_to_plugin_dir(filter);
      error=(*deinit_func)(instance);
      lives_chdir(cwd,FALSE);
      lives_free(cwd);
    }
  }
  weed_set_boolean_value(instance,"host_inited",WEED_FALSE);
  return error;
}




void weed_deinit_effect(int hotkey) {
  // mainw->osc_block should be set before calling this function !
  // caller should also handle mainw->rte

  weed_plant_t *instance,*inst,*last_inst,*next_inst;

  boolean is_modeswitch=FALSE;
  boolean was_transition=FALSE;
  boolean is_audio_gen=FALSE;
  int num_in_chans,num_out_chans;

  int error;

  if (hotkey<0) {
    is_modeswitch=TRUE;
    hotkey=-hotkey-1;
  }

  if (hotkey>=FX_KEYS_MAX) return;

  if ((instance=key_to_instance[hotkey][key_modes[hotkey]])==NULL) return;

  num_in_chans=enabled_in_channels(instance,FALSE);

  // handle compound fx
  last_inst=instance;
  while (weed_plant_has_leaf(last_inst,"host_next_instance")) last_inst=weed_get_plantptr_value(last_inst,"host_next_instance",&error);
  num_out_chans=enabled_out_channels(last_inst,FALSE);

  if (hotkey+1==mainw->agen_key) is_audio_gen=TRUE;

  if (num_in_chans==0&&num_out_chans>0&&!is_audio_gen) {
    // is (video) generator
    if (mainw->playing_file>-1&&mainw->whentostop==STOP_ON_VID_END&&(hotkey!=bg_generator_key)) {
      mainw->cancelled=CANCEL_GENERATOR_END;
    } else {
      weed_generator_end(instance);
    }
    return;
  }

  if (is_audio_gen) {
    // is audio generator
    // wait for current processing to finish
    pthread_mutex_lock(&mainw->interp_mutex);
    mainw->agen_key=0;
    pthread_mutex_unlock(&mainw->interp_mutex);
    mainw->agen_samps_count=0;


    // for external audio, switch back to reading

    if (mainw->playing_file>0&&prefs->audio_src==AUDIO_SRC_EXT) {
      if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
        if (mainw->jackd_read==NULL||!mainw->jackd_read->in_use) {
          mainw->jackd->in_use=FALSE; // deactivate writer
          jack_rec_audio_to_clip(-1,0,(lives_rec_audio_type_t)0); //activate reader
          mainw->jackd_read->frames_written+=mainw->jackd->frames_written; // ensure time continues monotonically
          if (mainw->record) mainw->jackd->playing_file=mainw->ascrap_file; // if recording, continue to write to
        }
#endif
      }
      if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
        if (mainw->pulsed_read==NULL||!mainw->pulsed_read->in_use) {
          mainw->pulsed->in_use=FALSE; // deactivate writer
          pulse_rec_audio_to_clip(-1,0,(lives_rec_audio_type_t)0); //activate reader
          mainw->pulsed_read->frames_written+=mainw->pulsed->frames_written; // ensure time continues monotonically
          if (mainw->record) mainw->pulsed->playing_file=mainw->ascrap_file; // if recording, continue to write to
        }
#endif
      }
    } else if (mainw->playing_file>0) {
      // for internal, continue where we should
      if (is_realtime_aplayer(prefs->audio_player)) {
        if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS) switch_audio_clip(mainw->playing_file,TRUE);
        else switch_audio_clip(mainw->pre_src_audio_file,TRUE);
      }
    }
  }

  filter_mutex_lock(hotkey);
  key_to_instance[hotkey][key_modes[hotkey]]=NULL;
  filter_mutex_unlock(hotkey);

  if (mainw->whentostop==STOP_ON_VID_END&&mainw->current_file>-1&&
      (cfile->frames==0||(mainw->loop&&cfile->achans>0&&!mainw->is_rendering&&(mainw->audio_end/cfile->fps)
                          <MAX(cfile->laudio_time,cfile->raudio_time)))) mainw->whentostop=STOP_ON_AUD_END;

  if (num_in_chans==2) {
    was_transition=TRUE;
    mainw->num_tr_applied--;
    if (mainw->num_tr_applied==0) {
      if (mainw->ce_thumbs) ce_thumbs_liberate_clip_area_register(SCREEN_AREA_FOREGROUND);
      if (mainw->active_sa_clips==SCREEN_AREA_BACKGROUND) {
        mainw->active_sa_clips=SCREEN_AREA_FOREGROUND;
      }
    }
  }

  inst=instance;

deinit3:

  if (weed_plant_has_leaf(inst,"host_next_instance")) next_inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
  else next_inst=NULL;

  weed_call_deinit_func(inst);
  weed_instance_unref(inst);

  if (next_inst!=NULL) {
    // handle compound fx
    inst=next_inst;
    goto deinit3;
  }


  // if the param window is already open, show any reinits now
  if (fx_dialog[1]!=NULL&&LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"key"))==hotkey&&
      LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"mode"))==key_modes[hotkey]) {
    redraw_pwindow(hotkey,key_modes[hotkey]);
  }


  if (was_transition&&!is_modeswitch) {
    if (mainw->num_tr_applied<1) {
      if (bg_gen_to_start!=-1) bg_gen_to_start=-1;
      if (mainw->blend_file!=-1&&mainw->blend_file!=mainw->current_file&&mainw->files[mainw->blend_file]!=NULL&&
          mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR) {
        // all transitions off, so end the bg generator
        weed_generator_end((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
      }
      mainw->blend_file=-1;
    }
  }
  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&init_events[hotkey]!=NULL&&
      (prefs->rec_opts&REC_EFFECTS)&&num_in_chans>0) {
    uint64_t rteval,new_rte;
    // place this synchronous with the preceding frame
    pthread_mutex_lock(&mainw->event_list_mutex);
    mainw->event_list=append_filter_deinit_event(mainw->event_list,mainw->currticks,init_events[hotkey],pchains[hotkey]);
    init_events[hotkey]=NULL;
    if (pchains[hotkey]!=NULL) lives_free(pchains[hotkey]);
    rteval=mainw->rte;
    new_rte=GU641<<(hotkey);
    if (rteval&new_rte) rteval^=new_rte;
    create_filter_map(rteval); // we create filter_map event_t * array with ordered effects
    mainw->event_list=append_filter_map_event(mainw->event_list,mainw->currticks,filter_map);
    pthread_mutex_unlock(&mainw->event_list_mutex);
  }
}


void deinit_render_effects(void) {
  // during rendering/render preview, we use the "keys" FX_KEYS_MAX_VIRTUAL -> FX_KEYS_MAX
  // here we deinit all active ones, similar to weed_deinit_all, but we use higher keys
  register int i;

  for (i=FX_KEYS_MAX_VIRTUAL; i<FX_KEYS_MAX; i++) {
    if (key_to_instance[i][0]!=NULL) {
      weed_deinit_effect(i);
      if (mainw->multitrack!=NULL&&mainw->multitrack->is_rendering) lives_free(pchains[i]);
    }
  }
}


void weed_deinit_all(boolean shutdown) {
  // deinit all (except generators* during playback)
  // this is called on ctrl-0 or on shutdown

  // * background generators will be killed because their transition will be deinited

  int i;
  weed_plant_t *instance;

  mainw->osc_block=TRUE;
  if (rte_window!=NULL) {
    rtew_set_keygr(-1);
  }

  mainw->rte_keys=-1;
  mainw->last_grabbable_effect=-1;

  for (i=0; i<FX_KEYS_MAX_VIRTUAL; i++) {
    if (rte_key_valid(i+1,TRUE)) {
      if (rte_window!=NULL) rtew_set_keych(i,FALSE);
      if (mainw->ce_thumbs) ce_thumbs_set_keych(i,FALSE);
    }
    if ((mainw->rte&(GU641<<i))) {
      if ((instance=key_to_instance[i][key_modes[i]])!=NULL) {
        if (shutdown||mainw->playing_file==-1||(enabled_in_channels(instance,FALSE)>0)) {
          weed_deinit_effect(i);
          mainw->rte^=(GU641<<i);
        }
      }
    }
  }

  mainw->osc_block=FALSE;
}


/////////////////////
// special handling for generators (sources)


weed_plant_t *weed_layer_new_from_generator(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *channel,**out_channels;
  weed_plant_t *filter;
  weed_plant_t *chantmpl;
  weed_plant_t *achan;
  weed_process_f *process_func_ptr_ptr;
  weed_process_f process_func;

  lives_filter_error_t retval;

  int num_channels;
  int error;
  int flags;
  int palette;
  int filter_flags=0;
  int key=-1;

  boolean did_thread=FALSE;

  char *cwd;

  if (inst==NULL) return NULL;

  if ((num_channels=weed_leaf_num_elements(inst,"out_channels"))==0) return NULL;
  out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
  if ((channel=get_enabled_channel(inst,0,FALSE))==NULL) {
    lives_free(out_channels);
    return NULL;
  }

  chantmpl=weed_get_plantptr_value(channel,"template",&error);
  palette=weed_get_int_value(chantmpl,"current_palette",&error);
  weed_set_int_value(channel,"current_palette",palette);

  create_empty_pixel_data(channel,FALSE,TRUE);

  // align memory if necessary
  if (weed_plant_has_leaf(chantmpl,"alignment")) {
    int alignment=weed_get_int_value(chantmpl,"alignment",&error);
    void **pixel_data=weed_get_voidptr_array(channel,"pixel_data",&error);
    int numplanes=weed_leaf_num_elements(channel,"pixel_data");
    int height=weed_get_int_value(channel,"height",&error);
    int *rowstrides=weed_get_int_array(channel,"rowstrides",&error);
    boolean contiguous=FALSE;
    if (weed_plant_has_leaf(channel,"host_pixel_data_contiguous") &&
        weed_get_boolean_value(channel,"host_pixel_data_contiguous",&error)==WEED_TRUE) contiguous=TRUE;
    align(pixel_data,alignment,numplanes,height,rowstrides,(int *)&contiguous);
    weed_set_voidptr_array(channel,"pixel_data",numplanes,pixel_data);
    lives_free(pixel_data);
    lives_free(rowstrides);
    if (contiguous) weed_set_boolean_value(channel,"host_pixel_data_contiguous",WEED_TRUE);
    else if (weed_plant_has_leaf(channel,"host_pixel_data_contiguous"))
      weed_leaf_delete(channel,"host_pixel_data_contiguous");
  }

  // if we have an optional audio channel, we can push audio to it
  if ((achan=get_enabled_audio_channel(inst,0,TRUE))!=NULL) {
    if (mainw->audio_frame_buffer!=NULL&&mainw->audio_frame_buffer->samples_filled>0) {
      // lock the buffer, convert audio to format requested, and copy it to the audio channel data
      pthread_mutex_lock(&mainw->abuf_frame_mutex);
      push_audio_to_channel(achan,mainw->audio_frame_buffer);
      free_audio_frame_buffer(mainw->audio_frame_buffer); // TODO: this needs reimplementing to handle the case where we have fg and bg gens
      pthread_mutex_unlock(&mainw->abuf_frame_mutex);
    } else {
      // no audio has been buffered
      weed_set_int_value(achan,"audio_data_length",0);
      weed_set_voidptr_value(achan,"audio_data",NULL);
    }
  }

  weed_set_double_value(inst,"fps",cfile->pb_fps);

  filter=weed_instance_get_filter(inst,FALSE);
  cwd=cd_to_plugin_dir(filter);

  if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);

  // see if we can multithread
  if ((prefs->nfx_threads=future_prefs->nfx_threads)>1 && weed_plant_has_leaf(filter,"flags"))
    filter_flags=weed_get_int_value(filter,"flags",&error);

  if (filter_flags&WEED_FILTER_HINT_MAY_THREAD) {
    filter_mutex_lock(key);
    retval=process_func_threaded(inst,out_channels,tc);
    filter_mutex_unlock(key);
    if (retval!=FILTER_ERROR_DONT_THREAD) did_thread=TRUE;
  }
  if (!did_thread) {
    // normal single threaded version
    weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
    process_func=process_func_ptr_ptr[0];
    filter_mutex_lock(key);
    (*process_func)(inst,tc);
    filter_mutex_unlock(key);
  }

  lives_free(out_channels);

  if (achan!=NULL) {
    void *abuf=weed_get_voidptr_value(achan,"audio_data",&error);
    if (abuf!=NULL) lives_free(abuf);
  }

  lives_chdir(cwd,FALSE);
  lives_free(cwd);

  chantmpl=weed_get_plantptr_value(channel,"template",&error);

  if (weed_plant_has_leaf(chantmpl,"flags")) flags=weed_get_int_value(chantmpl,"flags",&error);
  else flags=0;

  if (flags&WEED_CHANNEL_OUT_ALPHA_PREMULT) weed_set_int_value(channel,"flags",WEED_CHANNEL_ALPHA_PREMULT);

  return channel;
}


boolean weed_generator_start(weed_plant_t *inst, int key) {
  // key here is zero based
  // make an "ephemeral clip"

  // cf. yuv4mpeg.c
  // start "playing" but receive frames from a plugin

  weed_plant_t **out_channels,*channel,*filter,*achan;
  char *filter_name;
  int error,num_channels;
  int old_file=mainw->current_file,blend_file=mainw->blend_file;
  int palette;

  // create a virtual clip
  int new_file=0;
  boolean is_bg=FALSE;

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

  if ((mainw->preview||(mainw->current_file>-1&&cfile!=NULL&&cfile->opening))&&
      (mainw->num_tr_applied==0||mainw->blend_file==-1||mainw->blend_file==mainw->current_file)) return FALSE;

  if (mainw->playing_file==-1) mainw->pre_src_file=mainw->current_file;

  if (old_file!=-1&&mainw->blend_file!=-1&&mainw->blend_file!=mainw->current_file&&
      mainw->num_tr_applied>0&&mainw->files[mainw->blend_file]!=NULL&&
      mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR) {
    weed_generator_end((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
    mainw->current_file=mainw->blend_file;
  }

  // old_file can also be -1 if we are doing a fg_modeswitch
  if (old_file>-1&&mainw->playing_file>-1&&mainw->num_tr_applied>0) is_bg=TRUE;

  filter=weed_instance_get_filter(inst,FALSE);

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

    cfile=(lives_clip_t *)(lives_malloc(sizeof(lives_clip_t)));
    lives_snprintf(cfile->handle,256,"ephemeral%d",mainw->current_file);
    create_cfile();
    cfile->clip_type=CLIP_TYPE_GENERATOR;
    get_next_free_file();

    filter_name=weed_get_string_value(filter,"name",&error);
    lives_snprintf(cfile->type,40,"generator:%s",filter_name);
    lives_snprintf(cfile->file_name,PATH_MAX,"generator: %s",filter_name);
    lives_snprintf(cfile->name,256,"generator: %s",filter_name);
    lives_free(filter_name);
    cfile->achans=0;
    cfile->asampsize=0;

    // open as a clip with 1 frame
    cfile->start=cfile->end=cfile->frames=1;
    cfile->arps=cfile->arate=0;
    cfile->changed=FALSE;
  }

  cfile->ext_src=inst;

  if (is_bg) {
    mainw->blend_file=mainw->current_file;
    if (mainw->ce_thumbs&&mainw->active_sa_clips==SCREEN_AREA_BACKGROUND) ce_thumbs_highlight_current_clip();
  }

  if (!is_bg||old_file==-1||old_file==new_file) fg_generator_clip=new_file;

  if (weed_plant_has_leaf(inst,"target_fps")) {
    // if plugin sets "target_fps" for the instance we assume there is some special reason
    // and use that
    cfile->pb_fps=cfile->fps=weed_get_double_value(inst,"target_fps",&error);
  } else {
    if (weed_plant_has_leaf(filter,"host_fps")) cfile->pb_fps=cfile->fps=weed_get_double_value(filter,"host_fps",&error);
    else if (weed_plant_has_leaf(filter,"target_fps"))
      cfile->pb_fps=cfile->fps=weed_get_double_value(filter,"target_fps",&error);
    else {
      cfile->pb_fps=cfile->fps=prefs->default_fps;
    }
  }

  if ((num_channels=weed_leaf_num_elements(inst,"out_channels"))==0) {
    close_current_file(mainw->pre_src_file);
    return FALSE;
  }
  out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
  if ((channel=get_enabled_channel(inst,0,FALSE))==NULL) {
    lives_free(out_channels);
    close_current_file(mainw->pre_src_file);
    return FALSE;
  }
  lives_free(out_channels);

  cfile->hsize=weed_get_int_value(channel,"width",&error);
  cfile->vsize=weed_get_int_value(channel,"height",&error);

  palette=weed_get_int_value(channel,"current_palette",&error);
  if (palette==WEED_PALETTE_RGBA32||palette==WEED_PALETTE_ARGB32||palette==WEED_PALETTE_BGRA32) cfile->bpp=32;
  else cfile->bpp=24;

  cfile->opening=FALSE;
  cfile->proc_ptr=NULL;

  // if the generator has an optional audio in channel, enable it: TODO - make this configurable
  if ((achan=get_audio_channel_in(inst,0))!=NULL) {
    if (weed_plant_has_leaf(achan,"disabled")) weed_leaf_delete(achan,"disabled");
    mainw->afbuffer_clients++;
    if (mainw->afbuffer_clients==1) {
      pthread_mutex_lock(&mainw->abuf_frame_mutex);
      init_audio_frame_buffer(prefs->audio_player);
      pthread_mutex_unlock(&mainw->abuf_frame_mutex);
    }

  }

  // allow clip switching
  cfile->is_loaded=TRUE;

  // if not playing, start playing
  if (mainw->playing_file==-1) {
    uint64_t new_rte;

    if (!is_bg||old_file==-1||old_file==new_file) {
      switch_to_file((mainw->current_file=old_file),new_file);
      set_main_title(cfile->file_name,0);
      mainw->play_start=1;
      mainw->play_end=INT_MAX;
      if (is_bg) {
        mainw->blend_file=mainw->current_file;
        if (old_file!=-1) mainw->current_file=old_file;
      }
    } else {
      mainw->blend_file=mainw->current_file;
      mainw->current_file=old_file;
      mainw->play_start=cfile->start;
      mainw->play_end=cfile->end;
      mainw->playing_sel=FALSE;
    }

    new_rte=GU641<<key;
    if (!(mainw->rte&new_rte)) mainw->rte|=new_rte;

    mainw->last_grabbable_effect=key;
    if (rte_window!=NULL) rtew_set_keych(key,TRUE);
    if (mainw->ce_thumbs) {
      ce_thumbs_set_keych(key,TRUE);

      // if effect was auto (from ACTIVATE data connection), leave all param boxes
      // otherwise, remove any which are not "pinned"
      if (!mainw->fx_is_auto) ce_thumbs_add_param_box(key,!mainw->fx_is_auto);
    }

    play_file();

    // need to set this after playback ends; this stops the key from being activated (again) in effects.c
    mainw->gen_started_play=TRUE;

    if (mainw->play_window!=NULL) {
      lives_widget_queue_draw(mainw->play_window);
    }
  } else {
    // already playing

    if (old_file!=-1&&mainw->files[old_file]!=NULL) {
      if (mainw->files[old_file]->clip_type==CLIP_TYPE_DISK||mainw->files[old_file]->clip_type==CLIP_TYPE_FILE)
        mainw->pre_src_file=old_file;
      mainw->current_file=old_file;
    }

    if (!is_bg||old_file==-1||old_file==new_file) {
      if (mainw->current_file==-1) mainw->current_file=new_file;

      if (new_file!=old_file) {
        do_quick_switch(new_file);

        // switch audio clip
        if (is_realtime_aplayer(prefs->audio_player)&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)
            &&!mainw->is_rendering&&(mainw->preview||!(mainw->agen_key!=0||prefs->audio_src==AUDIO_SRC_EXT))) {
          switch_audio_clip(new_file,TRUE);
        }

        if (mainw->files[mainw->new_blend_file]!=NULL) mainw->blend_file=mainw->new_blend_file;
        if (!is_bg&&blend_file!=-1&&mainw->files[blend_file]!=NULL) mainw->blend_file=blend_file;
        mainw->new_blend_file=-1;
      } else {
        lives_widget_show(mainw->playframe);
        resize(1);
      }
      //if (old_file==-1) mainw->whentostop=STOP_ON_VID_END;
    } else {
      if (mainw->current_file==-1) {
        mainw->current_file=new_file;
        if (is_realtime_aplayer(prefs->audio_player)&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_CLIPS)
            &&!mainw->is_rendering&&(mainw->preview||!(mainw->agen_key!=0||prefs->audio_src==AUDIO_SRC_EXT))) {
          switch_audio_clip(new_file,TRUE);
        }
      } else mainw->blend_file=new_file;
      if (mainw->ce_thumbs&&(mainw->active_sa_clips==SCREEN_AREA_BACKGROUND||mainw->active_sa_clips==SCREEN_AREA_FOREGROUND))
        ce_thumbs_highlight_current_clip();
    }

    if (mainw->cancelled==CANCEL_GENERATOR_END) mainw->cancelled=CANCEL_NONE;

  }

  return TRUE;
}



void weed_bg_generator_end(weed_plant_t *inst) {
  // when we stop with a bg generator we want it to be restarted next time
  // i.e we will need a new clip for it
  int bg_gen_key=bg_generator_key;
  weed_generator_end(inst);
  bg_gen_to_start=bg_gen_key;
}



boolean weed_playback_gen_start(void) {
  // init generators on pb. We have to do this after audio startup
  weed_plant_t *inst=NULL,*filter;
  weed_plant_t *next_inst=NULL;

  char *filter_name;

  int error=WEED_NO_ERROR;
  int weed_error;
  int bgs=bg_gen_to_start;
  boolean was_started=FALSE;

  if (mainw->is_rendering) return TRUE;

  if (fg_gen_to_start==bg_gen_to_start) bg_gen_to_start=-1;

  if (cfile->frames==0&&fg_gen_to_start==-1&&bg_gen_to_start!=-1) {
    fg_gen_to_start=bg_gen_to_start;
    bg_gen_to_start=-1;
  }

  mainw->osc_block=TRUE;

  if (fg_gen_to_start!=-1) {
    // check is still gen

    if (enabled_in_channels(weed_filters[key_to_fx[fg_gen_to_start][key_modes[fg_gen_to_start]]],FALSE)==0) {
      inst=key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]];
      if (inst!=NULL) {

geninit1:

        filter=weed_instance_get_filter(inst,FALSE);
        if (weed_plant_has_leaf(filter,"init_func")) {
          weed_init_f *init_func_ptr_ptr;
          weed_init_f init_func;
          weed_leaf_get(filter,"init_func",0,(void *)&init_func_ptr_ptr);
          init_func=init_func_ptr_ptr[0];
          update_host_info(inst);
          if (init_func!=NULL) {
            char *cwd=cd_to_plugin_dir(filter);
            set_param_gui_readwrite(inst);
            error=(*init_func)(inst);
            set_param_gui_readonly(inst);
            lives_chdir(cwd,FALSE);
            lives_free(cwd);
          }
        }


        if (error!=WEED_NO_ERROR) {
          inst=key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]];
          key_to_instance[fg_gen_to_start][key_modes[fg_gen_to_start]]=NULL;
          if (inst!=NULL) {
            filter=weed_instance_get_filter(inst,TRUE);
            filter_name=weed_get_string_value(filter,"name",&weed_error);
            d_print(_("Failed to start generator %s\n"),filter_name);
            lives_free(filter_name);

deinit4:

            if (weed_plant_has_leaf(inst,"host_next_instance")) next_inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
            else next_inst=NULL;

            weed_call_deinit_func(inst);
            weed_instance_unref(inst);

            if (next_inst!=NULL) {
              // handle compound fx
              inst=next_inst;
              if (weed_get_boolean_value(inst,"host_inited",&error)==WEED_TRUE) goto deinit4;
            }

          }

          fg_gen_to_start=-1;
          cfile->ext_src=NULL;
          mainw->osc_block=FALSE;
          return FALSE;
        }

        weed_set_boolean_value(inst,"host_inited",WEED_TRUE);

        if (weed_plant_has_leaf(inst,"host_next_instance")) {
          inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
          goto geninit1;
        }


        // TODO
        if (weed_plant_has_leaf(inst,"target_fps")) {
          int current_file=mainw->current_file;
          mainw->current_file=fg_generator_clip;
          cfile->fps=weed_get_double_value(inst,"target_fps",&error);
          set_main_title(cfile->file_name,0);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->fps);
          mainw->current_file=current_file;
        }

        mainw->clip_switched=TRUE;

        cfile->ext_src=inst;
      }

      fg_gen_to_start=-1;
    }
  }

  if (bg_gen_to_start!=-1) {

    if (mainw->blend_file==-1) return TRUE; // for example if transition was swapped for filter in mapper

    // check is still gen
    if (enabled_in_channels(weed_filters[key_to_fx[bg_gen_to_start][key_modes[bg_gen_to_start]]],FALSE)==0) {
      if (key_to_instance[bg_gen_to_start][key_modes[bg_gen_to_start]]==NULL) {
        // restart bg generator
        if (!weed_init_effect(bg_gen_to_start)) return TRUE;
        was_started=TRUE;
      }
      inst=key_to_instance[bgs][key_modes[bgs]];

      if (inst==NULL) {
        // 2nd playback
        int playing_file=mainw->playing_file;
        mainw->playing_file=-100; //kludge to stop playing a second time
        if (!weed_init_effect(bg_gen_to_start)) {
          error++;
        }
        mainw->playing_file=playing_file;
        inst=key_to_instance[bg_gen_to_start][key_modes[bg_gen_to_start]];
      } else {
        if (!was_started) {

genstart2:

          weed_call_init_func(inst);

          // handle compound fx
          if (weed_plant_has_leaf(inst,"host_next_instance")) {
            inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
            goto genstart2;
          }
          inst=key_to_instance[bgs][key_modes[bgs]];
        }
      }

      if (error!=WEED_NO_ERROR) {
        if (inst!=NULL) {
          filter=weed_instance_get_filter(inst,TRUE);
          filter_name=weed_get_string_value(filter,"name",&weed_error);
          d_print(_("Failed to start generator %s, error %d\n"),filter_name,error);
          lives_free(filter_name);

deinit5:

          if (weed_plant_has_leaf(inst,"host_next_instance")) next_inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
          else next_inst=NULL;

          weed_call_deinit_func(inst);
          weed_instance_unref(inst);

          if (next_inst!=NULL) {
            // handle compound fx
            inst=next_inst;
            goto deinit5;
          }

        }
        key_to_instance[bg_gen_to_start][key_modes[bg_gen_to_start]]=NULL;
        bg_gen_to_start=-1;
        mainw->blend_file=-1;
        if (mainw->rte&(GU641<<ABS(bg_gen_to_start))) mainw->rte^=(GU641<<ABS(bg_gen_to_start));
        mainw->osc_block=FALSE;
        return FALSE;
      }
      mainw->files[mainw->blend_file]->ext_src=inst;
    }
    bg_gen_to_start=-1;
  }

setgui1:

  if (inst!=NULL) set_param_gui_readonly(inst);

  // handle compound fx
  if (inst!=NULL&&weed_plant_has_leaf(inst,"host_next_instance")) {
    inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
    goto setgui1;
  }

  mainw->osc_block=FALSE;

  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// weed parameter functions


boolean is_hidden_param(weed_plant_t *plant, int i) {
  // find out if in_param i is visible or not for plant. Plant can be an instance or a filter
  boolean visible=TRUE;
  weed_plant_t **wtmpls;
  int error,flags=0;
  weed_plant_t *filter,*gui=NULL;
  int num_params=0;
  weed_plant_t *wtmpl;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) filter=weed_instance_get_filter(plant,TRUE);
  else filter=plant;

  if (weed_plant_has_leaf(filter,"in_parameter_templates"))
    num_params=weed_leaf_num_elements(filter,"in_parameter_templates");

  if (num_params==0) return TRUE;

  wtmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  wtmpl=wtmpls[i];

  if (weed_plant_has_leaf(wtmpl,"flags")) flags=weed_get_int_value(wtmpl,"flags",&error);
  if (weed_plant_has_leaf(wtmpl,"gui")) gui=weed_get_plantptr_value(wtmpl,"gui",&error);

  if (gui!=NULL&&weed_plant_has_leaf(gui,"hidden")&&
      weed_get_boolean_value(gui,"hidden",&error)==WEED_TRUE) {
    lives_free(wtmpls);
    return TRUE;
  }
  if (!(flags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE)
      &&(gui==NULL||(!weed_plant_has_leaf(gui,"hidden")
                     ||weed_get_boolean_value(gui,"hidden",&error)==WEED_FALSE))) {
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
          if (param_hint==param_hint2
              &&((flags2&WEED_PARAMETER_VARIABLE_ELEMENTS)
                 ||(flags&WEED_PARAMETER_ELEMENT_PER_CHANNEL&&flags2&WEED_PARAMETER_ELEMENT_PER_CHANNEL)
                 ||weed_leaf_num_elements(wtmpl,"default")==weed_leaf_num_elements(wtmpl2,"default"))) {
            if (!(flags2&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE)) {
              visible=TRUE;
            }
          }
        }
      }
    }
  }

  // internally connected parameters for compound fx
  if (weed_plant_has_leaf(wtmpl,"host_internal_connection")) visible=FALSE;

  lives_free(wtmpls);
  return !visible;
}


int get_transition_param(weed_plant_t *filter, boolean skip_internal) {
  int error,num_params,i,count=0;
  weed_plant_t **in_ptmpls;

  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return -1; // has no in_parameters

  num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  for (i=0; i<num_params; i++) {
    if (skip_internal&&weed_plant_has_leaf(in_ptmpls[i],"host_internal_connection")) continue;
    if (weed_plant_has_leaf(in_ptmpls[i],"transition")&&
        weed_get_boolean_value(in_ptmpls[i],"transition",&error)==WEED_TRUE) {
      lives_free(in_ptmpls);
      return count;
    }
    count++;
  }
  lives_free(in_ptmpls);
  return -1;
}


int get_master_vol_param(weed_plant_t *filter, boolean skip_internal) {
  int error,num_params,i,count=0;
  weed_plant_t **in_ptmpls;

  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return -1; // has no in_parameters

  num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  in_ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  for (i=0; i<num_params; i++) {
    if (skip_internal&&weed_plant_has_leaf(in_ptmpls[i],"host_internal_connection")) continue;
    if (weed_plant_has_leaf(in_ptmpls[i],"is_volume_master")&&
        weed_get_boolean_value(in_ptmpls[i],"is_volume_master",&error)==WEED_TRUE) {
      lives_free(in_ptmpls);
      return count;
    }
    count++;
  }
  lives_free(in_ptmpls);
  return -1;
}




boolean is_perchannel_multiw(weed_plant_t *param) {
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



boolean has_perchannel_multiw(weed_plant_t *filter) {
  int error,nptmpl,i;
  weed_plant_t **ptmpls;

  if (!weed_plant_has_leaf(filter,"in_parameter_templates")||
      (nptmpl=weed_leaf_num_elements(filter,"in_parameter_templates"))==0) return FALSE;

  ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  for (i=0; i<nptmpl; i++) {
    if (is_perchannel_multiw(ptmpls[i])) {
      lives_free(ptmpls);
      return TRUE;
    }
  }

  lives_free(ptmpls);
  return FALSE;
}





weed_plant_t *weed_inst_in_param(weed_plant_t *inst, int param_num, boolean skip_hidden, boolean skip_internal) {
  weed_plant_t **in_params;
  weed_plant_t *param;
  int error,num_params;

  do {

    if (!weed_plant_has_leaf(inst,"in_parameters")) continue; // has no in_parameters

    num_params=weed_leaf_num_elements(inst,"in_parameters");

    if (!skip_hidden&&!skip_internal) {
      if (num_params>param_num) {
        in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
        param=in_params[param_num];
        lives_free(in_params);
        return param;
      }
      param_num-=num_params;
    }

    else {
      int count=0;
      register int i;

      in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

      for (i=0; i<num_params; i++) {
        param=in_params[i];
        if ((!skip_hidden||!is_hidden_param(inst,i))&&(!skip_internal||!weed_plant_has_leaf(param,"host_internal_connection"))) {
          if (count==param_num) {
            lives_free(in_params);
            return param;
          }
          count++;
        }
      }
      param_num-=count;
      lives_free(in_params);
    }

  } while (weed_plant_has_leaf(inst,"host_next_instance")&&(inst=weed_get_plantptr_value(inst,"host_next_instance",&error))!=NULL);

  return NULL;
}





weed_plant_t *weed_inst_out_param(weed_plant_t *inst, int param_num) {
  weed_plant_t **out_params;
  weed_plant_t *param;
  int error,num_params;

  do {
    if (!weed_plant_has_leaf(inst,"out_parameters")) continue; // has no out_parameters

    num_params=weed_leaf_num_elements(inst,"out_parameters");

    if (num_params>param_num) {
      out_params=weed_get_plantptr_array(inst,"out_parameters",&error);
      param=out_params[param_num];
      lives_free(out_params);
      return param;
    }
    param_num-=num_params;
  } while (weed_plant_has_leaf(inst,"host_next_instance")&&(inst=weed_get_plantptr_value(inst,"host_next_instance",&error))!=NULL);

  return NULL;
}




weed_plant_t *weed_filter_in_paramtmpl(weed_plant_t *filter, int param_num, boolean skip_internal) {
  weed_plant_t **in_params;
  weed_plant_t *ptmpl;
  int error,num_params;

  int count=0;
  register int i;

  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return NULL; // has no in_parameters

  num_params=weed_leaf_num_elements(filter,"in_parameter_templates");

  if (num_params<=param_num) return NULL; // invalid parameter number

  in_params=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  if (!skip_internal) {
    ptmpl=in_params[param_num];
    lives_free(in_params);
    return ptmpl;
  }

  for (i=0; i<num_params; i++) {
    ptmpl=in_params[i];
    if (!weed_plant_has_leaf(ptmpl,"host_internal_connection")) {
      if (count==param_num) {
        return ptmpl;
      }
      count++;
    }
  }

  return NULL;
}



weed_plant_t *weed_filter_out_paramtmpl(weed_plant_t *filter, int param_num) {
  weed_plant_t **out_params;
  weed_plant_t *ptmpl;
  int error,num_params;

  if (!weed_plant_has_leaf(filter,"out_parameter_templates")) return NULL; // has no out_parameters

  num_params=weed_leaf_num_elements(filter,"out_parameter_templates");

  if (num_params<=param_num) return NULL; // invalid parameter number

  out_params=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

  ptmpl=out_params[param_num];
  lives_free(out_params);
  return ptmpl;
}




int get_nth_simple_param(weed_plant_t *plant, int pnum) {
  // return the number of the nth "simple" parameter
  // we define "simple" as - must be single valued int or float, must not be hidden

  // -1 is returned if no such parameter is found

  int i,error,hint,flags,nparams;
  weed_plant_t **in_ptmpls;
  weed_plant_t *tparamtmpl;
  weed_plant_t *gui;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) plant=weed_instance_get_filter(plant,TRUE);

  if (!weed_plant_has_leaf(plant,"in_parameter_templates")) return -1;

  in_ptmpls=weed_get_plantptr_array(plant,"in_parameter_templates",&error);
  nparams=weed_leaf_num_elements(plant,"in_parameter_templates");

  for (i=0; i<nparams; i++) {
    gui=NULL;
    tparamtmpl=in_ptmpls[i];
    if (weed_plant_has_leaf(tparamtmpl,"gui")) gui=weed_get_plantptr_value(tparamtmpl,"gui",&error);


    hint=weed_get_int_value(tparamtmpl,"hint",&error);

    if (gui!=NULL&&hint==WEED_HINT_INTEGER&&weed_plant_has_leaf(gui,"choices")) continue;

    flags=weed_get_int_value(tparamtmpl,"flags",&error);

    if ((hint==WEED_HINT_INTEGER||hint==WEED_HINT_FLOAT)&&flags==0&&weed_leaf_num_elements(tparamtmpl,"default")==1&&
        !is_hidden_param(plant,i)) {
      if (pnum==0) {
        lives_free(in_ptmpls);
        return i;
      }
      pnum--;
    }
  }
  lives_free(in_ptmpls);
  return -1;
}



int count_simple_params(weed_plant_t *plant) {

  int i,error,hint,flags,nparams,count=0;
  weed_plant_t **in_ptmpls;
  weed_plant_t *tparamtmpl;

  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) plant=weed_instance_get_filter(plant,TRUE);

  if (!weed_plant_has_leaf(plant,"in_parameter_templates")) return count;

  in_ptmpls=weed_get_plantptr_array(plant,"in_parameter_templates",&error);
  nparams=weed_leaf_num_elements(plant,"in_parameter_templates");

  for (i=0; i<nparams; i++) {
    tparamtmpl=in_ptmpls[i];
    hint=weed_get_int_value(tparamtmpl,"hint",&error);
    flags=weed_get_int_value(tparamtmpl,"flags",&error);
    if ((hint==WEED_HINT_INTEGER||hint==WEED_HINT_FLOAT)&&flags==0&&weed_leaf_num_elements(tparamtmpl,"default")==1&&
        !is_hidden_param(plant,i)) {
      count++;
    }
  }
  lives_free(in_ptmpls);
  return count;
}



char *get_weed_display_string(weed_plant_t *inst, int pnum) {
  // TODO - for setting defaults, we will need to create params
  char *disp_string;
  weed_plant_t *param=weed_inst_in_param(inst,pnum,FALSE,FALSE);
  weed_plant_t *ptmpl,*gui,*filter;
  int error;
  weed_display_f *display_func_ptr;
  weed_display_f display_func;
  char *cwd;

  if (param==NULL) return NULL;

  ptmpl=weed_get_plantptr_value(param,"template",&error);
  if (!weed_plant_has_leaf(ptmpl,"gui")) return NULL;
  gui=weed_get_plantptr_value(ptmpl,"gui",&error);
  if (!weed_plant_has_leaf(gui,"display_func")) return NULL;

  display_func_ptr=(weed_display_f *)weed_get_voidptr_value(gui,"display_func",&error);
  display_func=(weed_display_f)*display_func_ptr;

  weed_leaf_set_flags(gui,"display_value",(weed_leaf_get_flags(gui,"display_value")|
                      WEED_LEAF_READONLY_PLUGIN)^WEED_LEAF_READONLY_PLUGIN);
  filter=weed_instance_get_filter(inst,FALSE);
  cwd=cd_to_plugin_dir(filter);
  (*display_func)(param);
  lives_chdir(cwd,FALSE);
  lives_free(cwd);
  weed_leaf_set_flags(gui,"display_value",(weed_leaf_get_flags(gui,"display_value")|WEED_LEAF_READONLY_PLUGIN));

  if (!weed_plant_has_leaf(gui,"display_value")) return NULL;
  if (weed_leaf_seed_type(gui,"display_value")!=WEED_SEED_STRING) return NULL;
  disp_string=weed_get_string_value(gui,"display_value",&error);

  return disp_string;
}



int set_copy_to(weed_plant_t *inst, int pnum, boolean update) {
  // if we update a plugin in_parameter, evaluate any "copy_value_to"
  int error;
  boolean copy_ok=FALSE;
  int copyto;

  weed_plant_t **in_params;

  weed_plant_t *gui=NULL;
  weed_plant_t *in_param=weed_inst_in_param(inst,pnum,FALSE,FALSE); // use this here in case of compound fx
  weed_plant_t *paramtmpl=weed_get_plantptr_value(in_param,"template",&error);

  if (weed_plant_has_leaf(paramtmpl,"gui")) gui=weed_get_plantptr_value(paramtmpl,"gui",&error);

  if (gui==NULL) return -1;

  if (weed_plant_has_leaf(gui,"copy_value_to")) {
    weed_plant_t *paramtmpl2;
    int param_hint2,flags2=0;
    int param_hint=weed_get_int_value(paramtmpl,"hint",&error);
    int nparams=weed_leaf_num_elements(inst,"in_parameters");

    copyto=weed_get_int_value(gui,"copy_value_to",&error);
    if (copyto==pnum||copyto<0||copyto>=nparams) return -1;

    in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

    if (weed_plant_has_leaf(in_params[copyto],"host_internal_connection")) return -1;

    paramtmpl2=weed_get_plantptr_value(in_params[copyto],"template",&error);
    if (weed_plant_has_leaf(paramtmpl2,"flags")) flags2=weed_get_int_value(paramtmpl2,"flags",&error);
    param_hint2=weed_get_int_value(paramtmpl2,"hint",&error);
    if (param_hint==param_hint2&&((flags2&WEED_PARAMETER_VARIABLE_ELEMENTS)||
                                  weed_leaf_num_elements(paramtmpl,"default")==
                                  weed_leaf_num_elements(paramtmpl2,"default"))) {
      copy_ok=TRUE;
    }
    lives_free(in_params);
  }

  if (!copy_ok) return -1;

  if (update) {
    int key=-1;
    if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);
    filter_mutex_lock(key);
    weed_leaf_copy(in_params[copyto],"value",in_param,"value");
    filter_mutex_unlock(key);
  }
  return copyto;
}


void rec_param_change(weed_plant_t *inst, int pnum) {
  int error;
  weed_timecode_t tc;
  weed_plant_t *in_param;
  int key;

  // do not record changes for generators - those get recorded to scrap_file or ascrap_file
  if (enabled_in_channels(inst,FALSE)==0) return;

  tc=get_event_timecode(get_last_event(mainw->event_list));
  key=weed_get_int_value(inst,"host_key",&error);

  in_param=weed_inst_in_param(inst,pnum,FALSE,FALSE);

  pthread_mutex_lock(&mainw->event_list_mutex);
  mainw->event_list=append_param_change_event(mainw->event_list,tc,pnum,in_param,init_events[key],pchains[key]);
  pthread_mutex_unlock(&mainw->event_list_mutex);
}



#define KEYSCALE 255.


void weed_set_blend_factor(int hotkey) {
  // mainw->osc_block should be set to TRUE before calling this function !
  weed_plant_t *inst,*in_param,*in_param2=NULL,*paramtmpl;

  LiVESList *list=NULL;

  weed_plant_t **in_params;

  double vald,mind,maxd;

  weed_timecode_t tc=0;

  int error;
  int vali,mini,maxi;

  int param_hint;
  int copyto=-1;

  int pnum;

  int inc_count;
  int key=-1;

  if (hotkey<0) return;
  inst=key_to_instance[hotkey][key_modes[hotkey]];

  if (inst==NULL) return;

  pnum=get_nth_simple_param(inst,0);

  if (pnum==-1) return;

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  in_param=in_params[pnum];
  lives_free(in_params);

  paramtmpl=weed_get_plantptr_value(in_param,"template",&error);
  param_hint=weed_get_int_value(paramtmpl,"hint",&error);

  inc_count=enabled_in_channels(inst,FALSE);

  // record old value
  copyto=set_copy_to(inst,pnum,FALSE);

  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)&&inc_count>0) {
    pthread_mutex_lock(&mainw->event_list_mutex);
    tc=get_event_timecode(get_last_event(mainw->event_list));
    mainw->event_list=append_param_change_event(mainw->event_list,tc,pnum,in_param,init_events[hotkey],pchains[hotkey]);
    if (copyto>-1) {
      mainw->event_list=append_param_change_event(mainw->event_list,tc,copyto,in_param2,init_events[hotkey],
                        pchains[hotkey]);
    }
    pthread_mutex_unlock(&mainw->event_list_mutex);
  }

  if (weed_plant_has_leaf(paramtmpl,"wrap")&&weed_get_boolean_value(paramtmpl,"wrap",&error)==WEED_TRUE) {
    if (mainw->blend_factor>=256.) mainw->blend_factor-=256.;
    else if (mainw->blend_factor<=-1.) mainw->blend_factor+=256.;
  } else {
    if (mainw->blend_factor<0.) mainw->blend_factor=0.;
    else if (mainw->blend_factor>255.) mainw->blend_factor=255.;
  }

  filter_mutex_lock(hotkey);

  switch (param_hint) {
  case WEED_HINT_INTEGER:
    vali=weed_get_int_value(in_param,"value",&error);
    mini=weed_get_int_value(paramtmpl,"min",&error);
    maxi=weed_get_int_value(paramtmpl,"max",&error);

    weed_set_int_value(in_param,"value",(int)((double)mini+(mainw->blend_factor/KEYSCALE*(double)(maxi-mini))+.5));

    vali=weed_get_int_value(in_param,"value",&error);

    list=lives_list_append(list,lives_strdup_printf("%d",vali));
    list=lives_list_append(list,lives_strdup_printf("%d",mini));
    list=lives_list_append(list,lives_strdup_printf("%d",maxi));
    update_pwindow(hotkey,pnum,list);
    if (mainw->ce_thumbs) ce_thumbs_update_params(hotkey,pnum,list);
    lives_list_free_strings(list);
    lives_list_free(list);

    break;
  case WEED_HINT_FLOAT:
    vald=weed_get_double_value(in_param,"value",&error);
    mind=weed_get_double_value(paramtmpl,"min",&error);
    maxd=weed_get_double_value(paramtmpl,"max",&error);

    weed_set_double_value(in_param,"value",mind+(mainw->blend_factor/KEYSCALE*(maxd-mind)));
    vald=weed_get_double_value(in_param,"value",&error);

    list=lives_list_append(list,lives_strdup_printf("%.4f",vald));
    list=lives_list_append(list,lives_strdup_printf("%.4f",mind));
    list=lives_list_append(list,lives_strdup_printf("%.4f",maxd));
    update_pwindow(hotkey,pnum,list);
    if (mainw->ce_thumbs) ce_thumbs_update_params(hotkey,pnum,list);
    lives_list_free_strings(list);
    lives_list_free(list);

    break;
  case WEED_HINT_SWITCH:
    vali=!!(int)mainw->blend_factor;
    weed_set_boolean_value(in_param,"value",vali);
    vali=weed_get_boolean_value(in_param,"value",&error);
    mainw->blend_factor=(double)vali;

    list=lives_list_append(list,lives_strdup_printf("%d",vali));
    update_pwindow(hotkey,pnum,list);
    if (mainw->ce_thumbs) ce_thumbs_update_params(hotkey,pnum,list);
    lives_list_free_strings(list);
    lives_list_free(list);

    break;
  }

  filter_mutex_unlock(key);

  set_copy_to(inst,pnum,TRUE);

  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)&&inc_count>0) {
    pthread_mutex_lock(&mainw->event_list_mutex);
    tc=get_event_timecode(get_last_event(mainw->event_list));
    mainw->event_list=append_param_change_event(mainw->event_list,tc,pnum,in_param,init_events[hotkey],pchains[hotkey]);
    if (copyto>-1) {

      mainw->event_list=append_param_change_event(mainw->event_list,tc,copyto,in_param2,init_events[hotkey],
                        pchains[hotkey]);
    }
    pthread_mutex_unlock(&mainw->event_list_mutex);
  }


}



int weed_get_blend_factor(int hotkey) {
  // mainw->osc_block should be set to TRUE before calling this function !

  weed_plant_t *inst,**in_params,*in_param,*paramtmpl;
  int error;
  int vali,mini,maxi;
  double vald,mind,maxd;
  int weed_hint;
  int i;

  if (hotkey<0) return 0;
  inst=key_to_instance[hotkey][key_modes[hotkey]];

  if (inst==NULL) return 0;

  i=get_nth_simple_param(inst,0);

  if (i==-1) return 0;

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  in_param=in_params[i];

  paramtmpl=weed_get_plantptr_value(in_param,"template",&error);
  weed_hint=weed_get_int_value(paramtmpl,"hint",&error);

  switch (weed_hint) {
  case WEED_HINT_INTEGER:
    vali=weed_get_int_value(in_param,"value",&error);
    mini=weed_get_int_value(paramtmpl,"min",&error);
    maxi=weed_get_int_value(paramtmpl,"max",&error);
    lives_free(in_params);
    return (double)(vali-mini)/(double)(maxi-mini)*KEYSCALE;
  case WEED_HINT_FLOAT:
    vald=weed_get_double_value(in_param,"value",&error);
    mind=weed_get_double_value(paramtmpl,"min",&error);
    maxd=weed_get_double_value(paramtmpl,"max",&error);
    lives_free(in_params);
    return (vald-mind)/(maxd-mind)*KEYSCALE;
  case WEED_HINT_SWITCH:
    vali=weed_get_boolean_value(in_param,"value",&error);
    lives_free(in_params);
    return vali;
  }

  lives_free(in_params);

  return 0;
}






weed_plant_t *get_new_inst_for_keymode(int key, int mode)  {
  // key is 0 based

  weed_plant_t *inst;

  int error;

  register int i;

  for (i=FX_KEYS_MAX_VIRTUAL; i<FX_KEYS_MAX; i++) {
    if ((inst=key_to_instance[i][key_modes[i]])==NULL) continue;
    if (weed_plant_has_leaf(inst,"host_mode")) {
      if (weed_get_int_value(inst,"host_key",&error)==key && weed_get_int_value(inst,"host_mode",&error)==mode) {
        return inst;
      }
    }
  }

  return NULL;
}











////////////////////////////////////////////////////////////////////////


static LIVES_INLINE char *weed_instance_get_type(weed_plant_t *inst, boolean getsub) {
  // return value should be free'd after use
  weed_plant_t *filter=weed_instance_get_filter(inst,TRUE);
  return weed_filter_get_type(filter,getsub,TRUE);
}




char *rte_keymode_get_type(int key, int mode) {
  // return value should be free'd after use
  char *type=lives_strdup("");
  weed_plant_t *filter,*inst;
  int idx;

  key--;
  if (!rte_keymode_valid(key+1,mode,TRUE)) return type;

  if ((idx=key_to_fx[key][mode])==-1) return type;
  if ((filter=weed_filters[idx])==NULL) return type;

  lives_free(type);

  mainw->osc_block=TRUE;

  if ((inst=key_to_instance[key][mode])!=NULL) {
    // return details for instance
    type=weed_instance_get_type(inst,TRUE);
  } else type=weed_filter_get_type(filter,TRUE,TRUE);

  mainw->osc_block=FALSE;
  return type;
}



lives_fx_cat_t rte_keymode_get_category(int key, int mode) {
  weed_plant_t *filter;
  int idx;
  lives_fx_cat_t cat;

  key--;
  if (!rte_keymode_valid(key+1,mode,TRUE)) return LIVES_FX_CAT_NONE;

  if ((idx=key_to_fx[key][mode])==-1) return LIVES_FX_CAT_NONE;
  if ((filter=weed_filters[idx])==NULL) return LIVES_FX_CAT_NONE;

  else cat=weed_filter_categorise(filter,
                                    enabled_in_channels(filter,FALSE),
                                    enabled_out_channels(filter,FALSE));

  return cat;
}



///////////////////////////////////////////////////////////////////////////////

int get_next_free_key(void) {
  // 0 based
  int i,free_key;
  free_key=next_free_key;
  for (i=free_key+1; i<FX_KEYS_MAX; i++) {
    if (key_to_fx[i][0]==-1) {
      next_free_key=i;
      break;
    }
  }
  if (i==FX_KEYS_MAX) next_free_key=-1;
  return free_key;
}


boolean weed_delete_effectkey(int key, int mode) {
  // delete the effect binding for key/mode and move higher numbered slots down
  // also moves the active mode if applicable
  // returns FALSE if there was no effect bound to key/mode

  char *tmp;

  boolean was_started=FALSE;

  int oldkeymode=key_modes[--key];
  int orig_mode=mode;
  int modekey=key;

  if (key_to_fx[key][mode]==-1) return FALSE;

  if (key<FX_KEYS_MAX_VIRTUAL) free_key_defaults(key,mode);

  for (; mode<(key<FX_KEYS_MAX_VIRTUAL?prefs->max_modes_per_key:1); mode++) {

    mainw->osc_block=TRUE;
    if (key>=FX_KEYS_MAX_VIRTUAL||mode==prefs->max_modes_per_key-1||key_to_fx[key][mode+1]==-1) {
      if (key_to_instance[key][mode]!=NULL) {
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
    } else if (key<FX_KEYS_MAX_VIRTUAL) {
      rte_switch_keymode(key+1,mode,(tmp=make_weed_hashname
                                         (key_to_fx[key][mode+1],TRUE,FALSE)));
      lives_free(tmp);

      key_defaults[key][mode]=key_defaults[key][mode+1];
      key_defaults[key][mode+1]=NULL;
    }
  }

  if (key>=FX_KEYS_MAX_VIRTUAL&&key<next_free_key) next_free_key=key;

  mainw->osc_block=FALSE;
  if (key_modes[key]>orig_mode) key_modes[key]--;

  return TRUE;
}








/////////////////////////////////////////////////////////////////////////////

boolean rte_key_valid(int key, boolean is_userkey) {
  key--;

  if (key<0||(is_userkey&&key>=FX_KEYS_MAX_VIRTUAL)||key>=FX_KEYS_MAX) return FALSE;
  if (key_to_fx[key][key_modes[key]]==-1) return FALSE;
  return TRUE;
}

boolean rte_keymode_valid(int key, int mode, boolean is_userkey) {
  if (key<1||(is_userkey&&key>FX_KEYS_MAX_VIRTUAL)||key>FX_KEYS_MAX||mode<0||
      mode>=(key<FX_KEYS_MAX_VIRTUAL?prefs->max_modes_per_key:1)) return FALSE;
  if (key_to_fx[--key][mode]==-1) return FALSE;
  return TRUE;
}

int rte_keymode_get_filter_idx(int key, int mode) {
  if (key<1||key>FX_KEYS_MAX||mode<0||
      mode>=(key<FX_KEYS_MAX_VIRTUAL?prefs->max_modes_per_key:1)) return -1;
  return (key_to_fx[--key][mode]);
}

int rte_key_getmode(int key) {
  if (key<1||key>FX_KEYS_MAX) return -1;
  return key_modes[--key];
}

int rte_key_getmaxmode(int key) {
  register int i;

  if (key<1||key>FX_KEYS_MAX) return -1;

  key--;

  for (i=0; i<(key<FX_KEYS_MAX_VIRTUAL?prefs->max_modes_per_key:1); i++) {
    if (key_to_fx[key][i]==-1) return i-1;
  }
  return i-1;
}

weed_plant_t *rte_keymode_get_instance(int key, int mode) {
  weed_plant_t *inst;

  key--;
  if (!rte_keymode_valid(key+1,mode,FALSE)) return NULL;
  mainw->osc_block=TRUE;
  if ((inst=key_to_instance[key][mode])==NULL) {
    mainw->osc_block=FALSE;
    return NULL;
  }
  mainw->osc_block=FALSE;
  return inst;
}


weed_plant_t *rte_keymode_get_filter(int key, int mode) {
  key--;
  if (!rte_keymode_valid(key+1,mode,FALSE)) return NULL;
  return weed_filters[key_to_fx[key][mode]];
}


char *weed_filter_idx_get_name(int idx) {
  // return value should be lives_free'd after use
  weed_plant_t *filter;
  int error;
  char *filter_name,*retval;

  if (idx==-1) return lives_strdup("");
  if ((filter=weed_filters[idx])==NULL) return lives_strdup("");
  filter_name=weed_get_string_value(filter,"name",&error);
  retval=lives_strdup(filter_name); // copy so we can use lives_free() instead of lives_free()
  lives_free(filter_name);
  return retval;
}


char *weed_instance_get_filter_name(weed_plant_t *inst, boolean get_compound_parent) {
  // return value should be lives_free'd after use
  weed_plant_t *filter;
  int error;
  char *filter_name,*retval;

  if (inst==NULL) return lives_strdup("");
  filter=weed_instance_get_filter(inst,get_compound_parent);
  filter_name=weed_get_string_value(filter,"name",&error);
  retval=lives_strdup(filter_name); // copy so we can use lives_free() instead of lives_free()
  lives_free(filter_name);
  return retval;
}


char *rte_keymode_get_filter_name(int key, int mode) {
  // return value should be lives_free'd after use
  key--;
  if (!rte_keymode_valid(key+1,mode,TRUE)) return lives_strdup("");
  return (weed_filter_idx_get_name(key_to_fx[key][mode]));
}


char *rte_keymode_get_plugin_name(int key, int mode) {
  // return value should be lives_free'd after use
  weed_plant_t *filter,*plugin_info;
  char *name;
  int error;
  char *retval;

  key--;
  if (!rte_keymode_valid(key+1,mode,TRUE)) return lives_strdup("");

  filter=weed_filters[key_to_fx[key][mode]];
  plugin_info=weed_get_plantptr_value(filter,"plugin_info",&error);
  name=weed_get_string_value(plugin_info,"name",&error);
  // do this so we can lives_free() instead of lives_free();
  retval=lives_strdup(name);
  lives_free(name);
  return retval;
}




G_GNUC_PURE int rte_getmodespk(void) {
  return prefs->max_modes_per_key;
}

G_GNUC_PURE int rte_bg_gen_key(void) {
  return bg_generator_key;
}

G_GNUC_PURE int rte_fg_gen_key(void) {
  return fg_generator_key;
}

G_GNUC_PURE int rte_bg_gen_mode(void) {
  return bg_generator_mode;
}

G_GNUC_PURE int rte_fg_gen_mode(void) {
  return fg_generator_mode;
}






weed_plant_t *get_textparm() {
  // for rte textmode, get first string parameter for current key/mode instance
  // we will then forward all keystrokes to this parm "value" until the exit key (TAB)
  // is pressed

  weed_plant_t *inst,**in_params,*ptmpl,*ret;

  int key=mainw->rte_keys,mode,error,i,hint;

  if (key==-1) return NULL;

  mode=rte_key_getmode(key+1);

  if ((inst=key_to_instance[key][mode])!=NULL) {
    int nparms;

    if (!weed_plant_has_leaf(inst,"in_parameters")||
        (nparms=weed_leaf_num_elements(inst,"in_parameters"))==0) return NULL;

    in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

    for (i=0; i<nparms; i++) {
      ptmpl=weed_get_plantptr_value(in_params[0],"template",&error);

      hint=weed_get_int_value(ptmpl,"hint",&error);

      if (hint==WEED_HINT_TEXT) {
        ret=in_params[i];
        weed_set_int_value(ret,"host_idx",i);
        weed_set_plantptr_value(ret,"host_instance",inst);
        lives_free(in_params);
        return ret;
      }

    }

    lives_free(in_params);

  }

  return NULL;

}



boolean rte_key_setmode(int key, int newmode) {
  // newmode has two special values, -1 = cycle forwards, -2 = cycle backwards

  weed_plant_t *inst,*last_inst;
  int oldmode;
  int blend_file;
  lives_whentostop_t whentostop=mainw->whentostop;
  boolean was_started=FALSE;
  int real_key;
  int error;

  if (key==0) {
    if ((key=mainw->rte_keys)==-1) return FALSE;
  } else key--;

  real_key=key;

  oldmode=key_modes[key];

  if (key_to_fx[key][0]==-1) return FALSE; // nothing is mapped to effect key

  if (newmode==-1) {
    // cycle forwards
    if (oldmode==prefs->max_modes_per_key-1||key_to_fx[key][oldmode+1]==-1) {
      newmode=0;
    } else {
      newmode=key_modes[key]+1;
    }
  }

  if (newmode==-2) {
    // cycle backwards
    newmode=key_modes[key]-1;
    if (newmode<0) {
      for (newmode=prefs->max_modes_per_key-1; newmode>=0; newmode--) {
        if (key_to_fx[key][newmode]!=-1) break;
      }
    }
  }

  if (newmode<0||newmode>=prefs->max_modes_per_key) return FALSE;

  if (key_to_fx[key][newmode]==-1) return FALSE;

  if (rte_window!=NULL) rtew_set_mode_radio(key,newmode);
  if (mainw->ce_thumbs) ce_thumbs_set_mode_combo(key,newmode);

  mainw->osc_block=TRUE;

  // TODO - block template channel changes

  if ((inst=key_to_instance[key][oldmode])!=NULL) {
    was_started=TRUE;
    if (enabled_in_channels(inst,FALSE)==2&&enabled_in_channels(weed_filters[key_to_fx[key][newmode]],FALSE)==2) {
      // transition --> transition, allow any bg generators to survive
      key=-key-1;
    }
  }

  if (oldmode!=newmode) {
    blend_file=mainw->blend_file;

    // handle compound fx
    last_inst=inst;
    while (weed_plant_has_leaf(last_inst,"host_next_instance")) last_inst=weed_get_plantptr_value(last_inst,"host_next_instance",&error);

    if (was_started&&(enabled_in_channels(inst,FALSE)>0||enabled_out_channels(last_inst,FALSE)==0||is_pure_audio(inst,FALSE))) {
      // not a (video or video/audio) generator
      weed_deinit_effect(key);
    } else if (enabled_in_channels(weed_filters[key_to_fx[key][newmode]],FALSE)==0&&
               has_video_chans_out(weed_filters[key_to_fx[key][newmode]],TRUE))
      mainw->whentostop=NEVER_STOP; // when gen->gen, dont stop pb

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
      if (mainw->ce_thumbs) ce_thumbs_add_param_box(real_key,TRUE);
    }
    // TODO - unblock template channel changes
    mainw->whentostop=whentostop;
  }

  mainw->osc_block=FALSE;
  return TRUE;
}


int weed_add_effectkey_by_idx(int key, int idx) {
  // we will add a filter_class at the next free slot for key, and return the slot number
  // if idx is -1 (probably meaning the filter was not found), we return -1
  // if all slots are full, we return -3
  // currently, generators and non-generators cannot be mixed on the same key (causes problems if the mode is switched)
  // in this case a -2 is returned
  boolean has_gen=FALSE;
  boolean has_non_gen=FALSE;

  int i;

  if (idx==-1) return -1;

  key--;

  for (i=0; i<prefs->max_modes_per_key; i++) {
    if (key_to_fx[key][i]!=-1) {
      if (enabled_in_channels(weed_filters[key_to_fx[key][i]],FALSE)==0
          &&has_video_chans_out(weed_filters[key_to_fx[key][i]],TRUE))
        has_gen=TRUE;
      else has_non_gen=TRUE;
    } else {
      if ((enabled_in_channels(weed_filters[idx],FALSE)==0&&has_non_gen&&has_video_chans_out(weed_filters[idx],TRUE)) ||
          (enabled_in_channels(weed_filters[idx],FALSE)>0&&has_gen)) return -2;
      key_to_fx[key][i]=idx;
      return i;
    }
  }
  return -3;
}


int weed_add_effectkey(int key, const char *hashname, boolean fullname) {
  // add a filter_class by hashname to an effect_key
  int idx=weed_get_idx_for_hashname(hashname,fullname);
  return weed_add_effectkey_by_idx(key,idx);
}



int rte_switch_keymode(int key, int mode, const char *hashname) {
  // this is called when we switch the filter_class bound to an effect_key/mode

  int oldkeymode=key_modes[--key];
  int id=weed_get_idx_for_hashname(hashname,TRUE),tid;
  boolean osc_block;
  boolean has_gen=FALSE,has_non_gen=FALSE;

  int test=(mode==0?1:0);

  // effect not found
  if (id==-1) return -1;

  if ((tid=key_to_fx[key][test])!=-1) {
    if (enabled_in_channels(weed_filters[tid],FALSE)==0&&has_video_chans_out(weed_filters[tid],TRUE)) has_gen=TRUE;
    else has_non_gen=TRUE;
  }

  if ((enabled_in_channels(weed_filters[id],FALSE)==0&&has_video_chans_out(weed_filters[id],TRUE)&&has_non_gen)||
      (enabled_in_channels(weed_filters[id],FALSE)>0&&has_gen)) return -2;

  osc_block=mainw->osc_block;
  mainw->osc_block=TRUE;

  // must be done before switching the key_to_fx, as we need to know number of in_parameter_templates
  if (key_defaults[key][mode]!=NULL) free_key_defaults(key,mode);

  if (key_to_instance[key][mode]!=NULL) {
    key_modes[key]=mode;
    weed_deinit_effect(-key-1); // set is_modeswitch
    key_to_fx[key][mode]=id;
    weed_init_effect(-key-1);
    key_modes[key]=oldkeymode;
  } else key_to_fx[key][mode]=id;

  mainw->osc_block=osc_block;

  return 0;
}



void rte_swap_fg_bg(void) {
  int key=fg_generator_key;
  int mode=fg_generator_mode;

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



LiVESList *weed_get_all_names(lives_fx_list_t list_type) {
  // remember to free list after use, if non-NULL
  LiVESList *list=NULL;
  int i,error;
  char *filter_name,*filter_type,*hashname,*string;

  for (i=0; i<num_weed_filters-num_weed_dupes; i++) {
    filter_name=weed_get_string_value(weed_filters[i],"name",&error);
    switch (list_type) {
    case FX_LIST_NAME:
      // just name
      string=lives_strdup(filter_name);
      list=lives_list_append(list,(livespointer)string);
      break;
    case FX_LIST_NAME_AND_TYPE:
      // name and type
      filter_type=weed_filter_get_type(weed_filters[i],TRUE,FALSE);

      if (weed_plant_has_leaf(weed_filters[i],"plugin_unstable")&&
          weed_get_boolean_value(weed_filters[i],"plugin_unstable",&error)==WEED_TRUE) {
        string=lives_strdup_printf(_("%s [unstable] (%s)"),filter_name,filter_type);
      } else string=lives_strdup_printf("%s (%s)",filter_name,filter_type);
      list=lives_list_append(list,(livespointer)string);
      lives_free(filter_type);
      break;
    case FX_LIST_HASHNAME:
      // hashnames
      hashname=make_weed_hashname(i,TRUE,FALSE);
      list=lives_list_append(list,(livespointer)hashname);
      break;
    }
    lives_free(filter_name);
  }
  return list;
}


int rte_get_numfilters(boolean inc_dupes) {
  if (!inc_dupes) return num_weed_filters-num_weed_dupes;
  return num_weed_filters;
}


///////////////////
// parameter interpolation

void fill_param_vals_to(weed_plant_t *param, weed_plant_t *paramtmpl, int index) {
  // for a multi valued parameter or pchange, we will fill "value" up to element index with "new_default"

  // paramtmpl must be supplied, since pchanges do not have one directly

  int i,error,hint;
  int num_vals=weed_leaf_num_elements(param,"value");
  int new_defi,*valis,*nvalis;
  double new_defd,*valds,*nvalds;
  char *new_defs,**valss,**nvalss;
  int cspace;
  int *colsis,*coli;
  int vcount;
  double *colsds,*cold;

  hint=weed_get_int_value(paramtmpl,"hint",&error);

  vcount=weed_leaf_num_elements(param,"value");
  if (index>=vcount) vcount=++index;

  switch (hint) {
  case WEED_HINT_INTEGER:
    new_defi=weed_get_int_value(paramtmpl,"new_default",&error);
    valis=weed_get_int_array(param,"value",&error);
    nvalis=(int *)lives_malloc(vcount*sizint);
    for (i=0; i<vcount; i++) {
      if (i<num_vals&&i<index) nvalis[i]=valis[i];
      else if (i<=num_vals&&i>index) nvalis[i]=valis[i-1];
      else nvalis[i]=new_defi;
    }
    weed_set_int_array(param,"value",vcount,nvalis);
    lives_free(valis);
    lives_free(nvalis);
    break;
  case WEED_HINT_FLOAT:
    new_defd=weed_get_double_value(paramtmpl,"new_default",&error);
    valds=weed_get_double_array(param,"value",&error);
    nvalds=(double *)lives_malloc(vcount*sizdbl);
    for (i=0; i<vcount; i++) {
      if (i<num_vals&&i<index) nvalds[i]=valds[i];
      else if (i<=num_vals&&i>index) nvalds[i]=valds[i-1];
      else nvalds[i]=new_defd;
    }
    weed_set_double_array(param,"value",vcount,nvalds);

    lives_free(valds);
    lives_free(nvalds);
    break;
  case WEED_HINT_SWITCH:
    new_defi=weed_get_boolean_value(paramtmpl,"new_default",&error);
    valis=weed_get_boolean_array(param,"value",&error);
    nvalis=(int *)lives_malloc(vcount*sizint);
    for (i=0; i<vcount; i++) {
      if (i<num_vals&&i<index) nvalis[i]=valis[i];
      else if (i<=num_vals&&i>index) nvalis[i]=valis[i-1];
      else nvalis[i]=new_defi;
    }
    weed_set_boolean_array(param,"value",vcount,nvalis);
    lives_free(valis);
    lives_free(nvalis);
    break;
  case WEED_HINT_TEXT:
    new_defs=weed_get_string_value(paramtmpl,"new_default",&error);
    valss=weed_get_string_array(param,"value",&error);
    nvalss=(char **)lives_malloc(vcount*sizeof(char *));
    for (i=0; i<vcount; i++) {
      if (i<num_vals&&i<index) nvalss[i]=valss[i];
      else if (i<=num_vals&&i>index) nvalss[i]=valss[i-1];
      else nvalss[i]=new_defs;
    }
    weed_set_string_array(param,"value",vcount,nvalss);

    for (i=0; i<index; i++) {
      lives_free(nvalss[i]);
    }

    lives_free(valss);
    lives_free(nvalss);
    break;
  case WEED_HINT_COLOR:
    cspace=weed_get_int_value(paramtmpl,"colorspace",&error);
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      index*=3;
      vcount*=3;
      if (weed_leaf_seed_type(paramtmpl,"new_default")==WEED_SEED_INT) {
        colsis=weed_get_int_array(param,"value",&error);
        if (weed_leaf_num_elements(paramtmpl,"new_default")==1) {
          coli=(int *)lives_malloc(3*sizint);
          coli[0]=coli[1]=coli[2]=weed_get_int_value(paramtmpl,"new_default",&error);
        } else coli=weed_get_int_array(paramtmpl,"new_default",&error);
        valis=weed_get_int_array(param,"value",&error);
        nvalis=(int *)lives_malloc(vcount*sizint);
        for (i=0; i<vcount; i+=3) {
          if (i<num_vals&&i<index) {
            nvalis[i]=valis[i];
            nvalis[i+1]=valis[i+1];
            nvalis[i+2]=valis[i+2];
          } else if (i<=num_vals&&i>index) {
            nvalis[i]=valis[i-3];
            nvalis[i+1]=valis[i-2];
            nvalis[i+2]=valis[i-1];
          } else {
            nvalis[i]=coli[0];
            nvalis[i+1]=coli[1];
            nvalis[i+2]=coli[2];
          }
        }
        weed_set_int_array(param,"value",vcount,nvalis);
        lives_free(valis);
        lives_free(colsis);
        lives_free(nvalis);
      } else {
        colsds=weed_get_double_array(param,"value",&error);
        if (weed_leaf_num_elements(paramtmpl,"new_default")==1) {
          cold=(double *)lives_malloc(3*sizdbl);
          cold[0]=cold[1]=cold[2]=weed_get_double_value(paramtmpl,"new_default",&error);
        } else cold=weed_get_double_array(paramtmpl,"new_default",&error);
        valds=weed_get_double_array(param,"value",&error);
        nvalds=(double *)lives_malloc(vcount*sizdbl);
        for (i=0; i<vcount; i+=3) {
          if (i<num_vals&&i<index) {
            nvalds[i]=valds[i];
            nvalds[i+1]=valds[i+1];
            nvalds[i+2]=valds[i+2];
          } else if (i<=num_vals&&i>index) {
            nvalds[i]=valds[i-3];
            nvalds[i+1]=valds[i-2];
            nvalds[i+2]=valds[i-1];
          } else {
            nvalds[i]=cold[0];
            nvalds[i+1]=cold[1];
            nvalds[i+2]=cold[2];
          }
        }
        weed_set_double_array(param,"value",vcount,nvalds);
        lives_free(valds);
        lives_free(colsds);
        lives_free(nvalds);
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
    pchange=(weed_plant_t *)pchain;
    while (pchange!=NULL) {
      pchange=(weed_plant_t *)weed_get_voidptr_value(pchange,"next_change",&error);
      num++;
    }
  }

  param_array=(weed_plant_t **)lives_malloc((num+1)*sizeof(weed_plant_t *));
  pchange=(weed_plant_t *)pchain;
  while (pchange!=NULL) {
    param_array[i]=weed_plant_new(WEED_PLANT_PARAMETER);
    weed_set_plantptr_value(param_array[i],"template",tmpl);
    weed_leaf_copy(param_array[i],"timecode",pchange,"timecode");
    weed_leaf_copy(param_array[i],"value",pchange,"value");
    weed_add_plant_flags(param_array[i],WEED_LEAF_READONLY_PLUGIN);
    pchange=(weed_plant_t *)weed_get_voidptr_value(pchange,"next_change",&error);
    i++;
  }
  param_array[i]=NULL;
  return param_array;
}


static int get_default_element_int(weed_plant_t *param, int idx, int mpy, int add) {
  int *valsi,val;
  int error;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"host_default")&&weed_leaf_num_elements(ptmpl,"host_default")>idx*mpy+add) {
    valsi=weed_get_int_array(ptmpl,"host_default",&error);
    val=valsi[idx*mpy+add];
    lives_free(valsi);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl,"default")&&weed_leaf_num_elements(ptmpl,"default")>idx*mpy+add) {
    valsi=weed_get_int_array(ptmpl,"default",&error);
    val=valsi[idx*mpy+add];
    lives_free(valsi);
    return val;
  }
  if (weed_leaf_num_elements(ptmpl,"new_default")==mpy) {
    valsi=weed_get_int_array(ptmpl,"default",&error);
    val=valsi[add];
    lives_free(valsi);
    return val;
  }
  return weed_get_int_value(ptmpl,"new_default",&error);
}


static double get_default_element_double(weed_plant_t *param, int idx, int mpy, int add) {
  double *valsd,val;
  int error;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"host_default")&&weed_leaf_num_elements(ptmpl,"host_default")>idx*mpy+add) {
    valsd=weed_get_double_array(ptmpl,"host_default",&error);
    val=valsd[idx*mpy+add];
    lives_free(valsd);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl,"default")&&weed_leaf_num_elements(ptmpl,"default")>idx*mpy+add) {
    valsd=weed_get_double_array(ptmpl,"default",&error);
    val=valsd[idx*mpy+add];
    lives_free(valsd);
    return val;
  }
  if (weed_leaf_num_elements(ptmpl,"new_default")==mpy) {
    valsd=weed_get_double_array(ptmpl,"default",&error);
    val=valsd[add];
    lives_free(valsd);
    return val;
  }
  return weed_get_double_value(ptmpl,"new_default",&error);
}


static int get_default_element_bool(weed_plant_t *param, int idx) {
  int *valsi,val;
  int error;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"host_default")&&weed_leaf_num_elements(ptmpl,"host_default")>idx) {
    valsi=weed_get_boolean_array(ptmpl,"host_default",&error);
    val=valsi[idx];
    lives_free(valsi);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl,"default")&&weed_leaf_num_elements(ptmpl,"default")>idx) {
    valsi=weed_get_boolean_array(ptmpl,"default",&error);
    val=valsi[idx];
    lives_free(valsi);
    return val;
  }
  return weed_get_boolean_value(ptmpl,"new_default",&error);
}


static char *get_default_element_string(weed_plant_t *param, int idx) {
  char **valss,*val,*val2;
  int error,i;
  int numvals;
  weed_plant_t *ptmpl=weed_get_plantptr_value(param,"template",&error);

  if (weed_plant_has_leaf(ptmpl,"host_default")&&(numvals=weed_leaf_num_elements(ptmpl,"host_default"))>idx) {
    valss=weed_get_string_array(ptmpl,"host_default",&error);
    val=lives_strdup(valss[idx]);
    for (i=0; i<numvals; i++) lives_free(valss[i]);
    lives_free(valss);
    return val;
  }
  if (weed_plant_has_leaf(ptmpl,"default")&&(numvals=weed_leaf_num_elements(ptmpl,"default"))>idx) {
    valss=weed_get_string_array(ptmpl,"default",&error);
    val=lives_strdup(valss[idx]);
    for (i=0; i<numvals; i++) lives_free(valss[i]);
    lives_free(valss);
    return val;
  }
  val=weed_get_string_value(ptmpl,"new_default",&error);
  val2=lives_strdup(val);
  lives_free(val);
  return val2;
}






boolean interpolate_param(weed_plant_t *inst, int i, void *pchain, weed_timecode_t tc) {
  // return FALSE if param has no "value" - this can happen during realtime audio processing, if the effect is inited, but no "value" has been set yet
  weed_plant_t **param_array;
  int error,j;
  weed_plant_t *pchange=(weed_plant_t *)pchain,*last_pchange=NULL;
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
  char **valss,**nvalss;
  int num_ign=0;

  if (pchange==NULL) {
    lives_free(in_params);
    return TRUE;
  }

  if (!weed_plant_has_leaf(param,"value")||weed_leaf_num_elements(param,"value")==0) {
    lives_free(in_params);
    return FALSE;  // do not apply effect
  }

  while (pchange!=NULL&&get_event_timecode(pchange)<=tc) {
    last_pchange=pchange;
    pchange=(weed_plant_t *)weed_get_voidptr_value(pchange,"next_change",&error);
  }

  // we need to single thread here, because it's possible to have a conflict - if the audio and video threads are
  // both doing simultaneous interpolation of the same parameter
  pthread_mutex_lock(&mainw->interp_mutex);

  // if plugin wants to do its own interpolation, we let it
  wtmpl=weed_get_plantptr_value(param,"template",&error);
  if (weed_plant_has_leaf(wtmpl,"interpolate_func")) {
    boolean needs_more;
    boolean more_available;
    weed_interpolate_f *interpolate_func_ptr;
    weed_interpolate_f interpolate_func;
    weed_plant_t *calc_param=weed_plant_new(WEED_PLANT_PARAMETER),*filter;
    char *cwd;


    // setup our calc_param (return result)
    weed_set_plantptr_value(calc_param,"template",wtmpl);
    weed_set_int64_value(calc_param,"timecode",tc);

    // try first with just the two surrounding values
    if (pchange==last_pchange&&pchange==NULL) {
      param_array=void_ptrs_to_plant_array(wtmpl,pchange,0);
      more_available=FALSE;
    } else if (last_pchange==NULL) {
      param_array=void_ptrs_to_plant_array(wtmpl,pchange,1);
      more_available=FALSE;
    } else if (pchange==NULL) {
      param_array=void_ptrs_to_plant_array(wtmpl,last_pchange,1);
      more_available=FALSE;
    } else {
      param_array=void_ptrs_to_plant_array(wtmpl,last_pchange,2);
      more_available=TRUE; // maybe...
    }

    weed_add_plant_flags(calc_param,WEED_LEAF_READONLY_PLUGIN);
    interpolate_func_ptr=(weed_interpolate_f *)weed_get_voidptr_value(wtmpl,"interpolate_func",&error);
    interpolate_func=(weed_interpolate_f)*interpolate_func_ptr;
    filter=weed_instance_get_filter(inst,FALSE);
    cwd=cd_to_plugin_dir(filter);
    needs_more=(*interpolate_func)(param_array,calc_param);
    lives_chdir(cwd,FALSE);
    lives_free(cwd);

    if (needs_more==WEED_FALSE||!more_available) {
      // got an accurate result from 2 points
      weed_leaf_copy(param,"value",calc_param,"value");
      weed_plant_free(calc_param);
      lives_free(in_params);
      for (i=0; param_array[i]!=NULL; i++) weed_plant_free(param_array[i]);
      lives_free(param_array);
      pthread_mutex_unlock(&mainw->interp_mutex);
      return TRUE;
    }
    // try to pass more values
    lives_free(param_array);
    param_array=void_ptrs_to_plant_array(wtmpl,pchain,-1);

    (*interpolate_func)(param_array,calc_param);

    weed_leaf_copy(param,"value",calc_param,"value");
    weed_plant_free(calc_param);
    lives_free(in_params);
    for (i=0; param_array[i]!=NULL; i++) weed_plant_free(param_array[i]);
    lives_free(param_array);
    pthread_mutex_unlock(&mainw->interp_mutex);
    return TRUE;
  }

  num_values=weed_leaf_num_elements(param,"value");

  if ((num_pvals=weed_leaf_num_elements((weed_plant_t *)pchain,"value"))>num_values)
    num_values=num_pvals; // init a multivalued param

  lpc=(void **)lives_malloc(num_values*sizeof(void *));
  npc=(void **)lives_malloc(num_values*sizeof(void *));

  if (num_values==1) {
    lpc[0]=last_pchange;
    npc[0]=pchange;
  } else {
    pchange=(weed_plant_t *)pchain;

    for (j=0; j<num_values; j++) npc[j]=lpc[j]=NULL;

    while (pchange!=NULL) {
      num_pvals=weed_leaf_num_elements(pchange,"value");
      if (num_pvals>num_values) num_pvals=num_values;
      if (weed_plant_has_leaf(pchange,"ignore")) {
        num_ign=weed_leaf_num_elements(pchange,"ignore");
        ign=weed_get_boolean_array(pchange,"ignore",&error);
      } else ign=NULL;
      if (get_event_timecode(pchange)<=tc) {
        for (j=0; j<num_pvals; j++) if (ign==NULL||j>=num_ign||ign[j]==WEED_FALSE) lpc[j]=pchange;
      } else {
        for (j=0; j<num_pvals; j++) {
          if (npc[j]==NULL&&(ign==NULL||j>=num_ign||ign[j]==WEED_FALSE)) npc[j]=pchange;
        }
        got_npc=0;
        for (j=0; j<num_values; j++) {
          if (npc[j]!=NULL) got_npc++;
        }
        if (got_npc==num_values) {
          if (ign!=NULL) lives_free(ign);
          break;
        }
      }
      pchange=(weed_plant_t *)weed_get_voidptr_value(pchange,"next_change",&error);
      if (ign!=NULL) lives_free(ign);
    }
  }


  hint=weed_get_int_value(wtmpl,"hint",&error);
  switch (hint) {
  case WEED_HINT_FLOAT:
    valds=(double *)lives_malloc(num_values*(sizeof(double)));
    break;
  case WEED_HINT_COLOR:
    cspace=weed_get_int_value(wtmpl,"colorspace",&error);
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (num_values%3!=0) return TRUE;
      if (weed_leaf_seed_type(wtmpl,"default")==WEED_SEED_INT) {
        valis=(int *)lives_malloc(num_values*sizint);
      } else {
        valds=(double *)lives_malloc(num_values*(sizeof(double)));
      }
      break;
    }
    break;
  case WEED_HINT_SWITCH:
  case WEED_HINT_INTEGER:
    valis=(int *)lives_malloc(num_values*sizint);
    break;
  }



  for (j=0; j<num_values; j++) {
    // must interpolate - we use linear interpolation
    if (lpc[j]==NULL&&npc[j]==NULL) continue;
    if (lpc[j]!=NULL&&npc[j]!=NULL) tc_diff=weed_get_int64_value((weed_plant_t *)npc[j],"timecode",&error)-
                                              weed_get_int64_value((weed_plant_t *)lpc[j],"timecode",&error);
    switch (hint) {
    case WEED_HINT_FLOAT:
      if (lpc[j]==NULL) {
        // before first change
        valds[j]=get_default_element_double(param,j,1,0);
        continue;
      }
      if (npc[j]==NULL) {
        // after last change
        xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
        if (xnum>j) {
          nvalds=weed_get_double_array((weed_plant_t *)lpc[j],"value",&error);
          valds[j]=nvalds[j];
          lives_free(nvalds);
        } else valds[j]=get_default_element_double(param,j,1,0);
        continue;
      }

      next_valuesd=weed_get_double_array((weed_plant_t *)npc[j],"value",&error);
      last_valuesd=weed_get_double_array((weed_plant_t *)lpc[j],"value",&error);
      xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
      if (xnum>j) last_valued=last_valuesd[j];
      else last_valued=get_default_element_double(param,j,1,0);

      valds[j]=last_valued+(double)(next_valuesd[j]-last_valued)/(double)(tc_diff/U_SEC)*
               (double)((tc-weed_get_int64_value((weed_plant_t *)lpc[j],"timecode",&error))/U_SEC);

      lives_free(last_valuesd);
      lives_free(next_valuesd);
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
            xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
            if (xnum>k) {
              nvalis=weed_get_int_array((weed_plant_t *)lpc[j],"value",&error);
              valis[k]=nvalis[k];
              valis[k+1]=nvalis[k+1];
              valis[k+2]=nvalis[k+2];
              lives_free(nvalis);
            } else {
              valis[k]=get_default_element_int(param,j,3,0);
              valis[k+1]=get_default_element_int(param,j,3,1);
              valis[k+2]=get_default_element_int(param,j,3,2);
            }
            j+=3;
            continue;
          }

          next_valuesi=weed_get_int_array((weed_plant_t *)npc[j],"value",&error);
          last_valuesi=weed_get_int_array((weed_plant_t *)lpc[j],"value",&error);
          xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
          if (xnum>k) {
            last_valueir=last_valuesi[k];
            last_valueig=last_valuesi[k+1];
            last_valueib=last_valuesi[k+2];
          } else {
            last_valueir=get_default_element_int(param,j,3,0);
            last_valueig=get_default_element_int(param,j,3,1);
            last_valueib=get_default_element_int(param,j,3,2);
          }

          if (next_valuesi==NULL) continue; // can happen if we recorded a param change

          valis[k]=last_valueir+(next_valuesi[k]-last_valueir)/(tc_diff/U_SEC)*
                   ((tc_diff2=(tc-weed_get_int64_value((weed_plant_t *)lpc[j],"timecode",&error)))/U_SEC)+.5;
          valis[k+1]=last_valueig+(next_valuesi[k+1]-last_valueig)/(tc_diff/U_SEC)*(tc_diff2/U_SEC)+.5;
          valis[k+2]=last_valueib+(next_valuesi[k+2]-last_valueib)/(tc_diff/U_SEC)*(tc_diff2/U_SEC)+.5;

          lives_free(last_valuesi);
          lives_free(next_valuesi);
        } else {
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
            xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
            if (xnum>k) {
              nvalds=weed_get_double_array((weed_plant_t *)lpc[j],"value",&error);
              valds[k]=nvalds[k];
              valds[k+1]=nvalds[k+1];
              valds[k+2]=nvalds[k+2];
              lives_free(nvalds);
            } else {
              valds[k]=get_default_element_double(param,j,3,0);
              valds[k+1]=get_default_element_double(param,j,3,1);
              valds[k+2]=get_default_element_double(param,j,3,2);
            }
            j+=3;
            continue;
          }

          next_valuesd=weed_get_double_array((weed_plant_t *)npc[j],"value",&error);
          last_valuesd=weed_get_double_array((weed_plant_t *)lpc[j],"value",&error);
          xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
          if (xnum>k) {
            last_valuedr=last_valuesd[k];
            last_valuedg=last_valuesd[k+1];
            last_valuedb=last_valuesd[k+2];
          } else {
            last_valuedr=get_default_element_double(param,j,3,0);
            last_valuedg=get_default_element_double(param,j,3,1);
            last_valuedb=get_default_element_double(param,j,3,2);
          }
          valds[k]=last_valuedr+(next_valuesd[k]-last_valuedr)/(tc_diff/U_SEC)*
                   ((tc_diff2=(tc-weed_get_int64_value((weed_plant_t *)lpc[j],"timecode",&error)))/U_SEC);
          valds[k+1]=last_valuedg+(next_valuesd[k+1]-last_valuedg)/(tc_diff/U_SEC)*(tc_diff2/U_SEC)+.5;
          valds[k+2]=last_valuedb+(next_valuesd[k+2]-last_valuedb)/(tc_diff/U_SEC)*(tc_diff2/U_SEC)+.5;

          lives_free(last_valuesd);
          lives_free(next_valuesd);
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
        if (npc[j]!=NULL&&get_event_timecode((weed_plant_t *)npc[j])==tc) {
          nvalis=weed_get_int_array((weed_plant_t *)npc[j],"value",&error);
          valis[j]=nvalis[j];
          lives_free(nvalis);
          continue;
        } else {
          // use last_pchange value
          xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
          if (xnum>j) {
            nvalis=weed_get_int_array((weed_plant_t *)lpc[j],"value",&error);
            valis[j]=nvalis[j];
            lives_free(nvalis);
          } else valis[j]=get_default_element_int(param,j,1,0);
          continue;
        }
      } else {
        if (lpc[j]==NULL) {
          // before first change
          valis[j]=get_default_element_int(param,j,1,0);
          continue;
        }
        if (npc[j]==NULL) {
          // after last change
          xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
          if (xnum>j) {
            nvalis=weed_get_int_array((weed_plant_t *)lpc[j],"value",&error);
            valis[j]=nvalis[j];
            lives_free(nvalis);
          } else valis[j]=get_default_element_int(param,j,1,0);
          continue;
        }

        next_valuesi=weed_get_int_array((weed_plant_t *)npc[j],"value",&error);
        last_valuesi=weed_get_int_array((weed_plant_t *)lpc[j],"value",&error);
        xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
        if (xnum>j) last_valuei=last_valuesi[j];
        else last_valuei=get_default_element_int(param,j,1,0);

        valis[j]=last_valuei+(next_valuesi[j]-last_valuei)/(tc_diff/U_SEC)*
                 ((tc-weed_get_int64_value((weed_plant_t *)lpc[j],"timecode",&error))/U_SEC)+.5;

        lives_free(last_valuesi);
        lives_free(next_valuesi);
        break;
      }
    case WEED_HINT_SWITCH:
      // no interpolation
      if (npc[j]!=NULL&&get_event_timecode((weed_plant_t *)npc[j])==tc) {
        nvalis=weed_get_boolean_array((weed_plant_t *)npc[j],"value",&error);
        valis[j]=nvalis[j];
        lives_free(nvalis);
        continue;
      } else {
        // use last_pchange value
        xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
        if (xnum>j) {
          nvalis=weed_get_boolean_array((weed_plant_t *)lpc[j],"value",&error);
          valis[j]=nvalis[j];
          lives_free(nvalis);
        } else valis[j]=get_default_element_bool(param,j);
        continue;
      }
      break;
    case WEED_HINT_TEXT:
      // no interpolation
      valss=weed_get_string_array(param,"value",&error);

      if (npc[j]!=NULL&&get_event_timecode((weed_plant_t *)npc[j])==tc) {
        nvalss=weed_get_string_array((weed_plant_t *)npc[j],"value",&error);
        valss[j]=lives_strdup(nvalss[j]);
        for (k=0; k<num_values; k++) lives_free(nvalss[k]);
        lives_free(nvalss);
        weed_set_string_array(param,"value",num_values,valss);
        for (k=0; k<num_values; k++) lives_free(valss[k]);
        lives_free(valss);
        continue;
      } else {
        // use last_pchange value
        xnum=weed_leaf_num_elements((weed_plant_t *)lpc[j],"value");
        if (xnum>j) {
          nvalss=weed_get_string_array((weed_plant_t *)lpc[j],"value",&error);
          valss[j]=lives_strdup(nvalss[j]);
          for (k=0; k<xnum; k++) lives_free(nvalss[k]);
          lives_free(nvalss);
        } else valss[j]=get_default_element_string(param,j);
        weed_set_string_array(param,"value",num_values,valss);
        for (k=0; k<num_values; k++) lives_free(valss[k]);
        lives_free(valss);
        continue;
      }
      break;
    } // parameter hint
  } // j


  switch (hint) {
  case WEED_HINT_FLOAT:
    weed_set_double_array(param,"value",num_values,valds);
    lives_free(valds);
    break;
  case WEED_HINT_COLOR:
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (weed_leaf_seed_type(wtmpl,"default")==WEED_SEED_INT) {
        weed_set_int_array(param,"value",num_values,valis);
        lives_free(valis);
      } else {
        weed_set_double_array(param,"value",num_values,valds);
        lives_free(valds);
      }
      break;
    }
    break;
  case WEED_HINT_INTEGER:
    weed_set_int_array(param,"value",num_values,valis);
    lives_free(valis);
    break;
  case WEED_HINT_SWITCH:
    weed_set_boolean_array(param,"value",num_values,valis);
    lives_free(valis);
    break;
  }

  pthread_mutex_unlock(&mainw->interp_mutex);

  lives_free(npc);
  lives_free(lpc);
  lives_free(in_params);
  return TRUE;
}


boolean interpolate_params(weed_plant_t *inst, void **pchains, weed_timecode_t tc) {
  // interpolate all in_parameters for filter_instance inst, using void **pchain, which is an array of param_change events in temporal order
  // values are calculated for timecode tc. We skip "hidden" parameters
  void *pchain;
  int num_params;
  int offset=0,error;

  register int i;

  do {

    if (!weed_plant_has_leaf(inst,"in_parameters")||pchains==NULL) continue;

    num_params=weed_leaf_num_elements(inst,"in_parameters");

    if (num_params==0) continue; // no in_parameters ==> do nothing

    for (i=offset; i<offset+num_params; i++) {
      if (!is_hidden_param(inst,i-offset)) {
        pchain=pchains[i];
        if (pchain!=NULL&&WEED_PLANT_IS_EVENT((weed_plant_t *)pchain)&&WEED_EVENT_IS_PARAM_CHANGE((weed_plant_t *)pchain))
          if (!interpolate_param(inst,i-offset,pchain,tc)) return FALSE; // FALSE if param is not ready
      }
    }

    offset+=num_params;

  } while (weed_plant_has_leaf(inst,"host_next_instance")&&(inst=weed_get_plantptr_value(inst,"host_next_instance",&error))!=NULL);



  return TRUE;
}




///////////////////////////////////////////////////////////
////// hashnames

char *make_weed_hashname(int filter_idx, boolean fullname, boolean use_extra_authors) {
  // return value should be freed after use

  // make hashname from filter_idx: if use_extra_authors is set we use "extra_authors" instead of "authors"
  // (for reverse compatibility)

  weed_plant_t *filter,*plugin_info;

  char plugin_fname[PATH_MAX];

  char *plugin_name,*filter_name,*filter_author,*filter_version,*hashname,*filename;

  int error,version;

  if (filter_idx<0||filter_idx>=num_weed_filters) return lives_strdup("");

  filter=weed_filters[filter_idx];

  if (hashnames[filter_idx]!=NULL&&fullname&&(!use_extra_authors||!weed_plant_has_leaf(filter,"extra_authors")))
    return lives_strdup(hashnames[filter_idx]);

  if (weed_plant_has_leaf(filter,"plugin_info")) {
    plugin_info=weed_get_plantptr_value(filter,"plugin_info",&error);
    plugin_name=weed_get_string_value(plugin_info,"name",&error);

    lives_snprintf(plugin_fname,PATH_MAX,"%s",plugin_name);
    lives_free(plugin_name);
    get_filename(plugin_fname,TRUE);
  } else memset(plugin_fname,0,1);

  filter_name=weed_get_string_value(filter,"name",&error);

  // should we really use utf-8 here ? (needs checking)
  filename=F2U8(plugin_fname);

  if (fullname) {

    if (!use_extra_authors||!weed_plant_has_leaf(filter,"extra_authors"))
      filter_author=weed_get_string_value(filter,"author",&error);
    else
      filter_author=weed_get_string_value(filter,"extra_authors",&error);

    version=weed_get_int_value(filter,"version",&error);
    filter_version=lives_strdup_printf("%d",version);

    hashname=lives_strconcat(filename,filter_name,filter_author,filter_version,NULL);
    lives_free(filter_author);
    lives_free(filter_version);
  } else hashname=lives_strconcat(filename,filter_name,NULL);
  lives_free(filter_name);
  lives_free(filename);

  //g_print("added filter %s\n",hashname);

  return hashname;
}



int weed_get_idx_for_hashname(const char *hashname, boolean fullname) {
  register int i;
  char *chashname;

  for (i=0; i<num_weed_filters; i++) {
    chashname=make_weed_hashname(i,fullname,FALSE);
    if (!lives_utf8_strcasecmp(hashname,chashname)) {
      lives_free(chashname);
      return i;
    }
    lives_free(chashname);
    if (fullname) {
      chashname=make_weed_hashname(i,fullname,TRUE);
      if (!lives_utf8_strcasecmp(hashname,chashname)) {
        lives_free(chashname);
        return i;
      }
      lives_free(chashname);
    }
  }
  return -1;
}



static boolean check_match(weed_plant_t *filter, const char *pkg, const char *fxname, const char *auth, int version) {
  // perform template matching for a function, see weed_get_indices_from_template()

  weed_plant_t *plugin_info;

  char plugin_fname[PATH_MAX];
  char *plugin_name,*filter_name,*filter_author;

  int filter_version;
  int error;

  if (pkg != NULL && strlen(pkg)) {
    char *filename;

    if (weed_plant_has_leaf(filter,"plugin_info")) {
      plugin_info=weed_get_plantptr_value(filter,"plugin_info",&error);
      plugin_name=weed_get_string_value(plugin_info,"name",&error);

      lives_snprintf(plugin_fname,PATH_MAX,"%s",plugin_name);
      lives_free(plugin_name);
      get_filename(plugin_fname,TRUE);
    } else memset(plugin_fname,0,1);

    filename=F2U8(plugin_fname);

    if (lives_utf8_strcasecmp(pkg,filename)) {
      lives_free(filename);
      return FALSE;
    }
    lives_free(filename);
  }


  if (fxname != NULL && strlen(fxname)) {
    filter_name=weed_get_string_value(filter,"name",&error);
    if (lives_utf8_strcasecmp(fxname,filter_name)) {
      lives_free(filter_name);
      return FALSE;
    }
    lives_free(filter_name);
  }

  if (auth != NULL && strlen(auth)) {
    filter_author=weed_get_string_value(filter,"author",&error);
    if (lives_utf8_strcasecmp(auth,filter_author)) {
      if (weed_plant_has_leaf(filter,"extra_authors")) {
        lives_free(filter_author);
        filter_author=weed_get_string_value(filter,"extra_authors",&error);
        if (lives_utf8_strcasecmp(auth,filter_author)) {
          lives_free(filter_author);
          return FALSE;
        }
      } else {
        lives_free(filter_author);
        return FALSE;
      }
    }
    lives_free(filter_author);
  }


  if (version>0) {
    filter_version=weed_get_int_value(filter,"version",&error);
    if (version!=filter_version) return FALSE;
  }

  return TRUE;
}



int *weed_get_indices_from_template(const char *pkg, const char *fxname, const char *auth, int version) {
  // generate a list of filter indices from a given template. Char values may be NULL or "" to signify match-any.
  // version may be 0 to signify match-any
  // otherwise values must match those specified

  // returns: a list of int indices, with the last value == -1
  // return value should be freed after use

  weed_plant_t *filter;
  int *rvals;

  int count=1,count2=0;

  register int i;

  // count number of return values
  for (i=0; i<num_weed_filters; i++) {
    filter=weed_filters[i];

    if (check_match(filter,pkg,fxname,auth,version)) {
      count++;
    }
  }

  // allocate storage space
  rvals=(int *)lives_malloc(count*sizint);
  i=0;

  // get return values
  while (count2 < count-1) {
    filter=weed_filters[i++];

    if (check_match(filter,pkg,fxname,auth,version)) {
      rvals[count2++]=i-1;
    }
  }

  // end-of-array indicator
  rvals[count2]=-1;

  return rvals;
}



weed_plant_t *get_weed_filter(int idx) {
  if (idx>-1&&idx<num_weed_filters) return weed_filters[idx];
  return NULL;
}




static void weed_leaf_serialise(int fd, weed_plant_t *plant, const char *key, boolean write_all, unsigned char **mem) {
  void *value,*valuer;
  uint32_t vlen;
  int st,ne;
  int j;
  uint32_t i=(uint32_t)strlen(key);

  // write errors will be checked for by the calling function

  if (write_all) {
    // write byte length of key, followed by key in utf-8
    if (mem==NULL) {
      lives_write_le_buffered(fd,&i,4,TRUE);
      lives_write_buffered(fd,key,(size_t)i,TRUE);
    } else {
      lives_memcpy(*mem,&i,4);
      *mem+=4;
      lives_memcpy(*mem,key,(size_t)i);
      *mem+=i;
    }
  }

  // write seed type and number of elements
  st=weed_leaf_seed_type(plant,key);

  if (mem==NULL) lives_write_le_buffered(fd,&st,4,TRUE);
  else {
    lives_memcpy(*mem,&st,4);
    *mem+=4;
  }
  ne=weed_leaf_num_elements(plant,key);
  if (mem==NULL) lives_write_le_buffered(fd,&ne,4,TRUE);
  else {
    lives_memcpy(*mem,&ne,4);
    *mem+=4;
  }

  // write errors will be checked for by the calling function


  // for each element, write the data size followed by the data
  for (j=0; j<ne; j++) {
    vlen=(uint32_t)weed_leaf_element_size(plant,key,j);
    if (st!=WEED_SEED_STRING) {
      value=lives_malloc((size_t)vlen);
      weed_leaf_get(plant,key,j,value);
    } else {
      value=lives_malloc((size_t)(vlen+1));
      weed_leaf_get(plant,key,j,&value);
    }

    if (mem==NULL && weed_leaf_seed_type(plant,key)>=64) {
      // save voidptr as 64 bit values (**NEW**)
      valuer=(uint64_t *)lives_malloc(sizeof(uint64_t));
      *((uint64_t *)valuer)=(uint64_t)(*((void **)value));
      vlen=sizeof(uint64_t);
    } else valuer=value;

    if (mem==NULL) {
      lives_write_le_buffered(fd,&vlen,4,TRUE);
      if (st!=WEED_SEED_STRING) {
        lives_write_le_buffered(fd,valuer,(size_t)vlen,TRUE);
      } else lives_write_buffered(fd,valuer,(size_t)vlen,TRUE);
    } else {
      lives_memcpy(*mem,&vlen,4);
      *mem+=4;
      lives_memcpy(*mem,value,(size_t)vlen);
      *mem+=vlen;
    }
    lives_free(value);
    if (valuer!=value) lives_free(valuer);
  }

  // write errors will be checked for by the calling function

}



boolean weed_plant_serialise(int fd, weed_plant_t *plant, unsigned char **mem) {
  // write errors will be checked for by the calling function


  int i=0;
  char **proplist=weed_plant_list_leaves(plant);
  char *prop;
  for (prop=proplist[0]; (prop=proplist[i])!=NULL; i++);

  if (mem==NULL) lives_write_le_buffered(fd,&i,4,TRUE); // write number of leaves
  else {
    lives_memcpy(*mem,&i,4);
    *mem+=4;
  }

  // write errors will be checked for by the calling function

  weed_leaf_serialise(fd,plant,"type",TRUE,mem);
  i=0;

  for (prop=proplist[0]; (prop=proplist[i])!=NULL; i++) {
    // write each leaf and key
    if (strcmp(prop,"type")) weed_leaf_serialise(fd,plant,prop,TRUE,mem);
    lives_free(prop);
  }
  lives_free(proplist);
  return TRUE;
}



static int weed_leaf_deserialise(int fd, weed_plant_t *plant, const char *key, unsigned char **mem,
                                 boolean check_key) {
  // if plant is NULL, returns type
  // "host_default" sets key; otherwise NULL
  // check_key set to TRUE - check that we read the correct key

  void **values;

  ssize_t bytes;

  int *ints;
  double *dubs;
  int64_t *int64s;

  uint32_t len,vlen;

  char *mykey=NULL;

  int st; // seed type
  int ne; // num elems
  int type=0;

  register int i,j;

  if (key==NULL || check_key) {
    if (mem==NULL) {
      if (lives_read_le_buffered(fd,&len,4,TRUE)<4) {
        return -4;
      }
    } else {
      lives_memcpy(&len,*mem,4);
      *mem+=4;
    }

    if (check_key&&len!=strlen(key)) return -9;

    if (len>65535) return -10;

    mykey=(char *)lives_try_malloc((size_t)len+1);
    if (mykey==NULL) return -5;

    if (mem==NULL) {
      if (lives_read_buffered(fd,mykey,(size_t)len,TRUE)<len) {
        lives_free(mykey);
        return -4;
      }
    } else {
      lives_memcpy(mykey,*mem,(size_t)len);
      *mem+=len;
    }
    memset(mykey+(size_t)len,0,1);

    if (check_key&&strcmp(mykey,key)) {
      lives_free(mykey);
      return -1;
    }

    if (key==NULL) key=mykey;
    else {
      lives_free(mykey);
      mykey=NULL;
    }
  }


  if (mem==NULL) {
    if (lives_read_le_buffered(fd,&st,4,TRUE)<4) {
      if (mykey!=NULL) lives_free(mykey);
      return -4;
    }
  } else {
    lives_memcpy(&st,*mem,4);
    *mem+=4;
  }

  if (st!=WEED_SEED_INT&&st!=WEED_SEED_BOOLEAN&&st!=WEED_SEED_DOUBLE&&st!=WEED_SEED_INT64&&
      st!=WEED_SEED_STRING&&st!=WEED_SEED_VOIDPTR&&st!=WEED_SEED_PLANTPTR) {
    return -6;
  }

  if (check_key&&!strcmp(key,"type")) {
    // for the "type" leaf perform some extra checks
    if (st!=WEED_SEED_INT) {
      return -2;
    }
  }


  if (mem==NULL) {
    if (lives_read_le_buffered(fd,&ne,4,TRUE)<4) {
      if (mykey!=NULL) lives_free(mykey);
      return -4;
    }
  } else {
    lives_memcpy(&ne,*mem,4);
    *mem+=4;
  }


  if (ne>0) values=(void **)lives_malloc(ne*sizeof(void *));
  else values=NULL;

  if (check_key&&!strcmp(key,"type")) {
    // for the "type" leaf perform some extra checks
    if (ne!=1) {
      return -3;
    }
  }

  for (i=0; i<ne; i++) {
    if (mem==NULL) {
      bytes=lives_read_le_buffered(fd,&vlen,4,TRUE);
      if (bytes<4) {
        for (--i; i>=0; lives_free(values[i--]));
        lives_free(values);
        if (mykey!=NULL) lives_free(mykey);
        return -4;
      }
    } else {
      lives_memcpy(&vlen,*mem,4);
      *mem+=4;
    }

    if (st==WEED_SEED_STRING) {
      values[i]=lives_malloc((size_t)vlen+1);
    } else {
      if (vlen<=8) {
        values[i]=lives_malloc((size_t)vlen);
      } else return -8;
    }

    if (mem==NULL) {
      if (st!=WEED_SEED_STRING)
        bytes=lives_read_le_buffered(fd,values[i],vlen,TRUE);
      else
        bytes=lives_read_buffered(fd,values[i],vlen,TRUE);
      if (bytes<vlen) {
        for (--i; i>=0; lives_free(values[i--]));
        lives_free(values);
        if (mykey!=NULL) lives_free(mykey);
        return -4;
      }
    } else {
      lives_memcpy(values[i],*mem,vlen);
      *mem+=vlen;
    }
    if (st==WEED_SEED_STRING) {
      memset((char *)values[i]+vlen,0,1);
    }
  }

  if (plant==NULL&&!strcmp(key,"type")) {
    type=*(int *)(values[0]);
  } else {
    if (values==NULL) weed_leaf_set(plant,key,st,0,NULL);
    else {
      switch (st) {
      case WEED_SEED_INT:
        // fallthrough
      case WEED_SEED_BOOLEAN:
        ints=(int *)lives_malloc(ne*4);
        for (j=0; j<ne; j++) ints[j]=*(int *)values[j];
        weed_leaf_set(plant, key, st, ne, (void *)ints);
        lives_free(ints);
        break;
      case WEED_SEED_DOUBLE:
        dubs=(double *)lives_malloc(ne*sizdbl);
        for (j=0; j<ne; j++) dubs[j]=*(double *)values[j];
        weed_leaf_set(plant, key, st, ne, (void *)dubs);
        lives_free(dubs);
        break;
      case WEED_SEED_INT64:
        int64s=(int64_t *)lives_malloc(ne*8);
        for (j=0; j<ne; j++) int64s[j]=*(int64_t *)values[j];
        weed_leaf_set(plant, key, st, ne, (void *)int64s);
        lives_free(int64s);
        break;
      case WEED_SEED_STRING:
        weed_leaf_set(plant, key, st, ne, (void *)values);
        break;
      default:
        if (plant!=NULL) {
          if (mem==NULL && prefs->force64bit) {
            // force pointers to uint64_t
            uint64_t *voids=(uint64_t *)lives_malloc(ne*sizeof(uint64_t));
            for (j=0; j<ne; j++) voids[j]=(uint64_t)(*(void **)values[j]);
            weed_leaf_set(plant, key, WEED_SEED_INT64, ne, (void *)voids);
            lives_free(voids);
          } else {
            void **voids=(void **)lives_malloc(ne*sizeof(void *));
            for (j=0; j<ne; j++) voids[j]=*(void **)values[j];
            weed_leaf_set(plant, key, st, ne, (void *)voids);
            lives_free(voids);
          }
        }
      }
    }
  }

  if (values!=NULL) {
    for (i=0; i<ne; i++) lives_free(values[i]);
    lives_free(values);
  }
  if (mykey!=NULL) lives_free(mykey);
  if (plant==NULL) return type;
  return 0;
}



weed_plant_t *weed_plant_deserialise(int fd, unsigned char **mem) {
  // deserialise a plant from file fd or mem
  weed_plant_t *plant;
  int numleaves;
  ssize_t bytes;
  int err;

  // caller should clear and check mainw->read_failed

  if (mem==NULL) {
    if ((bytes=lives_read_le_buffered(fd,&numleaves,4,TRUE))<4) {
      mainw->read_failed=FALSE; // we are allowed to EOF here
      return NULL;
    }
  } else {
    lives_memcpy(&numleaves,*mem,4);
    *mem+=4;
  }

  plant=weed_plant_new(WEED_PLANT_UNKNOWN);
  weed_leaf_set_flags(plant,"type",0);

  if ((err=weed_leaf_deserialise(fd,plant,"type",mem,TRUE))) {
    // check the "type" leaf first
    weed_plant_free(plant);
    return NULL;
  }

  numleaves--;

  while (numleaves--) {
    if ((err=weed_leaf_deserialise(fd,plant,NULL,mem,FALSE))) {
      weed_plant_free(plant);
      return NULL;
    }
  }
  if (weed_get_plant_type(plant)==WEED_PLANT_UNKNOWN) {
    weed_plant_free(plant);
    return NULL;
  }
  weed_leaf_set_flags(plant,"type",WEED_LEAF_READONLY_PLUGIN|WEED_LEAF_READONLY_HOST);

  return plant;
}



boolean write_filter_defaults(int fd, int idx) {
  // return FALSE on write error
  char *hashname;
  weed_plant_t *filter=weed_filters[idx],**ptmpls;
  int num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  int i,error;
  boolean wrote_hashname=FALSE;
  size_t vlen;
  int ntowrite=0;

  if (num_params==0) return TRUE;
  ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);

  for (i=0; i<num_params; i++) {
    if (weed_plant_has_leaf(ptmpls[i],"host_default")) {
      ntowrite++;
    }
  }

  mainw->write_failed=FALSE;

  for (i=0; i<num_params; i++) {
    if (weed_plant_has_leaf(ptmpls[i],"host_default")) {
      if (!wrote_hashname) {
        hashname=make_weed_hashname(idx,TRUE,FALSE);
        vlen=strlen(hashname);

        lives_write_le_buffered(fd,&vlen,4,TRUE);
        lives_write_buffered(fd,hashname,vlen,TRUE);
        lives_free(hashname);
        wrote_hashname=TRUE;
        lives_write_le_buffered(fd,&ntowrite,4,TRUE);
      }
      lives_write_le_buffered(fd,&i,4,TRUE);
      weed_leaf_serialise(fd,ptmpls[i],"host_default",FALSE,NULL);
    }
  }
  if (wrote_hashname) lives_write_buffered(fd,"\n",1,TRUE);

  if (ptmpls!=NULL) lives_free(ptmpls);

  if (mainw->write_failed) {
    return FALSE;
  }
  return TRUE;
}



boolean read_filter_defaults(int fd) {
  weed_plant_t *filter,**ptmpls;
  void *buf;

  size_t vlen;

  int vleni,vlenz;
  int i,error,pnum;
  int num_params=0;
  int ntoread;

  boolean found=FALSE;

  char *tmp;

  mainw->read_failed=FALSE;

  while (1) {
    if (lives_read_le_buffered(fd,&vleni,4,TRUE)<4) {
      // we are allowed to EOF here
      mainw->read_failed=FALSE;
      break;
    }

    // some files erroneously used a vlen of 8
    if (lives_read_le_buffered(fd,&vlenz,4,TRUE)<4) {
      // we are allowed to EOF here
      mainw->read_failed=FALSE;
      break;
    }

    vlen=(size_t)vleni;

    if (capable->byte_order==LIVES_BIG_ENDIAN&&prefs->bigendbug) {
      if (vleni==0&&vlenz!=0) vlen=(size_t)vlenz;
    } else {
      if (vlenz!=0) {
        if (lives_lseek_buffered_rdonly(fd,-4)<0) return FALSE;
      }
    }

    if (vlen>65535) return FALSE;

    buf=lives_malloc(vlen+1);
    if (lives_read_buffered(fd,buf,vlen,TRUE)<vlen) break;

    memset((char *)buf+vlen,0,1);
    for (i=0; i<num_weed_filters; i++) {
      if (!strcmp((char *)buf,(tmp=make_weed_hashname(i,TRUE,FALSE)))) {
        lives_free(tmp);
        found=TRUE;
        break;
      }
      lives_free(tmp);
    }
    if (!found) {
      for (i=0; i<num_weed_filters; i++) {
        if (!strcmp((char *)buf,(tmp=make_weed_hashname(i,TRUE,TRUE)))) {
          lives_free(tmp);
          break;
        }
        lives_free(tmp);
      }
    }

    lives_free(buf);

    ptmpls=NULL;

    if (i<num_weed_filters) {
      filter=weed_filters[i];
      num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
      if (num_params>0) ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
    } else num_params=0;

    if (lives_read_le_buffered(fd,&ntoread,4,TRUE)<4) {
      if (ptmpls!=NULL) lives_free(ptmpls);
      break;
    }

    for (i=0; i<ntoread; i++) {
      if (lives_read_le_buffered(fd,&pnum,4,TRUE)<4) {
        if (ptmpls!=NULL) lives_free(ptmpls);
        break;
      }

      if (pnum<num_params) {
        weed_leaf_deserialise(fd,ptmpls[pnum],"host_default",NULL,FALSE);
      } else {
        weed_plant_t *dummyplant=weed_plant_new(WEED_PLANT_UNKNOWN);
        weed_leaf_deserialise(fd,dummyplant,"host_default",NULL,FALSE);
        weed_plant_free(dummyplant);
      }
      if (mainw->read_failed) {
        break;
      }
    }
    if (mainw->read_failed) {
      if (ptmpls!=NULL) lives_free(ptmpls);
      break;
    }

    buf=lives_malloc(strlen("\n"));
    lives_read_buffered(fd,buf,strlen("\n"),TRUE);
    lives_free(buf);
    if (ptmpls!=NULL) lives_free(ptmpls);
    if (mainw->read_failed) {
      break;
    }
  }

  if (mainw->read_failed) return FALSE;
  return TRUE;

}




boolean write_generator_sizes(int fd, int idx) {
  // TODO - handle optional channels
  // return FALSE on write error
  char *hashname;
  weed_plant_t *filter,**ctmpls;
  int num_channels;
  int i,error;
  size_t vlen;
  boolean wrote_hashname=FALSE;

  num_channels=enabled_in_channels(weed_filters[idx],FALSE);
  if (num_channels!=0) return TRUE;

  filter=weed_filters[idx];

  num_channels=weed_leaf_num_elements(filter,"out_channel_templates");
  if (num_channels==0) return TRUE;

  ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);

  mainw->write_failed=FALSE;

  for (i=0; i<num_channels; i++) {
    if (weed_plant_has_leaf(ctmpls[i],"host_width")||weed_plant_has_leaf(ctmpls[i],"host_height")||
        (!wrote_hashname&&weed_plant_has_leaf(filter,"host_fps"))) {
      if (!wrote_hashname) {
        hashname=make_weed_hashname(idx,TRUE,FALSE);
        vlen=strlen(hashname);
        lives_write_le_buffered(fd,&vlen,4,TRUE);
        lives_write_buffered(fd,hashname,vlen,TRUE);
        lives_free(hashname);
        wrote_hashname=TRUE;
        if (weed_plant_has_leaf(filter,"host_fps")) {
          int j=-1;
          lives_write_le_buffered(fd,&j,4,TRUE);
          weed_leaf_serialise(fd,filter,"host_fps",FALSE,NULL);
        }
      }

      lives_write_le_buffered(fd,&i,4,TRUE);
      if (weed_plant_has_leaf(ctmpls[i],"host_width")) weed_leaf_serialise(fd,ctmpls[i],"host_width",FALSE,NULL);
      else weed_leaf_serialise(fd,ctmpls[i],"width",FALSE,NULL);
      if (weed_plant_has_leaf(ctmpls[i],"host_height")) weed_leaf_serialise(fd,ctmpls[i],"host_height",FALSE,NULL);
      else weed_leaf_serialise(fd,ctmpls[i],"height",FALSE,NULL);
    }
  }
  if (wrote_hashname) lives_write_buffered(fd,"\n",1,TRUE);

  if (mainw->write_failed) {
    return FALSE;
  }
  return TRUE;
}




boolean read_generator_sizes(int fd) {
  weed_plant_t *filter,**ctmpls;
  ssize_t bytes;
  size_t vlen;

  char *buf;
  char *tmp;

  boolean found=FALSE;

  int vleni,vlenz;
  int i,error;
  int num_chans=0;
  int cnum;

  mainw->read_failed=FALSE;

  while (1) {
    if (lives_read_le_buffered(fd,&vleni,4,TRUE)<4) {
      // we are allowed to EOF here
      mainw->read_failed=FALSE;
      break;
    }

    // some files erroneously used a vlen of 8
    if (lives_read_le_buffered(fd,&vlenz,4,TRUE)<4) {
      // we are allowed to EOF here
      mainw->read_failed=FALSE;
      break;
    }

    vlen=(size_t)vleni;

    if (capable->byte_order==LIVES_BIG_ENDIAN&&prefs->bigendbug) {
      if (vleni==0&&vlenz!=0) vlen=(size_t)vlenz;
    } else {
      if (vlenz!=0) {
        if (lives_lseek_buffered_rdonly(fd,-4)<0) {
          return FALSE;
        }
      }
    }

    if (vlen>65535) {
      return FALSE;
    }

    buf=(char *)lives_malloc(vlen+1);

    bytes=lives_read_buffered(fd,buf,vlen,TRUE);
    if (bytes<vlen) {
      break;
    }
    memset((char *)buf+vlen,0,1);

    for (i=0; i<num_weed_filters; i++) {
      if (!strcmp(buf,(tmp=make_weed_hashname(i,TRUE,FALSE)))) {
        lives_free(tmp);
        found=TRUE;
        break;
      }
      lives_free(tmp);
    }
    if (!found) {
      for (i=0; i<num_weed_filters; i++) {
        if (!strcmp(buf,(tmp=make_weed_hashname(i,TRUE,TRUE)))) {
          lives_free(tmp);
          break;
        }
        lives_free(tmp);
      }
    }


    lives_free(buf);
    ctmpls=NULL;

    if (i<num_weed_filters) {
      boolean ready=FALSE;
      filter=weed_filters[i];
      num_chans=weed_leaf_num_elements(filter,"out_channel_templates");
      if (num_chans>0) ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);

      while (!ready) {
        ready=TRUE;
        bytes=lives_read_le_buffered(fd,&cnum,4,TRUE);
        if (bytes<4) {
          break;
        }

        if (cnum<num_chans&&cnum>=0) {
          weed_leaf_deserialise(fd,ctmpls[cnum],"host_width",NULL,FALSE);
          weed_leaf_deserialise(fd,ctmpls[cnum],"host_height",NULL,FALSE);
          if (weed_get_int_value(ctmpls[cnum],"host_width",&error)==0)
            weed_set_int_value(ctmpls[cnum],"host_width",DEF_GEN_WIDTH);
          if (weed_get_int_value(ctmpls[cnum],"host_height",&error)==0)
            weed_set_int_value(ctmpls[cnum],"host_height",DEF_GEN_HEIGHT);
        } else if (cnum==-1) {
          weed_leaf_deserialise(fd,filter,"host_fps",NULL,FALSE);
          ready=FALSE;
        }
      }
    }
    if (ctmpls!=NULL) lives_free(ctmpls);

    if (mainw->read_failed) {
      break;
    }
    buf=(char *)lives_malloc(strlen("\n"));
    lives_read_buffered(fd,buf,strlen("\n"),TRUE);
    lives_free(buf);

    if (mainw->read_failed) {
      break;
    }
  }

  if (mainw->read_failed) return FALSE;
  return TRUE;
}



void reset_frame_and_clip_index(void) {
  if (mainw->clip_index==NULL) {
    mainw->clip_index=(int *)lives_malloc(sizint);
    mainw->clip_index[0]=-1;
  }
  if (mainw->frame_index==NULL) {
    mainw->frame_index=(int *)lives_malloc(sizint);
    mainw->frame_index[0]=0;
  }
}


// key/mode parameter defaults

boolean read_key_defaults(int fd, int nparams, int key, int mode, int ver) {
  // read default param values for key/mode from file

  // if key < 0 we just read past the bytes in the file

  // return FALSE on EOF

  int i,j,nvals,nigns;
  int idx;
  ssize_t bytes;
  weed_plant_t *filter;
  int xnparams=0,maxparams=nparams;
  weed_plant_t **key_defs;
  weed_timecode_t tc;

  boolean ret=FALSE;

  mainw->read_failed=FALSE;

  if (key>=0) {
    idx=key_to_fx[key][mode];
    filter=weed_filters[idx];
    xnparams=num_in_params(filter,FALSE,FALSE);
  }

  if (xnparams>maxparams) maxparams=xnparams;

  key_defs=(weed_plant_t **)lives_malloc(maxparams*sizeof(weed_plant_t *));
  for (i=0; i<maxparams; i++) {
    key_defs[i]=NULL;
  }

  for (i=0; i<nparams; i++) {
    if (key<0||i<xnparams) {
      key_defs[i]=weed_plant_new(WEED_PLANT_PARAMETER);
    }
    if (ver>1) {
      // for future - read nvals
      bytes=lives_read_le_buffered(fd,&nvals,4,TRUE);
      if (bytes<4) {
        goto err123;
      }
      bytes=lives_read_le_buffered(fd,&tc,8,TRUE);
      if (bytes<sizeof(weed_timecode_t)) {
        goto err123;
      }
      // read n ints (booleans)
      bytes=lives_read_le_buffered(fd,&nigns,4,TRUE);
      if (bytes<4) {
        goto err123;
      }
      if (nigns>0) {
        int *igns=(int *)lives_malloc(nigns*4);
        for (j=0; j<nigns; j++) {
          bytes=lives_read_le_buffered(fd,&igns[j],4,TRUE);
          if (bytes<4) {
            goto err123;
          }
        }
        lives_free(igns);
      }
    }

    if (weed_leaf_deserialise(fd,key_defs[i],"value",NULL,FALSE)<0) goto err123;

    if (ver>1) {
      for (j=0; j<nvals; j++) {
        // for future - read timecodes
        weed_plant_t *plant=weed_plant_new(WEED_PLANT_PARAMETER);
        bytes=lives_read_le_buffered(fd,&tc,8,TRUE);
        if (bytes<8) {
          goto err123;
        }
        // read n ints (booleans)
        bytes=lives_read_le_buffered(fd,&nigns,4,TRUE);
        if (bytes<4) {
          goto err123;
        }
        if (nigns>0) {
          int *igns=(int *)lives_malloc(nigns*4);
          for (j=0; j<nigns; j++) {
            bytes=lives_read_le_buffered(fd,&igns[j],4,TRUE);
            if (bytes<4) {
              goto err123;
            }
          }
          // discard excess values
          ret=weed_leaf_deserialise(fd,plant,"value",NULL,FALSE);
          weed_plant_free(plant);
          if (ret<0) goto err123;
        }
      }
    }
  }

  if (key>=0) key_defaults[key][mode]=key_defs;

err123:
  if (key<0) {
    for (i=0; i<nparams; i++) {
      if (key_defs[i]!=NULL) weed_plant_free(key_defs[i]);
    }
    lives_free(key_defs);
  }

  if (ret<0||mainw->read_failed) {
    return FALSE;
  }

  return TRUE;
}




void apply_key_defaults(weed_plant_t *inst, int key, int mode) {
  // apply key/mode param defaults to a filter instance
  int error;
  int nparams;
  weed_plant_t **defs;
  weed_plant_t **params;

  weed_plant_t *filter=weed_instance_get_filter(inst,TRUE);

  register int i,j=0;

  nparams=num_in_params(filter,FALSE,FALSE);

  if (nparams==0) return;
  defs=key_defaults[key][mode];

  if (defs==NULL) return;

  do {
    params=weed_get_plantptr_array(inst,"in_parameters",&error);
    nparams=num_in_params(inst,FALSE,FALSE);

    for (i=0; i<nparams; i++) {
      if (!is_hidden_param(inst,i))
        weed_leaf_copy(params[i],"value",defs[j],"value");
      j++;
    }
    lives_free(params);
  } while (weed_plant_has_leaf(inst,"host_next_instance")&&(inst=weed_get_plantptr_value(inst,"host_next_instance",&error))!=NULL);


}



void write_key_defaults(int fd, int key, int mode) {
  // save key/mode param defaults to file
  // caller should check for write errors

  weed_plant_t *filter;
  weed_plant_t **key_defs;
  int i,nparams=0;

  if ((key_defs=key_defaults[key][mode])==NULL) {
    lives_write_le_buffered(fd,&nparams,4,TRUE);
    return;
  }

  filter=weed_filters[key_to_fx[key][mode]];
  nparams=num_in_params(filter,FALSE,FALSE);
  lives_write_le_buffered(fd,&nparams,4,TRUE);

  for (i=0; i<nparams; i++) {
    if (mainw->write_failed) break;
    weed_leaf_serialise(fd,key_defs[i],"value",FALSE,NULL);
  }

}



void free_key_defaults(int key, int mode) {
  // free key/mode param defaults
  weed_plant_t *filter;
  weed_plant_t **key_defs;
  int i=0;
  int nparams;

  if (key>=FX_KEYS_MAX_VIRTUAL||mode>=prefs->max_modes_per_key) return;

  key_defs=key_defaults[key][mode];

  if (key_defs==NULL) return;

  filter=weed_filters[key_to_fx[key][mode]];
  nparams=num_in_params(filter,FALSE,FALSE);

  while (i<nparams) {
    if (key_defs[i]!=NULL) weed_plant_free(key_defs[i]);
    i++;
  }
  lives_free(key_defaults[key][mode]);
  key_defaults[key][mode]=NULL;
}



void set_key_defaults(weed_plant_t *inst, int key, int mode) {
  // copy key/mode param defaults from an instance
  weed_plant_t **key_defs,**params;
  weed_plant_t *filter=weed_instance_get_filter(inst,TRUE);
  int i=0,error;
  int nparams,poffset=0;

  if (key_defaults[key][mode]!=NULL) free_key_defaults(key,mode);

  nparams=num_in_params(filter,FALSE,FALSE);

  if (nparams==0) return;

  key_defs=(weed_plant_t **)lives_malloc(nparams*sizeof(weed_plant_t *));

  do {
    nparams=num_in_params(inst,FALSE,FALSE);

    if (nparams>0) {
      params=weed_get_plantptr_array(inst,"in_parameters",&error);

      while (i<nparams+poffset) {
        key_defs[i]=weed_plant_new(WEED_PLANT_PARAMETER);
        weed_leaf_copy(key_defs[i],"value",params[i-poffset],"value");
        i++;
      }

      lives_free(params);
    }
    poffset+=nparams;

  } while (weed_plant_has_leaf(inst,"host_next_instance")&&(inst=weed_get_plantptr_value(inst,"host_next_instance",&error))!=NULL);

  key_defaults[key][mode]=key_defs;
}



boolean has_key_defaults(void) {
  // check if any key/mode has default parameters set
  int i,j;
  for (i=0; i<prefs->rte_keys_virtual; i++) {
    for (j=0; j<prefs->max_modes_per_key; j++) {
      if (key_defaults[i][j]!=NULL) return TRUE;
    }
  }
  return FALSE;
}

