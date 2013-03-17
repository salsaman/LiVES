// ldvcallbacks.h
// LiVES
// (c) G. Finch 2006 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


void on_camgrab_clicked (GtkButton *, gpointer s_cam);
void on_camplay_clicked (GtkButton *, gpointer s_cam);
void on_camstop_clicked (GtkButton *, gpointer s_cam);
void on_camrew_clicked (GtkButton *, gpointer s_cam);
void on_camff_clicked (GtkButton *, gpointer s_cam);
void on_cameject_clicked (GtkButton *, gpointer s_cam);
void on_campause_clicked (GtkButton *, gpointer s_cam);
void on_camquit_clicked (GtkButton *, gpointer s_cam);

boolean on_camdelete_event (GtkWidget *, GdkEvent *, gpointer s_cam);
