// paramspecial.c
// LiVES
// (c) G. Finch 2004 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// dynamic window generation from parameter arrays :-)
// special widgets

#include "main.h"
#include "resample.h"
#include "effects.h"
#include "paramwindow.h"
#include "framedraw.h"
#include "callbacks.h"

static lives_special_aspect_t aspect;
static lives_special_fontchooser_t fchooser;
static lives_special_framedraw_rect_t framedraw;
static LiVESList *fileread;
static LiVESList *filewrite;
static LiVESList *passwd_widgets;

static boolean special_inited = FALSE;

void init_special(void) {
  if (special_inited) return;

  special_inited = TRUE;
  fchooser.nwidgets = 0;

  framedraw.xstart_param = framedraw.ystart_param = framedraw.xend_param = framedraw.yend_param = NULL;
  framedraw.stdwidgets = 0;
  framedraw.extra_params = NULL;
  framedraw.num_extra = 0;
  framedraw.added = FALSE;
  framedraw.type = LIVES_PARAM_SPECIAL_TYPE_NONE;
  mergealign.start_param = mergealign.end_param = NULL;
  passwd_widgets = NULL;
  fileread = NULL;
  filewrite = NULL;
}


const lives_special_aspect_t *paramspecial_get_aspect() {return &aspect;}


void add_to_special(const char *sp_string, lives_rfx_t *rfx) {
  int num_widgets = get_token_count(sp_string, '|') - 2;
  int pnum;

  char **array = lives_strsplit(sp_string, "|", num_widgets + 2);

  register int i;

  // TODO - assert only one of each of these

  if (!strcmp(array[0], "aspect")) {
    aspect.width_param = &rfx->params[atoi(array[1])];
    aspect.height_param = &rfx->params[atoi(array[2])];
  } else if (!strcmp(array[0], "fontchooser")) {
#if GTK_CHECK_VERSION(3, 2, 0)
    fchooser.font_param = &rfx->params[atoi(array[1])];
    fchooser.size_param = &rfx->params[atoi(array[2])];
#endif
  } else if (!strcmp(array[0], "mergealign")) {
    mergealign.start_param = &rfx->params[atoi(array[1])];
    mergealign.end_param = &rfx->params[atoi(array[2])];
    mergealign.rfx = rfx;
  } else if (!strcmp(array[0], "framedraw")) {
    if (fx_dialog[1] != NULL) {
      lives_strfreev(array);
      return;
    }
    framedraw.rfx = rfx;
    if (!strcmp(array[1], "rectdemask")) {
      framedraw.type = LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK;
      framedraw.xstart_param = &rfx->params[atoi(array[2])];
      framedraw.ystart_param = &rfx->params[atoi(array[3])];
      framedraw.xend_param = &rfx->params[atoi(array[4])];
      framedraw.yend_param = &rfx->params[atoi(array[5])];
      framedraw.stdwidgets = 4;
    } else if (!strcmp(array[1], "multirect") || !strcmp(array[1], "multrect")) { // allow for spelling errors in earlier draft
      framedraw.type = LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT;
      framedraw.xstart_param = &rfx->params[atoi(array[2])];
      framedraw.ystart_param = &rfx->params[atoi(array[3])];
      framedraw.xend_param = &rfx->params[atoi(array[4])];
      framedraw.yend_param = &rfx->params[atoi(array[5])];
      framedraw.stdwidgets = 4;
    } else if (!strcmp(array[1], "singlepoint")) {
      framedraw.type = LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT;
      framedraw.xstart_param = &rfx->params[atoi(array[2])];
      framedraw.ystart_param = &rfx->params[atoi(array[3])];
      framedraw.stdwidgets = 2;
    } else if (!strcmp(array[1], "scaledpoint")) {
      framedraw.type = LIVES_PARAM_SPECIAL_TYPE_SCALEDPOINT;
      framedraw.xstart_param = &rfx->params[atoi(array[2])];
      framedraw.ystart_param = &rfx->params[atoi(array[3])];
      framedraw.scale_param = &rfx->params[atoi(array[4])];
      framedraw.stdwidgets = 3;
    }

    if (num_widgets > framedraw.stdwidgets) framedraw.extra_params =
        (int *)lives_malloc(((framedraw.num_extra = (num_widgets - framedraw.stdwidgets))) * sizint);

    for (i = 0; i < num_widgets; i++) {
      pnum = atoi(array[i + 2]);
      if (rfx->status == RFX_STATUS_WEED) {
        if (mainw->multitrack != NULL) {
          if (rfx->params[pnum].multi == PVAL_MULTI_PER_CHANNEL) {
            if ((rfx->params[pnum].hidden & HIDDEN_MULTI) == HIDDEN_MULTI) {
              if (mainw->multitrack->track_index != -1) {
                rfx->params[pnum].hidden ^= HIDDEN_MULTI; // multivalues allowed
              } else {
                rfx->params[pnum].hidden |= HIDDEN_MULTI; // multivalues hidden
		// *INDENT-OFF*
              }}}}}
      // *INDENT-ON*
      if (i >= framedraw.stdwidgets) framedraw.extra_params[i - framedraw.stdwidgets] = pnum;
    }

    if (mainw->multitrack != NULL) {
      mainw->multitrack->framedraw = &framedraw;
      lives_widget_set_bg_color(mainw->multitrack->fd_frame, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);
    }
  }

  // can be multiple of each of these

  else if (!strcmp(array[0], "fileread")) {
    int idx = atoi(array[1]);
    fileread = lives_list_append(fileread, (livespointer)&rfx->params[idx]);

    // ensure we get an entry and not a text_view
    if ((int)rfx->params[idx].max > RFX_TEXT_MAGIC) rfx->params[idx].max = (double)RFX_TEXT_MAGIC;
  } else if (!strcmp(array[0], "filewrite")) {
    int idx = atoi(array[1]);
    filewrite = lives_list_append(filewrite, (livespointer)&rfx->params[idx]);
    rfx->params[idx].edited = TRUE;

    // ensure we get an entry and not a text_view
    if ((int)rfx->params[idx].max > RFX_TEXT_MAGIC) rfx->params[idx].max = (double)RFX_TEXT_MAGIC;
  } else if (!strcmp(array[0], "password")) {
    int idx = atoi(array[1]);
    passwd_widgets = lives_list_append(passwd_widgets, (livespointer)&rfx->params[idx]);

    // ensure we get an entry and not a text_view
    if ((int)rfx->params[idx].max > RFX_TEXT_MAGIC) rfx->params[idx].max = (double)RFX_TEXT_MAGIC;
  }

  lives_strfreev(array);
}


void fd_tweak(lives_rfx_t *rfx) {
  if (rfx->props & RFX_PROPS_MAY_RESIZE) {
    if (framedraw.type != LIVES_PARAM_SPECIAL_TYPE_NONE) {
      // for effects which can resize, and have a special framedraw, we will use original sized image
      lives_widget_hide(mainw->framedraw_preview);
      lives_widget_set_sensitive(mainw->framedraw_spinbutton, TRUE);
      lives_widget_set_sensitive(mainw->framedraw_scale, TRUE);
    }
  }
  if (framedraw.type != LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK) {
    lives_widget_set_no_show_all(mainw->framedraw_maskbox, TRUE);
  }
}


void fd_connect_spinbutton(lives_rfx_t *rfx) {
  framedraw_connect_spinbutton(&framedraw, rfx);
}


static void passwd_toggle_vis(LiVESToggleButton * b, livespointer entry) {
  lives_entry_set_visibility(LIVES_ENTRY(entry), lives_toggle_button_get_active(b));
}

static void reset_aspect(LiVESButton * button, livespointer user_data) {
  if (lives_lock_button_get_locked(button)) {
    lives_special_aspect_t *aspect = (lives_special_aspect_t *)user_data;
    double width = lives_spin_button_get_value(LIVES_SPIN_BUTTON(aspect->width_param->widgets[0]));
    double height = lives_spin_button_get_value(LIVES_SPIN_BUTTON(aspect->height_param->widgets[0]));
    aspect->ratio = width / height;
  }
}

#if GTK_CHECK_VERSION(3, 2, 0)
static void font_set_cb(LiVESFontButton * button, livespointer data) {
  lives_rfx_t *rfx = (lives_rfx_t *)data;
  char *fname = lives_font_chooser_get_font(LIVES_FONT_CHOOSER(button));
  LingoFontDescription *lfd = lives_font_chooser_get_font_desc(LIVES_FONT_CHOOSER(button));
  int size = lingo_font_description_get_size(lfd);
  lives_signal_handler_block(fchooser.font_param->widgets[0], fchooser.entry_func);
  lives_entry_set_text(LIVES_ENTRY(fchooser.font_param->widgets[0]), fname);
  after_param_text_changed(fchooser.font_param->widgets[0], rfx);
  lives_signal_handler_unblock(fchooser.font_param->widgets[0], fchooser.entry_func);
  lives_signal_handler_block(fchooser.size_param->widgets[0], fchooser.size_paramfunc);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(fchooser.size_param->widgets[0]), size / LINGO_SCALE);
  lives_signal_handler_unblock(fchooser.size_param->widgets[0], fchooser.size_paramfunc);
  lives_free(fname);
  lingo_font_description_free(lfd);
}

static void font_size_cb(LiVESSpinButton * button, livespointer data) {
  int sval = lives_spin_button_get_value_as_int(button);
  LingoFontDescription *lfd =
    lives_font_chooser_get_font_desc(LIVES_FONT_CHOOSER(fchooser.font_param->widgets[1]));
  lingo_font_description_set_size(lfd, sval * LINGO_SCALE);
  lives_font_chooser_set_font_desc(LIVES_FONT_CHOOSER(fchooser.font_param->widgets[1]), lfd);
  lingo_font_description_free(lfd);
}

static void font_entry_cb(LiVESEntry * entry, livespointer data) {
  LiVESFontButton *button = (LiVESFontButton *)fchooser.font_param->widgets[1];
  lives_font_chooser_set_font(LIVES_FONT_CHOOSER(button), lives_entry_get_text(entry));
  font_size_cb(LIVES_SPIN_BUTTON(fchooser.size_param->widgets[0]), data);
  font_set_cb(button, data);
}
#endif


void check_for_special_type(lives_rfx_t *rfx, lives_param_t *param, LiVESBox * pbox) {
  LiVESList *slist;
  // check if this parameter is part of a special window
  // as we are drawing the paramwindow

  if (param == framedraw.xstart_param) {
    param->special_type = framedraw.type;
    param->special_type_index = 0;
  }
  if (param == framedraw.ystart_param) {
    param->special_type = framedraw.type;
    param->special_type_index = 1;
  }
  if (mainw->current_file > -1) {
    if (param == framedraw.xend_param) {
      param->special_type = framedraw.type;
      param->special_type_index = 2;
    }
    if (param == framedraw.yend_param) {
      param->special_type = framedraw.type;
      param->special_type_index = 3;
    }

    if (param == aspect.width_param) {
      param->special_type = LIVES_PARAM_SPECIAL_TYPE_ASPECT_RATIO;
      param->special_type_index = 0;
    }

    if (param == aspect.height_param) {
      param->special_type = LIVES_PARAM_SPECIAL_TYPE_ASPECT_RATIO;
      param->special_type_index = 1;
    }

    if (param == fchooser.font_param) {
      param->special_type = LIVES_PARAM_SPECIAL_TYPE_FONT_CHOOSER;
      param->special_type_index = 0;
    }
    if (param == fchooser.size_param) {
      param->special_type = LIVES_PARAM_SPECIAL_TYPE_FONT_CHOOSER;
      param->special_type_index = 1;
    }
  }

  slist = fileread;
  while (slist) {
    if (param == (lives_param_t *)(slist->data)) {
      param->special_type = LIVES_PARAM_SPECIAL_TYPE_FILEREAD;
    }
    slist = slist->next;
  }

  slist = filewrite;
  while (slist != NULL) {
    if (param == (lives_param_t *)(slist->data)) {
      param->special_type = LIVES_PARAM_SPECIAL_TYPE_FILEWRITE;
      slist = slist->next;
    }
  }

  // password fields
  slist = passwd_widgets;
  while (slist) {
    if (param == (lives_param_t *)(slist->data)) {
      param->special_type = LIVES_PARAM_SPECIAL_TYPE_PASSWORD;
      slist = slist->next;
    }
  }
}


void check_for_special(lives_rfx_t *rfx, lives_param_t *param, LiVESBox * pbox) {
  LiVESWidget *checkbutton;
  LiVESWidget *widget = param->widgets[0];
  LiVESWidget *hbox;
  LiVESWidget *box;
  LiVESWidget *buttond;
  LiVESList *slist;

  // check if this parameter is part of a special window
  // as we are drawing the paramwindow

  if (param == framedraw.xstart_param) {
    param->special_type = framedraw.type;
    param->special_type_index = 0;
    if (framedraw.type == LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(widget), 0.);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_framedraw_widget_changed), &framedraw);
  }
  if (param == framedraw.ystart_param) {
    param->special_type = framedraw.type;
    param->special_type_index = 1;
    if (framedraw.type == LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(widget), 0.);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_framedraw_widget_changed), &framedraw);
  }
  if (mainw->current_file > -1) {
    if (param == framedraw.xend_param) {
      param->special_type = framedraw.type;
      param->special_type_index = 2;
      if (framedraw.type == LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(widget), (double)cfile->hsize);
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                      LIVES_GUI_CALLBACK(after_framedraw_widget_changed), &framedraw);
    }
    if (param == framedraw.yend_param) {
      param->special_type = framedraw.type;
      param->special_type_index = 3;
      if (framedraw.type == LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(widget), (double)cfile->vsize);
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                      LIVES_GUI_CALLBACK(after_framedraw_widget_changed), &framedraw);
    }

    if (framedraw.stdwidgets > 0 && !framedraw.added) {
      if (framedraw.xstart_param != NULL && framedraw.xstart_param->widgets[0] != NULL &&
          framedraw.ystart_param != NULL && framedraw.ystart_param->widgets[0] != NULL) {
        if (framedraw.stdwidgets == 2 || (framedraw.stdwidgets == 3 && framedraw.scale_param != NULL &&
                                          framedraw.scale_param->widgets[0] != NULL) || (framedraw.xend_param != NULL
                                              && framedraw.xend_param->widgets[0] != NULL &&
                                              framedraw.yend_param != NULL && framedraw.yend_param->widgets[0] != NULL)) {
          if (mainw->multitrack == NULL) {
            framedraw_connect(&framedraw, cfile->hsize, cfile->vsize, rfx); // turn passive preview->active
            framedraw_add_reset(LIVES_VBOX(LIVES_WIDGET(pbox)), &framedraw);
          } else {
            mainw->framedraw = mainw->play_image;
          }
          framedraw.added = TRUE;
        }
      }
    }

    if (param == aspect.width_param) {
      if (CURRENT_CLIP_HAS_VIDEO) lives_spin_button_set_value(LIVES_SPIN_BUTTON(widget), cfile->hsize);
      aspect.width_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                          LIVES_GUI_CALLBACK(after_aspect_width_changed),
                          NULL);
      aspect.nwidgets++;
    }

    if (param == aspect.height_param) {
      if (CURRENT_CLIP_HAS_VIDEO) lives_spin_button_set_value(LIVES_SPIN_BUTTON(widget), cfile->vsize);
      aspect.height_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                           LIVES_GUI_CALLBACK(after_aspect_height_changed),
                           NULL);
      aspect.nwidgets++;
    }

#if GTK_CHECK_VERSION(3, 2, 0)
    if (param == fchooser.font_param) {
      LiVESWidget *tbox = widget;
      int idx;
      box = lives_widget_get_parent(widget);
      while (box != NULL && !LIVES_IS_HBOX(box)) {
        tbox = box;
        box = lives_widget_get_parent(box);
      }
      if (!box) return;
      idx = get_box_child_index(LIVES_BOX(box), tbox);
      param->widgets[1] = buttond = lives_standard_font_chooser_new();
      lives_box_pack_start(LIVES_BOX(box), buttond, TRUE, TRUE, 0);
      lives_box_reorder_child(LIVES_BOX(box), buttond, idx);
      if (lives_widget_is_visible(widget)) lives_widget_show(buttond);

      lives_widget_object_ref(tbox);
      lives_widget_unparent(tbox);

      if (!lives_widget_is_sensitive(widget)) lives_widget_set_sensitive(buttond, FALSE);
      lives_widget_set_show_hide_with(widget, buttond);
      lives_widget_set_sensitive_with(widget, buttond);

      lives_widget_destroy_with(buttond, tbox);

      lives_signal_sync_connect(LIVES_GUI_OBJECT(param->widgets[1]), LIVES_WIDGET_FONT_SET_SIGNAL,
                                LIVES_GUI_CALLBACK(font_set_cb),
                                (livespointer)rfx);
      fchooser.entry_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(widget),
                            LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(font_entry_cb),
                            (livespointer)rfx);
      if (fchooser.nwidgets == 1) {
        font_entry_cb(LIVES_ENTRY(param->widgets[0]), (livespointer)rfx);
      }
      fchooser.nwidgets++;
    }

    if (param == fchooser.size_param) {
      fchooser.size_paramfunc = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget),
                                LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(font_size_cb),
                                (livespointer)rfx);
      if (fchooser.nwidgets == 1) {
        font_entry_cb(LIVES_ENTRY(param->widgets[0]), (livespointer)rfx);
      }
      fchooser.nwidgets++;
    }
#endif

    if ((param == aspect.width_param || param == aspect.height_param) && aspect.nwidgets == 2) {
      boolean expand = widget_opts.expand == LIVES_EXPAND_EXTRA;
      char *labeltext = lives_strdup(_("    Maintain _Aspect Ratio    "));
      LiVESWidget *eventbox = lives_event_box_new();

      aspect.no_reset = TRUE;

      aspect.lockbutton = lives_standard_lock_button_new(TRUE, ASPECT_BUTTON_WIDTH,
                          ASPECT_BUTTON_HEIGHT, _("Maintain aspect ratio"));
      lives_signal_sync_connect(aspect.lockbutton, LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(reset_aspect), (livespointer)&aspect);

      reset_aspect(LIVES_BUTTON(aspect.lockbutton), &aspect);

      if (widget_opts.mnemonic_label) {
        aspect.label = lives_standard_label_new_with_mnemonic_widget(labeltext, aspect.lockbutton);
      } else aspect.label = lives_standard_label_new(labeltext);
      lives_free(labeltext);

      lives_tooltips_copy(eventbox, aspect.lockbutton);

      lives_container_add(LIVES_CONTAINER(eventbox), aspect.label);

      lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                                LIVES_GUI_CALLBACK(label_act_lockbutton),
                                aspect.lockbutton);

      lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);

      hbox = lives_hbox_new(FALSE, 0);
      lives_box_set_homogeneous(LIVES_BOX(hbox), FALSE);
      lives_widget_apply_theme(hbox, LIVES_WIDGET_STATE_NORMAL);

      if (!LIVES_IS_HBOX(pbox)) {
        add_fill_to_box(LIVES_BOX(hbox));
        add_fill_to_box(LIVES_BOX(hbox));
        lives_box_pack_start(LIVES_BOX(LIVES_WIDGET(pbox)), hbox, FALSE, FALSE, widget_opts.packing_height * 2);
      } else {
        lives_box_pack_start(LIVES_BOX(LIVES_WIDGET(pbox)), hbox, FALSE, FALSE, widget_opts.packing_width * 1.5);
      }

      if (widget_opts.swap_label)
        lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, 0);

      if (expand) add_fill_to_box(LIVES_BOX(hbox));

      lives_box_pack_start(LIVES_BOX(hbox), aspect.lockbutton, expand, FALSE, 0);

      if (expand) add_fill_to_box(LIVES_BOX(hbox));

      if (!widget_opts.swap_label)
        lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, 0);

      lives_widget_set_show_hide_parent(aspect.label);
      lives_widget_set_show_hide_parent(aspect.lockbutton);

      if (!LIVES_IS_HBOX(pbox)) {
        add_spring_to_box(LIVES_BOX(hbox), 0);
      }
    }
  }

  slist = fileread;
  while (slist) {
    if (param == (lives_param_t *)(slist->data)) {
      char *def_dir;
      if (!widget) continue;

      box = lives_widget_get_parent(widget);

      while (box && !LIVES_IS_HBOX(box)) {
        box = lives_widget_get_parent(box);
      }

      if (!box) return;

      def_dir = lives_get_current_dir();

      if (LIVES_IS_ENTRY(widget)) {
        if (*(lives_entry_get_text(LIVES_ENTRY(widget)))) {
          char dirnamex[PATH_MAX];
          lives_snprintf(dirnamex, PATH_MAX, "%s", lives_entry_get_text(LIVES_ENTRY(widget)));
          get_dirname(dirnamex);
          lives_free(def_dir);
          def_dir = lives_strdup(dirnamex);
        }
      }

      //epos = get_box_child_index(LIVES_BOX(box), widget);

      param->widgets[2] = buttond = lives_standard_file_button_new(FALSE, def_dir);
      lives_free(def_dir);
      lives_box_pack_start(LIVES_BOX(box), buttond, FALSE, FALSE, widget_opts.packing_width);
      //lives_box_reorder_child(LIVES_BOX(box), buttond, epos); // insert after label, before textbox
      lives_signal_sync_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                                (livespointer)widget);

      if (!lives_widget_is_sensitive(widget)) lives_widget_set_sensitive(buttond, FALSE);
      lives_widget_set_show_hide_with(widget, buttond);
      lives_widget_set_sensitive_with(widget, buttond);

      if (LIVES_IS_ENTRY(widget)) {
        lives_entry_set_editable(LIVES_ENTRY(widget), FALSE);
        if (param->widgets[1] != NULL &&
            LIVES_IS_LABEL(param->widgets[1]) &&
            lives_label_get_mnemonic_widget(LIVES_LABEL(param->widgets[1])) != NULL)
          lives_label_set_mnemonic_widget(LIVES_LABEL(param->widgets[1]), buttond);
        lives_entry_set_max_length(LIVES_ENTRY(widget), PATH_MAX);
      }
    }

    slist = slist->next;
  }

  slist = filewrite;
  while (slist != NULL) {
    if (param == (lives_param_t *)(slist->data)) {

      if (widget == NULL) continue;

      box = lives_widget_get_parent(widget);

      while (box != NULL && !LIVES_IS_HBOX(box)) {
        box = lives_widget_get_parent(box);
      }

      if (box == NULL) return;

      param->widgets[2] = buttond = lives_standard_file_button_new(FALSE, NULL);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(buttond), "filesel_type", (livespointer)LIVES_FILE_SELECTION_SAVE);
      lives_box_pack_start(LIVES_BOX(box), buttond, FALSE, FALSE, widget_opts.packing_width);
      lives_signal_sync_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                                (livespointer)widget);

      if (!lives_widget_is_sensitive(widget)) lives_widget_set_sensitive(buttond, FALSE);
      lives_widget_set_show_hide_with(widget, buttond);
      lives_widget_set_sensitive_with(widget, buttond);

      if (LIVES_IS_ENTRY(widget)) {
        lives_entry_set_editable(LIVES_ENTRY(widget), TRUE);
        if (param->widgets[1] != NULL &&
            LIVES_IS_LABEL(param->widgets[1]) &&
            lives_label_get_mnemonic_widget(LIVES_LABEL(param->widgets[1])) != NULL)
          lives_label_set_mnemonic_widget(LIVES_LABEL(param->widgets[1]), buttond);
        lives_entry_set_max_length(LIVES_ENTRY(widget), PATH_MAX);
      }
    }

    slist = slist->next;
  }

  // password fields

  slist = passwd_widgets;
  while (slist) {
    if (param == (lives_param_t *)(slist->data)) {
      if (!widget) continue;

      lives_entry_set_visibility(LIVES_ENTRY(widget), FALSE);

      box = lives_widget_get_parent(widget);

      while (box && !LIVES_IS_BOX(box)) {
        box = lives_widget_get_parent(box);
      }

      if (!box) continue;

      if (!LIVES_IS_HBOX(box)) {
        hbox = lives_hbox_new(FALSE, 0);
        lives_box_pack_start(LIVES_BOX(LIVES_WIDGET(box)), hbox, FALSE, FALSE, widget_opts.packing_height);
      } else hbox = box;

      param->widgets[2] = checkbutton = lives_standard_check_button_new(_("Display Password"), FALSE, LIVES_BOX(hbox), NULL);

      lives_button_set_focus_on_click(LIVES_BUTTON(checkbutton), FALSE);

      if (!lives_widget_is_sensitive(widget)) lives_widget_set_sensitive(checkbutton, FALSE);
      lives_widget_show_all(hbox);

      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                      LIVES_GUI_CALLBACK(passwd_toggle_vis),
                                      (livespointer)widget);

    }
    slist = slist->next;
  }
}


void after_aspect_width_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  if (lives_lock_button_get_locked(LIVES_BUTTON(aspect.lockbutton))) {
    int width = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    double height = (double)width / aspect.ratio;
    LiVESWidget *spbutton = aspect.height_param->widgets[0];
    lives_signal_handler_block(spbutton, aspect.height_func);
    aspect.width_param->change_blocked = TRUE;
    height = lives_spin_button_get_snapval(LIVES_SPIN_BUTTON(spbutton), height);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(spbutton), height);
    lives_spin_button_update(LIVES_SPIN_BUTTON(spbutton));
    lives_signal_handler_unblock(spbutton, aspect.height_func);
    aspect.width_param->change_blocked = FALSE;
  }
}


void after_aspect_height_changed(LiVESToggleButton * spinbutton, livespointer user_data) {
  if (lives_lock_button_get_locked(LIVES_BUTTON(aspect.lockbutton))) {
    int height = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    double width = (double)height * aspect.ratio;
    LiVESWidget *spbutton = aspect.width_param->widgets[0];
    lives_signal_handler_block(spbutton, aspect.width_func);
    aspect.height_param->change_blocked = TRUE;
    width = lives_spin_button_get_snapval(LIVES_SPIN_BUTTON(spbutton), width);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(spbutton), width);
    lives_spin_button_update(LIVES_SPIN_BUTTON(spbutton));
    lives_signal_handler_unblock(spbutton, aspect.width_func);
    aspect.height_param->change_blocked = FALSE;
  }
}


boolean check_filewrite_overwrites(void) {
  // check all writeable files which were user edited (param->edited), to make sure we don't overwrite without permission
  // if the value was set by the file button we would have checked there, and param->edited will be FALSE

  if (filewrite != NULL) {
    LiVESList *slist = filewrite;
    while (slist != NULL) {
      lives_param_t *param = (lives_param_t *)(slist->data);
      if (param->edited) {
        // check for overwrite
        if (LIVES_IS_ENTRY(param->widgets[0])) {
          if (strlen(lives_entry_get_text(LIVES_ENTRY(param->widgets[0])))) {
            if (!check_file(lives_entry_get_text(LIVES_ENTRY(param->widgets[0])), TRUE)) {
              return FALSE;
            }
          }
        }
      }
      slist = slist->next;
    }
  }
  return TRUE;
}


boolean special_cleanup(boolean is_ok) {
  // free some memory now
  if (special_inited) {
    if (is_ok && !check_filewrite_overwrites()) return FALSE;
    aspect.no_reset = FALSE;

    mainw->framedraw = mainw->framedraw_reset = NULL;
    mainw->framedraw_spinbutton = NULL;

    if (mainw->fd_layer != NULL) weed_layer_free(mainw->fd_layer);
    mainw->fd_layer = NULL;

    if (mainw->fd_layer_orig != NULL) weed_layer_free(mainw->fd_layer_orig);
    mainw->fd_layer_orig = NULL;

    mainw->framedraw_preview = NULL;

    if (framedraw.extra_params != NULL) lives_free(framedraw.extra_params);
    framedraw.extra_params = NULL;

    framedraw.type = LIVES_PARAM_SPECIAL_TYPE_NONE;

    if (fileread != NULL) lives_list_free(fileread);
    fileread = NULL;

    if (filewrite != NULL) lives_list_free(filewrite);
    filewrite = NULL;

    if (passwd_widgets != NULL) lives_list_free(passwd_widgets);
    passwd_widgets = NULL;

    fchooser.nwidgets = 0;

    framedraw.added = FALSE;
    special_inited = FALSE;
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE void set_aspect_ratio_widgets(lives_param_t *w, lives_param_t *h) {
  aspect.width_param = w;
  aspect.height_param = h;
}


void setmergealign(void) {
  lives_param_t *param;
  int cb_frames = clipboard->frames;

  if (prefs->ins_resample && clipboard->fps != cfile->fps) {
    cb_frames = count_resampled_frames(clipboard->frames, clipboard->fps, cfile->fps);
  }

  if (cfile->end - cfile->start + 1 > (cb_frames * ((merge_opts != NULL && merge_opts->spinbutton_loops != NULL) ?
                                       lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(merge_opts->spinbutton_loops)) : 1))
      && !merge_opts->loop_to_fit) {
    // set special transalign widgets to their default values
    if (mergealign.start_param != NULL && mergealign.start_param->widgets[0] != NULL && LIVES_IS_SPIN_BUTTON
        (mergealign.start_param->widgets[0]) && (param = mergealign.start_param)->type == LIVES_PARAM_NUM) {
      if (param->dp) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), get_double_param(param->def));
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)get_int_param(param->def));
      }
    }
    if (mergealign.end_param != NULL && mergealign.end_param->widgets[0] != NULL && LIVES_IS_SPIN_BUTTON
        (mergealign.end_param->widgets[0]) && (param = mergealign.end_param)->type == LIVES_PARAM_NUM) {
      if (param->dp) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), get_double_param(param->def));
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)get_int_param(param->def));
      }
    }
  } else {
    if (merge_opts->align_start) {
      // set special transalign widgets to min/max values
      if (mergealign.start_param != NULL && mergealign.start_param->widgets[0] != NULL && LIVES_IS_SPIN_BUTTON
          (mergealign.start_param->widgets[0]) && (param = mergealign.start_param)->type == LIVES_PARAM_NUM) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)param->min);
      }
      if (mergealign.end_param != NULL && mergealign.end_param->widgets[0] != NULL && LIVES_IS_SPIN_BUTTON
          (mergealign.end_param->widgets[0]) && (param = mergealign.end_param)->type == LIVES_PARAM_NUM) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)param->max);
      }
    } else {
      // set special transalign widgets to max/min values
      if (mergealign.start_param != NULL && mergealign.start_param->widgets[0] != NULL && LIVES_IS_SPIN_BUTTON
          (mergealign.start_param->widgets[0]) && (param = mergealign.start_param)->type == LIVES_PARAM_NUM) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)param->max);
      }
      if (mergealign.end_param != NULL && mergealign.end_param->widgets[0] != NULL && LIVES_IS_SPIN_BUTTON
          (mergealign.end_param->widgets[0]) && (param = mergealign.end_param)->type == LIVES_PARAM_NUM) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)param->min);
      }
    }
  }
}


LiVESPixbuf *mt_framedraw(lives_mt * mt, weed_layer_t *layer) {
  LiVESPixbuf *pixbuf = NULL;

  if (framedraw.added) {
    switch (framedraw.type) {
    case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
      if (mt->track_index == -1) {
        // TODO - hide widgets
      } else {
        //
      }
      break;

    default:
      break;
    }

    // draw on top of layer
    framedraw_redraw(&framedraw, layer);
  }
  return pixbuf;
}


boolean is_perchannel_multi(lives_rfx_t *rfx, int i) {
  // updated for weed spec 1.1
  if (rfx->params[i].multi == PVAL_MULTI_PER_CHANNEL) return TRUE;
  return FALSE;
}

