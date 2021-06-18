// messaging.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "interface.h"

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


static int log_msg(FILE *logfile, const char *text) {
  if (text && logfile) {
    lives_fputs(text, logfile);
  }
  return 0;
}


#define LOGFILENAME "debug.log"

FILE *open_logfile(const char *logfilename) {
  FILE *logfile;
  char *xlog;
  if (!logfilename) {
    xlog = lives_build_filename(prefs->config_datadir, LOGFILENAME, NULL);
  } else xlog = lives_strndup(logfilename, PATH_MAX);
  g_print("ppening log %s\n", xlog);
  logfile = fopen(xlog, "w");
  lives_free(xlog);
  return logfile;
}


LIVES_GLOBAL_INLINE void close_logfile(FILE *logfile) {fclose(logfile);}


static weed_plant_t *make_msg(const char *text) {
  // make single msg. text should have no newlines in it, except possibly as the last character.
  if (!text) return NULL;
  else {
    weed_plant_t *msg = lives_plant_new(LIVES_WEED_SUBTYPE_MESSAGE);
    if (!msg) return NULL;

    weed_set_string_value(msg, WEED_LEAF_LIVES_MESSAGE_STRING, text);
    weed_set_plantptr_value(msg, WEED_LEAF_NEXT, NULL);
    weed_set_plantptr_value(msg, WEED_LEAF_PREVIOUS, NULL);
    return msg;
  }
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

    if (mainw->debug) log_msg(mainw->debug_log, text);

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

  //if (mainw->debug) log_msg(mainw->debug_log, text);

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
          || (mainw->multitrack && mainw->multitrack->msg_area
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

