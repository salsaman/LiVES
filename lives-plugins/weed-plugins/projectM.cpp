// projectM.c
// weed plugin
// (c) G. Finch (salsaman) 2014 - 2016
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_AUDIO

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

static int verbosity = WEED_VERBOSITY_ERROR;

/////////////////////////////////////////////////////////////

#define USE_DBLBUF 1

#include <libprojectM/projectM.hpp>

#include <GL/gl.h>

#include <SDL.h>

#ifndef HAVE_SDL2
#include <SDL_syswm.h>
#endif

#include <pthread.h>

#include <limits.h>

#include <sys/time.h>

#include <errno.h>
#include <unistd.h>

#include "projectM-ConfigFile.h"
#include "projectM-getConfigFilename.h"
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>

#define TARGET_FPS 50.
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
  volatile bool worker_active;
  volatile int pidx;
  int opidx;
  volatile int nprs;
  char **volatile prnames;  // volatile ptr to non-volatile strings !!
  pthread_mutex_t mutex;
  pthread_mutex_t frame_mutex;
  pthread_mutex_t pcm_mutex;
  pthread_t thread;
  volatile int audio_frames;
  int achans;
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
static void winhide() {
  SDL_SysWMinfo info;

  Atom atoms[2];
  SDL_VERSION(&info.version);
  if (SDL_GetWMInfo(&info)) {
    Window win = info.info.x11.wmwindow;
    Display *dpy = info.info.x11.display;
    info.info.x11.lock_func();

    atoms[0] = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);
    atoms[1] = XInternAtom(dpy, "_NET_WM_STATE_DESKTOP", False);
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_STATE", False), XA_ATOM, 32,
                    PropModeReplace, (const unsigned char *) &atoms, 2);

    XIconifyWindow(dpy, win, 0);

    XFlush(dpy);
    info.info.x11.unlock_func();
  }
}


static int resize_display(int width, int height) {
  int flags = SDL_OPENGL | SDL_HWSURFACE | SDL_RESIZABLE;

  // 0 : use current bits per pixel
  if (!SDL_SetVideoMode(width, height, 0, flags)) {
    fprintf(stderr, "Video mode set failed: %s\n", SDL_GetError());
    return 1;
  }

  winhide();

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
  sdata->fbuffer = (GLubyte *)weed_calloc(sizeof(GLubyte) * sdata->width * sdata->height * 3, 1);
  return ret;
}


static int init_display(_sdata *sd) {
  int defwidth = sd->width;
  int defheight = sd->height;

  /* First, initialize SDL's video subsystem. */
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
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

  if (verbosity >= WEED_VERBOSITY_DEBUG)
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
  glFlush();

#if USE_DBLBUF
  pthread_mutex_lock(&sdata->frame_mutex);
  glReadPixels(0, 0, sdata->width, sdata->height, GL_RGB, GL_UNSIGNED_BYTE, sdata->fbuffer);
  pthread_mutex_unlock(&sdata->frame_mutex);
#ifdef HAVE_SDL2
  SDL_GL_SwapWindow(sdata->win);
#else
  SDL_GL_SwapBuffers();
#endif
#else
  pthread_mutex_lock(&sdata->frame_mutex);
  glReadPixels(0, 0, sdata->width, sdata->height, GL_RGB, GL_UNSIGNED_BYTE, sdata->fbuffer);
  pthread_mutex_unlock(&sdata->frame_mutex);
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
  float hwratio = (float)sd->height / (float)sd->width;
  //  int new_stdout, new_stderr;

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

  // if (verbosity < WEED_VERBOSITY_INFO) {
  //   new_stdout = dup(1);
  //   new_stderr = dup(2);
  //   close(1);
  //   close(2);
  //   new_stdout = new_stdout;
  //   new_stderr = new_stderr;
  // }

  settings.windowWidth = sd->width;
  settings.windowHeight = sd->height;
  settings.meshX = MESHSIZE;
  settings.meshY = settings.meshX * hwratio;
  settings.fps = 0;//sd->fps;
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

  for (int i = 1; i < sd->nprs; i++) sd->prnames[i] = strdup((sd->globalPM->getPresetName(i - 1)).c_str());

  // tell main thread we are ready
  pthread_mutex_lock(&cond_mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&cond_mutex);

  sd->worker_ready = true;
  pthread_mutex_lock(&sd->mutex);

  while (!sd->die) {
    if (!sd->rendering) {
      // deinit() will only exit once the mutex is unlocked, we need to make sure we are idling
      // else we can pick up the wm response for the playback plugin and cause it to hang
      pthread_mutex_unlock(&sd->mutex);
      sd->worker_active = false;
      usleep(10000);
      rerand = true;
      pthread_mutex_lock(&sd->mutex);
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
      if (sd->achans == 1) {
        sd->globalPM->pcm()->addPCMfloat(sd->audio, sd->audio_frames);
      }
      //else
      //sd->globalPM->pcm()->addPCMfloat_2ch(sd->audio, sd->audio_frames);
      sd->audio_frames = 0;
    }
    pthread_mutex_unlock(&sd->pcm_mutex);

    pthread_mutex_lock(&sd->frame_mutex);
    if (sd->update_size) {
      change_size(sd);
      sd->update_size = false;
    }
    pthread_mutex_unlock(&sd->frame_mutex);
    render_frame(sd);
  }
  pthread_mutex_unlock(&sd->mutex);

  // TODO : segfault
  //if (sd->globalPM != NULL) delete(sd->globalPM);

  SDL_Quit();
  return NULL;
}


static weed_error_t projectM_deinit(weed_plant_t *inst) {
  _sdata *sd = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sd) sd->rendering = false;
  pthread_mutex_lock(&sd->mutex);
  pthread_mutex_unlock(&sd->mutex);
  copies--;
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t projectM_init(weed_plant_t *inst) {
  if (copies == 1) return WEED_ERROR_TOO_MANY_INSTANCES;
  else {
    _sdata *sd;
    weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
    weed_plant_t **iparams = weed_get_in_params(inst, NULL);

    copies++;

    if (!inited) {
      int rc = 0;
      int width = weed_channel_get_width(out_channel);
      int height = weed_channel_get_height(out_channel);

      sd = (_sdata *)weed_malloc(sizeof(_sdata));
      if (!sd) return WEED_ERROR_MEMORY_ALLOCATION;

      sd->fbuffer = (GLubyte *)weed_calloc(sizeof(GLubyte) * width * height * 3, 1);

      if (!sd->fbuffer) {
        weed_free(sd);
        return WEED_ERROR_MEMORY_ALLOCATION;
      }

      weed_set_voidptr_value(inst, "plugin_internal", sd);

      sd->pidx = sd->opidx = -1;

      sd->fps = TARGET_FPS;
      if (weed_plant_has_leaf(inst, WEED_LEAF_FPS)) sd->fps = weed_get_double_value(inst, WEED_LEAF_FPS, NULL);

      sd->width = width;
      sd->height = height;

      sd->die = false;
      sd->failed = false;
      sd->update_size = false;

      sd->audio = (float *)weed_calloc(4096,  sizeof(float));
      sd->audio_frames = 0;
      sd->achans = 0;

      pthread_mutex_init(&sd->mutex, NULL);
      pthread_mutex_init(&sd->frame_mutex, NULL);
      pthread_mutex_init(&sd->pcm_mutex, NULL);

      sd->nprs = 0;
      sd->prnames = NULL;
      sd->worker_ready = false;
      sd->worker_active = false;
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
        return WEED_ERROR_PLUGIN_INVALID;
      }
      statsd = sd;
      inited = 1;
    } else sd = statsd;

    sd->rendering = true;
    weed_set_voidptr_value(inst, "plugin_internal", sd);

    if (!weed_plant_has_leaf(iparams[0], WEED_LEAF_GUI)) {
      weed_plant_t *iparamgui = weed_param_get_gui(iparams[0]);
      weed_set_string_array(iparamgui, WEED_LEAF_CHOICES, sd->nprs, (char **)sd->prnames);
    }
    weed_free(iparams);
  }
  return WEED_SUCCESS;
}


static weed_error_t projectM_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  _sdata *sd = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t *inparam = weed_get_plantptr_value(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  unsigned char *dst = (unsigned char *)weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *ptrs, *end;

  int width = weed_get_int_value(out_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);

  //int palette=weed_get_int_value(out_channel,WEED_LEAF_CURRENT_PALETTE,NULL);

  int rowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);

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

  if (sd->update_size || sd->fbuffer == NULL) return WEED_SUCCESS;

  // ex. nprs = 10, we have 9 programs 1 - 9 and 0 is random
  // first we shift the vals. down by 1, so -1 is random and prgs 0 - 8
  // 0 - 9, we just use the value - 1
  // else val % (nprs - 1) .e.g 9 mod 9 is 0, 10 mod 9 is 1, etc

  sd->pidx = (weed_param_get_value_int(inparam) - 1) % (sd->nprs - 1);

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

  if (weed_plant_has_leaf(inst, WEED_LEAF_FPS)) sd->fps = weed_get_double_value(inst, WEED_LEAF_FPS, NULL);

  /// fill the audio buffer for the next frame, then get the frame

  if (in_channel != NULL) {
    int achans;
    int adlen = weed_get_int_value(in_channel, WEED_LEAF_AUDIO_DATA_LENGTH, NULL);
    float **adata = (float **)weed_channel_get_audio_data(in_channel, &achans);
    if (adlen > 0 && adata != NULL && adata[0] != NULL) {
      int offset = 0, sdf;
      int overflow;
      achans = 1; /// 2 needs newer version of projM
      pthread_mutex_lock(&sd->pcm_mutex);
      if (achans != sd->achans) {
        sd->audio_frames = 0;
        sd->achans = achans;
      }
      /// there is no point sending more than 2048 samples per channel ysince projectM will not use more
      /// so we use the buffer like a sliding window which will contain at most 2048 samples per channel
      sdf = sd->audio_frames / achans;
      overflow = sdf + adlen - 2048;
      //fprintf(stderr, "projm %d %d %d\n", sdf, adlen, overflow);
      if (overflow >= sdf) {
        /// adlen >= 2048, write the last 2048 samples from buffer
        sd->audio_frames = 0;
        offset = adlen - 2048;
        adlen = 2048;
      } else if (overflow > 0) {
        /// make space by shifting the old audio
        weed_memmove((void *)sd->audio, (char *)sd->audio + overflow * achans * sizeof(float), (sdf - overflow) * sizeof(float));
        sd->audio_frames -= overflow * achans;
      }
      //fprintf(stderr, "projm2 %p %p %f\n", adata, adata ? adata[0] : 0, (float)adata[0][0]);
      for (int i = 0; i < adlen; i ++) {
        for (j = 0; j < achans; j++) {
          sd->audio[sd->audio_frames + i * achans + j] = adata[j][i + offset];
        }
      }
      sd->audio_frames += adlen * achans;
      pthread_mutex_unlock(&sd->pcm_mutex);
    }
    if (adata != NULL) weed_free(adata);
  }

  //if (palette==WEED_PALETTE_RGBA32) widthx=width*4;

  ptrs = sd->fbuffer;

  pthread_mutex_lock(&sd->frame_mutex);

  // copy sd->fbuffer -> dst
  if (rowstride == widthx && width == sd->width && height == sd->height) {
    weed_memcpy(dst, ptrs, widthx * height);
  } else {
    for (end = dst + height * rowstride; dst < end; dst += rowstride) {
      weed_memcpy(dst, ptrs, widthx);
      ptrs += widthx;
    }
  }

  pthread_mutex_unlock(&sd->frame_mutex);

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_END};
  const char *xlist[3] = {"- Random -", "Choose...", NULL};
  weed_plant_t *in_params[] = {weed_string_list_init("preset", "_Preset", 0, xlist), NULL};
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("In audio", WEED_CHANNEL_OPTIONAL), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0",
                                   WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE), NULL
                                  };
  weed_plant_t *filter_class = weed_filter_class_init("projectM", "salsaman/projectM authors", 1, 0, palette_list, projectM_init,
                               projectM_process, projectM_deinit, in_chantmpls, out_chantmpls, in_params, NULL);
  weed_plant_t *gui = weed_paramtmpl_get_gui(in_params[0]);
  weed_gui_set_flags(gui, WEED_GUI_CHOICES_SET_ON_INIT);
  weed_set_int_value(in_chantmpls[0], WEED_LEAF_MAX_AUDIO_LENGTH, 2048);
  weed_set_int_value(in_params[0], WEED_LEAF_MAX, INT_MAX);
  weed_set_double_value(filter_class, WEED_LEAF_TARGET_FPS, TARGET_FPS); // set reasonable default fps
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  verbosity = weed_get_host_verbosity(weed_get_host_info(plugin_info));
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
  statsd = NULL;
  XInitThreads();
}
WEED_SETUP_END;


WEED_DESETUP_START {
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
    pthread_mutex_destroy(&statsd->frame_mutex);
    pthread_mutex_destroy(&statsd->pcm_mutex);
    weed_free(statsd);
    statsd = NULL;
  }
  inited = 0;
}
WEED_DESETUP_END;

