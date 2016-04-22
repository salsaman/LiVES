// LiVES - openGL playback engine
// (c) G. Finch 2012 - 2014 <salsaman@gmail.com>
// (c) OpenGL effects by Antti Silvast, 2012 <antti.silvast@iki.fi>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>


#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../../../libweed/weed.h"
#include "../../../../libweed/weed-effects.h"
#include "../../../../libweed/weed-palettes.h"
#endif

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h>
#else
#include "../../../../libweed/weed-plugin.h"
#endif

#include "../../../../lives-plugins/weed-plugins/weed-utils-code.c"
#include "../../../../lives-plugins/weed-plugins/weed-plugin-utils.c"


////////////////////////////////////////////////////////////////////////////////////

static char plugin_version[64]="LiVES openGL playback engine version 1.1";
static char error[256];

static boolean (*render_fn)(int hsize, int vsize, void **pixel_data, void **return_data);
static boolean render_frame_rgba (int hsize, int vsize, void **pixel_data, void **return_data);
static boolean render_frame_unknown (int hsize, int vsize, void **pixel_data, void **return_data);

static int palette_list[5];
static int mypalette;

#include <math.h> // for sin and cos

#include <sys/time.h> // added to sync to ticks! -AS
#include <time.h> // added to sync to ticks! -AS

#include <pthread.h>

/////////////////////////////////////////////
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <GLee.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

typedef struct {
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
} MotifWmHints, MwmHints;

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)

static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_ADD;
static Atom XA_NET_WM_STATE_MAXIMIZED_VERT;
static Atom XA_NET_WM_STATE_MAXIMIZED_HORZ;
static Atom XA_NET_WM_STATE_FULLSCREEN;
static Atom XA_WIN_LAYER;

static Display *dpy;

static Window xWin;
static GLXWindow glxWin;
static GLXContext context;


static boolean swapFlag = TRUE;
static boolean is_direct;
static boolean pbo_available;
static boolean is_ext;

static volatile boolean playing;
static volatile boolean rthread_ready;
static volatile boolean has_texture;
static volatile boolean has_new_texture;
static volatile boolean return_ready;

static volatile uint8_t *texturebuf;
static volatile uint8_t *retdata;
static volatile uint8_t *retbuf;

static int m_WidthFS;
static int m_HeightFS;

static int window_width;
static int window_height;

static int mode=0;
static int dblbuf=1;
static int nbuf=32;
static boolean fsover=FALSE;
static boolean use_pbo=FALSE;

static float rquad;

static pthread_t rthread;
static pthread_mutex_t rthread_mutex;
static pthread_mutex_t dpy_mutex;

static volatile uint32_t imgWidth;
static volatile uint32_t imgHeight;

static void *render_thread_func(void *data);

static boolean WaitForNotify( Display *dpy, XEvent *event, XPointer arg ) {
  return (event->type == MapNotify) && (event->xmap.window == (Window) arg);
}

static GLenum m_TexTarget=GL_TEXTURE_2D;

static float tfps;

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131}; // array of weed api versions supported in plugin
static weed_plant_t *plugin_info;

static weed_plant_t *params[7];
static int zmode;
static float zfft0;
static char *zsubtitles;


typedef struct {
  int width;
  int height;
  boolean fullscreen;
  uint64_t window_id;
  int argc;
  char **argv;
} _xparms;


typedef struct {
  int width;
  int height;
  uint32_t type;
  uint32_t typesize;
} _texture;

static int ntextures;
static int ctexture;

static _texture *textures;

static GLuint *texID;
static GLuint video_pbo;

static int type;
static int typesize;

//////////////////////////////////////////////

static int get_real_tnum(int tnum, bool do_assert) {
  tnum=ctexture-1-tnum;
  if (tnum<0) tnum+=nbuf;
  assert(tnum>=0);
  if (do_assert) assert (tnum<ntextures);
  return tnum;
}

static int get_texture_width(int tnum) {
  tnum=get_real_tnum(tnum,TRUE);
  return textures[tnum].width;
}


static int get_texture_height(int tnum) {
  tnum=get_real_tnum(tnum,TRUE);
  return textures[tnum].height;
}


static int get_texture_texID(int tnum) {
  tnum=get_real_tnum(tnum,FALSE);
  return texID[tnum];
}


static int get_texture_type(int tnum) {
  tnum=get_real_tnum(tnum,TRUE);
  return textures[tnum].type;
}

/*
static int get_texture_typesize(int tnum) {
  tnum=get_real_tnum(tnum,TRUE);
  return textures[tnum].type;
}
*/

///////////////////////////////////////////////

const char *module_check_init(void) {
  if( !GL_ARB_texture_non_power_of_two) {
    snprintf (error,256,"\n\nGL_ARB_texture_non_power_of_two unavailable.\nCannot use plugin.\n");
    return error;
  }

  XInitThreads();
  
  pbo_available=FALSE;

  if (GL_ARB_pixel_buffer_object) {
    pbo_available=TRUE;
    //use_pbo=TRUE;
  }

  render_fn=&render_frame_unknown;

  glShadeModel(GL_SMOOTH);  

  glClearDepth( 1.0f );
  glEnable( GL_DEPTH_TEST );

  glDepthFunc( GL_LEQUAL );
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glPixelStorei( GL_PACK_ALIGNMENT,   1 );
  glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
  glClearColor( 0.0, 0.0, 0.0, 0.0 );
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  texID=(GLuint *)malloc(nbuf * sizeof(GLuint));
  glGenTextures(nbuf, texID);

  mypalette=WEED_PALETTE_END;

  zsubtitles=NULL;
  plugin_info=NULL;

  return NULL;
}



const char *version (void) {
  return plugin_version;
}

const char *get_description (void) {
  return "The openGL plugin allows faster playback.\n";
}

uint64_t get_capabilities (int palette) {
  return VPP_CAN_RESIZE|VPP_CAN_RETURN|VPP_LOCAL_DISPLAY;
}

const char *get_init_rfx (void) {
  return \
    "<define>\\n\
|1.7\\n\
</define>\\n\
<language_code>\\n\
0xF0\\n\
</language_code>\\n\
<params> \\n\
mode|_Mode|string_list|0|Normal|Triangle|Rotating|Wobbler|Landscape|Insider|Cube|Turning|Tunnel|Particles|Dissolve\\n\
tfps|Target _Framerate|num2|50.|1.|200.\\n\
nbuf|Number of _buffered frames|num0|32|1|256\\n\
dbuf|Use _double buffering|bool|1|0 \\n\
fsover|Over-ride _fullscreen setting (for debugging)|bool|0|0 \\n\
</params> \\n\
<param_window> \\n\
</param_window> \\n\
<onchange> \\n\
</onchange> \\n\
";
}


const void **get_play_params (func_ptr weed_bootd) {
  weed_bootstrap_f weed_boot=(weed_bootstrap_f)weed_bootd;

  //weed_plant_t *gui;

  //int api,error;

  if (plugin_info==NULL) {
    plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

    // play params
    params[0]=weed_integer_init ("mode", "Playback _mode", -1, -1, 10);
    weed_set_int_value(weed_parameter_template_get_gui(params[0]),"hidden",WEED_TRUE);

    params[1]=weed_float_init ("fft0", "fft value 0", -1., 0., 1.);
    weed_set_int_value(weed_parameter_template_get_gui(params[1]),"hidden",WEED_TRUE);

    params[2]=weed_float_init ("fft1", "fft value 1", -1., 0., 1.);
    weed_set_int_value(weed_parameter_template_get_gui(params[2]),"hidden",WEED_TRUE);

    params[3]=weed_float_init ("fft2", "fft value 2", -1., 0., 1.);
    weed_set_int_value(weed_parameter_template_get_gui(params[3]),"hidden",WEED_TRUE);

    params[4]=weed_float_init ("fft3", "fft value 3", -1., 0., 1.);
    weed_set_int_value(weed_parameter_template_get_gui(params[4]),"hidden",WEED_TRUE);

    params[5]=weed_text_init ("subtitles", "_Subtitles", "");
    weed_set_int_value(weed_parameter_template_get_gui(params[5]),"hidden",WEED_TRUE);

    params[6]=NULL;
  }

  return (const void **)params;
}


const int *get_palette_list(void) {
  // return palettes in order of preference, ending with WEED_PALETTE_END
  palette_list[0]=WEED_PALETTE_RGB24;
  palette_list[1]=WEED_PALETTE_BGR24;
  palette_list[2]=WEED_PALETTE_RGBA32;
  palette_list[3]=WEED_PALETTE_BGRA32;
  palette_list[4]=WEED_PALETTE_END;
  return palette_list;
}


boolean set_palette (int palette) {
  if (palette==WEED_PALETTE_RGBA32||palette==WEED_PALETTE_RGB24||
      palette==WEED_PALETTE_BGR24||palette==WEED_PALETTE_BGRA32) {
    render_fn=&render_frame_rgba;
    mypalette=palette;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}



static void setWindowDecorations(void) {
  unsigned char* pucData;
  int iFormat;
  unsigned long ulItems;
  unsigned long ulBytesAfter;
  Atom typeAtom;
  MotifWmHints newHints;

  Atom WM_HINTS;
  boolean set=FALSE;

  WM_HINTS = XInternAtom(dpy, "_MOTIF_WM_HINTS", True);
  if (WM_HINTS != None) {

    XGetWindowProperty (dpy, xWin, WM_HINTS, 0,
			sizeof (MotifWmHints) / sizeof (long),
			False, AnyPropertyType, &typeAtom,
			&iFormat, &ulItems, &ulBytesAfter, &pucData);
  
    newHints.flags = MWM_HINTS_DECORATIONS;
    newHints.decorations = 0;
  
    XChangeProperty (dpy, xWin, WM_HINTS, WM_HINTS,
		     32, PropModeReplace, (unsigned char *) &newHints,
		     sizeof (MotifWmHints) / sizeof (long));

    set = TRUE;
  }

  /* Now try to set KWM hints */
  WM_HINTS = XInternAtom(dpy, "KWM_WIN_DECORATION", True);
  if (WM_HINTS != None) {
    long KWMHints = 0;

    XChangeProperty(dpy, xWin, WM_HINTS, WM_HINTS, 32,
		    PropModeReplace,
		    (unsigned char *) &KWMHints,
		    sizeof(KWMHints) / 4);
    set = TRUE;
  }
  /* Now try to set GNOME hints */
  WM_HINTS = XInternAtom(dpy, "_WIN_HINTS", True);
  if (WM_HINTS != None) {
    long GNOMEHints = 0;

    XChangeProperty(dpy, xWin, WM_HINTS, WM_HINTS, 32,
		    PropModeReplace,
		    (unsigned char *) &GNOMEHints,
		    sizeof(GNOMEHints) / 4);
    set = TRUE;
  }
  /* Finally set the transient hints if necessary */
  if (!set) {
    XSetTransientForHint(dpy, xWin, RootWindow(dpy, DefaultScreen(dpy)));
  }

}


static void toggleVSync() {
  if( GLX_SGI_swap_control ) {
    if (1)
      glXSwapIntervalSGI(1);
    else
      glXSwapIntervalSGI(2);
  }
}


static void alwaysOnTop() {
  long propvalue = 12;
  XChangeProperty( dpy, xWin, XA_WIN_LAYER, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&propvalue, 1 );
  XRaiseWindow(dpy, xWin);
}




static boolean isWindowMapped(void) {
  XWindowAttributes attr;

  XGetWindowAttributes(dpy, xWin, &attr);
  if (attr.map_state != IsUnmapped) {
    return TRUE;
  } else {
    return FALSE;
  }
}



static void setFullScreen(void) {
  XWindowChanges changes;
  unsigned int valueMask = CWX | CWY | CWWidth | CWHeight;
  
  setWindowDecorations();

  XA_NET_WM_STATE = XInternAtom(dpy, "_NET_WM_STATE", False);
  XA_NET_WM_STATE_ADD = XInternAtom(dpy, "_NET_WM_STATE_ADD", False);

  XA_NET_WM_STATE_MAXIMIZED_VERT = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT",False);
  XA_NET_WM_STATE_MAXIMIZED_HORZ = XInternAtom(dpy,"_NET_WM_STATE_MAXIMIZED_HORZ",False);
  XA_NET_WM_STATE_FULLSCREEN = XInternAtom(dpy,"_NET_WM_STATE_FULLSCREEN",False);
  

  if (isWindowMapped()) {
    XEvent e;

    memset(&e,0,sizeof(e));
    e.xany.type = ClientMessage; 
    e.xclient.message_type = XA_NET_WM_STATE;
    e.xclient.format = 32;
    e.xclient.window = xWin;
    e.xclient.data.l[0] = XA_NET_WM_STATE_ADD;
    e.xclient.data.l[1] = XA_NET_WM_STATE_FULLSCREEN;
    e.xclient.data.l[3] = 0l;

    XSendEvent(dpy, RootWindow(dpy, 0), 0,
	       SubstructureNotifyMask | SubstructureRedirectMask, &e);
  } else {
    int count = 0;
    Atom atoms[3];

    atoms[count++] = XA_NET_WM_STATE_FULLSCREEN;
    atoms[count++] = XA_NET_WM_STATE_MAXIMIZED_VERT;
    atoms[count++] = XA_NET_WM_STATE_MAXIMIZED_HORZ;
    XChangeProperty(dpy, xWin, XA_NET_WM_STATE, XA_ATOM, 32,
		    PropModeReplace, (unsigned char *)atoms, count);
  }

  changes.x = 0;
  changes.y = 0;
  changes.width = m_WidthFS;
  changes.height = m_HeightFS;
  changes.stack_mode = Above;
  valueMask |= CWStackMode;
  
  XMapRaised(dpy, xWin);
  XConfigureWindow(dpy, xWin, valueMask, &changes);
  XResizeWindow(dpy, xWin, m_WidthFS, m_HeightFS);

  alwaysOnTop();
}





static int get_size_for_type(int type) {
  switch (type) {
  case GL_RGBA:
  case GL_BGRA:
    return 4;
  case GL_RGB:
  case GL_BGR:
    return 3;
  default:
    assert(0);
  }
}

static GLint binding;

static volatile uint8_t *buffer_free(volatile uint8_t *retbuf) {
  if (retbuf==NULL) return NULL;
  if (use_pbo) {
    //GLint binding;
    //glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_ARB, &binding);
    //glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, video_pbo);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
    glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, binding);
  }
  else {
    free((void *)retbuf);
  }
  return NULL;
}


static uint8_t *render_to_mainmem(int type) {
  // copy GL drawing buffer to main mem
  XWindowAttributes attr;

  uint8_t *xretbuf;

  XGetWindowAttributes(dpy, xWin, &attr);

  window_width=attr.width;
  window_height=attr.height;

  glFlush();

  glPushAttrib(GL_PIXEL_MODE_BIT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);

  glReadBuffer(swapFlag?GL_BACK:GL_FRONT);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  
  if (!use_pbo) {
    xretbuf=(uint8_t *)malloc(window_width*window_height*get_size_for_type(type));
    if (!xretbuf) {
      glPopClientAttrib();
      glPopAttrib();
      return NULL;
    }
    glReadPixels(0, 0, window_width, window_height, type, GL_UNSIGNED_BYTE, xretbuf);
  }
  else {

    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_ARB, &binding);
    glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, video_pbo);

    // read pixels into pbo buffer
    glReadPixels(0, 0, window_width, window_height, type, GL_UNSIGNED_BYTE, NULL);

    // map pbo to main memory
    xretbuf = (uint8_t *)glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);

    //glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
    //glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, binding);
  }

  return xretbuf;
}




static void render_to_gpumem_inner(int tnum, int width, int height, int type, int typesize, volatile uint8_t *texturebuf) {
  int mipMapLevel=0;
  int texID=get_texture_texID(tnum);
  
  glEnable( m_TexTarget );
  
  glBindTexture( m_TexTarget, texID );
  
  glTexImage2D( m_TexTarget, mipMapLevel, type, width, height, 0, type, GL_UNSIGNED_BYTE, (const GLvoid*)texturebuf );
  glGenerateMipmap(m_TexTarget);
  
  glDisable( m_TexTarget );
  
  tnum=get_real_tnum(tnum,FALSE);

  textures[tnum].width=width;
  textures[tnum].height=height;
  
  textures[tnum].type=type;
  textures[tnum].typesize=typesize;

}



/*
static void render_to_gpumem(int tnum, uint8_t *texturebuf) {
  render_to_gpumem_inner(get_real_tnum(tnum,TRUE),get_texture_width(tnum),get_texture_height(tnum),
			 get_texture_type(tnum),get_size_for_type(get_texture_type(tnum)),texturebuf);
}

*/


boolean init_screen (int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  _xparms xparms;

  register int i;

  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"openGL plugin error: No palette was set !\n");
    return FALSE;
  }

  xparms.width=width;
  xparms.height=height;
  xparms.fullscreen=fullscreen;
  xparms.window_id=window_id;
  xparms.argc=argc;
  xparms.argv=argv;

  mode=0;
  tfps=50.;
  nbuf=32;
  dblbuf=1;
  fsover=FALSE;

  if (argc>0) {
    mode=atoi(argv[0]);
    if (argc>1) {
      tfps=atof(argv[1]);
      if (argc>2) {
	nbuf=atoi(argv[2]);
	if (argc>3) {
	  dblbuf=atoi(argv[3]);
	  if (argc>4) {
	    fsover=atoi(argv[4]);
	  }}}}}

  textures=(_texture *)malloc(nbuf*sizeof(_texture));

  for (i=0;i<nbuf;i++) {
    textures[i].width=textures[i].height=0;
  }

  ntextures=ctexture=0;

  playing=TRUE;

  rthread_ready=FALSE;
  has_texture=FALSE;
  has_new_texture=FALSE;
  texturebuf=NULL;

  pthread_create(&rthread,NULL,render_thread_func,&xparms);

  // wait for render thread to start up
  while (!rthread_ready) usleep(1000);

  if (!playing) {
    fprintf(stderr,"openGL plugin error: Failed to start render thread\n");
    return FALSE;
  }

  return TRUE;
}




static boolean init_screen_inner (int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {

  // screen size is in RGB pixels

  int renderEventBase;
  int renderErrorBase;
  int error;

  Cursor invisibleCursor;
  Pixmap bitmapNoData;
  XColor black;
  static char noData[] = { 0,0,0,0,0,0,0,0 };

  int singleBufferAttributess[] = {
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_RED_SIZE,      1,			//	Request a single buffered color buffer
    GLX_GREEN_SIZE,    1,			//	with the maximum number of color bits
    GLX_BLUE_SIZE,     1,			//	for each component.
    GLX_ALPHA_SIZE,    1,
    None
  };
  
  int doubleBufferAttributes[] = {
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,  True,		//	Request a double-buffered color buffer with
    GLX_RED_SIZE,      1,			//	the maximum number of bits per component.
    GLX_GREEN_SIZE,    1,
    GLX_BLUE_SIZE,     1,
    GLX_ALPHA_SIZE,    1,
    None
  };

  XVisualInfo          *vInfo;
  GLXFBConfig          *fbConfigs=NULL;
  XEvent                event;
  XSetWindowAttributes  swa;
  int                   swaMask;
  int                   numReturned;

  if (fsover) fullscreen=FALSE;

  /* Open a connection to the X server */
  dpy = XOpenDisplay( NULL );

  if ( dpy == NULL ) {
    fprintf(stderr, "Unable to open a connection to the X server\n" );
    return FALSE;
  }

  m_WidthFS = WidthOfScreen(DefaultScreenOfDisplay(dpy));
  m_HeightFS = HeightOfScreen(DefaultScreenOfDisplay(dpy));

  XA_WIN_LAYER = XInternAtom(dpy, "_WIN_LAYER", False);

  if( !XRenderQueryExtension( dpy, &renderEventBase, &renderErrorBase ) ) {
    fprintf(stderr, "No RENDER extension found!" );
    return FALSE;
  }

  swa.event_mask = StructureNotifyMask | ButtonPressMask | KeyPressMask | KeyReleaseMask;

  
  if (window_id) {
    XVisualInfo *xvis;
    XVisualInfo xvtmpl;
    XWindowAttributes attr;

    xWin = (Window) window_id;
    XGetWindowAttributes(dpy, xWin, &attr);
    glxWin = xWin;

    xvtmpl.visual=attr.visual;
    xvtmpl.visualid=XVisualIDFromVisual(attr.visual);

    xvis=XGetVisualInfo(dpy,VisualIDMask,&xvtmpl,&numReturned);

    if (numReturned==0) {
      fprintf(stderr,"openGL plugin error: No xvis could be set !\n");
      return FALSE;
    }

    context = glXCreateContext ( dpy, &xvis[0], 0, GL_TRUE);

    width=window_width=attr.width;
    height=window_height=attr.height;

    glXGetConfig(dpy, xvis, GLX_DOUBLEBUFFER, &swapFlag);
    XFree(xvis);
    is_ext=TRUE;
  }
  else {
    width=window_width=fullscreen?m_WidthFS:width;
    height=window_height=fullscreen?m_HeightFS:height; 

    if (dblbuf) {
      /* Request a suitable framebuffer configuration - try for a double 
      ** buffered configuration first */
      fbConfigs = glXChooseFBConfig( dpy, DefaultScreen(dpy),
				     doubleBufferAttributes, &numReturned );
    }
    
    if ( fbConfigs == NULL ) {  /* no double buffered configs available */
      fbConfigs = glXChooseFBConfig( dpy, DefaultScreen(dpy),
				     singleBufferAttributess, &numReturned );
      swapFlag = FALSE;
    }
    
    if (!fbConfigs) {
      fprintf(stderr,"openGL plugin error: No config could be set !\n");
      return FALSE;
    }
    
    /* Create an X colormap and window with a visual matching the first
    ** returned framebuffer config */
    vInfo = glXGetVisualFromFBConfig( dpy, fbConfigs[0] );
    
    if (!vInfo) {
      fprintf(stderr,"openGL plugin error: No vInfo could be got !\n");
      return FALSE;
    }

    swa.colormap = XCreateColormap( dpy, RootWindow(dpy, vInfo->screen),
				    vInfo->visual, AllocNone );
    
    if (!swa.colormap) {
      fprintf(stderr,"openGL plugin error: No colormap could be set !\n");
      XFree (vInfo);
      return FALSE;
    }
    
    swaMask = CWBorderPixel | CWColormap | CWEventMask;
    
    swa.border_pixel = 0;

    xWin = XCreateWindow( dpy, RootWindow(dpy, vInfo->screen), 0, 0,
			  width,height,
			  0, vInfo->depth, InputOutput, vInfo->visual,
			  swaMask, &swa );

    XFreeColormap(dpy,swa.colormap);

    if (fullscreen) setFullScreen();

    XMapRaised( dpy, xWin );
    if (fullscreen) XIfEvent( dpy, &event, WaitForNotify, (XPointer) xWin );

    if (fullscreen) setFullScreen();

    /* Create a GLX context for OpenGL rendering */
    context = glXCreateNewContext( dpy, fbConfigs[0], GLX_RGBA_TYPE,
				   NULL, True );

    /* Create a GLX window to associate the frame buffer configuration
    ** with the created X window */
    glxWin = glXCreateWindow( dpy, fbConfigs[0], xWin, NULL );
    
    XFree (vInfo);

    black.red = black.green = black.blue = 0;
    
    bitmapNoData = XCreateBitmapFromData( dpy, xWin, noData, 8, 8 );
    invisibleCursor = XCreatePixmapCursor( dpy, bitmapNoData, bitmapNoData, 
					   &black, &black, 0, 0 );
    XDefineCursor( dpy, xWin, invisibleCursor );
    XFreeCursor( dpy, invisibleCursor );

    is_ext=FALSE;
  }

  glXMakeCurrent( dpy, glxWin, context );

  toggleVSync();

  error = glGetError();
  if( error != GL_NO_ERROR ) {
    char *msg = "";
      
    if( error == GL_INVALID_ENUM )	msg = "GL_INVALID_ENUM";
    else if( error == GL_INVALID_VALUE ) msg = "GL_INVALID_VALUE";
    else if( error ==    GL_INVALID_OPERATION) msg = "GL_INVALID_OPERATION";
    else if( error ==    GL_STACK_OVERFLOW)	msg = "GL_STACK_OVERFLOW";
    else if( error ==    GL_STACK_UNDERFLOW)	msg = "GL_STACK_UNDERFLOW";
    else if( error ==    GL_OUT_OF_MEMORY)	msg = "GL_OUT_OF_MEMORY";
    else if( error ==    GL_INVALID_FRAMEBUFFER_OPERATION_EXT)	msg = "GL_INVALID_FRAMEBUFFER_OPERATION_EXT";
    else msg = "Unrecognized OpenGL error";
    
    fprintf(stderr, "%s in %s(%d)", msg, __FILE__, __LINE__ );
    return FALSE;
  }


  /* OpenGL rendering ... */
  glClearColor( 0.0, 0.0, 0.0, 0.0 );
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


  if (use_pbo) {
    GLint binding;
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_ARB, &binding);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glGenBuffers(1, &video_pbo);

    glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, video_pbo);

    // create a buffer the size of the window
    glBufferData(GL_PIXEL_PACK_BUFFER_ARB, window_width * window_height * 4, NULL, GL_DYNAMIC_READ );
    
    glPopAttrib();
    glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, binding);
  }

  glFlush();
  if ( swapFlag ) glXSwapBuffers( dpy, glxWin );

  type=GL_RGBA;
  if (mypalette==WEED_PALETTE_RGB24) type=GL_RGB;
  else if (mypalette==WEED_PALETTE_BGR24) type=GL_BGR;
  else if (mypalette==WEED_PALETTE_BGRA32) type=GL_BGRA;

  typesize=get_size_for_type(type);


  rquad=0.;

  if (glXIsDirect(dpy, context)) 
    is_direct=TRUE;
  else
    is_direct=FALSE;

  /*
  XMapWindow(dpy, xWin);
  XSync(dpy, xWin);
  XSetInputFocus(dpy, xWin, RevertToNone, CurrentTime);
  XSelectInput(dpy, xWin, KeyPressMask | KeyReleaseMask);
  */
  return TRUE;
}



static void set_priorities(void) {
  // prioritise textures so the most recent ones have highest priority
  // GL should swap oldest out memory and swap newst in memory

  float pri=1.;
  GLclampf *prios=(GLclampf *)malloc(nbuf * sizeof(GLclampf));
  int idx=ctexture;

  register int i;

  for (i=0;i<nbuf;i++) {
    prios[i]=0.;
  }

  for (i=0;i<nbuf;i++) {
    prios[idx]=pri;
    idx--;
    if (idx<0) idx+=nbuf;
    if (idx>ntextures) break;
    pri-=1./(float)nbuf;
    if (pri<0.) pri=0.;
  }

  glPrioritizeTextures(nbuf,texID,prios);
  
  free(prios);

}

/*
static void resize_buffer(uint8_t *out, int owidth, int oheight, uint8_t *in, int iwidth, int iheight, int type) {
  int xi,xj;
  int typesize=get_size_for_type(type);
  float scalex,scaley;

  register int i,j;

  uint8_t *ptr, *dst;

  scalex=(float)iwidth/(float)owidth;
  scaley=(float)iheight/(float)oheight;

  dst=out;

  for (i=0;i<oheight;i++) {
    xi=(float)i*scaley;
    ptr=in+xi*iwidth*typesize;
    for (j=0;j<owidth;j++) {
      xj=(float)j*scalex;
      memcpy(dst,ptr+xj*typesize,typesize);
      dst+=typesize;
    }
    
  }
}
*/


static boolean Upload(int width, int height) {
  XWindowAttributes attr;

  int imgWidth=width;
  int imgHeight=height;

  int texID;

  texID=get_texture_texID(0);

  if (zmode!=-1) mode=zmode;

  if (has_new_texture) {
    ctexture++;
    if (ctexture==nbuf) ctexture=0;
    if (ntextures<nbuf) ntextures++;

    has_new_texture=FALSE;

    render_to_gpumem_inner(0,width,height,type,typesize,texturebuf);
  
    set_priorities();

  }

  pthread_mutex_unlock(&rthread_mutex); // re-enable texture thread

  if (!return_ready&&retbuf!=NULL) {
    retbuf=buffer_free(retbuf);
  }

  texID=get_texture_texID(0);


  ////////////////////////////////////////////////////////////
  // modes

  XGetWindowAttributes(dpy, xWin, &attr);

  window_width=attr.width;
  window_height=attr.height;

  switch (mode) {
  case 0:
    {
      // flat:
      glClear(GL_COLOR_BUFFER_BIT);
      glClear(GL_DEPTH_BUFFER_BIT);

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glViewport(0, 0, window_width, window_height);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glFrustum(-1, 1, -1, 1, 2, 10);

      glEnable(GL_DEPTH_TEST);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();

      glTranslatef(.0, .0, -2);

      glEnable(m_TexTarget);
      glMatrixMode(GL_TEXTURE);
      glLoadIdentity();

      /*	glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
		glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);*/

      glBindTexture(m_TexTarget, texID);
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

      glDisable(m_TexTarget);

      glMatrixMode(GL_MODELVIEW);
      glDisable(GL_DEPTH_TEST);


    }
    break;

  case 1:
    {
      glClear(GL_COLOR_BUFFER_BIT);
      glClear(GL_DEPTH_BUFFER_BIT);

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glViewport(0, 0, window_width, window_height);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();

      glEnable(GL_DEPTH_TEST);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();

      glTranslatef(.0, .0, -1);

      glEnable(m_TexTarget);
      glMatrixMode(GL_TEXTURE);
      glLoadIdentity();

      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

      glBindTexture(m_TexTarget, texID);
      glColor4d(1.0, 1.0, 1.0, 1.0);


      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glBegin(GL_TRIANGLES);                      // Drawing Using Triangles

      glTexCoord2f (0.5, 0.0);
      glVertex3f( 0.0f, 1.0f, 0.0f);              // Top
      
      glTexCoord2f (0.0, 1.0);
      glVertex3f(-1.0f,-1.0f, 0.0f);              // Bottom Left
      
      glTexCoord2f (1.0, 1.0);
      glVertex3f( 1.0f,-1.0f, 0.0f);              // Bottom Right
      glEnd();     
      
      glTranslatef(-1.5f,0.0f,-6.0f);
      glEnd();

      glDisable(m_TexTarget);

      glMatrixMode(GL_MODELVIEW);
      glDisable(GL_DEPTH_TEST);


    }
    break;

  case 2:
    {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glTranslatef(0.0,0.0,-1.);

      // rotations
      glMatrixMode(GL_PROJECTION);                // Select The Projection Matrix
      //glLoadIdentity();
      //gluPerspective(45.0f,(GLfloat)window_width/(GLfloat)window_height,0.1f,100.0f);
 
      glRotatef(rquad,0.0f,0.0f,1.0f);            // Rotate The Quad On The Z axis
      
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );
      
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      rquad-=0.5f;                       // Decrease The Rotation Variable For The Quad 

      glBegin (GL_QUADS);
      
      glTexCoord2f (0.0, 0.0);
      glVertex3f (-1.0, 1.1, 0.0);
      
      glTexCoord2f (1.0, 0.0);
      glVertex3f (1.0, 1.1, 0.0);
      
      glTexCoord2f (1.0, 1.0);
      glVertex3f (1.0, -1.0, 0.0);
      
      glTexCoord2f (0.0, 1.0);
      glVertex3f (-1.0, -1.0, 0.0);
      
      glEnd ();
      
      glDisable( m_TexTarget );
    }
    break;

  case 3:
    {
      // wobbler:

      // sync to the clock with the clock_gettime function
      struct timespec now;
      int ticks;

      float vx=-1.0,vy=1.0;
      float tx=0.0,ty;

      float vz;

      clock_gettime(CLOCK_MONOTONIC,&now);
      ticks=now.tv_sec*1000+now.tv_nsec/1000000;
      ty=sin(ticks*0.001)*0.2;

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_PROJECTION); // use the projection mode
      glLoadIdentity ();
      gluPerspective(60.0, (float)imgWidth/(float)imgHeight, 0.01, 1135.0);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glTranslatef(0.0,0.0,-1.1);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );

      glEnable(GL_DEPTH_TEST);

      // parameters of the wobbler grid: the larger the width and height, the more detailed the effect is
#define WX 160 // grid width
#define WY 160 // grid height
	
      // precalculate the gaps between grid points
#define VX_PLUS (2.0/WX)
#define VY_PLUS (2.0/WY)
#define TX_PLUS (1.0/WX)
#define TY_PLUS (1.0/WY)

      // parameters for the sin waves
#define TSPD1 0.003 	// speed (in msec)
#define XSPD1 0.01 	// speed (along grid X)
#define YSPD1 0.2 	// speed (along grid Y)
#define A1 0.13 	// amplitute

#define TSPD2 0.004	// speed (in msec)
#define XSPD2 0.15	// speed (along grid X)
#define YSPD2 0.03	// speed (along grid Y)
#define A2 0.04		// amplitude


      for (int j=0; j<WY; j++) {
	vx=-1.0; tx=0.0;
	for (int i=0; i<WX; i++) {
	  float col=1.0-sin(i*0.05+j*0.06)*1.0;

	  glBegin (GL_QUADS);
      
	  glColor3f(col,col,col);

	  vz=sin(ticks*TSPD1+i*XSPD1+j*YSPD1)*A1+cos(ticks*TSPD2+i*XSPD2+j*YSPD2)*A2;
	  glTexCoord2f (tx,ty);
	  glVertex3f (vx,vy, vz);
      
	  vz=sin(ticks*TSPD1+(i+1)*XSPD1+j*YSPD1)*A1+cos(ticks*TSPD2+(i+1)*XSPD2+j*YSPD2)*A2;
	  col=1.0-sin((i+1)*0.05+j*0.06)*1.0;
	  glColor3f(col,col,col);

	  glTexCoord2f (tx+TX_PLUS, ty);
	  glVertex3f (vx+VX_PLUS, vy, vz);
      
	  vz=sin(ticks*TSPD1+(i+1)*XSPD1+(j+1)*YSPD1)*A1+cos(ticks*TSPD2+(i+1)*XSPD2+(j+1)*YSPD2)*A2;
	  col=1.0-sin((i+1)*0.05+(j+1)*0.06)*1.0;
	  glColor3f(col,col,col);

	  glTexCoord2f (tx+TX_PLUS, ty+TY_PLUS);
	  glVertex3f (vx+VX_PLUS, vy-VY_PLUS, vz);
      
	  vz=sin(ticks*TSPD1+i*XSPD1+(j+1)*YSPD1)*A1+cos(ticks*TSPD2+i*XSPD2+(j+1)*YSPD2)*A2;
	  col=1.0-sin(i*0.05+(j+1)*0.06)*1.0;
	  glColor3f(col,col,col);

	  glTexCoord2f (tx, ty+TY_PLUS);
	  glVertex3f (vx, vy-VY_PLUS, vz);
      
	  glEnd ();

	  vx+=VX_PLUS;
	  tx+=TX_PLUS;

	}
	vy-=VY_PLUS;
	ty+=TY_PLUS;

      }
      glDisable( m_TexTarget );
    }
    break;

  case 4:
    {
      // landscape:

      // time sync
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC,&now);
      int ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_PROJECTION); // use the projection mode
      glLoadIdentity ();
      gluPerspective(60.0, (float)imgWidth/(float)imgHeight, 0.01, 1135.0);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glTranslatef(0.0,0.0,-1.1);

      // tilt the texture to look like a landscape
      glRotatef(-60,1,0,0);
      // add a little bit of rotating movement
      glRotatef(sin(ticks*0.001)*15,0,1,1);

      glScalef(1.3,1.0,1.0); // make the landscape wide!

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );

      glEnable(GL_DEPTH_TEST);

      // parameters of the landscape grid: the larger the width and height, the more detailed the effect is
#define WX 160 // grid width
#define WY 160 // grid height
	
      // precalculate the gaps between grid points
#define VX_PLUS (2.0/WX)
#define VY_PLUS (2.0/WY)
#define TX_PLUS (1.0/WX)
#define TY_PLUS (1.0/WY)

      // sin wave parameters
#define TSPD1 0.003     // speed (in msecs)
#define XSPD1 0.01      // speed (along grid X)
#define YSPD1 0.2       // speed (along grid Y)
#define A12 0.04         // amplitute

#define TSPD2 0.004     // speed (in msec)
#define XSPD2 0.15      // speed (along grid X)
#define YSPD2 0.03      // speed (along grid Y)
#define A22 0.05         // amplitude


      float vx=-1.0,vy=1.0;
      float tx=0.0,ty=-(ticks % 4000)*0.00025;
      float vz;

      for (int j=0; j<WY; j++) {
	vx=-1.0; tx=0.0;
	for (int i=0; i<WX; i++) {
	  float col=1.0-sin(i*0.05+j*0.06)*1.0;

	  glBegin (GL_QUADS);
      
	  glColor3f(col,col,col);

	  vz=sin(ticks*TSPD1+i*XSPD1+j*YSPD1)*A12+cos(ticks*TSPD2+i*XSPD2+j*YSPD2)*A22;
	  glTexCoord2f (tx,ty);
	  glVertex3f (vx,vy, vz);
      
	  vz=sin(ticks*TSPD1+(i+1)*XSPD1+j*YSPD1)*A12+cos(ticks*TSPD2+(i+1)*XSPD2+j*YSPD2)*A22;
	  col=1.0-sin((i+1)*0.05+j*0.06)*1.0;
	  glColor3f(col,col,col);

	  glTexCoord2f (tx+TX_PLUS, ty);
	  glVertex3f (vx+VX_PLUS, vy, vz);
      
	  vz=sin(ticks*TSPD1+(i+1)*XSPD1+(j+1)*YSPD1)*A12+cos(ticks*TSPD2+(i+1)*XSPD2+(j+1)*YSPD2)*A22;
	  col=1.0-sin((i+1)*0.05+(j+1)*0.06)*1.0;
	  glColor3f(col,col,col);

	  glTexCoord2f (tx+TX_PLUS, ty+TY_PLUS);
	  glVertex3f (vx+VX_PLUS, vy-VY_PLUS, vz);
      
	  vz=sin(ticks*TSPD1+i*XSPD1+(j+1)*YSPD1)*A12+cos(ticks*TSPD2+i*XSPD2+(j+1)*YSPD2)*A22;
	  col=1.0-sin(i*0.05+(j+1)*0.06)*1.0;
	  glColor3f(col,col,col);

	  glTexCoord2f (tx, ty+TY_PLUS);
	  glVertex3f (vx, vy-VY_PLUS, vz);
      
	  glEnd ();

	  vx+=VX_PLUS;
	  tx+=TX_PLUS;

	}
	vy-=VY_PLUS;
	ty+=TY_PLUS;

      }
      glDisable( m_TexTarget );
    }

    break;

  case 5:
    {
      // insider:

      // time sync
      struct timespec now;
      int ticks;

      clock_gettime(CLOCK_MONOTONIC,&now);
      ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_PROJECTION); // use the projection mode
      glLoadIdentity ();
      gluPerspective(60.0, (float)imgWidth/(float)imgHeight, 0.01, 1135.0);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glTranslatef(0.0,0.0,-1.0);

      // turn the cube
      glRotatef(ticks*0.07,0,1,0);
      glRotatef(ticks*0.08,0,0,1);
      glRotatef(ticks*0.035,1,0,0);

      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );
      
      glEnable(GL_DEPTH_TEST);

      // draw the cube with the texture
      for (int i=0; i<6; i++) {
	glBegin (GL_QUADS);
      
	glTexCoord2f (0.0, 0.0);
	glVertex3f (-1.0, 1.0, 1.0);
      
	glTexCoord2f (1.0, 0.0);
	glVertex3f (1.0, 1.0, 1.0);
     
	glTexCoord2f (1.0, 1.0);
	glVertex3f (1.0, -1.0, 1.0);
      
	glTexCoord2f (0.0, 1.0);
	glVertex3f (-1.0, -1.0, 1.0);
      
	glEnd ();
	if (i<3) glRotatef(90,0,1,0);
	else if (i==3) glRotatef(90,1,0,0);
	else if (i==4) glRotatef(180,1,0,0);
      }

      glDisable( m_TexTarget );
    }

    break;

  case 6:
    {
      // cube:

      // time sync
      struct timespec now;
      int ticks;

      clock_gettime(CLOCK_MONOTONIC,&now);
      ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_PROJECTION); // use the projection mode
      glLoadIdentity ();
      gluPerspective(60.0, (float)imgWidth/(float)imgHeight, 0.01, 1135.0);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();

      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );
      
      glEnable(GL_DEPTH_TEST);

      // inner + outer cube
      for (int k=0; k<2; k++) {

	glPushMatrix();
	glTranslatef(0.0,0.0,-1.0);

	// turn the cube
	glRotatef(ticks*0.036,0,1,0);
	glRotatef(ticks*0.07,0,0,1);
	glRotatef(ticks*0.08,1,0,0);

	glScalef(1.0-k*0.75,1.0-k*0.75,1.0-k*0.75);

	// draw the cube with the texture
	for (int i=0; i<6; i++) {
	  glBegin (GL_QUADS);
	      
	  glTexCoord2f (0.0, 0.0);
	  glVertex3f (-1.0, 1.0, 1.0);
      
	  glTexCoord2f (1.0, 0.0);
	  glVertex3f (1.0, 1.0, 1.0);
     
	  glTexCoord2f (1.0, 1.0);
	  glVertex3f (1.0, -1.0, 1.0);
      
	  glTexCoord2f (0.0, 1.0);
	  glVertex3f (-1.0, -1.0, 1.0);
      
	  glEnd ();
	  if (i<3) glRotatef(90,0,1,0);
	  else if (i==3) glRotatef(90,1,0,0);
	  else if (i==4) glRotatef(180,1,0,0);
	}

	glPopMatrix();
      }
      glDisable( m_TexTarget );
    }

    break;

  case 7:
    {
      // turning:

      // time sync
      struct timespec now;
      int ticks;
      clock_gettime(CLOCK_MONOTONIC,&now);
      ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_PROJECTION); // use the projection mode
      glLoadIdentity ();
      gluPerspective(60.0, (float)imgWidth/(float)imgHeight, 0.01, 1135.0);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glTranslatef(0.0,0.0,-2.3);

      // turn the cube
      glRotatef(cos((ticks % 5000)*M_PI/5000.0)*45+45,0,1,0);

      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );
      
      glEnable(GL_DEPTH_TEST);

      // draw the cube with the texture
      for (int i=0; i<6; i++) {
	glBegin (GL_QUADS);
      
	glTexCoord2f (0.0, 0.0);
	glVertex3f (-1.0, 1.0, 1.0);
      
	glTexCoord2f (1.0, 0.0);
	glVertex3f (1.0, 1.0, 1.0);
     
	glTexCoord2f (1.0, 1.0);
	glVertex3f (1.0, -1.0, 1.0);
      
	glTexCoord2f (0.0, 1.0);
	glVertex3f (-1.0, -1.0, 1.0);
      
	glEnd ();
	if (i<3) glRotatef(90,0,1,0);
	else if (i==3) glRotatef(90,1,0,0);
	else if (i==4) glRotatef(180,1,0,0);
      }

      glDisable( m_TexTarget );
    }

    break;

  case 8:
    {
      // tunnel:

      // sync to the clock
      struct timespec now;
      int ticks;
      float tx=0.0,ty;

      clock_gettime(CLOCK_MONOTONIC,&now);
      ticks=now.tv_sec*1000+now.tv_nsec/1000000;
      ty=(ticks % 2000)*0.0005;

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_PROJECTION); // use the projection mode
      glLoadIdentity ();
      gluPerspective(60.0, (float)imgWidth/(float)imgHeight, 0.01, 1135.0);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glTranslatef(0.0,0.0,-1.0);

      glRotatef(5,0,1,0);
      glRotatef(ticks*0.05,0,0,1);
      glRotatef(7,1,0,0);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );
      
      glEnable(GL_DEPTH_TEST);

      // parameters of the tunnel grid: the larger the width and height, the more detailed the effect is
#define TR 160 // tunnel radius
#define TD 160 // tunnel depth
	
      // size on screen
#define SR 2.0 // radius
#define SD 0.4 // depth factor

      // precalculate the gaps between grid points
#define TX_PLUS2 (1.0/WX)
#define TY_PLUS2 (1.0/160)



      for (int j=0; j<TD; j++) {
	tx=0.0;
	for (int i=0; i<TR; i++) {
	  float vx=cos(i*2*M_PI/TR)*SR;
	  float vy=sin(i*2*M_PI/TR)*SR;
	  float vz=-j*SD;

	  float col=2.0-2.0*(float)j/TD;

	  glColor3f(col,col*0.92,col*0.93); 
	  glBegin (GL_QUADS);

	  glTexCoord2f (tx,ty);
	  glVertex3f (vx,vy,vz);
      
	  vx=cos((i+1)*2*M_PI/TR)*SR;
	  vy=sin((i+1)*2*M_PI/TR)*SR;
	  vz=-j*SD;
	  glTexCoord2f (tx+TX_PLUS2, ty);
	  glVertex3f (vx, vy, vz);
      
	  vx=cos((i+1)*2*M_PI/TR)*SR;
	  vy=sin((i+1)*2*M_PI/TR)*SR;
	  vz=-(j+1)*SD;
	  glTexCoord2f (tx+TX_PLUS2, ty+TY_PLUS2);
	  glVertex3f (vx, vy, vz);
      
	  vx=cos(i*2*M_PI/TR)*SR;
	  vy=sin(i*2*M_PI/TR)*SR;
	  vz=-(j+1)*SD;
	  glTexCoord2f (tx, ty+TY_PLUS2);
	  glVertex3f (vx, vy, vz);
      
	  glEnd ();

	  tx+=TX_PLUS2;

	}
	ty+=TY_PLUS2;

      }
      glDisable( m_TexTarget );
    }
    break;
  case 9:
    {
      // particles:

      typedef struct {
	float x,y,z; 	// position coordinate
	float sx,sy;	// size of the particle square
	float vx,vy,vz; // speed
	float tx1,ty1,tx2,ty2; // texture position (color)
	int start_time; // when was created (tick)
	int end_time; 	// when will disappear (tick)
      } PARTICLE;

#define NOT_CREATED -1 	// a flag for a particle that was not created
#define NOF_PARTS 10000  // the number of particles	
#define PIXEL_SIZE 1.0	// size of the particle pixels (1.0 = a pixel)

      static PARTICLE parts[NOF_PARTS]; // particle array

      static int parts_init=FALSE; // have been inited?

      if (!parts_init) {
	for (int i=0; i<NOF_PARTS; i++) parts[i].start_time=NOT_CREATED;
	parts_init=TRUE;
      }	
      // time sync
      struct timespec now;
      int ticks;

      clock_gettime(CLOCK_MONOTONIC,&now);
      ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_PROJECTION); // use the projection mode
      glLoadIdentity ();
      gluPerspective(60.0, (float)imgWidth/(float)imgHeight, 0.01, 1135.0);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glTranslatef(0.0,0.0,-2.3);

      // turn the place
      float rotx=sin(ticks*M_PI/5000.0)*5-15;
      float roty=sin(ticks*M_PI/5000.0)*5+45;
      float rotz=sin(ticks*M_PI/5000.0)*5;

      glRotatef(rotx,1,0,0);
      glRotatef(roty,0,1,0);
      glRotatef(rotz,0,0,1);

      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );
      
      glEnable(GL_DEPTH_TEST);

      // draw the emitter with the texture

      glBegin (GL_QUADS);
      glColor3f(1,1,1);      
      glTexCoord2f (0.0, 0.0);
      glVertex3f (-1.0, 1.0, 0.0);
      
      glTexCoord2f (1.0, 0.0);
      glVertex3f (1.0, 1.0, 0.0);
     
      glTexCoord2f (1.0, 1.0);
      glVertex3f (1.0, -1.0, 0.0);
      
      glTexCoord2f (0.0, 1.0);
      glVertex3f (-1.0, -1.0, 0.0);

      glEnd ();

      // draw the particles

      for (int i=0; i<NOF_PARTS; i++) {

	//int pos;
	if ((parts[i].start_time==NOT_CREATED) || (ticks>=parts[i].end_time)) {
	  parts[i].start_time=ticks;
	  parts[i].x=(rand() % 2000)/1000.0-1.0;
	  parts[i].y=(rand() % 2000)/1000.0-1.0;
	  parts[i].z=0.0;
	  parts[i].vx=0.001*((rand() % 2000)/1000.0-1.0);
	  parts[i].vy=0.0;
	  parts[i].vz=0.01;
	  parts[i].end_time=4000+parts[i].start_time+rand() % 500;

	  parts[i].sx=PIXEL_SIZE*2.0/imgWidth;
	  parts[i].sy=PIXEL_SIZE*2.0/imgHeight;

	  parts[i].tx1=((parts[i].x+1.0)/2.0);
	  parts[i].ty1=(1.0-(parts[i].y+1.0)/2.0);
	  parts[i].tx2=parts[i].tx1+parts[i].sx/2.0;
	  parts[i].ty2=parts[i].ty1+parts[i].sy/2.0;
	}
	glBegin(GL_QUADS);

	glTexCoord2f (parts[i].tx1, parts[i].ty1);	
	glVertex3f(parts[i].x,parts[i].y,parts[i].z);

	glTexCoord2f (parts[i].tx2, parts[i].ty1);	
	glVertex3f(parts[i].x+parts[i].sx,parts[i].y,parts[i].z);

	glTexCoord2f (parts[i].tx2, parts[i].ty2);	
	glVertex3f(parts[i].x+parts[i].sx,parts[i].y-parts[i].sy,parts[i].z);

	glTexCoord2f (parts[i].tx1, parts[i].ty2);	
	glVertex3f(parts[i].x,parts[i].y-parts[i].sy,parts[i].z);
	glEnd();
		
	parts[i].x+=parts[i].vx;
	parts[i].y+=parts[i].vy;
	parts[i].z+=parts[i].vz;
	parts[i].vy-=0.0001; // adds a small gravity


      }
      glDisable( m_TexTarget );

    }

    break;

  case 10:
    {
      // dissolve:

      typedef struct {
	float x,y,z; 	// position coordinate
	float sx,sy;	// size of the particle square
	float vx,vy,vz; // speed
	float tx1,ty1,tx2,ty2; // texture position (color)
	int start_time; // when was created (tick)
	int end_time; 	// when will disappear (tick)
      } PARTICLE;

#define NOT_CREATED -1 	// a flag for a particle that was not created
#define NOF_PARTS2 20000  // the number of particles	
#define PIXEL_SIZE2 4.0	// size of the particle pixels (1.0 = a pixel)

      static PARTICLE parts[NOF_PARTS2]; // particle array

      static int parts_init=FALSE; // have been inited?

      if (!parts_init) {
	for (int i=0; i<NOF_PARTS2; i++) parts[i].start_time=NOT_CREATED;
	parts_init=TRUE;
      }	
      // time sync
      struct timespec now;
      float rotx,roty,rotz;
      int ticks;
      clock_gettime(CLOCK_MONOTONIC,&now);
      ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, window_width, window_height);

      glMatrixMode (GL_PROJECTION); // use the projection mode
      glLoadIdentity ();
      gluPerspective(60.0, (float)imgWidth/(float)imgHeight, 0.01, 1135.0);

      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glTranslatef(0.0,0.0,-2.3);

      // turn the place
      rotx=sin(ticks*M_PI/5000.0)*5+15;
      roty=sin(ticks*M_PI/5000.0)*15+15;
      rotz=sin(ticks*M_PI/5000.0)*5;

      glRotatef(rotx,1,0,0);
      glRotatef(roty,0,1,0);
      glRotatef(rotz,0,0,1);

      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );
      
      glEnable(GL_DEPTH_TEST);

      // draw the emitter with the texture
      /*
	glBegin (GL_QUADS);
	glColor3f(1,1,1);      
      	glTexCoord2f (0.0, 0.0);
      	glVertex3f (-1.0, 1.0, 0.0);
      
      	glTexCoord2f (1.0, 0.0);
      	glVertex3f (1.0, 1.0, 0.0);
     
      	glTexCoord2f (1.0, 1.0);
      	glVertex3f (1.0, -1.0, 0.0);
      
      	glTexCoord2f (0.0, 1.0);
      	glVertex3f (-1.0, -1.0, 0.0);

      	glEnd ();
      */
      // draw the squares

      glEnable(GL_BLEND);
      glBlendFunc ( GL_SRC_ALPHA , GL_ONE_MINUS_SRC_ALPHA );
 
      for (int i=0; i<NOF_PARTS2; i++) {

	if ((parts[i].start_time==NOT_CREATED) || (ticks>=parts[i].end_time)) {
	  parts[i].start_time=ticks;
	  parts[i].x=(rand() % 2000)/1000.0-1.0;
	  parts[i].y=(rand() % 2000)/1000.0-1.0;
	  parts[i].z=0.0;
	  parts[i].vx=0.001*((rand() % 2000)/1000.0-1.0);
	  parts[i].vy=0.001*((rand() % 2000)/1000.0-1.0);
	  parts[i].vz=0.01;
	  parts[i].end_time=parts[i].start_time+500+rand() % 500;

	  parts[i].sx=PIXEL_SIZE2*2.0/imgWidth;
	  parts[i].sy=PIXEL_SIZE2*2.0/imgHeight;

	  parts[i].tx1=((parts[i].x+1.0)/2.0);
	  parts[i].ty1=(1.0-(parts[i].y+1.0)/2.0);
	  parts[i].tx2=parts[i].tx1+parts[i].sx/2.0;
	  parts[i].ty2=parts[i].ty1+parts[i].sy/2.0;
	}
	glColor4f(1.0,1.0,1.0,0.5+(float)(parts[i].end_time-ticks)/(parts[i].end_time-parts[i].start_time));
	glBegin(GL_QUADS);

	glTexCoord2f (parts[i].tx1, parts[i].ty1);	
	glVertex3f(parts[i].x,parts[i].y,parts[i].z);

	glTexCoord2f (parts[i].tx2, parts[i].ty1);	
	glVertex3f(parts[i].x+parts[i].sx,parts[i].y,parts[i].z);

	glTexCoord2f (parts[i].tx2, parts[i].ty2);	
	glVertex3f(parts[i].x+parts[i].sx,parts[i].y-parts[i].sy,parts[i].z);

	glTexCoord2f (parts[i].tx1, parts[i].ty2);	
	glVertex3f(parts[i].x,parts[i].y-parts[i].sy,parts[i].z);
	glEnd();
		
	parts[i].x+=parts[i].vx;
	parts[i].y+=parts[i].vy;
	parts[i].z+=parts[i].vz;
	parts[i].vy+=0.0004; // adds a small pull up


      }
      glDisable( m_TexTarget );
      glDisable(GL_BLEND);

    }

    break;

  }

  if (retdata!=NULL) {
    // copy buffer to retbuf

    if (retbuf!=NULL) {
      buffer_free(retbuf);
    }

    retbuf=render_to_mainmem(type);
    return_ready=TRUE;
  }

  if (swapFlag) glXSwapBuffers( dpy, glxWin );

  return TRUE;
}


static void *render_thread_func(void *data) {
  _xparms *xparms=(_xparms *)data;

  retbuf=NULL;

  init_screen_inner (xparms->width, xparms->height, xparms->fullscreen, xparms->window_id, xparms->argc, xparms->argv);

  rthread_ready=TRUE;

  while (playing) {
    usleep(1000000./tfps);
    pthread_mutex_lock(&rthread_mutex);
    if (has_texture&&playing) {
      Upload(imgWidth,imgHeight);
    }
    else pthread_mutex_unlock(&rthread_mutex);
  }

  if (retbuf!=NULL) {
    buffer_free(retbuf);
  }

  retbuf=NULL;

  return NULL;
}




boolean render_frame_rgba (int hsize, int vsize, void **pixel_data, void **return_data) {

  pthread_mutex_lock(&rthread_mutex); // wait for lockout of render thread

  has_texture=TRUE;
  has_new_texture=TRUE;


  if (return_data!=NULL) {
    size_t twidth=window_width*typesize;
    uint8_t *dst,*src;
    register int i;

    if (texturebuf!=NULL) {
      free((void *)texturebuf);
    }

    texturebuf=(uint8_t *)pixel_data[0]; // no memcpy needed, as we will not free pixel_data until render_thread has used it
    return_ready=FALSE;
    retdata=(uint8_t *)return_data[0]; // host created space for return data

    imgWidth=hsize;
    imgHeight=vsize;

    pthread_mutex_unlock(&rthread_mutex); // render thread - GO !

    while (!return_ready) usleep(1000); // wait for return data
    pthread_mutex_lock(&rthread_mutex); // lock render thread while we grab data

    dst=(uint8_t *)retdata;
    retdata=NULL;

    texturebuf=NULL;

    src=(uint8_t *)retbuf+(window_height-1)*twidth;

    // texture is upside-down compared to image
    for (i=0;i<window_height;i++) {
      memcpy(dst,src,twidth);
      dst+=twidth;
      src-=twidth;
    }

  }
  else {
    if (hsize!=imgWidth || vsize!=imgHeight || texturebuf==NULL) {
      if (texturebuf!=NULL) {
	free((void *)texturebuf);
      }
      texturebuf=(uint8_t *)malloc(hsize*vsize*typesize);
    }
    
    memcpy((void *)texturebuf,pixel_data[0],hsize*vsize*typesize);

    imgWidth=hsize;
    imgHeight=vsize;

    retdata=NULL;
  }

  pthread_mutex_unlock(&rthread_mutex); // re-enable render thread

  return TRUE;
}





boolean render_frame_unknown (int hsize, int vsize, void **pixel_data, void **return_data) {
  fprintf(stderr,"openGL plugin error: No palette was set !\n");
  return FALSE;
}




void decode_pparams(weed_plant_t **pparams) {
  weed_plant_t *ptmpl;
  char *pname;
  int error,type;

  register int i=0;

  zmode=0;
  zfft0=0.;
  if (zsubtitles!=NULL) weed_free(zsubtitles);
  zsubtitles=NULL;

  if (pparams==NULL) return;
  while (pparams[i]!=NULL) {
    
    type=weed_get_int_value(pparams[i],"type",&error);


    if (type==WEED_PLANT_PARAMETER) {
      ptmpl=weed_get_plantptr_value(pparams[i],"template",&error);
      pname=weed_get_string_value(ptmpl,"name",&error);
      
      if (!strcmp(pname,"mode")) {
	zmode=weed_get_int_value(pparams[i],"value",&error);
      }
      else if (!strcmp(pname,"fft0")) {
	zfft0=(float)weed_get_double_value(pparams[i],"value",&error);
      }
      else if (!strcmp(pname,"subtitles")) {
	zsubtitles=weed_get_string_value(pparams[i],"value",&error);
      }
      
      weed_free(pname);
    }
    else {
      // must be an alpha channel

      
      
    }
    i++;
  }


}


boolean render_frame (int hsize, int vsize, int64_t tc, void **pixel_data, void **return_data, void **pp) {
  // call the function which was set in set_palette
  weed_plant_t **pparams=(weed_plant_t **)pp;

  if (pparams!=NULL) {
    decode_pparams(pparams);
  }

  return render_fn (hsize,vsize,pixel_data,return_data);
}


void exit_screen (int16_t mouse_x, int16_t mouse_y) {
  playing=FALSE;

  pthread_join(rthread,NULL);

  if (texturebuf!=NULL) {
    free((void *)texturebuf);
  }

  if (use_pbo) glDeleteBuffers(1, &video_pbo);

  free(textures);

  if (!is_ext) {
    XUnmapWindow (dpy, xWin);
    XDestroyWindow (dpy, xWin);
  }

  XFlush(dpy);

  pthread_mutex_lock(&dpy_mutex);
  glXMakeContextCurrent(dpy, 0, 0, 0);
  glXDestroyContext(dpy, context);
  XCloseDisplay (dpy);
  dpy=NULL;
  pthread_mutex_unlock(&dpy_mutex);

}



void module_unload(void) {
  if (ntextures>0) glDeleteTextures(ntextures,texID);
  free(texID);
  if (zsubtitles!=NULL) weed_free(zsubtitles);
}

