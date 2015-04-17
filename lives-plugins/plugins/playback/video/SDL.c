// LiVES - SDL playback engine
// (c) G. Finch 2003 - 2008 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#if IS_MINGW
#include <windows.h>
#endif

#include "videoplugin.h"

#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////////

static char plugin_version[64]="LiVES SDL playback engine version 1.2";
static char error[256];

static boolean(*render_fn)(int hsize, int vsize, void **pixel_data);
static boolean render_frame_rgb(int hsize, int vsize, void **pixel_data);
static boolean render_frame_yuv(int hsize, int vsize, void **pixel_data);
static boolean render_frame_unknown(int hsize, int vsize, void **pixel_data);

static int palette_list[6];
static int mypalette;

static int clampings[2];

static boolean is_ready;

/////////////////////////////////////////////
// SDL specific stuff
#include <SDL.h>

static  SDL_Surface *screen;
static  SDL_Surface *RGBimage;
static  SDL_Overlay *overlay;
static  int ov_hsize;
static  int ov_vsize;
static  SDL_Rect *rect;
static  SDL_Event event;
static  SDLMod mod;

//////////////////////////////////////////////


static boolean my_setenv(const char *name, const char *value) {
  // ret TRUE on success
#if IS_MINGW
  return SetEnvironmentVariable(name,value);
#else
#if IS_IRIX
  int len  = strlen(name) + strlen(value) + 2;
  char *env = malloc(len);
  if (env != NULL) {
    strcpy(env, name);
    strcat(env, "=");
    strcat(env, val);
    return !putenv(env);
  }
}
#else
  return !setenv(name,value,1);
#endif
#endif
}


const char *module_check_init(void) {
  if (getenv("HAVE_SDL")==NULL&&system("which sdl-config >/dev/null 2>&1")==256) {
    snprintf(error,256,
             "\n\nUnable to find sdl-config in your path.\nPlease make sure you have SDL installed correctly to use this plugin.\nYou can override this with 'export HAVE_SDL=1'\n");
    return error;
  }

  render_fn=&render_frame_unknown;

  RGBimage=NULL;
  overlay=NULL;
  ov_vsize=ov_hsize=0;

  mypalette=WEED_PALETTE_END;
  rect=(SDL_Rect *)malloc(sizeof(SDL_Rect));

  return NULL;
}



const char *version(void) {
  return plugin_version;
}

const char *get_description(void) {
  return "The SDL plugin allows faster playback.\n";
}

uint64_t get_capabilities(int palette) {
  if (palette==WEED_PALETTE_UYVY8888) {
    return VPP_CAN_RESIZE|VPP_LOCAL_DISPLAY;
  }
  return VPP_LOCAL_DISPLAY;
}

const char *get_init_rfx(void) {
  return \
         "<define>\\n\
|1.7\\n\
</define>\\n\
<language_code>\\n\
0xF0\\n\
</language_code>\\n\
<params> \\n\
hwa|Hardware _acceleration|bool|1|0 \\n\
yuvd|YUV _direct|bool|1|0 \\n\
yuvha|_YUV hardware acceleration|bool|1|0 \\n\
dblbuf|_Double buffering|bool|1|0 \\n\
hws|Hardware _surface|bool|1|0 \\n\
fsover|Over-ride fullscreen setting (for debugging)|bool|0|0 \\n\
</params> \\n\
<param_window> \\n\
</param_window> \\n\
<onchange> \\n\
</onchange> \\n\
";
}


const int *get_palette_list(void) {
  // return palettes in order of preference, ending with WEED_PALETTE_END
  palette_list[0]=WEED_PALETTE_UYVY8888;
  palette_list[1]=WEED_PALETTE_YUYV8888;
  palette_list[2]=WEED_PALETTE_YVU420P;
  palette_list[3]=WEED_PALETTE_YUV420P;
  palette_list[4]=WEED_PALETTE_RGB24;
  palette_list[5]=WEED_PALETTE_END;
  return palette_list;
}


boolean set_palette(int palette) {
  if (palette==WEED_PALETTE_RGB24) {
    render_fn=&render_frame_rgb;
    mypalette=palette;
    return TRUE;
  } else if (palette==WEED_PALETTE_UYVY8888||palette==WEED_PALETTE_YUYV8888||palette==WEED_PALETTE_YUV420P||palette==WEED_PALETTE_YVU420P) {
    render_fn=&render_frame_yuv;
    mypalette=palette;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}


const int *get_yuv_palette_clamping(int palette) {
  if (palette==WEED_PALETTE_RGB24) clampings[0]=-1;
  else {
    clampings[0]=WEED_YUV_CLAMPING_CLAMPED;
    clampings[1]=-1;
  }
  return clampings;
}




boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  // screen size is in RGB pixels
  int hwaccel=1;
  int yuvdir=1;
  int yuvhwa=1;
  int dblbuf=1;
  int hws=1;
  int fsover=0;

  char tmp[32];

  uint32_t modeopts=0;

  if (argc>0) {
    hwaccel=atoi(argv[0]);
    yuvdir=atoi(argv[1]);
    yuvhwa=atoi(argv[2]);
    dblbuf=atoi(argv[3]);
    hws=atoi(argv[4]);
    fsover=atoi(argv[5]);
  }

  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"SDL plugin error: No palette was set !\n");
    return FALSE;
  }

  snprintf(tmp,32,"%d",yuvdir);
  my_setenv("SDL_VIDEO_YUV_DIRECT", tmp);

  snprintf(tmp,32,"%d",yuvhwa);
  my_setenv("SDL_VIDEO_YUV_HWACCEL", tmp);

  snprintf(tmp,32,"%"PRIu64,window_id);
  if (!fullscreen) my_setenv("SDL_WINDOWID", tmp);

  if (fsover) fullscreen=FALSE;

  if ((SDL_Init(SDL_INIT_VIDEO)==-1)) {
    fprintf(stderr,"SDL player : Could not initialize SDL: %s.\n", SDL_GetError());
    return FALSE;
  }

  modeopts=(SDL_HWSURFACE*hws)|(SDL_DOUBLEBUF*dblbuf)|(SDL_HWACCEL*hwaccel);

  SDL_ShowCursor(FALSE);
  screen = SDL_SetVideoMode(width, height, 24, modeopts | (fullscreen?SDL_FULLSCREEN:0) | SDL_NOFRAME);
  if (screen == NULL) {
    fprintf(stderr,"SDL player : Couldn't set %dx%dx24 video mode: %s\n", width, height,
            SDL_GetError());
    // do we need SDL_ShowCursor/SDL_quit here ?
    return FALSE;
  }

  /* Enable Unicode translation */
  SDL_EnableUNICODE(1);

  // if palette is RGB, create RGB surface the same size as the screen
  if (mypalette==WEED_PALETTE_RGB24) {
    RGBimage = SDL_CreateRGBSurface(SDL_HWSURFACE, width, height, 24,
                                    0x0000FF, 0x00FF00, 0xFF0000, 0x00);
    if (RGBimage == NULL) {
      fprintf(stderr,"SDL player: Can't create: %s\n", SDL_GetError());
      return FALSE;
    }
    return TRUE;
  }

  rect->x=rect->y=0;
  rect->h=height;
  rect->w=width;
  return TRUE;
}


boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // call the function which was set in set_palette
  return render_fn(hsize,vsize,pixel_data);
}

boolean render_frame_rgb(int hsize, int vsize, void **pixel_data) {
  // broken - crashes
  // hsize and vsize are in pixels (n-byte)
  SDL_LockSurface(RGBimage);
  memcpy(RGBimage->pixels,pixel_data[0],hsize*vsize*3);
  SDL_UnlockSurface(RGBimage);
  SDL_BlitSurface(RGBimage, NULL, screen, NULL);
  //SDL_FreeSurface(RGBimage);
  SDL_UpdateRect(screen, 0, 0, 0, 0);
  return TRUE;
}


boolean render_frame_yuv(int hsize, int vsize, void **pixel_data) {
  // hsize may be in uyvy-macropixels (2 real pixels per 4 byte macropixel !)
  uint32_t ovtype=SDL_IYUV_OVERLAY;

  if (mypalette==WEED_PALETTE_UYVY8888) {
    ovtype=SDL_UYVY_OVERLAY;
    hsize*=2;
  } else if (mypalette==WEED_PALETTE_YUYV8888) {
    ovtype=SDL_YUY2_OVERLAY;
    hsize*=2;
  } else if (mypalette==WEED_PALETTE_YVU420P) ovtype=SDL_YV12_OVERLAY;


  if ((ov_hsize!=hsize||ov_vsize!=vsize)&&(overlay!=NULL)) {
    SDL_FreeYUVOverlay(overlay);
    overlay=NULL;
  }

  if (overlay==NULL) {
    overlay=SDL_CreateYUVOverlay(hsize, vsize, ovtype, screen);
    ov_hsize=hsize;
    ov_vsize=vsize;
  }

  SDL_LockYUVOverlay(overlay);

  if (mypalette==WEED_PALETTE_UYVY||mypalette==WEED_PALETTE_YUYV) memcpy(overlay->pixels[0],pixel_data[0],hsize*vsize*2);
  else {
    memcpy(overlay->pixels[0],pixel_data[0],hsize*vsize);
    memcpy(overlay->pixels[1],pixel_data[1],hsize*vsize>>2);
    memcpy(overlay->pixels[2],pixel_data[2],hsize*vsize>>2);
  }

  SDL_UnlockYUVOverlay(overlay);
  SDL_DisplayYUVOverlay(overlay,rect);
  is_ready=TRUE;
  return TRUE;
}


boolean render_frame_unknown(int hsize, int vsize, void **pixel_data) {
  fprintf(stderr,"SDL plugin error: No palette was set !\n");
  return FALSE;
}


void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  if (mypalette==WEED_PALETTE_RGB24) {
    if (RGBimage!=NULL) {
      SDL_FreeSurface(RGBimage);
      RGBimage=NULL;
    }
  } else if (overlay!=NULL) {
    SDL_FreeYUVOverlay(overlay);
    overlay=NULL;
  }
  if (mouse_x>=0&&mouse_y>=0) {
    SDL_ShowCursor(TRUE);
    SDL_WarpMouse((int16_t)mouse_x, (int16_t)mouse_y);
  }
  SDL_Quit();
  is_ready=FALSE;
}


void module_unload(void) {
  free(rect);
}




boolean send_keycodes(keyfunc host_key_fn) {
  // poll for keyboard events, pass them back to the caller
  // return FALSE on error
  uint16_t mod_mask,scancode=0;

  if (host_key_fn==NULL) return FALSE;

  while (is_ready && SDL_PollEvent(&event)) {
    mod_mask=0;
    if (event.type==SDL_KEYDOWN||event.type==SDL_KEYUP) {
      mod=event.key.keysym.mod;

      if (mod&KMOD_CTRL) {
        mod_mask|=MOD_CONTROL_MASK;
      }
      if (mod&KMOD_ALT) {
        mod_mask|=MOD_ALT_MASK;
      }
      if (event.type==SDL_KEYDOWN) {
        if (!mod_mask) {
          scancode=event.key.keysym.unicode;
        }
        if (!scancode) {
          scancode=(uint16_t)event.key.keysym.scancode;
          mod_mask|=MOD_NEEDS_TRANSLATION;
        }
        host_key_fn(TRUE,scancode,mod_mask);
      }

      else {
        // key up - no unicode :-(
        host_key_fn(FALSE,(uint16_t)event.key.keysym.scancode,(mod_mask|MOD_NEEDS_TRANSLATION));
      }
    }
  }
  return TRUE;
}

