// projectM.cpp
// weed plugin
// (c) G. Finch (salsaman) 2014 - 2020
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// TODO: are we segfaulting on .prjm programs ??

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

// how this works:
// - we create a hidden (fullscreen) SDL window with a gl context
//
// - we start up projectM and ask it to render to a texture
//   (the texture has to be square, so we use MAX(screen width, screen height). This is adjusted to the nxt power of two,
//   as this makes projectM render more efficiently
//
// - we draw the texture in the SDL window, resizing it to the image size
// - we read the pixels from the window into sd->fbuffer
// - we copy sd->fbuffer to the output, adjusting the rowstride to match
//   (sd->fbuffer has no rowstride padding, except in the case of RGB24, where we make it a multiple of 4 bytes)
// - since we can only draw up to the window (screen) size, if the image size > screen size we set a scaling hint for the host
//   (we also set MAXWIDTH and MAXHEIGHT for the channel, so the host can be aware of this limitation)
// - the projectM texture size is fixed, so we just vary img size and scale accordingly

// TODO - handle screen size changes (we need to completely reset the filter)

#define FONT_DIR1 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#define FONT_DIR2 "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf"

#define NEED_AUDIO
#define NEED_PALETTE_UTILS
#define NEED_RANDOM

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

//static int next_pot(int val) {for (int i = 2;; i *= 2) if (i >= val) return i;}
static int next_pot(int val) {return val;}

static int verbosity = WEED_VERBOSITY_ERROR;
#define WORKER_TIMEOUT_SEC 30 /// how long to wait for worker thread startup
#define MAX_AUDLEN 2048 /// this is defined by projectM itself, increasing the value above 2048 will only result in jumps in the audio
#define DEF_SENS 2.5 /// beat sensitivity 0. -> 5.  (lower is more sensitive); too high -> less dynamic, too low - nothing w. silence
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

#define PREF_FPS 30.
#define MESHSIZE 32.

#define AUTO_PRESET_TIME 20.

static Display *dpy;

static int copies = 0;

static const GLushort RGBA_TO_ARGB[8] =  {0, 1, 1, 2, 2, 3, 3, 0};
static const GLushort RGBA_TO_BGRA[8] = {0, 2, 1, 1, 2, 0, 3, 3};
static const GLushort RGB_TO_BGR[6] = {0, 2, 1, 1, 2, 0};

static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timespec ts;

static int count = 0;
static int pcount = 0;
static int blanks = 0, nonblanks = 0;

static int scrwidth, scrheight;
static int imgwidth, imgheight;

static bool rerand = true;

static void finalise(void);

// class myProjectM : public projectM {

// public:
//   myProjectM(std::string config_file, int flags = FLAG_NONE) :
//     projectM(config_file, flags) {
//   } ;

//   myProjectM(Settings settings, int flags = FLAG_NONE) :
//     projectM(settings, flags) {
//   };

//   void presetSwitchedEvent(bool isHardCut, unsigned int index) const {
//     // after switching, select the next program and queue it
//     rerand = true;
//   }
// };


typedef struct {
  projectM *globalPM;
  GLubyte *fbuffer;
  size_t fbuffer_size;
  int textureHandle;
  int texsize;
  int palette, psize;
  int rowstride;
  volatile bool worker_ready;
  volatile bool worker_active;
  int pidx, opidx;
  int nprs;
  int ctime;
  char **prnames;
  uint8_t *bad_programs, *good_programs;
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
  bool did_update;
  bool got_first;
#ifdef HAVE_SDL2
  SDL_Window *win;
  SDL_GLContext glCtx;
#endif
  weed_error_t error;
  double timer;
  weed_timecode_t timestamp;
  volatile bool busy;
  volatile bool silent;
  bool screen_inited;
} _sdata;

static bool resize_buffer(_sdata *sd);

static _sdata *statsd = NULL;

static bool inited = false;

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
      std::cerr << "{ProjectM plugin: Video mode set failed: " <<  SDL_GetError() << std::endl;
    return 1;
  }

  winhide();

  return 0;
}
#endif


#if 0
static int change_size(_sdata *sdata) {
  int ret = 0;
  int newsize = scrwidth;
  if (scrheight > newsize) newsize = scrheight;
  if (scrwidth >= scrheight)
    newsize = scrwidth;//next_pot(scrwidth);
  else
    newsize = scrheight;//next_pot(scrheight);

  // if (newsize > maxwidth) newsize = maxwidth;
  // if (newsize > maxheight) newsize = maxheight;

  std::cerr << "CHANGED SIZE to " << scrwidth << " X " << scrheight << std::endl;

#ifdef HAVE_SDL2
  SDL_SetWindowSize(sdata->win, scrwidth, scrheight);
#else
  ret = resize_display(scrwidth, scrheight);
#endif
  //if (sdata->worker_ready)

  sdata->texsize = newsize;

  if (sdata->worker_ready) {
    // does not work, only the initial textSize in settings seems to have any bearing
    //sdata->globalPM->projectM_resetTextures();

    //sdata->globalPM->projectM_resetGL(scrwidth, scrheight);

    // sdata->textureHandle = sdata->globalPM->initRenderToTexture();
    // sdata->globalPM->changeTextureSize(newsize);
  }

  //if (sdata->worker_ready) sdata->globalPM->projectM_resetGL(scrwidth, scrheight);

  return ret;
}
#endif

static int setup_display(void) {
  XInitThreads();
  dpy = XOpenDisplay(NULL);

  /* First, initialize SDL's video subsystem. */
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    if (verbosity >= WEED_VERBOSITY_CRITICAL)
      fprintf(stderr, "{ProjectM plugin: Video initialization failed: %s\n",
              SDL_GetError());
    return 1;
  }
  //std::cerr << "worker initscr 2" << std::endl;

  /* Let's get some video information. */
#ifdef HAVE_SDL2
  SDL_Rect rect;
  SDL_GetDisplayBounds(0, &rect);
  scrwidth = rect.w;
  scrheight = rect.h;
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
  scrwidth = info->current_w;
  scrheight = info->current_h;
#endif

  //std::cerr << "worker initscr 3" << std::endl;

  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, USE_DBLBUF);

  SDL_GL_SetSwapInterval(0);
  return 0;
}


static int init_display(_sdata *sd) {
  //std::cerr << "worker initscr" << std::endl;

  if (!sd->screen_inited) {
#ifdef HAVE_SDL2
    sd->win = SDL_CreateWindow("projectM", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, scrwidth, scrheight,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_FULLSCREEN
			       | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_BORDERLESS);
    //std::cerr << "worker initscr 5" << std::endl;
    sd->glCtx = SDL_GL_CreateContext(sd->win);
#else
    if (resize_display(scrwidth, scrheight)) return 3;
#endif
  }
  sd->screen_inited = true;
  return 0;
}


static bool resize_buffer(_sdata *sd) {
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
  sd->fbuffer = (GLubyte *)weed_calloc(sizeof(GLubyte) * sd->rowstride
				       * scrheight / align + align - 1, align);
  if (!sd->fbuffer) return false;
  sd->fbuffer_size = (sizeof(GLubyte) * sd->rowstride
		      * scrheight / align + align - 1) / align;
  return true;
}


static int render_frame(_sdata *sd) {
  float yscale, xscale;
  int maxwidth = imgwidth, maxheight = imgheight;
  bool checked_audio = false;
  bool do_unlock = false;

  if (maxwidth > scrwidth) maxwidth = scrwidth;
  if (maxheight > scrheight) maxheight = scrheight;

  xscale = (float)maxwidth / sd->texsize * 2.;
  yscale = (float)maxheight / sd->texsize * 2.;

#ifdef HAVE_SDL2
  SDL_GL_MakeCurrent(sd->win, sd->glCtx);
#endif

  if (sd->needs_update || !sd->got_first) {
    if (sd->needs_update) {
      // signal to render thrd, we have noticed needs_update
      pthread_mutex_lock(&cond_mutex);
      sd->needs_update = false;
      pthread_cond_signal(&cond);
      pthread_mutex_unlock(&cond_mutex);
      // signal to render thrd, we have noticed needs_update
      // other thread will now set the new details, and set sd->set_update
      // when done
      pthread_mutex_lock(&cond_mutex);
      while (!sd->set_update) {
        pthread_cond_wait(&cond, &cond_mutex);
      }
      pthread_mutex_unlock(&cond_mutex);

      // render thread will wait again until the buffer is resized

      // reset set_update
      sd->set_update = false;
      if (sd->update_size) {
        glFlush();
        resize_buffer(sd);
        //change_size(sd);
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

    // render thread can continue
    pthread_mutex_lock(&cond_mutex);
    sd->did_update = true;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);

    if (sd->update_psize || !sd->got_first) {
      sd->update_psize = false;
      // define how we will map screen to sd->fbuffer
      glPixelStorei(GL_UNPACK_ROW_LENGTH, sd->rowstride / sd->psize);
      if (sd->psize == 4) {
        if (sd->palette == WEED_PALETTE_BGRA32) {
          glPixelMapusv(GL_PIXEL_MAP_I_TO_I, 8, RGBA_TO_BGRA);
        } else if (sd->palette == WEED_PALETTE_ARGB32) {
          glPixelMapusv(GL_PIXEL_MAP_I_TO_I, 8, RGBA_TO_ARGB);
        }
      } else {
        if (sd->palette == WEED_PALETTE_BGR24) {
          glPixelMapusv(GL_PIXEL_MAP_I_TO_I, 6, RGB_TO_BGR);
        }
      }
    }
    sd->needs_more = true;
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glViewport(0, 0, imgwidth, imgheight);

#define NORM_AUDIO

  if (sd->needs_more || sd->audio_frames > sd->audio_offs) {
    size_t audlen = MAX_AUDLEN;
#ifdef NORM_AUDIO
    float maxvol = 0., myvol;
    size_t i;
#endif
    pthread_mutex_lock(&sd->pcm_mutex);
    if (sd->audio_frames - sd->audio_offs < audlen)
      audlen = sd->audio_frames - sd->audio_offs;
    if (audlen) {
#ifdef NORM_AUDIO
      for (i = 0; i < audlen; i++) {
        if ((myvol = fabsf(sd->audio[sd->audio_offs + i]) > maxvol)) maxvol = myvol;
        if (maxvol > .8) break;
      }
      if (i == audlen && maxvol > 0.05 && maxvol < 1.) {
        for (i = 0; i < audlen; i++) {
          sd->audio[sd->audio_offs + i] /= maxvol;
        }
      }
#endif
      if (maxvol == 0.) sd->silent = true;
      else {
        sd->silent = false;
        sd->globalPM->pcm()->addPCMfloat(sd->audio + sd->audio_offs, audlen);
        sd->audio_offs += audlen;
      }
    }
    pthread_mutex_unlock(&sd->pcm_mutex);
    checked_audio = true;
    if (sd->ncycs > 1.) sd->cycadj = -(int)(sd->ncycs);
  }

  XLockDisplay(dpy);
  sd->globalPM->renderFrame();
  XUnlockDisplay(dpy);
  pcount++;

  if ((sd->needs_more && checked_audio) || (sd->pidx == -1 && sd->checkforblanks)) {
    glMatrixMode(GL_PROJECTION);
    glOrtho(0, 1, 0, 1, -1, 1);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

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
    glReadPixels(0, 0, sd->rowstride / sd->psize, imgheight, sd->psize == 4
                 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, sd->fbuffer);

#define BLANK_LIM 8
#define PCOUNT_LIM (1000 * BLANK_LIM)
#define ALPHA_MASK 0XF8F8F800F8F8F800

    if (sd->pidx == -1 && sd->checkforblanks) {
      /// check for blank frames: if the first BLANK_LIM frames from a new program are all blank, mark the program as "bad"
      /// and pick another (not sure why the blank frames happen, but generally if the first two come back blank, so do all the
      /// rest. Possibly we need an image texture to load, which we don't have; more investigation needed).
      uint64_t *p = (uint64_t *)sd->fbuffer;
      int pclim = PCOUNT_LIM / (1 + blanks + nonblanks), pcount = 0;
      int i = sd->fbuffer_size;
      if (sd->psize == 4) {
	i >>= 3;
	while (--i && (!(*p & ALPHA_MASK) || ++pcount < pclim)) p++;
      }
      else while (--i && (!(*p & 0XF8) || ++pcount < pclim)) p++;
      if (!i) {
	if (++blanks >= BLANK_LIM) {
	  sd->checkforblanks = false;
	  sd->bad_prog = true;
	}
      }
      else {
	blanks--;
	if (++nonblanks >= BLANK_LIM) {
	  sd->checkforblanks = false;
	  sd->bad_programs[sd->program] = 2;
	}
      }
    }
    sd->needs_more = false;
  }
  else {
    if (sd->ncycs > 1.) {
      sd->ncycs--;
      if (sd->ncycs < 1. && sd->cycadj < 0) sd->cycadj++;
    }
  }

  pthread_mutex_unlock(&buffer_mutex);
  pthread_mutex_lock(&cond_mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&cond_mutex);

  sd->got_first = true;

  if (sd->fps > 0.) sd->ncycs += sd->tfps / sd->fps - 1.;
  sd->ncycs += sd->cycadj;
  if (sd->ncycs < 0.) sd->ncycs = 0.;

  return 0;
}


static void do_exit(void) {
  //pthread_mutex_lock(&cond_mutex);
  //pthread_cond_signal(&cond);
  //pthread_mutex_unlock(&cond_mutex);
  //std::cerr << "EXIT !!" << std::endl;
  if (inited && statsd) {
    statsd->die = true;
  }
}


static void *worker(void *data) {
  std::string prname;
  projectM::Settings settings;
  _sdata *sd = (_sdata *)data;
  float hwratio;
  int nprs;

  //  int new_stdout, new_stderr;
  //std::cerr << "worker start" << std::endl;

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
  hwratio = (float)scrheight / (float)scrwidth;
  //std::cerr << "worker start 2" << std::endl;
  settings.windowWidth = scrwidth;
  settings.windowHeight = scrheight;

  //std::cerr << "RESET size to " << scrwidth << " X " << scrheight << std::endl;

  settings.meshX = scrwidth / MESHSIZE;
  settings.meshY = ((int)(settings.meshX * hwratio + 1) >> 1) << 1;
  settings.fps = sd->fps;
  settings.smoothPresetDuration = 20.;
  settings.presetDuration = 20; /// ignored
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

  //std::cerr << "SIZE is " << scrwidth << " X " << scrheight << std::endl;

  // setting to pot seems to speed things up
  if (scrwidth >= scrheight)
    settings.textureSize = sd->texsize = next_pot(scrwidth);
  else
    settings.textureSize = sd->texsize = next_pot(scrheight);

  if (sd->failed) {
    // can happen if the host is overloaded and the caller timed out
    SDL_Quit();
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    return NULL;
  }

  //std::cerr << "worker start 3" << std::endl;

  // can fail here
  sd->globalPM = new projectM(settings, 0);
  //std::cerr << "worker start 3a" << std::endl;
  sd->textureHandle = sd->globalPM->initRenderToTexture();
  //std::cerr << "worker start 3b" << std::endl;
  sd->globalPM->setPresetLock(true);

  //std::cerr << "worker start 4" << std::endl;

  nprs = sd->globalPM->getPlaylistSize() + 1;
  sd->checkforblanks = true;
  sd->cycadj = 0;

  sd->prnames = (char **volatile)weed_calloc(nprs, sizeof(char *));
  if (!sd->prnames) sd->error = WEED_ERROR_MEMORY_ALLOCATION;
  else sd->bad_programs = (uint8_t *)weed_calloc(nprs - 1, 1);
  if (!sd->bad_programs) sd->error = WEED_ERROR_MEMORY_ALLOCATION;
  sd->good_programs = (uint8_t *)weed_calloc(nprs - 1, 1);
  if (!sd->good_programs) sd->error = WEED_ERROR_MEMORY_ALLOCATION;
  if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) sd->rendering = false;
  else {
    int ff = 1;
    sd->prnames[0] = strdup("- Random -");
    sd->nprs = 1;
    for (int i = 1; i < nprs; i++) {
      if (sd->globalPM->getPresetURL(i - 1).substr(sd->globalPM->getPresetURL(i - 1)
						   .find_last_of(".") + 1) == "prjm") {
	sd->bad_programs[i] = 1;
	continue;
      }
      sd->good_programs[ff - 1] = i;
      sd->prnames[ff++] = strdup((sd->globalPM->getPresetName(i - 1)).c_str());
      sd->nprs++;
    }
  }
  //std::cerr << "worker start 5" << std::endl;

  if (sd->failed) {
    // can happen if the host is overloaded and the caller timed out
    SDL_Quit();
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    return NULL;
  }

  //sd->globalPM->projectM_resetGL(scrwidth, scrheight);

  pthread_mutex_lock(&sd->mutex);
  sd->worker_ready = true;
  //std::cerr << "worker start 6" << std::endl;

  // tell main thread we are ready
  pthread_mutex_lock(&cond_mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&cond_mutex);

  rerand = true;
  //std::cerr << "worker start 7" << std::endl;

  while (!sd->die) {
    //std::cerr << "worker start 8" << std::endl;
    if (!sd->rendering) {
      //std::cerr << "worker start 9" << std::endl;
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
    if (sd->ctime > 0. && sd->timer > (double)sd->ctime) {
      sd->timer = 0.;
      rerand = true;
    }

#define CHECKED_BIAS 2 // higher values prefer known and tested programs

    if (sd->pidx == -1) {
      if (rerand) {
        sd->busy = true;
        sd->cycadj = 0;
        for (int rr = 0; rr < CHECKED_BIAS; rr++) {
          sd->program = fastrnd_int(sd->nprs - 2);
	  sd->program = sd->good_programs[sd->program];

          // make it N times more likely to select a known good program than an untested one
          // values can be: 0 - unchecked, 1 - known bad, 2 - known good, 3 - bad if silent
          if (sd->bad_programs[sd->program] == 2) break;
          if (sd->bad_programs[sd->program] < 2 || (sd->silent && sd->bad_programs[sd->program] == 3)) {
            if (sd->bad_programs[sd->program]) rr--;
            continue;
          }
        }
        sd->globalPM->selectPreset(sd->program, sd->bad_prog || sd->bad_programs[sd->program] == 0);

        // unfortunately queuePreset seems to be only available in certain versions,
        // otherwise we could get a better effect by queuing the next preset and allowing projectM
        // to do the change
        //sd->globalPM->queuePreset(sd->program);
        rerand = false;
        sd->bad_prog = false;
        blanks = nonblanks = 0;
        if (sd->bad_programs[sd->program] == 0) sd->checkforblanks = true;
        sd->busy = false;
      }
    } else if (sd->pidx != sd->opidx) {
      sd->globalPM->setPresetLock(true);
      sd->globalPM->selectPreset(sd->pidx);
    }

    if (sd->die) break;
    sd->opidx = sd->pidx;
    //std::cerr << "worker start 9" << std::endl;

    if (sd->needs_update || sd->needs_more || !sd->got_first || sd->ncycs > 1.) {
      if (render_frame(sd)) {
        if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
          sd->rendering = false;
        }
      }
      else if (sd->pidx == -1) {
        if (sd->bad_prog) {
	  if (sd->bad_programs[sd->program] != 1 && !(sd->silent
						      && sd->bad_programs[sd->program] == 3)) {
	    if (sd->silent) sd->bad_programs[sd->program] = 3;
	    else sd->bad_programs[sd->program] = 1;
	  }
          rerand = true;
        }
      }
    }
    //std::cerr << "worker start 14" << std::endl;
  }

  //std::cerr << "worker start 44" << std::endl;
  sd->needs_more = false;
  sd->worker_active = false;
  pthread_mutex_unlock(&sd->mutex);

  SDL_GL_DeleteContext(sd->glCtx);

  if (sd->globalPM) delete (sd->globalPM);
  sd->globalPM = NULL;
  //SDL_Quit();

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
      if (sd->bad_programs) weed_free(sd->bad_programs);
      weed_free(sd);
      weed_set_voidptr_value(inst, "plugin_internal", NULL);
      inited = false;
    } else {
      if (sd->failed) {
        weed_free(sd);
      }
    }
    copies--;
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t projectM_init(weed_plant_t *inst) {
  if (access(FONT_DIR1, R_OK) && access(FONT_DIR2, R_OK)) {
    if (verbosity >= WEED_VERBOSITY_CRITICAL)
      std::cerr << "{ProjectM plugin: could not access font directory, tried:" << std::endl
                << FONT_DIR1 << std::endl << FONT_DIR2 << std::endl;
    return WEED_ERROR_FILTER_INVALID;
  }
  if (copies >= 1) {
    if (verbosity >= WEED_VERBOSITY_CRITICAL)
      std::cerr << "{ProjectM plugin: cannot run multiple instances." << std::endl;
    return WEED_ERROR_TOO_MANY_INSTANCES;
  } else {
#ifdef HAVE_SDL2
    SDL_Window *win;
    SDL_GLContext glCtx;
    bool reinit = false;
#endif
    _sdata *sd;
    weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
    int height = weed_channel_get_height(out_channel);
    int width = weed_channel_get_width(out_channel);
    int palette = weed_channel_get_palette(out_channel);
    int psize = pixel_size(palette);
    int xrowstride = width * psize;
    int64_t rseed = weed_get_int64_value(inst, WEED_LEAF_RANDOM_SEED, NULL);

    copies++;

    // host can set rseed to a random value and then set it ro same again for repeatable
    // RNG
    fastrand(rseed);

    if (inited) {
      //std::cerr << "already inited" << std::endl;
      sd = statsd;
      weed_set_voidptr_value(inst, "plugin_internal", sd);
      copies--;
      if (imgwidth != width || imgheight != height || xrowstride != sd->rowstride) {
        if (imgheight != height || xrowstride != sd->rowstride) {
          sd->rowstride = xrowstride;
          imgheight = height;
          resize_buffer(sd);
        }
	imgwidth = width;
      }
    } else {
      weed_plant_t **iparams = weed_get_in_params(inst, NULL);
      int rc = 0;
      sd = (_sdata *)weed_calloc(1, sizeof(_sdata));
      if (!sd) return WEED_ERROR_MEMORY_ALLOCATION;
      //std::cerr << "resetting" << std::endl;

      sd->error = WEED_SUCCESS;
      weed_set_voidptr_value(inst, "plugin_internal", sd);

      sd->pidx = sd->opidx = -1;

      sd->tfps = PREF_FPS;
      if (weed_plant_has_leaf(inst, WEED_LEAF_FPS)) sd->fps = weed_get_double_value(inst, WEED_LEAF_FPS, NULL);
      if (weed_plant_has_leaf(inst, WEED_LEAF_TARGET_FPS)) sd->tfps = weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);

      pthread_mutex_init(&sd->mutex, NULL);
      pthread_mutex_init(&sd->pcm_mutex, NULL);

      sd->nprs = 0;
      sd->prnames = NULL;
      sd->worker_ready = false;
      sd->worker_active = false;
      sd->timer = 0.;
      sd->timestamp = 0.;

      sd->die = sd->failed = false;
      sd->rendering = false;

      imgwidth = width;
      imgheight = height;
      sd->rowstride = xrowstride;

      //std::cerr << "width " << width << " X " << rowstride << "ht " << height << std::endl;
      sd->bad_programs = NULL;

      // kick off a thread to init screean and render
      pthread_create(&sd->thread, NULL, worker, sd);

      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += WORKER_TIMEOUT_SEC;

#define DEBUG
#ifdef DEBUG
      ts.tv_sec *= 100;
#endif

      // wait for worker thread ready
      //std::cerr << "Waiting for worker_ready" << std::endl;
      pthread_mutex_lock(&cond_mutex);
      while (!sd->worker_ready && rc != ETIMEDOUT) {
        rc = pthread_cond_timedwait(&cond, &cond_mutex, &ts);
      }
      pthread_mutex_unlock(&cond_mutex);

      if (rc == ETIMEDOUT && !sd->worker_ready) {
        // if we timedout then die
        //std::cerr << "timed out Waiting for worker_ready" << std::endl;
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

      // if (!sd->fbuffer || scrheight != height || sd->rowstride != rowstride) {
      // 	if (!resize_buffer(sd) && sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
      // 	  projectM_deinit(inst);
      // 	  return WEED_ERROR_MEMORY_ALLOCATION;
      // 	}
      // }

      resize_buffer(sd);
      //change_size(sd);

      statsd = sd;
      inited = true;
    }

    //sd->rendering = false;

    sd->audio = (float *)weed_calloc(MAX_AUDLEN * 2, sizeof(float));
    if (!sd->audio) {
      projectM_deinit(inst);
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    sd->abufsize = 4096;

    sd->got_first = false;
    sd->psize = psize;
    sd->palette = palette;

    sd->update_size = false;
    sd->update_psize = false;
    sd->needs_more = false;
    sd->needs_update = false;
    sd->set_update = sd->did_update = false;
    sd->audio_frames = MAX_AUDLEN;
    sd->audio_offs = 0;
    pcount = count = 0;
    //sd->tfps = 0.;
    sd->ncycs = 0.;
    sd->bad_prog = false;
    sd->busy = false;
    sd->silent = false;
    sd->rendering = true;

    if (sd->pidx == -1) rerand = true;

    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
  }
  return WEED_SUCCESS;
}


static weed_error_t projectM_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  static double ltt;
  _sdata *sd = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **inparams = weed_get_in_params(inst, NULL);
  int ctime = weed_param_get_value_int(inparams[1]);
  unsigned char *dst = (unsigned char *)weed_channel_get_pixel_data(out_channel);

  int width = weed_channel_get_width(out_channel);
  int height = weed_channel_get_height(out_channel);

  int palette = weed_channel_get_palette(out_channel);
  int psize = pixel_size(palette);
  int rowstride = weed_channel_get_stride(out_channel), xrowstride = width * psize;
  int pidx = weed_param_get_value_int(inparams[0]);
  bool did_update = false;
  double timer;

  weed_free(inparams);

  if (!sd) {
    projectM_init(inst);
    sd = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  }

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

  //std::cerr << "pm size " << width << " X " << height << std::endl;

  if (!sd->worker_active) {
    sd->rendering = true;
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    goto retnull;
  }

  //std::cerr << scrwidth << " X " << scrheight << " and " << width << " X " << height << std::endl;
  sd->psize = psize;
  imgwidth = width;

  if (imgheight != height || sd->rowstride != xrowstride || palette != sd->palette) {
    // std::cerr << imgwidth << " X " << imgheight << " and " << width << " X " << height
    //           << " featuring " << xrowstride << " against " << sd->rowstride
    //           << " and also " << sd->psize << " vs " << psize << " with " << sd->palette << " for " << palette << std::endl;
    /// we must update size / pal, this has to be done before reading the buffer
    if (sd->busy) {
      fprintf(stderr, "BUSY\n");
      goto retnull;
    }
    pthread_mutex_lock(&cond_mutex);
    sd->needs_update = true;
    /// wait for worker thread to acknowledge the update request
    // this is fine since we only need to ensure that it has noticed before writing to the buffer
    while (sd->needs_update) {
      pthread_cond_wait(&cond, &cond_mutex);
    }
    pthread_mutex_unlock(&cond_mutex);

    // so, worker thread has seen needs_update and reset it, and cond_signalled

    //sd->updating = true;
    /// now we can set new values
    if (sd->rowstride != xrowstride) {
      // force sd->fbuffer resize
      imgheight = height;
      sd->rowstride = xrowstride;
      sd->update_size = true;
    }

    if (palette != sd->palette) {
      // force pixel mapping update
      sd->update_psize = true;
      sd->palette = palette;
    }

    /// let the worker thread know that the values have been updated
    sd->needs_more = true;
    pthread_mutex_lock(&cond_mutex);
    sd->set_update = true;
    sd->did_update = false;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&cond_mutex);
    did_update = true;
  }
  else {
    if (sd->busy) {
      fprintf(stderr, "BUSY\n");
      goto copytodest;
    }
  }
  if (sd->busy) {
    fprintf(stderr, "BUSY\n");
    goto retnull;
  }

  /// get the program number:
  // ex. if nprs == 10, we have 9 programs 1 - 9 and 0 is random
  // first we shift the vals. down by 1, so -1 is random and prgs 0 - 8
  // 0 - 9, we just use the value - 1
  // else val % (nprs - 1) .e.g 9 mod 9 is 0, 10 mod 9 is 1, etc

  sd->pidx = (pidx - 1) % sd->nprs;

  if (ctime == -1) ctime = AUTO_PRESET_TIME;
  sd->ctime = ctime;

  // if (sd->pidx != -1) {
  //   sd->globalPM->setPresetLock(true);
  // }
  // else {
  //   sd->globalPM->setPresetLock(false);
  // }

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
    if (sd->timestamp > 0) sd->timer += (double)(timestamp - sd->timestamp)
                                          / (double)WEED_TICKS_PER_SECOND;
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
        if (sd->audio) weed_free(sd->audio);
        sd->audio = (float *)weed_calloc(adlen, 4);
        if (!sd->audio) {
	  sd->abufsize = 0;
          sd->error = WEED_ERROR_MEMORY_ALLOCATION;
          pthread_mutex_unlock(&sd->pcm_mutex);
          projectM_deinit(inst);
          return WEED_ERROR_MEMORY_ALLOCATION;
        }
	sd->abufsize = adlen;
      }
      weed_memcpy(sd->audio, adata[0], adlen * 4);
    } else adlen = 0;

    if (verbosity >= WEED_VERBOSITY_DEBUG)
      fprintf(stderr, "copied %f vs %f len %d\n", adlen ? sd->audio[adlen >> 1] : 0.,
	      adata[0] ? adata[0][adlen >>1] : 0., adlen);
    sd->audio_frames = adlen;
    sd->audio_offs = 0;
    pthread_mutex_unlock(&sd->pcm_mutex);
    if (adata) weed_free(adata);
  }

  if (did_update) {
    /// if we updated we MUST wait for the buffer resize to finish before reading
    /// (optionally we could return WEED_ERROR_NOT_READY)
    pthread_mutex_lock(&cond_mutex);
    while (!sd->did_update) pthread_cond_wait(&cond, &cond_mutex);
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

copytodest:
  pthread_mutex_lock(&buffer_mutex);
  if (height > scrheight) height = scrheight;
  for (int yy = 0; yy < height; yy++) {
    weed_memcpy(&dst[yy * rowstride], &sd->fbuffer[yy * xrowstride], xrowstride);
  }

  sd->needs_more = true;
  pthread_mutex_unlock(&buffer_mutex);

  if (sd->error == WEED_ERROR_MEMORY_ALLOCATION) {
    projectM_deinit(inst);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  if (1) {
    weed_plant_t *gui = weed_channel_get_gui(out_channel);
    int xgap = 0, ygap = 0;
    if (imgwidth > scrwidth) xgap = (imgwidth - scrwidth) >> 1;
    if (imgheight > scrheight) ygap = (imgheight - scrheight) >> 1;
    weed_set_int_value(gui, WEED_LEAF_BORDER_TOP, ygap);
    weed_set_int_value(gui, WEED_LEAF_BORDER_BOTTOM, ygap);
    weed_set_int_value(gui, WEED_LEAF_BORDER_LEFT, xgap);
    weed_set_int_value(gui, WEED_LEAF_BORDER_RIGHT, xgap);
  }

  return WEED_SUCCESS;

 retnull:
  return WEED_ERROR_NOT_READY;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_RGB24, WEED_PALETTE_BGRA32,
                        WEED_PALETTE_BGR24, WEED_PALETTE_END};

  const char *xlist[3] = {"- Random -", "Choose...", NULL};
  weed_plant_t *in_params[] = {weed_string_list_init("preset", "_Preset", 0, xlist),
                               weed_integer_init("ctime", "_Random pattern hold time (seconds - 0 for never change, -1 for auto changes)",
						 -1, -1, 100000), NULL};
  weed_plant_t *in_chantmpls[] = {weed_audio_channel_template_init("In audio", WEED_CHANNEL_OPTIONAL), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};

  int flags = WEED_FILTER_PREF_LINEAR_GAMMA;

  weed_plant_t *filter_class = weed_filter_class_init("projectM", "salsaman/projectM authors", 1, flags, palette_list,
                               projectM_init, projectM_process, projectM_deinit,
                               in_chantmpls, out_chantmpls, in_params, NULL);
  weed_plant_t *gui = weed_paramtmpl_get_gui(in_params[0]);

  weed_gui_set_flags(gui, WEED_GUI_CHOICES_SET_ON_INIT);
  weed_set_int_value(in_params[0], WEED_LEAF_MAX, INT_MAX);
  weed_set_double_value(filter_class, WEED_LEAF_PREFERRED_FPS, PREF_FPS); // set reasonable default fps
  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  verbosity = weed_get_host_verbosity(weed_get_host_info(plugin_info));

  setup_display();

  weed_set_int_value(out_chantmpls[0], WEED_LEAF_MAXWIDTH, scrwidth);
  weed_set_int_value(out_chantmpls[0], WEED_LEAF_MAXHEIGHT, scrheight);

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;


static void finalise(void) {
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
    if (statsd->good_programs) weed_free(statsd->good_programs);
    // pthread_mutex_destroy(&statsd->mutex);
    // pthread_mutex_destroy(&statsd->pcm_mutex);
    weed_free(statsd);
    statsd = NULL;
  }
  inited = false;
}


WEED_DESETUP_START {
  if (statsd) SDL_GL_DeleteContext(statsd->glCtx);
  finalise();
  SDL_Quit();
  XCloseDisplay(dpy);
}
WEED_DESETUP_END;

