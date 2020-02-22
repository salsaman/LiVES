// dialogs.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2018 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "support.h"
#include "cvirtual.h"
#include "audio.h" // for fill_abuffer_from
#include "resample.h"
#include "rte_window.h"
#include "paramwindow.h"
#include "ce_thumbs.h"
#include "callbacks.h"

extern void reset_frame_and_clip_index(void);

// processing
static uint64_t event_start;
static double audio_start;
static boolean accelerators_swapped;
static int frames_done;
static double disp_fraction_done;
static ticks_t proc_start_ticks;
static ticks_t last_open_check_ticks;
static ticks_t spare_cycles;

static ticks_t last_kbd_ticks;

static boolean shown_paused_frames;
static boolean td_had_focus;

// how often to we count frames when opening
#define OPEN_CHECK_TICKS (TICKS_PER_SECOND/10l)

static volatile boolean display_ready;

static int64_t sttime;

static xprocess *procw = NULL;

static lives_time_source_t last_time_source;

void on_warn_mask_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  LiVESWidget *tbutton;

  if (lives_toggle_button_get_active(togglebutton)) prefs->warning_mask |= LIVES_POINTER_TO_INT(user_data);
  else prefs->warning_mask ^= LIVES_POINTER_TO_INT(user_data);
  set_int_pref(PREF_LIVES_WARNING_MASK, prefs->warning_mask);

  if ((tbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(togglebutton), "auto")) != NULL) {
    // this is for the cds window - disable autoreload if we are not gonna show this window
    if (lives_toggle_button_get_active(togglebutton)) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(tbutton), FALSE);
      lives_widget_set_sensitive(tbutton, FALSE);
    } else {
      lives_widget_set_sensitive(tbutton, TRUE);
    }
  }
}


static void add_xlays_widget(LiVESBox *box) {
  char *tmp = lives_strdup(_("Show affected _layouts"));
  add_list_expander(box, tmp, ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V, mainw->xlays);
  lives_free(tmp);
}


void add_warn_check(LiVESBox *box, int warn_mask_number) {
  LiVESWidget *checkbutton;

  checkbutton = lives_standard_check_button_new(
                  _("Do _not show this warning any more\n(can be turned back on from Preferences/Warnings)"),
                  FALSE, LIVES_BOX(box), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_warn_mask_toggled),
                       LIVES_INT_TO_POINTER(warn_mask_number));
}


static void add_clear_ds_button(LiVESDialog *dialog) {
  LiVESWidget *button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CLEAR, _("_Recover disk space"),
                        LIVES_RESPONSE_RETRY);
  if (mainw->tried_ds_recover) lives_widget_set_sensitive(button, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cleardisk_activate),
                       (livespointer)button);
}


static void add_clear_ds_adv(LiVESBox *box) {
  // add a button which opens up  Recover/Repair widget
  LiVESWidget *button = lives_standard_button_new_with_label(_(" _Advanced Settings >>"));
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(hbox), button, FALSE, FALSE, widget_opts.packing_width * 2);
  lives_box_pack_start(box, hbox, FALSE, FALSE, widget_opts.packing_height);

  lives_signal_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cleardisk_advanced_clicked),
                       NULL);
}


//Warning or yes/no dialog

// the type of message box here is with 2 or more buttons (e.g. OK/CANCEL, YES/NO, ABORT/CANCEL/RETRY)
// if a single OK button is needed, use create_info_error_dialog() in inteface.c instead


LiVESWidget *create_message_dialog(lives_dialog_t diat, const char *text, LiVESWindow *transient,
                                   int warn_mask_number, boolean is_blocking) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *label;
  LiVESWidget *cancelbutton = NULL;
  LiVESWidget *okbutton = NULL;
  LiVESWidget *abortbutton = NULL;

  LiVESAccelGroup *accel_group = NULL;

  char *textx, *form_text, *pad, *mytext;

  mytext = lives_strdup(text); // because of translation issues

  switch (diat) {
  case LIVES_DIALOG_WARN:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0,
                                      LIVES_MESSAGE_WARNING, LIVES_BUTTONS_NONE, NULL);
    lives_window_set_title(LIVES_WINDOW(dialog), _("Warning !"));
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
               LIVES_RESPONSE_OK);
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         NULL);

    break;
  case LIVES_DIALOG_ERROR:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0,
                                      LIVES_MESSAGE_ERROR, LIVES_BUTTONS_NONE, NULL);
    lives_window_set_title(LIVES_WINDOW(dialog), _("Error !"));
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
               LIVES_RESPONSE_OK);
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         NULL);
    break;
  case LIVES_DIALOG_INFO:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0,
                                      LIVES_MESSAGE_INFO, LIVES_BUTTONS_NONE, NULL);
    lives_window_set_title(LIVES_WINDOW(dialog), _("Information"));
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
               LIVES_RESPONSE_OK);
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         NULL);
    break;

  case LIVES_DIALOG_WARN_WITH_CANCEL:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_WARNING, LIVES_BUTTONS_NONE, NULL);

    if (mainw != NULL && mainw->add_clear_ds_button) {
      mainw->add_clear_ds_button = FALSE;
      add_clear_ds_button(LIVES_DIALOG(dialog));
    }

    lives_window_set_title(LIVES_WINDOW(dialog), _("Warning !"));
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                   LIVES_RESPONSE_CANCEL);
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
               LIVES_RESPONSE_OK);
    break;

  case LIVES_DIALOG_YESNO:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_QUESTION, LIVES_BUTTONS_NONE, NULL);
    lives_window_set_title(LIVES_WINDOW(dialog), _("Question"));
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_NO, NULL,
                   LIVES_RESPONSE_NO);
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_YES, NULL,
               LIVES_RESPONSE_YES);
    break;

  case LIVES_DIALOG_QUESTION:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_QUESTION, LIVES_BUTTONS_NONE, NULL);
    // caller will set title and buttons
    break;

  case LIVES_DIALOG_ABORT_CANCEL_RETRY:
  case LIVES_DIALOG_ABORT_RETRY:
  case LIVES_DIALOG_ABORT_OK:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_ERROR, LIVES_BUTTONS_NONE, NULL);
    abortbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_QUIT, _("_Abort"),
                  LIVES_RESPONSE_ABORT);
    if (diat == LIVES_DIALOG_ABORT_CANCEL_RETRY) {
      lives_window_set_title(LIVES_WINDOW(dialog), _("File Error"));
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                     LIVES_RESPONSE_CANCEL);
    }
    if (diat == LIVES_DIALOG_ABORT_OK) {
      okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                 LIVES_RESPONSE_OK);
    } else {
      okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH, _("_Retry"),
                 LIVES_RESPONSE_RETRY);
    }
    break;

  case LIVES_DIALOG_CANCEL_RETRY_BROWSE:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_ERROR, LIVES_BUTTONS_NONE, NULL);
    lives_window_set_title(LIVES_WINDOW(dialog), _("Missing file"));
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                   LIVES_RESPONSE_CANCEL);
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH, _("_Retry"),
               LIVES_RESPONSE_RETRY);
    abortbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_QUIT, _("_Browse"),
                  LIVES_RESPONSE_BROWSE);
    break;

  default:
    cancelbutton = abortbutton; // stop compiler complaining
    return NULL;
    break;
  }

  lives_window_set_default_size(LIVES_WINDOW(dialog), MIN_MSGBOX_WIDTH, -1);
  lives_widget_set_minimum_size(dialog, MIN_MSGBOX_WIDTH, -1);

  lives_window_set_deletable(LIVES_WINDOW(dialog), FALSE);
  lives_window_set_resizable(LIVES_WINDOW(dialog), FALSE);

  if (mainw != NULL) {
    if (widget_opts.screen != NULL) lives_window_set_screen(LIVES_WINDOW(dialog), widget_opts.screen);

    if (widget_opts.apply_theme && (palette->style & STYLE_1)) {
      lives_dialog_set_has_separator(LIVES_DIALOG(dialog), FALSE);
      lives_widget_set_bg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }
    if (widget_opts.apply_theme) {
      funkify_dialog(dialog);
    } else {
      lives_container_set_border_width(LIVES_CONTAINER(dialog), widget_opts.border_width * 2);
    }
  }

  textx = insert_newlines(mytext, MAX_MSG_WIDTH_CHARS);
  lives_free(mytext);

  pad = lives_strdup("");
  if (lives_strlen(textx) < MIN_MSG_WIDTH_CHARS) {
    lives_free(pad);
    pad = lives_strndup("                                  ", (MIN_MSG_WIDTH_CHARS - lives_strlen(textx)) / 2);
  }
  form_text = lives_strdup_printf("\n%s%s%s\n", pad, textx, pad);

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  widget_opts.mnemonic_label = FALSE;
  label = lives_standard_label_new(form_text);
  widget_opts.mnemonic_label = TRUE;
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_free(form_text);
  lives_free(textx);
  lives_free(pad);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  //lives_container_set_border_width(LIVES_CONTAINER(dialog_vbox), widget_opts.border_width * 2);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);
  lives_label_set_selectable(LIVES_LABEL(label), TRUE);

  if (mainw != NULL) {
    if (mainw != NULL && mainw->add_clear_ds_adv) {
      mainw->add_clear_ds_adv = FALSE;
      add_clear_ds_adv(LIVES_BOX(dialog_vbox));
    }

    if (warn_mask_number > 0) {
      add_warn_check(LIVES_BOX(dialog_vbox), warn_mask_number);
    }

    if (mainw->xlays != NULL) {
      add_xlays_widget(LIVES_BOX(dialog_vbox));
    }

    if (mainw->iochan != NULL && is_blocking) {
      LiVESWidget *details_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), NULL, _("Show _Details"),
                                    LIVES_RESPONSE_SHOW_DETAILS);

      lives_signal_connect(LIVES_GUI_OBJECT(details_button), LIVES_WIDGET_CLICKED_SIGNAL,
                           LIVES_GUI_CALLBACK(lives_general_button_clicked),
                           NULL);
    }
  }

  if (okbutton != NULL || cancelbutton != NULL) {
    accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
    lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);
  }

  if (cancelbutton != NULL) {
    lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                                 LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);
  }


  if (okbutton != NULL && mainw != NULL && mainw->iochan == NULL) {
    lives_button_grab_default_special(okbutton);
    lives_widget_grab_focus(okbutton);
  }

  lives_window_center(LIVES_WINDOW(dialog));
  lives_widget_show_all(dialog);
  lives_widget_context_update();
  lives_window_center(LIVES_WINDOW(dialog));

  if (is_blocking)
    lives_window_set_modal(LIVES_WINDOW(dialog), TRUE);

  if (prefs != NULL && prefs->present) {
    lives_window_present(LIVES_WINDOW(dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(dialog));
  }

  if (transient == NULL) lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);

  return dialog;
}


LIVES_GLOBAL_INLINE LiVESWidget *create_question_dialog(const char *title, const char *text, LiVESWindow *parent) {
  LiVESWidget *dialog = create_message_dialog(LIVES_DIALOG_QUESTION, text, parent, 0, TRUE);
  lives_window_set_title(LIVES_WINDOW(dialog), title);
  return dialog;
}


boolean do_warning_dialogf(const char *fmt, ...) {
  va_list xargs;
  boolean resb;
  char *textx;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  resb = do_warning_dialog_with_check(textx, 0);
  lives_free(textx);
  return resb;
}


boolean do_warning_dialog(const char *text) {
  return do_warning_dialog_with_check(text, 0);
}


boolean do_warning_dialog_with_check(const char *text, uint64_t warn_mask_number) {
  if (!prefs->show_gui) {
    return do_warning_dialog_with_check_transient(text, warn_mask_number, NULL);
  } else {
    return do_warning_dialog_with_check_transient(text, warn_mask_number, LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }
}


boolean do_yesno_dialog_with_check(const char *text, uint64_t warn_mask_number) {
  return do_yesno_dialog_with_check_transient(text, warn_mask_number, NULL);
}


boolean do_warning_dialog_with_check_transient(const char *text, uint64_t warn_mask_number, LiVESWindow *transient) {
  // show OK/CANCEL, returns FALSE if cancelled
  LiVESWidget *warning;
  int response = 1;
  char *mytext;

  if (prefs->warning_mask & warn_mask_number) {
    return TRUE;
  }

  if (transient == NULL && prefs->show_gui) transient = get_transient_full();

  mytext = lives_strdup(text); // must copy this because of translation issues

  do {
    warning = create_message_dialog(LIVES_DIALOG_WARN_WITH_CANCEL, mytext, (transient != NULL &&
                                    lives_widget_is_visible(LIVES_WIDGET(transient))) ? transient : NULL,
                                    warn_mask_number, TRUE);
    response = lives_dialog_run(LIVES_DIALOG(warning));
    lives_widget_destroy(warning);
  } while (response == LIVES_RESPONSE_RETRY);

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  lives_freep((void **)&mytext);

  return (response == LIVES_RESPONSE_OK);
}


boolean do_yesno_dialog_with_check_transient(const char *text, uint64_t warn_mask_number, LiVESWindow *transient) {
  // show YES/NO, returns TRUE for YES
  LiVESWidget *warning;
  int response = 1;
  char *mytext;

  if (prefs->warning_mask & warn_mask_number) {
    return TRUE;
  }

  mytext = lives_strdup(text); // must copy this because of translation issues

  do {
    warning = create_message_dialog(LIVES_DIALOG_YESNO, mytext, transient, warn_mask_number, TRUE);
    response = lives_dialog_run(LIVES_DIALOG(warning));
    lives_widget_destroy(warning);
  } while (response == LIVES_RESPONSE_RETRY);

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  lives_freep((void **)&mytext);

  return (response == LIVES_RESPONSE_YES);
}


LiVESWindow *get_transient_full(void) {
  LiVESWindow *transient = NULL;
  if (prefs == NULL) return NULL;
  if (prefs->show_gui) {
    if (prefsw != NULL && prefsw->prefs_dialog != NULL) transient = LIVES_WINDOW(prefsw->prefs_dialog);
    else if (!rte_window_hidden()) transient = LIVES_WINDOW(rte_window);
    else if (LIVES_MAIN_WINDOW_WIDGET != NULL && mainw->is_ready) transient = LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET);
  }
  return transient;
}


boolean do_yesno_dialogf(const char *fmt, ...) {
  // show Yes/No, returns TRUE if Yes
  LiVESWidget *warning;
  LiVESWindow *transient = get_transient_full();
  int response;
  va_list xargs;
  char *textx;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  warning = create_message_dialog(LIVES_DIALOG_YESNO, textx, transient, 0, TRUE);
  lives_free(textx);
  response = lives_dialog_run(LIVES_DIALOG(warning));
  lives_widget_destroy(warning);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  return (response == LIVES_RESPONSE_YES);
}


boolean do_yesno_dialog(const char *text) {
  // show Yes/No, returns TRUE if Yes
  LiVESWidget *warning;
  LiVESWindow *transient = get_transient_full();
  int response;

  warning = create_message_dialog(LIVES_DIALOG_YESNO, text, transient, 0, TRUE);
  response = lives_dialog_run(LIVES_DIALOG(warning));
  lives_widget_destroy(warning);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  return (response == LIVES_RESPONSE_YES);
}


static LiVESResponseType _do_abort_cancel_retry_dialog(const char *text, LiVESWindow *transient, lives_dialog_t dtype) {
  LiVESResponseType response;
  char *mytext;
  LiVESWidget *warning;

  mytext = lives_strdup(text); // translation issues

  if (transient == NULL) transient = get_transient_full();

  do {
    warning = create_message_dialog(dtype, mytext, transient, 0, TRUE);
    lives_widget_show_all(warning);
    response = lives_dialog_run(LIVES_DIALOG(warning)); // looping on retry
    lives_widget_destroy(warning);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

    if (response == LIVES_RESPONSE_ABORT) {
      if (mainw->is_ready) {
        if (do_abort_check()) {
          if (CURRENT_CLIP_IS_VALID) {
            if (cfile->handle != NULL) {
              // stop any processing
              lives_kill_subprocesses(cfile->handle, TRUE);
            }
          }
          LIVES_FATAL("Aborted");
          lives_notify(LIVES_OSC_NOTIFY_QUIT, mytext);
          exit(1);
        }
      } else {
        LIVES_FATAL("Aborted");
        lives_notify(LIVES_OSC_NOTIFY_QUIT, mytext);
        exit(0);
      }
    }
  } while (response == LIVES_RESPONSE_ABORT);
  return response;
}


// returns LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY
LIVES_GLOBAL_INLINE LiVESResponseType do_abort_cancel_retry_dialog(const char *text, LiVESWindow *transient) {
  return _do_abort_cancel_retry_dialog(text, transient, LIVES_DIALOG_ABORT_CANCEL_RETRY);
}


// always returns LIVES_RESPONSE_RETRY
LIVES_GLOBAL_INLINE LiVESResponseType do_abort_retry_dialog(const char *text, LiVESWindow *transient) {
  return _do_abort_cancel_retry_dialog(text, transient, LIVES_DIALOG_ABORT_RETRY);
}


// always returns LIVES_RESPONSE_OK
LIVES_GLOBAL_INLINE LiVESResponseType do_abort_ok_dialog(const char *text, LiVESWindow *transient) {
  return _do_abort_cancel_retry_dialog(text, transient, LIVES_DIALOG_ABORT_OK);
}


LiVESResponseType do_error_dialogf(const char *fmt, ...) {
  // show error box
  LiVESWindow *transient = get_transient_full();
  LiVESResponseType resi;
  char *textx;
  va_list xargs;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  resi = do_error_dialog_with_check_transient(textx, FALSE, 0, transient);
  lives_free(textx);
  return resi;
}


LIVES_GLOBAL_INLINE LiVESResponseType do_error_dialog(const char *text) {
  // show error box
  LiVESWindow *transient = get_transient_full();
  return do_error_dialog_with_check_transient(text, FALSE, 0, transient);
}


static LiVESResponseType do_info_dialog_with_transient(const char *text, boolean is_blocking, LiVESWindow *transient,
    const char *exp_title, LiVESList *exp_list) {
  LiVESResponseType ret = LIVES_RESPONSE_NONE;
  LiVESWidget *info_box = create_info_error_dialog(LIVES_DIALOG_INFO, text, transient, 0, is_blocking);

  if (exp_list != NULL) {
    LiVESWidget *dab = lives_dialog_get_content_area(LIVES_DIALOG(info_box));
    add_list_expander(LIVES_BOX(dab), exp_title, ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V, exp_list);
    lives_widget_show_all(info_box);
  }

  if (is_blocking) {
    ret = lives_dialog_run(LIVES_DIALOG(info_box));
    if (mainw != NULL && mainw->is_ready && transient != NULL) {
      lives_widget_queue_draw(LIVES_WIDGET(transient));
    }
  }

  return ret;
}


LiVESResponseType do_info_dialogf(const char *fmt, ...) {
  // show info box
  LiVESWindow *transient = get_transient_full();
  LiVESResponseType resi;
  char *textx;
  va_list xargs;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  resi = do_info_dialog_with_transient(textx, FALSE, transient, NULL, NULL);
  lives_free(textx);
  return resi;
}


LIVES_GLOBAL_INLINE LiVESResponseType do_info_dialog(const char *text) {
  // show info box
  LiVESWindow *transient = get_transient_full();
  return do_info_dialog_with_transient(text, FALSE, transient, NULL, NULL);
}


LiVESResponseType do_error_dialog_with_check(const char *text, uint64_t warn_mask_number) {
  // show warning box
  LiVESWindow *transient = get_transient_full();
  return do_error_dialog_with_check_transient(text, FALSE, warn_mask_number, transient);
}


LiVESResponseType do_blocking_error_dialogf(const char *fmt, ...) {
  // show error box - blocks until OK is pressed
  LiVESWindow *transient = get_transient_full();
  va_list xargs;
  char *textx;
  LiVESResponseType resi;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  resi = do_error_dialog_with_check_transient(textx, TRUE, 0, transient);
  lives_free(textx);
  return resi;
}


LiVESResponseType do_blocking_info_dialogf(const char *fmt, ...) {
  // show info box - blocks until OK is pressed
  LiVESWindow *transient = get_transient_full();
  va_list xargs;
  char *textx;
  LiVESResponseType resi;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  resi = do_info_dialog_with_transient(textx, TRUE, transient, NULL, NULL);
  lives_free(textx);
  return resi;
}


LiVESResponseType do_blocking_error_dialog(const char *text) {
  // show error box - blocks until OK is pressed
  LiVESWindow *transient = get_transient_full();
  return do_error_dialog_with_check_transient(text, TRUE, 0, transient);
}


static LiVESResponseType _do_blocking_info_dialog(const char *text,  const char *exp_text, LiVESList *list) {
  // show info box - blocks until OK is pressed
  LiVESWindow *transient = get_transient_full();
  return do_info_dialog_with_transient(text, TRUE, transient, exp_text, list);
}


LiVESResponseType do_blocking_info_dialog(const char *text) {
  // show info box - blocks until OK is pressed
  return _do_blocking_info_dialog(text, NULL, NULL);
}


LiVESResponseType do_blocking_info_dialog_with_expander(const char *text, const char *exp_text, LiVESList *list) {
  // show info box - blocks until OK is pressed
  return _do_blocking_info_dialog(text, exp_text, list);
}


LiVESResponseType do_error_dialog_with_check_transient(const char *text, boolean is_blocking, uint64_t warn_mask_number,
    LiVESWindow *transient) {
  // show error box

  LiVESWidget *err_box;

  LiVESResponseType ret = LIVES_RESPONSE_NONE;

  if (prefs != NULL && (prefs->warning_mask & warn_mask_number)) return ret;
  err_box = create_info_error_dialog(warn_mask_number == 0 ? LIVES_DIALOG_ERROR :
                                     LIVES_DIALOG_WARN, text, transient, warn_mask_number,
                                     is_blocking);

  if (is_blocking) {
    ret = lives_dialog_run(LIVES_DIALOG(err_box));
    if (mainw != NULL && mainw->is_ready && transient != NULL) {
      lives_widget_queue_draw(LIVES_WIDGET(transient));
    }
  }

  return ret;
}


char *ds_critical_msg(const char *dir, uint64_t dsval) {
  char *msg;
  char *msgx;
  char *tmp;
  char *dscr = lives_format_storage_space_string(prefs->ds_crit_level); ///< crit level
  char *dscu = lives_format_storage_space_string(dsval); ///< current level
  msg = lives_strdup_printf(
          _("FREE SPACE IN THE PARTITION CONTAINING\n%s\nHAS FALLEN BELOW THE CRITICAL LEVEL OF %s\n"
            "CURRENT FREE SPACE IS %s\n\n(Disk warning levels can be configured in Preferences.)"),
          (tmp = lives_filename_to_utf8(dir, -1, NULL, NULL, NULL)), dscr, dscu);
  msgx = insert_newlines(msg, MAX_MSG_WIDTH_CHARS);
  lives_free(msg);
  lives_free(tmp);
  lives_free(dscr);
  lives_free(dscu);
  return msgx;
}


char *ds_warning_msg(const char *dir, uint64_t dsval, uint64_t cwarn, uint64_t nwarn) {
  char *msg;
  char *msgx;
  char *tmp;
  char *dscw = lives_format_storage_space_string(cwarn); ///< warn level
  char *dscu = lives_format_storage_space_string(dsval); ///< current level
  char *dscn = lives_format_storage_space_string(nwarn); ///< next warn level
  msg = lives_strdup_printf(
          _("Free space in the partition containing\n%s\nhas fallen below the warning level of %s\nCurrent free space is %s\n\n"
            "(Next warning will be shown at %s. Disk warning levels can be configured in Preferences.)"),
          (tmp = lives_filename_to_utf8(dir, -1, NULL, NULL, NULL)), dscw, dscu, dscn);
  msgx = insert_newlines(msg, MAX_MSG_WIDTH_CHARS);
  lives_free(msg);
  lives_free(dscw);
  lives_free(dscu);
  lives_free(dscn);
  return msgx;
}


LIVES_GLOBAL_INLINE void do_abortblank_error(const char *what) {
  char *msg = lives_strdup_printf(_("%s may not be blank.\nClick Abort to exit LiVES immediately or Ok "
                                    "to continue with the default value."), what);
  do_abort_ok_dialog(msg, NULL);
  lives_free(msg);
}


LIVES_GLOBAL_INLINE void do_optarg_blank_err(const char *what) {
  char *msg = lives_strdup_printf(_("-%s requires an argument, ignoring it\n"), what);
  LIVES_WARN(msg);
  lives_free(msg);
}


LIVES_GLOBAL_INLINE void do_clip_divergence_error(int fileno) {
  char *msg = lives_strdup_printf(_("Errors were encountered when reloading LiVES' copy of the clip %s\n"
                                    "Please click Abort if you wish to exit from LiVES,\n"
                                    "or OK to update the clip details in LiVES and continue anyway.\n"),
                                  IS_VALID_CLIP(fileno) ? mainw->files[fileno]->name : "??????");
  do_abort_ok_dialog(msg, NULL);
  lives_free(msg);
  check_storage_space(fileno, FALSE);
}


void do_aud_during_play_error(void) {
  do_blocking_error_dialog(_("Audio players cannot be switched during playback."));
}


LiVESResponseType do_memory_error_dialog(char *op, size_t bytes) {
  LiVESResponseType response;
  char *sizestr, *msg;
  if (bytes > 0) {
    sizestr = lives_strdup_printf(_(" with size %ld bytes "), bytes);
  } else {
    sizestr = lives_strdup("");
  }
  msg = lives_strdup_printf(_("\n\nLiVES encountered a memory error when %s%s.\n"
                              "Click Abort to exit from LiVES, Cancel to abandon the operation\n"
                              "or Retry to try again. You may need to close some other applications first.\n"), op, sizestr);
  lives_free(sizestr);
  response = do_abort_cancel_retry_dialog(msg, NULL);
  lives_free(msg);
  return response;
}


LiVESResponseType handle_backend_errors(boolean can_retry, LiVESWindow *transient) {
  /// handle error conditions returned from the back end

  char **array;
  char *addinfo;
  LiVESResponseType response = LIVES_RESPONSE_NONE;
  int pxstart = 1;
  int numtok;
  int i;

  if (mainw->cancelled != CANCEL_NONE) return LIVES_RESPONSE_ACCEPT; // if the user/system cancelled we can expect errors !

  numtok = get_token_count(mainw->msg, '|');

  array = lives_strsplit(mainw->msg, "|", numtok);

  if (numtok > 2 && !strcmp(array[1], "read")) {
    /// got read error from backend
    if (numtok > 3 && *(array[3])) addinfo = array[3];
    else addinfo = NULL;
    if (mainw->current_file == -1 || cfile == NULL || !cfile->no_proc_read_errors)
      do_read_failed_error_s(array[2], addinfo);
    pxstart = 3;
    mainw->read_failed = TRUE;
    mainw->read_failed_file = lives_strdup(array[2]);
    mainw->cancelled = CANCEL_ERROR;
    response = LIVES_RESPONSE_CANCEL;
  }

  else if (numtok > 2 && !strcmp(array[1], "write")) {
    /// got write error from backend
    if (numtok > 3 && *(array[3])) addinfo = array[3];
    else addinfo = NULL;
    if (mainw->current_file == -1 || cfile == NULL || !cfile->no_proc_write_errors)
      do_write_failed_error_s(array[2], addinfo);
    pxstart = 3;
    mainw->write_failed = TRUE;
    mainw->write_failed_file = lives_strdup(array[2]);
    mainw->cancelled = CANCEL_ERROR;
    response = LIVES_RESPONSE_CANCEL;
  }

  else if (numtok > 3 && !strcmp(array[1], "system")) {
    /// got (sub) system error from backend
    if (numtok > 4 && *(array[4])) addinfo = array[4];
    else addinfo = NULL;
    if (!CURRENT_CLIP_IS_VALID || !cfile->no_proc_sys_errors) {
      response = do_system_failed_error(array[2], atoi(array[3]), addinfo, can_retry, transient);
      if (response == LIVES_RESPONSE_RETRY) return response;
    }
    pxstart = 3;
    mainw->cancelled = CANCEL_ERROR;
    response = LIVES_RESPONSE_CANCEL;
  }

  // for other types of errors...more info....
  /// set mainw->error but not mainw->cancelled
  lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "\n\n");
  for (i = pxstart; i < numtok; i++) {
    if (!*(array[i]) || !strcmp(array[i], "ERROR")) break;
    lives_strappend(mainw->msg, MAINW_MSG_SIZE, _(array[i]));
    lives_strappend(mainw->msg, MAINW_MSG_SIZE, "\n");
  }
  lives_strfreev(array);

  mainw->error = TRUE;
  return response;
}


boolean check_backend_return(lives_clip_t *sfile, LiVESWindow *transient) {
  // check return code after synchronous (foreground) backend commands

  FILE *infofile;

  (infofile = fopen(sfile->info_file, "r"));
  if (!infofile) return FALSE;

  mainw->read_failed = FALSE;
  lives_fread(mainw->msg, 1, MAINW_MSG_SIZE, infofile);
  fclose(infofile);

  // TODO: consider permitting retry here
  if (!strncmp(mainw->msg, "error", 5)) handle_backend_errors(FALSE, transient);

  return TRUE;
}


void pump_io_chan(LiVESIOChannel *iochan) {
  // pump data from stdout to textbuffer
  char *str_return = NULL;
  size_t retlen = 0;
  size_t plen;
  LiVESTextBuffer *optextbuf = lives_text_view_get_buffer(mainw->optextview);

#ifdef GUI_GTK
  LiVESError *gerr = NULL;

  if (!iochan->is_readable) return;
  g_io_channel_read_to_end(iochan, &str_return, &retlen, &gerr);
  if (gerr != NULL) lives_error_free(gerr);
#endif

#ifdef GUI_QT
  QByteArray qba = iochan->readAll();
  str_return = strdup(qba.constData());
  retlen = strlen(str_return);
#endif
  // check each line of str_return, if it contains ptext, (whitespace), then number get the number and set percentage

  if (retlen > 0 && cfile->proc_ptr != NULL) {
    double max;
    LiVESAdjustment *adj = lives_scrolled_window_get_vadjustment(LIVES_SCROLLED_WINDOW(((xprocess *)(
                             cfile->proc_ptr))->scrolledwindow));
    lives_text_buffer_insert_at_end(optextbuf, str_return);
    max = lives_adjustment_get_upper(adj);
    lives_adjustment_set_value(adj, max);

    if ((plen = strlen(prefs->encoder.ptext)) > 0) {
      boolean linebrk = FALSE;
      char *cptr = str_return;
      int ispct = 0;

      if (prefs->encoder.ptext[0] == '%') {
        ispct = 1;
        plen--;
      }

      while (cptr < (str_return + retlen - plen)) {
        if (!lives_strncmp(cptr, prefs->encoder.ptext + ispct, plen)) {
          cptr += plen;
          while (*cptr == ' ' || *cptr == '\n' || *cptr == '=') {
            if (*cptr == '\n') {
              linebrk = TRUE;
              break;
            }
            cptr++;
          }
          if (!linebrk) {
            if (ispct) cfile->proc_ptr->frames_done = (int)(strtod(cptr, NULL) * (cfile->progress_end - cfile->progress_start + 1.) / 100.);
            else cfile->proc_ptr->frames_done = atoi(cptr);
          }
          break;
        }
        cptr++;
      }
    }
  }

  lives_freep((void **)&str_return);
}


boolean check_storage_space(int clipno, boolean is_processing) {
  // check storage space in prefs->workdir, and if sfile!=NULL, in sfile->op_dir
  uint64_t dsval;
  lives_clip_t *sfile = NULL;
  int retval;
  boolean did_pause = FALSE;

  lives_storage_status_t ds;

  char *msg, *tmp;
  char *pausstr = lives_strdup(_("Processing has been paused."));

  if (IS_VALID_CLIP(clipno)) sfile = mainw->files[clipno];

  do {
    ds = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsval);
    if (ds == LIVES_STORAGE_STATUS_WARNING) {
      uint64_t curr_ds_warn = mainw->next_ds_warn_level;
      mainw->next_ds_warn_level >>= 1;
      if (mainw->next_ds_warn_level > (dsval >> 1)) mainw->next_ds_warn_level = dsval >> 1;
      if (mainw->next_ds_warn_level < prefs->ds_crit_level) mainw->next_ds_warn_level = prefs->ds_crit_level;
      if (is_processing && sfile != NULL && sfile->proc_ptr != NULL && !mainw->effects_paused &&
          lives_widget_is_visible(sfile->proc_ptr->pause_button)) {
        on_effects_paused(LIVES_BUTTON(sfile->proc_ptr->pause_button), NULL);
        did_pause = TRUE;
      }

      tmp = ds_warning_msg(prefs->workdir, dsval, curr_ds_warn, mainw->next_ds_warn_level);
      if (!did_pause)
        msg = lives_strdup_printf("\n%s\n", tmp);
      else
        msg = lives_strdup_printf("\n%s\n%s\n", tmp, pausstr);
      lives_free(tmp);
      mainw->add_clear_ds_button = TRUE; // gets reset by do_warning_dialog()
      if (!do_warning_dialog(msg)) {
        lives_free(msg);
        lives_free(pausstr);
        mainw->cancelled = CANCEL_USER;
        if (is_processing) {
          if (sfile != NULL) sfile->nokeep = TRUE;
          on_cancel_keep_button_clicked(NULL, NULL); // press the cancel button
        }
        return FALSE;
      }
      lives_free(msg);
    } else if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
      if (is_processing && sfile != NULL && sfile->proc_ptr != NULL && !mainw->effects_paused &&
          lives_widget_is_visible(sfile->proc_ptr->pause_button)) {
        on_effects_paused(LIVES_BUTTON(sfile->proc_ptr->pause_button), NULL);
        did_pause = TRUE;
      }
      tmp = ds_critical_msg(prefs->workdir, dsval);
      if (!did_pause)
        msg = lives_strdup_printf("\n%s\n", tmp);
      else
        msg = lives_strdup_printf("\n%s\n%s\n", tmp, pausstr);
      lives_free(tmp);
      retval = do_abort_cancel_retry_dialog(msg, NULL);
      lives_free(msg);
      if (retval == LIVES_RESPONSE_CANCEL) {
        if (is_processing) {
          if (sfile != NULL) sfile->nokeep = TRUE;
          on_cancel_keep_button_clicked(NULL, NULL); // press the cancel button
        }
        mainw->cancelled = CANCEL_ERROR;
        lives_free(pausstr);
        return FALSE;
      }
    }
  } while (ds == LIVES_STORAGE_STATUS_CRITICAL);

  if (sfile != NULL && sfile->op_dir != NULL && strcmp(sfile->op_dir, prefs->workdir)) {
    do {
      ds = get_storage_status(sfile->op_dir, sfile->op_ds_warn_level, &dsval);
      if (ds == LIVES_STORAGE_STATUS_WARNING) {
        uint64_t curr_ds_warn = sfile->op_ds_warn_level;
        sfile->op_ds_warn_level >>= 1;
        if (sfile->op_ds_warn_level > (dsval >> 1)) sfile->op_ds_warn_level = dsval >> 1;
        if (sfile->op_ds_warn_level < prefs->ds_crit_level) sfile->op_ds_warn_level = prefs->ds_crit_level;
        if (is_processing && sfile != NULL && sfile->proc_ptr != NULL && !mainw->effects_paused &&
            lives_widget_is_visible(sfile->proc_ptr->pause_button)) {
          on_effects_paused(LIVES_BUTTON(sfile->proc_ptr->pause_button), NULL);
          did_pause = TRUE;
        }
        tmp = ds_warning_msg(sfile->op_dir, dsval, curr_ds_warn, sfile->op_ds_warn_level);
        if (!did_pause)
          msg = lives_strdup_printf("\n%s\n", tmp);
        else
          msg = lives_strdup_printf("\n%s\n%s\n", tmp, pausstr);
        lives_free(tmp);
        mainw->add_clear_ds_button = TRUE; // gets reset by do_warning_dialog()
        if (!do_warning_dialog(msg)) {
          lives_free(msg);
          lives_free(pausstr);
          lives_freep((void **)&sfile->op_dir);
          if (is_processing) {
            sfile->nokeep = TRUE;
            on_cancel_keep_button_clicked(NULL, NULL); // press the cancel button
          }
          mainw->cancelled = CANCEL_USER;
          return FALSE;
        }
        lives_free(msg);
      } else if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
        if (is_processing && sfile != NULL && sfile->proc_ptr != NULL && !mainw->effects_paused &&
            lives_widget_is_visible(sfile->proc_ptr->pause_button)) {
          on_effects_paused(LIVES_BUTTON(sfile->proc_ptr->pause_button), NULL);
          did_pause = TRUE;
        }
        tmp = ds_critical_msg(sfile->op_dir, dsval);
        if (!did_pause)
          msg = lives_strdup_printf("\n%s\n", tmp);
        else
          msg = lives_strdup_printf("\n%s\n%s\n", tmp, pausstr);
        lives_free(tmp);
        retval = do_abort_cancel_retry_dialog(msg, NULL);
        lives_free(msg);
        if (retval == LIVES_RESPONSE_CANCEL) {
          if (is_processing) {
            sfile->nokeep = TRUE;
            on_cancel_keep_button_clicked(NULL, NULL); // press the cancel button
          }
          mainw->cancelled = CANCEL_ERROR;
          if (sfile != NULL) lives_free(sfile->op_dir);
          lives_free(pausstr);
          return FALSE;
        }
      }
    } while (ds == LIVES_STORAGE_STATUS_CRITICAL);
  }

  if (did_pause && mainw->effects_paused) {
    on_effects_paused(LIVES_BUTTON(sfile->proc_ptr->pause_button), NULL);
  }

  lives_free(pausstr);

  return TRUE;
}


static void cancel_process(boolean visible) {
  if (prefs->show_player_stats && !visible && mainw->fps_measure > 0.) {
    // statistics
    mainw->fps_measure /= (lives_get_relative_ticks(mainw->origsecs, mainw->origusecs) / TICKS_PER_SECOND_DBL);
  }
  if (visible) {
    if (mainw->preview_box != NULL && !mainw->preview) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Play all"));
    if (accelerators_swapped) {
      if (!mainw->preview) lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play all"));
      lives_widget_remove_accelerator(cfile->proc_ptr->preview_button, mainw->accel_group, LIVES_KEY_p, (LiVESXModifierType)0);
      lives_widget_add_accelerator(mainw->playall, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group, LIVES_KEY_p,
                                   (LiVESXModifierType)0,
                                   LIVES_ACCEL_VISIBLE);
    }
    if (cfile->proc_ptr != NULL) {
      lives_widget_destroy(LIVES_WIDGET(cfile->proc_ptr->processing));
      lives_free(cfile->proc_ptr);
      cfile->proc_ptr = NULL;
    }
    mainw->is_processing = FALSE;
    if (!(cfile->menuentry == NULL)) {
      sensitize();
    }
  } else {
    /// ????
    mainw->is_processing = TRUE;
  }
  sched_yield();
  if (visible && mainw->current_file > -1 && cfile->clip_type == CLIP_TYPE_DISK && ((mainw->cancelled != CANCEL_NO_MORE_PREVIEW
      && mainw->cancelled != CANCEL_PREVIEW_FINISHED && mainw->cancelled != CANCEL_USER) || !cfile->opening)) {
    lives_rm(cfile->info_file);
    sched_yield();
  }
}


static void disp_fraction(double fraction_done, double timesofar, xprocess *proc) {
  // display fraction done and estimated time remaining
  char *prog_label;
  double est_time;

  if (fraction_done > 1.) fraction_done = 1.;
  if (fraction_done < 0.) fraction_done = 0.;

  if (fraction_done > disp_fraction_done + .0001)
    lives_progress_bar_set_fraction(LIVES_PROGRESS_BAR(proc->progressbar), fraction_done);

  est_time = timesofar / fraction_done - timesofar;
  prog_label = lives_strdup_printf(_("\n%d%% done. Time remaining: %u sec\n"), (int)(fraction_done * 100.),
                                   (uint32_t)(est_time + .5));
  if (LIVES_IS_LABEL(proc->label3)) lives_label_set_text(LIVES_LABEL(proc->label3), prog_label);
  lives_free(prog_label);

  disp_fraction_done = fraction_done;
}


static int progress_count;
static double progress_speed;
static int prog_fs_check;

#define PROG_LOOP_VAL 200

static void progbar_pulse_or_fraction(lives_clip_t *sfile, int frames_done) {
  double timesofar, fraction_done;

  if ((progress_count++ * progress_speed) >= PROG_LOOP_VAL) {
    if (frames_done <= sfile->progress_end && sfile->progress_end > 0 && !mainw->effects_paused &&
        frames_done > 0) {
      timesofar = (lives_get_current_ticks() - proc_start_ticks - mainw->timeout_ticks) / TICKS_PER_SECOND_DBL;
      fraction_done = (double)(frames_done - sfile->progress_start) / (double)(sfile->progress_end - sfile->progress_start + 1.);
      disp_fraction(fraction_done, timesofar, sfile->proc_ptr);
      progress_count = 0;
      progress_speed = 4.;
    } else {
      lives_progress_bar_pulse(LIVES_PROGRESS_BAR(sfile->proc_ptr->progressbar));
      progress_count = 0;
      if (!mainw->is_rendering)  progress_speed = 2.;
    }
  }
}


void update_progress(boolean visible) {
  double fraction_done, timesofar;
  static double est_time = 0.;
  char *prog_label;

  if (cfile->opening && cfile->clip_type == CLIP_TYPE_DISK && !cfile->opening_only_audio &&
      (cfile->hsize > 0 || cfile->vsize > 0 || cfile->frames > 0) && (!mainw->effects_paused || !shown_paused_frames)) {
    uint32_t apxl;
    ticks_t currticks = lives_get_current_ticks();
    if ((currticks - last_open_check_ticks) > OPEN_CHECK_TICKS *
        ((apxl = get_approx_ln((uint32_t)cfile->opening_frames)) < 50 ? apxl : 50) ||
        (mainw->effects_paused && !shown_paused_frames)) {
      cfile->proc_ptr->frames_done = cfile->end = cfile->opening_frames = get_frame_count(mainw->current_file,
                                     cfile->opening_frames > 1 ? cfile->opening_frames : 1);
      last_open_check_ticks = currticks;
      if (cfile->opening_frames > 1) {
        if (cfile->frames > 0 && cfile->frames != 123456789) {
          fraction_done = (double)(cfile->opening_frames - 1.) / (double)cfile->frames;
          if (fraction_done > 1.) fraction_done = 1.;
          if (!mainw->effects_paused) {
            timesofar = (currticks - proc_start_ticks - mainw->timeout_ticks) / TICKS_PER_SECOND_DBL;
            est_time = timesofar / fraction_done - timesofar;
          }
          lives_progress_bar_set_fraction(LIVES_PROGRESS_BAR(cfile->proc_ptr->progressbar), fraction_done);
          if (est_time != -1.) prog_label = lives_strdup_printf(_("\n%d/%d frames opened. Time remaining %u sec.\n"),
                                              cfile->opening_frames - 1, cfile->frames, (uint32_t)(est_time + .5));
          else prog_label = lives_strdup_printf(_("\n%d/%d frames opened.\n"), cfile->opening_frames - 1, cfile->frames);
        } else {
          lives_progress_bar_pulse(LIVES_PROGRESS_BAR(cfile->proc_ptr->progressbar));
          prog_label = lives_strdup_printf(_("\n%d frames opened.\n"), cfile->opening_frames - 1);
        }
        lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label3), prog_label);
        lives_free(prog_label);
        cfile->start = cfile->end > 0 ? 1 : 0;
        showclipimgs();
      }
    }
    shown_paused_frames = mainw->effects_paused;
  } else {
    if (visible && cfile->proc_ptr->frames_done >= cfile->progress_start) {
      if (progress_count == 0) check_storage_space(mainw->current_file, TRUE);
      // display progress fraction or pulse bar
      progbar_pulse_or_fraction(cfile, cfile->proc_ptr->frames_done);
      sched_yield();
      lives_usleep(prefs->sleep_time);
    }
  }
}


int process_one(boolean visible) {
  ticks_t new_ticks;
  ticks_t real_ticks;

  lives_time_source_t time_source;
  boolean show_frame;

  lives_rfx_t *xrfx;

#ifdef RT_AUDIO
  double audio_stretch;
  ticks_t audio_ticks = 0;
#endif

  if (!visible) {
    // INTERNAL PLAYER

    if (LIVES_UNLIKELY(mainw->new_clip != -1)) {
      do_quick_switch(mainw->new_clip);
      mainw->force_show = TRUE;
      mainw->startticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->origusecs, NULL);
      mainw->deltaticks = 0;
      mainw->new_clip = -1;
    }

    /* if (prefs->loadchecktime > 0.) { */
    /*   load_measure_idle(NULL); */
    /* } */
    // get current time

    // time is obtained as follows:
    // -  if there is an external transport or clock active, we take our time from that
    // -  else if we have a fixed output framerate (e.g. we are streaming) we take our time from
    //         the system clock
    //  in these cases we adjust our audio rate slightly to keep in synch with video
    // - otherwise, we take the time from soundcard by counting samples played (the normal case),
    //   and we synch video with that; however, the soundcard time only updates when samples are played -
    //   so, between updates we interpolate with the system clock and then adjust when we get a new value
    //   from the card
    //g_print("process_one @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    time_source = LIVES_TIME_SOURCE_NONE;
    mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->origusecs, &time_source);
    if (mainw->currticks == -1) {
      if (time_source == LIVES_TIME_SOURCE_SOUNDCARD) handle_audio_timeout();
      mainw->cancelled = CANCEL_ERROR;
      return mainw->cancelled;
    }
    if (last_time_source != LIVES_TIME_SOURCE_NONE && time_source != last_time_source) {
      // time source changed, make sure there is no discontinuity by adjusting deltaticks
      ticks_t delta = lives_get_current_playback_ticks(mainw->origsecs, mainw->origusecs, &last_time_source);
      mainw->deltaticks += delta - mainw->currticks;
    }

    last_time_source = time_source;

#define ADJUST_AUDIO_RATE
#ifdef ADJUST_AUDIO_RATE
    // adjust audio rate slightly if we are behind or ahead
    // shouldn't need this since normally we sync video to soundcard
    // - unless we are controlled externally (e.g. jack transport) or system clock is forced
    if (time_source != LIVES_TIME_SOURCE_SOUNDCARD) {
#ifdef ENABLE_JACK
      if (prefs->audio_src == AUDIO_SRC_INT && prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL &&
          cfile->achans > 0 && (!mainw->is_rendering || (mainw->multitrack != NULL && !mainw->multitrack->is_rendering)) &&
          (mainw->currticks - mainw->offsetticks) > TICKS_PER_SECOND * 10 && ((audio_ticks = lives_jack_get_time(mainw->jackd)) >
              mainw->offsetticks || audio_ticks == -1)) {
        if (audio_ticks == -1) {
          if (mainw->cancelled == CANCEL_NONE) {
            if (cfile != NULL && !cfile->is_loaded) mainw->cancelled = CANCEL_NO_PROPOGATE;
            else mainw->cancelled = CANCEL_AUDIO_ERROR;
            return mainw->cancelled;
          }
        }
        if ((audio_stretch = (double)(audio_ticks - mainw->offsetticks) / (double)(mainw->currticks - mainw->offsetticks)) < 2. &&
            audio_stretch > 0.5) {
          // if audio_stretch is > 1. it means that audio is playing too fast
          // < 1. it is playing too slow

          // if too fast we increase the apparent sample rate so that it gets downsampled more
          // if too slow we decrease the apparent sample rate so that it gets upsampled more

          if (mainw->multitrack == NULL) {
            if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) {
              if (!cfile->play_paused) mainw->jackd->sample_in_rate = cfile->arate * cfile->pb_fps / cfile->fps * audio_stretch;
              else mainw->jackd->sample_in_rate = cfile->arate * cfile->freeze_fps / cfile->fps * audio_stretch;
            } else mainw->jackd->sample_in_rate = cfile->arate * audio_stretch;
          } else {
            if (mainw->jackd->read_abuf > -1) mainw->jackd->abufs[mainw->jackd->read_abuf]->arate = cfile->arate * audio_stretch;
          }
        }
      }
#endif

#ifdef HAVE_PULSE_AUDIO
      if (prefs->audio_src == AUDIO_SRC_INT && prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL &&
          cfile->achans > 0 && (!mainw->is_rendering || (mainw->multitrack != NULL && !mainw->multitrack->is_rendering)) &&
          (mainw->currticks - mainw->offsetticks) > TICKS_PER_SECOND * 10 &&
          ((audio_ticks = lives_pulse_get_time(mainw->pulsed)) >
           mainw->offsetticks || audio_ticks == -1)) {
        if (audio_ticks == -1) {
          if (mainw->cancelled == CANCEL_NONE) {
            if (cfile != NULL && !cfile->is_loaded) mainw->cancelled = CANCEL_NO_PROPOGATE;
            else mainw->cancelled = CANCEL_AUDIO_ERROR;
            return mainw->cancelled;
          }
        }
        // fps is synched to external source, so we adjust the audio rate to fit
        if ((audio_stretch = (double)(audio_ticks - mainw->offsetticks) / (double)(mainw->currticks - mainw->offsetticks)) < 2. &&
            audio_stretch > 0.5) {
          // if audio_stretch is > 1. it means that audio is playing too fast
          // < 1. it is playing too slow

          // if too fast we increase the apparent sample rate so that it gets downsampled more
          // if too slow we decrease the apparent sample rate so that it gets upsampled more

          if (mainw->multitrack == NULL) {
            if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) {
              if (!cfile->play_paused) mainw->pulsed->in_arate = cfile->arate * cfile->pb_fps / cfile->fps * audio_stretch;
              else mainw->pulsed->in_arate = cfile->arate * cfile->freeze_fps / cfile->fps * audio_stretch;
            } else mainw->pulsed->in_arate = cfile->arate * audio_stretch;
          } else {
            if (mainw->pulsed->read_abuf > -1) mainw->pulsed->abufs[mainw->pulsed->read_abuf]->arate = cfile->arate * audio_stretch;
          }
        }
      }
#endif
    }
#endif

    // playing back an event_list
    // here we need to add mainw->offsetticks, to get the correct position whe playing back in multitrack
    if (cfile->proc_ptr == NULL && cfile->next_event != NULL) {
      // playing an event_list
      if (mainw->scratch != SCRATCH_NONE && mainw->multitrack != NULL) {
#ifdef ENABLE_JACK_TRANSPORT
        // handle transport jump in multitrack : end current playback and restart it from the new position
        ticks_t transtc = q_gint64(jack_transport_get_time() * TICKS_PER_SECOND_DBL, cfile->fps);
        mainw->multitrack->pb_start_event = get_frame_event_at(mainw->multitrack->event_list, transtc, NULL, TRUE);
        if (mainw->cancelled == CANCEL_NONE) mainw->cancelled = CANCEL_EVENT_LIST_END;
#endif
      } else {
        if (mainw->multitrack != NULL) mainw->currticks += mainw->offsetticks; // add the offset of playback start time
        if (mainw->currticks >= event_start) {
          // see if we are playing a selection and reached the end
          if (mainw->multitrack != NULL && mainw->multitrack->playing_sel &&
              get_event_timecode(cfile->next_event) / TICKS_PER_SECOND_DBL >=
              mainw->multitrack->region_end) mainw->cancelled = CANCEL_EVENT_LIST_END;
          else {
            //cfile->next_event = process_events(cfile->next_event, FALSE, mainw->currticks);
            cfile->next_event = process_events(cfile->next_event, FALSE, lives_get_relative_ticks(mainw->origsecs,
                                               mainw->origusecs) + mainw->offsetticks);
            if (cfile->next_event == NULL) mainw->cancelled = CANCEL_EVENT_LIST_END;
            else {
              // see if we need to fill an audio buffer
#ifdef ENABLE_JACK
              if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL && mainw->abufs_to_fill > 0) {
                wake_audio_thread();
              }
#endif
#ifdef HAVE_PULSE_AUDIO
              if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL && mainw->abufs_to_fill > 0) {
                wake_audio_thread();
              }
#endif
	      // *INDENT-OFF*
            }}}}
      // *INDENT-ON*

      lives_widget_context_update(); // animate GUI, allow kb timer to run

      if (mainw->cancelled == CANCEL_NONE) return 0;
      cancel_process(visible);
      return mainw->cancelled;
    }

    // free playback

    if (mainw->currticks - last_kbd_ticks > KEY_RPT_INTERVAL * 100000) {
      // if we have a cached key (ctrl-up, ctrl-down, ctrl-left, crtl-right) trigger it here
      // this is to avoid the keyboard repeat delay (dammit !) so we get smooth trickplay
      // BUT we need a timer sufficiently large so it isn't triggered on every loop, to produce a constant repeat rate
      // but not so large so it doesn't get triggered enough
      if (last_kbd_ticks > 0) handle_cached_keys();
      last_kbd_ticks = mainw->currticks;
    }

    new_ticks = mainw->currticks + mainw->deltaticks; // deltaticks are set by scratch and other methods
    cfile->last_frameno = cfile->frameno;

    show_frame = FALSE;

    if (cfile->pb_fps != 0.) {
      // on calling:
      // mainw->startticks is the timecode of the last frame shown
      // new_ticks is the (adjusted) current time
      // cfile->last_frameno, cfile->pb_fps (if playing) or cfile->fps (if not) are also used in the calculation
      // as well as selection bounds and loop mode settings (if appropriate)
      //
      // on return, new_ticks is set to either mainw->starticks or the timecode of the next frame to show
      // which will be <= the current time
      // and cfile->frameno is set to the frame to show
      double opbfps = cfile->pb_fps;
      short scratch = mainw->scratch; // this gets reset in calc_new_playback_position()

      if (scratch == SCRATCH_BACK) {
        // scratch backwards: we temporarily play backwards at a quicker value
        new_ticks += KEY_RPT_INTERVAL * prefs->scratchback_amount;
        cfile->pb_fps = -cfile->pb_fps;
      }
      if (scratch == SCRATCH_FWD) {
        // scratch forwards: simpler; we just add to mainw->deltaticks
        new_ticks += (ticks_t)KEY_RPT_INTERVAL * (ticks_t)prefs->scratchfwd_amount;
        mainw->deltaticks += (ticks_t)KEY_RPT_INTERVAL * (ticks_t)prefs->scratchfwd_amount;
      }

      pthread_mutex_lock(&mainw->audio_resync_mutex);
      cfile->frameno = calc_new_playback_position(mainw->current_file, mainw->startticks, &new_ticks);

      if (scratch == SCRATCH_BACK) {
        // reset framerate to the proper value now
        if (cfile->pb_fps * opbfps > 0.) cfile->pb_fps = -opbfps;
        cfile->pb_fps = opbfps;
      }

      if (new_ticks != mainw->startticks) {
        mainw->startticks = new_ticks;
        pthread_mutex_unlock(&mainw->audio_resync_mutex);
        if (display_ready) {
          show_frame = TRUE;
#ifdef USE_GDK_FRAME_CLOCK
          display_ready = FALSE;
#endif
        }
      } else {
        pthread_mutex_unlock(&mainw->audio_resync_mutex);
        spare_cycles++;
      }
    }

    real_ticks = lives_get_relative_ticks(mainw->origsecs, mainw->origusecs);

    // play next frame
    if (LIVES_LIKELY(mainw->cancelled == CANCEL_NONE)) {
      // calculate the audio 'frame' for non-realtime audio players
      // for realtime players, we did this in calc_new_playback_position()
      if (!is_realtime_aplayer(prefs->audio_player)) {
        calc_aframeno(mainw->current_file);
        ///(real_ticks - mainw->firstticks) / TICKS_PER_SECOND_DBL * cfile->fps + audio_start;
        if (LIVES_UNLIKELY(mainw->loop_cont && (mainw->aframeno > (mainw->audio_end ? mainw->audio_end :
                                                cfile->laudio_time * cfile->fps)))) {
          mainw->firstticks = real_ticks;
        }
      }

#ifdef ENABLE_JACK
      // note the audio seek position
      if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL) {
        cfile->aseek_pos = mainw->jackd->seek_pos;
      }
#endif
#ifdef HAVE_PULSE_AUDIO
      // note the audio seek position
      if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL) {
        cfile->aseek_pos = mainw->pulsed->seek_pos;
      }
#endif
#if 0
      if (prefs->audio_player == AUD_PLAYER_NONE) {
        cfile->aseek_pos = nullaudio_get_seek_pos();
      }
#endif

      if (mainw->force_show || (mainw->fixed_fpsd <= 0. && show_frame && (mainw->vpp == NULL ||
                                mainw->vpp->fixed_fpsd <= 0. || !mainw->ext_playback)) ||
          (mainw->fixed_fpsd > 0. && (real_ticks - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL >= 1. / mainw->fixed_fpsd) ||
          (mainw->vpp != NULL && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback &&
           (real_ticks - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL >= 1. / mainw->vpp->fixed_fpsd)) {
        // time to show a new frame

        if (LIVES_IS_PLAYING && (mainw->event_list == NULL || mainw->record ||
                                 mainw->preview_rendering || (mainw->multitrack != NULL &&
                                     !mainw->multitrack->is_rendering))) {
          if (prefs->pbq_adaptive) {
            update_dfr(ABS(cfile->frameno - cfile->last_frameno) - 1, TRUE);
            if (spare_cycles) update_dfr(1, FALSE);
          }
        }

        // load and display the new frame
        load_frame_image(cfile->frameno);
        //g_print("lfi done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        if (mainw->last_display_ticks == 0) mainw->last_display_ticks = real_ticks;
        else {
          if (mainw->vpp != NULL && mainw->ext_playback && mainw->vpp->fixed_fpsd > 0.)
            mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->vpp->fixed_fpsd;
          else if (mainw->fixed_fpsd > 0.)
            mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->fixed_fpsd;
          else mainw->last_display_ticks = real_ticks;
        }
        mainw->force_show = FALSE;
        spare_cycles = 0ul;
      }
#ifdef ENABLE_JACK
      // request for another audio buffer - used only during mt render preview
      if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL && mainw->abufs_to_fill > 0) {
        fill_abuffer_from(mainw->jackd->abufs[mainw->write_abuf], mainw->event_list, NULL, FALSE);
      }
#endif
#ifdef HAVE_PULSE_AUDIO
      // request for another audio buffer - used only during mt render preview
      if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL && mainw->abufs_to_fill > 0) {
        fill_abuffer_from(mainw->pulsed->abufs[mainw->write_abuf], mainw->event_list, NULL, FALSE);
      }
#endif
    }
    // paused
    if (LIVES_UNLIKELY(cfile->play_paused)) {
      pthread_mutex_lock(&mainw->audio_resync_mutex);
      mainw->startticks = mainw->currticks + mainw->deltaticks;
      pthread_mutex_unlock(&mainw->audio_resync_mutex);
    }
  }

  if (visible) {
    if (cfile->proc_ptr == NULL) {
      // fixes a problem with opening preview with bg generator
      if (mainw->cancelled == CANCEL_NONE) mainw->cancelled = CANCEL_NO_PROPOGATE;
    } else {
      if (LIVES_IS_SPIN_BUTTON(mainw->framedraw_spinbutton))
        lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton), 1, cfile->proc_ptr->frames_done);
      // set the progress bar %
      update_progress(visible);
    }

    frames_done = cfile->proc_ptr->frames_done;

    if (cfile->clip_type == CLIP_TYPE_FILE && cfile->fx_frame_pump > 0) {
      if (virtual_to_images(mainw->current_file, cfile->fx_frame_pump, cfile->fx_frame_pump, FALSE, NULL)) {
        cfile->fx_frame_pump++;
      } else mainw->cancelled = CANCEL_ERROR;
      if (cfile->fx_frame_pump >= cfile->end) cfile->fx_frame_pump = 0; // all frames were realised
    }
  }

  if (LIVES_LIKELY(mainw->cancelled == CANCEL_NONE)) {
    boolean reload = FALSE;
    if ((xrfx = (lives_rfx_t *)mainw->vrfx_update) != NULL && fx_dialog[1] != NULL) {
      // the audio thread wants to update the parameter window
      mainw->vrfx_update = NULL;
      update_visual_params(xrfx, FALSE);
    }

    // the audio thread wants to update the parameter scroll(s)
    if (mainw->ce_thumbs) ce_thumbs_apply_rfx_changes();
refresh:
    // a segfault here can indicate memory corruption in an FX plugin
    lives_widget_context_update();  // animate GUI, allow kb timer to run
    sched_yield();

    if (LIVES_UNLIKELY(mainw->cancelled != CANCEL_NONE)) {
      cancel_process(visible);
      return 1000000 + mainw->cancelled;
    }

    if (mainw->force_show) {
      mainw->force_show = FALSE;
      if (!visible && !((mainw->vpp != NULL && mainw->ext_playback && mainw->vpp->fixed_fpsd > 0.) || (mainw->fixed_fpsd > 0.))) {
        reload = TRUE;
        goto refresh;
      }
    }

    if (reload) {
      reload = FALSE;
      load_frame_image(cfile->frameno);
      sched_yield();
      goto refresh;
    }

    return 0;
  }
  cancel_process(visible);

  return 2000000 + mainw->cancelled;
}

#ifdef USE_GDK_FRAME_CLOCK
static boolean using_gdk_frame_clock;
static GdkFrameClock *gclock;
static void clock_upd(GdkFrameClock * clock, gpointer user_data) {
  display_ready = TRUE;
}
#endif


static void reset_timebase(void) {
  // [IMPORTANT] we subtract these from every calculation to make the numbers smaller
#if _POSIX_TIMERS
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  mainw->origsecs = ts.tv_sec;
  mainw->origusecs = ts.tv_nsec / 1000;
#else

#ifdef USE_MONOTONIC_TIME
  mainw->origsecs = 0; // not used
  mainw->origusecs = lives_get_monotonic_time();
#else

  /***************************************************/
  gettimeofday(&tv, NULL);
  /***************************************************/

  mainw->origsecs = tv.tv_sec;
  mainw->origusecs = tv.tv_usec;
#endif
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed != NULL) {
    pa_time_reset(mainw->pulsed, 0);
  }
  if (mainw->pulsed_read != NULL) pa_time_reset(mainw->pulsed_read, 0);
#endif

#ifdef ENABLE_JACK
  if (mainw->jackd != NULL) {
    jack_time_reset(mainw->jackd, 0);
  }
  if (mainw->jackd_read != NULL) {
    jack_time_reset(mainw->jackd_read, 0);
  }
#endif
}


boolean do_progress_dialog(boolean visible, boolean cancellable, const char *text) {
  // monitor progress, return FALSE if the operation was cancelled

  // this is the outer loop for playback and all kinds of processing

  // visible is set for processing (progress dialog is visible)
  // or unset for video playback (progress dialog is not shown)

  FILE *infofile = NULL;
  char *mytext = NULL;

  int frames_done, frames;

  boolean got_err = FALSE;

  // translation issues
  if (visible && text != NULL) mytext = lives_strdup(text);

  if (visible) {
    // check we have sufficient storage space
    if (mainw->next_ds_warn_level != 0 &&
        !check_storage_space(mainw->current_file, FALSE)) {
      lives_cancel_t cancelled = mainw->cancelled;
      on_cancel_keep_button_clicked(NULL, NULL);
      if (mainw->cancelled != CANCEL_NONE) mainw->cancelled = cancelled;
      d_print_cancelled();
      return FALSE;
    }
  }

  event_start = 0;
  audio_start = mainw->play_start;
  if (visible) accelerators_swapped = FALSE;
  frames_done = 0;
  disp_fraction_done = 0.;
  mainw->last_display_ticks = 0;
  shown_paused_frames = FALSE;

  if (!visible) mainw->force_show = TRUE;
  else mainw->force_show = FALSE;

  mainw->cevent_tc = 0;

  progress_count = 0;
  progress_speed = 4.;
  prog_fs_check = 0;

  mainw->render_error = LIVES_RENDER_ERROR_NONE;

  if (!visible) {
    reset_frame_and_clip_index();
  }

  if (prefs->show_player_stats) {
    mainw->fps_measure = 0.;
  }

  mainw->cancelled = CANCEL_NONE;
  mainw->error = FALSE;
  clear_mainw_msg();
  if (!mainw->preview || cfile->opening) mainw->timeout_ticks = 0;

  if (visible) {
    mainw->is_processing = TRUE;
    desensitize();
    procw_desensitize();
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

    cfile->proc_ptr = create_processing(mytext);
    lives_freep((void **)&mytext);

    lives_progress_bar_set_pulse_step(LIVES_PROGRESS_BAR(cfile->proc_ptr->progressbar), .01);

    cfile->proc_ptr->frames_done = 0;

    if (!cancellable) {
      lives_widget_hide(cfile->proc_ptr->cancel_button);
    }

    if (!mainw->interactive) {
      lives_widget_set_sensitive(cfile->proc_ptr->cancel_button, FALSE);
      if (cfile->proc_ptr->stop_button != NULL)
        lives_widget_set_sensitive(cfile->proc_ptr->stop_button, FALSE);
      lives_widget_set_sensitive(cfile->proc_ptr->pause_button, FALSE);
      lives_widget_set_sensitive(cfile->proc_ptr->preview_button, FALSE);
    }

    if (cfile->opening && (capable->has_sox_play || (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL) ||
                           (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL)) && !LIVES_IS_PLAYING) {
      if (mainw->preview_box != NULL) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Preview"));
      lives_widget_set_tooltip_text(mainw->m_playbutton, _("Preview"));
      lives_widget_remove_accelerator(mainw->playall, mainw->accel_group, LIVES_KEY_p, (LiVESXModifierType)0);
      lives_widget_add_accelerator(cfile->proc_ptr->preview_button, LIVES_WIDGET_CLICKED_SIGNAL, mainw->accel_group, LIVES_KEY_p,
                                   (LiVESXModifierType)0, (LiVESAccelFlags)0);
      accelerators_swapped = TRUE;
    }
  }

  if (cfile->next_event != NULL) event_start = get_event_timecode(cfile->next_event);

  // mainw->origsecs, mainw->origusecs is our base for quantising
  // (and is constant for each playback session)

  // firstticks is to do with the audio "frame" for sox, mplayer
  // startticks is the ticks value of the last frame played

  mainw->startticks = mainw->currticks = mainw->offsetticks = 0;
  last_open_check_ticks = 0;

#ifdef ENABLE_JACK
  if (mainw->record && prefs->audio_src == AUDIO_SRC_EXT && prefs->audio_player == AUD_PLAYER_JACK &&
      mainw->jackd_read != NULL && prefs->ahold_threshold > 0.) {
    // if recording with external audio, wait for audio threshold before commencing
    mainw->jackd_read->abs_maxvol_heard = 0.;
    cfile->progress_end = 0;
    do_threaded_dialog(_("Waiting for external audio"), TRUE);
    while (mainw->jackd_read->abs_maxvol_heard < prefs->ahold_threshold && mainw->cancelled == CANCEL_NONE) {
      lives_usleep(prefs->sleep_time);
      threaded_dialog_spin(0.);
      lives_widget_context_update();
    }
    end_threaded_dialog();
    if (mainw->cancelled != CANCEL_NONE) return FALSE;
  }

#endif

#ifdef HAVE_PULSE_AUDIO
  // start audio recording now
  if (mainw->pulsed_read != NULL) {
    pulse_driver_uncork(mainw->pulsed_read);
  }
  if (mainw->record && prefs->audio_src == AUDIO_SRC_EXT && prefs->audio_player == AUD_PLAYER_PULSE &&
      prefs->ahold_threshold > 0.) {
    cfile->progress_end = 0;
    do_threaded_dialog(_("Waiting for external audio"), TRUE);
    while (mainw->pulsed_read->abs_maxvol_heard < prefs->ahold_threshold && mainw->cancelled == CANCEL_NONE) {
      lives_usleep(prefs->sleep_time);
      threaded_dialog_spin(0.);
      lives_widget_context_update();
    }
    end_threaded_dialog();
    if (mainw->cancelled != CANCEL_NONE) return FALSE;
  }
#endif
  last_kbd_ticks = 0;
  mainw->scratch = SCRATCH_NONE;
  if (mainw->iochan != NULL) lives_widget_show(cfile->proc_ptr->pause_button);
  display_ready = TRUE;

  last_time_source = LIVES_TIME_SOURCE_NONE;

  /////////////////////////
  reset_timebase();
  //////////////////////////

  if (visible) {
    proc_start_ticks = lives_get_current_ticks();
  } else {
    // video playback

#ifdef ENABLE_JACK_TRANSPORT
    if (mainw->jack_can_stop && mainw->multitrack == NULL && (prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT) &&
        !(mainw->record && !(prefs->rec_opts & REC_FRAMES) && cfile->next_event == NULL)) {
      // calculate the start position from jack transport

      ticks_t ntc = jack_transport_get_time() * TICKS_PER_SECOND_DBL;
      boolean noframedrop = prefs->noframedrop;
      prefs->noframedrop = FALSE;
      cfile->last_frameno = 1;
      if (prefs->jack_opts & JACK_OPTS_TIMEBASE_START) {
        mainw->play_start = calc_new_playback_position(mainw->current_file, 0, &ntc);
      }
      prefs->noframedrop = noframedrop;
      if (prefs->jack_opts & JACK_OPTS_TIMEBASE_CLIENT) {
        // timebase client - follows jack transport position
        mainw->startticks = ntc;
      }
      mainw->currticks = ntc;
    }
#endif
    cfile->last_frameno = cfile->frameno = mainw->play_start;

    // deltaticks is used for scratching (forwards and back)
    mainw->deltaticks = 0;
  }

  if (!mainw->playing_sel) mainw->play_start = 1;

  if (mainw->multitrack != NULL && !mainw->multitrack->is_rendering) {
    // playback start from middle of multitrack
    // calculate when we "would have started" at time 0
    mainw->offsetticks = get_event_timecode(mainw->multitrack->pb_start_event);
  }

  // set initial audio seek position for current file
  if (cfile->achans) {
    mainw->aframeno = calc_frame_from_time4(mainw->current_file,
                                            cfile->aseek_pos / cfile->arps / cfile->achans / (cfile->asampsize >> 3));
  }
  frames = cfile->frames;
  cfile->frames = 0; // allow seek beyond video length
  // MUST do re-seek after setting origsecs in order to set our clock properly
  // re-seek to new playback start
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && cfile->achans > 0 && cfile->laudio_time > 0. &&
      !mainw->is_rendering && !(cfile->opening && !mainw->preview) && mainw->jackd != NULL && mainw->jackd->playing_file > -1) {
    if (!jack_audio_seek_frame(mainw->jackd, mainw->aframeno)) {
      if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd, mainw->aframeno);
    }

    if (!(mainw->record && (prefs->audio_src == AUDIO_SRC_EXT || mainw->agen_key != 0 || mainw->agen_needs_reinit)))
      jack_get_rec_avals(mainw->jackd);
    else {
      mainw->rec_aclip = mainw->ascrap_file;
      mainw->rec_avel = 1.;
      mainw->rec_aseek = 0;
    }
    if (prefs->audio_src == AUDIO_SRC_INT)
      mainw->jackd->in_use = TRUE;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && cfile->achans > 0 && cfile->laudio_time > 0. &&
      !mainw->is_rendering && !(cfile->opening && !mainw->preview) && mainw->pulsed != NULL && mainw->pulsed->playing_file > -1) {
    if (!pulse_audio_seek_frame(mainw->pulsed, mainw->aframeno)) {
      handle_audio_timeout();
      return FALSE;
    }

    if (!(mainw->record && (prefs->audio_src == AUDIO_SRC_EXT || mainw->agen_key != 0 || mainw->agen_needs_reinit)))
      pulse_get_rec_avals(mainw->pulsed);
    else {
      mainw->rec_aclip = mainw->ascrap_file;
      mainw->rec_avel = 1.;
      mainw->rec_aseek = 0;
    }
    /* if (prefs->audio_src == AUDIO_SRC_INT) */
    /*   mainw->pulsed->in_use = TRUE; */
  }
#endif
  cfile->frames = frames;
  // tell jack transport we are ready to play
  mainw->video_seek_ready = TRUE;

#ifdef USE_GDK_FRAME_CLOCK
  using_gdk_frame_clock = FALSE;
  if (prefs->show_gui) {
    using_gdk_frame_clock = TRUE;
    display_ready = FALSE;
    gclock = gtk_widget_get_frame_clock(LIVES_MAIN_WINDOW_WIDGET);
    gdk_frame_clock_begin_updating(gclock);
    lives_signal_connect(LIVES_GUI_OBJECT(gclock), "update",
                         LIVES_GUI_CALLBACK(clock_upd),
                         NULL);
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read != NULL) {
    mainw->pulsed_read->is_paused = FALSE;
  }
#endif
#ifdef ENABLE_JACK
  if (mainw->jackd_read != NULL) {
    mainw->jackd_read->is_paused = FALSE;
  }
#endif

  if (mainw->record) mainw->record_paused = FALSE;

  //try to open info file - or if internal_messaging is TRUE, we get mainw->msg
  // from the mainw->progress_fn function
  while (1) {
    while (!mainw->internal_messaging && ((!visible && (mainw->whentostop != STOP_ON_AUD_END ||
                                           is_realtime_aplayer(prefs->audio_player))) ||
                                          !lives_file_test(cfile->info_file, LIVES_FILE_TEST_EXISTS))) {
      // just pulse the progress bar, or play video
      // returns a code if pb stopped
      int ret;
      if ((ret = process_one(visible))) {
        //g_print("pb stopped, reason %d\n", ret);
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
        if (mainw->current_file > -1 && cfile != NULL) lives_freep((void **)&cfile->op_dir);
#ifdef USE_GDK_FRAME_CLOCK
        if (using_gdk_frame_clock) {
          gdk_frame_clock_end_updating(gclock);
        }
#endif
        return FALSE;
      }

      if (LIVES_UNLIKELY(mainw->agen_needs_reinit)) {
        // we are generating audio from a plugin and it needs reinit - we do it in this thread so as not to hold up the player thread
        reinit_audio_gen();
      }

      if ((visible && !mainw->internal_messaging) || (LIVES_IS_PLAYING && cfile->play_paused)) lives_usleep(prefs->sleep_time);

      sched_yield();

      // normal playback, wth realtime audio player
      if (!visible && (mainw->whentostop != STOP_ON_AUD_END || is_realtime_aplayer(prefs->audio_player))) continue;

      if (mainw->iochan != NULL && progress_count == 0) {
        // pump data from stdout to textbuffer
        // this is for encoder output
        pump_io_chan(mainw->iochan);
      }
    }

    if (!mainw->internal_messaging) {
      // background processing (e.g. rendered effects)
      if ((infofile = fopen(cfile->info_file, "r"))) {
        // OK, now we might have some frames
        mainw->read_failed = FALSE;
        lives_fread(mainw->msg, 1, MAINW_MSG_SIZE, infofile);
        fclose(infofile);
      }
      progbar_pulse_or_fraction(cfile, cfile->proc_ptr->frames_done);
    }
    // else call realtime effect pass
    else {
      mainw->render_error = (*mainw->progress_fn)(FALSE);

      if (mainw->render_error >= LIVES_RENDER_ERROR) {
        if (mainw->current_file > -1 && cfile != NULL) lives_freep((void **)&cfile->op_dir);
        got_err = TRUE;
        goto finish;
      }

      // display progress fraction or pulse bar
      if (*(mainw->msg) && (frames_done = atoi(mainw->msg)) > 0)
        cfile->proc_ptr->frames_done = atoi(mainw->msg);
      else
        cfile->proc_ptr->frames_done = 0;

      if (!mainw->effects_paused) {
        if (prog_fs_check-- <= 0) {
          check_storage_space(mainw->current_file, TRUE);
          prog_fs_check = PROG_LOOP_VAL;
        }
        progbar_pulse_or_fraction(cfile, cfile->proc_ptr->frames_done);
      }
    }

    //#define DEBUG
#ifdef DEBUG
    if (*(mainw->msg)) g_print("%s msg %s\n", cfile->info_file, mainw->msg);
#endif

    // we got a message from the backend...

    if (visible && (!accelerators_swapped || cfile->opening) && cancellable && (!cfile->nopreview || cfile->keep_without_preview)) {
      if (!cfile->nopreview && !(cfile->opening && mainw->multitrack != NULL)) {
        lives_widget_show(cfile->proc_ptr->preview_button);
      }

      // show buttons
      if (cfile->opening_loc) {
        lives_widget_hide(cfile->proc_ptr->pause_button);
        if (cfile->proc_ptr->stop_button != NULL)
          lives_widget_show(cfile->proc_ptr->stop_button);
      } else {
        lives_widget_show(cfile->proc_ptr->pause_button);
        if (cfile->proc_ptr->stop_button != NULL)
          lives_widget_hide(cfile->proc_ptr->stop_button);
      }

      if (!cfile->opening && !cfile->nopreview) {
        lives_button_grab_default_special(cfile->proc_ptr->preview_button);
        if (mainw->preview_box != NULL) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Preview"));
        lives_widget_set_tooltip_text(mainw->m_playbutton, _("Preview"));
        lives_widget_remove_accelerator(mainw->playall, mainw->accel_group, LIVES_KEY_p, (LiVESXModifierType)0);
        lives_widget_add_accelerator(cfile->proc_ptr->preview_button, LIVES_WIDGET_CLICKED_SIGNAL, mainw->accel_group, LIVES_KEY_p,
                                     (LiVESXModifierType)0, (LiVESAccelFlags)0);
        accelerators_swapped = TRUE;
      }
    }

    //    g_print("MSG is %s\n", mainw->msg);

    if (lives_strncmp(mainw->msg, "completed", 8) && strncmp(mainw->msg, "error", 5) &&
        strncmp(mainw->msg, "killed", 6) && (visible ||
            ((lives_strncmp(mainw->msg, "video_ended", 11) || mainw->whentostop != STOP_ON_VID_END)
             && (lives_strncmp(mainw->msg, "audio_ended", 11) || mainw->preview ||
                 mainw->whentostop != STOP_ON_AUD_END)))) {
      // processing not yet completed...
      if (visible) {
        // last frame processed ->> will go from cfile->start to cfile->end
        int numtok = get_token_count(mainw->msg, '|');
        // get progress count from backend
        if (numtok > 1) {
          char **array = lives_strsplit(mainw->msg, "|", numtok);
          cfile->proc_ptr->frames_done = atoi(array[0]);
          if (numtok == 2 && *(array[1])) cfile->progress_end = atoi(array[1]);
          else if (numtok == 5 && *(array[4])) {
            // rendered generators
            cfile->start = cfile->undo_start = 1;
            cfile->frames = cfile->end = cfile->undo_end = atoi(array[0]);
            cfile->hsize = atoi(array[1]);
            cfile->vsize = atoi(array[2]);
            cfile->fps = cfile->pb_fps = strtod(array[3], NULL);
            if (cfile->fps == 0.) cfile->fps = cfile->pb_fps = prefs->default_fps;
            cfile->progress_end = atoi(array[4]);
          }
          lives_strfreev(array);
        } else cfile->proc_ptr->frames_done = atoi(mainw->msg);
      }

      // do a processing pass
      if (process_one(visible)) {
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
        if (mainw->current_file > -1 && cfile != NULL) lives_freep((void **)&cfile->op_dir);
#ifdef USE_GDK_FRAME_CLOCK
        if (using_gdk_frame_clock) {
          gdk_frame_clock_end_updating(gclock);
        }
#endif
        return FALSE;
      }

      if (LIVES_UNLIKELY(mainw->agen_needs_reinit)) {
        // we are generating audio from a plugin and it needs reinit - we do it in this thread so as not to hold up the player thread
        reinit_audio_gen();
      }

      if (mainw->iochan != NULL && progress_count == 0) {
        // pump data from stdout to textbuffer
        pump_io_chan(mainw->iochan);
      }

      lives_usleep(prefs->sleep_time);
    } else break;
  }

#ifdef USE_GDK_FRAME_CLOCK
  if (using_gdk_frame_clock) {
    gdk_frame_clock_end_updating(gclock);
  }
#endif

#ifdef DEBUG
  g_print("exit pt 3 %s\n", mainw->msg);
#endif

finish:

  //play/operation ended
  if (visible) {
    if (cfile->clip_type == CLIP_TYPE_DISK && (mainw->cancelled != CANCEL_NO_MORE_PREVIEW || !cfile->opening)) {
      lives_rm(cfile->info_file);
    }
    if (mainw->preview_box != NULL && !mainw->preview) lives_widget_set_tooltip_text(mainw->p_playbutton,
          _("Play all"));
    if (accelerators_swapped) {
      if (!mainw->preview) lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play all"));
      lives_widget_remove_accelerator(cfile->proc_ptr->preview_button, mainw->accel_group, LIVES_KEY_p, (LiVESXModifierType)0);
      lives_widget_add_accelerator(mainw->playall, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group, LIVES_KEY_p,
                                   (LiVESXModifierType)0,
                                   LIVES_ACCEL_VISIBLE);
      accelerators_swapped = FALSE;
    }
    if (cfile->proc_ptr != NULL) {
      const char *btext = NULL;
      if (mainw->iochan != NULL) btext = lives_text_view_get_text(mainw->optextview);
      if (cfile->proc_ptr->processing != NULL) lives_widget_destroy(cfile->proc_ptr->processing);
      lives_free(cfile->proc_ptr);
      cfile->proc_ptr = NULL;
      if (btext != NULL) {
        lives_text_view_set_text(mainw->optextview, btext, -1);
        lives_free((char *)btext);
      }
    }
    mainw->is_processing = FALSE;
    if (!(cfile->menuentry == NULL)) {
      // note - for operations to/from clipboard (file 0) we
      // should manually call sensitize() after operation
      sensitize();
    }
  } else {
    if (prefs->show_player_stats) {
      if (mainw->fps_measure > 0.) {
        mainw->fps_measure /= (lives_get_relative_ticks(mainw->origsecs, mainw->origusecs) / TICKS_PER_SECOND_DBL);
      }
    }
    mainw->is_processing = TRUE;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  if (mainw->current_file > -1 && cfile != NULL) lives_freep((void **)&cfile->op_dir);

  // get error message (if any)
  if (!strncmp(mainw->msg, "error", 5)) {
    handle_backend_errors(FALSE, NULL);
    if (mainw->cancelled || mainw->error) return FALSE;
  } else {
    if (!check_storage_space(mainw->current_file, FALSE)) return FALSE;
  }

  if (got_err) return FALSE;

#ifdef DEBUG
  g_print("exiting progress dialog\n");
#endif
  return TRUE;
}


#define MIN_FLASH_TIME 100000000

boolean do_auto_dialog(const char *text, int type) {
  // type 0 = normal auto_dialog
  // type 1 = countdown dialog for audio recording
  // type 2 = normal with cancel

  FILE *infofile = NULL;

  uint64_t time = 0, stime = 0;

  xprocess *proc_ptr;

  char *label_text;
  char *mytext = lives_strdup(text);

  int time_rem, last_time_rem = 10000000;
  lives_alarm_t alarm_handle = 0;

  if (type == 1 && mainw->rec_end_time != -1.) {
    stime = lives_get_current_ticks();
  }

  mainw->error = FALSE;

  proc_ptr = create_processing(mytext);

  lives_freep((void **)&mytext);
  if (proc_ptr->stop_button != NULL)
    lives_widget_hide(proc_ptr->stop_button);

  if (type == 2) {
    lives_widget_show(proc_ptr->cancel_button);
    lives_widget_hide(proc_ptr->pause_button);
    mainw->cancel_type = CANCEL_SOFT;
  }
  if (type == 0) {
    lives_widget_hide(proc_ptr->cancel_button);
  }

  lives_progress_bar_set_pulse_step(LIVES_PROGRESS_BAR(proc_ptr->progressbar), .01);

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_set_cursor_style(LIVES_CURSOR_BUSY, proc_ptr->processing);
  lives_widget_context_update();

  if (type == 0 || type == 2) {
    clear_mainw_msg();
    alarm_handle = lives_alarm_set(MIN_FLASH_TIME); // don't want to flash too fast...
  } else if (type == 1) {
    // show buttons
    if (cfile->proc_ptr->stop_button != NULL)
      lives_widget_show(proc_ptr->stop_button);
    lives_widget_show(proc_ptr->cancel_button);
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed_read != NULL) {
      pulse_driver_uncork(mainw->pulsed_read);
    }
#endif
    if (mainw->rec_samples != 0) {
      lives_widget_context_update();
      lives_usleep(prefs->sleep_time);
    }
  }

  while (mainw->cancelled == CANCEL_NONE && !(infofile = fopen(cfile->info_file, "r"))) {
    lives_progress_bar_pulse(LIVES_PROGRESS_BAR(proc_ptr->progressbar));
    lives_widget_context_update();
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    lives_widget_process_updates(proc_ptr->processing, TRUE);
    lives_usleep(prefs->sleep_time);
    if (type == 1 && mainw->rec_end_time != -1.) {
      time = lives_get_current_ticks();

      // subtract start time
      time -= stime;

      time_rem = (int)(mainw->rec_end_time - (double)time / TICKS_PER_SECOND_DBL + .5);
      if (time_rem >= 0 && time_rem < last_time_rem) {
        label_text = lives_strdup_printf(_("\nTime remaining: %d sec"), time_rem);
        lives_label_set_text(LIVES_LABEL(proc_ptr->label2), label_text);
        lives_free(label_text);
        last_time_rem = time_rem;
      } else if (time_rem < 0) break;
    }
  }

  if (!mainw->cancelled) {
    if (infofile != NULL) {
      if (type == 0 || type == 2) {
        mainw->read_failed = FALSE;
        lives_fread(mainw->msg, 1, MAINW_MSG_SIZE, infofile);
        fclose(infofile);
        infofile = NULL;
        if (cfile->clip_type == CLIP_TYPE_DISK) lives_rm(cfile->info_file);
        if (alarm_handle > 0) {
          ticks_t tl;
          while ((tl = lives_alarm_check(alarm_handle)) > 0 && !mainw->cancelled) {
            lives_progress_bar_pulse(LIVES_PROGRESS_BAR(proc_ptr->progressbar));
            lives_widget_context_update();
            lives_usleep(prefs->sleep_time);
          }
          lives_alarm_clear(alarm_handle);
        }
      } else fclose(infofile);
    }
  }

  if (proc_ptr != NULL) {
    lives_widget_destroy(proc_ptr->processing);
    lives_free(proc_ptr);
  }

  if (type == 2) mainw->cancel_type = CANCEL_KILL;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  if (mainw->cancelled) return FALSE;

  // get error message (if any)
  if (type != 1 && !strncmp(mainw->msg, "error", 5)) {
    handle_backend_errors(FALSE, NULL);
    if (mainw->cancelled || mainw->error) return FALSE;
  } else {
    if (mainw->current_file > -1 && cfile != NULL)
      if (!check_storage_space(mainw->current_file, FALSE)) return FALSE;
  }
  return TRUE;
}


///// TODO: split warnings etc into separate file   ///

boolean do_save_clipset_warn(void) {
  char *extra;
  char *msg;

  if (!mainw->no_exit && !mainw->only_close) extra = lives_strdup(", and LiVES will exit");
  else extra = lives_strdup("");

  msg = lives_strdup_printf(
          _("Saving the set will cause copies of all loaded clips to remain on the disk%s.\n\n"
            "Please press 'Cancel' if that is not what you want.\n"),
          extra);
  lives_free(extra);

  if (!do_warning_dialog_with_check(msg, WARN_MASK_SAVE_SET)) {
    lives_free(msg);
    return FALSE;
  }
  lives_free(msg);
  return TRUE;
}


void too_many_files(void) {
  char *warn = lives_strdup_printf(_("\nSorry, LiVES can only open %d files at once.\nPlease close a file and then try again."),
                                   MAX_FILES);
  do_error_dialog(warn);
  lives_free(warn);
}


void workdir_warning(void) {
  char *tmp, *com = lives_strdup_printf(
                      _("LiVES was unable to write to its working directory.\n\nThe current working directory is:\n\n%s\n\n"
                        "Please make sure you can write to this directory."),
                      (tmp = lives_filename_to_utf8(prefs->workdir, -1, NULL, NULL, NULL)));
  lives_free(tmp);
  if (mainw != NULL && mainw->is_ready) {
    do_error_dialog(com);
  }
  lives_free(com);
}


void do_no_mplayer_sox_error(void) {
  do_blocking_error_dialog(_("\nLiVES currently requires either 'mplayer', 'mplayer2', or 'sox' to function. "
                             "Please install one or other of these, and try again.\n"));
}


void do_need_mplayer_dialog(void) {
  do_error_dialog(
    _("\nThis function requires either mplayer or mplayer2 to operate.\nYou may wish to install one or other of these and try again.\n"));
}


void do_need_mplayer_mpv_dialog(void) {
  do_error_dialog(
    _("\nThis function requires either mplayer, mplayer2 or mpv to operate.\nYou may wish to install one or other of these and try again.\n"));
}


void do_audio_warning(void) {
  do_error_dialog(_("Audio was not loaded; please install mplayer or mplayer2 if you expected audio for this clip.\n"));
}


void do_encoder_sox_error(void) {
  do_error_dialog(
    _("Audio resampling is required for this format.\nPlease install 'sox'\nOr switch to another encoder format in Tools | Preferences | Encoding\n"));
}


void do_encoder_acodec_error(void) {
  do_error_dialog(
    _("\n\nThis encoder/format cannot use the requested audio codec.\nPlease set the audio codec in Tools|Preferences|Encoding\n"));
}


void do_layout_scrap_file_error(void) {
  do_blocking_error_dialog(
    _("This layout includes generated frames.\nIt cannot be saved, you must render it to a clip first.\n"));
}


void do_layout_ascrap_file_error(void) {
  do_blocking_error_dialog(
    _("This layout includes generated or recorded audio.\nIt cannot be saved, you must render it to a clip first.\n"));
}


boolean rdet_suggest_values(int width, int height, double fps, int fps_num, int fps_denom, int arate, int asigned,
                            boolean swap_endian, boolean anr, boolean ignore_fps) {
  LiVESWidget *prep_dialog;

  char *msg1 = lives_strdup_printf(_("\n\nDue to restrictions in the %s format\n"), prefs->encoder.of_desc);
  char *msg2 = lives_strdup("");
  char *msg3 = lives_strdup("");
  char *msg4 = lives_strdup("");
  char *msg5 = lives_strdup("");
  char *msg6 = lives_strdup("");
  char *msg7 = lives_strdup("");
  char *msg8 = lives_strdup("");
  char *msg_a;

  boolean ochange = FALSE;
  boolean ret;

  mainw->fx1_bool = FALSE;

  if (swap_endian || (asigned == 1 && rdet->aendian == AFORM_UNSIGNED) || (asigned == 2 && rdet->aendian == AFORM_SIGNED) ||
      (fps > 0. && fps != rdet->fps) || (fps_denom > 0 && (fps_num * 1.) / (fps_denom * 1.) != rdet->fps) ||
      (!anr && (rdet->width != width || rdet->height != height) && height * width > 0) ||
      (arate != rdet->arate && arate > 0)) {
    lives_free(msg2);
    msg2 = lives_strdup(_("LiVES recommends the following settings:\n\n"));
    if (swap_endian || (asigned == 1 && rdet->aendian == AFORM_UNSIGNED) || (asigned == 2 && rdet->aendian == AFORM_SIGNED)
        || (arate > 0 && arate != rdet->arate)) {
      char *sstring;
      char *estring;

      if (asigned == 1 && rdet->aendian == AFORM_UNSIGNED) sstring = lives_strdup(_(", signed"));
      else if (asigned == 2 && rdet->aendian == AFORM_SIGNED) sstring = lives_strdup(_(", unsigned"));
      else sstring = lives_strdup("");

      if (swap_endian) {
        if (mainw->endian != AFORM_BIG_ENDIAN) estring = lives_strdup(_(", little-endian"));
        else estring = lives_strdup(_(", big-endian"));
      } else estring = lives_strdup("");

      ochange = TRUE;
      lives_free(msg3);
      msg3 = lives_strdup_printf(_("Use an audio rate of %d Hz%s%s\n"), arate, sstring, estring);
      lives_free(sstring);
      lives_free(estring);
    }
    if (!ignore_fps) {
      ochange = TRUE;
      if (fps > 0 && fps != rdet->fps) {
        lives_free(msg4);
        msg4 = lives_strdup_printf(_("Set video rate to %.3f frames per second\n"), fps);
      } else if (fps_denom > 0 && (fps_num * 1.) / (fps_denom * 1.) != rdet->fps) {
        lives_free(msg4);
        msg4 = lives_strdup_printf(_("Set video rate to %d:%d frames per second\n"), fps_num, fps_denom);
      }
    }
    if (!anr && ((rdet->width != width || rdet->height != height) && height * width > 0)) {
      lives_free(msg5);
      msg5 = lives_strdup_printf(_("Set video size to %d x %d pixels\n"), width, height);
      mainw->fx1_bool = TRUE;
    }
  }
  if (anr || arate < 0) {
    if (arate < 1 || ((rdet->width != width || rdet->height != height) && height * width > 0)) {
      lives_free(msg6);
      if (!ochange) anr = FALSE;
      msg6 = lives_strdup(_("\nYou may wish to:\n"));
      if ((rdet->width != width || rdet->height != height) && height * width > 0) {
        lives_free(msg7);
        msg7 = lives_strdup_printf(_("resize video to %d x %d pixels\n"), width, height);
      } else anr = FALSE;
      if (arate < 1) {
        lives_free(msg8);
        msg8 = lives_strdup(_("disable audio, since the target encoder cannot encode audio\n"));
      }
    } else anr = FALSE;
  }
  msg_a = lives_strconcat(msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, NULL);
  lives_free(msg1);
  lives_free(msg2);
  lives_free(msg3);
  lives_free(msg4);
  lives_free(msg5);
  lives_free(msg6);
  lives_free(msg7);
  lives_free(msg8);
  prep_dialog = create_encoder_prep_dialog(msg_a, NULL, anr);
  lives_free(msg_a);
  ret = (lives_dialog_run(LIVES_DIALOG(prep_dialog)) == LIVES_RESPONSE_OK);
  lives_widget_destroy(prep_dialog);
  return ret;
}


boolean do_encoder_restrict_dialog(int width, int height, double fps, int fps_num, int fps_denom, int arate, int asigned,
                                   boolean swap_endian, boolean anr, boolean save_all) {
  LiVESWidget *prep_dialog;

  char *msg1 = lives_strdup_printf(_("\n\nDue to restrictions in the %s format\n"), prefs->encoder.of_desc);
  char *msg2 = lives_strdup("");
  char *msg3 = lives_strdup("");
  char *msg4 = lives_strdup("");
  char *msg5 = lives_strdup("");
  char *msg6 = lives_strdup("");
  char *msg7 = lives_strdup("");
  char *msg_a, *msg_b = NULL;

  double cfps;

  boolean ret;

  int carate, chsize, cvsize;

  if (rdet != NULL) {
    carate = rdet->arate;
    chsize = rdet->width;
    cvsize = rdet->height;
    cfps = rdet->fps;
  } else {
    carate = cfile->arate;
    chsize = cfile->hsize;
    cvsize = cfile->vsize;
    cfps = cfile->fps;
  }

  if (swap_endian || asigned != 0 || (arate > 0 && arate != carate) || (fps > 0. && fps != cfps) ||
      (fps_denom > 0 && (fps_num * 1.) / (fps_denom * 1.) != cfps) || (!anr &&
          (chsize != width || cvsize != height) && height * width > 0)) {
    lives_free(msg2);
    msg2 = lives_strdup(_("LiVES must:\n"));
    if (swap_endian || asigned != 0 || (arate > 0 && arate != carate)) {
      char *sstring;
      char *estring;
      if (asigned == 1) sstring = lives_strdup(_(", signed"));
      else if (asigned == 2) sstring = lives_strdup(_(", unsigned"));
      else sstring = lives_strdup("");

      if (swap_endian) {
        if (cfile->signed_endian & AFORM_BIG_ENDIAN) estring = lives_strdup(_(", little-endian"));
        else estring = lives_strdup(_(", big-endian"));
      } else estring = lives_strdup("");

      lives_free(msg3);
      msg3 = lives_strdup_printf(_("resample audio to %d Hz%s%s\n"), arate, sstring, estring);
      lives_free(sstring);
      lives_free(estring);

    }
    if (fps > 0 && fps != cfps) {
      lives_free(msg4);
      msg4 = lives_strdup_printf(_("resample video to %.3f frames per second\n"), fps);
    } else if (fps_denom > 0 && (fps_num * 1.) / (fps_denom * 1.) != cfps) {
      lives_free(msg4);
      msg4 = lives_strdup_printf(_("resample video to %d:%d frames per second\n"), fps_num, fps_denom);
    }
    if (!anr && ((chsize != width || cvsize != height) && height * width > 0)) {
      lives_free(msg5);
      msg5 = lives_strdup_printf(_("resize video to %d x %d pixels\n"), width, height);
      mainw->fx1_bool = TRUE;
    }
  }
  if (anr) {
    if ((chsize != width || cvsize != height) && height * width > 0) {
      lives_free(msg6);
      lives_free(msg7);
      msg6 = lives_strdup(_("\nYou may wish to:\n"));
      msg7 = lives_strdup_printf(_("Set video size to %d x %d pixels\n"), width, height);
    } else anr = FALSE;
  }
  msg_a = lives_strconcat(msg1, msg2, msg3, msg4, msg5, msg6, msg7, NULL);
  if (save_all) {
    msg_b = lives_strdup(
              _("\nYou will be able to undo these changes afterwards.\n\nClick `OK` to proceed, `Cancel` to abort.\n\n"));
  } else {
    msg_b = lives_strdup(_("\nChanges applied to the selection will not be permanent.\n\n"));
  }
  lives_free(msg1);
  lives_free(msg2);
  lives_free(msg3);
  lives_free(msg4);
  lives_free(msg5);
  lives_free(msg6);
  lives_free(msg7);
  prep_dialog = create_encoder_prep_dialog(msg_a, msg_b, anr);
  lives_free(msg_a);
  if (msg_b != NULL) lives_free(msg_b);
  ret = (lives_dialog_run(LIVES_DIALOG(prep_dialog)) == LIVES_RESPONSE_OK);
  lives_widget_destroy(prep_dialog);
  return ret;
}


void perf_mem_warning(void) {
  do_error_dialog(
    _("\n\nLiVES was unable to record a performance. There is currently insufficient memory available.\n"
      "Try recording for just a selection of the file."));
}


boolean do_clipboard_fps_warning(void) {
  if (prefs->warning_mask & WARN_MASK_FPS) {
    return TRUE;
  }
  return do_warning_dialog_with_check(
           _("The playback speed (fps), or the audio rate\n of the clipboard does not match\n"
             "the playback speed or audio rate of the clip you are inserting into.\n\n"
             "The insertion will be adjusted to fit into the clip.\n\nPlease press Cancel to abort the insert, or OK to continue."),
           WARN_MASK_FPS);
}


LIVES_GLOBAL_INLINE boolean do_reload_set_query(void) {
  return do_yesno_dialog(_("Current clips will be added to the clip set.\nIs that what you want ?\n"));
}


LIVES_GLOBAL_INLINE boolean findex_bk_dialog(const char *fname_back) {
  return do_yesno_dialogf(_("I can attempt to restore the frame index from a backup.\n(%s)\nShall I try ?\n"), fname_back);
}


LIVES_GLOBAL_INLINE boolean paste_enough_dlg(int lframe) {
  return do_yesno_dialogf(P_("\nPaste %d frame ?\n", "Paste %d frames ?\n", lframe), lframe);
}

boolean do_yuv4m_open_warning(void) {
  char *msg;
  boolean resp;
  if (prefs->warning_mask & WARN_MASK_OPEN_YUV4M) {
    return TRUE;
  }
  msg = lives_strdup_printf(
          _("When opening a yuvmpeg stream, you should first create a fifo file, and then write yuv4mpeg frames to it.\n"
            "Now you will get a chance to browse for the fifo file here.\nFollowing that,\n"
            "LiVES will pause briefly until frames are received.\nYou should only click OK if you understand what you are doing, "
            "otherwise, click Cancel."),
          prefs->workdir);
  resp = do_warning_dialog_with_check(msg, WARN_MASK_OPEN_YUV4M);
  lives_free(msg);
  return resp;
}


boolean do_comments_dialog(int fileno, char *filename) {
  lives_clip_t *sfile = mainw->files[fileno];

  boolean response;
  boolean encoding = FALSE;

  commentsw = create_comments_dialog(sfile, filename);

  if (sfile == NULL) sfile = cfile;
  else encoding = TRUE;

  while (1) {
    if ((response = (lives_dialog_run(LIVES_DIALOG(commentsw->comments_dialog)) == LIVES_RESPONSE_OK))) {
      lives_snprintf(sfile->title, 256, "%s", lives_entry_get_text(LIVES_ENTRY(commentsw->title_entry)));
      lives_snprintf(sfile->author, 256, "%s", lives_entry_get_text(LIVES_ENTRY(commentsw->author_entry)));
      lives_snprintf(sfile->comment, 256, "%s", lives_entry_get_text(LIVES_ENTRY(commentsw->comment_entry)));

      save_clip_value(fileno, CLIP_DETAILS_TITLE, sfile->title);
      save_clip_value(fileno, CLIP_DETAILS_AUTHOR, sfile->author);
      save_clip_value(fileno, CLIP_DETAILS_COMMENT, sfile->comment);

      if (encoding && sfile->subt != NULL && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(commentsw->subt_checkbutton))) {
        char *ext = get_extension(lives_entry_get_text(LIVES_ENTRY(commentsw->subt_entry)));
        if (strcmp(ext, LIVES_FILE_EXT_SUB) && strcmp(ext, LIVES_FILE_EXT_SRT)) {
          if (!do_sub_type_warning(ext, sfile->subt->type == SUBTITLE_TYPE_SRT ? LIVES_FILE_EXT_SRT : LIVES_FILE_EXT_SUB)) {
            lives_entry_set_text(LIVES_ENTRY(commentsw->subt_entry), mainw->subt_save_file);
            lives_free(ext);
            continue;
          }
        }
        lives_free(ext);
        lives_freep((void **)&mainw->subt_save_file);
        mainw->subt_save_file = lives_strdup(lives_entry_get_text(LIVES_ENTRY(commentsw->subt_entry)));
      } else {
        lives_freep((void **)&mainw->subt_save_file);
        mainw->subt_save_file = NULL;
      }
      lives_widget_destroy(commentsw->comments_dialog);
    }
    break;
  }

  lives_free(commentsw);
  return response;
}


void do_messages_window(void) {
  char *text = dump_messages(-1, -1);
  widget_opts.expand = LIVES_EXPAND_EXTRA;
  create_text_window(_("Message History"), text, NULL);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_free(text);
}


void do_upgrade_error_dialog(void) {
  char *tmp;
  char *msg = lives_strdup_printf(
                _("After upgrading/installing, you may need to adjust the <prefix_dir> setting in your %s file"),
                (tmp = lives_filename_to_utf8(capable->rcfile, -1, NULL, NULL, NULL)));
  startup_message_info(msg);
  lives_free(msg);
  lives_free(tmp);
}


void do_rendered_fx_dialog(void) {
  char *tmp;
  char *msg = lives_strdup_printf(
                _("\n\nLiVES could not find any rendered effect plugins.\nPlease make sure you have them installed in\n"
                  "%s%s%s\nor change the value of <lib_dir> in %s\n"),
                prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_RENDERED_EFFECTS_BUILTIN,
                (tmp = lives_filename_to_utf8(capable->rcfile, -1, NULL, NULL,
                       NULL)));
  do_error_dialog_with_check(msg, WARN_MASK_RENDERED_FX);
  lives_free(msg);
  lives_free(tmp);
}


void do_audio_import_error(void) {
  char *msg = lives_strdup(_("Sorry, unknown audio type.\n\n (Filenames must end in"));
  char *tmp;

  char *filt[] = LIVES_AUDIO_LOAD_FILTER;

  register int i = 0;

  while (filt[i] != NULL) {
    if (filt[i + 1] == NULL) {
      tmp = lives_strdup_printf("%s or .%s)", msg, filt[i] + 2);
    } else if (i == 0) {
      tmp = lives_strdup_printf("%s .%s)", msg, filt[i] + 2);
    } else {
      tmp = lives_strdup_printf("%s, .%s)", msg, filt[i] + 2);
    }
    lives_free(msg);
    msg = tmp;
    i++;
  }

  do_error_dialog(msg);
  lives_free(msg);
  d_print(_("failed (unknown type)\n"));
}


boolean prompt_remove_layout_files(void) {
  return (do_yesno_dialog(
            _("\nDo you wish to remove the layout files associated with this set ?\n(They will not be usable without the set).\n")));
}


boolean do_set_duplicate_warning(const char *new_set) {
  char *msg = lives_strdup_printf(
                _("\nA set entitled %s already exists.\nClick OK to add the current clips and layouts to the existing set.\n"
                  "Click Cancel to pick a new name.\n"),
                new_set);
  boolean retcode = do_warning_dialog_with_check(msg, WARN_MASK_DUPLICATE_SET);
  lives_free(msg);
  return retcode;
}


boolean do_layout_alter_frames_warning(void) {
  return do_warning_dialog(
           _("\nFrames from this clip are used in some multitrack layouts.\nAre you sure you wish to continue ?\n."));
}


boolean do_layout_alter_audio_warning(void) {
  return do_warning_dialog(
           _("\nAudio from this clip is used in some multitrack layouts.\nAre you sure you wish to continue ?\n."));
}


LiVESResponseType do_original_lost_warning(const char *fname) {
  char *msg = lives_strdup_printf(
                _("\nThe original file\n%s\ncould not be found.\nClick Retry to try again, or Browse to browse to the new location.\n"
                  "Otherwise click Cancel to skip loading this file.\n"),
                fname);
  LiVESWidget *warning = create_message_dialog(LIVES_DIALOG_CANCEL_RETRY_BROWSE, msg,
                         LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), 0, TRUE);
  LiVESResponseType response = lives_dialog_run(LIVES_DIALOG(warning));
  lives_widget_destroy(warning);
  lives_widget_context_update();
  lives_free(msg);
  return response;
}


void do_no_decoder_error(const char *fname) {
  do_blocking_error_dialogf(
    _("\n\nLiVES could not find a required decoder plugin for the clip\n%s\nThe clip could not be loaded.\n"), fname);
}


void do_no_loadfile_error(const char *fname) {
  do_blocking_error_dialogf(_("\n\nThe file\n%s\nCould not be found.\n"), fname);
}


#ifdef ENABLE_JACK
void do_jack_noopen_warn(void) {
  do_blocking_error_dialogf(_("\nUnable to start up jack. Please ensure that %s is set up correctly on your machine\n"
                              "and also that the soundcard is not in use by another program\nAutomatic jack startup will be disabled now.\n"),
                            JACK_DRIVER_NAME);
}


void do_jack_noopen_warn3(void) {
  do_blocking_error_dialog(_("\nUnable to connect to jack server. Please start jack before starting LiVES\n"));
}


void do_jack_noopen_warn4(void) {
#ifdef HAVE_PULSE_AUDIO
  const char *otherbit = "\"lives -aplayer pulse\"";
#else
  const char *otherbit = "\"lives -aplayer sox\"";
#endif
  char *msg = lives_strdup_printf(_("\nAlternatively, try to start lives with either:\n\n\"lives -jackopts 16\", or\n\n%s\n"),
                                  otherbit);
  do_blocking_info_dialog(msg);
  lives_free(msg);
}


void do_jack_noopen_warn2(void) {
  do_blocking_info_dialog(_("\nAlternately, you can restart LiVES and select another audio player.\n"));
}
#endif

void do_mt_backup_space_error(lives_mt * mt, int memreq_mb) {
  char *msg = lives_strdup_printf(
                _("\n\nLiVES needs more backup space for this layout.\nYou can increase the value in Preferences/Multitrack.\n"
                  "It is recommended to increase it to at least %d MB"),
                memreq_mb);
  do_error_dialog_with_check_transient(msg, TRUE, WARN_MASK_MT_BACKUP_SPACE, LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  lives_free(msg);
}


boolean do_set_rename_old_layouts_warning(const char *new_set) {
  return do_yesno_dialogf(
           _("\nSome old layouts for the set %s already exist.\nIt is recommended that you delete them.\nDo you wish to delete them ?\n"),
           new_set);
}


void do_mt_undo_mem_error(void) {
  do_error_dialog(
    _("\nLiVES was unable to reserve enough memory for multitrack undo.\n"
      "Either close some other applications, or reduce the undo memory\n"
      "using Preferences/Multitrack/Undo Memory\n"));
}


void do_mt_undo_buf_error(void) {
  do_error_dialog(_("\nOut of memory for undo.\nYou may need to increase the undo memory\n"
                    "using Preferences/Multitrack/Undo Memory\n"));
}


void do_mt_set_mem_error(boolean has_mt, boolean trans) {
  char *msg1 = (_("\nLiVES was unable to reserve enough memory for the multitrack undo buffer.\n"));
  char *msg2;
  char *msg3 = (_("or enter a smaller value.\n"));
  char *msg;
  if (has_mt) msg2 = (_("Try again from the clip editor, try closing some other applications\n"));
  else msg2 = (_("Try closing some other applications\n"));

  msg = lives_strdup_printf("%s%s%s", msg1, msg2, msg3);

  if (!trans) do_blocking_error_dialog(msg);
  else do_error_dialog_with_check_transient(msg, TRUE, 0, LIVES_WINDOW(prefsw->prefs_dialog));
  lives_free(msg);
}


void do_mt_audchan_error(int warn_mask) {
  do_error_dialog_with_check_transient(
    _("Multitrack is set to 0 audio channels, but this layout has audio.\nYou should adjust the audio settings from the Tools menu.\n"),
    warn_mask, FALSE, NULL);
}


void do_mt_no_audchan_error(void) {
  do_error_dialog(_("The current layout has audio, so audio channels may not be set to zero.\n"));
}


void do_mt_no_jack_error(int warn_mask) {
  do_error_dialog_with_check(
    _("Multitrack audio preview is only available with the\n\"jack\" or \"pulseaudio\" audio player.\n"
      "You can set this in Tools|Preferences|Playback."),
    warn_mask);
}


boolean do_mt_rect_prompt(void) {
  return do_yesno_dialog(
           _("Errors were detected in the layout (which may be due to transferring from another system, "
             "or from an older version of LiVES).\n"
             "Should I try to repair the disk copy of the layout ?\n"));
}


void do_bad_layout_error(void) {
  do_error_dialog(_("LiVES was unable to load the layout.\nSorry.\n"));
}


void do_program_not_found_error(const char *progname) {
  char *msg = lives_strdup_printf(_("The program %s is required to use this feature.\nPlease install it and try again."),
                                  progname);
  do_blocking_error_dialog(msg);
  lives_free(msg);
}


void do_lb_composite_error(void) {
  do_blocking_error_dialog(
    _("LiVES currently requires composite from ImageMagick to do letterboxing.\nPlease install 'imagemagick' and try again."));
}


void do_lb_convert_error(void) {
  do_blocking_error_dialog(
    _("LiVES currently requires convert from ImageMagick to do letterboxing.\nPlease install 'imagemagick' and try again."));
}


void do_ra_convert_error(void) {
  do_blocking_error_dialog(
    _("LiVES currently requires convert from ImageMagick resize frames.\nPlease install 'imagemagick' and try again."));
}


void do_audrate_error_dialog(void) {
  do_error_dialog(_("\n\nAudio rate must be greater than 0.\n"));
}


boolean do_event_list_warning(void) {
  return do_yesno_dialog(
           _("\nEvent list will be very large\nand may take a long time to display.\nAre you sure you wish to view it ?\n"));
}


void do_dvgrab_error(void) {
  do_error_dialog(_("\n\nYou must install 'dvgrab' to use this function.\n"));
}


void do_nojack_rec_error(void) {
  do_error_dialog(
    _("\n\nAudio recording can only be done using either\nthe \"jack\" or the \"pulseaudio\" audio player.\n"
      "You may need to select one of these in Tools/Preferences/Playback.\n"));
}


void do_vpp_palette_error(void) {
  do_error_dialog_with_check_transient(_("Video playback plugin failed to initialise palette !\n"), TRUE, 0,
                                       prefsw != NULL ? LIVES_WINDOW(prefsw->prefs_dialog) : LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
}


void do_decoder_palette_error(void) {
  do_blocking_error_dialog(_("Decoder plugin failed to initialise palette !\n"));
}


void do_vpp_fps_error(void) {
  do_error_dialog_with_check_transient(_("Unable to set framerate of video plugin\n"), TRUE, 0,
                                       prefsw != NULL ? LIVES_WINDOW(prefsw->prefs_dialog) : LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
}


void do_after_crash_warning(void) {
  do_error_dialog_with_check(_("After a crash, it is advisable to clean up the disk with\nFile|Clean up disk space\n"),
                             WARN_MASK_CLEAN_AFTER_CRASH);
}


void do_after_invalid_warning(void) {
  do_error_dialog_with_check(_("Invalid clips were detected during reaload.\nIt is advisable to clean up the disk with\n"
                               "File|Clean up disk space\n"),
                             WARN_MASK_CLEAN_INVALID);
}


void do_rmem_max_error(int size) {
  char *msg = lives_strdup_printf((
                                    _("Stream frame size is too large for your network buffers.\nYou should do the following as root:\n\n"
                                      "echo %d > /proc/sys/net/core/rmem_max\n")),
                                  size);
  do_error_dialog(msg);
  lives_free(msg);
}


void threaded_dialog_spin(double fraction) {
  double timesofar;
  int progress;

  if (mainw->splash_window != NULL) {
    return;
  }

  if (procw == NULL || !procw->is_ready || !prefs->show_gui) return;

  if (fraction > 0.) {
    timesofar = (double)(lives_get_current_ticks() - sttime) / TICKS_PER_SECOND_DBL;
    disp_fraction(fraction, timesofar, procw);
    lives_widget_context_update();
    lives_widget_process_updates(procw->processing, TRUE);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  } else {
    if (!CURRENT_CLIP_IS_VALID || cfile->progress_start == 0 || cfile->progress_end == 0 ||
        *(mainw->msg) || (progress = atoi(mainw->msg)) == 0) {
      // pulse the progress bar
      //#define GDB
#ifndef GDB
      if (LIVES_IS_PROGRESS_BAR(procw->progressbar)) {
#if GTK_CHECK_VERSION(3, 0, 0)
        lives_widget_context_update();
        lives_widget_process_updates(procw->processing, TRUE);
        lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
#endif
        lives_progress_bar_pulse(LIVES_PROGRESS_BAR(procw->progressbar));
      }
#endif
    } else {
      // show fraction
      double fraction_done = (double)(progress - cfile->progress_start) / (double)(cfile->progress_end - cfile->progress_start + 1.);
      timesofar = (double)(lives_get_current_ticks() - sttime) / TICKS_PER_SECOND_DBL;
      disp_fraction(fraction_done, timesofar, procw);
    }
  }

  if (!td_had_focus && lives_has_toplevel_focus(LIVES_MAIN_WINDOW_WIDGET)) {
    if (LIVES_IS_WIDGET(procw->processing)) {
#if GTK_CHECK_VERSION(3, 0, 0)
      lives_widget_context_update();
      lives_widget_show_all(procw->processing);
      lives_widget_queue_draw(procw->processing);
#endif
    }
    td_had_focus = TRUE;
  }
}


void do_threaded_dialog(const char *trans_text, boolean has_cancel) {
  // calling this causes a threaded progress dialog to appear
  // until end_threaded_dialog() is called
  //
  // WARNING: if trans_text is a translated string, it will be automatically freed by translations inside this function

  char *copy_text;

  if (!prefs->show_gui) return;

  if (mainw->threaded_dialog) return;

  copy_text = lives_strdup(trans_text);

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

  sttime = lives_get_current_ticks();

  mainw->threaded_dialog = TRUE;
  clear_mainw_msg();

  procw = create_threaded_dialog(copy_text, has_cancel, &td_had_focus);
  lives_free(copy_text);

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

  if (CURRENT_CLIP_IS_VALID) cfile->proc_ptr = procw;
}


void end_threaded_dialog(void) {
  mainw->cancel_type = CANCEL_KILL;

  if (procw != NULL) {
    if (procw->processing != NULL) lives_widget_destroy(procw->processing);
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);

  if (procw != NULL) {
    lives_free(procw);
    procw = NULL;
    if (CURRENT_CLIP_IS_VALID) cfile->proc_ptr = NULL;
  }

  mainw->threaded_dialog = FALSE;

  if (prefs->show_msg_area) {
    // TODO
    if (LIVES_IS_WINDOW(LIVES_MAIN_WINDOW_WIDGET)) {
      lives_window_present(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      lives_widget_grab_focus(mainw->msg_area);
      gtk_window_set_focus(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->msg_area);
    }
  }

  if (mainw->is_ready && prefs->show_gui)
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
}


void response_ok(LiVESButton * button, livespointer user_data) {
  lives_dialog_response(LIVES_DIALOG(lives_widget_get_toplevel(LIVES_WIDGET(button))), LIVES_RESPONSE_OK);
}


LiVESResponseType do_system_failed_error(const char *com, int retval, const char *addinfo, boolean can_retry,
    LiVESWindow * transient) {
  // if can_retry is set, we can return LIVES_RESPONSE_RETRY
  char *msg, *tmp, *emsg, *msgx;
  char *bit;
  char *retstr = lives_strdup_printf("%d", retval >> 8);
  char *bit2 = (retval > 255) ? lives_strdup("") : lives_strdup_printf("[%s]", lives_strerror(retval));
  char *addbit;
  char *dsmsg1 = lives_strdup("");
  char *dsmsg2 = lives_strdup("");

  uint64_t dsval1, dsval2;

  lives_storage_status_t ds1 = get_storage_status(prefs->workdir, prefs->ds_crit_level, &dsval1), ds2;
  LiVESResponseType response = LIVES_RESPONSE_NONE;

  if (mainw->current_file > -1 && cfile != NULL && cfile->op_dir != NULL) {
    ds2 = get_storage_status(cfile->op_dir, prefs->ds_crit_level, &dsval2);
    if (ds2 == LIVES_STORAGE_STATUS_CRITICAL) {
      lives_free(dsmsg2);
      tmp = ds_critical_msg(cfile->op_dir, dsval2);
      dsmsg2 = lives_strdup_printf("%s\n", tmp);
      lives_free(tmp);
    }
  }

  if (ds1 == LIVES_STORAGE_STATUS_CRITICAL) {
    lives_free(dsmsg1);
    tmp = ds_critical_msg(prefs->workdir, dsval1);
    dsmsg1 = lives_strdup_printf("%s\n", tmp);
    lives_free(tmp);
  }

  if (addinfo != NULL) addbit = lives_strdup_printf(_("Additional info: %s\n"), addinfo);
  else addbit = lives_strdup("");

  if (retval > 0) bit = lives_strdup_printf(_("The error value was %d%s\n"), retval, bit2);
  else bit = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES failed doing the following:\n%s\nPlease check your system for errors.\n%s%s%s"),
                            com, bit, addbit, dsmsg1, dsmsg2);

  emsg = lives_strdup_printf("Command failed doing\n%s\n%s%s", com, bit, addbit);
  LIVES_ERROR(emsg);
  lives_free(emsg);

  msgx = insert_newlines(msg, MAX_MSG_WIDTH_CHARS);
  if (can_retry) {
    response = do_abort_retry_dialog(msgx, transient);
  } else {
    do_error_dialog(msgx);
  }
  lives_free(msgx);
  lives_free(msg);
  lives_free(dsmsg1);
  lives_free(dsmsg2);
  lives_free(bit);
  lives_free(bit2);
  lives_free(addbit);
  lives_free(retstr);
  return response;
}


void do_write_failed_error_s(const char *s, const char *addinfo) {
  char *msg, *emsg;
  char *addbit, *tmp;
  char *dsmsg = lives_strdup("");

  char dirname[PATH_MAX];
  char *sutf = lives_filename_to_utf8(s, -1, NULL, NULL, NULL);

  boolean exists;

  uint64_t dsval;

  lives_storage_status_t ds;

  lives_snprintf(dirname, PATH_MAX, "%s", s);
  get_dirname(dirname);
  exists = lives_file_test(dirname, LIVES_FILE_TEST_EXISTS);
  ds = get_storage_status(dirname, prefs->ds_crit_level, &dsval);
  if (!exists) lives_rmdir(dirname, FALSE);

  if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
    lives_free(dsmsg);
    tmp = ds_critical_msg(dirname, dsval);
    dsmsg = lives_strdup_printf("%s\n", tmp);
    lives_free(tmp);
  }

  if (addinfo != NULL) addbit = lives_strdup_printf(_("Additional info: %s\n"), addinfo);
  else addbit = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES was unable to write to the file\n%s\nPlease check for possible error causes.\n%s"),
                            sutf, addbit, dsmsg);
  emsg = lives_strdup_printf("Unable to write to file\n%s\n%s", s, addbit);

  lives_free(sutf);

  LIVES_ERROR(emsg);
  lives_free(emsg);

  do_blocking_error_dialog(msg);
  lives_free(addbit);
  lives_free(dsmsg);
  lives_free(msg);
}


void do_read_failed_error_s(const char *s, const char *addinfo) {
  char *msg, *emsg;
  char *addbit;
  char *sutf = lives_filename_to_utf8(s, -1, NULL, NULL, NULL);

  if (addinfo != NULL) addbit = lives_strdup_printf(_("Additional info: %s\n"), addinfo);
  else addbit = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES was unable to read from the file\n%s\nPlease check for possible error causes.\n%s"),
                            sutf, addbit);
  emsg = lives_strdup_printf("Unable to read from the file\n%s\n%s", s, addbit);

  LIVES_ERROR(emsg);
  lives_free(emsg);
  lives_free(sutf);

  do_blocking_error_dialog(msg);
  lives_free(msg);
  lives_free(addbit);
}


LiVESResponseType do_write_failed_error_s_with_retry(const char *fname, const char *errtext, LiVESWindow * transient) {
  // err can be errno from open/fopen etc.

  // return same as do_abort_cancel_retry_dialog() - LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY (both non-zero)

  LiVESResponseType ret;
  char *msg, *emsg, *tmp;
  char *sutf = lives_filename_to_utf8(fname, -1, NULL, NULL, NULL);
  char *dsmsg = lives_strdup("");

  char dirname[PATH_MAX];

  boolean exists;

  uint64_t dsval;

  lives_storage_status_t ds;

  lives_snprintf(dirname, PATH_MAX, "%s", fname);
  get_dirname(dirname);
  exists = lives_file_test(dirname, LIVES_FILE_TEST_EXISTS);
  ds = get_storage_status(dirname, prefs->ds_crit_level, &dsval);
  if (!exists) lives_rmdir(dirname, FALSE);

  if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
    lives_free(dsmsg);
    tmp = ds_critical_msg(dirname, dsval);
    dsmsg = lives_strdup_printf("%s\n", tmp);
    lives_free(tmp);
  }

  if (errtext == NULL) {
    emsg = lives_strdup_printf("Unable to write to file %s", fname);
    msg = lives_strdup_printf(_("\nLiVES was unable to write to the file\n%s\nPlease check for possible error causes.\n"), sutf);
  } else {
    emsg = lives_strdup_printf("Unable to write to file %s, error was %s", fname, errtext);
    msg = lives_strdup_printf(_("\nLiVES was unable to write to the file\n%s\nThe error was\n%s.\n"), sutf, errtext);
  }

  LIVES_ERROR(emsg);
  lives_free(emsg);

  ret = do_abort_cancel_retry_dialog(msg, transient);

  lives_free(dsmsg);
  lives_free(msg);
  lives_free(sutf);

  mainw->write_failed = FALSE; // reset this

  return ret;
}


LiVESResponseType do_read_failed_error_s_with_retry(const char *fname, const char *errtext, LiVESWindow * transient) {
  // err can be errno from open/fopen etc.

  // return same as do_abort_cancel_retry_dialog() - LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY (both non-zero)

  LiVESResponseType ret;
  char *msg, *emsg;
  char *sutf = lives_filename_to_utf8(fname, -1, NULL, NULL, NULL);

  if (errtext == NULL) {
    emsg = lives_strdup_printf("Unable to read from file %s", fname);
    msg = lives_strdup_printf(_("\nLiVES was unable to read from the file\n%s\nPlease check for possible error causes.\n"), sutf);
  } else {
    emsg = lives_strdup_printf("Unable to read from file %s, error was %s", fname, errtext);
    msg = lives_strdup_printf(_("\nLiVES was unable to read from the file\n%s\nThe error was\n%s.\n"), sutf, errtext);
  }

  LIVES_ERROR(emsg);
  lives_free(emsg);

  ret = do_abort_cancel_retry_dialog(msg, transient);

  lives_free(msg);
  lives_free(sutf);

  mainw->read_failed = FALSE; // reset this

  return ret;
}


LiVESResponseType do_header_read_error_with_retry(int clip) {
  LiVESResponseType ret;
  char *hname;
  if (mainw->files[clip] == NULL) return 0;

  hname = lives_build_filename(prefs->workdir, mainw->files[clip]->handle, LIVES_CLIP_HEADER, NULL);

  ret = do_read_failed_error_s_with_retry(hname, NULL, NULL);

  lives_free(hname);
  return ret;
}


boolean do_header_write_error(int clip) {
  // returns TRUE if we manage to clear the error

  char *hname;
  LiVESResponseType retval;

  if (mainw->files[clip] == NULL) return TRUE;

  hname = lives_build_filename(prefs->workdir, mainw->files[clip]->handle, LIVES_CLIP_HEADER, NULL);
  retval = do_write_failed_error_s_with_retry(hname, NULL, NULL);
  if (retval == LIVES_RESPONSE_RETRY && save_clip_values(clip)) retval = 0; // on retry try to save all values
  lives_free(hname);

  return (!retval);
}


LiVESResponseType do_header_missing_detail_error(int clip, lives_clip_details_t detail) {
  LiVESResponseType ret;
  char *hname, *key, *msg;
  if (mainw->files[clip] == NULL) return 0;

  hname = lives_build_filename(prefs->workdir, mainw->files[clip]->handle, LIVES_CLIP_HEADER, NULL);

  key = clip_detail_to_string(detail, NULL);

  if (key == NULL) {
    msg = lives_strdup_printf("Invalid detail %d requested from file %s", detail, hname);
    LIVES_ERROR(msg);
    lives_free(msg);
    lives_free(hname);
    return 0;
  }

  msg = lives_strdup_printf(_("Value for \"%s\" could not be read."), key);
  ret = do_read_failed_error_s_with_retry(hname, msg, NULL);

  lives_free(msg);
  lives_free(key);
  lives_free(hname);
  return ret;
}


void do_chdir_failed_error(const char *dir) {
  char *msg;
  char *dutf;
  char *emsg = lives_strdup_printf("Failed directory change to\n%s", dir);
  LIVES_ERROR(emsg);
  lives_free(emsg);
  dutf = lives_filename_to_utf8(dir, -1, NULL, NULL, NULL);
  msg = lives_strdup_printf(_("\nLiVES failed to change directory to\n%s\nPlease check your system for errors.\n"), dutf);
  do_error_dialog(msg);
  lives_free(msg);
  lives_free(dutf);
}


void do_file_perm_error(const char *file_name) {
  char *msg = lives_strdup_printf(
                _("\nLiVES was unable to write to the file:\n%s\nPlease check the file permissions and try again."),
                file_name);
  do_blocking_error_dialog(msg);
  lives_free(msg);
}


void do_dir_perm_error(const char *dir_name) {
  char *msg = lives_strdup_printf(
                _("\nLiVES was unable to either create or write to the directory:\n%s\nPlease check the directory permissions and try again."),
                dir_name);
  do_blocking_error_dialog(msg);
  lives_free(msg);
}


void do_dir_perm_access_error(const char *dir_name) {
  char *msg = lives_strdup_printf(_("\nLiVES was unable to read from the directory:\n%s\n"), dir_name);
  do_blocking_error_dialog(msg);
  lives_free(msg);
}


boolean do_abort_check(void) {
  return do_yesno_dialog(_("\nAbort and exit immediately from LiVES\nAre you sure ?\n"));
}


void do_encoder_img_fmt_error(render_details * rdet) {
  char *msg = lives_strdup_printf(
                _("\nThe %s cannot encode clips with image type %s.\nPlease select another encoder from the list.\n"),
                prefs->encoder.name, get_image_ext_for_type(cfile->img_type));

  do_error_dialog_with_check_transient(msg, TRUE, 0, LIVES_WINDOW(rdet->dialog));

  lives_free(msg);
}


void do_card_in_use_error(void) {
  do_blocking_error_dialog(_("\nThis card is already in use and cannot be opened multiple times.\n"));
}


void do_dev_busy_error(const char *devstr) {
  char *msg = lives_strdup_printf(
                _("\nThe device %s is in use or unavailable.\n- Check the device permissions\n"
                  "- Check if this device is in use by another program.\n"
                  "- Check if the device actually exists.\n"),
                devstr);
  do_blocking_error_dialog(msg);
  lives_free(msg);
}


boolean do_existing_subs_warning(void) {
  return do_yesno_dialog(_("\nThis file already has subtitles loaded.\nDo you wish to overwrite the existing subtitles ?\n"));
}


void do_invalid_subs_error(void) {
  char *msg = lives_strdup(_("\nLiVES currently only supports subtitles of type"));
  char *tmp;

  char *filt[] = LIVES_SUBS_FILTER;

  register int i = 0;

  while (filt[i] != NULL) {
    if (filt[i + 1] == NULL) {
      tmp = lives_strdup_printf("%s or .%s\n", msg, filt[i] + 2);
    } else if (i > 0) {
      tmp = lives_strdup_printf("%s, .%s)", msg, filt[i] + 2);
    } else {
      tmp = lives_strdup_printf("%s .%s)", msg, filt[i] + 2);
    }
    lives_free(msg);
    msg = tmp;
    i++;
  }

  do_error_dialog(msg);
  lives_free(msg);
}


boolean do_erase_subs_warning(void) {
  return do_yesno_dialog(_("\nErase all subtitles from this clip.\nAre you sure ?\n"));
}


boolean do_sub_type_warning(const char *ext, const char *type_ext) {
  boolean ret;
  char *msg = lives_strdup_printf(
                _("\nLiVES does not recognise the subtitle file type \"%s\".\nClick Cancel to set another file name\nor OK to continue and save as type \"%s\"\n"),
                ext, type_ext);
  ret = do_warning_dialogf(msg);
  lives_free(msg);
  return ret;
}


boolean do_move_workdir_dialog(void) {
  return do_yesno_dialog(_("\nDo you wish to move the current clip sets to the new directory ?\n(If unsure, click Yes)\n"));
}


boolean do_set_locked_warning(const char *setname) {
  return do_yesno_dialogf(
           _("\nWarning - the set %s\nis in use by another copy of LiVES.\nYou are strongly advised to close the other copy before clicking Yes to continue\n.\nClick No to cancel loading the set.\n"),
           setname);
}


void do_no_in_vdevs_error(void) {
  do_error_dialog(_("\nNo video input devices could be found.\n"));
}


void do_locked_in_vdevs_error(void) {
  do_error_dialog(_("\nAll video input devices are already in use.\n"));
}


void do_do_not_close_d(void) {
  char *msg = lives_strdup(_("\n\nCLEANING AND COPYING FILES. THIS MAY TAKE SOME TIME.\nDO NOT SHUT DOWN OR CLOSE LIVES !\n"));
  create_info_error_dialog(LIVES_DIALOG_WARN, msg, NULL, 0, FALSE);
  lives_free(msg);
}


void do_bad_theme_error(const char *themefile) {
  do_error_dialogf(_("\nThe theme file %s has missing elements.\nThe theme could not be loaded correctly.\n"), themefile);
}


void do_set_noclips_error(const char *setname) {
  char *msg = lives_strdup_printf(
                _("No clips were recovered for set (%s).\nPlease check the spelling of the set name and try again.\n"),
                setname);
  d_print(msg);
  lives_free(msg);
}


char *get_upd_msg(void) {
  LIVES_DEBUG("upd msg !");
  // TRANSLATORS: make sure the menu text matches what is in gui.c
  char *msg = lives_strdup_printf(_("\nWelcome to LiVES version %s\n\nAfter upgrading, you are advised to run:"
                                    "\n\nFile -> Clean up Diskspace\n"), LiVES_VERSION);
  return msg;
}


void do_no_autolives_error(void) {
  do_error_dialogf(_("\nYou must have %s installed and in your path to use this toy.\nConsult your package distributor.\n"),
                   EXEC_AUTOLIVES_PL);
}


void do_autolives_needs_clips_error(void) {
  do_error_dialog(_("\nYou must have a minimum of one clip loaded to use this toy.\n"));
}


void do_jack_lost_conn_error(void) {
  do_error_dialog(_("\nLiVES lost its connection to jack and was unable to reconnect.\nRestarting LiVES is recommended.\n"));
}


void do_pulse_lost_conn_error(void) {
  do_error_dialog(
    _("\nLiVES lost its connection to pulseaudio and was unable to reconnect.\nRestarting LiVES is recommended.\n"));
}


void do_cd_error_dialog(void) {
  do_error_dialog(_("Please set your CD play device in Tools | Preferences | Misc\n"));
}


void do_bad_theme_import_error(const char *theme_file) {
  do_error_dialogf(_("\nLiVES was unable to import the theme file\n%s\n(Theme name not found).\n"), theme_file);
}


boolean do_theme_exists_warn(const char *themename) {
  return do_yesno_dialogf(_("\nA custom theme with the name\n%s\nalready exists. Would you like to overwrite it ?\n"), themename);
}


void add_resnn_label(LiVESDialog * dialog) {
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(dialog);
  LiVESWidget *label;
  LiVESWidget *hsep = lives_standard_hseparator_new();
  lives_box_pack_first(LIVES_BOX(dialog_vbox), hsep, FALSE, TRUE, 0);
  lives_widget_show(hsep);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new(_(
                                     "\n\nResizing of clips is no longer necessary, as LiVES will internally adjust frame sizes as "
                                     "needed at the appropriate moments.\n\n"
                                     "However, physicallly reducing the frame size may in some cases lead to improved playback \n"
                                     "and processing rates.\n\n"));
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_box_pack_first(LIVES_BOX(dialog_vbox), label, FALSE, TRUE, 0);
  lives_widget_show(label);
}


boolean ask_permission_dialog(int what) {
  if (!prefs->show_gui) return FALSE;

  switch (what) {
  case LIVES_PERM_OSC_PORTS:
    return do_yesno_dialogf(
             _("\nLiVES would like to open a local network connection (UDP port %d),\nto let other applications connect to it.\n"
               "Do you wish to allow this (for this session only) ?\n"),
             prefs->osc_udp_port);
  default:
    break;
  }

  return FALSE;
}


boolean do_layout_recover_dialog(void) {
  if (!do_yesno_dialog(
        _("\nLiVES has detected a multitrack layout from a previous session.\nWould you like to try and recover it ?\n"))) {
    recover_layout_cancelled(TRUE);
    return FALSE;
  } else return recover_layout();
}

