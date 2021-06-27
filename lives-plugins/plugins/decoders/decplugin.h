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

// decoder plugin API version
#define PLUGIN_API_VERSION_MAJOR 3
#define PLUGIN_API_VERSION_MINOR 0

#ifndef PLUGIN_TYPE
#define PLUGIN_TYPE PLUGIN_TYPE_DECODER
#endif

#ifndef PLUGIN_DEVSTATE
#define PLUGIN_DEVSTATE PLUGIN_DEVSTATE_NORMAL
#endif

#ifndef PLUGIN_PKGTYPE
#define PLUGIN_PKGTYPE PLUGIN_PKGTYPE_DYNAMIC
#endif

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

#if defined (IS_DARWIN) || defined (__FreeBSD__)
#ifndef lseek64
#define lseek64 lseek
#endif
#ifndef off64_t
#define off64_t off_t
#endif
#endif

#ifndef NEED_LOCAL_WEED
#include <weed/weed-plugin.h>
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#else
#include "../../../libweed/weed-plugin.h"
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-palettes.h"
#include "../../../libweed/weed-effects.h"
#include "../../../libweed/weed-utils.h"
#endif

#include "../../weed-plugins/weed-plugin-utils.c"

#include "lives-plugin.h"

typedef int boolean;

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
  double const_time; /// avg const time apart from seek / decode (e.g. memcpy)
  double ib_time; /// avg time to decode inter / b frame
  double k_time; /// avg time to decode keyframe
  double ks_time; /// avg time to decode keyframe following seek / flush
  double seekback_time; /// avg extra time per iframe to arrive at backwd kframe

  double xvals[64];  /// extra values which may be stored depending on codec
} adv_timing_t;

// memfuncs
typedef void *(*malloc_f)(size_t);
typedef void (*free_f)(void *);
typedef void *(*memset_f)(void *, int, size_t);
typedef void *(*memcpy_f)(void *, const void *, size_t);
typedef void *(*realloc_f)(void *, size_t);
typedef void *(*calloc_f)(size_t, size_t);
typedef void *(*memmove_f)(void *, const void *, size_t);

#if defined NEED_TIMING || !defined HAVE_GETENTROPY

#ifdef _POSIX_TIMERS
#include <time.h>
#else
#include <sys/time.h>
#endif

static inline int64_t get_current_ticks(void) {
  int64_t ret;
#if _POSIX_TIMERS
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ret = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  ret = tv.tv_sec * 1000000 + tv.tv_usec;
#endif
  return ret;
}
#endif

#ifndef HAVE_GETENTROPY

#include <string.h>

#define myfastrand1(fval) ((fval) ^ ((fval) << 13))
#define myfastrand2(fval) ((fval) ^ ((fval) >> 7))
#define myfastrand3(fval) ((fval) ^ ((fval) << 17))
#define myfastrand0(fval) (myfastrand3(myfastrand2(myfastrand1((fval)))))

static inline void myrand(void *ptr, size_t size) {
  static uint64_t fval = 0;
  if (fval == 0) {
    fval = 0xAAAAAAAAAAAAAAAA ^ (get_current_ticks() >> 17);
  }
  fval = myfastrand0(fval);
  memcpy(ptr, &fval, size);
}

#define LSD_RANDFUNC(ptr, size) myrand(ptr, size)
#endif

#include "../../../src/lsd.h"

/// good
#define LIVES_SEEK_FAST (1<<0)
#define LIVES_SEEK_FAST_REV (1<<1)

/// not so good
#define LIVES_SEEK_NEEDS_CALCULATION (1<<2)
#define LIVES_SEEK_QUALITY_LOSS (1<<3)

//typedef weed_plant_t weed_layer_t;

typedef struct _lives_clip_data {
  // fixed parLUt
  lives_struct_def_t lsd;

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

  int64_t kframe_dist; /// number of frames from one keyframe to the next, for fixed gop only, 0 if unknown
  int64_t kframe_dist_max; /// max number of frames fdetected rom one keyframe to the next, 0 if unknown
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

  adv_timing_t adv_timing;

  boolean debug;
} lives_clip_data_t;

/// pass in NULL clip_data for the first call, subsequent calls (if the URI, current_clip or current_palette changes)
/// should reuse the previous value. If URI or current_clip are invalid, clip_data will be freed and NULL returned.
///
/// plugin may or may not check current_palette to see if it is valid

// should be threadsafe, and clip_data should be freed with clip_data_free() when no longer required

lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *);

/// frame starts at 0
boolean get_frame(const lives_clip_data_t *, int64_t frame, int *rowstrides, int height, void **pixel_data);

/// free buffers when we aren't playing sequentially / on standby
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

double estimate_delay(const lives_clip_data_t *cdata, int64_t tframe);

// little-endian
#define get_le16int(p) (*(p + 1) << 8 | *(p))
#define get_le32int(p) ((get_le16int(p + 2) << 16) | get_le16int(p))
#define get_le64int(p) (int64_t)(((uint64_t)(get_le32int(p + 4)) << 32) | (uint64_t)(get_le32int(p)))

double get_fps(const char *uri);

#ifdef NEED_CLONEFUNC
#define CREATOR_ID "LiVES decoder plugin"
static const lives_struct_def_t *cdata_lsd = NULL;

static void make_acid(void) {
  cdata_lsd = lsd_create("lives_clip_data_t", sizeof(lives_clip_data_t), "debug", 6);
  if (!cdata_lsd) return;
  else {
    lives_special_field_t **specf = cdata_lsd->special_fields;
    lives_clip_data_t *cdata = (lives_clip_data_t *)calloc(1, sizeof(lives_clip_data_t));
    specf[0] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY
                                  | LIVES_FIELD_FLAG_FREE_ON_DELETE, cdata, &cdata->priv,
                                  "priv", 0, NULL, NULL, NULL);
    specf[1] = make_special_field(LIVES_FIELD_CHARPTR, cdata, &cdata->URI,
                                  "URI", 0, NULL, NULL, NULL);
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
#endif

static lives_clip_data_t *cdata_new(lives_clip_data_t *data) {
  lives_clip_data_t *cdata;
  if (data) cdata = data;
  else {
#ifdef NEED_CLONEFUNC
    if (!cdata_lsd) make_acid();
    if (!cdata_lsd) return NULL;
    cdata = lives_struct_create(cdata_lsd);
#else
    cdata = calloc(1, sizeof(lives_clip_data_t));
#endif
  }
  return cdata;
}


#ifdef NEED_CLONEFUNC
static lives_clip_data_t *clone_cdata(const lives_clip_data_t *cdata) {
  if (!cdata) return NULL;
  if (!cdata_lsd) make_acid();
  return lives_struct_copy((void *)&cdata->lsd);
}
#endif


/////////////////////////////////////////////////////

#ifdef NEED_INDEX

#include <pthread.h>

static pthread_mutex_t indices_mutex;
static int nidxc;

typedef struct _index_entry index_entry;

struct _index_entry {
  index_entry *next; ///< ptr to next entry
  int64_t dts; ///< dts of keyframe
  uint64_t offs;  ///< offset in file
};

typedef struct {
  index_entry *idxhh;  ///< head of head list
  index_entry *idxht; ///< tail of head list

  int nclients;
  lives_clip_data_t **clients;
  pthread_mutex_t mutex;
} index_container_t;

static index_container_t **indices;

static void index_free(index_entry *idx) {
  index_entry *cidx = idx, *next;

  while (cidx) {
    next = cidx->next;
    free(cidx);
    cidx = next;
  }
}

/// here we assume that pts of interframes > pts of previous keyframe
// should be true for most formats (except eg. dirac)

// we further assume that pts == dts for all frames

static index_entry *index_walk(index_entry *idx, int64_t pts) {
  index_entry *xidx = idx;
  while (xidx) {
    //fprintf(stderr, "VALS %ld %ld\n", pts, xidx->dts);
    //if (xidx->next)
    //fprintf(stderr, "VALS2 %ld\n", xidx->next->dts);
    //if (xidx->next) fprintf(stderr, "WALK: %ld %ld %ld\n", xidx->dts, pts, xidx->next->dts);
    if (pts >= xidx->dts && (!xidx->next || pts < xidx->next->dts)) return xidx;
    xidx = xidx->next;
  }
  /// oops. something went wrong
  return NULL;
}

static index_entry *index_add(index_container_t *idxc, uint64_t offset, int64_t pts) {
  //lives_mkv_priv_t *priv = cdata->priv;
  index_entry *nidx;
  index_entry *nentry;

  nidx = idxc->idxht;

  nentry = malloc(sizeof(index_entry));

  nentry->dts = pts;
  nentry->offs = offset;
  nentry->next = NULL;

  if (!nidx) {
    // first entry in list
    idxc->idxhh = idxc->idxht = nentry;
    return nentry;
  }

  if (nidx->dts < pts) {
    // last entry in list
    nidx->next = nentry;
    idxc->idxht = nentry;
    return nentry;
  }

  if (idxc->idxhh->dts > pts) {
    // before head
    nentry->next = idxc->idxhh;
    idxc->idxhh = nentry;
    return nentry;
  }

  nidx = index_walk(idxc->idxhh, pts);

  // after nidx in list

  nentry->next = nidx->next;
  nidx->next = nentry;

  return nentry;
}

static inline index_entry *index_get(index_container_t *idxc, int64_t pts) {
  return index_walk(idxc->idxhh, pts);
}

///////////////////////////////////////////////////////

static index_container_t *idxc_for(lives_clip_data_t *cdata) {
  // check all idxc for string match with URI
  index_container_t *idxc;
  int i;

  pthread_mutex_lock(&indices_mutex);

  for (i = 0; i < nidxc; i++) {
    if (indices[i]->clients[0]->current_clip == cdata->current_clip &&
        !strcmp(indices[i]->clients[0]->URI, cdata->URI)) {
      idxc = indices[i];
      // append cdata to clients
      idxc->clients = (lives_clip_data_t **)realloc(idxc->clients, (idxc->nclients + 1) * sizeof(lives_clip_data_t *));
      idxc->clients[idxc->nclients] = cdata;
      idxc->nclients++;
      //
      pthread_mutex_unlock(&indices_mutex);
      return idxc;
    }
  }

  indices = (index_container_t **)realloc(indices, (nidxc + 1) * sizeof(index_container_t *));

  // match not found, create a new index container
  idxc = (index_container_t *)malloc(sizeof(index_container_t));

  idxc->idxhh = NULL;
  idxc->idxht = NULL;

  idxc->nclients = 1;
  idxc->clients = (lives_clip_data_t **)malloc(sizeof(lives_clip_data_t *));
  idxc->clients[0] = cdata;
  pthread_mutex_init(&idxc->mutex, NULL);

  indices[nidxc] = idxc;
  pthread_mutex_unlock(&indices_mutex);

  nidxc++;

  return idxc;
}

static void idxc_release(lives_clip_data_t *cdata, index_container_t *idxc) {
  int i, j;

  if (!idxc) return;

  pthread_mutex_lock(&indices_mutex);

  if (idxc->nclients == 1) {
    // remove this index
    index_free(idxc->idxhh);
    free(idxc->clients);
    for (i = 0; i < nidxc; i++) {
      if (indices[i] == idxc) {
        nidxc--;
        for (j = i; j < nidxc; j++) {
          indices[j] = indices[j + 1];
        }
        free(idxc);
        if (nidxc == 0) {
          free(indices);
          indices = NULL;
        } else indices = (index_container_t **)realloc(indices, nidxc * sizeof(index_container_t *));
        break;
      }
    }
  } else {
    // reduce client count by 1
    for (i = 0; i < idxc->nclients; i++) {
      if (idxc->clients[i] == cdata) {
        // remove this entry
        idxc->nclients--;
        for (j = i; j < idxc->nclients; j++) {
          idxc->clients[j] = idxc->clients[j + 1];
        }
        idxc->clients = (lives_clip_data_t **)realloc(idxc->clients, idxc->nclients * sizeof(lives_clip_data_t *));
        break;
      }
    }
  }
  pthread_mutex_unlock(&indices_mutex);
}

static void idxc_release_all(void) {
  for (int i = 0; i < nidxc; i++) {
    index_free(indices[i]->idxhh);
    free(indices[i]->clients);
    free(indices[i]);
  }
  nidxc = 0;
}

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __DECPLUGIN_H__
