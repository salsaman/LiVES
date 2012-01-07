// widget-helper.h
// LiVES
// (c) G. Finch 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_WIDGET_HELPER_H
#define HAS_LIVES_WIDGET_HELPER_H

#ifdef GUI_GTK
typedef GtkObject                         LiVESObject;
typedef GtkWidget                         LiVESWidget;
typedef GtkDialog                         LiVESDialog;
typedef GtkBox                            LiVESBox;

typedef gboolean                          boolean;
typedef GList                             LiVESList;
typedef GSList                            LiVESSList;
#endif


// basic functions (wrappers for Toolkit functions)

LiVESWidget *lives_dialog_get_content_area(LiVESDialog *dialog);




// compound functions (composed of basic functions)


LiVESWidget *lives_standard_check_button_new(const char *label, boolean use_mnemonic, LiVESBox *box, const char *tooltip);
LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup, 
					     LiVESBox *box, const char *tooltips);
LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min, 
					    double max, double step, double page, int dp, LiVESBox *box, 
					    const char *tooltip);
LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *box, 
				       const char *tooltip);

#endif

