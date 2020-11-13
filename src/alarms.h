// alarms.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _ALARMS_H_
#define _ALARMS_H_

typedef int lives_alarm_t;

/// lives_alarms
#define LIVES_NO_ALARM 0
#define LIVES_MAX_ALARMS 1024
#define LIVES_MAX_USER_ALARMS 512

#define LIVES_URGENCY_ALARM LIVES_MAX_ALARMS // this is fine since we will subtract 1
#define URGENCY_MSG_TIMEOUT 10. // seconds

typedef struct {
  ticks_t tleft;
  volatile ticks_t lastcheck;
} lives_timeout_t;

lives_alarm_t lives_alarm_set(ticks_t ticks);
ticks_t lives_alarm_check(lives_alarm_t alarm_handle);
boolean lives_alarm_clear(lives_alarm_t alarm_handle);
lives_alarm_t lives_alarm_reset(lives_alarm_t alarm_handle, ticks_t ticks);

#endif
