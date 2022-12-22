// widget-helper-gtk.h
// LiVES
// (c) G. Finch 2012 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// widget implementation for GTK+

#ifndef HAS_LIVES_WIDGET_HELPER_GTK_H
#define HAS_LIVES_WIDGET_HELPER_GTK_H

#ifdef GUI_GTK

#ifdef ENABLE_GIW_3
#include "giw/giwtimeline.h"
#endif

#ifdef ENABLE_GIW
#include "giw/giwvslider.h"
#include "giw/giwled.h"
#endif

#ifdef LIVES_DIR_SEP
#undef LIVES_DIR_SEP
#endif

#define LIVES_DIR_SEP G_DIR_SEPARATOR_S

// a bug where gtk_radio_menu_item_set_active() does not update visually
// workaround: use check menuitems and update manually
#define GTK_RADIO_MENU_BUG

// a bug where setting a menuitem insensitive fails if it has a submenu
// workaround: (?) unparent submenu, change its state, reparent it
// not a bug, was setting menu instead of menuitem (or vice-versa ?)
#define GTK_SUBMENU_SENS_BUG

// a bug where textview crashes if too much text in it (maybe not a bug, was missing expose function ?)
#define GTK_TEXT_VIEW_DRAW_BUG

#if !GTK_CHECK_VERSION(3, 18, 9) // fixed version
// a bug where named textviews cannot be set by CSS: (res. not a bug, need to set the "text" node)
#define GTK_TEXT_VIEW_CSS_BUG
#endif

#if GTK_CHECK_VERSION(3, 16, 0)
#define USE_SPECIAL_BUTTONS
#endif

#define COMBO_LIST_LIMIT 256 // if we get a combo longer than this, we use a tree store

#ifndef IS_MINGW
typedef gboolean                          boolean;
#else

#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>

#ifndef GDK_IS_WIN32_DISPLAY
#define GDK_IS_WIN32_DISPLAY(display) (TRUE)
#endif

typedef uint8_t                           boolean;

#endif //GDK_WINDOWING_WIN32

#endif // IS_MINGW

#ifdef GDK_WINDOWING_X11

// needed for GDK_WINDOW_XID - for fileselector preview
// needed for gdk_x11_screen_get_window_manager_name()

#include <gdk/gdkx.h>

#ifndef GDK_IS_X11_DISPLAY
#define GDK_IS_X11_DISPLAY(display) (TRUE)
#endif

#ifndef GDK_IS_WIN32_DISPLAY
#define GDK_IS_WIN32_DISPLAY(display) (FALSE)
#endif

#define LIVES_XWINDOW_XID(z) GDK_WINDOW_XID(z)

#else

#ifndef GDK_IS_X11_DISPLAY
#define GDK_IS_X11_DISPLAY(display) (FALSE)
#endif

#define LIVES_XWINDOW_XID(z) (z)

#endif // GDK_WINDOWING_X11

#endif // GUI_GTK

#ifdef GUI_GTK

/// custom tweaks
#define PROGBAR_IS_ENTRY 1

/// Glib type stuff //////////////////////////////////////////
#ifndef G_ENCODE_VERSION
#define G_ENCODE_VERSION(major, minor) ((major) << 16 | (minor) << 8)
#endif

typedef GMainContext		  	  LiVESWidgetContext;
typedef GMainLoop		  	  LiVESWidgetLoop;

typedef GError                            LiVESError;

typedef GList                             LiVESList;
typedef GSList                            LiVESSList;

typedef gpointer                          livespointer;
typedef gconstpointer                     livesconstpointer;

typedef GClosure                          LiVESWidgetClosure;
typedef GCClosure                         LiVESWidgetCClosure;

//typedef GObject LiVESWidgetObject;
typedef GObject LiVESWidgetObject;

typedef GLogLevelFlags LiVESLogLevelFlags;

#define LIVES_LOG_LEVEL_MASK G_LOG_LEVEL_MASK
#define LIVES_LOG_FATAL_MASK G_LOG_FATAL_MASK

#define LIVES_LOG_FLAG_FATAL G_LOG_FLAG_FATAL ///< glib internal flag
#define LIVES_LOG_FLAG_RECURSION G_LOG_FLAG_RECURSION ///< glib internal flag

#define LIVES_LOG_LEVEL_FATAL G_LOG_LEVEL_ERROR
#define LIVES_LOG_LEVEL_CRITICAL G_LOG_LEVEL_CRITICAL
#define LIVES_LOG_LEVEL_WARNING G_LOG_LEVEL_WARNING
#define LIVES_LOG_LEVEL_MESSAGE G_LOG_LEVEL_MESSAGE
#define LIVES_LOG_LEVEL_INFO G_LOG_LEVEL_INFO
#define LIVES_LOG_LEVEL_DEBUG G_LOG_LEVEL_DEBUG

//////////////////////////////////////////////////

#define LIVES_WIDGET_PRIORITY_HIGH G_PRIORITY_HIGH // - 100 (not used by GTK / GDK)
#define LIVES_WIDGET_PRIORITY_DEFAULT G_PRIORITY_DEFAULT // 0 (timeouts)

#define LIVES_WIDGET_PRIORITY_HIGH_IDLE G_PRIORITY_HIGH_IDLE // 100 (110 resize, 120 redraw)
#define LIVES_WIDGET_PRIORITY_DEFAULT_IDLE G_PRIORITY_DEFAULT_IDLE // 200 (standard idle funcs)
#define LIVES_WIDGET_PRIORITY_LOW G_PRIORITY_LOW // 300 (low priority, not used by GTK / GDK)

#if GTK_CHECK_VERSION(3, 10, 0)
#define LIVES_TABLE_IS_GRID 1
#endif

#define LIVES_IS_PLACES_SIDEBAR(w) GTK_IS_PLACES_SIDEBAR(w)

#define LIVES_ACCEL_PATH_QUIT "<LiVES>/quit"
#define LIVES_ACCEL_PATH_SAVE "<LiVES>/save"

#define return_true gtk_true

typedef void (*LiVESGuiCallback)(void);
typedef void (*LiVESWidgetCallback)(GtkWidget *widget, gpointer data);
typedef gboolean(*LiVESWidgetSourceFunc)(gpointer data);
typedef gint(*LiVESCompareFunc)(gconstpointer a, gconstpointer b);

#define LIVES_LITTLE_ENDIAN G_LITTLE_ENDIAN
#define LIVES_BIG_ENDIAN G_BIG_ENDIAN

#define LIVES_MAXINT G_MAXINT
#define LIVES_MAXUINT32 G_MAXUINT32
#define LIVES_MAXINT32 G_MAXINT32
#define LIVES_MAXUINT64 G_MAXUINT64
#define LIVES_MAXINT64 G_MAXINT64
#define LIVES_MAXSIZE G_MAXSIZE
#define LIVES_MAXFLOAT G_MAXFLOAT
#define LIVES_MAXDOUBLE G_MAXDOUBLE

#define LIVES_IS_RTL (gtk_widget_get_default_direction() == GTK_TEXT_DIR_RTL)

#define LIVES_GUI_CALLBACK(f) ((LiVESGuiCallback) (f))

#define lives_widget_context_new() g_main_context_new()
#define lives_widget_context_unref(ctx) g_main_context_unref(ctx)
#define lives_widget_context_get_thread_default() g_main_context_get_thread_default()
#define lives_widget_context_default() g_main_context_default()
#define lives_widget_context_push_thread_default(ctx) g_main_context_push_thread_default(ctx)
#define lives_widget_context_pop_thread_default(ctx) g_main_context_pop_thread_default(ctx)
#define lives_widget_context_invoke(ctx, func, arg) g_main_context_invoke(ctx, func, arg)
#define lives_widget_context_invoke_full(ctx, prio, func, arg, delfunc) g_main_context_invoke_full(ctx, prio, func, arg, delfunc)
#define lives_widget_context_pending(ctx) g_main_context_pending(ctx)
#define lives_widgets_get_current_event() gtk_get_current_event()

#define lives_idle_add_simple(func, data) g_idle_add(func, data)
#if GTK_CHECK_VERSION(3, 0, 0)
#define lives_timer_add_simple(interval, func, data) (interval < 1000 ? g_timeout_add(interval, func, data) \
						      : g_timeout_add_seconds(interval / 1000., func, data))
#else
#define lives_timer_add_simple(interval, func, data) g_timeout_add(interval, func, data)
#endif

#define lives_markup_escape_text(a, b) g_markup_escape_text(a, b)

#define lives_markup_printf_escaped(...) g_markup_printf_escaped(__VA_ARGS__)

#define lives_print(...) g_print(__VA_ARGS__)
#define lives_fprintf(...) fprintf(__VA_ARGS__)
#define lives_printerr(...) g_printerr(__VA_ARGS__)
#define lives_strdup_printf(...) g_strdup_printf(__VA_ARGS__)
#define lives_strdup_free(a, b) (lives_free_and_return((a)) ? NULL : lives_strdup(b))
#define lives_strdup_printf_free(a, ...) (lives_free_and_return((a)) ? NULL : lives_strdup_printf(__VA_ARGS__))
#define lives_strdup_vprintf(fmt, ...) g_strdup_vprintf(fmt, __VA_ARGS__)
#define lives_strndup(a, b) g_strndup(a, b)
#define lives_snprintf(a, b, ...) g_snprintf(a, b, __VA_ARGS__)
#define lives_strsplit(a, b, c) g_strsplit(a, b, c)
#define lives_strfreev(a) g_strfreev(a)
#define lives_strdup(a) g_strdup(a)
#define lives_ascii_strup(a, b) g_ascii_strup(a, b)
#define lives_ascii_strdown(a, b) g_ascii_strdown(a, b)
#define lives_ascii_strcasecmp(a, b) g_ascii_strcasecmp(a, b)
#define lives_ascii_strncasecmp(a, b, c) g_ascii_strncasecmp(a, b, c)
#define lives_strconcat(a, ...) g_strconcat(a, __VA_ARGS__)
#define lives_strstrip(a) g_strstrip(a)
#define lives_strrstr(a, b) g_strrstr(a, b)
#define lives_strstr_len(a, b, c) g_strstr_len(a, b, c)
#define lives_strdelimit(a, b, c) g_strdelimit(a, b, c)

#define lives_utf8_collate(a, b) g_utf8_collate(a, b)
#define lives_utf8_collate_key(a, b) g_utf8_collate_key(a, b)
#define lives_utf8_casefold(a, b) g_utf8_casefold(a, b)

#define lives_list_nth_data(list, i) g_list_nth_data(list, i)
#define lives_list_nth(list, i) g_list_nth(list, i)
#define lives_list_length(list) g_list_length(list)
#define lives_list_free(list) g_list_free(list)
#define _lives_list_free g_list_free
#define lives_list_append(list, data) g_list_append(list, data)
#define lives_list_prepend(list, data) g_list_prepend(list, data)
#define lives_list_find(list, data) g_list_find(list, data)
#define lives_list_previous(list) g_list_previous(list)
#define lives_list_last(list) g_list_last(list)
#define lives_list_delete_link(list, link) g_list_delete_link(list, link)
#define lives_list_copy(list) g_list_copy(list)
#define lives_list_next(list) g_list_next(list)
#define lives_list_first(list) g_list_first(list)
#define lives_list_reverse(list) g_list_reverse(list)
#define lives_list_remove(list, data) g_list_remove(list, data)
#define lives_list_remove_link(list, data) g_list_remove_link(list, data)
#define lives_list_concat(list, data) g_list_concat(list, data)
#define lives_list_insert(list, data, pos) g_list_insert(list, data, pos)
#define lives_list_index(list, data) g_list_index(list, data)
#define lives_list_sort(list, cmp_func) g_list_sort(list, cmp_func)
#define lives_list_sort_with_data(list, cmp_func, userdata) g_list_sort_with_data(list, cmp_func, userdata)
#define lives_list_find_custom(list, data, func) g_list_find_custom(list, data, func)

#define lives_slist_free(list) g_slist_free(list)
#define lives_slist_length(list) g_slist_length(list)
#define lives_slist_nth_data(list, i) g_slist_nth_data(list, i)
#define lives_slist_append(list, data) g_slist_append(list, data)
#define lives_slist_prepend(list, data) g_slist_prepend(list, data)

#define lives_build_filename(...) g_build_filename(LIVES_DIR_SEP, __VA_ARGS__)
#define lives_build_filename_relative(...) g_build_filename(__VA_ARGS__)
#define lives_build_path(...) g_build_path(LIVES_DIR_SEP, LIVES_DIR_SEP, __VA_ARGS__)
#define lives_build_path_relative(...) g_build_path(LIVES_DIR_SEP, __VA_ARGS__)
#define lives_filename_to_utf8(a, b, c, d, e) g_filename_to_utf8(a, b, c, d, e)
#define lives_filename_from_utf8(a, b, c, d, e) g_filename_from_utf8(a, b, c, d, e)

#define lives_utf8_strdown(a, b) g_utf8_strdown(a, b)
#define lives_utf8_strup(a, b) g_utf8_strup(a, b)

#define lives_find_program_in_path(a) g_find_program_in_path(a)

#define lives_set_prgname(a) g_set_prgname(a)
#define lives_get_prgname() g_get_prgname()

#define lives_set_application_name(a) g_set_application_name(a)
#define lives_get_application_name() g_get_application_name()

#define lives_mkdir_with_parents(a, b) g_mkdir_with_parents(a, b)

#define lives_strtod(a) g_strtod(a, NULL)

#define lives_path_get_basename(a) g_path_get_basename(a)

#define LIVES_UNLIKELY(a) G_UNLIKELY(a)
#define LIVES_LIKELY(a) G_LIKELY(a)

#define lives_file_test(a, b) g_file_test(a, b)

#define lives_get_current_dir() g_get_current_dir()

#define lives_error_free(a) g_error_free(a)

#define lives_strerror(a) g_strerror(a)

#define lives_cclosure_new(a, b, c) g_cclosure_new(a, b, c)

#define lives_path_get_dirname(a) g_path_get_dirname(a)

#ifndef lives_locale_to_utf8
#define lives_locale_to_utf8(a, b, c, d, e) g_locale_to_utf8(a, b, c, d, e)
#endif

#define lives_charset_convert(String, from, to) (g_convert(String, -1, from, to, NULL, NULL, NULL))

#define U82L(String) (g_locale_from_utf8(String, -1, NULL, NULL, NULL))
#define L2U8(String) (g_locale_to_utf8(String, -1, NULL, NULL, NULL))

#define U82F(String) (g_filename_from_utf8(String, -1, NULL, NULL, NULL))
#define F2U8(String) (g_filename_to_utf8(String, -1, NULL, NULL, NULL))

#define LIVES_FILE_TEST_EXISTS G_FILE_TEST_EXISTS
#define LIVES_FILE_TEST_IS_DIR G_FILE_TEST_IS_DIR
#define LIVES_FILE_TEST_IS_REGULAR G_FILE_TEST_IS_REGULAR
#define LIVES_FILE_TEST_IS_SYMLINK G_FILE_TEST_IS_SYMLINK
#define LIVES_FILE_TEST_IS_EXECUTABLE G_FILE_TEST_IS_EXECUTABLE

typedef GtkJustification LiVESJustification;

#define LIVES_JUSTIFY_START   GTK_JUSTIFY_LEFT
#define LIVES_JUSTIFY_END  GTK_JUSTIFY_RIGHT
#define LIVES_JUSTIFY_CENTER GTK_JUSTIFY_CENTER
#define LIVES_JUSTIFY_FILL   GTK_JUSTIFY_RIGHT

typedef GtkOrientation LiVESOrientation;
#define LIVES_ORIENTATION_HORIZONTAL GTK_ORIENTATION_HORIZONTAL
#define LIVES_ORIENTATION_VERTICAL   GTK_ORIENTATION_VERTICAL

typedef GdkEvent                          		LiVESXEvent;
typedef GdkXEvent                         		LiVESXXEvent;
typedef GdkEventButton                    	LiVESXEventButton;
typedef GdkEventMotion                    	LiVESXEventMotion;
typedef GdkEventScroll                    	LiVESXEventScroll;
typedef GdkEventExpose                    	LiVESXEventExpose;
typedef GdkEventCrossing                  	LiVESXEventCrossing;
typedef GdkEventConfigure                 	LiVESXEventConfigure;
typedef GdkEventFocus                     	LiVESXEventFocus;
typedef GdkEventKey                       	LiVESXEventKey;
typedef GdkEvent                          		LiVESXEventDelete;
typedef GdkDisplay                        		LiVESXDisplay;
typedef GdkScreen                         		LiVESXScreen;
#if GTK_CHECK_VERSION(3, 22, 0)
typedef GdkMonitor                             	LiVESXMonitor;
#endif
typedef GdkDevice                         		LiVESXDevice;

#define LIVES_KEY_RELEASE GDK_KEY_RELEASE
#define LIVES_KEY_PRESS GDK_KEY_PRESS

#define LiVESScrollDirection GdkScrollDirection
#define LIVES_SCROLL_UP   GDK_SCROLL_UP
#define LIVES_SCROLL_DOWN GDK_SCROLL_DOWN

#if GTK_CHECK_VERSION(3, 0, 0)
#if !GTK_CHECK_VERSION(4, 0, 0)
#undef LIVES_HAS_DEVICE_MANAGER
#define LIVES_HAS_DEVICE_MANAGER 1
typedef GdkDeviceManager                  LiVESXDeviceManager;
#endif
#endif

#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(4, 0, 0)
// gtk+ 4
/*         several updates will be needed in order to include gtk4 support, including:

	   gtk_container methods will all need modifying (possible quick fix:
	   change GTK_CONTAINER -> GTK_WIDGET, then modify gtk_container_* functions)

	   gtk_bin no longer exusts (all widgets now can take multiple child widgets - may require some
	   redesign - TBD)

	   gtk_widget_destroy deprecated (quick fix: lives_widget_destroy can check the widget
	   type and then call the apprpriate gtk 4 function. Longer term, introduce lives_widget_t

	   GdkWindow -> GdkSurface (partially done, but untested), some functions moved to
	   gdk_x11_*, some functions need implementing using xlib directly (e.g. keep_above)

	   gtkaccelgroup -> shortcut (should be relatively easing using the wrapper functions)

	   no more gtkeventbox (all widgets now receive all events. Possible quick fix, replace
	   with GtkBox or similar widget, longer term, rewrite code to ignore the widget)

	   no more gtkbuttonbox (seems conterproductive to remove a widget that does a useful job,
	   will need replacing with GtkBox and impleentations of layout, homogenous, secondary)
	   - related to this, GtkDialog action area no longer directly accessible; this is
	   already taken care of by adding a button box to the content area. Longer term it needs to
	   use the replicated buttonbox functions.

	   GtkScrolledWindow - adjustments no longer appear as parameters in _new. Done.

	   GtkCssProvider load functions have lost their GError argument - starightforward, as the
	   value is ignored anyway

	   gtk_widget_get_width - maybe better than allocation_width ?

	   gtk_drawing_area_set_draw_func() - straightforwards, replaces expose function

	   gdk_pixbuf -> texture (quick fix - convert gdkpixbuf to texture using provided function,
	   longer term, use alias, conversion to / from layers will benefit

	   GtkMenu, GtkMenuBar and GtkMenuItem are gone (will likely need a combination of aliasing
	   and architectural changes - TBD)

	   GtkToolbar has been removed - replace with GtkBox, no longer need now that buttons
	   are drawn internally
*/

#define EXPOSE_FN_DECL(fn, widget, user_data)				\
  // TODO: gtk_snaphot_append_cairo()
boolean fn(LiVESWidget *widget, \lives_painter_t *cairo, int width, int height, livespointer user_data) \
{
  LiVESXEventExpose *event = NULL; (void)event; // avoid compiler warnings

#define EXPOSE_FN_PAINTER
#define EXPOSE_FN_PROTOTYPE(fn) boolean fn(LiVESWidget *, lives_painter_t *, int, int, livespointer);

#else
// gtk+ 3

#define EXPOSE_FN_DECL(fn, widget, user_data)				\
  boolean fn(LiVESWidget *widget, lives_painter_t *cairo, livespointer user_data) {LiVESXEventExpose *event = NULL; (void)event;

#define EXPOSE_FN_PAINTER
#define EXPOSE_FN_PROTOTYPE(fn) boolean fn(LiVESWidget *, lives_painter_t *, livespointer);
#endif

#else
// gtk+ 2

#define EXPOSE_FN_DECL(fn, widget, user_data) boolean fn(LiVESWidget *widget, LiVESXEventExpose *event, \
							 livespointer user_data) {lives_painter_t *cairo = NULL;

#define EXPOSE_FN_EVENT
#define EXPOSE_FN_PROTOTYPE(fn) boolean fn(LiVESWidget *, LiVESXEventExpose *, livespointer);
#endif

#define EXPOSE_FN_END }

#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(4, 0, 0)
#define LIVES_WIDGET_EXPOSE_EVENT "snapshot"
#else
#define LIVES_WIDGET_EXPOSE_EVENT "draw"
#endif
#define LIVES_GUI_OBJECT(a)                     a
#else
#define LIVES_GUI_OBJECT(a)                     GTK_OBJECT(a)
#define LIVES_GUI_OBJECT_CLASS(a)               GTK_OBJECT_CLASS(a)
#define LIVES_WIDGET_EXPOSE_EVENT "expose_event"
#endif

#define lives_widget_object_set_data(a, b, c) g_object_set_data(a, b, c)
#define lives_widget_object_set_data_full(a, b, c, d) g_object_set_data_full(a, b, c, d)
#define lives_widget_object_get_data(a, b) g_object_get_data(a, b)
#define lives_widget_object_steal_data(a, b) g_object_steal_data(a, b)

#define lives_widget_object_set(a, b, c) g_object_set(a, b, c, NULL)
#define lives_widget_object_get(a, b, c) g_object_get(a, b, c, NULL)

#define LIVES_WIDGET_OBJECT(a) G_OBJECT(a)

#define lives_widget_object_freeze_notify(a) g_object_freeze_notify(a)
#define lives_widget_object_thaw_notify(a) g_object_thaw_notify(a)

#if GTK_CHECK_VERSION(3, 0, 0)
#define NO_MEM_OVERRIDE TRUE
#else
#define NO_MEM_OVERRIDE g_mem_is_system_malloc()
#endif

  typedef GMemVTable LiVESMemVTable;
  typedef GIOChannel LiVESIOChannel;

  typedef GtkTargetEntry LiVESTargetEntry;

  typedef GdkFilterReturn LiVESFilterReturn;

#define LIVES_FILTER_REMOVE GDK_FILTER_REMOVE
#define LIVES_FILTER_CONTINUE GDK_FILTER_CONTINUE

  typedef GtkIconTheme			  LiVESIconTheme;

  // events
#define LIVES_WIDGET_EVENT "event"
#define LIVES_WIDGET_SCROLL_EVENT "scroll-event"
#define LIVES_WIDGET_CONFIGURE_EVENT "configure-event"
#define LIVES_WIDGET_ENTER_EVENT "enter-notify-event"
#define LIVES_WIDGET_BUTTON_PRESS_EVENT "button-press-event"
#define LIVES_WIDGET_BUTTON_RELEASE_EVENT "button-release-event"
#define LIVES_WIDGET_MOTION_NOTIFY_EVENT "motion-notify-event"
#define LIVES_WIDGET_LEAVE_NOTIFY_EVENT "leave-notify-event"
#define LIVES_WIDGET_FOCUS_IN_EVENT "focus-in-event"
#define LIVES_WIDGET_FOCUS_OUT_EVENT "focus-out-event"
#define LIVES_WIDGET_DELETE_EVENT "delete-event"
#define LIVES_WIDGET_KEY_PRESS_EVENT "key-press-event"
#define LIVES_WIDGET_KEY_RELEASE_EVENT "key-release-event"

  // signals
#define LIVES_WIDGET_DESTROY_SIGNAL "destroy"
#define LIVES_WIDGET_CLICKED_SIGNAL "clicked"
#define LIVES_WIDGET_TOGGLED_SIGNAL "toggled"
#define LIVES_WIDGET_CHANGED_SIGNAL "changed"
#define LIVES_WIDGET_ACTIVATE_SIGNAL "activate"
#define LIVES_WIDGET_ACTIVATE_ITEM_SIGNAL "activate-item"
#define LIVES_WIDGET_VALUE_CHANGED_SIGNAL "value-changed"
#define LIVES_WIDGET_SELECTION_CHANGED_SIGNAL "selection-changed"
#define LIVES_WIDGET_CURRENT_FOLDER_CHANGED_SIGNAL "current-folder-changed"
#define LIVES_WIDGET_RESPONSE_SIGNAL "response"
#define LIVES_WIDGET_DRAG_DATA_RECEIVED_SIGNAL "drag-data-received"
#define LIVES_WIDGET_SIZE_PREPARED_SIGNAL "size-prepared"
#define LIVES_WIDGET_MODE_CHANGED_SIGNAL "mode-changed"
#define LIVES_WIDGET_SWITCH_PAGE_SIGNAL "switch-page"
#define LIVES_WIDGET_UNMAP_SIGNAL "unmap"
#define LIVES_WIDGET_EDITED_SIGNAL "edited"
#define LIVES_WIDGET_ROW_EXPANDED_SIGNAL "row-expanded"
#define LIVES_WIDGET_COLOR_SET_SIGNAL "color-set"
#define LIVES_WIDGET_SET_FOCUS_CHILD_SIGNAL "set-focus-child"
#define LIVES_WIDGET_SHOW_SIGNAL "show"
#define LIVES_WIDGET_HIDE_SIGNAL "hide"
#define LIVES_WIDGET_FONT_SET_SIGNAL "font-set"

#if GTK_CHECK_VERSION(3, 0, 0)
#define LIVES_WIDGET_STATE_CHANGED_SIGNAL "state-flags-changed"
#else
#define LIVES_WIDGET_STATE_CHANGED_SIGNAL "state-changed"
#endif

#define LIVES_WIDGET_NOTIFY_SIGNAL "notify::"

  typedef GtkWidget                         LiVESWidget;
  typedef GtkWindow                         LiVESWindow;
  typedef GtkContainer                      LiVESContainer;
  typedef GtkBin                            LiVESBin;
  typedef GtkDialog                         LiVESDialog;
  typedef GtkBox                            LiVESBox;
  typedef GtkFrame                          LiVESFrame;
  typedef GtkComboBox                       LiVESCombo;
  typedef GtkComboBox                       LiVESComboBox;
  typedef GtkButton                         LiVESButton;
  typedef GtkButtonBox                      LiVESButtonBox;

  typedef GtkTextView                       LiVESTextView;
  typedef GtkTextBuffer                     LiVESTextBuffer;
  typedef GtkTextMark                       LiVESTextMark;
  typedef GtkTextIter                       LiVESTextIter;

  typedef GtkEntry                          LiVESEntry;
  typedef GtkEntryCompletion                LiVESEntryCompletion;
  typedef GtkRadioButton                    LiVESRadioButton;
  typedef GtkSpinButton                     LiVESSpinButton;
  typedef GtkColorButton                    LiVESColorButton;
  typedef GtkToolButton                     LiVESToolButton;
  typedef GtkLabel                          LiVESLabel;
  typedef GtkImage                          LiVESImage;
  typedef GtkFileChooser                    LiVESFileChooser;
#if GTK_CHECK_VERSION(3, 2, 0)
  typedef GtkFontChooser                    LiVESFontChooser;
  typedef GtkFontButton                     LiVESFontButton;
#endif
  typedef GtkAlignment                      LiVESAlignment;
  typedef GtkAllocation                     LiVESAllocation;
#ifdef GTK_HEADER_BAR
#define LIVES_HAS_HEADER_BAR_WIDGET 1
  typedef GtkHeaderBar   		    LiVESHeaderBar;
#else
  typedef GtkWidget		    	    LiVESHeaderBar;
#define LIVES_HEADER_BAR		    GTK_WIDGET
#endif
  typedef GtkMenu                           LiVESMenu;
  typedef GtkMenuShell                      LiVESMenuShell;
  typedef GtkMenuItem                       LiVESMenuItem;
  typedef GtkMenuToolButton                 LiVESMenuToolButton;
  typedef GtkToggleToolButton               LiVESToggleToolButton;
  typedef GtkCheckMenuItem                  LiVESCheckMenuItem;
  typedef GtkImageMenuItem                  LiVESImageMenuItem;
  typedef GtkRadioMenuItem                  LiVESRadioMenuItem;

  typedef GtkNotebook                       LiVESNotebook;

  typedef GtkExpander                       LiVESExpander;

#ifdef GTK_SPINNER
#define LIVES_HAS_SPINNER_WIDGET 1
  typedef GtkSpinner        	          LiVESSpinner;
#else
  typedef GtkWidget        	          LiVESSpinner;
#endif

#ifdef PROGBAR_IS_ENTRY
  typedef GtkEntry 	                   LiVESProgressBar;
#else
  typedef GtkProgressBar                    LiVESProgressBar;
#endif

  typedef GtkAboutDialog                    LiVESAboutDialog;

  // values here are long unsigned int
#define LIVES_COL_TYPE_OBJECT G_TYPE_OBJECT
#define LIVES_COL_TYPE_STRING G_TYPE_STRING
#define LIVES_COL_TYPE_INT G_TYPE_INT
#define LIVES_COL_TYPE_UINT G_TYPE_UINT
#define LIVES_COL_TYPE_BOOLEAN G_TYPE_BOOLEAN
#define LIVES_COL_TYPE_PIXBUF GDK_TYPE_PIXBUF

  typedef GtkTreeView                       LiVESTreeView;
  typedef GtkTreeViewColumn                 LiVESTreeViewColumn;

  typedef GtkTreeViewColumnSizing LiVESTreeViewColumnSizing;
#define LIVES_TREE_VIEW_COLUMN_GROW_ONLY GTK_TREE_VIEW_COLUMN_GROW_ONLY
#define LIVES_TREE_VIEW_COLUMN_AUTOSIZE GTK_TREE_VIEW_COLUMN_AUTOSIZE
#define LIVES_TREE_VIEW_COLUMN_FIXED GTK_TREE_VIEW_COLUMN_FIXED

#define LIVES_TREE_VIEW_COLUMN_TEXT "text"
#define LIVES_TREE_VIEW_COLUMN_PIXBUF "pixbuf"

  typedef GtkCellRenderer                   LiVESCellRenderer;
  typedef GtkTreeModel                      LiVESTreeModel;
  typedef GtkTreeIter                       LiVESTreeIter;
  typedef GtkTreePath                       LiVESTreePath;
  typedef GtkTreeStore                      LiVESTreeStore;
  typedef GtkTreeSelection                  LiVESTreeSelection;
  typedef GtkListStore                      LiVESListStore;

  typedef GtkScrolledWindow                 LiVESScrolledWindow;
  typedef GtkScrollbar                      LiVESScrollbar;
  typedef GtkHScrollbar                     LiVESHScrollbar;
  typedef GtkToolbar                        LiVESToolbar;
  typedef GtkToolItem                       LiVESToolItem;

#if GTK_CHECK_VERSION(2, 14, 0)
  typedef GtkScaleButton                    LiVESScaleButton;
#else
  typedef GtkRange                          LiVESScaleButton;
#endif

#ifdef GTK_GRID
  typedef GtkGrid                           LiVESGrid;
#undef LIVES_HAS_GRID_WIDGET
#define LIVES_HAS_GRID_WIDGET 1
#else
  typedef LiVESWidget                       LiVESGrid;
#endif

#if LIVES_TABLE_IS_GRID
  typedef GtkGrid                           LiVESTable;
#else
  typedef GtkTable                          LiVESTable;
#endif

#ifdef GTK_SWITCH
#undef LIVES_HAS_SWITCH_WIDGET
#define LIVES_HAS_SWITCH_WIDGET 1
  typedef GtkSwitch                         LiVESSwitch;
  typedef GtkWidget                         LiVESToggleButton;
#else
  typedef LiVESWidget                       LiVESSwitch;
  typedef GtkToggleButton                   LiVESToggleButton;
#endif

#define LiVESLayout LiVESTable

  typedef GtkEditable                       LiVESEditable;

#if GTK_CHECK_VERSION(3, 0, 0)
#define LIVES_WIDGET_COLOR_HAS_ALPHA (1)
#define LIVES_WIDGET_COLOR_SCALE(x) (x) ///< macro to get 0. to 1. from widget colour
#define LIVES_WIDGET_COLOR_STRETCH(x) (x * 65535.) ///< macro to get 0. to 65535. from widget colour
#define LIVES_WIDGET_COLOR_SCALE_65535(x) ((double)x / 65535.) ///< macro to convert from (0. - 65535.) to widget color
#define LIVES_WIDGET_COLOR_SCALE_255(x) ((double)x / 255.) ///< macro to convert from (0. - 255.) to widget color
  typedef GdkRGBA                           LiVESWidgetColor; ///< component values are 0. to 1.

  typedef GtkStateFlags LiVESWidgetState;

#define LIVES_WIDGET_STATE_NORMAL         GTK_STATE_FLAG_NORMAL        // 0
#define LIVES_WIDGET_STATE_ACTIVE         GTK_STATE_FLAG_ACTIVE       //  1
#define LIVES_WIDGET_STATE_PRELIGHT       GTK_STATE_FLAG_PRELIGHT     //  2
#define LIVES_WIDGET_STATE_SELECTED       GTK_STATE_FLAG_SELECTED     //  4
#define LIVES_WIDGET_STATE_INSENSITIVE    GTK_STATE_FLAG_INSENSITIVE     // 8
#define LIVES_WIDGET_STATE_INCONSISTENT   GTK_STATE_FLAG_INCONSISTENT   // 16
#define LIVES_WIDGET_STATE_FOCUSED        GTK_STATE_FLAG_FOCUSED         // 32
#define LIVES_WIDGET_STATE_BACKDROP       GTK_STATE_FLAG_BACKDROP       // 64
#if GTK_CHECK_VERSION(3, 14, 0)
#define LIVES_WIDGET_STATE_CHECKED        GTK_STATE_FLAG_CHECKED
#endif
#else

#define LIVES_WIDGET_COLOR_HAS_ALPHA (0)
#define LIVES_WIDGET_COLOR_SCALE(x) ((double)x/65535.)     ///< macro to get 0. to 1. from widget color
#define LIVES_WIDGET_COLOR_STRETCH(x) (x)     ///< macro to get 0 to 65535 from widget color
#define LIVES_WIDGET_COLOR_SCALE_65535(x) (x)     ///< macro to get 0 - 65535 to widget color
#define LIVES_WIDGET_COLOR_SCALE_255(x) ((int)((double)x*256.+.5))     ///< macro to get 0 - 255 to widget color
  typedef GdkColor                          LiVESWidgetColor;  ///< component values are 0 to 65535
  typedef GtkStateType LiVESWidgetState;

#define LIVES_WIDGET_STATE_NORMAL         GTK_STATE_NORMAL
#define LIVES_WIDGET_STATE_ACTIVE         GTK_STATE_ACTIVE
#define LIVES_WIDGET_STATE_PRELIGHT       GTK_STATE_PRELIGHT
#define LIVES_WIDGET_STATE_SELECTED       GTK_STATE_SELECTED
#define LIVES_WIDGET_STATE_INSENSITIVE    GTK_STATE_INSENSITIVE

#define LIVES_WIDGET_STATE_INCONSISTENT   (GTK_STATE_INSENSITIVE+1)
#define LIVES_WIDGET_STATE_FOCUSED        (GTK_STATE_INSENSITIVE+2)
#define LIVES_WIDGET_STATE_BACKDROP       (GTK_STATE_INSENSITIVE+3)
#endif

  typedef int LiVESResponseType;
#define LIVES_RESPONSE_NONE GTK_RESPONSE_NONE // -1
#define LIVES_RESPONSE_REJECT GTK_RESPONSE_REJECT  // -2
#define LIVES_RESPONSE_ACCEPT GTK_RESPONSE_ACCEPT  // -3
#define LIVES_RESPONSE_DELETE_EVENT GTK_RESPONSE_DELETE_EVENT  // -4
#define LIVES_RESPONSE_OK GTK_RESPONSE_OK     // -5
#define LIVES_RESPONSE_CANCEL GTK_RESPONSE_CANCEL  // -6
#define LIVES_RESPONSE_CLOSE GTK_RESPONSE_CLOSE  // -7
#define LIVES_RESPONSE_YES GTK_RESPONSE_YES  // -8
#define LIVES_RESPONSE_NO GTK_RESPONSE_NO  // -9
#define LIVES_RESPONSE_APPLY GTK_RESPONSE_APPLY  // -10
#define LIVES_RESPONSE_HELP GTK_RESPONSE_HELP  // -11

  // positive values for custom responses
#define LIVES_RESPONSE_INVALID 0
#define LIVES_RESPONSE_RETRY 1
#define LIVES_RESPONSE_ABORT 2
#define LIVES_RESPONSE_RESET 3  // be careful NOT to confuse this with RETRY !!
#define LIVES_RESPONSE_SHOW_DETAILS 4
#define LIVES_RESPONSE_BROWSE 5

  typedef GtkAttachOptions LiVESAttachOptions;
#define LIVES_EXPAND GTK_EXPAND
#define LIVES_SHRINK GTK_SHRINK
#define LIVES_FILL GTK_FILL

  typedef GtkPackType LiVESPackType;
#define LIVES_PACK_START GTK_PACK_START
#define LIVES_PACK_END GTK_PACK_END

  typedef GtkWindowType LiVESWindowType;
#define LIVES_WINDOW_TOPLEVEL GTK_WINDOW_TOPLEVEL
#define LIVES_WINDOW_POPUP GTK_WINDOW_POPUP

  typedef GtkDialogFlags LiVESDialogFlags;

  typedef GtkMessageType LiVESMessageType;
#define LIVES_MESSAGE_INFO GTK_MESSAGE_INFO
#define LIVES_MESSAGE_WARNING GTK_MESSAGE_WARNING
#define LIVES_MESSAGE_QUESTION GTK_MESSAGE_QUESTION
#define LIVES_MESSAGE_ERROR GTK_MESSAGE_ERROR
#define LIVES_MESSAGE_OTHER GTK_MESSAGE_OTHER

  typedef GtkButtonsType LiVESButtonsType;
#define LIVES_BUTTONS_NONE GTK_BUTTONS_NONE

  typedef GtkFileChooserAction LiVESFileChooserAction;
#define LIVES_FILE_CHOOSER_ACTION_OPEN GTK_FILE_CHOOSER_ACTION_OPEN
#define LIVES_FILE_CHOOSER_ACTION_SAVE GTK_FILE_CHOOSER_ACTION_SAVE
#define LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
#define LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER

#define LIVES_FILE_CHOOSER_ACTION_SELECT_CREATE_FOLDER ((GtkFileChooserAction)(GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER + 8))

#define LIVES_FILE_CHOOSER_ACTION_SELECT_FILE ((GtkFileChooserAction)(GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER + 16))
#define LIVES_FILE_CHOOSER_ACTION_SELECT_HIDDEN_FILE ((GtkFileChooserAction)(GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER + 17))
#define LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE ((GtkFileChooserAction)(GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER + 18))

  typedef GtkIconSize LiVESIconSize;
#define LIVES_ICON_SIZE_INVALID GTK_ICON_SIZE_INVALID
#define LIVES_ICON_SIZE_MENU GTK_ICON_SIZE_MENU // 16px
#define LIVES_ICON_SIZE_SMALL_TOOLBAR GTK_ICON_SIZE_SMALL_TOOLBAR // 16px
#define LIVES_ICON_SIZE_LARGE_TOOLBAR GTK_ICON_SIZE_LARGE_TOOLBAR // 24px
#define LIVES_ICON_SIZE_BUTTON GTK_ICON_SIZE_BUTTON // 16px
#define LIVES_ICON_SIZE_DND GTK_ICON_SIZE_DND // 32px
#define LIVES_ICON_SIZE_DIALOG GTK_ICON_SIZE_DIALOG // 48px
#define LIVES_ICON_SIZE_CUSTOM 1024

  // scrolledwindow policies
  typedef GtkPolicyType LiVESPolicyType;
#define LIVES_POLICY_ALWAYS GTK_POLICY_ALWAYS
#define LIVES_POLICY_AUTOMATIC GTK_POLICY_AUTOMATIC
#define LIVES_POLICY_NEVER GTK_POLICY_NEVER

  typedef GtkPositionType LiVESPositionType;
#define LIVES_POS_LEFT GTK_POS_LEFT
#define LIVES_POS_RIGHT GTK_POS_RIGHT
#define LIVES_POS_TOP GTK_POS_TOP
#define LIVES_POS_BOTTOM GTK_POS_BOTTOM

  typedef GtkArrowType LiVESArrowType;
#define LIVES_ARROW_UP GTK_ARROW_UP
#define LIVES_ARROW_DOWN GTK_ARROW_DOWN
#define LIVES_ARROW_LEFT GTK_ARROW_LEFT
#define LIVES_ARROW_RIGHT GTK_ARROW_RIGHT
#define LIVES_ARROW_NONE GTK_ARROW_NONE

#if GTK_CHECK_VERSION(3, 0, 0)
  typedef GtkAlign LiVESAlign;
#define LIVES_ALIGN_FILL GTK_ALIGN_FILL
#define LIVES_ALIGN_START GTK_ALIGN_START
#define LIVES_ALIGN_END GTK_ALIGN_END
#define LIVES_ALIGN_CENTER GTK_ALIGN_CENTER
#else
  typedef int LiVESAlign;
#define LIVES_ALIGN_FILL 0
#define LIVES_ALIGN_START 1
#define LIVES_ALIGN_END 2
#define LIVES_ALIGN_CENTER 3
#endif

#if GTK_CHECK_VERSION(3, 10, 0)
#define LIVES_ALIGN_BASELINE GTK_ALIGN_BASELINE
#endif

  typedef GtkWrapMode LiVESWrapMode;
#define LIVES_WRAP_NONE GTK_WRAP_NONE
  //#define LIVES_WRAP_CHAR GTK_WRAP_CHAR
#define LIVES_WRAP_WORD GTK_WRAP_WORD
  //#define LIVES_WRAP_WORD_CHAR GTK_WRAP_WORD_CHAR

  typedef GtkReliefStyle LiVESReliefStyle;
#define LIVES_RELIEF_NORMAL GTK_RELIEF_NORMAL
#define LIVES_RELIEF_HALF GTK_RELIEF_HALF
#define LIVES_RELIEF_NONE GTK_RELIEF_NONE

#define LIVES_ACCEL_VISIBLE GTK_ACCEL_VISIBLE

  typedef GtkToolbarStyle LiVESToolbarStyle;
#define LIVES_TOOLBAR_ICONS GTK_TOOLBAR_ICONS
#define LIVES_TOOLBAR_TEXT  GTK_TOOLBAR_TEXT

  typedef GtkSelectionMode LiVESSelectionMode;
#define LIVES_SELECTION_NONE GTK_SELECTION_NONE
#define LIVES_SELECTION_SINGLE GTK_SELECTION_SINGLE
  //#define LIVES_SELECTION_BROWSE GTK_SELECTION_BROWSE
#define LIVES_SELECTION_MULTIPLE GTK_SELECTION_MULTIPLE

  typedef GtkButtonBoxStyle LiVESButtonBoxStyle;
#define LIVES_BUTTONBOX_DEFAULT_STYLE GTK_BUTTONBOX_DEFAULT_STYLE
#define LIVES_BUTTONBOX_SPREAD GTK_BUTTONBOX_SPREAD
#define LIVES_BUTTONBOX_EDGE GTK_BUTTONBOX_EDGE  // like SPREAD but buttons touch edges
#define LIVES_BUTTONBOX_START GTK_BUTTONBOX_START
#define LIVES_BUTTONBOX_END GTK_BUTTONBOX_END
#define LIVES_BUTTONBOX_CENTER GTK_BUTTONBOX_CENTER
#if GTK_CHECK_VERSION(3, 12, 0)
#define LIVES_BUTTONBOX_EXPAND GTK_BUTTONBOX_EXPAND // buttons take max size
#else
#define LIVES_BUTTONBOX_EXPAND 9999
#endif

  typedef GdkEventMask LiVESEventMask;
#define LIVES_EXPOSURE_MASK GDK_EXPOSURE_MASK
#define LIVES_POINTER_MOTION_MASK GDK_POINTER_MOTION_MASK
#define LIVES_POINTER_MOTION_HINT_MASK GDK_POINTER_MOTION_HINT_MASK
#define LIVES_BUTTON_MOTION_MASK GDK_BUTTON_MOTION_MASK
#define LIVES_BUTTON1_MOTION_MASK GDK_BUTTON1_MOTION_MASK
#define LIVES_BUTTON2_MOTION_MASK GDK_BUTTON2_MOTION_MASK
#define LIVES_BUTTON3_MOTION_MASK GDK_BUTTON3_MOTION_MASK
#define LIVES_BUTTON_PRESS_MASK GDK_BUTTON_PRESS_MASK
#define LIVES_BUTTON_RELEASE_MASK GDK_BUTTON_RELEASE_MASK
#define LIVES_KEY_PRESS_MASK GDK_KEY_PRESS_MASK
#define LIVES_KEY_RELEASE_MASK GDK_KEY_RELEASE_MASK
#define LIVES_ENTER_NOTIFY_MASK GDK_ENTER_NOTIFY_MASK
#define LIVES_LEAVE_NOTIFY_MASK GDK_LEAVE_NOTIFY_MASK
#define LIVES_FOCUS_CHANGE_MASK GDK_FOCUS_CHANGE_MASK
#define LIVES_STRUCTURE_MASK GDK_STRUCTURE_MASK
#define LIVES_PROPERTY_CHANGE_MASK GDK_PROPERTY_CHANGE_MASK
#define LIVES_VISIBILITY_NOTIFY_MASK GDK_VISIBILITY_NOTIFY_MASK
#define LIVES_PROXIMITY_IN_MASK GDK_PROXIMITY_IN_MASK
#define LIVES_PROXIMITY_OUT_MASK GDK_PROXIMITY_OUT_MASK
#define LIVES_SUBSTRUCTURE_MASK GDK_SUBSTRUCTURE_MASK
#define LIVES_SCROLL_MASK GDK_SCROLL_MASK

#define LIVES_BUTTON_PRESS GDK_BUTTON_PRESS
#define LIVES_BUTTON_RELEASE GDK_BUTTON_RELEASE
#define LIVES_BUTTON2_PRESS GDK_2BUTTON_PRESS

#if GTK_CHECK_VERSION(3, 4, 0)
#define LIVES_TOUCH_MASK GDK_TOUCH_MASK
#define LIVES_SMOOTH_SCROLL_MASK GDK_SMOOTH_SCROLL_MASK
#else
#define LIVES_TOUCH_MASK 0
#define LIVES_SMOOTH_SCROLL_MASK 0
#endif

#define LIVES_ALL_EVENTS_MASK GDK_ALL_EVENTS_MASK

  typedef GtkShadowType LiVESShadowType;
#define LIVES_SHADOW_NONE GTK_SHADOW_NONE
#define LIVES_SHADOW_IN GTK_SHADOW_IN
#define LIVES_SHADOW_OUT GTK_SHADOW_OUT
#define LIVES_SHADOW_ETCHED_IN GTK_SHADOW_ETCHED_IN
#define LIVES_SHADOW_ETCHED_OUT GTK_SHADOW_ETCHED_OUT

  typedef GtkWindowPosition LiVESWindowPosition;
#define LIVES_WIN_POS_NONE GTK_WIN_POS_NONE
#define LIVES_WIN_POS_CENTER_ALWAYS GTK_WIN_POS_CENTER_ALWAYS

#if GTK_CHECK_VERSION(3, 0, 0)
  typedef GtkScale                          LiVESRuler;
  typedef GtkBox                            LiVESVBox;
  typedef GtkBox                            LiVESHBox;
#else
  typedef GtkWidget                         LiVESSwitch;
  typedef GtkRuler                          LiVESRuler;
  typedef GtkVBox                           LiVESVBox;
  typedef GtkHBox                           LiVESHBox;
#endif

  typedef GtkEventBox                       LiVESEventBox;

  typedef GtkRange                          LiVESRange;

  typedef GtkAdjustment                     LiVESAdjustment;

  typedef GdkPixbuf                         LiVESPixbuf;

#if GTK_CHECK_VERSION(4, 0, 0)
  typedef GdkSurface                        LiVESXWindow;
#else
  typedef GdkWindow                         LiVESXWindow;
#endif

  typedef GdkCursor                         LiVESXCursor;

  typedef GdkModifierType                   LiVESXModifierType;

  typedef GtkAccelGroup                     LiVESAccelGroup;
  typedef GtkAccelFlags                     LiVESAccelFlags;

  typedef GtkRequisition                    LiVESRequisition;

  typedef GtkPaned                          LiVESPaned;

  typedef GtkScale                          LiVESScale;

  typedef GdkPixbufDestroyNotify            LiVESPixbufDestroyNotify;
  typedef GDestroyNotify            	  LiVESDestroyNotify;

  typedef GdkInterpType                     LiVESInterpType;

#define LIVES_WIDGET(widget) GTK_WIDGET(widget)
#define LIVES_PIXBUF(widget) GDK_PIXBUF(widget)
#define LIVES_WINDOW(widget) GTK_WINDOW(widget)
#define LIVES_XWINDOW(widget) GDK_WINDOW(widget)
#define LIVES_BOX(widget) GTK_BOX(widget)
#define LIVES_EVENT_BOX(widget) GTK_EVENT_BOX(widget)
#define LIVES_ENTRY(widget) GTK_ENTRY(widget)
#define LIVES_FRAME(widget) GTK_FRAME(widget)
#define LIVES_CONTAINER(widget) GTK_CONTAINER(widget)
#define LIVES_BIN(widget) GTK_BIN(widget)
#define LIVES_ADJUSTMENT(widget) GTK_ADJUSTMENT(widget)
#define LIVES_DIALOG(widget) GTK_DIALOG(widget)
#define LIVES_SCALE(widget) GTK_SCALE(widget)
#define LIVES_PANED(widget) GTK_PANED(widget)
#define LIVES_COMBO(widget) GTK_COMBO_BOX(widget)
#define LIVES_COMBO_BOX(widget) GTK_COMBO_BOX(widget)
#define LIVES_BUTTON(widget) GTK_BUTTON(widget)
#define LIVES_BUTTON_BOX(widget) GTK_BUTTON_BOX(widget)
#define LIVES_LABEL(widget) GTK_LABEL(widget)
#define LIVES_ALIGNMENT(widget) GTK_ALIGNMENT(widget)
#define LIVES_FILES_CHOOSER(widget) GTK_FILE_CHOOSER(widget)
#if GTK_CHECK_VERSION(3, 2, 0)
#define LIVES_FONT_CHOOSER(widget) GTK_FONT_CHOOSER(widget)
#define LIVES_FONT_BUTTON(widget) GTK_FONT_BUTTON(widget)
#endif
#define LIVES_RADIO_BUTTON(widget) GTK_RADIO_BUTTON(widget)
#define LIVES_SPIN_BUTTON(widget) GTK_SPIN_BUTTON(widget)
#define LIVES_COLOR_BUTTON(widget) GTK_COLOR_BUTTON(widget)
#define LIVES_TOOL_BUTTON(widget) GTK_TOOL_BUTTON(widget)

#if LIVES_HAS_HEADER_BAR_WIDGET
#define LIVES_HEADER_BAR(widget) GTK_HEADER_BAR(widget)
#endif

#define LIVES_MENU(widget) GTK_MENU(widget)
#define LIVES_MENU_SHELL(widget) GTK_MENU_SHELL(widget)
#define LIVES_MENU_TOOL_BUTTON(widget) GTK_MENU_TOOL_BUTTON(widget)
#define LIVES_MENU_ITEM(widget) GTK_MENU_ITEM(widget)
#define LIVES_IMAGE(widget) GTK_IMAGE(widget)
#define LIVES_CHECK_MENU_ITEM(widget) GTK_CHECK_MENU_ITEM(widget)
#define LIVES_RADIO_MENU_ITEM(widget) GTK_RADIO_MENU_ITEM(widget)
#define LIVES_FILE_CHOOSER(widget) GTK_FILE_CHOOSER(widget)
#define LIVES_SCROLLED_WINDOW(widget) GTK_SCROLLED_WINDOW(widget)
#define LIVES_TOOLBAR(widget) GTK_TOOLBAR(widget)
#define LIVES_TOOL_ITEM(widget) GTK_TOOL_ITEM(widget)

#define LIVES_NOTEBOOK(widget) GTK_NOTEBOOK(widget)

#ifdef PROGBAR_IS_ENTRY
#define LIVES_PROGRESS_BAR(widget) GTK_ENTRY(widget)
#else
#define LIVES_PROGRESS_BAR(widget) GTK_PROGRESS_BAR(widget)
#endif

#if LIVES_HAS_SPINNER_WIDGET
#define LIVES_SPINNER(widget) GTK_SPINNER(widget)
#endif

#define LIVES_EXPANDER(widget) GTK_EXPANDER(widget)

#define LIVES_MISC(widget) GTK_MISC(widget)

#if GTK_CHECK_VERSION(2, 14, 0)
#define LIVES_SCALE_BUTTON(widget) GTK_SCALE_BUTTON(widget)
#else
#define LIVES_SCALE_BUTTON(widget) GTK_RANGE(widget)
#endif

#define LIVES_TOGGLE_TOOL_BUTTON(widget) GTK_TOGGLE_TOOL_BUTTON(widget)
#define LIVES_TEXT_VIEW(widget) GTK_TEXT_VIEW(widget)
#define LIVES_TEXT_BUFFER(widget) GTK_TEXT_BUFFER(widget)

#define LIVES_TREE_VIEW(widget) GTK_TREE_VIEW(widget)
#define LIVES_TREE_MODEL(object) GTK_TREE_MODEL(object)

#define LIVES_LIST_STORE(object) GTK_LIST_STORE(object)
#define LIVES_TREE_STORE(object) GTK_TREE_STORE(object)

#define LIVES_ACCEL_GROUP(object) GTK_ACCEL_GROUP(object)

#if GTK_CHECK_VERSION(3, 0, 0)
#define LIVES_RULER(widget) GTK_SCALE(widget)
#define LIVES_ORIENTABLE(widget) GTK_ORIENTABLE(widget)
#define LIVES_VBOX(widget) GTK_BOX(widget)
#define LIVES_HBOX(widget) GTK_BOX(widget)
#else
#define LIVES_RULER(widget) GTK_RULER(widget)
#define LIVES_VBOX(widget) GTK_VBOX(widget)
#define LIVES_HBOX(widget) GTK_HBOX(widget)
#endif

#if LIVES_HAS_SWITCH_WIDGET
#define LIVES_SWITCH(widget) GTK_SWITCH(widget)
#define LIVES_TOGGLE_BUTTON(widget) widget
#else
#define LIVES_SWITCH(widget) GTK_TOGGLE_BUTTON(widget)
#define LIVES_TOGGLE_BUTTON(widget) GTK_TOGGLE_BUTTON(widget)
#endif

#ifndef GTK_IMAGE_MENU_ITEM
#define LIVES_IMAGE_MENU_ITEM(widget) GTK_MENU_ITEM(widget)
#else
#define LIVES_IMAGE_MENU_ITEM(widget) GTK_IMAGE_MENU_ITEM(widget)
#undef LIVES_HAS_IMAGE_MENU_ITEM
#define LIVES_HAS_IMAGE_MENU_ITEM 1
#endif

#ifdef GTK_GRID
#define LIVES_GRID(widget) GTK_GRID(widget)
#define LIVES_TABLE(widget) GTK_GRID(widget)
#define LIVES_IS_TABLE(widget) GTK_IS_GRID(widget)
#define LIVES_IS_GRID(widget) GTK_IS_GRID(widget)
#else
#define LIVES_GRID(widget) GTK_WIDGET(widget)
#define LIVES_TABLE(widget) GTK_TABLE(widget)
#define LIVES_IS_TABLE(widget) GTK_IS_TABLE(widget)
#define LIVES_IS_GRID(widget) FALSE
#endif

#define LIVES_IS_LIST_STORE(object) GTK_IS_LIST_STORE(object)
#define LIVES_IS_TREE_STORE(object) GTK_IS_TREE_STORE(object)

#define LIVES_LAYOUT LIVES_TABLE

#define LIVES_RANGE(widget) GTK_RANGE(widget)

#define LIVES_EDITABLE(widget) GTK_EDITABLE(widget)

#define LIVES_XEVENT(event) ((GdkEvent *)(event))

#define LIVES_IS_WIDGET_OBJECT(object) G_IS_OBJECT(object)
#define LIVES_IS_WIDGET(widget) GTK_IS_WIDGET(widget)
#define LIVES_IS_WINDOW(widget) GTK_IS_WINDOW(widget)
#if GTK_CHECK_VERSION(4, 0, 0)
#define LIVES_IS_XWINDOW(widget) GDK_IS_SURFACE(widget)
#else
#define LIVES_IS_XWINDOW(widget) GDK_IS_WINDOW(widget)
#endif
#define LIVES_IS_PIXBUF(widget) GDK_IS_PIXBUF(widget)
#define LIVES_IS_CONTAINER(widget) GTK_IS_CONTAINER(widget)
#define LIVES_IS_BIN(widget) GTK_IS_BIN(widget)
#define LIVES_IS_SCROLLBAR(widget) GTK_IS_SCROLLBAR(widget)
#define LIVES_IS_TOOL_BUTTON(widget) GTK_IS_TOOL_BUTTON(widget)

#if GTK_CHECK_VERSION(3, 0, 0)
#define LIVES_IS_HBOX(widget) (GTK_IS_BOX(widget) && gtk_orientable_get_orientation(GTK_ORIENTABLE(widget)) \
			       == GTK_ORIENTATION_HORIZONTAL)
#define LIVES_IS_VBOX(widget) (GTK_IS_BOX(widget) && gtk_orientable_get_orientation(GTK_ORIENTABLE(widget)) \
			       == GTK_ORIENTATION_VERTICAL)
#define LIVES_IS_SCROLLABLE(widget) GTK_IS_SCROLLABLE(widget)
#else
#define LIVES_IS_HBOX(widget) GTK_IS_HBOX(widget)
#define LIVES_IS_VBOX(widget) GTK_IS_VBOX(widget)
#endif

#ifdef LIVES_HAS_SWITCH_WIDGET
#define LIVES_IS_SWITCH(widget) GTK_IS_SWITCH(widget)
#else
#define LIVES_IS_SWITCH(widget) 0
#endif

#define LIVES_IS_BOX(widget) (LIVES_IS_HBOX(widget) || LIVES_IS_VBOX(widget))

#define LIVES_IS_FRAME(widget) GTK_IS_FRAME(widget)

#define LIVES_IS_TOOLBAR(widget) GTK_IS_TOOLBAR(widget)
#define LIVES_IS_EVENT_BOX(widget) GTK_IS_EVENT_BOX(widget)

#define LIVES_IS_COMBO(widget) GTK_IS_COMBO_BOX(widget)
#define LIVES_IS_DIALOG(widget) GTK_IS_DIALOG(widget)
#define LIVES_IS_FILE_CHOOSER_DIALOG(widget) GTK_IS_FILE_CHOOSER_DIALOG(widget)
#define LIVES_IS_LABEL(widget) GTK_IS_LABEL(widget)
#define LIVES_IS_BUTTON(widget) GTK_IS_BUTTON(widget)
#define LIVES_IS_DRAWING_AREA(widget) GTK_IS_DRAWING_AREA(widget)
#define LIVES_IS_SPIN_BUTTON(widget) GTK_IS_SPIN_BUTTON(widget)
#if LIVES_HAS_SWITCH_WIDGET
#define LIVES_IS_TOGGLE_BUTTON(widget) (GTK_IS_SWITCH(widget) ? 1 : GTK_IS_TOGGLE_BUTTON(widget))
#else
#define LIVES_IS_TOGGLE_BUTTON(widget) GTK_IS_TOGGLE_BUTTON(widget)
#endif
#define LIVES_IS_TOGGLE_TOOL_BUTTON(widget) GTK_IS_TOGGLE_TOOL_BUTTON(widget)
#define LIVES_IS_IMAGE(widget) GTK_IS_IMAGE(widget)
#define LIVES_IS_ENTRY(widget) GTK_IS_ENTRY(widget)
#define LIVES_IS_RANGE(widget) GTK_IS_RANGE(widget)
#ifdef PROGBAR_IS_ENTRY
#define LIVES_IS_PROGRESS_BAR(widget) GTK_IS_ENTRY(widget)
#else
#define LIVES_IS_PROGRESS_BAR(widget) GTK_IS_PROGRESS_BAR(widget)
#endif
#define LIVES_IS_TEXT_VIEW(widget) GTK_IS_TEXT_VIEW(widget)
#define LIVES_IS_MENU_ITEM(widget) GTK_IS_MENU_ITEM(widget)
#define LIVES_IS_SEPARATOR_MENU_ITEM(widget) GTK_IS_SEPARATOR_MENU_ITEM(widget)
#define LIVES_IS_MENU_BAR(widget) GTK_IS_MENU_BAR(widget)
#define LIVES_IS_CHECK_MENU_ITEM(widget) GTK_IS_CHECK_MENU_ITEM(widget)
#define LIVES_IS_FILE_CHOOSER(widget) GTK_IS_FILE_CHOOSER(widget)
#define LIVES_IS_BUTTON_BOX(widget) GTK_IS_BUTTON_BOX(widget)
#define LIVES_IS_SCALE(widget) GTK_IS_SCALE(widget)

#define LIVES_IS_ADJUSTMENT(adj) GTK_IS_ADJUSTMENT(adj)

  // (image resize) interpolation types
#define LIVES_INTERP_BEST   GDK_INTERP_HYPER
#define LIVES_INTERP_NORMAL GDK_INTERP_BILINEAR
#define LIVES_INTERP_FAST   GDK_INTERP_NEAREST

#if GTK_CHECK_VERSION(3, 10, 0)
#define STOCK_ALTS_MEDIA_PAUSE 0
#define STOCK_ALTS_KEEP 1
#define N_STOCK_ALTS 2

#define GET_STOCK_ALT(stock_name) (lives_get_stock_icon_alt((stock_name)))

#define LIVES_STOCK_YES "gtk-yes"
#define LIVES_STOCK_NO "gtk-no"
#define LIVES_STOCK_APPLY "gtk-apply"
#define LIVES_STOCK_CANCEL "gtk-cancel"
#define LIVES_STOCK_OK "gtk-ok"
#define LIVES_STOCK_EDIT "gtk-edit"

#define LIVES_STOCK_UNDO "edit-undo"
#define LIVES_STOCK_REDO "edit-redo"
#define LIVES_STOCK_ADD "list-add"
#define LIVES_STOCK_REMOVE "list-remove"
#define LIVES_STOCK_NO2 "media-record"
#define LIVES_STOCK_QUIT "application-exit"
#define LIVES_STOCK_OPEN "document-open"
#define LIVES_STOCK_CLOSE "window-close"
#define LIVES_STOCK_CLEAR "edit-clear"
#define LIVES_STOCK_DELETE "edit-delete"
#define LIVES_STOCK_SAVE_AS "document-save-as"
#define LIVES_STOCK_SAVE "document-save"
#define LIVES_STOCK_REFRESH "view-refresh"
#define LIVES_STOCK_REVERT_TO_SAVED "document-revert"
#define LIVES_STOCK_GO_BACK "go-previous"
#define LIVES_STOCK_GO_FORWARD "go-next"
#define LIVES_STOCK_GO_UP "go-up"
#define LIVES_STOCK_GO_DOWN "go-down"
#define LIVES_STOCK_STOP "stop"
#define LIVES_STOCK_SELECT_ALL "edit-select-all"
#define LIVES_STOCK_MEDIA_PLAY "media-playback-start"
#define LIVES_STOCK_MEDIA_STOP "media-playback-stop"
#define LIVES_STOCK_MEDIA_REWIND "media-seek-backward"
#define LIVES_STOCK_MEDIA_FORWARD "media-seek-forward"
#define LIVES_STOCK_MEDIA_RECORD "media-record"

#define LIVES_STOCK_LOOP "system-reboot"

#define LIVES_STOCK_PREFERENCES "preferences-system"
#define LIVES_STOCK_DIALOG_INFO "gtk-dialog-info"
#define LIVES_STOCK_DIALOG_WARNING "dialog-warning"
#define LIVES_STOCK_DIALOG_QUESTION "dialog-question"
#define LIVES_STOCK_MISSING_IMAGE "image-missing"

#define LIVES_STOCK_MEDIA_PAUSE_ALT_1 "media-playback-pause"
#define LIVES_STOCK_MEDIA_PAUSE_ALT_2 "media-pause"
#define LIVES_STOCK_MEDIA_PAUSE GET_STOCK_ALT(STOCK_ALTS_MEDIA_PAUSE)

#define LIVES_STOCK_KEEP_ALT_1 "gtk-jump"
#define LIVES_STOCK_KEEP_ALT_2 "gtk-jump-to-ltr"
#define LIVES_STOCK_KEEP GET_STOCK_ALT(STOCK_ALTS_KEEP)

#else

#define LIVES_STOCK_UNDO GTK_STOCK_UNDO
#define LIVES_STOCK_REDO GTK_STOCK_REDO
#define LIVES_STOCK_ADD GTK_STOCK_ADD
#define LIVES_STOCK_APPLY GTK_STOCK_APPLY
#define LIVES_STOCK_REMOVE GTK_STOCK_REMOVE
#define LIVES_STOCK_NO GTK_STOCK_NO
#define LIVES_STOCK_YES GTK_STOCK_YES
#define LIVES_STOCK_KEEP GTK_STOCK_KEEP
#define LIVES_STOCK_QUIT GTK_STOCK_QUIT
#define LIVES_STOCK_OPEN GTK_STOCK_OPEN
#define LIVES_STOCK_CLOSE GTK_STOCK_CLOSE
#define LIVES_STOCK_CANCEL GTK_STOCK_CANCEL
#define LIVES_STOCK_OK GTK_STOCK_OK
#define LIVES_STOCK_CLEAR GTK_STOCK_CLEAR
#define LIVES_STOCK_DELETE GTK_STOCK_DELETE
#define LIVES_STOCK_SAVE_AS GTK_STOCK_SAVE_AS
#define LIVES_STOCK_SAVE GTK_STOCK_SAVE
#define LIVES_STOCK_REFRESH GTK_STOCK_REFRESH
#define LIVES_STOCK_REVERT_TO_SAVED GTK_STOCK_REVERT_TO_SAVED
#define LIVES_STOCK_GO_BACK GTK_STOCK_GO_BACK
#define LIVES_STOCK_GO_FORWARD GTK_STOCK_GO_FORWARD
#define LIVES_STOCK_REFRESH GTK_STOCK_REFRESH
#define LIVES_STOCK_SELECT_ALL GTK_STOCK_SELECT_ALL
#define LIVES_STOCK_PREFERENCES GTK_STOCK_PREFERENCES
#define LIVES_STOCK_DIALOG_INFO GTK_STOCK_DIALOG_INFO
#define LIVES_STOCK_DIALOG_WARNING GTK_STOCK_DIALOG_WARNING
#define LIVES_STOCK_DIALOG_QUESTION GTK_STOCK_DIALOG_QUESTION
#define LIVES_STOCK_MISSING_IMAGE GTK_STOCK_MISSING_IMAGE

#if GTK_CHECK_VERSION(2, 6, 0)
#define LIVES_STOCK_MEDIA_PAUSE GTK_STOCK_MEDIA_PAUSE
#else
#define LIVES_STOCK_MEDIA_PAUSE GTK_STOCK_REFRESH
#endif

#if GTK_CHECK_VERSION(2, 6, 0)
#define LIVES_STOCK_MEDIA_PLAY GTK_STOCK_MEDIA_PLAY
#else
#define LIVES_STOCK_MEDIA_PLAY GTK_STOCK_GO_FORWARD
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
#define LIVES_STOCK_MEDIA_STOP GTK_STOCK_MEDIA_STOP
#else
#define LIVES_STOCK_MEDIA_STOP GTK_STOCK_STOP
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
#define LIVES_STOCK_MEDIA_REWIND GTK_STOCK_MEDIA_REWIND
#else
#define LIVES_STOCK_MEDIA_REWIND GTK_STOCK_GOTO_FIRST
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
#define LIVES_STOCK_MEDIA_FORWARD GTK_STOCK_MEDIA_FORWARD
#else
#define LIVES_STOCK_MEDIA_FORWARD GTK_STOCK_GOTO_LAST
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
#define LIVES_STOCK_MEDIA_RECORD GTK_STOCK_MEDIA_RECORD
#else
#define LIVES_STOCK_MEDIA_RECORD GTK_STOCK_NO
#endif

#endif

#define LIVES_LIVES_STOCK_HELP_INFO "livestock-help-info"

#if GTK_CHECK_VERSION(3, 2, 0)
#define LIVES_LIVES_STOCK_LOCKED "changes-prevent"
#define LIVES_LIVES_STOCK_UNLOCKED "changes-allow"
#else
#define LIVES_LIVES_STOCK_LOCKED "locked"
#define LIVES_LIVES_STOCK_UNLOCKED "unlocked"
#endif

#define LIVES_DEFAULT_MOD_MASK (gtk_accelerator_get_default_mod_mask ())

#define LIVES_CONTROL_MASK GDK_CONTROL_MASK // 4
#define LIVES_ALT_MASK     GDK_MOD1_MASK  // 8
#define LIVES_NUMLOCK_MASK     GDK_MOD2_MASK  // 16
#define LIVES_SHIFT_MASK   GDK_SHIFT_MASK  // 1
#define LIVES_LOCK_MASK    GDK_LOCK_MASK // 2 (capslock)

#define LIVES_BUTTON1_MASK    GDK_BUTTON1_MASK
#define LIVES_BUTTON2_MASK    GDK_BUTTON2_MASK
#define LIVES_BUTTON3_MASK    GDK_BUTTON3_MASK
#define LIVES_BUTTON4_MASK    GDK_BUTTON4_MASK
#define LIVES_BUTTON5_MASK    GDK_BUTTON5_MASK

#define LIVES_SPECIAL_MASK (1 << 31)

#ifdef GDK_KEY_a
#define LIVES_KEY_Left GDK_KEY_Left
#define LIVES_KEY_Right GDK_KEY_Right
#define LIVES_KEY_Up GDK_KEY_Up
#define LIVES_KEY_Down GDK_KEY_Down

#define LIVES_KEY_BackSpace GDK_KEY_BackSpace
#define LIVES_KEY_Return GDK_KEY_Return
#define LIVES_KEY_Tab GDK_KEY_Tab
#define LIVES_KEY_Home GDK_KEY_Home
#define LIVES_KEY_End GDK_KEY_End
#define LIVES_KEY_Underscore GDK_KEY_underscore
#define LIVES_KEY_Slash GDK_KEY_slash
#define LIVES_KEY_Space GDK_KEY_space
#define LIVES_KEY_Plus GDK_KEY_plus
#define LIVES_KEY_Less GDK_KEY_less
#define LIVES_KEY_Greater GDK_KEY_greater
#define LIVES_KEY_Minus GDK_KEY_minus
#define LIVES_KEY_Equal GDK_KEY_equal
#define LIVES_KEY_Delete GDK_KEY_Delete

#define LIVES_KEY_1 GDK_KEY_1
#define LIVES_KEY_2 GDK_KEY_2
#define LIVES_KEY_3 GDK_KEY_3
#define LIVES_KEY_4 GDK_KEY_4
#define LIVES_KEY_5 GDK_KEY_5
#define LIVES_KEY_6 GDK_KEY_6
#define LIVES_KEY_7 GDK_KEY_7
#define LIVES_KEY_8 GDK_KEY_8
#define LIVES_KEY_9 GDK_KEY_9
#define LIVES_KEY_0 GDK_KEY_0

#define LIVES_KEY_a GDK_KEY_a
#define LIVES_KEY_b GDK_KEY_b
#define LIVES_KEY_c GDK_KEY_c
#define LIVES_KEY_d GDK_KEY_d
#define LIVES_KEY_e GDK_KEY_e
#define LIVES_KEY_f GDK_KEY_f
#define LIVES_KEY_g GDK_KEY_g
#define LIVES_KEY_h GDK_KEY_h
#define LIVES_KEY_i GDK_KEY_i
#define LIVES_KEY_j GDK_KEY_j
#define LIVES_KEY_k GDK_KEY_k
#define LIVES_KEY_l GDK_KEY_l
#define LIVES_KEY_m GDK_KEY_m
#define LIVES_KEY_n GDK_KEY_n
#define LIVES_KEY_o GDK_KEY_o
#define LIVES_KEY_p GDK_KEY_p
#define LIVES_KEY_q GDK_KEY_q
#define LIVES_KEY_r GDK_KEY_r
#define LIVES_KEY_s GDK_KEY_s
#define LIVES_KEY_t GDK_KEY_t
#define LIVES_KEY_u GDK_KEY_u
#define LIVES_KEY_v GDK_KEY_v
#define LIVES_KEY_w GDK_KEY_w
#define LIVES_KEY_x GDK_KEY_x
#define LIVES_KEY_y GDK_KEY_y
#define LIVES_KEY_z GDK_KEY_z

#define LIVES_KEY_F1 GDK_KEY_F1
#define LIVES_KEY_F2 GDK_KEY_F2
#define LIVES_KEY_F3 GDK_KEY_F3
#define LIVES_KEY_F4 GDK_KEY_F4
#define LIVES_KEY_F5 GDK_KEY_F5
#define LIVES_KEY_F6 GDK_KEY_F6
#define LIVES_KEY_F7 GDK_KEY_F7
#define LIVES_KEY_F8 GDK_KEY_F8
#define LIVES_KEY_F9 GDK_KEY_F9
#define LIVES_KEY_F10 GDK_KEY_F10
#define LIVES_KEY_F11 GDK_KEY_F11
#define LIVES_KEY_F12 GDK_KEY_F12

#define LIVES_KEY_Page_Up GDK_KEY_Page_Up
#define LIVES_KEY_Page_Down GDK_KEY_Page_Down
#define LIVES_KEY_KP_Page_Up GDK_KEY_KP_Page_Up
#define LIVES_KEY_KP_Page_Down GDK_KEY_KP_Page_Down

#define LIVES_KEY_Escape GDK_KEY_Escape

#else
#define LIVES_KEY_Left GDK_Left
#define LIVES_KEY_Right GDK_Right
#define LIVES_KEY_Up GDK_Up
#define LIVES_KEY_Down GDK_Down

#define LIVES_KEY_BackSpace GDK_BackSpace
#define LIVES_KEY_Return GDK_Return
#define LIVES_KEY_Tab GDK_Tab
#define LIVES_KEY_Underscore GDK_underscore
#define LIVES_KEY_Home GDK_Home
#define LIVES_KEY_End GDK_End
#define LIVES_KEY_Slash GDK_slash
#define LIVES_KEY_Space GDK_space
#define LIVES_KEY_Plus GDK_plus
#define LIVES_KEY_Less GDK_less
#define LIVES_KEY_Greater GDK_greater
#define LIVES_KEY_Minus GDK_minus
#define LIVES_KEY_Equal GDK_equal
#define LIVES_KEY_Delete GDK_delete

#define LIVES_KEY_1 GDK_1
#define LIVES_KEY_2 GDK_2
#define LIVES_KEY_3 GDK_3
#define LIVES_KEY_4 GDK_4
#define LIVES_KEY_5 GDK_5
#define LIVES_KEY_6 GDK_6
#define LIVES_KEY_7 GDK_7
#define LIVES_KEY_8 GDK_8
#define LIVES_KEY_9 GDK_9
#define LIVES_KEY_0 GDK_0

#define LIVES_KEY_a GDK_a
#define LIVES_KEY_b GDK_b
#define LIVES_KEY_c GDK_c
#define LIVES_KEY_d GDK_d
#define LIVES_KEY_e GDK_e
#define LIVES_KEY_f GDK_f
#define LIVES_KEY_g GDK_g
#define LIVES_KEY_h GDK_h
#define LIVES_KEY_i GDK_i
#define LIVES_KEY_j GDK_j
#define LIVES_KEY_k GDK_k
#define LIVES_KEY_l GDK_l
#define LIVES_KEY_m GDK_m
#define LIVES_KEY_n GDK_n
#define LIVES_KEY_o GDK_o
#define LIVES_KEY_p GDK_p
#define LIVES_KEY_q GDK_q
#define LIVES_KEY_r GDK_r
#define LIVES_KEY_s GDK_s
#define LIVES_KEY_t GDK_t
#define LIVES_KEY_u GDK_u
#define LIVES_KEY_v GDK_v
#define LIVES_KEY_w GDK_w
#define LIVES_KEY_x GDK_x
#define LIVES_KEY_y GDK_y
#define LIVES_KEY_z GDK_z

#define LIVES_KEY_F1 GDK_F1
#define LIVES_KEY_F2 GDK_F2
#define LIVES_KEY_F3 GDK_F3
#define LIVES_KEY_F4 GDK_F4
#define LIVES_KEY_F5 GDK_F5
#define LIVES_KEY_F6 GDK_F6
#define LIVES_KEY_F7 GDK_F7
#define LIVES_KEY_F8 GDK_F8
#define LIVES_KEY_F9 GDK_F9
#define LIVES_KEY_F10 GDK_F10
#define LIVES_KEY_F11 GDK_F11
#define LIVES_KEY_F12 GDK_F12

#define LIVES_KEY_Page_Up GDK_Page_Up
#define LIVES_KEY_Page_Down GDK_Page_Down

#define LIVES_KEY_Escape GDK_Escape

#endif

#define LIVES_INT_TO_POINTER  GINT_TO_POINTER
#define LIVES_UINT_TO_POINTER GUINT_TO_POINTER
#define LIVES_POINTER_TO_INT  GPOINTER_TO_INT

#if GTK_CHECK_VERSION(3, 10, 0)
  struct fc_dissection {
    LiVESList *entry_list;
    LiVESWidget *sidebar;
    LiVESWidget *bbox;
    LiVESWidget *revealer;
    LiVESWidget *treeview;
    LiVESWidget *selbut;
    LiVESWidget *old_entry, *new_entry;
  };
#endif

#endif

#endif
