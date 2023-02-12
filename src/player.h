// player.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <sys/time.h>
struct timeval tv;

#define OBJECT_TYPE_PLAYER	IMkType("obj.PLAY")
#define PLAYER_SUBTYPE_VIDEO	IMkType("PLAY.vid")
#define PLAYER_SUBTYPE_AUDIO	IMkType("PLAY.aud")

lives_object_instance_t *lives_player_inst_create(uint64_t subtype);

#define LIVES_IS_IDLE (lives_get_status() == LIVES_STATUS_IDLE)

#define LIVES_IS_PLAYING (mainw && mainw->playing_file > -1)

// normal playback, not previwing, processing, rendering or recording
#define LIVES_NORMAL_PLAYBACK (lives_get_status() == LIVES_STATUS_PLAYING)
#define LIVES_CE_PLAYBACK (LIVES_NORMAL_PLAYBACK && !mainw->multitrack)
#define LIVES_MT_PLAYBACK (LIVES_NORMAL_PLAYBACK && mainw->multitrack)
#define LIVES_IS_RECORDING (!RECORD_PAUSED && (lives_get_status() & LIVES_STATUS_RECORDING))
#define LIVES_IS_RENDERING (mainw && ((!mainw->multitrack && mainw->is_rendering) \
				      || (mainw->multitrack && mainw->multitrack->is_rendering)) \
			    && !mainw->preview_rendering)

#define LIVES_STATUS_IDLE			0
#define LIVES_STATUS_PLAYING			(1 << 0)
#define LIVES_STATUS_PROCESSING			(1 << 1)
#define LIVES_STATUS_PREVIEW			(1 << 2)
#define LIVES_STATUS_RENDERING			(1 << 3)
#define LIVES_STATUS_RECORDING			(1 << 4)

#define ACTIVE_STATUS ((1 << 14) - 1)

// special states
#define LIVES_STATUS_NOTREADY			(1 << 14)
#define LIVES_STATUS_EXITING			(1 << 15)
//
#define LIVES_STATUS_ERROR 			(1 << 16)
#define LIVES_STATUS_FATAL 			(1 << 17)

#define RECORD_PAUSED (mainw && mainw->record && mainw->record_paused)

// resizing ??

// multitrack mode
#define LIVES_MODE_MT (mainw && mainw->multitrack)

// clip edit mode
#define LIVES_MODE_CE (mainw && !mainw->multitrack)

int lives_set_status(int status);
int lives_unset_status(int status);
boolean lives_has_status(int status);
int lives_get_status(void);

void get_player_size(int *opwidth, int *opheight);

void player_desensitize(void);
void player_sensitize(void);

void init_track_sources(void);
void free_track_sources(void);

void track_source_free(int i, int oclip);

boolean record_setup(ticks_t actual_ticks);

weed_layer_t *load_frame_image(frames_t frame);

weed_layer_t *get_old_frame_layer(void);
void reset_old_frame_layer(void);

frames_t clamp_frame(int clipno, frames_t nframe);

void reset_playback_clock(void);
ticks_t lives_get_current_playback_ticks(ticks_t origsecs, ticks_t origusecs, lives_time_source_t *time_source);
frames_t calc_new_playback_position(int fileno, ticks_t otc, ticks_t *ntc);
void calc_aframeno(int fileno);

void ready_player_one(weed_timecode_t estart);

int process_one(boolean visible);

boolean clip_can_reverse(int clipno);

const char *get_cache_stats(void);

#endif
