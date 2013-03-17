// ldvgrab.h
// LiVES
// (c) G. Finch 2006 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


/* linux1394 includes */
#include <libraw1394/raw1394.h>
#include <libavc1394/rom1394.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>

typedef struct {
  raw1394handle_t handle;
  raw1394handle_t rec_handle;
  int device;
  int format;
  boolean grabbed_clips;
  pid_t pgid;
} s_cam;

/////////////////////////

boolean rec(s_cam *cam);
void camplay(s_cam *cam);
void camstop (s_cam *cam);
void camrew (s_cam *cam);
void camff (s_cam *cam);
void campause (s_cam *cam);
void cameject (s_cam *cam);

void close_raw1394(raw1394handle_t handle);

gchar *find_free_camfile(gint format);


struct _dvgrabw {
  GtkWidget *dialog;
  GtkWidget *filent;
  GtkWidget *stop;
  GtkWidget *grab;
  GtkWidget *play;
  GtkWidget *quit;
  GtkWidget *status_entry;
  GtkWidget *split;
  GdkCursor *cursor;
  boolean playing;
  gchar *dirname;
  gchar *filename;
  s_cam *cam;
};


struct _dvgrabw *dvgrabw;

