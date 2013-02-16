// ldvcallbacks.c
// LiVES
// (c) G. Finch 2006 - 2013 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "support.h"
#include "ldvcallbacks.h"
#include "ldvinterface.h"



void on_camgrab_clicked (GtkButton *button, gpointer user_data) {
  gchar *msg;
  s_cam *cam=(s_cam *)user_data;
  if (dvgrabw->filename!=NULL) g_free(dvgrabw->filename);
  dvgrabw->filename=find_free_camfile(cam->format);
  if (dvgrabw->filename==NULL) return;
  gtk_widget_set_sensitive(dvgrabw->grab,FALSE);
  lives_set_cursor_style(LIVES_CURSOR_BUSY,dvgrabw->window);
  if (!dvgrabw->playing) on_camplay_clicked(NULL,user_data);
  msg=g_strdup_printf(_("Recording to %s/%s"),dvgrabw->dirname,dvgrabw->filename);
  gtk_entry_set_text(GTK_ENTRY(dvgrabw->status_entry),msg);
  if (cam->format==CAM_FORMAT_DV) {
    dvgrabw->filename=g_strdup(gtk_entry_get_text(GTK_ENTRY(dvgrabw->filent)));
  }
  rec(cam);
  cam->grabbed_clips=TRUE;
}


void on_camstop_clicked (GtkButton *button, gpointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  gtk_widget_set_sensitive(dvgrabw->stop,FALSE);
  dvgrabw->playing=FALSE;

  if (cam->format==CAM_FORMAT_DV) {
    lives_kill(cam->pgid, LIVES_SIGTERM);
    cam->pgid=-0;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,dvgrabw->window);
  camstop(cam);
  gtk_button_set_label(GTK_BUTTON(dvgrabw->play),"gtk-media-play");
  gtk_widget_set_sensitive(dvgrabw->grab,TRUE);
  gtk_entry_set_text(GTK_ENTRY(dvgrabw->status_entry),_("Status: Ready"));
}


void on_camplay_clicked (GtkButton *button, gpointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  camplay(cam);
  dvgrabw->playing=!dvgrabw->playing;
  if (dvgrabw->playing) {
    gtk_button_set_label(GTK_BUTTON(dvgrabw->play),"gtk-media-pause");
  }
  else {
    gtk_button_set_label(GTK_BUTTON(dvgrabw->play),"gtk-media-play");
  }
  gtk_widget_set_sensitive(dvgrabw->stop,TRUE);
}

void on_camrew_clicked (GtkButton *button, gpointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  camrew(cam);
  gtk_widget_set_sensitive(dvgrabw->stop,TRUE);
}


void on_camff_clicked (GtkButton *button, gpointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  camff(cam);
  gtk_widget_set_sensitive(dvgrabw->stop,TRUE);
}

void on_cameject_clicked (GtkButton *button, gpointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  cameject(cam);
}


void on_camfile_clicked (GtkFileChooser *ch, gpointer entry) {
  gchar *dir=gtk_file_chooser_get_current_folder(ch);
  if (access(dir,W_OK)) {
    gchar *tmp;
    gtk_file_chooser_set_current_folder(ch,(tmp=g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(entry)),-1,NULL,NULL,NULL)));
    g_free(tmp);
    return;
  }
  if (dvgrabw->dirname!=NULL) g_free(dvgrabw->dirname);
  gtk_entry_set_text(GTK_ENTRY(entry),(dvgrabw->dirname=g_filename_to_utf8(dir,-1,NULL,NULL,NULL)));
  g_free(dir);
}



boolean on_camdelete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  on_camquit_clicked(NULL,user_data);
  return FALSE;
}


void on_camquit_clicked (GtkButton *button, gpointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  on_camstop_clicked(button,user_data);
  //if (cam->format==CAM_FORMAT_HDV) close_raw1394(cam->rec_handle);
  gtk_widget_destroy(dvgrabw->window);
  if (dvgrabw->cursor!=NULL) lives_cursor_unref(dvgrabw->cursor);
  if (dvgrabw->filename!=NULL) g_free(dvgrabw->filename);
  if (dvgrabw->dirname!=NULL) g_free(dvgrabw->dirname);
  if (cam->grabbed_clips) do_error_dialog_with_check
			    (_("\nClips grabbed from the device can now be loaded with File|Open File/Directory.\n"),
			     WARN_MASK_AFTER_DVGRAB);

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}

