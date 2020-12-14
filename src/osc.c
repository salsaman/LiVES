// osc.c
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2016 <salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef IS_MINGW
#include <netinet/in.h>
#endif

#include "main.h"

#ifdef ENABLE_OSC

#include "osc.h"
#include "htmsocket.h"
#include "callbacks.h"
#include "effects.h"
#include "rte_window.h"
#include "resample.h"
#include "paramwindow.h"
#include "ce_thumbs.h"
#ifdef HAVE_UNICAP
#include "videodev.h"
#endif
#include "lbindings.h"

void *status_socket = NULL;
void *notify_socket = NULL;

static lives_osc *livesOSC = NULL;

static char constval[OSC_CONSTLEN];

static boolean via_shortcut = FALSE;

#define FX_MAX FX_KEYS_MAX_VIRTUAL-1

// TODO: replace mainw->osc_block with filter_mutex_lock()

static boolean osc_init_generator(livespointer data) {
  // do this via an idle function, as it will trigger playback and hang
  mainw->osc_auto = 1; ///< request notifiction of success
  rte_key_toggle(LIVES_POINTER_TO_INT(data));
  mainw->osc_auto = 0;
  return FALSE;
}


/* convert a big endian 32 bit string to an int for internal use */

static int toInt(const char *b) {
  if (capable->byte_order == LIVES_LITTLE_ENDIAN) {
    return (((int) b[3]) & 0xff) + ((((int) b[2]) & 0xff) << 8) + ((((int) b[1]) & 0xff) << 16) +
           ((((int) b[0]) & 0xff) << 24);
  }
  return (((int) b[0]) & 0xff) + ((((int) b[1]) & 0xff) << 8) + ((((int) b[2]) & 0xff) << 16) +
         ((((int) b[3]) & 0xff) << 24);
}


static boolean using_types;
static int osc_header_len;
static int offset;


static int lives_osc_get_num_arguments(const void *vargs) {
  // check if using type tags and get num_arguments
  const char *args = (const char *)vargs;
  if (args[0] != 0x2c) return 0;
  return strlen(args) - 1;
}


static boolean lives_osc_check_arguments(int arglen, const void *vargs, const char *check_pattern, boolean calc_header_len) {
  // check if using type tags and get header_len
  // should be called from each cb that uses parameters
  const char *args = (const char *)vargs;
  int header_len;

  osc_header_len = 0;
  offset = 0;

  using_types = FALSE;
  if (arglen < 4 || args[0] != 0x2c) return FALSE; // missing comma or typetags
  using_types = TRUE;

  header_len = pad4(strlen(check_pattern) + 1);

  if (arglen < header_len) return FALSE;
  if (!strncmp(check_pattern, ++args, strlen(check_pattern))) {
    if (calc_header_len) osc_header_len = header_len;
    return TRUE;
  }
  return FALSE;
}


/* not used yet */
/*static void lives_osc_parse_char_argument(const void *vargs, char *dst)
  {
  const char *args = (char*)vargs;
  strncpy(dst, args+osc_header_len+offset,1);
  offset+=4;
  }*/


static void lives_osc_parse_string_argument(const void *vargs, char *dst) {
  const char *args = (char *)vargs;
  lives_snprintf(dst, OSC_STRING_SIZE, "%s", args + osc_header_len + offset);
  offset += pad4(strlen(dst));
}


static void lives_osc_parse_int_argument(const void *vargs, int *arguments) {
  const char *args = (char *)vargs;
  arguments[0] = toInt(args + osc_header_len + offset);
  offset += 4;
}


static void lives_osc_parse_float_argument(const void *vargs, float *arguments) {
  const char *args = (char *)vargs;
  arguments[0] = LEFloat_to_BEFloat(*((float *)(args + osc_header_len + offset)));
  offset += 4;
}


/* memory allocation functions of libOMC_dirty (OSC) */
void *lives_osc_malloc(int num_bytes) {
  return lives_malloc(num_bytes);
}


// notifications

void lives_notify(int msgnumber, const char *msgstring) {
#ifdef IS_LIBLIVES
  binding_cb(msgnumber, msgstring, mainw->id);
#endif
#ifdef ENABLE_OSC
  lives_osc_notify(msgnumber, msgstring);
#endif

#ifdef TEST_NOTIFY
  if (msgnumber == LIVES_OSC_NOTIFY_CLIPSET_OPENED) {
    char *details = lives_strdup_printf(_("'LiVES opened the clip set' '%s'"), msgstring);
    char *tmp = lives_strdup_printf("notify-send %s", details);
    lives_system(tmp, TRUE);
    lives_free(tmp);
    lives_free(details);
  }

  if (msgnumber == LIVES_OSC_NOTIFY_CLIPSET_SAVED) {
    char *details = lives_strdup_printf(_("'LiVES saved the clip set' '%s'"), msgstring);
    char *tmp = lives_strdup_printf("notify-send %s", details);
    lives_system(tmp, TRUE);
    lives_free(tmp);
    lives_free(details);
  }
#endif
}


LIVES_GLOBAL_INLINE void lives_notify_int(int msgnumber, int msgint) {
  char *tmp = lives_strdup_printf("%d", msgint);
  lives_notify(msgnumber, tmp);
  lives_free(tmp);
}

// status returns

boolean lives_status_send(const char *msg) {
  if (!status_socket) return FALSE;
  else {
    // note we send the terminating \nul
    boolean retval = lives_stream_out(status_socket, strlen(msg) + 1, (void *)msg);
    return retval;
  }
}


boolean lives_osc_notify(int msgnumber, const char *msgstring) {
  if (!notify_socket) return FALSE;
  if (!prefs || (!prefs->omc_events && (msgnumber != LIVES_OSC_NOTIFY_SUCCESS
                                        && msgnumber != LIVES_OSC_NOTIFY_FAILED))) return FALSE;
  else {
    char *msg;
    boolean retval;
    if (msgstring) {
      msg = lives_strdup_printf("%d|%s\n", msgnumber, msgstring);
    } else msg = lives_strdup_printf("%d\n", msgnumber);
    retval = lives_stream_out(notify_socket, strlen(msg) + 1, (void *)msg);
    lives_free(msg);
    return retval;
  }
}


boolean lives_osc_notify_success(const char *msg) {
  if (prefs->omc_noisy)
    lives_osc_notify(LIVES_OSC_NOTIFY_SUCCESS, msg);
  return TRUE;
}


boolean lives_osc_notify_failure(void) {
  if (prefs->omc_noisy)
    lives_osc_notify(LIVES_OSC_NOTIFY_FAILED, NULL);
  return FALSE;
}

/* unused */
/*
  void lives_osc_notify_cancel (void) {
  if (prefs->omc_noisy);
  lives_osc_notify(LIVES_OSC_NOTIFY_CANCELLED,NULL);
  }*/


void lives_osc_close_status_socket(void) {
  if (status_socket) CloseHTMSocket(status_socket);
  status_socket = NULL;
}


void lives_osc_close_notify_socket(void) {
  if (notify_socket) CloseHTMSocket(notify_socket);
  notify_socket = NULL;
}


LIVES_INLINE const char *get_value_of(const int what) {
  lives_snprintf(constval, OSC_CONSTLEN, "%d", what);
  return (const char *)&constval;
}


static const char *get_omc_const(const char *cname) {
  // looping modes
  if (!strcmp(cname, "LIVES_LOOP_MODE_NONE")) return "0";
  if (!strcmp(cname, "LIVES_LOOP_MODE_CONTINUOUS")) return "1";
  if (!strcmp(cname, "LIVES_LOOP_MODE_FIT_AUDIO")) return "2";

  // interface modes
  if (!strcmp(cname, "LIVES_INTERFACE_MODE_CLIPEDIT")) return "0";
  if (!strcmp(cname, "LIVES_INTERFACE_MODE_MULTITRACK")) return "1";

  // status
  if (!strcmp(cname, "LIVES_STATUS_NOTREADY")) return "0";
  if (!strcmp(cname, "LIVES_STATUS_READY")) return "1";
  if (!strcmp(cname, "LIVES_STATUS_PLAYING")) return "2";
  if (!strcmp(cname, "LIVES_STATUS_PROCESSING")) return "3";
  if (!strcmp(cname, "LIVES_STATUS_PREVIEW")) return "4";

  // parameter types
  if (!strcmp(cname, "LIVES_PARAM_TYPE_INTEGER"))
    return get_value_of((const int)WEED_PARAM_INTEGER);
  if (!strcmp(cname, "LIVES_PARAM_TYPE_FLOAT"))
    return get_value_of((const int)WEED_PARAM_FLOAT);
  if (!strcmp(cname, "LIVES_PARAM_TYPE_BOOL"))
    return get_value_of((const int)WEED_PARAM_SWITCH);
  if (!strcmp(cname, "LIVES_PARAM_TYPE_STRING"))
    return get_value_of((const int)WEED_PARAM_TEXT);
  if (!strcmp(cname, "LIVES_PARAM_TYPE_COLOR"))
    return get_value_of((const int)WEED_PARAM_COLOR);

  // colorspaces
  if (!strcmp(cname, "LIVES_COLORSPACE_RGB_INT"))
    return "1";
  if (!strcmp(cname, "LIVES_COLORSPACE_RGBA_INT"))
    return "2";
  if (!strcmp(cname, "LIVES_COLORSPACE_RGB_FLOAT"))
    return "3";
  if (!strcmp(cname, "LIVES_COLORSPACE_RGBA_FLOAT"))
    return "4";

  // boolean values
  if (!strcmp(cname, "LIVES_TRUE")) return "1";
  if (!strcmp(cname, "LIVES_FALSE")) return "0";

  // parameter flags
  if (!strcmp(cname, "LIVES_PARAM_FLAGS_REINIT_ON_VALUE_CHANGE"))
    return get_value_of((const int)WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
  if (!strcmp(cname, "LIVES_PARAM_FLAGS_VARIABLE_SIZE"))
    return get_value_of((const int)WEED_PARAMETER_VARIABLE_SIZE);
  if (!strcmp(cname, "LIVES_PARAM_FLAGS_VALUE_PER_CHANNEL"))
    return get_value_of((const int)WEED_PARAMETER_VALUE_PER_CHANNEL);

  // notification types
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_SUCCESS"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_SUCCESS);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_FAILED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_FAILED);

  // notification events
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_FRAME_SYNCH"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_FRAME_SYNCH);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_PLAYBACK_STARTED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_PLAYBACK_STARTED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_PLAYBACK_STOPPED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_PLAYBACK_STOPPED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_PLAYBACK_STOPPED_RD"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_PLAYBACK_STOPPED_RD);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_RECORD_STARTED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_RECORD_STARTED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_RECORD_STOPPED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_RECORD_STOPPED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_QUIT"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_QUIT);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_CLIP_OPENED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CLIP_OPENED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_CLIP_CLOSED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CLIP_CLOSED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_CLIPSET_OPENED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CLIPSET_OPENED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_CLIPSET_SAVED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CLIPSET_SAVED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_SUCCESS"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_SUCCESS);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_FAILED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_FAILED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_CANCELLED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_CANCELLED);
  if (!strcmp(cname, "LIVES_OSC_NOTIFY_MODE_CHANGED"))
    return get_value_of((const int)LIVES_OSC_NOTIFY_MODE_CHANGED);

  // audio sources
  if (!strcmp(cname, "LIVES_AUDIO_SOURCE_INTERNAL"))
    return get_value_of((const int)AUDIO_SRC_INT);
  if (!strcmp(cname, "LIVES_AUDIO_SOURCE_EXTERNAL"))
    return get_value_of((const int)AUDIO_SRC_EXT);

  // generic constants
  if (!strcmp(cname, "LIVES_FPS_MAX"))
    return get_value_of((const int)FPS_MAX);

  if (!strcmp(cname, "LIVES_DEFAULT_OVERRIDDEN"))
    return "2";

  IGN_RET(lives_osc_notify_failure());

  return "";
}


static char *lives_osc_format_result(weed_plant_t *plant, const char *key, int st, int end) {
  int stype;
  int error, i;

  char *retval = NULL, *tmp;

  if (end == -1) end = weed_leaf_num_elements(plant, key);

  if (end <= st) return lives_strdup("");

  stype = weed_leaf_seed_type(plant, key);

  switch (stype) {
  case WEED_SEED_INT: {
    int *vals = weed_get_int_array(plant, key, &error);
    for (i = st; i < end; i++) {
      if (!retval) tmp = lives_strdup_printf("%d", vals[i]);
      else {
        tmp = lives_strdup_printf("%s,%d", retval, vals[i]);
        lives_free(retval);
      }
      retval = tmp;
    }
    lives_free(vals);
    break;
  }

  case WEED_SEED_DOUBLE: {
    double *vals = weed_get_double_array(plant, key, &error);
    for (i = st; i < end; i++) {
      if (!retval) tmp = lives_strdup_printf("%f", vals[i]);
      else {
        tmp = lives_strdup_printf("%s,%f", retval, vals[i]);
        lives_free(retval);
      }
      retval = tmp;
    }
    lives_free(vals);
    break;
  }

  case WEED_SEED_BOOLEAN: {
    int *vals = weed_get_boolean_array(plant, key, &error);
    for (i = st; i < end; i++) {
      if (!retval) tmp = lives_strdup_printf("%d", vals[i] == WEED_TRUE);
      else {
        tmp = lives_strdup_printf("%s,%d", retval, vals[i] == WEED_TRUE);
        lives_free(retval);
      }
      retval = tmp;
    }
    lives_free(vals);
    //g_print("get from %p %s %s\n", plant, key, tmp);

    break;
  }

  case WEED_SEED_STRING: {
    char **vals = weed_get_string_array(plant, key, &error);
    char *tmp2;
    for (i = st; i < end; i++) {
      if (!retval) tmp = lives_strdup_printf("\"%s\"", (tmp2 = subst(vals[i], "\"", "\\\"")));
      else {
        tmp = lives_strdup_printf("%s,\"%s\"", retval, (tmp2 = subst(vals[i], "\"", "\\\"")));
        lives_free(retval);
      }
      lives_free(tmp2);
      retval = tmp;
      lives_free(vals[i]);
    }
    lives_free(vals);
    break;
  }
  }
  return retval;
}


///////////////////////////////////// CALLBACKS FOR OSC ////////////////////////////////////////
// TODO - handle clipboard playback

boolean lives_osc_cb_test(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int val = lives_osc_get_num_arguments(vargs);
  lives_printerr("got %d\n", val);
  return TRUE;
}


/* /video/play */
boolean lives_osc_cb_play(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  float ent, stt;
  double entd, sttd;

  if (mainw->go_away) return lives_osc_notify_failure();
  mainw->osc_auto = 1; ///< request early notifiction of success

  if (mainw->current_file <= 0 || mainw->playing_file != -1) return lives_osc_notify_failure();

  mainw->play_start = calc_frame_from_time(mainw->current_file,
                      cfile->pointer_time);
  mainw->play_end = cfile->frames;

  if (!lives_osc_check_arguments(arglen, vargs, "ff", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "f", FALSE)) {
      if (!lives_osc_check_arguments(arglen, vargs, "", FALSE)) {
        return lives_osc_notify_failure();
      }
    } else {
      lives_osc_check_arguments(arglen, vargs, "f", TRUE);
      lives_osc_parse_float_argument(vargs, &stt);
      sttd = (double)stt;
      mainw->play_start = calc_frame_from_time(mainw->current_file,
                          sttd);
    }
  } else {
    lives_osc_check_arguments(arglen, vargs, "ff", TRUE);
    lives_osc_parse_float_argument(vargs, &stt);
    lives_osc_parse_float_argument(vargs, &ent);
    sttd = (double)stt;
    entd = (double)ent;
    mainw->play_end = calc_frame_from_time(mainw->current_file,
                                           entd);
    mainw->play_start = calc_frame_from_time(mainw->current_file,
                        sttd);
  }

  start_playback_async(3);

  return TRUE;
}


boolean lives_osc_cb_playsel(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) return lives_osc_notify_failure();
  if (!LIVES_IS_PLAYING && mainw->current_file > 0 && !mainw->is_processing) {
    start_playback_async(4);
  }
  return TRUE;
}


boolean lives_osc_cb_play_reverse(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!CURRENT_CLIP_IS_NORMAL || !LIVES_IS_PLAYING)
    return lives_osc_notify_failure();
  dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_bgplay_reverse(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!IS_NORMAL_CLIP(mainw->blend_file) || mainw->blend_file == mainw->current_file ||  !LIVES_IS_PLAYING)
    return lives_osc_notify_failure();

  mainw->files[mainw->blend_file]->pb_fps = -mainw->files[mainw->blend_file]->pb_fps;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_play_forward(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) return lives_osc_notify_failure(); // not ready to play yet

  if (!CURRENT_CLIP_IS_NORMAL)
    if (mainw->playing_file == 1) return lives_osc_notify_failure();

  if (!LIVES_IS_PLAYING && mainw->current_file > 0 && !mainw->is_processing) {
    start_playback_async(3);
    return TRUE;
  } else if (mainw->playing_file > 0) {
    if (cfile->pb_fps < 0 || (cfile->play_paused && cfile->freeze_fps < 0))
      dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
    if (cfile->play_paused) freeze_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);
    return lives_osc_notify_success(NULL);
  }

  return lives_osc_notify_failure();
}


boolean lives_osc_cb_play_backward(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!CURRENT_CLIP_IS_NORMAL) return lives_osc_notify_failure();

  if (!LIVES_IS_PLAYING && mainw->current_file > 0 && !mainw->is_processing) {
    mainw->reverse_pb = TRUE;
    start_playback_async(3);
    return TRUE;
  } else if (mainw->playing_file > 0) {
    if (cfile->pb_fps > 0 || (cfile->play_paused && cfile->freeze_fps > 0))
      dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
    if (cfile->play_paused) freeze_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);
    return lives_osc_notify_success(NULL);
  }

  return lives_osc_notify_failure();
}


boolean lives_osc_cb_play_faster(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!LIVES_IS_PLAYING) return lives_osc_notify_failure();

  on_faster_pressed(NULL, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_bgplay_faster(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(mainw->blend_file) || mainw->blend_file == mainw->current_file) return lives_osc_notify_failure();

  on_faster_pressed(NULL, LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND));
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_play_slower(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!LIVES_IS_PLAYING) return lives_osc_notify_failure();

  on_slower_pressed(NULL, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_bgplay_slower(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(mainw->blend_file) || mainw->blend_file == mainw->current_file) return lives_osc_notify_failure();

  on_slower_pressed(NULL, LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND));
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_play_reset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!LIVES_IS_PLAYING) return lives_osc_notify_failure();

  fps_reset_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);
  if (cfile->pb_fps < 0 || (cfile->play_paused &&
                            cfile->freeze_fps < 0)) dirchange_callback(NULL, NULL, 0,
                                  (LiVESXModifierType)0, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
  if (cfile->play_paused) freeze_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_bgplay_reset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(mainw->blend_file) || mainw->blend_file == mainw->current_file) return lives_osc_notify_failure();

  if (mainw->files[mainw->blend_file]->play_paused) {
    mainw->files[mainw->blend_file]->play_paused = FALSE;
  }

  if (mainw->files[mainw->blend_file]->pb_fps >= 0.) mainw->files[mainw->blend_file]->pb_fps =
      mainw->files[mainw->blend_file]->fps;
  else mainw->files[mainw->blend_file]->pb_fps = -mainw->files[mainw->blend_file]->fps;
  return lives_osc_notify_success(NULL);
}


/* /video/stop */
boolean lives_osc_cb_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (LIVES_IS_PLAYING) {
    on_stop_activate(NULL, NULL); // should send play stop event
    return lives_osc_notify_success(NULL);
  } else return lives_osc_notify_failure();
}


boolean lives_osc_cb_set_loop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int lmode;

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &lmode);
  } else return lives_osc_notify_failure();

  if (lmode == atoi(get_omc_const("LIVES_LOOP_MODE_NONE"))) {
    if (mainw->loop_cont) on_loop_button_activate(NULL, NULL);
    if (mainw->loop) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video), !mainw->loop);
  } else {
    if (lmode & atoi(get_omc_const("LIVES_LOOP_MODE_CONTINUOUS"))) {
      if (!mainw->loop_cont) on_loop_button_activate(NULL, NULL);
    } else if (mainw->loop_cont) on_loop_button_activate(NULL, NULL);

    if (lmode & atoi(get_omc_const("LIVES_LOOP_MODE_FIT_AUDIO"))) {
      if (!mainw->loop) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video), TRUE);
    } else if (mainw->loop) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video), FALSE);
  }

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_get_loop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int lmode = 0;
  char *lmodes;

  if (mainw->loop) lmode |= atoi(get_omc_const("LIVES_LOOP_MODE_FIT_AUDIO"));
  if (mainw->loop_cont) lmode |= atoi(get_omc_const("LIVES_LOOP_MODE_CONTINUOUS"));

  lmodes = lives_strdup_printf("%d", lmode);
  lives_status_send(lmodes);
  lives_free(lmodes);
  return TRUE;
}


boolean lives_osc_cb_set_pingpong(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int lmode;
  char *boolstr;

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &lmode);
    boolstr = lives_strdup_printf("%d", lmode);
    if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) lmode = TRUE;
    else {
      if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) lmode = FALSE;
      else {
        lives_free(boolstr);
        return lives_osc_notify_failure();
      }
    }
    lives_free(boolstr);
  } else return lives_osc_notify_failure();

  if ((lmode && !mainw->ping_pong) || (!lmode && mainw->ping_pong))
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_ping_pong), !mainw->ping_pong);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_get_pingpong(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->ping_pong) return lives_status_send(get_omc_const("LIVES_TRUE"));
  return lives_status_send(get_omc_const("LIVES_FALSE"));
}


boolean lives_osc_cb_set_fps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fpsi;
  float fps;
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &fpsi);
    fps = (float)fpsi;
  } else {
    if (!lives_osc_check_arguments(arglen, vargs, "f", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_float_argument(vargs, &fps);
  }

  if (LIVES_IS_PLAYING) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), (double)(fps));
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_bgset_fps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fpsi;
  float fps;

  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!IS_VALID_CLIP(mainw->blend_file) || mainw->blend_file == mainw->current_file) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &fpsi);
    fps = (float)fpsi;
  } else {
    if (!lives_osc_check_arguments(arglen, vargs, "f", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_float_argument(vargs, &fps);
  }

  mainw->files[mainw->blend_file]->pb_fps = (double)fps;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_set_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fpsi;
  float fps;
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &fpsi);
    fps = (float)fpsi;
  } else {
    if (!lives_osc_check_arguments(arglen, vargs, "f", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_float_argument(vargs, &fps);
  }

  if (LIVES_IS_PLAYING) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),
        (double)(fps)*mainw->files[mainw->playing_file]->fps);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_bgset_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int fpsi;
  float fps;
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(mainw->blend_file) || mainw->blend_file == mainw->current_file) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &fpsi);
    fps = (float)fpsi;
  } else {
    if (!lives_osc_check_arguments(arglen, vargs, "f", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_float_argument(vargs, &fps);
  }

  mainw->files[mainw->blend_file]->pb_fps = mainw->files[mainw->blend_file]->fps * (double)fps;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fx_reset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (!mainw->osc_block) rte_keys_reset();
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fx_map_clear(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (!mainw->osc_block) on_clear_all_clicked(NULL, NULL);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fx_map(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  char effect_name[OSC_STRING_SIZE];

  if (!lives_osc_check_arguments(arglen, vargs, "is", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_string_argument(vargs, effect_name);
  if (effect_key < 1 || effect_key >= prefs->rte_keys_virtual) return lives_osc_notify_failure();
  if (!mainw->osc_block) {
    weed_add_effectkey(effect_key, effect_name, FALSE); // allow partial matches
    return lives_osc_notify_success(NULL);
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_fx_unmap(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &mode);
  mode--;
  if (!mainw->osc_block && rte_keymode_valid(effect_key, mode, TRUE)) {
    int idx = effect_key * rte_getmodespk() + mode;
    on_clear_clicked(NULL, LIVES_INT_TO_POINTER(idx));
    return lives_osc_notify_success(NULL);
  }
  return lives_osc_notify_failure();
}


static boolean osc_fx_on(int effect_key) {
  int count;
  int grab = mainw->last_grabbable_effect;

  weed_plant_t *filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));

  if (!filter) return lives_osc_notify_failure();
  count = enabled_in_channels(filter, FALSE);
  if (!LIVES_IS_PLAYING && via_shortcut && count != 0) return lives_osc_notify_failure(); // is no generator

  if (!LIVES_IS_PLAYING && count == 0) {
    if (mainw->preview || (!mainw->multitrack && mainw->event_list) || mainw->is_processing ||
        mainw->multitrack) return lives_osc_notify_failure();
    mainw->error = FALSE;
    lives_timer_add_simple(0, osc_init_generator, LIVES_INT_TO_POINTER(effect_key));
    return TRUE;
  } else {
    rte_key_toggle(effect_key);
    mainw->last_grabbable_effect = grab;
  }
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fx_enable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // if via_shortcut and not playing, we ignore unless a generator starts (which starts playback)
  int effect_key;

  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  if (effect_key < 1 || effect_key >= prefs->rte_keys_virtual) return lives_osc_notify_failure();

  if (!mainw->osc_block) {
    if (!(mainw->rte & (GU641 << (effect_key - 1)))) {
      return osc_fx_on(effect_key);
    }
  } else return lives_osc_notify_failure();

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fx_disable(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  if (effect_key < 1 || effect_key >= prefs->rte_keys_virtual) return lives_osc_notify_failure();
  if (mainw->rte & (GU641 << (effect_key - 1))) {
    if (!mainw->osc_block) {
      rte_key_toggle(effect_key);
      return lives_osc_notify_success(NULL);
    }
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_fx_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;

  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  if (effect_key < 1 || effect_key >= prefs->rte_keys_virtual) return lives_osc_notify_failure();

  if (!(mainw->rte & (GU641 << (effect_key - 1)))) {
    return osc_fx_on(effect_key);
  }

  if (!mainw->osc_block) rte_key_toggle(effect_key);

  return lives_osc_notify_success(NULL);
}

// *_set will allow setting of invalid clips - in this case nothing happens
//*_select will index only valid clips


boolean lives_osc_cb_fgclip_set(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch fg clip

  int clip;
  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &clip);

  if (clip > 0 && clip < MAX_FILES - 1) {
    if (IS_NORMAL_CLIP(clip)) {
      if (mainw->playing_file != 0) {
        char *msg = lives_strdup_printf("%d", clip);
        switch_clip(1, clip, FALSE);
        lives_osc_notify_success(msg);
        lives_free(msg);
        return TRUE;
      }
    }
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_bgclip_set(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch bg clip

  int clip;
  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &clip);

  if (clip > 0 && clip < MAX_FILES - 1) {
    if (IS_NORMAL_CLIP(clip)) {
      char *msg = lives_strdup_printf("%d", clip);
      switch_clip(2, clip, FALSE);
      lives_osc_notify_success(msg);
      lives_free(msg);
      return TRUE;
    }
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_fgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // switch fg clip
  int clip, i;
  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &clip);

  if (clip < 1 || !mainw->cliplist) return lives_osc_notify_failure();

  if (mainw->scrap_file != -1 && clip >= mainw->scrap_file) clip++;
  if (mainw->ascrap_file != -1 && clip >= mainw->ascrap_file) clip++;

  if (clip > lives_list_length(mainw->cliplist)) return lives_osc_notify_failure();

  i = LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->cliplist, clip - 1));

  if (i == mainw->current_file) return lives_osc_notify_failure();
  if (mainw->playing_file != 0) {
    char *msg;
    switch_clip(1, i, FALSE);
    msg = lives_strdup_printf("%d", i);
    lives_osc_notify_success(msg);
    lives_free(msg);
    return TRUE;
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_bgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  // switch bg clip
  char *msg;
  int clip, i;

  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure(); // etc

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &clip);

  if (clip < 1 || !mainw->cliplist) return lives_osc_notify_failure();

  if (mainw->scrap_file != -1 && clip >= mainw->scrap_file) clip++;

  if (mainw->ascrap_file != -1 && clip >= mainw->ascrap_file) clip++;

  if (clip > lives_list_length(mainw->cliplist)) return lives_osc_notify_failure();

  if (mainw->num_tr_applied < 1) return lives_osc_notify_failure();

  i = LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->cliplist, clip - 1));

  if (i == mainw->blend_file) return lives_osc_notify_failure();

  switch_clip(2, i, FALSE);

  msg = lives_strdup_printf("%d", i);
  lives_osc_notify_success(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_clip_resample(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fps;
  float fpsf;
  double fpsd;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "f", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &fps);
    fpsd = (double)(fps * 1.);
  } else {
    lives_osc_check_arguments(arglen, vargs, "f", TRUE);
    lives_osc_parse_float_argument(vargs, &fpsf);
    fpsd = (double)fpsf;
  }

  if (fpsd < 1. && fpsd > FPS_MAX) return lives_osc_notify_failure();

  cfile->undo1_dbl = fpsd;

  on_resample_vid_ok(NULL, NULL);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clip_close(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int noaudio = 0;
  int clipno = mainw->current_file;
  int current_file = clipno;

  char *boolstr;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
  } else if (mainw->multitrack || !lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  boolstr = lives_strdup_printf("%d", noaudio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) noaudio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) noaudio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  if (!IS_VALID_CLIP(clipno) || (mainw->multitrack && clipno == mainw->multitrack->render_file))
    return lives_osc_notify_failure();

  if (clipno == current_file) current_file = -1;

  mainw->current_file = clipno;

  close_current_file(current_file);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clip_undo(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int clipno = mainw->current_file;
  int current_file = clipno;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || mainw->preview || mainw->event_list || mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  if (!IS_VALID_CLIP(clipno)) return lives_osc_notify_failure();

  if (!mainw->files[clipno]->undoable) return lives_osc_notify_failure();

  mainw->current_file = clipno;

  on_undo_activate(NULL, NULL);

  switch_to_file(mainw->current_file = 0, current_file);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clip_redo(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int clipno = mainw->current_file;
  int current_file = clipno;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || mainw->preview || mainw->event_list || mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  if (!IS_VALID_CLIP(clipno)) return lives_osc_notify_failure();

  if (!mainw->files[clipno]->redoable) return lives_osc_notify_failure();

  mainw->current_file = clipno;

  on_redo_activate(NULL, NULL);

  switch_to_file(mainw->current_file = 0, current_file);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fgclip_copy(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int noaudio = 0;
  int clipno = mainw->current_file;
  int start, end, current_file = clipno;
  boolean ccpd;

  char *boolstr;

  if (LIVES_IS_PLAYING || mainw->multitrack) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  boolstr = lives_strdup_printf("%d", noaudio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) noaudio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) noaudio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  if (!IS_NORMAL_CLIP(clipno) || clipno == 0) return lives_osc_notify_failure();

  mainw->current_file = clipno;
  start = cfile->start;
  end = cfile->end;

  cfile->start = 1;
  cfile->end = cfile->frames;

  ccpd = mainw->ccpd_with_sound;

  mainw->ccpd_with_sound = !noaudio;

  on_copy_activate(NULL, NULL);

  mainw->ccpd_with_sound = ccpd;

  cfile->start = start;
  cfile->end = end;

  mainw->current_file = current_file;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fgclipsel_rteapply(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  int clipno = mainw->current_file;
  int current_file = clipno;

  if (LIVES_IS_PLAYING || mainw->multitrack) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  if (!IS_NORMAL_CLIP(clipno) || clipno == 0) return lives_osc_notify_failure();

  mainw->current_file = clipno;

  on_realfx_activate(NULL, &mainw->rendered_fx[0]);

  mainw->current_file = current_file;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fgclipsel_copy(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int noaudio = 0;
  int clipno = mainw->current_file;
  int current_file = clipno;
  boolean ccpd;

  char *boolstr;

  if (LIVES_IS_PLAYING || mainw->multitrack) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  boolstr = lives_strdup_printf("%d", noaudio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) noaudio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) noaudio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  if (!IS_NORMAL_CLIP(clipno) || clipno == 0) return lives_osc_notify_failure();

  mainw->current_file = clipno;

  ccpd = mainw->ccpd_with_sound;

  mainw->ccpd_with_sound = !noaudio;

  on_copy_activate(NULL, NULL);

  mainw->ccpd_with_sound = ccpd;

  mainw->current_file = current_file;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fgclipsel_cut(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int noaudio = 0;
  int clipno = mainw->current_file;
  int current_file = clipno;
  boolean ccpd;

  char *boolstr;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  boolstr = lives_strdup_printf("%d", noaudio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) noaudio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) noaudio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  if (!IS_NORMAL_CLIP(clipno) || clipno == 0) return lives_osc_notify_failure();

  mainw->current_file = clipno;

  ccpd = mainw->ccpd_with_sound;

  mainw->ccpd_with_sound = !noaudio;

  mainw->osc_auto = 1;
  on_cut_activate(NULL, NULL);
  mainw->osc_auto = 0;

  mainw->ccpd_with_sound = ccpd;

  mainw->current_file = current_file;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fgclipsel_delete(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int noaudio = 0;
  int clipno = mainw->current_file;
  int current_file = clipno;
  boolean ccpd;

  char *boolstr;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  boolstr = lives_strdup_printf("%d", noaudio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) noaudio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) noaudio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  if (!IS_NORMAL_CLIP(clipno) || clipno == 0) return lives_osc_notify_failure();

  mainw->current_file = clipno;

  ccpd = mainw->ccpd_with_sound;

  mainw->ccpd_with_sound = !noaudio;

  mainw->osc_auto = 1;
  on_delete_activate(NULL, NULL);
  mainw->osc_auto = 0;

  mainw->ccpd_with_sound = ccpd;

  mainw->current_file = current_file;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clipbd_paste(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int noaudio = 0;
  boolean ccpd;
  char *boolstr;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!clipboard) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  boolstr = lives_strdup_printf("%d", noaudio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) noaudio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) noaudio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  ccpd = mainw->ccpd_with_sound;

  mainw->ccpd_with_sound = !noaudio;

  on_paste_as_new_activate(NULL, NULL);

  mainw->ccpd_with_sound = ccpd;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clipbd_insertb(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  int noaudio = 0;
  int times = 1;
  int clipno = mainw->current_file;
  int current_file = clipno;

  char *boolstr;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
    lives_osc_parse_int_argument(vargs, &times);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
    lives_osc_parse_int_argument(vargs, &times);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  boolstr = lives_strdup_printf("%d", noaudio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) noaudio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) noaudio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  if (!IS_NORMAL_CLIP(clipno) || clipno == 0) return lives_osc_notify_failure();

  if (times == 0 || times < -1) return lives_osc_notify_failure();

  mainw->current_file = clipno;

  mainw->insert_after = FALSE;

  if (clipboard->achans == 0 && cfile->achans == 0) noaudio = TRUE;

  mainw->fx1_bool = (times == -1); // fit to audio
  mainw->fx1_val = times;     // times to insert otherwise
  mainw->fx2_bool = !noaudio; // with audio

  on_insert_activate(NULL, NULL);

  mainw->current_file = current_file;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clipbd_inserta(void *context, int arglen, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra) {
  int noaudio = 0;
  int times = 1;
  int clipno = mainw->current_file;
  int current_file = clipno;

  char *boolstr;

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list)) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
    lives_osc_parse_int_argument(vargs, &times);
    lives_osc_parse_int_argument(vargs, &clipno);
  } else if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
    lives_osc_parse_int_argument(vargs, &times);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &noaudio);
  } else if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    return lives_osc_notify_failure();
  }

  boolstr = lives_strdup_printf("%d", noaudio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) noaudio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) noaudio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  if (!IS_NORMAL_CLIP(clipno) || clipno == 0) return lives_osc_notify_failure();

  if (times == 0 || times < -1) return lives_osc_notify_failure();

  mainw->current_file = clipno;

  mainw->insert_after = TRUE;

  if (clipboard->achans == 0 && cfile->achans == 0) noaudio = TRUE;

  mainw->fx1_bool = (times == -1); // fit to audio
  mainw->fx1_val = times;     // times to insert otherwise
  mainw->fx2_bool = !noaudio; // with audio

  mainw->fx1_start = 1;
  mainw->fx2_start = count_resampled_frames(clipboard->frames, clipboard->fps, cfile->fps);

  on_insert_activate(NULL, NULL);

  mainw->current_file = current_file;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fgclip_retrigger(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  // switch fg clip and reset framenumber

  if (mainw->playing_file < 1 || (mainw->preview || (mainw->event_list && !mainw->record)) ||
      mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();

  lives_osc_cb_fgclip_select(context, arglen, vargs, when, ra);

  if (cfile->pb_fps > 0. || (cfile->play_paused && cfile->freeze_fps > 0.)) cfile->frameno = cfile->last_frameno = 1;
  else cfile->frameno = cfile->last_frameno = cfile->frames;

#ifdef RT_AUDIO
  if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) {
    resync_audio(cfile->frameno);
  }
#endif
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_bgclip_retrigger(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  // switch bg clip and reset framenumber

  if (mainw->playing_file < 1 || (mainw->preview || (mainw->event_list && !mainw->record)) ||
      mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();

  lives_osc_cb_bgclip_select(context, arglen, vargs, when, ra);

  if (!IS_VALID_CLIP(mainw->blend_file) || mainw->blend_file == mainw->current_file) return lives_osc_notify_failure();

  if (mainw->files[mainw->blend_file]->pb_fps > 0. || (mainw->files[mainw->blend_file]->play_paused &&
      mainw->files[mainw->blend_file]->freeze_fps > 0.))
    mainw->files[mainw->blend_file]->frameno = mainw->files[mainw->blend_file]->last_frameno = 1;
  else mainw->files[mainw->blend_file]->frameno = mainw->files[mainw->blend_file]->last_frameno =
          mainw->files[mainw->blend_file]->frames;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fgclip_select_next(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  // switch fg clip

  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing) return lives_osc_notify_failure(); // TODO
  if (mainw->multitrack) return lives_osc_notify_failure();

  nextclip_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(1));

  if (!LIVES_IS_PLAYING && prefs->omc_noisy) {
    char *msg = lives_strdup_printf("%d", mainw->current_file);
    lives_osc_notify_success(msg);
    lives_free(msg);
    return TRUE;
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_bgclip_select_next(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  // switch bg clip

  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing) return lives_osc_notify_failure(); // TODO
  if (!IS_VALID_CLIP(mainw->blend_file)) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  nextclip_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(2));

  if (!LIVES_IS_PLAYING && prefs->omc_noisy) {
    char *msg = lives_strdup_printf("%d", mainw->blend_file);
    lives_osc_notify_success(msg);
    lives_free(msg);
    return TRUE;
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_fgclip_select_previous(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  // switch fg clip

  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing) return lives_osc_notify_failure(); // TODO
  if (mainw->multitrack) return lives_osc_notify_failure();

  prevclip_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(1));

  if (!LIVES_IS_PLAYING && prefs->omc_noisy) {
    char *msg = lives_strdup_printf("%d", mainw->current_file);
    lives_osc_notify_success(msg);
    lives_free(msg);
    return TRUE;
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_bgclip_select_previous(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  // switch bg clip

  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing) return lives_osc_notify_failure(); // TODO
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(mainw->blend_file)) return lives_osc_notify_failure();

  prevclip_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(2));

  if (!LIVES_IS_PLAYING) {
    char *msg = lives_strdup_printf("%d", mainw->blend_file);
    lives_osc_notify_success(msg);
    lives_free(msg);
    return TRUE;
  }
  return lives_osc_notify_failure();
}


boolean lives_osc_cb_quit(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  mainw->only_close = mainw->no_exit = FALSE;
  mainw->leave_recovery = FALSE;

  if (mainw->was_set) {
    on_save_set_activate(NULL, mainw->set_name);
  } else mainw->leave_files = FALSE;
  lives_exit(0);
  return TRUE;
}


boolean lives_osc_cb_getname(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  return lives_status_send(PACKAGE_NAME);
}


boolean lives_osc_cb_getversion(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  return lives_status_send(VERSION);
}


boolean lives_osc_cb_getstatus(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->go_away) return lives_status_send(get_omc_const("LIVES_STATUS_NOTREADY"));
  if (LIVES_IS_PLAYING) return lives_status_send(get_omc_const("LIVES_STATUS_PLAYING"));
  if (mainw->is_processing) return lives_status_send(get_omc_const("LIVES_STATUS_PROCESSING"));
  if ((mainw->preview || (!mainw->multitrack && mainw->event_list && (!mainw->record ||
                          !LIVES_IS_PLAYING)))) return lives_status_send(get_omc_const("LIVES_STATUS_PREVIEW"));
  return lives_status_send(get_omc_const("LIVES_STATUS_READY"));
}


boolean lives_osc_cb_getconst(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  const char *retval;
  char cname[OSC_STRING_SIZE];

  if (!lives_osc_check_arguments(arglen, vargs, "s", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_string_argument(vargs, cname);
  retval = get_omc_const(cname);
  return lives_status_send(retval);
}


boolean lives_osc_cb_open_status_socket(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  char host[OSC_STRING_SIZE];
  int port;

  if (!lives_osc_check_arguments(arglen, vargs, "si", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
    lives_snprintf(host, OSC_STRING_SIZE, "localhost");
  } else {
    lives_osc_check_arguments(arglen, vargs, "si", TRUE);
    lives_osc_parse_string_argument(vargs, host);
  }
  lives_osc_parse_int_argument(vargs, &port);

  if (status_socket) {
    LIVES_INFO("OMC status socket already opened");
    return lives_osc_notify_failure();
  }

  if (!(status_socket = OpenHTMSocket(host, port, TRUE))) {
    LIVES_WARN("Unable to open status socket !");
    return lives_osc_notify_failure();
  }

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_open_notify_socket(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  char host[OSC_STRING_SIZE];
  int port;

  if (!lives_osc_check_arguments(arglen, vargs, "si", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
    lives_snprintf(host, OSC_STRING_SIZE, "localhost");
  } else {
    lives_osc_check_arguments(arglen, vargs, "si", TRUE);
    lives_osc_parse_string_argument(vargs, host);
  }
  lives_osc_parse_int_argument(vargs, &port);

  if (notify_socket) {
    LIVES_INFO("OMC notify socket already opened");
    return lives_osc_notify_failure();
  }

  prefs->omc_noisy = FALSE; // default for confirms is OFF
  if (!(notify_socket = OpenHTMSocket(host, port, TRUE))) {
    LIVES_WARN("Unable to open notify socket !");
    return lives_osc_notify_failure();
  }
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_close_status_socket(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  lives_osc_close_status_socket();
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_notify_c(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int state;
  char *boolstr;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &state);
  boolstr = lives_strdup_printf("%d", state);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) state = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) state = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);
  prefs->omc_noisy = state;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_notify_e(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int state;
  char *boolstr;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &state);
  boolstr = lives_strdup_printf("%d", state);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) state = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) state = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);
  prefs->omc_events = state;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clip_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  lives_status_send((tmp = lives_strdup_printf("%d", mainw->clips_available)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_clip_goto(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int frame;
  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->playing_file < 1 ||
      mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &frame);

  if (frame < 1 || frame > cfile->frames || !CURRENT_CLIP_IS_NORMAL) return lives_osc_notify_failure();

  cfile->last_frameno = cfile->frameno = frame;

#ifdef RT_AUDIO
  if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) {
    resync_audio(frame);
  }
#endif
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clip_getframe(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;
  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list && !mainw->record)) ||
      mainw->playing_file < 1) return lives_status_send("0");
  else {
    lives_status_send((tmp = lives_strdup_printf("%d", mainw->actual_frame)));
    lives_free(tmp);
  }
  return TRUE;
}


boolean lives_osc_cb_clip_getfps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file < 1) return lives_osc_notify_failure();

  if (mainw->current_file < 0) lives_status_send((tmp = lives_strdup_printf("%.3f", 0.)));
  else if ((mainw->preview || (!mainw->multitrack && mainw->event_list && !mainw->record)) ||
           mainw->playing_file < 1) lives_status_send((tmp = lives_strdup_printf("%.3f", cfile->fps)));
  else lives_status_send((tmp = lives_strdup_printf("%.3f", cfile->pb_fps)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_clip_get_ifps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;
  lives_clip_t *sfile;
  int clip = mainw->current_file;

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clip);
  }

  if (!IS_VALID_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];
  if ((mainw->preview || (!mainw->multitrack && mainw->event_list && !mainw->record)) ||
      mainw->playing_file < 1) lives_status_send((tmp = lives_strdup_printf("%.3f", sfile->fps)));
  else lives_status_send((tmp = lives_strdup_printf("%.3f", sfile->pb_fps)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_get_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file < 1) return lives_osc_notify_failure();

  if (mainw->current_file < 0) lives_status_send((tmp = lives_strdup_printf("%.4f", 0.)));
  else if ((mainw->preview || (!mainw->multitrack && mainw->event_list && !mainw->record)) ||
           mainw->playing_file < 1) lives_status_send((tmp = lives_strdup_printf("%.4f", 1.)));
  else lives_status_send((tmp = lives_strdup_printf("%.4f", cfile->pb_fps / cfile->fps)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_bgget_fps_ratio(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file < 1) return lives_osc_notify_failure();

  if (!CURRENT_CLIP_IS_VALID || !IS_VALID_CLIP(mainw->blend_file))
    lives_status_send((tmp = lives_strdup_printf("%.4f", 0.)));
  else if ((mainw->preview || (!mainw->multitrack && mainw->event_list && !mainw->record)) ||
           mainw->playing_file < 1) lives_status_send((tmp = lives_strdup_printf("%.4f", 1.)));
  else lives_status_send((tmp = lives_strdup_printf("%.4f", mainw->files[mainw->blend_file]->pb_fps /
                                  mainw->files[mainw->blend_file]->fps)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_bgclip_getframe(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list && !mainw->record)) ||
      mainw->playing_file < 1 || !IS_VALID_CLIP(mainw->blend_file)) return lives_status_send("0");
  else {
    lives_status_send((tmp = lives_strdup_printf("%d", mainw->files[mainw->blend_file]->frameno)));
    lives_free(tmp);
  }
  return TRUE;
}


boolean lives_osc_cb_bgclip_getfps(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file < 1) return lives_osc_notify_failure();
  if (!IS_VALID_CLIP(mainw->blend_file))lives_status_send((tmp = lives_strdup_printf("%.3f", 0.)));
  else if ((mainw->preview || (!mainw->multitrack && mainw->event_list && !mainw->record)) ||
           mainw->playing_file < 1) lives_status_send((tmp = lives_strdup_printf("%.3f", mainw->files[mainw->blend_file]->fps)));
  else lives_status_send((tmp = lives_strdup_printf("%.3f", mainw->files[mainw->blend_file]->pb_fps)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_get_amute(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (!is_realtime_aplayer(prefs->audio_player)) {
    return lives_status_send(get_omc_const("LIVES_FALSE"));
  }
  if (!mainw->mute) return lives_status_send(get_omc_const("LIVES_FALSE"));
  else return lives_status_send(get_omc_const("LIVES_TRUE"));
}


boolean lives_osc_cb_set_amute(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int mute;
  char *boolstr;

  if (!is_realtime_aplayer(prefs->audio_player)) {
    return lives_osc_notify_failure();
  }

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &mute);
    boolstr = lives_strdup_printf("%d", mute);
    if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) mute = TRUE;
    else {
      if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) mute = FALSE;
      else {
        lives_free(boolstr);
        return lives_osc_notify_failure();
      }
    }
    lives_free(boolstr);
  } else return lives_osc_notify_failure();

  if ((mute && !mainw->mute) || (!mute && mainw->mute))
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio), !mainw->mute);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_set_avol(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  float vol;

  if (lives_osc_check_arguments(arglen, vargs, "f", TRUE)) {
    lives_osc_parse_float_argument(vargs, &vol);
  } else return lives_osc_notify_failure();

  if (vol < 0. || vol > 1.) return lives_osc_notify_failure();

  pref_factory_float(PREF_MASTER_VOLUME, vol, TRUE);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_get_avol(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (!is_realtime_aplayer(prefs->audio_player)) {
    tmp = lives_strdup("100.00");
    lives_status_send(tmp);
    lives_free(tmp);
    return TRUE;
  }

  tmp = lives_strdup_printf("%.2f", future_prefs->volume);
  lives_status_send(tmp);
  lives_free(tmp);

  return TRUE;
}


boolean lives_osc_cb_getmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_status_send(get_omc_const("LIVES_MODE_MULTITRACK"));
  else return lives_status_send(get_omc_const("LIVES_MODE_CLIPEDIT"));
}


boolean lives_osc_cb_setmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *modes;
  int mode;
  int cliped = atoi(get_omc_const("LIVES_MODE_CLIPEDIT")); // 0
  int mt = atoi(get_omc_const("LIVES_MODE_MULTITRACK")); // 1

  if ((mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &mode);

  if (mode != cliped && mode != mt) return lives_osc_notify_failure();

  // these two will also send a status changed message
  if (mode == mt && !mainw->multitrack) on_multitrack_activate(NULL, NULL);
  else if (mode == cliped && mainw->multitrack) multitrack_delete(mainw->multitrack, FALSE);

  modes = lives_strdup_printf("%d", mode);
  lives_osc_notify_success(modes);
  lives_free(modes);
  return TRUE;
}


boolean lives_osc_cb_clearlay(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (LIVES_IS_PLAYING || (mainw->preview || (mainw->event_list && !mainw->record)) || mainw->is_processing ||
      !mainw->multitrack) return lives_osc_notify_failure();
  wipe_layout(mainw->multitrack);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_blockcount(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;
  int nblocks;
  int track;
  if (!mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &track);
  } else if (lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
    track = mainw->multitrack->current_track;
  } else return lives_osc_notify_failure();

  nblocks = mt_get_block_count(mainw->multitrack, track);

  tmp = lives_strdup_printf("%d", nblocks);
  lives_status_send(tmp);
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_blockinsert(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int ins_audio, oins_audio;
  int ign_ins_sel, oign_ins_sel;

  int clip;

  char *tmp;
  char *boolstr;

  if (LIVES_IS_PLAYING || (mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      !mainw->multitrack) return lives_osc_notify_failure();

  oins_audio = ins_audio = mainw->multitrack->opts.insert_audio;
  oign_ins_sel = ign_ins_sel = mainw->multitrack->opts.ign_ins_sel;

  if (!ign_ins_sel) ign_ins_sel = 0;
  else ign_ins_sel = 1;

  if (!ins_audio) ins_audio = 0;
  else ins_audio = 1;

  if (lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &clip);
    lives_osc_parse_int_argument(vargs, &ign_ins_sel);
    lives_osc_parse_int_argument(vargs, &ins_audio);
  }
  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &clip);
    lives_osc_parse_int_argument(vargs, &ign_ins_sel);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", TRUE)) {
    lives_osc_parse_int_argument(vargs, &clip);
  } else return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(clip) || clip == mainw->current_file || clip == mainw->scrap_file ||
      clip == mainw->ascrap_file)
    return lives_osc_notify_failure();

  boolstr = lives_strdup_printf("%d", ins_audio);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) ins_audio = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) ins_audio = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  boolstr = lives_strdup_printf("%d", ign_ins_sel);
  if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) ign_ins_sel = TRUE;
  else {
    if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) ign_ins_sel = FALSE;
    else {
      lives_free(boolstr);
      return lives_osc_notify_failure();
    }
  }
  lives_free(boolstr);

  mainw->multitrack->clip_selected = clip - 1;
  mt_clip_select(mainw->multitrack, TRUE);

  mainw->multitrack->opts.insert_audio = ins_audio;
  mainw->multitrack->opts.ign_ins_sel = ign_ins_sel;

  multitrack_insert(NULL, mainw->multitrack);

  mainw->multitrack->opts.insert_audio = oins_audio;
  mainw->multitrack->opts.ign_ins_sel = oign_ins_sel;

  tmp = lives_strdup_printf("%lu", mt_get_last_block_uid(mainw->multitrack));

  lives_osc_notify_success(tmp);
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_mtctimeset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  float time;
  char *msg;

  if (LIVES_IS_PLAYING || (mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      !mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "f", TRUE)) {
    lives_osc_parse_float_argument(vargs, &time);
  } else return lives_osc_notify_failure();

  if (time < 0.) return lives_osc_notify_failure();

  time = q_dbl(time, mainw->multitrack->fps) / TICKS_PER_SECOND_DBL;
  mt_tl_move(mainw->multitrack, time);

  msg = lives_strdup_printf("%.8f", lives_ruler_get_value(LIVES_RULER(mainw->multitrack->timeline)));
  lives_osc_notify_success(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_mtctimeget(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *msg;

  if (!mainw->multitrack) return lives_osc_notify_failure();

  msg = lives_strdup_printf("%.8f", lives_ruler_get_value(LIVES_RULER(mainw->multitrack->timeline)));
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_mtctrackset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int track;
  char *msg;

  if (LIVES_IS_PLAYING || (mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      !mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", TRUE)) {
    lives_osc_parse_int_argument(vargs, &track);
  } else return lives_osc_notify_failure();

  if (mt_track_is_video(mainw->multitrack, track) || mt_track_is_audio(mainw->multitrack, track)) {
    mainw->multitrack->current_track = track;
    track_select(mainw->multitrack);
    msg = lives_strdup_printf("%d", track);
    lives_osc_notify_success(msg);
    lives_free(msg);
    return TRUE;
  } else return lives_osc_notify_failure();
}


boolean lives_osc_cb_mtctrackget(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *msg;
  if (!mainw->multitrack) return lives_osc_notify_failure();

  msg = lives_strdup_printf("%d", mainw->multitrack->current_track);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_blockstget(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int track;
  int nblock;
  double sttime;
  char *tmp;
  if (!mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &track);
    lives_osc_parse_int_argument(vargs, &nblock);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", TRUE)) {
    track = mainw->multitrack->current_track;
    lives_osc_parse_int_argument(vargs, &nblock);
  } else return lives_osc_notify_failure();

  if (mt_track_is_video(mainw->multitrack, track) || mt_track_is_audio(mainw->multitrack, track)) {
    sttime = mt_get_block_sttime(mainw->multitrack, track, nblock);
    if (sttime < 0.) return lives_osc_notify_failure();
    tmp = lives_strdup_printf("%.8f", sttime);
    lives_status_send(tmp);
    lives_free(tmp);
    return TRUE;
  }
  return lives_osc_notify_failure(); ///< invalid track
}


boolean lives_osc_cb_blockenget(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int track;
  int nblock;
  double entime;
  char *tmp;
  if (!mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &track);
    lives_osc_parse_int_argument(vargs, &nblock);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", TRUE)) {
    track = mainw->multitrack->current_track;
    lives_osc_parse_int_argument(vargs, &nblock);
  } else return lives_osc_notify_failure();

  if (mt_track_is_video(mainw->multitrack, track) || mt_track_is_audio(mainw->multitrack, track)) {
    entime = mt_get_block_entime(mainw->multitrack, track, nblock);
    if (entime < 0.) return lives_osc_notify_failure();
    tmp = lives_strdup_printf("%.8f", entime);
    lives_status_send(tmp);
    lives_free(tmp);
    return TRUE;
  }
  return lives_osc_notify_failure(); ///< invalid track
}


boolean lives_osc_cb_get_playtime(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list && !mainw->record)) ||
      mainw->playing_file < 1) return lives_osc_notify_failure();

  lives_status_send((tmp = lives_strdup_printf("%.8f", (double)mainw->currticks / TICKS_PER_SECOND_DBL)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_bgclip_goto(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int frame;
  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && !mainw->record)) || mainw->playing_file < 1 ||
      mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(mainw->blend_file)) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &frame);

  if (frame < 1 || frame > mainw->files[mainw->blend_file]->frames ||
      !IS_NORMAL_CLIP(mainw->blend_file)) return lives_osc_notify_failure();

  mainw->files[mainw->blend_file]->last_frameno = mainw->files[mainw->blend_file]->frameno = frame;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clip_get_current(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  char *tmp;
  lives_status_send((tmp = lives_strdup_printf("%d", mainw->current_file < 0 ? 0 : mainw->current_file)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_bgclip_get_current(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  char *tmp;

  if (mainw->multitrack) return lives_osc_notify_failure();
  lives_status_send((tmp = lives_strdup_printf("%d", mainw->blend_file < 0 ? 0 : mainw->blend_file)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_clip_set_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *msg;

  int current_file = mainw->current_file;
  int clip = current_file;
  int frame;

  boolean selwidth_locked = mainw->selwidth_locked;

  lives_clip_t *sfile;

  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &frame);
    lives_osc_parse_int_argument(vargs, &clip);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", TRUE)) {
    lives_osc_parse_int_argument(vargs, &frame);
  } else return lives_osc_notify_failure();

  if (frame < 1 || !IS_NORMAL_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];

  if (frame > sfile->frames) frame = sfile->frames;

  mainw->selwidth_locked = FALSE;
  if (clip == mainw->current_file) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), frame);
  else {
    if (sfile->end < frame) sfile->end = frame;
    sfile->start = frame;
  }
  mainw->selwidth_locked = selwidth_locked;

  msg = lives_strdup_printf("%d", frame);
  lives_osc_notify_success(msg);
  lives_free(msg);

  return TRUE;
}


boolean lives_osc_cb_clip_get_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file = mainw->current_file;
  int clip = current_file;
  char *tmp;

  lives_clip_t *sfile;

  if (mainw->current_file < 1) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clip);
  }

  if (!IS_VALID_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];

  lives_status_send((tmp = lives_strdup_printf("%d", sfile->start)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_clip_set_end(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *msg;

  int current_file = mainw->current_file;
  int clip = current_file;
  int frame;

  boolean selwidth_locked = mainw->selwidth_locked;

  lives_clip_t *sfile;

  if (mainw->current_file < 1 || (mainw->preview || (mainw->event_list && (!mainw->record || !LIVES_IS_PLAYING))) ||
      mainw->is_processing) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &frame);
    lives_osc_parse_int_argument(vargs, &clip);
  } else if (lives_osc_check_arguments(arglen, vargs, "i", TRUE)) {
    lives_osc_parse_int_argument(vargs, &frame);
  } else return lives_osc_notify_failure();

  if (frame < 1 || !IS_NORMAL_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];

  if (frame > sfile->frames) frame = sfile->frames;

  mainw->selwidth_locked = FALSE;
  if (clip == mainw->current_file) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), frame);
  else {
    if (sfile->start > frame) sfile->start = frame;
    sfile->end = frame;
  }
  mainw->selwidth_locked = selwidth_locked;

  msg = lives_strdup_printf("%d", frame);
  lives_osc_notify_success(msg);
  lives_free(msg);

  return TRUE;
}


boolean lives_osc_cb_clip_get_end(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file = mainw->current_file;
  int clip = current_file;
  char *tmp;

  lives_clip_t *sfile;

  if (mainw->current_file < 1) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clip);
  }

  if (!IS_VALID_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];

  lives_status_send((tmp = lives_strdup_printf("%d", sfile->end)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_clip_get_size(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file = mainw->current_file;
  int clip = current_file;
  char *tmp;

  lives_clip_t *sfile;

  if (mainw->current_file < 1) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clip);
  }

  if (!IS_VALID_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];

  lives_status_send((tmp = lives_strdup_printf("%d|%d", sfile->hsize, sfile->vsize)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_clip_get_name(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file = mainw->current_file;
  int clip = current_file;

  lives_clip_t *sfile;

  if (mainw->current_file < 1) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clip);
  } else if (mainw->multitrack) return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];

  return lives_status_send(sfile->name);
}


boolean lives_osc_cb_clip_set_name(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int current_file = mainw->current_file;
  int clip = current_file;
  char name[OSC_STRING_SIZE];

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list && (!mainw->record ||
                                  !LIVES_IS_PLAYING))) ||
      mainw->is_processing) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "si", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "si", TRUE);
    lives_osc_parse_string_argument(vargs, name);
    lives_osc_parse_int_argument(vargs, &clip);
  } else if (lives_osc_check_arguments(arglen, vargs, "s", TRUE)) {
    if (mainw->multitrack) return lives_osc_notify_failure();
    lives_osc_parse_string_argument(vargs, name);
  } else return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(clip)) return lives_osc_notify_failure();

  mainw->current_file = clip;
  on_rename_clip_name(NULL, (livespointer)name);

  if (clip == current_file) set_main_title(name, 0);
  else mainw->current_file = current_file;

  return lives_osc_notify_success(name);
}


boolean lives_osc_cb_clip_get_frames(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int current_file = mainw->current_file;
  int clip = current_file;
  char *tmp;

  lives_clip_t *sfile;

  if (mainw->current_file < 1) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &clip);
  } else if (mainw->multitrack) return lives_osc_notify_failure();

  if (!IS_VALID_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];

  lives_status_send((tmp = lives_strdup_printf("%d", sfile->frames)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_clip_save_frame(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int current_file = mainw->current_file;
  int clip = current_file;
  int frame, width = -1, height = -1;
  char fname[OSC_STRING_SIZE];
  boolean retval;

  lives_clip_t *sfile;

  if (mainw->current_file < 1) return lives_osc_notify_failure();
  if (mainw->multitrack) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "is", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "iis", FALSE)) {
      if (!lives_osc_check_arguments(arglen, vargs, "isii", FALSE)) {
        if (!lives_osc_check_arguments(arglen, vargs, "iisii", TRUE)) {
          return lives_osc_notify_failure();
        }
        lives_osc_parse_int_argument(vargs, &frame);
        lives_osc_parse_int_argument(vargs, &clip);
        lives_osc_parse_string_argument(vargs, fname);
        lives_osc_parse_int_argument(vargs, &width);
        lives_osc_parse_int_argument(vargs, &height);
      } else {
        lives_osc_check_arguments(arglen, vargs, "isii", TRUE);
        if (mainw->multitrack) return lives_osc_notify_failure();
        lives_osc_parse_int_argument(vargs, &frame);
        lives_osc_parse_string_argument(vargs, fname);
        lives_osc_parse_int_argument(vargs, &width);
        lives_osc_parse_int_argument(vargs, &height);
      }
    } else {
      lives_osc_check_arguments(arglen, vargs, "iis", TRUE);
      lives_osc_parse_int_argument(vargs, &frame);
      lives_osc_parse_int_argument(vargs, &clip);
      lives_osc_parse_string_argument(vargs, fname);
    }
  } else {
    if (mainw->multitrack) return lives_osc_notify_failure();
    lives_osc_check_arguments(arglen, vargs, "is", TRUE);
    lives_osc_parse_int_argument(vargs, &frame);
    lives_osc_parse_string_argument(vargs, fname);
  }

  if (frame < 1 || !IS_NORMAL_CLIP(clip)) return lives_osc_notify_failure();

  sfile = mainw->files[clip];

  if (frame > sfile->frames) return lives_osc_notify_failure();

  retval = save_frame_inner(clip, frame, fname, width, height, TRUE);

  if (retval) return lives_osc_notify_success(NULL);
  else return lives_osc_notify_failure();
}


boolean lives_osc_cb_clip_select_all(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  boolean selwidth_locked = mainw->selwidth_locked;

  if (mainw->current_file < 1 || (mainw->preview || (!mainw->multitrack && mainw->event_list && (!mainw->record ||
                                  !LIVES_IS_PLAYING))) ||
      mainw->is_processing) return lives_osc_notify_failure();
  if (!CURRENT_CLIP_IS_NORMAL || !CURRENT_CLIP_HAS_VIDEO) return lives_osc_notify_failure();

  mainw->selwidth_locked = FALSE;
  on_select_all_activate(NULL, NULL);
  mainw->selwidth_locked = selwidth_locked;

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_clip_isvalid(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int clip;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &clip);

  if (IS_VALID_CLIP(clip) && !(mainw->multitrack &&
                               clip == mainw->multitrack->render_file) &&
      clip != mainw->scrap_file)
    return lives_status_send(get_omc_const("LIVES_TRUE"));
  else return lives_status_send(get_omc_const("LIVES_FALSE"));
}


boolean lives_osc_cb_rte_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // count realtime effects - (only those assigned to keys for now)
  char *tmp;
  lives_status_send((tmp = lives_strdup_printf("%d", prefs->rte_keys_virtual)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_rteuser_count(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  // count realtime effects
  char *tmp;
  lives_status_send((tmp = lives_strdup_printf("%d", FX_MAX)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_fssepwin_enable(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  if (!mainw->sep_win) {
    on_sepwin_pressed(NULL, NULL);
  }

  if (!mainw->fs) {
    on_full_screen_pressed(NULL, NULL);
  }
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fssepwin_disable(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  if (mainw->fs) {
    on_full_screen_pressed(NULL, NULL);
  }
  if (mainw->sep_win) {
    on_sepwin_pressed(NULL, NULL);
  }
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_op_fps_set(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int fps;
  float fpsf;
  double fpsd;
  char *msg;

  if (mainw->fixed_fpsd > 0.) return lives_osc_notify_failure();
  if (!lives_osc_check_arguments(arglen, vargs, "f", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &fps);
    fpsd = (double)(fps * 1.);
  } else {
    lives_osc_check_arguments(arglen, vargs, "f", TRUE);
    lives_osc_parse_float_argument(vargs, &fpsf);
    fpsd = (double)fpsf;
  }
  if (fpsd > 0. && fpsd <= FPS_MAX) {
    mainw->fixed_fpsd = fpsd;
    d_print(_("Syncing to external framerate of %.8f frames per second.\n"), fpsd);
  } else {
    if (fpsd == 0.) mainw->fixed_fpsd = -1.; ///< 0. to release
    else return lives_osc_notify_failure();
  }
  msg = lives_strdup_printf("%.3f", fpsd);
  lives_osc_notify_success(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_pref_set_audio_source(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  int val;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &val);
  if (val != AUDIO_SRC_INT && val != AUDIO_SRC_EXT) return lives_osc_notify_failure();
  pref_factory_bool(PREF_REC_EXT_AUDIO, val == AUDIO_SRC_EXT, FALSE);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_pref_get_audio_source(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  if (prefs->audio_src == AUDIO_SRC_EXT) return lives_status_send(get_omc_const("LIVES_AUDIO_SOURCE_EXTERNAL"));
  return lives_status_send(get_omc_const("LIVES_AUDIO_SOURCE_INTERNAL"));
}


boolean lives_osc_cb_freeze(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->playing_file < 1) return lives_osc_notify_failure();

  if (!mainw->osc_block) {
    freeze_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);
  }
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_op_nodrope(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  prefs->noframedrop = TRUE;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_op_nodropd(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  prefs->noframedrop = FALSE;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_fx_getname(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *retval;
  int fidx;

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &fidx);
  } else return lives_osc_notify_failure();

  retval = make_weed_hashname(fidx, FALSE, FALSE, 0, FALSE);

  lives_status_send(retval);

  lives_free(retval);

  return TRUE;
}


boolean lives_osc_cb_clip_encodeas(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char fname[OSC_STRING_SIZE];

  if (LIVES_IS_PLAYING || mainw->current_file < 1) return lives_osc_notify_failure();

  if (mainw->preview || mainw->event_list || mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (!cfile || cfile->opening) return lives_osc_notify_failure();

  mainw->osc_enc_width = mainw->osc_enc_height = 0;
  mainw->osc_enc_fps = 0.;

  if (!lives_osc_check_arguments(arglen, vargs, "siif", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "sii", FALSE)) {
      if (!lives_osc_check_arguments(arglen, vargs, "sf", FALSE)) {
        if (!lives_osc_check_arguments(arglen, vargs, "s", TRUE))
          return lives_osc_notify_failure();
        lives_osc_parse_string_argument(vargs, fname);
      }
      lives_osc_check_arguments(arglen, vargs, "sf", TRUE);
      lives_osc_parse_string_argument(vargs, fname);
      lives_osc_parse_float_argument(vargs, &mainw->osc_enc_fps);
    } else {
      lives_osc_check_arguments(arglen, vargs, "sii", TRUE);
      lives_osc_parse_string_argument(vargs, fname);
      lives_osc_parse_int_argument(vargs, &mainw->osc_enc_width);
      lives_osc_parse_int_argument(vargs, &mainw->osc_enc_height);
    }
  } else {
    lives_osc_check_arguments(arglen, vargs, "siif", TRUE);
    lives_osc_parse_string_argument(vargs, fname);
    lives_osc_parse_int_argument(vargs, &mainw->osc_enc_width);
    lives_osc_parse_int_argument(vargs, &mainw->osc_enc_height);
    lives_osc_parse_float_argument(vargs, &mainw->osc_enc_fps);
  }

  if (cfile->frames == 0) {
    // TODO
    on_export_audio_activate(NULL, NULL);
    return lives_osc_notify_success(NULL);
  }

  mainw->osc_auto = 1;
  save_file(mainw->current_file, 1, cfile->frames, fname);
  mainw->osc_auto = 0;
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_setmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &mode);
  if (effect_key < 1 || effect_key >= FX_KEYS_MAX_VIRTUAL || mode < 1 ||
      mode > rte_getmodespk()) return lives_osc_notify_failure();
  if (!mainw->osc_block) rte_key_setmode(effect_key, mode - 1);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_nextmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  if (effect_key < 1 || effect_key >= FX_KEYS_MAX_VIRTUAL) return lives_osc_notify_failure();
  if (!mainw->osc_block) rte_key_setmode(effect_key, -1);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_prevmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  if (effect_key < 1 || effect_key >= FX_KEYS_MAX_VIRTUAL) return lives_osc_notify_failure();
  if (!mainw->osc_block) rte_key_setmode(effect_key, -2);

  return lives_osc_notify_success(NULL);
}


///////////////////////////////////////////////////////////////

static boolean setfx(weed_plant_t *plant, weed_plant_t *tparam, int pnum, int nargs, const void *vargs, int skip) {
  // set value of "value" leaf for tparam, or "host_default" for a template
  // set it to vargs (length nargs)

  weed_plant_t *ptmpl, *inst = NULL;

  float valuef;

  int valuei;
  int i;
  int ptype, cspace = -1;
  int x = 0;
  //int copyto = -1;
  int defargs;
  int maxi_r = 255, maxi_g = 255, maxi_b = 255, maxi_a = 255, mini_r = 0, mini_g = 0, mini_b = 0, mini_a = 0, mini, maxi;
  int key = -1;

  double maxd_r = 1., maxd_g = 1., maxd_b = 1., maxd_a = 1., mind_r = 0., mind_g = 0., mind_b = 0., mind_a = 0., mind, maxd;
  char values[OSC_STRING_SIZE];
  const char *pattern;

  if (nargs <= 0) return FALSE; // must set at least one value

  if (WEED_PLANT_IS_FILTER_CLASS(plant)) {
    ptmpl = tparam;
  } else {
    ptmpl = weed_get_plantptr_value(tparam, WEED_LEAF_TEMPLATE, NULL);
    inst = plant;
    if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
  }

  ptype = weed_paramtmpl_get_type(ptmpl);
  if (ptype == WEED_PARAM_COLOR) cspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, NULL);

  if (!(weed_parameter_has_variable_elements_strict(inst, ptmpl))) {
    if (nargs > (defargs = weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT))) {
      if (!(ptype == WEED_PARAM_COLOR && defargs == 1 && ((cspace == WEED_COLORSPACE_RGB && (nargs % 3 == 0)) ||
            (cspace == WEED_COLORSPACE_RGBA &&
             !(nargs & 3)))))
        // error: parameter does not have variable elements, and the user sent too many values
        return FALSE;
    }
  }

  pattern = (char *)vargs + skip; // skip comma,int,(int)

  switch (ptype) {
  case WEED_PARAM_INTEGER: {
    int *valuesi = (int *)lives_malloc(nargs * sizint);

    while (pattern[x] != 0) {
      if (pattern[x] == 'f') {
        // we wanted an int but we got a float
        //so we will round to the nearest value
        lives_osc_parse_float_argument(vargs, &valuef);
        valuesi[x] = myround(valuef);
      } else if (pattern[x] == 'i') {
        lives_osc_parse_int_argument(vargs, &valuesi[x]);
      } else {
        lives_free(valuesi);
        return FALSE;
      }
      mini = weed_get_int_value(ptmpl, WEED_LEAF_MIN, NULL);
      maxi = weed_get_int_value(ptmpl, WEED_LEAF_MAX, NULL);

      if (valuesi[x] < mini) valuesi[x] = mini;
      if (valuesi[x] > maxi) valuesi[x] = maxi;
      x++;
    }

    if (inst) {
      if (!filter_mutex_trylock(key)) {
        //copyto = set_copy_to(inst, pnum, FALSE);
        if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
          //if (copyto != -1) rec_param_change(inst, copyto);
        }
        weed_set_int_array(tparam, WEED_LEAF_VALUE, nargs, valuesi);
        //set_copy_to(inst, pnum, TRUE);

        if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
          //if (copyto != -1) rec_param_change(inst, copyto);
        }
        filter_mutex_unlock(key);
      }
    } else {
      weed_set_int_array(tparam, WEED_LEAF_HOST_DEFAULT, nargs, valuesi);
    }

    lives_free(valuesi);

    break;
  }

  case WEED_PARAM_SWITCH: {
    int group = 0;
    int *valuesb = (int *)lives_malloc(nargs * sizint);

    while (pattern[x] != 0) {
      if (pattern[x] == 'i') {
        lives_osc_parse_int_argument(vargs, &valuesb[x]);
      } else {
        lives_free(valuesb);
        return FALSE;
      }
      valuesb[x] = !!valuesb[x];
      x++;
    }

    if (weed_plant_has_leaf(ptmpl, WEED_LEAF_GROUP))
      group = weed_get_int_value(ptmpl, WEED_LEAF_GROUP, NULL);

    if (group != 0 && valuesb[0] == WEED_FALSE) goto grpinvalid;

    if (inst) {
      if (!filter_mutex_trylock(key)) {
        weed_set_boolean_array(tparam, WEED_LEAF_VALUE, nargs, valuesb);
        //copyto = set_copy_to(inst, pnum, TRUE);
        if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
          //if (copyto != -1) rec_param_change(inst, copyto);
        }
        filter_mutex_unlock(key);
      }

      if (group != 0) {
        // set all other values in group to WEED_FALSE
        weed_plant_t *filter = weed_instance_get_filter(inst, FALSE), *xtparam;
        int nparams = num_in_params(filter, FALSE, TRUE);

        for (pnum = 0; pnum < nparams; pnum++) {
          xtparam = weed_inst_in_param(inst, pnum, FALSE, TRUE);

          if (xtparam != tparam) {
            ptmpl = weed_param_get_template(xtparam);
            ptype = weed_paramtmpl_get_type(ptmpl);
            if (ptype == WEED_PARAM_SWITCH) {
              int xgroup = 0;

              if (weed_plant_has_leaf(ptmpl, WEED_LEAF_GROUP))
                xgroup = weed_get_int_value(ptmpl, WEED_LEAF_GROUP, NULL);

              if (xgroup == group) {
                if (!filter_mutex_trylock(key)) {
                  weed_set_boolean_value(xtparam, WEED_LEAF_VALUE, WEED_FALSE);
                  //copyto = set_copy_to(inst, pnum, TRUE);
                  if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
                    // if we are recording, add this change to our event_list
                    rec_param_change(inst, pnum);
                    //if (copyto != -1) rec_param_change(inst, copyto);
                  }
                    filter_mutex_unlock(key);
		  // *INDENT-OFF*
}}}}}}
// *INDENT-ON*
    } else {
      weed_set_boolean_array(tparam, WEED_LEAF_HOST_DEFAULT, nargs, valuesb);

      if (group != 0) {
        // set all other values in group to WEED_FALSE
        weed_plant_t *filter = plant, *xtparam;
        int nparams = num_in_params(filter, FALSE, TRUE);

        for (pnum = 0; pnum < nparams; pnum++) {
          xtparam = weed_filter_in_paramtmpl(inst, pnum, TRUE);

          if (xtparam != tparam) {
            ptype = weed_paramtmpl_get_type(xtparam);
            if (ptype == WEED_PARAM_SWITCH) {
              int xgroup = 0;

              if (weed_plant_has_leaf(ptmpl, WEED_LEAF_GROUP))
                xgroup = weed_get_int_value(ptmpl, WEED_LEAF_GROUP, NULL);

              if (xgroup == group) {
                  weed_set_boolean_value(tparam, WEED_LEAF_HOST_DEFAULT, WEED_FALSE);
		// *INDENT-OFF*
}}}}}}
// *INDENT-ON*

grpinvalid:

    lives_free(valuesb);

    break;
  }

  case WEED_PARAM_FLOAT: {
    double *valuesd = (double *)lives_malloc(nargs * sizdbl);

    while (pattern[x] != 0) {
      if (pattern[x] == 'i') {
        // we wanted an float but we got an int
        //so we will convert
        lives_osc_parse_int_argument(vargs, &valuei);
        valuesd[x] = (double)(valuei);
      } else if (pattern[x] == 'f') {
        lives_osc_parse_float_argument(vargs, &valuef);
        valuesd[x] = (double)(valuef);
      } else {
        lives_free(valuesd);
        return FALSE;
      }
      mind = weed_get_double_value(ptmpl, WEED_LEAF_MIN, NULL);
      maxd = weed_get_double_value(ptmpl, WEED_LEAF_MAX, NULL);

      if (valuesd[x] < mind) valuesd[x] = mind;
      if (valuesd[x] > maxd) valuesd[x] = maxd;
      x++;
    }

    if (inst) {
      //copyto = set_copy_to(inst, pnum, FALSE);

      if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst, pnum);
        //if (copyto != -1) rec_param_change(inst, copyto);
      }
      if (!filter_mutex_trylock(key)) {
        weed_set_double_array(tparam, WEED_LEAF_VALUE, nargs, valuesd);
        //set_copy_to(inst, pnum, TRUE);

        if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
          //if (copyto != -1) rec_param_change(inst, copyto);
        }
        filter_mutex_unlock(key);
      }
    } else {
      weed_set_double_array(tparam, WEED_LEAF_HOST_DEFAULT, nargs, valuesd);
    }

    lives_free(valuesd);

    break;
  }

  case WEED_PARAM_TEXT: {
    char **valuess = (char **)lives_malloc(nargs * sizeof(char *));

    while (pattern[x] != 0) {
      if (pattern[x] == 'i') {
        // we wanted a string but we got an int
        //so we will convert
        lives_osc_parse_int_argument(vargs, &valuei);
        valuess[x] = lives_strdup_printf("%d", valuei);
      } else if (pattern[x] == 'f') {
        // we wanted a string but we got a float
        //so we will convert
        lives_osc_parse_float_argument(vargs, &valuef);
        valuess[x] = lives_strdup_printf("%f", valuef);
      } else if (pattern[x] == 's') {
        lives_osc_parse_string_argument(vargs, values);
        valuess[x] = lives_strdup(values);
      } else {
        for (i = 0; i < x; i++) lives_free(valuess[i]);
        lives_free(valuess);
        return FALSE;
      }

      x++;
    }

    if (inst) {
      if (!filter_mutex_trylock(key)) {
        weed_set_string_array(tparam, WEED_LEAF_VALUE, nargs, valuess);
        //copyto = set_copy_to(inst, pnum, TRUE);

        if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
          //if (copyto != -1) rec_param_change(inst, copyto);
        }
        filter_mutex_unlock(key);
      }
    } else {
      weed_set_string_array(tparam, WEED_LEAF_HOST_DEFAULT, nargs, valuess);
    }

    for (i = 0; i < x; i++) lives_free(valuess[i]);
    lives_free(valuess);

    break;
  }

  // COLOR is the most complicated, as we can have 3-values (RGB) or 4-values (RGBA), and we can have
  // an int or a float type. Also min and max can have 1,n or N values.

  case WEED_PARAM_COLOR:
    switch (cspace) {
    case WEED_COLORSPACE_RGB:
      if (nargs % 3 != 0) return FALSE; //nargs must be a multiple of 3

      if (weed_leaf_seed_type(ptmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
        // RGB, int type
        int *valuesi = (int *)lives_malloc(nargs * sizint);
        int nmins = weed_leaf_num_elements(ptmpl, WEED_LEAF_MIN);
        int nmaxs = weed_leaf_num_elements(ptmpl, WEED_LEAF_MAX);
        int *minis = NULL, *maxis = NULL;

        // get min and max values - 3 possibilities: 1 value, 3 values or N values

        if (nmins == 1) {
          mini_r = mini_g = mini_b = weed_get_int_value(ptmpl, WEED_LEAF_MIN, NULL);
        } else {
          minis = weed_get_int_array(ptmpl, WEED_LEAF_MIN, NULL);
          if (nmins == 3) {
            mini_r = minis[0];
            mini_g = minis[1];
            mini_b = minis[2];
            lives_free(minis);
          }
        }

        if (nmaxs == 1) {
          maxi_r = maxi_g = maxi_b = weed_get_int_value(ptmpl, WEED_LEAF_MAX, NULL);
        } else {
          maxis = weed_get_int_array(ptmpl, WEED_LEAF_MAX, NULL);
          if (nmaxs == 3) {
            maxi_r = maxis[0];
            maxi_g = maxis[1];
            maxi_b = maxis[2];
            lives_free(maxis);
          }
        }

        // read vals from OSC message
        while (pattern[x] != 0) {
          // get next 3 values
          for (i = 0; i < 3; i++) {
            if (pattern[x + i] == 'f') {
              // we wanted int but we got floats
              lives_osc_parse_float_argument(vargs, &valuef);
              valuesi[x + i] = myround(valuef);
            } else {
              lives_osc_parse_int_argument(vargs, &valuesi[x + i]);
            }
          }

          if (nmins <= 3) {
            if (valuesi[x] < mini_r) valuesi[x] = mini_r;
            if (valuesi[x + 1] < mini_g) valuesi[x + 1] = mini_g;
            if (valuesi[x + 2] < mini_b) valuesi[x + 2] = mini_b;
          } else {
            if (valuesi[x] < minis[x])   valuesi[x] = minis[x];
            if (valuesi[x + 1] < minis[x + 1]) valuesi[x + 1] = minis[x + 1];
            if (valuesi[x + 2] < minis[x + 2]) valuesi[x + 2] = minis[x + 2];
          }

          if (nmaxs <= 3) {
            if (valuesi[x] > maxi_r) valuesi[x] = maxi_r;
            if (valuesi[x + 1] > maxi_g) valuesi[x + 1] = maxi_g;
            if (valuesi[x + 2] > maxi_b) valuesi[x + 2] = maxi_b;
          } else {
            if (valuesi[x] > maxis[x + i])   valuesi[x] = maxis[x + i];
            if (valuesi[x + 1] > maxis[x + 1]) valuesi[x + 1] = maxis[x + 1];
            if (valuesi[x + 2] > maxis[x + 2]) valuesi[x + 2] = maxis[x + 2];
          }
          x += 3;
        }

        if (inst) {
          if (!filter_mutex_trylock(key)) {
            //copyto = set_copy_to(inst, pnum, FALSE);

            if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
              // if we are recording, add this change to our event_list
              rec_param_change(inst, pnum);
              //if (copyto != -1) rec_param_change(inst, copyto);
            }

            weed_set_int_array(tparam, WEED_LEAF_VALUE, nargs, valuesi);
            //set_copy_to(inst, pnum, TRUE);

            if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
              // if we are recording, add this change to our event_list
              rec_param_change(inst, pnum);
              //if (copyto != -1) rec_param_change(inst, copyto);
            }
            filter_mutex_unlock(key);
          }
        } else {
          weed_set_int_array(tparam, WEED_LEAF_HOST_DEFAULT, nargs, valuesi);
        }

        lives_free(valuesi);

        if (nmins > 3) lives_free(minis);
        if (nmaxs > 3) lives_free(maxis);
      } else {
        // RGB, float type
        double *valuesd = (double *)lives_malloc(nargs * sizeof(double));
        int nmins = weed_leaf_num_elements(ptmpl, WEED_LEAF_MIN);
        int nmaxs = weed_leaf_num_elements(ptmpl, WEED_LEAF_MAX);
        double *minds = NULL, *maxds = NULL;
        // get min and max values - 3 possibilities: 1 value, 3 values or N values

        if (nmins == 1) {
          mind_r = mind_g = mind_b = weed_get_double_value(ptmpl, WEED_LEAF_MIN, NULL);
        } else {
          minds = weed_get_double_array(ptmpl, WEED_LEAF_MIN, NULL);
          if (nmins == 3) {
            mind_r = minds[0];
            mind_g = minds[1];
            mind_b = minds[2];
            lives_free(minds);
          }
        }

        if (nmaxs == 1) {
          maxd_r = maxd_g = maxd_b = weed_get_double_value(ptmpl, WEED_LEAF_MAX, NULL);
        } else {
          maxds = weed_get_double_array(ptmpl, WEED_LEAF_MAX, NULL);
          if (nmaxs == 3) {
            maxd_r = maxds[0];
            maxd_g = maxds[1];
            maxd_b = maxds[2];
            lives_free(maxds);
          }
        }

        // read vals from OSC message
        while (pattern[x] != 0) {
          // get next 3 values
          for (i = 0; i < 3; i++) {
            if (pattern[x + i] == 'i') {
              // we wanted float but we got floats
              lives_osc_parse_int_argument(vargs, &valuei);
              valuesd[x + i] = (double)valuei;
            } else {
              lives_osc_parse_float_argument(vargs, &valuef);
              valuesd[x + i] = (double)valuef;
            }
          }

          if (nmins <= 3) {
            if (valuesd[x] < mind_r) valuesd[x] = mind_r;
            if (valuesd[x + 1] < mind_g) valuesd[x + 1] = mind_g;
            if (valuesd[x + 2] < mind_b) valuesd[x + 2] = mind_b;
          } else {
            if (valuesd[x] < minds[x])   valuesd[x] = minds[x];
            if (valuesd[x + 1] < minds[x + 1]) valuesd[x + 1] = minds[x + 1];
            if (valuesd[x + 2] < minds[x + 2]) valuesd[x + 2] = minds[x + 2];
          }

          if (nmaxs <= 3) {
            if (valuesd[x] > maxd_r) valuesd[x] = maxd_r;
            if (valuesd[x + 1] > maxd_g) valuesd[x + 1] = maxd_g;
            if (valuesd[x + 2] > maxd_b) valuesd[x + 2] = maxd_b;
          } else {
            if (valuesd[x] > maxds[x])   valuesd[x] = maxds[x];
            if (valuesd[x + 1] > maxds[x + 1]) valuesd[x + 1] = maxds[x + 1];
            if (valuesd[x + 2] > maxds[x + 2]) valuesd[x + 2] = maxds[x + 2];
          }
          x += 3;
        }

        if (inst) {
          if (!filter_mutex_trylock(key)) {
            //copyto = set_copy_to(inst, pnum, FALSE);

            if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
              // if we are recording, add this change to our event_list
              rec_param_change(inst, pnum);
              //if (copyto != -1) rec_param_change(inst, copyto);
            }

            weed_set_double_array(tparam, WEED_LEAF_VALUE, nargs, valuesd);
            //set_copy_to(inst, pnum, TRUE);

            if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
              // if we are recording, add this change to our event_list
              rec_param_change(inst, pnum);
              //if (copyto != -1) rec_param_change(inst, copyto);
            }
            filter_mutex_unlock(key);
          }
        } else {
          weed_set_double_array(tparam, WEED_LEAF_HOST_DEFAULT, nargs, valuesd);
        }

        lives_free(valuesd);

        if (nmins > 3) lives_free(minds);
        if (nmaxs > 3) lives_free(maxds);
      }

      break;

    // RGBA colorspace

    case WEED_COLORSPACE_RGBA:
      if (nargs & 3) return FALSE; //nargs must be a multiple of 4

      if (weed_leaf_seed_type(ptmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
        // RGBA, int type
        int *valuesi = (int *)lives_malloc(nargs * sizint);
        int nmins = weed_leaf_num_elements(ptmpl, WEED_LEAF_MIN);
        int nmaxs = weed_leaf_num_elements(ptmpl, WEED_LEAF_MAX);
        int *minis = NULL, *maxis = NULL;

        // get min and max values - 3 possibilities: 1 value, 4 values or N values

        if (nmins == 1) {
          mini_r = mini_g = mini_b = mini_a = weed_get_int_value(ptmpl, WEED_LEAF_MIN, NULL);
        } else {
          minis = weed_get_int_array(ptmpl, WEED_LEAF_MIN, NULL);
          if (nmins == 4) {
            mini_r = minis[0];
            mini_g = minis[1];
            mini_b = minis[2];
            mini_a = minis[3];
            lives_free(minis);
          }
        }

        if (nmaxs == 1) {
          maxi_r = maxi_g = maxi_b = maxi_a = weed_get_int_value(ptmpl, WEED_LEAF_MAX, NULL);
        } else {
          maxis = weed_get_int_array(ptmpl, WEED_LEAF_MAX, NULL);
          if (nmaxs == 4) {
            maxi_r = maxis[0];
            maxi_g = maxis[1];
            maxi_b = maxis[2];
            maxi_a = maxis[3];
            lives_free(maxis);
          }
        }

        // read vals from OSC message
        while (pattern[x] != 0) {
          // get next 4 values
          for (i = 0; i < 4; i++) {
            if (pattern[x] == 'f') {
              // we wanted int but we got floats
              lives_osc_parse_float_argument(vargs, &valuef);
              valuesi[x + i] = myround(valuef);
            } else {
              lives_osc_parse_int_argument(vargs, &valuesi[x + i]);
            }
          }

          if (nmins <= 3) {
            if (valuesi[x] < mini_r) valuesi[x] = mini_r;
            if (valuesi[x + 1] < mini_g) valuesi[x + 1] = mini_g;
            if (valuesi[x + 2] < mini_b) valuesi[x + 2] = mini_b;
            if (valuesi[x + 3] < mini_a) valuesi[x + 3] = mini_a;
          } else {
            if (valuesi[x] < minis[x])   valuesi[x] = minis[x];
            if (valuesi[x + 1] < minis[x + 1]) valuesi[x + 1] = minis[x + 1];
            if (valuesi[x + 2] < minis[x + 2]) valuesi[x + 2] = minis[x + 2];
            if (valuesi[x + 3] < minis[x + 3]) valuesi[x + 3] = minis[x + 3];
          }

          if (nmaxs <= 4) {
            if (valuesi[x] > maxi_r) valuesi[x] = maxi_r;
            if (valuesi[x + 1] > maxi_g) valuesi[x + 1] = maxi_g;
            if (valuesi[x + 2] > maxi_b) valuesi[x + 2] = maxi_b;
            if (valuesi[x + 3] > maxi_a) valuesi[x + 3] = maxi_a;
          } else {
            if (valuesi[x] > maxis[x])   valuesi[x] = maxis[x];
            if (valuesi[x + 1] > maxis[x + 1]) valuesi[x + 1] = maxis[x + 1];
            if (valuesi[x + 2] > maxis[x + 2]) valuesi[x + 2] = maxis[x + 2];
            if (valuesi[x + 3] > maxis[x + 3]) valuesi[x + 3] = maxis[x + 3];
          }
          x += 4;
        }

        if (inst) {
          if (!filter_mutex_trylock(key)) {
            //copyto = set_copy_to(inst, pnum, FALSE);

            if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
              // if we are recording, add this change to our event_list
              rec_param_change(inst, pnum);
              //if (copyto != -1) rec_param_change(inst, copyto);
            }

            weed_set_int_array(tparam, WEED_LEAF_VALUE, nargs, valuesi);
            //set_copy_to(inst, pnum, TRUE);

            if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
              // if we are recording, add this change to our event_list
              rec_param_change(inst, pnum);
              //if (copyto != -1) rec_param_change(inst, copyto);
            }
            filter_mutex_unlock(key);
          }
        } else {
          weed_set_int_array(tparam, WEED_LEAF_HOST_DEFAULT, nargs, valuesi);
        }

        lives_free(valuesi);

        if (nmins > 4) lives_free(minis);
        if (nmaxs > 4) lives_free(maxis);

      } else {
        // RGBA, float type
        double *valuesd = (double *)lives_malloc(nargs * sizdbl);
        int nmins = weed_leaf_num_elements(ptmpl, WEED_LEAF_MIN);
        int nmaxs = weed_leaf_num_elements(ptmpl, WEED_LEAF_MAX);
        double *minds = NULL, *maxds = NULL;

        // get min and max values - 3 possibilities: 1 value, 3 values or N values

        if (nmins == 1) {
          mind_r = mind_g = mind_b = mind_a = weed_get_double_value(ptmpl, WEED_LEAF_MIN, NULL);
        } else {
          minds = weed_get_double_array(ptmpl, WEED_LEAF_MIN, NULL);
          if (nmins == 4) {
            mind_r = minds[0];
            mind_g = minds[1];
            mind_b = minds[2];
            mind_a = minds[3];
            lives_free(minds);
          }
        }

        if (nmaxs == 1) {
          maxd_r = maxd_g = maxd_b = mind_a = weed_get_double_value(ptmpl, WEED_LEAF_MAX, NULL);
        } else {
          maxds = weed_get_double_array(ptmpl, WEED_LEAF_MAX, NULL);
          if (nmaxs == 4) {
            maxd_r = maxds[0];
            maxd_g = maxds[1];
            maxd_b = maxds[2];
            maxd_a = maxds[3];
            lives_free(maxds);
          }
        }

        // read vals from OSC message
        while (pattern[x] != 0) {
          // get next 4 values
          for (i = 0; i < 4; i++) {
            if (pattern[x] == 'i') {
              // we wanted float but we got floats
              lives_osc_parse_int_argument(vargs, &valuei);
              valuesd[x + i] = (double)valuei;
            } else {
              lives_osc_parse_float_argument(vargs, &valuef);
              valuesd[x + i] = (double)valuef;
            }
          }

          if (nmins <= 4) {
            if (valuesd[x] < mind_r) valuesd[x] = mind_r;
            if (valuesd[x + 1] < mind_g) valuesd[x + 1] = mind_g;
            if (valuesd[x + 2] < mind_b) valuesd[x + 2] = mind_b;
            if (valuesd[x + 3] < mind_a) valuesd[x + 3] = mind_a;
          } else {
            if (valuesd[x] < minds[x])   valuesd[x] = minds[x];
            if (valuesd[x + 1] < minds[x + 1]) valuesd[x + 1] = minds[x + 1];
            if (valuesd[x + 2] < minds[x + 2]) valuesd[x + 2] = minds[x + 2];
            if (valuesd[x + 3] < minds[x + 3]) valuesd[x + 3] = minds[x + 3];
          }

          if (nmaxs <= 4) {
            if (valuesd[x] > maxd_r) valuesd[x] = maxd_r;
            if (valuesd[x + 1] > maxd_g) valuesd[x + 1] = maxd_g;
            if (valuesd[x + 2] > maxd_b) valuesd[x + 2] = maxd_b;
            if (valuesd[x + 3] > maxd_a) valuesd[x + 3] = maxd_a;
          } else {
            if (valuesd[x] > maxds[x])   valuesd[x] = maxds[x];
            if (valuesd[x + 1] > maxds[x + 1]) valuesd[x + 1] = maxds[x + 1];
            if (valuesd[x + 2] > maxds[x + 2]) valuesd[x + 2] = maxds[x + 2];
            if (valuesd[x + 3] > maxds[x + 3]) valuesd[x + 3] = maxds[x + 3];
          }
          x += 4;
        }

        if (inst) {
          if (!filter_mutex_trylock(key)) {
            //copyto = set_copy_to(inst, pnum, FALSE);

            if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
              // if we are recording, add this change to our event_list
              rec_param_change(inst, pnum);
              //if (copyto != -1) rec_param_change(inst, copyto);
            }

            weed_set_double_array(tparam, WEED_LEAF_VALUE, nargs, valuesd);
            //set_copy_to(inst, pnum, TRUE);

            if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
              // if we are recording, add this change to our event_list
              rec_param_change(inst, pnum);
              //if (copyto != -1) rec_param_change(inst, copyto);
            }
            filter_mutex_unlock(key);
          }
        } else {
          weed_set_double_array(tparam, WEED_LEAF_HOST_DEFAULT, nargs, valuesd);
        }

        lives_free(valuesd);

        if (nmins > 4) lives_free(minds);
        if (nmaxs > 4) lives_free(maxds);
      }

      break;

    default:
      // invalid colorspace
      return FALSE;
    }

  default:
    // invalid param type
    return FALSE;
  }

  return TRUE;
}


boolean lives_osc_cb_rte_getparamtype(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int ptype;
  int nparams = 0;
  int effect_key, mode, pnum;

  const char *retval;

  // TODO - handle compound fx

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);
  ptype = weed_paramtmpl_get_type(ptmpl);

  switch (ptype) {
  case WEED_PARAM_INTEGER:
    retval = get_omc_const("LIVES_PARAM_TYPE_INT");
    break;
  case WEED_PARAM_FLOAT:
    retval = get_omc_const("LIVES_PARAM_TYPE_FLOAT");
    break;
  case WEED_PARAM_TEXT:
    retval = get_omc_const("LIVES_PARAM_TYPE_STRING");
    break;
  case WEED_PARAM_SWITCH:
    retval = get_omc_const("LIVES_PARAM_TYPE_BOOL");
    break;
  case WEED_PARAM_COLOR:
    retval = get_omc_const("LIVES_PARAM_TYPE_COLOR");
    break;
  default:
    return lives_osc_notify_failure();
  }

  return lives_status_send(retval);
}


boolean lives_osc_cb_rte_getoparamtype(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;
  int ptype;
  int nparams = 0;
  int effect_key, mode, pnum;

  const char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();
  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL);

  ptmpl = out_ptmpls[pnum];
  ptype = weed_paramtmpl_get_type(ptmpl);
  lives_free(out_ptmpls);

  switch (ptype) {
  case WEED_PARAM_INTEGER:
    retval = get_omc_const("LIVES_PARAM_TYPE_INT");
    break;
  case WEED_PARAM_FLOAT:
    retval = get_omc_const("LIVES_PARAM_TYPE_FLOAT");
    break;
  case WEED_PARAM_TEXT:
    retval = get_omc_const("LIVES_PARAM_TYPE_STRING");
    break;
  case WEED_PARAM_SWITCH:
    retval = get_omc_const("LIVES_PARAM_TYPE_BOOL");
    break;
  case WEED_PARAM_COLOR:
    retval = get_omc_const("LIVES_PARAM_TYPE_COLOR");
    break;
  default:
    return lives_osc_notify_failure();
  }

  return lives_status_send(retval);
}


boolean lives_osc_cb_rte_getpparamtype(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  // playback plugin params
  weed_plant_t *ptmpl, *param;
  int ptype;
  int pnum;

  const char *retval;

  if (!mainw->vpp || mainw->vpp->num_play_params == 0) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &pnum);

  if (pnum < 0 || pnum >= mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  ptmpl = weed_param_get_template(param);

  ptype = weed_param_get_type(ptmpl);

  switch (ptype) {
  case WEED_PARAM_INTEGER:
    retval = get_omc_const("LIVES_PARAM_TYPE_INT");
    break;
  case WEED_PARAM_FLOAT:
    retval = get_omc_const("LIVES_PARAM_TYPE_FLOAT");
    break;
  case WEED_PARAM_TEXT:
    retval = get_omc_const("LIVES_PARAM_TYPE_STRING");
    break;
  case WEED_PARAM_SWITCH:
    retval = get_omc_const("LIVES_PARAM_TYPE_BOOL");
    break;
  case WEED_PARAM_COLOR:
    retval = get_omc_const("LIVES_PARAM_TYPE_COLOR");
    break;
  default:
    return lives_osc_notify_failure();
  }

  return lives_status_send(retval);
}


boolean lives_osc_cb_rte_getnparamtype(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int ptype;
  int effect_key, pnum, i;

  const char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();
  if (!weed_plant_has_leaf(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();
  i = get_nth_simple_param(filter, pnum);
  if (i == -1) return lives_osc_notify_failure();
  in_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);
  ptmpl = in_ptmpls[i];

  ptype = weed_param_get_type(ptmpl);
  lives_free(in_ptmpls);

  switch (ptype) {
  case WEED_PARAM_INTEGER:
    retval = get_omc_const("LIVES_PARAM_TYPE_INT");
    break;
  case WEED_PARAM_FLOAT:
    retval = get_omc_const("LIVES_PARAM_TYPE_FLOAT");
    break;
  default:
    return lives_osc_notify_failure();
  }

  return lives_status_send(retval);
}


boolean lives_osc_cb_rte_getparamcspace(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int ptype;
  int nparams;
  int effect_key, mode;
  int pnum, cspace;
  int stype;

  const char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  ptype = weed_paramtmpl_get_type(ptmpl);

  if (ptype != WEED_PARAM_COLOR) {
    return lives_osc_notify_failure();
  }
  cspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, NULL);

  stype = weed_leaf_seed_type(ptmpl, WEED_LEAF_DEFAULT);

  if (cspace == WEED_COLORSPACE_RGB) {
    if (stype == WEED_SEED_INT) retval = get_omc_const("LIVES_COLORSPACE_RGB_INT");
    else retval = get_omc_const("LIVES_COLORSPACE_RGB_FLOAT");
  } else {
    if (stype == WEED_SEED_INT) retval = get_omc_const("LIVES_COLORSPACE_RGBA_INT");
    else retval = get_omc_const("LIVES_COLORSPACE_RGBA_FLOAT");
  }

  return lives_status_send(retval);
}


boolean lives_osc_cb_rte_getparamgrp(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int ptype;
  int nparams;
  int effect_key, mode, pnum, grp;

  char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  ptype = weed_paramtmpl_get_type(ptmpl);

  if (ptype != WEED_PARAM_SWITCH) {
    return lives_osc_notify_failure();
  }
  grp = weed_get_int_value(ptmpl, WEED_LEAF_GROUP, NULL);

  retval = lives_strdup_printf("%d", grp);

  lives_status_send(retval);
  lives_free(retval);

  return TRUE;
}


boolean lives_osc_cb_rte_getoparamcspace(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;
  int ptype;
  int nparams;
  int effect_key, mode, pnum, cspace;
  int stype;

  const char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (pnum < 0) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &nparams);
  if (pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = out_ptmpls[pnum];
  if (!ptmpl) return lives_osc_notify_failure();

  ptype = weed_paramtmpl_get_type(ptmpl);

  if (ptype != WEED_PARAM_COLOR) {
    return lives_osc_notify_failure();
  }
  cspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, NULL);

  stype = weed_leaf_seed_type(ptmpl, WEED_LEAF_DEFAULT);

  if (cspace == WEED_COLORSPACE_RGB) {
    if (stype == WEED_SEED_INT) retval = get_omc_const("LIVES_COLORSPACE_RGB_INT");
    else retval = get_omc_const("LIVES_COLORSPACE_RGB_FLOAT");
  } else {
    if (stype == WEED_SEED_INT) retval = get_omc_const("LIVES_COLORSPACE_RGBA_INT");
    else retval = get_omc_const("LIVES_COLORSPACE_RGBA_FLOAT");
  }

  lives_status_send(retval);
  lives_free(out_ptmpls);
  return TRUE;
}


boolean lives_osc_cb_rte_getpparamcspace(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  // playback plugin params
  weed_plant_t *ptmpl, *param;
  int ptype;
  int pnum, cspace;
  int stype;

  const char *retval;

  if (!mainw->vpp || mainw->vpp->num_play_params == 0) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &pnum);

  if (pnum < 0 || pnum >= mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  ptype = weed_param_get_type(param);

  if (ptype != WEED_PARAM_COLOR) {
    return lives_osc_notify_failure();
  }
  ptmpl = weed_param_get_template(param);
  cspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, NULL);
  stype = weed_leaf_seed_type(ptmpl, WEED_LEAF_DEFAULT);

  if (cspace == WEED_COLORSPACE_RGB) {
    if (stype == WEED_SEED_INT) retval = get_omc_const("LIVES_COLORSPACE_RGB_INT");
    else retval = get_omc_const("LIVES_COLORSPACE_RGB_FLOAT");
  } else {
    if (stype == WEED_SEED_INT) retval = get_omc_const("LIVES_COLORSPACE_RGBA_INT");
    else retval = get_omc_const("LIVES_COLORSPACE_RGBA_FLOAT");
  }

  return lives_status_send(retval);
}


boolean lives_osc_cb_rte_getparamflags(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int nparams;
  int effect_key;
  int mode;
  int pnum, flags = 0;

  char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_FLAGS))
    flags = weed_get_int_value(ptmpl, WEED_LEAF_FLAGS, NULL);

  retval = lives_strdup_printf("%d", flags);
  lives_status_send(retval);
  lives_free(retval);
  return TRUE;
}


boolean lives_osc_cb_rte_getpparamflags(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  weed_plant_t *ptmpl, *param;
  int pnum, flags = 0;

  char *retval;

  if (!mainw->ext_playback || !mainw->vpp->play_params) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &pnum);

  if (pnum < 0 || pnum >= mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, NULL);

  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_FLAGS))
    flags = weed_get_int_value(ptmpl, WEED_LEAF_FLAGS, NULL);

  retval = lives_strdup_printf("%d", flags);
  lives_status_send(retval);
  lives_free(retval);
  return TRUE;
}


boolean lives_osc_cb_rte_getparamname(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;
  int nparams;
  int effect_key;
  int mode;
  int pnum;

  char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  retval = weed_get_string_value(ptmpl, WEED_LEAF_NAME, NULL);

  lives_status_send(retval);

  lives_free(retval);
  return TRUE;
}


boolean lives_osc_cb_pgui_countchoices(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  // TODO: for a running instance, the value may differ from the filter_class
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  int nparams;
  int effect_key;
  int mode;
  int pnum;
  int val = 0;

  char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_CHOICES))
    val = weed_leaf_num_elements(ptmpl, WEED_LEAF_CHOICES);

  retval = lives_strdup_printf("%d", val);

  lives_status_send(retval);

  lives_free(retval);
  return TRUE;
}


boolean lives_osc_cb_pgui_getchoice(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                    NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  boolean ret = FALSE;

  int nparams;
  int effect_key;
  int mode;
  int pnum;
  int cc;

  char *retval = lives_strdup("");

  if (!lives_osc_check_arguments(arglen, vargs, "iiii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "iii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    lives_osc_parse_int_argument(vargs, &cc);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iiii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    lives_osc_parse_int_argument(vargs, &cc);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_CHOICES)) {
    int nc = weed_leaf_num_elements(ptmpl, WEED_LEAF_CHOICES);
    if (cc < nc) {
      char **choices = weed_get_string_array(ptmpl, WEED_LEAF_CHOICES, NULL);
      register int i;
      for (i = 0; i < nc; i++) {
        if (i == cc) {
          lives_free(retval);
          retval = choices[i];
          ret = TRUE;
        } else lives_free(choices[i]);
      }
      lives_free(choices);
    }
  }

  lives_status_send(retval);
  lives_free(retval);

  return ret;
}


boolean lives_osc_cb_rte_getoparamname(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  int nparams;
  int effect_key;
  int mode;
  int pnum;

  char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();
  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();

  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL);
  ptmpl = out_ptmpls[pnum];

  retval = weed_get_string_value(ptmpl, WEED_LEAF_NAME, NULL);

  lives_status_send(retval);

  lives_free(retval);
  lives_free(out_ptmpls);
  return TRUE;
}


boolean lives_osc_cb_rte_getpparamname(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  weed_plant_t *ptmpl, *param;
  int pnum;

  char *retval;

  if (!mainw->ext_playback || !mainw->vpp->play_params) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &pnum);

  if (pnum < 0 || pnum >= mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, NULL);

  retval = weed_get_string_value(ptmpl, WEED_LEAF_NAME, NULL);

  lives_status_send(retval);

  lives_free(retval);
  return TRUE;
}


boolean lives_osc_cb_rte_getnparamname(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int effect_key;
  int pnum, i;

  char *retval;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();
  if (!weed_plant_has_leaf(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();

  i = get_nth_simple_param(filter, pnum);
  if (i == -1) return lives_osc_notify_failure();

  in_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);
  ptmpl = in_ptmpls[i];

  retval = weed_get_string_value(ptmpl, WEED_LEAF_NAME, NULL);

  lives_status_send(retval);

  lives_free(in_ptmpls);
  lives_free(retval);
  return TRUE;
}


boolean lives_osc_cb_rte_setparam(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                  NetworkReturnAddressPtr ra) {
  weed_plant_t *inst, *filter;
  weed_plant_t *tparam;
  int nparams;

  int effect_key;
  int pnum, nargs;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  nargs = lives_osc_get_num_arguments(vargs);
  if (nargs < 3) return lives_osc_notify_failure();

  osc_header_len = pad4(nargs + 1); // add comma

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  inst = rte_keymode_get_instance(effect_key, rte_key_getmode(effect_key));

  if (!inst) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (pnum < 0 || pnum >= nparams) {
    weed_instance_unref(inst);
    return lives_osc_notify_failure();
  }

  tparam = weed_inst_in_param(inst, pnum, FALSE, TRUE);

  if (!mainw->osc_block) {
    if (!setfx(inst, tparam, pnum, nargs - 2, vargs, 3)) {
      weed_instance_unref(inst);
      return lives_osc_notify_failure();
    }
  } else {
    weed_instance_unref(inst);
    return lives_osc_notify_failure();
  }

  weed_instance_unref(inst);

  if (fx_dialog[1]) {
    lives_rfx_t *rfx = fx_dialog[1]->rfx;
    if (!rfx->is_template) {
      int keyw = fx_dialog[1]->key;
      int modew = fx_dialog[1]->mode;
      if (keyw == effect_key && modew == rte_key_getmode(effect_key))
        mainw->vrfx_update = rfx;
    }
  }

  if (mainw->ce_thumbs) ce_thumbs_register_rfx_change(effect_key, rte_key_getmode(effect_key));

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_setparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *tptmpl;
  int nparams;

  int effect_key;
  int mode;
  int pnum, nargs, skip;

  nargs = lives_osc_get_num_arguments(vargs);

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    if (nargs < 3) return lives_osc_notify_failure();
    osc_header_len = pad4(nargs + 1); // add comma
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
    skip = 3;
  } else {
    if (nargs < 4) return lives_osc_notify_failure();
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    osc_header_len = pad4(nargs + 1); // add comma
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &pnum);
    skip = 4;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);

  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  tptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (!setfx(filter, tptmpl, pnum, nargs - 2, vargs, skip)) {
    return lives_osc_notify_failure();
  }

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_setpparam(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                   NetworkReturnAddressPtr ra) {
  // set playback plugin param
  weed_plant_t *param;
  int pnum, nargs;

  if (!mainw->ext_playback || !mainw->vpp->play_params) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();

  nargs = lives_osc_get_num_arguments(vargs);
  if (nargs < 2) return lives_osc_notify_failure();

  osc_header_len = pad4(nargs + 1); // add comma

  lives_osc_parse_int_argument(vargs, &pnum);

  if (pnum < 0 || pnum >= mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  if (!mainw->osc_block) {
    if (!setfx(NULL, param, pnum, nargs - 1, vargs, 2)) return lives_osc_notify_failure();
  } else return lives_osc_notify_failure();

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_setnparam(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                   NetworkReturnAddressPtr ra) {
  int effect_key;
  int pnum, i, nargs;
  int error;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  weed_plant_t *inst, *param;
  weed_plant_t **in_params;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  nargs = lives_osc_get_num_arguments(vargs);
  if (nargs < 3) return lives_osc_notify_failure();

  osc_header_len = pad4(nargs + 1); // add comma

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  inst = rte_keymode_get_instance(effect_key, rte_key_getmode(effect_key));
  if (!inst) return lives_osc_notify_failure();

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, &error);

  i = get_nth_simple_param(inst, pnum);

  param = in_params[i];

  if (i != -1 && !mainw->osc_block) {
    if (!setfx(inst, param, pnum, nargs - 2, vargs, 3)) {
      weed_instance_unref(inst);
      return lives_osc_notify_failure();
    }
  } else {
    weed_instance_unref(inst);
    return lives_osc_notify_failure();
  }
  weed_instance_unref(inst);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_setnparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  weed_plant_t *filter;
  weed_plant_t *tptmpl;
  int nparams;

  int effect_key;
  int mode;
  int pnum, nargs, skip;

  nargs = lives_osc_get_num_arguments(vargs);

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    if (nargs < 3) return lives_osc_notify_failure();
    osc_header_len = pad4(nargs + 1); // add comma
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
    skip = 3;
  } else {
    if (nargs < 4) return lives_osc_notify_failure();
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    osc_header_len = pad4(nargs + 1); // add comma
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &pnum);
    skip = 4;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);

  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  tptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (!setfx(filter, tptmpl, pnum, nargs - 2, vargs, skip)) {
    return lives_osc_notify_failure();
  }

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_paramcount(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key, mode;

  int count = 0;
  weed_plant_t *filter;
  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  count = num_in_params(filter, FALSE, TRUE);

  msg = lives_strdup_printf("%d", count);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_oparamcount(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int effect_key, mode;
  int count = 0;
  weed_plant_t *filter;
  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) {
    count = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  }

  msg = lives_strdup_printf("%d", count);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_getinpal(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  weed_plant_t **ctmpls, *template;
  weed_plant_t *filter, *inst, *ctmpl, *chan = NULL;
  int filter_flags;
  int effect_key, mode, cnum, count;
  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &cnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &cnum);
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  inst = rte_keymode_get_instance(effect_key, mode);
  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (inst) {
    chan = get_enabled_channel(inst, cnum, TRUE);
    ctmpl = weed_get_plantptr_value(chan, WEED_LEAF_TEMPLATE, NULL);
  } else {
    if (!weed_plant_has_leaf(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES)) return lives_osc_notify_failure();
    count = weed_leaf_num_elements(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES);
    if (cnum >= count) return lives_osc_notify_failure();
    ctmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, NULL);
    ctmpl = ctmpls[cnum];
    lives_free(ctmpls);
  }

  if (weed_plant_has_leaf(ctmpl, WEED_LEAF_IS_AUDIO)) {
    weed_instance_unref(inst);
    msg = lives_strdup_printf("%d", WEED_PALETTE_END);
    lives_status_send(msg);
    lives_free(msg);
    return TRUE;
  }

  if (inst) {
    weed_instance_unref(inst);
    msg = lives_strdup_printf("%d", weed_get_int_value(chan, WEED_LEAF_CURRENT_PALETTE, NULL));
    lives_status_send(msg);
    lives_free(msg);
    return TRUE;
  }

  filter_flags = weed_get_int_value(filter, WEED_LEAF_FLAGS, NULL);
  if ((filter_flags & WEED_FILTER_PALETTES_MAY_VARY) && weed_plant_has_leaf(ctmpl, WEED_LEAF_PALETTE_LIST)) {
    template = ctmpl;;
  } else template = filter;
  msg = lives_osc_format_result(template, WEED_LEAF_PALETTE_LIST, 0, -1);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_getoutpal(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  weed_plant_t **ctmpls, *template;
  weed_plant_t *filter, *inst, *ctmpl, *chan = NULL;
  int effect_key, mode, cnum, count, filter_flags;
  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &cnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &cnum);
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  inst = rte_keymode_get_instance(effect_key, mode);
  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (inst) {
    chan = get_enabled_channel(inst, cnum, FALSE);
    ctmpl = weed_get_plantptr_value(chan, WEED_LEAF_TEMPLATE, NULL);
  } else {
    if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES)) return lives_osc_notify_failure();
    count = weed_leaf_num_elements(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES);
    if (cnum >= count) return lives_osc_notify_failure();
    ctmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, NULL);
    ctmpl = ctmpls[cnum];
    lives_free(ctmpls);
  }

  if (weed_plant_has_leaf(ctmpl, WEED_LEAF_IS_AUDIO)) {
    weed_instance_unref(inst);
    msg = lives_strdup_printf("%d", WEED_PALETTE_END);
    lives_status_send(msg);
    lives_free(msg);
    return TRUE;
  }

  if (inst) {
    weed_instance_unref(inst);
    msg = lives_strdup_printf("%d", weed_get_int_value(chan, WEED_LEAF_CURRENT_PALETTE, NULL));
    lives_status_send(msg);
    lives_free(msg);
    return TRUE;
  }
  filter_flags = weed_get_int_value(filter, WEED_LEAF_FLAGS, NULL);

  if ((filter_flags & WEED_FILTER_PALETTES_MAY_VARY) && weed_plant_has_leaf(ctmpl, WEED_LEAF_PALETTE_LIST)) {
    template = ctmpl;;
  } else template = filter;
  msg = lives_osc_format_result(template, WEED_LEAF_PALETTE_LIST, 0, -1);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_pparamcount(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  // return num playback plugin params
  int count = 0;
  char *msg;

  if (!mainw->vpp) {
    return lives_status_send("0");
  }

  count = mainw->vpp->num_play_params;

  msg = lives_strdup_printf("%d", count);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_nparamcount(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int effect_key;
  int count = -1, i;

  // return number of numeric single valued, non-reinit
  // i.e. simple numeric parameters

  weed_plant_t *filter;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();

  do {
    i = get_nth_simple_param(filter, ++count);
  } while (i != -1);

  msg = lives_strdup_printf("%d", count);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_getnchannels(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int effect_key, mode;
  int count;

  weed_plant_t *plant;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    if (mode < 0 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  plant = rte_keymode_get_instance(effect_key, mode);
  if (!plant) plant = rte_keymode_get_filter(effect_key, mode);
  if (!plant) return lives_osc_notify_failure();

  count = enabled_in_channels(plant, FALSE);
  if (WEED_PLANT_IS_FILTER_INSTANCE(plant)) weed_instance_unref(plant);

  msg = lives_strdup_printf("%d", count);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_getnochannels(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  int effect_key;
  int count;

  weed_plant_t *plant, *orig_plant;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);
  orig_plant = plant = rte_keymode_get_instance(effect_key, rte_key_getmode(effect_key));

  // handle compound fx
  if (plant) while (weed_plant_has_leaf(plant, WEED_LEAF_HOST_NEXT_INSTANCE)) plant = weed_get_plantptr_value(plant,
          WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
  else plant = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (plant) return lives_osc_notify_failure();

  count = enabled_out_channels(plant, FALSE);
  if (WEED_PLANT_IS_FILTER_INSTANCE(orig_plant)) weed_instance_unref(orig_plant);

  msg = lives_strdup_printf("%d", count);
  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_getparammin(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum;

  int nparams;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_MIN)) {
    return lives_osc_notify_failure();
  }

  msg = lives_osc_format_result(ptmpl, WEED_LEAF_MIN, 0, -1);

  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_getoparammin(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum;

  int error, nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();

  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &error);

  ptmpl = out_ptmpls[pnum];

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_MIN)) {
    lives_free(out_ptmpls);
    return lives_osc_notify_failure();
  }

  msg = lives_osc_format_result(ptmpl, WEED_LEAF_MIN, 0, -1);

  lives_status_send(msg);
  lives_free(msg);
  lives_free(out_ptmpls);
  return TRUE;
}


boolean lives_osc_cb_rte_getohasparammin(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum;

  int error, nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();

  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &error);

  ptmpl = out_ptmpls[pnum];
  lives_free(out_ptmpls);

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_MIN)) return lives_status_send(get_omc_const("LIVES_FALSE"));
  else return lives_status_send(get_omc_const("LIVES_TRUE"));
}


boolean lives_osc_cb_rte_getpparammin(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int pnum;

  int error;
  weed_plant_t *ptmpl, *param;

  char *msg;

  if (!mainw->ext_playback || !mainw->vpp->play_params) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &pnum);

  if (pnum < 0 || pnum >= mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_MIN)) {
    return lives_osc_notify_failure();
  }

  msg = lives_osc_format_result(ptmpl, WEED_LEAF_MIN, 0, -1);

  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_getparammax(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum;

  int nparams;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_MAX)) {
    return lives_osc_notify_failure();
  }

  msg = lives_osc_format_result(ptmpl, WEED_LEAF_MAX, 0, -1);

  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_getoparammax(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum;

  int error, nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();
  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &error);

  ptmpl = out_ptmpls[pnum];

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_MAX)) {
    lives_free(out_ptmpls);
    return lives_osc_notify_failure();
  }

  msg = lives_osc_format_result(ptmpl, WEED_LEAF_MAX, 0, -1);

  lives_status_send(msg);
  lives_free(msg);
  lives_free(out_ptmpls);
  return TRUE;
}


boolean lives_osc_cb_rte_getohasparammax(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum;

  int error, nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();
  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &error);

  ptmpl = out_ptmpls[pnum];
  lives_free(out_ptmpls);

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_MAX)) return lives_status_send(get_omc_const("LIVES_FALSE"));
  else return lives_status_send(get_omc_const("LIVES_TRUE"));
}


boolean lives_osc_cb_rte_getpparammax(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  // playback plugin param max

  int pnum;

  int error;
  weed_plant_t *ptmpl, *param;

  char *msg;

  if (!mainw->ext_playback || !mainw->vpp->play_params) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &pnum);

  if (pnum < 0 || pnum >= mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_MAX)) {
    return lives_osc_notify_failure();
  }

  msg = lives_osc_format_result(ptmpl, WEED_LEAF_MAX, 0, -1);

  lives_status_send(msg);
  lives_free(msg);

  return TRUE;
}


boolean lives_osc_cb_rte_getparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum, nvals;

  int nparams;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT)) {
    msg = lives_osc_format_result(ptmpl, WEED_LEAF_HOST_DEFAULT, 0, -1);
  } else {
    nvals = weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT);
    if (nvals > 0)
      msg = lives_osc_format_result(ptmpl, WEED_LEAF_DEFAULT, 0, nvals);
    else {
      // default can have 0 values if param has variable elements; in this case we use WEED_LEAF_NEW_DEFAULT
      msg = lives_osc_format_result(ptmpl, WEED_LEAF_NEW_DEFAULT, 0, -1);
    }
  }

  lives_status_send(msg);
  lives_free(msg);

  return TRUE;
}


boolean lives_osc_cb_rte_getoparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum, nvals;

  int error, nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();
  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &error);

  ptmpl = out_ptmpls[pnum];

  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT)) {
    msg = lives_osc_format_result(ptmpl, WEED_LEAF_HOST_DEFAULT, 0, -1);
  } else {
    if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_DEFAULT)) {
      lives_free(out_ptmpls);
      return lives_osc_notify_failure();
    }

    nvals = weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT);
    if (nvals > 0)
      msg = lives_osc_format_result(ptmpl, WEED_LEAF_DEFAULT, 0, nvals);
    else {
      // default can have 0 values if param has variable elements; in this case we use WEED_LEAF_NEW_DEFAULT
      msg = lives_osc_format_result(ptmpl, WEED_LEAF_NEW_DEFAULT, 0, -1);
    }
  }

  lives_status_send(msg);
  lives_free(msg);
  lives_free(out_ptmpls);

  return TRUE;
}


boolean lives_osc_cb_rte_gethasparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum;

  int nparams;
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT)) {
    if (weed_leaf_num_elements(ptmpl, WEED_LEAF_HOST_DEFAULT) == 0) return lives_status_send(get_omc_const("LIVES_FALSE"));
    return lives_status_send(get_omc_const("LIVES_DEFAULT_OVERRIDDEN"));
  }
  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_DEFAULT) ||
      weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT) == 0) return lives_status_send(get_omc_const("LIVES_FALSE"));
  else return lives_status_send(get_omc_const("LIVES_TRUE"));
}


boolean lives_osc_cb_rte_getohasparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  int pnum;

  int error, nparams;
  weed_plant_t *filter;
  weed_plant_t **out_ptmpls;
  weed_plant_t *ptmpl;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();
  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &error);

  ptmpl = out_ptmpls[pnum];
  lives_free(out_ptmpls);

  if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT) &&
      !weed_plant_has_leaf(ptmpl, WEED_LEAF_DEFAULT)) return lives_status_send(get_omc_const("LIVES_FALSE"));
  else return lives_status_send(get_omc_const("LIVES_TRUE"));
}


boolean lives_osc_cb_rte_getpparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  // default for playback plugin param

  int pnum, nvals;

  int error;
  weed_plant_t *param;
  weed_plant_t *ptmpl;

  char *msg;

  if (!mainw->ext_playback || !mainw->vpp->play_params) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &pnum);

  if (pnum < 0 || pnum >= mainw->vpp->num_play_params) return lives_osc_notify_failure();

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  nvals = weed_leaf_num_elements(ptmpl, WEED_LEAF_DEFAULT);
  if (nvals > 0)
    msg = lives_osc_format_result(ptmpl, WEED_LEAF_DEFAULT, 0, nvals);
  else {
    // default can have 0 values if param has variable elements; in this case we use WEED_LEAF_NEW_DEFAULT
    msg = lives_osc_format_result(ptmpl, WEED_LEAF_NEW_DEFAULT, 0, -1);
  }

  lives_status_send(msg);
  lives_free(msg);

  return TRUE;
}


boolean lives_osc_cb_rte_getparamval(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                     NetworkReturnAddressPtr ra) {
  int effect_key;
  int pnum, st = 0, end = 1, ptype, cspace;

  int error, nparams;
  weed_plant_t *inst, *filter;
  weed_plant_t *param, *ptmpl;
  char *msg;

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
  } else {
    if (!lives_osc_check_arguments(arglen, vargs, "iii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    lives_osc_parse_int_argument(vargs, &st);
    end = st + 1;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  inst = rte_keymode_get_instance(effect_key, rte_key_getmode(effect_key));

  if (!inst) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) {
    weed_instance_unref(inst);
    return lives_osc_notify_failure();
  }
  if (pnum < 0 || pnum >= nparams) {
    weed_instance_unref(inst);
    return lives_osc_notify_failure();
  }

  param = weed_inst_in_param(inst, pnum, FALSE, TRUE);
  ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  ptype = weed_paramtmpl_get_type(ptmpl);
  if (ptype == WEED_PARAM_COLOR) {
    int valsize = 4;
    cspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, &error);
    if (cspace == WEED_COLORSPACE_RGB) valsize = 3;
    st *= valsize;
    end = st + valsize;
  }

  if (end > weed_leaf_num_elements(param, WEED_LEAF_VALUE)) {
    weed_instance_unref(inst);
    return lives_osc_notify_failure();
  }

  msg = lives_osc_format_result(param, WEED_LEAF_VALUE, st, end);

  weed_instance_unref(inst);
  lives_status_send(msg);
  lives_free(msg);

  return TRUE;
}


boolean lives_osc_cb_rte_getoparamval(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int effect_key;
  int pnum, st = 0, end = 1, ptype, cspace;

  int error, nparams;
  weed_plant_t *inst, *filter;
  weed_plant_t **out_params, **out_ptmpls;
  weed_plant_t *param, *ptmpl;
  char *msg;

  if (lives_osc_check_arguments(arglen, vargs, "ii", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "ii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
  } else {
    if (!lives_osc_check_arguments(arglen, vargs, "iii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    lives_osc_parse_int_argument(vargs, &st);
    end = st + 1;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();

  if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) return lives_osc_notify_failure();
  nparams = weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  inst = rte_keymode_get_instance(effect_key, rte_key_getmode(effect_key));
  if (!inst) return lives_osc_notify_failure();

  out_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, &error);

  ptmpl = out_ptmpls[pnum];

  out_params = weed_get_plantptr_array(inst, WEED_LEAF_OUT_PARAMETERS, &error);

  param = out_params[pnum];

  lives_free(out_ptmpls);
  lives_free(out_params);

  ptype = weed_paramtmpl_get_type(ptmpl);
  if (ptype == WEED_PARAM_COLOR) {
    int valsize = 4;
    cspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, &error);
    if (cspace == WEED_COLORSPACE_RGB) valsize = 3;
    st *= valsize;
    end = st + valsize;
  }

  if (end > weed_leaf_num_elements(param, WEED_LEAF_VALUE)) {
    weed_instance_unref(inst);
    return lives_osc_notify_failure();
  }

  if (!filter_mutex_trylock(effect_key - 1)) {
    msg = lives_osc_format_result(param, WEED_LEAF_VALUE, st, end);
    weed_instance_unref(inst);
    filter_mutex_unlock(effect_key - 1);
    lives_status_send(msg);
    lives_free(msg);
  } else {
    weed_instance_unref(inst);
    return lives_osc_notify_failure();
  }
  weed_instance_unref(inst);
  return TRUE;
}


boolean lives_osc_cb_rte_getpparamval(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  // playback plugin param value
  int pnum, st = 0, end = 1, ptype, cspace;

  int error;
  weed_plant_t *param, *ptmpl;
  char *msg;

  if (!mainw->ext_playback || !mainw->vpp->play_params) return lives_osc_notify_failure();

  if (lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    lives_osc_check_arguments(arglen, vargs, "i", TRUE);
    lives_osc_parse_int_argument(vargs, &pnum);
  } else {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &pnum);
    lives_osc_parse_int_argument(vargs, &st);
    end = st + 1;
  }

  param = (weed_plant_t *)pp_get_param(mainw->vpp->play_params, pnum);

  ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);

  ptype = weed_paramtmpl_get_type(ptmpl);
  if (ptype == WEED_PARAM_COLOR) {
    int valsize = 4;
    cspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, &error);
    if (cspace == WEED_COLORSPACE_RGB) valsize = 3;
    st *= valsize;
    end = st + valsize;
  }

  if (end > weed_leaf_num_elements(param, WEED_LEAF_VALUE)) return lives_osc_notify_failure();

  msg = lives_osc_format_result(param, WEED_LEAF_VALUE, st, end);

  lives_status_send(msg);
  lives_free(msg);

  return TRUE;
}


boolean lives_osc_cb_rte_getnparam(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  int pnum, i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int ptype;

  int vali;
  double vald;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();

  i = get_nth_simple_param(filter, pnum);

  if (i == -1) return lives_osc_notify_failure();

  in_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);

  ptmpl = in_ptmpls[i];
  ptype = weed_paramtmpl_get_type(ptmpl);

  if (ptype == WEED_PARAM_INTEGER) {
    vali = weed_get_int_value(ptmpl, WEED_LEAF_VALUE, NULL);
    msg = lives_strdup_printf("%d", vali);
  } else {
    vald = weed_get_double_value(ptmpl, WEED_LEAF_VALUE, NULL);
    msg = lives_strdup_printf("%f", vald);
  }
  lives_status_send(msg);
  lives_free(msg);
  lives_free(in_ptmpls);

  return TRUE;
}


boolean lives_osc_cb_rte_getnparammin(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int effect_key;
  int pnum, i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int ptype;

  int vali;
  double vald;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();

  i = get_nth_simple_param(filter, pnum);

  if (i == -1) return lives_osc_notify_failure();

  in_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);

  ptmpl = in_ptmpls[i];
  ptype = weed_paramtmpl_get_type(ptmpl);

  if (ptype == WEED_PARAM_INTEGER) {
    vali = weed_get_int_value(ptmpl, WEED_LEAF_MIN, NULL);
    msg = lives_strdup_printf("%d", vali);
  } else {
    vald = weed_get_double_value(ptmpl, WEED_LEAF_MIN, NULL);
    msg = lives_strdup_printf("%f", vald);
  }
  lives_status_send(msg);
  lives_free(msg);
  lives_free(in_ptmpls);

  return TRUE;
}


boolean lives_osc_cb_rte_getnparammax(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int effect_key;
  int pnum, i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int ptype;

  int vali;
  double vald;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();

  i = get_nth_simple_param(filter, pnum);

  if (i == -1) return lives_osc_notify_failure();

  in_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);

  ptmpl = in_ptmpls[i];
  ptype = weed_paramtmpl_get_type(ptmpl);

  if (ptype == WEED_PARAM_INTEGER) {
    vali = weed_get_int_value(ptmpl, WEED_LEAF_MAX, NULL);
    msg = lives_strdup_printf("%d", vali);
  } else {
    vald = weed_get_double_value(ptmpl, WEED_LEAF_MAX, NULL);
    msg = lives_strdup_printf("%f", vald);
  }
  lives_status_send(msg);
  lives_free(msg);
  lives_free(in_ptmpls);
  return TRUE;
}


boolean lives_osc_cb_rte_getnparamdef(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                      NetworkReturnAddressPtr ra) {
  int effect_key;
  int pnum, i;

  // pick pnum which is numeric single valued, non-reinit
  // i.e. simple numeric parameter

  weed_plant_t *filter;
  weed_plant_t **in_ptmpls;
  weed_plant_t *ptmpl;
  int ptype;

  int vali;
  double vald;

  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();

  i = get_nth_simple_param(filter, pnum);

  if (i == -1) return lives_osc_notify_failure();

  in_ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);

  ptmpl = in_ptmpls[i];
  ptype = weed_paramtmpl_get_type(ptmpl);

  if (ptype == WEED_PARAM_INTEGER) {
    if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT)) vali = weed_get_int_value(ptmpl, WEED_LEAF_DEFAULT, NULL);
    else vali = weed_get_int_value(ptmpl, WEED_LEAF_HOST_DEFAULT, NULL);
    msg = lives_strdup_printf("%d", vali);
  } else {
    if (!weed_plant_has_leaf(ptmpl, WEED_LEAF_HOST_DEFAULT)) vald = weed_get_double_value(ptmpl, WEED_LEAF_DEFAULT, NULL);
    else vald = weed_get_double_value(ptmpl, WEED_LEAF_HOST_DEFAULT, NULL);
    msg = lives_strdup_printf("%f", vald);
  }

  lives_status_send(msg);
  lives_free(msg);
  lives_free(in_ptmpls);
  return TRUE;
}


boolean lives_osc_cb_rte_getnparamtrans(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  // check if nparam is the transition parameter
  weed_plant_t *filter;

  int effect_key;
  int pnum;
  int nparams;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &pnum);

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, rte_key_getmode(effect_key));
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  if (pnum == get_transition_param(filter, TRUE)) return lives_status_send(get_omc_const("LIVES_TRUE"));
  return lives_status_send(get_omc_const("LIVES_FALSE"));
}


boolean lives_osc_cb_rte_getparamtrans(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  // check if param is the transition parameter
  weed_plant_t *filter;
  weed_plant_t *ptmpl;

  int effect_key;
  int mode;
  int pnum;
  int nparams;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", FALSE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &pnum);
    mode = rte_key_getmode(effect_key);
  } else {
    lives_osc_check_arguments(arglen, vargs, "iii", TRUE);
    lives_osc_parse_int_argument(vargs, &effect_key);
    lives_osc_parse_int_argument(vargs, &mode);
    lives_osc_parse_int_argument(vargs, &pnum);
    if (mode < 1 || mode > rte_key_getmaxmode(effect_key) + 1) return lives_osc_notify_failure();
    mode--;
  }

  if (effect_key < 1 || effect_key > FX_MAX) return lives_osc_notify_failure();
  //g_print("key %d pnum %d",effect_key,pnum);

  filter = rte_keymode_get_filter(effect_key, mode);
  if (!filter) return lives_osc_notify_failure();

  nparams = num_in_params(filter, FALSE, TRUE);
  if (nparams == 0) return lives_osc_notify_failure();
  if (pnum < 0 || pnum >= nparams) return lives_osc_notify_failure();

  ptmpl = weed_filter_in_paramtmpl(filter, pnum, TRUE);

  if (weed_plant_has_leaf(ptmpl, WEED_LEAF_IS_TRANSITION) &&
      weed_get_boolean_value(ptmpl, WEED_LEAF_IS_TRANSITION, NULL) == WEED_TRUE)
    return lives_status_send(get_omc_const("LIVES_TRUE"));
  return lives_status_send(get_omc_const("LIVES_FALSE"));
}


boolean lives_osc_cb_rte_getmode(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char *tmp;

  int effect_key;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);

  if (effect_key < 1 || effect_key > FX_MAX) {
    return lives_status_send("0");
  }

  lives_status_send((tmp = lives_strdup_printf("%d", rte_key_getmode(effect_key) + 1)));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_rte_getstate(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  weed_plant_t *inst;
  int effect_key;

  if (!lives_osc_check_arguments(arglen, vargs, "i", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);

  if (effect_key < 1 || effect_key > FX_KEYS_MAX_VIRTUAL) {
    return lives_status_send(get_omc_const("LIVES_FALSE"));
  }
  if ((inst = rte_keymode_get_instance(effect_key,
                                       rte_key_getmode(effect_key))) == NULL)
    return lives_status_send(get_omc_const("LIVES_FALSE"));
  weed_instance_unref(inst);
  return lives_status_send(get_omc_const("LIVES_TRUE"));
}


boolean lives_osc_cb_rte_get_keyfxname(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                       NetworkReturnAddressPtr ra) {
  int effect_key;
  int mode;
  char *tmp;

  if (!lives_osc_check_arguments(arglen, vargs, "ii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &effect_key);
  lives_osc_parse_int_argument(vargs, &mode);
  if (effect_key < 1 || effect_key > FX_MAX || mode < 1 || mode > rte_getmodespk()) return lives_osc_notify_failure();
  lives_status_send((tmp = lives_strdup_printf("%s", rte_keymode_get_filter_name(effect_key, mode - 1, FALSE))));
  lives_free(tmp);
  return TRUE;
}


boolean lives_osc_cb_rte_getmodespk(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  int effect_key;
  char *tmp;

  if (!lives_osc_check_arguments(arglen, vargs, "i", FALSE)) {
    if (lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
      lives_status_send((tmp = lives_strdup_printf("%d", rte_getmodespk())));
      lives_free(tmp);
      return TRUE;
    }
    return lives_osc_notify_failure();
  }

  lives_osc_check_arguments(arglen, vargs, "i", TRUE);
  lives_osc_parse_int_argument(vargs, &effect_key);

  if (effect_key > FX_KEYS_MAX_VIRTUAL || effect_key < 1) {
    return lives_status_send("0");
  }

  lives_status_send((tmp = lives_strdup_printf("%d", rte_key_getmaxmode(effect_key) + 1)));
  lives_free(tmp);

  return TRUE;
}


boolean lives_osc_cb_rte_addpconnection(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  weed_plant_t *ofilter, *ifilter;

  int key0, mode0, pnum0;
  int key1, mode1, pnum1;
  int autoscale;

  if (!lives_osc_check_arguments(arglen, vargs, "iiiiiii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &key0);
  lives_osc_parse_int_argument(vargs, &mode0);
  lives_osc_parse_int_argument(vargs, &pnum0);

  lives_osc_parse_int_argument(vargs, &autoscale);

  lives_osc_parse_int_argument(vargs, &key1);
  lives_osc_parse_int_argument(vargs, &mode1);
  lives_osc_parse_int_argument(vargs, &pnum1);

  if (key0 < 1 || key0 >= FX_KEYS_MAX_VIRTUAL || mode0 < 1 || mode0 > rte_getmodespk()) return lives_osc_notify_failure();
  if (key1 < -2 || key1 == 0 || key1 >= FX_KEYS_MAX_VIRTUAL || mode1 < 1 || (key1 >= 0 &&
      mode1 > rte_getmodespk())) return lives_osc_notify_failure();

  if (key0 == key1) return lives_osc_notify_failure();

  if (autoscale != TRUE && autoscale != FALSE) return lives_osc_notify_failure();

  mode0--;
  mode1--;

  ofilter = rte_keymode_get_filter(key0, mode0);
  if (!ofilter) return lives_osc_notify_failure();

  if (pnum0 >= num_out_params(ofilter)) return lives_osc_notify_failure();

  if (key1 == -1) {
    // connecting to the playback plugin
    if (mode1 > 1 || !mainw->vpp || pnum1 >= mainw->vpp->num_play_params) return lives_osc_notify_failure();
  } else if (key1 == -2) {
    // connecting to subtitler
    if (mode1 > 1 || pnum1 > 0) return lives_osc_notify_failure();
  } else {
    ifilter = rte_keymode_get_filter(key1, mode1);
    if (!ifilter) return lives_osc_notify_failure();

    if (pnum1 >= num_in_params(ifilter, FALSE, TRUE)) return lives_osc_notify_failure();
  }

  if (pnum0 < -EXTRA_PARAMS_OUT || pnum1 < -EXTRA_PARAMS_IN) return lives_osc_notify_failure();

  if (pconx_check_connection(ofilter, pnum0, key1, mode1, pnum1, FALSE, NULL, NULL, NULL, NULL,
                             NULL)) return lives_osc_notify_failure();

  key0--;
  key1--;

  pconx_add_connection(key0, mode0, pnum0, key1, mode1, pnum1, autoscale);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_delpconnection(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  int key0, mode0, pnum0;
  int key1, mode1, pnum1;

  if (!lives_osc_check_arguments(arglen, vargs, "iiiiii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &key0);
  lives_osc_parse_int_argument(vargs, &mode0);
  lives_osc_parse_int_argument(vargs, &pnum0);
  lives_osc_parse_int_argument(vargs, &key1);
  lives_osc_parse_int_argument(vargs, &mode1);
  lives_osc_parse_int_argument(vargs, &pnum1);

  if (key0 < 0 || key0 >= FX_KEYS_MAX_VIRTUAL || mode0 < 1 || mode0 > rte_getmodespk()) return lives_osc_notify_failure();
  if (key1 < -2 || key1 >= FX_KEYS_MAX_VIRTUAL || mode1 < 1 || mode1 > rte_getmodespk()) return lives_osc_notify_failure();

  if (pnum0 < -EXTRA_PARAMS_OUT || pnum1 < -EXTRA_PARAMS_IN) return lives_osc_notify_failure();

  pconx_delete(key0 == 0 ? FX_DATA_WILDCARD : key0 - 1, key0 == 0 ? FX_DATA_WILDCARD : --mode0,
               key0 == 0 ? FX_DATA_WILDCARD : pnum0, key1 == 0 ? FX_DATA_WILDCARD : key1 - 1,
               key1 == 0 ? FX_DATA_WILDCARD : --mode1, key1 == 0 ? FX_DATA_WILDCARD : pnum1);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_listpconnection(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  int okey, omode, opnum;
  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &okey);
  lives_osc_parse_int_argument(vargs, &omode);
  lives_osc_parse_int_argument(vargs, &opnum);

  if (okey < 1 || okey >= FX_KEYS_MAX_VIRTUAL || omode < 1 || omode > rte_getmodespk()) return lives_osc_notify_failure();

  msg = pconx_list(okey, omode, opnum);

  if (!*msg) {
    lives_free(msg);
    msg = lives_strdup("0 0 0 0");
  }

  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_rte_addcconnection(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  int key0, mode0, cnum0;
  int key1, mode1, cnum1;
  weed_plant_t *filter;

  if (!lives_osc_check_arguments(arglen, vargs, "iiiiii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &key0);
  lives_osc_parse_int_argument(vargs, &mode0);
  lives_osc_parse_int_argument(vargs, &cnum0);
  lives_osc_parse_int_argument(vargs, &key1);
  lives_osc_parse_int_argument(vargs, &mode1);
  lives_osc_parse_int_argument(vargs, &cnum1);

  if (key0 < 1 || key0 >= FX_KEYS_MAX_VIRTUAL || mode0 < 1 || mode0 > rte_getmodespk()) return lives_osc_notify_failure();
  if (key1 < -1 || key1 == 0 || key1 >= FX_KEYS_MAX_VIRTUAL || mode1 < 1 ||
      mode1 > rte_getmodespk()) return lives_osc_notify_failure();

  if (key0 == key1) return lives_osc_notify_failure();

  mode0--;
  mode1--;

  filter = rte_keymode_get_filter(key0, mode0);
  if (!filter) return lives_osc_notify_failure();

  if (cnum0 >= enabled_out_channels(filter, FALSE)) return lives_osc_notify_failure();

  if (key1 == -1) {
    // connecting to the playback plugin
    if (mode1 > 1 || !mainw->vpp || cnum1 >= mainw->vpp->num_alpha_chans) return lives_osc_notify_failure();
  } else {
    filter = rte_keymode_get_filter(key1, mode1);
    if (!filter) return lives_osc_notify_failure();

    if (cnum1 >= enabled_in_channels(filter, FALSE)) return lives_osc_notify_failure();
  }

  if (cconx_check_connection(key1, mode1, cnum1, FALSE, NULL, NULL, NULL, NULL, NULL)) return lives_osc_notify_failure();

  key0--;
  key1--;

  cconx_add_connection(key0, mode0, cnum0, key1, mode1, cnum1);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_delcconnection(void *context, int arglen, const void *vargs, OSCTimeTag when,
                                        NetworkReturnAddressPtr ra) {
  int key0, mode0, cnum0;
  int key1, mode1, cnum1;

  if (!lives_osc_check_arguments(arglen, vargs, "iiiiii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &key0);
  lives_osc_parse_int_argument(vargs, &mode0);
  lives_osc_parse_int_argument(vargs, &cnum0);
  lives_osc_parse_int_argument(vargs, &key1);
  lives_osc_parse_int_argument(vargs, &mode1);
  lives_osc_parse_int_argument(vargs, &cnum1);

  if (key0 < 0 || key0 >= FX_KEYS_MAX_VIRTUAL || mode0 < 1 || mode0 > rte_getmodespk()) return lives_osc_notify_failure();
  if (key1 < -2 || key1 >= FX_KEYS_MAX_VIRTUAL || mode1 < 1 || mode1 > rte_getmodespk()) return lives_osc_notify_failure();

  cconx_delete(key0 == 0 ? FX_DATA_WILDCARD : --key0, --mode0, cnum0, key1 == 0 ? FX_DATA_WILDCARD : --key1, --mode1, cnum1);

  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_rte_listcconnection(void *context, int arglen, const void *vargs, OSCTimeTag when,
    NetworkReturnAddressPtr ra) {
  int okey, omode, ocnum;
  char *msg;

  if (!lives_osc_check_arguments(arglen, vargs, "iii", TRUE)) return lives_osc_notify_failure();
  lives_osc_parse_int_argument(vargs, &okey);
  lives_osc_parse_int_argument(vargs, &omode);
  lives_osc_parse_int_argument(vargs, &ocnum);

  if (okey < 1 || okey >= FX_KEYS_MAX_VIRTUAL || omode < 1 || omode > rte_getmodespk()) return lives_osc_notify_failure();

  msg = cconx_list(okey, omode, ocnum);

  if (!*msg) {
    lives_free(msg);
    msg = lives_strdup("0 0 0");
  }

  lives_status_send(msg);
  lives_free(msg);
  return TRUE;
}


boolean lives_osc_cb_swap(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  swap_fg_bg_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_record_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  record_toggle_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER((int)TRUE));
  return lives_osc_notify_success(NULL);
  // TODO - send record start and record stop events
}


boolean lives_osc_record_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  record_toggle_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER((int)FALSE));
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_record_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  if (mainw->multitrack) return lives_osc_notify_failure();
  record_toggle_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(!mainw->record));
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_ping(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  //g_print("send pong\n");
  return lives_status_send("pong");
}


boolean lives_osc_cb_getsetname(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  return lives_status_send(mainw->set_name);
}


boolean lives_osc_cb_open_file(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char filename[OSC_STRING_SIZE];
  float starttime = 0.;
  int numframes = 0; // all frames by default

  int type = 0;

  if ((mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "sfi", FALSE)) {
    type++;
    if (!lives_osc_check_arguments(arglen, vargs, "sf", FALSE)) {
      type++;
      if (!lives_osc_check_arguments(arglen, vargs, "s", TRUE)) return lives_osc_notify_failure();
    } else lives_osc_check_arguments(arglen, vargs, "sf", TRUE);
  } else lives_osc_check_arguments(arglen, vargs, "sfi", TRUE);

  lives_osc_parse_string_argument(vargs, filename);
  if (type < 2) {
    lives_osc_parse_float_argument(vargs, &starttime);
    if (type < 1) {
      lives_osc_parse_int_argument(vargs, &numframes);
    }
  }
  deduce_file(filename, starttime, numframes);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_open_unicap(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
#ifdef HAVE_UNICAP
  char devname[OSC_STRING_SIZE];
  int deint = FALSE;

  char *boolstr;

  if ((mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "si", FALSE)) {
    if (lives_osc_check_arguments(arglen, vargs, "s", FALSE)) {
      lives_osc_parse_string_argument(vargs, devname);
    } else return lives_osc_notify_failure();
  } else {
    lives_osc_parse_string_argument(vargs, devname);
    lives_osc_parse_int_argument(vargs, &deint);
    boolstr = lives_strdup_printf("%d", deint);
    if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) deint = TRUE;
    else {
      if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) deint = FALSE;
      else {
        lives_free(boolstr);
        return lives_osc_notify_failure();
      }
    }
    lives_free(boolstr);
  }

  mainw->open_deint = deint;

  on_open_vdev_activate(NULL, (livespointer)devname);

  return lives_osc_notify_success(NULL);

#endif

  return lives_osc_notify_failure();
}


boolean lives_osc_cb_new_audio(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char filename[OSC_STRING_SIZE];

  if ((mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (!CURRENT_CLIP_HAS_VIDEO || cfile->opening) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "s", TRUE)) return lives_osc_notify_failure();

  lives_osc_parse_string_argument(vargs, filename);
  on_open_new_audio_clicked(NULL, filename);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_loadset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  char setname[OSC_STRING_SIZE];

  char *tmp;

  if ((mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (*mainw->set_name) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "s", TRUE)) {
    return lives_osc_notify_failure();
  }
  lives_osc_parse_string_argument(vargs, setname);

  mainw->osc_auto = 1;
  if (!is_legal_set_name((tmp = U82F(setname)), TRUE, TRUE)) {
    mainw->osc_auto = 0;
    lives_free(tmp);
    return lives_osc_notify_failure();
  }
  mainw->osc_auto = 0;

  lives_free(tmp);

  reload_set(setname);
  return lives_osc_notify_success(NULL);
}


boolean lives_osc_cb_saveset(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra) {
  boolean ret;
  int force_append = FALSE;
  char setname[OSC_STRING_SIZE];

  char *tmp;
  char *boolstr;

  // setname should be in filesystem encoding

  lives_memset(setname, 0, 1);

  if ((mainw->preview || (!mainw->multitrack && mainw->event_list)) || mainw->is_processing ||
      mainw->multitrack) return lives_osc_notify_failure();

  if (LIVES_IS_PLAYING) return lives_osc_notify_failure();

  if (!lives_osc_check_arguments(arglen, vargs, "s", TRUE)) {
    if (!lives_osc_check_arguments(arglen, vargs, "si", TRUE)) {
      if (!lives_osc_check_arguments(arglen, vargs, "", TRUE)) {
        return lives_osc_notify_failure();
      }
    } else {
      lives_osc_parse_string_argument(vargs, setname);
      lives_osc_parse_int_argument(vargs, &force_append);
      boolstr = lives_strdup_printf("%d", force_append);
      if (!strcmp(boolstr, get_omc_const("LIVES_TRUE"))) force_append = TRUE;
      else {
        if (!strcmp(boolstr, get_omc_const("LIVES_FALSE"))) force_append = FALSE;
        else {
          lives_free(boolstr);
          return lives_osc_notify_failure();
        }
      }
      lives_free(boolstr);
    }
  } else {
    lives_osc_parse_string_argument(vargs, setname);
  }

  if (!*setname) {
    mainw->only_close = TRUE;
    ret = on_save_set_activate(NULL, NULL);
    mainw->only_close = FALSE;
    if (ret) return lives_osc_notify_success(NULL);
    else return lives_osc_notify_failure();
  }

  if (is_legal_set_name((tmp = U82F(setname)), TRUE, FALSE)) {
    mainw->only_close = TRUE;
    if (force_append) mainw->osc_auto = 2;
    else mainw->osc_auto = 1;
    ret = on_save_set_activate(NULL, setname);
    mainw->osc_auto = 0;
    mainw->only_close = FALSE;
    lives_free(tmp);
    if (ret) return lives_osc_notify_success(NULL);
    else return lives_osc_notify_failure();
  }

  lives_free(tmp);

  return lives_osc_notify_failure();
}

typedef void (*osc_cb)(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);

static struct {
  char	 *descr;
  char	 *name;
  void (*cb)(void *ctx, int len, const void *vargs, OSCTimeTag when,	NetworkReturnAddressPtr ra);
  int		 leave; // leaf
} osc_methods[] = {
  { "/record/enable",		"enable",	(osc_cb)lives_osc_record_start,			3	},
  { "/record/disable",	"disable",	(osc_cb)lives_osc_record_stop,			3	},
  { "/record/toggle",	        "toggle",	(osc_cb)lives_osc_record_toggle,			3	},
  { "/video/play",		"play",	(osc_cb)lives_osc_cb_play,			5	},
  { "/video/selection/play",		"play",	(osc_cb)lives_osc_cb_playsel,			46	},
  { "/video/play/forwards",		"forwards",	(osc_cb)lives_osc_cb_play_forward,			36	},
  { "/video/play/backwards",		"backwards",	(osc_cb)lives_osc_cb_play_backward,			36	},
  { "/video/play/faster",		"faster",	(osc_cb)lives_osc_cb_play_faster,			36	},
  { "/clip/foreground/fps/faster",		"faster",	(osc_cb)lives_osc_cb_play_faster,			61	},
  { "/clip/foreground/fps/get",		"get",	(osc_cb)lives_osc_cb_clip_getfps,			61	},
  { "/clip/background/fps/faster",		"faster",	(osc_cb)lives_osc_cb_bgplay_faster,			63	},
  { "/clip/background/fps/get",		"get",	(osc_cb)lives_osc_cb_bgclip_getfps,			63	},
  { "/video/play/slower",		"slower",	(osc_cb)lives_osc_cb_play_slower,			36	},
  { "/clip/foreground/fps/slower",		"slower",	(osc_cb)lives_osc_cb_play_slower,			61	},
  { "/clip/background/fps/slower",		"slower",	(osc_cb)lives_osc_cb_bgplay_slower,			63	},
  { "/video/play/reset",		"reset",	(osc_cb)lives_osc_cb_play_reset,			36	},
  { "/video/play/parameter/count",		"set",	(osc_cb)lives_osc_cb_rte_pparamcount,			140	},
  { "/video/play/parameter/value/set",		"set",	(osc_cb)lives_osc_cb_rte_setpparam,			140	},
  { "/video/play/parameter/flags/get",		"get",	(osc_cb)lives_osc_cb_rte_getpparamflags,        141	},
  { "/video/play/parameter/min/get",		"get",	(osc_cb)lives_osc_cb_rte_getpparammin,		        142	},
  { "/video/play/parameter/max/get",		"get",	(osc_cb)lives_osc_cb_rte_getpparammax,		        143	},
  { "/video/play/parameter/type/get",		"get",	(osc_cb)lives_osc_cb_rte_getpparamtype,		        144	},
  { "/video/play/parameter/name/get",		"get",	(osc_cb)lives_osc_cb_rte_getpparamname,		        145	},
  { "/video/play/parameter/colorspace/get",		"get",	(osc_cb)lives_osc_cb_rte_getpparamcspace,      	        146	},
  { "/video/play/parameter/default/get",		"get",	(osc_cb)lives_osc_cb_rte_getpparamdef,		        147	},
  { "/video/play/parameter/value/get",		"get",	(osc_cb)lives_osc_cb_rte_getpparamval,		        140	},
  { "/clip/foreground/fps/reset",		"reset",	(osc_cb)lives_osc_cb_play_reset,			61	},
  { "/clip/background/fps/reset",		"reset",	(osc_cb)lives_osc_cb_bgplay_reset,			63	},
  { "/video/stop",		"stop", (osc_cb)lives_osc_cb_stop,				5	},
  { "/video/fps/set",	       "set",	(osc_cb)lives_osc_cb_set_fps,			40	},
  { "/video/fps/get",	       "get",	(osc_cb)lives_osc_cb_clip_getfps,			40	},
  { "/video/loop/set",	       "set",	(osc_cb)lives_osc_cb_set_loop,			38	},
  { "/video/loop/get",	       "get",	(osc_cb)lives_osc_cb_get_loop,			38	},
  { "/video/pingpong/set",	       "set",	(osc_cb)lives_osc_cb_set_pingpong,			39	},
  { "/video/pingpong/get",	       "get",	(osc_cb)lives_osc_cb_get_pingpong,			39	},
  { "/lives/mode/set",	       "set",	(osc_cb)lives_osc_cb_setmode,			103	},
  { "/lives/mode/get",	       "get",	(osc_cb)lives_osc_cb_getmode,			103	},
  { "/video/fps/ratio/set",	       "set",	(osc_cb)lives_osc_cb_set_fps_ratio,			65	},
  { "/video/fps/ratio/get",	       "get",	(osc_cb)lives_osc_cb_get_fps_ratio,			65	},
  { "/video/play/time/get",	       "get",	(osc_cb)lives_osc_cb_get_playtime,			67	},
  { "/audio/mute/get",	       "get",	(osc_cb)lives_osc_cb_get_amute,			300	},
  { "/audio/mute/set",	       "set",	(osc_cb)lives_osc_cb_set_amute,			300	},
  { "/audio/volume/get",	       "get",	(osc_cb)lives_osc_cb_get_avol,			301	},
  { "/audio/volume/set",	       "set",	(osc_cb)lives_osc_cb_set_avol,			301	},
  { "/audio/source/get",	       "get",	(osc_cb)lives_osc_cb_pref_get_audio_source,			302	},
  { "/audio/source/set",	       "set",	(osc_cb)lives_osc_cb_pref_set_audio_source,			302	},
  { "/clip/foreground/fps/set",	"set",	(osc_cb)lives_osc_cb_set_fps,			61	},
  { "/clip/background/fps/set",	"set",	(osc_cb)lives_osc_cb_bgset_fps,			63	},
  { "/clip/foreground/fps/ratio/set",	"set",	(osc_cb)lives_osc_cb_set_fps_ratio,			64	},
  { "/clip/foreground/fps/ratio/get",	"get",	(osc_cb)lives_osc_cb_get_fps_ratio,			64	},
  { "/clip/background/fps/ratio/set",	"set",	(osc_cb)lives_osc_cb_bgset_fps_ratio,			66	},
  { "/clip/background/fps/ratio/get",	"get",	(osc_cb)lives_osc_cb_bgget_fps_ratio,			66	},
  { "/video/play/reverse",		"reverse",	(osc_cb)lives_osc_cb_play_reverse,		36	},
  { "/clip/foreground/fps/reverse",	"reverse",	(osc_cb)lives_osc_cb_play_reverse,		61	},
  { "/clip/background/fps/reverse",	"reverse",	(osc_cb)lives_osc_cb_bgplay_reverse,		63	},
  { "/video/freeze/toggle",		"toggle", (osc_cb)lives_osc_cb_freeze,		37	},
  { "/effects/realtime/name/get",		"get",	(osc_cb)lives_osc_cb_fx_getname,			115	},
  { "/effect_key/map",		"map",	(osc_cb)lives_osc_cb_fx_map,			25	},
  { "/effect_key/unmap",		"unmap",	(osc_cb)lives_osc_cb_fx_unmap,			25	},
  { "/effect_key/map/clear",		"clear",	(osc_cb)lives_osc_cb_fx_map_clear,			32	},
  { "/effect_key/reset",		"reset",	(osc_cb)lives_osc_cb_fx_reset,			25	},
  { "/effect_key/enable",		"enable",	(osc_cb)lives_osc_cb_fx_enable,		        25	},
  { "/effect_key/disable",		"disable",	(osc_cb)lives_osc_cb_fx_disable,		        25	},
  { "/effect_key/toggle",		"toggle",	(osc_cb)lives_osc_cb_fx_toggle,		        25	},
  { "/effect_key/count",		"count",	(osc_cb)lives_osc_cb_rte_count,		        25	},
  { "/effect_key/parameter/value/set",		"set",	(osc_cb)lives_osc_cb_rte_setparam,		        42	},
  { "/effect_key/parameter/type/get",		"get",	(osc_cb)lives_osc_cb_rte_getparamtype,		        68	},
  { "/effect_key/outparameter/type/get",		"get",	(osc_cb)lives_osc_cb_rte_getoparamtype,		        153	},
  { "/effect_key/nparameter/type/get",		"get",	(osc_cb)lives_osc_cb_rte_getnparamtype,		        116	},
  { "/effect_key/parameter/name/get",		"get",	(osc_cb)lives_osc_cb_rte_getparamname,		        71	},
  { "/effect_key/outparameter/name/get",		"get",	(osc_cb)lives_osc_cb_rte_getoparamname,		        152	},
  { "/effect_key/nparameter/name/get",		"get",	(osc_cb)lives_osc_cb_rte_getnparamname,		        72	},
  { "/effect_key/parameter/colorspace/get",		"get",	(osc_cb)lives_osc_cb_rte_getparamcspace,		        73	},
  { "/effect_key/outparameter/colorspace/get",		"get",	(osc_cb)lives_osc_cb_rte_getoparamcspace,		        154	},
  { "/effect_key/parameter/flags/get",		"get",	(osc_cb)lives_osc_cb_rte_getparamflags,		        74	},
  { "/effect_key/parameter/min/get",		"get",	(osc_cb)lives_osc_cb_rte_getparammin,		        75	},
  { "/effect_key/parameter/max/get",		"get",	(osc_cb)lives_osc_cb_rte_getparammax,		        76	},
  { "/effect_key/parameter/default/get",		"get",	(osc_cb)lives_osc_cb_rte_getparamdef,		        77	},
  { "/effect_key/parameter/default/set",		"set",	(osc_cb)lives_osc_cb_rte_setparamdef,		        77	},
  { "/effect_key/parameter/group/get",		"get",	(osc_cb)lives_osc_cb_rte_getparamgrp,		        78	},
  { "/effect_key/parameter/gui/choices/count",	"count",	(osc_cb)lives_osc_cb_pgui_countchoices,		        181	},
  { "/effect_key/parameter/gui/choices/get",	"get", (osc_cb)lives_osc_cb_pgui_getchoice,		        181	},
  { "/effect_key/outparameter/min/get",		"get",	(osc_cb)lives_osc_cb_rte_getoparammin,		        156	},
  { "/effect_key/outparameter/max/get",		"get",	(osc_cb)lives_osc_cb_rte_getoparammax,		        157	},
  { "/effect_key/outparameter/default/get",		"get",	(osc_cb)lives_osc_cb_rte_getoparamdef,		        158	},
  { "/effect_key/outparameter/has_min",		"has_min",	(osc_cb)lives_osc_cb_rte_getohasparammin,		        150	},
  { "/effect_key/outparameter/has_max",		"has_max",	(osc_cb)lives_osc_cb_rte_getohasparammax,		        150	},
  { "/effect_key/outparameter/has_default",		"has_default",	(osc_cb)lives_osc_cb_rte_getohasparamdef,		        150	},
  { "/effect_key/parameter/has_default",		"has_default",	(osc_cb)lives_osc_cb_rte_gethasparamdef,		        41	},
  { "/effect_key/parameter/value/get",		"get",	(osc_cb)lives_osc_cb_rte_getparamval,		        42	},
  { "/effect_key/outparameter/value/get",		"get",	(osc_cb)lives_osc_cb_rte_getoparamval,		        155	},
  { "/effect_key/nparameter/count",		"count",	(osc_cb)lives_osc_cb_rte_nparamcount,		        91	},
  { "/effect_key/parameter/count",		"count",	(osc_cb)lives_osc_cb_rte_paramcount,		        41	},
  { "/effect_key/outparameter/count",		"count",	(osc_cb)lives_osc_cb_rte_oparamcount,		        150	},
  { "/effect_key/nparameter/value/set",		"set",	(osc_cb)lives_osc_cb_rte_setnparam,		        92	},
  { "/effect_key/nparameter/value/get",		"get",	(osc_cb)lives_osc_cb_rte_getnparam,		        92	},
  { "/effect_key/nparameter/min/get",		"get",	(osc_cb)lives_osc_cb_rte_getnparammin,		        93	},
  { "/effect_key/nparameter/max/get",		"get",	(osc_cb)lives_osc_cb_rte_getnparammax,		        94	},
  { "/effect_key/nparameter/default/get",		"get",	(osc_cb)lives_osc_cb_rte_getnparamdef,		        95	},
  { "/effect_key/nparameter/default/set",		"set",	(osc_cb)lives_osc_cb_rte_setnparamdef,		        95	},
  { "/effect_key/nparameter/is_transition",		"is_transition",	(osc_cb)lives_osc_cb_rte_getnparamtrans,		        91	},
  { "/effect_key/parameter/is_transition",		"is_transition",	(osc_cb)lives_osc_cb_rte_getparamtrans,		        41	},
  { "/effect_key/inchannel/active/count",		"count",	(osc_cb)lives_osc_cb_rte_getnchannels,		        131	},
  { "/effect_key/inchannel/palette/get",		"get",	(osc_cb)lives_osc_cb_rte_getinpal,		        132	},
  { "/effect_key/outchannel/active/count",		"count",	(osc_cb)lives_osc_cb_rte_getnochannels,		        171	},
  { "/effect_key/outchannel/palette/get",		"get",	(osc_cb)lives_osc_cb_rte_getoutpal,		        162	},
  { "/effect_key/mode/set",		"set",	(osc_cb)lives_osc_cb_rte_setmode,		        43	},
  { "/effect_key/mode/get",		"get",	(osc_cb)lives_osc_cb_rte_getmode,		        43	},
  { "/effect_key/mode/next",		"next",	(osc_cb)lives_osc_cb_rte_nextmode,		        43	},
  { "/effect_key/mode/previous",	"previous",	(osc_cb)lives_osc_cb_rte_prevmode,		        43	},
  { "/effect_key/name/get",		"get",	(osc_cb)lives_osc_cb_rte_get_keyfxname,		        44	},
  { "/effect_key/maxmode/get",		"get",	(osc_cb)lives_osc_cb_rte_getmodespk,		        45	},
  { "/effect_key/state/get",		"get",	(osc_cb)lives_osc_cb_rte_getstate,		        56	},
  { "/effect_key/outparameter/connection/add",		"add",	(osc_cb)lives_osc_cb_rte_addpconnection,		        151	},
  { "/effect_key/outparameter/connection/delete",		"delete",	(osc_cb)lives_osc_cb_rte_delpconnection,		        151	},
  { "/effect_key/outparameter/connection/list",		"list",	(osc_cb)lives_osc_cb_rte_listpconnection,		        151	},
  { "/effect_key/outchannel/connection/add",		        "add",	(osc_cb)lives_osc_cb_rte_addcconnection,		        161	},
  { "/effect_key/outchannel/connection/delete",		"delete",	(osc_cb)lives_osc_cb_rte_delcconnection,		        161	},
  { "/effect_key/outchannel/connection/list",		"list",	(osc_cb)lives_osc_cb_rte_listcconnection,		        161	},
  { "/clip/encode_as",		"encode_as",	(osc_cb)lives_osc_cb_clip_encodeas,			1	},
  { "/clip/select",		"select",	(osc_cb)lives_osc_cb_fgclip_select,			1	},
  { "/clip/close",		"close",	(osc_cb)lives_osc_cb_clip_close,	  		        1	},
  { "/clip/copy",		"copy",	(osc_cb)lives_osc_cb_fgclip_copy,	  		        1	},
  { "/clip/undo",		"undo",	(osc_cb)lives_osc_cb_clip_undo,	  		        1	},
  { "/clip/redo",		"redo",	(osc_cb)lives_osc_cb_clip_redo,	  		        1	},
  { "/clip/selection/copy",		"copy",	(osc_cb)lives_osc_cb_fgclipsel_copy,	  		        55	},
  { "/clip/selection/cut",		"cut",	(osc_cb)lives_osc_cb_fgclipsel_cut,	  		        55	},
  { "/clip/selection/delete",		"delete",	(osc_cb)lives_osc_cb_fgclipsel_delete,	  		        55	},
  { "/clip/selection/rte_apply",		"rte_apply",	(osc_cb)lives_osc_cb_fgclipsel_rteapply,	  		        55	},
  { "/clipboard/paste",		"paste",	(osc_cb)lives_osc_cb_clipbd_paste,			70	},
  { "/clipboard/insert_before",		"insert_before",	(osc_cb)lives_osc_cb_clipbd_insertb,			70	},
  { "/clipboard/insert_after",		"insert_after",	(osc_cb)lives_osc_cb_clipbd_inserta,			70	},
  { "/clip/retrigger",		"retrigger",	(osc_cb)lives_osc_cb_fgclip_retrigger,			1	},
  { "/clip/resample",		        "resample",	(osc_cb)lives_osc_cb_clip_resample,			1	},
  { "/clip/select/next",		"next",	(osc_cb)lives_osc_cb_fgclip_select_next,			54	},
  { "/clip/select/previous",		"previous",	(osc_cb)lives_osc_cb_fgclip_select_previous,			54	},
  { "/clip/foreground/select",		"select",	(osc_cb)lives_osc_cb_fgclip_select,			47	},
  { "/clip/background/select",		"select",	(osc_cb)lives_osc_cb_bgclip_select,			48	},
  { "/clip/foreground/retrigger",		"retrigger",	(osc_cb)lives_osc_cb_fgclip_retrigger,			47	},
  { "/clip/background/retrigger",		"retrigger",	(osc_cb)lives_osc_cb_bgclip_retrigger,			48	},
  { "/clip/foreground/set",		"set",	(osc_cb)lives_osc_cb_fgclip_set,			47	},
  { "/clip/background/set",		"set",	(osc_cb)lives_osc_cb_bgclip_set,			48	},
  { "/clip/foreground/get",		"get",	(osc_cb)lives_osc_cb_clip_get_current,			47	},
  { "/clip/background/get",		"get",	(osc_cb)lives_osc_cb_bgclip_get_current,			48	},
  { "/clip/foreground/next",		"next",	(osc_cb)lives_osc_cb_fgclip_select_next,			47	},
  { "/clip/background/next",		"next",	(osc_cb)lives_osc_cb_bgclip_select_next,			48	},
  { "/clip/foreground/previous",		"previous",	(osc_cb)lives_osc_cb_fgclip_select_previous,			47	},
  { "/clip/background/previous",		"previous",	(osc_cb)lives_osc_cb_bgclip_select_previous,			48	},
  { "/lives/quit",	         "quit", (osc_cb)lives_osc_cb_quit,			21	},
  { "/lives/version/get",	         "get", (osc_cb)lives_osc_cb_getversion,			24	},
  { "/lives/status/get",	         "get", (osc_cb)lives_osc_cb_getstatus,			122	},
  { "/lives/constant/value/get",	         "get", (osc_cb)lives_osc_cb_getconst,			121	},
  { "/app/quit",	         "quit", (osc_cb)lives_osc_cb_quit,			22	},
  { "/app/name",	         "name", (osc_cb)lives_osc_cb_getname,			22	},
  { "/app/name/get",	         "get", (osc_cb)lives_osc_cb_getname,			23	},
  { "/app/version/get",	         "get", (osc_cb)lives_osc_cb_getversion,			125	},
  { "/quit",	         "quit", (osc_cb)lives_osc_cb_quit,			2	},
  { "/reply_to",	         "reply_to", (osc_cb)lives_osc_cb_open_status_socket,			2	},
  { "/lives/open_status_socket",	         "open_status_socket", (osc_cb)lives_osc_cb_open_status_socket,			21	},
  { "/app/open_status_socket",	         "open_status_socket", (osc_cb)lives_osc_cb_open_status_socket,			22	},
  { "/app/ping",	         "ping", (osc_cb)lives_osc_cb_ping,			22	},
  { "/lives/ping",	         "ping", (osc_cb)lives_osc_cb_ping,			21	},
  { "/ping",	         "ping", (osc_cb)lives_osc_cb_ping,			2	},
  { "/notify_to",	         "notify_to", (osc_cb)lives_osc_cb_open_notify_socket,			2	},
  { "/lives/open_notify_socket",	         "open_notify_socket", (osc_cb)lives_osc_cb_open_notify_socket,			21	},
  { "/notify/confirmations/set",	         "set", (osc_cb)lives_osc_cb_notify_c,			101	},
  { "/notify/events/set",	         "set", (osc_cb)lives_osc_cb_notify_e,			102	},
  { "/clip/count",	         "count", (osc_cb)lives_osc_cb_clip_count,			1  },
  { "/clip/goto",	         "goto", (osc_cb)lives_osc_cb_clip_goto,			1	},
  { "/clip/foreground/frame/set",	         "set", (osc_cb)lives_osc_cb_clip_goto,			60	},
  { "/clip/foreground/frame/get",	         "get", (osc_cb)lives_osc_cb_clip_getframe,			60	},
  { "/clip/background/frame/set",	         "set", (osc_cb)lives_osc_cb_bgclip_goto,			62	},
  { "/clip/background/frame/get",	         "get", (osc_cb)lives_osc_cb_bgclip_getframe,			62	},
  { "/clip/is_valid/get",	         "get", (osc_cb)lives_osc_cb_clip_isvalid,			49	},
  { "/clip/frame/count",	         "count", (osc_cb)lives_osc_cb_clip_get_frames,			57	},
  { "/clip/frame/save_as_image",	         "save_as_image", (osc_cb)lives_osc_cb_clip_save_frame,			57	},
  { "/clip/select_all",	         "select_all", (osc_cb)lives_osc_cb_clip_select_all,			1	},
  { "/clip/start/set",	 "set", (osc_cb)lives_osc_cb_clip_set_start,			50	},
  { "/clip/start/get",	 "get", (osc_cb)lives_osc_cb_clip_get_start,			50	},
  { "/clip/end/set",	 "set", (osc_cb)lives_osc_cb_clip_set_end,			51	},
  { "/clip/end/get",	 "get", (osc_cb)lives_osc_cb_clip_get_end,			51	},
  { "/clip/size/get",	 "get", (osc_cb)lives_osc_cb_clip_get_size,			58	},
  { "/clip/name/get",	 "get", (osc_cb)lives_osc_cb_clip_get_name,			59	},
  { "/clip/name/set",	 "set", (osc_cb)lives_osc_cb_clip_set_name,			59	},
  { "/clip/fps/get",	 "get", (osc_cb)lives_osc_cb_clip_get_ifps,			113	},
  { "/clip/open/file",	 "file", (osc_cb)lives_osc_cb_open_file,			33	},
  { "/clip/open/unicap",	 "unicap", (osc_cb)lives_osc_cb_open_unicap,			33	},
  { "/clip/audio/new",	 "new", (osc_cb)lives_osc_cb_new_audio,			108	},
  { "/output/fullscreen/enable",		"enable",	(osc_cb)lives_osc_cb_fssepwin_enable,		28	},
  { "/output/fullscreen/disable",		"disable",	(osc_cb)lives_osc_cb_fssepwin_disable,       	28	},
  { "/output/fps/set",		"set",	(osc_cb)lives_osc_cb_op_fps_set,       	52	},
  { "/output/nodrop/enable",		"enable",	(osc_cb)lives_osc_cb_op_nodrope,       	30	},
  { "/output/nodrop/disable",		"disable",	(osc_cb)lives_osc_cb_op_nodropd,       	30	},
  { "/clip/foreground/background/swap",		"swap",	(osc_cb)lives_osc_cb_swap,       	53	},
  { "/clipset/load",		"load",	(osc_cb)lives_osc_cb_loadset,       	35	},
  { "/clipset/save",		"save",	(osc_cb)lives_osc_cb_saveset,       	35	},
  { "/clipset/name/get",		"get",	(osc_cb)lives_osc_cb_getsetname,       	135	},
  { "/layout/clear",		"clear",	(osc_cb)lives_osc_cb_clearlay,       	104	},
  { "/block/count",		"count",	(osc_cb)lives_osc_cb_blockcount,       	105	},
  { "/block/insert",		"insert",	(osc_cb)lives_osc_cb_blockinsert,       	105	},
  { "/block/start/time/get",		"get",	(osc_cb)lives_osc_cb_blockstget,       	111	},
  { "/block/end/time/get",		"get",	(osc_cb)lives_osc_cb_blockenget,       	112	},
  { "/mt/time/get",		"get",	(osc_cb)lives_osc_cb_mtctimeget,       	201	},
  { "/mt/time/set",		"set",	(osc_cb)lives_osc_cb_mtctimeset,       	201	},
  { "/mt/ctrack/get",		"get",	(osc_cb)lives_osc_cb_mtctrackget,       	201	},
  { "/mt/ctrack/set",		"set",	(osc_cb)lives_osc_cb_mtctrackset,       	201	},
  { "/test",		"",	(osc_cb)lives_osc_cb_test,       	500	},

  { NULL,					NULL,		NULL,							0	},
};

static struct {
  char *comment; // leaf comment
  char *name;  // leaf name
  int  leave; // leaf number
  int  att;  // attached to parent number
  int  it; // ???
} osc_cont[] = {
  {	"/",	 	"",	                 2, -1, 0   	},
  {	"/video/",	 	"video",	 5, -1, 0   	},
  {	"/video/selection/",	 	"selection",	 46, 5, 0   	},
  {	"/video/fps/",	 	"fps",	 40, 5, 0   	},
  {	"/video/fps/ratio/",	 	"ratio",	 65, 40, 0   	},
  {	"/video/play/ start video playback",	 	"play",	         36, 5, 0   	},
  {	"/video/play/time",	 	"time",	         67, 36, 0   	},
  {	"/video/play/parameter",	 	"parameter",	         69, 36, 0   	},
  {	"/video/play/parameter/value",	 	"value",	         140, 69, 0   	},
  {	"/video/play/parameter/flags",	 	"flags",	         141, 69, 0   	},
  {	"/video/play/parameter/min",	 	"min",	         142, 69, 0   	},
  {	"/video/play/parameter/max",	 	"max",	         143, 69, 0   	},
  {	"/video/play/parameter/type",	 	"type",	         144, 69, 0   	},
  {	"/video/play/parameter/name",	 	"name",	         145, 69, 0   	},
  {	"/video/play/parameter/colorspace",	"colorspace",	 146, 69, 0   	},
  {	"/video/play/parameter/default",	"default",	 147, 69, 0   	},
  {	"/video/freeze/",	"freeze",        37, 5, 0   	},
  {	"/video/loop/",	"loop",        38, 5, 0   	},
  {	"/video/pingpong/",	"pingpong",        39, 5, 0   	},
  {	"/audio/",	 	"audio",	 6, -1, 0   	},
  {	"/audio/mute",	 	"mute",	 300, 6, 0   	},
  {	"/audio/volume",	 	"volume",	 301, 6, 0   	},
  {	"/audio/source",	 	"source",	 302, 6, 0   	},
  {	"/clip/", 		"clip",		 1, -1, 0	},
  {	"/clip/fps/", 		"fps",		 113, 1, 0	},
  {	"/clip/foreground/", 	"foreground",    47, 1, 0	},
  {	"/clip/foreground/valid/", 	"valid",    80, 1, 0	},
  {	"/clip/foreground/background/",  "background",    53, 47, 0	},
  {	"/clip/foreground/frame/",  "frame",    60, 47, 0	},
  {	"/clip/foreground/fps/",  "fps",    61, 47, 0	},
  {	"/clip/foreground/fps/ratio/",  "ratio",    64, 61, 0	},
  {	"/clip/background/", 	"background",    48, 1, 0	},
  {	"/clip/background/valid/", 	"valid",    81, 1, 0	},
  {	"/clip/background/frame/",  "frame",    62, 48, 0	},
  {	"/clip/background/fps/",  "fps",    63, 48, 0	},
  {	"/clip/background/fps/ratio/",  "ratio",    66, 63, 0	},
  {	"/clip/is_valid/", 	"is_valid",      49, 1, 0	},
  {	"/clip/frame/", 	"frame",      57, 1, 0	},
  {	"/clip/start/", 	"start",         50, 1, 0	},
  {	"/clip/end/", 	        "end",           51, 1, 0	},
  {	"/clip/select/", 	        "select",           54, 1, 0	},
  {	"/clip/selection/", 	        "selection",           55, 1, 0	},
  {	"/clip/size/", 	        "size",           58, 1, 0	},
  {	"/clip/name/", 	        "name",           59, 1, 0	},
  {	"/clip/audio/", 	"audio",           108, 1, 0	},
  {	"/clipboard/", 		"clipboard",		 70, -1, 0	},
  {	"/record/", 		"record",	 3, -1, 0	},
  {	"/effect/", 		"effects",	 4, -1, 0	},
  {	"/effect/realtime/", 		"realtime",	 114, 4, 0	},
  {	"/effect/realtime/name/", 		"name",	 115, 114, 0	},
  {	"/effect_key/", 		"effect_key",	 25, -1, 0	},
  {	"/effect_key/inchannel/", 	"inchannel",	 130, 25, 0	},
  {	"/effect_key/inchannel/active/", 	"active",	 131, 130, 0	},
  {	"/effect_key/inchannel/palette/", 	"palette",	 132, 130, 0	},
  {	"/effect_key/parameter/", 	"parameter",	 41, 25, 0	},
  {	"/effect_key/parameter/value/", "value",	 42, 41, 0	},
  {	"/effect_key/parameter/type/", "type",	 68, 41, 0	},
  {	"/effect_key/parameter/name/", "name",	 71, 41, 0	},
  {	"/effect_key/parameter/colorspace/", "colorspace",	 73, 41, 0	},
  {	"/effect_key/parameter/flags/", "flags",	 74, 41, 0	},
  {	"/effect_key/parameter/min/", "min",	 75, 41, 0	},
  {	"/effect_key/parameter/max/", "max",	 76, 41, 0	},
  {	"/effect_key/parameter/default/", "default",	 77, 41, 0	},
  {	"/effect_key/parameter/group/", "group",	 78, 41, 0	},
  {	"/effect_key/parameter/gui/", "gui",	 180, 41, 0	},
  {	"/effect_key/parameter/gui/choices", "choices",	 181, 180, 0	},
  {	"/effect_key/nparameter/", 	"nparameter",	 91, 25, 0	},
  {	"/effect_key/nparameter/name/", "name",	 72, 91, 0	},
  {	"/effect_key/nparameter/value/", "value",	 92, 91, 0	},
  {	"/effect_key/nparameter/type/", "type",	 116, 91, 0	},
  {	"/effect_key/nparameter/min/", "min",	 93, 91, 0	},
  {	"/effect_key/nparameter/max/", "max",	 94, 91, 0	},
  {	"/effect_key/nparameter/default/", "default",	 95, 91, 0	},
  {	"/effect_key/map/", 		"map",	 32, 25, 0	},
  {	"/effect_key/mode/", 		"mode",	 43, 25, 0	},
  {	"/effect_key/name/", 		"name",	 44, 25, 0	},
  {	"/effect_key/maxmode/", 	"maxmode",	 45, 25, 0	},
  {	"/effect_key/state/", 	"state",	 56, 25, 0	},
  {	"/effect_key/outchannel/", 	"outchannel",	 160, 25, 0	},
  {	"/effect_key/outchannel/connection/", 	"connection",	 161, 160, 0	},
  {	"/effect_key/outchannel/palette/", 	"palette",	 162, 160, 0	},
  {	"/effect_key/outchannel/active/", 	"active",	 171, 160, 0	},
  {	"/effect_key/outparameter/", 	"outparameter",	 150, 25, 0	},
  {	"/effect_key/outparameter/connection/", 	"connection",	 151, 150, 0	},
  {	"/effect_key/outparameter/name/", 	"name",	 152, 150, 0	},
  {	"/effect_key/outparameter/type/", 	"type",	 153, 150, 0	},
  {	"/effect_key/outparameter/colorspace/", 	"colorspace",	 154, 150, 0	},
  {	"/effect_key/outparameter/value/", 	"value",	 155, 150, 0	},
  {	"/effect_key/outparameter/min/", 	"min",	 156, 150, 0	},
  {	"/effect_key/outparameter/max/", 	"max",	 157, 150, 0	},
  {	"/effect_key/outparameter/default/", 	"default",	 158, 150, 0	},
  {	"/lives/", 		"lives",	 21, -1, 0	},
  {	"/lives/version/", 		"version",	 24, 21, 0	},
  {	"/lives/mode/", 		"mode",	 103, 21, 0	},
  {	"/lives/status/", 		"status",	 122, 21, 0	},
  {	"/lives/constant/", 		"constant",	 120, 21, 0	},
  {	"/lives/constant/value/", 		"value",	 121, 120, 0	},
  {	"/clipset/", 		"clipset",	 35, -1, 0	},
  {	"/clipset/name/", 		"name",	 135, 35, 0	},
  {	"/app/", 		"app",	         22, -1, 0	},
  {	"/app/name/", 		"name",	         23, 22, 0	},
  {	"/app/version/", 		"version",	         125, 22, 0	},
  {	"/output/", 	"output",	 27, -1, 0	},
  {	"/output/fullscreen/", 	"fullscreen",	 28, 27, 0	},
  {	"/output/fps/", 	        "fps",	 52, 27, 0	},
  {	"/output/nodrop/", 	"nodrop",	 30, 27, 0	},
  {	"/clip/open/",   		"open",		 33, 1, 0	},
  {	"/notify/",   		"notify",		 100, -1, 0	},
  {	"/notify/confirmations/",   		"confirmations",		 101, 100, 0	},
  {	"/notify/events/",   		"events",		 102, 100, 0	},
  {	"/layout/",   		"layout",		 104, -1, 0	},
  {	"/block/",   		"block",		 105, -1, 0	},
  {	"/block/start/",   		"start",		 106, 105, 0	},
  {	"/block/start/time/",   		"time",		 111, 106, 0	},
  {	"/block/end/",   		"end",		 107, 105, 0	},
  {	"/block/end/time/",   		"time",		 112, 107, 0	},
  {	"/mt/",   		"mt",		 200, -1, 0	},
  {	"/mt/ctime/",   		"ctime",		 201, 200, 0	},
  {	"/mt/ctrack/",   		"ctrack",		 202, 200, 0	},
  {	"/test/",   		"test",		 500, -1, 0	},
  {	NULL,			NULL,		0, -1, 0		},
};


int lives_osc_build_cont(lives_osc * o) {
  /* Create containers /video , /clip, /chain and /tag */
  register int i;
  for (i = 0; osc_cont[i].name ; i++) {
    if (osc_cont[i].it == 0) {
      o->cqinfo.comment = osc_cont[i].comment;

      // add a container to a leaf
      if ((o->leaves[osc_cont[i].leave] =
             OSCNewContainer(osc_cont[i].name,
                             (osc_cont[i].att == -1 ? o->container : o->leaves[osc_cont[i].att]),
                             &(o->cqinfo))) == 0) {
        if (osc_cont[i].att == - 1) {
          lives_printerr("Cannot create container %d (%s) \n",
                         i, osc_cont[i].name);
          return 0;
        } else {
          lives_printerr("Cannot add branch %s to  container %d)\n",
                         osc_cont[i].name, osc_cont[i].att);
          return 0;
        }
      }
    } else {
      char name[50];
      char comment[50];
      int n = osc_cont[i].it;
      int base = osc_cont[i].leave;
      register int j;

      for (j = 0; j < n ; j++) {
        sprintf(name, "N%d", j);
        sprintf(comment, "<%d>", j);
        lives_printerr("Try cont.%d  '%s', %d %d\n", j, name,
                       base + j, base);
        o->cqinfo.comment = comment;
        if ((o->leaves[base + j] = OSCNewContainer(name,
                                   o->leaves[osc_cont[i].att],
                                   &(o->cqinfo))) == 0) {
          lives_printerr("Cannot auto numerate container %s \n",
                         osc_cont[i].name);
          return 0;

        }
      }
    }
  }
  return 1;
}


int lives_osc_attach_methods(lives_osc * o) {
  int i;

  for (i = 0; osc_methods[i].name ; i++) {
    o->ris.description = osc_methods[i].descr;
    OSCNewMethod(osc_methods[i].name,
                 o->leaves[osc_methods[i].leave],
                 osc_methods[i].cb,
                 NULL, // this is the context which is reurned but it seems to be unused
                 & (o->ris));
  }
  return 1;
}


/* initialization, setup a UDP socket and invoke OSC magic */
lives_osc *lives_osc_allocate(int port_id) {
  lives_osc *o;

  if (!livesOSC) {
    o = (lives_osc *)lives_malloc(sizeof(lives_osc));
    //o->osc_args = (osc_arg*)lives_malloc(50 * sizeof(*o->osc_args));
    o->osc_args = NULL;
    o->rt.InitTimeMemoryAllocator = lives_osc_malloc;
    o->rt.RealTimeMemoryAllocator = lives_osc_malloc;
    o->rt.receiveBufferSize = 1024;
    o->rt.numReceiveBuffers = 100;
    o->rt.numQueuedObjects = 100;
    o->rt.numCallbackListNodes = 200;
    o->leaves = (OSCcontainer *) lives_malloc(sizeof(OSCcontainer) * 1000);
    o->t.initNumContainers = 1000;
    o->t.initNumMethods = 2000;
    o->t.InitTimeMemoryAllocator = lives_osc_malloc;
    o->t.RealTimeMemoryAllocator = lives_osc_malloc;

    if (!OSCInitReceive(&(o->rt))) {
      d_print(_("Cannot initialize OSC receiver\n"));
      return NULL;
    }
    o->packet = OSCAllocPacketBuffer();

    /* Top level container / */
    o->container = OSCInitAddressSpace(&(o->t));

    OSCInitContainerQueryResponseInfo(&(o->cqinfo));
    o->cqinfo.comment = "Video commands";

    if (!lives_osc_build_cont(o))
      return NULL;

    OSCInitMethodQueryResponseInfo(&(o->ris));

    if (!lives_osc_attach_methods(o))
      return NULL;
  } else o = livesOSC;

  if (port_id > 0) {
    if (NetworkStartUDPServer(o->packet, port_id) != TRUE) {
      d_print(_("WARNING: Cannot start OSC server at UDP port %d\n"), port_id);
    } else {
      d_print(_("Started OSC server at UDP port %d\n"), port_id);
    }
  }

  return o;
}


void lives_osc_dump(void) {OSCPrintWholeAddressSpace();}

// CALL THIS PERIODICALLY, will read all queued messages and call callbacks

/* get a packet */
static int lives_osc_get_packet(lives_osc * o) {
  //OSCTimeTag tag;

  /* see if there is something to read , this is effectively NetworkPacketWaiting */
  // if(ioctl( o->sockfd, FIONREAD, &bytes,0 ) == -1) return 0;
  // if(bytes==0) return 0;
  if (NetworkPacketWaiting(o->packet)) {
    /* yes, receive packet from UDP */
    if (NetworkReceivePacket(o->packet)) {
      /* OSC must accept this packet (OSC will auto-invoke it, see source !) */
      OSCAcceptPacket(o->packet);

#ifdef DEBUG_OSC
      g_print("got osc msg %s\n", OSCPacketBufferGetBuffer((OSCPacketBuffer)o->packet));
#endif
      /* Is this really productive ? */
      OSCBeProductiveWhileWaiting();
      // here we call the callbacks

      /* tell caller we had 1 packet */
      return 1;
    }
  }
  return 0;
}


static void oscbuf_to_packet(OSCbuf * obuf, OSCPacketBuffer packet) {
  int *psize = OSCPacketBufferGetSize(packet);
  int bufsize = OSC_packetSize(obuf);

  if (bufsize > 100) {
    LIVES_ERROR("error, OSC msglen > 100 !");
  }

  lives_memcpy(OSCPacketBufferGetBuffer(packet), OSC_getPacket(obuf), bufsize);
  *psize = bufsize;
}


boolean lives_osc_act(OSCbuf * obuf) {
  // this is a shortcut route to make LiVES carry out the OSC message in msg

  OSCPacketBuffer packet;

  if (!livesOSC) lives_osc_init(0);

  packet = livesOSC->packet;

  oscbuf_to_packet(obuf, packet);

  OSCAcceptPacket(packet);

  via_shortcut = TRUE;
  OSCBeProductiveWhileWaiting();
  via_shortcut = FALSE;

  return TRUE;
}


void lives_osc_free(lives_osc * c) {
  if (!c) return;
  if (c->leaves) free(c->leaves);
  if (c) free(c);
  c = NULL;
}


////////////////////////////// API public functions /////////////////////

boolean lives_osc_init(uint32_t udp_port) {
  if (livesOSC && udp_port != 0) {
    /*  OSCPacketBuffer packet=livesOSC->packet;
      if (shutdown (packet->returnAddr->sockfd,SHUT_RDWR)) {
      d_print( lives_strdup_printf (_("Cannot shut down OSC/UDP server\n"));
      }
    */
    if (NetworkStartUDPServer(livesOSC->packet, udp_port) != TRUE) {
      d_print(_("Cannot start OSC/UDP server at port %d \n"), udp_port);
    }
  } else {
    livesOSC = lives_osc_allocate(udp_port);
    if (!livesOSC) return FALSE;
    status_socket = NULL;
    notify_socket = NULL;
  }
  return TRUE;
}


boolean lives_osc_poll(livespointer data) {
  // data is always NULL
  // must return TRUE
  if (!mainw->osc_block && livesOSC) lives_osc_get_packet(livesOSC);
  return TRUE;
}


void lives_osc_end(void) {
  if (notify_socket) {
    lives_osc_notify(LIVES_OSC_NOTIFY_QUIT, "");
    lives_osc_close_notify_socket();
  }
  if (status_socket) {
    lives_osc_close_status_socket();
  }

  if (livesOSC) lives_osc_free(livesOSC);
  livesOSC = NULL;
}

#endif
