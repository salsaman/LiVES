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

#define CAM_FORMAT_DV 0
#define CAM_FORMAT_HDV 1

typedef struct {
  raw1394handle_t handle;
  raw1394handle_t rec_handle;
  int device;
  int format;
  boolean grabbed_clips;
  lives_pgid_t pgid;
} s_cam;

/////////////////////////

boolean rec(s_cam *cam);
void camplay(s_cam *cam);
void camstop(s_cam *cam);
void camrew(s_cam *cam);
void camff(s_cam *cam);
void campause(s_cam *cam);
void cameject(s_cam *cam);

void close_raw1394(raw1394handle_t handle);

char *find_free_camfile(int format);

void on_open_fw_activate(LiVESMenuItem *menuitem, livespointer format);


struct _dvgrabw {
  LiVESWidget *dialog;
  LiVESWidget *filent;
  LiVESWidget *dirent;
  LiVESWidget *stop;
  LiVESWidget *grab;
  LiVESWidget *play;
  LiVESWidget *quit;
  LiVESWidget *status_entry;
  LiVESWidget *split;
  LiVESXCursor *cursor;
  boolean playing;
  char *dirname;
  char *filename;
  s_cam *cam;
};


struct _dvgrabw *dvgrabw;

