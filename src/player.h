// player.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include <sys/time.h>
struct timeval tv;

void reset_playback_clock(void);
ticks_t lives_get_current_playback_ticks(ticks_t origsecs, ticks_t origusecs, lives_time_source_t *time_source);
frames_t calc_new_playback_position(int fileno, ticks_t otc, ticks_t *ntc);
void calc_aframeno(int fileno);

boolean clip_can_reverse(int clipno);
