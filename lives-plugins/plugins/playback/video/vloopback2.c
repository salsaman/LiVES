// LiVES - vloopback2 playback engine
// (c) G. Finch 2011 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <linux/videodev2.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/ioctl.h>


/////////////////////////////////////////////////////////////////

static char plugin_version[64]="LiVES vloopback2 output client 1.0.0";
static int palette_list[4];
static int clampings[3];
static int mypalette;
static int mysubspace=WEED_YUV_SUBSPACE_YCBCR;
static int myclamp;

//////////////////////////////////////////////////////////////////

#include <errno.h>
#include <dirent.h>
#include <unistd.h>

static int vdevfd;

static char *vdevname;

static char *tmpdir;

static char xfile[4096];

static int aforms[2];

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

static char **get_vloopback2_devices(void) {
  char devname[256];
  struct dirent **namelist;
  int n;
  int fd;
  int i=-1;
  int ndevices=0;

  struct v4l2_capability vid_caps;

  char **devnames=malloc(MAX_DEVICES * sizeof(char *));

  for (i=0; i<MAX_DEVICES; devnames[i++]=NULL);

  n = scandir("/dev", &namelist, file_filter, alphasort);
  if (n < 0) return devnames;


  for (i=0; i < n && ndevices < MAX_DEVICES-1; i++) {
    sprintf(devname, "/dev/%s", namelist[i]->d_name);

    if ((fd = open(devname, O_RDWR | O_NONBLOCK)) == -1) {
      // could not open device
      continue;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &vid_caps) < 0) {
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
  char **vdevs = get_vloopback2_devices();

  char buf[16384];

  int i=0;
  int dummyvar,rfd;

  size_t ret;

  if (vdevs[0]==NULL) {
    free(vdevs);
    return "No vloopback2 devices were found\nInstall vloopback2 and then try: sudo modprobe v4l2loopback\nAlso check the device permissions for /dev/video*.\n";
  }

  while (vdevs[i]!=NULL) free(vdevs[i++]);
  free(vdevs);

  // get tempdir
  dummyvar=system("smogrify get_tempdir oggstream");
  rfd=open("/tmp/.smogrify.oggstream",O_RDONLY);
  ret=read(rfd,(void *)buf,16383);
  memset(buf+ret,0,1);

  tmpdir=strdup(buf);

  dummyvar=dummyvar; // keep compiler happy


  return NULL;
}

const char *version(void) {
  return plugin_version;
}

const char *get_description(void) {
  return "The vloopback2 playback plugin makes LiVES appear as a video device in /dev.\nIt requires the vloopback2 kernel module which can be downloaded from\nhttps://github.com/umlaeute/v4l2loopback\n";
}

uint64_t get_capabilities(int palette) {
  return 0;
}


const int *get_audio_fmts() {
  // this is not yet documented, but is an optional function to get a list of audio formats. If the user chooses to stream audio then it will be sent to a fifo file in the tempdir called livesaudio.stream, in one of the supported formats
  aforms[0]=1; // pcm
  aforms[1]=-1; // end

  return aforms;
}

const char rfx[32768];


const char *get_init_rfx(void) {
  char **vdevs = get_vloopback2_devices();
  char devstr[30000];
  size_t slen=0;
  int i=0;

  if (vdevs[0]==NULL) {
    free(vdevs);
    return "No vloopback2 devices were found\nInstall vloopback2 and then try: sudo modprobe webcamstudio\nAlso check the device permissions.\n";
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
afname|Send _audio to|string|/tmp/audio.wav|1024|\\n\
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
  palette_list[2]=WEED_PALETTE_RGBA32;
  palette_list[3]=WEED_PALETTE_END;
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
  if (palette==WEED_PALETTE_RGBA32) {
    mypalette=palette;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}

const int *get_yuv_palette_clamping(int palette) {
  if (palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_RGBA32) clampings[0]=-1;
  else {
    clampings[0]=WEED_YUV_CLAMPING_UNCLAMPED;
    clampings[1]=WEED_YUV_CLAMPING_CLAMPED;
    clampings[2]=-1;
  }
  return clampings;
}


boolean set_yuv_palette_clamping(int clamping_type) {
  myclamp=clamping_type;
  return TRUE;
}

static void make_path(const char *fname, int pid, const char *ext) {
  snprintf(xfile,4096,"%s/%s-%d.%s",tmpdir,fname,pid,ext);
}


boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  // width in pixels

  int i=0,idx=0,ret_code;
  int afd,audio=0;

  int mypid=getpid();

  int dummyvar;

  char cmd[8192];

  char *audfile=NULL;

  char **vdevs;

  struct v4l2_capability vid_caps;
  struct v4l2_format vid_format;

  vdevfd=-1;

  if (argc>0) idx=atoi(argv[0]);
  if (argc>1) audfile=argv[1];

  vdevs = get_vloopback2_devices();
  if (vdevs[idx]!=NULL) {
    vdevname=strdup(vdevs[idx]);
  } else vdevname=NULL;

  while (vdevs[i]!=NULL) free(vdevs[i++]);
  free(vdevs);

  if (vdevname==NULL) return FALSE;

  vdevfd=open(vdevname, O_WRONLY);

  if (vdevfd==-1) {
    fprintf(stderr, "vloopback2 output: cannot open %s %s\n",vdevname,strerror(errno));
    return FALSE;
  }

  ret_code = ioctl(vdevfd, VIDIOC_QUERYCAP, &vid_caps);

  if (ret_code) {
    fprintf(stderr, "vloopback2 output: cannot ioct failed for %s\n",vdevname);
    return FALSE;
  }

  vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  vid_format.fmt.pix.width = width;
  vid_format.fmt.pix.height = height;

  switch (mypalette) {
  case WEED_PALETTE_RGB24:
    vid_format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    vid_format.fmt.pix.bytesperline = width *3;
    vid_format.fmt.pix.sizeimage = width*height*3;
    break;
  case WEED_PALETTE_RGBA32:
    vid_format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
    vid_format.fmt.pix.bytesperline = width *3;
    vid_format.fmt.pix.sizeimage = width*height*3;
    break;
  case WEED_PALETTE_UYVY:
    vid_format.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
    vid_format.fmt.pix.bytesperline = width *2;
    vid_format.fmt.pix.sizeimage = width*height*2;
    break;
  }

  vid_format.fmt.pix.field = V4L2_FIELD_NONE;
  vid_format.fmt.pix.priv = 0;

  if (mypalette==WEED_PALETTE_UYVY) {
    if (mysubspace==WEED_YUV_SUBSPACE_BT709)
      vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
    else {
      if (myclamp==WEED_YUV_CLAMPING_UNCLAMPED)
        vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
      else vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;
    }
  } else vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

  ret_code = ioctl(vdevfd, VIDIOC_S_FMT, &vid_format);

  make_path("livesaudio",mypid,"stream");

  if (audfile!=NULL) {
    afd=open(xfile,O_RDONLY|O_NONBLOCK);
    if (afd!=-1) {
      audio=1;
      close(afd);
    }

    if (audio) {
      snprintf(cmd,8192,"/bin/cat %s > \"%s\" &",xfile,audfile);
      dummyvar=system(cmd);
    }
    dummyvar=dummyvar; // keep compiler happy
  }
  return TRUE;
}



boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // hsize and vsize are in [macro]pixels (n-byte)
  size_t frame_size,bytes;

  if (mypalette==WEED_PALETTE_RGB24||mypalette==WEED_PALETTE_BGR24)
    frame_size=hsize*vsize*3;
  else frame_size=hsize*vsize*4;

  bytes=write(vdevfd,pixel_data[0],frame_size);

  if (bytes!=frame_size) {
    fprintf(stderr, "Error %s writing frame to %s\n",strerror(errno),vdevname);
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

