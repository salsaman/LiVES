// callbacks.h
// LiVES
// (c) G. Finch <salsaman@gmail.com> 2003 - 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_CALLBACKS_H
#define HAS_LIVES_CALLBACKS_H

boolean
on_LiVES_delete_event(LiVESWidget       *widget,
                      LiVESXEvent        *event,
                      livespointer         user_data);

void
on_open_activate(LiVESMenuItem     *menuitem,
                 livespointer         user_data);

void
on_open_sel_activate(LiVESMenuItem     *menuitem,
                     livespointer         user_data);

void
on_open_loc_activate(LiVESMenuItem     *menuitem,
                     livespointer         user_data);

void
on_open_utube_activate(LiVESMenuItem     *menuitem,
                       livespointer         user_data);

void
on_stop_clicked(LiVESMenuItem     *menuitem,
                livespointer         user_data);

void
on_save_selection_activate(LiVESMenuItem     *menuitem,
                           livespointer         user_data);

void
on_save_as_activate(LiVESMenuItem     *menuitem,
                    livespointer         user_data);

void
on_show_clipboard_info_activate(LiVESMenuItem     *menuitem,
                                livespointer         user_data);

void
on_close_activate(LiVESMenuItem     *menuitem,
                  livespointer         user_data);

void
on_import_proj_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

void
on_export_proj_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

void
on_quit_activate(LiVESMenuItem     *menuitem,
                 livespointer         user_data);

void
on_undo_activate(LiVESMenuItem     *menuitem,
                 livespointer         user_data);

void
on_redo_activate(LiVESMenuItem     *menuitem,
                 livespointer         user_data);

void
on_paste_as_new_activate(LiVESMenuItem     *menuitem,
                         livespointer         user_data);

void
on_copy_activate(LiVESMenuItem     *menuitem,
                 livespointer         user_data);

void
on_cut_activate(LiVESMenuItem     *menuitem,
                livespointer         user_data);

void
on_insert_pre_activate(LiVESMenuItem     *menuitem,
                       livespointer         user_data);

void
on_insert_activate(LiVESButton     *button,
                   livespointer         user_data);

void
on_merge_activate(LiVESMenuItem     *menuitem,
                  livespointer         user_data);

void
on_delete_activate(LiVESMenuItem     *menuitem,
                   livespointer         user_data);

void
on_select_all_activate(LiVESMenuItem     *menuitem,
                       livespointer         user_data);

void
on_select_new_activate(LiVESMenuItem     *menuitem,
                       livespointer         user_data);

void
on_select_last_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

void
on_select_to_end_activate(LiVESMenuItem     *menuitem,
                          livespointer         user_data);

void
on_select_from_start_activate(LiVESMenuItem     *menuitem,
                              livespointer         user_data);

void
on_lock_selwidth_activate(LiVESMenuItem     *menuitem,
                          livespointer         user_data);

void
on_playall_activate(LiVESMenuItem     *menuitem,
                    livespointer         user_data);

void
on_playsel_activate(LiVESMenuItem     *menuitem,
                    livespointer         user_data);

void
on_playclip_activate(LiVESMenuItem     *menuitem,
                     livespointer         user_data);

void
on_stop_activate(LiVESMenuItem     *menuitem,
                 livespointer         user_data);

void
on_rev_clipboard_activate(LiVESMenuItem     *menuitem,
                          livespointer         user_data);

void on_encoder_entry_changed(LiVESCombo *, livespointer ptr);

void on_vid_playback_plugin_changed(LiVESEntry *vpp_entry, livespointer user_data);


void
on_prefs_apply_clicked(LiVESButton       *button,
                       livespointer         user_data);

void
on_show_file_info_activate(LiVESMenuItem     *menuitem,
                           livespointer         user_data);

void
switch_clip_activate(LiVESMenuItem     *menuitem,
                     livespointer         user_data);

void
on_about_activate(LiVESMenuItem     *menuitem,
                  livespointer         user_data);

void
show_manual_activate(LiVESMenuItem     *menuitem,
                     livespointer         user_data);

void
email_author_activate(LiVESMenuItem     *menuitem,
                      livespointer         user_data);

void
donate_activate(LiVESMenuItem     *menuitem,
                livespointer         user_data);

void
report_bug_activate(LiVESMenuItem     *menuitem,
                    livespointer         user_data);

void
suggest_feature_activate(LiVESMenuItem     *menuitem,
                         livespointer         user_data);

void
help_translate_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

void
on_ok_button1_clicked(LiVESButton       *button,
                      livespointer         user_data);


void on_ok_file_open_clicked(LiVESFileChooser *, LiVESSList *fnames);


void open_sel_range_activate(void);



void
on_location_select(LiVESButton       *button,
                   livespointer         user_data);

void
on_utube_select(LiVESButton       *button,
                livespointer         user_data);

void
on_autoreload_toggled(LiVESToggleButton *togglebutton,
                      livespointer         user_data);

void
on_opensel_range_ok_clicked(LiVESButton       *button,
                            livespointer         user_data);


void on_open_sel_ok_button_clicked(LiVESButton *, livespointer user_data);

void on_save_textview_clicked(LiVESButton *, livespointer);


void on_filechooser_cancel_clicked(LiVESWidget *);


void on_full_screen_pressed(LiVESButton *button, livespointer user_data);

void
on_full_screen_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

void
on_double_size_pressed(LiVESButton *button,
                       livespointer user_data);
void
on_double_size_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

void
on_sepwin_pressed(LiVESButton *button,
                  livespointer user_data);

void
on_sepwin_activate(LiVESMenuItem     *menuitem,
                   livespointer         user_data);

void
on_fade_pressed(LiVESButton *button,
                livespointer user_data);

void
on_fade_activate(LiVESMenuItem     *menuitem,
                 livespointer         user_data);



void
on_loop_video_activate(LiVESMenuItem     *menuitem,
                       livespointer         user_data);

void
on_mute_button_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

void
on_mute_activate(LiVESMenuItem     *menuitem,
                 livespointer         user_data);



void
on_resize_activate(LiVESMenuItem     *menuitem,
                   livespointer         user_data);

void
on_rename_activate(LiVESMenuItem     *menuitem,
                   livespointer         user_data);

void
on_rename_set_name(LiVESButton       *button,
                   livespointer         user_data);

void
on_spinbutton_start_value_changed(LiVESSpinButton   *spinbutton,
                                  livespointer         user_data);

void
on_spinbutton_end_value_changed(LiVESSpinButton   *spinbutton,
                                livespointer         user_data);

void on_open_new_audio_clicked(LiVESFileChooser *, livespointer opt_filename);

void on_load_audio_activate(LiVESMenuItem *, livespointer);

void on_load_subs_activate(LiVESMenuItem *, livespointer);

void on_save_subs_activate(LiVESMenuItem *, livespointer entry_widget);

void on_erase_subs_activate(LiVESMenuItem *, livespointer);


void
on_insfitaudio_toggled(LiVESToggleButton *togglebutton,
                       livespointer         user_data);

void
on_resize_hsize_value_changed(LiVESSpinButton   *spinbutton,
                              livespointer         user_data);

void
on_resize_vsize_value_changed(LiVESSpinButton   *spinbutton,
                              livespointer         user_data);

void
on_resize_ok_clicked(LiVESButton       *button,
                     livespointer         user_data);


void
on_spin_value_changed(LiVESSpinButton   *spinbutton,
                      livespointer         user_data);

void
on_spin_start_value_changed(LiVESSpinButton   *spinbutton,
                            livespointer         user_data);

void
on_spin_step_value_changed(LiVESSpinButton   *spinbutton,
                           livespointer         user_data);

void
on_spin_end_value_changed(LiVESSpinButton   *spinbutton,
                          livespointer         user_data);


#if GTK_CHECK_VERSION(3,0,0)
boolean expose_vid_event(LiVESWidget *, lives_painter_t *cr, livespointer user_data);
#else
boolean expose_vid_event(LiVESWidget *, LiVESXEventExpose *event);
#endif

#if GTK_CHECK_VERSION(3,0,0)
boolean expose_laud_event(LiVESWidget *, lives_painter_t *cr, livespointer user_data);
#else
boolean expose_laud_event(LiVESWidget *, LiVESXEventExpose *event);
#endif

#if GTK_CHECK_VERSION(3,0,0)
boolean expose_raud_event(LiVESWidget *, lives_painter_t *cr, livespointer user_data);
#else
boolean expose_raud_event(LiVESWidget *, LiVESXEventExpose *event);
#endif



void
on_preview_clicked(LiVESButton       *button,
                   livespointer         user_data);

void
on_recent_activate(LiVESMenuItem     *menuitem,
                   livespointer         user_data);

boolean config_event(LiVESWidget *, LiVESXEventConfigure *, livespointer);

void
changed_fps_during_pb(LiVESSpinButton   *spinbutton,
                      livespointer         user_data);

boolean
on_mouse_scroll(LiVESWidget       *widget,
                LiVESXEventScroll  *event,
                livespointer         user_data);

boolean
on_mouse_sel_update(LiVESWidget       *widget,
                    LiVESXEventMotion  *event,
                    livespointer         user_data);

boolean
on_mouse_sel_reset(LiVESWidget       *widget,
                   LiVESXEventButton  *event,
                   livespointer         user_data);

boolean
on_mouse_sel_start(LiVESWidget       *widget,
                   LiVESXEventButton  *event,
                   livespointer         user_data);

void
on_load_cdtrack_activate(LiVESMenuItem     *menuitem,
                         livespointer         user_data);

void on_load_cdtrack_ok_clicked(LiVESButton     *button,
                                livespointer         user_data);

void
on_eject_cd_activate(LiVESMenuItem     *menuitem,
                     livespointer         user_data);


void
on_slower_pressed(LiVESButton *button,
                  livespointer user_data);

void
on_faster_pressed(LiVESButton *button,
                  livespointer user_data);

void
on_back_pressed(LiVESButton *button,
                livespointer user_data);

void
on_forward_pressed(LiVESButton *button,
                   livespointer user_data);

void on_capture_activate(LiVESMenuItem *, livespointer user_data);

void on_capture2_activate(void);


void
on_select_invert_activate(LiVESMenuItem     *menuitem,
                          livespointer         user_data);

void
on_warn_mask_toggled(LiVESToggleButton *togglebutton,
                     livespointer         user_data);

boolean
frame_context(LiVESWidget       *widget,
              LiVESXEventButton  *event,
              livespointer         which);

void on_fs_preview_clicked(LiVESWidget *widget, livespointer user_data);


void
on_restore_activate(LiVESMenuItem     *menuitem,
                    livespointer         user_data);

void
on_backup_activate(LiVESMenuItem     *menuitem,
                   livespointer         user_data);


void
on_record_perf_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

boolean record_toggle_callback(LiVESAccelGroup *, LiVESObject *, uint32_t keyval, LiVESXModifierType mod, livespointer);


boolean fps_reset_callback(LiVESAccelGroup *, LiVESObject *, uint32_t keyval, LiVESXModifierType mod, livespointer);


boolean mute_audio_callback(LiVESAccelGroup *, LiVESObject *, uint32_t keyval, LiVESXModifierType mod, livespointer);



boolean
on_stop_activate_by_del(LiVESWidget       *widget,
                        LiVESXEvent        *event,
                        livespointer         user_data);

void on_pause_clicked(void);


void
on_select_start_only_activate(LiVESMenuItem     *menuitem,
                              livespointer         user_data);

void
on_select_end_only_activate(LiVESMenuItem     *menuitem,
                            livespointer         user_data);

void
on_filesel_complex_clicked(LiVESButton *button,
                           LiVESEntry *entry);

void
on_filesel_complex_ok_clicked(LiVESButton *button,
                              LiVESEntry *entry);

void on_encoder_ofmt_changed(LiVESCombo *combo, livespointer user_data);

void
on_ok_export_audio_clicked(LiVESButton *button,
                           livespointer user_data);

void on_append_audio_activate(LiVESMenuItem *, livespointer user_data);

void
on_menubar_activate_menuitem(LiVESMenuItem     *menuitem,
                             livespointer         user_data);

void
on_rb_audrec_time_toggled(LiVESToggleButton *togglebutton,
                          livespointer         user_data);

void
on_recaudclip_activate(LiVESMenuItem     *menuitem,
                       livespointer         user_data);

void
on_recaudsel_activate(LiVESMenuItem     *menuitem,
                      livespointer         user_data);


void
on_recaudclip_ok_clicked(LiVESButton *button,
                         livespointer user_data);
#if GTK_CHECK_VERSION(2,14,0)
void on_volume_slider_value_changed(LiVESScaleButton *, livespointer);
#else
void on_volume_slider_value_changed(LiVESRange *, livespointer);
#endif

void on_fade_audio_activate(LiVESMenuItem *, livespointer user_data);


void
on_resample_video_activate(LiVESMenuItem     *menuitem,
                           livespointer         user_data);

void
on_resample_vid_ok(LiVESButton       *button,
                   LiVESEntry         *entry);

void on_trim_audio_activate(LiVESMenuItem *, livespointer user_data);

void
on_resample_audio_activate(LiVESMenuItem     *menuitem,
                           livespointer         user_data);


void
on_export_audio_activate(LiVESMenuItem *menuitem,
                         livespointer user_data);


void
on_resaudio_ok_clicked(LiVESButton *button,
                       LiVESEntry *entry);

void
on_cancel_opensel_clicked(LiVESButton       *button,
                          livespointer         user_data);

void
end_fs_preview(void);


void
on_sticky_activate(LiVESMenuItem     *menuitem,
                   livespointer         user_data);

void on_resaudw_asamps_changed(LiVESWidget *, livespointer);


void
on_insertwsound_toggled(LiVESToggleButton *togglebutton,
                        livespointer         user_data);

void
on_showfct_activate(LiVESMenuItem     *menuitem,
                    livespointer         user_data);

void on_boolean_toggled(LiVESObject *, livespointer user_data);


void on_showsubs_toggled(LiVESObject *, livespointer);

void
on_show_messages_activate(LiVESMenuItem     *menuitem,
                          livespointer         user_data);

boolean on_hrule_enter(LiVESWidget *, LiVESXEventCrossing *, livespointer);

boolean
on_hrule_update(LiVESWidget       *widget,
                LiVESXEventMotion  *event,
                livespointer         user_data);
boolean
on_hrule_reset(LiVESWidget       *widget,
               LiVESXEventButton  *event,
               livespointer         user_data);

boolean
on_hrule_set(LiVESWidget       *widget,
             LiVESXEventButton  *event,
             livespointer         user_data);

void
on_rewind_activate(LiVESMenuItem     *menuitem,
                   livespointer         user_data);
void
on_loop_button_activate(LiVESMenuItem     *menuitem,
                        livespointer         user_data);

void
on_loop_cont_activate(LiVESMenuItem     *menuitem,
                      livespointer         user_data);

void on_show_file_comments_activate(LiVESMenuItem *menuitem, livespointer user_data);

void on_toolbar_hide(LiVESButton *button, livespointer user_data);

void on_toy_activate(LiVESMenuItem *new_toy, livespointer old_toy_p);

void on_preview_spinbutton_changed(LiVESSpinButton   *spinbutton, livespointer user_data);

boolean prevclip_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer);

boolean nextclip_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer);

boolean freeze_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer);

boolean storeclip_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer);

boolean nervous_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer);

boolean show_sync_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer clip_number);

boolean on_save_set_activate(LiVESMenuItem *, livespointer);

void on_save_set_ok(void);

char *on_load_set_activate(LiVESMenuItem *, livespointer);

boolean reload_set(const char *set_name);

void on_open_vcd_activate(LiVESMenuItem *, livespointer int_type);

void on_load_vcd_ok_clicked(LiVESButton *, livespointer user_data);

void on_ping_pong_activate(LiVESMenuItem *, livespointer);

void on_show_keys_activate(LiVESMenuItem *, livespointer);

void on_vj_reset_activate(LiVESMenuItem *, livespointer);

void on_prv_link_toggled(LiVESToggleButton *, livespointer);

boolean on_del_audio_activate(LiVESMenuItem *, livespointer);

boolean on_ins_silence_activate(LiVESMenuItem *, livespointer);

void on_ins_silence_details_clicked(LiVESButton *, livespointer);

void on_lerrors_close_clicked(LiVESButton *, livespointer);
void on_lerrors_clear_clicked(LiVESButton *, livespointer);
void on_lerrors_delete_clicked(LiVESButton *, livespointer);

#ifdef GUI_GTK
void drag_from_outside(LiVESWidget *, GdkDragContext *, int x, int y,
                       GtkSelectionData *, uint32_t info, uint32_t time, livespointer user_data);

#endif

#endif
