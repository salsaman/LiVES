// saveplay.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _SAVEPLAY_H_
#define _SAVEPLAY_H_

boolean add_file_info(const char *check_handle, boolean aud_only);
boolean save_file_comments(int fileno);
void set_default_comment(lives_clip_t *sfile, const char *extract);
boolean reload_clip(int fileno, frames_t maxframe);
void wait_for_bg_audio_sync(int fileno);
ulong deduce_file(const char *filename, double start_time, int end);
ulong open_file(const char *filename);
ulong open_file_sel(const char *file_name, double start_time, int frames);
void open_fw_device(void);
int get_next_free_file(void);
boolean get_new_handle(int index, const char *name);
boolean get_temp_handle(int index);
int close_temp_handle(int new_clip);
boolean get_handle_from_info_file(int index);
void save_file(int clip, frames_t start, frames_t end, const char *filename);
void play_file(void);
void start_playback_async(int type);
boolean start_playback(int type);
void play_start_timer(int type);
void save_frame(LiVESMenuItem *, livespointer user_data);
boolean save_frame_inner(int clip, frames_t frame, const char *file_name, int width, int height, boolean from_osc);
void wait_for_stop(const char *stop_command);
void add_to_recovery_file(const char *handle);
boolean rewrite_recovery_file(void);
boolean check_for_recovery_files(boolean auto_recover, boolean no_recover);
boolean recover_files(char *recovery_file, boolean auto_recover);
const char *get_deinterlace_string(void);
void reload_subs(int fileno);
void pad_with_silence(int clipno, boolean at_start, boolean is_auto);

// saveplay.c backup
void backup_file(int clip, int start, int end, const char *filename);

// saveplay.c restore
ulong restore_file(const char *filename);

// saveplay.c scrap file
boolean open_scrap_file(void);
boolean open_ascrap_file(int clipno);
int save_to_scrap_file(weed_layer_t *);
boolean load_from_scrap_file(weed_layer_t *, frames_t frame);
void close_ascrap_file(boolean remove);
void close_scrap_file(boolean remove);
void add_to_ascrap_mb(uint64_t bytes);
double get_ascrap_mb(void);

#endif
