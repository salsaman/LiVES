// projectM.cpp
// weed plugin
// (c) G. Finch (salsaman) 2014 - 2020
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_AUDIO
#define NEED_PALETTE_UTILS
#define NEED_RANDOM

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

static int verbosity = WEED_VERBOSITY_ERROR;
#define WORKER_TIMEOUT_SEC 30 /// how long to wait for worker thread startup
#define MAX_AUDLEN 2048 /// this is defined by projectM itself, increasing the value above 2048 will only result in jumps in the audio
#define DEF_SENS 1. /// beat sensitivity 0. -> 5.  (lower is more sensitive); too high -> less dynamic, too low - nothing w. silence
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

#define PREF_FPS 50.
#define MESHSIZE 128
#define DEF_TEXTURESIZE 1024

static int copies = 0;

static const GLushort RGB2ARGB[8] =  {0, 1, 1, 2, 2, 3, 4, 0};
static const GLushort RGB2BGR[4] = {0, 2, 2, 0};

static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timespec ts;

static int count = 0;
static int pcount = 0;
static int blanks = 0;

typedef struct {
  projectM *globalPM;
  GLubyte *fbuffer;
  int textureHandle;
  int width, height, texsize;
  int palette, psize;
  int rowstride;
  volatile bool worker_ready;
  volatile bool worker_active;
  int pidx, opidx;
  int nprs;
  char **prnames;
  uint8_t *bad_programs;
  int program;
  bool bad_prog, checkforblanks;
  pthread_mutex_t mutex, pcm_mutex;
  pthread_t thread;
  size_t audio_frames, abufsize, audio_offs;
  int achans;
  float *audio;
  float fps, tfps;
  float ncycs;
  int cycadj;
  volatile bool die;
  volatile bool failed;
  bool update_size, update_psize;
  volatile bool needs_more;
  volatile bool rendering;
  volatile bool needs_update;
  bool set_update;
  bool got_first;
#ifdef HAVE_SDL2
  SDL_Window *win;
  SDL_GLContext glCtx;
#endif
  weed_error_t error;
  double timer;
  weed_timecode_t timestamp;
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
    if (verbosity >= WEED_VERBOSITY_CRITICAL)
      fprintf(stderr, "{ProjectM plugin: Video mode set failed: %s\n", SDL_GetError());
    return 1;
  }

  winhide();

  return 0;
}
#endif


static int change_size(_sdata *sdata) {
  int ret = 0;
  int newsize = sdata->width;
  if (sdata->height > newsize) newsize = sdata->height;
  sdata->globalPM->projectM_resetGL(sdata->width, sdata->height); //1

  // TODO: can we change the texture size ?
  //settings.textureSize = sd->width;
#ifdef HAVE_SDL2
  SDL_SetWindowSize(sdata->win, sdata->width, sdata->height);
#else
  ret = resize_display(sdata->width, sdata->height);
#endif

  //sdata->globalPM->projectM_resetTextures(); // 2
  std::cerr << "Set new size to " << newsize << std::endl;
  sdata->globalPM->changeTextureSize(newsize); // 3
  sdata->textureHandle = sdata->globalPM->initRenderToTexture();

  return ret;
}


static int init_display(_sdata *sd) {
  int defwidth = sd->width;
  int defheight = sd->height;

  /* First, initialize SDL's video subsystem. */
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    if (verbosity >= WEED_VERBOSITY_CRITICAL)
      fprintf(stderr, "{ProjectM plugin: Video initialization failed: %s\n",
              SDL_GetError());
    return 1;
  }

  /* Let's get some video information. */
#ifdef HAVE_SDL2
  SDL_Rect rect;
  SDL_GetDisplayBounds(0, &rect);
  maxwidth = rect.w;
  maxheight = rect.h;
  if (verbosity >= WEED_VERBOSITY_DEBUG)
    fprintf(stderr, "ProjectM running with SDL 2\n");
#else
  const SDL_VideoInfo *info = SDL_GetVideoInfo();
  if (verbosity >= WEED_VERBOSITY_DEBUG)
    fprintf(stderr, "ProjectM running with SDL 1\n");
  if (!info) {
    /* This should probably never happen. */
    if (verbosity >= WEED_VERBOSITY_CRITICAL)
      fprintf(stderr, "ProjectM plugin: Video query failed: %s\n",
              SDL_GetError());
    return 2;
  }
  maxwidth = info->current_w;
  maxheight = info->current_h;
#endif

  if (verbosity >= WEED_VERBOSITY_DEBUG)
    printf("Screen Resolution: %d x %d\n", maxwidth, maxheight);

  if (defwidth > maxwidth) defwidth = maxwidth;
  if (defheight > maxheight) defheight = maxheight;

  //SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, USE_DBLBUF);

#ifdef HAVE_SDL2
  sd->win = SDL_CreateWindow("projectM", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, defwidth, defheight,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
  sd->glCtx = SDL_GL_CreateContext(sd->win);
#else
  if (resize_display(defwidth, defheight)) return 3;
#endif
  //if (change_size(sd)) return 4;

  return 0;
}


bool resize_buffer(_sdata *sd) {
  size_t align = 1;
  if ((sd->rowstride & 0X01) == 0) {
    if ((sd->rowstride & 0X03) == 0) {
      if ((sd->rowstride & 0X07) == 0) {
        if ((sd->rowstride & 0X0F) == 0) {
          align = 16;
        } else align = 8;
      } else align = 4;
    } else align = 2;
  }
  if (sd->fbuffer) weed_free(sd->fbuffer);
  sd->fbuffer = (GLubyte *)weed_calloc(sizeof(GLubyte) * sd->rowstride * sd->height / align, align);
  if (!sd->fbuffer) return false;
  return true;
}


static int render_frame(_sdata *sd) {
  float yscale = (float)sd->height / sd->texsize * 2.;
  float xscale = (float)sd->width / sd->texsize * 2.;
  bool checked_audio = false;

  if (sd->needs_update || !sd->got_first) {
    if (sd->needs_update) {
      pthread_mutex_lock(&cond_mutex);
      sd->needs_update = false;
      pthread_cond_signal(&cond);
      while (!sd->set_update) {
        pthread_cond_wait(&cond, &cond_mutex);
      }
      pthread_mutex_unlock(&cond_mutex);
      sd->set_update = false;
      if (sd->update_size) {
        glFlush();
        change_size(sd);
        sd->update_size = false;
      }
      if (sd->update_psize) {
        if (!resize_buffer(sd)) {
          sd->error = WEED_ERROR_MEMORY_ALLOCATION;
          sd->update_psize = false;
          sd->needs_more = false;
          pthread_mutex_lock(&cond_mutex);
          pthread_cond_signal(&cond);
          pthread_mutex_unlock(&cond_mutex);
          return 1;
        }
      }
    }
    if (sd->update_psize || !sd->got_first) {
      sd->update_psize = false;
      glPixelStorei(GL_UNPACK_ROW_LENGTH, sd->rowstride / sd->psize);
      if (sd->psize == 4) {
        if (sd->palette == WEED_PALETTE_BGRA32) {
          glPixelMapusv(GL_PIXEL_MAP_I_TO_I, 4, RGB2BGR);
        } else if (sd->palette == WEED_PALETTE_ARGB32) {
          glPixelMapusv(GL_PIXEL_MAP_I_TO_I, 8, RGB2ARGB);
        }
      } else {
        if (sd->palette == WEED_PALETTE_BGR24) {
          glPixelMapusv(GL_PIXEL_MAP_I_TO_I, 4, RGB2BGR);
        }
      }
    }
    sd->needs_more = true;
  }

  glFlush();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //glEnable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glViewport(0, 0, sd->width, sd->height);

  if (sd->needs_more || sd->audio_frames > sd->audio_offs) {
    size_t audlen = MAX_AUDLEN;
    pthread_mutex_lock(&sd->pcm_mutex);
    if (sd->audio_frames - sd->audio_offs < audlen)
      audlen = sd->audio_frames - sd->audio_offs;
    if (audlen) {
      sd->globalPM->pcm()->addPCMfloat(sd->audio + sd->audio_offs, audlen);
      sd->audio_offs += audlen;
    }
    pthread_mutex_unlock(&sd->pcm_mutex);
    checked_audio = true;
    if (sd->ncycs > 1.) sd->cycadj = -(int)(sd->ncycs);
  }

  sd->globalPM->renderFrame();
  pcount++;

  if (sd->needs_more && checked_audio) {
    glMatrixMode(GL_PROJECTION);
    glOrtho(0, 1, 0, 1, -1, 1);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glBindTexture(GL_TEXTURE_2D, sd->textureHandle);

    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);
    glVertex2d(-1., yscale - 1.);
    glTexCoord2i(0, 1);
    glVertex2i(-1, -1);
    glTexCoord2i(1, 1);
    glVertex2d(xscale - 1., -1.);
    glTexCoord2i(1, 0);
    glVertex2d(xscale - 1., yscale - 1.);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);

#if USE_DBLBUF
#ifdef HAVE_SDL2
    SDL_GL_SwapWindow(sd->win);
#else
    SDL_GL_SwapBuffers();
#endif
#endif
    pthread_mutex_lock(&buffer_mutex);
    glReadPixels(0, 0, sd->rowstride / sd->psize, sd->height, sd->psize == 4
                 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, sd->fbuffer);
    sd->needs_more = false;
    pthread_mutex_unlock(&buffer_mutex);
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    sd->got_first = true;
    if (sd->fps > 0.) sd->ncycs += sd->tfps / sd->fps - 1.;
    sd->ncycs += sd->cycadj;
    if (sd->ncycs < 0.) sd->ncycs = 0.;
    if (sd->pidx == -1 && sd->checkforblanks) {
      /// check for blank frames: if the first and second from a new program are both blank, mark the program as "bad"
      /// and pick another (not sure why the blank frames happen, but generally if the first two come back blank, so do all the
      /// rest. Possibly we need an image texture to load, which we don't have; more investigation needed).
      register int i;
      ssize_t frmsize = sd->rowstride * sd->height;
      if (sd->psize == 4) {
        for (i = 0; i < frmsize;
             i += sd->psize) if (((sd->fbuffer[i] | sd->fbuffer[i + 1] | sd->fbuffer[i + 2]) & sd->fbuffer[i + 3]) > 0) break;
      } else {
        for (i = 0; i < frmsize; i += sd->psize) if ((sd->fbuffer[i] | sd->fbuffer[i + 1] | sd->fbuffer[i + 2]) > 0) break;
      }
      if (i >= frmsize) blanks++;
      else {
        blanks = 0;
        //sd->check = false;
        sd->bad_programs[sd->program] = 2;
      }
      if (blanks > 1) sd->bad_prog = true;
    }
  } else {
    if (sd->ncycs > 1.) {
      sd->ncycs--;
      if (sd->ncycs < 1. && sd->cycadj < 0) sd->cycadj++;
    }
  }
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

  settings.windowWidth = sd->width;
  settings.windowHeight = sd->height;
  settings.meshX = sd->width / 64;
  settings.meshY = ((int)(settings.meshX * hwratio + 1) >> 1) << 1;
  settings.fps = sd->fps;
  settings.smoothPresetDuration = 2.;
  settings.presetDuration = 60;
  settings.beatSensitivity = DEF_SENS;
  settings.aspectCorrection = 1;
  settings.softCutRatingsEnabled = 1;
  settings.shuffleEnabled = 0;
  settings.presetURL = "/usr/share/projectM/presets";

  if (!access("/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf", R_OK)) {
    settings.menuFontURL = "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf";
    settings.titleFontURL = "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf";
  } else {
    settings.menuFontURL = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    settings.titleFontURL = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
  }
  settings.easterEgg = 1;

  if (sd->width >= sd->height)
    settings.textureSize = sd->texsize = sd->width;
  else
    settings.textureSize = sd->texsize = sd->height;

  if (sd->failed) {
    // can happen if the host is overloaded and the caller timed out
    SDL_Quit();
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    return NULL;
  }

  // can fail here
  sd->globalPM = new projectM(settings, 0);
  sd->globalPM->setPresetLock(true);
  sd->textureHandle = sd->globalPM->initRenderToTexture();
  sd->nprs = sd->globalPM->getPlaylistSize() + 1;
  sd->checkforblanks = true;
  sd->cycadj = 0;

  sd->prnames = (char **volatile)weed_malloc(sd->nprs * sizeof(char *));
  if (!sd->prnames) sd->error = WEED_ERROR_MEMORY_ALLOCATION;
  else sd->bad_programs = (uint8_t *)weed_calloc((sd->nprs - 1), 1);
  if (!sd->prnames) sd->error = WEED_ERROR_MEMORY_ALLOCATION;
  if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) sd->rendering = false;
  else {
    sd->prnames[0] = strdup("- Random -");
    for (int i = 1; i < sd->nprs; i++) sd->prnames[i] = strdup((sd->globalPM->getPresetName(i - 1)).c_str());
  }

  if (sd->failed) {
    // can happen if the host is overloaded and the caller timed out
    SDL_Quit();
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    return NULL;
  }

  pthread_mutex_lock(&sd->mutex);
  sd->worker_ready = true;

  // tell main thread we are ready
  pthread_mutex_lock(&cond_mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&cond_mutex);

  while (!sd->die) {
    if (!sd->rendering) {
      // deinit() will only exit once the mutex is unlocked, we need to make sure we are idling
      // else we can pick up the wm response for the playback plugin and cause it to hang
      sd->worker_active = false;
      sd->timer = 0;
      sd->timestamp = 0;
      pthread_mutex_lock(&cond_mutex);
      pthread_mutex_unlock(&sd->mutex);
      pthread_cond_wait(&cond, &cond_mutex);
      pthread_mutex_unlock(&cond_mutex);
      pthread_mutex_lock(&sd->mutex);
      if (sd->die) break;
      sd->worker_active = true;
      rerand = true;
      continue;
    }

    sd->worker_active = true;
    if (sd->timer > 10.) {
      sd->timer = 0.;
      rerand = true;
    }
    if (sd->pidx == -1) {
      if (rerand) {
        sd->cycadj = 0;
        while (1) {
          sd->program = fastrand_int(sd->nprs - 2);
          if (sd->bad_programs[sd->program] != 1) {
            sd->globalPM->selectPreset(sd->program, sd->bad_prog);
            break;
          }
          //sd->globalPM->selectRandom(true);
        }
        rerand = false;
        sd->bad_prog = false;
        blanks = 0;
        if (sd->bad_programs[sd->program] == 0) sd->checkforblanks = true;
      }
    } else if (sd->pidx != sd->opidx) {
      sd->globalPM->setPresetLock(true);
      sd->globalPM->selectPreset(sd->pidx);
    }

    sd->opidx = sd->pidx;

    if (sd->needs_update || sd->needs_more || !sd->got_first || sd->ncycs > 1.) {
      if (render_frame(sd)) {
        if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
          sd->rendering = false;
        }
      } else if (sd->pidx == -1) {
        if (sd->bad_prog) {
          sd->bad_programs[sd->program] = 1;
          rerand = true;
        }
      }
    }
  }
  sd->worker_active = false;
  pthread_mutex_unlock(&sd->mutex);

  if (sd->globalPM) delete(sd->globalPM);
  sd->globalPM = NULL;
  SDL_Quit();

  pthread_mutex_lock(&cond_mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&cond_mutex);

  return NULL;
}


static weed_error_t projectM_deinit(weed_plant_t *inst) {
  _sdata *sd = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sd) {
    sd->rendering = false;
    pthread_mutex_lock(&sd->mutex);
    pthread_mutex_unlock(&sd->mutex);
    if (sd->audio) {
      weed_free(sd->audio);
      sd->audio = NULL;
    }
    if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
      sd->die = true;
      pthread_mutex_lock(&cond_mutex);
      pthread_cond_signal(&cond);
      pthread_mutex_unlock(&cond_mutex);

      pthread_mutex_lock(&cond_mutex);
      pthread_cond_wait(&cond, &cond_mutex);
      pthread_mutex_unlock(&cond_mutex);
      if (sd->audio) weed_free(sd->audio);
      if (sd->bad_programs) weed_free(sd->bad_programs);
      weed_free(sd);
      weed_set_voidptr_value(inst, "plugin_internal", NULL);
      inited = 0;
    } else {
      if (sd->failed) {
        weed_free(sd);
        inited = 0;
      }
    }
  }
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
    int height = weed_channel_get_height(out_channel);
    int rowstride = weed_channel_get_stride(out_channel);
    int width = weed_channel_get_width(out_channel);
    int palette = weed_channel_get_palette(out_channel);
    int psize = pixel_size(palette);

    copies++;

    if (!inited) {
      int rc = 0;
      sd = (_sdata *)weed_calloc(1, sizeof(_sdata));
      if (!sd) return WEED_ERROR_MEMORY_ALLOCATION;

      sd->error = WEED_SUCCESS;
      sd->fbuffer = NULL;
      weed_set_voidptr_value(inst, "plugin_internal", sd);

      sd->pidx = sd->opidx = -1;

      sd->fps = PREF_FPS;
      if (weed_plant_has_leaf(inst, WEED_LEAF_FPS)) sd->fps = weed_get_double_value(inst, WEED_LEAF_FPS, NULL);
      if (weed_plant_has_leaf(inst, WEED_LEAF_TARGET_FPS)) sd->fps = weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);

      pthread_mutex_init(&sd->mutex, NULL);
      pthread_mutex_init(&sd->pcm_mutex, NULL);

      sd->nprs = 0;
      sd->prnames = NULL;
      sd->worker_ready = false;
      sd->worker_active = false;

      sd->die = sd->failed = false;
      sd->rendering = false;
      sd->width = width;
      sd->height = height;
      sd->rowstride = rowstride;
      sd->bad_programs = NULL;

      // kick off a thread to init screean and render
      pthread_create(&sd->thread, NULL, worker, sd);

      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += WORKER_TIMEOUT_SEC;

      //#define DEBUG
#ifdef DEBUG
      ts.tv_sec *= 100;
#endif

      // wait for worker thread ready
      pthread_mutex_lock(&cond_mutex);
      while (!sd->worker_ready && rc != ETIMEDOUT) {
        rc = pthread_cond_timedwait(&cond, &cond_mutex, &ts);
      }
      pthread_mutex_unlock(&cond_mutex);

      if (rc == ETIMEDOUT && !sd->worker_ready) {
        // if we timedout then die
        sd->failed = true;
      }

      if (sd->failed) {
        projectM_deinit(inst);
        return WEED_ERROR_PLUGIN_INVALID;
      }

      if (!weed_plant_has_leaf(iparams[0], WEED_LEAF_GUI)) {
        weed_plant_t *iparamgui = weed_param_get_gui(iparams[0]);
        weed_set_string_array(iparamgui, WEED_LEAF_CHOICES, sd->nprs, (char **)sd->prnames);
      }
      weed_free(iparams);

      statsd = sd;
      inited = 1;
    } else {
      sd = statsd;
      weed_set_voidptr_value(inst, "plugin_internal", sd);
    }

    sd->rendering = false;

    if (!sd->fbuffer || sd->height != height || sd->rowstride != rowstride) {
      if (!resize_buffer(sd) && sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
        projectM_deinit(inst);
        return WEED_ERROR_MEMORY_ALLOCATION;
      }
    }

    sd->audio = (float *)weed_calloc(MAX_AUDLEN * 2,  sizeof(float));
    if (!sd->audio) {
      projectM_deinit(inst);
      return WEED_ERROR_MEMORY_ALLOCATION;
    }
    sd->abufsize = 4096;

    sd->got_first = false;
    sd->width = width;
    sd->height = height;
    sd->psize = psize;
    sd->palette = palette;
    sd->rowstride = 0;
    sd->update_size = false;
    sd->update_psize = false;
    sd->needs_more = false;
    sd->needs_update = sd->set_update = false;
    sd->audio_frames = sd->audio_offs = 0;
    pcount = count = 0;
    sd->rendering = true;
    sd->tfps = 0.;
    sd->ncycs = 0.;
    sd->bad_prog = false;
    sd->timer = 0.;
    sd->timestamp = 0.;

    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
  }
  return WEED_SUCCESS;
}


static weed_error_t projectM_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  _sdata *sd = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **inparams = weed_get_in_params(inst, NULL);
  weed_plant_t *inparam = inparams[0];
  unsigned char *dst = (unsigned char *)weed_channel_get_pixel_data(out_channel);

  int width = weed_channel_get_width(out_channel);
  int height = weed_channel_get_height(out_channel);

  int palette = weed_channel_get_palette(out_channel);
  int psize = pixel_size(palette);
  int rowstride = weed_channel_get_stride(out_channel);
  bool did_update = false;
  static double ltt;
  double timer;

  weed_free(inparams);

  if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
    projectM_deinit(inst);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  if (verbosity >= WEED_VERBOSITY_DEBUG) {
    count++;
    if (count == 1 || count == 101) {
      double tt;
      int ppcount = pcount;
      pcount = 0;
      clock_gettime(CLOCK_REALTIME, &ts);
      tt = (double)(ts.tv_sec * 1000000000 + ts.tv_nsec);
      if (count == 101) {
        double period = (tt - ltt) / 1000000000.;
        if (period > 0.)
          fprintf(stderr, "projectM running at display rate of %f fps (%f), engine rendering at %f fps\n",
                  100. / period, sd->fps, ppcount / period);
        count = 1;
      }
      ltt = tt;
    }
  }

  if (sd->die) return WEED_ERROR_REINIT_NEEDED;

  while (!sd->worker_active) {
    sd->rendering = true;
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
  }

  if (width > maxwidth) width = maxwidth;
  if (height > maxheight) height = maxheight;

  if (sd->width != width || sd->height != height || sd->psize != psize || sd->rowstride != rowstride || sd->palette != palette) {
    /// we must update size / pal, this has to be done before reading the buffer
    pthread_mutex_lock(&cond_mutex);
    sd->needs_update = true;
    /// wait for worker thread to aknowledge the update request
    while (sd->needs_update) {
      pthread_cond_wait(&cond, &cond_mutex);
    }
    pthread_mutex_unlock(&cond_mutex);
    //sd->updating = true;
    /// now we can set new values
    if (sd->width != width || sd->height != height) {
      sd->height = height;
      sd->width = width;
      sd->update_size = true;
    }
    if (sd->palette != palette || sd->rowstride != rowstride || sd->psize != psize) {
      sd->update_psize = true;
      sd->psize = psize;
      sd->palette = palette;
      sd->rowstride = rowstride;
    }
    /// let the worker thread know that the values have been updated
    sd->needs_more = true;
    pthread_mutex_lock(&cond_mutex);
    sd->set_update = true;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    did_update = true;
  }

  /// get the program number:
  // ex. if nprs == 10, we have 9 programs 1 - 9 and 0 is random
  // first we shift the vals. down by 1, so -1 is random and prgs 0 - 8
  // 0 - 9, we just use the value - 1
  // else val % (nprs - 1) .e.g 9 mod 9 is 0, 10 mod 9 is 1, etc
  sd->pidx = (weed_param_get_value_int(inparam) - 1) % (sd->nprs - 1);

  if (0) {
    /// TODO - we can control the player with fake keystrokes
    projectMEvent evt;
    projectMKeycode key;
    projectMModifier mod;
    evt = PROJECTM_KEYDOWN;
    //mod=PROJECTM_KMOD_LSHIFT;
    key = PROJECTM_K_n;
    // send any keystrokes to projectM
    sd->globalPM->key_handler(evt, key, mod);
  }

  /// read the fps value from the player if we have that
  if (weed_plant_has_leaf(inst, WEED_LEAF_FPS)) sd->fps = weed_get_double_value(inst, WEED_LEAF_FPS, NULL);
  if (weed_plant_has_leaf(inst, WEED_LEAF_TARGET_FPS)) sd->tfps = weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);

  if (sd->fps) {
    sd->timer += 1. / sd->fps;
  } else {
    if (sd->timestamp > 0) sd->timer += (double)(timestamp - sd->timestamp) / 100000000.;
  }
  timer = sd->timer;
  if (sd->timer < timer) sd->timer = timer;
  sd->timestamp = timestamp;
  /// update audio

  if (in_channel) {
    /// fill the audio buffer for the following frame(s)
    int achans;
    int adlen = weed_channel_get_audio_length(in_channel);
    float **adata = (float **)weed_channel_get_audio_data(in_channel, &achans);
    pthread_mutex_lock(&sd->pcm_mutex);
    if (adlen > 0 && adata && adata[0]) {
      if (!sd->audio || (sd->abufsize < (size_t)adlen)) {
        sd->audio = (float *)weed_calloc(adlen, 4);
        if (!sd->audio) {
          sd->error = WEED_ERROR_MEMORY_ALLOCATION;
          pthread_mutex_unlock(&sd->pcm_mutex);
          projectM_deinit(inst);
          return WEED_ERROR_MEMORY_ALLOCATION;
        }
      }
      sd->abufsize = adlen;
      weed_memcpy(sd->audio, adata[0], adlen * 4);
    } else adlen = 0;
    sd->audio_frames = adlen;
    sd->audio_offs = 0;
    pthread_mutex_unlock(&sd->pcm_mutex);
    if (adata) weed_free(adata);
  }

  if (did_update) {
    /// if we updated we MUST wait for the buffer resize to finish before reading
    /// (optionally we could return WEED_ERROR_NOT_READY)
    pthread_mutex_lock(&cond_mutex);
    while (sd->needs_more) pthread_cond_wait(&cond, &cond_mutex);
    pthread_mutex_unlock(&cond_mutex);
  }

  if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
    projectM_deinit(inst);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  /// we did all the updates so...

  /// copy the buffer now, whether we have a new frame or not
  /// optionally we could return WEED_ERROR_NOT_READY if there is no new frame
  //if (!sd->got_first) return WEED_ERROR_NOT_READY;

  pthread_mutex_lock(&buffer_mutex);
  weed_memcpy(dst, sd->fbuffer, rowstride * height);
  sd->needs_more = true;
  pthread_mutex_unlock(&buffer_mutex);

  if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
    projectM_deinit(inst);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  if (access("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", R_OK)
      && access("/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf", R_OK)) {
    return NULL;
  }

  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_RGB24, WEED_PALETTE_BGRA32,
                        WEED_PALETTE_BGR24, WEED_PALETTE_END
                       };
  const char *xlist[3] = {"- Random -", "Choose...", NULL};
  weed_plant_t *in_params[] = {weed_string_list_init("preset", "_Preset", 0, xlist), NULL};
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("In audio", WEED_CHANNEL_OPTIONAL), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *filter_class = weed_filter_class_init("projectM", "salsaman/projectM authors", 1,
                               WEED_FILTER_PREF_LINEAR_GAMMA, palette_list, projectM_init,
                               projectM_process, projectM_deinit, in_chantmpls, out_chantmpls, in_params, NULL);
  weed_plant_t *gui = weed_paramtmpl_get_gui(in_params[0]);
  weed_gui_set_flags(gui, WEED_GUI_CHOICES_SET_ON_INIT);
  //weed_set_int_value(in_chantmpls[0], WEED_LEAF_MAX_AUDIO_LENGTH, 2048); // we can accept more audio since now it is buffered
  weed_set_int_value(in_params[0], WEED_LEAF_MAX, INT_MAX);
  weed_set_double_value(filter_class, WEED_LEAF_PREFERRED_FPS, PREF_FPS); // set reasonable default fps
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  verbosity = weed_get_host_verbosity(weed_get_host_info(plugin_info));
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
  statsd = NULL;
  XInitThreads();
}
WEED_SETUP_END;


WEED_DESETUP_START {
  if (inited && statsd) {
    statsd->die = true;
    if (!statsd->rendering) {
      pthread_mutex_lock(&cond_mutex);
      pthread_cond_signal(&cond);
      pthread_mutex_unlock(&cond_mutex);
    }
    pthread_join(statsd->thread, NULL);
    if (statsd->fbuffer) weed_free(statsd->fbuffer);
    if (statsd->audio) weed_free(statsd->audio);
    if (statsd->prnames) {
      for (int i = 0; i < statsd->nprs; i++) {
        free(statsd->prnames[i]);
      }
      weed_free(statsd->prnames);
    }
    if (statsd->bad_programs) weed_free(statsd->bad_programs);
    pthread_mutex_destroy(&statsd->mutex);
    pthread_mutex_destroy(&statsd->pcm_mutex);
    weed_free(statsd);
    statsd = NULL;
  }
  inited = 0;
}
WEED_DESETUP_END;

