// machinestate.h
// LiVES
// (c) G. Finch 2003 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

/// disk/storage status values
typedef enum {
  LIVES_STORAGE_STATUS_UNKNOWN = 0,
  LIVES_STORAGE_STATUS_NORMAL,
  LIVES_STORAGE_STATUS_WARNING,
  LIVES_STORAGE_STATUS_CRITICAL,
  LIVES_STORAGE_STATUS_OFFLINE
} lives_storage_status_t;

#define INIT_LOAD_CHECK_COUNT 100 // initial loops to get started
#define N_QUICK_CHECKS 3 // how many times we should run with quick checks after that
#define QUICK_CHECK_TIME .1 // the time in sec.s between quick checks
#define TARGET_CHECK_TIME 1. // how often we should check after that

// ignore variance outside these limits
#define VAR_MAX 1.2
#define VAR_MIN .8
#define ME_DELAY 2.

#define LOAD_SCALING (100000. / ME_DELAY) // scale factor to get reasonable values

// internal memory allocator
typedef struct memheader {
  unsigned int    size;
  struct memheader   *next;
  size_t align;
} memheader_t;


void quick_free(memheader_t *bp);
void *quick_malloc(size_t alloc_size) GNU_MALLOC;

void init_random(void);

boolean load_measure_idle(livespointer data);

#ifdef ENABLE_ORC
livespointer lives_orc_memcpy(livespointer dest, livesconstpointer src, size_t n);
#endif

#ifdef ENABLE_OIL
livespointer lives_oil_memcpy(livespointer dest, livesconstpointer src, size_t n);
#endif

livespointer proxy_realloc(livespointer ptr, size_t new_size);

char *get_md5sum(const char *filename);

char *lives_format_storage_space_string(uint64_t space);
lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, uint64_t *dsval);
uint64_t get_fs_free(const char *dir);

ticks_t lives_get_relative_ticks(int64_t origsecs, int64_t origusecs);
ticks_t lives_get_current_ticks(void);
char *lives_datetime(struct timeval *tv);

boolean check_dev_busy(char *devstr);

uint64_t get_file_size(int fd);
uint64_t sget_file_size(const char *name);

void reget_afilesize(int fileno);

uint64_t reget_afilesize_inner(int fileno);

#ifdef PRODUCE_LOG
// disabled by default
void lives_log(const char *what);
#endif

lives_cancel_t check_for_bad_ffmpeg(void);

