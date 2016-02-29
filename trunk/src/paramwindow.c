// paramwindow.c
// LiVES
// (c) G. Finch 2004 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// dynamic window generation from parameter arrays :-)



// NOTE: this is now the "house style" for all LiVES widgets, there should be a consistent look across all
// windows.

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"
#include "paramwindow.h"
#include "callbacks.h"
#include "support.h"
#include "resample.h"
#include "effects.h"
#include "rte_window.h"
#include "framedraw.h"
#include "ce_thumbs.h"

#ifdef ENABLE_GIW
#include "giw/giwknob.h"
#endif

extern boolean do_effect(lives_rfx_t *, boolean is_preview);  //effects.c in LiVES
extern void on_realfx_activate(LiVESMenuItem *, livespointer rfx);  // effects.c in LiVES


static void after_param_text_buffer_changed(LiVESTextBuffer *textbuffer, lives_rfx_t *rfx);


LiVESWidget *fx_dialog[2];

// TODO -
// use list of these in case we have multiple windows open
// right now this is single threaded because of this
static LiVESSList *usrgrp_to_livesgrp[2]= {NULL,NULL}; // ordered list of lives_widget_group_t

LiVESList *do_onchange_init(lives_rfx_t *rfx) {
  LiVESList *onchange=NULL;
  LiVESList *retvals=NULL;
  char **array;
  char *type;

  register int i;

  if (rfx->status==RFX_STATUS_WEED) return NULL;

  switch (rfx->status) {
  case RFX_STATUS_BUILTIN:
    type=lives_strdup(PLUGIN_RENDERED_EFFECTS_BUILTIN);
    break;
  case RFX_STATUS_CUSTOM:
    type=lives_strdup(PLUGIN_RENDERED_EFFECTS_CUSTOM);
    break;
  default:
    type=lives_strdup_printf(PLUGIN_RENDERED_EFFECTS_TEST);
    break;
  }
  if ((onchange=plugin_request_by_line(type,rfx->name,"get_onchange"))!=NULL) {
    for (i=0; i<lives_list_length(onchange); i++) {
      array=lives_strsplit((char *)lives_list_nth_data(onchange,i),rfx->delim,-1);
      if (!strcmp(array[0],"init")) {
        // onchange is init
        // create dummy object with data
        LiVESWidget *dummy_widget=lives_label_new(NULL);
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dummy_widget),"param_number",LIVES_INT_TO_POINTER(-1));
        retvals=do_onchange(LIVES_WIDGET_OBJECT(dummy_widget),rfx);
        lives_widget_destroy(dummy_widget);
        lives_strfreev(array);
        break;
      }
      lives_strfreev(array);
    }
    lives_list_free_strings(onchange);
    lives_list_free(onchange);
  }
  lives_free(type);

  return retvals;
}


void on_paramwindow_ok_clicked(LiVESButton *button, lives_rfx_t *rfx) {
  register int i;

  if (rfx!=NULL&&rfx->status!=RFX_STATUS_SCRAP) mainw->keep_pre=mainw->did_rfx_preview;

  if (mainw->textwidget_focus!=NULL) {
    LiVESWidget *textwidget=(LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->textwidget_focus),"textwidget");
    after_param_text_changed(textwidget,rfx);
  }

  if (mainw->did_rfx_preview) {
    for (i=0; i<rfx->num_params; i++) {
      if (rfx->params[i].changed) {
        mainw->keep_pre=FALSE;
        break;
      }
    }

    if (!mainw->keep_pre) {
      lives_kill_subprocesses(cfile->handle,TRUE);

      if (cfile->start==0) {
        cfile->start=1;
        cfile->end=cfile->frames;
      }

      do_rfx_cleanup(rfx);
    }
    mainw->did_rfx_preview=FALSE;
    mainw->show_procd=TRUE;
  }

  if (button!=NULL) {
    lives_general_button_clicked(button,NULL);
  }

  if (usrgrp_to_livesgrp[0]!=NULL) lives_slist_free(usrgrp_to_livesgrp[0]);
  usrgrp_to_livesgrp[0]=NULL;
  if (fx_dialog[1]==NULL) special_cleanup();
  if (mainw->invis!=NULL) {
    lives_widget_destroy(mainw->invis);
    mainw->invis=NULL;
  }

  if (rfx!=NULL&&rfx->status==RFX_STATUS_SCRAP) return;

  if (rfx->status==RFX_STATUS_WEED) on_realfx_activate(NULL,rfx);
  else on_render_fx_activate(NULL,rfx);

  mainw->keep_pre=FALSE;
  mainw->is_generating=FALSE;

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



void on_paramwindow_cancel_clicked2(LiVESButton *button, lives_rfx_t *rfx) {
  // close from rte window

  on_paramwindow_cancel_clicked(button,rfx);
  fx_dialog[1]=NULL;
  if (mainw->invis!=NULL) {
    lives_widget_destroy(mainw->invis);
    mainw->invis=NULL;
  }
}

void on_paramwindow_cancel_clicked(LiVESButton *button, lives_rfx_t *rfx) {
  mainw->block_param_updates=TRUE;
  if (mainw->did_rfx_preview) {
    lives_kill_subprocesses(cfile->handle,TRUE);

    mainw->did_rfx_preview=FALSE;
    mainw->show_procd=TRUE;

    if (cfile->start==0) {
      cfile->start=1;
      cfile->end=cfile->frames;
    }

    do_rfx_cleanup(rfx);
  }

  if (rfx!=NULL&&rfx->name!=NULL&&rfx->status!=RFX_STATUS_WEED&&rfx->status!=RFX_STATUS_SCRAP&&
      rfx->num_in_channels==0&&rfx->min_frames>=0&&!rfx->is_template) {
    // for a generator, we silently close the (now) temporary file we would have generated frames into
    mainw->suppress_dprint=TRUE;
    close_current_file(mainw->pre_src_file);
    mainw->suppress_dprint=FALSE;
    mainw->is_generating=FALSE;
    if (mainw->multitrack!=NULL) mainw->pre_src_file=-1;
  }

  if (button!=NULL) {
    lives_general_button_clicked(button,NULL);
  }
  if (rfx==NULL) {
    if (usrgrp_to_livesgrp[1]!=NULL) lives_slist_free(usrgrp_to_livesgrp[1]);
    usrgrp_to_livesgrp[1]=NULL;
  } else {
    if (usrgrp_to_livesgrp[0]!=NULL) lives_slist_free(usrgrp_to_livesgrp[0]);
    usrgrp_to_livesgrp[0]=NULL;
    if (rfx->status==RFX_STATUS_WEED&&rfx!=mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx) {
      rfx_free(rfx);
      lives_free(rfx);
    }
  }
  if (fx_dialog[1]==NULL) special_cleanup();

  mainw->block_param_updates=FALSE;

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}

/**
   get a (radiobutton) list from an index
*/
static lives_widget_group_t *get_group(lives_rfx_t *rfx, lives_param_t *param) {


  if (rfx->status==RFX_STATUS_WEED) {
    return livesgrp_from_usrgrp(usrgrp_to_livesgrp[1], param->group);
  } else {
    return livesgrp_from_usrgrp(usrgrp_to_livesgrp[0], param->group);
  }
  return NULL;
}


void on_render_fx_activate(LiVESMenuItem *menuitem, lives_rfx_t *rfx) {
  boolean has_lmap_error=FALSE;

  if (menuitem!=NULL&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&rfx->num_in_channels>0&&
      (mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
    if (!do_layout_alter_frames_warning()) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,
                   0,0.,cfile->stored_layout_frame>0);
    has_lmap_error=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  // do onchange|init
  if (menuitem!=NULL) {
    LiVESList *retvals=do_onchange_init(rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
  }
  if (rfx->min_frames>-1) {
    do_effect(rfx,FALSE);
  }
  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

}


static void gen_width_changed(LiVESSpinButton *spin, livespointer user_data) {
  weed_plant_t *ctmpl=(weed_plant_t *)user_data;
  int val=lives_spin_button_get_value_as_int(spin);
  int error,old_val=0;
  int step;

  if (weed_plant_has_leaf(ctmpl,"host_width")) old_val=weed_get_int_value(ctmpl,"host_width",&error);

  if (val==old_val) return;
  step=1;
  if (weed_plant_has_leaf(ctmpl,"hstep")) step=weed_get_int_value(ctmpl,"hstep",&error);

  val=step_val(val,step);
  weed_set_int_value(ctmpl,"host_width",val);
  lives_spin_button_set_value(spin,(double)val);
}


static void gen_height_changed(LiVESSpinButton *spin, livespointer user_data) {
  weed_plant_t *ctmpl=(weed_plant_t *)user_data;
  int val=lives_spin_button_get_value_as_int(spin);
  int error,old_val=0;
  int step;

  if (weed_plant_has_leaf(ctmpl,"host_height")) old_val=weed_get_int_value(ctmpl,"host_height",&error);

  if (val==old_val) return;
  step=1;
  if (weed_plant_has_leaf(ctmpl,"hstep")) step=weed_get_int_value(ctmpl,"hstep",&error);

  val=step_val(val,step);
  weed_set_int_value(ctmpl,"host_height",val);
  lives_spin_button_set_value(spin,(double)val);
}


static void gen_fps_changed(LiVESSpinButton *spin, livespointer user_data) {
  weed_plant_t *filter=(weed_plant_t *)user_data;
  double val=lives_spin_button_get_value(spin);
  weed_set_double_value(filter,"host_fps",val);
}



static void trans_in_out_pressed(lives_rfx_t *rfx, boolean in) {
  weed_plant_t **in_params;

  weed_plant_t *inst=(weed_plant_t *)rfx->source;
  weed_plant_t *filter=weed_instance_get_filter(inst,TRUE);
  weed_plant_t *tparam;
  weed_plant_t *tparamtmpl;

  int key=-1;
  int hint,error,nparams;
  int old_val;
  int trans=get_transition_param(filter,FALSE);

  do {
    // handle compound fx
    if (weed_plant_has_leaf(inst,"in_parameters")) {
      nparams=weed_leaf_num_elements(inst,"in_parameters");
      if (trans<nparams) break;
      trans-=nparams;
    }
  } while (weed_plant_has_leaf(inst,"host_next_instance")&&(inst=weed_get_plantptr_value(inst,"host_next_instance",&error))!=NULL);

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  tparam=in_params[trans];
  tparamtmpl=weed_get_plantptr_value(tparam,"template",&error);
  hint=weed_get_int_value(tparamtmpl,"hint",&error);

  old_val=get_int_param(rfx->params[trans].value);

  if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);
  filter_mutex_lock(key);
  if (hint==WEED_HINT_INTEGER) {
    if (in) weed_set_int_value(tparam,"value",weed_get_int_value(tparamtmpl,"min",&error));
    else weed_set_int_value(tparam,"value",weed_get_int_value(tparamtmpl,"max",&error));
  } else {
    if (in) weed_set_double_value(tparam,"value",weed_get_double_value(tparamtmpl,"min",&error));
    else weed_set_double_value(tparam,"value",weed_get_double_value(tparamtmpl,"max",&error));
  }
  filter_mutex_unlock(key);
  set_copy_to(inst,trans,TRUE);
  update_visual_params(rfx,FALSE);
  lives_free(in_params);

  if (mainw->multitrack!=NULL) {
    // force parameter update in multitrack
    set_int_param(rfx->params[trans].value,old_val);
    after_param_value_changed(LIVES_SPIN_BUTTON(rfx->params[trans].widgets[0]), rfx);
  }

}


static void transition_in_pressed(LiVESToggleButton *tbut, livespointer rfx) {
  trans_in_out_pressed((lives_rfx_t *)rfx,TRUE);
}

static void transition_out_pressed(LiVESToggleButton *tbut, livespointer rfx) {
  trans_in_out_pressed((lives_rfx_t *)rfx,FALSE);
}

static void after_transaudio_toggled(LiVESToggleButton *togglebutton, livespointer rfx) {
  weed_plant_t *init_event=mainw->multitrack->init_event;

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(togglebutton)))
    weed_set_boolean_value(init_event,"host_audio_transition",WEED_TRUE);
  else weed_set_boolean_value(init_event,"host_audio_transition",WEED_FALSE);

}

static void gen_cb_toggled(LiVESToggleButton *tbut, livespointer rfx) {
  mainw->gen_to_clipboard=!mainw->gen_to_clipboard;
}




void transition_add_in_out(LiVESBox *vbox, lives_rfx_t *rfx, boolean add_audio_check) {
  // add in/out radios for multitrack transitions
  LiVESWidget *radiobutton_in;
  LiVESWidget *radiobutton_out;
  LiVESWidget *hbox,*hbox2;
  LiVESWidget *hseparator;

  LiVESSList *radiobutton_group = NULL;

  char *tmp,*tmp2;

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_width);

  radiobutton_in=lives_standard_radio_button_new((tmp=lives_strdup(_("Transition _In"))),TRUE,
                 radiobutton_group,LIVES_BOX(hbox),
                 (tmp2=lives_strdup(_("Click to set the transition parameter to show only the front frame"))));
  lives_free(tmp);
  lives_free(tmp2);

  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton_in));

  lives_signal_connect_after(LIVES_GUI_OBJECT(radiobutton_in), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(transition_in_pressed),
                             (livespointer)rfx);



  if (add_audio_check) {
    int error;
    weed_plant_t *filter=weed_instance_get_filter((weed_plant_t *)rfx->source,FALSE);

    LiVESWidget *checkbutton;

    hbox2 = lives_hbox_new(FALSE, 0);

    if (has_video_chans_in(filter,FALSE))
      lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, FALSE, widget_opts.packing_width);

    checkbutton = lives_standard_check_button_new((tmp=lives_strdup(_("Crossfade audio"))),FALSE,LIVES_BOX(hbox2),
                  (tmp2=lives_strdup(_("Check the box to make audio transition with the video"))));

    lives_free(tmp);
    lives_free(tmp2);

    if (!weed_plant_has_leaf(mainw->multitrack->init_event,"host_audio_transition")||
        weed_get_boolean_value(mainw->multitrack->init_event,"host_audio_transition",&error)==WEED_FALSE)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton),FALSE);
    else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), TRUE);

    lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(after_transaudio_toggled),
                               (livespointer)rfx);

    after_transaudio_toggled(LIVES_TOGGLE_BUTTON(checkbutton),(livespointer)rfx);

  }

  widget_opts.pack_end=TRUE;
  radiobutton_out=lives_standard_radio_button_new((tmp=lives_strdup(_("Transition _Out"))),TRUE,
                  radiobutton_group,LIVES_BOX(hbox),
                  (tmp2=lives_strdup(_("Click to set the transition parameter to show only the rear frame"))));

  lives_free(tmp);
  lives_free(tmp2);

  widget_opts.pack_end=FALSE;

  lives_signal_connect_after(LIVES_GUI_OBJECT(radiobutton_out), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(transition_out_pressed),
                             (livespointer)rfx);

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  hseparator = lives_hseparator_new();
  lives_box_pack_start(vbox, hseparator, FALSE, FALSE, 0);

}






static boolean add_sizes(LiVESBox *vbox, boolean add_fps, boolean has_param, lives_rfx_t *rfx) {
  // add size settings for generators and resize effects
  static lives_param_t aspect_width,aspect_height;

  LiVESWidget *label,*hbox;
  LiVESWidget *spinbuttonh=NULL,*spinbuttonw=NULL;
  LiVESWidget *spinbuttonf;

  int error;

  weed_plant_t *filter=weed_instance_get_filter((weed_plant_t *)rfx->source,TRUE),*tmpl;

  weed_plant_t **ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);

  double def_fps=0.;

  char *cname,*ltxt;

  boolean added=add_fps;
  boolean chk_params=(vbox==NULL);

  int num_chans=weed_leaf_num_elements(filter,"out_channel_templates");

  int def_width=0,max_width,width_step;
  int def_height=0,max_height,height_step;

  register int i;


  if (!has_param) {
    lives_widget_set_size_request(LIVES_WIDGET(vbox), RFX_WINSIZE_H, RFX_WINSIZE_V);
  }

  // add fps


  if (add_fps) {

    if (has_param)
      add_hsep_to_box(vbox);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height*2);

    add_fill_to_box(LIVES_BOX(hbox));

    if (weed_plant_has_leaf(filter,"host_fps")) def_fps=weed_get_double_value(filter,"host_fps",&error);
    else if (weed_plant_has_leaf(filter,"target_fps")) def_fps=weed_get_double_value(filter,"target_fps",&error);

    if (def_fps==0.) def_fps=prefs->default_fps;

    spinbuttonf = lives_standard_spin_button_new(_("Target _FPS (plugin may override this)"),TRUE,
                  def_fps,1.,FPS_MAX,1.,10.,3,LIVES_BOX(hbox),NULL);

    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbuttonf), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(gen_fps_changed),
                               filter);

    add_fill_to_box(LIVES_BOX(hbox));
  }

  for (i=0; i<num_chans; i++) {
    tmpl=ctmpls[i];

    // TODO ***: allow alteration of "host_disabled" under some circumstances
    // (e.g. allow enabling a first or second in channel, or first out_channel, or more for alphas)

    // make this into function called from here and from effects with optional enable-able channels
    if (weed_plant_has_leaf(tmpl,"host_disabled")&&weed_get_boolean_value(tmpl,"host_disabled",&error)==WEED_TRUE) continue;
    if (weed_plant_has_leaf(tmpl,"width")&&weed_get_int_value(tmpl,"width",&error)!=0) continue;
    if (weed_plant_has_leaf(tmpl,"height")&&weed_get_int_value(tmpl,"height",&error)!=0) continue;

    added=TRUE;

    if (chk_params) continue;

    if (rfx->is_template) {
      cname=weed_get_string_value(tmpl,"name",&error);
      ltxt=lives_strdup_printf(_("%s : size"),cname);
      lives_free(cname);
    } else {
      ltxt=lives_strdup(_("New size (pixels)"));
    }

    label=lives_standard_label_new(ltxt);
    lives_free(ltxt);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);


    if (weed_plant_has_leaf(tmpl,"host_width")) def_width=weed_get_int_value(tmpl,"host_width",&error);
    if (def_width==0) def_width=DEF_GEN_WIDTH;
    max_width=INT_MAX;
    if (weed_plant_has_leaf(tmpl,"maxwidth")) max_width=weed_get_int_value(tmpl,"maxwidth",&error);
    if (def_width>max_width) def_width=max_width;
    width_step=1;
    if (weed_plant_has_leaf(tmpl,"hstep")) width_step=weed_get_int_value(tmpl,"hstep",&error);

    spinbuttonw = lives_standard_spin_button_new(_("_Width"),TRUE,def_width,4.,max_width,width_step==1?4:width_step,
                  width_step==1?16:width_step*4,0,
                  LIVES_BOX(hbox),NULL);

    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbuttonw), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(gen_width_changed),
                               tmpl);
    weed_leaf_delete(tmpl,"host_width"); // force a reset
    gen_width_changed(LIVES_SPIN_BUTTON(spinbuttonw),tmpl);

    if (weed_plant_has_leaf(tmpl,"host_height")) def_height=weed_get_int_value(tmpl,"host_height",&error);
    if (def_height==0) def_height=DEF_GEN_HEIGHT;
    max_height=INT_MAX;
    if (weed_plant_has_leaf(tmpl,"maxheight")) max_height=weed_get_int_value(tmpl,"maxheight",&error);
    if (def_height>max_height) def_height=max_height;
    height_step=1;
    if (weed_plant_has_leaf(tmpl,"vstep")) height_step=weed_get_int_value(tmpl,"vstep",&error);


    spinbuttonh = lives_standard_spin_button_new(_("_Height"),TRUE,def_height,4.,max_height,height_step==1?4:height_step,
                  height_step==1?16:height_step*4,0,
                  LIVES_BOX(hbox),NULL);

    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbuttonh), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(gen_height_changed),
                               tmpl);
    weed_leaf_delete(tmpl,"host_height"); // force a reset
    gen_height_changed(LIVES_SPIN_BUTTON(spinbuttonh),tmpl);

  }


  if (!chk_params) {
    if (!rfx->is_template) {
      // add "aspectratio" widget
      init_special();

      aspect_width.widgets[0]=spinbuttonw;
      aspect_height.widgets[0]=spinbuttonh;

      set_aspect_ratio_widgets(&aspect_width,&aspect_height);
      check_for_special(rfx,&aspect_width,vbox);
      check_for_special(rfx,&aspect_height,vbox);
    }

  }

  return added;

}




static void add_gen_to(LiVESBox *vbox, lives_rfx_t *rfx) {
  // add "generate to clipboard/new clip" for rendered generators
  LiVESSList *radiobutton_group = NULL;

  LiVESWidget *radiobutton;
  LiVESWidget *hseparator;

  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);

  char *tmp,*tmp2;

  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  radiobutton = lives_standard_radio_button_new((tmp=lives_strdup(_("Generate to _Clipboard"))),
                TRUE,radiobutton_group,LIVES_BOX(hbox),
                (tmp2=lives_strdup(_("Generate frames to the clipboard"))));

  lives_free(tmp);
  lives_free(tmp2);

  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton));

  widget_opts.pack_end=TRUE;
  radiobutton=lives_standard_radio_button_new((tmp=lives_strdup(_("Generate to _New Clip"))),
              TRUE,radiobutton_group,LIVES_BOX(hbox),
              (tmp2=lives_strdup(_("Generate frames to a new clip"))));
  widget_opts.pack_end=FALSE;

  lives_free(tmp);
  lives_free(tmp2);

  hseparator = lives_hseparator_new();
  lives_box_pack_start(vbox, hseparator, FALSE, FALSE, 0);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), !mainw->gen_to_clipboard);

  lives_signal_connect_after(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(gen_cb_toggled),
                             (livespointer)rfx);

}




void on_render_fx_pre_activate(LiVESMenuItem *menuitem, lives_rfx_t *rfx) {
  on_fx_pre_activate(rfx,0,NULL);
}


void on_fx_pre_activate(lives_rfx_t *rfx, int didx, LiVESWidget *pbox) {
  // didx:
  // 0 == rendered fx
  // 1 == pbox==NULL : standalone window for mapper
  // pbox != NULL: put params in box

  LiVESWidget *top_dialog_vbox=NULL;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *resetbutton=NULL;

  LiVESAccelGroup *fxw_accel_group;

  LiVESList *retvals=NULL;

  char *txt;

  boolean no_process=FALSE;

  boolean is_realtime=FALSE;
  boolean is_defaults=FALSE;

  //boolean has_lmap_error=FALSE;

  boolean has_param;

  int scrw,scrh;

  if (didx==0&&!check_storage_space((mainw->current_file>-1)?cfile:NULL,FALSE)) return;

  // TODO - remove this and check in rfx / realfx activate

  if (rfx->num_in_channels>0) {
    if (didx==0&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&
        (mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
      if (!do_layout_alter_frames_warning()) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);

        mainw->xlays=NULL;
      }
    }
  }

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (didx==1) {
    no_process=TRUE;
    is_realtime=TRUE;
  } else if (rfx->status!=RFX_STATUS_WEED) {
    retvals=do_onchange_init(rfx);
  }

  if (rfx->min_frames<0) no_process=TRUE;

  if (!no_process&&rfx->num_in_channels==0) {
    int new_file;
    mainw->pre_src_file=mainw->current_file;

    // create a new file to generate frames into
    if (!get_new_handle((new_file=mainw->first_free_file),NULL)) {

      if (mainw->multitrack!=NULL) {
        mt_sensitise(mainw->multitrack);
        mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
      }

      if (retvals!=NULL) {
        lives_list_free_strings(retvals);
        lives_list_free(retvals);
      }

      return;
    }

    mainw->is_generating=TRUE;
    mainw->current_file=new_file;

    // dummy values
    cfile->progress_start=1;

  }

  if (!no_process&&rfx->num_in_channels>0) {
    // check we have a real clip open
    if (mainw->current_file<=0) {
      if (retvals!=NULL) {
        lives_list_free_strings(retvals);
        lives_list_free(retvals);
      }
      return;
    }
    if (cfile->end-cfile->start+1<rfx->min_frames) {
      if (retvals!=NULL) {
        lives_list_free_strings(retvals);
        lives_list_free(retvals);
      }
      txt=lives_strdup_printf(_("\nYou must select at least %d frames to use this effect.\n\n"),rfx->min_frames);
      do_blocking_error_dialog(txt);
      lives_free(txt);
      return;
    }

    // here we invalidate cfile->ohsize, cfile->ovsize
    cfile->ohsize=cfile->hsize;
    cfile->ovsize=cfile->vsize;

    if (cfile->undo_action==UNDO_RESIZABLE) {
      set_undoable(NULL,FALSE);
    }
  }


  if (pbox==NULL) {

    if (prefs->gui_monitor!=0) {
      scrw=mainw->mgeom[prefs->gui_monitor-1].width;
      scrh=mainw->mgeom[prefs->gui_monitor-1].height;
    } else {
      scrw=mainw->scr_width;
      scrh=mainw->scr_height;
    }

    scrh-=SCR_HEIGHT_SAFETY;
    scrw-=SCR_WIDTH_SAFETY;

    if (rfx->status==RFX_STATUS_WEED||no_process||(rfx->num_in_channels==0&&rfx->props&RFX_PROPS_BATCHG)) scrw=RFX_WINSIZE_H;

    widget_opts.non_modal=TRUE;
    fx_dialog[didx] = lives_standard_dialog_new(_(rfx->menu_text),FALSE,scrw,RFX_WINSIZE_V);
    widget_opts.non_modal=FALSE;
  }

  if (rfx->status==RFX_STATUS_WEED&&rfx->is_template) is_defaults=TRUE;

  if (didx==0) {
    // activated from the menu for a rendered effect
    if (prefs->show_gui) {
      if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(fx_dialog[0]),LIVES_WINDOW(mainw->LiVES));
      else lives_window_set_transient_for(LIVES_WINDOW(fx_dialog[0]),LIVES_WINDOW(mainw->multitrack->window));
    }
  }

  if (pbox==NULL) {
    pbox = top_dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(fx_dialog[didx]));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(fx_dialog[didx]),"rfx",rfx);

    lives_widget_set_hexpand(pbox,TRUE);
    lives_widget_set_vexpand(pbox,TRUE);
  }


  if (rfx->status!=RFX_STATUS_WEED&&!no_process) {
    // rendered fx preview

    LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(top_dialog_vbox), hbox, TRUE, TRUE, 0);

    lives_widget_set_hexpand(hbox,TRUE);
    lives_widget_set_vexpand(hbox,TRUE);

    pbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox), pbox, TRUE, TRUE, 0);

    lives_widget_set_hexpand(pbox,TRUE);
    lives_widget_set_vexpand(pbox,TRUE);

    // add preview window
    if (rfx->num_in_channels>0) {
      mainw->framedraw_frame=cfile->start;
      widget_add_framedraw(LIVES_VBOX(pbox),cfile->start,cfile->end,!(rfx->props&RFX_PROPS_MAY_RESIZE),
                           cfile->hsize,cfile->vsize);
    } else {
      if (!(rfx->props&RFX_PROPS_BATCHG)) {
        mainw->framedraw_frame=0;
        widget_add_framedraw(LIVES_VBOX(pbox),1,1,TRUE,MAX_PRE_X,MAX_PRE_Y);
      }
    }

    if (!(rfx->props&RFX_PROPS_BATCHG)) {
      // connect spinbutton to preview
      fd_connect_spinbutton(rfx);
    }
  }

  has_param=make_param_box(LIVES_VBOX(pbox), rfx);

  // update widgets from onchange_init here

  if (top_dialog_vbox!=NULL) {

    cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL,NULL);

    fxw_accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
    lives_window_add_accel_group(LIVES_WINDOW(fx_dialog[didx]), fxw_accel_group);

    if (!no_process||is_defaults||rfx->status==RFX_STATUS_SCRAP) {

      lives_dialog_add_action_widget(LIVES_DIALOG(fx_dialog[didx]), cancelbutton, LIVES_RESPONSE_CANCEL);
      lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, fxw_accel_group,
                                   LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

      if (is_defaults) {
        okbutton = lives_button_new_from_stock(LIVES_STOCK_APPLY,_("Set as default"));
        if (!has_param) lives_widget_set_sensitive(okbutton,FALSE);
        resetbutton = lives_button_new_from_stock(LIVES_STOCK_REVERT_TO_SAVED,_("Reset"));
        if (!has_param) lives_widget_set_sensitive(resetbutton,FALSE);
        lives_dialog_add_action_widget(LIVES_DIALOG(fx_dialog[didx]), resetbutton, LIVES_RESPONSE_RESET);
      } else okbutton = lives_button_new_from_stock(LIVES_STOCK_OK,NULL);
      lives_dialog_add_action_widget(LIVES_DIALOG(fx_dialog[didx]), okbutton, LIVES_RESPONSE_OK);
    } else {
      okbutton = lives_button_new_from_stock(LIVES_STOCK_APPLY,_("Set as default"));
      if (!has_param) lives_widget_set_sensitive(okbutton,FALSE);
      cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CLOSE,_("_Close Window"));

      if (rfx->status!=RFX_STATUS_WEED&&no_process) {
        lives_widget_set_size_request(cancelbutton, DEF_BUTTON_WIDTH*4, -1);
      }
      if (rfx->status==RFX_STATUS_WEED) {
        resetbutton = lives_button_new_from_stock(LIVES_STOCK_REVERT_TO_SAVED,_("Reset"));
        lives_dialog_add_action_widget(LIVES_DIALOG(fx_dialog[didx]), resetbutton, LIVES_RESPONSE_RESET);
        lives_dialog_add_action_widget(LIVES_DIALOG(fx_dialog[didx]), okbutton, LIVES_RESPONSE_OK);
      }
      lives_dialog_add_action_widget(LIVES_DIALOG(fx_dialog[didx]), cancelbutton, LIVES_RESPONSE_CANCEL);
      lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, fxw_accel_group,
                                   LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

    }

    lives_widget_set_can_focus(cancelbutton,TRUE);

    if (lives_widget_get_parent(okbutton)!=NULL) {
      lives_widget_set_can_focus_and_default(okbutton);
      lives_widget_grab_default(okbutton);
    } else {
      lives_widget_set_can_focus_and_default(cancelbutton);
      lives_widget_grab_default(cancelbutton);
    }

    lives_widget_show_all(fx_dialog[didx]);

    if (no_process&&!is_defaults) {
      if (!is_realtime) {
        if (lives_widget_get_parent(okbutton)!=NULL)
          lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_paramwindow_cancel_clicked),
                               rfx);
        lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_paramwindow_cancel_clicked),
                             rfx);
        lives_signal_connect(LIVES_GUI_OBJECT(fx_dialog[didx]), LIVES_WIDGET_DELETE_EVENT,
                             LIVES_GUI_CALLBACK(on_paramwindow_cancel_clicked),
                             rfx);
      } else {
        lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_paramwindow_cancel_clicked2),
                             rfx);
        if (rfx->status==RFX_STATUS_SCRAP)
          lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_paramwindow_cancel_clicked2),
                               rfx);
        else {
          lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                               LIVES_GUI_CALLBACK(rte_set_key_defs),
                               rfx);
          if (resetbutton!=NULL) {
            lives_signal_connect_after(LIVES_GUI_OBJECT(resetbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                       LIVES_GUI_CALLBACK(rte_reset_defs_clicked),
                                       rfx);
          }
        }
        lives_signal_connect(LIVES_GUI_OBJECT(fx_dialog[didx]), LIVES_WIDGET_DELETE_EVENT,
                             LIVES_GUI_CALLBACK(on_paramwindow_cancel_clicked2),
                             rfx);
      }
    } else {
      if (!is_defaults) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_paramwindow_ok_clicked),
                             (livespointer)rfx);
        lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_paramwindow_cancel_clicked),
                             (livespointer)rfx);
        lives_signal_connect(LIVES_GUI_OBJECT(fx_dialog[didx]), LIVES_WIDGET_DELETE_EVENT,
                             LIVES_GUI_CALLBACK(on_paramwindow_cancel_clicked),
                             (livespointer)rfx);

      } else {
        lives_signal_connect_after(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                   LIVES_GUI_CALLBACK(rte_set_defs_ok),
                                   rfx);
        if (resetbutton!=NULL) {
          lives_signal_connect_after(LIVES_GUI_OBJECT(resetbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                     LIVES_GUI_CALLBACK(rte_reset_defs_clicked),
                                     rfx);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(resetbutton),"cancelbutton",(livespointer)cancelbutton);

        }
        lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(rte_set_defs_cancel),
                             rfx);
        lives_signal_connect(LIVES_GUI_OBJECT(fx_dialog[didx]), LIVES_WIDGET_DELETE_EVENT,
                             LIVES_GUI_CALLBACK(rte_set_defs_cancel),
                             rfx);
      }
    }
  }

  // tweak some things to do with framedraw preview
  if (mainw->framedraw!=NULL) fd_tweak(rfx);

  if (retvals!=NULL) {
    // now apply visually anything we got from onchange_init
    param_demarshall(rfx,retvals,TRUE,TRUE);
    lives_list_free_strings(retvals);
    lives_list_free(retvals);
  }


}


static void check_hidden_gui(weed_plant_t *inst, lives_param_t *param) {
  int error;
  weed_plant_t *wtmpl,*gui;

  if (param->reinit&&(weed_get_int_value(inst,"host_refs",&error)==2||
                      (mainw->multitrack!=NULL&&mainw->multitrack->fx_box!=NULL&&
                       mt_get_effect_time(mainw->multitrack)>0.))) {
    // effect is running and user is editing the params (or in multitrack at not at fx time 0.)
    param->hidden|=HIDDEN_NEEDS_REINIT;
  } else if (param->hidden&HIDDEN_NEEDS_REINIT) param->hidden^=HIDDEN_NEEDS_REINIT;

  if ((wtmpl=(weed_plant_t *)param->source)==NULL) return;

  if (!weed_plant_has_leaf(wtmpl,"gui")) return;

  gui=weed_get_plantptr_value(wtmpl,"gui",&error);

  if (weed_plant_has_leaf(gui,"hidden")) {
    int hidden=weed_get_boolean_value(gui,"hidden",&error);
    if (hidden==WEED_TRUE) param->hidden|=HIDDEN_GUI;
    else if (param->hidden&HIDDEN_GUI) param->hidden^=HIDDEN_GUI;
  }
}


static int num_in_params_for_nth_instance(weed_plant_t *inst, int idx) {
  // get number of params for nth instance in a compound effect - gives an offset for param number within the compound

  int error;
  while (--idx>0) inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
  return weed_leaf_num_elements(inst,"in_parameters");
}




boolean make_param_box(LiVESVBox *top_vbox, lives_rfx_t *rfx) {
  // make a dynamic parameter window

  // returns TRUE if we added any parameters
  lives_param_t *param=NULL;

  LiVESWidget *param_vbox=NULL;
  LiVESWidget *top_hbox=NULL;
  LiVESWidget *hbox=NULL;

  // put whole thing in scrolled window
  LiVESWidget *scrolledwindow;

  LiVESList *hints=NULL;
  LiVESList *onchange=NULL;
  LiVESList *layout=NULL;

  char **array;
  char label_text[256]; // max length of a label in layout hints

  char *line;
  char *type=NULL;

  boolean used[rfx->num_params];
  boolean has_box=FALSE;
  boolean internal=FALSE;
  boolean noslid;
  boolean has_param=FALSE;
  boolean chk_params=FALSE;
  boolean needs_sizes=FALSE;

  int pnum;
  int length;
  int poffset=0,inum=0;

  int num_tok;

  register int i,j,k;

  if (top_vbox==NULL) {
    // just check how many non-hidden params without displaying
    chk_params=TRUE;
  } else {
    mainw->textwidget_focus=NULL;

    // initialise special widgets
    init_special();

    if (rfx->status==RFX_STATUS_WEED) usrgrp_to_livesgrp[1]=NULL;
    else usrgrp_to_livesgrp[0]=NULL;

    // paramwindow start, everything goes in top_hbox

    top_hbox = lives_hbox_new(FALSE, widget_opts.packing_width);

    // param_vbox holds the dynamic parameters
    param_vbox = lives_vbox_new(FALSE, widget_opts.packing_height);

    lives_box_pack_start(LIVES_BOX(top_hbox), param_vbox, TRUE, TRUE, widget_opts.packing_width);
    lives_box_set_spacing(LIVES_BOX(param_vbox), widget_opts.packing_height/2);
  }

  switch (rfx->status) {
  case RFX_STATUS_BUILTIN:
    type=lives_strdup(PLUGIN_RENDERED_EFFECTS_BUILTIN);
    break;
  case RFX_STATUS_CUSTOM:
    type=lives_strdup(PLUGIN_RENDERED_EFFECTS_CUSTOM);
    break;
  case RFX_STATUS_SCRAP:
    type=lives_strdup(PLUGIN_RFX_SCRAP);
    break;
  case RFX_STATUS_WEED:
    internal=TRUE;
    break;
  default:
    type=lives_strdup(PLUGIN_RENDERED_EFFECTS_TEST);
    break;
  }

  // extras for multitrack
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED&&!chk_params) {
    weed_plant_t *filter=weed_instance_get_filter((weed_plant_t *)rfx->source,TRUE);
    if (enabled_in_channels(filter,FALSE)==2&&get_transition_param(filter,FALSE)!=-1) {
      // add in/out for multitrack transition
      transition_add_in_out(LIVES_BOX(param_vbox),rfx,(mainw->multitrack->opts.pertrack_audio));
      //trans_in_out_pressed(rfx,TRUE);
    }
  }

  // extras for converters
  if (internal&&weed_instance_is_resizer((weed_plant_t *)rfx->source)&&!chk_params) {
    has_param=add_sizes(LIVES_BOX(param_vbox),FALSE,has_param,rfx);
  }

  if (rfx->status!=RFX_STATUS_SCRAP&&!internal&&rfx->num_in_channels==0&&rfx->min_frames>-1&&!chk_params) {
    if (mainw->multitrack==NULL) add_gen_to(LIVES_BOX(param_vbox),rfx);
    else mainw->gen_to_clipboard=FALSE;
  }

  if (!internal&&!chk_params) {
    // do onchange|init
    if ((onchange=plugin_request_by_line(type,rfx->name,"get_onchange"))!=NULL) {
      for (i=0; i<lives_list_length(onchange); i++) {
        array=lives_strsplit((char *)lives_list_nth_data(onchange,i),rfx->delim,-1);
        if (strcmp(array[0],"init")) {
          // note other onchanges so we don't have to keep parsing the list
          int which=atoi(array[0]);
          if (which>=0&&which<rfx->num_params) {
            rfx->params[which].onchange=TRUE;
          }
        }
        lives_strfreev(array);
      }
      lives_list_free_strings(onchange);
      lives_list_free(onchange);
    }
    hints=plugin_request_by_line(type,rfx->name,"get_param_window");
    lives_free(type);
  } else if (!chk_params) hints=get_external_window_hints(rfx);

  // do param window hints
  if (hints!=NULL) {
    char *lstring=lives_strconcat("layout",rfx->delim,NULL);
    char *sstring=lives_strconcat("special",rfx->delim,NULL);
    char *istring=lives_strconcat("internal",rfx->delim,NULL);
    for (i=0; i<lives_list_length(hints); i++) {
      if (!strncmp((char *)lives_list_nth_data(hints,i),lstring,7)) {
        layout=lives_list_append(layout,lives_strdup((char *)lives_list_nth_data(hints,i)+7));
      } else if (!strncmp((char *)lives_list_nth_data(hints,i),istring,9)) {
        layout=lives_list_append(layout,lives_strdup((char *)lives_list_nth_data(hints,i)+9));
      } else if (!strncmp((char *)lives_list_nth_data(hints,i),sstring,8)) {
        add_to_special((char *)lives_list_nth_data(hints,i)+8,rfx);
      }
    }
    lives_list_free_strings(hints);
    lives_list_free(hints);   // no longer needed
    lives_free(lstring);
    lives_free(sstring);
  }

  for (i=0; i<rfx->num_params; i++) {
    used[i]=FALSE;
    for (j=0; j<MAX_PARAM_WIDGETS; rfx->params[i].widgets[j++]=NULL);
  }

  mainw->block_param_updates=TRUE; // block framedraw updates until all parameter widgets have been created

  // use layout hints to build as much as we can
  for (i=0; i<lives_list_length(layout); i++) {
    has_box=FALSE;
    noslid=FALSE;
    line=(char *)lives_list_nth_data(layout,i);
    num_tok=get_token_count(line,(unsigned int)rfx->delim[0]);
    // ignore | inside strings
    array=lives_strsplit(line,rfx->delim,num_tok);
    if (!strlen(array[num_tok-1])) num_tok--;
    for (j=0; j<num_tok; j++) {
      if (!strcmp(array[j],"nextfilter")) {
        // handling for compound fx - add an offset to the param number
        poffset+=num_in_params_for_nth_instance((weed_plant_t *)rfx->source,inum);
        inum++;
        continue;
      }

      if (!strncmp(array[j],"p",1)&&(pnum=atoi((char *)(array[j]+1)))>=0&&(pnum=pnum+poffset)<rfx->num_params&&!used[pnum]) {
        param=&rfx->params[pnum];
        if (rfx->source_type==LIVES_RFX_SOURCE_WEED) check_hidden_gui((weed_plant_t *)rfx->source,param);
        if ((param->hidden&&param->hidden!=HIDDEN_NEEDS_REINIT)||
            param->type==LIVES_PARAM_UNDISPLAYABLE) continue;
        // parameter, eg. p1
        if (!has_box) {
          hbox = lives_hbox_new(FALSE, 0);
          lives_box_pack_start(LIVES_BOX(param_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
          has_box=TRUE;
          has_param=TRUE;
        }
        if (add_param_to_box(LIVES_BOX(hbox),rfx,pnum,(j==(num_tok-1))&&!noslid)) noslid=TRUE;
        used[pnum]=TRUE;
        has_param=TRUE;
      } else if (!j&&!strcmp(array[j],"hseparator")&&has_param) {
        add_hsep_to_box(LIVES_BOX(param_vbox));
        j=num_tok;  // ignore anything after hseparator
      } else if (!strncmp(array[j],"fill",4)) {
        // can be filln
        if (strlen(array[j])==4||!(length=atoi(array[j]+4))) length=1;
        if (!has_box) {
          hbox = lives_hbox_new(FALSE, 0);
          lives_box_pack_start(LIVES_BOX(param_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
          has_box=TRUE;
        }
        for (k=0; k<length; k++) {
          add_fill_to_box(LIVES_BOX(hbox));
        }
      } else if (!strncmp(array[j],"\"",1)) {
        // label
        if (!has_box) {
          hbox = lives_hbox_new(FALSE, 0);
          lives_box_pack_start(LIVES_BOX(param_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
          has_box=TRUE;
        }
        lives_snprintf(label_text,256,"%s",array[j]+1);
        while (strcmp(array[j]+strlen(array[j])-1,"\"")&&j<num_tok-1) {
          lives_strappend(label_text,256,array[++j]);
        }
        if (strlen(label_text)>1) {
          if (!strcmp(label_text+strlen(label_text)-1,"\"")) {
            memset(label_text+strlen(label_text)-1,0,1);
          }
          add_param_label_to_box(LIVES_BOX(hbox),TRUE,label_text);
        }
      }
    }
    lives_strfreev(array);
  }
  if (layout!=NULL) {
    lives_list_free_strings(layout);
    lives_list_free(layout);
  }

  // add any unused parameters
  for (i=0; i<rfx->num_params; i++) {
    rfx->params[i].changed=FALSE;
    if (rfx->source_type==LIVES_RFX_SOURCE_WEED) check_hidden_gui((weed_plant_t *)rfx->source,&rfx->params[i]);
    if ((rfx->params[i].hidden&&rfx->params[i].hidden!=HIDDEN_NEEDS_REINIT)||
        rfx->params[i].type==LIVES_PARAM_UNDISPLAYABLE) continue;
    if (!used[i]) {
      if (!chk_params) {
        add_param_to_box(LIVES_BOX(param_vbox),rfx,i,TRUE);
      }
      has_param=TRUE;
    }
  }



  if (mainw->multitrack==NULL&&rfx->status==RFX_STATUS_WEED&&rfx->is_template) {
    weed_plant_t *filter=weed_instance_get_filter((weed_plant_t *)rfx->source,TRUE);
    if (enabled_in_channels(filter,FALSE)==0&&enabled_out_channels(filter,FALSE)>0&&has_video_chans_out(filter,TRUE)) {
      // out channel size(s) and target_fps for generators
      if (!chk_params) {
        needs_sizes=TRUE;
        if (mainw->overflow_height<900) mainw->overflow_height=900;
      }
      chk_params=TRUE;
    }
  }


  if (!chk_params) {
    if (!has_param) {
      hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(param_vbox), hbox, FALSE, FALSE, widget_opts.packing_height*2);
      add_fill_to_box(LIVES_BOX(hbox));
      add_param_label_to_box(LIVES_BOX(hbox),FALSE,_("No parameters"));
      add_fill_to_box(LIVES_BOX(hbox));
    }

    if (mainw->multitrack==NULL||rfx->status!=RFX_STATUS_WEED) {
      if (mainw->scr_height>=mainw->overflow_height)
        scrolledwindow=lives_standard_scrolled_window_new(RFX_WINSIZE_H,RFX_WINSIZE_V,top_hbox);
      else
        scrolledwindow=lives_standard_scrolled_window_new(RFX_WINSIZE_H,RFX_WINSIZE_V>>1,top_hbox);
    } else
      scrolledwindow=lives_standard_scrolled_window_new(-1,-1,top_hbox);

    lives_box_pack_start(LIVES_BOX(top_vbox), scrolledwindow, TRUE, TRUE, 0);

  }

  if (needs_sizes) {
    add_sizes(LIVES_BOX(top_vbox),TRUE,has_param,rfx);
    has_param=TRUE;
  }

  mainw->block_param_updates=FALSE;
  mainw->overflow_height=0;

  return has_param;
}




boolean add_param_to_box(LiVESBox *box, lives_rfx_t *rfx, int pnum, boolean add_slider) {
  // box here is vbox inside top_hbox inside top_dialog

  // add paramter pnum for rfx to box

  LiVESWidget *label;
  LiVESWidget *checkbutton;
  LiVESWidget *radiobutton;
  LiVESWidget *spinbutton;
  LiVESWidget *scale=NULL;
#ifdef ENABLE_GIW
  LiVESWidget *scale2;
#endif
  LiVESWidget *spinbutton_red;
  LiVESWidget *spinbutton_green;
  LiVESWidget *spinbutton_blue;
  LiVESWidget *cbutton;
  LiVESWidget *entry=NULL;
  LiVESWidget *hbox;
  LiVESWidget *combo;
  LiVESWidget *dlabel=NULL;
  LiVESWidget *textview=NULL;
  LiVESWidget *scrolledwindow;

  LiVESAdjustment *spinbutton_adj;

  LiVESTextBuffer *textbuffer=NULL;

  lives_param_t *param;
  lives_widget_group_t *group;
  LiVESSList *rbgroup;

  lives_colRGB48_t rgb;
  lives_colRGBA64_t rgba;

  char *name;
  char *txt,*tmp;
  char *disp_string;

  boolean use_mnemonic;
  boolean was_num=FALSE;

  boolean add_scalers=TRUE;

  if (pnum>=rfx->num_params) {
    add_param_label_to_box(box,FALSE,(_("Invalid parameter")));
    return FALSE;
  }

  param=&rfx->params[pnum];

  name=lives_strdup_printf("%s",param->label);
  use_mnemonic=param->use_mnemonic;

  // reinit can cause the window to be redrawn, which invalidates the slider adjustment...and bang !
  // so dont add sliders for such params
  if (param->reinit) add_scalers=FALSE;

  if (LIVES_IS_HBOX(LIVES_WIDGET(box))) {
    hbox=LIVES_WIDGET(box);
  } else {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, widget_opts.packing_height);
  }

  switch (param->type) {
  case LIVES_PARAM_BOOL :
    if (!param->group) {

      if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
        dlabel=lives_standard_label_new((tmp=lives_strdup_printf("(%s)",_(disp_string))));
        lives_free(tmp);
        lives_free(disp_string);
        lives_box_pack_start(LIVES_BOX(hbox), dlabel, FALSE, FALSE, widget_opts.packing_width);
        param->widgets[1]=dlabel;
      }


      checkbutton=lives_standard_check_button_new(name,use_mnemonic,(LiVESBox *)hbox,param->desc);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), get_bool_param(param->value));
      lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                 LIVES_GUI_CALLBACK(after_boolean_param_toggled),
                                 (livespointer)rfx);

      // store parameter so we know whose trigger to use
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(checkbutton),"param_number",LIVES_INT_TO_POINTER(pnum));
      param->widgets[0]=checkbutton;
      if (param->hidden) lives_widget_set_sensitive(checkbutton,FALSE);
    } else {
      group=get_group(rfx,param);

      if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
        dlabel=lives_standard_label_new((tmp=lives_strdup_printf("(%s)",_(disp_string))));
        lives_free(tmp);
        lives_free(disp_string);
        lives_box_pack_start(LIVES_BOX(hbox), dlabel, FALSE, FALSE, widget_opts.packing_width);
        param->widgets[1]=dlabel;
      }

      if (group!=NULL) rbgroup=group->rbgroup;
      else rbgroup=NULL;

      radiobutton=lives_standard_radio_button_new(name,use_mnemonic,rbgroup,LIVES_BOX(hbox),param->desc);

      if (group==NULL) {
        if (rfx->status==RFX_STATUS_WEED) {
          usrgrp_to_livesgrp[1]=add_usrgrp_to_livesgrp(usrgrp_to_livesgrp[1],
                                lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton)),
                                param->group);
        } else {
          usrgrp_to_livesgrp[0]=add_usrgrp_to_livesgrp(usrgrp_to_livesgrp[0],
                                lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton)),
                                param->group);
        }
      }

      group=get_group(rfx,param);

      if (group!=NULL) {
        group->rbgroup=lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton));
        if (get_bool_param(param->value)) {
          group->active_param=pnum+1;
        }
      } else LIVES_WARN("Button group was NULL");

      lives_signal_connect_after(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                 LIVES_GUI_CALLBACK(after_boolean_param_toggled),
                                 (livespointer)rfx);

      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), get_bool_param(param->value));

      // store parameter so we know whose trigger to use
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(radiobutton),"param_number",LIVES_INT_TO_POINTER(pnum));
      param->widgets[0]=radiobutton;
      if (param->hidden) lives_widget_set_sensitive(radiobutton,FALSE);
    }
    break;
  case LIVES_PARAM_NUM :
    was_num=TRUE;

    if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
      dlabel=lives_standard_label_new((tmp=lives_strdup_printf("%s",_(disp_string))));
      lives_free(tmp);
      lives_free(disp_string);
      lives_box_pack_start(LIVES_BOX(hbox), dlabel, FALSE, FALSE, widget_opts.packing_width);
      param->widgets[1]=dlabel;
    }

    if (param->dp) {
      spinbutton=lives_standard_spin_button_new(name, use_mnemonic, get_double_param(param->value), param->min,
                 param->max, param->step_size, param->step_size, param->dp,
                 (LiVESBox *)hbox, param->desc);
    } else {
      spinbutton=lives_standard_spin_button_new(name, use_mnemonic, (double)get_int_param(param->value), param->min,
                 param->max, param->step_size, param->step_size, param->dp,
                 (LiVESBox *)hbox, param->desc);
    }


    lives_spin_button_set_wrap(LIVES_SPIN_BUTTON(spinbutton),param->wrap);

    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(after_param_value_changed),
                               (livespointer)rfx);

    // store parameter so we know whose trigger to use
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton),"param_number",LIVES_INT_TO_POINTER(pnum));
    param->widgets[0]=spinbutton;
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(param->widgets[0]),"rfx",rfx);
    if (param->hidden) lives_widget_set_sensitive(spinbutton,FALSE);


    if (add_scalers) {
      spinbutton_adj=lives_spin_button_get_adjustment(LIVES_SPIN_BUTTON(spinbutton));
#ifdef ENABLE_GIW
      if (!prefs->lamp_buttons) {
#endif
        if (add_slider) {
          scale=lives_hscale_new(LIVES_ADJUSTMENT(spinbutton_adj));
          lives_scale_set_draw_value(LIVES_SCALE(scale),FALSE);
          lives_box_pack_start(LIVES_BOX(hbox), scale, TRUE, TRUE, 0);
        }
#ifdef ENABLE_GIW
      } else {
        scale=giw_knob_new(LIVES_ADJUSTMENT(spinbutton_adj));
        lives_widget_set_size_request(scale,GIW_KNOB_WIDTH,GIW_KNOB_HEIGHT);
        giw_knob_set_legends_digits(GIW_KNOB(scale),0);
        lives_box_pack_start(LIVES_BOX(hbox), scale, FALSE, FALSE, 0);
        add_fill_to_box(LIVES_BOX(hbox));
        lives_widget_set_fg_color(scale,LIVES_WIDGET_STATE_NORMAL,&palette->black);
        lives_widget_set_fg_color(scale,LIVES_WIDGET_STATE_PRELIGHT,&palette->dark_orange);
        if (add_slider) {
          scale2=lives_hscale_new(LIVES_ADJUSTMENT(spinbutton_adj));
          lives_scale_set_draw_value(LIVES_SCALE(scale2),FALSE);
          lives_box_pack_start(LIVES_BOX(hbox), scale2, TRUE, TRUE, 0);
          if (!LIVES_IS_HBOX(LIVES_WIDGET(box))) add_fill_to_box(LIVES_BOX(hbox));

          if (palette->style&STYLE_1) {
            lives_widget_set_bg_color(scale2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
            lives_widget_set_text_color(scale2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
            lives_widget_set_fg_color(scale2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
          }
          if (param->desc!=NULL) lives_widget_set_tooltip_text(scale2, param->desc);
        }
      }
#endif
      if (palette->style&STYLE_1&&scale!=NULL) {
        lives_widget_set_bg_color(scale, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
        lives_widget_set_text_color(scale, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
        lives_widget_set_fg_color(scale, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
      }

      if (param->desc!=NULL) lives_widget_set_tooltip_text(scale, param->desc);
    }
    break;

  case LIVES_PARAM_COLRGB24 :
    get_colRGB24_param(param->value,&rgb);

    rgba.red=rgb.red<<8;
    rgba.green=rgb.green<<8;
    rgba.blue=rgb.blue<<8;
    rgba.alpha=65535;

    cbutton = lives_standard_color_button_new(LIVES_BOX(hbox),_(name),use_mnemonic,FALSE,&rgba,&spinbutton_red,&spinbutton_green,
              &spinbutton_blue,NULL);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton),"param_number",LIVES_INT_TO_POINTER(pnum));
    if (param->desc!=NULL) lives_widget_set_tooltip_text(cbutton, param->desc);

    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(after_param_red_changed),
                               (livespointer)rfx);
    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(after_param_green_changed),
                               (livespointer)rfx);
    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(after_param_blue_changed),
                               (livespointer)rfx);

    lives_signal_connect_after(LIVES_GUI_OBJECT(cbutton), LIVES_WIDGET_COLOR_SET_SIGNAL,
                               LIVES_GUI_CALLBACK(on_pwcolsel),
                               (livespointer)rfx);

    // store parameter so we know whose trigger to use
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_red),"param_number",LIVES_INT_TO_POINTER(pnum));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_green),"param_number",LIVES_INT_TO_POINTER(pnum));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_blue),"param_number",LIVES_INT_TO_POINTER(pnum));

    param->widgets[0]=spinbutton_red;
    param->widgets[1]=spinbutton_green;
    param->widgets[2]=spinbutton_blue;
    //param->widgets[3]=spinbutton_alpha;
    param->widgets[4]=cbutton;

    if (param->hidden) {
      lives_widget_set_sensitive(spinbutton_red,FALSE);
      lives_widget_set_sensitive(spinbutton_green,FALSE);
      lives_widget_set_sensitive(spinbutton_blue,FALSE);
      //lives_widget_set_sensitive(spinbutton_alpha,FALSE);
      lives_widget_set_sensitive(cbutton,FALSE);
    }


    break;

  case LIVES_PARAM_STRING:

    if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
      if (param->max==0.) txt=lives_strdup(disp_string);
      else txt=lives_strndup(disp_string,(int)param->max);
      lives_free(disp_string);
    } else {
      if (param->max==0.) txt=lives_strdup((char *)param->value);
      else txt=lives_strndup((char *)param->value,(int)param->max);
    }



    if (((int)param->max>RFX_TEXT_MAGIC||param->max==0.)&&
        param->special_type!=LIVES_PARAM_SPECIAL_TYPE_FILEREAD) {
      LiVESWidget *vbox;

      boolean woat;

      widget_opts.justify=LIVES_JUSTIFY_CENTER;
      if (use_mnemonic) label = lives_standard_label_new_with_mnemonic(_(name),NULL);
      else label = lives_standard_label_new(_(name));
      widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

      vbox=lives_vbox_new(FALSE,0);
      lives_box_pack_start(LIVES_BOX(hbox), vbox, TRUE, TRUE, widget_opts.packing_width);
      lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height>>1);

      hbox=lives_hbox_new(FALSE,0);
      lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height>>1);

      param->widgets[0] = textview = lives_text_view_new();
      if (param->desc!=NULL) lives_widget_set_tooltip_text(textview, param->desc);
      textbuffer=lives_text_view_get_buffer(LIVES_TEXT_VIEW(textview));

      lives_signal_connect_after(LIVES_WIDGET_OBJECT(textbuffer), LIVES_WIDGET_CHANGED_SIGNAL,
                                 LIVES_GUI_CALLBACK(after_param_text_buffer_changed),
                                 (livespointer) rfx);

      lives_text_view_set_editable(LIVES_TEXT_VIEW(textview), TRUE);
      lives_text_view_set_wrap_mode(LIVES_TEXT_VIEW(textview), LIVES_WRAP_WORD);
      lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(textview), TRUE);

      lives_text_buffer_set_text(textbuffer, txt, -1);

      woat=widget_opts.apply_theme;
      widget_opts.apply_theme=FALSE;
      widget_opts.expand=LIVES_EXPAND_NONE;
      scrolledwindow = lives_standard_scrolled_window_new(-1, RFX_TEXT_SCROLL_HEIGHT, textview);
      widget_opts.expand=LIVES_EXPAND_DEFAULT;
      widget_opts.apply_theme=woat;

      if (palette->style&STYLE_1) {
        lives_widget_set_base_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->white);
        lives_widget_set_text_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->black);
      }

      lives_box_pack_start(LIVES_BOX(hbox), scrolledwindow, TRUE, TRUE, 0);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(textbuffer),"textview",textview);

    } else {
      if (use_mnemonic) label = lives_standard_label_new_with_mnemonic(_(name),NULL);
      else label = lives_standard_label_new(_(name));

      lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
      param->widgets[0]=entry=lives_standard_entry_new(NULL,FALSE,txt,(int)param->max,(int)param->max,LIVES_BOX(hbox),param->desc);

      if (rfx->status==RFX_STATUS_WEED&&param->special_type!=LIVES_PARAM_SPECIAL_TYPE_FILEREAD) {
        lives_signal_connect_after(LIVES_WIDGET_OBJECT(entry), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(after_param_text_changed),
                                   (livespointer) rfx);
      }

    }

    if (param->desc!=NULL) lives_widget_set_tooltip_text(label, param->desc);

    lives_signal_connect_after(LIVES_WIDGET_OBJECT(hbox), LIVES_WIDGET_SET_FOCUS_CHILD_SIGNAL,
                               LIVES_GUI_CALLBACK(after_param_text_focus_changed),
                               (livespointer) rfx);

    if (param->hidden) lives_widget_set_sensitive(param->widgets[0],FALSE);
    if (use_mnemonic) lives_label_set_mnemonic_widget(LIVES_LABEL(label),param->widgets[0]);

    lives_free(txt);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox),"textwidget",(livespointer)param->widgets[0]);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(param->widgets[0]),"param_number",LIVES_INT_TO_POINTER(pnum));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(param->widgets[0]),"rfx",rfx);

    param->widgets[1]=label;

    break;

  case LIVES_PARAM_STRING_LIST:

    widget_opts.expand=LIVES_EXPAND_EXTRA;
    combo=lives_standard_combo_new(name, use_mnemonic, param->list, (LiVESBox *)hbox, param->desc);
    widget_opts.expand=LIVES_EXPAND_DEFAULT;

    if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
      lives_combo_set_active_string(LIVES_COMBO(combo),disp_string);
      lives_free(disp_string);
    } else if (param->list!=NULL) {
      lives_combo_set_active_string(LIVES_COMBO(combo),
                                    (char *)lives_list_nth_data(param->list,get_int_param(param->value)));
    }

    lives_signal_connect_after(LIVES_WIDGET_OBJECT(combo), LIVES_WIDGET_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(after_string_list_changed), (livespointer) rfx);

    // store parameter so we know whose trigger to use
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo),"param_number",LIVES_INT_TO_POINTER(pnum));
    param->widgets[0]=combo;
    if (param->hidden) lives_widget_set_sensitive(combo,FALSE);
    break;

  default:
    break;

  }


  // see if there were any 'special' hints
  check_for_special(rfx,param,LIVES_BOX(lives_widget_get_parent(LIVES_WIDGET(box))));

  lives_free(name);
  return was_num;
}


void add_param_label_to_box(LiVESBox *box, boolean do_trans, const char *text) {
  LiVESWidget *label;

  lives_box_set_homogeneous(LIVES_BOX(box),FALSE);

  if (do_trans) {
    char *markup;
#ifdef GUI_GTK
    markup=g_markup_printf_escaped("<span weight=\"bold\" style=\"italic\"> %s </span>",_(text));
#endif
#ifdef GUI_QT
    QString qs = QString("<span weight=\"bold\" style=\"italic\"> %s </span>").arg(_(text));
    markup=strdup((const char *)qs.toHtmlEscaped().constData());
#endif
    label = lives_standard_label_new(NULL);
    lives_label_set_markup_with_mnemonic(LIVES_LABEL(label),markup);
    lives_free(markup);
  } else label = lives_standard_label_new_with_mnemonic(text,NULL);

  if (LIVES_IS_HBOX(LIVES_WIDGET(box)))
    lives_box_pack_start(box, label, FALSE, FALSE, widget_opts.packing_width);
  else
    lives_box_pack_start(box, label, FALSE, FALSE, widget_opts.packing_height);
  lives_widget_show(label);
}

LiVESSList *add_usrgrp_to_livesgrp(LiVESSList *u2l, LiVESSList *rbgroup, int usr_number) {
  lives_widget_group_t *wgroup=(lives_widget_group_t *)lives_malloc(sizeof(lives_widget_group_t));
  wgroup->usr_number=usr_number;
  wgroup->rbgroup=rbgroup;
  wgroup->active_param=0;
  u2l=lives_slist_append(u2l, (livespointer)wgroup);
  return u2l;
}




lives_widget_group_t *livesgrp_from_usrgrp(LiVESSList *u2l, int usrgrp) {
  int i;
  lives_widget_group_t *group;

  for (i=0; i<lives_slist_length(u2l); i++) {
    group=(lives_widget_group_t *)lives_slist_nth_data(u2l,i);
    if (group->usr_number==usrgrp) return group;
  }
  return NULL;
}








void after_boolean_param_toggled(LiVESToggleButton *togglebutton, lives_rfx_t *rfx) {
  int param_number=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(togglebutton),"param_number"));

  LiVESList *retvals=NULL;

  lives_param_t *param=&rfx->params[param_number];

  boolean old_bool=get_bool_param(param->value),new_bool;
  boolean was_reinited=FALSE;

  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready
  new_bool=lives_toggle_button_get_active(togglebutton);

  if (old_bool==new_bool) return;

  set_bool_param(param->value,new_bool);

  if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;
    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      char *disp_string;
      weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE,FALSE);
      int index=0,numvals;
      int key=-1;
      int *valis;

      if (mainw->multitrack!=NULL&&is_perchannel_multi(rfx,param_number)) {
        index=mainw->multitrack->track_index;
      }
      numvals=weed_leaf_num_elements(wparam,"value");

      if (index>=numvals) {
        weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
        fill_param_vals_to(wparam,paramtmpl,index);
        numvals=index+1;
      }

      valis=weed_get_boolean_array(wparam,"value",&error);
      valis[index]=new_bool;
      if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);
      filter_mutex_lock(key);
      weed_set_boolean_array(wparam,"value",numvals,valis);
      filter_mutex_unlock(key);
      copyto=set_copy_to(inst,param_number,TRUE);

      lives_free(valis);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst,param_number);
        if (copyto!=-1) rec_param_change(inst,copyto);
      }

      disp_string=get_weed_display_string(inst,param_number);
      if (disp_string!=NULL) {
        lives_label_set_text(LIVES_LABEL(param->widgets[1]),disp_string);
        lives_free(disp_string);
      }
      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
        weed_reinit_effect(inst,FALSE);
        was_reinited=TRUE;
      }

    }


  }
  if (get_bool_param(param->value)!=old_bool&&param->onchange) {
    param->change_blocked=TRUE;
    retvals=do_onchange(LIVES_WIDGET_OBJECT(togglebutton), rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
    lives_widget_context_update();
    param->change_blocked=FALSE;
  }
  if (!was_reinited&&copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}



void after_param_value_changed(LiVESSpinButton *spinbutton, lives_rfx_t *rfx) {
  int param_number=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton),"param_number"));
  lives_param_t *param=&rfx->params[param_number];

  LiVESList *retvals=NULL;

  double new_double=0.,old_double=0.;

  boolean was_reinited=FALSE;

  int new_int=0,old_int=0;
  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this (pre)change to our event_list
    // however, we need to use the actual instance and not the one generated for the rte_window
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }

  if (param->dp>0) {
    old_double=get_double_param(param->value);
    new_double=lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton));
    if (old_double==new_double) return;
    set_double_param(param->value,new_double);
  } else {
    old_int=get_int_param(param->value);
    new_int=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    if (old_int==new_int) return;
    set_int_param(param->value,new_int);
  }

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;
    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      char *disp_string;

      weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE,FALSE);
      int index=0,numvals;
      int key=-1;
      double *valds;
      int *valis;

      if (mainw->multitrack!=NULL&&is_perchannel_multi(rfx,param_number)) {
        index=mainw->multitrack->track_index;
      }
      numvals=weed_leaf_num_elements(wparam,"value");
      if (index>=numvals) {
        weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
        fill_param_vals_to(wparam,paramtmpl,index);
        numvals=index+1;
      }

      if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);

      if (weed_leaf_seed_type(wparam,"value")==WEED_SEED_DOUBLE) {
        valds=weed_get_double_array(wparam,"value",&error);
        if (param->dp>0) valds[index]=new_double;
        else valds[index]=(double)new_int;
        filter_mutex_lock(key);
        weed_set_double_array(wparam,"value",numvals,valds);
        filter_mutex_unlock(key);
        copyto=set_copy_to(inst,param_number,TRUE);
        lives_free(valds);
      } else {
        valis=weed_get_int_array(wparam,"value",&error);
        valis[index]=new_int;
        filter_mutex_lock(key);
        weed_set_int_array(wparam,"value",numvals,valis);
        filter_mutex_unlock(key);
        copyto=set_copy_to(inst,param_number,TRUE);
        lives_free(valis);
      }

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst,param_number);
        if (copyto!=-1) rec_param_change(inst,copyto);
      }

      disp_string=get_weed_display_string(inst,param_number);
      if (disp_string!=NULL) {
        lives_label_set_text(LIVES_LABEL(param->widgets[1]),disp_string);
        lives_free(disp_string);
      }
      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
        weed_reinit_effect(inst,FALSE);
        was_reinited=TRUE;
      }

    }
  }

  if (((param->dp>0&&(get_double_param(param->value)!=old_double))||(param->dp==0&&
       (get_int_param(param->value)!=old_int)))&&
      param->onchange) {
    param->change_blocked=TRUE;
    retvals=do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
    lives_widget_context_update();
    param->change_blocked=FALSE;
  }
  if (!was_reinited&&copyto!=-1) update_visual_params(rfx,FALSE);

  if (fx_dialog[1]!=NULL) {
    // transfer param changes from rte_window to ce_thumbs window, and vice-versa
    lives_rfx_t *rte_rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"rfx");
    int key=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"key"));
    int mode=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fx_dialog[1]),"mode"));
    mainw->block_param_updates=TRUE;
    if (rfx==rte_rfx&&mainw->ce_thumbs) ce_thumbs_update_visual_params(key);
    else if (mode==rte_key_getmode(key+1)) ce_thumbs_check_for_rte(rfx,rte_rfx,key);
    mainw->block_param_updates=FALSE;
  }

  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    if (was_reinited) add_mt_param_box(mainw->multitrack);
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


void update_weed_color_value(weed_plant_t *plant, int pnum, int c1, int c2, int c3, int c4) {
  weed_plant_t *ptmpl;
  weed_plant_t *param=NULL;

  int *maxs=NULL,*mins=NULL;
  int cols[4]= {c1,c2,c3,c4};
  int cspace;
  int rmax,rmin,gmax,gmin,bmax,bmin;
  int error;

  boolean is_default=WEED_PLANT_IS_FILTER_CLASS(plant);
  boolean is_int;

  double *maxds=NULL,*minds=NULL;
  double colds[4];
  double rmaxd,rmind,gmaxd,gmind,bmaxd,bmind;

  if (!is_default) {
    param=weed_inst_in_param(plant,pnum,FALSE,FALSE);
    ptmpl=weed_get_plantptr_value(param,"template",&error);
  } else {
    // called only from rte_set_defs_ok
    ptmpl=weed_filter_in_paramtmpl(plant,pnum,FALSE);
  }

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  is_int=(weed_leaf_seed_type(ptmpl,"default")==WEED_SEED_INT);
  cspace=weed_get_int_value(ptmpl,"colorspace",&error);

  switch (cspace) {
  // TODO - other cspaces
  case WEED_COLORSPACE_RGB:
    if (is_int) {
      if (weed_leaf_num_elements(ptmpl,"max")==3) {
        maxs=weed_get_int_array(ptmpl,"max",&error);
        rmax=maxs[0];
        gmax=maxs[1];
        bmax=maxs[2];
        lives_free(maxs);
      } else rmax=gmax=bmax=weed_get_int_value(ptmpl,"max",&error);
      if (weed_leaf_num_elements(ptmpl,"min")==3) {
        mins=weed_get_int_array(ptmpl,"min",&error);
        rmin=mins[0];
        gmin=mins[1];
        bmin=mins[2];
        lives_free(mins);
      } else rmin=gmin=bmin=weed_get_int_value(ptmpl,"min",&error);

      cols[0]=rmin+(int)((double)cols[0]/255.*(double)(rmax-rmin));
      cols[1]=gmin+(int)((double)cols[1]/255.*(double)(gmax-gmin));
      cols[2]=bmin+(int)((double)cols[2]/255.*(double)(bmax-bmin));
      if (is_default) {
        weed_set_int_array(ptmpl,"host_default",3,cols);
      } else {
        int index=0,numvals;
        int *valis;

        if (mainw->multitrack!=NULL&&is_perchannel_multiw(ptmpl)) {
          index=mainw->multitrack->track_index;
        }
        numvals=weed_leaf_num_elements(param,"value");
        if (index*3>=numvals) {
          weed_plant_t *paramtmpl=weed_get_plantptr_value(param,"template",&error);
          fill_param_vals_to(param,paramtmpl,index);
          numvals=(index+1)*3;
        }

        valis=weed_get_int_array(param,"value",&error);
        valis[index*3]=cols[0];
        valis[index*3+1]=cols[1];
        valis[index*3+2]=cols[2];
        weed_set_int_array(param,"value",numvals,valis);
        lives_free(valis);
      }
      break;
    } else {
      // double
      if (weed_leaf_num_elements(ptmpl,"max")==3) {
        maxds=weed_get_double_array(ptmpl,"max",&error);
        rmaxd=maxds[0];
        gmaxd=maxds[1];
        bmaxd=maxds[2];
        lives_free(maxds);
      } else rmaxd=gmaxd=bmaxd=weed_get_double_value(ptmpl,"max",&error);
      if (weed_leaf_num_elements(ptmpl,"min")==3) {
        minds=weed_get_double_array(ptmpl,"min",&error);
        rmind=minds[0];
        gmind=minds[1];
        bmind=minds[2];
        lives_free(minds);
      } else rmind=gmind=bmind=weed_get_double_value(ptmpl,"min",&error);
      colds[0]=rmind+(double)cols[0]/255.*(rmaxd-rmind);
      colds[1]=gmind+(double)cols[1]/255.*(gmaxd-gmind);
      colds[2]=bmind+(double)cols[2]/255.*(bmaxd-bmind);
      if (is_default) {
        weed_set_double_array(ptmpl,"host_default",3,colds);
      } else {
        int index=0,numvals;
        double *valds;

        if (mainw->multitrack!=NULL&&is_perchannel_multiw(ptmpl)) {
          index=mainw->multitrack->track_index;
        }
        numvals=weed_leaf_num_elements(param,"value");
        if (index*3>=numvals) {
          weed_plant_t *paramtmpl=weed_get_plantptr_value(param,"template",&error);
          fill_param_vals_to(param,paramtmpl,index);
          numvals=(index+1)*3;
        }

        valds=weed_get_double_array(param,"value",&error);
        valds[index*3]=colds[0];
        valds[index*3+1]=colds[1];
        valds[index*3+2]=colds[2];
        weed_set_double_array(param,"value",numvals,valds);
        lives_free(valds);
      }
    }
    break;
  }

}


void after_param_red_changed(LiVESSpinButton *spinbutton, lives_rfx_t *rfx) {
  LiVESList *retvals=NULL;

  lives_colRGB48_t old_value;

  int param_number=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton),"param_number"));
  int new_red;
  int copyto=-1;

  boolean was_reinited=FALSE;

  lives_param_t *param=&rfx->params[param_number];

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  get_colRGB24_param(param->value,&old_value);
  new_red=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  if (old_value.red==new_red) return;

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }


  set_colRGB24_param(param->value,new_red,old_value.green,old_value.blue);

  if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;

    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE)  {
      update_weed_color_value(inst,param_number,
                              new_red,old_value.green,old_value.blue,0);
      copyto=set_copy_to(inst,param_number,TRUE);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst,param_number);
        if (copyto!=-1) rec_param_change(inst,copyto);
      }

      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
        weed_reinit_effect(inst,FALSE);
        was_reinited=TRUE;
      }
    }
  }

  if (new_red!=old_value.red&&param->onchange) {
    param->change_blocked=TRUE;
    retvals=do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
    lives_widget_context_update();
    param->change_blocked=FALSE;
  }
  if (!was_reinited&&copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


void after_param_green_changed(LiVESSpinButton *spinbutton, lives_rfx_t *rfx) {
  LiVESList *retvals=NULL;

  lives_colRGB48_t old_value;

  int new_green;
  int copyto=-1;

  int param_number=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton),"param_number"));

  boolean was_reinited=FALSE;

  lives_param_t *param=&rfx->params[param_number];


  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  get_colRGB24_param(param->value,&old_value);
  new_green=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  if (old_value.green==new_green) return;

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }

  set_colRGB24_param(param->value,old_value.red,new_green,old_value.blue);

  if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;

    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      update_weed_color_value(inst,param_number,old_value.red,new_green,old_value.blue,0);

      copyto=set_copy_to(inst,param_number,TRUE);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst,param_number);
        if (copyto!=-1) rec_param_change(inst,copyto);
      }

      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
        weed_reinit_effect(inst,FALSE);
        was_reinited=TRUE;
      }
    }
  }

  if (new_green!=old_value.green&&param->onchange) {
    param->change_blocked=TRUE;
    retvals=do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
    lives_widget_context_update();
    param->change_blocked=FALSE;
  }
  if (!was_reinited&&copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


void after_param_blue_changed(LiVESSpinButton *spinbutton, lives_rfx_t *rfx) {
  LiVESList *retvals=NULL;

  lives_colRGB48_t old_value;

  int new_blue;
  int copyto=-1;
  int param_number=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton),"param_number"));

  boolean was_reinited=FALSE;

  lives_param_t *param=&rfx->params[param_number];

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  get_colRGB24_param(param->value,&old_value);
  new_blue=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  if (old_value.blue==new_blue) return;

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }

  set_colRGB24_param(param->value,old_value.red,old_value.green,new_blue);

  if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;

    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      update_weed_color_value(inst,param_number,old_value.red,old_value.green,new_blue,0);
      copyto=set_copy_to(inst,param_number,TRUE);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst,param_number);
        if (copyto!=-1) rec_param_change(inst,copyto);
      }

      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
        weed_reinit_effect(inst,FALSE);
        was_reinited=TRUE;
      }
    }
  }

  if (new_blue!=old_value.blue&&param->onchange) {
    param->change_blocked=TRUE;
    retvals=do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
    lives_widget_context_update();
    param->change_blocked=FALSE;
  }
  if (!was_reinited&&copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


void after_param_alpha_changed(LiVESSpinButton *spinbutton, lives_rfx_t *rfx) {
  // not used yet
  int param_number=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton),"param_number"));

  LiVESList *retvals=NULL;

  lives_param_t *param=&rfx->params[param_number];

  lives_colRGBA64_t old_value;

  int new_alpha=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));

  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }

  get_colRGBA32_param(param->value,&old_value);

  if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  set_colRGBA32_param(param->value,old_value.red,old_value.green,old_value.blue,new_alpha);

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }


  if (new_alpha!=old_value.alpha&&param->onchange) {
    param->change_blocked=TRUE;
    retvals=do_onchange(LIVES_WIDGET_OBJECT(spinbutton), rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
    lives_widget_context_update();
    param->change_blocked=FALSE;
  }
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


boolean after_param_text_focus_changed(LiVESWidget *hbox, LiVESWidget *child, lives_rfx_t *rfx) {
  // for non realtime effects
  // we don't usually want to run the trigger every single time the user presses a key in a text widget
  // so we only update when the user clicks OK or focusses out of the widget

  LiVESWidget *textwidget;

  if (rfx==NULL) return FALSE;

  if (mainw->multitrack!=NULL) {
    if (child!=NULL)
      lives_window_remove_accel_group(LIVES_WINDOW(mainw->multitrack->window),mainw->multitrack->accel_group);
    else
      lives_window_add_accel_group(LIVES_WINDOW(mainw->multitrack->window),mainw->multitrack->accel_group);

  }

  if (mainw->textwidget_focus!=NULL) {
    textwidget=(LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->textwidget_focus),"textwidget");
    after_param_text_changed(textwidget,rfx);
  }

  if (hbox!=NULL) {
    mainw->textwidget_focus=hbox;
  }

  return FALSE;
}


void after_param_text_changed(LiVESWidget *textwidget, lives_rfx_t *rfx) {
  LiVESTextBuffer *textbuffer=NULL;

  LiVESList *retvals=NULL;

  lives_param_t *param;

  char *old_text;
  const char *new_text;

  boolean was_reinited=FALSE;

  int copyto=-1;
  int param_number;


  if (rfx==NULL||rfx->params==NULL||textwidget==NULL) return;

  param_number=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(textwidget),"param_number"));

  param=&rfx->params[param_number];

  old_text=(char *)param->value;

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  if (LIVES_IS_TEXT_VIEW(textwidget)) {
    new_text=lives_text_view_get_text(LIVES_TEXT_VIEW(textwidget));
    if (!strcmp(new_text,old_text)) return;
    param->value=lives_strdup(new_text);
  } else {
    new_text=lives_entry_get_text(LIVES_ENTRY(textwidget));
    if (!strcmp(new_text,old_text)) return;
    param->value=lives_strdup(new_text);
  }

  if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error,i;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;
    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      char *disp_string=get_weed_display_string(inst,param_number);
      weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE,FALSE);
      int index=0,numvals;
      int key=-1;
      char **valss;

      if (mainw->multitrack!=NULL&&is_perchannel_multi(rfx,param_number)) {
        index=mainw->multitrack->track_index;
      }
      numvals=weed_leaf_num_elements(wparam,"value");
      if (index>=numvals) {
        weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
        fill_param_vals_to(wparam,paramtmpl,index);
        numvals=index+1;
      }

      valss=weed_get_string_array(wparam,"value",&error);
      valss[index]=lives_strdup((char *)param->value);
      if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);
      filter_mutex_lock(key);
      weed_set_string_array(wparam,"value",numvals,valss);
      filter_mutex_unlock(key);
      copyto=set_copy_to(inst,param_number,TRUE);
      for (i=0; i<numvals; i++) lives_free(valss[i]);
      lives_free(valss);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst,param_number);
        if (copyto!=-1) rec_param_change(inst,copyto);
      }

      if (disp_string!=NULL) {
        if ((int)param->max>RFX_TEXT_MAGIC||param->max==0.) {
          lives_text_buffer_set_text(LIVES_TEXT_BUFFER(textbuffer), (char *)param->value, -1);
        } else {
          lives_entry_set_text(LIVES_ENTRY(textwidget),disp_string);
        }
        lives_free(disp_string);
      }

      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
        weed_reinit_effect(inst,FALSE);
        was_reinited=TRUE;
      }

    }
  }

  if (strcmp(old_text,(char *)param->value)&&param->onchange) {
    param->change_blocked=TRUE;
    retvals=do_onchange(LIVES_WIDGET_OBJECT(textwidget), rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
    lives_widget_context_update();
    param->change_blocked=FALSE;
  }
  lives_free(old_text);
  if (!was_reinited&&copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;

}

static void after_param_text_buffer_changed(LiVESTextBuffer *textbuffer, lives_rfx_t *rfx) {
  LiVESWidget *textview=(LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(textbuffer),"textview");
  after_param_text_changed(textview,rfx);
}


void after_string_list_changed(LiVESCombo *combo, lives_rfx_t *rfx) {
  int param_number=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo),"param_number"));

  LiVESList *retvals=NULL;

  lives_param_t *param=&rfx->params[param_number];

  char *txt=lives_combo_get_active_text(combo);

  boolean was_reinited=FALSE;

  int old_index=get_int_param(param->value);
  int new_index=lives_list_strcmp_index(param->list,txt);
  int copyto=-1;

  lives_free(txt);

  if (mainw->block_param_updates) return; // updates are blocked until all params are ready

  if (new_index==-1) return;

  if (new_index==old_index) return;

  set_int_param(param->value,new_index);

  if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;
    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      char *disp_string=get_weed_display_string(inst,param_number);
      weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE,FALSE);
      int index=0,numvals;
      int key=-1;
      int *valis;

      if (mainw->multitrack!=NULL&&is_perchannel_multi(rfx,param_number)) {
        index=mainw->multitrack->track_index;
      }
      numvals=weed_leaf_num_elements(wparam,"value");
      if (index>=numvals) {
        weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
        fill_param_vals_to(wparam,paramtmpl,index);
        numvals=index+1;
      }

      valis=weed_get_int_array(wparam,"value",&error);
      valis[index]=new_index;
      if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);
      filter_mutex_lock(key);
      weed_set_int_array(wparam,"value",numvals,valis);
      filter_mutex_unlock(key);
      copyto=set_copy_to(inst,param_number,TRUE);
      lives_free(valis);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
        // if we are recording, add this change to our event_list
        rec_param_change(inst,param_number);
        if (copyto!=-1) rec_param_change(inst,copyto);
      }

      if (disp_string!=NULL) {
        lives_signal_handlers_block_by_func(combo,(livespointer)after_string_list_changed,(livespointer)rfx);
        lives_combo_set_active_string(LIVES_COMBO(combo),disp_string);
        lives_signal_handlers_unblock_by_func(combo,(livespointer)after_string_list_changed,(livespointer)rfx);
        lives_free(disp_string);
      }

      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
        weed_reinit_effect(inst,FALSE);
        was_reinited=TRUE;
      }

    }
  }

  if (old_index!=new_index&&param->onchange) {
    param->change_blocked=TRUE;
    retvals=do_onchange(LIVES_WIDGET_OBJECT(combo), rfx);
    if (retvals!=NULL) {
      lives_list_free_strings(retvals);
      lives_list_free(retvals);
    }
    lives_widget_context_update();
    param->change_blocked=FALSE;
  }
  if (!was_reinited&&copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}



char **param_marshall_to_argv(lives_rfx_t *rfx) {
  // this function will marshall all parameters into a argv array
  // last array element will be NULL

  // the returned **argv should be lives_free()'ed after use

  lives_colRGB48_t rgb;

  char **argv=(char **)lives_malloc((rfx->num_params+1)*(sizeof(char *)));

  char *tmp;

  register int i;

  for (i=0; i<rfx->num_params; i++) {
    switch (rfx->params[i].type) {
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param(rfx->params[i].value,&rgb);
      argv[i]=lives_strdup_printf("%u",(((rgb.red<<8)+rgb.green)<<8)+rgb.blue);
      break;

    case LIVES_PARAM_STRING:
      // escape strings
      argv[i]=lives_strdup_printf("%s",(tmp=U82L((char *)rfx->params[i].value)));
      lives_free(tmp);
      break;

    case LIVES_PARAM_STRING_LIST:
      // escape strings
      argv[i]=lives_strdup_printf("%d",get_int_param(rfx->params[i].value));
      break;

    default:
      if (rfx->params[i].dp) {
        char *return_pattern=lives_strdup_printf("%%.%df",rfx->params[i].dp);
        argv[i]=lives_strdup_printf(return_pattern,get_double_param(rfx->params[i].value));
        lives_free(return_pattern);
      } else {
        argv[i]=lives_strdup_printf("%d",get_int_param(rfx->params[i].value));
      }
    }
  }
  argv[i]=NULL;
  return argv;
}







char *param_marshall(lives_rfx_t *rfx, boolean with_min_max) {
  // this function will marshall all parameters into a space separated string
  // in case of string parameters, these will be surrounded by " and all
  // quotes will be escaped \"

  // the returned string should be lives_free()'ed after use
  lives_colRGB48_t rgb;

  char *new_return=lives_strdup("");
  char *old_return=new_return;
  char *return_pattern;
  char *tmp,*mysubst,*mysubst2;

  register int i;


  for (i=0; i<rfx->num_params; i++) {
    switch (rfx->params[i].type) {
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param(rfx->params[i].value,&rgb);
      if (!with_min_max) {
        new_return=lives_strdup_printf("%s %u",old_return,(((rgb.red<<8)+rgb.green)<<8)+rgb.blue);
      } else {
        new_return=lives_strdup_printf("%s %d %d %d",old_return,rgb.red,rgb.green,rgb.blue);
      }
      lives_free(old_return);
      old_return=new_return;
      break;

    case LIVES_PARAM_STRING:
      // we need to doubly escape strings
      mysubst=subst((char *)rfx->params[i].value,"\\","\\\\\\\\");
      mysubst2=subst(mysubst,"\"","\\\\\\\"");
      lives_free(mysubst);
      mysubst=subst(mysubst2,"`","\\`");
      lives_free(mysubst2);
      mysubst2=subst(mysubst,"'","\\`");
      lives_free(mysubst);
      new_return=lives_strdup_printf("%s \"%s\"",old_return,(tmp=U82L(mysubst2)));
      lives_free(tmp);
      lives_free(mysubst2);
      lives_free(old_return);
      old_return=new_return;
      break;

    case LIVES_PARAM_STRING_LIST:
      new_return=lives_strdup_printf("%s %d",old_return,get_int_param(rfx->params[i].value));
      lives_free(old_return);
      old_return=new_return;
      break;

    default:
      if (rfx->params[i].dp) {
        return_pattern=lives_strdup_printf("%%s %%.%df",rfx->params[i].dp);
        new_return=lives_strdup_printf(return_pattern,old_return,get_double_param(rfx->params[i].value));
        if (with_min_max) {
          lives_free(old_return);
          old_return=new_return;
          new_return=lives_strdup_printf(return_pattern,old_return,rfx->params[i].min);
          lives_free(old_return);
          old_return=new_return;
          new_return=lives_strdup_printf(return_pattern,old_return,rfx->params[i].max);
        }
        lives_free(return_pattern);
      } else {
        new_return=lives_strdup_printf("%s %d",old_return,get_int_param(rfx->params[i].value));
        if (with_min_max&&rfx->params[i].type!=LIVES_PARAM_BOOL) {
          lives_free(old_return);
          old_return=new_return;
          new_return=lives_strdup_printf("%s %d",old_return,(int)rfx->params[i].min);
          lives_free(old_return);
          old_return=new_return;
          new_return=lives_strdup_printf("%s %d",old_return,(int)rfx->params[i].max);
        }
      }
      lives_free(old_return);
      old_return=new_return;
    }
  }
  if (mainw->current_file>0&&with_min_max) {
    if (rfx->num_in_channels<2) {
      new_return=lives_strdup_printf("%s %d %d %d %d %d",old_return,cfile->hsize,cfile->vsize,cfile->start,
                                     cfile->end,cfile->frames);
    } else {
      // for transitions, change the end to indicate the merge section
      // this is better for length calculations
      int cb_frames=clipboard->frames;
      int start=cfile->start,end=cfile->end,ttl;

      if (prefs->ins_resample&&clipboard->fps!=cfile->fps) {
        cb_frames=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
      }

      if (merge_opts->spinbutton_loops!=NULL&&
          cfile->end-cfile->start+1>(cb_frames*(ttl=lives_spin_button_get_value_as_int
                                     (LIVES_SPIN_BUTTON(merge_opts->spinbutton_loops))))&&
          !merge_opts->loop_to_fit) {
        end=cb_frames*ttl;
        if (!merge_opts->align_start) {
          start=cfile->end-end+1;
          end=cfile->end;
        } else {
          start=cfile->start;
          end+=start-1;
        }
      }
      new_return=lives_strdup_printf("%s %d %d %d %d %d %d %d",old_return,cfile->hsize,cfile->vsize,start,end,
                                     cfile->frames,clipboard->hsize,clipboard->vsize);
    }
  } else {
    new_return=lives_strdup(old_return);
  }
  lives_free(old_return);

  return new_return;
}


char *reconstruct_string(LiVESList *plist, int start, int *offs) {
  // convert each piece from locale to utf8
  // concat list entries to get reconstruct
  // replace \" with "

  char *word=NULL;
  char *ret=lives_strdup(""),*ret2;
  char *tmp;

  boolean lastword=FALSE;

  register int i;

  word=L2U8((char *)lives_list_nth_data(plist,start));

  if (word==NULL||!strlen(word)||word[0]!='\"') {
    if (word!=NULL) lives_free(word);
    return 0;
  }

  word++;

  for (i=start; i<lives_list_length(plist); i++) {
    if (strlen(word)) {
      if ((word[strlen(word)-1]=='\"')&&(strlen(word)==1||word[strlen(word)-2]!='\\')) {
        lastword=TRUE;
        memset(word+strlen(word)-1,0,1);
      }
    }

    ret2=lives_strconcat(ret,(tmp=subst(word,"\\\"","\""))," ",NULL);
    lives_free(tmp);
    if (ret2!=ret) lives_free(ret);
    ret=ret2;

    if (i==start) word--;
    lives_free(word);

    if (lastword) break;

    if (i<lives_list_length(plist)-1) word=L2U8((char *)lives_list_nth_data(plist,i+1));
  }

  set_int_param(offs,i-start+1);

  // remove trailing space
  memset(ret+strlen(ret)-1,0,1);
  return ret;

}


void param_demarshall(lives_rfx_t *rfx, LiVESList *plist, boolean with_min_max, boolean upd) {
  int i;
  int pnum=0;
  lives_param_t *param;

  // here we take a LiVESList * of param values, set them in rfx, and if upd is TRUE we also update their visual appearance

  // param->widgets[n] are only valid if upd==TRUE

  if (plist==NULL) return;

  for (i=0; i<rfx->num_params; i++) {
    param=&rfx->params[i];
    pnum=set_param_from_list(plist,param,pnum,with_min_max,upd);
  }
}



LiVESList *argv_to_marshalled_list(lives_rfx_t *rfx, int argc, char **argv) {
  LiVESList *plist=NULL;

  char *tmp,*tmp2,*tmp3;

  register int i;

  if (argc==0) return plist;

  for (i=0; i<=argc&&argv[i]!=NULL; i++) {
    if (rfx->params[i].type==LIVES_PARAM_STRING) {
      tmp=lives_strdup_printf("\"%s\"",(tmp2=U82L(tmp3=subst(argv[i],"\"","\\\""))));
      plist=lives_list_append(plist,tmp);
      lives_free(tmp2);
      lives_free(tmp3);
    } else {
      plist=lives_list_append(plist,lives_strdup(argv[i]));
    }
  }
  return plist;
}





int set_param_from_list(LiVESList *plist, lives_param_t *param, int pnum, boolean with_min_max, boolean upd) {
  // update values for param using values in plist
  // if upd is TRUE, the widgets for that param also are updated;
  // otherwise, we do not update the widgets, but we do update the default

  // for LIVES_PARAM_NUM, setting pnum negative avoids having to send min,max
  // (other types dont have a min/max anyway)
  char *tmp; // work around some weirdness in glib

  int red,green,blue;
  int offs=0;
  int maxlen=lives_list_length(plist)-1;

  if (ABS(pnum)>maxlen) return 0;

  switch (param->type) {
  case LIVES_PARAM_BOOL:
    if (param->change_blocked) {
      pnum++;
      break;
    }
    tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
    set_bool_param(param->value,(atoi(tmp)));
    if (upd) {
      if (param->widgets[0]&&LIVES_IS_TOGGLE_BUTTON(param->widgets[0])) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(param->widgets[0]),get_bool_param(param->value));
      }
    } else set_bool_param(param->def,(atoi(tmp)));
    lives_free(tmp);
    break;
  case LIVES_PARAM_NUM:
    if (param->change_blocked) {
      pnum++;
      if (with_min_max) pnum+=2;
      break;
    }
    if (param->dp) {
      double double_val;
      tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
      double_val=lives_strtod(tmp,NULL);
      lives_free(tmp);
      if (with_min_max) {
        if (ABS(pnum)>maxlen) return 1;
        tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
        param->min=lives_strtod(tmp,NULL);
        lives_free(tmp);
        if (ABS(pnum)>maxlen) return 2;
        tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
        param->max=lives_strtod(tmp,NULL);
        lives_free(tmp);
        if (double_val<param->min) double_val=param->min;
        if (double_val>param->max) double_val=param->max;
      }
      set_double_param(param->value,double_val);
      if (upd) {
        if (param->widgets[0]&&LIVES_IS_SPIN_BUTTON(param->widgets[0])) {
          lives_rfx_t *rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(param->widgets[0]),"rfx");
          lives_signal_handlers_block_by_func(param->widgets[0],(livespointer)after_param_value_changed,(livespointer)rfx);
          lives_spin_button_set_range(LIVES_SPIN_BUTTON(param->widgets[0]),(double)param->min,(double)param->max);
          lives_spin_button_update(LIVES_SPIN_BUTTON(param->widgets[0]));
          lives_signal_handlers_unblock_by_func(param->widgets[0],(livespointer)after_param_value_changed,(livespointer)rfx);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]),get_double_param(param->value));
          lives_spin_button_update(LIVES_SPIN_BUTTON(param->widgets[0]));
        }
      } else set_double_param(param->def,double_val);
    } else {
      int int_value;
      tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
      int_value=atoi(tmp);
      lives_free(tmp);
      if (with_min_max) {
        int int_min,int_max;
        if (ABS(pnum)>maxlen) return 1;
        tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
        int_min=atoi(tmp);
        lives_free(tmp);
        if (ABS(pnum)>maxlen) return 2;
        tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
        int_max=atoi(tmp);
        lives_free(tmp);
        if (int_value<int_min) int_value=int_min;
        if (int_value>int_max) int_value=int_max;
        param->min=(double)int_min;
        param->max=(double)int_max;
      }
      set_int_param(param->value,int_value);

      if (upd) {
        if (param->widgets[0]&&LIVES_IS_SPIN_BUTTON(param->widgets[0])) {
          lives_rfx_t *rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(param->widgets[0]),"rfx");
          lives_signal_handlers_block_by_func(param->widgets[0],(livespointer)after_param_value_changed,(livespointer)rfx);
          lives_spin_button_set_range(LIVES_SPIN_BUTTON(param->widgets[0]),(double)param->min,(double)param->max);
          lives_spin_button_update(LIVES_SPIN_BUTTON(param->widgets[0]));
          lives_signal_handlers_unblock_by_func(param->widgets[0],(livespointer)after_param_value_changed,(livespointer)rfx);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]),(double)get_int_param(param->value));
          lives_spin_button_update(LIVES_SPIN_BUTTON(param->widgets[0]));
        }
      } else set_int_param(param->def,int_value);
    }
    break;
  case LIVES_PARAM_COLRGB24:
    tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
    red=atoi(tmp);
    lives_free(tmp);
    if (ABS(pnum)>maxlen) return 1;
    tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
    green=atoi(tmp);
    lives_free(tmp);
    if (ABS(pnum)>maxlen) return 2;
    tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
    blue=atoi(tmp);
    lives_free(tmp);
    if (param->change_blocked) break;
    set_colRGB24_param(param->value,red,green,blue);

    if (upd) {
      if (param->widgets[0]&&LIVES_IS_SPIN_BUTTON(param->widgets[0])) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]),(double)red);
      }
      if (param->widgets[1]&&LIVES_IS_SPIN_BUTTON(param->widgets[1])) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[1]),(double)green);
      }
      if (param->widgets[2]&&LIVES_IS_SPIN_BUTTON(param->widgets[2])) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[2]),(double)blue);
      }
    } else set_colRGB24_param(param->def,red,green,blue);
    break;
  case LIVES_PARAM_STRING:
    if (param->value!=NULL) lives_free(param->value);
    param->value=reconstruct_string(plist,pnum,&offs);
    if (upd) {
      if (param->widgets[0]!=NULL) {
        if (LIVES_IS_TEXT_VIEW(param->widgets[0])) {
          char *string=lives_strdup((char *)param->value); // work around bug in glib ???
          lives_text_view_set_text(LIVES_TEXT_VIEW(param->widgets[0]), string, -1);
          lives_free(string);
        } else {
          lives_entry_set_text(LIVES_ENTRY(param->widgets[0]),(char *)param->value);
        }
      }
    } else param->def=(void *)lives_strdup((char *)param->value);
    pnum+=offs;
    break;
  case LIVES_PARAM_STRING_LIST: {
    int int_value;
    tmp=lives_strdup((char *)lives_list_nth_data(plist,pnum++));
    int_value=atoi(tmp);
    lives_free(tmp);
    if (param->change_blocked) break;
    set_int_param(param->value,int_value);
    if (upd&&param->widgets[0]!=NULL&&LIVES_IS_COMBO(param->widgets[0])&&int_value<lives_list_length(param->list))
      lives_combo_set_active_string(LIVES_COMBO(param->widgets[0]),(char *)lives_list_nth_data(param->list,int_value));
    if (!upd) set_int_param(param->def,int_value);

    break;
  }
  default:
    break;
  }
  return pnum;
}


LiVESList *do_onchange(LiVESObject *object, lives_rfx_t *rfx) {
  LiVESList *retvals;

  int which=LIVES_POINTER_TO_INT(lives_widget_object_get_data(object,"param_number"));
  int width=0,height=0;

  const char *handle="";

  char *plugdir;
  char *com,*tmp;

  // weed plugins do not have triggers
  if (rfx->status==RFX_STATUS_WEED) return NULL;

  if (which<0) {
    // init
    switch (rfx->status) {
    case RFX_STATUS_BUILTIN:
      plugdir=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_RENDERED_EFFECTS_BUILTIN,NULL);
      break;
    case RFX_STATUS_CUSTOM:
      plugdir=lives_build_filename(capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_CUSTOM,NULL);
      break;
    case RFX_STATUS_TEST:
      plugdir=lives_build_filename(capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_TEST,NULL);
      break;
    default:
      plugdir=lives_strdup_printf("%s",prefs->tmpdir);
    }

    if (mainw->current_file>0) {
      width=cfile->hsize;
      height=cfile->vsize;
      handle=cfile->handle;
    }

    com=lives_strdup_printf("%s \"fxinit_%s\" \"%s\" \"%s\" %d %d %s",prefs->backend_sync,rfx->name,handle,plugdir,
                            width,height,(tmp=param_marshall(rfx,TRUE)));
    retvals=plugin_request_by_space(NULL,NULL,com);

    lives_free(tmp);
    lives_free(plugdir);
  } else {
    com=lives_strdup_printf("onchange_%d%s",which,param_marshall(rfx,TRUE));
    switch (rfx->status) {
    case RFX_STATUS_BUILTIN:
      retvals=plugin_request_by_space(PLUGIN_RENDERED_EFFECTS_BUILTIN,rfx->name,com);
      break;
    case RFX_STATUS_CUSTOM:
      retvals=plugin_request_by_space(PLUGIN_RENDERED_EFFECTS_CUSTOM,rfx->name,com);
      break;
    case RFX_STATUS_TEST:
      retvals=plugin_request_by_space(PLUGIN_RENDERED_EFFECTS_TEST,rfx->name,com);
      break;
    default:
      retvals=plugin_request_by_space(PLUGIN_RFX_SCRAP,rfx->name,com);
    }
  }

  if (retvals!=NULL) {
    param_demarshall(rfx,retvals,TRUE,which>=0);
  } else {
    if (which<=0&&mainw->error) {
      mainw->error=FALSE;
      do_blocking_error_dialog(lives_strdup_printf("\n\n%s\n\n",mainw->msg));
    }
  }
  lives_free(com);

  return retvals;

}



void on_pwcolsel(LiVESButton *button, lives_rfx_t *rfx) {
  LiVESWidgetColor selected;

  int pnum=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button),"param_number"));
  int r,g,b;

  lives_param_t *param=&rfx->params[pnum];

  lives_color_button_get_color(LIVES_COLOR_BUTTON(button),&selected);

  r=(int)((double)(selected.red+LIVES_WIDGET_COLOR_SCALE_255(0.5))/(double)LIVES_WIDGET_COLOR_SCALE_255(1.));
  g=(int)((double)(selected.green+LIVES_WIDGET_COLOR_SCALE_255(0.5))/(double)LIVES_WIDGET_COLOR_SCALE_255(1.));
  b=(int)((double)(selected.blue+LIVES_WIDGET_COLOR_SCALE_255(0.5))/(double)LIVES_WIDGET_COLOR_SCALE_255(1.));

  set_colRGB24_param(param->value,r,g,b);

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[0]),(double)r);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[1]),(double)g);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(param->widgets[2]),(double)b);
  lives_color_button_set_color(LIVES_COLOR_BUTTON(param->widgets[4]),&selected);
}




void update_visual_params(lives_rfx_t *rfx, boolean update_hidden) {
  // update parameters visually from an rfx object
  LiVESList *list;

  weed_plant_t **in_params,*in_param;
  weed_plant_t *inst=(weed_plant_t *)rfx->source;
  weed_plant_t *paramtmpl;

  int *colsi,*colsis,*valis;
  int *maxis=NULL,*minis=NULL;

  double *colsd,*colsds,*valds;
  double *maxds=NULL,*minds=NULL;

  double red_maxd,green_maxd,blue_maxd;
  double red_mind,green_mind,blue_mind;
  double vald,mind,maxd;

  char **valss;

  char *vals,*pattern;
  char *tmp,*tmp2;

  int cspace;
  int error;
  int num_params=0;
  int param_hint;
  int vali,mini,maxi;

  int red_max,green_max,blue_max;
  int red_min,green_min,blue_min;

  int index,numvals;
  int key=-1;

  register int i,j;

  if (weed_plant_has_leaf(inst,"in_parameters")) num_params=weed_leaf_num_elements(inst,"in_parameters");
  if (num_params==0) return;

  if (weed_plant_has_leaf(inst,"host_key")) key=weed_get_int_value(inst,"host_key",&error);

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  for (i=0; i<num_params; i++) {
    if (!is_hidden_param(inst,i)||update_hidden) {
      // by default we dont update hidden or reinit params

      in_param=in_params[i];
      paramtmpl=weed_get_plantptr_value(in_param,"template",&error);
      param_hint=weed_get_int_value(paramtmpl,"hint",&error);
      list=NULL;

      // assume index is 0, unless we are a framedraw multi parameter
      // most of the time this will be ok, as other such multivalued parameters should be hidden
      index=0;

      if (mainw->multitrack!=NULL&&is_perchannel_multi(rfx,i)) {
        index=mainw->multitrack->track_index;
      }

      filter_mutex_lock(key);

      numvals=weed_leaf_num_elements(in_param,"value");

      if (param_hint!=WEED_HINT_COLOR&&index>=numvals) {
        fill_param_vals_to(in_param,paramtmpl,index);
        numvals=index+1;
      }

      switch (param_hint) {
      case WEED_HINT_INTEGER:
        valis=weed_get_int_array(in_param,"value",&error);
        vali=valis[index];
        lives_free(valis);

        mini=weed_get_int_value(paramtmpl,"min",&error);
        maxi=weed_get_int_value(paramtmpl,"max",&error);

        list=lives_list_append(list,lives_strdup_printf("%d",vali));
        list=lives_list_append(list,lives_strdup_printf("%d",mini));
        list=lives_list_append(list,lives_strdup_printf("%d",maxi));
        set_param_from_list(list,&rfx->params[i],0,TRUE,TRUE);
        lives_list_free_strings(list);
        lives_list_free(list);

        break;
      case WEED_HINT_FLOAT:
        valds=weed_get_double_array(in_param,"value",&error);
        vald=valds[index];
        lives_free(valds);

        mind=weed_get_double_value(paramtmpl,"min",&error);
        maxd=weed_get_double_value(paramtmpl,"max",&error);

        pattern=lives_strdup("%.2f");

        if (weed_plant_has_leaf(paramtmpl,"gui")) {
          weed_plant_t *gui=weed_get_plantptr_value(paramtmpl,"gui",&error);
          if (weed_plant_has_leaf(gui,"decimals")) {
            int dp=weed_get_int_value(gui,"decimals",&error);
            lives_free(pattern);
            pattern=lives_strdup_printf("%%.%df",dp);
          }
        }

        list=lives_list_append(list,lives_strdup_printf(pattern,vald));
        list=lives_list_append(list,lives_strdup_printf(pattern,mind));
        list=lives_list_append(list,lives_strdup_printf(pattern,maxd));

        lives_free(pattern);

        set_param_from_list(list,&rfx->params[i],0,TRUE,TRUE);
        lives_list_free_strings(list);
        lives_list_free(list);

        break;
      case WEED_HINT_SWITCH:
        valis=weed_get_boolean_array(in_param,"value",&error);
        vali=valis[index];
        lives_free(valis);

        list=lives_list_append(list,lives_strdup_printf("%d",vali));
        set_param_from_list(list,&rfx->params[i],0,FALSE,TRUE);
        lives_list_free_strings(list);
        lives_list_free(list);

        break;
      case WEED_HINT_TEXT:
        valss=weed_get_string_array(in_param,"value",&error);
        vals=valss[index];

        list=lives_list_append(list,lives_strdup_printf("\"%s\"",(tmp=U82L(tmp2=subst(vals,"\"","\\\"")))));
        lives_free(tmp);
        lives_free(tmp2);
        set_param_from_list(list,&rfx->params[i],0,FALSE,TRUE);
        for (j=0; j<numvals; j++) {
          lives_free(valss[j]);
        }
        lives_free(valss);
        lives_list_free_strings(list);
        lives_list_free(list);
        break;
      case WEED_HINT_COLOR:
        cspace=weed_get_int_value(paramtmpl,"colorspace",&error);
        switch (cspace) {
        case WEED_COLORSPACE_RGB:
          numvals=weed_leaf_num_elements(in_param,"value");
          if (index*3>=numvals) fill_param_vals_to(in_param,paramtmpl,index);

          if (weed_leaf_seed_type(paramtmpl,"default")==WEED_SEED_INT) {
            colsis=weed_get_int_array(in_param,"value",&error);
            colsi=&colsis[3*index];

            if (weed_leaf_num_elements(paramtmpl,"max")==1) {
              red_max=green_max=blue_max=weed_get_int_value(paramtmpl,"max",&error);
            } else {
              maxis=weed_get_int_array(paramtmpl,"max",&error);
              red_max=maxis[0];
              green_max=maxis[1];
              blue_max=maxis[2];
            }
            if (weed_leaf_num_elements(paramtmpl,"min")==1) {
              red_min=green_min=blue_min=weed_get_int_value(paramtmpl,"min",&error);
            } else {
              minis=weed_get_int_array(paramtmpl,"min",&error);
              red_min=minis[0];
              green_min=minis[1];
              blue_min=minis[2];
            }

            colsi[0]=(int)((double)(colsi[0]-red_min)/(double)(red_max-red_min)*255.+.5);
            colsi[1]=(int)((double)(colsi[1]-green_min)/(double)(green_max-green_min)*255.+.5);
            colsi[2]=(int)((double)(colsi[2]-blue_min)/(double)(blue_max-blue_min)*255.+.5);

            if (colsi[0]<red_min) colsi[0]=red_min;
            if (colsi[1]<green_min) colsi[1]=green_min;
            if (colsi[2]<blue_min) colsi[2]=blue_min;
            if (colsi[0]>red_max) colsi[0]=red_max;
            if (colsi[1]>green_max) colsi[1]=green_max;
            if (colsi[2]>blue_max) colsi[2]=blue_max;

            list=lives_list_append(list,lives_strdup_printf("%d",colsi[0]));
            list=lives_list_append(list,lives_strdup_printf("%d",colsi[1]));
            list=lives_list_append(list,lives_strdup_printf("%d",colsi[2]));

            set_param_from_list(list,&rfx->params[i],0,FALSE,TRUE);

            lives_list_free_strings(list);
            lives_list_free(list);
            lives_free(colsis);
            if (maxis!=NULL) lives_free(maxis);
            if (minis!=NULL) lives_free(minis);
          } else {
            colsds=weed_get_double_array(in_param,"value",&error);
            colsd=&colsds[3*index];
            if (weed_leaf_num_elements(paramtmpl,"max")==1) {
              red_maxd=green_maxd=blue_maxd=weed_get_double_value(paramtmpl,"max",&error);
            } else {
              maxds=weed_get_double_array(paramtmpl,"max",&error);
              red_maxd=maxds[0];
              green_maxd=maxds[1];
              blue_maxd=maxds[2];
            }
            if (weed_leaf_num_elements(paramtmpl,"min")==1) {
              red_mind=green_mind=blue_mind=weed_get_double_value(paramtmpl,"min",&error);
            } else {
              minds=weed_get_double_array(paramtmpl,"min",&error);
              red_mind=minds[0];
              green_mind=minds[1];
              blue_mind=minds[2];
            }
            colsd[0]=(colsd[0]-red_mind)/(red_maxd-red_mind)*255.+.5;
            colsd[1]=(colsd[1]-green_mind)/(green_maxd-green_mind)*255.+.5;
            colsd[2]=(colsd[2]-blue_mind)/(blue_maxd-blue_mind)*255.+.5;

            if (colsd[0]<red_mind) colsd[0]=red_mind;
            if (colsd[1]<green_mind) colsd[1]=green_mind;
            if (colsd[2]<blue_mind) colsd[2]=blue_mind;
            if (colsd[0]>red_maxd) colsd[0]=red_maxd;
            if (colsd[1]>green_maxd) colsd[1]=green_maxd;
            if (colsd[2]>blue_maxd) colsd[2]=blue_maxd;

            list=lives_list_append(list,lives_strdup_printf("%.2f",colsd[0]));
            list=lives_list_append(list,lives_strdup_printf("%.2f",colsd[1]));
            list=lives_list_append(list,lives_strdup_printf("%.2f",colsd[2]));
            set_param_from_list(list,&rfx->params[i],0,FALSE,TRUE);

            lives_list_free_strings(list);
            lives_list_free(list);
            lives_free(colsds);
            if (maxds!=NULL) lives_free(maxds);
            if (minds!=NULL) lives_free(minds);
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

