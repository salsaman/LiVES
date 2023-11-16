// utils.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _UTILS_H_
#define _UTILS_H_

#define clear_mainw_msg() lives_memset(mainw->msg, 0, MAINW_MSG_SIZE)

/////////////// unit formatting

typedef struct {
  uint64_t multiplier;
  const char **big_units, * *small_units;
} lives_unitdef_t;

#define UNIT_FMT(utype, val, qshort, qlong, fix)			\
  UNIT_VAL(utype,(val),UNIT_IDX(utype,(val)),fix,qshort,qlong)
#define UNIT_FMT_LONG(utype, val, qshort, qlong, fix)			\
  UNIT_VAL_LONG(utype,(val),UNIT_IDX(utype,(val)),fix,qshort,qlong)
#define UNIT_IDX(utype, val) unit_idx(lives_unit_##utype,(double)(val))
#define UNIT_VAL(utype, val, idx, fix, qs, ql) unit_val(lives_unit_##utype,(double)(val), idx, fix, qs, ql)
#define UNIT_VAL_LONG(utype, val, idx, fix, qs, ql) \
  unit_val_long(lives_unit_##utype,(double)(val), idx, fix, qs, ql)

//////////////////

typedef enum {lives_unit_SI, lives_unit_IEC, N_UNIT_TYPES} lives_unit_type_t;

#define SI_UNITS_BIG "K", "M", "G", "T", "P", "E", "Z", "Y"
#define SI_UNITS_SMALL "m", "u", "n", "p", "f", "a", "z", "y"
#define SI_UNITS_X 1000

#define IEC_UNITS_BIG "Ki", "Mi", "Gi", "Ti"
#define IEC_UNITS_SMALL NULL
#define IEC_UNITS_X 1024

extern const lives_unitdef_t unitdefs[N_UNIT_TYPES];

////////////////////////

char *lives_format_storage_space_string(uint64_t space);
char *lives_format_timing_string(double secs);
char *lives_format_memory_size_string(uint64_t size);
char *lives_format_storage_space_string_long(uint64_t space);

char *commafmt(double val, int fix);

////////////// OS utils ///

void lives_abort(const char *reason);
int lives_system(const char *com, boolean allow_error);
ssize_t _lives_popen(const char *com, boolean allow_error, void  *buff, size_t buflen);
#define lives_popen(com, allow_error, buff) _lives_popen(com, allow_error, (void *)buff, sizeof(buff))
ssize_t lives_popen_txtbuf(const char *com, boolean allow_error, LiVESTextBuffer *tbuff);
lives_pid_t lives_fork(const char *com);
boolean lives_fork_cb(lives_obj_t *dummy, void *com);

int lives_chdir(const char *path, boolean no_error_dlg);
pid_t lives_getpid(void);
int lives_getgid(void);
int lives_getuid(void);
void lives_kill_subprocesses(const char *dirname, boolean kill_parent);
void lives_suspend_resume_process(const char *dirname, boolean suspend);
int lives_kill(lives_pid_t pid, int sig);
int lives_killpg(lives_pid_t pgrp, int sig);
boolean lives_setenv(const char *name, const char *value);
boolean lives_unsetenv(const char *name);
lives_result_t lives_rmdir(const char *dir, boolean force);
int lives_rmdir_with_parents(const char *dir);
int lives_rm(const char *file);
int lives_rmglob(const char *files);
int lives_cp(const char *from, const char *to);
int lives_cp_noclobber(const char *from, const char *to);
int lives_cp_recursive(const char *from, const char *to, boolean incl_dir);
int lives_cp_keep_perms(const char *from, const char *to);
int lives_mv(const char *from, const char *to);
int lives_touch(const char *tfile);
int lives_chmod(const char *target, const char *mode);
int lives_cat(const char *from, const char *to, boolean append);
int lives_echo(const char *text, const char *to, boolean append);
int lives_ln(const char *from, const char *to);

void restart_me(LiVESList *extra_argv, const char *xreason);

///// versioning ///

uint64_t get_version_hash(const char *exe, const char *sep, int piece);
uint64_t make_version_hash(const char *ver);
char *unhash_version(uint64_t version);
int verhash(const char *version);

/////////////////// clip and frame utils ////

double calc_time_from_frame(int clip, frames_t frame);
frames_t calc_frame_from_time(int filenum, double time);   ///< nearest frame [1, frames]
frames_t calc_frame_from_time2(int filenum, double time);  ///< nearest frame [1, frames+1]
frames_t calc_frame_from_time3(int filenum, double time);  ///< nearest frame rounded down, [1, frames+1]
frames_t calc_frame_from_time4(int filenum, double time);  ///<  nearest frame, no maximum

void calc_maxspect(int rwidth, int rheight, int *cwidth, int *cheight);
void calc_minspect(int rwidth, int rheight, int *cwidth, int *cheight);
double find_nearest_ar(int width, int height, int *wm, int *hm);

void minimise_aspect_delta(double allowed_aspect, int hblock, int vblock, int hsize, int vsize,
                           int *width, int *height);

int get_next_free_file(void);

void get_total_time(lives_clip_t
                    *file); ///< calculate laudio, raudio and video time (may be deprecated and replaced with macros)
void get_play_times(void); ///< recalculate video / audio lengths and draw the timer bars
void update_play_times(void); ///< like get_play_times, but will force redraw of audio waveforms

boolean check_frame_count(int idx, boolean last_chkd);
frames_t get_frame_count(int idx, int xsize);
frames_t count_resampled_frames(frames_t in_frames, double orig_fps, double resampled_fps);

uint32_t get_signed_endian(boolean is_signed, boolean little_endian); ///< produce bitmapped value

void find_when_to_stop(void);

////////////// audio players /////

void switch_aud_to_none(boolean set_pref);
boolean switch_aud_to_jack(boolean set_pref);
boolean switch_aud_to_pulse(boolean set_pref);

////////// window grab //////

boolean prepare_to_play_foreign(void);
boolean after_foreign_play(void);

/////////////// layout map errors /////////

boolean add_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data,
                       int clipno, int frameno, double atime, boolean affects_current);
void buffer_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data, int clipno,
                       int frameno, double atime, boolean affects_current);
void unbuffer_lmap_errors(boolean add);

void clear_lmap_errors(void);

///////////////////////

boolean do_std_checks(const char *type_name, const char *type, size_t maxlen, const char *nreject);
char *repl_workdir(const char *entry, boolean fwd);

//////////// ui utils ///

void reset_clipmenu(void);
void add_to_recent(const char *filename, double start, int frames, const char *file_open_params);
void zero_spinbuttons(void);
void set_start_end_spins(int clipno);
void set_sel_label(LiVESWidget *label);

////////////// misc ///

boolean create_event_space(int length_in_eventsb);

LiVESPixbuf *lives_pixbuf_new_blank(int width, int height, int palette);

LiVESInterpType get_interp_value(short quality, boolean low_for_mt);

boolean get_screen_usable_size(int *w, int *h);

//////////////

void activate_url_inner(const char *link);
void activate_url(LiVESAboutDialog *about, const char *link, livespointer data);
void show_manual_section(const char *lang, const char *section);

/////

void maybe_add_mt_idlefunc(void);

int check_for_bad_ffmpeg(void);

#endif
