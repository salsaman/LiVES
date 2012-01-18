// merge.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2012
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_MERGE_H
#define HAS_LIVES_MERGE_H

void create_merge_dialog (void);

void on_merge_activate (GtkMenuItem *, gpointer);

void on_merge_ok_clicked (GtkButton *, gpointer);

void on_align_start_end_toggled (GtkToggleButton *, gpointer);

void on_trans_method_changed (GtkWidget *, gpointer);

void bang (GtkWidget *, gpointer);

void on_merge_cancel_clicked (GtkButton *, gpointer);

void on_fit_toggled (GtkToggleButton *, gpointer);

void on_ins_frames_toggled (GtkToggleButton *, gpointer);

void after_spinbutton_loops_changed (GtkSpinButton *, gpointer);


#endif
