// rfx-builder.h
// LiVES
// (c) G. Finch 2004 - 2009 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#define RFX_BUILDER "build-lives-rfx-plugin"

/// must match whatever is in RFX_BUILDER
#define RFX_VERSION "1.7"

/// this definition must match with smogrify
#define RFX_BUILDER_MULTI "build-lives-rfx-plugin-multi"

/// default script extension when saving
#define RFXBUILDER_SCRIPT_SUFFIX "script"

/// length in chars of G_MAXFLOAT (for display only)
#define MAXFLOATLEN 11

/// length in chars of G_MAXINT (for display only)
#define MAXINTLEN strlen(g_strdup_printf("%d",G_MAXINT))

// advanced menu entries
void on_new_rfx_activate (GtkMenuItem *, gpointer status);
void on_edit_rfx_activate (GtkMenuItem *, gpointer status);
void on_copy_rfx_activate (GtkMenuItem *, gpointer);
void on_rename_rfx_activate (GtkMenuItem *, gpointer);
void on_delete_rfx_activate (GtkMenuItem *, gpointer status);
void on_rebuild_rfx_activate (GtkMenuItem *, gpointer);
void on_promote_rfx_activate (GtkMenuItem *, gpointer);
void on_import_rfx_activate (GtkMenuItem *, gpointer status);
void on_export_rfx_activate (GtkMenuItem *, gpointer status);

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


#define PREF_RFXDIALOG_W 800
#define PREF_RFXDIALOG_H 650


/// maximum decimal places allowed (should correspond to precision of a "float")
#define RFXBUILD_MAX_DP 16
typedef struct {
  gint when;
  gchar *code;
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
  RFX_BUILDER_MODE_NEW=0,
  RFX_BUILDER_MODE_EDIT,
  RFX_BUILDER_MODE_COPY
} lives_rfx_builder_mode_t;



typedef struct {
  lives_rfx_build_type_t type;

  GtkWidget *dialog;
  GtkWidget *name_entry;
  GtkWidget *spinbutton_version;
  GtkWidget *author_entry;
  GtkWidget *url_entry;
  GtkWidget *menu_text_entry;
  GtkWidget *action_desc_label;
  GtkWidget *action_desc_entry;
  GtkWidget *action_desc_hsep;
  GtkWidget *spinbutton_min_frames;
  GtkWidget *type_effect1_radiobutton;
  GtkWidget *type_effect2_radiobutton;
  GtkWidget *type_effect0_radiobutton;
  GtkWidget *type_tool_radiobutton;
  GtkWidget *type_utility_radiobutton;
  GtkWidget *langc_combo;
  GtkWidget *table;
  GtkWidget *requirements_button;
  GtkWidget *properties_button;
  GtkWidget *params_button;
  GtkWidget *param_window_button;
  GtkWidget *pre_button;
  GtkWidget *loop_button;
  GtkWidget *post_button;
  GtkWidget *trigger_button;
  GtkWidget *prop_slow;
  GtkWidget *prop_batchg;
  GtkWidget *hbox_batchg;
  GtkWidget *min_frames_label;

  lives_rfx_table_type_t table_type;

  gint table_rows;
  gint ptable_rows;

  lives_rfx_code_type_t codetype;

  GtkWidget *entry[RFXBUILD_MAX_TROWS];
  GtkWidget *entry2[RFXBUILD_MAX_TROWS];
  GtkWidget *entry3[RFXBUILD_MAX_TROWS];
  GtkWidget *param_dialog;
  GtkWidget *param_name_entry;
  GtkWidget *param_label_entry;
  GtkWidget *param_type_combo;
  GtkWidget *param_dp_label;
  GtkWidget *param_def_label;
  GtkWidget *param_min_label;
  GtkWidget *param_max_label;
  GtkWidget *param_step_label;
  GtkWidget *param_wrap_hbox;
  GtkWidget *param_wrap_checkbutton;
  GtkWidget *param_strlist_hbox;
  GtkWidget *param_def_combo;
  GtkWidget *paramw_rest_entry;
  GtkWidget *paramw_kw_combo;
  GtkWidget *paramw_sp_combo;
  GtkWidget *paramw_spsub_combo;
  GtkWidget *paramw_rest_label;
  GtkWidget *bg_label;
  GtkWidget *param_strdef_button;
  GtkWidget *trigger_when_entry;
  GtkWidget *spinbutton_param_dp;
  GtkWidget *spinbutton_param_group;
  GtkWidget *spinbutton_param_def;
  GtkWidget *spinbutton_param_min;
  GtkWidget *spinbutton_param_max;
  GtkWidget *spinbutton_param_step;
  GtkWidget *code_textview;
  GtkWidget *move_up_button;
  GtkWidget *move_down_button;

  guint32 props;

  gchar *pre_code;
  gchar *loop_code;
  gchar *post_code;

  gint edit_param;

  gchar *reqs[RFXBUILD_MAX_REQ];
  gint num_reqs;
  gint onum_reqs;

  lives_param_t *params;   ///< store our parameters
  lives_param_t *copy_params;   ///< store our parameters while editing
  gint num_params; ///< upper limit is RFXBUILD_MAX_PARAMS-1
  gint onum_params;

  gchar *paramw_hints[RFXBUILD_MAX_PARAMS];
  gint num_paramw_hints;  ///< upper limit is RFXBUILD_MAX_PARAMW_HINTS-1
  gint onum_paramw_hints;

  rfx_trigger_t *triggers;
  rfx_trigger_t *copy_triggers; ///< store triggers while editing
  gint num_triggers;   ///< upper limit is RFXBUILD_MAX_PARAMS, 0 == init
  gint onum_triggers;

  gboolean has_init_trigger;

  gchar *field_delim;

  lives_rfx_builder_mode_t mode;

  gint table_swap_row1;
  gint table_swap_row2;

  gchar *script_name;
  gchar *oname;

  gulong min_spin_f;
  gulong max_spin_f;
  gulong def_spin_f;
  gulong step_spin_f;

  gchar *rfx_version;
  gint plugin_version;

} rfx_build_window_t;

// builder window widgets/callbacks
rfx_build_window_t *make_rfx_build_window (const gchar *in_script_name, lives_rfx_status_t in_status);
void on_rfxbuilder_ok (GtkButton *, gpointer);
void on_rfxbuilder_cancel (GtkButton *, gpointer);
void rfxbuilder_destroy (rfx_build_window_t *);
void on_list_table_clicked (GtkButton *, gpointer rfxbuilder);
void on_requirements_ok (GtkButton *, gpointer rfxbuilder);
void on_requirements_cancel (GtkButton *, gpointer);
void on_properties_clicked (GtkButton *, gpointer rfxbuilder);
void on_properties_ok (GtkButton *, gpointer rfxbuilder);
void on_properties_cancel (GtkButton *, gpointer);
void on_params_ok (GtkButton *, gpointer rfxbuilder);
void on_params_cancel (GtkButton *, gpointer);
void on_param_window_ok (GtkButton *, gpointer rfxbuilder);
void on_param_window_cancel (GtkButton *, gpointer);
void on_code_clicked (GtkButton *, gpointer rfxbuilder);
void on_code_ok (GtkButton *, gpointer rfxbuilder);
void on_code_cancel (GtkButton *, gpointer);
void on_triggers_ok (GtkButton *, gpointer rfxbuilder);
void on_triggers_cancel (GtkButton *, gpointer);




GtkWidget * make_param_dialog (gint pnum, rfx_build_window_t *rfxbuilder);
GtkWidget * make_param_window_dialog (gint hnum, rfx_build_window_t *rfxbuilder);
GtkWidget * make_trigger_dialog (gint tnum, rfx_build_window_t *rfxbuilder);

void on_table_add_row (GtkButton *, gpointer rfxbuilder);
void on_table_edit_row (GtkButton *, gpointer rfxbuilder);
void on_table_swap_row (GtkButton *, gpointer rfxbuilder);
void on_table_delete_row (GtkButton *, gpointer rfxbuilder);

void param_set_from_dialog (lives_param_t *copy_param, rfx_build_window_t *rfxbuilder);

void after_param_dp_changed (GtkSpinButton *, gpointer rfxbuilder);
void after_param_min_changed (GtkSpinButton *, gpointer rfxbuilder);
void after_param_max_changed (GtkSpinButton *, gpointer rfxbuilder);
void after_param_def_changed (GtkSpinButton *, gpointer rfxbuilder);
void after_rfxbuilder_type_toggled (GtkToggleButton *, gpointer rfxbuilder);
void on_param_type_changed (GtkComboBox *, gpointer rfxbuilder);
void on_paramw_kw_changed (GtkComboBox *, gpointer rfxbuilder);
void on_paramw_sp_changed (GtkComboBox *, gpointer rfxbuilder);
void on_paramw_spsub_changed (GtkComboBox *, gpointer rfxbuilder);
void populate_script_combo(GtkComboBox *script_combo, lives_rfx_status_t status);
void on_script_status_changed (GtkComboBox *status_combo, gpointer script_combo);

// fileselectors
void on_export_rfx_ok (GtkButton *, gchar *script_name);
void on_import_rfx_ok (GtkButton *, gpointer status);

/// add dynamic menu entries
void add_rfx_effects(void);

// utility functions
gchar *prompt_for_script_name (const gchar *sname, lives_rfx_status_t status);
gboolean check_builder_programs (void);
GList *get_script_list (gshort status);

gboolean perform_rfxbuilder_checks (rfx_build_window_t *);
gboolean perform_param_checks (rfx_build_window_t *, gint index, gint rows);


// read/write script files
gboolean rfxbuilder_to_script (rfx_build_window_t *);
gboolean script_to_rfxbuilder (rfx_build_window_t *, const gchar *script_file);

GList *get_script_section (const gchar *section, const gchar *script_file, gboolean strip);

