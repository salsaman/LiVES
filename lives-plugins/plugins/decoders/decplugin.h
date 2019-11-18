// LiVES - decoder plugin header
// (c) G. Finch 2008 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef __DECPLUGIN_H__
#define __DECPLUGIN_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#ifndef ALLOW_UNUSED
#ifdef __GNUC__
#  define ALLOW_UNUSED  __attribute__((unused))
#else
#  define ALLOW_UNUSED
#endif
#endif

// palettes, etc. :: don't include weed-compat.h, since plugins need to #define stuff first
#ifdef NEED_LOCAL_WEED
#include "../../../libweed/weed-palettes.h"
#else
#include <weed/weed-palettes.h>
#endif

#if defined (IS_DARWIN) || defined (__FreeBSD__)
#ifndef lseek64
#define lseek64 lseek
#endif
#ifndef off64_t
#define off64_t off_t
#endif
#endif

typedef int boolean;
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

typedef enum {
  LIVES_INTERLACE_NONE = 0,
  LIVES_INTERLACE_BOTTOM_FIRST = 1,
  LIVES_INTERLACE_TOP_FIRST = 2
} lives_interlace_t;

/// good
#define LIVES_SEEK_FAST (1<<0)

/// not so good
#define LIVES_SEEK_NEEDS_CALCULATION (1<<1)
#define LIVES_SEEK_QUALITY_LOSS (1<<2)

// memfuncs
typedef void *(*malloc_f)(size_t);
typedef void (*free_f)(void *);
typedef void *(*memset_f)(void *, int, size_t);
typedef void *(*memcpy_f)(void *, const void *, size_t);
typedef void *(*realloc_f)(void *, size_t);
typedef void *(*calloc_f)(size_t, size_t);
typedef void *(*memmove_f)(void *, const void *, size_t);

typedef struct {
  malloc_f  *ext_malloc;
  free_f    *ext_free;
  memcpy_f  *ext_memcpy;
  memset_f  *ext_memset;
  memmove_f *ext_memmove;
  realloc_f *ext_realloc;
  calloc_f  *ext_calloc;

  // TODO use fix sized array
  char *URI; ///< the URI of this cdata

  int nclips; ///< number of clips (titles) in container
  char container_name[512]; ///< name of container, e.g. "ogg" or NULL

  char title[256];
  char author[256];
  char comment[256];

  /// plugin should init this to 0 if URI changes
  int current_clip; ///< current clip number in container (starts at 0, MUST be <= nclips) [rw host]

  // video data
  int width;
  int height;
  int64_t nframes;
  lives_interlace_t interlace;

  /// x and y offsets of picture within frame
  /// for primary pixel plane
  int offs_x;
  int offs_y;
  int frame_width;  ///< frame is the surrounding part, including any black border (>=width)
  int frame_height;

  float par; ///< pixel aspect ratio (sample width / sample height)

  float video_start_time;

  float fps;
  float max_decode_fps;

  // TODO use fix sized array
  int *palettes; ///< list of palettes which the format supports, terminated with WEED_PALETTE_END

  /// plugin should init this to palettes[0] if URI changes
  int current_palette;  ///< current palette [rw host]; must be contained in palettes

  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;
  int frame_gamma; ///< values WEED_GAMMA_UNKNOWN (0), WEED_GAMMA_SRGB (1), WEED_GAMMA_LINEAR (2)
  char video_name[512]; ///< name of video codec, e.g. "theora" or NULL

  /* audio data */
  int arate;
  int achans;
  int asamps;
  boolean asigned;
  boolean ainterleaf;
  char audio_name[512]; ///< name of audio codec, e.g. "vorbis" or NULL

  int seek_flag; ///< bitmap of seek properties

#define SYNC_HINT_AUDIO_TRIM_START (1<<0)
#define SYNC_HINT_AUDIO_PAD_START (1<<1)
#define SYNC_HINT_AUDIO_TRIM_END (1<<2)
#define SYNC_HINT_AUDIO_PAD_END (1<<3)

#define SYNC_HINT_VIDEO_PAD_START (1<<4)
#define SYNC_HINT_VIDEO_PAD_END (1<<5)

  int sync_hint;

  void *priv; ///< private data for demuxer/decoder
} lives_clip_data_t;

// std functions
const char *version(void);

/// pass in NULL clip_data for the first call, subsequent calls (if the URI, current_clip or current_palette changes)
/// should reuse the previous value. If URI or current_clip are invalid, clip_data will be freed and NULL returned.
///
/// plugin may or may not check current_palette to see if it is valid

// should be threadsafe, and clip_data should be freed with clip_data_free() when no longer required

lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *);

/// frame starts at 0
boolean get_frame(const lives_clip_data_t *, int64_t frame, int *rowstrides, int height, void **pixel_data);

/// free clip data - this should be called for each instance before unloading the module
void clip_data_free(lives_clip_data_t *);

// opt fns //////////////////////////////////////////

const char *module_check_init(void);

boolean set_palette(lives_clip_data_t *);

int64_t rip_audio(const lives_clip_data_t *, const char *fname, int64_t stframe, int64_t nframes, unsigned char **abuff);
boolean rip_audio_sequential(const lives_clip_data_t *, const char *fname);
void rip_audio_cleanup(const lives_clip_data_t *);

void module_unload(void);

// little-endian
#define get_le16int(p) (*(p + 1) << 8 | *(p))
#define get_le32int(p) ((get_le16int(p + 2) << 16) | get_le16int(p))
#define get_le64int(p) (int64_t)(((uint64_t)(get_le32int(p + 4)) << 32) | (uint64_t)(get_le32int(p)))

#define MK_FOURCC(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | d)

#define ABS(a) ((a) >= 0. ? (a) : -(a))

double get_fps(const char *uri);

enum LiVESMediaType {
  LIVES_MEDIA_TYPE_UNKNOWN = 0,
  LIVES_MEDIA_TYPE_VIDEO,
  LIVES_MEDIA_TYPE_AUDIO,
  LIVES_MEDIA_TYPE_DATA
};

#ifdef NEED_CLONEFUNC

static lives_clip_data_t *clone_cdata(lives_clip_data_t *clone, const lives_clip_data_t *cdata) ALLOW_UNUSED;

static lives_clip_data_t *clone_cdata(lives_clip_data_t *clone, const lives_clip_data_t *cdata) {
  if (clone == NULL || cdata == NULL) return NULL;
  if (cdata->URI != NULL) clone->URI = strdup(cdata->URI);
  clone->nclips = cdata->nclips;
  snprintf(clone->container_name, 512, "%s", cdata->container_name);
  clone->current_clip = cdata->current_clip;
  clone->width = cdata->width;
  clone->height = cdata->height;
  clone->nframes = cdata->nframes;
  clone->interlace = cdata->interlace;
  clone->offs_x = cdata->offs_x;
  clone->offs_y = cdata->offs_y;
  clone->frame_width = cdata->frame_width;
  clone->frame_height = cdata->frame_height;
  clone->par = cdata->par;
  clone->frame_gamma = WEED_GAMMA_UNKNOWN;
  clone->fps = cdata->fps;
  clone->max_decode_fps = cdata->max_decode_fps;
  if (cdata->palettes != NULL) clone->palettes[0] = cdata->palettes[0];
  clone->current_palette = cdata->current_palette;
  clone->YUV_sampling = cdata->YUV_sampling;
  clone->YUV_clamping = cdata->YUV_clamping;
  snprintf(clone->video_name, 512, "%s", cdata->video_name);
  clone->arate = cdata->arate;
  clone->achans = cdata->achans;
  clone->asamps = cdata->asamps;
  clone->asigned = cdata->asigned;
  clone->ainterleaf = cdata->ainterleaf;
  snprintf(clone->audio_name, 512, "%s", cdata->audio_name);
  clone->seek_flag = cdata->seek_flag;
  clone->sync_hint = cdata->sync_hint;

  clone->ext_malloc  = cdata->ext_malloc;
  clone->ext_free    = cdata->ext_free;
  clone->ext_memcpy  = cdata->ext_memcpy;
  clone->ext_memset  = cdata->ext_memset;
  clone->ext_memmove = cdata->ext_memmove;
  clone->ext_realloc = cdata->ext_realloc;
  clone->ext_calloc  = cdata->ext_calloc;

  snprintf(clone->author, 256, "%s", cdata->author);
  snprintf(clone->title, 256, "%s", cdata->title);
  snprintf(clone->comment, 256, "%s", cdata->comment);
  return clone;
}
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __DECPLUGIN_H__
