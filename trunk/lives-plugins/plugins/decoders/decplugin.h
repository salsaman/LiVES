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
  char *URI; // the URI of this cdata

  int nclips; // number of clips (titles) in container
  char container_name[512]; // name of container, e.g. "ogg" or NULL

  // plugin should init this to 0 if URI changes
  int current_clip; // current clip number in container (starts at 0, MUST be <= nclips) [rw host]

  // video data
  int width;
  int height;
  int64_t nframes;
  int interlace;

  // x and y offsets of picture within frame
  // for primary pixel plane
  int offs_x;
  int offs_y;
  int frame_width;
  int frame_height;

  float par; // pixel aspect ratio

  float fps;

  int *palettes;

  // plugin should init this to palettes[0] if URI changes
  int current_palette;  // current palette [rw host]; must be contained in palettes
  
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;
  char video_name[512]; // name of video codec, e.g. "theora" or NULL

  /* audio data */
  int arate;
  int achans;
  int asamps;
  boolean asigned;
  boolean ainterleaf;
  char audio_name[512]; // name of audio codec, e.g. "vorbis" or NULL

  void *priv; // private data for demuxer/decoder

} lives_clip_data_t;



// std functions
const char *version(void);


// pass in NULL clip_data for the first call, subsequent calls (if the URI, current_clip or current_palette changes) 
// should reuse the previous value. If URI or current_clip are invalid, clip_data will be freed and NULL returned.
// plugin may or may not check current_palette to see if it is valid

lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *clip_data);

// frame starts at 0
boolean get_frame(const lives_clip_data_t *cdata, int64_t frame, void **pixel_data);

// free clip data - this should be called for each instance before unloading the module
void clip_data_free(lives_clip_data_t *);




// opt fns
const char *module_check_init(void);

int64_t rip_audio (const lives_clip_data_t *, const char *fname, int64_t stframe, int64_t nframes, unsigned char **abuff);
void rip_audio_cleanup(const lives_clip_data_t *);

void module_unload(void);





#define MK_FOURCC(a, b, c, d) ((a<<24)|(b<<16)|(c<<8)|d)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __DECPLUGIN_H__
