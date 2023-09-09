// player-controller.h
// LiVES
// (c) G. Finch 2003 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _PLAYER_CONTROL_H
#define _PLAYER_CONTROL_H 1

void play_file(void);

lives_proc_thread_t start_playback_async(int type);

boolean start_playback(int type);

void play_start_timer(int type);

#endif
