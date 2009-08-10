// ldvcallbacks.h
// LiVES
// (c) G. Finch 2006 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


void on_camgrab_clicked (GtkButton *button, gpointer user_data);
void on_camplay_clicked (GtkButton *button, gpointer user_data);
void on_camstop_clicked (GtkButton *button, gpointer user_data);
void on_camrew_clicked (GtkButton *button, gpointer user_data);
void on_camff_clicked (GtkButton *button, gpointer user_data);
void on_cameject_clicked (GtkButton *button, gpointer user_data);
void on_camfile_clicked (GtkFileChooser *ch, gpointer entry);
void on_campause_clicked (GtkButton *button, gpointer user_data);
void on_camquit_clicked (GtkButton *button, gpointer user_data);

gboolean on_camdelete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data);
