// widget-helper.h
// LiVES
// (c) G. Finch 2012 - 2015 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// this is kind of a fun project.
// a) for me to learn more C++
// b) to experiment with another widget toolkit
// c) to make LiVES a little less dependent on GKT+


#ifndef HAS_LIVES_WIDGET_HELPER_QT_H
#define HAS_LIVES_WIDGET_HELPER_QT_H

#ifdef GUI_QT
// just for testing !!!!

using namespace std;

#include <QtCore/QLinkedList>

#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocumentFragment>
#include <QtGui/QShortcutEvent>
#include <QtGui/QWheelEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QWidgetAction>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLayout>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QShortcut>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>

#include <QtGui/QColor>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QSharedPointer>
#include <QtGui/QStandardItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QTimer>

#define GTK_CHECK_VERSION(a,b,c) 0

#define g_free(a) lives_free(a)

typedef char gchar;

#define NO_GTK
#include "support.h"

// TODO - move to support.c
char *translate(const char *String) {
  delete trString; // be very careful, as trString is free()d automatically here
  if (strlen(String)) {
    QString qs = QString::fromLocal8Bit(dgettext (PACKAGE, String));
    trString = strdup(qs.toUtf8().constData());
  }
  else trString=strdup(String);
  return trString;
}

char *translate_with_plural(const char *String, const char *StringPlural, unsigned long int n) {
  delete trString; // be very careful, as trString is free()d automatically here
  if (strlen(String)) {
    QString qs = QString::fromLocal8Bit(dngettext (PACKAGE, String, StringPlural, n));
    trString = strdup(qs.toUtf8().constData());
  }
  else trString=strdup(String);
  return trString;
}


//////////////////

#ifndef IS_MINGW
typedef bool                          boolean;
#endif

#ifndef FALSE
#define FALSE false
#endif

#ifndef TRUE
#define TRUE true
#endif

typedef void*                             gpointer;  // TODO
typedef void*                             livespointer;
typedef const void*                       livesconstpointer;

typedef int                               gint;
typedef uchar                             guchar;
  
typedef livespointer(*LiVESPixbufDestroyNotify(uchar *, livespointer));


typedef int                               lives_painter_content_t;

typedef gpointer                          livespointer;
typedef ulong                             gulong;

typedef void GError;
typedef void LiVESError;

typedef QScreen LiVESXScreen;
typedef QScreen LiVESXDisplay;
typedef QScreen GdkDisplay;
typedef QScreen LiVESXDevice;

//#define LIVES_TABLE_IS_GRID 1 // no "remove row" avaliable



typedef void PangoLayout; // TODO - replace

typedef QStyle::StateFlag LiVESWidgetState;

#define LIVES_WIDGET_STATE_NORMAL         QStyle::State_Enabled
#define LIVES_WIDGET_STATE_ACTIVE         QStyle::State_Active
#define LIVES_WIDGET_STATE_PRELIGHT       QStyle::State_MouseOver
#define LIVES_WIDGET_STATE_SELECTED       QStyle::State_Selected

#define LIVES_WIDGET_STATE_INSENSITIVE    QStyle::State_None

#define LIVES_WIDGET_STATE_FOCUSED        QStyle::State_HasFocus

//#define LIVES_WIDGET_STATE_INCONSISTENT   GTK_STATE_FLAG_INCONSISTENT
//#define LIVES_WIDGET_STATE_BACKDROP       GTK_STATE_FLAG_BACKDROP

#define LIVES_WIDGET_COLOR_HAS_ALPHA (1)
#define LIVES_WIDGET_COLOR_SCALE(x) (x) ///< macro to get 0. to 1.
#define LIVES_WIDGET_COLOR_SCALE_255(x) ((double)x/255.) ///< macro to convert from (0. - 255.) to component


typedef Qt::Modifier LiVESXModifierType;

#define LIVES_CONTROL_MASK Qt::CTRL
#define LIVES_ALT_MASK     Qt::ALT
#define LIVES_SHIFT_MASK   Qt::SHIFT
#define LIVES_LOCK_MASK    Qt::SHIFT


#define LIVES_KEY_Left (static_cast<uint32_t>(Qt::Key_Left))
#define LIVES_KEY_Right (static_cast<uint32_t>(Qt::Key_Right))
#define LIVES_KEY_Up (static_cast<uint32_t>(Qt::Key_Up))
#define LIVES_KEY_Down (static_cast<uint32_t>(Qt::Key_Down))

#define LIVES_KEY_BackSpace (static_cast<uint32_t>(Qt::Key_Backspace))
#define LIVES_KEY_Return (static_cast<uint32_t>(Qt::Key_Return))
#define LIVES_KEY_Tab (static_cast<uint32_t>(Qt::Key_Tab))
#define LIVES_KEY_Home (static_cast<uint32_t>(Qt::Key_Home))
#define LIVES_KEY_End (static_cast<uint32_t>(Qt::Key_End))
#define LIVES_KEY_Slash (static_cast<uint32_t>(Qt::Key_Slash))
#define LIVES_KEY_Space (static_cast<uint32_t>(Qt::Key_Space))
#define LIVES_KEY_Plus (static_cast<uint32_t>(Qt::Key_Plus))
#define LIVES_KEY_Minus (static_cast<uint32_t>(Qt::Key_Minus))
#define LIVES_KEY_Equal (static_cast<uint32_t>(Qt::Key_Equal))

#define LIVES_KEY_1 (static_cast<uint32_t>(Qt::Key_1))
#define LIVES_KEY_2 (static_cast<uint32_t>(Qt::Key_2))
#define LIVES_KEY_3 (static_cast<uint32_t>(Qt::Key_3))
#define LIVES_KEY_4 (static_cast<uint32_t>(Qt::Key_4))
#define LIVES_KEY_5 (static_cast<uint32_t>(Qt::Key_5))
#define LIVES_KEY_6 (static_cast<uint32_t>(Qt::Key_6))
#define LIVES_KEY_7 (static_cast<uint32_t>(Qt::Key_7))
#define LIVES_KEY_8 (static_cast<uint32_t>(Qt::Key_8))
#define LIVES_KEY_9 (static_cast<uint32_t>(Qt::Key_9))
#define LIVES_KEY_0 (static_cast<uint32_t>(Qt::Key_0))

#define LIVES_KEY_a (static_cast<uint32_t>(Qt::Key_A))
#define LIVES_KEY_b (static_cast<uint32_t>(Qt::Key_B))
#define LIVES_KEY_c (static_cast<uint32_t>(Qt::Key_C))
#define LIVES_KEY_d (static_cast<uint32_t>(Qt::Key_D))
#define LIVES_KEY_e (static_cast<uint32_t>(Qt::Key_E))
#define LIVES_KEY_f (static_cast<uint32_t>(Qt::Key_F))
#define LIVES_KEY_g (static_cast<uint32_t>(Qt::Key_G))
#define LIVES_KEY_h (static_cast<uint32_t>(Qt::Key_H))
#define LIVES_KEY_i (static_cast<uint32_t>(Qt::Key_I))
#define LIVES_KEY_j (static_cast<uint32_t>(Qt::Key_J))
#define LIVES_KEY_k (static_cast<uint32_t>(Qt::Key_K))
#define LIVES_KEY_l (static_cast<uint32_t>(Qt::Key_L))
#define LIVES_KEY_m (static_cast<uint32_t>(Qt::Key_M))
#define LIVES_KEY_n (static_cast<uint32_t>(Qt::Key_N))
#define LIVES_KEY_o (static_cast<uint32_t>(Qt::Key_O))
#define LIVES_KEY_p (static_cast<uint32_t>(Qt::Key_P))
#define LIVES_KEY_q (static_cast<uint32_t>(Qt::Key_Q))
#define LIVES_KEY_r (static_cast<uint32_t>(Qt::Key_R))
#define LIVES_KEY_s (static_cast<uint32_t>(Qt::Key_S))
#define LIVES_KEY_t (static_cast<uint32_t>(Qt::Key_T))
#define LIVES_KEY_u (static_cast<uint32_t>(Qt::Key_U))
#define LIVES_KEY_v (static_cast<uint32_t>(Qt::Key_V))
#define LIVES_KEY_w (static_cast<uint32_t>(Qt::Key_W))
#define LIVES_KEY_x (static_cast<uint32_t>(Qt::Key_X))
#define LIVES_KEY_y (static_cast<uint32_t>(Qt::Key_Y))
#define LIVES_KEY_z (static_cast<uint32_t>(Qt::Key_Z))

#define LIVES_KEY_F1 (static_cast<uint32_t>(Qt::Key_F1))
#define LIVES_KEY_F2 (static_cast<uint32_t>(Qt::Key_F2))
#define LIVES_KEY_F3 (static_cast<uint32_t>(Qt::Key_F3))
#define LIVES_KEY_F4 (static_cast<uint32_t>(Qt::Key_F4))
#define LIVES_KEY_F5 (static_cast<uint32_t>(Qt::Key_F5))
#define LIVES_KEY_F6 (static_cast<uint32_t>(Qt::Key_F6))
#define LIVES_KEY_F7 (static_cast<uint32_t>(Qt::Key_F7))
#define LIVES_KEY_F8 (static_cast<uint32_t>(Qt::Key_F8))
#define LIVES_KEY_F9 (static_cast<uint32_t>(Qt::Key_F9))
#define LIVES_KEY_F10 (static_cast<uint32_t>(Qt::Key_F10))
#define LIVES_KEY_F11 (static_cast<uint32_t>(Qt::Key_F11))
#define LIVES_KEY_F12 (static_cast<uint32_t>(Qt::Key_F12))

#define LIVES_KEY_Page_Up (static_cast<uint32_t>(Qt::Key_PageUp))
#define LIVES_KEY_Page_Down (static_cast<uint32_t>(Qt::Key_PageDown))

#define LIVES_KEY_Escape (static_cast<uint32_t>(Qt::Key_Escape))

typedef int LiVESAccelFlags;

typedef class LiVESAccelGroup LiVESAccelGroup;
typedef class LiVESWidget LiVESWidget;

typedef struct {
  ulong function;
  livespointer data;
} LiVESClosure;

typedef LiVESClosure LiVESWidgetClosure;

typedef boolean (*LiVESWidgetSourceFunc)(livespointer data);


typedef QEvent LiVESXEvent;
typedef Qt::MouseButton LiVESXEventButton;
typedef void LiVESXEventMotion;
typedef QCursor LiVESXCursor; 


#define LIVES_SCROLL_UP   1
#define LIVES_SCROLL_DOWN 2

typedef struct {
  int direction;
  LiVESXModifierType state;
} LiVESXEventScroll;


typedef class LiVESObject LiVESObject;

typedef void (*LiVESWidgetCallback) (LiVESWidget *widget, livespointer data);
typedef boolean (*LiVESScrollEventCallback) (LiVESWidget *widget, LiVESXEventScroll *event, livespointer data);
typedef boolean (*LiVESAccelCallback) (LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer data);

#define LIVES_GUI_CALLBACK(a) (ulong)(a)


typedef struct {
  ulong handler_id; // set for signals, 0 for key accels
  QKeySequence ks;  // set for key accels
  LiVESAccelFlags flags;
  LiVESAccelGroup *group;
  QShortcut *shortcut; // created for key accels

  // for key accels, one or the other of these: - either we emit accel_signal, or we call closure
  // for normal signals, both are set (name of signal and the closure it calls)
  const char *signal_name;  // or NULL, if not NULL we emit signal (or is set for signal)
  LiVESClosure *closure; // or NULL, if not NULL we call closure
  bool blocked;
} LiVESAccel;



typedef struct {
  // values from 0.0 -> 1.0
  double red;
  double green;
  double blue;
  double alpha;
} LiVESWidgetColor;


boolean return_true() {
  return TRUE;
}

typedef QList<livesconstpointer> LiVESSList;
typedef LiVESSList LiVESList;
typedef LiVESSList GSList;
typedef LiVESList GList;

LIVES_INLINE static livesconstpointer lives_slist_nth_data(LiVESSList *slist, uint32_t n) {
  return slist->at(n);
}

LIVES_INLINE static LiVESSList *lives_slist_remove(LiVESSList *slist, livesconstpointer data) {
  slist->removeOne(data);
  return slist;
}

LIVES_INLINE static LiVESSList *lives_slist_append(LiVESSList *slist, livesconstpointer data) {
  slist->push_back(data);
  return slist;
}


LIVES_INLINE static void lives_slist_free(LiVESSList *slist) {
  delete slist;
}

#define g_list_free(a) lives_slist_free(a) // TODO


typedef uint32_t LiVESEventMask;
#define LIVES_EXPOSURE_MASK (1<<0)
#define LIVES_POINTER_MOTION_MASK (1<<1)
#define LIVES_POINTER_MOTION_HINT_MASK (1<<2)
#define LIVES_BUTTON_MOTION_MASK (1<<3)
#define LIVES_BUTTON1_MOTION_MASK (1<<4)
#define LIVES_BUTTON2_MOTION_MASK (1<<5)
#define LIVES_BUTTON3_MOTION_MASK (1<<6)
#define LIVES_BUTTON_PRESS_MASK (1<<7)
#define LIVES_BUTTON_RELEASE_MASK (1<<8)
#define LIVES_KEY_PRESS_MASK (1<<9)
#define LIVES_KEY_RELEASE_MASK (1<<10)
#define LIVES_ENTER_NOTIFY_MASK (1<<11)
#define LIVES_LEAVE_NOTIFY_MASK (1<<12)
#define LIVES_FOCUS_CHANGE_MASK (1<<13)
#define LIVES_STRUCTURE_MASK (1<<14)
#define LIVES_PROPERTY_CHANGE_MASK (1<<15)
#define LIVES_VISIBILITY_NOTIFY_MASK (1<<16)
#define LIVES_PROXIMITY_IN_MASK (1<<17)
#define LIVES_PROXIMITY_OUT_MASK (1<<18)
#define LIVES_SUBSTRUCTURE_MASK (1<<19)
#define LIVES_SCROLL_MASK (1<<20)
#define LIVES_TOUCH_MASK (1<<21)
#define LIVES_SMOOTH_SCROLL_MASK (1<<22)

#define LIVES_ALL_EVENTS_MASK 0xFFFF

// events
#define LIVES_WIDGET_EXPOSE_EVENT "update"
#define LIVES_WIDGET_SCROLL_EVENT "scroll_event"

// TODO - add: "button_press_event", "motion_notify_event", "button_release_event", "enter-notify-event", "leave-notify-event", "drag-data-received", "delete-event", "configure-event", "expose_event"


// signals
#define LIVES_WIDGET_CLICKED_EVENT "clicked"
#define LIVES_WIDGET_TOGGLED_EVENT "toggled"
#define LIVES_WIDGET_CHANGED_EVENT "changed"
#define LIVES_WIDGET_ACTIVATE_EVENT "activate"
#define LIVES_WIDGET_VALUE_CHANGED_EVENT "value-changed"
#define LIVES_WIDGET_SELECTION_CHANGED_EVENT "selection-changed"
#define LIVES_WIDGET_CURRENT_FOLDER_CHANGED_EVENT "current-folder-changed"

// add "unmap", "color-set", "drag-data-received", "mode-changed", accept-position", "edited", "set-focus-child", "state-changed", "switch-page", "size-prepared", "response", 

LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1orNULL, const LiVESWidgetColor *c2);


static QString make_col(LiVESWidgetColor *col) {
  QString qc = QString("rgba(%1,%2,%3,%4)")
    .arg(col->red*255.)
    .arg(col->green*255.)
    .arg(col->blue*255.)
    .arg(col->alpha*255.);
  return qc;
}


typedef enum {
  LIVES_OBJECT_TYPE_UNKNOWN,
  LIVES_OBJECT_TYPE_TIMER,
  LIVES_WIDGET_TYPE_BUTTON,
  LIVES_WIDGET_TYPE_SPIN_BUTTON,
  LIVES_WIDGET_TYPE_RADIO_BUTTON,
  LIVES_WIDGET_TYPE_CHECK_BUTTON,
  LIVES_WIDGET_TYPE_COLOR_BUTTON,
  LIVES_WIDGET_TYPE_TOOL_BUTTON,
  LIVES_WIDGET_TYPE_TOOLBAR,
  LIVES_WIDGET_TYPE_BUTTON_BOX,
  LIVES_WIDGET_TYPE_TEXT_VIEW,
  LIVES_WIDGET_TYPE_SCALE,
  LIVES_WIDGET_TYPE_SCROLLBAR,
  LIVES_WIDGET_TYPE_SCROLLED_WINDOW,
  LIVES_WIDGET_TYPE_PROGRESS_BAR,
  LIVES_WIDGET_TYPE_PANED,
  LIVES_WIDGET_TYPE_MENU,
  LIVES_WIDGET_TYPE_MENU_ITEM,
  LIVES_WIDGET_TYPE_RADIO_MENU_ITEM,
  LIVES_WIDGET_TYPE_CHECK_MENU_ITEM,
  LIVES_WIDGET_TYPE_MENU_TOOL_BUTTON,
  LIVES_WIDGET_TYPE_MENU_BAR,
  LIVES_WIDGET_TYPE_ENTRY,
  LIVES_WIDGET_TYPE_FRAME,
  LIVES_WIDGET_TYPE_FILE_CHOOSER,
  LIVES_WIDGET_TYPE_ARROW,
  LIVES_WIDGET_TYPE_RULER,
  LIVES_WIDGET_TYPE_COMBO,
  LIVES_WIDGET_TYPE_TABLE,
  LIVES_WIDGET_TYPE_DIALOG,
  LIVES_WIDGET_TYPE_ALIGNMENT,
  LIVES_WIDGET_TYPE_IMAGE,
  LIVES_WIDGET_TYPE_LABEL,
  LIVES_WIDGET_TYPE_NOTEBOOK,
  LIVES_WIDGET_TYPE_TREE_VIEW,
  LIVES_WIDGET_TYPE_HSEPARATOR,
  LIVES_WIDGET_TYPE_VSEPARATOR,
  LIVES_WIDGET_TYPE_MAIN_WINDOW,
  LIVES_WIDGET_TYPE_SUB_WINDOW,
  LIVES_WIDGET_TYPE_EVENT_BOX,
  LIVES_WIDGET_TYPE_HBOX,
  LIVES_WIDGET_TYPE_VBOX,
} LiVESObjectType;


LIVES_INLINE QKeySequence make_qkey_sequence(uint32_t key, LiVESXModifierType mods) {
  return static_cast<Qt::Key>(key) | mods;
}

LIVES_INLINE uint32_t qkeysequence_get_key(QKeySequence ks) {
  return ks[0] & 0x01FFFFFF;
}


LIVES_INLINE LiVESXModifierType qkeysequence_get_mod(QKeySequence ks) {
  return static_cast<LiVESXModifierType>(ks[0] & 0xF2000000);
}


class evFilter : public QObject {
 public:

 protected:
  bool eventFilter(QObject *obj, QEvent *event);

};



class LiVESObject : public QObject {
  Q_OBJECT

 public:

  void block_signal(ulong handler_id);
  void block_signals(const char *signame);
  void unblock_signal(ulong handler_id);
  void disconnect_signal(ulong handler_id);

  void add_accel(LiVESAccel *accel);
  void add_accel(ulong handler_id, const char *signal_name, ulong funcptr, gpointer data);
  void add_accel(const char* signal_name, LiVESAccelGroup* group, uint32_t key, LiVESXModifierType mod, LiVESAccelFlags flags);

  boolean remove_accel(LiVESAccel *accel);
  boolean remove_accels(LiVESAccelGroup*, uint32_t key, LiVESXModifierType mod);

  boolean remove_accel_group(LiVESAccelGroup *group);
  void add_accel_group(LiVESAccelGroup *group);

  QList<LiVESAccelGroup *> get_accel_groups();

  QList<LiVESAccel *> get_accels_for(LiVESAccelGroup *group, QKeySequence ks);
  QList<LiVESAccel *> get_accels_for(const char *signame);
  LiVESAccel *get_accel_for(ulong handler_id);

  boolean activate_accel(uint32_t key, LiVESXModifierType mod);
  boolean activate_accel(QKeySequence ks);

  void remove_all_accels();

  LiVESObject() {
    init();
  };


  LiVESObject(const LiVESObject &xobj) {
    init();
    type = xobj.type;
  }

  void ref_sink() {
    if (floating) floating = false;
    else inc_refcount();
  }

  void inc_refcount() {
    refcount++;
  }

  void dec_refcount() {
    if (--refcount == 0) {
      finalise();
    }
  }

  void no_refcount() {
    refcount = 0;
  }


  void set_type(LiVESObjectType xtype) {
    type = xtype;
  }

  LiVESObjectType get_type() {
    return type;
  }


 private:
  bool floating;
  int refcount;
  evFilter *eFilter;
  QList<LiVESAccel *>accels;
  QList<LiVESAccelGroup *>accel_groups;
  LiVESObjectType type;

  void init() {
    eFilter = new evFilter(); 
    (static_cast<QObject *>(this))->installEventFilter(eFilter);
    refcount = 1;
    type = LIVES_OBJECT_TYPE_UNKNOWN;
  }

  void finalise(void) {
    (static_cast<QObject *>(this))->deleteLater(); // schedule for deletion

    // remove us from all accel_groups
    while (accel_groups.size() > 0) {
      remove_accel_group(accel_groups[0]);
    }

    remove_all_accels();
  }

};




#define ICON_SCALE(a) ((int)(1.0 * a))
typedef QSize LiVESIconSize;
#define LIVES_ICON_SIZE_INVALID QSize(0,0)
#define LIVES_ICON_SIZE_MENU QSize(ICON_SCALE(16),ICON_SCALE(16))
#define LIVES_ICON_SIZE_SMALL_TOOLBAR QSize(ICON_SCALE(16),ICON_SCALE(16))
#define LIVES_ICON_SIZE_LARGE_TOOLBAR QSize(ICON_SCALE(24),ICON_SCALE(24))
#define LIVES_ICON_SIZE_BUTTON QSize(ICON_SCALE(16),ICON_SCALE(16))
#define LIVES_ICON_SIZE_DND QSize(ICON_SCALE(32),ICON_SCALE(32))
#define LIVES_ICON_SIZE_DIALOG QSize(ICON_SCALE(48),ICON_SCALE(48))


typedef Qt::TransformationMode LiVESInterpType;
#define LIVES_INTERP_BEST Qt::SmoothTransformation
#define LIVES_INTERP_NORMAL Qt::SmoothTransformation
#define LIVES_INTERP_FAST Qt::FastTransformation

typedef int LiVESResponseType;
#define LIVES_RESPONSE_NONE QDialogButtonBox::InvalidRole
#define LIVES_RESPONSE_OK QDialogButtonBox::AcceptRole
#define LIVES_RESPONSE_CANCEL QDialogButtonBox::RejectRole

#define LIVES_RESPONSE_ACCEPT QDialogButtonBox::AcceptRole

#define LIVES_RESPONSE_YES QDialogButtonBox::YesRole
#define LIVES_RESPONSE_NO QDialogButtonBox::NoRole

#define LIVES_RESPONSE_INVALID QDialogButtonBox::InvalidRole
#define LIVES_RESPONSE_SHOW_DETAILS 100


typedef QStyle::StandardPixmap LiVESArrowType;
#define LIVES_ARROW_UP QStyle::SP_ArrowUp
#define LIVES_ARROW_DOWN QStyle::SP_ArrowDown
#define LIVES_ARROW_LEFT QStyle::SP_ArrowLeft
#define LIVES_ARROW_RIGHT QStyle::SP_ArrowRight
#define LIVES_ARROW_NONE -1

class LiVESWidget : public LiVESObject, public QWidget {
  Q_OBJECT

 public:

  Q_PROPERTY(bool prelight READ get_prelight WRITE set_prelight)

  void set_prelight(bool val) {
    (static_cast<QObject *>(static_cast<QWidget *>(this)))->setProperty("prelight", val);
  }

  bool get_prelight() {
    QVariant qv = (static_cast<QObject *>(static_cast<QWidget *>(this)))->property("prelight");
    return qv.value<bool>();
  }

  void set_fg_color(LiVESWidgetState state, const LiVESWidgetColor *col);
  void set_bg_color(LiVESWidgetState state, const LiVESWidgetColor *col);
  void set_base_color(LiVESWidgetState state, const LiVESWidgetColor *col);
  void set_text_color(LiVESWidgetState state, const LiVESWidgetColor *col);

  LiVESWidgetColor *get_fg_color(LiVESWidgetState state);
  LiVESWidgetColor *get_bg_color(LiVESWidgetState state);

  void update_stylesheet();


 LiVESWidget() : parent(NULL) {

    QVariant qv = QVariant::fromValue(static_cast<LiVESObject *>(this));
    (static_cast<QWidget *>(this))->setProperty("LiVESObject", qv);

    fg_norm=bg_norm=base_norm=text_norm=NULL;
    fg_act=bg_act=base_act=text_act=NULL;
    fg_insen=bg_insen=base_insen=text_insen=NULL;
    fg_hover=bg_hover=base_hover=text_hover=NULL;
    fg_sel=bg_sel=base_sel=text_sel=NULL;
    state = LIVES_WIDGET_STATE_NORMAL;
    widgetName = QString("%1").arg(ulong_random()); 
    events_mask = LIVES_EXPOSURE_MASK;
    onetime_events_mask = 0;

    static_cast<QObject *>(static_cast<QWidget *>(this))->connect(static_cast<QObject *>(static_cast<QWidget *>(this)),
								  SIGNAL(destroyed()),
								  static_cast<QObject *>(static_cast<QWidget *>(this)),
								  SLOT(onDestroyed()));
  }

  void onDestroyed() {
    // if has child widgets, try to remove them and dec_refcount()
    // if widget is a toolitem/menuitem, remove its actions

    // TODO - this should never get called, instead we should call dec_refcount()


  }

  ~LiVESWidget();


  void add_child(LiVESWidget *child) {
    if (child->parent != NULL) return;
    child->set_parent(this);
    child->ref_sink();
    children->push_back(child);
  }

  void remove_child(LiVESWidget *child) {
    children->removeOne(child);
    child->set_parent(NULL);
    child->dec_refcount();
  }

  void set_parent(LiVESWidget *new_parent) {
    parent = new_parent;
  }

  LiVESWidget *get_parent() {
    return parent;
  }

  QString get_name() {
    return widgetName;
  }

  void set_events(uint32_t mask) {
    events_mask = mask;
  }

  void add_onetime_event_block(uint32_t mask) {
    onetime_events_mask |= mask;
  }

  void remove_onetime_event_block(uint32_t mask) {
    if (onetime_events_mask & mask) onetime_events_mask  ^= mask;
  }

  uint32_t get_onetime_events_block() {
    return onetime_events_mask;
  }

  uint32_t get_events() {
    return events_mask;
  }

  int count_children() {
    return children->count();
  }

  LiVESSList *get_children() {
    return children;
  }

  void set_children(LiVESSList *xchildren) {
    delete children;
    children = xchildren;
  }

  LiVESWidget *get_child(int index) {
    return (LiVESWidget *)children->at(index);
  }


  int get_child_index(LiVESWidget *child) {
    for (int i = 0; i<children->size(); i++) {
      if (children->at(i) == child) return i;
    }
    return -1;
  }

  LiVESWidgetState get_state() {
    return state;
  }

  void set_state(LiVESWidgetState xstate) {
    state = xstate;

    if (state & LIVES_WIDGET_STATE_INSENSITIVE) {
      setEnabled(false);
    }
    else {
      setEnabled(true);
    }


    if (state & LIVES_WIDGET_STATE_PRELIGHT) {
      if (!get_prelight()) set_prelight(true);
    }
    else {
      if (get_prelight()) set_prelight(false);
    }

  }


 Q_SLOTS
   void cb_wrapper_clicked() {
   // "clicked" callback
   call_accels_for(LIVES_WIDGET_CLICKED_EVENT);
 }

 void cb_wrapper_toggled() {
   // "toggled" callback
   call_accels_for(LIVES_WIDGET_CLICKED_EVENT);
 }


 void cb_wrapper_changed() {
   // "changed" callback
   call_accels_for(LIVES_WIDGET_CHANGED_EVENT);
 }


 void cb_wrapper_value_changed() {
   // "value-changed" callback
   call_accels_for(LIVES_WIDGET_VALUE_CHANGED_EVENT);
 }

 void cb_wrapper_selection_changed() {
     // "selection-changed" callback
   call_accels_for(LIVES_WIDGET_SELECTION_CHANGED_EVENT);
 }

 void cb_wrapper_current_folder_changed() {
     // "selection-changed" callback
   call_accels_for(LIVES_WIDGET_CURRENT_FOLDER_CHANGED_EVENT);
 }

 void cb_wrapper_activate() {
   // "activate" callback
   call_accels_for(LIVES_WIDGET_ACTIVATE_EVENT);
 }

 private:
   LiVESSList *children;
   LiVESWidget *parent;
   QString widgetName;
   
   LiVESWidgetColor *fg_norm,*bg_norm,*base_norm,*text_norm;
   LiVESWidgetColor *fg_act,*bg_act,*base_act,*text_act;
   LiVESWidgetColor *fg_insen,*bg_insen,*base_insen,*text_insen;
   LiVESWidgetColor *fg_hover,*bg_hover,*base_hover,*text_hover;
   LiVESWidgetColor *fg_sel,*bg_sel,*base_sel,*text_sel; 
   
   LiVESWidgetState state;
   
   uint32_t events_mask;
   uint32_t onetime_events_mask;

   void call_accels_for(const char *type) {
     QList<LiVESAccel *> ql = get_accels_for(type);
     for (int i=0; i < ql.size(); i++) {
       LiVESWidgetCallback *cb = (LiVESWidgetCallback *)ql[i]->closure->function;
       (*cb)(this, ql[i]->closure->data);
     }
   }

};


#define LIVES_IS_WIDGET(a) 1
#define LIVES_IS_CONTAINER(a) 1
#define LIVES_IS_XWINDOW(a) 1

#define LIVES_IS_BUTTON(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_BUTTON || \
			    static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_CHECK_BUTTON)
#define LIVES_IS_PUSH_BUTTON(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_BUTTON || \
				 static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_RADIO_BUTTON)
#define LIVES_IS_RANGE(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_SCALE || \
			   static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_SCROLLBAR)
#define LIVES_IS_LABEL(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_LABEL)
#define LIVES_IS_HBOX(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_HBOX)
#define LIVES_IS_VBOX(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_VBOX)
#define LIVES_IS_COMBO(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_COMBO)
#define LIVES_IS_ENTRY(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_ENTRY)
#define LIVES_IS_MENU(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_MENU)
#define LIVES_IS_MENU_BAR(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_MENU_BAR)
#define LIVES_IS_TOOLBAR(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_TOOLBAR)
#define LIVES_IS_SCALE(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_SCALE)
#define LIVES_IS_FRAME(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_FRAME)
#define LIVES_IS_WINDOW(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_MAIN_WINDOW)
#define LIVES_IS_DIALOG(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_SUB_WINDOW)
#define LIVES_IS_PANED(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_PANED)
#define LIVES_IS_TABLE(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_TABLE)
#define LIVES_IS_NOTEBOOK(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_NOTEBOOK)
#define LIVES_IS_SPIN_BUTTON(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_SPIN_BUTTON)
#define LIVES_IS_SCROLLBAR(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_SCROLLBAR)
#define LIVES_IS_TREE_VIEW(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_TREE_VIEW)
#define LIVES_IS_SCROLLED_WINDOW(a) (static_cast<LiVESObject *>(a)->get_type() == LIVES_WIDGET_TYPE_SCROLLED_WINDOW)

   
bool evFilter::eventFilter(QObject *obj, QEvent *event) {

  // TODO - add button-press-event, motion-notify-event, enter-notify-event, leave-notify-event, delete-event, configure-event, button-release-event, focus_out_event

  // event->accept() to block ?

  // return true to block ?

  switch (event->type()) {

  case (QEvent::Shortcut): {
    QShortcutEvent *qevent = static_cast<QShortcutEvent *>(event);
    QKeySequence ks = qevent->key();
    LiVESObject *object = static_cast<LiVESObject *>(obj);
    object->activate_accel(ks);
    return false;
  }

  case (QEvent::Wheel): {
    QWheelEvent *qevent = static_cast<QWheelEvent *>(event);
    LiVESObject *object = static_cast<LiVESObject *>(obj);
    LiVESWidget *widget = static_cast<LiVESWidget *>(object);
    if (!(widget->get_events() & LIVES_SCROLL_MASK)) return true;
    if (!(widget->get_onetime_events_block() & LIVES_SCROLL_MASK)) {widget->remove_onetime_event_block(LIVES_SCROLL_MASK); return true;}
    QList<LiVESAccel *>accels = object->get_accels_for(LIVES_WIDGET_SCROLL_EVENT);
    LiVESXEventScroll *scrollevent = NULL;
    if (qevent->angleDelta().y() > 0) scrollevent->direction = LIVES_SCROLL_UP;
    else scrollevent->direction = LIVES_SCROLL_DOWN;

    // TODO - set kstate

    for (int i=0; i < accels.size(); i++) {
      LiVESScrollEventCallback *cb = (LiVESScrollEventCallback *)accels[i]->closure->function;
      bool ret = (*cb)(widget, scrollevent, accels[i]->closure->data);
      if (ret) return true;
    }
  }

  default:
    return false;

  }

  return false; // continue
}




void LiVESWidget::update_stylesheet() {
  QWidget *qw = static_cast<QWidget *>(this);
  QString stylesheet;
  QString col;

  stylesheet = "QWidget#" + widgetName + " {color: ";
  col=make_col(fg_norm);
  stylesheet += col;
  stylesheet += "; background-color: ";
  col=make_col(bg_norm);
  stylesheet += col;
  stylesheet += "; selection-background-color: ";
  col=make_col(bg_sel);
  stylesheet += col;
  stylesheet += "; selection-color: ";
  col=make_col(fg_sel);
  stylesheet += col;
  stylesheet += " } ";
  stylesheet = "QWidget#" + widgetName + ":active {color: ";
  col=make_col(fg_act);
  stylesheet += col;
  stylesheet += "; background-color: ";
  col=make_col(bg_act);
  stylesheet += col;
  stylesheet += " } ";
  stylesheet = "QWidget#" + widgetName + ":[prelight=true] {color: ";
  col=make_col(fg_hover);
  stylesheet += col;
  stylesheet += "; background-color: ";
  col=make_col(bg_hover);
  stylesheet += col;
  stylesheet += " } ";
  stylesheet = "QWidget#" + widgetName + ":hover {color: ";
  col=make_col(fg_hover);
  stylesheet += col;
  stylesheet += "; background-color: ";
  col=make_col(bg_hover);
  stylesheet += col;
  stylesheet += " } ";
  stylesheet = "QWidget#" + widgetName + ":disabled {color: ";
  col=make_col(fg_insen);
  stylesheet += col;
  stylesheet += "; background-color: ";
  col=make_col(bg_insen);
  stylesheet += col;
  stylesheet += " } ";

  qw->setStyleSheet(stylesheet);
  qw->update();
}



void LiVESWidget::set_fg_color(LiVESWidgetState state, const LiVESWidgetColor *col) {
  switch (state) {
  case (LIVES_WIDGET_STATE_NORMAL):
    lives_widget_color_copy(fg_norm,col);
    break;
  case (LIVES_WIDGET_STATE_ACTIVE):
    lives_widget_color_copy(fg_act,col);
    break;
  case (LIVES_WIDGET_STATE_INSENSITIVE):
    lives_widget_color_copy(fg_insen,col);
    break;
  case (LIVES_WIDGET_STATE_PRELIGHT):
    lives_widget_color_copy(fg_hover,col);
    break;
  case (LIVES_WIDGET_STATE_SELECTED):
    lives_widget_color_copy(fg_sel,col);
    break;
  default:
    break;
  }
  update_stylesheet();
}


void LiVESWidget::set_bg_color(LiVESWidgetState state, const LiVESWidgetColor *col) {
  switch (state) {
  case (LIVES_WIDGET_STATE_NORMAL):
    lives_widget_color_copy(bg_norm,col);
    break;
  case (LIVES_WIDGET_STATE_ACTIVE):
    lives_widget_color_copy(bg_act,col);
    break;
  case (LIVES_WIDGET_STATE_INSENSITIVE):
    lives_widget_color_copy(bg_insen,col);
    break;
  case (LIVES_WIDGET_STATE_PRELIGHT):
    lives_widget_color_copy(bg_hover,col);
    break;
  case (LIVES_WIDGET_STATE_SELECTED):
    lives_widget_color_copy(bg_sel,col);
    break;
  default:
    break;
  }
  update_stylesheet();
}


void LiVESWidget::set_base_color(LiVESWidgetState state, const LiVESWidgetColor *col) {
  switch (state) {
  case (LIVES_WIDGET_STATE_NORMAL):
    lives_widget_color_copy(base_norm,col);
    break;
  case (LIVES_WIDGET_STATE_ACTIVE):
    lives_widget_color_copy(base_act,col);
    break;
  case (LIVES_WIDGET_STATE_INSENSITIVE):
    lives_widget_color_copy(base_insen,col);
    break;
  case (LIVES_WIDGET_STATE_PRELIGHT):
    lives_widget_color_copy(base_hover,col);
    break;
  case (LIVES_WIDGET_STATE_SELECTED):
    lives_widget_color_copy(base_sel,col);
    break;
  default:
    break;
  }
  update_stylesheet();
}


void LiVESWidget::set_text_color(LiVESWidgetState state, const LiVESWidgetColor *col) {
  switch (state) {
  case (LIVES_WIDGET_STATE_NORMAL):
    lives_widget_color_copy(text_norm,col);
    break;
  case (LIVES_WIDGET_STATE_ACTIVE):
    lives_widget_color_copy(text_act,col);
    break;
  case (LIVES_WIDGET_STATE_INSENSITIVE):
    lives_widget_color_copy(text_insen,col);
    break;
  case (LIVES_WIDGET_STATE_PRELIGHT):
    lives_widget_color_copy(text_hover,col);
    break;
  case (LIVES_WIDGET_STATE_SELECTED):
    lives_widget_color_copy(text_sel,col);
    break;
  default:
    break;
  }
  update_stylesheet();
}


LiVESWidgetColor *LiVESWidget::get_fg_color(LiVESWidgetState state) {
  switch (state) {
  case (LIVES_WIDGET_STATE_NORMAL):
    return fg_norm;
    break;
  case (LIVES_WIDGET_STATE_ACTIVE):
    return fg_act;
    break;
  case (LIVES_WIDGET_STATE_INSENSITIVE):
    return fg_insen;
    break;
  case (LIVES_WIDGET_STATE_PRELIGHT):
    return fg_hover;
    break;
  case (LIVES_WIDGET_STATE_SELECTED):
    return fg_sel;
    break;
  default:
    break;
  }
  return NULL;
}


LiVESWidgetColor *LiVESWidget::get_bg_color(LiVESWidgetState state) {
  switch (state) {
  case (LIVES_WIDGET_STATE_NORMAL):
    return bg_norm;
    break;
  case (LIVES_WIDGET_STATE_ACTIVE):
    return bg_act;
    break;
  case (LIVES_WIDGET_STATE_INSENSITIVE):
    return bg_insen;
    break;
  case (LIVES_WIDGET_STATE_PRELIGHT):
    return bg_hover;
    break;
  case (LIVES_WIDGET_STATE_SELECTED):
    return bg_sel;
    break;
  default:
    break;
  }
  return NULL;
}


ulong lives_signal_connect(LiVESObject *object, const char *signal_name, ulong funcptr, gpointer data) {
  ulong handler_id;
  handler_id=ulong_random();

  object->add_accel(handler_id, signal_name, funcptr, data);

  if (!strcmp(signal_name, LIVES_WIDGET_CLICKED_EVENT)) {
    (static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
					      SIGNAL(clicked()), 
					      static_cast<QObject *>(object), 
					      SLOT(cb_wrapper_clicked()));
  }
  else if (!strcmp(signal_name, LIVES_WIDGET_TOGGLED_EVENT)) {
    (static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
					      SIGNAL(toggled()), 
					      static_cast<QObject *>(object), 
					      SLOT(cb_wrapper_toggled()));
  }
  else if (!strcmp(signal_name, LIVES_WIDGET_CHANGED_EVENT)) {
    // for combo, entry, LiVESTreeSelection
    if (LIVES_IS_COMBO(object))
	(static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
						  SIGNAL(currentTextChanged()), 
						  static_cast<QObject *>(object), 
						  SLOT(cb_wrapper_changed()));

    else if (LIVES_IS_ENTRY(object)) 
	     (static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
						       SIGNAL(textChanged()), 
						       static_cast<QObject *>(object), 
						       SLOT(cb_wrapper_changed()));

    else {
      QTreeWidget *qtw = dynamic_cast<QTreeWidget *>(object);
      if (qtw != NULL) { 
	(static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
						  SIGNAL(itemSelectionChanged()), 
						  static_cast<QObject *>(object), 
						  SLOT(cb_wrapper_changed()));

      }
    }
  }
  else if (!strcmp(signal_name, LIVES_WIDGET_VALUE_CHANGED_EVENT)) {
    // for spinbutton, scrollbar
    (static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
					      SIGNAL(valueChanged()), 
					      static_cast<QObject *>(object), 
					      SLOT(cb_wrapper_value_changed()));

  }
  else if (!strcmp(signal_name, LIVES_WIDGET_SELECTION_CHANGED_EVENT)) {
    // for filedialog
    (static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
					      SIGNAL(currentChanged()), 
					      static_cast<QObject *>(object), 
					      SLOT(cb_wrapper_selection_changed()));

  }
  else if (!strcmp(signal_name, LIVES_WIDGET_CURRENT_FOLDER_CHANGED_EVENT)) {
    // for filedialog
    (static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
					      SIGNAL(currentUrlChanged()), 
					      static_cast<QObject *>(object), 
					      SLOT(cb_wrapper_current_folder_changed()));

  }
  else if (!strcmp(signal_name, LIVES_WIDGET_ACTIVATE_EVENT)) {
    // for menuitems (QAction)
    (static_cast<QObject *>(object))->connect(static_cast<QObject *>(object), 
					      SIGNAL(triggered()), 
					      static_cast<QObject *>(object), 
					      SLOT(cb_wrapper_activate()));

  }


  return handler_id;
}


LIVES_INLINE void lives_widget_object_set_data(LiVESObject *widget, const char *prop, livespointer value) {
  QVariant v = qVariantFromValue((livespointer) value);
  widget->setProperty(prop, v);
}


LIVES_INLINE livespointer lives_widget_object_get_data(LiVESObject *widget, const char *prop) {
  QVariant v = widget->property(prop);
  return v.value<livespointer>();
}




#define lives_signal_connect_after(a,b,c,d) lives_signal_connect(a,b,c,d)



void LiVESObject::add_accel(LiVESAccel *accel) {
  accels.push_back(accel);

  QShortcut *shortcut = new QShortcut(accel->ks, dynamic_cast<QWidget *>(this));
  accel->shortcut = shortcut;
}


void LiVESObject::add_accel(ulong handler_id, const char *signal_name, ulong funcptr, gpointer data) {
  LiVESAccel *accel = new LiVESAccel;
  accel->handler_id = handler_id;
  accel->signal_name = strdup(signal_name);
  LiVESClosure *cl = new LiVESClosure;
  cl->function = funcptr;
  cl->data = data;
  accel->closure = cl;
  add_accel(accel);
}


void LiVESObject::add_accel(const char* signal_name, LiVESAccelGroup* group, uint32_t key, LiVESXModifierType mod, LiVESAccelFlags flags) {
  LiVESAccel *accel = new LiVESAccel;
  accel->handler_id = 0;
  accel->signal_name = strdup(signal_name);
  accel->closure = NULL;
  accel->group = group;
  accel->ks = make_qkey_sequence(key, mod);
  accel->flags = flags;
  add_accel(accel);
}


boolean LiVESObject::remove_accel(LiVESAccel *accel) {
  QList<LiVESAccel *>::iterator it = accels.begin();

  while (it != accels.end()) {
    if (((LiVESAccel *)*it) == accel) {
      delete accel->shortcut;
      delete accel->signal_name;
      delete accel->closure;
      accels.erase(it);
      return TRUE;
    }
    else
      ++it;
  }

  return FALSE;
}


boolean LiVESObject::activate_accel(uint32_t key, LiVESXModifierType mod) {
  return activate_accel(make_qkey_sequence(key, mod));
}


boolean LiVESObject::activate_accel(QKeySequence ks) {
  for (int j=0; j < accel_groups.size(); j++) {
    QList<LiVESAccel *> ql = get_accels_for(accel_groups.at(j), ks);
    for (int i=0; i < ql.size(); i++) {
      LiVESAccel *accel = ql.at(i);
      if (accel->closure != NULL) {
	LiVESAccelCallback *cb = (LiVESAccelCallback *)accel->closure->function;
	uint32_t key = qkeysequence_get_key(ks);
	LiVESXModifierType mod = qkeysequence_get_mod(ks);
	(*cb)(accel->group, this, key, mod, accel->closure->data);
      }
      else {
	if (!strcmp(accel->signal_name, LIVES_WIDGET_CLICKED_EVENT)) {
	  QAbstractButton *widget = dynamic_cast<QAbstractButton *>(this);
	  if (widget != NULL) widget->click();
	}

	if (!strcmp(accel->signal_name, LIVES_WIDGET_TOGGLED_EVENT)) {
	  QAbstractButton *widget = dynamic_cast<QAbstractButton *>(this);
	  if (widget != NULL) widget->toggle();
	}

      }
      return TRUE;
    }
  }
  return FALSE;
}



void LiVESObject::remove_all_accels() {
  QList<LiVESAccel *>::iterator it = accels.begin();
  while (it != accels.end()) {
    remove_accel((LiVESAccel *)*it);
  }
}


QList<LiVESAccel *> LiVESObject::get_accels_for(const char *signame) {
  QList<LiVESAccel *> ql;
  for (int i=0; i < accels.size(); i++) {
    if (accels[i]->signal_name == signame) ql.push_back(accels[i]);
  }
  return ql;
}


LiVESAccel *LiVESObject::get_accel_for(ulong handler_id) {
  for (int i=0; i < accels.size(); i++) {
    if (accels[i]->handler_id == handler_id) {
      return accels[i];
    }
  }
  return NULL;
}


void LiVESObject::block_signal(ulong handler_id) {
  LiVESAccel *accel = get_accel_for(handler_id);
  accel->blocked = true;
}


void LiVESObject::block_signals(const char *signame) {
  QList<LiVESAccel *>ql = get_accels_for(signame);
  for (int i=0; i < ql.size(); i++) {
    ql[i]->blocked = true;
  }
}


void LiVESObject::unblock_signal(ulong handler_id) {
  LiVESAccel *accel = get_accel_for(handler_id);
  accel->blocked = false;
}


void LiVESObject::disconnect_signal(ulong handler_id) {
  LiVESAccel *accel = get_accel_for(handler_id);
  remove_accel(accel);
}


QList<LiVESAccelGroup *> LiVESObject::get_accel_groups() {
  return accel_groups;
}



class LiVESBox : public LiVESWidget {};


class LiVESHBox : public LiVESBox, public QHBoxLayout {
 public:
  LiVESHBox() {
    set_type(LIVES_WIDGET_TYPE_HBOX);
  }
};


class LiVESEventBox : public LiVESBox, public QHBoxLayout {
 public:
  LiVESEventBox() {
    set_type(LIVES_WIDGET_TYPE_EVENT_BOX);
  }
};


class LiVESVBox : public LiVESBox, public QVBoxLayout {
 public:
  LiVESVBox() {
    set_type(LIVES_WIDGET_TYPE_VBOX);
  }
};


typedef class LiVESRange LiVESRange;

class LiVESAdjustment : public LiVESObject {

 public:

 LiVESAdjustment(double xval, double low, double upp, double step_i, double page_i, double page_s) : 
  value(xval), lower(low), upper(upp), step_increment(step_i), page_increment(page_i), page_size(page_s), frozen(FALSE) {};

  void set_value(double newval);
  void set_lower(double newval);
  void set_upper(double newval);
  void set_step_increment(double newval);
  void set_page_increment(double newval);
  void set_page_size(double newval);

  void set_owner(LiVESWidget *widget) {
    owner = widget;
  }

  LiVESWidget *get_owner() {
    return owner;
  }


  double get_value() {
    return value;
  }


  double get_upper() {
    return upper;
  }


  double get_lower() {
    return lower;
  }

  double get_step_increment() {
    return step_increment;
  }

  double get_page_increment() {
    return page_increment;
  }

  double get_page_size() {
    return page_size;
  }

  void freeze() {
    frozen = true;
  }

  void thaw() {
    frozen = false;
  }

 private:
  double value;
  double lower;
  double upper;
  double step_increment;
  double page_increment;
  double page_size;
  boolean frozen;

  LiVESWidget *owner;

};


LIVES_INLINE QString qmake_mnemonic(QString qlabel) {
  qlabel = qlabel.replace('&',"&&");
  qlabel = qlabel.replace('_','&');
  return qlabel;
}


LIVES_INLINE QString qmake_underline(QString qlabel) {
  qlabel = qlabel.replace('&','_');
  qlabel = qlabel.replace("__","&");
  return qlabel;
}


class LiVESButtonBase : public LiVESWidget {
 public:
  LiVESButtonBase() {
    init();
  }

 LiVESButtonBase(QString qs) {
   init();
  }

 void set_use_underline(bool use) {
   QAbstractButton *qab = dynamic_cast<QAbstractButton *>(this);
   if (qab != NULL) {
     if (use && !use_underline) {
       // alter label
       qab->setText(qmake_mnemonic(qab->text()));
     }
     else if (!use && use_underline) {
       qab->setText(qmake_underline(qab->text()));
     }
   }
   use_underline = use;
 }

 bool get_use_underline() {
   return use_underline;
 }

 private:
 bool use_underline;

 void init() {
    use_underline = false;
 }

};


class LiVESButton : public LiVESButtonBase, public QPushButton {
 public:

 LiVESButton() {
    init();
  }

 LiVESButton(QString qs) {
   init();
 }

 LiVESWidget *get_layout() {
   if (layout == NULL) {
     layout = new LiVESVBox;
   }
   (static_cast<QPushButton *>(this))->setLayout(dynamic_cast<QLayout *>(layout));
   if ((static_cast<LiVESWidget *>(this))->isVisible()) layout->setVisible(true);
   add_child(layout);
   return layout;
 }

 private:
 LiVESWidget *layout;
 void init() {
   set_type(LIVES_WIDGET_TYPE_BUTTON);
 }

};




class LiVESSpinButton : public LiVESButtonBase, public QDoubleSpinBox {
 public:

 LiVESSpinButton(LiVESAdjustment *xadj, double climb_rate, uint32_t digits) : adj(xadj) {
    init();

    adj->set_owner(this);

    setDecimals(digits);
    setSingleStep(climb_rate);

    setValue(adj->get_value());
    setMinimum(adj->get_lower());
    setMaximum(adj->get_upper());

  }

  LiVESAdjustment *get_adj() {
    return adj;
  }

  void valueChanged(double value) {
    adj->freeze();
    adj->set_value(value);
    adj->thaw();
  }

 private:
 LiVESAdjustment *adj;

 void init() {
   set_type(LIVES_WIDGET_TYPE_SPIN_BUTTON);
 }

};




class LiVESRadioButton : public LiVESButtonBase, public QPushButton {
 public:
  LiVESRadioButton() {
    init();
  }

 LiVESRadioButton(QString qs) {
   init();
 }

 void set_group(LiVESSList *xlist) {
   slist = xlist;
 }

 LiVESSList *get_list() {
   return slist;
 }


  ~LiVESRadioButton() {
    QButtonGroup *qbg = (QButtonGroup *)lives_slist_nth_data(slist, 0);
    QList<QAbstractButton *> ql = qbg->buttons();
    if (ql.size() == 1) lives_slist_remove(slist, (livesconstpointer)qbg);
  }

 private:
 LiVESSList *slist;

 void init() {
   setCheckable(true);
   set_type(LIVES_WIDGET_TYPE_RADIO_BUTTON);
 }

};



class LiVESCheckButton : public LiVESButtonBase, public QCheckBox {
 public:

  LiVESCheckButton() {
    init();
  }

 LiVESCheckButton(QString qs) {
   init();
 }

 private:
 void init() {
    set_type(LIVES_WIDGET_TYPE_CHECK_BUTTON);
 }

};



class LiVESButtonBox : public LiVESWidget, public QDialogButtonBox {
 public:
  LiVESButtonBox() {
    set_type(LIVES_WIDGET_TYPE_BUTTON_BOX);
  }

};



class LiVESMenuBar : public LiVESWidget, public QMenuBar {  
 public:
  LiVESMenuBar() {
    set_type(LIVES_WIDGET_TYPE_MENU_BAR);
  }


  void reorder_child(LiVESWidget *child, int pos) {
    LiVESSList *children = get_children();
    LiVESSList *new_children = new LiVESSList;
    QMenuBar *qmenu = static_cast<QMenuBar *>(this);

    for (int i = 0; i < children->size(); i++) {
      if (i==pos) new_children->append(child);
      else if (children->at(i) != (livespointer)child) new_children->append(children[i]);
      qmenu->removeAction((QAction *)(children->at(i)));
    }

    for (int i = 0; i < new_children->size(); i++) {
      qmenu->addAction((QAction *)(new_children->at(i)));
    }

    set_children(new_children);
  }



};




class LiVESMenu : public LiVESWidget, public QMenu {  
 public:
  LiVESMenu() {
    set_type(LIVES_WIDGET_TYPE_MENU);
  }

  void reorder_child(LiVESWidget *child, int pos) {
    LiVESSList *children = get_children();
    LiVESSList *new_children = new LiVESSList;
    QMenu *qmenu = static_cast<QMenu *>(this);

    for (int i = 0; i < children->size(); i++) {
      if (i==pos) new_children->append(child);
      else if (children->at(i) != (livespointer)child) new_children->append(children[i]);
      qmenu->removeAction((QAction *)(children->at(i)));
    }

    for (int i = 0; i < new_children->size(); i++) {
      qmenu->addAction((QAction *)(new_children->at(i)));
    }

    set_children(new_children);
  }

};


class LiVESTornOffMenu : public LiVESMenu {  
 public:
  LiVESTornOffMenu(LiVESMenu *menu) {
    QWidget *qmenu = static_cast<QWidget *>(static_cast<QMenu *>(menu));
    QWidget *parentWidget = qmenu->parentWidget();
    QWidget *qwidget = (static_cast<QWidget *>(static_cast<QMenu *>(this)));
    qwidget->setParent(parentWidget, Qt::Window | Qt::Tool);
    qwidget->setAttribute(Qt::WA_DeleteOnClose, true);
    qwidget->setAttribute(Qt::WA_X11NetWmWindowTypeMenu, true);
    qwidget->setWindowTitle(qmenu->windowTitle());
    qwidget->setEnabled(qmenu->isEnabled());

    QList<QAction*> items = qmenu->actions();
    for (int i = 0; i < items.count(); i++)
      qwidget->addAction(items.at(i));

    set_parent(menu->get_parent());

    if (qmenu->isVisible()) {
      qmenu->hide();
      qwidget->setVisible(true);
    }

  }
};





class LiVESMenuItem : public LiVESWidget, public QAction {  
 public:
 LiVESMenuItem(LiVESWidget *parent) : QAction(static_cast<QObject *>(static_cast<LiVESObject *>(parent))) {
    set_type(LIVES_WIDGET_TYPE_MENU_ITEM);
  }

 LiVESMenuItem(const QString &text, LiVESWidget *parent) : QAction(text, static_cast<QObject *>(static_cast<LiVESObject *>(parent))) {
    set_type(LIVES_WIDGET_TYPE_MENU_ITEM);
    this->setText(text);
  }

  

};




typedef LiVESMenuItem LiVESImageMenuItem;



class LiVESMenuToolButton : public LiVESWidget, public QWidgetAction {  
 public:

 LiVESMenuToolButton(const QString &text, LiVESWidget *parent, LiVESWidget *icon) : 
  QWidgetAction(static_cast<QObject *>(static_cast<LiVESObject *>(parent))) {
    QPushButton *qbutton = new QPushButton(text);
    set_type(LIVES_WIDGET_TYPE_MENU_TOOL_BUTTON);
    setDefaultWidget(qbutton);
    //setIcon(qicon);
  }

};




class LiVESCheckMenuItem : public LiVESWidget, public QAction {  
 public:
 LiVESCheckMenuItem(LiVESWidget *parent) : QAction(static_cast<QObject *>(static_cast<LiVESObject *>(parent))) {
  }

 LiVESCheckMenuItem(const QString &text, LiVESWidget *parent) : QAction(text, static_cast<QObject *>(static_cast<LiVESObject *>(parent))) {
    setText(text);
  }

 private:
  void init() {
    set_type(LIVES_WIDGET_TYPE_CHECK_MENU_ITEM);
    setCheckable(true);
  }


};


class LiVESRadioMenuItem : public LiVESMenuItem {  
 public:

 LiVESRadioMenuItem(LiVESWidget *parent) : LiVESMenuItem(parent) {
    init();
  }

 LiVESRadioMenuItem(const QString &text, LiVESWidget *parent) : LiVESMenuItem(text, parent) {
    init();
    setText(text);
  }


 void set_group(LiVESSList *xlist) {
   slist = xlist;
 }

 LiVESSList *get_list() {
   return slist;
 }


  ~LiVESRadioMenuItem() {
    QActionGroup *qag = const_cast<QActionGroup *>(static_cast<const QActionGroup *>(lives_slist_nth_data(slist, 0)));
    QList<QAction *> ql = qag->actions();
    if (ql.size() == 1) lives_slist_remove(slist, (livesconstpointer)qag);
  }

 private:
 LiVESSList *slist;

 void init() {
   setCheckable(true);
   set_type(LIVES_WIDGET_TYPE_RADIO_MENU_ITEM);
 }
};




class LiVESCombo : public LiVESWidget, public QComboBox {  
 public:
  LiVESCombo() {
    set_type(LIVES_WIDGET_TYPE_COMBO);
  }
};


class LiVESHSeparator : public LiVESWidget, public QHBoxLayout {  
 public:
  LiVESHSeparator() {
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    addWidget(line);
    set_type(LIVES_WIDGET_TYPE_HSEPARATOR);
  }
};



class LiVESVSeparator : public LiVESWidget, public QVBoxLayout {  
 public:
  LiVESVSeparator() {
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    addWidget(line);
    set_type(LIVES_WIDGET_TYPE_VSEPARATOR);
  }
};



class LiVESTextBuffer : public LiVESWidget, public QTextDocument {  
 public:

  LiVESTextBuffer() {
    insert = QTextCursor(this);
  }

  QTextCursor get_cursor() {
    return insert;
  }

 private:
  QTextCursor insert;

};



class LiVESTextView : public LiVESWidget, public QTextEdit {  
 public:

  LiVESTextView() {
    buff = new LiVESTextBuffer();
  }


 LiVESTextView(LiVESTextBuffer *xbuff) : QTextEdit(), buff(xbuff) {
    set_type(LIVES_WIDGET_TYPE_TEXT_VIEW);
    setDocument(buff);
  }

  ~LiVESTextView() {
    delete buff;
  }


  LiVESTextBuffer *get_buffer() {
    return buff;
  }

 private:
  LiVESTextBuffer *buff;

  void init() {
    set_type(LIVES_WIDGET_TYPE_TEXT_VIEW);
    setDocument(buff);
  }


};



class LiVESTextMark : public LiVESObject, public QTextCursor {
 public:

 LiVESTextMark(LiVESTextBuffer *tbuff, const char *mark_name, 
	       int where, boolean left_gravity) : QTextCursor(tbuff), name(mark_name), lgrav(left_gravity) {
    (static_cast<QTextCursor *>(this))->setPosition(where);
  }

  ~LiVESTextMark() {
    delete name;
  }

  QString get_name() {
    return name;
  }

 private:
  const char *name;
  boolean lgrav;

};



class LiVESRange : public LiVESWidget {
  Q_OBJECT

 public:
  LiVESRange(LiVESAdjustment *adj) {
    init(adj);
  }

  virtual void init(LiVESAdjustment *adj) {}
  
  LiVESAdjustment *get_adj() {
    return adj;
  }

Q_SLOTS
  void valueChanged(int value) {
    adj->freeze();
    adj->set_value(value);
    adj->thaw();
  }

 private:
  LiVESAdjustment *adj;  

};


class LiVESScale : public LiVESRange, public QSlider {  
 public:
 LiVESScale(Qt::Orientation, LiVESAdjustment *xadj) : LiVESRange(xadj) {
    set_type(LIVES_WIDGET_TYPE_SCALE);
  }

  void init(LiVESAdjustment *adj) {
    adj->set_owner(this);
    setMinimum(adj->get_lower());
    setMaximum(adj->get_upper());
    setValue(adj->get_value());
    setSingleStep(adj->get_step_increment());
    setPageStep(adj->get_page_increment());
  }
};


class LiVESScrollbar : public LiVESRange, public QScrollBar {  
 public:
 LiVESScrollbar(Qt::Orientation, LiVESAdjustment *xadj) : LiVESRange(xadj) {
    set_type(LIVES_WIDGET_TYPE_SCROLLBAR);
  }

  void init(LiVESAdjustment *adj) {
    adj->set_owner(this);
    setMinimum(adj->get_lower());
    setMaximum(adj->get_upper());
    setValue(adj->get_value());
    setSingleStep(adj->get_step_increment());
    setPageStep(adj->get_page_increment());
  }

};


class LiVESEntry : public LiVESWidget, public QLineEdit {  
 public:
  LiVESEntry() {
    set_type(LIVES_WIDGET_TYPE_ENTRY);
  }
};


class LiVESProgressBar : public LiVESWidget, public QProgressBar {  
 public:
  LiVESProgressBar() {
    set_type(LIVES_WIDGET_TYPE_PROGRESS_BAR);
  }
};


class LiVESPaned : public LiVESWidget, public QSplitter {
 public:

  LiVESPaned() {
    set_type(LIVES_WIDGET_TYPE_PANED);
  }
};


class LiVESLabel : public LiVESWidget, public QLabel {
 public:
  LiVESLabel(QString qstring) {
    set_type(LIVES_WIDGET_TYPE_LABEL);
  }

  LiVESWidget *get_mnemonic_widget() {
    return mnemonicw;
  }

  void set_mnemonic_widget(LiVESWidget *widget) {
    mnemonicw = widget;
  }

  void set_owner(LiVESWidget *xowner) {
    owner = xowner;
  }

  LiVESWidget *get_owner() {
    return owner;
  }

  void set_text(QString text) {
    // if we have an owner, update owner text
    // TODO
  }


 private:
  LiVESWidget *mnemonicw;
  LiVESWidget *owner; // TODO
};


class LiVESRuler : public LiVESWidget, public QSlider {
 public:
  LiVESRuler() {
    set_type(LIVES_WIDGET_TYPE_RULER);
  }
};




class LiVESScrolledWindow : public LiVESWidget, public QScrollArea {
 public:
 LiVESScrolledWindow(LiVESAdjustment *xhadj, LiVESAdjustment *xvadj) : hadj(xhadj), vadj(xvadj) {
    set_type(LIVES_WIDGET_TYPE_SCROLLED_WINDOW);
    hadj->set_owner(this);
    vadj->set_owner(this);

    QScrollBar *hs = horizontalScrollBar();
    hs->setMinimum(hadj->get_lower());
    hs->setMaximum(hadj->get_upper());
    hs->setValue(hadj->get_value());
    hs->setSingleStep(hadj->get_step_increment());
    hs->setPageStep(hadj->get_page_increment());

    QScrollBar *vs = verticalScrollBar();
    vs->setMinimum(vadj->get_lower());
    vs->setMaximum(vadj->get_upper());
    vs->setValue(vadj->get_value());
    vs->setSingleStep(vadj->get_step_increment());
    vs->setPageStep(vadj->get_page_increment());

  }

  LiVESAdjustment *get_hadj() {
    return hadj;
  }

  LiVESAdjustment *get_vadj() {
    return vadj;
  }

 private:
  LiVESAdjustment *hadj;
  LiVESAdjustment *vadj;

};


typedef LiVESWidget LiVESToolItem;


class LiVESToolButton : public LiVESButtonBase, public QToolButton {
 public:

  LiVESToolButton(LiVESWidget *icon_widget, const char *label) {
    set_type(LIVES_WIDGET_TYPE_TOOL_BUTTON);
    set_icon_widget(icon_widget);
    layout = new LiVESVBox;
    QBoxLayout *ql = static_cast<QVBoxLayout *>(layout);
    ql->setParent(static_cast<QToolButton *>(this));
    add_child(layout);
    if (label != NULL) {
      LiVESWidget *widget = new LiVESLabel(label);
      layout->add_child(widget);
      label_widget = dynamic_cast<LiVESLabel *>(widget);
      (static_cast<QBoxLayout *>(layout))->addWidget(static_cast<QLabel *>(label_widget));
    }
    else label_widget = NULL;
  }


  void set_icon_widget(LiVESWidget *widget) {
    if (icon_widget != NULL) remove_child(icon_widget);
    icon_widget = NULL;
    QImage *qim = NULL;
    if (widget != NULL) qim = dynamic_cast<QImage *>(widget);
    if (qim) {
      QPixmap *qpx = new QPixmap();
      qpx->convertFromImage(*qim);
      QIcon *qi = new QIcon(*qpx);
      setIcon(*qi);
      setIconSize(LIVES_ICON_SIZE_SMALL_TOOLBAR);
      icon_widget = widget;
      add_child(widget);
    }
  }


  void set_label_widget(LiVESWidget *widget) {
    if (label_widget != NULL) {
      layout->remove_child(static_cast<LiVESWidget *>(label_widget));
      layout->removeWidget(static_cast<LiVESWidget *>(label_widget));
    }
    if (widget != NULL) {
      layout->addWidget(widget);
      label_widget = dynamic_cast<LiVESLabel *>(widget);
      layout->add_child(label_widget);
      label_widget->set_owner(this);
    }
    else label_widget = NULL;
  }

 private:
  LiVESWidget *icon_widget;
  LiVESLabel *label_widget;
  LiVESVBox *layout;
  
};




class LiVESToggleButton : public LiVESButtonBase, public QAbstractButton {};




typedef class LiVESWindow LiVESWindow;

class LiVESAccelGroup {
 public:
  void add_all_accelerators(LiVESObject *window);
  void add_widget(LiVESObject *window);
  void remove_all_accelerators(LiVESObject *window);
  void remove_widget(LiVESObject *window);

  void connect(uint32_t key, LiVESXModifierType mod, LiVESAccelFlags flags, LiVESWidgetClosure *closure);
  void disconnect(LiVESWidgetClosure *closure);

  ~LiVESAccelGroup();

 private:
  QList<LiVESObject *>widgets;
  QList<LiVESAccel *>accels;
  

};




void LiVESObject::add_accel_group(LiVESAccelGroup *group) {
  accel_groups.push_back(group);
  group->add_all_accelerators(this);
  group->add_widget(this);
}



boolean LiVESObject::remove_accel_group(LiVESAccelGroup *group) {
  if (accel_groups.removeAll(group) > 0) {
    group->remove_all_accelerators(this);
    group->remove_widget(this);
    return TRUE;
  }
  return FALSE;
}



boolean LiVESObject::remove_accels(LiVESAccelGroup *group, uint32_t key, LiVESXModifierType mods) {
  bool ret = false;
  QKeySequence ks = make_qkey_sequence(key, mods);

  QList<LiVESAccel *>::iterator it = accels.begin();
  while (it != accels.end()) {
    if (((LiVESAccel *)*it)->group == group && ((LiVESAccel *)*it)->ks == ks) {
      remove_accel((LiVESAccel *)*it);
      ret = true;
    }
    else
      ++it;
  }
  return ret;
}


QList<LiVESAccel *> LiVESObject::get_accels_for(LiVESAccelGroup *group, QKeySequence ks) {
  QList<LiVESAccel *> ql;
  for (int i=0; i < accels.size(); i++) {
    if (accels[i]->group == group && accels[i]->ks == ks) ql.push_back(accels[i]);
  }
  return ql;
}



class LiVESWindow : public LiVESWidget {};

typedef int LiVESWindowPosition;
#define LIVES_WIN_POS_DEFAULT 0
#define LIVES_WIN_POS_CENTER_ALWAYS 1


class LiVESMainWindow : public LiVESWindow, public QMainWindow {
 public:
  LiVESMainWindow() {
    set_type(LIVES_WIDGET_TYPE_MAIN_WINDOW);
    set_position(LIVES_WIN_POS_DEFAULT);
  }

  LiVESWidget *get_layout() {
    if (layout == NULL) {
      layout = new LiVESVBox;
    }
    (static_cast<QMainWindow *>(this))->setLayout(dynamic_cast<QLayout *>(layout));
    if ((static_cast<LiVESWidget *>(this))->isVisible()) layout->setVisible(true);
    add_child(layout);
    return layout;
  }

  void set_position(LiVESWindowPosition xpos) {
    pos = xpos;
  }

  LiVESWindowPosition get_position() {
    return pos;
  }


 private:
  LiVESWidget *layout;
  LiVESWindowPosition pos;

};



void LiVESAccelGroup::add_all_accelerators(LiVESObject *object) {
  for (int i=0; i < accels.size(); i++) {
    object->add_accel(accels.at(i));
  }
}

void LiVESAccelGroup::add_widget(LiVESObject *window) {
  widgets.push_back(window);
}


void LiVESAccelGroup::remove_all_accelerators(LiVESObject *object) {
  while (accels.size() > 0) {
    object->remove_accel(accels.at(0));
  }
}


void LiVESAccelGroup::remove_widget(LiVESObject *window) {
  widgets.removeAll(window);
}


LiVESAccelGroup::~LiVESAccelGroup() {
  while (widgets.size() > 0) {
    remove_all_accelerators(widgets.at(0));
  }
}


class LiVESVLayout: public LiVESWidget, public QVBoxLayout {};

class LiVESDialog : public LiVESWindow, public QDialog {
 public:
  LiVESDialog() {
    QDialog *qd = static_cast<QDialog *>(this);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(0);
    qd->setLayout(layout);

    contentArea = new LiVESVLayout();
    QVBoxLayout *ca = dynamic_cast<QVBoxLayout *>(contentArea);
    ca->setMargin(0);
    layout->insertLayout(0, ca);

    actionArea = new LiVESButtonBox;
    QDialogButtonBox *bb = dynamic_cast<QDialogButtonBox *>(actionArea);
    bb->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(bb);

    set_type(LIVES_WIDGET_TYPE_DIALOG);

    add_child(actionArea);
    add_child(contentArea);

  }

  LiVESWidget *get_content_area() {
    return contentArea;
  }

  LiVESWidget *get_action_area() {
    return actionArea;
  }

  LiVESWidget *get_layout() {
    if (layout == NULL) {
      layout = new LiVESVBox;
    }
    (static_cast<QDialog *>(this))->setLayout(dynamic_cast<QLayout *>(layout));
    if ((static_cast<LiVESWidget *>(this))->isVisible()) layout->setVisible(true);
    add_child(layout);
    return layout;
  }

 private:
  LiVESWidget *layout;
  LiVESWidget *contentArea;
  LiVESWidget *actionArea;
};


class LiVESAlignment: public LiVESWidget, public QGridLayout {
 public:
  float xalign, yalign, xscale, yscale;

 LiVESAlignment(float xxalign, float yyalign, float xxscale, float yyscale) :
  xalign(xxalign),
    yalign(yyalign),
    xscale(xxscale),
    yscale(yyscale)
  {
    set_type(LIVES_WIDGET_TYPE_ALIGNMENT);
  }

 void set_alignment(float xxalign, float yyalign, float xxscale, float yyscale) 
 {
   xalign = xxalign;
   yalign = yyalign;
   xscale = xxscale;
   yscale = yyscale;

   // do something with layout spacers
   // or setContentsmargins()

 }


};


class LiVESImage : public LiVESWidget, public QImage {
 public:

 LiVESImage() {
   init();
 }

 LiVESImage (QImage *lim) {
   init();
 };

 LiVESImage(int width, int height, QImage::Format fmt) {
   init();
 }

 LiVESImage(uint8_t *data, int width, int height, int bpl, QImage::Format fmt, QImageCleanupFunction cleanupFunction, livespointer cleanupInfo) {
   init();
 }

 private:
 void init() {
   set_type(LIVES_WIDGET_TYPE_IMAGE);
 }

};




class LiVESArrow : public LiVESImage {
 public:
 LiVESArrow(QImage *image) {
    set_type(LIVES_WIDGET_TYPE_ARROW);
  }
};



typedef int LiVESCellRenderer;
#define LIVES_CELL_RENDERER_TEXT 1
#define LIVES_CELL_RENDERER_SPIN 2
#define LIVES_CELL_RENDERER_TOGGLE 3
#define LIVES_CELL_RENDERER_PIXBUF 4


typedef struct {
  const char *attr;
  int col;
} tvattrcol;

typedef QHeaderView::ResizeMode LiVESTreeViewColumnSizing;

#define LIVES_TREE_VIEW_COLUMN_GROW_ONLY QHeaderView::Stretch
#define LIVES_TREE_VIEW_COLUMN_AUTOSIZE QHeaderView::ResizeToContents
#define LIVES_TREE_VIEW_COLUMN_FIXED QHeaderView::Fixed

typedef class LiVESTreeView LiVESTreeView;
typedef LiVESTreeView LiVESTreeSelection;


class LiVESTreeViewColumn : public QStyledItemDelegate {
  friend LiVESTreeView;

 public:

  LiVESTreeViewColumn() {
    fwidth = -1;
  }

  void set_title(const char *xtitle) {
    title = strdup(xtitle);
  }

  void pack_start(LiVESCellRenderer *xrend, boolean xexpand) {
    rend = xrend;
    expand = xexpand;
  }

  void add_attribute(const char *attr, int col) {
    tvattrcol *attrcol = new tvattrcol;
    attrcol->attr = strdup(attr);
    attrcol->col = col;
    attributes.push_back(attrcol);
  }

  void set_sizing(LiVESTreeViewColumnSizing xsizing) {
    sizing = xsizing;
  }


  void set_fixed_width(int xfwidth) {
    fwidth = xfwidth;
  }


  QList<tvattrcol *> get_attributes() {
    return attributes;
  }


  ~LiVESTreeViewColumn() {
    delete title;
    for (int i=0; i < attributes.size(); i++) {
      delete attributes[i]->attr;
    }
  }


 protected:
  int fwidth;
  LiVESTreeViewColumnSizing sizing;
  QList<tvattrcol *>attributes;
  const char *title;
  LiVESCellRenderer *rend;
  boolean expand;
};





typedef QAbstractItemView::SelectionMode LiVESSelectionMode;
#define LIVES_SELECTION_NONE QAbstractItemView::NoSelection
#define LIVES_SELECTION_SINGLE QAbstractItemView::SingleSelection
#define LIVES_SELECTION_MULTIPLE QAbstractItemView::MultiSelection


typedef class LiVESTreeStore LiVESTreeStore;


class LiVESTreeModel : public LiVESObject, public QStandardItemModel {
  friend LiVESTreeStore;

 public:
  
  QStandardItemModel *to_qsimodel() {
    QStandardItemModel *qsim = static_cast<QStandardItemModel *>(this);
    QVariant qv = QVariant::fromValue(static_cast<LiVESObject *>(this));
    qsim->setProperty("LiVESObject", qv);
    return qsim;
  }

  int get_coltype(int index) {
    return coltypes[index];
  }

  void set_tree_widget(LiVESTreeView *twidget);
  QTreeWidget *get_qtree_widget();

 protected:
  void set_coltypes(int ncols, int *types) {
    for (int i = 0; i < ncols; i++) {
      coltypes.append(types[i]);
    }
  }


 private:
  LiVESTreeView *widget;
  QList<int> coltypes;

};



class LiVESTreeView : public LiVESWidget, public QTreeWidget {
  Q_OBJECT

 public:
  LiVESTreeView() {
    set_type(LIVES_WIDGET_TYPE_TREE_VIEW);

    QAbstractSlider *sbar = static_cast<QAbstractSlider *>(horizontalScrollBar());
    hadj = new LiVESAdjustment(sbar->value(), sbar->minimum(), sbar->maximum(), sbar->singleStep(), sbar->pageStep(), -1.);
    hadj->set_owner(this);
    sbar->connect(sbar, SIGNAL(valueChanged(int)), static_cast<QObject *>(static_cast<LiVESObject *>(this)), SLOT(hvalue_changed(int)));

    sbar = static_cast<QAbstractSlider *>(verticalScrollBar());
    vadj = new LiVESAdjustment(sbar->value(), sbar->minimum(), sbar->maximum(), sbar->singleStep(), sbar->pageStep(), -1.);
    vadj->set_owner(this);
    sbar->connect(sbar, SIGNAL(valueChanged(int)), static_cast<QObject *>(static_cast<LiVESObject *>(this)), SLOT(vvalue_changed(int)));

  }

  void set_model (LiVESTreeModel *xmodel) {
    model = xmodel;
    xmodel->set_tree_widget(this);
    QStandardItemModel *qmodel = xmodel->to_qsimodel();
    (static_cast<QAbstractItemView *>(this))->setModel(static_cast<QAbstractItemModel *>(qmodel));
  }

  LiVESTreeModel *get_model() {
    return model;
  }



  void append_column(LiVESTreeViewColumn *col) {
    // make QList from data in column x

    // get stuff from attributes
    QList<tvattrcol *> ql = col->get_attributes();
    QList<QStandardItem *> qvals;
    QStandardItemModel *qmodel = model->to_qsimodel();

    int attrcol;

    for (int i=0; i < ql.size(); i++) {
      attrcol = ql[i]->col;
      if (!strcmp(ql[i]->attr,"text")) {
	// TODO
	// make QList of QString from model col 
	qmodel->appendColumn(qvals);
      }

      else if (!strcmp(ql[i]->attr,"pixbuf")) {
	// make QList of QIcons from model col 
	qmodel->appendColumn(qvals);
      }

      else if (!strcmp(ql[i]->attr,"active")) {
	// make QList of checkable from model col 
	qmodel->appendColumn(qvals);
      }
    }
    
    int newcol = qmodel->columnCount();
    QTreeView *qtv = static_cast<QTreeView *>(this);
    if (col->fwidth != -1) {
      qtv->setColumnWidth(newcol, col->fwidth);
    }

    QHeaderView *qv = (static_cast<QTreeView *>(this))->header();
    qv->setSectionResizeMode(newcol, col->sizing);
    
    //resizeColumnToContents()

  }

  LiVESAdjustment *get_hadj() {
    return hadj;
  }

  LiVESAdjustment *get_vadj() {
    return vadj;
  }


Q_SLOTS

  void hvalue_changed(int newval) {
    hadj->freeze();
    hadj->set_value(newval);
    hadj->thaw();
  }

  void vvalue_changed(int newval) {
    vadj->freeze();
    vadj->set_value(newval);
    vadj->thaw();
  }


 private:
  LiVESTreeModel *model;
  LiVESAdjustment *hadj, *vadj;
};


#define LIVES_COL_TYPE_STRING 1
#define LIVES_COL_TYPE_INT 2
#define LIVES_COL_TYPE_UINT 3
#define LIVES_COL_TYPE_BOOLEAN 4
#define LIVES_COL_TYPE_PIXBUF 5


typedef QTreeWidgetItem LiVESTreeIter;

void LiVESTreeModel::set_tree_widget(LiVESTreeView *twidget) {
  widget = twidget;
}

QTreeWidget *LiVESTreeModel::get_qtree_widget() {
  return static_cast<QTreeWidget *>(widget);
}



class LiVESTreeStore : public LiVESTreeModel {
 public:
  LiVESTreeStore (int ncols, int *types) {}

};



void LiVESAdjustment::set_value(double newval) {
  if (newval != value) {
    value = newval;

    if (frozen) return;

    if (LIVES_IS_SCALE(owner)) {
      (dynamic_cast<QAbstractSlider *>(owner))->setValue(newval);
    }
    else if (LIVES_IS_SCROLLBAR(owner)){
      //
      (dynamic_cast<QScrollBar *>(owner))->setValue(newval);
    }
    else if (LIVES_IS_SPIN_BUTTON(owner)){
      //
      (dynamic_cast<QDoubleSpinBox *>(owner))->setValue(newval);
    }
    else if (LIVES_IS_TREE_VIEW(owner)) {
      if (this == (dynamic_cast<LiVESTreeView *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setValue(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setValue(newval);
      }
    }
    else if (LIVES_IS_SCROLLED_WINDOW(owner)) {
      if (this == (dynamic_cast<LiVESScrolledWindow *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setValue(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setValue(newval);
      }
    }
  }
}


void LiVESAdjustment::set_upper(double newval) {
  if (newval != upper) {
    upper = newval;

    if (frozen) return;

    if (LIVES_IS_SCALE(owner)) {
      (dynamic_cast<QAbstractSlider *>(owner))->setMaximum(newval);
    }
    else if (LIVES_IS_SCROLLBAR(owner)){
      //
      (dynamic_cast<QScrollBar *>(owner))->setMaximum(newval);
    }
    else if (LIVES_IS_SPIN_BUTTON(owner)){
      //
      (dynamic_cast<QDoubleSpinBox *>(owner))->setMaximum(newval);
    }
    else if (LIVES_IS_TREE_VIEW(owner)) {
      if (this == (dynamic_cast<LiVESTreeView *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setMaximum(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setMaximum(newval);
      }
    }
    else if (LIVES_IS_SCROLLED_WINDOW(owner)) {
      if (this == (dynamic_cast<LiVESScrolledWindow *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setMaximum(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setMaximum(newval);
      }
    }
  }


}



void LiVESAdjustment::set_lower(double newval) {
  if (newval != lower) {
    lower = newval;
    if (frozen) return;

    if (LIVES_IS_SCALE(owner)) {
      (dynamic_cast<QAbstractSlider *>(owner))->setMinimum(newval);
    }
    else if (LIVES_IS_SCROLLBAR(owner)){
      //
      (dynamic_cast<QScrollBar *>(owner))->setMinimum(newval);
    }
    else if (LIVES_IS_SPIN_BUTTON(owner)){
      //
      (dynamic_cast<QDoubleSpinBox *>(owner))->setMinimum(newval);
    }
    else if (LIVES_IS_TREE_VIEW(owner)) {
      if (this == (dynamic_cast<LiVESTreeView *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setMinimum(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setMinimum(newval);
      }
    }
    else if (LIVES_IS_SCROLLED_WINDOW(owner)) {
      if (this == (dynamic_cast<LiVESScrolledWindow *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setMinimum(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setMinimum(newval);
      }
    }
  }
}



void LiVESAdjustment::set_step_increment(double newval) {
  if (newval != step_increment) {
    step_increment = newval;

    if (frozen) return;

    if (LIVES_IS_SCALE(owner)) {
      (dynamic_cast<QAbstractSlider *>(owner))->setSingleStep(newval);
    }
    else if (LIVES_IS_SCROLLBAR(owner)){
      //
      (dynamic_cast<QScrollBar *>(owner))->setSingleStep(newval);
    }
    else if (LIVES_IS_SPIN_BUTTON(owner)){
      //
      (dynamic_cast<QDoubleSpinBox *>(owner))->setSingleStep(newval);
    }
    else if (LIVES_IS_TREE_VIEW(owner)) {
      if (this == (dynamic_cast<LiVESTreeView *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setSingleStep(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setSingleStep(newval);
      }
    }
    else if (LIVES_IS_SCROLLED_WINDOW(owner)) {
      if (this == (dynamic_cast<LiVESScrolledWindow *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setSingleStep(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setSingleStep(newval);
      }
    }
  }
}


void LiVESAdjustment::set_page_increment(double newval) {
  if (newval != page_increment) {
    page_increment = newval;

    if (frozen) return;

    if (LIVES_IS_SCALE(owner)) {
      (dynamic_cast<QAbstractSlider *>(owner))->setPageStep(newval);
    }
    else if (LIVES_IS_SCROLLBAR(owner)){
      //
      (dynamic_cast<QScrollBar *>(owner))->setPageStep(newval);
    }
    else if (LIVES_IS_TREE_VIEW(owner)) {
      if (this == (dynamic_cast<LiVESTreeView *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setPageStep(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setPageStep(newval);
      }
    }
    else if (LIVES_IS_SCROLLED_WINDOW(owner)) {
      if (this == (dynamic_cast<LiVESScrolledWindow *>(owner))->get_hadj()) {
	(dynamic_cast<QAbstractScrollArea *>(owner))->horizontalScrollBar()->setPageStep(newval);
      }
      else {
	(dynamic_cast<QAbstractScrollArea *>(owner))->verticalScrollBar()->setPageStep(newval);
      }
    }
  }
}


void LiVESAdjustment::set_page_size(double newval) {
  // TODO
  if (newval != page_size) {
    page_size = newval;

    if (frozen) return;
  }
}



class LiVESTable : public LiVESWidget, public QGridLayout {

 public:
 LiVESTable() {
    set_type(LIVES_WIDGET_TYPE_TABLE);
  }


 private:
  LiVESWidget *layout;

};


typedef QWindow LiVESXWindow;

typedef class LiVESImage LiVESPixbuf;

typedef LiVESWidget LiVESEditable;
typedef LiVESWidget LiVESContainer;
typedef LiVESWidget LiVESMenuShell;
typedef LiVESWidget LiVESBin;

typedef QSlider LiVESScaleButton; // TODO - create
typedef void LiVESExpander; // TODO - create



// scrolledwindow policies
typedef Qt::ScrollBarPolicy LiVESPolicyType;
#define LIVES_POLICY_AUTOMATIC Qt::ScrollBarAsNeeded
#define LIVES_POLICY_NEVER Qt::ScrollBarAlwaysOff
#define LIVES_POLICY_ALWAYS Qt::ScrollBarAlwaysOn


class LiVESFrame : public LiVESWidget, public QGroupBox {
 public:
  LiVESFrame(QString text) {
    set_type(LIVES_WIDGET_TYPE_FRAME);

    LiVESWidget *label_widget = new LiVESLabel(text);
    set_label_widget(label_widget);
  }


  void set_label(QString text) {
    label_widget->set_text(text);
  }


  void set_label_widget(LiVESWidget *widget) {
    if (label_widget != NULL) {
      remove_child(static_cast<LiVESWidget *>(label_widget));
    }
    if (widget != NULL) {
      add_child(label_widget);
      label_widget = dynamic_cast<LiVESLabel *>(widget);
      label_widget->set_owner(this);
      setTitle(label_widget->text());
    }
    else {
      label_widget = NULL;
      setTitle(NULL);
    }

  }

  LiVESWidget *get_label_widget() {
    return static_cast<LiVESWidget *>(label_widget);
  }



  LiVESWidget *get_layout() {
    if (layout == NULL) {
      layout = new LiVESVBox;
    }
    (static_cast<QGroupBox *>(this))->setLayout(dynamic_cast<QLayout *>(layout));
    if ((static_cast<LiVESWidget *>(this))->isVisible()) layout->setVisible(true);
    add_child(layout);
    return layout;
  }


 private:
  LiVESWidget *layout;
  LiVESLabel *label_widget;


};


class LiVESFileChooser : public LiVESWidget, public QFileDialog {
 public:
  LiVESFileChooser() {
    set_type(LIVES_WIDGET_TYPE_FILE_CHOOSER);
  }

};


typedef int LiVESFileChooserAction;
#define LIVES_FILE_CHOOSER_ACTION_OPEN 1
#define LIVES_FILE_CHOOSER_ACTION_SAVE 2
#define LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER 3
#define LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER 4
#define LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE 5


class LiVESNotebook : public LiVESWidget, public QTabWidget {
 public:

 LiVESNotebook() {
    set_type(LIVES_WIDGET_TYPE_NOTEBOOK);
  }

 ~LiVESNotebook() {
   for (int i = 0; i < label_widgets.size(); i++) {
     label_widgets[i]->dec_refcount();
   }
 }

 void set_tab_label(LiVESWidget *child, LiVESWidget *xlabel) {
   int i = get_child_index(child);
   if (i == -1) return;

   if (xlabel != NULL) {
     label_widgets[i]->dec_refcount();
     LiVESLabel *label = dynamic_cast<LiVESLabel *>(xlabel);
     label_widgets[i] = label;
     label->set_owner(this);
     QLabel *qlabel =static_cast<QLabel *>(label);
     setTabText(i, qlabel->text());
   }
 }
 

 void append_page() {
   LiVESWidget *label_widget = new LiVESLabel(NULL);
   label_widgets.append(label_widget);
 }

 private:
 QList <LiVESWidget *> label_widgets;


};



class LiVESToolbar : public LiVESWidget, public QToolBar {
 public:

  LiVESToolbar() {
    set_type(LIVES_WIDGET_TYPE_TOOLBAR);
  }

  void add_action(QAction *act, int pos) {
    actions.insert(pos, act);
  }

  QAction *get_action(int pos) {
    return actions.at(pos);
  }

  int num_actions() {
    return actions.size();
  }

 private:
  QList<QAction *>actions;


};



class LiVESColorButton : public LiVESButtonBase, public QPushButton {
  Q_OBJECT

 public:

  LiVESColorButton(const LiVESWidgetColor *col) {
    set_type(LIVES_WIDGET_TYPE_COLOR_BUTTON);
    static_cast<QObject *>(static_cast<QPushButton *>(this))->connect(static_cast<QObject *>(static_cast<QPushButton *>(this)),
								      SIGNAL(clicked()),
								      static_cast<QObject *>(static_cast<QPushButton *>(this)),
								      SLOT(onClicked()));

    use_alpha = FALSE;
    set_colour(col);
  }

  void set_colour(const LiVESWidgetColor *col) {
    QColor xcolor(col->red * 255., col->green * 255., col->blue * 255., col->alpha * 255.);
    set_colour(xcolor);
  }

  void set_colour(QColor xcolour) {
    if (colour != xcolour) {
      colour = xcolour;
      QPushButton *qpb = static_cast<QPushButton *>(this);
      QPalette p = qpb->palette();
      QColor mycolour = xcolour;
      if (!use_alpha) mycolour.setAlpha(255);
      p.setColor(QPalette::Button, mycolour);
      qpb->setPalette(p);
      
      Q_EMIT changed();
    }
  }


  QColor get_colour() {
    return colour;
  }

  void get_colour(LiVESWidgetColor *col) {
    col->red = (float)colour.red() / 255.;
    col->green = (float)colour.green() / 255.;
    col->blue = (float)colour.blue() / 255.;
    col->alpha = (float)colour.alpha() / 255.;
  }


  void set_title(const char *xtitle) {
    title = QString::fromUtf8(xtitle);
  }
  
  void set_use_alpha(boolean val) {
    use_alpha = val;
  }


 Q_SIGNALS:
  void changed();

  private Q_SLOTS:

    void onClicked() {
      QColor mycolour = colour;
      if (!use_alpha) mycolour.setAlpha(255);
      QColorDialog dlg(mycolour);
      dlg.setWindowTitle(title);
      dlg.setOption(QColorDialog::ShowAlphaChannel, use_alpha);
      if(dlg.exec() == QDialog::Accepted) {
	set_colour(dlg.selectedColor());
      }
      if (!use_alpha) colour.setAlpha(255);
    }

 private:
    QColor colour;
    boolean use_alpha;
    QString title;
};



class LiVESTimer : public LiVESObject, public QTimer {
  Q_OBJECT

 public:

  LiVESTimer(uint32_t interval, LiVESWidgetSourceFunc xfunc, livespointer data);

  uint32_t get_handle() {
    return handle;
  }

  Q_SLOTS

    void update();


 private:
  uint32_t handle;
  LiVESWidgetSourceFunc func;
  livespointer data;


};

static QList<LiVESTimer *> static_timers;


LiVESTimer::LiVESTimer(uint32_t interval, LiVESWidgetSourceFunc xfunc, livespointer data) {
  set_type(LIVES_OBJECT_TYPE_TIMER);
  static_timers.append(this);

  static_cast<QObject *>(static_cast<QTimer *>(this))->connect(static_cast<QObject *>(static_cast<QTimer *>(this)),
							       SIGNAL(timeout()),
							       static_cast<QObject *>(static_cast<QTimer *>(this)),
							       SLOT(update()));
  start(interval);

}


void LiVESTimer::update() {
  // ret false to stop
  boolean ret = (func)(data);
  if (!ret) {
    stop();
    static_timers.removeOne(this);
  }
}


void remove_static_timer(uint32_t handle) {
  for (int i = 0; i < static_timers.size(); i++) {
    if (static_timers.at(i)->get_handle() == handle) {
      static_timers.removeOne(static_timers.at(i));
      break;
    }
  }
}


typedef Qt::ToolButtonStyle LiVESToolbarStyle;
#define LIVES_TOOLBAR_ICONS Qt::ToolButtonIconOnly
#define LIVES_TOOLBAR_TEXT  Qt::ToolButtonTextOnly

typedef QSize LiVESRequisition;

typedef int LiVESTextIter;

class LiVESTreePath {
 public:

  LiVESTreePath(const char *string) {
    QString qs(string);
    QStringList qsl = qs.split(":");
    QList<int> qli;

    for (int i=0; i < qsl.size(); i++) {
      qli.append(qsl.at(i).toInt());
    }

    init(qli);
   }

  LiVESTreePath(QList<int> qli) {
    init(qli);
  }

  ~LiVESTreePath() {
    delete indices;
  }


  int get_depth() {
    return cnt;
  }


  int *get_indices() {
    return indices;
  }

 private:
  int *indices;
  int cnt;

  void init(QList<int> qli) {
    cnt = qli.size();
    indices = (int *)(malloc(cnt * sizeof(int)));
    for (int i=0; i < cnt; i++) {
      indices[i] = qli.at(i);
    }
  }

};


typedef Qt::Orientation LiVESOrientation;
#define LIVES_ORIENTATION_HORIZONTAL Qt::Horizontal
#define LIVES_ORIENTATION_VERTICAL   Qt::Vertical

typedef int LiVESButtonBoxStyle;
#define LIVES_BUTTONBOX_DEFAULT_STYLE 0
#define LIVES_BUTTONBOX_SPREAD 1
#define LIVES_BUTTONBOX_EDGE 2
#define LIVES_BUTTONBOX_CENTER 3

typedef int LiVESReliefStyle;

#define LIVES_RELIEF_NORMAL 2
#define LIVES_RELIEF_HALF 1
#define LIVES_RELIEF_NONE 0

typedef int LiVESWindowType;
#define LIVES_WINDOW_TOPLEVEL Qt::Window
#define LIVES_WINDOW_POPUP Qt::Popup

typedef QFrame::Shadow LiVESShadowType;
#define LIVES_SHADOW_NONE QFrame::Plain
#define LIVES_SHADOW_IN QFrame::Raised
#define LIVES_SHADOW_OUT QFrame::Sunken
#define LIVES_SHADOW_ETCHED_IN QFrame::Raised
#define LIVES_SHADOW_ETCHED_OUT QFrame::Sunken



typedef int LiVESPositionType;
#define LIVES_POS_LEFT 1
#define LIVES_POS_RIGHT 2
#define LIVES_POS_TOP 3
#define LIVES_POS_BOTTOM 4


#define LIVES_WIDGET(a) ((LiVESWidget *)a)


#define LIVES_EDITABLE(a) (a)

#define LIVES_CONTAINER(a) LIVES_WIDGET(a)
#define LIVES_GUI_OBJECT(a) LIVES_WIDGET(a)
#define LIVES_EXPANDER(a) LIVES_WIDGET(a)
#define LIVES_BIN(a) LIVES_WIDGET(a)

#define LIVES_WIDGET_OBJECT(a) ((LiVESObject *)a)
#define LIVES_COMBO(a) ((LiVESCombo *)a)
#define LIVES_HBOX(a) ((LiVESHBox *)a)
#define LIVES_VBOX(a) ((LiVESVBox *)a)
#define LIVES_BOX(a) ((LiVESBox *)a)
#define LIVES_DIALOG(a) ((LiVESDialog *)a)
#define LIVES_LABEL(a) ((LiVESLabel *)a)
#define LIVES_ENTRY(a) ((LiVESEntry *)a)
#define LIVES_ACCEL_GROUP(a) ((LiVESAccelGroup *)a)
#define LIVES_WINDOW(a) ((LiVESWindow *)a)
#define LIVES_SCROLLED_WINDOW(a) ((LiVESScrolledWindow *)a)
#define LIVES_TREE_MODEL(a) ((LiVESTreeModel *)a)



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
#define LIVES_STOCK_REFRESH "view-refresh"
#define LIVES_STOCK_MEDIA_PLAY "media-playback-start"
#define LIVES_STOCK_MEDIA_STOP "media-playback-stop"
#define LIVES_STOCK_MEDIA_REWIND "media-seek-backward"
#define LIVES_STOCK_MEDIA_RECORD "media-record"
#define LIVES_STOCK_MEDIA_PAUSE "media-pause"
#define LIVES_STOCK_PREFERENCES "preferences-system"
#define LIVES_STOCK_DIALOG_INFO "dialog-information"
#define LIVES_STOCK_MISSING_IMAGE "image-missing"


#define LIVES_STOCK_YES "gtk-yes"             // non-standard image ?
#define LIVES_STOCK_APPLY "gtk-apply"      // non-standard image ?
#define LIVES_STOCK_CANCEL "gtk-cancel"    // non-standard image ?
#define LIVES_STOCK_OK "gtk-ok"    // non-standard image ?


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


typedef int LiVESAttachOptions;
#define LIVES_EXPAND 1
#define LIVES_SHRINK 2
#define LIVES_FILL 3


//typedef int LiVESWrapMode;
//#define LIVES_WRAP_NONE QTextEdit::NoWrap
//#define LIVES_WRAP_WORD QTextEdit::WidgetWidth

typedef bool LiVESWrapMode;
#define LIVES_WRAP_NONE false
#define LIVES_WRAP_WORD true

typedef Qt::Alignment LiVESJustification;

#define LIVES_JUSTIFY_LEFT   Qt::AlignLeft
#define LIVES_JUSTIFY_RIGHT  Qt::AlignRight
#define LIVES_JUSTIFY_CENTER Qt::AlignHCenter
#define LIVES_JUSTIFY_FILL   Qt::AlignJustify

boolean lives_container_remove(LiVESContainer *, LiVESWidget *);

LiVESWidget::~LiVESWidget() {
  if (LIVES_IS_SPIN_BUTTON(this)) {
    LiVESAdjustment *adj = (static_cast<LiVESSpinButton *>(this))->get_adj();
    adj->dec_refcount();
  }

  if (LIVES_IS_RANGE(this)) {
    LiVESAdjustment *adj = (dynamic_cast<LiVESRange *>(this))->get_adj();
    adj->dec_refcount();
  }

  if (LIVES_IS_TREE_VIEW(this)) {
    LiVESAdjustment *adj = (static_cast<LiVESTreeView *>(this))->get_hadj();
    adj->dec_refcount();
    adj = (static_cast<LiVESTreeView *>(this))->get_vadj();
    adj->dec_refcount();
  }

  if (LIVES_IS_SCROLLED_WINDOW(this)) {
    LiVESAdjustment *adj = (static_cast<LiVESScrolledWindow *>(this))->get_hadj();
    adj->dec_refcount();
    adj = (static_cast<LiVESScrolledWindow *>(this))->get_vadj();
    adj->dec_refcount();
  }

  // remove from parents children
  if (parent != NULL) {
    inc_refcount();
    parent->remove_child(this);
  }

  // decref all children
  while (children->size() > 0) {
    lives_container_remove(this, (LiVESWidget *)children->at(0));
  }

  delete children;
}


#endif



#ifdef PAINTER_QPAINTER
# include <QtGui/QPainter>

extern void lives_free(gpointer ptr);

static void imclean(livespointer data) {
  lives_free(data);
}

typedef QImage::Format lives_painter_format_t;
#define LIVES_PAINTER_FORMAT_A1   QImage::Format_Mono
#define LIVES_PAINTER_FORMAT_A8   QImage::Format_Indexed8
#define LIVES_PAINTER_FORMAT_ARGB32 QImage::Format_ARGB32_Premultiplied


class lives_painter_surface_t : public QImage, public LiVESObject {
 public:
  int refcount;

 lives_painter_surface_t(int width, int height, QImage::Format fmt) : QImage(width, height, fmt) {
    refcount = 0;
  }

  lives_painter_surface_t (uint8_t *data, lives_painter_format_t fmt, int width, int height, int stride) 
    :  QImage (data,width,height,stride,fmt,imclean,(livespointer)data) {
    refcount = 0;
  }

};


boolean lives_painter_surface_destroy(lives_painter_surface_t *);


class lives_painter_t : public QPainter {
 public:
  QPainterPath *p;
  lives_painter_surface_t *target;
  QPen pen;

  
 lives_painter_t(QWidget *widget) : QPainter(widget) {
    init();
  };
  

 lives_painter_t(lives_painter_surface_t *surf) : QPainter() {
    init();
    target = surf;
  };


  ~lives_painter_t() {
    if (target!=NULL) lives_painter_surface_destroy(target);
    delete p;
  }


 private:

  void init(void) {
    p = new QPainterPath();
    pen = QPen();
    target = NULL;
  }


};


#define LIVES_PAINTER_CONTENT_COLOR 0

typedef QPainter::CompositionMode lives_painter_operator_t;

#define LIVES_PAINTER_OPERATOR_UNKNOWN QPainter::CompositionMode_SourceOver
#define LIVES_PAINTER_OPERATOR_DEFAULT QPainter::CompositionMode_SourceOver

#define LIVES_PAINTER_OPERATOR_DEST_OUT QPainter::CompositionMode_DestinationOut

#define LIVES_PAINTER_OPERATOR_DIFFERENCE QPainter::CompositionMode_Difference
#define LIVES_PAINTER_OPERATOR_OVERLAY QPainter::CompositionMode_Overlay

typedef Qt::FillRule lives_painter_fill_rule_t;

#define LIVES_PAINTER_FILL_RULE_WINDING  Qt::WindingFill
#define LIVES_PAINTER_FILL_RULE_EVEN_ODD Qt::OddEvenFill



#endif

#endif
