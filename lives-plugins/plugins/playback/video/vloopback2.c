// LiVES - vloopback2 playback engine for LiVES
// (c) G. Finch 2011 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#define PLUGIN_UID 0X9F9490472BEF94Dull

#include "lives-plugin.h"

#define PLUGIN_NAME "LiVES vloopback2 output"
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 1

#include "videoplugin.h"

#include <linux/videodev2.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#define _IGN_RET(a) ((void)((a) + 1))

/////////////////////////////////////////////////////////////////

static int palette_list[4];
static int clampings[3];
static int mypalette;
static int mysubspace = WEED_YUV_SUBSPACE_YCBCR;
static int myclamp;

//////////////////////////////////////////////////////////////////

#include <errno.h>
#include <dirent.h>
#include <unistd.h>

static int vdevfd;

static char *vdevname;

static char *audfile;

static char xfile[PATH_MAX];

static int aforms[2];

//////////////////////////////////////////////

static int xioctl(int fh, int request, void *arg) {
  int r;
  do {
    r = ioctl(fh, request, arg);
  } while (-1 == r && EINTR == errno);
  return r;
}


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

static char **get_vloopback2_devices(void) {
  char devname[512];
  struct dirent **namelist;
  int n;
  int fd;
  int i = -1;
  int ndevices = 0;

  struct v4l2_capability vid_caps;

  char **devnames = malloc(MAX_DEVICES * sizeof(char *));

  for (i = 0; i < MAX_DEVICES; devnames[i++] = NULL);

  n = scandir("/dev", &namelist, file_filter, alphasort);
  if (n < 0) return devnames;

  for (i = 0; i < n && ndevices < MAX_DEVICES - 1; i++) {
    sprintf(devname, "/dev/%s", namelist[i]->d_name);

    if ((fd = open(devname, O_RDWR)) == -1) {
      // could not open device
      continue;
    }

    if (xioctl(fd, VIDIOC_QUERYCAP, &vid_caps) < 0) {
      // not a video device
      close(fd);
      continue;
    }

    if (!(vid_caps.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
      // is not an output device
      close(fd);
      continue;
    }

    close(fd);
    devnames[ndevices++] = strdup(devname);
    //fprintf(stderr,"got %s\n",devname);
  }

  devnames[ndevices] = NULL;

  for (i = 0; i < n; free(namelist[i++]));
  free(namelist);

  return devnames;
}


///////////////////////////////////////////////////

const char *module_check_init(void) {
  char **vdevs = get_vloopback2_devices();

  FILE *fp;
  char buffer[PATH_MAX];

  int i = 0;

  if (!vdevs[0]) {
    free(vdevs);
    return "No vloopback2 devices were found\nInstall vloopback2 and then try: sudo modprobe v4l2loopback\n"
           "Also check the device permissions for /dev/video*.\n";
  }

  while (vdevs[i]) free(vdevs[i++]);
  free(vdevs);

  fp = popen("echo -n $(mktemp --tmpdir -q lives-v4l2-audio-XXXXXXXXXX)", "r");
  _IGN_RET(fgets(buffer, PATH_MAX, fp));
  pclose(fp);
  audfile = strdup(buffer);

  return NULL;
}


const char *get_description(void) {
  return "The vloopback2 playback plugin makes LiVES appear as a video device in /dev.\n"
         "It requires the vloopback2 kernel module which can be downloaded from\nhttps://github.com/umlaeute/v4l2loopback\n";
}


uint64_t get_capabilities(int palette) {return 0;}


static char ifbuf[32768];

const char *get_init_rfx(plugin_intentcap_t *icaps) {
  char **vdevs = get_vloopback2_devices();
  char devstr[30000];
  size_t slen = 0;

  if (!vdevs[0]) {
    free(vdevs);
    return "No vloopback2 devices were found\nInstall vloopback2 and then try: sudo modprobe webcamstudio\n"
           "Also check the device permissions.\n";
  }

  memset(devstr, 0, 1);

  for (int i = 0; vdevs[i]; free(vdevs[i++])) {
    snprintf(devstr + slen, 30000 - slen, "%s|", vdevs[i]);
    slen += strlen(vdevs[i]) + 1;
  }
  free(vdevs);

  snprintf(ifbuf, 32768,
           "<define>\\n"
           "|1.7\\n"
           "</define>\\n"
           "<language_code>\\n"
           "0xF0\\n"
           "</language_code>\\n"
           "<params> \\n"
           "vdevname|Video _device|string_list|0|\\n"
           "%s\\n"
           "</params>\\n", devstr);
  return ifbuf;
}


const int *get_palette_list(void) {
  palette_list[0] = WEED_PALETTE_UYVY;
  palette_list[1] = WEED_PALETTE_RGB24;
  palette_list[2] = WEED_PALETTE_RGBA32;
  palette_list[3] = WEED_PALETTE_END;
  return palette_list;
}


boolean set_palette(int palette) {
  if (palette == WEED_PALETTE_UYVY) {
    mypalette = palette;
    return TRUE;
  }
  if (palette == WEED_PALETTE_RGB24) {
    mypalette = palette;
    return TRUE;
  }
  if (palette == WEED_PALETTE_RGBA32) {
    mypalette = palette;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}


const int *get_yuv_palette_clamping(int palette) {
  if (palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_RGBA32) clampings[0] = -1;
  else {
    clampings[0] = WEED_YUV_CLAMPING_UNCLAMPED;
    clampings[1] = WEED_YUV_CLAMPING_CLAMPED;
    clampings[2] = -1;
  }
  return clampings;
}


boolean set_yuv_palette_clamping(int clamping_type) {
  myclamp = clamping_type;
  return TRUE;
}


boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  // width in pixels

  int idx = 0, ret_code;
  int mypid = getpid();
  char **vdevs;

  struct v4l2_capability vid_caps;
  struct v4l2_format vid_format;

  vdevfd = -1;

  if (argc > 0) idx = atoi(argv[0]);
  if (argc > 1) audfile = argv[1];

  vdevs = get_vloopback2_devices();
  if (vdevs[idx]) {
    vdevname = strdup(vdevs[idx]);
  } else vdevname = NULL;

  for (int i = 0; vdevs[i]; free(vdevs[i++]));
  free(vdevs);

  if (!vdevname) return FALSE;

  vdevfd = open(vdevname, O_RDWR);

  if (vdevfd == -1) {
    fprintf(stderr, "vloopback2 output: cannot open %s %s\n", vdevname, strerror(errno));
    return FALSE;
  }

  ret_code = xioctl(vdevfd, VIDIOC_QUERYCAP, &vid_caps);

  if (ret_code) {
    fprintf(stderr, "vloopback2 output: cannot ioct failed for %s\n", vdevname);
    return FALSE;
  }

  vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  ret_code = xioctl(vdevfd, VIDIOC_G_FMT, &vid_format);

  vid_format.fmt.pix.width = width;
  vid_format.fmt.pix.height = height;

  switch (mypalette) {
  case WEED_PALETTE_RGB24:
    vid_format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    vid_format.fmt.pix.bytesperline = width * 3;
    vid_format.fmt.pix.sizeimage = width * height * 3;
    break;
  case WEED_PALETTE_RGBA32:
    vid_format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
    vid_format.fmt.pix.bytesperline = width * 3;
    vid_format.fmt.pix.sizeimage = width * height * 3;
    break;
  case WEED_PALETTE_UYVY:
    vid_format.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
    vid_format.fmt.pix.bytesperline = width * 2;
    vid_format.fmt.pix.sizeimage = width * height * 2;
    break;
  }

  //vid_format.fmt.pix.field = V4L2_FIELD_NONE;
  //vid_format.fmt.pix.priv = 0;

  if (mypalette == WEED_PALETTE_UYVY) {
    if (mysubspace == WEED_YUV_SUBSPACE_BT709)
      vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
    else {
      if (myclamp == WEED_YUV_CLAMPING_UNCLAMPED)
        vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
      else vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;
    }
  } else vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

  ret_code = xioctl(vdevfd, VIDIOC_S_FMT, &vid_format);

  return TRUE;
}


boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // hsize and vsize are in [macro]pixels (n-byte)
  size_t frame_size, bytes;

  if (mypalette == WEED_PALETTE_RGB24 || mypalette == WEED_PALETTE_BGR24)
    frame_size = hsize * vsize * 3;
  else frame_size = hsize * vsize * 4;

  bytes = write(vdevfd, pixel_data[0], frame_size);

  if (bytes != frame_size) {
    fprintf(stderr, "Error %s writing frame of %ld bytes to %s\n", strerror(errno), frame_size, vdevname);
    return FALSE;
  }

  return TRUE;
}


void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  int xval = 0;
  if (vdevfd != -1) xval = close(vdevfd);
  if (vdevname) free(vdevname);
  xval = xval;
}

