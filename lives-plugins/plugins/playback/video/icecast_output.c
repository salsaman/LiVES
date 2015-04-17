// LiVES - ogg/theora/vorbis to icecast stream engine
// (c) G. Finch 2004 - 2011 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int mypalette=WEED_PALETTE_END;
static int palette_list[2];

static int clampings[3];
static int myclamp;

static char plugin_version[64]="LiVES ogg/theora/vorbis stream engine version 1.0";

static boolean(*render_fn)(int hsize, int vsize, void **pixel_data);
boolean render_frame_yuv420(int hsize, int vsize, void **pixel_data);
boolean render_frame_unknown(int hsize, int vsize, void **pixel_data);

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
  int bufn;
  int bufc;
  uint8_t ** *framebuf;
} yuv4m_t;


static yuv4m_t *yuv4mpeg;

yuv4m_t *yuv4mpeg_alloc(void) {
  yuv4m_t *yuv4mpeg = (yuv4m_t *) malloc(sizeof(yuv4m_t));
  if (!yuv4mpeg) return NULL;
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  //yuv4mpeg->dar = y4m_dar_4_3;
  return yuv4mpeg;
}



static void make_path(const char *fname, int pid, const char *ext) {
  snprintf(xfile,4096,"%s/%s-%d.%s",tmpdir,fname,pid,ext);
}

static uint8_t **blankframe;




static uint8_t **make_blankframe(size_t size, boolean clear) {
  uint8_t **planes;

  planes=(uint8_t **)malloc(3*sizeof(uint8_t *));

  if (!planes) return NULL;

  planes[0]=(uint8_t *)malloc(size);

  if (!planes[0]) {
    free(planes);
    return NULL;
  }

  if (clear) {
    if (myclamp==WEED_YUV_CLAMPING_CLAMPED) memset(planes[0],16,size);
    else memset(planes[0],1,size);
  }

  size>>=2;

  planes[1]=(uint8_t *)malloc(size);
  if (!planes[1]) {
    free(planes[0]);
    free(planes);
    return NULL;
  }

  // yes i know....129....well some encoders may ignore "black frames" @ start
  if (clear) memset(planes[1],129,size);

  planes[2]=(uint8_t *)malloc(size);
  if (!planes[2]) {
    free(planes[1]);
    free(planes[0]);
    free(planes);
    return NULL;
  }
  if (clear) memset(planes[2],128,size);

  return planes;
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
  y4m_init_stream_info(&(yuv4mpeg->streaminfo));
  y4m_init_frame_info(&(yuv4mpeg->frameinfo));
  yuv4mpeg->fd=-1;

  // get tempdir
  dummyvar=system("smogrify get_tempdir oggstream");
  rfd=open("/tmp/.smogrify.oggstream",O_RDONLY);
  ret=read(rfd,(void *)buf,16383);
  memset(buf+ret,0,1);

  tmpdir=strdup(buf);

  blankframe=NULL;

  dummyvar=dummyvar; // keep compiler happy

  return NULL;
}


const char *version(void) {
  return plugin_version;
}

const char *get_description(void) {
  return "The icecast_output plugin provides realtime encoding\n to an icecast2 server in ogg/theora/vorbis format.\nIt requires ffmpeg2theora, oggTranscode, oggfwd and oggJoin.\nTry first with small frame sizes and low fps.\nNB: oggTranscode can be downloaded as part of oggvideotools 0.8a\nhttp://sourceforge.net/projects/oggvideotools/files/\n";
}

const int *get_palette_list(void) {
  palette_list[0]=WEED_PALETTE_YUV420P;
  palette_list[1]=WEED_PALETTE_END;
  return palette_list;
}


uint64_t get_capabilities(int palette) {
  return 0;
}


const int *get_audio_fmts() {
  // this is not yet documented in the manual, but is an optional function to get a list of audio formats. If the user chooses to stream audio then it will be sent to a fifo file in the tempdir called livesaudio.stream, in one of the supported formats
  aforms[0]=3; // vorbis - see src/plugins.h
  aforms[1]=-1; // end

  return aforms;
}


const char *get_init_rfx(void) {
  return \
         "<define>\\n\
|1.7\\n\
</define>\\n\
<language_code>\\n\
0xF0\\n\
</language_code>\\n\
<params> \\n\
syncd|A/V Sync _delay (seconds)|num2|4.|0.|20.|\\n\
address|Icecast server _address|string|127.0.0.1|16|\\n\
port|Icecast server _port|num0|8000|1024|65535|\\n\
passwd|Icecast server pass_word|string|hackme|32|\\n\
mountpt|Icecast _mount point|string|/stream.ogg|80|\\n\
</params> \\n\
<param_window> \\n\
special|password|3|\\n\
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
  } else clampings[0]=-1;
  return clampings;
}



boolean set_yuv_palette_clamping(int clamping_type) {
  myclamp=clamping_type;
  return TRUE;
}



boolean set_palette(int palette) {
  if (!yuv4mpeg) return FALSE;
  if (palette==WEED_PALETTE_YUV420P) {
    mypalette=palette;
    render_fn=&render_frame_yuv420;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}

const char *get_fps_list(int palette) {
  return "12|16|8|4|2|1|20|24|24000:1001|25|30000:1001|30|60";
}


boolean set_fps(double in_fps) {
  if (in_fps>23.97599&&in_fps<23.9761) {
    yuv4mpeg->fps=y4m_fps_NTSC_FILM;
    return TRUE;
  }
  if (in_fps>=29.97&&in_fps<29.9701) {
    yuv4mpeg->fps=y4m_fps_NTSC;
    return TRUE;
  }
  yuv4mpeg->fps.n=(int)(in_fps);
  yuv4mpeg->fps.d=1;
  return TRUE;
}

static int audio;
#include <errno.h>

boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  int dummyvar;
  char cmd[8192];
  const char *ics=NULL,*icpw=NULL,*icmp=NULL;
  int afd;
  int icp=8000;
  int i;

  int mypid=getpid();

  double syncd=0;

  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"oggstream plugin error: No palette was set !\n");
    return FALSE;
  }

  if (argc>4) {
    syncd=strtod(argv[0],NULL);
    ics=argv[1];
    icp=atoi(argv[2]);
    icpw=argv[3];
    icmp=argv[4];
  }

  make_path("video",mypid,"ogv");
  unlink(xfile);
  make_path("video2",mypid,"ogv");
  unlink(xfile);
  make_path("video3",mypid,"ogv");
  unlink(xfile);
  make_path("stream",mypid,"fifo");
  unlink(xfile);

  yuv4mpeg->bufn=(int)(syncd*yuv4mpeg->fps.n+.5);
  if (syncd==0) yuv4mpeg->bufn=0;
  if (yuv4mpeg->bufn>0) {
    yuv4mpeg->bufc=1;
    yuv4mpeg->framebuf=(uint8_t ** *)malloc(yuv4mpeg->bufn*sizeof(uint8_t **));
    if (!yuv4mpeg->framebuf) return FALSE;
    for (i=0; i<yuv4mpeg->bufn; i++) yuv4mpeg->framebuf[i]=NULL;
  } else yuv4mpeg->bufc=0;

  make_path("stream",mypid,"fifo");
  mkfifo(xfile,S_IRUSR|S_IWUSR); // raw yuv4m
  make_path("video",mypid,"ogv");
  mkfifo(xfile,S_IRUSR|S_IWUSR); // raw ogg stream
  make_path("video2",mypid,"ogv");
  mkfifo(xfile,S_IRUSR|S_IWUSR); // corrected ogg stream
  make_path("video3",mypid,"ogv");
  mkfifo(xfile,S_IRUSR|S_IWUSR); // feed to oggfwd

  snprintf(cmd,8192,"ffmpeg2theora -f yuv4m -o %s/video-%d.ogv %s/stream-%d.fifo 2>/dev/null&",tmpdir,mypid,tmpdir,mypid);
  dummyvar=system(cmd);

  make_path("livesaudio",mypid,"stream");

  afd=open(xfile,O_RDONLY|O_NONBLOCK);
  if (afd!=-1) {
    audio=1;
    close(afd);
  } else audio=0;

  if (audio) {
    snprintf(cmd,8192,"oggTranscode %s/video-%d.ogv %s/video2-%d.ogv &",tmpdir,mypid,tmpdir,mypid);
    dummyvar=system(cmd);
    snprintf(cmd,8192,"oggJoin %s/video3-%d.ogv %s/video2-%d.ogv %s/livesaudio-%d.stream &",tmpdir,mypid,tmpdir,mypid,tmpdir,mypid);
    dummyvar=system(cmd);
  } else {
    snprintf(cmd,8192,"oggTranscode %s/video-%d.ogv %s/video3-%d.ogv &",tmpdir,mypid,tmpdir,mypid);
    dummyvar=system(cmd);
  }

  snprintf(cmd,8192,"oggfwd -d \"LiVES stream\" \"%s\" %d \"%s\" \"%s\" < %s/video3-%d.ogv &",ics,icp,icpw,icmp,tmpdir,mypid);
  dummyvar=system(cmd);

  // open first fifo for writing
  make_path("stream",mypid,"fifo");
  yuv4mpeg->fd=open(xfile,O_WRONLY);

  ov_vsize=ov_hsize=0;

  y4m_si_set_framerate(&(yuv4mpeg->streaminfo),yuv4mpeg->fps);
  y4m_si_set_interlace(&(yuv4mpeg->streaminfo), Y4M_ILACE_NONE);

  if (blankframe!=NULL) free(blankframe);
  blankframe=NULL;

  dummyvar=dummyvar; // keep compiler happy

  //y4m_log_stream_info(LOG_INFO, "lives-yuv4mpeg", &(yuv4mpeg->streaminfo));
  return TRUE;
}


boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // call the function which was set in set_palette
  return render_fn(hsize,vsize,pixel_data);
}

boolean render_frame_yuv420(int hsize, int vsize, void **pixel_data) {
  int i,z;
  size_t fsize;
  register int j;

  if ((ov_hsize!=hsize||ov_vsize!=vsize)) {
    //start new stream
    y4m_si_set_width(&(yuv4mpeg->streaminfo), hsize);
    y4m_si_set_height(&(yuv4mpeg->streaminfo), vsize);

    y4m_si_set_sampleaspect(&(yuv4mpeg->streaminfo), yuv4mpeg->sar);

    i = y4m_write_stream_header(yuv4mpeg->fd, &(yuv4mpeg->streaminfo));

    if (i != Y4M_OK) return FALSE;

    ov_hsize=hsize;
    ov_vsize=vsize;

    if (yuv4mpeg->bufn>0) {
      yuv4mpeg->bufc=1; // reset delay (for now)

      for (i=0; i<yuv4mpeg->bufn; i++) {
        if (yuv4mpeg->framebuf[i]!=NULL) {
          for (j=0; j<3; j++) {
            free(yuv4mpeg->framebuf[i][j]);
          }
          free(yuv4mpeg->framebuf[i]);
          yuv4mpeg->framebuf[i]=NULL;
        }
      }

      if (blankframe!=NULL) free(blankframe);
      blankframe=NULL;
    }

  }


  if (yuv4mpeg->bufn==0) {
    // no sync delay
    i = y4m_write_frame(yuv4mpeg->fd, &(yuv4mpeg->streaminfo),
                        &(yuv4mpeg->frameinfo), (uint8_t **)pixel_data);
  } else {
    // write frame to next slot in buffer
    z=yuv4mpeg->bufc-1;
    fsize=hsize*vsize;

    if (yuv4mpeg->framebuf[z]==NULL) {
      // blank to output
      yuv4mpeg->framebuf[z]=make_blankframe(fsize,FALSE);
      if (yuv4mpeg->framebuf[z]==NULL) return FALSE;

      if (blankframe==NULL) blankframe=make_blankframe(fsize,FALSE);
      if (blankframe==NULL) return FALSE; // oom

      i = y4m_write_frame(yuv4mpeg->fd, &(yuv4mpeg->streaminfo),
                          &(yuv4mpeg->frameinfo), blankframe);
    } else {
      // old frame to op
      i = y4m_write_frame(yuv4mpeg->fd, &(yuv4mpeg->streaminfo),
                          &(yuv4mpeg->frameinfo), (uint8_t **)yuv4mpeg->framebuf[z]);
    }

    for (j=0; j<3; j++) {
      memcpy(yuv4mpeg->framebuf[z][j],pixel_data[j],fsize);
      if (j==0) fsize>>=2;
    }

    yuv4mpeg->bufc++;
    if (yuv4mpeg->bufc>yuv4mpeg->bufn) yuv4mpeg->bufc=1;

  }

  if (i != Y4M_OK) return FALSE;


  return TRUE;
}

boolean render_frame_unknown(int hsize, int vsize, void **pixel_data) {
  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"ogg_stream plugin error: No palette was set !\n");
  }
  return FALSE;
}

void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  int dummyvar;
  int mypid=getpid();

  int i,j;

  y4m_fini_stream_info(&(yuv4mpeg->streaminfo));
  y4m_fini_frame_info(&(yuv4mpeg->frameinfo));

  if (yuv4mpeg->fd!=-1) {
    close(yuv4mpeg->fd);
    yuv4mpeg->fd=-1;
  }

  dummyvar=system("pkill -g 0 -P 1");

  make_path("video",mypid,"ogv");
  unlink(xfile);
  make_path("video2",mypid,"ogv");
  unlink(xfile);
  make_path("video3",mypid,"ogv");
  unlink(xfile);
  make_path("stream",mypid,"fifo");
  unlink(xfile);

  if (blankframe!=NULL) free(blankframe);
  blankframe=NULL;

  if (yuv4mpeg->bufc!=0) {
    if (yuv4mpeg->bufc<0) {
      yuv4mpeg->bufn=-yuv4mpeg->bufc-1;
    }

    if (yuv4mpeg->framebuf!=NULL) {
      for (i=0; i<yuv4mpeg->bufn; i++) {
        if (yuv4mpeg->framebuf[i]!=NULL) {
          for (j=0; j<3; j++) {
            free(yuv4mpeg->framebuf[i][j]);
          }
          free(yuv4mpeg->framebuf[i]);
        }
      }
      free(yuv4mpeg->framebuf);
    }
  }

  dummyvar=dummyvar; // keep compiler happy


}



void module_unload(void) {
  if (yuv4mpeg!=NULL) {
    free(yuv4mpeg);
  }
  yuv4mpeg=NULL;
}


