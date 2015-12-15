// widget-helper-gtk.h
// LiVES
// (c) G. Finch 2012 - 2015 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// widget implementation for GTK+

#ifndef HAS_LIVES_WIDGET_HELPER_GTK_H
#define HAS_LIVES_WIDGET_HELPER_GTK_H

#ifdef GUI_GTK


#define GTK_RADIO_MENU_BUG // a bug where gtk_radio_menu_item_set_active() does not update visually
#define GTK_SUBMENU_SENS_BUG // a bug where setting a menuitem insensitive fails if it has a submenu


#ifndef IS_MINGW
typedef gboolean                          boolean;
#endif

#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>

#ifndef GDK_IS_WIN32_DISPLAY
#define GDK_IS_WIN32_DISPLAY(display) (TRUE)
#endif

#endif //GDK_WINDOWING_WIN32

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


#else

#ifndef GDK_IS_X11_DISPLAY
#define GDK_IS_X11_DISPLAY(display) (FALSE)
#endif

#endif // GDK_WINDOWING_X11

#endif // GUI_GTK


#ifdef PAINTER_CAIRO

#ifndef GUI_GTK
#include <cairo/cairo.h>
#endif


typedef cairo_t lives_painter_t;
typedef cairo_surface_t lives_painter_surface_t;

boolean lives_painter_surface_destroy(lives_painter_surface_t *);


typedef cairo_format_t lives_painter_format_t;

#define LIVES_PAINTER_FORMAT_A1   CAIRO_FORMAT_A1
#define LIVES_PAINTER_FORMAT_A8   CAIRO_FORMAT_A8
#define LIVES_PAINTER_FORMAT_ARGB32 CAIRO_FORMAT_ARGB32


typedef cairo_content_t lives_painter_content_t; // eg. color, alpha, color+alpha

#define LIVES_PAINTER_CONTENT_COLOR CAIRO_CONTENT_COLOR


typedef cairo_operator_t lives_painter_operator_t;

#define LIVES_PAINTER_OPERATOR_UNKNOWN CAIRO_OPERATOR_OVER
#define LIVES_PAINTER_OPERATOR_DEFAULT CAIRO_OPERATOR_OVER

#define LIVES_PAINTER_OPERATOR_DEST_OUT CAIRO_OPERATOR_DEST_OUT
#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1, 10, 0)
#define LIVES_PAINTER_OPERATOR_DIFFERENCE CAIRO_OPERATOR_OVER
#define LIVES_PAINTER_OPERATOR_OVERLAY CAIRO_OPERATOR_OVER
#else
#define LIVES_PAINTER_OPERATOR_DIFFERENCE CAIRO_OPERATOR_DIFFERENCE
#define LIVES_PAINTER_OPERATOR_OVERLAY CAIRO_OPERATOR_OVERLAY
#endif

typedef cairo_fill_rule_t lives_painter_fill_rule_t;

#define LIVES_PAINTER_FILL_RULE_WINDING  CAIRO_FILL_RULE_WINDING
#define LIVES_PAINTER_FILL_RULE_EVEN_ODD CAIRO_FILL_RULE_EVEN_ODD


#endif


#ifdef GUI_GTK

#include "support.h"



/// Glib type stuff //////////////////////////////////////////
#ifndef G_ENCODE_VERSION
#define G_ENCODE_VERSION(major,minor) ((major) << 16 | (minor) << 8)
#endif

#define lives_free(a) g_free(a)
#define lives_malloc(a) g_malloc(a)
#define lives_malloc0(a) g_malloc0(a)
#define lives_realloc(a,b) g_realloc(a,b)
#define lives_try_malloc0_n(a,b) g_try_malloc0_n(a,b)
#define lives_try_malloc(a) g_try_malloc(a)
#define lives_try_malloc0(a) g_try_malloc0(a)
#define lives_try_realloc(a,b) g_try_realloc(a,b)


typedef GError                            LiVESError;

typedef GList                             LiVESList;
typedef GSList                            LiVESSList;

typedef gpointer                          livespointer;
typedef gconstpointer                     livesconstpointer;

typedef GClosure                          LiVESWidgetClosure;

typedef GObject LiVESObject;

typedef GLogLevelFlags LiVESLogLevelFlags;

#define LIVES_LOG_LEVEL_WARNING G_LOG_LEVEL_WARNING
#define LIVES_LOG_LEVEL_MASK G_LOG_LEVEL_MASK
#define LIVES_LOG_LEVEL_CRITICAL G_LOG_LEVEL_CRITICAL
#define LIVES_LOG_FATAL_MASK G_LOG_FATAL_MASK

//////////////////////////////////////////////////

#if GTK_CHECK_VERSION(3,10,0)
#define LIVES_TABLE_IS_GRID 1
#endif

#define return_true gtk_true

typedef void (*LiVESGuiCallback)(void);
typedef void (*LiVESWidgetCallback)(GtkWidget *widget, gpointer data);
typedef gboolean(*LiVESWidgetSourceFunc)(gpointer data);
typedef gint(*LiVESCompareFunc)(gconstpointer a, gconstpointer b);

#define LIVES_LITTLE_ENDIAN G_LITTLE_ENDIAN
#define LIVES_BIG_ENDIAN G_BIG_ENDIAN

#define LIVES_MAXINT G_MAXINT
#define LIVES_MAXUINT32 G_MAXUINT32
#define LIVES_MAXSIZE G_MAXSIZE
#define LIVES_MAXFLOAT G_MAXFLOAT

#define LIVES_IS_RTL (gtk_widget_get_default_direction()==GTK_TEXT_DIR_RTL)

#define LIVES_GUI_CALLBACK(f) ((LiVESGuiCallback) (f))

#define lives_printerr(args...) g_printerr(args)
#define lives_strdup_printf(args...) g_strdup_printf(args)
#define lives_strdup_vprintf(args...) g_strdup_vprintf(args)
#define lives_strndup_printf(args...) g_strndup_printf(args)
#define lives_strndup(a,b) g_strndup(a,b)
#define lives_snprintf(a,b,args...) g_snprintf(a,b,args)
#define lives_strsplit(a,b,c) g_strsplit(a,b,c)
#define lives_strfreev(a) g_strfreev(a)
#define lives_ascii_strcasecmp(a,b) g_ascii_strcasecmp(a,b)
#define lives_ascii_strncasecmp(a,b,c) g_ascii_strncasecmp(a,b,c)
#define lives_strconcat(a,args...) g_strconcat(a,args)
#define lives_strstrip(a) g_strstrip(a)
#define lives_strrstr(a,b) g_strrstr(a,b)
#define lives_strstr_len(a,b,c) g_strstr_len(a,b,c)
#define lives_strdelimit(a,b,c) g_strdelimit(a,b,c)

#define LIVES_NORMALIZE_DEFAULT G_NORMALIZE_DEFAULT

#define lives_utf8_normalize(a,b,c) g_utf8_normalize(a,b,c)


#define lives_list_nth_data(list,i) g_list_nth_data(list,i)
#define lives_list_nth(list,i) g_list_nth(list,i)
#define lives_list_length(list) g_list_length(list)
#define lives_list_free(list) g_list_free(list)
#define lives_list_append(list,data) g_list_append(list,data)
#define lives_list_prepend(list,data) g_list_prepend(list,data)
#define lives_list_find(list,data) g_list_find(list,data)
#define lives_list_previous(list) g_list_previous(list)
#define lives_list_last(list) g_list_last(list)
#define lives_list_delete_link(list,link) g_list_delete_link(list,link)
#define lives_list_copy(list) g_list_copy(list)
#define lives_list_next(list) g_list_next(list)
#define lives_list_first(list) g_list_first(list)
#define lives_list_remove(list,data) g_list_remove(list,data)
#define lives_list_remove_link(list,data) g_list_remove_link(list,data)
#define lives_list_concat(list,data) g_list_concat(list,data)
#define lives_list_insert(list,data,pos) g_list_insert(list,data,pos)
#define lives_list_index(list,data) g_list_index(list,data)
#define lives_list_find_custom(list,data,func) g_list_find_custom(list,data,func)

#define lives_slist_free(list) g_slist_free(list)
#define lives_slist_length(list) g_slist_length(list)
#define lives_slist_nth_data(list,i) g_slist_nth_data(list,i)
#define lives_slist_append(list,data) g_slist_append(list,data)

#define lives_build_filename(args...) g_build_filename(args)
#define lives_filename_to_utf8(a,b,c,d,e) g_filename_to_utf8(a,b,c,d,e)
#define lives_filename_from_utf8(a,b,c,d,e) g_filename_from_utf8(a,b,c,d,e)

#define lives_utf8_strdown(a,b) g_utf8_strdown(a,b)

#define lives_find_program_in_path(a) g_find_program_in_path(a)

#define lives_idle_add(a,b) g_idle_add(a,b)
#define lives_idle_add_full(a,b,c,d) g_idle_add_full(a,b,c,d)

#define lives_set_application_name(a) g_set_application_name(a)
#define lives_get_application_name() g_get_application_name()

#define lives_usleep(a) g_usleep(a)

#define lives_mkdir_with_parents(a,b) g_mkdir_with_parents(a,b)

#define lives_strtod(a,b) g_strtod(a,b)

#define lives_path_get_basename(a) g_path_get_basename(a)

#define LIVES_UNLIKELY(a) G_UNLIKELY(a)
#define LIVES_LIKELY(a) G_LIKELY(a)

#define lives_file_test(a,b) g_file_test(a,b)

#define lives_get_current_dir() g_get_current_dir()

#define lives_error_free(a) g_error_free(a)

#define lives_strerror(a) g_strerror(a)

#define lives_cclosure_new(a,b,c) g_cclosure_new(a,b,c)

#define lives_path_get_dirname(a) g_path_get_dirname(a)

#define U82L(String) ( g_locale_from_utf8 (String,-1,NULL,NULL,NULL) )
#define L2U8(String) ( g_locale_to_utf8 (String,-1,NULL,NULL,NULL) )

#define U82F(String) ( g_filename_from_utf8 (String,-1,NULL,NULL,NULL) )
#define F2U8(String) ( g_filename_to_utf8 (String,-1,NULL,NULL,NULL) )


#define LIVES_FILE_TEST_EXISTS G_FILE_TEST_EXISTS
#define LIVES_FILE_TEST_IS_DIR G_FILE_TEST_IS_DIR
#define LIVES_FILE_TEST_IS_REGULAR G_FILE_TEST_IS_REGULAR

#define LIVES_DIR_SEPARATOR_S G_DIR_SEPARATOR_S

typedef GtkJustification LiVESJustification;

#define LIVES_JUSTIFY_LEFT   GTK_JUSTIFY_LEFT
#define LIVES_JUSTIFY_RIGHT  GTK_JUSTIFY_RIGHT
#define LIVES_JUSTIFY_CENTER GTK_JUSTIFY_CENTER
#define LIVES_JUSTIFY_FILL   GTK_JUSTIFY_RIGHT

typedef GtkOrientation LiVESOrientation;
#define LIVES_ORIENTATION_HORIZONTAL GTK_ORIENTATION_HORIZONTAL
#define LIVES_ORIENTATION_VERTICAL   GTK_ORIENTATION_VERTICAL

typedef GdkEvent                          LiVESXEvent;
typedef GdkXEvent                         LiVESXXEvent;
typedef GdkEventButton                    LiVESXEventButton;
typedef GdkEventMotion                    LiVESXEventMotion;
typedef GdkEventScroll                    LiVESXEventScroll;
typedef GdkEventExpose                    LiVESXEventExpose;
typedef GdkEventCrossing                  LiVESXEventCrossing;
typedef GdkEventConfigure                 LiVESXEventConfigure;
typedef GdkEventFocus                     LiVESXEventFocus;
typedef GdkEvent                          LiVESXEventDelete;
typedef GdkDisplay                        LiVESXDisplay;
typedef GdkScreen                         LiVESXScreen;
typedef GdkDevice                         LiVESXDevice;


#define LIVES_SCROLL_UP   GDK_SCROLL_UP
#define LIVES_SCROLL_DOWN GDK_SCROLL_DOWN

#if GTK_CHECK_VERSION(3,0,0)
#undef LIVES_HAS_DEVICE_MANAGER
#define LIVES_HAS_DEVICE_MANAGER 1
typedef GdkDeviceManager                  LiVESXDeviceManager;
#endif

#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_WIDGET_EXPOSE_EVENT "draw"
#define LIVES_GUI_OBJECT(a)                     a
#else
#define LIVES_GUI_OBJECT(a)                     GTK_OBJECT(a)
#define LIVES_WIDGET_EXPOSE_EVENT "expose_event"
#define LIVES_GUI_OBJECT_CLASS(a) GTK_OBJECT_CLASS(a)
#endif

#define lives_widget_object_set_data(a, b, c) g_object_set_data(a, b, c)
#define lives_widget_object_get_data(a, b) g_object_get_data(a, b)

#define LIVES_WIDGET_OBJECT(a) G_OBJECT(a)

#if GTK_CHECK_VERSION(3,0,0)
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

// events
#define LIVES_WIDGET_SCROLL_EVENT "scroll-event"
#define LIVES_WIDGET_CONFIGURE_EVENT "configure-event"
#define LIVES_WIDGET_ENTER_EVENT "enter-notify-event"
#define LIVES_WIDGET_BUTTON_PRESS_EVENT "button-press-event"
#define LIVES_WIDGET_BUTTON_RELEASE_EVENT "button-release-event"
#define LIVES_WIDGET_MOTION_NOTIFY_EVENT "motion-notify-event"
#define LIVES_WIDGET_LEAVE_NOTIFY_EVENT "leave-notify-event"
#define LIVES_WIDGET_FOCUS_OUT_EVENT "focus-out-event"
#define LIVES_WIDGET_DELETE_EVENT "delete-event"

// signals
#define LIVES_WIDGET_CLICKED_SIGNAL "clicked"
#define LIVES_WIDGET_TOGGLED_SIGNAL "toggled"
#define LIVES_WIDGET_CHANGED_SIGNAL "changed"
#define LIVES_WIDGET_ACTIVATE_SIGNAL "activate"
#define LIVES_WIDGET_VALUE_CHANGED_SIGNAL "value-changed"
#define LIVES_WIDGET_SELECTION_CHANGED_SIGNAL "selection-changed"
#define LIVES_WIDGET_CURRENT_FOLDER_CHANGED_SIGNAL "current-folder-changed"
#define LIVES_WIDGET_RESPONSE_SIGNAL "response"
#define LIVES_WIDGET_DRAG_DATA_RECEIVED_SIGNAL "drag-data-received"
#define LIVES_WIDGET_SIZE_PREPARED_SIGNAL "size-prepared"
#define LIVES_WIDGET_MODE_CHANGED_SIGNAL "mode-changed"
#define LIVES_WIDGET_ACCEPT_POSITION_SIGNAL "accept-position"
#define LIVES_WIDGET_SWITCH_PAGE_SIGNAL "switch-page"
#define LIVES_WIDGET_UNMAP_SIGNAL "unmap"
#define LIVES_WIDGET_EDITED_SIGNAL "edited"
#define LIVES_WIDGET_ROW_EXPANDED_SIGNAL "row-expanded"
#define LIVES_WIDGET_COLOR_SET_SIGNAL "color-set"
#define LIVES_WIDGET_SET_FOCUS_CHILD_SIGNAL "set-focus-child"

#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_WIDGET_STATE_CHANGED_SIGNAL "state-flags-changed"
#else
#define LIVES_WIDGET_STATE_CHANGED_SIGNAL "state-changed"
#endif


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
typedef GtkToggleButton                   LiVESToggleButton;

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
typedef GtkAlignment                      LiVESAlignment;
typedef GtkMenu                           LiVESMenu;
typedef GtkMenuShell                      LiVESMenuShell;
typedef GtkMenuItem                       LiVESMenuItem;
typedef GtkMenuToolButton                 LiVESMenuToolButton;
typedef GtkCheckMenuItem                  LiVESCheckMenuItem;
typedef GtkImageMenuItem                  LiVESImageMenuItem;
typedef GtkRadioMenuItem                  LiVESRadioMenuItem;

typedef GtkNotebook                       LiVESNotebook;

typedef GtkExpander                       LiVESExpander;

typedef GtkProgressBar                    LiVESProgressBar;

typedef GtkAboutDialog                    LiVESAboutDialog;

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

#if GTK_CHECK_VERSION(2,14,0)
typedef GtkScaleButton                    LiVESScaleButton;
#else
typedef GtkRange                          LiVESScaleButton;
#endif

#if GTK_CHECK_VERSION(3,2,0)
typedef GtkGrid                           LiVESGrid;
#undef LIVES_HAS_GRID_WIDGET
#define LIVES_HAS_GRID_WIDGET 1
#else
typedef LiVESWidget                       LiVESGrid;
#endif

#ifdef LIVES_TABLE_IS_GRID
typedef GtkGrid                           LiVESTable;
#else
typedef GtkTable                          LiVESTable;
#endif

typedef GtkEditable                       LiVESEditable;

#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_WIDGET_COLOR_HAS_ALPHA (1)
#define LIVES_WIDGET_COLOR_SCALE(x) (x) ///< macro to get 0. to 1.
#define LIVES_WIDGET_COLOR_SCALE_255(x) ((double)x/255.) ///< macro to convert from (0. - 255.) to component
typedef GdkRGBA                           LiVESWidgetColor;

typedef GtkStateFlags LiVESWidgetState;

#define LIVES_WIDGET_STATE_NORMAL         GTK_STATE_FLAG_NORMAL
#define LIVES_WIDGET_STATE_ACTIVE         GTK_STATE_FLAG_ACTIVE
#define LIVES_WIDGET_STATE_PRELIGHT       GTK_STATE_FLAG_PRELIGHT
#define LIVES_WIDGET_STATE_SELECTED       GTK_STATE_FLAG_SELECTED
#define LIVES_WIDGET_STATE_INSENSITIVE    GTK_STATE_FLAG_INSENSITIVE
#define LIVES_WIDGET_STATE_INCONSISTENT   GTK_STATE_FLAG_INCONSISTENT
#define LIVES_WIDGET_STATE_FOCUSED        GTK_STATE_FLAG_FOCUSED
#define LIVES_WIDGET_STATE_BACKDROP       GTK_STATE_FLAG_BACKDROP

#else
#define LIVES_WIDGET_COLOR_HAS_ALPHA (0)
#define LIVES_WIDGET_COLOR_SCALE(x) ((double)x/65535.)     ///< macro to get 0. to 1.
#define LIVES_WIDGET_COLOR_SCALE_255(x) ((int)((double)x*256.+.5))     ///< macro to get 0 - 255
typedef GdkColor                          LiVESWidgetColor;
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
#define LIVES_RESPONSE_NONE GTK_RESPONSE_NONE
#define LIVES_RESPONSE_OK GTK_RESPONSE_OK
#define LIVES_RESPONSE_CANCEL GTK_RESPONSE_CANCEL
#define LIVES_RESPONSE_ACCEPT GTK_RESPONSE_ACCEPT
#define LIVES_RESPONSE_YES GTK_RESPONSE_YES
#define LIVES_RESPONSE_NO GTK_RESPONSE_NO

// positive values for custom responses
#define LIVES_RESPONSE_INVALID 0
#define LIVES_RESPONSE_RETRY 1
#define LIVES_RESPONSE_ABORT 2
#define LIVES_RESPONSE_RESET 3
#define LIVES_RESPONSE_SHOW_DETAILS 4


typedef GtkAttachOptions LiVESAttachOptions;
#define LIVES_EXPAND GTK_EXPAND
#define LIVES_SHRINK GTK_SHRINK
#define LIVES_FILL GTK_FILL


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
#define LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE ((GtkFileChooserAction)(GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER+1))


typedef GtkIconSize LiVESIconSize;
#define LIVES_ICON_SIZE_INVALID GTK_ICON_SIZE_INVALID
#define LIVES_ICON_SIZE_MENU GTK_ICON_SIZE_MENU
#define LIVES_ICON_SIZE_SMALL_TOOLBAR GTK_ICON_SIZE_SMALL_TOOLBAR
#define LIVES_ICON_SIZE_LARGE_TOOLBAR GTK_ICON_SIZE_LARGE_TOOLBAR
#define LIVES_ICON_SIZE_BUTTON GTK_ICON_SIZE_BUTTON
#define LIVES_ICON_SIZE_DND GTK_ICON_SIZE_DND
#define LIVES_ICON_SIZE_DIALOG GTK_ICON_SIZE_DIALOG



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
#define LIVES_BUTTONBOX_EDGE GTK_BUTTONBOX_EDGE
#define LIVES_BUTTONBOX_START GTK_BUTTONBOX_START
#define LIVES_BUTTONBOX_END GTK_BUTTONBOX_END
#define LIVES_BUTTONBOX_CENTER GTK_BUTTONBOX_CENTER


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

#if GTK_CHECK_VERSION(3,4,0)
#define LIVES_TOUCH_MASK GDK_TOUCH_MASK
#define LIVES_SMOOTH_SCROLL_MASK GDK_SMOOTH_SCROLL_MASK
#endif

#define LIVES_ALL_EVENTS_MASK GDK_ALL_EVENTS_MASK


typedef GtkShadowType LiVESShadowType;
#define LIVES_SHADOW_NONE GTK_SHADOW_NONE
#define LIVES_SHADOW_IN GTK_SHADOW_IN
#define LIVES_SHADOW_OUT GTK_SHADOW_OUT
#define LIVES_SHADOW_ETCHED_IN GTK_SHADOW_ETCHED_IN
#define LIVES_SHADOW_ETCHED_OUT GTK_SHADOW_ETCHED_OUT

typedef GtkWindowPosition LiVESWindowPosition;
#define LIVES_WIN_POS_CENTER_ALWAYS GTK_WIN_POS_CENTER_ALWAYS


#if GTK_CHECK_VERSION(3,0,0)
typedef GtkScale                          LiVESRuler;
typedef GtkBox                            LiVESVBox;
typedef GtkBox                            LiVESHBox;
#else
typedef GtkRuler                          LiVESRuler;
typedef GtkVBox                           LiVESVBox;
typedef GtkHBox                           LiVESHBox;
#endif

typedef GtkEventBox                       LiVESEventBox;

typedef GtkRange                          LiVESRange;

typedef GtkAdjustment                     LiVESAdjustment;

typedef GdkPixbuf                         LiVESPixbuf;

typedef GdkWindow                         LiVESXWindow;

typedef GdkCursor                         LiVESXCursor;

typedef GdkModifierType                   LiVESXModifierType;

typedef GtkAccelGroup                     LiVESAccelGroup;
typedef GtkAccelFlags                     LiVESAccelFlags;

typedef GtkRequisition                    LiVESRequisition;

typedef GtkPaned                          LiVESPaned;

typedef GtkScale                          LiVESScale;

typedef GdkPixbufDestroyNotify            LiVESPixbufDestroyNotify;

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
#define LIVES_RADIO_BUTTON(widget) GTK_RADIO_BUTTON(widget)
#define LIVES_SPIN_BUTTON(widget) GTK_SPIN_BUTTON(widget)
#define LIVES_COLOR_BUTTON(widget) GTK_COLOR_BUTTON(widget)
#define LIVES_TOOL_BUTTON(widget) GTK_TOOL_BUTTON(widget)

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

#define LIVES_PROGRESS_BAR(widget) GTK_PROGRESS_BAR(widget)

#define LIVES_EXPANDER(widget) GTK_EXPANDER(widget)

#define LIVES_MISC(widget) GTK_MISC(widget)

#if GTK_CHECK_VERSION(2,14,0)
#define LIVES_SCALE_BUTTON(widget) GTK_SCALE_BUTTON(widget)
#else
#define LIVES_SCALE_BUTTON(widget) GTK_RANGE(widget)
#endif

#define LIVES_TOGGLE_BUTTON(widget) GTK_TOGGLE_BUTTON(widget)
#define LIVES_TEXT_VIEW(widget) GTK_TEXT_VIEW(widget)
#define LIVES_TEXT_BUFFER(widget) GTK_TEXT_BUFFER(widget)

#define LIVES_TREE_VIEW(widget) GTK_TREE_VIEW(widget)
#define LIVES_TREE_MODEL(object) GTK_TREE_MODEL(object)

#define LIVES_LIST_STORE(object) GTK_LIST_STORE(object)

#define LIVES_ACCEL_GROUP(object) GTK_ACCEL_GROUP(object)


#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_RULER(widget) GTK_SCALE(widget)
#define LIVES_ORIENTABLE(widget) GTK_ORIENTABLE(widget)
#define LIVES_VBOX(widget) GTK_BOX(widget)
#define LIVES_HBOX(widget) GTK_BOX(widget)
#else
#define LIVES_RULER(widget) GTK_RULER(widget)
#define LIVES_VBOX(widget) GTK_VBOX(widget)
#define LIVES_HBOX(widget) GTK_HBOX(widget)
#endif

#if GTK_CHECK_VERSION(3,2,0)
#define LIVES_GRID(widget) GTK_GRID(widget)
#else
#define LIVES_GRID(widget) GTK_WIDGET(widget)
#endif

#if GTK_CHECK_VERSION(3,10,0)
#define LIVES_IMAGE_MENU_ITEM(widget) GTK_MENU_ITEM(widget)
#else
#define LIVES_IMAGE_MENU_ITEM(widget) GTK_IMAGE_MENU_ITEM(widget)
#undef LIVES_HAS_IMAGE_MENU_ITEM
#define LIVES_HAS_IMAGE_MENU_ITEM 1
#endif

#if LIVES_TABLE_IS_GRID
#define LIVES_TABLE(widget) GTK_GRID(widget)
#else
#define LIVES_TABLE(widget) GTK_TABLE(widget)
#endif

#define LIVES_RANGE(widget) GTK_RANGE(widget)

#define LIVES_EDITABLE(widget) GTK_EDITABLE(widget)


#define LIVES_XEVENT(event) GDK_EVENT(event)

#define LIVES_IS_WIDGET_OBJECT(object) G_IS_OBJECT(object)
#define LIVES_IS_WIDGET(widget) GTK_IS_WIDGET(widget)
#define LIVES_IS_WINDOW(widget) GTK_IS_WINDOW(widget)
#define LIVES_IS_XWINDOW(widget) GDK_IS_WINDOW(widget)
#define LIVES_IS_PIXBUF(widget) GDK_IS_PIXBUF(widget)
#define LIVES_IS_CONTAINER(widget) GTK_IS_CONTAINER(widget)

#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_IS_HBOX(widget) (GTK_IS_BOX(widget)&&gtk_orientable_get_orientation(GTK_ORIENTABLE(widget))==GTK_ORIENTATION_HORIZONTAL)
#define LIVES_IS_VBOX(widget) (GTK_IS_BOX(widget)&&gtk_orientable_get_orientation(GTK_ORIENTABLE(widget))==GTK_ORIENTATION_HORIZONTAL)
#define LIVES_IS_SCROLLABLE(widget) GTK_IS_SCROLLABLE(widget)
#else
#define LIVES_IS_HBOX(widget) GTK_IS_HBOX(widget)
#define LIVES_IS_VBOX(widget) GTK_IS_VBOX(widget)
#endif

#define LIVES_IS_BOX(widget) (LIVES_IS_HBOX(widget) || LIVES_IS_VBOX(widget))

#define LIVES_IS_TOOLBAR(widget) GTK_IS_TOOLBAR(widget)
#define LIVES_IS_EVENT_BOX(widget) GTK_IS_EVENT_BOX(widget)

#define LIVES_IS_COMBO(widget) GTK_IS_COMBO_BOX(widget)
#define LIVES_IS_LABEL(widget) GTK_IS_LABEL(widget)
#define LIVES_IS_BUTTON(widget) GTK_IS_BUTTON(widget)
#define LIVES_IS_SPIN_BUTTON(widget) GTK_IS_SPIN_BUTTON(widget)
#define LIVES_IS_TOGGLE_BUTTON(widget) GTK_IS_TOGGLE_BUTTON(widget)
#define LIVES_IS_IMAGE(widget) GTK_IS_IMAGE(widget)
#define LIVES_IS_ENTRY(widget) GTK_IS_ENTRY(widget)
#define LIVES_IS_RANGE(widget) GTK_IS_RANGE(widget)
#define LIVES_IS_PROGRESS_BAR(widget) GTK_IS_PROGRESS_BAR(widget)
#define LIVES_IS_TEXT_VIEW(widget) GTK_IS_TEXT_VIEW(widget)
#define LIVES_IS_MENU_ITEM(widget) GTK_IS_MENU_ITEM(widget)
#define LIVES_IS_FILE_CHOOSER(widget) GTK_IS_FILE_CHOOSER(widget)
#define LIVES_IS_BUTTON_BOX(widget) GTK_IS_BUTTON_BOX(widget)

// (image resize) interpolation types
#define LIVES_INTERP_BEST   GDK_INTERP_HYPER
#define LIVES_INTERP_NORMAL GDK_INTERP_BILINEAR
#define LIVES_INTERP_FAST   GDK_INTERP_NEAREST


#if GTK_CHECK_VERSION(3,10,0)
#define LIVES_STOCK_YES "gtk-yes"             // non-standard image ?
#define LIVES_STOCK_APPLY "gtk-apply"      // non-standard image ?
#define LIVES_STOCK_CANCEL "gtk-cancel"    // non-standard image ?
#define LIVES_STOCK_OK "gtk-ok"    // non-standard image ?
#define LIVES_STOCK_UNDO "edit-undo"
#define LIVES_STOCK_REDO "edit-redo"
#define LIVES_STOCK_ADD "list-add"
#define LIVES_STOCK_REMOVE "list-remove"
#define LIVES_STOCK_NO "media-record"
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
#define LIVES_STOCK_REFRESH "view-refresh"
#define LIVES_STOCK_MEDIA_PLAY "media-playback-start"
#define LIVES_STOCK_MEDIA_STOP "media-playback-stop"
#define LIVES_STOCK_MEDIA_REWIND "media-seek-backward"
#define LIVES_STOCK_MEDIA_FORWARD "media-seek-forward"
#define LIVES_STOCK_MEDIA_RECORD "media-record"
#define LIVES_STOCK_MEDIA_PAUSE "media-pause"
#define LIVES_STOCK_PREFERENCES "preferences-system"
#define LIVES_STOCK_DIALOG_INFO "dialog-information"
#define LIVES_STOCK_MISSING_IMAGE "image-missing"

char LIVES_STOCK_LABEL_CANCEL[32];
char LIVES_STOCK_LABEL_OK[32];
char LIVES_STOCK_LABEL_YES[32];
char LIVES_STOCK_LABEL_NO[32];
char LIVES_STOCK_LABEL_SAVE[32];
char LIVES_STOCK_LABEL_SAVE_AS[32];
char LIVES_STOCK_LABEL_OPEN[32];
char LIVES_STOCK_LABEL_QUIT[32];
char LIVES_STOCK_LABEL_APPLY[32];
char LIVES_STOCK_LABEL_CLOSE[32];
char LIVES_STOCK_LABEL_REVERT[32];
char LIVES_STOCK_LABEL_REFRESH[32];
char LIVES_STOCK_LABEL_DELETE[32];
char LIVES_STOCK_LABEL_GO_FORWARD[32];

char LIVES_STOCK_LABEL_MEDIA_FORWARD[32];
char LIVES_STOCK_LABEL_MEDIA_REWIND[32];
char LIVES_STOCK_LABEL_MEDIA_STOP[32];
char LIVES_STOCK_LABEL_MEDIA_PLAY[32];
char LIVES_STOCK_LABEL_MEDIA_PAUSE[32];
char LIVES_STOCK_LABEL_MEDIA_RECORD[32];


#else
#define LIVES_STOCK_UNDO GTK_STOCK_UNDO
#define LIVES_STOCK_REDO GTK_STOCK_REDO
#define LIVES_STOCK_ADD GTK_STOCK_ADD
#define LIVES_STOCK_APPLY GTK_STOCK_APPLY
#define LIVES_STOCK_REMOVE GTK_STOCK_REMOVE
#define LIVES_STOCK_NO GTK_STOCK_NO
#define LIVES_STOCK_YES GTK_STOCK_YES
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
#define LIVES_STOCK_PREFERENCES GTK_STOCK_PREFERENCES
#define LIVES_STOCK_DIALOG_INFO GTK_STOCK_DIALOG_INFO
#define LIVES_STOCK_MISSING_IMAGE GTK_STOCK_MISSING_IMAGE

#define LIVES_STOCK_LABEL_CANCEL GTK_STOCK_CANCEL
#define LIVES_STOCK_LABEL_OK GTK_STOCK_OK
#define LIVES_STOCK_LABEL_OPEN GTK_STOCK_OPEN
#define LIVES_STOCK_LABEL_SAVE GTK_STOCK_SAVE
#define LIVES_STOCK_LABEL_QUIT GTK_STOCK_QUIT


#if GTK_CHECK_VERSION(2,6,0)
#define LIVES_STOCK_LABEL_MEDIA_FORWARD GTK_STOCK_MEDIA_FORWARD
#define LIVES_STOCK_LABEL_MEDIA_REWIND GTK_STOCK_MEDIA_REWIND
#define LIVES_STOCK_LABEL_MEDIA_STOP GTK_STOCK_MEDIA_STOP
#define LIVES_STOCK_LABEL_MEDIA_PLAY GTK_STOCK_MEDIA_PLAY
#define LIVES_STOCK_LABEL_MEDIA_PAUSE GTK_STOCK_MEDIA_PAUSE
#define LIVES_STOCK_LABEL_MEDIA_RECORD GTK_STOCK_MEDIA_RECORD
#else
#define LIVES_STOCK_LABEL_MEDIA_FORWARD GTK_STOCK_GOTO_LAST
#define LIVES_STOCK_LABEL_MEDIA_REWIND GTK_STOCK_GOTO_FIRST
#define LIVES_STOCK_LABEL_MEDIA_STOP GTK_STOCK_NO
#define LIVES_STOCK_LABEL_MEDIA_PLAY GTK_STOCK_GO_FORWARD
#define LIVES_STOCK_LABEL_MEDIA_PAUSE GTK_STOCK_REFRESH
#define LIVES_STOCK_LABEL_MEDIA_RECORD GTK_STOCK_NO
#endif


#if GTK_CHECK_VERSION(2,6,0)
#define LIVES_STOCK_MEDIA_PAUSE GTK_STOCK_MEDIA_PAUSE
#else
#define LIVES_STOCK_MEDIA_PAUSE GTK_STOCK_REFRESH
#endif

#if GTK_CHECK_VERSION(2,6,0)
#define LIVES_STOCK_MEDIA_PLAY GTK_STOCK_MEDIA_PLAY
#else
#define LIVES_STOCK_MEDIA_PLAY GTK_STOCK_GO_FORWARD
#endif
#if GTK_CHECK_VERSION(2,6,0)
#define LIVES_STOCK_MEDIA_STOP GTK_STOCK_MEDIA_STOP
#else
#define LIVES_STOCK_MEDIA_STOP GTK_STOCK_STOP
#endif
#if GTK_CHECK_VERSION(2,6,0)
#define LIVES_STOCK_MEDIA_REWIND GTK_STOCK_MEDIA_REWIND
#else
#define LIVES_STOCK_MEDIA_REWIND GTK_STOCK_GOTO_FIRST
#endif
#if GTK_CHECK_VERSION(2,6,0)
#define LIVES_STOCK_MEDIA_FORWARD GTK_STOCK_MEDIA_FORWARD
#else
#define LIVES_STOCK_MEDIA_FORWARD GTK_STOCK_GOTO_LAST
#endif
#if GTK_CHECK_VERSION(2,6,0)
#define LIVES_STOCK_MEDIA_RECORD GTK_STOCK_MEDIA_RECORD
#else
#define LIVES_STOCK_MEDIA_RECORD GTK_STOCK_NO
#endif

#endif


#define LIVES_CONTROL_MASK GDK_CONTROL_MASK
#define LIVES_ALT_MASK     GDK_MOD1_MASK
#define LIVES_SHIFT_MASK   GDK_SHIFT_MASK
#define LIVES_LOCK_MASK    GDK_LOCK_MASK

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
#define LIVES_KEY_Slash GDK_KEY_slash
#define LIVES_KEY_Space GDK_KEY_space
#define LIVES_KEY_Plus GDK_KEY_plus
#define LIVES_KEY_Minus GDK_KEY_minus
#define LIVES_KEY_Equal GDK_KEY_equal

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

#define LIVES_KEY_Escape GDK_KEY_Escape

#else
#define LIVES_KEY_Left GDK_Left
#define LIVES_KEY_Right GDK_Right
#define LIVES_KEY_Up GDK_Up
#define LIVES_KEY_Down GDK_Down

#define LIVES_KEY_BackSpace GDK_BackSpace
#define LIVES_KEY_Return GDK_Return
#define LIVES_KEY_Tab GDK_Tab
#define LIVES_KEY_Home GDK_Home
#define LIVES_KEY_End GDK_End
#define LIVES_KEY_Slash GDK_slash
#define LIVES_KEY_Space GDK_space
#define LIVES_KEY_Plus GDK_plus
#define LIVES_KEY_Minus GDK_minus
#define LIVES_KEY_Equal GDK_equal

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

// pango stuff
typedef PangoLayout LingoLayout;
#define lingo_layout_set_alignment(a,b) pango_layout_set_alignment(a,b)

#define LINGO_ALIGN_LEFT PANGO_ALIGN_LEFT
#define LINGO_ALIGN_RIGHT PANGO_ALIGN_RIGHT
#define LINGO_ALIGN_CENTER PANGO_ALIGN_CENTER

#define lingo_layout_set_text(a,b,c) pango_layout_set_text(a,b,c)
#define lingo_painter_show_layout(a,b) pango_cairo_show_layout(a,b)

#define lingo_layout_get_size(a,b,c,d,e) pango_layout_get_size(a,b,c)

#define LINGO_SCALE PANGO_SCALE

#endif


#endif
