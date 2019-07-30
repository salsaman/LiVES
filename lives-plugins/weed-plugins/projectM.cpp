// projectM.c
// weed plugin
// (c) G. Finch (salsaman) 2014 - 2016
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


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

static int num_versions = 2; // number of different weed api versions supported
static int api_versions[] = {131, 100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////
#define HAVE_SDL2
#define USE_DBLBUF 1

#include <libprojectM/projectM.hpp>

#include <GL/gl.h>

#ifdef HAVE_SDL2
#include <SDL2/SDL.h>
#else

#include <SDL/SDL.h>
#include <SDL_syswm.h>
#endif

#include <pthread.h>

#include <limits.h>

#include <sys/time.h>

#include <errno.h>
#include <unistd.h>

#include "projectM-ConfigFile.h"
#include "projectM-getConfigFilename.h"

#define TARGET_FPS 35.
#define MESHSIZE 128
#define DEF_TEXTURESIZE 1024

static int copies = 0;

static pthread_cond_t cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timespec ts;

typedef struct {
  projectM *globalPM;
  GLubyte *fbuffer;
  int textureHandle;
  int width;
  int height;
  volatile bool worker_ready;
  volatile int pidx;
  int opidx;
  volatile int nprs;
  char **volatile prnames;  // volatile ptr to non-volatile strings !!
  pthread_mutex_t mutex;
  pthread_mutex_t pcm_mutex;
  pthread_t thread;
  int audio_frames;
  float *audio;
  float fps;
  volatile bool die;
  volatile bool failed;
  volatile bool update_size;
  volatile bool rendering;
#ifdef HAVE_SDL2
  SDL_Window *win;
  SDL_GLContext glCtx;
#endif
} _sdata;

static _sdata *statsd;

static int maxwidth, maxheight;

static int inited = 0;

#ifndef HAVE_SDL2
static int resize_display(int width, int height) {
  int flags = SDL_OPENGL | SDL_HWSURFACE | SDL_RESIZABLE;

  // 0 : use current bits per pixel
  if (!SDL_SetVideoMode(width, height, 0, flags)) {
    fprintf(stderr, "Video mode set failed: %s\n", SDL_GetError());
    return 1;
  }

  return 0;
}
#endif

static int change_size(_sdata *sdata) {
  int ret = 0;
  sdata->globalPM->projectM_resetGL(sdata->width, sdata->height);
  if (sdata->fbuffer != NULL) weed_free(sdata->fbuffer);
#ifdef HAVE_SDL2
  SDL_SetWindowSize(sdata->win, sdata->width, sdata->height);
#else
  ret = resize_display(sdata->width, sdata->height);
#endif
  sdata->fbuffer = (GLubyte *)weed_malloc(sizeof(GLubyte) * sdata->width * sdata->height * 3);
  return ret;
}


static int init_display(_sdata *sd) {
  int defwidth = sd->width;
  int defheight = sd->height;

  /* First, initialize SDL's video subsystem. */
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
    fprintf(stderr, "Video initialization failed: %s\n",
            SDL_GetError());
    return 1;
  }

  /* Let's get some video information. */
#ifdef HAVE_SDL2
  SDL_Rect rect;
  SDL_GetDisplayBounds(0, &rect);
  maxwidth = rect.w;
  maxheight = rect.h;
#else
  const SDL_VideoInfo *info = SDL_GetVideoInfo();
  if (!info) {
    /* This should probably never happen. */
    fprintf(stderr, "Video query failed: %s\n",
            SDL_GetError());

    return 2;
  }
  maxwidth = info->current_w;
  maxheight = info->current_h;
#endif
  printf("Screen Resolution: %d x %d\n", maxwidth, maxheight);

  // if (defwidth > maxwidth) defwidth = maxwidth;
  // if (defheight > maxheight) defheight = maxheight;

  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, USE_DBLBUF);

#ifdef HAVE_SDL2
  sd->win = SDL_CreateWindow("projectM", SDL_WINDOWPOS_UNDEFINED , SDL_WINDOWPOS_UNDEFINED, defwidth, defheight,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
  sd->glCtx = SDL_GL_CreateContext(sd->win);
#else
  if (resize_display(defwidth, defheight)) return 3;
#endif
  //if (change_size(sd)) return 4;

  return 0;
}


static int render_frame(_sdata *sdata) {
  sdata->globalPM->renderFrame();

  glClear(GL_COLOR_BUFFER_BIT);
  glClear(GL_DEPTH_BUFFER_BIT);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glViewport(0, 0, sdata->width, sdata->height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1, 1, -1, 1, 2, 10);

  glEnable(GL_DEPTH_TEST);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glTranslatef(.0, .0, -2);

  glEnable(GL_TEXTURE_2D);
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  glBindTexture(GL_TEXTURE_2D, sdata->textureHandle);
  glColor4d(1.0, 1.0, 1.0, 1.0);

  glBegin(GL_QUADS);
  glTexCoord2d(0, 1);
  glVertex3d(-1, -1, 0);
  glTexCoord2d(0, 0);
  glVertex3d(-1, 1, 0);
  glTexCoord2d(1, 0);
  glVertex3d(1, 1, 0);
  glTexCoord2d(1, 1);
  glVertex3d(1, -1, 0);
  glEnd();

  glDisable(GL_TEXTURE_2D);

  glMatrixMode(GL_MODELVIEW);
  glDisable(GL_DEPTH_TEST);

#if USE_DBLBUF
  glReadPixels(0, 0, sdata->width, sdata->height, GL_RGB, GL_UNSIGNED_BYTE, sdata->fbuffer);
  pthread_mutex_lock(&sdata->mutex);
#ifdef HAVE_SDL2
  SDL_GL_SwapWindow(sdata->win);
#else
  SDL_GL_SwapBuffers();
#endif
  pthread_mutex_unlock(&sdata->mutex);
#else
  pthread_mutex_lock(&sdata->mutex);
  glReadPixels(0, 0, sdata->width, sdata->height, GL_RGB, GL_UNSIGNED_BYTE, sdata->fbuffer);
  pthread_mutex_unlock(&sdata->mutex);
#endif
  return 0;
}


static void do_exit(void) {
  //pthread_mutex_lock(&cond_mutex);
  //pthread_cond_signal(&cond);
  //pthread_mutex_unlock(&cond_mutex);

  if (inited && statsd != NULL) {
    statsd->die = true;
  }
}


static void *worker(void *data) {
  std::string prname;

  projectM::Settings settings;

  bool rerand = true;

  _sdata *sd = (_sdata *)data;

  register int i = 0;

  float hwratio = (float)sd->height / (float)sd->width;

  if (init_display(sd)) {
    sd->failed = true;
    sd->worker_ready = true;

    // tell main thread we are ready
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    SDL_Quit();
    return NULL;
  }

  atexit(do_exit);

  settings.windowWidth = sd->width;
  settings.windowHeight = sd->height;
  settings.meshX = MESHSIZE;
  settings.meshY = settings.meshX * hwratio;
  settings.fps = sd->fps;
  settings.smoothPresetDuration = 2;
  settings.presetDuration = 10;
  settings.beatSensitivity = .5;
  settings.aspectCorrection = 1;
  settings.softCutRatingsEnabled = 0;
  settings.shuffleEnabled = 1;
  settings.presetURL = "/usr/share/projectM/presets";
  settings.menuFontURL = "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf";
  settings.titleFontURL = "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf";
  settings.easterEgg = 1;
  settings.textureSize = DEF_TEXTURESIZE;

  // can fail here
  sd->globalPM = new projectM(settings, 0);

  sd->textureHandle = sd->globalPM->initRenderToTexture();

  sd->nprs = sd->globalPM->getPlaylistSize() + 1;

  sd->prnames = (char **volatile)weed_malloc(sd->nprs * sizeof(char *));
  sd->prnames[0] = strdup("- Random -");

  for (i = 1; i < sd->nprs; i++) {
    sd->prnames[i] = strdup((sd->globalPM->getPresetName(i - 1)).c_str());
  };

  // tell main thread we are ready
  pthread_mutex_lock(&cond_mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&cond_mutex);

  sd->worker_ready = true;

  while (!sd->die) {
    if (!sd->rendering) {
      usleep(10000);
      rerand = true;
      continue;
    }

    if (sd->pidx == -1) {
      if (rerand) sd->globalPM->selectRandom(true);
      rerand = false;
    } else if (sd->pidx != sd->opidx) {
      sd->globalPM->setPresetLock(true);
      sd->globalPM->selectPreset(sd->pidx);
    }

    sd->opidx = sd->pidx;

    pthread_mutex_lock(&sd->pcm_mutex);
    if (sd->audio_frames > 0) {
      // sd->audio should contain data for 1 channel only
      sd->globalPM->pcm()->addPCMfloat(sd->audio, sd->audio_frames);
      sd->audio_frames = 0;
      weed_free(sd->audio);
      sd->audio = NULL;
    }
    pthread_mutex_unlock(&sd->pcm_mutex);
    pthread_mutex_lock(&sd->mutex);
    if (sd->update_size) {
      change_size(sd);
      sd->update_size = false;
    }
    pthread_mutex_unlock(&sd->mutex);
    render_frame(sd);
  }

  // TODO : segfault
  //if (sd->globalPM != NULL) delete(sd->globalPM);

  SDL_Quit();
  return NULL;
}


static int projectM_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sd = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);

  copies--;

  if (sd != NULL) {
    sd->rendering = false;
  }

  return WEED_NO_ERROR;
}


static int projectM_init(weed_plant_t *inst) {
  _sdata *sd;

  int error;

  if (copies == 1) return WEED_ERROR_TOO_MANY_INSTANCES;
  copies++;

  if (!inited) {
    int rc = 0;

    weed_plant_t *out_channel = weed_get_plantptr_value(inst, "out_channels", &error);
    weed_plant_t *iparam = weed_get_plantptr_value(inst, "in_parameters", &error);
    weed_plant_t *itmpl = weed_get_plantptr_value(iparam, "template", &error);
    weed_plant_t *iparamgui = weed_get_plantptr_value(itmpl, "gui", &error);

    int width = weed_get_int_value(out_channel, "width", &error);
    int height = weed_get_int_value(out_channel, "height", &error);

    //int palette=weed_get_int_value(out_channel,"current_palette",&error);

    sd = (_sdata *)weed_malloc(sizeof(_sdata));
    if (sd == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

    sd->fbuffer = (GLubyte *)weed_malloc(sizeof(GLubyte) * width * height * 3);

    if (sd->fbuffer == NULL) {
      weed_free(sd);
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    weed_set_voidptr_value(inst, "plugin_internal", sd);

    sd->pidx = sd->opidx = -1;

    sd->fps = TARGET_FPS;
    if (weed_plant_has_leaf(inst, "fps")) sd->fps = weed_get_double_value(inst, "fps", &error);

    sd->width = width;
    sd->height = height;

    sd->die = false;
    sd->failed = false;
    sd->update_size = false;

    sd->audio = NULL;
    sd->audio_frames = 0;

    pthread_mutex_init(&sd->mutex, NULL);
    pthread_mutex_init(&sd->pcm_mutex, NULL);

    sd->nprs = 0;
    sd->prnames = NULL;
    sd->worker_ready = false;
    sd->rendering = false;

    // kick off a thread to init screean and render
    pthread_create(&sd->thread, NULL, worker, sd);

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 30;

    // wait for worker thread ready
    while (!sd->worker_ready && rc == 0) {
      pthread_mutex_lock(&cond_mutex);
      rc = pthread_cond_timedwait(&cond, &cond_mutex, &ts);
      pthread_mutex_unlock(&cond_mutex);
    }

    if (rc == ETIMEDOUT && !sd->worker_ready) {
      // if we timedout then die
      projectM_deinit(inst);
      return WEED_ERROR_INIT_ERROR;
    }

    inited = 1;
    weed_set_string_array(iparamgui, "choices", sd->nprs, (char **)sd->prnames);
  } else sd = statsd;

  sd->nprs--;

  sd->rendering = true;

  statsd = sd;

  weed_set_voidptr_value(inst, "plugin_internal", sd);

  return WEED_NO_ERROR;
}


static int projectM_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;

  _sdata *sd = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", &error);

  weed_plant_t *in_channel = weed_get_plantptr_value(inst, "in_channels", &error);
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, "out_channels", &error);
  weed_plant_t *inparam = weed_get_plantptr_value(inst, "in_parameters", &error);
  unsigned char *dst = (unsigned char *)weed_get_voidptr_value(out_channel, "pixel_data", &error);

  unsigned char *ptrd, *ptrs;

  int width = weed_get_int_value(out_channel, "width", &error);
  int height = weed_get_int_value(out_channel, "height", &error);

  //int palette=weed_get_int_value(out_channel,"current_palette",&error);

  int rowstride = weed_get_int_value(out_channel, "rowstrides", &error);

  int widthx = width * 3;

  register int j;

  if (sd == NULL || sd->failed) return WEED_ERROR_PLUGIN_INVALID;

  if (sd->width != width || sd->height != height) {
    sd->width = width;
    sd->height = height;
    if (sd->width > maxwidth) sd->width = maxwidth;
    if (sd->height > maxheight) sd->height = maxheight;
    sd->update_size = true;
  }

  if (sd->update_size || sd->fbuffer == NULL) return WEED_NO_ERROR;

  // ex. nprs = 10, we have 10 programs 0 - 9 and -1 is random
  // 0 - 10, we just subtract 1
  // else (val - 1) % nprs .e.g 11 - 1 = 10, 10 % 10 = 0

  sd->pidx = weed_get_int_value(inparam, "value", &error);

  if (sd->pidx <= sd->nprs) sd->pidx--;
  else sd->pidx = (sd->pidx - 1) % sd->nprs;

  if (0) {
    projectMEvent evt;
    projectMKeycode key;
    projectMModifier mod;

    evt = PROJECTM_KEYDOWN;
    //mod=PROJECTM_KMOD_LSHIFT;
    key = PROJECTM_K_n;

    // send any keystrokes to projectM
    sd->globalPM->key_handler(evt, key, mod);
  }

  if (weed_plant_has_leaf(inst, "fps")) sd->fps = weed_get_double_value(inst, "fps", &error);

  if (in_channel != NULL) {
    int adlen = weed_get_int_value(in_channel, "audio_data_length", &error);
    float *adata = (float *)weed_get_voidptr_value(in_channel, "audio_data", &error);
    if (adlen > 0 && adata != NULL) {
      float *aud_data;
      int ainter = weed_get_boolean_value(in_channel, "audio_interleaf", &error);
      pthread_mutex_lock(&sd->pcm_mutex);
      aud_data = (float *)weed_malloc((adlen + sd->audio_frames) * sizeof(float));
      if (sd->audio != NULL) {
        weed_memcpy(aud_data, sd->audio, sd->audio_frames * sizeof(float));
        weed_free(sd->audio);
      }
      if (ainter == WEED_FALSE) {
        weed_memcpy(aud_data + sd->audio_frames, adata, adlen * sizeof(float));
      } else {
        int achans = weed_get_int_value(in_channel, "audio_channels", &error);
        for (j = 0; j < adlen; j++) {
          weed_memcpy(aud_data + sd->audio_frames + j, adata, sizeof(float));
          adata += achans;
        }
      }
      sd->audio_frames += adlen;
      sd->audio = aud_data;
      pthread_mutex_unlock(&sd->pcm_mutex);
    }
  }

  //if (palette==WEED_PALETTE_RGBA32) widthx=width*4;

  ptrd = dst;
  ptrs = sd->fbuffer;

  pthread_mutex_lock(&sd->mutex);

  // copy sd->fbuffer -> dst
  if (rowstride == widthx && width == sd->width && height == sd->height) {
    weed_memcpy(ptrd, ptrs, widthx * height);
  } else {
    for (j = 0; j < sd->height; j++) {
      weed_memcpy(ptrd, ptrs, widthx);
      ptrd += rowstride;
      ptrs += sd->width * 3;
    }
  }

  pthread_mutex_unlock(&sd->mutex);

  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, num_versions, api_versions);
  if (plugin_info != NULL) {
    int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_END};

    const char *xlist[3] = {"- Random -", "Choose...", NULL};

    weed_plant_t *in_params[] = {weed_string_list_init("preset", "_Preset", 0, xlist), NULL};

    weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("In audio", 0), NULL};

    weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE |
                                     WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE, palette_list), NULL
                                    };
    weed_plant_t *filter_class = weed_filter_class_init("projectM", "salsaman/projectM authors", 1, 0, &projectM_init,
                                 &projectM_process, &projectM_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

    //weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

    weed_set_int_value(in_params[0], "max", INT_MAX);

    weed_set_int_value(in_chantmpls[0], "audio_channels", 1);
    weed_set_boolean_value(in_chantmpls[0], "audio_interleaf", WEED_TRUE);
    weed_set_boolean_value(in_chantmpls[0], "optional", WEED_TRUE);

    weed_set_double_value(filter_class, "target_fps", TARGET_FPS); // set reasonable default fps

    weed_plugin_info_add_filter_class(plugin_info, filter_class);

    weed_set_int_value(plugin_info, "version", package_version);
  }

  statsd = NULL;

  return plugin_info;
}


void weed_desetup(void) {
  if (inited && statsd != NULL) {
    statsd->die = true;
    pthread_join(statsd->thread, NULL);
    if (statsd->fbuffer != NULL) weed_free(statsd->fbuffer);
    if (statsd->audio != NULL) weed_free(statsd->audio);
    if (statsd->prnames != NULL) {
      for (int i = 0; i < statsd->nprs; i++) {
        free(statsd->prnames[i]);
      }
      weed_free(statsd->prnames);
    }
    pthread_mutex_destroy(&statsd->mutex);
    pthread_mutex_destroy(&statsd->pcm_mutex);
    weed_free(statsd);
    statsd = NULL;
  }
  inited = 0;
}
