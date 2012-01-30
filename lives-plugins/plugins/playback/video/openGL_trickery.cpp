// LiVES - openGL playback engine
// (c) G. Finch 2012 <salsaman@gmail.com>
// (c) OpenGL effects by Antti Silvast, 2012 <antti.silvast@iki.fi>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////////

static char plugin_version[64]="LiVES openGL trickery playback engine version 1.0";
static char error[256];

static int (*render_fn)(int hsize, int vsize, void **pixel_data, void **return_data);
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

typedef uint32_t uint32;

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
static boolean is_direct;
static boolean is_ext;

static volatile boolean playing;
static volatile boolean rthread_ready;
static volatile boolean has_texture;
static volatile boolean has_new_texture;

static uint8_t *texturebuf;

static int m_WidthFS;
static int m_HeightFS;

static int mode=0;

static uint32_t type;
static uint32_t typesize;

static float rquad;

static pthread_t rthread;
static pthread_mutex_t rthread_mutex;
static pthread_mutex_t swap_mutex;

static volatile uint32 imgWidth;
static volatile uint32 imgHeight;

static void *render_thread_func(void *data);

static Bool WaitForNotify( Display *dpy, XEvent *event, XPointer arg ) {
    return (event->type == MapNotify) && (event->xmap.window == (Window) arg);
}

static GLenum m_TexTarget=GL_TEXTURE_2D;

static float tfps=50.;


typedef struct {
  int width;
  int height;
  boolean fullscreen;
  uint64_t window_id;
  int argc;
  char **argv;
} _xparms;


//////////////////////////////////////////////


const char *module_check_init(void) {
  if( !GL_ARB_texture_non_power_of_two) {
    snprintf (error,256,"\n\nGL_ARB_texture_non_power_of_two unavailable.\nCannot use plugin.\n");
    return error;
  }

  render_fn=&render_frame_unknown;

  //glShadeModel( GL_FLAT );
  glShadeModel(GL_SMOOTH);  

  glClearDepth( 1.0f );
  glEnable( GL_DEPTH_TEST );

  glDepthFunc( GL_LEQUAL );
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glPixelStorei( GL_PACK_ALIGNMENT,   1 );
  glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
  glClearColor( 0.0, 0.0, 0.0, 0.0 );
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

const char *get_rfx (void) {
  return \
"<define>\\n\
|1.7\\n\
</define>\\n\
<language_code>\\n\
0xF0\\n\
</language_code>\\n\
<params> \\n\
mode|_Mode|string_list|0|Flat|Triangle|Rotating|Wobbler|Landscape|Insider|Cube|Turning|Tunnel\\n\
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




static boolean
isWindowMapped(void)
{
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




static void add_perspective(uint32_t width, uint32_t height) {
  glMatrixMode(GL_PROJECTION);                // Select The Projection Matrix
  glLoadIdentity();                           // Reset The Projection Matrix
 
  // Calculate The Aspect Ratio Of The Window
  gluPerspective(45.0f,(GLfloat)width/(GLfloat)height,0.1f,100.0f);
 
  glMatrixMode(GL_MODELVIEW);                 // Select The Modelview Matrix
  glLoadIdentity();                           // Reset The Modelview Matrix

}





boolean init_screen (int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  _xparms xparms;
  xparms.width=width;
  xparms.height=height;
  xparms.fullscreen=fullscreen;
  xparms.window_id=window_id;
  xparms.argc=argc;
  xparms.argv=argv;

  playing=TRUE;

  rthread_ready=FALSE;
  has_texture=FALSE;
  has_new_texture=FALSE;
  texturebuf=NULL;

  pthread_mutex_lock(&swap_mutex);

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
  int dblbuf=1;
  int fsover=0;

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

  mode=0;

  if (argc>0) {
    mode=atoi(argv[0]);
    if (argc>1) {
      dblbuf=atoi(argv[1]);
      if (argc>2) {
	fsover=atoi(argv[2]);
      }
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
    
     XFree (vInfo);

    black.red = black.green = black.blue = 0;
    
    bitmapNoData = XCreateBitmapFromData( dpy, xWin, noData, 8, 8 );
    invisibleCursor = XCreatePixmapCursor( dpy, bitmapNoData, bitmapNoData, 
					   &black, &black, 0, 0 );
    XDefineCursor( dpy, xWin, invisibleCursor );
    XFreeCursor( dpy, invisibleCursor );

    is_ext=FALSE;
  }

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

  if (mode==1||mode==2) add_perspective(width,height);

  /* OpenGL rendering ... */
  glClearColor( 0.0, 0.0, 0.0, 0.0 );
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glFlush();
  if ( swapFlag ) glXSwapBuffers( dpy, glxWin );

  type=GL_RGBA;
  typesize=4;
  if (mypalette==WEED_PALETTE_RGB24) {
    type=GL_RGB;
    typesize=3;
  }
  if (mypalette==WEED_PALETTE_BGR24) {
    type=GL_BGR;
    typesize=3;
  }
  if (mypalette==WEED_PALETTE_BGRA32) type=GL_BGRA;

  rquad=0.;

  glXMakeCurrent( dpy, glxWin, context );

  if (glXIsDirect(dpy, context)) 
    is_direct=TRUE;
  else
    is_direct=FALSE;

  return TRUE;
}


static boolean Upload(void) {

  pthread_mutex_lock(&rthread_mutex); // wait for lockout of texture thread

  if (has_new_texture) {
    uint32_t mipMapLevel=0;
    glEnable( m_TexTarget );

    has_new_texture=FALSE;
    glBindTexture( m_TexTarget, texID );
     
    glTexImage2D( m_TexTarget, mipMapLevel, type, imgWidth, imgHeight, 0, type, GL_UNSIGNED_BYTE, texturebuf );
    glGenerateMipmap(m_TexTarget);
    
    glDisable( m_TexTarget );
  }


  switch (mode) {
  case 0:
    {
      // flat:
      glEnable( m_TexTarget );
      
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
      
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
      
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

      glBindTexture( m_TexTarget, texID );
      
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

  case 1:
    {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glMatrixMode(GL_PROJECTION);                // Select The Projection Matrix
      glLoadIdentity();
      
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( m_TexTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    
      glTexParameteri( m_TexTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
      glTexParameteri( m_TexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      
      glBindTexture( m_TexTarget, texID );
      
      glEnable( m_TexTarget );
      
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

      glDisable( m_TexTarget );
    }
    break;

  case 2:
    {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // rotations
      glMatrixMode(GL_PROJECTION);                // Select The Projection Matrix
      glLoadIdentity();
 
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
        clock_gettime(CLOCK_MONOTONIC,&now);
        int ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

	float vx=-1.0,vy=1.0;
	float tx=0.0,ty=sin(ticks*0.001)*0.2;

	float vz;

	for (int j=0; j<WY; j++) {
		vx=-1.0; tx=0.0;
		for (int i=0; i<WX; i++) {
 		     	glBegin (GL_QUADS);
      
			float col=1.0-sin(i*0.05+j*0.06)*1.0;
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
 		     	glBegin (GL_QUADS);
      
			float col=1.0-sin(i*0.05+j*0.06)*1.0;
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
        clock_gettime(CLOCK_MONOTONIC,&now);
        int ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
        clock_gettime(CLOCK_MONOTONIC,&now);
        int ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
        clock_gettime(CLOCK_MONOTONIC,&now);
        int ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
        clock_gettime(CLOCK_MONOTONIC,&now);
        int ticks=now.tv_sec*1000+now.tv_nsec/1000000;

      	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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


	float tx=0.0,ty=(ticks % 2000)*0.0005;

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

  }

  pthread_mutex_unlock(&rthread_mutex); // re-enable texture thread

  pthread_mutex_unlock(&swap_mutex); // allow return of back buffer

  pthread_mutex_lock(&swap_mutex); // lock out data return

  if (swapFlag) glXSwapBuffers( dpy, glxWin );

  return TRUE;
}


static void *render_thread_func(void *data) {
  _xparms *xparms=(_xparms *)data;
  init_screen_inner (xparms->width, xparms->height, xparms->fullscreen, xparms->window_id, xparms->argc, xparms->argv);

  rthread_ready=TRUE;

  while (playing) {
    usleep(1000000./tfps);
    pthread_mutex_lock(&rthread_mutex);
    if (has_texture&&playing) {
      pthread_mutex_unlock(&rthread_mutex);
      Upload();
    }
    else pthread_mutex_unlock(&rthread_mutex);
  }
  return NULL;
}




boolean render_frame_rgba (int hsize, int vsize, void **pixel_data, void **return_data) {
  pthread_mutex_lock(&rthread_mutex); // wait for lockout of render thread

  if (hsize!=imgWidth || vsize!=imgHeight || texturebuf==NULL) {
    if (texturebuf!=NULL) {
      free(texturebuf);
    }
    texturebuf=(uint8_t *)malloc(hsize*vsize*typesize);
  }

  memcpy(texturebuf,pixel_data[0],hsize*vsize*typesize);

  has_texture=TRUE;
  has_new_texture=TRUE;

  imgWidth=hsize;
  imgHeight=vsize;

  pthread_mutex_unlock(&rthread_mutex); // re-enable render thread

  
  if (return_data!=NULL) {
    // needs testing

    // allow a render pass...

    pthread_mutex_lock(&swap_mutex); // hold render thread just before buffer swap

    glPushAttrib(GL_PIXEL_MODE_BIT);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  
    glReadBuffer(swapFlag?GL_BACK:GL_FRONT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, hsize, vsize, type, GL_UNSIGNED_BYTE, return_data[0]);
  
    glPopClientAttrib();
    glPopAttrib();
    pthread_mutex_unlock(&swap_mutex); // release render thread
  }
  return TRUE;
}


boolean render_frame_unknown (int hsize, int vsize, void **pixel_data, void **return_data) {
  fprintf(stderr,"openGL plugin error: No palette was set !\n");
  return FALSE;
}


boolean render_frame (int hsize, int vsize, int64_t tc, void **pixel_data, void **return_data) {
  // call the function which was set in set_palette
  return render_fn (hsize,vsize,pixel_data,return_data);
}


void exit_screen (int16_t mouse_x, int16_t mouse_y) {
  playing=FALSE;

  pthread_join(rthread,NULL);

  pthread_mutex_unlock(&swap_mutex);

  if (texturebuf!=NULL) {
    free(texturebuf);
  }

  if (!is_ext) {
    XUnmapWindow (dpy, xWin);
    XDestroyWindow (dpy, xWin);
  }

  XFlush(dpy);

  glXMakeContextCurrent(dpy, 0, 0, 0);
  glXDestroyContext(dpy, context);

  XCloseDisplay (dpy);
  dpy=NULL;
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

  while (dpy!=NULL && XCheckWindowEvent( dpy, xWin, KeyPressMask | KeyReleaseMask, &xEvent ) ) {
    keySymbol = XKeycodeToKeysym( dpy, xEvent.xkey.keycode, 0 );
    mod_mask=xEvent.xkey.state;
    host_key_fn (xEvent.type == KeyPress, keySymbol, mod_mask);
  }
}















