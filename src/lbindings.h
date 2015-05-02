// bindings.h
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_LBINDINGS_H
#define HAS_LIVES_LBINDINGS_H

#ifndef __cplusplus

#ifdef IS_LIBLIVES
void binding_cb(int msgnumber, const char *msgstring, ulong myid);

#endif

#endif

#define pad4(val) ((int)((val+4)/4)*4)

int padup(char **str, int arglen);
int add_int_arg(char **str, int arglen, int val);


boolean start_player(void);
boolean idle_stop_playback(ulong id);

boolean idle_quit(pthread_t *gtk_thread);

boolean idle_show_info(const char *text, boolean blocking, ulong id);
boolean idle_save_set(const char *name, boolean force_append, ulong id);
boolean idle_choose_file_with_preview(const char *dirname, const char *title, int preview_type, ulong id);
boolean idle_open_file(const char *fname, double stime, int frames, ulong id);
boolean idle_set_interactive(boolean setting, ulong id);
boolean idle_choose_set(ulong id);
boolean idle_reload_set(const char *setname, ulong id);
boolean idle_set_set_name(ulong id);
boolean idle_set_pref_bool(int prefidx, boolean val, ulong id);
boolean idle_set_pref_int(int prefidx, int val, ulong id);
boolean idle_set_pref_bitmapped(int prefidx, int bitfield, boolean val, ulong id);
boolean idle_switch_clip(int type, int cnum, ulong id);
boolean idle_unmap_effects(ulong id);
boolean idle_map_fx(int key, int mode, int idx, ulong id);
boolean idle_unmap_fx(int key, int mode, ulong id);
boolean idle_fx_setmode(int key, int mode, ulong id);
boolean idle_fx_enable(int key, boolean setting, ulong id);
boolean idle_set_fullscreen_sepwin(boolean setting, ulong id);
boolean idle_set_fullscreen(boolean setting, ulong id);
boolean idle_set_sepwin(boolean setting, ulong id);
boolean idle_set_if_mode(int mode, ulong id);
boolean idle_insert_block(int clipno, boolean ign_sel, boolean with_audio, ulong id);
boolean idle_remove_block(ulong block_id, ulong id);
boolean idle_move_block(ulong block_uid, int track, double time, ulong id);
boolean idle_mt_set_track(int tnum, ulong id);
boolean idle_set_current_time(double time, ulong id);
boolean idle_set_current_audio_time(double time, ulong id);
boolean idle_set_current_frame(int frame, boolean bg, ulong id);
boolean idle_wipe_layout(boolean force, ulong id);
boolean idle_choose_layout(ulong id);
boolean idle_save_layout(const char *lname, ulong id);
boolean idle_reload_layout(const char *lname, ulong id);
boolean idle_render_layout(boolean with_aud, boolean normalise_aud, ulong id);
boolean idle_select_all(int cnum, ulong id);
boolean idle_select_start(int cnum, int frame, ulong id);
boolean idle_select_end(int cnum, int frame, ulong id);
boolean idle_set_current_fps(double fps, ulong id);
boolean idle_set_loop_mode(int mode, ulong id);
boolean idle_set_ping_pong(boolean setting, ulong id);
boolean idle_resync_fps(ulong id);
boolean idle_cancel_proc(ulong id);
boolean idle_set_track_label(int tnum, const char *label, ulong id);
boolean idle_insert_vtrack(boolean in_front, ulong id);
boolean idle_set_gravity(int grav, ulong id);
boolean idle_set_insert_mode(int mode, ulong id);

ulong *get_unique_ids(void);
int cnum_for_uid(ulong uid);

int get_first_fx_matched(const char *package, const char *fxname, const char *author, int version);
int get_num_mapped_modes_for_key(int i);
int get_current_mode_for_key(int key);
boolean get_rte_key_is_enabled(int key);

#endif //HAS_LIVES_LBINDINGS_H
