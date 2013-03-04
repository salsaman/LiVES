// LiVES - openGL playback engine
// (c) G. Finch 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <math.h>

////////////////////////////////////////////////////////////////////////////////////

static char plugin_version[64]="LiVES openGL playback engine version 1.0";
static char error[256];

static boolean (*render_fn)(int hsize, int vsize, void **pixel_data);
static boolean render_frame_rgba (int hsize, int vsize, void **pixel_data);
static boolean render_frame_unknown (int hsize, int vsize, void **pixel_data);

static int palette_list[5];
static int mypalette;

/////////////////////////////////////////////
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <GLee.h>

#include <GL/gl.h>
#include <GL/glx.h>

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
static GLuint texID;

static boolean swapFlag = TRUE;
static boolean has_gl_texture;

static int m_WidthFS;
static int m_HeightFS;

static uint32_t type;

static boolean is_direct;

static boolean is_ext;

static Bool WaitForNotify( Display *dpy, XEvent *event, XPointer arg ) {
  return (event->type == MapNotify) && (event->xmap.window == (Window) arg);
}

static pthread_mutex_t dpy_mutex;

#include <pthread.h>

//////////////////////////////////////////////


const char *module_check_init(void) {
  if( !GL_ARB_texture_non_power_of_two) {
    snprintf (error,256,"\n\nGL_ARB_texture_non_power_of_two unavailable.\nCannot use plugin.\n");
    return error;
  }

  render_fn=&render_frame_unknown;

  XInitThreads();

  glShadeModel( GL_FLAT );
  glClearDepth( 0.0f );
  glDisable( GL_BLEND );
  glDisable( GL_DEPTH_TEST );
  glDepthFunc( GL_ALWAYS );
  glDisable( GL_LIGHTING );
  glFrontFace(GL_CW);

  glPixelStorei( GL_PACK_ALIGNMENT,   1 );
  glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
  glClearColor( 0.0, 0.0, 0.0, 0.0 );
  glClear( GL_COLOR_BUFFER_BIT );

  glGenTextures(1, &texID);

  mypalette=WEED_PALETTE_END;

  return NULL;
}



const char *version (void) {
  return plugin_version;
}

const char *get_description (void) {
  return "The openGL plugin allows faster playback.\n";
}

uint64_t get_capabilities (int palette) {
  return VPP_CAN_RESIZE|VPP_LOCAL_DISPLAY;
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
dbuf|Use _double buffering|bool|1|0 \\n\
fsover|Over-ride _fullscreen setting (for debugging)|bool|0|0 \\n\
</params> \\n\
<param_window> \\n\
</param_window> \\n\
<onchange> \\n\
</onchange> \\n\
";
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
  if( GLX_SGI_swap_control ) glXSwapIntervalSGI(1);
}


static void alwaysOnTop() {
  long propvalue = 12;
  XChangeProperty( dpy, xWin, XA_WIN_LAYER, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&propvalue, 1 );
  XRaiseWindow(dpy, xWin);
}



static boolean
isWindowMapped(void) {
  XWindowAttributes attr;
  
  XGetWindowAttributes(dpy, xWin, &attr);
  return (attr.map_state != IsUnmapped);
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




boolean init_screen (int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  // screen size is in RGB pixels
  int dblbuf=1;
  boolean fsover=FALSE;

  char tmp[32];

  uint32_t modeopts=0;

  int renderEventBase;
  int renderErrorBase;
  int error;

  int numElements;

  Cursor invisibleCursor;
  Pixmap bitmapNoData;
  XColor black;
  static char noData[] = { 0,0,0,0,0,0,0,0 };

  Atom wmDelete;
  
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

  GLXFBConfig          *fbConfigs;

  XEvent                event;
  XSetWindowAttributes  swa;
  int                   swaMask;
  int                   numReturned;


  if (argc>0) {
    dblbuf=atoi(argv[0]);
    if (argc>1) {
      fsover=atoi(argv[1]);
    }
  }

  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"openGL plugin error: No palette was set !\n");
    return FALSE;
  }

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

    glXMakeCurrent( dpy, glxWin, context );

    width=attr.width;
    height=attr.height;
    glXGetConfig(dpy, xvis, GLX_DOUBLEBUFFER, &swapFlag);
    is_ext=TRUE;
  }
  else {
    width=fullscreen?m_WidthFS:width;
    height=fullscreen?m_HeightFS:height; 

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
    
    swa.colormap = XCreateColormap( dpy, RootWindow(dpy, vInfo->screen),
				    vInfo->visual, AllocNone );
    
    if (!swa.colormap) {
      fprintf(stderr,"openGL plugin error: No colormap could be set !\n");
      return FALSE;
    }
    
    swaMask = CWBorderPixel | CWColormap | CWEventMask;
    
    swa.border_pixel = 0;

    xWin = XCreateWindow( dpy, RootWindow(dpy, vInfo->screen), 0, 0,
			  width,height,
			  0, vInfo->depth, InputOutput, vInfo->visual,
			  swaMask, &swa );

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
    
    glXMakeContextCurrent( dpy, glxWin, glxWin, context );

    XFree (vInfo);

    black.red = black.green = black.blue = 0;
    
    bitmapNoData = XCreateBitmapFromData( dpy, xWin, noData, 8, 8 );
    invisibleCursor = XCreatePixmapCursor( dpy, bitmapNoData, bitmapNoData, 
					   &black, &black, 0, 0 );
    XDefineCursor( dpy, xWin, invisibleCursor );
    XFreeCursor( dpy, invisibleCursor );

    is_ext=FALSE;
  }

  if (glXIsDirect(dpy, context)) 
    is_direct=TRUE;
  else
    is_direct=FALSE;
    
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
  glClear( GL_COLOR_BUFFER_BIT );

  glFlush();
  if ( swapFlag ) glXSwapBuffers( dpy, glxWin );

  type=GL_RGBA;
  if (mypalette==WEED_PALETTE_RGB24) type=GL_RGB;
  if (mypalette==WEED_PALETTE_BGR24) type=GL_BGR;
  if (mypalette==WEED_PALETTE_BGRA32) type=GL_BGRA;

  has_gl_texture=FALSE;

  return TRUE;
}



static boolean Upload(uint8_t *src, uint32_t imgWidth, uint32_t imgHeight, uint32_t type) {
  uint32_t mipMapLevel = 0;
  GLenum m_TexTarget=GL_TEXTURE_2D;

  glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
  
  glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  // Upload it all
  glBindTexture( m_TexTarget, texID );

  glEnable( m_TexTarget );

  glTexImage2D( m_TexTarget, mipMapLevel, type, imgWidth, imgHeight, 0, type, GL_UNSIGNED_BYTE, src );
  glGenerateMipmap(m_TexTarget);

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

  has_gl_texture=TRUE;

  return TRUE;
}


static boolean render_frame_rgba (int hsize, int vsize, void **pixel_data) {

  Upload((uint8_t *)pixel_data[0], hsize, vsize, type);
  if (swapFlag) glXSwapBuffers( dpy, glxWin );

  return TRUE;
}


static boolean render_frame_unknown (int hsize, int vsize, void **pixel_data) {
  fprintf(stderr,"openGL plugin error: No palette was set !\n");
  return FALSE;
}


boolean render_frame (int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // call the function which was set in set_palette
  return render_fn (hsize,vsize,pixel_data);
}


void exit_screen (int16_t mouse_x, int16_t mouse_y) {
  if (has_gl_texture)
    glDeleteTextures(1,&texID);

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

}




boolean send_keycodes (keyfunc host_key_fn) {
  // poll for keyboard events, pass them back to the caller
  // return FALSE if there are no more codes to return
  uint16_t mod_mask,scancode=0;
  XEvent xEvent;
  KeySym keySymbol;

  if (host_key_fn==NULL || dpy == NULL) return FALSE;

  while ((volatile Display *)dpy!=NULL) {
    pthread_mutex_lock(&dpy_mutex);
    if ((volatile Display *)dpy!=NULL) {
      if (XCheckWindowEvent( dpy, xWin, KeyPressMask | KeyReleaseMask, &xEvent ) ) {
	int keysyms_per_keycode_return;
	keySymbol = (KeySym)XGetKeyboardMapping(dpy,xEvent.xkey.keycode,0,&keysyms_per_keycode_return);
	mod_mask=xEvent.xkey.state;
	pthread_mutex_unlock(&dpy_mutex);

	host_key_fn (xEvent.type == KeyPress, keySymbol, mod_mask);
      }
      else break;
    }
  }
  pthread_mutex_unlock(&dpy_mutex);

}






