// LiVES - vloopback playback engine
// (c) G. Finch 2010 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdio.h>

/////////////////////////////////////////////////////////////////

static char plugin_version[64]="LiVES vloopback output client 1.0.2";
static int palette_list[3];
static int clampings[2];
static int mypalette;

//////////////////////////////////////////////////////////////////

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#if v4l1_INCFILE == 2
#include <libv4l1-videodev.h>
#else
#if v4l1_INCFILE == 1
#include <linux/videodev.h>
#endif
#endif

#include <sys/ioctl.h>

static struct video_window x_vidwin;
static struct video_picture x_vidpic;

static int vdevfd;

static char *vdevname;

//////////////////////////////////////////////


static int file_filter(const struct dirent *a) {
  int match = 0;

  // match: 'videoXY' where X = {0..9} and Y = {0..9}
  if (!strncmp(a->d_name, "video", 5)) {
    if (strlen(a->d_name) > 5) {
      if ((a->d_name[5] >= '0') && (a->d_name[5] <= '9'))      // match
        // the 'X'
      {
        match = 1;
      }

      if (strlen(a->d_name) > 6) {
        match = 0;

        if ((a->d_name[6] >= '0') && (a->d_name[6] <= '9')) {
          match = 1;
        }
      }

      if (strlen(a->d_name) > 7) {
        match = 0;
      }
    }
  }

  return match;
}


#define MAX_DEVICES 65

static char **get_vloopback_devices(void) {
  char devname[256];
  struct dirent **namelist;
  int n;
  int fd;
  int i=-1;
  int ndevices=0;
  struct video_capability v4lcap;
  char **devnames=malloc(MAX_DEVICES * sizeof(char *));

  for (i=0; i<MAX_DEVICES; devnames[i++]=NULL);

  n = scandir("/dev", &namelist, file_filter, alphasort);
  if (n < 0) return devnames;


  for (i=0; i < n && ndevices < MAX_DEVICES-1; i++) {
    sprintf(devname, "/dev/%s", namelist[i]->d_name);

    if ((fd = open(devname, O_RDONLY | O_NONBLOCK)) == -1) {
      // could not open device
      continue;
    }

    if (ioctl(fd, VIDIOCGCAP, &v4lcap) < 0) {
      // not a video device
      close(fd);
      continue;
    }

    // is it vloopback ?
    if (strstr(v4lcap.name,"loopback")==NULL) continue;

    if ((v4lcap.type & VID_TYPE_CAPTURE)) {
      // is an output device
      close(fd);
      continue;
    }

    close(fd);
    devnames[ndevices++]=strdup(devname);
    //fprintf(stderr,"got %s\n",devname);
  }
  devnames[ndevices]=NULL;

  for (i=0; i < n; free(namelist[i++]));
  free(namelist);

  return devnames;
}



///////////////////////////////////////////////////


const char *module_check_init(void) {
  char **vdevs = get_vloopback_devices();
  int i=0;

  if (vdevs[0]==NULL) {
    free(vdevs);
    return "No vloopback devices were found\nTry: sudo modprobe vloopback\n";
  }

  while (vdevs[i]!=NULL) free(vdevs[i++]);
  free(vdevs);

  return NULL;
}

const char *version(void) {
  return plugin_version;
}

const char *get_description(void) {
  return "The vloopback playback plugin makes LiVES appear as a video device in /dev.\n";
}

uint64_t get_capabilities(int palette) {
  return 0;
}

const char rfx[32768];

const char *get_init_rfx(void) {
  char **vdevs = get_vloopback_devices();
  char devstr[30000];
  size_t slen=0;
  int i=0;

  if (vdevs[0]==NULL) {
    free(vdevs);
    return "No vloopback devices were found\nTry: sudo modprobe vloopback\n";
  }

  memset(devstr, 0, 1);

  while (vdevs[i]!=NULL) {
    snprintf(devstr+slen,30000-slen,"%s|",vdevs[i]);
    slen+=strlen(vdevs[i])+1;
    free(vdevs[i++]);
  }
  free(vdevs);

  snprintf((char *)rfx,32768,"%s%s%s",
           "<define>\\n\
|1.7\\n\
</define>\\n\
<language_code>\\n\
0xF0\\n\
</language_code>\\n\
<params> \\n\
vdevname|Video _device|string_list|0|",
           devstr,
           "\\n\
</params> \\n\
<param_window> \\n\
</param_window> \\n\
<onchange> \\n\
</onchange> \\n\
"
          );

  return rfx;

}


const int *get_palette_list(void) {
  palette_list[0]=WEED_PALETTE_UYVY;
  palette_list[1]=WEED_PALETTE_RGB24;
  palette_list[2]=WEED_PALETTE_END;
  return palette_list;
}

boolean set_palette(int palette) {
  if (palette==WEED_PALETTE_UYVY) {
    mypalette=palette;
    return TRUE;
  }
  if (palette==WEED_PALETTE_RGB24) {
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
  int i=0,idx=0;
  char **vdevs;

  vdevfd=-1;

  if (argc>0) idx=atoi(argv[0]);

  vdevs = get_vloopback_devices();
  if (vdevs[idx]!=NULL) {
    vdevname=strdup(vdevs[idx]);
  } else vdevname=NULL;

  while (vdevs[i]!=NULL) free(vdevs[i++]);
  free(vdevs);

  if (vdevname==NULL) return FALSE;

  vdevfd=open(vdevname, O_WRONLY);

  if (vdevfd==-1) {
    fprintf(stderr, "vloopback output: cannot open %s %s\n",vdevname,strerror(errno));
    return FALSE;
  }

  if (ioctl(vdevfd, VIDIOCGPICT, &x_vidpic) == -1) {
    fprintf(stderr, "vloopback output: cannot get palette for %s\n",vdevname);
    return FALSE;
  }

  if (mypalette==WEED_PALETTE_RGB24) x_vidpic.palette=VIDEO_PALETTE_RGB24;
  else if (mypalette==WEED_PALETTE_UYVY) x_vidpic.palette=VIDEO_PALETTE_UYVY;

  if (ioctl(vdevfd, VIDIOCSPICT, &x_vidpic) == -1) {
    fprintf(stderr, "vloopback output: cannot set palette for %s\n",vdevname);
    return FALSE;
  }


  if (ioctl(vdevfd, VIDIOCGWIN, &x_vidwin) == -1) {
    fprintf(stderr, "vloopback output: cannot get dimensions for %s\n",vdevname);
    return FALSE;
  }

  x_vidwin.width=width;
  x_vidwin.height=height;

  if (ioctl(vdevfd, VIDIOCSWIN, &x_vidwin) == -1) {
    fprintf(stderr, "vloopback output: cannot set dimensions for %s\n",vdevname);
    return FALSE;
  }
  return TRUE;
}


boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // hsize and vsize are in [macro]pixels (n-byte)
  size_t frame_size,bytes;

  if (mypalette==WEED_PALETTE_UYVY) frame_size=hsize*vsize*4;
  else frame_size=hsize*vsize*3;

  bytes=write(vdevfd,pixel_data[0],frame_size);

  if (bytes!=frame_size) {
    fprintf(stderr, "Error writing frame to %s\n",vdevname);
    return FALSE;
  }

  return TRUE;
}

void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  int xval=0;
  if (vdevfd!=-1) xval=close(vdevfd);
  if (vdevname!=NULL) free(vdevname);
  xval=xval;
}

