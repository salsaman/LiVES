// LiVES - decoder plugin header
// (c) G. Finch 2008 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef __DECPLUGIN_H__
#define __DECPLUGIN_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define DEC_PLUGIN_VERSION_MAJOR 3
#define DEC_PLUGIN_VERSION_MINOR 0

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

typedef struct {
  /// a ctiming_ratio of 0. indicates that none of the other values are set.
  // a value > 0. indicates some values are set. Unset values may be left as 0., or
  /// A value < 0. means that the value is known to be non-zero, but cannot be accurately measured.
  /// In this case, calculations involving this quantity should be avoided, as the result cannot be determined.

  double ctiming_ratio; // dynamic multiplier for timing info, depends on machine load and other factors.
  double idecode_time; /// avg time to decode inter frame
  double kdecode_time; /// avg time to decode keyframe
  double buffer_flush_time; /// time to flush buffers after a seek
  double kframe_nseek_time; /// avg time to seek to following keyframe (const)
  double kframe_delay_time; /// avg extra time per iframe to arrive at following kframe

  double kframe_kframe_time; /// avg time to seek from keyframe to keyframe (const) :: default == 0. (use kframe_nseek_time)
  double kframe_inter_time; /// extra time to seek from kframe to kframe per iframe between them :: default == kframe_delay_time
  double kframe_extra_time; /// extra time to seek from kframe to kframe per kframe between them :: default == kframe_inter_time

  // examples:
  // iframe to next kframe with decode: kframe_nseek_time + n * kframe_delay_time + buffer_flush_tome + kdecode_time
  // where n is the number of iframes skipped over

  // seek from iframe to another iframe, passing over several kframes, decoding frames from final kframe to target

  /// kframe_nseek_time + A * kframe_delay_time + kframe_kframe_time + B * kframe_inter_time * C * kframe_extra_time +
  /// kdecode_time + D * idecode_time
  /// where A == nframes between origin and next kframe, B == iframes between kframse, C == kframes between kframes,
  /// D = iframes after target kframe
  /// this can approximated as: kframe_nseek_time + (A + B + C) * kframe_delay_time + kdecode_time + D * idecode_time

  double xvals[64];  /// extra values which may be
} adv_timing_t;

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

#include "../../../src/lsd.h"

typedef struct {
  char type[16];  ///< "decoder"
  int api_version_major;
  int api_version_minor;
} lives_plugin_id_t;


typedef struct _lives_clip_data {
  // fixed part
  lives_struct_def_t lsd;
  lives_plugin_id_t plugin_id;

  malloc_f  *ext_malloc;
  free_f    *ext_free;
  memcpy_f  *ext_memcpy;
  memset_f  *ext_memset;
  memmove_f *ext_memmove;
  realloc_f *ext_realloc;
  calloc_f  *ext_calloc;

  void *priv;

  char *URI; ///< the URI of this cdata

  int nclips; ///< number of clips (titles) in container
  char container_name[512]; ///< name of container, e.g. "ogg" or NULL

  char title[1024];
  char author[1024];
  char comment[1024];

  /// plugin should init this to 0 if URI changes
  int current_clip; ///< current clip number in container (starts at 0, MUST be <= nclips) [rw host]

  // video data
  int width;
  int height;
  int64_t nframes;
  lives_interlace_t interlace;
  int *rec_rowstrides; ///< if non-NULL, plugin can set recommended vals, pointer to single value set by host

  /// x and y offsets of picture within frame
  /// for primary pixel plane
  int offs_x;
  int offs_y;
  int frame_width;  ///< frame is the surrounding part, including any black border (>=width)
  int frame_height;

  float par; ///< pixel aspect ratio (sample width / sample height)

  float video_start_time;

  float fps;

  /// optional info ////////////////
  float max_decode_fps; ///< theoretical value with no memcpy
  int64_t fwd_seek_time;
  int64_t jump_limit; ///< for internal use

  int64_t kframe_start; /// frame number of first keyframe (usually 0)
  int64_t kframe_dist; /// number forames from one keyframe to the next, 0 if unknown
  //////////////////////////////////

  int *palettes; ///< list of palettes which the format supports, terminated with WEED_PALETTE_END

  /// plugin should init this to palettes[0] if URI changes
  int current_palette;  ///< current palette [rw host]; must be contained in palettes

  /// plugin can change per frame
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

  /// plugin can change per frame
  int seek_flag; ///< bitmap of seek properties

#define SYNC_HINT_AUDIO_TRIM_START (1<<0)
#define SYNC_HINT_AUDIO_PAD_START (1<<1)
#define SYNC_HINT_AUDIO_TRIM_END (1<<2)
#define SYNC_HINT_AUDIO_PAD_END (1<<3)

#define SYNC_HINT_VIDEO_PAD_START (1<<4)
#define SYNC_HINT_VIDEO_PAD_END (1<<5)

  int sync_hint;

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

/// free buffers when we arent playing sequentially / on standby
boolean chill_out(const lives_clip_data_t *cdata);

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
#define CREATOR_ID "LiVES decoder plugin"
static const lives_struct_def_t *cdata_lsd = NULL;
#endif

static lives_clip_data_t *cdata_new(lives_clip_data_t *data) {
  lives_clip_data_t *cdata;
  if (data) cdata = data;
  else {
#ifdef NEED_CLONEFUNC
    if (!cdata_lsd) {
      cdata_lsd = lsd_create("lives_clip_data_t", sizeof(lives_clip_data_t), "strgs", 6);
      if (!cdata_lsd) return NULL;
      else {
        lives_special_field_t **specf = cdata_lsd->special_fields;
        cdata = (lives_clip_data_t *)calloc(1, sizeof(lives_clip_data_t));
        specf[0] = make_special_field(LIVES_FIELD_CHARPTR, cdata, &cdata->URI,
                                      "URI", 0, NULL, NULL, NULL);
        specf[1] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->priv,
                                      "priv", 0, NULL, NULL, NULL);
        specf[2] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->title,
                                      "title", 1024, NULL, NULL, NULL);
        specf[3] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->author,
                                      "author", 1024, NULL, NULL, NULL);
        specf[4] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, cdata, &cdata->comment,
                                      "comment", 1024, NULL, NULL, NULL);
        specf[5] = make_special_field(LIVES_FIELD_ARRAY, cdata, &cdata->palettes,
                                      "palettes", 4, NULL, NULL, NULL);
        lives_struct_init(cdata_lsd, cdata, &cdata->lsd);
        free(cdata);
        lives_struct_set_class_data((lives_struct_def_t *)cdata_lsd, CREATOR_ID);
      }
    }
    cdata = lives_struct_create(cdata_lsd);
#else
    cdata = calloc(1, sizeof(lives_clip_data_t));
#endif
  }
  if (cdata) {
    snprintf(cdata->plugin_id.type, 16, "%s", "decoder");
    cdata->plugin_id.api_version_major = DEC_PLUGIN_VERSION_MAJOR;
    cdata->plugin_id.api_version_major = DEC_PLUGIN_VERSION_MINOR;
  }
  return cdata;
}


#ifdef NEED_CLONEFUNC
static lives_clip_data_t *clone_cdata(lives_clip_data_t *clone, const lives_clip_data_t *cdata) {
  if (!cdata) return NULL;
  clone = lives_struct_copy((void *)&cdata->lsd);
  return clone;
}
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __DECPLUGIN_H__
