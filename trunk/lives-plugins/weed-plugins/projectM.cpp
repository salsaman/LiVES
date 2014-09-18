// projectM.c
// weed plugin
// (c) G. Finch (salsaman) 2014
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

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_UTILS
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional


/////////////////////////////////////////////////////////////

#include <libprojectM/projectM.hpp>
#include "projectM-ConfigFile.h"
#include "projectM-getConfigFilename.h"

#include <GL/gl.h>

#include <SDL/SDL.h>
#include "SDL_syswm.h"

#include <pthread.h>

static int

typedef struct {
  projectM *globalPM;
  GLubyte *fbuffer;
  int textureHandle;
  int width;
  int height;
  pthread_mutex_t mutex;
  pthread_t thread;
  volatile int die;
  volatile int failed;
} _sdata;

static int maxwidth,maxheight;


static void winhide() {
  SDL_SysWMinfo info;

  Atom atoms[2];
  SDL_VERSION(&info.version);
  if (SDL_GetWMInfo(&info)) {
    Window win=info.info.x11.wmwindow;
    Display *dpy=info.info.x11.display;
    info.info.x11.lock_func();
  
    atoms[0] = XInternAtom (dpy, "_NET_WM_STATE_BELOW", False);
    atoms[1] = XInternAtom (dpy, "_NET_WM_STATE_DESKTOP", False);
    XChangeProperty (dpy, win, XInternAtom (dpy,"_NET_WM_STATE",False),XA_ATOM,32,PropModeReplace, (const unsigned char *) &atoms, 2);

    XIconifyWindow(dpy,win,0);

    XFlush(dpy);
    info.info.x11.unlock_func();
  }


}



static int resize_display(int width, int height) {
  int flags = SDL_OPENGL|SDL_HWSURFACE|SDL_RESIZABLE;

  // 0 : use current bits per pixel
  if(!SDL_SetVideoMode(width, height, 0, flags)) {
      fprintf(stderr, "Video mode set failed: %s\n", SDL_GetError());
      return 1;
  }

  winhide();

  return 0;
}



static int init_display(void) {
  const SDL_VideoInfo* info;

  int defwidth=640;
  int defheight=480;

  /* First, initialize SDL's video subsystem. */
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
    fprintf(stderr, "Video initialization failed: %s\n",
	    SDL_GetError());
    return 1;
  }

  /* Let's get some video information. */
  info = SDL_GetVideoInfo();
  if (!info) {
    /* This should probably never happen. */
    fprintf(stderr, "Video query failed: %s\n",
	    SDL_GetError());
    
    return 2;
  }

  //printf("Screen Resolution: %d x %d\n", info->current_w, info->current_h);
  maxwidth = info->current_w;
  maxheight = info->current_h;

  if (defwidth>maxwidth) defwidth=maxwidth;
  if (defheight>maxheight) defheight=maxheight;

  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
	
  if (resize_display(defwidth,defheight)) return 3;

  return 0;
}



static void change_size(_sdata *sdata) {
  sdata->globalPM->projectM_resetGL(sdata->width,sdata->height);
  resize_display(sdata->width,sdata->height);
  if (sdata->fbuffer!=NULL) weed_free(sdata->fbuffer);
  sdata->fbuffer = (GLubyte *)weed_malloc( sizeof( GLubyte ) * sdata->width * sdata->height * 3 );
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
  glVertex3d(-1, 1, 0);
  glTexCoord2d(0, 0);
  glVertex3d(-1, -1, 0);
  glTexCoord2d(1, 0);
  glVertex3d(1, -1, 0);
  glTexCoord2d(1, 1);
  glVertex3d(1, 1, 0);
  glEnd();

  glDisable(GL_TEXTURE_2D);

  glMatrixMode(GL_MODELVIEW);
  glDisable(GL_DEPTH_TEST);

  // fbuffer is (BGR ?) upside down
  pthread_mutex_lock(&sdata->mutex);
  glReadPixels(0, 0, sdata->width, sdata->height, GL_RGB, GL_UNSIGNED_BYTE, sdata->fbuffer);
  pthread_mutex_unlock(&sdata->mutex);

}





static void *worker(void *data) {
  std::string config_filename = getConfigFilename();
  ConfigFile config(config_filename);

  _sdata *sd=(_sdata *)data;

  if (init_display()) {
    sd->failed=1;
    goto fail;
  }

  sd->globalPM = new projectM(config_filename);
  sd->textureHandle = sd->globalPM->initRenderToTexture();

  while (!sd->die) {
    render_frame(sd);
  }

  if (sd->globalPM!=NULL) delete(sd->globalPM);


 fail:

  SDL_Quit();

  return NULL;

}








static int projectM_init (weed_plant_t *inst) {


  int error;
  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  int width=weed_get_int_value(out_channel,"width",&error);
  int height=weed_get_int_value(out_channel,"height",&error);

  int palette=weed_get_int_value(out_channel,"current_palette",&error);

  _sdata *sd=(_sdata *)weed_malloc(sizeof(_sdata));
  if (sd==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sd->fbuffer = (GLubyte *)weed_malloc( sizeof( GLubyte ) * width * height * 3 );

  if (sd->fbuffer==NULL) {
    weed_free(sd);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sd->width=width;
  sd->height=height;

  sd->die=0;
  sd->failed=0;

  pthread_mutex_init(&sd->mutex,NULL);

  // kick off a thread to init screean and render
  pthread_create(&sd->thread,NULL,worker,sd);

  weed_set_voidptr_value(inst,"plugin_internal",sd);

  return WEED_NO_ERROR;


}


static int projectM_deinit (weed_plant_t *inst) {
  int error;
  _sdata *sd=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  sd->die=1;

  pthread_join(sd->thread,NULL);

  if (sd->fbuffer!=NULL) weed_free(sd->fbuffer);

  weed_free(sd);

  return WEED_NO_ERROR;
}




static int projectM_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;

  _sdata *sd=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  SDL_SysWMinfo info;

  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  unsigned char *dst=(unsigned char *)weed_get_voidptr_value(out_channel,"pixel_data",&error);

  int width=weed_get_int_value(out_channel,"width",&error);
  int height=weed_get_int_value(out_channel,"height",&error);

  int palette=weed_get_int_value(out_channel,"current_palette",&error);

  int rowstride=weed_get_int_value(out_channel,"rowstrides",&error);

  int widthx=width*3;

  unsigned char *end,*ptrd,*ptrs;

  register int j;

  if (sd->failed) return WEED_ERROR_PLUGIN_INVALID;

  if (sd->width!=width||sd->height!=height) {
    sd->width=width;
    sd->height=height;
    if (sd->width>maxwidth) sd->width=maxwidth;
    if (sd->height>maxheight) sd->height=maxheight;
    change_size(sd);
  }

  if (sd->fbuffer==NULL) return WEED_NO_ERROR;

  if (0) {
    projectMEvent evt;
    projectMKeycode key;
    projectMModifier mod;

    evt=PROJECTM_KEYDOWN;
    //mod=PROJECTM_KMOD_LSHIFT;
    key=PROJECTM_K_n;

    // send any keystrokes to projectM
    sd->globalPM->key_handler(evt, key, mod);
  }

  //if (palette==WEED_PALETTE_RGBA32) widthx=width*4;

  end=dst+height*widthx;
  ptrd=end-widthx;
  ptrs=sd->fbuffer;

  pthread_mutex_lock(&sd->mutex);

  // copy sd->fbuffer -> dst
  for (j=0;j<sd->height;j++) {
    weed_memcpy(ptrd,ptrs,widthx);
    ptrd-=widthx;
    ptrs+=sd->width*3;
  }

  pthread_mutex_unlock(&sd->mutex);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {

    int palette_list[]={WEED_PALETTE_RGB24,WEED_PALETTE_END};

    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE|
							      WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("projectM","salsaman/projectM authors",1,0,&projectM_init,
						      &projectM_process,&projectM_deinit,NULL,out_chantmpls,NULL,NULL);
    weed_set_double_value(filter_class,"target_fps",50.); // set reasonable default fps

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}





