// LiVES - ogg/theora/vorbis stream engine
// (c) G. Finch 2004 - 2011 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int mypalette=WEED_PALETTE_END;
static int palette_list[2];

static int clampings[3];

static char plugin_version[64]="LiVES ogg/theora/vorbis stream engine version 1.0";

static boolean (*render_fn)(int hsize, int vsize, void **pixel_data, void **return_data);
boolean render_frame_yuv420 (int hsize, int vsize, void **pixel_data, void **return_data);
boolean render_frame_unknown (int hsize, int vsize, void **pixel_data, void **return_data);

static int ov_vsize,ov_hsize;

static char *tmpdir;

static char xfile[4096];

static int aforms[2];

/////////////////////////////////////////////////////////////////////////

// yuv4mpeg specific stuff

#include <yuv4mpeg.h>

typedef struct {
  y4m_stream_info_t streaminfo;
  y4m_frame_info_t frameinfo;
  y4m_ratio_t sar;
  y4m_ratio_t dar;
  int fd;
  int hsize;
  int vsize;
  y4m_ratio_t fps;
} yuv4m_t;


static yuv4m_t *yuv4mpeg;

yuv4m_t *yuv4mpeg_alloc (void) {
  yuv4m_t *yuv4mpeg = (yuv4m_t *) malloc (sizeof(yuv4m_t));
  if(!yuv4mpeg) return NULL;
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  //yuv4mpeg->dar = y4m_dar_4_3;
  return yuv4mpeg;
}



static void make_path(const char *fname) {
  snprintf(xfile,4096,"%s/%s",tmpdir,fname);
}



//////////////////////////////////////////////


const char *module_check_init(void) {
  int rfd;
  char buf[16384];
  int dummyvar;

  ssize_t ret;
  // check all binaries are present


  render_fn=&render_frame_unknown;
  ov_vsize=ov_hsize=0;

  yuv4mpeg=yuv4mpeg_alloc();
  y4m_init_stream_info (&(yuv4mpeg->streaminfo));
  y4m_init_frame_info (&(yuv4mpeg->frameinfo));
  yuv4mpeg->fd=-1;

  // get tempdir
  dummyvar=system("smogrify get_tempdir oggstream");
  rfd=open("/tmp/.smogrify.oggstream",O_RDONLY);
  ret=read(rfd,(void *)buf,16384);
  memset(buf+ret,0,1);

  tmpdir=strdup(buf);

  return NULL;
}


const char *version (void) {
  return plugin_version;
}

const char *get_description (void) {
  return "The oggstream plugin provides realtime encoding to ogg/theora/vorbis format.\nIt requires ffmpeg2theora, oggTranscode and oggJoin.\nThe output file can be sent to a pipe or a file.\n";
}

const int *get_palette_list(void) {
  palette_list[0]=WEED_PALETTE_YUV420P;
  palette_list[1]=WEED_PALETTE_END;
  return palette_list;
}


uint64_t get_capabilities (int palette) {
  return 0;
}


const int *get_audio_fmts() {
  // this is not yet documented, but is an optional function to get a list of audio formats. If the user chooses to stream audio then it will be sent to a fifo file in the tempdir called livesaudio.stream, in one of the supported formats
  aforms[0]=3; // vorbis - see src/plugins.h
  aforms[1]=-1; // end

  return aforms;
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
output|Output _file|string|/tmp/output.ogv|1024|\\n\
</params> \\n\
<param_window> \\n\
</param_window> \\n\
<onchange> \\n\
</onchange> \\n\
";
}

const int *get_yuv_palette_clamping(int palette) {
  if (palette==WEED_PALETTE_YUV420P) {
    clampings[0]=WEED_YUV_CLAMPING_UNCLAMPED;
    clampings[1]=WEED_YUV_CLAMPING_CLAMPED;
    clampings[2]=-1;
  }
  else clampings[0]=-1;
  return clampings;
}


boolean set_palette (int palette) {
  if (!yuv4mpeg) return FALSE;
  if (palette==WEED_PALETTE_YUV420P) {
    mypalette=palette;
    render_fn=&render_frame_yuv420;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}

const char * get_fps_list (int palette) {
  return "24|24000:1001|25|30000:1001|30|60";
}


boolean set_fps (double in_fps) {
  if (in_fps>23.97599&&in_fps<23.9761) {
    yuv4mpeg->fps=y4m_fps_NTSC_FILM;
    return TRUE;
  }
  if (in_fps>=23.97&&in_fps<23.9701) {
    yuv4mpeg->fps=y4m_fps_NTSC;
    return TRUE;
  }
  yuv4mpeg->fps.n=(int)(in_fps);
  yuv4mpeg->fps.d=1;
  return TRUE;
}



boolean init_screen (int width, int height, boolean fullscreen, uint32_t window_id, int argc, char **argv) {
  int dummyvar;
  const char *outfile;
  char cmd[8192];
  int audio=0,afd;

  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"oggstream plugin error: No palette was set !\n");
    return FALSE;
  }

  if (argc>0) {
    outfile=argv[0];
  }
  else {
    outfile="-";
  }

  make_path("video.ogv");
  unlink(xfile);
  make_path("video2.ogv");
  unlink(xfile);
  make_path("stream.fifo");
  unlink(xfile);

  make_path("stream.fifo");
  mkfifo(xfile,S_IRUSR|S_IWUSR); // raw yuv4m
  make_path("video.ogv");
  mkfifo(xfile,S_IRUSR|S_IWUSR); // raw ogg stream
  make_path("video2.ogv");
  mkfifo(xfile,S_IRUSR|S_IWUSR); // corrected ogg stream

  snprintf(cmd,8192,"ffmpeg2theora -f yuv4m -o %s/video.ogv %s/stream.fifo 2>/dev/null&",tmpdir,tmpdir);
  dummyvar=system(cmd);

  make_path("livesaudio.stream");

  afd=open(xfile,O_RDONLY|O_NONBLOCK);
  if (afd!=-1) {
    audio=1;
    close(afd);
  }

  if (audio) {
    snprintf(cmd,8192,"oggTranscode %s/video.ogv %s/video2.ogv &",tmpdir,tmpdir); 
    dummyvar=system(cmd);
    snprintf(cmd,8192,"oggJoin \"%s\" %s/video2.ogv %s/livesaudio.stream &",outfile,tmpdir,tmpdir);
    dummyvar=system(cmd);
  }
  else {
    snprintf(cmd,8192,"oggTranscode %s/video.ogv \"%s\" &",tmpdir,outfile); 
    dummyvar=system(cmd);
  }
  // open fifo for writing

  make_path("stream.fifo");
  yuv4mpeg->fd=open(xfile,O_WRONLY);
  dup2(yuv4mpeg->fd,1);
  close(yuv4mpeg->fd);
  ov_vsize=ov_hsize=0;

  y4m_si_set_framerate(&(yuv4mpeg->streaminfo),yuv4mpeg->fps);
  y4m_si_set_interlace(&(yuv4mpeg->streaminfo), Y4M_ILACE_NONE);

  //y4m_log_stream_info(LOG_INFO, "lives-yuv4mpeg", &(yuv4mpeg->streaminfo));
  return TRUE;
 }


boolean render_frame (int hsize, int vsize, int64_t tc, void **pixel_data, void **return_data) {
  // call the function which was set in set_palette
  return render_fn (hsize,vsize,pixel_data,return_data);
}

boolean render_frame_yuv420 (int hsize, int vsize, void **pixel_data, void **return_data) {
  int i;

  if ((ov_hsize!=hsize||ov_vsize!=vsize)) {
    //start new stream
    y4m_si_set_width(&(yuv4mpeg->streaminfo), hsize);
    y4m_si_set_height(&(yuv4mpeg->streaminfo), vsize);
    
    y4m_si_set_sampleaspect(&(yuv4mpeg->streaminfo), yuv4mpeg->sar);
    
    i = y4m_write_stream_header(1, &(yuv4mpeg->streaminfo));

    if (i != Y4M_OK) return FALSE;

    ov_hsize=hsize;
    ov_vsize=vsize;
  }


  i = y4m_write_frame(1, &(yuv4mpeg->streaminfo),
  		&(yuv4mpeg->frameinfo), (uint8_t **)pixel_data);
  if (i != Y4M_OK) return FALSE;


  return TRUE;
}

boolean render_frame_unknown (int hsize, int vsize, void **pixel_data, void **return_data) {
  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"ogg_stream plugin error: No palette was set !\n");
  }
  return FALSE;
}

void exit_screen (int16_t mouse_x, int16_t mouse_y) {
  int dummyvar;

  y4m_fini_stream_info(&(yuv4mpeg->streaminfo));
  y4m_fini_frame_info(&(yuv4mpeg->frameinfo));

  if (yuv4mpeg->fd!=-1) {
    int new_fd=open("/dev/null",O_WRONLY);
    dup2(new_fd,1);
    close(new_fd);
  }

  // TODO - *** only kill our child processes
  dummyvar=system("killall -9 ffmpeg2theora 2>/dev/null");
  dummyvar=system("killall -9 OggTranscode 2>/dev/null");
  dummyvar=system("killall -9 OggJoin 2>/dev/null");

  make_path("video.ogv");
  unlink(xfile);
  make_path("video2.ogv");
  unlink(xfile);
  make_path("stream.fifo");
  unlink(xfile);

}



void module_unload(void) {
  if (yuv4mpeg!=NULL) {
    free (yuv4mpeg);
  }
}


