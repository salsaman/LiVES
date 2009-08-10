// effects.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2007
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "../libweed/weed.h"
#include "effects-weed.h"

gboolean do_effect(lives_rfx_t *rfx, gboolean is_preview); // defined as extern in paramwindow.c

void on_render_fx_activate (GtkMenuItem *menuitem, lives_rfx_t *rfx);

///////////////// real time effects

// render
void on_realfx_activate (GtkMenuItem *, gpointer rfx);
gboolean on_realfx_activate_inner(gint type, lives_rfx_t *rfx);

gint  realfx_progress (gboolean reset);

// key callbacks

gboolean textparm_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data);

gboolean grabkeys_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer user_data); // for accel groups
gboolean grabkeys_callback_hook (GtkToggleButton *button, gpointer user_data); // for widgets

gboolean rte_on_off_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer user_data); // for accel groups
gboolean rte_on_off_callback_hook (GtkToggleButton *, gpointer user_data); // for widgets

gboolean rtemode_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer user_data); // for accel groups
gboolean rtemode_callback_hook (GtkToggleButton *, gpointer user_data); // for widgets

gboolean swap_fg_bg_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer user_data);

weed_plant_t *get_blend_layer(weed_timecode_t tc);

weed_plant_t *on_rte_apply (weed_plant_t *main_layer, int opwidth, int opheight, weed_timecode_t tc);


void deinterlace_frame(weed_plant_t *layer, weed_timecode_t tc);
