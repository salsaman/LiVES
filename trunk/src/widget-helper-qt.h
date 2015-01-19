#ifdef GUI_QT
// just for testing !!!!

#include <QtCore/QLinkedList>

#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtGui/QTextCursor>

#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QDialog>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLayout>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QShortcut>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>

#include <QtGui/QColor>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#define GTK_CHECK_VERSION(a,b,c) 0

#define g_free(a) lives_free(a)

#ifndef IS_MINGW
typedef bool                          boolean;
#endif

#ifndef FALSE
#define FALSE false
#endif

#ifndef TRUE
#define TRUE true
#endif

typedef void*                             gpointer;

//#define LIVES_TABLE_IS_GRID 1

typedef int LiVESAttachOptions;
#define LIVES_EXPAND 1
#define LIVES_SHRINK 2
#define LIVES_FILL 3

typedef QStringList LiVESSList;
typedef QStringList LiVESList;

typedef QStringList GSList; // TODO - remove
typedef QStringList GList; // TODO - remove

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

typedef struct {
// values from 0.0 -> 1.0
double red;
double green;
double blue;
double alpha;
} LiVESWidgetColor;

typedef class LiVESWidget LWid;

typedef void (*LiVESWidgetCallback) (LWid *widget, gpointer data);

#define LIVES_WIDGET_EVENT_EXPOSE_EVENT "update"

class ExposeBlocker
{
public:
 ExposeBlocker() : m_widg(NULL)
    {
    }

  void set_widget( QWidget *widg) {
    m_widg=widg;
    widg->setUpdatesEnabled(false );
  }

  ~ExposeBlocker()
    {
      if (m_widg!=NULL) m_widg->setUpdatesEnabled( m_old );
    }

private:
    QWidget *m_widg;
    bool m_old;
};

LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1orNULL, const LiVESWidgetColor *c2);


static QString make_col(LiVESWidgetColor *col) {
  QString qc = QString("rgba(%1,%2,%3,%4)")
    .arg(col->red*255.)
    .arg(col->green*255.)
    .arg(col->blue*255.)
    .arg(col->alpha*255.);
    return qc;
}


class LiVESWidget : public QWidget {
 public:
  int refcount;
  LiVESWidgetState state;
  
  LiVESWidget() {
    fg_norm=bg_norm=base_norm=text_norm=NULL;
    fg_act=bg_act=base_act=text_act=NULL;
    fg_insen=bg_insen=base_insen=text_insen=NULL;
    fg_hover=bg_hover=base_hover=text_hover=NULL;
    fg_sel=bg_sel=base_sel=text_sel=NULL;

    refcount=0;
    state = LIVES_WIDGET_STATE_NORMAL;

    widgetName = QString("%1").arg(ulong_random());
  }

  LiVESWidget(const LiVESWidget &widget) {
    refcount = widget.refcount;
    state = widget.state;
  }

  ~LiVESWidget() {

  }

  QWidget *get_widget() {
    QWidget *qw = qobject_cast<QWidget *>(this);
    qw->setProperty("LiVESWidget", QVariant::fromValue(this));
    return qw;
  }

  QWidget *parentWidget() {
    return get_widget();
  }

  boolean isWidgetType() {
    return FALSE;
  }


void update_stylesheet() {
QWidget *qw = qobject_cast<QWidget *>(this);

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






void set_fg_color(LiVESWidgetState state, const LiVESWidgetColor *col) {
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


void set_bg_color(LiVESWidgetState state, const LiVESWidgetColor *col) {
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


void set_base_color(LiVESWidgetState state, const LiVESWidgetColor *col) {
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


void set_text_color(LiVESWidgetState state, const LiVESWidgetColor *col) {
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


LiVESWidgetColor *get_fg_color(LiVESWidgetState state) {
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

LiVESWidgetColor *get_bg_color(LiVESWidgetState state) {
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

  void block_signal(ulong handler_id) {
    // TODO - add to blocked list

  }


  void block_signal(const char *signame) {
    // TODO :: need to block expose / paint signals

  }


  void unblock_signal(ulong handler_id) {
    // TODO - remove from blocked list

  }


  void disconnect_signal(ulong handler_id) {
    // TODO - remove from list, if no more with same name, disconnect


  }



  public Q_SLOTS:
    void cb_wrapper_clicked() {

      // retrieve func and data
      LiVESWidgetCallback funcptr;
      ulong func;
      void *data;
      ulong handler_id;

      //retrieve_signal_callbacks("clicked",&func,&data,&handler_id);
      // (may be multiple with different handler_id)

      // may be blocked by handler_id

      funcptr = (LiVESWidgetCallback)func;

      (*funcptr)(this, data);
    }

private:
QString widgetName;

QWidget *widget;

LiVESWidgetColor *fg_norm,*bg_norm,*base_norm,*text_norm;
LiVESWidgetColor *fg_act,*bg_act,*base_act,*text_act;
LiVESWidgetColor *fg_insen,*bg_insen,*base_insen,*text_insen;
LiVESWidgetColor *fg_hover,*bg_hover,*base_hover,*text_hover;
LiVESWidgetColor *fg_sel,*bg_sel,*base_sel,*text_sel;

};


//Q_DECLARE_METATYPE(LiVESWidget)

#define LIVES_GUI_CALLBACK(a) (ulong)(a)


ulong lives_signal_connect(LiVESWidget *widget, const char *signal_name, ulong funcptr, gpointer data) {
  ulong handler_id;

  handler_id=ulong_random();

  // generate random handler_id

  // need to store: (ulong)handler_id, signal_name, funcptr, data
  //widget->store_signal_callback(handler_id,signal_name,funcptr,data);

  if (!strcmp(signal_name,"clicked")) {
    widget->connect(widget, SIGNAL(clicked()), widget, SLOT(cb_wrapper_clicked()));
  }
  // etc. "scroll_event", "toggled", "changed", "button_press_event", "value_changed", "motion_notify_event", "button_release_event", "enter-notify-event", "leave-notify-event", "drag-data-received", "delete-event", "configure-event", "selection_changed", "expose_event", "state_flags_changed", "state_changed"

  return handler_id;
}


#define lives_signal_connect_after(a,b,c,d) lives_signal_connect(a,b,c,d)




typedef int                               gint;
typedef uchar                             guchar;
  
typedef void *(*LiVESPixbufDestroyNotify(uchar *, void *));


typedef int                               lives_painter_content_t;

typedef gpointer                          livespointer;
typedef char                              gchar;
typedef ulong                             gulong;

typedef void GError;

typedef struct {
  ulong func;
} LiVESWidgetClosure;


typedef struct {
  ulong func;
} LiVESWidgetSourceFunc;


#define LiVESError void

typedef QScreen LiVESXScreen;
typedef QScreen LiVESXDisplay; // TODO
typedef QScreen GdkDisplay; // TODO - specialised
typedef QScreen LiVESXDevice; // TODO

/*    QDesktopWidget *pDesktop = QApplication::desktop();
    QRect geometry = pDesktop->availableScreenGeometry(number);
    move(geometry.topLeft());*/

typedef QWindow LiVESXWindow;

typedef QEvent LiVESXEvent;

typedef Qt::MouseButton LiVESXEventButton;
typedef void LiVESXEventMotion;

typedef QCursor LiVESXCursor;

typedef QObject LiVESObject;

typedef QShortcut LiVESAccelGroup;
typedef QKeySequence LiVESXModifierType;
typedef QAbstractItemModel LiVESTreeModel;
typedef QAbstractListModel LiVESTreeStore;
typedef QModelIndex LiVESTreeIter;
typedef QPersistentModelIndex LiVESTreePath;
typedef QTreeWidget LiVESTreeView;

typedef int LiVESTreeViewColumn;
typedef QItemSelection LiVESTreeSelection;


typedef Qt::TransformationMode LiVESInterpType;
#define LIVES_INTERP_BEST   Qt::SmoothTransformation
#define LIVES_INTERP_NORMAL Qt::SmoothTransformation
#define LIVES_INTERP_FAST   Qt::FastTransformation

typedef int LiVESResponseType;
#define LIVES_RESPONSE_NONE QDialog::Rejected
#define LIVES_RESPONSE_OK QDialog::Accepted
#define LIVES_RESPONSE_CANCEL QDialog::Rejected
#define LIVES_RESPONSE_ACCEPT QDialog::Accepted
#define LIVES_RESPONSE_YES QDialog::Accepted
#define LIVES_RESPONSE_NO QDialog::Rejected


#define LIVES_RESPONSE_INVALID QDialog::Rejected
#define LIVES_RESPONSE_SHOW_DETAILS QDialog::Rejected

class LiVESLabel : public LiVESWidget, public QLabel {};
class LiVESBox : public LiVESWidget, public QBoxLayout {};
class LiVESHBox : public LiVESWidget, public QHBoxLayout {};
class LiVESVBox : public LiVESWidget, public QVBoxLayout {};
class LiVESEventBox : public LiVESWidget, public QHBoxLayout {};
class LiVESEntry : public LiVESWidget, public QLineEdit {};
class LiVESCombo : public LiVESWidget, public QComboBox {};
class LiVESSpinButton : public LiVESWidget, public QSpinBox {};
class LiVESMenu : public LiVESWidget, public QMenu {};
class LiVESButton : public LiVESWidget, public QPushButton {};
class LiVESColorButton : public LiVESWidget, public QPushButton {};
class LiVESToggleButton : public LiVESWidget, public QAbstractButton {};

/*class LiVESPixbuf : public LiVESWidget, public QImage {
 public:
 LiVESPixbuf() : QImage() {};
 LiVESPixbuf(QImage& qi) : QImage(qi) {};
 LiVESPixbuf(int width, int height, QImage::Format fmt) : QImage(width,height,fmt) {};
 LiVESPixbuf(uint8_t *data, int width, int height, int stride, QImage::Format fmt) : QImage(data,width,height,stride,fmt) {};
};*/

typedef class LiVESImage LiVESPixbuf;


class LiVESAdjustment {
 public:
  double value, lower, upper, si, pi, ps;

 LiVESAdjustment(double A, double B, double C, double D, double E, double F) :
  value(A), lower(B), upper(C), si(D), pi(E), ps(F) {};

};


typedef LiVESWidget                           LiVESWindow;
typedef LiVESWidget LiVESEditable;

//typedef QLayout LiVESEventBox;


typedef QDialog LiVESDialog;
typedef QCheckBox LiVESCheckButton;
typedef QButtonGroup LiVESButtonBox;
typedef QRadioButton LiVESRadioButton;
typedef QMenu LiVESMenuShell;
typedef QToolButton LiVESMenuToolButton;
typedef QAction LiVESRadioMenuItem;
typedef QAction LiVESImageMenuItem;
typedef QAction LiVESCheckMenuItem;
typedef QAction LiVESMenuItem;
typedef QLayout LiVESAlignment;

//typedef QGridLayout LiVESTable;
//typedef QGridLayout LiVESGrid;

typedef QTableWidget LiVESTable;
typedef QTableWidget LiVESGrid;
typedef LiVESWidget LiVESContainer;
typedef QProgressBar LiVESProgressBar;
typedef QWidget LiVESToolItem;
typedef QWidget LiVESRuler; // TODO - create
typedef QWidget LiVESRange; // TODO
typedef QWidget LiVESScaleButton; // TODO - create
typedef QScrollBar LiVESScrollbar;
typedef QScrollArea LiVESScrolledWindow;


// scrolledwindow policies
typedef int LiVESPolicyType;
#define LIVES_POLICY_AUTOMATIC Qt::ScrollBarAsNeeded
#define LIVES_POLICY_NEVER Qt::ScrollBarAlwaysOff
#define LIVES_POLICY_ALWAYS Qt::ScrollBarAlwaysOn


typedef QFrame LiVESFrame;
typedef QFileDialog LiVESFileChooser;
typedef QTabWidget LiVESNotebook;
typedef QToolBar LiVESToolbar;
typedef QToolButton LiVESToolButton;

typedef int LiVESToolbarStyle;
#define LIVES_TOOLBAR_ICONS Qt::ToolButtonIconOnly
#define LIVES_TOOLBAR_TEXT  Qt::ToolButtonTextOnly

typedef LiVESWidget LiVESBin;

typedef QSize LiVESRequisition;

typedef QTextDocument                     LiVESTextBuffer;
typedef QTextEdit                         LiVESTextView;

typedef int LiVESWrapMode;
#define LIVES_WRAP_NONE QTextEdit::NoWrap
#define LIVES_WRAP_WORD QTextEdit::WidgetWidth

typedef int LiVESJustification;

#define LIVES_JUSTIFY_LEFT   Qt::AlignLeft
#define LIVES_JUSTIFY_RIGHT  Qt::AlignRight
#define LIVES_JUSTIFY_CENTER Qt::AlignHCenter
#define LIVES_JUSTIFY_FILL   Qt::AlignJustify

typedef QTextCursor LiVESTextMark;
typedef QTextCursor LiVESTextIter;

typedef void LiVESCellRenderer;

typedef int LiVESAccelFlags;

typedef int LiVESTreeViewColumnSizing;
#define LIVES_TREE_VIEW_COLUMN_GROW_ONLY 0
#define LIVES_TREE_VIEW_COLUMN_AUTOSIZE 1
#define LIVES_TREE_VIEW_COLUMN_FIXED 2

typedef int LiVESSelectionMode;
#define LIVES_SELECTION_NONE QAbstractItemView::NoSelection
#define LIVES_SELECTION_SINGLE QAbstractItemView::SingleSelection
#define LIVES_SELECTION_MULTIPLE QAbstractItemView::MultiSelection

typedef void LiVESExpander;

typedef int LiVESOrientation;
#define LIVES_ORIENTATION_HORIZONTAL 1
#define LIVES_ORIENTATION_VERTICAL   2

typedef int LiVESButtonBoxStyle;
#define LIVES_BUTTONBOX_DEFAULT_STYLE 0
#define LIVES_BUTTONBOX_SPREAD 1
#define LIVES_BUTTONBOX_EDGE 0
#define LIVES_BUTTONBOX_CENTER 1

typedef int LiVESReliefStyle;

#define LIVES_RELIEF_NORMAL 2
#define LIVES_RELIEF_HALF 1
#define LIVES_RELIEF_NONE 0

#define ICON_SCALE(a) ((int)(1.0 * a))

typedef QSize LiVESIconSize;
#define LIVES_ICON_SIZE_INVALID QSize(0,0)
#define LIVES_ICON_SIZE_MENU QSize(ICON_SCALE(16),ICON_SCALE(16))
#define LIVES_ICON_SIZE_SMALL_TOOLBAR QSize(ICON_SCALE(16),ICON_SCALE(16))
#define LIVES_ICON_SIZE_LARGE_TOOLBAR QSize(ICON_SCALE(24),ICON_SCALE(24))
#define LIVES_ICON_SIZE_BUTTON QSize(ICON_SCALE(16),ICON_SCALE(16))
#define LIVES_ICON_SIZE_DND QSize(ICON_SCALE(32),ICON_SCALE(32))
#define LIVES_ICON_SIZE_DIALOG QSize(ICON_SCALE(48),ICON_SCALE(48))

typedef int LiVESWindowType;
#define LIVES_WINDOW_TOPLEVEL Qt::Window
#define LIVES_WINDOW_POPUP Qt::Popup

typedef int LiVESWindowPosition;
#define LIVES_WIN_POS_CENTER_ALWAYS 1


typedef int LiVESArrowType;
#define LIVES_ARROW_UP 1
#define LIVES_ARROW_DOWN 2
#define LIVES_ARROW_LEFT 3
#define LIVES_ARROW_RIGHT 4
#define LIVES_ARROW_NONE 0


typedef int LiVESShadowType;
#define LIVES_SHADOW_NONE 0
#define LIVES_SHADOW_IN 1
#define LIVES_SHADOW_OUT 2
#define LIVES_SHADOW_ETCHED_IN 3
#define LIVES_SHADOW_ETCHED_OUT 4



typedef int LiVESPositionType;
#define LIVES_POS_LEFT 1
#define LIVES_POS_RIGHT 2
#define LIVES_POS_TOP 3
#define LIVES_POS_BOTTOM 4

// LiVESWidget from QWidget
//#define LIVES_WIDGET(a) (a->isWidgetType()?((LiVESWidget *)( (a->property("LiVESWidget")).value<LiVESWidget *>() )):a)

#define LIVES_WIDGET(a) (a)

// QWidget from LiVESWidget / QWidget
//#define QT_WIDGET(a) (a->isWidgetType()?(QWidget *)a:a->parentWidget())

#define QT_WIDGET(a) (a->get_widget())


#define LIVES_EDITABLE(a) (a)

#define LIVES_CONTAINER(a) LIVES_WIDGET(a)
#define LIVES_GUI_OBJECT(a) LIVES_WIDGET(a)
#define LIVES_EXPANDER(a) LIVES_WIDGET(a)
#define LIVES_BIN(a) LIVES_WIDGET(a)

#define LIVES_IS_WIDGET(a) 1
#define LIVES_IS_CONTAINER(a) 1

#define LIVES_IS_BUTTON(a) ((LiVESHBox *)dynamic_cast<QAbstractButton *>(a) != NULL)
#define LIVES_IS_LABEL(a) ((LiVESHBox *)dynamic_cast<LiVESLabel *>(a) != NULL)
#define LIVES_IS_HBOX(a) ((LiVESHBox *)dynamic_cast<LiVESHBox *>(a) != NULL)
#define LIVES_IS_VBOX(a) ((LiVESHBox *)dynamic_cast<LiVESVBox *>(a) != NULL)
#define LIVES_IS_XWINDOW(a) ((LiVESHBox *)dynamic_cast<LiVESXWindow *>(a) != NULL)

#define LIVES_COMBO(a) dynamic_cast<LiVESCombo *>(QT_WIDGET(a))
#define LIVES_HBOX(a) dynamic_cast<LiVESHBox *>(QT_WIDGET(a))
#define LIVES_VBOX(a) dynamic_cast<LiVESVBox *>(QT_WIDGET(a))
#define LIVES_BOX(a) dynamic_cast<LiVESBox *>(QT_WIDGET(a))
#define LIVES_LABEL(a) dynamic_cast<LiVESLabel *>(QT_WIDGET(a))
#define LIVES_ENTRY(a) dynamic_cast<LiVESEntry *>(QT_WIDGET(a))
#define LIVES_WINDOW(a) dynamic_cast<LiVESWindow *>(QT_WIDGET(a))


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


#endif



#ifdef PAINTER_QPAINTER
# include <QtGui/QPainter>

extern void lives_free(gpointer ptr);

static void imclean(void *data) {
  lives_free(data);
}

typedef QImage::Format lives_painter_format_t;
#define LIVES_PAINTER_FORMAT_A1   QImage::Format_Mono
#define LIVES_PAINTER_FORMAT_A8   QImage::Format_Indexed8
#define LIVES_PAINTER_FORMAT_ARGB32 QImage::Format_ARGB32_Premultiplied


class lives_painter_surface_t : public QImage {
 public:
  int refcount;

 lives_painter_surface_t(int width, int height, QImage::Format fmt) : QImage(width, height, fmt) {
    refcount = 0;
  }

  lives_painter_surface_t (uint8_t *data, lives_painter_format_t fmt, int width, int height, int stride) 
    :  QImage (data,width,height,stride,fmt,imclean,(void *)data) {
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

class LiVESImage : public LiVESWidget {
 public:

 LiVESImage() {

  }

 LiVESImage(int width, int height, QImage::Format fmt) {
   qi = QImage(width, height, fmt);
 }

 LiVESImage(QImage *qix) {
   copy_from(qix);
 }

 LiVESImage(uint8_t *data, int width, int height, int bpl, QImage::Format fmt, QImageCleanupFunction cleanupFunction, void * cleanupInfo) {
   qi = QImage(data,width,height,bpl,fmt,cleanupFunction,cleanupInfo);
  }

 ~LiVESImage() {

 }

 void copy_from(QImage *qix) {
   qi = qix->copy();
 }

 void copy_from(LiVESImage *li) {
   qi = li->get_image().copy();
 }

 QImage get_image() {
   return qi;
 }

 private:
  QImage qi;

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

