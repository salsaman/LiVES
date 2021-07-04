// callbacks.h
// LiVES
// (c) G. Finch <salsaman+lives@gmail.com> 2003 - 2017
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_CALLBACKS_H
#define HAS_LIVES_CALLBACKS_H

#include "interface.h"

void lives_exit(int signum);

void play_all(boolean from_menu);
void play_sel(void);

void on_effects_paused(LiVESButton *, livespointer);

void on_cancel_keep_button_clicked(LiVESButton *, livespointer);

void on_cleardisk_activate(LiVESWidget *, livespointer);

void on_cleardisk_advanced_clicked(LiVESWidget *, livespointer);

boolean check_for_layout_errors(const char *operation, int fileno, int start, int end, uint32_t *in_mask);

void popup_lmap_errors(LiVESMenuItem *, livespointer);

void switch_clip(int type, int newclip, boolean force);

void switch_clip_activate(LiVESMenuItem *, livespointer);

void on_details_button_clicked(void);

boolean on_LiVES_delete_event(LiVESWidget *, LiVESXEvent *, livespointer);

void on_open_activate(LiVESMenuItem *, livespointer);

void on_open_sel_activate(LiVESMenuItem *, livespointer);

void on_open_loc_activate(LiVESMenuItem *, livespointer);

void on_open_utube_activate(LiVESMenuItem *, livespointer);

void on_stop_clicked(LiVESMenuItem *, livespointer);

#ifdef LIBAV_TRANSCODE
void on_transcode_activate(LiVESMenuItem *, livespointer);
#endif

void on_save_selection_activate(LiVESMenuItem *, livespointer);

void on_save_as_activate(LiVESMenuItem *, livespointer);

void on_show_clipboard_info_activate(LiVESMenuItem *, livespointer);

void on_close_activate(LiVESMenuItem *, livespointer);

void on_import_proj_activate(LiVESMenuItem *, livespointer);

void on_export_proj_activate(LiVESMenuItem *, livespointer);

void on_export_theme_activate(LiVESMenuItem *, livespointer);

void on_import_theme_activate(LiVESMenuItem *, livespointer);

void on_quit_activate(LiVESMenuItem *, livespointer);

void on_undo_activate(LiVESWidget *, livespointer);

void on_redo_activate(LiVESWidget *, livespointer);

void on_paste_as_new_activate(LiVESMenuItem *, livespointer);

void on_copy_activate(LiVESMenuItem *, livespointer);

void on_cut_activate(LiVESMenuItem *, livespointer);

void on_insert_pre_activate(LiVESMenuItem *, livespointer);

void on_insert_activate(LiVESButton *, livespointer);

void on_merge_activate(LiVESMenuItem *, livespointer);

void on_delete_activate(LiVESMenuItem *, livespointer);

void on_trim_vid_activate(LiVESMenuItem *, livespointer);

void update_sel_menu(void);

void sel_vismatch_activate(LiVESWidget *, livespointer);

void on_select_activate(LiVESWidget *widget, livespointer type);

void sel_skipbl_activate(LiVESWidget *, livespointer);

void on_select_new_activate(LiVESMenuItem *, livespointer);

void on_select_last_activate(LiVESMenuItem *, livespointer);

void on_select_to_aend_activate(LiVESMenuItem *, livespointer);

void on_lock_selwidth_activate(LiVESMenuItem *, livespointer);

void on_playall_activate(LiVESMenuItem *, livespointer);

void on_playsel_activate(LiVESMenuItem *, livespointer);

void on_playclip_activate(LiVESMenuItem *, livespointer);

void on_stop_activate(LiVESMenuItem *, livespointer);

void on_rev_clipboard_activate(LiVESMenuItem *, livespointer);

void on_encoder_entry_changed(LiVESCombo *, livespointer ptr);

void on_show_file_info_activate(LiVESMenuItem *, livespointer);

void on_about_activate(LiVESMenuItem *, livespointer);

void show_manual_activate(LiVESMenuItem *, livespointer);

void email_author_activate(LiVESMenuItem *, livespointer);

void donate_activate(LiVESMenuItem *, livespointer);

void report_bug_activate(LiVESMenuItem *, livespointer);

void suggest_feature_activate(LiVESMenuItem *, livespointer);

void help_translate_activate(LiVESMenuItem *, livespointer);

void vj_mode_toggled(LiVESCheckMenuItem *, livespointer);

void on_ok_button1_clicked(LiVESButton *, livespointer);

void on_ok_file_open_clicked(LiVESFileChooser *, LiVESSList *fnames);

void on_location_select(LiVESButton *, livespointer);

lives_remote_clip_request_t *on_utube_select(lives_remote_clip_request_t *req, const char *tmpdir);

void on_open_sel_ok_button_clicked(LiVESButton *, livespointer);

void on_save_textview_clicked(LiVESButton *, livespointer);

void on_filechooser_cancel_clicked(LiVESWidget *);

void on_full_screen_pressed(LiVESButton *, livespointer);

void on_full_screen_activate(LiVESMenuItem *, livespointer);

void on_double_size_pressed(LiVESButton *, livespointer);

void on_double_size_activate(LiVESMenuItem *, livespointer);

void on_sepwin_pressed(LiVESButton *, livespointer);

void on_sepwin_activate(LiVESMenuItem *, livespointer);

void on_fade_pressed(LiVESButton *, livespointer);

void on_fade_activate(LiVESMenuItem *, livespointer);

void on_loop_video_activate(LiVESMenuItem *, livespointer);

void on_mute_button_activate(LiVESMenuItem *, livespointer);

void on_mute_activate(LiVESMenuItem *, livespointer);

void on_resize_activate(LiVESMenuItem *, livespointer);

void on_rename_activate(LiVESMenuItem *, livespointer);

void on_rename_clip_name(LiVESButton *, livespointer);

void on_spinbutton_start_value_changed(LiVESSpinButton *, livespointer);

void on_spinbutton_end_value_changed(LiVESSpinButton *, livespointer);

void on_open_new_audio_clicked(LiVESFileChooser *, livespointer opt_filename);

void on_load_audio_activate(LiVESMenuItem *, livespointer);

void on_load_subs_activate(LiVESMenuItem *, livespointer);

void on_save_subs_activate(LiVESMenuItem *, livespointer entry_widget);

void on_erase_subs_activate(LiVESMenuItem *, livespointer);

void on_insfitaudio_toggled(LiVESToggleButton *, livespointer);

void on_resize_hsize_value_changed(LiVESSpinButton *, livespointer);

void on_resize_vsize_value_changed(LiVESSpinButton *, livespointer);

void on_resize_ok_clicked(LiVESButton *, livespointer);

void on_spin_value_changed(LiVESSpinButton *, livespointer);

void on_spin_start_value_changed(LiVESSpinButton *, livespointer  user_data);

void on_spin_step_value_changed(LiVESSpinButton *, livespointer);

void on_spin_end_value_changed(LiVESSpinButton *, livespointer);

EXPOSE_FN_PROTOTYPE(expose_vid_event);

EXPOSE_FN_PROTOTYPE(expose_laud_event);

EXPOSE_FN_PROTOTYPE(expose_raud_event);

void on_preview_clicked(LiVESButton *, livespointer);

void on_recent_activate(LiVESMenuItem *, livespointer);

boolean all_expose_pb(LiVESWidget *, lives_painter_t *, livespointer psurf);
boolean all_expose_overlay(LiVESWidget *, lives_painter_t *, livespointer psurf);
boolean all_expose_nopb(LiVESWidget *, lives_painter_t *, livespointer psurf);

boolean config_event(LiVESWidget *, LiVESXEventConfigure *, livespointer);
boolean config_event2(LiVESWidget *, LiVESXEventConfigure *, livespointer);

void changed_fps_during_pb(LiVESSpinButton *, livespointer);

void on_volch_pressed(LiVESButton *, livespointer dirny);

boolean on_mouse_scroll(LiVESWidget *, LiVESXEventScroll *, livespointer);

boolean on_mouse_sel_update(LiVESWidget *, LiVESXEventMotion *, livespointer);

boolean on_mouse_sel_reset(LiVESWidget *, LiVESXEventButton *, livespointer);

boolean on_mouse_sel_start(LiVESWidget *, LiVESXEventButton *, livespointer);

void on_load_cdtrack_activate(LiVESMenuItem *, livespointer);

void on_load_cdtrack_ok_clicked(LiVESButton *, livespointer);

void on_eject_cd_activate(LiVESMenuItem *, livespointer);

void on_slower_pressed(LiVESButton *, livespointer);

void on_faster_pressed(LiVESButton *, livespointer);

void on_less_pressed(LiVESButton *, livespointer);

void on_more_pressed(LiVESButton *, livespointer);

void on_back_pressed(LiVESButton *, livespointer);

void on_forward_pressed(LiVESButton *, livespointer);

boolean dirchange_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer area_enum);

boolean dirchange_lock_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod,
                                livespointer area_enum);

void unlock_loop_lock(void);

void rec_desktop(LiVESMenuItem *, livespointer);

void on_capture_activate(LiVESMenuItem *, livespointer);

void on_capture2_activate(void);

void on_warn_mask_toggled(LiVESToggleButton *, livespointer);

boolean frame_context(LiVESWidget *, LiVESXEventButton *, livespointer which);

void on_fs_preview_clicked1(LiVESWidget *, LiVESDialog *);
void on_fs_preview_clicked2(LiVESWidget *, opensel_win *);

void on_restore_activate(LiVESMenuItem *, livespointer);

void on_backup_activate(LiVESMenuItem *, livespointer);

void on_record_perf_activate(LiVESMenuItem *, livespointer);

boolean record_toggle_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod, livespointer);

boolean fps_reset_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod, livespointer);

boolean mute_audio_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod, livespointer);

boolean on_stop_activate_by_del(LiVESWidget *, LiVESXEvent *, livespointer);

void on_pause_clicked(void);

void on_encoder_ofmt_changed(LiVESCombo *, livespointer);

void on_ok_export_audio_clicked(LiVESButton *, livespointer);

void on_normalise_audio_activate(LiVESMenuItem *, livespointer);

void on_append_audio_activate(LiVESMenuItem *, livespointer);

void on_rb_audrec_time_toggled(LiVESToggleButton *, livespointer);

void on_recaudclip_activate(LiVESMenuItem *, livespointer);

void on_recaudsel_activate(LiVESMenuItem *, livespointer);

void on_recaudclip_ok_clicked(LiVESButton *, livespointer);

#if GTK_CHECK_VERSION(2, 14, 0)
void on_volume_slider_value_changed(LiVESScaleButton *, livespointer);
#else
void on_volume_slider_value_changed(LiVESRange *, livespointer);
#endif

void on_voladj_activate(LiVESMenuItem *, livespointer);

void on_fade_audio_activate(LiVESMenuItem *, livespointer);

void on_resample_video_activate(LiVESMenuItem *, livespointer);

void on_resample_vid_ok(LiVESButton *, livespointer pclipno);

boolean on_trim_audio_activate(LiVESMenuItem *, livespointer);

void on_resample_audio_activate(LiVESMenuItem *, livespointer);

void on_export_audio_activate(LiVESMenuItem *, livespointer);

void on_resaudio_ok_clicked(LiVESButton *, LiVESEntry *entry);

void end_fs_preview(LiVESFileChooser *fchoo, LiVESWidget *button);

void on_sticky_activate(LiVESMenuItem *, livespointer);

void on_resaudw_asamps_changed(LiVESWidget *, livespointer);

void on_insertwsound_toggled(LiVESToggleButton *, livespointer);

void on_showfct_activate(LiVESMenuItem *, livespointer);

void on_boolean_toggled(LiVESWidgetObject *, livespointer);

void on_audio_toggled(LiVESWidget *, LiVESWidget *label);

void on_showsubs_toggled(LiVESWidgetObject *, livespointer);

void on_show_messages_activate(LiVESMenuItem *, livespointer);

#ifdef ENABLE_GIW_3
void on_hrule_value_changed(LiVESWidget *widget, livespointer user_data);
#else
boolean on_hrule_update(LiVESWidget *, LiVESXEventMotion *, livespointer);
boolean on_hrule_set(LiVESWidget *, LiVESXEventButton *, livespointer);
boolean on_hrule_reset(LiVESWidget *, LiVESXEventButton *, livespointer);
#endif

void on_rewind_activate(LiVESMenuItem *, livespointer);

void on_loop_button_activate(LiVESMenuItem *, livespointer);

void on_loop_cont_activate(LiVESMenuItem *, livespointer);

void on_show_file_comments_activate(LiVESMenuItem *, livespointer);

void on_toolbar_hide(LiVESButton *, livespointer);

void on_toy_activate(LiVESMenuItem *, livespointer old_toy_p);

void autolives_toggle(LiVESMenuItem *, livespointer);

void on_preview_spinbutton_changed(LiVESSpinButton *, livespointer);

boolean prevclip_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer);

boolean nextclip_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer);

boolean freeze_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer);

boolean storeclip_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer);

boolean retrigger_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType,
                           livespointer);

boolean nervous_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer);

boolean aud_lock_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod,
                          livespointer statep);

boolean aud_lock_act(LiVESToggleToolButton *, livespointer statep);

char *get_palette_name_for_clip(int clipno);

boolean show_sync_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t, LiVESXModifierType, livespointer clip_number);

void on_open_vcd_activate(LiVESMenuItem *, livespointer int_type);

void on_load_vcd_ok_clicked(LiVESButton *, livespointer);

void on_ping_pong_activate(LiVESMenuItem *, livespointer);

void on_show_keys_activate(LiVESMenuItem *, livespointer);

void on_vj_reset_activate(LiVESMenuItem *, livespointer);

void on_vj_realize_activate(LiVESMenuItem *, livespointer);

void on_prv_link_toggled(LiVESToggleButton *, livespointer);

boolean on_del_audio_activate(LiVESMenuItem *, livespointer);

boolean on_ins_silence_activate(LiVESMenuItem *, livespointer);

void audio_details_clicked(LiVESButton *, livespointer clipno);

void on_lerrors_close_clicked(LiVESButton *, livespointer);

void on_lerrors_clear_clicked(LiVESButton *, livespointer);

void on_lerrors_delete_clicked(LiVESButton *, livespointer);

#ifdef GUI_GTK
void drag_from_outside(LiVESWidget *, GdkDragContext *, int x, int y,
                       GtkSelectionData *, uint32_t info, uint32_t time, livespointer);

#endif

#endif
