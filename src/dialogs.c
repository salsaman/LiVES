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
#include "startup.h"

extern void reset_frame_and_clip_index(void);

static void *extra_cb_data = NULL;
static int extra_cb_key = 0;
static int del_cb_key = 0;

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
  uint64_t wmn = GET_INT64_DATA(togglebutton, WARN_MASK_KEY);

  if (get_warn_mask_state(wmn) == RET_ALWAYS) {
    prefs->always_mask |= wmn;
  }

  if (lives_toggle_button_get_active(togglebutton)) prefs->warning_mask |= wmn;
  else prefs->warning_mask &= ~(LIVES_POINTER_TO_INT(user_data));
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


void add_warn_check(LiVESBox *box, uint64_t warn_mask_number, const char *detail) {
  LiVESWidget *checkbutton;
  char *text, *xdetail = (char *)detail, *tmp;
  warn_mask_state  wmstate;
  boolean defstate = FALSE;

  if (!box || !warn_mask_number) return;

  if (warn_mask_number >= WARN_MASK_DEF_OFF) defstate = TRUE;

  wmstate = get_warn_mask_state(warn_mask_number);

  if (!xdetail) {
    if (wmstate == RET_ALWAYS)
      xdetail = _("Remember my decision and always do this");
    else
      xdetail = _("Do not show this _warning any more");
    defstate = FALSE;
  }

  text = lives_strdup_printf((tmp = _("%s\n(can be turned back on from Preferences/Warnings)")), xdetail);

  checkbutton = lives_standard_check_button_new(text, defstate, LIVES_BOX(box), NULL);
  lives_free(text); lives_free(tmp);
  if (xdetail != detail) lives_free(xdetail);

  SET_INT64_DATA(checkbutton, WARN_MASK_KEY, warn_mask_number);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_warn_mask_toggled), NULL);
}


static void add_clear_ds_button(LiVESDialog *dialog) {
  LiVESWidget *button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CLEAR, _("_Recover disk space"),
                        LIVES_RESPONSE_RETRY);
  if (mainw->tried_ds_recover) lives_widget_set_sensitive(button, FALSE);
  lives_dialog_make_widget_first(LIVES_DIALOG(dialog), button);

  lives_signal_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
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
  lives_standard_expander_new(_("_Show complete details"), _("Hide details"), LIVES_BOX(hbox), vbox);
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
          (tmp = lives_strdup(H_("Scan other directories for LiVES Clip Sets. May take time "
                                 "for some directories."))));
    lives_free(tmp);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), "disp_label", label);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(scan_for_sets), entry);

    layout = lives_layout_new(LIVES_BOX(dialog_vbox));
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_EXTRA_WIDTH;
    lives_layout_add_label(LIVES_LAYOUT(layout), _("If you believe there should be clips in the "
                           "current directory,\nyou can try to recover them by launching\n"
                           "*Clean up Diskspace / Recover Missing Clips* from the File menu.\n"),
                           FALSE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    lives_label_set_selectable(LIVES_LABEL(widget_opts.last_label), TRUE);
    break;
  case 2:
    // add send to trash button
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

LiVESWidget *create_message_dialog(lives_dialog_t diat, const char *text, uint64_t warn_mask_number) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *label;
  LiVESWidget *cancelbutton = NULL;
  LiVESWidget *okbutton = NULL, *defbutton = NULL;

  LiVESWindow *transient = widget_opts.transient;

  char *colref;

  boolean woum = widget_opts.use_markup;
  int cb_key = extra_cb_key;
  int del_key = del_cb_key;

  widget_opts.use_markup = FALSE;
  extra_cb_key = 0;
  del_cb_key = 0;

  if (!transient) transient = get_transient_full();

  // doesnt seem to work...
  //if (*capable->wm_caps.wm_focus) wm_property_set(WM_PROP_NEW_FOCUS, "strict");

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

    lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_EDGE);
    break;

  case LIVES_DIALOG_QUESTION:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_QUESTION,
                                      LIVES_BUTTONS_NONE, NULL);
    // caller will set title and buttons
    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);
    break;

  case LIVES_DIALOG_ABORT_RETRY_CANCEL:
  case LIVES_DIALOG_ABORT_RETRY_IGNORE:
  case LIVES_DIALOG_RETRY_CANCEL:
  case LIVES_DIALOG_ABORT_RETRY:
  case LIVES_DIALOG_ABORT_OK:
  case LIVES_DIALOG_ABORT:
  case LIVES_DIALOG_ABORT_RESTART:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_ERROR, LIVES_BUTTONS_NONE, NULL);

    if (diat != LIVES_DIALOG_RETRY_CANCEL) {
      lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
                                         LIVES_STOCK_CLOSE, LIVES_STOCK_LABEL_ABORT, LIVES_RESPONSE_ABORT);

      if (diat == LIVES_DIALOG_RETRY_CANCEL || diat == LIVES_DIALOG_ABORT_RETRY
          || diat == LIVES_DIALOG_ABORT_RETRY_CANCEL
          || diat == LIVES_DIALOG_ABORT_RETRY_IGNORE) {
        okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH,
                   _("_Retry"), LIVES_RESPONSE_RETRY);
        if (diat == LIVES_DIALOG_ABORT_RETRY_CANCEL) {
          lives_window_set_title(LIVES_WINDOW(dialog), _("File Error"));
          cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                         LIVES_RESPONSE_CANCEL);
        } else if (diat == LIVES_DIALOG_ABORT_RETRY_IGNORE) {
          cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL,
                         _("Ignore or fix the problem"),
                         LIVES_RESPONSE_CANCEL);
        }
      } else {
        if (diat == LIVES_DIALOG_ABORT_OK) {
          okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                     LIVES_RESPONSE_OK);
        } else if (diat == LIVES_DIALOG_ABORT_RESTART) {
          okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH,
                     _("_Restart LiVES"), LIVES_RESPONSE_RESET);
        }
      }
    }
    if (diat == LIVES_DIALOG_RETRY_CANCEL) {
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                     LIVES_RESPONSE_CANCEL);
    }
    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
    break;

  case LIVES_DIALOG_ABORT_SKIP_BROWSE:
  case LIVES_DIALOG_ABORT_CANCEL_BROWSE:
  case LIVES_DIALOG_CANCEL_BROWSE:
  case LIVES_DIALOG_RETRY_SKIP_BROWSE:
  case LIVES_DIALOG_RETRY_CANCEL_BROWSE:
    dialog = lives_message_dialog_new(transient, (LiVESDialogFlags)0, LIVES_MESSAGE_ERROR,
                                      LIVES_BUTTONS_NONE, NULL);

    lives_window_set_title(LIVES_WINDOW(dialog), _("Missing File or Directory"));

    if (diat == LIVES_DIALOG_ABORT_SKIP_BROWSE || diat == LIVES_DIALOG_ABORT_CANCEL_BROWSE) {
      if (!prefs->startup_phase)
        lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
                                           LIVES_STOCK_CLOSE, LIVES_STOCK_LABEL_ABORT, LIVES_RESPONSE_ABORT);
      else
        lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
                                           LIVES_STOCK_CLOSE, _("_Exit Setup"), LIVES_RESPONSE_ABORT);
    }

    if (diat == LIVES_DIALOG_RETRY_SKIP_BROWSE || diat == LIVES_DIALOG_RETRY_CANCEL_BROWSE)
      okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH,
                 _("_Retry"), LIVES_RESPONSE_RETRY);

    if (diat == LIVES_DIALOG_CANCEL_BROWSE || diat == LIVES_DIALOG_RETRY_CANCEL_BROWSE
        || diat == LIVES_DIALOG_ABORT_CANCEL_BROWSE) {
      if (prefs->startup_phase)
        cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK, LIVES_STOCK_LABEL_BACK,
                       LIVES_RESPONSE_CANCEL);
      else
        cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                       LIVES_RESPONSE_CANCEL);
    }

    if (diat == LIVES_DIALOG_RETRY_SKIP_BROWSE || diat == LIVES_DIALOG_ABORT_SKIP_BROWSE)
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
                     LIVES_STOCK_LABEL_SKIP, LIVES_RESPONSE_CANCEL);

    lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OPEN,
                                       LIVES_STOCK_LABEL_BROWSE, LIVES_RESPONSE_BROWSE);

    if (palette && widget_opts.apply_theme)
      lives_widget_set_fg_color(dialog, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
    break;

  default:
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

  lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_SPREAD);

  widget_opts.use_markup = woum;
  label = lives_standard_formatted_label_new(text);
  widget_opts.use_markup = FALSE;
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

  if (diat == LIVES_DIALOG_YESNO || diat ==  LIVES_DIALOG_ABORT_RESTART)
    lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_EDGE);

  if (mainw && mainw->permmgr && (mainw->permmgr->cmdlist || mainw->permmgr->futures))
    add_perminfo(dialog);

  if (mainw) {
    if (mainw->add_clear_ds_adv) {
      mainw->add_clear_ds_adv = FALSE;
      add_clear_ds_adv(LIVES_BOX(dialog_vbox));
    }

    if (warn_mask_number > 0) add_warn_check(LIVES_BOX(dialog_vbox), warn_mask_number, NULL);

    if (mainw->xlays) add_xlays_widget(LIVES_BOX(dialog_vbox));

    if (mainw->iochan && !widget_opts.non_modal) {
      LiVESWidget *details_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), NULL, _("Show _Details"),
                                    LIVES_RESPONSE_SHOW_DETAILS);

      lives_signal_sync_connect(LIVES_GUI_OBJECT(details_button), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);
    }
  }

  if (cancelbutton) {
    lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);
  }

  lives_widget_show_all(dialog);

  if (transient) {
    if ((widget_opts.non_modal && !(prefs->focus_steal & FOCUS_STEAL_MSG)) ||
        (!widget_opts.non_modal && !(prefs->focus_steal & (FOCUS_STEAL_BLOCKED
                                     | FOCUS_STEAL_MSG))))
      gtk_window_set_focus_on_map(LIVES_WINDOW(dialog), FALSE);
    gtk_window_present(LIVES_WINDOW(dialog));
  }

  if (okbutton && mainw && mainw->iochan) {
    lives_button_grab_default_special(okbutton);
    lives_widget_grab_focus(okbutton);
  } else if (defbutton) {
    lives_button_grab_default_special(defbutton);
    lives_widget_grab_focus(defbutton);
  }

  if (mainw->mgeom)
    lives_window_center(LIVES_WINDOW(dialog));

  if (!widget_opts.non_modal)
    lives_window_set_modal(LIVES_WINDOW(dialog), TRUE);

  if (!transient) {
    pop_to_front(dialog, NULL);
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


boolean do_warning_dialog_with_checkf(uint64_t warn_mask_number, const char *fmt, ...) {
  va_list xargs;
  boolean resb;
  char *textx;
  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  resb = do_warning_dialog_with_check(textx, warn_mask_number);
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

  if (warn_mask_number >= WARN_MASK_DEF_OFF) {
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


LIVES_GLOBAL_INLINE warn_mask_state get_warn_mask_state(uint64_t warn_mask_number) {
  if (warn_mask_number & REMEMBER_MASK) {
    if (warn_mask_number & prefs->always_mask) {
      return (warn_mask_number & prefs->warning_mask ? RET_TRUE : RET_FALSE);
    }
    return RET_ALWAYS;
  }

  if (warn_mask_number >= WARN_MASK_DEF_OFF) {
    if (!(prefs->warning_mask & warn_mask_number)) return RET_TRUE;
  } else {
    if (prefs->warning_mask & warn_mask_number) return RET_TRUE;
  }
  return RET_WARN;
}


boolean do_yesno_dialog_with_check(const char *text, uint64_t warn_mask_number) {
  // show YES/NO, returns TRUE for YES
  // if warning is disabled, does not show the dialog and returns TRUE
  LiVESWidget *warning;
  int response = 1;
  char *mytext;
  warn_mask_state  wmstate = get_warn_mask_state(warn_mask_number);
  if (wmstate == RET_TRUE) return TRUE;
  if (wmstate == -RET_FALSE) return FALSE;

  mytext = lives_strdup(text); // must copy this because of translation issues

  do {
    warning = create_message_dialog(LIVES_DIALOG_YESNO, mytext, warn_mask_number);
    response = lives_dialog_run(LIVES_DIALOG(warning));
    lives_widget_destroy(warning);
  } while (response == LIVES_RESPONSE_RETRY);

  if (wmstate == RET_ALWAYS) {
    if (response == LIVES_RESPONSE_YES) prefs->warning_mask |= warn_mask_number;
    else prefs->warning_mask &= ~warn_mask_number;
  }

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
    else if (rte_window_visible()) transient = LIVES_WINDOW(rte_window);
    else if (LIVES_MAIN_WINDOW_WIDGET && mainw->is_ready) transient = LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET);
  }
  return transient;
}


LiVESResponseType lives_dialog_run_with_countdown(LiVESDialog *dialog, LiVESResponseType target, int nclicks) {
  LiVESWidget *button = lives_dialog_get_widget_for_response(dialog, target);
  char *btext = lives_strdup(lives_standard_button_get_label(LIVES_BUTTON(button)));
  LiVESResponseType response = LIVES_RESPONSE_NONE;
  boolean use_markup = GET_INT_DATA(button, SBUTT_MARKUP_KEY);
  boolean woum = widget_opts.use_markup;
  for (; nclicks > 0; nclicks--) {
    char *tmp = lives_strdup_printf(P_("%s\n(Click %d more time)", "%s\n(Click %d more times)", nclicks),
                                    btext, nclicks);
    widget_opts.use_markup = use_markup;
    lives_standard_button_set_label(LIVES_BUTTON(button), tmp);
    lives_free(tmp);
    widget_opts.use_markup = woum;
    response = lives_dialog_run(LIVES_DIALOG(dialog));
    if (response != target) break;
  }
  return response;
}


boolean do_yesno_dialogf_with_countdown(int nclicks, boolean isyes, const char *fmt, ...) {
  // show Yes/No, returns TRUE if Yes is clicked nclicks times (if isyes if FALSE then 'No' must be clicked)
  LiVESWidget *warning;
  LiVESResponseType response;
  va_list xargs;
  char *textx, *text, *msg;

  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  msg = lives_strdup_printf(_("Click the '%s' button %d times to confirm your choice"),
                            isyes ? STOCK_LABEL_TEXT(YES) : STOCK_LABEL_TEXT(NO), nclicks);
  text = lives_strdup_printf("%s\n%s", textx, msg);
  lives_free(msg);

  lives_free(textx);

  warning = create_message_dialog(LIVES_DIALOG_YESNO, text, 0);
  lives_free(text);

  response = lives_dialog_run_with_countdown(LIVES_DIALOG(warning), isyes ? LIVES_RESPONSE_YES
             : LIVES_RESPONSE_NO, nclicks);

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


void maybe_abort(boolean do_check, LiVESList *restart_opts) {
  //LiVESList *xlists[1];
  LiVESResponseType resp = LIVES_RESPONSE_ABORT;
  //xlists[0] = restart_opts;
  if (do_check)
    resp = do_abort_restart_check(TRUE, NULL);
  if (CURRENT_CLIP_IS_VALID) {
    if (*cfile->handle) {
      // stop any processing
      lives_kill_subprocesses(cfile->handle, TRUE);
    }
  }
  if (resp == LIVES_RESPONSE_RESET) restart_me(restart_opts, NULL);
  lives_abort("User aborted");
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
      maybe_abort(mainw->is_ready && dtype != LIVES_DIALOG_ABORT, NULL);
    }
  } while (response == LIVES_RESPONSE_ABORT);

  return response;
}


// returns LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY
LIVES_GLOBAL_INLINE LiVESResponseType do_abort_retry_cancel_dialog(const char *text) {
  return _do_abort_cancel_retry_dialog(text, LIVES_DIALOG_ABORT_RETRY_CANCEL);
}


// returns LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY
LIVES_GLOBAL_INLINE LiVESResponseType do_abort_retry_ignore_dialog(const char *text) {
  return _do_abort_cancel_retry_dialog(text, LIVES_DIALOG_ABORT_RETRY_IGNORE);
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

  if (warn_mask_number >= WARN_MASK_DEF_OFF) {
    if (!(prefs->warning_mask & warn_mask_number)) return TRUE;
  } else {
    if (prefs->warning_mask & warn_mask_number) return TRUE;
  }

  err_box = create_message_dialog(warn_mask_number == 0 ? LIVES_DIALOG_ERROR :
                                  LIVES_DIALOG_WARN, text, warn_mask_number);

  if (!widget_opts.non_modal) {
    do {
      ret = lives_dialog_run(LIVES_DIALOG(err_box));
    } while (ret == LIVES_RESPONSE_NONE);
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
  char *msg = lives_strdup_printf(_("%s may not be blank.\nClick %s to exit LiVES immediately or %s "
                                    "to continue with the default value."), what,
                                  STOCK_LABEL_TEXT(ABORT), STOCK_LABEL_TEXT(OK));
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
                                    "Please click %s if you wish to exit from LiVES,\n"
                                    "or %s to update the clip details in LiVES and continue anyway.\n"),
                                  (IS_VALID_CLIP(fileno) && *mainw->files[fileno]->name) ? mainw->files[fileno]->name
                                  : "??????", STOCK_LABEL_TEXT(ABORT), STOCK_LABEL_TEXT(OK));
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
                              "Click %s to exit from LiVES, "
                              "%s to try again, or %s to abandon the operation\n"
                              "You may need to close some other applications first.\n"), op, sizestr,
                            STOCK_LABEL_TEXT(ABORT), STOCK_LABEL_TEXT(RETRY), STOCK_LABEL_TEXT(CANCEL));
  lives_free(sizestr);
  response = do_abort_retry_cancel_dialog(msg);
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


static char *remtime_string(double timerem) {
  char *fmtstr, *tstr, *tmp;
  if (timerem < 0.) {
    tstr = _("unknown");
  } else {
    tstr = format_tstr(timerem, 5);
  }
  fmtstr = lives_strdup_printf((tmp = _("Time remaining: %s")), tstr);
  lives_free(tmp);
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
  char *prog_label, *remtstr, *tmp;

  if (fraction_done > 1.) fraction_done = 1.;
  if (fraction_done < 0.) fraction_done = 0.;

  if (fraction_done > disp_fraction_done + .0001) {
    lives_progress_bar_set_fraction(LIVES_PROGRESS_BAR(proc->progressbar), fraction_done);
    disp_fraction_done = fraction_done;
  } else if (fraction_done < disp_fraction_done) disp_fraction_done = fraction_done;

  remtime = timesofar / fraction_done - timesofar;
  remtstr = remtime_string(remtime);
  if (remtime > MIN_NOTIFY_TIME && mainw->proc_ptr->notify_cb) {
    lives_widget_set_opacity(lives_widget_get_parent(mainw->proc_ptr->notify_cb), 1.);
    lives_widget_set_sensitive(mainw->proc_ptr->notify_cb, TRUE);
  }
  prog_label = lives_strdup_printf((tmp = _("\n%d%% done. %s\n")), (int)(fraction_done * 100.), remtstr);
  lives_free(tmp);
  lives_free(remtstr);
  progbar_display(prog_label, FALSE);
  lives_free(prog_label);
}


#define DEF_PROGRESS_SPEED 4.

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
      if (fdim(fraction_done, last_fraction_done) < 1.) progress_count = 0;
      progress_speed = DEF_PROGRESS_SPEED;
      last_fraction_done = fraction_done;
    } else {
      lives_progress_bar_pulse(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar));
      progress_count = 0;
      if (!mainw->is_rendering) progress_speed = DEF_PROGRESS_SPEED / 2.;
    }
  }

  lives_widget_context_update();
}


void update_progress(boolean visible, int clipno) {
  double fraction_done, timesofar;
  static double est_time = 0.;
  lives_clip_t *sfile = mainw->files[clipno];
  char *prog_label;

  if (sfile->opening && sfile->clip_type == CLIP_TYPE_DISK && !sfile->opening_only_audio &&
      (sfile->hsize > 0 || sfile->vsize > 0 || sfile->frames > 0) && (!mainw->effects_paused || !shown_paused_frames)) {
    uint32_t apxl;
    ticks_t currticks = lives_get_current_ticks();
    if ((currticks - last_open_check_ticks) > TICKS_PER_SECOND *
        ((apxl = 10 * get_log2((uint32_t)sfile->opening_frames)) < 50 ? apxl : 50) ||
        (mainw->effects_paused && !shown_paused_frames)) {
      mainw->proc_ptr->frames_done = sfile->end = sfile->opening_frames = get_frame_count(mainw->current_file,
                                     sfile->opening_frames > 1 ? sfile->opening_frames : 1);
      last_open_check_ticks = currticks;
      if (sfile->opening_frames > 1) {
        if (sfile->frames > 0 && sfile->frames != 123456789) {
          fraction_done = (double)(sfile->opening_frames - 1.) / (double)sfile->frames;
          if (fraction_done > 1.) fraction_done = 1.;
          if (!mainw->effects_paused) {
            timesofar = (currticks - proc_start_ticks - mainw->timeout_ticks) / TICKS_PER_SECOND_DBL;
            est_time = timesofar / fraction_done - timesofar;
          }
          lives_progress_bar_set_fraction(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar), fraction_done);

          if (est_time != -1.) {
            char *remtstr = remtime_string(est_time);
            prog_label = lives_strdup_printf(_("\n%d/%d frames opened. %s\n"),
                                             sfile->opening_frames - 1, sfile->frames, remtstr);
            lives_free(remtstr);
          } else prog_label = lives_strdup_printf(_("\n%d/%d frames opened.\n"), sfile->opening_frames - 1, sfile->frames);
        } else {
          lives_progress_bar_pulse(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar));
          prog_label = lives_strdup_printf(_("\n%d frames opened.\n"), sfile->opening_frames - 1);
        }
#ifdef PROGBAR_IS_ENTRY
        widget_opts.justify = LIVES_JUSTIFY_CENTER;
        lives_entry_set_text(LIVES_ENTRY(mainw->proc_ptr->label3), prog_label);
        widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
#else
        lives_label_set_text(LIVES_LABEL(mainw->proc_ptr->label3), prog_label);
#endif
        lives_free(prog_label);
        sfile->start = sfile->end > 0 ? 1 : 0;
        showclipimgs();
      }
    }
    shown_paused_frames = mainw->effects_paused;
  } else {
    lives_nanosleep(LIVES_FORTY_WINKS);
  }
}


// TODO - these functions should be moved to another file

#ifdef USE_GDK_FRAME_CLOCK
static boolean using_gdk_frame_clock;
static GdkFrameClock *gclock;
static void clock_upd(GdkFrameClock * clock, gpointer user_data) {display_ready = TRUE;}
#endif


static boolean accelerators_swapped;

boolean get_accels_swapped(void) {return accelerators_swapped;}

void cancel_process(boolean visible) {
  if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) disk_monitor_forget();
  if (!visible) {
    if (CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_DISK
        && ((mainw->cancelled != CANCEL_NO_MORE_PREVIEW && mainw->cancelled != CANCEL_PREVIEW_FINISHED
             && mainw->cancelled != CANCEL_USER) || !cfile->opening)) {
      lives_rm(cfile->info_file);
    }
    if (mainw->preview_box && !mainw->preview) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Play all"));
  }

  if (accelerators_swapped) {
    if (!mainw->preview) lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play all"));
    if (mainw->proc_ptr) lives_widget_remove_accelerator(mainw->proc_ptr->preview_button,
          mainw->accel_group, LIVES_KEY_p,
          (LiVESXModifierType)0);
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
      if (mainw->proc_ptr->audint_cb
          && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(mainw->proc_ptr->audint_cb))) {
        pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, TRUE);
      }
      if (mainw->proc_ptr->notify_cb
          && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(mainw->proc_ptr->notify_cb))) {
        notify_user(mainw->proc_ptr->text);
      }
      lives_hooks_trigger(NULL, COMPLETED_HOOK);
      lives_freep((void **)&mainw->proc_ptr->text);
      lives_widget_destroy(mainw->proc_ptr->processing);
      mainw->proc_ptr->processing = NULL;;
      lives_widget_context_update();
    }
    lives_free(mainw->proc_ptr);
    mainw->proc_ptr = NULL;
    if (btext) {
      lives_text_view_set_text(mainw->optextview, btext, -1);
      lives_free((char *)btext);
    }
  }

  mainw->is_processing = FALSE;
  if (CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD) {
    // note - for operations to/from clipboard (file 0) we
    // should manually call sensitize() after operation
    sensitize();
  } else mainw->is_processing = mainw->preview;
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

  if (*cfile->staging_dir && lives_strncmp(cfile->info_file, cfile->staging_dir,
      lives_strlen(cfile->staging_dir))) {
    BREAK_ME("Should not processing a clip while in staging !");
    migrate_from_staging(mainw->current_file);
  }

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
      cancel_process(visible);
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
  progress_speed = DEF_PROGRESS_SPEED;
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

    //////////////

    if (cfile->opening && ((prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd) ||
                           (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed)) && !LIVES_IS_PLAYING) {
      if (mainw->preview_box) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Preview"));
      lives_widget_set_tooltip_text(mainw->m_playbutton, _("Preview"));
      lives_widget_add_accelerator(mainw->proc_ptr->preview_button,
                                   LIVES_WIDGET_CLICKED_SIGNAL, mainw->accel_group, LIVES_KEY_p,
                                   (LiVESXModifierType)0, (LiVESAccelFlags)0);
      accelerators_swapped = TRUE;
    }
  }

  // mainw->origsecs, mainw->orignsecs is our base for quantising
  // (and is constant for each playback session)

  // startticks is the ticks value of the last frame played

  last_open_check_ticks = mainw->offsetticks = 0;

  IF_AREADER_JACK
  (
    if (mainw->record && prefs->audio_src == AUDIO_SRC_EXT &&
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
      cancel_process(visible);
      return FALSE;
    }
  })

  IF_AREADER_PULSE
  (
    // start audio recording now
  if (prefs->audio_src == AUDIO_SRC_EXT) {
  if (mainw->pulsed_read) {
      // valgrind - something undefined here
      pulse_driver_uncork(mainw->pulsed_read);
    }

    if (mainw->record && prefs->ahold_threshold > 0.) {
      cfile->progress_end = 0;
      do_threaded_dialog(_("Waiting for external audio"), TRUE);
      while (mainw->pulsed_read->abs_maxvol_heard < prefs->ahold_threshold && mainw->cancelled == CANCEL_NONE) {
        lives_usleep(prefs->sleep_time);
        threaded_dialog_spin(0.);
        lives_widget_context_update();
      }
      end_threaded_dialog();
      if (mainw->cancelled != CANCEL_NONE) {
        cancel_process(visible);
        return FALSE;
      }
    }
  })

  mainw->scratch = SCRATCH_NONE;
  if (mainw->iochan && mainw->proc_ptr) lives_widget_show_all(mainw->proc_ptr->pause_button);
  display_ready = TRUE;

  if (mainw->record_starting) {
    if (!record_setup(lives_get_current_playback_ticks(mainw->origticks, NULL))) {
      cancel_process(visible);
      return FALSE;
    }
  }

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
  // check rate rather than chans, as we can have chans but no rate during open
  if (cfile->achans && cfile->arps) {
    mainw->aframeno = calc_frame_from_time4(mainw->current_file,
                                            cfile->aseek_pos / cfile->arps / cfile->achans / (cfile->asampsize >> 3));
  }
  frames = cfile->frames;
  cfile->frames = 0; // allow seek beyond video length
  // MUST do re-seek after setting origsecs in order to set our clock properly
  // re-seek to new playback start

  IF_APLAYER_JACK
  (
    if (cfile->achans > 0 && cfile->laudio_time > 0. &&
        !mainw->is_rendering && !(cfile->opening && !mainw->preview) && mainw->jackd
  && mainw->jackd->playing_file > -1) {
  if (!jack_audio_seek_frame(mainw->jackd, mainw->aframeno)) {
      cancel_process(visible);
      return FALSE;
    }

    if (!(mainw->record && (prefs->audio_src == AUDIO_SRC_EXT || mainw->agen_key != 0 || mainw->agen_needs_reinit)))
      jack_get_rec_avals(mainw->jackd);
    else {
      mainw->rec_aclip = mainw->ascrap_file;
      mainw->rec_avel = 1.;
      mainw->rec_aseek = 0;
    }
  })

  IF_APLAYER_PULSE
  (
    if (cfile->achans > 0 && cfile->laudio_time > 0. &&
        !mainw->is_rendering && !(cfile->opening && !mainw->preview) && mainw->pulsed
  && mainw->pulsed->playing_file > -1) {
  if (!pulse_audio_seek_frame(mainw->pulsed, mainw->aframeno)) {
      handle_audio_timeout();
      cancel_process(visible);
      return FALSE;
    }

    if (!(mainw->record && (prefs->audio_src == AUDIO_SRC_EXT || mainw->agen_key != 0 || mainw->agen_needs_reinit)))
      pulse_get_rec_avals(mainw->pulsed);
    else {
      mainw->rec_aclip = mainw->ascrap_file;
      mainw->rec_avel = 1.;
      mainw->rec_aseek = 0;
    }
  })

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

  // must call reset_timebase first, since we set playback ticks
  if (!mainw->foreign && !mainw->multitrack) {
    avsync_force();
  } else mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;

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
        cancel_process(visible);
        return FALSE;
      }

      /* if (mainw->disk_mon & MONITOR_GROWTH) { *\/
        /\* 	int64_t dsused = disk_monitor_check_result(mainw->monitor_dir); *\/
        /\* 	if (dsused >= 0) mainw->monitor_size = dsused; *\/
        /\* 	disk_monitor_start(mainw->monitor_dir); *\/
        /\* } */
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

      if (mainw->render_error == LIVES_RENDER_COMPLETE) goto finish;

      // display progress fraction or pulse bar
      if (*mainw->msg && (frames_done = atoi(mainw->msg)) > 0 && mainw->proc_ptr) {
        if (mainw->msg[lives_strlen(mainw->msg) - 1] == '%')
          mainw->proc_ptr->frac_done = atof(mainw->msg);
        else
          mainw->proc_ptr->frames_done = frames_done;
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
      } //else lives_widget_context_update();
    }

    if (mainw->preview_req) {
      mainw->preview_req = FALSE;
      if (visible) mainw->noswitch = FALSE;
      if (mainw->proc_ptr) on_preview_clicked(LIVES_BUTTON(mainw->proc_ptr->preview_button), NULL);
      if (visible) mainw->noswitch = TRUE;
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
        if (mainw->proc_ptr->stop_button) {
          lives_widget_set_no_show_all(mainw->proc_ptr->stop_button, FALSE);
          lives_widget_show_all(mainw->proc_ptr->stop_button);
        }
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
        //lives_widget_remove_accelerator(mainw->playall, mainw->accel_group, LIVES_KEY_p, (LiVESXModifierType)0);
        ///
        ///
        ///
        lives_widget_add_accelerator(mainw->proc_ptr->preview_button, LIVES_WIDGET_CLICKED_SIGNAL,
                                     mainw->accel_group, LIVES_KEY_p,
                                     (LiVESXModifierType)0, (LiVESAccelFlags)0);
        accelerators_swapped = TRUE;
      }
    }

    //    g_print("MSG is %s\n", mainw->msg);

    if (!*mainw->msg || (lives_strncmp(mainw->msg, "completed", 8) && strncmp(mainw->msg, "error", 5) &&
                         strncmp(mainw->msg, "killed", 6) && (visible ||
                             ((lives_strncmp(mainw->msg, "video_ended", 11) || mainw->whentostop != STOP_ON_VID_END)
                              && (lives_strncmp(mainw->msg, "audio_ended", 11) || mainw->preview ||
                                  mainw->whentostop != STOP_ON_AUD_END))))) {
      // processing not yet completed...
      if (*mainw->msg) {
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
		  }}}}}}}
      // *INDENT-ON*

      // do a processing pass
      if (process_one(visible)) {
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
#ifdef USE_GDK_FRAME_CLOCK
        if (using_gdk_frame_clock) {
          gdk_frame_clock_end_updating(gclock);
        }
#endif
        if (visible) mainw->noswitch = FALSE;
        cancel_process(visible);
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

      if (!mainw->internal_messaging) {
        lives_nanosleep(1000000);
      }
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
  cancel_process(visible);

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

static boolean _do_auto_dialog(const char *text, int type, boolean is_async) {
  // type 0 = normal auto_dialog
  // type 1 = countdown dialog for audio recording
  // type 2 = normal with cancel
  GET_PROC_THREAD_SELF(self);
  FILE *infofile = NULL;
  uint64_t time = 0, stime = 0;

  char *label_text;
  char *mytext = lives_strdup(text);

  double time_rem, last_time_rem = 10000000.;
  lives_alarm_t alarm_handle = 0;

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

  while (mainw->cancelled == CANCEL_NONE && (!self || !lives_proc_thread_get_cancel_requested(self))
         && !(infofile = fopen(cfile->info_file, "r"))) {
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

  if (!mainw->cancelled && (!self || !lives_proc_thread_get_cancel_requested(self))) {
    if (infofile) {
      if (type == 0 || type == 2) {
        read_from_infofile(infofile);
        if (cfile->clip_type == CLIP_TYPE_DISK) lives_rm(cfile->info_file);
        if (alarm_handle > 0) {
          ticks_t tl;
          while ((tl = lives_alarm_check(alarm_handle)) > 0 && !mainw->cancelled) {
            lives_progress_bar_pulse(LIVES_PROGRESS_BAR(mainw->proc_ptr->progressbar));
            lives_widget_process_updates(mainw->proc_ptr->processing);
            // need to recheck after calling process_updates
            if (!mainw->proc_ptr || !mainw->proc_ptr->processing) break;
            lives_nanosleep(LIVES_FORTY_WINKS);
          }
          lives_alarm_clear(alarm_handle);
        }
      } else fclose(infofile);
    }
  }

  if (mainw->proc_ptr) {
    if (mainw->proc_ptr->processing)
      lives_widget_destroy(mainw->proc_ptr->processing);
    lives_freep((void **)&mainw->proc_ptr->text);
    lives_free(mainw->proc_ptr);
    mainw->proc_ptr = NULL;
  }

  if (type == 2) mainw->cancel_type = CANCEL_KILL;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  if (self && lives_proc_thread_get_cancel_requested(self)) {
    lives_proc_thread_cancel(self);
    return FALSE;
  }

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


// start up an auto dialog - processing dialog running in bg thread
// there are two ways to use this
// - launch it before running a backend, then call lives_proc_thread_join on the returned proc_thread
//	it will check for .status file from backend and return when this is present
//
// - launch it, do some processing, then call lives_proc_thread_cancel on the returned proc_thread
//    after calling cancel, again wait for lives_proc_thread_join to return
//
boolean do_auto_dialog(const char *text, int type) {
  // blocking types
  // type 0 = normal auto_dialog
  // type 1 = countdown dialog for audio recording
  // type 2 = normal with cancel
  return _do_auto_dialog(text, type, FALSE);
}


lives_proc_thread_t do_auto_dialog_async(const char *text, int type) {
  // blocking types
  // type 0 = normal auto_dialog
  // type 1 = countdown dialog for audio recording
  // type 2 = normal with cancel
  lives_proc_thread_t lpt = lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)_do_auto_dialog,
                            WEED_SEED_BOOLEAN, "sib", text, type, TRUE);
  return lpt;
}

///// TODO: end processing.c /////

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
    msg_b = lives_strdup_printf(
              _("\nYou will be able to undo these changes afterwards.\n\nClick %s to proceed, %s to abort.\n\n"),
              STOCK_LABEL_TEXT(OK), STOCK_LABEL_TEXT(CANCEL));
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
            "LiVES will pause briefly until frames are received.\nYou should only click %s if you understand what you are doing, "
            "otherwise, click %s."),
          prefs->workdir, STOCK_LABEL_TEXT(OK), STOCK_LABEL_TEXT(CANCEL));
  resp = do_warning_dialog_with_check(msg, WARN_MASK_OPEN_YUV4M);
  lives_free(msg);
  return resp;
}


boolean do_comments_dialog(int fileno, char *filename) {
  lives_clip_t *sfile = mainw->files[fileno];

  boolean response;
  boolean encoding = FALSE;
  _commentsw *commentsw = create_comments_dialog(sfile, filename);

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
  char *text = _dump_messages(-1, -1);
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
  lives_dialog_run(LIVES_DIALOG(textwindow->dialog));
}


static char *expl_missplug(const char *pltype) {
  if (!lives_strcmp(pltype, PLUGIN_ENCODERS)) {
    return (_("\nLiVES was unable to find any encoder plugins.\n"
              "You will not be able to 'Save' (encode) clips without them.\n"));
  }

  if (!lives_strcmp(pltype, PLUGIN_DECODERS)) {
    return (_("\nLiVES was unable to find any decoder plugins.\n"
              "The 'Instant Open' feature cannnot operate without them.\n"));
  }

  if (!lives_strcmp(pltype, PLUGIN_RENDERED_EFFECTS_BUILTIN)) {
    return (_("\nLiVES was unable to find any rendered effect plugins.\n"
              "Some editing tools and frame generators will be unavailable, "
              "and effects will be limited to realtime effects only\n"));
  }

  if (!lives_strcmp(pltype, PLUGIN_VID_PLAYBACK)) {
    return (_("\nLiVES was unable to find any video player plugins.\n"
              "Optimised playback in fullscreen mode, as well as quick transcode and some "
              "streaming features may not be usable without them\n"));
  }
  return lives_strdup("");
}


const char *miss_plugdirs_warn(const char *dirnm, LiVESList * subdirs) {
  // IMPORTANT: part of the funcioning of the warning dialog may cause prefs->lib_dir to change !
  // this always accompanied by its new value being returned
  // in all other cases the pref is not updated, and NULL is returned instead

  char *msg1, *msg2, *msg3 = NULL, *msg4, *msg;
  char *new_libdir;
  LiVESWidget *dlg;
  LiVESResponseType response;

  if (prefs->warning_mask & WARN_MASK_CHECK_PLUGINS) {
    prefs->warning_mask |= WARN_MASK_CHECK_PLUGINS;
    return NULL;
  }

  msg1 = lives_strdup_printf(_("LiVES was unable to find some of its standard plugins "
                               "within the plugin library directory:\n%s\n\n"), dirnm);

  if (lives_list_length(subdirs) == N_PLUGIN_SUBDIRS) {
    msg2 = lives_strdup(_("<big><b>All of the plugin subdirectories appear to be absent, including:</b></big>\n\n"));
  } else {
    msg2 = lives_strdup(_("The following directories could not be found:\n\n"));
  }
  for (LiVESList *list = subdirs; list; list = list->next) {
    char *pltype = subst((const char *)list->data, "|", LIVES_DIR_SEP);
    char *expl = expl_missplug(pltype), *tmp;
    if (msg3)
      tmp = lives_strdup_printf("%s %s%s\n", msg3, pltype, expl);
    else
      tmp = lives_strdup_printf("%s%s\n", pltype, expl);
    lives_free(expl);
    if (msg3) lives_free(msg3);
    msg3 = tmp;
    lives_free(pltype);
  }

  if (!prefs->startup_phase) msg4 = lives_strdup_printf(_("Click %s to exit immediately from LiVES"),
                                      STOCK_LABEL_TEXT(ABORT));
  else msg4 = _("Click 'Exit Setup' to quit from LiVES setup");

  msg = lives_strdup_printf(_("%s%s%s\n%s, %s to continue with the current value, "
                              "or %s to locate the 'plugins' directory.\n"
                              "E.g /usr/local/lib/lives/plugins (the 'plugins' directory MUST always be a subdirectory inside 'lives')"),
                            msg1, msg2, msg3, msg4, STOCK_LABEL_TEXT(SKIP), STOCK_LABEL_TEXT(BROWSE));
  lives_free(msg1); lives_free(msg2); lives_free(msg3); lives_free(msg4);

  while (1) {
    widget_opts.use_markup = TRUE;
    dlg = create_message_dialog(LIVES_DIALOG_CANCEL_BROWSE, msg, WARN_MASK_CHECK_PLUGINS);
    lives_dialog_set_button_layout(LIVES_DIALOG(dlg), LIVES_BUTTONBOX_EDGE);
    widget_opts.use_markup = FALSE;
    if (!mainw->is_ready) pop_to_front(dlg, NULL);
    response = lives_dialog_run(LIVES_DIALOG(dlg));

    lives_widget_destroy(dlg);
    lives_widget_context_update();

    if (response == LIVES_RESPONSE_ABORT) {
      maybe_abort(TRUE, NULL);
      continue;
    }
    if (response == LIVES_RESPONSE_BROWSE) {
      lives_widget_context_update();
      new_libdir = choose_file(prefs->lib_dir, PLUGINS_LITERAL, NULL,
                               LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, NULL);
      if (!new_libdir) continue;
    }
    break;
  }
  lives_free(msg);
  if (mainw->splash_window) {
    lives_widget_show(mainw->splash_window);
  }

  if (response == LIVES_RESPONSE_BROWSE) return new_libdir;
  return NULL;
}


const char *miss_prefix_warn(const char *dirnm, LiVESList * subdirs) {
  // IMPORTANT: part of the funcioning of the warning dialog may cause prefs->prefix_dir to change !
  // this always accompanied by its new value being returned
  // in all other cases the pref is not updated, and NULL is returned instead

  char *msg1, *msg2, *msg3 = NULL, *msg, *msg4, *tmp;
  char *new_prefixdir;
  LiVESWidget *dlg;
  LiVESResponseType response;

  if (prefs->warning_mask & (WARN_MASK_CHECK_PREFIX)) {
    return NULL;
  }

  msg1 = lives_strdup_printf(_("LiVES was unable to find some extra components"
                               " within:\n%s\n\n"), dirnm);
  msg2 = lives_strdup(_("<big><b>For example, the following subdirectories appear to be absent:</b></big>\n\n"));

  for (LiVESList *list = subdirs; list; list = list->next) {
    char *resource = subst((const char *)list->data, "|", LIVES_DIR_SEP);
    if (msg3)
      tmp = lives_strdup_printf("%s %s\n", msg3, resource);
    else
      tmp = lives_strdup_printf("%s\n", resource);
    if (msg3) lives_free(msg3);
    msg3 = tmp;
    lives_free(resource);
  }


  if (!prefs->startup_phase) msg4 = lives_strdup_printf(_("Click %s to exit immediately from LiVES"),
                                      STOCK_LABEL_TEXT(ABORT));
  else msg4 = _("Click 'Exit Setup' to quit from LiVES setup");

  msg = lives_strdup_printf(_("%s%s%s\n%s, %s to continue with the current value,\n"
                              "or %s to locate the prefix directory, "
                              "E.g /usr/local"), msg1, msg2, msg3, msg4,
                            STOCK_LABEL_TEXT(SKIP), STOCK_LABEL_TEXT(BROWSE));
  lives_free(msg1); lives_free(msg2); lives_free(msg3); lives_free(msg4);

  while (1) {
    widget_opts.use_markup = TRUE;
    dlg = create_message_dialog(LIVES_DIALOG_CANCEL_BROWSE, msg, WARN_MASK_CHECK_PREFIX);
    lives_dialog_set_button_layout(LIVES_DIALOG(dlg), LIVES_BUTTONBOX_EDGE);
    widget_opts.use_markup = FALSE;
    if (!mainw->is_ready) pop_to_front(dlg, NULL);
    response = lives_dialog_run(LIVES_DIALOG(dlg));
    lives_widget_destroy(dlg);
    lives_widget_context_update();

    if (response == LIVES_RESPONSE_ABORT) {
      maybe_abort(TRUE, NULL);
      continue;
    }
    if (response == LIVES_RESPONSE_BROWSE) {
      lives_widget_context_update();
      new_prefixdir = choose_file(prefs->prefix_dir, NULL, NULL,
                                  LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, NULL);
      if (!new_prefixdir) continue;
    }
    break;
  }
  lives_free(msg);
  if (mainw->splash_window) {
    lives_widget_show(mainw->splash_window);
  }

  if (response == LIVES_RESPONSE_BROWSE) return new_prefixdir;
  return NULL;
}


void do_audio_import_error(void) {
  char *msg = (_("Sorry, unknown audio type.\n\n (Filenames must end in"));
  char *tmp;

  char *filt[] = LIVES_AUDIO_LOAD_FILTER;

  int i = 0;

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
                                     "Click %s to proceed, or %s to keep the current setting\n\n"
                                     "(Note: the value for the current layout can be modified at any time\n"
                                     "via the menu option 'Tools' / 'Change Width, Height and Audio Values')\n")),
                            endised, endis, STOCK_LABEL_TEXT(YES), STOCK_LABEL_TEXT(NO));
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
    }
    extra = _("could not be found.");
    whatitis = (_("this file"));
  } else {
    if (!detail) {
      xdetail = lives_strdup(_("The directory"));
      extra = _("could not be found.");
    } else extra = lives_strdup("");
    whatitis = (_("this directory"));
  }
  msg = lives_strdup_printf(_("\n%s\n%s\n%s\n"
                              "Click %s to try again, %s to browse to the new location.\n"
                              "otherwise click %s to skip loading %s.\n"), xdetail, dfname, extra,
                            STOCK_LABEL_TEXT(RETRY), STOCK_LABEL_TEXT(BROWSE), STOCK_LABEL_TEXT(SKIP), whatitis);
  warning = create_message_dialog(LIVES_DIALOG_RETRY_SKIP_BROWSE, msg, 0);
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


LIVES_GLOBAL_INLINE void do_no_decoder_error(int clipno) {
  do_delete_or_mark(clipno, 1);

  /* lives_widget_context_update(); */
  /* do_error_dialogf( */
  /*   _("\n\nLiVES could not find a required decoder plugin for the clip\n%s\n" */
  /*     "The clip could not be loaded.\n\n" */
  /*     "The clip will be marked 'ignore' - you can try to recover it using\n" */
  /*     "Files | Clean up Disk Space / Recover files, " */
  /*     "with the option 'Consider Files Marked Ignore' enabled."), fname); */
}


LIVES_GLOBAL_INLINE void do_no_loadfile_error(const char *fname) {
  do_error_dialogf(_("\n\nThe file\n%s\nCould not be found.\n"), fname);
}


LIVES_LOCAL_INLINE char *get_jack_restart_warn(int suggest_opts, const char *srvname) {
  // suggest_opts is a hint for the -jackopts value to use on restart
  // a value of -1 will not add any recommendation
  if (prefs->startup_phase == 0) {
    const char *otherbit;
    char *firstbit, *msg;
    if (suggest_opts != -1) {
      char *suggest_srv;
      char *rsparam = "-jackopts";
      mainw->restart_params = lives_list_append(mainw->restart_params, (void *)rsparam);
      rsparam = lives_strdup_printf("%u", suggest_opts);
      mainw->restart_params = lives_list_append(mainw->restart_params, (void *)rsparam);

      if (!srvname) suggest_srv = "";
      else {
        suggest_srv = lives_strdup_printf(" -jackserver '%s'", srvname);
        rsparam = lives_strdup("-jackserver");
        mainw->restart_params = lives_list_append(mainw->restart_params, (void *)rsparam);
        rsparam = lives_strdup_printf("%s", srvname);
        mainw->restart_params = lives_list_append(mainw->restart_params, (void *)rsparam);
      }
      firstbit = lives_strdup_printf("lives -jackopts %u%s\n(which will allow LiVES to start the server itself), "
                                     "or\n\n", suggest_opts & JACK_OPTS_OPTS_MASK, suggest_srv);
      if (srvname) lives_free(suggest_srv);
    } else firstbit = "";
#ifdef HAVE_PULSE_AUDIO
    if (capable->has_pulse_audio == PRESENT) {
      //char *rsparam = lives_strdup("-aplayer pulse");
      otherbit = "lives -aplayer pulse";
      //mainw->restart_params = lives_list_append(mainw->restart_params, (void *)rsparam);
    } else {
#else
    if (1) {
#endif
      //rsparam = lives_strdup("-aplayer none");
      otherbit = "lives -aplayer none";
      //mainw->restart_params = lives_list_append(mainw->restart_params, (void *)rsparam);
    }
#if 0
  }
#endif
  msg = lives_strdup_printf(_("\nAlternatively, you may start lives with commandline option:\n\n"
                              "%s%s\n(to avoid use of jack audio altogether)"),
                            firstbit, otherbit);
  if (*firstbit) lives_free(firstbit);
  return msg;
}
return lives_strdup("");
}


LIVES_GLOBAL_INLINE void do_jack_restart_warn(int suggest_opts, const char *srvname) {
  char *msg = get_jack_restart_warn(suggest_opts, srvname);
  do_info_dialog(msg);
  lives_free(msg);
}

static LiVESWidget *statbutton, *drvbutton, *srvbutton;

static void diag_button_clicked(LiVESWidget * w, livespointer dta) {
  lives_widget_set_no_show_all(statbutton, FALSE);
  lives_widget_set_no_show_all(drvbutton, FALSE);
  lives_widget_set_no_show_all(srvbutton, FALSE);

  lives_widget_show_all(statbutton);
  lives_widget_show_all(drvbutton);
  lives_widget_show_all(srvbutton);

  lives_widget_set_no_show_all(w, TRUE); // because lives_dlg_run
  lives_widget_hide(w);
}


#ifdef ENABLE_JACK
boolean do_jack_no_startup_warn(boolean is_trans) {
  char *tmp = NULL, *tmp2 = NULL, *tmp3 = NULL, *tmp4, *tmp5, *tmp6, *msg, *msg1;
  boolean chkname = FALSE;
  if ((is_trans && *prefs->jack_tserver_cfg) ||
      (!is_trans && *prefs->jack_aserver_cfg)) {
    char *srvname;

    if (is_trans) {
      srvname = jack_parse_script(prefs->jack_tserver_cfg);
      if (srvname && lives_strcmp(srvname, future_prefs->jack_tserver_cname)) chkname = TRUE;
    } else {
      srvname = jack_parse_script(prefs->jack_aserver_cfg);
      if (srvname && lives_strcmp(srvname, future_prefs->jack_aserver_cname)) chkname = TRUE;
    }
    if (chkname) tmp6 = lives_strdup_printf(_("\n\nPerhaps the server name should be set to '%s' ?\n"),
                                              srvname);
    else tmp6 = lives_strdup(".");

    tmp = lives_strdup_printf(_("jackd using the configuration file\n%s%s"),
                              is_trans ? prefs->jack_tserver_cfg : prefs->jack_aserver_cfg, tmp6);
    lives_free(tmp6);
  } else {
    if (is_trans) {
      if (*future_prefs->jack_tserver_sname) {
        if (!lives_strcmp(future_prefs->jack_tserver_sname, future_prefs->jack_tserver_cname))
          tmp = lives_strdup(_("it"));
        else tmp = lives_strdup_printf(_("the jack server '%s'."), future_prefs->jack_tserver_sname);
      }
    } else {
      if (*future_prefs->jack_aserver_sname) {
        if (!lives_strcmp(future_prefs->jack_aserver_sname, future_prefs->jack_aserver_cname))
          tmp = lives_strdup(_("it"));
        else tmp = lives_strdup_printf(_("the jack server '%s'."), future_prefs->jack_aserver_sname);
      }
    }
  }

  if (is_trans) {
    if (*future_prefs->jack_tserver_cname)
      tmp2 = lives_strdup_printf(_("the jack server '%s'"), future_prefs->jack_tserver_cname);
  } else {
    if (*future_prefs->jack_aserver_cname)
      tmp2 = lives_strdup_printf(_("the jack server '%s'"), future_prefs->jack_aserver_cname);
  }
  if (!tmp2) {
    tmp2 = _("the default jack server");
    if (!tmp) tmp = lives_strdup(_("it"));
  }

  if (!tmp) tmp = _("the default jack server.");

  if (prefs->startup_phase) {
    tmp4 = lives_strdup(_("\nRestarting LiVES will allow you to adjust the settings\n"
                          "or select another audio player.\n"));
  } else tmp4 = lives_strdup("");

  if (!prefs->startup_phase) {
    char *rsparam = "-jackopts";
    mainw->restart_params = lives_list_append(mainw->restart_params, (void *)rsparam);
    rsparam = "0";
    mainw->restart_params = lives_list_append(mainw->restart_params, (void *)rsparam);
    tmp5 = lives_strdup_printf("\n<b>%s</b>\n",
                               _("Automatic jack startup will be disabled for now.\n"
                                 "If you have not done so already, try restarting LiVES."));
  } else tmp5 = lives_strdup("");

  msg1 = lives_strdup_printf(_("LiVES failed to connect the %s client to %s,\n"
                               "and in addition was unable to start %s.\n\n"),
                             is_trans ? _("transport") : _("audio"), tmp2, tmp);
  if (!is_trans && !chkname) msg = lives_strdup_printf(_("%s"
                                     "Please ensure that %s is set up correctly on your machine\n"
                                     "and also that the soundcard is not being blocked by another program, "
                                     "or another instance of jackd.\n\n"
                                     "Also make sure that LiVES is not trying to start up\n"
                                     "a server which is already running.\n\n"
                                     "%s%s"), msg1, (tmp3 = is_trans
                                         ? (future_prefs->jack_tdriver
                                            ? lives_markup_escape_text(future_prefs->jack_tdriver, -1)
                                            : _("audio"))
                                         : (future_prefs->jack_adriver
                                            ? lives_markup_escape_text(future_prefs->jack_adriver, -1)
                                            : _("audio"))), tmp5, tmp4);
  else msg = lives_strdup_printf("%s%s", msg1, tmp5);
  if (msg1) lives_free(msg1);
  if (tmp) lives_free(tmp);
  if (tmp2) lives_free(tmp2);
  if (tmp3) lives_free(tmp3);
  if (tmp4) lives_free(tmp4);
  if (tmp5) lives_free(tmp5);
  if (prefsw || prefs->startup_phase) {
    widget_opts.use_markup = TRUE;
    do_error_dialog(msg);
    widget_opts.use_markup = FALSE;
    lives_free(msg);
  } else {
    LiVESWidget *dlg = lives_standard_dialog_new(_("JACK Startup Error"), FALSE, -1, -1);
    LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dlg));
    LiVESWidget *label, *diagbutton, *cancbutton;

    widget_opts.use_markup = TRUE;
    label = lives_standard_formatted_label_new(msg);
    widget_opts.use_markup = FALSE;
    lives_free(msg);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);

    statbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), NULL, _("View Status _Log"),
                 LIVES_RESPONSE_BROWSE);

    if (!(prefs->jack_opts & JACK_INFO_TEMP_NAMES)) {
      lives_widget_set_no_show_all(statbutton, TRUE);
      lives_widget_hide(statbutton);

      drvbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_PREFERENCES,
                  _("_Driver Settings"), LIVES_RESPONSE_RETRY);

      lives_widget_set_no_show_all(drvbutton, TRUE);
      lives_widget_hide(drvbutton);

      srvbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_PREFERENCES,
                  _("Jack _Server Setup"), LIVES_RESPONSE_RESET);

      lives_widget_set_no_show_all(srvbutton, TRUE);
      lives_widget_hide(srvbutton);

      diagbutton =  lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_PREFERENCES,
                    _("Troubleshooting Options"), LIVES_RESPONSE_NONE);

      lives_signal_sync_connect(LIVES_GUI_OBJECT(diagbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(diag_button_clicked), NULL);
    }
    cancbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_QUIT, _("_Exit / Restart LiVES"),
                 LIVES_RESPONSE_CANCEL);
    lives_window_add_escape(LIVES_WINDOW(dlg), cancbutton);

    lives_widget_grab_focus(cancbutton);
    lives_button_grab_default_special(cancbutton);

    lives_dialog_set_button_layout(LIVES_DIALOG(dlg), LIVES_BUTTONBOX_EDGE);

    while (1) {
      LiVESResponseType resp = lives_dialog_run(LIVES_DIALOG(dlg));
      if (resp == LIVES_RESPONSE_NONE) continue;
      if (resp == LIVES_RESPONSE_BROWSE) {
        lives_widget_hide(dlg);
        lives_widget_context_update();
        show_jack_status(LIVES_BUTTON(statbutton), LIVES_INT_TO_POINTER(2));
        lives_widget_show(dlg);
      } else {
        lives_widget_destroy(dlg);
        if (resp == LIVES_RESPONSE_CANCEL) return FALSE;
        if (resp == LIVES_RESPONSE_RESET) {
          return do_jack_config(2, is_trans);
        }
        return jack_drivers_config(drvbutton, is_trans
                                   ? LIVES_INT_TO_POINTER(2)
                                   : LIVES_INT_TO_POINTER(3));
      }
    }
  }
  return FALSE;
}


boolean do_jack_no_connect_warn(boolean is_trans) {
  char *extra = "", *tmp, *more, *warn, *msg;
  boolean is_bad = FALSE;

  if (is_trans && (future_prefs->jack_opts & JACK_OPTS_START_TSERVER)) {
    if (future_prefs->jack_tdriver && future_prefs->jack_tdriver) {
      extra = lives_strdup_printf(_("\nThere may be a problem with the <b>'%s'</b> driver"),
                                  (tmp = lives_markup_escape_text(future_prefs->jack_tdriver, -1)));
      lives_free(tmp);
    }
  } else {
    if (future_prefs->jack_adriver && future_prefs->jack_adriver) {
      extra = lives_strdup_printf(_("\nThere may be a problem with the <b>'%s'</b> driver."),
                                  (tmp = lives_markup_escape_text(future_prefs->jack_adriver, -1)));
      lives_free(tmp);
    }
  }
  if (is_trans && *future_prefs->jack_tserver_cname)
    tmp = lives_markup_escape_text(future_prefs->jack_tserver_cname, -1);
  else {
    if (!is_trans && *future_prefs->jack_aserver_cname)
      tmp = lives_markup_escape_text(future_prefs->jack_aserver_cname, -1);
    else
      tmp = lives_markup_escape_text(prefs->jack_def_server_name, -1);
  }

  if (mainw->fatal || (!is_trans && (prefs->jack_opts & JACK_OPTS_START_ASERVER))
      || (is_trans && (prefs->jack_opts & JACK_OPTS_START_TSERVER))) {
    warn = _("<b>Something really bad happened with jack...</b>\n\nPlease try restarting LiVES.\n"
             "If that still does not work, please check the jackd status and try restarting it manually.");
    more = lives_strdup("");
    is_bad = TRUE;
  } else {
    if (!prefsw) {
      if (!is_trans) {
        more = get_jack_restart_warn(future_prefs->jack_opts & ~JACK_OPTS_ENABLE_TCLIENT,
                                     *future_prefs->jack_aserver_cname
                                     ? future_prefs->jack_aserver_cname : NULL);
      } else {
        more = get_jack_restart_warn(future_prefs->jack_opts & ~JACK_OPTS_START_ASERVER,
                                     *future_prefs->jack_tserver_cname
                                     ? future_prefs->jack_tserver_cname : NULL);
      }
    } else more = lives_strdup("");

    if (!prefsw && !prefs->startup_phase) {
      warn = lives_strdup_printf(_("<big><b>Please try starting the jack server before restarting LiVES%s</b></big>"),
                                 is_trans ? _("\nJack transport features will be disabled for now, "
                                              "'Preferences / Jack Integration'\n"
                                              "may be selected in order to re-enable them")
                                 : _(", or use the button below to redefine the server setup"));
    } else {
      warn = _("Please ensure the server is running before trying again");
      if (prefs->startup_phase) {
        warn = lives_strcollate(&warn, ", ", _("or else adjust the jack configuration settings"));
      }
    }
  }

  msg = lives_strdup_printf(_("\nLiVES was unable to establish %s to jack server <b>'%s'</b>.%s"
                              "\n\n%s\n%s"),
                            is_trans ? _("a transport client connection") : _("an audio connection"),
                            tmp, extra, warn, more);
  lives_free(tmp); lives_free(more);
  lives_free(warn);
  if (*extra) lives_free(extra);

  if (prefsw || prefs->startup_phase) {
    widget_opts.use_markup = TRUE;
    do_error_dialog(msg);
    widget_opts.use_markup = FALSE;
    lives_free(msg);
  } else {
    LiVESWidget *dlg = lives_standard_dialog_new(_("JACK Startup Error"), FALSE, -1, -1);
    LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dlg));
    LiVESWidget *label, *cancbutton;

    widget_opts.use_markup = TRUE;
    label = lives_standard_formatted_label_new(msg);
    widget_opts.use_markup = FALSE;
    lives_free(msg);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);

    if (!mainw->fatal)
      statbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), NULL, _("View Status _Log"),
                   LIVES_RESPONSE_BROWSE);

    if (!is_bad)
      srvbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_PREFERENCES,
                  _("Jack _Server Setup"), LIVES_RESPONSE_RESET);

    cancbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_QUIT, _("_Exit / Restart LiVES"),
                 LIVES_RESPONSE_CANCEL);

    lives_window_add_escape(LIVES_WINDOW(dlg), cancbutton);

    lives_widget_grab_focus(cancbutton);
    lives_button_grab_default_special(cancbutton);

    lives_dialog_set_button_layout(LIVES_DIALOG(dlg), LIVES_BUTTONBOX_EDGE);

    if (!mainw->is_ready) {
      lives_widget_show_all(dlg);
      pop_to_front(dlg, NULL);
    }

    while (1) {
      LiVESResponseType resp = lives_dialog_run(LIVES_DIALOG(dlg));
      if (resp == LIVES_RESPONSE_BROWSE) {
        lives_widget_hide(dlg);
        lives_widget_context_update();
        show_jack_status(LIVES_BUTTON(statbutton), LIVES_INT_TO_POINTER(2));
        lives_widget_show(dlg);
      } else {
        lives_widget_destroy(dlg);
        if (resp == LIVES_RESPONSE_CANCEL) return FALSE;
        if (resp == LIVES_RESPONSE_RESET) {
          if (is_trans) {
            // disable transport for now
            future_prefs->jack_opts &= ~JACK_OPTS_ENABLE_TCLIENT;
            set_int_pref(PREF_JACK_OPTS, future_prefs->jack_opts);
          }
          return do_jack_config(2, is_trans);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*
  return FALSE;
}
#endif

LIVES_GLOBAL_INLINE void do_mt_backup_space_error(lives_mt * mt, int memreq_mb) {
  char *msg = lives_strdup_printf(
                _("\n\nLiVES needs more backup space for this layout.\nYou can increase "
                  "the value in Preferences/Multitrack.\n"
                  "It is recommended to increase it to at least %d MB"), memreq_mb);
  do_error_dialog_with_check(msg, WARN_MASK_MT_BACKUP_SPACE);
  lives_free(msg);
}


LIVES_GLOBAL_INLINE boolean do_set_rename_old_layouts_warning(const char *new_set) {
  return do_yesno_dialogf(
           _("\nSome old layouts for the set %s already exist.\n"
             "However no matching clips were encountered\n"
             "It is recommended that you delete these old layouts since they will not be usable\n"
             "with the newly imported clips.\nDo you wish to do so ?\n"), new_set);
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
      "You should adjust the audio settings from the Tools menu.\n"), warn_mask);
}


LIVES_GLOBAL_INLINE void do_mt_no_audchan_error(void) {
  do_error_dialog(_("The current layout has audio, so audio channels may not be set to zero.\n"));
}


LIVES_GLOBAL_INLINE void do_mt_no_jack_error(int warn_mask) {
  do_error_dialog_with_check(
    _("Multitrack audio preview is only available with the\n\"jack\" or \"pulseaudio\" audio player.\n"
      "You can set this in Tools|Preferences|Playback."), warn_mask);
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


LiVESResponseType do_please_install(const char *info, const char *exec, const char *exec2, uint64_t gflags) {
  LiVESResponseType ret;
  char *extra = lives_strdup(""), *msg;
  if (gflags & INSTALL_CANLOCAL) {
    lives_free(extra);
    extra = _("\n\nAlternately, LiVES may be able to install\na local user copy "
              "of the program.\n");
  }

  if (capable->distro_name && *capable->distro_name) {
    char *icmd = get_install_cmd(capable->distro_name, exec), *extra2;
    if (icmd) {
      //lives_free(extra);
      char *icmdx = lives_strdup_printf("'<b>%s</b>'", icmd), *icmd2x = NULL;
      lives_free(icmd);
      if (exec2) {
        char *icmd2 = get_install_cmd(capable->distro_name, exec2);
        if (icmd2) {
          icmd2x = lives_strdup_printf("\nor '<b>%s</b>'", icmd2);
          lives_free(icmd2);
        }
      }

      /* if (gflags & IS_ALTERNATIVE) alter = _("Alternatively, you can try"); */
      /* else alter = _("Try"); */
      extra2 = lives_strdup_printf(_("\n\nTry %s%s from a terminal window,\n"
                                     "or use your software package manager to install %s\n%s"), icmdx,
                                   icmd2x ? icmd2x : "", icmd2x ? _("one of those packages") : _("the package"), extra);
      lives_free(extra);
      extra = extra2;
      lives_free(icmdx);
      if (icmd2x) lives_free(icmd2x);
    }
  }

  if (exec2) {
    msg = lives_strdup_printf(_("Either '%s' or '%s' must be installed for this feature to work.\n"
                                "If possible, kindly install one or other of these before continuing\n%s"),
                              exec, exec2, extra);
  } else
    msg = lives_strdup_printf(_("%s '%s' is necessary for this feature to work.\n"
                                "If possible, kindly install %s before continuing.%s"), info, exec, exec, extra);
  lives_free(extra);

  if (gflags & INSTALL_CANLOCAL) {
    LiVESWidget *dlg;
    widget_opts.use_markup = TRUE;
    dlg = create_question_dialog(NULL, msg);
    widget_opts.use_markup = FALSE;
    lives_free(msg);

    lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_CANCEL,
                                       _("Cancel / Install Manually"), LIVES_RESPONSE_CANCEL);

    lives_dialog_add_button_from_stock(LIVES_DIALOG(dlg), LIVES_STOCK_ADD,
                                       _("Continue and Install Local Copy"), LIVES_RESPONSE_YES);

    lives_dialog_set_button_layout(LIVES_DIALOG(dlg), LIVES_BUTTONBOX_SPREAD);

    ret = lives_dialog_run(LIVES_DIALOG(dlg));
    lives_widget_destroy(dlg);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    return ret;
  }

  if (gflags & INSTALL_IMPORTANT) {
    char *msg2 = lives_strdup_printf(_("IMPORTANT !\n\n%s"), msg);
    ret = do_abort_retry_cancel_dialog(msg2);
    lives_free(msg2); lives_free(msg);
    return ret;
  }
  widget_opts.use_markup = TRUE;
  do_info_dialog(msg);
  widget_opts.use_markup = FALSE;
  lives_free(msg);
  return LIVES_RESPONSE_NONE;
}


LIVES_GLOBAL_INLINE boolean do_please_install_eitherz(const char *exec, const char *exec2) {
  do_info_dialogf(_("Either '%s' or '%s' must be installed for this feature to work.\n"
                    "If possible, kindly install one or other of these before continuing\n"),
                  exec, exec2);
  return FALSE;
}


LIVES_GLOBAL_INLINE void do_audrate_error_dialog(void) {
  do_error_dialog(_("\n\nAudio rate must be greater than 0.\n"));
}


LIVES_GLOBAL_INLINE boolean do_event_list_warning(void) {
  if (prefs->event_window_show_frame_events) {
    extra_cb_key = -1;
    extra_cb_data = lives_hbox_new(FALSE, 0);
    toggle_toggles_var(LIVES_TOGGLE_BUTTON(lives_standard_check_button_new
                                           (_("Show all FRAME events (unselecting thsi may reduce the size)"),
                                            prefs->event_window_show_frame_events, LIVES_BOX(extra_cb_data),
                                            _("Uncheck this to avoid showing all frames and reduce the "
                                                "event list size"))),
                       &prefs->event_window_show_frame_events, FALSE);
  }
  return do_yesno_dialog(_("\nEvent list will be very large\n and may take a long time to display.\n"
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
  if (prefs->vj_mode || prefs->show_dev_opts || prefs->startup_phase) return;
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
      // need to recheck after calling process_updates
      if (mainw->proc_ptr && mainw->proc_ptr->processing)
        mainw->threaded_dialog = TRUE;
    }
  }
}


// "threaded" (not really threaded !) dialog is similar to auto dialog, but it shows progress percentage
// and it may also have a "Cancel" button
//
// after calling do_threaded_dialog(), the progress can be updated manually by calling threaded_dialog_spin(fract)
// if fract is > 0. it is used to set the progress percent (frac * 100)
// if frac is 0., then progress is read from a file produced by the back end
// threaded_dialog_auto_spin can be called in this case the progress will updat automatically without
// needing to call threaded_dialog_spin(), threaded_dialog_stop_spin() goes back to manual mode
// end_threaded_dialog calls this autmaitically
//
// threaded dialog push() and threaded_dialog_pop() allow nesting of threaded_dialogs, eg.
// do_threaded_dialog(...); threaded_dialog_push()l; do_threaded_dialog(..);
// threaded_dialog_pop(); end_threaded_dialog()

static double xfraction = 0.;

static void _threaded_dialog_spin(double fraction) {
  double timesofar;
  int progress = 0;

  xfraction = fraction;

  if (!mainw->proc_ptr) return;

  if (fraction > 0.) {
    timesofar = (double)(lives_get_current_ticks() - sttime) / TICKS_PER_SECOND_DBL;
    disp_fraction(fraction, timesofar, mainw->proc_ptr);
  } else {
    if (*mainw->msg) progress = atoi(mainw->msg);
    if (!CURRENT_CLIP_IS_VALID || !progress || !mainw->proc_ptr ||
        !mainw->proc_ptr->progress_start || !mainw->proc_ptr->progress_end) {
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
  lives_widget_process_updates(mainw->proc_ptr->processing);
}


void threaded_dialog_spin(double fraction) {
  if (!mainw->threaded_dialog || mainw->splash_window || !mainw->proc_ptr
      || !mainw->is_ready || !prefs->show_gui) return;
  if (!mainw->is_exiting && !is_fg_thread()) {
    if (THREADVAR(no_gui)) return;
    main_thread_execute_rvoid(_threaded_dialog_spin, 0, "d", fraction);
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
  if (mainw->threaded_dialog || mainw->dlg_spin_thread) return;
  if (!mainw->is_exiting) {
    if (!!is_fg_thread()) {
      main_thread_execute_rvoid(_do_threaded_dialog, 0, "sb", trans_text, has_cancel);
    } else _do_threaded_dialog(trans_text, has_cancel);
  }
}


static void _thdlg_auto_spin(void) {
  GET_PROC_THREAD_SELF(self);
  uint64_t syncid = GET_SELF_VALUE(uint64, "sync_idx");
  lives_proc_thread_set_cancellable(self);
  lives_proc_thread_sync_with(lives_proc_thread_get_dispatcher(self), syncid, MM_IGNORE);
  THREADVAR(perm_hook_hints) = HOOK_OPT_FG_LIGHT;
  while (mainw->threaded_dialog && !lives_proc_thread_get_cancel_requested(self)) {
    int count = 1000;
    boolean spun = FALSE;
    while (!lives_proc_thread_get_cancel_requested(self) && --count) {
      if (mainw->threaded_dialog && !mainw->cancelled && !spun && !FG_THREADVAR(fg_service)) {
        threaded_dialog_spin(xfraction);
        spun = TRUE;
      }
      lives_nanosleep(100000);
      //g_print("ah %d", lives_proc_thread_get_cancel_requested(self));
    }
  }
  THREADVAR(perm_hook_hints) = 0;
  if (lives_proc_thread_get_cancel_requested(self))
    lives_proc_thread_cancel(self);
}


void threaded_dialog_auto_spin(void) {
  uint64_t syncid;;
  lives_proc_thread_t lpt;
  if (!prefs->show_gui) return;
  if (!mainw->threaded_dialog || mainw->dlg_spin_thread) return;
  syncid = gen_unique_id();
  lpt = mainw->dlg_spin_thread = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                 (lives_funcptr_t)_thdlg_auto_spin, -1, "", NULL);
  SET_LPT_VALUE(lpt, uint64, "sync_idx", syncid);

  lives_proc_thread_queue(lpt, 0);
  lives_proc_thread_sync_with(lpt, syncid, MM_IGNORE);
}


void threaded_dialog_stop_spin(void) {
  if (mainw->dlg_spin_thread) {
    lives_proc_thread_request_cancel(mainw->dlg_spin_thread, FALSE);
    lives_proc_thread_join(mainw->dlg_spin_thread);
    mainw->dlg_spin_thread = NULL;
  }
}


static void _end_threaded_dialog(void) {
  if (!mainw->threaded_dialog) return;

  if (mainw->dlg_spin_thread) threaded_dialog_stop_spin();

  mainw->cancel_type = CANCEL_KILL;

  if (mainw->proc_ptr && mainw->proc_ptr->processing) {
    lives_widget_destroy(mainw->proc_ptr->processing);
    mainw->proc_ptr->processing = NULL;
    lives_widget_context_update();
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);

  lives_freep((void **)&mainw->proc_ptr->text);
  lives_freep((void **)&mainw->proc_ptr);

  mainw->threaded_dialog = FALSE;

  if (prefs->show_msg_area) {
    // TODO


    /* if (LIVES_IS_WINDOW(LIVES_MAIN_WINDOW_WIDGET)) { */
    /*   lives_window_present(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET)); */
    /*   lives_widget_grab_focus(mainw->msg_area); */
    /*   gtk_window_set_focus(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->msg_area); */
    /* } */
  }
}

void end_threaded_dialog(void) {
  if (THREADVAR(no_gui)) return;
  if (!mainw->threaded_dialog) return;
  if (!mainw->is_exiting && !is_fg_thread()) {
    BG_THREADVAR(hook_hints) = HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
    main_thread_execute_void(_end_threaded_dialog, 0);
    BG_THREADVAR(hook_hints) = 0;
  } else _end_threaded_dialog();
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

  capable->ds_status = ds;
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

  char *sutf = lives_filename_to_utf8(s, -1, NULL, NULL, NULL), *xsutf, *xaddbit;

  if (file_is_ours(s)) {
    boolean exists;
    int64_t dsval = capable->ds_used;
    lives_storage_status_t ds;
    char dirname[PATH_MAX];

    lives_snprintf(dirname, PATH_MAX, "%s", s);
    get_dirname(dirname);
    exists = lives_file_test(dirname, LIVES_FILE_TEST_EXISTS);
    ds = get_storage_status(dirname, prefs->ds_crit_level, &dsval, 0);
    capable->ds_status = ds;
    capable->ds_free = dsval;
    if (!exists) lives_rmdir(dirname, FALSE);

    if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
      lives_free(dsmsg);
      tmp = ds_critical_msg(dirname, &capable->mountpoint, dsval);
      dsmsg = lives_strdup_printf("%s\n", tmp);
      lives_free(tmp);
    }
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
  const char *xs = s ? s : "uknown";
  char *sutf = lives_filename_to_utf8(xs, -1, NULL, NULL, NULL);

  if (addinfo) addbit = lives_strdup_printf(_("Additional info: %s\n"), addinfo);
  else addbit = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES was unable to read from the file\n%s\n"
                              "Please check for possible error causes.\n%s"),
                            sutf, addbit);
  emsg = lives_strdup_printf("Unable to read from the file\n%s\n%s", xs, addbit);
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

  // return same as do_abort_retry_cancel_dialog() - LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY (both non-zero)

  LiVESResponseType ret;
  char *msg, *emsg, *tmp;
  char *sutf = lives_filename_to_utf8(fname, -1, NULL, NULL, NULL), *xsutf;
  char *dsmsg = lives_strdup("");

  if (file_is_ours(fname)) {
    char dirname[PATH_MAX];
    boolean exists;
    int64_t dsval = capable->ds_used;
    lives_storage_status_t ds;
    lives_snprintf(dirname, PATH_MAX, "%s", fname);
    get_dirname(dirname);
    exists = lives_file_test(dirname, LIVES_FILE_TEST_EXISTS);
    ds = get_storage_status(dirname, prefs->ds_crit_level, &dsval, 0);
    capable->ds_status = ds;
    capable->ds_free = dsval;
    if (!exists) lives_rmdir(dirname, FALSE);

    if (ds == LIVES_STORAGE_STATUS_CRITICAL) {
      lives_free(dsmsg);
      tmp = ds_critical_msg(dirname, &capable->mountpoint, dsval);
      dsmsg = lives_strdup_printf("%s\n", tmp);
      lives_free(tmp);
    }
  }

  xsutf = lives_markup_escape_text(sutf, -1);

  if (!errtext) {
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
  ret = do_abort_retry_cancel_dialog(msg);
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

  if (!(THREADVAR(thrdnative_flags) & THRDNATIVE_CAN_CORRECT))
    ret = do_abort_retry_cancel_dialog(msg);
  else
    ret = do_abort_retry_ignore_dialog(msg);

  lives_free(msg);
  lives_free(sutf);

  THREADVAR(read_failed) = 0; // reset this

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
    can_cancel = lives_strdup_printf(_(", click %s to continue regardless,\n"),
                                     STOCK_LABEL_TEXT(CANCEL));
  else
    can_cancel = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES was unable to write to the file:\n%s\n"
                              "Please check the file permissions and try again."
                              "%sor click %s to exit from LiVES"), file_name, can_cancel,
                            STOCK_LABEL_TEXT(ABORT));
  if (!allow_cancel)
    resp = do_abort_retry_dialog(msg);
  else
    resp = do_abort_retry_cancel_dialog(msg);
  lives_free(msg);
  return resp;
}


LiVESResponseType do_dir_perm_error(const char *dir_name, boolean allow_cancel) {
  LiVESResponseType resp;
  char *msg, *can_cancel;
  if (allow_cancel)
    can_cancel = lives_strdup_printf(_("click %s to continue regardless, "),
                                     STOCK_LABEL_TEXT(CANCEL));
  else
    can_cancel = lives_strdup("");

  msg = lives_strdup_printf(_("\nLiVES was unable to either create or write to the directory:\n%s\n"
                              "Please check the directory permissions and try again,\n"
                              "%sor click %s to exit from LiVES"), dir_name, can_cancel,
                            STOCK_LABEL_TEXT(ABORT));
  lives_free(can_cancel);

  if (!allow_cancel)
    resp = do_abort_retry_dialog(msg);
  else
    resp = do_abort_retry_cancel_dialog(msg);

  lives_free(msg);
  return resp;
}


void do_dir_perm_access_error(const char *dir_name) {
  char *msg = lives_strdup_printf(_("\nLiVES was unable to read from the directory:\n%s\n"), dir_name);
  do_abort_ok_dialog(msg);
  lives_free(msg);
}


LiVESResponseType do_abort_restart_check(boolean allow_restart, LiVESList * xrestart_opts) {
  //LiVESList **restart_opts = NULL;
  LiVESResponseType resp;
  if (!allow_restart) {
    boolean bval;
    if (!prefs->startup_phase)
      bval = do_yesno_dialog(_("\nAbort and exit immediately from LiVES\nAre you sure ?\n"));
    else
      bval = do_yesno_dialog(_("\nAre are you sure you would like to exit from LiVES setup ?\n"));
    if (bval) return LIVES_RESPONSE_YES;
    return LIVES_RESPONSE_NO;
  } else {
    LiVESWidget *dialog;
    char *txt = lives_list_to_string(xrestart_opts, " ");
    if (txt) {
      char *labtxt = lives_strdup_printf(_("The following options will be appended to the commandline: %s\n"), txt);
      lives_free(txt);
      extra_cb_key = -1;
      extra_cb_data = lives_standard_label_new(labtxt);
      lives_free(labtxt);
    }
    dialog = create_message_dialog(LIVES_DIALOG_ABORT_RESTART, _("Would you like to exit from LiVES or "
                                   "attempt to restart it ?"), 0);
    resp = lives_dialog_run(LIVES_DIALOG(dialog));
    lives_widget_destroy(dialog);
    return resp;
  }
}


LIVES_GLOBAL_INLINE boolean do_abort_check(void) {
  LiVESResponseType resp = do_abort_restart_check(TRUE, NULL);
  return (resp == LIVES_RESPONSE_YES);
}



boolean do_fxload_query(int maxkey, int maxmode) {
  char *morekeys, *moremodes;
  boolean ret;

  if (maxkey > prefs->rte_keys_virtual) {
    char *extra;
    if (maxmode > prefs->rte_modes_per_key)
      extra = lives_strdup(_(",\nand "));
    else
      extra = lives_strdup("");
    morekeys = lives_strdup_printf(_("%d effect key slots (current setting is %d)%s"),
                                   maxkey, prefs->rte_keys_virtual, extra);
    lives_free(extra);
  } else morekeys = lives_strdup("");

  if (maxmode > prefs->rte_modes_per_key) {
    moremodes = lives_strdup_printf(_("%d modes per key (current setting is %d)"),
                                    maxmode, prefs->rte_modes_per_key);
  } else moremodes = lives_strdup("");

  ret = do_yesno_dialogf(_("The effect keymap could not be completely loaded as it requires %s%s\n"
                           "Should I update %s in Preferences and try again ?\n"),
                         morekeys, moremodes, (*morekeys && *moremodes) ? _("these values") :
                         _("this value"));
  lives_free(morekeys); lives_free(moremodes);
  return ret;
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

  int i = 0;

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
                  "Click %s to set another file name\nor %s to continue and save as type \"%s\"\n"),
                ext, type_ext, STOCK_LABEL_TEXT(CANCEL), STOCK_LABEL_TEXT(OK));
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



LIVES_GLOBAL_INLINE boolean do_noworkdirchange_dialog(void) {
  return do_yesno_dialog(_("You must choose an action in order to change to the new work directory.\n"
                           "Do you still want to proceed with the directory change ?\n"));
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
  return do_yesno_dialogf_with_countdown(3, TRUE,  _("All files will be irrevocably deleted from the working directory\n%s\n"
                                         "Are you certain you wish to continue ?\n"
                                         "Click 3 times on the 'Yes' button to confirm you understand\n"), prefs->workdir);
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
  do_info_dialogf(
    _("\nLiVES will now shut down. You need to restart it for the new "
      "preferences to take effect.\nClick %s to continue.\n"), STOCK_LABEL_TEXT(OK));
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
    case LIVES_PERM_KILL_PIDS: {
      LiVESList *pidlist = (LiVESList *)argv[1];
      char *reason = (char *)argv[2];
      char *outcome = (char *)argv[3];
      int npids = lives_list_length(pidlist);
      char *msg, *tmp;
      boolean res = FALSE;
      if (npids > 0) {
        if (npids == 1) {
          msg = lives_strdup_printf((tmp = _("%s process %d.\nShall I terminate that process to %s ?")),
                                    reason, LIVES_POINTER_TO_INT(pidlist->data), outcome);
          lives_free(tmp);
        } else {
          msg = lives_strdup_printf((tmp = _("%s the following processes:\n")), reason);
          lives_free(tmp);
          msg = lives_concat_sep(msg, "", lives_strdup_printf("%d", LIVES_POINTER_TO_INT(pidlist->data)));
          for (LiVESList *list = pidlist->next; list; list = list->next) {
            if (list->next)
              msg = lives_concat_sep(msg, ", ", lives_strdup_printf("%d", LIVES_POINTER_TO_INT(list->data)));
            else {
              tmp = _(" and ");
              msg = lives_concat_sep(msg, tmp, lives_strdup_printf("%d", LIVES_POINTER_TO_INT(list->data)));
              lives_free(tmp);
            }
          }

          msg = lives_concat_sep(msg, "", lives_strdup_printf((tmp = _("\nShall I terminate the processes to %s ?")),
                                 outcome));
          lives_free(tmp);
        }
        res = do_yesno_dialog(msg);
        lives_free(msg);
      }
      return res;
    }
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


LIVES_GLOBAL_INLINE void do_no_sets_dialog(const char *dir) {
  extra_cb_key = 1;
  do_info_dialogf(_("No Sets could be found in the directory\n%s\n\n"
                    "If you have Sets in another directory, you can either:\n"
                    " - change the working directory in Preferences, or\n"
                    " - restart lives with the -workdir switch to set it temporarily"), dir);
}


int do_audexport_dlg(int arps, int arate) {
  LiVESWidget *dialog, *dialog_vbox, *hbox, *layout;
  LiVESWidget *rb = NULL, *cancelbutton, *okbutton;
  char *msg = NULL, *xmsg;
  LiVESResponseType resp;
  boolean use_adjrate = FALSE;
  boolean resamp1 = FALSE, resamp2 = FALSE;
  int xarate = arate, xarps = arps;
  int res = 0;

  if (arps != arate) {
    msg = _("The audio playback speed has been altered for this clip.\n");
  } else {
    if (find_standard_arate(arate) != arate) {
      msg = lives_strdup_printf(_("The current audio rate (%d) uses a non-standard value\n"), arate);
      extra_cb_key = 3;
    } else return 0;
  }

  xmsg = lives_strdup_printf(_("\n\n%s\nWhat would you like to do ?"), msg);
  lives_free(msg);

  dialog = create_question_dialog(NULL, xmsg);
  lives_free(xmsg);
  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  xarps = find_standard_arate(arps);

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  if (arps != arate) {
    LiVESSList *rbgroup = NULL;
    xarate = find_standard_arate(arate);
    use_adjrate = TRUE;
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    msg = lives_strdup_printf(_("Use original rate (%d Hz)"), arps);
    lives_standard_radio_button_new(msg, &rbgroup, LIVES_BOX(hbox), NULL);
    lives_free(msg);
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    msg = lives_strdup_printf(_("Use adjusted rate (%d Hz)"), arate);
    rb = lives_standard_radio_button_new(msg, &rbgroup, LIVES_BOX(hbox), NULL);
    lives_free(msg);
    toggle_toggles_var(LIVES_TOGGLE_BUTTON(rb), &use_adjrate, FALSE);
    lives_layout_add_row(LIVES_LAYOUT(layout));
  }

  if (xarps != arps || xarate != arate) {
    LiVESWidget *checkb1, *checkb2;
    if (arps != arate) {
      lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
      hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    } else hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    if (xarps != arps) {
      resamp1 = TRUE;
      msg = lives_strdup_printf(_("Resample to standard rate (%d Hz)"), xarps);
      checkb1 = lives_standard_check_button_new(msg, FALSE, LIVES_BOX(hbox), NULL);
      lives_free(msg);
      toggle_toggles_var(LIVES_TOGGLE_BUTTON(checkb1), &resamp1, FALSE);
      if (rb) toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), checkb1, TRUE);
    }
    if (arate != arps && xarate != arate) {
      resamp2 = TRUE;
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
      msg = lives_strdup_printf(_("Resample to standard rate (%d Hz)"), xarate);
      checkb2 = lives_standard_check_button_new(msg, FALSE, LIVES_BOX(hbox), NULL);
      lives_free(msg);
      toggle_toggles_var(LIVES_TOGGLE_BUTTON(checkb2), &resamp2, FALSE);
      if (rb) toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), checkb2, FALSE);
    }
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  lives_widget_destroy(dialog);

  if (resp == LIVES_RESPONSE_CANCEL) return -1;

  if (use_adjrate) {
    res++;
    if (resamp2) res += 2;
  } else if (resamp1) res += 2;
  return res;
}

