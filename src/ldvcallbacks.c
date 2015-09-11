// ldvcallbacks.c
// LiVES
// (c) G. Finch 2006 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "support.h"
#include "ldvcallbacks.h"
#include "ldvinterface.h"



void on_camgrab_clicked(LiVESButton *button, livespointer user_data) {
  char *msg;
  s_cam *cam=(s_cam *)user_data;
  if (dvgrabw->filename!=NULL) lives_free(dvgrabw->filename);
  dvgrabw->filename=find_free_camfile(cam->format);
  if (dvgrabw->filename==NULL) return;
  lives_widget_set_sensitive(dvgrabw->grab,FALSE);
  lives_set_cursor_style(LIVES_CURSOR_BUSY,dvgrabw->dialog);
  if (!dvgrabw->playing) on_camplay_clicked(NULL,user_data);

  if (dvgrabw->dirname!=NULL) lives_free(dvgrabw->dirname);
  dvgrabw->dirname=lives_strdup(lives_entry_get_text(LIVES_ENTRY(dvgrabw->dirent)));

  msg=lives_strdup_printf(_("Recording to %s/%s"),dvgrabw->dirname,dvgrabw->filename);
  lives_entry_set_text(LIVES_ENTRY(dvgrabw->status_entry),msg);
  if (cam->format==CAM_FORMAT_DV) {
    dvgrabw->filename=lives_strdup(lives_entry_get_text(LIVES_ENTRY(dvgrabw->filent)));
  }
  rec(cam);
  cam->grabbed_clips=TRUE;
}


void on_camstop_clicked(LiVESButton *button, livespointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  lives_widget_set_sensitive(dvgrabw->stop,FALSE);
  dvgrabw->playing=FALSE;

  if (cam->format==CAM_FORMAT_DV) {
    if (cam->pgid!=0) lives_killpg(cam->pgid, LIVES_SIGTERM);
    cam->pgid=0;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,dvgrabw->dialog);
  camstop(cam);
  lives_button_set_label(LIVES_BUTTON(dvgrabw->play),LIVES_STOCK_LABEL_MEDIA_PLAY);
  lives_widget_set_sensitive(dvgrabw->grab,TRUE);
  lives_entry_set_text(LIVES_ENTRY(dvgrabw->status_entry),_("Status: Ready"));
}


void on_camplay_clicked(LiVESButton *button, livespointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  camplay(cam);
  dvgrabw->playing=!dvgrabw->playing;
  if (dvgrabw->playing) {
    lives_button_set_label(LIVES_BUTTON(dvgrabw->play),LIVES_STOCK_LABEL_MEDIA_PAUSE);
  } else {
    lives_button_set_label(LIVES_BUTTON(dvgrabw->play),LIVES_STOCK_LABEL_MEDIA_PLAY);
  }
  lives_widget_set_sensitive(dvgrabw->stop,TRUE);
}

void on_camrew_clicked(LiVESButton *button, livespointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  camrew(cam);
  lives_widget_set_sensitive(dvgrabw->stop,TRUE);
}


void on_camff_clicked(LiVESButton *button, livespointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  camff(cam);
  lives_widget_set_sensitive(dvgrabw->stop,TRUE);
}

void on_cameject_clicked(LiVESButton *button, livespointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  cameject(cam);
}

/*
boolean on_camdelete_event(LiVESWidget *widget, GdkEvent *event, livespointer user_data) {
  on_camquit_clicked(NULL,user_data);
  return FALSE;
}
*/

void on_camquit_clicked(LiVESButton *button, livespointer user_data) {
  s_cam *cam=(s_cam *)user_data;
  on_camstop_clicked(button,user_data);
  //if (cam->format==CAM_FORMAT_HDV) close_raw1394(cam->rec_handle);
  lives_widget_destroy(dvgrabw->dialog);
  if (dvgrabw->cursor!=NULL) lives_cursor_unref(dvgrabw->cursor);
  if (dvgrabw->filename!=NULL) lives_free(dvgrabw->filename);
  if (dvgrabw->dirname!=NULL) lives_free(dvgrabw->dirname);
  if (cam->grabbed_clips) do_error_dialog_with_check
    (_("\nClips grabbed from the device can now be loaded with File|Open File/Directory.\n"),
     WARN_MASK_AFTER_DVGRAB);

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}

