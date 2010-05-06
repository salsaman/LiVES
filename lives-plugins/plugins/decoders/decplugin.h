// LiVES - decoder plugin header
// (c) G. Finch 2008 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


#ifndef __DECPLUGIN_H__
#define __DECPLUGIN_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <inttypes.h>


#ifdef IS_DARWIN
#ifndef lseek64
#define lseek64 lseek
#endif
#ifndef off64_t
#define off64_t off_t
#endif
#endif


typedef int boolean;
#define TRUE 1
#define FALSE 0

#define LIVES_INTERLACE_NONE 0
#define LIVES_INTERLACE_BOTTOM_FIRST 1
#define LIVES_INTERLACE_TOP_FIRST 2


typedef struct {
  int nclips; // number of clips in container
  int width;
  int height;
  int64_t nframes;
  int interlace;

  // x and y offsets of picture within frame
  // for primary pixel plane
  int offs_x;
  int offs_y;

  float par; // pixel aspect ratio

  float fps;

  int *palettes;

  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;

  char container_name[512]; // name of container, e.g. "ogg" or NULL
  char video_name[512]; // name of video codec, e.g. "theora" or NULL
  char audio_name[512]; // name of audio codec, e.g. "vorbis" or NULL

  int arate;
  int achans;
  int asamps;
  boolean asigned;
  boolean ainterleaf;
} lives_clip_data_t;



// std functions
const char *module_check_init(void);
const char *version(void);

// nclip starts at 0
const lives_clip_data_t *get_clip_data(const char *URI, int nclip);

// frame starts at 0
boolean get_frame(const char *URI, int nclip, int64_t frame, void **pixel_data);

// opt fns
int64_t rip_audio (const char *URI, int nclip, const char *fname, int64_t stframe, int64_t nframes, unsigned char **abuff);
void rip_audio_cleanup(void);
boolean set_palette(int palette);
void module_unload(void);

#define MK_FOURCC(a, b, c, d) ((a<<24)|(b<<16)|(c<<8)|d)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __DECPLUGIN_H__
