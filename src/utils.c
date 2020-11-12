// utils.c
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <fcntl.h>
#include <dirent.h>
#include <sys/statvfs.h>
#ifdef HAVE_LIBEXPLAIN
#include <libexplain/system.h>
#endif
#include "main.h"
#include "interface.h"
#include "audio.h"
#include "resample.h"
#include "callbacks.h"
#include "cvirtual.h"

#define ASPECT_ALLOWANCE 0.005

typedef struct {
  uint32_t hash;
  char *key;
  char *data;
} lives_speed_cache_t;

static boolean  omute,  osepwin,  ofs,  ofaded,  odouble;

static int get_hex_digit(const char c) GNU_CONST;


LIVES_GLOBAL_INLINE boolean lives_setenv(const char *name, const char *value) {
  // ret TRUE on success
#if IS_IRIX
  char *env = lives_strdup_printf("%s=%s", name, val);
  boolean ret = !putenv(env);
  lives_free(env);
  return ret;
#else
  return !setenv(name, value, 1);
#endif
}

LIVES_GLOBAL_INLINE boolean lives_unsetenv(const char *name) {
  // ret TRUE on success
#if IS_IRIX
  char *env = lives_strdup_printf("%s=", name);
  boolean ret = !putenv(env);
  lives_free(env);
  return ret;
#else
  return !unsetenv(name);
#endif
}


int lives_system(const char *com, boolean allow_error) {
  LiVESResponseType response;
  int retval;
  boolean cnorm = FALSE;

  //g_print("doing: %s\n",com);

  if (mainw && mainw->is_ready && !mainw->is_exiting &&
      ((!mainw->multitrack && mainw->cursor_style == LIVES_CURSOR_NORMAL) ||
       (mainw->multitrack && mainw->multitrack->cursor_style == LIVES_CURSOR_NORMAL))) {
    cnorm = TRUE;
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
    /*   lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET); */
  }

  do {
    THREADVAR(com_failed) = FALSE;
    response = LIVES_RESPONSE_NONE;
    retval = system(com);
    if (retval) {
      char *msg = NULL;
      THREADVAR(com_failed) = TRUE;
      if (!allow_error) {
        msg = lives_strdup_printf("lives_system failed with code %d: %s\n%s", retval, com,
#ifdef HAVE_LIBEXPLAIN
                                  explain_system(com)
#else
                                  ""
#endif
                                 );
        LIVES_ERROR(msg);
        response = do_system_failed_error(com, retval, NULL, TRUE, FALSE);
      }
#ifndef LIVES_NO_DEBUG
      else {
        msg = lives_strdup_printf("lives_system failed with code %d: %s (not an error)", retval, com);
        LIVES_DEBUG(msg);
      }
#endif
      if (msg) lives_free(msg);
    }
  } while (response == LIVES_RESPONSE_RETRY);

  if (cnorm) lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  return retval;
}


ssize_t lives_popen(const char *com, boolean allow_error, char *buff, ssize_t buflen) {
  // runs com, fills buff with a NUL terminated string (total length <= buflen)
  // returns number of bytes read. If an error occurs during popen or fread
  // then THREADVAR(com_failed) is set, and if allow_error is FALSE then an an error dialog is displayed to the user

  // on error we return err as a -ve number

  // id buflen is 0, then buff os cast from a textbuff, and the output will be appended to it

  FILE *fp;
  char *xbuff;
  LiVESResponseType response;
  ssize_t totlen = 0, xtotlen = 0;
  size_t slen;
  LiVESTextBuffer *tbuff = NULL;
  LiVESTextIter end_iter;
  boolean cnorm = FALSE;
  int err = 0;

  if (buflen <= 0) {
    tbuff = (LiVESTextBuffer *)buff;
    buflen = get_read_buff_size(BUFF_SIZE_READ_LARGE);
    xbuff = (char *)lives_calloc(1, buflen);
  } else {
    xbuff = buff;
    lives_memset(xbuff, 0, 1);
  }
  //g_print("doing: %s\n",com);

  if (mainw && mainw->is_ready && !mainw->is_exiting &&
      ((!mainw->multitrack && mainw->cursor_style == LIVES_CURSOR_NORMAL) ||
       (mainw->multitrack && mainw->multitrack->cursor_style == LIVES_CURSOR_NORMAL))) {
    cnorm = TRUE;
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
    //lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  }

  do {
    char *strg = NULL;
    response = LIVES_RESPONSE_NONE;
    THREADVAR(com_failed) = FALSE;
    fflush(NULL);
    fp = popen(com, "r");
    if (!fp) {
      err = errno;
    } else {
      while (1) {
        strg = fgets(xbuff + totlen, tbuff ? buflen : buflen - totlen, fp);
        err = ferror(fp);
        if (err != 0 || !strg || !(*strg)) break;
        slen = lives_strlen(xbuff);
        if (tbuff) {
          lives_text_buffer_get_end_iter(LIVES_TEXT_BUFFER(tbuff), &end_iter);
          lives_text_buffer_insert(LIVES_TEXT_BUFFER(tbuff), &end_iter, xbuff, slen);
          xtotlen += slen;
        } else {
          //lives_snprintf(buff + totlen, buflen - totlen, "%s", xbuff);
          totlen = slen;
          if (slen >= buflen - 1) break;
        }
      }
      pclose(fp);
    }

    if (tbuff) {
      lives_free(xbuff);
      totlen = xtotlen;
    }

    if (err != 0) {
      char *msg = NULL;
      THREADVAR(com_failed) = TRUE;
      if (!allow_error) {
        msg = lives_strdup_printf("lives_popen failed p after %ld bytes with code %d: %s",
                                  !strg ? 0 : lives_strlen(strg), err, com);
        LIVES_ERROR(msg);
        response = do_system_failed_error(com, err, NULL, TRUE, FALSE);
      }
#ifndef LIVES_NO_DEBUG
      else {
        msg = lives_strdup_printf("lives_popen failed with code %d: %s (not an error)", err, com);
        LIVES_DEBUG(msg);
      }
#endif
      if (msg) lives_free(msg);
    }
  } while (response == LIVES_RESPONSE_RETRY);

  if (cnorm) lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  if (err != 0) return -ABS(err);
  return totlen;
}


lives_pgid_t lives_fork(const char *com) {
  // returns a number which is the pgid to use for lives_killpg

  // mingw - return PROCESS_INFORMATION * to use in GenerateConsoleCtrlEvent (?)

  // to signal to sub process and all children
  // TODO *** - error check

  pid_t ret;

  if (!(ret = fork())) {
    setsid(); // create new session id
    setpgid(capable->mainpid, 0); // create new pgid
    IGN_RET(system(com));
    _exit(0);
  }

  return ret;
}


/////////////////////////////////////////////

int lives_chdir(const char *path, boolean no_error_dlg) {
  /// returns 0 on success
  /// on failure pops up an error dialog, unless no_error_dlg is TRUE
  int retval = chdir(path);

  if (retval) {
    char *msg = lives_strdup_printf("Chdir failed to: %s", path);
    THREADVAR(chdir_failed) = TRUE;
    if (!no_error_dlg) {
      LIVES_ERROR(msg);
      do_chdir_failed_error(path);
    } else LIVES_DEBUG(msg);
    lives_free(msg);
  }
  return retval;
}


LIVES_GLOBAL_INLINE boolean lives_freep(void **ptr) {
  // free a pointer and nullify it, only if it is non-null to start with
  // pass the address of the pointer in
  if (ptr && *ptr) {
    lives_free(*ptr);
    *ptr = NULL;
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE int lives_kill(lives_pid_t pid, int sig) {
  if (pid == 0) {
    LIVES_ERROR("Tried to kill pid 0");
    return -1;
  }
  return kill(pid, sig);
}


LIVES_GLOBAL_INLINE int lives_killpg(lives_pgid_t pgrp, int sig) {return killpg(pgrp, sig);}


LIVES_GLOBAL_INLINE void clear_mainw_msg(void) {lives_memset(mainw->msg, 0, MAINW_MSG_SIZE);}


LIVES_GLOBAL_INLINE uint64_t lives_10pow(int pow) {
  register int i;
  uint64_t res = 1;
  for (i = 0; i < pow; i++) res *= 10;
  return res;
}


LIVES_GLOBAL_INLINE double lives_fix(double val, int decimals) {
  double factor = (double)lives_10pow(decimals);
  if (val >= 0.) return (double)((int)(val * factor + 0.5)) / factor;
  return (double)((int)(val * factor - 0.5)) / factor;
}


LIVES_GLOBAL_INLINE uint32_t get_approx_ln(uint32_t x) {
  x |= (x >> 1); x |= (x >> 2); x |= (x >> 4); x |= (x >> 8); x |= (x >> 16);
  return (++x) >> 1;
}

LIVES_GLOBAL_INLINE uint64_t get_approx_ln64(uint64_t x) {
  x |= (x >> 1); x |= (x >> 2); x |= (x >> 4); x |= (x >> 8); x |= (x >> 16); x |= (x >> 32);
  return (++x) >> 1;
}

LIVES_GLOBAL_INLINE uint64_t get_near2pow(uint64_t val) {
  uint64_t low = get_approx_ln64(val), high = low * 2;
  if (high < low || (val - low < high - val)) return low;
  return high;
}

///////////////////////////////////////////////////////////

static lives_time_source_t lastt = LIVES_TIME_SOURCE_NONE;
static ticks_t delta = 0;

void reset_playback_clock(void) {
  mainw->cadjticks = mainw->adjticks = mainw->syncticks = 0;
  lastt = LIVES_TIME_SOURCE_NONE;
  delta = 0;
}


ticks_t lives_get_current_playback_ticks(int64_t origsecs, int64_t orignsecs, lives_time_source_t *time_source) {
  // get the time using a variety of methods
  // time_source may be NULL or LIVES_TIME_SOURCE_NONE to set auto
  // or another value to force it (EXTERNAL cannot be forced)
  lives_time_source_t *tsource, xtsource = LIVES_TIME_SOURCE_NONE;
  ticks_t clock_ticks, current = -1;
  static ticks_t lclock_ticks, interticks;

  if (time_source) tsource = time_source;
  else tsource = &xtsource;

  clock_ticks = lives_get_relative_ticks(origsecs, orignsecs);
  mainw->clock_ticks = clock_ticks;

  if (*tsource == LIVES_TIME_SOURCE_EXTERNAL) *tsource = LIVES_TIME_SOURCE_NONE;

  if (mainw->foreign || prefs->force_system_clock || (prefs->vj_mode && (prefs->audio_src == AUDIO_SRC_EXT))) {
    *tsource = LIVES_TIME_SOURCE_SYSTEM;
    current = clock_ticks;
  }

  if (*tsource == LIVES_TIME_SOURCE_NONE) {
#ifdef ENABLE_JACK_TRANSPORT
    if (mainw->jack_can_stop && (prefs->jack_opts & JACK_OPTS_TIMEBASE_CLIENT) &&
        (prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT) && !(mainw->record && !(prefs->rec_opts & REC_FRAMES))) {
      // calculate the time from jack transport
      *tsource = LIVES_TIME_SOURCE_EXTERNAL;
      current = jack_transport_get_current_ticks();
    }
#endif
  }

  if (is_realtime_aplayer(prefs->audio_player) && (*tsource == LIVES_TIME_SOURCE_NONE ||
      *tsource == LIVES_TIME_SOURCE_SOUNDCARD)) {
    if ((!mainw->is_rendering || (mainw->multitrack && !cfile->opening && !mainw->multitrack->is_rendering)) &&
        (!(mainw->fixed_fpsd > 0. || (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback)))) {
      // get time from soundcard
      // this is done so as to synch video stream with the audio
      // we do this in two cases:
      // - for internal audio, playing back a clip with audio (writing)
      // - or when audio source is set to external (reading) and we are recording, no internal audio generator is running

      // we ignore this if we are running with a playback plugin which requires a fixed framerate (e.g a streaming plugin)
      // in that case we will adjust the audio rate to fit the system clock

      // or if we are rendering

#ifdef ENABLE_JACK
      if (prefs->audio_player == AUD_PLAYER_JACK &&
          ((prefs->audio_src == AUDIO_SRC_INT && mainw->jackd && mainw->jackd->in_use &&
            IS_VALID_CLIP(mainw->jackd->playing_file) && mainw->files[mainw->jackd->playing_file]->achans > 0) ||
           (prefs->audio_src == AUDIO_SRC_EXT && mainw->jackd_read && mainw->jackd_read->in_use))) {
        *tsource = LIVES_TIME_SOURCE_SOUNDCARD;
        if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          current = lives_jack_get_time(mainw->jackd_read);
        else
          current = lives_jack_get_time(mainw->jackd);
      }
#endif

#ifdef HAVE_PULSE_AUDIO
      if (prefs->audio_player == AUD_PLAYER_PULSE &&
          ((prefs->audio_src == AUDIO_SRC_INT && mainw->pulsed && mainw->pulsed->in_use &&
            IS_VALID_CLIP(mainw->pulsed->playing_file) && mainw->files[mainw->pulsed->playing_file]->achans > 0) ||
           (prefs->audio_src == AUDIO_SRC_EXT && mainw->pulsed_read && mainw->pulsed_read->in_use))) {
        *tsource = LIVES_TIME_SOURCE_SOUNDCARD;
        if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          current = lives_pulse_get_time(mainw->pulsed_read);
        else current = lives_pulse_get_time(mainw->pulsed);
      }
#endif
    }
  }

  if (*tsource == LIVES_TIME_SOURCE_NONE || current == -1) {
    *tsource = LIVES_TIME_SOURCE_SYSTEM;
    current = clock_ticks;
  }

  //if (lastt != *tsource) {
  /* g_print("t1 = %ld, t2 = %ld cadj =%ld, adj = %ld del =%ld %ld %ld\n", clock_ticks, current, mainw->cadjticks, mainw->adjticks, */
  /*         delta, clock_ticks + mainw->cadjticks, current + mainw->adjticks); */
  //}

  /// synchronised timing
  /// it can be helpful to imagine a virtual clock which is at currrent time:
  /// clock time - cadjticks = virtual time = other time + adjticks
  /// cadjticks and adjticks are only set when we switch from one source to another, i.e the virtual clock will run @ different rates
  /// depending on the source. This is fine as it enables sync with the clock source, provided the time doesn't jump when moving
  /// from one source to another.
  /// when the source changes we then alter either cadjticks or adjticks so that the initial timing matches
  /// e.g when switching to clock source, cadjticks and adjticks will have diverged. So we want to set new cadjtick s.t:
  /// clock ticks - cadjticks == source ticks + adjticks. i.e cadjticks = clock ticks - (source ticks + adjticks).
  /// we use the delta calculated the last time, since the other source may longer be available.
  /// this should not be a concern since this function is called very frequently
  /// recalling cadjticks_new = clock_ticks - (source_ticks + adjticks), and substituting for delta we get:
  // cadjticks_new = clock_ticks - (source_ticks + adjticks) = delta + cadjticks_old
  /// conversely, when switching from clock to source, adjticks_new = clock_ticks - cadjticks - source_ticks
  /// again, this just delta + adjticks; in this case we can use current delta since it is assumed that the system clock is always available

  /// this scheme does, however introduce a small problem, which is that when the sources are switched, we assume that the
  /// time on both clocks is equivalent. This can lead to a problem when switching clips, since temporarily we switch to system
  /// time and then back to soundcard. However, this can cause some updates to the timer to be missed, i.e the audio is playing but the
  /// samples are not counted, however we cannot simply add these to the soundcard timer, as they will be lost due to the resync.
  /// hence we need mainw->syncticks --> a global adjustment which is independant of the clock source. This is similar to
  /// mainw->deltaticks for the player, however, deltaticks is a temporary impulse, whereas syncticks is a permanent adjustment.

  if (*tsource == LIVES_TIME_SOURCE_SYSTEM)  {
    if (lastt != LIVES_TIME_SOURCE_SYSTEM && lastt != LIVES_TIME_SOURCE_NONE) {
      // current + adjt == clock_ticks - cadj /// interticks == lcurrent + adj
      // current - ds + adjt == clock_ticks - dc - cadj /// interticks == lcurrent + adj

      // cadj = clock_ticks - interticks + (current - lcurrent) - since we may not have current
      // we have to approximate with clock_ticks - lclock_ticks
      mainw->cadjticks = clock_ticks - interticks - (clock_ticks - lclock_ticks);
    }
    interticks = clock_ticks - mainw->cadjticks;
  } else {
    if (lastt == LIVES_TIME_SOURCE_SYSTEM) {
      // current - ds + adjt == clock_ticks - dc - cadj /// iinterticks == lclock_ticks - cadj ///
      mainw->adjticks = interticks - current + (clock_ticks - lclock_ticks);
    }
    interticks = current + mainw->adjticks;
  }

  /* if (lastt != *tsource) { */
  /*   g_print("aft t1 = %ld, t2 = %ld cadj =%ld, adj = %ld del =%ld %ld %ld\n", clock_ticks, current, mainw->cadjticks, */
  /*           mainw->adjticks, delta, clock_ticks + mainw->cadjticks, current + mainw->adjticks); */
  /* } */
  lclock_ticks = clock_ticks;
  lastt = *tsource;
  return interticks + mainw->syncticks;
}


///////////////// alarms /////

LIVES_GLOBAL_INLINE lives_alarm_t lives_alarm_reset(lives_alarm_t alarm_handle, ticks_t ticks) {
  // set to now + offset
  // invalid alarm number
  lives_timeout_t *alarm;
  if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid alarm handle");
    break_me("inv alarm handle in lives_alarm_reset");
    return -1;
  }

  // offset of 1 was added for caller
  alarm = &mainw->alarms[--alarm_handle];

  alarm->lastcheck = lives_get_current_ticks();
  alarm->tleft = ticks;
  return ++alarm_handle;
}


/** set alarm for now + delta ticks (10 nanosec)
   param ticks (10 nanoseconds) is the offset when we want our alarm to trigger
   returns int handle or -1
   call lives_get_alarm(handle) to test if time arrived
*/

lives_alarm_t lives_alarm_set(ticks_t ticks) {
  int i;

  // we will assign [this] next
  lives_alarm_t ret;

  pthread_mutex_lock(&mainw->alarmlist_mutex);

  ret = mainw->next_free_alarm;

  if (ret > LIVES_MAX_USER_ALARMS) ret--;
  else {
    // no alarm slots left
    if (mainw->next_free_alarm == ALL_USED) {
      pthread_mutex_unlock(&mainw->alarmlist_mutex);
      LIVES_WARN("No alarms left");
      return ALL_USED;
    }
  }

  // system alarms
  if (ret >= LIVES_MAX_USER_ALARMS) {
    lives_alarm_reset(++ret, ticks);
    pthread_mutex_unlock(&mainw->alarmlist_mutex);
    return ret;
  }

  i = ++mainw->next_free_alarm;

  // find free slot for next time
  while (mainw->alarms[i].lastcheck != 0 && i < LIVES_MAX_USER_ALARMS) i++;

  if (i == LIVES_MAX_USER_ALARMS) mainw->next_free_alarm = ALL_USED; // no more alarm slots
  else mainw->next_free_alarm = i; // OK
  lives_alarm_reset(++ret, ticks);
  pthread_mutex_unlock(&mainw->alarmlist_mutex);

  return ret;
}


/*** check if alarm time passed yet, if so clear that alarm and return TRUE
   else return FALSE
*/
ticks_t lives_alarm_check(lives_alarm_t alarm_handle) {
  ticks_t curticks;
  lives_timeout_t *alarm;

  // invalid alarm number
  if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid alarm handle");
    break_me("inv alarm handle in lives_alarm_check");
    return -1;
  }

  // offset of 1 was added for caller
  alarm = &mainw->alarms[--alarm_handle];

  // alarm time was never set !
  if (alarm->lastcheck == 0) {
    LIVES_WARN("Alarm time not set");
    return 0;
  }

  curticks = lives_get_current_ticks();

  if (prefs->show_dev_opts) {
    /// guard against long interrupts (when debugging for example)
    // if the last check was > 5 seconds ago, we ignore the time jump, updating the check time but not reducing the time left
    if (curticks - alarm->lastcheck > 5 * TICKS_PER_SECOND) {
      alarm->lastcheck = curticks;
      return alarm->tleft;
    }
  }

  alarm->tleft -= curticks - alarm->lastcheck;

  if (alarm->tleft <= 0) {
    // reached alarm time, free up this timer and return TRUE
    alarm->lastcheck = 0;
    LIVES_DEBUG("Alarm reached");
    return 0;
  }
  alarm->lastcheck = curticks;
  // alarm time not reached yet
  return alarm->tleft;
}


boolean lives_alarm_clear(lives_alarm_t alarm_handle) {
  if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid clear alarm handle");
    return FALSE;
  }

  mainw->alarms[--alarm_handle].lastcheck = 0;

  if (alarm_handle < LIVES_MAX_USER_ALARMS
      && (mainw->next_free_alarm == ALL_USED || alarm_handle < mainw->next_free_alarm)) {
    mainw->next_free_alarm = alarm_handle;
  }
  return TRUE;
}



/* convert to/from a big endian 32 bit float for internal use */
LIVES_GLOBAL_INLINE float LEFloat_to_BEFloat(float f) {
  if (capable->byte_order == LIVES_LITTLE_ENDIAN) swab4(&f, &f, 1);
  return f;
}


LIVES_GLOBAL_INLINE double calc_time_from_frame(int clip, int frame) {return (frame - 1.) / mainw->files[clip]->fps;}


LIVES_GLOBAL_INLINE int calc_frame_from_time(int filenum, double time) {
  // return the nearest frame (rounded) for a given time, max is cfile->frames
  int frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (int)(time * mainw->files[filenum]->fps + 1.49999);
  return (frame < mainw->files[filenum]->frames) ? frame : mainw->files[filenum]->frames;
}


LIVES_GLOBAL_INLINE int calc_frame_from_time2(int filenum, double time) {
  // return the nearest frame (rounded) for a given time
  // allow max (frames+1)
  int frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (int)(time * mainw->files[filenum]->fps + 1.49999);
  return (frame < mainw->files[filenum]->frames + 1) ? frame : mainw->files[filenum]->frames + 1;
}


LIVES_GLOBAL_INLINE int calc_frame_from_time3(int filenum, double time) {
  // return the nearest frame (floor) for a given time
  // allow max (frames+1)
  int frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (int)(time * mainw->files[filenum]->fps + 1.);
  return (frame < mainw->files[filenum]->frames + 1) ? frame : mainw->files[filenum]->frames + 1;
}


LIVES_GLOBAL_INLINE int calc_frame_from_time4(int filenum, double time) {
  // return the nearest frame (rounded) for a given time, no maximum
  int frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (int)(time * mainw->files[filenum]->fps + 1.49999);
  return frame;
}


static boolean check_for_audio_stop(int fileno, frames_t first_frame, frames_t last_frame) {
  // this is only used for older versions with non-realtime players
  // return FALSE if audio stops playback
  lives_clip_t *sfile = mainw->files[fileno];
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd && mainw->jackd->playing_file == fileno) {
    if (!mainw->loop || mainw->playing_sel) {
      if (!mainw->loop_cont) {
        if ((sfile->adirection == LIVES_DIRECTION_REVERSE && mainw->aframeno - 0.0001 < (double)first_frame + 0.0001)
            || (sfile->adirection == LIVES_DIRECTION_FORWARD && mainw->aframeno + 0.0001 >= (double)last_frame - 0.0001)) {
          return FALSE;
        }
      }
    } else {
      if (!mainw->loop_cont) {
        if ((sfile->adirection == LIVES_DIRECTION_REVERSE && mainw->aframeno < 0.9999) ||
            (sfile->adirection == LIVES_DIRECTION_FORWARD && calc_time_from_frame(mainw->current_file, mainw->aframeno + 1.0001)
             >= cfile->laudio_time - 0.0001)) {
          return FALSE;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed && mainw->pulsed->playing_file == fileno) {
    if (!mainw->loop || mainw->playing_sel) {
      if (!mainw->loop_cont) {
        if ((sfile->adirection == LIVES_DIRECTION_REVERSE && mainw->aframeno - 0.0001 < (double)first_frame + 0.0001)
            || (sfile->adirection == LIVES_DIRECTION_FORWARD && mainw->aframeno + 1.0001 >= (double)last_frame - 0.0001)) {
          return FALSE;
        }
      }
    } else {
      if (!mainw->loop_cont) {
        if ((sfile->adirection == LIVES_DIRECTION_REVERSE && mainw->aframeno < 0.9999) ||
            (sfile->adirection == LIVES_DIRECTION_FORWARD && calc_time_from_frame(mainw->current_file, mainw->aframeno + 1.0001)
             >= cfile->laudio_time - 0.0001)) {
          return FALSE;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

#endif
  return TRUE;
}


void calc_aframeno(int fileno) {
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && ((mainw->jackd && mainw->jackd->playing_file == fileno) ||
      (mainw->jackd_read && mainw->jackd_read->playing_file == fileno))) {
    // get seek_pos from jack
    if (mainw->jackd_read) mainw->aframeno = lives_jack_get_pos(mainw->jackd_read) * cfile->fps + 1.;
    else mainw->aframeno = lives_jack_get_pos(mainw->jackd) * cfile->fps + 1.;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && ((mainw->pulsed && mainw->pulsed->playing_file == fileno) ||
      (mainw->pulsed_read && mainw->pulsed_read->playing_file == fileno))) {
    // get seek_pos from pulse
    if (mainw->pulsed_read) mainw->aframeno = lives_pulse_get_pos(mainw->pulsed_read) * cfile->fps + 1.;
    else mainw->aframeno = lives_pulse_get_pos(mainw->pulsed) * cfile->fps + 1.;
  }
#endif
}


frames_t calc_new_playback_position(int fileno, ticks_t otc, ticks_t *ntc) {
  // returns a frame number (floor) using sfile->last_frameno and ntc-otc
  // takes into account looping modes

  // the range is first_frame -> last_frame

  // which is generally 1 -> sfile->frames, unless we are playing a selection

  // in case the frame is out of range and playing, returns 0 and sets mainw->cancelled

  // ntc is adjusted backwards to timecode of the new frame

  // the basic operation is quite simple, given the time difference between the last frame and
  // now, we calculate the new frame from the current fps and then ensure it is in the range
  // first_frame -> last_frame

  // Complications arise because we have ping-pong loop mode where the play direction
  // alternates - here we need to determine how many times we have reached the start or end
  // play point. This is similar to the winding number in topological calculations.

  // caller should check return value of ntc, and if it differs from otc, show the frame

  // note we also calculate the audio "frame" and position for realtime audio players
  // this is done so we can check here if audio limits stopped playback

  static boolean norecurse = FALSE;
  static ticks_t last_ntc = 0;

  ticks_t ddtc = *ntc - last_ntc;
  ticks_t dtc = *ntc - otc;
  int64_t first_frame, last_frame, selrange;
  lives_clip_t *sfile = mainw->files[fileno];
  double fps;
  lives_direction_t dir;
  frames_t cframe, nframe, fdelta;
  int nloops;
  int aplay_file = fileno;

  if (!sfile) return 0;

  cframe = sfile->last_frameno;
  if (norecurse) return cframe;

  if (sfile->achans > 0) {
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE) {
      if (mainw->pulsed) aplay_file = mainw->pulsed->playing_file;
    }
#endif
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      if (mainw->jackd) aplay_file = mainw->jackd->playing_file;
    }
#endif
    if (!IS_VALID_CLIP(aplay_file)) aplay_file = -1;
    else {
      if (fileno != aplay_file) {
        off64_t aseek_pos_delta = (off64_t)((double)dtc / TICKS_PER_SECOND_DBL * (double)(sfile->arate))
                                  * sfile->achans * sfile->asampsize / 8;
        if (sfile->adirection == LIVES_DIRECTION_FORWARD) sfile->aseek_pos += aseek_pos_delta;
        else sfile->aseek_pos -= aseek_pos_delta;
        if (sfile->aseek_pos < 0 || sfile->aseek_pos > sfile->afilesize) {
          nloops = sfile->aseek_pos / sfile->afilesize;
          if (mainw->ping_pong && (sfile->adirection == LIVES_DIRECTION_REVERSE || clip_can_reverse(fileno))) {
            sfile->adirection += nloops;
            sfile->adirection &= 1;
            if (sfile->adirection == LIVES_DIRECTION_BACKWARD && !clip_can_reverse(fileno))
              sfile->adirection = LIVES_DIRECTION_REVERSE;
          }
          sfile->aseek_pos -= nloops * sfile->afilesize;
          if (sfile->adirection == LIVES_DIRECTION_REVERSE) sfile->aseek_pos = sfile->afilesize - sfile->aseek_pos;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (sfile->frames == 0 && !mainw->foreign) {
    if (fileno == mainw->playing_file) mainw->scratch = SCRATCH_NONE;
    return 0;
  }

  fps = sfile->pb_fps;
  if (!LIVES_IS_PLAYING || (fps < 0.001 && fps > -0.001 && mainw->scratch != SCRATCH_NONE)) fps = sfile->fps;

  if (fps < 0.001 && fps > -0.001) {
    *ntc = otc;
    if (fileno == mainw->playing_file) {
      mainw->scratch = SCRATCH_NONE;
      if (prefs->audio_src == AUDIO_SRC_INT) calc_aframeno(fileno);
    }
    return cframe;
  }

  // dtc is delta ticks (last frame time - current time), quantise this to the frame rate and round down
  dtc = q_gint64_floor(dtc, fps);

  // ntc is the time when the next frame should be / have been played, or if dtc is zero we just set it to otc - the last frame time
  *ntc = otc + dtc;

  // nframe is our new frame; convert dtc to sencods, and multiply by the frame rate, then add or subtract from current frame number
  // the small constant is just to account for rounding errors
  if (fps >= 0.)
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps + .00001);
  else
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps - .00001);

  if (nframe != cframe) {
    if (!IS_NORMAL_CLIP(fileno)) {
      return 1;
    }
    if (mainw->foreign) return sfile->frameno + 1;
  }

  if (fileno == mainw->playing_file) {
    /// if we are scratching we do the following:
    /// the time since the last call is considered to have happened at an increased fps (fwd or back)
    /// we recalculate the frame at ntc as if we were at the faster framerate.

    if (mainw->scratch == SCRATCH_FWD || mainw->scratch == SCRATCH_BACK
        || mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA) {
      if (mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA) ddtc *= 4;
      if (mainw->scratch == SCRATCH_BACK || mainw->scratch == SCRATCH_BACK_EXTRA) {
        mainw->deltaticks -= ddtc * KEY_RPT_INTERVAL * prefs->scratchback_amount
                             * USEC_TO_TICKS / TICKS_PER_SECOND_DBL;
      } else mainw->deltaticks += ddtc * KEY_RPT_INTERVAL * prefs->scratchback_amount
                                    * USEC_TO_TICKS / TICKS_PER_SECOND_DBL;
      // dtc is delta ticks, quantise this to the frame rate and round down
      mainw->deltaticks = q_gint64_floor(mainw->deltaticks, fps * 4);
    }

    if (nframe != cframe) {
      int delval = (ticks_t)((double)mainw->deltaticks / TICKS_PER_SECOND_DBL * fps + .5);
      if (delval <= -1 || delval >= 1) {
        /// the frame number changed, but we will recalulate the value using mainw->deltaticks
        frames64_t xnframe = cframe + (int64_t)delval;
        frames64_t dframes = xnframe - nframe;

        if (xnframe != nframe) {
          nframe = xnframe;
          /// retain the fractional part for next time
          mainw->deltaticks -= (ticks_t)((double)delval / fps * TICKS_PER_SECOND_DBL);
          if (nframe != cframe) {
            sfile->last_frameno += dframes;
            if (fps < 0. && mainw->scratch == SCRATCH_FWD) sfile->last_frameno--;
            if (fps > 0. &&  mainw->scratch == SCRATCH_BACK) sfile->last_frameno++;
            mainw->scratch = SCRATCH_JUMP_NORESYNC;
          } else mainw->scratch = SCRATCH_NONE;
        }
      }
    }
    last_ntc = *ntc;
  }

  last_frame = sfile->frames;
  first_frame = 1;

  if (fileno == mainw->playing_file) {
    // calculate audio "frame" from the number of samples played
    if (prefs->audio_src == AUDIO_SRC_INT) calc_aframeno(fileno);

    if (nframe == cframe || mainw->foreign) {
      if (!mainw->foreign && fileno == mainw->playing_file &&
          mainw->scratch == SCRATCH_JUMP && (!mainw->event_list || mainw->record || mainw->record_paused) &&
          (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
        resync_audio(nframe);
        mainw->scratch = SCRATCH_JUMP_NORESYNC;
      }
      return nframe;
    }

    if (!mainw->clip_switched && (mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV)) {
      last_frame = mainw->playing_sel ? sfile->end : mainw->play_end;
      if (last_frame > sfile->frames) last_frame = sfile->frames;
      first_frame = mainw->playing_sel ? sfile->start : mainw->loop_video ? mainw->play_start : 1;
      if (first_frame > sfile->frames) first_frame = sfile->frames;
    }

    if (sfile->frames > 1 && prefs->noframedrop && (mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV)) {
      // if noframedrop is set, we may not skip any frames
      // - the usual situation is that we are allowed to drop late frames
      // in this mode we may be forced to play at a reduced framerate
      if (nframe > cframe + 1) {
        // update this so the player can calculate 'dropped' frames correctly
        cfile->last_frameno -= (nframe - cframe - 1);
        nframe = cframe + 1;
      } else if (nframe < cframe - 1) {
        cfile->last_frameno += (cframe - 1 - nframe);
        nframe = cframe - 1;
      }
    }
  }

  while (IS_NORMAL_CLIP(fileno) && (nframe < first_frame || nframe > last_frame)) {
    // get our frame back to within bounds:
    // define even parity (0) as backwards, odd parity (1) as forwards
    // we subtract the lower bound from the new frame, and divide the result by the selection length,
    // rounding towards zero. (nloops)
    // in ping mode this is then added to the original direction, and the resulting parity gives the new direction
    // the remainder after doing the division is then either added to the selection start (forwards)
    /// or subtracted from the selection end (backwards) [if we started backwards then the boundary crossing will be with the
    // lower bound and nloops and the remainder will be negative, so we subtract and add the negatvie value instead]
    // we must also set

    if (fileno == mainw->playing_file) {
      // check if video stopped playback
      if (mainw->whentostop == STOP_ON_VID_END) {
        mainw->cancelled = CANCEL_VID_END;
        mainw->scratch = SCRATCH_NONE;
        return 0;
      }
      // we need to set this for later in the function
      mainw->scratch = SCRATCH_JUMP;
    }

    if (first_frame == last_frame) {
      nframe = first_frame;
      break;
    }

    if (fps < 0.) dir = LIVES_DIRECTION_BACKWARD; // 0, and even parity
    else dir = LIVES_DIRECTION_FORWARD; // 1, and odd parity

    if (dir == LIVES_DIRECTION_FORWARD && nframe < first_frame) {
      // if FWD and before lower bound, just jump to lower bound
      nframe = first_frame;
      sfile->last_frameno = first_frame;
      break;
    }

    if (dir == LIVES_DIRECTION_BACKWARD && nframe  > last_frame) {
      // if BACK and after upper bound, just jump to upper bound
      nframe = last_frame;
      sfile->last_frameno = last_frame;
      break;
    }

    fdelta = ABS(sfile->frameno - sfile->last_frameno);
    nframe -= first_frame;
    selrange = (1 + last_frame - first_frame);
    if (nframe < 0) nframe = -nframe;
    nloops = nframe / selrange;
    if (mainw->ping_pong && (dir == LIVES_DIRECTION_BACKWARD || (clip_can_reverse(fileno)))) {
      dir += nloops + dir + 1;
      dir = LIVES_DIRECTION_PAR(dir);
      if (dir == LIVES_DIRECTION_BACKWARD && !clip_can_reverse(fileno))
        dir = LIVES_DIRECTION_FORWARD;
    }

    nframe -= nloops * selrange;

    if (dir == LIVES_DIRECTION_FORWARD) {
      nframe += first_frame;
      sfile->last_frameno = nframe - fdelta;
      if (fps < 0.) {
        // backwards -> forwards
        if (fileno == mainw->playing_file) {
          /// must set norecurse, otherwise we can end up in an infinite loop since dirchange_callback calls this function
          norecurse = TRUE;
          dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0,
                             LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
          norecurse = FALSE;
        } else sfile->pb_fps = -sfile->pb_fps;
      }
    }

    else {
      nframe = last_frame - nframe;
      sfile->last_frameno = nframe + fdelta;
      if (fps > 0.) {
        // forwards -> backwards
        if (fileno == mainw->playing_file) {
          norecurse = TRUE;
          dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0,
                             LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
          norecurse = FALSE;
        } else sfile->pb_fps = -sfile->pb_fps;
      }
    }
    break;
  }

  if (fileno == mainw->playing_file && prefs->audio_src == AUDIO_SRC_INT && fileno == aplay_file && sfile->achans > 0
      && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
      && (mainw->scratch != SCRATCH_NONE && mainw->scratch != SCRATCH_JUMP_NORESYNC)) {
    if (mainw->whentostop == STOP_ON_AUD_END) {
      // check if audio stopped playback. The audio player will also be checking this, BUT: we have to check here too
      // before doing any resync, otherwise the video can loop and if the audio is then resynced it may never reach the end
      calc_aframeno(fileno);
      if (!check_for_audio_stop(fileno, first_frame + 1, last_frame - 1)) {
        mainw->cancelled = CANCEL_AUD_END;
        mainw->scratch = SCRATCH_NONE;
        return 0;
      }
      resync_audio(nframe);
      if (mainw->scratch == SCRATCH_JUMP) {
        mainw->video_seek_ready = TRUE;   /// ????
        mainw->scratch = SCRATCH_JUMP_NORESYNC;
      }
    }
  }
  if (fileno == mainw->playing_file) {
    if (mainw->scratch != SCRATCH_NONE) {
      sfile->last_frameno = nframe;
      mainw->scratch = SCRATCH_JUMP_NORESYNC;
    }
  }
  return nframe;
}


void calc_maxspect(int rwidth, int rheight, int *cwidth, int *cheight) {
  // calculate maxspect (maximum size which maintains aspect ratio)
  // of cwidth, cheight - given restrictions rwidth * rheight

  double aspect;
  if (*cwidth <= 0 || *cheight <= 0 || rwidth <= 0 || rheight <= 0) return;

  aspect = (double)(*cwidth) / (double)(*cheight);

  *cwidth = rwidth;
  *cheight = (double)(*cwidth) / aspect;
  if (*cheight > rheight) {
    // image too tall shrink it
    *cheight = rheight;
    *cwidth = (double)(*cheight) * aspect;
  }
  *cwidth = ((*cwidth + 1) >> 1) << 1;
  *cheight = ((*cheight + 1) >> 1) << 1;
}


void calc_minspect(int rwidth, int rheight, int *cwidth, int *cheight) {
  // calculate minspect (maximum size which conforms to aspect ratio of
  // of rwidth, rheight) - given restrictions cwidth * cheight

  double aspect, dheight;

  if (*cwidth <= 0 || *cheight <= 0 || rwidth <= 0 || rheight <= 0) return;

  aspect = (double)(rwidth) / (double)(rheight);
  dheight = (double)(*cwidth) / aspect;

  if (dheight <= ((double)(*cheight) * (1. + ASPECT_ALLOWANCE)))
    *cheight = (int)dheight;
  else
    *cwidth = (int)((double)(*cheight * aspect));

  *cwidth = ((*cwidth + 1) >> 1) << 1;
  *cheight = ((*cheight + 1) >> 1) << 1;
}


void calc_midspect(int rwidth, int rheight, int *cwidth, int *cheight) {
  // calculate midspect (minimum size which conforms to aspect ratio of
  // of rwidth, rheight) - which contains cwidth, cheight

  double aspect, dheight;

  if (*cwidth <= 0 || *cheight <= 0 || rwidth <= 0 || rheight <= 0) return;

  aspect = (double)(rwidth) / (double)(rheight);
  dheight = (double)(*cwidth) / aspect;

  if (dheight >= ((double)(*cheight) * (1. - ASPECT_ALLOWANCE)))
    *cheight = (int)dheight;
  else
    *cwidth = (int)((double)(*cheight * aspect));

  *cwidth = ((*cwidth + 1) >> 1) << 1;
  *cheight = ((*cheight + 1) >> 1) << 1;
}

/////////////////////////////////////////////////////////////////////////////

void init_clipboard(void) {
  int current_file = mainw->current_file;
  char *com;

  if (!clipboard) {
    // here is where we create the clipboard
    // use get_new_handle(clipnumber,name);
    if (!get_new_handle(CLIPBOARD_FILE, "clipboard")) {
      mainw->error = TRUE;
      return;
    }
  } else {
    // experimental feature - we can have duplicate copies of the clipboard with different palettes / gamma
    for (int i = 0; i < mainw->ncbstores; i++) {
      if (mainw->cbstores[i] != clipboard) {
        char *clipd = lives_build_path(prefs->workdir, mainw->cbstores[i]->handle, NULL);
        if (lives_file_test(clipd, LIVES_FILE_TEST_EXISTS)) {
          char *com = lives_strdup_printf("%s close \"%s\"", prefs->backend, mainw->cbstores[i]->handle);
          char *permitname = lives_build_path(clipd, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
          lives_touch(permitname);
          lives_free(permitname);
          lives_system(com, TRUE);
          lives_free(com);
        }
        lives_free(clipd);
      }
    }
    mainw->ncbstores = 0;

    if (clipboard->frames > 0) {
      // clear old clipboard
      // need to set current file to 0 before monitoring progress
      mainw->current_file = CLIPBOARD_FILE;
      cfile->cb_src = current_file;

      if (cfile->clip_type == CLIP_TYPE_FILE) {
        lives_freep((void **)&cfile->frame_index);
        if (cfile->ext_src && cfile->ext_src_type == LIVES_EXT_SRC_DECODER) {
          close_clip_decoder(CLIPBOARD_FILE);
        }
        cfile->clip_type = CLIP_TYPE_DISK;
      }

      com = lives_strdup_printf("%s clear_clipboard \"%s\"", prefs->backend, clipboard->handle);
      lives_rm(clipboard->info_file);
      lives_system(com, FALSE);
      lives_free(com);

      if (THREADVAR(com_failed)) {
        mainw->current_file = current_file;
        return;
      }

      cfile->progress_start = cfile->start;
      cfile->progress_end = cfile->end;
      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE, FALSE, _("Clearing the clipboard"));
    }
  }
  mainw->current_file = current_file;
  *clipboard->file_name = 0;
  clipboard->img_type = IMG_TYPE_BEST; // override the pref
  clipboard->cb_src = current_file;
}


weed_plant_t *get_nth_info_message(int n) {
  weed_plant_t *msg = mainw->msg_list;
  const char *leaf;
  weed_error_t error;
  int m = 0;

  if (n < 0) return NULL;

  if (n >= mainw->n_messages) n = mainw->n_messages - 1;

  if (n >= (mainw->n_messages >> 1)) {
    m = mainw->n_messages - 1;
    msg = weed_get_plantptr_value(msg, WEED_LEAF_PREVIOUS, &error);
  }
  if (mainw->ref_message && ABS(mainw->ref_message_n - n) < ABS(m - n)) {
    m = mainw->ref_message_n;
    msg = mainw->ref_message;
  }

  if (m > n) leaf = WEED_LEAF_PREVIOUS;
  else leaf = WEED_LEAF_NEXT;

  while (m != n) {
    msg = weed_get_plantptr_value(msg, leaf, &error);
    if (error != WEED_SUCCESS) return NULL;
    if (m > n) m--;
    else m++;
  }
  mainw->ref_message = msg;
  mainw->ref_message_n = n;
  return msg;
}


char *dump_messages(int start, int end) {
  weed_plant_t *msg = mainw->msg_list;
  char *text = lives_strdup(""), *tmp, *msgtext;
  boolean needs_newline = FALSE;
  int msgno = 0;
  int error;

  while (msg) {
    msgtext = weed_get_string_value(msg, WEED_LEAF_LIVES_MESSAGE_STRING, &error);
    if (error != WEED_SUCCESS) break;
    if (msgno >= start) {
#ifdef SHOW_MSG_LINENOS
      tmp = lives_strdup_printf("%s%s(%d)%s", text, needs_newline ? "\n" : "", msgno, msgtext);
#else
      tmp = lives_strdup_printf("%s%s%s", text, needs_newline ? "\n" : "", msgtext);
#endif
      lives_free(text);
      text = tmp;
      needs_newline = TRUE;
    }
    lives_free(msgtext);
    if (++msgno > end) if (end > -1) break;
    msg = weed_get_plantptr_value(msg, WEED_LEAF_NEXT, &error);
    if (error != WEED_SUCCESS) break;
  }
  return text;
}


static weed_plant_t *make_msg(const char *text) {
  // make single msg. text should have no newlines in it, except possibly as the last character.
  weed_plant_t *msg = weed_plant_new(WEED_PLANT_LIVES);
  if (!msg) return NULL;

  weed_set_int_value(msg, WEED_LEAF_LIVES_SUBTYPE, LIVES_WEED_SUBTYPE_MESSAGE);
  weed_set_string_value(msg, WEED_LEAF_LIVES_MESSAGE_STRING, text);

  weed_set_plantptr_value(msg, WEED_LEAF_NEXT, NULL);
  weed_set_plantptr_value(msg, WEED_LEAF_PREVIOUS, NULL);
  return msg;
}


int free_n_msgs(int frval) {
  int error;
  weed_plant_t *next, *end;

  if (frval <= 0) return WEED_SUCCESS;
  if (frval > mainw->n_messages || !mainw->msg_list) frval = mainw->n_messages;

  end = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, &error); // list end
  if (error != WEED_SUCCESS) {
    return error;
  }

  while (frval-- && mainw->msg_list) {
    next = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_NEXT, &error); // becomes new head
    if (error != WEED_SUCCESS) {
      return error;
    }
    weed_plant_free(mainw->msg_list);
    mainw->msg_list = next;
    if (mainw->msg_list) {
      if (mainw->msg_list == end) weed_set_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, NULL);
      else weed_set_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, end);
    }
    mainw->n_messages--;
    if (mainw->ref_message) {
      if (--mainw->ref_message_n < 0) mainw->ref_message = NULL;
    }
  }

  if (mainw->msg_adj)
    lives_adjustment_set_value(mainw->msg_adj, lives_adjustment_get_value(mainw->msg_adj) - 1.);
  return WEED_SUCCESS;
}


int add_messages_to_list(const char *text) {
  // append text to our message list, splitting it into lines
  // if we hit the max message limit then free the oldest one
  // returns a weed error
  weed_plant_t *msg, *end;;
  char **lines;
  int error, i, numlines;

  if (prefs->max_messages == 0) return WEED_SUCCESS;
  if (!text || !*text) return WEED_SUCCESS;

  // split text into lines
  numlines = get_token_count(text, '\n');
  lines = lives_strsplit(text, "\n", numlines);

  for (i = 0; i < numlines; i++) {
    if (!mainw->msg_list) {
      mainw->msg_list = make_msg(lines[i]);
      if (!mainw->msg_list) {
        mainw->n_messages = 0;
        lives_strfreev(lines);
        return WEED_ERROR_MEMORY_ALLOCATION;
      }
      mainw->n_messages = 1;
      continue;
    }

    end = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, &error);
    if (error != WEED_SUCCESS) {
      lives_strfreev(lines);
      return error;
    }
    if (!end) end = mainw->msg_list;

    if (i == 0) {
      // append first line to text of last msg
      char *strg2, *strg = weed_get_string_value(end, WEED_LEAF_LIVES_MESSAGE_STRING, &error);
      if (error != WEED_SUCCESS) {
        lives_strfreev(lines);
        return error;
      }
      strg2 = lives_strdup_printf("%s%s", strg, lines[0]);
      weed_set_string_value(end, WEED_LEAF_LIVES_MESSAGE_STRING, strg2);
      lives_free(strg);
      lives_free(strg2);
      continue;
    }

    if (prefs->max_messages > 0 && mainw->n_messages + 1 > prefs->max_messages) {
      // retire the oldest if we reached the limit
      error = free_n_msgs(1);
      if (error != WEED_SUCCESS) {
        lives_strfreev(lines);
        return error;
      }
      if (!mainw->msg_list) {
        i = numlines - 2;
        continue;
      }
    }

    msg = make_msg(lines[i]);
    if (!msg) {
      lives_strfreev(lines);
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    mainw->n_messages++;

    // head will get new previous (us)
    weed_set_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, msg);
    // we will get new previous (end)
    weed_set_plantptr_value(msg, WEED_LEAF_PREVIOUS, end);
    // end will get new next (us)
    weed_set_plantptr_value(end, WEED_LEAF_NEXT, msg);
  }
  lives_strfreev(lines);
  return WEED_SUCCESS;
}


boolean d_print_urgency(double timeout, const char *fmt, ...) {
  // overlay emergency message on playback frame
  va_list xargs;
  char *text;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  d_print(text);

  if (LIVES_IS_PLAYING && prefs->show_urgency_msgs) {
    int nfa = mainw->next_free_alarm;
    mainw->next_free_alarm = LIVES_URGENCY_ALARM;
    lives_freep((void **)&mainw->urgency_msg);
    lives_alarm_set(timeout * TICKS_PER_SECOND_DBL);
    mainw->next_free_alarm = nfa;
    mainw->urgency_msg = lives_strdup(text);
    lives_free(text);
    return TRUE;
  }
  lives_free(text);
  return FALSE;
}


boolean d_print_overlay(double timeout, const char *fmt, ...) {
  // overlay a message on playback frame
  va_list xargs;
  char *text;
  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  if (LIVES_IS_PLAYING && prefs->show_overlay_msgs && !(mainw->urgency_msg && prefs->show_urgency_msgs)) {
    lives_freep((void **)&mainw->overlay_msg);
    mainw->overlay_msg = lives_strdup(text);
    lives_free(text);
    lives_alarm_reset(mainw->overlay_alarm, timeout * TICKS_PER_SECOND_DBL);
    return TRUE;
  }
  lives_free(text);
  return FALSE;
}


void d_print(const char *fmt, ...) {
  // collect output for the main message area (and info log)

  // there are several small tweaks for this:

  // mainw->suppress_dprint :: TRUE - dont print anything, return (for silencing noisy message blocks)
  // mainw->no_switch_dprint :: TRUE - disable printing of switch message when maine->current_file changes

  // mainw->last_dprint_file :: clip number of last mainw->current_file;
  va_list xargs;

  char *tmp, *text;

  if (!prefs->show_gui) return;
  if (mainw->suppress_dprint) return;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  if (mainw->current_file != mainw->last_dprint_file && mainw->current_file != 0 && !mainw->multitrack &&
      (mainw->current_file == -1 || (cfile && cfile->clip_type != CLIP_TYPE_GENERATOR)) && !mainw->no_switch_dprint) {
    if (mainw->current_file > 0) {
      char *swtext = lives_strdup_printf(_("\n==============================\nSwitched to clip %s\n"),
                                         tmp = get_menu_name(cfile,
                                             TRUE));
      lives_free(tmp);
      add_messages_to_list(swtext);
      lives_free(swtext);
    } else {
      add_messages_to_list(_("\n==============================\nSwitched to empty clip\n"));
    }
  }

  add_messages_to_list(text);
  lives_free(text);

  if (!mainw->go_away && prefs->show_gui && prefs->show_msg_area
      && ((!mainw->multitrack && mainw->msg_area
           && mainw->msg_adj)
          || (!mainw->multitrack && mainw->multitrack->msg_area
              && mainw->multitrack->msg_adj))) {
    if (mainw->multitrack) {
      msg_area_scroll_to_end(mainw->multitrack->msg_area, mainw->multitrack->msg_adj);
      lives_widget_queue_draw_if_visible(mainw->multitrack->msg_area);
    } else {
      msg_area_scroll_to_end(mainw->msg_area, mainw->msg_adj);
      lives_widget_queue_draw_if_visible(mainw->msg_area);
    }
  }

  if ((mainw->current_file == -1 || (cfile && cfile->clip_type != CLIP_TYPE_GENERATOR)) &&
      (!mainw->no_switch_dprint || mainw->current_file != 0)) mainw->last_dprint_file = mainw->current_file;
}


static void d_print_utility(const char *text, int osc_note, const char *osc_detail) {
  boolean nsdp = mainw->no_switch_dprint;
  mainw->no_switch_dprint = TRUE;
  d_print(text);
  if (osc_note != LIVES_OSC_NOTIFY_NONE) lives_notify(osc_note, osc_detail);
  if (!nsdp) {
    mainw->no_switch_dprint = FALSE;
    d_print("");
  }
}


LIVES_GLOBAL_INLINE void d_print_cancelled(void) {
  d_print_utility(_("cancelled.\n"), LIVES_OSC_NOTIFY_CANCELLED, "");
}


LIVES_GLOBAL_INLINE void d_print_failed(void) {
  d_print_utility(_("failed.\n"), LIVES_OSC_NOTIFY_FAILED, "");
}


LIVES_GLOBAL_INLINE void d_print_done(void) {
  d_print_utility(_("done.\n"), 0, NULL);
}


LIVES_GLOBAL_INLINE void d_print_file_error_failed(void) {
  d_print_utility(_("error in file. Failed.\n"), 0, NULL);
}


LIVES_GLOBAL_INLINE void d_print_enough(int frames) {
  if (frames == 0) d_print_cancelled();
  else {
    char *msg = lives_strdup_printf(P_("%d frame is enough !\n", "%d frames are enough !\n", frames), frames);
    d_print_utility(msg, 0, NULL);
    lives_free(msg);
  }
}


void buffer_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data, int clipno,
                       int frameno, double atime, boolean affects_current) {
  lmap_error *err = (lmap_error *)lives_malloc(sizeof(lmap_error));
  if (!err) return;
  err->type = lerror;
  if (name) err->name = lives_strdup(name);
  else err->name = NULL;
  err->data = user_data;
  err->clipno = clipno;
  err->frameno = frameno;
  err->atime = atime;
  err->current = affects_current;
  mainw->new_lmap_errors = lives_list_prepend(mainw->new_lmap_errors, err);
}


void unbuffer_lmap_errors(boolean add) {
  LiVESList *list = mainw->new_lmap_errors;
  while (list) {
    lmap_error *err = (lmap_error *)list->data;
    if (add) add_lmap_error(err->type, err->name, err->data, err->clipno, err->frameno, err->atime, err->current);
    else mainw->files[err->clipno]->tcache_dubious_from = 0;
    if (err->name) lives_free(err->name);
    lives_free(err);
    list = list->next;
  }
  if (mainw->new_lmap_errors) {
    lives_list_free(mainw->new_lmap_errors);
    mainw->new_lmap_errors = NULL;
  }
}


boolean add_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data, int clipno,
                       int frameno, double atime, boolean affects_current) {
  // potentially add a layout map error to the layout textbuffer
  LiVESTextIter end_iter;
  LiVESList *lmap;

  char *text, *name2;
  char **array;

  double orig_fps;
  double max_time;

  int resampled_frame;

  lives_text_buffer_get_end_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter);

  if (affects_current && !user_data) {
    mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                   (livespointer)lives_text_buffer_create_mark
                                   (LIVES_TEXT_BUFFER(mainw->layout_textbuffer), NULL, &end_iter, TRUE));
  }

  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    if (!(*name)) name2 = (_("(blank)"));
    else name2 = lives_strdup(name);
    text = lives_strdup_printf
           (_("The set name has been changed from %s to %s. Affected layouts have been updated accordingly\n"),
            name2, (char *)user_data);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(name2);
    lives_free(text);
    break;
  case LMAP_ERROR_MISSING_CLIP:
    if (prefs->warning_mask & WARN_MASK_LAYOUT_MISSING_CLIPS) return FALSE;
    text = lives_strdup_printf(_("The clip %s is missing from this set.\nIt is required by the following layouts:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
  case LMAP_ERROR_CLOSE_FILE:
    text = lives_strdup_printf(_("The clip %s has been closed.\nIt is required by the following layouts:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
    text = lives_strdup_printf(_("Frames have been shifted in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_DELETE_FRAMES:
    text = lives_strdup_printf(_("Frames have been deleted from the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_DELETE_AUDIO:
    text = lives_strdup_printf(_("Audio has been deleted from the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
    text = lives_strdup_printf(_("Audio has been shifted in clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_ALTER_AUDIO:
    text = lives_strdup_printf(_("Audio has been altered in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_ALTER_FRAMES:
    text = lives_strdup_printf(_("Frames have been altered in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  }

  if (affects_current && user_data) {
    mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                   (livespointer)lives_text_buffer_create_mark
                                   (LIVES_TEXT_BUFFER(mainw->layout_textbuffer), NULL, &end_iter, TRUE));
  }

  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    lmap = mainw->current_layouts_map;
    while (lmap) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      text = lives_strdup_printf("%s\n", array[0]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      // we could list all affected layouts, which could potentially be a lot !
      //mainw->affected_layouts_map=lives_list_append_unique(mainw->affected_layouts_map,array[0]);
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_MISSING_CLIP:
  case LMAP_ERROR_CLOSE_FILE:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map,
                                    mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));

    }
    lmap = (LiVESList *)user_data;
    while (lmap) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      text = lives_strdup_printf("%s\n", array[0]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, array[0]);
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
  case LMAP_ERROR_DELETE_FRAMES:
  case LMAP_ERROR_ALTER_FRAMES:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map,
                                    mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));
    }
    lmap = (LiVESList *)user_data;
    while (lmap) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      orig_fps = strtod(array[3], NULL);
      resampled_frame = count_resampled_frames(frameno, orig_fps, mainw->files[clipno]->fps);
      if (resampled_frame <= atoi(array[2])) {
        text = lives_strdup_printf("%s\n", array[0]);
        lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
        lives_free(text);
        mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, array[0]);
      }
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
  case LMAP_ERROR_DELETE_AUDIO:
  case LMAP_ERROR_ALTER_AUDIO:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map,
                                    mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));
    }
    lmap = (LiVESList *)user_data;
    while (lmap) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      max_time = strtod(array[4], NULL);
      if (max_time > 0. && atime <= max_time) {
        text = lives_strdup_printf("%s\n", array[0]);
        lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
        lives_free(text);
        mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, array[0]);
      }
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  }

  lives_widget_set_sensitive(mainw->show_layout_errors, TRUE);
  if (mainw->multitrack) lives_widget_set_sensitive(mainw->multitrack->show_layout_errors, TRUE);
  return TRUE;
}


void clear_lmap_errors(void) {
  LiVESTextIter start_iter, end_iter;
  LiVESList *lmap;

  lives_text_buffer_get_start_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &start_iter);
  lives_text_buffer_get_end_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter);
  lives_text_buffer_delete(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &start_iter, &end_iter);

  lmap = mainw->affected_layouts_map;

  while (lmap) {
    lives_free((livespointer)lmap->data);
    lmap = lmap->next;
  }
  lives_list_free(lmap);

  mainw->affected_layouts_map = NULL;
  lives_widget_set_sensitive(mainw->show_layout_errors, FALSE);
  if (mainw->multitrack) lives_widget_set_sensitive(mainw->multitrack->show_layout_errors, FALSE);

  if (mainw->affected_layout_marks) {
    remove_current_from_affected_layouts(mainw->multitrack);
  }
}

/**
   @brief check for set lock file
   do this via the back-end (smogrify)
   this allows for the locking scheme to be more flexible

   smogrify indicates a lock very simply by by writing > 0 bytes to stdout
   we read this via popen

   type == 0 for load, type == 1 for save

*/
boolean check_for_lock_file(const char *set_name, int type) {
  char *com;

  if (type == 1 && !lives_strcmp(set_name, mainw->set_name)) return TRUE;

  com = lives_strdup_printf("%s check_for_lock \"%s\" \"%s\" %d", prefs->backend_sync, set_name, capable->myname,
                            capable->mainpid);

  clear_mainw_msg();

  threaded_dialog_spin(0.);
  lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
  threaded_dialog_spin(0.);
  lives_free(com);

  if (THREADVAR(com_failed)) return FALSE;

  if (*(mainw->msg)) {
    if (type == 0) {
      if (mainw->recovering_files) return do_set_locked_warning(set_name);
      threaded_dialog_spin(0.);
      widget_opts.non_modal = TRUE;
      do_error_dialogf(_("Set %s\ncannot be opened, as it is in use\nby another copy of LiVES.\n"), set_name);
      widget_opts.non_modal = FALSE;
      threaded_dialog_spin(0.);
    } else if (type == 1) {
      if (!mainw->osc_auto) do_error_dialogf(_("\nThe set %s is currently in use by another copy of LiVES.\n"
                                               "Please choose another set name.\n"), set_name);
    }
    return FALSE;
  }
  return TRUE;
}


boolean do_std_checks(const char *type_name, const char *type, size_t maxlen, const char *nreject) {
  char *xtype = lives_strdup(type), *msg;
  const char *reject = " /\\*\"";
  size_t slen = strlen(type_name);

  if (nreject) reject = nreject;

  if (slen == 0) {
    msg = lives_strdup_printf(_("\n%s names may not be blank.\n"), xtype);
    if (!mainw->osc_auto) do_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  if (slen > MAX_SET_NAME_LEN) {
    msg = lives_strdup_printf(_("\n%s names may not be longer than %d characters.\n"), xtype, (int)maxlen);
    if (!mainw->osc_auto) do_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  if (strcspn(type_name, reject) != slen) {
    msg = lives_strdup_printf(_("\n%s names may not contain spaces or the characters%s.\n"), xtype, reject);
    if (!mainw->osc_auto) do_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  for (int i = 0; i < slen; i++) {
    if (type_name[i] == '.' && (i == 0 || type_name[i - 1] == '.')) {
      msg = lives_strdup_printf(_("\n%s names may not start with a '.' or contain '..'\n"), xtype);
      if (!mainw->osc_auto) do_error_dialog(msg);
      lives_free(msg);
      lives_free(xtype);
      return FALSE;
    }
  }

  lives_free(xtype);
  return TRUE;
}


boolean is_legal_set_name(const char *set_name, boolean allow_dupes, boolean leeway) {
  // check (clip) set names for validity
  // - may not be of zero length
  // - may not contain spaces or characters / \ * "
  // - must NEVER be name of a set in use by another copy of LiVES (i.e. with a lock file)

  // - as of 1.6.0:
  // -  may not start with a .
  // -  may not contain ..

  // - as of 3.2.0
  //   - must start with a letter [a - z] or [A - Z]

  // should be in FILESYSTEM encoding

  // may not be longer than MAX_SET_NAME_LEN chars

  // iff allow_dupes is FALSE then we disallow the name of any existing set (has a subdirectory in the working directory)

  if (!do_std_checks(set_name, _("Set"), MAX_SET_NAME_LEN, NULL)) return FALSE;

  // check if this is a set in use by another copy of LiVES
  if (mainw && mainw->is_ready && !check_for_lock_file(set_name, 1)) return FALSE;

  if ((set_name[0] < 'a' || set_name[0] > 'z') && (set_name[0] < 'A' || set_name[0] > 'Z')) {
    if (leeway) {
      if (mainw->is_ready)
        do_warning_dialog(_("As of LiVES 3.2.0 all set names must begin with alphabetical character\n"
                            "(A - Z or a - z)\nYou will need to give a new name for the set when saving it.\n"));
    } else {
      do_error_dialog(_("All set names must begin with an alphabetical character\n(A - Z or a - z)\n"));
      return FALSE;
    }
  }

  if (!allow_dupes) {
    // check for duplicate set names
    char *set_dir = lives_build_filename(prefs->workdir, set_name, NULL);
    if (lives_file_test(set_dir, LIVES_FILE_TEST_IS_DIR)) {
      lives_free(set_dir);
      return do_yesno_dialogf(_("\nThe set %s already exists.\n"
                                "Do you want to add the current clips to the existing set ?.\n"), set_name);
    }
    lives_free(set_dir);
  }

  return TRUE;
}


LIVES_GLOBAL_INLINE const char *get_image_ext_for_type(lives_img_type_t imgtype) {
  switch (imgtype) {
  case IMG_TYPE_JPEG: return LIVES_FILE_EXT_JPG; // "jpg"
  case IMG_TYPE_PNG: return LIVES_FILE_EXT_PNG; // "png"
  default: return "";
  }
}


LIVES_GLOBAL_INLINE lives_img_type_t lives_image_ext_to_img_type(const char *img_ext) {
  return lives_image_type_to_img_type(image_ext_to_lives_image_type(img_ext));
}


LIVES_GLOBAL_INLINE const char *image_ext_to_lives_image_type(const char *img_ext) {
  if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) return LIVES_IMAGE_TYPE_PNG;
  if (!strcmp(img_ext, LIVES_FILE_EXT_JPG)) return LIVES_IMAGE_TYPE_JPEG;
  return LIVES_IMAGE_TYPE_UNKNOWN;
}


LIVES_GLOBAL_INLINE lives_img_type_t lives_image_type_to_img_type(const char *lives_img_type) {
  if (!strcmp(lives_img_type, LIVES_IMAGE_TYPE_PNG)) return IMG_TYPE_PNG;
  if (!strcmp(lives_img_type, LIVES_IMAGE_TYPE_JPEG)) return IMG_TYPE_JPEG;
  return IMG_TYPE_UNKNOWN;
}


LIVES_GLOBAL_INLINE char *make_image_file_name(lives_clip_t *sfile, frames_t frame,
    const char *img_ext) {
  char *fname, *ret;
  if (!*img_ext) {
    sfile->img_type = resolve_img_type(sfile);
    img_ext = get_image_ext_for_type(sfile->img_type);
  }
  fname = lives_strdup_printf("%08d.%s", frame, img_ext);
  ret = lives_build_filename(prefs->workdir, sfile->handle, fname, NULL);
  lives_free(fname);
  return ret;
}


/** @brief check number of frames is correct
  for files of type CLIP_TYPE_DISK
  - check the image files (e.g. jpeg or png)

  use a "goldilocks" algorithm (just the right frames, not too few and not too many)

  ignores gaps */
boolean check_frame_count(int idx, boolean last_checked) {
  /// make sure nth frame is there...
  char *frame;
  if (mainw->files[idx]->frames > 0) {
    frame = make_image_file_name(mainw->files[idx], mainw->files[idx]->frames,
                                 get_image_ext_for_type(mainw->files[idx]->img_type));
    if (!lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) {
      // not enough frames
      lives_free(frame);
      return FALSE;
    }
    lives_free(frame);
  }

  /// ...make sure n + 1 th frame is not
  frame = make_image_file_name(mainw->files[idx], mainw->files[idx]->frames + 1,
                               get_image_ext_for_type(mainw->files[idx]->img_type));

  if (lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) {
    /// too many frames
    lives_free(frame);
    return FALSE;
  }
  lives_free(frame);

  /// just right
  return TRUE;
}


/** @brief sets mainw->files[idx]->frames with current framecount

   calls smogrify which physically finds the last frame using a (fast) O(log n) binary search method
   for CLIP_TYPE_DISK only
   (CLIP_TYPE_FILE should use the decoder plugin frame count) */
int get_frame_count(int idx, int start) {
  ssize_t bytes;
  char *com = lives_strdup_printf("%s count_frames \"%s\" %s %d", prefs->backend_sync, mainw->files[idx]->handle,
                                  get_image_ext_for_type(mainw->files[idx]->img_type), start);

  bytes = lives_popen(com, FALSE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);

  if (THREADVAR(com_failed)) return 0;

  if (bytes > 0) return atoi(mainw->msg);
  return 0;
}


boolean get_frames_sizes(int fileno, int frame, int *hsize, int *vsize) {
  // get the actual physical frame size
  lives_clip_t *sfile = mainw->files[fileno];
  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  char *fname = make_image_file_name(sfile, frame, get_image_ext_for_type(sfile->img_type));
  weed_set_int_value(layer, WEED_LEAF_HOST_FLAGS, LIVES_LAYER_GET_SIZE_ONLY);
  if (!weed_layer_create_from_file_progressive(layer, fname, 0, 0, WEED_PALETTE_END,
      get_image_ext_for_type(sfile->img_type))) {
    lives_free(fname);
    return FALSE;
  }
  lives_free(fname);
  *hsize = weed_layer_get_width(layer);
  *vsize = weed_layer_get_height(layer);
  weed_layer_free(layer);
  return FALSE;
}


boolean lives_string_ends_with(const char *string, const char *fmt, ...) {
  char *textx;
  va_list xargs;
  size_t slen, cklen;
  boolean ret = FALSE;

  if (!string) return FALSE;

  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  if (!textx) return FALSE;
  slen = lives_strlen(string);
  cklen = lives_strlen(textx);
  if (cklen == 0 || cklen > slen) {
    lives_free(textx);
    return FALSE;
  }
  if (!lives_strncmp(string + slen - cklen, textx, cklen)) ret = TRUE;
  lives_free(textx);
  return ret;
}


void get_dirname(char *filename) {
  char *tmp;
  // get directory name from a file
  // filename should point to char[PATH_MAX]
  // WARNING: will change contents of filename

  lives_snprintf(filename, PATH_MAX, "%s%s", (tmp = lives_path_get_dirname(filename)), LIVES_DIR_SEP);
  if (!strcmp(tmp, ".")) {
    char *tmp1 = lives_get_current_dir(), *tmp2 = lives_build_filename(tmp1, filename + 2, NULL);
    lives_free(tmp1);
    lives_snprintf(filename, PATH_MAX, "%s", tmp2);
    lives_free(tmp2);
  }

  lives_free(tmp);
}


char *get_dir(const char *filename) {
  // get directory as string, should free after use
  char tmp[PATH_MAX];
  lives_snprintf(tmp, PATH_MAX, "%s", filename);
  get_dirname(tmp);
  return lives_strdup(tmp);
}


LIVES_GLOBAL_INLINE void get_basename(char *filename) {
  // get basename from a file
  // (filename without directory)
  // filename should point to char[PATH_MAX]
  // WARNING: will change contents of filename
  char *tmp = lives_path_get_basename(filename);
  lives_snprintf(filename, PATH_MAX, "%s", tmp);
  lives_free(tmp);
}


LIVES_GLOBAL_INLINE void get_filename(char *filename, boolean strip_dir) {
  // get filename (part without extension) of a file
  //filename should point to char[PATH_MAX]
  // WARNING: will change contents of filename
  if (strip_dir) get_basename(filename);
  lives_strstop(filename, '.');
}

/// return filename (no dir, no .ext)
LIVES_GLOBAL_INLINE char *lives_get_filename(char *uri) {return lives_strstop(lives_path_get_basename(uri), '.');}


char *get_extension(const char *filename) {
  // return file extension without the "."
  char *tmp = lives_path_get_basename(filename);
  char *ptr = strrchr(tmp, '.');
  if (!ptr) {
    lives_free(tmp);
    return lives_strdup("");
  } else {
    char *ret = lives_strdup(ptr + 1);
    lives_free(tmp);
    return ret;
  }
}


char *ensure_extension(const char *fname, const char *ext) {
  // make sure filename fname has file extension ext
  // if ext does not begin with a "." we prepend one to the start of ext
  // we then check if fname ends with ext. If not we append ext to fname.
  // we return a copy of fname, possibly modified. The string returned should be freed after use.
  // NOTE: the original ext is not changed.

  size_t se = strlen(ext), sf;
  char *eptr = (char *)ext;

  if (!fname) return NULL;

  if (se == 0) return lives_strdup(fname);

  if (eptr[0] == '.') {
    eptr++;
    se--;
  }

  sf = lives_strlen(fname);
  if (sf < se + 1 || strcmp(fname + sf - se, eptr) || fname[sf - se - 1] != '.') {
    return lives_strconcat(fname, ".", eptr, NULL);
  }

  return lives_strdup(fname);
}


// input length includes terminating NUL

LIVES_GLOBAL_INLINE char *lives_ellipsize(char *txt, size_t maxlen, LiVESEllipsizeMode mode) {
  /// eg. txt = "abcdefgh", maxlen = 6, LIVES_ELLIPSIZE_END  -> txt == "...gh" + NUL
  ///     txt = "abcdefgh", maxlen = 6, LIVES_ELLIPSIZE_START  -> txt == "ab..." + NUL
  ///     txt = "abcdefgh", maxlen = 6, LIVES_ELLIPSIZE_MIDDLE  -> txt == "a...h" + NUL
  // LIVES_ELLIPSIZE_NONE - do not ellipsise
  // return value should be freed, unless txt is returned
  const char ellipsis[4] = "...\0";
  size_t slen = lives_strlen(txt);
  off_t stlen, enlen;
  char *retval = txt;
  if (!maxlen) return NULL;
  if (slen >= maxlen) {
    if (maxlen == 1) return lives_strdup("");
    retval = (char *)lives_malloc(maxlen);
    if (maxlen == 2) return lives_strdup(".");
    if (maxlen == 3) return lives_strdup("..");
    if (maxlen == 4) return lives_strdup("...");
    maxlen -= 4;
    switch (mode) {
    case LIVES_ELLIPSIZE_END:
      lives_memcpy(retval, ellipsis, 3);
      lives_memcpy(retval + 3, txt + slen - maxlen, maxlen + 1);
      break;
    case LIVES_ELLIPSIZE_START:
      lives_memcpy(retval, txt, maxlen);
      lives_memcpy(retval + maxlen, ellipsis, 4);
      break;
    case LIVES_ELLIPSIZE_MIDDLE:
      enlen = maxlen >> 1;
      stlen = maxlen - enlen;
      lives_memcpy(retval, txt, stlen);
      lives_memcpy(retval + stlen, ellipsis, 3);
      lives_memcpy(retval + stlen + 3, txt + slen - enlen, enlen + 1);
      break;
    default: break;
    }
  }
  return retval;
}


LIVES_GLOBAL_INLINE char *lives_pad(char *txt, size_t minlen, int align) {
  // pad with spaces at start and end respectively
  // ealign gives ellipsis pos, palign can be LIVES_ALIGN_START, LIVES_ALIGN_END
  // LIVES_ALIGN_START -> pad end, LIVES_ALIGN_END -> pad start
  // LIVES_ALIGN_CENTER -> pad on both sides
  // LIVES_ALIGN_FILL - do not pad
  size_t slen = lives_strlen(txt);
  char *retval = txt;
  off_t ipos = 0;
  if (align == LIVES_ALIGN_FILL) return txt;
  if (slen < minlen - 1) {
    retval = (char *)lives_malloc(minlen);
    lives_memset(retval, ' ', --minlen);
    retval[minlen] = 0;
    switch (align) {
    case LIVES_ALIGN_END:
      ipos = minlen - slen;
      break;
    case LIVES_ALIGN_CENTER:
      ipos = minlen - slen;
      break;
    default:
      break;
    }
    lives_memcpy(retval + ipos, txt, slen);
  }
  return retval;
}


LIVES_GLOBAL_INLINE char *lives_pad_ellipsize(char *txt, size_t fixlen, int palign,  LiVESEllipsizeMode emode) {
  // if len of txt < fixlen it will be padded, if longer, ellipsised
  // ealign gives ellipsis pos, palign can be LIVES_ALIGN_START, LIVES_ALIGN_END
  // pad with spaces at start and end respectively
  // LIVES_ALIGN_CENTER -> pad on both sides
  // LIVES_ALIGN_FILL - do not pad
  size_t slen = lives_strlen(txt);
  if (slen == fixlen - 1) return txt;
  if (slen >= fixlen) return lives_ellipsize(txt, fixlen, emode);
  return lives_pad(txt, fixlen, palign);
}


boolean ensure_isdir(char *fname) {
  // ensure dirname ends in a single dir separator
  // fname should be char[PATH_MAX]

  // returns TRUE if fname was altered

  size_t tlen = lives_strlen(fname), slen, tlen2;
  size_t dslen = strlen(LIVES_DIR_SEP);
  ssize_t offs;
  boolean ret = FALSE;
  char *tmp = lives_strdup(fname), *tmp2;

  while (1) {
    // recursively remove double DIR_SEP
    tmp2 = subst(tmp, LIVES_DIR_SEP LIVES_DIR_SEP, LIVES_DIR_SEP);
    if ((tlen2 = lives_strlen(tmp2)) < tlen) {
      ret = TRUE;
      lives_free(tmp);
      tmp = tmp2;
      tlen = tlen2;
    } else {
      lives_free(tmp2);
      break;
    }
  }

  if (ret) lives_snprintf(fname, PATH_MAX, "%s", tmp);
  lives_free(tmp);

  slen = tlen - 1;
  offs = slen;

  // we should now only have one or zero DIR_SEP at the end, but just in case we remove all but the last one
  while (offs >= 0 && !strncmp(fname + offs, LIVES_DIR_SEP, dslen)) offs -= dslen;
  if (offs == slen - dslen) return ret; // format is OK as-is

  // strip off all terminating DIR_SEP and then append one
  if (++offs < 0) offs = 0;
  if (offs < slen) fname[offs] = 0;
  fname = strncat(fname, LIVES_DIR_SEP, PATH_MAX - offs - 1);
  return TRUE;
}


boolean dirs_equal(const char *dira, const char *dirb) {
  // filenames in locale encoding
  char *tmp;
  char dir1[PATH_MAX];
  char dir2[PATH_MAX];
  lives_snprintf(dir1, PATH_MAX, "%s", (tmp = F2U8(dira)));
  lives_free(tmp);
  lives_snprintf(dir2, PATH_MAX, "%s", (tmp = F2U8(dirb)));
  lives_free(tmp);
  ensure_isdir(dir1);
  ensure_isdir(dir2);
  // TODO: for some (Linux) fstypes we should use strcasecmp
  // can get this using "df -T"
  return (!lives_strcmp(dir1, dir2));
}


void get_location(const char *exe, char *val, int maxlen) {
  // find location of "exe" in path
  // sets it in val which is a char array of maxlen bytes

  char *loc;
  if ((loc = lives_find_program_in_path(exe)) != NULL) {
    lives_snprintf(val, maxlen, "%s", loc);
    lives_free(loc);
  } else {
    lives_memset(val, 0, 1);
  }
}


LIVES_LOCAL_INLINE lives_presence_t has_executable(const char *exe) {
  char *loc;
  if ((loc = lives_find_program_in_path(exe)) != NULL) {
    lives_free(loc);
    return PRESENT;
  }
  // for now we don't return MISSING (requires code update to differentiate MISSING / UNCHECKED / PRESENT)
  return FALSE;
}


// check if executable is present, missing or unchecked
// if unchecked, check for it, and if not found ask the user politely to install it
boolean check_for_executable(lives_checkstatus_t *cap, const char *exec) {
#ifdef NEW_CHECKSTATUS
  if (!cap || (*cap)->present == UNCHECKED) {
    if (!cap || ((*cap)->flags & INSTALL_CANLOCAL)) {
      /// TODO (next version)
#else
  if (!cap || *cap == UNCHECKED) {
    if (!lives_strcmp(exec, EXEC_YOUTUBE_DL)) {
#endif
      char *localv = lives_build_filename(capable->home_dir, LOCAL_HOME_DIR, "bin", exec, NULL);
      if (lives_file_test(localv, LIVES_FILE_TEST_IS_EXECUTABLE)) {
        lives_free(localv);
        if (cap) *cap = LOCAL;
        return TRUE;
      }
      lives_free(localv);
    }
    if (has_executable(exec)) {
      if (cap) *cap = PRESENT;
      return TRUE;
    } else {
      if (!lives_strcmp(exec, EXEC_XDOTOOL) || !lives_strcmp(exec, EXEC_WMCTRL)) {
        if (cap) *cap = MISSING;
      }
      //if (importance == necessary)
      //do_please_install(exec);
#ifdef HAS_MISSING_PRESENCE
      if (cap) *cap = MISSING;
#endif
      //do_program_not_found_error(exec);
      return FALSE;
    }
  }
#if 0
}
}
#endif
return (*cap == PRESENT || *cap == LOCAL);
}


uint64_t get_version_hash(const char *exe, const char *sep, int piece) {
  /// get version hash output for an executable from the backend
  uint64_t val;
  char buff[128];
  char **array;
  int ntok;

  lives_popen(exe, TRUE, buff, 128);
  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return -2;
  }
  ntok = get_token_count(buff, sep[0]);
  if (ntok < piece) return -1;
  array = lives_strsplit(buff, sep, ntok);
  val = make_version_hash(array[piece]);
  lives_strfreev(array);
  return val;
}


#define VER_MAJOR_MULT 1000000
#define VER_MINOR_MULT 1000
#define VER_MICRO_MULT 1

uint64_t make_version_hash(const char *ver) {
  /// convert a version to uint64_t hash, for comparing
  char **array;
  uint64_t hash;
  int ntok;

  if (!ver) return 0;

  ntok = get_token_count((char *)ver, '.');
  array = lives_strsplit(ver, ".", ntok);

  hash = atoi(array[0]) * VER_MAJOR_MULT;
  if (ntok > 1) {
    hash += atoi(array[1]) * VER_MINOR_MULT;
    if (ntok > 2) hash += atoi(array[2]) * VER_MICRO_MULT;
  }

  lives_strfreev(array);
  return hash;
}


char *unhash_version(uint64_t version) {
  if (!version) return lives_strdup(_("'Unknown'"));
  else {
    uint64_t maj = version / VER_MAJOR_MULT, min;
    version -= maj * VER_MAJOR_MULT;
    min = version / VER_MINOR_MULT;
    version -= min * VER_MINOR_MULT;
    return lives_strdup_printf("%lu.%lu.%lu", maj, min, version);
  }
}


char *repl_workdir(const char *entry, boolean fwd) {
  // replace prefs->workdir with string workdir or vice-versa. This allows us to relocate workdir if necessary.
  // used for layout.map file
  // return value should be freed

  // fwd TRUE replaces "/tmp/foo" with "workdir"
  // fwd FALSE replaces "workdir" with "/tmp/foo"
  size_t wdl;
  char *string = lives_strdup(entry);

  if (fwd) {
    if (!lives_strncmp(entry, prefs->workdir, (wdl = lives_strlen(prefs->workdir)))) {
      lives_free(string);
      string = lives_strdup_printf("%s%s", WORKDIR_LITERAL, entry + wdl);
    }
  } else {
    if (!lives_strncmp(entry, WORKDIR_LITERAL, WORKDIR_LITERAL_LEN)) {
      lives_free(string);
      string = lives_build_filename(prefs->workdir, entry + WORKDIR_LITERAL_LEN, NULL);
    }
  }
  return string;
}


void remove_layout_files(LiVESList * map) {
  // removes a LiVESList of layouts from the set layout map

  // removes from: - global layouts map
  //               - disk
  //               - clip layout maps

  // called after, for example: a clip is removed or altered and the user opts to remove all associated layouts

  LiVESList *lmap, *lmap_next, *cmap, *cmap_next, *map_next;
  size_t maplen;
  char **array;
  char *fname, *fdir;
  boolean is_current;

  while (map) {
    map_next = map->next;
    if (map->data) {
      if (!lives_utf8_strcasecmp((char *)map->data, mainw->string_constants[LIVES_STRING_CONSTANT_CL])) {
        is_current = TRUE;
        fname = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      } else {
        is_current = FALSE;
        maplen = lives_strlen((char *)map->data);

        // remove from mainw->current_layouts_map
        cmap = mainw->current_layouts_map;
        while (cmap) {
          cmap_next = cmap->next;
          if (!lives_utf8_strcasecmp((char *)cmap->data, (char *)map->data)) {
            lives_free((livespointer)cmap->data);
            mainw->current_layouts_map = lives_list_delete_link(mainw->current_layouts_map, cmap);
            break;
          }
          cmap = cmap_next;
        }

        array = lives_strsplit((char *)map->data, "|", -1);
        fname = repl_workdir(array[0], FALSE);
        lives_strfreev(array);
      }

      // fname should now hold the layout name on disk
      d_print(_("Removing layout %s\n"), fname);

      if (!is_current) {
        lives_rm(fname);

        // if no more layouts in parent dir, we can delete dir

        // ensure that parent dir is below our own working dir
        if (!lives_strncmp(fname, prefs->workdir, lives_strlen(prefs->workdir))) {
          // is in workdir, safe to remove parents

          char *protect_file = lives_build_filename(prefs->workdir, "noremove", NULL);

          // touch a file in tpmdir, so we cannot remove workdir itself
          lives_touch(protect_file);

          if (!THREADVAR(com_failed)) {
            // ok, the "touch" worked
            // now we call rmdir -p : remove directory + any empty parents
            fdir = lives_path_get_dirname(fname);
            lives_rmdir_with_parents(fdir);
            lives_free(fdir);
          }

          // remove the file we touched to clean up
          lives_rm(protect_file);
          lives_free(protect_file);
        }

        // remove from mainw->files[]->layout_map
        for (int i = 1; i <= MAX_FILES; i++) {
          if (mainw->files[i]) {
            if (mainw->files[i]->layout_map) {
              lmap = mainw->files[i]->layout_map;
              while (lmap) {
                lmap_next = lmap->next;
                if (!lives_strncmp((char *)lmap->data, (char *)map->data, maplen)) {
                  lives_free((livespointer)lmap->data);
                  mainw->files[i]->layout_map = lives_list_delete_link(mainw->files[i]->layout_map, lmap);
                }
                lmap = lmap_next;
		// *INDENT-OFF*
              }}}}
	// *INDENT-ON*

      } else {
        // asked to remove the currently loaded layout

        if (mainw->stored_event_list || mainw->sl_undo_mem) {
          // we are in CE mode, so event_list is in storage
          stored_event_list_free_all(TRUE);
        }
        // in mt mode we need to do more
        else remove_current_from_affected_layouts(mainw->multitrack);

        // and we dont want to try reloading this next time
        prefs->ar_layout = FALSE;
        set_string_pref(PREF_AR_LAYOUT, "");
        lives_memset(prefs->ar_layout_name, 0, 1);
      }
      lives_free(fname);
    }
    map = map_next;
  }

  // save updated layout.map
  save_layout_map(NULL, NULL, NULL, NULL);
}


LIVES_GLOBAL_INLINE void get_play_times(void) {
  update_timer_bars(0, 0, 0, 0, 0);
}


void update_play_times(void) {
  // force a redraw, reread audio
  if (!CURRENT_CLIP_IS_VALID) return;
  if (cfile->audio_waveform) {
    int i;
    for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
    lives_freep((void **)&cfile->audio_waveform);
    lives_freep((void **)&cfile->aw_sizes);
  }
  get_play_times();
}


void get_total_time(lives_clip_t *file) {
  // get times (video, left and right audio)

  file->laudio_time = file->raudio_time = file->video_time = 0.;

  if (file->opening) {
    int frames;
    if (file->frames != 123456789) frames = file->frames;
    else frames = file->opening_frames;
    if (frames * file->fps > 0) {
      file->video_time = file->frames / file->fps;
    }
    return;
  }

  if (file->fps > 0.) {
    file->video_time = file->frames / file->fps;
  }

  if (file->asampsize >= 8 && file->arate > 0 && file->achans > 0) {
    file->laudio_time = (double)(file->afilesize / (file->asampsize >> 3) / file->achans) / (double)file->arate;
    if (file->achans > 1) {
      file->raudio_time = file->laudio_time;
    }
  }

  if (file->laudio_time + file->raudio_time == 0. && !file->opening) {
    file->achans = file->afilesize = file->asampsize = file->arate = file->arps = 0;
  }
}


void find_when_to_stop(void) {
  // work out when to stop playing
  //
  // ---------------
  //        no loop              loop to fit                 loop cont
  //        -------              -----------                 ---------
  // a>v    stop on video end    stop on audio end           no stop
  // v>a    stop on video end    stop on video end           no stop
  // generator start - not playing : stop on vid_end, unless pure audio;
  if (mainw->alives_pgid > 0) mainw->whentostop = NEVER_STOP;
  else if (mainw->aud_rec_fd != -1 &&
           mainw->ascrap_file == -1) mainw->whentostop = STOP_ON_VID_END;
  else if (mainw->multitrack && CURRENT_CLIP_HAS_VIDEO) mainw->whentostop = STOP_ON_VID_END;
  else if (!CURRENT_CLIP_IS_NORMAL) {
    if (mainw->loop_cont) mainw->whentostop = NEVER_STOP;
    else mainw->whentostop = STOP_ON_VID_END;
  } else if (cfile->opening_only_audio) mainw->whentostop = STOP_ON_AUD_END;
  else if (cfile->opening_audio) mainw->whentostop = STOP_ON_VID_END;
  else if (!mainw->preview && (mainw->loop_cont)) mainw->whentostop = NEVER_STOP;
  else if (!CURRENT_CLIP_HAS_VIDEO || (mainw->loop && cfile->achans > 0 && !mainw->is_rendering
                                       && (mainw->audio_end / cfile->fps)
                                       < MAX(cfile->laudio_time, cfile->raudio_time) &&
                                       calc_time_from_frame(mainw->current_file, mainw->play_start) < cfile->laudio_time))
    mainw->whentostop = STOP_ON_AUD_END;
  else mainw->whentostop = STOP_ON_VID_END; // tada...
}


void minimise_aspect_delta(double aspect, int hblock, int vblock, int hsize, int vsize, int *width, int *height) {
  // we will use trigonometry to calculate the smallest difference between a given
  // aspect ratio and the actual frame size. If the delta is smaller than current
  // we set the height and width
  int cw = width[0];
  int ch = height[0];

  int real_width, real_height;
  uint64_t delta, current_delta;

  // minimise d[(x-x1)^2 + (y-y1)^2]/d[x1], to get approximate values
  int calc_width = (int)((vsize + aspect * hsize) * aspect / (aspect * aspect + 1.));

  int i;

  current_delta = (hsize - cw) * (hsize - cw) + (vsize - ch) * (vsize - ch);

#ifdef DEBUG_ASPECT
  lives_printerr("aspect %.8f : width %d height %d is best fit\n", aspect, calc_width, (int)(calc_width / aspect));
#endif
  // use the block size to find the nearest allowed size
  for (i = -1; i < 2; i++) {
    real_width = (int)(calc_width / hblock + i) * hblock;
    real_height = (int)(real_width / aspect / vblock + .5) * vblock;
    delta = (hsize - real_width) * (hsize - real_width) + (vsize - real_height) * (vsize - real_height);

    if (real_width % hblock != 0 || real_height % vblock != 0 ||
        ABS((double)real_width / (double)real_height - aspect) > ASPECT_ALLOWANCE) {
      // encoders can be fussy, so we need to fit both aspect ratio and blocksize
      while (1) {
        real_width = ((int)(real_width / hblock) + 1) * hblock;
        real_height = (int)((double)real_width / aspect + .5);

        if (real_height % vblock == 0) break;

        real_height = ((int)(real_height / vblock) + 1) * vblock;
        real_width = (int)((double)real_height * aspect + .5);

        if (real_width % hblock == 0) break;
      }
    }

#ifdef DEBUG_ASPECT
    lives_printerr("block quantise to %d x %d\n", real_width, real_height);
#endif
    if (delta < current_delta) {
#ifdef DEBUG_ASPECT
      lives_printerr("is better fit\n");
#endif
      current_delta = delta;
      width[0] = real_width;
      height[0] = real_height;
    }
  }
}


void zero_spinbuttons(void) {
  lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 0., 0.);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 0.);
  lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);
  lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end), 0., 0.);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), 0.);
  lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);
}


boolean switch_aud_to_jack(boolean set_in_prefs) {
#ifdef ENABLE_JACK
  if (mainw->is_ready) {
    if (!mainw->jack_inited) lives_jack_init();
    if (!mainw->jackd) {
      jack_audio_init();
      jack_audio_read_init();
      mainw->jackd = jack_get_driver(0, TRUE);
      if (!jack_create_client_writer(mainw->jackd)) {
        mainw->jackd = NULL;
        return FALSE;
      }
      mainw->jackd->whentostop = &mainw->whentostop;
      mainw->jackd->cancelled = &mainw->cancelled;
      mainw->jackd->in_use = FALSE;
      mainw->jackd->play_when_stopped = (prefs->jack_opts & JACK_OPTS_NOPLAY_WHEN_PAUSED) ? FALSE : TRUE;
      jack_write_driver_activate(mainw->jackd);
    }

    mainw->aplayer_broken = FALSE;
    lives_widget_show(mainw->vol_toolitem);
    if (mainw->vol_label) lives_widget_show(mainw->vol_label);
    lives_widget_show(mainw->recaudio_submenu);
    lives_widget_set_sensitive(mainw->vol_toolitem, TRUE);

    if (mainw->vpp && mainw->vpp->get_audio_fmts)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed_read) {
      pulse_close_client(mainw->pulsed_read);
      mainw->pulsed_read = NULL;
    }

    if (mainw->pulsed) {
      pulse_close_client(mainw->pulsed);
      mainw->pulsed = NULL;
      pulse_shutdown();
    }
#endif
  }
  prefs->audio_player = AUD_PLAYER_JACK;
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_JACK);

  if (mainw->is_ready && mainw->vpp && mainw->vpp->get_audio_fmts)
    mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

  if (prefs->perm_audio_reader && prefs->audio_src == AUDIO_SRC_EXT) {
    jack_rec_audio_to_clip(-1, -1, RECA_MONITOR);
    mainw->jackd_read->in_use = FALSE;
  }

  lives_widget_set_sensitive(mainw->int_audio_checkbutton, TRUE);
  lives_widget_set_sensitive(mainw->ext_audio_checkbutton, TRUE);
  lives_widget_set_sensitive(mainw->mute_audio, TRUE);
  lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
  lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);

  return TRUE;
#endif
  return FALSE;
}


boolean switch_aud_to_pulse(boolean set_in_prefs) {
#ifdef HAVE_PULSE_AUDIO
  boolean retval;

  if (mainw->is_ready) {
    if ((retval = lives_pulse_init(-1))) {
      if (!mainw->pulsed) {
        pulse_audio_init();
        pulse_audio_read_init();
        mainw->pulsed = pulse_get_driver(TRUE);
        mainw->pulsed->whentostop = &mainw->whentostop;
        mainw->pulsed->cancelled = &mainw->cancelled;
        mainw->pulsed->in_use = FALSE;
        pulse_driver_activate(mainw->pulsed);
      }
      mainw->aplayer_broken = FALSE;
      lives_widget_show(mainw->vol_toolitem);
      if (mainw->vol_label) lives_widget_show(mainw->vol_label);
      lives_widget_show(mainw->recaudio_submenu);
      lives_widget_set_sensitive(mainw->vol_toolitem, TRUE);

      prefs->audio_player = AUD_PLAYER_PULSE;
      if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
      lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_PULSE);

      if (mainw->vpp && mainw->vpp->get_audio_fmts)
        mainw->vpp->audio_codec = get_best_audio(mainw->vpp);
    }

#ifdef ENABLE_JACK
    if (mainw->jackd_read) {
      jack_close_device(mainw->jackd_read);
      mainw->jackd_read = NULL;
    }

    if (mainw->jackd) {
      jack_close_device(mainw->jackd);
      mainw->jackd = NULL;
    }
#endif

    if (prefs->perm_audio_reader && prefs->audio_src == AUDIO_SRC_EXT) {
      pulse_rec_audio_to_clip(-1, -1, RECA_MONITOR);
      mainw->pulsed_read->in_use = FALSE;
    }

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, TRUE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, TRUE);
    lives_widget_set_sensitive(mainw->mute_audio, TRUE);
    lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
    if (mainw->play_window)
      lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);

    return retval;
  }
#endif
  return FALSE;
}


boolean switch_aud_to_sox(boolean set_in_prefs) {
  if (!capable->has_sox_play) return FALSE; // TODO - show error

  prefs->audio_player = AUD_PLAYER_SOX;
  lives_snprintf(prefs->audio_play_command, 256, "%s", EXEC_PLAY);
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_SOX);
  //set_string_pref(PREF_AUDIO_PLAY_COMMAND, prefs->audio_play_command);

  if (mainw->is_ready) {
    /* //ubuntu / Unity has a hissy fit if you hide things in the menu !
      lives_widget_hide(mainw->vol_toolitem);
      if (mainw->vol_label) lives_widget_hide(mainw->vol_label);
    */
    lives_widget_set_sensitive(mainw->vol_toolitem, FALSE);
    lives_widget_hide(mainw->recaudio_submenu);

    if (mainw->vpp && mainw->vpp->get_audio_fmts)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

    pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, TRUE);

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->mute_audio, TRUE);
    lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
    lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read) {
    jack_close_device(mainw->jackd_read);
    mainw->jackd_read = NULL;
  }

  if (mainw->jackd) {
    jack_close_device(mainw->jackd);
    mainw->jackd = NULL;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read) {
    pulse_close_client(mainw->pulsed_read);
    mainw->pulsed_read = NULL;
  }

  if (mainw->pulsed) {
    pulse_close_client(mainw->pulsed);
    mainw->pulsed = NULL;
    pulse_shutdown();
  }
#endif
  return TRUE;
}


void switch_aud_to_none(boolean set_in_prefs) {
  prefs->audio_player = AUD_PLAYER_NONE;
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_NONE);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_NONE);

  if (mainw->is_ready) {
    /* //ubuntu has a hissy fit if you hide things in the menu
      lives_widget_hide(mainw->vol_toolitem);
      if (mainw->vol_label) lives_widget_hide(mainw->vol_label);
    */
    lives_widget_set_sensitive(mainw->vol_toolitem, FALSE);
    // lives_widget_hide(mainw->recaudio_submenu);

    if (mainw->vpp && mainw->vpp->get_audio_fmts)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

    pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, TRUE);

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->mute_audio, FALSE);
    lives_widget_set_sensitive(mainw->m_mutebutton, FALSE);
    if (mainw->preview_box) {
      lives_widget_set_sensitive(mainw->p_mutebutton, FALSE);
    }
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read) {
    jack_close_device(mainw->jackd_read);
    mainw->jackd_read = NULL;
  }

  if (mainw->jackd) {
    jack_close_device(mainw->jackd);
    mainw->jackd = NULL;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read) {
    pulse_close_client(mainw->pulsed_read);
    mainw->pulsed_read = NULL;
  }

  if (mainw->pulsed) {
    pulse_close_client(mainw->pulsed);
    mainw->pulsed = NULL;
    pulse_shutdown();
  }
#endif
}


boolean prepare_to_play_foreign(void) {
  // here we are going to 'play' a captured external window

#ifdef GUI_GTK

#if !GTK_CHECK_VERSION(3, 0, 0)
#ifdef GDK_WINDOWING_X11
  GdkVisual *vissi = NULL;
  register int i;
#endif
#endif
#endif

  int new_file = mainw->first_free_file;

  mainw->foreign_window = NULL;

  // create a new 'file' to play into
  if (!get_new_handle(new_file, NULL)) {
    return FALSE;
  }

  mainw->current_file = new_file;

  if (mainw->rec_achans > 0) {
    cfile->arate = cfile->arps = mainw->rec_arate;
    cfile->achans = mainw->rec_achans;
    cfile->asampsize = mainw->rec_asamps;
    cfile->signed_endian = mainw->rec_signed_endian;
#ifdef HAVE_PULSE_AUDIO
    if (mainw->rec_achans > 0 && prefs->audio_player == AUD_PLAYER_PULSE) {
      pulse_rec_audio_to_clip(mainw->current_file, -1, RECA_WINDOW_GRAB);
      mainw->pulsed_read->in_use = TRUE;
    }
#endif
#ifdef ENABLE_JACK
    if (mainw->rec_achans > 0 && prefs->audio_player == AUD_PLAYER_JACK) {
      jack_rec_audio_to_clip(mainw->current_file, -1, RECA_WINDOW_GRAB);
      mainw->jackd_read->in_use = TRUE;
    }
#endif
  }

  cfile->hsize = mainw->foreign_width / 2 + 1;
  cfile->vsize = mainw->foreign_height / 2 + 3;

  cfile->fps = cfile->pb_fps = mainw->rec_fps;

  resize(-2);

  lives_widget_show(mainw->playframe);
  lives_widget_show(mainw->playarea);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  lives_widget_set_opacity(mainw->playframe, 1.);

  // size must be exact, must not be larger than play window or we end up with nothing
  mainw->pwidth = lives_widget_get_allocation_width(mainw->playframe);// - H_RESIZE_ADJUST + 2;
  mainw->pheight = lives_widget_get_allocation_height(mainw->playframe);// - V_RESIZE_ADJUST + 2;

  cfile->hsize = mainw->pwidth;
  cfile->vsize = mainw->pheight;

  cfile->img_type = IMG_TYPE_BEST; // override the pref

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)

#ifdef GDK_WINDOWING_X11
  mainw->foreign_window = gdk_x11_window_foreign_new_for_display
                          (mainw->mgeom[widget_opts.monitor].disp,
                           mainw->foreign_id);
#else
#ifdef GDK_WINDOWING_WIN32
  if (!mainw->foreign_window)
    mainw->foreign_window = gdk_win32_window_foreign_new_for_display
                            (mainw->mgeom[widget_opts.monitor].disp,
                             mainw->foreign_id);
#endif

#endif // GDK_WINDOWING

  if (mainw->foreign_window) lives_xwindow_set_keep_above(mainw->foreign_window, TRUE);

#else // 3, 0, 0
  mainw->foreign_window = gdk_window_foreign_new(mainw->foreign_id);
#endif
#endif // GUI_GTK

#ifdef GUI_GTK
#ifdef GDK_WINDOWING_X11
#if !GTK_CHECK_VERSION(3, 0, 0)

  if (mainw->foreign_visual) {
    for (i = 0; i < capable->nmonitors; i++) {
      vissi = gdk_x11_screen_lookup_visual(mainw->mgeom[i].screen, hextodec(mainw->foreign_visual));
      if (vissi) break;
    }
  }

  if (!vissi) vissi = gdk_visual_get_best_with_depth(mainw->foreign_bpp);
  if (!vissi) return FALSE;

  mainw->foreign_cmap = gdk_x11_colormap_foreign_new(vissi,
                        gdk_x11_colormap_get_xcolormap(gdk_colormap_new(vissi, TRUE)));

  if (!mainw->foreign_cmap) return FALSE;

#endif
#endif
#endif

  if (!mainw->foreign_window) return FALSE;

  mainw->play_start = 1;
  if (mainw->rec_vid_frames == -1) mainw->play_end = INT_MAX;
  else mainw->play_end = mainw->rec_vid_frames;

  mainw->rec_samples = -1;

  omute = mainw->mute;
  osepwin = mainw->sep_win;
  ofs = mainw->fs;
  ofaded = mainw->faded;
  odouble = mainw->double_size;

  mainw->mute = TRUE;
  mainw->sep_win = FALSE;
  mainw->fs = FALSE;
  mainw->faded = TRUE;
  mainw->double_size = FALSE;

  lives_widget_hide(mainw->t_sepwin);
  lives_widget_hide(mainw->t_infobutton);

  return TRUE;
}


boolean after_foreign_play(void) {
  // read details from capture file
  int capture_fd = -1;
  char *capfile = lives_strdup_printf("%s/.capture.%d", prefs->workdir, capable->mainpid);
  char capbuf[256];
  ssize_t length;
  int new_frames = 0;
  int old_file = mainw->current_file;

  char **array;

  // assume for now we only get one clip passed back
  if ((capture_fd = lives_open2(capfile, O_RDONLY)) > -1) {
    lives_memset(capbuf, 0, 256);
    if ((length = read(capture_fd, capbuf, 256))) {
      if (get_token_count(capbuf, '|') > 2) {
        array = lives_strsplit(capbuf, "|", 3);
        new_frames = atoi(array[1]);
        if (new_frames > 0) {
          create_cfile(-1, array[0], FALSE);
          lives_strfreev(array);
          lives_snprintf(cfile->file_name, 256, "Capture %d", mainw->cap_number);
          lives_snprintf(cfile->name, CLIP_NAME_MAXLEN, "Capture %d", mainw->cap_number++);
          lives_snprintf(cfile->type, 40, "Frames");

          cfile->progress_start = cfile->start = 1;
          cfile->progress_end = cfile->frames = cfile->end = new_frames;
          cfile->pb_fps = cfile->fps = mainw->rec_fps;

          cfile->hsize = CEIL(mainw->foreign_width, 4);
          cfile->vsize = CEIL(mainw->foreign_height, 4);

          cfile->img_type = IMG_TYPE_BEST;
          cfile->changed = TRUE;

          if (mainw->rec_achans > 0) {
            cfile->arate = cfile->arps = mainw->rec_arate;
            cfile->achans = mainw->rec_achans;
            cfile->asampsize = mainw->rec_asamps;
            cfile->signed_endian = mainw->rec_signed_endian;
          }

          save_clip_values(mainw->current_file);
          if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);

          close(capture_fd);
          lives_rm(capfile);
          capture_fd = -1;
          do_threaded_dialog(_("Cleaning up clip"), FALSE);
          lives_widget_show_all(mainw->proc_ptr->processing);
          resize_all(mainw->current_file, cfile->hsize, cfile->vsize, cfile->img_type, FALSE, NULL, NULL);
          end_threaded_dialog();
          if (cfile->afilesize > 0 && cfile->achans > 0
              && CLIP_TOTAL_TIME(mainw->current_file) > cfile->laudio_time + AV_TRACK_MIN_DIFF) {
            pad_init_silence();
          }
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (capture_fd > -1) {
    close(capture_fd);
    lives_rm(capfile);
  }

  if (new_frames == 0) {
    // nothing captured; or cancelled
    lives_free(capfile);
    return FALSE;
  }

  cfile->nopreview = FALSE;
  lives_free(capfile);

  add_to_clipmenu();
  if (!mainw->multitrack) switch_to_file(old_file, mainw->current_file);

  else {
    int new_file = mainw->current_file;
    mainw->current_file = mainw->multitrack->render_file;
    mt_init_clips(mainw->multitrack, new_file, TRUE);
    mt_clip_select(mainw->multitrack, TRUE);
  }

  cfile->is_loaded = TRUE;
  cfile->changed = TRUE;
  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean int_array_contains_value(int *array, int num_elems, int value) {
  for (int i = 0; i < num_elems; i++) if (array[i] == value) return TRUE;
  return FALSE;
}


void reset_clipmenu(void) {
  // sometimes the clip menu gets messed up, e.g. after reloading a set.
  // This function will clean up the 'x's and so on.

  if (mainw->current_file > 0 && cfile && cfile->menuentry) {
#ifdef GTK_RADIO_MENU_BUG
    register int i;
    for (i = 1; i < MAX_FILES; i++) {
      if (i != mainw->current_file && mainw->files[i] && mainw->files[i]->menuentry) {
        lives_signal_handler_block(mainw->files[i]->menuentry, mainw->files[i]->menuentry_func);
        lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->files[i]->menuentry), FALSE);
        lives_signal_handler_unblock(mainw->files[i]->menuentry, mainw->files[i]->menuentry_func);
      }
    }
#endif
    lives_signal_handler_block(cfile->menuentry, cfile->menuentry_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(cfile->menuentry), TRUE);
    lives_signal_handler_unblock(cfile->menuentry, cfile->menuentry_func);
  }
}


boolean check_file(const char *file_name, boolean check_existing) {
  int check;
  boolean exists = FALSE;
  char *msg;
  // file_name should be in utf8
  char *lfile_name = U82F(file_name);

  mainw->error = FALSE;

  while (1) {
    // check if file exists
    if (lives_file_test(lfile_name, LIVES_FILE_TEST_EXISTS)) {
      if (check_existing) {
        msg = lives_strdup_printf(_("\n%s\nalready exists.\n\nOverwrite ?\n"), file_name);
        if (!do_warning_dialog(msg)) {
          lives_free(msg);
          lives_free(lfile_name);
          return FALSE;
        }
        lives_free(msg);
      }
      check = open(lfile_name, O_WRONLY);
      exists = TRUE;
    }
    // if not, check if we can write to it
    else {
      check = open(lfile_name, O_CREAT | O_EXCL | O_WRONLY, DEF_FILE_PERMS);
    }

    if (check < 0) {
      LiVESResponseType resp = LIVES_RESPONSE_NONE;
      mainw->error = TRUE;
      if (mainw && mainw->is_ready) {
        if (errno == EACCES)
          resp = do_file_perm_error(lfile_name, TRUE);
        else
          resp = do_write_failed_error_s_with_retry(lfile_name, NULL);
        if (resp == LIVES_RESPONSE_RETRY) {
          continue;
        }
      }
      lives_free(lfile_name);
      return FALSE;
    }

    close(check);
    break;
  }
  if (!exists) lives_rm(lfile_name);
  lives_free(lfile_name);
  return TRUE;
}


int lives_rmdir(const char *dir, boolean force) {
  // if force is TRUE, removes non-empty dirs, otherwise leaves them
  // may fail
  char *com, *cmd;
  int retval;

  if (force) {
    cmd = lives_strdup_printf("%s -rf", capable->rm_cmd);
  } else {
    cmd = lives_strdup(capable->rmdir_cmd);
  }

  com = lives_strdup_printf("%s \"%s/\" >\"%s\" 2>&1", cmd, dir, prefs->cmd_log);
  retval = lives_system(com, TRUE);
  lives_free(com);
  lives_free(cmd);
  return retval;
}


int lives_rmdir_with_parents(const char *dir) {
  // may fail, will not remove empty dirs
  char *com = lives_strdup_printf("%s -p \"%s\" >\"%s\" 2>&1", capable->rmdir_cmd, dir, prefs->cmd_log);
  int retval = lives_system(com, TRUE);
  lives_free(com);
  return retval;
}


int lives_rm(const char *file) {
  // may fail
  char *com = lives_strdup_printf("%s -f \"%s\" >\"%s\" 2>&1", capable->rm_cmd, file, prefs->cmd_log);
  int retval = lives_system(com, TRUE);
  lives_free(com);
  return retval;
}


int lives_rmglob(const char *files) {
  // delete files with name "files"*
  // may fail
  char *com = lives_strdup_printf("%s \"%s\"* >\"%s\" 2>&1", capable->rm_cmd, files, prefs->cmd_log);
  int retval = lives_system(com, TRUE);
  lives_free(com);
  return retval;
}


int lives_cp(const char *from, const char *to) {
  // may not fail - BUT seems to return -1 sometimes
  char *com = lives_strdup_printf("%s \"%s\" \"%s\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_cp_recursive(const char *from, const char *to, boolean incl_dir) {
  // may not fail
  int retval;
  char *com;
  if (incl_dir) com = lives_strdup_printf("%s -r \"%s\" \"%s\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  else com = lives_strdup_printf("%s -rf \"%s\"/* \"%s\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  if (!lives_file_test(to, LIVES_FILE_TEST_EXISTS))
    lives_mkdir_with_parents(to, capable->umask);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_cp_keep_perms(const char *from, const char *to) {
  // may not fail
  char *com = lives_strdup_printf("%s -a \"%s\" \"%s/\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_mv(const char *from, const char *to) {
  // may not fail
  char *com = lives_strdup_printf("%s \"%s\" \"%s\"", capable->mv_cmd, from, to);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_touch(const char *tfile) {
  // may not fail
  char *com = lives_strdup_printf("%s \"%s\" >\"%s\" 2>&1", capable->touch_cmd, tfile, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_ln(const char *from, const char *to) {
  // may not fail
  char *com;
  int retval;
  com = lives_strdup_printf("%s -s \"%s\" \"%s\" >\"%s\" 2>&1", capable->ln_cmd, from, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_chmod(const char *target, const char *mode) {
  // may not fail
  char *com = lives_strdup_printf("%s %s \"%s\" >\"%s\" 2>&1", capable->chmod_cmd, mode, target, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_cat(const char *from, const char *to, boolean append) {
  // may not fail
  char *com;
  char *op;
  int retval;

  if (append) op = ">>";
  else op = ">";

  com = lives_strdup_printf("%s \"%s\" %s \"%s\" >\"%s\" 2>&1", capable->cat_cmd, from, op, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_echo(const char *text, const char *to, boolean append) {
  // may not fail
  char *com;
  char *op;
  int retval;

  if (append) op = ">>";
  else op = ">";

  com = lives_strdup_printf("%s \"%s\" %s \"%s\" 2>\"%s\"", capable->echo_cmd, text, op, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


void lives_kill_subprocesses(const char *dirname, boolean kill_parent) {
  char *com;
  if (kill_parent)
    com = lives_strdup_printf("%s stopsubsub \"%s\"", prefs->backend_sync, dirname);
  else
    com = lives_strdup_printf("%s stopsubsubs \"%s\"", prefs->backend_sync, dirname);
  lives_system(com, TRUE);
  lives_free(com);
}


void lives_suspend_resume_process(const char *dirname, boolean suspend) {
  char *com;
  if (!suspend)
    com = lives_strdup_printf("%s stopsubsub \"%s\" SIGCONT 2>/dev/null", prefs->backend_sync, dirname);
  else
    com = lives_strdup_printf("%s stopsubsub \"%s\" SIGTSTP 2>/dev/null", prefs->backend_sync, dirname);
  lives_system(com, TRUE);
  lives_free(com);

  com = lives_strdup_printf("%s resume \"%s\"", prefs->backend_sync, dirname);
  lives_system(com, FALSE);
  lives_free(com);
}


boolean check_dir_access(const char *dir, boolean leaveit) {
  // if a directory exists, make sure it is readable and writable
  // otherwise create it and then check
  // we test here by actually creating a (mkstemp) file and writing to it
  // dir is in locale encoding

  // see also is_writeable_dir() which uses statvfs

  // WARNING: may leave some parents around
  char test[5] = "1234";
  char *testfile;
  boolean exists = lives_file_test(dir, LIVES_FILE_TEST_EXISTS);
  int fp;

  if (!exists) lives_mkdir_with_parents(dir, capable->umask);

  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) return FALSE;

  testfile = lives_build_filename(dir, "livestst-XXXXXX", NULL);
  fp = g_mkstemp(testfile);
  if (fp == -1) {
    lives_free(testfile);
    if (!exists) {
      lives_rmdir(dir, FALSE);
    }
    return FALSE;
  }
  if (lives_write(fp, test, 4, TRUE) != 4) {
    close(fp);
    lives_rm(testfile);
    if (!exists) {
      lives_rmdir(dir, FALSE);
    }
    lives_free(testfile);
    return FALSE;
  }
  close(fp);
  fp = lives_open2(testfile, O_RDONLY);
  if (fp < 0) {
    lives_rm(testfile);
    if (!exists) {
      lives_rmdir(dir, FALSE);
    }
    lives_free(testfile);
    return FALSE;
  }
  if (lives_read(fp, test, 4, TRUE) != 4) {
    close(fp);
    lives_rm(testfile);
    if (!exists) {
      lives_rmdir(dir, FALSE);
    }
    lives_free(testfile);
    return FALSE;
  }
  close(fp);
  lives_rm(testfile);
  if (!exists && !leaveit) {
    lives_rmdir(dir, FALSE);
  }
  lives_free(testfile);
  return TRUE;
}


void activate_url_inner(const char *link) {
#if GTK_CHECK_VERSION(2, 14, 0)
  LiVESError *err = NULL;
#if GTK_CHECK_VERSION(3, 22, 0)
  gtk_show_uri_on_window(NULL, link, GDK_CURRENT_TIME, &err);
#else
  gtk_show_uri(NULL, link, GDK_CURRENT_TIME, &err);
#endif
#else
  char *com = getenv("BROWSER");
  com = lives_strdup_printf("\"%s\" '%s' &", com ? com : "gnome-open", link);
  lives_system(com, FALSE);
  lives_free(com);
#endif
}


void activate_url(LiVESAboutDialog * about, const char *link, livespointer data) {
  activate_url_inner(link);
}


void show_manual_section(const char *lang, const char *section) {
  char *tmp = NULL, *tmp2 = NULL;
  const char *link;

  link = lives_strdup_printf("%s%s%s%s", LIVES_MANUAL_URL, (lang == NULL ? "" : (tmp2 = lives_strdup_printf("//%s//", lang))),
                             LIVES_MANUAL_FILENAME, (section == NULL ? "" : (tmp = lives_strdup_printf("#%s", section))));

  activate_url_inner(link);

  if (tmp) lives_free(tmp);
  if (tmp2) lives_free(tmp2);
}



void wait_for_bg_audio_sync(int fileno) {
  char *afile = lives_get_audio_file_name(fileno);
  lives_alarm_t alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
  int fd;

  while ((fd = open(afile, O_RDONLY)) < 0 && lives_alarm_check(alarm_handle) > 0) {
    lives_sync(1);
    lives_usleep(prefs->sleep_time);
  }
  lives_alarm_clear(alarm_handle);

  if (fd >= 0) close(fd);
  lives_free(afile);
}


boolean create_event_space(int length) {
  // try to create desired events
  // if we run out of memory, all events requested are freed, and we return FALSE
  // otherwise we return TRUE

  // NOTE: this is the OLD event system, it's only used for reordering in the clip editor

  if (cfile->resample_events) {
    lives_free(cfile->resample_events);
  }
  if ((cfile->resample_events = (resample_event *)(lives_calloc(length, sizeof(resample_event)))) == NULL) {
    // memory overflow
    return FALSE;
  }
  return TRUE;
}


int lives_list_strcmp_index(LiVESList * list, livesconstpointer data, boolean case_sensitive) {
  // find data in list, using strcmp
  int i;
  int len;
  if (!list) return -1;

  len = lives_list_length(list);

  if (case_sensitive) {
    for (i = 0; i < len; i++) {
      if (!lives_strcmp((const char *)lives_list_nth_data(list, i), (const char *)data)) return i;
      if (!lives_strcmp((const char *)lives_list_nth_data(list, i), (const char *)data)) return i;
    }
  } else {
    for (i = 0; i < len; i++) {
      if (!lives_utf8_strcasecmp((const char *)lives_list_nth_data(list, i), (const char *)data)) return i;
      if (!lives_utf8_strcasecmp((const char *)lives_list_nth_data(list, i), (const char *)data)) return i;
    }
  }
  return -1;
}


void add_to_recent(const char *filename, double start, frames_t frames, const char *extra_params) {
  const char *mtext;
  char buff[PATH_MAX * 2];
  char *file, *mfile, *prefname;
  register int i;

  if (frames > 0) {
    mfile = lives_strdup_printf("%s|%.2f|%d", filename, start, frames);
    if (!extra_params || (!(*extra_params))) file = lives_strdup(mfile);
    else file = lives_strdup_printf("%s\n%s", mfile, extra_params);
  } else {
    mfile = lives_strdup(filename);
    if (!extra_params || (!(*extra_params))) file = lives_strdup(mfile);
    else file = lives_strdup_printf("%s\n%s", mfile, extra_params);
  }

  for (i = 0; i < N_RECENT_FILES; i++) {
    mtext = lives_menu_item_get_text(mainw->recent[i]);
    if (!lives_strcmp(mfile, mtext)) break;
  }

  if (i == 0) return;

  if (i == N_RECENT_FILES) --i;

  for (; i > 0; i--) {
    mtext = lives_menu_item_get_text(mainw->recent[i - 1]);
    lives_menu_item_set_text(mainw->recent[i], mtext, FALSE);
    if (mainw->multitrack) lives_menu_item_set_text(mainw->multitrack->recent[i], mtext, FALSE);

    prefname = lives_strdup_printf("%s%d", PREF_RECENT, i);
    get_utf8_pref(prefname, buff, PATH_MAX * 2);
    lives_free(prefname);

    prefname = lives_strdup_printf("%s%d", PREF_RECENT, i + 1);
    set_utf8_pref(prefname, buff);
    lives_free(prefname);
  }

  lives_menu_item_set_text(mainw->recent[0], mfile, FALSE);
  if (mainw->multitrack) lives_menu_item_set_text(mainw->multitrack->recent[0], mfile, FALSE);
  prefname = lives_strdup_printf("%s%d", PREF_RECENT, 1);
  set_utf8_pref(prefname, file);
  lives_free(prefname);

  for (; i < N_RECENT_FILES; i++) {
    mtext = lives_menu_item_get_text(mainw->recent[i]);
    if (*mtext) lives_widget_show(mainw->recent[i]);
  }

  lives_free(mfile); lives_free(file);
}


int verhash(char *xv) {
  char *version, *s;
  int major = 0, minor = 0, micro = 0;

  if (!xv) return 0;

  version = lives_strdup(xv);

  if (!(*version)) {
    lives_free(version);
    return 0;
  }

  s = strtok(version, ".");
  if (s) {
    major = atoi(s);
    s = strtok(NULL, ".");
    if (s) {
      minor = atoi(s);
      s = strtok(NULL, ".");
      if (s) micro = atoi(s);
    }
  }
  lives_free(version);
  return major * 1000000 + minor * 1000 + micro;
}


// TODO - move into undo.c
void set_undoable(const char *what, boolean sensitive) {
  if (mainw->current_file > -1) {
    cfile->redoable = FALSE;
    cfile->undoable = sensitive;
    if (what) {
      char *what_safe = lives_strdelimit(lives_strdup(what), "_", ' ');
      lives_snprintf(cfile->undo_text, 32, _("_Undo %s"), what_safe);
      lives_snprintf(cfile->redo_text, 32, _("_Redo %s"), what_safe);
      lives_free(what_safe);
    } else {
      cfile->undoable = FALSE;
      cfile->undo_action = UNDO_NONE;
      lives_snprintf(cfile->undo_text, 32, "%s", _("_Undo"));
      lives_snprintf(cfile->redo_text, 32, "%s", _("_Redo"));
    }
    lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
    lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);
  }

  lives_widget_hide(mainw->redo);
  lives_widget_show(mainw->undo);
  lives_widget_set_sensitive(mainw->undo, sensitive);

#ifdef PRODUCE_LOG
  lives_log(what);
#endif
}


void set_redoable(const char *what, boolean sensitive) {
  if (mainw->current_file > -1) {
    cfile->undoable = FALSE;
    cfile->redoable = sensitive;
    if (what) {
      char *what_safe = lives_strdelimit(lives_strdup(what), "_", ' ');
      lives_snprintf(cfile->undo_text, 32, _("_Undo %s"), what_safe);
      lives_snprintf(cfile->redo_text, 32, _("_Redo %s"), what_safe);
      lives_free(what_safe);
    } else {
      cfile->redoable = FALSE;
      cfile->undo_action = UNDO_NONE;
      lives_snprintf(cfile->undo_text, 32, "%s", _("_Undo"));
      lives_snprintf(cfile->redo_text, 32, "%s", _("_Redo"));
    }
    lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
    lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);
  }

  lives_widget_hide(mainw->undo);
  lives_widget_show(mainw->redo);
  lives_widget_set_sensitive(mainw->redo, sensitive);
}


void set_sel_label(LiVESWidget * sel_label) {
  char *tstr, *frstr, *tmp;
  char *sy, *sz;

  if (mainw->current_file == -1 || !cfile->frames || mainw->multitrack) {
    lives_label_set_text(LIVES_LABEL(sel_label), _("-------------Selection------------"));
  } else {
    tstr = lives_strdup_printf("%.2f", calc_time_from_frame(mainw->current_file, cfile->end + 1) -
                               calc_time_from_frame(mainw->current_file, cfile->start));
    frstr = lives_strdup_printf("%d", cfile->end - cfile->start + 1);

    // TRANSLATORS: - try to keep the text of the middle part the same length, by deleting "-" if necessary
    lives_label_set_text(LIVES_LABEL(sel_label),
                         (tmp = lives_strconcat("---------- [ ", tstr, (sy = ((_(" sec ] ----------Selection---------- [ ")))),
                                frstr, (sz = (_(" frames ] ----------"))), NULL)));
    lives_free(sy); lives_free(sz);
    lives_free(tmp); lives_free(frstr); lives_free(tstr);
  }
  lives_widget_queue_draw(sel_label);
}


LIVES_GLOBAL_INLINE void lives_list_free_strings(LiVESList * list) {
  for (; list; list = list->next) lives_freep((void **)&list->data);
}


LIVES_GLOBAL_INLINE void lives_slist_free_all(LiVESSList **list) {
  if (!list || !*list) return;
  lives_list_free_strings((LiVESList *)*list);
  lives_slist_free(*list);
  *list = NULL;
}


LIVES_GLOBAL_INLINE void lives_list_free_all(LiVESList **list) {
  if (!list || !*list) return;
  lives_list_free_strings(*list);
  lives_list_free(*list);
  *list = NULL;
}


LIVES_GLOBAL_INLINE void cached_list_free(LiVESList **list) {
  lives_speed_cache_t *speedy;
  for (LiVESList *xlist = *list; xlist; xlist = xlist->next) {
    speedy = (lives_speed_cache_t *)(*list)->data;
    if (speedy) {
      if (speedy->key) lives_free(speedy->key);
      if (speedy->data) lives_free(speedy->data);
      lives_free(speedy);
    }
    xlist->data = NULL;
  }
  lives_list_free(*list);
  *list = NULL;
}


void print_cache(LiVESList * cache) {
  /// for debugging
  lives_speed_cache_t *speedy;
  LiVESList *ll = cache;
  g_print("dumping cache %p\n", cache);
  for (; ll; ll = ll->next) {
    speedy = (lives_speed_cache_t *)ll->data;
    g_print("cach dets: %s = %s\n", speedy->key, speedy->data);
  }
}


LiVESList *cache_file_contents(const char *filename) {
  lives_speed_cache_t *speedy;
  LiVESList *list = NULL;
  FILE *hfile;
  size_t kelen;
  char buff[65536];
  char *key = NULL, *keystr_end = NULL, *cptr, *tmp, *data = NULL;

  if (!(hfile = fopen(filename, "r"))) return NULL;
  while (fgets(buff, 65536, hfile)) {
    if (!*buff) continue;
    if (*buff == '#') continue;
    if (key) {
      if (!lives_strncmp(buff, keystr_end, kelen)) {
        speedy = (lives_speed_cache_t *)lives_calloc(1, sizeof(lives_speed_cache_t));
        speedy->hash = fast_hash(key);
        speedy->key = key;
        speedy->data = data;
        key = data = NULL;
        lives_free(keystr_end);
        keystr_end = NULL;
        list = lives_list_prepend(list, speedy);
        continue;
      }
      cptr = buff;
      if (data) {
        if (*buff != '|') continue;
        cptr++;
      }
      lives_chomp(cptr);
      tmp = lives_strdup_printf("%s%s", data ? data : "", cptr);
      if (data) lives_free(data);
      data = tmp;
      continue;
    }
    if (*buff != '<') continue;
    kelen = 0;
    for (cptr = buff; cptr; cptr++) {
      if (*cptr == '>') {
        kelen = cptr - buff;
        if (kelen > 2) {
          *cptr = 0;
          key = lives_strdup(buff + 1);
          keystr_end = lives_strdup_printf("</%s>", key);
          kelen++;
        }
        break;
      }
    }
  }
  fclose(hfile);
  if (key) lives_free(key);
  if (keystr_end) lives_free(keystr_end);
  return lives_list_reverse(list);
}


char *get_val_from_cached_list(const char *key, size_t maxlen, LiVESList * cache) {
  // WARNING - contents may be invalid if the underlying file is updated (e.g with set_*_pref())
  LiVESList *list = cache;
  uint32_t khash = fast_hash(key);
  lives_speed_cache_t *speedy;
  for (; list; list = list->next) {
    speedy = (lives_speed_cache_t *)list->data;
    if (khash == speedy->hash && !lives_strcmp(key, speedy->key))
      return lives_strndup(speedy->data, maxlen);
  }
  return NULL;
}


char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp) {
  char *key = NULL;

  switch (what) {
  case CLIP_DETAILS_HEADER_VERSION:
    key = lives_strdup("header_version"); break;
  case CLIP_DETAILS_BPP:
    key = lives_strdup("bpp"); break;
  case CLIP_DETAILS_FPS:
    key = lives_strdup("fps"); break;
  case CLIP_DETAILS_PB_FPS:
    key = lives_strdup("pb_fps"); break;
  case CLIP_DETAILS_WIDTH:
    key = lives_strdup("width"); break;
  case CLIP_DETAILS_HEIGHT:
    key = lives_strdup("height"); break;
  case CLIP_DETAILS_UNIQUE_ID:
    key = lives_strdup("unique_id"); break;
  case CLIP_DETAILS_ARATE:
    key = lives_strdup("audio_rate"); break;
  case CLIP_DETAILS_PB_ARATE:
    key = lives_strdup("pb_audio_rate"); break;
  case CLIP_DETAILS_ACHANS:
    key = lives_strdup("audio_channels"); break;
  case CLIP_DETAILS_ASIGNED:
    key = lives_strdup("audio_signed"); break;
  case CLIP_DETAILS_AENDIAN:
    key = lives_strdup("audio_endian"); break;
  case CLIP_DETAILS_ASAMPS:
    key = lives_strdup("audio_sample_size"); break;
  case CLIP_DETAILS_FRAMES:
    key = lives_strdup("frames"); break;
  case CLIP_DETAILS_TITLE:
    key = lives_strdup("title"); break;
  case CLIP_DETAILS_AUTHOR:
    key = lives_strdup("author"); break;
  case CLIP_DETAILS_COMMENT:
    key = lives_strdup("comment"); break;
  case CLIP_DETAILS_KEYWORDS:
    key = lives_strdup("keywords"); break;
  case CLIP_DETAILS_PB_FRAMENO:
    key = lives_strdup("pb_frameno"); break;
  case CLIP_DETAILS_CLIPNAME:
    key = lives_strdup("clipname"); break;
  case CLIP_DETAILS_FILENAME:
    key = lives_strdup("filename"); break;
  case CLIP_DETAILS_INTERLACE:
    key = lives_strdup("interlace"); break;
  case CLIP_DETAILS_DECODER_NAME:
    key = lives_strdup("decoder"); break;
  case CLIP_DETAILS_GAMMA_TYPE:
    key = lives_strdup("gamma_type"); break;
  default: break;
  }
  if (maxlenp && *maxlenp == 0) *maxlenp = 256;
  return key;
}


boolean get_clip_value(int which, lives_clip_details_t what, void *retval, size_t maxlen) {
  lives_clip_t *sfile = mainw->files[which];
  char *lives_header = NULL;
  char *val, *key, *tmp;

  int retval2 = LIVES_RESPONSE_NONE;

  if (!IS_VALID_CLIP(which)) return FALSE;

  if (!mainw->hdrs_cache) {
    /// ascrap_file now uses a different header name; this is to facilitate diskspace cleanup
    /// otherwise it may be wrongly classified as a recoverable clip
    /// (here this is largely academic, since the values are only read during crash recovery,
    /// and the header should have been cached)
    if (which == mainw->ascrap_file) {
      lives_header = lives_build_filename(prefs->workdir, mainw->files[which]->handle,
                                          LIVES_ACLIP_HEADER, NULL);
      if (!lives_file_test(lives_header, LIVES_FILE_TEST_EXISTS)) {
        lives_free(lives_header);
        lives_header = NULL;
      }
    }
    if (!lives_header)
      lives_header = lives_build_filename(prefs->workdir, mainw->files[which]->handle,
                                          LIVES_CLIP_HEADER, NULL);
    if (!sfile->checked_for_old_header) {
      struct stat mystat;
      time_t old_time = 0, new_time = 0;
      char *old_header = lives_build_filename(prefs->workdir, sfile->handle, LIVES_CLIP_HEADER_OLD, NULL);
      sfile->checked_for_old_header = TRUE;
      if (!lives_file_test(old_header, LIVES_FILE_TEST_EXISTS)) {
        if (!stat(old_header, &mystat)) old_time = mystat.st_mtime;
        if (!stat(lives_header, &mystat)) new_time = mystat.st_mtime;
        if (old_time > new_time) {
          sfile->has_old_header = TRUE;
          lives_free(lives_header);
          return FALSE; // clip has been edited by an older version of LiVES
        }
      }
      lives_free(old_header);
    }
  }

  //////////////////////////////////////////////////
  key = clip_detail_to_string(what, &maxlen);

  if (!key) {
    tmp = lives_strdup_printf("Invalid detail %d requested from file %s", which, lives_header);
    LIVES_ERROR(tmp);
    lives_free(tmp);
    lives_free(lives_header);
    return FALSE;
  }

  if (mainw->hdrs_cache) {
    val = get_val_from_cached_list(key, maxlen, mainw->hdrs_cache);
    lives_free(key);
    if (!val) return FALSE;
  } else {
    val = (char *)lives_malloc(maxlen);
    if (!val) return FALSE;
    retval2 = get_pref_from_file(lives_header, key, val, maxlen);
    lives_free(lives_header);
    lives_free(key);
  }

  if (retval2 == LIVES_RESPONSE_CANCEL) {
    lives_free(val);
    return FALSE;
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
  case CLIP_DETAILS_WIDTH:
  case CLIP_DETAILS_HEIGHT:
  case CLIP_DETAILS_ARATE:
  case CLIP_DETAILS_ACHANS:
  case CLIP_DETAILS_ASAMPS:
  case CLIP_DETAILS_FRAMES:
  case CLIP_DETAILS_GAMMA_TYPE:
  case CLIP_DETAILS_HEADER_VERSION:
    *(int *)retval = atoi(val); break;
  case CLIP_DETAILS_ASIGNED:
    *(int *)retval = 0;
    if (sfile->header_version == 0) *(int *)retval = atoi(val);
    if (*(int *)retval == 0 && (!strcasecmp(val, "false"))) *(int *)retval = 1; // unsigned
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    *(int *)retval = atoi(val);
    if (retval == 0) *(int *)retval = 1;
    break;
  case CLIP_DETAILS_PB_ARATE:
    *(int *)retval = atoi(val);
    if (retval == 0) *(int *)retval = sfile->arps;
    break;
  case CLIP_DETAILS_INTERLACE:
    *(int *)retval = atoi(val);
    break;
  case CLIP_DETAILS_FPS:
    *(double *)retval = strtod(val, NULL);
    if (*(double *)retval == 0.) *(double *)retval = prefs->default_fps;
    break;
  case CLIP_DETAILS_PB_FPS:
    *(double *)retval = strtod(val, NULL);
    if (*(double *)retval == 0.) *(double *)retval = sfile->fps;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    if (capable->cpu_bits == 32) {
      *(uint64_t *)retval = (uint64_t)atoll(val);
    } else {
      *(uint64_t *)retval = (uint64_t)atol(val);
    }
    break;
  case CLIP_DETAILS_AENDIAN:
    *(int *)retval = atoi(val) * 2; break;
  case CLIP_DETAILS_TITLE:
  case CLIP_DETAILS_AUTHOR:
  case CLIP_DETAILS_COMMENT:
  case CLIP_DETAILS_CLIPNAME:
  case CLIP_DETAILS_KEYWORDS:
    lives_snprintf((char *)retval, maxlen, "%s", val);
    break;
  case CLIP_DETAILS_FILENAME:
  case CLIP_DETAILS_DECODER_NAME:
    lives_snprintf((char *)retval, maxlen, "%s", (tmp = F2U8(val)));
    lives_free(tmp);
    break;
  default:
    lives_free(val);
    return FALSE;
  }
  lives_free(val);
  return TRUE;
}


boolean save_clip_value(int which, lives_clip_details_t what, void *val) {
  lives_clip_t *sfile;
  char *lives_header;
  char *com, *tmp;
  char *myval;
  char *key;

  boolean needs_sigs = FALSE;

  THREADVAR(write_failed) = 0;
  THREADVAR(com_failed) = FALSE;

  if (which == 0 || which == mainw->scrap_file) return FALSE;

  if (!IS_VALID_CLIP(which)) return FALSE;

  sfile = mainw->files[which];

  /// ascrap_file now uses a different header name; this is to facilitate diskspace cleanup
  /// otherwise it may be wrongly classified as a recoverable clip
  if (which == mainw->ascrap_file)
    lives_header = lives_build_filename(prefs->workdir, sfile->handle, LIVES_ACLIP_HEADER, NULL);
  else
    lives_header = lives_build_filename(prefs->workdir, sfile->handle, LIVES_CLIP_HEADER, NULL);

  key = clip_detail_to_string(what, NULL);

  if (!key) {
    tmp = lives_strdup_printf("Invalid detail %d added for file %s", which, lives_header);
    LIVES_ERROR(tmp);
    lives_free(tmp);
    lives_free(lives_header);
    return FALSE;
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_FPS:
    if (!sfile->ratio_fps) myval = lives_strdup_printf("%.3f", *(double *)val);
    else myval = lives_strdup_printf("%.8f", *(double *)val);
    break;
  case CLIP_DETAILS_PB_FPS:
    if (sfile->ratio_fps && (sfile->pb_fps == sfile->fps))
      myval = lives_strdup_printf("%.8f", *(double *)val);
    else myval = lives_strdup_printf("%.3f", *(double *)val);
    break;
  case CLIP_DETAILS_WIDTH:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_HEIGHT:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_UNIQUE_ID:
    myval = lives_strdup_printf("%"PRIu64, *(uint64_t *)val); break;
  case CLIP_DETAILS_ARATE:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_PB_ARATE:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_ACHANS:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_ASIGNED:
    if ((*(int *)val) == 1) myval = lives_strdup("true");
    else myval = lives_strdup("false");
    break;
  case CLIP_DETAILS_AENDIAN:
    myval = lives_strdup_printf("%d", (*(int *)val) / 2);
    break;
  case CLIP_DETAILS_ASAMPS:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_FRAMES:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_GAMMA_TYPE:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_INTERLACE:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_TITLE:
    myval = lives_strdup((char *)val); break;
  case CLIP_DETAILS_AUTHOR:
    myval = lives_strdup((char *)val); break;
  case CLIP_DETAILS_COMMENT:
    myval = lives_strdup((const char *)val); break;
  case CLIP_DETAILS_KEYWORDS:
    myval = lives_strdup((const char *)val); break;
  case CLIP_DETAILS_PB_FRAMENO:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_CLIPNAME:
    myval = lives_strdup((char *)val); break;
  case CLIP_DETAILS_FILENAME:
    myval = U82F((const char *)val); break;
  case CLIP_DETAILS_DECODER_NAME:
    myval = U82F((const char *)val); break;
  case CLIP_DETAILS_HEADER_VERSION:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  default:
    return FALSE;
  }

  if (mainw->clip_header) {
    char *keystr_start = lives_strdup_printf("<%s>\n", key);
    char *keystr_end = lives_strdup_printf("\n</%s>\n\n", key);
    lives_fputs(keystr_start, mainw->clip_header);
    lives_fputs(myval, mainw->clip_header);
    lives_fputs(keystr_end, mainw->clip_header);
    lives_free(keystr_start);
    lives_free(keystr_end);
  } else {
    if (!mainw->signals_deferred) {
      set_signal_handlers((SignalHandlerPointer)defer_sigint);
      needs_sigs = TRUE;
    }
    com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"", prefs->backend_sync, lives_header, key, myval);
    lives_system(com, FALSE);
    if (mainw->signal_caught) catch_sigint(mainw->signal_caught);
    if (needs_sigs) set_signal_handlers((SignalHandlerPointer)catch_sigint);
    lives_free(com);
  }

  lives_free(lives_header);
  lives_free(myval);
  lives_free(key);

  if (mainw->clip_header && THREADVAR(write_failed) == fileno(mainw->clip_header) + 1) {
    THREADVAR(write_failed) = 0;
    return FALSE;
  }
  if (THREADVAR(com_failed)) return FALSE;
  return TRUE;
}


LiVESList *get_set_list(const char *dir, boolean utf8) {
  // get list of sets in top level dir
  // values will be in filename encoding

  LiVESList *setlist = NULL;
  DIR *tldir, *subdir;
  struct dirent *tdirent, *subdirent;
  char *subdirname;

  if (!dir) return NULL;

  tldir = opendir(dir);

  if (!tldir) return NULL;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  while (1) {
    tdirent = readdir(tldir);

    if (!tdirent) {
      closedir(tldir);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
      return setlist;
    }

    if (tdirent->d_name[0] == '.'
        && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;

    subdirname = lives_build_filename(dir, tdirent->d_name, NULL);
    subdir = opendir(subdirname);

    if (!subdir) {
      lives_free(subdirname);
      continue;
    }

    while (1) {
      subdirent = readdir(subdir);
      if (!subdirent) break;

      if (!strcmp(subdirent->d_name, "order")) {
        if (!utf8)
          setlist = lives_list_append(setlist, lives_strdup(tdirent->d_name));
        else
          setlist = lives_list_append(setlist, F2U8(tdirent->d_name));
        break;
      }
    }
    lives_free(subdirname);
    closedir(subdir);
  }
}


boolean check_for_ratio_fps(double fps) {
  boolean ratio_fps;
  char *test_fps_string1 = lives_strdup_printf("%.3f00000", fps);
  char *test_fps_string2 = lives_strdup_printf("%.8f", fps);

  if (strcmp(test_fps_string1, test_fps_string2)) {
    // got a ratio
    ratio_fps = TRUE;
  } else {
    ratio_fps = FALSE;
  }
  lives_free(test_fps_string1);
  lives_free(test_fps_string2);

  return ratio_fps;
}


double get_ratio_fps(const char *string) {
  // return a ratio (8dp) fps from a string with format num:denom
  double fps;
  char *fps_string;
  char **array = lives_strsplit(string, ":", 2);
  int num = atoi(array[0]);
  int denom = atoi(array[1]);
  lives_strfreev(array);
  fps = (double)num / (double)denom;
  fps_string = lives_strdup_printf("%.8f", fps);
  fps = lives_strtod(fps_string, NULL);
  lives_free(fps_string);
  return fps;
}


char *remove_trailing_zeroes(double val) {
  int i;
  double xval = val;

  if (val == (int)val) return lives_strdup_printf("%d", (int)val);
  for (i = 0; i <= 16; i++) {
    xval *= 10.;
    if (xval == (int)xval) return lives_strdup_printf("%.*f", i, val);
  }
  return lives_strdup_printf("%.*f", i, val);
}


uint32_t get_signed_endian(boolean is_signed, boolean little_endian) {
  // asigned TRUE == signed, FALSE == unsigned

  if (is_signed) {
    if (little_endian) {
      return 0;
    } else {
      return AFORM_BIG_ENDIAN;
    }
  } else {
    if (!is_signed) {
      if (little_endian) {
        return AFORM_UNSIGNED;
      } else {
        return AFORM_UNSIGNED | AFORM_BIG_ENDIAN;
      }
    }
  }
  return AFORM_UNKNOWN;
}


size_t get_token_count(const char *string, int delim) {
  size_t pieces = 1;
  if (!string) return 0;
  if (delim <= 0 || delim > 255) return 1;

  while ((string = strchr(string, delim)) != NULL) {
    pieces++;
    string++;
  }
  return pieces;
}


char *get_nth_token(const char *string, const char *delim, int pnumber) {
  char **array;
  char *ret = NULL;
  register int i;
  if (pnumber < 0 || pnumber >= get_token_count(string, (int)delim[0])) return NULL;
  array = lives_strsplit(string, delim, pnumber + 1);
  for (i = 0; i < pnumber; i++) {
    if (i == pnumber) ret = array[i];
    else lives_free(array[i]);
  }
  lives_free(array);
  return ret;
}


int lives_utf8_strcasecmp(const char *s1, const char *s2) {
  // ignore case
  char *s1u = lives_utf8_casefold(s1, -1);
  char *s2u = lives_utf8_casefold(s2, -1);
  int ret = lives_strcmp(s1u, s2u);
  lives_free(s1u);
  lives_free(s2u);
  return ret;
}


LIVES_GLOBAL_INLINE int lives_utf8_strcmp(const char *s1, const char *s2) {
  return lives_utf8_collate(s1, s2);
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_sort_alpha(LiVESList * list, boolean fwd) {
  /// stable sort, so input list should NOT be freed
  /// handles utf-8 strings
  return lives_list_sort_with_data(list, lives_utf8_strcmpfunc, LIVES_INT_TO_POINTER(fwd));
}


#define BSIZE (8)
#define INITSIZE 32

char *subst(const char *xstring, const char *from, const char *to) {
  // return a string with all occurrences of from replaced with to
  // return value should be freed after use
  char *ret = lives_calloc(INITSIZE, BSIZE);
  uint64_t ubuff = 0;
  char *buff;

  const size_t fromlen = strlen(from);
  const size_t tolen = strlen(to);
  const size_t tolim = BSIZE - tolen;

  size_t match = 0;
  size_t xtolen = tolen;
  size_t bufil = 0;
  size_t retfil = 0;
  size_t retsize = INITSIZE;
  size_t retlimit = retsize - BSIZE;

  buff = (char *)&ubuff;

  for (char *cptr = (char *)xstring; *cptr; cptr++) {
    if (*cptr == from[match++]) {
      if (match == fromlen) {
        match = 0;
        if (bufil > tolim) xtolen = BSIZE - bufil;
        lives_memcpy(buff + bufil, to, xtolen);
        if ((bufil += xtolen) == BSIZE) {
          if (retfil > retlimit) {
            ret = lives_recalloc(ret, retsize * 2, retsize, BSIZE);
            retsize *= 2;
            retlimit = (retsize - 1) *  BSIZE;
          }
          lives_memcpy(ret + retfil, buff, BSIZE);
          retfil += BSIZE;
          bufil = 0;
          if (xtolen < tolen) {
            lives_memcpy(buff, to + xtolen, tolen - xtolen);
            bufil += tolen - xtolen;
            xtolen = tolen;
          }
        }
      }
      continue;
    }
    if (--match > 0) {
      xtolen = match;
      if (bufil > BSIZE - match) xtolen = BSIZE - bufil;
      lives_memcpy(buff + bufil, from, xtolen);
      if ((bufil += xtolen) == BSIZE) {
        if (retfil > retlimit) {
          ret = lives_recalloc(ret, retsize * 2, retsize, BSIZE);
          retsize *= 2;
          retlimit = (retsize - 1) *  BSIZE;
        }
        lives_memcpy(ret + retfil, buff, BSIZE);
        retfil += BSIZE;
        bufil = 0;
        if (xtolen < fromlen) {
          lives_memcpy(buff, from + xtolen, fromlen - xtolen);
          bufil += fromlen - xtolen;
          xtolen = tolen;
        }
      }
      match = 0;
    }
    buff[bufil] = *cptr;
    if (++bufil == BSIZE) {
      if (retfil > retlimit) {
        ret = lives_recalloc(ret, retsize * 2, retsize, BSIZE);
        retsize *= 2;
        retlimit = (retsize - 1) *  BSIZE;
      }
      lives_memcpy(ret + retfil, buff, BSIZE);
      retfil += BSIZE;
      bufil = 0;
    }
  }

  if (bufil) {
    if (retsize > retlimit) {
      ret = lives_recalloc(ret, retsize + 1, retsize, BSIZE);
      retsize++;
    }
    lives_memcpy(ret + retfil, buff, bufil);
    retfil += bufil;
  }
  if (match) {
    if (retsize > retlimit) {
      ret = lives_recalloc(ret, retsize + 1, retsize, BSIZE);
      retsize++;
    }
    lives_memcpy(ret + retsize, from, match);
    retfil += match;
  }
  ret[retfil++] = 0;
  retsize *= BSIZE;

  if (retsize - retfil > (retsize >> 2)) {
    char *tmp = lives_malloc(retfil);
    lives_memcpy(tmp, ret, retfil);
    lives_free(ret);
    return tmp;
  }
  return ret;
}


char *insert_newlines(const char *text, int maxwidth) {
  // crude formating of strings, ensure a newline after every run of maxwidth chars
  // does not take into account for example utf8 multi byte chars

  wchar_t utfsym;
  char *retstr;

  size_t runlen = 0;
  size_t req_size = 1; // for the terminating \0
  size_t tlen, align = 1;

  int xtoffs;

  boolean needsnl = FALSE;

  register int i;

  if (!text) return NULL;

  if (maxwidth < 1) return lives_strdup("Bad maxwidth, dummy");

  tlen = lives_strlen(text);

  xtoffs = mbtowc(NULL, NULL, 0); // reset read state

  //pass 1, get the required size
  for (i = 0; i < tlen; i += xtoffs) {
    xtoffs = mbtowc(&utfsym, &text[i], 4); // get next utf8 wchar
    if (!xtoffs) break;
    if (xtoffs == -1) {
      LIVES_WARN("mbtowc returned -1");
      return lives_strdup(text);
    }

    if (*(text + i) == '\n') runlen = 0; // is a newline (in any encoding)
    else {
      runlen++;
      if (needsnl) req_size++; ///< we will insert a nl here
    }

    if (runlen == maxwidth) {
      if (i < tlen - 1 && (*(text + i + 1) != '\n')) {
        // needs a newline
        needsnl = TRUE;
        runlen = 0;
      }
    } else needsnl = FALSE;
    req_size += xtoffs;
  }

  xtoffs = mbtowc(NULL, NULL, 0); // reset read state

  align = get_max_align(req_size, DEF_ALIGN);

  retstr = (char *)lives_calloc(req_size / align, align);
  req_size = 0; // reuse as a ptr to offset in retstr
  runlen = 0;
  needsnl = FALSE;

  //pass 2, copy and insert newlines

  for (i = 0; i < tlen; i += xtoffs) {
    xtoffs = mbtowc(&utfsym, &text[i], 4); // get next utf8 wchar
    if (!xtoffs) break;
    if (*(text + i) == '\n') runlen = 0; // is a newline (in any encoding)
    else {
      runlen++;
      if (needsnl) {
        *(retstr + req_size) = '\n';
        req_size++;
      }
    }

    if (runlen == maxwidth) {
      if (i < tlen - 1 && (*(text + i + 1) != '\n')) {
        // needs a newline
        needsnl = TRUE;
        runlen = 0;
      }
    } else needsnl = FALSE;
    lives_memcpy(retstr + req_size, &utfsym, xtoffs);
    req_size += xtoffs;
  }

  *(retstr + req_size) = 0;

  return retstr;
}


static int get_hex_digit(const char c) {
  switch (c) {
  case 'a': case 'A': return 10;
  case 'b': case 'B': return 11;
  case 'c': case 'C': return 12;
  case 'd': case 'D': return 13;
  case 'e': case 'E': return 14;
  case 'f': case 'F': return 15;
  default: return c - 48;
  }
}


LIVES_GLOBAL_INLINE int hextodec(const char *string) {
  int tot = 0;
  for (char c = *string; c; c = *(++string)) tot = (tot << 4) + get_hex_digit(c);
  return tot;
}


boolean is_writeable_dir(const char *dir) {
  // return FALSE if we cannot create / write to dir
  // dir should be in locale encoding
  // WARNING: this will actually create the directory (since we dont know if its parents are needed)

  struct statvfs sbuf;
  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
    lives_mkdir_with_parents(dir, capable->umask);
    if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
      return FALSE;
    }
  }

  // use statvfs to get fs details
  if (statvfs(dir, &sbuf) == -1) return FALSE;
  if (sbuf.f_flag & ST_RDONLY) return FALSE;
  return TRUE;
}


boolean lives_make_writeable_dir(const char *newdir) {
  /// create a directory (including parents)
  /// and ensure we can actually write to it
  int ret = lives_mkdir_with_parents(newdir, capable->umask);
  int myerrno = errno;
  if (!check_dir_access(newdir, TRUE)) {
    // abort if we cannot create the new subdir
    if (myerrno == EINVAL) {
      LIVES_ERROR("Could not write to directory");
    } else LIVES_ERROR("Could not create directory");
    LIVES_ERROR(newdir);
    THREADVAR(com_failed) = FALSE;
    return FALSE;
  } else {
    if (ret != -1) {
      LIVES_DEBUG("Created directory");
      LIVES_DEBUG(newdir);
    }
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE LiVESInterpType get_interp_value(short quality, boolean low_for_mt) {
  if ((mainw->is_rendering || (mainw->multitrack && mainw->multitrack->is_rendering)) && !mainw->preview_rendering)
    return LIVES_INTERP_BEST;
  if (low_for_mt && mainw->multitrack) return LIVES_INTERP_FAST;
  if (quality <= PB_QUALITY_LOW) return LIVES_INTERP_FAST;
  else if (quality == PB_QUALITY_MED) return LIVES_INTERP_NORMAL;
  return LIVES_INTERP_BEST;
}


#define BL_LIM 128
LIVES_GLOBAL_INLINE LiVESList *buff_to_list(const char *buffer, const char *delim, boolean allow_blanks, boolean strip) {
  LiVESList *list = NULL;
  int pieces = get_token_count(buffer, delim[0]);
  char *buf, **array = lives_strsplit(buffer, delim, pieces);
  boolean biglist = pieces >= BL_LIM;
  for (int i = 0; i < pieces; i++) {
    if (array[i]) {
      if (strip) buf = lives_strstrip(array[i]);
      else buf = array[i];
      if (*buf || allow_blanks) {
        if (biglist) list = lives_list_prepend(list, lives_strdup(buf));
        else list = lives_list_append(list, lives_strdup(buf));
      }
    }
  }
  lives_strfreev(array);
  if (biglist && list) return lives_list_reverse(list);
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_append_unique(LiVESList * xlist, const char *add) {
  LiVESList *list = xlist, *listlast = NULL;
  while (list) {
    listlast = list;
    if (!lives_utf8_strcasecmp((const char *)list->data, add)) return xlist;
    list = list->next;
  }
  list = lives_list_append(listlast, lives_strdup(add));
  if (!xlist) return list;
  return xlist;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_move_to_first(LiVESList * list, LiVESList * item) {
  // move item to first in list
  LiVESList *xlist = item;
  if (xlist == list || !xlist) return list;
  if (xlist->prev) xlist->prev->next = xlist->next;
  if (xlist->next) xlist->next->prev = xlist->prev;
  xlist->prev = NULL;
  if ((xlist->next = list) != NULL) list->prev = xlist;
  return xlist;
}


LiVESList *lives_list_delete_string(LiVESList * list, const char *string) {
  // remove string from list, using strcmp

  LiVESList *xlist = list;
  for (; xlist; xlist = xlist->next) {
    if (!lives_utf8_strcasecmp((char *)xlist->data, string)) {
      lives_free((livespointer)xlist->data);
      if (xlist->prev) xlist->prev->next = xlist->next;
      else list = xlist;
      if (xlist->next) xlist->next->prev = xlist->prev;
      xlist->next = xlist->prev = NULL;
      lives_list_free(xlist);
      return list;
    }
  }
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_copy_strings(LiVESList * list) {
  // copy a list, copying the strings too
  LiVESList *xlist = NULL, *olist = list;
  while (olist) {
    xlist = lives_list_prepend(xlist, lives_strdup((char *)olist->data));
    olist = olist->next;
  }
  return lives_list_reverse(xlist);
}


boolean string_lists_differ(LiVESList * alist, LiVESList * blist) {
  // compare 2 lists of strings and see if they are different (ignoring ordering)
  // for long lists this would be quicker if we sorted the lists first; however this function
  // is designed to deal with short lists only

  LiVESList *plist, *rlist = blist;

  if (lives_list_length(alist) != lives_list_length(blist)) return TRUE; // check the simple case first

  // run through alist and see if we have a mismatch

  plist = alist;
  while (plist) {
    LiVESList *qlist = rlist;
    boolean matched = TRUE;
    while (qlist) {
      if (!(lives_utf8_strcasecmp((char *)plist->data, (char *)qlist->data))) {
        if (matched) rlist = qlist->next;
        break;
      }
      matched = FALSE;
      qlist = qlist->next;
    }
    if (!qlist) return TRUE;
    plist = plist->next;
  }

  // since both lists were of the same length, there is no need to check blist

  return FALSE;
}

