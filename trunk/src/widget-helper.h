// widget-helper.h
// LiVES
// (c) G. Finch 2012 - 2014 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_WIDGET_HELPER_H
#define HAS_LIVES_WIDGET_HELPER_H

#ifndef M_PI
#define M_PI 3.1415926536
#endif

typedef enum {
  LIVES_DISPLAY_TYPE_UNKNOWN=0,
  LIVES_DISPLAY_TYPE_X11,
  LIVES_DISPLAY_TYPE_WIN32
} lives_display_t;


#define W_PACKING_WIDTH  10 // packing width for widgets with labels
#define W_PACKING_HEIGHT 10 // packing height for widgets
#define W_BORDER_WIDTH   10 // default border width


#ifdef PAINTER_CAIRO
typedef cairo_t lives_painter_t;
typedef cairo_surface_t lives_painter_surface_t;

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

// needs testing with gtk+ 3,10,0+
//#if GTK_CHECK_VERSION(3,10,0)
//#define LIVES_TABLE_IS_GRID 1
//#endif


#ifndef G_ENCODE_VERSION
#define G_ENCODE_VERSION(major,minor) ((major) << 16 | (minor) << 8)
#endif

#define return_true gtk_true

typedef GObject LiVESObject;

typedef GtkJustification LiVESJustification;

#define LIVES_JUSTIFY_LEFT   GTK_JUSTIFY_LEFT
#define LIVES_JUSTIFY_RIGHT  GTK_JUSTIFY_RIGHT
#define LIVES_JUSTIFY_CENTER GTK_JUSTIFY_CENTER
#define LIVES_JUSTIFY_FILL   GTK_JUSTIFY_RIGHT

typedef GtkOrientation LiVESOrientation;
#define LIVES_ORIENTATION_HORIZONTAL GTK_ORIENTATION_HORIZONTAL
#define LIVES_ORIENTATION_VERTICAL   GTK_ORIENTATION_VERTICAL

typedef GdkEvent                          LiVESXEvent;
typedef GdkEventButton                    LiVESXEventButton;
typedef GdkDisplay                        LiVESXDisplay;
typedef GdkScreen                         LiVESXScreen;
typedef GdkDevice                         LiVESXDevice;

#if GTK_CHECK_VERSION(3,0,0)
typedef GdkDeviceManager                  LiVESXDeviceManager;
#endif

#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_WIDGET_EVENT_EXPOSE_EVENT "draw"
#define GTK_OBJECT(a)                     a
#else
#define LIVES_WIDGET_EVENT_EXPOSE_EVENT "expose_event"
#endif

typedef GtkWidget                         LiVESWidget;
typedef GtkWindow                         LiVESWindow;
typedef GtkContainer                      LiVESContainer;
typedef GtkBin                            LiVESBin;
typedef GtkDialog                         LiVESDialog;
typedef GtkBox                            LiVESBox;
typedef GtkComboBox                       LiVESCombo;
typedef GtkComboBox                       LiVESComboBox;
typedef GtkButton                         LiVESButton;
typedef GtkToggleButton                   LiVESToggleButton;

typedef GtkTextView                       LiVESTextView;
typedef GtkTextBuffer                     LiVESTextBuffer;
typedef GtkTextMark                       LiVESTextMark;
typedef GtkTextIter                       LiVESTextIter;

typedef GtkEntry                          LiVESEntry;
typedef GtkRadioButton                    LiVESRadioButton;
typedef GtkSpinButton                     LiVESSpinButton;
typedef GtkColorButton                    LiVESColorButton;
typedef GtkToolButton                     LiVESToolButton;
typedef GtkLabel                          LiVESLabel;
typedef GtkImage                          LiVESImage;
typedef GtkFileChooser                    LiVESFileChooser;
typedef GtkAlignment                      LiVESAlignment;
typedef GtkMenu                           LiVESMenu;
typedef GtkMenuItem                       LiVESMenuItem;
typedef GtkCheckMenuItem                  LiVESCheckMenuItem;
typedef GtkImageMenuItem                  LiVESImageMenuItem;
typedef GtkRadioMenuItem                  LiVESRadioMenuItem;

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


typedef GtkScrolledWindow                 LiVESScrolledWindow;
typedef GtkToolbar                        LiVESToolbar;
typedef GtkToolItem                       LiVESToolItem;

#if GTK_CHECK_VERSION(2,14,0)
typedef GtkScaleButton                    LiVESScaleButton;
#else
typedef GtkRange                          LiVESScaleButton;
#endif

#if GTK_CHECK_VERSION(3,2,0)
typedef GtkGrid                           LiVESGrid;
#else
typedef GtkWidget                         LiVESGrid;
#endif

#ifdef LIVES_TABLE_IS_GRID
typedef GtkGrid                           LiVESTable;
#else
typedef GtkTable                          LiVESTable;
#endif

typedef GClosure                          LiVESWidgetClosure;


#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_WIDGET_COLOR_HAS_ALPHA (1)
#define LIVES_WIDGET_COLOR_SCALE(x) (x) ///< macro to get 0. to 1.
#define LIVES_WIDGET_COLOR_SCALE_255(x) ((double)x/255.) ///< macro to convert 0. - 255. to component
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
#define LIVES_WIDGET_STATE_INCONSISTENT   GTK_STATE_INSENSITIVE+1
#define LIVES_WIDGET_STATE_FOCUSED        GTK_STATE_INSENSITIVE+2
#define LIVES_WIDGET_STATE_BACKDROP       GTK_STATE_INSENSITIVE+3
#endif


// TODO - change enums for #defines to allow c++ compilation
typedef enum {
  LIVES_EXPAND=GTK_EXPAND,
  LIVES_SHRINK=GTK_SHRINK,
  LIVES_FILL=GTK_FILL
} lives_attach_options_t;


typedef enum {
  LIVES_WINDOW_TOPLEVEL=GTK_WINDOW_TOPLEVEL,
  LIVES_WINDOW_POPUP=GTK_WINDOW_POPUP
} lives_window_type_t;


typedef enum {
  LIVES_FILE_CHOOSER_ACTION_OPEN=GTK_FILE_CHOOSER_ACTION_OPEN,
  LIVES_FILE_CHOOSER_ACTION_SAVE=GTK_FILE_CHOOSER_ACTION_SAVE,
  LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER=GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
  LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER=GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
  LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE
} lives_file_chooser_action_t;


typedef enum {
  LIVES_ICON_SIZE_INVALID=GTK_ICON_SIZE_INVALID,
  LIVES_ICON_SIZE_MENU=GTK_ICON_SIZE_MENU,
  LIVES_ICON_SIZE_SMALL_TOOLBAR=GTK_ICON_SIZE_SMALL_TOOLBAR,
  LIVES_ICON_SIZE_LARGE_TOOLBAR=GTK_ICON_SIZE_LARGE_TOOLBAR,
  LIVES_ICON_SIZE_BUTTON=GTK_ICON_SIZE_BUTTON,
  LIVES_ICON_SIZE_DND=GTK_ICON_SIZE_DND,
  LIVES_ICON_SIZE_DIALOG=GTK_ICON_SIZE_DIALOG
} lives_icon_size_t;


// scrolledwindow policies
typedef enum {
  LIVES_POLICY_ALWAYS=GTK_POLICY_ALWAYS,
  LIVES_POLICY_AUTOMATIC=GTK_POLICY_AUTOMATIC,
  LIVES_POLICY_NEVER=GTK_POLICY_NEVER
} lives_policy_t;


typedef enum {
  LIVES_POS_LEFT=GTK_POS_LEFT,
  LIVES_POS_RIGHT=GTK_POS_RIGHT,
  LIVES_POS_TOP=GTK_POS_TOP,
  LIVES_POS_BOTTOM=GTK_POS_BOTTOM
} lives_position_t;


typedef enum {
  LIVES_SHADOW_NONE=GTK_SHADOW_NONE,
  LIVES_SHADOW_IN=GTK_SHADOW_IN,
  LIVES_SHADOW_OUT=GTK_SHADOW_OUT,
  LIVES_SHADOW_ETCHED_IN=GTK_SHADOW_ETCHED_IN,
  LIVES_SHADOW_ETCHED_OUT=GTK_SHADOW_ETCHED_OUT
} lives_shadow_t;


typedef enum {
  LIVES_ARROW_UP=GTK_ARROW_UP,
  LIVES_ARROW_DOWN=GTK_ARROW_DOWN,
  LIVES_ARROW_LEFT=GTK_ARROW_LEFT,
  LIVES_ARROW_RIGHT=GTK_ARROW_RIGHT,
  LIVES_ARROW_NONE=GTK_ARROW_NONE
} lives_arrow_t;


typedef GtkWrapMode LiVESWrapMode;
#define LIVES_WRAP_NONE GTK_WRAP_NONE
#define LIVES_WRAP_CHAR GTK_WRAP_CHAR
#define LIVES_WRAP_WORD GTK_WRAP_WORD
#define LIVES_WRAP_WORD_CHAR GTK_WRAP_WORD_CHAR


#define LIVES_ACCEL_VISIBLE GTK_ACCEL_VISIBLE
  
typedef GtkToolbarStyle LiVESToolbarStyle;
#define LIVES_TOOLBAR_ICONS GTK_TOOLBAR_ICONS
#define LIVES_TOOLBAR_TEXT  GTK_TOOLBAR_TEXT


typedef GtkSelectionMode LiVESSelectionMode;
#define LIVES_SELECTION_NONE GTK_SELECTION_NONE
#define LIVES_SELECTION_SINGLE GTK_SELECTION_SINGLE
#define LIVES_SELECTION_BROWSE GTK_SELECTION_BROWSE
#define LIVES_SELECTION_MULTIPLE GTK_SELECTION_MULTIPLE


#if GTK_CHECK_VERSION(3,0,0)
typedef GtkScale                          LiVESRuler;
typedef GtkBox                            LiVESVBox;
typedef GtkBox                            LiVESHBox;
#else
typedef GtkRuler                          LiVESRuler;
typedef GtkVBox                           LiVESVBox;
typedef GtkHBox                           LiVESHBox;
#endif

typedef GtkRange                          LiVESRange;

typedef GtkAdjustment                     LiVESAdjustment;

typedef GdkPixbuf                         LiVESPixbuf;

typedef GdkWindow                         LiVESXWindow;

typedef GdkCursor                         LiVESXCursor;

typedef GError                            LiVESError;

#ifndef IS_MINGW
typedef gboolean                          boolean;
#endif

typedef GList                             LiVESList;
typedef GSList                            LiVESSList;

typedef GtkAccelGroup                     LiVESAccelGroup;
typedef GtkAccelFlags                     LiVESAccelFlags;

typedef GtkRequisition                    LiVESRequisition;

typedef GdkPixbufDestroyNotify            LiVESPixbufDestroyNotify;

typedef GdkInterpType                     LiVESInterpType;

typedef gpointer                          livespointer;

#define LIVES_WIDGET(widget) GTK_WIDGET(widget)
#define LIVES_PIXBUF(widget) GDK_PIXBUF(widget)
#define LIVES_WINDOW(widget) GTK_WINDOW(widget)
#define LIVES_XWINDOW(widget) GDK_WINDOW(widget)
#define LIVES_BOX(widget) GTK_BOX(widget)
#define LIVES_EVENT_BOX(widget) GTK_EVENT_BOX(widget)
#define LIVES_ENTRY(widget) GTK_ENTRY(widget)
#define LIVES_CONTAINER(widget) GTK_CONTAINER(widget)
#define LIVES_BIN(widget) GTK_BIN(widget)
#define LIVES_ADJUSTMENT(widget) GTK_ADJUSTMENT(widget)
#define LIVES_DIALOG(widget) GTK_DIALOG(widget)
#define LIVES_COMBO(widget) GTK_COMBO_BOX(widget)
#define LIVES_COMBO_BOX(widget) GTK_COMBO_BOX(widget)
#define LIVES_BUTTON(widget) GTK_BUTTON(widget)
#define LIVES_LABEL(widget) GTK_LABEL(widget)
#define LIVES_ALIGNMENT(widget) GTK_ALIGNMENT(widget)
#define LIVES_FILES_CHOOSER(widget) GTK_FILE_CHOOSER(widget)
#define LIVES_RADIO_BUTTON(widget) GTK_RADIO_BUTTON(widget)
#define LIVES_SPIN_BUTTON(widget) GTK_SPIN_BUTTON(widget)
#define LIVES_COLOR_BUTTON(widget) GTK_COLOR_BUTTON(widget)
#define LIVES_TOOL_BUTTON(widget) GTK_TOOL_BUTTON(widget)

#define LIVES_MENU(widget) GTK_MENU(widget)
#define LIVES_MENU_ITEM(widget) GTK_MENU_ITEM(widget)
#define LIVES_IMAGE(widget) GTK_IMAGE(widget)
#define LIVES_CHECK_MENU_ITEM(widget) GTK_CHECK_MENU_ITEM(widget)
#define LIVES_RADIO_MENU_ITEM(widget) GTK_RADIO_MENU_ITEM(widget)
#define LIVES_FILE_CHOOSER(widget) GTK_FILE_CHOOSER(widget)
#define LIVES_SCROLLED_WINDOW(widget) GTK_SCROLLED_WINDOW(widget)
#define LIVES_TOOLBAR(widget) GTK_TOOLBAR(widget)
#define LIVES_TOOL_ITEM(widget) GTK_TOOL_ITEM(widget)

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
#endif

#if LIVES_TABLE_IS_GRID
#define LIVES_TABLE(widget) GTK_GRID(widget)
#else
#define LIVES_TABLE(widget) GTK_TABLE(widget)
#endif

#define LIVES_RANGE(widget) GTK_RANGE(widget)

#define LIVES_XEVENT(event) GDK_EVENT(event)

#define LIVES_IS_WIDGET(widget) GTK_IS_WIDGET(widget)
#define LIVES_IS_WINDOW(widget) GTK_IS_WINDOW(widget)
#define LIVES_IS_CONTAINER(widget) GTK_IS_CONTAINER(widget)

#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_IS_HBOX(widget) (GTK_IS_BOX(widget)&&gtk_orientable_get_orientation(GTK_ORIENTABLE(widget))==GTK_ORIENTATION_HORIZONTAL)
#define LIVES_IS_VBOX(widget) (GTK_IS_BOX(widget)&&gtk_orientable_get_orientation(GTK_ORIENTABLE(widget))==GTK_ORIENTATION_HORIZONTAL)
#define LIVES_IS_SCROLLABLE(widget) GTK_IS_SCROLLABLE(widget)
#else
#define LIVES_IS_HBOX(widget) GTK_IS_HBOX(widget)
#define LIVES_IS_VBOX(widget) GTK_IS_VBOX(widget)
#endif

#define LIVES_IS_COMBO(widget) GTK_IS_COMBO_BOX(widget)
#define LIVES_IS_LABEL(widget) GTK_IS_LABEL(widget)
#define LIVES_IS_BUTTON(widget) GTK_IS_BUTTON(widget)
#define LIVES_IS_IMAGE(widget) GTK_IS_IMAGE(widget)
#define LIVES_IS_ENTRY(widget) GTK_IS_ENTRY(widget)
#define LIVES_IS_RANGE(widget) GTK_IS_RANGE(widget)
#define LIVES_IS_TEXT_VIEW(widget) GTK_IS_TEXT_VIEW(widget)

// (image resize) interpolation types
#define LIVES_INTERP_BEST   GDK_INTERP_HYPER
#define LIVES_INTERP_NORMAL GDK_INTERP_BILINEAR
#define LIVES_INTERP_FAST   GDK_INTERP_NEAREST

typedef GLogLevelFlags LiVESLogLevelFlags;

#define LIVES_LOG_LEVEL_WARNING G_LOG_LEVEL_WARNING
#define LIVES_LOG_LEVEL_MASK G_LOG_LEVEL_MASK
#define LIVES_LOG_LEVEL_CRITICAL G_LOG_LEVEL_CRITICAL
#define LIVES_LOG_FATAL_MASK G_LOG_FATAL_MASK


#if GTK_CHECK_VERSION(3,10,0)
#define LIVES_STOCK_UNDO "edit-undo"
#define LIVES_STOCK_REDO "edit-redo"
#define LIVES_STOCK_ADD "list-add"
#define LIVES_STOCK_REMOVE "list-remove"
#define LIVES_STOCK_NO "media-record"
#define LIVES_STOCK_OPEN "document-open"
#define LIVES_STOCK_CLOSE "window-close"
#define LIVES_STOCK_CLEAR "edit-clear"
#define LIVES_STOCK_GO_BACK "go-previous"
#define LIVES_STOCK_GO_FORWARD "go-next"
#define LIVES_STOCK_REFRESH "view-refresh"
#define LIVES_STOCK_MEDIA_PLAY "media-playback-start"
#define LIVES_STOCK_MEDIA_STOP "media-playback-stop"
#define LIVES_STOCK_MEDIA_REWIND "media-seek-backward"
#define LIVES_STOCK_MEDIA_RECORD "media-record"
#define LIVES_STOCK_MEDIA_PAUSE "media-pause"
#define LIVES_STOCK_PREFERENCES "preferences-system"
#define LIVES_STOCK_DIALOG_INFO "dialog-information"
#define LIVES_STOCK_MISSING_IMAGE "image-missing"

// be careful with these - labels only
#define LIVES_STOCK_LABEL_CANCEL (_("_Cancel"))
#define LIVES_STOCK_LABEL_OK (_("_OK"))
#define LIVES_STOCK_LABEL_SAVE (_("_Save"))
#define LIVES_STOCK_LABEL_OPEN (_("_Open"))
#define LIVES_STOCK_LABEL_QUIT (_("_Quit"))

#else
#define LIVES_STOCK_UNDO GTK_STOCK_UNDO
#define LIVES_STOCK_REDO GTK_STOCK_REDO
#define LIVES_STOCK_ADD GTK_STOCK_ADD
#define LIVES_STOCK_REMOVE GTK_STOCK_REMOVE
#define LIVES_STOCK_NO GTK_STOCK_NO
#define LIVES_STOCK_YES GTK_STOCK_YES
#define LIVES_STOCK_OPEN GTK_STOCK_OPEN
#define LIVES_STOCK_CLOSE GTK_STOCK_CLOSE
#define LIVES_STOCK_CLEAR GTK_STOCK_CLEAR
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

#define LIVES_STOCK_MEDIA_PAUSE GTK_STOCK_MEDIA_PAUSE
#define LIVES_STOCK_MEDIA_PLAY GTK_STOCK_MEDIA_PLAY
#define LIVES_STOCK_MEDIA_STOP GTK_STOCK_MEDIA_STOP
#define LIVES_STOCK_MEDIA_REWIND GTK_STOCK_MEDIA_REWIND
#define LIVES_STOCK_MEDIA_RECORD GTK_STOCK_MEDIA_RECORD

#endif



typedef GdkModifierType LiVESModifierType;

#define LIVES_CONTROL_MASK GDK_CONTROL_MASK
#define LIVES_ALT_MASK     GDK_MOD1_MASK
#define LIVES_SHIFT_MASK   GDK_SHIFT_MASK
#define LIVES_LOCK_MASK    GDK_LOCK_MASK

#ifdef GDK_KEY_a
#define LIVES_KEY_Left GDK_KEY_Left
#define LIVES_KEY_Right GDK_KEY_Right
#define LIVES_KEY_Up GDK_KEY_Up
#define LIVES_KEY_Down GDK_KEY_Down

#define LIVES_KEY_Space GDK_KEY_space
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

#define LIVES_KEY_Space GDK_space
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

#define LIVES_INT_TO_POINTER GINT_TO_POINTER
#define LIVES_POINTER_TO_INT GPOINTER_TO_INT

#endif


#ifdef GUI_QT
typedef QImage                            LiVESPixbuf;
typedef bool                              boolean;
typedef int                               gint;
typedef uchar                             guchar;
typedef (void *)                          gpointer;  
typedef (void *)(LiVESPixbufDestroyNotify(uchar *, gpointer));

// etc.



#define LIVES_INTERP_BEST   Qt::SmoothTransformation
#define LIVES_INTERP_NORMAL Qt::SmoothTransformation
#define LIVES_INTERP_FAST   Qt::FastTransformation


#endif














// basic functions (wrappers for Toolkit functions)

// lives_painter_functions

lives_painter_t *lives_painter_create(lives_painter_surface_t *target);
lives_painter_t *lives_painter_create_from_widget(LiVESWidget *);
void lives_painter_set_source_pixbuf (lives_painter_t *, const LiVESPixbuf *, double pixbuf_x, double pixbuf_y);
void lives_painter_set_source_surface (lives_painter_t *, lives_painter_surface_t *, double x, double y);
lives_painter_surface_t *lives_painter_image_surface_create(lives_painter_format_t format, int width, int height);
lives_painter_surface_t *lives_painter_image_surface_create_for_data(uint8_t *data, lives_painter_format_t, 
								     int width, int height, int stride);
lives_painter_surface_t *lives_painter_surface_create_from_widget(LiVESWidget *, lives_painter_content_t, 
								  int width, int height);
void lives_painter_surface_flush(lives_painter_surface_t *);

void lives_painter_destroy(lives_painter_t *);
void lives_painter_surface_destroy(lives_painter_surface_t *);

void lives_painter_new_path(lives_painter_t *);

void lives_painter_paint(lives_painter_t *);
void lives_painter_fill(lives_painter_t *);
void lives_painter_stroke(lives_painter_t *);
void lives_painter_clip(lives_painter_t *);

void lives_painter_set_source_rgb(lives_painter_t *, double red, double green, double blue);
void lives_painter_set_source_rgba(lives_painter_t *, double red, double green, double blue, double alpha);

void lives_painter_set_line_width(lives_painter_t *, double width);

void lives_painter_translate(lives_painter_t *, double x, double y);

void lives_painter_rectangle(lives_painter_t *, double x, double y, double width, double height);
void lives_painter_arc(lives_painter_t *, double xc, double yc, double radius, double angle1, double angle2);
void lives_painter_line_to(lives_painter_t *, double x, double y);
void lives_painter_move_to(lives_painter_t *, double x, double y);

boolean lives_painter_set_operator(lives_painter_t *, lives_painter_operator_t);

void lives_painter_set_fill_rule(lives_painter_t *, lives_painter_fill_rule_t);


lives_painter_surface_t *lives_painter_get_target(lives_painter_t *);
int lives_painter_format_stride_for_width(lives_painter_format_t, int width);

uint8_t *lives_painter_image_surface_get_data(lives_painter_surface_t *);
int lives_painter_image_surface_get_width(lives_painter_surface_t *);
int lives_painter_image_surface_get_height(lives_painter_surface_t *);
int lives_painter_image_surface_get_stride(lives_painter_surface_t *);
lives_painter_format_t lives_painter_image_surface_get_format(lives_painter_surface_t *);




// utils

void widget_helper_init(void);

// object funcs.

void lives_object_unref(livespointer);


// lives_pixbuf functions

int lives_pixbuf_get_width(const LiVESPixbuf *);
int lives_pixbuf_get_height(const LiVESPixbuf *);
boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *);
int lives_pixbuf_get_rowstride(const LiVESPixbuf *);
int lives_pixbuf_get_n_channels(const LiVESPixbuf *);
unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *);
const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *);
LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height);
LiVESPixbuf *lives_pixbuf_new_from_data (const unsigned char *buf, boolean has_alpha, int width, int height, 
					 int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn, 
					 gpointer destroy_fn_data);

LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error);
LiVESWidget *lives_image_new_from_pixbuf(LiVESPixbuf *);
LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, boolean preserve_aspect_ratio,
						 LiVESError **error);


LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height, 
				       LiVESInterpType interp_type);

boolean lives_pixbuf_saturate_and_pixelate(const LiVESPixbuf *src, LiVESPixbuf *dest, float saturation, boolean pixilate);

// basic widget fns

void lives_widget_set_sensitive(LiVESWidget *, boolean state);
boolean lives_widget_get_sensitive(LiVESWidget *);

void lives_widget_show(LiVESWidget *);
void lives_widget_show_all(LiVESWidget *);
void lives_widget_hide(LiVESWidget *);
void lives_widget_destroy(LiVESWidget *);

void lives_widget_queue_draw(LiVESWidget *);
void lives_widget_queue_draw_area(LiVESWidget *, int x, int y, int width, int height);
void lives_widget_queue_resize(LiVESWidget *);
void lives_widget_set_size_request(LiVESWidget *, int width, int height);

void lives_widget_reparent(LiVESWidget *, LiVESWidget *new_parent);

void lives_widget_set_app_paintable(LiVESWidget *widget, boolean paintable);

LiVESWidget *lives_event_box_new(void);
LiVESWidget *lives_label_new(const char *text);
LiVESWidget *lives_label_new_with_mnemonic(const char *text);
LiVESWidget *lives_button_new(void);
LiVESWidget *lives_button_new_from_stock(const char *stock_id);
LiVESWidget *lives_button_new_with_label(const char *label);
LiVESWidget *lives_button_new_with_mnemonic(const char *label);

boolean lives_button_set_label(LiVESButton *, const char *label);

LiVESWidget *lives_check_button_new(void);
LiVESWidget *lives_check_button_new_with_label(const char *label);
LiVESWidget *lives_spin_button_new(LiVESAdjustment *, double climb_rate, uint32_t digits);

int lives_dialog_run(LiVESDialog *);

void lives_widget_set_bg_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
void lives_widget_set_fg_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
void lives_widget_set_text_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
void lives_widget_set_base_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);

void lives_widget_get_fg_state_color(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);
void lives_widget_get_bg_state_color(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);

boolean lives_color_parse(const char *spec, LiVESWidgetColor *);

LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1orNULL, const LiVESWidgetColor *c2);

LiVESWidget *lives_event_box_new(void);

LiVESWidget *lives_image_new(void);
LiVESWidget *lives_image_new_from_file(const char *filename);
LiVESWidget *lives_image_new_from_stock(const char *stock_id, lives_icon_size_t size);

void lives_image_set_from_pixbuf(LiVESImage *, LiVESPixbuf *);
LiVESPixbuf *lives_image_get_pixbuf(LiVESImage *);

LiVESWidget *lives_dialog_get_content_area(LiVESDialog *);
LiVESWidget *lives_dialog_get_action_area(LiVESDialog *);

void lives_dialog_add_action_widget(LiVESDialog *, LiVESWidget *, int response_id);

LiVESWidget *lives_window_new(lives_window_type_t wintype);
boolean lives_window_set_title(LiVESWindow *, const char *title);
const char *lives_window_get_title(LiVESWindow *);
boolean lives_window_set_transient_for(LiVESWindow *, LiVESWindow *parent);

boolean lives_window_set_modal(LiVESWindow *, boolean modal);
boolean lives_window_set_deletable(LiVESWindow *, boolean deletable);
boolean lives_window_set_resizable(LiVESWindow *, boolean resizable);
boolean lives_window_set_keep_below(LiVESWindow *, boolean keep_below);
boolean lives_window_set_decorated(LiVESWindow *, boolean decorated);

boolean lives_window_set_default_size(LiVESWindow *, int width, int height);

boolean lives_window_set_screen(LiVESWindow *, LiVESXScreen *);

boolean lives_window_move(LiVESWindow *, int x, int y);
boolean lives_window_get_position(LiVESWindow *, int *x, int *y);
boolean lives_window_resize(LiVESWindow *, int width, int height);
boolean lives_window_present(LiVESWindow *);
boolean lives_window_fullscreen(LiVESWindow *);
boolean lives_window_unfullscreen(LiVESWindow *);
boolean lives_window_maximize(LiVESWindow *);
boolean lives_window_unmaximize(LiVESWindow *);
boolean lives_window_set_hide_titlebar_when_maximized(LiVESWindow *, boolean setting);

boolean lives_window_add_accel_group(LiVESWindow *, LiVESAccelGroup *group);
boolean lives_window_remove_accel_group(LiVESWindow *, LiVESAccelGroup *group);
boolean lives_menu_set_accel_group(LiVESMenu *, LiVESAccelGroup *group);

boolean lives_window_has_toplevel_focus(LiVESWindow *);

LiVESAdjustment *lives_adjustment_new(double value, double lower, double upper, 
						   double step_increment, double page_increment, double page_size);

void lives_box_reorder_child(LiVESBox *, LiVESWidget *child, int pos);
void lives_box_set_homogeneous(LiVESBox *, boolean homogeneous);
void lives_box_set_spacing(LiVESBox *, int spacing);

void lives_box_pack_start(LiVESBox *, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding);
void lives_box_pack_end(LiVESBox *, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding);

LiVESWidget *lives_hbox_new(boolean homogeneous, int spacing);
LiVESWidget *lives_vbox_new(boolean homogeneous, int spacing);

LiVESWidget *lives_hseparator_new(void);
LiVESWidget *lives_vseparator_new(void);

LiVESWidget *lives_hbutton_box_new(void);
LiVESWidget *lives_vbutton_box_new(void);

LiVESWidget *lives_hscale_new(LiVESAdjustment *);
LiVESWidget *lives_vscale_new(LiVESAdjustment *);

LiVESWidget *lives_hpaned_new(void);
LiVESWidget *lives_vpaned_new(void);

LiVESWidget *lives_hscrollbar_new(LiVESAdjustment *);
LiVESWidget *lives_vscrollbar_new(LiVESAdjustment *);

LiVESWidget *lives_label_new(const char *text);

LiVESWidget *lives_arrow_new(lives_arrow_t arrow_type, lives_shadow_t shadow_type);

LiVESWidget *lives_alignment_new(float xalign, float yalign, float xscale, float yscale);
void lives_alignment_set(LiVESAlignment *, float xalign, float yalign, float xscale, float yscale);

LiVESWidget *lives_combo_new(void);
LiVESWidget *lives_combo_new_with_model (LiVESTreeModel *);
LiVESTreeModel *lives_combo_get_model(LiVESCombo *);

void lives_combo_append_text(LiVESCombo *, const char *text);
void lives_combo_set_entry_text_column(LiVESCombo *, int column);

char *lives_combo_get_active_text(LiVESCombo *) WARN_UNUSED;
void lives_combo_set_active_text(LiVESCombo *, const char *text);
void lives_combo_set_active_index(LiVESCombo *, int index);
int lives_combo_get_active(LiVESCombo *);
boolean lives_combo_get_active_iter(LiVESCombo *, LiVESTreeIter *);
void lives_combo_set_active_iter(LiVESCombo *, LiVESTreeIter *);
void lives_combo_set_active_string(LiVESCombo *, const char *active_str);

LiVESWidget *lives_combo_get_entry(LiVESCombo *);

void lives_combo_populate(LiVESCombo *, LiVESList *list);

LiVESWidget *lives_text_view_new(void);
LiVESWidget *lives_text_view_new_with_buffer(LiVESTextBuffer *);
LiVESTextBuffer *lives_text_view_get_buffer(LiVESTextView *);
boolean lives_text_view_set_editable(LiVESTextView *, boolean setting);
boolean lives_text_view_set_accepts_tab(LiVESTextView *, boolean setting);
boolean lives_text_view_set_cursor_visible(LiVESTextView *, boolean setting);
boolean lives_text_view_set_wrap_mode(LiVESTextView *, LiVESWrapMode wrapmode);
boolean lives_text_view_set_justification(LiVESTextView *, LiVESJustification justify);


boolean lives_text_view_scroll_mark_onscreen(LiVESTextView *, LiVESTextMark *mark);


LiVESTextBuffer *lives_text_buffer_new(void);
char *lives_text_buffer_get_text(LiVESTextBuffer *tbuff, LiVESTextIter *start, LiVESTextIter *end, boolean inc_hidden_chars);
boolean lives_text_buffer_set_text(LiVESTextBuffer *, const char *, int len);

boolean lives_text_buffer_insert(LiVESTextBuffer *, LiVESTextIter *, const char *, int len);
boolean lives_text_buffer_insert_at_cursor(LiVESTextBuffer *, const char *, int len);

boolean lives_text_buffer_get_start_iter(LiVESTextBuffer *, LiVESTextIter *);
boolean lives_text_buffer_get_end_iter(LiVESTextBuffer *, LiVESTextIter *);

boolean lives_text_buffer_place_cursor(LiVESTextBuffer *, LiVESTextIter *);

LiVESTextMark *lives_text_buffer_create_mark(LiVESTextBuffer *, const char *mark_name, 
					     const LiVESTextIter *where, boolean left_gravity);
boolean lives_text_buffer_delete_mark(LiVESTextBuffer *, LiVESTextMark *);

boolean lives_text_buffer_delete(LiVESTextBuffer *, LiVESTextIter *start, LiVESTextIter *end);

boolean lives_text_buffer_get_iter_at_mark(LiVESTextBuffer *, LiVESTextIter *, LiVESTextMark *);


boolean lives_tree_model_get(LiVESTreeModel *, LiVESTreeIter *, ...);
boolean lives_tree_model_get_iter(LiVESTreeModel *, LiVESTreeIter *, LiVESTreePath *);
boolean lives_tree_model_get_iter_first(LiVESTreeModel *, LiVESTreeIter *);
LiVESTreePath *lives_tree_model_get_path(LiVESTreeModel *, LiVESTreeIter *);
boolean lives_tree_model_iter_children(LiVESTreeModel *, LiVESTreeIter *, LiVESTreeIter *parent);
int lives_tree_model_iter_n_children(LiVESTreeModel *, LiVESTreeIter *);
boolean lives_tree_model_iter_next(LiVESTreeModel *, LiVESTreeIter *);

boolean lives_tree_path_free(LiVESTreePath *);
LiVESTreePath *lives_tree_path_new_from_string(const char *path);
int lives_tree_path_get_depth(LiVESTreePath *);
int *lives_tree_path_get_indices(LiVESTreePath *);

LiVESTreeStore *lives_tree_store_new(int ncols, ...);
boolean lives_tree_store_append(LiVESTreeStore *, LiVESTreeIter *, LiVESTreeIter *parent);
boolean lives_tree_store_set(LiVESTreeStore *, LiVESTreeIter *, ...);

LiVESWidget *lives_tree_view_new(void);
LiVESWidget *lives_tree_view_new_with_model(LiVESTreeModel *);
boolean lives_tree_view_set_model(LiVESTreeView *, LiVESTreeModel *);
LiVESTreeModel *lives_tree_view_get_model(LiVESTreeView *);
int lives_tree_view_append_column(LiVESTreeView *, LiVESTreeViewColumn *);
boolean lives_tree_view_set_headers_visible(LiVESTreeView *, boolean vis);
LiVESAdjustment *lives_tree_view_get_hadjustment(LiVESTreeView *);
LiVESTreeSelection *lives_tree_view_get_selection(LiVESTreeView *);


LiVESTreeViewColumn *lives_tree_view_column_new_with_attributes(const char *title, LiVESCellRenderer *, ...);
boolean lives_tree_view_column_set_sizing(LiVESTreeViewColumn *, LiVESTreeViewColumnSizing type);
boolean lives_tree_view_column_set_fixed_width(LiVESTreeViewColumn *, int fwidth);

boolean lives_tree_selection_get_selected(LiVESTreeSelection *, LiVESTreeModel **, LiVESTreeIter *);
boolean lives_tree_selection_set_mode(LiVESTreeSelection *, LiVESSelectionMode);
boolean lives_tree_selection_select_iter(LiVESTreeSelection *, LiVESTreeIter *);


boolean lives_toggle_button_get_active(LiVESToggleButton *);
void lives_toggle_button_set_active(LiVESToggleButton *, boolean active);

boolean lives_has_icon(const char *stock_id, lives_icon_size_t size);

void lives_tooltips_set (LiVESWidget *, const char *tip_text);

LiVESSList *lives_radio_button_get_group(LiVESRadioButton *);
LiVESSList *lives_radio_menu_item_get_group(LiVESRadioMenuItem *);

LiVESWidget *lives_widget_get_parent(LiVESWidget *);
LiVESWidget *lives_widget_get_toplevel(LiVESWidget *);

LiVESXWindow *lives_widget_get_xwindow(LiVESWidget *);

boolean lives_widget_set_can_focus(LiVESWidget *, boolean state);
boolean lives_widget_set_can_default(LiVESWidget *, boolean state);
boolean lives_widget_set_can_focus_and_default(LiVESWidget *);

boolean lives_widget_add_events(LiVESWidget *, int events);
boolean lives_widget_set_events(LiVESWidget *, int events);
boolean lives_widget_remove_accelerator(LiVESWidget *, LiVESAccelGroup *, uint32_t accel_key, LiVESModifierType accel_mods);
boolean lives_widget_get_preferred_size(LiVESWidget *, LiVESRequisition *min_size, LiVESRequisition *nat_size);

boolean lives_container_remove(LiVESContainer *, LiVESWidget *);
boolean lives_container_add(LiVESContainer *, LiVESWidget *);
boolean lives_container_set_border_width(LiVESContainer *, uint32_t width);

double lives_spin_button_get_value(LiVESSpinButton *);
int lives_spin_button_get_value_as_int(LiVESSpinButton *);

LiVESAdjustment *lives_spin_button_get_adjustment(LiVESSpinButton *);

boolean lives_spin_button_set_value(LiVESSpinButton *, double value);
boolean lives_spin_button_set_range(LiVESSpinButton *, double min, double max);

boolean lives_spin_button_set_wrap(LiVESSpinButton *, boolean wrap);

boolean lives_spin_button_set_digits(LiVESSpinButton *, uint32_t digits);

boolean lives_spin_button_update(LiVESSpinButton *);

LiVESWidget *lives_color_button_new_with_color(const LiVESWidgetColor *);
boolean lives_color_button_get_color(LiVESColorButton *, LiVESWidgetColor *);
boolean lives_color_button_set_color(LiVESColorButton *, const LiVESWidgetColor *);
boolean lives_color_button_set_title(LiVESColorButton *, const char *title);
boolean lives_color_button_set_use_alpha(LiVESColorButton *, boolean use_alpha);

LiVESToolItem *lives_tool_button_new(LiVESWidget *icon_widget, const char *label);
boolean lives_tool_button_set_icon_widget(LiVESToolButton *, LiVESWidget *icon);
boolean lives_tool_button_set_label_widget(LiVESToolButton *, LiVESWidget *label);
boolean lives_tool_button_set_use_underline(LiVESToolButton *, boolean use_underline);

boolean lives_button_set_use_stock(LiVESButton *, boolean use_stock);

double lives_ruler_get_value(LiVESRuler *);
double lives_ruler_set_value(LiVESRuler *, double value);

void lives_ruler_set_range(LiVESRuler *, double lower, double upper, double position, double max_size);
double lives_ruler_set_upper(LiVESRuler *, double value);
double lives_ruler_set_lower(LiVESRuler *, double value);

LiVESWidget *lives_toolbar_new(void);
boolean lives_toolbar_insert(LiVESToolbar *, LiVESToolItem *, int pos);
boolean lives_toolbar_set_show_arrow(LiVESToolbar *, boolean show);
lives_icon_size_t lives_toolbar_get_icon_size(LiVESToolbar *);
boolean lives_toolbar_set_icon_size(LiVESToolbar *, lives_icon_size_t icon_size);
boolean lives_toolbar_set_style(LiVESToolbar *, LiVESToolbarStyle style);

int lives_widget_get_allocation_x(LiVESWidget *);
int lives_widget_get_allocation_y(LiVESWidget *);
int lives_widget_get_allocation_width(LiVESWidget *);
int lives_widget_get_allocation_height(LiVESWidget *);

void lives_widget_set_state(LiVESWidget *, LiVESWidgetState state);
LiVESWidgetState lives_widget_get_state(LiVESWidget *widget);

LiVESWidget *lives_bin_get_child(LiVESBin *);

boolean lives_widget_is_sensitive(LiVESWidget *);
boolean lives_widget_is_visible(LiVESWidget *);
boolean lives_widget_is_realized(LiVESWidget *);


double lives_adjustment_get_upper(LiVESAdjustment *);
double lives_adjustment_get_lower(LiVESAdjustment *);
double lives_adjustment_get_page_size(LiVESAdjustment *);

void lives_adjustment_set_upper(LiVESAdjustment *, double upper);
void lives_adjustment_set_lower(LiVESAdjustment *, double lower);
void lives_adjustment_set_page_size(LiVESAdjustment *, double page_size);

const char *lives_label_get_text(LiVESLabel *);
void lives_label_set_text(LiVESLabel *, const char *text);
void lives_label_set_text_with_mnemonic(LiVESLabel *, const char *text);

boolean lives_entry_set_editable(LiVESEntry *, boolean editable);
const char *lives_entry_get_text(LiVESEntry *);
boolean lives_entry_set_text(LiVESEntry *, const char *text);
boolean lives_entry_set_width_chars(LiVESEntry *, int nchars);

double lives_scale_button_get_value(LiVESScaleButton *);

LiVESWidget *lives_table_new(uint32_t rows, uint32_t cols, boolean homogeneous);
boolean lives_table_set_row_spacings(LiVESTable *, uint32_t spacing);
boolean lives_table_set_col_spacings(LiVESTable *, uint32_t spacing);
boolean lives_table_resize(LiVESTable *, uint32_t rows, uint32_t cols);
void lives_table_attach(LiVESTable *, LiVESWidget *child, uint32_t left, uint32_t right, 
			uint32_t top, uint32_t bottom, lives_attach_options_t xoptions, lives_attach_options_t yoptions,
			uint32_t xpad, uint32_t ypad);


LiVESWidget *lives_grid_new(void);
boolean lives_grid_set_row_spacing(LiVESGrid *, uint32_t spacing);
boolean lives_grid_set_column_spacing(LiVESGrid *, uint32_t spacing);
boolean lives_grid_attach_next_to(LiVESGrid *, LiVESWidget *child, LiVESWidget *sibling, 
				  lives_position_t side, int width, int height);

boolean lives_grid_insert_row(LiVESGrid *, int posn);
boolean lives_grid_remove_row(LiVESGrid *, int posn);

LiVESWidget *lives_menu_new(void);

void lives_menu_popup(LiVESMenu *, LiVESXEventButton *);

LiVESWidget *lives_menu_item_new(void);
LiVESWidget *lives_menu_item_new_with_mnemonic(const char *label);
LiVESWidget *lives_menu_item_new_with_label(const char *label);
LiVESWidget *lives_check_menu_item_new_with_mnemonic(const char *label);
LiVESWidget *lives_check_menu_item_new_with_label(const char *label);
LiVESWidget *lives_radio_menu_item_new_with_label(LiVESSList *group, const char *label);
LiVESWidget *lives_image_menu_item_new_with_label(const char *label);
LiVESWidget *lives_image_menu_item_new_with_mnemonic(const char *label);
LiVESWidget *lives_image_menu_item_new_from_stock(const char *stock_id, LiVESAccelGroup *accel_group);

#if !GTK_CHECK_VERSION(3,10,0)

void lives_image_menu_item_set_image(LiVESImageMenuItem *, LiVESWidget *image);

#endif

void lives_menu_item_set_submenu(LiVESMenuItem *, LiVESWidget *);

void lives_check_menu_item_set_active(LiVESCheckMenuItem *, boolean state);
boolean lives_check_menu_item_get_active(LiVESCheckMenuItem *);

boolean lives_menu_set_title(LiVESMenu *, const char *title);


int lives_display_get_n_screens(LiVESXDisplay *);


char *lives_file_chooser_get_filename(LiVESFileChooser *);
LiVESSList *lives_file_chooser_get_filenames(LiVESFileChooser *);

boolean lives_widget_grab_focus(LiVESWidget *);
boolean lives_widget_grab_default(LiVESWidget *);

void lives_widget_set_tooltip_text(LiVESWidget *, const char *text);


LiVESAccelGroup *lives_accel_group_new(void);
void lives_accel_group_connect(LiVESAccelGroup *, uint32_t key, LiVESModifierType mod, LiVESAccelFlags flags, LiVESWidgetClosure *closure);
void lives_accel_group_disconnect(LiVESAccelGroup *, LiVESWidgetClosure *closure);
void lives_accel_groups_activate(LiVESObject *object, uint32_t key, LiVESModifierType mod);

void lives_widget_add_accelerator(LiVESWidget *, const char *accel_signal, LiVESAccelGroup *accel_group,
				  uint32_t accel_key, LiVESModifierType accel_mods, LiVESAccelFlags accel_flags);

void lives_widget_get_pointer(LiVESXDevice *, LiVESWidget *, int *x, int *y);
LiVESXWindow *lives_display_get_window_at_pointer (LiVESXDevice *, LiVESXDisplay *, int *win_x, int *win_y);
void lives_display_get_pointer (LiVESXDevice *, LiVESXDisplay *, LiVESXScreen **, int *x, int *y, LiVESModifierType *mask);
void lives_display_warp_pointer (LiVESXDevice *, LiVESXDisplay *, LiVESXScreen *, int x, int y);

LiVESXDisplay *lives_widget_get_display(LiVESWidget *);
lives_display_t lives_widget_get_display_type(LiVESWidget *);

uint64_t lives_widget_get_xwinid(LiVESWidget *, const char *failure_msg);

boolean lives_scrolled_window_set_policy(LiVESScrolledWindow *, lives_policy_t hpolicy, lives_policy_t vpolicy);
boolean lives_scrolled_window_add_with_viewport(LiVESScrolledWindow *, LiVESWidget *child);

// optional (return TRUE if implemented)

boolean lives_dialog_set_has_separator(LiVESDialog *, boolean has);
boolean lives_widget_set_hexpand(LiVESWidget *, boolean state);
boolean lives_widget_set_vexpand(LiVESWidget *, boolean state);
boolean lives_image_menu_item_set_always_show_image(LiVESImageMenuItem *, boolean show);
boolean lives_scale_button_set_orientation(LiVESScaleButton *, LiVESOrientation orientation);
boolean lives_window_set_auto_startup_notification(boolean set);




// compound functions (composed of basic functions)

void lives_painter_set_source_to_bg(lives_painter_t *, LiVESWidget *);

LiVESWidget *lives_standard_label_new(const char *text);
LiVESWidget *lives_standard_label_new_with_mnemonic(const char *text, LiVESWidget *mnemonic_widget);

LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean use_mnemonic, LiVESBox *box, const char *tooltip);
LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup, 
					     LiVESBox *, const char *tooltip);
LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min, 
					    double max, double step, double page, int dp, LiVESBox *, 
					    const char *tooltip);
LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *, 
				       const char *tooltip);

LiVESWidget *lives_standard_entry_new(const char *labeltext, boolean use_mnemonic, const char *txt, int dispwidth, int maxchars, LiVESBox *, 
				      const char *tooltip);

LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons);

LiVESWidget *lives_standard_hruler_new(void);

LiVESWidget *lives_standard_scrolled_window_new(int width, int height, LiVESWidget *child);

LiVESWidget *lives_standard_expander_new(const char *label, boolean use_mnemonic, LiVESBox *parent, LiVESWidget *child);

LiVESWidget *lives_volume_button_new(LiVESOrientation orientation, LiVESAdjustment *, double volume);

LiVESWidget *lives_standard_file_button_new(boolean is_dir, const char *def_dir);



// util functions

void lives_widget_apply_theme(LiVESWidget *, LiVESWidgetState state); // normal theme colours
void lives_widget_apply_theme2(LiVESWidget *, LiVESWidgetState state); // menu and bars colours (bg only...)

void lives_cursor_unref(LiVESXCursor *cursor);

void lives_widget_context_update(void);
void lives_widget_context_update_one(void);

LiVESWidget *lives_menu_add_separator(LiVESMenu *menu);

void lives_widget_get_fg_color(LiVESWidget *, LiVESWidgetColor *);
void lives_widget_get_bg_color(LiVESWidget *, LiVESWidgetColor *);

void lives_window_center(LiVESWindow *);

boolean lives_entry_set_completion_from_list(LiVESEntry *, LiVESList *);

void lives_widget_unparent(LiVESWidget *);

void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source);

char *lives_text_view_get_text(LiVESTextView *);
boolean lives_text_view_set_text(LiVESTextView *, const char *text, int len);


boolean lives_text_buffer_insert_at_end(LiVESTextBuffer *, const char *text);
boolean lives_text_view_scroll_onscreen(LiVESTextView *);

 


void lives_general_button_clicked (LiVESButton *, livespointer data_to_free);

void lives_spin_button_configure(LiVESSpinButton *, double value, double lower, double upper, 
				 double step_increment, double page_increment);



size_t calc_spin_button_width(double min, double max, int dp);

int get_box_child_index (LiVESBox *, LiVESWidget *child);

boolean label_act_toggle (LiVESWidget *, LiVESXEventButton *, LiVESToggleButton *);
boolean widget_act_toggle (LiVESWidget *, LiVESToggleButton *);

void set_button_width(LiVESWidget *buttonbox, LiVESWidget *button, int width);

void toggle_button_toggle (LiVESToggleButton *);

void set_child_colour(GtkWidget *widget, gpointer set_all);

void unhide_cursor(LiVESXWindow *);
void hide_cursor(LiVESXWindow *);

void get_border_size (LiVESWidget *win, int *bx, int *by);

LiVESWidget *add_hsep_to_box (LiVESBox *);
LiVESWidget *add_vsep_to_box (LiVESBox *);

LiVESWidget *add_fill_to_box (LiVESBox *);


#define LIVES_JUSTIFY_DEFAULT (widget_opts.default_justify)

#define W_MAX_FILLER_LEN 65535


typedef enum {
  LIVES_CURSOR_NORMAL=0,  ///< must be zero
  LIVES_CURSOR_BUSY,
  LIVES_CURSOR_CENTER_PTR,
  LIVES_CURSOR_HAND2,
  LIVES_CURSOR_SB_H_DOUBLE_ARROW,
  LIVES_CURSOR_CROSSHAIR,
  LIVES_CURSOR_TOP_LEFT_CORNER,
  LIVES_CURSOR_BOTTOM_RIGHT_CORNER,

  /// non-standard cursors
  LIVES_CURSOR_BLOCK,
  LIVES_CURSOR_AUDIO_BLOCK,
  LIVES_CURSOR_VIDEO_BLOCK,
  LIVES_CURSOR_FX_BLOCK
} lives_cursor_t;

void lives_set_cursor_style(lives_cursor_t cstyle, LiVESWidget *);

typedef enum {
  LIVES_EXPAND_DEFAULT,
  LIVES_EXPAND_NONE,
  LIVES_EXPAND_EXTRA
} lives_expand_t;


typedef struct {
  boolean no_gui; // show nothing !
  boolean swap_label; // swap label/widget position
  boolean pack_end;
  boolean line_wrap; // line wrapping for labels
  boolean non_modal; // non-modal for dialogs
  boolean expand; // whether spin,check,radio buttons should expand
  boolean apply_theme; // whether to apply theming to widget
  double scale; // scale factor for all sizes
  int packing_width; // default should be W_PACKING_WIDTH
  int packing_height; // default should be W_PACKING_HEIGHT
  int border_width; // default should be W_BORDER_WIDTH
  int filler_len; // length of extra "fill" between widgets
  LiVESWidget *last_label; // label widget of last standard widget (spin,radio,check,entry,combo) [readonly]
  LiVESJustification justify; // justify for labels
  LiVESJustification default_justify;
} widget_opts_t;


widget_opts_t widget_opts;

#ifdef NEED_DEF_WIDGET_OPTS

const widget_opts_t def_widget_opts = {
    FALSE, // no_gui
    FALSE, // swap_label
    FALSE, //pack_end
    FALSE, // line_wrap
    FALSE, // non_modal
    LIVES_EXPAND_DEFAULT, // default expand
    FALSE, // no themeing
    1.0, // default scale
    W_PACKING_WIDTH, // def packing width
    W_PACKING_HEIGHT, // def packing height
    W_BORDER_WIDTH, // def border width
    8, // def fill width (in chars)
    NULL, // last_label
    LIVES_JUSTIFY_LEFT, // justify
    LIVES_JUSTIFY_LEFT // default justify
};

#else

extern const widget_opts_t def_widget_opts;

#endif

#endif

