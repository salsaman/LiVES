// messaging.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "interface.h"
#include "htmsocket.h"

static pthread_mutex_t dprint_mutex = PTHREAD_MUTEX_INITIALIZER;

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


char *_dump_messages(int start, int end) {
  weed_plant_t *msg = mainw->msg_list;
  char *text = lives_strdup(""), *tmp;
  const char *msgtext;
  boolean needs_newline = FALSE;
  int msgno = 0;
  int error;

  while (msg) {
    msgtext = weed_get_const_string_value(msg, LIVES_LEAF_MESSAGE_STRING, &error);
    if (msgtext) {
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
    }
    if (++msgno > end) if (end > -1) break;
    msg = weed_get_plantptr_value(msg, WEED_LEAF_NEXT, &error);
    if (error != WEED_SUCCESS) break;
  }
  return text;
}


void dump_messages(FILE *stream) {
  char *msgs;
  if (!stream) stream = stderr;
  lives_fprintf(stream, "%s", (msgs = _dump_messages(-1, -1)));
  lives_free(msgs);
}


static int log_msg(FILE *logfile, const char *text) {
  if (text && logfile) lives_fputs(text, logfile);
  return 0;
}


#define LOGFILENAME "debug.log"

FILE *open_logfile(const char *logfilename) {
  FILE *logfile;
  char *xlog;
  if (!logfilename) {
    xlog = lives_build_filename(prefs->config_datadir, LOGFILENAME, NULL);
  } else xlog = lives_strndup(logfilename, PATH_MAX);
  g_printerr("ppening log %s\n", xlog);
  logfile = fopen(xlog, "w");
  lives_free(xlog);
  return logfile;
}


LIVES_GLOBAL_INLINE void close_logfile(FILE *logfile) {if (logfile) fclose(logfile);}


static weed_plant_t *make_msg(const char *text) {
  // make single msg. text should have no newlines in it, except possibly as the last character.
  if (!text) return NULL;
  else {
    weed_plant_t *msg = lives_plant_new(LIVES_PLANT_MESSAGE);
    if (!msg) return NULL;

    weed_set_const_string_value(msg, LIVES_LEAF_MESSAGE_STRING, text);
    //g_print("\n\ntext %d was %s\n", err, text);
    weed_set_plantptr_value(msg, WEED_LEAF_NEXT, NULL);
    weed_set_plantptr_value(msg, WEED_LEAF_PREVIOUS, NULL);
    return msg;
  }
}


weed_error_t free_n_msgs(int frval) {
  weed_error_t error;
  weed_plant_t *next, *end;

  if (frval <= 0) return WEED_SUCCESS;
  if (frval > mainw->n_messages || !mainw->msg_list) frval = mainw->n_messages;

  end = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, &error); // list end
  if (error != WEED_SUCCESS) return error;

  while (frval-- && mainw->msg_list) {
    next = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_NEXT, &error); // becomes new head
    if (error != WEED_SUCCESS) return error;
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


static weed_error_t _add_message_to_list(const char *text, boolean is_top) {
  // append text to our message list, splitting it into lines
  // if we hit the max message limit then free the oldest one
  // returns a weed error
  weed_plant_t *msg, *end = NULL;
  weed_plant_t *omsg_list = NULL;
  char **lines;
  weed_error_t error;
  int i, numlines, on_msgs = 0;

  if (prefs && prefs->max_messages == 0) return WEED_SUCCESS;
  if (!text || !*text) return WEED_SUCCESS;

  // split text into lines
  numlines = get_token_count(text, '\n');
  lines = lives_strsplit(text, "\n", numlines);

  if (is_top) {
    // first line becomes new top, old top is set aside temporarily
    omsg_list = mainw->msg_list;
    mainw->msg_list = NULL;
    on_msgs = mainw->n_messages;
    mainw->n_messages = 0;
  }

  for (i = 0; i < numlines; i++) {
    if (!mainw->msg_list) {
      mainw->msg_list = make_msg(lines[i]);
      if (!mainw->msg_list) {
        mainw->n_messages = 0;
        lives_strfreev(lines);
        return WEED_ERROR_MEMORY_ALLOCATION;
      }
      //g_print("\n\n\nADDED msg 1\n\n");
      mainw->n_messages = 1;
      continue;
    }

    if (!end) {
      end = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, &error);
      if (error != WEED_SUCCESS) {
        lives_strfreev(lines);
        return error;
      }
      if (!end) end = mainw->msg_list;
    }

    if (i == 0) {
      // append first line to text of last msg
      char *strg2;
      const char *strg = weed_get_const_string_value(end, LIVES_LEAF_MESSAGE_STRING, &error);
      if (error != WEED_SUCCESS) {
        lives_strfreev(lines);
        return error;
      }
      strg2 = lives_strdup_printf("%s%s", strg, lines[0]);
      weed_leaf_delete(end, LIVES_LEAF_MESSAGE_STRING);
      weed_set_const_string_value(end, LIVES_LEAF_MESSAGE_STRING, strg2);
      lives_free(strg2);
      /* g_print("GOT %s\n", */
      /*         weed_get_string_value(end, LIVES_LEAF_MESSAGE_STRING, NULL)); */
      //lives_free(strg2);
      continue;
    }

    if (prefs && prefs->max_messages > 0 && mainw->n_messages + 1 > prefs->max_messages) {
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

    end = msg;
  }
  lives_strfreev(lines);

  if (omsg_list) {
    mainw->n_messages += on_msgs;
    if (prefs && prefs->max_messages > 0 && mainw->n_messages > prefs->max_messages) {
      error = free_n_msgs(mainw->n_messages - prefs->max_messages);
      if (error != WEED_SUCCESS) return error;
    }

    if (!mainw->msg_list) mainw->msg_list = omsg_list;
    else {
      end = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, &error);
      if (error != WEED_SUCCESS) return error;
      // head will get new previous (oend)
      lives_leaf_dup(mainw->msg_list, omsg_list, WEED_LEAF_PREVIOUS);
      // omsg_list gets new previous (end)
      weed_set_plantptr_value(omsg_list, WEED_LEAF_PREVIOUS, end);
      // end will get new next omsg_list
      weed_set_plantptr_value(end, WEED_LEAF_NEXT, omsg_list);
    }
  }
  return WEED_SUCCESS;
}


static LiVESList *msgcache = NULL;

weed_error_t add_message_to_list(const char *text) {return _add_message_to_list(text, FALSE);}

lives_result_t add_message_first(const char *text) {
  MSGMODE_ON(STORE);
  MSGMODE_OFF(CACHE);
  _add_message_to_list(text, TRUE);
  if (prefs->show_msg_area) MSGMODE_ON(DISPLAY);
  if (msgcache) {
    msgcache = lives_list_reverse(msgcache);
    for (LiVESList *list = msgcache; list; list = list->next) {
      if (list->data) {
        d_print((const char *)list->data);
      }
    }
    lives_list_free(msgcache);
    msgcache = NULL;
  }
  return LIVES_RESULT_SUCCESS;
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
    lives_freep((void **)&mainw->urgency_msg);
    lives_sys_alarm_set_timeout(urgent_msg_timeout, timeout * ONE_BILLION);
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
    lives_sys_alarm_set_timeout(overlay_msg_timeout, timeout * ONE_BILLION);
    return TRUE;
  }
  lives_free(text);
  return FALSE;
}

LIVES_GLOBAL_INLINE void _cache_msg(const char *fmt, ...) {
  va_list xargs;
  char *text;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  if (text) msgcache = lives_list_prepend(msgcache, (void *)text);
}


#define DPRINT_SEP "==============================\n"

static boolean d_print_inner(const char *text) {
  boolean ret = TRUE;
  if (MSGMODE_HAS(CACHE)) {
    cache_msg(text);
    if (!prefs || !prefs->msg_routing) {
      if (mainw && mainw->debug) {
        fprintf(stderr, "%s", text);
        if (mainw->debug_log) log_msg(mainw->debug_log, text);
      }
    }
  }

  if (MSGMODE_HAS(STDERR)) fprintf(stderr, "%s", text);
  if (MSGMODE_HAS(LOGFILE)) log_msg(mainw->debug_log, text);

  if (MSGMODE_HAS(SOCKET)) {
    void *socket = THREADVAR(msgsocket);
    if (socket) ret = lives_stream_out(socket, lives_strlen(text) + 1, (void *)text);
  }

  if (MSGMODE_HAS(STORE)) {
    if (MSGMODE_HAS(FANCY) && !mainw->no_switch_dprint) {
      if (mainw->current_file != mainw->last_dprint_file && mainw->current_file != 0
          && !mainw->multitrack && (mainw->current_file == -1 || CURRENT_CLIP_IS_VALID)) {
        char *swtext, *tmp;
        if (CURRENT_CLIP_IS_VALID) {
          char *xtmp = get_menu_name(cfile, TRUE);
          tmp = lives_strdup_printf(_("clip %s"), xtmp);
          lives_free(xtmp);
        } else tmp = lives_strdup(_("empty clip"));

        swtext = lives_strdup_printf(_("\n%s\nSwitched to %s\n"), DPRINT_SEP, tmp);
        add_message_to_list(swtext);
        lives_free(swtext); lives_free(tmp);
      }
    }

    add_message_to_list(text);

    if (MSGMODE_HAS(DISPLAY)) {
      if (mainw->multitrack) {
        msg_area_scroll_to_end(mainw->multitrack->msg_area, mainw->multitrack->msg_adj);
        lives_widget_queue_draw_if_visible(mainw->multitrack->msg_area);
      } else {
        msg_area_scroll_to_end(mainw->msg_area, mainw->msg_adj);
        lives_widget_queue_draw_if_visible(mainw->msg_area);
      }
    }
  }
  return ret;
}


boolean d_print(const char *fmt, ...) {
  // collect output for "show messages" and optionally for main message area
  // (and other targets a defined by prefs->msg_routing)

  // initially, routing is set to MSG_ROUTE_CACHE. Messages sre stored in a linked list.
  // when add_message_first(text) is called routing is set to MSG_ROUTE_CACHE i cleared,  MSG_ROUTE_DISPLAY
  // (if msg area can be shown) is set,
  // and possibly MSG_ROUTE_STDERR, MSG_ROUTE_LOGFILE
  // and all cached messages are d_printed again.
  // At the end of the startup phase, MSG_ROUtE_FANCY is also set.
  //

  // there are several small tweaks for this:

  /// deprecating;
  // mainw->suppress_dprint :: TRUE - dont print anything, return (for silencing noisy message blocks)
  // NEW: prefs-.msg_routing |= MSG_ROUTE_BLOCKED

  // deprecating:
  // mainw->no_switch_dprint :: TRUE - disable printing of switch message when maine->current_file changes
  // NEW: prefs->msg_routing &= ~MSG_ROuTE_FANCY;

  // mainw->last_dprint_file :: clip number of last mainw->current_file;
  va_list xargs;
  char *text;
  boolean ret;

  if (mainw && mainw->suppress_dprint) return FALSE;
  if (prefs && MSGMODE_HAS(BLOCK)) return FALSE;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  pthread_mutex_lock(&dprint_mutex);

  ret = d_print_inner(text);

  if (mainw && (mainw->current_file == -1 || (cfile && cfile->clip_type != CLIP_TYPE_GENERATOR)) &&
      (!mainw->no_switch_dprint || mainw->current_file != 0))
    mainw->last_dprint_file = mainw->current_file;

  pthread_mutex_unlock(&dprint_mutex);
  return ret;
}


LIVES_GLOBAL_INLINE void d_print_debug(const char *fmt, ...) {
  // print out but only if MSGMODE_ON(DEBUG)
  va_list xargs;
  if (MSGMODE_HAS(DEBUG) || mainw->debug) {
    va_start(xargs, fmt);
    vfprintf(stderr, fmt, xargs);
    va_end(xargs);
  }
}


void d_print_utility(const char *text) {
  boolean nsdp = MSGMODE_HAS(FANCY);
  if (nsdp) MSGMODE_OFF(FANCY);
  d_print(text);
  if (nsdp) {
    MSGMODE_ON(FANCY);
    d_print("");
  }
}

LIVES_GLOBAL_INLINE void d_print_done(void) {d_print_utility(_("done.\n"));}

LIVES_GLOBAL_INLINE void d_print_file_error_failed(void) {d_print_utility(_("error in file. Failed.\n"));}

LIVES_GLOBAL_INLINE void d_print_enough(int frames) {
  if (!frames) d_print_cancelled();
  else {
    char *msg = lives_strdup_printf(P_("%d frame is enough !\n", "%d frames are enough !\n", frames), frames);
    d_print_utility(msg);
    lives_free(msg);
  }
}
