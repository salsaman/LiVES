// player.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <sys/time.h>
struct timeval tv;

#define LIVES_STATUS_IDLE			0
#define LIVES_STATUS_NOTREADY			(1 << 0)
#define LIVES_STATUS_PLAYING			(1 << 1)
#define LIVES_STATUS_PROCESSING			(1 << 2)
#define LIVES_STATUS_PREVIEW			(1 << 3)
#define LIVES_STATUS_RENDERING			(1 << 4)
#define LIVES_STATUS_EXITING			(1 << 5)
//
#define LIVES_STATUS_ERROR 			(1 << 16)
#define LIVES_STATUS_FATAL 			(1 << 17)

// resizing ??
// recording, record paused

int lives_set_status(int status);
int lives_unset_status(int status);
boolean lives_has_status(int status);
int lives_get_status(void);

void get_player_size(int *opwidth, int *opheight);

void player_desensitize(void);
void player_sensitize(void);

void init_track_decoders(void);
void free_track_decoders(void);

boolean load_frame_image(frames_t frame);

void reset_playback_clock(void);
ticks_t lives_get_current_playback_ticks(ticks_t origsecs, ticks_t origusecs, lives_time_source_t *time_source);
frames_t calc_new_playback_position(int fileno, ticks_t otc, ticks_t *ntc);
void calc_aframeno(int fileno);

void ready_player_one(weed_timecode_t estart);

int process_one(boolean visible);

boolean clip_can_reverse(int clipno);

#endif
