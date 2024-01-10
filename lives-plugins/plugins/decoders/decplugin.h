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

#include <weed/weed-plugin.h>
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>

#include <weed/weed-plugin-utils/weed-plugin-utils.c>

#include "lives-plugin.h"

typedef int boolean;

typedef enum {
  LIVES_INTERLACE_NONE = 0,
  LIVES_INTERLACE_BOTTOM_FIRST = 1,
  LIVES_INTERLACE_TOP_FIRST = 2
} lives_interlace_t;

typedef struct {
  // for each of these values, 0. means the value has not / cannot be measured or estimated
  // (exceptionally ctimng_ratio starts as 1.)
  // a value < 0. implies a guessed / estimated value
  // a value > 0. signifies that the value has been measured

  // all these values are dynamic and may change between succesive reads

  double ctiming_ratio; // dynamic multiplier for timing info, depends on machine load and other factors.
  double const_time; /// avg const time apart from seek / decode (e.g. memcpy)
  double ib_time; /// avg time to decode inter / b frame
  double k_time; /// avg time to decode keyframe not following seek (if we can distinguish from ib_time. else use ib_time)
  double ks_time; /// avg time to seek and decode kframe
  double kb_time; /// avg time to seek / decode backwd kframe
  double blockread_time; /// avg time to read . parse a data block
  double seekback_time; // unused

  double xvals[64];  /// extra values which may be stored depending on codec
} adv_timing_t;

// memfuncs
typedef void *(*malloc_f)(size_t);
typedef void *(*calloc_f)(size_t, size_t);
typedef void *(*realloc_f)(void *, size_t);
typedef void (*free_f)(void *);
typedef void *(*memcpy_f)(void *, const void *, size_t);
typedef void *(*memset_f)(void *, int, size_t);
typedef int (*memcmp_f)(const void *, const void *, size_t);
typedef void *(*memmove_f)(void *, const void *, size_t);

#ifndef HAVE_GETENTROPY
#if defined _UNISTD_H && defined  _DEFAULT_SOURCE
#define HAVE_GETENTROPY
#endif
#endif

#if defined NEED_TIMING || !defined HAVE_GETENTROPY
#ifdef _POSIX_TIMERS
#include <time.h>
#else
#include <sys/time.h>
#endif

static inline int64_t get_current_nsec(void) {
  int64_t ret;
#if _POSIX_TIMERS
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ret = (ts.tv_sec * 1000000000 + ts.tv_nsec);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  ret = (tv.tv_sec * 1000000 + tv.tv_usec)  * 1000;
#endif
  return ret;
}

#endif

#ifndef HAVE_GETENTROPY
#include <string.h>
#define _myfastrand(fval, n) (fval ^ (fval << n))
#define myfastrand(fval) (_myfastrand(_myfastrand(_myfastrand((fval), 13), 7), 17))
static inline void myrand(void *ptr, size_t size) {
  static uint64_t fval = 0;
  if (!fval) fval = 0xAAAAAAAAAAAAAAAA ^ (get_current_usec() >> 17);
  fval = myfastrand(fval);
  memcpy(ptr, &fval, size);
}
#endif

#include "../../../src/lsd.h"

/// good
#define LIVES_SEEK_FAST (1<<0)
#define LIVES_SEEK_FAST_REV (1<<1)

/// not so good
#define LIVES_SEEK_NEEDS_CALCULATION (1<<2)
#define LIVES_SEEK_QUALITY_LOSS (1<<3)

typedef struct {
  malloc_f  *malloc;
  calloc_f  *calloc;
  realloc_f *realloc;
  free_f    *free;
  memcpy_f  *memcpy;
  memset_f  *memset;
  memcmp_f  *memcmp;
  memmove_f *memmove;
} ext_funcs_t;

typedef struct _lives_clip_data {
  // fixed part
  lsd_struct_def_t *lsd;

  ext_funcs_t ext_funcs;

  void *priv;

  char *URI; ///< the URI of this cdata

  int nclips; ///< number of clips (titles) in container
  char container_name[512]; ///< name of container, e.g. "ogg" (if known)

  char title[1024];
  char author[1024];
  char comment[1024];

  /// plugin should init this to 0 if URI changes
  int current_clip; ///< current clip number in container (starts at 0, MUST be <= nclips) [rw host]

  // video data
  int width, height;
  int64_t nframes; // number of frames in current clip
  lives_interlace_t interlace; // frame interlacing (if any)

  // the host may initialise this by creating an array of n ints, where n is the number of planes in the current palette
  // the plugin may then fill the n values with its own rowstride values. The host can then use the values on the
  // subsequent call to get_frame(). The plugin MUST set the values each time a frame is returned.
  int *rec_rowstrides;

  /// x and y offsets of picture within frame
  /// for primary pixel plane
  int offs_x, offs_y;
  ///< frame is the surrounding part, including any blank border ( >= width, height )
  int frame_width, frame_height;

  float par; ///< pixel aspect ratio (sample width / sample height) (default of 0. implies square pixels)

  float video_start_time; // if the clip is a chapter, thhen this can be set to the chapter start time, info only

  float fps; // playback frame rate (variable rates not supported currently)

  int *palettes; ///< list of palettes which the format supports, terminated with WEED_PALETTE_END

  /// plugin should init this to palettes[0] if URI changes
  int current_palette;  ///< current palette [rw host]; must be contained in palettes

  /// plugin can change per frame
  int YUV_sampling, YUV_clamping, YUV_subspace;
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

  int sync_hint; ///< hint to host how to correct in case of audio / video stream misalignments

  /// decoder details /////

  int64_t last_frame_decoded; // last frame read / decoded from video stream

  /// optional info ////////////////

  //< estimate of the forward frame difference beyond which it always becomes
  // quicker to re-seek rathere than decode sequntially
  // 0 means no estimate, it is suggested to set this either to kframe_dist if valid, else to some other measured value
  int64_t jump_limit;

  float max_decode_fps; ///< theoretical value with no memcpy
  int64_t fwd_seek_time; // deprecated

  // handling for keyframes / seek points
  // these values are intended for use with delay estimation
  // (it is ASSUMED that we can jump to any keyframe and begin decoding forward from there)


  boolean kframes_complete; /// TRUE if all keyframes have been mapped (e.g read from index)

  // Otherwise,
  // if keyframes are regularly spaced, then this information can be used to guess the positions
  // of as yet unmapped keyframes.

  int64_t kframe_dist;
  //////////////////////////////////

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

// estimate delay (secs) to decode tframe, assuming we just decooed from_frame
// the estimate may include - jumping forwards or backwards to a keyframae
// decoding the keyframe, then decoding or skipping over inter frames until we reach tframe
// decoding tframe. The estimate may also include the time to read the data from the start of the block(s)
// containing the keyframe until the end of the block(s) containing tframe and including inter frame blocks
double estimate_delay(const lives_clip_data_t *, int64_t tframe,  int64_t from_frame, double *confidence);

// if there are multiple decoders for the same source, then this function may be called to update common data
// for that source
int64_t update_stats(const lives_clip_data_t *);

// little-endian
#define get_le16int(p) (*(p + 1) << 8 | *(p))
#define get_le32int(p) ((get_le16int(p + 2) << 16) | get_le16int(p))
#define get_le64int(p) (int64_t)(((uint64_t)(get_le32int(p + 4)) << 32) | (uint64_t)(get_le32int(p)))

double get_fps(const char *uri);

#ifdef NEED_CLONEFUNC
#define CLASS_ID "LiVES decoder plugin"

static void lfd_setdef(void *strct, const char *stype, const char *fname, int64_t *ptr, void *data) {*ptr = -1;}
static void adv_timing_init(void *strct, const char *stype, const char *fname, adv_timing_t *adv, void *data) {adv->ctiming_ratio = 1.;}

static lives_clip_data_t *cdata_create(void) {
  static const lsd_struct_def_t *cdata_lsd = NULL;
  if (!cdata_lsd) {
    lives_clip_data_t *cdata = (lives_clip_data_t *)calloc(1, sizeof(lives_clip_data_t));
    cdata_lsd = lsd_create_p("lives_clip_data_t", cdata, sizeof(lives_clip_data_t), &cdata->lsd);
    if (!cdata_lsd) return NULL;
    lsd_add_special_field((lsd_struct_def_t *)cdata_lsd, "priv", LSD_FIELD_FLAG_ZERO_ON_COPY |
                          LSD_FIELD_FLAG_FREE_ON_DELETE, &cdata->priv, 0, cdata, NULL);
    lsd_add_special_field((lsd_struct_def_t *)cdata_lsd, "URI", LSD_FIELD_CHARPTR, &cdata->URI,
                          0, cdata, NULL);
    lsd_add_special_field((lsd_struct_def_t *)cdata_lsd, "title", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->title,
                          1024, cdata, NULL);
    lsd_add_special_field((lsd_struct_def_t *)cdata_lsd, "author", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->author,
                          1024, cdata, NULL);
    lsd_add_special_field((lsd_struct_def_t *)cdata_lsd, "comment", LSD_FIELD_FLAG_ZERO_ON_COPY, &cdata->comment,
                          1024, cdata, NULL);
    lsd_add_special_field((lsd_struct_def_t *)cdata_lsd, "palettes", LSD_FIELD_ARRAY, &cdata->palettes,
                          4, cdata, NULL);
    lsd_add_special_field((lsd_struct_def_t *)cdata_lsd, "last_frame_decoded", LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY,
                          &cdata->last_frame_decoded, 0, cdata, (lsd_field_init_cb)lfd_setdef, NULL, lsd_null_cb, lsd_null_cb);
    lsd_add_special_field((lsd_struct_def_t *)cdata_lsd, "adv_timing", LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY, &cdata->adv_timing,
                          0, cdata, (lsd_field_init_cb)adv_timing_init, NULL, lsd_null_cb, lsd_null_cb);
    free(cdata);
    lsd_struct_set_owner_uid((lsd_struct_def_t *)cdata_lsd, PLUGIN_UID);
  }
  return lsd_struct_create(cdata_lsd);
}

static int cdata_is_mine(lives_clip_data_t *data) ALLOW_UNUSED;
static int cdata_is_mine(lives_clip_data_t *data) {
  if (lsd_struct_get_owner_uid(data->lsd) == PLUGIN_UID) return 1;
  return 0;
}
#endif


static lives_clip_data_t *cdata_new(lives_clip_data_t *data) {
  lives_clip_data_t *cdata;
  if (data) cdata = data;
  else {
#ifdef NEED_CLONEFUNC
    cdata = cdata_create();
#else
    cdata = calloc(1, sizeof(lives_clip_data_t));
#endif
  }
  return cdata;
}

#ifdef NEED_CLONEFUNC
static lives_clip_data_t *clone_cdata(const lives_clip_data_t *cdata) {
  if (!cdata) return NULL;
  else {
    lives_clip_data_t *clone;
    clone = lsd_struct_copy(cdata->lsd);
    return clone;
  }
}
#endif

/////////////////////////////////////////////////////

#ifdef NEED_INDEX

#include <pthread.h>

typedef struct _index_entry index_entry;

struct _index_entry {
  index_entry *next; ///< ptr to next entry
  int64_t dts; /// dts or frame number as preferred
  uint64_t offs;  ///< offset in file
  int64_t data;
};

typedef struct {
  index_entry *idxhh;  ///< head of head list
  index_entry *idxht; ///< tail of head list
  int nclients;
  lives_clip_data_t **clients;
  pthread_mutex_t mutex;
} index_container_t;

static index_container_t **indices;

static inline void _index_free(index_container_t *idxc) {
  for (index_entry *cidx = idxc->idxhh, *next; cidx; cidx = next) {
    next = cidx->next;
    free(cidx);
  }
  free(idxc->clients);
  free(idxc);
}

static inline index_entry *_index_walk(index_entry *idx, int64_t pts) {
  for (index_entry *xidx = idx; xidx; xidx = xidx->next)
    if (pts >= xidx->dts && (!xidx->next || pts < xidx->next->dts)) return xidx;
  return NULL;
}

static index_entry *_index_add(index_container_t *idxc, uint64_t pts, int64_t offset, int64_t data) {
  if (!idxc) return NULL;
  else {
    index_entry *nidx = idxc->idxht, *nentry = calloc(1, sizeof(index_entry));
    nentry->dts = pts;
    nentry->offs = offset;
    nentry->data = data;

    if (!nidx) idxc->idxhh = idxc->idxht = nentry; // first entry in list
    else if (nidx->dts < pts) nidx->next = idxc->idxht = nentry; // last entry in list
    else if (idxc->idxhh->dts > pts) { // before head
      nentry->next = idxc->idxhh;
      idxc->idxhh = nentry;
    } else {
      nidx = _index_walk(idxc->idxhh, pts);
      if (nidx->dts == pts) {
        nidx->offs = nentry->offs;
        free(nentry);
        nentry = nidx;
      } else { // after nidx in list
        nentry->next = nidx->next;
        nidx->next = nentry;
      }
    }
    return nentry;
  }
}

static index_entry *index_add(index_container_t *idxc, uint64_t pts, int64_t offset) {
  return _index_add(idxc, pts, offset, 0);
}

static index_entry *index_add_with_data(index_container_t *idxc, uint64_t pts, int64_t offset, int64_t data) {
  return _index_add(idxc, pts, offset, data);
}



static inline index_entry *index_get(index_container_t *idxc, int64_t pts) {return _index_walk(idxc->idxhh, pts);}

static pthread_mutex_t indices_mutex = PTHREAD_MUTEX_INITIALIZER;
static int nidxc = 0;

static index_container_t *idxc_for(lives_clip_data_t *cdata) {
  // check all idxc for string match with URI
  index_container_t *idxc;
  int j;

  pthread_mutex_lock(&indices_mutex);

  for (int i = 0; i < nidxc; i++) {
    idxc = indices[i];
    if (idxc->clients[0] == cdata || idxc->clients[0]->current_clip != cdata->current_clip
        || strcmp(idxc->clients[0]->URI, cdata->URI)) continue;
    for (j = 1; j < idxc->nclients; j++) if (idxc->clients[j] == cdata) break;
    if (j < idxc->nclients) continue;

    // append cdata to clients
    idxc->clients =
      (lives_clip_data_t **)realloc(idxc->clients, (idxc->nclients + 1) * sizeof(lives_clip_data_t *));
    idxc->clients[idxc->nclients++] = cdata;
    //
    pthread_mutex_unlock(&indices_mutex);
    return idxc;
  }

  // match not found, or already in list - create a new index container
  indices = (index_container_t **)realloc(indices, (nidxc + 1) * sizeof(index_container_t *));
  indices[nidxc] = idxc = (index_container_t *)calloc(1, sizeof(index_container_t));

  idxc->nclients = 1;
  idxc->clients = (lives_clip_data_t **)malloc(sizeof(lives_clip_data_t *));
  idxc->clients[0] = cdata;
  pthread_mutex_init(&idxc->mutex, NULL);

  nidxc++;
  pthread_mutex_unlock(&indices_mutex);
  return idxc;
}

static void idxc_release(lives_clip_data_t *cdata, index_container_t *idxc) {
  int i, j;
  if (!idxc) return;

  pthread_mutex_lock(&indices_mutex);

  if (idxc->nclients == 1 && idxc->clients[0] == cdata) {
    // remove this index
    _index_free(idxc);
    for (i = 0; i < nidxc; i++)
      if (indices[i] == idxc) {
        nidxc--;
        for (j = i; j < nidxc; indices[j] = indices[j + 1], j++);
        if (nidxc == 0) {
          free(indices);
          indices = NULL;
        } else indices = (index_container_t **)realloc(indices, nidxc * sizeof(index_container_t *));
        break;
      }
  } else {
    // reduce client count by 1
    for (i = 0; i < idxc->nclients; i++)
      if (idxc->clients[i] == cdata) {
        // remove this entry
        idxc->nclients--;
        for (j = i; j < idxc->nclients; idxc->clients[j] = idxc->clients[j + 1], j++);
        idxc->clients =
          (lives_clip_data_t **)realloc(idxc->clients, idxc->nclients * sizeof(lives_clip_data_t *));
        break;
      }
  }
  pthread_mutex_unlock(&indices_mutex);
}

static void idxc_release_all(void) {
  pthread_mutex_lock(&indices_mutex);
  for (int i = 0; i < nidxc; i++) _index_free(indices[i]);
  free(indices);
  indices = NULL;
  nidxc = 0;
  pthread_mutex_unlock(&indices_mutex);
}

static int count_between(index_container_t *idxc, int64_t start, int64_t end, int64_t *tot) {
  int64_t xtot = 0;
  int count = 0;
  if (idxc) {
    for (index_entry *xidx = idxc->idxhh; xidx; xidx = xidx->next) {
      if (xidx->dts < start) continue;
      if (xidx->dts > end) return count;
      count++;
      xtot += xidx->offs;
    }
  }
  if (tot) *tot = xtot;
  return count;
}

typedef int64_t (*kframe_check_cb_f)(int64_t tframe, void *user_data);

// function can be called to try to determine if keyframes are regularly spaced. A return of 0 indicates no
// pattern, any other value will be the distance, however if there are unmapped keyframes, then this could be
// invalidated by discovery or non confroming or lack of conforming values
// the value returned may be used to set cdata->kframe_dist in order to aid delay estimates
//
// algorithm: we have an index of found kframes (xidx), but it may be incomplete
// we begin by taking the first non zero frame from the index, subtract 1 from the frame value
// then ask the decoder to seek to that frame. Generally, the decoder will seek to the kframe
// before the target. We find the frame that the decoder jumped to, and subtract this from the original
// frame number to get a distance in frames.
// We first check if both frame numbers are integer multiples of the distance, and if so then we check
// all other frames in the index likewise. If we succeed, then we assume that we have a regular keyframe
// distance

static int64_t idxc_analyse(index_container_t *idxc, double fpsc, kframe_check_cb_f chk_cb, void *cb_data) {
  // fpsc is conversion factor, frame == floor(dts * fpsc)
  int64_t frame, dist = 0, ndist;
  if (idxc && fpsc > 0.) {
    index_entry *xidx = idxc->idxhh;
    if (!xidx) return 0;
    // walk xidx, converting dts to frame number, skip over 0 frames
    for (frame = (int64_t)(xidx->dts * fpsc - 0.5); !frame; frame = (int64_t)(xidx->dts * fpsc - 0.5)) {
      xidx = xidx->next;
      if (!xidx) return 0;
    }
    // set dist == frame (eg. 64)
    dist = frame;
    // subtract 1 from frame, and find kf for it (eg. 63 -> 48)
    frame = (*chk_cb)(--frame, cb_data);
    if (frame) {
      // e.g 64 % 16
      if (dist % (dist - frame)) return 0;
      dist -= frame;
      if (frame % dist) return 0;
    }
    for (xidx = xidx->next; xidx; xidx = xidx->next) {
      frame = (int64_t)(xidx->dts * fpsc - 0.5);
      ndist = frame % dist;
      if (ndist) {
	if (!(dist % ndist)) dist = ndist;
	else return 0;
      }
      if (xidx->offs - frame > dist) return 0;
    }
  }
  return dist;
}

#endif // need index

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __DECPLUGIN_H__
