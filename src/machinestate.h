// machinestate.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

/// TODO - split into: memory, files, sysops, threads

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

typedef weed_plantptr_t lives_proc_thread_t;

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

#define lives_nanosleep(nanosec) {struct timespec ts; ts.tv_sec = (uint64_t)nanosec / ONE_BILLION; \
    ts.tv_nsec = (uint64_t)nanosec - ts.tv_sec * ONE_BILLION; while (nanosleep(&ts, &ts) == -1 && \
								     errno != ETIMEDOUT);}
#define lives_nanosleep_until_nonzero(condition) {while (!(condition)) lives_nanosleep(1000);}

int check_dev_busy(char *devstr);

off_t get_file_size(int fd);
off_t sget_file_size(const char *name);

void reget_afilesize(int fileno);
off_t reget_afilesize_inner(int fileno);

off_t get_dir_size(const char *dirname);

boolean compress_files_in_dir(const char *dir, int method, void *data);
LiVESResponseType send_to_trash(const char *item);

/// extras we can check for, may consume more time
#define EXTRA_DETAILS_CHECK_MISSING	       	(1ul << 0)
#define EXTRA_DETAILS_DIRSIZE			(1ul << 1)
#define EXTRA_DETAILS_EMPTY_DIRS	       	(1ul << 2)
#define EXTRA_DETAILS_SYMLINK		       	(1ul << 3)
#define EXTRA_DETAILS_ACCESSIBLE	       	(1ul << 4)
#define EXTRA_DETAILS_WRITEABLE			(1ul << 5)
#define EXTRA_DETAILS_EXECUTABLE       		(1ul << 6)
#define EXTRA_DETAILS_CLIPHDR			(1ul << 7)

/// derived values
#define EXTRA_DETAILS_MD5SUM			(1ul << 33)

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
  char *md5sum; /// only filled if EXTRA_DETAILS_MD5 is set, otherwis NULL
  char *extra_details;  /// intialized to NULL, set to at least ""
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

//// threadpool API

typedef void *(*lives_funcptr_t)(void *);

typedef struct _lives_thread_data_t lives_thread_data_t;

typedef struct {
  lives_proc_thread_t var_tinfo;
  lives_thread_data_t *var_mydata;
  boolean var_com_failed;
  int var_write_failed, var_read_failed;
  boolean var_chdir_failed;
  char *var_read_failed_file, *var_write_failed_file, *var_bad_aud_file;
  int var_rowstride_alignment;   // used to align the rowstride bytesize in create_empty_pixel_data
  int var_rowstride_alignment_hint;
  int var_last_sws_block;
  boolean var_no_gui;
} lives_threadvars_t;

struct _lives_thread_data_t {
  LiVESWidgetContext *ctx;
  int64_t idx;
  lives_threadvars_t vars;
};

typedef struct {
  lives_funcptr_t func;
  void *arg;
  uint64_t flags;
  volatile uint64_t busy;
  volatile uint64_t done;
  void *ret;
  volatile boolean sync_ready;
} thrd_work_t;

#define WEED_LEAF_NOTIFY "notify"
#define WEED_LEAF_DONE "done"
#define WEED_LEAF_THREADFUNC "tfunction"
#define WEED_LEAF_THREAD_PROCESSING "t_processing"
#define WEED_LEAF_THREAD_CANCELLABLE "t_can_cancel"
#define WEED_LEAF_THREAD_CANCELLED "t_cancelled"
#define WEED_LEAF_RETURN_VALUE "return_value"
#define WEED_LEAF_DONTCARE "dontcare"  ///< tell proc_thread with return value that we n o longer need return val.
#define WEED_LEAF_DONTCARE_MUTEX "dontcare_mutex" ///< ensures we can set dontcare without it finishing while doing so

#define WEED_LEAF_SIGNALLED "signalled"
#define WEED_LEAF_SIGNAL_DATA "signal_data"

#define WEED_LEAF_THREAD_PARAM "thrd_param"
#define _WEED_LEAF_THREAD_PARAM(n) WEED_LEAF_THREAD_PARAM  n
#define WEED_LEAF_THREAD_PARAM0 _WEED_LEAF_THREAD_PARAM("0")
#define WEED_LEAF_THREAD_PARAM1 _WEED_LEAF_THREAD_PARAM("1")
#define WEED_LEAF_THREAD_PARAM2 _WEED_LEAF_THREAD_PARAM("2")

#define LIVES_THRDFLAG_AUTODELETE	(1 << 0)
#define LIVES_THRDFLAG_TUNING		(1 << 1)
#define LIVES_THRDFLAG_WAIT_SYNC	(1 << 2)

typedef LiVESList lives_thread_t;
typedef uint64_t lives_thread_attr_t;

#define LIVES_THRDATTR_NONE		0
#define LIVES_THRDATTR_AUTODELETE	(1 << 0)
#define LIVES_THRDATTR_PRIORITY		(1 << 1)
#define LIVES_THRDATTR_WAIT_SYNC	(1 << 2)
#define LIVES_THRDATTR_FG_THREAD	(1 << 3)
#define LIVES_THRDATTR_NO_GUI		(1 << 4)

void lives_threadpool_init(void);
void lives_threadpool_finish(void);
int lives_thread_create(lives_thread_t *thread, lives_thread_attr_t attr, lives_funcptr_t func, void *arg);
uint64_t lives_thread_join(lives_thread_t work, void **retval);

// lives_proc_thread_t //////////////////////////////////////////////////////////////////////////////////////////////////////////

#define _RV_ WEED_LEAF_RETURN_VALUE

typedef uint64_t funcsig_t;

typedef int(*funcptr_int_t)();
typedef double(*funcptr_dbl_t)();
typedef int(*funcptr_bool_t)();
typedef char *(*funcptr_string_t)();
typedef int64_t(*funcptr_int64_t)();
typedef weed_funcptr_t(*funcptr_funcptr_t)();
typedef void *(*funcptr_voidptr_t)();
typedef weed_plant_t(*funcptr_plantptr_t)();

#define GETARG(type, n) WEED_LEAF_GET(info, _WEED_LEAF_THREAD_PARAM(n), type)

#define ARGS1(t1) GETARG(t1, "0")
#define ARGS2(t1, t2) ARGS1(t1), GETARG(t2, "1")
#define ARGS3(t1, t2, t3) ARGS2(t1, t2), GETARG(t3, "2")
#define ARGS4(t1, t2, t3, t4) ARGS3(t1, t2, t3), GETARG(t4, "3")
#define ARGS5(t1, t2, t3, t4, t5) ARGS4(t1, t2, t3, t4), GETARG(t5, "4")
#define ARGS6(t1, t2, t3, t4, t5, t6) ARGS5(t1, t2, t3, t4, t5), GETARG(t6, "5")
#define ARGS7(t1, t2, t3, t4, t5, t6, t7) ARGS6(t1, t2, t3, t4, t5, t6), GETARG(t7, "6")
#define ARGS8(t1, t2, t3, t4, t5, t6, t7, t8) ARGS7(t1, t2, t3, t4, t5, t6, t7), GETARG(t8, "7")
#define ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9) ARGS8(t1, t2, t3, t4, t5, t6, t7. t8), GETARG(t9, "8")
#define ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9), GETARG(t10, "9")

#define CALL_VOID_10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) (*thefunc->func)(ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10))
#define CALL_VOID_9(t1, t2, t3, t4, t5, t6, t7, t8, t9) (*thefunc->func)(ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9))
#define CALL_VOID_8(t1, t2, t3, t4, t5, t6, t7, t8) (*thefunc->func)(ARGS8(t1, t2, t3, t4, t5, t6, t7, t8))
#define CALL_VOID_7(t1, t2, t3, t4, t5, t6, t7) (*thefunc->func)(ARGS7(t1, t2, t3, t4, t5, t6, t7))
#define CALL_VOID_6(t1, t2, t3, t4, t5, t6) (*thefunc->func)(ARGS6(t1, t2, t3, t4, t5, t6))
#define CALL_VOID_5(t1, t2, t3, t4, t5) (*thefunc->func)(ARGS5(t1, t2, t3, t4, t5))
#define CALL_VOID_4(t1, t2, t3, t4) (*thefunc->func)(ARGS4(t1, t2, t3, t4))
#define CALL_VOID_3(t1, t2, t3) (*thefunc->func)(ARGS3(t1, t2, t3))
#define CALL_VOID_2(t1, t2) (*thefunc->func)(ARGS2(t1, t2))
#define CALL_VOID_1(t1) (*thefunc->func)(ARGS1(t1))
#define CALL_VOID_0() (*thefunc->func)()

#define CALL_10(ret, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) weed_set_##ret##_value(info, _RV_, \
										     (*thefunc->func##ret)(ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t19)))
#define CALL_9(ret, t1, t2, t3, t4, t5, t6, t7, t8, t9) weed_set_##ret##_value(info, _RV_, \
									       (*thefunc->func##ret)(ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9)))
#define CALL_8(ret, t1, t2, t3, t4, t5, t6, t7, t8) weed_set_##ret##_value(info, _RV_, \
									   (*thefunc->func##ret)(ARGS8(t1, t2, t3, t4, t5, t6, t7, t7)))
#define CALL_7(ret, t1, t2, t3, t4, t5, t6, t7) weed_set_##ret##_value(info, _RV_, \
								       (*thefunc->func##ret)(ARGS7(t1, t2, t3, t4, t5, t6, t7)))
#define CALL_6(ret, t1, t2, t3, t4, t5, t6) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS6(t1, t2, t3, t4, t5, t6)))
#define CALL_5(ret, t1, t2, t3, t4, t5) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS5(t1, t2, t3, t4, t5)))
#define CALL_4(ret, t1, t2, t3, t4) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS4(t1, t2, t3, t4)))
#define CALL_3(ret, t1, t2, t3) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS3(t1, t2, t3)))
#define CALL_2(ret, t1, t2) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS2(t1, t2)))
#define CALL_1(ret, t1) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS1(t1)))
#define CALL_0(ret) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)())

typedef union {
  weed_funcptr_t func;
  funcptr_int_t funcint;
  funcptr_dbl_t funcdouble;
  funcptr_bool_t funcboolean;
  funcptr_int64_t funcint64;
  funcptr_string_t funcstring;
  funcptr_funcptr_t funcfuncptr;
  funcptr_voidptr_t funcvoidptr;
  funcptr_plantptr_t funcplantptr;
} allfunc_t;

lives_proc_thread_t lives_proc_thread_create(lives_thread_attr_t, lives_funcptr_t, int return_type, const char *args_fmt,
    ...);

void lives_proc_thread_free(lives_proc_thread_t lpt);

/// returns FALSE while the thread is running, TRUE once it has finished
boolean lives_proc_thread_check(lives_proc_thread_t);
int lives_proc_thread_signalled(lives_proc_thread_t tinfo);
int64_t lives_proc_thread_signalled_idx(lives_proc_thread_t tinfo);

lives_thread_data_t *get_thread_data(void);
lives_threadvars_t *get_threadvars(void);
lives_thread_data_t *lives_thread_data_create(uint64_t idx);

#define THREADVAR(var) (get_threadvars()->var_##var)

/// only threads with no return value can possibly be cancellable. For threads with a value, use
/// lives_proc_thread_dontcare()
void lives_proc_thread_set_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_get_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_cancel(lives_proc_thread_t);
boolean lives_proc_thread_cancelled(lives_proc_thread_t);

/// tell a threead with return value that we no longer need the value so it can free itself
boolean lives_proc_thread_dontcare(lives_proc_thread_t);

void lives_proc_thread_sync_ready(lives_proc_thread_t);

// WARNING !! version without a return value will free tinfo !
void lives_proc_thread_join(lives_proc_thread_t);

// with return value should free proc_thread
int lives_proc_thread_join_int(lives_proc_thread_t);
double lives_proc_thread_join_double(lives_proc_thread_t);
int lives_proc_thread_join_boolean(lives_proc_thread_t);
char *lives_proc_thread_join_string(lives_proc_thread_t);
weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t);
void *lives_proc_thread_join_voidptr(lives_proc_thread_t);
weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t) ;
int64_t lives_proc_thread_join_int64(lives_proc_thread_t);

void resubmit_proc_thread(lives_proc_thread_t, lives_thread_attr_t);

void *fg_run_func(lives_proc_thread_t lpt, void *retval);
void *main_thread_execute(lives_funcptr_t func, int return_type, void *retval, const char *args_fmt, ...);

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

boolean get_wm_caps(void);
boolean get_distro_dets(void);
boolean get_machine_dets(void);
int get_num_cpus(void);
double get_disk_load(const char *mp);
int64_t get_cpu_load(int cpun); ///< percent * 1 million

char *get_systmp(const char *suff, boolean is_dir);
char *get_worktmp(const char *prefix);
char *get_worktmpfile(const char *prefix);

boolean check_snap(const char *prog);

#endif
