// clip_load_save.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _CLIP_LOAD_SAVE_H_
#define _CLIP_LOAD_SAVE_H_

// metadata
void save_clip_audio_values(int clipno);

boolean add_file_info(const char *check_handle, boolean aud_only);
boolean save_file_comments(int fileno);
void set_default_comment(lives_clip_t *sfile, const char *extract);

void wait_for_bg_audio_sync(int fileno);

// crash recovery
boolean check_for_recovery_files(boolean auto_recover, boolean no_recover);
boolean recover_files(char *recovery_file, boolean auto_recover);

void add_to_recovery_file(const char *handle);
boolean rewrite_recovery_file(void);

// clip loading
ulong deduce_file(const char *filename, double start_time, int end);
ulong open_file(const char *filename);
ulong open_file_sel(const char *file_name, double start_time, int frames);
void open_fw_device(void); // TOTDO - move
boolean reload_clip(int fileno, frames_t maxframe);

// low level API
boolean get_new_handle(int index, const char *name);
boolean get_temp_handle(int index);
int close_temp_handle(int new_clip);
boolean get_handle_from_info_file(int index);

// encoding
void save_file(int clip, frames_t start, frames_t end, const char *filename);

void save_frame(LiVESMenuItem *, livespointer user_data);
boolean save_frame_inner(int clip, frames_t frame, const char *file_name, int width, int height, boolean from_osc);

const char *get_deinterlace_string(void);

void reload_subs(int fileno);

void pad_with_silence(int clipno, boolean at_start, boolean is_auto);

void backup_file(int clip, int start, int end, const char *filename);
ulong restore_file(const char *filename);

// scrapfiles
boolean open_scrap_file(void);
boolean open_ascrap_file(int clipno);
void close_ascrap_file(boolean remove);
void close_scrap_file(boolean remove);
void add_to_ascrap_mb(uint64_t bytes);
double get_ascrap_mb(void);

#endif
