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
#define NEED_AUDIO

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <libvisual/libvisual.h>

#include <pthread.h>


#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

#define LIBVIS_ABUF_SIZE 512

static int libvis_host_audio_callback(VisInput *, VisAudio *, void *user_data);

typedef struct {
  VisVideo *video;
  VisActor *actor;
  VisInput *input;
  short *audio;
  off_t audio_frames;
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
  int width, height, rowstride;

  param = weed_get_in_param(inst, 0);
  listener = weed_param_get_value_int(param);

  filter = weed_instance_get_filter(inst);

  switch (listener) {
  case 1:
    ainput = "alsa"; break;
  case 2:
    ainput = "esd"; break;
  case 3:
    ainput = "jack"; break;
  case 4:
    ainput = "mplayer"; break;
  case 5:
    ainput = "auto"; break;
  default:
    ainput = NULL; // no audio input
    break;
  }

  if (ainput && instances && strcmp(ainput, "jack")) return WEED_ERROR_TOO_MANY_INSTANCES;

  if (!libvis) libvis = (weed_libvis_t *)weed_calloc(1, sizeof(weed_libvis_t));
  if (!libvis) return WEED_ERROR_MEMORY_ALLOCATION;

  if (!old_input || !ainput || strcmp(ainput, old_input) || instances > 0) {
    if (old_input) {
      if (instances == 0) {
        visual_object_destroy(VISUAL_OBJECT(old_visinput));
        old_visinput = NULL;
      }
      free(old_input);
      old_input = NULL;
    }
    if (ainput) {
      old_visinput = libvis->input = visual_input_new(!strcmp(ainput, "auto") ? NULL : ainput);
      old_input = strdup(ainput);
    }
  } else libvis->input = old_visinput;

  // host supplied audio buffers

  if (!libvis->input) { // wish this worked
    weed_free(libvis);
    return WEED_ERROR_PLUGIN_INVALID;
  }

  libvis->video = visual_video_new();

  out_channel = weed_get_out_channel(inst, 0);
  palette = weed_channel_get_palette(out_channel);
  rowstride = weed_channel_get_stride(out_channel);
  width = weed_channel_get_width(out_channel);
  height = weed_channel_get_height(out_channel);

  visual_video_set_attributes(libvis->video, width, height, rowstride,
                              palette == WEED_PALETTE_RGB24 ? VISUAL_VIDEO_DEPTH_24BIT
                              : VISUAL_VIDEO_DEPTH_32BIT);

  filter_name = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
  if (!strncmp(filter_name, "libvisual: ", 11)) filtname = filter_name + 11;
  else filtname = filter_name;
  libvis->actor = visual_actor_new(filtname);
  weed_free(filter_name);

  visual_actor_realize(libvis->actor);
  visual_actor_set_video(libvis->actor, libvis->video);
  visual_actor_video_negotiate(libvis->actor, 0, FALSE, FALSE);
  visual_input_realize(libvis->input);

  libvis->audio = (short *)weed_calloc(LIBVIS_ABUF_SIZE * 2, sizeof(short));
  if (!libvis->audio) return WEED_ERROR_MEMORY_ALLOCATION;
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
  weed_libvis_t *libvis = (weed_libvis_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (libvis) {
    if (libvis->instance > 0) {
      visual_object_destroy(VISUAL_OBJECT(libvis->input));
    }
    if (libvis->video) visual_object_free(VISUAL_OBJECT(libvis->video));
    if (libvis->actor) visual_object_destroy(VISUAL_OBJECT(libvis->actor));
    if (libvis->audio) weed_free(libvis->audio);
    weed_free(libvis);
    libvis = NULL;
    weed_set_voidptr_value(inst, "plugin_internal", libvis);
  }
  if (--instances < 0) instances = 0;

  return WEED_SUCCESS;
}


static void store_audio(weed_libvis_t *libvis, weed_plant_t *in_channel) {
  // convert float audio to s16le, append to libvis->audio

  if (in_channel) {
    int achans;
    int adlen = weed_channel_get_audio_length(in_channel);
    float **adata = (float **)weed_channel_get_audio_data(in_channel, &achans);
    if (adlen > 0 && adata && achans) {
      off_t sdf, offset = 0;
      off_t overflow;

      pthread_mutex_lock(&libvis->pcm_mutex);

      /// we want to send LIBVIS_ABUF_SIZE samples of left, followed by LIBVIS_ABUF_SIZE samples of right / mono
      /// so we use the buffer like two sliding windows
      sdf = libvis->audio_frames;
      overflow = sdf + adlen - LIBVIS_ABUF_SIZE;
      if (overflow > LIBVIS_ABUF_SIZE) {
        /// adlen >= LIBVIS_ABUF_SIZE, write the last LIBVIS_ABUF_SIZE samples from buffer
        libvis->audio_frames = 0;
        offset = adlen - LIBVIS_ABUF_SIZE;
        adlen = LIBVIS_ABUF_SIZE;
      } else if (overflow > 0) {
        /// make space by shifting the old audio
        weed_memmove((void *)libvis->audio, (void *)libvis->audio + overflow  * sizeof(short), (sdf - overflow) * sizeof(short));
        libvis->audio_frames -= overflow;
      }
      for (int i = 0; i < adlen; i ++) {
        libvis->audio[libvis->audio_frames + i] = 32767. * adata[0][offset + i];
        if (achans == 2)
          libvis->audio[libvis->audio_frames + LIBVIS_ABUF_SIZE + i] = 32767. * adata[1][offset + i];
        else
          libvis->audio[libvis->audio_frames + LIBVIS_ABUF_SIZE + i] = libvis->audio[libvis->audio_frames + i];
      }
      libvis->audio_frames += adlen;
      //libvis->audio_frames += adlen * achans;
      pthread_mutex_unlock(&libvis->pcm_mutex);
      weed_free(adata);
    }
  }
}


static weed_error_t libvis_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_libvis_t *libvis = (weed_libvis_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  void *pixel_data = weed_channel_get_pixel_data(out_channel);

  if (in_channel) store_audio(libvis, in_channel);

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

  char *xlpp = getenv("VISUAL_PLUGIN_PATH"), *lpp;

  char *vdir;

  int filter_flags = 0;

  if (!xlpp || !*xlpp) return NULL;

  weed_set_string_value(plugin_info, WEED_LEAF_PACKAGE_NAME, "libvisual");

  // set hints for host
  weed_set_int_value(in_chantmpls[0], WEED_LEAF_AUDIO_RATE, 44100);
  weed_set_int_value(in_chantmpls[0], WEED_LEAF_MAX_AUDIO_LENGTH, LIBVIS_ABUF_SIZE);
  weed_set_int_value(in_chantmpls[0], WEED_LEAF_MIN_AUDIO_LENGTH, LIBVIS_ABUF_SIZE);

  instances = 0;
  old_input = NULL;
  old_visinput = NULL;

  if (VISUAL_PLUGIN_API_VERSION < 2) return NULL;
  visual_log_set_verboseness(VISUAL_LOG_VERBOSENESS_NONE);

  lpp = strdup(xlpp);
  vdir = strtok(lpp, ":");

  // add lpp paths
  while (vdir) {
    if (!*vdir) continue;

    curvdir = opendir(vdir);
    if (!curvdir) continue;
    closedir(curvdir);

    visual_init_path_add(strdup(vdir));
    vdir = strtok(NULL, ":");
  }
  weed_free(lpp);

  if (visual_init(NULL, NULL) < 0) {
    fprintf(stderr, "Libvis : Unable to init libvisual plugins\n");
    return NULL;
  }

  in_params[1] = NULL;
  out_chantmpls[1] = NULL;

  while ((name = (char *)visual_actor_get_next_by_name_nogl(name))) {
    snprintf(fullname, PATH_MAX, "%s", name);
    in_params[0] = weed_string_list_init("listener", "Audio _listener", 5, listeners);
    weed_paramtmpl_set_flags(in_params[0], WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
    out_chantmpls[0] = weed_channel_template_init("out channel 0",
                       WEED_CHANNEL_REINIT_ON_SIZE_CHANGE | WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE);
    filter_class = weed_filter_class_init(fullname, "Team libvisual", 1, filter_flags, palette_list,
                                          libvis_init, libvis_process, libvis_deinit, in_chantmpls,
                                          out_chantmpls, in_params, NULL);
    weed_set_double_value(filter_class, WEED_LEAF_PREFERRED_FPS, 50.); // set reasonable default fps

    list = add_to_list_sorted(list, filter_class, (const char *)name);
  }

  add_filters_from_list(plugin_info, list);

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;


///////////// callback so host can supply its own audio sammples //////////

static int libvis_host_audio_callback(VisInput *input, VisAudio *audio, void *user_data) {
  // audio_data is 16bit signed, little endian
  // two channels non-interlaced, ideally LIBVIS_ABUF_SIZE samples in length
  // a rate of 44100Hz is recommended

  // return -1 on failure, 0 on success

  int alen;

  weed_libvis_t *libvis = (weed_libvis_t *)user_data;

  alen = libvis->audio_frames * 4;

  if (alen > 2048) alen = 2048;

  if (libvis->audio) {
    pthread_mutex_lock(&libvis->pcm_mutex);
    weed_memcpy(audio->plugpcm, libvis->audio, alen);
    libvis->audio_frames = 0;
    pthread_mutex_unlock(&libvis->pcm_mutex);
  }
  return 0;
}
