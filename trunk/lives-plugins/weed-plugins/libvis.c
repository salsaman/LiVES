/* libvisual plugin for Weed
   authors: Salsaman (G. Finch) <salsaman@xs4all.nl,salsaman@gmail.com>
            Dennis Smit  <synap@yourbase.nl>
            Duilio J. Protti <dprotti@users.sourceforge.net>

   Released under the Lesser Gnu Public License (LGPL) 3 or later
   See www.gnu.org for details

 (c) 2004, project authors
*/


// WARNING ! Only "jack" and "host audio" inputs are multi threaded


#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#endif


///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]= {131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=2; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <libvisual/lv_actor.h>
#include <libvisual/lv_input.h>
#include <libvisual/lv_libvisual.h>

#include <pthread.h>

static int libvis_host_audio_callback(VisInput *, VisAudio *, void *user_data);

typedef struct {
  VisVideo *video;
  VisActor *actor;
  VisInput *input;
  void *audio;
  size_t audio_frames;
  pthread_mutex_t pcm_mutex;
  int instance;
} weed_libvis_t;

static int instances;
static char *old_input;
static VisInput *old_visinput;

///////////////////////////////////////////////////////////////

int libvis_init(weed_plant_t *inst) {
  weed_libvis_t *libvis=NULL;
  weed_plant_t *out_channel,*filter;

  int error;

  char *filter_name;
  char *filtname;

  weed_plant_t *param;

  int palette,listener;

  char *ainput=NULL;

  param=weed_get_plantptr_value(inst,"in_parameters",&error);
  listener=weed_get_int_value(param,"value",&error);

  filter=weed_get_plantptr_value(inst,"filter_class",&error);

  switch (listener) {
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
    ainput="auto";
    break;
  default:
    ainput=NULL; // no audio input
    break;
  }

  if (ainput!=NULL&&instances&&strcmp(ainput,"jack")) return WEED_ERROR_TOO_MANY_INSTANCES;

  if (libvis==NULL) libvis=(weed_libvis_t *)weed_malloc(sizeof(weed_libvis_t));
  if (libvis==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  if (old_input==NULL||ainput==NULL||strcmp(ainput,old_input)||instances>0) {
    if (old_input!=NULL) {
      if (instances==0) {
        visual_object_destroy(VISUAL_OBJECT(old_visinput));
        old_visinput=NULL;
      }
      free(old_input);
      old_input=NULL;
    }
    if (ainput!=NULL) {
      old_visinput=libvis->input = visual_input_new(!strcmp(ainput,"auto")?NULL:ainput);
      old_input=strdup(ainput);
    }
  } else libvis->input=old_visinput;


  // host supplied audio buffers

  if (libvis->input==NULL) { // wish this worked
    weed_free(libvis);
    return WEED_ERROR_INIT_ERROR;
  }


  libvis->video = visual_video_new();

  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  palette=weed_get_int_value(out_channel,"current_palette",&error);

  if (palette==WEED_PALETTE_RGB24) visual_video_set_depth(libvis->video, VISUAL_VIDEO_DEPTH_24BIT);
  else visual_video_set_depth(libvis->video, VISUAL_VIDEO_DEPTH_32BIT);

  visual_video_set_dimension(libvis->video, weed_get_int_value(out_channel,"width",&error), weed_get_int_value(out_channel,"height",&error));

  filter_name=weed_get_string_value(filter,"name",&error);
  if (!strncmp(filter_name,"libvisual: ",11)) filtname=filter_name+11;
  else filtname=filter_name;
  libvis->actor = visual_actor_new(filtname);
  weed_free(filter_name);

  visual_actor_realize(libvis->actor);
  visual_actor_set_video(libvis->actor, libvis->video);
  visual_actor_video_negotiate(libvis->actor, 0, FALSE, FALSE);
  visual_input_realize(libvis->input);

  libvis->audio=NULL;
  libvis->audio_frames=0;
  libvis->instance=instances;

  weed_set_voidptr_value(inst,"plugin_internal",(void *)libvis);

  instances++;

  if (!strcmp(ainput,"auto")) {
    pthread_mutex_init(&libvis->pcm_mutex,NULL);
    visual_input_set_callback(libvis->input, libvis_host_audio_callback, (void *)libvis);
  }

  return WEED_NO_ERROR;
}


int libvis_deinit(weed_plant_t *inst) {
  weed_libvis_t *libvis;
  int error;

  if (weed_plant_has_leaf(inst,"plugin_internal")) {
    libvis=(weed_libvis_t *)weed_get_voidptr_value(inst,"plugin_internal",&error);
    if (libvis->instance>0) {
      visual_object_destroy(VISUAL_OBJECT(libvis->input));
    }
    if (libvis->video!=NULL) visual_object_free(VISUAL_OBJECT(libvis->video));
    if (libvis->actor!=NULL) visual_object_destroy(VISUAL_OBJECT(libvis->actor));
    if (libvis->audio!=NULL) weed_free(libvis->audio);
    weed_free(libvis);
    libvis=NULL;
    weed_set_voidptr_value(inst,"plugin_internal",libvis);
  }
  if (--instances<0) instances=0;

  return WEED_NO_ERROR;
}



static void store_audio(weed_libvis_t *libvis, weed_plant_t *in_channel) {
  // convert float audio to s16le, append to libvis->audio

  int error;

  int adlen=weed_get_int_value(in_channel,"audio_data_length",&error);
  float *adata=(float *)weed_get_voidptr_value(in_channel,"audio_data",&error),*oadata=adata;

  register int i,j;

  if (adlen>0&&adata!=NULL) {
    short *aud_data;
    int ainter=weed_get_boolean_value(in_channel,"audio_interleaf",&error);
    int achans=weed_get_int_value(in_channel,"audio_channels",&error);

    pthread_mutex_lock(&libvis->pcm_mutex);
    aud_data=(short *)weed_malloc((adlen+libvis->audio_frames)*4);

    if (libvis->audio!=NULL) {
      weed_memcpy(aud_data,libvis->audio,libvis->audio_frames*4);
      weed_free(libvis->audio);
    }

    for (j=0; j<adlen; j++) {
      if (ainter==WEED_TRUE) {
        // interlaced
        for (i=0; i<2; i++) {
          aud_data[libvis->audio_frames*2+i]=32767.*adata[i];
        }
        adata+=achans;
      } else {
        // non-interlaced
        for (i=0; i<2; i++) {
          aud_data[libvis->audio_frames*2+i]=32767.*adata[j];
          adata+=adlen;
        }
        adata=oadata;
      }
      libvis->audio_frames++;
    }
    libvis->audio=aud_data;
    pthread_mutex_unlock(&libvis->pcm_mutex);
  }
}



int libvis_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_libvis_t *libvis=(weed_libvis_t *)weed_get_voidptr_value(inst,"plugin_internal",&error);
  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  void *pixel_data=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  if (in_channel!=NULL) store_audio(libvis,in_channel);

  visual_input_run(libvis->input);
  visual_video_set_buffer(libvis->video, pixel_data);
  visual_actor_run(libvis->actor, libvis->input->audio);
  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_END};
    weed_plant_t *out_chantmpls[2];
    char *name=NULL;
    char fullname[256];
    weed_plant_t *filter_class;
    weed_plant_t *in_params[2];
    const char *listeners[]= {"None","Alsa","ESD","Jack","Mplayer","Auto",NULL};

    weed_plant_t *in_chantmpls[]= {weed_audio_channel_template_init("In audio",0),NULL};

    // set hints for host
    weed_set_int_value(in_chantmpls[0],"audio_channels",2);
    weed_set_int_value(in_chantmpls[0],"audio_rate",44100);
    weed_set_boolean_value(in_chantmpls[0],"audio_interleaf",WEED_FALSE);
    weed_set_boolean_value(in_chantmpls[0],"audio_data_length",512);
    weed_set_boolean_value(in_chantmpls[0],"optional",WEED_TRUE);

    instances=0;
    old_input=NULL;
    old_visinput=NULL;

    if (VISUAL_PLUGIN_API_VERSION<2) return NULL;
    visual_log_set_verboseness(VISUAL_LOG_VERBOSENESS_NONE);

    if (visual_init(NULL,NULL)<0) {
      fprintf(stderr, "Libvis : Unable to init libvisual plugins\n");
      return NULL;
    }

    in_params[1]=NULL;
    out_chantmpls[1]=NULL;

    while ((name=(char *)visual_actor_get_next_by_name_nogl(name))!=NULL) {
      snprintf(fullname,256,"libvisual: %s",name);
      in_params[0]=weed_string_list_init("listener","Audio _listener",5,listeners);
      weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
      out_chantmpls[0]=weed_channel_template_init("out channel 0",0,palette_list);
      filter_class=weed_filter_class_init(fullname,"Team libvisual",1,0,&libvis_init,&libvis_process,&libvis_deinit,
                                          in_chantmpls,out_chantmpls,in_params,NULL);
      weed_set_double_value(filter_class,"target_fps",50.); // set reasonable default fps

      weed_plugin_info_add_filter_class(plugin_info,filter_class);
    }
    weed_set_int_value(plugin_info,"version",package_version);
    //weed_plant_free(out_chantmpls[0]);
  }
  return plugin_info;
}


///////////// callback so host can supply its own audio sammples //////////

static int libvis_host_audio_callback(VisInput *input, VisAudio *audio, void *user_data) {
  // audio_data is 16bit signed, little endian
  // two channels non-interlaced, ideally 512 samples in length
  // a rate of 44100Hz is recommended

  // return -1 on failure, 0 on success

  int alen;

  weed_libvis_t *libvis=(weed_libvis_t *)user_data;

  alen=libvis->audio_frames*4;

  if (alen>2048) alen=2048;

  if (libvis->audio!=NULL) {
    pthread_mutex_lock(&libvis->pcm_mutex);
    weed_memcpy(audio->plugpcm,libvis->audio,alen);
    weed_free(libvis->audio);
    libvis->audio=NULL;
    libvis->audio_frames=0;
    pthread_mutex_unlock(&libvis->pcm_mutex);
  }
  return 0;
}
