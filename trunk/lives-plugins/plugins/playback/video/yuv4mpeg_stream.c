// LiVES - yuv4mpeg stream engine
// (c) G. Finch 2004 - 2012 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdio.h>

static int mypalette=WEED_PALETTE_END;
static int palette_list[2];

static int clampings[3];

static char plugin_version[64]="LiVES yuv4mpeg playback engine version 1.1";

static boolean(*render_fn)(int hsize, int vsize, void **pixel_data);
boolean render_frame_yuv420(int hsize, int vsize, void **pixel_data);
boolean render_frame_unknown(int hsize, int vsize, void **pixel_data);

static int ov_vsize,ov_hsize;

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

yuv4m_t *yuv4mpeg_alloc(void) {
  yuv4m_t *yuv4mpeg = (yuv4m_t *) malloc(sizeof(yuv4m_t));
  if (!yuv4mpeg) return NULL;
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  //yuv4mpeg->dar = y4m_dar_4_3;
  y4m_init_stream_info(&(yuv4mpeg->streaminfo));
  y4m_init_frame_info(&(yuv4mpeg->frameinfo));
  return yuv4mpeg;
}

//////////////////////////////////////////////


const char *module_check_init(void) {
  render_fn=&render_frame_unknown;
  ov_vsize=ov_hsize=0;

  yuv4mpeg=yuv4mpeg_alloc();

  return NULL;
}


const char *version(void) {
  return plugin_version;
}

const char *get_description(void) {
  return "The yuvmpeg_stream plugin allows streaming in yuv4mpeg format.\nOutput is on stdout, so it can be piped into another application.\n";
}

const int *get_palette_list(void) {
  palette_list[0]=WEED_PALETTE_YUV420P;
  palette_list[1]=WEED_PALETTE_END;
  return palette_list;
}


uint64_t get_capabilities(int palette) {
  return 0;
}


const int *get_yuv_palette_clamping(int palette) {
  if (palette==WEED_PALETTE_YUV420P) {
    clampings[0]=WEED_YUV_CLAMPING_UNCLAMPED;
    clampings[1]=WEED_YUV_CLAMPING_CLAMPED;
    clampings[2]=-1;
  } else clampings[0]=-1;
  return clampings;
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
  return "24|24000:1001|25|30000:1001|30|60";
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

boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"yuv4mpeg_stream plugin error: No palette was set !\n");
    return FALSE;
  }

  y4m_si_set_framerate(&(yuv4mpeg->streaminfo),yuv4mpeg->fps);
  y4m_si_set_interlace(&(yuv4mpeg->streaminfo), Y4M_ILACE_NONE);

  //y4m_log_stream_info(LOG_INFO, "lives-yuv4mpeg", &(yuv4mpeg->streaminfo));
  return TRUE;
}


boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // call the function which was set in set_palette
  return render_fn(hsize,vsize,pixel_data);
}

boolean render_frame_yuv420(int hsize, int vsize, void **pixel_data) {
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

boolean render_frame_unknown(int hsize, int vsize, void **pixel_data) {
  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"yuv4mpeg_stream plugin error: No palette was set !\n");
  }
  return FALSE;
}

void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  y4m_fini_stream_info(&(yuv4mpeg->streaminfo));
  y4m_fini_frame_info(&(yuv4mpeg->frameinfo));
}


void module_unload(void) {
  if (yuv4mpeg!=NULL) {
    free(yuv4mpeg);
  }
}


