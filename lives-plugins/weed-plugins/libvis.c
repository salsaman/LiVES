/* libvisual plugin for Weed
   authors: Salsaman (G. Finch) <salsaman@xs4all.nl>
            Dennis Smit  <synap@yourbase.nl>
            Duilio J. Protti <dprotti@users.sourceforge.net>

   Released under the Lesser Gnu Public License (LGPL) 3 or later
   See www.gnu.org for details

 (c) 2004, project authors
*/


// WARNING ! Only "jack" and "host audio" inputs are multi threaded


#include "../../libweed/weed.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]={100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=2; // version of this package

//////////////////////////////////////////////////////////////////

#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <libvisual/lv_actor.h>
#include <libvisual/lv_input.h>
#include <libvisual/lv_libvisual.h>

static int libvis_host_audio_callback (VisInput *, VisAudio *, void *user_data);

typedef struct {
  VisVideo *video;
  VisActor *actor;
  VisInput *input;
  void *audio_data;
  int instance;
} weed_libvis_t;

static int instances;
static char *old_input;
static VisInput *old_visinput;

///////////////////////////////////////////////////////////////

int libvis_init (weed_plant_t *inst) {
  weed_libvis_t *libvis=NULL;
  weed_plant_t *out_channel,*filter;
  int error;
  char *filter_name;
  char *filtname;
  weed_plant_t *param;
  int palette,listener;
  weed_plant_t *pinfo,*hinfo;
  char *hap;
  char *ainput=NULL;

  param=weed_get_plantptr_value(inst,"in_parameters",&error);
  listener=weed_get_int_value(param,"value",&error);

  filter=weed_get_plantptr_value(inst,"filter_class",&error);

  switch (listener) {
  case 0:
    ainput=NULL; // no audio input
    break;
  case 1:
    ainput="alsa";
    break;
  case 2:
    ainput="esd";
    break;
  case 3:
    ainput="jack";
    break;
  case 4:
    ainput="mplayer";
    break;
  case 5:
    pinfo=weed_get_plantptr_value(filter,"plugin_info",&error);
    hinfo=weed_get_plantptr_value(pinfo,"host_info",&error);
    if (weed_plant_has_leaf(hinfo,"host_audio_player")) {
      hap=weed_get_string_value(hinfo,"host_audio_player",&error);
      if (!strcmp(hap,"sox")||!strcmp(hap,"mplayer")) {
	ainput="alsa";
      }
      else {
	ainput="jack";
      }
      weed_free(hap);
    }
    else {
      ainput="jack";
    }
    break;
  default:
    // not implemented yet
    libvis=(weed_libvis_t *)weed_malloc (sizeof(weed_libvis_t));
    if (libvis==NULL) return WEED_ERROR_MEMORY_ALLOCATION;
    visual_input_set_callback (libvis->input, libvis_host_audio_callback, (void *)libvis);
  }

  if (ainput!=NULL&&instances&&strcmp(ainput,"jack")) return WEED_ERROR_TOO_MANY_INSTANCES;

  if (libvis==NULL) libvis=(weed_libvis_t *)weed_malloc (sizeof(weed_libvis_t));
  if (libvis==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  if (old_input==NULL||ainput==NULL||strcmp(ainput,old_input)||instances>0) {
    if (old_input!=NULL) {
      if (instances==0) {
	visual_object_destroy (VISUAL_OBJECT (old_visinput));
	old_visinput=NULL;
      }
      free(old_input);
      old_input=NULL;
    }
    if (ainput!=NULL) {
      old_visinput=libvis->input = visual_input_new (ainput);
      old_input=strdup(ainput);
    }
  }
  else libvis->input=old_visinput;


  // host supplied audio buffers

  if (libvis->input==NULL) { // wish this worked
    weed_free (libvis);
    return WEED_ERROR_INIT_ERROR;
  }

  libvis->video = visual_video_new();

  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  palette=weed_get_int_value(out_channel,"current_palette",&error);

  if (palette==WEED_PALETTE_RGB24) visual_video_set_depth (libvis->video, VISUAL_VIDEO_DEPTH_24BIT);
  else visual_video_set_depth (libvis->video, VISUAL_VIDEO_DEPTH_32BIT);

  visual_video_set_dimension (libvis->video, weed_get_int_value(out_channel,"width",&error), weed_get_int_value(out_channel,"height",&error));

  filter_name=weed_get_string_value(filter,"name",&error);
  if (!strncmp(filter_name,"libvisual: ",11)) filtname=filter_name+11;
  else filtname=filter_name;
  libvis->actor = visual_actor_new (filtname);
  weed_free(filter_name);

  visual_actor_realize (libvis->actor);
  visual_actor_set_video (libvis->actor, libvis->video);
  visual_actor_video_negotiate (libvis->actor, 0, FALSE, FALSE);
  visual_input_realize (libvis->input);
  libvis->audio_data=NULL;
  libvis->instance=instances;

  weed_set_voidptr_value(inst,"plugin_internal",(void *)libvis);

  instances++;

  return WEED_NO_ERROR;
}


int libvis_deinit (weed_plant_t *inst) {
  weed_libvis_t *libvis;
  int error;

  if (weed_plant_has_leaf(inst,"plugin_internal")) {
    libvis=(weed_libvis_t *)weed_get_voidptr_value(inst,"plugin_internal",&error);
    if (libvis->instance>0) {
      visual_object_destroy (VISUAL_OBJECT (libvis->input));
    }
    if (libvis->video!=NULL) visual_object_free (VISUAL_OBJECT (libvis->video));
    if (libvis->actor!=NULL) visual_object_destroy (VISUAL_OBJECT (libvis->actor));
    weed_free (libvis);
    libvis=NULL;
    weed_set_voidptr_value(inst,"plugin_internal",libvis);
  }
  if (--instances<0) instances=0;
  
  return WEED_NO_ERROR;
}


int libvis_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_libvis_t *libvis=(weed_libvis_t *)weed_get_voidptr_value(inst,"plugin_internal",&error);
  weed_plant_t *channel=weed_get_plantptr_value(inst,"out_channels",&error);
  void *pixel_data=weed_get_voidptr_value(channel,"pixel_data",&error);

  visual_input_run (libvis->input);
  visual_video_set_buffer (libvis->video, pixel_data);
  visual_actor_run (libvis->actor, libvis->input->audio);
  return WEED_NO_ERROR;
}

weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_END};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",0,palette_list),NULL};
    char *name=NULL;
    char fullname[256];
    weed_plant_t *filter_class;
    weed_plant_t *in_params[2];
    char *listeners[]={"None","Alsa","ESD","Jack","Mplayer","Auto",NULL};

    instances=0;
    old_input=NULL;
    old_visinput=NULL;

    if (VISUAL_PLUGIN_API_VERSION<2) return NULL;
    visual_log_set_verboseness (VISUAL_LOG_VERBOSENESS_NONE);

    if (visual_init (NULL,NULL)<0) {
      fprintf (stderr, "Libvis : Unable to init libvisual plugins\n");
      return NULL;
    }

    in_params[1]=NULL;

    while ((name=(char *)visual_actor_get_next_by_name_nogl (name))!=NULL) {
      snprintf(fullname,256,"libvisual: %s",name);
      in_params[0]=weed_string_list_init("listener","Audio _listener",5,listeners);
      weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

      filter_class=weed_filter_class_init(fullname,"Team libvisual",1,0,&libvis_init,&libvis_process,&libvis_deinit,NULL,weed_clone_plants(out_chantmpls),in_params,NULL);
      weed_set_double_value(filter_class,"target_fps",50.); // set reasonable default fps

      weed_plugin_info_add_filter_class (plugin_info,filter_class);
    }
    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}


///////////// callback so host can supply its own audio sammples //////////

static int libvis_host_audio_callback (VisInput *input, VisAudio *audio, void *user_data) {
  // audio_data is 16bit signed, little endian
  // two channels non-interlaced, 512 samples in length
  // a rate of 44100Hz is recommended

  // since we have no in channels, we have to use the audio_data of the 
  // output channel (!)

  // return -1 on failure, 0 on success

  weed_libvis_t *libvis=(weed_libvis_t *)user_data;
  if (libvis->audio_data!=NULL) {
    weed_memcpy (audio->plugpcm,libvis->audio_data,2048);
  }
  return 0;
}
