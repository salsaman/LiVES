// machinestate.h
// (c) G. Finch 2019 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _MACHINESTATE_H_
#define _MACHINESTATE_H_

//#define USE_INTRINSICS 1

#include <sys/time.h>
#include <time.h>

// hardware config details (capable->hw.*)

#define CPU_VENDOR_INTEL		"intel"
#define CPU_VENDOR_AMD			"amd"

#define CPU_TYPE_OTHER		-1
#define CPU_TYPE_UNKNOWN	0
#define CPU_TYPE_INTEL		1
#define CPU_TYPE_AMD		2

#define CPU_FEATURE_HAS_SSE		(1ull << 0)
#define CPU_FEATURE_HAS_SSE2		(1ull << 1)
#define CPU_FEATURE_HAS_FMA		(1ull << 2)
#define CPU_FEATURE_HAS_AVX		(1ull << 3)
#define CPU_FEATURE_HAS_AVX2		(1ull << 4)
#define CPU_FEATURE_HAS_AVX512		(1ull << 5)
#define CPU_FEATURE_HAS_F16C		(1ull << 6)

typedef struct {
  int byte_order;
  int ncpus;
  short cpu_maxlvl;
  short cpu_bits;
  char *cpu_name;
  char *cpu_vendor;
  int cpu_type;
  uint64_t cpu_features;
  int cacheline_size;
  size_t cache_size;
  size_t pagesize;
  //
  int mem_status;
  int64_t memtotal;
  int64_t memfree;
  int64_t memavail;
  int64_t memlocked;
  //
#define OOM_ADJ_RANGE 1000
  char *oom_adjust_file;
  int oom_adj_value;
  uint64_t rt_clock_res;
  uint64_t mono_clock_res;
} hw_caps_t;

//////////////

/// TODO - move elsewhere, does not really belong here
///
// weed plants with type >= 16384 are reserved for custom use, so let's take advantage of that
#define WEED_PLANT_LIVES 31337

#define LIVES_PLANT_MESSAGE 1
#define LIVES_PLANT_WIDGET 2
#define LIVES_PLANT_TUNABLE 3
#define LIVES_PLANT_PROC_THREAD 4
#define LIVES_PLANT_PREFERENCE 5

#define LIVES_PLANT_BAG_OF_HOLDING 256 // generic - cant think of a better name right now

#define LIVES_PLANT_HASH_STORE 513

#define LIVES_PLANT_INDEX 514

// used for debugging purposes
#define LIVES_PLANT_AUDIT 1024

#define LIVES_PROC_DIR "/proc/self/task"

weed_plant_t *lives_plant_new(int subtype);
weed_plant_t *lives_plant_new_with_index(int subtype, int64_t index);
weed_plant_t *lives_plant_new_with_refcount(int subtype);

void lives_get_randbytes(void *ptr, size_t size);

#define LIVES_LEAF_TRIALS "trials"
#define LIVES_LEAF_NTRIALS "ntrials"
#define LIVES_LEAF_TSTART "tstart"
#define LIVES_LEAF_TOT_TIME "tot_time"
#define LIVES_LEAF_TOT_COST "tot_cost"
#define LIVES_LEAF_SMALLER "smaller"
#define LIVES_LEAF_LARGER "larger"
#define LIVES_LEAF_CYCLES "cycles"

LiVESList *allthrds_list(void);
LiVESList *cull_unknown_threads(LiVESList *cull_list);

void autotune_u64_start(weed_plant_t *tuner,  uint64_t min, uint64_t max, int ntrials);
uint64_t autotune_u64_end(weed_plant_t **tuner, uint64_t val, double cost);

void init_random(void);
void lives_srandom(unsigned int seed);
uint64_t lives_random(void);

void show_nrcalls(void);

uint64_t fastrand(void) LIVES_HOT;
void fastrand_add(uint64_t entropy);
double fastrand_dbl(double range);
uint32_t fastrand_int(uint32_t range);

uint64_t gen_unique_id(void);

char *get_md5sum(const char *filename);

boolean get_memstatus(void);
boolean check_mem_status(void);

char *get_symlink_for(const char *link);

char *get_mountpoint_for(const char *dir);
char *get_fstype_for(const char *vol);

boolean file_is_ours(const char *fname);

ticks_t lives_get_relative_ticks(ticks_t origticks);
ticks_t lives_get_current_ticks(void);
ticks_t lives_get_session_ticks(void);
double lives_get_session_time(void);
char *lives_datetime(uint64_t secs, boolean use_local);
char *lives_datetime_rel(const char *datetime);
char *get_current_timestamp(void);

// cancelation point for threads with deferred cancel type
#define lives_cancel_point pthread_testcancel();

int check_dev_busy(char *devstr);

off_t get_dir_size(const char *dirname);

LiVESList *check_for_subdirs(const char *dirname, LiVESList *subdirs);
boolean is_empty_dir(const char *dirname);

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
  lsd_struct_def_t *lsd;
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

uint64_t lives_bin_hash(uint8_t *bin, size_t binlen) LIVES_PURE LIVES_HOT;
uint32_t lives_string_hash(const char *) LIVES_PURE LIVES_HOT;
uint32_t fast_hash(const char *, size_t strln) LIVES_PURE LIVES_HOT;
uint64_t fast_hash64(const char *);

void update_effort(double nthings, boolean is_bad);
void reset_effort(void);

void free_fdets_list(LiVESList **);
lives_proc_thread_t dir_to_file_details(LiVESList **, const char *dir,
                                        const char *orig_loc, uint64_t extra);
lives_proc_thread_t ordfile_to_file_details(LiVESList **listp, const char *ofname,
    const char *orig_loc, uint64_t extra);

///// cmdline

char *grep_in_cmd(const char *cmd, int mstart, int npieces, const char *mphrase, int ridx, int rlen, boolean partial);

/// x11
char *get_wid_for_name(const char *wname);
boolean hide_x11_window(const char *wid);
boolean unhide_x11_window(const char *wid);
boolean activate_x11_window(const char *wid);
boolean show_desktop_panel(void);
boolean hide_desktop_panel(void);
boolean get_x11_visible(const char *wname);

#ifdef GDK_WINDOWING_X11
// this doesn't really belong here, but for now, OK
typedef struct {
  int clipno;
  int screen_area;
  double fps;
  double scale;
  uint32_t delay_time, rec_time;
  int achans, asamps, arate, signed_endian;
  boolean duplex;
  ticks_t tottime;
  lives_proc_thread_t lpt;
} rec_args;

void rec_desk(void *args);
#endif

boolean lives_disable_screensaver(void);
boolean lives_reenable_screensaver(void);

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

#define WM_PROP_NEW_FOCUS "focus-new-windows"

boolean wm_property_set(const char *key, const char *val);
char *wm_property_get(const char *key, int *type_guess);

boolean get_wm_caps(void);
boolean get_distro_dets(void);

void get_monitors(boolean reset);

boolean get_machine_dets(int phase);
int get_num_cpus(void);
double get_disk_load(const char *mp);
double check_disk_pressure(double current);

int64_t get_cpu_load(int cpun); ///< percent * 1 million
volatile float **const get_proc_loads(boolean reset);

int set_thread_cpuid(pthread_t pth);

volatile float *get_core_loadvar(int corenum);

double analyse_cpu_stats(void);

char *get_systmp(const char *suff, boolean is_dir);
char *get_worktmp(const char *prefix);
char *get_worktmpfile(const char *prefix);
char *get_localsharedir(const char *subdir);
boolean notify_user(const char *detail);

void perf_manager(void);

#endif
