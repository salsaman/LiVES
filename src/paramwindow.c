// paramwindow.c
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@gmail.com>
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

#ifdef ENABLE_GIW
#include "giw/giwknob.h"
#endif

extern gboolean do_effect (lives_rfx_t *, gboolean is_preview); //effects.c in LiVES
extern void on_realfx_activate (GtkMenuItem *, gpointer rfx); // effects.c in LiVES

GtkWidget *fx_dialog[2];

// TODO -
// use list of these in case we have multiple windows open
// right now this is single threaded because of this
static GSList *usrgrp_to_livesgrp[2]={NULL,NULL}; // ordered list of lives_widget_group_t

void do_onchange_init(lives_rfx_t *rfx) {
  GList *onchange=NULL;
  gchar **array;
  gchar *type;
  int i;

  if (rfx->status==RFX_STATUS_WEED) return;

  switch (rfx->status) {
  case RFX_STATUS_BUILTIN:
    type=g_strdup(PLUGIN_RENDERED_EFFECTS_BUILTIN);
    break;
  case RFX_STATUS_CUSTOM:
    type=g_strdup(PLUGIN_RENDERED_EFFECTS_CUSTOM);
    break;
  default:
    type=g_strdup_printf(PLUGIN_RENDERED_EFFECTS_TEST);
    break;
  }
  if ((onchange=plugin_request_by_line (type,rfx->name,"get_onchange"))!=NULL) {
    for (i=0;i<g_list_length (onchange);i++) {
      array=g_strsplit ((gchar *)g_list_nth_data (onchange,i),rfx->delim,-1);
      if (!strcmp (array[0],"init")) {
	// onchange is init
	// create dummy object with data
	GtkWidget *dummy_widget=gtk_label_new(NULL);
	g_object_set_data (G_OBJECT (dummy_widget),"param_number",GINT_TO_POINTER (-1));
	do_onchange (G_OBJECT (dummy_widget),rfx);
	gtk_widget_destroy (dummy_widget);
      }
      g_strfreev (array);
    }
    g_list_free_strings (onchange);
    g_list_free (onchange);
  }
  g_free (type);
}


void on_paramwindow_ok_clicked (GtkButton *button, lives_rfx_t *rfx) {
  int i;

  if (rfx!=NULL&&rfx->status!=RFX_STATUS_SCRAP) mainw->keep_pre=mainw->did_rfx_preview;

  if (mainw->textwidget_focus!=NULL) {
    GtkWidget *textwidget=(GtkWidget *)g_object_get_data (G_OBJECT (mainw->textwidget_focus),"textwidget");
    after_param_text_changed(textwidget,rfx);
  }

  if (mainw->did_rfx_preview) {
    for (i=0;i<rfx->num_params;i++) {
      if (rfx->params[i].changed) {
	mainw->keep_pre=FALSE;
	break;
      }
    }

    if (!mainw->keep_pre) {
      gchar *com;
#ifndef IS_MINGW
      com=g_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
      lives_system(com,TRUE); // try to stop any current previews / processing
#else
      // get pid from backend
      FILE *rfile;
      ssize_t rlen;
      char val[16];
      int pid;
      com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
      rfile=popen(com,"r");
      rlen=fread(val,1,16,rfile);
      pclose(rfile);
      memset(val+rlen,0,1);
      pid=atoi(val);
      
      lives_win32_kill_subprocesses(pid,TRUE);
#endif
      g_free(com);
      
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
    gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  }

  if (usrgrp_to_livesgrp[0]!=NULL) g_slist_free (usrgrp_to_livesgrp[0]);
  usrgrp_to_livesgrp[0]=NULL;
  if (fx_dialog[1]==NULL) special_cleanup();
  if (mainw->invis!=NULL) {
    gtk_widget_destroy(mainw->invis);
    mainw->invis=NULL;
  }

  if (rfx!=NULL&&rfx->status==RFX_STATUS_SCRAP) return;

  if (rfx->status==RFX_STATUS_WEED) on_realfx_activate(NULL,rfx);
  else on_render_fx_activate (NULL,rfx);

  mainw->keep_pre=FALSE;
  mainw->is_generating=FALSE;

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



void on_paramwindow_cancel_clicked2 (GtkButton *button, lives_rfx_t *rfx) {
  // close from rte window

  on_paramwindow_cancel_clicked(button,rfx);
  fx_dialog[1]=NULL;
  if (mainw->invis!=NULL) {
    gtk_widget_destroy(mainw->invis);
    mainw->invis=NULL;
  }
}

void on_paramwindow_cancel_clicked (GtkButton *button, lives_rfx_t *rfx) {
  mainw->block_param_updates=TRUE;
  if (mainw->did_rfx_preview) {
    gchar *com;
#ifndef IS_MINGW
    com=g_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
    lives_system(com,TRUE); // try to stop processing
#else
    // get pid from backend
    FILE *rfile;
    ssize_t rlen;
    char val[16];
    int pid;
    com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    pid=atoi(val);
    
    lives_win32_kill_subprocesses(pid,TRUE);
#endif
    g_free(com);
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
    gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  }
  if (rfx==NULL) {
    if (usrgrp_to_livesgrp[1]!=NULL) g_slist_free (usrgrp_to_livesgrp[1]);
    usrgrp_to_livesgrp[1]=NULL;
  }
  else {
    if (usrgrp_to_livesgrp[0]!=NULL) g_slist_free (usrgrp_to_livesgrp[0]);
    usrgrp_to_livesgrp[0]=NULL;
    if (rfx->status==RFX_STATUS_WEED&&rfx!=mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx) {
      rfx_free(rfx);
      g_free(rfx);
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
    return livesgrp_from_usrgrp (usrgrp_to_livesgrp[1], param->group);
  }
  else {
    return livesgrp_from_usrgrp (usrgrp_to_livesgrp[0], param->group);
  }
  return NULL;
}


void on_render_fx_activate (GtkMenuItem *menuitem, lives_rfx_t *rfx) {
  gboolean has_lmap_error=FALSE;

  if (menuitem!=NULL&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&rfx->num_in_channels>0&&
      (mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
    if (!do_layout_alter_frames_warning()) {
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,
		   0,0.,cfile->stored_layout_frame>0);
    has_lmap_error=TRUE;
    g_list_free_strings(mainw->xlays);
    g_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  // do onchange|init
  if (menuitem!=NULL) {
    do_onchange_init(rfx);
  }
  if (rfx->min_frames>-1) {
    do_effect(rfx,FALSE);
  }
  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

}


static void gen_width_changed (GtkSpinButton *spin, gpointer user_data) {
  weed_plant_t *ctmpl=(weed_plant_t *)user_data;
  gint val=gtk_spin_button_get_value_as_int(spin);
  int error,old_val=0;
  gint step;

  if (weed_plant_has_leaf(ctmpl,"host_width")) old_val=weed_get_int_value(ctmpl,"host_width",&error);

  if (val==old_val) return;
  step=1;
  if (weed_plant_has_leaf(ctmpl,"hstep")) step=weed_get_int_value(ctmpl,"hstep",&error);

  val=step_val(val,step);
  weed_set_int_value(ctmpl,"host_width",val);
  gtk_spin_button_set_value(spin,(gdouble)val);
}


static void gen_height_changed (GtkSpinButton *spin, gpointer user_data) {
  weed_plant_t *ctmpl=(weed_plant_t *)user_data;
  gint val=gtk_spin_button_get_value_as_int(spin);
  int error,old_val=0;
  gint step;

  if (weed_plant_has_leaf(ctmpl,"host_height")) old_val=weed_get_int_value(ctmpl,"host_height",&error);

  if (val==old_val) return;
  step=1;
  if (weed_plant_has_leaf(ctmpl,"hstep")) step=weed_get_int_value(ctmpl,"hstep",&error);

  val=step_val(val,step);
  weed_set_int_value(ctmpl,"host_height",val);
  gtk_spin_button_set_value(spin,(gdouble)val);
}


static void gen_fps_changed (GtkSpinButton *spin, gpointer user_data) {
  weed_plant_t *filter=(weed_plant_t *)user_data;
  gdouble val=gtk_spin_button_get_value(spin);
  weed_set_double_value(filter,"host_fps",val);
}



static void trans_in_out_pressed(lives_rfx_t *rfx, gboolean in) {
  int error;
  weed_plant_t *inst=(weed_plant_t *)rfx->source;
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);
  int trans=get_transition_param(filter);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t *tparam=in_params[trans];
  weed_plant_t *tparamtmpl=weed_get_plantptr_value(tparam,"template",&error);
  int hint=weed_get_int_value(tparamtmpl,"hint",&error);

  pthread_mutex_lock(&mainw->data_mutex);
  if (hint==WEED_HINT_INTEGER) {
    if (in) weed_set_int_value(tparam,"value",weed_get_int_value(tparamtmpl,"min",&error));
    else weed_set_int_value(tparam,"value",weed_get_int_value(tparamtmpl,"max",&error));
  }
  else {
    if (in) weed_set_double_value(tparam,"value",weed_get_double_value(tparamtmpl,"min",&error));
    else weed_set_double_value(tparam,"value",weed_get_double_value(tparamtmpl,"max",&error));
  }
  pthread_mutex_unlock(&mainw->data_mutex);
  set_copy_to(inst,trans,TRUE);
  update_visual_params(rfx,FALSE);
  weed_free(in_params);
}


static void transition_in_pressed(GtkToggleButton *tbut, gpointer rfx) {
  trans_in_out_pressed((lives_rfx_t *)rfx,TRUE);
}

static void transition_out_pressed(GtkToggleButton *tbut, gpointer rfx) {
  trans_in_out_pressed((lives_rfx_t *)rfx,FALSE);
}

static void after_transaudio_toggled(GtkToggleButton *togglebutton, gpointer rfx) {
  weed_plant_t *init_event=mainw->multitrack->init_event;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton))) 
    weed_set_boolean_value(init_event,"host_audio_transition",WEED_TRUE);
  else weed_set_boolean_value(init_event,"host_audio_transition",WEED_FALSE);

}

static void gen_cb_toggled(GtkToggleButton *tbut, gpointer rfx) {
  mainw->gen_to_clipboard=!mainw->gen_to_clipboard;
}




void transition_add_in_out(GtkBox *vbox, lives_rfx_t *rfx, gboolean add_audio_check) {
  // add in/out radios for multitrack transitions
  GtkWidget *radiobutton_in;
  GtkWidget *radiobutton_out;
  GtkWidget *hbox,*hbox2;
  GtkWidget *label;
  GtkWidget *eventbox;
  GtkWidget *hseparator;
  GSList *radiobutton_group = NULL;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  radiobutton_in=gtk_radio_button_new(NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton_in), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton_in));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton_in, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Transition _In"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton_in);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Transition in"));
  lives_tooltips_copy(radiobutton_in,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton_in);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  
  g_signal_connect_after (GTK_OBJECT (radiobutton_in), "toggled",
			  G_CALLBACK (transition_in_pressed),
			  (gpointer)rfx);



  if (add_audio_check) {
    int error;
    weed_plant_t *filter=weed_get_plantptr_value((weed_plant_t *)rfx->source,"filter_class",&error);
    GtkWidget *checkbutton = gtk_check_button_new ();

    if (weed_plant_has_leaf(mainw->multitrack->init_event,"host_audio_transition")&&
	weed_get_boolean_value(mainw->multitrack->init_event,"host_audio_transition",&error)==WEED_FALSE) 
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),FALSE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), TRUE);

    gtk_widget_set_tooltip_text( checkbutton, _("Check the box to make audio transition with the video"));
    eventbox=gtk_event_box_new();
    lives_tooltips_copy(eventbox,checkbutton);
    label=gtk_label_new_with_mnemonic (_("Crossfade audio"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);

    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      checkbutton);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }

    hbox2 = gtk_hbox_new (FALSE, 0);

    if (has_video_chans_in(filter,FALSE)) 
      gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 10);

    gtk_box_pack_start (GTK_BOX (hbox2), checkbutton, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox2), eventbox, FALSE, FALSE, 10);
    GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

    g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			    G_CALLBACK (after_transaudio_toggled),
			    (gpointer)rfx);

    after_transaudio_toggled(GTK_TOGGLE_BUTTON(checkbutton),(gpointer)rfx);
    
  }

  radiobutton_out=gtk_radio_button_new(radiobutton_group);

  gtk_box_pack_end (GTK_BOX (hbox), radiobutton_out, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Transition _Out"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton_out);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Transition out"));
  lives_tooltips_copy(radiobutton_out,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton_out);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_end (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  
  g_signal_connect_after (GTK_OBJECT (radiobutton_out), "toggled",
			  G_CALLBACK (transition_out_pressed),
			  (gpointer)rfx);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(hbox, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  hseparator = gtk_hseparator_new ();
  gtk_box_pack_start (vbox, hseparator, FALSE, FALSE, 0);

}






static gboolean add_sizes(GtkBox *vbox, gboolean add_fps, lives_rfx_t *rfx) {
  // add size settings for generators and resize effects
  int i,error;
  weed_plant_t *filter=weed_get_plantptr_value((weed_plant_t *)rfx->source,"filter_class",&error);
  int num_chans=weed_leaf_num_elements(filter,"out_channel_templates");
  weed_plant_t **ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error),*tmpl;
  gchar *cname,*ltxt;
  GtkWidget *label,*hbox,*hseparator;
  GtkWidget *spinbuttonh=NULL,*spinbuttonw=NULL;
  GtkWidget *spinbuttonf;
  GObject *spinbutton_adj;
  int def_width=0,max_width,width_step;
  int def_height=0,max_height,height_step;

  gdouble def_fps=0.;

  gboolean added=add_fps;
  gboolean chk_params=(vbox==NULL);

  // add fps


  if (add_fps) {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 20);
    
    add_fill_to_box(GTK_BOX(hbox));
    
    label=gtk_label_new_with_mnemonic(_("Target _FPS (plugin may override this)"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    
    if (weed_plant_has_leaf(filter,"host_fps")) def_fps=weed_get_double_value(filter,"host_fps",&error);
    else if (weed_plant_has_leaf(filter,"target_fps")) def_fps=weed_get_double_value(filter,"target_fps",&error);
    
    if (def_fps==0.) def_fps=prefs->default_fps;
    
    spinbutton_adj = (GObject *)gtk_adjustment_new (def_fps, 1., FPS_MAX, 1., 10., 0.);
    spinbuttonf = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1., 3);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbuttonf)->entry)), TRUE);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),spinbuttonf);
    gtk_box_pack_start (GTK_BOX (hbox), spinbuttonf, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbuttonf),GTK_UPDATE_IF_VALID);
    
    g_signal_connect_after (GTK_OBJECT (spinbuttonf), "value_changed",
			    G_CALLBACK (gen_fps_changed),
			    filter);
    
    add_fill_to_box(GTK_BOX(hbox));
  }


  
  for (i=0;i<num_chans;i++) {
    tmpl=ctmpls[i];
    if (weed_plant_has_leaf(tmpl,"disabled")&&weed_get_boolean_value(tmpl,"disabled",&error)==WEED_TRUE) continue;
    if (weed_plant_has_leaf(tmpl,"width")&&weed_get_int_value(tmpl,"width",&error)!=0) continue;
    if (weed_plant_has_leaf(tmpl,"height")&&weed_get_int_value(tmpl,"height",&error)!=0) continue;
   
    added=TRUE;
 
    if (chk_params) continue;

    if (rfx->is_template) {
      cname=weed_get_string_value(tmpl,"name",&error);
      ltxt=g_strdup_printf(_("%s : size"),cname);
      weed_free(cname);
    }
    else {
      ltxt=g_strdup(_("New size (pixels)"));
    }

    label=gtk_label_new(ltxt);
    g_free(ltxt);

    gtk_box_pack_start (vbox, label, FALSE, FALSE, 10);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
    label=gtk_label_new_with_mnemonic(_("_Width"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);

    if (weed_plant_has_leaf(tmpl,"host_width")) def_width=weed_get_int_value(tmpl,"host_width",&error);
    if (def_width==0) def_width=DEF_GEN_WIDTH;
    max_width=INT_MAX;
    if (weed_plant_has_leaf(tmpl,"maxwidth")) max_width=weed_get_int_value(tmpl,"maxwidth",&error);
    if (def_width>max_width) def_width=max_width;
    width_step=1;
    if (weed_plant_has_leaf(tmpl,"hstep")) width_step=weed_get_int_value(tmpl,"hstep",&error);

    spinbutton_adj = (GObject *)gtk_adjustment_new (def_width, 1., max_width, width_step==1?4:width_step, 
					 width_step==1?16:width_step*4, 0.);
    spinbuttonw = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), width_step, 0);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbuttonw)->entry)), TRUE);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),spinbuttonw);
    gtk_box_pack_start (GTK_BOX (hbox), spinbuttonw, FALSE, FALSE, 10);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbuttonw),GTK_UPDATE_IF_VALID);

    g_signal_connect_after (GTK_OBJECT (spinbuttonw), "value_changed",
			    G_CALLBACK (gen_width_changed),
			    tmpl);
    weed_leaf_delete(tmpl,"host_width"); // force a reset
    gen_width_changed(GTK_SPIN_BUTTON(spinbuttonw),tmpl);

    label=gtk_label_new_with_mnemonic(_("_Height"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    if (weed_plant_has_leaf(tmpl,"host_height")) def_height=weed_get_int_value(tmpl,"host_height",&error);
    if (def_height==0) def_height=DEF_GEN_HEIGHT;
    max_height=INT_MAX;
    if (weed_plant_has_leaf(tmpl,"maxheight")) max_height=weed_get_int_value(tmpl,"maxheight",&error);
    if (def_height>max_height) def_height=max_height;
    height_step=1;
    if (weed_plant_has_leaf(tmpl,"vstep")) height_step=weed_get_int_value(tmpl,"vstep",&error);

    spinbutton_adj = (GObject *)gtk_adjustment_new (def_height, 1., max_height, height_step==1?4:height_step, 
					 height_step==1?16:height_step*4, 0.);
    spinbuttonh = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), height_step, 0);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbuttonh)->entry)), TRUE);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),spinbuttonh);
    gtk_box_pack_end (GTK_BOX (hbox), spinbuttonh, FALSE, FALSE, 10);
    gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbuttonh),GTK_UPDATE_IF_VALID);
    g_signal_connect_after (GTK_OBJECT (spinbuttonh), "value_changed",
			    G_CALLBACK (gen_height_changed),
			    tmpl);
    weed_leaf_delete(tmpl,"host_height"); // force a reset
    gen_height_changed(GTK_SPIN_BUTTON(spinbuttonh),tmpl);

  }



  if (!chk_params) {
    hseparator = gtk_hseparator_new ();
    if (added) gtk_box_pack_start (vbox, hseparator, TRUE, TRUE, 0);
    
    if (!rfx->is_template) {
      lives_param_t param;
      // add "aspectratio" widget
      init_special();
      add_to_special("aspect|-100|-101|",rfx); // use virtual parameter numbers -100 and -101
      param.widgets[0]=spinbuttonw;
      check_for_special (&param,-100,vbox,rfx);
      param.widgets[0]=spinbuttonh;
      check_for_special (&param,-101,vbox,rfx);
    }
  }

  return added;

}




static void add_gen_to(GtkBox *vbox, lives_rfx_t *rfx) {
  // add "generate to clipboard/new clip" for rendered generators

  GtkWidget *radiobutton;
  GtkWidget *label;
  GtkWidget *eventbox;
  GtkWidget *hseparator;

  GSList *radiobutton_group = NULL;

  GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
  
  radiobutton = gtk_radio_button_new (NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Generate to _Clipboard"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Generate frames to the clipboard"));
  lives_tooltips_copy(radiobutton,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  
  radiobutton=gtk_radio_button_new(radiobutton_group);

  gtk_box_pack_end (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Generate to _New Clip"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Generate frames to a new clip"));
  lives_tooltips_copy(radiobutton,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_end (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(hbox, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  hseparator = gtk_hseparator_new ();
  gtk_box_pack_start (vbox, hseparator, FALSE, FALSE, 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton), !mainw->gen_to_clipboard);

  g_signal_connect_after (GTK_OBJECT (radiobutton), "toggled",
			  G_CALLBACK (gen_cb_toggled),
			  (gpointer)rfx);





}




void on_render_fx_pre_activate (GtkMenuItem *menuitem, lives_rfx_t *rfx) {
  GtkWidget *top_dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *resetbutton=NULL;
  GtkWidget *pbox;
  GtkAccelGroup *fxw_accel_group;

  gchar *txt;
  gboolean no_process=FALSE;

  gboolean is_realtime=FALSE;
  gboolean is_defaults=FALSE;

  int n=0;

  gboolean has_lmap_error=FALSE;

  gboolean has_param;

  if (!check_storage_space((mainw->current_file>-1)?cfile:NULL,FALSE)) return;

  // TODO - remove this and check in rfx / realfx activate

  if (rfx->num_in_channels>0) {
    if (menuitem!=NULL&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&
	(mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
      if (!do_layout_alter_frames_warning()) {
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		     cfile->stored_layout_frame>0);
      has_lmap_error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (menuitem==NULL) {
    no_process=TRUE;
    is_realtime=TRUE;
    n=1;
  }
  else if (rfx->status!=RFX_STATUS_WEED) {
    do_onchange_init(rfx);
  }

  if (rfx->min_frames<0) no_process=TRUE;

  if (!no_process&&rfx->num_in_channels==0) {
    gint new_file;
    mainw->pre_src_file=mainw->current_file;

    // create a new file to generate frames into
    if (!get_new_handle((new_file=mainw->first_free_file),NULL)) {

      if (mainw->multitrack!=NULL) {
	mt_sensitise(mainw->multitrack);
	mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
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
    if (mainw->current_file<=0) return;
    if (cfile->end-cfile->start+1<rfx->min_frames) {
      txt=g_strdup_printf (_("\nYou must select at least %d frames to use this effect.\n\n"),rfx->min_frames);
      do_blocking_error_dialog (txt);
      g_free (txt);
      return;
    }
    
    // here we invalidate cfile->ohsize, cfile->ovsize
    cfile->ohsize=cfile->hsize;
    cfile->ovsize=cfile->vsize;
    
    if (cfile->undo_action==UNDO_RESIZABLE) {
      set_undoable(NULL,FALSE);
    }
  }

  fx_dialog[n] = gtk_dialog_new ();
  txt=g_strdup_printf ("LiVES: - %s",_(rfx->menu_text));
  gtk_window_set_title (GTK_WINDOW (fx_dialog[n]), txt);
  g_free (txt);

  if (prefs->gui_monitor!=0) {
    gtk_window_set_screen(GTK_WINDOW(fx_dialog[n]),mainw->mgeom[prefs->gui_monitor-1].screen);
  }

  if (rfx->status==RFX_STATUS_WEED&&rfx->is_template) is_defaults=TRUE;

  gtk_container_set_border_width (GTK_CONTAINER (fx_dialog[n]), 20);
  gtk_window_set_position (GTK_WINDOW (fx_dialog[n]), GTK_WIN_POS_CENTER_ALWAYS);

  if (menuitem!=NULL) {
    // activated from the menu for a rendered effect
    if (prefs->show_gui) {
      if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(fx_dialog[n]),GTK_WINDOW(mainw->LiVES));
      else gtk_window_set_transient_for(GTK_WINDOW(fx_dialog[n]),GTK_WINDOW(mainw->multitrack->window));
    }
    gtk_window_set_modal (GTK_WINDOW (fx_dialog[n]), TRUE);
  }

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(fx_dialog[n], GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(fx_dialog[n]),FALSE);
  }

  pbox = top_dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(fx_dialog[n]));

  g_object_set_data(G_OBJECT(fx_dialog[n]),"rfx",rfx);

  if (rfx->status!=RFX_STATUS_WEED&&!no_process) {
    // rendered fx preview

    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (top_dialog_vbox), hbox, FALSE, FALSE, 0);
    pbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), pbox, FALSE, FALSE, 0);
    
    // add preview window
    if (rfx->num_in_channels>0) {
      mainw->framedraw_frame=cfile->start;
      widget_add_framedraw(GTK_VBOX(pbox),cfile->start,cfile->end,!(rfx->props&RFX_PROPS_MAY_RESIZE),
			   cfile->hsize,cfile->vsize);
    }
    else {
      if (!(rfx->props&RFX_PROPS_BATCHG)) {
	mainw->framedraw_frame=0;
	widget_add_framedraw(GTK_VBOX(pbox),1,1,TRUE,MAX_PRE_X,MAX_PRE_Y);
      }
    }

    if (!(rfx->props&RFX_PROPS_BATCHG)) {
      // connect spinbutton to preview
      fd_connect_spinbutton(rfx);
    }
  }

  has_param=make_param_box(GTK_VBOX (pbox), rfx);

  dialog_action_area = GTK_DIALOG (fx_dialog[n])->action_area;
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area), 80);
  gtk_box_set_spacing (GTK_BOX (dialog_action_area),10);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (dialog_action_area), DEF_BUTTON_WIDTH, -1);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
    
  fxw_accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (fx_dialog[n]), fxw_accel_group);

  if (!no_process||is_defaults||rfx->status==RFX_STATUS_SCRAP) {
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);
    gtk_dialog_add_action_widget (GTK_DIALOG (fx_dialog[n]), cancelbutton, GTK_RESPONSE_CANCEL);
    gtk_widget_add_accelerator (cancelbutton, "activate", fxw_accel_group,
				GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

    if (is_defaults) {
      okbutton = gtk_button_new_with_mnemonic (_("Set as default"));
      if (!has_param) gtk_widget_set_sensitive(okbutton,FALSE);
      resetbutton = gtk_button_new_with_mnemonic (_("Reset"));
      if (!has_param) gtk_widget_set_sensitive(resetbutton,FALSE);
      gtk_dialog_add_action_widget (GTK_DIALOG (fx_dialog[n]), resetbutton, LIVES_RESET);
    }
    else okbutton = gtk_button_new_from_stock ("gtk-ok");
    gtk_dialog_add_action_widget (GTK_DIALOG (fx_dialog[n]), okbutton, GTK_RESPONSE_OK);
  }
  else {
    okbutton = gtk_button_new_with_mnemonic (_("Set as default"));
    if (!has_param) gtk_widget_set_sensitive(okbutton,FALSE);
    cancelbutton = gtk_button_new_with_mnemonic (_("Close _window"));
    if (rfx->status==RFX_STATUS_WEED) {
      resetbutton = gtk_button_new_with_mnemonic (_("Reset"));
      gtk_dialog_add_action_widget (GTK_DIALOG (fx_dialog[n]), resetbutton, LIVES_RESET);
      gtk_dialog_add_action_widget (GTK_DIALOG (fx_dialog[n]), okbutton, GTK_RESPONSE_OK);
    }
    gtk_dialog_add_action_widget (GTK_DIALOG (fx_dialog[n]), cancelbutton, GTK_RESPONSE_CANCEL);
    gtk_widget_add_accelerator (cancelbutton, "activate", fxw_accel_group,
				GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  }

  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  if (okbutton->parent!=NULL) {
    GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
    gtk_widget_grab_default (okbutton);
  }

  gtk_widget_show_all (fx_dialog[n]);

  if (no_process&&!is_defaults) {
    if (!is_realtime) {
      if (okbutton->parent!=NULL)
	g_signal_connect (GTK_OBJECT (okbutton), "clicked",
			  G_CALLBACK (on_paramwindow_cancel_clicked),
			  rfx);
      g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
			G_CALLBACK (on_paramwindow_cancel_clicked),
			rfx);
      g_signal_connect (GTK_OBJECT (fx_dialog[n]), "delete_event",
			G_CALLBACK (on_paramwindow_cancel_clicked),
			rfx);
    }
    else {
      g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
			G_CALLBACK (on_paramwindow_cancel_clicked2),
			rfx);
      if (rfx->status==RFX_STATUS_SCRAP)
	g_signal_connect (GTK_OBJECT (okbutton), "clicked",
			  G_CALLBACK (on_paramwindow_cancel_clicked2),
			  rfx);
      else {
	g_signal_connect (GTK_OBJECT (okbutton), "clicked",
			  G_CALLBACK (rte_set_key_defs),
			  rfx);
	if (resetbutton!=NULL) {
	  g_signal_connect_after (GTK_OBJECT (resetbutton), "clicked",
				  G_CALLBACK (rte_reset_defs_clicked),
				  rfx);
	}
      }
      g_signal_connect (GTK_OBJECT (fx_dialog[n]), "delete_event",
			G_CALLBACK (on_paramwindow_cancel_clicked2),
			rfx);
    }
  }
  else {
    if (!is_defaults) {
      g_signal_connect (GTK_OBJECT (okbutton), "clicked",
			G_CALLBACK (on_paramwindow_ok_clicked),
			(gpointer)rfx);
      g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
			G_CALLBACK (on_paramwindow_cancel_clicked),
			(gpointer)rfx);
      g_signal_connect (GTK_OBJECT (fx_dialog[n]), "delete_event",
			G_CALLBACK (on_paramwindow_cancel_clicked),
			(gpointer)rfx);

    }
    else {
      g_signal_connect_after (GTK_OBJECT (okbutton), "clicked",
			G_CALLBACK (rte_set_defs_ok),
			rfx);
      if (resetbutton!=NULL) {
	g_signal_connect_after (GTK_OBJECT (resetbutton), "clicked",
				G_CALLBACK (rte_reset_defs_clicked),
				rfx);
	g_object_set_data(G_OBJECT(resetbutton),"cancelbutton",(gpointer)cancelbutton);

      }
      g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
			G_CALLBACK (rte_set_defs_cancel),
			rfx);
      g_signal_connect (GTK_OBJECT (fx_dialog[n]), "delete_event",
			G_CALLBACK (rte_set_defs_cancel),
			rfx);
    }
  }
  g_signal_connect (GTK_OBJECT (fx_dialog[n]), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  // tweak some things to do with framedraw preview
  if (mainw->framedraw!=NULL) fd_tweak(rfx);


}

  
static void check_hidden_gui(weed_plant_t *inst, lives_param_t *param) {
  int error;
  weed_plant_t *wtmpl,*gui;

  if (param->reinit&&(weed_get_int_value(inst,"host_refs",&error)==2||
		      (mainw->multitrack!=NULL&&mainw->multitrack->fx_box!=NULL&&
		       mt_get_effect_time(mainw->multitrack)>0.))) {
    // effect is running and user is editing the params (or in multitrack at not at fx time 0.)
    param->hidden|=HIDDEN_NEEDS_REINIT;
  }
  else if (param->hidden&HIDDEN_NEEDS_REINIT) param->hidden^=HIDDEN_NEEDS_REINIT;

  if ((wtmpl=(weed_plant_t *)param->source)==NULL) return;

  if (!weed_plant_has_leaf(wtmpl,"gui")) return;

  gui=weed_get_plantptr_value(wtmpl,"gui",&error);

  if (weed_plant_has_leaf(gui,"hidden")) {
    int hidden=weed_get_boolean_value(gui,"hidden",&error);
    if (hidden==WEED_TRUE) param->hidden|=HIDDEN_GUI;
    else if (param->hidden&HIDDEN_GUI) param->hidden^=HIDDEN_GUI;
  }
}



gboolean make_param_box(GtkVBox *top_vbox, lives_rfx_t *rfx) {
  // make a dynamic parameter window

  // returns TRUE if we added any parameters

  GtkWidget *param_vbox=NULL;
  GtkWidget *top_hbox=NULL;
  GtkWidget *hbox=NULL;

  gchar **array;
  gboolean used[rfx->num_params];
  gboolean has_box=FALSE;
  gchar *line;
  gchar *type=NULL;
  GList *hints=NULL;
  GList *onchange=NULL;
  GList *layout=NULL;
  int i,j,k,pnum,error;
  int length;
  lives_param_t *param=NULL;

  gint num_tok;
  gchar label_text[256]; // max length of a label in layout hints

  // put whole thing in scrolled window
  GtkWidget *scrolledwindow;

  gboolean internal=FALSE;
  gboolean noslid;
  gboolean has_param=FALSE;
  gboolean chk_params=FALSE;

  if (top_vbox==NULL) {
    // I think this means we just check some things with params without displaying them
    chk_params=TRUE;
  }
  else {
    mainw->textwidget_focus=NULL;

    // initialise special widgets
    init_special();
    
    if (rfx->status==RFX_STATUS_WEED) usrgrp_to_livesgrp[1]=NULL;
    else usrgrp_to_livesgrp[0]=NULL;

    // paramwindow start, everything goes in top_hbox
    
    top_hbox = gtk_hbox_new (FALSE, 10);
    
    // param_vbox holds the dynamic parameters
    param_vbox = gtk_vbox_new (FALSE, 10);
    gtk_box_pack_start (GTK_BOX (top_hbox), param_vbox, TRUE, TRUE, 10);
    gtk_box_set_spacing (GTK_BOX (param_vbox), 5);
  }

  switch (rfx->status) {
  case RFX_STATUS_BUILTIN:
    type=g_strdup(PLUGIN_RENDERED_EFFECTS_BUILTIN);
    break;
  case RFX_STATUS_CUSTOM:
    type=g_strdup(PLUGIN_RENDERED_EFFECTS_CUSTOM);
    break;
  case RFX_STATUS_SCRAP:
      type=g_strdup(PLUGIN_RFX_SCRAP);
      break;
  case RFX_STATUS_WEED:
      internal=TRUE;
      break;
  default:
    type=g_strdup(PLUGIN_RENDERED_EFFECTS_TEST);
    break;
  }
  
  // extras for multitrack
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED&&!chk_params) {
    weed_plant_t *filter=weed_get_plantptr_value((weed_plant_t *)rfx->source,"filter_class",&error);
    if (enabled_in_channels(filter,FALSE)==2&&get_transition_param(filter)!=-1) {
      // add in/out for multitrack transition
      transition_add_in_out(GTK_BOX(param_vbox),rfx,(mainw->multitrack->opts.pertrack_audio));
      //trans_in_out_pressed(rfx,TRUE);
    }
  }

  // extras for converters
  if (internal&&weed_instance_is_resizer((weed_plant_t *)rfx->source)&&!chk_params) {
    has_param=add_sizes(GTK_BOX(param_vbox),FALSE,rfx);
  }

  if (rfx->status!=RFX_STATUS_SCRAP&&!internal&&rfx->num_in_channels==0&&rfx->min_frames>-1&&!chk_params) {
    if (mainw->multitrack==NULL) add_gen_to(GTK_BOX(param_vbox),rfx);
    else mainw->gen_to_clipboard=FALSE;
  }

  if (!internal&&!chk_params) {
    // do onchange|init
    if ((onchange=plugin_request_by_line (type,rfx->name,"get_onchange"))!=NULL) {
      for (i=0;i<g_list_length (onchange);i++) {
	array=g_strsplit ((gchar *)g_list_nth_data (onchange,i),rfx->delim,-1);
	if (strcmp (array[0],"init")) {
	  // note other onchanges so we don't have to keep parsing the list
	  gint which=atoi (array[0]);
	  if (which>=0&&which<rfx->num_params) {
	    rfx->params[which].onchange=TRUE;
	  }
	}
	g_strfreev (array);
      }
      g_list_free_strings (onchange);
      g_list_free (onchange);
    }
    hints=plugin_request_by_line (type,rfx->name,"get_param_window");
    g_free(type);
  }
  else if (!chk_params) hints=get_external_window_hints(rfx);

  // do param window hints
  if (hints!=NULL) {
    gchar *lstring=g_strconcat("layout",rfx->delim,NULL);
    gchar *sstring=g_strconcat("special",rfx->delim,NULL);
    for (i=0;i<g_list_length (hints);i++) {
      if (!strncmp ((gchar *)g_list_nth_data (hints,i),lstring,7)) {
	layout=g_list_append (layout,g_strdup((gchar *)g_list_nth_data (hints,i)+7));
      }
      else if (!strncmp ((gchar *)g_list_nth_data (hints,i),sstring,8)) {
	add_to_special((gchar *)g_list_nth_data (hints,i)+8,rfx);
      }
    }
    g_list_free_strings (hints);
    g_list_free (hints);  // no longer needed
    g_free(lstring);
    g_free(sstring);
  }

  for (i=0;i<rfx->num_params;i++) {
    used[i]=FALSE;
    for (j=0;j<MAX_PARAM_WIDGETS;rfx->params[i].widgets[j++]=NULL);
  }

  mainw->block_param_updates=TRUE; // block framedraw updates until all parameter widgets have been created

  // use layout hints to build as much as we can
  for (i=0;i<g_list_length (layout);i++) {
    has_box=FALSE;
    noslid=FALSE;
    line=(gchar *)g_list_nth_data (layout,i);
    num_tok=get_token_count (line,(unsigned int)rfx->delim[0]);
    // ignore | inside strings
    array=g_strsplit (line,rfx->delim,num_tok);
    if (!strlen(array[num_tok-1])) num_tok--;
    for (j=0;j<num_tok;j++) {
      if (!strncmp (array[j],"p",1)&&(pnum=atoi ((gchar *)(array[j]+1)))>=0&&pnum<rfx->num_params&&!used[pnum]) {
	param=&rfx->params[pnum];
	if (rfx->source_type==LIVES_RFX_SOURCE_WEED) check_hidden_gui((weed_plant_t *)rfx->source,param);
	if ((param->hidden&&param->hidden!=HIDDEN_NEEDS_REINIT)||
	    param->type==LIVES_PARAM_UNDISPLAYABLE) continue;
	// parameter, eg. p1
	if (!has_box) {
	  hbox = gtk_hbox_new (TRUE, 0);
	  gtk_box_pack_start (GTK_BOX (param_vbox), hbox, FALSE, FALSE, 10);
	  has_box=TRUE;
	  has_param=TRUE;
	}
	if (add_param_to_box (GTK_BOX (hbox),rfx,pnum,(j==(num_tok-1))&&!noslid)) noslid=TRUE;
	used[pnum]=TRUE;
	has_param=TRUE;
      }
      else if (!j&&!strcmp (array[j],"hseparator")&&has_param) {
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (param_vbox), hbox, FALSE, FALSE, 10);
	add_hsep_to_box (GTK_BOX (hbox));
	j=num_tok;  // ignore anything after hseparator
      }
      else if (!strncmp (array[j],"fill",4)) {
	// can be filln
	if (strlen (array[j])==4||!(length=atoi (array[j]+4))) length=1;
	if (!has_box) {
	  hbox = gtk_hbox_new (FALSE, 0);
	  gtk_box_pack_start (GTK_BOX (param_vbox), hbox, FALSE, FALSE, 10);
	  has_box=TRUE;
	}
	for (k=0;k<length;k++) {
	  add_fill_to_box (GTK_BOX (hbox));
	}
      }
      else if (!strncmp (array[j],"\"",1)) {
	// label
	if (!has_box) {
	  hbox = gtk_hbox_new (FALSE, 0);
	  gtk_box_pack_start (GTK_BOX (param_vbox), hbox, FALSE, FALSE, 10);
	  has_box=TRUE;
	}
	g_snprintf (label_text,256,"%s",array[j]+1);
	while (strcmp (array[j]+strlen (array[j])-1,"\"")&&j<num_tok-1) {
	  g_strappend (label_text,256,array[++j]);
	}
	if (strlen (label_text)>1) {
	  if (!strcmp (label_text+strlen (label_text)-1,"\"")) {
	    memset (label_text+strlen (label_text)-1,0,1);
	  }
	  add_label_to_box (GTK_BOX (hbox),TRUE,label_text);
	}}}
    g_strfreev (array);
  }
  if (layout!=NULL) {
    g_list_free_strings (layout);
    g_list_free (layout);
  }

  // add any unused parameters
  for (i=0;i<rfx->num_params;i++) {
    rfx->params[i].changed=FALSE;
    if (rfx->source_type==LIVES_RFX_SOURCE_WEED) check_hidden_gui((weed_plant_t *)rfx->source,&rfx->params[i]);
    if ((rfx->params[i].hidden&&rfx->params[i].hidden!=HIDDEN_NEEDS_REINIT)||
	rfx->params[i].type==LIVES_PARAM_UNDISPLAYABLE) continue;
    if (!used[i]) {
      if (!chk_params) add_param_to_box (GTK_BOX (param_vbox),rfx,i,TRUE);
      has_param=TRUE;
    }
  }

  if (!chk_params) {
    if (!has_param) {
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (param_vbox), hbox, FALSE, FALSE, 20);
      add_fill_to_box(GTK_BOX(hbox));
      add_label_to_box(GTK_BOX(hbox),FALSE,_("No parameters"));
      add_fill_to_box(GTK_BOX(hbox));
    }
    
    scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (scrolledwindow);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), top_hbox);
    gtk_box_pack_start (GTK_BOX (top_vbox), scrolledwindow, TRUE, TRUE, 0);
    if (mainw->multitrack==NULL||rfx->status!=RFX_STATUS_WEED) 
      gtk_widget_set_size_request (scrolledwindow, RFX_WINSIZE_H, RFX_WINSIZE_V);
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(gtk_bin_get_child (GTK_BIN (scrolledwindow)), GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolledwindow))),GTK_SHADOW_IN);
  }

  if (mainw->multitrack==NULL&&rfx->status==RFX_STATUS_WEED&&rfx->is_template) {
    weed_plant_t *filter=weed_get_plantptr_value((weed_plant_t *)rfx->source,"filter_class",&error);
    if (enabled_in_channels(filter,FALSE)==0&&enabled_out_channels(filter,FALSE)>0&&has_video_chans_out(filter,TRUE)) {
      // out channel size(s) and target_fps for generators
      if (!chk_params) add_sizes(GTK_BOX(top_vbox),TRUE,rfx);
      has_param=TRUE;
    }
  }

  mainw->block_param_updates=FALSE;

  return has_param;
}



#define GIW_KNOB_WIDTH 40
#define GIW_KNOB_HEIGHT 40





gboolean add_param_to_box (GtkBox *box, lives_rfx_t *rfx, gint pnum, gboolean add_slider) {
  // box here is vbox inside top_hbox inside top_dialog

  // add paramter pnum for rfx to box

  GtkWidget *label;
  GtkWidget *labelcname;
  GtkWidget *checkbutton;
  GtkWidget *radiobutton;
  GtkWidget *spinbutton;
  GtkWidget *scale;
  GtkWidget *spinbutton_red;
  GtkWidget *spinbutton_green;
  GtkWidget *spinbutton_blue;
  GtkWidget *cbutton;
  GtkWidget *entry=NULL;
  GtkWidget *hbox;
  GtkWidget *combo;
  GtkWidget *dlabel=NULL;
  GtkWidget *textview=NULL;
  GtkWidget *scrolledwindow;
  GtkAdjustment *spinbutton_adj;

  GtkTextBuffer *textbuffer=NULL;

  lives_param_t *param;
  lives_widget_group_t *group;
  GSList *rbgroup;
  gulong spinfunc,blockfunc;
  gchar *name;
  gchar *txt,*tmp,*tmp2;
  gboolean use_mnemonic;

  lives_colRGB24_t rgb;
  GdkColor colr;
  char *disp_string;
  gboolean was_num=FALSE;

  if (pnum>=rfx->num_params) {
    add_label_to_box (box,FALSE,(_("Invalid parameter")));
    return FALSE;
  }

  param=&rfx->params[pnum];

  name=g_strdup_printf ("%s",param->label);
  use_mnemonic=param->use_mnemonic;
  switch (param->type) {
  case LIVES_PARAM_BOOL :
    if (!param->group) {

      if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
      else {
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
      }
      
      if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
	dlabel=gtk_label_new (g_strdup_printf("(%s)",_ (disp_string)));
	if (palette->style&STYLE_1) {
	  gtk_widget_modify_fg(dlabel, GTK_STATE_NORMAL, &palette->normal_fore);
	}
	weed_free(disp_string);
	gtk_box_pack_start (GTK_BOX (hbox), dlabel, FALSE, FALSE, 10);
	param->widgets[1]=dlabel;
      }


      checkbutton=lives_standard_check_button_new(name,use_mnemonic,(LiVESBox *)hbox,param->desc);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), get_bool_param(param->value));
      g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			      G_CALLBACK (after_boolean_param_toggled),
			      (gpointer)rfx);
      
      // store parameter so we know whose trigger to use
      g_object_set_data (G_OBJECT (checkbutton),"param_number",GINT_TO_POINTER (pnum));
      param->widgets[0]=checkbutton;
      if (param->hidden) gtk_widget_set_sensitive(checkbutton,FALSE);
    }
    else {
      group=get_group(rfx,param);

      if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
      else {
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
      }
      
      if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
	dlabel=gtk_label_new (g_strdup_printf("(%s)",_ (disp_string)));
	if (palette->style&STYLE_1) {
	  gtk_widget_modify_fg(dlabel, GTK_STATE_NORMAL, &palette->normal_fore);
	}
	weed_free(disp_string);
	gtk_box_pack_start (GTK_BOX (hbox), dlabel, FALSE, FALSE, 10);
	param->widgets[1]=dlabel;
      }

      if (group!=NULL) rbgroup=group->rbgroup;
      else rbgroup=NULL;

      radiobutton=lives_standard_radio_button_new(name,use_mnemonic,rbgroup,(LiVESBox *)hbox,param->desc);

      if (group==NULL) {
	if (rfx->status==RFX_STATUS_WEED) {
	  usrgrp_to_livesgrp[1]=add_usrgrp_to_livesgrp (usrgrp_to_livesgrp[1], 
							gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton)), 
							param->group);
	}
	else {
	  usrgrp_to_livesgrp[0]=add_usrgrp_to_livesgrp (usrgrp_to_livesgrp[0], 
							gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton)), 
							param->group);
	}
      }

      group=get_group(rfx,param);

      if (group!=NULL) {
	group->rbgroup=gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
	if (get_bool_param(param->value)) {
	  group->active_param=pnum+1;
	}
      }
      else LIVES_WARN("Button group was NULL");

      g_signal_connect_after (GTK_OBJECT (radiobutton), "toggled",
			      G_CALLBACK (after_boolean_param_toggled),
			      (gpointer)rfx);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton), get_bool_param(param->value));

      // store parameter so we know whose trigger to use
      g_object_set_data (G_OBJECT (radiobutton),"param_number",GINT_TO_POINTER (pnum));
      param->widgets[0]=radiobutton;
      if (param->hidden) gtk_widget_set_sensitive(radiobutton,FALSE);
      }
    break;
  case LIVES_PARAM_NUM :
    was_num=TRUE;

    if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
    else {
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
    }

    if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
      dlabel=gtk_label_new (g_strdup_printf("%s",_ (disp_string)));
      weed_free(disp_string);
      gtk_box_pack_start (GTK_BOX (hbox), dlabel, FALSE, FALSE, 10);
      param->widgets[1]=dlabel;
    }

    if (param->dp) {
      spinbutton=lives_standard_spin_button_new(name, use_mnemonic, get_double_param(param->value), param->min, 
						param->max, param->step_size, param->step_size, param->dp, 
						(LiVESBox *)hbox, param->desc);
    }
    else {
      spinbutton=lives_standard_spin_button_new(name, use_mnemonic, (double)get_int_param(param->value), param->min, 
						param->max, param->step_size, param->step_size, param->dp, 
						(LiVESBox *)hbox, param->desc);
    }


    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinbutton),param->wrap);

    spinfunc=g_signal_connect_after (GTK_OBJECT (spinbutton), "value_changed",
			    G_CALLBACK (after_param_value_changed),
			    (gpointer)rfx);
    g_object_set_data(G_OBJECT(spinbutton),"spinfunc",(gpointer)spinfunc);
    
    // store parameter so we know whose trigger to use
    g_object_set_data (G_OBJECT (spinbutton),"param_number",GINT_TO_POINTER (pnum));
    param->widgets[0]=spinbutton;
    if (param->hidden) gtk_widget_set_sensitive(spinbutton,FALSE);


    if (add_slider) {
      spinbutton_adj=gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spinbutton));

#ifdef ENABLE_GIW
      if (!prefs->lamp_buttons) {
#endif
	scale=gtk_hscale_new(GTK_ADJUSTMENT(spinbutton_adj));
	gtk_scale_set_draw_value(GTK_SCALE(scale),FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
#ifdef ENABLE_GIW
      }
      else {
	scale=giw_knob_new(GTK_ADJUSTMENT(spinbutton_adj));
	gtk_widget_set_size_request(scale,GIW_KNOB_WIDTH,GIW_KNOB_HEIGHT);
	giw_knob_set_legends_digits(GIW_KNOB(scale),0);
	if (palette->style&STYLE_1) {
	  gtk_widget_modify_bg (scale, GTK_STATE_NORMAL, &palette->normal_back);
	  gtk_widget_modify_text(scale, GTK_STATE_NORMAL, &palette->normal_back);
	  gtk_widget_modify_fg(scale, GTK_STATE_NORMAL, &palette->normal_fore);
	}
	gtk_box_pack_start (GTK_BOX (hbox), scale, FALSE, FALSE, 10);
	add_fill_to_box (GTK_BOX (hbox));
      }
#endif
      if (param->desc!=NULL) gtk_widget_set_tooltip_text( scale, param->desc);
    }
    break;
    
  case LIVES_PARAM_COLRGB24 :
    get_colRGB24_param(param->value,&rgb);

    if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
    else {
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
    }

    // colsel button

    colr.red=rgb.red<<8;
    colr.green=rgb.green<<8;
    colr.blue=rgb.blue<<8;

    cbutton = gtk_color_button_new_with_color(&colr);
    gtk_color_button_set_title (GTK_COLOR_BUTTON(cbutton),_("LiVES: - Select Colour"));

    g_object_set_data (G_OBJECT (cbutton),"param_number",GINT_TO_POINTER (pnum));
    if (param->desc!=NULL) gtk_widget_set_tooltip_text( cbutton, param->desc);
    else gtk_widget_set_tooltip_text( cbutton, (_("Click to set the colour")));

    if (use_mnemonic) {
      labelcname=gtk_label_new_with_mnemonic (_(name));
      gtk_label_set_mnemonic_widget (GTK_LABEL (labelcname),cbutton);
    }
    else labelcname=gtk_label_new (_(name));
    if (param->desc!=NULL) gtk_widget_set_tooltip_text( labelcname, param->desc);

    gtk_label_set_justify (GTK_LABEL (labelcname), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(labelcname, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), labelcname, FALSE, FALSE, 10);

    spinbutton_red = lives_standard_spin_button_new((tmp=g_strdup(_("_Red"))), TRUE, rgb.red, 0., 255., 1., 1., 0, 
						    (LiVESBox *)hbox, (tmp2=g_strdup(_("The red value (0 - 255)"))));
    g_free(tmp);
    g_free(tmp2);
    spinbutton_green = lives_standard_spin_button_new((tmp=g_strdup(_("_Green"))), TRUE, rgb.green, 0., 255., 1., 1., 0, 
						      (LiVESBox *)hbox, (tmp2=g_strdup(_("The green value (0 - 255)"))));
    g_free(tmp);
    g_free(tmp2);
    spinbutton_blue = lives_standard_spin_button_new((tmp=g_strdup(_("_Blue"))), TRUE, rgb.blue, 0., 255., 1., 1., 0, 
						     (LiVESBox *)hbox, (tmp2=g_strdup(_("The blue value (0 - 255)"))));
    g_free(tmp);
    g_free(tmp2);

    gtk_box_pack_start (GTK_BOX (hbox), cbutton, TRUE, TRUE, 20);

    g_signal_connect (GTK_OBJECT (cbutton), "color-set",
		      G_CALLBACK (on_pwcolsel),
		      (gpointer)rfx);
    
    g_signal_connect_after (GTK_OBJECT (spinbutton_red), "value_changed",
			    G_CALLBACK (after_param_red_changed),
			    (gpointer)rfx);
    g_signal_connect_after (GTK_OBJECT (spinbutton_green), "value_changed",
			    G_CALLBACK (after_param_green_changed),
			    (gpointer)rfx);
    g_signal_connect_after (GTK_OBJECT (spinbutton_blue), "value_changed",
			    G_CALLBACK (after_param_blue_changed),
			    (gpointer)rfx);
    
    // store parameter so we know whose trigger to use
    g_object_set_data (G_OBJECT (spinbutton_red),"param_number",GINT_TO_POINTER (pnum));
    g_object_set_data (G_OBJECT (spinbutton_green),"param_number",GINT_TO_POINTER (pnum));
    g_object_set_data (G_OBJECT (spinbutton_blue),"param_number",GINT_TO_POINTER (pnum));
    
    param->widgets[0]=spinbutton_red;
    param->widgets[1]=spinbutton_green;
    param->widgets[2]=spinbutton_blue;
    //param->widgets[3]=spinbutton_alpha;
    param->widgets[4]=cbutton;

    if (param->hidden) {
      gtk_widget_set_sensitive(spinbutton_red,FALSE);
      gtk_widget_set_sensitive(spinbutton_green,FALSE);
      gtk_widget_set_sensitive(spinbutton_blue,FALSE);
      //gtk_widget_set_sensitive(spinbutton_alpha,FALSE);
      gtk_widget_set_sensitive(cbutton,FALSE);
    }
    break;

  case LIVES_PARAM_STRING:

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 0);
    gtk_box_set_homogeneous(GTK_BOX(box),FALSE);

    if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
      if (param->max==0.) txt=g_strdup (disp_string);
      else txt=g_strndup (disp_string,(gint)param->max);
      weed_free(disp_string);
    }
    else {
      if (param->max==0.) txt=g_strdup ((gchar *)param->value);
      else txt=g_strndup ((gchar *)param->value,(gint)param->max);
    }

    if ((gint)param->max>RFX_TEXT_MAGIC||param->max==0.) {
      param->widgets[0] = textview = gtk_text_view_new ();
      if (param->hidden) gtk_widget_set_sensitive(textview,FALSE);
      if (param->desc!=NULL) gtk_widget_set_tooltip_text( textview, param->desc);
      textbuffer=gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
      g_object_set_data(G_OBJECT(textview),"textbuffer",(gpointer)textbuffer);


      gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), TRUE);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
      gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), TRUE);

      gtk_text_buffer_set_text (textbuffer, txt, -1);
    }
    else {
      param->widgets[0]=entry=gtk_entry_new();
      if (param->hidden) gtk_widget_set_sensitive(entry,FALSE);
      if (param->desc!=NULL) gtk_widget_set_tooltip_text( entry, param->desc);
      gtk_entry_set_text (GTK_ENTRY (entry),txt);
      gtk_entry_set_max_length(GTK_ENTRY (entry),(gint)param->max);
      gtk_entry_set_width_chars (GTK_ENTRY (entry),(gint)param->max);
      gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    }
    g_free (txt);

    if (use_mnemonic) label = gtk_label_new_with_mnemonic (_(name));
    else label = gtk_label_new (_(name));
    if (param->desc!=NULL) gtk_widget_set_tooltip_text( label, param->desc);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);

    blockfunc=g_signal_connect_after (G_OBJECT (hbox), "set-focus-child", G_CALLBACK (after_param_text_focus_changed), 
				      (gpointer) rfx);

    if ((gint)param->max>RFX_TEXT_MAGIC||param->max==0.) {
      scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      
      gtk_container_add (GTK_CONTAINER (scrolledwindow), textview);
      if (use_mnemonic) gtk_label_set_mnemonic_widget (GTK_LABEL (label),textview);
      gtk_box_pack_start (GTK_BOX (hbox), scrolledwindow, TRUE, TRUE, 10);
    }
    else {
      if (use_mnemonic) gtk_label_set_mnemonic_widget (GTK_LABEL (label),entry);
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 10);
    }

    g_object_set_data(G_OBJECT(hbox),"textwidget",(gpointer)param->widgets[0]);
    g_object_set_data(G_OBJECT(param->widgets[0]),"blockfunc",(gpointer)blockfunc);
    g_object_set_data (G_OBJECT (param->widgets[0]),"param_number",GINT_TO_POINTER (pnum));
    g_object_set_data (G_OBJECT (param->widgets[0]),"rfx",rfx);


    break;

  case LIVES_PARAM_STRING_LIST:

    combo=lives_standard_combo_new(name, use_mnemonic, param->list, (LiVESBox *)box, param->desc);

    if (rfx->status==RFX_STATUS_WEED&&(disp_string=get_weed_display_string((weed_plant_t *)rfx->source,pnum))!=NULL) {
      gtk_entry_set_text (GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))),disp_string);
      weed_free(disp_string);
    }
    else if (param->list!=NULL) {
      gtk_entry_set_text (GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))), 
			  (gchar *)g_list_nth_data (param->list,get_int_param (param->value)));
    }

    blockfunc=g_signal_connect_after (G_OBJECT (combo), "changed", 
				      G_CALLBACK (after_string_list_changed), (gpointer) rfx);

    g_object_set_data(G_OBJECT(combo),"blockfunc",(gpointer)blockfunc);
 
    // store parameter so we know whose trigger to use
    g_object_set_data (G_OBJECT (combo),"param_number",GINT_TO_POINTER (pnum));
    param->widgets[0]=combo;
    if (param->hidden) gtk_widget_set_sensitive(combo,FALSE);

    break;

  default:
    break;

  }
  
  // see if there were any 'special' hints
  //mainw->block_param_updates=FALSE; // need to keep blocked until last param widget has been created
  check_for_special (param,pnum,GTK_BOX(GTK_WIDGET(box)->parent),rfx);
  //mainw->block_param_updates=TRUE;

  g_free (name);
  return was_num;
}

void add_hsep_to_box (GtkBox *box) {
  GtkWidget *hseparator = gtk_hseparator_new ();
  gtk_box_pack_start (box, hseparator, TRUE, TRUE, 0);
  gtk_widget_show(hseparator);
}

void add_fill_to_box (GtkBox *box) {
  GtkWidget *blank_label = gtk_label_new ("");
  gtk_box_pack_start (box, blank_label, TRUE, TRUE, 0);
  gtk_widget_show(blank_label);
}

void add_label_to_box (GtkBox *box, gboolean do_trans, const gchar *text) {
  GtkWidget *label;

  gtk_box_set_homogeneous(GTK_BOX(box),FALSE);

  if (do_trans) {
    char *markup;
    markup=g_markup_printf_escaped("<span weight=\"bold\" style=\"italic\">%s</span>",_(text));
    label = gtk_label_new(NULL);
    gtk_label_set_markup_with_mnemonic (GTK_LABEL(label),markup);
    g_free(markup);
  }
  else label = gtk_label_new_with_mnemonic (text);

  gtk_box_pack_start (box, label, FALSE, FALSE, 10);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show(label);
}

GSList *add_usrgrp_to_livesgrp (GSList *u2l, GSList *rbgroup, gint usr_number) {
  lives_widget_group_t *wgroup=(lives_widget_group_t *)g_malloc (sizeof(lives_widget_group_t));
  wgroup->usr_number=usr_number;
  wgroup->rbgroup=rbgroup;
  wgroup->active_param=0;
  u2l=g_slist_append (u2l, (gpointer)wgroup);
  return u2l;
}




lives_widget_group_t *livesgrp_from_usrgrp (GSList *u2l, gint usrgrp) {
  int i;
  lives_widget_group_t *group;

  for (i=0;i<g_slist_length (u2l);i++) {
    group=(lives_widget_group_t *)g_slist_nth_data (u2l,i);
    if (group->usr_number==usrgrp) return group;
  }
  return NULL;
}








void
after_boolean_param_toggled        (GtkToggleButton *togglebutton,
				    lives_rfx_t *         rfx)
{
  gint param_number=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (togglebutton),"param_number"));
  lives_param_t *param=&rfx->params[param_number];
  gboolean old_bool=get_bool_param(param->value),new_bool;
  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked when we update visually

  set_bool_param(param->value,(new_bool=gtk_toggle_button_get_active (togglebutton)));

  if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;
    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      char *disp_string;
      weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE);
      int index=0,numvals;
      int *valis;


      if (mainw->multitrack!=NULL&&mainw->multitrack->track_index!=-1&&is_perchannel_multi(rfx,param_number)) 
	index=mainw->multitrack->track_index;
      numvals=weed_leaf_num_elements(wparam,"value");

      if (index>=numvals) {
	weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
	fill_param_vals_to(paramtmpl,wparam,param_number,WEED_HINT_SWITCH,index);
	numvals=index+1;
      }

      valis=weed_get_boolean_array(wparam,"value",&error);
      valis[index]=new_bool;
      pthread_mutex_lock(&mainw->data_mutex);
      weed_set_boolean_array(wparam,"value",numvals,valis);
      pthread_mutex_unlock(&mainw->data_mutex);
      copyto=set_copy_to(inst,param_number,TRUE);

      weed_free(valis);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	// if we are recording, add this change to our event_list
	rec_param_change(inst,param_number);
	if (copyto!=-1) rec_param_change(inst,copyto);
      }

      disp_string=get_weed_display_string(inst,param_number);
      if (disp_string!=NULL) {
	gtk_label_set_text(GTK_LABEL(param->widgets[1]),disp_string);
	weed_free(disp_string);
      }
      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
	weed_reinit_effect(inst);
      }

    }
  }
  if (get_bool_param(param->value)!=old_bool&&param->onchange) {
    param->change_blocked=TRUE;
    do_onchange (G_OBJECT (togglebutton), rfx);
    while (g_main_context_iteration(NULL,FALSE));
    param->change_blocked=FALSE;
  }
  if (copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}



void
after_param_value_changed           (GtkSpinButton   *spinbutton,
				     lives_rfx_t *rfx) {
  gint param_number=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (spinbutton),"param_number"));
  lives_param_t *param=&rfx->params[param_number];
  gdouble new_double,old_double=0.;
  gint new_int,old_int=0;
  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked when we update visually

  if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,TRUE);

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
  }
  else {
    old_int=get_int_param(param->value);
  }

  if (param->dp>0) {
    set_double_param(param->value,(new_double=gtk_spin_button_get_value(GTK_SPIN_BUTTON(spinbutton))));

    if (rfx->status==RFX_STATUS_WEED) {
      int error;
      weed_plant_t *inst=(weed_plant_t *)rfx->source;
      if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
	weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE);
	int index=0,numvals;
	double *valds;

	if (mainw->multitrack!=NULL&&mainw->multitrack->track_index!=-1&&is_perchannel_multi(rfx,param_number)) 
	  index=mainw->multitrack->track_index;
	numvals=weed_leaf_num_elements(wparam,"value");
	if (index>=numvals) {
	  weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
	  fill_param_vals_to(paramtmpl,wparam,param_number,WEED_HINT_FLOAT,index);
	  numvals=index+1;
	}
	
	valds=weed_get_double_array(wparam,"value",&error);
	valds[index]=new_double;
	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_double_array(wparam,"value",numvals,valds);
	pthread_mutex_unlock(&mainw->data_mutex);
	copyto=set_copy_to(inst,param_number,TRUE);
	weed_free(valds);

      }
    }
  }
  else {
    set_int_param(param->value,(new_int=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton))));

    if (rfx->status==RFX_STATUS_WEED) {
      int error;
      weed_plant_t *inst=(weed_plant_t *)rfx->source;
      if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
	weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE);
	int index=0,numvals;
	int *valis;
	
	if (mainw->multitrack!=NULL&&mainw->multitrack->track_index!=-1&&is_perchannel_multi(rfx,param_number)) 
	  index=mainw->multitrack->track_index;
	numvals=weed_leaf_num_elements(wparam,"value");
	if (index>=numvals) {
	  weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
	  fill_param_vals_to(paramtmpl,wparam,param_number,WEED_HINT_INTEGER,index);
	  numvals=index+1;
	}
	
	valis=weed_get_int_array(wparam,"value",&error);
	valis[index]=new_int;
	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_int_array(wparam,"value",numvals,valis);
	pthread_mutex_unlock(&mainw->data_mutex);
	copyto=set_copy_to(inst,param_number,TRUE);
	weed_free(valis);
      }
    }
  }

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;
    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      char *disp_string;

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	// if we are recording, add this change to our event_list
	rec_param_change(inst,param_number);
	if (copyto!=-1) rec_param_change(inst,copyto);
      }

      disp_string=get_weed_display_string(inst,param_number);
      if (disp_string!=NULL) {
	gtk_label_set_text(GTK_LABEL(param->widgets[1]),disp_string);
	weed_free(disp_string);
      }
      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
	weed_reinit_effect(inst);
      }

    }
  }

  if (((param->dp>0&&(get_double_param(param->value)!=old_double))||(param->dp==0&&
								     (get_int_param(param->value)!=old_int)))&&
      param->onchange) {
    param->change_blocked=TRUE;
    do_onchange (G_OBJECT (spinbutton), rfx);
    while (g_main_context_iteration(NULL,FALSE));
    param->change_blocked=FALSE;
  }
  if (copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


void update_weed_color_value(weed_plant_t *param, int pnum, int c1, int c2, int c3, int c4) {
  int error;
  int cols[4]={c1,c2,c3,c4};
  double colds[4];
  weed_plant_t *ptmpl;
  int cspace;
  int rmax,rmin,gmax,gmin,bmax,bmin;
  int *maxs=NULL,*mins=NULL;
  double rmaxd,rmind,gmaxd,gmind,bmaxd,bmind;
  double *maxds=NULL,*minds=NULL;
  gboolean is_default=(weed_get_int_value(param,"type",&error)==WEED_PLANT_PARAMETER_TEMPLATE);

  gboolean is_int;

  if (mainw->block_param_updates) return; // updates are blocked when we update visually

  if (is_default) ptmpl=param;  // called only from rte_set_defs_ok
  else ptmpl=weed_get_plantptr_value(param,"template",&error);

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
	weed_free(maxs);
      }
      else rmax=gmax=bmax=weed_get_int_value(ptmpl,"max",&error);
      if (weed_leaf_num_elements(ptmpl,"min")==3) {
	mins=weed_get_int_array(ptmpl,"min",&error);
	rmin=mins[0];
	gmin=mins[1];
	bmin=mins[2];
	weed_free(mins);
      }
      else rmin=gmin=bmin=weed_get_int_value(ptmpl,"min",&error);

      cols[0]=rmin+(int)((double)cols[0]/255.*(double)(rmax-rmin));
      cols[1]=gmin+(int)((double)cols[1]/255.*(double)(gmax-gmin));
      cols[2]=bmin+(int)((double)cols[2]/255.*(double)(bmax-bmin));
      if (is_default) weed_set_int_array(ptmpl,"host_default",3,cols);
      else {
	int index=0,numvals;
	int *valis;
	
	if (mainw->multitrack!=NULL&&mainw->multitrack->track_index!=-1&&is_perchannel_multiw(param)) 
	  index=mainw->multitrack->track_index;
	numvals=weed_leaf_num_elements(param,"value");
	if (index*3>=numvals) {
	  weed_plant_t *paramtmpl=weed_get_plantptr_value(param,"template",&error);
	  fill_param_vals_to(paramtmpl,param,pnum,WEED_HINT_COLOR,index);
	  numvals=(index+1)*3;
	}
	
	valis=weed_get_int_array(param,"value",&error);
	valis[index*3]=cols[0];
	valis[index*3+1]=cols[1];
	valis[index*3+2]=cols[2];
	weed_set_int_array(param,"value",numvals,valis);
	weed_free(valis);
      }
      break;
    }
    else {
      // double
      if (weed_leaf_num_elements(ptmpl,"max")==3) {
	maxds=weed_get_double_array(ptmpl,"max",&error);
	rmaxd=maxds[0];
	gmaxd=maxds[1];
	bmaxd=maxds[2];
	weed_free(maxds);
      }
      else rmaxd=gmaxd=bmaxd=weed_get_double_value(ptmpl,"max",&error);
      if (weed_leaf_num_elements(ptmpl,"min")==3) {
	minds=weed_get_double_array(ptmpl,"min",&error);
	rmind=minds[0];
	gmind=minds[1];
	bmind=minds[2];
	weed_free(minds);
      }
      else rmind=gmind=bmind=weed_get_double_value(ptmpl,"min",&error);
      colds[0]=rmind+(double)cols[0]/255.*(rmaxd-rmind);
      colds[1]=gmind+(double)cols[1]/255.*(gmaxd-gmind);
      colds[2]=bmind+(double)cols[2]/255.*(bmaxd-bmind);
      if (is_default) weed_set_double_array(ptmpl,"host_default",3,colds);
      else {
	int index=0,numvals;
	double *valds;
	
	if (mainw->multitrack!=NULL&&mainw->multitrack->track_index!=-1&&is_perchannel_multiw(param)) 
	  index=mainw->multitrack->track_index;
	numvals=weed_leaf_num_elements(param,"value");
	if (index*3>=numvals) {
	  weed_plant_t *paramtmpl=weed_get_plantptr_value(param,"template",&error);
	  fill_param_vals_to(paramtmpl,param,pnum,WEED_HINT_COLOR,index);
	  numvals=(index+1)*3;
	}
	
	valds=weed_get_double_array(param,"value",&error);
	valds[index*3]=colds[0];
	valds[index*3+1]=colds[1];
	valds[index*3+2]=colds[2];
	weed_set_double_array(param,"value",numvals,valds);
	weed_free(valds);
      }
    }
    break;
  }

}


void
after_param_red_changed           (GtkSpinButton   *spinbutton,
				   lives_rfx_t *rfx) {
  gint param_number=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (spinbutton),"param_number"));
  lives_param_t *param=&rfx->params[param_number];
  lives_colRGB24_t old_value;
  gint new_red;
  GdkColor colr;
  GtkWidget *cbutton;
  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked when we update visually

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }

  get_colRGB24_param(param->value,&old_value);
  new_red=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
  set_colRGB24_param(param->value,new_red,old_value.green,old_value.blue);

  colr.red=new_red<<8;
  colr.green=old_value.green<<8;
  colr.blue=old_value.blue<<8;
  cbutton=param->widgets[4];
  gtk_color_button_set_color(GTK_COLOR_BUTTON(cbutton),&colr);


  if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;

    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE)  {
      update_weed_color_value(weed_inst_in_param(inst,param_number,FALSE),param_number,
			      new_red,old_value.green,old_value.blue,0);
      copyto=set_copy_to(inst,param_number,TRUE);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	// if we are recording, add this change to our event_list
	rec_param_change(inst,param_number);
	if (copyto!=-1) rec_param_change(inst,copyto);
      }
      
      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
	weed_reinit_effect(inst);
      }
    }
  }

  if (new_red!=old_value.red&&param->onchange) {
    param->change_blocked=TRUE;
    do_onchange (G_OBJECT (spinbutton), rfx);
    while (g_main_context_iteration(NULL,FALSE));
    param->change_blocked=FALSE;
  }
  if (copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


void
after_param_green_changed           (GtkSpinButton   *spinbutton,
				     lives_rfx_t *rfx) {
  gint param_number=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (spinbutton),"param_number"));
  lives_param_t *param=&rfx->params[param_number];
  lives_colRGB24_t old_value;
  gint new_green;
  GdkColor colr;
  GtkWidget *cbutton;
  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked when we update visually

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }

  get_colRGB24_param(param->value,&old_value);
  new_green=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
  set_colRGB24_param(param->value,old_value.red,new_green,old_value.blue);

  colr.red=old_value.red<<8;
  colr.green=new_green<<8;
  colr.blue=old_value.blue<<8;
  cbutton=param->widgets[4];
  gtk_color_button_set_color(GTK_COLOR_BUTTON(cbutton),&colr);

  if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;

    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      update_weed_color_value(weed_inst_in_param(inst,param_number,FALSE),
			      param_number,old_value.red,new_green,old_value.blue,0);

      copyto=set_copy_to(inst,param_number,TRUE);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	// if we are recording, add this change to our event_list
	rec_param_change(inst,param_number);
	if (copyto!=-1) rec_param_change(inst,copyto);
      }
      
      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
	weed_reinit_effect(inst);
      }
    }
  }

  if (new_green!=old_value.green&&param->onchange) {
    param->change_blocked=TRUE;
    do_onchange (G_OBJECT (spinbutton), rfx);
    while (g_main_context_iteration(NULL,FALSE));
    param->change_blocked=FALSE;
  }
  if (copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}

void
after_param_blue_changed           (GtkSpinButton   *spinbutton,
				    lives_rfx_t *rfx) {
  gint param_number=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (spinbutton),"param_number"));
  lives_param_t *param=&rfx->params[param_number];
  lives_colRGB24_t old_value;
  gint new_blue;
  GdkColor colr;
  GtkWidget *cbutton;
  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked when we update visually

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }

  get_colRGB24_param(param->value,&old_value);
  new_blue=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
  set_colRGB24_param(param->value,old_value.red,old_value.green,new_blue);

  colr.red=old_value.red<<8;
  colr.green=old_value.green<<8;
  colr.blue=new_blue<<8;
  cbutton=param->widgets[4];
  gtk_color_button_set_color(GTK_COLOR_BUTTON(cbutton),&colr);


  if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;

    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      update_weed_color_value(weed_inst_in_param(inst,param_number,FALSE),param_number,
			      old_value.red,old_value.green,new_blue,0);
      copyto=set_copy_to(inst,param_number,TRUE);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	// if we are recording, add this change to our event_list
	rec_param_change(inst,param_number);
	if (copyto!=-1) rec_param_change(inst,copyto);
      }
      
      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
	weed_reinit_effect(inst);
      }
    }
  }

  if (new_blue!=old_value.blue&&param->onchange) {
    param->change_blocked=TRUE;
    do_onchange (G_OBJECT (spinbutton), rfx);
    while (g_main_context_iteration(NULL,FALSE));
    param->change_blocked=FALSE;
  }
  if (copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


void
after_param_alpha_changed           (GtkSpinButton   *spinbutton,
				     lives_rfx_t *rfx) {
  // not used yet
  gint param_number=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (spinbutton),"param_number"));
  lives_param_t *param=&rfx->params[param_number];
  lives_colRGBA32_t old_value;
  gint new_alpha=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinbutton));
  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked when we update visually

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    copyto=set_copy_to((weed_plant_t *)rfx->source,param_number,FALSE);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }

  get_colRGBA32_param(param->value,&old_value);
  
  if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,TRUE);


  set_colRGBA32_param(param->value,old_value.red,old_value.green,old_value.blue,new_alpha);

  if (rfx->status==RFX_STATUS_WEED&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&
      (prefs->rec_opts&REC_EFFECTS)) {
    // if we are recording, add this change to our event_list
    rec_param_change((weed_plant_t *)rfx->source,param_number);
    if (copyto!=-1) rec_param_change((weed_plant_t *)rfx->source,copyto);
  }


  if (new_alpha!=old_value.alpha&&param->onchange) {
    param->change_blocked=TRUE;
    do_onchange (G_OBJECT (spinbutton), rfx);
    while (g_main_context_iteration(NULL,FALSE));
    param->change_blocked=FALSE;
  }
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}


gboolean after_param_text_focus_changed (GtkWidget *hbox, GtkWidget *child, lives_rfx_t *rfx) {
  // for non realtime effects
  // we don't usually want to run the trigger every single time the user presses a key in a text widget
  // so we only update when the user clicks OK or focusses out of the widget

  GtkWidget *textwidget;

  if (mainw->multitrack!=NULL) {
    if (child!=NULL)
      gtk_window_remove_accel_group(GTK_WINDOW(mainw->multitrack->window),mainw->multitrack->accel_group);
    else
      gtk_window_add_accel_group(GTK_WINDOW(mainw->multitrack->window),mainw->multitrack->accel_group);

    // effects and generators make their own arrangements to be updated in multitrack
    // rfx scraps don't  (for example vpp advanced in prefs)
    if (rfx->status!=RFX_STATUS_SCRAP) return FALSE;
  }

  if (hbox!=NULL) {
    if (mainw->textwidget_focus!=NULL) {
      textwidget=(GtkWidget *)g_object_get_data (G_OBJECT (mainw->textwidget_focus),"textwidget");
      textwidget=(GtkWidget *)g_object_get_data (G_OBJECT (textwidget),"textbuffer");
      after_param_text_changed(textwidget,rfx);
    }
    mainw->textwidget_focus=hbox;
    return FALSE;
  }

  // for non-realtime, function is only called when focus leaves the textwidget
  textwidget=(GtkWidget *)g_object_get_data (G_OBJECT (mainw->textwidget_focus),"textwidget");
  after_param_text_changed(textwidget,rfx);
  return FALSE;
}

void 
after_param_text_changed (GtkWidget *textwidget, lives_rfx_t *rfx) {
  gint param_number=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (textwidget),"param_number"));
  lives_param_t *param=&rfx->params[param_number];
  gchar *old_text=(gchar *)param->value;
  int copyto=-1;

  if (mainw->block_param_updates) return; // updates are blocked when we update visually


  if ((gint)param->max>RFX_TEXT_MAGIC||param->max==0.) {
    GtkTextIter start_iter,end_iter;
 
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textwidget),&start_iter);
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(textwidget),&end_iter);
    
    param->value=gtk_text_buffer_get_text (GTK_TEXT_BUFFER(textwidget),&start_iter,&end_iter,FALSE);
  }
  else {
    param->value=g_strdup (gtk_entry_get_text (GTK_ENTRY (textwidget)));
  }

  if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error,i;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;
    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      char *disp_string=get_weed_display_string(inst,param_number);
      weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE);
      int index=0,numvals;
      char **valss;

      if (mainw->multitrack!=NULL&&mainw->multitrack->track_index!=-1&&is_perchannel_multi(rfx,param_number)) 
	index=mainw->multitrack->track_index;
      numvals=weed_leaf_num_elements(wparam,"value");
      if (index>=numvals) {
	weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
	fill_param_vals_to(paramtmpl,wparam,param_number,WEED_HINT_INTEGER,index);
	numvals=index+1;
      }
      
      valss=weed_get_string_array(wparam,"value",&error);
      valss[index]=g_strdup((gchar *)param->value);
      pthread_mutex_lock(&mainw->data_mutex);
      weed_set_string_array(wparam,"value",numvals,valss);
      pthread_mutex_unlock(&mainw->data_mutex);
      copyto=set_copy_to(inst,param_number,TRUE);
      for (i=0;i<numvals;i++) weed_free(valss[i]);
      weed_free(valss);

      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	// if we are recording, add this change to our event_list
	rec_param_change(inst,param_number);
	if (copyto!=-1) rec_param_change(inst,copyto);
      }

      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
	weed_reinit_effect(inst);
      }

      if (disp_string!=NULL) {
	gulong blockfunc=(gulong)g_object_get_data(G_OBJECT(textwidget),"blockfunc");
	g_signal_handler_block(textwidget,blockfunc);
	if ((gint)param->max>RFX_TEXT_MAGIC||param->max==0.) {
	  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (textwidget), (gchar *)param->value, -1);
	}
	else {
	  gtk_entry_set_text(GTK_ENTRY(textwidget),disp_string);
	}
	g_signal_handler_unblock(textwidget,blockfunc);
	weed_free(disp_string);
      }

    }
  }

  if (strcmp (old_text,(gchar *)param->value)&&param->onchange) {
    param->change_blocked=TRUE;
    do_onchange (G_OBJECT (textwidget), rfx);
    while (g_main_context_iteration(NULL,FALSE));
    param->change_blocked=FALSE;
  }
  g_free (old_text);
  if (copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;

}


void 
after_string_list_changed (GtkComboBox *combo, lives_rfx_t *rfx) {
  gint param_number=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo),"param_number"));
  lives_param_t *param=&rfx->params[param_number];
  gint old_index=get_int_param(param->value);
  char *txt=lives_combo_get_active_text(combo);
  gint new_index=lives_list_index(param->list,txt);
  int copyto=-1;

  g_free(txt);

  if (mainw->block_param_updates) return; // updates are blocked when we update visually

  if (new_index==-1) return;

  set_int_param(param->value,new_index);

  if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,TRUE);

  if (rfx->status==RFX_STATUS_WEED) {
    int error;
    weed_plant_t *inst=(weed_plant_t *)rfx->source;
    if (inst!=NULL&&weed_get_int_value(inst,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      char *disp_string=get_weed_display_string(inst,param_number);
      weed_plant_t *wparam=weed_inst_in_param(inst,param_number,FALSE);
      int index=0,numvals;
      int *valis;

      if (mainw->multitrack!=NULL&&mainw->multitrack->track_index!=-1&&is_perchannel_multi(rfx,param_number)) 
	index=mainw->multitrack->track_index;
      numvals=weed_leaf_num_elements(wparam,"value");
      if (index>=numvals) {
	weed_plant_t *paramtmpl=weed_get_plantptr_value(wparam,"template",&error);
	fill_param_vals_to(paramtmpl,wparam,param_number,WEED_HINT_INTEGER,index);
	numvals=index+1;
      }
      
      valis=weed_get_int_array(wparam,"value",&error);
      valis[index]=new_index;
      pthread_mutex_lock(&mainw->data_mutex);
      weed_set_int_array(wparam,"value",numvals,valis);
      pthread_mutex_unlock(&mainw->data_mutex);
      copyto=set_copy_to(inst,param_number,TRUE);
      weed_free(valis);

     if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
      // if we are recording, add this change to our event_list
	rec_param_change(inst,param_number);
	if (copyto!=-1) rec_param_change(inst,copyto);
      }
    
      
      if (param->reinit||(copyto!=-1&&rfx->params[copyto].reinit)) {
	weed_reinit_effect(inst);
      }

      if (disp_string!=NULL) {
	gulong blockfunc=(gulong)g_object_get_data(G_OBJECT(combo),"blockfunc");
	g_signal_handler_block(combo,blockfunc);
	gtk_entry_set_text (GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))),disp_string);
	g_signal_handler_unblock(combo,blockfunc);
	weed_free(disp_string);
      } 
    }
  }

  if (old_index!=new_index&&param->onchange) {
    param->change_blocked=TRUE;
    do_onchange(G_OBJECT(combo), rfx);
    while (g_main_context_iteration(NULL,FALSE));
    param->change_blocked=FALSE;
  }
  if (copyto!=-1) update_visual_params(rfx,FALSE);
  if (mainw->multitrack!=NULL&&rfx->status==RFX_STATUS_WEED) {
    activate_mt_preview(mainw->multitrack);
  }
  param->changed=TRUE;
}



gchar **param_marshall_to_argv (lives_rfx_t *rfx) {
  // this function will marshall all parameters into a argv array
  // last array element will be NULL

  // the returned **argv should be g_free()'ed after use

  int i;
  lives_colRGB24_t rgb;
  gchar **argv=(gchar **)g_malloc((rfx->num_params+1)*(sizeof(gchar *)));

  gchar *tmp;
 
  for (i=0;i<rfx->num_params;i++) {
    switch (rfx->params[i].type) {
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param(rfx->params[i].value,&rgb);
      argv[i]=g_strdup_printf("%u",(((rgb.red<<8)+rgb.green)<<8)+rgb.blue);
      break;

    case LIVES_PARAM_STRING:
      // escape strings
      argv[i]=g_strdup_printf ("%s",(tmp=U82L ((gchar *)rfx->params[i].value)));
      g_free(tmp);
      break;

    case LIVES_PARAM_STRING_LIST:
      // escape strings
      argv[i]=g_strdup_printf ("%d",get_int_param(rfx->params[i].value));
      break;
      
    default:
      if (rfx->params[i].dp) {
	gchar *return_pattern=g_strdup_printf ("%%.%df",rfx->params[i].dp);
	argv[i]=g_strdup_printf (return_pattern,get_double_param(rfx->params[i].value));
	g_free(return_pattern);
      }
      else {
	argv[i]=g_strdup_printf ("%d",get_int_param(rfx->params[i].value));
      }
    }
  }
  argv[i]=NULL;
  return argv;
}







gchar *param_marshall (lives_rfx_t *rfx, gboolean with_min_max) {
  // this function will marshall all parameters into a space separated string
  // in case of string parameters, these will be surrounded by " and all 
  // quotes will be escaped \"

  // the returned string should be g_free()'ed after use
  gchar *new_return=g_strdup ("");
  gchar *old_return=new_return;
  gchar *return_pattern;
  lives_colRGB24_t rgb;
  int i;
 
  gchar *tmp,*mysubst,*mysubst2;

  for (i=0;i<rfx->num_params;i++) {
    switch (rfx->params[i].type) {
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param(rfx->params[i].value,&rgb);
      if (!with_min_max) {
	new_return=g_strdup_printf ("%s %u",old_return,(((rgb.red<<8)+rgb.green)<<8)+rgb.blue);
      }
      else {
	new_return=g_strdup_printf ("%s %d %d %d",old_return,rgb.red,rgb.green,rgb.blue);
      }
      g_free (old_return);
      old_return=new_return;
      break;

    case LIVES_PARAM_STRING:
      // we need to doubly escape strings 
      mysubst=subst((gchar *)rfx->params[i].value,"\\","\\\\\\\\");
      mysubst2=subst(mysubst,"\"","\\\\\\\"");
      g_free(mysubst);
      mysubst=subst(mysubst2,"`","\\`");
      g_free(mysubst2);
      mysubst2=subst(mysubst,"'","\\`");
      g_free(mysubst);
      new_return=g_strdup_printf ("%s \"%s\"",old_return,(tmp=U82L (mysubst2)));
      g_free(tmp);
      g_free(mysubst2);
      g_free (old_return);
      old_return=new_return;
      break;

    case LIVES_PARAM_STRING_LIST:
      new_return=g_strdup_printf ("%s %d",old_return,get_int_param(rfx->params[i].value));
      g_free (old_return);
      old_return=new_return;
      break;
      
    default:
      if (rfx->params[i].dp) {
	return_pattern=g_strdup_printf ("%%s %%.%df",rfx->params[i].dp);
	new_return=g_strdup_printf (return_pattern,old_return,get_double_param(rfx->params[i].value));
	if (with_min_max) {
	  g_free (old_return);
	  old_return=new_return;
	  new_return=g_strdup_printf (return_pattern,old_return,rfx->params[i].min);
	  g_free (old_return);
	  old_return=new_return;
	  new_return=g_strdup_printf (return_pattern,old_return,rfx->params[i].max);
	}
	g_free (return_pattern);
      }
      else {
	new_return=g_strdup_printf ("%s %d",old_return,get_int_param(rfx->params[i].value));
	if (with_min_max&&rfx->params[i].type!=LIVES_PARAM_BOOL) {
	  g_free (old_return);
	  old_return=new_return;
	  new_return=g_strdup_printf ("%s %d",old_return,(int)rfx->params[i].min);
	  g_free (old_return);
	  old_return=new_return;
	  new_return=g_strdup_printf ("%s %d",old_return,(int)rfx->params[i].max);
	}
      }
      g_free (old_return);
      old_return=new_return;
    }
  }
  if (mainw->current_file>0&&with_min_max) {
    if (rfx->num_in_channels<2) {
      new_return=g_strdup_printf ("%s %d %d %d %d %d",old_return,cfile->hsize,cfile->vsize,cfile->start,
				  cfile->end,cfile->frames);
    }
    else {
      // for transitions, change the end to indicate the merge section
      // this is better for length calculations
      gint cb_frames=clipboard->frames;
      gint start=cfile->start,end=cfile->end,ttl;

      if (prefs->ins_resample&&clipboard->fps!=cfile->fps) {
	cb_frames=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
      }

      if (cfile->end-cfile->start+1>(cb_frames*(ttl=gtk_spin_button_get_value_as_int
						(GTK_SPIN_BUTTON (merge_opts->spinbutton_loops))))&&
	  !merge_opts->loop_to_fit) {
	end=cb_frames*ttl;
	if (!merge_opts->align_start) {
	  start=cfile->end-end+1;
	  end=cfile->end;
	}
	else {
	  start=cfile->start;
	  end+=start-1;
	}
      }
      new_return=g_strdup_printf ("%s %d %d %d %d %d %d %d",old_return,cfile->hsize,cfile->vsize,start,end,
				  cfile->frames,clipboard->hsize,clipboard->vsize);
    }
  }
  else {
    new_return=g_strdup (old_return);
  }
  g_free (old_return);

  return new_return;
}


gchar *reconstruct_string (GList *plist, gint start, gint *offs) {
  // convert each piece from locale to utf8
  // concat list entries to get reconstruct
  // replace \" with "

  gchar *word=NULL;
  int i;
  gboolean lastword=FALSE;
  gchar *ret=g_strdup (""),*ret2;

  gchar *tmp;

  word=L2U8 ((gchar *)g_list_nth_data (plist,start));

  if (word==NULL||!strlen (word)||word[0]!='\"') {
    if (word!=NULL) g_free (word);
    return 0;
  }

  word++;

  for (i=start;i<g_list_length (plist);i++) {
    if (strlen (word)) {
      if ((word[strlen (word)-1]=='\"')&&(strlen (word)==1||word[strlen (word)-2]!='\\')) {
	lastword=TRUE;
	memset (word+strlen (word)-1,0,1);
      }
    }

    ret2=g_strconcat (ret,(tmp=subst (word,"\\\"","\""))," ",NULL);
    g_free(tmp);
    if (ret2!=ret) g_free (ret);
    ret=ret2;

    if (i==start) word--;
    g_free (word);

    if (lastword) break;

    if (i<g_list_length (plist)-1) word=L2U8 ((gchar *)g_list_nth_data (plist,i+1));
  }

  set_int_param (offs,i-start+1);

  // remove trailing space
  memset (ret+strlen (ret)-1,0,1);
  return ret;

}


void param_demarshall (lives_rfx_t *rfx, GList *plist, gboolean with_min_max, gboolean upd) {
  int i;
  gint pnum=0;
  lives_param_t *param;

  // here we take a GList * of param values, set them in rfx, and if upd is TRUE we also update their visual appearance

  // param->widgets[n] are only valid if upd==TRUE

  if (plist==NULL) return;

  for (i=0;i<rfx->num_params;i++) {
    param=&rfx->params[i];
    pnum=set_param_from_list(plist,param,pnum,with_min_max,upd);
  }
}



GList *argv_to_marshalled_list (lives_rfx_t *rfx, gint argc, gchar **argv) {
  int i;
  GList *plist=NULL;
  gchar *tmp,*tmp2,*tmp3;

  if (argc==0) return plist;

  for (i=0;i<=argc&&argv[i]!=NULL;i++) {
    if (rfx->params[i].type==LIVES_PARAM_STRING) {
      tmp=g_strdup_printf ("\"%s\"",(tmp2=U82L (tmp3=subst (argv[i],"\"","\\\""))));
      plist=g_list_append(plist,tmp);
      g_free(tmp2);
      g_free(tmp3);
    }
    else {
      plist=g_list_append(plist,g_strdup(argv[i]));
    }
  }
  return plist;
}





gint set_param_from_list(GList *plist, lives_param_t *param, gint pnum, gboolean with_min_max, gboolean upd) {
  // update values for param using values in plist
  // if upd is TRUE, the widgets for that param also are updated

  // for LIVES_PARAM_NUM, setting pnum negative avoids having to send min,max

  gint red,green,blue;
  gint offs=0;
  gint maxlen=g_list_length(plist)-1;

  if (ABS(pnum)>maxlen) return 0;

  switch (param->type) {
  case LIVES_PARAM_BOOL:
    if (param->change_blocked) {
      pnum++;
      break;
    }
    set_bool_param(param->value,(atoi ((gchar *)g_list_nth_data (plist,pnum++))));
    if (upd) {
      if (param->widgets[0]&&GTK_IS_TOGGLE_BUTTON (param->widgets[0])) {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (param->widgets[0]),get_bool_param(param->value));
      }
    }
    break;
  case LIVES_PARAM_NUM:
    if (param->change_blocked) {
      pnum++;
      if (with_min_max) pnum+=2;
      break;
    }
    if (param->dp) {
      gdouble double_val=g_strtod ((gchar *)g_list_nth_data (plist,pnum++),NULL);
      if (with_min_max) {
	if (ABS(pnum)>maxlen) return 1;
	param->min=g_strtod ((gchar *)g_list_nth_data (plist,pnum++),NULL);
	if (ABS(pnum)>maxlen) return 2;
	param->max=g_strtod ((gchar *)g_list_nth_data (plist,pnum++),NULL);
	if (double_val<param->min) double_val=param->min;
	if (double_val>param->max) double_val=param->max;
      }
      set_double_param(param->value,double_val);
      if (upd) {
	if (param->widgets[0]&&GTK_IS_SPIN_BUTTON (param->widgets[0])) {
	  gulong spinfunc=(gulong)g_object_get_data(G_OBJECT(param->widgets[0]),"spinfunc");
	  g_signal_handler_block(param->widgets[0],spinfunc);
	  gtk_spin_button_set_range (GTK_SPIN_BUTTON (param->widgets[0]),(gdouble)param->min,(gdouble)param->max);
	  gtk_spin_button_update(GTK_SPIN_BUTTON(param->widgets[0]));
	  g_signal_handler_unblock(param->widgets[0],spinfunc);
	  gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),get_double_param(param->value));
	  gtk_spin_button_update(GTK_SPIN_BUTTON(param->widgets[0]));
	}
      }
    }
    else {
      gint int_value=atoi ((gchar *)g_list_nth_data (plist,pnum++));
      if (with_min_max) {
	gint int_min,int_max;
	if (ABS(pnum)>maxlen) return 1;
	int_min=atoi ((gchar *)g_list_nth_data (plist,pnum++));
	if (ABS(pnum)>maxlen) return 2;
	int_max=atoi ((gchar *)g_list_nth_data (plist,pnum++));
	if (int_value<int_min) int_value=int_min;
	if (int_value>int_max) int_value=int_max;
	param->min=(gdouble)int_min;
	param->max=(gdouble)int_max;
      }
      set_int_param(param->value,int_value);
      
      if (upd) {
	if (param->widgets[0]&&GTK_IS_SPIN_BUTTON (param->widgets[0])) {
	  gulong spinfunc=(gulong)g_object_get_data(G_OBJECT(param->widgets[0]),"spinfunc");
	  g_signal_handler_block(param->widgets[0],spinfunc);
	  gtk_spin_button_set_range (GTK_SPIN_BUTTON (param->widgets[0]),(gdouble)param->min,(gdouble)param->max);
	  gtk_spin_button_update(GTK_SPIN_BUTTON(param->widgets[0]));
	  g_signal_handler_unblock(param->widgets[0],spinfunc);
	  gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),(gdouble)get_int_param(param->value));
	  gtk_spin_button_update(GTK_SPIN_BUTTON(param->widgets[0]));
	}
      }
    }
    break;
  case LIVES_PARAM_COLRGB24:
    red=atoi ((gchar *)g_list_nth_data (plist,pnum++)); // red
    if (ABS(pnum)>maxlen) return 1;
    green=atoi ((gchar *)g_list_nth_data (plist,pnum++)); // green
    if (ABS(pnum)>maxlen) return 2;
    blue=atoi ((gchar *)g_list_nth_data (plist,pnum++)); // blue
    if (param->change_blocked) break;
    set_colRGB24_param(param->value,red,green,blue);

    if (upd) {
      if (param->widgets[0]&&GTK_IS_SPIN_BUTTON (param->widgets[0])) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),(gdouble)red);
      }
      if (param->widgets[1]&&GTK_IS_SPIN_BUTTON (param->widgets[1])) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[1]),(gdouble)green);
      }
      if (param->widgets[2]&&GTK_IS_SPIN_BUTTON (param->widgets[2])) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[2]),(gdouble)blue);
      }
    }
    break;
  case LIVES_PARAM_STRING:
    param->value=reconstruct_string (plist,pnum,&offs);
    if (upd) {
      if (param->widgets[0]!=NULL) {
	if ((gint)param->max>RFX_TEXT_MAGIC||param->max==0.) {
	  GtkTextBuffer *textbuffer=GTK_TEXT_BUFFER (g_object_get_data(G_OBJECT(param->widgets[0]),"textbuffer"));
	  gtk_text_buffer_set_text (textbuffer, (gchar *)param->value, -1);
	}
	else {
	  gtk_entry_set_text (GTK_ENTRY (param->widgets[0]),(gchar *)param->value);
	}
      }
    }
    pnum+=offs;
    break;
  case LIVES_PARAM_STRING_LIST:
    {
      gint int_value=atoi ((gchar *)g_list_nth_data (plist,pnum++));
      if (param->change_blocked) break;
      set_int_param(param->value,int_value);
      if (upd&&param->widgets[0]!=NULL&&GTK_IS_COMBO(param->widgets[0])&&int_value<g_list_length(param->list))
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(param->widgets[0]))),(gchar *)g_list_nth_data(param->list,int_value));
      break;
    }
  default:
    break;
  }
  return pnum;
}


void do_onchange (GObject *object, lives_rfx_t *rfx) {
  gint which=GPOINTER_TO_INT (g_object_get_data (object,"param_number"));
  gchar *com,*tmp;
  GList *retvals;
  gint width=0,height=0;
  gchar *plugdir;

  const gchar *handle="";

  if (rfx->status==RFX_STATUS_WEED) return;

  if (which<0) {
    // init
    switch (rfx->status) {
    case RFX_STATUS_BUILTIN:
      plugdir=g_build_filename (prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_RENDERED_EFFECTS_BUILTIN,NULL);
      break;
    case RFX_STATUS_CUSTOM:
      plugdir=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_CUSTOM,NULL);
      break;
    case RFX_STATUS_TEST:
      plugdir=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_TEST,NULL);
      break;
    default:
      plugdir=g_strdup_printf ("%s",prefs->tmpdir);
    }

    if (mainw->current_file>0) {
      width=cfile->hsize;
      height=cfile->vsize;
      handle=cfile->handle;
    }

    com=g_strdup_printf ("%s \"fxinit_%s\" \"%s\" \"%s\" %d %d %s",prefs->backend_sync,rfx->name,handle,plugdir,
			 width,height,(tmp=param_marshall (rfx,TRUE)));
    retvals=plugin_request_by_space (NULL,NULL,com);

    g_free(tmp);
    g_free(plugdir);
  }
  else {
    com=g_strdup_printf ("onchange_%d%s",which,param_marshall (rfx,TRUE));
    switch (rfx->status) {
    case RFX_STATUS_BUILTIN:
      retvals=plugin_request_by_space (PLUGIN_RENDERED_EFFECTS_BUILTIN,rfx->name,com);
      break;
    case RFX_STATUS_CUSTOM:
      retvals=plugin_request_by_space (PLUGIN_RENDERED_EFFECTS_CUSTOM,rfx->name,com);
      break;
    case RFX_STATUS_TEST:
      retvals=plugin_request_by_space (PLUGIN_RENDERED_EFFECTS_TEST,rfx->name,com);
      break;
    default:
      retvals=plugin_request_by_space (PLUGIN_RFX_SCRAP,rfx->name,com);
    }
  }

  if (retvals!=NULL) {
    param_demarshall (rfx,retvals,TRUE,which>=0);
    g_list_free_strings (retvals);
    g_list_free (retvals);
  }
  else {
    if (which<=0&&mainw->error) {
      mainw->error=FALSE;
      do_blocking_error_dialog (g_strdup_printf ("\n\n%s\n\n",mainw->msg));
    }
  }
  g_free (com);

}



void
on_pwcolsel (GtkButton *button, lives_rfx_t *rfx)
{
  GdkColor selected;
  gint pnum=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),"param_number"));
  lives_param_t *param=&rfx->params[pnum];

  gtk_color_button_get_color(GTK_COLOR_BUTTON(button),&selected);

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(param->widgets[0]),(gdouble)(gint)((selected.red+128)/257));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(param->widgets[1]),(gdouble)(gint)((selected.green+128)/257));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(param->widgets[2]),(gdouble)(gint)((selected.blue+128)/257));
  gtk_color_button_set_color(GTK_COLOR_BUTTON(param->widgets[4]),&selected);
}




void update_visual_params(lives_rfx_t *rfx, gboolean update_hidden) {
  // update parameters visually from an rfx object
  int i,j,error;
  weed_plant_t *inst=(weed_plant_t *)rfx->source;
  weed_plant_t **in_params,*in_param;
  int num_params=0;
  weed_plant_t *paramtmpl;
  int param_hint;
  int *valis,vali,mini,maxi;
  gchar *vals,**valss;
  double *valds,vald,mind,maxd;
  int cspace;
  GList *list;
  int *colsi,*colsis;
  double *colsd,*colsds;
  int red_max,green_max,blue_max;
  int red_min,green_min,blue_min;
  int *maxis=NULL,*minis=NULL;
  double *maxds=NULL,*minds=NULL;
  double red_maxd,green_maxd,blue_maxd;
  double red_mind,green_mind,blue_mind;
  int index,numvals;
  gchar *pattern;
  gchar *tmp,*tmp2;

    
  if (weed_plant_has_leaf(inst,"in_parameters")) num_params=weed_leaf_num_elements(inst,"in_parameters");
  if (num_params==0) return;

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  for (i=0;i<num_params;i++) {
    if (!is_hidden_param(inst,i)||update_hidden) {

      in_param=in_params[i];
      paramtmpl=weed_get_plantptr_value(in_param,"template",&error);
      param_hint=weed_get_int_value(paramtmpl,"hint",&error);
      list=NULL;

      // assume index is 0, unless we are a framedraw multi parameter
      // most of the time this will be ok, as other such multivalued parameters should be hidden
      index=0;

      if (mainw->multitrack!=NULL&&mainw->multitrack->track_index!=-1&&is_perchannel_multi(rfx,i)) 
	index=mainw->multitrack->track_index;

      pthread_mutex_lock(&mainw->data_mutex);

      numvals=weed_leaf_num_elements(in_param,"value");

      if (param_hint!=WEED_HINT_COLOR&&index>=numvals) {
	fill_param_vals_to(paramtmpl,in_param,i,param_hint,index);
	numvals=index+1;
      }

      switch (param_hint) {
      case WEED_HINT_INTEGER:
	valis=weed_get_int_array(in_param,"value",&error);
	vali=valis[index];
	weed_free(valis);

	mini=weed_get_int_value(paramtmpl,"min",&error);
	maxi=weed_get_int_value(paramtmpl,"max",&error);
	
	list=g_list_append(list,g_strdup_printf("%d",vali));
	list=g_list_append(list,g_strdup_printf("%d",mini));
	list=g_list_append(list,g_strdup_printf("%d",maxi));
	set_param_from_list(list,&rfx->params[i],0,TRUE,TRUE);
	g_list_free_strings(list);
	g_list_free(list);
	
	break;
      case WEED_HINT_FLOAT:
	valds=weed_get_double_array(in_param,"value",&error);
	vald=valds[index];
	weed_free(valds);

	mind=weed_get_double_value(paramtmpl,"min",&error);
	maxd=weed_get_double_value(paramtmpl,"max",&error);

	pattern=g_strdup("%.2f");

	if (weed_plant_has_leaf(paramtmpl,"gui")) {
	  weed_plant_t *gui=weed_get_plantptr_value(paramtmpl,"gui",&error);
	  if (weed_plant_has_leaf(gui,"decimals")) {
	    int dp=weed_get_int_value(gui,"decimals",&error);
	    g_free(pattern);
	    pattern=g_strdup_printf("%%.%df",dp);
	  }
	}
	
	list=g_list_append(list,g_strdup_printf(pattern,vald));
	list=g_list_append(list,g_strdup_printf(pattern,mind));
	list=g_list_append(list,g_strdup_printf(pattern,maxd));

	g_free(pattern);

	set_param_from_list(list,&rfx->params[i],0,TRUE,TRUE);
	g_list_free_strings(list);
	g_list_free(list);

	break;
      case WEED_HINT_SWITCH:
	valis=weed_get_boolean_array(in_param,"value",&error);
	vali=valis[index];
	weed_free(valis);

	list=g_list_append(list,g_strdup_printf("%d",vali));
	set_param_from_list(list,&rfx->params[i],0,FALSE,TRUE);
	g_list_free_strings(list);
	g_list_free(list);

	break;
      case WEED_HINT_TEXT:
	valss=weed_get_string_array(in_param,"value",&error);
	vals=valss[index];

	list=g_list_append(list,g_strdup_printf ("\"%s\"",(tmp=U82L (tmp2=subst (vals,"\"","\\\"")))));
	g_free(tmp);
	g_free(tmp2);
	set_param_from_list(list,&rfx->params[i],0,FALSE,TRUE);
	for (j=0;j<numvals;j++) {
	  weed_free(valss[j]);
	}
	weed_free(valss);
	g_list_free_strings(list);
	g_list_free(list);
	break;
      case WEED_HINT_COLOR:
	cspace=weed_get_int_value(paramtmpl,"colorspace",&error);
	switch (cspace) {
	case WEED_COLORSPACE_RGB:
	  numvals=weed_leaf_num_elements(in_param,"value");
	  if (index*3>=numvals) fill_param_vals_to(paramtmpl,in_param,i,param_hint,index);

	  if (weed_leaf_seed_type(paramtmpl,"default")==WEED_SEED_INT) {
	    colsis=weed_get_int_array(in_param,"value",&error);
	    colsi=&colsis[3*index];

	    if (weed_leaf_num_elements(paramtmpl,"max")==1) {
	      red_max=green_max=blue_max=weed_get_int_value(paramtmpl,"max",&error);
	    }
	    else {
	      maxis=weed_get_int_array(paramtmpl,"max",&error);
	      red_max=maxis[0];
	      green_max=maxis[1];
	      blue_max=maxis[2];
	    }
	    if (weed_leaf_num_elements(paramtmpl,"min")==1) {
	      red_min=green_min=blue_min=weed_get_int_value(paramtmpl,"min",&error);
	    }
	    else {
	      minis=weed_get_int_array(paramtmpl,"min",&error);
	      red_min=minis[0];
	      green_min=minis[1];
	      blue_min=minis[2];
	    }

	    colsi[0]=(gint)((gdouble)(colsi[0]-red_min)/(gdouble)(red_max-red_min)*255.+.5);
	    colsi[1]=(gint)((gdouble)(colsi[1]-green_min)/(gdouble)(green_max-green_min)*255.+.5);
	    colsi[2]=(gint)((gdouble)(colsi[2]-blue_min)/(gdouble)(blue_max-blue_min)*255.+.5);

	    if (colsi[0]<red_min) colsi[0]=red_min;
	    if (colsi[1]<green_min) colsi[1]=green_min;
	    if (colsi[2]<blue_min) colsi[2]=blue_min;
	    if (colsi[0]>red_max) colsi[0]=red_max;
	    if (colsi[1]>green_max) colsi[1]=green_max;
	    if (colsi[2]>blue_max) colsi[2]=blue_max;

	    list=g_list_append(list,g_strdup_printf("%d",colsi[0]));
	    list=g_list_append(list,g_strdup_printf("%d",colsi[1]));
	    list=g_list_append(list,g_strdup_printf("%d",colsi[2]));

	    set_param_from_list(list,&rfx->params[i],0,FALSE,TRUE);

	    g_list_free_strings(list);
	    g_list_free(list);
	    weed_free(colsis);
	    if (maxis!=NULL) weed_free(maxis);
	    if (minis!=NULL) weed_free(minis);
	  }
	  else {
	    colsds=weed_get_double_array(in_param,"value",&error);
	    colsd=&colsds[3*index];
	    if (weed_leaf_num_elements(paramtmpl,"max")==1) {
	      red_maxd=green_maxd=blue_maxd=weed_get_double_value(paramtmpl,"max",&error);
	    }
	    else {
	      maxds=weed_get_double_array(paramtmpl,"max",&error);
	      red_maxd=maxds[0];
	      green_maxd=maxds[1];
	      blue_maxd=maxds[2];
	    }
	    if (weed_leaf_num_elements(paramtmpl,"min")==1) {
	      red_mind=green_mind=blue_mind=weed_get_double_value(paramtmpl,"min",&error);
	    }
	    else {
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
	    
	    list=g_list_append(list,g_strdup_printf("%.2f",colsd[0]));
	    list=g_list_append(list,g_strdup_printf("%.2f",colsd[1]));
	    list=g_list_append(list,g_strdup_printf("%.2f",colsd[2]));
	    set_param_from_list(list,&rfx->params[i],0,FALSE,TRUE);

	    g_list_free_strings(list);
	    g_list_free(list);
	    weed_free(colsds);
	    if (maxds!=NULL) weed_free(maxds);
	    if (minds!=NULL) weed_free(minds);
	  }
	  break;
	  // TODO - other color spaces, e.g. RGBA24
	}
	break;
      } // hint
    }
    pthread_mutex_unlock(&mainw->data_mutex);
  }
  weed_free(in_params);
}

