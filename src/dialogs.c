// dialogs.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

/// TODO: split into player, progress, dialogs

#include "main.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "cvirtual.h"
#include "resample.h"
#include "rte_window.h"
#include "paramwindow.h"
#include "ce_thumbs.h"
#include "callbacks.h"
#include "effects-weed.h"
#include "diagnostics.h"

extern void reset_frame_and_clip_index(void);

static void *extra_cb_data = NULL;
static int extra_cb_key = 0;
static int del_cb_key = 0;

// how often to we count frames when opening
#define OPEN_CHECK_TICKS (TICKS_PER_SECOND/10l)

static volatile boolean display_ready;
static double disp_fraction_done;
static ticks_t proc_start_ticks;
static boolean shown_paused_frames;
static ticks_t last_open_check_ticks;
static double audio_start;
static boolean td_had_focus;
static int64_t sttime;


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
  char *tmp = (_("Show affected _layouts"));
  add_list_expander(box, tmp, ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V, mainw->xlays);
  lives_free(tmp);
}


void add_warn_check(LiVESBox *box, int warn_mask_number) {
  LiVESWidget *checkbutton = lives_standard_check_button_new(
                               _("Do _not show this warning any more\n(can be turned back on from Preferences/Warnings)"),
                               FALSE, LIVES_BOX(box), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_warn_mask_toggled), LIVES_INT_TO_POINTER(warn_mask_number));
}


static void add_clear_ds_button(LiVESDialog *dialog) {
  LiVESWidget *button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CLEAR, _("_Recover disk space"),
                        LIVES_RESPONSE_RETRY);
  if (mainw->tried_ds_recover) lives_widget_set_sensitive(button, FALSE);
  lives_dialog_make_widget_first(LIVES_DIALOG(dialog), button);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_cleardisk_activate), (livespointer)button);
}


static void add_clear_ds_adv(LiVESBox *box) {
  // add a button which opens up  Recover/Repair widget
  LiVESWidget *button = lives_standard_button_new_with_label(_(" _Advanced Settings >>"),
                        DEF_BUTTON_WIDTH * 2,
                        DEF_BUTTON_HEIGHT);
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(hbox), button, FALSE, FALSE, widget_opts.packing_width * 2);
  lives_box_pack_start(box, hbox, FALSE, FALSE, widget_opts.packing_height);
  add_fill_to_box(LIVES_BOX(box));

  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_cleardisk_advanced_clicked), NULL);
}


static void add_perminfo(LiVESWidget *dialog) {
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0), *vbox;
  LiVESWidget *label;
  char *txt;

  lives_box_pack_end(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, 0);
  vbox = lives_vbox_new(FALSE, 0);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  lives_standard_expander_new(_("_Show complete details"), LIVES_BOX(hbox), vbox);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_widget_apply_theme(hbox, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_apply_theme(vbox, LIVES_WIDGET_STATE_NORMAL);

  if (mainw->permmgr->cmdlist) {
    txt = lives_strdup_printf(_("Should you agree, the following commands will be run:\n%s"),
                              mainw->permmgr->cmdlist);
    label = lives_standard_label_new(txt);
    lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, TRUE, widget_opts.packing_height);
    add_fill_to_box(LIVES_BOX(vbox));

    lives_free(txt);
  }

  if (mainw->permmgr->futures) {
    txt = lives_strdup_printf(_("After this you will need to update using:\n%s\n\n"
                                "Please make a note of this.\n"),
                              mainw->permmgr->futures);
    label = lives_standard_label_new(txt);
    lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, TRUE, widget_opts.packing_height);
    lives_free(txt);
  }
}


static void scan_for_sets(LiVESWidget *button, livespointer data) {
  LiVESWidget *entry = (LiVESWidget *)data;
  LiVESWidget *label =
    (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), "disp_label");
  LiVESList *list = NULL;
  char *txt;
  const char *dir = lives_entry_get_text(LIVES_ENTRY(entry));
  if (dir) list = get_set_list(dir, TRUE);
  txt = lives_strdup_printf("%d", lives_list_length(list));
  if (list) lives_list_free_all(&list);
  lives_label_set_text(LIVES_LABEL(label), txt);
  lives_free(txt);
}


static void extra_cb(LiVESWidget *dialog, int key) {
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *bbox = lives_dialog_get_action_area(LIVES_DIALOG(dialog));
  LiVESWidget *layout, *button, *entry, *hbox, *label;
  char *tmp;
  switch (key) {
  case 1:
    /// no sets found
    lives_label_chomp(LIVES_LABEL(widget_opts.last_label));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, TRUE, 0);

    entry = lives_standard_direntry_new(NULL, prefs->workdir, MEDIUM_ENTRY_WIDTH, PATH_MAX,
                                        LIVES_BOX(hbox), NULL);

    layout = lives_layout_new(LIVES_BOX(dialog_vbox));
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Sets detected: "), TRUE);
    label = lives_layout_add_label(LIVES_LAYOUT(layout), _("0"), TRUE);

    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

    button =
      lives_standard_button_new_from_stock_full(LIVES_STOCK_REFRESH,
          _("Scan other directory"), DEF_BUTTON_WIDTH,
          DEF_BUTTON_HEIGHT, LIVES_BOX(hbox), TRUE,
          (tmp = lives_strdup(H_("Scan other directories for "
                                 "LiVES Clip Sets. May be slow "
                                 "for some directories."))));
    lives_free(tmp);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), "disp_label", label);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(scan_for_sets), entry);

    layout = lives_layout_new(LIVES_BOX(dialog_vbox));
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_EXTRA_WIDTH;
    lives_layout_add_label(LIVES_LAYOUT(layout), _("If you believe there should be clips in the "
                           "current directory,\n"
                           "you can try to recover them by launching\n"
                           " 'Clean up Diskspace / Recover Missing Clips' "
                           "from the File menu.\n"), FALSE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    lives_label_set_selectable(LIVES_LABEL(widget_opts.last_label), TRUE);
    break;
  case 2:
    trash_rb(LIVES_BUTTON_BOX(bbox));
    break;
  default:
    if (extra_cb_data)
      lives_box_pack_start(LIVES_BOX(dialog_vbox), (LiVESWidget *)extra_cb_data, FALSE, TRUE, 0);
    break;
  }
}


static void del_event_cb(LiVESWidget *dialog, livespointer data) {
  int key = LIVES_POINTER_TO_INT(data);
  switch (key) {
  default: break;
  }
}

//Warning or yes/no dialog

// the type of message box here is with 2 or more buttons (e.g. OK/CANCEL, YES/NO, ABORT/CANCEL/RETRY)
// if a single OK button is needed, use create_message_dialog() in interface.c instead

LiVESWidget *create_message_dialog(lives_dialog_t diat, const char *text, int warn_mask_number) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *label;
  LiVESWidget *cancelbutton = NULL;
  LiVESWidget *okbutton = NULL, *defbutton = NULL;
  LiVESWidget *abortbutton = NULL;

  LiVESAccelGroup *accel_group = NULL;
  LiVESWindow *transient = widget_opts.transient;

  char *colref;

  int cb_key = extra_cb_key;
  int del_key = del_cb_key;
  extra_cb_key = 0;
  del_cb_key = 0;

  if (!transient) transient = get_transient_full();

  switch (diat) {
  case LIVES_DIALOG_WARN:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0,
                                      LIVES_MESSAGE_WARNING, LIVES_BUTTONS_NONE, NULL);

    lives_window_set_title(LIVES_WINDOW(dialog), _("Warning !"));

    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->dark_orange);

    defbutton = okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                           LIVES_RESPONSE_OK);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);

    break;
  case LIVES_DIALOG_ERROR:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0,
                                      LIVES_MESSAGE_ERROR, LIVES_BUTTONS_NONE, NULL);
    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);

    lives_window_set_title(LIVES_WINDOW(dialog), _("Error !"));

    defbutton = okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                           LIVES_RESPONSE_OK);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);
    break;
  case LIVES_DIALOG_INFO:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0,
                                      LIVES_MESSAGE_INFO, LIVES_BUTTONS_NONE, NULL);

    lives_window_set_title(LIVES_WINDOW(dialog), _("Information"));

    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->light_green);

    defbutton = okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                           LIVES_RESPONSE_OK);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);
    break;

  case LIVES_DIALOG_WARN_WITH_CANCEL:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_WARNING,
                                      LIVES_BUTTONS_NONE, NULL);

    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->dark_orange);

    if (mainw && mainw->add_clear_ds_button) {
      mainw->add_clear_ds_button = FALSE;
      add_clear_ds_button(LIVES_DIALOG(dialog));
    }

    lives_window_set_title(LIVES_WINDOW(dialog), _("Warning !"));

    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->dark_orange);

    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                   LIVES_RESPONSE_CANCEL);

    defbutton = okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                           LIVES_RESPONSE_OK);
    break;

  case LIVES_DIALOG_YESNO:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_QUESTION,
                                      LIVES_BUTTONS_NONE, NULL);

    lives_window_set_title(LIVES_WINDOW(dialog), _("Question"));

    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);

    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_NO, NULL,
                   LIVES_RESPONSE_NO);

    defbutton = okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_YES, NULL,
                           LIVES_RESPONSE_YES);
    break;

  case LIVES_DIALOG_QUESTION:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_QUESTION,
                                      LIVES_BUTTONS_NONE, NULL);
    // caller will set title and buttons
    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);
    break;

  case LIVES_DIALOG_ABORT_CANCEL_RETRY:
  case LIVES_DIALOG_RETRY_CANCEL:
  case LIVES_DIALOG_ABORT_RETRY:
  case LIVES_DIALOG_ABORT_OK:
  case LIVES_DIALOG_ABORT:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_ERROR, LIVES_BUTTONS_NONE, NULL);

    if (diat != LIVES_DIALOG_RETRY_CANCEL) {
      abortbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
                    LIVES_STOCK_QUIT, _("_Abort"), LIVES_RESPONSE_ABORT);

      if (diat == LIVES_DIALOG_ABORT_CANCEL_RETRY) {
        lives_window_set_title(LIVES_WINDOW(dialog), _("File Error"));
        cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                       LIVES_RESPONSE_CANCEL);
        okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH,
                   _("_Retry"), LIVES_RESPONSE_RETRY);
      }
      if (diat == LIVES_DIALOG_ABORT_OK) {
        okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                   LIVES_RESPONSE_OK);
      }
      if (diat == LIVES_DIALOG_RETRY_CANCEL || diat == LIVES_DIALOG_ABORT_RETRY)
        okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH,
                   _("_Retry"), LIVES_RESPONSE_RETRY);
    }
    if (diat == LIVES_DIALOG_RETRY_CANCEL) {
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                     LIVES_RESPONSE_CANCEL);
    }
    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
    break;

  case LIVES_DIALOG_SKIP_RETRY_BROWSE:
  case LIVES_DIALOG_CANCEL_RETRY_BROWSE:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_ERROR,
                                      LIVES_BUTTONS_NONE, NULL);

    lives_window_set_title(LIVES_WINDOW(dialog), _("Missing File or Directory"));

    if (diat == LIVES_DIALOG_CANCEL_RETRY_BROWSE)
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                     LIVES_RESPONSE_CANCEL);

    if (diat == LIVES_DIALOG_SKIP_RETRY_BROWSE)
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL,
                     LIVES_STOCK_LABEL_SKIP, LIVES_RESPONSE_CANCEL);

    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH,
               _("_Retry"), LIVES_RESPONSE_RETRY);

    abortbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_QUIT,
                  _("_Browse"), LIVES_RESPONSE_BROWSE);

    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
    break;

  default:
    cancelbutton = abortbutton; // stop compiler complaining
    return NULL;
  }

  if (del_key)
    lives_signal_sync_connect(LIVES_GUI_OBJECT(dialog), LIVES_WIDGET_DESTROY_SIGNAL,
                              LIVES_GUI_CALLBACK(del_event_cb), LIVES_INT_TO_POINTER(del_key));

  lives_window_set_default_size(LIVES_WINDOW(dialog), MIN_MSGBOX_WIDTH, -1);
  lives_widget_set_minimum_size(dialog, MIN_MSGBOX_WIDTH, -1);

  lives_window_set_deletable(LIVES_WINDOW(dialog), FALSE);
  lives_window_set_resizable(LIVES_WINDOW(dialog), FALSE);

  if (mainw && mainw->mgeom)
    lives_window_set_monitor(LIVES_WINDOW(dialog), widget_opts.monitor);

  if (widget_opts.apply_theme) {
    lives_dialog_set_has_separator(LIVES_DIALOG(dialog), FALSE);
    if (palette)
      lives_widget_set_bg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  if (widget_opts.apply_theme) {
    funkify_dialog(dialog);
  } else {
    lives_container_set_border_width(LIVES_CONTAINER(dialog), widget_opts.border_width * 2);
  }

  label = lives_standard_formatted_label_new(text);
  widget_opts.last_label = label;

  if (palette) {
    colref = gdk_rgba_to_string(&palette->normal_back);
    set_css_value_direct(label, LIVES_WIDGET_STATE_NORMAL, "",
                         "caret-color", colref);
    lives_free(colref);
  }
  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);
  lives_label_set_selectable(LIVES_LABEL(label), TRUE);

  if (mainw && mainw->permmgr && (mainw->permmgr->cmdlist || mainw->permmgr->futures))
    add_perminfo(dialog);

  if (mainw) {
    if (mainw->add_clear_ds_adv) {
      mainw->add_clear_ds_adv = FALSE;
      add_clear_ds_adv(LIVES_BOX(dialog_vbox));
    }

    if (warn_mask_number > 0) {
      add_warn_check(LIVES_BOX(dialog_vbox), warn_mask_number);
    }

    if (mainw->xlays) {
      add_xlays_widget(LIVES_BOX(dialog_vbox));
    }

    if (mainw->iochan && !widget_opts.non_modal) {
      LiVESWidget *details_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), NULL, _("Show _Details"),
                                    LIVES_RESPONSE_SHOW_DETAILS);

      lives_signal_sync_connect(LIVES_GUI_OBJECT(details_button), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);
    }
  }

  if (okbutton || cancelbutton) {
    accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
    lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);
  }

  if (cancelbutton) {
    lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                                 LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);
  }

  if (okbutton && mainw && mainw->iochan) {
    lives_button_grab_default_special(okbutton);
    lives_widget_grab_focus(okbutton);
  } else if (defbutton) lives_button_grab_default_special(defbutton);

  lives_widget_show_all(dialog);
  //gdk_window_show_unraised(lives_widget_get_xwindow(dialog));

  if (mainw->mgeom)
    lives_window_center(LIVES_WINDOW(dialog));

  if (!widget_opts.non_modal)
    lives_window_set_modal(LIVES_WINDOW(dialog), TRUE);

  if (!transient) {
    char *wid =
      lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));

    /// MUST check if execs are MISSING, else we can get stuck in a loop of warning dialogs !!!
    if (!wid || (capable->has_xdotool == MISSING && capable->has_wmctrl == MISSING)
        || !activate_x11_window(wid) || 1)
      lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);
  }
  if (cb_key) extra_cb(dialog, cb_key);

  if (mainw && mainw->add_trash_rb)
    trash_rb(LIVES_BUTTON_BOX(lives_dialog_get_action_area(LIVES_DIALOG(dialog))));

  return dialog;
}


LIVES_GLOBAL_INLINE LiVESWidget *create_question_dialog(const char *title, const char *text) {
  LiVESWidget *dialog;
  char *xtitle = (char *)title;
  if (!xtitle) xtitle = _("Question");
  dialog = create_message_dialog(LIVES_DIALOG_QUESTION, text, 0);
  lives_window_set_title(LIVES_WINDOW(dialog), xtitle);
  if (xtitle != title) lives_free(xtitle);
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


LIVES_GLOBAL_INLINE boolean do_warning_dialog(const char *text) {
  return do_warning_dialog_with_check(text, 0);
}


boolean do_warning_dialog_with_check(const char *text, uint64_t warn_mask_number) {
  // show OK/CANCEL, returns FALSE if cancelled
  LiVESWidget *warning;
  int response = 1;
  char *mytext;

  if (warn_mask_number >= (1ull << 48)) {
    if (!(prefs->warning_mask & warn_mask_number)) return TRUE;
  } else {
    if (prefs->warning_mask & warn_mask_number) return TRUE;
  }

  mytext = lives_strdup(text); // must copy this because of translation issues

  warning = create_message_dialog(LIVES_DIALOG_WARN_WITH_CANCEL, mytext, warn_mask_number);

  response = lives_dialog_run(LIVES_DIALOG(warning));
  lives_widget_destroy(warning);

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  lives_freep((void **)&mytext);

  return (response == LIVES_RESPONSE_OK);
}


boolean do_yesno_dialog_with_check(const char *text, uint64_t warn_mask_number) {
  // show YES/NO, returns TRUE for YES
  LiVESWidget *warning;
  int response = 1;
  char *mytext;

  if (warn_mask_number >= (1ull << 48)) {
    if (!(prefs->warning_mask & warn_mask_number)) return TRUE;
  } else {
    if (prefs->warning_mask & warn_mask_number) return TRUE;
  }

  mytext = lives_strdup(text); // must copy this because of translation issues

  do {
    warning = create_message_dialog(LIVES_DIALOG_YESNO, mytext, warn_mask_number);
    response = lives_dialog_run(LIVES_DIALOG(warning));
    lives_widget_destroy(warning);
  } while (response == LIVES_RESPONSE_RETRY);

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  lives_freep((void **)&mytext);

  return (response == LIVES_RESPONSE_YES);
}


LIVES_GLOBAL_INLINE LiVESWindow *get_transient_full(void) {
  LiVESWindow *transient = NULL;
  if (!prefs) return NULL;
  if (prefs->show_gui) {
    if (rdet && rdet->dialog) transient = LIVES_WINDOW(rdet->dialog);
    else if (prefsw && prefsw->prefs_dialog) transient = LIVES_WINDOW(prefsw->prefs_dialog);
    else if (!rte_window_hidden()) transient = LIVES_WINDOW(rte_window);
    else if (LIVES_MAIN_WINDOW_WIDGET && mainw->is_ready) transient = LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET);
  }
  return transient;
}


boolean do_yesno_dialogf_with_countdown(int nclicks, const char *fmt, ...) {
  // show Yes/No, returns TRUE if Yes is clicked nclicks times
  LiVESWidget *warning, *yesbutton;
  int response = LIVES_RESPONSE_YES;
  va_list xargs;
  char *textx;
  const char *btext;

  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  warning = create_message_dialog(LIVES_DIALOG_YESNO, textx, 0);
  lives_free(textx);
  yesbutton = lives_dialog_get_widget_for_response(LIVES_DIALOG(warning), LIVES_RESPONSE_YES);
  btext = lives_strdup(lives_standard_button_get_label(LIVES_BUTTON(yesbutton)));
  for (; nclicks > 0; nclicks--) {
    char *tmp = lives_strdup_printf(P_("%s\n(Click %d more time)", "%s\n(Click %d more times)", nclicks),
                                    btext, nclicks);
    lives_standard_button_set_label(LIVES_BUTTON(yesbutton), tmp);
    lives_free(tmp);
    response = lives_dialog_run(LIVES_DIALOG(warning));
    if (response == LIVES_RESPONSE_NO) break;
  }

  lives_widget_destroy(warning);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  return (response == LIVES_RESPONSE_YES);
}


boolean do_yesno_dialogf(const char *fmt, ...) {
  // show Yes/No, returns TRUE if Yes
  LiVESWidget *warning;
  int response;
  va_list xargs;
  char *textx;

  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  warning = create_message_dialog(LIVES_DIALOG_YESNO, textx, 0);
  lives_free(textx);
  response = lives_dialog_run(LIVES_DIALOG(warning));
  lives_widget_destroy(warning);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  return (response == LIVES_RESPONSE_YES);
}


boolean do_yesno_dialog(const char *text) {
  // show Yes/No, returns TRUE if Yes
  LiVESWidget *warning;
  int response;

  warning = create_message_dialog(LIVES_DIALOG_YESNO, text, 0);
  lives_widget_show_all(warning);
  response = lives_dialog_run(LIVES_DIALOG(warning));
  lives_widget_destroy(warning);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  return (response == LIVES_RESPONSE_YES);
}


void maybe_abort(boolean do_check) {
  if (!do_check || do_abort_check()) {
    if (CURRENT_CLIP_IS_VALID) {
      if (*cfile->handle) {
        // stop any processing
        lives_kill_subprocesses(cfile->handle, TRUE);
      }
    }
    if (mainw->abort_hook_func)(*mainw->abort_hook_func)(NULL);
    LIVES_FATAL("Aborted");
    lives_notify(LIVES_OSC_NOTIFY_QUIT, "Aborted");
    exit(1);
  }
}


static LiVESResponseType _do_abort_cancel_retry_dialog(const char *mytext, lives_dialog_t dtype) {
  LiVESResponseType response;
  LiVESWidget *warning;

  do {
    warning = create_message_dialog(dtype, mytext, 0);
    lives_widget_show_all(warning);
    response = lives_dialog_run(LIVES_DIALOG(warning)); // looping on retry
    lives_widget_destroy(warning);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

    if (response == LIVES_RESPONSE_ABORT) {
      maybe_abort(mainw->is_ready && dtype != LIVES_DIALOG_ABORT);
    }
  } while (response == LIVES_RESPONSE_ABORT);

  return response;
}


// returns LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY
LIVES_GLOBAL_INLINE LiVESResponseType do_abort_cancel_retry_dialog(const char *text) {
  return _do_abort_cancel_retry_dialog(text, LIVES_DIALOG_ABORT_CANCEL_RETRY);
}


// always returns LIVES_RESPONSE_RETRY
LIVES_GLOBAL_INLINE LiVESResponseType do_abort_retry_dialog(const char *text) {
  return _do_abort_cancel_retry_dialog(text, LIVES_DIALOG_ABORT_RETRY);
}


// always returns LIVES_RESPONSE_OK
LIVES_GLOBAL_INLINE LiVESResponseType do_abort_ok_dialog(const char *text) {
  return _do_abort_cancel_retry_dialog(text, LIVES_DIALOG_ABORT_OK);
}

// does not return
LIVES_GLOBAL_INLINE void do_abort_dialog(const char *text) {
  _do_abort_cancel_retry_dialog(text, LIVES_DIALOG_ABORT);
}


LIVES_GLOBAL_INLINE LiVESResponseType do_retry_cancel_dialog(const char *text) {
  return _do_abort_cancel_retry_dialog(text, LIVES_DIALOG_RETRY_CANCEL);
}


LiVESResponseType do_error_dialogf(const char *fmt, ...) {
  // show error box
  LiVESResponseType resi;
  char *textx;
  va_list xargs;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  resi = do_error_dialog_with_check(textx, 0);
  lives_free(textx);
  return resi;
}


LIVES_GLOBAL_INLINE LiVESResponseType do_error_dialog(const char *text) {
  // show error box
  return do_error_dialog_with_check(text, 0);
}


static LiVESResponseType _do_info_dialog(const char *text, const char *exp_title, LiVESList *exp_list) {
  LiVESResponseType ret = LIVES_RESPONSE_NONE;
  LiVESWidget *info_box = create_message_dialog(LIVES_DIALOG_INFO, text, 0);

  if (exp_list) {
    LiVESWidget *dab = lives_dialog_get_content_area(LIVES_DIALOG(info_box));
    add_list_expander(LIVES_BOX(dab), exp_title, ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V, exp_list);
    lives_widget_show_all(info_box);
  }

  if (!widget_opts.non_modal) {
    ret = lives_dialog_run(LIVES_DIALOG(info_box));
  }

  return ret;
}


LiVESResponseType do_info_dialogf(const char *fmt, ...) {
  // show info box
  LiVESResponseType resi;
  char *textx;
  va_list xargs;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  resi = _do_info_dialog(textx, NULL, NULL);
  lives_free(textx);
  return resi;
}


LIVES_GLOBAL_INLINE LiVESResponseType do_info_dialog(const char *text) {
  // show info box - blocks until OK is pressed
  return _do_info_dialog(text, NULL, NULL);
}


LIVES_GLOBAL_INLINE LiVESResponseType do_info_dialog_with_expander(const char *text,
    const char *exp_text, LiVESList *list) {
  // show info box - blocks until OK is pressed
  return _do_info_dialog(text, exp_text, list);
}


LiVESResponseType do_error_dialog_with_check(const char *text, uint64_t warn_mask_number) {
  // show error box
  LiVESWidget *err_box;
  LiVESResponseType ret = LIVES_RESPONSE_NONE;

  if (warn_mask_number >= (1ull << 48)) {
    if (!(prefs->warning_mask & warn_mask_number)) return TRUE;
  } else {
    if (prefs->warning_mask & warn_mask_number) return TRUE;
  }

  err_box = create_message_dialog(warn_mask_number == 0 ? LIVES_DIALOG_ERROR :
                                  LIVES_DIALOG_WARN, text, warn_mask_number);

  if (!widget_opts.non_modal) {
    ret = lives_dialog_run(LIVES_DIALOG(err_box));
  }

  return ret;
}


char *ds_critical_msg(const char *dir, char **mountpoint, uint64_t dsval) {
  char *msg, *msgx, *tmp, *tmp2, *mp, *mpstr;
  char *dscr = lives_format_storage_space_string(prefs->ds_crit_level); ///< crit level
  char *dscu = lives_format_storage_space_string(dsval); ///< current level
  if (!mountpoint || !*mountpoint) mp = get_mountpoint_for(dir);
  else mp = *mountpoint;
  if (mp) {
    tmp = lives_markup_escape_text(mp, -1);
    mpstr = lives_strdup_printf("(%s)\n", tmp);
    lives_free(tmp);
  } else mpstr = lives_strdup("");
  tmp = lives_filename_to_utf8(dir, -1, NULL, NULL, NULL);
  tmp2 = lives_markup_escape_text(tmp, -1);
  lives_free(tmp);
  msg = lives_strdup_printf(
          _("<b>FREE SPACE IN THE PARTITION CONTAINING\n%s\n%sHAS FALLEN BELOW THE CRITICAL LEVEL OF %s\n"
            "CURRENT FREE SPACE IS %s\n</b>\n\n(Disk warning levels can be configured in Preferences / Warnings.)"),
          tmp2, mpstr, dscr, dscu);
  msgx = insert_newlines(msg, MAX_MSG_WIDTH_CHARS);
  lives_free(msg); lives_free(tmp2); lives_free(dscr);
  lives_free(dscu); lives_free(mpstr);
  if (mountpoint) {
    if (!*mountpoint) *mountpoint = mp;
  } else if (mp) lives_free(mp);
  return msgx;
}


char *ds_warning_msg(const char *dir, char **mountpoint, uint64_t dsval, uint64_t cwarn, uint64_t nwarn) {
  char *msg, *msgx, *tmp, *mp, *mpstr;
  char *dscw = lives_format_storage_space_string(cwarn); ///< warn level
  char *dscu = lives_format_storage_space_string(dsval); ///< current level
  char *dscn = lives_format_storage_space_string(nwarn); ///< next warn level
  if (!mountpoint || !*mountpoint) mp = get_mountpoint_for(dir);
  else mp = *mountpoint;
  if (mp) mpstr = lives_strdup_printf("(%s)\n", mp);
  else mpstr = lives_strdup("");
  msg = lives_strdup_printf(
          _("Free space in the partition containing\n%s\nhas fallen below the warning level of %s\nCurrent free space is %s\n\n"
            "(Next warning will be shown at %s. Disk warning levels can be configured in Preferences / Warnings.)"),
          (tmp = lives_filename_to_utf8(dir, -1, NULL, NULL, NULL)), mpstr, dscw, dscu, dscn);
  msgx = insert_newlines(msg, MAX_MSG_WIDTH_CHARS);
  lives_free(msg); lives_free(dscw); lives_free(mpstr);
  lives_free(dscu); lives_free(dscn);
  if (mountpoint) {
    if (!*mountpoint) *mountpoint = mp;
  } else if (mp) lives_free(mp);
  return msgx;
}


LIVES_GLOBAL_INLINE void do_abortblank_error(const char *what) {
  char *msg = lives_strdup_printf(_("%s may not be blank.\nClick Abort to exit LiVES immediately or Ok "
                                    "to continue with the default value."), what);
  do_abort_ok_dialog(msg);
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
  do_abort_ok_dialog(msg);
  lives_free(msg);
  check_storage_space(fileno, FALSE);
}


LIVES_GLOBAL_INLINE void do_aud_during_play_error(void) {
  do_error_dialog(_("Audio players cannot be switched during playback."));
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
  response = do_abort_cancel_retry_dialog(msg);
  lives_free(msg);
  return response;
}


/// TODO - move the follwoing fns into processing.c

LiVESResponseType handle_backend_errors(boolean can_retry) {
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
    THREADVAR(read_failed) = TRUE;
    THREADVAR(read_failed_file) = lives_strdup(array[2]);
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
    THREADVAR(write_failed) = TRUE;
    THREADVAR(write_failed_file) = lives_strdup(array[2]);
    mainw->cancelled = CANCEL_ERROR;
    response = LIVES_RESPONSE_CANCEL;
  }

  else if (numtok > 3 && !strcmp(array[1], "system")) {
    /// got (sub) system error from backend
    if (numtok > 4 && *(array[4])) addinfo = array[4];
    else addinfo = NULL;
    if (!CURRENT_CLIP_IS_VALID || !cfile->no_proc_sys_errors) {
      if (numtok > 5 && strstr(addinfo, "_ASKPERM_")) {
        /// sys error is possibly recoverable, but requires user PERMS
        /// ask for them and then return either LIVES_RESPONSE_CANCEL
        /// or LIVES_RESPONSE_ACCEPT, as well as setting mainw->perm_idx and mainw->perm_key
        if (lives_ask_permission(array, numtok, 5)) response = LIVES_RESPONSE_ACCEPT;
        else {
          response = LIVES_RESPONSE_CANCEL;
          mainw->cancelled = CANCEL_USER;
          goto handled;
        }
      } else {
        boolean trysudo = FALSE;
        if (addinfo && strstr(addinfo, "_TRY_SUDO_")) trysudo = TRUE;
        response = do_system_failed_error(array[2], atoi(array[3]), addinfo,
                                          can_retry, trysudo);
        if (response == LIVES_RESPONSE_RETRY) return response;
      }
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

handled:
  lives_strfreev(array);

  mainw->error = TRUE;
  return response;
}


boolean check_backend_return(lives_clip_t *sfile) {
  // check return code after synchronous (foreground) backend commands
  lives_fread_string(mainw->msg, MAINW_MSG_SIZE, sfile->info_file);

  // TODO: consider permitting retry here
  if (!strncmp(mainw->msg, "error", 5)) handle_backend_errors(FALSE);

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
  if (gerr) lives_error_free(gerr);
#endif

#ifdef GUI_QT
  QByteArray qba = iochan->readAll();
  str_return = strdup(qba.constData());
  retlen = strlen(str_return);
#endif
  // check each line of str_return, if it contains ptext, (whitespace), then number get the number and set percentage

  if (retlen > 0 && mainw->proc_ptr) {
    double max;
    LiVESAdjustment *adj = lives_scrolled_window_get_vadjustment(LIVES_SCROLLED_WINDOW(((xprocess *)(
                             mainw->proc_ptr))->scrolledwindow));
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
            if (ispct) mainw->proc_ptr->frames_done = (int)(lives_strtod(cptr)
                  * (mainw->proc_ptr->progress_end - mainw->proc_ptr->progress_start + 1.) / 100.);
            else mainw->proc_ptr->frames_done = atoi(cptr);
          }
          break;
        }
        cptr++;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*

  lives_freep((void **)&str_return);
}


boolean check_storage_space(int clipno, boolean is_processing) {
  // check storage space in prefs->workdir
  lives_clip_t *sfile = NULL;

  int64_t dsval = -1;

  lives_storage_status_t ds;
  int retval;
  boolean did_pause = FALSE;

  char *msg, *tmp;
  char *pausstr = (_("Processing has been paused."));

  if (IS_VALID_CLIP(clipno)) sfile = mainw->files[clipno];

  do {
    if (mainw->dsu_valid && capable->ds_used > -1) {
      dsval = capable->ds_used;
    } else if (prefs->disk_quota) {
      dsval = disk_monitor_check_result(prefs->workdir);
      if (dsval >= 0) capable->ds_used = dsval;
    }
    ds = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsval, 0);
    capable->ds_free = dsval;
    if (ds == LIVES_STORAGE_STATUS_WARNING) {
      uint64_t curr_ds_warn = mainw->next_ds_warn_level;
      mainw->next_ds_warn_level >>= 1;
      if (mainw->next_ds_warn_level > (dsval >> 1)) mainw->next_ds_warn_level = dsval >> 1;
      if (mainw->next_ds_warn_level < prefs->ds_crit_level) mainw->next_ds_warn_level = prefs->ds_crit_level;
      if (is_processing && sfile && mainw->proc_ptr && !mainw->effects_paused &&
          lives_widget_is_visible(mainw->proc_ptr->pause_button)) {
        on_effects_paused(LIVES_BUTTON(mainw->proc_ptr->pause_button), NULL);
        did_pause = TRUE;
      }

      tmp = ds_warning_msg(prefs->workdir, &capable->mountpoint, dsval, curr_ds_warn, mainw->next_ds_warn_level);
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
          if (sfile) sfile->nokeep = TRUE;
          on_cancel_keep_button_clicked(NULL, NULL); // press the cancel button
        }
        return FALSE;
      }
      lives_free(msg);
    } else if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
      if (is_processing && sfile && mainw->proc_ptr && !mainw->effects_paused &&
          lives_widget_is_visible(mainw->proc_ptr->pause_button)) {
        on_effects_paused(LIVES_BUTTON(mainw->proc_ptr->pause_button), NULL);
        did_pause = TRUE;
      }
      tmp = ds_critical_msg(prefs->workdir, &capable->mountpoint, dsval);
      if (!did_pause)
        msg = lives_strdup_printf("\n%s\n", tmp);
      else {
        char *xpausstr = lives_markup_escape_text(pausstr, -1);
        msg = lives_strdup_printf("\n%s\n%s\n", tmp, xpausstr);
        lives_free(xpausstr);
      }
      lives_free(tmp);
      widget_opts.use_markup = TRUE;
      retval = do_abort_cancel_retry_dialog(msg);
      widget_opts.use_markup = FALSE;
      lives_free(msg);
      if (retval == LIVES_RESPONSE_CANCEL) {
        if (is_processing) {
          if (sfile) sfile->nokeep = TRUE;
          on_cancel_keep_button_clicked(NULL, NULL); // press the cancel button
        }
        mainw->cancelled = CANCEL_ERROR;
        lives_free(pausstr);
        return FALSE;
      }
    }
  } while (ds == LIVES_STORAGE_STATUS_CRITICAL);

  if (ds == LIVES_STORAGE_STATUS_OVER_QUOTA && !mainw->is_processing) {
    run_diskspace_dialog();
  }

  if (did_pause && mainw->effects_paused) {
    on_effects_paused(LIVES_BUTTON(mainw->proc_ptr->pause_button), NULL);
  }

  lives_free(pausstr);

  return TRUE;
}


static boolean accelerators_swapped;

void cancel_process(boolean visible) {
  if (visible) {
    if (mainw->preview_box && !mainw->preview) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Play all"));
    if (accelerators_swapped) {
      if (!mainw->preview) lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play all"));
      lives_widget_remove_accelerator(mainw->proc_ptr->preview_button, mainw->accel_group, LIVES_KEY_p, (LiVESXModifierType)0);
      lives_widget_add_accelerator(mainw->playall, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group, LIVES_KEY_p,
                                   (LiVESXModifierType)0,
                                   LIVES_ACCEL_VISIBLE);
    }
    if (mainw->proc_ptr) {
      lives_freep((void **)&mainw->proc_ptr->text);
      lives_widget_destroy(LIVES_WIDGET(mainw->proc_ptr->processing));
      lives_free(mainw->proc_ptr);
      mainw->proc_ptr = NULL;
    }
    mainw->is_processing = FALSE;
    if (!(cfile->menuentry == NULL)) {
      sensitize();
    }
  } else {
    /// ????
    mainw->is_processing = TRUE;
  }
  if (visible && mainw->current_file > -1 && cfile->clip_type == CLIP_TYPE_DISK && ((mainw->cancelled != CANCEL_NO_MORE_PREVIEW
      && mainw->cancelled != CANCEL_PREVIEW_FINISHED && mainw->cancelled != CANCEL_USER) || !cfile->opening)) {
    lives_rm(cfile->info_file);
  }
}


static char *remtime_string(double timerem) {
  char *fmtstr, *tstr;
  if (timerem < 0.) {
    tstr = lives_strdup(_("unknown"));
  } else {
    tstr = format_tstr(timerem, 5);
  }
  fmtstr = lives_strdup_printf(_("Time remaining: %s"), tstr);
  lives_free(tstr);
  return fmtstr;
}


#define MIN_NOTIFY_TIME 300.

static void progbar_display(const char *txt, boolean lunch) {
  char *lbtext, *tmp, *rttxt;
  if (lunch) rttxt = remtime_string(-1.);
  else rttxt = lives_strdup("");
#ifdef PROGBAR_IS_ENTRY
  tmp = lives_strtrim(txt);
  if (lunch) {
    lbtext = lives_strdup_printf("%s - %s", tmp, rttxt);
    lives_free(tmp);
  } else lbtext = tmp;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  lives_entry_set_text(LIVES_ENTRY(mainw->proc_ptr->label3), lbtext);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
#else
  if (LIVES_IS_LABEL(proc->label3)) {
    if (lunch)
      lbtext = lives_strdup_printf("%s - %s", txt, rttxt);
    else
      lbtext = lives_strdup(txt);
    lives_label_set_text(LIVES_LABEL(proc->label3), lbtext);
  }
#endif
  lives_free(rttxt);
  lives_free(lbtext);
}


static void disp_fraction(double fraction_done, double timesofar, xprocess * proc) {
  // display fraction done and estimated time remaining
  double remtime;
  char *prog_label, *remtstr;

  if (fraction_done > 1.) fraction_done = 1.;
  if (fraction_done < 0.) fraction_done = 0.;

  if (fraction_done > disp_fraction_done + .0001)
    lives_progress_bar_set_fraction(LIVES_PROGRESS_BAR(proc->progressbar), fraction_done);

  remtime = timesofar / fraction_done - timesofar;
  remtstr = remtime_string(remtime);
  if (remtime > MIN_NOTIFY_TIME && mainw->proc_ptr->notify_cb) {
    lives_widget_set_opacity(lives_widget_get_parent(mainw->proc_ptr->notify_cb), 1.);
    lives_widget_set_sensitive(mainw->proc_ptr->notify_cb, TRUE);
  }
  prog_label = lives_strdup_printf(_("\n%d%% done. %s\n"), (int)(fraction_done * 100.), remtstr);
  lives_free(remtstr);
  progbar_display(prog_label, FALSE);
  lives_free(prog_label);

  disp_fraction_done = fraction_done;
}


static int progress_count;
static double progress_speed;
static int prog_fs_check;

#define PROG_LOOP_VAL 100

static void progbar_pulse_or_fraction(lives_clip_t *sfile, int frames_done, double fraction_done) {
  static double last_fraction_done = -1.;
  double timesofar;

  if (progress_count++ * progress_speed >= PROG_LOOP_VAL) {
    if (fraction_done > 0. || (frames_done <= mainw->proc_ptr->progress_end && mainw->proc_ptr->progress_end > 0
                               && !mainw->effects_paused && frames_done
                               > mainw->proc_ptr->progress_start)) {
      timesofar = (lives_get_current_ticks() - proc_start_ticks - mainw->timeout_ticks) / TICKS_PER_SECOND_DBL;
      if (fraction_done < 0.)
        fraction_done = (double)(frames_done - mainw->proc_ptr->progress_start)
                        / (double)(mainw->proc_ptr->progress_end - mainw->proc_ptr->progress_start + 1.);
      disp_fraction(fraction_done, timesofar, mainw->proc_ptr);
      if (fabs(fraction_done - last_fraction_done) < 1.) progress_count = 0;
      progress_speed = 4.;
      last_fraction_done = fraction_done;
    } else {
      lives_progress_bar_pulse(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar));
      progress_count = 0;
      if (!mainw->is_rendering) progress_speed = 2.;
    }
  }
  lives_widget_context_update();
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
      mainw->proc_ptr->frames_done = cfile->end = cfile->opening_frames = get_frame_count(mainw->current_file,
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
          lives_progress_bar_set_fraction(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar), fraction_done);

          if (est_time != -1.) {
            char *remtstr = remtime_string(est_time);
            prog_label = lives_strdup_printf(_("\n%d/%d frames opened. %s\n"),
                                             cfile->opening_frames - 1, cfile->frames, remtstr);
            lives_free(remtstr);
          } else prog_label = lives_strdup_printf(_("\n%d/%d frames opened.\n"), cfile->opening_frames - 1, cfile->frames);
        } else {
          lives_progress_bar_pulse(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar));
          prog_label = lives_strdup_printf(_("\n%d frames opened.\n"), cfile->opening_frames - 1);
        }
#ifdef PROGBAR_IS_ENTRY
        widget_opts.justify = LIVES_JUSTIFY_CENTER;
        lives_entry_set_text(LIVES_ENTRY(mainw->proc_ptr->label3), prog_label);
        widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
#else
        lives_label_set_text(LIVES_LABEL(mainw->proc_ptr->label3), prog_label);
#endif
        lives_free(prog_label);
        cfile->start = cfile->end > 0 ? 1 : 0;
        showclipimgs();
      }
    }
    shown_paused_frames = mainw->effects_paused;
  } else {
    lives_usleep(prefs->sleep_time);
  }
}


#ifdef USE_GDK_FRAME_CLOCK
static boolean using_gdk_frame_clock;
static GdkFrameClock *gclock;
static void clock_upd(GdkFrameClock * clock, gpointer user_data) {display_ready = TRUE;}
#endif


static boolean reset_timebase(void) {
  // [IMPORTANT] we subtract these from every calculation to make the numbers smaller
#if _POSIX_TIMERS
  ticks_t originticks = lives_get_current_ticks();
  originticks *= TICKS_TO_NANOSEC;
  mainw->origsecs = originticks / ONE_BILLION;
  mainw->orignsecs = originticks - mainw->origsecs * ONE_BILLION;
#else

#ifdef USE_MONOTONIC_TIME
  mainw->origsecs = 0; // not used
  mainw->orignsecs = lives_get_monotonic_time() * 1000;
#else

  /***************************************************/
  gettimeofday(&tv, NULL);
  /***************************************************/

  mainw->origsecs = tv.tv_sec;
  mainw->orignsecs = tv.tv_usec * 1000;
#endif
#endif

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
    boolean pa_reset = TRUE;
    if (prefs->audio_src == AUDIO_SRC_INT) {
      if (mainw->pulsed && !pa_time_reset(mainw->pulsed, 0)) {
        pa_reset = FALSE;
      }
    } else {
      if (mainw->pulsed_read && !pa_time_reset(mainw->pulsed_read, 0)) {
        pa_reset = FALSE;
      }
    }
    if (!pa_reset) {
      handle_audio_timeout();
      return FALSE;
    }
  }
#endif

#ifdef ENABLE_JACK
  if (mainw->jackd) {
    jack_time_reset(mainw->jackd, 0);
  }
  if (mainw->jackd_read) {
    jack_time_reset(mainw->jackd_read, 0);
  }
#endif

  reset_playback_clock();
  return TRUE;
}


boolean do_progress_dialog(boolean visible, boolean cancellable, const char *text) {
  // monitor progress, return FALSE if the operation was cancelled

  // this is the outer loop for playback and all kinds of processing

  // visible is set for processing (progress dialog is visible)
  // or unset for video playback (progress dialog is not shown)
  char *mytext = NULL;
  frames_t frames_done, frames;
  boolean got_err = FALSE;
  boolean markup = widget_opts.use_markup;

  widget_opts.use_markup = FALSE;

  // translation issues
  if (visible && text) mytext = lives_strdup(text);

  if (visible) {
    // check we have sufficient storage space
    if (mainw->next_ds_warn_level != 0 &&
        !check_storage_space(mainw->current_file, FALSE)) {
      lives_cancel_t cancelled = mainw->cancelled;
      on_cancel_keep_button_clicked(NULL, NULL);
      if (mainw->cancelled != CANCEL_NONE) mainw->cancelled = cancelled;
      d_print_cancelled();
      if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();
      return FALSE;
    }
  }

  audio_start = mainw->play_start;
  if (visible) accelerators_swapped = FALSE;
  frames_done = 0;
  disp_fraction_done = 0.;
  mainw->last_display_ticks = 0;
  shown_paused_frames = FALSE;

  mainw->cevent_tc = -1;

  progress_count = 0;
  progress_speed = 4.;
  prog_fs_check = 0;

  mainw->render_error = LIVES_RENDER_ERROR_NONE;

  if (!visible) {
    reset_frame_and_clip_index();
    mainw->force_show = TRUE;
    ready_player_one(cfile->next_event ? get_event_timecode(cfile->next_event) : 0);
  } else mainw->force_show = FALSE;

  mainw->cancelled = CANCEL_NONE;
  mainw->error = FALSE;
  clear_mainw_msg();
  if (!mainw->preview || cfile->opening) mainw->timeout_ticks = 0;

  if (visible) {
    mainw->noswitch = TRUE;
    mainw->is_processing = TRUE;
    desensitize();
    procw_desensitize();
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

    widget_opts.use_markup = markup;
    mainw->proc_ptr = create_processing(mytext);
    mainw->proc_ptr->owner = mainw->current_file;

    if (CURRENT_CLIP_IS_VALID) {
      mainw->proc_ptr->progress_start = cfile->progress_start;
      mainw->proc_ptr->progress_end = cfile->progress_end;
    } else {
      mainw->proc_ptr->progress_start = mainw->proc_ptr->progress_end = 0;
    }

    lives_freep((void **)&mytext);

    lives_progress_bar_set_pulse_step(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar), .01);

    mainw->proc_ptr->frames_done = 0;

    if (!cancellable) {
      lives_widget_hide(mainw->proc_ptr->cancel_button);
    }

    if (!LIVES_IS_INTERACTIVE) {
      lives_widget_set_sensitive(mainw->proc_ptr->cancel_button, FALSE);
      if (mainw->proc_ptr->stop_button)
        lives_widget_set_sensitive(mainw->proc_ptr->stop_button, FALSE);
      lives_widget_set_sensitive(mainw->proc_ptr->pause_button, FALSE);
      lives_widget_set_sensitive(mainw->proc_ptr->preview_button, FALSE);
    }

    if (cfile->opening && (capable->has_sox_play || (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd) ||
                           (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed)) && !LIVES_IS_PLAYING) {
      if (mainw->preview_box) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Preview"));
      lives_widget_set_tooltip_text(mainw->m_playbutton, _("Preview"));
      lives_widget_remove_accelerator(mainw->playall, mainw->accel_group, LIVES_KEY_p, (LiVESXModifierType)0);
      lives_widget_add_accelerator(mainw->proc_ptr->preview_button, LIVES_WIDGET_CLICKED_SIGNAL, mainw->accel_group, LIVES_KEY_p,
                                   (LiVESXModifierType)0, (LiVESAccelFlags)0);
      accelerators_swapped = TRUE;
    }
  }

  // mainw->origsecs, mainw->orignsecs is our base for quantising
  // (and is constant for each playback session)

  // firstticks is to do with the audio "frame" for sox, mplayer
  // startticks is the ticks value of the last frame played

  last_open_check_ticks = mainw->offsetticks = mainw->deltaticks = mainw->adjticks = 0;

#ifdef ENABLE_JACK
  if (mainw->record && prefs->audio_src == AUDIO_SRC_EXT && prefs->audio_player == AUD_PLAYER_JACK &&
      mainw->jackd_read && prefs->ahold_threshold > 0.) {
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
    mainw->proc_ptr = NULL;
    if (mainw->cancelled != CANCEL_NONE) {
      if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();
      return FALSE;
    }
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  // start audio recording now
  if (mainw->pulsed_read) {
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
    if (mainw->cancelled != CANCEL_NONE) {
      if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();
      return FALSE;
    }
  }
#endif
  mainw->scratch = SCRATCH_NONE;
  if (mainw->iochan && mainw->proc_ptr) lives_widget_show_all(mainw->proc_ptr->pause_button);
  display_ready = TRUE;

  /////////////////////////
  if (!reset_timebase()) {
    mainw->cancelled = CANCEL_INTERNAL_ERROR;
    return FALSE;
  }
  //////////////////////////
  //if (mainw->disk_mon & MONITOR_GROWTH) disk_monitor_start(mainw->monitor_dir);
  if ((mainw->disk_mon & MONITOR_QUOTA && prefs->disk_quota)) disk_monitor_start(prefs->workdir);

  if (visible) {
    proc_start_ticks = lives_get_current_ticks();
  } else {
    // video playback
    if (mainw->event_list || !CLIP_HAS_VIDEO(mainw->playing_file)) mainw->video_seek_ready = TRUE;
    if (mainw->event_list || !CLIP_HAS_AUDIO(mainw->playing_file)) mainw->audio_seek_ready = TRUE;
    cfile->last_frameno = cfile->frameno = mainw->play_start;
  }

  if (!mainw->playing_sel && (mainw->multitrack || !mainw->event_list)) mainw->play_start = 1;

  if (mainw->multitrack && !mainw->multitrack->is_rendering) {
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
      !mainw->is_rendering && !(cfile->opening && !mainw->preview) && mainw->jackd
      && mainw->jackd->playing_file > -1) {
    if (!jack_audio_seek_frame(mainw->jackd, mainw->aframeno)) {
      if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();
      return FALSE;
      /* if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd, mainw->aframeno); */
      /* else mainw->video_seek_ready = mainw->audio_seek_ready = TRUE; */
    }

    if (!(mainw->record && (prefs->audio_src == AUDIO_SRC_EXT || mainw->agen_key != 0 || mainw->agen_needs_reinit)))
      jack_get_rec_avals(mainw->jackd);
    else {
      mainw->rec_aclip = mainw->ascrap_file;
      mainw->rec_avel = 1.;
      mainw->rec_aseek = 0;
    }
    /* if (prefs->audio_src == AUDIO_SRC_INT) */
    /*   mainw->jackd->in_use = TRUE; */
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && cfile->achans > 0 && cfile->laudio_time > 0. &&
      !mainw->is_rendering && !(cfile->opening && !mainw->preview) && mainw->pulsed
      && mainw->pulsed->playing_file > -1) {
    if (!pulse_audio_seek_frame(mainw->pulsed, mainw->aframeno)) {
      handle_audio_timeout();
      if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();
      return FALSE;
    }

    if (!(mainw->record && (prefs->audio_src == AUDIO_SRC_EXT || mainw->agen_key != 0 || mainw->agen_needs_reinit)))
      pulse_get_rec_avals(mainw->pulsed);
    else {
      mainw->rec_aclip = mainw->ascrap_file;
      mainw->rec_avel = 1.;
      mainw->rec_aseek = 0;
    }
  }
#endif
  cfile->frames = frames;

#ifdef USE_GDK_FRAME_CLOCK
  using_gdk_frame_clock = FALSE;
  if (prefs->show_gui) {
    using_gdk_frame_clock = TRUE;
    display_ready = FALSE;
    gclock = gtk_widget_get_frame_clock(LIVES_MAIN_WINDOW_WIDGET);
    gdk_frame_clock_begin_updating(gclock);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(gclock), "update",
                              LIVES_GUI_CALLBACK(clock_upd), NULL);
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read) {
    mainw->pulsed_read->is_paused = FALSE;
  }
#endif
#ifdef ENABLE_JACK
  if (mainw->jackd_read) {
    mainw->jackd_read->is_paused = FALSE;
  }
#endif

  if (mainw->record) mainw->record_paused = FALSE;

  if (!mainw->proc_ptr && cfile->next_event) {
    /// reset dropped frame count etc
    process_events(NULL, FALSE, 0);
  }

  if (prefs->pbq_adaptive) reset_effort();
  if (mainw->multitrack && !mainw->multitrack->is_rendering) mainw->effort = EFFORT_RANGE_MAX;

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
        if (visible) mainw->noswitch = FALSE;
        //g_print("pb stopped, reason %d\n", ret);
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
#ifdef USE_GDK_FRAME_CLOCK
        if (using_gdk_frame_clock) {
          gdk_frame_clock_end_updating(gclock);
        }
#endif
        if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();
        return FALSE;
      }

      /* if (mainw->disk_mon & MONITOR_GROWTH) { */
      /* 	int64_t dsused = disk_monitor_check_result(mainw->monitor_dir); */
      /* 	if (dsused >= 0) mainw->monitor_size = dsused; */
      /* 	disk_monitor_start(mainw->monitor_dir); */
      /* } */
      if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) {
        int64_t dsused = disk_monitor_check_result(prefs->workdir);
        if (dsused >= 0) {
          capable->ds_used = dsused;
        }
        disk_monitor_start(prefs->workdir);
        mainw->dsu_valid = FALSE;
      }

      if (LIVES_UNLIKELY(mainw->agen_needs_reinit)) {
        // we are generating audio from a plugin and it needs reinit
        // - we do it in this thread so as not to hold up the player thread
        reinit_audio_gen();
      }

      if ((visible && !mainw->internal_messaging)
          || (LIVES_IS_PLAYING && CURRENT_CLIP_IS_VALID && cfile->play_paused)) lives_usleep(prefs->sleep_time);

      // normal playback, with realtime audio player
      if (!visible && (mainw->whentostop != STOP_ON_AUD_END || is_realtime_aplayer(prefs->audio_player))) continue;

      if (mainw->iochan && !progress_count) {
        // pump data from stdout to textbuffer
        // this is for encoder output
        pump_io_chan(mainw->iochan);
      }
      if (!mainw->internal_messaging && mainw->proc_ptr) {
        // background processing (e.g. rendered effects)
        progbar_pulse_or_fraction(cfile, mainw->proc_ptr->frames_done, mainw->proc_ptr->frac_done);
      }
    }

    if (!mainw->internal_messaging && mainw->proc_ptr) {
      // background processing (e.g. rendered effects)
      lives_fread_string(mainw->msg, MAINW_MSG_SIZE, cfile->info_file);
      progbar_pulse_or_fraction(cfile, mainw->proc_ptr->frames_done, mainw->proc_ptr->frac_done);
    }
    // else call realtime effect pass
    else {
      mainw->render_error = (*mainw->progress_fn)(FALSE);

      if (mainw->render_error >= LIVES_RENDER_ERROR) {
        got_err = TRUE;
        goto finish;
      }

      // display progress fraction or pulse bar
      if (*mainw->msg && (frames_done = atoi(mainw->msg)) > 0 && mainw->proc_ptr) {
        if (mainw->msg[lives_strlen(mainw->msg) - 1] == '%')
          mainw->proc_ptr->frac_done = atof(mainw->msg);
        else
          mainw->proc_ptr->frames_done = atoi(mainw->msg);
      }
      if (!mainw->effects_paused) {
        if (prog_fs_check-- <= 0) {
          check_storage_space(mainw->current_file, TRUE);
          prog_fs_check = PROG_LOOP_VAL;
        }
        if (mainw->proc_ptr) {
          progress_speed = 1000000.;
          progbar_pulse_or_fraction(cfile, mainw->proc_ptr->frames_done, mainw->proc_ptr->frac_done);
        }
      } else lives_widget_context_update();
    }

    if (mainw->preview_req) {
      mainw->preview_req = FALSE;
      mainw->noswitch = FALSE;
      if (mainw->proc_ptr) on_preview_clicked(LIVES_BUTTON(mainw->proc_ptr->preview_button), NULL);
      mainw->noswitch = TRUE;
    }

    //    #define DEBUG
#ifdef DEBUG
    if (*(mainw->msg)) g_print("%s msg %s\n", cfile->info_file, mainw->msg);
#endif

    // we got a message from the backend...

    if (visible && *mainw->msg && *mainw->msg != '!' && mainw->proc_ptr
        && (!accelerators_swapped || cfile->opening) && cancellable
        && (!cfile->nopreview || cfile->keep_without_preview)) {
      if (!cfile->nopreview && !(cfile->opening && mainw->multitrack)) {
        lives_widget_set_no_show_all(mainw->proc_ptr->preview_button, FALSE);
        lives_widget_show_all(mainw->proc_ptr->preview_button);
        lives_button_grab_default_special(mainw->proc_ptr->preview_button);
        lives_widget_grab_focus(mainw->proc_ptr->preview_button);
      }

      // show buttons
      if (cfile->opening_loc) {
        lives_widget_hide(mainw->proc_ptr->pause_button);
        if (mainw->proc_ptr->stop_button)
          lives_widget_show_all(mainw->proc_ptr->stop_button);
      } else {

        // AHA !!
        lives_widget_show_all(mainw->proc_ptr->pause_button);
        if (mainw->proc_ptr->stop_button)
          lives_widget_hide(mainw->proc_ptr->stop_button);
      }

      if (!cfile->opening && !cfile->nopreview) {
        lives_button_grab_default_special(mainw->proc_ptr->preview_button);
        lives_widget_grab_focus(mainw->proc_ptr->preview_button);
        if (mainw->preview_box) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Preview"));
        lives_widget_set_tooltip_text(mainw->m_playbutton, _("Preview"));
        lives_widget_remove_accelerator(mainw->playall, mainw->accel_group, LIVES_KEY_p, (LiVESXModifierType)0);
        lives_widget_add_accelerator(mainw->proc_ptr->preview_button, LIVES_WIDGET_CLICKED_SIGNAL,
                                     mainw->accel_group, LIVES_KEY_p,
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
          int frames_done = atoi(array[0]);
          if (frames_done > 0 && mainw->proc_ptr) mainw->proc_ptr->frames_done = frames_done;
          if (numtok == 2 && *(array[1])) cfile->progress_end = atoi(array[1]);
          else if (numtok == 5 && *(array[4])) {
            // rendered generators
            cfile->start = cfile->undo_start = 1;
            cfile->frames = cfile->end = cfile->undo_end = atoi(array[0]);
            cfile->hsize = atoi(array[1]);
            cfile->vsize = atoi(array[2]);
            cfile->fps = cfile->pb_fps = lives_strtod(array[3]);
            if (cfile->fps == 0.) cfile->fps = cfile->pb_fps = prefs->default_fps;
            cfile->progress_end = atoi(array[4]);
          }
          lives_strfreev(array);
        } else {
          if (mainw->proc_ptr) {
            if (*mainw->msg) {
              if (*mainw->msg == '!')
                progbar_display(mainw->msg + 1, TRUE);
              else {
                if (mainw->msg[lives_strlen(mainw->msg) - 1] == '%')
                  mainw->proc_ptr->frac_done = atof(mainw->msg) / 100.;
                else {
                  int frames_done = atoi(mainw->msg);
                  if (frames_done > 0 && mainw->proc_ptr) mainw->proc_ptr->frames_done = frames_done;
		  // *INDENT-OFF*
		}}}}}}
      // *INDENT-ON*

      // do a processing pass
      if (process_one(visible)) {
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
#ifdef USE_GDK_FRAME_CLOCK
        if (using_gdk_frame_clock) {
          gdk_frame_clock_end_updating(gclock);
        }
#endif
        if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();
        if (visible) mainw->noswitch = FALSE;
        return FALSE;
      }

      if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) {
        int64_t dsused = disk_monitor_check_result(prefs->workdir);
        if (dsused >= 0) {
          capable->ds_used = dsused;
        }
        disk_monitor_start(prefs->workdir);
        mainw->dsu_valid = FALSE;
      }


      if (LIVES_UNLIKELY(mainw->agen_needs_reinit)) {
        // we are generating audio from a plugin and it needs reinit
        // - we do it in this thread so as not to hold up the player thread
        reinit_audio_gen();
      }

      if (mainw->iochan && progress_count == 0) {
        // pump data from stdout to textbuffer
        pump_io_chan(mainw->iochan);
      }

      if (!mainw->internal_messaging) lives_nanosleep(1000);
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
  if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();

  //play/operation ended
  if (visible) {
    if (cfile->clip_type == CLIP_TYPE_DISK && (mainw->cancelled != CANCEL_NO_MORE_PREVIEW || !cfile->opening)) {
      lives_rm(cfile->info_file);
    }
    if (mainw->preview_box && !mainw->preview) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Play all"));
    if (accelerators_swapped) {
      if (!mainw->preview) lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play all"));
      if (mainw->proc_ptr) lives_widget_remove_accelerator(mainw->proc_ptr->preview_button, mainw->accel_group, LIVES_KEY_p,
            (LiVESXModifierType)0);
      lives_widget_add_accelerator(mainw->playall, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group, LIVES_KEY_p,
                                   (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);
      accelerators_swapped = FALSE;
    }
    if (mainw->proc_ptr) {
      const char *btext = NULL;
      if (mainw->iochan) btext = lives_text_view_get_text(mainw->optextview);
      if (mainw->proc_ptr->processing) {
        if (mainw->proc_ptr->rte_off_cb
            && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(mainw->proc_ptr->rte_off_cb))) {
          weed_deinit_all(FALSE);
        }
        if (mainw->proc_ptr->notify_cb
            && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(mainw->proc_ptr->notify_cb))) {
          notify_user(mainw->proc_ptr->text);
        }
        lives_freep((void **)&mainw->proc_ptr->text);
        lives_widget_destroy(mainw->proc_ptr->processing);
      }
      lives_free(mainw->proc_ptr);
      mainw->proc_ptr = NULL;
      if (btext) {
        lives_text_view_set_text(mainw->optextview, btext, -1);
        lives_free((char *)btext);
      }
    }
    mainw->is_processing = FALSE;
    if (cfile->menuentry) {
      // note - for operations to/from clipboard (file 0) we
      // should manually call sensitize() after operation
      sensitize();
    }
  } else {
    mainw->is_processing = TRUE;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  // get error message (if any)
  if (!strncmp(mainw->msg, "error", 5)) {
    handle_backend_errors(FALSE);
    if (mainw->cancelled || mainw->error) {
      if (visible) mainw->noswitch = FALSE;
      return FALSE;
    }
  } else {
    if (!check_storage_space(mainw->current_file, FALSE)) {
      if (visible) mainw->noswitch = FALSE;
      return FALSE;
    }
  }

  if (got_err) {
    if (visible) mainw->noswitch = FALSE;
    return FALSE;
  }
#ifdef DEBUG
  g_print("exiting progress dialog\n");
#endif
  if (visible) mainw->noswitch = FALSE;
  return TRUE;
}


#define MIN_FLASH_TIME MILLIONS(100)

boolean do_auto_dialog(const char *text, int type) {
  // type 0 = normal auto_dialog
  // type 1 = countdown dialog for audio recording
  // type 2 = normal with cancel

  FILE *infofile = NULL;

  uint64_t time = 0, stime = 0;

  char *label_text;
  char *mytext = lives_strdup(text);

  double time_rem, last_time_rem = 10000000.;
  lives_alarm_t alarm_handle = 0;

  mainw->cancelled = CANCEL_NONE;

  if (type == 1 && mainw->rec_end_time != -1.) {
    stime = lives_get_current_ticks();
  }

  mainw->error = FALSE;

  mainw->proc_ptr = create_processing(mytext);

  lives_freep((void **)&mytext);
  if (mainw->proc_ptr->stop_button)
    lives_widget_hide(mainw->proc_ptr->stop_button);

  if (type == 2) {
    lives_widget_show_all(mainw->proc_ptr->cancel_button);
    lives_widget_hide(mainw->proc_ptr->pause_button);
    mainw->cancel_type = CANCEL_SOFT;
  }
  if (type == 0) {
    lives_widget_hide(mainw->proc_ptr->cancel_button);
  }

  lives_progress_bar_set_pulse_step(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar), .01);

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_set_cursor_style(LIVES_CURSOR_BUSY, mainw->proc_ptr->processing);

  //lives_widget_context_update();

  if (type == 0 || type == 2) {
    clear_mainw_msg();
    alarm_handle = lives_alarm_set(MIN_FLASH_TIME); // don't want to flash too fast...
  } else if (type == 1) {
    // show buttons
    if (mainw->proc_ptr->stop_button)
      lives_widget_show_all(mainw->proc_ptr->stop_button);
    lives_widget_show_all(mainw->proc_ptr->cancel_button);
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed_read) {
      pulse_driver_uncork(mainw->pulsed_read);
    }
#endif
    if (mainw->rec_samples != 0) {
      lives_usleep(prefs->sleep_time);
    }
  }

  while (mainw->cancelled == CANCEL_NONE && !(infofile = fopen(cfile->info_file, "r"))) {
    lives_progress_bar_pulse(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar));
    lives_widget_context_update();
    lives_usleep(prefs->sleep_time);
    if (type == 1 && mainw->rec_end_time != -1.) {
      time = lives_get_current_ticks();

      // subtract start time
      time -= stime;

      time_rem = mainw->rec_end_time - (double)time / TICKS_PER_SECOND_DBL;
      if (time_rem >= 0. && time_rem < last_time_rem) {
        char *remtstr = remtime_string(time_rem);
        label_text = lives_strdup_printf(_("\n%s"), remtstr);
        lives_free(remtstr);
        lives_label_set_text(LIVES_LABEL(mainw->proc_ptr->label2), label_text);
        lives_free(label_text);
        last_time_rem = time_rem;
      } else if (time_rem < 0) break;
    }
  }

  if (!mainw->cancelled) {
    if (infofile) {
      if (type == 0 || type == 2) {
        size_t bread;
        THREADVAR(read_failed) = FALSE;
        bread = lives_fread(mainw->msg, 1, MAINW_MSG_SIZE, infofile);
        fclose(infofile);
        lives_memset(mainw->msg + bread, 0, 1);
        if (cfile->clip_type == CLIP_TYPE_DISK) lives_rm(cfile->info_file);
        if (alarm_handle > 0) {
          ticks_t tl;
          while ((tl = lives_alarm_check(alarm_handle)) > 0 && !mainw->cancelled) {
            lives_progress_bar_pulse(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar));
            //lives_widget_process_updates(mainw->proc_ptr->processing);
            lives_usleep(prefs->sleep_time);
          }
          lives_alarm_clear(alarm_handle);
        }
      } else fclose(infofile);
    }
  }

  if (mainw->proc_ptr) {
    lives_widget_destroy(mainw->proc_ptr->processing);
    lives_freep((void **)&mainw->proc_ptr->text);
    lives_free(mainw->proc_ptr);
    mainw->proc_ptr = NULL;
  }

  if (type == 2) mainw->cancel_type = CANCEL_KILL;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  if (mainw->cancelled) return FALSE;

  // get error message (if any)
  if (type != 1 && !strncmp(mainw->msg, "error", 5)) {
    handle_backend_errors(FALSE);
    if (mainw->cancelled || mainw->error) return FALSE;
  } else {
    if (CURRENT_CLIP_IS_VALID)
      if (!check_storage_space(mainw->current_file, FALSE)) return FALSE;
  }
  return TRUE;
}


///// TODO: end processing.c /////

boolean do_save_clipset_warn(void) {
  char *extra;
  char *msg;

  if (!mainw->no_exit && !mainw->only_close) extra = lives_strdup(", and LiVES will exit");
  else extra = lives_strdup("");

  msg = lives_strdup_printf(
          _("Saving the set will cause copies of all loaded clips to remain on the disk%s.\n\n"
            "Please press 'Cancel' if that is not what you want.\n"), extra);
  lives_free(extra);

  if (!do_warning_dialog_with_check(msg, WARN_MASK_SAVE_SET)) {
    lives_free(msg);
    return FALSE;
  }
  lives_free(msg);
  return TRUE;
}


LIVES_GLOBAL_INLINE void too_many_files(void) {
  do_error_dialogf(_("\nSorry, LiVES can only open %d files at once.\nPlease close a file and then try again."), MAX_FILES);
}


void workdir_warning(void) {
  char *tmp, *com = lives_strdup_printf(
                      _("LiVES was unable to write to its working directory.\n\nThe current working directory is:\n\n%s\n\n"
                        "Please make sure you can write to this directory."),
                      (tmp = lives_filename_to_utf8(prefs->workdir, -1, NULL, NULL, NULL)));
  lives_free(tmp);
  if (mainw && mainw->is_ready) {
    do_error_dialog(com);
  }
  lives_free(com);
}


LIVES_GLOBAL_INLINE void do_no_mplayer_sox_error(void) {
  do_error_dialog(_("\nLiVES currently requires either 'mplayer', 'mplayer2', or 'sox' to function. "
                    "Please install one or other of these, and try again.\n"));
}


LIVES_GLOBAL_INLINE void do_need_mplayer_dialog(void) {
  do_error_dialog(
    _("\nThis function requires either mplayer or mplayer2 to operate.\nYou may wish to install "
      "one or other of these and try again.\n"));
}


LIVES_GLOBAL_INLINE void do_need_mplayer_mpv_dialog(void) {
  do_error_dialog(
    _("\nThis function requires either mplayer, mplayer2 or mpv to operate.\nYou may wish to install one or other of these "
      "and try again.\n"));
}


LIVES_GLOBAL_INLINE void do_audio_warning(void) {
  do_error_dialog(_("Audio was not loaded; please install mplayer or mplayer2 if you expected audio for this clip.\n"));
}


LIVES_GLOBAL_INLINE void do_encoder_sox_error(void) {
  do_error_dialog(
    _("Audio resampling is required for this format.\nPlease install 'sox'\nOr switch to another encoder format in "
      "Tools | Preferences | Encoding\n"));
}


LIVES_GLOBAL_INLINE void do_encoder_acodec_error(void) {
  do_error_dialog(
    _("\n\nThis encoder/format cannot use the requested audio codec.\n"
      "Please set the audio codec in Tools|Preferences|Encoding\n"));
}


LIVES_GLOBAL_INLINE void do_layout_scrap_file_error(void) {
  do_error_dialog(
    _("This layout includes generated frames.\nIt cannot be saved, you must render it to a clip first.\n"));
}


LIVES_GLOBAL_INLINE void do_layout_ascrap_file_error(void) {
  do_error_dialog(
    _("This layout includes generated or recorded audio.\nIt cannot be saved, you must render it to a clip first.\n"));
}


boolean rdet_suggest_values(int width, int height, double fps, int fps_num, int fps_denom, int arate, int asigned,
                            boolean swap_endian, boolean anr, boolean ignore_fps) {
  LiVESWidget *prep_dialog;

  char *msg1 = lives_strdup_printf(_("\n\nDue to restrictions in the %s format\n"), prefs->encoder.of_desc);
  char *msg2 = lives_strdup(""), *msg3 = lives_strdup(""), *msg4 = lives_strdup("");
  char *msg5 = lives_strdup(""), *msg6 = lives_strdup(""), *msg7 = lives_strdup("");
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
    msg2 = (_("LiVES recommends the following settings:\n\n"));
    if (swap_endian || (asigned == 1 && rdet->aendian == AFORM_UNSIGNED) || (asigned == 2 && rdet->aendian == AFORM_SIGNED)
        || (arate > 0 && arate != rdet->arate)) {
      char *sstring;
      char *estring;

      if (asigned == 1 && rdet->aendian == AFORM_UNSIGNED) sstring = (_(", signed"));
      else if (asigned == 2 && rdet->aendian == AFORM_SIGNED) sstring = (_(", unsigned"));
      else sstring = lives_strdup("");

      if (swap_endian) {
        if (mainw->endian != AFORM_BIG_ENDIAN) estring = (_(", little-endian"));
        else estring = (_(", big-endian"));
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
      msg6 = (_("\nYou may wish to:\n"));
      if ((rdet->width != width || rdet->height != height) && height * width > 0) {
        lives_free(msg7);
        msg7 = lives_strdup_printf(_("resize video to %d x %d pixels\n"), width, height);
      } else anr = FALSE;
      if (arate < 1) {
        lives_free(msg8);
        msg8 = (_("disable audio, since the target encoder cannot encode audio\n"));
      }
    } else anr = FALSE;
  }
  msg_a = lives_strconcat(msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, NULL);
  lives_free(msg1); lives_free(msg2); lives_free(msg3); lives_free(msg4);
  lives_free(msg5); lives_free(msg6); lives_free(msg7); lives_free(msg8);
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
  char *msg2 = lives_strdup(""), *msg3 = lives_strdup(""), *msg4 = lives_strdup("");
  char *msg5 = lives_strdup(""), *msg6 = lives_strdup(""), *msg7 = lives_strdup("");
  char *msg_a, *msg_b = NULL;

  double cfps;

  boolean ret;

  int carate, chsize, cvsize;

  if (rdet) {
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
    msg2 = (_("LiVES must:\n"));
    if (swap_endian || asigned != 0 || (arate > 0 && arate != carate)) {
      char *sstring;
      char *estring;
      if (asigned == 1) sstring = (_(", signed"));
      else if (asigned == 2) sstring = (_(", unsigned"));
      else sstring = lives_strdup("");

      if (swap_endian) {
        if (cfile->signed_endian & AFORM_BIG_ENDIAN) estring = (_(", little-endian"));
        else estring = (_(", big-endian"));
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
      msg6 = (_("\nYou may wish to:\n"));
      msg7 = lives_strdup_printf(_("Set video size to %d x %d pixels\n"), width, height);
    } else anr = FALSE;
  }
  msg_a = lives_strconcat(msg1, msg2, msg3, msg4, msg5, msg6, msg7, NULL);
  if (save_all) {
    msg_b = lives_strdup(
              _("\nYou will be able to undo these changes afterwards.\n\nClick `OK` to proceed, `Cancel` to abort.\n\n"));
  } else {
    msg_b = (_("\nChanges applied to the selection will not be permanent.\n\n"));
  }
  lives_free(msg1); lives_free(msg2); lives_free(msg3); lives_free(msg4);
  lives_free(msg5); lives_free(msg6); lives_free(msg7);
  prep_dialog = create_encoder_prep_dialog(msg_a, msg_b, anr);
  lives_free(msg_a);
  if (msg_b) lives_free(msg_b);
  ret = (lives_dialog_run(LIVES_DIALOG(prep_dialog)) == LIVES_RESPONSE_OK);
  lives_widget_destroy(prep_dialog);
  return ret;
}


LIVES_GLOBAL_INLINE void perf_mem_warning(void) {
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
             "The insertion will be adjusted to fit into the clip.\n\n"
             "Please press Cancel to abort the insert, or OK to continue."), WARN_MASK_FPS);
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
      lives_snprintf(sfile->title, 1024, "%s", lives_entry_get_text(LIVES_ENTRY(commentsw->title_entry)));
      lives_snprintf(sfile->author, 1024, "%s", lives_entry_get_text(LIVES_ENTRY(commentsw->author_entry)));
      lives_snprintf(sfile->comment, 1024, "%s", lives_entry_get_text(LIVES_ENTRY(commentsw->comment_entry)));

      save_clip_value(fileno, CLIP_DETAILS_TITLE, sfile->title);
      save_clip_value(fileno, CLIP_DETAILS_AUTHOR, sfile->author);
      save_clip_value(fileno, CLIP_DETAILS_COMMENT, sfile->comment);

      if (encoding && sfile->subt && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(commentsw->subt_checkbutton))) {
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


LIVES_GLOBAL_INLINE void do_messages_window(boolean is_startup) {
  text_window *textwindow;
  char *text = dump_messages(-1, -1);
  widget_opts.expand = LIVES_EXPAND_EXTRA;
  textwindow = create_text_window(_("Message History"), text, NULL, TRUE);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_free(text);
  if (is_startup) {
    LiVESWidget *area =
      lives_dialog_get_action_area((LIVES_DIALOG(textwindow->dialog)));
    LiVESWidget *cb = lives_standard_check_button_new(_("Show messages on startup"), TRUE,
                      LIVES_BOX(area), NULL);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(cb), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(toggle_sets_pref),
                              (livespointer)PREF_MSG_START);
    lives_button_box_make_first(LIVES_BUTTON_BOX(area), widget_opts.last_container);
    lives_widget_show_all(textwindow->dialog);
  }
  lives_widget_context_update();
  lives_scrolled_window_scroll_to(LIVES_SCROLLED_WINDOW(textwindow->scrolledwindow), LIVES_POS_BOTTOM);
}


LIVES_GLOBAL_INLINE void do_upgrade_error_dialog(void) {
  char *tmp;
  char *msg = lives_strdup_printf(
                _("After upgrading/installing, you may need to adjust the <prefix_dir> setting in your %s file"),
                (tmp = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)));
  startup_message_info(msg);
  lives_free(msg); lives_free(tmp);
}


LIVES_GLOBAL_INLINE void do_rendered_fx_dialog(void) {
  char *tmp;
  char *msg = lives_strdup_printf(
                _("\n\nLiVES could not find any rendered effect plugins.\nPlease make sure you have them installed in\n"
                  "%s%s%s\nor change the value of <lib_dir> in %s\n"),
                prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_RENDERED_EFFECTS_BUILTIN,
                (tmp = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)));
  do_error_dialog_with_check(msg, WARN_MASK_RENDERED_FX);
  lives_free(msg);
  lives_free(tmp);
}


void do_audio_import_error(void) {
  char *msg = (_("Sorry, unknown audio type.\n\n (Filenames must end in"));
  char *tmp;

  char *filt[] = LIVES_AUDIO_LOAD_FILTER;

  register int i = 0;

  while (filt[i]) {
    if (filt[i + 1]) {
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


LIVES_GLOBAL_INLINE boolean prompt_remove_layout_files(void) {
  return (do_yesno_dialog(
            _("\nDo you wish to remove the layout files associated with this set ?\n"
              "(They will not be usable without the set).\n")));
}


boolean do_set_duplicate_warning(const char *new_set) {
  char *msg = lives_strdup_printf(
                _("\nA set entitled %s already exists.\n"
                  "Click OK to add the current clips and layouts to the existing set.\n"
                  "Click Cancel to pick a new name.\n"), new_set);
  boolean retcode = do_warning_dialog_with_check(msg, WARN_MASK_DUPLICATE_SET);
  lives_free(msg);
  return retcode;
}


LIVES_GLOBAL_INLINE boolean do_layout_alter_frames_warning(void) {
  return do_warning_dialog(
           _("\nFrames from this clip are used in some multitrack layouts.\n"
             "Are you sure you wish to continue ?\n."));
}


LIVES_GLOBAL_INLINE boolean do_layout_alter_audio_warning(void) {
  return do_warning_dialog(
           _("\nAudio from this clip is used in some multitrack layouts.\n"
             "Are you sure you wish to continue ?\n."));
}


LIVES_GLOBAL_INLINE boolean do_gamma_import_warn(uint64_t fv, int gamma_type) {
  char *fvx = unhash_version(fv);
  boolean ret = do_yesno_dialogf(_("This clip is saved with a gamma type of %s\n"
                                   "from a future version of LiVES (%s)\n"
                                   "Opening it with the current version may result in a loss of quality\n"
                                   "Do you wish to continue ?"), weed_gamma_get_name(gamma_type), fvx);
  lives_free(fvx);
  return ret;
}


boolean do_mt_lb_warn(boolean lb) {
  char *tmp, *msg, *endis, *endised;
  boolean ret;

  if (lb) {
    endis = _("enable");
    endised = _("enabled");
  } else {
    endis = _("disable");
    endised = _("disabled");
  }
  msg = lives_strdup_printf((tmp = _("This layout was saved with letterboxing %s\n"
                                     "To preserve the original appearance, I can override\n"
                                     "the current setting and %s letterboxing for this layout\n\n"
                                     "Click 'Yes' to proceed, or 'No' to keep the current setting\n\n"
                                     "(Note: the value for the current layout can be modified at any time\n"
                                     "via the menu option 'Tools' / 'Change Width, Height and Audio Values')\n")),
                            endised, endis);
  lives_free(tmp); lives_free(endised); lives_free(endis);
  ret = do_yesno_dialog_with_check(msg, WARN_MASK_LAYOUT_LB);
  lives_free(msg);
  return ret;
}


static LiVESResponseType _do_df_notfound_dialog(const char *detail, const char *dfname, boolean is_dir) {
  LiVESWidget *warning;
  LiVESResponseType response;
  char *xdetail, *msg, *whatitis, *extra;

  if (detail) xdetail = (char *)detail;

  if (!is_dir) {
    if (!detail) {
      xdetail = lives_strdup(_("The file"));
      extra = _("could not be found.");
    } else extra = lives_strdup("");
    whatitis = (_("this file"));
  } else {
    if (!detail) {
      xdetail = lives_strdup(_("The directory"));
      extra = _("could not be found.");
    } else extra = lives_strdup("");
    whatitis = (_("this directory"));
  }
  msg = lives_strdup_printf(_("\n%s\n%s\n%s\n"
                              "Click Retry to try again, Browse to browse to the new location.\n"
                              "otherwise click Skip to skip loading %s.\n"), xdetail, dfname, extra, whatitis);
  warning = create_message_dialog(LIVES_DIALOG_SKIP_RETRY_BROWSE, msg, 0);
  response = lives_dialog_run(LIVES_DIALOG(warning));
  lives_widget_destroy(warning);
  lives_widget_context_update();
  lives_free(msg); lives_free(whatitis);
  if (xdetail != detail) lives_free(xdetail);
  return response;
}


LiVESResponseType do_dir_notfound_dialog(const char *detail, const char *dirname) {
  return _do_df_notfound_dialog(detail, dirname, TRUE);
}

LiVESResponseType do_file_notfound_dialog(const char *detail, const char *filename) {
  return _do_df_notfound_dialog(detail, filename, FALSE);
}


LIVES_GLOBAL_INLINE void do_no_decoder_error(const char *fname) {
  lives_widget_context_update();
  do_error_dialogf(
    _("\n\nLiVES could not find a required decoder plugin for the clip\n%s\n"
      "The clip could not be loaded.\n"), fname);
}


LIVES_GLOBAL_INLINE void do_no_loadfile_error(const char *fname) {
  do_error_dialogf(_("\n\nThe file\n%s\nCould not be found.\n"), fname);
}


#ifdef ENABLE_JACK

LIVES_GLOBAL_INLINE boolean do_jack_nonex_warn(const char *server_cfg) {
  if (!lives_file_test(server_cfg, LIVES_FILE_TEST_EXISTS))
    return do_yesno_dialogf(_("The script file selected for server startup does not exist.\n"
                              "(%s)\nAre you sure you wish to continue with these settings ?\n"),
                            server_cfg);
  else
    return do_yesno_dialogf(_("The script file selected for server startup is not executable.\n"
                              "(%s)\nAre you sure you wish to continue with these settings ?\n"),
                            server_cfg);
}


LIVES_GLOBAL_INLINE boolean do_jack_scripts_warn(const char *scrptx) {
  char *err =
    lives_strdup_printf(_("\nMultiple jack clients (audio and transport) are set to use the same "
                          "startup script file\n"
                          "%s\nIf one client starts the server, then the other will fail.\n"
                          "Are you sure you wish to continue with the current settings ?"),
                        scrptx);
  boolean ret = do_yesno_dialog_with_check(err, WARN_MASK_JACK_SCRPT);
  lives_free(err);
  return ret;
}


LIVES_GLOBAL_INLINE void do_jack_noopen_warn(boolean is_trans) {
  do_error_dialogf(_("\nLiVES was unable to start up jack. "
                     "Please ensure that %s is set up correctly on your machine\n"
                     "and also that the soundcard is not in use by another program\n"
                     "Automatic jack startup will be disabled now.\n"),
                   is_trans ? prefs->jack_tdriver : prefs->jack_adriver);
}

LIVES_GLOBAL_INLINE void do_jack_noopen_warn3(boolean is_trans) {
  char *extra = "", *tmp;
  if (is_trans && (prefs->jack_opts & JACK_OPTS_START_TSERVER)) {
    if (prefs->jack_tdriver && *prefs->jack_tdriver) {
      extra = lives_strdup_printf(_("\nThere may be a problem with the <b>'%s'</b> driver"),
                                  (tmp = lives_markup_escape_text(prefs->jack_tdriver, -1)));
      lives_free(tmp);
    }
  } else if (prefs->jack_opts & JACK_OPTS_START_ASERVER) {
    if (prefs->jack_adriver && *prefs->jack_adriver) {
      extra = lives_strdup_printf(_("\nThere may be a problem with the <b>'%s'</b> driver."),
                                  (tmp = lives_markup_escape_text(prefs->jack_adriver, -1)));
      lives_free(tmp);
    }
  }
  widget_opts.use_markup = TRUE;
  do_error_dialogf(_("\nLiVES was unable to establish a connection to jack server <b>'%s'</b>.%s"
                     "\nPlease start jack before restarting LiVES\n"),
                   (tmp = lives_markup_escape_text(is_trans ? prefs->jack_tserver_sname
                          : prefs->jack_aserver_sname, -1)), extra);
  lives_free(tmp);
  widget_opts.use_markup = FALSE;
  if (*extra) lives_free(extra);
}

LIVES_GLOBAL_INLINE void do_jack_noopen_warn4(int suggest_opts) {
  const char *otherbit;
  char *firstbit;
  if (suggest_opts != -1) {
    firstbit = lives_strdup_printf("lives -jackopts %d\nwhich will let LiVES start the server itself, "
                                   "or\n", suggest_opts);
  } else firstbit = "";
#ifdef HAVE_PULSE_AUDIO
  if (capable->has_pulse_audio == PRESENT) otherbit = "lives -aplayer pulse";
  else
#endif
    otherbit = "lives -aplayer none";
  do_info_dialogf(_("\nAlternatively, you may start lives with commandline option:\n\n"
                    "%s%s\nto avoid use of jack audio altogether"),
                  firstbit, otherbit);
  if (*firstbit) lives_free(firstbit);
}

LIVES_GLOBAL_INLINE void do_jack_noopen_warn2(void) {
  do_info_dialog(_("\nAlternately, you can restart LiVES and select another audio player.\n"));
}
#endif

LIVES_GLOBAL_INLINE void do_mt_backup_space_error(lives_mt * mt, int memreq_mb) {
  char *msg = lives_strdup_printf(
                _("\n\nLiVES needs more backup space for this layout.\nYou can increase "
                  "the value in Preferences/Multitrack.\n"
                  "It is recommended to increase it to at least %d MB"),
                memreq_mb);
  do_error_dialog_with_check(msg, WARN_MASK_MT_BACKUP_SPACE);
  lives_free(msg);
}


LIVES_GLOBAL_INLINE boolean do_set_rename_old_layouts_warning(const char *new_set) {
  return do_yesno_dialogf(
           _("\nSome old layouts for the set %s already exist.\n"
             "It is recommended that you delete them.\nDo you wish to delete them ?\n"),
           new_set);
}


LIVES_GLOBAL_INLINE void do_mt_undo_mem_error(void) {
  do_error_dialog(
    _("\nLiVES was unable to reserve enough memory for multitrack undo.\n"
      "Either close some other applications, or reduce the undo memory\n"
      "using Preferences/Multitrack/Undo Memory\n"));
}


LIVES_GLOBAL_INLINE void do_mt_undo_buf_error(void) {
  do_error_dialog(_("\nOut of memory for undo.\nYou may need to increase the undo memory\n"
                    "using Preferences/Multitrack/Undo Memory\n"));
}


LIVES_GLOBAL_INLINE void do_mt_set_mem_error(boolean has_mt) {
  char *msg1 = (_("\nLiVES was unable to reserve enough memory for the multitrack undo buffer.\n"));
  char *msg2;
  char *msg3 = (_("or enter a smaller value.\n"));

  if (has_mt) msg2 = (_("Try again from the clip editor, try closing some other applications\n"));
  else msg2 = (_("Try closing some other applications\n"));

  do_error_dialogf("%s%s%s", msg1, msg2, msg3);
  lives_free(msg1); lives_free(msg2); lives_free(msg3);
}


LIVES_GLOBAL_INLINE void do_mt_audchan_error(int warn_mask) {
  do_error_dialog_with_check(
    _("Multitrack is set to 0 audio channels, but this layout has audio.\n"
      "You should adjust the audio settings from the Tools menu.\n"),
    warn_mask);
}


LIVES_GLOBAL_INLINE void do_mt_no_audchan_error(void) {
  do_error_dialog(_("The current layout has audio, so audio channels may not be set to zero.\n"));
}


LIVES_GLOBAL_INLINE void do_mt_no_jack_error(int warn_mask) {
  do_error_dialog_with_check(
    _("Multitrack audio preview is only available with the\n\"jack\" or \"pulseaudio\" audio player.\n"
      "You can set this in Tools|Preferences|Playback."),
    warn_mask);
}


LIVES_GLOBAL_INLINE boolean do_mt_rect_prompt(void) {
  return do_yesno_dialog(
           _("Errors were detected in the layout (which may be due to transferring from another system, "
             "or from an older version of LiVES).\n"
             "Should I try to repair the disk copy of the layout ?\n"));
}


LIVES_GLOBAL_INLINE void do_bad_layout_error(void) {
  do_error_dialog(_("LiVES was unable to load the layout.\nSorry.\n"));
}


LIVES_GLOBAL_INLINE void do_program_not_found_error(const char *progname) {
  do_error_dialogf(_("The program %s is required to use this feature.\nPlease install it and try again."), progname);
}


LIVES_GLOBAL_INLINE void do_lb_composite_error(void) {
  do_error_dialog(
    _("LiVES currently requires composite from ImageMagick to do letterboxing.\n"
      "Please install 'imagemagick' and try again."));
}


LIVES_GLOBAL_INLINE void do_lb_convert_error(void) {
  do_error_dialog(
    _("LiVES currently requires convert from ImageMagick to do letterboxing.\n"
      "Please install 'imagemagick' and try again."));
}


LIVES_GLOBAL_INLINE boolean do_please_install(const char *info, const char *exec, uint64_t gflags) {
  char *extra = lives_strdup(""), *msg;
  if (gflags & INSTALL_CANLOCAL) {
    lives_free(extra);
    extra = lives_strdup(_("\n\nAlternately, LiVES may be able to install\na local user copy "
                           "of the program.\n"));
  } else {
    if (capable->distro_name && *capable->distro_name) {
      char *icmd = get_install_cmd(capable->distro_name, exec);
      if (icmd) {
        lives_free(extra);
        extra = lives_strdup_printf(_("\n\nTry '%s' from a terminal window,\n"
                                      "or use your software package manager to install that package\n"), icmd);
        lives_free(icmd);
      }
    }
  }

  msg = lives_strdup_printf(_("%s'%s' is necessary for this feature to work.\n"
                              "If possible, kindly install %s before continuing.%s"), info, exec, exec, extra);

  if (gflags & INSTALL_CANLOCAL) {
    LiVESWidget *dlg = create_question_dialog(NULL, msg);
    LiVESResponseType ret;
    lives_free(msg);
    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_CANCEL,
                                       _("Cancel / Install Later"), LIVES_RESPONSE_CANCEL);
    lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_ADD,
                                       _("Continue"), LIVES_RESPONSE_YES);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;

    lives_dialog_set_button_layout(LIVES_DIALOG(dlg), LIVES_BUTTONBOX_SPREAD);

    ret = lives_dialog_run(LIVES_DIALOG(dlg));
    lives_widget_destroy(dlg);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    return (ret == LIVES_RESPONSE_YES);
  }
  do_info_dialog(msg);
  lives_free(msg);
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean do_please_install_either(const char *exec, const char *exec2) {
  do_info_dialogf(_("Either '%s' or '%s' must be installed for this feature to work.\n"
                    "If possible, kindly install one or other of these before continuing\n"),
                  exec, exec2);
  return FALSE;
}


LIVES_GLOBAL_INLINE void do_audrate_error_dialog(void) {
  do_error_dialog(_("\n\nAudio rate must be greater than 0.\n"));
}


LIVES_GLOBAL_INLINE boolean do_event_list_warning(void) {
  return do_yesno_dialog(
           _("\nEvent list will be very large\nand may take a long time to display.\n"
             "Are you sure you wish to view it ?\n"));
}


LIVES_GLOBAL_INLINE void do_dvgrab_error(void) {
  do_error_dialog(_("\n\nYou must install 'dvgrab' to use this function.\n"));
}


LIVES_GLOBAL_INLINE void do_nojack_rec_error(void) {
  do_error_dialog(
    _("\n\nAudio recording can only be done using either\nthe \"jack\" "
      "or the \"pulseaudio\" audio player.\n"
      "You may need to select one of these in Tools/Preferences/Playback.\n"));
}


LIVES_GLOBAL_INLINE void do_vpp_palette_error(void) {
  do_error_dialog(_("Video playback plugin failed to initialise palette !\n"));
}


LIVES_GLOBAL_INLINE void do_decoder_palette_error(void) {
  do_error_dialog(_("Decoder plugin failed to initialise palette !\n"));
}


LIVES_GLOBAL_INLINE void do_vpp_fps_error(void) {
  do_error_dialog(_("Unable to set framerate of video plugin\n"));
}


LIVES_GLOBAL_INLINE void do_after_crash_warning(void) {
  if (prefs->vj_mode || prefs->show_dev_opts) return;
  do_error_dialog_with_check(_("After a crash, it is advisable to clean up the disk with\nFile|Clean up disk space\n"),
                             WARN_MASK_CLEAN_AFTER_CRASH);
}


LIVES_GLOBAL_INLINE void do_after_invalid_warning(void) {
  do_error_dialog_with_check(_("Invalid clips were detected during reload.\nIt is advisable to clean up the disk with\n"
                               "File|Clean up disk space\n"),
                             WARN_MASK_CLEAN_INVALID);
}


LIVES_GLOBAL_INLINE void do_rmem_max_error(int size) {
  do_error_dialogf(_("Stream frame size is too large for your network buffers.\nYou should do the following as root:\n\n"
                     "echo %d > /proc/sys/net/core/rmem_max\n"), size);
}

static LiVESList *tdlglist = NULL;

void threaded_dialog_push(void) {
  if (mainw->proc_ptr) {
    tdlglist = lives_list_prepend(tdlglist, mainw->proc_ptr);
    if (mainw->proc_ptr->processing) lives_widget_hide(mainw->proc_ptr->processing);
    mainw->proc_ptr = NULL;
  }
  mainw->threaded_dialog = FALSE;
}

void threaded_dialog_pop(void) {
  end_threaded_dialog();
  if (tdlglist) {
    LiVESList *xtdlglist;
    mainw->proc_ptr = (xprocess *)tdlglist->data;
    xtdlglist = tdlglist;
    tdlglist = tdlglist->next;
    if (tdlglist) tdlglist->prev = NULL;
    xtdlglist->next = NULL;
    xtdlglist->data = NULL;
    lives_list_free(xtdlglist);
    if (mainw->proc_ptr && mainw->proc_ptr->processing) {
      lives_widget_show(mainw->proc_ptr->processing);
      lives_window_set_modal(LIVES_WINDOW(mainw->proc_ptr->processing), TRUE);
      lives_widget_process_updates(mainw->proc_ptr->processing);
      mainw->threaded_dialog = TRUE;
    }
  }
}


static void _threaded_dialog_spin(double fraction) {
  double timesofar;
  int progress;

  if (fraction > 0.) {
    timesofar = (double)(lives_get_current_ticks() - sttime) / TICKS_PER_SECOND_DBL;
    disp_fraction(fraction, timesofar, mainw->proc_ptr);
  } else {
    if (!CURRENT_CLIP_IS_VALID || !mainw->proc_ptr->progress_start || !mainw->proc_ptr->progress_end ||
        *(mainw->msg) || !(progress = atoi(mainw->msg))) {
      // pulse the progress bar
      //#define GDB
#ifndef GDB
      if (LIVES_IS_PROGRESS_BAR(mainw->proc_ptr->progressbar)) {
        lives_progress_bar_pulse(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar));
      }
#endif
    } else {
      // show fraction
      double fraction_done = (double)(progress - mainw->proc_ptr->progress_start)
                             / (double)(mainw->proc_ptr->progress_end - mainw->proc_ptr->progress_start + 1.);
      timesofar = (double)(lives_get_current_ticks() - sttime) / TICKS_PER_SECOND_DBL;
      disp_fraction(fraction_done, timesofar, mainw->proc_ptr);
    }
  }
  // necessary
  lives_widget_context_update();
}


void threaded_dialog_spin(double fraction) {
  if (!mainw->threaded_dialog || mainw->splash_window || !mainw->proc_ptr
      || !mainw->is_ready || !prefs->show_gui) return;
  if (!mainw->is_exiting) {
    if (THREADVAR(no_gui)) return;
    main_thread_execute((lives_funcptr_t)_threaded_dialog_spin, 0,
                        NULL, "d", fraction);
  } else _threaded_dialog_spin(fraction);
}

static void _do_threaded_dialog(const char *trans_text, boolean has_cancel) {
  // calling this causes a threaded progress dialog to appear
  // until end_threaded_dialog() is called
  char *copy_text;
  mainw->cancelled = CANCEL_NONE;
  copy_text = lives_strdup(trans_text);
  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  sttime = lives_get_current_ticks();
  mainw->threaded_dialog = TRUE;
  clear_mainw_msg();
  mainw->proc_ptr = create_threaded_dialog(copy_text, has_cancel, &td_had_focus);
  lives_free(copy_text);
  lives_widget_process_updates(mainw->proc_ptr->processing);
}


void do_threaded_dialog(const char *trans_text, boolean has_cancel) {
  if (!prefs->show_gui) return;
  if (mainw->threaded_dialog || !prefs->show_gui) return;
  if (!mainw->is_exiting)
    main_thread_execute((lives_funcptr_t)_do_threaded_dialog, 0,
                        NULL, "sb", trans_text, has_cancel);
  else
    _do_threaded_dialog(trans_text, has_cancel);
}


static void _end_threaded_dialog(void) {
  if (!mainw->threaded_dialog) return;
  mainw->cancel_type = CANCEL_KILL;

  if (mainw->proc_ptr && mainw->proc_ptr->processing) {
    lives_widget_destroy(mainw->proc_ptr->processing);
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);

  lives_freep((void **)&mainw->proc_ptr->text);
  lives_freep((void **)&mainw->proc_ptr);

  mainw->threaded_dialog = FALSE;

  if (prefs->show_msg_area) {
    // TODO
    if (LIVES_IS_WINDOW(LIVES_MAIN_WINDOW_WIDGET)) {
      lives_window_present(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      lives_widget_grab_focus(mainw->msg_area);
      gtk_window_set_focus(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->msg_area);
    }
  }
}

void end_threaded_dialog(void) {
  if (THREADVAR(no_gui)) return;
  if (!mainw->threaded_dialog) return;
  if (!mainw->is_exiting)
    main_thread_execute((lives_funcptr_t)_end_threaded_dialog, 0, NULL, "");
  else _end_threaded_dialog();
}


void response_ok(LiVESButton * button, livespointer user_data) {
  lives_dialog_response(LIVES_DIALOG(lives_widget_get_toplevel(LIVES_WIDGET(button))), LIVES_RESPONSE_OK);
}


LiVESResponseType do_system_failed_error(const char *com, int retval, const char *addinfo,
    boolean can_retry, boolean trysudo) {
  // if can_retry is set, we can return LIVES_RESPONSE_RETRY
  // in all other cases we abort (exit) here.
  // if abort_hook_func() fails with a syserror, we don't show the abort / retry dialog, and we return LIVES_RESPONSE_NONE
  // from the inner call (otherwise we could get stuck in an infinite recursion)
  static boolean norecurse = FALSE;
  char *xcom, *xaddbit, *xbit, *xsudomsg;
  char *msg, *tmp, *emsg, *msgx, *bit;
  char *retstr = lives_strdup_printf("%d", retval >> 8);
  char *bit2 = (retval > 255) ? lives_strdup("") : lives_strdup_printf("[%s]", lives_strerror(retval));
  char *addbit;
  char *dsmsg = lives_strdup("");
  char *sudomsg = lives_strdup("");

  int64_t dsval = capable->ds_used;

  lives_storage_status_t ds = get_storage_status(prefs->workdir, prefs->ds_crit_level, &dsval, 0);
  LiVESResponseType response = LIVES_RESPONSE_NONE;

  capable->ds_free = dsval;

  if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
    lives_free(dsmsg);
    tmp = ds_critical_msg(prefs->workdir, &capable->mountpoint, dsval);
    dsmsg = lives_strdup_printf("%s\n", tmp);
    lives_free(tmp);
  }

  if (addinfo) addbit = lives_strdup_printf(_("Additional info: %s\n"), addinfo);
  else addbit = lives_strdup("");

  if (retval > 0) bit = lives_strdup_printf(_("The error value was %d%s\n"), retval, bit2);
  else bit = lives_strdup("");

  if (trysudo) {
    char *retryop;
    if (can_retry) retryop = (_("before clicking 'Retry'"));
    else retryop = (_("before retrying the operation"));
    lives_free(sudomsg);
    sudomsg = lives_strdup_printf(_("\n\nYou may be able to fix this by running:\n  %s %s\n"
                                    "from the commandline %s"), EXEC_SUDO, com, retryop);
    lives_free(retryop);
  }

  xcom = lives_markup_escape_text(com, -1);
  xbit = lives_markup_escape_text(bit, -1);
  xaddbit = lives_markup_escape_text(addbit, -1);
  xsudomsg = lives_markup_escape_text(sudomsg, -1);

  msg = lives_strdup_printf(_("\nLiVES failed doing the following:\n%s\nPlease check your system for "
                              "errors.\n%s%s%s"),
                            xcom, xbit, xaddbit, dsmsg, xsudomsg);

  lives_free(xcom); lives_free(xbit); lives_free(xaddbit); lives_free(xsudomsg);
  emsg = lives_strdup_printf("Command failed doing\n%s\n%s%s", com, bit, addbit);
  d_print("\n"); d_print(emsg);
  LIVES_ERROR(emsg);
  lives_free(emsg);

  msgx = insert_newlines(msg, MAX_MSG_WIDTH_CHARS);
  if (can_retry) {
    if (!norecurse) {
      /// we must not fail during the abort hook
      norecurse = TRUE;
      widget_opts.use_markup = TRUE;
      response = do_abort_retry_dialog(msgx);
      widget_opts.use_markup = FALSE;
      norecurse = FALSE;
    }
  } else {
    widget_opts.use_markup = TRUE;
    do_error_dialog(msgx);
    widget_opts.use_markup = FALSE;
  }
  lives_free(msgx); lives_free(msg); lives_free(sudomsg);
  lives_free(dsmsg); lives_free(bit); lives_free(bit2);
  lives_free(addbit); lives_free(retstr);
  return response;
}


void do_write_failed_error_s(const char *s, const char *addinfo) {
  char *msg, *emsg;
  char *addbit, *tmp;
  char *dsmsg = lives_strdup("");

  char dirname[PATH_MAX];
  char *sutf = lives_filename_to_utf8(s, -1, NULL, NULL, NULL), *xsutf, *xaddbit;

  boolean exists;

  int64_t dsval = capable->ds_used;

  lives_storage_status_t ds;

  lives_snprintf(dirname, PATH_MAX, "%s", s);
  get_dirname(dirname);
  exists = lives_file_test(dirname, LIVES_FILE_TEST_EXISTS);
  ds = get_storage_status(dirname, prefs->ds_crit_level, &dsval, 0);
  capable->ds_free = dsval;
  if (!exists) lives_rmdir(dirname, FALSE);

  if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
    lives_free(dsmsg);
    tmp = ds_critical_msg(dirname, &capable->mountpoint, dsval);
    dsmsg = lives_strdup_printf("%s\n", tmp);
    lives_free(tmp);
  }

  if (addinfo) addbit = lives_strdup_printf(_("Additional info: %s\n"), addinfo);
  else addbit = lives_strdup("");

  xsutf = lives_markup_escape_text(sutf, -1);
  lives_free(sutf);

  xaddbit = lives_markup_escape_text(addbit, -1);

  msg = lives_strdup_printf(_("\nLiVES was unable to write to the file\n%s\n"
                              "Please check for possible error causes.\n%s"),
                            xsutf, xaddbit, dsmsg);
  lives_free(xsutf); lives_free(xaddbit);

  emsg = lives_strdup_printf("Unable to write to file\n%s\n%s", s, addbit);
  lives_free(addbit);
  d_print("\n"); d_print(emsg);

  LIVES_ERROR(emsg);
  lives_free(emsg);

  widget_opts.use_markup = TRUE;
  do_error_dialog(msg);
  widget_opts.use_markup = FALSE;
  lives_free(addbit); lives_free(dsmsg); lives_free(msg);
}


void do_read_failed_error_s(const char *s, const char *addinfo) {
  char *msg, *emsg;
  char *addbit;
  char *sutf = lives_filename_to_utf8(s, -1, NULL, NULL, NULL);

  if (addinfo) addbit = lives_strdup_printf(_("Additional info: %s\n"), addinfo);
  else addbit = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES was unable to read from the file\n%s\n"
                              "Please check for possible error causes.\n%s"),
                            sutf, addbit);
  emsg = lives_strdup_printf("Unable to read from the file\n%s\n%s", s, addbit);
  d_print("\n"); d_print(emsg);

  LIVES_ERROR(emsg);
  lives_free(emsg);
  lives_free(sutf);

  do_error_dialog(msg);
  lives_free(msg);
  lives_free(addbit);
}


LiVESResponseType do_write_failed_error_s_with_retry(const char *fname, const char *errtext) {
  // err can be errno from open/fopen etc.

  // return same as do_abort_cancel_retry_dialog() - LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY (both non-zero)

  LiVESResponseType ret;
  char *msg, *emsg, *tmp;
  char *sutf = lives_filename_to_utf8(fname, -1, NULL, NULL, NULL), *xsutf;
  char *dsmsg = lives_strdup("");

  char dirname[PATH_MAX];

  boolean exists;

  int64_t dsval = capable->ds_used;

  lives_storage_status_t ds;

  lives_snprintf(dirname, PATH_MAX, "%s", fname);
  get_dirname(dirname);
  exists = lives_file_test(dirname, LIVES_FILE_TEST_EXISTS);
  ds = get_storage_status(dirname, prefs->ds_crit_level, &dsval, 0);
  capable->ds_free = dsval;
  if (!exists) lives_rmdir(dirname, FALSE);

  if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
    lives_free(dsmsg);
    tmp = ds_critical_msg(dirname, &capable->mountpoint, dsval);
    dsmsg = lives_strdup_printf("%s\n", tmp);
    lives_free(tmp);
  }

  xsutf = lives_markup_escape_text(sutf, -1);

  if (errtext) {
    emsg = lives_strdup_printf("Unable to write to file %s", fname);
    msg = lives_strdup_printf(_("\nLiVES was unable to write to the file\n%s\n"
                                "Please check for possible error causes.\n%s"), xsutf, dsmsg);
  } else {
    char *xerrtext = lives_markup_escape_text(errtext, -1);
    emsg = lives_strdup_printf("Unable to write to file %s, error was %s", fname, errtext);
    msg = lives_strdup_printf(_("\nLiVES was unable to write to the file\n%s\nThe error was\n%s.\n%s"),
                              xsutf, xerrtext, dsmsg);
    lives_free(xerrtext);
  }

  lives_free(xsutf);
  LIVES_ERROR(emsg);
  lives_free(emsg);

  widget_opts.use_markup = TRUE;
  ret = do_abort_cancel_retry_dialog(msg);
  widget_opts.use_markup = FALSE;

  lives_free(dsmsg);
  lives_free(msg);
  lives_free(sutf);

  THREADVAR(write_failed) = FALSE; // reset this

  return ret;
}


LiVESResponseType do_read_failed_error_s_with_retry(const char *fname, const char *errtext) {
  // err can be errno from open/fopen etc.

  // return same as do_abort_cancel_retry_dialog() - LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY (both non-zero)

  LiVESResponseType ret;
  char *msg, *emsg;
  char *sutf = lives_filename_to_utf8(fname, -1, NULL, NULL, NULL);

  if (!errtext) {
    emsg = lives_strdup_printf("Unable to read from file %s", fname);
    msg = lives_strdup_printf(_("\nLiVES was unable to read from the file\n%s\n"
                                "Please check for possible error causes.\n"), sutf);
  } else {
    emsg = lives_strdup_printf("Unable to read from file %s, error was %s", fname, errtext);
    msg = lives_strdup_printf(_("\nLiVES was unable to read from the file\n%s\nThe error was\n%s.\n"),
                              sutf, errtext);
  }

  LIVES_ERROR(emsg);
  lives_free(emsg);

  ret = do_abort_cancel_retry_dialog(msg);

  lives_free(msg);
  lives_free(sutf);

  THREADVAR(read_failed) = FALSE; // reset this

  return ret;
}


LiVESResponseType do_header_read_error_with_retry(int clip) {
  LiVESResponseType ret;
  char *hname;
  if (!mainw->files[clip]) return 0;

  hname = lives_build_filename(prefs->workdir, mainw->files[clip]->handle, LIVES_CLIP_HEADER, NULL);

  ret = do_read_failed_error_s_with_retry(hname, NULL);

  lives_free(hname);
  return ret;
}


boolean do_header_write_error(int clip) {
  // returns TRUE if we manage to clear the error

  char *hname;
  LiVESResponseType retval;

  if (mainw->files[clip] == NULL) return TRUE;

  hname = lives_build_filename(prefs->workdir, mainw->files[clip]->handle, LIVES_CLIP_HEADER, NULL);
  retval = do_write_failed_error_s_with_retry(hname, NULL);
  if (retval == LIVES_RESPONSE_RETRY && save_clip_values(clip)) retval = 0; // on retry try to save all values
  lives_free(hname);

  return (!retval);
}


LiVESResponseType do_header_missing_detail_error(int clip, lives_clip_details_t detail) {
  LiVESResponseType ret;
  char *hname, *key, *msg;
  if (!mainw->files[clip]) return 0;

  hname = lives_build_filename(prefs->workdir, mainw->files[clip]->handle, LIVES_CLIP_HEADER, NULL);

  key = clip_detail_to_string(detail, NULL);

  if (!key) {
    msg = lives_strdup_printf("Invalid detail %d requested from file %s", detail, hname);
    LIVES_ERROR(msg);
    lives_free(msg);
    lives_free(hname);
    return 0;
  }

  msg = lives_strdup_printf(_("Value for \"%s\" could not be read."), key);
  ret = do_read_failed_error_s_with_retry(hname, msg);

  lives_free(msg);
  lives_free(key);
  lives_free(hname);
  return ret;
}


void do_chdir_failed_error(const char *dir) {
  char *dutf, *msg, *emsg = lives_strdup_printf("Failed directory change to\n%s", dir);
  LIVES_ERROR(emsg);
  lives_free(emsg);
  dutf = lives_filename_to_utf8(dir, -1, NULL, NULL, NULL);
  msg = lives_strdup_printf(_("\nLiVES failed to change directory to\n%s\n"
                              "Please check your system for errors.\n"), dutf);
  lives_free(dutf);
  do_abort_ok_dialog(msg);
  lives_free(msg);
}


LiVESResponseType  do_file_perm_error(const char *file_name, boolean allow_cancel) {
  LiVESResponseType resp;
  char *msg, *can_cancel;
  if (allow_cancel)
    can_cancel = (_(", click Cancel to continue regardless,\n"));
  else
    can_cancel = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES was unable to write to the file:\n%s\n"
                              "Please check the file permissions and try again."
                              "%sor click Abort to exit from LiVES"), file_name, can_cancel);
  if (!allow_cancel)
    resp = do_abort_retry_dialog(msg);
  else
    resp = do_abort_cancel_retry_dialog(msg);
  lives_free(msg);
  return resp;
}


LiVESResponseType do_dir_perm_error(const char *dir_name, boolean allow_cancel) {
  LiVESResponseType resp;
  char *msg, *can_cancel;
  if (allow_cancel)
    can_cancel = (_("click Cancel to continue regardless, "));
  else
    can_cancel = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES was unable to either create or write to the directory:\n%s\n"
                              "Please check the directory permissions and try again,\n"
                              "%sor click Abort to exit from LiVES"), dir_name, can_cancel);
  lives_free(can_cancel);

  if (!allow_cancel)
    resp = do_abort_retry_dialog(msg);
  else
    resp = do_abort_cancel_retry_dialog(msg);

  lives_free(msg);
  return resp;
}


void do_dir_perm_access_error(const char *dir_name) {
  char *msg = lives_strdup_printf(_("\nLiVES was unable to read from the directory:\n%s\n"), dir_name);
  do_abort_ok_dialog(msg);
  lives_free(msg);
}


boolean do_abort_check(void) {
  return do_yesno_dialog(_("\nAbort and exit immediately from LiVES\nAre you sure ?\n"));
}


void do_encoder_img_fmt_error(render_details * rdet) {
  do_error_dialogf(_("\nThe %s cannot encode clips with image type %s.\n"
                     "Please select another encoder from the list.\n"),
                   prefs->encoder.name, get_image_ext_for_type(cfile->img_type));
}


LIVES_GLOBAL_INLINE void do_card_in_use_error(void) {
  do_error_dialog(_("\nThis card is already in use and cannot be opened multiple times.\n"));
}


LIVES_GLOBAL_INLINE void do_dev_busy_error(const char *devstr) {
  do_error_dialogf(_("\nThe device %s is in use or unavailable.\n"
                     "- Check the device permissions\n"
                     "- Check if this device is in use by another program.\n"
                     "- Check if the device actually exists.\n"), devstr);
}


LIVES_GLOBAL_INLINE boolean do_existing_subs_warning(void) {
  return do_yesno_dialog(_("\nThis file already has subtitles loaded.\n"
                           "Do you wish to overwrite the existing subtitles ?\n"));
}


void do_invalid_subs_error(void) {
  char *msg = (_("\nLiVES currently only supports subtitles of type"));
  char *tmp;

  char *filt[] = LIVES_SUBS_FILTER;

  register int i = 0;

  while (filt[i]) {
    if (!filt[i + 1]) {
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


LIVES_GLOBAL_INLINE boolean do_erase_subs_warning(void) {
  return do_yesno_dialog(_("\nErase all subtitles from this clip.\nAre you sure ?\n"));
}


boolean do_sub_type_warning(const char *ext, const char *type_ext) {
  boolean ret;
  char *msg = lives_strdup_printf(
                _("\nLiVES does not recognise the subtitle file type \"%s\".\n"
                  "Click Cancel to set another file name\nor OK to continue and save as type \"%s\"\n"),
                ext, type_ext);
  ret = do_warning_dialogf(msg);
  lives_free(msg);
  return ret;
}


LIVES_GLOBAL_INLINE boolean do_move_workdir_dialog(void) {
  return do_yesno_dialogf(_("\nThe contents of the working directory currently located in\n%s\n"
                            "are about to moved to the new working directory at\n%s\n"
                            "Would you like for LiVES to check and clean the contents of the current directory\n"
                            "prior to transferring them ?"), prefs->workdir, future_prefs->workdir);
}


LIVES_GLOBAL_INLINE boolean do_set_locked_warning(const char *setname) {
  return do_yesno_dialogf(
           _("\nWarning - the set %s\nis in use by another copy of LiVES.\n"
             "You are strongly advised to close the other copy before clicking Yes to continue\n.\n"
             "Click No to cancel loading the set.\n"),
           setname);
}


LIVES_GLOBAL_INLINE void do_no_sets_dialog(const char *dir) {
  extra_cb_key = 1;
  do_info_dialogf(_("No Sets could be found in the directory\n%s\n\n"
                    "If you have Sets in another directory, you can either:\n"
                    " - change the working directory in Preferences, or\n"
                    " - restart lives with the -workdir switch to set it temporarily"),
                  dir);
}


boolean do_foundclips_query(void) {
  char *text = (_("Possible lost clips were detected within the LiVES working directory.\n"
                  "What would you like me to do with them ?\n"));
  char *title = (_("Missing Clips Detected"));
  LiVESWidget *dlg = create_question_dialog(title, text);
  LiVESResponseType ret;
  lives_free(text); lives_free(title);
  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_CLEAR,
                                     _("Maybe later"), LIVES_RESPONSE_NO);
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_REMOVE,
                                     _("Try to recover them"), LIVES_RESPONSE_YES);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  lives_dialog_set_button_layout(LIVES_DIALOG(dlg), LIVES_BUTTONBOX_SPREAD);

  ret = lives_dialog_run(LIVES_DIALOG(dlg));
  lives_widget_destroy(dlg);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  return (ret == LIVES_RESPONSE_YES);
}


LIVES_GLOBAL_INLINE void do_no_in_vdevs_error(void) {
  do_error_dialog(_("\nNo video input devices could be found.\n"));
}


LIVES_GLOBAL_INLINE void do_locked_in_vdevs_error(void) {
  do_error_dialog(_("\nAll video input devices are already in use.\n"));
}


LIVES_GLOBAL_INLINE void do_do_not_close_d(void) {
  char *msg = (_("\n\nCLEANING AND COPYING FILES. THIS MAY TAKE SOME TIME.\nDO NOT SHUT DOWN OR "
                 "CLOSE LIVES !\n"));
  create_message_dialog(LIVES_DIALOG_WARN, msg, 0);
  lives_free(msg);
}


LIVES_GLOBAL_INLINE LiVESResponseType do_resize_dlg(int cwidth, int cheight, int fwidth, int fheight) {
  LiVESWidget *butt;
  LiVESResponseType resp;
  char *text = lives_strdup_printf(_("Some frames in this clip may be wrongly sized.\n"
                                     "The clip size is %d X %d, however at least one frame has size %d X %d\n"
                                     "What would you like to do ?"), cwidth, cheight, fwidth, fheight);

  LiVESWidget *dialog = create_question_dialog(_("Problem Detected"), text);

  lives_free(text);

  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, _("Continue anyway"),
                                     LIVES_RESPONSE_CANCEL);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CLEAR, _("Use the image size"),
                                     LIVES_RESPONSE_ACCEPT);

  butt = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REVERT_TO_SAVED,
         _("Resize images to clip size"),
         LIVES_RESPONSE_YES);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_button_grab_default_special(butt);

  lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_EXPAND);
  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  lives_widget_destroy(dialog);
  lives_widget_context_update();
  return resp;
}


LIVES_GLOBAL_INLINE LiVESResponseType do_imgfmts_error(lives_img_type_t imgtype) {
  LiVESResponseType resp;
  char *text = lives_strdup_printf(_("Some frames in this clip have the wrong image format.\n"
                                     "The image format should be %s\n"
                                     "What would you like to do ?"),
                                   image_ext_to_lives_image_type(get_image_ext_for_type(imgtype)));

  LiVESWidget *dialog = create_question_dialog(_("Problem Detected"), text);

  lives_free(text);

  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, _("Continue anyway"),
                                     LIVES_RESPONSE_CANCEL);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REVERT_TO_SAVED, _("Correct the images"),
                                     LIVES_RESPONSE_OK);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  lives_widget_destroy(dialog);
  return resp;
}


LIVES_GLOBAL_INLINE void do_bad_theme_error(const char *themefile) {
  do_error_dialogf(_("\nThe theme file %s has missing elements.\n"
                     "The theme could not be loaded correctly.\n"), themefile);
}


LIVES_GLOBAL_INLINE void do_set_noclips_error(const char *setname) {
  char *msg = lives_strdup_printf(
                _("No clips were recovered for set (%s).\n"
                  "Please check the spelling of the set name and try again.\n"),
                setname);
  d_print(msg);
  lives_free(msg);
}


LIVES_GLOBAL_INLINE char *get_upd_msg(void) {
  LIVES_DEBUG("upd msg !");
  // TRANSLATORS: make sure the menu text matches what is in gui.c
  char *msg = lives_strdup_printf(_("\nWelcome to LiVES version %s\n\n"
                                    "After upgrading, you are advised to run:"
                                    "\n\nFiles -> Clean up Diskspace\n"), LiVES_VERSION);
  return msg;
}


LIVES_GLOBAL_INLINE void do_no_autolives_error(void) {
  do_error_dialogf(_("\nYou must have %s installed and in your path to use this toy.\n"
                     "Consult your package distributor.\n"),
                   EXEC_AUTOLIVES_PL);
}


LIVES_GLOBAL_INLINE void do_exec_missing_error(const char *execname) {
  do_error_dialogf(
    _("\n\nLiVES was unable to find the program %s.\n"
      "Please check this program is in your path and executable.\n"),
    execname);
}


LIVES_GLOBAL_INLINE void do_autolives_needs_clips_error(void) {
  do_error_dialog(_("\nYou must have a minimum of one clip loaded to use this feature.\n"));
}


LIVES_GLOBAL_INLINE void do_jack_lost_conn_error(void) {
  do_error_dialog(_("\nLiVES lost its connection to jack and was unable to reconnect.\n"
                    "Restarting LiVES is recommended.\n"));
}


LIVES_GLOBAL_INLINE void do_pulse_lost_conn_error(void) {
  do_error_dialog(
    _("\nLiVES lost its connection to pulseaudio and was unable to reconnect.\n"
      "Restarting LiVES is recommended.\n"));
}


LIVES_GLOBAL_INLINE void do_cd_error_dialog(void) {
  do_error_dialog(_("Please set your CD play device in Tools | Preferences | Misc\n"));
}


LIVES_GLOBAL_INLINE void do_bad_theme_import_error(const char *theme_file) {
  do_error_dialogf(_("\nLiVES was unable to import the theme file\n%s\n(Theme name not found).\n"),
                   theme_file);
}


LIVES_GLOBAL_INLINE boolean do_close_changed_warn(void) {
  extra_cb_key = del_cb_key = 2;
  return do_warning_dialog(_("Changes made to this clip have not been saved or backed up.\n\n"
                             "Really close it ?"));
}


LIVES_GLOBAL_INLINE boolean check_del_workdir(const char *dirname) {
  return do_yesno_dialogf_with_countdown(3, "%s",  _("All files will be irrevocably deleted from the working directory\n%s\n"
                                         "Are you certain you wish to continue ?\n"
                                         "Click 3 times on the 'Yes' button to confirm you understand\n"));
}


LIVES_GLOBAL_INLINE char *workdir_ch_warning(void) {
  return lives_strdup(
           _("<big>You have chosen to change the working directory.\n"
             "<b>Please make sure you have no other copies of LiVES open before proceeding.</b>\n\n"
             "You can opt whether to move the contents of the old directory into the new one, "
             "leave them where thay are, or delete them.\nPlease choose from the options below.\n"
             "or clicking on Cancel to leave the current value unchanged.</big>"));
}


LIVES_GLOBAL_INLINE void do_shutdown_msg(void) {
  do_info_dialog(
    _("\nLiVES will now shut down. You need to restart it for the new "
      "preferences to take effect.\nClick OK to continue.\n"));
}


LIVES_GLOBAL_INLINE boolean do_theme_exists_warn(const char *themename) {
  return do_yesno_dialogf(_("\nA custom theme with the name\n%s\nalready exists. "
                            "Would you like to overwrite it ?\n"), themename);
}


void add_resnn_label(LiVESDialog * dialog) {
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(dialog);
  LiVESWidget *label;
  LiVESWidget *hsep = lives_standard_hseparator_new();
  lives_box_pack_first(LIVES_BOX(dialog_vbox), hsep, FALSE, TRUE, 0);
  lives_widget_show(hsep);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new(_(
                                     "\n\nResizing of clips is no longer necessary, "
                                     "as LiVES will internally adjust frame sizes as "
                                     "needed at the appropriate moments.\n\n"
                                     "However, physically reducing the frame size may in some cases "
                                     "lead to improved playback \n"
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
             _("\nLiVES would like to open a local network connection (UDP port %d),\n"
               "to let other applications connect to it.\n"
               "Do you wish to allow this (for this session only) ?\n"),
             prefs->osc_udp_port);
  default:
    break;
  }

  return FALSE;
}


boolean ask_permission_dialog_complex(int what, char **argv, int argc, int offs, const char *sudocom) {
  if (prefs->show_gui) {
    LiVESWidget *dlg;
    LiVESResponseType ret;
    char *text, *title, *prname, *errtxt, *errtxt2, *xsudt;
    char *tmp, *action, *verb;
    int nrem = argc - offs;
    //boolean retry;

    //try_again:
    //retry = FALSE;
    switch (what) {
    case LIVES_PERM_DOWNLOAD_LOCAL:
    case LIVES_PERM_COPY_LOCAL:
      // argv (starting at offs) should have: name_of_package_bin, grant_idx, grant_key,
      // cmds to run, future consequences.
      if (nrem < 4) return FALSE; /// badly formed request, ignore it

      mainw->permmgr = (lives_permmgr_t *)lives_calloc(1, sizeof(lives_permmgr_t));
      mainw->permmgr->idx = atoi(argv[offs + 1]);
      mainw->permmgr->key = lives_strdup(argv[offs + 2]);
      if (nrem >= 4) mainw->permmgr->cmdlist = argv[offs + 3];
      if (nrem >= 5) mainw->permmgr->futures = argv[offs + 4];

      if (sudocom) {
        char *sudotext = lives_strdup_printf(_("Alternately, you can try running\n"
                                               "  %s %s\n from a commandline terminal\n"),
                                             EXEC_SUDO, sudocom);
        xsudt = lives_markup_escape_text(sudotext, -1);
        lives_free(sudotext);
      } else xsudt = lives_strdup("");

      prname = lives_markup_escape_text(argv[offs], -1);
      errtxt = lives_markup_escape_text(argv[3], -1);

      if (errtxt && *errtxt)
        errtxt2 = lives_strdup_printf(_("The following error occurred when running %s:"
                                        "\n\n'%s'\n\n"), prname, errtxt);
      else
        errtxt2 = lives_strdup_printf(_("LiVES was not able to run the programe %s\n"), prname);

      lives_free(errtxt);

      if (what == LIVES_PERM_DOWNLOAD_LOCAL) {
        verb = _("download");
        action = _("by downloading");
      } else {
        verb = _("local installation");
        action = lives_strdup(_("by creating"));
      }

      text = lives_strdup_printf(_("%sYou may need to reinstall or update %s\n\n"
                                   "<b>Alternately, it may be possible to fix this "
                                   "%s an individual copy of the program\n%s to your "
                                   "home directory</b>\n"
                                   "Please consider the options "
                                   "and then decide how to proceed.\n"),
                                 errtxt2, prname, action, prname);

      lives_free(prname); lives_free(xsudt); lives_free(errtxt2); lives_free(action);
      title = (_("Problem Detected"));
      widget_opts.use_markup = TRUE;
      dlg = create_question_dialog(title, text);
      widget_opts.use_markup = FALSE;
      lives_free(title);
      lives_free(text);
      widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;

      lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_CANCEL, NULL,
                                         LIVES_RESPONSE_CANCEL);

      lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_ADD,
                                         (tmp = lives_strdup_printf(_("Proceed with %s"), verb)),
                                         LIVES_RESPONSE_YES);
      lives_free(tmp); lives_free(verb);
      widget_opts.expand = LIVES_EXPAND_DEFAULT;
      ret = lives_dialog_run(LIVES_DIALOG(dlg));
      lives_widget_destroy(dlg);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

      if (ret == LIVES_RESPONSE_CANCEL) {
        lives_freep((void **)&mainw->permmgr->key);
        return FALSE;
      }
      return TRUE;
    default:
      break;
    }
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean do_layout_recover_dialog(void) {
  if (!do_yesno_dialog(
        _("\nLiVES has detected a multitrack layout from a previous session.\n"
          "Would you like to try and recover it ?\n"))) {
    recover_layout_cancelled(TRUE);
    return FALSE;
  } else {
    boolean ret;
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
    //lives_widget_context_update();
    ret = recover_layout();
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    return ret;
  }
}

