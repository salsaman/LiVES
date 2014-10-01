// merge.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2013
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_MERGE_H
#define HAS_LIVES_MERGE_H

void create_merge_dialog (void);

void on_merge_activate (LiVESMenuItem *, gpointer);

void on_merge_ok_clicked (LiVESButton *, gpointer);

void on_align_start_end_toggled (LiVESToggleButton *, gpointer);

void on_trans_method_changed (LiVESCombo *, gpointer);

void on_merge_cancel_clicked (LiVESButton *, gpointer);

void on_fit_toggled (LiVESToggleButton *, gpointer);

void on_ins_frames_toggled (LiVESToggleButton *, gpointer);

void after_spinbutton_loops_changed (LiVESSpinButton *, gpointer);


#endif
