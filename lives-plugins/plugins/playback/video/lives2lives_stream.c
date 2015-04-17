// LiVES - LiVES stream engine
// (c) G. Finch 2008 - 2011 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifndef IS_MINGW
#include <netinet/in.h>
#endif

//////////////////////////////////////

static int palette_list[3];

static int clampings[3];

static char plugin_version[64]="LiVES to LiVES streaming engine version 1.1";

static boolean(*render_fn)(int hsize, int vsize, int64_t tc, void **pixel_data);
boolean render_frame_stream(int hsize, int vsize, int64_t tc, void **pixel_data);
boolean render_frame_unknown(int hsize, int vsize, int64_t tc, void **pixel_data);

/////////////////////////////////////////////////////////////////////////

typedef struct {
  int hsize;
  int vsize;
  double fps;
  int palette;
  int YUV_clamping;
  size_t mtu;
  void *handle;
} lives_stream_t;


static lives_stream_t *lstream;

//////////////////////////////////////////////
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

#define UNIXDG_PATH "/tmp/lives2lives"
#define UNIXDG_TMP "/tmp/lives2lives.XXXXXX"

typedef struct {
  struct sockaddr_in serv_addr;
  int sockfd;
  int len;
  void *addr;
} desc;


lives_stream_t *lstream_alloc(void) {
  lives_stream_t *lstream = (lives_stream_t *) malloc(sizeof(lives_stream_t));
  if (!lstream) return NULL;
  lstream->handle=NULL;
  lstream->YUV_clamping=WEED_YUV_CLAMPING_CLAMPED;
  return lstream;
}


void *OpenHTMSocket(char *host, int portnumber) {
  int sockfd;
  struct sockaddr_in cl_addr;
  desc *o;
  struct hostent *hostsEntry;
  uint64_t address;

  o = (desc *)malloc(sizeof(desc));
  if (o==NULL) return NULL;

  o->len = sizeof(cl_addr);
  memset((char *)&o->serv_addr, 0, sizeof(o->serv_addr));
  o->serv_addr.sin_family = AF_INET;

  hostsEntry = gethostbyname(host);
  if (hostsEntry == NULL) {
    herror(NULL);
    return NULL;
  }

  address = *((uint64_t *) hostsEntry->h_addr_list[0]);
  o->serv_addr.sin_addr.s_addr = address;

  o->serv_addr.sin_port = htons(portnumber);
  o->addr = &(o->serv_addr);

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
    memset((char *)&cl_addr, 0, sizeof(cl_addr));
    cl_addr.sin_family = AF_INET;
    cl_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    cl_addr.sin_port = htons(0);

    if (bind(sockfd, (struct sockaddr *) &cl_addr, sizeof(cl_addr)) < 0) {
      fprintf(stderr,"could not bind\n");
      close(sockfd);
      sockfd = -1;
    }
  } else fprintf(stderr,"unable to make socket\n");

  if (sockfd<0) {
    free(o);
    o = NULL;
  } else o->sockfd = sockfd;

  if (o!=NULL&&strcmp(host,"INADDR_ANY")) {
    connect(sockfd, o->addr, sizeof(cl_addr));
  }

  return o;
}


static boolean sendudp(const struct sockaddr *sp, int sockfd, int length, size_t count, void  *b) {
  ssize_t res;
  size_t mcount=count;
  if (lstream->mtu>0&&mcount>lstream->mtu) mcount=lstream->mtu;

  while (count>0) {
    if (mcount>count) mcount=count;
    if ((res=sendto(sockfd, b, mcount, 0, sp, length))==-1) {
      if (errno==EMSGSIZE) {
        mcount>>=1;
        lstream->mtu=mcount;
      } else return FALSE;
    } else {
      count-=res;
      b+=res;
    }
  }
  return TRUE;
}


static int lives_stream_out(void *buffer, size_t length) {
  desc *o = (desc *)(lstream->handle);
  return sendudp(o->addr, o->sockfd, o->len, length, buffer);
}


void lstream_close_socket(lives_stream_t *lstream) {
  desc *o = (desc *)(lstream->handle);
  close(o->sockfd);
  free(o);
}


////////////////

const char *module_check_init(void) {
  render_fn=&render_frame_unknown;

  lstream=lstream_alloc();

  return NULL;
}


const char *version(void) {
  return plugin_version;
}

const char *get_description(void) {
  return "The LiVES 2 LiVES stream plugin allows streaming to another copy of LiVES.\n";
}

const int *get_palette_list(void) {
  palette_list[0]=WEED_PALETTE_YUV420P;
  palette_list[1]=WEED_PALETTE_RGB24;
  palette_list[2]=WEED_PALETTE_END;
  return palette_list;
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
  if (clamping_type==WEED_YUV_CLAMPING_CLAMPED||clamping_type==WEED_YUV_CLAMPING_UNCLAMPED) {
    lstream->YUV_clamping=clamping_type;
    return TRUE;
  }
  return FALSE;
}


uint64_t get_capabilities(int palette) {
  return 0;
}


boolean set_palette(int palette) {
  if (!lstream) return FALSE;
  if (palette==WEED_PALETTE_YUV420P||palette==WEED_PALETTE_RGB24) {
    lstream->palette=palette;
    render_fn=&render_frame_stream;
    return TRUE;
  }
  // invalid palette
  return FALSE;
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
ip1|_IP Address|string|127|3| \\n\
ip2||string|0|3| \\n\
ip3||string|0|3| \\n\
ip4||string|1|3| \\n\
port|_Port|num0|8888|1|65535 \\n\
</params> \\n\
<param_window> \\n\
layout|\\\"Enter an IP address and port to stream to LiVES output to.\\\"| \\n\
layout|\\\"In the other copy of LiVES, you must select Advanced/Receive LiVES stream from...\\\"| \\n\
layout|\\\"You are advised to start with a small frame size and low framerate,\\\"| \\n\
layout|\\\"and increase this if your network bandwidth allows it.\\\"| \\n\
layout|p0|\\\".\\\"|p1|\\\".\\\"|p2|\\\".\\\"|p3|fill|fill|fill|fill| \\n\
layout|p4|fill\\n\
</param_window> \\n\
<onchange> \\n\
</onchange> \\n\
";
}

const char *get_fps_list(int palette) {
  return "8|12|16|24|25|30|50|60";
}


boolean set_fps(double in_fps) {
  lstream->fps=in_fps;
  return TRUE;
}

boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  char host[16];
  int port;

  if (lstream->palette==WEED_PALETTE_END) {
    fprintf(stderr,"lives2lives_stream plugin error: No palette was set !\n");
    return FALSE;
  }

  if (argc>0) {
    snprintf(host,16,"%s.%s.%s.%s",argv[0],argv[1],argv[2],argv[3]);
    port=atoi(argv[4]);
    lstream->handle=OpenHTMSocket(host,port);
    if (lstream->handle==NULL) {
      fprintf(stderr,"lives2lives_stream plugin error: Could not open port !\n");
      return FALSE;
    }
  }

  lstream->mtu=0;

  return TRUE;
}


boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // call the function which was set in set_palette
  return render_fn(hsize,vsize,tc,pixel_data);
}

boolean render_frame_stream(int hsize, int vsize, int64_t tc, void **pixel_data) {
  char hdrstr[128];
  size_t hdrstrlen;
  int mcount;
  int dsize=0;

  // send: 8 bytes "PACKET "
  // n bytes header

  // format is: (uint32_t)packet_type [1=video], (uint32_t)stream_id, (uint32_t)flags, (uint32_t)packet_length
  //
  // then for video type:
  // (int64_t)timecode, (int32_t)width in macropixels, (int32_t)height, (double)fps, (int32_t)palette
  // last 4 entries reserved for: (int32_t)yuv sampling [0=mpeg], (int32_t)yuv clamping [0=clamped,1=unclamped],
  // (int32_t)yuv subspace [1=YCbCr]; (int32_t)compression [0=uncompressed]

  // flags for video type are currently: bit 0 set == packet is continuation of current frame

  // 4 bytes "DATA"

  // packet_length bytes data

  // on stream end send "STREND" instead of "PACKET "

  if (lstream==NULL||lstream->handle==NULL) return FALSE;

  if (lstream->palette==WEED_PALETTE_YUV420P) dsize=hsize*vsize*3/2;
  else if (lstream->palette==WEED_PALETTE_RGB24) dsize=hsize*vsize*3;

  mcount=dsize*4;
  setsockopt(((desc *)(lstream->handle))->sockfd, SOL_SOCKET, SO_SNDBUF, (void *) &mcount, sizeof(mcount));

  snprintf(hdrstr,128,"1 0 0 %d %"PRId64" %d %d %.8f %d 1 %d 0 0 ", dsize, tc, hsize, vsize, lstream->fps, lstream->palette,
           lstream->YUV_clamping);

  hdrstrlen=strlen(hdrstr);

  lives_stream_out("PACKET ",7);
  lives_stream_out(hdrstr,hdrstrlen);
  lives_stream_out("DATA",4);

  if (lstream->palette==WEED_PALETTE_YUV420P) {
    lives_stream_out(pixel_data[0],hsize*vsize);
    lives_stream_out(pixel_data[1],(hsize*vsize)>>2);
    lives_stream_out(pixel_data[2],(hsize*vsize)>>2);
  } else if (lstream->palette==WEED_PALETTE_RGB24) {
    lives_stream_out(pixel_data[0],dsize);
  }

  return TRUE;
}


boolean render_frame_unknown(int hsize, int vsize, int64_t tc, void **pixel_data) {
  if (lstream->palette==WEED_PALETTE_END) {
    fprintf(stderr,"lives2lives_stream plugin error: No palette was set !\n");
    return 0;
  }
  return FALSE;
}

void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  if (lstream!=NULL&&lstream->handle!=NULL) {
    lives_stream_out("STREND",6);
    lstream_close_socket(lstream);
  }
  lstream->handle=NULL;
}


void module_unload(void) {
  if (lstream!=NULL) {
    free(lstream);
    lstream=NULL;
  }
}


