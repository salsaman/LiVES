// dialogs.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2010
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "interface.h"
#include "support.h"
#include "cvirtual.h"
#include "audio.h" // for fill_abuffer_from

// for ping-pong loops

//
extern void reset_frame_and_clip_index (void);


// processing
static gint64 event_start;
static gdouble audio_start;
static gboolean accelerators_swapped;
static gint frames_done;
static gint disp_frames_done;
static guint64 last_display_ticks=0;  // ticks when last display happened (fixed)

static gint64 last_open_check_ticks;

// how often to we count frames when opening
#define OPEN_CHECK_TICKS (U_SECL/10l)

void on_warn_mask_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
  GtkWidget *tbutton;

  if (gtk_toggle_button_get_active(togglebutton)) prefs->warning_mask|=GPOINTER_TO_INT(user_data);
  else prefs->warning_mask^=GPOINTER_TO_INT(user_data);
  set_int_pref("lives_warning_mask",prefs->warning_mask);

  if ((tbutton=g_object_get_data(G_OBJECT(togglebutton),"auto"))!=NULL) {
    // this is for the cds window - disable autoreload if we are not gonna show this window
    if (gtk_toggle_button_get_active(togglebutton)) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tbutton),FALSE);
      gtk_widget_set_sensitive(tbutton,FALSE);
    }
    else {
      gtk_widget_set_sensitive(tbutton,TRUE);
    }
  }

}

static int progress_count;


#define PROG_LOOP_VAL 100





static void add_xlays_widget(GtkBox *box) {
  // add widget to preview affected layouts

  GtkWidget *expander=gtk_expander_new_with_mnemonic(_("Show affected _layouts"));
  GtkWidget *textview=gtk_text_view_new();
  GtkWidget *label;
  GList *xlist=mainw->xlays;
  GtkTextBuffer *textbuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  gtk_container_add (GTK_CONTAINER (expander), textview);

  if (palette->style&STYLE_1) {
    label=gtk_expander_get_label_widget(GTK_EXPANDER(expander));
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_text(textview, GTK_STATE_NORMAL, &palette->info_text);
    gtk_widget_modify_base(textview, GTK_STATE_NORMAL, &palette->info_base);
  }

  gtk_text_buffer_insert_at_cursor(textbuffer,"\n",strlen("\n"));
  
  while (xlist!=NULL) {
    gtk_text_buffer_insert_at_cursor(textbuffer,(const gchar *)xlist->data,strlen(xlist->data));
    gtk_text_buffer_insert_at_cursor(textbuffer,"\n",strlen("\n"));
    xlist=xlist->next;
  }

  gtk_box_pack_start (box, expander, FALSE, FALSE, 10);
  gtk_widget_show_all(expander);
}






void add_warn_check (GtkBox *box, gint warn_mask_number) {
  GtkWidget *checkbutton = gtk_check_button_new ();
  GtkWidget *eventbox,*hbox;
  GtkWidget *label=gtk_label_new_with_mnemonic (_("Do _not show this warning any more\n(can be turned back on from Preferences/Warnings)"));

  gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);

  eventbox=gtk_event_box_new();

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		      checkbutton);
  
  hbox = gtk_hbox_new (FALSE, 0);
  
  if (mainw->is_ready&&palette->style&STYLE_1&&mainw!=NULL) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_box_pack_start (box, hbox, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  gtk_widget_show_all (hbox);
  GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  g_signal_connect (GTK_OBJECT (checkbutton), "toggled",
		    G_CALLBACK (on_warn_mask_toggled),
                      GINT_TO_POINTER(warn_mask_number));
}




//Warning dialog
GtkWidget*
create_warn_dialog (gint warn_mask_number) {
  GtkWidget *dialog2;
  GtkWidget *dialog_vbox2;
  GtkWidget *dialog_action_area2;
  GtkWidget *warning_cancelbutton;
  GtkWidget *warning_okbutton;

  dialog2 = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog2), _("LiVES: - Warning !"));
  if (mainw->is_ready&&palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog2),FALSE);
    gtk_widget_modify_bg(dialog2, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_window_set_modal (GTK_WINDOW (dialog2), TRUE);
  gtk_window_set_position (GTK_WINDOW (dialog2), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (dialog2), 350, 200);
  gtk_window_set_deletable(GTK_WINDOW(dialog2), FALSE);

  dialog_vbox2 = GTK_DIALOG (dialog2)->vbox;
  gtk_widget_show (dialog_vbox2);

  mainw->warning_label = gtk_label_new (_("warning"));
  gtk_widget_show (mainw->warning_label);
  gtk_box_pack_start (GTK_BOX (dialog_vbox2), mainw->warning_label, TRUE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (mainw->warning_label), GTK_JUSTIFY_CENTER);
  gtk_label_set_line_wrap (GTK_LABEL (mainw->warning_label), FALSE);
  gtk_label_set_selectable (GTK_LABEL (mainw->warning_label), TRUE);

  if (mainw->is_ready&&palette->style&STYLE_1) {
    gtk_widget_modify_fg(mainw->warning_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  if (warn_mask_number>0) {
    add_warn_check(GTK_BOX(dialog_vbox2),warn_mask_number);
  }

  if (mainw->xlays!=NULL) {
    add_xlays_widget(GTK_BOX(dialog_vbox2));
  }

  dialog_action_area2 = GTK_DIALOG (dialog2)->action_area;
  gtk_widget_show (dialog_action_area2);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area2), GTK_BUTTONBOX_END);

  warning_cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (warning_cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog2), warning_cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (warning_cancelbutton, GTK_CAN_DEFAULT);

  warning_okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (warning_okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog2), warning_okbutton, GTK_RESPONSE_OK);



  GTK_WIDGET_SET_FLAGS (warning_okbutton, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (warning_okbutton);

  return dialog2;
}



gboolean
do_warning_dialog(const gchar *text) {
  return do_warning_dialog_with_check(text, 0);
}


gboolean do_warning_dialog_with_check (const gchar *text, gint warn_mask_number) {
  if (!prefs->show_gui) {
    return do_warning_dialog_with_check_transient(text,warn_mask_number,NULL);
  } else {
    if (mainw->multitrack==NULL) return do_warning_dialog_with_check_transient(text,warn_mask_number,GTK_WINDOW(mainw->LiVES));
    return do_warning_dialog_with_check_transient(text,warn_mask_number,GTK_WINDOW(mainw->multitrack->window));
  }
}



gboolean
do_warning_dialog_with_check_transient(const gchar *text, gint warn_mask_number, GtkWindow *transient) {
  // show OK/CANCEL, returns FALSE if cancelled
  GtkWidget *warning;
  gint response=1;
  gchar *mytext;

  if (prefs->warning_mask&warn_mask_number) {
    return TRUE;
  }

  mytext=g_strdup(text); // trans
  warning=create_warn_dialog(warn_mask_number);
  if (transient!=NULL) gtk_window_set_transient_for(GTK_WINDOW(warning),transient);
  gtk_label_set_text(GTK_LABEL(mainw->warning_label),mytext);
  if (mytext!=NULL) g_free(mytext);

  response=gtk_dialog_run (GTK_DIALOG (warning));
  gtk_widget_destroy (warning);

  while (g_main_context_iteration(NULL,FALSE));
  return (response==GTK_RESPONSE_OK);
}



void 
do_error_dialog(const gchar *text) {
  // show error/info box
  if (!prefs->show_gui) {
    do_error_dialog_with_check_transient(text,FALSE,0,NULL);
  } else {
    if (mainw->multitrack==NULL) do_error_dialog_with_check_transient(text,FALSE,0,GTK_WINDOW(mainw->LiVES));
    else do_error_dialog_with_check_transient(text,FALSE,0,GTK_WINDOW(mainw->multitrack->window));
  }
}


void 
do_error_dialog_with_check(const gchar *text, gint warn_mask_number) {
  // show error/info box
  if (!prefs->show_gui) {
    do_error_dialog_with_check_transient(text,FALSE,warn_mask_number,NULL);
  } else {
    if (mainw->multitrack==NULL) do_error_dialog_with_check_transient(text,FALSE,warn_mask_number,GTK_WINDOW(mainw->LiVES));
    else do_error_dialog_with_check_transient(text,FALSE,warn_mask_number,GTK_WINDOW(mainw->multitrack->window));
  }
}


void 
do_blocking_error_dialog(const gchar *text) {
  // show error/info box - blocks until OK is pressed
  if (!prefs->show_gui) {
    do_error_dialog_with_check_transient(text,TRUE,0,NULL);
  } else {
    if (mainw->multitrack==NULL) do_error_dialog_with_check_transient(text,TRUE,0,GTK_WINDOW(mainw->LiVES));
    else do_error_dialog_with_check_transient(text,TRUE,0,GTK_WINDOW(mainw->multitrack->window));
  }
}


void 
do_error_dialog_with_check_transient(const gchar *text, gboolean is_blocking, gint warn_mask_number, GtkWindow *transient) {
  // show error/info box

  GtkWidget *err_box;
  gchar *mytext;
  if (prefs->warning_mask&warn_mask_number) return;
  mytext=g_strdup(text);
  err_box=create_dialog3(mytext,is_blocking,warn_mask_number);
  if (mytext!=NULL) g_free(mytext);
  if (transient!=NULL) gtk_window_set_transient_for(GTK_WINDOW(err_box),transient);
  gtk_widget_show(err_box);
  gtk_window_present (GTK_WINDOW (err_box));
  gdk_window_raise (err_box->window);

  if (is_blocking) {
    gtk_dialog_run(GTK_DIALOG (err_box));
    if (mainw!=NULL&&mainw->is_ready) {
      gtk_widget_queue_draw(GTK_WIDGET(transient));
    }
  }
}

void do_aud_during_play_error(void) {
  do_error_dialog_with_check_transient(_("Audio players cannot be switched during playback."),TRUE,0,GTK_WINDOW(prefsw->prefs_dialog));
}  

void 
do_memory_error_dialog (void) {
  do_error_dialog (_ ("\n\nLiVES was unable to perform this operation due to unsufficient memory.\nPlease try closing some other applications first.\n"));
}





void pump_io_chan(GIOChannel *iochan) {
  // pump data from stdout to textbuffer
  gchar *str_return=NULL;
  gsize retlen;
  GError *gerr=NULL;

  GtkTextIter iter;
  GtkTextMark *mark;
  GtkTextBuffer *optextbuf=gtk_text_view_get_buffer(mainw->optextview);

  g_io_channel_read_to_end(iochan,&str_return,&retlen,&gerr);

  if (gerr!=NULL) g_error_free(gerr);

  if (retlen>0) {
    gtk_text_buffer_get_end_iter(optextbuf,&iter);
    gtk_text_buffer_insert(optextbuf,&iter,str_return,retlen);
    mark=gtk_text_buffer_create_mark(optextbuf,NULL,&iter,FALSE);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW (mainw->optextview),mark);
    gtk_text_buffer_delete_mark (optextbuf,mark);
  }
  if (str_return!=NULL) g_free(str_return);

}



void cancel_process(gboolean visible) {
  if (prefs->show_player_stats&&!visible&&mainw->fps_measure>0.) {
    // statistics
    gettimeofday(&tv, NULL);
    mainw->fps_measure/=(gdouble)(U_SECL*(tv.tv_sec-mainw->startsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origticks)/U_SEC;
  }
  if (visible) {
    if (mainw->preview_box!=NULL&&!mainw->preview) gtk_tooltips_set_tip (mainw->tooltips, mainw->p_playbutton,_ ("Play all"), NULL);
    if (accelerators_swapped) {
      if (!mainw->preview) gtk_tooltips_set_tip (mainw->tooltips, mainw->m_playbutton,_ ("Play all"), NULL);
      gtk_widget_remove_accelerator (cfile->proc_ptr->preview_button, mainw->accel_group, GDK_p, 0);
      gtk_widget_add_accelerator (mainw->playall, "activate", mainw->accel_group,GDK_p, 0, GTK_ACCEL_VISIBLE);
    }
    if (cfile->proc_ptr!=NULL) {
      gtk_widget_destroy(GTK_WIDGET(cfile->proc_ptr->processing));
      g_free(cfile->proc_ptr);
      cfile->proc_ptr=NULL;
    }
    mainw->is_processing=FALSE;
    if (!(cfile->menuentry==NULL)) {
      sensitize();
    }
  }
  else mainw->is_processing=TRUE;
  if (mainw->current_file>-1&&cfile->clip_type==CLIP_TYPE_DISK&&((mainw->cancelled!=CANCEL_NO_MORE_PREVIEW&&mainw->cancelled!=CANCEL_USER)||!cfile->opening)) {
    unlink(cfile->info_file);
  }
}

gboolean process_one (gboolean visible) {
  gint64 tc;
  guint first_frame=0,last_frame=INT_MAX;
  gint oframeno=0;

  gboolean normal_time;

#ifdef RT_AUDIO
  gdouble audio_stretch;
  guint64 audio_ticks=0.;
#endif

  if (!visible) {
    // INTERNAL PLAYER
    if (G_UNLIKELY(mainw->new_clip!=-1)) {
      do_quick_switch(mainw->new_clip);
      mainw->new_clip=-1;
    }

    normal_time=TRUE;
    
#ifdef ENABLE_JACK
    if (!mainw->foreign&&prefs->audio_player==AUD_PLAYER_JACK&&cfile->achans>0&&!mainw->is_rendering&&mainw->jackd!=NULL&&mainw->jackd->in_use) {
      mainw->currticks=lives_jack_get_time(mainw->jackd,TRUE);
      if (mainw->fixed_fpsd>0.||(mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.&&mainw->ext_playback)) normal_time=TRUE;
      else normal_time=FALSE;
    }
#endif

#ifdef HAVE_PULSE_AUDIO
    if (!mainw->foreign&&prefs->audio_player==AUD_PLAYER_PULSE&&cfile->achans>0&&!mainw->is_rendering&&mainw->pulsed!=NULL&&mainw->pulsed->in_use) {
      mainw->currticks=lives_pulse_get_time(mainw->pulsed,TRUE);
      if (mainw->fixed_fpsd>0.||(mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.&&mainw->ext_playback)) normal_time=TRUE;
      else normal_time=FALSE;
    }
#endif

    if (normal_time) {
      gettimeofday(&tv, NULL);
      mainw->currticks=U_SECL*(tv.tv_sec-mainw->startsecs)+tv.tv_usec*U_SEC_RATIO;

#ifdef ENABLE_JACK
      if ((mainw->fixed_fpsd>0.||(mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.))&&prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&cfile->achans>0&&!mainw->is_rendering&&(audio_ticks>mainw->origticks)) {
	// fps is synched to external source, so we adjust the audio rate to fit
	audio_ticks=mainw->currticks;
	if ((audio_stretch=(mainw->currticks-mainw->origticks)/(audio_ticks-mainw->origticks))<2.) {
	  if (!cfile->play_paused) mainw->jackd->sample_in_rate=cfile->arate*cfile->pb_fps/cfile->fps*audio_stretch;
	  else mainw->jackd->sample_in_rate=cfile->arate*cfile->freeze_fps/cfile->fps*audio_stretch;
	}
      }
#endif



#ifdef HAVE_PULSE_AUDIO
      if ((mainw->fixed_fpsd>0.||(mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.))&&prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&cfile->achans>0&&!mainw->is_rendering&&(audio_ticks>mainw->origticks)) {
	// fps is synched to external source, so we adjust the audio rate to fit
	audio_ticks=mainw->currticks;
	if ((audio_stretch=(mainw->currticks-mainw->origticks)/(audio_ticks-mainw->origticks))<2.) {
	  if (!cfile->play_paused) mainw->pulsed->in_arate=cfile->arate*cfile->pb_fps/cfile->fps*audio_stretch;
	  else mainw->pulsed->in_arate=cfile->arate*cfile->freeze_fps/cfile->fps*audio_stretch;
	}
      }
#endif

    }



    if (G_UNLIKELY(cfile->proc_ptr==NULL&&cfile->next_event!=NULL&&(tc=mainw->currticks-mainw->origticks)>=event_start)) {
      cfile->next_event=process_events (cfile->next_event,mainw->currticks-mainw->origticks);

#ifdef ENABLE_JACK
      if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&mainw->abufs_to_fill>0) {
	mainw->jackd->abufs[mainw->write_abuf]->samples_filled=0;
	fill_abuffer_from(mainw->jackd->abufs[mainw->write_abuf],mainw->event_list,NULL,FALSE);
      }
#endif
#ifdef HAVE_PULSE_AUDIO
      if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&mainw->abufs_to_fill>0) {
	mainw->pulsed->abufs[mainw->write_abuf]->samples_filled=0;
	fill_abuffer_from(mainw->pulsed->abufs[mainw->write_abuf],mainw->event_list,NULL,FALSE);
      }
#endif

      if (cfile->next_event==NULL&&mainw->preview) mainw->cancelled=CANCEL_EVENT_LIST_END;
      if (mainw->cancelled==CANCEL_NONE) return TRUE;
      cancel_process(visible);
      return FALSE;
    }
    else {
      oframeno=cfile->frameno;
      last_frame=(mainw->playing_sel&&!mainw->is_rendering)?cfile->end:mainw->play_end;
      first_frame=mainw->playing_sel?cfile->start:1;
      cfile->frameno+=cfile->play_paused?0:(gint)(((gint64)(mainw->currticks-mainw->startticks)+mainw->deltaticks)/mainw->period);
      if (mainw->noframedrop) cfile->frameno=cfile->frameno>oframeno?oframeno+1:cfile->frameno<oframeno?oframeno-1:cfile->frameno;
    }
    // see if we reached the start (backwards...)
    if (G_UNLIKELY(cfile->frameno<first_frame)) {
      // loop backwards...
      mainw->deltaticks=0;
      mainw->startticks=mainw->currticks-(cfile->frameno-oframeno)*mainw->period;
      if (mainw->ping_pong&&(cfile->pb_fps<0.||mainw->scratch!=SCRATCH_NONE)) {
	dirchange_callback (NULL,NULL,0,0,GINT_TO_POINTER(mainw->playing_sel));
	oframeno=(cfile->frameno=first_frame)-1;
      }
      else {
	oframeno=(cfile->frameno=last_frame)+1;
      }
      if (!mainw->foreign&&cfile->achans>0&&mainw->playing_sel&&!mainw->is_rendering) {
	// audio seek to start
#ifdef ENABLE_JACK
	if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
	  if (mainw->jackd->sample_in_rate<0) {
	    jack_audio_seek_frame(mainw->jackd,cfile->end);
	  }
	  else {
	    jack_audio_seek_frame(mainw->jackd,cfile->frameno);
	  }
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
	  if (mainw->pulsed->in_arate<0) {
	    pulse_audio_seek_frame(mainw->pulsed,cfile->end);
	  }
	  else {
	    pulse_audio_seek_frame(mainw->pulsed,cfile->frameno);
	  }
	}
#endif
      }
    }
    
    // check if we are done
    if (G_UNLIKELY((cfile->frameno>last_frame)&&last_frame>0)) {
      if (mainw->whentostop==STOP_ON_VID_END) {
	mainw->cancelled=CANCEL_VID_END;
      }
      else {

	// loop forwards
	if (mainw->playing_sel) mainw->play_start=cfile->start;
	if (mainw->ping_pong&&(cfile->pb_fps>0.||mainw->scratch!=SCRATCH_NONE)) {
	  dirchange_callback (NULL,NULL,0,0,GINT_TO_POINTER(mainw->playing_sel));
	  oframeno=(cfile->frameno=last_frame)+1;
	}
	else {
	  oframeno=(cfile->frameno=first_frame)-1;
	}
	mainw->deltaticks=0;
	mainw->startticks=mainw->currticks-(cfile->frameno-oframeno)*mainw->period;
	if (!mainw->foreign&&(prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE)&&cfile->achans>0&&mainw->playing_sel) {
	  if (!mainw->loop_cont&&mainw->whentostop==STOP_ON_AUD_END) {
	    mainw->cancelled=CANCEL_AUD_END;
	  }
	  else {
	    // audio seek to start
#ifdef ENABLE_JACK
	    if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&!mainw->is_rendering) {
	      gint audio_frameno;
	      if (mainw->ping_pong&&(cfile->pb_fps<0.||mainw->scratch!=SCRATCH_NONE)&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)) {
		audio_frameno=cfile->frameno;
	      }
	      else audio_frameno=first_frame;
	      jack_audio_seek_frame(mainw->jackd,audio_frameno);
	    }
#endif
#ifdef HAVE_PULSE_AUDIO
	    if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&!mainw->is_rendering) {
	      gint audio_frameno;
	      if (mainw->ping_pong&&(cfile->pb_fps<0.||mainw->scratch!=SCRATCH_NONE)&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)) {
		audio_frameno=cfile->frameno;
	      }
	      else audio_frameno=first_frame;
	      pulse_audio_seek_frame(mainw->pulsed,audio_frameno);
	    }
#endif
	  }
	}
      }
    }

    mainw->scratch=SCRATCH_NONE;
    
    // play next frame
    if (G_LIKELY(mainw->cancelled==CANCEL_NONE)) {
      // calculate the audio 'frame'
      mainw->aframeno=(gint64)(mainw->currticks-mainw->firstticks)*cfile->fps/U_SEC+audio_start;
      if (G_UNLIKELY(mainw->loop_cont&&(mainw->aframeno>(mainw->audio_end?mainw->audio_end:cfile->laudio_time*cfile->fps)))) {
	mainw->firstticks=mainw->currticks;
      }

      mainw->startticks+=(gint64)((cfile->frameno-oframeno)*mainw->period);
      if ((mainw->fixed_fpsd<=0.&&cfile->frameno!=oframeno&&(mainw->vpp==NULL||mainw->vpp->fixed_fpsd<=0.||!mainw->ext_playback))||(mainw->fixed_fpsd>0.&&(mainw->currticks-last_display_ticks)/U_SEC>=1./mainw->fixed_fpsd)||(mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.&&mainw->ext_playback&&(mainw->currticks-last_display_ticks)/U_SEC>=1./mainw->vpp->fixed_fpsd)) {
	load_frame_image(cfile->frameno,oframeno);
	last_display_ticks=mainw->currticks;
      }

#ifdef ENABLE_JACK
      // request for another audio buffer - used only during mt playback
      if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&mainw->abufs_to_fill>0) {
	fill_abuffer_from(mainw->jackd->abufs[mainw->write_abuf],mainw->event_list,NULL,FALSE);
      }
#endif
#ifdef HAVE_PULSE_AUDIO
      // request for another audio buffer - used only during mt playback
      if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&mainw->abufs_to_fill>0) {
	fill_abuffer_from(mainw->pulsed->abufs[mainw->write_abuf],mainw->event_list,NULL,FALSE);
      }
#endif

    }
  }
  
  // paused
  if (G_UNLIKELY(cfile->play_paused)) {
    mainw->startticks=mainw->currticks+mainw->deltaticks;
    if (mainw->ext_playback&&mainw->vpp->send_keycodes!=NULL) (*mainw->vpp->send_keycodes)(pl_key_function);
  }

  if (visible) {
    // fixes a problem with opening preview with bg generator
    if (cfile->proc_ptr==NULL) {
      if (mainw->cancelled==CANCEL_NONE) mainw->cancelled=CANCEL_NO_PROPOGATE;
    }
    else {
      gdouble est_time,fraction_done,timesofar;
      gchar *prog_label;

      if (GTK_IS_SPIN_BUTTON(mainw->framedraw_spinbutton)) gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->framedraw_spinbutton),1,cfile->proc_ptr->frames_done);
      // set the progress bar %

      if (cfile->opening&&cfile->clip_type==CLIP_TYPE_DISK&&!cfile->opening_only_audio&&(cfile->hsize>0||cfile->vsize>0||cfile->frames>0)) {
	gettimeofday(&tv, NULL);
	mainw->currticks=U_SECL*(tv.tv_sec-mainw->startsecs)+tv.tv_usec*U_SEC_RATIO;
	if ((mainw->currticks-last_open_check_ticks)>OPEN_CHECK_TICKS*get_approx_ln((guint)mainw->opening_frames)) {
	  on_check_clicked();
	  last_open_check_ticks=mainw->currticks;
	  if (mainw->opening_frames!=-1) {
	    if (cfile->frames>0&&cfile->frames!=123456789) {
	      fraction_done=(gdouble)mainw->opening_frames/(gdouble)cfile->frames;
	      if (fraction_done>1.) fraction_done=1.;
	      timesofar=(mainw->currticks-mainw->origticks-mainw->timeout_ticks)/U_SEC;
	      est_time=timesofar/fraction_done-timesofar;
	      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(cfile->proc_ptr->progressbar),fraction_done);
	      prog_label=g_strdup_printf(_("\n%d/%d frames opened. Time remaining %u sec.\n"),mainw->opening_frames,cfile->frames,(guint)(est_time+.5));
	    }
	    else {
	      gtk_progress_bar_pulse(GTK_PROGRESS_BAR(cfile->proc_ptr->progressbar));
	      prog_label=g_strdup_printf(_("\n%d frames opened.\n"),mainw->opening_frames);
	    }
	    gtk_label_set_text(GTK_LABEL(cfile->proc_ptr->label3),prog_label);
	    g_free(prog_label);
	  }
	}
      }

      else if (visible&&cfile->proc_ptr->frames_done>=cfile->progress_start&&cfile->proc_ptr->frames_done<=cfile->progress_end&&cfile->progress_end>0&&!mainw->effects_paused) {
	if (progress_count++>=PROG_LOOP_VAL) {
	  gettimeofday(&tv, NULL);
	  mainw->currticks=U_SECL*(tv.tv_sec-mainw->startsecs)+tv.tv_usec*U_SEC_RATIO;
	  fraction_done=(cfile->proc_ptr->frames_done-cfile->progress_start)/(cfile->progress_end-cfile->progress_start+1.);
	  if (fraction_done>1.) fraction_done=1.;
	  if (cfile->proc_ptr->frames_done>disp_frames_done) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(cfile->proc_ptr->progressbar),fraction_done);
	  timesofar=(mainw->currticks-mainw->origticks-mainw->timeout_ticks)/U_SEC;
	  est_time=timesofar/fraction_done-timesofar;
	  prog_label=g_strdup_printf(_("\n%d%% done. Time remaining: %u sec\n"),(gint)(fraction_done*100.),(guint)(est_time+.5));
	  gtk_label_set_text(GTK_LABEL(cfile->proc_ptr->label3),prog_label);
	  g_free(prog_label);
	  progress_count=0;
	  disp_frames_done=cfile->proc_ptr->frames_done;
	}
      }
      else {
	if (progress_count++>=PROG_LOOP_VAL) {
	  gtk_progress_bar_pulse(GTK_PROGRESS_BAR(cfile->proc_ptr->progressbar));
	  progress_count=0;
	}
      }
    }
    frames_done=cfile->proc_ptr->frames_done;


    if (cfile->clip_type==CLIP_TYPE_FILE&&cfile->fx_frame_pump>0&&cfile->progress_start+frames_done>=cfile->fx_frame_pump) {
      gint vend=cfile->fx_frame_pump+FX_FRAME_PUMP_VAL;
      if (vend>cfile->progress_end) vend=cfile->progress_end;
      if (vend>cfile->fx_frame_pump) {
	virtual_to_images(mainw->current_file,cfile->fx_frame_pump,vend);
	cfile->fx_frame_pump=vend;
      }
      progress_count=PROG_LOOP_VAL;
    }


  }

  if (G_LIKELY(mainw->cancelled==CANCEL_NONE)) {
    while (g_main_context_iteration(NULL,FALSE));
    if (G_UNLIKELY(mainw->cancelled!=CANCEL_NONE)) {
      cancel_process(visible);
      return FALSE;
    }
    return TRUE;
  }
  cancel_process(visible);

  return FALSE;
}


gboolean do_progress_dialog(gboolean visible, gboolean cancellable, const gchar *text) {
  // monitor progress, return FALSE if the operation was cancelled
  FILE *infofile=NULL;
  gboolean finished=FALSE;
  gchar *mytext=g_strdup(text);

  event_start=0;
  audio_start=mainw->play_start;
  if (visible) accelerators_swapped=FALSE;
  frames_done=disp_frames_done=0;
  last_display_ticks=0;

  mainw->cevent_tc=0;

  progress_count=0;

  if (!visible) {
    mainw->startticks=mainw->startsecs=mainw->currticks=0;
    if (mainw->event_list!=NULL) {
      // this is for audio, work out the apparent frame when we have variable fps
      audio_start=calc_time_from_frame(mainw->current_file,mainw->play_start)*cfile->fps;
    }
    reset_frame_and_clip_index();
  }

  if (prefs->show_player_stats) {
    mainw->fps_measure=0.;
  }

  mainw->cancelled=CANCEL_NONE;   
  mainw->error=FALSE;
  clear_mainw_msg();
  if (!mainw->preview) mainw->timeout_ticks=0;

  if (visible) {
    mainw->is_processing=TRUE;
    desensitize();
    procw_desensitize();
    lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);

    cfile->proc_ptr=create_processing (mytext);
    if (mytext!=NULL) g_free(mytext);

    gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(cfile->proc_ptr->progressbar),.01);
    if (mainw->show_procd) gtk_widget_show(cfile->proc_ptr->processing);
    
    cfile->proc_ptr->frames_done=0;
    
    if (cancellable) {
      gtk_widget_show (cfile->proc_ptr->cancel_button);
    }
    else lives_set_cursor_style(LIVES_CURSOR_BUSY,GDK_WINDOW(cfile->proc_ptr->processing->window));

    if (cfile->opening&&capable->has_sox&&mainw->playing_file==-1) {
      //gtk_widget_show (cfile->proc_ptr->preview_button);
      if (mainw->preview_box!=NULL) gtk_tooltips_set_tip (mainw->tooltips, mainw->p_playbutton,_ ("Preview"), NULL);
      gtk_tooltips_set_tip (mainw->tooltips, mainw->m_playbutton,_ ("Preview"), NULL);
      gtk_widget_remove_accelerator (mainw->playall, mainw->accel_group, GDK_p, 0);
      gtk_widget_add_accelerator (cfile->proc_ptr->preview_button, "clicked", mainw->accel_group, GDK_p, 0, 0);
      accelerators_swapped=TRUE;
    }

    if (cfile->clip_type==CLIP_TYPE_FILE&&cfile->fx_frame_pump>0) {
      gint vend=cfile->fx_frame_pump+FX_FRAME_PUMP_VAL;
      if (vend>cfile->progress_end) vend=cfile->progress_end;
      if (vend>cfile->fx_frame_pump) {
	virtual_to_images(mainw->current_file,cfile->fx_frame_pump,vend);
	cfile->fx_frame_pump+=FX_FRAME_PUMP_VAL>>1;
      }
    }

  }

  if (cfile->next_event!=NULL) event_start=get_event_timecode(cfile->next_event);

  if (!visible) {
#ifdef ENABLE_JACK_TRANSPORT
    if (mainw->jack_can_stop&&mainw->multitrack==NULL&&(prefs->jack_opts&JACK_OPTS_TIMEBASE_CLIENT)&&(prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)&&!(mainw->record&&!(prefs->rec_opts&REC_FRAMES)&&cfile->next_event==NULL)) {
      weed_timecode_t ntc=jack_transport_get_time()*U_SEC;
      cfile->last_frameno=1;
      mainw->currticks=mainw->firstticks=0;
      mainw->play_start=calc_new_playback_position(mainw->current_file,0,&ntc);
    }
#endif
    cfile->last_frameno=cfile->frameno=mainw->play_start;
    mainw->deltaticks=0;
  }

  gettimeofday(&tv, NULL);
  mainw->video_seek_ready=TRUE;
  
  // [IMPORTANT] we subtract this from every calculation to make the numbers smaller
  mainw->startsecs=tv.tv_sec;
  
  if (!mainw->foreign&&!visible) {
#ifdef ENABLE_JACK
    if (prefs->audio_player==AUD_PLAYER_JACK&&cfile->achans>0&&cfile->laudio_time>0.&&!mainw->is_rendering&&!(cfile->opening&&!mainw->preview)&&mainw->jackd!=NULL&&mainw->jackd->playing_file>-1) jack_audio_seek_frame(mainw->jackd,mainw->play_start);
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player==AUD_PLAYER_PULSE&&cfile->achans>0&&cfile->laudio_time>0.&&!mainw->is_rendering&&!(cfile->opening&&!mainw->preview)&&mainw->pulsed!=NULL&&mainw->pulsed->playing_file>-1) pulse_audio_seek_frame(mainw->pulsed,mainw->play_start);
#endif
  }

  // mainw->origticks is our base for quantising (and is constant for each playback session)
  mainw->origticks=mainw->firstticks=mainw->startticks=tv.tv_usec*U_SEC_RATIO;
  last_open_check_ticks=mainw->origticks;

  if (mainw->multitrack!=NULL&&!mainw->multitrack->is_rendering) mainw->origticks-=get_event_timecode(mainw->multitrack->pb_start_event); // playback start from middle of multitrack

  //try to open info file - or if internal_messaging is TRUE, we get mainw->msg
  // from the mainw->progress_fn function

  do {
    while (!mainw->internal_messaging&&((!visible&&(mainw->whentostop!=STOP_ON_AUD_END||prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE))||!g_file_test(cfile->info_file,G_FILE_TEST_EXISTS))) {

      // just pulse the progress bar
      if (!process_one(visible)) {
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return FALSE;
      }

      if (mainw->iochan!=NULL&&progress_count==0) {
	// pump data from stdout to textbuffer
	pump_io_chan(mainw->iochan);
      }

    }
    if (!mainw->internal_messaging) {
      if ((infofile=fopen(cfile->info_file,"r"))) {
	// OK, now we might have some frames
	dummychar=fgets(mainw->msg,512,infofile);
	fclose(infofile);
      }
    }
    else (*mainw->progress_fn)(FALSE);

#ifdef DEBUG
    g_print("msg %s\n",mainw->msg);
#endif



    if (visible&&(!accelerators_swapped||cfile->opening)&&cancellable&&!cfile->nopreview) {
      if (!cfile->opening||(capable->has_sox&&mainw->playing_file==-1)) gtk_widget_show (cfile->proc_ptr->preview_button);
      if (cfile->opening) gtk_widget_show (cfile->proc_ptr->stop_button);
      else {
	gtk_widget_show (cfile->proc_ptr->pause_button);

	gtk_widget_grab_default (cfile->proc_ptr->preview_button);
	if (mainw->preview_box!=NULL) gtk_tooltips_set_tip (mainw->tooltips, mainw->p_playbutton,_ ("Preview"), NULL);
	gtk_tooltips_set_tip (mainw->tooltips, mainw->m_playbutton,_ ("Preview"), NULL);
	gtk_widget_remove_accelerator (mainw->playall, mainw->accel_group, GDK_p, 0);
	gtk_widget_add_accelerator (cfile->proc_ptr->preview_button, "clicked", mainw->accel_group, GDK_p, 0, 0);
	accelerators_swapped=TRUE;
      }
    }

    if (strncmp(mainw->msg,"completed",8)&&strncmp(mainw->msg,"error",5)&&(visible||((strncmp(mainw->msg,"video_ended",11)||mainw->whentostop!=STOP_ON_VID_END)&&(strncmp(mainw->msg,"audio_ended",11)||mainw->preview||mainw->whentostop!=STOP_ON_AUD_END)))) {
      // processing not yet completed...
      if (visible) {
	// last frame processed ->> will go from cfile->start to cfile->end
	gint numtok=get_token_count (mainw->msg,'|');
	
	if (numtok>1) {
	  gchar **array=g_strsplit(mainw->msg,"|",numtok);
	  cfile->proc_ptr->frames_done=atoi(array[0]);
	  if (numtok==2&&strlen(array[1])>0) cfile->progress_end=atoi(array[1]);
	  else if (numtok==5&&strlen(array[4])>0) {
	    // rendered generators
	    cfile->start=cfile->undo_start=1;
	    cfile->frames=cfile->end=cfile->undo_end=atoi(array[0]);
	    cfile->hsize=atoi(array[1]);
	    cfile->vsize=atoi(array[2]);
	    cfile->fps=cfile->pb_fps=strtod(array[3],NULL);
	    if (cfile->fps==0.) cfile->fps=cfile->pb_fps=prefs->default_fps;
	    cfile->progress_end=atoi(array[4]);
	  }
	  g_strfreev(array);
	}
	else cfile->proc_ptr->frames_done=atoi(mainw->msg);
      }
      if (!process_one(visible)) {
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return FALSE;
      }

      if (mainw->iochan!=NULL&&progress_count==0) {
	// pump data from stdout to textbuffer
	pump_io_chan(mainw->iochan);
      }

      g_usleep(prefs->sleep_time);
    }
    else finished=TRUE;
  } while (!finished);

#ifdef DEBUG
  g_print ("exit pt 3 %s\n",mainw->msg);
#endif

  //play/operation ended
  if (visible) {
    if (cfile->clip_type==CLIP_TYPE_DISK&&(mainw->cancelled!=CANCEL_NO_MORE_PREVIEW||!cfile->opening)) {
      unlink(cfile->info_file);
    }
    if (mainw->preview_box!=NULL&&!mainw->preview) gtk_tooltips_set_tip (mainw->tooltips, mainw->p_playbutton,_ ("Play all"), NULL);
    if (accelerators_swapped) {
      if (!mainw->preview) gtk_tooltips_set_tip (mainw->tooltips, mainw->m_playbutton,_ ("Play all"), NULL);
      gtk_widget_remove_accelerator (cfile->proc_ptr->preview_button, mainw->accel_group, GDK_p, 0);
      gtk_widget_add_accelerator (mainw->playall, "activate", mainw->accel_group, GDK_p, 0, GTK_ACCEL_VISIBLE);
      accelerators_swapped=FALSE;
    }
    if (cfile->proc_ptr!=NULL) {
      const gchar *btext=NULL;
      if (mainw->iochan!=NULL) btext=text_view_get_text(mainw->optextview); 
      gtk_widget_destroy(cfile->proc_ptr->processing);
      g_free(cfile->proc_ptr);
      cfile->proc_ptr=NULL;
      if (btext!=NULL) {
	text_view_set_text(mainw->optextview,btext);
	g_free((gchar *)btext);
      }
    }
    mainw->is_processing=FALSE;
    if (!(cfile->menuentry==NULL)) {
      // note - for operations to/from clipboard (file 0) we
      // should manually call sensitize() after operation
      sensitize();
    }
  }
  else {
    if (prefs->show_player_stats) {
      if (mainw->fps_measure>0.) {
	gettimeofday(&tv, NULL);
	mainw->fps_measure/=(gdouble)(U_SECL*(tv.tv_sec-mainw->startsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origticks)/U_SEC;
      }
    }
    mainw->is_processing=TRUE;
  }
  
  // get error message (if any)
  if (!strncmp(mainw->msg,"error",5)) {
    int i;
    gchar **array;
    gint numtok=get_token_count (mainw->msg,'|');
    
    array=g_strsplit(mainw->msg,"|",numtok);
    g_snprintf(mainw->msg,512,"\n\n");
    for (i=1;i<numtok;i++) {
      g_strappend(mainw->msg,512,_(array[i]));
      g_strappend(mainw->msg,512,"\n");
    }
    g_strfreev(array);
    mainw->error=TRUE;
  }
#ifdef DEBUG
  g_print("exiting progress dialogue\n");
#endif
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
  return TRUE;
}



void do_auto_dialog (const gchar *text, gint type) {
  // type 0 = normal auto_dialog
  // type 1 = countdown dialog for audio recording
  // type 2 = normal with cancel

  FILE *infofile=NULL;
  gint count=0;
  guint64 time=0,end_time=1;
  gchar *label_text;
  gint time_rem,last_time_rem=10000000;
  process *proc_ptr;
  gchar *mytext=g_strdup(text);

  proc_ptr=create_processing (mytext);
  if (mytext!=NULL) g_free(mytext);
  gtk_widget_hide (proc_ptr->stop_button);
  gtk_window_set_modal (GTK_WINDOW (proc_ptr->processing), TRUE);
     
  if (type==2) {
    gtk_widget_show (proc_ptr->cancel_button);
    gtk_widget_hide (proc_ptr->pause_button);
    mainw->cancel_type=CANCEL_SOFT;
  }

  gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(proc_ptr->progressbar),.01);
  gtk_widget_show(proc_ptr->processing);
  
  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);
  while (g_main_context_iteration(NULL,FALSE));

  lives_set_cursor_style(LIVES_CURSOR_BUSY,proc_ptr->processing->window);

  if (type==0) {
    clear_mainw_msg();
    count=100000./prefs->sleep_time;  // don't want to flash too fast...
  }
  else if (type==1) {
    gtk_widget_show(proc_ptr->stop_button);
    //gtk_widget_show(proc_ptr->pause_button);
    gtk_widget_show(proc_ptr->cancel_button);
    if (mainw->rec_end_time!=-1.) {
      while (g_main_context_iteration(NULL,FALSE));
      gettimeofday(&tv, NULL);
      time=tv.tv_sec*1000000.+tv.tv_usec; // time in microseconds
      end_time=time+mainw->rec_end_time*1000000.;
    }
  }

  while ((type==1&&mainw->cancelled==CANCEL_NONE)||(type==0&&!(infofile=fopen(cfile->info_file,"r")))) {
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(proc_ptr->progressbar));
    while (g_main_context_iteration(NULL,FALSE));
    g_usleep(prefs->sleep_time);
    if (type==1&&mainw->rec_end_time!=-1.) {
      gettimeofday(&tv, NULL);
      time=tv.tv_sec*1000000.+tv.tv_usec; // time in microseconds
      time_rem=(gint)((gdouble)(end_time-time)/1000000.+.5);
      if (time_rem>=0&&time_rem<last_time_rem) {
	label_text=g_strdup_printf(_("\nTime remaining: %d sec"),time_rem);
	gtk_label_set_text(GTK_LABEL(proc_ptr->label2),label_text);
	g_free(label_text);
	last_time_rem=time_rem;
      }
    }
  }
  

  if (type==0) {
    dummychar=fgets(mainw->msg,512,infofile);
    fclose(infofile);
    if (cfile->clip_type==CLIP_TYPE_DISK) unlink(cfile->info_file);

    while (count>0) {
      gtk_progress_bar_pulse(GTK_PROGRESS_BAR(proc_ptr->progressbar));
      while (g_main_context_iteration(NULL,FALSE));
      g_usleep(prefs->sleep_time);
      count--;
    }
  }

  if (type==2) {
    while (!mainw->cancelled) {
      gtk_progress_bar_pulse(GTK_PROGRESS_BAR(proc_ptr->progressbar));
      while (g_main_context_iteration(NULL,FALSE));
      g_usleep(prefs->sleep_time);
    }
  }

  if (proc_ptr!=NULL) {
    gtk_widget_destroy(proc_ptr->processing);
    g_free(proc_ptr);
  }

  if (type==2) mainw->cancel_type=CANCEL_KILL;

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);

  if (type==1||type==2) return;

  // get error message (if any)
  if (!strncmp(mainw->msg,"error",5)) {
    gchar **array=g_strsplit(mainw->msg,"|",5);
    g_snprintf(mainw->msg,512,"\n\n%s\n\n%s\n\n%s\n\n%s\n",array[1],array[2],array[3],array[4]);
    g_strfreev(array);
    mainw->error=TRUE;
  }
}




void
too_many_files(void) {
  gchar *warn=g_strdup_printf(_ ("\nSorry, LiVES can only open %d files at once.\nPlease close a file and then try again."),MAX_FILES);
  do_error_dialog(warn);
  g_free(warn);
}

void
tempdir_warning (void) {
  gchar *tmp,*com=g_strdup_printf(_ ("LiVES was unable to write to its temporary directory.\n\nThe current temporary directory is:\n\n%s\n\nPlease make sure you can write to this directory."),(tmp=g_filename_to_utf8(prefs->tmpdir,-1,NULL,NULL,NULL)));
  g_free(tmp);
  if (mainw!=NULL&&mainw->is_ready) {
    do_error_dialog(com);
  }
  g_free(com);
}


void
do_encoder_sox_error(void) {
  do_error_dialog (_ ("Audio resampling is required for this format.\nPlease install 'sox'\nOr switch to another encoder format in Tools | Preferences | Encoding\n"));
}


void do_encoder_acodec_error (void) {
  do_error_dialog (_ ("\n\nThis encoder/format cannot use the requested audio codec.\nPlease set the audio codec in Tools|Preferences|Encoding\n"));
}


void do_layout_scrap_file_error(void) {
  do_blocking_error_dialog(_("This layout includes generated frames.\nIt cannot be saved, you must render it to a clip first.\n"));
}




gboolean rdet_suggest_values (gint width, gint height, gdouble fps, gint fps_num, gint fps_denom, gint arate, gint asigned, gboolean anr, gboolean ignore_fps) {
  gchar *msg1=g_strdup_printf (_ ("\n\nDue to restrictions in the %s format\n"),prefs->encoder.of_desc);
  gchar *msg2=g_strdup ("");
  gchar *msg3=g_strdup ("");
  gchar *msg4=g_strdup ("");
  gchar *msg5=g_strdup ("");
  gchar *msg6=g_strdup ("");
  gchar *msg7=g_strdup ("");
  gchar *msg8=g_strdup ("");
  gchar *msg_a;
  gboolean ochange=FALSE;

  gboolean ret;

  GtkWidget *prep_dialog;

  if ((asigned==1&&rdet->aendian==AFORM_UNSIGNED)||(asigned==2&&rdet->aendian==AFORM_SIGNED)||(fps>0.&&fps!=rdet->fps)||(fps_denom>0&&(fps_num*1.)/(fps_denom*1.)!=rdet->fps)||(!anr&&(rdet->width!=width||rdet->height!=height)&&height*width>0)||(arate!=rdet->arate&&arate>0)) {
    g_free (msg2);
    msg2=g_strdup (_ ("LiVES recommends the following settings:\n\n"));
    if ((asigned==1&&rdet->aendian==AFORM_UNSIGNED)||(asigned==2&&rdet->aendian==AFORM_SIGNED)||(arate>0&&arate!=rdet->arate)) {
      gchar *sstring;

      if (asigned==1&&rdet->aendian==AFORM_UNSIGNED) sstring=g_strdup(_(", signed"));
      else if (asigned==2&&rdet->aendian==AFORM_SIGNED) sstring=g_strdup(_(", unsigned"));
      else sstring=g_strdup("");

      ochange=TRUE;
      g_free (msg3);
      msg3=g_strdup_printf (_ ("Use an audio rate of %d Hz%s\n"),arate,sstring);
      g_free(sstring);
    }
    if (!ignore_fps) {
      ochange=TRUE;
      if (fps>0&&fps!=rdet->fps) {
	g_free (msg4);
	msg4=g_strdup_printf (_ ("Set video rate to %.3f frames per second\n"),fps);
      }
      else if (fps_denom>0&&(fps_num*1.)/(fps_denom*1.)!=rdet->fps) {
	g_free (msg4);
	msg4=g_strdup_printf (_ ("Set video rate to %d:%d frames per second\n"),fps_num,fps_denom);
      }
    }
    if (!anr&&((rdet->width!=width||rdet->height!=height)&&height*width>0)) {
      g_free (msg5);
      msg5=g_strdup_printf (_ ("Set video size to %d x %d pixels\n"),width,height);
      mainw->fx1_bool=TRUE;
    }
  }
  if (anr||arate<0) {
    if (arate<1||((rdet->width!=width||rdet->height!=height)&&height*width>0)) {
      g_free (msg6);
      if (!ochange) anr=FALSE;
      msg6=g_strdup (_ ("\nYou may wish to:\n"));
      if ((rdet->width!=width||rdet->height!=height)&&height*width>0) {
	g_free (msg7);
	msg7=g_strdup_printf(_ ("resize video to %d x %d pixels\n"),width,height);
      }
      else anr=FALSE;
      if (arate<1) {
	g_free (msg8);
	msg8=g_strdup(_("disable audio, since the target encoder cannot encode audio\n"));
      }
    }
  }
  msg_a=g_strconcat (msg1,msg2,msg3,msg4,msg5,msg6,msg7,msg8,NULL);
  g_free (msg1);
  g_free (msg2);
  g_free (msg3);
  g_free (msg4);
  g_free (msg5);
  g_free (msg6);
  g_free (msg7);
  g_free (msg8);
  prep_dialog=create_encoder_prep_dialog(msg_a,NULL,anr);
  g_free (msg_a);
  ret=(gtk_dialog_run(GTK_DIALOG (prep_dialog))==GTK_RESPONSE_OK);
  gtk_widget_destroy (prep_dialog);
  return ret;
}



gboolean 
do_encoder_restrict_dialog (gint width, gint height, gdouble fps, gint fps_num, gint fps_denom, gint arate, gint asigned, gboolean anr) {
  gchar *msg1=g_strdup_printf (_ ("\n\nDue to restrictions in the %s format\n"),prefs->encoder.of_desc);
  gchar *msg2=g_strdup ("");
  gchar *msg3=g_strdup ("");
  gchar *msg4=g_strdup ("");
  gchar *msg5=g_strdup ("");
  gchar *msg6=g_strdup ("");
  gchar *msg7=g_strdup ("");
  gchar *msg_a,*msg_b=NULL;

  gboolean ret;

  GtkWidget *prep_dialog;

  gint carate,chsize,cvsize;
  gdouble cfps;

  if (rdet!=NULL) {
    carate=rdet->arate;
    chsize=rdet->width;
    cvsize=rdet->height;
    cfps=rdet->fps;
  }
  else {
    carate=cfile->arate;
    chsize=cfile->hsize;
    cvsize=cfile->vsize;
    cfps=cfile->fps;
  }


  if (asigned!=0||(arate>0&&arate!=carate)||(fps>0.&&fps!=cfps)||(fps_denom>0&&(fps_num*1.)/(fps_denom*1.)!=cfps)||(!anr&&(chsize!=width||cvsize!=height)&&height*width>0)) {
    g_free (msg2);
    msg2=g_strdup (_ ("LiVES must:\n"));
    if (asigned!=0||(arate>0&&arate!=carate)) {
      gchar *sstring;
      if (asigned==1) sstring=g_strdup(_(", signed"));
      else if (asigned==2) sstring=g_strdup(_(", unsigned"));
      else sstring=g_strdup("");

      g_free (msg3);
      msg3=g_strdup_printf (_ ("resample audio to %d Hz%s\n"),arate,sstring);
      g_free(sstring);

    }
    if (fps>0&&fps!=cfps) {
      g_free (msg4);
      msg4=g_strdup_printf (_ ("resample video to %.3f frames per second\n"),fps);
    }
    else if (fps_denom>0&&(fps_num*1.)/(fps_denom*1.)!=cfps) {
      g_free (msg4);
      msg4=g_strdup_printf (_ ("resample video to %d:%d frames per second\n"),fps_num,fps_denom);
    }
    if (!anr&&((chsize!=width||cvsize!=height)&&height*width>0)) {
      g_free (msg5);
      msg5=g_strdup_printf (_ ("resize video to %d x %d pixels\n"),width,height);
      mainw->fx1_bool=TRUE;
    }
  }
  if (anr) {
    if ((chsize!=width||cvsize!=height)&&height*width>0) {
      g_free (msg6);
      g_free (msg7);
      msg6=g_strdup(_ ("\nYou may wish to:\n"));
      msg7=g_strdup_printf (_ ("Set video size to %d x %d pixels\n"),width,height);
    }
    else anr=FALSE;
  }
  msg_a=g_strconcat (msg1,msg2,msg3,msg4,msg5,msg6,msg7,NULL);
  if (mainw->save_all) {
    msg_b=g_strdup (_ ("\nYou will be able to undo these changes afterwards.\n\nClick `OK` to proceed, `Cancel` to abort.\n\n"));
  }
  else {
    msg_b=g_strdup (_ ("\nChanges applied to the selection will not be permanent.\n\n"));
  }
  g_free (msg1);
  g_free (msg2);
  g_free (msg3);
  g_free (msg4);
  g_free (msg5);
  g_free (msg6);
  g_free (msg7);
  prep_dialog=create_encoder_prep_dialog(msg_a,msg_b,anr);
  g_free (msg_a);
  if (msg_b!=NULL) g_free (msg_b);
  ret=(gtk_dialog_run(GTK_DIALOG (prep_dialog))==GTK_RESPONSE_OK);
  gtk_widget_destroy (prep_dialog);
  return ret;
}


void
perf_mem_warning(void) {
  do_error_dialog(_ ("\n\nLiVES was unable to record a performance. There is currently insufficient memory available.\nTry recording for just a selection of the file."));
}

gboolean
do_clipboard_fps_warning(void) {
  if (prefs->warning_mask&WARN_MASK_FPS) {
    return TRUE;
  }
  return do_warning_dialog_with_check(_ ("The playback speed (fps), or the audio rate\n of the clipboard does not match\nthe playback speed or audio rate of the clip you are inserting into.\n\nThe insertion will be adjusted to fit into the clip.\n\nPlease press Cancel to abort the insert, or OK to continue."),WARN_MASK_FPS);
}

gboolean
do_yuv4m_open_warning(void) {
  if (prefs->warning_mask&WARN_MASK_OPEN_YUV4M) {
    return TRUE;
  }
  return do_warning_dialog_with_check(_ ("When opening a yuvmpeg stream, you should first create a fifo file and then write yuv4mpeg frames to it.\nLiVES WILL HANG until frames are received.\nYou should only click OK if you understand what you are doing, otherwise, click Cancel."),WARN_MASK_OPEN_YUV4M);
}



gboolean
do_comments_dialog (void) {
  gboolean response;

  commentsw=create_comments_dialog();
  if ((response=(gtk_dialog_run(GTK_DIALOG (commentsw->comments_dialog))==GTK_RESPONSE_OK))) {
    g_snprintf (cfile->title,256,"%s",gtk_entry_get_text (GTK_ENTRY (commentsw->title_entry)));
    g_snprintf (cfile->author,256,"%s",gtk_entry_get_text (GTK_ENTRY (commentsw->author_entry)));
    g_snprintf (cfile->comment,256,"%s",gtk_entry_get_text (GTK_ENTRY (commentsw->comment_entry)));
  }
  gtk_widget_destroy (commentsw->comments_dialog);
  g_free (commentsw);
  
  return response;
}


void 
do_keys_window (void) {
  gchar *tmp=g_strdup(_ ("Show Keys"));
  do_text_window (tmp,_ ("You can use the following keys during playback to control LiVES:-\n\nRecordable keys (press 'r' before playback to make a recording)\n-----------------------\nctrl-left                     skip back\nctrl-right                   skip forwards\nctrl-up                      faster/increase effect\nctrl-down                 slower/decrease effect\nctrl-enter                    reset frame rate\nctrl-space                reverse direction\nctrl-backspace         freeze frame\nn                             nervous\nctrl-page up            previous clip\nctrl-page down        next clip\n\nctrl-1                       toggle real-time effect 1\nctrl-2                       toggle real-time effect 2\n                                          ...etc...\nctrl-0                       real-time effects off\n\nk           grab keyboard for last activated effect\nm         switch effect mode (when effect has keyboard grab)\nx                     swap background/foreground\nf1                           store/switch to clip mnemonic 1\nf2                           store/switch to clip mnemonic 2\n                                          ...etc...\nf12                          clear function keys\n\n\n Other playback keys\n-----------------------------\np                             play all\ny                             play selection\nq                             stop\nf                               fullscreen\ns                              separate window\nd                             double size\ng                             ping pong loops\n"));
  g_free(tmp);
}



void 
do_mt_keys_window (void) {
  gchar *tmp=g_strdup(_ ("Multitrack Keys"));
  do_text_window (tmp,_ ("You can use the following keys to control the multitrack window:-\n\nctrl-left-arrow              move timeline cursor left 1 second\nctrl-right-arrow            move timeline cursor right 1 second\nshift-left-arrow            move timeline cursor left 1 frame\nshift-right-arrow          move timeline cursor right 1 frame\nctrl-up-arrow               move current track up\nctrl-down-arrow           move current track down\nctrl-page-up                select next clip\nctrl-page-down            select previous clip\nctrl-space                    select/deselect current track\nctrl-plus                       zoom in\nctrl-minus                    zoom out\nm                                 make a mark on the timeline (during playback)\nw                                 rewind to play start.\n\nFor other keys, see the menus.\n"));
  g_free(tmp);
}








void 
do_messages_window (void) {
  GtkTextIter start_iter,end_iter;
  gtk_text_buffer_get_start_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mainw->textview1)),&start_iter);
  gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mainw->textview1)),&end_iter);
  do_text_window (_ ("Message History"),gtk_text_iter_get_text (&start_iter,&end_iter));
}


void 
do_text_window (const gchar *title, const gchar *text) {
  text_window *textwindow=create_text_window (title,text,NULL);
  gtk_widget_show (textwindow->dialog);
}



void 
do_firstever_dialog (void) {
  startup_message_nonfatal (g_strdup (_ ("\n\nWelcome to LiVES. If this is your first time using LiVES, please check all the\nPreferences using Control-P. In particular make sure the temporary directory is located somewhere with plenty of diskspace.\nHave fun !\n")));
}


void 
do_upgrade_error_dialog (void) {
  startup_message_nonfatal (g_strdup (_("After upgrading/installing, you may need to adjust the <prefix_dir> setting in your ~/.lives file")));
}


void do_rendered_fx_dialog(void) {
  gchar *msg=g_strdup_printf(_("\n\nLiVES could not find any rendered effect plugins.\nPlease make sure you have them installed in\n%s%s%s\nor change the value of <lib_dir> in ~/.lives\n"),prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_RENDERED_EFFECTS_BUILTIN);
    do_error_dialog_with_check(msg,WARN_MASK_RENDERED_FX);
    g_free(msg);
}

void do_audio_import_error(void) {
  do_error_dialog(_ ("Sorry, unknown audio type.\n\n (Filenames must end in .mp3, .ogg, .wav, .mod, .xm or .it)"));
  d_print(_ ("failed (unknown type)\n"));
}


gboolean prompt_remove_layout_files(void) {
  return (do_warning_dialog(_("\nDo you wish to remove the layout files associated with this set ?\nClick OK to remove them, or Cancel to leave them.\n(They will not be usable without the set).\n")));
}


gboolean do_set_duplicate_warning (gchar *new_set) {
  gchar *msg=g_strdup_printf(_("\nA set entitled %s already exists.\nClick OK to add the current clips and layouts to the existing set.\nClick Cancel to pick a new name.\n"),new_set);
  gboolean retcode=do_warning_dialog_with_check(msg,WARN_MASK_DUPLICATE_SET);
  g_free(msg);
  return retcode;
}

gboolean do_layout_alter_frames_warning(void) {
  return do_warning_dialog(_("\nFrames from this clip are used in some multitrack layouts.\nAre you sure you wish to continue ?\n."));
}

gboolean do_layout_alter_audio_warning(void) {
  return do_warning_dialog(_("\nAudio from this clip is used in some multitrack layouts.\nAre you sure you wish to continue ?\n."));
}

gboolean do_original_lost_warning(gchar *fname) {
  gchar *msg=g_strdup_printf(_("\nThe original file\n%s\ncould not be found.\nIf this file has been moved, please copy it back to its original location\nand click 'OK' to try again.\nOtherwise click cancel to skip loading this file.\n"),fname);
  gboolean retcode=do_warning_dialog(msg);
  g_free(msg);
  return retcode;
}

void do_no_decoder_error(gchar *fname) {
  gchar *msg=g_strdup_printf(_("\n\nLiVES could not find a required decoder plugin for the clip\n%s\nThe clip could not be loaded.\n"),fname);
    do_error_dialog(msg);
    g_free(msg);
}


void do_jack_noopen_warn(void) {
  do_blocking_error_dialog(_("\nUnable to start up jack. Please ensure that alsa is set up correctly on your machine\nand also that the soundcard is not in use by another program.\n"));
}


void do_jack_noopen_warn2(void) {
  do_blocking_error_dialog(_("\nAlternately, you can restart LiVES and select another audio player.\n"));
}


void do_mt_backup_space_error(lives_mt *mt, gint memreq_mb) {
  gchar *msg=g_strdup_printf(_("\n\nLiVES needs more backup space for this layout.\nYou can increase the value in Preferences/Multitrack.\nIt is recommended to increase it to at least %d MB"),memreq_mb);
    do_error_dialog_with_check_transient(msg,TRUE,WARN_MASK_MT_BACKUP_SPACE,GTK_WINDOW(mt->window));
    g_free(msg);
}

gboolean do_set_rename_old_layouts_warning(gchar *new_set) {
  gchar *msg=g_strdup_printf(_("\nSome old layouts for the set %s already exist.\nIt is recommended that you delete them.\nClick OK to delete them, Cancel to leave them on the disk.\n"),new_set);
  gboolean retcode=do_warning_dialog(msg);
  g_free(msg);
  return retcode;
}

void do_mt_undo_mem_error(void) {
  do_error_dialog(_("\nLiVES was unable to reserve enough memory for multitrack undo.\nEither close some other applications, or reduce the undo memory\nusing Preferences/Multitrack/Undo Memory\n"));
}

void do_mt_undo_buf_error(void) {
  do_error_dialog(_("\nOut of memory for undo.\nYou may need to increase the undo memory\nusing Preferences/Multitrack/Undo Memory\n"));
}

void do_mt_set_mem_error(gboolean has_mt, gboolean trans) {
  gchar *msg1=(_("\nLiVES was unable to reserve enough memory for the multitrack undo buffer.\n"));
  gchar *msg2;
  gchar *msg3=(_("or enter a smaller value.\n"));
  gchar *msg;
  if (has_mt) msg2=(_("Try again from the clip editor, try closing some other applications\n"));
  else msg2=(_("Try closing some other applications\n"));

  msg=g_strdup_printf("%s%s%s",msg1,msg2,msg3);

  if (!trans) do_blocking_error_dialog(msg);
  else do_error_dialog_with_check_transient(msg,TRUE,0,GTK_WINDOW(prefsw->prefs_dialog));
  g_free(msg);
}


void do_mt_audchan_error(gint warn_mask) {
  do_error_dialog_with_check(_("Multitrack is set to 0 audio channels, but this layout has audio.\nYou should adjust the audio settings from the Tools menu.\n"),warn_mask);
}

void do_mt_no_audchan_error(void) {
  do_error_dialog(_("The current layout has audio, so audio channels may not be set to zero.\n"));
}

void do_mt_no_jack_error(gint warn_mask) {
  do_error_dialog_with_check(_("Multitrack audio preview is only available with the\n\"jack\" or \"pulse audio\" audio player.\nYou can set this in Tools|Preferences|Playback."),warn_mask);
}

gboolean do_mt_rect_prompt(void) {
  return do_warning_dialog(_("Errors were detected in the layout (which may be due to transferring from another system, or from an older version of LiVES).\nShould I try to repair the disk copy of the layout ?\n"));
}

void do_bad_layout_error(void) {
  do_error_dialog(_("LiVES was unable to load the layout.\nSorry.\n"));
}  



void do_audrate_error_dialog(void) {
  do_error_dialog(_ ("\n\nAudio rate must be greater than 0.\n"));
}

gboolean do_event_list_warning(void) {
  return do_warning_dialog(_("\nEvent list will be very large\nand may take a long time to display.\nAre you sure you wish to view it ?\n"));
}


void do_dvgrab_error(void) {
  do_error_dialog(_ ("\n\nYou must install 'dvgrab' to use this function.\n"));
}


void do_nojack_rec_error(void) {
 do_error_dialog(_ ("\n\nAudio recording can only be done using either\nthe \"jack\" or the \"pulse audio\" audio player.\nYou may need to select one of these in Tools/Preferences/Playback.\n"));
}

void do_vpp_palette_error (void) {
  do_error_dialog_with_check_transient(_("Video playback plugin failed to initialise palette !\n"),FALSE,0,GTK_WINDOW(prefsw->prefs_dialog));
}

void do_decoder_palette_error (void) {
  do_blocking_error_dialog(_("Decoder plugin failed to initialise palette !\n"));
}


void do_vpp_fps_error (void) {
  do_error_dialog_with_check_transient(_("Unable to set framerate of video plugin\n"),FALSE,0,GTK_WINDOW(prefsw->prefs_dialog));
}


void do_after_crash_warning (void) {
  do_error_dialog_with_check(_("After a crash, it is advisable to clean up the disk with\nFile|Clean up disk space\n"),WARN_MASK_CLEAN_AFTER_CRASH);
}



static void on_dth_cancel_clicked (GtkButton *button, gpointer user_data) {
  if (GPOINTER_TO_INT(user_data)==1) mainw->cancelled=CANCEL_KEEP;
  else mainw->cancelled=CANCEL_USER;
}


void do_rmem_max_error (gint size) {
  gchar *msg=g_strdup_printf((_("Stream frame size is too large for your network buffers.\nYou should do the following as root:\n\necho %d > /proc/sys/net/core/rmem_max\n")),size);
  do_error_dialog(msg);
  g_free(msg);
}



static void dth2_inner (void *arg, gboolean has_cancel) {
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox2;
  GtkWidget *vbox3;
  process *procw=(process*)(g_malloc(sizeof(process)));
  gchar tmp_label[256];
  gboolean pthread_islocked=TRUE;

  // mutex lock
  do {
    if (!mainw->threaded_dialog||mainw->cancelled==CANCEL_USER) {
      pthread_islocked=FALSE;
      break;
    }
    sched_yield();
    g_usleep(prefs->sleep_time);
  } while (pthread_mutex_trylock(&mainw->gtk_mutex));

  if (!pthread_islocked) {
    g_free(procw);
    return; // parent process finished already
  }

  procw->processing = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (procw->processing), 10);
  gtk_window_add_accel_group (GTK_WINDOW (procw->processing), mainw->accel_group);
  gtk_widget_show(procw->processing);

  gtk_widget_set_size_request (procw->processing, 320, 185);
  gtk_window_set_resizable (GTK_WINDOW (procw->processing), FALSE);
  
  if (mainw->is_ready) gtk_widget_modify_bg(procw->processing, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_window_set_title (GTK_WINDOW (procw->processing), _("LiVES: - Processing..."));

  if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(procw->processing),GTK_WINDOW(mainw->LiVES));
  else gtk_window_set_transient_for(GTK_WINDOW(procw->processing),GTK_WINDOW(mainw->multitrack->window));

  gtk_window_set_position (GTK_WINDOW (procw->processing), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (procw->processing), TRUE);

  dialog_vbox1 = GTK_DIALOG (procw->processing)->vbox;
  gtk_widget_show (dialog_vbox1);

  gtk_dialog_set_has_separator(GTK_DIALOG(procw->processing),FALSE);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox2, TRUE, TRUE, 0);

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox3);
  gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, TRUE, 0);

  g_snprintf(tmp_label,256,"%s...\n",(gchar *)arg);
  procw->label = gtk_label_new (tmp_label);
  gtk_widget_show (procw->label);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->label, FALSE, FALSE, 0);
  if (mainw->is_ready) gtk_widget_modify_fg(procw->label, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_label_set_justify (GTK_LABEL (procw->label), GTK_JUSTIFY_LEFT);

  procw->progressbar = gtk_progress_bar_new ();
  gtk_widget_show (procw->progressbar);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->progressbar, FALSE, FALSE, 0);
  if (mainw->is_ready) gtk_widget_modify_fg(procw->progressbar, GTK_STATE_NORMAL, &palette->normal_fore);

  procw->label2 = gtk_label_new (_("\nPlease Wait"));
  gtk_widget_show (procw->label2);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->label2, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (procw->label2), GTK_JUSTIFY_CENTER);
  if (mainw->is_ready) gtk_widget_modify_fg(procw->label2, GTK_STATE_NORMAL, &palette->normal_fore);

  procw->label3 = gtk_label_new (NULL);
  gtk_widget_show (procw->label3);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->label3, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (procw->label3), GTK_JUSTIFY_CENTER);
  if (mainw->is_ready) gtk_widget_modify_fg(procw->label3, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(procw->progressbar),.01);







  if (has_cancel) {
    GtkWidget *cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
    gtk_widget_show (cancelbutton);

    if (mainw->current_file>-1&&cfile!=NULL&&cfile->opening_only_audio) {
      GtkWidget *enoughbutton = gtk_button_new_with_mnemonic (_ ("_Enough"));
      gtk_widget_show (enoughbutton);
      gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), enoughbutton, GTK_RESPONSE_CANCEL);
      GTK_WIDGET_SET_FLAGS (enoughbutton, GTK_CAN_DEFAULT);
      
      g_signal_connect (GTK_OBJECT (enoughbutton), "clicked",
			G_CALLBACK (on_dth_cancel_clicked),
			GINT_TO_POINTER(1));
      
      mainw->cancel_type=CANCEL_SOFT;
    }

    gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), cancelbutton, GTK_RESPONSE_CANCEL);
    GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
                      G_CALLBACK (on_dth_cancel_clicked),
                      GINT_TO_POINTER(0));

    mainw->cancel_type=CANCEL_SOFT;
  }

  while (!mainw->is_ready&&pthread_islocked) {
    pthread_mutex_unlock(&mainw->gtk_mutex);
    do {
      if (!mainw->threaded_dialog||mainw->cancelled==CANCEL_USER) {
	pthread_islocked=FALSE;
	break;
      }
      sched_yield();
      g_usleep(prefs->sleep_time);
    } while (pthread_mutex_trylock(&mainw->gtk_mutex));
  }

  if (!pthread_islocked) {
    gtk_widget_destroy(procw->processing);
    g_free(procw);
    return;
  }

  while (g_main_context_iteration(NULL,FALSE));
  lives_set_cursor_style(LIVES_CURSOR_BUSY,procw->processing->window);

  while (mainw->threaded_dialog&&mainw->cancelled!=CANCEL_USER&&mainw->cancelled!=CANCEL_KEEP&&pthread_islocked) {
    // mutex locked
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(procw->progressbar));

    while (g_main_context_iteration(NULL,FALSE));

    pthread_mutex_unlock(&mainw->gtk_mutex);
    do {
      if (!mainw->threaded_dialog||mainw->cancelled==CANCEL_USER) {
	pthread_islocked=FALSE;
	break;
      }
      sched_yield();
      g_usleep(prefs->sleep_time);
    } while (pthread_mutex_trylock(&mainw->gtk_mutex));
  }

  gtk_widget_destroy(procw->processing);
  while (g_main_context_iteration(NULL,FALSE));
  if (pthread_islocked) pthread_mutex_unlock(&mainw->gtk_mutex);

  g_free(procw);
}



static void *dth2 (void *arg) {
  dth2_inner(arg,FALSE);
  return NULL;
}

static void *dth2_with_cancel (void *arg) {
  dth2_inner(arg,TRUE);
  if ((mainw->cancelled==CANCEL_USER||mainw->cancelled==CANCEL_KEEP)&&mainw->sig_pid>0) {
    
    while (!g_file_test(mainw->sig_file,G_FILE_TEST_EXISTS)) {
      g_usleep(prefs->sleep_time);
    }

    pthread_kill(mainw->sig_pid,SIGUSR1);
  }
  return NULL;
}




static void *splash_prog (void *arg) {
  while (mainw->threaded_dialog&&mainw->cancelled!=CANCEL_USER) {
    if (!pthread_mutex_trylock(&mainw->gtk_mutex)) {
      gtk_progress_bar_pulse(GTK_PROGRESS_BAR(mainw->splash_progress));
      while (g_main_context_iteration(NULL,FALSE));
      pthread_mutex_unlock(&mainw->gtk_mutex);
    }
    sched_yield();
    g_usleep(prefs->sleep_time);
  }
  return NULL;
}







static pthread_t dthread;
static gchar *thread_text;
 
void do_threaded_dialog(gchar *text, gboolean has_cancel) {
  // calling this causes a threaded progress dialog to appear
  // until end_threaded_dialog() is called
  //
  // WARNING: excercise *extreme caution* after calling this;
  // in the main thread, all drawing operations MUST be surrounded by:
  // 
  // pthread_mutex_lock(&mainw->gtk_mutex);
  // ...
  // pthread_mutex_unlock(&mainw->gtk_mutex);
  // 
  // otherwise the app can crash very badly !


  // in addition all calls to weed_set_* must be wrapped, as these can use the slice allocator
  // also weed_plant_new, weed_plant_free

  thread_text=g_strdup(text);
  mainw->threaded_dialog=TRUE;
  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);
  if (!has_cancel) pthread_create(&dthread,NULL,dth2,thread_text);
  else pthread_create(&dthread,NULL,dth2_with_cancel,thread_text);
}


void end_threaded_dialog(void) {
  if (mainw->threaded_dialog) {
    mainw->threaded_dialog=FALSE;
    while (pthread_mutex_trylock(&mainw->gtk_mutex)) {
      // wait to get lock
      sched_yield();
      g_usleep(prefs->sleep_time);
    }
    pthread_join(dthread,NULL);
    pthread_mutex_unlock(&mainw->gtk_mutex);
    if (thread_text!=NULL) g_free(thread_text);
    if (mainw->splash_window==NULL) lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
    else lives_set_cursor_style(LIVES_CURSOR_NORMAL,mainw->splash_window->window);
  }
  mainw->cancel_type=CANCEL_KILL;
  if (mainw->multitrack==NULL) gtk_widget_queue_draw(mainw->LiVES);
  else gtk_widget_queue_draw(mainw->multitrack->window);
}




void do_splash_progress(void) {
  // calling this causes splash screen progress to pulse
  // until end_threaded_dialog() is called
  //
  // WARNING: excercise *extreme caution* after calling this;
  // in the main thread, all drawing operations MUST be surrounded by:
  // 
  // pthread_mutex_lock(&mainw->gtk_mutex);
  // ...
  // pthread_mutex_unlock(&mainw->gtk_mutex);
  // 
  // otherwise the app can crash very badly !
  thread_text=NULL;

  mainw->threaded_dialog=TRUE;
  lives_set_cursor_style(LIVES_CURSOR_BUSY,mainw->splash_window->window);
  pthread_create(&dthread,NULL,splash_prog,NULL);
}











void 
response_ok (GtkButton *button, gpointer user_data) {
  gtk_dialog_response (GTK_DIALOG (gtk_widget_get_toplevel(GTK_WIDGET(button))), GTK_RESPONSE_OK);
}

void 
response_cancel (GtkButton *button, gpointer user_data) {
  gtk_dialog_response (GTK_DIALOG (gtk_widget_get_toplevel(GTK_WIDGET(button))), GTK_RESPONSE_CANCEL);
}


inline void d_print_cancelled(void) {
  d_print(_("cancelled.\n"));
#ifdef ENABLE_OSC
      lives_osc_notify(LIVES_OSC_NOTIFY_CANCELLED,"");
#endif
}

inline void d_print_failed(void) {
  d_print(_("failed.\n"));
#ifdef ENABLE_OSC
      lives_osc_notify(LIVES_OSC_NOTIFY_FAILED,"");
#endif
}

inline void d_print_done(void) {
  d_print(_("done.\n"));
}

inline void d_print_file_error_failed(void) {
  d_print(_("error in file. Failed.\n"));
}


void do_file_perm_error(gchar *file_name) {
  gchar *msg=g_strdup_printf(_("\nLiVES was unable to write to the file:\n%s\nPlease check the file permissions and try again."),file_name);
  do_error_dialog(msg);
  g_free(msg);
}



void do_encoder_img_ftm_error(render_details *rdet) {
  gchar *msg=g_strdup_printf(_("\nThe %s cannot encode clips with image type %s.\nPlease select another encoder from the list.\n"),prefs->encoder.name,cfile->img_type==IMG_TYPE_JPEG?"jpg":"png");

  do_error_dialog_with_check_transient(msg,TRUE,0,GTK_WINDOW(rdet->dialog));

  g_free(msg);
}
