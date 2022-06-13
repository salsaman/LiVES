// dialogs.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _DIALOGS_H_
#define _DIALOGS_H_

LiVESWindow *get_transient_full();

/// generic ......

boolean do_warning_dialog(const char *text);
boolean do_warning_dialogf(const char *fmt, ...);
boolean do_warning_dialog_with_check(const char *text, uint64_t warn_mask_number);

boolean do_yesno_dialog(const char *text);
boolean do_yesno_dialogf(const char *fmt, ...);
boolean do_yesno_dialog_with_check(const char *text, uint64_t warn_mask_number);
boolean do_yesno_dialogf_with_countdown(int nclicks, boolean isyes, const char *fmt, ...);

void maybe_abort(boolean do_check, LiVESList *restart_opts);
void do_abortblank_error(const char *what);
void do_abort_dialog(const char *text);
LiVESResponseType do_abort_ok_dialog(const char *text);
LiVESResponseType do_abort_retry_dialog(const char *text);
LiVESResponseType do_abort_retry_cancel_dialog(const char *text) WARN_UNUSED;
LiVESResponseType do_abort_retry_ignore_dialog(const char *text) WARN_UNUSED;

LiVESResponseType do_retry_cancel_dialog(const char *text);

LiVESResponseType do_error_dialog(const char *text);
LiVESResponseType do_error_dialogf(const char *fmt, ...);
LiVESResponseType do_error_dialog_with_check(const char *text, uint64_t warn_mask_number);

LiVESResponseType do_info_dialog(const char *text);
LiVESResponseType do_info_dialogf(const char *fmt, ...);
LiVESResponseType do_info_dialog_with_expander(const char *text, const char *exp_text, LiVESList *);

LiVESWidget *create_message_dialog(lives_dialog_t diat, const char *text, int warn_mask_number);
LiVESWidget *create_question_dialog(const char *title, const char *text);

void do_text_window(const char *title, const char *text);

LiVESResponseType lives_dialog_run_with_countdown(LiVESDialog *dialog, LiVESResponseType target,
    int nclicks);

boolean do_abort_check(void);

LiVESResponseType do_abort_restart_check(boolean allow_restart, LiVESList *restart_opts);

void response_ok(LiVESButton *button, livespointer user_data);

/////////// progress dialogs /////
boolean do_progress_dialog(boolean visible, boolean cancellable, const char *text);

void cancel_process(boolean visible);

void update_progress(boolean visible, int clipno);
void do_threaded_dialog(const char *translated_text, boolean has_cancel);
void end_threaded_dialog(void);
void threaded_dialog_spin(double fraction);
void threaded_dialog_push(void);
void threaded_dialog_pop(void);
void threaded_dialog_auto_spin(void);

boolean do_auto_dialog(const char *text, int type);

void do_splash_progress(void);

void pump_io_chan(LiVESIOChannel *iochan);

//////////// general //

void add_resnn_label(LiVESDialog *dialog);

void do_optarg_blank_err(const char *what);
void do_clip_divergence_error(int fileno);

LiVESResponseType do_file_perm_error(const char *file_name, boolean allow_cancel);
LiVESResponseType do_dir_perm_error(const char *dir_name, boolean allow_cancel);
void do_dir_perm_access_error(const char *dir_name);

LiVESResponseType do_system_failed_error(const char *com, int retval, const char *addinfo, boolean can_retry,
    boolean try_sudo);
LiVESResponseType do_write_failed_error_s_with_retry(const char *fname, const char *errtext) WARN_UNUSED;
void do_write_failed_error_s(const char *filename, const char *addinfo);
LiVESResponseType do_read_failed_error_s_with_retry(const char *fname, const char *errtext) WARN_UNUSED;
void do_read_failed_error_s(const char *filename, const char *addinfo);
boolean do_header_write_error(int clip);
void do_chdir_failed_error(const char *dir);

// detail can be set to override default "do not show this warning again"
// e.g "show this warning at startup"
void add_warn_check(LiVESBox *box, uint64_t warn_mask_number, const char *detail);

LiVESResponseType do_file_notfound_dialog(const char *detail, const char *dirname);
LiVESResponseType do_dir_notfound_dialog(const char *detail, const char *dirname);

LiVESResponseType do_header_read_error_with_retry(int clip) WARN_UNUSED;
LiVESResponseType do_header_missing_detail_error(int clip, lives_clip_details_t detail) WARN_UNUSED;

LiVESResponseType do_please_install(const char *info, const char *exec, const char *exec2,
                                    uint64_t guidance_flags);
boolean do_please_install_either(const char *exec, const char *exec2);

//// move to other file...///

boolean check_backend_return(lives_clip_t *sfile);
LiVESResponseType handle_backend_errors(boolean can_retry);

///////////// specific ///

void do_messages_window(boolean is_startup);

/** warn about disk space */
char *ds_critical_msg(const char *dir, char **mountpoint, uint64_t dsval);
char *ds_warning_msg(const char *dir, char **mountpoint, uint64_t dsval, uint64_t cwarn, uint64_t nwarn);
char *get_upd_msg(void);

void do_exec_missing_error(const char *execname);
boolean ask_permission_dialog(int what);
boolean ask_permission_dialog_complex(int what, char **argv, int argc, int offs, const char *sudocom);

LiVESResponseType do_memory_error_dialog(char *op, size_t bytes);

boolean check_del_workdir(const char *dirname);
char *workdir_ch_warning(void);
void workdir_warning(void);
boolean do_move_workdir_dialog(void);
boolean do_noworkdirchange_dialog(void);

void do_shutdown_msg(void);

void do_do_not_close_d(void);

void too_many_files(void);

void do_audio_import_error(void);
void do_mt_backup_space_error(lives_mt *, int memreq_mb);

boolean do_close_changed_warn(void);

boolean do_comments_dialog(int fileno, char *filename);

////////////////

boolean do_fxload_query(int maxkey, int maxmode);

LiVESResponseType do_resize_dlg(int cwidth, int cheight, int fwidth, int fheight);
LiVESResponseType do_imgfmts_error(lives_img_type_t imgtype);

boolean do_clipboard_fps_warning(void);

void perf_mem_warning(void);
void do_dvgrab_error(void);

void do_encoder_acodec_error(void);
void do_encoder_sox_error(void);

boolean rdet_suggest_values(int width, int height, double fps, int fps_num, int fps_denom, int arate,
                            int asigned, boolean swap_endian, boolean anr, boolean ignore_fps);
boolean do_encoder_restrict_dialog(int width, int height, double fps, int fps_num, int fps_denom,
                                   int arate, int asigned, boolean swap_endian, boolean anr, boolean save_all);
void do_no_mplayer_sox_error(void);
void do_need_mplayer_dialog(void);
void do_need_mplayer_mpv_dialog(void);
void do_aud_during_play_error(void);

const char *miss_plugdirs_warn(const char *dirnm, LiVESList *subdirs);
const char *miss_prefix_warn(const char *dirnm, LiVESList *subdirs);

void do_program_not_found_error(const char *progname);

//////////// targetted ///
void do_layout_scrap_file_error(void);
void do_layout_ascrap_file_error(void);
void do_lb_composite_error(void);
void do_lb_convert_error(void);
boolean do_set_rename_old_layouts_warning(const char *new_set);
boolean do_layout_alter_frames_warning(void);
boolean do_layout_alter_audio_warning(void);
void do_no_sets_dialog(const char *dir);
boolean findex_bk_dialog(const char *fname_back);
boolean paste_enough_dlg(int lframe);
boolean do_yuv4m_open_warning(void);
void do_mt_undo_mem_error(void);
void do_mt_undo_buf_error(void);
void do_mt_set_mem_error(boolean has_mt);
void do_mt_audchan_error(int warn_mask);
void do_mt_no_audchan_error(void);
void do_mt_no_jack_error(int warn_mask);
boolean do_mt_rect_prompt(void);
void do_audrate_error_dialog(void);
int do_audexport_dlg(int arps, int arate);
boolean do_event_list_warning(void);
void do_nojack_rec_error(void);
void do_vpp_palette_error(void);
void do_vpp_fps_error(void);
void do_decoder_palette_error(void);
void do_rmem_max_error(int size);
boolean do_gamma_import_warn(uint64_t fv, int gamma_type);
boolean do_mt_lb_warn(boolean lb);

void do_no_decoder_error(const char *fname);
void do_no_loadfile_error(const char *fname);

#ifdef ENABLE_JACK
boolean do_jack_no_startup_warn(boolean is_trans);
void do_jack_setup_warn(void);
boolean do_jack_no_connect_warn(boolean is_trans);
void do_jack_restart_warn(int suggest_opts, const char *srvname);
#endif

void do_encoder_img_fmt_error(render_details *rdet);

void do_after_crash_warning(void);
void do_after_invalid_warning(void);

void do_bad_layout_error(void);
void do_card_in_use_error(void);
void do_dev_busy_error(const char *devstr);

boolean do_existing_subs_warning(void);
void do_invalid_subs_error(void);
boolean do_erase_subs_warning(void);
boolean do_sub_type_warning(const char *ext, const char *type_ext);

void do_no_in_vdevs_error(void);
void do_locked_in_vdevs_error(void);

boolean do_foundclips_query(void);

void do_no_autolives_error(void);
void do_autolives_needs_clips_error(void);

void do_pulse_lost_conn_error(void);
void do_jack_lost_conn_error(void);

void do_cd_error_dialog(void);

void do_bad_theme_error(const char *themefile);
void do_bad_theme_import_error(const char *theme_file);
boolean do_theme_exists_warn(const char *themename);
boolean do_layout_recover_dialog(void);

#endif
