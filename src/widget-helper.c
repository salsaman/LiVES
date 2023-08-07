// widget-helper.c
// LiVES
// (c) G. Finch 2012 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#define NEED_DEF_WIDGET_OPTS

#include "main.h"
#include "startup.h"
#include "functions.h"
#include "callbacks.h"

// max number of GUI events we process per update loop: 64 - 256 seems about right
#define EV_LIM 64.
// how many do nothing cycles before switch to low prio
// - should result in 1.0 - 0.1 second delay
#define MISS_PRIO_THRESH 100000
// nanosec to wait in loops - a value of about 500 seems to be optimal
#define NSLEEP_TIME 500
// how much longer to wait in low prio mode, multilpier (sLo_FACTOR + 1)
// gui_tight mode
#define sLO_FACTOR 8
// usec per cycle in low prio mode, absolute, gui slack mode
#define LO_WAIT usleep(2048);
// EV_LIM multipler when expecing large updates
#define MUCH_EV_MPY 2.

#define PRIO_HIGH LIVES_WIDGET_PRIORITY_HIGH
#define PRIO_LOW LIVES_WIDGET_PRIORITY_LOW

static volatile boolean gui_loop_tight = FALSE;

// this is set when actioning: lives_widget_context_iteration, when in _dialog_run, and
static volatile int cprio = PRIO_HIGH;
static pthread_mutex_t lpt_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile lives_proc_thread_t lpttorun = NULL;

static boolean _lives_widget_context_update(void);
static boolean _lives_widget_process_updates(LiVESWidget *);

extern boolean all_config_deferrable(LiVESWidget *, LiVESXEventConfigure *event, livespointer ppsurf);
extern boolean all_expose(LiVESWidget *, lives_painter_t *, livespointer psurf);

static boolean _lives_standard_button_set_label(LiVESButton *, const char *txt);
static void lives_widget_show_all_cb(LiVESWidget *other, livespointer user_data);

static void set_child_colour_internal(LiVESWidget *, livespointer set_allx);
static void set_child_alt_colour_internal(LiVESWidget *, livespointer set_allx);
static void async_sig_handler(livespointer instance, livespointer data);

typedef void (*bifunc)(livespointer, livespointer);
typedef boolean(*trifunc)(livespointer, livespointer, livespointer);

static boolean show_css = FALSE;

#ifdef GUI_GTK
boolean set_css_value_direct(LiVESWidget *, LiVESWidgetState state, const char *selector,
                             const char *detail, const char *value);
#endif

/// internal data keys
#define STD_KEY "_wh_is_standard"
#define EXCL_KEY "_wh_excl"
#define BACCL_GROUP_KEY "_wh_baccl_group"
#define BACCL_ACCL_KEY "_wh_baccl_accl"
#define TTIPS_KEY "_wh_lives_tooltips"
#define TTIPS_OVERRIDE_KEY "_wh_lives_tooltips_override"
#define TTIPS_HIDE_KEY "_wh_lives_tooltips_hide"
#define HAS_TTIPS_IMAGE_KEY "_wh_has_ttips_image"
#define TTIPS_IMAGE_KEY "_wh_ttips_image"
#define WARN_IMAGE_KEY "_wh_warn_image"
#define SHOWALL_OVERRIDE_KEY "_wh_lives_showall_override"
#define SHOWHIDE_CONTROLLER_KEY "_wh_lives_showhide_controller"
#define SUBMENU_INS_KEY "_wh_submenu_ins"
#define ROWS_KEY "_wh_rows"
#define COLS_KEY "_wh_cols"
#define CDEF_KEY "_wh_current_default"
#define DEFBUTTON_KEY "_wh_default_button"
#define DEFOVERRIDE_KEY "_wh_default_override"
#define EXP_LIST_KEY "_wh_expansion_list"
#define LROW_KEY "_wh_layout_row"
#define EXPANSION_KEY "_wh_expansion"
#define JUST_KEY "_wh_justification"
#define WADDED_KEY "_wh_widgets_added"
#define NWIDTH_KEY "_wh_normal_width"
#define FBUTT_KEY "_wh_first_button"
#define SNAPVAL_KEY "_wh_snap_to_ticks"
#define ISLOCKED_KEY "_wh_is_locked"
#define CBUTTON_KEY "_wh_cbutton"
#define SPRED_KEY "_wh_sp_red"
#define SPGREEN_KEY "_wh_sp_green"
#define SPBLUE_KEY "_wh_sp_blue"
#define SPALPHA_KEY "_wh_sp_alpha"
#define THEME_KEY "_wh_theme"
#define COND_PLANT_KEY "_wh_cond_plant"

#define RESPONSE_KEY "_wh_response"
#define DESTROYED_KEY "_wh_destroyed"
#define ACTION_AREA_KEY "_wh_act_area"

#define SBUTT_TXT_KEY "_sbutt_txt"
#define SBUTT_LAYOUT_KEY "_sbutt_layout"
#define SBUTT_LW_KEY "_sbutt_lw"
#define SBUTT_LH_KEY "_sbutt_lh"
#define SBUTT_PIXBUF_KEY "_sbutt_pixbuf"
#define SBUTT_FORCEIMG_KEY "_sbutt_forceimg"
#define SBUTT_FAKEDEF_KEY "_sbutt_fakedef"

#define EXPANDER_TEXT_KEY "_exp_txt"
#define EXPANDER_XTEXT_KEY "_exp_alt_txt"
#define EXPANDER_XLABEL_KEY "_exp_expand_label"

static LiVESWindow *modalw = NULL;

#if 0
weed_plant_t *LiVESWidgetObject_to_weed_plant(LiVESWidgetObject *o) {
  int nprops;
  GParamSpec **pspec;
  GObjectClass oclass;
  weed_plant_t *plant;

  if (!o || !G_IS_OBJECT(o)) return NULL;

  plant = lives_plant_new(LIVES_WEED_SUBTYPE_WIDGET);

  oclass = G_OBJECT_GET_CLASS(o);

  // get all the properties
  pspec = g_object_class_list_properties(oclass, &nprops);
  // also g_object_interface_list_properties

  if (nprops > 0) {
    GType gtype;
    weed_plant_t **params = (weed_plant_t **)lives_malloc(nprops * sizeof(weed_plant_t *));
    for (i = 0; i < nprops; i++) {
      // check pspec[i]->flags (G_PARAM_READABLE, G_PARAM_WRITABLE...)
      // if (!G_PARAM_EXPLICIT_NOTIFY), we can hook to the notify signal to shadow it
      //

      params[i] = weed_plant_new(WEED_PLANT_PARAMETER);
      weed_set_string_value(params[i], WEED_LEAF_NAME, g_param_spec_get_name(pspec[i]));
      gtype = G_PARAM_SPEC_VALUE_TYPE(pspec[i]);
      switch (gtype) {
      case G_TYPE_STRING:
      case G_TYPE_CHAR:
      case G_TYPE_UCHAR:
        // WEED_SEED_STRING
        break;
      case G_TYPE_FLOAT:
      case G_TYPE_DOUBLE:
        // WEED_SEED_DOUBLE
        break;
      case G_TYPE_INT:
      case G_TYPE_FLAGS:
      case G_TYPE_ENUM:
      case G_TYPE_UINT: {
        int ival;
        g_object_get(o, name, &ival, NULL);
        //g_object_set(o, name, ival, NULL);
        weed_set_int_value(params[i], WEED_LEAF_VALUE, ival);
        break;
      }
      case G_TYPE_BOOLEAN:
        // WEED_SEED_BOOLEAN
        break;
      case G_TYPE_INT64:
      case G_TYPE_UINT64:
      case G_TYPE_LONG:
      case G_TYPE_ULONG:
        // WEED_SEED_INT64
        break;
      case G_TYPE_POINTER:
        // WEED_SEED_VOIDPTR
        break;
      default:
        break;
      }
    }
    params[num_params] = NULL;
    weed_set_plantptr_array(plant, WEED_LEAF_IN_PARAMETERS, nprops, params);
  }
}
#endif


WIDGET_HELPER_GLOBAL_INLINE boolean is_standard_widget(LiVESWidget *widget) {
  livespointer val;
  if (!(val = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), STD_KEY))) return FALSE;
  return (LIVES_POINTER_TO_INT(val));
}

WIDGET_HELPER_LOCAL_INLINE void set_standard_widget(LiVESWidget *widget, boolean is) {
  widget_opts.last_widget = widget;
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), STD_KEY, LIVES_INT_TO_POINTER(is));
}


static void edit_state_cb(LiVESWidgetObject *object, livespointer pspec, livespointer user_data) {
  LiVESWidget *entry = LIVES_WIDGET(object);
  if (lives_entry_get_editable(LIVES_ENTRY(object))) {
    lives_widget_apply_theme3(entry, LIVES_WIDGET_STATE_NORMAL);
  } else {
    lives_widget_apply_theme2(entry, LIVES_WIDGET_STATE_NORMAL, TRUE);
  }
}


#if !GTK_CHECK_VERSION(3, 16, 0)
static boolean widget_state_cb(LiVESWidgetObject *object, livespointer pspec, livespointer user_data) {
  // This callback is here because:
  //
  // a) cannot alter the text colour of a button after the initial draw of a button
  // this is because it doesn't have a proper label widget
  // we can only change the background colour, so here we change the border colour via updating the parent container

  // note: if we need a button with changeable text colour we must use a toolbar button instead !
  //
  // b) CSS appears broken in gtk+ 3.18.9 and possibly other versions, preventing setting of colours for
  // non-default states (e.g. insensitive)
  // thus we need to set a callback to listen to "sensitive" changes, and update the colours in response
  //
  // c) it is also easier just to set the CSS colours when the widget state changes than to figure out ahead of time
  //     what the colours should be for each state. Hopefully it doesn't add too much overhead listening for sensitivity
  //     changes and then updating the CSS manually.
  //
  LiVESWidget *widget = (LiVESWidget *)object;
  LiVESWidgetState state;
  int woat = widget_opts.apply_theme;

  if (LIVES_IS_PLAYING || !mainw->is_ready) return FALSE;

  widget_opts.apply_theme = 1;

  state = lives_widget_get_state(widget);

  if (LIVES_IS_TOOL_BUTTON(widget)) {
    LiVESWidget *label;
    LiVESWidget *icon = gtk_tool_button_get_icon_widget(LIVES_TOOL_BUTTON(widget));
    if (icon) {
      // if we have an icon (no label) just update the border
      lives_tool_button_set_border_color(widget, state, &palette->menu_and_bars);
      widget_opts.apply_theme = woat;
      return FALSE;
    }
    label = gtk_tool_button_get_label_widget(LIVES_TOOL_BUTTON(widget));
    if (label) {
      float dimval;
      LiVESWidgetColor dimmed_fg;
      LiVESList *list, *olist;
      // if we have a label we CAN set the text colours for TOOL_buttons
      // as well as the outline colour
      if (!lives_widget_is_sensitive(widget)) {
        dimval = (0.2 * 65535.);
        lives_widget_color_copy(&dimmed_fg, &palette->normal_fore);
        lives_widget_color_mix(&dimmed_fg, &palette->normal_back, (float)dimval / 65535.);
        lives_tool_button_set_border_color(widget, state, &dimmed_fg);
        lives_widget_apply_theme_dimmed2(label, state, BUTTON_DIM_VAL);
      } else {
        dimval = (0.6 * 65535.);
        lives_widget_color_copy(&dimmed_fg, &palette->normal_fore);
        lives_widget_color_mix(&dimmed_fg, &palette->normal_back, (float)dimval / 65535.);
        lives_tool_button_set_border_color(widget, state, &dimmed_fg);
        lives_widget_apply_theme2(label, state, TRUE);
      }
      // menutoolbuttons will also have an arrow
      // since CSS selectors are borked we have to find it by brute force
      olist = list = lives_container_get_children(LIVES_CONTAINER(widget));
      while (list) {
        widget = (LiVESWidget *)list->data;
        if (LIVES_IS_VBOX(widget)) {
          lives_widget_set_bg_color(widget, state, &palette->menu_and_bars);
        }
        list = list->next;
      }
      lives_list_free(olist);
      widget_opts.apply_theme = woat;
      return FALSE;
    }
  }

  if (LIVES_IS_LABEL(widget)) {
    // other widgets get dimmed text
    int themetype = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), THEME_KEY));
    if (themetype == 2) {
      if (!lives_widget_is_sensitive(widget)) {
        set_child_dimmed_colour2(widget, BUTTON_DIM_VAL); // insens, themecols 1, child only
      } else set_child_alt_colour(widget, TRUE);
    } else {
      if (!lives_widget_is_sensitive(widget)) {
        set_child_dimmed_colour(widget, BUTTON_DIM_VAL); // insens, themecols 1, child only
      } else set_child_colour(widget, TRUE);
    }
  }

  if (LIVES_IS_ENTRY(widget) || LIVES_IS_COMBO(widget)) {
    // other widgets get dimmed text
    if (!lives_widget_is_sensitive(widget)) {
      set_child_dimmed_colour2(widget, BUTTON_DIM_VAL); // insens, themecols 1, child only
      lives_widget_apply_theme_dimmed2(widget, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    } else {
      if (LIVES_IS_ENTRY(widget) && !LIVES_IS_SPIN_BUTTON(widget))
        edit_state_cb(LIVES_WIDGET_OBJECT(widget), NULL, NULL);
      else {
        if (LIVES_IS_COMBO(widget)) {
          LiVESWidget *entry = lives_combo_get_entry(LIVES_COMBO(widget));
          lives_widget_apply_theme2(entry, LIVES_WIDGET_STATE_NORMAL, TRUE);
          lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_NORMAL, TRUE);
        } else set_child_colour(widget, TRUE);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
  widget_opts.apply_theme = woat;
  return FALSE;
}
#endif


WIDGET_HELPER_GLOBAL_INLINE void lives_widget_object_set_data_auto(LiVESWidgetObject * obj, const char *key,
    livespointer data) {
  // free data on obj destroy
  lives_widget_object_set_data_full(obj, key, data, lives_free);
}


typedef struct {
  LiVESWidgetObject *obj;
  char *key;
} lives_objkey;

static void lives_widget_nullify_objkey(LiVESWidget * widget, livespointer xobjkey) {
  lives_objkey *objkey = (lives_objkey *)xobjkey;
  if (objkey->obj) {
    lives_widget_object_set_data(objkey->obj, objkey->key, NULL);
    if (!widget_opts.no_gui) {
      lives_signal_handlers_sync_disconnect_by_func(LIVES_GUI_OBJECT(objkey->obj),
          LIVES_GUI_CALLBACK(lives_widget_show_all_cb),
          (livespointer)(widget));
    }
  }
}

WIDGET_HELPER_GLOBAL_INLINE void lives_widget_object_set_data_destroyable(LiVESWidgetObject * obj, const char *key,
    LiVESWidgetObject * widget) {
  // nullify data on target obj destroy
  lives_objkey *objkey = lives_malloc(sizeof(lives_objkey));
  objkey->obj = obj;
  objkey->key = lives_strdup(key);
  lives_widget_object_set_data(obj, key, (livespointer)widget);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_DESTROY_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_widget_nullify_objkey), objkey);
  lives_widget_nullify_with((LiVESWidget *)obj, (void **)&objkey->obj);
}


static void weed_plant_free_cb(livespointer plant) {weed_plant_free((weed_plant_t *)plant);}

WIDGET_HELPER_GLOBAL_INLINE void lives_widget_object_set_data_plantptr(LiVESWidgetObject * obj,
    const char *key, weed_plantptr_t plant) {
  lives_widget_object_set_data_full(obj, key, plant, weed_plant_free_cb);
}


static void _lives_mutex_free_cb(livespointer mutex) {
  pthread_mutex_destroy((pthread_mutex_t *)mutex);
  lives_free(mutex);
}

WIDGET_HELPER_LOCAL_INLINE void lives_widget_object_set_data_mutex(LiVESWidgetObject * obj, const char *key,
    pthread_mutex_t *mutex) {
  lives_widget_object_set_data_full(obj, key, mutex, _lives_mutex_free_cb);
}


/// needed because lives_list_free() is a macro
static void _lives_list_free_cb(livespointer list) {lives_list_free((LiVESList *)list);}

WIDGET_HELPER_GLOBAL_INLINE void lives_widget_object_set_data_list(LiVESWidgetObject * obj, const char *key, LiVESList * list) {
  lives_widget_object_set_data_full(obj, key, list, _lives_list_free_cb);
}

static void _lives_widget_object_unref_cb(livespointer obj) {lives_widget_object_unref((LiVESWidgetObject *)obj);}

WIDGET_HELPER_GLOBAL_INLINE void lives_widget_object_set_data_widget_object(LiVESWidgetObject * obj, const char *key,
    livespointer other) {
  lives_widget_object_set_data_full(obj, key, other, _lives_widget_object_unref_cb);
}


// basic functions

////////////////////////////////////////////////////
//lives_painter functions

WIDGET_HELPER_GLOBAL_INLINE lives_painter_t *lives_painter_create_from_surface(lives_painter_surface_t *target) {
  lives_painter_t *cr = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
  cr = cairo_create(target);
#endif
  return cr;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_source_pixbuf(lives_painter_t *cr, const LiVESPixbuf * pixbuf,
    double pixbuf_x,
    double pixbuf_y) {
  // blit pixbuf to cairo at x,y
#ifdef LIVES_PAINTER_IS_CAIRO
  gdk_cairo_set_source_pixbuf(cr, pixbuf, pixbuf_x, pixbuf_y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_source_surface(lives_painter_t *cr, lives_painter_surface_t *surface,
    double x,
    double y) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_source_surface(cr, surface, x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_paint(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_paint(cr);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_fill(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_fill(cr);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_stroke(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  if (!gui_loop_tight || is_fg_thread()) cairo_stroke(cr);
  else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(cairo_stroke, "v", cr);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_clip(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_clip(cr);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_destroy(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_destroy(cr);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_render_background(LiVESWidget * widget, lives_painter_t *cr, double x,
    double y, double width, double height) {
  if (!palette) return FALSE;
#ifdef LIVES_PAINTER_IS_CAIRO
  if (widget == mainw->play_image && mainw->multitrack) {
    if (prefs->dev_show_dabg)
      lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->dark_orange);
    else
      lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->black);
  } else {
    if (LIVES_IS_PLAYING && mainw->faded) {
      if (prefs->dev_show_dabg)
        lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->light_green);
      else
        lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->fade_colour);
    } else {
      if (prefs->dev_show_dabg)
        lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->dark_red);
      else
        lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->normal_back);
    }
  }
  lives_painter_rectangle(cr, x, y, width, height);
  lives_painter_fill(cr);
  //lives_widget_queue_draw(widget);
  return TRUE;
#endif /// painter cairo
  return FALSE;
}


WIDGET_HELPER_LOCAL_INLINE void lives_painter_lozenge(lives_painter_t *cr, double offs_x, double offs_y, double width,
    double height,
    double rad) {
  width += offs_x * 2;
  height += offs_y * 2;

  lives_painter_move_to(cr, rad + offs_x, offs_y);
  lives_painter_line_to(cr, width - 1. - offs_x - rad, offs_y);
  lives_painter_arc(cr, width - 1. - offs_x - rad, rad + offs_y, rad, 1.5 * M_PI, 0.);
  lives_painter_move_to(cr, width - 1. - offs_x, offs_y + rad);
  lives_painter_line_to(cr, width - 1 - offs_x, height - 6. - offs_y);

  lives_painter_arc(cr, width - 1. - offs_x - rad, height - 1. - offs_y - rad, rad, 0., M_PI / 2.);
  lives_painter_line_to(cr, offs_x + rad, height - 1. - offs_y);

  lives_painter_arc(cr, offs_x + rad, height - 1. - offs_y - rad, rad, M_PI / 2., M_PI);
  lives_painter_line_to(cr, offs_x, offs_y + rad);

  lives_painter_arc(cr, offs_x + rad, offs_y + rad, rad, M_PI, 1.5 * M_PI);
  lives_painter_line_to(cr, rad + offs_x, offs_y);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_surface_destroy(lives_painter_surface_t *surf) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_surface_destroy(surf);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_painter_surface_reference(lives_painter_surface_t *surf) {
#ifdef LIVES_PAINTER_IS_CAIRO
  return cairo_surface_reference(surf);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_new_path(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_new_path(cr);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_translate(lives_painter_t *cr, double x, double y) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_translate(cr, x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_line_width(lives_painter_t *cr, double width) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_line_width(cr, width);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_move_to(lives_painter_t *cr, double x, double y) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_move_to(cr, x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_line_to(lives_painter_t *cr, double x, double y) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_line_to(cr, x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_close_path(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_close_path(cr);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_rectangle(lives_painter_t *cr, double x, double y, double width,
    double height) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_rectangle(cr, x, y, width, height);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_arc(lives_painter_t *cr, double xc, double yc, double radius, double angle1,
    double angle2) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_arc(cr, xc, yc, radius, angle1, angle2);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_operator(lives_painter_t *cr, lives_painter_operator_t op) {
  // if op was not LIVES_PAINTER_OPERATOR_DEFAULT, and FALSE is returned, then the operation failed,
  // and op was set to the default
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_operator(cr, op);
  if (op == LIVES_PAINTER_OPERATOR_UNKNOWN) return FALSE;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_source_rgb(lives_painter_t *cr, double red, double green, double blue) {
  // r,g,b values 0.0 -> 1.0
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_source_rgb(cr, red, green, blue);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_source_rgba(lives_painter_t *cr, double red, double green, double blue,
    double alpha) {
  // r,g,b,a values 0.0 -> 1.0
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_source_rgba(cr, red, green, blue, alpha);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_fill_rule(lives_painter_t *cr, lives_painter_fill_rule_t fill_rule) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_fill_rule(cr, fill_rule);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_surface_flush(lives_painter_surface_t *surf) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_surface_flush(surf);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_surface_mark_dirty(lives_painter_surface_t *surf) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_surface_mark_dirty(surf);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_painter_image_surface_create_for_data(uint8_t *data,
    lives_painter_format_t format,
    int width, int height, int stride) {
  lives_painter_surface_t *surf = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO

  surf = cairo_image_surface_create_for_data(data, format, width, height, stride);
#endif
  return surf;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t
*lives_xwindow_create_similar_surface(LiVESXWindow * window, lives_painter_content_t cont,
                                      int width, int height) {
#if GTK_CHECK_VERSION(4, 0, 0)
  lives_painter_surface_t *surf = gdk_surface_create_similar_surface(window, cont, width, height);
#else
  lives_painter_surface_t *surf = gdk_window_create_similar_surface(window, cont, width, height);
#endif
  lives_painter_t *cr = lives_painter_create_from_surface(surf);
  lives_painter_set_source_rgb(cr, 0., 0., 0.);
  lives_painter_paint(cr);
  lives_painter_destroy(cr);
  return surf;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_widget_create_painter_surface(LiVESWidget * widget) {
  if (widget)
    return lives_xwindow_create_similar_surface(lives_widget_get_xwindow(widget),
           LIVES_PAINTER_CONTENT_COLOR,
           lives_widget_get_allocation_width(widget),
           lives_widget_get_allocation_height(widget));
  return NULL;
}

////////////////////////// painter info funcs

WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_painter_get_target(lives_painter_t *cr) {
  lives_painter_surface_t *surf = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
  surf = cairo_get_target(cr);
#endif

  return surf;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_painter_format_stride_for_width(lives_painter_format_t form, int width) {
  int stride = -1;
#ifdef LIVES_PAINTER_IS_CAIRO
  stride = cairo_format_stride_for_width(form, width);
#endif
  return stride;
}


WIDGET_HELPER_GLOBAL_INLINE uint8_t *lives_painter_image_surface_get_data(lives_painter_surface_t *surf) {
  uint8_t *data = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
  data = cairo_image_surface_get_data(surf);
#endif
  return data;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_painter_image_surface_get_width(lives_painter_surface_t *surf) {
  int width = 0;
#ifdef LIVES_PAINTER_IS_CAIRO
  width = cairo_image_surface_get_width(surf);
#endif
  return width;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_painter_image_surface_get_height(lives_painter_surface_t *surf) {
  int height = 0;
#ifdef LIVES_PAINTER_IS_CAIRO
  height = cairo_image_surface_get_height(surf);
#endif
  return height;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_painter_image_surface_get_stride(lives_painter_surface_t *surf) {
  int stride = 0;
#ifdef LIVES_PAINTER_IS_CAIRO
  stride = cairo_image_surface_get_stride(surf);
#endif
  return stride;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_format_t lives_painter_image_surface_get_format(lives_painter_surface_t *surf) {
  lives_painter_format_t format = (lives_painter_format_t)0;
#ifdef LIVES_PAINTER_IS_CAIRO
  format = cairo_image_surface_get_format(surf);
#endif
  return format;
}


////////////////////////////////////////////////////////

WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_ref(livespointer object) {
#ifdef GUI_GTK
  if (LIVES_IS_WIDGET_OBJECT(object)) g_object_ref(object);
  else {
    LIVES_WARN("Attempted ref of non-object");
    break_me("ref of nonobj");
    return FALSE;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_unref(livespointer object) {
#ifdef GUI_GTK
  if (LIVES_IS_WIDGET_OBJECT(object)) g_object_unref(object);
  else {
    LIVES_WARN("Attempted unref of non-object");
    break_me("unref of nonobj");
    return FALSE;
  }
  return TRUE;
#endif
  return FALSE;
}


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_ref_sink(livespointer object) {
  if (!LIVES_IS_WIDGET_OBJECT(object)) {
    LIVES_WARN("Attempted ref_sink of non-object");
    break_me("ref sink of nonobj");
    return FALSE;
  }
  g_object_ref_sink(object);
  return TRUE;
}
#else
WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_ref_sink(livespointer object) {
  GtkObject *gtkobject;
  if (!LIVES_IS_WIDGET_OBJECT(object)) {
    LIVES_WARN("Attempted ref_sink of non-object");
    return FALSE;
  }
  gtkobject = (GtkObject *)object;
  gtk_object_sink(gtkobject);
  return TRUE;
}
#endif
#endif


/// signal handling
static LiVESList *active_sigdets = NULL;

static void sigdata_free(livespointer data, LiVESWidgetClosure * cl) {
  lives_proc_thread_t lpt;
  lives_sigdata_t *sigdata = (lives_sigdata_t *)data;

  if (!sigdata) return;

  if (cl) active_sigdets = lives_list_remove(active_sigdets, sigdata);

  if (sigdata->instance && !sigdata->callback) {
    break_me("invalid sigdata");
  }

  lives_proc_thread_ref((lpt = sigdata->proc));

  if (sigdata->proc) {
    // must not free sigdata->proc after this
    if (sigdata->callback) {
      // do NOT for external added lpt
      lives_proc_thread_dontcare(sigdata->proc);
    }
    sigdata->proc = NULL;
    lives_proc_thread_unref(lpt);
  }

  //}
  if (sigdata->detsig) lives_free(sigdata->detsig);
  if (sigdata->instance && !sigdata->callback) {
    break_me("invalid sigdata");
  }
  lives_freep((void **)&sigdata);
}


static void async_notify_redirect_handler(LiVESWidgetObject * object, livespointer pspec,
    livespointer user_data) {
  /// async shim to convert "notify::xxx" to some other signal
  async_sig_handler(object, user_data);
}

static void notify_redirect_handler(LiVESWidgetObject * object, livespointer pspec,
                                    livespointer user_data) {
  /// shim to convert "notify::xxx" to some other signal
  lives_sigdata_t *sigdata = (lives_sigdata_t *)user_data;
  LiVESWidget *widget = (LiVESWidget *)object;
  if (!sigdata->swapped)
    (*((bifunc)sigdata->callback))(widget, sigdata->user_data);
  else
    (*((bifunc)sigdata->callback))(sigdata->user_data, widget);
}

unsigned long lives_signal_connect_sync(livespointer instance, const char *detailed_signal,
                                        LiVESGuiCallback c_handler, livespointer data,
                                        LiVESConnectFlags flags) {
  unsigned long func_id;
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_WIDGET(instance) && LIVES_IS_SWITCH(LIVES_WIDGET(instance))
      && !strcmp(detailed_signal, LIVES_WIDGET_TOGGLED_SIGNAL)) {
    /// to make switch and checkbutton interchangeable,
    /// we substitute the "toggled" signal for a switch with "notify::active"
    /// and then redirect it back to the desired callback
    lives_sigdata_t *sigdata = lives_calloc(1, sizeof(lives_sigdata_t));
    sigdata->instance = instance;
    sigdata->callback = (lives_funcptr_t)c_handler;
    sigdata->user_data = data;
    sigdata->swapped = (flags & LIVES_CONNECT_SWAPPED) ? TRUE : FALSE;
    sigdata->detsig = lives_strdup(LIVES_WIDGET_TOGGLED_SIGNAL);
    sigdata->funcid = g_signal_connect_data(instance, LIVES_WIDGET_NOTIFY_SIGNAL "active",
                                            LIVES_GUI_CALLBACK(notify_redirect_handler),
                                            sigdata, NULL, (flags & LIVES_CONNECT_AFTER));
    active_sigdets = lives_list_prepend(active_sigdets, (livespointer)sigdata);
    return sigdata->funcid;
  }
#endif
  if (!flags)
    func_id = g_signal_connect(instance, detailed_signal, c_handler, data);
  else {
    if (flags & LIVES_CONNECT_AFTER)
      func_id = g_signal_connect_after(instance, detailed_signal, c_handler, data);
    else
      func_id = g_signal_connect_swapped(instance, detailed_signal, c_handler, data);
  }
  return func_id;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handler_block(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_block(instance, handler_id);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handler_unblock(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_unblock(instance, handler_id);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handler_disconnect(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_disconnect(instance, handler_id);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_stop_emission_by_name(livespointer instance, const char *detailed_signal) {
#ifdef GUI_GTK
  g_signal_stop_emission_by_name(instance, detailed_signal);
  return TRUE;
#endif
  return FALSE;
}


boolean set_gui_loop_tight(boolean val) {
  if (!is_fg_thread() || !val) gui_loop_tight = val;
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_context_iteration(LiVESWidgetContext * ctx, boolean may_block) {
  RECURSE_GUARD_START;
  boolean ret = FALSE;
  RETURN_VAL_IF_RECURSED(FALSE);
  RECURSE_GUARD_LOCK;
  if (!is_fg_thread()) {
    if (!gui_loop_tight) mainw->do_ctx_update = TRUE;
    RECURSE_GUARD_END;
    return FALSE;
  } else {
    boolean no_idlefuncs = FALSE;
    if (mainw) {
      no_idlefuncs = mainw->no_idlefuncs;
      mainw->no_idlefuncs = TRUE;
      if (mainw->fg_service_source) {
        if (cprio == PRIO_HIGH)
          lives_source_set_priority(mainw->fg_service_source, PRIO_LOW);
      }
    }

    ret = g_main_context_iteration(ctx, may_block);

    if (mainw) {
      mainw->no_idlefuncs = no_idlefuncs;
      if (mainw->fg_service_source) {
        if (cprio == PRIO_HIGH)
          lives_source_set_priority(mainw->fg_service_source, PRIO_HIGH);
      }
    }
  }
  RECURSE_GUARD_END;
  return ret;
}


boolean fg_service_fulfill(void) {
  lives_proc_thread_t lptr = NULL;
  boolean is_fg_service = FALSE;
  void *retval;

  lptr = lpttorun;

  if (lptr && lives_proc_thread_ref(lptr) > 1) {
    if (mainw->debug) {
      char *fcall = lives_proc_thread_show_func_call(lptr);
      g_print("fulfill %s\n", fcall);
      lives_free(fcall);
    }
    if (!lives_proc_thread_is_queued(lptr)) {
      lives_proc_thread_unref(lptr);
      lptr = NULL;
    }
  } else lptr = NULL;

  if (!lptr || lives_proc_thread_is_running(lptr)
      || lives_proc_thread_is_done(lptr)) {
    if (lptr) lives_proc_thread_unref(lptr);

    if (mainw->global_hook_stacks) {
      if (!pthread_mutex_trylock(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex)) {
        if (mainw->global_hook_stacks[LIVES_GUI_HOOK]->stack) {
          boolean is_active;
          pthread_mutex_unlock(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);
          if (THREADVAR(fg_service)) {
            is_fg_service = TRUE;
          } else THREADVAR(fg_service) = TRUE;
          is_active = lives_hooks_trigger(mainw->global_hook_stacks, LIVES_GUI_HOOK);
          if (!is_fg_service) THREADVAR(fg_service) = FALSE;
          return is_active;
        }
        pthread_mutex_unlock(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);
        return FALSE;
      }
      // if we cannot get a lock on the hook_stack, we assume it is busy
      return TRUE;
    }
    return FALSE;
  }

  if (THREADVAR(fg_service)) {
    is_fg_service = TRUE;
  } else THREADVAR(fg_service) = TRUE;

  retval = weed_get_voidptr_value(lptr, "retloc", NULL);
  lives_proc_thread_execute(lptr, retval);

  if (!is_fg_service) THREADVAR(fg_service) = FALSE;
  if (!pthread_mutex_trylock(&lpt_mutex)) {
    if (lpttorun == lptr) lpttorun = NULL;
    pthread_mutex_unlock(&lpt_mutex);
  }

  lives_proc_thread_unref(lptr);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_widget_get_opacity(LiVESWidget * widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 8, 0)
  return gtk_widget_get_opacity(widget);
#endif
#endif
  return FALSE;
}

static volatile int misses = 0;

boolean fg_service_fulfill_cb(void *dummy) {
  // this is a GUI idlefunc which the gui_thread runs
  // there are two modes, and two priorities
  // modes are slack and tight
  // slacK: the idlefunc runs, checks for requests and queued items from bg threads
  // tight: the gui main_loop blocks here, and we only allow updates at specific points
  // this is useful for example when the player is running
  // in high priority, the loop runs as fast as it can to be super responsive
  // after a set number of cycles with no activity, we will switch to low priority
  // to reduce the CPU load. Before pushing requests, background threads can force a kick from low to high prio
  // in advance
  static int prio =  PRIO_HIGH;
  static boolean omode = -1;
  boolean is_fg_service = FALSE;
  boolean is_active;

  if (mainw->no_idlefuncs) {
    // callback has to be disabled during calls to g_main_context_iteration,
    // to prevent that function from blocking
    return TRUE;
  }

  if (omode != gui_loop_tight || cprio != prio) {
    cprio = PRIO_HIGH;
    lives_source_set_priority(mainw->fg_service_source, cprio);
    prio = cprio;
    omode = gui_loop_tight ? TRUE : FALSE;
    misses = 0;
    cprio = prio;
  }

  // just to be sure...
  if (gui_loop_tight && g_main_depth() > 1) return TRUE;

  do {
    is_active = FALSE;
    if (lpttorun || mainw->global_hook_stacks[LIVES_GUI_HOOK]->stack)
      is_active = fg_service_fulfill();
    if (is_active) {
      cprio = PRIO_HIGH;
      if (gui_loop_tight && !mainw->do_ctx_update && !lpttorun
          && !mainw->global_hook_stacks[LIVES_GUI_HOOK]->stack)
        lives_widget_context_iteration(NULL, FALSE);
      if (prio != PRIO_HIGH) {
        lives_source_set_priority(mainw->fg_service_source, cprio);
        prio = cprio;
      }
      misses = 0;
    } else if (misses >= 0 && cprio == PRIO_HIGH)
      if (++misses >= MISS_PRIO_THRESH) {
        cprio = PRIO_LOW;
        if (!gui_loop_tight) {
          lives_source_set_priority(mainw->fg_service_source, cprio);
          prio = cprio;
        }
      }
    if (!gui_loop_tight) break;

    if (mainw->do_ctx_update) {

      // during playback this is where all callbacks such as key presses will happen
      if (!is_active) _lives_widget_context_update();
      mainw->do_ctx_update = FALSE;
    } else {
      lives_nanosleep(NSLEEP_TIME);
      if (cprio == PRIO_LOW) {
        for (int i = 0; i < sLO_FACTOR; i++) {
          if (cprio != PRIO_LOW) break;
          pthread_yield();
          lives_nanosleep(NSLEEP_TIME);
        }
        if (modalw) lives_widget_context_iteration(NULL, FALSE);
      }
    }
  } while (gui_loop_tight);

  if (mainw->do_ctx_update && !is_active) {
    if (THREADVAR(fg_service)) {
      is_fg_service = TRUE;
    } else THREADVAR(fg_service) = TRUE;
    _lives_widget_context_update();
    mainw->do_ctx_update = FALSE;
    if (!is_fg_service) THREADVAR(fg_service) = FALSE;
  } else if (!gui_loop_tight) {
    if (prio == PRIO_HIGH)
      lives_widget_context_iteration(NULL, FALSE);
    else {
      for (int zz = 0; zz < 2048 && cprio == PRIO_LOW; zz++)
        lives_microsleep;
    }
  }
  mainw->do_ctx_update = FALSE;
  return TRUE;
}


void fg_service_wake(void) {
  cprio = PRIO_HIGH;
  // signal to keep high prio until something is run
  misses = 0;
}


WIDGET_HELPER_GLOBAL_INLINE void fg_stack_wait(void) {
  // test fo nonzero-ness: if the trylock fails we try again
  // when trylock succeeds and returns 0, then we proceed to the next part of the &&
  // we negate the next part, if the (anti)condition is FALSE, the other part of the || is irrelevant
  // (the mutex unlock) and the loop finshes, then we just need to ensure the mutex unlock happens
  // if the (anti)condition is TRUE, then the other part of the || is checked
  // - the mutex unlock should always return 0, then negated this becomes 1, so we loop again
  lives_nanosleep_until_zero(pthread_mutex_trylock(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex) // a;lways 0 to pass
                             && !(!(mainw->global_hook_stacks[LIVES_GUI_HOOK]->stack != NULL
                                    && (mainw->global_hook_stacks[LIVES_GUI_HOOK]->flags & STACK_TRIGGERING)) ||
                                  pthread_mutex_unlock(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex)));
  pthread_mutex_unlock(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);
}


static void async_sig_handler(livespointer instance, livespointer data) {
  lives_thread_attr_t attr = 0;//LIVES_THRDATTR_WAIT_SYNC;
  lives_sigdata_t *sigdata = lives_calloc(1, sizeof(lives_sigdata_t));
  lives_memcpy(sigdata, data, sizeof(lives_sigdata_t));
  sigdata->detsig = NULL;
  if (sigdata->instance != instance) {
    sigdata_free(sigdata, NULL);
    return;
  }
  if (sigdata->swapped) {
    lives_proc_thread_create(attr, (lives_funcptr_t)sigdata->callback, 0, "vv",
                             sigdata->user_data, instance);
  } else {
    lives_proc_thread_create(attr, (lives_funcptr_t)sigdata->callback, 0, "vv",
                             instance, sigdata->user_data);
  }
  sigdata_free(sigdata, NULL);
}


static void async_sig_handler3(livespointer instance, livespointer extra, livespointer data) {
  lives_sigdata_t *sigdata = lives_calloc(1, sizeof(lives_sigdata_t));
  lives_thread_attr_t attr = 0;//LIVES_THRDATTR_WAIT_SYNC;
  lives_memcpy(sigdata, data, sizeof(lives_sigdata_t));
  sigdata->detsig = NULL;
  if (sigdata->instance != instance) {
    sigdata_free(sigdata, NULL);
    return;
  }
  if (sigdata->swapped)
    lives_proc_thread_create(attr, sigdata->callback, 0, "vvv", sigdata->user_data,
                             extra, instance);
  else
    lives_proc_thread_create(attr, sigdata->callback, 0, "vvv", instance, extra,
                             sigdata->user_data);
  sigdata_free(sigdata, NULL);
}

#if 0
static boolean async_timer_handler(livespointer data) {
  if (mainw->is_exiting) return FALSE;
  else {
    lives_sigdata_t *sigdata = lives_calloc(1, sizeof(lives_sigdata_t));
    lives_memcpy(sigdata, data, sizeof(lives_sigdata_t));
    sigdata->detsig = NULL;
    sigdata->is_timer = TRUE;
    //g_print("SOURCE is %s\n", g_source_get_name(g_main_current_source())); // NULL for timer, GIdleSource for idle
    //g_print("hndling %p %s %p\n", sigdata, sigdata->detsig, (void *)sigdata->detsig);

    if (sigdata->state == SIGDATA_STATE_NEW) {
      lives_thread_attr_t attr = LIVES_THRDATTR_WAIT_SYNC;
      sigdata->proc = lives_proc_thread_create(attr, (lives_funcptr_t)sigdata->callback, WEED_SEED_BOOLEAN,
                      "v", sigdata->user_data);
    }

    while (1) {
      if (sigdata->state == SIGDATA_STATE_DESTROYED || sigdata->state == SIGDATA_STATE_FINISHED) {
        boolean res = FALSE;
        if (sigdata->state == SIGDATA_STATE_FINISHED) {
          res = lives_proc_thread_join_boolean(sigdata->proc);
        }
        if (sigdata->proc) lives_proc_thread_unref(sigdata->proc);
        sigdata->proc = NULL;
        sigdata_free(sigdata, NULL);
        return res;
      } else {
        lives_nanosleep_while_true(gov_loop_blocked || governor_loop(sigdata));
      }
    }
  }
  return FALSE;
}
#endif

unsigned long lives_signal_connect_async(livespointer instance, const char *detailed_signal, LiVESGuiCallback c_handler,
    livespointer data, LiVESConnectFlags flags) {
  static size_t notilen = -1;
  lives_sigdata_t *sigdata;
  uint32_t nvals;
  GSignalQuery sigq;

#if LIVES_HAS_SWITCH_WIDGET
  boolean swtog = FALSE;
  if (LIVES_IS_WIDGET(instance) && LIVES_IS_SWITCH(LIVES_WIDGET(instance))
      && !strcmp(detailed_signal, LIVES_WIDGET_TOGGLED_SIGNAL)) swtog = TRUE;
  else {
#endif

    if (notilen == -1) notilen = lives_strlen(LIVES_WIDGET_NOTIFY_SIGNAL);
    if (!lives_strncmp(detailed_signal, LIVES_WIDGET_NOTIFY_SIGNAL, notilen)) {
      return lives_signal_connect_sync(instance, detailed_signal, c_handler, data, flags);
    }

    g_signal_query(g_signal_lookup(detailed_signal, G_OBJECT_TYPE(instance)), &sigq);
    if (sigq.return_type != 4) {
      return lives_signal_connect_sync(instance, detailed_signal, c_handler, data, flags);
    }

    nvals = sigq.n_params + 2; // add instance, user_data

    if (nvals != 2 && nvals != 3) {
      return lives_signal_connect_sync(instance, detailed_signal, c_handler, data, flags);
    }

#if LIVES_HAS_SWITCH_WIDGET
  }
#endif

  sigdata = (lives_sigdata_t *)lives_calloc(1, sizeof(lives_sigdata_t));
  sigdata->instance = instance;
  sigdata->callback = (lives_funcptr_t)c_handler;
  sigdata->user_data = data;
  sigdata->swapped = (flags & LIVES_CONNECT_SWAPPED) ? TRUE : FALSE;
  active_sigdets = lives_list_prepend(active_sigdets, (livespointer)sigdata);

#if LIVES_HAS_SWITCH_WIDGET
  if (swtog) {
    /// to make switch and checkbutton interchangeable,
    /// we substitute the "toggled" signal for a switch with "notify::active"
    /// and then redirect it back to the desired callback
    sigdata->detsig = lives_strdup(LIVES_WIDGET_TOGGLED_SIGNAL);
    sigdata->funcid = g_signal_connect_data(instance, LIVES_WIDGET_NOTIFY_SIGNAL "active",
                                            LIVES_GUI_CALLBACK(async_notify_redirect_handler),
                                            sigdata, NULL, (flags & LIVES_CONNECT_AFTER));
    return sigdata->funcid;
  }
#endif

  sigdata->detsig = lives_strdup(detailed_signal);

  if (nvals == 2) {
    sigdata->funcid = g_signal_connect_data(instance, detailed_signal,
                                            LIVES_GUI_CALLBACK(async_sig_handler),
                                            sigdata, NULL, (flags & LIVES_CONNECT_AFTER));
  } else {
    sigdata->funcid = g_signal_connect_data(instance, detailed_signal,
                                            LIVES_GUI_CALLBACK(async_sig_handler3),
                                            sigdata, NULL, (flags & LIVES_CONNECT_AFTER));
  }
  return sigdata->funcid;
}

static lives_sigdata_t *find_sigdata(livespointer instance, LiVESGuiCallback func, livespointer data) {
  LiVESList *list = active_sigdets;
  for (; list; list = list->next) {
    lives_sigdata_t *sigdata = (lives_sigdata_t *)list->data;
    if (sigdata->instance == instance && sigdata->callback == (lives_funcptr_t)func
        && sigdata->user_data == data) return sigdata;
  }
  return NULL;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_sync_disconnect_by_func(livespointer instance,
    LiVESGuiCallback func, livespointer data) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_WIDGET(instance) && LIVES_IS_SWITCH(LIVES_WIDGET(instance))) {
    /// to make switch and checkbutton interchangeable,
    /// we substitute the "toggled" signal for a switch with "notify::active"
    /// and then redirect it back to the desired callback
    lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
    if (sigdata) {
      g_signal_handler_disconnect(instance, sigdata->funcid);
      return TRUE;
    }
  }
#endif
  g_signal_handlers_disconnect_by_func(instance, func, data);
  return TRUE;
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_sync_block_by_func(livespointer instance,
    LiVESGuiCallback func, livespointer data) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_WIDGET(instance) && LIVES_IS_SWITCH(LIVES_WIDGET(instance))) {
    /// to make switch and checkbutton interchangeable,
    /// we substitute the "toggled" signal for a switch with "notify::active"
    /// and then redirect it back to the desired callback
    lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
    if (sigdata) {
      g_signal_handler_block(instance, sigdata->funcid);
      return TRUE;
    }
  }
#endif
  g_signal_handlers_block_by_func(instance, func, data);
  return TRUE;
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_sync_unblock_by_func(livespointer instance,
    LiVESGuiCallback func, livespointer data) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_WIDGET(instance) && LIVES_IS_SWITCH(LIVES_WIDGET(instance))) {
    /// to make switch and checkbutton interchangeable,
    /// we substitute the "toggled" signal for a switch with "notify::active"
    /// and then redirect it back to the desired callback
    lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
    if (sigdata) {
      g_signal_handler_unblock(instance, sigdata->funcid);
      return TRUE;
    }
  }
#endif
  g_signal_handlers_unblock_by_func(instance, func, data);
  return TRUE;
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_disconnect_by_func(livespointer instance,
    LiVESGuiCallback func, livespointer data) {
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_WIDGET(instance) && LIVES_IS_SWITCH(LIVES_WIDGET(instance))) {
    return lives_signal_handlers_sync_disconnect_by_func(instance, func, data);
  } else {
#endif
    lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
    if (sigdata) {
      lives_signal_handler_disconnect(instance, sigdata->funcid);
      return TRUE;
    }
    return lives_signal_handlers_sync_disconnect_by_func(instance, func, data);
#if LIVES_HAS_SWITCH_WIDGET
  }
#endif
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_block_by_func(livespointer instance,
    LiVESGuiCallback func, livespointer data) {
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_WIDGET(instance) && LIVES_IS_SWITCH(LIVES_WIDGET(instance))) {
    return lives_signal_handlers_sync_block_by_func(instance, func, data);
  } else {
#endif
    lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
    if (sigdata) {
      lives_signal_handler_block(instance, sigdata->funcid);
      return TRUE;
    }
    return lives_signal_handlers_sync_block_by_func(instance, func, data);
#if LIVES_HAS_SWITCH_WIDGET
  }
#endif
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_unblock_by_func(livespointer instance,
    LiVESGuiCallback func, livespointer data) {
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_WIDGET(instance) && LIVES_IS_SWITCH(LIVES_WIDGET(instance))) {
    return lives_signal_handlers_sync_unblock_by_func(instance, func, data);
  } else {
#endif
    lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
    if (sigdata) {
      lives_signal_handler_unblock(instance, sigdata->funcid);
      return TRUE;
    }
    return lives_signal_handlers_sync_unblock_by_func(instance, func, data);
#if LIVES_HAS_SWITCH_WIDGET
  }
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grab_add(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_grab_add(widget);
  return TRUE;
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_grab_remove(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_grab_remove(widget);
  return TRUE;
#endif
  return FALSE;
}


static void _lives_menu_item_set_sensitive(LiVESWidget * item, boolean state);
static void _lives_menu_item_set_sensitive_p(LiVESWidget * item, livespointer ptr) {
  _lives_menu_item_set_sensitive(item, LIVES_POINTER_TO_INT(ptr));
}


static void _lives_menu_item_set_sensitive(LiVESWidget * item, boolean state) {
  if (GTK_IS_MENU_ITEM(item)) {
    if (1) {
      LiVESWidget *parent = lives_widget_get_parent(item);
      LiVESWidget *sub = lives_menu_item_get_submenu(LIVES_MENU_ITEM(item));
      boolean old_state = gtk_widget_get_sensitive(item);
      if (parent && GTK_IS_MENU_ITEM(parent)) {
        // when changing state, we set the new state, and store old state internally
        // s -> i set i, store s
        // i -> s set s, store i
        // change is passed to child submenus
        // internal state is toggled
        //
        // however we also need to check parent state:
        // old s, new s - not passed down, normal
        // old s, new i - i is passed down.  - store current state internally, set to i,
        //		s - i - set i, store s, change passed down, interal state NOT toggled
        //	       	i - i,  store i, not passed down
        // old i new i, same as old s new i
        // old i new s, if stored is s, set state s - pass down

        if (!lives_widget_get_sensitive(parent)) {
          // if parent (or ancestor) became insensitive:
          // update "virtual" state
          // we need to do this even if already insensitive
          SET_INT_DATA(item, SUBMENU_INS_KEY, (int)!state);
          // if widget state is sensitive, it will change, and this is passed down
        } else {
          // if parent is sensitive, need to check prior state
          // if it is unchanged then
          // otherwise check stored state, if it is insensitve we do nothing
          if (GET_INT_DATA(parent, SUBMENU_INS_KEY))
            if (GET_INT_DATA(item, SUBMENU_INS_KEY)) return;
          // else parent state has not changed - normal change
        }
      }
      if (state != old_state) {
        // update widget state
        gtk_widget_set_sensitive(item, state);
        if (sub) {
          // set old state, so child menus know if it has altered and been passed down
          // rather than being directed at them
          SET_INT_DATA(item, SUBMENU_INS_KEY, (int)!old_state);
          // if we have submenus, then pass the change downwards recursively
          // each submenu entry will check parent state (this widget), then update accordingly
          // the change may be passed down further, recursively
          lives_container_foreach_int(LIVES_CONTAINER(sub), _lives_menu_item_set_sensitive_p, (int)state);
          // set to new state so child menus know this is now unchanged
          SET_INT_DATA(item, SUBMENU_INS_KEY, (int)!state);
        }
      }
    }
  }
}


WIDGET_HELPER_LOCAL_INLINE void _lives_widget_set_sensitive(LiVESWidget * widget, boolean state) {
  if (!LIVES_IS_WIDGET(widget)) break_me("non widget in set_sensitive");
#ifdef GUI_GTK
#ifdef GTK_SUBMENU_SENS_BUG
  if (GTK_IS_MENU_ITEM(widget)) {
    _lives_menu_item_set_sensitive(widget, (int)state);
    return;
  }
#endif
  gtk_widget_set_sensitive(widget, state);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_sensitive(LiVESWidget * widget, boolean state) {
#ifdef GUI_GTK
  if (!LIVES_IS_WIDGET(widget)) break_me("non widget in set_sensitive");
  if (1 || is_fg_thread()) _lives_widget_set_sensitive(widget, state);
  else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(_lives_widget_set_sensitive, "vb", widget, state);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_sensitive(LiVESWidget * widget) {
#ifdef GUI_GTK
  return gtk_widget_get_sensitive(widget);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_show(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_widget_show(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_hide(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_widget_hide(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_LOCAL_INLINE boolean _lives_widget_show_all(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_widget_show_all(widget);
  if (mainw->is_ready) {
    // recommended to center the window again after adding all its widgets
    if (LIVES_IS_DIALOG(widget) && mainw->mgeom) lives_window_center(LIVES_WINDOW(widget));
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_show_all(LiVESWidget * widget) {
  boolean retloc = FALSE;
#ifdef GUI_GTK
  if (is_fg_thread()) {
    gtk_widget_show_all(widget);
    if (mainw->is_ready) {
      // recommended to center the window again after adding all its widgets
      if (LIVES_IS_DIALOG(widget) && mainw->mgeom) lives_window_center(LIVES_WINDOW(widget));
    }
  } else {
    BG_THREADVAR(hook_hints) = HOOK_UNIQUE_DATA | HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
    MAIN_THREAD_EXECUTE(_lives_widget_show_all, WEED_SEED_BOOLEAN, &retloc, "v", widget);
    BG_THREADVAR(hook_hints) = 0;
  }

  return TRUE;
#endif
  return retloc;
}


static boolean _lives_widget_queue_draw_and_update(LiVESWidget * widget) {
  gtk_widget_queue_draw(widget);
  _lives_widget_process_updates(widget);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw_and_update(LiVESWidget * widget) {
  if (is_fg_thread()) _lives_widget_queue_draw_and_update(widget);
  else {
    //BG_THREADVAR(hook_hints) = HOOK_UNIQUE_DATA | HOOK_CB_PRIORITY | HOOK_CB_BLOCK;
    BG_THREADVAR(hook_hints) = HOOK_UNIQUE_DATA | HOOK_CB_PRIORITY | HOOK_CB_BLOCK;
    MAIN_THREAD_EXECUTE_RVOID(_lives_widget_queue_draw_and_update, "v", widget);
    BG_THREADVAR(hook_hints) = 0;
  }
  return FALSE;
}


WIDGET_HELPER_LOCAL_INLINE boolean _lives_widget_show_now(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_widget_show_now(widget);
  _lives_widget_queue_draw_and_update(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_show_now(LiVESWidget * widget) {
  // run in main thread as it seems to give a smoother result
  boolean ret;
  if (is_fg_thread()) return _lives_widget_show_now(widget);
  BG_THREADVAR(hook_hints) = HOOK_UNIQUE_DATA | HOOK_CB_PRIORITY | HOOK_CB_BLOCK;
  main_thread_execute(_lives_widget_show_now, WEED_SEED_BOOLEAN, &ret, "v", widget);
  BG_THREADVAR(hook_hints) = 0;
  return ret;
}


static void _lives_widget_destroy(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_widget_destroy(widget);
#endif
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_destroy(LiVESWidget * widget) {
#ifdef GUI_GTK
  if (LIVES_IS_WIDGET(widget)) {
    if (mainw && mainw->is_ready) {
      THREADVAR(hook_match_nparams) = 1;
      THREADVAR(hook_hints) |= HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD | HOOK_CB_TRANSFER_OWNER;
      // do this even for main thread so we can invalidate data for other threads
      MAIN_THREAD_EXECUTE_RVOID(_lives_widget_destroy, "v", widget);
      THREADVAR(hook_hints) = 0;
      THREADVAR(hook_match_nparams) = 0;
      return TRUE;
    }
    gtk_widget_destroy(widget);
    return TRUE;
  }
#endif
  return FALSE;
}


static boolean _lives_widget_realize(LiVESWidget * widget) {
#ifdef GUI_GTK
  if (!LIVES_IS_WIDGET(widget)) {
    LIVES_WARN("Realize invalid widget");
    return FALSE;
  }
  gtk_widget_realize(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_realize(LiVESWidget * widget) {
  return _lives_widget_realize(widget);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw(LiVESWidget * widget) {
#ifdef GUI_GTK
  if (!LIVES_IS_WIDGET(widget)) {
    abort();
    LIVES_WARN("Draw queue invalid widget");
    return FALSE;
  }
  if (is_fg_thread()) gtk_widget_queue_draw(widget);
  else {
    BG_THREADVAR(hook_hints) = HOOK_UNIQUE_DATA | HOOK_CB_PRIORITY | HOOK_CB_BLOCK;
    MAIN_THREAD_EXECUTE_RVOID(gtk_widget_queue_draw, "v", widget);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw_area(LiVESWidget * widget, int x, int y, int width, int height) {
#ifdef GUI_GTK
  if (1 || is_fg_thread()) {
    gtk_widget_queue_draw_area(widget, x, y, width, height);
    return TRUE;
  } else {
    MAIN_THREAD_EXECUTE_RVOID(gtk_widget_queue_draw_area, "viiii", widget, x, y, width, height);
    return TRUE;
  }
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_resize(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_widget_queue_resize(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_size_request(LiVESWidget * widget, int width, int height) {
#ifdef GUI_GTK
  if (!mainw->ignore_screen_size) {
    if (width > GUI_SCREEN_WIDTH || height > GUI_SCREEN_HEIGHT) abort();
  } else {
    if (width > GUI_SCREEN_PHYS_WIDTH || height > GUI_SCREEN_PHYS_HEIGHT) abort();
  }
  if (LIVES_IS_WINDOW(widget)) lives_window_resize(LIVES_WINDOW(widget), width, height);
  else gtk_widget_set_size_request(widget, width, height);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_minimum_size(LiVESWidget * widget, int width, int height) {
#ifdef GUI_GTK
  GdkGeometry geom;
  GdkWindowHints mask;
  GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
  if (!mainw->ignore_screen_size) {
    if (width > GUI_SCREEN_WIDTH || height > GUI_SCREEN_HEIGHT) abort();
  } else {
    if (width > GUI_SCREEN_PHYS_WIDTH || height > GUI_SCREEN_PHYS_HEIGHT) abort();
  }
  if (GTK_IS_WINDOW(toplevel)) {
    geom.min_width = width;
    geom.min_height = height;
    mask = GDK_HINT_MIN_SIZE;
    gtk_window_set_geometry_hints(GTK_WINDOW(toplevel), widget, &geom, mask);
    return TRUE;
  }
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_maximum_size(LiVESWidget * widget, int width, int height) {
#ifdef GUI_GTK
  GdkGeometry geom;
  GdkWindowHints mask;
  GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
  if (!mainw->ignore_screen_size) {
    if (width > GUI_SCREEN_WIDTH || height > GUI_SCREEN_HEIGHT) abort();
  } else {
    if (width > GUI_SCREEN_PHYS_WIDTH || height > GUI_SCREEN_PHYS_HEIGHT) abort();
  }
  if (GTK_IS_WINDOW(toplevel)) {
    geom.max_width = width;
    geom.max_height = height;
    mask = GDK_HINT_MAX_SIZE;
    gtk_window_set_geometry_hints(GTK_WINDOW(toplevel), widget, &geom, mask);
    return TRUE;
  }
#endif
  return FALSE;
}


void unlock_lpt(lives_proc_thread_t lpt) {
  if (!pthread_mutex_trylock(&lpt_mutex)) {
    if (lpttorun == lpt) lpttorun = NULL;
    pthread_mutex_unlock(&lpt_mutex);
  }
}

static void wait_for_fg_response(lives_proc_thread_t lpt, void *retval) {
  GET_PROC_THREAD_SELF(self);
  //volatile boolean bvar = FALSE;
  boolean do_cancel = FALSE;

  // must set this, so we get tyoed return value from finished action
  weed_set_voidptr_value(lpt, "retloc", retval);

  //lives_proc_thread_add_hook(lpt, COMPLETED_HOOK, 0, (hook_funcptr_t)lptdone, &bvar);

  // setting this, we trigger the main thread to run lpt in fg_service_fulfill)
  lpttorun = lpt;

  // wait for main thread to pick it up and set PREPARING state
  lives_nanosleep_until_nonzero((do_cancel = lives_proc_thread_should_cancel(lpt))
                                || lives_proc_thread_is_preparing(lpt)
                                || lives_proc_thread_is_running(lpt)
                                || lives_proc_thread_is_done(lpt));

  // once we unlock this, a) main thread can complete lpttorun and nullify lpttorun
  // this is fine since it would have triggered lptdone and hence bvar will be TRUE now
  // and b) another bg thread can now grab lpt_mutex, and it will set lpttorun to NULL, then to its lpt
  // this is also fine, it will block waiting for PREAPRING, with mutex locked
  pthread_mutex_unlock(&lpt_mutex);

  lives_nanosleep_while_false(do_cancel || lives_proc_thread_should_cancel(self) || lives_proc_thread_is_done(lpt));

  // if another bg thread is waiting it will have changed lpttorun, so only NULLify if ours and with mutex locked
  //     (so it cannot change while we check)
  // if we dont get mutex lock, then either main_thread will reset it or another bg thread has lock and will reset it
  if (lpt) unlock_lpt(lpt);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_get_origin(LiVESXWindow * xwin, int *posx, int *posy) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(4, 0, 0)
  gdk_surface_get_origin(xwin, posx, posy);
#else
  gdk_window_get_origin(xwin, posx, posy);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_get_frame_extents(LiVESXWindow * xwin, lives_rect_t *rect) {
#ifdef GUI_GTK
  gdk_window_get_frame_extents(xwin, (GdkRectangle *)rect);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_reparent(LiVESWidget * widget, LiVESWidget * new_parent) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 14, 0)
  GtkWidget *parent = gtk_widget_get_parent(widget);
  g_object_ref(widget);
  if (parent) {
    gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(widget)), widget);
  }
  lives_container_add(GTK_CONTAINER(new_parent), widget);
  g_object_unref(widget);
#else
  gtk_widget_reparent(widget, new_parent);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_ancestor(LiVESWidget * widget, LiVESWidget * ancestor) {
#ifdef GUI_GTK
  return gtk_widget_is_ancestor(widget, ancestor);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_app_paintable(LiVESWidget * widget, boolean paintable) {
#ifdef GUI_GTK
  gtk_widget_set_app_paintable(widget, paintable);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_opacity(LiVESWidget * widget, double opacity) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 8, 0)
  if (capable->wm_caps.is_composited) {
    gtk_widget_set_opacity(widget, opacity);
  }
  return TRUE;
#endif
  if (opacity == 0.) lives_widget_hide(widget);
  if (opacity == 1.) lives_widget_show(widget);
#endif
  return FALSE;
}


static void _dialog_resp_set(LiVESDialog * dlg, int resp, livespointer data) {
  SET_INT_DATA(dlg, RESPONSE_KEY, resp);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESResponseType lives_dialog_get_response(LiVESDialog * dlg) {
  return GET_INT_DATA(dlg, RESPONSE_KEY);
}

static boolean lives_dialog_destroyed(LiVESWidget * dialog, void *data) {
  SET_INT_DATA(dialog, DESTROYED_KEY, TRUE);
  return FALSE;
}

static LiVESResponseType _dialog_run(LiVESDialog * dialog) {
  LiVESResponseType resp;
  ulong func = lives_signal_sync_connect(dialog, LIVES_WIDGET_RESPONSE_SIGNAL,
                                         LIVES_GUI_CALLBACK(_dialog_resp_set), NULL);
  ulong dfunc = lives_signal_sync_connect(LIVES_GUI_OBJECT(dialog), LIVES_WIDGET_DESTROY_SIGNAL,
                                          LIVES_GUI_CALLBACK(lives_dialog_destroyed), NULL);
  boolean dest;

  lives_widget_object_ref(dialog);
  SET_INT_DATA(dialog, RESPONSE_KEY, LIVES_RESPONSE_INVALID);
  _lives_widget_show_all(LIVES_WIDGET(dialog));

  if (!mainw->is_ready) pop_to_front(LIVES_WIDGET(dialog), NULL);

  do {
    fg_service_fulfill();
    lives_widget_context_iteration(NULL, FALSE);
    pthread_yield();
    lives_nanosleep(NSLEEP_TIME);
    resp = GET_INT_DATA(dialog, RESPONSE_KEY);
  } while (!(dest = GET_INT_DATA(dialog, DESTROYED_KEY)) && resp == LIVES_RESPONSE_INVALID);

  if (!dest) {
    if (dialog) lives_signal_handler_disconnect(dialog, dfunc);
    if (dialog) lives_signal_handler_disconnect(dialog, func);
  }
  SET_INT_DATA(dialog, RESPONSE_KEY, LIVES_RESPONSE_INVALID);
  lives_widget_object_unref(dialog);

  g_print("DLG RESPaa %d\n", resp);
  return resp;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESResponseType lives_dialog_run(LiVESDialog * dialog) {
  LiVESResponseType resp = LIVES_RESPONSE_INVALID;
#ifdef GUI_GTK
  //resp = gtk_dialog_run(dialog);
  if (is_fg_thread()) {
    gtk_widget_show_all(GTK_WIDGET(dialog));
    _lives_widget_context_update();
    resp = gtk_dialog_run(dialog);
  } else {
    BG_THREADVAR(hook_hints) = HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
    main_thread_execute(_dialog_run, WEED_SEED_INT, &resp, "v", dialog);
    BG_THREADVAR(hook_hints) = 0;
  }
  g_print("DLG RESP %d\n", resp);
#endif
  return resp;
}


// the purpose of this function is to force a lives_proc_thread to be run by the foreground
// (i.e graphics) thread. Background threads which need to do GUI updates should use this
// to run a lpt. Also note that fg service calls may not be nested,
// instead the calls may be deferred and run in sequence
void fg_service_call(lives_proc_thread_t lpt, void *retval) {
  lives_proc_thread_ref(lpt);
  if (!lpt) return;

  if (is_fg_thread()) {
    // should not happend, but just in case
    lives_proc_thread_execute(lpt, retval);
    lives_proc_thread_unref(lpt);
    return;
  } else {
    GET_PROC_THREAD_SELF(self);
    // wait here until we get the mutex - this means any thread ahead of us has waited
    // its lpt has passed at least to prepared, or it is locke transiently while lpttorun is nullified
    fg_service_wake();
    while (pthread_mutex_trylock(&lpt_mutex)) {
      if (lives_proc_thread_get_cancel_requested(lpt)
          || lives_proc_thread_get_cancel_requested(self)) {
        lives_proc_thread_cancel(lpt);
        lives_proc_thread_unref(lpt);
        return;
      }
      lives_nanosleep(LIVES_QUICK_NAP);
    }
    // once we have lock, no other thread can reset lpttotrun so we will do that here
    // then in wati_for_fg_response, we will set lpttorun to our lpt, and block until
    // state is at least PREPENDING (or CANCELLED !)

    // resetting this avoids main_thread rechecking the completed request
    lpttorun = NULL;

    if (lives_proc_thread_should_cancel(lpt)) {
      /* if (!lives_proc_thread_was_cancelled) */
      /* 	lives_proc_thread_cancel(lpt); */
      lives_proc_thread_unref(lpt);
      pthread_mutex_unlock(&lpt_mutex);
      return;
    }

    wait_for_fg_response(lpt, retval);

    lives_proc_thread_unref(lpt);
  }
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_dialog_response(LiVESDialog * dialog, LiVESResponseType response) {
#ifdef GUI_GTK
  gtk_dialog_response(dialog, response);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESResponseType lives_dialog_get_response_for_widget(LiVESDialog * dialog, LiVESWidget * widget) {
#ifdef GUI_GTK
  if (is_standard_widget(LIVES_WIDGET(dialog))) {
    LiVESWidget *action = lives_standard_dialog_get_action_area(dialog);
    LiVESList *children = lives_container_get_children(LIVES_CONTAINER(action)), *list = children;
    for (; list; list = list->next) {
      LiVESWidget *w = (LiVESWidget *)list->data;
      if (w == widget) return LIVES_POINTER_TO_INT(lives_widget_object_get_data
                                (LIVES_WIDGET_OBJECT(w), RESPONSE_KEY));
    }
  } else return gtk_dialog_get_response_for_widget(dialog, widget);
#endif
  return LIVES_RESPONSE_NONE;
}


WIDGET_HELPER_GLOBAL_INLINE
LiVESWidget *lives_dialog_get_widget_for_response(LiVESDialog * dialog, LiVESResponseType response) {
#ifdef GUI_GTK
  if (is_standard_widget(LIVES_WIDGET(dialog))) {
    LiVESWidget *action = lives_standard_dialog_get_action_area(dialog);
    LiVESList *children = lives_container_get_children(LIVES_CONTAINER(action)), *list = children;
    for (; list; list = list->next) {
      LiVESWidget *w = (LiVESWidget *)list->data;
      if (GET_INT_DATA(w, RESPONSE_KEY) == response) return w;
    }
    return NULL;
  } else return gtk_dialog_get_widget_for_response(dialog, response);
#endif
  return NULL;
}


#if GTK_CHECK_VERSION(3, 16, 0)

#define RND_STRLEN 12
#define RND_STR_PREFIX "XXX"

static char *make_random_string(const char *prefix) {
  // give each widget a random name so we can style it individually
  char *str;
  size_t psize = strlen(prefix);
  size_t rsize = RND_STRLEN << 1;

  if (psize > RND_STRLEN) return NULL;

  str = (char *)lives_malloc(rsize);
  lives_snprintf(str, psize + 1, "%s", prefix);

  rsize--;

  for (int i = psize; i < rsize; i++) str[i] = ((lives_random() & 15) + 65);
  str[rsize] = 0;
  return str;
}
#endif


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
//#define ORD_NAMES

static boolean set_css_value_for_state_flag(LiVESWidget * widget, LiVESWidgetState state, const char *xselector,
    const char *detail, const char *value) {
  GtkCssProvider *provider;
  GtkStyleContext *ctx;
  char *widget_name, *wname, *selector;
  char *css_string;
  char *state_str, *selstr;
  int prio = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;
#ifdef ORD_NAMES
  static int widnum = 1;
  int brk_widnum = 3128;
#endif
  if (!widget) {
    int numtok = get_token_count(xselector, ' ') ;
    if (numtok > 1) {
      char **array = lives_strsplit(xselector, " ", 2);
      widget_name = lives_strdup(array[0]);
      selector = lives_strdup(array[1]);
      lives_strfreev(array);
    } else {
      widget_name = lives_strdup(xselector);
      selector = lives_strdup("");
    }
    provider = gtk_css_provider_new();

    // setting context provider for screen is VERY slow, so this should be used sparingly
    prio = GTK_STYLE_PROVIDER_PRIORITY_USER;
    gtk_style_context_add_provider_for_screen(mainw->mgeom[widget_opts.monitor].screen, GTK_STYLE_PROVIDER
        (provider), prio);
  } else {
    if (!LIVES_IS_WIDGET(widget)) return FALSE;
    selector = (char *)xselector;

    ctx = gtk_widget_get_style_context(widget);
    provider = gtk_css_provider_new();
    if (!strcmp(detail, "font-size")) prio = GTK_STYLE_PROVIDER_PRIORITY_USER;

    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(provider), prio);

    widget_name = lives_strdup(gtk_widget_get_name(widget));

    if (!widget_name || (strncmp(widget_name, RND_STR_PREFIX, strlen(RND_STR_PREFIX)))) {
      lives_freep((void **)&widget_name);
#ifdef ORD_NAMES
      widget_name = lives_strdup_printf("%s-%d", RND_STR_PREFIX, ++widnum);
#else
      widget_name = make_random_string(RND_STR_PREFIX);
#endif
      gtk_widget_set_name(widget, widget_name);
#ifdef ORD_NAMES
      if (widnum == brk_widnum) break_me("widnum");
#endif
    }
  }

#ifdef GTK_TEXT_VIEW_CSS_BUG
  if (widget && GTK_IS_TEXT_VIEW(widget)) {
    lives_freep((void **)&widget_name);
    widget_name = lives_strdup("GtkTextView");
  } else {
#endif
    switch (state) {
    // TODO: gtk+ 3.x can set multiple states
    case GTK_STATE_FLAG_ACTIVE:
      state_str = ":active";
      break;
    case GTK_STATE_FLAG_FOCUSED:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":focus";
#endif
      break;
    case GTK_STATE_FLAG_PRELIGHT:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":hover";
#else
      state_str = ":prelight";
#endif
      break;
    case GTK_STATE_FLAG_SELECTED:
      state_str = ":selected";
      break;
    case GTK_STATE_FLAG_CHECKED:
      state_str = ":checked";
      break;
    case GTK_STATE_FLAG_INCONSISTENT:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":indeterminate";
#endif
      break;
    case GTK_STATE_FLAG_BACKDROP:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":backdrop";
#endif
      break;
    case GTK_STATE_FLAG_INSENSITIVE:
#if GTK_CHECK_VERSION(3, 24, 0)
      state_str = ":disabled";
#else
      state_str = ":insensitive";
#endif
      break;
    default:
      state_str = "";
    }
    if (widget) {
      // special tweaks
      if (!selector) {
        if (GTK_IS_FRAME(widget)) {
          if (selector != xselector) lives_free(selector);
          selector = lives_strdup("label");
        } else if (GTK_IS_TEXT_VIEW(widget)) {
          if (selector != xselector) lives_free(selector);
          selector = lives_strdup("text");
        }
        /* if (GTK_IS_SPIN_BUTTON(widget)) { */
        /*   if (selector != xselector) lives_free(selector); */
        /*   selector = lives_strdup("*"); */
        /* } */
      }
    }

    if (!selector || !*selector || *selector == '-') selstr = lives_strdup("");
    else selstr = lives_strdup_printf(" %s", selector);
    if (widget) {
#if GTK_CHECK_VERSION(3, 24, 0)
      wname = lives_strdup_printf("#%s%s%s", widget_name, state_str, selstr);
#else
      wname = lives_strdup_printf("#%s%s%s", widget_name, selstr, state_str);
#endif
    } else {
      wname = lives_strdup_printf("%s%s%s", widget_name, selstr, state_str);
    }
    lives_free(selstr);
    if (selector && selector != xselector) lives_free(selector);

#ifdef GTK_TEXT_VIEW_CSS_BUG
  }
#endif

  if (widget_name) lives_free(widget_name);

  css_string = g_strdup_printf(" %s {\n %s: %s;}\n", wname, detail, value);

  if (show_css && !widget) g_print("running CSS %s\n", css_string);

#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                  css_string, -1);
#else
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                  css_string, -1, NULL);
#endif
  lives_free(wname);
  lives_free(css_string);
  lives_widget_object_unref(provider);
  return TRUE;
}


boolean set_css_value(LiVESWidget * widget, LiVESWidgetState state, const char *detail, const char *value) {
  if (state == GTK_STATE_FLAG_NORMAL) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_NORMAL, NULL, detail, value);
  if (state & GTK_STATE_FLAG_ACTIVE) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_ACTIVE, NULL, detail, value);
  if (state & GTK_STATE_FLAG_PRELIGHT) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_PRELIGHT, NULL, detail, value);
  if (state & GTK_STATE_FLAG_SELECTED) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_SELECTED, NULL, detail, value);
  if (state & GTK_STATE_FLAG_INSENSITIVE) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_INSENSITIVE, NULL, detail, value);
  if (state & GTK_STATE_FLAG_INCONSISTENT) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_INCONSISTENT, NULL, detail, value);
  if (state & GTK_STATE_FLAG_FOCUSED) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_FOCUSED, NULL, detail, value);
  if (state & GTK_STATE_FLAG_BACKDROP) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_BACKDROP, NULL, detail, value);
  if (state & GTK_STATE_FLAG_CHECKED) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_CHECKED, NULL, detail, value);
  return TRUE;
}
#endif
#endif


#ifdef GUI_GTK
boolean set_css_value_direct(LiVESWidget * widget, LiVESWidgetState state, const char *selector, const char *detail,
                             const char *value) {
#if GTK_CHECK_VERSION(3, 16, 0)

#if !GTK_CHECK_VERSION(3, 24, 0)
  if (!lives_strcmp(detail, "min-width")
      || !lives_strcmp(detail, "min-height")
      || !lives_strcmp(detail, "caret-color"))
    return FALSE;
#endif
  return set_css_value_for_state_flag(widget, state, selector, detail, value);
#endif
  return FALSE;
}
#endif


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_bg_color(LiVESWidget * widget, LiVESWidgetState state,
    const LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "background-color", colref);
  if (retb) retb = lives_widget_set_base_color(widget, state, color);
  lives_free(colref);
  return retb;
#else
  gtk_widget_override_background_color(widget, state, color);
  gtk_widget_modify_base(widget, state, color);
#endif
#else
  gtk_widget_modify_bg(widget, state, color);
  gtk_widget_modify_base(widget, state, color);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_fg_color(LiVESWidget * widget, LiVESWidgetState state,
    const LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "color", colref);
  lives_free(colref);
  return retb;
#else
  gtk_widget_override_color(widget, state, color);
#endif
#else
  gtk_widget_modify_text(widget, state, color);
  gtk_widget_modify_fg(widget, state, color);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_text_color(LiVESWidget * widget, LiVESWidgetState state,
    const LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "color", colref);
  lives_free(colref);
  return retb;
#else
  gtk_widget_override_color(widget, state, color);
#endif
#else
  gtk_widget_modify_text(widget, state, color);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_base_color(LiVESWidget * widget, LiVESWidgetState state,
    const LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "background", colref);
  lives_free(colref);
  return retb;
#else
  gtk_widget_override_color(widget, state, color);
#endif
#else
  gtk_widget_modify_base(widget, state, color);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_outline_color(LiVESWidget * widget, LiVESWidgetState state,
    const LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "outline-color", colref);
  lives_free(colref);
  return retb;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_border_color(LiVESWidget * widget, LiVESWidgetState state,
    const LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "border-color", colref);
  lives_free(colref);
  return retb;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_text_size(LiVESWidget * widget, LiVESWidgetState state,
    const char *tsize) {

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  char *xxsize;
  int xsize = capable->font_size;
  boolean retb;
  if (!strcmp(tsize, LIVES_TEXT_SIZE_LARGE)) xsize *= 1.2;
  else if (!strcmp(tsize, LIVES_TEXT_SIZE_X_LARGE)) xsize *= 1.4;
  else if (!strcmp(tsize, LIVES_TEXT_SIZE_XX_LARGE)) xsize *= 1.6;
  else if (!strcmp(tsize, LIVES_TEXT_SIZE_SMALL)) xsize *= .8;
  else if (!strcmp(tsize, LIVES_TEXT_SIZE_X_SMALL)) xsize *= .6;
  else if (!strcmp(tsize, LIVES_TEXT_SIZE_XX_SMALL)) xsize *= .4;
  else if (strcmp(tsize, LIVES_TEXT_SIZE_MEDIUM)) xsize = atoi(tsize);
  xxsize = lives_strdup_printf("%dpx", xsize);
  retb = set_css_value(widget, state, "font-size", xxsize);
  return retb;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_fg_state_color(LiVESWidget * widget, LiVESWidgetState state,
    LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_style_context_get_color(gtk_widget_get_style_context(widget), color);
#else
  gtk_style_context_get_color(gtk_widget_get_style_context(widget), lives_widget_get_state(widget), color);
#endif
#else
  lives_widget_color_copy(color, &gtk_widget_get_style(widget)->fg[LIVES_WIDGET_STATE_NORMAL]);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_bg_state_color(LiVESWidget * widget, LiVESWidgetState state,
    LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  LIVES_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color(gtk_widget_get_style_context(widget), lives_widget_get_state(widget), color);
  LIVES_IGNORE_DEPRECATIONS_END
#else
  lives_widget_color_copy(color, &gtk_widget_get_style(widget)->bg[LIVES_WIDGET_STATE_NORMAL]);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_color_equal(LiVESWidgetColor * c1, const LiVESWidgetColor * c2) {
#ifdef GUI_GTK
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (c1->alpha != c2->alpha) return FALSE;
#endif
  if (c1->red != c2->red || c1->green != c2->green || c1->blue != c2->blue) return FALSE;
  return TRUE;
#endif
  return FALSE;
}


boolean lives_widget_color_mix(LiVESWidgetColor * c1, const LiVESWidgetColor * c2, float mixval) {
  // c1 = mixval * c1 + (1. - mixval) * c2
  if (mixval < 0. || mixval > 1. || !c1 || !c2) return FALSE;
#ifdef GUI_GTK
  c1->red = (float)c1->red * mixval + (float)c2->red * (1. - mixval);
  c1->green = (float)c1->green * mixval + (float)c2->green * (1. - mixval);
  c1->blue = (float)c1->blue * mixval + (float)c2->blue * (1. - mixval);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor * c1, const LiVESWidgetColor * c2) {
  // if c1 is NULL, create a new copy of c2, otherwise copy c2 -> c1
  LiVESWidgetColor *c0 = NULL;
#ifdef GUI_GTK
  if (c1) {
    c1->red = c2->red;
    c1->green = c2->green;
    c1->blue = c2->blue;
#if GTK_CHECK_VERSION(3, 0, 0)
    c1->alpha = c2->alpha;
#else
    c1->pixel = c2->pixel;
#endif
  } else {
#if GTK_CHECK_VERSION(3, 0, 0)
    c0 = gdk_rgba_copy(c2);
#else
    c0 = gdk_color_copy(c2);
#endif
  }
#endif

  return c0;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_event_box_new(void) {
  LiVESWidget *eventbox = NULL;
#ifdef GUI_GTK
  eventbox = gtk_event_box_new();
#endif
  return eventbox;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_event_box_set_above_child(LiVESEventBox * ebox, boolean set) {
#ifdef GUI_GTK
  gtk_event_box_set_above_child(ebox, set);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_new(void) {
  LiVESWidget *image = NULL;
#ifdef GUI_GTK
  image = gtk_image_new();
#endif
  return image;
}


WIDGET_HELPER_LOCAL_INLINE int get_real_size_from_icon_size(LiVESIconSize size) {
  switch (size) {
  case LIVES_ICON_SIZE_SMALL_TOOLBAR:
  case LIVES_ICON_SIZE_BUTTON:
  case LIVES_ICON_SIZE_MENU:
    return 16;
  case LIVES_ICON_SIZE_LARGE_TOOLBAR:
    return 24;
  case LIVES_ICON_SIZE_DND:
    return 32;
  case LIVES_ICON_SIZE_DIALOG:
    return 48;
  default:
    break;
  }
  return -1;
}


LiVESPixbuf *lives_pixbuf_new_from_stock_at_size(const char *stock_id, LiVESIconSize size, int csize) {
  LiVESPixbuf *pixbuf = NULL;
  LiVESWidget *image = NULL;
  if (size == LIVES_ICON_SIZE_CUSTOM) {
    if (csize == get_real_size_from_icon_size(LIVES_ICON_SIZE_MENU)) size = LIVES_ICON_SIZE_MENU;
    if (csize == get_real_size_from_icon_size(LIVES_ICON_SIZE_SMALL_TOOLBAR))
      size = LIVES_ICON_SIZE_SMALL_TOOLBAR;
    if (csize == get_real_size_from_icon_size(LIVES_ICON_SIZE_LARGE_TOOLBAR))
      size = LIVES_ICON_SIZE_LARGE_TOOLBAR;
    if (csize == get_real_size_from_icon_size(LIVES_ICON_SIZE_BUTTON)) size = LIVES_ICON_SIZE_BUTTON;
    if (csize == get_real_size_from_icon_size(LIVES_ICON_SIZE_DND)) size = LIVES_ICON_SIZE_DND;
    if (csize == get_real_size_from_icon_size(LIVES_ICON_SIZE_DIALOG)) size = LIVES_ICON_SIZE_DIALOG;
  }

  if (size != LIVES_ICON_SIZE_CUSTOM) {
    if (lives_has_icon(widget_opts.icon_theme, stock_id, size)) {
#if GTK_CHECK_VERSION(3, 10, 0)
      pixbuf = gtk_icon_theme_load_icon((LiVESIconTheme *)widget_opts.icon_theme, stock_id,
                                        get_real_size_from_icon_size(size),
                                        GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
      return pixbuf;
#else
      image = gtk_image_new_from_stock(stock_id, size);
#endif
    }
    if (image) return lives_image_get_pixbuf(LIVES_IMAGE(image));
  } else {
#if GTK_CHECK_VERSION(3, 10, 0)
    if (csize > 0) {
      pixbuf = gtk_icon_theme_load_icon((LiVESIconTheme *)widget_opts.icon_theme, stock_id,
                                        csize, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
      if (pixbuf) return pixbuf;
    }
#endif
  }

  // custom size, or failed at specified size
  // try all sizes to see if we get one
  if (!image) {
    if (lives_has_icon(widget_opts.icon_theme, stock_id, LIVES_ICON_SIZE_DIALOG)) {
      size = LIVES_ICON_SIZE_DIALOG;
    } else if (lives_has_icon(widget_opts.icon_theme, stock_id, LIVES_ICON_SIZE_DND)) {
      size = LIVES_ICON_SIZE_DND;
    } else if (lives_has_icon(widget_opts.icon_theme, stock_id, LIVES_ICON_SIZE_LARGE_TOOLBAR)) {
      size = LIVES_ICON_SIZE_LARGE_TOOLBAR;
    } else if (lives_has_icon(widget_opts.icon_theme, stock_id, LIVES_ICON_SIZE_SMALL_TOOLBAR)) {
      size = LIVES_ICON_SIZE_SMALL_TOOLBAR;
    } else if (lives_has_icon(widget_opts.icon_theme, stock_id, LIVES_ICON_SIZE_BUTTON)) {
      size = LIVES_ICON_SIZE_BUTTON;
    } else if (lives_has_icon(widget_opts.icon_theme, stock_id, LIVES_ICON_SIZE_MENU)) {
      size = LIVES_ICON_SIZE_MENU;
    } else return NULL;

#if GTK_CHECK_VERSION(3, 10, 0)
    pixbuf = gtk_icon_theme_load_icon((LiVESIconTheme *)widget_opts.icon_theme, stock_id,
                                      get_real_size_from_icon_size(size),
                                      GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    return pixbuf;
#else
    image = gtk_image_new_from_stock(stock_id, size);
#endif
    if (!image) return NULL;
    pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(image));
  }
  return pixbuf;
}


LiVESWidget *lives_image_new_from_stock_at_size(const char *stock_id, LiVESIconSize size, int csize) {
  LiVESWidget *image = NULL;
  LiVESPixbuf *pixbuf = lives_pixbuf_new_from_stock_at_size(stock_id, size, csize);
  if (pixbuf) {
    image = lives_image_new_from_pixbuf(pixbuf);
    lives_widget_object_unref(pixbuf);
  }
  return image;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_new_from_stock(const char *stock_id,
    LiVESIconSize size) {
  return lives_image_new_from_stock_at_size(stock_id, size, get_real_size_from_icon_size(size));
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_new_from_file(const char *filename) {
  LiVESWidget *image = NULL;
#ifdef GUI_GTK
  image = gtk_image_new_from_file(filename);
#endif
  return image;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_new_from_pixbuf(LiVESPixbuf * pixbuf) {
  LiVESWidget *image = NULL;
#ifdef GUI_GTK
  image = gtk_image_new_from_pixbuf(pixbuf);
#endif
  return image;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_image_set_from_pixbuf(LiVESImage * image, LiVESPixbuf * pixbuf) {
#ifdef GUI_GTK
  gtk_image_set_from_pixbuf(image, pixbuf);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_image_get_pixbuf(LiVESImage * image) {
  LiVESPixbuf *pixbuf = NULL;
#ifdef GUI_GTK
  pixbuf = gtk_image_get_pixbuf(image);
#endif
  return pixbuf;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_parse(const char *spec, LiVESWidgetColor * color) {
  boolean retval = FALSE;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  retval = gdk_rgba_parse(color, spec);
#else
  retval = gdk_color_parse(spec, color);
#endif
#endif
  return retval;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_dialog_get_content_area(LiVESDialog * dialog) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2, 14, 0)
  return gtk_dialog_get_content_area(LIVES_DIALOG(dialog));
#else
  return LIVES_DIALOG(dialog)->vbox;
#endif
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_dialog_get_action_area(LiVESDialog * dialog) {
  return lives_widget_object_get_data(LIVES_WIDGET_OBJECT(dialog), ACTION_AREA_KEY);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_dialog_get_action_area(LiVESDialog * dialog) {
#ifdef GUI_GTK
  if (is_standard_widget(LIVES_WIDGET(dialog))) return lives_standard_dialog_get_action_area(dialog);
#if GTK_CHECK_VERSION(2, 14, 0)
#ifdef LIVES_IGNORE_DEPRECATIONS
  LIVES_IGNORE_DEPRECATIONS
#endif
  return gtk_dialog_get_action_area(LIVES_DIALOG(dialog));
#ifdef LIVES_IGNORE_DEPRECATIONS_END
  LIVES_IGNORE_DEPRECATIONS_END
#endif
#else
  return LIVES_DIALOG(dialog)->vbox;
#endif
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin_left(LiVESWidget * widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 12, 0)
  gtk_widget_set_margin_start(widget, margin);
#else
  gtk_widget_set_margin_left(widget, margin);
#endif
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin_right(LiVESWidget * widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 12, 0)
  gtk_widget_set_margin_end(widget, margin);
#else
  gtk_widget_set_margin_right(widget, margin);
#endif
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin_top(LiVESWidget * widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_margin_top(widget, margin);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin_bottom(LiVESWidget * widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_margin_bottom(widget, margin);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin(LiVESWidget * widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_set_margin_bottom(widget, margin);
  lives_widget_set_margin_top(widget, margin);
  lives_widget_set_margin_left(widget, margin);
  lives_widget_set_margin_right(widget, margin);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_padding(LiVESWidget * widget, int padding) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  char *wpx = lives_strdup_printf("%dpx", padding);
  set_css_value_direct(widget, LIVES_WIDGET_STATE_NORMAL, "", "padding", wpx);
  lives_free(wpx);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_LOCAL_INLINE void _dialog_resp(LiVESWidget * w, LiVESDialog * dlg) {
  lives_dialog_response(dlg, GET_INT_DATA(w, RESPONSE_KEY));
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_dialog_add_action_widget(LiVESDialog * dialog,
    LiVESWidget * widget,
    LiVESResponseType response) {
  // TODO: use lives_dialog_add_button, lives_dialog_add_button_from_stock
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_set_margin_left(widget, widget_opts.packing_width / 2);
  lives_widget_set_margin_right(widget, widget_opts.packing_width / 2);
#endif
  if (is_standard_widget(LIVES_WIDGET(dialog))) {
    LiVESWidget *action = lives_standard_dialog_get_action_area(dialog);
    lives_box_pack_start(LIVES_BOX(action), widget, FALSE, FALSE, 0);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), RESPONSE_KEY,
                                 LIVES_INT_TO_POINTER(response));
    lives_signal_sync_connect(widget, LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(_dialog_resp), dialog);
  } else gtk_dialog_add_action_widget(dialog, widget, response);
  lives_box_set_spacing(LIVES_BOX(lives_widget_get_parent(widget)), widget_opts.packing_width * 4);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_window_new(LiVESWindowType wintype) {
  LiVESWidget *window = NULL;
#ifdef GUI_GTK
  window = gtk_window_new(wintype);
#endif
  return window;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_title(LiVESWindow * window, const char *title) {
#ifdef GUI_GTK
  char *ntitle;
  if (*widget_opts.title_prefix) {
    ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  } else ntitle = lives_strdup(title);
  if (GTK_IS_WINDOW(window)) gtk_window_set_title(window, ntitle);
  lives_free(ntitle);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_transient_for(LiVESWindow * window, LiVESWindow * parent) {
#ifdef GUI_GTK
  lives_widget_object_ref(window);
  gtk_window_set_transient_for(window, parent);
  lives_widget_object_unref(window);
  return TRUE;
#endif
  return FALSE;
}


static void modunmap(LiVESWindow * win, livespointer data) {if (win == modalw) {set_gui_loop_tight(FALSE); modalw = NULL;}}
static void moddest(LiVESWindow * win, livespointer data) {if (win == modalw) {set_gui_loop_tight(FALSE); modalw = NULL;}}
static boolean moddelete(LiVESWindow * win, LiVESXEvent * event, livespointer data) {
  if (win == modalw) {set_gui_loop_tight(FALSE); modalw = NULL;}
  return TRUE;
}

static boolean _lives_window_set_modal(LiVESWindow * window, boolean modal, boolean no_slack) {
  if (modal) {
    if (modalw) return FALSE;
    lives_signal_sync_connect(window, LIVES_WIDGET_DELETE_EVENT,
                              LIVES_GUI_CALLBACK(moddelete), NULL);
    lives_signal_sync_connect(window, LIVES_WIDGET_DESTROY_SIGNAL,
                              LIVES_GUI_CALLBACK(moddest), NULL);
    lives_signal_sync_connect(window, LIVES_WIDGET_UNMAP_SIGNAL,
                              LIVES_GUI_CALLBACK(modunmap), NULL);
    modalw = window;
    set_gui_loop_tight(TRUE);
  } else {
    if (!modalw) return FALSE;
    lives_signal_handlers_sync_disconnect_by_func(LIVES_GUI_OBJECT(modalw), LIVES_GUI_CALLBACK(moddest), NULL);
    lives_signal_handlers_sync_disconnect_by_func(LIVES_GUI_OBJECT(modalw), LIVES_GUI_CALLBACK(moddelete), NULL);
    lives_signal_handlers_sync_disconnect_by_func(LIVES_GUI_OBJECT(modalw), LIVES_GUI_CALLBACK(modunmap), NULL);
    modalw = NULL;
    if (!no_slack) set_gui_loop_tight(FALSE);
  }

#ifdef GUI_GTK
  if (GTK_IS_WINDOW(window)) gtk_window_set_modal(window, modal);
  return TRUE;
#endif
  return FALSE;
}


boolean lives_window_set_modal(LiVESWindow * window, boolean modal) {
  return _lives_window_set_modal(window, modal, FALSE);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_get_modal(LiVESWindow * window) {
#ifdef GUI_GTK
  return gtk_window_get_modal(window);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_deletable(LiVESWindow * window, boolean deletable) {
#ifdef GUI_GTK
  gtk_window_set_deletable(window, deletable);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_resizable(LiVESWindow * window, boolean resizable) {
#ifdef GUI_GTK
  gtk_window_set_resizable(window, resizable);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_keep_below(LiVESWindow * window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_keep_below(window, set);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_keep_above(LiVESWindow * window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_keep_above(window, set);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_decorated(LiVESWindow * window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_decorated(window, set);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_auto_startup_notification(boolean set) {
#ifdef GUI_GTK
  gtk_window_set_auto_startup_notification(set);
  return TRUE;
#endif
  return FALSE;
}


/* WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_screen(LiVESWindow *window, LiVESXScreen *screen) { */
/*   if (LIVES_IS_WINDOW(window)) { */
/* #ifdef GUI_GTK */
/*     gtk_window_set_screen(window, screen); */
/*     return TRUE; */
/* #endif */
/*   } */
/*   return FALSE; */
/* } */


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_monitor(LiVESWindow * window, int monnum) {
#ifdef GUI_GTK
  if (LIVES_IS_WINDOW(window)) {
#if !GTK_CHECK_VERSION(3, 20, 0)
    gtk_window_set_screen(window, mainw->mgeom[monnum].screen);
#else
    gtk_window_fullscreen_on_monitor(window, mainw->mgeom[monnum].screen, monnum);
    gtk_window_unfullscreen(window);
#endif
    return TRUE;
  }
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_default_size(LiVESWindow * window, int width, int height) {
#ifdef GUI_GTK
  if (!mainw->ignore_screen_size) {
    if (width > GUI_SCREEN_WIDTH || height > GUI_SCREEN_HEIGHT) abort();
  } else {
    if (width > GUI_SCREEN_PHYS_WIDTH || height > GUI_SCREEN_PHYS_HEIGHT) abort();
  }
  gtk_window_set_default_size(window, width, height);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_window_get_title(LiVESWindow * window) {
#ifdef GUI_GTK
  return gtk_window_get_title(window);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_move(LiVESWindow * window, int x, int y) {
#ifdef GUI_GTK
  gtk_window_move(window, x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_move_resize(LiVESWindow * window, int x, int y, int w, int h) {
#ifdef GUI_GTK
  if (!mainw->ignore_screen_size) {
    if (w + x > GUI_SCREEN_WIDTH || h + y > GUI_SCREEN_HEIGHT) abort();
  } else {
    if (w + x > GUI_SCREEN_PHYS_WIDTH || h + y > GUI_SCREEN_PHYS_HEIGHT) abort();
  }
  gdk_window_move_resize(lives_widget_get_xwindow(LIVES_WIDGET(window)), x, y, w, h);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_position(LiVESWidget * widget, int *x, int *y) {
#ifdef GUI_GTK
  GdkWindow *window = lives_widget_get_xwindow(widget);
  if (x) *x = 0;
  if (y) *y = 0;
  if (GDK_IS_WINDOW(window))
    gdk_window_get_position(window, x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_get_position(LiVESWindow * window, int *x, int *y) {
#ifdef GUI_GTK
  gtk_window_get_position(window, x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_position(LiVESWindow * window, LiVESWindowPosition pos) {
#ifdef GUI_GTK
  gtk_window_set_position(window, pos);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_hide_titlebar_when_maximized(LiVESWindow * window, boolean setting) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_window_set_hide_titlebar_when_maximized(window, setting);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_resize(LiVESWindow * window, int width, int height) {
#ifdef GUI_GTK
  if (!mainw->ignore_screen_size) {
    if (width > GUI_SCREEN_WIDTH || height > GUI_SCREEN_HEIGHT) abort();
  } else {
    if (width > GUI_SCREEN_PHYS_WIDTH || height > GUI_SCREEN_PHYS_HEIGHT) abort();
  }
  gtk_window_resize(window, width, height);
  gtk_widget_set_size_request(GTK_WIDGET(window), width, height);
  return TRUE;
#endif
  // TODO
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_present(LiVESWindow * window) {
#ifdef GUI_GTK
  uint32_t tstamp = gtk_get_current_event_time();
  gtk_window_present_with_time(window, tstamp);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_fullscreen(LiVESWindow * window) {
#ifdef GUI_GTK
  gtk_window_fullscreen(window);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_unfullscreen(LiVESWindow * window) {
#ifdef GUI_GTK
  gtk_window_unfullscreen(window);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_maximize(LiVESWindow * window) {
#ifdef GUI_GTK
  gtk_window_maximize(window);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_unmaximize(LiVESWindow * window) {
#ifdef GUI_GTK
  gtk_window_unmaximize(window);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_window_get_focus(LiVESWindow * window) {
#ifdef GUI_GTK
  return gtk_window_get_focus(window);
#endif
  return NULL;
}


static boolean _lives_widget_process_updates(LiVESWidget * widget) {
#ifdef GUI_GTK
  LiVESWindow *win, *modalold = modalw;
  boolean was_modal = TRUE;
  boolean no_slack = FALSE;

  if (LIVES_IS_WINDOW(widget)) win = (LiVESWindow *)widget;
  else if (LIVES_IS_WIDGET(widget))
    win = lives_widget_get_window(widget);
  else return FALSE;
  if (win && LIVES_IS_WINDOW(win)) {
    was_modal = lives_window_get_modal(win);
    if (!was_modal) {
      no_slack = gui_loop_tight;
      lives_window_set_modal(win, TRUE);
    }
  }

  _lives_widget_context_update();

  if (!was_modal) {
    if (win) {
      _lives_window_set_modal(win, FALSE, no_slack);
    }
    if (modalold) lives_window_set_modal(modalold, TRUE);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw_noblock(LiVESWidget * widget) {
#ifdef GUI_GTK
  if (!LIVES_IS_WIDGET(widget)) {
    LIVES_WARN("Draw queue invalid widget");
    return FALSE;
  }
  if (is_fg_thread()) gtk_widget_queue_draw(widget);
  else {
    BG_THREADVAR(hook_hints) = HOOK_CB_PRIORITY | HOOK_CB_TRANSFER_OWNER;
    MAIN_THREAD_EXECUTE_RVOID(gtk_widget_queue_draw, "v", widget);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw_update_noblock(LiVESWidget * widget) {
#ifdef GUI_GTK
  if (!LIVES_IS_WIDGET(widget)) {
    LIVES_WARN("Draw queue invalid widget");
    return FALSE;
  }
  if (is_fg_thread()) _lives_widget_queue_draw_and_update(widget);
  else {
    BG_THREADVAR(hook_hints) |= HOOK_CB_PRIORITY  | HOOK_CB_TRANSFER_OWNER;
    MAIN_THREAD_EXECUTE_RVOID(_lives_widget_queue_draw_and_update, "v", widget);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_process_updates(LiVESWidget * widget) {
  boolean ret;
  if (is_fg_thread()) return _lives_widget_process_updates(widget);
  BG_THREADVAR(hook_hints) = HOOK_CB_BLOCK | HOOK_CB_PRIORITY | HOOK_UNIQUE_DATA;
  main_thread_execute(_lives_widget_process_updates, WEED_SEED_BOOLEAN, &ret, "v", widget);
  BG_THREADVAR(hook_hints) = 0;
  return ret;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAccelGroup *lives_accel_group_new(void) {
  LiVESAccelGroup *group = NULL;
#ifdef GUI_GTK
  group = gtk_accel_group_new();
#endif
  return group;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_accel_group_connect(LiVESAccelGroup * group, uint32_t key, LiVESXModifierType mod,
    LiVESAccelFlags flags, LiVESWidgetClosure * closure) {
#ifdef GUI_GTK
  gtk_accel_group_connect(group, key, mod, flags, closure);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_accel_group_disconnect(LiVESAccelGroup * group, LiVESWidgetClosure * closure) {
#ifdef GUI_GTK
  gtk_accel_group_disconnect(group, closure);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_add_accelerator(LiVESWidget * widget, const char *accel_signal,
    LiVESAccelGroup * accel_group,
    uint32_t accel_key, LiVESXModifierType accel_mods, LiVESAccelFlags accel_flags) {
#ifdef GUI_GTK
  gtk_widget_add_accelerator(widget, accel_signal, accel_group, accel_key, accel_mods, accel_flags);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_add_accel_group(LiVESWindow * window, LiVESAccelGroup * group) {
#ifdef GUI_GTK
  gtk_window_add_accel_group(window, group);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_has_focus(LiVESWidget * widget) {
  /// physical
#ifdef GUI_GTK
  return gtk_widget_has_focus(widget);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_focus(LiVESWidget * widget) {
  /// logical
#ifdef GUI_GTK
  return gtk_widget_is_focus(widget);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_has_default(LiVESWidget * widget) {
#ifdef GUI_GTK
  return gtk_widget_has_default(widget);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_remove_accel_group(LiVESWindow * window, LiVESAccelGroup * group) {
#ifdef GUI_GTK
  gtk_window_remove_accel_group(window, group);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_set_accel_group(LiVESMenu * menu, LiVESAccelGroup * group) {
#ifdef GUI_GTK
  gtk_menu_set_accel_group(menu, group);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_accel_groups_activate(LiVESWidgetObject * object, uint32_t key,
    LiVESXModifierType mod) {
#ifdef GUI_GTK
  gtk_accel_groups_activate(object, key, mod);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height) {
#ifdef GUI_GTK
  // alpha fmt is RGBA post mult
  return gdk_pixbuf_new(GDK_COLORSPACE_RGB, has_alpha, 8, width, height);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_pixbuf_copy(LiVESPixbuf * orig) {
#ifdef GUI_GTK
  // alpha fmt is RGBA post mult
  return gdk_pixbuf_copy(orig);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE
LiVESPixbuf *lives_pixbuf_new_from_data(const unsigned char *buf, boolean has_alpha, int width,
                                        int height, int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn,
                                        livespointer destroy_fn_data) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_data((const guchar *)buf, GDK_COLORSPACE_RGB, has_alpha,
                                  8, width, height, rowstride, lives_free_buffer_fn, destroy_fn_data);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE
LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file(filename, error);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE
LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height,
    boolean preserve_aspect_ratio, LiVESError **error) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file_at_scale(filename, width, height, preserve_aspect_ratio, error);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_pixbuf_get_rowstride(const LiVESPixbuf * pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_rowstride(pixbuf);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE int lives_pixbuf_get_width(const LiVESPixbuf * pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_width(pixbuf);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE int lives_pixbuf_get_height(const LiVESPixbuf * pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_height(pixbuf);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE int lives_pixbuf_get_n_channels(const LiVESPixbuf * pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_n_channels(pixbuf);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf * pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_pixels(pixbuf);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf * pixbuf) {
#ifdef GUI_GTK
  return (const guchar *)gdk_pixbuf_get_pixels(pixbuf);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf * pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_has_alpha(pixbuf);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf * src, int dest_width, int dest_height,
    LiVESInterpType interp_type) {
#ifdef GUI_GTK
  return gdk_pixbuf_scale_simple(src, dest_width, dest_height, interp_type);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE
boolean lives_pixbuf_saturate_and_pixelate(const LiVESPixbuf * src, LiVESPixbuf * dest,
    float saturation, boolean pixelate) {
#ifdef GUI_GTK
  gdk_pixbuf_saturate_and_pixelate(src, dest, saturation, pixelate);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE
LiVESAdjustment *lives_adjustment_new(double value, double lower, double upper,
                                      double step_increment, double page_increment, double page_size) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  adj = gtk_adjustment_new(value, lower, upper, step_increment, page_increment, page_size);
#else
  adj = GTK_ADJUSTMENT(gtk_adjustment_new(value, lower, upper, step_increment, page_increment, page_size));
#endif
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_set_homogeneous(LiVESBox * box, boolean homogeneous) {
#ifdef GUI_GTK
  gtk_box_set_homogeneous(box, homogeneous);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_reorder_child(LiVESBox * box, LiVESWidget * child, int pos) {
#ifdef GUI_GTK
  gtk_box_reorder_child(box, child, pos);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_set_child_packing(LiVESBox * box, LiVESWidget * child, boolean expand,
    boolean fill,
    uint32_t padding, LiVESPackType pack_type) {
#ifdef GUI_GTK
  gtk_box_set_child_packing(box, child, expand, fill, padding, pack_type);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_set_spacing(LiVESBox * box, int spacing) {
#ifdef GUI_GTK
  gtk_box_set_spacing(box, spacing);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hbox_new(boolean homogeneous, int spacing) {
  LiVESWidget *hbox = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hbox = gtk_box_new(LIVES_ORIENTATION_HORIZONTAL, spacing);
  lives_box_set_homogeneous(LIVES_BOX(hbox), homogeneous);
#else
  hbox = gtk_hbox_new(homogeneous, spacing);
#endif
#endif
  //if (hbox && LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox)) lives_widget_set_hexpand(hbox, TRUE);
  return hbox;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vbox_new(boolean homogeneous, int spacing) {
  LiVESWidget *vbox = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vbox = gtk_box_new(LIVES_ORIENTATION_VERTICAL, spacing);
  lives_box_set_homogeneous(LIVES_BOX(vbox), homogeneous);
#else
  vbox = gtk_vbox_new(homogeneous, spacing);
#endif
#endif
  //if (vbox && LIVES_SHOULD_EXPAND_EXTRA_FOR(vbox)) lives_widget_set_vexpand(vbox, TRUE);
  return vbox;
}


WIDGET_HELPER_GLOBAL_INLINE
boolean lives_box_pack_start(LiVESBox * box, LiVESWidget * child, boolean expand, boolean fill,
                             uint32_t padding) {
#ifdef GUI_GTK
  if (1 || is_fg_thread())
    gtk_box_pack_start(box, child, expand, fill, padding);
  else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(gtk_box_pack_start, "vvbbi", box, child, expand, fill, padding);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE
boolean lives_box_pack_end(LiVESBox * box, LiVESWidget * child, boolean expand, boolean fill,
                           uint32_t padding) {
#ifdef GUI_GTK
  gtk_box_pack_end(box, child, expand, fill, padding);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hseparator_new(void) {
  LiVESWidget *hsep = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hsep = gtk_separator_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  hsep = gtk_hseparator_new();
#endif
  lives_widget_set_size_request(hsep, -1, 1);
#endif
  return hsep;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vseparator_new(void) {
  LiVESWidget *vsep = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vsep = gtk_separator_new(LIVES_ORIENTATION_VERTICAL);
#else
  vsep = gtk_vseparator_new();
#endif
  lives_widget_set_size_request(vsep, 1, -1);
#endif
  return vsep;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hbutton_box_new(void) {
  LiVESWidget *bbox = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  bbox = gtk_button_box_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  bbox = gtk_hbutton_box_new();
#endif
#endif
  return bbox;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vbutton_box_new(void) {
  LiVESWidget *bbox = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  bbox = gtk_button_box_new(LIVES_ORIENTATION_VERTICAL);
#else
  bbox = gtk_vbutton_box_new();
#endif
#endif
  return bbox;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_box_set_layout(LiVESButtonBox * bbox, LiVESButtonBoxStyle bstyle) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 12, 0)
  if (bstyle == LIVES_BUTTONBOX_EXPAND) {
    gtk_box_set_homogeneous(GTK_BOX(bbox), TRUE);
    gtk_box_set_spacing(GTK_BOX(bbox), 0);
    return TRUE;
  }
#endif
  gtk_button_box_set_layout(bbox, bstyle);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE
boolean lives_button_box_set_child_non_homogeneous(LiVESButtonBox * bbox, LiVESWidget * child, boolean set) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 2, 0)
  gtk_button_box_set_child_non_homogeneous(bbox, child, set);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vscale_new(LiVESAdjustment * adj) {
  LiVESWidget *vscale = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vscale = gtk_scale_new(LIVES_ORIENTATION_VERTICAL, adj);
#else
  vscale = gtk_vscale_new(adj);
#endif
#endif
  return vscale;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hpaned_new(void) {
  LiVESWidget *hpaned = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hpaned = gtk_paned_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  hpaned = gtk_hpaned_new();
#endif
#endif
  return hpaned;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vpaned_new(void) {
  LiVESWidget *vpaned = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vpaned = gtk_paned_new(LIVES_ORIENTATION_VERTICAL);
#else
  vpaned = gtk_vpaned_new();
#endif
#endif
  return vpaned;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hscrollbar_new(LiVESAdjustment * adj) {
  LiVESWidget *hscrollbar = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hscrollbar = gtk_scrollbar_new(LIVES_ORIENTATION_HORIZONTAL, adj);
#else
  hscrollbar = gtk_hscrollbar_new(adj);
#endif
#endif
  return hscrollbar;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vscrollbar_new(LiVESAdjustment * adj) {
  LiVESWidget *vscrollbar = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vscrollbar = gtk_scrollbar_new(LIVES_ORIENTATION_VERTICAL, adj);
#else
  vscrollbar = gtk_vscrollbar_new(adj);
#endif
#endif
  return vscrollbar;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_label_new(const char *text) {
  LiVESWidget *label = NULL;
#ifdef GUI_GTK
  label = gtk_label_new(text);
  gtk_label_set_use_underline(LIVES_LABEL(label), widget_opts.mnemonic_label);
  gtk_label_set_justify(LIVES_LABEL(label), widget_opts.justify);
  gtk_label_set_line_wrap(LIVES_LABEL(label), widget_opts.line_wrap);
#endif
  return label;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_arrow_new(LiVESArrowType arrow_type, LiVESShadowType shadow_type) {
  LiVESWidget *arrow = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 12, 0)
  const char *format = "<b>%s</b>";
  char *markup;
  char *str;

  switch (arrow_type) {
  case LIVES_ARROW_DOWN:
    str = "v";
    break;
  case LIVES_ARROW_LEFT:
    str = "<";
    break;
  case LIVES_ARROW_RIGHT:
    str = ">";
    break;
  default:
    return arrow;
  }

  arrow = gtk_label_new("");
  markup = lives_markup_printf_escaped(format, str);
  gtk_label_set_markup(GTK_LABEL(arrow), markup);
  lives_free(markup);

#else
  arrow = gtk_arrow_new(arrow_type, shadow_type);
#endif
#endif
  return arrow;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_halign(LiVESWidget * widget, LiVESAlign align) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (LIVES_IS_LABEL(widget)) {
    if (align == LIVES_ALIGN_START) gtk_label_set_xalign(LIVES_LABEL(widget), 0.);
    if (align == LIVES_ALIGN_CENTER) gtk_label_set_xalign(LIVES_LABEL(widget), 0.5);
    if (align == LIVES_ALIGN_END) gtk_label_set_xalign(LIVES_LABEL(widget), 1.);
  } else gtk_widget_set_halign(widget, align);
#else
  if (LIVES_IS_LABEL(widget)) {
    float xalign, yalign;
    gtk_misc_get_alignment(GTK_MISC(widget), &xalign, &yalign);
    switch (align) {
    case LIVES_ALIGN_START:
      gtk_misc_set_alignment(GTK_MISC(widget), 0., yalign);
      break;
    case LIVES_ALIGN_END:
      gtk_misc_set_alignment(GTK_MISC(widget), 1., yalign);
      break;
    case LIVES_ALIGN_CENTER:
      gtk_misc_set_alignment(GTK_MISC(widget), 0.5, yalign);
      break;
    default:
      return FALSE;
    }
    return TRUE;
  }
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_valign(LiVESWidget * widget, LiVESAlign align) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_valign(widget, align);
#else
  if (!LIVES_IS_LABEL(widget)) return FALSE;
  else {
    float xalign, yalign;
    gtk_misc_get_alignment(GTK_MISC(widget), &xalign, &yalign);
    switch (align) {
    case LIVES_ALIGN_START:
      gtk_misc_set_alignment(GTK_MISC(widget), xalign, 0.);
      break;
    case LIVES_ALIGN_END:
      gtk_misc_set_alignment(GTK_MISC(widget), xalign, 1.);
      break;
    case LIVES_ALIGN_CENTER:
      gtk_misc_set_alignment(GTK_MISC(widget), xalign, 0.5);
      break;
    default:
      return FALSE;
    }
  }
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_alignment_new(float xalign, float yalign, float xscale, float yscale) {
  LiVESWidget *alignment = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  alignment = gtk_aspect_frame_new(NULL, xalign, yalign, xscale / yscale, TRUE);
  lives_frame_set_shadow_type(LIVES_FRAME(alignment), LIVES_SHADOW_NONE);
#else
  alignment = gtk_alignment_new(xalign, yalign, xscale, yscale);
#endif
#endif
  return alignment;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_alignment_set(LiVESWidget * alignment, float xalign, float yalign, float xscale,
    float yscale) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_aspect_frame_set(GTK_ASPECT_FRAME(alignment), xalign, yalign, xscale / yscale, TRUE);
#else
  gtk_alignment_set(LIVES_ALIGNMENT(alignment), xalign, yalign, xscale, yscale);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_expander_new(const char *label) {
  LiVESWidget *expander = NULL;
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) expander = gtk_expander_new(label);
  else expander = gtk_expander_new_with_mnemonic(label);
#if GTK_CHECK_VERSION(3, 2, 0)
  if (LIVES_SHOULD_EXPAND)
    gtk_expander_set_resize_toplevel(GTK_EXPANDER(expander), TRUE);
#endif
#endif
  return expander;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_expander_get_label_widget(LiVESExpander * expander) {
  LiVESWidget *widget = NULL;
#ifdef GUI_GTK
  widget = gtk_expander_get_label_widget(expander);
#endif
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_expander_set_use_markup(LiVESExpander * expander, boolean val) {
#ifdef GUI_GTK
  gtk_expander_set_use_markup(expander, val);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_expander_set_expanded(LiVESExpander * expander, boolean val) {
#ifdef GUI_GTK
  gtk_expander_set_expanded(expander, val);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_expander_set_label(LiVESExpander * expander, const char *text) {

#ifdef GUI_GTK
  char *labeltext = lives_strdup_printf("<big>%s</big>", text);
  gtk_expander_set_label(expander, labeltext);
  lives_free(labeltext);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_expander_get_expanded(LiVESExpander * expander) {
#ifdef GUI_GTK
  return gtk_expander_get_expanded(expander);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_width_chars(LiVESLabel * label, int nchars) {
#ifdef GUI_GTK
  gtk_label_set_width_chars(label, nchars);
  gtk_label_set_max_width_chars(label, nchars);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_line_wrap(LiVESLabel * label, boolean set) {
#ifdef GUI_GTK
  gtk_label_set_line_wrap(label, set);
  return TRUE;
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_line_wrap_mode(LiVESLabel * label, LingoWrapMode mode) {
#ifdef GUI_GTK
  gtk_label_set_line_wrap_mode(label, mode);
  return TRUE;
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_lines(LiVESLabel * label, int nlines) {
#ifdef GUI_GTK
  gtk_label_set_lines(label, nlines);
  return TRUE;
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_ellipsize(LiVESLabel * label, LiVESEllipsizeMode mode) {
#ifdef GUI_GTK
  gtk_label_set_ellipsize(label, mode);
  return TRUE;
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_halignment(LiVESLabel * label, float xalign) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  gtk_label_set_xalign(label, xalign);
#else
  if (xalign == 0.)
    lives_widget_set_halign(LIVES_WIDGET(label), LIVES_ALIGN_START);
  else if (xalign == 1.)
    lives_widget_set_halign(LIVES_WIDGET(label), LIVES_ALIGN_END);
  else
    lives_widget_set_halign(LIVES_WIDGET(label), LIVES_ALIGN_CENTER);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_combo_new(void) {
  LiVESWidget *combo = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  combo = gtk_combo_box_new_with_entry();
#else
  combo = gtk_combo_box_entry_new_text();
#endif
#endif
  return combo;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_combo_new_with_model(LiVESTreeModel * model) {
  LiVESWidget *combo = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  combo = gtk_combo_box_new_with_model_and_entry(model);
#else
  combo = gtk_combo_box_entry_new();
  gtk_combo_box_set_model(GTK_COMBO_BOX(combo), model);
#endif
#endif
  return combo;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeModel *lives_combo_get_model(LiVESCombo * combo) {
  LiVESTreeModel *model = NULL;
#ifdef GUI_GTK
  model = gtk_combo_box_get_model(combo);
#endif
  return model;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_model(LiVESCombo * combo, LiVESTreeModel * model) {
#ifdef GUI_GTK
  gtk_combo_box_set_model(combo, model);
  return TRUE;
#endif
  return FALSE;
}


void lives_combo_popup(LiVESCombo * combo) {
  // used in callback, so no inline
#ifdef GUI_GTK
  gtk_combo_box_popup(combo);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_focus_on_click(LiVESCombo * combo, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 20, 0)
  gtk_widget_set_focus_on_click(GTK_WIDGET(combo), state);
#else
  gtk_combo_box_set_focus_on_click(combo, state);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_append_text(LiVESCombo * combo, const char *text) {
#ifdef GUI_GTK
  LiVESTreeModel *tmodel = lives_combo_get_model(combo);
  if (!tmodel) {
    LiVESListStore *lstore = lives_list_store_new(1, LIVES_COL_TYPE_STRING);
    lives_combo_set_model(combo, (tmodel = LIVES_TREE_MODEL(lstore)));
    lives_combo_set_entry_text_column(combo, 0);
  }
  if (!LIVES_IS_LIST_STORE(tmodel)) return FALSE;
  else {
    LiVESTreeIter iter;
    LiVESListStore *lstore = LIVES_LIST_STORE(tmodel);
    lives_list_store_append(lstore, &iter);   /* Acquire an iterator */
    lives_list_store_set(lstore, &iter, 0, text, -1);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_prepend_text(LiVESCombo * combo, const char *text) {
#ifdef GUI_GTK
  LiVESTreeModel *tmodel = lives_combo_get_model(combo);
  if (!tmodel) {
    LiVESListStore *lstore = lives_list_store_new(1, LIVES_COL_TYPE_STRING);
    lives_combo_set_model(combo, (tmodel = LIVES_TREE_MODEL(lstore)));
    lives_combo_set_entry_text_column(combo, 0);
  }
  if (!LIVES_IS_LIST_STORE(tmodel)) return FALSE;
  else {
    LiVESTreeIter iter;
    LiVESListStore *lstore = LIVES_LIST_STORE(tmodel);
    lives_list_store_prepend(lstore, &iter);   /* Acquire an iterator */
    lives_list_store_set(lstore, &iter, 0, text, -1);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_combo_get_n_entries(LiVESCombo * combo) {
  int nnodes = -1;
#ifdef GUI_GTK
  LiVESTreeModel *tmodel = lives_combo_get_model(combo);
  nnodes = gtk_tree_model_iter_n_children(tmodel, NULL);
#endif
  return nnodes;
}


boolean lives_combo_remove_text(LiVESCombo * combo, const char *text) {
#ifdef GUI_GTK
  LiVESTreeModel *tmodel = lives_combo_get_model(combo);
  if (GTK_IS_LIST_STORE(tmodel)) {
    LiVESTreeIter iter;
    if (lives_tree_model_get_iter_first(tmodel, &iter)) {
      LiVESTreeStore *xtmodel = lives_tree_store_new(1, LIVES_COL_TYPE_STRING);
      LiVESTreeIter xiter;
      char *ret;
      do {
        gtk_tree_model_get(LIVES_TREE_MODEL(tmodel), &iter, 0, &ret, -1);
        if (strcmp(ret, text)) {
          lives_tree_store_append(xtmodel, &xiter, NULL);
          lives_tree_store_set(xtmodel, &xiter, 0, ret, -1);
        }
      }	while (gtk_tree_model_iter_next(tmodel, &iter));
      lives_combo_set_model(combo, LIVES_TREE_MODEL(xtmodel));
    }
  }
#endif
  return FALSE;
}

boolean lives_combo_remove_all_text(LiVESCombo * combo) {
#ifdef GUI_GTK
  LiVESTreeModel *tmodel = lives_combo_get_model(combo);
  if (GTK_IS_TREE_STORE(tmodel)) {
    LiVESTreeStore *tstore = GTK_TREE_STORE(tmodel);
    gtk_tree_store_clear(tstore);
  } else if (GTK_IS_LIST_STORE(tmodel)) {
    LiVESListStore *lstore = GTK_LIST_STORE(tmodel);
    /// block CHANGED signal else it gets called for EVERY SINGLE removed element !
    //uint32_t sigid = g_signal_lookup(LIVES_WIDGET_CHANGED_SIGNAL, GTK_TYPE_COMBO_BOX);
    // does NOT WORK ! bug in glib / gtk ?
    //g_signal_handlers_block_matched(combo, G_SIGNAL_MATCH_ID, sigid, 0, NULL, NULL, NULL);
    gtk_list_store_clear(lstore);
    //g_signal_handlers_unblock_matched(combo, G_SIGNAL_MATCH_ID, sigid, 0, NULL, NULL, NULL);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_entry_text_column(LiVESCombo * combo, int column) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo), column);
#else
  gtk_combo_box_entry_set_text_column(GTK_COMBO_BOX_ENTRY(combo), column);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_combo_get_active_text(LiVESCombo * combo) {
  // return value should be freed
#ifdef GUI_GTK
  return lives_entry_get_text(LIVES_ENTRY(lives_bin_get_child(LIVES_BIN(combo))));
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_active_index(LiVESCombo * combo, int index) {
#ifdef GUI_GTK
  gtk_combo_box_set_active(combo, index);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_active_iter(LiVESCombo * combo, LiVESTreeIter * iter) {
#ifdef GUI_GTK
  gtk_combo_box_set_active_iter(combo, iter);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_get_active_iter(LiVESCombo * combo, LiVESTreeIter * iter) {
#ifdef GUI_GTK
  return gtk_combo_box_get_active_iter(combo, iter);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_combo_get_active_index(LiVESCombo * combo) {
#ifdef GUI_GTK
  LiVESTreeModel *tmodel = lives_combo_get_model(combo);
  if (GTK_IS_TREE_STORE(tmodel)) {
    int count = 0;
    LiVESTreeIter iter, iter1, iter2;
    if (!lives_combo_get_active_iter(combo, &iter)) return -1;
    if (gtk_tree_model_iter_children(tmodel, &iter1, NULL)) {
      if (gtk_tree_model_iter_children(tmodel, &iter2, &iter1)) {
        while (1) {
          if (iter2.stamp == iter.stamp) return count;
          count++;
          if (!gtk_tree_model_iter_next(tmodel, &iter2)) break;
        }
      }
    }
  }
  return gtk_combo_box_get_active(combo);
#endif
  return -1;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_text_view_new(void) {
  LiVESWidget *tview = NULL;
#ifdef GUI_GTK
  tview = gtk_text_view_new();
#endif
  return tview;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_text_view_new_with_buffer(LiVESTextBuffer * tbuff) {
  LiVESWidget *tview = NULL;
#ifdef GUI_GTK
  tview = gtk_text_view_new_with_buffer(tbuff);
#endif
  return tview;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTextBuffer *lives_text_view_get_buffer(LiVESTextView * tview) {
  LiVESTextBuffer *tbuff = NULL;
#ifdef GUI_GTK
  tbuff = gtk_text_view_get_buffer(tview);
#endif
  return tbuff;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_editable(LiVESTextView * tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_editable(tview, setting);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_accepts_tab(LiVESTextView * tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_accepts_tab(tview, setting);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_cursor_visible(LiVESTextView * tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_cursor_visible(tview, setting);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_wrap_mode(LiVESTextView * tview, LiVESWrapMode wrapmode) {
#ifdef GUI_GTK
  gtk_text_view_set_wrap_mode(tview, wrapmode);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_justification(LiVESTextView * tview, LiVESJustification justify) {
#ifdef GUI_GTK
  gtk_text_view_set_justification(tview, justify);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_top_margin(LiVESTextView * tview, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 18, 0)
  gtk_text_view_set_top_margin(tview, margin);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_bottom_margin(LiVESTextView * tview, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 18, 0)
  gtk_text_view_set_bottom_margin(tview, margin);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTextBuffer *lives_text_buffer_new(void) {
  LiVESTextBuffer *tbuff = NULL;
#ifdef GUI_GTK
  tbuff = gtk_text_buffer_new(NULL);
#endif
  return tbuff;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_insert(LiVESTextBuffer * tbuff, LiVESTextIter * iter, const char *text,
    int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert(tbuff, iter, text, len);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_insert_markup(LiVESTextBuffer * tbuff, LiVESTextIter * iter,
    const char *markup, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert_markup(tbuff, iter, markup, len);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_insert_at_cursor(LiVESTextBuffer * tbuff, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert_at_cursor(tbuff, text, len);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_set_text(LiVESTextBuffer * tbuff, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_set_text(tbuff, text, len);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE char *lives_text_buffer_get_text(LiVESTextBuffer * tbuff, LiVESTextIter * start,
    LiVESTextIter * end,
    boolean inc_hidden_chars) {
#ifdef GUI_GTK
  return gtk_text_buffer_get_text(tbuff, start, end, inc_hidden_chars);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE char *lives_text_buffer_get_all_text(LiVESTextBuffer * tbuff) {
  LiVESTextIter s, e;
  lives_text_buffer_get_start_iter(tbuff, &s);
  lives_text_buffer_get_end_iter(tbuff, &e);
  return lives_text_buffer_get_text(tbuff, &s, &e, FALSE);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_get_start_iter(LiVESTextBuffer * tbuff, LiVESTextIter * iter) {
#ifdef GUI_GTK
  gtk_text_buffer_get_start_iter(tbuff, iter);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_get_end_iter(LiVESTextBuffer * tbuff, LiVESTextIter * iter) {
#ifdef GUI_GTK
  gtk_text_buffer_get_end_iter(tbuff, iter);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_place_cursor(LiVESTextBuffer * tbuff, LiVESTextIter * iter) {
#ifdef GUI_GTK
  gtk_text_buffer_place_cursor(tbuff, iter);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTextMark *lives_text_buffer_create_mark(LiVESTextBuffer * tbuff, const char *mark_name,
    const LiVESTextIter * where, boolean left_gravity) {
  LiVESTextMark *tmark;
#ifdef GUI_GTK
  tmark = gtk_text_buffer_create_mark(tbuff, mark_name, where, left_gravity);
#endif
  return tmark;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_delete_mark(LiVESTextBuffer * tbuff, LiVESTextMark * mark) {
#ifdef GUI_GTK
  gtk_text_buffer_delete_mark(tbuff, mark);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_delete(LiVESTextBuffer * tbuff, LiVESTextIter * start,
    LiVESTextIter * end) {
#ifdef GUI_GTK
  gtk_text_buffer_delete(tbuff, start, end);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_get_iter_at_mark(LiVESTextBuffer * tbuff, LiVESTextIter * iter,
    LiVESTextMark * mark) {
#ifdef GUI_GTK
  gtk_text_buffer_get_iter_at_mark(tbuff, iter, mark);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_dialog_new(void) {
  LiVESWidget *dialog = NULL;
#ifdef GUI_GTK
  dialog = gtk_dialog_new();
#endif
  return dialog;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_button_new(void) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = gtk_button_new();
  gtk_button_set_use_underline(GTK_BUTTON(button), widget_opts.mnemonic_label);
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_button_new_with_label(const char *label) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = lives_button_new();
  lives_button_set_label(LIVES_BUTTON(button), label);
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_image_from_stock(LiVESButton * button,
    const char *stock_id) {
  boolean can_show = FALSE, always_show = FALSE;
#ifdef USE_SPECIAL_BUTTONS
  can_show = TRUE;
#endif
  if (THREADVAR(force_button_image)) always_show = TRUE;

  /// tweaks for better icons
  if (!strcmp(stock_id, LIVES_STOCK_YES)) stock_id = LIVES_STOCK_APPLY;
  if (!strcmp(stock_id, LIVES_STOCK_NO)) stock_id = LIVES_STOCK_STOP;

  if (!strcmp(stock_id, LIVES_STOCK_OK)) stock_id = LIVES_STOCK_APPLY;
  if (!strcmp(stock_id, LIVES_STOCK_CANCEL)) stock_id = LIVES_STOCK_STOP;

  if (!is_standard_widget(LIVES_WIDGET(button))) {
    if (stock_id && (widget_opts.show_button_images
                     || !strcmp(stock_id, LIVES_STOCK_ADD)
                     || !strcmp(stock_id, LIVES_STOCK_REMOVE)
                    )) {
      LiVESWidget *image = gtk_image_new_from_icon_name(stock_id, LIVES_ICON_SIZE_BUTTON);
      if (LIVES_IS_IMAGE(image)) {
        gtk_button_set_image(LIVES_BUTTON(button), image);
      } else return FALSE;
    }
  } else {
    if (can_show || always_show) {
      if (stock_id) {
        LiVESPixbuf *pixbuf = lives_pixbuf_new_from_stock_at_size(stock_id, LIVES_ICON_SIZE_BUTTON, 0);
        if (LIVES_IS_PIXBUF(pixbuf)) {
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), SBUTT_PIXBUF_KEY, (livespointer)pixbuf);
        } else {
          const char *iname = widget_helper_suggest_icons(stock_id, 0);
          if (iname) {
            LiVESPixbuf *pixbuf = lives_pixbuf_new_from_stock_at_size(iname, LIVES_ICON_SIZE_BUTTON, 0);
            if (LIVES_IS_PIXBUF(pixbuf)) {
              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), SBUTT_PIXBUF_KEY, (livespointer)pixbuf);
              //if (prefs->show_dev_opts) g_print("Guessed icon %s for %s\n", iname, stock_id);
            } else return FALSE;
          } else return FALSE;
        }
        if (always_show) lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), SBUTT_FORCEIMG_KEY, pixbuf);
      }
    }
  }
  return TRUE;
}


LiVESWidget *lives_standard_button_new_from_stock(const char *stock_id, const char *label, int width, int height) {
  LiVESWidget *button = NULL;

#if GTK_CHECK_VERSION(3, 10, 0)
  do {
    if (!palette || !stock_id) {
      button = lives_standard_button_new(width, height);
      if (stock_id && *stock_id) {
        if (!strcmp(stock_id, LIVES_STOCK_CANCEL)) label = LIVES_STOCK_LABEL_CANCEL;
        if (!strcmp(stock_id, LIVES_STOCK_OK)) label = LIVES_STOCK_LABEL_OK;
      }
      if (label && *label) lives_button_set_label(LIVES_BUTTON(button), label);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_LABEL_CANCEL)) stock_id = LIVES_STOCK_CANCEL;
    if (!strcmp(stock_id, LIVES_STOCK_LABEL_OK)) stock_id = LIVES_STOCK_OK;

    // gtk 3.10 + -> we need to set the text ourselves
    if (!strcmp(stock_id, LIVES_STOCK_APPLY)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_APPLY, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_OK)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_OK, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_CANCEL)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_CANCEL, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_YES)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_YES, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_NO)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_NO, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_CLOSE)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_CLOSE, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_REVERT_TO_SAVED)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_REVERT, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_REFRESH)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_REFRESH, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_DELETE)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_DELETE, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_SAVE)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_SAVE, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_SAVE_AS)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_SAVE_AS, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_OPEN)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_OPEN, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_SELECT_ALL)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_SELECT_ALL, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_QUIT)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_QUIT, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_GO_FORWARD)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_GO_FORWARD, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_FORWARD)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_FORWARD, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_REWIND)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_REWIND, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_STOP)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_STOP, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_PLAY)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_PLAY, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_PAUSE)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_PAUSE, width, height);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_RECORD)) {
      button = lives_standard_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_RECORD, width, height);
      break;
    }
    // text not known
    button = lives_standard_button_new(width, height);
  } while (FALSE);

  if (stock_id) lives_button_set_image_from_stock(LIVES_BUTTON(button), stock_id);

#else
  // < 3.10
  button = gtk_button_new_from_stock(stock_id);
#endif

  if (!LIVES_IS_BUTTON(button)) {
    char *msg = lives_strdup_printf("Unable to find button with stock_id: %s", stock_id);
    LIVES_WARN(msg);
    lives_free(msg);
    button = lives_standard_button_new(width, height);
  }

#ifdef GUI_GTK
  if (label)
    _lives_standard_button_set_label(LIVES_BUTTON(button), label);
#endif

  lives_widget_set_can_focus_and_default(button);
  lives_widget_apply_theme(button, LIVES_WIDGET_STATE_NORMAL);
  return button;
}



WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_label(LiVESButton * button, const char *label) {
#ifdef USE_SPECIAL_BUTTONS
  if (is_standard_widget(LIVES_WIDGET(button))) return lives_standard_button_set_label(button, label);
#endif
#ifdef GUI_GTK
  gtk_button_set_label(button, label);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_button_get_label(LiVESButton * button) {
#ifdef USE_SPECIAL_BUTTONS
  if (is_standard_widget(LIVES_WIDGET(button))) return lives_standard_button_get_label(button);
#endif
#ifdef GUI_GTK
  return gtk_button_get_label(button);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_clicked(LiVESButton * button) {
#ifdef GUI_GTK
  gtk_button_clicked(button);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_relief(LiVESButton * button, LiVESReliefStyle rstyle) {
#ifdef GUI_GTK
  gtk_button_set_relief(button, rstyle);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_image(LiVESButton * button, LiVESWidget * image) {
#ifdef GUI_GTK
  gtk_button_set_image(button, image);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_focus_on_click(LiVESWidget * widget, boolean focus) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 20, 0)
  gtk_widget_set_focus_on_click(widget, focus);
#else
  if (!LIVES_IS_BUTTON(widget)) return FALSE;
  gtk_button_set_focus_on_click(LIVES_BUTTON(widget), focus);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_focus_on_click(LiVESButton * button, boolean focus) {
  return lives_widget_set_focus_on_click(LIVES_WIDGET(button), focus);
}


WIDGET_HELPER_GLOBAL_INLINE int lives_paned_get_position(LiVESPaned * paned) {
#ifdef GUI_GTK
  return gtk_paned_get_position(paned);
#endif
  return -1;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_paned_set_position(LiVESPaned * paned, int pos) {
  // call this only after adding widgets
#ifdef GUI_GTK
  gtk_paned_set_position(paned, pos);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_paned_pack(int where, LiVESPaned * paned, LiVESWidget * child, boolean resize,
    boolean shrink) {
#ifdef GUI_GTK
  if (where == 1) gtk_paned_pack1(paned, child, resize, shrink);
  else gtk_paned_pack2(paned, child, resize, shrink);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_drawing_area_new(void) {
  LiVESWidget *darea = NULL;
#ifdef GUI_GTK
  darea = gtk_drawing_area_new();
#endif
  return darea;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_event_get_time(LiVESXEvent * event) {
#ifdef GUI_GTK
  return gdk_event_get_time(event);
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_get_active(LiVESToggleButton * button) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_SWITCH(button)) return gtk_switch_get_active(LIVES_SWITCH(button));
#endif
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_get_inactive(LiVESToggleButton * button) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_SWITCH(button)) return !gtk_switch_get_active(LIVES_SWITCH(button));
#endif
  return !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_set_active(LiVESToggleButton * button, boolean active) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_SWITCH(button)) lives_switch_set_active(LIVES_SWITCH(button), active);
  else
#endif
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_set_mode(LiVESToggleButton * button, boolean drawind) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (!LIVES_IS_SWITCH(button))
#endif
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), drawind);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_toggle_tool_button_new(void) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = LIVES_WIDGET(gtk_toggle_tool_button_new());
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_tool_button_get_active(LiVESToggleToolButton * button) {
#ifdef GUI_GTK
  return gtk_toggle_tool_button_get_active(button);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_tool_button_set_active(LiVESToggleToolButton * button, boolean active) {
#ifdef GUI_GTK
  gtk_toggle_tool_button_set_active(button, active);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_radio_button_new(LiVESSList * group) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = gtk_radio_button_new(group);
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_switch_new(void) {
  LiVESWidget *swtch = NULL;
#if LIVES_HAS_SWITCH_WIDGET
  swtch = gtk_switch_new();
#endif
  return swtch;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_switch_get_active(LiVESSwitch * swtch) {
#if LIVES_HAS_SWITCH_WIDGET
  return gtk_switch_get_active(swtch);
#endif
  return FALSE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_switch_set_active(LiVESSwitch * swtch, boolean active) {
#if LIVES_HAS_SWITCH_WIDGET
  gtk_switch_set_active(swtch, active);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_spinner_new(void) {
  LiVESWidget *spinner = NULL;
#if LIVES_HAS_SPINNER_WIDGET
  spinner = gtk_spinner_new();
#endif
  return spinner;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_spinner_start(LiVESSpinner * spinner) {
  if (spinner) {
#if LIVES_HAS_SPINNER_WIDGET
    gtk_spinner_start(GTK_SPINNER(spinner));
    return TRUE;
#endif
  }
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spinner_stop(LiVESSpinner * spinner) {
  if (spinner) {
#if LIVES_HAS_SPINNER_WIDGET
    gtk_spinner_stop(GTK_SPINNER(spinner));
    return TRUE;
#endif
  }
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_check_button_new(void) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = gtk_check_button_new();
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_check_button_new_with_label(const char *label) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = gtk_check_button_new_with_label(label);
#endif
  return button;
}


static LiVESWidget *make_ttips_image_for(LiVESWidget * widget, const char *text) {
  LiVESWidget *ttips_image = lives_image_new_from_stock_at_size("livestock-help-info",
                             LIVES_ICON_SIZE_CUSTOM, widget_opts.css_min_height + 2);
  if (ttips_image) {
#if GTK_CHECK_VERSION(3, 16, 0)
    if (widget_opts.apply_theme) {
      set_css_value_direct(ttips_image, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.75");
    }
#endif
    lives_widget_set_no_show_all(ttips_image, TRUE);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ttips_image), TTIPS_IMAGE_KEY, ttips_image);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ttips_image), TTIPS_HIDE_KEY, ttips_image);
    if (text) lives_widget_set_tooltip_text(ttips_image, text);
    else {
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ttips_image),
                                   SHOWALL_OVERRIDE_KEY, LIVES_INT_TO_POINTER(TRUE));
    }
    lives_widget_set_show_hide_with(widget, ttips_image);
    lives_widget_set_sensitive_with(widget, ttips_image);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), HAS_TTIPS_IMAGE_KEY, ttips_image);
    lives_widget_set_valign(ttips_image, LIVES_ALIGN_CENTER);
  }
  ///lives_widget_set_valign(ttips_image, LIVES_ALIGN_START);
  return ttips_image;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_widget_set_tooltip_text(LiVESWidget * widget, const char *tip_text) {
  LiVESWidget *img_tips = NULL;
  boolean ttips_override = FALSE;
  char *ttext = NULL, *otext = NULL;

  if (!widget) return NULL;

  if (tip_text) {
    ttext = lives_strdup(tip_text);
    otext = lives_chomp(ttext, TRUE);
  }

  if (ttext && *ttext == '#' && !lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
      TTIPS_IMAGE_KEY)) {
    // create new image tips, or re-enter with img tips
    if (!(img_tips = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), HAS_TTIPS_IMAGE_KEY))) {
      img_tips = make_ttips_image_for(widget, ++ttext);
      if (img_tips) widget = img_tips;
    } else lives_widget_set_tooltip_text(img_tips, ++ttext);
  }

  if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), HAS_TTIPS_IMAGE_KEY)) {
    // has existing img tips
    if (!img_tips) {
      // not new
      if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_OVERRIDE_KEY))
        ttips_override = TRUE;
      if (!prefs->show_tooltips && !ttips_override) {
        // ttips hidden by pref
        if (ttext) {
          lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY,
                                            (livespointer)(lives_strdup(ttext)));
        } else {
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY, NULL);
        }

        if (!lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
                                          TTIPS_IMAGE_KEY)) {
          img_tips = make_ttips_image_for(widget, NULL);
          if (otext) lives_free(otext);
          lives_widget_hide(img_tips);
          return img_tips;
        }
        if (otext) lives_free(otext);
        return NULL;
      }

      if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
                                       TTIPS_IMAGE_KEY)) {
        // not hidden - set data or hide
        if (ttext) {
          if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
                                           SHOWALL_OVERRIDE_KEY)) {
            LiVESWidget *cntrl;
            lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget),
                                         SHOWALL_OVERRIDE_KEY, NULL);
            if ((cntrl = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
                         SHOWHIDE_CONTROLLER_KEY))) {
              if (lives_widget_is_visible(cntrl)) _lives_widget_show_all(widget);
            }
          }
        } else {
          // set empty text, hide image and make sure we never show it
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget),
                                       SHOWALL_OVERRIDE_KEY, widget);
          lives_widget_hide(widget);
        }
      } else {
        if (!lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_IMAGE_KEY)) {
          // should have image so create it
          img_tips = make_ttips_image_for(widget, NULL);
        }
      }
    }
    // set tips for img
    widget = img_tips;
  }

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text(widget, ttext);
#else
  GtkTooltips *tips;
  tips = gtk_tooltips_new();
  gtk_tooltips_set_tip(tips, widget, ttext, NULL);
#endif
  if (otext) lives_free(otext);
#endif
  return img_tips;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_grab_focus(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_widget_set_can_focus(widget, TRUE);
  gtk_widget_grab_focus(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_grab_default(LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_widget_grab_default(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESSList *lives_radio_button_get_group(LiVESRadioButton * rbutton) {
#ifdef GUI_GTK
  return gtk_radio_button_get_group(rbutton);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_widget_get_parent(LiVESWidget * widget) {
#ifdef GUI_GTK
  return gtk_widget_get_parent(widget);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_widget_get_toplevel(LiVESWidget * widget) {
#ifdef GUI_GTK
  if (!LIVES_IS_WIDGET(widget)) return NULL;
#if GTK_CHECK_VERSION(4, 0, 0)
  GtkMenu, GtkMenuBar and GtkMenuItem are gone

#else
  return gtk_widget_get_toplevel(widget);
#endif
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESXWindow *lives_widget_get_xwindow(LiVESWidget * widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(4, 0, 0)
  return gtk_native_get_surface(gtk_widget_get_native(widget));
#endif
#if GTK_CHECK_VERSION(2, 12, 0)
  return gtk_widget_get_window(widget);
#else
  return GDK_WINDOW(widget->window);
#endif
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWindow *lives_widget_get_window(LiVESWidget * widget) {
#ifdef GUI_GTK
  LiVESWidget *window = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
  if (GTK_IS_WINDOW(window)) return (LiVESWindow *)window;
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_set_keep_above(LiVESXWindow * xwin, boolean setting) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(4, 0, 0)
  XWindowChanges changes;
  unsigned int valueMask = CWStackMode;
  changes.stack_mode = Above;
  XConfigureWindow(GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(widget)),
                   gdk_x11_surface_get_xid(xwin),
                   valueMask, &changes);
#else
  gdk_window_set_keep_above(xwin, setting);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_can_focus(LiVESWidget * widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  gtk_widget_set_can_focus(widget, state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
  else
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_FOCUS);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_can_default(LiVESWidget * widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  gtk_widget_set_can_default(widget, state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_DEFAULT);
  else
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_DEFAULT);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_add_events(LiVESWidget * widget, int events) {
#ifdef GUI_GTK
  gtk_widget_add_events(widget, events);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_events(LiVESWidget * widget, int events) {
#ifdef GUI_GTK
  gtk_widget_set_events(widget, events);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_remove_accelerator(LiVESWidget * widget, LiVESAccelGroup * acgroup,
    uint32_t accel_key, LiVESXModifierType accel_mods) {
#ifdef GUI_GTK
  return gtk_widget_remove_accelerator(widget, acgroup, accel_key, accel_mods);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_preferred_size(LiVESWidget * widget, LiVESRequisition * min_size,
    LiVESRequisition * nat_size) {
  // for GTK 4.x we will use widget::measure()
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_get_preferred_size(widget, min_size, nat_size);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_no_show_all(LiVESWidget * widget, boolean set) {
#ifdef GUI_GTK
  gtk_widget_set_no_show_all(widget, set);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_no_show_all(LiVESWidget * widget) {
#ifdef GUI_GTK
  return gtk_widget_get_no_show_all(widget);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_sensitive(LiVESWidget * widget) {
  // return TRUE is widget + parent is sensitive
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  return gtk_widget_is_sensitive(widget);
#else
  return GTK_WIDGET_IS_SENSITIVE(widget);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_visible(LiVESWidget * widget) {
#ifdef GUI_GTK
  if (!LIVES_IS_WIDGET(widget)) return FALSE;
#if GTK_CHECK_VERSION(2, 18, 0)
  return gtk_widget_get_visible(widget);
#else
  return GTK_WIDGET_VISIBLE(widget);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_realized(LiVESWidget * widget) {
  // used for giw widgets
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  return gtk_widget_get_realized(widget);
#else
  return GTK_WIDGET_REALIZED(widget);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_add(LiVESContainer * container, LiVESWidget * widget) {
#ifdef GUI_GTK
  if (!gui_loop_tight || is_fg_thread())
    gtk_container_add(container, widget);
  else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(gtk_container_add, "vv", container, widget);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_remove(LiVESContainer * container, LiVESWidget * widget) {
#ifdef GUI_GTK
  if (is_fg_thread())
    gtk_container_remove(container, widget);
  else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(gtk_container_remove, "vv", container, widget);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_set_border_width(LiVESContainer * container, uint32_t width) {
  // sets border OUTSIDE container
#ifdef GUI_GTK
  gtk_container_set_border_width(container, width);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_foreach(LiVESContainer * cont, LiVESWidgetCallback callback,
    livespointer cb_data) {
  // excludes internal children
#ifdef GUI_GTK
  gtk_container_foreach(cont, callback, cb_data);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_foreach_int(LiVESContainer * cont, LiVESWidgetCallback callback,
    int cb_data) {
  // excludes internal children
#ifdef GUI_GTK
  gtk_container_foreach(cont, callback, LIVES_INT_TO_POINTER(cb_data));
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_LOCAL_INLINE boolean lives_container_forall(LiVESContainer * cont, LiVESWidgetCallback callback,
    livespointer cb_data) {
  // includes internal children
#ifdef GUI_GTK
  gtk_container_forall(cont, callback, cb_data);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESList *lives_container_get_children(LiVESContainer * cont) {
  LiVESList *children = NULL;
#ifdef GUI_GTK
  children = gtk_container_get_children(cont);
#endif
  return children;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_set_focus_child(LiVESContainer * cont, LiVESWidget * child) {
#ifdef GUI_GTK
  gtk_container_set_focus_child(cont, child);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_container_get_focus_child(LiVESContainer * cont) {
#ifdef GUI_GTK
  return gtk_container_get_focus_child(cont);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_progress_bar_new(void) {
  LiVESWidget *pbar = NULL;
#ifdef GUI_GTK
  pbar = gtk_progress_bar_new();
#endif
  return pbar;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_progress_bar_set_fraction(LiVESProgressBar * pbar, double fraction) {
#ifdef GUI_GTK
#ifdef PROGBAR_IS_ENTRY
  if (widget_opts.apply_theme) {
    lives_widget_set_sensitive(LIVES_WIDGET(pbar), FALSE);
  }
  gtk_entry_set_progress_fraction(pbar, fraction);
  if (is_standard_widget(LIVES_WIDGET(pbar)) && widget_opts.apply_theme) {
    set_css_value_direct(LIVES_WIDGET(pbar), LIVES_WIDGET_STATE_NORMAL, "progress",
                         "border-width", "0px");
  }
#else
  gtk_progress_bar_set_fraction(pbar, fraction);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_progress_bar_set_pulse_step(LiVESProgressBar * pbar, double fraction) {
#ifdef GUI_GTK
#ifdef PROGBAR_IS_ENTRY
  gtk_entry_set_progress_pulse_step(pbar, fraction);
#else
  gtk_progress_bar_set_pulse_step(pbar, fraction);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_progress_bar_pulse(LiVESProgressBar * pbar) {
#ifdef GUI_GTK
#ifdef PROGBAR_IS_ENTRY
  if (widget_opts.apply_theme) {
    lives_widget_set_sensitive(LIVES_WIDGET(pbar), TRUE);
  }
  gtk_entry_progress_pulse(pbar);
  if (is_standard_widget(LIVES_WIDGET(pbar)) && widget_opts.apply_theme) {
    set_css_value_direct(LIVES_WIDGET(pbar), LIVES_WIDGET_STATE_NORMAL, "progress",
                         "border-width", "0px");
  }
#else
  gtk_progress_bar_pulse(pbar);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_spin_button_new(LiVESAdjustment * adj, double climb_rate, uint32_t digits) {
  LiVESWidget *sbutton = NULL;
#ifdef GUI_GTK
  sbutton = gtk_spin_button_new(adj, climb_rate, digits);
#endif
  return sbutton;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_spin_button_get_value(LiVESSpinButton * button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_value(button);
#endif
  return 0.;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_spin_button_get_value_as_int(LiVESSpinButton * button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_value_as_int(button);
#endif
  return 0.;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_spin_button_get_adjustment(LiVESSpinButton * button) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
  adj = gtk_spin_button_get_adjustment(button);
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_spin_button_set_adjustment(LiVESSpinButton * button, LiVESAdjustment * adj) {
#ifdef GUI_GTK
  gtk_spin_button_set_adjustment(button, adj);
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_value(LiVESSpinButton * button, double value) {
  if (is_standard_widget(LIVES_WIDGET(button))) {
    if (GET_INT_DATA(button, SNAPVAL_KEY))
      value = lives_spin_button_get_snapval(button, value);
  }
#ifdef GUI_GTK
  if (!gui_loop_tight || is_fg_thread()) gtk_spin_button_set_value(button, value);
  else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(gtk_spin_button_set_value, "vd", button, value);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_range(LiVESSpinButton * button, double min, double max) {
#ifdef GUI_GTK
  if (is_fg_thread()) gtk_spin_button_set_range(button, min, max);
  else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(gtk_spin_button_set_range, "vdd", button, min, max);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_max(LiVESSpinButton * button, double max) {
#ifdef GUI_GTK
  double min;
  gtk_spin_button_get_range(button, &min, NULL);
  gtk_spin_button_set_range(button, min, max);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_min(LiVESSpinButton * button, double min) {
#ifdef GUI_GTK
  double max;
  gtk_spin_button_get_range(button, NULL, &max);
  gtk_spin_button_set_range(button, min, max);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_wrap(LiVESSpinButton * button, boolean wrap) {
#ifdef GUI_GTK
  gtk_spin_button_set_wrap(button, wrap);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_snap_to_ticks(LiVESSpinButton * button, boolean snap) {
#ifdef GUI_GTK
  gtk_spin_button_set_snap_to_ticks(button, snap);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_digits(LiVESSpinButton * button, uint32_t digits) {
#ifdef GUI_GTK
  gtk_spin_button_set_digits(button, digits);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE uint32_t lives_spin_button_get_digits(LiVESSpinButton * button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_digits(button);
#endif
  return 0;
}


static void _lives_spin_button_update(LiVESSpinButton * button) {
#ifdef GUI_GTK
  gtk_spin_button_update(button);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_update(LiVESSpinButton * button) {
#ifdef GUI_GTK
  if (!gui_loop_tight || is_fg_thread()) {
    _lives_spin_button_update(button);
  } else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(_lives_spin_button_update, "v", button);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#else
  return FALSE;
#endif
}


WIDGET_HELPER_GLOBAL_INLINE LiVESToolItem *lives_tool_button_new(LiVESWidget * icon_widget, const char *label) {
  LiVESToolItem *button = NULL;
#ifdef GUI_GTK
  button = gtk_tool_button_new(icon_widget, label);
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESToolItem *lives_tool_item_new(void) {
  LiVESToolItem *item = NULL;
#ifdef GUI_GTK
  item = gtk_tool_item_new();
#endif
  return item;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESToolItem *lives_separator_tool_item_new(void) {
  LiVESToolItem *item = NULL;
#ifdef GUI_GTK
  item = gtk_separator_tool_item_new();
#endif
  return item;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tool_button_set_icon_widget(LiVESToolButton * button, LiVESWidget * icon) {
#ifdef GUI_GTK
  gtk_tool_button_set_icon_widget(button, icon);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_tool_button_get_icon_widget(LiVESToolButton * button) {
#ifdef GUI_GTK
  return gtk_tool_button_get_icon_widget(button);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tool_button_set_label_widget(LiVESToolButton * button, LiVESWidget * label) {
#ifdef GUI_GTK
  gtk_tool_button_set_label_widget(button, label);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_tool_button_get_label_widget(LiVESToolButton * button) {
#ifdef GUI_GTK
  return gtk_tool_button_get_label_widget(button);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tool_button_set_use_underline(LiVESToolButton * button, boolean use_underline) {
#ifdef GUI_GTK
  gtk_tool_button_set_use_underline(button, use_underline);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_ruler_set_range(LiVESRuler * ruler, double lower, double upper, double position,
    double max_size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_range_set_range(GTK_RANGE(ruler), lower, upper);
  gtk_range_set_value(GTK_RANGE(ruler), position);
#else
  gtk_ruler_set_range(ruler, lower, upper, position, max_size);
  return TRUE;
#endif
  return FALSE;
#endif
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_message_dialog_new(LiVESWindow * parent, LiVESDialogFlags flags,
    LiVESMessageType type, LiVESButtonsType buttons, const char *msg_fmt, ...) {
  LiVESWidget *mdial = NULL;
#ifdef GUI_GTK
  mdial = gtk_message_dialog_new(parent, flags | GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, msg_fmt, NULL);
#endif
  if (mdial && mainw && mainw->mgeom) lives_window_set_monitor(LIVES_WINDOW(mdial),
        widget_opts.monitor);
  return mdial;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_ruler_get_value(LiVESRuler * ruler) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  return gtk_range_get_value(GTK_RANGE(ruler));
#else
  return ruler->position;
#endif
#endif
  return 0.;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_ruler_set_value(LiVESRuler * ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_range_set_value(GTK_RANGE(ruler), value);
#else
  ruler->position = value;
#endif
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_ruler_set_upper(LiVESRuler * ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#ifdef ENABLE_GIW_3
  if (GIW_IS_TIMELINE(ruler)) {
    LiVESAdjustment *adj = giw_timeline_get_adjustment(GIW_TIMELINE(ruler));
    double lower = lives_adjustment_get_lower(adj);
    giw_timeline_set_range(GIW_TIMELINE(ruler), lower, value, giw_timeline_get_max_size(GIW_TIMELINE(ruler)));
  } else
#endif
    gtk_adjustment_set_upper(gtk_range_get_adjustment(GTK_RANGE(ruler)), value);
#else
  ruler->upper = value;
#endif
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_ruler_set_lower(LiVESRuler * ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#ifdef ENABLE_GIW_3
  if (GIW_IS_TIMELINE(ruler)) {
    LiVESAdjustment *adj = giw_timeline_get_adjustment(GIW_TIMELINE(ruler));
    double upper = lives_adjustment_get_upper(adj);
    giw_timeline_set_range(GIW_TIMELINE(ruler), value, upper, giw_timeline_get_max_size(GIW_TIMELINE(ruler)));
  } else
#endif
    gtk_adjustment_set_lower(gtk_range_get_adjustment(GTK_RANGE(ruler)), value);
#else
  ruler->lower = value;
#endif
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESCellRenderer *lives_cell_renderer_text_new(void) {
  LiVESCellRenderer *renderer = NULL;
#ifdef GUI_GTK
  renderer = gtk_cell_renderer_text_new();
#endif
  return renderer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESCellRenderer *lives_cell_renderer_spin_new(void) {
  LiVESCellRenderer *renderer = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 10, 0)
  renderer = gtk_cell_renderer_spin_new();
#endif
#endif
  return renderer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESCellRenderer *lives_cell_renderer_toggle_new(void) {
  LiVESCellRenderer *renderer = NULL;
#ifdef GUI_GTK
  renderer = gtk_cell_renderer_toggle_new();
#endif
  return renderer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESCellRenderer *lives_cell_renderer_pixbuf_new(void) {
  LiVESCellRenderer *renderer = NULL;
#ifdef GUI_GTK
  renderer = gtk_cell_renderer_pixbuf_new();
#endif
  return renderer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_toolbar_new(void) {
  LiVESWidget *toolbar = NULL;
#ifdef GUI_GTK
  toolbar = gtk_toolbar_new();
#endif
  return toolbar;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toolbar_insert(LiVESToolbar * toolbar, LiVESToolItem * item, int pos) {
#ifdef GUI_GTK
  gtk_toolbar_insert(toolbar, item, pos);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toolbar_set_show_arrow(LiVESToolbar * toolbar, boolean show) {
#ifdef GUI_GTK
  gtk_toolbar_set_show_arrow(toolbar, show);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESIconSize lives_toolbar_get_icon_size(LiVESToolbar * toolbar) {
#ifdef GUI_GTK
  return gtk_toolbar_get_icon_size(toolbar);
#endif
  return LIVES_ICON_SIZE_INVALID;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toolbar_set_icon_size(LiVESToolbar * toolbar, LiVESIconSize icon_size) {
#ifdef GUI_GTK
  gtk_toolbar_set_icon_size(toolbar, icon_size);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toolbar_set_style(LiVESToolbar * toolbar, LiVESToolbarStyle style) {
#ifdef GUI_GTK
  gtk_toolbar_set_style(toolbar, style);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_widget_get_allocation_x(LiVESWidget * widget) {
  int x = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  x = alloc.x;
#else
  x = widget->allocation.x;
#endif
#endif
  return x;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_widget_get_allocation_y(LiVESWidget * widget) {
  int y = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  y = alloc.y;
#else
  y = widget->allocation.y;
#endif
#endif
  return y;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_widget_get_allocation_width(LiVESWidget * widget) {
  int width = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  width = alloc.width;
#else
  width = widget->allocation.width;
#endif
#endif
  return width;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_widget_get_allocation_height(LiVESWidget * widget) {
  int height = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  height = alloc.height;
#else
  height = widget->allocation.height;
#endif
#endif
  return height;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_state(LiVESWidget * widget, LiVESWidgetState state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_state_flags(widget, state, TRUE);
#else
  gtk_widget_set_state(widget, state);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetState lives_widget_get_state(LiVESWidget * widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  return gtk_widget_get_state_flags(widget);
#else
#if GTK_CHECK_VERSION(2, 18, 0)
  return gtk_widget_get_state(widget);
#else
  return GTK_WIDGET_STATE(widget);
#endif
#endif
#endif
  return (LiVESWidgetState)0;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_bin_get_child(LiVESBin * bin) {
  LiVESWidget *child = NULL;
#ifdef GUI_GTK
  child = gtk_bin_get_child(bin);
#endif
  return child;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_upper(LiVESAdjustment * adj) {
  double upper = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  upper = gtk_adjustment_get_upper(adj);
#else
  upper = adj->upper;
#endif
#endif
  return upper;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_lower(LiVESAdjustment * adj) {
  double lower = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  lower = gtk_adjustment_get_lower(adj);
#else
  lower = adj->lower;
#endif
#endif
  return lower;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_page_size(LiVESAdjustment * adj) {
  double page_size = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  page_size = gtk_adjustment_get_page_size(adj);
#else
  page_size = adj->page_size;
#endif
#endif
  return page_size;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_step_increment(LiVESAdjustment * adj) {
  double step_increment = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  step_increment = gtk_adjustment_get_step_increment(adj);
#else
  step_increment = adj->step_increment;
#endif
#endif
  return step_increment;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_page_increment(LiVESAdjustment * adj) {
  double page_increment = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  page_increment = gtk_adjustment_get_page_increment(adj);
#else
  page_increment = adj->page_increment;
#endif
#endif
  return page_increment;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_value(LiVESAdjustment * adj) {
  double value = 0.;
#ifdef GUI_GTK
  value = gtk_adjustment_get_value(adj);
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_upper(LiVESAdjustment * adj, double upper) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_upper(adj, upper);
#else
  adj->upper = upper;
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_lower(LiVESAdjustment * adj, double lower) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_lower(adj, lower);
#else
  adj->lower = lower;
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_page_size(LiVESAdjustment * adj, double page_size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_page_size(adj, page_size);
#else
  adj->page_size = page_size;
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_step_increment(LiVESAdjustment * adj, double step_increment) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_step_increment(adj, step_increment);
#else
  adj->step_increment = step_increment;
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_value(LiVESAdjustment * adj, double value) {
#ifdef GUI_GTK
  gtk_adjustment_set_value(adj, value);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_clamp_page(LiVESAdjustment * adj, double lower, double upper) {
#ifdef GUI_GTK
  gtk_adjustment_clamp_page(adj, lower, upper);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_range_get_adjustment(LiVESRange * range) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
  adj = gtk_range_get_adjustment(range);
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_adjustment(LiVESRange * range, LiVESAdjustment * adj) {
#ifdef GUI_GTK
  gtk_range_set_adjustment(range, adj);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_value(LiVESRange * range, double value) {
#ifdef GUI_GTK
  gtk_range_set_value(range, value);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_range(LiVESRange * range, double min, double max) {
#ifdef GUI_GTK
  gtk_range_set_range(range, min, max);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_increments(LiVESRange * range, double step, double page) {
#ifdef GUI_GTK
  gtk_range_set_increments(range, step, page);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_inverted(LiVESRange * range, boolean invert) {
#ifdef GUI_GTK
  gtk_range_set_inverted(range, invert);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_range_get_value(LiVESRange * range) {
  double value = 0.;
#ifdef GUI_GTK
  value = gtk_range_get_value(range);
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_get(LiVESTreeModel * tmod, LiVESTreeIter * titer, ...) {
  boolean res = FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_tree_model_get_valist(tmod, titer, argList);
  res = TRUE;
#endif
  va_end(argList);
  return res;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_get_iter(LiVESTreeModel * tmod, LiVESTreeIter * titer,
    LiVESTreePath * tpath) {
#ifdef GUI_GTK
  return gtk_tree_model_get_iter(tmod, titer, tpath);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_get_iter_first(LiVESTreeModel * tmod, LiVESTreeIter * titer) {
#ifdef GUI_GTK
  return gtk_tree_model_get_iter_first(tmod, titer);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreePath *lives_tree_model_get_path(LiVESTreeModel * tmod, LiVESTreeIter * titer) {
  LiVESTreePath *tpath = NULL;
#ifdef GUI_GTK
  tpath = gtk_tree_model_get_path(tmod, titer);
#endif
  return tpath;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_iter_children(LiVESTreeModel * tmod, LiVESTreeIter * titer,
    LiVESTreeIter * parent) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_children(tmod, titer, parent);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_tree_model_iter_n_children(LiVESTreeModel * tmod, LiVESTreeIter * titer) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_n_children(tmod, titer);
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_iter_next(LiVESTreeModel * tmod, LiVESTreeIter * titer) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_next(tmod, titer);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_path_free(LiVESTreePath * tpath) {
#ifdef GUI_GTK
  gtk_tree_path_free(tpath);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreePath *lives_tree_path_new_from_string(const char *path) {
  LiVESTreePath *tpath = NULL;
#ifdef GUI_GTK
  tpath = gtk_tree_path_new_from_string(path);
#endif
  return tpath;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_tree_path_get_depth(LiVESTreePath * tpath) {
  int depth = -1;
#ifdef GUI_GTK
  depth = gtk_tree_path_get_depth(tpath);
#endif
  return depth;
}


WIDGET_HELPER_GLOBAL_INLINE int *lives_tree_path_get_indices(LiVESTreePath * tpath) {
  int *indices = NULL;
#ifdef GUI_GTK
  indices = gtk_tree_path_get_indices(tpath);
#endif
  return indices;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeStore *lives_tree_store_new(int ncols, ...) {
  LiVESTreeStore *tstore = NULL;
  va_list argList;
  va_start(argList, ncols);
#ifdef GUI_GTK
  if (ncols > 0) {
    GType types[ncols];
    for (int i = 0; i < ncols; i++) {
      types[i] = va_arg(argList, long unsigned int);
    }
    tstore = gtk_tree_store_newv(ncols, types);
  }
  // supposedly speeds things up a bit...
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tstore),
                                       GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
                                       GTK_SORT_ASCENDING);
#endif
  va_end(argList);
  return tstore;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_list_store_append(LiVESListStore * lstore, LiVESTreeIter * liter) {
#ifdef GUI_GTK
  gtk_list_store_append(lstore, liter);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_list_store_prepend(LiVESListStore * lstore, LiVESTreeIter * liter) {
#ifdef GUI_GTK
  gtk_list_store_prepend(lstore, liter);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_store_append(LiVESTreeStore * tstore, LiVESTreeIter * titer,
    LiVESTreeIter * parent) {
#ifdef GUI_GTK
  gtk_tree_store_append(tstore, titer, parent);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_store_prepend(LiVESTreeStore * tstore, LiVESTreeIter * titer,
    LiVESTreeIter * parent) {
#ifdef GUI_GTK
  gtk_tree_store_prepend(tstore, titer, parent);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_store_set(LiVESTreeStore * tstore, LiVESTreeIter * titer, ...) {
  boolean res = FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_tree_store_set_valist(tstore, titer, argList);
  res = TRUE;
#endif
  va_end(argList);
  return res;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_tree_view_new_with_model(LiVESTreeModel * tmod) {
  LiVESWidget *tview = NULL;
#ifdef GUI_GTK
  tview = gtk_tree_view_new_with_model(tmod);
#endif
  return tview;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_tree_view_new(void) {
  LiVESWidget *tview = NULL;
#ifdef GUI_GTK
  tview = gtk_tree_view_new();
#endif
  return tview;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_view_set_model(LiVESTreeView * tview, LiVESTreeModel * tmod) {
#ifdef GUI_GTK
  gtk_tree_view_set_model(tview, tmod);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeModel *lives_tree_view_get_model(LiVESTreeView * tview) {
  LiVESTreeModel *tmod = NULL;
#ifdef GUI_GTK
  tmod = gtk_tree_view_get_model(tview);
#endif
  return tmod;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeSelection *lives_tree_view_get_selection(LiVESTreeView * tview) {
  LiVESTreeSelection *tsel = NULL;
#ifdef GUI_GTK
  tsel = gtk_tree_view_get_selection(tview);
#endif
  return tsel;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_tree_view_append_column(LiVESTreeView * tview, LiVESTreeViewColumn * tvcol) {
#ifdef GUI_GTK
  gtk_tree_view_append_column(tview, tvcol);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_view_set_headers_visible(LiVESTreeView * tview, boolean vis) {
#ifdef GUI_GTK
  gtk_tree_view_set_headers_visible(tview, vis);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_tree_view_get_hadjustment(LiVESTreeView * tview) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  adj = gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(tview));
#else
  adj = gtk_tree_view_get_hadjustment(tview);
#endif
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeViewColumn *lives_tree_view_column_new_with_attributes(const char *title,
    LiVESCellRenderer * crend, ...) {
  LiVESTreeViewColumn *tvcol = NULL;
  va_list args;
  va_start(args, crend);
  int column;
  char *attribute;
  boolean expand = FALSE;
#ifdef GUI_GTK

  tvcol = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(tvcol, title);
  gtk_tree_view_column_pack_start(tvcol, crend, expand);

  attribute = va_arg(args, char *);

  while (attribute) {
    column = va_arg(args, int);
    gtk_tree_view_column_add_attribute(tvcol, crend, attribute, column);
    attribute = va_arg(args, char *);
  }

#endif
  va_end(args);
  return tvcol;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_view_column_set_sizing(LiVESTreeViewColumn * tvcol,
    LiVESTreeViewColumnSizing type) {
#ifdef GUI_GTK
  gtk_tree_view_column_set_sizing(tvcol, type);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_view_column_set_fixed_width(LiVESTreeViewColumn * tvcol, int fwidth) {
#ifdef GUI_GTK
  gtk_tree_view_column_set_fixed_width(tvcol, fwidth);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_selection_get_selected(LiVESTreeSelection * tsel, LiVESTreeModel **tmod,
    LiVESTreeIter * titer) {
#ifdef GUI_GTK
  return gtk_tree_selection_get_selected(tsel, tmod, titer);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_selection_set_mode(LiVESTreeSelection * tsel, LiVESSelectionMode tselmod) {
#ifdef GUI_GTK
  gtk_tree_selection_set_mode(tsel, tselmod);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_selection_select_iter(LiVESTreeSelection * tsel, LiVESTreeIter * titer) {
#ifdef GUI_GTK
  gtk_tree_selection_select_iter(tsel, titer);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESListStore *lives_list_store_new(int ncols, ...) {
  LiVESListStore *lstore = NULL;
  va_list argList;
  va_start(argList, ncols);
#ifdef GUI_GTK
  if (ncols > 0) {
    GType types[ncols];
    int i;
    for (i = 0; i < ncols; i++) {
      types[i] = va_arg(argList, long unsigned int);
    }
    lstore = gtk_list_store_newv(ncols, types);
  }
#endif
  va_end(argList);
  return lstore;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_list_store_set(LiVESListStore * lstore, LiVESTreeIter * titer, ...) {
  boolean res = FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_list_store_set_valist(lstore, titer, argList);
  res = TRUE;
#endif
  va_end(argList);
  return res;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_list_store_insert(LiVESListStore * lstore, LiVESTreeIter * titer, int position) {
#ifdef GUI_GTK
  gtk_list_store_insert(lstore, titer, position);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_label_get_text(LiVESLabel * label) {
#ifdef GUI_GTK
  return gtk_label_get_text(label);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_text(LiVESLabel * label, const char *text) {
  if (!text) return lives_label_set_text(label, "");
  if (widget_opts.use_markup) return lives_label_set_markup(label, text);
#ifdef GUI_GTK
  if (widget_opts.mnemonic_label) {
    gtk_label_set_text_with_mnemonic(label, text);
  } else {
    gtk_label_set_text(label, text);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_markup(LiVESLabel * label, const char *markup) {
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) gtk_label_set_markup(label, markup);
  else gtk_label_set_markup_with_mnemonic(label, markup);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_mnemonic_widget(LiVESLabel * label, LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_label_set_mnemonic_widget(label, widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_label_get_mnemonic_widget(LiVESLabel * label) {
  LiVESWidget *widget = NULL;
#ifdef GUI_GTK
  widget = gtk_label_get_mnemonic_widget(label);
#endif
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_selectable(LiVESLabel * label, boolean setting) {
#ifdef GUI_GTK
  gtk_label_set_selectable(label, setting);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_editable_get_editable(LiVESEditable * editable) {
#ifdef GUI_GTK
  return gtk_editable_get_editable(editable);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_editable_set_editable(LiVESEditable * editable, boolean is_editable) {
  lives_widget_set_can_focus(LIVES_WIDGET(editable), is_editable);
#ifdef GUI_GTK
  gtk_editable_set_editable(editable, is_editable);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_editable_select_region(LiVESEditable * editable, int start_pos, int end_pos) {
#ifdef GUI_GTK
  gtk_editable_select_region(editable, start_pos, end_pos);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_entry_new(void) {
  LiVESWidget *entry = NULL;
#ifdef GUI_GTK
  entry = gtk_entry_new();
#endif
  return entry;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_max_length(LiVESEntry * entry, int len) {
  // entry length (not display length)
#ifdef GUI_GTK
  gtk_entry_set_max_length(entry, len);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_activates_default(LiVESEntry * entry, boolean act) {
#ifdef GUI_GTK
  gtk_entry_set_activates_default(entry, act);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_get_activates_default(LiVESEntry * entry) {
#ifdef GUI_GTK
  return gtk_entry_get_activates_default(entry);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_visibility(LiVESEntry * entry, boolean vis) {
#ifdef GUI_GTK
  gtk_entry_set_visibility(entry, vis);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_has_frame(LiVESEntry * entry, boolean has) {
#ifdef GUI_GTK
  gtk_entry_set_has_frame(entry, has);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_alignment(LiVESEntry * entry, float align) {
#ifdef GUI_GTK
  gtk_entry_set_alignment(entry, align);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_entry_get_text(LiVESEntry * entry) {
#ifdef GUI_GTK
  return gtk_entry_get_text(entry);
#endif
  return NULL;
}


static void _lives_entry_set_text(LiVESEntry * entry, const char *text) {
#ifdef GUI_GTK
  if (widget_opts.justify == LIVES_JUSTIFY_START) lives_entry_set_alignment(entry, 0.);
  else if (widget_opts.justify == LIVES_JUSTIFY_CENTER) lives_entry_set_alignment(entry, 0.5);
  else if (widget_opts.justify == LIVES_JUSTIFY_END) lives_entry_set_alignment(entry, 1.);
  gtk_entry_set_text(entry, text);
#endif
}


boolean lives_entry_set_text(LiVESEntry * entry, const char *text) {
  if (!LIVES_IS_ENTRY(entry)) return FALSE;
#ifdef GUI_GTK
  if (!gui_loop_tight || is_fg_thread()) {
    _lives_entry_set_text(entry, text);
  } else {
    BG_THREADVAR(hook_hints) |= HOOK_OPT_FG_LIGHT;
    MAIN_THREAD_EXECUTE_RVOID(_lives_entry_set_text, "vs", entry, text);
    BG_THREADVAR(hook_hints) = 0;
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_width_chars(LiVESEntry * entry, int nchars) {
  // display length
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 12, 0)
  if (LIVES_SHOULD_EXPAND_EXTRA_WIDTH)
    gtk_entry_set_max_width_chars(entry, 65536);
  else
    gtk_entry_set_max_width_chars(entry, nchars);
#endif
  gtk_entry_set_width_chars(entry, nchars);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_scrolled_window_new(void) {
  LiVESWidget *swindow = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(4, 0, 0)
  swindow = gtk_scrolled_window_new();
#else
  swindow = gtk_scrolled_window_new(NULL, NULL);
#endif
#endif
  return swindow;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_scrolled_window_new_with_adj(LiVESAdjustment * hadj, LiVESAdjustment * vadj) {
  LiVESWidget *swindow = lives_scrolled_window_new();
  if (swindow) {
    if (hadj) gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(swindow), hadj);
    if (vadj) gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(swindow), vadj);
  }
  return swindow;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_scrolled_window_get_hadjustment(LiVESScrolledWindow * swindow) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
  adj = gtk_scrolled_window_get_hadjustment(swindow);
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_scrolled_window_get_vadjustment(LiVESScrolledWindow * swindow) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
  adj = gtk_scrolled_window_get_vadjustment(swindow);
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_scrolled_window_get_hscrollbar(LiVESScrolledWindow * swindow) {
  LiVESWidget *scroll = NULL;
#ifdef GUI_GTK
  scroll = gtk_scrolled_window_get_hscrollbar(swindow);
#endif
  return scroll;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_scrolled_window_get_vscrollbar(LiVESScrolledWindow * swindow) {
  LiVESWidget *scroll = NULL;
#ifdef GUI_GTK
  scroll = gtk_scrolled_window_get_vscrollbar(swindow);
#endif
  return scroll;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scrolled_window_set_policy(LiVESScrolledWindow * scrolledwindow,
    LiVESPolicyType hpolicy,
    LiVESPolicyType vpolicy) {
#ifdef GUI_GTK
  gtk_scrolled_window_set_policy(scrolledwindow, hpolicy, vpolicy);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scrolled_window_add_with_viewport(LiVESScrolledWindow * scrolledwindow,
    LiVESWidget * child) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 8, 0)
  gtk_scrolled_window_add_with_viewport(scrolledwindow, child);
#else
  lives_container_add(LIVES_CONTAINER(scrolledwindow), child);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scrolled_window_set_min_content_height(LiVESScrolledWindow * scrolledwindow,
    int height) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_scrolled_window_set_min_content_height(scrolledwindow, height);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scrolled_window_set_min_content_width(LiVESScrolledWindow * scrolledwindow,
    int width) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_scrolled_window_set_min_content_width(scrolledwindow, width);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_raise(LiVESXWindow * xwin) {
#ifdef GUI_GTK
  gdk_window_raise(xwin);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_set_cursor(LiVESXWindow * xwin, LiVESXCursor * cursor) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(4, 0, 0)
  if (GDK_IS_SURFACE(xwin)) {
    if (!cursor || gdk_surface_get_display(xwin) == gdk_cursor_get_display(cursor)) {
      gdk_surface_set_cursor(xwin, cursor);
      return TRUE;
    }
  }
#else
  if (GDK_IS_WINDOW(xwin)) {
    if (!cursor || gdk_window_get_display(xwin) == gdk_cursor_get_display(cursor)) {
      gdk_window_set_cursor(xwin, cursor);
      return TRUE;
    }
  }
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_dialog_set_has_separator(LiVESDialog * dialog, boolean has) {
  // return TRUE if implemented

#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_dialog_set_has_separator(dialog, has);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_hexpand(LiVESWidget * widget, boolean state) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_hexpand(widget, state);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_vexpand(LiVESWidget * widget, boolean state) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_vexpand(widget, state);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_new(void) {
  LiVESWidget *menu = NULL;
#ifdef GUI_GTK
  menu = gtk_menu_new();
#endif
  return menu;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_bar_new(void) {
  LiVESWidget *menubar = NULL;
#ifdef GUI_GTK
  menubar = gtk_menu_bar_new();
#endif
  return menubar;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_item_new(void) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
  menuitem = gtk_menu_item_new();
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) menuitem = gtk_menu_item_new_with_label(label);
  else menuitem = gtk_menu_item_new_with_mnemonic(label);
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_item_set_accel_path(LiVESMenuItem * menuitem, const char *path) {
#ifdef GUI_GTK
  gtk_menu_item_set_accel_path(menuitem, path);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_item_get_submenu(LiVESMenuItem * menuitem) {
#ifdef GUI_GTK
  return gtk_menu_item_get_submenu(menuitem);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = NULL;
  if (!prefs->show_menu_images) return lives_menu_item_new_with_label(label);
#ifdef GUI_GTK
#if LIVES_HAS_IMAGE_MENU_ITEM
  LIVES_IGNORE_DEPRECATIONS
  if (!widget_opts.mnemonic_label) menuitem = gtk_image_menu_item_new_with_label(label);
  else menuitem = gtk_image_menu_item_new_with_mnemonic(label);
  LIVES_IGNORE_DEPRECATIONS_END
#else
  if (!widget_opts.mnemonic_label) menuitem = gtk_menu_item_new_with_label(label);
  else menuitem = gtk_menu_item_new_with_mnemonic(label);
#endif
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_radio_menu_item_new_with_label(LiVESSList * group, const char *label) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) menuitem = gtk_radio_menu_item_new_with_label(group, label);
  else menuitem = gtk_radio_menu_item_new_with_mnemonic(group, label);
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESSList *lives_radio_menu_item_get_group(LiVESRadioMenuItem * rmenuitem) {
#ifdef GUI_GTK
  return gtk_radio_menu_item_get_group(rmenuitem);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_check_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) menuitem = gtk_check_menu_item_new_with_label(label);
  else menuitem = gtk_check_menu_item_new_with_mnemonic(label);   // TODO - deprecated
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_check_menu_item_set_draw_as_radio(LiVESCheckMenuItem * item, boolean setting) {
#ifdef GUI_GTK
  gtk_check_menu_item_set_draw_as_radio(item, setting);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_menu_item_new_from_stock(const char *stock_id,
    LiVESAccelGroup * accel_group) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
#if LIVES_HAS_IMAGE_MENU_ITEM
  if (prefs->show_menu_images) {
    LIVES_IGNORE_DEPRECATIONS
    menuitem = gtk_image_menu_item_new_from_stock(stock_id, accel_group);
    LIVES_IGNORE_DEPRECATIONS_END
  } else {
#endif
    char *xstock_id = lives_strdup(stock_id); // need to back this up as we will use translation functions
    menuitem = gtk_menu_item_new_with_mnemonic(xstock_id);

    if (!strcmp(xstock_id, LIVES_STOCK_LABEL_SAVE)) {
      lives_menu_item_set_accel_path(LIVES_MENU_ITEM(menuitem), LIVES_ACCEL_PATH_SAVE);
    }

    if (!strcmp(xstock_id, LIVES_STOCK_LABEL_QUIT)) {
      lives_menu_item_set_accel_path(LIVES_MENU_ITEM(menuitem), LIVES_ACCEL_PATH_QUIT);
    }
    lives_free(xstock_id);
#if LIVES_HAS_IMAGE_MENU_ITEM
  }
#endif
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESToolItem *lives_menu_tool_button_new(LiVESWidget * icon, const char *label) {
  LiVESToolItem *toolitem = NULL;
#ifdef GUI_GTK
  toolitem = gtk_menu_tool_button_new(icon, label);
#endif
  return toolitem;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_tool_button_set_menu(LiVESMenuToolButton * toolbutton, LiVESWidget * menu) {
#ifdef GUI_GTK
  gtk_menu_tool_button_set_menu(toolbutton, menu);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_item_set_submenu(LiVESMenuItem * menuitem, LiVESWidget * menu) {
#ifdef GUI_GTK
  gtk_menu_item_set_submenu(menuitem, menu);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_item_activate(LiVESMenuItem * menuitem) {
#ifdef GUI_GTK
  gtk_menu_item_activate(menuitem);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_check_menu_item_set_active(LiVESCheckMenuItem * item, boolean state) {
#ifdef GUI_GTK
  gtk_check_menu_item_set_active(item, state);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_check_menu_item_get_active(LiVESCheckMenuItem * item) {
#ifdef GUI_GTK
  return gtk_check_menu_item_get_active(item);
#endif
  return FALSE;
}


#if LIVES_HAS_IMAGE_MENU_ITEM

WIDGET_HELPER_GLOBAL_INLINE boolean lives_image_menu_item_set_image(LiVESImageMenuItem * item, LiVESWidget * image) {
#ifdef GUI_GTK
  if (!prefs->show_menu_images) return FALSE;
  LIVES_IGNORE_DEPRECATIONS
  gtk_image_menu_item_set_image(item, image);
  LIVES_IGNORE_DEPRECATIONS_END
  return TRUE;
#endif
  return FALSE;
}

#endif

WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_set_title(LiVESMenu * menu, const char *title) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 10, 0)
  char *ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  gtk_menu_set_title(menu, ntitle);
  lives_free(ntitle);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_popup(LiVESMenu * menu, LiVESXEventButton * event) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 22, 0)
  gtk_menu_popup_at_pointer(menu, NULL);
#else
  gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_reorder_child(LiVESMenu * menu, LiVESWidget * child, int pos) {
#ifdef GUI_GTK
  gtk_menu_reorder_child(menu, child, pos);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_detach(LiVESMenu * menu) {
  // NB also calls detacher callback
#ifdef GUI_GTK
  gtk_menu_detach(menu);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_shell_append(LiVESMenuShell * menushell, LiVESWidget * child) {
#ifdef GUI_GTK
  gtk_menu_shell_append(menushell, child);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_shell_insert(LiVESMenuShell * menushell, LiVESWidget * child, int pos) {
#ifdef GUI_GTK
  gtk_menu_shell_insert(menushell, child, pos);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_shell_prepend(LiVESMenuShell * menushell, LiVESWidget * child) {
#ifdef GUI_GTK
  gtk_menu_shell_prepend(menushell, child);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_image_menu_item_set_always_show_image(LiVESImageMenuItem * item, boolean show) {
  // return TRUE if implemented
#ifdef GUI_GTK
  if (!prefs->show_menu_images) return FALSE;
#if GTK_CHECK_VERSION(2, 16, 0)
#if LIVES_HAS_IMAGE_MENU_ITEM
  LIVES_IGNORE_DEPRECATIONS
  gtk_image_menu_item_set_always_show_image(item, show);
  LIVES_IGNORE_DEPRECATIONS_END
#endif
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_set_draw_value(LiVESScale * scale, boolean draw_value) {
#ifdef GUI_GTK
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_set_value_pos(LiVESScale * scale, LiVESPositionType ptype) {
#ifdef GUI_GTK
  gtk_scale_set_value_pos(scale, ptype);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_set_digits(LiVESScale * scale, int digits) {
#ifdef GUI_GTK
  gtk_scale_set_digits(scale, digits);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_button_set_orientation(LiVESScaleButton * scale, LiVESOrientation orientation) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_orientable_set_orientation(GTK_ORIENTABLE(scale), orientation);
  return TRUE;
#else
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_scale_button_set_orientation(scale, orientation);
  return TRUE;
#endif
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_scale_button_get_value(LiVESScaleButton * scale) {
  double value = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  value = gtk_scale_button_get_value(scale);
#else
  value = gtk_adjustment_get_value(gtk_range_get_adjustment(scale));
#endif
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_button_set_value(LiVESScaleButton * scale, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_scale_button_set_value(scale, value);
#else
  gtk_adjustment_set_value(gtk_range_get_adjustment(scale), value);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE char *lives_file_chooser_get_filename(LiVESFileChooser * chooser) {
  char *fname = NULL;
#ifdef GUI_GTK
  fname = gtk_file_chooser_get_filename(chooser);
#endif
  return fname;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESSList *lives_file_chooser_get_filenames(LiVESFileChooser * chooser) {
  LiVESSList *fnlist = NULL;
#ifdef GUI_GTK
  fnlist = gtk_file_chooser_get_filenames(chooser);
#endif
  return fnlist;
}


#if GTK_CHECK_VERSION(3,2,0)
WIDGET_HELPER_GLOBAL_INLINE char *lives_font_chooser_get_font(LiVESFontChooser * fc) {
  return gtk_font_chooser_get_font(fc);
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_font_chooser_set_font(LiVESFontChooser * fc,
    const char *fontname) {
  gtk_font_chooser_set_font(fc, fontname);
  return TRUE;
}

WIDGET_HELPER_GLOBAL_INLINE LingoFontDesc *lives_font_chooser_get_font_desc(LiVESFontChooser * fc) {
  return gtk_font_chooser_get_font_desc(fc);
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_font_chooser_set_font_desc(LiVESFontChooser * fc,
    LingoFontDesc * lfd) {
  gtk_font_chooser_set_font_desc(fc, lfd);
  return TRUE;
}
#endif


#ifdef GUI_GTK
WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_grid_new(void) {
  LiVESWidget *grid = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  grid = gtk_grid_new();
#endif
#endif
  return grid;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_set_row_spacing(LiVESGrid * grid, uint32_t spacing) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_set_row_spacing(grid, spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_set_column_spacing(LiVESGrid * grid, uint32_t spacing) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_set_column_spacing(grid, spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_remove_row(LiVESGrid * grid, int posn) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  gtk_grid_remove_row(grid, posn);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_insert_row(LiVESGrid * grid, int posn) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  gtk_grid_insert_row(grid, posn);
  return TRUE;
#endif

  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_attach_next_to(LiVESGrid * grid, LiVESWidget * child, LiVESWidget * sibling,
    LiVESPositionType side, int width, int height) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_attach_next_to(grid, child, sibling, side, width, height);
  return TRUE;
#endif
#endif
  return FALSE;
}
#endif
#endif

WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_frame_new(const char *label) {
  LiVESWidget *frame = NULL;
#ifdef GUI_GTK
  frame = gtk_frame_new(label);
#endif
  return frame;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_frame_set_label(LiVESFrame * frame, const char *label) {
#ifdef GUI_GTK
  gtk_frame_set_label(frame, label);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_frame_set_label_align(LiVESFrame * frame, float xalign, float yalign) {
#ifdef GUI_GTK
  gtk_frame_set_label_align(frame, xalign, yalign);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_frame_set_label_widget(LiVESFrame * frame,
    LiVESWidget * widget) {
#ifdef GUI_GTK
  gtk_frame_set_label_widget(frame, widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_frame_get_label_widget(LiVESFrame * frame) {
  LiVESWidget *widget = NULL;
#ifdef GUI_GTK
  widget = gtk_frame_get_label_widget(frame);
#endif
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_frame_set_shadow_type(LiVESFrame * frame, LiVESShadowType stype) {
#ifdef GUI_GTK
  gtk_frame_set_shadow_type(frame, stype);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_notebook_new(void) {
  LiVESWidget *nbook = NULL;
#ifdef GUI_GTK
  nbook = gtk_notebook_new();
#endif
  return nbook;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_notebook_get_nth_page(LiVESNotebook * nbook, int pagenum) {
  LiVESWidget *page = NULL;
#ifdef GUI_GTK
  page = gtk_notebook_get_nth_page(nbook, pagenum);
#endif
  return page;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_notebook_get_current_page(LiVESNotebook * nbook) {
  int pagenum = -1;
#ifdef GUI_GTK
  pagenum = gtk_notebook_get_current_page(nbook);
#endif
  return pagenum;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_notebook_set_current_page(LiVESNotebook * nbook, int pagenum) {
#ifdef GUI_GTK
  gtk_notebook_set_current_page(nbook, pagenum);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_notebook_set_tab_label(LiVESNotebook * nbook, LiVESWidget * child,
    LiVESWidget * tablabel) {
#ifdef GUI_GTK
  gtk_notebook_set_tab_label(nbook, child, tablabel);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_table_new(uint32_t rows, uint32_t cols, boolean homogeneous) {
  LiVESWidget *table = NULL;
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  int i;
  GtkGrid *grid = (GtkGrid *)lives_grid_new();
  gtk_grid_set_row_homogeneous(grid, homogeneous);
  gtk_grid_set_column_homogeneous(grid, homogeneous);

  for (i = 0; i < rows; i++) {
    lives_grid_insert_row(grid, 0);
  }

  for (i = 0; i < cols; i++) {
    gtk_grid_insert_column(grid, 0);
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(grid), ROWS_KEY, LIVES_INT_TO_POINTER(rows));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(grid), COLS_KEY, LIVES_INT_TO_POINTER(cols));
  table = (LiVESWidget *)grid;
#else
  table = gtk_table_new(rows, cols, homogeneous);
#endif
#endif
  return table;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_set_row_spacings(LiVESTable * table, uint32_t spacing) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  lives_grid_set_row_spacing(table, spacing);
#else
  gtk_table_set_row_spacings(table, spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_set_col_spacings(LiVESTable * table, uint32_t spacing) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  lives_grid_set_column_spacing(table, spacing);
#else
  gtk_table_set_col_spacings(table, spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_set_row_homogeneous(LiVESTable * table, boolean homogeneous) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID
  gtk_grid_set_row_homogeneous(table, homogeneous);
  return TRUE;
#else
  gtk_table_set_homogeneous(table, homogeneous);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_set_column_homogeneous(LiVESTable * table, boolean homogeneous) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID
  gtk_grid_set_column_homogeneous(table, homogeneous);
  return TRUE;
#else
  gtk_table_set_homogeneous(table, homogeneous);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_resize(LiVESTable * table, uint32_t rows, uint32_t cols) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  int i;

  for (i = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(table), ROWS_KEY)); i < rows; i++) {
    lives_grid_insert_row(table, i);
  }

  for (i = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(table), COLS_KEY)); i < cols; i++) {
    gtk_grid_insert_column(table, i);
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(table), ROWS_KEY, LIVES_INT_TO_POINTER(rows));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(table), COLS_KEY, LIVES_INT_TO_POINTER(cols));
#else
  gtk_table_resize(table, rows, cols);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_attach(LiVESTable * table, LiVESWidget * child, uint32_t left, uint32_t right,
    uint32_t top, uint32_t bottom, LiVESAttachOptions xoptions, LiVESAttachOptions yoptions,
    uint32_t xpad, uint32_t ypad) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  gtk_grid_attach(table, child, left, top, right - left, bottom - top);
  if (xoptions & LIVES_EXPAND)
    lives_widget_set_hexpand(child, TRUE);
  else
    lives_widget_set_hexpand(child, FALSE);
  if (yoptions & LIVES_EXPAND)
    lives_widget_set_vexpand(child, TRUE);
  else
    lives_widget_set_vexpand(child, FALSE);

  lives_widget_set_margin_left(child, xpad);
  lives_widget_set_margin_right(child, xpad);

  lives_widget_set_margin_top(child, ypad);
  lives_widget_set_margin_bottom(child, ypad);
#else
  gtk_table_attach(table, child, left, right, top, bottom, xoptions, yoptions, xpad, ypad);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_color_button_new_with_color(const LiVESWidgetColor * color) {
  LiVESWidget *cbutton = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  cbutton = gtk_color_button_new_with_rgba(color);
#else
  cbutton = gtk_color_button_new_with_color(color);
#endif
#endif
  return cbutton;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetColor *lives_color_button_get_color(LiVESColorButton * button,
    LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_color_chooser_get_rgba((GtkColorChooser *)button, color);
#else
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_color_button_get_rgba((GtkColorChooser *)button, color);
#else
  gtk_color_button_get_color(button, color);
#endif
#endif
  return color;
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_button_set_alpha(LiVESColorButton * button, int16_t alpha) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  LiVESWidgetColor color;
  gtk_color_chooser_get_rgba((GtkColorChooser *)button, &color);
  color.alpha = LIVES_WIDGET_COLOR_SCALE_65535(alpha);
  gtk_color_chooser_set_rgba((GtkColorChooser *)button, &color);
#else
  gtk_color_button_set_alpha(button, alpha);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int16_t lives_color_button_get_alpha(LiVESColorButton * button) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  LiVESWidgetColor color;
  gtk_color_chooser_get_rgba((GtkColorChooser *)button, &color);
  return LIVES_WIDGET_COLOR_STRETCH(color.alpha);
#else
  return gtk_color_button_get_alpha(button);
#endif
#endif
  return -1;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_button_set_color(LiVESColorButton * button, const LiVESWidgetColor * color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_color_chooser_set_rgba((GtkColorChooser *)button, color);
#else
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_color_button_set_rgba((GtkColorChooser *)button, color);
#else
  gtk_color_button_set_color(button, color);
#endif
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_button_set_title(LiVESColorButton * button, const char *title) {
#ifdef GUI_GTK
  char *ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  gtk_color_button_set_title(button, title);
  lives_free(ntitle);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_button_set_use_alpha(LiVESColorButton * button, boolean use_alpha) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_color_chooser_set_use_alpha((GtkColorChooser *)button, use_alpha);
#else
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_color_button_set_use_alpha((GtkColorChooser *)button, use_alpha);
#else
  gtk_color_button_set_use_alpha(button, use_alpha);
#endif
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_LOCAL_INLINE boolean lives_widget_get_mods(LiVESXDevice * device, LiVESWidget * widget, int *x, int *y,
    LiVESXModifierType * modmask) {
#ifdef GUI_GTK
  LiVESXWindow *xwin;
  if (!widget) xwin = gdk_get_default_root_window();
  else xwin = lives_widget_get_xwindow(widget);
  if (!xwin) {
    LIVES_ERROR("Tried to get pointer for windowless widget");
    return TRUE;
  }
#if GTK_CHECK_VERSION(3, 0, 0)
  gdk_window_get_device_position(xwin, device, x, y, modmask);
#else
  gdk_window_get_pointer(xwin, x, y, modmask);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_pointer(LiVESXDevice * device, LiVESWidget * widget, int *x, int *y) {
  return lives_widget_get_mods(device, widget, x, y, NULL);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_modmask(LiVESXDevice * device, LiVESWidget * widget,
    LiVESXModifierType * modmask) {
  return lives_widget_get_mods(device, widget, NULL, NULL, modmask);
}


static boolean lives_widget_timetodie(LiVESWidget * widget, LiVESWidget * getoverhere) {
  if (LIVES_IS_WIDGET(getoverhere)) lives_widget_destroy(getoverhere);
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_nullify_with(LiVESWidget * widget, void **ptr) {
  lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_DESTROY_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_nullify_ptr_cb), ptr);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_destroy_with(LiVESWidget * widget, LiVESWidget * dieplease) {
  lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_DESTROY_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_widget_timetodie), dieplease);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESXDisplay *lives_widget_get_display(LiVESWidget * widget) {
  LiVESXDisplay *disp = NULL;
#ifdef GUI_GTK
  disp = gtk_widget_get_display(widget);
#endif
  return disp;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESXWindow *lives_display_get_window_at_pointer
(LiVESXDevice * device, LiVESXDisplay * display, int *win_x, int *win_y) {
  LiVESXWindow *xwindow = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (!device) return NULL;
  xwindow = gdk_device_get_window_at_position(device, win_x, win_y);
#else
  xwindow = gdk_display_get_window_at_pointer(display, win_x, win_y);
#endif
#endif
  return xwindow;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_display_get_pointer
(LiVESXDevice * device, LiVESXDisplay * display, LiVESXScreen **screen, int *x, int *y, LiVESXModifierType * mask) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (!device) return TRUE;
  gdk_device_get_position(device, screen, x, y);
#else
  gdk_display_get_pointer(display, screen, x, y, mask);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_display_warp_pointer
(LiVESXDevice * device, LiVESXDisplay * display, LiVESXScreen * screen, int x, int y) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (!device) return TRUE;
  gdk_device_warp(device, screen, x, y);
#else
#if GLIB_CHECK_VERSION(2, 8, 0)
  gdk_display_warp_pointer(display, screen, x, y);
#endif
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE lives_display_t lives_widget_get_display_type(LiVESWidget * widget) {
  lives_display_t dtype = LIVES_DISPLAY_TYPE_UNKNOWN;
#ifdef GUI_GTK
  if (GDK_IS_X11_DISPLAY(gtk_widget_get_display(widget))) dtype = LIVES_DISPLAY_TYPE_X11;
#ifdef GDK_WINDOWING_WAYLAND
  else if (GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(widget))) dtype = LIVES_DISPLAY_TYPE_WAYLAND;
#endif
  else if (GDK_IS_WIN32_DISPLAY(gtk_widget_get_display(widget))) dtype = LIVES_DISPLAY_TYPE_WIN32;
#endif
  return dtype;
}


WIDGET_HELPER_GLOBAL_INLINE lives_display_t lives_xwindow_get_display_type(LiVESXWindow * xwin) {
  lives_display_t dtype = LIVES_DISPLAY_TYPE_UNKNOWN;
#ifdef GUI_GTK
  if (GDK_IS_X11_DISPLAY(gdk_window_get_display(xwin))) dtype = LIVES_DISPLAY_TYPE_X11;
#ifdef GDK_WINDOWING_WAYLAND
  else if (GDK_IS_WAYLAND_DISPLAY(gdk_window_get_display(xwin))) dtype = LIVES_DISPLAY_TYPE_WAYLAND;
#endif
  else if (GDK_IS_WIN32_DISPLAY(gdk_window_get_display(xwin))) dtype = LIVES_DISPLAY_TYPE_WIN32;
#endif
  return dtype;
}


WIDGET_HELPER_GLOBAL_INLINE uint64_t lives_xwindow_get_xwinid(LiVESXWindow * xwin, const char *msg) {
#ifdef GUI_GTK
#ifdef GDK_WINDOWING_X11
  if (lives_xwindow_get_display_type(xwin) == LIVES_DISPLAY_TYPE_X11)
    return GDK_WINDOW_XID(xwin);
#endif
#ifdef GDK_WINDOWING_WIN32
  if (lives_xwindow_get_display_type(xwin) == LIVES_DISPLAY_TYPE_WIN32)
    return gdk_win32_window_get_handle(xwin);
#endif
#endif
  if (msg) LIVES_WARN(msg);
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE uint64_t lives_widget_get_xwinid(LiVESWidget * widget, const char *msg) {
  return lives_xwindow_get_xwinid(lives_widget_get_xwindow(widget), msg);
}


WIDGET_HELPER_GLOBAL_INLINE uint32_t lives_timer_immediate(LiVESWidgetSourceFunc func, livespointer data) {
  uint32_t source = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  source = g_timeout_add_full(0, LIVES_WIDGET_PRIORITY_HIGH, func, data, NULL);
#else
  source =  gtk_timeout_add_full(0, LIVES_WIDGET_PRIORITY_HIGH, func, data, NULL);
#endif
  g_source_set_can_recurse(g_main_context_find_source_by_id(NULL, source), FALSE);
#endif
  return source;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_source_set_priority(LiVESWidgetSource * source, int32_t prio) {
#ifdef GUI_GTK
  g_source_set_priority(source, prio);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_source_set_callback(LiVESWidgetSource * source, LiVESWidgetSourceFunc func,
    livespointer data) {
#ifdef GUI_GTK
  g_source_set_callback(source, func, data, NULL);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_source_attach(LiVESWidgetSource * source, LiVESWidgetContext * ctx) {
#ifdef GUI_GTK
  g_source_attach(source, ctx);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_source_remove(uint32_t src_handle) {
#ifdef GUI_GTK
  g_source_remove(src_handle);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetSource *lives_thrd_idle_priority(LiVESWidgetSourceFunc function,
    livespointer data) {
  LiVESWidgetSource *source = lives_idle_source_new();
  lives_source_set_callback(source, function, data);
  lives_source_set_priority(source, LIVES_WIDGET_PRIORITY_HIGH);
  lives_source_attach(source, THREADVAR(guictx));
  return source;
}

// priority her is  nothing to do with gui callback priority
WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetSource *lives_idle_priority(LiVESWidgetSourceFunc function, livespointer data) {
  LiVESWidgetSource *source = lives_idle_source_new();
  lives_source_set_callback(source, function, data);
  lives_source_set_priority(source, LIVES_WIDGET_PRIORITY_HIGH);
  lives_source_attach(source, g_main_context_default());
  g_source_set_can_recurse(source, FALSE);
  return source;
}


WIDGET_HELPER_GLOBAL_INLINE uint32_t lives_accelerator_get_default_mod_mask(void) {
#ifdef GUI_GTK
  return gtk_accelerator_get_default_mod_mask();
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_screen_get_width(LiVESXScreen * screen) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 22, 0)
  return gdk_screen_get_width(screen);
#endif
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_screen_get_height(LiVESXScreen * screen) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 22, 0)
  return gdk_screen_get_height(screen);
#endif
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE boolean global_recent_manager_add(const char *full_file_name) {
#ifdef GUI_GTK
  char *tmp = g_filename_to_uri(full_file_name, NULL, NULL);
  gtk_recent_manager_add_item(gtk_recent_manager_get_default(), tmp);
  g_free(tmp);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESXCursor *lives_cursor_new_from_pixbuf(LiVESXDisplay * disp, LiVESPixbuf * pixbuf, int x,
    int y) {
  LiVESXCursor *cursor = NULL;
#ifdef GUI_GTK
  cursor = gdk_cursor_new_from_pixbuf(disp, pixbuf, x, y);
#endif
  return cursor;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_has_toplevel_focus(LiVESWidget * widget) {
#ifdef GUI_GTK
  return gtk_window_has_toplevel_focus(LIVES_WINDOW(widget));
#endif
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_editable(LiVESEntry * entry, boolean editable) {
  return lives_editable_set_editable(LIVES_EDITABLE(entry), editable);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_get_editable(LiVESEntry * entry) {
  return lives_editable_get_editable(LIVES_EDITABLE(entry));
}


// compound functions

WIDGET_HELPER_GLOBAL_INLINE boolean lives_image_scale(LiVESImage * image, int width, int height, LiVESInterpType interp_type) {
  LiVESPixbuf *pixbuf;
  if (!LIVES_IS_IMAGE(image)) return FALSE;
  pixbuf = lives_image_get_pixbuf(image);
  if (pixbuf) {
    LiVESPixbuf *new_pixbuf = lives_pixbuf_scale_simple(pixbuf, width, height, interp_type);
    lives_image_set_from_pixbuf(image, new_pixbuf);
    //if (LIVES_IS_WIDGET_OBJECT(pixbuf)) lives_widget_object_unref(pixbuf);
    if (new_pixbuf && LIVES_IS_WIDGET_OBJECT(new_pixbuf)) lives_widget_object_unref(new_pixbuf);
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_pack_type(LiVESBox * box, LiVESWidget * child, LiVESPackType pack) {
#ifdef GUI_GTK
  boolean expand, fill;
  uint32_t padding;
  gtk_box_query_child_packing(box, child, &expand, &fill, &padding, NULL);
  lives_box_set_child_packing(box, child, expand, fill, padding, pack);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE void lives_label_set_hpadding(LiVESLabel * label, int pad) {
  const char *text = lives_label_get_text(label);
  lives_label_set_width_chars(label, strlen(text) + pad);
}


#define H_ALIGN_ADJ (22. * widget_opts.scaleH) // why 22 ? no idea

WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *align_horizontal_with(LiVESWidget * thingtoadd, LiVESWidget * thingtoalignwith) {
#ifdef GUI_GTK
  GtkWidget *fixed = gtk_fixed_new();
  int x = lives_widget_get_allocation_x(thingtoalignwith);
  // allow for 1 packing_width before adding the real widget
  gtk_fixed_put(GTK_FIXED(fixed), thingtoadd, x - H_ALIGN_ADJ - widget_opts.packing_width, 0);
  lives_widget_set_show_hide_parent(thingtoadd);
  return fixed;
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_pack_first(LiVESBox * box, LiVESWidget * child, boolean expand, boolean fill,
    uint32_t padding) {
  if (lives_box_pack_start(box, child, expand, fill, padding))
    return lives_box_reorder_child(box, child, 0);
  return FALSE;
}


void lives_tooltips_copy(LiVESWidget * dest, LiVESWidget * source) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  boolean mustfree = TRUE;
  char *text = (char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(source), TTIPS_KEY);
  if (!text) text = gtk_widget_get_tooltip_text(source);
  else mustfree = FALSE;
  lives_widget_set_tooltip_text(dest, text);
  if (mustfree && text) lives_free(text);
#else
  GtkTooltipsData *td = gtk_tooltips_data_get(source);
  if (!td) return;
  gtk_tooltips_set_tip(td->tooltips, dest, td->tip_text, td->tip_private);
#endif
#endif
}


boolean lives_combo_populate(LiVESCombo * combo, LiVESList * list) {
  LiVESList *revlist;

  // remove any current list
  LiVESTreeModel *tmodel = lives_combo_get_model(combo);
  if (tmodel) {
    if (!lives_combo_set_active_index(combo, -1)) return FALSE;
    if (!lives_combo_remove_all_text(combo)) return FALSE;
  }

  if (lives_list_length(list) > COMBO_LIST_LIMIT) {
    // use a treestore
    LiVESTreeIter iter1, iter2;
    LiVESTreeStore *tstore = lives_tree_store_new(1, LIVES_COL_TYPE_STRING);
    char *cat;
    for (revlist = list; revlist; revlist = revlist->next) {
      cat = lives_strndup((const char *)revlist->data, 1);
      // returns the iter for cat if it already exists, else appends cat and returns it
      lives_tree_store_find_iter(tstore, 0, cat, NULL, &iter1);
      lives_tree_store_append(tstore, &iter2, &iter1);   /* Acquire an iterator */
      lives_tree_store_set(tstore, &iter2, 0, revlist->data, -1);
      lives_free(cat);
    }
    lives_combo_set_model(LIVES_COMBO(combo), LIVES_TREE_MODEL(tstore));
    lives_combo_set_entry_text_column(combo, 0);
  } else {
    // reverse the list and then prepend the items
    // this is faster (O(1) than traversing the list and appending O(2))
    LiVESTreeIter iter;
    LiVESListStore *lstore = lives_list_store_new(1, LIVES_COL_TYPE_STRING);
    for (revlist = lives_list_last(list); revlist; revlist = revlist->prev) {
      gtk_list_store_prepend(lstore, &iter);   /* Acquire an iterator */
      gtk_list_store_set(GTK_LIST_STORE(lstore), &iter, 0, revlist->data, -1);
    }
    lives_combo_set_model(LIVES_COMBO(combo), LIVES_TREE_MODEL(lstore));
    lives_combo_set_entry_text_column(combo, 0);
  }
  return TRUE;
}


///// lives compounds

LiVESWidget *lives_volume_button_new(LiVESOrientation orientation, LiVESAdjustment * adj, double volume) {
  LiVESWidget *volume_scale = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  volume_scale = gtk_volume_button_new();
  gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_scale), volume);
  lives_scale_button_set_orientation(LIVES_SCALE_BUTTON(volume_scale), orientation);
#else
  if (orientation == LIVES_ORIENTATION_HORIZONTAL)
    volume_scale = gtk_hscale_new(adj);
  else
    volume_scale = gtk_vscale_new(adj);

  if (widget_opts.apply_theme) {
    set_css_value_direct(volume_scale, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "0px");
  }

  gtk_scale_set_draw_value(GTK_SCALE(volume_scale), FALSE);
#endif
#endif
  return volume_scale;
}


boolean lives_button_ungrab_default_special(LiVESWidget * button) {
  LiVESWidget *toplevel = lives_widget_get_toplevel(button);
  LiVESWidget *deflt = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(toplevel), DEFBUTTON_KEY);

  lives_widget_set_can_default(button, FALSE);
  if (button == deflt)
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(LIVES_WIDGET_OBJECT(toplevel)),
                                 DEFBUTTON_KEY, NULL);
#ifdef USE_SPECIAL_BUTTONS
  render_standard_button(LIVES_BUTTON(button));
#endif
  return TRUE;
}


boolean lives_button_grab_default_special(LiVESWidget * button) {
  // grab default and set as default default
  if (!lives_widget_set_can_focus_and_default(button)) return FALSE;
  if (!lives_widget_grab_default(button)) return FALSE;
  else {
    LiVESWidget *toplevel = lives_widget_get_toplevel(button);
    LiVESWidget *deflt = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(toplevel), DEFBUTTON_KEY);
    if (button == deflt) return TRUE;
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(LIVES_WIDGET_OBJECT(toplevel)),
                                 DEFBUTTON_KEY, button);
#ifdef USE_SPECIAL_BUTTONS
    render_standard_button(LIVES_BUTTON(button));
    if (deflt) render_standard_button(LIVES_BUTTON(deflt));
#endif
  }
  return TRUE;
}

static void _set_css_min_size(LiVESWidget * w, const char *sel, int mw, int mh) {
#if GTK_CHECK_VERSION(3, 16, 0)
  char *tmp;
  if (mw > 0) {
    tmp = lives_strdup_printf("%dpx", mw);
    set_css_value_direct(w, LIVES_WIDGET_STATE_NORMAL, sel, "min-width", tmp);
    lives_free(tmp);
  }
  if (mh > 0) {
    tmp = lives_strdup_printf("%dpx", mh);
    set_css_value_direct(w, LIVES_WIDGET_STATE_NORMAL, sel, "min-height", tmp);
    lives_free(tmp);
  }
#endif
}

void set_css_min_size(LiVESWidget * w, int mw, int mh) {
  _set_css_min_size(w, "", mw, mh);
  _set_css_min_size(w, "*", mw, mh);
}

static void set_css_min_size_selected(LiVESWidget * w, char *selector, int mw, int mh) {
  _set_css_min_size(w, selector, mw, mh);
}


///////////////// lives_layout ////////////////////////

WIDGET_HELPER_LOCAL_INLINE void lives_layout_attach(LiVESLayout * layout, LiVESWidget * widget, int start, int end, int row) {
  lives_table_attach(layout, widget, start, end, row, row + 1,
                     (LiVESAttachOptions)(LIVES_FILL | (LIVES_SHOULD_EXPAND_EXTRA_WIDTH
                                          ? LIVES_EXPAND : 0)), (LiVESAttachOptions)(0), 0, 0);
}


/**
   This function adds an 'expansion row' to a LiVES layout. An expansion row is a single box which
   expands to the width of a layout row. This can be useful to prevent a layout column from becoming overly wide.
   The function can be used in two ways, if the second parameter is NULL, then an hbox is returned which a widget
   can then be packed into. Otherwise, the second parameter can be set to point to an existing widget in the layout,
   allowing it to be converted to an expansion row after it has already been added, permitting the use of anonymous
   widgets.
*/
LiVESWidget *lives_layout_expansion_row_new(LiVESLayout * layout, LiVESWidget * widget) {
  LiVESList *xwidgets = (LiVESList *)lives_widget_object_steal_data(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY);
  LiVESWidget *box = NULL;
  int rows, columns;
  if (widget) box = lives_widget_get_parent(widget);

  if (!box) {
    box = lives_layout_row_new(layout);
    if (widget) lives_layout_pack(LIVES_HBOX(box), widget);
  }

  columns = GET_INT_DATA(layout, COLS_KEY);
  rows = GET_INT_DATA(layout, ROWS_KEY);
  if (columns > 1) {
    lives_widget_object_ref(LIVES_WIDGET_OBJECT(box));
    lives_widget_unparent(box);
    lives_layout_attach(layout, box, 0, columns, rows - 1);
    lives_widget_object_unref(LIVES_WIDGET_OBJECT(box));
  }
  lives_widget_set_halign(box, LIVES_ALIGN_FILL);
  lives_widget_set_hexpand(box, TRUE);
  xwidgets = lives_list_prepend(xwidgets, box);
  lives_widget_object_set_data_list(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY, xwidgets);
  SET_INT_DATA(box, LROW_KEY, rows - 1);
  SET_INT_DATA(box, EXPANSION_KEY, widget_opts.expand);
  SET_INT_DATA(box, JUST_KEY, widget_opts.justify);
  widget_opts.last_container = box;
  if (widget) return widget;
  return box;
}


static boolean lives_layout_resize(LiVESLayout * layout, int rows, int columns) {
  LiVESList *xwidgets = (LiVESList *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY);
  lives_table_resize(LIVES_TABLE(layout), rows, columns);
  SET_INT_DATA(layout, ROWS_KEY, rows);
  SET_INT_DATA(layout, COLS_KEY, columns);
  while (xwidgets) {
    LiVESWidget *widget = (LiVESWidget *)xwidgets->data;
    LiVESJustification justification = GET_INT_DATA(widget, JUST_KEY);
    LiVESJustification woj = widget_opts.justify;
    int row = GET_INT_DATA(widget, LROW_KEY);
    int expansion = GET_INT_DATA(widget, EXPANSION_KEY);
    int woe = widget_opts.expand;
    lives_widget_object_ref(widget);
    lives_widget_unparent(widget);
    widget_opts.expand = expansion;
    widget_opts.justify = justification;
    lives_layout_attach(layout, widget, 0, columns, row);
    widget_opts.expand = woe;
    widget_opts.justify = woj;
    lives_widget_object_unref(widget);
    xwidgets = xwidgets->next;
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_pack(LiVESHBox * box, LiVESWidget * widget) {
  LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box), WH_LAYOUT_KEY);
  if (layout) {
    LiVESList *xwidgets = (LiVESList *)lives_widget_object_steal_data(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY);
    int row = GET_INT_DATA(layout, ROWS_KEY) - 1;
    // remove any expansion widgets on this row
    if (xwidgets) {
      LiVESList *list = xwidgets;
      for (; list; list = list->next) {
        if (GET_INT_DATA(xwidgets->data, LROW_KEY) == row) {
          if (list->prev) list->prev->next = list->next;
          else xwidgets = list->next;
          if (list->next) list->next->prev = list->prev;
          list->prev = list->next = NULL;
          lives_list_free(list);
          break;
	  // *INDENT-OFF*
        }}
      lives_widget_object_set_data_list(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY, xwidgets);
    }}
  // *INDENT-ON*
  lives_box_pack_start(LIVES_BOX(box), widget, LIVES_SHOULD_EXPAND_EXTRA_WIDTH
                       || (LIVES_IS_LABEL(widget) && LIVES_SHOULD_EXPAND_WIDTH),
                       TRUE, LIVES_SHOULD_EXPAND_WIDTH ? widget_opts.packing_width >> 1 : 0);
  lives_widget_set_halign(widget, lives_justify_to_align(widget_opts.justify));
  lives_widget_set_show_hide_parent(widget);
  widget_opts.last_container = LIVES_WIDGET(box);
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_new(LiVESBox * box) {
  LiVESWidget *layout = lives_table_new(0, 0, FALSE);
  if (box) {
    if (LIVES_IS_VBOX(box)) {
      lives_box_pack_start(box, layout, LIVES_SHOULD_EXPAND_EXTRA_HEIGHT, TRUE,
                           LIVES_SHOULD_EXPAND_HEIGHT ? widget_opts.packing_height : 0);
    } else {
      lives_box_pack_start(box, layout, LIVES_SHOULD_EXPAND_EXTRA_WIDTH, TRUE,
                           LIVES_SHOULD_EXPAND_WIDTH ? widget_opts.packing_width : 0);
    }
  }
  SET_INT_DATA(layout, ROWS_KEY, 1);
  SET_INT_DATA(layout, COLS_KEY, 0);
  SET_INT_DATA(layout, WADDED_KEY, 0);
  lives_widget_object_set_data_list(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY, NULL);
  lives_table_set_col_spacings(LIVES_TABLE(layout), 0);
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_table_set_row_spacings(LIVES_TABLE(layout), widget_opts.packing_height);
  if (LIVES_SHOULD_EXPAND_EXTRA_WIDTH)
    lives_table_set_col_spacings(LIVES_TABLE(layout), widget_opts.packing_width);
  return layout;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_hbox_new(LiVESLayout * layout) {
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
  int nadded = GET_INT_DATA(layout, WADDED_KEY);
  int rows = GET_INT_DATA(layout, ROWS_KEY);
  int columns = GET_INT_DATA(layout, COLS_KEY);

#if GTK_CHECK_VERSION(3, 0, 0)
  LiVESWidget *widget = hbox;
#else
  LiVESWidget *alignment =
    lives_alignment_new(widget_opts.justify == LIVES_JUSTIFY_CENTER ? 0.5
                        : widget_opts.justify == LIVES_JUSTIFY_END
                        ? 1. : 0., .5, 0., 0.);
  LiVESWidget *widget = alignment;
  lives_container_add(LIVES_CONTAINER(alignment), hbox);
#endif

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, layout);
  if (++nadded > columns) lives_layout_resize(layout, rows, nadded);
  lives_layout_attach(layout, widget, nadded - 1, nadded, rows - 1);
  SET_INT_DATA(layout, WADDED_KEY, nadded);
  if (widget_opts.apply_theme == 2) {
    lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_NORMAL, TRUE);
  }
#if GTK_CHECK_VERSION(3, 0, 0)
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_widget_set_valign(widget, LIVES_ALIGN_FILL);
  else
    lives_widget_set_valign(widget, LIVES_ALIGN_CENTER);
  if (widget_opts.justify == LIVES_JUSTIFY_CENTER)
    lives_widget_set_halign(widget, LIVES_ALIGN_CENTER);
  else if (widget_opts.justify == LIVES_JUSTIFY_END)
    lives_widget_set_halign(widget, LIVES_ALIGN_END);
  else
    lives_widget_set_halign(widget, LIVES_ALIGN_START);
#endif
  return hbox;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_layout_add_row(LiVESLayout * layout) {
  int rows = GET_INT_DATA(layout, ROWS_KEY);
  SET_INT_DATA(layout, ROWS_KEY, ++rows);
  SET_INT_DATA(layout, WADDED_KEY, 0);
  return rows;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_row_new(LiVESLayout * layout) {
  lives_layout_add_row(layout);
  return lives_layout_hbox_new(layout);
}


WIDGET_HELPER_GLOBAL_INLINE void lives_layout_label_set_text(LiVESLabel * label, const char *text) {
  if (text) {
    char *markup, *full_markup;
    if (!widget_opts.use_markup) markup = lives_markup_escape_text(text, -1);
    else markup = (char *)text;
    full_markup = lives_big_and_bold(markup);
    lives_label_set_markup(label, full_markup);
    if (markup != text) lives_free(markup);
    lives_free(full_markup);
  }
}


WIDGET_HELPER_GLOBAL_INLINE
LiVESWidget *lives_layout_add_label(LiVESLayout * layout, const char *text, boolean horizontal) {
  LiVESWidget *hbox, *label, *conter;
  if (horizontal) {
    hbox = lives_layout_hbox_new(layout);
    label = lives_standard_label_new_with_tooltips(text, LIVES_BOX(hbox), NULL);
    conter = widget_opts.last_container;
    lives_widget_object_ref(conter);
    lives_widget_unparent(conter);
    lives_layout_pack(LIVES_HBOX(hbox), conter);
    widget_opts.last_container = hbox;
#if GTK_CHECK_VERSION(3, 16, 0)
    if (LIVES_SHOULD_EXPAND_HEIGHT)
      set_css_min_size(label, widget_opts.css_min_width, ((widget_opts.css_min_height * 3 + 3) >> 2) << 1);
    lives_widget_set_size_request(label, -1, (((widget_opts.css_min_height * 3 + 3) >> 2) << 1) + widget_opts.packing_height);
#endif
  } else {
    hbox = lives_hbox_new(FALSE, 0);
    label = lives_standard_label_new_with_tooltips(NULL, LIVES_BOX(hbox), NULL);
    conter = widget_opts.last_container;
    lives_layout_label_set_text(LIVES_LABEL(label), text);
    lives_widget_object_ref(conter);
    lives_widget_unparent(conter);
    lives_widget_destroy(hbox);
    lives_layout_expansion_row_new(layout, conter);
    hbox = widget_opts.last_container;
  }

  widget_opts.last_label = label;
  lives_widget_object_unref(conter);
  if (widget_opts.apply_theme == 2) set_child_alt_colour(hbox, TRUE);

#if GTK_CHECK_VERSION(3, 0, 0)
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_widget_set_valign(label, LIVES_ALIGN_FILL);
  else
    lives_widget_set_valign(label, LIVES_ALIGN_CENTER);

  if (widget_opts.justify == LIVES_JUSTIFY_CENTER) {
    lives_widget_set_halign(hbox, LIVES_ALIGN_CENTER);
  } else {
    if (widget_opts.justify == LIVES_JUSTIFY_END)
      lives_widget_set_halign(hbox, LIVES_ALIGN_END);
    else
      lives_widget_set_halign(hbox, LIVES_ALIGN_START);
  }
  if (LIVES_SHOULD_EXPAND_EXTRA_WIDTH)
    lives_widget_set_halign(hbox, LIVES_ALIGN_FILL);
#endif
  return label;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_add_fill(LiVESLayout * layout, boolean horizontal) {
  LiVESWidget *widget;
  if (horizontal) {
    LiVESWidget *hbox = lives_layout_hbox_new(layout);
    widget = add_fill_to_box(LIVES_BOX(hbox));
    widget_opts.last_container = hbox;
    lives_widget_set_hexpand(hbox, TRUE);
    lives_table_set_column_homogeneous(LIVES_TABLE(layout), FALSE);
    if (LIVES_SHOULD_EXPAND_WIDTH)
      lives_widget_set_halign(hbox, LIVES_ALIGN_FILL);
  } else {
    widget = lives_layout_add_label(layout, NULL, FALSE);
    lives_layout_expansion_row_new(layout, widget);
  }
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_add_separator(LiVESLayout * layout, boolean horizontal) {
  // here, horizontal means on the same line as other widgets, ie. a vseparator
  // vertical means below preceding widgets, ie. an hseparator
  LiVESWidget *separator;
  LiVESJustification woj = widget_opts.justify;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  if (horizontal) separator = lives_layout_pack(LIVES_HBOX(lives_layout_hbox_new(layout)), lives_vseparator_new());
  else separator = add_hsep_to_box(LIVES_BOX(lives_layout_expansion_row_new(layout, NULL)));
  widget_opts.justify = woj;
  return separator;
}

////////////////////////////////////////////////////////////////////

static LiVESWidget *add_warn_image(LiVESWidget * widget, LiVESWidget * hbox) {
  LiVESWidget *warn_image = lives_image_find_in_stock_at_size(widget_opts.css_min_height + 4, "dialog-warning", NULL);

  if (hbox) {
    if (!widget_opts.pack_end)
      lives_box_pack_start(LIVES_BOX(hbox), warn_image, FALSE, FALSE, 4);
    else
      lives_box_pack_end(LIVES_BOX(hbox), warn_image, FALSE, FALSE, 4);
  }
  lives_widget_set_no_show_all(warn_image, TRUE);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(warn_image), TTIPS_OVERRIDE_KEY, warn_image);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), WARN_IMAGE_KEY, warn_image);
  lives_widget_set_sensitive_with(widget, warn_image);
#if GTK_CHECK_VERSION(3, 16, 0)
  if (widget_opts.apply_theme) {
    set_css_value_direct(warn_image, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.8");
  }
#endif
  return warn_image;
}


WIDGET_HELPER_GLOBAL_INLINE boolean show_warn_image(LiVESWidget * widget, const char *text) {
  LiVESWidget *warn_image;
  if (!(warn_image = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), WARN_IMAGE_KEY))) return FALSE;
  if (text) lives_widget_set_tooltip_text(warn_image, text);
  lives_widget_set_no_show_all(warn_image, FALSE);
  _lives_widget_show_all(warn_image);
  lives_widget_set_sensitive(warn_image, lives_widget_get_sensitive(widget));
  if (is_standard_widget(widget)) {
    if (LIVES_IS_ENTRY(widget) || LIVES_IS_SPIN_BUTTON(widget) || LIVES_IS_COMBO(widget)) {
      char *colref = gdk_rgba_to_string(&palette->dark_red);
      if (LIVES_IS_COMBO(widget))
        widget = lives_combo_get_entry(LIVES_COMBO(widget));
      set_css_value_direct(widget, LIVES_WIDGET_STATE_NORMAL, "", "border-color", colref);
      set_css_value_direct(widget, LIVES_WIDGET_STATE_FOCUSED, "", "border-color", colref);
      set_css_value_direct(widget, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "4px");
      set_css_value_direct(widget, LIVES_WIDGET_STATE_FOCUSED, "", "border-width", "4px");
      lives_free(colref);
    }
  }
  return TRUE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean hide_warn_image(LiVESWidget * widget) {
  LiVESWidget *warn_image;
  if (!(warn_image = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), WARN_IMAGE_KEY))) return FALSE;
  lives_widget_set_no_show_all(warn_image, TRUE);
  lives_widget_hide(warn_image);
  if (is_standard_widget(widget)) {
    if (LIVES_IS_ENTRY(widget) || LIVES_IS_SPIN_BUTTON(widget) || LIVES_IS_COMBO(widget)) {
      if (prefs->extra_colours && mainw->pretty_colours) {
        char *colref = gdk_rgba_to_string(&palette->nice1);
        if (LIVES_IS_COMBO(widget))
          widget = lives_combo_get_entry(LIVES_COMBO(widget));
        set_css_value_direct(widget, LIVES_WIDGET_STATE_NORMAL, "", "border-color", colref);
        set_css_value_direct(widget, LIVES_WIDGET_STATE_FOCUSED, "", "border-color", colref);
        set_css_value_direct(widget, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "2px");
        set_css_value_direct(widget, LIVES_WIDGET_STATE_FOCUSED, "", "border-width", "2px");
        lives_free(colref);
      } else {
        set_css_value_direct(widget, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "0px");
        set_css_value_direct(widget, LIVES_WIDGET_STATE_FOCUSED, "", "border-width", "0px");
      }
    }
  }
  return TRUE;
}


static LiVESWidget *make_inner_hbox(LiVESBox * box, boolean start) {
  /// create an hbox, this gets packed into box
  /// "start" defines whether we pack at start or end
  /// if box is a vbox, "start" is ignored, and we pack a second hbox in the first
  /// so it is always hbox inside an hbox
  ///
  /// create a vbox, this is packed into hbox, this allows adding space on either side
  ///
  /// finally, create another hbox and pack into vbox, this will hold all sub widgets,
  /// e.g. labels, buttons, filler
  /// this allows for expanding the vertical size, whilst packing sub widgets horizontally

  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
  LiVESWidget *vbox = lives_vbox_new(FALSE, 0);

  if (widget_opts.apply_theme == 2) lives_widget_apply_theme2(hbox, LIVES_WIDGET_STATE_NORMAL, TRUE);
  else lives_widget_apply_theme(hbox, LIVES_WIDGET_STATE_NORMAL);
  if (LIVES_IS_HBOX(box)) {
    widget_opts.last_container = hbox;
    if (!widget_opts.pack_end)
      lives_box_pack_start(LIVES_BOX(box), hbox, LIVES_SHOULD_EXPAND_EXTRA_WIDTH,
                           LIVES_SHOULD_EXPAND_WIDTH, LIVES_SHOULD_EXPAND_FOR(box)
                           ? widget_opts.packing_width : 0);
    else
      lives_box_pack_end(LIVES_BOX(box), hbox, LIVES_SHOULD_EXPAND_EXTRA_WIDTH,
                         LIVES_SHOULD_EXPAND_WIDTH, LIVES_SHOULD_EXPAND_FOR(box)
                         ? widget_opts.packing_width : 0);
    if (widget_opts.justify == LIVES_JUSTIFY_END) lives_widget_set_halign(hbox, LIVES_ALIGN_END);
  } else {
    lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, LIVES_SHOULD_EXPAND_FOR(box)
                         ? widget_opts.packing_height : 0);
    //lives_widget_set_show_hide_parent(hbox);
    widget_opts.last_container = hbox;
    box = LIVES_BOX(hbox);
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, LIVES_SHOULD_EXPAND_FOR(box)
                         ? widget_opts.packing_width : 0);
  }

  lives_widget_set_valign(hbox, LIVES_ALIGN_CENTER);

  if (start)
    lives_box_pack_start(LIVES_BOX(hbox), vbox, LIVES_SHOULD_EXPAND_EXTRA_WIDTH,
                         LIVES_SHOULD_EXPAND_EXTRA_WIDTH, 0);
  else
    lives_box_pack_end(LIVES_BOX(hbox), vbox, LIVES_SHOULD_EXPAND_EXTRA_WIDTH,
                       LIVES_SHOULD_EXPAND_EXTRA_WIDTH, 0);

  //lives_widget_set_show_hide_parent(hbox);
  //lives_widget_set_show_hide_parent(vbox);

  hbox = lives_hbox_new(FALSE, 0);

  if (!LIVES_SHOULD_EXPAND_EXTRA_FOR(vbox)) lives_widget_set_valign(hbox, LIVES_ALIGN_CENTER);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, TRUE, FALSE, LIVES_SHOULD_EXPAND_FOR(vbox) ? widget_opts.packing_height / 2 : 0);
  lives_widget_set_show_hide_parent(hbox);
  return hbox;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_frozen(LiVESWidget * widget, boolean state, double opac) {
  // set insens. but w.out dimming
#if GTK_CHECK_VERSION(3, 16, 0)
  char *sopac;
  if (opac == 0.) {
    if (state) opac = .75;
    else opac = 0.5;
  }
  sopac = lives_strdup_printf("%f", opac);
  set_css_value_direct(widget, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", sopac);
  set_css_value_direct(widget, LIVES_WIDGET_STATE_INSENSITIVE, "*", "opacity", sopac);
  lives_free(sopac);
#endif
  return lives_widget_set_sensitive(widget, !state);
}


#ifdef USE_SPECIAL_BUTTONS

#define BT_PRE_WIDTH 4.0
#define BT_UNPRE_WIDTH 2.0

static void lives_painter_psurface_destroy_cb(livespointer data) {
  struct pbs_struct *pbs = (struct pbs_struct *)data;
  if (pbs) {
    pthread_mutex_t *mutex = NULL;
    LiVESWidget *w = pbs->widget;
    lives_painter_surface_t **pbsurf;
    if (w) {
      mutex = lives_widget_get_mutex(w);
      if (mutex) pthread_mutex_lock(mutex);
    }
    pbsurf = pbs->surfp;
    if (pbsurf) {
      lives_painter_surface_t *bsurf = *pbsurf;
      if (bsurf) {
        lives_painter_surface_destroy(bsurf);
        *pbsurf = NULL;
      }
    }
    if (w) {
      if (pbs->key) SET_VOIDP_DATA(w, pbs->key, NULL);
      if (mutex) pthread_mutex_unlock(mutex);
    }
    lives_free(pbs);
  }
}

static void lives_widget_object_set_data_psurface(LiVESWidgetObject * obj, const char *key, livespointer data) {
  struct pbs_struct *pbs;
  pthread_mutex_t *mutex = NULL;
  LiVESWidget *w = (LiVESWidget *)obj;
  if (w) {
    mutex = lives_widget_get_mutex(w);
    if (mutex) pthread_mutex_lock(mutex);
  }
  pbs = (struct pbs_struct *)GET_VOIDP_DATA(obj, key);
  if (!pbs) {
    pbs = (struct pbs_struct *)lives_calloc(1, sizeof(struct pbs_struct));
    lives_widget_object_set_data_full(obj, key, pbs,
                                      (LiVESDestroyNotify)lives_painter_psurface_destroy_cb);
  }
  pbs->surfp = (lives_painter_surface_t **)data;
  pbs->widget = w;
  pbs->key = key;
  if (mutex) pthread_mutex_unlock(mutex);
}


void render_standard_button(LiVESButton * sbutt) {
  LiVESWidget *widget = LIVES_WIDGET(sbutt);
  if (!is_standard_widget(widget)) return;
  else {
    LiVESWidget *da = lives_bin_get_child(LIVES_BIN(sbutt));
    struct pbs_struct *pbs = (struct pbs_struct *)GET_VOIDP_DATA(da, PBS_KEY);
    lives_painter_surface_t **pbsurf;
    if (!pbs) return;
    pbsurf = pbs->surfp;
    if (!pbsurf || !*pbsurf) return;
    else {
      lives_painter_t *cr;
      lives_painter_surface_t *bsurf;
      LingoLayout *layout =
        (LingoLayout *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(sbutt),
            SBUTT_LAYOUT_KEY);
      LiVESWidgetState state = lives_widget_get_state(LIVES_WIDGET(sbutt));
      lives_colRGBA64_t fg, bg, pab, pab2;
      LiVESWidget *toplevel = lives_widget_get_toplevel(widget);
      LiVESWidget *deflt = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(toplevel), DEFBUTTON_KEY);
      LiVESWidget *defover = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(toplevel), DEFOVERRIDE_KEY);
      LiVESPixbuf *pixbuf = NULL;
      double offs_x = 0., offs_y = 0;
      boolean fake_default = (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(sbutt), SBUTT_FAKEDEF_KEY)
                              ? TRUE : FALSE);
      boolean use_markup = GET_INT_DATA(sbutt, SBUTT_MARKUP_KEY);
      boolean prelit = (state & LIVES_WIDGET_STATE_PRELIGHT) == 0 ? FALSE : TRUE;
      boolean insens = (state & LIVES_WIDGET_STATE_INSENSITIVE) == 0 ? FALSE : TRUE;
      boolean focused = lives_widget_is_focus(widget);
      uint32_t acc;
      int themetype = 0;
      int width, height, minwidth, minheight;
      int lw = 0, lh = 0, x_pos, y_pos, w_, h_;
      int pbw = 0, pbh = 0;

      if (insens) prelit = focused = FALSE;

      pab2.red = pab2.green = pab2.blue = 0;
      pab2.alpha = 1.;

      if (palette) themetype = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(sbutt), THEME_KEY));
      if (themetype) {
        if (themetype == 1) {
          widget_color_to_lives_rgba(&bg, &palette->normal_back);
          widget_color_to_lives_rgba(&fg, &palette->normal_fore);
        } else {
          widget_color_to_lives_rgba(&bg, &palette->menu_and_bars);
          widget_color_to_lives_rgba(&fg, &palette->menu_and_bars_fore);
        }
        if (prefs->extra_colours && mainw->pretty_colours) {
          widget_color_to_lives_rgba(&pab, &palette->nice1);
          if (themetype == 2)
            widget_color_to_lives_rgba(&pab2, &palette->nice1);
          else
            widget_color_to_lives_rgba(&pab2, &palette->nice2);
        } else {
          widget_color_to_lives_rgba(&pab, &palette->menu_and_bars);
          widget_color_to_lives_rgba(&pab2, &palette->menu_and_bars);
        }
      }

      bsurf = *pbsurf;

      width = lives_widget_get_allocation_width(da);
      height = lives_widget_get_allocation_height(da);

      cr = lives_painter_create_from_surface(bsurf);

      if (themetype) {
        lives_painter_set_line_width(cr, BT_PRE_WIDTH);
        if (prelit) lives_painter_set_source_rgb_from_lives_rgba(cr, &pab);
        else lives_painter_set_source_rgb_from_lives_rgba(cr, &bg);
        lives_painter_lozenge(cr, 0, 0, width, height, BUTTON_RAD);
        lives_painter_stroke(cr);
        offs_x += BT_PRE_WIDTH;
        offs_y += BT_PRE_WIDTH;

        lives_painter_set_line_width(cr, BT_UNPRE_WIDTH);
        lives_painter_set_source_rgb_from_lives_rgba(cr, &pab);

        lives_painter_lozenge(cr, offs_x, offs_y, width - offs_x * 2., height - offs_y * 2., BUTTON_RAD);
        lives_painter_stroke(cr);
      } else {
        offs_x += BT_PRE_WIDTH;
        offs_y += BT_PRE_WIDTH;
      }

      offs_x += BT_UNPRE_WIDTH;
      offs_y += BT_UNPRE_WIDTH;

      /// account for rounding errors
      offs_x -= 2.;
      offs_y -= 2.;

      if (offs_x < 0.) offs_x = 0.;
      if (offs_y < 0.) offs_y = 0.;

      lives_painter_lozenge(cr, offs_x, offs_y, width - offs_x * 2., height - offs_y * 2., BUTTON_RAD);
      lives_painter_clip(cr);

      if (widget_opts.show_button_images
          || (pixbuf = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(sbutt), SBUTT_FORCEIMG_KEY))) {
        if (!pixbuf) pixbuf = (LiVESPixbuf *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(sbutt),
                                SBUTT_PIXBUF_KEY);
        if (pixbuf) {
          pbw = lives_pixbuf_get_width(pixbuf);
          pbh = lives_pixbuf_get_height(pixbuf);
        }
      }

      if (!layout) {
        const char *text = (const char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(sbutt),
                           SBUTT_TXT_KEY);
        if (text && *text) {
          LiVESWidget *topl;
          LingoContext *ctx = gtk_widget_get_pango_context(da);
          char *markup, *full_markup;
          layout = pango_layout_new(ctx);
          lingo_layout_set_alignment(layout, LINGO_ALIGN_CENTER);

          if (!use_markup) markup = lives_markup_escape_text(text, -1);
          else markup = (char *)text;
          full_markup = lives_strdup_printf("<span size=\"%s\">%s</span>", widget_opts.text_size,
                                            markup);

          lingo_layout_set_markup_with_accel(layout, full_markup, -1, '_', &acc);
          if (markup != text) lives_free(markup);
          lives_free(full_markup);
          if (acc) {
            if (LIVES_IS_FRAME(toplevel)) topl = lives_bin_get_child(LIVES_BIN(toplevel));
            else topl = toplevel;

            if (topl && LIVES_IS_WINDOW(topl)) {
              LiVESAccelGroup *accel_group =
                (LiVESAccelGroup *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(topl), BACCL_GROUP_KEY);
              if (!accel_group) {
                lives_widget_object_set_data(LIVES_WIDGET_OBJECT(topl), BACCL_GROUP_KEY, accel_group);
                accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
                lives_window_add_accel_group(LIVES_WINDOW(topl), accel_group);
              } else {
                uint32_t oaccl = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(topl), BACCL_ACCL_KEY));
                if (oaccl) lives_widget_remove_accelerator(widget,
                      accel_group, oaccl, (LiVESXModifierType)LIVES_ALT_MASK);
              }
              lives_widget_add_accelerator(widget, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                                           acc, (LiVESXModifierType)LIVES_ALT_MASK, (LiVESAccelFlags)0);
              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(topl), BACCL_ACCL_KEY, LIVES_INT_TO_POINTER(acc));
            }
          }
          lingo_layout_get_size(layout, &w_, &h_);

          // scale width, height to pixels
          lw = ((double)w_) / (double)LINGO_SCALE;
          lh = ((double)h_) / (double)LINGO_SCALE;

          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(da), SBUTT_LW_KEY,
                                       LIVES_INT_TO_POINTER(lw));
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(da), SBUTT_LH_KEY,
                                       LIVES_INT_TO_POINTER(lh));
          lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(widget), SBUTT_LAYOUT_KEY,
              (livespointer)layout);
        }
      } else {
        lw = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(da),
                                  SBUTT_LW_KEY));
        lh = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(da),
                                  SBUTT_LH_KEY));
      }

      if (focused || fake_default || ((LiVESWidget *)sbutt == deflt && !defover)) {
        if (!fake_default && deflt && (LiVESWidget *)sbutt != deflt) {
          // clear def bg
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(toplevel), DEFOVERRIDE_KEY, sbutt);
          render_standard_button(LIVES_BUTTON(deflt));
        }
        if (themetype) {
          lives_painter_set_source_rgb_from_lives_rgba(cr, &pab2);
          lives_painter_lozenge(cr, offs_x, offs_y, width, height, BUTTON_RAD);
          lives_painter_fill(cr);
        }
      } else {
        if ((LiVESWidget *)sbutt == defover) {
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(toplevel), DEFOVERRIDE_KEY, NULL);
          if (deflt) render_standard_button(LIVES_BUTTON(deflt));
        }
        if (themetype) {
          lives_painter_set_source_rgb_from_lives_rgba(cr, &bg);
          lives_painter_lozenge(cr, offs_x, offs_y, width, height, BUTTON_RAD);
          lives_painter_fill(cr);
        }
      }

      // top left of layout
      x_pos = (width - lw) >> 1;
      y_pos = (height - lh) >> 1;

      // if pixbuf, offset a little more
      if (pixbuf && lw && layout) {
        if (!widget_opts.swap_label)
          x_pos -= (pbw + widget_opts.packing_width) >> 1;
        else
          x_pos += (pbw + widget_opts.packing_width) >> 1;
      }

      if (lh && lw && layout) {
        layout_to_lives_painter(layout, cr, LIVES_TEXT_MODE_FOREGROUND_ONLY, &fg,
                                &bg, lw, lh, x_pos, y_pos, x_pos, y_pos);
        if (LINGO_IS_LAYOUT(layout))
          lingo_painter_show_layout(cr, layout);
      }

      if (pixbuf) {
        if (lw && layout) {
          // shift to get pixbuf pos
          if (!widget_opts.swap_label) {
            x_pos += (lw + widget_opts.packing_width);
            if (x_pos + pbw + widget_opts.packing_width + widget_opts.border_width < width)
              x_pos += widget_opts.packing_width;
          } else {
            x_pos -= (pbw + widget_opts.packing_width);
            if (x_pos > widget_opts.packing_width + widget_opts.border_width)
              x_pos -= widget_opts.packing_width;
          }
        } else x_pos -= pbw >> 1;
        y_pos = (height - pbh) >> 1;
        lives_painter_set_source_pixbuf(cr, pixbuf, x_pos, y_pos);
        lives_painter_rectangle(cr, 0, 0, pbw, pbh);
        lives_painter_paint(cr);
      }
      lives_painter_destroy(cr);
      if (LIVES_EXPAND_WIDTH(GET_INT_DATA(sbutt, EXPANSION_KEY))) {
        minwidth = lw + (pbw ? pbw + widget_opts.packing_width
                         : 0) + widget_opts.border_width * 4;
      } else minwidth = lw;
      if (LIVES_EXPAND_HEIGHT(GET_INT_DATA(sbutt, EXPANSION_KEY))) {
        minheight = lh + widget_opts.border_width * 2;
      } else minheight = lh;
      width = lives_widget_get_allocation_width(widget);
      height = lives_widget_get_allocation_height(widget);

      if (width < minwidth || height < minheight) {
        if (width < minwidth) width = minwidth;
        if (height < minheight) height = minheight;
        lives_widget_set_size_request(widget, width, height);
      }
      //if (is_fg_thread())
      lives_widget_queue_draw_and_update(widget);
    }
  }
}


static void sbutt_render(LiVESButton * sbutt, LiVESWidgetState state, livespointer user_data) {
  render_standard_button(sbutt);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_standard_button_set_image(LiVESButton * sbutt,
    LiVESWidget * img, boolean force_show) {
  LiVESWidget *widget = LIVES_WIDGET(sbutt);
  if (!is_standard_widget(widget)) return lives_button_set_image(sbutt, img);

  if (img) {
    LiVESPixbuf *pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(img));
    if (LIVES_IS_PIXBUF(pixbuf)) lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(sbutt),
          SBUTT_PIXBUF_KEY, (livespointer)pixbuf);
    if (force_show)
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(sbutt), SBUTT_FORCEIMG_KEY, pixbuf);
    else
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(sbutt), SBUTT_FORCEIMG_KEY, NULL);
  } else {
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(sbutt), SBUTT_PIXBUF_KEY, NULL);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(sbutt), SBUTT_FORCEIMG_KEY, NULL);
  }
  render_standard_button(sbutt);
  return TRUE;
}


LiVESWidget *lives_standard_button_new(int width, int height) {
  lives_painter_surface_t **pbsurf =
    (lives_painter_surface_t **)lives_calloc(1, sizeof(lives_painter_surface_t *));
  LiVESWidget *button, *da;

  button = lives_button_new();

  if (!palette) return button;

  da = lives_standard_drawing_area_new(LIVES_GUI_CALLBACK(all_expose), pbsurf);

  lives_container_add(LIVES_CONTAINER(button), da);
  lives_widget_set_show_hide_with(button, da);

  SET_INT_DATA(button, EXPANSION_KEY, widget_opts.expand);

  if (widget_opts.apply_theme) {
    set_standard_widget(button, TRUE);

    if (palette->style & STYLE_LIGHT)
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), THEME_KEY,
                                   LIVES_INT_TO_POINTER(2));
    else
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), THEME_KEY,
                                   LIVES_INT_TO_POINTER(widget_opts.apply_theme));

#if GTK_CHECK_VERSION(3, 16, 0)
    set_css_min_size(da, width, height);
    set_css_value_direct(da, LIVES_WIDGET_STATE_NORMAL, "", "border-radius", "5px");
    set_css_value_direct(button, LIVES_WIDGET_STATE_NORMAL, "", "border-radius", "5px");
    set_css_value_direct(da, LIVES_WIDGET_STATE_PRELIGHT, "", "border-radius", "5px");
    set_css_value_direct(button, LIVES_WIDGET_STATE_PRELIGHT, "", "border-radius", "5px");
    set_css_value_direct(button, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");

    lives_widget_set_padding(button, 0);
    set_css_value_direct(button, LIVES_WIDGET_STATE_NORMAL, "", "background", "none");
    set_css_value_direct(button, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "0px");
#endif
  }

  lives_widget_set_can_focus_and_default(button);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_STATE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(sbutt_render), NULL);
#ifdef USE_SPECIAL_BUTTONS
  lives_widget_apply_theme(button, LIVES_WIDGET_STATE_NORMAL);
#endif
  lives_widget_set_app_paintable(button, TRUE);
  lives_widget_set_app_paintable(da, TRUE);
  lives_widget_set_size_request(da, width, height);

  render_standard_button(LIVES_BUTTON(button));
  return button;
}

WIDGET_HELPER_LOCAL_INLINE boolean _lives_standard_button_set_label(LiVESButton * sbutt,
    const char *txt) {
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(sbutt), SBUTT_LAYOUT_KEY, NULL);
  SET_INT_DATA(sbutt, SBUTT_MARKUP_KEY, widget_opts.use_markup);
  lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(sbutt), SBUTT_TXT_KEY,
                                    txt ? lives_strdup(txt) : NULL);
  render_standard_button(sbutt);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_standard_button_set_label(LiVESButton * sbutt,
    const char *txt) {
  boolean ret;
  LiVESWidget *widget = LIVES_WIDGET(sbutt);
  if (!is_standard_widget(widget)) return lives_button_set_label(sbutt, txt);
  ret = _lives_standard_button_set_label(sbutt, txt);
  return ret;
}

WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_button_new_with_label(const char *txt,
    int width, int height) {
  LiVESWidget *sbutt = lives_standard_button_new(width, height);
  _lives_standard_button_set_label(LIVES_BUTTON(sbutt), txt);
  return sbutt;
}

WIDGET_HELPER_GLOBAL_INLINE const char *lives_standard_button_get_label(LiVESButton * sbutt) {
  LiVESWidget *widget = LIVES_WIDGET(sbutt);
  if (!is_standard_widget(widget)) return lives_button_get_label(sbutt);
  return lives_widget_object_get_data(LIVES_WIDGET_OBJECT(sbutt), SBUTT_TXT_KEY);
}
#endif



static LiVESWidget *_lives_standard_button_set_full(LiVESWidget * sbutt, LiVESBox * box,
    boolean fake_default, const char *ttips) {
  LiVESWidget *img_tips = NULL, *hbox;

  if (ttips) img_tips = lives_widget_set_tooltip_text(sbutt, ttips);

  lives_button_set_focus_on_click(LIVES_BUTTON(sbutt), FALSE);

  if (box) {
    LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box), WH_LAYOUT_KEY);
    int packing_width = 0;
    boolean expand;

    if (layout) {
      box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
      hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
      lives_widget_set_show_hide_with(sbutt, hbox);
    } else hbox = make_inner_hbox(LIVES_BOX(box), TRUE);

    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);
    if (expand) add_fill_to_box(LIVES_BOX(hbox));
    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    lives_widget_set_hexpand(sbutt, FALSE);
    lives_widget_set_valign(sbutt, LIVES_ALIGN_CENTER);

    lives_box_pack_start(LIVES_BOX(hbox), sbutt, LIVES_SHOULD_EXPAND_WIDTH,
                         expand, packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));
    lives_widget_set_show_hide_parent(sbutt);

    add_warn_image(sbutt, hbox);
    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
    }
  } else {
    if (img_tips) break_me("floating img tips for button !");
  }

#ifdef USE_SPECIAL_BUTTONS
  if (is_standard_widget(sbutt)) {
    if (fake_default) {
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(LIVES_WIDGET_OBJECT(sbutt)),
                                   SBUTT_FAKEDEF_KEY, sbutt);
    }
#endif
  }
  return sbutt;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_button_new_full(const char *label, int width,
    int height, LiVESBox * box, boolean fake_default,
    const char *ttips) {
  LiVESWidget *sbutt = lives_standard_button_new_with_label(label, width, height);
  return _lives_standard_button_set_full(sbutt, box, fake_default, ttips);
}


LiVESWidget *lives_standard_button_new_from_stock_full(const char *stock_id, const char *label,
    int width, int height,  LiVESBox * box, boolean fake_default, const char *ttips) {
  LiVESWidget *sbutt = lives_standard_button_new_from_stock(stock_id, label, width, height);
  return _lives_standard_button_set_full(sbutt, box, fake_default, ttips);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_header_bar_set_title(LiVESHeaderBar * hdrbar, const char *title) {
#ifdef GUI_GTK
#if LIVES_HAS_HEADER_BAR_WIDGET
  gtk_header_bar_set_title(hdrbar, title);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_header_bar_new(LiVESWindow * toplevel) {
  LiVESWidget *hdrbar = NULL;
#ifdef GUI_GTK
#if LIVES_HAS_HEADER_BAR_WIDGET
  hdrbar = gtk_header_bar_new();
  gtk_window_set_titlebar(toplevel, hdrbar);
  gtk_header_bar_set_has_subtitle(LIVES_HEADER_BAR(hdrbar), FALSE);
  gtk_header_bar_set_show_close_button(LIVES_HEADER_BAR(hdrbar), TRUE);
  if (widget_opts.apply_theme) {
    set_css_value_direct(hdrbar, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "0");
    set_css_min_size(hdrbar, widget_opts.css_min_width, widget_opts.css_min_height >> 1);
    lives_widget_apply_theme2(hdrbar, LIVES_WIDGET_STATE_NORMAL, TRUE);
  }
#endif
#endif
  return hdrbar;
}


WIDGET_HELPER_GLOBAL_INLINE
boolean lives_header_bar_pack_start(LiVESHeaderBar * hdrbar, LiVESWidget * w) {
#ifdef GUI_GTK
#if LIVES_HAS_HEADER_BAR_WIDGET
  gtk_header_bar_pack_start(GTK_HEADER_BAR(hdrbar), w);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_hpaned_new(void) {
  LiVESWidget *hpaned;
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
  hpaned = lives_hpaned_new();
#else
  hpaned = gtk_paned_new(LIVES_ORIENTATION_HORIZONTAL);
  gtk_paned_set_wide_handle(GTK_PANED(hpaned), TRUE);
  if (widget_opts.apply_theme) {
    set_standard_widget(hpaned, TRUE);
#if GTK_CHECK_VERSION(3, 16, 0)
    if (prefs->extra_colours && mainw->pretty_colours) {
      char *colref = gdk_rgba_to_string(&palette->nice1);
      // clear background image
      char *tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(hpaned, LIVES_WIDGET_STATE_NORMAL, "separator",
                           "background-image", tmp);
      lives_free(tmp);
      lives_free(colref);
    }
#endif
  }
#endif
#endif
  return hpaned;
}

WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_vpaned_new(void) {
  LiVESWidget *vpaned;
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
  vpaned = lives_vpaned_new();
#else
  vpaned = gtk_paned_new(LIVES_ORIENTATION_VERTICAL);
  gtk_paned_set_wide_handle(GTK_PANED(vpaned), TRUE);
  if (widget_opts.apply_theme) {
    set_standard_widget(vpaned, TRUE);
#if GTK_CHECK_VERSION(3, 16, 0)
    if (prefs->extra_colours && mainw->pretty_colours) {
      char *colref = gdk_rgba_to_string(&palette->nice1);
      // clear background image
      char *tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(vpaned, LIVES_WIDGET_STATE_NORMAL, "separator",
                           "background-image", tmp);
      lives_free(tmp);
      lives_free(colref);
    }
#endif
  }
#endif
#endif
  return vpaned;
}


LiVESWidget *lives_standard_menu_new(void) {
  LiVESWidget *menu = lives_menu_new();
  if (menu) {
    if (widget_opts.apply_theme) {
      set_standard_widget(menu, TRUE);
      lives_widget_apply_theme2(menu, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if !GTK_CHECK_VERSION(3, 16, 0)
      lives_widget_apply_theme_dimmed2(menu, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
      set_child_dimmed_colour2(menu, BUTTON_DIM_VAL);
#else
      set_css_value_direct(menu, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    }
  }
  return menu;
}


LiVESWidget *lives_standard_menu_item_new(void) {
  LiVESWidget *menuitem = lives_menu_item_new();
  if (menuitem) {
    if (widget_opts.apply_theme) {
      set_standard_widget(menuitem, TRUE);
      lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if !GTK_CHECK_VERSION(3, 16, 0)
      lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
      set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
#else
      set_css_value_direct(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    }
  }
  return menuitem;
}


LiVESWidget *lives_standard_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = lives_menu_item_new_with_label(label);
  if (menuitem) {
    if (widget_opts.apply_theme) {
      set_standard_widget(menuitem, TRUE);
      lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if !GTK_CHECK_VERSION(3, 16, 0)
      lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
      set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
#else
      set_css_value_direct(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    }
  }
  return menuitem;
}


LiVESWidget *lives_standard_image_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem;
  if (!prefs->show_menu_images) return lives_standard_menu_item_new_with_label(label);
  menuitem = lives_image_menu_item_new_with_label(label);
  if (menuitem) {
    if (widget_opts.apply_theme) {
      set_standard_widget(menuitem, TRUE);
      lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if !GTK_CHECK_VERSION(3, 16, 0)
      lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
      set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
#else
      set_css_value_direct(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    }
  }
  return menuitem;
}



LiVESWidget *lives_standard_image_menu_item_new_from_stock(const char *stock_id, LiVESAccelGroup * accel_group) {
  LiVESWidget *menuitem = lives_image_menu_item_new_from_stock(stock_id, accel_group);
  if (menuitem) {
    if (widget_opts.apply_theme) {
      set_standard_widget(menuitem, TRUE);
      lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if !GTK_CHECK_VERSION(3, 16, 0)
      lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
      set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
#else
      set_css_value_direct(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    }
  }
  return menuitem;
}


LiVESWidget *lives_standard_radio_menu_item_new_with_label(LiVESSList * group, const char *label) {
  LiVESWidget *menuitem = lives_radio_menu_item_new_with_label(group, label);
  if (menuitem) {
    if (widget_opts.apply_theme) {
      set_standard_widget(menuitem, TRUE);
      lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if !GTK_CHECK_VERSION(3, 16, 0)
      lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
      set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
#else
      set_css_value_direct(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    }
  }
  return menuitem;
}


LiVESWidget *lives_standard_check_menu_item_new_with_label(const char *label, boolean active) {
  LiVESWidget *menuitem = lives_check_menu_item_new_with_label(label);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(menuitem), active);
  if (menuitem) {
    if (widget_opts.apply_theme) {
      set_standard_widget(menuitem, TRUE);
      lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if !GTK_CHECK_VERSION(3, 16, 0)
      lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
      set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
#else
      set_css_value_direct(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    }
  }
  return menuitem;
}

static void togglevar_cb(LiVESWidget * w, boolean * var) {if (var) *var = !(*var);}

LiVESWidget *
lives_standard_check_menu_item_new_for_var(const char *labeltext, boolean * var, boolean invert) {
  LiVESWidget *mi = lives_standard_check_menu_item_new_with_label(labeltext, TRUE);
  if (invert) lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mi), !(*var));
  else lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mi), *var);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mi), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(togglevar_cb),
                                  (livespointer)var);
  return mi;
}


LiVESWidget *lives_standard_notebook_new(const LiVESWidgetColor * bg_color, const LiVESWidgetColor * act_color) {
  LiVESWidget *notebook = lives_notebook_new();

#ifdef GUI_GTK
  gtk_notebook_set_show_border(LIVES_NOTEBOOK(notebook), FALSE);

#if GTK_CHECK_VERSION(3, 16, 0)
  if (widget_opts.apply_theme) {
    char *colref = gdk_rgba_to_string(bg_color);
    set_standard_widget(notebook, TRUE);
    // clear background image
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_NORMAL, "*", "background", "none");
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_NORMAL, "*", "background-color", colref);
    lives_free(colref);
    colref = gdk_rgba_to_string(act_color);
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_ACTIVE, "*", "background", "none");
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_ACTIVE, "*", "background-color", colref);
    lives_free(colref);
  }
#endif
#endif
  lives_widget_set_hexpand(notebook, TRUE);
  return notebook;
}


LiVESWidget *lives_standard_label_new(const char *text) {
  LiVESWidget *label = NULL;
  label = lives_label_new(NULL);
  // allows markup

  if (text) lives_label_set_text(LIVES_LABEL(label), text);
  if (strcmp(widget_opts.text_size, LIVES_TEXT_SIZE_NORMAL))
    lives_widget_set_text_size(label, LIVES_WIDGET_STATE_NORMAL, widget_opts.text_size);
  lives_widget_set_halign(label, lives_justify_to_align(widget_opts.justify));
  if (widget_opts.apply_theme) {
    set_standard_widget(label, TRUE);
#if !GTK_CHECK_VERSION(3, 24, 0)
    // non functional in gtk 3.18
    set_child_dimmed_colour(label, BUTTON_DIM_VAL);
    set_child_colour(label, TRUE);
#else
    set_css_value_direct(label, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif

    if (widget_opts.apply_theme == 2) lives_widget_apply_theme2(label, LIVES_WIDGET_STATE_NORMAL, TRUE);
    else lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);

#if !GTK_CHECK_VERSION(3, 16, 0)
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(label), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb),
                                    NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(label), NULL, NULL);
#endif
  }
  return label;
}


LiVESWidget *lives_standard_label_new_with_tooltips(const char *text, LiVESBox * box,
    const char *tips) {
  LiVESWidget *label = lives_standard_label_new(text);
  LiVESWidget *img_tips = make_ttips_image_for(label, tips);
  LiVESWidget *hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  if (img_tips) {
    add_warn_image(label, hbox);
    lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
  }
  lives_widget_set_show_hide_parent(label);
  return label;
}


char *lives_big_and_bold(const char *fmt, ...) {
  va_list xargs;
  char *text, *text2, *text3;
  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  text2 = lives_markup_escape_text(text, -1);
  lives_free(text);
  text3 = lives_strdup_printf("<big><b>%s</b></big>", text2);
  lives_free(text2);
  return text3;
}


LiVESWidget *lives_standard_formatted_label_new(const char *text) {
  LiVESWidget *label;
  char *form_text;
  int woml = widget_opts.mnemonic_label;
  form_text = lives_strdup_printf("\n%s\n", text);

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  widget_opts.mnemonic_label = FALSE;
  label = lives_standard_label_new(NULL);
  if (widget_opts.use_markup)
    lives_label_set_markup(LIVES_LABEL(label), form_text);
  else
    lives_label_set_text(LIVES_LABEL(label), form_text);
  widget_opts.mnemonic_label = woml;
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  if (lives_strlen(text) < MIN_MSG_WIDTH_CHARS) {
    lives_label_set_width_chars(LIVES_LABEL(label), MIN_MSG_WIDTH_CHARS);
  }

  lives_free(form_text);
  return label;
}


WIDGET_HELPER_GLOBAL_INLINE void lives_label_chomp(LiVESLabel * label) {
  char *txt = lives_strdup(lives_label_get_text(label));
  lives_chomp(txt, TRUE);
  lives_label_set_text(label, txt);
  lives_free(txt);
}


WIDGET_HELPER_LOCAL_INLINE pthread_mutex_t *lives_widget_add_mutex(LiVESWidget * w) {
  pthread_mutex_t *mutex = GET_VOIDP_DATA(w, MUTEX_KEY);
  if (mutex) return mutex;
  mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(mutex, NULL);
  lives_widget_object_set_data_mutex((LiVESWidgetObject *)w, MUTEX_KEY, mutex);
  return mutex;
}


WIDGET_HELPER_GLOBAL_INLINE pthread_mutex_t *lives_widget_get_mutex(LiVESWidget * w) {
  pthread_mutex_t *mutex = GET_VOIDP_DATA(w, MUTEX_KEY);
  if (mutex) return mutex;
  return lives_widget_add_mutex(w);
}


LiVESWidget *lives_standard_drawing_area_new(LiVESGuiCallback callback, lives_painter_surface_t **ppsurf) {
  LiVESWidget *darea = NULL;
#ifdef GUI_GTK
  darea = gtk_drawing_area_new();
  lives_widget_set_app_paintable(darea, TRUE);

  lives_widget_add_mutex(darea);
  lives_widget_object_set_data_psurface(LIVES_WIDGET_OBJECT(darea), PBS_KEY, ppsurf);

  if (ppsurf) {
    if (callback)
#if GTK_CHECK_VERSION(4, 0, 0)
      gtk_drawing_area_set_draw_func(darea, callback, (livespointer)ppsurf, NULL);
#else
      lives_signal_connect(LIVES_GUI_OBJECT(darea), LIVES_WIDGET_EXPOSE_EVENT,
                           LIVES_GUI_CALLBACK(callback),
                           (livespointer)ppsurf);
#endif
  }
  lives_signal_sync_connect(LIVES_GUI_OBJECT(darea), LIVES_WIDGET_CONFIGURE_EVENT,
                            LIVES_GUI_CALLBACK(all_config_deferrable),
                            (livespointer)ppsurf);

  if (widget_opts.apply_theme) {
    set_standard_widget(darea, TRUE);
    lives_widget_apply_theme(darea, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(darea, LIVES_WIDGET_STATE_BACKDROP);
  }
#endif
  return darea;
}


LiVESWidget *lives_standard_label_new_with_mnemonic_widget(const char *text, LiVESWidget * mnemonic_widget) {
  LiVESWidget *label = NULL;

  label = lives_standard_label_new("");
  lives_label_set_text(LIVES_LABEL(label), text);

  if (mnemonic_widget) lives_label_set_mnemonic_widget(LIVES_LABEL(label), mnemonic_widget);

  return label;
}


LiVESWidget *lives_standard_frame_new(const char *labeltext, float xalign, boolean invis) {
  LiVESWidget *frame = lives_frame_new(NULL);
  LiVESWidget *label = NULL;

  if (LIVES_SHOULD_EXPAND)
    lives_container_set_border_width(LIVES_CONTAINER(frame), widget_opts.border_width);

  if (labeltext) {
    label = lives_standard_label_new(labeltext);
    lives_frame_set_label_widget(LIVES_FRAME(frame), label);
  }

  widget_opts.last_label = label;

  if (invis) lives_frame_set_shadow_type(LIVES_FRAME(frame), LIVES_SHADOW_NONE);

  if (widget_opts.apply_theme) {
    // WARNING - special css case - default selector only applies to label !
    set_standard_widget(frame, TRUE);
    lives_widget_apply_theme(frame, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_set_text_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

#if !GTK_CHECK_VERSION(3, 24, 0)
    if (prefs->extra_colours && mainw->pretty_colours)
      lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->nice1);
    else
      lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
#else
    if (prefs->extra_colours && mainw->pretty_colours) {
      char *colref = gdk_rgba_to_string(&palette->nice1);
      set_css_value_direct(frame, LIVES_WIDGET_STATE_NORMAL, "border",
                           "border-color", colref);
      lives_free(colref);
    } else {
      char *colref = gdk_rgba_to_string(&palette->menu_and_bars);
      set_css_value_direct(frame, LIVES_WIDGET_STATE_NORMAL, "border",
                           "border-color", colref);
      lives_free(colref);
    }
#endif
  }
  if (xalign >= 0.) lives_frame_set_label_align(LIVES_FRAME(frame), xalign, 0.5);

  return frame;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAlign lives_justify_to_align(LiVESJustification justify) {
  if (justify == LIVES_JUSTIFY_DEFAULT) return LIVES_ALIGN_START;
  if (justify == LIVES_JUSTIFY_CENTER) return LIVES_ALIGN_CENTER;
  else return LIVES_ALIGN_END;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESScrollDirection lives_get_scroll_direction(LiVESXEventScroll * event) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  double dx, dy;
  if (gdk_event_get_scroll_deltas(LIVES_XEVENT(event), &dx, &dy)) {
    if (dy < 0.) return LIVES_SCROLL_UP;
    if (dy > 0.) return LIVES_SCROLL_DOWN;
  }
#endif
#if GTK_CHECK_VERSION(3, 2, 0)
  LiVESScrollDirection direction;
  gdk_event_get_scroll_direction(LIVES_XEVENT(event), &direction);
  return direction;
#else
  return event->direction;
#endif
#endif
  return LIVES_SCROLL_UP;
}


static LiVESWidget *_make_label_eventbox(const char *labeltext, LiVESWidget * widget, boolean add_sens) {
  LiVESWidget *label;
  LiVESWidget *eventbox = lives_event_box_new();
  if (widget) lives_tooltips_copy(eventbox, widget);
  if (widget && widget_opts.mnemonic_label && labeltext) {
    label = lives_standard_label_new_with_mnemonic_widget(labeltext, widget);
  } else label = lives_standard_label_new(labeltext);

  widget_opts.last_label = label;
  lives_container_add(LIVES_CONTAINER(eventbox), label);
  lives_widget_set_halign(label, lives_justify_to_align(widget_opts.justify));

  if (widget && (LIVES_IS_TOGGLE_BUTTON(widget) || LIVES_IS_TOGGLE_TOOL_BUTTON(widget))) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                              LIVES_GUI_CALLBACK(label_act_toggle), widget);
  }
  if (add_sens) {
    lives_widget_set_sensitive_with(widget, eventbox);
    lives_widget_set_sensitive_with(eventbox, label);
  }
  lives_widget_set_show_hide_with(widget, eventbox);
  lives_widget_set_show_hide_with(eventbox, label);
  if (widget_opts.apply_theme) {
    // default themeing
    lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_INSENSITIVE);

#if !GTK_CHECK_VERSION(3, 16, 0)
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(label), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb), NULL);
#endif
  }
  return eventbox;
}

WIDGET_HELPER_LOCAL_INLINE LiVESWidget *make_label_eventbox(const char *labeltext, LiVESWidget * widget) {
  return _make_label_eventbox(labeltext, widget, TRUE);
}


static void sens_insens_cb(LiVESWidgetObject * object, livespointer pspec, livespointer user_data) {
  LiVESWidget *widget = (LiVESWidget *)object;
  LiVESWidget *other = (LiVESWidget *)user_data;
  boolean sensitive = lives_widget_get_sensitive(widget);
  if (lives_widget_get_sensitive(other) != sensitive) {
    lives_widget_set_sensitive(other, sensitive);
  }
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_sensitive_with(LiVESWidget * w1, LiVESWidget * w2) {
  // set w2 sensitivity == w1
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(w1), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                  LIVES_GUI_CALLBACK(sens_insens_cb),
                                  (livespointer)w2);
  return TRUE;
}


static void lives_widget_show_all_cb(LiVESWidget * other, livespointer user_data) {
  LiVESWidget *controller;

  if (LIVES_IS_WIDGET_OBJECT(other)) {
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(other), SHOWALL_OVERRIDE_KEY)) return;
    controller = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(other),
                 SHOWHIDE_CONTROLLER_KEY);
    if (controller) {
      if (lives_widget_get_no_show_all(controller)) {
        lives_widget_set_no_show_all(other, TRUE);
        lives_widget_hide(other);
        return;
      }
    }
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(other), TTIPS_HIDE_KEY)) {
      if (prefs->show_tooltips) {
        lives_widget_set_no_show_all(other, FALSE);
        lives_widget_show(other);
      }
      return;
    }

    if (controller && !lives_widget_get_no_show_all(controller)) {
      if (lives_widget_get_no_show_all(other)) {
        lives_widget_set_no_show_all(other, FALSE);
      }
    }
    if (!lives_widget_is_visible(other)) {
      _lives_widget_show_all(other);
    }
  }
}


boolean lives_widget_set_show_hide_with(LiVESWidget * widget, LiVESWidget * other) {
  // show / hide the other widget when and only when the child is shown / hidden
  if (!widget || !other) return FALSE;

  // need to remove this key when widget is destroyed
  // however we need to avoid doing so in the case where other is destryoed first
  // thus we use lives_widget_object_set_data_destroyable()
  lives_widget_object_set_data_destroyable(LIVES_WIDGET_OBJECT(other),
      SHOWHIDE_CONTROLLER_KEY, (LiVESWidgetObject *)widget);

  if (!widget_opts.no_gui) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(other), LIVES_WIDGET_SHOW_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_widget_show_all_cb),
                              (livespointer)(widget));

    lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_SHOW_SIGNAL,
                                      LIVES_GUI_CALLBACK(lives_widget_show_all_cb),
                                      (livespointer)(other));

    lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_HIDE_SIGNAL,
                                      LIVES_GUI_CALLBACK(lives_widget_hide),
                                      (livespointer)(other));
  }
  return TRUE;
}


boolean lives_widget_unset_show_hide_with(LiVESWidget * widget, LiVESWidget * other) {
  // show / hide the other widget when and only when the child is shown / hidden
  if (!widget || !other) return FALSE;
  lives_signal_handlers_sync_disconnect_by_func
  (widget, LIVES_GUI_CALLBACK(lives_widget_show_all_cb), (livespointer)other);
  lives_signal_handlers_sync_disconnect_by_func
  (other, LIVES_GUI_CALLBACK(lives_widget_show_all_cb), (livespointer)widget);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(other),
                               SHOWHIDE_CONTROLLER_KEY, NULL);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_show_hide_parent(LiVESWidget * widget) {
  LiVESWidget *parent = lives_widget_get_parent(widget);
  if (parent) return lives_widget_set_show_hide_with(widget, parent);
  return FALSE;
}


LiVESWidget *lives_standard_switch_new(const char *labeltext, boolean active, LiVESBox * box,
                                       const char *tooltip) {
  LiVESWidget *swtch = NULL;
#if !LIVES_HAS_SWITCH_WIDGET
  return lives_standard_check_button_new(labeltext, active, box, tooltip);
#else
  LiVESWidget *eventbox = NULL;
  LiVESWidget *container = NULL;
  LiVESWidget *hbox;
  LiVESWidget *img_tips = NULL;

#if GTK_CHECK_VERSION(3, 14, 0)
  char *colref;
#endif
  //char *tmp;

  boolean expand;

  swtch = lives_switch_new();
  lives_widget_set_halign(swtch, LIVES_ALIGN_CENTER);

  widget_opts.last_label = NULL;

  if (tooltip) img_tips = lives_widget_set_tooltip_text(swtch, tooltip);

  if (box) {
    int packing_width = 0;

    if (labeltext) {
      eventbox = make_label_eventbox(labeltext, swtch);
      lives_widget_set_show_hide_with(swtch, eventbox);
    }

    hbox = make_inner_hbox(LIVES_BOX(box), !widget_opts.swap_label);
    container = widget_opts.last_container;

    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);
    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (widget_opts.swap_label && eventbox)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start(LIVES_BOX(hbox), swtch, expand, expand,
                         !eventbox ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));
    lives_widget_set_show_hide_parent(swtch);

    if (!widget_opts.swap_label && eventbox)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    add_warn_image(swtch, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
    }
  } else {
    if (img_tips) break_me("floating img tips for switch !");
  }

  if (widget_opts.apply_theme) {
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
    char *tmp;
#endif
    set_standard_widget(swtch, TRUE);
    lives_widget_apply_theme(swtch, LIVES_WIDGET_STATE_NORMAL);
    set_css_min_size(swtch, widget_opts.css_min_width, widget_opts.css_min_height);

#if GTK_CHECK_VERSION(3, 16, 0)
    if (prefs->extra_colours && mainw->pretty_colours) {
      lives_widget_set_border_color(swtch, LIVES_WIDGET_STATE_NORMAL, &palette->nice1);

      colref = gdk_rgba_to_string(&palette->menu_and_bars);
      tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(swtch, LIVES_WIDGET_STATE_NORMAL, "slider",
                           "background-image", tmp);
      lives_free(tmp);
      lives_free(colref);

      colref = gdk_rgba_to_string(&palette->nice2);
      tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(swtch, LIVES_WIDGET_STATE_INSENSITIVE, "slider",
                           "background-image", tmp);
      lives_free(tmp);
      lives_free(colref);

      lives_widget_set_border_color(swtch, LIVES_WIDGET_STATE_INSENSITIVE, &palette->nice2);
    }
    colref = gdk_rgba_to_string(&palette->normal_fore);
    tmp = lives_strdup_printf("image(%s)", colref);
    set_css_value_direct(swtch, LIVES_WIDGET_STATE_CHECKED, "slider",
                         "background-image", tmp);
    lives_free(tmp);
    lives_free(colref);
#endif
#endif
  }
  lives_switch_set_active(LIVES_SWITCH(swtch), active);
  widget_opts.last_container = container;
#endif
  return swtch;
}


LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean active, LiVESBox * box,
    const char *tooltip) {
  LiVESWidget *checkbutton = NULL;
  LiVESWidget *eventbox = NULL;
  LiVESWidget *hbox;
  LiVESWidget *container = NULL;
  LiVESWidget *img_tips = NULL;

#if GTK_CHECK_VERSION(3, 14, 0)
  char *colref;
#endif
  char *tmp;

  boolean expand;

#if LIVES_HAS_SWITCH_WIDGET
  if (prefs->cb_is_switch) return lives_standard_switch_new(labeltext, active, box, tooltip);
  else
#endif
    checkbutton = lives_check_button_new();

  lives_widget_set_halign(checkbutton, LIVES_ALIGN_CENTER);

  widget_opts.last_label = NULL;

  if (tooltip) img_tips = lives_widget_set_tooltip_text(checkbutton, tooltip);

  if (box) {
    LiVESWidget *layout;
    int packing_width = 0;

    if (labeltext) {
      eventbox = make_label_eventbox(labeltext, checkbutton);
      lives_widget_set_show_hide_with(checkbutton, eventbox);
    }

    hbox = make_inner_hbox(LIVES_BOX(box), !widget_opts.swap_label);
    container = widget_opts.last_container;

    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);
    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (widget_opts.swap_label && eventbox)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box),
             WH_LAYOUT_KEY);

    if (LIVES_SHOULD_EXPAND_WIDTH) {
      if (layout) {
        int nadded = GET_INT_DATA(layout, WADDED_KEY);
        if (nadded > 0) lives_widget_set_margin_left(checkbutton, widget_opts.packing_width);
      }
    }
    lives_box_pack_start(LIVES_BOX(hbox), checkbutton, expand, expand,
                         !eventbox ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));
    lives_widget_set_show_hide_parent(checkbutton);

    if (!widget_opts.swap_label && eventbox)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    add_warn_image(checkbutton, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
    }
  } else {
    if (img_tips) break_me("floating img tips for chkbutton !");
  }

  if (widget_opts.apply_theme) {
    set_standard_widget(checkbutton, TRUE);
    lives_widget_apply_theme(checkbutton, LIVES_WIDGET_STATE_NORMAL);
#if GTK_CHECK_VERSION(3, 0, 0)
    set_css_min_size(checkbutton, widget_opts.css_min_width, widget_opts.css_min_height);
#if GTK_CHECK_VERSION(3, 16, 0)
    if (prefs->extra_colours && mainw->pretty_colours) {
      if (!(palette->style & STYLE_LIGHT))
        colref = gdk_rgba_to_string(&palette->nice2);
      else
        colref = gdk_rgba_to_string(&palette->menu_and_bars);

      tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_NORMAL, "check",
                           "background-image", tmp);
      set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_CHECKED, "check",
                           "background-image", tmp);
      set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_CHECKED, "check",
                           "border-image", tmp);
      lives_free(colref); lives_free(tmp);

      colref = gdk_rgba_to_string(&palette->normal_fore);
      set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_CHECKED, "check",
                           "color", colref);
      lives_free(colref);

      /* colref = gdk_rgba_to_string(&palette->nice2); */
      /* tmp = lives_strdup_printf("image(%s)", colref); */
      /* set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_INSENSITIVE, "check", */
      /*                      "background-image", tmp); */
      /* set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_INSENSITIVE, "check", */
      /*                      "border-image", tmp); */
      //lives_free(tmp);
    } else {
      colref = gdk_rgba_to_string(&palette->normal_fore);
      set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_CHECKED, "check",
                           "color", colref);
      lives_free(colref);

      colref = gdk_rgba_to_string(&palette->normal_back);
      set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_CHECKED, "check",
                           "background-color", colref);
      lives_free(colref);
    }
    set_css_value_direct(checkbutton, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
#endif
  }

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), active);
  widget_opts.last_container = container;
  return checkbutton;
}


LiVESWidget *lives_glowing_check_button_new(const char *labeltext, LiVESBox * box, const char *tooltip, boolean * togglevalue) {
  boolean active = FALSE;
  LiVESWidget *checkbutton;
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref;
#endif
  if (togglevalue) active = *togglevalue;

  checkbutton = lives_standard_check_button_new(labeltext, active, box, tooltip);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(lives_cool_toggled),
                                  togglevalue);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget_opts.last_label),
                               THEME_KEY, LIVES_INT_TO_POINTER(2));

  if (prefs->lamp_buttons) {
    lives_toggle_button_set_mode(LIVES_TOGGLE_BUTTON(checkbutton), FALSE);
    lives_widget_set_bg_color(checkbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    lives_widget_set_bg_color(checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
    lives_cool_toggled(checkbutton, togglevalue);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_EXPOSE_EVENT,
                              LIVES_GUI_CALLBACK(draw_cool_toggle), NULL);
    if (widget_opts.apply_theme) {

      set_css_value_direct(checkbutton,  LIVES_WIDGET_STATE_NORMAL, "",
                           "box-shadow", "4px 0 alpha(white, 0.5)");
#if GTK_CHECK_VERSION(3, 16, 0)
      colref = gdk_rgba_to_string(&palette->dark_red);
      set_css_value_direct(checkbutton,  LIVES_WIDGET_STATE_NORMAL, "button",
                           "color", colref);
      set_css_value_direct(checkbutton,  LIVES_WIDGET_STATE_NORMAL, "button",
                           "background-color", colref);
      lives_free(colref);

      set_css_value_direct(checkbutton,  LIVES_WIDGET_STATE_NORMAL, "",
                           "transition-duration", "0.2s");
#endif
    }
  }
  return checkbutton;
}


LiVESWidget *lives_glowing_tool_button_new(const char *labeltext, LiVESToolbar * tbar, const char *tooltip,
    boolean * togglevalue) {
  LiVESToolItem *titem = lives_tool_item_new();
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT;
  LiVESWidget *button = lives_glowing_check_button_new(labeltext, LIVES_BOX(hbox), tooltip, togglevalue);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_container_add(LIVES_CONTAINER(titem), hbox);
  if (tbar) lives_toolbar_insert(tbar, titem, -1);
  return button;
}


LiVESToolItem *lives_standard_menu_tool_button_new(LiVESWidget * icon, const char *label) {
  LiVESToolItem *toolitem = NULL;
#ifdef GUI_GTK
  toolitem = lives_menu_tool_button_new(icon, label);
  if (widget_opts.apply_theme) {
    LiVESList *children = lives_container_get_children(LIVES_CONTAINER(toolitem)), *list = children;
    set_standard_widget(LIVES_WIDGET(toolitem), TRUE);
    lives_widget_set_bg_color(LIVES_WIDGET(toolitem), LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    while (list) {
      LiVESWidget *widget = (LiVESWidget *)list->data;
      if (LIVES_IS_VBOX(widget)) {
        LiVESList *children2 = lives_container_get_children(LIVES_CONTAINER(toolitem)), *list2 = children2;
        lives_container_set_border_width(LIVES_CONTAINER(widget), 0);
        while (list2) {
          LiVESWidget *child = (LiVESWidget *)list2->data;
          if (LIVES_IS_CONTAINER(child)) lives_container_set_border_width(LIVES_CONTAINER(child), 0);
          lives_widget_set_valign(child, LIVES_ALIGN_FILL);
          lives_widget_set_halign(child, LIVES_ALIGN_FILL);
          lives_widget_set_margin_left(child, 0.);
          lives_widget_set_margin_right(child, 0.);
          lives_widget_set_margin_top(child, 0.);
          lives_widget_set_margin_bottom(child, 0.);
          list2 = list2->next;
        }
        lives_widget_set_bg_color(widget, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
        list2 = list2->next;
        lives_list_free(children2);
      }
      list = list->next;
    }
    lives_list_free(children);
    set_css_value_direct(LIVES_WIDGET(toolitem), LIVES_WIDGET_STATE_NORMAL, "box *", "min-height", "0px");
  }
#endif
  return toolitem;
}


LiVESWidget *lives_standard_radio_button_new(const char *labeltext, LiVESSList **rbgroup, LiVESBox * box, const char *tooltip) {
  LiVESWidget *radiobutton = NULL;

  // pack a themed check button into box

  LiVESWidget *eventbox = NULL;
  LiVESWidget *img_tips = NULL;
  LiVESWidget *hbox;

#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref, *tmp; //*csstxt;
#endif

  boolean expand;

  widget_opts.last_label = NULL;

  radiobutton = lives_radio_button_new(*rbgroup);

  *rbgroup = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton));

  if (tooltip) img_tips = lives_widget_set_tooltip_text(radiobutton, tooltip);

  if (box) {
    LiVESWidget *layout;
    int packing_width = 0;

    if (labeltext) {
      eventbox = make_label_eventbox(labeltext, radiobutton);
      lives_widget_set_show_hide_with(radiobutton, eventbox);
    }

    hbox = make_inner_hbox(LIVES_BOX(box), !widget_opts.swap_label);
    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (widget_opts.swap_label && eventbox)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box),
             WH_LAYOUT_KEY);

    if (LIVES_SHOULD_EXPAND_WIDTH) {
      if (layout) {
        int nadded = GET_INT_DATA(layout, WADDED_KEY);
        if (nadded > 0) lives_widget_set_margin_left(radiobutton, widget_opts.packing_width);
      }
    }
    lives_box_pack_start(LIVES_BOX(hbox), radiobutton, expand, expand,
                         !eventbox ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));
    lives_widget_set_show_hide_parent(radiobutton);

    if (!widget_opts.swap_label && eventbox)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    add_warn_image(radiobutton, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
    }
  } else {
    if (img_tips) break_me("floating img tips for radiobutton !");
  }

  if (widget_opts.apply_theme) {
    set_standard_widget(radiobutton, TRUE);
    lives_widget_apply_theme(radiobutton, LIVES_WIDGET_STATE_NORMAL);
#if GTK_CHECK_VERSION(3, 16, 0)
    if (prefs->extra_colours && mainw->pretty_colours) {

      if (!(palette->style & STYLE_LIGHT))
        colref = gdk_rgba_to_string(&palette->nice2);
      else
        colref = gdk_rgba_to_string(&palette->menu_and_bars);
      set_css_value_direct(radiobutton, LIVES_WIDGET_STATE_NORMAL, "radio", "color", colref);
      tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(radiobutton, LIVES_WIDGET_STATE_NORMAL, "radio",
                           "background-image", tmp);
      lives_free(colref);


      if (!(palette->style & STYLE_LIGHT)) {
        colref = gdk_rgba_to_string(&palette->normal_fore);
        set_css_value_direct(radiobutton, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
      } else {
        set_css_value_direct(radiobutton, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.25");
        set_css_value_direct(radiobutton, LIVES_WIDGET_STATE_CHECKED, "", "opacity", "0.5");
        colref = gdk_rgba_to_string(&palette->nice3);
      }
      set_css_value_direct(radiobutton, LIVES_WIDGET_STATE_CHECKED, "radio", "color", colref);

      tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(radiobutton, LIVES_WIDGET_STATE_CHECKED, "radio",
                           "background-image", tmp);
      /* csstxt = lives_strdup_printf("-gtk-gradient (radial, center center, 0, center center, " */
      /*                              "0.125, to (%s), to (rgba(0,0,0,0)))", colref); */
      /* set_css_value_direct(radiobutton, LIVES_WIDGET_STATE_CHECKED, "radio", */
      /*                      "border-image-source", csstxt); */
      //lives_free(csstxt);
      lives_free(tmp);
      lives_free(colref);
    }
#endif
  }
  return radiobutton;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_step_increment(LiVESSpinButton * button, double step_increment) {
#ifdef GUI_GTK
  LiVESAdjustment *adj = lives_spin_button_get_adjustment(button);
  lives_adjustment_set_step_increment(adj, step_increment);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_snap_to_multiples(LiVESSpinButton * button, double mult) {
#ifdef GUI_GTK
  lives_spin_button_set_step_increment(button, mult);
  lives_spin_button_set_snap_to_ticks(button, TRUE);
  return TRUE;
#endif
  return FALSE;
}


size_t calc_spin_button_width(double min, double max, int dp) {
  char *txt = lives_strdup_printf("%d", (int)max);
  size_t maxlen = strlen(txt);
  lives_free(txt);
  txt = lives_strdup_printf("%d", (int)min);
  if (strlen(txt) > maxlen) maxlen = strlen(txt);
  lives_free(txt);
  if (dp > 0) maxlen += dp + 1;
  if (maxlen < MIN_SPINBUTTON_SIZE) return MIN_SPINBUTTON_SIZE;
  return maxlen;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_adjustment_copy(LiVESAdjustment * adj) {
  LiVESAdjustment *adj2 = lives_adjustment_new(lives_adjustment_get_value(adj),
                          lives_adjustment_get_lower(adj),
                          lives_adjustment_get_upper(adj),
                          lives_adjustment_get_step_increment(adj),
                          lives_adjustment_get_page_increment(adj),
                          lives_adjustment_get_page_size(adj));
  return adj2;
}


WIDGET_HELPER_GLOBAL_INLINE
boolean lives_adjustment_configure_the_good_bits(LiVESAdjustment * adj,
    double value, double lower, double upper) {
  return lives_adjustment_configure(adj, value, lower, upper,
                                    lives_adjustment_get_step_increment(adj),
                                    lives_adjustment_get_page_increment(adj));
}


void spval_sets_start(LiVESSpinButton * sp1, LiVESSpinButton * sp2) {
  LiVESAdjustment *adj = lives_spin_button_get_adjustment(sp2);
  double val;
  int excl = GET_INT_DATA(sp1, EXCL_KEY);
  if (!excl) val = lives_spin_button_get_value(sp1);
  else val = (double)(lives_spin_button_get_value_as_int(sp1) + excl);
  lives_adjustment_set_lower(adj, val);
}

void spval_sets_end(LiVESSpinButton * sp1, LiVESSpinButton * sp2) {
  LiVESAdjustment *adj = lives_spin_button_get_adjustment(sp2);
  double val;
  int excl = GET_INT_DATA(sp1, EXCL_KEY);
  if (!excl) val = lives_spin_button_get_value(sp1);
  else val = (double)(lives_spin_button_get_value_as_int(sp1) + excl);
  lives_adjustment_set_upper(adj, val);
}


boolean spin_ranges_set_exclusive(LiVESSpinButton * sp1, LiVESSpinButton * sp2, int excl) {
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(sp1), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(spval_sets_start), sp2);
  if (excl) SET_INT_DATA(sp1, EXCL_KEY, excl);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(sp2), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(spval_sets_end), sp1);
  if (excl) SET_INT_DATA(sp2, EXCL_KEY, excl);

  return TRUE;
}



LiVESWidget *lives_standard_spin_button_new(const char *labeltext, double val, double min,
    double max, double step, double page, int dp, LiVESBox * box,
    const char *tooltip) {
  // pack a themed spin button into box
  LiVESWidget *spinbutton = NULL;
  LiVESWidget *img_tips = NULL;
  LiVESWidget *eventbox = NULL;
  LiVESWidget *container = NULL;
  LiVESWidget *hbox;
  LiVESAdjustment *adj;

#if GTK_CHECK_VERSION(3, 14, 0)
  char *colref;
#endif
  boolean has_snapval = FALSE;
  boolean expand;

  int maxlen;

  if (step < 0.) {
    step = -step;
    has_snapval = TRUE;
  }

  widget_opts.last_label = NULL;

  if (step == 0.) step = 1. / (double)lives_10pow(dp);
  if (page == 0.) page = step;
  adj = lives_adjustment_new(val, min, max, step, page, 0.);
  spinbutton = lives_spin_button_new(adj, 1, dp);

  if (has_snapval) {
    SET_INT_DATA(spinbutton, SNAPVAL_KEY, TRUE);
    lives_spin_button_set_snap_to_ticks(LIVES_SPIN_BUTTON(spinbutton), TRUE);
    val = lives_spin_button_get_snapval(LIVES_SPIN_BUTTON(spinbutton), val);
  }

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(spinbutton), val);
  lives_spin_button_update(LIVES_SPIN_BUTTON(spinbutton));
  set_standard_widget(spinbutton, TRUE);

  if (tooltip) img_tips = lives_widget_set_tooltip_text(spinbutton, tooltip);

  maxlen = calc_spin_button_width(min, max, dp);
  lives_entry_set_width_chars(LIVES_ENTRY(spinbutton), maxlen);

  lives_entry_set_activates_default(LIVES_ENTRY(spinbutton), TRUE);
  lives_entry_set_has_frame(LIVES_ENTRY(spinbutton), TRUE);
  lives_entry_set_alignment(LIVES_ENTRY(spinbutton), 0.2);
#ifdef GUI_GTK
  gtk_spin_button_set_numeric(LIVES_SPIN_BUTTON(spinbutton), TRUE);
  gtk_entry_set_overwrite_mode(LIVES_ENTRY(spinbutton), TRUE);
#endif

  if (box) {
    LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box),
                          WH_LAYOUT_KEY);
    int packing_width = 0;

    /* if (layout) { */
    /*   int nadded = GET_INT_DATA(layout, WADDED_KEY); */
    /*   if (nadded <= 1) layout = NULL; */
    /* } */

    if (labeltext) {
      eventbox = make_label_eventbox(labeltext, spinbutton);
      lives_widget_set_show_hide_with(spinbutton, eventbox);
    }

    hbox = make_inner_hbox(LIVES_BOX(box), widget_opts.swap_label || !eventbox);
    lives_widget_set_show_hide_with(spinbutton, hbox);
    container = widget_opts.last_container;

    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (!widget_opts.swap_label && eventbox) {
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

      if (layout) {
        if (widget_opts.justify == LIVES_JUSTIFY_CENTER) {
          lives_widget_set_halign(LIVES_WIDGET(box), LIVES_ALIGN_CENTER);
        } else {
          // pack end because box is a layout hbox
          lives_widget_set_pack_type(LIVES_BOX(box), container, LIVES_PACK_END);
          lives_widget_set_halign(LIVES_WIDGET(box), LIVES_ALIGN_END);
        }
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
      }
    }

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start(LIVES_BOX(hbox), spinbutton, expand, TRUE, packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.swap_label && eventbox) {
      if (layout) {
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        if (widget_opts.justify == LIVES_JUSTIFY_CENTER) {
          lives_widget_set_halign(LIVES_WIDGET(box), LIVES_ALIGN_CENTER);
        }
        hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
        lives_widget_set_show_hide_with(spinbutton, hbox);
      }
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);
    }
    lives_widget_set_show_hide_parent(spinbutton);

    add_warn_image(spinbutton, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
    }
  } else {
    if (img_tips) break_me("floating img tips for spinbutton !");
  }

  if (widget_opts.apply_theme) {
    set_css_min_size(spinbutton, widget_opts.css_min_width, (((widget_opts.css_min_height * 3 + 3) >> 2) << 1) - 2);

#if !GTK_CHECK_VERSION(3, 16, 0)
    // breaks button insens !
    lives_widget_apply_theme2(LIVES_WIDGET(spinbutton), LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(spinbutton, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
#else

    lives_widget_apply_theme2(LIVES_WIDGET(spinbutton), LIVES_WIDGET_STATE_NORMAL, FALSE);

    colref = gdk_rgba_to_string(&palette->normal_fore);
    set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_NORMAL, "", "color", colref);
    lives_free(colref);

    set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");

    if (prefs->extra_colours && mainw->pretty_colours) {
      char *tmp;
      colref = gdk_rgba_to_string(&palette->nice1);
      set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_NORMAL, "", "border-color", colref);
      set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "2px");
      set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_FOCUSED, "", "border-width", "2px");
      set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_NORMAL, "entry selection", "background-color", colref);
      tmp = lives_strdup_printf("0 0 0 1px %s inset", colref);
      set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_FOCUSED, "", "box-shadow", tmp);
      lives_free(tmp);
      lives_free(colref);
      colref = gdk_rgba_to_string(&palette->nice3);
      set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_NORMAL, "", "caret-color", colref);
      lives_free(colref);
      colref = gdk_rgba_to_string(&palette->normal_fore);
      set_css_value_direct(spinbutton, LIVES_WIDGET_STATE_NORMAL, "entry selection", "color", colref);
      lives_free(colref);
    }
#endif
  }

  widget_opts.last_container = container;
  return spinbutton;
}


static void setminsz(LiVESWidget * widget, livespointer data) {
  set_css_min_size(widget, widget_opts.css_min_width, ((widget_opts.css_min_height * 3 + 3) >> 2) << 1);
  if (LIVES_IS_BUTTON(widget)) {
    set_css_value_direct(widget, LIVES_WIDGET_STATE_NORMAL, "", "padding-top", "0");
    set_css_value_direct(widget, LIVES_WIDGET_STATE_NORMAL, "", "padding-bottom", "0");
  }
  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), setminsz, NULL);
  }
}

LiVESWidget *lives_standard_combo_new(const char *labeltext, LiVESList * list, LiVESBox * box, const char *tooltip) {
  LiVESWidget *combo = NULL;
  // pack a themed combo box into box

  // seems like it is not possible to set the arrow colours
  // nor the entirety of the background for the popup list

  LiVESWidget *eventbox = NULL;
  LiVESWidget *container = NULL;
  LiVESWidget *hbox;
  LiVESWidget *img_tips = NULL;
  LiVESEntry *entry;

  boolean expand;

  widget_opts.last_label = NULL;

  combo = lives_combo_new();

  if (tooltip) img_tips = lives_widget_set_tooltip_text(combo, tooltip);

  entry = (LiVESEntry *)lives_combo_get_entry(LIVES_COMBO(combo));
  lives_widget_set_text_size(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL, widget_opts.text_size);

  lives_entry_set_has_frame(entry, TRUE);
  lives_entry_set_editable(LIVES_ENTRY(entry), FALSE);
  lives_entry_set_activates_default(entry, TRUE);
  lives_entry_set_width_chars(LIVES_ENTRY(entry), SHORT_ENTRY_WIDTH);

  lives_widget_set_sensitive_with(LIVES_WIDGET(entry), combo);
  lives_widget_set_sensitive_with(combo, LIVES_WIDGET(entry));

  lives_widget_set_can_focus(LIVES_WIDGET(entry), FALSE);

  lives_combo_set_focus_on_click(LIVES_COMBO(combo), FALSE);

  lives_widget_add_events(LIVES_WIDGET(entry), LIVES_BUTTON_RELEASE_MASK);
  lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                                    LIVES_GUI_CALLBACK(lives_combo_popup), combo);

  if (box) {
    LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box), WH_LAYOUT_KEY);
    int packing_width = 0;

    if (labeltext) {
      eventbox = make_label_eventbox(labeltext, LIVES_WIDGET(entry));
    }

    hbox = make_inner_hbox(LIVES_BOX(box), widget_opts.swap_label || !eventbox);
    lives_widget_set_show_hide_with(combo, hbox);
    container = widget_opts.last_container;

    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);
    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (!widget_opts.swap_label && eventbox) {
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);
      if (layout) {
        // pack end because box is a layout hbox
        lives_widget_set_pack_type(LIVES_BOX(box), container, LIVES_PACK_END);
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
      }
    }

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_widget_set_hexpand(combo, FALSE);
    lives_widget_set_valign(combo, LIVES_ALIGN_CENTER);
    lives_box_pack_start(LIVES_BOX(hbox), combo, LIVES_SHOULD_EXPAND_WIDTH,
                         expand, !eventbox ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.swap_label && eventbox) {
      if (layout) {
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
        lives_widget_set_show_hide_with(combo, hbox);
      }
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, 0);
    }
    lives_widget_set_show_hide_parent(combo);

    add_warn_image(combo, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
    }
  } else {
    if (img_tips) break_me("floating img tips for combo !");
  }

  if (list) {
    lives_combo_populate(LIVES_COMBO(combo), list);
    lives_combo_set_active_index(LIVES_COMBO(combo), 0);
  }

  if (widget_opts.apply_theme) {
    set_standard_widget(combo, TRUE);
    set_child_alt_colour(combo, TRUE);

#if GTK_CHECK_VERSION(3, 0, 0)
    //set_css_value_direct(combo, LIVES_WIDGET_STATE_NORMAL, "*", "border-width", "0");

    set_css_min_size(combo, widget_opts.css_min_width, ((widget_opts.css_min_height * 3 + 3) >> 2) << 1);
    lives_container_forall(LIVES_CONTAINER(combo), setminsz, NULL);

    set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL, "", "border-radius", "5px");

#if !GTK_CHECK_VERSION(3, 16, 0)
    set_child_dimmed_colour(combo, BUTTON_DIM_VAL); // insens, themecols 1, child only
#else
    set_css_value_direct(combo, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
    if (prefs->extra_colours && mainw->pretty_colours) {
      char *tmp;
      char *colref = gdk_rgba_to_string(&palette->nice1);
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL, "", "border-color", colref);
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL, "", "border-width", "2px");
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_FOCUSED, "", "border-width", "2px");
      tmp = lives_strdup_printf("0 0 0 1px %s inset", colref);
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_FOCUSED, "", "box-shadow", tmp);
      lives_free(tmp);
      lives_free(colref);
    }
#endif
#endif
#if !GTK_CHECK_VERSION(3, 16, 0)
    lives_widget_apply_theme_dimmed(combo, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    lives_widget_apply_theme_dimmed(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb), NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(entry), NULL, NULL);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(combo), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb), NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(combo), NULL, NULL);
#endif
  }
  widget_opts.last_container = container;
  return combo;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_combo_new_with_model(LiVESTreeModel * model, LiVESBox * box) {
  LiVESWidget *combo = lives_standard_combo_new(NULL, NULL, box, NULL);
  lives_combo_set_model(LIVES_COMBO(combo), model);
  return combo;
}


LiVESWidget *lives_standard_entry_new(const char *labeltext, const char *txt, int dispwidth, int maxchars,
                                      LiVESBox * box, const char *tooltip) {
  LiVESWidget *entry = NULL;
  LiVESWidget *img_tips = NULL;
  LiVESWidget *container = NULL;
  LiVESWidget *hbox = NULL;
  LiVESWidget *eventbox = NULL;

  boolean expand;

  widget_opts.last_label = NULL;

  entry = lives_entry_new();
  lives_widget_set_valign(entry, LIVES_ALIGN_CENTER);

  lives_widget_set_text_size(entry, LIVES_WIDGET_STATE_NORMAL, widget_opts.text_size);

  if (tooltip) img_tips = lives_widget_set_tooltip_text(entry, tooltip);

  if (txt) lives_entry_set_text(LIVES_ENTRY(entry), txt);

  if (dispwidth != -1) lives_entry_set_width_chars(LIVES_ENTRY(entry), dispwidth);
  else {
    if (!LIVES_SHOULD_EXPAND_EXTRA_WIDTH) lives_entry_set_width_chars(LIVES_ENTRY(entry), MEDIUM_ENTRY_WIDTH);
    else lives_widget_set_hexpand(entry, TRUE);
  }

  if (maxchars != -1) lives_entry_set_max_length(LIVES_ENTRY(entry), maxchars);

  lives_entry_set_activates_default(LIVES_ENTRY(entry), TRUE);
  lives_entry_set_has_frame(LIVES_ENTRY(entry), TRUE);

  //lives_widget_set_halign(entry, LIVES_ALIGN_START);  // NO ! - causes entry to shrink
  if (widget_opts.justify == LIVES_JUSTIFY_START) {
    lives_entry_set_alignment(LIVES_ENTRY(entry), 0.);
  }
  if (widget_opts.justify == LIVES_JUSTIFY_CENTER) {
    lives_entry_set_alignment(LIVES_ENTRY(entry), 0.5);
  }
  if (widget_opts.justify == LIVES_JUSTIFY_END) {
    lives_entry_set_alignment(LIVES_ENTRY(entry), 1.);
  }

  if (box) {
    LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box),
                          WH_LAYOUT_KEY);
    int packing_width = 0;

    if (labeltext) {
      eventbox = make_label_eventbox(labeltext, entry);
    }

    hbox = make_inner_hbox(LIVES_BOX(box), widget_opts.swap_label || !eventbox);
    lives_widget_set_show_hide_with(entry, hbox);
    container = widget_opts.last_container;

    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (!widget_opts.swap_label && eventbox) {
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

      if (layout) {
        // pack end because box is a layout hbox
        lives_widget_set_pack_type(LIVES_BOX(box), container, LIVES_PACK_END);
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
      }
    }

    if (expand && dispwidth != -1) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start(LIVES_BOX(hbox), entry, LIVES_SHOULD_EXPAND_WIDTH, dispwidth == -1, packing_width);

    if (expand && dispwidth != -1) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.swap_label && eventbox) {
      if (layout) {
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
        lives_widget_set_show_hide_with(entry, hbox);
      }
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);
    }
    lives_widget_set_show_hide_parent(entry);

    add_warn_image(entry, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
    }
  } else {
    if (img_tips) break_me("floating img tips for entry !");
  }

  if (widget_opts.apply_theme) {
    set_standard_widget(entry, TRUE);
    lives_widget_apply_theme2(entry, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
    set_css_min_size(entry, widget_opts.css_min_width, ((widget_opts.css_min_height * 3 + 3) >> 2) << 1);
#if GTK_CHECK_VERSION(3, 16, 0)
    if (prefs->extra_colours && mainw->pretty_colours) {
      char *tmp;
      char *colref = gdk_rgba_to_string(&palette->nice1);
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL, "", "border-color", colref);
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_FOCUSED, "", "border-color", colref);
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL, "", "border-width", "2px");
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_FOCUSED, "", "border-width", "2px");
      tmp = lives_strdup_printf("0 0 0 2px %s inset", colref);
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_FOCUSED, "", "box-shadow", tmp);
      lives_free(tmp);
      lives_free(colref);
      colref = gdk_rgba_to_string(&palette->nice2);
      set_css_value_direct(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL, "selection", "background-color", colref);
      lives_free(colref);
    }

    set_css_value_direct(entry, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_NOTIFY_SIGNAL "editable",
                                    LIVES_GUI_CALLBACK(edit_state_cb), NULL);
#if !GTK_CHECK_VERSION(3, 16, 0)
    lives_widget_apply_theme_dimmed(entry, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
#endif
#else
    lives_widget_apply_theme_dimmed(entry, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
#endif
#if !GTK_CHECK_VERSION(3, 16, 0)
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb), NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(entry), NULL, NULL);
#endif
  }

  //lives_widget_set_size_request(entry, -1, (widget_opts.css_min_height * 2 + 1) >> 1);
  widget_opts.last_container = container;
  return entry;
}


void set_progbar_colours(LiVESWidget * pbar, boolean new) {
#if GTK_CHECK_VERSION(3, 16, 0)
  static boolean done = FALSE;
  if (new || !done) {
    char *tmp, *colref;
#ifdef PROGBAR_IS_ENTRY
    char *colref2;
#endif
    if (prefs->extra_colours && mainw->pretty_colours) {
      done = TRUE;
      colref = gdk_rgba_to_string(&palette->nice1);
      tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(pbar, LIVES_WIDGET_STATE_NORMAL, "progress",
                           "background-image", tmp);
      set_css_value_direct(pbar, LIVES_WIDGET_STATE_NORMAL, "progress",
                           "background-color", colref);
      set_css_value_direct(pbar, LIVES_WIDGET_STATE_NORMAL, "progress",
                           "color", colref);
      set_css_value_direct(pbar, LIVES_WIDGET_STATE_NORMAL, "progress",
                           "border-color", colref);
      lives_free(tmp);
#ifdef PROGBAR_IS_ENTRY
      colref2 = gdk_rgba_to_string(&palette->nice2);
      tmp = lives_strdup_printf("linear-gradient(to right, %s, %s)", colref2, colref);
      set_css_value_direct(pbar, LIVES_WIDGET_STATE_INSENSITIVE, "progress",
                           "background-image", tmp);
      lives_free(tmp);
      lives_free(colref2);
      set_css_min_size_selected(pbar, "progress", widget_opts.css_min_width * 4, widget_opts.css_min_height);
#endif
      lives_free(colref);
    }
  }
#endif
}

LiVESWidget *lives_standard_progress_bar_new(void) {
  LiVESWidget *pbar;
#ifdef PROGBAR_IS_ENTRY
  pbar = lives_entry_new();
  set_standard_widget(pbar, TRUE);
  lives_widget_set_valign(pbar, LIVES_ALIGN_CENTER);
  lives_widget_set_text_size(pbar, LIVES_WIDGET_STATE_NORMAL, widget_opts.text_size);
  lives_entry_set_editable(LIVES_ENTRY(pbar), FALSE);
  lives_widget_set_can_focus(LIVES_WIDGET(pbar), FALSE);
  if (widget_opts.justify == LIVES_JUSTIFY_START) {
    lives_entry_set_alignment(LIVES_ENTRY(pbar), 0.);
  }
  if (widget_opts.justify == LIVES_JUSTIFY_CENTER) {
    lives_entry_set_alignment(LIVES_ENTRY(pbar), 0.5);
  }
  if (widget_opts.justify == LIVES_JUSTIFY_END) {
    lives_entry_set_alignment(LIVES_ENTRY(pbar), 1.);
  }
#else
  pbar = lives_progress_bar_new();
#endif
  if (widget_opts.apply_theme) {
    set_standard_widget(pbar, TRUE);
    lives_widget_apply_theme(pbar, LIVES_WIDGET_STATE_NORMAL);
    set_standard_widget(pbar, TRUE);
    set_css_min_size(pbar, -1, widget_opts.css_min_height);
#ifndef VALGRIND_ON
#if GTK_CHECK_VERSION(3, 16, 0)
    if (!prefs->vj_mode && !mainw->debug) {
      set_progbar_colours(pbar, TRUE);
    }
#endif
#endif
  }
#ifdef PROGBAR_IS_ENTRY
  set_css_min_size_selected(pbar, "progress", -1, -1);
#endif
  return pbar;
}


LiVESWidget *lives_dialog_add_button_from_stock(LiVESDialog * dialog, const char *stock_id,
    const char *label, LiVESResponseType response_id) {
  int bwidth = LIVES_SHOULD_EXPAND_EXTRA_WIDTH ? DLG_BUTTON_WIDTH * 2 : DLG_BUTTON_WIDTH;
  LiVESWidget *button = lives_standard_button_new_from_stock(stock_id, label, bwidth,
                        DLG_BUTTON_HEIGHT);
  LiVESWidget *first_button;

  if (dialog) lives_dialog_add_action_widget(dialog, button, response_id);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), NWIDTH_KEY, LIVES_INT_TO_POINTER(bwidth));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), THEME_KEY,
                               LIVES_INT_TO_POINTER(widget_opts.apply_theme));

  if (dialog) {
    /// if we have only one button, center it
    if (!(first_button =
            (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(dialog), FBUTT_KEY))) {
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dialog), FBUTT_KEY, (livespointer)button);
      if (LIVES_SHOULD_EXPAND_WIDTH) lives_button_center(button);
    } else {
      /// else attach at end
      lives_button_uncenter(first_button,
                            LIVES_POINTER_TO_INT(lives_widget_object_get_data
                                (LIVES_WIDGET_OBJECT(first_button), NWIDTH_KEY)));
    }
  }

  lives_widget_apply_theme(button, LIVES_WIDGET_STATE_NORMAL);
  if (is_standard_widget(button)) render_standard_button(LIVES_BUTTON(button));
  return button;
}


WIDGET_HELPER_LOCAL_INLINE void dlg_focus_changed(LiVESContainer * c, LiVESWidget * widget, livespointer user_data) {
#if GTK_CHECK_VERSION(2, 18, 0)
  LiVESWidget *entry = NULL;
  while (LIVES_IS_CONTAINER(widget)) {
    LiVESWidget *fchild = lives_container_get_focus_child(LIVES_CONTAINER(widget));
    if (!fchild || fchild == widget) break;
    widget = fchild;
  }

  if (LIVES_IS_COMBO(widget)) {
    entry = lives_combo_get_entry(LIVES_COMBO(widget));
  } else entry = widget;

  if (entry && LIVES_IS_ENTRY(entry)) {
    if (lives_entry_get_activates_default(LIVES_ENTRY(widget))) {
      LiVESWidget *toplevel = lives_widget_get_toplevel(widget);
      LiVESWidget *button;
      if (!LIVES_IS_WIDGET(toplevel)) return;
      button = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(toplevel), DEFBUTTON_KEY);
      if (button && lives_widget_is_sensitive(button)) {
        // default button gets the default
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(toplevel), CDEF_KEY, NULL);
        lives_widget_grab_default(button);
        lives_widget_queue_draw(button);
      }
    }
  }
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_dialog_set_button_layout(LiVESDialog * dlg,
    LiVESButtonBoxStyle bstyle) {
  LiVESWidget *bbox = lives_dialog_get_action_area(dlg);
  return lives_button_box_set_layout(LIVES_BUTTON_BOX(bbox), bstyle);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAccelGroup *lives_window_add_escape(LiVESWindow * win, LiVESWidget * button) {
  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_widget_add_accelerator(button, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);
  lives_window_add_accel_group(win, accel_group);
  return accel_group;
}


WIDGET_HELPER_GLOBAL_INLINE ulong lives_window_block_delete(LiVESWindow * win) {
  ulong func = lives_signal_sync_connect(LIVES_GUI_OBJECT(win), LIVES_WIDGET_DELETE_EVENT,
                                         LIVES_GUI_CALLBACK(return_true), NULL);
  return func;
}


LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons, int width, int height) {
  // in case of problems, try setting widget_opts.no_gui=TRUE

  LiVESWidget *dialog = NULL;
  LiVESWidget *content = NULL;
  LiVESWidget *fake_action = NULL;

  dialog = lives_dialog_new();
  content = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  set_standard_widget(dialog, TRUE);
  fake_action = lives_hbutton_box_new();
  // buttons can be at top too...
  //lives_box_pack_start(LIVES_BOX(content), fake_action, FALSE, FALSE, 0);
  lives_box_pack_end(LIVES_BOX(content), fake_action, FALSE, FALSE, 0);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dialog), ACTION_AREA_KEY, (livespointer)fake_action);

  if (width <= 0) width = 8;
  if (height <= 0) height = 8;

  if (!widget_opts.no_gui) {
    LiVESWindow *transient = widget_opts.transient;
    if (!transient) transient = get_transient_full();
    if (transient) lives_window_set_transient_for(LIVES_WINDOW(dialog), transient);
  }

  lives_window_set_monitor(LIVES_WINDOW(dialog), widget_opts.monitor);

  if (width == -1 && height == -1) {
    lives_widget_set_minimum_size(dialog, DEF_DIALOG_WIDTH >> 1, DEF_DIALOG_HEIGHT >> 1);
    lives_window_set_default_size(LIVES_WINDOW(dialog), DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);
  } else {
#if !GTK_CHECK_VERSION(3, 0, 0)
    if (height > 8 && width > 8) {
#endif
      lives_widget_set_minimum_size(dialog, width, height);
#if !GTK_CHECK_VERSION(3, 0, 0)
    }
#endif

    lives_window_set_default_size(LIVES_WINDOW(dialog), width, height);
    lives_widget_set_size_request(dialog, width, height);
  }

  if (title) lives_window_set_title(LIVES_WINDOW(dialog), title);

  lives_window_set_deletable(LIVES_WINDOW(dialog), FALSE);

  if (LIVES_SHOULD_EXPAND_WIDTH) lives_widget_set_hexpand(dialog, TRUE);
  if (LIVES_SHOULD_EXPAND_HEIGHT) lives_widget_set_vexpand(dialog, TRUE);

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(dialog, LIVES_WIDGET_STATE_NORMAL);
    funkify_dialog(dialog);
#if GTK_CHECK_VERSION(2, 18, 0)
    lives_signal_sync_connect(LIVES_GUI_OBJECT(lives_dialog_get_content_area(LIVES_DIALOG(dialog))),
                              LIVES_WIDGET_SET_FOCUS_CHILD_SIGNAL,
                              LIVES_GUI_CALLBACK(dlg_focus_changed), NULL);
#endif
  } else {
    lives_container_set_border_width(LIVES_CONTAINER(dialog), widget_opts.border_width * 2);
  }

  // do this before widget_show(), then call lives_window_center() afterwards
  lives_window_set_position(LIVES_WINDOW(dialog), LIVES_WIN_POS_CENTER_ALWAYS);

  if (add_std_buttons) {
    // cancel button will automatically destroy the dialog
    // ok button needs manual destruction
    LiVESWidget *cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
                                LIVES_STOCK_CANCEL, NULL, LIVES_RESPONSE_CANCEL);

    LiVESWidget *okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
                            LIVES_STOCK_OK, NULL, LIVES_RESPONSE_OK);

    lives_button_grab_default_special(okbutton);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);

    lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);
    lives_window_block_delete(LIVES_WINDOW(dialog));

    if (widget_opts.apply_theme) {
#if !GTK_CHECK_VERSION(3, 16, 0)
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                      LIVES_GUI_CALLBACK(widget_state_cb), NULL);
      widget_state_cb(LIVES_WIDGET_OBJECT(cancelbutton), NULL, NULL);

      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                      LIVES_GUI_CALLBACK(widget_state_cb), NULL);
      widget_state_cb(LIVES_WIDGET_OBJECT(okbutton), NULL, NULL);
#endif
    }
  }

  if (!widget_opts.non_modal) {
    lives_window_set_modal(LIVES_WINDOW(dialog), TRUE);
    lives_window_set_resizable(LIVES_WINDOW(dialog), FALSE);
  }

  return dialog;
}


LiVESWidget *lives_standard_font_chooser_new(const char *fontname) {
  LiVESWidget *font_choo = NULL;
  int width = DEF_BUTTON_WIDTH, height = ((widget_opts.css_min_height * 3 + 3) >> 2) << 1;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 2, 0)
  char *ttl;
  font_choo = gtk_font_button_new();
  gtk_font_button_set_show_size(GTK_FONT_BUTTON(font_choo), FALSE);
  gtk_font_chooser_set_show_preview_entry(GTK_FONT_CHOOSER(font_choo), TRUE);
  gtk_font_chooser_set_preview_text(GTK_FONT_CHOOSER(font_choo), "LiVES");
  ttl = lives_strdup_printf("%s%s", widget_opts.title_prefix, _("Choose a Font..."));
  gtk_font_button_set_title(GTK_FONT_BUTTON(font_choo), ttl);
  lives_free(ttl);

  if (fontname) lives_font_chooser_set_font(LIVES_FONT_CHOOSER(font_choo), fontname);

  if (widget_opts.apply_theme) {
    set_standard_widget(font_choo, TRUE);
    lives_widget_apply_theme2(font_choo, LIVES_WIDGET_STATE_NORMAL, TRUE);

#if GTK_CHECK_VERSION(3, 16, 0)
    set_css_min_size(font_choo, width, height);
    set_css_value_direct(font_choo, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");

    lives_widget_set_padding(font_choo, 0);
    set_css_value_direct(font_choo, LIVES_WIDGET_STATE_NORMAL, "", "background", "none");
    //set_css_value_direct(font_choo, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "0px");

    if (prefs->extra_colours && mainw->pretty_colours) {
      char *tmp;
      char *colref = gdk_rgba_to_string(&palette->nice1);
      set_css_value_direct(LIVES_WIDGET(font_choo), LIVES_WIDGET_STATE_NORMAL, "", "border-color", colref);
      tmp = lives_strdup_printf("0 0 0 1px %s inset", colref);
      set_css_value_direct(LIVES_WIDGET(font_choo), LIVES_WIDGET_STATE_PRELIGHT, "", "box-shadow", tmp);
      lives_free(tmp);
      lives_free(colref);
      colref = gdk_rgba_to_string(&palette->nice2);
      set_css_value_direct(LIVES_WIDGET(font_choo), LIVES_WIDGET_STATE_NORMAL, "", "background-color", colref);
    }

#endif
  }
#endif
#endif
  return font_choo;
}


WIDGET_HELPER_GLOBAL_INLINE
boolean lives_standard_font_chooser_set_size(LiVESFontChooser * fchoo, int fsize) {
  LingoFontDesc *lfd = lives_font_chooser_get_font_desc(fchoo);
  lingo_fontdesc_set_size(lfd, fsize * LINGO_SCALE);
  lives_font_chooser_set_font_desc(LIVES_FONT_CHOOSER(fchoo), lfd);
  lingo_fontdesc_free(lfd);
  return TRUE;
}


extern void on_filesel_button_clicked(LiVESButton *, livespointer);

static LiVESWidget *lives_standard_dfentry_new(const char *labeltext, const char *txt, const char *defdir, int dispwidth,
    int maxchars, LiVESBox * box, const char *tooltip, boolean isdir) {
  LiVESWidget *direntry = NULL;
  LiVESWidget *buttond;
  LiVESWidget *img_tips;
  LiVESWidget *warn_img;
  LiVESWidget *layout;

  if (!box) return NULL;

  direntry = lives_standard_entry_new(labeltext, txt, dispwidth, maxchars == -1 ? PATH_MAX : maxchars, box, tooltip);
  lives_entry_set_editable(LIVES_ENTRY(direntry), FALSE);

  // add dir, with filechooser button
  buttond = lives_standard_file_button_new(isdir, defdir);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(direntry), BUTTON_KEY, buttond);

  if (widget_opts.last_label) lives_label_set_mnemonic_widget(LIVES_LABEL(widget_opts.last_label), buttond);

  layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box), WH_LAYOUT_KEY);
  if (layout) {
    LiVESWidget *hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_box_pack_start(LIVES_BOX(hbox), buttond, FALSE, FALSE, widget_opts.packing_width);
    if ((warn_img = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(direntry), WARN_IMAGE_KEY))) {
      lives_widget_object_ref(warn_img);
      lives_widget_unparent(warn_img);
      lives_box_pack_start(LIVES_BOX(hbox), warn_img, FALSE, FALSE, widget_opts.packing_width);
      lives_widget_object_unref(warn_img);
    }
    if ((img_tips = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(direntry), HAS_TTIPS_IMAGE_KEY))) {
      lives_widget_object_ref(img_tips);
      lives_widget_unparent(img_tips);
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width);
      lives_widget_object_unref(img_tips);
    }
  } else {
    lives_box_pack_start(LIVES_BOX(lives_widget_get_parent(direntry)), buttond, FALSE, FALSE, widget_opts.packing_width);

    if ((warn_img = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(direntry), WARN_IMAGE_KEY))) {
      lives_box_reorder_child(LIVES_BOX(lives_widget_get_parent(direntry)), buttond,
                              get_box_child_index(LIVES_BOX(lives_widget_get_parent(direntry)), warn_img));
    } else if ((img_tips = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(direntry), HAS_TTIPS_IMAGE_KEY))) {
      lives_box_reorder_child(LIVES_BOX(lives_widget_get_parent(direntry)), buttond,
                              get_box_child_index(LIVES_BOX(lives_widget_get_parent(direntry)), img_tips));
    }
  }

  lives_signal_sync_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                            (livespointer)direntry);

  lives_widget_set_sensitive_with(buttond, direntry);
  lives_widget_set_show_hide_with(buttond, direntry);
  lives_widget_set_sensitive_with(direntry, buttond);
  lives_widget_set_show_hide_with(direntry, buttond);
  return direntry;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_direntry_new(const char *labeltext, const char *txt, int dispwidth,
    int maxchars, LiVESBox * box, const char *tooltip) {
  return lives_standard_dfentry_new(labeltext, txt, txt, dispwidth, maxchars, box, tooltip, TRUE);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_fileentry_new(const char *labeltext, const char *txt,
    const char *defdir, int dispwidth, int maxchars, LiVESBox * box, const char *tooltip) {
  return lives_standard_dfentry_new(labeltext, txt, defdir, dispwidth, maxchars, box, tooltip, FALSE);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_toolbar_new(void) {
  LiVESWidget *toolbar = lives_toolbar_new();
  set_standard_widget(toolbar, TRUE);
  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(toolbar), TRUE);
  lives_toolbar_set_style(LIVES_TOOLBAR(toolbar), LIVES_TOOLBAR_ICONS);
  lives_toolbar_set_icon_size(LIVES_TOOLBAR(toolbar), widget_opts.icon_size);
  if (widget_opts.apply_theme) {
#if GTK_CHECK_VERSION(3, 0, 0)
    set_css_value_direct(toolbar, LIVES_WIDGET_STATE_NORMAL, "", "border-width", "0");
    set_css_min_size(toolbar, widget_opts.css_min_width, widget_opts.css_min_height);
#endif
  }
  return toolbar;
}


LiVESWidget *lives_standard_hscale_new(LiVESAdjustment * adj) {
  LiVESWidget *hscale = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hscale = gtk_scale_new(LIVES_ORIENTATION_HORIZONTAL, adj);

  if (widget_opts.apply_theme) {
    set_standard_widget(hscale, TRUE);
    lives_widget_apply_theme(hscale, LIVES_WIDGET_STATE_NORMAL);
#if GTK_CHECK_VERSION(3, 16, 0)
    char *colref = gdk_rgba_to_string(&palette->white);
    char *tmp = lives_strdup_printf("image(%s)", colref);
    set_css_value_direct(hscale, LIVES_WIDGET_STATE_NORMAL, "*",
                         "background-image", tmp);
    lives_free(tmp);
    lives_free(colref);

    if (prefs->extra_colours && mainw->pretty_colours) {
      colref = gdk_rgba_to_string(&palette->nice1);
      tmp = lives_strdup_printf("image(%s)", colref);
      set_css_value_direct(hscale, LIVES_WIDGET_STATE_NORMAL, "trough",
                           "background-image", tmp);
      lives_free(tmp);
      lives_free(colref);
    }

    set_css_min_size_selected(hscale, "slider", widget_opts.css_min_width, widget_opts.css_min_height);
    set_css_min_size_selected(hscale, "scale", DEF_BUTTON_WIDTH, widget_opts.css_min_height);
    set_css_value_direct(hscale, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
  }

#else
  hscale = gtk_hscale_new(adj);
#endif
  gtk_scale_set_draw_value(LIVES_SCALE(hscale), FALSE);
#endif
  return hscale;
}


LiVESWidget *lives_standard_hruler_new(void) {
  LiVESWidget *hruler = NULL;

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hruler = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, NULL);
  gtk_scale_set_draw_value(GTK_SCALE(hruler), FALSE);
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_scale_set_has_origin(GTK_SCALE(hruler), FALSE);
#endif
  gtk_scale_set_digits(GTK_SCALE(hruler), 8);
#else
  hruler = gtk_hruler_new();
  lives_widget_apply_theme(hruler, LIVES_WIDGET_STATE_INSENSITIVE);
  set_standard_widget(hruler, TRUE);
#endif

#endif

  return hruler;
}


double lives_scrolled_window_scroll_to(LiVESScrolledWindow * sw, LiVESPositionType pos) {
  double val;
  LiVESAdjustment *adj;
  if (!sw) return -1.;
  else {
    if (pos == LIVES_POS_TOP || pos == LIVES_POS_BOTTOM) {
      adj = lives_scrolled_window_get_vadjustment(sw);
    } else {
      adj = lives_scrolled_window_get_hadjustment(sw);
    }

    if (pos == LIVES_POS_TOP || pos == LIVES_POS_LEFT) val = lives_adjustment_get_lower(adj);
    else val = lives_adjustment_get_upper(adj) - lives_adjustment_get_page_size(adj);
    lives_adjustment_set_value(adj, val);
  }
  return val;
}


LiVESWidget *lives_standard_scrolled_window_new(int width, int height, LiVESWidget * child) {
  LiVESWidget *scrolledwindow = NULL;
  LiVESWidget *swchild;

  scrolledwindow = lives_scrolled_window_new();
  set_standard_widget(scrolledwindow, TRUE);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(scrolledwindow),
                                   LIVES_POLICY_AUTOMATIC, LIVES_POLICY_AUTOMATIC);

  if (LIVES_SHOULD_EXPAND_WIDTH)
    lives_widget_set_hexpand(scrolledwindow, TRUE);
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_widget_set_vexpand(scrolledwindow, TRUE);

  lives_container_set_border_width(LIVES_CONTAINER(scrolledwindow), widget_opts.border_width);

  if (child) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
    if (!LIVES_IS_SCROLLABLE(child))
#else
    if (!LIVES_IS_TEXT_VIEW(child))
#endif
    {
      lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(scrolledwindow), child);
    } else {
      if (!LIVES_SHOULD_EXPAND_EXTRA_WIDTH) {
        LiVESWidget *align;
        align = lives_alignment_new(.5, 0., 0., 0.);
        lives_container_add(LIVES_CONTAINER(align), child);
        lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(scrolledwindow), align);
      } else {
        lives_container_add(LIVES_CONTAINER(scrolledwindow), child);
      }
    }
#endif
  }

  swchild = lives_bin_get_child(LIVES_BIN(scrolledwindow));

  lives_widget_apply_theme(swchild, LIVES_WIDGET_STATE_NORMAL);

  if (LIVES_SHOULD_EXPAND_WIDTH) {
    lives_widget_set_halign(swchild, LIVES_ALIGN_FILL);
    lives_widget_set_hexpand(swchild, TRUE);
  }
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_widget_set_vexpand(swchild, TRUE);

  if (LIVES_IS_CONTAINER(child) && LIVES_SHOULD_EXPAND) lives_container_set_border_width(LIVES_CONTAINER(child),
        widget_opts.border_width >> 1);

#ifdef GUI_GTK
  if (GTK_IS_VIEWPORT(swchild))
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(swchild), LIVES_SHADOW_IN);

  if (width != 0 && height != 0) {
#if !GTK_CHECK_VERSION(3, 0, 0)
    if (width > -1 || height > -1)
      lives_widget_set_size_request(scrolledwindow, width, height);
    lives_widget_set_minimum_size(scrolledwindow, width, height); // crash if we dont have toplevel win
#else
    if (height != -1) lives_scrolled_window_set_min_content_height(LIVES_SCROLLED_WINDOW(scrolledwindow), height);
    if (width != -1) lives_scrolled_window_set_min_content_width(LIVES_SCROLLED_WINDOW(scrolledwindow), width);
#endif
  }
#endif

  return scrolledwindow;
}


LiVESWidget *lives_standard_spinner_new(boolean start) {
  LiVESWidget *spinner = lives_spinner_new();
  if (widget_opts.apply_theme) {
    set_standard_widget(spinner, TRUE);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 16, 0)
    set_css_value_direct(spinner, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
#endif
    lives_widget_apply_theme(spinner, LIVES_WIDGET_STATE_NORMAL);
  }
  if (start) lives_spinner_start(LIVES_SPINNER(spinner));
  return spinner;
}


static void expander_swap_label(LiVESWidget * exp, livespointer data) {
  const char *ltext;
  char *labeltext;

  if (!lives_expander_get_expanded(LIVES_EXPANDER(exp))) {
    ltext = (const char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(exp),
            EXPANDER_TEXT_KEY);
  } else {
    ltext = (const char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(exp),
            EXPANDER_XTEXT_KEY);
  }

  if (GET_INT_DATA(exp, EXPANDER_XLABEL_KEY))
    labeltext = lives_strdup_printf("<big>%s</big>", ltext);
  else labeltext = (char *)ltext;

  lives_expander_set_label(LIVES_EXPANDER(exp), labeltext);

  if (labeltext != ltext) lives_free(labeltext);
}


LiVESWidget *lives_standard_expander_new(const char *ltext, const char *alt_text, LiVESBox * box, LiVESWidget * child) {
  LiVESWidget *expander = NULL, *container = NULL, *label = NULL;

#ifdef GUI_GTK
  LiVESWidget *hbox;

  expander = lives_expander_new(NULL);

  if (LIVES_SHOULD_EXPAND)
    SET_INT_DATA(expander, EXPANDER_XLABEL_KEY, TRUE);
  else
    SET_INT_DATA(expander, EXPANDER_XLABEL_KEY, FALSE);

  lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(expander), EXPANDER_TEXT_KEY,
                                    (livespointer)(lives_strdup(ltext)));
  expander_swap_label(expander, NULL);

  lives_expander_set_use_markup(LIVES_EXPANDER(expander), TRUE);

  if (box) {
    LiVESWidget *img_tips;
    int packing_width = 0;

    hbox = make_inner_hbox(LIVES_BOX(box), TRUE);
    container = widget_opts.last_container;

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (widget_opts.justify == LIVES_JUSTIFY_CENTER || widget_opts.justify == LIVES_JUSTIFY_END)
      add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.justify == LIVES_JUSTIFY_START) lives_widget_set_halign(expander, LIVES_ALIGN_START);
    //if (widget_opts.justify != LIVES_JUSTIFY_END) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.justify == LIVES_JUSTIFY_CENTER) lives_widget_set_halign(expander, LIVES_ALIGN_CENTER);
    lives_box_pack_start(LIVES_BOX(hbox), expander, TRUE, TRUE, packing_width);
    lives_widget_set_valign(expander, LIVES_ALIGN_CENTER);
    lives_widget_set_show_hide_parent(expander);

    if (widget_opts.justify == LIVES_JUSTIFY_END) lives_widget_set_halign(expander, LIVES_ALIGN_END);

    if (widget_opts.justify != LIVES_JUSTIFY_END) add_fill_to_box(LIVES_BOX(hbox));

    if (child) lives_container_add(LIVES_CONTAINER(expander), child);
    lives_container_set_border_width(LIVES_CONTAINER(expander), widget_opts.border_width);
    add_warn_image(expander, hbox);
    img_tips = lives_widget_set_tooltip_text(expander, NULL);
    if (img_tips) lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);
  }

  if (widget_opts.apply_theme) {
    set_standard_widget(expander, TRUE);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 16, 0)
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(expander), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb), NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(expander), NULL, NULL);

    if (widget_opts.last_label) {
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget_opts.last_label), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                      LIVES_GUI_CALLBACK(widget_state_cb), NULL);
      widget_state_cb(LIVES_WIDGET_OBJECT(widget_opts.last_label), NULL, NULL);
    }
#else
    set_css_value_direct(expander, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
#endif
    lives_widget_apply_theme(expander, LIVES_WIDGET_STATE_NORMAL);
    lives_container_forall(LIVES_CONTAINER(expander), set_child_colour_internal, LIVES_INT_TO_POINTER(TRUE));
#endif
  }
  label = lives_expander_get_label_widget(LIVES_EXPANDER(expander));
#endif
  widget_opts.last_container = container;
  widget_opts.last_label = label;

  if (alt_text) {
    lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(expander), EXPANDER_XTEXT_KEY,
                                      (livespointer)(lives_strdup(alt_text)));
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(expander), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                    LIVES_GUI_CALLBACK(expander_swap_label), NULL);
  }
  return expander;
}


LiVESWidget *lives_standard_table_new(uint32_t rows, uint32_t cols, boolean homogeneous) {
  LiVESWidget *table = lives_table_new(rows, cols, homogeneous);
  lives_widget_apply_theme(table, LIVES_WIDGET_STATE_NORMAL);
  set_standard_widget(table, TRUE);
  if (LIVES_SHOULD_EXPAND_WIDTH) lives_table_set_row_spacings(LIVES_TABLE(table),
        LIVES_SHOULD_EXPAND_EXTRA_WIDTH ? (widget_opts.packing_width << 2) : widget_opts.packing_width);
  else lives_table_set_row_spacings(LIVES_TABLE(table), 0);
  if (LIVES_SHOULD_EXPAND_HEIGHT) lives_table_set_col_spacings(LIVES_TABLE(table),
        LIVES_SHOULD_EXPAND_EXTRA_HEIGHT ? (widget_opts.packing_height << 2) : widget_opts.packing_height);
  else lives_table_set_col_spacings(LIVES_TABLE(table), 0);
  return table;
}


LiVESWidget *lives_standard_text_view_new(const char *text, LiVESTextBuffer * tbuff) {
  LiVESWidget *textview;

  if (!tbuff)
    textview = lives_text_view_new();
  else
    textview = lives_text_view_new_with_buffer(tbuff);

  lives_widget_set_text_size(textview, LIVES_WIDGET_STATE_NORMAL, widget_opts.text_size);
  lives_text_view_set_editable(LIVES_TEXT_VIEW(textview), FALSE);
  lives_text_view_set_wrap_mode(LIVES_TEXT_VIEW(textview), LIVES_WRAP_WORD);
  lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(textview), FALSE);
  lives_container_set_border_width(LIVES_CONTAINER(textview), 2);

  if (text) {
    if (widget_opts.use_markup) lives_text_view_set_markup(LIVES_TEXT_VIEW(textview), text);
    else {
      lives_text_view_set_text(LIVES_TEXT_VIEW(textview), text, -1);
      lives_text_view_strip_markup(LIVES_TEXT_VIEW(textview));
    }
  }

  if (widget_opts.apply_theme) {
    // WARNING - special css case - default selector only applies to text !
    set_standard_widget(textview, TRUE);
    lives_widget_apply_theme3(textview, LIVES_WIDGET_STATE_NORMAL);
    if (prefs->extra_colours && mainw->pretty_colours) {
      char *colref = gdk_rgba_to_string(&palette->menu_and_bars);
      set_css_value_direct(textview, LIVES_WIDGET_STATE_NORMAL, "", "background-color", colref);
      lives_free(colref);
    }
    set_css_value_direct(textview, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
  }

  lives_text_view_set_justification(LIVES_TEXT_VIEW(textview), widget_opts.justify);
  if (widget_opts.justify == LIVES_JUSTIFY_CENTER) {
    lives_widget_set_halign(textview, LIVES_ALIGN_CENTER);
    lives_widget_set_valign(textview, LIVES_ALIGN_CENTER);
  }
  return textview;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_file_button_new(boolean is_dir, const char *def_dir) {
  LiVESWidget *fbutton;
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_OPEN, LIVES_ICON_SIZE_LARGE_TOOLBAR);
  /// height X height is correct
  int height = ((widget_opts.css_min_height * 2 + 2) >> 2);
  fbutton = lives_standard_button_new(height, height * 4);
  lives_widget_set_valign(fbutton, LIVES_ALIGN_CENTER);
  SET_INT_DATA(fbutton, ISDIR_KEY, is_dir);
  if (def_dir) lives_widget_object_set_data(LIVES_WIDGET_OBJECT(fbutton), DEFDIR_KEY, (livespointer)def_dir);
  lives_standard_button_set_image(LIVES_BUTTON(fbutton), image, TRUE);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(LIVES_WIDGET_OBJECT(fbutton)),
                               SBUTT_FAKEDEF_KEY, fbutton);
  return fbutton;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_lock_button_get_locked(LiVESButton * button) {
  return GET_INT_DATA(button, ISLOCKED_KEY);
}


static void _on_lock_button_clicked(LiVESButton * button, livespointer user_data) {
  LiVESWidget *image;
  int locked = !GET_INT_DATA(button, ISLOCKED_KEY);
  SET_INT_DATA(button, ISLOCKED_KEY, locked);
  if (locked) {
    image = lives_image_new_from_stock(LIVES_LIVES_STOCK_LOCKED, LIVES_ICON_SIZE_BUTTON);
    lives_widget_set_opacity(LIVES_WIDGET(button), 1.0);
  } else {
    image = lives_image_new_from_stock(LIVES_LIVES_STOCK_UNLOCKED, LIVES_ICON_SIZE_BUTTON);
    lives_widget_set_opacity(LIVES_WIDGET(button), .75);
  }
  lives_standard_button_set_image(LIVES_BUTTON(button), image, TRUE);
}


boolean label_act_lockbutton(LiVESWidget * widget, LiVESXEventButton * event, LiVESButton * lockbutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(lockbutton))) return FALSE;
  _on_lock_button_clicked(lockbutton, NULL);
  return FALSE;
}


boolean lives_lock_button_toggle(LiVESButton * button) {
  _on_lock_button_clicked(button, NULL);
  return lives_lock_button_get_locked(button);
}


boolean lives_lock_button_set_locked(LiVESButton * button, boolean state) {
  if (GET_INT_DATA(button, ISLOCKED_KEY) != state)
    _on_lock_button_clicked(button, NULL);
  return lives_lock_button_get_locked(button);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_lock_button_new(boolean is_locked, const char *label,
    const char *tooltip) {
  LiVESWidget *lockbutton;
  int height = ((widget_opts.css_min_height * 3 + 3) >> 2) << 1;

  // setting width of 2 will force it to shrink to text / icon
  lockbutton = lives_standard_button_new_with_label(label, 2, height);
  lives_button_set_focus_on_click(LIVES_BUTTON(lockbutton), FALSE);
  if (tooltip) lives_widget_set_tooltip_text(lockbutton, tooltip);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(LIVES_WIDGET_OBJECT(lockbutton)),
                               SBUTT_FAKEDEF_KEY, lockbutton);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(lockbutton), ISLOCKED_KEY, LIVES_INT_TO_POINTER(!is_locked));
  lives_signal_sync_connect(lockbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(_on_lock_button_clicked), NULL);
  _on_lock_button_clicked(LIVES_BUTTON(lockbutton), LIVES_INT_TO_POINTER(widget_opts.apply_theme));
  return lockbutton;
}


static void on_pwcolselx(LiVESButton * button, lives_rfx_t *rfx) {
  LiVESWidgetColor selected;
  LiVESWidget *sp_red = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), SPRED_KEY);
  LiVESWidget *sp_green = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), SPGREEN_KEY);
  LiVESWidget *sp_blue = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), SPBLUE_KEY);
  LiVESWidget *sp_alpha = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), SPALPHA_KEY);

  int r, g, b, a;

  lives_color_button_get_color(LIVES_COLOR_BUTTON(button), &selected);

  // get 0. -> 255. values
  if (sp_red) {
    r = (int)((double)(selected.red + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(sp_red), (double)r);
  }

  if (sp_green) {
    g = (int)((double)(selected.green + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(sp_green), (double)g);
  }

  if (sp_blue) {
    b = (int)((double)(selected.blue + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(sp_blue), (double)b);
  }

  if (sp_alpha) {
#if !LIVES_WIDGET_COLOR_HAS_ALPHA
    a = lives_color_button_get_alpha(LIVES_COLOR_BUTTON(button)) / 255.;
#else
    a = (int)((double)(selected.alpha + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
#endif
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(sp_alpha), (double)a);
  }

  lives_color_button_set_color(LIVES_COLOR_BUTTON(button), &selected);
}


static void after_param_red_changedx(LiVESSpinButton * spinbutton, livespointer udata) {
  LiVESWidgetColor colr;

  LiVESWidget *cbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), CBUTTON_KEY);
  LiVESWidget *sp_green = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPGREEN_KEY);
  LiVESWidget *sp_blue = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPBLUE_KEY);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  LiVESWidget *sp_alpha = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPALPHA_KEY);
#endif

  int new_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int old_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_green));
  int old_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_blue));

  colr.red = LIVES_WIDGET_COLOR_SCALE_255(new_red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_255(old_green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_255(old_blue);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (sp_alpha) {
    int old_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_alpha));
    colr.alpha = LIVES_WIDGET_COLOR_SCALE_255(old_alpha);
  } else colr.alpha = 1.0;
#endif
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
}


static void after_param_green_changedx(LiVESSpinButton * spinbutton, livespointer udata) {
  LiVESWidgetColor colr;

  LiVESWidget *cbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), CBUTTON_KEY);
  LiVESWidget *sp_red = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPRED_KEY);
  LiVESWidget *sp_blue = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPBLUE_KEY);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  LiVESWidget *sp_alpha = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPALPHA_KEY);
#endif

  int new_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int old_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_red));
  int old_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_blue));

  colr.red = LIVES_WIDGET_COLOR_SCALE_255(old_red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_255(new_green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_255(old_blue);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (sp_alpha) {
    int old_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_alpha));
    colr.alpha = LIVES_WIDGET_COLOR_SCALE_255(old_alpha);
  } else colr.alpha = 1.0;
#endif
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
}


static void after_param_blue_changedx(LiVESSpinButton * spinbutton, livespointer udata) {
  LiVESWidgetColor colr;

  LiVESWidget *cbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), CBUTTON_KEY);
  LiVESWidget *sp_green = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPGREEN_KEY);
  LiVESWidget *sp_red = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPRED_KEY);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  LiVESWidget *sp_alpha = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPALPHA_KEY);
#endif

  int new_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int old_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_green));
  int old_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_red));

  colr.red = LIVES_WIDGET_COLOR_SCALE_255(old_red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_255(old_green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_255(new_blue);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (sp_alpha) {
    int old_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_alpha));
    colr.alpha = LIVES_WIDGET_COLOR_SCALE_255(old_alpha);
  } else colr.alpha = 1.0;
#endif
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
}


static void after_param_alpha_changedx(LiVESSpinButton * spinbutton, livespointer udata) {
  LiVESWidgetColor colr;

  LiVESWidget *cbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), CBUTTON_KEY);
  LiVESWidget *sp_green = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPGREEN_KEY);
  LiVESWidget *sp_blue = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPBLUE_KEY);
  LiVESWidget *sp_red = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPRED_KEY);

  int new_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int old_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_red));
  int old_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_green));
  int old_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_blue));

  colr.red = LIVES_WIDGET_COLOR_SCALE_255(old_red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_255(old_green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_255(old_blue);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  colr.alpha = LIVES_WIDGET_COLOR_SCALE_255(new_alpha);
#else
  lives_color_button_set_alpha(LIVES_COLOR_BUTTON(cbutton), LIVES_WIDGET_COLOR_SCALE_255(new_alpha));
#endif
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
}


LiVESWidget *lives_standard_color_button_new(LiVESBox * box, const char *name, boolean use_alpha, lives_colRGBA64_t *rgba,
    LiVESWidget **sb_red, LiVESWidget **sb_green, LiVESWidget **sb_blue, LiVESWidget **sb_alpha) {
  LiVESWidgetColor colr;
  LiVESWidget *cbutton, *labelcname = NULL;
  LiVESWidget *hbox = NULL;
  LiVESWidget *layout;
  LiVESWidget *frame = lives_standard_frame_new(NULL, 0., FALSE);
  LiVESWidget *spinbutton_red = NULL, *spinbutton_green = NULL, *spinbutton_blue = NULL, *spinbutton_alpha = NULL;
  LiVESWidget *parent = NULL;
  char *tmp, *tmp2;

  int packing_width = 0;

  boolean parent_is_layout = FALSE;
  boolean expand = FALSE;

  widget_opts.last_label = NULL;

  lives_container_set_border_width(LIVES_CONTAINER(frame), 0);

  if (box) {
    parent = lives_widget_get_parent(LIVES_WIDGET(box));
    if (parent && LIVES_IS_TABLE(parent) &&
        lives_widget_object_get_data(LIVES_WIDGET_OBJECT(parent), WADDED_KEY)) {
      parent_is_layout = TRUE;
      lives_table_set_column_homogeneous(LIVES_TABLE(parent), FALSE);
      hbox = LIVES_WIDGET(box);
    } else {
      hbox = make_inner_hbox(LIVES_BOX(box), !box || widget_opts.swap_label || !labelcname);
    }
    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width >> 1;
  }

  colr.red = LIVES_WIDGET_COLOR_SCALE_65535(rgba->red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_65535(rgba->green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_65535(rgba->blue);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (use_alpha) colr.alpha = LIVES_WIDGET_COLOR_SCALE_65535(rgba->alpha);
  else colr.alpha = 1.;
#endif

  cbutton = lives_color_button_new_with_color(&colr);

  lives_color_button_set_use_alpha(LIVES_COLOR_BUTTON(cbutton), use_alpha);
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
  lives_widget_apply_theme(cbutton, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_apply_theme2(cbutton, LIVES_WIDGET_STATE_PRELIGHT, TRUE);
  lives_widget_set_border_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

  //g_object_set(cbutton, "show-editor", TRUE, NULL);

#if !LIVES_WIDGET_COLOR_HAS_ALPHA
  if (use_alpha)
    lives_color_button_set_alpha(LIVES_COLOR_BUTTON(cbutton), rgba->alpha);
#endif

  if (name && box) {
    // must do this before re-using translation string !
    if (widget_opts.mnemonic_label) {
      labelcname = lives_standard_label_new_with_mnemonic_widget(name, cbutton);
    } else labelcname = lives_standard_label_new(name);
    lives_widget_set_show_hide_with(cbutton, labelcname);
    lives_widget_set_sensitive_with(cbutton, labelcname);
  }

  lives_widget_set_tooltip_text(cbutton, (_("Click to set the colour")));
  lives_color_button_set_title(LIVES_COLOR_BUTTON(cbutton), _("Select Colour"));

  if (box) {
    if (!widget_opts.swap_label) {
      if (labelcname) {
        if (LIVES_SHOULD_EXPAND_WIDTH) lives_widget_set_margin_left(labelcname, widget_opts.packing_width >> 2);
        lives_box_pack_start(LIVES_BOX(hbox), labelcname, FALSE, FALSE, widget_opts.packing_width);
        if (parent_is_layout) {
          hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
          widget_opts.justify = LIVES_JUSTIFY_END;
        }
      }
    }

    if (sb_red) {
      layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
      spinbutton_red = lives_standard_spin_button_new((tmp = (_("_Red"))), rgba->red / 255., 0., 255., 1., 1., 0,
                       (LiVESBox *)hbox, (tmp2 = (_("The red value (0 - 255)"))));
      lives_free(tmp);
      lives_free(tmp2);
      lives_entry_set_width_chars(LIVES_ENTRY(spinbutton_red), 3);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, layout);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_red), CBUTTON_KEY, cbutton);
      *sb_red = spinbutton_red;
      lives_signal_sync_connect(LIVES_GUI_OBJECT(spinbutton_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(after_param_red_changedx), NULL);
      if (parent_is_layout) {
        hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
      } else if (expand) add_fill_to_box(LIVES_BOX(hbox));
      lives_widget_set_sensitive_with(cbutton, spinbutton_red);
      lives_widget_set_show_hide_with(cbutton, spinbutton_red);
    }

    if (sb_green) {
      layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
      spinbutton_green = lives_standard_spin_button_new((tmp = (_("_Green"))), rgba->green / 255., 0., 255., 1., 1., 0,
                         (LiVESBox *)hbox, (tmp2 = (_("The green value (0 - 255)"))));
      lives_free(tmp);
      lives_free(tmp2);
      lives_entry_set_width_chars(LIVES_ENTRY(spinbutton_green), 3);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, layout);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_green), CBUTTON_KEY, cbutton);
      *sb_green = spinbutton_green;
      lives_signal_sync_connect(LIVES_GUI_OBJECT(spinbutton_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(after_param_green_changedx), NULL);
      if (parent_is_layout) {
        hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
      } else if (expand) add_fill_to_box(LIVES_BOX(hbox));
      lives_widget_set_sensitive_with(cbutton, spinbutton_green);
      lives_widget_set_show_hide_with(cbutton, spinbutton_green);
    }

    if (sb_blue) {
      layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
      spinbutton_blue = lives_standard_spin_button_new((tmp = (_("_Blue"))), rgba->blue / 255., 0., 255., 1., 1., 0,
                        (LiVESBox *)hbox, (tmp2 = (_("The blue value (0 - 255)"))));
      lives_free(tmp);
      lives_free(tmp2);
      lives_entry_set_width_chars(LIVES_ENTRY(spinbutton_blue), 3);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, layout);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_blue), CBUTTON_KEY, cbutton);
      *sb_blue = spinbutton_blue;
      lives_signal_sync_connect(LIVES_GUI_OBJECT(spinbutton_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(after_param_blue_changedx), NULL);
      if (parent_is_layout) {
        hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
      } else if (expand) add_fill_to_box(LIVES_BOX(hbox));
      lives_widget_set_sensitive_with(cbutton, spinbutton_blue);
      lives_widget_set_show_hide_with(cbutton, spinbutton_blue);
    }

    if (use_alpha && sb_alpha) {
      layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
      spinbutton_alpha = lives_standard_spin_button_new((tmp = (_("_Alpha"))), rgba->alpha / 255., 0., 255., 1., 1., 0,
                         (LiVESBox *)hbox, (tmp2 = (_("The alpha value (0 - 255)"))));
      lives_free(tmp);
      lives_free(tmp2);
      lives_entry_set_width_chars(LIVES_ENTRY(spinbutton_alpha), 3);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, layout);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_alpha), CBUTTON_KEY, cbutton);
      *sb_alpha = spinbutton_alpha;
      lives_signal_sync_connect(LIVES_GUI_OBJECT(spinbutton_alpha), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(after_param_alpha_changedx), NULL);
      if (parent_is_layout) {
        hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
      } else if (expand) add_fill_to_box(LIVES_BOX(hbox));
      lives_widget_set_sensitive_with(cbutton, spinbutton_alpha);
      lives_widget_set_show_hide_with(cbutton, spinbutton_alpha);
    }

    if (parent_is_layout) {
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
      hbox = make_inner_hbox(LIVES_BOX(hbox), TRUE);
    }

    lives_container_add(LIVES_CONTAINER(frame), cbutton);
    lives_box_pack_start(LIVES_BOX(hbox), frame, TRUE, FALSE, packing_width * 2.);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), SPRED_KEY, spinbutton_red);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), SPGREEN_KEY, spinbutton_green);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), SPBLUE_KEY, spinbutton_blue);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), SPALPHA_KEY, spinbutton_alpha);

    lives_widget_set_show_hide_parent(cbutton);

    if (widget_opts.apply_theme) {
      lives_widget_set_padding(cbutton, 0);
    }

    if (widget_opts.swap_label) {
      if (labelcname) {
        if (parent_is_layout) {
          hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
          widget_opts.justify = LIVES_JUSTIFY_START;
        }
        if (LIVES_SHOULD_EXPAND_WIDTH) lives_widget_set_margin_right(labelcname, widget_opts.packing_width >> 2);
        lives_box_pack_start(LIVES_BOX(hbox), labelcname, FALSE, FALSE, widget_opts.packing_width);
      }
    }
  }

  if (parent_is_layout) {
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cbutton), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(on_pwcolselx), NULL);

  widget_opts.last_label = labelcname;
  return cbutton;
}


// utils

#if GTK_CHECK_VERSION(3, 10, 0)

static const char *LIVES_STOCK_ALTS[N_STOCK_ALTS];

const char *lives_get_stock_icon_alt(int alt_stock_id) {
  return LIVES_STOCK_ALTS[alt_stock_id];
}

static const char *lives_icon_get_stock_alt(LiVESIconTheme * icon_theme, const char *str,  ...) LIVES_SENTINEL;
static const char *lives_icon_get_stock_alt(LiVESIconTheme * icon_theme, const char *str, ...) {
  va_list xargs;
  va_start(xargs, str);
  for (; str; str++) {
    if (lives_has_icon(icon_theme, str, LIVES_ICON_SIZE_BUTTON)) break;
  }
  va_end(xargs);
  return str;
}
#endif


void widget_helper_set_stock_icon_alts(LiVESIconTheme * icon_theme) {
#if GTK_CHECK_VERSION(3, 10, 0)
  LIVES_STOCK_ALTS[STOCK_ALTS_MEDIA_PAUSE] =
    lives_icon_get_stock_alt(icon_theme, LIVES_STOCK_MEDIA_PAUSE_ALT_1, LIVES_STOCK_MEDIA_PAUSE_ALT_2, (char *)NULL);
  LIVES_STOCK_ALTS[STOCK_ALTS_KEEP] =
    lives_icon_get_stock_alt(icon_theme, LIVES_STOCK_KEEP_ALT_1, LIVES_STOCK_KEEP_ALT_2, (char *)NULL);
#endif
}


const char *widget_helper_suggest_icons(const char *part, int idx) {
  LiVESList *list = capable->all_icons;
  const char *iname = NULL;
  //boolean found = FALSE;
  if (!list || !part) return NULL;
#ifdef GUI_GTK
  // prefer icons starting with gtk-
  for (; list; list = list->next) {
    iname = (const char *)list->data;
    if (strncmp(iname, "gtk-", 4) && strstr(iname, part)) {
      if (!idx--) break;
      //g_print("suggest for %s icon: %s\n", part, iname);
      //found = TRUE;
    }
  }
  list = capable->all_icons;
#endif
  if (idx >= 0) {
    for (; list; list = list->next) {
      if (strstr((iname = (const char *)list->data), part)) {
        if (!idx--) break;
        //g_print("suggest for %s icon: %s\n", part, iname);
        //found = TRUE;
      }
    }
  }
  //if (!found) g_print("No suitable icons match %s\n", part);
  return iname;
}


LiVESWidget *lives_image_find_in_stock(LiVESIconSize size, ...) {
  va_list ap;
  const char *iname;
  char *match;
  va_start(ap, size);
  while ((match = va_arg(ap, char *))) {
    iname = widget_helper_suggest_icons(match, 0);
    if (iname) break;
  }
  va_end(ap);
  if (iname) return lives_image_new_from_stock(iname, size);
  return NULL;
}


LiVESWidget *lives_image_find_in_stock_at_size(int size, ...) {
  va_list ap;
  const char *iname;
  char *match;
  va_start(ap, size);
  while ((match = va_arg(ap, char *))) {
    iname = widget_helper_suggest_icons(match, 0);
    if (iname) break;
  }
  va_end(ap);
  if (iname) return lives_image_new_from_stock_at_size(iname, LIVES_ICON_SIZE_CUSTOM, size);
  return NULL;
}


boolean widget_helper_init(void) {
#ifdef GUI_GTK
  GSList *flist, *slist;
#endif
  LiVESList *dlist, *xlist = NULL;
  int i;

  lives_snprintf(LIVES_STOCK_LABEL_CANCEL, 32, "%s", (_("_Cancel")));
  lives_snprintf(LIVES_STOCK_LABEL_OK, 32, "%s", (_("_OK")));
  lives_snprintf(LIVES_STOCK_LABEL_YES, 32, "%s", (_("_Yes")));
  lives_snprintf(LIVES_STOCK_LABEL_NO, 32, "%s", (_("_No")));
  lives_snprintf(LIVES_STOCK_LABEL_SAVE, 32, "%s", (_("_Save")));
  lives_snprintf(LIVES_STOCK_LABEL_SAVE_AS, 32, "%s", (_("Save _As")));
  lives_snprintf(LIVES_STOCK_LABEL_OPEN, 32, "%s", (_("_Open")));
  lives_snprintf(LIVES_STOCK_LABEL_QUIT, 32, "%s", (_("_Quit")));
  lives_snprintf(LIVES_STOCK_LABEL_APPLY, 32, "%s", (_("_Apply")));
  lives_snprintf(LIVES_STOCK_LABEL_CLOSE, 32, "%s", (_("_Close")));
  lives_snprintf(LIVES_STOCK_LABEL_REVERT, 32, "%s", (_("_Revert")));
  lives_snprintf(LIVES_STOCK_LABEL_REFRESH, 32, "%s", (_("_Refresh")));
  lives_snprintf(LIVES_STOCK_LABEL_DELETE, 32, "%s", (_("_Delete")));
  lives_snprintf(LIVES_STOCK_LABEL_GO_FORWARD, 32, "%s", (_("_Forward")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_FORWARD, 32, "%s", (_("R_ewind")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_REWIND, 32, "%s", (_("_Forward")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_PLAY, 32, "%s", (_("_Play")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_PAUSE, 32, "%s", (_("P_ause")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_STOP, 32, "%s", (_("_Stop")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_RECORD, 32, "%s", (_("_Record")));
  lives_snprintf(LIVES_STOCK_LABEL_SELECT_ALL, 32, "%s", (_("_Select All")));

  // non-standard
  lives_snprintf(LIVES_STOCK_LABEL_CLOSE_WINDOW, 32, "%s", (_("_Close Window")));
  lives_snprintf(LIVES_STOCK_LABEL_SKIP, 32, "%s", (_("_Skip")));
  lives_snprintf(LIVES_STOCK_LABEL_ABORT, 32, "%s", (_("_Abort")));
  lives_snprintf(LIVES_STOCK_LABEL_BROWSE, 32, "%s", (_("B_rowse")));
  lives_snprintf(LIVES_STOCK_LABEL_SELECT, 32, "%s", (_("_Select")));
  lives_snprintf(LIVES_STOCK_LABEL_BACK, 32, "%s", (_("_Back")));
  lives_snprintf(LIVES_STOCK_LABEL_NEXT, 32, "%s", (_("_Next")));
  lives_snprintf(LIVES_STOCK_LABEL_RETRY, 32, "%s", (_("Re_try")));
  lives_snprintf(LIVES_STOCK_LABEL_RESET, 32, "%s", (_("_Reset")));

  def_widget_opts = _def_widget_opts;
  lives_memcpy(&widget_opts, &def_widget_opts, sizeof(widget_opts_t));

#ifdef GUI_GTK
  gtk_accel_map_add_entry("<LiVES>/save", LIVES_KEY_s, LIVES_CONTROL_MASK);
  gtk_accel_map_add_entry("<LiVES>/quit", LIVES_KEY_q, LIVES_CONTROL_MASK);

  slist = flist = gdk_pixbuf_get_formats();
  while (slist) {
    GdkPixbufFormat *form = (GdkPixbufFormat *)slist->data;
    char **ext = gdk_pixbuf_format_get_extensions(form);
    for (i = 0; ext[i]; i++) {
      xlist = lives_list_append_unique_str(xlist, lives_strdup(ext[i]));
    }
    lives_strfreev(ext);
    slist = slist->next;
  }
  g_slist_free(flist);
#endif

  if (xlist) {
    dlist = xlist;
    widget_opts.image_filter = (char **)lives_malloc((lives_list_length(xlist) + 1) * sizeof(char *));
    for (i = 0; dlist; i++) {
      widget_opts.image_filter[i] = lives_strdup_printf("*.%s", (char *)dlist->data);
      dlist = dlist->next;
    }
    widget_opts.image_filter[i] = NULL;
    lives_list_free_all(&xlist);
  }
  return TRUE;
}


boolean widget_opts_set_scale(double scale) {
  double ar = find_nearest_ar(GUI_SCREEN_WIDTH, GUI_SCREEN_HEIGHT, NULL, NULL);
  widget_opts.scaleW = widget_opts.scaleH = scale;
  // hs = ws * 4/3 / ar
  widget_opts.scaleH *= 4. / (3. * ar);

  if (def_widget_opts.css_min_width != -1) {
    widget_opts.css_min_width = ALIGN_CEIL((int)((double)def_widget_opts.css_min_width * widget_opts.scaleW + .5), 2);
  }
  if (def_widget_opts.css_min_height != -1) {
    widget_opts.css_min_height = ALIGN_CEIL((int)((double)def_widget_opts.css_min_height * widget_opts.scaleH + .5), 2);
  }
  widget_opts.border_width = ALIGN_CEIL((int)((double)def_widget_opts.border_width * widget_opts.scaleW + .5), 2);
  widget_opts.packing_width = ALIGN_CEIL((int)((double)def_widget_opts.packing_width * widget_opts.scaleW + .5), 2);
  widget_opts.packing_height = ALIGN_CEIL((int)((double)def_widget_opts.packing_height * widget_opts.scaleH + .5), 2);
  widget_opts.filler_len = ALIGN_CEIL((int)((double)def_widget_opts.filler_len * widget_opts.scaleW + .5), 2);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE void lives_widget_queue_draw_if_visible(LiVESWidget * widget) {
  if (widget && LIVES_IS_WIDGET(widget) && gtk_widget_is_drawable(widget)) {
    if (lives_widget_is_visible(widget)) {
      lives_widget_queue_draw(widget);
    }
  }
}


int lives_utf8_strcmpfunc(livesconstpointer a, livesconstpointer b, livespointer fwd) {
  // do not inline !
  int ret;
  char *tmp1 = NULL, *tmp2 = NULL;

  if (LIVES_POINTER_TO_INT(fwd)) {
    if (*(char *)a == '!' && *(char *)b != '!') ret = -1;
    else if (*(char *)b == '!' && *(char *)a != '!') ret = 1;
    else
      ret = lives_strcmp_ordered((tmp1 = lives_utf8_collate_key(a, -1)),
                                 (tmp2 = lives_utf8_collate_key(b, -1)));
  } else {
    if (*(char *)a == '!' && *(char *)b != '!') ret = 1;
    else if (*(char *)b == '!' && *(char *)a != '!') ret = -1;
    else ret = lives_strcmp_ordered((tmp1 = lives_utf8_collate_key(b, -1)),
                                      (tmp2 = lives_utf8_collate_key(a, -1)));
  }
  if (tmp1) lives_free(tmp1);
  if (tmp2) lives_free(tmp2);
  return ret;
}


static int lives_utf8_menu_strcmpfunc(livesconstpointer a, livesconstpointer b, livespointer fwd) {
  return lives_utf8_strcmpfunc(lives_menu_item_get_text((LiVESWidget *)a), lives_menu_item_get_text((LiVESWidget *)b), fwd);
}


WIDGET_HELPER_LOCAL_INLINE LiVESList *lives_menu_list_sort_alpha(LiVESList * list, boolean fwd) {
  return lives_list_sort_with_data(list, lives_utf8_menu_strcmpfunc, LIVES_INT_TO_POINTER(fwd));
}


LiVESList *add_sorted_list_to_menu(LiVESMenu * menu, LiVESList * menu_list) {
  LiVESList **seclist;
  LiVESList *xmenu_list = menu_list = lives_menu_list_sort_alpha(menu_list, TRUE);
  while (menu_list) {
    if (!(LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menu_list->data), HIDDEN_KEY)))) {
      lives_container_add(LIVES_CONTAINER(menu), (LiVESWidget *)menu_list->data);
    }
    if ((seclist = (LiVESList **)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menu_list->data), SECLIST_KEY)) != NULL)
      * seclist = lives_list_prepend(*seclist, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menu_list->data),
                                     SECLIST_VAL_KEY));
    menu_list = menu_list->next;
  }
  return xmenu_list;
}


boolean lives_has_icon(LiVESIconTheme * icon_theme, const char *stock_id, LiVESIconSize size)  {
  boolean has_icon = FALSE;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  GtkIconInfo *iset = gtk_icon_theme_lookup_icon(icon_theme, stock_id, size, 0);
#else
  GtkIconSet *iset = gtk_icon_factory_lookup_default(stock_id);
#endif
  has_icon = (iset != NULL);
#endif
  return has_icon;
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGB48_t *lives_painter_set_source_rgb_from_lives_rgb(lives_painter_t *cr,
    lives_colRGB48_t *col) {
  lives_painter_set_source_rgb(cr, (double)col->red / 65535., (double)col->green / 65535.,
                               (double)col->blue / 65535.);
  return col;
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGBA64_t *lives_painter_set_source_rgb_from_lives_rgba(lives_painter_t *cr,
    lives_colRGBA64_t *col) {
  lives_painter_set_source_rgb(cr, (double)col->red / 65535., (double)col->green / 65535., (double)col->blue / 65535.);
  return col;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetColor *lives_painter_set_source_rgb_from_lives_widget_color(lives_painter_t *cr,
    LiVESWidgetColor * wcol) {
  lives_colRGBA64_t col;
  widget_color_to_lives_rgba(&col, wcol);
  lives_painter_set_source_rgb_from_lives_rgba(cr, &col);
  return wcol;
}


WIDGET_HELPER_GLOBAL_INLINE boolean clear_widget_bg(LiVESWidget * widget, lives_painter_surface_t *s) {
  lives_painter_t *cr;
  if (!s) return FALSE;
  if (!(cr = lives_painter_create_from_surface(s))) return FALSE;
  else {
    int rwidth = lives_widget_get_allocation_width(LIVES_WIDGET(widget));
    int rheight = lives_widget_get_allocation_height(LIVES_WIDGET(widget));
    lives_painter_render_background(widget, cr, 0., 0., rwidth, rheight);
    lives_painter_destroy(cr);
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean clear_widget_bg_area(LiVESWidget * widget, lives_painter_surface_t *s,
    double x, double y, double width, double height) {
  lives_painter_t *cr;
  if (!s) return FALSE;
  if (!(cr = lives_painter_create_from_surface(s))) return FALSE;
  else {
    int rwidth = lives_widget_get_allocation_width(LIVES_WIDGET(widget));
    int rheight = lives_widget_get_allocation_height(LIVES_WIDGET(widget));
    if (width <= 0.) width = rwidth;
    if (height <= 0.) height = rheight;
    lives_painter_render_background(widget, cr, x, y, width, height);
    lives_painter_destroy(cr);
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_cursor_unref(LiVESXCursor * cursor) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  g_object_unref(LIVES_GUI_OBJECT(cursor));
#else
  gdk_cursor_unref(cursor);
  return TRUE;
#endif
#endif
  return FALSE;
}


void lives_widget_apply_theme(LiVESWidget * widget, LiVESWidgetState state) {
  if (!palette || !widget_opts.apply_theme) return;
  lives_widget_set_fg_color(widget, state, &palette->normal_fore);
  lives_widget_set_bg_color(widget, state, &palette->normal_back);
#if GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_set_base_color(widget, state, &palette->normal_back);
  lives_widget_set_text_color(widget, state, &palette->normal_fore);
#endif
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), THEME_KEY,
                               LIVES_INT_TO_POINTER(widget_opts.apply_theme));
}


void lives_widget_apply_theme2(LiVESWidget * widget, LiVESWidgetState state, boolean set_fg) {
  if (!widget_opts.apply_theme) {
    lives_widget_set_fg_color(widget, state, &palette->normal_fore);
    lives_widget_set_bg_color(widget, state, &palette->normal_back);
    return;
  }
  if (set_fg)
    lives_widget_set_fg_color(widget, state, &palette->menu_and_bars_fore);
  lives_widget_set_bg_color(widget, state, &palette->menu_and_bars);
}


void lives_widget_apply_theme3(LiVESWidget * widget, LiVESWidgetState state) {
  if (!widget_opts.apply_theme) {
    lives_widget_set_fg_color(widget, state, &palette->normal_fore);
    lives_widget_set_bg_color(widget, state, &palette->normal_back);
  } else {
    lives_widget_set_text_color(widget, state, &palette->info_text);
    lives_widget_set_base_color(widget, state, &palette->info_base);
    lives_widget_set_fg_color(widget, state, &palette->info_text);
    lives_widget_set_bg_color(widget, state, &palette->info_base);
  }
}


void lives_widget_apply_theme_dimmed(LiVESWidget * widget, LiVESWidgetState state, int dimval) {
  if (widget_opts.apply_theme) {
    LiVESWidgetColor dimmed_fg;
    lives_widget_color_copy(&dimmed_fg, &palette->normal_fore);
    lives_widget_color_mix(&dimmed_fg, &palette->normal_back, (float)dimval / 65535.);
    lives_widget_set_fg_color(widget, state, &dimmed_fg);
    lives_widget_set_bg_color(widget, state, &palette->normal_back);
  }
}


void lives_widget_apply_theme_dimmed2(LiVESWidget * widget, LiVESWidgetState state, int dimval) {
  if (widget_opts.apply_theme) {
    LiVESWidgetColor dimmed_fg;
    lives_widget_color_copy(&dimmed_fg, &palette->menu_and_bars_fore);
    lives_widget_color_mix(&dimmed_fg, &palette->menu_and_bars, (float)dimval / 65535.);
    lives_widget_set_fg_color(widget, state, &dimmed_fg);
    lives_widget_set_bg_color(widget, state, &palette->menu_and_bars);
  }
}


boolean lives_entry_set_completion_from_list(LiVESEntry * entry, LiVESList * xlist) {
#ifdef GUI_GTK
  GtkListStore *store;
  LiVESEntryCompletion *completion;
  store = gtk_list_store_new(1, LIVES_COL_TYPE_STRING);

  while (xlist) {
    LiVESTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, (char *)xlist->data, -1);
    xlist = xlist->next;
  }

  completion = gtk_entry_completion_new();
  gtk_entry_completion_set_model(completion, (GtkTreeModel *)store);
  gtk_entry_completion_set_text_column(completion, 0);
  gtk_entry_completion_set_inline_completion(completion, TRUE);
  gtk_entry_completion_set_popup_set_width(completion, TRUE);
  gtk_entry_completion_set_popup_completion(completion, TRUE);
  gtk_entry_completion_set_popup_single_match(completion, FALSE);
  gtk_entry_set_completion(entry, completion);
  return TRUE;
#endif
  return FALSE;
}


boolean lives_window_center(LiVESWindow * window) {
  if (!widget_opts.no_gui) {
    int xcen, ycen;
    int width, height;
    int bx, by;

    lives_window_set_monitor(LIVES_WINDOW(window), widget_opts.monitor);

    if (!mainw->mgeom) {
      lives_widget_show(LIVES_WIDGET(window));
      lives_window_set_position(LIVES_WINDOW(window), LIVES_WIN_POS_CENTER_ALWAYS);
      return TRUE;
    }

    lives_window_set_position(LIVES_WINDOW(window), LIVES_WIN_POS_CENTER_ALWAYS);

    width = lives_widget_get_allocation_width(LIVES_WIDGET(window));

    if (width == 0) width = MIN_MSGBOX_WIDTH;
    height = lives_widget_get_allocation_height(LIVES_WIDGET(window));

    get_border_size(LIVES_WIDGET(window), &bx, &by);
    width += abs(bx);
    height += abs(by);

    xcen = mainw->mgeom[widget_opts.monitor].x + ((mainw->mgeom[widget_opts.monitor].width - width) >> 1);
    ycen = mainw->mgeom[widget_opts.monitor].y + ((mainw->mgeom[widget_opts.monitor].height - height) >> 1);
    lives_window_move(LIVES_WINDOW(window), xcen, ycen);
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_uncenter(LiVESWindow * window) {
  lives_window_set_position(LIVES_WINDOW(window), LIVES_WIN_POS_NONE);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_fg_color(LiVESWidget * widget, LiVESWidgetColor * color) {
  return lives_widget_get_fg_state_color(widget, LIVES_WIDGET_STATE_NORMAL, color);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_unparent(LiVESWidget * widget) {
  return lives_container_remove(LIVES_CONTAINER(lives_widget_get_parent(widget)), widget);
}


static void _toggle_if_condmet(LiVESWidget * tbut, livespointer widget, boolean cond, const char *type) {
  // cond_func points to a function. int (*cond)(void *user_data)

  /// IF cond is TRUE, then this is the pointer stored in a key like
  // "<pwidget>_visi_cond_f" and "<pwidget>_visi_cond_data",
  // where <pwidget> is a pointer to widget (the target of the condition) converted to a number
  //    (<pwidget>_sens_cond_f, <pwidget>_act_cond_f, etc)
  // these keys are created indirectly in the toggle button, when the relationship is set up
  // (e.g by calling toggle_sets_visible(toggle, widget, inverted)
  //
  // if cond is FALSE then <pwidget>_invisi_cond_f and <pwidget>_invisi_cond_data
  // would eb used instead
  //
  // if the value return calling func is 0 (FALSE) then no change is made

  // e.g to make widget invisible then cond must be FALSE, thus the value returned from
  // <pwidget>_invis_cond_f(<pwidget>_invis_cond_data) must be non-zero (e.g. TRUE)
  // otherwise there would be no effect
  //
  // then for example, when toggle_sets_visible(toggle, widget, invert) is called,
  // callbacks are added such that this function is called when the active state of toggle changes,
  // if invert is FALSE, condx is derived from the active state of toggle
  // if invert is TRUE then cond is the inverted state
  // so  e.g if invert is FALSE and active is TRUE, then we check if the positive variable is TRUE
  // however, in this case the function does not set any conditions, so the match is automatic.
  //
  // the more useful case is when  we want to check for alternate conditions than just the active state
  // instead of calling toggle_sets_visible, we can call toggle_sets_visible_cond,
  // and provide pointer(s) to functions whose return value determines if the target is made visible
  // when the toggle goes on (active), and if it is made invisible when the toggle goes off
  // either condition may be NULL, and then the action happens automatically
  //
  // toggle_sets_invisible_cond is similar but inverts the state of toggle
  // however, the condition checks are still the same, the positive condition function must still
  // return non-zero to make the target visible when the toggle goes inactive
  // and the negative must be non zero to make the target invisible when toggle goes active
  //
  /// eg:  toggle_sets_sensitive_cond(toggle, radiobutton, ret_int, &cfile->frames, NULL, NULL, FALSE);
  // assuming ret_int is a funciton that casts its data to (int *) and returns the value pointed to,
  // - the effect of this is that when toggle becomes active, radiobutton is set sensitive,
  // but only if cfile->frames is non-zero. In any other case radiobutton is left inactive

  weed_plant_t *cond_plant;

  if ((cond_plant = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tbut), COND_PLANT_KEY))) {
    condfuncptr_t func;
    void *data;
    char *keyvalf, *keyvald;
    boolean condx = TRUE;

    if (!cond) {
      keyvalf = lives_strdup_printf("%p_in%s_cond_f", widget, type);
      keyvald = lives_strdup_printf("%p_in%s_cond_data", widget, type);
      func = (condfuncptr_t)weed_get_funcptr_value(cond_plant, keyvalf, NULL);
      if (func) {
        data = weed_get_voidptr_value(cond_plant, keyvald, NULL);
        condx = (*func)(data);
      }
    } else {
      keyvalf = lives_strdup_printf("%p_%s_cond_f", widget, type);
      keyvald = lives_strdup_printf("%p_%s_cond_data", widget, type);
      func = (condfuncptr_t)weed_get_funcptr_value(cond_plant, keyvalf, NULL);
      if (func) {
        data = weed_get_voidptr_value(cond_plant, keyvald, NULL);
        condx = (*func)(data);
      }
    }
    lives_free(keyvalf);
    lives_free(keyvald);
    if (!condx) return;
  }
  if (!strcmp(type, "sens"))
    lives_widget_set_sensitive(LIVES_WIDGET(widget), cond);
  else if (!strcmp(type, "act"))
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(widget), cond);
  else if (!strcmp(type, "visi")) {
    if (cond) lives_widget_show(LIVES_WIDGET(widget));
    else lives_widget_hide(LIVES_WIDGET(widget));
  } else if (!strcmp(type, "show_warn")) {
    if (cond) show_warn_image(LIVES_WIDGET(widget), NULL);
    else hide_warn_image(LIVES_WIDGET(widget));
  }
}

static void toggle_show_warn_img(LiVESWidget * tbut, livespointer widget) {
  if (LIVES_IS_TOGGLE_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)), "show_warn");
}

static void toggle_hide_warn_img(LiVESWidget * tbut, livespointer widget) {
  if (LIVES_IS_TOGGLE_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)), "show_warn");
}

static void toggle_set_sensitive(LiVESWidget * tbut, livespointer widget) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(tbut))) return;
  if (LIVES_IS_TOGGLE_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)), "sens");
  else if (LIVES_IS_TOGGLE_TOOL_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbut)),
                       "sens");
  else if (LIVES_IS_CHECK_MENU_ITEM(tbut))
    _toggle_if_condmet(tbut, widget, lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(tbut)),
                       "sens");
}

static void toggle_set_insensitive(LiVESWidget * tbut, livespointer widget) {
  if (LIVES_IS_TOGGLE_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)),
                       "sens");
  if (LIVES_IS_TOGGLE_TOOL_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, !lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbut)),
                       "sens");
  else if (LIVES_IS_CHECK_MENU_ITEM(tbut))
    _toggle_if_condmet(tbut, widget, !lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(tbut)),
                       "sens");
}

static void toggle_set_visible(LiVESWidget * tbut, livespointer widget) {
  if (LIVES_IS_TOGGLE_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)), "visi");
  else if (LIVES_IS_TOGGLE_TOOL_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbut)),
                       "visi");
  else if (LIVES_IS_CHECK_MENU_ITEM(tbut))
    _toggle_if_condmet(tbut, widget, lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(tbut)),
                       "visi");
}

static void toggle_set_invisible(LiVESWidget * tbut, livespointer widget) {
  if (LIVES_IS_TOGGLE_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)),
                       "visi");
  else if (LIVES_IS_TOGGLE_TOOL_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, !lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbut)),
                       "visi");
  else if (LIVES_IS_CHECK_MENU_ITEM(tbut))
    _toggle_if_condmet(tbut, widget, !lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(tbut)),
                       "visi");
}

static void toggle_set_active(LiVESWidget * tbut, livespointer widget) {
  if (LIVES_IS_TOGGLE_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)), "act");
  else if (LIVES_IS_TOGGLE_TOOL_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbut)),
                       "act");
  else if (LIVES_IS_CHECK_MENU_ITEM(tbut))
    _toggle_if_condmet(tbut, widget, lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(tbut)),
                       "act");
}

static void toggle_set_inactive(LiVESWidget * tbut, livespointer widget) {
  if (LIVES_IS_TOGGLE_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)),
                       "act");
  else if (LIVES_IS_TOGGLE_TOOL_BUTTON(tbut))
    _toggle_if_condmet(tbut, widget, !lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbut)),
                       "act");
  else if (LIVES_IS_CHECK_MENU_ITEM(tbut))
    _toggle_if_condmet(tbut, widget, !lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(tbut)),
                       "act");
}

// togglebutton functions

boolean toggle_shows_warn_cond(LiVESWidget * tb, LiVESWidget * widget,
                               condfuncptr_t condshow_f, void *condshow_data,
                               condfuncptr_t condhide_f, void *condhide_data,
                               boolean invert) {
  if (condshow_f || condhide_f) {
    weed_plant_t *cond_plant =
      lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tb), COND_PLANT_KEY);
    if (!cond_plant) {
      cond_plant = lives_plant_new(LIVES_WEED_SUBTYPE_BAG_OF_HOLDING);
      lives_widget_object_set_data_plantptr(LIVES_WIDGET_OBJECT(tb), COND_PLANT_KEY,
                                            cond_plant);
    }
    if (condshow_f) {
      /// set sensitive only if *condsens > 0
      char *keyvalf = lives_strdup_printf("%p_shwarn_cond_f", widget);
      char *keyvald = lives_strdup_printf("%p_shwarn_cond_data", widget);
      weed_set_funcptr_value(cond_plant, keyvalf, (weed_funcptr_t)condshow_f);
      weed_set_voidptr_value(cond_plant, keyvald, condshow_data);
      lives_free(keyvalf);
      lives_free(keyvald);
    }
    if (condhide_f) {
      /// set sensitive only if *condsens > 0
      char *keyvalf = lives_strdup_printf("%p_inshwarn_cond_f", widget);
      char *keyvald = lives_strdup_printf("%p_inshwarn_cond_data", widget);
      weed_set_funcptr_value(cond_plant, keyvalf, (weed_funcptr_t)condhide_f);
      weed_set_voidptr_value(cond_plant, keyvald, condhide_data);
      lives_free(keyvalf);
      lives_free(keyvald);
    }
  }

  if (!invert) {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(toggle_show_warn_img),
                                    (livespointer)widget);
    toggle_show_warn_img(LIVES_WIDGET(tb), (livespointer)widget);
  } else {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(toggle_hide_warn_img),
                                    (livespointer)widget);
    toggle_hide_warn_img(tb, (livespointer)widget);
  }
  return TRUE;
}


boolean toggle_sets_sensitive_cond(LiVESWidget * tb, LiVESWidget * widget,
                                   condfuncptr_t condsens_f, void *condsens_data,
                                   condfuncptr_t condinsens_f, void *condinsens_data,
                                   boolean invert) {
  if (condsens_f || condinsens_f) {
    weed_plant_t *cond_plant =
      lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tb), COND_PLANT_KEY);
    if (!cond_plant) {
      cond_plant = lives_plant_new(LIVES_WEED_SUBTYPE_BAG_OF_HOLDING);
      lives_widget_object_set_data_plantptr(LIVES_WIDGET_OBJECT(tb), COND_PLANT_KEY,
                                            cond_plant);
    }
    if (condsens_f) {
      /// set sensitive only if *condsens > 0
      char *keyvalf = lives_strdup_printf("%p_sens_cond_f", widget);
      char *keyvald = lives_strdup_printf("%p_sens_cond_data", widget);
      weed_set_funcptr_value(cond_plant, keyvalf, (weed_funcptr_t)condsens_f);
      weed_set_voidptr_value(cond_plant, keyvald, condsens_data);
      lives_free(keyvalf);
      lives_free(keyvald);
    }
    if (condinsens_f) {
      /// set sensitive only if *condsens > 0
      char *keyvalf = lives_strdup_printf("%p_insens_cond_f", widget);
      char *keyvald = lives_strdup_printf("%p_insens_cond_data", widget);
      weed_set_funcptr_value(cond_plant, keyvalf, (weed_funcptr_t)condinsens_f);
      weed_set_voidptr_value(cond_plant, keyvald, condinsens_data);
      lives_free(keyvalf);
      lives_free(keyvald);
    }
  }

  if (!invert) {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(toggle_set_sensitive),
                                    (livespointer)widget);
    toggle_set_sensitive(tb, (livespointer)widget);
  } else {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(toggle_set_insensitive),
                                    (livespointer)widget);
    toggle_set_insensitive(tb, (livespointer)widget);
  }
  return TRUE;
}


boolean toggle_sets_visible_cond(LiVESWidget * tb, LiVESWidget * widget,
                                 condfuncptr_t condvisi_f, void *condvisi_data,
                                 condfuncptr_t condinvisi_f, void *condinvisi_data,
                                 boolean invert) {
  if (condvisi_f || condinvisi_f) {
    weed_plant_t *cond_plant =
      lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tb), COND_PLANT_KEY);
    if (!cond_plant) {
      cond_plant = lives_plant_new(LIVES_WEED_SUBTYPE_BAG_OF_HOLDING);
      lives_widget_object_set_data_plantptr(LIVES_WIDGET_OBJECT(tb), COND_PLANT_KEY,
                                            cond_plant);
    }
    if (condvisi_f) {
      /// set sensitive only if *condsens > 0
      char *keyvalf = lives_strdup_printf("%p_visi_cond_f", widget);
      char *keyvald = lives_strdup_printf("%p_visi_cond_data", widget);
      weed_set_funcptr_value(cond_plant, keyvalf, (weed_funcptr_t)condvisi_f);
      weed_set_voidptr_value(cond_plant, keyvald, condvisi_data);
      lives_free(keyvalf);
      lives_free(keyvald);
    }
    if (condinvisi_f) {
      /// set sensitive only if *condsens > 0
      char *keyvalf = lives_strdup_printf("%p_invisi_cond_f", widget);
      char *keyvald = lives_strdup_printf("%p_invisi_cond_data", widget);
      weed_set_funcptr_value(cond_plant, keyvalf, (weed_funcptr_t)condinvisi_f);
      weed_set_voidptr_value(cond_plant, keyvald, condinvisi_data);
      lives_free(keyvalf);
      lives_free(keyvald);
    }
  }

  if (!invert) {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(toggle_set_visible),
                                    (livespointer)widget);
    toggle_set_visible(tb, (livespointer)widget);
  } else {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(toggle_set_invisible),
                                    (livespointer)widget);
    toggle_set_invisible(tb, (livespointer)widget);
  }
  return TRUE;
}

boolean toggle_sets_active_cond(LiVESWidget * tb, LiVESWidget * widget,
                                condfuncptr_t condact_f, void *condact_data,
                                condfuncptr_t condinact_f, void *condinact_data,
                                boolean invert) {
  if (condact_f || condinact_f) {
    weed_plant_t *cond_plant =
      lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tb), COND_PLANT_KEY);
    if (!cond_plant) {
      cond_plant = lives_plant_new(LIVES_WEED_SUBTYPE_BAG_OF_HOLDING);
      lives_widget_object_set_data_plantptr(LIVES_WIDGET_OBJECT(tb), COND_PLANT_KEY,
                                            cond_plant);
    }
    if (condact_f) {
      /// set sensitive only if *condsens > 0
      char *keyvalf = lives_strdup_printf("%p_act_cond_f", widget);
      char *keyvald = lives_strdup_printf("%p_act_cond_data", widget);
      weed_set_funcptr_value(cond_plant, keyvalf, (weed_funcptr_t)condact_f);
      weed_set_voidptr_value(cond_plant, keyvald, condact_data);
      lives_free(keyvalf);
      lives_free(keyvald);
    }
    if (condinact_f) {
      /// set sensitive only if *condsens > 0
      char *keyvalf = lives_strdup_printf("%p_inact_cond_f", widget);
      char *keyvald = lives_strdup_printf("%p_inact_cond_data", widget);
      weed_set_funcptr_value(cond_plant, keyvalf, (weed_funcptr_t)condinact_f);
      weed_set_voidptr_value(cond_plant, keyvald, condinact_data);
      lives_free(keyvalf);
      lives_free(keyvald);
    }
  }

  if (!invert) {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(toggle_set_active),
                                    (livespointer)widget);
    toggle_set_active(tb, (livespointer)widget);
  } else {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(toggle_set_inactive),
                                    (livespointer)widget);
    toggle_set_inactive(tb, (livespointer)widget);
  }
  return TRUE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean toggle_shows_warn_img(LiVESToggleButton * tb, LiVESWidget * widget,
    boolean invert) {
  return toggle_shows_warn_cond(LIVES_WIDGET(tb), widget, NULL, NULL, NULL, NULL, invert);
}
WIDGET_HELPER_GLOBAL_INLINE boolean toggle_sets_sensitive(LiVESToggleButton * tb, LiVESWidget * widget,
    boolean invert) {
  return toggle_sets_sensitive_cond(LIVES_WIDGET(tb), widget, NULL, NULL, NULL, NULL, invert);
}
WIDGET_HELPER_GLOBAL_INLINE boolean toggle_toolbutton_sets_sensitive(LiVESToggleToolButton * ttb, LiVESWidget * widget,
    boolean invert) {
  return toggle_sets_sensitive_cond(LIVES_WIDGET(ttb), widget, NULL, NULL, NULL, NULL, invert);
}
WIDGET_HELPER_GLOBAL_INLINE boolean menu_sets_sensitive(LiVESCheckMenuItem * mi, LiVESWidget * widget,
    boolean invert) {
  return toggle_sets_sensitive_cond(LIVES_WIDGET(mi), widget, NULL, NULL, NULL, NULL, invert);
}

WIDGET_HELPER_GLOBAL_INLINE boolean toggle_sets_visible(LiVESToggleButton * tb, LiVESWidget * widget,
    boolean invert) {
  return toggle_sets_visible_cond(LIVES_WIDGET(tb), widget, NULL, NULL, NULL, NULL, invert);
}
WIDGET_HELPER_GLOBAL_INLINE boolean toggle_toolbutton_sets_visible(LiVESToggleToolButton * ttb, LiVESWidget * widget,
    boolean invert) {
  return toggle_sets_visible_cond(LIVES_WIDGET(ttb), widget, NULL, NULL, NULL, NULL, invert);
}
WIDGET_HELPER_GLOBAL_INLINE boolean menu_sets_visible(LiVESCheckMenuItem * mi, LiVESWidget * widget,
    boolean invert) {
  return toggle_sets_visible_cond(LIVES_WIDGET(mi), widget, NULL, NULL, NULL, NULL, invert);
}

WIDGET_HELPER_GLOBAL_INLINE boolean toggle_sets_active(LiVESToggleButton * tb, LiVESToggleButton * widget,
    boolean invert) {
  return toggle_sets_active_cond(LIVES_WIDGET(tb), widget, NULL, NULL, NULL, NULL, invert);
}

// widget callback sets togglebutton active
boolean widget_act_toggle(LiVESWidget * widget, LiVESWidget * togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  if (LIVES_IS_TOGGLE_TOOL_BUTTON(togglebutton))
    lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(togglebutton), TRUE);
  else
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(togglebutton), TRUE);
  return FALSE;
}


// widget callback sets togglebutton inactive
boolean widget_inact_toggle(LiVESWidget * widget, LiVESWidget * togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  if (LIVES_IS_TOGGLE_TOOL_BUTTON(togglebutton))
    lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(togglebutton), FALSE);
  else
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(togglebutton), FALSE);
  return FALSE;
}


boolean label_act_toggle(LiVESWidget * widget, LiVESXEventButton * event, LiVESWidget * togglebutton) {
  if (mainw && LIVES_IS_PLAYING) return FALSE;
  if (LIVES_IS_TOGGLE_TOOL_BUTTON(togglebutton))
    return lives_toggle_tool_button_toggle(LIVES_TOGGLE_TOOL_BUTTON(togglebutton));
  return lives_toggle_button_toggle(LIVES_TOGGLE_BUTTON(togglebutton));
}


// set callback so that togglebutton controls var
WIDGET_HELPER_GLOBAL_INLINE boolean toggle_toggles_var(LiVESToggleButton * tbut, boolean * var, boolean invert) {
  if (invert) lives_toggle_button_set_active(tbut, !(*var));
  else lives_toggle_button_set_active(tbut, *var);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tbut), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(togglevar_cb), (livespointer)var);
  return TRUE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_toggle(LiVESToggleButton * tbutton) {
  if (lives_toggle_button_get_active(tbutton)) return lives_toggle_button_set_active(tbutton, FALSE);
  else return lives_toggle_button_set_active(tbutton, TRUE);
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_tool_button_toggle(LiVESToggleToolButton * tbutton) {
  if (lives_toggle_tool_button_get_active(tbutton)) return lives_toggle_tool_button_set_active(tbutton, FALSE);
  else return lives_toggle_tool_button_set_active(tbutton, TRUE);
}


WIDGET_HELPER_GLOBAL_INLINE void entry_text_copy(LiVESEntry * e1, LiVESEntry * e2) {
  // convenience callback
  lives_entry_set_text(e2, lives_entry_get_text(e1));
}


WIDGET_HELPER_GLOBAL_INLINE void lives_entries_link(LiVESEntry * from, LiVESEntry * to) {
  lives_signal_sync_connect(LIVES_WIDGET(from), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(entry_text_copy), to);
}


static void _set_tooltips_state(LiVESWidget * widget, livespointer state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  char *ttip;
  if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_OVERRIDE_KEY)) return;

  if (LIVES_POINTER_TO_INT(state)) {
    // enable
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_HIDE_KEY)) {
      if (!lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), SHOWALL_OVERRIDE_KEY)) {
        lives_widget_show(widget);
      }
      return;
    }
    ttip = (char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY);
    if (ttip) {
      lives_widget_set_tooltip_text(widget, ttip);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY, NULL);
    }
  } else {
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_HIDE_KEY)) {
      lives_widget_hide(widget);
      return;
    }
    ttip = gtk_widget_get_tooltip_text(widget);
    lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY, ttip);
    lives_widget_set_tooltip_text(widget, NULL);
  }
  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), _set_tooltips_state, state);
  }
#endif
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean set_tooltips_state(LiVESWidget * widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  _set_tooltips_state(widget, LIVES_INT_TO_POINTER(state));
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_spin_button_get_snapval(LiVESSpinButton * button, double val) {
  double stepval, min, max, nval;
  int digs = gtk_spin_button_get_digits(button);
  boolean wrap = gtk_spin_button_get_wrap(button);
  double tenpow = (double)lives_10pow(digs);
  gtk_spin_button_get_increments(button, &stepval, NULL);
  gtk_spin_button_get_range(button, &min, &max);
  if (val >= 0.) {
    nval = (double)((int64_t)(val * tenpow + .5)) / tenpow;
    if (stepval > 0.) nval = (double)((int64_t)((nval + stepval / 2.) / stepval)) * stepval;
  } else {
    nval = (double)((int64_t)(val * tenpow  - .5)) / tenpow;
    if (stepval > 0.) nval = (double)((int64_t)((nval - stepval / 2.) / stepval)) * stepval;
  }

  if (nval < min) {
    if (wrap && max > min) while (nval < min) nval += (max - min);
    else nval = min;
  }
  if (nval > max) {
    if (wrap && max > min) while (nval > max) nval -= (max - min);
    else nval = max;
  }
  return nval;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_clamp(LiVESSpinButton * button) {
  double val = lives_spin_button_get_value(button);
  lives_spin_button_set_range(button, val, val);
  return TRUE;
}


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
static void dissect_filechooser(LiVESWidget * widget, livespointer user_data) {
  static boolean set_allx = FALSE;
  static boolean set_all2 = FALSE;
  static boolean get_entry  = FALSE;
  boolean set_all = set_allx;
  boolean get_e = get_entry;
  struct fc_dissection *diss = (struct fc_dissection *)user_data;

  if (LIVES_IS_FILE_CHOOSER(widget)) {
    get_entry = set_all2 = set_allx = FALSE;
  }

  if (LIVES_IS_PLACES_SIDEBAR(widget)) {
    diss->sidebar = widget;
    set_allx = TRUE;
  }
  if (GTK_IS_GRID(widget)) {
    get_entry = TRUE;
  } else if (get_entry && LIVES_IS_ENTRY(widget)) {
    diss->entry_list = lives_list_append(diss->entry_list, widget);
  } else if (!lives_strcmp(gtk_widget_get_name(widget), "browser_files_stack")) {
    set_allx = TRUE;
  } else if (LIVES_IS_BUTTON_BOX(widget)) {
    diss->bbox = widget;
  } else if (GTK_IS_LIST_BOX(widget)) {
    diss->treeview = widget;
    set_all2 = TRUE;
  } else if (GTK_IS_REVEALER(widget) && !lives_strcmp(gtk_widget_get_name(widget), "browser_header_revealer")) {
    diss->revealer = widget;
  }

  if (!set_all && LIVES_IS_BUTTON(widget)) return; // avoids a problem with filechooser
  if (set_all2) {
    lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_NORMAL, FALSE);
    lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_SELECTED);
  } else {
    if (set_all || LIVES_IS_LABEL(widget)) {
      lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_NORMAL);
      if (!LIVES_IS_LABEL(widget))
        lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_INSENSITIVE);
    }
  }
  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), dissect_filechooser, user_data);
  }
  set_allx = set_all;
  get_entry = get_e;
}
#endif
#endif

static void set_child_colour_internal(LiVESWidget * widget, livespointer set_allx) {
  boolean set_all = LIVES_POINTER_TO_INT(set_allx);
  if (!set_all && LIVES_IS_BUTTON(widget)) return; // avoids a problem with filechooser
  if (set_all || LIVES_IS_LABEL(widget)) {
    lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_NORMAL);
    if (!LIVES_IS_LABEL(widget))
      lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_INSENSITIVE);
  }
  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_foreach(LIVES_CONTAINER(widget), set_child_colour_internal, set_allx);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void *set_child_colour(LiVESWidget * widget, boolean set_all) {
  // set widget and all children widgets
  // if set_all is FALSE, we only set labels (and ignore labels in buttons)
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  if (LIVES_IS_FILE_CHOOSER(widget)) {
    struct fc_dissection *diss = (struct fc_dissection *)lives_calloc(sizeof(struct fc_dissection), 1);
    dissect_filechooser(widget, diss);
    return diss;
  }
#endif
#endif
  set_child_colour_internal(widget, LIVES_INT_TO_POINTER(set_all));
  return NULL;
}


static void set_child_dimmed_colour_internal(LiVESWidget * widget, livespointer dim) {
  int dimval = LIVES_POINTER_TO_INT(dim);

  lives_widget_apply_theme_dimmed(widget, LIVES_WIDGET_STATE_INSENSITIVE, dimval);
  lives_widget_apply_theme_dimmed(widget, LIVES_WIDGET_STATE_NORMAL, dimval);

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_foreach(LIVES_CONTAINER(widget), set_child_dimmed_colour_internal, dim);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_dimmed_colour(LiVESWidget * widget, int dim) {
  // set widget and all children widgets
  // fg is affected dim value
  // dim takes a value from 0 (full fg) -> 65535 (full bg)
  set_child_dimmed_colour_internal(widget, LIVES_INT_TO_POINTER(dim));
}


static void set_child_dimmed_colour2_internal(LiVESWidget * widget, livespointer dim) {
  int dimval = LIVES_POINTER_TO_INT(dim);

  lives_widget_apply_theme_dimmed2(widget, LIVES_WIDGET_STATE_INSENSITIVE, dimval);

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_foreach(LIVES_CONTAINER(widget), set_child_dimmed_colour2_internal, dim);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_dimmed_colour2(LiVESWidget * widget, int dim) {
  // set widget and all children widgets
  // fg is affected dim value
  // dim takes a value from 0 (full fg) -> 65535 (full bg)
  set_child_dimmed_colour2_internal(widget, LIVES_INT_TO_POINTER(dim));
}


static void set_child_alt_colour_internal(LiVESWidget * widget, livespointer set_allx) {
  boolean set_all = LIVES_POINTER_TO_INT(set_allx);

  if (!set_all && LIVES_IS_BUTTON(widget)) return;

  if (set_all || LIVES_IS_LABEL(widget)) {
    lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_INSENSITIVE, TRUE);
    lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_NORMAL, TRUE),
                              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget),
                                  THEME_KEY, LIVES_INT_TO_POINTER(2));
  }

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_foreach(LIVES_CONTAINER(widget), set_child_alt_colour_internal, set_allx);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_alt_colour(LiVESWidget * widget, boolean set_all) {
  // set widget and all children widgets
  // if set_all is FALSE, we only set labels (and ignore labels in buttons)

  set_child_alt_colour_internal(widget, LIVES_INT_TO_POINTER(set_all));
}


static void set_child_alt_colour_internal_prelight(LiVESWidget * widget, livespointer data) {
  lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_PRELIGHT, TRUE);
  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_foreach(LIVES_CONTAINER(widget), set_child_alt_colour_internal_prelight, NULL);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_alt_colour_prelight(LiVESWidget * widget) {
  // set widget and all children widgets
  // if set_all is FALSE, we only set labels (and ignore labels in buttons)
  set_child_alt_colour_internal_prelight(widget, NULL);
}


static void set_child_colour3_internal(LiVESWidget * widget, livespointer set_allx) {
  boolean set_all = LIVES_POINTER_TO_INT(set_allx);

  if (!set_all && (LIVES_IS_BUTTON(widget))) {// || LIVES_IS_SCROLLBAR(widget))) {
    lives_widget_set_base_color(widget, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_text_color(widget, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
    return;
  }

  if (set_all || LIVES_IS_LABEL(widget)) {
    lives_widget_apply_theme3(widget, LIVES_WIDGET_STATE_NORMAL);
  }

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_foreach(LIVES_CONTAINER(widget), set_child_colour3_internal, set_allx);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_colour3(LiVESWidget * widget, boolean set_all) {
  // set widget and all children widgets
  // if set_all is FALSE, we only set labels (and ignore labels in buttons)

  set_child_colour3_internal(widget, LIVES_INT_TO_POINTER(set_all));
}


WIDGET_HELPER_GLOBAL_INLINE char *lives_text_view_get_text(LiVESTextView * textview) {
  LiVESTextBuffer *textbuf = lives_text_view_get_buffer(textview);
  if (textbuf) {
    LiVESTextIter siter, eiter;
    lives_text_buffer_get_start_iter(textbuf, &siter);
    lives_text_buffer_get_end_iter(textbuf, &eiter);
    return lives_text_buffer_get_text(textbuf, &siter, &eiter, FALSE);
  }
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_text(LiVESTextView * textview, const char *text, int len) {
  LiVESTextBuffer *textbuf = lives_text_view_get_buffer(textview);
  if (textbuf) return lives_text_buffer_set_text(textbuf, text, len);
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_markup(LiVESTextView * textview, const char *markup) {
  LiVESTextBuffer *textbuf = lives_text_view_get_buffer(textview);
  if (textbuf) return lives_text_buffer_insert_markup_at_end(textbuf, markup);
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_strip_markup(LiVESTextView * textview) {
  LiVESTextBuffer *textbuf = lives_text_view_get_buffer(textview);
  if (textbuf) {
    char *text = lives_text_buffer_get_all_text(textbuf);
    char *stripped = lives_text_strip_markup(text);
    lives_free(text);
    lives_text_buffer_set_text(textbuf, stripped, -1);
    lives_free(stripped);
    return TRUE;
  }
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_insert_at_end(LiVESTextBuffer * tbuff, const char *text) {
  LiVESTextIter xiter;
  if (lives_text_buffer_get_end_iter(tbuff, &xiter))
    return lives_text_buffer_insert(tbuff, &xiter, text, -1);
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_insert_markup_at_end(LiVESTextBuffer * tbuff, const char *markup) {
  LiVESTextIter xiter;
  if (lives_text_buffer_get_end_iter(tbuff, &xiter))
    return lives_text_buffer_insert_markup(tbuff, &xiter, markup, -1);
  return FALSE;
}


int get_box_child_index(LiVESBox * box, LiVESWidget * tchild) {
  LiVESList *list = lives_container_get_children(LIVES_CONTAINER(box));
  int val = -1;
  if (list) {
    val = lives_list_index(list, tchild);
    lives_list_free(list);
  }
  return val;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_pack_top(LiVESBox * box, LiVESWidget * child, boolean expand, boolean fill,
    uint32_t padding) {
  lives_box_pack_start(box, child, expand, fill, padding);
  lives_box_reorder_child(box, child, 0);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_child_set_shrinkable(LiVESContainer * c, LiVESWidget * child, boolean val) {
#ifdef GUI_GTK
  GValue xbool = G_VALUE_INIT;
  g_value_init(&xbool, G_TYPE_BOOLEAN);
  g_value_set_boolean(&xbool, val);
  gtk_container_child_set_property(c, child, "shrink", &xbool);
  return TRUE;
#endif
  return FALSE;
}


boolean set_submenu_colours(LiVESMenu * menu, LiVESWidgetColor * colf, LiVESWidgetColor * colb) {
  LiVESList *children = lives_container_get_children(LIVES_CONTAINER(menu)), *list = children;
  lives_widget_set_bg_color(LIVES_WIDGET(menu), LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_fg_color(LIVES_WIDGET(menu), LIVES_WIDGET_STATE_NORMAL, colf);
  for (; list; list = list->next) {
    LiVESWidget *child = (LiVESWidget *)list->data;
    if (LIVES_IS_SEPARATOR_MENU_ITEM(child)) continue;
    if (LIVES_IS_MENU_ITEM(child)) {
      if ((menu = (LiVESMenu *)lives_menu_item_get_submenu(LIVES_MENU_ITEM(child))))
        set_submenu_colours(menu, colf, colb);
      else {
        lives_widget_set_bg_color(LIVES_WIDGET(child), LIVES_WIDGET_STATE_NORMAL, colb);
        lives_widget_set_fg_color(LIVES_WIDGET(child), LIVES_WIDGET_STATE_NORMAL, colf);
      }
    }
  }
  if (children) lives_list_free(children);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_configure(LiVESAdjustment * adj,
    double value, double lower,
    double upper, double step_increment,
    double page_increment) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_configure(adj, value, lower, upper, step_increment, page_increment, 0.);
#else
  g_object_freeze_notify(LIVES_WIDGET_OBJECT(adj));
  adj->upper = upper;
  adj->lower = lower;
  adj->value = value;
  adj->step_increment = step_increment;
  adj->page_increment = page_increment;
  g_object_thaw_notify(LIVES_WIDGET_OBJECT(adj));
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_configure(LiVESSpinButton * spinbutton,
    double value, double lower,
    double upper, double step_increment,
    double page_increment) {
  LiVESAdjustment *adj = lives_spin_button_get_adjustment(spinbutton);
  return lives_adjustment_configure(adj, value, lower, upper, step_increment, page_increment);
}


boolean lives_tree_store_find_iter(LiVESTreeStore * tstore, int col, const char *val, LiVESTreeIter * titer1,
                                   LiVESTreeIter * titer2) {
#ifdef GUI_GTK
  if (gtk_tree_model_iter_children(LIVES_TREE_MODEL(tstore), titer2, titer1)) {
    char *ret;
    while (1) {
      gtk_tree_model_get(LIVES_TREE_MODEL(tstore), titer2, col, &ret, -1);
      if (!lives_strcmp(ret, val)) {
        lives_free(ret);
        return TRUE;
      }
      lives_free(ret);
      if (!gtk_tree_model_iter_next(LIVES_TREE_MODEL(tstore), titer2)) break;
    }
  }
  lives_tree_store_append(tstore, titer2, titer1);
  lives_tree_store_set(tstore, titer2, col, val, -1);
  return TRUE;
#endif
  return FALSE;
}


///// lives specific functions

#include "rte_window.h"
#include "ce_thumbs.h"

static boolean re_add_idlefunc = FALSE;

static void do_some_things(void) {
  // some old junk that may or may not be relevant now

  if (mainw->multitrack && mainw->multitrack->idlefunc > 0) {
    /// remove th multitrack timer; we don't want to trigger an autosave right now
    lives_source_remove(mainw->multitrack->idlefunc);
    mainw->multitrack->idlefunc = 0;
    if (!mainw->mt_needs_idlefunc) {
      re_add_idlefunc = mainw->mt_needs_idlefunc = TRUE;
    }
  }

  if (!mainw->is_exiting) {
    /// update the state of some widgets caused by OOB changes
    if (rte_window) rtew_set_key_check_state();
    if (mainw->ce_thumbs) {
      ce_thumbs_set_key_check_state();
      ce_thumbs_apply_liberation();
      if (mainw->ce_upd_clip) {
        ce_thumbs_highlight_current_clip();
        mainw->ce_upd_clip = FALSE;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


static boolean _lives_widget_context_update(void) {
  boolean is_fg_service = FALSE;
  int limit = EV_LIM;
  int count = 0;

  if (mainw->no_context_update) return FALSE;

  if (THREADVAR(fg_service)) {
    is_fg_service = TRUE;
  } else THREADVAR(fg_service) = TRUE;

  if (mainw->gui_much_events) {
    // (may be superfluous - toggle which permits more events to be processed)
    mainw->gui_much_events = FALSE;
    limit *= MUCH_EV_MPY;
  }

  while (count++ < limit && !mainw->is_exiting) {
    lives_widget_context_iteration(NULL, FALSE);
    if (!lives_widget_context_pending(NULL)) break;
    pthread_yield();
    lives_nanosleep(NSLEEP_TIME);
  }

  if (!is_fg_service) THREADVAR(fg_service) = FALSE;
  return FALSE;
}


static void do_more_stuff(void) {
  /// re-enable the multitrack autosave timer if removed it, otherwise caller can do it
  if (re_add_idlefunc) maybe_add_mt_idlefunc();
}


boolean lives_widget_context_update(void) {
  boolean ret = FALSE;
  if (mainw->no_context_update) return FALSE;

  if (!is_fg_thread()) {
    if (gui_loop_tight) {
      mainw->do_ctx_update = TRUE;
      return FALSE;
    } else {
      GET_PROC_THREAD_SELF(self);
      lives_hook_stack_t **lpt_hooks = lives_proc_thread_get_hook_stacks(self);
      // trip gui loop to high prio
      fg_service_wake();
      if (!(lpt_hooks[LIVES_GUI_HOOK]->flags & STACK_TRIGGERING)) {
        // action any deferred updates first
        if (lpt_hooks[LIVES_GUI_HOOK]->stack) {
          pthread_mutex_lock(&mainw->all_hstacks_mutex);
          mainw->all_hstacks =
            lives_list_remove_data(mainw->all_hstacks, lpt_hooks, FALSE);
          pthread_mutex_unlock(&mainw->all_hstacks_mutex);
          lives_proc_thread_trigger_hooks(self, LIVES_GUI_HOOK);
        }
      }
      // trigger gui loop update
      mainw->do_ctx_update = TRUE;
      lives_nanosleep_while_true(mainw->do_ctx_update);
      return TRUE;
    }
  }
  do_some_things();
  ret = _lives_widget_context_update();
  do_more_stuff();
  return ret;
}


LiVESWidget *lives_separator_menu_item_new(void) {
  LiVESWidget *separatormenuitem = NULL;
#ifdef GUI_GTK
  separatormenuitem = gtk_separator_menu_item_new();
#endif
  return separatormenuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_add_separator(LiVESMenu * menu) {
  LiVESWidget *separatormenuitem = lives_separator_menu_item_new();
  if (separatormenuitem) {
    lives_container_add(LIVES_CONTAINER(menu), separatormenuitem);
    lives_widget_set_sensitive(separatormenuitem, TRUE);
  }
  return separatormenuitem;
}


WIDGET_HELPER_GLOBAL_INLINE void lives_menu_item_set_text(LiVESWidget * menuitem, const char *text, boolean use_mnemonic) {
  LiVESWidget *label;
  if (LIVES_IS_MENU_ITEM(menuitem)) {
    label = lives_bin_get_child(LIVES_BIN(menuitem));
    widget_opts.mnemonic_label = use_mnemonic;
    lives_label_set_text(LIVES_LABEL(label), text);
    widget_opts.mnemonic_label = TRUE;
  }
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_menu_item_get_text(LiVESWidget * menuitem) {
  // text MUST be at least 255 chars long
  LiVESWidget *label = lives_bin_get_child(LIVES_BIN(menuitem));
  return lives_label_get_text(LIVES_LABEL(label));
}


WIDGET_HELPER_GLOBAL_INLINE int lives_display_get_n_screens(LiVESXDisplay * disp) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  return 1;
#else
  return gdk_display_get_n_screens(disp);
#endif
#endif
  return 1;
}


void lives_set_cursor_style(lives_cursor_t cstyle, LiVESWidget * widget) {
#ifdef GUI_GTK
  LiVESXWindow *window;
  GdkCursor *cursor = NULL;
  GdkDisplay *disp;
  GdkCursorType ctype = GDK_X_CURSOR;

  if (!widget) {
    if (mainw->recovering_files || ((!mainw->multitrack && mainw->is_ready)
                                    || (mainw->multitrack &&
                                        mainw->multitrack->is_ready))) {
      if (cstyle != LIVES_CURSOR_NORMAL && mainw->cursor_style == cstyle) return;
      window = lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET);
    } else return;
  } else window = lives_widget_get_xwindow(widget);

  if (!window || !LIVES_IS_XWINDOW(window)) return;

  switch (cstyle) {
  case LIVES_CURSOR_NORMAL:
    break;
  case LIVES_CURSOR_BUSY:
    ctype = GDK_WATCH;
    break;
  case LIVES_CURSOR_CENTER_PTR:
    ctype = GDK_CENTER_PTR;
    break;
  case LIVES_CURSOR_HAND2:
    ctype = GDK_HAND2;
    break;
  case LIVES_CURSOR_SB_H_DOUBLE_ARROW:
    ctype = GDK_SB_H_DOUBLE_ARROW;
    break;
  case LIVES_CURSOR_CROSSHAIR:
    ctype = GDK_CROSSHAIR;
    break;
  case LIVES_CURSOR_TOP_LEFT_CORNER:
    ctype = GDK_TOP_LEFT_CORNER;
    break;
  case LIVES_CURSOR_BOTTOM_RIGHT_CORNER:
    ctype = GDK_BOTTOM_RIGHT_CORNER;
    break;
  default: return;
  }
  if (!widget) {
    if (mainw->multitrack) mainw->multitrack->cursor_style = cstyle;
    else mainw->cursor_style = cstyle;
  }
#if GTK_CHECK_VERSION(2, 22, 0)
  cursor = gdk_window_get_cursor(window);
  if (cursor && gdk_cursor_get_cursor_type(cursor) == ctype) return;
  cursor = NULL;
#endif
  disp = gdk_window_get_display(window);
  if (cstyle != LIVES_CURSOR_NORMAL) {
    cursor = gdk_cursor_new_for_display(disp, ctype);
    gdk_window_set_cursor(window, cursor);
  } else gdk_window_set_cursor(window, NULL);
  if (cursor) lives_cursor_unref(cursor);
#endif

  // TODO: gdk_x11_cursor_update_theme (
  // XFixesChangeCursor (Display *dpy, Cursor source, Cursor destination);
  // and then wait for X11 event...
  // then no need for the majority of lives_window_process_updates().....
}


void hide_cursor(LiVESXWindow * window) {
  //make the cursor invisible in playback windows
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2, 16, 0)
  if (GDK_IS_WINDOW(window)) {
#if GTK_CHECK_VERSION(3, 16, 0)
    GdkCursor *cursor = gdk_cursor_new_for_display(gdk_window_get_display(window), GDK_BLANK_CURSOR);
#else
    GdkCursor *cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
#endif
    if (cursor) {
      gdk_window_set_cursor(window, cursor);
      lives_cursor_unref(cursor);
    }
  }
#else
  static GdkCursor *hidden_cursor = NULL;

  char cursor_bits[] = {0x00};
  char cursormask_bits[] = {0x00};
  GdkPixmap *source, *mask;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0, 0, 0 };

  if (!hidden_cursor) {
    source = gdk_bitmap_create_from_data(NULL, cursor_bits, 1, 1);
    mask = gdk_bitmap_create_from_data(NULL, cursormask_bits, 1, 1);
    hidden_cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 0, 0);
    g_object_unref(source);
    g_object_unref(mask);
  }
  if (GDK_IS_WINDOW(window)) gdk_window_set_cursor(window, hidden_cursor);
#endif
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean unhide_cursor(LiVESXWindow * window) {
  if (LIVES_IS_XWINDOW(window)) return lives_xwindow_set_cursor(window, NULL);
  return FALSE;
}

///#define USE_REVEAL - not working here
void funkify_dialog(LiVESWidget * dialog) {
  if (prefs->funky_widgets) {
    LiVESWidget *frame = lives_standard_frame_new(NULL, 0., FALSE);
    LiVESWidget *box = lives_vbox_new(FALSE, 0);
    LiVESWidget *content = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

    lives_container_set_border_width(LIVES_CONTAINER(dialog), 0);
    lives_container_set_border_width(LIVES_CONTAINER(frame), 0);

    lives_widget_object_ref(content);
    lives_widget_unparent(content);

    lives_container_add(LIVES_CONTAINER(dialog), frame);
    lives_container_add(LIVES_CONTAINER(frame), box);

    lives_box_pack_start(LIVES_BOX(box), content, TRUE, TRUE, 0);

    lives_widget_set_margin_bottom(box, widget_opts.packing_height); // only works for gtk+ 3.x

    lives_container_set_border_width(LIVES_CONTAINER(box), widget_opts.border_width * 2);
#ifdef USE_REVEAL
    gtk_revealer_set_reveal_child(GTK_REVEALER(frame), TRUE);
#endif
  } else {
    lives_container_set_border_width(LIVES_CONTAINER(dialog), widget_opts.border_width);
  }
}


void lives_cool_toggled(LiVESWidget * tbutton, livespointer user_data) {
#if GTK_CHECK_VERSION(3, 0, 0)
  //#if !GTK_CHECK_VERSION(3, 16, 0)
  // connect toggled event to this
  boolean *ret = (boolean *)user_data, active;
  if (!LIVES_IS_INTERACTIVE) return;
  active = ((LIVES_IS_TOGGLE_BUTTON(tbutton)
             && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbutton))) ||
            (LIVES_IS_TOGGLE_TOOL_BUTTON(tbutton)
             && lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbutton))));
  if (prefs->lamp_buttons) {
    if (active) {
      lives_widget_set_bg_color(tbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    } else lives_widget_set_bg_color(tbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
  }
  if (ret) *ret = active;
  lives_widget_queue_draw(tbutton);
  //#endif
#endif
}


boolean draw_cool_toggle(LiVESWidget * widget, lives_painter_t *cr, livespointer data) {
  // connect expose event to this
  double rwidth, rheight;
  double scalex = 1., scaley = .8;
  boolean active =
    ((LIVES_IS_TOGGLE_BUTTON(widget) && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widget))) ||
     (LIVES_IS_TOGGLE_TOOL_BUTTON(widget)
      && lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(widget))));

  rwidth = (double)GET_INT_DATA(widget, WIDTH_KEY);
  rheight = (double)GET_INT_DATA(widget, HEIGHT_KEY);

  if (rwidth <= 0.) rwidth = (double)lives_widget_get_allocation_width(LIVES_WIDGET(widget));
  else scalex = 1.;
  if (rheight <= 0.) {
    rheight = (double)lives_widget_get_allocation_height(LIVES_WIDGET(widget));
    if (!mainw->multitrack) rheight /= 2.;
  } else scaley = 1.;

  lives_painter_translate(cr, rwidth * (1. - scalex) / 2., rheight * (1. - scaley) / 2.);

  rwidth *= scalex;
  rheight *= scaley;

  // draw the inside
  if (active) {
    lives_painter_set_source_rgba(cr, palette->light_green.red, palette->light_green.green,
                                  palette->light_green.blue, 1.);
  } else {
    lives_painter_set_source_rgba(cr, palette->dark_red.red, palette->dark_red.green,
                                  palette->dark_red.blue, 1.);
  }

  // draw rounded rectangle
  lives_painter_lozenge(cr, 0, 0, rwidth, rheight, rwidth / 4.);
  lives_painter_fill(cr);

  // draw the surround

  lives_painter_new_path(cr);

  lives_painter_set_source_rgba(cr, 0., 0., 0., .8);
  lives_painter_set_line_width(cr, 1.);

  lives_painter_lozenge(cr, 0, 0, rwidth, rheight, rwidth / 4.);
  lives_painter_stroke(cr);

  if (active) {
    lives_painter_set_source_rgba(cr, 1., 1., 1., .6);

    lives_painter_move_to(cr, rwidth / 4., rwidth / 4.);
    lives_painter_line_to(cr, rwidth / 4.*3., rheight - rwidth / 4.);
    lives_painter_stroke(cr);

    lives_painter_move_to(cr, rwidth / 4., rheight - rwidth / 4.);
    lives_painter_line_to(cr, rwidth / 4.*3., rwidth / 4.);
    lives_painter_stroke(cr);
  }
  return TRUE;
}



WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_get_inner_size(LiVESWindow * win, int *x, int *y) {
  // get size request for child to fill window "win" (assuming win is maximised and moved maximum top / left)
  // function may not work in all circumstances
#ifdef GUI_GTK
  GdkRectangle rect;
  gint wx, wy;
  gdk_window_get_frame_extents(lives_widget_get_xwindow(LIVES_WIDGET(win)), &rect);
  //gdk_window_get_origin(lives_widget_get_xwindow(LIVES_WIDGET(win)), &wx, &wy);
  get_border_size(LIVES_WIDGET(win), &wx, &wy);
  if (x) *x = mainw->mgeom[widget_opts.monitor].width - (abs(wx) - rect.x) * 2;
  if (y) *y = mainw->mgeom[widget_opts.monitor].height;
  return TRUE;
#endif
  return FALSE;
}


boolean get_border_size(LiVESWidget * win, int *bx, int *by) {
  static int px = 10000000, py = 10000000;
#ifdef GUI_GTK
  int px1 = 0, py1 = 0, px2 = 0, py2 = 0, px3 = 0, py3 = 0, px4 = 0, py4 = 0;
  if (win == LIVES_MAIN_WINDOW_WIDGET) {
    int eww, ewh;
    if (mainw->hdrbar && lives_widget_is_realized(mainw->hdrbar)) {
      GdkWindow *xwin = lives_widget_get_xwindow(win);
      int xx, yy, xpx, xpy;
      gdk_window_get_root_origin(xwin, &xx, &yy);
      if (xx >= 0) xpx = xx;
      else xpx = lives_widget_get_allocation_x(mainw->hdrbar) - xx;
      if (yy >= 0) xpy = yy;
      else xpy = lives_widget_get_allocation_y(mainw->hdrbar) - yy;
      if (xpx < px) px1 = xpx;
      if (xpy < py) py1 = xpy;
    }
    if (get_screen_usable_size(&eww, &ewh)) {
      px2 = mainw->mgeom[widget_opts.monitor].phys_width - eww;
      py2 = mainw->mgeom[widget_opts.monitor].phys_height - ewh;
    }
    px3 =
      mainw->mgeom[widget_opts.monitor].phys_width - mainw->mgeom[widget_opts.monitor].width;
    py3 =
      mainw->mgeom[widget_opts.monitor].phys_height - mainw->mgeom[widget_opts.monitor].height;

    if (1) {
      GdkRectangle rect;
      GdkWindow *xwin = lives_widget_get_xwindow(win);
      if (xwin) {
        gdk_window_get_frame_extents(lives_widget_get_xwindow(win), &rect);
        px4  = rect.width - lives_widget_get_allocation_width(win);
        py4  = rect.height - lives_widget_get_allocation_height(win);
      }
    }
  }
  // x3, y3 give screen size minus panels
  // x1, y1 should be equal to x1, y1 when window is maximised
  // x3, y3 are calculated values for scr width / height - actual values; these should be 0, 0
  // x4 ???

  //g_print("BORD size: 1- %d X %d, 2- %d X %d, 3- %d X %d, 4- %d X %d\n", px1, py1, px2, py2, px3, py3, px4, py4);

  if (win == LIVES_MAIN_WINDOW_WIDGET) {
    if (px1 == px3 && px2 == 0 && py1 == py3 && py2 == 0) {
      if (bx) *bx = px3;
      if (by) *by = py3;
    } else {
      if (bx) *bx = 16;
      if (by) *by = 16;
    }
  } else {
    if (bx) *bx = px4;
    if (by) *by = py4;
  }
  return TRUE;
#endif
  return FALSE;
}

//   Set active string to the combo box
WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_active_string(LiVESCombo * combo, const char *active_str) {
  return lives_entry_set_text(LIVES_ENTRY(lives_bin_get_child(LIVES_BIN(combo))), active_str);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_combo_get_entry(LiVESCombo * widget) {
  return lives_bin_get_child(LIVES_BIN(widget));
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_can_focus_and_default(LiVESWidget * widget) {
  if (!lives_widget_set_can_focus(widget, TRUE)) return FALSE;
  return lives_widget_set_can_default(widget, TRUE);
}


void lives_general_button_clicked(LiVESButton * button, livespointer data_to_free) {
  // destroy the button top-level and free data
  if (LIVES_IS_WIDGET(lives_widget_get_toplevel(LIVES_WIDGET(button)))) {
    lives_widget_destroy(lives_widget_get_toplevel(LIVES_WIDGET(button)));
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  } else lives_abort("Invalid toplevel widget for clicked button");
  if (data_to_free) lives_free(data_to_free);

  /// TODO: this is BAD. Need to check that mainw->mt_needs_idlefunc is set conistently
  maybe_add_mt_idlefunc(); ///< add idlefunc iff mainw->mt_needs_idlefunc is set
}

void lives_general_button_clickedp(LiVESButton * button, livespointer * data_to_free) {
  lives_general_button_clicked(button, NULL);
  lives_freep(data_to_free);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_hseparator_new(void) {
  LiVESWidget *hseparator = lives_hseparator_new();
  if (widget_opts.apply_theme) {
    if (prefs->extra_colours && mainw->pretty_colours) {
      lives_widget_set_bg_color(hseparator, LIVES_WIDGET_STATE_NORMAL, &palette->nice1);
    } else {
      lives_widget_set_bg_color(hseparator, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_widget_set_fg_color(hseparator, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    set_css_value_direct(LIVES_WIDGET(hseparator), LIVES_WIDGET_STATE_NORMAL, "", "min-height", "4px");
  }
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_widget_set_margin_top(hseparator, widget_opts.packing_height);
  if (LIVES_SHOULD_EXPAND_EXTRA_HEIGHT)
    lives_widget_set_margin_bottom(hseparator, widget_opts.packing_height);
  return hseparator;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_vseparator_new(void) {
  LiVESWidget *vseparator = lives_vseparator_new();
  if (widget_opts.apply_theme) {
    if (prefs->extra_colours && mainw->pretty_colours) {
      lives_widget_set_bg_color(vseparator, LIVES_WIDGET_STATE_NORMAL, &palette->nice1);
    } else {
      lives_widget_set_bg_color(vseparator, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_widget_set_fg_color(vseparator, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }
  if (LIVES_SHOULD_EXPAND_WIDTH)
    lives_widget_set_margin_left(vseparator, widget_opts.packing_width);
  if (LIVES_SHOULD_EXPAND_EXTRA_WIDTH)
    lives_widget_set_margin_right(vseparator, widget_opts.packing_width);
  return vseparator;
}


LiVESWidget *add_hsep_to_box(LiVESBox * box) {
  LiVESWidget *hseparator = lives_standard_hseparator_new();
  int packing_height = widget_opts.packing_height;
  if (LIVES_IS_HBOX(box) || !LIVES_SHOULD_EXPAND_HEIGHT) packing_height = 0;
  lives_box_pack_start(box, hseparator, LIVES_IS_HBOX(box)
                       || LIVES_SHOULD_EXPAND_EXTRA_FOR(box), TRUE, packing_height);
  return hseparator;
}


LiVESWidget *add_vsep_to_box(LiVESBox * box) {
  LiVESWidget *vseparator = lives_standard_vseparator_new();
  int packing_width = widget_opts.packing_width >> 1;
  if (LIVES_SHOULD_EXPAND_EXTRA_FOR(box)) packing_width *= 2;
  if (LIVES_IS_VBOX(box)) packing_width = 0;
  lives_box_pack_start(box, vseparator, LIVES_IS_VBOX(box)
                       || LIVES_SHOULD_EXPAND_EXTRA_FOR(box), TRUE, packing_width);
  return vseparator;
}


//#define SHOW_FILL
LiVESWidget *add_fill_to_box(LiVESBox * box) {
#ifdef SHOW_FILL
  LiVESWidget *widget = lives_label_new("fill");
#else
  LiVESWidget *widget = lives_standard_label_new("");
#endif
  if (LIVES_IS_HBOX(box)) {
    int flen = 0;
    if (LIVES_SHOULD_EXPAND_FOR(box)) {
      int w = 0;
      LingoContext *ctx = lives_widget_create_lingo_context(LIVES_WIDGET(box));
      flen = widget_opts.filler_len;
      if (ctx && LINGO_IS_CONTEXT(ctx)) {
        LingoLayout *layout = lingo_layout_new(ctx);
        if (layout && LINGO_IS_LAYOUT(layout)) {
          lingo_layout_set_text(layout, "X", -1);
          lingo_layout_get_size(layout, &w, NULL);
          w /= LINGO_SCALE;
          lives_widget_object_unref(layout);
        }
        lives_widget_object_unref(ctx);
      }
      if (w) {
        int nchars = (float)widget_opts.filler_len / (float)w;
        if (nchars > 0) {
          flen = 0;
          lives_label_set_width_chars(LIVES_LABEL(widget), nchars);
        }
      }
    }
    lives_box_pack_start(box, widget, LIVES_SHOULD_EXPAND_EXTRA_FOR(box), TRUE, flen);
    if (LIVES_SHOULD_EXPAND_EXTRA_FOR(box))
      lives_widget_set_hexpand(widget, TRUE);
  } else {
    if (LIVES_SHOULD_EXPAND_EXTRA_FOR(box)) {
      lives_box_pack_start(box, widget, TRUE, TRUE, widget_opts.filler_len);
      lives_widget_set_vexpand(widget, TRUE);
    } else lives_box_pack_start(box, widget, FALSE, TRUE, LIVES_SHOULD_EXPAND_FOR(box) ? widget_opts.packing_height : 0);
  }
  if (widget_opts.apply_theme == 2) set_child_alt_colour(widget, TRUE);
  return widget;
}


LiVESWidget *add_spring_to_box(LiVESBox * box, int min) {
  LiVESWidget *widget;
  int filler_len = widget_opts.filler_len;
  int woe = widget_opts.expand;
  widget_opts.filler_len = min;
  widget_opts.expand = LIVES_EXPAND_EXTRA;
  widget = add_fill_to_box(box);
  widget_opts.expand = woe;
  widget_opts.filler_len = filler_len;
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_toolbar_insert_space(LiVESToolbar * bar) {
  LiVESWidget *spacer = NULL;
#ifdef GUI_GTK
  spacer = LIVES_WIDGET(lives_separator_tool_item_new());
  gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(spacer), FALSE);
  gtk_tool_item_set_homogeneous(LIVES_TOOL_ITEM(spacer), FALSE);
  gtk_tool_item_set_expand(LIVES_TOOL_ITEM(spacer), LIVES_SHOULD_EXPAND_WIDTH);
  lives_toolbar_insert(LIVES_TOOLBAR(bar), LIVES_TOOL_ITEM(spacer), -1);
#endif
  return spacer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_toolbar_insert_label(LiVESToolbar * bar, const char *text,
    LiVESWidget * actwidg) {
  LiVESWidget *item = NULL;
  widget_opts.last_label = widget_opts.last_container = NULL;
#ifdef GUI_GTK
  item = LIVES_WIDGET(lives_tool_item_new());
  if (!actwidg) {
    widget_opts.last_label = lives_label_new(text);
    lives_container_add(LIVES_CONTAINER(item), widget_opts.last_label);
  } else {
    widget_opts.last_container = _make_label_eventbox(text, actwidg, FALSE);
    lives_container_add(LIVES_CONTAINER(item), widget_opts.last_container);
  }
  lives_toolbar_insert(bar, LIVES_TOOL_ITEM(item), -1);
#endif
  return item;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_box_set_button_width(LiVESButtonBox * bbox, LiVESWidget * button,
    int min_width) {
  lives_button_box_set_layout(bbox, LIVES_BUTTONBOX_SPREAD);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_button_box_set_child_size(bbox, min_width / 4, DLG_BUTTON_HEIGHT);
  return TRUE;
#endif
#endif
  lives_widget_set_size_request(button, min_width, DLG_BUTTON_HEIGHT);
  return TRUE;
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_box_make_first(LiVESButtonBox * bbox, LiVESWidget * widget) {
#ifdef GUI_GTK
  if (!lives_widget_get_parent(widget)) {
    lives_box_pack_start(LIVES_BOX(bbox), widget, FALSE, FALSE, 0);
  }
  // any other layout seems to prevent this from working
  lives_button_box_set_layout(bbox, LIVES_BUTTONBOX_END);
  gtk_button_box_set_child_secondary(bbox, widget, TRUE);
  gtk_button_box_set_child_non_homogeneous(bbox, widget, TRUE);
  if (LIVES_SHOULD_EXPAND_WIDTH) {
    if (LIVES_IS_CONTAINER(widget))
      lives_container_set_border_width(LIVES_CONTAINER(widget), widget_opts.border_width);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_dialog_make_widget_first(LiVESDialog * dlg, LiVESWidget * widget) {
  LiVESWidget *daa = lives_dialog_get_action_area(dlg);
  return lives_button_box_make_first(LIVES_BUTTON_BOX(daa), widget);
}



WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_center(LiVESWidget * button) {
  if (LIVES_SHOULD_EXPAND_WIDTH)
    lives_widget_set_size_request(button, DEF_BUTTON_WIDTH * 2, DLG_BUTTON_HEIGHT);
  lives_button_box_set_layout(LIVES_BUTTON_BOX(lives_widget_get_parent(button)),
                              LIVES_BUTTONBOX_CENTER);
  lives_widget_set_halign(lives_widget_get_parent(button), LIVES_ALIGN_CENTER);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_uncenter(LiVESWidget * button, int normal_width) {
  lives_widget_set_size_request(button, normal_width, DLG_BUTTON_HEIGHT);
  lives_button_box_set_layout(LIVES_BUTTON_BOX(lives_widget_get_parent(button)), LIVES_BUTTONBOX_END);
  lives_widget_set_halign(lives_widget_get_parent(button), LIVES_ALIGN_FILL);
  return TRUE;
}


boolean lives_tool_button_set_border_color(LiVESWidget * button, LiVESWidgetState state, LiVESWidgetColor * colour) {
  if (LIVES_IS_TOOL_BUTTON(button)) {
    LiVESWidget *widget, *parent;
    widget = lives_tool_button_get_icon_widget(LIVES_TOOL_BUTTON(button));
    if (!widget) widget = lives_tool_button_get_label_widget(LIVES_TOOL_BUTTON(button));
    if (widget) {
      parent  = lives_widget_get_parent(widget);
      if (parent) lives_widget_set_bg_color(parent, state, colour);
      lives_widget_set_valign(widget, LIVES_ALIGN_FILL);
      lives_widget_set_halign(widget, LIVES_ALIGN_FILL);
    }
    return TRUE;
  }
  return FALSE;
}


LiVESWidget *lives_standard_tool_button_new(LiVESToolbar * bar, GtkWidget * icon_widget, const char *label,
    const char *tooltips) {
  LiVESToolItem *tbutton;
  widget_opts.last_label = NULL;
  if (label) {
    if (LIVES_SHOULD_EXPAND_HEIGHT) {
      char *labeltext = lives_strdup_printf("\n%s\n", label);
      widget_opts.last_label = lives_standard_label_new(labeltext);
      lives_free(labeltext);
    } else widget_opts.last_label = lives_standard_label_new(label);
  }
  tbutton = lives_tool_button_new(icon_widget, NULL);
  if (widget_opts.last_label) lives_tool_button_set_label_widget(LIVES_TOOL_BUTTON(tbutton), widget_opts.last_label);
  if (widget_opts.apply_theme) {
#if !GTK_CHECK_VERSION(3, 16, 0)
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tbutton), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb), NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(tbutton), NULL, NULL);
#endif
  }
  if (tooltips) lives_widget_set_tooltip_text(LIVES_WIDGET(tbutton), tooltips);
  if (bar) lives_toolbar_insert(bar, tbutton, -1);
  return LIVES_WIDGET(tbutton);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_accel_path_disconnect(LiVESAccelGroup * group, const char *path) {
#ifdef GUI_GTK
  GtkAccelKey key;
  gtk_accel_map_lookup_entry(path, &key);
  gtk_accel_group_disconnect_key(group, key.accel_key, key.accel_mods);
  return TRUE;
#endif
  return FALSE;
}


LiVESPixbuf *get_desktop_icon(const char *dir) {
  LiVESPixbuf *pixbuf = NULL;
  char *iconpfx = lives_build_path(prefs->prefix_dir, dir, NULL);
  char *icon = lives_build_filename(iconpfx, LIVES_LITERAL "." LIVES_FILE_EXT_PNG, NULL);
  if (!lives_file_test(icon, LIVES_FILE_TEST_EXISTS)) {
    lives_free(iconpfx); lives_free(icon);
    icon = iconpfx = NULL;
    if (!dirs_equal(prefs->prefix_dir, LIVES_USR_DIR)) {
      iconpfx = lives_build_path(LIVES_USR_DIR, dir, NULL);
      icon = lives_build_filename(iconpfx, LIVES_LITERAL "." LIVES_FILE_EXT_PNG, NULL);
      if (!lives_file_test(icon, LIVES_FILE_TEST_EXISTS)) {
        lives_free(iconpfx); lives_free(icon);
        icon = iconpfx = NULL;
      }
    }
  }
  if (icon) {
    LiVESError *error = NULL;
    pixbuf = lives_pixbuf_new_from_file(icon, &error);
    lives_free(iconpfx); lives_free(icon);
    if (error) lives_error_free(error);
  }
  return pixbuf;
}

WIDGET_HELPER_GLOBAL_INLINE lives_colRGBA64_t lives_rgba_col_new(int red, int green, int blue, int alpha) {
  lives_colRGBA64_t lcol = {red, green, blue, alpha};
  return lcol;
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGBA64_t *widget_color_to_lives_rgba(lives_colRGBA64_t *lcolor, LiVESWidgetColor * color) {
  lcolor->red = LIVES_WIDGET_COLOR_STRETCH(color->red);
  lcolor->green = LIVES_WIDGET_COLOR_STRETCH(color->green);
  lcolor->blue = LIVES_WIDGET_COLOR_STRETCH(color->blue);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  lcolor->alpha = LIVES_WIDGET_COLOR_STRETCH(color->alpha);
#else
  lcolor->alpha = 65535;
#endif
  return lcolor;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetColor *lives_rgba_to_widget_color(LiVESWidgetColor * color, lives_colRGBA64_t *lcolor) {
  color->red = LIVES_WIDGET_COLOR_SCALE_65535(lcolor->red);
  color->green = LIVES_WIDGET_COLOR_SCALE_65535(lcolor->green);
  color->blue = LIVES_WIDGET_COLOR_SCALE_65535(lcolor->blue);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  color->alpha = LIVES_WIDGET_COLOR_SCALE_65535(lcolor->alpha);
#endif
  return color;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_rgba_equal(lives_colRGBA64_t *col1, lives_colRGBA64_t *col2) {
  return !lives_memcmp(col1, col2, sizeof(lives_colRGBA64_t));
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGBA64_t *lives_rgba_copy(lives_colRGBA64_t *col1, lives_colRGBA64_t *col2) {
  col1->red = col2->red;
  col1->green = col2->green;
  col1->blue = col2->blue;
  col1->alpha = col2->alpha;
  return col1;
}


LiVESList *get_textsizes_list(void) {
  LiVESList *textsize_list = NULL;
#ifdef GUI_GTK
  textsize_list = lives_list_append(textsize_list, (_("Extra extra small")));
  textsize_list = lives_list_append(textsize_list, (_("Extra small")));
  textsize_list = lives_list_append(textsize_list, (_("Small")));
  textsize_list = lives_list_append(textsize_list, (_("Medium")));
  textsize_list = lives_list_append(textsize_list, (_("Large")));
  textsize_list = lives_list_append(textsize_list, (_("Extra large")));
  textsize_list = lives_list_append(textsize_list, (_("Extra extra large")));
#endif
  return textsize_list;
}


const char *lives_textsize_to_string(int val) {
  switch (val) {
  case 0:
    return LIVES_TEXT_SIZE_XX_SMALL;
  case 1:
    return LIVES_TEXT_SIZE_X_SMALL;
  case 2:
    return LIVES_TEXT_SIZE_SMALL;
  case 3:
    return LIVES_TEXT_SIZE_MEDIUM;
  case 4:
    return LIVES_TEXT_SIZE_LARGE;
  case 5:
    return LIVES_TEXT_SIZE_X_LARGE;
  case 6:
    return LIVES_TEXT_SIZE_XX_LARGE;
  default:
    return LIVES_TEXT_SIZE_NORMAL;
  }
}


#ifdef WEED_WIDGETS
// TODO - bring this all back online, using updated objects system
#include "paramwindow.h"

#define LIVES_LEAF_KLASS_ROLE "klass_role"
#define LIVES_LEAF_INTENTION "lives_intention_"
#define LIVES_LEAF_KLASS "klass"
#define LIVES_LEAF_KLASS_IDX "klass_idx"

static lives_widget_klass_t *w_klasses[N_TOOLKITS][KLASSES_PER_TOOLKIT];
static LiVESList *tk_list = NULL;
static LiVESList *k_list[N_TOOLKITS];

const LiVESList *lives_toolkits_available(void) {
  return tk_list;
}


const LiVESList *widget_toolkit_klasses_list(lives_toolkit_t tk) {
  if (tk < 1 || tk > N_TOOLKITS) return NULL;
  return k_list[tk - 1];
}


WIDGET_HELPER_LOCAL_INLINE lives_widget_klass_t *lives_widget_klass_new(int idx) {
  lives_widget_klass_t *k = lives_plant_new(LIVES_WEED_SUBTYPE_WIDGET);
  if (idx >= 0) weed_set_int_value(k, LIVES_LEAF_KLASS_IDX, idx);
  return k;
}

WIDGET_HELPER_LOCAL_INLINE
lives_widget_instance_t *lives_widget_instance_new(const lives_widget_klass_t *k) {
  lives_widget_instance_t *winst = lives_widget_klass_new(-1);
  weed_set_voidptr_value(winst, LIVES_LEAF_KLASS, (void *)k);
  return winst;
}

static lives_funcdef_t *lives_func_info_new(int cat, lives_funcptr_t func,
    uint32_t rettype, const char *args_fmt, void *data) {
  lives_funcdef_t *funcinf = (lives_funcdef_t *)lives_malloc(sizeof(lives_funcdef_t));
  funcinf->category = cat;
  funcinf->function = func;
  funcinf->rettype = rettype;
  funcinf->args_fmt = lives_strdup(args_fmt);
  funcinf->data = data;
  return funcinf;
}

WIDGET_HELPER_GLOBAL_INLINE int widget_klass_get_idx(const lives_widget_klass_t *k) {
  return weed_get_int_value((weed_plant_t *)k, LIVES_LEAF_KLASS_IDX, NULL);
}

WIDGET_HELPER_GLOBAL_INLINE int widget_klass_get_role(const lives_widget_klass_t *k) {
  return weed_get_int_value((weed_plant_t *)k, LIVES_LEAF_KLASS_ROLE, NULL);
}

WIDGET_HELPER_LOCAL_INLINE void widget_klass_set_role(lives_widget_klass_t *k, int role) {
  weed_set_int_value(k, LIVES_LEAF_KLASS_ROLE, role);
}


static lives_funcdef_t *get_func_with_rettype(lives_widget_instance_t *winst,
    lives_intention intent, uint32_t rettype) {
  int n_info;
  char *lname = lives_strdup_printf("%s%d", LIVES_LEAF_INTENTION, intent);
  lives_funcdef_t *funcinf = NULL;
  lives_funcdef_t **afuncinf
    = (lives_funcdef_t **)weed_get_voidptr_array_counted((weed_plant_t *)winst, lname, &n_info);
  for (int i = 0; i < n_info; i++) {
    if (afuncinf[i]->rettype == rettype) {
      funcinf = afuncinf[i];
      break;
    }
  }
  lives_freep((void **)&afuncinf);
  if (!funcinf) {
    const lives_widget_klass_t *k = widget_instance_get_klass(winst);
    if (k) {
      afuncinf
        = (lives_funcdef_t **)weed_get_voidptr_array_counted((weed_plant_t *)k, lname, &n_info);
      for (int i = 0; i < n_info; i++) {
        if (afuncinf[i]->rettype == rettype) {
          funcinf = afuncinf[i];
          break;
        }
      }
      lives_freep((void **)&afuncinf);
    }
  }
  lives_free(lname);
  return funcinf;
}

WIDGET_HELPER_LOCAL_INLINE
void add_method(lives_widget_klass_t *k, lives_intention intent, lives_funcptr_t func, uint32_t rettype,
                const char *fmt_args) {
  lives_funcdef_t *func_info = lives_func_info_new(0, func, rettype, fmt_args, k);
  char *lname = lives_strdup_printf("%s%d", LIVES_LEAF_INTENTION, intent);
  weed_set_voidptr_value(k, lname, intent);
  lives_free(lname);
}


boolean widget_klasses_init(lives_toolkit_t tk) {
  lives_widget_klass_t *k;
  lives_funcdef_t *afunc_info[8];
  char *lname;
  int i;

  if (tk == LIVES_TOOLKIT_GTK) {
    tk_list = lives_list_append(tk_list, LIVES_INT_TO_POINTER(tk));
    k_list[--tk] = NULL;

    for (i = 0; i < KLASSES_PER_TOOLKIT; i++) w_klasses[tk][i] = NULL;

    // GtkSpinButton
    k = w_klasses[tk][LIVES_PARAM_WIDGET_SPINBUTTON]
        = lives_widget_klass_new(LIVES_PARAM_WIDGET_SPINBUTTON);

    // CREATE
    add_method(k, LIVES_INTENT_CREATE_INSTANCE, (lives_funcptr_t)lives_spin_button_new,
               WEED_SEED_VOIDPTR, "Vdi");

    // GET_VALUE - method with multiple return value types
    afunc_info[0]
      = lives_func_info_new(0, (lives_funcptr_t)lives_spin_button_get_value,
                            WEED_SEED_DOUBLE, "V", (void *)k);
    afunc_info[1]
      = lives_func_info_new(0, (lives_funcptr_t)lives_spin_button_get_value_as_int,
                            WEED_SEED_INT, "V", (void *)k);

    lname = lives_strdup_printf("%s%d", LIVES_LEAF_INTENTION, OBJ_INTENTION_GET_VALUE);
    weed_set_voidptr_array(k, lname, 2, (void **)afunc_info);
    lives_free(lname);

    // SET_VALUE
    BG_THREADVAR(hook_hints) |= HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
    add_method(k, OBJ_INTENTION_SET_VALUE, (lives_funcptr_t)lives_spin_button_set_value,
               WEED_SEED_BOOLEAN, "Vd");
    BG_THREADVAR(hook_hints) = 0;

    widget_klass_set_role(k, KLASS_ROLE_WIDGET);

    while (i) if (w_klasses[tk][--i]) k_list[tk] = lives_list_prepend(k_list[tk], w_klasses[tk][i]);

    return TRUE;
  }
  return FALSE;
}


const lives_widget_klass_t *widget_klass_for_type(lives_toolkit_t tk, int typex) {
  if (tk == LIVES_TOOLKIT_GTK && typex >= 0 && typex < KLASSES_PER_TOOLKIT)
    return w_klasses[--tk][typex];
  return NULL;
}


lives_widget_instance_t *widget_instance_from_klass(const lives_widget_klass_t *k, ...) {
  if (!k || widget_klass_get_role(k) != KLASS_ROLE_WIDGET) return NULL;
  else {
    lives_funcdef_t *funcinf
      = get_func_with_rettype((lives_widget_instance_t *)k,
                              OBJ_INTENTION_CREATE_INSTANCE, WEED_SEED_VOIDPTR);
    if (!funcinf) return NULL;
    else {
      // create fake proc_thread, which we will pass to run_funsig
      // the return value will be in the _RV_ leaf
      va_list xargs;
      lives_proc_thread_t pth;
      lives_widget_instance_t *winst = lives_widget_instance_new(k);
      va_start(xargs, k);
      pth = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, funcinf->function,
                                           funcinf->rettype, funcinf->args_fmt, xargs);
      call_funcsig(pth);
      va_end(xargs);
      weed_set_voidptr_value(winst, LIVES_LEAF_WIDGET, lives_proc_thread_join_voidptr(pth));
      weed_plant_free(pth);
      return winst;
    }
  }
}


WIDGET_HELPER_GLOBAL_INLINE void *widget_instance_get_widget(lives_widget_instance_t *winst) {
  if (winst) return weed_get_voidptr_value(winst, LIVES_LEAF_WIDGET, NULL);
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE
const lives_widget_klass_t *widget_instance_get_klass(lives_widget_instance_t *winst) {
  if (winst) return weed_get_voidptr_value(winst, LIVES_LEAF_KLASS, NULL);
  return NULL;
}


double widget_func_double(lives_widget_instance_t *winst, lives_intention intent, ...) {
  if (!winst || !weed_get_voidptr_value(winst, LIVES_LEAF_WIDGET, NULL)) return 0.;
  else {
    // get transform with output
    lives_funcdef_t *funcinf = get_intent_with_rettype(winst, intent, WEED_SEED_DOUBLE);
    if (!funcinf) return 0.;
    else {
      // create fake proc_thread, which we will pass to run_funsig
      // the return value will be in the _RV_ leaf
      lives_proc_thread_t pth;
      va_list xargs;
      double dval;
      va_start(xargs, functype);
      pth = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, funcinf->function,
                                           WEED_SEED_DOUBLE, funcinf->args_fmt, xargs);
      call_funcsig(pth);
      va_end(xargs);
      dval = lives_proc_thread_join_double(pth);
      weed_plant_free(pth);
      return dval;
    }
  }
}


int widget_func_boolean(lives_widget_instance_t *winst, lives_intention intent, ...) {
  if (!winst || !weed_get_voidptr_value(winst, LIVES_LEAF_WIDGET, NULL)) return 0.;
  else {
    lives_funcdef_t *funcinf = get_func_with_rettype(winst, functype, WEED_SEED_BOOLEAN);
    if (!funcinf) return WEED_FALSE;
    else {
      // create fake proc_thread, which we will pass to run_funsig
      // the return value will be in the _RV_ leaf
      lives_proc_thread_t pth;
      va_list xargs;
      int bval;
      va_start(xargs, functype);
      pth = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, funcinf->function,
                                           WEED_SEED_BOOLEAN, funcinf->args_fmt, xargs);
      call_funcsig(pth);
      va_end(xargs);
      bval = lives_proc_thread_join_boolean(pth);
      weed_plant_free(pth);
      return bval;
    }
  }
}


LiVESList *widget_klass_list_intentions(const lives_widget_klass_t *k, boolean list_all) {
  // list_all ignored for now, until inheritance is implemented
  LiVESList *intentlist = NULL;
  weed_size_t nleaves;
  char **leaves = weed_plant_list_leaves((weed_plant_t *)k, &nleaves);
  size_t intentlen = lives_strlen(LIVES_LEAF_INTENTION);
  while (nleaves) {
    if (!lives_strncmp(leaves[--nleaves], LIVES_LEAF_INTENTION, intentlen)) {
      intentlist = lives_list_prepend(intentlist, LIVES_INT_TO_POINTER(atoi(leaves[nleaves] + fnlen)));
    }
    free(leaves[nleaves]);
  }
  free(leaves);
  intentlist = lives_list_reverse(intentlist);
  return intentlist;
}


lives_funcdef_t **get_widget_funcs_for_klass(const lives_widget_klass_t *k,
    lives_intention intent, int *nfuncs) {
  // TODO - when inheritance is implemented, check inherited klasses, and create
  // unified set but avoiding duplicates with same return types
  char *lname = lives_strdup_printf("%s%ld", LIVES_LEAF_INTENTION, intent);
  lives_funcdef_t **afuncinf
    = (lives_funcdef_t **)weed_get_voidptr_array_counted((weed_plant_t *)k, lname, nfuncs);
  lives_free(lname);
  return afuncinf;
}


const char *klass_idx_get_name(int klass_idx) {
  switch (klass_idx) {
  case LIVES_PARAM_WIDGET_SPINBUTTON: return "spin button";
  case LIVES_PARAM_WIDGET_SLIDER: return "slider control";
  case LIVES_PARAM_WIDGET_KNOB: return "knob control";
  default: return "unknown";
  }
}

const char *widget_toolkit_get_name(lives_toolkit_t tk) {
  switch (tk) {
  case LIVES_TOOLKIT_GTK: return "gtk+";
  default: return "Unknown";
  }
}


const char *klass_role_get_name(lives_toolkit_t tk, int role) {
  switch (role) {
  case KLASS_ROLE_WIDGET: return "widget";
  case KLASS_ROLE_INTERFACE: return "interface";
  case KLASS_ROLE_TK0: {
    if (tk == LIVES_TOOLKIT_GTK) return "adjustment";
  }
  default: return "unknown";
  }
}

const char *widget_intention_get_name(lives_intention intent) {
  switch (intent) {
  case LIVES_WIDGET_CREATE_FUNC: return "LIVES_WIDGET_CREATE_FUNC";
  case LIVES_WIDGET_DESTROY_FUNC: return "LIVES_WIDGET_DESTROY_FUNC";
  case LIVES_WIDGET_REF_FUNC: return "LIVES_WIDGET_REF_FUNC";
  case LIVES_WIDGET_UNREF_FUNC: return "LIVES_WIDGET_UNREF_FUNC";
  case LIVES_WIDGET_GET_VALUE_FUNC: return "LIVES_WIDGET_GET_VALUE_FUNC";
  case LIVES_WIDGET_SET_VALUE_FUNC: return "LIVES_WIDGET_SET_VALUE_FUNC";
  default: return "unknown";
  }
}

#endif
