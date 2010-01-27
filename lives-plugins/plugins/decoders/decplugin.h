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
#define LIVES_INTERLACE_PROGRESSIVE 3


typedef struct {
  int width;
  int height;
  int nframes;
  int interlace;
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;
  float fps;
  int *palettes;

  char container_name[512]; // name of container, e.g. "ogg" or NULL
  char video_name[512]; // name of video codec, e.g. "theora" or NULL
  char audio_name[512]; // name of audio codec, e.g. "vorbis" or NULL

  int arate;
  int achans;
  int asamps;
  int asigned;
  int ainterleaf;
} lives_clip_data_t;



// std functions
const char *module_check_init(void);
const char *version(void);
const lives_clip_data_t *get_clip_data(char *URI);
boolean get_frame(char *URI, int64_t frame, void **pixel_data);

// opt fns
boolean rip_audio (char *URI, char *fname, int stframe, int frames);
void rip_audio_cleanup(void);
boolean set_palette(int palette);
void module_unload(void);

#define MK_FOURCC(a, b, c, d) ((a<<24)|(b<<16)|(c<<8)|d)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __DECPLUGIN_H__
