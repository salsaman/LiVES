// paramwindow.c
// LiVES
// (c) G. Finch 2004 - 2018 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// dynamic window generation from parameter arrays :-)

#include "main.h"
#include "paramwindow.h"
#include "callbacks.h"
#include "resample.h"
#include "effects.h"
#include "rte_window.h"
#include "framedraw.h"
#include "ce_thumbs.h"
#include "interface.h"

#ifdef ENABLE_GIW
#include "giw/giwknob.h"
#endif

static int ireinit = 0;

extern boolean do_effect(lives_rfx_t *, boolean is_preview);  //effects.c in LiVES
extern void on_realfx_activate(LiVESMenuItem *, livespointer rfx);  // effects.c in LiVES

static void after_param_text_buffer_changed(LiVESTextBuffer *textbuffer, lives_rfx_t *rfx);

// TODO -
// use list of these in case we have multiple windows open
// right now this is single threaded because of this
static LiVESSList *usrgrp_to_livesgrp[2] = {NULL, NULL}; // ordered list of lives_widget_group_t

LiVESList *do_onchange_init(lives_rfx_t *rfx) {
  LiVESList *onchange = NULL;
  LiVESList *retvals = NULL;
  char **array;
  char *type;

  register int i;

  if (rfx->status == RFX_STATUS_WEED) return NULL;

  switch (rfx->status) {
  case RFX_STATUS_SCRAP:
    type = lives_strdup(PLUGIN_RFX_SCRAP);
    break;
  case RFX_STATUS_BUILTIN:
    type = lives_strdup(PLUGIN_RENDERED_EFFECTS_BUILTIN);
    break;
  case RFX_STATUS_CUSTOM:
    type = lives_strdup(PLUGIN_RENDERED_EFFECTS_CUSTOM);
    break;
  default:
    type = lives_strdup_printf(PLUGIN_RENDERED_EFFECTS_TEST);
    break;
  }
  if ((onchange = plugin_request_by_line(type, rfx->name, "get_onchange")) != NULL) {
    for (i = 0; i < lives_list_length(onchange); i++) {
      array = lives_strsplit((char *)lives_list_nth_data(onchange, i), rfx->delim, -1);
      if (!strcmp(array[0], "init")) {
        // onchange is init
        // create dummy object with data
        LiVESWidget *dummy_widget = lives_label_new(NULL);
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dummy_widget), PARAM_NUMBER_KEY,
                                     LIVES_INT_TO_POINTER(-1));
        retvals = do_onchange(LIVES_WIDGET_OBJECT(dummy_widget), rfx);
        lives_widget_destroy(dummy_widget);
        lives_strfreev(array);
        break;
      }
      lives_strfreev(array);
    }
    lives_list_free_all(&onchange);
  }
  lives_free(type);

  return retvals;
}


static void on_paramwindow_button_clicked2(LiVESButton *button, lives_rfx_t *rfx) {
  // close from rte window
  on_paramwindow_button_clicked(button, rfx);
  lives_freep((void **)&fx_dialog[1]);
}


void on_paramwindow_button_clicked(LiVESButton *button, lives_rfx_t *rfx) {
  LiVESWidget *dialog = NULL;
  boolean def_ok = FALSE;
  int i;

  if (button) {
    lives_widget_set_sensitive(LIVES_WIDGET(button), FALSE);
    dialog = lives_widget_get_toplevel(LIVES_WIDGET(button));
  }

  if (dialog && LIVES_IS_DIALOG(dialog)) {
    if (lives_dialog_get_response_for_widget(LIVES_DIALOG(dialog), LIVES_WIDGET(button)) == LIVES_RESPONSE_OK) {
      def_ok = TRUE;
    }
  }

  if (mainw->textwidget_focus && LIVES_IS_WIDGET_OBJECT(mainw->textwidget_focus)) {
    // make sure text widgets are updated if they activate the default
    LiVESWidget *textwidget =
      (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->textwidget_focus), TEXTWIDGET_KEY);
    after_param_text_changed(textwidget, rfx);
  }

  if (!special_cleanup(def_ok)) {
    lives_dialog_response(LIVES_DIALOG(lives_widget_get_toplevel(LIVES_WIDGET(button))), LIVES_RESPONSE_RETRY);
    if (button) lives_widget_set_sensitive(LIVES_WIDGET(button), TRUE);
    return;
  }

  mainw->textwidget_focus = NULL;

  if (def_ok && rfx && rfx->status != RFX_STATUS_SCRAP) mainw->keep_pre = mainw->did_rfx_preview;

  mainw->block_param_updates = TRUE;

  if (mainw->did_rfx_preview) {
    if (def_ok) {
      for (i = 0; i < rfx->num_params; i++) {
        if (rfx->params[i].changed) {
          mainw->keep_pre = FALSE;
          break;
        }
      }
    }

    if (!mainw->keep_pre) {
      lives_kill_subprocesses(cfile->handle, TRUE);

      if (cfile->start == 0) {
        cfile->start = 1;
        cfile->end = cfile->frames;
      }

      do_rfx_cleanup(rfx);
      mainw->did_rfx_preview = FALSE;
    }
    mainw->show_procd = TRUE;
  }

  if (!def_ok) {
    if (rfx && mainw->is_generating && rfx->source_type == LIVES_RFX_SOURCE_NEWCLIP &&
        CURRENT_CLIP_IS_NORMAL && rfx->source == cfile &&
        rfx->name && rfx->status != RFX_STATUS_WEED && rfx->status != RFX_STATUS_SCRAP &&
        rfx->num_in_channels == 0 && rfx->min_frames >= 0 && !rfx->is_template) {
      // for a generator, we silently close the (now) temporary file we would have generated frames into
      mainw->suppress_dprint = TRUE;
      close_current_file(mainw->pre_src_file);
      mainw->suppress_dprint = FALSE;
      if (mainw->multitrack) mainw->pre_src_file = -1;
      mainw->is_generating = FALSE;
      rfx->source = NULL;
      rfx->source_type = LIVES_RFX_SOURCE_RFX;
    }
    mainw->keep_pre = FALSE;
  }

  if (!rfx) {
    if (usrgrp_to_livesgrp[1]) lives_slist_free(usrgrp_to_livesgrp[1]);
    usrgrp_to_livesgrp[1] = NULL;
  } else {
    if (rfx->status == RFX_STATUS_WEED) {
      if (usrgrp_to_livesgrp[1]) lives_slist_free(usrgrp_to_livesgrp[1]);
      usrgrp_to_livesgrp[1] = NULL;
      if (rfx != mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx) {
        rfx_free(rfx);
        lives_free(rfx);
      }
    } else {
      if (usrgrp_to_livesgrp[0]) lives_slist_free(usrgrp_to_livesgrp[0]);
      usrgrp_to_livesgrp[0] = NULL;
    }
  }

  mainw->block_param_updates = FALSE;

  if (def_ok && rfx && rfx->status == RFX_STATUS_SCRAP) return;

  if (button)
    if (dialog) {
      // prevent a gtk+ crash by removing the focus before detroying the dialog
      LiVESWidget *content_area = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
      lives_container_set_focus_child(LIVES_CONTAINER(content_area), NULL);
    }

  lives_general_button_clicked(button, NULL);

  if (def_ok) {
    if (rfx->status == RFX_STATUS_WEED) on_realfx_activate(NULL, rfx);
    else on_render_fx_activate(NULL, rfx);
  }

  if (mainw->multitrack) {
    polymorph(mainw->multitrack, POLY_NONE);
    polymorph(mainw->multitrack, POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
  }
}


/**
   get a (radiobutton) list from an index
*/
static lives_widget_group_t *get_group(lives_rfx_t *rfx, lives_param_t *param) {
  if (rfx->status == RFX_STATUS_WEED) {
    return livesgrp_from_usrgrp(usrgrp_to_livesgrp[1], param->group);
  } else {
    return livesgrp_from_usrgrp(usrgrp_to_livesgrp[0], param->group);
  }
  return NULL;
}


void on_render_fx_activate(LiVESMenuItem *menuitem, lives_rfx_t *rfx) {
  uint32_t chk_mask = 0;

  if (menuitem != NULL && rfx->num_in_channels > 0) {
    chk_mask = WARN_MASK_LAYOUT_ALTER_FRAMES;
    if (!check_for_layout_errors(NULL, mainw->current_file, cfile->start, cfile->end, &chk_mask)) {
      return;
    }
  }

  // do onchange|init
  if (menuitem != NULL) {
    LiVESList *retvals = do_onchange_init(rfx);
    lives_list_free_all(&retvals);
  }
  if (rfx->min_frames > -1) {
    do_effect(rfx, FALSE);
  }

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
}


static void gen_width_changed(LiVESSpinButton *spin, livespointer user_data) {
  weed_plant_t *ctmpl = (weed_plant_t *)user_data;
  int val = lives_spin_button_get_value_as_int(spin);
  int error, old_val = 0;
  int step;
  // value in chantmp in pixels, not macropixels
  if (weed_plant_has_leaf(ctmpl, WEED_LEAF_HOST_WIDTH)) old_val = weed_get_int_value(ctmpl, WEED_LEAF_HOST_WIDTH, &error);
  if (val == old_val) return;
  step = 1;
  if (weed_plant_has_leaf(ctmpl, WEED_LEAF_HSTEP)) step = weed_get_int_value(ctmpl, WEED_LEAF_HSTEP, &error);

  val = ALIGN_CEIL(val, step);
  weed_set_int_value(ctmpl, WEED_LEAF_HOST_WIDTH, val);
  lives_spin_button_set_value(spin, (double)val);
}


static void gen_height_changed(LiVESSpinButton *spin, livespointer user_data) {
  weed_plant_t *ctmpl = (weed_plant_t *)user_data;
  int val = lives_spin_button_get_value_as_int(spin);
  int error, old_val = 0;
  int step;

  if (weed_plant_has_leaf(ctmpl, WEED_LEAF_HOST_HEIGHT)) old_val = weed_get_int_value(ctmpl, WEED_LEAF_HOST_HEIGHT, &error);

  if (val == old_val) return;
  step = 1;
  if (weed_plant_has_leaf(ctmpl, WEED_LEAF_HSTEP)) step = weed_get_int_value(ctmpl, WEED_LEAF_HSTEP, &error);

  val = ALIGN_CEIL(val, step);
  weed_set_int_value(ctmpl, WEED_LEAF_HOST_HEIGHT, val);
  lives_spin_button_set_value(spin, (double)val);
}


static void gen_fps_changed(LiVESSpinButton *spin, livespointer user_data) {
  weed_plant_t *filter = (weed_plant_t *)user_data;
  double val = lives_spin_button_get_value(spin);
  weed_set_double_value(filter, WEED_LEAF_HOST_FPS, val);
}


static void trans_in_out_pressed(lives_rfx_t *rfx, boolean in) {
  weed_plant_t **in_params;

  weed_plant_t *inst = (weed_plant_t *)rfx->source;
  weed_plant_t *filter = weed_instance_get_filter(inst, TRUE);
  weed_plant_t *tparam;
  weed_plant_t *tparamtmpl;

  int key = -1;
  int ptype, nparams;
  int trans = get_transition_param(filter, FALSE);

  do {
    // handle compound fx
    if (weed_plant_has_leaf(inst, WEED_LEAF_IN_PARAMETERS)) {
      nparams = weed_leaf_num_elements(inst, WEED_LEAF_IN_PARAMETERS);
      if (trans < nparams) break;
      trans -= nparams;
    }
  } while (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE) &&
           (inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL)) != NULL);

  in_params = weed_instance_get_in_params(inst, NULL);
  tparam = in_params[trans];
  tparamtmpl = weed_param_get_template(tparam);
  ptype = weed_paramtmpl_get_type(tparamtmpl);

  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
  if (!filter_mutex_trylock(key)) {
    if (ptype == WEED_PARAM_INTEGER) {
      if (in) weed_set_int_value(tparam, WEED_LEAF_VALUE, weed_get_int_value(tparamtmpl, WEED_LEAF_MIN, NULL));
      else weed_set_int_value(tparam, WEED_LEAF_VALUE, weed_get_int_value(tparamtmpl, WEED_LEAF_MAX, NULL));
    } else {
      if (in) weed_set_double_value(tparam, WEED_LEAF_VALUE, weed_get_double_value(tparamtmpl, WEED_LEAF_MIN, NULL));
      else weed_set_double_value(tparam, WEED_LEAF_VALUE, weed_get_double_value(tparamtmpl, WEED_LEAF_MAX, NULL));
    }
    filter_mutex_unlock(key);
    set_copy_to(inst, trans, rfx, TRUE);
  }

  update_visual_params(rfx, FALSE);
  lives_free(in_params);
  activate_mt_preview(mainw->multitrack);
}


static void transition_in_pressed(LiVESToggleButton *tbut, livespointer rfx) {
  trans_in_out_pressed((lives_rfx_t *)rfx, TRUE);
}


static void transition_out_pressed(LiVESToggleButton *tbut, livespointer rfx) {
  trans_in_out_pressed((lives_rfx_t *)rfx, FALSE);
}


static void after_transaudio_toggled(LiVESToggleButton *togglebutton, livespointer rfx) {
  weed_plant_t *init_event = mainw->multitrack->init_event;

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(togglebutton)))
    weed_set_boolean_value(init_event, WEED_LEAF_HOST_AUDIO_TRANSITION, WEED_TRUE);
  else weed_set_boolean_value(init_event, WEED_LEAF_HOST_AUDIO_TRANSITION, WEED_FALSE);
}


void transition_add_in_out(LiVESBox *vbox, lives_rfx_t *rfx, boolean add_audio_check) {
  // add in/out radios for multitrack transitions
  LiVESWidget *radiobutton_in;
  LiVESWidget *radiobutton_out;
  LiVESWidget *radiobutton_dummy;
  LiVESWidget *hbox, *hbox2;
  LiVESWidget *hseparator;

  LiVESSList *radiobutton_group = NULL;

  weed_plant_t *filter = weed_instance_get_filter((weed_plant_t *)rfx->source, TRUE);
  int trans = get_transition_param(filter, FALSE);

  char *tmp, *tmp2;

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_width);

  // dummy radiobutton so we can have neither in nor out set
  radiobutton_dummy = lives_standard_radio_button_new(NULL, &radiobutton_group, LIVES_BOX(hbox), NULL);
  lives_widget_set_no_show_all(radiobutton_dummy, TRUE);

  radiobutton_in = lives_standard_radio_button_new((tmp = (_("Transition _In"))),
                   &radiobutton_group, LIVES_BOX(hbox),
                   (tmp2 = (_("Click to set the transition parameter to show only the front frame"))));
  lives_free(tmp); lives_free(tmp2);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(radiobutton_in), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(transition_in_pressed), (livespointer)rfx);

  if (add_audio_check) {
    int error;

    LiVESWidget *checkbutton;

    hbox2 = lives_hbox_new(FALSE, 0);

    if (has_video_chans_in(filter, FALSE))
      lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, FALSE, widget_opts.packing_width);

    checkbutton = lives_standard_check_button_new((tmp = (_("_Crossfade audio"))),
                  weed_plant_has_leaf(mainw->multitrack->init_event, WEED_LEAF_HOST_AUDIO_TRANSITION) &&
                  weed_get_boolean_value(mainw->multitrack->init_event, WEED_LEAF_HOST_AUDIO_TRANSITION, &error) == WEED_TRUE,
                  LIVES_BOX(hbox2), (tmp2 = lives_strdup(
                      _("If checked, audio from both layers is mixed relative to the transition parameter.\n"
                        "The setting is applied instantly to the entire transition."))));

    lives_free(tmp);
    lives_free(tmp2);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_transaudio_toggled),
                                    (livespointer)rfx);

    after_transaudio_toggled(LIVES_TOGGLE_BUTTON(checkbutton), (livespointer)rfx);
  }

  widget_opts.pack_end = TRUE;
  radiobutton_out = lives_standard_radio_button_new((tmp = (_("Transition _Out"))),
                    &radiobutton_group, LIVES_BOX(hbox),
                    (tmp2 = (_("Click to set the transition parameter to show only the rear frame"))));

  lives_free(tmp);
  lives_free(tmp2);

  widget_opts.pack_end = FALSE;

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(radiobutton_out), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(transition_out_pressed),
                                  (livespointer)rfx);


  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  hseparator = lives_hseparator_new();
  lives_box_pack_start(vbox, hseparator, FALSE, FALSE, 0);

  rfx->params[trans].widgets[WIDGET_RB_IN] = radiobutton_in;
  rfx->params[trans].widgets[WIDGET_RB_OUT] = radiobutton_out;
  rfx->params[trans].widgets[WIDGET_RB_DUMMY] = radiobutton_dummy;
}


static boolean add_sizes(LiVESBox *vbox, boolean add_fps, boolean has_param, lives_rfx_t *rfx) {
  // add size settings for generators and resize effects
  LiVESWidget *label, *hbox;
  LiVESWidget *spinbuttonh = NULL, *spinbuttonw = NULL;
  LiVESWidget *spinbuttonf;
  int num_chans = 0;

  weed_plant_t *filter = weed_instance_get_filter((weed_plant_t *)rfx->source, TRUE), *tmpl;
  weed_plant_t **ctmpls = weed_get_plantptr_array_counted(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &num_chans);

  double def_fps = 0.;

  char *cname, *ltxt;

  boolean chk_params = (vbox == NULL);
  boolean added = FALSE;

  int def_width = 0, max_width, width_step;
  int def_height = 0, max_height, height_step;
  int wopw = widget_opts.packing_width;

  register int i;

  if (chk_params) {
    if (add_fps) return TRUE;
  } else {
    if (!has_param) lives_widget_set_size_request(LIVES_WIDGET(vbox), RFX_WINSIZE_H, RFX_WINSIZE_V);

    if (add_fps) {
      added = TRUE;

      hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height * 2);

      add_fill_to_box(LIVES_BOX(hbox));

      if (weed_plant_has_leaf(filter, WEED_LEAF_HOST_FPS)) def_fps = weed_get_double_value(filter, WEED_LEAF_HOST_FPS, NULL);
      else if (weed_plant_has_leaf(filter, WEED_LEAF_PREFERRED_FPS))
        def_fps = weed_get_double_value(filter, WEED_LEAF_PREFERRED_FPS, NULL);

      if (def_fps == 0.) def_fps = prefs->default_fps;

      spinbuttonf = lives_standard_spin_button_new(_("Target _FPS (plugin may override this)"),
                    def_fps, 1., FPS_MAX, 1., 10., 3, LIVES_BOX(hbox), NULL);

      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbuttonf), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                      LIVES_GUI_CALLBACK(gen_fps_changed), filter);

      add_fill_to_box(LIVES_BOX(hbox));
    }
  }

  for (i = 0; i < num_chans; i++) {
    tmpl = ctmpls[i];

    // TODO ***: allow alteration of "host_disabled" under some circumstances
    // (e.g. allow enabling a first or second in channel, or first out_channel, or more for alphas)

    // make this into function called from here and from effects with optional enable-able channels
    if (weed_get_boolean_value(tmpl, WEED_LEAF_HOST_DISABLED, NULL) == WEED_TRUE) continue;
    if (weed_get_int_value(tmpl, WEED_LEAF_WIDTH, NULL)) continue;
    if (weed_get_int_value(tmpl, WEED_LEAF_HEIGHT, NULL)) continue;

    if (chk_params) return TRUE;

    added = TRUE;

    if (rfx->is_template) {
      cname = weed_get_string_value(tmpl, WEED_LEAF_NAME, NULL);
      ltxt = lives_strdup_printf(_("%s : size"), cname);
      lives_free(cname);
    } else {
      ltxt = (_("New size (pixels)"));
    }

    label = lives_standard_label_new(ltxt);
    lives_free(ltxt);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

    def_width = weed_get_int_value(tmpl, WEED_LEAF_HOST_WIDTH, NULL);
    if (!def_width) def_width = DEF_GEN_WIDTH;
    max_width = weed_get_int_value(tmpl, WEED_LEAF_MAXWIDTH, NULL);
    if (!max_width) max_width = INT_MAX;
    if (def_width > max_width) def_width = max_width;
    width_step = weed_get_int_value(tmpl, WEED_LEAF_HSTEP, NULL);
    if (!width_step) width_step = 4;

    spinbuttonw = lives_standard_spin_button_new(_("_Width"), def_width, width_step, max_width, width_step,
                  width_step, 0, LIVES_BOX(hbox), NULL);
    lives_spin_button_set_snap_to_multiples(LIVES_SPIN_BUTTON(spinbuttonw), width_step);
    lives_spin_button_update(LIVES_SPIN_BUTTON(spinbuttonw));

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbuttonw), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(gen_width_changed), tmpl);
    weed_leaf_delete(tmpl, WEED_LEAF_HOST_WIDTH); // force a reset
    gen_width_changed(LIVES_SPIN_BUTTON(spinbuttonw), tmpl);

    widget_opts.packing_width >>= 1;
    add_fill_to_box(LIVES_BOX(hbox));
    widget_opts.packing_width = wopw;

    def_height = weed_get_int_value(tmpl, WEED_LEAF_HOST_HEIGHT, NULL);
    if (!def_height) def_height = DEF_GEN_HEIGHT;
    max_height = weed_get_int_value(tmpl, WEED_LEAF_MAXHEIGHT, NULL);
    if (!max_height) max_height = INT_MAX;
    if (def_height > max_height) def_height = max_height;
    height_step = weed_get_int_value(tmpl, WEED_LEAF_VSTEP, NULL);
    if (!height_step) height_step = 4;

    spinbuttonh = lives_standard_spin_button_new(_("_Height"), def_height, height_step, max_height, height_step,
                  height_step, 0, LIVES_BOX(hbox), NULL);
    lives_spin_button_set_snap_to_multiples(LIVES_SPIN_BUTTON(spinbuttonh), height_step);
    lives_spin_button_update(LIVES_SPIN_BUTTON(spinbuttonh));

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbuttonh), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(gen_height_changed), tmpl);
    weed_leaf_delete(tmpl, WEED_LEAF_HOST_HEIGHT); // force a reset
    gen_height_changed(LIVES_SPIN_BUTTON(spinbuttonh), tmpl);
  }

  if (!rfx->is_template && num_chans == 1) {
    if (chk_params) return TRUE;
    added = TRUE;
    // add "aspectratio" widget
    add_aspect_ratio_button(LIVES_SPIN_BUTTON(spinbuttonw), LIVES_SPIN_BUTTON(spinbuttonh), LIVES_BOX(vbox));
  }

  if (added) {
    if (has_param) {
      add_fill_to_box(LIVES_BOX(vbox));
      add_hsep_to_box(vbox);
    } else has_param = TRUE;
  }
  return has_param;
}


static void add_gen_to(LiVESBox *vbox, lives_rfx_t *rfx) {
  // add "generate to clipboard/new clip" for rendered generators
  LiVESSList *radiobutton_group = NULL;

  LiVESWidget *radiobutton;
  LiVESWidget *hseparator;

  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);

  char *tmp, *tmp2;

  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  radiobutton = lives_standard_radio_button_new((tmp = (_("Generate to _Clipboard"))),
                &radiobutton_group, LIVES_BOX(hbox),
                (tmp2 = (_("Generate frames to the clipboard"))));

  lives_free(tmp);
  lives_free(tmp2);

  widget_opts.pack_end = TRUE;
  radiobutton = lives_standard_radio_button_new((tmp = (_("Generate to _New Clip"))),
                &radiobutton_group, LIVES_BOX(hbox),
                (tmp2 = (_("Generate frames to a new clip"))));
  widget_opts.pack_end = FALSE;

  lives_free(tmp);
  lives_free(tmp2);

  hseparator = lives_hseparator_new();
  lives_box_pack_start(vbox, hseparator, FALSE, FALSE, 0);

  toggle_toggles_var(LIVES_TOGGLE_BUTTON(radiobutton), &mainw->gen_to_clipboard, TRUE);
}


static void xspinw_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  cfile->ohsize = cfile->hsize = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  reset_framedraw_preview();
}

static void xspinh_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  cfile->ovsize = cfile->vsize = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  reset_framedraw_preview();
}

static void xspinfr_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  cfile->end = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
}

static void xspinfps_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  cfile->pb_fps = cfile->fps = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  reset_framedraw_preview();
}

static void add_genparams(LiVESWidget *vbox, lives_rfx_t *rfx) {
  // add nframes, fps, width, heights
  LiVESWidget *sp_width, *sp_height, *sp_frames, *sp_fps;
  LiVESWidget *frame = add_video_options(&sp_width, cfile->hsize, &sp_height, cfile->vsize, &sp_fps, cfile->fps,
                                         &sp_frames, cfile->end, TRUE, NULL);
  lives_box_pack_start(LIVES_BOX(vbox), frame, FALSE, TRUE, 0);

  lives_spin_button_update(LIVES_SPIN_BUTTON(sp_width));
  lives_spin_button_update(LIVES_SPIN_BUTTON(sp_height));
  if (sp_frames) lives_spin_button_update(LIVES_SPIN_BUTTON(sp_frames));
  lives_spin_button_update(LIVES_SPIN_BUTTON(sp_fps));
  cfile->ohsize = cfile->hsize = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_width));
  cfile->ovsize = cfile->vsize = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_height));

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(sp_width), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(xspinw_changed), NULL);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(sp_height), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(xspinh_changed), NULL);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(sp_frames), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(xspinfr_changed), NULL);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(sp_fps), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(xspinfps_changed), NULL);
}


LIVES_GLOBAL_INLINE void on_render_fx_pre_activate(LiVESMenuItem *menuitem, lives_rfx_t *rfx) {
  _fx_dialog *fxdialog;
  uint32_t chk_mask;
  int start, end;

  if (!check_storage_space(mainw->current_file, FALSE)) return;
  if ((rfx->props & RFX_PROPS_MAY_RESIZE && rfx->num_in_channels == 1) || rfx->min_frames < 0) {
    start = 1;
    end = 0;
  } else {
    start = cfile->start;
    end = cfile->end;
  }

  if (rfx->num_in_channels > 0) {
    chk_mask = WARN_MASK_LAYOUT_ALTER_FRAMES;
    if (!check_for_layout_errors(NULL, mainw->current_file, start, end, &chk_mask)) {
      return;
    }
  }
  fxdialog = on_fx_pre_activate(rfx, FALSE, NULL);
  if (fxdialog) {
    if (menuitem == LIVES_MENU_ITEM(mainw->resize_menuitem)) add_resnn_label(LIVES_DIALOG(fxdialog->dialog));
    /* do { */
    /*   resp = lives_dialog_run(LIVES_DIALOG(fxdialog->dialog)); */
    /* } while (resp == LIVES_RESPONSE_RETRY); */
  }

}


_fx_dialog *on_fx_pre_activate(lives_rfx_t *rfx, boolean is_realtime, LiVESWidget *pbox) {
  // render a pre dialog for: rendered effects (fx_dialog[0]), or rte(fx_dialog[1]), or encoder plugin, or vpp (fx_dialog[1])
  LiVESWidget *top_dialog_vbox = NULL;
  LiVESAccelGroup *fxw_accel_group;
  LiVESList *retvals = NULL;

  char *txt;

  boolean no_process = FALSE;
  boolean is_defaults = FALSE;
  boolean add_reset_ok = FALSE;
  boolean has_param;

  int scrw, didx = 0;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (is_realtime) {
    didx = 1;
    no_process = TRUE;
  } else if (rfx->status != RFX_STATUS_WEED) {
    retvals = do_onchange_init(rfx);
  }
  if (rfx->min_frames < 0) no_process = TRUE;

  if (!no_process && rfx->num_in_channels == 0) {
    int new_file;
    mainw->pre_src_file = mainw->current_file;

    // create a new file to generate frames into
    if (!get_new_handle((new_file = mainw->first_free_file), NULL)) {
      if (mainw->multitrack != NULL) {
        mt_sensitise(mainw->multitrack);
        mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
      }

      lives_list_free_all(&retvals);

      return NULL;
    }

    if (CURRENT_CLIP_IS_NORMAL) {
      mainw->files[new_file]->hsize = cfile->hsize;
      mainw->files[new_file]->vsize = cfile->vsize;
      mainw->files[new_file]->fps = cfile->fps;
    } else {
      mainw->files[new_file]->hsize = DEF_GEN_WIDTH;
      mainw->files[new_file]->vsize = DEF_GEN_HEIGHT;
      mainw->files[new_file]->fps = DEF_FPS;
    }

    mainw->is_generating = TRUE;
    mainw->current_file = new_file;
    rfx->source_type = LIVES_RFX_SOURCE_NEWCLIP;
    rfx->source = cfile;

    cfile->ohsize = cfile->hsize;
    cfile->ovsize = cfile->vsize;
    cfile->pb_fps = cfile->fps;

    // dummy values
    cfile->start = 1;
    cfile->end = 100;
  }

  if (!no_process && rfx->num_in_channels > 0) {
    // check we have a real clip open
    if (!CURRENT_CLIP_IS_VALID) {
      lives_list_free_all(&retvals);
      return NULL;
    }
    if (cfile->end - cfile->start + 1 < rfx->min_frames) {
      lives_list_free_all(&retvals);
      txt = lives_strdup_printf(_("\nYou must select at least %d frames to use this effect.\n\n"),
                                rfx->min_frames);
      do_error_dialog(txt);
      lives_free(txt);
      return NULL;
    }

    // here we invalidate cfile->ohsize, cfile->ovsize
    cfile->ohsize = cfile->hsize;
    cfile->ovsize = cfile->vsize;

    if (cfile->undo_action == UNDO_RESIZABLE) {
      set_undoable(NULL, FALSE);
    }
  }

  if (rfx->status == RFX_STATUS_WEED && rfx->is_template) is_defaults = TRUE;

  if (pbox == NULL) {
    char *title, *defstr;
    if (rfx->status == RFX_STATUS_WEED || no_process || (rfx->num_in_channels == 0 &&
        rfx->props & RFX_PROPS_BATCHG)) scrw = RFX_WINSIZE_H * 2. * widget_opts.scale;
    else scrw = GUI_SCREEN_WIDTH - SCR_WIDTH_SAFETY;

    fx_dialog[didx] = (_fx_dialog *)lives_malloc(sizeof(_fx_dialog));
    fx_dialog[didx]->okbutton = fx_dialog[didx]->cancelbutton = fx_dialog[didx]->resetbutton = NULL;
    fx_dialog[didx]->rfx = NULL;
    fx_dialog[didx]->key = fx_dialog[didx]->mode = -1;
    if (is_defaults) defstr = (_("Defaults for "));
    else defstr = lives_strdup("");
    title = lives_strdup_printf("%s%s", defstr, _(rfx->menu_text[0] == '_' ? rfx->menu_text + 1 : rfx->menu_text));

    fx_dialog[didx]->dialog = lives_standard_dialog_new(title, FALSE, scrw, RFX_WINSIZE_V);
    lives_free(defstr);
    lives_free(title);
    pbox = top_dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(fx_dialog[didx]->dialog));
    fx_dialog[didx]->rfx = rfx;
    lives_widget_set_hexpand(pbox, TRUE);
  }

  if (rfx->status != RFX_STATUS_WEED && !no_process) {
    // rendered fx preview

    LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(top_dialog_vbox), hbox, TRUE, TRUE, 0);

    lives_widget_set_hexpand(hbox, TRUE);
    lives_widget_set_vexpand(hbox, TRUE);

    pbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox), pbox, TRUE, TRUE, 0);

    lives_widget_set_hexpand(pbox, TRUE);
    lives_widget_set_vexpand(pbox, TRUE);

    // add preview window
    if (rfx->num_in_channels > 0 || !(rfx->props & RFX_PROPS_BATCHG)) {
      mainw->framedraw_frame = cfile->start;
      widget_add_framedraw(LIVES_VBOX(pbox), cfile->start, cfile->end, !(rfx->props & RFX_PROPS_MAY_RESIZE),
                           cfile->hsize, cfile->vsize, rfx);
      if (rfx->props & RFX_PROPS_MAY_RESIZE) mainw->fd_max_frame = cfile->end;
    }

    if (!(rfx->props & RFX_PROPS_BATCHG)) {
      // connect spinbutton to preview
      fd_connect_spinbutton(rfx);
    }
  }

  // add the param widgets; here we also set parameters for any special widgets in the framedraw
  //main_thread_execute((lives_funcptr_t)make_param_box, WEED_SEED_BOOLEAN, &has_param, "vv", pbox, rfx);
  has_param = make_param_box(LIVES_VBOX(pbox), rfx);

  // update widgets from onchange_init here
  if (top_dialog_vbox != NULL) {
    fxw_accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
    lives_window_add_accel_group(LIVES_WINDOW(fx_dialog[didx]->dialog), fxw_accel_group);

    if (!no_process || is_defaults || rfx->status == RFX_STATUS_SCRAP) {
      if (!is_defaults) {
        fx_dialog[didx]->cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(fx_dialog[didx]->dialog),
                                        LIVES_STOCK_CANCEL, NULL, LIVES_RESPONSE_CANCEL);
        fx_dialog[didx]->okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(fx_dialog[didx]->dialog),
                                    LIVES_STOCK_OK, NULL, LIVES_RESPONSE_OK);
      } else add_reset_ok = TRUE;
    } else {
      if (rfx->status == RFX_STATUS_WEED) {
        add_reset_ok = TRUE;
      }
    }

    if (add_reset_ok) {
      fx_dialog[didx]->resetbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(fx_dialog[didx]->dialog),
                                     LIVES_STOCK_REVERT_TO_SAVED, _("Reset"), LIVES_RESPONSE_RESET);
      fx_dialog[didx]->okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(fx_dialog[didx]->dialog), LIVES_STOCK_APPLY,
                                  _("Set as default"), LIVES_RESPONSE_OK);
      if (!has_param) {
        lives_widget_set_sensitive(fx_dialog[didx]->resetbutton, FALSE);
        lives_widget_set_sensitive(fx_dialog[didx]->okbutton, FALSE);
      }
    }

    if (fx_dialog[didx]->cancelbutton == NULL) {
      fx_dialog[didx]->cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(fx_dialog[didx]->dialog), LIVES_STOCK_CLOSE,
                                      _("_Close Window"), LIVES_RESPONSE_CANCEL);
    }
    lives_widget_add_accelerator(fx_dialog[didx]->cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, fxw_accel_group,
                                 LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

    if (fx_dialog[didx]->okbutton != NULL) {
      lives_button_grab_default_special(fx_dialog[didx]->okbutton);
    } else {
      lives_button_grab_default_special(fx_dialog[didx]->cancelbutton);
    }

    if (no_process && !is_defaults) {
      if (!is_realtime) {
        if (fx_dialog[didx]->okbutton != NULL)
          lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_paramwindow_button_clicked), rfx);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_paramwindow_button_clicked), rfx);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->dialog), LIVES_WIDGET_DELETE_EVENT,
                                  LIVES_GUI_CALLBACK(on_paramwindow_button_clicked), rfx);
      } else {
        lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_paramwindow_button_clicked2), rfx);
        if (rfx->status == RFX_STATUS_SCRAP)
          lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_paramwindow_button_clicked2), rfx);
        else {
          if (fx_dialog[didx]->okbutton != NULL)
            lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                      LIVES_GUI_CALLBACK(rte_set_key_defs), rfx);
          if (fx_dialog[didx]->resetbutton != NULL) {
            lives_signal_sync_connect_after(LIVES_GUI_OBJECT(fx_dialog[didx]->resetbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                            LIVES_GUI_CALLBACK(rte_reset_defs_clicked), rfx);
          }
        }
        lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->dialog), LIVES_WIDGET_DELETE_EVENT,
                                  LIVES_GUI_CALLBACK(on_paramwindow_button_clicked2), rfx);
      }
    } else {
      if (!is_defaults) {
        if (fx_dialog[didx]->okbutton != NULL)
          lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_paramwindow_button_clicked), (livespointer)rfx);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_paramwindow_button_clicked), (livespointer)rfx);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->dialog), LIVES_WIDGET_DELETE_EVENT,
                                  LIVES_GUI_CALLBACK(on_paramwindow_button_clicked), (livespointer)rfx);
      } else {
        if (fx_dialog[didx]->okbutton != NULL)
          lives_signal_sync_connect_after(LIVES_GUI_OBJECT(fx_dialog[didx]->okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                          LIVES_GUI_CALLBACK(rte_set_defs_ok), rfx);
        if (fx_dialog[didx]->resetbutton != NULL) {
          lives_signal_sync_connect_after(LIVES_GUI_OBJECT(fx_dialog[didx]->resetbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                          LIVES_GUI_CALLBACK(rte_reset_defs_clicked), rfx);
        }
        lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(rte_set_defs_cancel), rfx);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(fx_dialog[didx]->dialog), LIVES_WIDGET_DELETE_EVENT,
                                  LIVES_GUI_CALLBACK(rte_set_defs_cancel), rfx);
      }
    }
  }

  // tweak some things to do with framedraw preview
  if (mainw->framedraw) fd_tweak(rfx);
  lives_widget_show_all(fx_dialog[didx]->dialog);

  if (retvals) {
    // now apply visually anything we got from onchange_init
    param_demarshall(rfx, retvals, TRUE, TRUE);
    lives_list_free_all(&retvals);
  }
  return fx_dialog[didx];
}


static void check_hidden_gui(weed_plant_t *inst, lives_param_t *param, int idx) {
  weed_plant_t *wparam;
  if ((param->reinit & REINIT_FUNCTIONAL) && (weed_get_int_value(inst, WEED_LEAF_HOST_REFS, NULL) == 2)) {
    // effect is running and user is editing the params, we should hide reinit params so as not to disturb the karma
    param->hidden |= HIDDEN_NEEDS_REINIT;
  } else if (param->hidden & HIDDEN_NEEDS_REINIT) param->hidden ^= HIDDEN_NEEDS_REINIT;

  wparam = weed_inst_in_param(inst, idx, FALSE, FALSE);
  if (wparam != NULL && weed_param_is_hidden(wparam))
    param->hidden |= HIDDEN_GUI;
  else if (param->hidden & HIDDEN_GUI) param->hidden ^= HIDDEN_GUI;
}


static int num_in_params_for_nth_instance(weed_plant_t *inst, int idx) {
  // get number of params for nth instance in a compound effect - gives an offset for param number within the compound
  while (--idx > 0) inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
  return weed_leaf_num_elements(inst, WEED_LEAF_IN_PARAMETERS);
}


static boolean fmt_match(char *fmt_string) {
  const char *myfmt = fmt_string, *xfmt = myfmt + FMT_STRING_SIZE;
  size_t xlen = lives_strlen(myfmt), ylen;

  // g_print("\nROW\n");
  if (xlen == 0) {
    //g_print("HSEP\n");
    return FALSE;
  }
  ylen = lives_strlen(xfmt);
  if (ylen == 0) {
    //g_print("2HSEP\n");
    return FALSE;
  }

  if (xlen < ylen) ylen = xlen;

  for (int j = 0; j < ylen; j++) {
    //g_print(" CF %d %d", myfmt[j], xfmt[j]);
    if (xfmt[j] != -1 && myfmt[j] != -1 && xfmt[j] != myfmt[j]) return FALSE;
    if ((xfmt[j] == -2 || myfmt[j] == -2) && xfmt[j] != myfmt[j]) return FALSE;
  }

  //g_print("\nMATch\n");
  return TRUE;
}


/**
   @brief make a dynamic parameter window

   if top_vbox is NULL: we just check for displayable params, returning FALSE there are none to be shown.
   otherwise, adds widgets to top_vbox, returning FALSE if nothing was added
*/
boolean make_param_box(LiVESVBox *top_vbox, lives_rfx_t *rfx) {
  lives_param_t *param = NULL;

  LiVESWidget *param_vbox = NULL;
  LiVESWidget *top_hbox = NULL;
  LiVESWidget *hbox = NULL;
  LiVESWidget *last_label = NULL;
  LiVESWidget *layoutx = NULL;
  LiVESWidget *dummy_label = NULL;

  // put whole thing in scrolled window
  LiVESWidget *scrolledwindow;

  LiVESList *hints = NULL;
  LiVESList *onchange = NULL;
  LiVESList *layout = NULL;
  LiVESList *list;

  char **array;
  char label_text[256]; // max length of a label in layout hints

  char *line;
  char *type = NULL;
  char *format = NULL;

  char fmt_strings[MAX_FMT_STRINGS][FMT_STRING_SIZE];

  size_t fmtlen, ll;

  boolean used[rfx->num_params];
  boolean has_box = FALSE;
  boolean internal = FALSE;
  boolean noslid;
  boolean has_param = FALSE;
  boolean chk_params = FALSE;
  boolean needs_sizes = FALSE;
  boolean layout_mode = FALSE;
  boolean keepsmall;

  int pnum;
  int length;
  int poffset = 0, inum = 0;
  int wofl = widget_opts.filler_len;

  int num_tok;

  int c_fmt_strings = 0;
  int pass;
  int woph = widget_opts.packing_height;

  register int i, j, k;

  char sepnpnum[1024];
  size_t sepnpnumlen;

  lives_snprintf(sepnpnum, 1024, "s%s", rfx->delim);
  sepnpnumlen = strlen(sepnpnum);

  if (!top_vbox) {
    // just check how many non-hidden params without displaying
    chk_params = TRUE;
  } else {
    dummy_label = lives_label_new(NULL);
    lives_widget_object_ref_sink(LIVES_WIDGET_OBJECT(dummy_label));

    mainw->textwidget_focus = NULL;

    // initialise special widgets
    init_special();

    if (rfx->status == RFX_STATUS_WEED) usrgrp_to_livesgrp[1] = NULL;
    else usrgrp_to_livesgrp[0] = NULL;

    // paramwindow start, everything goes in top_hbox
    top_hbox = lives_hbox_new(FALSE, widget_opts.packing_width);

    // param_vbox holds the dynamic parameters
    param_vbox = lives_vbox_new(FALSE, widget_opts.packing_height);
    lives_widget_set_halign(param_vbox, LIVES_ALIGN_FILL);
    lives_widget_set_valign(param_vbox, LIVES_ALIGN_CENTER);
    lives_box_pack_start(LIVES_BOX(top_hbox), param_vbox, TRUE, TRUE, widget_opts.packing_width);

    for (i = 0; i < rfx->num_params; i++) {
      used[i] = FALSE;
      for (j = 0; j < MAX_PARAM_WIDGETS; j++) {
        if (rfx->params[i].transition && j > 0 && j < 4) continue;
        rfx->params[i].widgets[j] = NULL;
      }
    }
  }

  switch (rfx->status) {
  case RFX_STATUS_BUILTIN:
    if (!chk_params) type = lives_strdup(PLUGIN_RENDERED_EFFECTS_BUILTIN);
    break;
  case RFX_STATUS_CUSTOM:
    if (!chk_params) type = lives_strdup(PLUGIN_RENDERED_EFFECTS_CUSTOM);
    break;
  case RFX_STATUS_SCRAP:
    if (!chk_params) type = lives_strdup(PLUGIN_RFX_SCRAP);
    break;
  case RFX_STATUS_WEED:
    if (!mainw->multitrack && rfx->is_template) {
      weed_plant_t *filter = weed_instance_get_filter((weed_plant_t *)rfx->source, TRUE);
      if (enabled_in_channels(filter, FALSE) == 0 && enabled_out_channels(filter, FALSE) > 0
          && has_video_chans_out(filter, TRUE)) {
        // out channel size(s) and target_fps for generators
        needs_sizes = TRUE;
      }
    }
    // extras for converters
    if (weed_instance_is_resizer((weed_plant_t *)rfx->source)) {
      has_param = add_sizes(LIVES_BOX(param_vbox), FALSE, FALSE, rfx);
      if (chk_params && has_param) return TRUE;
    }
    internal = TRUE;
    break;
  default:
    if (!chk_params) type = lives_strdup(PLUGIN_RENDERED_EFFECTS_TEST);
    break;
  }

  if (internal) {
    if (mainw->multitrack) {
      // extras for multitrack
      weed_plant_t *filter = weed_instance_get_filter((weed_plant_t *)rfx->source, TRUE);
      if (enabled_in_channels(filter, FALSE) == 2 && get_transition_param(filter, FALSE) != -1) {
        // add in/out for multitrack transition
        if (chk_params) return TRUE;
        has_param = TRUE;
        transition_add_in_out(LIVES_BOX(param_vbox), rfx, (mainw->multitrack->opts.pertrack_audio));
      }
    }
    if (!chk_params) hints = get_external_window_hints(rfx);
  } else {
    if (rfx->status != RFX_STATUS_SCRAP && rfx->num_in_channels == 0 && rfx->min_frames > -1) {
      if (!mainw->multitrack) {
        if (chk_params) return TRUE;
        add_gen_to(LIVES_BOX(param_vbox), rfx);
      } else mainw->gen_to_clipboard = FALSE;
      /// add nframes, fps, width, height
      add_genparams(param_vbox, rfx);
      has_param = TRUE;
    }

    if (!chk_params) {
      // do onchange|init
      if ((onchange = plugin_request_by_line(type, rfx->name, "get_onchange"))) {
        for (i = 0; i < lives_list_length(onchange); i++) {
          array = lives_strsplit((char *)lives_list_nth_data(onchange, i), rfx->delim, -1);
          if (strcmp(array[0], "init")) {
            // note other onchanges so we don't have to keep parsing the list
            int which = atoi(array[0]);
            if (which >= 0 && which < rfx->num_params) {
              rfx->params[which].onchange = TRUE;
            }
          }
          lives_strfreev(array);
        }
        lives_list_free_all(&onchange);
      }
      hints = plugin_request_by_line(type, rfx->name, "get_param_window");
      lives_free(type);
    }
  }

  // do param window hints
  if (hints) {
    LiVESList *list;
    char *lstring = lives_strconcat("layout", rfx->delim, NULL);
    char *sstring = lives_strconcat("special", rfx->delim, NULL);
    char *istring = lives_strconcat("internal", rfx->delim, NULL);
    for (list = hints; list; list = list->next) {
      char *line = (char *)list->data;
      if (!lives_strncmp(line, lstring, 7)) {
        layout = lives_list_append(layout, lives_strdup(line + 7));
      } else if (!lives_strncmp(line, istring, 9)) {
        layout = lives_list_append(layout, lives_strdup(line + 9));
      } else if (!lives_strncmp(line, sstring, 8)) {
        add_to_special(line + 8, rfx); // add any special actions to the framedraw preview
      }
    }
    lives_list_free_all(&hints);
    lives_free(lstring);
    lives_free(sstring);
    lives_free(istring);
  }

  lives_memset(fmt_strings, 0, MAX_FMT_STRINGS * FMT_STRING_SIZE);

  for (pass = 0; pass < 2; pass++) {
    // in this mode we do 2 passes: first check if the row is similar to the following row
    // (ignoring any rows with just labels or hseparators)
    // if so we mark it as 'layoutable'

    // to compare: make a string with the following vals: paramtype, or label (-2), or fill (-1)
    // following this we compare the strings

    // if the string has the same value as its successor we will create or extend the layout
    if (chk_params) pass = 1;
    //g_print("in pass %d\n", pass);

    list = layout;
    // use layout hints to build as much as we can
    for (i = 0; list; i++) {
      line = (char *)list->data;
      list = list->next;
      layout_mode = FALSE;
      has_box = FALSE;
      last_label = NULL;
      noslid = FALSE;
      if (i < MAX_FMT_STRINGS - 1) {
        format = fmt_strings[i];
        if (pass == 1 && !chk_params && (i > 0 || list)) {
          if (fmt_match((char *)fmt_strings[list == NULL ? i  - 1 : i])) {
            layout_mode = TRUE;
            if (!layoutx) {
              widget_opts.packing_height *= 2;
              layoutx = lives_layout_new(LIVES_BOX(param_vbox));
              lives_widget_set_halign(layoutx, LIVES_ALIGN_CENTER);
              widget_opts.packing_height = woph;
            }
            //g_print("LAYOUT MODE\n");
          }
        }
      } else if (pass == 0) break;

      num_tok = get_token_count(line, (unsigned int)rfx->delim[0]);
      // ignore | inside strings
      array = lives_strsplit(line, rfx->delim, num_tok);
      if (!*(array[num_tok - 1])) num_tok--;

      for (j = 0; j < num_tok; j++) {
        if (!strcmp(array[j], "nextfilter")) {
          // handling for compound fx - add an offset to the param number
          poffset += num_in_params_for_nth_instance((weed_plant_t *)rfx->source, inum);
          inum++;
          continue;
        }

        if (!strcmp(array[j], "hseparator")) {
          // hseparator ///////////////
          if (pass == 1 && !chk_params) {
            // add a separator
            if (layoutx) lives_layout_add_separator(LIVES_LAYOUT(layoutx), TRUE);
            else add_hsep_to_box(LIVES_BOX(param_vbox));
          }
          break; // ignore anything after hseparator
        }

        if (!strncmp(array[j], "p", 1) && (pnum = atoi((char *)(array[j] + 1))) >= 0
            && (pnum = pnum + poffset) < rfx->num_params && !used[pnum]) {
          // parameter, eg. p1 ////////////////////////////
          param = &rfx->params[pnum];
          if (!chk_params && !(rfx->flags & RFX_FLAGS_NO_RESET)) {
            rfx->params[pnum].changed = FALSE;
          }
          if (rfx->source_type == LIVES_RFX_SOURCE_WEED) check_hidden_gui((weed_plant_t *)rfx->source, param, pnum);
          if (param->type == LIVES_PARAM_UNDISPLAYABLE || param->type == LIVES_PARAM_UNKNOWN) continue;

          has_param = TRUE;

          if (pass == 0) {
            if ((fmtlen = lives_strlen((const char *)format)) < FMT_STRING_SIZE - 1) format[fmtlen] = (unsigned char)param->type;
          } else {
            used[pnum] = TRUE;
            if (!has_box) {
              // add a new row if needed
              if (layoutx) lives_layout_add_row(LIVES_LAYOUT(layoutx));
              else {
                hbox = lives_hbox_new(FALSE, 0);
                lives_box_pack_start(LIVES_BOX(param_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
              }
              has_box = TRUE;
            } else {
              widget_opts.filler_len >>= 1;
              if (layoutx) lives_layout_add_fill(LIVES_LAYOUT(layoutx), TRUE);
              else add_fill_to_box(LIVES_BOX(hbox));
              widget_opts.filler_len = wofl;
            }

            if (last_label) {
              lives_widget_set_halign(last_label, LIVES_ALIGN_START);
            }
            if (layoutx) hbox = lives_layout_hbox_new(LIVES_LAYOUT(layoutx));
            if (add_param_to_box(LIVES_BOX(hbox), rfx, pnum, (j == (num_tok - 1)) && !noslid)) noslid = TRUE;
          }
        } else if (!strncmp(array[j], "fill", 4)) {
          //// fill //////////////////
          // (can be filln)

          if (strlen(array[j]) == 4 || (length = atoi(array[j] + 4)) == 0) length = 1;

          if (pass == 1) {
            if (!has_box) {
              // add a new row if needed
              if (layoutx) lives_layout_add_row(LIVES_LAYOUT(layoutx));
              else {
                hbox = lives_hbox_new(FALSE, 0);
                lives_box_pack_start(LIVES_BOX(param_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
              }
              if (layoutx) lives_layout_add_fill(LIVES_LAYOUT(layoutx), TRUE);
              else add_fill_to_box(LIVES_BOX(hbox));
              has_box = TRUE;
            } else {
              if (last_label) lives_widget_set_halign(last_label, LIVES_ALIGN_START);
              widget_opts.filler_len >>= 1;
              if (layoutx) {
                lives_layout_add_fill(LIVES_LAYOUT(layoutx), TRUE);
                lives_layout_add_fill(LIVES_LAYOUT(layoutx), TRUE);
              } else {
                add_fill_to_box(LIVES_BOX(hbox));
                add_fill_to_box(LIVES_BOX(hbox));
              }
              widget_opts.filler_len = wofl;
            }
          }

          for (k = 1; k < length; k++) {
            if (pass == 1) {
              widget_opts.filler_len >>= 1;
              if (layoutx) {
                lives_layout_add_fill(LIVES_LAYOUT(layoutx), TRUE);
                lives_layout_add_fill(LIVES_LAYOUT(layoutx), TRUE);
              } else {
                add_fill_to_box(LIVES_BOX(hbox));
                add_fill_to_box(LIVES_BOX(hbox));
              }
              widget_opts.filler_len = wofl;
            } else if ((fmtlen = lives_strlen((const char *)format)) < FMT_STRING_SIZE) format[fmtlen] = -1;
          }
        } else if (*array[j] == '"') {
          // add a label
          if (pass == 0) {
            if ((fmtlen = lives_strlen((const char *)format)) < FMT_STRING_SIZE) format[fmtlen] = -2;
            if (has_box) last_label = dummy_label;
            continue;
          }

          if (!has_box) {
            // add a new row if needed
            if (layoutx) lives_layout_add_row(LIVES_LAYOUT(layoutx));
            else {
              hbox = lives_hbox_new(FALSE, 0);
              lives_box_pack_start(LIVES_BOX(param_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
            }
            has_box = TRUE;
          } else {
            widget_opts.filler_len >>= 1;
            if (layoutx) lives_layout_add_fill(LIVES_LAYOUT(layoutx), TRUE);
            else add_fill_to_box(LIVES_BOX(hbox));
            widget_opts.filler_len = wofl;
          }

          ll = lives_snprintf(label_text, 256, "%s", array[j] + 1);
          if (ll > 255) ll = 255;

          while (j < num_tok - 1 && label_text[ll - 1] != '"') {
            // handle separators within label text
            ll += lives_strappend(label_text, 256, rfx->delim);
            ll += lives_strappend(label_text, 256, array[++j]);
          }

          keepsmall = TRUE;
          if (!last_label && !has_param) {
            if (j == num_tok - 1 || strncmp(array[j + 1], sepnpnum, sepnpnumlen)) keepsmall = TRUE;
          }

          if (ll) {
            if (label_text[ll - 1] == '"') label_text[ll - 1] = 0;

            if (!keepsmall) widget_opts.justify = LIVES_JUSTIFY_CENTER;
            else if (last_label) {
              lives_widget_set_halign(last_label, LIVES_ALIGN_START);
              lives_widget_set_hexpand(last_label, FALSE);
            }

            if (layoutx) {
              last_label = lives_layout_add_label(LIVES_LAYOUT(layoutx), label_text, keepsmall);
            } else last_label = add_param_label_to_box(LIVES_BOX(hbox), !keepsmall, label_text);
            widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
            lives_widget_set_hexpand(last_label, TRUE);
          }
        }
      }
      if (!layout_mode) layoutx = NULL;
      lives_strfreev(array);
    }

    if (!chk_params) {
      c_fmt_strings = i;
      if (pass == 1) lives_list_free_all(&layout);
    }

    // add any unused parameters
    for (i = 0; i < rfx->num_params; i++) {
      if (!chk_params && !(rfx->flags & RFX_FLAGS_NO_RESET)) {
        rfx->params[i].changed = FALSE;
        if (used[i]) continue;
      }

      layout_mode = FALSE;
      format = NULL;

      if (c_fmt_strings + i < MAX_FMT_STRINGS - 1) {
        format = fmt_strings[c_fmt_strings + i];
        if (pass == 1 && !chk_params) {
          if (fmt_match((char *)fmt_strings[i + c_fmt_strings])) {
            layout_mode = TRUE;
            if (!layoutx) {
              widget_opts.packing_height *= 2;
              layoutx = lives_layout_new(LIVES_BOX(param_vbox));
              lives_widget_set_halign(layoutx, LIVES_ALIGN_CENTER);
              widget_opts.packing_height = woph;
            }
            //g_print("LAYOUT MODE\n");
          }
        }
      } else if (pass == 0) break;

      if (rfx->source_type == LIVES_RFX_SOURCE_WEED) check_hidden_gui((weed_plant_t *)rfx->source, &rfx->params[i], i);
      if (rfx->params[i].type == LIVES_PARAM_UNDISPLAYABLE || rfx->params[i].type == LIVES_PARAM_UNKNOWN) continue;

      if (chk_params) return TRUE;

      has_param = TRUE;
      if (pass == 0) {
        if ((fmtlen = lives_strlen((const char *)format)) < FMT_STRING_SIZE) format[fmtlen] =
            (unsigned char)(rfx->params[i].type);
      } else {
        if (layoutx) {
          add_param_to_box(LIVES_BOX(lives_layout_row_new(LIVES_LAYOUT(layoutx))), rfx, i, TRUE);
        } else add_param_to_box(LIVES_BOX(param_vbox), rfx, i, TRUE);
      }
      if (!layout_mode) layoutx = NULL;
    }
  }

  if (needs_sizes) has_param = add_sizes(chk_params ? NULL : LIVES_BOX(top_vbox), TRUE, has_param, rfx);
  if (chk_params) return has_param;

  if (!has_param) {
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    LiVESWidget *label = lives_standard_label_new(_("No parameters"));
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(param_vbox), hbox, TRUE, FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, FALSE, 0);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  }

  if (!mainw->multitrack || rfx->status != RFX_STATUS_WEED) {
    float box_scale = 1.;
    // for resize effects we add the framedraw to get its widgets, but hide it, so the box should get extra width and less height
    if (rfx->props & RFX_PROPS_MAY_RESIZE) box_scale = 1.5 * widget_opts.scale;
    scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H * box_scale, RFX_WINSIZE_V >> 1, top_hbox);
  } else scrolledwindow = lives_standard_scrolled_window_new(-1, -1, top_hbox);

  lives_box_pack_start(LIVES_BOX(top_vbox), scrolledwindow, TRUE, TRUE, 0);
  lives_widget_destroy(dummy_label);
  if (has_param)
    update_widget_vis(rfx, -1, -1);
  return has_param;
}


boolean add_param_to_box(LiVESBox *box, lives_rfx_t *rfx, int pnum, boolean add_slider) {
  // box here is vbox inside top_hbox inside top_dialog

  // add paramter pnum for rfx to box

  LiVESWidget *label;
  LiVESWidget *checkbutton;
  LiVESWidget *radiobutton;
  LiVESWidget *spinbutton;
  LiVESWidget *scale = NULL;
  LiVESWidget *spinbutton_red;
  LiVESWidget *spinbutton_green;
  LiVESWidget *spinbutton_blue;
  LiVESWidget *cbutton;
  LiVESWidget *entry = NULL;
  LiVESWidget *hbox;
  LiVESWidget *combo;
  //LiVESWidget *dlabel = NULL;
  LiVESWidget *textview = NULL;
  LiVESWidget *scrolledwindow;
  LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box),
                        WH_LAYOUT_KEY);

  LiVESAdjustment *spinbutton_adj;

  LiVESTextBuffer *textbuffer = NULL;

  lives_param_t *param;
  lives_widget_group_t *group;
  LiVESSList *rbgroup;

  lives_colRGB48_t rgb;
  lives_colRGBA64_t rgba;

  char *name;
  char *txt;//, *tmp;
  //char *disp_string;

  int wcount = 0;

  boolean use_mnemonic;
  boolean was_num = FALSE;

  boolean add_scalers = TRUE;

  if (pnum >= rfx->num_params) {
    add_param_label_to_box(box, FALSE, (_("Invalid parameter")));
    return FALSE;
  }

  param = &rfx->params[pnum];

  name = lives_strdup_printf("%s", param->label);
  use_mnemonic = param->use_mnemonic;

  // reinit can cause the window to be redrawn, which invalidates the slider adjustment...and bang !
  // so dont add sliders for such params
  if (param->reinit) add_scalers = FALSE;

  // for plugins (encoders and video playback) sliders look silly
  if (rfx->flags & RFX_FLAGS_NO_SLIDERS) add_scalers = FALSE;

  if (LIVES_IS_HBOX(LIVES_WIDGET(box))) {
    hbox = LIVES_WIDGET(box);
  } else {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, widget_opts.packing_height);
  }

  // see if there were any 'special' hints
  if (!layout)
    check_for_special_type(rfx, param, LIVES_BOX(lives_widget_get_parent(LIVES_WIDGET(box))));
  else
    check_for_special_type(rfx, param, LIVES_BOX(lives_widget_get_parent(layout)));

  switch (param->type) {
  case LIVES_PARAM_BOOL:
    if (!param->group) {
      widget_opts.mnemonic_label = use_mnemonic;
      checkbutton = lives_standard_check_button_new(name, get_bool_param(param->value), (LiVESBox *)hbox, param->desc);
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                      LIVES_GUI_CALLBACK(after_boolean_param_toggled),
                                      (livespointer)rfx);
      widget_opts.mnemonic_label = TRUE;

      // store parameter so we know whose trigger to use
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(checkbutton), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));
      param->widgets[0] = checkbutton;
    } else {
      group = get_group(rfx, param);

      if (group != NULL) rbgroup = group->rbgroup;
      else rbgroup = NULL;

      widget_opts.mnemonic_label = use_mnemonic;
      radiobutton = lives_standard_radio_button_new(name, &rbgroup, LIVES_BOX(hbox), param->desc);
      widget_opts.mnemonic_label = TRUE;

      if (group == NULL) {
        if (rfx->status == RFX_STATUS_WEED) {
          usrgrp_to_livesgrp[1] = add_usrgrp_to_livesgrp(usrgrp_to_livesgrp[1],
                                  rbgroup, param->group);
        } else {
          usrgrp_to_livesgrp[0] = add_usrgrp_to_livesgrp(usrgrp_to_livesgrp[0],
                                  rbgroup, param->group);
        }
      }

      group = get_group(rfx, param);

      if (group != NULL) {
        group->rbgroup = rbgroup;
        if (get_bool_param(param->value)) {
          group->active_param = pnum + 1;
        }
      } else LIVES_WARN("Button group was NULL");

      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), get_bool_param(param->value));

      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                      LIVES_GUI_CALLBACK(after_boolean_param_toggled), (livespointer)rfx);

      // store parameter so we know whose trigger to use
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(radiobutton), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));
      param->widgets[0] = radiobutton;
    }
    param->widgets[1] = widget_opts.last_label;
    break;

  case LIVES_PARAM_NUM:
    was_num = TRUE;

    widget_opts.mnemonic_label = use_mnemonic;
    if (param->dp) {
      spinbutton = lives_standard_spin_button_new(name, get_double_param(param->value), param->min,
                   param->max, param->step_size, param->step_size, param->dp,
                   (LiVESBox *)hbox, param->desc);
    } else {
      spinbutton = lives_standard_spin_button_new(name, (double)get_int_param(param->value), param->min,
                   param->max, param->step_size, param->step_size, param->dp,
                   (LiVESBox *)hbox, param->desc);
    }
    widget_opts.mnemonic_label = TRUE;

    lives_spin_button_set_wrap(LIVES_SPIN_BUTTON(spinbutton), param->wrap);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_param_value_changed), (livespointer)rfx);

    // store parameter so we know whose trigger to use
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));
    param->widgets[0] = spinbutton;
    param->widgets[++wcount] = widget_opts.last_label;
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(param->widgets[0]), RFX_KEY, rfx);

    if (add_scalers) {
      spinbutton_adj = lives_spin_button_get_adjustment(LIVES_SPIN_BUTTON(spinbutton));
#ifdef ENABLE_GIW
      if (prefs->lamp_buttons) {
        scale = giw_knob_new(LIVES_ADJUSTMENT(spinbutton_adj));
        giw_knob_set_wrap(GIW_KNOB(scale), param->wrap);
        lives_widget_set_size_request(scale, GIW_KNOB_WIDTH, GIW_KNOB_HEIGHT);
        giw_knob_set_legends_digits(GIW_KNOB(scale), 0);
        if (layout) {
          hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
          lives_layout_pack(LIVES_HBOX(hbox), scale);
        } else
          lives_box_pack_start(LIVES_BOX(hbox), scale, FALSE, FALSE, widget_opts.packing_width >> 1);
        if (param->desc) lives_widget_set_tooltip_text(scale, param->desc);
        lives_widget_set_fg_color(scale, LIVES_WIDGET_STATE_NORMAL, &palette->white);
        lives_widget_set_fg_color(scale, LIVES_WIDGET_STATE_PRELIGHT, &palette->dark_orange);
        lives_widget_set_bg_color(scale, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
        param->widgets[++wcount] = scale;
      }
#endif

      if (add_slider && !param->wrap) {
        spinbutton_adj = lives_spin_button_get_adjustment(LIVES_SPIN_BUTTON(spinbutton));
        scale = lives_standard_hscale_new(LIVES_ADJUSTMENT(spinbutton_adj));
        lives_widget_set_size_request(scale, DEF_SLIDER_WIDTH, -1);
        if (layout != NULL) {
          hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
          lives_layout_pack(LIVES_HBOX(hbox), scale);
        } else {
          lives_box_pack_start(LIVES_BOX(hbox), scale, TRUE, TRUE, widget_opts.packing_width >> 1);
          if (!LIVES_IS_HBOX(LIVES_WIDGET(box))) add_fill_to_box(LIVES_BOX(hbox));
        }
        lives_widget_apply_theme(scale, LIVES_WIDGET_STATE_NORMAL);
        if (param->desc != NULL) lives_widget_set_tooltip_text(scale, param->desc);
        param->widgets[++wcount] = scale;
      }
    }

    if (param->desc != NULL) lives_widget_set_tooltip_text(scale, param->desc);
    break;

  case LIVES_PARAM_COLRGB24:
    get_colRGB24_param(param->value, &rgb);

    rgba.red = rgb.red << 8;
    rgba.green = rgb.green << 8;
    rgba.blue = rgb.blue << 8;
    rgba.alpha = 65535;

    widget_opts.mnemonic_label = use_mnemonic;
    cbutton = lives_standard_color_button_new(LIVES_BOX(hbox), _(name), FALSE, &rgba, &spinbutton_red, &spinbutton_green,
              &spinbutton_blue, NULL);
    widget_opts.mnemonic_label = TRUE;
    lives_widget_set_size_request(cbutton, DEF_BUTTON_WIDTH / 2, -1);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));
    if (param->desc != NULL) lives_widget_set_tooltip_text(cbutton, param->desc);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_param_red_changed), (livespointer)rfx);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_param_green_changed), (livespointer)rfx);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_param_blue_changed), (livespointer)rfx);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(cbutton), LIVES_WIDGET_COLOR_SET_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_pwcolsel), (livespointer)rfx);

    // store parameter so we know whose trigger to use
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_red), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_green), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_blue), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));

    param->widgets[0] = spinbutton_red;
    param->widgets[1] = spinbutton_green;
    param->widgets[2] = spinbutton_blue;
    //param->widgets[3]=spinbutton_alpha;
    param->widgets[4] = cbutton;
    param->widgets[5] = widget_opts.last_label;
    break;

  case LIVES_PARAM_STRING:
    if (param->max == 0.) txt = lives_strdup((char *)param->value);
    else txt = lives_strndup((char *)param->value, (int)param->max);

    if (((int)param->max > RFX_TEXT_MAGIC || param->max == 0.) &&
        param->special_type != LIVES_PARAM_SPECIAL_TYPE_FILEREAD
        && param->special_type != LIVES_PARAM_SPECIAL_TYPE_FONT_CHOOSER
        && param->special_type != LIVES_PARAM_SPECIAL_TYPE_FILEWRITE) {
      LiVESWidget *vbox = lives_vbox_new(FALSE, 0);
      int woat = widget_opts.apply_theme;

      widget_opts.justify = LIVES_JUSTIFY_CENTER;
      if (use_mnemonic) label = lives_standard_label_new_with_mnemonic_widget(_(name), NULL);
      else label = lives_standard_label_new(_(name));
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

      lives_box_pack_start(LIVES_BOX(hbox), vbox, TRUE, TRUE, widget_opts.packing_width);
      lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height >> 1);

      hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height >> 1);

      param->widgets[0] = textview = lives_text_view_new();
      if (param->desc != NULL) lives_widget_set_tooltip_text(textview, param->desc);
      textbuffer = lives_text_view_get_buffer(LIVES_TEXT_VIEW(textview));

      lives_signal_sync_connect_after(LIVES_WIDGET_OBJECT(textbuffer), LIVES_WIDGET_CHANGED_SIGNAL,
                                      LIVES_GUI_CALLBACK(after_param_text_buffer_changed),
                                      (livespointer) rfx);

      lives_text_view_set_editable(LIVES_TEXT_VIEW(textview), TRUE);
      lives_text_view_set_wrap_mode(LIVES_TEXT_VIEW(textview), LIVES_WRAP_WORD);
      lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(textview), TRUE);

      lives_text_buffer_set_text(textbuffer, txt, -1);

      widget_opts.apply_theme = 0;
      widget_opts.expand = LIVES_EXPAND_EXTRA;
      scrolledwindow = lives_standard_scrolled_window_new(-1, RFX_TEXT_SCROLL_HEIGHT, textview);
      widget_opts.expand = LIVES_EXPAND_DEFAULT;
      widget_opts.apply_theme = woat;

      if (mainw->multitrack == NULL)
        lives_widget_apply_theme3(textview, LIVES_WIDGET_STATE_NORMAL);
      else
        lives_widget_apply_theme2(textview, LIVES_WIDGET_STATE_NORMAL, TRUE);

      lives_box_pack_start(LIVES_BOX(hbox), scrolledwindow, TRUE, TRUE, 0);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(textbuffer), "textview", textview);
    } else {
      if (use_mnemonic) label = lives_standard_label_new_with_mnemonic_widget(_(name), NULL);
      else label = lives_standard_label_new(_(name));

      lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
      param->widgets[0] = entry = lives_standard_entry_new(NULL, txt, (int)param->max,
                                  (int)param->max, LIVES_BOX(hbox), param->desc);

      if (rfx->status == RFX_STATUS_WEED && param->special_type != LIVES_PARAM_SPECIAL_TYPE_FILEREAD) {
        lives_signal_sync_connect_after(LIVES_WIDGET_OBJECT(entry), LIVES_WIDGET_CHANGED_SIGNAL,
                                        LIVES_GUI_CALLBACK(after_param_text_changed), (livespointer)rfx);
      }
    }
    param->widgets[1] = widget_opts.last_label;

    if (param->desc != NULL) lives_widget_set_tooltip_text(label, param->desc);

    lives_signal_sync_connect_after(LIVES_WIDGET_OBJECT(hbox), LIVES_WIDGET_SET_FOCUS_CHILD_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_param_text_focus_changed),
                                    (livespointer)rfx);

    if (use_mnemonic) lives_label_set_mnemonic_widget(LIVES_LABEL(label), param->widgets[0]);

    lives_free(txt);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), TEXTWIDGET_KEY, (livespointer)param->widgets[0]);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(param->widgets[0]), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(param->widgets[0]), RFX_KEY, rfx);

    param->widgets[1] = label;

    break;

  case LIVES_PARAM_STRING_LIST:
    widget_opts.expand = LIVES_EXPAND_EXTRA;
    widget_opts.mnemonic_label = use_mnemonic;

    combo = lives_standard_combo_new(name, param->list, (LiVESBox *)hbox, param->desc);
    widget_opts.mnemonic_label = TRUE;
    widget_opts.expand = LIVES_EXPAND_DEFAULT;

    if (param->list != NULL) {
      lives_combo_set_active_string(LIVES_COMBO(combo),
                                    (char *)lives_list_nth_data(param->list, get_int_param(param->value)));
    }

    lives_signal_sync_connect_after(LIVES_WIDGET_OBJECT(combo), LIVES_WIDGET_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(after_string_list_changed), (livespointer)rfx);

    // store parameter so we know whose trigger to use
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo), PARAM_NUMBER_KEY, LIVES_INT_TO_POINTER(pnum));
    param->widgets[0] = combo;
    param->widgets[1] = widget_opts.last_label;
    break;

  default:
    break;
  }

  // see if there were any 'special' hints
  if (!layout) {
    check_for_special(rfx, param, LIVES_BOX(lives_widget_get_parent(LIVES_WIDGET(box))));
  } else {
    check_for_special(rfx, param, LIVES_BOX(lives_widget_get_parent(layout)));
  }
  lives_free(name);
  return was_num;
}


LiVESWidget *add_param_label_to_box(LiVESBox *box, boolean do_trans, const char *text) {
  LiVESWidget *label;

  lives_box_set_homogeneous(LIVES_BOX(box), FALSE);

  if (do_trans) {
    char *markup;
#ifdef GUI_GTK
    markup = g_markup_printf_escaped("<span weight=\"bold\" style=\"italic\"> %s </span>", _(text));
#endif
#ifdef GUI_QT
    QString qs = QString("<span weight=\"bold\" style=\"italic\"> %s </span>").arg(_(text));
    markup = strdup((const char *)qs.toHtmlEscaped().constData());
#endif
    label = lives_standard_label_new(NULL);
    lives_label_set_markup(LIVES_LABEL(label), markup);
    lives_free(markup);
  } else label = lives_standard_label_new_with_mnemonic_widget(text, NULL);

  if (LIVES_IS_HBOX(LIVES_WIDGET(box)))
    lives_box_pack_start(box, label, FALSE, FALSE, widget_opts.packing_width);
  else
    lives_box_pack_start(box, label, FALSE, FALSE, widget_opts.packing_height);

  return label;
}


LiVESSList *add_usrgrp_to_livesgrp(LiVESSList *u2l, LiVESSList *rbgroup, int usr_number) {
  lives_widget_group_t *wgroup = (lives_widget_group_t *)lives_malloc(sizeof(lives_widget_group_t));
  wgroup->usr_number = usr_number;
  wgroup->rbgroup = rbgroup;
  wgroup->active_param = 0;
  u2l = lives_slist_append(u2l, (livespointer)wgroup);
  return u2l;
}


lives_widget_group_t *livesgrp_from_usrgrp(LiVESSList *u2l, int usrgrp) {
  lives_widget_group_t *group;
  LiVESSList *list = u2l;
  for (; list != NULL; list = list->next) {
    group = (lives_widget_group_t *)list->data;
    if (group->usr_number == usrgrp) return group;
  }
  return NULL;
}


boolean update_widget_vis(lives_rfx_t *rfx, int key, int mode) {
  weed_plant_t *wparam = NULL, *inst;
  int keyw, modew;
  lives_param_t *param;

  if (mainw->multitrack == NULL) {
    if (fx_dialog[1] != NULL) {
      rfx = fx_dialog[1]->rfx;
      if (!rfx->is_template) {
        keyw = fx_dialog[1]->key;
        modew = fx_dialog[1]->mode;
      }
      if (!rfx->is_template && (key != keyw && mode != modew)) return FALSE;
    }
  }

  if ((fx_dialog[1] == NULL && mainw->multitrack == NULL) || rfx == NULL || rfx->status != RFX_STATUS_WEED) return FALSE;
  inst = (weed_plant_t *)rfx->source;
  for (int i = 0; i < rfx->num_params; i++) {
    param = &rfx->params[i];
    if ((wparam = weed_inst_in_param(inst, i, FALSE, FALSE)) != NULL) {
      check_hidden_gui(inst, param, i);
      for (int j = 0; j < RFX_MAX_NORM_WIDGETS; j++) {
        if (param->type == LIVES_PARAM_COLRGB24 && j == 3 && param->widgets[j] == NULL) continue;
        if (param->widgets[j] == NULL) break;
        if (param->hidden) {
          lives_widget_hide(param->widgets[j]);
          lives_widget_set_no_show_all(param->widgets[j], TRUE);
        } else {
          lives_widget_set_no_show_all(param->widgets[j], FALSE);
          lives_widget_show_all(param->widgets[j]);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  return TRUE;
}


static void after_any_changed_1(lives_rfx_t *rfx, int param_number, int index) {
  weed_plant_t *inst = (weed_plant_t *)rfx->source;
  weed_plant_t *wparam = weed_inst_in_param(inst, param_number, FALSE, FALSE), *paramtmpl;
  int numvals = weed_leaf_num_elements(wparam, WEED_LEAF_VALUE);
  int *ign, nvals;
  //// update pt. 1:
  /// fill param vals and  set "ignore" values
  if (index >= numvals) {
    paramtmpl = weed_param_get_template(wparam);
    fill_param_vals_to(wparam, paramtmpl, index);
    numvals = index + 1;
  }

  if (mainw->multitrack != NULL && is_perchannel_multi(rfx, param_number)) {
    if (weed_plant_has_leaf(wparam, WEED_LEAF_IGNORE)) {
      ign = weed_get_boolean_array(wparam, WEED_LEAF_IGNORE, &nvals);
      if (index >= 0 && index < nvals) {
        ign[index] = WEED_FALSE;
        weed_set_boolean_array(wparam, WEED_LEAF_IGNORE, nvals, ign);
      }
      lives_freep((void **)&ign);
    }
  }
}


/**
  @brief part 2 function for updating params visually
*/
static void after_any_changed_2(lives_rfx_t *rfx, lives_param_t *param, boolean needs_update) {
  weed_plant_t *wparam = NULL, *gui, *inst = NULL;
  lives_filter_error_t retval = FILTER_SUCCESS;

  /// update widgets on screen (as a result of copying values or triggers)
  if (needs_update) update_visual_params(rfx, FALSE);
  needs_update = FALSE;

  /// only the first param in the chain can reinit
  if (--ireinit > 0) {
    param->changed = TRUE;
    param->change_blocked = FALSE;
    return;
  }

  /// plugin is allowed to show / hide params during its init_func()
  /// so here we record the state b4 calling it; if any changes occur we want to refresh the whole window.
  if (rfx->status == RFX_STATUS_WEED) {
    if (mainw->multitrack != NULL) {
      for (int i = 0; i < rfx->num_params; i++) {
        if ((wparam = weed_inst_in_param(inst, i, FALSE, FALSE)) != NULL) {
          if ((gui = weed_param_get_gui(wparam, FALSE)) != NULL) {
            if (retval != FILTER_INFO_REDRAWN) {
              if (weed_get_boolean_value(gui, "host_hidden_backup", NULL) != weed_get_boolean_value(gui, WEED_LEAF_HIDDEN, NULL))
                needs_update = TRUE;
            }
            weed_leaf_delete(gui, "host_hidden_backup");
	  // *INDENT-OFF*
          }}}}
    // *INDENT-ON*

    inst = (weed_plant_t *)rfx->source;
    if (rfx->needs_reinit) {
      if (!(rfx->needs_reinit & REINIT_FUNCTIONAL)) {
        weed_instance_set_flags(inst, weed_instance_get_flags(inst) | WEED_INSTANCE_UPDATE_GUI_ONLY);
      }

      retval = weed_reinit_effect(inst, FALSE);

      if (!(rfx->needs_reinit & REINIT_FUNCTIONAL)) {
        weed_instance_set_flags(inst, weed_instance_get_flags(inst) ^ WEED_INSTANCE_UPDATE_GUI_ONLY);
      }
      rfx->needs_reinit = 0;
    }
  }

  needs_update = FALSE;
  rfx->needs_reinit = 0;

  if (fx_dialog[1] != NULL) {
    // transfer param changes from rte_window to ce_thumbs window, and vice-versa
    lives_rfx_t *rte_rfx = fx_dialog[1]->rfx;
    int key = fx_dialog[1]->key;
    int mode = fx_dialog[1]->mode;
    mainw->block_param_updates = TRUE;
    if (rfx == rte_rfx && mainw->ce_thumbs) ce_thumbs_update_visual_params(key);
    else if (mode == rte_key_getmode(key + 1)) ce_thumbs_check_for_rte(rfx, rte_rfx, key);
    mainw->block_param_updates = FALSE;
  }

  if (!weed_param_value_irrelevant(wparam)) {
    param->changed = TRUE;
  }

  if (mainw->multitrack != NULL && rfx->status == RFX_STATUS_WEED) {
    update_widget_vis(rfx, -1, -1);
    activate_mt_preview(mainw->multitrack);
  }

  param->change_blocked = FALSE;
}


void after_boolean_param_toggled(LiVESToggleButton * togglebutton, lives_rfx_t *rfx) {
  int param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(togglebutton), PARAM_NUMBER_KEY));
  LiVESList *retvals = NULL;
  weed_plant_t *inst = NULL;
  lives_param_t *param = &rfx->params[param_number];
  boolean old_bool = get_bool_param(param->value), new_bool;
  boolean needs_update = FALSE;
  int copyto = -1;

  new_bool = lives_toggle_button_get_active(togglebutton);
  if (old_bool == new_bool) return;

  if (mainw->block_param_updates) {
    if (rfx->status == RFX_STATUS_WEED && param->reinit) rfx->needs_reinit |= param->reinit;
    return; // updates are blocked until all params are ready
  }

  ireinit++;

  set_bool_param(param->value, new_bool);
  if (mainw->framedraw_preview != NULL) reset_framedraw_preview();
  param->change_blocked = TRUE;

  if (rfx->status == RFX_STATUS_WEED) {
    inst = (weed_plant_t *)rfx->source;
    if (inst != NULL && WEED_PLANT_IS_FILTER_INSTANCE(inst)) {
      //char *disp_string;
      int index = 0, numvals;
      int key = -1;
      weed_plant_t *wparam = weed_inst_in_param(inst, param_number, FALSE, FALSE);
      int *valis = weed_get_boolean_array(wparam, WEED_LEAF_VALUE, NULL);

      if (mainw->multitrack != NULL && is_perchannel_multi(rfx, param_number)) {
        index = mainw->multitrack->track_index;
      }

      after_any_changed_1(rfx, param_number, index);

      valis[index] = new_bool;
      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      numvals = weed_leaf_num_elements(wparam, WEED_LEAF_VALUE);
      if (!filter_mutex_trylock(key)) {
        weed_set_boolean_array(wparam, WEED_LEAF_VALUE, numvals, valis);
        copyto = set_copy_to(inst, param_number, rfx, TRUE);
        filter_mutex_unlock(key); \
        if (copyto != -1) needs_update = TRUE;
      }
      lives_freep((void **)&valis);

      if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst, param_number);
      }
      if (param->reinit) rfx->needs_reinit |= param->reinit;
    }
  }

  if (get_bool_param(param->value) != old_bool && param->onchange) {
    param->change_blocked = TRUE;
    retvals = do_onchange(LIVES_WIDGET_OBJECT(togglebutton), rfx);
    lives_list_free_all(&retvals);
    needs_update = TRUE;
  }
  after_any_changed_2(rfx, param, needs_update);
}


void after_param_value_changed(LiVESSpinButton * spinbutton, lives_rfx_t *rfx) {
  int param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), PARAM_NUMBER_KEY));
  LiVESList *retvals = NULL;
  lives_param_t *param = &rfx->params[param_number];
  double new_double = 0., old_double = 0.;
  int new_int = 0, old_int = 0;
  boolean needs_update = FALSE;
  int copyto = -1;

  lives_spin_button_update(LIVES_SPIN_BUTTON(spinbutton));

  if (param->dp > 0) {
    old_double = get_double_param(param->value);
    new_double = lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton));
    if (old_double == new_double) return;
  } else {
    old_int = get_int_param(param->value);
    new_int = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    if (old_int == new_int) return;
  }

  if (mainw->block_param_updates) {
    if (rfx->status == RFX_STATUS_WEED && param->reinit) rfx->needs_reinit |= param->reinit;
    return; // updates are blocked until all params are ready
  }

  ireinit++;

  if (mainw->framedraw_preview) reset_framedraw_preview();

  if (rfx->status == RFX_STATUS_WEED && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING &&
      (prefs->rec_opts & REC_EFFECTS)) {
    // if we are recording, add this (pre)change to our event_list
    rec_param_change((weed_plant_t *)rfx->source, param_number);
    copyto = set_copy_to((weed_plant_t *)rfx->source, param_number, rfx, FALSE);
  }

  if (param->dp > 0) {
    set_double_param(param->value, new_double);
  } else {
    set_int_param(param->value, new_int);
  }

  param->change_blocked = TRUE;

  if (rfx->status == RFX_STATUS_WEED) {
    weed_plant_t *inst = (weed_plant_t *)rfx->source;
    if (inst != NULL && WEED_PLANT_IS_FILTER_INSTANCE(inst)) {
      weed_plant_t *wparam = weed_inst_in_param(inst, param_number, FALSE, FALSE);
      int index = 0, numvals;
      int key = -1;
      double *valds;
      int *valis;

      // update transition in/out radios
      if (mainw->multitrack != NULL) {
        weed_plant_t *filter = weed_instance_get_filter(inst, TRUE);
        if (enabled_in_channels(filter, FALSE) == 2 && param->transition) {
          if (param->dp == 0) {
            if (new_int == (int)param->min)
              lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(param->widgets[WIDGET_RB_IN]), TRUE);
            else if (new_int == (int)param->max)
              lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(param->widgets[WIDGET_RB_OUT]), TRUE);
            else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(param->widgets[WIDGET_RB_DUMMY]), TRUE);
          } else {
            if (new_double == param->min)
              lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(param->widgets[WIDGET_RB_IN]), TRUE);
            else if (new_double == param->max)
              lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(param->widgets[WIDGET_RB_OUT]), TRUE);
            else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(param->widgets[WIDGET_RB_DUMMY]), TRUE);
          }
        }
      }

      if (mainw->multitrack != NULL && is_perchannel_multi(rfx, param_number)) {
        index = mainw->multitrack->track_index;
      }

      after_any_changed_1(rfx, param_number, index);

      numvals = weed_leaf_num_elements(wparam, WEED_LEAF_VALUE);
      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      if (weed_leaf_seed_type(wparam, WEED_LEAF_VALUE) == WEED_SEED_DOUBLE) {
        valds = weed_get_double_array(wparam, WEED_LEAF_VALUE, NULL);
        if (param->dp > 0) valds[index] = new_double;
        else valds[index] = (double)new_int;
        if (!filter_mutex_trylock(key)) {
          weed_set_double_array(wparam, WEED_LEAF_VALUE, numvals, valds);
          copyto = set_copy_to(inst, param_number, rfx, TRUE);
          filter_mutex_unlock(key);
          if (copyto != -1) needs_update = TRUE;
        }
        lives_freep((void **)&valds);
      } else {
        valis = weed_get_int_array(wparam, WEED_LEAF_VALUE, NULL);
        valis[index] = new_int;
        weed_set_int_array(wparam, WEED_LEAF_VALUE, numvals, valis);
        copyto = set_copy_to(inst, param_number, rfx, TRUE);
        filter_mutex_unlock(key);
        if (copyto != -1) needs_update = TRUE;
        lives_freep((void **)&valis);
      }
    }

    if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
      // if we are recording, add this change to our event_list
      rec_param_change(inst, param_number);
    }
    if (param->reinit) rfx->needs_reinit |= param->reinit;
  }

  if (((param->dp > 0 && (get_double_param(param->value) != old_double)) || (param->dp == 0 &&
       (get_int_param(param->value) != old_int))) && param->onchange) {
    param->change_blocked = TRUE;
    retvals = do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    lives_list_free_all(&retvals);
    needs_update = TRUE;
  }

  after_any_changed_2(rfx, param, needs_update);
}


void update_weed_color_value(weed_plant_t *plant, int pnum, int c1, int c2, int c3, int c4, lives_rfx_t *rfx) {
  weed_plant_t *ptmpl;
  weed_plant_t *param = NULL;

  int *maxs = NULL, *mins = NULL;
  int cols[4] = {c1, c2, c3, c4};
  int cspace;
  int rmax, rmin, gmax, gmin, bmax, bmin;
  int error;

  boolean is_default = WEED_PLANT_IS_FILTER_CLASS(plant);
  boolean is_int;

  double *maxds = NULL, *minds = NULL;
  double colds[4];
  double rmaxd, rmind, gmaxd, gmind, bmaxd, bmind;

  if (!is_default) {
    param = weed_inst_in_param(plant, pnum, FALSE, FALSE);
    ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);
  } else {
    // called only from rte_set_defs_ok
    ptmpl = weed_filter_in_paramtmpl(plant, pnum, FALSE);
  }

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  is_int = (weed_leaf_seed_type(ptmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT);
  cspace = weed_get_int_value(ptmpl, WEED_LEAF_COLORSPACE, &error);

  switch (cspace) {
  // TODO - other cspaces
  case WEED_COLORSPACE_RGB:
    if (is_int) {
      if (weed_leaf_num_elements(ptmpl, WEED_LEAF_MAX) == 3) {
        maxs = weed_get_int_array(ptmpl, WEED_LEAF_MAX, &error);
        rmax = maxs[0];
        gmax = maxs[1];
        bmax = maxs[2];
        lives_free(maxs);
      } else rmax = gmax = bmax = weed_get_int_value(ptmpl, WEED_LEAF_MAX, &error);
      if (weed_leaf_num_elements(ptmpl, WEED_LEAF_MIN) == 3) {
        mins = weed_get_int_array(ptmpl, WEED_LEAF_MIN, &error);
        rmin = mins[0];
        gmin = mins[1];
        bmin = mins[2];
        lives_free(mins);
      } else rmin = gmin = bmin = weed_get_int_value(ptmpl, WEED_LEAF_MIN, &error);

      cols[0] = rmin + (int)((double)cols[0] / 255.*(double)(rmax - rmin));
      cols[1] = gmin + (int)((double)cols[1] / 255.*(double)(gmax - gmin));
      cols[2] = bmin + (int)((double)cols[2] / 255.*(double)(bmax - bmin));
      if (is_default) {
        weed_set_int_array(ptmpl, WEED_LEAF_HOST_DEFAULT, 3, cols);
      } else {
        int index = 0, numvals;
        int *valis;

        if (mainw->multitrack != NULL && is_perchannel_multiw(ptmpl)) {
          index = mainw->multitrack->track_index;
        }
        numvals = weed_leaf_num_elements(param, WEED_LEAF_VALUE);
        if (index * 3 >= numvals) {
          weed_plant_t *paramtmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);
          fill_param_vals_to(param, paramtmpl, index);
          numvals = (index + 1) * 3;
        }

        if (mainw->multitrack != NULL && is_perchannel_multi(rfx, pnum)) {
          if (weed_plant_has_leaf(param, WEED_LEAF_IGNORE)) {
            int nvals = weed_leaf_num_elements(param, WEED_LEAF_IGNORE);
            if (index >= 0 && index < nvals) {
              int *ign = weed_get_boolean_array(param, WEED_LEAF_IGNORE, &error);
              ign[index] = WEED_FALSE;
              weed_set_boolean_array(param, WEED_LEAF_IGNORE, nvals, ign);
              lives_free(ign);
            }
          }
        }

        valis = weed_get_int_array(param, WEED_LEAF_VALUE, &error);
        valis[index * 3] = cols[0];
        valis[index * 3 + 1] = cols[1];
        valis[index * 3 + 2] = cols[2];
        weed_set_int_array(param, WEED_LEAF_VALUE, numvals, valis);
        lives_free(valis);
      }
      break;
    } else {
      // double
      if (weed_leaf_num_elements(ptmpl, WEED_LEAF_MAX) == 3) {
        maxds = weed_get_double_array(ptmpl, WEED_LEAF_MAX, &error);
        rmaxd = maxds[0];
        gmaxd = maxds[1];
        bmaxd = maxds[2];
        lives_free(maxds);
      } else rmaxd = gmaxd = bmaxd = weed_get_double_value(ptmpl, WEED_LEAF_MAX, &error);
      if (weed_leaf_num_elements(ptmpl, WEED_LEAF_MIN) == 3) {
        minds = weed_get_double_array(ptmpl, WEED_LEAF_MIN, &error);
        rmind = minds[0];
        gmind = minds[1];
        bmind = minds[2];
        lives_free(minds);
      } else rmind = gmind = bmind = weed_get_double_value(ptmpl, WEED_LEAF_MIN, &error);
      colds[0] = rmind + (double)cols[0] / 255.*(rmaxd - rmind);
      colds[1] = gmind + (double)cols[1] / 255.*(gmaxd - gmind);
      colds[2] = bmind + (double)cols[2] / 255.*(bmaxd - bmind);
      if (is_default) {
        weed_set_double_array(ptmpl, WEED_LEAF_HOST_DEFAULT, 3, colds);
      } else {
        int index = 0, numvals;
        double *valds;

        if (mainw->multitrack != NULL && is_perchannel_multiw(ptmpl)) {
          index = mainw->multitrack->track_index;
        }
        numvals = weed_leaf_num_elements(param, WEED_LEAF_VALUE);
        if (index * 3 >= numvals) {
          weed_plant_t *paramtmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);
          fill_param_vals_to(param, paramtmpl, index);
          numvals = (index + 1) * 3;
        }

        if (mainw->multitrack != NULL && is_perchannel_multi(rfx, pnum)) {
          if (weed_plant_has_leaf(param, WEED_LEAF_IGNORE)) {
            int nvals = weed_leaf_num_elements(param, WEED_LEAF_IGNORE);
            if (index >= 0 && index < nvals) {
              int *ign = weed_get_boolean_array(param, WEED_LEAF_IGNORE, &error);
              ign[index] = WEED_FALSE;
              weed_set_boolean_array(param, WEED_LEAF_IGNORE, nvals, ign);
              lives_free(ign);
            }
          }
        }

        valds = weed_get_double_array(param, WEED_LEAF_VALUE, &error);
        valds[index * 3] = colds[0];
        valds[index * 3 + 1] = colds[1];
        valds[index * 3 + 2] = colds[2];
        weed_set_double_array(param, WEED_LEAF_VALUE, numvals, valds);
        lives_free(valds);
      }
    }
    break;
  }
}


void after_param_red_changed(LiVESSpinButton * spinbutton, lives_rfx_t *rfx) {
  LiVESList *retvals = NULL;
  lives_colRGB48_t old_value;
  int param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), PARAM_NUMBER_KEY));
  int new_red;
  boolean needs_update = FALSE;
  int copyto = -1;
  lives_param_t *param = &rfx->params[param_number];

  get_colRGB24_param(param->value, &old_value);
  new_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  if (old_value.red == new_red) return;

  if (mainw->block_param_updates) {
    if (rfx->status == RFX_STATUS_WEED && param->reinit) rfx->needs_reinit |= param->reinit;
    return; // updates are blocked until all params are ready
  }

  ireinit++;

  if (rfx->status == RFX_STATUS_WEED && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING &&
      (prefs->rec_opts & REC_EFFECTS)) {
    // if we are recording, add this change to our event_list

    rec_param_change((weed_plant_t *)rfx->source, param_number);
    copyto = set_copy_to((weed_plant_t *)rfx->source, param_number, rfx, FALSE);
  }

  set_colRGB24_param(param->value, new_red, old_value.green, old_value.blue);

  if (mainw->framedraw_preview != NULL) reset_framedraw_preview();
  param->change_blocked = TRUE;

  if (rfx->status == RFX_STATUS_WEED) {
    int key = -1;
    weed_plant_t *inst = (weed_plant_t *)rfx->source;
    if (inst != NULL && WEED_PLANT_IS_FILTER_INSTANCE(inst)) {
      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      if (!filter_mutex_trylock(key)) {
        update_weed_color_value(inst, param_number, new_red, old_value.green, old_value.blue, 0, rfx);
        copyto = set_copy_to(inst, param_number, rfx, TRUE);
        filter_mutex_unlock(key);
        if (copyto != -1) needs_update = TRUE;
      }

      if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst, param_number);
      }
      if (param->reinit) rfx->needs_reinit |= param->reinit;
    }
  }

  if (new_red != old_value.red && param->onchange) {
    param->change_blocked = TRUE;
    retvals = do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    lives_list_free_all(&retvals);
    needs_update = TRUE;
  }
  after_any_changed_2(rfx, param, needs_update);
}


void after_param_green_changed(LiVESSpinButton * spinbutton, lives_rfx_t *rfx) {
  LiVESList *retvals = NULL;
  lives_colRGB48_t old_value;
  int new_green;
  int copyto = -1;
  boolean needs_update = FALSE;
  int param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), PARAM_NUMBER_KEY));
  lives_param_t *param = &rfx->params[param_number];

  get_colRGB24_param(param->value, &old_value);
  new_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  if (old_value.green == new_green) return;

  if (mainw->block_param_updates) {
    if (rfx->status == RFX_STATUS_WEED && param->reinit) rfx->needs_reinit |= param->reinit;
    return; // updates are blocked until all params are ready
  }

  ireinit++;

  if (rfx->status == RFX_STATUS_WEED && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING &&
      (prefs->rec_opts & REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source, param_number);
    copyto = set_copy_to((weed_plant_t *)rfx->source, param_number, rfx, FALSE);
  }

  set_colRGB24_param(param->value, old_value.red, new_green, old_value.blue);

  if (mainw->framedraw_preview != NULL) reset_framedraw_preview();
  param->change_blocked = TRUE;

  if (rfx->status == RFX_STATUS_WEED) {
    int key = -1;
    weed_plant_t *inst = (weed_plant_t *)rfx->source;
    if (inst != NULL && WEED_PLANT_IS_FILTER_INSTANCE(inst)) {
      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      if (!filter_mutex_trylock(key)) {
        update_weed_color_value(inst, param_number, old_value.red, new_green, old_value.blue, 0, rfx);
        copyto = set_copy_to(inst, param_number, rfx, TRUE);
        filter_mutex_unlock(key);
        if (copyto != -1) needs_update = TRUE;
      }

      if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst, param_number);
      }
      rfx->needs_reinit |= param->reinit;
    }
  }

  if (new_green != old_value.green && param->onchange) {
    param->change_blocked = TRUE;
    retvals = do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    lives_list_free_all(&retvals);
    needs_update = TRUE;
  }
  after_any_changed_2(rfx, param, needs_update);
}


void after_param_blue_changed(LiVESSpinButton * spinbutton, lives_rfx_t *rfx) {
  LiVESList *retvals = NULL;
  lives_colRGB48_t old_value;
  int new_blue;
  int copyto = -1;
  boolean needs_update = FALSE;
  int param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), PARAM_NUMBER_KEY));
  lives_param_t *param = &rfx->params[param_number];

  get_colRGB24_param(param->value, &old_value);
  new_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  if (old_value.blue == new_blue) return;

  if (mainw->block_param_updates) {
    if (rfx->status == RFX_STATUS_WEED && param->reinit) rfx->needs_reinit |= param->reinit;
    return; // updates are blocked until all params are ready
  }

  ireinit++;

  if (rfx->status == RFX_STATUS_WEED && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING &&
      (prefs->rec_opts & REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source, param_number);
  }

  set_colRGB24_param(param->value, old_value.red, old_value.green, new_blue);

  if (mainw->framedraw_preview != NULL) reset_framedraw_preview();
  param->change_blocked = TRUE;

  if (rfx->status == RFX_STATUS_WEED) {
    int key = -1;
    weed_plant_t *inst = (weed_plant_t *)rfx->source;
    if (inst != NULL && WEED_PLANT_IS_FILTER_INSTANCE(inst)) {
      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      if (!filter_mutex_trylock(key)) {
        update_weed_color_value(inst, param_number, old_value.red, old_value.green, new_blue, 0, rfx);
        copyto = set_copy_to(inst, param_number, rfx, TRUE);
        filter_mutex_unlock(key);
        if (copyto != -1) needs_update = TRUE;
      }

      if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst, param_number);
      }
      rfx->needs_reinit |= param->reinit;
    }
  }

  if (new_blue != old_value.blue && param->onchange) {
    param->change_blocked = TRUE;
    retvals = do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    lives_list_free_all(&retvals);
    needs_update = TRUE;
  }
  after_any_changed_2(rfx, param, needs_update);
}


void after_param_alpha_changed(LiVESSpinButton * spinbutton, lives_rfx_t *rfx) {
  // not used yet
  int param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), PARAM_NUMBER_KEY));
  LiVESList *retvals = NULL;
  lives_param_t *param = &rfx->params[param_number];
  lives_colRGBA64_t old_value;
  int new_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int copyto = -1;
  boolean needs_update = FALSE;

  if (mainw->block_param_updates) {
    if (rfx->status == RFX_STATUS_WEED && param->reinit) rfx->needs_reinit |= param->reinit;
    return; // updates are blocked until all params are ready
  }

  ireinit++;

  if (rfx->status == RFX_STATUS_WEED && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING &&
      (prefs->rec_opts & REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source, param_number);
    copyto = set_copy_to((weed_plant_t *)rfx->source, param_number, rfx, FALSE);
  }

  get_colRGBA32_param(param->value, &old_value);

  if (mainw->framedraw_preview != NULL) reset_framedraw_preview();

  set_colRGBA32_param(param->value, old_value.red, old_value.green, old_value.blue, new_alpha);
  param->change_blocked = TRUE;

  if (rfx->status == RFX_STATUS_WEED && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING &&
      (prefs->rec_opts & REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source, param_number);
    if (copyto != -1) rec_param_change((weed_plant_t *)rfx->source, copyto);
  }

  if (new_alpha != old_value.alpha && param->onchange) {
    param->change_blocked = TRUE;
    retvals = do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    lives_list_free_all(&retvals);
    needs_update = TRUE;
  }
  after_any_changed_2(rfx, param, needs_update);
}


boolean after_param_text_focus_changed(LiVESWidget * hbox, LiVESWidget * child, lives_rfx_t *rfx) {
  // for non realtime effects
  // we don't usually want to run the trigger every single time the user presses a key in a text widget
  // so we only update when the user clicks OK or focusses out of the widget

  LiVESWidget *textwidget;

  if (rfx == NULL) return FALSE;

  if (mainw->multitrack != NULL) {
    if (child != NULL)
      lives_window_remove_accel_group(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->multitrack->accel_group);
    else
      lives_window_add_accel_group(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->multitrack->accel_group);
  }

  if (mainw->textwidget_focus != NULL) {
    textwidget = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->textwidget_focus), TEXTWIDGET_KEY);
    after_param_text_changed(textwidget, rfx);
  }

  if (hbox != NULL) {
    mainw->textwidget_focus = hbox;
  }

  return FALSE;
}


void after_param_text_changed(LiVESWidget * textwidget, lives_rfx_t *rfx) {
  //LiVESTextBuffer *textbuffer = NULL;
  weed_plant_t *inst = NULL, *wparam = NULL;
  LiVESList *retvals = NULL;
  lives_param_t *param;
  char *old_text;
  const char *new_text;
  int copyto = -1;
  boolean needs_update = FALSE;
  int param_number;

  if (rfx == NULL || rfx->params == NULL || textwidget == NULL) return;


  param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(textwidget), PARAM_NUMBER_KEY));
  param = &rfx->params[param_number];
  old_text = (char *)param->value;

  if (LIVES_IS_TEXT_VIEW(textwidget)) {
    new_text = lives_text_view_get_text(LIVES_TEXT_VIEW(textwidget));
    if (!lives_strcmp(new_text, old_text)) return;
  } else {
    new_text = lives_entry_get_text(LIVES_ENTRY(textwidget));
    if (!lives_strcmp(new_text, old_text)) return;
  }

  if (mainw->block_param_updates) {
    if (rfx->status == RFX_STATUS_WEED && param->reinit) rfx->needs_reinit |= param->reinit;
    return; // updates are blocked until all params are ready
  }

  ireinit++;

  param->value = lives_strdup(new_text);

  if (mainw->framedraw_preview != NULL) reset_framedraw_preview();
  param->change_blocked = TRUE;

  if (rfx->status == RFX_STATUS_WEED) {
    inst = (weed_plant_t *)rfx->source;
    if (inst != NULL && WEED_PLANT_IS_FILTER_INSTANCE(inst)) {
      char **valss;
      int index = 0, numvals, key = -1;
      wparam = weed_inst_in_param(inst, param_number, FALSE, FALSE);

      if (mainw->multitrack != NULL && is_perchannel_multi(rfx, param_number)) {
        index = mainw->multitrack->track_index;
      }

      after_any_changed_1(rfx, param_number, index);

      numvals = weed_leaf_num_elements(wparam, WEED_LEAF_VALUE);

      valss = weed_get_string_array(wparam, WEED_LEAF_VALUE, NULL);
      valss[index] = lives_strdup((char *)param->value);

      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      if (!filter_mutex_trylock(key)) {
        weed_set_string_array(wparam, WEED_LEAF_VALUE, numvals, valss);
        copyto = set_copy_to(inst, param_number, rfx, TRUE);
        filter_mutex_unlock(key);
        if (copyto != -1) needs_update = TRUE;
      }
      for (int i = 0; i < numvals; i++) lives_free(valss[i]);
      lives_free(valss);

      if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst, param_number);
        //if (copyto != -1) rec_param_change(inst, copyto);
      }
      rfx->needs_reinit |= param->reinit;
    }
  }

  if (lives_strcmp(old_text, (char *)param->value) && param->onchange) {
    param->change_blocked = TRUE;
    retvals = do_onchange(LIVES_WIDGET_OBJECT(textwidget), rfx);
    lives_list_free_all(&retvals);
    needs_update = TRUE;
  }
  after_any_changed_2(rfx, param, needs_update);
}


static void after_param_text_buffer_changed(LiVESTextBuffer * textbuffer, lives_rfx_t *rfx) {
  LiVESWidget *textview = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(textbuffer), "textview");
  after_param_text_changed(textview, rfx);
}


void after_string_list_changed(LiVESWidget * entry, lives_rfx_t *rfx) {
  LiVESList *retvals = NULL;
  int param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(entry), PARAM_NUMBER_KEY));
  LiVESCombo *combo = (LiVESCombo *)(rfx->params[param_number].widgets[0]);
  lives_param_t *param = &rfx->params[param_number];
  const char *txt = lives_combo_get_active_text(combo);
  int old_index = get_int_param(param->value);
  int new_index = lives_list_strcmp_index(param->list, txt, TRUE);
  boolean needs_update = FALSE;
  int copyto = -1;

  if (new_index == -1) return;
  if (new_index == old_index) return;

  if (mainw->block_param_updates) {
    if (rfx->status == RFX_STATUS_WEED && param->reinit) rfx->needs_reinit |= param->reinit;
    return; // updates are blocked until all params are ready
  }

  ireinit++;

  set_int_param(param->value, new_index);

  if (mainw->framedraw_preview != NULL) reset_framedraw_preview();
  param->change_blocked = TRUE;
  if (rfx->status == RFX_STATUS_WEED) {
    weed_plant_t *inst = (weed_plant_t *)rfx->source;
    if (inst != NULL && WEED_PLANT_IS_FILTER_INSTANCE(inst)) {
      //char *disp_string = get_weed_display_string(inst, param_number);
      weed_plant_t *wparam = weed_inst_in_param(inst, param_number, FALSE, FALSE);
      int index = 0, numvals;
      int key = -1;
      int *valis;

      if (mainw->multitrack != NULL && is_perchannel_multi(rfx, param_number)) {
        index = mainw->multitrack->track_index;
      }

      after_any_changed_1(rfx, param_number, index);

      valis = weed_get_int_array(wparam, WEED_LEAF_VALUE, NULL);
      valis[index] = new_index;
      numvals = weed_leaf_num_elements(wparam, WEED_LEAF_VALUE);
      if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      if (!filter_mutex_trylock(key)) {
        weed_set_int_array(wparam, WEED_LEAF_VALUE, numvals, valis);
        copyto = set_copy_to(inst, param_number, rfx, TRUE);
        filter_mutex_unlock(key);
        if (copyto != -1) needs_update = TRUE;
      }
      lives_free(valis);

      if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst, param_number);
        //if (copyto != -1) rec_param_change(inst, copyto);
      }
      rfx->needs_reinit |= param->reinit;
    }
  }

  if (old_index != new_index && param->onchange) {
    param->change_blocked = TRUE;
    retvals = do_onchange(LIVES_WIDGET_OBJECT(combo), rfx);
    lives_list_free_all(&retvals);
    needs_update = TRUE;
  }
  after_any_changed_2(rfx, param, needs_update);
}


char **param_marshall_to_argv(lives_rfx_t *rfx) {
  // this function will marshall all parameters into a argv array
  // last array element will be NULL

  // the returned **argv should be lives_free()'ed after use

  lives_colRGB48_t rgb;

  char **argv = (char **)lives_malloc((rfx->num_params + 1) * (sizeof(char *)));

  char *tmp;

  register int i;

  for (i = 0; i < rfx->num_params; i++) {
    switch (rfx->params[i].type) {
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param(rfx->params[i].value, &rgb);
      argv[i] = lives_strdup_printf("%u", (((rgb.red << 8) + rgb.green) << 8) + rgb.blue);
      break;

    case LIVES_PARAM_STRING:
      // escape strings
      argv[i] = lives_strdup_printf("%s", (tmp = U82L((char *)rfx->params[i].value)));
      lives_free(tmp);
      break;

    case LIVES_PARAM_STRING_LIST:
      // escape strings
      argv[i] = lives_strdup_printf("%d", get_int_param(rfx->params[i].value));
      break;

    default:
      if (rfx->params[i].dp) {
        char *return_pattern = lives_strdup_printf("%%.%df", rfx->params[i].dp);
        argv[i] = lives_strdup_printf(return_pattern, get_double_param(rfx->params[i].value));
        lives_free(return_pattern);
      } else {
        argv[i] = lives_strdup_printf("%d", get_int_param(rfx->params[i].value));
      }
    }
  }
  argv[i] = NULL;
  return argv;
}


char *param_marshall(lives_rfx_t *rfx, boolean with_min_max) {
  // this function will marshall all parameters into a space separated string
  // in case of string parameters, these will be surrounded by " and all
  // quotes will be escaped \"

  // the returned string should be lives_free()'ed after use
  lives_colRGB48_t rgb;

  char *new_return = lives_strdup("");
  char *old_return = new_return;
  char *return_pattern;
  char *tmp, *mysubst, *mysubst2;

  register int i;

  for (i = 0; i < rfx->num_params; i++) {
    switch (rfx->params[i].type) {
    case LIVES_PARAM_UNKNOWN:
      continue;
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param(rfx->params[i].value, &rgb);
      if (!with_min_max) {
        new_return = lives_strdup_printf("%s %u", old_return, (((rgb.red << 8) + rgb.green) << 8) + rgb.blue);
      } else {
        new_return = lives_strdup_printf("%s %d %d %d", old_return, rgb.red, rgb.green, rgb.blue);
      }
      lives_free(old_return);
      old_return = new_return;
      break;

    case LIVES_PARAM_STRING:
      // we need to doubly escape strings
      mysubst = subst((char *)rfx->params[i].value, "\\", "\\\\\\\\");
      mysubst2 = subst(mysubst, "\"", "\\\\\\\"");
      lives_free(mysubst);
      mysubst = subst(mysubst2, "`", "\\`");
      lives_free(mysubst2);
      mysubst2 = subst(mysubst, "'", "\\`");
      lives_free(mysubst);
      new_return = lives_strdup_printf("%s \"%s\"", old_return, (tmp = U82L(mysubst2)));
      lives_free(tmp);
      lives_free(mysubst2);
      lives_free(old_return);
      old_return = new_return;
      break;

    case LIVES_PARAM_STRING_LIST:
      new_return = lives_strdup_printf("%s %d", old_return, get_int_param(rfx->params[i].value));
      lives_free(old_return);
      old_return = new_return;
      break;

    default:
      if (rfx->params[i].dp) {
        return_pattern = lives_strdup_printf("%%s %%.%df", rfx->params[i].dp);
        new_return = lives_strdup_printf(return_pattern, old_return, get_double_param(rfx->params[i].value));
        if (with_min_max) {
          lives_free(old_return);
          old_return = new_return;
          new_return = lives_strdup_printf(return_pattern, old_return, rfx->params[i].min);
          lives_free(old_return);
          old_return = new_return;
          new_return = lives_strdup_printf(return_pattern, old_return, rfx->params[i].max);
        }
        lives_free(return_pattern);
      } else {
        new_return = lives_strdup_printf("%s %d", old_return, get_int_param(rfx->params[i].value));
        if (with_min_max && rfx->params[i].type != LIVES_PARAM_BOOL) {
          lives_free(old_return);
          old_return = new_return;
          new_return = lives_strdup_printf("%s %d", old_return, (int)rfx->params[i].min);
          lives_free(old_return);
          old_return = new_return;
          new_return = lives_strdup_printf("%s %d", old_return, (int)rfx->params[i].max);
        }
      }
      lives_free(old_return);
      old_return = new_return;
    }
  }
  if (mainw->current_file > 0 && with_min_max) {
    if (rfx->num_in_channels < 2) {
      new_return = lives_strdup_printf("%s %d %d %d %d %d", old_return, cfile->hsize, cfile->vsize, cfile->start,
                                       cfile->end, cfile->frames);
    } else {
      // for transitions, change the end to indicate the merge section
      // this is better for length calculations
      int cb_frames = clipboard->frames;
      int start = cfile->start, end = cfile->end, ttl;

      if (prefs->ins_resample && clipboard->fps != cfile->fps) {
        cb_frames = count_resampled_frames(clipboard->frames, clipboard->fps, cfile->fps);
      }

      if (merge_opts->spinbutton_loops != NULL &&
          cfile->end - cfile->start + 1 > (cb_frames * (ttl = lives_spin_button_get_value_as_int
                                           (LIVES_SPIN_BUTTON(merge_opts->spinbutton_loops)))) &&
          !merge_opts->loop_to_fit) {
        end = cb_frames * ttl;
        if (!merge_opts->align_start) {
          start = cfile->end - end + 1;
          end = cfile->end;
        } else {
          start = cfile->start;
          end += start - 1;
        }
      }
      new_return = lives_strdup_printf("%s %d %d %d %d %d %d %d", old_return, cfile->hsize, cfile->vsize, start, end,
                                       cfile->frames, clipboard->hsize, clipboard->vsize);
    }
  } else {
    new_return = lives_strdup(old_return);
  }
  lives_free(old_return);

  return new_return;
}


char *reconstruct_string(LiVESList * plist, int start, int *offs) {
  // convert each piece from locale to utf8
  // concat list entries to get reconstruct
  // replace \" with "

  char *word = NULL;
  char *ret = lives_strdup(""), *ret2;
  char *tmp;

  boolean lastword = FALSE;

  register int i;

  word = L2U8((char *)lives_list_nth_data(plist, start));

  if (word == NULL || !(*word) || word[0] != '\"') {
    if (word != NULL) lives_free(word);
    return 0;
  }

  word++;

  for (i = start; i < lives_list_length(plist); i++) {
    size_t wl = lives_strlen(word);
    if (wl > 0) {
      if ((word[wl - 1] == '\"') && (wl == 1 || word[wl - 2] != '\\')) {
        lastword = TRUE;
        lives_memset(word + wl - 1, 0, 1);
      }
    }

    ret2 = lives_strconcat(ret, (tmp = subst(word, "\\\"", "\"")), " ", NULL);
    lives_free(tmp);
    if (ret2 != ret) lives_free(ret);
    ret = ret2;

    if (i == start) word--;
    lives_free(word);

    if (lastword) break;

    if (i < lives_list_length(plist) - 1) word = L2U8((char *)lives_list_nth_data(plist, i + 1));
  }

  set_int_param(offs, i - start + 1);

  // remove trailing space
  lives_memset(ret + lives_strlen(ret) - 1, 0, 1);
  return ret;
}


void param_demarshall(lives_rfx_t *rfx, LiVESList * plist, boolean with_min_max, boolean upd) {
  int i;
  int pnum = 0;
  lives_param_t *param;

  // here we take a LiVESList * of param values, set them in rfx, and if upd is TRUE we also update their visual appearance

  // param->widgets[n] are only valid if upd==TRUE

  if (plist == NULL) return;

  for (i = 0; i < rfx->num_params; i++) {
    param = &rfx->params[i];
    pnum = set_param_from_list(plist, param, pnum, with_min_max, upd);
  }
}


LiVESList *argv_to_marshalled_list(lives_rfx_t *rfx, int argc, char **argv) {
  LiVESList *plist = NULL;

  char *tmp, *tmp2, *tmp3;

  register int i;

  if (argc == 0) return plist;

  for (i = 0; i <= argc && argv[i] != NULL; i++) {
    if (rfx->params[i].type == LIVES_PARAM_STRING) {
      tmp = lives_strdup_printf("\"%s\"", (tmp2 = U82L(tmp3 = subst(argv[i], "\"", "\\\""))));
      plist = lives_list_append(plist, tmp);
      lives_free(tmp2);
      lives_free(tmp3);
    } else {
      plist = lives_list_append(plist, lives_strdup(argv[i]));
    }
  }
  return plist;
}


/**
   @brief  update values for param using values in plist
  if upd is TRUE, the widgets for that param also are updated;
  otherwise, we do not update the widgets, but we do update the default

  for LIVES_PARAM_NUM, setting pnum negative avoids having to send min,max
  - deprecated, use with_min_max = FALSE
  (other types dont have a min/max anyway)

  pnum here is not param number, but rather the offset of the element in plist
*/
int set_param_from_list(LiVESList * plist, lives_param_t *param, int pnum, boolean with_min_max, boolean upd) {
  char *tmp;
  char *strval;
  int red, green, blue;
  int offs = 0;
  int maxlen = lives_list_length(plist) - 1;

  if (ABS(pnum) > maxlen) return 0;

  switch (param->type) {
  case LIVES_PARAM_BOOL:
    if (param->change_blocked) {
      pnum++;
      break;
    }
    tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
    if (upd) {
      if (param->widgets[0] && LIVES_IS_TOGGLE_BUTTON(param->widgets[0])) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(param->widgets[0]), atoi(tmp));
      }
    } else set_bool_param(param->def, (atoi(tmp)));
    if (upd && param->widgets[0] && LIVES_IS_TOGGLE_BUTTON(param->widgets[0])) {
      set_bool_param(param->value,
                     lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(param->widgets[0])));
    } else set_bool_param(param->value, (atoi(tmp)));
    lives_free(tmp);
    break;
  case LIVES_PARAM_NUM:
    if (param->change_blocked) {
      pnum++;
      if (with_min_max) pnum += 2;
      break;
    }
    if (param->dp) {
      double double_val;
      tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
      double_val = lives_strtod(tmp, NULL);
      lives_free(tmp);
      if (with_min_max) {
        if (ABS(pnum) > maxlen) return 1;
        tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
        param->min = lives_strtod(tmp, NULL);
        lives_free(tmp);
        if (ABS(pnum) > maxlen) return 2;
        tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
        param->max = lives_strtod(tmp, NULL);
        lives_free(tmp);
        if (double_val < param->min) double_val = param->min;
        if (double_val > param->max) double_val = param->max;
      }
      if (upd) {
        if (param->widgets[0] && LIVES_IS_SPIN_BUTTON(param->widgets[0])) {
          lives_rfx_t *rfx = (lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(param->widgets[0]), RFX_KEY);
          lives_signal_handlers_block_by_func(param->widgets[0], (livespointer)after_param_value_changed, (livespointer)rfx);
          lives_spin_button_set_range(LIVES_SPIN_BUTTON(param->widgets[0]), param->min, param->max);
          lives_spin_button_update(LIVES_SPIN_BUTTON(param->widgets[0]));
          lives_signal_handlers_unblock_by_func(param->widgets[0], (livespointer)after_param_value_changed, (livespointer)rfx);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), double_val);
          lives_spin_button_update(LIVES_SPIN_BUTTON(param->widgets[0]));
        }
      } else set_double_param(param->def, double_val);
      if (upd && param->widgets[0] && LIVES_IS_SPIN_BUTTON(param->widgets[0])) {
        set_double_param(param->value,
                         lives_spin_button_get_value(LIVES_SPIN_BUTTON(param->widgets[0])));
      } else set_double_param(param->value, double_val);
    } else {
      int int_value;
      int int_min, int_max;
      tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
      int_value = atoi(tmp);
      lives_free(tmp);
      if (param->step_size > 1.)
        int_value = (int)((double)int_value / param->step_size + .5) * (int)param->step_size;
      int_min = (int)param->min;
      int_max = (int)param->max;
      if (int_value < int_min) int_value = int_min;
      if (int_value > int_max) int_value = int_max;

      if (with_min_max) {
        if (ABS(pnum) > maxlen) return 1;
        tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
        int_min = atoi(tmp);
        lives_free(tmp);
        if (ABS(pnum) > maxlen) return 2;
        tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
        int_max = atoi(tmp);
        lives_free(tmp);
        if (int_value < int_min) int_value = int_min;
        if (int_value > int_max) int_value = int_max;
        param->min = (double)int_min;
        param->max = (double)int_max;
      }
      if (upd) {
        if (param->widgets[0] && LIVES_IS_SPIN_BUTTON(param->widgets[0])) {
          lives_rfx_t *rfx = (lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(param->widgets[0]), RFX_KEY);
          lives_signal_handlers_block_by_func(param->widgets[0], (livespointer)after_param_value_changed, (livespointer)rfx);
          lives_spin_button_set_range(LIVES_SPIN_BUTTON(param->widgets[0]), param->min, param->max);
          lives_spin_button_update(LIVES_SPIN_BUTTON(param->widgets[0]));
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)int_value);
          lives_spin_button_update(LIVES_SPIN_BUTTON(param->widgets[0]));
          lives_signal_handlers_unblock_by_func(param->widgets[0], (livespointer)after_param_value_changed, (livespointer)rfx);
        }
      } else set_int_param(param->def, int_value);
      if (upd && param->widgets[0] && LIVES_IS_SPIN_BUTTON(param->widgets[0])) {
        set_int_param(param->value,
                      lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(param->widgets[0])));
      } else set_int_param(param->value, int_value);
    }
    break;
  case LIVES_PARAM_COLRGB24:
    tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
    red = atoi(tmp);
    lives_free(tmp);
    if (ABS(pnum) > maxlen) return 1;
    tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
    green = atoi(tmp);
    lives_free(tmp);
    if (ABS(pnum) > maxlen) return 2;
    tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
    blue = atoi(tmp);
    lives_free(tmp);
    if (param->change_blocked) break;
    if (upd) {
      if (param->widgets[0] && LIVES_IS_SPIN_BUTTON(param->widgets[0])) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)red);
      }
      if (param->widgets[1] && LIVES_IS_SPIN_BUTTON(param->widgets[1])) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[1]), (double)green);
      }
      if (param->widgets[2] && LIVES_IS_SPIN_BUTTON(param->widgets[2])) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[2]), (double)blue);
      }
    } else set_colRGB24_param(param->def, red, green, blue);
    if (upd && param->widgets[0] && LIVES_IS_SPIN_BUTTON(param->widgets[0])
        && param->widgets[1] && LIVES_IS_SPIN_BUTTON(param->widgets[1])
        && param->widgets[2] && LIVES_IS_SPIN_BUTTON(param->widgets[2])) {
      set_colRGB24_param(param->value,
                         lives_spin_button_get_value(LIVES_SPIN_BUTTON(param->widgets[0])),
                         lives_spin_button_get_value(LIVES_SPIN_BUTTON(param->widgets[1])),
                         lives_spin_button_get_value(LIVES_SPIN_BUTTON(param->widgets[2])));
    } else set_colRGB24_param(param->value, red, green, blue);
    break;
  case LIVES_PARAM_STRING:
    strval = reconstruct_string(plist, pnum, &offs);
    pnum += offs;
    if (param->change_blocked) {
      lives_free(strval);
      break;
    }
    if (upd) {
      if (param->widgets[0]) {
        if (LIVES_IS_TEXT_VIEW(param->widgets[0])) {
          lives_text_view_set_text(LIVES_TEXT_VIEW(param->widgets[0]), strval, -1);
        } else {
          lives_entry_set_text(LIVES_ENTRY(param->widgets[0]), strval);

        }
      }
    } else {
      if (param->def != NULL) lives_free(param->def);
      param->def = (void *)lives_strdup(strval);
    }
    if (param->value != NULL) lives_free(param->value);

    /// read value back from widget in case some callback changed the value
    if (upd && param->widgets[0] && (LIVES_IS_TEXT_VIEW(param->widgets[0])
                                     || LIVES_IS_ENTRY(param->widgets[0]))) {
      lives_free(strval);
      if (LIVES_IS_TEXT_VIEW(param->widgets[0])) {
        param->value = lives_strdup(lives_text_view_get_text(LIVES_TEXT_VIEW(param->widgets[0])));
      } else {
        param->value = lives_strdup(lives_entry_get_text(LIVES_ENTRY(param->widgets[0])));
      }
    } else {
      param->value = strval;
    }
    break;
  case LIVES_PARAM_STRING_LIST: {
    int int_value;
    tmp = lives_strdup((char *)lives_list_nth_data(plist, pnum++));
    int_value = atoi(tmp);
    lives_free(tmp);
    if (param->change_blocked) break;
    if (upd && param->widgets[0] && LIVES_IS_COMBO(param->widgets[0]) && int_value < lives_list_length(param->list))
      lives_combo_set_active_string(LIVES_COMBO(param->widgets[0]), (char *)lives_list_nth_data(param->list, int_value));
    if (!upd) set_int_param(param->def, int_value);
    if (upd && param->widgets[0] && LIVES_IS_COMBO(param->widgets[0])) {
      const char *txt = lives_combo_get_active_text(LIVES_COMBO(param->widgets[0]));
      int new_index = lives_list_strcmp_index(param->list, txt, TRUE);
      set_int_param(param->value, new_index);
    } else set_int_param(param->value, int_value);
    break;
  }
  default:
    break;
  }
  return pnum;
}


LiVESList *do_onchange(LiVESWidgetObject * object, lives_rfx_t *rfx) {
  LiVESList *retvals;

  int which = LIVES_POINTER_TO_INT(lives_widget_object_get_data(object, PARAM_NUMBER_KEY));
  int width = 0, height = 0;

  const char *handle = "";

  char *plugdir;
  char *com, *tmp;

  // weed plugins do not have triggers
  if (rfx->status == RFX_STATUS_WEED) return NULL;

  if (which < 0) {
    // init
    switch (rfx->status) {
    case RFX_STATUS_BUILTIN:
      plugdir = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_RENDERED_EFFECTS_BUILTIN, NULL);
      break;
    case RFX_STATUS_CUSTOM:
      plugdir = lives_build_filename(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_CUSTOM, NULL);
      break;
    case RFX_STATUS_TEST:
      plugdir = lives_build_filename(prefs->config_datadir, PLUGIN_RENDERED_EFFECTS_TEST, NULL);
      break;
    default:
      plugdir = lives_strdup_printf("%s", prefs->workdir);
    }

    if (mainw->current_file > 0) {
      width = cfile->hsize;
      height = cfile->vsize;
      handle = cfile->handle;
    }

    com = lives_strdup_printf("%s \"fxinit_%s\" \"%s\" \"%s\" %d %d %s", prefs->backend_sync, rfx->name, handle, plugdir,
                              width, height, (tmp = param_marshall(rfx, TRUE)));
    retvals = plugin_request_by_space(NULL, NULL, com);

    lives_free(tmp);
    lives_free(plugdir);
  } else {
    com = lives_strdup_printf("onchange_%d%s", which, param_marshall(rfx, TRUE));
    switch (rfx->status) {
    case RFX_STATUS_BUILTIN:
      retvals = plugin_request_by_space(PLUGIN_RENDERED_EFFECTS_BUILTIN, rfx->name, com);
      break;
    case RFX_STATUS_CUSTOM:
      retvals = plugin_request_by_space(PLUGIN_RENDERED_EFFECTS_CUSTOM, rfx->name, com);
      break;
    case RFX_STATUS_TEST:
      retvals = plugin_request_by_space(PLUGIN_RENDERED_EFFECTS_TEST, rfx->name, com);
      break;
    default:
      retvals = plugin_request_by_space(PLUGIN_RFX_SCRAP, rfx->name, com);
    }
  }

  if (retvals != NULL) {
    param_demarshall(rfx, retvals, TRUE, which >= 0);
  } else {
    if (which <= 0 && mainw->error) {
      mainw->error = FALSE;
      do_error_dialog(lives_strdup_printf("\n\n%s\n\n", mainw->msg));
    }
  }
  lives_free(com);

  return retvals;
}


void on_pwcolsel(LiVESButton * button, lives_rfx_t *rfx) {
  LiVESWidgetColor selected;

  int pnum = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), PARAM_NUMBER_KEY));
  int r, g, b;

  lives_param_t *param = &rfx->params[pnum];

  lives_color_button_get_color(LIVES_COLOR_BUTTON(button), &selected);

  r = (int)((double)(selected.red + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
  g = (int)((double)(selected.green + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
  b = (int)((double)(selected.blue + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));

  set_colRGB24_param(param->value, r, g, b);

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]), (double)r);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[1]), (double)g);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[2]), (double)b);
  lives_color_button_set_color(LIVES_COLOR_BUTTON(param->widgets[4]), &selected);
}


void update_visual_params(lives_rfx_t *rfx, boolean update_hidden) {
  // update parameters visually from an rfx object
  LiVESList *list;

  weed_plant_t **in_params, *in_param;
  weed_plant_t *inst = (weed_plant_t *)rfx->source;
  weed_plant_t *paramtmpl;

  int *colsi, *colsis, *valis;
  int *maxis = NULL, *minis = NULL;

  double *colsd, *colsds, *valds;
  double *maxds = NULL, *minds = NULL;

  double red_maxd, green_maxd, blue_maxd;
  double red_mind, green_mind, blue_mind;
  double vald, mind, maxd;

  char **valss;

  char *vals, *pattern;
  char *tmp, *tmp2;

  int cspace;
  int error;
  int num_params = 0;
  int param_type;
  int vali, mini, maxi;

  int red_max, green_max, blue_max;
  int red_min, green_min, blue_min;

  int index, numvals;
  int key = -1;

  register int i, j;

  if (rfx->source_type != LIVES_RFX_SOURCE_WEED) return;

  in_params = weed_instance_get_in_params(inst, &num_params);
  if (num_params == 0) return;

  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, &error);

  for (i = 0; i < num_params; i++) {
    if (!is_hidden_param(inst, i) || update_hidden) {
      // by default we dont update hidden or reinit params

      in_param = in_params[i];
      paramtmpl = weed_param_get_template(in_param);
      param_type = weed_paramtmpl_get_type(paramtmpl);
      list = NULL;

      // assume index is 0, unless we are a framedraw multi parameter
      // most of the time this will be ok, as other such multivalued parameters should be hidden
      index = 0;

      if (mainw->multitrack != NULL && is_perchannel_multi(rfx, i)) {
        index = mainw->multitrack->track_index;
      }

      filter_mutex_lock(key);

      numvals = weed_leaf_num_elements(in_param, WEED_LEAF_VALUE);

      if (param_type != WEED_PARAM_COLOR && index >= numvals) {
        fill_param_vals_to(in_param, paramtmpl, index);
        numvals = index + 1;
      }

      switch (param_type) {
      case WEED_PARAM_INTEGER:
        valis = weed_get_int_array(in_param, WEED_LEAF_VALUE, &error);
        vali = valis[index];
        lives_free(valis);

        mini = weed_get_int_value(paramtmpl, WEED_LEAF_MIN, &error);
        maxi = weed_get_int_value(paramtmpl, WEED_LEAF_MAX, &error);

        list = lives_list_append(list, lives_strdup_printf("%d", vali));
        list = lives_list_append(list, lives_strdup_printf("%d", mini));
        list = lives_list_append(list, lives_strdup_printf("%d", maxi));
        set_param_from_list(list, &rfx->params[i], 0, TRUE, TRUE);
        lives_list_free_all(&list);

        break;
      case WEED_PARAM_FLOAT:
        valds = weed_get_double_array(in_param, WEED_LEAF_VALUE, &error);
        vald = valds[index];
        lives_free(valds);

        mind = weed_get_double_value(paramtmpl, WEED_LEAF_MIN, &error);
        maxd = weed_get_double_value(paramtmpl, WEED_LEAF_MAX, &error);

        pattern = lives_strdup("%.2f");

        if (weed_plant_has_leaf(paramtmpl, WEED_LEAF_GUI)) {
          weed_plant_t *gui = weed_get_plantptr_value(paramtmpl, WEED_LEAF_GUI, &error);
          if (weed_plant_has_leaf(gui, WEED_LEAF_DECIMALS)) {
            int dp = weed_get_int_value(gui, WEED_LEAF_DECIMALS, &error);
            lives_free(pattern);
            pattern = lives_strdup_printf("%%.%df", dp);
          }
        }

        list = lives_list_append(list, lives_strdup_printf(pattern, vald));
        list = lives_list_append(list, lives_strdup_printf(pattern, mind));
        list = lives_list_append(list, lives_strdup_printf(pattern, maxd));

        lives_free(pattern);

        set_param_from_list(list, &rfx->params[i], 0, TRUE, TRUE);
        lives_list_free_all(&list);

        break;
      case WEED_PARAM_SWITCH:
        valis = weed_get_boolean_array(in_param, WEED_LEAF_VALUE, &error);
        vali = valis[index];
        lives_free(valis);

        list = lives_list_append(list, lives_strdup_printf("%d", vali));
        set_param_from_list(list, &rfx->params[i], 0, FALSE, TRUE);
        lives_list_free_all(&list);

        break;
      case WEED_PARAM_TEXT:
        valss = weed_get_string_array(in_param, WEED_LEAF_VALUE, &error);
        vals = valss[index];
        list = lives_list_append(list, lives_strdup_printf("\"%s\"", (tmp = U82L(tmp2 = subst(vals, "\"", "\\\"")))));
        lives_free(tmp);
        lives_free(tmp2);
        set_param_from_list(list, &rfx->params[i], 0, FALSE, TRUE);
        for (j = 0; j < numvals; j++) {
          lives_free(valss[j]);
        }
        lives_free(valss);
        lives_list_free_all(&list);

        break;
      case WEED_PARAM_COLOR:
        cspace = weed_get_int_value(paramtmpl, WEED_LEAF_COLORSPACE, &error);
        switch (cspace) {
        case WEED_COLORSPACE_RGB:
          numvals = weed_leaf_num_elements(in_param, WEED_LEAF_VALUE);
          if (index * 3 >= numvals) fill_param_vals_to(in_param, paramtmpl, index);

          if (weed_leaf_seed_type(paramtmpl, WEED_LEAF_DEFAULT) == WEED_SEED_INT) {
            colsis = weed_get_int_array(in_param, WEED_LEAF_VALUE, &error);
            colsi = &colsis[3 * index];

            if (weed_leaf_num_elements(paramtmpl, WEED_LEAF_MAX) == 1) {
              red_max = green_max = blue_max = weed_get_int_value(paramtmpl, WEED_LEAF_MAX, &error);
            } else {
              maxis = weed_get_int_array(paramtmpl, WEED_LEAF_MAX, &error);
              red_max = maxis[0];
              green_max = maxis[1];
              blue_max = maxis[2];
            }
            if (weed_leaf_num_elements(paramtmpl, WEED_LEAF_MIN) == 1) {
              red_min = green_min = blue_min = weed_get_int_value(paramtmpl, WEED_LEAF_MIN, &error);
            } else {
              minis = weed_get_int_array(paramtmpl, WEED_LEAF_MIN, &error);
              red_min = minis[0];
              green_min = minis[1];
              blue_min = minis[2];
            }

            colsi[0] = (int)((double)(colsi[0] - red_min) / (double)(red_max - red_min) * 255. + .5);
            colsi[1] = (int)((double)(colsi[1] - green_min) / (double)(green_max - green_min) * 255. + .5);
            colsi[2] = (int)((double)(colsi[2] - blue_min) / (double)(blue_max - blue_min) * 255. + .5);

            if (colsi[0] < red_min) colsi[0] = red_min;
            if (colsi[1] < green_min) colsi[1] = green_min;
            if (colsi[2] < blue_min) colsi[2] = blue_min;
            if (colsi[0] > red_max) colsi[0] = red_max;
            if (colsi[1] > green_max) colsi[1] = green_max;
            if (colsi[2] > blue_max) colsi[2] = blue_max;

            list = lives_list_append(list, lives_strdup_printf("%d", colsi[0]));
            list = lives_list_append(list, lives_strdup_printf("%d", colsi[1]));
            list = lives_list_append(list, lives_strdup_printf("%d", colsi[2]));

            set_param_from_list(list, &rfx->params[i], 0, FALSE, TRUE);

            lives_list_free_all(&list);
            lives_free(colsis);
            if (maxis != NULL) lives_free(maxis);
            if (minis != NULL) lives_free(minis);
          } else {
            colsds = weed_get_double_array(in_param, WEED_LEAF_VALUE, &error);
            colsd = &colsds[3 * index];
            if (weed_leaf_num_elements(paramtmpl, WEED_LEAF_MAX) == 1) {
              red_maxd = green_maxd = blue_maxd = weed_get_double_value(paramtmpl, WEED_LEAF_MAX, &error);
            } else {
              maxds = weed_get_double_array(paramtmpl, WEED_LEAF_MAX, &error);
              red_maxd = maxds[0];
              green_maxd = maxds[1];
              blue_maxd = maxds[2];
            }
            if (weed_leaf_num_elements(paramtmpl, WEED_LEAF_MIN) == 1) {
              red_mind = green_mind = blue_mind = weed_get_double_value(paramtmpl, WEED_LEAF_MIN, &error);
            } else {
              minds = weed_get_double_array(paramtmpl, WEED_LEAF_MIN, &error);
              red_mind = minds[0];
              green_mind = minds[1];
              blue_mind = minds[2];
            }
            colsd[0] = (colsd[0] - red_mind) / (red_maxd - red_mind) * 255. + .5;
            colsd[1] = (colsd[1] - green_mind) / (green_maxd - green_mind) * 255. + .5;
            colsd[2] = (colsd[2] - blue_mind) / (blue_maxd - blue_mind) * 255. + .5;

            if (colsd[0] < red_mind) colsd[0] = red_mind;
            if (colsd[1] < green_mind) colsd[1] = green_mind;
            if (colsd[2] < blue_mind) colsd[2] = blue_mind;
            if (colsd[0] > red_maxd) colsd[0] = red_maxd;
            if (colsd[1] > green_maxd) colsd[1] = green_maxd;
            if (colsd[2] > blue_maxd) colsd[2] = blue_maxd;

            list = lives_list_append(list, lives_strdup_printf("%.2f", colsd[0]));
            list = lives_list_append(list, lives_strdup_printf("%.2f", colsd[1]));
            list = lives_list_append(list, lives_strdup_printf("%.2f", colsd[2]));
            set_param_from_list(list, &rfx->params[i], 0, FALSE, TRUE);

            lives_list_free_all(&list);
            lives_free(colsds);
            if (maxds != NULL) lives_free(maxds);
            if (minds != NULL) lives_free(minds);
          }
          break;
          // TODO - other color spaces, e.g. RGBA24
        }
        break;
      } // hint
    }
    filter_mutex_unlock(key);
  }
  lives_free(in_params);
}
