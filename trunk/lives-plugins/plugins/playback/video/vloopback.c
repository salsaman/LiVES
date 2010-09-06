// LiVES - vloopback playback engine
// (c) G. Finch 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdio.h>

/////////////////////////////////////////////////////////////////

static char plugin_version[64]="LiVES vloopback output client 0.1";
static int palette_list[2];
static int mypalette;

//////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>

static struct video_window x_vidwin;
static struct video_picture x_vidpic;

static int vdevfd;

static char *vdevname;

//////////////////////////////////////////////

const char *module_check_init(void) {
  int ret;
  if ( ( ret = system( "/sbin/lsmod | grep vloopback" ) ) == 256 ) {
    fprintf (stderr, "vloopback output: vloopback module not found !\n");
    return "Vloopback module was not found.\nTry: sudo modprobe vloopback\n";
  }
  
  return NULL;
}

const char *version (void) {
  return plugin_version;
}

const char *get_description (void) {
  return "The vloopback playback plugin makes LiVES appear as a video device in /dev.\nThis is an experimental plugin\n";
}

uint64_t get_capabilities (int palette) {
  return 0;
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
vdevname|Video _device|string|\"/dev/video1\" \\n\
</params> \\n\
<param_window> \\n\
special|fileread|0|\\n\
</param_window> \\n\
<onchange> \\n\
</onchange> \\n\
";
}


int *get_palette_list(void) {
  palette_list[0]=WEED_PALETTE_RGB24;
  palette_list[1]=WEED_PALETTE_END;
  return palette_list;
}

boolean set_palette (int palette) {
  if (palette==WEED_PALETTE_RGB24) {
    mypalette=palette;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}

boolean init_screen (int width, int height, boolean fullscreen, uint32_t window_id, int argc, char **argv) {

  if (argc>0) {
    vdevname=strdup(argv[0]);
  }
  else vdevname=strdup("/dev/video1");

  vdevfd=open(vdevname, O_RDONLY);

  if (vdevfd==1) {
    fprintf (stderr, "vloopback output: cannot open %s\n",vdevname);
    return FALSE;
  }

  if( ioctl(vdevfd, VIDIOCGPICT, &x_vidpic) == -1) {
    fprintf (stderr, "vloopback output: cannot get palette for %s\n",vdevname);
    return FALSE;
  }

  x_vidpic.palette=VIDEO_PALETTE_RGB24;

  if( ioctl(vdevfd, VIDIOCSPICT, &x_vidpic) == -1) {
    fprintf (stderr, "vloopback output: cannot set palette for %s\n",vdevname);
    return FALSE;
  }

  if( ioctl(vdevfd, VIDIOCGWIN, &x_vidpic) == -1) {
    fprintf (stderr, "vloopback output: cannot get dimensions for %s\n",vdevname);
    return FALSE;
  }

  x_vidwin.width=width;
  x_vidwin.height=height;

  if( ioctl(vdevfd, VIDIOCSWIN, &x_vidpic) == -1) {
    fprintf (stderr, "vloopback output: cannot set dimensions for %s\n",vdevname);
    return FALSE;
  }

  return TRUE;
}


boolean render_frame (int hsize, int vsize, int64_t tc, void **pixel_data, void **return_data) {
  // hsize and vsize are in pixels (n-byte)
  size_t frame_size=hsize*vsize*3,bytes;

  bytes=write(vdevfd,pixel_data[0],frame_size);

  if (bytes!=frame_size) {
    fprintf (stderr, "Error writing frame to %s\n",vdevname);
    return FALSE;
  }

  return TRUE;
}

void exit_screen (int16_t mouse_x, int16_t mouse_y) {
  int xval=0;
  xval=close(vdevfd);
  free(vdevname);
}

