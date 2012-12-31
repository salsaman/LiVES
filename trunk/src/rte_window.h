// rte_window.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2010
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

void on_assign_rte_keys_activate (GtkMenuItem *, gpointer);
void on_rte_info_clicked (GtkButton *, gpointer data);
void load_default_keymap(void);
void rtew_set_keych (gint key, gboolean on);
void rtew_set_keygr (gint key);
void rtew_set_mode_radio (gint key, gint mode);
void rtew_set_grab_button (gboolean on);
void redraw_pwindow (gint key, gint mode);
void restore_pwindow (lives_rfx_t *);
void update_pwindow (gint key, gint i, GList *list);

GtkWidget *rte_window;


void rte_set_defs_activate (GtkMenuItem *, gpointer user_data);
void rte_set_defs_cancel (GtkButton *, lives_rfx_t *);
void rte_set_defs_ok (GtkButton *, lives_rfx_t *);
void rte_reset_defs_clicked (GtkButton *, lives_rfx_t *rfx);
void rte_set_key_defs (GtkButton *, lives_rfx_t *);
void on_save_rte_defs_activate (GtkMenuItem *, gpointer);
gboolean on_clear_all_clicked (GtkButton *, gpointer user_data);
