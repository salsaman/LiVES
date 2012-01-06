// dialogs.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2012
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
#include "resample.h"


extern void reset_frame_and_clip_index (void);


// processing
static gint64 event_start;
static gdouble audio_start;
static gboolean accelerators_swapped;
static gint frames_done;
static gint disp_frames_done;

static gint64 last_open_check_ticks;

static gboolean shown_paused_frames;
static gboolean force_show;

static gdouble est_time;


// how often to we count frames when opening
#define OPEN_CHECK_TICKS (U_SECL/10l)

static volatile gboolean dlg_thread_ready=FALSE;

void on_warn_mask_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
  GtkWidget *tbutton;

  if (gtk_toggle_button_get_active(togglebutton)) prefs->warning_mask|=GPOINTER_TO_INT(user_data);
  else prefs->warning_mask^=GPOINTER_TO_INT(user_data);
  set_int_pref("lives_warning_mask",prefs->warning_mask);

  if ((tbutton=(GtkWidget *)g_object_get_data(G_OBJECT(togglebutton),"auto"))!=NULL) {
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
    gtk_text_buffer_insert_at_cursor(textbuffer,(const gchar *)xlist->data,strlen((char *)xlist->data));
    gtk_text_buffer_insert_at_cursor(textbuffer,"\n",strlen("\n"));
    xlist=xlist->next;
  }

  gtk_box_pack_start (box, expander, FALSE, FALSE, 10);
  gtk_widget_show_all(expander);
}






void add_warn_check (GtkBox *box, gint warn_mask_number) {
  GtkWidget *checkbutton = gtk_check_button_new ();
  GtkWidget *eventbox,*hbox;
  GtkWidget *label=gtk_label_new_with_mnemonic 
    (_("Do _not show this warning any more\n(can be turned back on from Preferences/Warnings)"));

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


static void add_clear_ds_button(GtkDialog* dialog) {
  GtkWidget *button = gtk_button_new_from_stock ("gtk-clear");

  gtk_button_set_label(GTK_BUTTON(button),_("_Recover disk space"));
  if (mainw->tried_ds_recover) gtk_widget_set_sensitive(button,FALSE);

  g_signal_connect (GTK_OBJECT (button), "clicked",
		    G_CALLBACK (on_cleardisk_activate),
		    (gpointer)button);

  gtk_widget_show(button);
  gtk_dialog_add_action_widget (dialog, button, LIVES_RETRY);

}


static void add_clear_ds_adv(GtkBox *box) {
  // add a button which opens up  Recover/Repair widget
  GtkWidget *button = gtk_button_new_with_mnemonic(_(" _Advanced Settings >>"));
  GtkWidget *hbox = gtk_hbox_new (FALSE, 0);

  gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 20);
  gtk_box_pack_start (box, hbox, FALSE, FALSE, 10);

  gtk_widget_show_all(hbox);

  g_signal_connect (GTK_OBJECT (button), "clicked",
		    G_CALLBACK (on_cleardisk_advanced_clicked),
		    NULL);

}





//Warning or yes/no dialog
static GtkWidget* create_warn_dialog (gint warn_mask_number, GtkWindow *transient, const gchar *text, lives_dialog_t diat) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *warning_cancelbutton=NULL;
  GtkWidget *warning_okbutton=NULL;
  GtkWidget *abortbutton=NULL;

  gchar *textx;

  switch (diat) {
  case LIVES_DIALOG_WARN:
    dialog = gtk_message_dialog_new (transient,GTK_DIALOG_MODAL,GTK_MESSAGE_WARNING,GTK_BUTTONS_NONE,"%s","");

    if (mainw->add_clear_ds_button) {
      mainw->add_clear_ds_button=FALSE;
      add_clear_ds_button(GTK_DIALOG(dialog));
    }

    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Warning !"));
    mainw->warning_label = gtk_label_new (_("warning"));
    warning_cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), warning_cancelbutton, GTK_RESPONSE_CANCEL);
    warning_okbutton = gtk_button_new_from_stock ("gtk-ok");
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), warning_okbutton, GTK_RESPONSE_OK);
    break;
  case LIVES_DIALOG_YESNO:
    dialog = gtk_message_dialog_new (transient,GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_NONE,"%s","");
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Question"));
    mainw->warning_label = gtk_label_new (_("question"));
    warning_cancelbutton = gtk_button_new_from_stock ("gtk-no");
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), warning_cancelbutton, LIVES_NO);
    warning_okbutton = gtk_button_new_from_stock ("gtk-yes");
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), warning_okbutton, LIVES_YES);
    break;
  case LIVES_DIALOG_ABORT_CANCEL_RETRY:
    dialog = gtk_message_dialog_new (transient,GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_NONE,"%s","");
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - File Error"));
    mainw->warning_label = gtk_label_new (_("File Error"));
    abortbutton = gtk_button_new_from_stock ("gtk-quit");
    gtk_button_set_label(GTK_BUTTON(abortbutton),_("_Abort"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), abortbutton, LIVES_ABORT);
    gtk_widget_show (abortbutton);
    warning_cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), warning_cancelbutton, LIVES_CANCEL);
    warning_okbutton = gtk_button_new_from_stock ("gtk-refresh");
    gtk_button_set_label(GTK_BUTTON(warning_okbutton),_("_Retry"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), warning_okbutton, LIVES_RETRY);
    break;
  default:
    return NULL;
    break;
  }

  if (palette->style&STYLE_1) {
    //gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_window_set_deletable(GTK_WINDOW(dialog), FALSE);

  textx=insert_newlines(text,MAX_MSG_WIDTH_CHARS);

  gtk_label_set_text(GTK_LABEL(mainw->warning_label),textx);

  g_free(textx);

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_widget_show (dialog_vbox);

  gtk_widget_show (mainw->warning_label);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), mainw->warning_label, TRUE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (mainw->warning_label), GTK_JUSTIFY_CENTER);
  gtk_label_set_line_wrap (GTK_LABEL (mainw->warning_label), FALSE);
  gtk_label_set_selectable (GTK_LABEL (mainw->warning_label), TRUE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mainw->warning_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  if (mainw->add_clear_ds_adv) {
    mainw->add_clear_ds_adv=FALSE;
    add_clear_ds_adv(GTK_BOX(dialog_vbox));
  }

  if (warn_mask_number>0) {
    add_warn_check(GTK_BOX(dialog_vbox),warn_mask_number);
  }

  if (mainw->xlays!=NULL) {
    add_xlays_widget(GTK_BOX(dialog_vbox));
  }

  dialog_action_area = GTK_DIALOG (dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  gtk_widget_show (warning_cancelbutton);
  GTK_WIDGET_SET_FLAGS (warning_cancelbutton, GTK_CAN_DEFAULT);

  gtk_widget_show (warning_okbutton);

  GTK_WIDGET_SET_FLAGS (warning_okbutton, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (warning_okbutton);


  return dialog;
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

  mytext=g_strdup(text); // must copy this because of translation issues

  do {
    warning=create_warn_dialog(warn_mask_number,transient,mytext,LIVES_DIALOG_WARN);
    gtk_widget_show(warning);
    response=gtk_dialog_run (GTK_DIALOG (warning));
    gtk_widget_destroy (warning);
  } while (response==LIVES_RETRY);

  while (g_main_context_iteration(NULL,FALSE));
  if (mytext!=NULL) g_free(mytext);

  return (response==GTK_RESPONSE_OK);
}


gboolean do_yesno_dialog(const gchar *text) {
  // show Yes/No, returns TRUE if Yes
  GtkWidget *warning;
  int response;
  gchar *mytext;
  GtkWindow *transient=NULL;

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL&&mainw->is_ready) transient=GTK_WINDOW(mainw->LiVES);
    else if (mainw->multitrack->is_ready) transient=GTK_WINDOW(mainw->multitrack->window);
  }

  mytext=g_strdup(text); // translation issues
  warning=create_warn_dialog(0,transient,mytext,LIVES_DIALOG_YESNO);
  if (mytext!=NULL) g_free(mytext);

  response=gtk_dialog_run (GTK_DIALOG (warning));
  gtk_widget_destroy (warning);

  while (g_main_context_iteration(NULL,FALSE));
  return (response==LIVES_YES);
}



// returns LIVES_CANCEL or LIVES_RETRY
int do_abort_cancel_retry_dialog(const gchar *text, GtkWindow *transient) {
  int response;
  gchar *mytext;
  GtkWidget *warning;

  if (!prefs->show_gui) {
    transient=NULL;
  } else {
    if (transient==NULL) {
      if (mainw->multitrack==NULL&&mainw->is_ready) transient=GTK_WINDOW(mainw->LiVES);
      else if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) transient=GTK_WINDOW(mainw->multitrack->window);
    }
  }

  mytext=g_strdup(text); // translation issues

  do {
    warning=create_warn_dialog(0,transient,mytext,LIVES_DIALOG_ABORT_CANCEL_RETRY);

    response=gtk_dialog_run (GTK_DIALOG (warning));
    gtk_widget_destroy (warning);

    while (g_main_context_iteration(NULL,FALSE));

    if (response==LIVES_ABORT) {
      if (do_abort_check()) {
	if (mainw->current_file>-1) {
	  if (cfile->handle!=NULL) {
	    // stop any processing processing
	    gchar *com=g_strdup_printf("smogrify stopsubsub \"%s\" 2>/dev/null",cfile->handle);
	    lives_system(com,TRUE);
	    g_free(com);
	  }
	}
	exit(1);
      }
    }

  } while (response==LIVES_ABORT);

  if (mytext!=NULL) g_free(mytext);
  return response;
}





void 
do_error_dialog(const gchar *text) {
  // show error/info box
  if (!prefs->show_gui) {
    do_error_dialog_with_check_transient(text,FALSE,0,NULL);
  } else {
    if (prefsw!=NULL&&prefsw->prefs_dialog!=NULL) do_error_dialog_with_check_transient(text,FALSE,0,
										       GTK_WINDOW(prefsw->prefs_dialog));
    else {
      if (mainw->multitrack==NULL) do_error_dialog_with_check_transient(text,FALSE,0,GTK_WINDOW(mainw->LiVES));
      else do_error_dialog_with_check_transient(text,FALSE,0,GTK_WINDOW(mainw->multitrack->window));
    }
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


gchar *ds_critical_msg(const gchar *dir, guint64 dsval) {
  gchar *msg;
  gchar *msgx;
  gchar *tmp;
  gchar *dscr=lives_format_storage_space_string(prefs->ds_crit_level); ///< crit level
  gchar *dscu=lives_format_storage_space_string(dsval); ///< current level
  msg=g_strdup_printf(_("FREE SPACE IN THE PARTITION CONTAINING\n%s\nHAS FALLEN BELOW THE CRITICAL LEVEL OF %s\nCURRENT FREE SPACE IS %s\n\n(Disk warning levels can be configured in Preferences.)"),
		      (tmp=g_filename_to_utf8(dir,-1,NULL,NULL,NULL)),dscr,dscu);
  msgx=insert_newlines(msg,MAX_MSG_WIDTH_CHARS);
  g_free(msg);
  g_free(tmp);
  g_free(dscr);
  g_free(dscu);
  return msgx;
}


gchar *ds_warning_msg(const gchar *dir, guint64 dsval, guint64 cwarn, guint64 nwarn) {
  gchar *msg;
  gchar *msgx;
  gchar *tmp;
  gchar *dscw=lives_format_storage_space_string(cwarn); ///< warn level
  gchar *dscu=lives_format_storage_space_string(dsval); ///< current level
  gchar *dscn=lives_format_storage_space_string(nwarn); ///< next warn level
  msg=g_strdup_printf(_("Free space in the partition containing\n%s\nhas fallen below the warning level of %s\nCurrent free space is %s\n\n(Next warning will be shown at %s. Disk warning levels can be configured in Preferences.)"),
		      (tmp=g_filename_to_utf8(dir,-1,NULL,NULL,NULL)),dscw,dscu,dscn);
  msgx=insert_newlines(msg,MAX_MSG_WIDTH_CHARS);
  g_free(msg);
  g_free(dscw);
  g_free(dscu);
  g_free(dscn);
  return msgx;
}


void do_aud_during_play_error(void) {
  do_error_dialog_with_check_transient(_("Audio players cannot be switched during playback."),
				       TRUE,0,GTK_WINDOW(prefsw->prefs_dialog));
}  

void 
do_memory_error_dialog (void) {
  do_error_dialog (_ ("\n\nLiVES was unable to perform this operation due to unsufficient memory.\nPlease try closing some other applications first.\n"));
}



void handle_backend_errors(void) {
  // handle error conditions returned from the back end

  int i;
  int pxstart=1;
  gchar **array;
  gchar *addinfo;
  gint numtok;

  if (mainw->cancelled) return; // if the user/system cancelled we can expect errors !

  numtok=get_token_count (mainw->msg,'|');
  
  array=g_strsplit(mainw->msg,"|",numtok);
  
  if (numtok>2 && !strcmp(array[1],"read")) {
    // got read error from backend
    if (numtok>3&&strlen(array[3])) addinfo=array[3];
    else addinfo=NULL;
    if (mainw->current_file==-1 || cfile==NULL || !cfile->no_proc_read_errors) 
      do_read_failed_error_s(array[2],addinfo);
    pxstart=3;
    mainw->read_failed=TRUE;
    mainw->read_failed_file=g_strdup(array[2]);
    mainw->cancelled=CANCEL_ERROR;
  }
  
  else if (numtok>2 && !strcmp(array[1],"write")) {
    // got write error from backend
    if (numtok>3&&strlen(array[3])) addinfo=array[3];
    else addinfo=NULL;
    if (mainw->current_file==-1 || cfile==NULL || !cfile->no_proc_write_errors) 
      do_write_failed_error_s(array[2],addinfo);
    pxstart=3;
    mainw->write_failed=TRUE;
    mainw->write_failed_file=g_strdup(array[2]);
    mainw->cancelled=CANCEL_ERROR;
  }
  
  
  else if (numtok>3 && !strcmp(array[1],"system")) {
    // got (sub) system error from backend
    if (numtok>4&&strlen(array[4])) addinfo=array[4];
    else addinfo=NULL;
    if (mainw->current_file==-1 || cfile==NULL || !cfile->no_proc_sys_errors) 
      do_system_failed_error(array[2],atoi(array[3]),addinfo);
    pxstart=3;
    mainw->cancelled=CANCEL_ERROR;
  }
  
  // for other types of errors...more info....
  // set mainw->error but not mainw->cancelled
  g_snprintf(mainw->msg,512,"\n\n");
  for (i=pxstart;i<numtok;i++) {
    g_strappend(mainw->msg,512,_(array[i]));
    g_strappend(mainw->msg,512,"\n");
  }
  g_strfreev(array);

  mainw->error=TRUE;

}



gboolean check_backend_return(file *sfile) {
  // check return code after synchronous (foreground) backend commands

  FILE *infofile;

  (infofile=fopen(sfile->info_file,"r"));
  if (!infofile) return FALSE;

  mainw->read_failed=FALSE;
  lives_fgets(mainw->msg,512,infofile);
  fclose(infofile);

  if (!strncmp(mainw->msg,"error",5)) handle_backend_errors();

  return TRUE;
}

void pump_io_chan(GIOChannel *iochan) {
  // pump data from stdout to textbuffer
  gchar *str_return=NULL;
  gsize retlen;
  GError *gerr=NULL;

  GtkTextIter iter;
  GtkTextMark *mark;
  GtkTextBuffer *optextbuf=gtk_text_view_get_buffer(mainw->optextview);

  if (!iochan->is_readable) return;

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


gboolean check_storage_space(file *sfile, gboolean is_processing) {
  // check storage space in prefs->tmpdir, and if sfile!=NULL, in sfile->op_dir
  guint64 dsval;
  gchar *msg,*tmp;
  int retval;
  gboolean did_pause=FALSE;
  lives_storage_status_t ds;
  gchar *pausstr=g_strdup(_("Processing has been paused."));

  do {
    ds=get_storage_status(prefs->tmpdir,mainw->next_ds_warn_level,&dsval);
    if (ds==LIVES_STORAGE_STATUS_WARNING) {
      guint64 curr_ds_warn=mainw->next_ds_warn_level;
      mainw->next_ds_warn_level>>=1;
      if (mainw->next_ds_warn_level>(dsval>>1)) mainw->next_ds_warn_level=dsval>>1;
      if (mainw->next_ds_warn_level<prefs->ds_crit_level) mainw->next_ds_warn_level=prefs->ds_crit_level;
      if (is_processing&&sfile!=NULL&&sfile->proc_ptr!=NULL&&!mainw->effects_paused&&
	  GTK_WIDGET_VISIBLE(sfile->proc_ptr->pause_button)) {
	on_effects_paused(GTK_BUTTON(sfile->proc_ptr->pause_button),NULL);
	did_pause=TRUE;
      }

      tmp=ds_warning_msg(prefs->tmpdir,dsval,curr_ds_warn,mainw->next_ds_warn_level);
      if (!did_pause)
	msg=g_strdup_printf("\n%s\n",tmp);
      else 
	msg=g_strdup_printf("\n%s\n%s\n",tmp,pausstr);
      g_free(tmp);
      mainw->add_clear_ds_button=TRUE; // gets reset by do_warning_dialog()
      if (!do_warning_dialog(msg)) {
	g_free(msg);
	g_free(pausstr);
	mainw->cancelled=CANCEL_USER;
	if (is_processing) {
	  sfile->nokeep=TRUE;
	  on_cancel_keep_button_clicked(NULL,NULL); // press the cancel button
	}
	return FALSE;
      }
      g_free(msg);
    }
    else if (ds==LIVES_STORAGE_STATUS_CRITICAL) {
      if (is_processing&&sfile!=NULL&&sfile->proc_ptr!=NULL&&!mainw->effects_paused&&
	  GTK_WIDGET_VISIBLE(sfile->proc_ptr->pause_button)) {
	on_effects_paused(GTK_BUTTON(sfile->proc_ptr->pause_button),NULL);
	did_pause=TRUE;
      }
      tmp=ds_critical_msg(prefs->tmpdir,dsval);
      if (!did_pause)
	msg=g_strdup_printf("\n%s\n",tmp);
      else 
	msg=g_strdup_printf("\n%s\n%s\n",tmp,pausstr);
      g_free(tmp);
        retval=do_abort_cancel_retry_dialog(msg,NULL);
      g_free(msg);
      if (retval==LIVES_CANCEL) {
	if (is_processing) {
	  sfile->nokeep=TRUE;
	  on_cancel_keep_button_clicked(NULL,NULL); // press the cancel button
	}
	mainw->cancelled=CANCEL_ERROR;
	g_free(pausstr);
	return FALSE;
      }
    }
  } while (ds==LIVES_STORAGE_STATUS_CRITICAL);


  if (sfile!=NULL&&sfile->op_dir!=NULL) {
    do {
      ds=get_storage_status(sfile->op_dir,sfile->op_ds_warn_level,&dsval);
      if (ds==LIVES_STORAGE_STATUS_WARNING) {
	guint64 curr_ds_warn=sfile->op_ds_warn_level;
	sfile->op_ds_warn_level>>=1;
	if (sfile->op_ds_warn_level>(dsval>>1)) sfile->op_ds_warn_level=dsval>>1;
	if (sfile->op_ds_warn_level<prefs->ds_crit_level) sfile->op_ds_warn_level=prefs->ds_crit_level;
	if (is_processing&&sfile!=NULL&&sfile->proc_ptr!=NULL&&!mainw->effects_paused&&
	    GTK_WIDGET_VISIBLE(sfile->proc_ptr->pause_button)) {
	  on_effects_paused(GTK_BUTTON(sfile->proc_ptr->pause_button),NULL);
	  did_pause=TRUE;
	}
	tmp=ds_warning_msg(sfile->op_dir,dsval,curr_ds_warn,sfile->op_ds_warn_level);
	if (!did_pause)
	  msg=g_strdup_printf("\n%s\n",tmp);
	else 
	  msg=g_strdup_printf("\n%s\n%s\n",tmp,pausstr);
	g_free(tmp);
	mainw->add_clear_ds_button=TRUE; // gets reset by do_warning_dialog()
	if (!do_warning_dialog(msg)) {
	  g_free(msg);
	  g_free(pausstr);
	  lives_freep((void**)&sfile->op_dir);
	  if (is_processing) {
	    sfile->nokeep=TRUE;
	    on_cancel_keep_button_clicked(NULL,NULL); // press the cancel button
	  }
	  mainw->cancelled=CANCEL_USER;
	  return FALSE;
	}
	g_free(msg);
      }
      else if (ds==LIVES_STORAGE_STATUS_CRITICAL) {
	if (is_processing&&sfile!=NULL&&sfile->proc_ptr!=NULL&&!mainw->effects_paused&&
	    GTK_WIDGET_VISIBLE(sfile->proc_ptr->pause_button)) {
	  on_effects_paused(GTK_BUTTON(sfile->proc_ptr->pause_button),NULL);
	  did_pause=TRUE;
	}
	tmp=ds_critical_msg(sfile->op_dir,dsval);
	if (!did_pause)
	  msg=g_strdup_printf("\n%s\n",tmp);
	else 
	  msg=g_strdup_printf("\n%s\n%s\n",tmp,pausstr);
	g_free(tmp);
	retval=do_abort_cancel_retry_dialog(msg,NULL);
	g_free(msg);
	if (retval==LIVES_CANCEL) {
	  if (is_processing) {
	    sfile->nokeep=TRUE;
	    on_cancel_keep_button_clicked(NULL,NULL); // press the cancel button
	  }
	  mainw->cancelled=CANCEL_ERROR;
	  if (sfile!=NULL) lives_freep((void**)&cfile->op_dir);
	  g_free(pausstr);
	  return FALSE;
	}
      }
    } while (ds==LIVES_STORAGE_STATUS_CRITICAL);
  }

  if (did_pause&&mainw->effects_paused) {
    on_effects_paused(GTK_BUTTON(sfile->proc_ptr->pause_button),NULL);
  }

  g_free(pausstr);

  return TRUE;
}




void cancel_process(gboolean visible) {
  if (prefs->show_player_stats&&!visible&&mainw->fps_measure>0.) {
    // statistics
    gettimeofday(&tv, NULL);
    mainw->fps_measure/=(gdouble)(U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*
				  U_SEC_RATIO-mainw->offsetticks)/U_SEC;
  }
  if (visible) {
    if (mainw->preview_box!=NULL&&!mainw->preview) gtk_widget_set_tooltip_text( mainw->p_playbutton,_ ("Play all"));
    if (accelerators_swapped) {
      if (!mainw->preview) gtk_widget_set_tooltip_text( mainw->m_playbutton,_ ("Play all"));
      gtk_widget_remove_accelerator (cfile->proc_ptr->preview_button, mainw->accel_group, GDK_p, (GdkModifierType)0);
      gtk_widget_add_accelerator (mainw->playall, "activate", mainw->accel_group, GDK_p, (GdkModifierType)0, 
				  GTK_ACCEL_VISIBLE);
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
  else {
    mainw->is_processing=TRUE;
  }
  if (mainw->current_file>-1&&cfile->clip_type==CLIP_TYPE_DISK&&((mainw->cancelled!=CANCEL_NO_MORE_PREVIEW&&
								  mainw->cancelled!=CANCEL_USER)||!cfile->opening)) {
    unlink(cfile->info_file);
  }
}




static void disp_fraction(gint done, gint start, gint end, gdouble timesofar, process *proc) {
  // display fraction done and estimated time remaining
  gchar *prog_label;
  gdouble est_time;
  gdouble fraction_done=(done-start)/(end-start+1.);

  if (fraction_done>1.) fraction_done=1.;
  if (fraction_done<0.) fraction_done=0.;

  if (done>disp_frames_done) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(proc->progressbar),fraction_done);

  est_time=timesofar/fraction_done-timesofar;
  prog_label=g_strdup_printf(_("\n%d%% done. Time remaining: %u sec\n"),(gint)(fraction_done*100.),(guint)(est_time+.5));
  gtk_label_set_text(GTK_LABEL(proc->label3),prog_label);
  g_free(prog_label);

  disp_frames_done=done;

}



static int progress_count;

#define PROG_LOOP_VAL 50

static void progbar_pulse_or_fraction(file *sfile, int frames_done) {
  gdouble timesofar;

  if (progress_count++>=PROG_LOOP_VAL) {
    if (frames_done<=sfile->progress_end&&sfile->progress_end>0&&!mainw->effects_paused&&
	frames_done>0) {
      
      gettimeofday(&tv, NULL);
      mainw->currticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
      timesofar=(mainw->currticks-mainw->timeout_ticks)/U_SEC;
      
      disp_fraction(frames_done,sfile->progress_start,sfile->progress_end,
		    timesofar,sfile->proc_ptr);
      progress_count=0;
    }
    else {
      gtk_progress_bar_pulse(GTK_PROGRESS_BAR(sfile->proc_ptr->progressbar));
      progress_count=0;
    }
  }
}





gboolean process_one (gboolean visible) {
  gint64 new_ticks;

  lives_time_source_t time_source;
  gboolean show_frame;

#ifdef RT_AUDIO
  gdouble audio_stretch;
  gint64 audio_ticks=0;
#endif

  if (!visible) {
    // INTERNAL PLAYER
    if (G_UNLIKELY(mainw->new_clip!=-1)) {
      do_quick_switch(mainw->new_clip);
      mainw->new_clip=-1;
    }

    // get current time

    // time is obtained as follows:
    // -  if there is an external transport or clock active, we take our time from that
    // -  else if we have a fixed output framerate (e.g. we are streaming) we take our time from
    //         the system clock
    //  in these cases we adjust our audio rate slightly to keep in synch with video
    // - otherwise, we take the time from soundcard by counting samples played (the normal case),   
    //   and we synch video with that


    time_source=LIVES_TIME_SOURCE_NONE;

#ifdef ENABLE_JACK_TRANSPORT
    if (mainw->jack_can_stop&&(prefs->jack_opts&JACK_OPTS_TIMEBASE_CLIENT)&&(prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)&&!(mainw->record&&!(prefs->rec_opts&REC_FRAMES))) {
      // calculate the time from jack transport
      mainw->currticks=jack_transport_get_time()*U_SEC;
      time_source=LIVES_TIME_SOURCE_EXTERNAL;
    }
#endif

    
    if (!mainw->ext_playback||(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)) {
    // get time from soundcard


#ifdef ENABLE_JACK
      if (time_source==LIVES_TIME_SOURCE_NONE&&!mainw->foreign&&prefs->audio_player==AUD_PLAYER_JACK&&cfile->achans>0&&(!mainw->is_rendering||(mainw->multitrack!=NULL&&!cfile->opening&&!mainw->multitrack->is_rendering))&&mainw->jackd!=NULL&&mainw->jackd->in_use) {
	if (!(mainw->fixed_fpsd>0.||(mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.&&mainw->ext_playback))) {
	  if (mainw->aud_rec_fd!=-1) mainw->currticks=lives_jack_get_time(mainw->jackd_read,TRUE);
	  else mainw->currticks=lives_jack_get_time(mainw->jackd,TRUE);
	  time_source=LIVES_TIME_SOURCE_SOUNDCARD;
	}
      }
#endif

#ifdef HAVE_PULSE_AUDIO
      if (time_source==LIVES_TIME_SOURCE_NONE&&!mainw->foreign&&prefs->audio_player==AUD_PLAYER_PULSE&&
	  cfile->achans>0&&(!mainw->is_rendering||(mainw->multitrack!=NULL&&
						   !cfile->opening&&!mainw->multitrack->is_rendering))&&
	  ((mainw->pulsed!=NULL&&mainw->pulsed->in_use)||mainw->pulsed_read!=NULL)) {
	if (!(mainw->fixed_fpsd>0.||(mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.&&mainw->ext_playback))) {
	  if (mainw->aud_rec_fd!=-1) mainw->currticks=lives_pulse_get_time(mainw->pulsed_read,TRUE);
	  else mainw->currticks=lives_pulse_get_time(mainw->pulsed,TRUE);
	  time_source=LIVES_TIME_SOURCE_SOUNDCARD;
	}
      }
#endif
    }
    if (time_source==LIVES_TIME_SOURCE_NONE) {
      // get time from system clock
      gettimeofday(&tv, NULL);
      mainw->currticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
      time_source=LIVES_TIME_SOURCE_SYSTEM;
    }

    // adjust audio rate slightly if we are behind or ahead
    if (time_source!=LIVES_TIME_SOURCE_SOUNDCARD) {
#ifdef ENABLE_JACK
      if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&
	  cfile->achans>0&&(!mainw->is_rendering||(mainw->multitrack!=NULL&&!mainw->multitrack->is_rendering))&&
	  (mainw->currticks-mainw->offsetticks)>U_SECL*10&&(audio_ticks=lives_jack_get_time(mainw->jackd,TRUE))>
	  mainw->offsetticks) {
	if ((audio_stretch=(gdouble)(audio_ticks-mainw->offsetticks)/(gdouble)(mainw->currticks-mainw->offsetticks))<2.) {
	  // if audio_stretch is > 1. it means that audio is playing too fast
	  // < 1. it is playing too slow

	  // if too fast we increase the apparent sample rate so that it gets downsampled more
	  // if too slow we decrease the apparent sample rate so that it gets upsampled more

	  if (mainw->multitrack==NULL) {
	    if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
	      if (!cfile->play_paused) mainw->jackd->sample_in_rate=cfile->arate*cfile->pb_fps/cfile->fps*audio_stretch;
	      else mainw->jackd->sample_in_rate=cfile->arate*cfile->freeze_fps/cfile->fps*audio_stretch;
	    }
	    else mainw->jackd->sample_in_rate=cfile->arate*audio_stretch;
	  }
	  else {
	    mainw->jackd->abufs[mainw->jackd->read_abuf]->arate=cfile->arate*audio_stretch;
	  }
	}
      }
#endif


#ifdef HAVE_PULSE_AUDIO
      if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&
	  cfile->achans>0&&(!mainw->is_rendering||(mainw->multitrack!=NULL&&!mainw->multitrack->is_rendering))&&
	  (mainw->currticks-mainw->offsetticks)>U_SECL*10&&(audio_ticks=lives_pulse_get_time(mainw->pulsed,TRUE))>
	  mainw->offsetticks) {
	// fps is synched to external source, so we adjust the audio rate to fit
	if ((audio_stretch=(gdouble)(audio_ticks-mainw->offsetticks)/(gdouble)(mainw->currticks-mainw->offsetticks))<2.) {
	  // if audio_stretch is > 1. it means that audio is playing too fast
	  // < 1. it is playing too slow

	  // if too fast we increase the apparent sample rate so that it gets downsampled more
	  // if too slow we decrease the apparent sample rate so that it gets upsampled more

	  if (mainw->multitrack==NULL) {
	    if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
	      if (!cfile->play_paused) mainw->pulsed->in_arate=cfile->arate*cfile->pb_fps/cfile->fps*audio_stretch;
	      else mainw->pulsed->in_arate=cfile->arate*cfile->freeze_fps/cfile->fps*audio_stretch;
	    } 
	    else mainw->pulsed->in_arate=cfile->arate*audio_stretch;
	  }
	  else {
	    mainw->pulsed->abufs[mainw->pulsed->read_abuf]->arate=cfile->arate*audio_stretch;
	  }
	}
      }
#endif
    }

    if (G_UNLIKELY(cfile->proc_ptr==NULL&&cfile->next_event!=NULL)) {
      // playing an event_list


      if (mainw->scratch!=SCRATCH_NONE&&mainw->multitrack!=NULL) {
#ifdef ENABLE_JACK_TRANSPORT
	// handle transport jump
	weed_timecode_t transtc=q_gint64(jack_transport_get_time()*U_SEC,cfile->fps);
	mainw->multitrack->pb_start_event=get_frame_event_at(mainw->multitrack->event_list,transtc,NULL,TRUE);
	if (mainw->cancelled==CANCEL_NONE) mainw->cancelled=CANCEL_EVENT_LIST_END;
#endif
      }
      else if (mainw->currticks>=event_start) {
	// see if we are playing a selection and reached the end
	if (mainw->multitrack!=NULL&&mainw->multitrack->playing_sel&&get_event_timecode(cfile->next_event)/U_SEC>=
	    mainw->multitrack->region_end) mainw->cancelled=CANCEL_EVENT_LIST_END;
	else {
	  cfile->next_event=process_events (cfile->next_event,mainw->currticks);
	
	  // see if we need to fill an audio buffer
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
	}
      }
      while (g_main_context_iteration(NULL,FALSE)); // allow kb timer to run
      if (cfile->next_event==NULL&&mainw->preview) mainw->cancelled=CANCEL_EVENT_LIST_END;
      if (mainw->cancelled==CANCEL_NONE) return TRUE;
      cancel_process(visible);
      return FALSE;
    }

    // free playback
    new_ticks=mainw->currticks+mainw->deltaticks;
    cfile->last_frameno=cfile->frameno;

    show_frame=FALSE;

    cfile->frameno=calc_new_playback_position(mainw->current_file,mainw->startticks,&new_ticks);
    
    if (new_ticks!=mainw->startticks) {
      show_frame=TRUE;
      mainw->startticks=new_ticks;
    }

    // play next frame
    if (G_LIKELY(mainw->cancelled==CANCEL_NONE)) {

      // calculate the audio 'frame' for no-realtime audio players
      // for realtime players, we did this in calc_new_playback_position()
      if (prefs->audio_player==AUD_PLAYER_SOX||prefs->audio_player==AUD_PLAYER_MPLAYER) {
	mainw->aframeno=(gint64)(mainw->currticks-mainw->firstticks)*cfile->fps/U_SEC+audio_start;
	if (G_UNLIKELY(mainw->loop_cont&&(mainw->aframeno>(mainw->audio_end?mainw->audio_end:
							   cfile->laudio_time*cfile->fps)))) {
	  mainw->firstticks=mainw->startticks-mainw->deltaticks;
	}
      }


      if ((mainw->fixed_fpsd<=0.&&show_frame&&(mainw->vpp==NULL||
					       mainw->vpp->fixed_fpsd<=0.||!mainw->ext_playback))||
	  (mainw->fixed_fpsd>0.&&(mainw->currticks-mainw->last_display_ticks)/U_SEC>=1./mainw->fixed_fpsd)||
	  (mainw->vpp!=NULL&&mainw->vpp->fixed_fpsd>0.&&mainw->ext_playback&&
	   (mainw->currticks-mainw->last_display_ticks)/U_SEC>=1./mainw->vpp->fixed_fpsd)||force_show) {
	// time to show a new frame

#ifdef ENABLE_JACK
	// note the audio seek position
	if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
	  cfile->aseek_pos=mainw->jackd->seek_pos;
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	// note the audio seek position
	if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
	  cfile->aseek_pos=mainw->pulsed->seek_pos;
	}
#endif

	// load and display the new frame
	load_frame_image(cfile->frameno);
	if (1||mainw->last_display_ticks==0) mainw->last_display_ticks=mainw->currticks;
	else {
	  if (mainw->vpp!=NULL&&mainw->ext_playback&&mainw->vpp->fixed_fpsd>0.) 
	    mainw->last_display_ticks+=U_SEC/mainw->vpp->fixed_fpsd;
	  else if (mainw->fixed_fpsd>0.) 
	    mainw->last_display_ticks+=U_SEC/mainw->fixed_fpsd;
	  else mainw->last_display_ticks=mainw->currticks;
	}
	force_show=FALSE;
      }


#ifdef ENABLE_JACK
      // request for another audio buffer - used only during mt render preview
      if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&mainw->abufs_to_fill>0) {
	fill_abuffer_from(mainw->jackd->abufs[mainw->write_abuf],mainw->event_list,NULL,FALSE);
      }
#endif
#ifdef HAVE_PULSE_AUDIO
      // request for another audio buffer - used only during mt render preview
      if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&mainw->abufs_to_fill>0) {
	fill_abuffer_from(mainw->pulsed->abufs[mainw->write_abuf],mainw->event_list,NULL,FALSE);
      }
#endif

    }
    // paused
    if (G_UNLIKELY(cfile->play_paused)) {
      mainw->startticks=mainw->currticks+mainw->deltaticks;
      if (mainw->ext_playback&&mainw->vpp->send_keycodes!=NULL) (*mainw->vpp->send_keycodes)(pl_key_function);
    }
  }

  if (visible) {

    // fixes a problem with opening preview with bg generator
    if (cfile->proc_ptr==NULL) {
      if (mainw->cancelled==CANCEL_NONE) mainw->cancelled=CANCEL_NO_PROPOGATE;
    }
    else {
      gdouble fraction_done,timesofar;
      gchar *prog_label;

      if (GTK_IS_SPIN_BUTTON(mainw->framedraw_spinbutton)) 
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->framedraw_spinbutton),1,cfile->proc_ptr->frames_done);
      // set the progress bar %

      if (cfile->opening&&cfile->clip_type==CLIP_TYPE_DISK&&!cfile->opening_only_audio&&
	  (cfile->hsize>0||cfile->vsize>0||cfile->frames>0)&&(!mainw->effects_paused||!shown_paused_frames)) {
	guint apxl;
	gettimeofday(&tv, NULL);
	mainw->currticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
	if ((mainw->currticks-last_open_check_ticks)>OPEN_CHECK_TICKS*
	    ((apxl=get_approx_ln((guint)mainw->opening_frames))<200?apxl:200)||
	    (mainw->effects_paused&&!shown_paused_frames)) {
	  count_opening_frames();
	  last_open_check_ticks=mainw->currticks;
	  if (mainw->opening_frames>1) {
	    if (cfile->frames>0&&cfile->frames!=123456789) {
	      fraction_done=(gdouble)(mainw->opening_frames-1)/(gdouble)cfile->frames;
	      if (fraction_done>1.) fraction_done=1.;
	      if (!mainw->effects_paused) {
		timesofar=(mainw->currticks-mainw->timeout_ticks)/U_SEC;
		est_time=timesofar/fraction_done-timesofar;
	      }
	      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(cfile->proc_ptr->progressbar),fraction_done);
	      if (est_time!=-1.) prog_label=g_strdup_printf(_("\n%d/%d frames opened. Time remaining %u sec.\n"),
							    mainw->opening_frames-1,cfile->frames,(guint)(est_time+.5));
	      else prog_label=g_strdup_printf(_("\n%d/%d frames opened.\n"),mainw->opening_frames-1,cfile->frames);
	    }
	    else {
	      gtk_progress_bar_pulse(GTK_PROGRESS_BAR(cfile->proc_ptr->progressbar));
	      prog_label=g_strdup_printf(_("\n%d frames opened.\n"),mainw->opening_frames-1);
	    }
	    gtk_label_set_text(GTK_LABEL(cfile->proc_ptr->label3),prog_label);
	    g_free(prog_label);
	  }
	}
	shown_paused_frames=mainw->effects_paused;
      }

      else {
	if (visible&&cfile->proc_ptr->frames_done>=cfile->progress_start) {
	  if (progress_count==0) check_storage_space(cfile, TRUE);
	  // display progress fraction or pulse bar
	  progbar_pulse_or_fraction(cfile,cfile->proc_ptr->frames_done);
	}
      }
    }

    frames_done=cfile->proc_ptr->frames_done;

    if (cfile->clip_type==CLIP_TYPE_FILE&&cfile->fx_frame_pump>0&&
	(cfile->progress_start+frames_done+FX_FRAME_PUMP_VAL>cfile->fx_frame_pump)) {
      gint vend=cfile->fx_frame_pump;
      gboolean retb=virtual_to_images(mainw->current_file,vend,vend,FALSE);
      if (retb) cfile->fx_frame_pump=vend+1;
      else mainw->cancelled=CANCEL_ERROR;
      if (vend==cfile->end) cfile->fx_frame_pump=0; // all frames were realised
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

  // this is the outer loop for playback and all kinds of processing

  // visible is set for processing (progress dialog is visible)
  // or unset for video playback (progress dialog is not shown)


  FILE *infofile=NULL;
  gchar *mytext=NULL;

  int frames_done;
  
  // translation issues
  if (visible&&text!=NULL) mytext=g_strdup(text);

  if (visible) {
    // check we have sufficient storage space
    if (!check_storage_space((mainw->current_file>-1)?cfile:NULL,FALSE)) {
      lives_cancel_t cancelled=mainw->cancelled;
      on_cancel_keep_button_clicked(NULL,NULL);
      if (mainw->cancelled!=CANCEL_NONE) mainw->cancelled=cancelled;
      d_print_cancelled();
      return FALSE;
    }
  }

  event_start=0;
  audio_start=mainw->play_start;
  if (visible) accelerators_swapped=FALSE;
  frames_done=disp_frames_done=0;
  mainw->last_display_ticks=0;
  shown_paused_frames=FALSE;
  est_time=-1.;
  force_show=TRUE;

  mainw->cevent_tc=0;

  progress_count=0;

  mainw->render_error=LIVES_RENDER_ERROR_NONE;

  if (!visible) {
    if (mainw->event_list!=NULL) {
      // get audio start time
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
  if (!mainw->preview||cfile->opening) mainw->timeout_ticks=0;

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
    //else lives_set_cursor_style(LIVES_CURSOR_BUSY,GDK_WINDOW(cfile->proc_ptr->processing->window));





    // if we have virtual frames make sure the first FX_FRAME_PUMP_VAL are decoded for the backend
    // as we are processing we will continue to decode 1 frame in time with the backend
    // in this way we hope to stay ahead of the backend

    // the backend can either restrict itself to processing in the range x -> x + (FX_FRAME_PUMP_VAL) frames
    // -> if it needs frames further in the range (like "jumble") it can check carefully and wait

    // cfile->fx_frame_pump_val is currently only set for realtime effects and tools
    // it is also used for resampling

    // default FX_FRAME_PUMP_VAL is 200

    // (encoding and copying have their own mechanism which realises all frames in the selection first) 

    if (cfile->clip_type==CLIP_TYPE_FILE&&cfile->fx_frame_pump>0) {
      gint vend=cfile->fx_frame_pump+FX_FRAME_PUMP_VAL;
      if (vend>cfile->progress_end) vend=cfile->progress_end;
      if (vend>=cfile->fx_frame_pump) {
	register int i;
	for (i=cfile->fx_frame_pump;i<=vend;i++) {
	  gboolean retb=virtual_to_images(mainw->current_file,i,i,FALSE);
	  if (mainw->cancelled||!retb) {
	    if (mainw->current_file>-1&&cfile!=NULL) lives_freep((void**)&cfile->op_dir);
	    return FALSE;
	  }
	  while (g_main_context_iteration(NULL,FALSE));
	}
	cfile->fx_frame_pump+=FX_FRAME_PUMP_VAL>>1;
      }
    }


    if (cfile->opening&&(capable->has_sox||(prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL)||
			 (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL))&&mainw->playing_file==-1) {
      if (mainw->preview_box!=NULL) gtk_widget_set_tooltip_text( mainw->p_playbutton,_ ("Preview"));
      gtk_widget_set_tooltip_text( mainw->m_playbutton,_ ("Preview"));
      gtk_widget_remove_accelerator (mainw->playall, mainw->accel_group, GDK_p, (GdkModifierType)0);
      gtk_widget_add_accelerator (cfile->proc_ptr->preview_button, "clicked", mainw->accel_group, GDK_p,
				  (GdkModifierType)0, (GtkAccelFlags)0);
      accelerators_swapped=TRUE;
    }

  }

  if (cfile->next_event!=NULL) event_start=get_event_timecode(cfile->next_event);


  // mainw->origsecs, mainw->origusecs is our base for quantising 
  // (and is constant for each playback session)

  // firstticks is to do with the audio "frame" for sox, mplayer
  // startticks is the ticks value of the last frame played

  mainw->startticks=mainw->currticks=mainw->offsetticks=0;
  last_open_check_ticks=0;

  gettimeofday(&tv, NULL);

  // [IMPORTANT] we subtract these from every calculation to make the numbers smaller
  mainw->origsecs=tv.tv_sec;
  mainw->origusecs=tv.tv_usec;

  if (!visible) {
    // video playback

#ifdef ENABLE_JACK_TRANSPORT
    if (mainw->jack_can_stop&&mainw->multitrack==NULL&&(prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)&&
	!(mainw->record&&!(prefs->rec_opts&REC_FRAMES)&&cfile->next_event==NULL)) {
      // calculate the start position from jack transport

      gint64 ntc=jack_transport_get_time()*U_SEC;
      gboolean noframedrop=mainw->noframedrop;
      mainw->noframedrop=FALSE;
      cfile->last_frameno=1;
      if (prefs->jack_opts&JACK_OPTS_TIMEBASE_START) {
	mainw->play_start=calc_new_playback_position(mainw->current_file,0,&ntc);
      }
      mainw->noframedrop=noframedrop;
      if (prefs->jack_opts&JACK_OPTS_TIMEBASE_CLIENT) {
	// timebase client - follows jack transport position
	mainw->startticks=ntc;
      }
      mainw->currticks=ntc;
    }
#endif
    cfile->last_frameno=cfile->frameno=mainw->play_start;

    // deltaticks is used for scratching (forwards and back)
    mainw->deltaticks=0;
  }


  if (mainw->multitrack!=NULL&&!mainw->multitrack->is_rendering) {
    // playback start from middle of multitrack
    // calculate when we "would have started" at time 0

    // WARNING: origticks could be negative

    gint64 origticks=mainw->origsecs*U_SEC+mainw->origusecs*U_SEC_RATIO-
      (mainw->offsetticks=get_event_timecode(mainw->multitrack->pb_start_event));
    mainw->origsecs=origticks/U_SEC;
    mainw->origusecs=((gint64)(origticks/U_SEC_RATIO)-mainw->origsecs*1000000.);
  }

  if (cfile->achans) cfile->aseek_pos=(long)((gdouble)(mainw->play_start-1.)/
					     cfile->fps*cfile->arate*cfile->achans*(cfile->asampsize/8));



  // MUST do re-seek after setting origsecs in order to set our clock properly
  // re-seek to new playback start
#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK&&cfile->achans>0&&cfile->laudio_time>0.&&
      !mainw->is_rendering&&!(cfile->opening&&!mainw->preview)&&mainw->jackd!=NULL&&mainw->jackd->playing_file>-1) {

    if (!jack_audio_seek_frame(mainw->jackd,mainw->play_start)) {
      if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd,mainw->play_start);
    }

    mainw->rec_aclip=mainw->current_file;
    mainw->rec_avel=cfile->pb_fps/cfile->fps;
    mainw->rec_aseek=(gdouble)cfile->aseek_pos/(gdouble)(cfile->arate*cfile->achans*(cfile->asampsize/8));
  }
  if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&mainw->multitrack!=NULL&&
      !mainw->multitrack->is_rendering&&cfile->achans>0) {
    // have to set this here as we don't do a seek in multitrack
    mainw->jackd->audio_ticks=mainw->offsetticks;
    mainw->jackd->frames_written=0;
  }
#endif
#ifdef HAVE_PULSE_AUDIO

  if (mainw->pulsed_read!=NULL) pulse_driver_uncork(mainw->pulsed_read);

  if (prefs->audio_player==AUD_PLAYER_PULSE&&cfile->achans>0&&cfile->laudio_time>0.&&
      !mainw->is_rendering&&!(cfile->opening&&!mainw->preview)&&mainw->pulsed!=NULL&&mainw->pulsed->playing_file>-1) {

    if (!pulse_audio_seek_frame(mainw->pulsed,mainw->play_start)) {
      if (pulse_try_reconnect()) pulse_audio_seek_frame(mainw->pulsed,mainw->play_start);
    }

    mainw->rec_aclip=mainw->current_file;
    mainw->rec_avel=cfile->pb_fps/cfile->fps;
    mainw->rec_aseek=(gdouble)cfile->aseek_pos/(gdouble)(cfile->arate*cfile->achans*(cfile->asampsize/8));
  }
  if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&mainw->multitrack!=NULL&&
      !mainw->multitrack->is_rendering&&cfile->achans>0) {
    mainw->pulsed->audio_ticks=mainw->offsetticks;
    mainw->pulsed->frames_written=0;
  }
#endif

  if (mainw->iochan!=NULL) gtk_widget_show (cfile->proc_ptr->pause_button);

  
  // tell jack transport we are ready to play
  mainw->video_seek_ready=TRUE;

  mainw->scratch=SCRATCH_NONE;


  //try to open info file - or if internal_messaging is TRUE, we get mainw->msg
  // from the mainw->progress_fn function

  while (1) {
    while (!mainw->internal_messaging&&((!visible&&(mainw->whentostop!=STOP_ON_AUD_END||
						    prefs->audio_player==AUD_PLAYER_JACK||
						    prefs->audio_player==AUD_PLAYER_PULSE))||
					!g_file_test(cfile->info_file,G_FILE_TEST_EXISTS))) {


      // just pulse the progress bar, or play video
      // returns FALSE if playback ended
      if (!process_one(visible)) {
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	if (mainw->current_file>-1&&cfile!=NULL) lives_freep((void**)&cfile->op_dir);
	return FALSE;
      }

      if (mainw->iochan!=NULL&&progress_count==0) {
	// pump data from stdout to textbuffer
	// this is for encoder output
	pump_io_chan(mainw->iochan);
      }

      if (visible&&!mainw->internal_messaging) g_usleep(prefs->sleep_time);

    }

    if (!mainw->internal_messaging) {
      // background processing (e.g. rendered effects)
      if ((infofile=fopen(cfile->info_file,"r"))) {
	// OK, now we might have some frames
	mainw->read_failed=FALSE;
	lives_fgets(mainw->msg,512,infofile);
	fclose(infofile);
      }
    }
    // else call realtime effect pass
    else {
      mainw->render_error=(*mainw->progress_fn)(FALSE);

      if (mainw->render_error>=LIVES_RENDER_ERROR) {
	lives_freep((void**)&cfile->op_dir);
	if (mainw->current_file>-1&&cfile!=NULL) lives_freep((void**)&cfile->op_dir);
	return FALSE;
      }

      // display progress fraction or pulse bar
      if (mainw->msg!=NULL&&strlen(mainw->msg)>0&&(frames_done=atoi(mainw->msg))>0)
	cfile->proc_ptr->frames_done=atoi(mainw->msg);
      else
	cfile->proc_ptr->frames_done=0;
      if (progress_count==0) check_storage_space(cfile, TRUE);
      progbar_pulse_or_fraction(cfile,cfile->proc_ptr->frames_done);
    }


#ifdef DEBUG
    if (strlen(mainw->msg)) g_print("msg %s\n",mainw->msg);
#endif

    // we got a message from the backend...

    if (visible&&(!accelerators_swapped||cfile->opening)&&cancellable&&(!cfile->nopreview||cfile->keep_without_preview)) {
      if ((!cfile->opening||((capable->has_sox||(prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL)||
			     (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL))&&
			     mainw->playing_file==-1))&&!cfile->nopreview) 
	gtk_widget_show (cfile->proc_ptr->preview_button);
      if (cfile->opening_loc) gtk_widget_show (cfile->proc_ptr->stop_button);
      else gtk_widget_show (cfile->proc_ptr->pause_button);

      if (!cfile->opening&&!cfile->nopreview) {
	gtk_widget_grab_default (cfile->proc_ptr->preview_button);
	if (mainw->preview_box!=NULL) gtk_widget_set_tooltip_text( mainw->p_playbutton,_ ("Preview"));
	gtk_widget_set_tooltip_text( mainw->m_playbutton,_ ("Preview"));
	gtk_widget_remove_accelerator (mainw->playall, mainw->accel_group, GDK_p, (GdkModifierType)0);
	gtk_widget_add_accelerator (cfile->proc_ptr->preview_button, "clicked", mainw->accel_group, GDK_p,
				    (GdkModifierType)0, (GtkAccelFlags)0);
	accelerators_swapped=TRUE;
      }
    }

    if (strncmp(mainw->msg,"completed",8)&&strncmp(mainw->msg,"error",5)&&
	strncmp(mainw->msg,"killed",6)&&(visible||
					 ((strncmp(mainw->msg,"video_ended",11)||mainw->whentostop!=STOP_ON_VID_END)
					  &&(strncmp(mainw->msg,"audio_ended",11)||mainw->preview||
					     mainw->whentostop!=STOP_ON_AUD_END)))) {
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
	if (mainw->current_file>-1&&cfile!=NULL) lives_freep((void**)&cfile->op_dir);
	return FALSE;
      }

      if (mainw->iochan!=NULL&&progress_count==0) {
	// pump data from stdout to textbuffer
	pump_io_chan(mainw->iochan);
      }

      g_usleep(prefs->sleep_time);
    }
    else break;
  }


#ifdef DEBUG
  g_print ("exit pt 3 %s\n",mainw->msg);
#endif

  //play/operation ended
  if (visible) {
    if (cfile->clip_type==CLIP_TYPE_DISK&&(mainw->cancelled!=CANCEL_NO_MORE_PREVIEW||!cfile->opening)) {
      unlink(cfile->info_file);
    }
    if (mainw->preview_box!=NULL&&!mainw->preview) gtk_widget_set_tooltip_text( mainw->p_playbutton,
									 _("Play all"));
    if (accelerators_swapped) {
      if (!mainw->preview) gtk_widget_set_tooltip_text( mainw->m_playbutton,_ ("Play all"));
      gtk_widget_remove_accelerator (cfile->proc_ptr->preview_button, mainw->accel_group, GDK_p, (GdkModifierType)0);
      gtk_widget_add_accelerator (mainw->playall, "activate", mainw->accel_group, GDK_p, (GdkModifierType)0,
				  GTK_ACCEL_VISIBLE);
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
	mainw->fps_measure/=(gdouble)(U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-
				      mainw->origusecs*U_SEC_RATIO-mainw->offsetticks)/U_SEC;
      }
    }
    mainw->is_processing=TRUE;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
  if (mainw->current_file>-1&&cfile!=NULL) lives_freep((void**)&cfile->op_dir);

  // get error message (if any)
  if (!strncmp(mainw->msg,"error",5)) {
    handle_backend_errors();
    if (mainw->cancelled) return FALSE;
  }
  else {
    if (!check_storage_space((mainw->current_file>-1)?cfile:NULL,FALSE)) return FALSE;
  }

#ifdef DEBUG
  g_print("exiting progress dialogue\n");
#endif
  return TRUE;
}



gboolean do_auto_dialog (const gchar *text, gint type) {
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

  mainw->error=FALSE;

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
    gtk_widget_show(proc_ptr->cancel_button);
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed_read!=NULL) pulse_driver_uncork(mainw->pulsed_read);
#endif
    if (mainw->rec_samples!=0) {
      while (g_main_context_iteration(NULL,FALSE));
      g_usleep(prefs->sleep_time);
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
    mainw->read_failed=FALSE;
    lives_fgets(mainw->msg,512,infofile);
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

  // get error message (if any)
  if (type!=1 && !strncmp(mainw->msg,"error",5)) {
    handle_backend_errors();
    if (mainw->cancelled) return FALSE;
  }
  else {
    if (mainw->current_file>-1&&cfile!=NULL)
      if (!check_storage_space((mainw->current_file>-1)?cfile:NULL,FALSE)) return FALSE;
  }
  return TRUE;
}




void
too_many_files(void) {
  gchar *warn=g_strdup_printf(_ ("\nSorry, LiVES can only open %d files at once.\nPlease close a file and then try again."),MAX_FILES);
  do_error_dialog(warn);
  g_free(warn);
}

void
tempdir_warning (void) {
  gchar *tmp,*com=g_strdup_printf(_ ("LiVES was unable to write to its temporary directory.\n\nThe current temporary directory is:\n\n%s\n\nPlease make sure you can write to this directory."),
				  (tmp=g_filename_to_utf8(prefs->tmpdir,-1,NULL,NULL,NULL)));
  g_free(tmp);
  if (mainw!=NULL&&mainw->is_ready) {
    do_error_dialog(com);
  }
  g_free(com);
}


void do_no_mplayer_sox_error(void) {
  do_blocking_error_dialog(_ ("\nLiVES currently requires either 'mplayer' or 'sox' to function. Please install one or other of these, and try again.\n"));
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




gboolean rdet_suggest_values (gint width, gint height, gdouble fps, gint fps_num, gint fps_denom, gint arate, gint asigned, gboolean swap_endian, gboolean anr, gboolean ignore_fps) {
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

  mainw->fx1_bool=FALSE;

  if (swap_endian||(asigned==1&&rdet->aendian==AFORM_UNSIGNED)||(asigned==2&&rdet->aendian==AFORM_SIGNED)||
      (fps>0.&&fps!=rdet->fps)||(fps_denom>0&&(fps_num*1.)/(fps_denom*1.)!=rdet->fps)||
      (!anr&&(rdet->width!=width||rdet->height!=height)&&height*width>0)||
      (arate!=rdet->arate&&arate>0)) {
    g_free (msg2);
    msg2=g_strdup (_ ("LiVES recommends the following settings:\n\n"));
    if (swap_endian||(asigned==1&&rdet->aendian==AFORM_UNSIGNED)||(asigned==2&&rdet->aendian==AFORM_SIGNED)
	||(arate>0&&arate!=rdet->arate)) {
      gchar *sstring;
      gchar *estring;

      if (asigned==1&&rdet->aendian==AFORM_UNSIGNED) sstring=g_strdup(_(", signed"));
      else if (asigned==2&&rdet->aendian==AFORM_SIGNED) sstring=g_strdup(_(", unsigned"));
      else sstring=g_strdup("");

      if (swap_endian) {
	if (mainw->endian!=AFORM_BIG_ENDIAN) estring=g_strdup(_(", little-endian"));
	else estring=g_strdup(_(", big-endian"));
      }
      else estring=g_strdup("");

      ochange=TRUE;
      g_free (msg3);
      msg3=g_strdup_printf (_ ("Use an audio rate of %d Hz%s%s\n"),arate,sstring,estring);
      g_free(sstring);
      g_free(estring);
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
    else anr=FALSE;
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
do_encoder_restrict_dialog (gint width, gint height, gdouble fps, gint fps_num, gint fps_denom, gint arate, gint asigned, gboolean swap_endian, gboolean anr, gboolean save_all) {
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


  if (swap_endian||asigned!=0||(arate>0&&arate!=carate)||(fps>0.&&fps!=cfps)||
      (fps_denom>0&&(fps_num*1.)/(fps_denom*1.)!=cfps)||(!anr&&
							 (chsize!=width||cvsize!=height)&&height*width>0)) {
    g_free (msg2);
    msg2=g_strdup (_ ("LiVES must:\n"));
    if (swap_endian||asigned!=0||(arate>0&&arate!=carate)) {
      gchar *sstring;
      gchar *estring;
      if (asigned==1) sstring=g_strdup(_(", signed"));
      else if (asigned==2) sstring=g_strdup(_(", unsigned"));
      else sstring=g_strdup("");

      if (swap_endian) {
	if (cfile->signed_endian&AFORM_BIG_ENDIAN) estring=g_strdup(_(", little-endian"));
	else estring=g_strdup(_(", big-endian"));
      }
      else estring=g_strdup("");

      g_free (msg3);
      msg3=g_strdup_printf (_ ("resample audio to %d Hz%s%s\n"),arate,sstring,estring);
      g_free(sstring);
      g_free(estring);

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
  if (save_all) {
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



gboolean do_comments_dialog (file *sfile, gchar *filename) {
  gboolean response;
  gboolean ok=FALSE;
  gboolean encoding=FALSE;

  commentsw=create_comments_dialog(sfile,filename);

  if (sfile==NULL) sfile=cfile;
  else encoding=TRUE;

  while (!ok) {
    ok=TRUE;
    if ((response=(gtk_dialog_run(GTK_DIALOG (commentsw->comments_dialog))==GTK_RESPONSE_OK))) {
      g_snprintf (sfile->title,256,"%s",gtk_entry_get_text (GTK_ENTRY (commentsw->title_entry)));
      g_snprintf (sfile->author,256,"%s",gtk_entry_get_text (GTK_ENTRY (commentsw->author_entry)));
      g_snprintf (sfile->comment,256,"%s",gtk_entry_get_text (GTK_ENTRY (commentsw->comment_entry)));
      
      if (encoding&&sfile->subt!=NULL&&gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(commentsw->subt_checkbutton))) {
	gchar *ext=get_extension(gtk_entry_get_text(GTK_ENTRY(commentsw->subt_entry)));
	if (strcmp(ext,"sub")&&strcmp(ext,"srt")) {
	  if (!do_sub_type_warning(ext,sfile->subt->type==SUBTITLE_TYPE_SRT?"srt":"sub")) {
	    gtk_entry_set_text(GTK_ENTRY(commentsw->subt_entry),mainw->subt_save_file);
	    ok=FALSE;
	    continue;
	  }
	}
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=g_strdup(gtk_entry_get_text(GTK_ENTRY(commentsw->subt_entry)));
      }
      else {
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
      }
    }
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
  do_text_window (tmp,_ ("You can use the following keys to control the multitrack window:-\n\nctrl-left-arrow              move timeline cursor left 1 second\nctrl-right-arrow            move timeline cursor right 1 second\nshift-left-arrow            move timeline cursor left 1 frame\nshift-right-arrow          move timeline cursor right 1 frame\nctrl-up-arrow               move current track up\nctrl-down-arrow           move current track down\nctrl-page-up                select previous clip\nctrl-page-down            select next clip\nctrl-space                    select/deselect current track\nctrl-plus                       zoom in\nctrl-minus                    zoom out\nm                                 make a mark on the timeline (during playback)\nw                                 rewind to play start.\n\nFor other keys, see the menus.\n"));
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
  return (do_yesno_dialog(_("\nDo you wish to remove the layout files associated with this set ?\n(They will not be usable without the set).\n")));
}


gboolean do_set_duplicate_warning (const gchar *new_set) {
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

gboolean do_original_lost_warning(const gchar *fname) {
  gchar *msg=g_strdup_printf(_("\nThe original file\n%s\ncould not be found.\nIf this file has been moved, click 'OK' to browse to the new location.\nOtherwise click Cancel to skip loading this file.\n"),fname);
  gboolean retcode=do_warning_dialog(msg);
  g_free(msg);
  return retcode;
}

void do_no_decoder_error(const gchar *fname) {
  gchar *msg=g_strdup_printf(_("\n\nLiVES could not find a required decoder plugin for the clip\n%s\nThe clip could not be loaded.\n"),fname);
    do_error_dialog(msg);
    g_free(msg);
}


static void do_extra_jack_warning(void) {
  do_blocking_error_dialog(_("\nDear user, the jack developers decided to remove the -Z option from jackd.\nPlease check your ~/.jackdrc file and remove this option if present.\nAlternately, select a different audio player in Preferences.\n"));
}

void do_jack_noopen_warn(void) {
  do_blocking_error_dialog(_("\nUnable to start up jack. Please ensure that alsa is set up correctly on your machine\nand also that the soundcard is not in use by another program\nAutomatic jack startup will be disabled now.\n"));
  if (prefs->startup_phase!=2) do_extra_jack_warning();
}


void do_jack_noopen_warn3(void) {
  do_blocking_error_dialog(_("\nUnable to connect to jack server. Please start jack before starting LiVES\n"));
}

void do_jack_noopen_warn4(void) {
#ifdef HAVE_PULSE_AUDIO
  const gchar *otherbit="\"lives -aplayer pulse\"";
#else
  const gchar *otherbit="\"lives -aplayer sox\"";
#endif
  gchar *msg=g_strdup_printf(_("\nAlternatively, try to start lives with either:\n\n\"lives -jackopts 16\", or\n\n%s\n"),otherbit);
  do_blocking_error_dialog(msg);
  g_free(msg);
}


void do_jack_noopen_warn2(void) {
  do_blocking_error_dialog(_("\nAlternately, you can restart LiVES and select another audio player.\n"));
}


void do_mt_backup_space_error(lives_mt *mt, gint memreq_mb) {
  gchar *msg=g_strdup_printf(_("\n\nLiVES needs more backup space for this layout.\nYou can increase the value in Preferences/Multitrack.\nIt is recommended to increase it to at least %d MB"),memreq_mb);
    do_error_dialog_with_check_transient(msg,TRUE,WARN_MASK_MT_BACKUP_SPACE,GTK_WINDOW(mt->window));
    g_free(msg);
}

gboolean do_set_rename_old_layouts_warning(const gchar *new_set) {
  gchar *msg=g_strdup_printf(_("\nSome old layouts for the set %s already exist.\nIt is recommended that you delete them.\nDo you wish to delete them ?\n"),new_set);
  gboolean retcode=do_yesno_dialog(msg);
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
  return do_yesno_dialog(_("Errors were detected in the layout (which may be due to transferring from another system, or from an older version of LiVES).\nShould I try to repair the disk copy of the layout ?\n"));
}

void do_bad_layout_error(void) {
  do_error_dialog(_("LiVES was unable to load the layout.\nSorry.\n"));
}  



void do_audrate_error_dialog(void) {
  do_error_dialog(_ ("\n\nAudio rate must be greater than 0.\n"));
}

gboolean do_event_list_warning(void) {
  return do_yesno_dialog(_("\nEvent list will be very large\nand may take a long time to display.\nAre you sure you wish to view it ?\n"));
}


void do_dvgrab_error(void) {
  do_error_dialog(_ ("\n\nYou must install 'dvgrab' to use this function.\n"));
}


void do_nojack_rec_error(void) {
 do_error_dialog(_ ("\n\nAudio recording can only be done using either\nthe \"jack\" or the \"pulse audio\" audio player.\nYou may need to select one of these in Tools/Preferences/Playback.\n"));
}

void do_vpp_palette_error (void) {
  do_error_dialog_with_check_transient(_("Video playback plugin failed to initialise palette !\n"),TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
}

void do_decoder_palette_error (void) {
  do_blocking_error_dialog(_("Decoder plugin failed to initialise palette !\n"));
}


void do_vpp_fps_error (void) {
  do_error_dialog_with_check_transient(_("Unable to set framerate of video plugin\n"),TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
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

static process *procw=NULL;


static void create_threaded_dialog(gchar *text, gboolean has_cancel) {

  GtkWidget *dialog_vbox1;
  GtkWidget *vbox2;
  GtkWidget *vbox3;
  gchar tmp_label[256];
 
  procw=(process*)(g_malloc(sizeof(process)));

  procw->processing = gtk_dialog_new ();
  gtk_window_set_position (GTK_WINDOW (procw->processing), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_container_set_border_width (GTK_CONTAINER (procw->processing), 10);
  gtk_window_add_accel_group (GTK_WINDOW (procw->processing), mainw->accel_group);
  
  gtk_widget_show(procw->processing);

  gtk_widget_modify_bg(procw->processing, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_window_set_title (GTK_WINDOW (procw->processing), _("LiVES: - Processing..."));

  if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(procw->processing),GTK_WINDOW(mainw->LiVES));
  else gtk_window_set_transient_for(GTK_WINDOW(procw->processing),GTK_WINDOW(mainw->multitrack->window));

  gtk_window_set_modal (GTK_WINDOW (procw->processing), TRUE);

  dialog_vbox1 = lives_dialog_get_content_area(GTK_DIALOG(procw->processing));

  gtk_widget_show (dialog_vbox1);

  gtk_dialog_set_has_separator(GTK_DIALOG(procw->processing),FALSE);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox2, TRUE, TRUE, 0);

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox3);
  gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, TRUE, 0);

  g_snprintf(tmp_label,256,"%s...\n",text);
  procw->label = gtk_label_new (tmp_label);
  gtk_widget_show (procw->label);
  //gtk_widget_set_size_request (procw->label, PROG_LABEL_WIDTH, -1);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->label, FALSE, FALSE, 0);
  gtk_widget_modify_fg(procw->label, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_label_set_justify (GTK_LABEL (procw->label), GTK_JUSTIFY_LEFT);

  procw->progressbar = gtk_progress_bar_new ();
  gtk_widget_show (procw->progressbar);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->progressbar, FALSE, FALSE, 0);
  gtk_widget_modify_fg(procw->progressbar, GTK_STATE_NORMAL, &palette->normal_fore);

  procw->label2 = gtk_label_new (_("\nPlease Wait"));
  gtk_widget_show (procw->label2);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->label2, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (procw->label2), GTK_JUSTIFY_CENTER);
  gtk_widget_modify_fg(procw->label2, GTK_STATE_NORMAL, &palette->normal_fore);

  procw->label3 = gtk_label_new (PROCW_STRETCHER);
  gtk_widget_show (procw->label3);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->label3, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (procw->label3), GTK_JUSTIFY_CENTER);
  gtk_widget_modify_fg(procw->label3, GTK_STATE_NORMAL, &palette->normal_fore);
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


  gtk_window_set_resizable (GTK_WINDOW (procw->processing), FALSE);

  gtk_widget_queue_draw(procw->processing);

  lives_set_cursor_style(LIVES_CURSOR_BUSY,procw->processing->window);

}


static gdouble sttime;


void threaded_dialog_spin (void) {
  gdouble timesofar;
  gint progress;

  if (mainw->splash_window!=NULL) {
    do_splash_progress();
    return;
  }

  if (procw==NULL) return;

  if (mainw->current_file<0||cfile==NULL||cfile->progress_start==0||cfile->progress_end==0||
      strlen(mainw->msg)==0||(progress=atoi(mainw->msg))==0) {
    // pulse the progress bar
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(procw->progressbar));
  }
  else {
    // show fraction
    gettimeofday(&tv, NULL);
    timesofar=(gdouble)(tv.tv_sec*1000000+tv.tv_usec-sttime)*U_SEC_RATIO/U_SEC;
    disp_fraction(progress,cfile->progress_start,cfile->progress_end,timesofar,procw);
  }

  gtk_widget_queue_draw(procw->processing);
  while (g_main_context_iteration(NULL,FALSE));
}



void *splash_prog (void) {
  gtk_progress_bar_pulse(GTK_PROGRESS_BAR(mainw->splash_progress));
  return NULL;
}


 
void do_threaded_dialog(gchar *trans_text, gboolean has_cancel) {
  // calling this causes a threaded progress dialog to appear
  // until end_threaded_dialog() is called
  //
  // WARNING: if trans_text is a translated string, it will be autoamtically freed by translations inside this function

  gchar *copy_text=g_strdup(trans_text);

  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);

  gettimeofday(&tv, NULL);
  sttime=tv.tv_sec*1000000+tv.tv_usec;

  mainw->threaded_dialog=TRUE;
  clear_mainw_msg();

  create_threaded_dialog(copy_text, has_cancel);
  g_free(copy_text);

  while (g_main_context_iteration(NULL,FALSE));
}



void end_threaded_dialog(void) {
  if (procw!=NULL) {
    if (procw->processing!=NULL) gtk_widget_destroy(procw->processing);
  }
  if (mainw->splash_window==NULL) {
    lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
    if (mainw->multitrack==NULL) { 
      if (mainw->is_ready) gtk_widget_queue_draw(mainw->LiVES);
    }
    else gtk_widget_queue_draw(mainw->multitrack->window);
  }
  else lives_set_cursor_style(LIVES_CURSOR_NORMAL,mainw->splash_window->window);
  if (procw!=NULL) {
    g_free(procw);
    procw=NULL;
  }
  mainw->cancel_type=CANCEL_KILL;
  mainw->threaded_dialog=FALSE;
  while (g_main_context_iteration(NULL,FALSE));
}




void do_splash_progress(void) {
  mainw->threaded_dialog=TRUE;
  lives_set_cursor_style(LIVES_CURSOR_BUSY,mainw->splash_window->window);
  splash_prog();
}











void 
response_ok (GtkButton *button, gpointer user_data) {
  gtk_dialog_response (GTK_DIALOG (gtk_widget_get_toplevel(GTK_WIDGET(button))), GTK_RESPONSE_OK);
}

void 
response_cancel (GtkButton *button, gpointer user_data) {
  gtk_dialog_response (GTK_DIALOG (gtk_widget_get_toplevel(GTK_WIDGET(button))), GTK_RESPONSE_CANCEL);
}


LIVES_INLINE void d_print_cancelled(void) {
  d_print(_("cancelled.\n"));
#ifdef ENABLE_OSC
      lives_osc_notify(LIVES_OSC_NOTIFY_CANCELLED,"");
#endif
}

LIVES_INLINE void d_print_failed(void) {
  d_print(_("failed.\n"));
#ifdef ENABLE_OSC
      lives_osc_notify(LIVES_OSC_NOTIFY_FAILED,"");
#endif
}

LIVES_INLINE void d_print_done(void) {
  d_print(_("done.\n"));
}

LIVES_INLINE void d_print_file_error_failed(void) {
  d_print(_("error in file. Failed.\n"));
}


void do_system_failed_error(const char *com, int retval, const char *addinfo) {
  gchar *msg,*tmp,*emsg,*msgx;
  gchar *bit;
  gchar *retstr=g_strdup_printf("%d",retval>>8);
  gchar *bit2=(retval>255)?g_strdup(""):g_strdup_printf("[%s]",strerror(retval));
  gchar *addbit;
  gchar *dsmsg1=g_strdup("");
  gchar *dsmsg2=g_strdup("");

  guint64 dsval1,dsval2;

  lives_storage_status_t ds1=get_storage_status(prefs->tmpdir,prefs->ds_crit_level,&dsval1),ds2;

  if (cfile->op_dir!=NULL) {
    ds2=get_storage_status(cfile->op_dir,prefs->ds_crit_level,&dsval2);
    if (ds2==LIVES_STORAGE_STATUS_CRITICAL) {
      g_free(dsmsg2);
      tmp=ds_critical_msg(cfile->op_dir,dsval2);
      dsmsg2=g_strdup_printf("%s\n",tmp);
      g_free(tmp);
    }
  }

  if (ds1==LIVES_STORAGE_STATUS_CRITICAL) {
    g_free(dsmsg1);
    tmp=ds_critical_msg(prefs->tmpdir,dsval1);
    dsmsg1=g_strdup_printf("%s\n",tmp);
    g_free(tmp);
  }

  if (addinfo!=NULL) addbit=g_strdup_printf(_("Additional info: %s\n"),addinfo);
  else addbit=g_strdup("");

  if (retval>0) bit=g_strdup_printf(_("The error value was %d%s\n"),retval,bit2);
    else bit=g_strdup("");

  msg=g_strdup_printf(_("\nLiVES failed doing the following:\n%s\nPlease check your system for errors.\n%s%s%s"),
		      com,bit,addbit,dsmsg1,dsmsg2);

  emsg=g_strdup_printf("Command failed doing\n%s\n%s%s",com,bit,addbit);
  LIVES_ERROR(emsg);
  g_free(emsg);

  msgx=insert_newlines(msg,MAX_MSG_WIDTH_CHARS);
  do_error_dialog(msgx);
  g_free(msgx);
  g_free(msg);
  g_free(dsmsg1);
  g_free(dsmsg2);
  g_free(bit);
  g_free(bit2);
  g_free(addbit);
  g_free(retstr);
}


void do_write_failed_error_s(const char *s, const char *addinfo) {
  gchar *msg,*emsg;
  gchar *addbit,*tmp;
  gchar *dsmsg=g_strdup("");

  gchar dirname[PATH_MAX];
  gchar *sutf=g_filename_to_utf8(s,-1,NULL,NULL,NULL);

  guint64 dsval;

  lives_storage_status_t ds;

  g_snprintf(dirname,PATH_MAX,"%s",s);
  get_dirname(dirname);
  ds=get_storage_status(dirname,prefs->ds_crit_level,&dsval);

  if (ds==LIVES_STORAGE_STATUS_CRITICAL) {
    g_free(dsmsg);
    tmp=ds_critical_msg(dirname,dsval);
    dsmsg=g_strdup_printf("%s\n",tmp);
    g_free(tmp);
  }

  if (addinfo!=NULL) addbit=g_strdup_printf(_("Additional info: %s\n"),addinfo);
  else addbit=g_strdup("");

  msg=g_strdup_printf(_("\nLiVES was unable to write to the file\n%s\nPlease check for possible error causes.\n%s"),
		      sutf,addbit,dsmsg);
  emsg=g_strdup_printf("Unable to write to file\n%s\n%s",s,addbit);

  g_free(sutf);

  LIVES_ERROR(emsg);
  g_free(emsg);

  do_blocking_error_dialog(msg);
  g_free(addbit);
  g_free(dsmsg);
  g_free(msg);
}


void do_read_failed_error_s(const char *s, const char *addinfo) {
  gchar *msg,*emsg;
  gchar *addbit;
  gchar *sutf=g_filename_to_utf8(s,-1,NULL,NULL,NULL);

  if (addinfo!=NULL) addbit=g_strdup_printf(_("Additional info: %s\n"),addinfo);
  else addbit=g_strdup("");

  msg=g_strdup_printf(_("\nLiVES was unable to read from the file\n%s\nPlease check for possible error causes.\n%s"),
		      sutf,addbit);
  emsg=g_strdup_printf("Unable to read from the file\n%s\n%s",s,addbit);

  LIVES_ERROR(emsg);
  g_free(emsg);
  g_free(sutf);

  do_blocking_error_dialog(msg);
  g_free(msg);
  g_free(addbit);
}



int do_write_failed_error_s_with_retry(const gchar *fname, const gchar *errtext, GtkWindow *transient) {
  // err can be errno from open/fopen etc.

  // return same as do_abort_cancel_retry_dialog() - LIVES_CANCEL or LIVES_RETRY (both non-zero)

  int ret;
  gchar *msg,*emsg,*tmp;
  gchar *sutf=g_filename_to_utf8(fname,-1,NULL,NULL,NULL);
  gchar *dsmsg=g_strdup("");

  gchar dirname[PATH_MAX];

  guint64 dsval;

  lives_storage_status_t ds;

  g_snprintf(dirname,PATH_MAX,"%s",fname);
  get_dirname(dirname);
  ds=get_storage_status(dirname,prefs->ds_crit_level,&dsval);

  if (ds==LIVES_STORAGE_STATUS_CRITICAL) {
    g_free(dsmsg);
    tmp=ds_critical_msg(dirname,dsval);
    dsmsg=g_strdup_printf("%s\n",tmp);
    g_free(tmp);
  }

  if (errtext==NULL) {
    emsg=g_strdup_printf("Unable to write to file %s",fname);
    msg=g_strdup_printf(_("\nLiVES was unable to write to the file\n%s\nPlease check for possible error causes.\n"),sutf);
  }
  else {
    emsg=g_strdup_printf("Unable to write to file %s, error was %s",fname,errtext);
    msg=g_strdup_printf(_("\nLiVES was unable to write to the file\n%s\nThe error was\n%s.\n"),sutf,errtext);
  }
  
  LIVES_ERROR(emsg);
  g_free(emsg);

  ret=do_abort_cancel_retry_dialog(msg,transient);

  g_free(dsmsg);
  g_free(msg);
  g_free(sutf);

  mainw->write_failed=FALSE; // reset this

  return ret;

}



int do_read_failed_error_s_with_retry(const gchar *fname, const gchar *errtext, GtkWindow *transient) {
  // err can be errno from open/fopen etc.

  // return same as do_abort_cancel_retry_dialog() - LIVES_CANCEL or LIVES_RETRY (both non-zero)

  int ret;
  gchar *msg,*emsg;
  gchar *sutf=g_filename_to_utf8(fname,-1,NULL,NULL,NULL);

  if (errtext==NULL) {
    emsg=g_strdup_printf("Unable to read from file %s",fname);
    msg=g_strdup_printf(_("\nLiVES was unable to read from the file\n%s\nPlease check for possible error causes.\n"),sutf);
  }
  else {
    emsg=g_strdup_printf("Unable to read from file %s, error was %s",fname,errtext);
    msg=g_strdup_printf(_("\nLiVES was unable to read from the file\n%s\nThe error was\n%s.\n"),sutf,errtext);
  }
  
  LIVES_ERROR(emsg);
  g_free(emsg);

  ret=do_abort_cancel_retry_dialog(msg,transient);

  g_free(msg);
  g_free(sutf);

  mainw->read_failed=FALSE; // reset this

  return ret;

}


int do_header_read_error_with_retry(int clip) {
  int ret;
  gchar *hname;
  if (mainw->files[clip]==NULL) return 0;

  hname=g_build_filename(prefs->tmpdir,mainw->files[clip]->handle,"header.lives",NULL);

  ret=do_read_failed_error_s_with_retry(hname,NULL,NULL);

  g_free(hname);
  return ret;
}



gboolean do_header_write_error(int clip) {
  // returns TRUE if we manage to clear the error

  gchar *hname;
  int retval;

  if (mainw->files[clip]==NULL) return TRUE;

  hname=g_build_filename(prefs->tmpdir,mainw->files[clip]->handle,"header.lives",NULL);
  retval=do_write_failed_error_s_with_retry(hname,NULL,NULL);
  if (retval==LIVES_RETRY && save_clip_values(clip)) retval=0; // on retry try to save all values
  g_free(hname);

  return (!retval);
}



int do_header_missing_detail_error(int clip, lives_clip_details_t detail) {
  int ret;
  gchar *hname,*key,*msg;
  if (mainw->files[clip]==NULL) return 0;

  hname=g_build_filename(prefs->tmpdir,mainw->files[clip]->handle,"header.lives",NULL);

  key=clip_detail_to_string(detail,NULL);

  if (key==NULL) {
    msg=g_strdup_printf("Invalid detail %d requested from file %s",detail,hname);
    LIVES_ERROR(msg);
    g_free(msg);
    g_free(hname);
    return 0;
  }

  msg=g_strdup_printf(_("Value for \"%s\" could not be read."),key);
  ret=do_read_failed_error_s_with_retry(hname,msg,NULL);

  g_free(msg);
  g_free(key);
  g_free(hname);
  return ret;
}



void do_chdir_failed_error(const char *dir) {
  gchar *msg;
  gchar *dutf;
  gchar *emsg=g_strdup_printf("Failed directory change to\n%s",dir);
  LIVES_ERROR(emsg);
  g_free(emsg);
  dutf=g_filename_to_utf8(dir,-1,NULL,NULL,NULL);
  msg=g_strdup_printf(_("\nLiVES failed to change directory to\n%s\nPlease check your system for errors.\n"),dutf);
  do_error_dialog(msg);
  g_free(msg);
  g_free(dutf);
}



void do_file_perm_error(const gchar *file_name) {
  gchar *msg=g_strdup_printf(_("\nLiVES was unable to write to the file:\n%s\nPlease check the file permissions and try again."),file_name);
  do_blocking_error_dialog(msg);
  g_free(msg);
}


void do_dir_perm_error(const gchar *dir_name) {
  gchar *msg=g_strdup_printf(_("\nLiVES was unable to either create or write to the directory:\n%s\nPlease check the directory permissions and try again."),dir_name);
  do_blocking_error_dialog(msg);
  g_free(msg);
}


gboolean do_abort_check(void) {
  return do_yesno_dialog(_("\nAbort and exit immediately from LiVES\nAre you sure ?\n"));
}



void do_encoder_img_ftm_error(render_details *rdet) {
  gchar *msg=g_strdup_printf(_("\nThe %s cannot encode clips with image type %s.\nPlease select another encoder from the list.\n"),prefs->encoder.name,cfile->img_type==IMG_TYPE_JPEG?"jpg":"png");

  do_error_dialog_with_check_transient(msg,TRUE,0,GTK_WINDOW(rdet->dialog));

  g_free(msg);
}


void do_card_in_use_error(void) {
  do_blocking_error_dialog(_("\nThis card is already in use and cannot be opened multiple times.\n"));
}


void do_dev_busy_error(const gchar *devstr) {
  gchar *msg=g_strdup_printf(_("\nThe device %s is in use or unavailable.\n- Check the device permissions\n- Check if this device is in use by another program.\n- Check if the device actually exists.\n"),devstr);
  do_blocking_error_dialog(msg);
  g_free(msg);
}


gboolean do_existing_subs_warning(void) {
  return do_warning_dialog(_("\nThis file already has subtitles loaded.\nDo you wish to overwrite the existing subtitles ?\n"));
}

void do_invalid_subs_error(void) {
  do_error_dialog(_("\nLiVES currently only supports subtitles of type .srt and .sub.\n"));
}

gboolean do_erase_subs_warning(void) {
  return do_warning_dialog(_("\nErase all subtitles from this clip.\nAre you sure ?\n"));
}


gboolean do_sub_type_warning(const gchar *ext, const gchar *type_ext) {
  gboolean ret;
  gchar *msg=g_strdup_printf(_("\nLiVES does not recognise the subtitle file type \"%s\".\nClick Cancel to set another file name\nor OK to continue and save as type \"%s\"\n"),ext,type_ext);
  ret=do_warning_dialog(msg);
  g_free(msg);
  return ret;
}

gboolean do_move_tmpdir_dialog(void) {
  return do_yesno_dialog(_("\nDo you wish to move the current clip sets to the new directory ?\n(If unsure, click Yes)\n"));
}

void do_set_locked_warning (const gchar *setname) {
  gchar *msg=g_strdup_printf(_("\nWarning - the set %s\nis in use by another copy of LiVES.\nYou are strongly advised to close the other copy before clicking OK to continue\n."),setname);
  do_error_dialog(msg);
  g_free(msg);
}

void do_no_in_vdevs_error(void) {
  do_error_dialog(_("\nNo video input devices could be found.\n"));
}

void do_locked_in_vdevs_error(void) {
  do_error_dialog(_("\nAll video input devices are already in use.\n"));
}

void do_do_not_close_d (void) {
  gchar *msg=g_strdup(_("\n\nCLEANING AND COPYING FILES. THIS MAY TAKE SOME TIME.\nDO NOT SHUT DOWN OR CLOSE LIVES !\n"));
  GtkWidget *err_box=create_dialog3(msg,FALSE,0);
  GtkWindow *transient=NULL;

  g_free(msg);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL&&mainw->is_ready) transient=GTK_WINDOW(mainw->LiVES);
    else if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) transient=GTK_WINDOW(mainw->multitrack->window);
  }

  if (transient!=NULL) gtk_window_set_transient_for(GTK_WINDOW(err_box),transient);
  gtk_widget_show(err_box);
  gtk_window_present (GTK_WINDOW (err_box));
  gdk_window_raise (err_box->window);
}


void do_set_noclips_error(const char *setname) {
  gchar *msg=g_strdup_printf (_ ("No clips were recovered for set (%s).\nPlease check the spelling of the set name and try again.\n"),setname);
  d_print (msg);
  g_free (msg);
}


void get_upd_msg(char *buf, size_t len) {
  LIVES_DEBUG("upd msg !");
  // TRANSLATORS: make sure the menu text matches what is in gui.c
  g_snprintf(buf,len,_("\nWelcome to LiVES version %s\n\nAfter upgrading, you are *strongly* advised to run:\n\nFile -> Clean up Diskspace\n"),LiVES_VERSION);
}


void do_no_autolives_error(void) {
  do_error_dialog(_("\nYou must have autolives.pl installed and in your path to use this toy.\nConsult your package distributor.\n"));
}

void do_autolives_needs_clips_error(void) {
  do_error_dialog(_("\nYou must have a minimum of one clip loaded to use this toy.\n"));
}

void do_jack_lost_conn_error(void) {
  do_error_dialog(_("\nLiVES lost its connection to jack and was unable to reconnect.\nRestarting LiVES is recommended.\n"));
}

void do_pulse_lost_conn_error(void) {
  do_error_dialog(_("\nLiVES lost its connection to pulseaudio and was unable to reconnect.\nRestarting LiVES is recommended.\n"));
}



gboolean ask_permission_dialog(int what) {
  gchar *msg;
  gboolean ret;

  if (!prefs->show_gui) return FALSE;

  switch (what) {
  case LIVES_PERM_OSC_PORTS:
    msg=g_strdup_printf(_("\nLiVES would like to open a local network connection (UDP port %d),\nto let other applications connect to it.\nDo you wish to allow this (for this session only) ?\n"),prefs->osc_udp_port);
    ret=do_yesno_dialog(msg);
    g_free(msg);
    return ret;
  default:
    break;
  }

  return FALSE;
}


