/* libvisual plugin for Weed
   authors: Salsaman (G. Finch) <salsaman_lives@gmail.com>
            Dennis Smit  <synap@yourbase.nl>
            Duilio J. Protti <dprotti@users.sourceforge.net>

   Released under the Lesser Gnu Public License (LGPL) 3 or later
   See www.gnu.org for details

  (c) 2004, project authors

  (c) 2004 - 2019 salsaman

*/

// WARNING ! Only "jack" and "host audio" inputs are multi threaded
///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_ALPHA_SORT

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <libvisual/lv_actor.h>
#include <libvisual/lv_input.h>
#include <libvisual/lv_libvisual.h>

#include <pthread.h>

static int libvis_host_audio_callback(VisInput *, VisAudio *, void *user_data);

typedef struct {
  VisVideo *video;
  VisActor *actor;
  VisInput *input;
  short *audio;
  size_t audio_frames;
  pthread_mutex_t pcm_mutex;
  int instance;
} weed_libvis_t;

static int instances;
static char *old_input;
static VisInput *old_visinput;

///////////////////////////////////////////////////////////////

static weed_error_t libvis_init(weed_plant_t *inst) {
  weed_libvis_t *libvis = NULL;
  weed_plant_t *out_channel, *filter;
  weed_plant_t *param;

  char *filter_name;
  char *filtname;
  char *ainput = NULL;

  int palette, listener;

  param = weed_get_plantptr_value(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  listener = weed_get_int_value(param, WEED_LEAF_VALUE, NULL);

  filter = weed_get_plantptr_value(inst, WEED_LEAF_FILTER_CLASS, NULL);

  switch (listener) {
  case 1:
    ainput = "alsa";
    break;
  case 2:
    ainput = "esd";
    break;
  case 3:
    ainput = "jack";
    break;
  case 4:
    ainput = "mplayer";
    break;
  case 5:
    ainput = "auto";
    break;
  default:
    ainput = NULL; // no audio input
    break;
  }

  if (ainput != NULL && instances && strcmp(ainput, "jack")) return WEED_ERROR_TOO_MANY_INSTANCES;

  if (libvis == NULL) libvis = (weed_libvis_t *)weed_malloc(sizeof(weed_libvis_t));
  if (libvis == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  if (old_input == NULL || ainput == NULL || strcmp(ainput, old_input) || instances > 0) {
    if (old_input != NULL) {
      if (instances == 0) {
        visual_object_destroy(VISUAL_OBJECT(old_visinput));
        old_visinput = NULL;
      }
      free(old_input);
      old_input = NULL;
    }
    if (ainput != NULL) {
      old_visinput = libvis->input = visual_input_new(!strcmp(ainput, "auto") ? NULL : ainput);
      old_input = strdup(ainput);
    }
  } else libvis->input = old_visinput;

  // host supplied audio buffers

  if (libvis->input == NULL) { // wish this worked
    weed_free(libvis);
    return WEED_ERROR_PLUGIN_INVALID;
  }

  libvis->video = visual_video_new();

  out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  palette = weed_get_int_value(out_channel, WEED_LEAF_CURRENT_PALETTE, NULL);

  if (palette == WEED_PALETTE_RGB24) visual_video_set_depth(libvis->video, VISUAL_VIDEO_DEPTH_24BIT);
  else visual_video_set_depth(libvis->video, VISUAL_VIDEO_DEPTH_32BIT);

  visual_video_set_dimension(libvis->video, weed_get_int_value(out_channel, WEED_LEAF_WIDTH, NULL),
                             weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL));

  filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
  if (!strncmp(filter_name, "libvisual: ", 11)) filtname = filter_name + 11;
  else filtname = filter_name;
  libvis->actor = visual_actor_new(filtname);
  weed_free(filter_name);

  visual_actor_realize(libvis->actor);
  visual_actor_set_video(libvis->actor, libvis->video);
  visual_actor_video_negotiate(libvis->actor, 0, FALSE, FALSE);
  visual_input_realize(libvis->input);

  libvis->audio = (short *)weed_calloc(512, sizeof(short));
  libvis->audio_frames = 0;
  libvis->instance = instances;

  weed_set_voidptr_value(inst, "plugin_internal", (void *)libvis);

  instances++;

  if (!strcmp(ainput, "auto")) {
    pthread_mutex_init(&libvis->pcm_mutex, NULL);
    visual_input_set_callback(libvis->input, libvis_host_audio_callback, (void *)libvis);
  }

  return WEED_SUCCESS;
}


static weed_error_t libvis_deinit(weed_plant_t *inst) {
  weed_libvis_t *libvis;

  if (weed_plant_has_leaf(inst, "plugin_internal")) {
    libvis = (weed_libvis_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
    if (libvis->instance > 0) {
      visual_object_destroy(VISUAL_OBJECT(libvis->input));
    }
    if (libvis->video != NULL) visual_object_free(VISUAL_OBJECT(libvis->video));
    if (libvis->actor != NULL) visual_object_destroy(VISUAL_OBJECT(libvis->actor));
    if (libvis->audio != NULL) weed_free(libvis->audio);
    weed_free(libvis);
    libvis = NULL;
    weed_set_voidptr_value(inst, "plugin_internal", libvis);
  }
  if (--instances < 0) instances = 0;

  return WEED_SUCCESS;
}


static void store_audio(weed_libvis_t *libvis, weed_plant_t *in_channel) {
  // convert float audio to s16le, append to libvis->audio
  register int i;

  if (in_channel != NULL) {
    int adlen = weed_get_int_value(in_channel, WEED_LEAF_AUDIO_DATA_LENGTH, NULL);
    float **adata = (float **)weed_get_voidptr_array(in_channel, WEED_LEAF_AUDIO_DATA, NULL);
    if (adlen > 0 && adata != NULL) {
      size_t sdf, offset = 0, overflow;
      int achans = weed_get_int_value(in_channel, WEED_LEAF_AUDIO_CHANNELS, NULL);
      pthread_mutex_lock(&libvis->pcm_mutex);

      /// we want to send 256 samples of left, followed by 256 samples of right / monoe
      /// so we use the buffer like two sliding windows
      sdf = libvis->audio_frames;
      overflow = sdf + adlen - 256;
      if (overflow >= sdf) {
        /// adlen >= 256, write the last 256 samples from buffer
        libvis->audio_frames = 0;
        offset = adlen - 256;
        adlen = 256;
      } else if (overflow > 0) {
        /// make space by shifting the old audio
        weed_memmove((void *)libvis->audio, (void *)libvis->audio + overflow  * sizeof(short), (sdf - overflow) * sizeof(short));
        libvis->audio_frames -= overflow;
      }
      for (i = 0; i < adlen; i ++) {
        libvis->audio[libvis->audio_frames + i] = 32767. * adata[0][offset + i];
        if (achans == 2)
          libvis->audio[libvis->audio_frames + 256 + i] = 32767. * adata[1][offset + i];
        else
          libvis->audio[libvis->audio_frames + 256 + i] = 32767. * adata[0][offset + i];
      }
      libvis->audio_frames += adlen;;
      libvis->audio_frames += adlen * achans;
      pthread_mutex_unlock(&libvis->pcm_mutex);
      weed_free(adata);
    }
  }
}


static weed_error_t libvis_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_libvis_t *libvis = (weed_libvis_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  void *pixel_data = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  if (in_channel != NULL) store_audio(libvis, in_channel);

  visual_input_run(libvis->input);
  visual_video_set_buffer(libvis->video, pixel_data);
  visual_actor_run(libvis->actor, libvis->input->audio);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  dlink_list_t *list = NULL;
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_RGBA32, WEED_PALETTE_END};
  weed_plant_t *out_chantmpls[2];
  char *name = NULL;
  char fullname[PATH_MAX];
  weed_plant_t *filter_class;
  weed_plant_t *in_params[2];
  const char *listeners[] = {"None", "Alsa", "ESD", "Jack", "Mplayer", "Auto", NULL};

  DIR *curvdir;

  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("In audio", WEED_CHANNEL_OPTIONAL), NULL};

  char *lpp = getenv("VISUAL_PLUGIN_PATH");

  char *vdir;

  int filter_flags = 0;

  if (lpp == NULL) return NULL;

  weed_set_string_value(plugin_info, WEED_LEAF_PACKAGE_NAME, "libvisual");

  // set hints for host
  weed_set_int_value(in_chantmpls[0], WEED_LEAF_AUDIO_CHANNELS, 2);
  weed_set_int_value(in_chantmpls[0], WEED_LEAF_AUDIO_RATE, 44100);
  weed_set_boolean_value(in_chantmpls[0], WEED_LEAF_AUDIO_DATA_LENGTH, 512);

  instances = 0;
  old_input = NULL;
  old_visinput = NULL;

  if (VISUAL_PLUGIN_API_VERSION < 2) return NULL;
  visual_log_set_verboseness(VISUAL_LOG_VERBOSENESS_NONE);

  if (visual_init(NULL, NULL) < 0) {
    fprintf(stderr, "Libvis : Unable to init libvisual plugins\n");
    return NULL;
  }

  vdir = strtok(lpp, ":");

  // add lpp paths
  while (vdir != NULL) {
    if (!strlen(vdir)) continue;

    curvdir = opendir(vdir);
    if (curvdir == NULL) {
      continue;
    }

    visual_init_path_add(vdir);
    closedir(curvdir);
    vdir = strtok(NULL, ":");
  }

  in_params[1] = NULL;
  out_chantmpls[1] = NULL;

  while ((name = (char *)visual_actor_get_next_by_name_nogl(name)) != NULL) {
    snprintf(fullname, PATH_MAX, "%s", name);
    in_params[0] = weed_string_list_init("listener", "Audio _listener", 5, listeners);
    weed_set_int_value(in_params[0], WEED_LEAF_FLAGS, WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
    out_chantmpls[0] = weed_channel_template_init("out channel 0", 0);
    filter_class = weed_filter_class_init(fullname, "Team libvisual", 1, filter_flags, palette_list, libvis_init, libvis_process,
                                          libvis_deinit,
                                          in_chantmpls, out_chantmpls, in_params, NULL);
    weed_set_double_value(filter_class, WEED_LEAF_PREFERRED_FPS, 50.); // set reasonable default fps

    list = add_to_list_sorted(list, filter_class, (const char *)name);
  }

  add_filters_from_list(plugin_info, list);

  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


///////////// callback so host can supply its own audio sammples //////////

static int libvis_host_audio_callback(VisInput *input, VisAudio *audio, void *user_data) {
  // audio_data is 16bit signed, little endian
  // two channels non-interlaced, ideally 512 samples in length
  // a rate of 44100Hz is recommended

  // return -1 on failure, 0 on success

  int alen;

  weed_libvis_t *libvis = (weed_libvis_t *)user_data;

  alen = libvis->audio_frames * 4;

  if (alen > 2048) alen = 2048;

  if (libvis->audio != NULL) {
    pthread_mutex_lock(&libvis->pcm_mutex);
    weed_memcpy(audio->plugpcm, libvis->audio, alen);
    weed_free(libvis->audio);
    libvis->audio = NULL;
    libvis->audio_frames = 0;
    pthread_mutex_unlock(&libvis->pcm_mutex);
  }
  return 0;
}
