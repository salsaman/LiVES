// rfx-builder.h
// LiVES
// (c) G. Finch 2004 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_RFX_BUILDER_H
#define HAS_LIVES_RFX_BUILDER_H

#define EXEC_RFX_BUILDER "build-lives-rfx-plugin"

/// must match whatever is in RFX_BUILDER
#define RFX_VERSION "1.8.5"

/// this definition must match with smogrify
#define EXEC_RFX_BUILDER_MULTI "build-lives-rfx-plugin-multi"

/// default script extension when saving
#define RFXBUILDER_SCRIPT_SUFFIX "script"

/// length in chars of G_MAXFLOAT (for display only)
#define MAXFLOATLEN 11

/// length in chars of G_MAXINT (for display only)
#define MAXINTLEN strlen(lives_strdup_printf("%d", LIVES_MAXINT))

// advanced menu entries
void on_new_rfx_activate(LiVESMenuItem *, livespointer status);
void on_edit_rfx_activate(LiVESMenuItem *, livespointer status);
void on_copy_rfx_activate(LiVESMenuItem *, livespointer);
void on_rename_rfx_activate(LiVESMenuItem *, livespointer);
void on_delete_rfx_activate(LiVESMenuItem *, livespointer status);
void on_rebuild_rfx_activate(LiVESMenuItem *, livespointer);
void on_promote_rfx_activate(LiVESMenuItem *, livespointer);
void on_import_rfx_activate(LiVESMenuItem *, livespointer status);
void on_export_rfx_activate(LiVESMenuItem *, livespointer status);

// the builder window

/// maximum requirements
#define RFXBUILD_MAX_REQ 128

/// maximum parameters
#define RFXBUILD_MAX_PARAMS 32

/// maximum parameter window hints
#define RFXBUILD_MAX_PARAMW_HINTS 512

/// max table rows :
/// this should be set to the largest of the previous maxima
#define RFXBUILD_MAX_TROWS 512

#define PREF_RFXDIALOG_W ((int)(720.*widget_opts.scaleW))
#define PREF_RFXDIALOG_H ((int)(650.*widget_opts.scaleH))

#define RFX_LOADED (mainw->helper_procthreads[PT_LAZY_RFX] ?		\
		    lives_proc_thread_check_finished(mainw->helper_procthreads[PT_LAZY_RFX]) : FALSE)

/// maximum decimal places allowed (should correspond to precision of a "float")
#define RFXBUILD_MAX_DP 16
typedef struct {
  int when;
  char *code;
} rfx_trigger_t;

typedef enum {
  RFX_BUILD_TYPE_EFFECT1,
  RFX_BUILD_TYPE_EFFECT2,
  RFX_BUILD_TYPE_EFFECT0,
  RFX_BUILD_TYPE_TOOL,
  RFX_BUILD_TYPE_UTILITY
} lives_rfx_build_type_t;

typedef enum {
  RFX_TABLE_TYPE_REQUIREMENTS,
  RFX_TABLE_TYPE_PARAMS,
  RFX_TABLE_TYPE_TRIGGERS,
  RFX_TABLE_TYPE_PARAM_WINDOW
} lives_rfx_table_type_t;

typedef enum {
  RFX_CODE_TYPE_PRE,
  RFX_CODE_TYPE_LOOP,
  RFX_CODE_TYPE_POST,
  RFX_CODE_TYPE_STRDEF,
  RFX_CODE_TYPE_STRING_LIST
} lives_rfx_code_type_t;

typedef enum {
  RFX_BUILDER_MODE_NEW = 0,
  RFX_BUILDER_MODE_EDIT,
  RFX_BUILDER_MODE_COPY
} lives_rfx_builder_mode_t;

typedef struct {
  lives_rfx_build_type_t type;

  LiVESWidget *dialog;
  LiVESWidget *name_entry;
  LiVESWidget *spinbutton_version;
  LiVESWidget *author_entry;
  LiVESWidget *url_entry;
  LiVESWidget *menu_text_entry;
  LiVESWidget *action_desc_hbox;
  LiVESWidget *action_desc_entry;
  LiVESWidget *action_desc_hsep;
  LiVESWidget *spinbutton_min_frames;
  LiVESWidget *type_effect1_radiobutton;
  LiVESWidget *type_effect2_radiobutton;
  LiVESWidget *type_effect0_radiobutton;
  LiVESWidget *type_tool_radiobutton;
  LiVESWidget *type_utility_radiobutton;
  LiVESWidget *langc_combo;
  LiVESWidget *table;
  LiVESWidget *requirements_button;
  LiVESWidget *properties_button;
  LiVESWidget *params_button;
  LiVESWidget *param_window_button;
  LiVESWidget *pre_button;
  LiVESWidget *loop_button;
  LiVESWidget *post_button;
  LiVESWidget *trigger_button;
  LiVESWidget *prop_slow;
  LiVESWidget *prop_noprv;
  LiVESWidget *prop_batchg;
  LiVESWidget *hbox_batchg;
  LiVESWidget *min_frames_label;

  lives_rfx_table_type_t table_type;

  int table_rows;

  lives_rfx_code_type_t codetype;

  LiVESWidget *entry[RFXBUILD_MAX_TROWS];
  LiVESWidget *entry2[RFXBUILD_MAX_TROWS];
  LiVESWidget *entry3[RFXBUILD_MAX_TROWS];
  LiVESWidget *param_dialog;
  LiVESWidget *param_name_entry;
  LiVESWidget *param_label_entry;
  LiVESWidget *param_type_combo;
  LiVESWidget *param_dp_label;
  LiVESWidget *param_def_label;
  LiVESWidget *param_min_label;
  LiVESWidget *param_max_label;
  LiVESWidget *param_step_label;
  LiVESWidget *param_wrap_hbox;
  LiVESWidget *param_wrap_checkbutton;
  LiVESWidget *param_strlist_hbox;
  LiVESWidget *param_def_combo;
  LiVESWidget *paramw_rest_entry;
  LiVESWidget *paramw_kw_combo;
  LiVESWidget *paramw_sp_combo;
  LiVESWidget *paramw_spsub_combo;
  LiVESWidget *paramw_rest_label;
  LiVESWidget *hbox_bg;
  LiVESWidget *param_strdef_button;
  LiVESWidget *param_strdef_hbox;
  LiVESWidget *trigger_when_entry;
  LiVESWidget *spinbutton_param_dp;
  LiVESWidget *spinbutton_param_group;
  LiVESWidget *spinbutton_param_def;
  LiVESWidget *spinbutton_param_min;
  LiVESWidget *spinbutton_param_max;
  LiVESWidget *spinbutton_param_step;
  LiVESWidget *code_textview;
  LiVESWidget *new_entry_button;
  LiVESWidget *edit_entry_button;
  LiVESWidget *remove_entry_button;
  LiVESWidget *move_up_button;
  LiVESWidget *move_down_button;

  uint32_t props;

  char *pre_code;
  char *loop_code;
  char *post_code;

  int edit_param;

  char *reqs[RFXBUILD_MAX_REQ];
  int num_reqs;
  int onum_reqs;

  lives_param_t *params;   ///< store our parameters
  lives_param_t *copy_params;   ///< store our parameters while editing
  int num_params; ///< upper limit is RFXBUILD_MAX_PARAMS-1
  int onum_params;

  char *paramw_hints[RFXBUILD_MAX_PARAMS];
  int num_paramw_hints;  ///< upper limit is RFXBUILD_MAX_PARAMW_HINTS-1
  int onum_paramw_hints;

  rfx_trigger_t *triggers;
  rfx_trigger_t *copy_triggers; ///< store triggers while editing
  int num_triggers;   ///< upper limit is RFXBUILD_MAX_PARAMS, 0 == init
  int onum_triggers;

  boolean has_init_trigger;

  char *field_delim;

  lives_rfx_builder_mode_t mode;

  int table_swap_row1;
  int table_swap_row2;

  char *script_name;
  char *oname;

  ulong min_spin_f;
  ulong max_spin_f;
  ulong def_spin_f;
  ulong step_spin_f;

  char *rfx_version;
  int plugin_version;
} rfx_build_window_t;

// fileselectors
void on_export_rfx_ok(LiVESButton *, char *script_name);
void on_import_rfx_ok(LiVESButton *, livespointer status);

/// add dynamic menu entries
boolean add_rfx_effects(lives_rfx_status_t status);
void add_rfx_effects2(lives_rfx_status_t status);
void update_rfx_menus(void);

// utility functions
char *prompt_for_script_name(const char *sname, lives_rfx_status_t status);
boolean check_builder_programs(void);
LiVESList *get_script_list(lives_rfx_status_t status);

boolean perform_rfxbuilder_checks(rfx_build_window_t *);
boolean perform_param_checks(rfx_build_window_t *, int index, int rows);

// read/write script files
boolean rfxbuilder_to_script(rfx_build_window_t *);
boolean script_to_rfxbuilder(rfx_build_window_t *, const char *script_file);

LiVESList *get_script_section(const char *section, const char *script_file, boolean strip);

#endif // HAS_LIVES_RFX_BUILDER_H
