// machinestate.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _MACHINESTATE_H_
#define _MACHINESTATE_H_

#include <sys/time.h>
#include <time.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#undef PRId64
#undef PRIu64

#ifdef IS_MINGW
#define LONGSIZE 32
#else

#ifdef __WORDSIZE
#define LONGSIZE __WORDSIZE
#else
#if defined __x86_64__
# define LONGSIZE	64
#ifndef __WORDSIZE_COMPAT32
# define __WORDSIZE_COMPAT32	1
#endif
#else
# define LONGSIZE	32
#endif // x86
#endif // __WORDSIZE
#endif // mingw

#ifdef __PRI64_PREFIX
#undef __PRI64_PREFIX
#endif

# if LONGSIZE == 64
#  define __PRI64_PREFIX	"l"
# else
#  define __PRI64_PREFIX	"ll"
# endif

#undef PRId64
#undef PRIu64

# define PRId64		__PRI64_PREFIX "d"
# define PRIu64		__PRI64_PREFIX "u"

/// TODO: this file should be split into at least: memory functions, thread functions, file utils

#include "memory.h"
#include "threading.h"

/// disk/storage status values
typedef enum {
  LIVES_STORAGE_STATUS_UNKNOWN = 0,
  LIVES_STORAGE_STATUS_NORMAL,
  LIVES_STORAGE_STATUS_WARNING,
  LIVES_STORAGE_STATUS_CRITICAL,
  LIVES_STORAGE_STATUS_OVERFLOW,
  LIVES_STORAGE_STATUS_OVER_QUOTA,
  LIVES_STORAGE_STATUS_OFFLINE
} lives_storage_status_t;

//void shoatend(void);

#define WEED_LEAF_MD5SUM "md5sum"

// weed plants with type >= 16384 are reserved for custom use, so let's take advantage of that
#define WEED_PLANT_LIVES 31337

#define WEED_LEAF_LIVES_SUBTYPE "subtype"
#define WEED_LEAF_LIVES_MESSAGE_STRING "message_string"

#define LIVES_WEED_SUBTYPE_MESSAGE 1
#define LIVES_WEED_SUBTYPE_WIDGET 2
#define LIVES_WEED_SUBTYPE_TUNABLE 3
#define LIVES_WEED_SUBTYPE_PROC_THREAD 4
#define LIVES_WEED_SUBTYPE_TX_PARAM 5

#define LIVES_WEED_SUBTYPE_BAG_OF_HOLDING 256 // generic - cant think of a better name right now

weed_plant_t *lives_plant_new(int subtype);
weed_plant_t *lives_plant_new_with_index(int subtype, int64_t index);

void lives_get_randbytes(void *ptr, size_t size);

size_t lives_strlen(const char *) GNU_HOT GNU_PURE;
boolean lives_strcmp(const char *, const char *) GNU_HOT GNU_PURE;
boolean lives_strncmp(const char *, const char *, size_t) GNU_HOT GNU_PURE;
char *lives_strdup_quick(const char *s);
int lives_strcmp_ordered(const char *, const char *) GNU_HOT GNU_PURE;
char *lives_concat(char *, char *) GNU_HOT;
char *lives_concat_sep(char *st, const char *sep, char *x);
char *lives_strstop(char *, const char term) GNU_HOT;
int lives_strappend(const char *string, int len, const char *xnew);
const char *lives_strappendf(const char *string, int len, const char *fmt, ...);

void swab2(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void swab4(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void swab8(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void reverse_bytes(char *buff, size_t count, size_t granularity) 	GNU_HOT GNU_FLATTEN;
boolean reverse_buffer(uint8_t *buff, size_t count, size_t chunk) 	GNU_HOT;

uint64_t nxtval(uint64_t val, uint64_t lim, boolean less);
uint64_t autotune_u64_end(weed_plant_t **tuner, uint64_t val);
void autotune_u64(weed_plant_t *tuner,  uint64_t min, uint64_t max, int ntrials, double cost);

void init_random(void);
void lives_srandom(unsigned int seed);
uint64_t lives_random(void);

uint64_t fastrand(void) GNU_HOT;
void fastrand_add(uint64_t entropy);
double fastrand_dbl(double range);
uint32_t fastrand_int(uint32_t range);

uint64_t gen_unique_id(void);

#ifdef ENABLE_ORC
void *lives_orc_memcpy(void *dest, const void *src, size_t n);
#endif

#ifdef ENABLE_OIL
void *lives_oil_memcpy(void *dest, const void *src, size_t n);
#endif

void *proxy_realloc(void *ptr, size_t new_size);

char *get_md5sum(const char *filename);

char *lives_format_storage_space_string(uint64_t space);
lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, int64_t *dsval, int64_t resvd);
uint64_t get_ds_free(const char *dir);

lives_proc_thread_t disk_monitor_start(const char *dir);
boolean disk_monitor_running(const char *dir);
int64_t disk_monitor_check_result(const char *dir);
int64_t disk_monitor_wait_result(const char *dir, ticks_t timeout);
void disk_monitor_forget(void);

char *get_mountpoint_for(const char *dir);

ticks_t lives_get_relative_ticks(ticks_t origsecs, ticks_t orignsecs);
ticks_t lives_get_current_ticks(void);
char *lives_datetime(uint64_t secs, boolean use_local);
char *lives_datetime_rel(const char *datetime);
char *get_current_timestamp(void);

#define lives_nanosleep(nanosec) {struct timespec ts; ts.tv_sec = (uint64_t)nanosec / ONE_BILLION; \
    ts.tv_nsec = (uint64_t)nanosec - ts.tv_sec * ONE_BILLION; while (nanosleep(&ts, &ts) == -1 && \
								     errno != ETIMEDOUT);}
#define lives_nanosleep_until_nonzero(condition) {while (!(condition)) lives_nanosleep(1000);}

int check_dev_busy(char *devstr);

off_t get_file_size(int fd);
off_t sget_file_size(const char *name);

size_t reget_afilesize(int fileno);
off_t reget_afilesize_inner(int fileno);

off_t get_dir_size(const char *dirname);

boolean compress_files_in_dir(const char *dir, int method, void *data);
LiVESResponseType send_to_trash(const char *item);

/// extras we can check for, may consume more time
#define EXTRA_DETAILS_CHECK_MISSING	       	(1ull << 0)
#define EXTRA_DETAILS_DIRSIZE			(1ull << 1)
#define EXTRA_DETAILS_EMPTY_DIRS	       	(1ull << 2)
#define EXTRA_DETAILS_SYMLINK		       	(1ull << 3)
#define EXTRA_DETAILS_ACCESSIBLE	       	(1ull << 4)
#define EXTRA_DETAILS_WRITEABLE			(1ull << 5)
#define EXTRA_DETAILS_EXECUTABLE       		(1ull << 6)
#define EXTRA_DETAILS_CLIPHDR			(1ull << 7)

/// derived values
#define EXTRA_DETAILS_MD5SUM			(1ull << 33)

typedef struct {
  ///< if we can retrieve some kind of uinque id, we set it here
  ///< may be useful in future for dictionary lookups
  uint64_t uniq;
  lives_struct_def_t *lsd;
  char *name;
  uint64_t type; /// e.g. LIVES_FILE_TYPE_FILE
  off_t size; // -1 not checked, -2 unreadable
  uint64_t mode;
  uint64_t uid; /// userid as uint64_t
  uint64_t gid;
  uint64_t blk_size;
  uint64_t atime_sec;
  uint64_t atime_nsec;
  uint64_t mtime_sec;
  uint64_t mtime_nsec;
  uint64_t ctime_sec;
  uint64_t ctime_nsec;
  char *md5sum; /// only filled if EXTRA_DETAILS_MD5 is set, otherwise NULL
  char *extra_details;  /// initialized to NULL, set to at least ""
  LiVESWidget *widgets[16]; ///< caller set widgets for presentation, e.g. labels, spinners. final entry must be followed by NULL
} lives_file_dets_t;

#ifdef PRODUCE_LOG
// disabled by default
void lives_log(const char *what);
#endif

uint32_t lives_string_hash(const char *string) GNU_PURE GNU_HOT;
uint32_t fast_hash(const char *key) GNU_PURE GNU_HOT;
char *lives_chomp(char *string);
char *lives_strtrim(const char *buff);

int check_for_bad_ffmpeg(void);

void update_effort(int nthings, boolean badthings);
void reset_effort(void);

void free_fdets_list(LiVESList **);
lives_proc_thread_t dir_to_file_details(LiVESList **, const char *dir,
                                        const char *orig_loc, uint64_t extra);
lives_proc_thread_t ordfile_to_file_details(LiVESList **listp, const char *ofname,
    const char *orig_loc, uint64_t extra);

///// cmdline
char *grep_in_cmd(const char *cmd, int mstart, int npieces, const char *mphrase, int ridx, int rlen);

/// x11
char *get_wid_for_name(const char *wname);
boolean hide_x11_window(const char *wid);
boolean unhide_x11_window(const char *wid);
boolean activate_x11_window(const char *wid);
boolean show_desktop_panel(void);
boolean hide_desktop_panel(void);
boolean get_x11_visible(const char *wname);

int get_window_stack_level(LiVESXWindow *, int *nwins);

#define WM_XFWM4 "Xfwm4"
#define WM_XFCE4_PANEL "xfce4-panel"
#define WM_XFCE4_SSAVE "xfce4-ssave"
#define WM_XFCE4_COLOR "xfce4-color-settings"
#define WM_XFCE4_DISP "xfce4-display-settings"
#define WM_XFCE4_POW "xfce4-power-manager-settings"
#define WM_XFCE4_SETTINGS "xfce4-settings-manager"
#define WM_XFCE4_TERMINAL "xfce4-terminal"
#define WM_XFCE4_TASKMGR "xfce4-taskmanager"
#define WM_XFCE4_SSHOT "xfce4-screenshooter"

#define WM_KWIN "KWin"
#define WM_KWIN_PANEL ""
#define WM_KWIN_SSAVE ""
#define WM_KWIN_COLOR ""
#define WM_KWIN_DISP ""
#define WM_KWIN_POW ""
#define WM_KWIN_SETTINGS "systemseettings5"
#define WM_KWIN_TERMINAL "Konsole"
#define WM_KWIN_TASKMGR "systemmonitor"
#define WM_KWIN_SSHOT ""

#define XDG_CURRENT_DESKTOP "XDG_CURRENT_DESKTOP"
#define XDG_SESSION_TYPE "XDG_SESSION_TYPE"
#define XDG_DATA_HOME "XDG_DATA_HOME"
#define XDG_RUNTIME_DIR "XDG_RUNTIME_DIR"

boolean get_wm_caps(void);
boolean get_distro_dets(void);
boolean get_machine_dets(void);
int get_num_cpus(void);
double get_disk_load(const char *mp);
double check_disk_pressure(double current);

int64_t get_cpu_load(int cpun); ///< percent * 1 million
volatile float **const get_proc_loads(boolean reset);

int set_thread_cpuid(pthread_t pth);

volatile float *get_core_loadvar(int corenum);

double analyse_cpu_stats(void);

const char *get_shmdir(void);

char *get_systmp(const char *suff, boolean is_dir);
char *get_worktmp(const char *prefix);
char *get_worktmpfile(const char *prefix);
char *get_localsharedir(const char *subdir);
boolean notify_user(const char *detail);

char *get_install_cmd(const char *distro, const char *exe);

boolean check_snap(const char *prog);

char *use_staging_dir_for(int clipno);

#endif
