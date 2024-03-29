// mainwindow.h
// LiVES (lives-exe)
// (c) G. Finch <salsaman+lives@gmail.com> 2003 - 2021
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_MAINWINDOW_H
#define HAS_LIVES_MAINWINDOW_H

#include <pthread.h>

#ifdef ALSA_MIDI
#include <alsa/asoundlib.h>
#endif

#ifdef HAVE_PULSE_AUDIO
#include "pulse.h"
#endif

#include "effects-data.h"
#include "audio.h"
#include "clip_load_save.h"
#include "nodemodel.h"

// hardware related prefs

#define DEF_FPS 25.

// soft limit
#define FPS_MAX 200.

#define MAX_FRAME_WIDTH 100000.
#define MAX_FRAME_HEIGHT 100000.

/// rate to change pb fps when faster/slower pressed (TODO: make pref)
#define DEF_FPSCHANGE_AMOUNT 30000

/// forward/back scratch value (TODO: make pref)
#define DEF_SCRATCHBACK_AMOUNT 80000
#define DEF_SCRATCHFWD_AMOUNT 80000

/// adjustment amount for effect parameter via keyboard (TODO: make pref)
#define DEF_BLENDCHANGE_AMOUNT 100

#define LOOP_LOCK_MIN_FRAMES (cfile->pb_fps + 1)

#define DEF_DL_BANDWIDTH 5000 ///< Kb / sec

/////// GUI related constants /////////////////////////////////////////////////////////

typedef struct {
  int width_base, height_base;
  double ratio;
} lives_aspect_ratio;

#define _GUI_CONST_801 620. // min msgbox width

#define _GUI_CONST_802 420. // timelime vspace
#define _GUI_CONST_803 60. // tree row height

#define _GUI_CONST_804 36. // min sepwin width
#define _GUI_CONST_805 180. // button wid

#define _GUI_CONST_806 240. // frameblank min wid
#define _GUI_CONST_807 180. // frameblank

#define _GUI_CONST_808 400. //

#define _GUI_CONST_809 210. //

#define MIN_MSGBOX_WIDTH ((int)(_GUI_CONST_801 * widget_opts.scaleW))

// parameters for resizing the image frames, and for capture
#define H_RESIZE_ADJUST (widget_opts.packing_width * 2)
#define V_RESIZE_ADJUST (widget_opts.packing_height * 2)

// space to reserve for the CE timeline
// IMPORTANT to fine tune this - TODO
#define CE_TIMELINE_VSPACE ((int)(_GUI_CONST_802 * widget_opts.scaleH * capable->font_size / 10.))

/// char width of combo entries (default)
#define COMBOWIDTHCHARS 12

/// char width of framecounter
#define FCWIDTHCHARS 22

/// char width of preview spinbutton
#define PREVSBWIDTHCHARS 8

// min sizes for the separate play window
#define MIN_SEPWIN_WIDTH ((int)(_GUI_CONST_801 * widget_opts.scaleW))
#define MIN_SEPWIN_HEIGHT ((int)(_GUI_CONST_804 * widget_opts.scaleH))

#define MENU_HIDE_LIM 24

/// sepwin/screen size safety margins in pixels
#define SCR_WIDTH_SAFETY ((int)(100. * widget_opts.scaleW))
#define SCR_HEIGHT_SAFETY ((int)(200. * widget_opts.scaleH))

/// height of preview widgets in sepwin
#define PREVIEW_BOX_HT ((int)(_GUI_CONST_803 * widget_opts.scaleH))

/// (unexpanded) height of rows in treeviews
#define TREE_ROW_HEIGHT ((int)(_GUI_CONST_804 * widget_opts.scaleH))

// a few GUI specific settings
#define GUI_SCREEN_WIDTH (mainw->mgeom[widget_opts.monitor].width)
#define GUI_SCREEN_HEIGHT (mainw->mgeom[widget_opts.monitor].height)
#define GUI_SCREEN_PHYS_WIDTH (mainw->mgeom[widget_opts.monitor].phys_width)
#define GUI_SCREEN_PHYS_HEIGHT (mainw->mgeom[widget_opts.monitor].phys_height)
#define GUI_SCREEN_X (mainw->mgeom[widget_opts.monitor].x)
#define GUI_SCREEN_Y (mainw->mgeom[widget_opts.monitor].y)

// scaling limits
#define SCREEN_SCALE_DEF_WIDTH 1600

#define SCREEN_169_MIN_WIDTH 1280
#define SCREEN_169_MIN_HEIGHT 720

/// default size for frames
// 16:9
#define DEF_FRAME_HSIZE_8K_UNSCALED 8192.
#define DEF_FRAME_VSIZE_8K_UNSCALED 4608.

#define DEF_FRAME_HSIZE_8KTV_UNSCALED 7680.
#define DEF_FRAME_VSIZE_8KTV_UNSCALED 4320.

#define DEF_FRAME_HSIZE_4K_UNSCALED 4096.
#define DEF_FRAME_VSIZE_4K_UNSCALED 2304.

#define DEF_FRAME_HSIZE_4KTV_UNSCALED 3840.
#define DEF_FRAME_VSIZE_4KTV_UNSCALED 2160.

// 1080p
#define DEF_FRAME_HSIZE_HDTV_UNSCALED 1920.
#define DEF_FRAME_VSIZE_HDTV_UNSCALED 1080.

// 4:3
#define DEF_FRAME_HSIZE_HD43_UNSCALED 1440.
#define DEF_FRAME_VSIZE_HD43_UNSCALED 1080.

// 720p
#define DEF_FRAME_HSIZE_169_UNSCALED 1280.
#define DEF_FRAME_VSIZE_169_UNSCALED 720.

#define DEF_FRAME_HSIZE_43_UNSCALED 1024.
#define DEF_FRAME_VSIZE_43_UNSCALED 768.

#define DEF_FRAME_HSIZE_43S_UNSCALED 640.
#define DEF_FRAME_VSIZE_43S_UNSCALED 480.

#define SCREEN_43S_LIMIT_WIDTH DEF_FRAME_HSIZE_43_UNSCALED
#define SCREEN_43S_LIMIT_HEIGHT DEF_FRAME_VSIZE_169_UNSCALED

#define DEF_FRAME_HSIZE_GUI (((int)(DEF_FRAME_HSIZE_43S_UNSCALED * widget_opts.scaleW) >> 2) << 1)
#define DEF_FRAME_VSIZE_GUI (((int)(DEF_FRAME_VSIZE_43S_UNSCALED * widget_opts.scaleH) >> 1) << 1)

// min screen height to show the message area
#define MIN_MSGBAR_HEIGHT (widget_opts.scaleH >= 1. ? ((int)32. * widget_opts.scaleH) : 46)
#define MIN_MSG_AREA_SCRNHEIGHT (DEF_FRAME_VSIZE_GUI + CE_TIMELINE_VSPACE - MIN_MSGBAR_HEIGHT)
#define MIN_MSGBOX_LLINES 2

#define DEF_FRAME_HSIZE_UNSCALED ((GUI_SCREEN_PHYS_WIDTH >= SCREEN_169_MIN_WIDTH && GUI_SCREEN_PHYS_HEIGHT >= SCREEN_169_MIN_HEIGHT) ? \
				  DEF_FRAME_HSIZE_169_UNSCALED :	\
				  (GUI_SCREEN_PHYS_WIDTH >= SCREEN_43S_LIMIT_WIDTH && GUI_SCREEN_PHYS_HEIGHT >= SCREEN_43S_LIMIT_HEIGHT) ? \
				  DEF_FRAME_HSIZE_43_UNSCALED : DEF_FRAME_HSIZE_43S_UNSCALED)

#define DEF_FRAME_VSIZE_UNSCALED ((GUI_SCREEN_PHYS_WIDTH >= SCREEN_169_MIN_WIDTH && GUI_SCREEN_PHYS_HEIGHT >= SCREEN_169_MIN_HEIGHT) ? \
				  DEF_FRAME_VSIZE_169_UNSCALED :	\
				  (GUI_SCREEN_PHYS_WIDTH >= SCREEN_43S_LIMIT_WIDTH && GUI_SCREEN_PHYS_HEIGHT >= SCREEN_43S_LIMIT_HEIGHT) ? \
				  DEF_FRAME_VSIZE_43_UNSCALED : DEF_FRAME_VSIZE_43S_UNSCALED)

#define DEF_GEN_WIDTH DEF_FRAME_HSIZE_UNSCALED
#define DEF_GEN_HEIGHT DEF_FRAME_VSIZE_UNSCALED

#define DEF_FRAME_HSIZE ((((int)((double)DEF_FRAME_HSIZE_UNSCALED * widget_opts.scaleW)) >> 2) << 2)
#define DEF_FRAME_VSIZE ((((int)((double)DEF_FRAME_VSIZE_UNSCALED * widget_opts.scaleH)) >> 1) << 1)

#define FRAMEBLANK_MIN_WIDTH ((int)(_GUI_CONST_806 * widget_opts.scaleW))
#define FRAMEBLANK_MAX_WIDTH ((int)(_GUI_CONST_801 * widget_opts.scaleW))

#define FRAMEBLANK_MIN_HEIGHT ((int)(_GUI_CONST_807 * widget_opts.scaleH))
#define FRAMEBLANK_MAX_HEIGHT ((int)(_GUI_CONST_808 * widget_opts.scaleH))

#define IMSEP_MAX_HEIGHT ((int)(64. * widget_opts.scaleH))
#define IMSEP_MAX_WIDTH (GUI_SCREEN_WIDTH - 20)

#define MAIN_SPIN_SPACER ((int)52. * widget_opts.scaleW) ///< pixel spacing for start/end spins for clip and multitrack editors

///< horizontal size in pixels of the encoder output window
#define ENC_DETAILS_WIN_H ((int)(DEF_FRAME_HSIZE_43S_UNSCALED * widget_opts.scaleW))
///< vertical size in pixels of the encoder output window
#define ENC_DETAILS_WIN_V (((int)(DEF_FRAME_VSIZE_43S_UNSCALED * widget_opts.scaleH)) >> 1)

#define MIN_MSG_WIDTH_CHARS ((int)(40. * widget_opts.scaleW)) ///< min width of text on warning/error labels
#define MAX_MSG_WIDTH_CHARS ((int)(200. * widget_opts.scaleW)) ///< max width of text on warning/error labels

/// size of the fx dialog windows scrollwindow
#define RFX_WINSIZE_H ((int)(GUI_SCREEN_WIDTH >= SCREEN_SCALE_DEF_WIDTH \
			     ? _GUI_CONST_809 * (1. + widget_opts.scaleW) : \
			     DEF_FRAME_HSIZE_43S_UNSCALED))
#define RFX_WINSIZE_V ((int)(DEF_FRAME_VSIZE_43S_UNSCALED * widget_opts.scaleH))

#define DLG_BUTTON_WIDTH ((int)(_GUI_CONST_805 * widget_opts.scaleW))
#define DLG_BUTTON_HEIGHT (widget_opts.css_min_height * 3)

#define DEF_BUTTON_WIDTH ((int)(_GUI_CONST_805 * widget_opts.scaleW))
#define DEF_BUTTON_HEIGHT ((((widget_opts.css_min_height >> 1) + 2) >> 1) << 3)

#define DEF_DIALOG_WIDTH RFX_WINSIZE_H
#define DEF_DIALOG_HEIGHT RFX_WINSIZE_V

#define LIVES_MAIN_WINDOW_WIDGET (mainw->LiVES)
#define LIVES_MAIN_WIDGET_WINDOW LIVES_MAIN_WINDOW_WIDGET ///< since I can never remember which way round it is !

////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ALL_USED -1

/// number of function keys
#define FN_KEYS 12

/// FX keys, 1 - 9 normally
#define FX_KEYS_PHYSICAL 9
#define FX_KEYS_PHYSICAL_EXTRA FX_KEYS_PHYSICAL + 2

/// must be >= FX_KEYS_PHYSICAL, and <=64 (number of bits in a 64bit int mask)
/// (max number of keys accessible through rte window or via OSC)
#define FX_KEYS_MAX_VIRTUAL 64 ///< the "metaphysical" keys
#define FX_MODES_MAX 32 ///< max modes per key (total max is 64 * 32 = 2048

/// the rest of the keys are accessible through the multitrack renderer (must, be > FX_KEYS_MAX_VIRTUAL)
#define FX_KEYS_MAX 65536

#define EFFECT_NONE 0
#define GU641 ((uint64_t)1)

#define MAX_FX_THREADS 1024 ///< may be increased if necessary

#define LIVES_DEF_DCLICK_TIME 400 ///< default double click time (milliseconds), can be overridden by system settings
#define LIVES_DEF_DCLICK_DIST 5 ///< default double click dist. (pixels), can be overridden by system settings

/// external control types
typedef enum {
  EXT_CNTL_NONE = -1, ///< not used
  EXT_CNTL_JS,
  EXT_CNTL_MIDI,
  MAX_EXT_CNTL
} lives_ext_cntl_t;

/// toy types
typedef enum {
  LIVES_TOY_NONE = 0,
  LIVES_TOY_MAD_FRAMES,
  LIVES_TOY_TV
} lives_toy_t;

typedef enum {
  LIVES_DIALOG_INFO,
  LIVES_DIALOG_ERROR,
  LIVES_DIALOG_WARN,
  LIVES_DIALOG_WARN_WITH_CANCEL,
  LIVES_DIALOG_YESNO,
  LIVES_DIALOG_QUESTION,
  // combinations: ABORT, RETRY, SKIP, CANCEL, OK, BROWSE
  LIVES_DIALOG_ABORT,
  LIVES_DIALOG_ABORT_OK,
  LIVES_DIALOG_ABORT_RESTART,
  LIVES_DIALOG_ABORT_RETRY,
  //
  LIVES_DIALOG_RETRY_CANCEL,
  //
  LIVES_DIALOG_CANCEL_BROWSE,
  //
  LIVES_DIALOG_ABORT_RETRY_CANCEL,
  LIVES_DIALOG_ABORT_SKIP_BROWSE,
  LIVES_DIALOG_ABORT_CANCEL_BROWSE,
  LIVES_DIALOG_ABORT_RETRY_IGNORE,
  //
  LIVES_DIALOG_RETRY_CANCEL_BROWSE,
  LIVES_DIALOG_RETRY_SKIP_BROWSE,
} lives_dialog_t;

#define DVD_AUDIO_CHAN_MIN 128
#define DVD_AUDIO_CHAN_DEFAULT 128
#define DVD_AUDIO_CHAN_MAX 159

enum {
  LIVES_DEVICE_CD = 0,
  LIVES_DEVICE_DVD, // 1
  LIVES_DEVICE_VCD, // 2
  LIVES_DEVICE_INTERNAL, // 2
  LIVES_DEVICE_TV_CARD, // 4
  LIVES_DEVICE_FW_CARD, // 5
};

#define LIVES_DEF_THEME "crayons"

#define USE_LIVES_THEMEING 	(1 << 0)
#define LIVES_THEME_DARK 	(1 << 1)
#define LIVES_THEME_COMPACT 	(1 << 2)

#define THEME_DETAIL_NAME "theme_name"
#define THEME_DETAIL_STYLE "theme_style"
#define THEME_DETAIL_SEPWIN_IMAGE "sepwin_image"
#define THEME_DETAIL_FRAMEBLANK_IMAGE "frameblank_image"
#define THEME_DETAIL_NORMAL_FORE "normal_fore"
#define THEME_DETAIL_NORMAL_BACK "normal_back"
#define THEME_DETAIL_ALT_FORE "alt_fore"
#define THEME_DETAIL_ALT_BACK "alt_back"
#define THEME_DETAIL_INFO_TEXT "info_text"
#define THEME_DETAIL_INFO_BASE "info_base"

#define THEME_DETAIL_AUDCOL "audcol"
#define THEME_DETAIL_VIDCOL "vidcol"
#define THEME_DETAIL_FXCOL "fxcol"
#define THEME_DETAIL_MT_TLREG "mt_tlreg"
#define THEME_DETAIL_MT_MARK "mt_mark"
#define THEME_DETAIL_MT_EVBOX "mt_evbox"
#define THEME_DETAIL_MT_TCFG "mt_timecode_fg"
#define THEME_DETAIL_MT_TCBG "mt_timecode_bg"
#define THEME_DETAIL_FRAME_SURROUND "frame_surround"
#define THEME_DETAIL_CE_SEL "ce_sel"
#define THEME_DETAIL_CE_UNSEL "ce_unsel"

/// set in set_palette_colours()
typedef struct {
  int style;
#define STYLE_PLAIN 0 ///< no theme (theme 'none')
#define STYLE_1 (1<<0) ///< turn on theming if set
#define STYLE_2 (1<<1) ///< colour the spinbuttons on the front page if set
#define STYLE_3 (1<<2) ///< style is lightish - allow themeing of widgets with dark text, otherwise use menu bg
#define STYLE_4 (1<<3) ///< separator col. in mt

#define STYLE_LIGHT STYLE_3

  LiVESWidgetColor white;
  LiVESWidgetColor black;
  LiVESWidgetColor pink;
  LiVESWidgetColor light_red;
  LiVESWidgetColor light_green;
  LiVESWidgetColor dark_red;
  LiVESWidgetColor dark_orange;

  LiVESWidgetColor grey20;
  LiVESWidgetColor grey25;
  LiVESWidgetColor grey45;
  LiVESWidgetColor grey60;
  LiVESWidgetColor fade_colour;

  LiVESWidgetColor banner_fade_text;

  // set via theme API

  LiVESWidgetColor normal_back;
  LiVESWidgetColor normal_fore;

  LiVESWidgetColor menu_and_bars;
  LiVESWidgetColor menu_and_bars_fore;
  LiVESWidgetColor info_text;
  LiVESWidgetColor info_base;

  LiVESWidgetColor mt_timecode_bg;
  LiVESWidgetColor mt_timecode_fg;

  LiVESWidgetColor nice1;
  LiVESWidgetColor nice2;
  LiVESWidgetColor nice3;

  lives_colRGBA64_t audcol;
  lives_colRGBA64_t vidcol;
  lives_colRGBA64_t fxcol;
  lives_colRGBA64_t mt_timeline_reg;

  lives_colRGBA64_t frame_surround;
  lives_colRGBA64_t mt_mark;
  lives_colRGBA64_t mt_evbox;

  lives_colRGBA64_t ce_sel;
  lives_colRGBA64_t ce_unsel;
} _palette;

/// screen details
typedef struct {
  int x, y;
  int width, height;
  int phys_width, phys_height;
  LiVESXDevice *mouse_device; ///< unused for gtk+ < 3.0.0
  LiVESXDisplay *disp;
  LiVESXScreen *screen;
#if GTK_CHECK_VERSION(3, 22, 0)
  LiVESXMonitor *monitor;
#endif
  double dpi;
  double scale;
  boolean primary;
} lives_mgeometry_t;

/// constant strings
enum {
  LIVES_STRING_CONSTANT_ANY = 0,
  LIVES_STRING_CONSTANT_NONE,
  LIVES_STRING_CONSTANT_DEFAULT,
  LIVES_STRING_CONSTANT_RECOMMENDED,
  LIVES_STRING_CONSTANT_DISABLED,
  LIVES_STRING_CONSTANT_CL,  ///< "the current layout"
  LIVES_STRING_CONSTANT_BUILTIN,
  LIVES_STRING_CONSTANT_CUSTOM,
  LIVES_STRING_CONSTANT_TEST,
  LIVES_STRING_CONSTANT_CLOSE_WINDOW,
  NUM_LIVES_STRING_CONSTANTS
};

// executables
// mandatory
#define EXEC_SMOGRIFY "smogrify"
#define EXEC_PERL "perl"

#define EXEC_SED "sed"
#define EXEC_GREP "grep"
#define EXEC_WC "wc"

#define EXEC_FILE "file"
#define EXEC_MKTEMP "mktemp"

// recommended
#define EXEC_SOX "sox"

// at least one of these
#define EXEC_MPLAYER "mplayer"
#define EXEC_MPLAYER2 "mplayer2"
#define EXEC_MPV "mpv"

#define EXEC_PULSEAUDIO "pulseaudio"
#define EXEC_CONVERT "convert"
#define EXEC_COMPOSITE "composite"
#define EXEC_IDENTIFY "identify"
#define EXEC_FFPROBE "ffprobe"
#define EXEC_FFMPEG "ffmpeg"
#define EXEC_EJECT "eject"

// at least one of these
#define EXEC_YOUTUBE_DL "youtube-dl"
#define EXEC_YOUTUBE_DLC "youtube-dlc"

// at least one of these, depending on perl version
#define EXEC_PIP "pip"
#define EXEC_PIP3 "pip3"
#define EXEC_GZIP "gzip"
#define EXEC_DU "du"
#define EXEC_DF "df"
#define EXEC_WGET "wget"
#define EXEC_CURL "curl"

// optional
#define EXEC_PYTHON "python"
#define EXEC_PYTHON3 "python3"
#define EXEC_AUTOLIVES_PL "autolives.pl" ///< shipped
#define EXEC_MIDISTART "lives-midistart" ///< shipped
#define EXEC_MIDISTOP "lives-midistop" ///< shipped
#define EXEC_JACKD "jackd" ///< recommended if (!have_pulseaudio)
#define EXEC_DVGRAB "dvgrab"
#define EXEC_CDDA2WAV "cdda2wav"
#define EXEC_ICEDAX "icedax"
#define EXEC_XSET "xset"
#define EXEC_XWININFO "xwininfo"
#define EXEC_GCONFTOOL_2 "gconftool-2"
#define EXEC_GSETTINGS "gsettings"

#define EXEC_XDG_SCREENSAVER "xdg-screensaver"
#define EXEC_XDG_OPEN "xdg-open"
#define EXEC_XDG_MIME "xdg-mime"
#define EXEC_XDG_DESKTOP_ICON "xdg-desktop-icon"
#define EXEC_XDG_DESKTOP_MENU "xdg-desktop-menu"

#define EXEC_WMCTRL "wmctrl"
#define EXEC_XDOTOOL "xdotool"
#define EXEC_PLAY "play"
#define EXEC_GIO "gio"
#define EXEC_NOTIFY_SEND "notify-send"
#define EXEC_SNAP "snap"

// runners
#define EXEC_GDB "gdb"
#define EXEC_VALGRIND "valgrind"

#define EXEC_RUNNERS {EXEC_GDB, EXEC_VALGRIND, NULL}

#define RUNNER_IS(what) (!lives_strcmp(capable->runner, #what))

/// other executables
#define EXEC_SUDO "sudo" /// for info mesages only; for security this is not called directly

// file types
#define LIVES_FILE_TYPE_UNKNOWN					(0ull)

#define LIVES_FILE_TYPE_FIFO					(1ull << 0)
#define LIVES_FILE_TYPE_CHAR_DEV				(1ull << 1)
#define LIVES_FILE_TYPE_DIRECTORY				(1ull << 2)
#define LIVES_FILE_TYPE_BLOCK_DEV 				((1ull << 1) | (1ull << 2))
#define LIVES_FILE_TYPE_FILE					(1ull << 3)
#define LIVES_FILE_TYPE_SYMLINK					(1ull << 4)
#define LIVES_FILE_TYPE_SOCKET					(1ull << 5)

#define LIVES_FILE_TYPE_PIPE					(1ull << 6)
#define LIVES_FILE_TYPE_STREAM_LOCAL				(1ull << 7)
#define LIVES_FILE_TYPE_STREAM_REMOTE				(1ull << 8)

#define LIVES_FILE_TYPE_MASK					(0xFFFF)

#define LIVES_FILE_TYPE_FLAG_SYMLINK				(1ull << 32)
#define LIVES_FILE_TYPE_FLAG_EXECUTABLE				(1ull << 33)
#define LIVES_FILE_TYPE_FLAG_UNWRITEABLE	       		(1ull << 34)
#define LIVES_FILE_TYPE_FLAG_INACCESSIBLE		       	(1ull << 35)

#define LIVES_FILE_TYPE_FLAG_EMPTY				(1ull << 59)
#define LIVES_FILE_TYPE_FLAG_MISSING				(1ull << 60)
#define LIVES_FILE_TYPE_FLAG_DAMAGED				(1ull << 61)
#define LIVES_FILE_TYPE_FLAG_INCOMPLETE		       		(1ull << 62)
#define LIVES_FILE_TYPE_FLAG_SPECIAL				(1ull << 63)

#define LIVES_FILE_IS_FILE(ftype)		((ftype & LIVES_FILE_TYPE_FILE) ? TRUE : FALSE)
#define LIVES_FILE_IS_DIRECTORY(ftype)		((ftype & LIVES_FILE_TYPE_DIRECTORY) ? TRUE : FALSE)
#define LIVES_FILE_IS_BLOCK_DEV(ftype)		((ftype & LIVES_FILE_TYPE_BLOCK_DEV) == LIVES_FILE_TYPE_BLOCK_DEV \
						 ? TRUE : FALSE)
#define LIVES_FILE_IS_CHAR_DEV(ftype)		((ftype & LIVES_FILE_TYPE_CHAR_DEV) ? TRUE : FALSE)

#define LIVES_FILE_IS_EMPTYY_FILE(ftype)	((ftype & LIVES_FILE_TYPE_FLAG_EMPTY) && LIVES_FILE_IS_FILE(ftype) \
						 ? TRUE : FALSE)
#define LIVES_FILE_IS_EMPTY_DIR(ftype)		((ftype & LIVES_FILE_TYPE_FLAG_EMPTY) && LIVES_FILE_IS_DIR(ftype) \
						 ? TRUE : FALSE)

#define LIVES_FILE_IS_MISSING(ftype)		((ftype & LIVES_FILE_TYPE_FLAG_MISSING) ? TRUE : FALSE)

// image types (string)
#define LIVES_IMAGE_TYPE_UNKNOWN ""
#define LIVES_IMAGE_TYPE_JPEG "jpeg"
#define LIVES_IMAGE_TYPE_PNG "png"

// audio types (string)
#define LIVES_AUDIO_TYPE_PCM "pcm"

// file extensions
#define LIVES_FILE_EXT_TMP "tmp"
#define LIVES_FILE_EXT_PNG "png"
#define LIVES_FILE_EXT_JPG "jpg"
#define LIVES_FILE_EXT_MGK "mgk"
#define LIVES_FILE_EXT_PRE "pre"
#define LIVES_FILE_EXT_NEW "new"
#define LIVES_FILE_EXT_MAP "map"
#define LIVES_FILE_EXT_SCRAP SCRAP_LITERAL
#define LIVES_FILE_EXT_TEXT "txt"
#define LIVES_FILE_EXT_BAK "bak"
#define LIVES_FILE_EXT_BACK "back"
#define LIVES_FILE_EXT_WEBM "webm"
#define LIVES_FILE_EXT_MP4 "mp4"

#define LIVES_FILE_EXT_BACKUP "lv1"
#define LIVES_FILE_EXT_PROJECT "lv2"

#define LIVES_FILE_EXT_TAR "tar"
#define LIVES_FILE_EXT_GZIP "gz"
#define LIVES_FILE_EXT_TAR_GZ LIVES_FILE_EXT_TAR "." LIVES_FILE_EXT_GZIP

#define LIVES_FILE_EXT_SRT "srt"
#define LIVES_FILE_EXT_SUB "sub"

#define LIVES_FILE_EXT_PCM "pcm"
#define LIVES_FILE_EXT_WAV "wav"

#define LIVES_FILE_EXT_LAYOUT "lay"

#define LIVES_FILE_EXT_RFX_SCRIPT "script"

//////////////////////////////

// URLs
#define LIVES_WEBSITE PACKAGE_URL
#define LIVES_MANUAL_URL LIVES_WEBSITE "/manual/"
#define LIVES_MANUAL_FILENAME "LiVES_manual.html"
#define LIVES_AUTHOR_EMAIL "salsaman+lives@gmail.com"
#define LIVES_DONATE_URL "https://sourceforge.net/p/lives/donate/"
#define LIVES_BUG_URL PACKAGE_BUGREPORT
#define LIVES_FEATURE_URL "https://sourceforge.net/p/lives/feature-requests/"
#define LIVES_TRANSLATE_URL "https://translations.launchpad.net/lives/trunk"

#define LIVES_LITERAL "lives"

// file names
#define LIVES_STATUS_FILE_NAME ".status"
#define LIVES_ENC_DEBUG_FILE_NAME ".debug_out"

#define TOTALSAVE_NAME "totalsave"
#define CLIP_BINFMT_CHECK "LiVESXXX"
#define CLIP_AUDIO_FILENAME "audio"
#define CLIP_TEMP_AUDIO_FILENAME "audiodump." LIVES_FILE_EXT_PCM

#define WORKDIR_LITERAL "workdir"
#define WORKDIR_LITERAL_LEN 7

#define HEADER_LITERAL "header"
#define AHEADER_LITERAL "aheader"

#define THEME_LITERAL "theme"
#define THEME_SEP_IMG_LITERAL "main"
#define THEME_FRAME_IMG_LITERAL "frame"
#define THEME_HEADER HEADER_LITERAL "." THEME_LITERAL
#define THEME_HEADER_2 THEME_HEADER "_gtk2"

#define LIVES_THEME_NONE "none"
#define LIVES_THEME_CAMERA "camera"

#define LIVES_CLIP_HEADER HEADER_LITERAL "." LIVES_LITERAL
#define LIVES_ACLIP_HEADER AHEADER_LITERAL "." LIVES_LITERAL

#define LIVES_CLIP_HEADER_OLD HEADER_LITERAL
#define LIVES_CLIP_HEADER_OLD2 LIVES_CLIP_HEADER_OLD "2"

#define SUBS_FILENAME "subs"

#define TEMPFILE_MARKER "can_remove"

#define COMMENT_FILENAME ".comment"

#define LIVES_FILENAME_IGNORE ".ignore"

// trash removal
#define LIVES_FILENAME_NOREMOVE ".noremove"
#define LIVES_FILENAME_INUSE ".inuse"
#define LIVES_FILENAME_NOCLEAN ".noclean"

#define TRASH_NAME "__TRASH-"
#define TRASH_REMOVE 	"remove"
#define TRASH_RECOVER 	"recover"
#define TRASH_LEAVE 	"leave"

#define RECOVERY_LITERAL "recovery"

#define UNREC_CLIPS_DIR "unrecoverable_clips"
#define UNREC_LAYOUTS_DIR "unrecoverable_layouts"

// directory / subdir names
#define LIVES_SHARE_DIR "share"
#define LIVES_DIR_LITERAL LIVES_LITERAL
#define LIVES_DEVICE_DIR "dev"
#define LIVES_TMP_DIR "tmp"
#define LIVES_RUN_DIR "run"
#define LIVES_SHM_DIR "shm"
#define LIVES_ETC_DIR "etc"
#define LIVES_USR_DIR "usr"
#define LIVES_LOCAL_DIR "local"
#define LIVES_LIB_DIR "lib" /// not to be confused with LIVES_LIBDIR !!!

#define EXTRA_CMD_FILE "cmdline"

#define DEF_PREFIX_DIR LIVES_USR_DIR

#define LIVES_DEVNULL LIVES_DIR_SEP LIVES_DEVICE_DIR LIVES_DIR_SEP "null"

// system-wide defaults in prefs->lib_dir
#define PLUGIN_EXEC_DIR LIVES_DIR_LITERAL LIVES_DIR_SEP PLUGINS_LITERAL

// system-wide defaults in prefs->prefix_dir
#define LIVES_DATA_DIR LIVES_SHARE_DIR LIVES_DIR_SEP LIVES_DIR_LITERAL

#define THEME_DIR LIVES_DATA_DIR LIVES_DIR_SEP THEMES_LITERAL
#define PLUGIN_SCRIPTS_DIR LIVES_DATA_DIR LIVES_DIR_SEP PLUGINS_LITERAL
#define PLUGIN_COMPOUND_DIR PLUGIN_SCRIPTS_DIR

#define DOC_LITERAL "doc"
#define DOC_DIR LIVES_SHARE_DIR LIVES_DIR_SEP DOC_LITERAL LIVES_DIR_SEP LIVES_DIR_LITERAL

#define APPLICATIONS_DIR LIVES_SHARE_DIR LIVES_DIR_SEP "applications"  // normally in LIVES_USR_DIR

#define ICONS_LITERAL "icons"

#define APP_ICON_DIR LIVES_DATA_DIR LIVES_DIR_SEP ICONS_LITERAL

#define APPS_LITERAL "apps"

#define ICONS_DIR LIVES_SHARE_DIR LIVES_DIR_SEP ICONS_LITERAL
#define DESKTOP_ICON_DIR ICONS_DIR LIVES_DIR_SEP "hicolor"
#define DESKTOP_ICON_DIR_16  DESKTOP_ICON_DIR LIVES_DIR_SEP   "16x16" LIVES_DIR_SEP APPS_LITERAL
#define DESKTOP_ICON_DIR_22  DESKTOP_ICON_DIR LIVES_DIR_SEP   "22x22" LIVES_DIR_SEP APPS_LITERAL
#define DESKTOP_ICON_DIR_32  DESKTOP_ICON_DIR LIVES_DIR_SEP   "32x32" LIVES_DIR_SEP APPS_LITERAL
#define DESKTOP_ICON_DIR_48  DESKTOP_ICON_DIR LIVES_DIR_SEP   "48x48" LIVES_DIR_SEP APPS_LITERAL
#define DESKTOP_ICON_DIR_64  DESKTOP_ICON_DIR LIVES_DIR_SEP   "64x64" LIVES_DIR_SEP APPS_LITERAL
#define DESKTOP_ICON_DIR_128 DESKTOP_ICON_DIR LIVES_DIR_SEP "128x128" LIVES_DIR_SEP APPS_LITERAL
#define DESKTOP_ICON_DIR_256 DESKTOP_ICON_DIR LIVES_DIR_SEP "256x256" LIVES_DIR_SEP APPS_LITERAL

#define ICON_DIR(size) DESKTOP_ICON_DIR_##size

// per-user defaults
#define LOCAL_HOME_DIR "." LIVES_LOCAL_DIR

#define LIVES_DEF_CONFIG_DIR ".config" LIVES_DIR_SEP LIVES_DIR_LITERAL ///< in $HOME : used once to set configfile, and then discarded
#define LIVES_DEF_CONFIG_FILE "settings" ///< in LIVES_DEF_CONFIG_DIR unless overridden

#define LIVES_METADATA_FILE "metadata"

#define LIVES_DEF_CONFIG_FILE_OLD ".lives" ///< pre 3.2.0
#define LIVES_DEF_CONFIG_DATADIR_OLD ".lives-dir" ///< pre 3.2.0

#define STOCK_ICON_DIR "stock-icons"

#define LIVES_DEVICEMAP_DIR "devicemaps"
#define LIVES_DEF_WORK_SUBDIR "livesprojects"
#define LIVES_RESOURCES_DIR "resources"

// filters
#define LIVES_SUBS_FILTER  {"*.srt", "*.sub", NULL}
#define LIVES_AUDIO_LOAD_FILTER  {"*.it", "*.mp3", "*.wav", "*.ogg", "*.mod", "*.xm", "*.wma", "*.flac", NULL}
#define LIVES_TV_CARD_TYPES  {"v4l2", "v4l", "bsdbt848", "dummy", "*autodetect", "yv12", "*", "rgb32", "rgb24", "rgb16", \
      "rgb15", "uyvy", "yuy2", "i2420", NULL}

/* actions */

#define LIVES_ACTION_UNMATCHED -1
#define LIVES_ACTION_START_PLAYBACK 0
#define LIVES_ACTION_STOP_PLAYBACK 1
#define LIVES_ACTION_CLIP_SELECT 2
#define LIVES_ACTION_PLAY_FORWARDS 3
#define LIVES_ACTION_PLAY_BACKWARDS 4
#define LIVES_ACTION_REVERSE_PLAYBACK 5
#define LIVES_ACTION_PLAY_FASTER 6
#define LIVES_ACTION_PLAY_SLOWER 7
#define LIVES_ACTION_TOGGLE_FREEZE 8
#define LIVES_ACTION_SET_FRAMERATE 9
#define LIVES_ACTION_START_RECORDING 10
#define LIVES_ACTION_STOP_RECORDING 11
#define LIVES_ACTION_TOGGLE_RECORDING 12
#define LIVES_ACTION_SWAP_FOREGROUND_BACKGROUND 13
#define LIVES_ACTION_RESET_EFFECT_KEYS 14
#define LIVES_ACTION_ENABLE_EFFECT_KEY 15
#define LIVES_ACTION_DISABLE_EFFECT_KEY 16
#define LIVES_ACTION_TOGGLE_EFFECT_KEY 17
#define LIVES_ACTION_SET_PARAMETER_VALUE 18
#define LIVES_ACTION_NEXT_CLIP_SELECT 19
#define LIVES_ACTION_PREV_CLIP_SELECT 20
#define LIVES_ACTION_SET_FPS_RATIO 21
#define LIVES_ACTION_RETRIGGER_CLIP 22
#define LIVES_ACTION_NEXT_MODE_CYCLE 23
#define LIVES_ACTION_PREV_MODE_CYCLE 24
#define LIVES_ACTION_SET_VPP_PARAMETER_VALUE 25
#define LIVES_ACTION_OSC_NOTIFY 26

typedef struct {
  int idx;
  char *key;
  char *cmdlist;
  char *futures;
} lives_permmgr_t;

/// helper proc_threads
#define N_HLP_PROCTHREADS	256
#define PT_TRANSREND		2
#define PT_DLG_SPINNER		3
#define PT_LAZY_RFX		16
#define PT_LAZY_DSUSED		17
#define PT_CUSTOM_COLOURS	18
#define PT_LAZY_STARTUP		19
#define PT_PERF_MANAGER		32
#define PT_PLAYER		64
#define PT_PLAN_RUNNER		65
#define PT_SCRAPFILE_SAVER	128

#define lazy_starter helper_procthreads[PT_LAZY_STARTUP]
#define transrend_proc helper_procthreads[PT_TRANSREND]
#define dlg_spin_thread helper_procthreads[PT_DLG_SPINNER]
#define player_proc helper_procthreads[PT_PLAYER]
#define scrap_file_proc helper_procthreads[PT_SCRAPFILE_SAVER]
#define plan_runner_proc helper_procthreads[PT_PLAN_RUNNER]

typedef struct {
  char *name;
  lives_rect_t *rects; // for future use
  int z_index; // for future use
} lives_screen_area_t;

/// where do we add the builtin tools in the tools menu
#define RFX_TOOL_MENU_POSN 2

/// mainw->msg bytesize
#define MAINW_MSG_SIZE 8192

typedef struct {
  // processing / busy dialog (TODO - move into dialogs.h / or prog_dialogs.h)
  LiVESWidget *processing;
  LiVESWidget *progressbar;
  LiVESWidget *label;
  LiVESWidget *label2;
  LiVESWidget *label3;
  LiVESWidget *stop_button;
  LiVESWidget *pause_button;
  LiVESWidget *preview_button;
  LiVESWidget *cancel_button;
  LiVESWidget *scrolledwindow;
  LiVESWidget *rte_off_cb;
  LiVESWidget *audint_cb;
  LiVESWidget *notify_cb;
  frames_t progress_start, progress_end;
  frames_t frames_done;
  char *text;
  double frac_done;
  boolean is_ready;
  int owner;
} xprocess;


typedef struct {
  char msg[MAINW_MSG_SIZE];

  // clip files
  int current_file;
  int first_free_file;
  lives_clip_t *files[MAX_FILES + 1]; ///< +1 for the clipboard

  // directories -> move to prefs
  char vid_load_dir[PATH_MAX];
  char vid_save_dir[PATH_MAX];
  char vid_dl_dir[PATH_MAX];
  char audio_dir[PATH_MAX];
  char image_dir[PATH_MAX];
  char proj_load_dir[PATH_MAX];
  char proj_save_dir[PATH_MAX];

  // files
  char vpp_defs_file[PATH_MAX];

  char recent_file[PATH_MAX]; // for liblives only

  char metafile[PATH_MAX];  ///< metadata

  int untitled_number;
  int cap_number; // capture device number

  //
  int clips_available;

  /// list of (int to ptr) clips in menu order
  LiVESList *cliplist; /// IMPORTANT !!
  LiVESList *clip_grps; // list of lives_clipgrp_t *

  /// sets
  char set_name[256];   // actually 128 is the (soft) limit now, filesystem encoding
  boolean was_set;
  boolean leave_files;  ///< TRUE to leave clip files on disk even when closing (default FALSE)
  int num_sets; /// number of sets in workdir (minus the current set), -1 if not checked
  LiVESList *set_list; /// list of set names in current workdir, mau be NULL

  // playback state
  boolean playing_sel;
  boolean faded;
  boolean double_size;
  boolean sep_win;
  boolean fs;
  boolean loop;
  volatile boolean loop_cont;
  volatile boolean ping_pong;
  boolean oloop;
  boolean oloop_cont;
  boolean oping_pong;
  boolean loop_locked;
  boolean mute;
  frames_t audio_start, audio_end;

  boolean ext_playback; ///< using external video playback plugin
  volatile boolean ext_audio; ///< using external video playback plugin to stream audio

  int ptr_x, ptr_y; // mouse posn. when switching to / from fullscreen

  // playback fps
  frames_t fps_measure; ///< show fps stats after playback
  frames_t fps_mini_measure; ///< show fps stats during playback
  ticks_t fps_mini_ticks;
  double inst_fps;

  // flags
  boolean save_with_sound;
  boolean ccpd_with_sound;
  boolean selwidth_locked;
  boolean opening_loc;  ///< opening location (streaming)
  boolean dvgrab_preview;
  boolean switch_during_pb;

  // recording flags
  boolean clip_switched; ///< for recording - did we switch clips ?
  volatile boolean record;
  volatile boolean record_paused; ///< pause during recording
  boolean record_starting; ///< start recording at next frame
  volatile boolean record_all_audio;

  char *fsp_tmpdir;
  volatile boolean fs_preview_active, fs_preview_running, fs_preview_cleanup;
  volatile lives_cancel_t cancelled;
  lives_cancel_type_t cancel_type;

  boolean elist_eom;
  weed_event_list_t *event_list; ///< current event_list, for recording
  weed_event_list_t *stored_event_list; ///< stored mt -> clip editor
  boolean stored_event_list_changed;
  boolean stored_event_list_auto_changed;
  boolean stored_layout_save_all_vals;
  char stored_layout_name[PATH_MAX];

  LiVESList *stored_layout_undos;
  size_t sl_undo_buffer_used;
  unsigned char *sl_undo_mem;
  int sl_undo_offset;

  LiVESList *new_lmap_errors;

  LiVESList *bad_lmaps;

  short endian;

  boolean go_away;

  /// states
  int status;

  boolean is_ready;

  boolean can_play;

  boolean error; //  --> status
  volatile int fatal; ///< got fatal signal --> status

  volatile int critical; ///< got critical signal --> status
  uint64_t critical_thread;
  int critical_errno;
  const char *critical_errmsg;

  volatile boolean no_idlefuncs;
  uint32_t fg_service_handle;

  boolean is_processing;
  boolean is_rendering;
  boolean resizing; // ignore frame size check during previews

  boolean preview;
  boolean preview_rendering;

  volatile uint64_t n_service_calls;

  boolean foreign;  ///< for external window capture
  boolean record_foreign;
  boolean t_hidden;

  // recording from an external window
  uint32_t foreign_key;

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  Window foreign_id;
#else
  GdkNativeWindow foreign_id;
  GdkColormap *foreign_cmap;
  GdkPixmap *foreign_map;
#endif
#else
  Window foreign_id;
#endif

  LiVESXWindow *foreign_window;
  int foreign_width, foreign_height;
  int foreign_bpp;
  char *foreign_visual;

  /// some VJ effects / toys
  boolean nervous;
  int swapped_clip; ///< maintains the current cliplist position even if we swap fg and bg clips
  lives_toy_t toy_type;
  boolean toy_go_wild; ///< some silliness

  /// rendered fx
  lives_rfx_t **rendered_fx;
  int num_rendered_effects_builtin;
  int num_rendered_effects_custom;
  int num_rendered_effects_test;

  // for the merge dialog
  int last_transition_idx;
  int last_transition_loops;
  boolean last_transition_loop_to_fit;
  boolean last_transition_align_start;
  boolean last_transition_ins_frames;

  volatile uint64_t rte; ///< current max for VJ mode == 64 effects on fg clip
  volatile uint64_t rte_real; ///< keeps track of rte keys during a play cycle

  uint32_t last_grabbable_effect; // (spelt wrong but I like it this way)
  int rte_keys; ///< which effect is bound to keyboard (m) modechange and ctrl-alt-up-arrow / ctrl-alt-down-arrow param changes
  int num_tr_applied; ///< number of transitions active
  double blend_factor; ///< keyboard control parameter

  int scrap_file; ///< we throw odd sized frames here when recording in real time; used if a source is a generator or stream
  int ascrap_file; ///< scrap file for recording audio scraps
  ssize_t scrap_file_size;

  lives_pid_t alives_pid; // 0, or pid for autolives

  // copy/paste
  boolean insert_after;
  boolean with_sound;

  // selection
  int sel_start;
  short sel_move;

  /// which bar should we move ?
#define SEL_MOVE_START 1
#define SEL_MOVE_END 2
#define SEL_MOVE_AUTO 3
#define SEL_MOVE_SINGLE 4

  // prefs (Save on exit)
  int prefs_changed;
  boolean prefs_need_restart;

  /// default sizes for when no file is loaded
  int def_width, def_height;
  double ptrtime;
  /////////////////////////////////////////////////

  // end of static-ish info
  int old_scr_width, old_scr_height;

  /// extra parameters for opening special files
  char *file_open_params;
  boolean open_deint;

  int aud_file_to_kill; ///< # of audio file to kill on crash

  boolean reverse_pb; ///< used in osc.c

  /// TODO - make this a mutex and more finely grained : things we need to block are (clip switches, clip closure, effects on/off, etc)
  /// this field may even be totally / partially redundant now since mainw->noswitch has been re-implemented
  /// combined with filter_mutex_lock()
  boolean osc_block;

  int osc_auto; ///< bypass user choices automatically

  /// encode width, height and fps set externally
  int osc_enc_width, osc_enc_height;
  float osc_enc_fps;

  /// PLAYBACK

  int pwidth; ///< playback width in RGB pixels
  int pheight; ///< playback height

  volatile lives_whentostop_t whentostop;

  frames_t play_start, play_end;

  // for jack transport
  boolean jack_can_stop, jack_can_start, lives_can_stop, jack_master;

  // a/v seek synchronisation
  pthread_cond_t avseek_cond;
  pthread_mutex_t avseek_mutex;
  volatile boolean video_seek_ready;
  volatile boolean audio_seek_ready;
  double avsync_time;
  double sync_err;

  lives_obj_instance_t *aplayer;

  /// which number file we are playing (or -1) [generally mainw->current_file]
  int playing_file;

  // for the internal player
  LiVESWidget *play_image;
  LiVESWidget *play_window;
  weed_layer_t *frame_layer;

  weed_layer_t *ext_layer; // can feed a layer to play
  weed_layer_t *ext_player_layer;

  lives_painter_surface_t *play_surface;
  pthread_mutex_t play_surface_mutex;
  lives_painter_surface_t *pwin_surface;
  pthread_mutex_t pwin_surface_mutex;

  /// predictive caching apparatus
  weed_plant_t *cached_frame;
  weed_plant_t *frame_layer_preload;
  frames64_t pred_frame;
  int pred_clip;

  /// actual / last frame being displayed
  frames_t actual_frame;

  frames_t record_frame; ///< frame number to insert in recording

  /// recording values - to be inserted at the following video frame
  volatile int rec_aclip;
  volatile double rec_avel;
  volatile double rec_aseek;

  int pre_src_file; ///< video file we were playing before any ext input started
  int pre_src_audio_file; ///< audio file we were playing before any ext input started
  int pre_play_file; ///< the current file before playback started

  // node model for current fx chain
  lives_nodemodel_t *nodemodel;
  exec_plan_t *exec_plan;
  exec_plan_t *plan_cycle;
  boolean refresh_model;

  lives_layer_t **layers;

  /// background clip details
  int blend_file, last_blend_file, new_blend_file;
  weed_plant_t *blend_layer;

  /// stored clips (bookmarks) [0] = clip, [1] = frame
  int clipstore[FN_KEYS - 1][2];

  /// fixed fps playback; usually fixed_fpsd==0.
  int fixed_fps_numer, fixed_fps_denom;
  double fixed_fpsd; ///< <=0. means free playback

  /// timing variables
  // ticks are measured in 1. / TICKS_PER_SECOND_DBL of a second (by default a tick is 10 nano seconds)

  // for the internal player
  const int64_t initial_time; ///< set ASAP when app is (re)started
  volatile int64_t wall_time; /// wall clock time, updated whenever lives_get_*_ticks is called

  volatile ticks_t time_jump;

  volatile ticks_t startticks; ///< effective ticks when current frame was (should have been) displayed
  ticks_t last_startticks; ///< effective ticks when last frame was (should have been) displayed
  ticks_t timeout_ticks; ///< incremented if effect/rendering is paused/previewed

  ticks_t origticks; ///< playback start time

  ticks_t offsetticks; ///< offset for multitrack playback start
  volatile ticks_t clock_ticks; ///< unadjusted system time since pb start, measured concurrently with currticks

  volatile ticks_t currticks; ///< current playback ticks (relative)
  ticks_t firstticks; ///< ticks when audio started playing (for non-realtime audio plugins)
  //ticks_t syncticks; ///< adjustment to compensate for missed clock updates when switching time sources
  ticks_t stream_ticks;  ///< ticks since first frame sent to playback plugin

  ticks_t last_display_ticks; /// currticks when last display was shown (used for fixed fps)

  int play_sequence; ///< incremented for each playback

  double audio_stretch; ///< for fixed fps modes, the value is used to speed up / slow down audio

  int size_warn; ///< warn the user that incorrectly sized frames were found (threshold count)

  volatile boolean noswitch; /// set during playback, ensures that user clip switches change only mainw->new_clip
  volatile int new_clip; ///< clip we should switch to during playback; switch will happen at the designated SWITCH POINT
  volatile int close_this_clip; // if not -1, when we switch clip at the player switch point, we also close the old clip

  boolean preview_req; // preview requested

  volatile short scratch;
#define SCRATCH_NONE		0
#define SCRATCH_BACK		-1
#define SCRATCH_FWD		1
#define SCRATCH_REV	 	2 ///< set on direction change (video)
#define SCRATCH_JUMP		3 ///< jump and resync audio
#define SCRATCH_JUMP_NORESYNC	4 ///< jump with no audio resync
#define SCRATCH_REALIGN		5 ///< realign played (actual) frame with theoretical frame

#define SCRATCH_BACK_EXTRA 254
#define SCRATCH_FWD_EXTRA 256

  /////

  /// video playback plugin was updated; write settings to a file on exit
  boolean write_vpp_file;

  /// internal fx
  boolean internal_messaging; ///< set to indicate that frame processing progress values will be generated internally
  lives_render_error_t (*progress_fn)(boolean reset);

  volatile boolean threaded_dialog; ///< not really threaded ! but threaded_dialog_spin() can be called to animate it

  // fx controls (mostly unused - should be removed and replaced with generic toggle callbacks)
  double fx1_val, fx2_val, fx3_val, fx4_val, fx5_val, fx6_val;
  int fx1_start, fx2_start, fx3_start, fx4_start;
  int fx1_step, fx2_step, fx3_step, fx4_step;
  int fx1_end, fx2_end, fx3_end, fx4_end;
  boolean fx1_bool, fx2_bool, fx3_bool, fx4_bool, fx5_bool, fx6_bool;

  boolean effects_paused;
  boolean did_rfx_preview;

  uint32_t kb_timer;

  /// (GUI) function pointers
  ulong config_func;
  ulong pb_fps_func;
  ulong spin_start_func;
  ulong spin_end_func;
  ulong record_perf_func;
  ulong toy_func_none;
  ulong toy_func_random_frames;
  ulong toy_func_lives_tv;
  ulong hnd_id;
  ulong loop_cont_func;
  ulong mute_audio_func;
  ulong fullscreen_cb_func;
  ulong sepwin_cb_func;
  ulong fsp_func; ///< fileselector preview expose (for image thumbnails)
  ulong vj_mode_func;
  ulong lb_func;
  ulong desk_rec_func;
  ulong msg_adj_func;
  ulong mt_msg_adj_func;

  lives_painter_surface_t *fsp_surface;

  // selection pointers
  ulong mouse_fn1;
  boolean mouse_blocked;

  lives_mt *multitrack; ///< holds a pointer to the entire multitrack environment; NULL in Clip Edit mode
  boolean mt_needs_idlefunc; ///< set if we need to re-add the idlefunc for autobackup

  xprocess *proc_ptr; // progress dialog

  /// WIDGETS
  LiVESWidget *LiVES; ///< toplevel window
  LiVESWidget *hdrbar;
  LiVESWidget *frame1;
  LiVESWidget *frame2;
  LiVESWidget *freventbox0;
  LiVESWidget *freventbox1;
  LiVESWidget *playframe;
  LiVESWidget *plug;
  LiVESWidget *pl_eventbox;
  LiVESWidget *pf_grid;
  LiVESPixbuf *imframe;
  LiVESPixbuf *camframe;
  LiVESPixbuf *imsep;

  /// menus
  LiVESSList *clips_group;
  LiVESWidget *open;
  LiVESWidget *open_sel;
  LiVESWidget *open_vcd_menu;
  LiVESWidget *open_vcd_submenu;
  LiVESWidget *open_vcd;
  LiVESWidget *open_dvd;
  LiVESWidget *open_loc;
  LiVESWidget *open_utube;
  LiVESWidget *open_loc_menu;
  LiVESWidget *open_loc_submenu;
  LiVESWidget *open_yuv4m;
  LiVESWidget *open_lives2lives;
  LiVESWidget *send_lives2lives;
  LiVESWidget *open_device_menu;
  LiVESWidget *open_device_submenu;
  LiVESWidget *open_firewire;
  LiVESWidget *open_hfirewire;
  LiVESWidget *unicap;
  LiVESWidget *firewire;
  LiVESWidget *tvdev;
  LiVESWidget *recent_menu;
  LiVESWidget *recent_submenu;
  LiVESWidget *recent[N_RECENT_FILES];
  LiVESWidget *save_as;
#ifdef LIBAV_TRANSCODE
  LiVESWidget *transcode;
#endif
  LiVESWidget *backup;
  LiVESWidget *restore;
  LiVESWidget *save_selection;
  LiVESWidget *close;
  LiVESWidget *optimize;
  LiVESWidget *import_proj;
  LiVESWidget *export_proj;
  LiVESWidget *import_theme;
  LiVESWidget *export_theme;
  LiVESWidget *sw_sound;
  LiVESWidget *clear_ds;
  LiVESWidget *ccpd_sound;
  LiVESWidget *quit;
  LiVESWidget *undo;
  LiVESWidget *redo;
  LiVESWidget *copy;
  LiVESWidget *cut;
  LiVESWidget *insert;
  LiVESWidget *paste_as_new;
  LiVESWidget *merge;
  LiVESWidget *xdelete;
  LiVESWidget *trim_video;

#define LIVES_SELECT_NONE 0
#define LIVES_SELECT_ALL 1
#define LIVES_SELECT_START_ONLY 2
#define LIVES_SELECT_END_ONLY 3
#define LIVES_SELECT_INVERT 4
#define LIVES_SELECT_FROMSTART 5
#define LIVES_SELECT_TOEND 6
#define LIVES_SELECT_STTOPTR 7
#define LIVES_SELECT_ENTOPTR 8

  LiVESWidget *select_submenu;
  LiVESWidget *select_sttoptr;
  LiVESWidget *select_entoptr;
  LiVESWidget *select_vis;
  LiVESWidget *select_vismatch;
  LiVESWidget *select_skipbl;
  LiVESWidget *select_all;
  LiVESWidget *select_new;
  LiVESWidget *select_to_end;
  LiVESWidget *select_to_aend;
  LiVESWidget *select_from_start;
  LiVESWidget *select_start_only;
  LiVESWidget *select_end_only;
  LiVESWidget *select_last;
  LiVESWidget *select_invert;
  LiVESWidget *lock_selwidth;
  LiVESWidget *record_perf;
  LiVESWidget *playall;
  LiVESWidget *playsel;
  LiVESWidget *playclip;
  LiVESWidget *rev_clipboard;
  LiVESWidget *stop;
  LiVESWidget *rewind;
  LiVESWidget *full_screen;
  LiVESWidget *loop_video;
  LiVESWidget *loop_continue;
  LiVESWidget *loop_ping_pong;
  LiVESWidget *sepwin;
  LiVESWidget *mute_audio;
  LiVESWidget *sticky;
  LiVESWidget *showfct;
  LiVESWidget *showsubs;
  LiVESWidget *letter;
  LiVESWidget *aload_subs;
  LiVESWidget *load_subs;
  LiVESWidget *erase_subs;
  LiVESWidget *fade;
  LiVESWidget *dsize;
  LiVESWidget *midi_learn;
  LiVESWidget *midi_save;
  LiVESWidget *change_speed;
  LiVESWidget *capture;
  LiVESWidget *desk_rec;
  LiVESWidget *load_audio;
  LiVESWidget *load_cdtrack;
  LiVESWidget *eject_cd;
  LiVESWidget *recaudio_submenu;
  LiVESWidget *recaudio_clip;
  LiVESWidget *recaudio_sel;
  LiVESWidget *export_submenu;
  LiVESWidget *export_allaudio;
  LiVESWidget *export_selaudio;
  LiVESWidget *append_audio;
  LiVESWidget *normalize_audio;
  LiVESWidget *trim_submenu;
  LiVESWidget *trim_audio;
  LiVESWidget *trim_to_pstart;
  LiVESWidget *delaudio_submenu;
  LiVESWidget *delsel_audio;
  LiVESWidget *delspec_audio;
  LiVESWidget *delall_audio;
  LiVESWidget *ins_silence;
  LiVESWidget *voladj;
  LiVESWidget *fade_aud_in;
  LiVESWidget *fade_aud_out;
  LiVESWidget *resample_audio;
  LiVESWidget *adj_audio_sync;
  LiVESWidget *resample_video;
  LiVESWidget *preferences;
  LiVESWidget *rename;
  LiVESWidget *toy_none;
  LiVESWidget *toy_random_frames;
  LiVESWidget *toy_tv;
  LiVESWidget *autolives;
  LiVESWidget *show_file_info;
  LiVESWidget *show_file_comments;
  LiVESWidget *show_clipboard_info;
  LiVESWidget *show_messages;
  LiVESWidget *show_aplayer_attr;
  LiVESWidget *show_aplayer_read_attr;
  LiVESWidget *show_jackmsgs;
  LiVESWidget *show_layout_errors;
  LiVESWidget *show_quota;
  LiVESWidget *sel_label;
  LiVESAccelGroup *accel_group;
  LiVESWidget *sep_image;
  LiVESWidget *hruler;
  LiVESWidget *vj_save_set;
  LiVESWidget *vj_load_set;
  LiVESWidget *vj_show_keys;
  LiVESWidget *rte_defs_menu;
  LiVESWidget *rte_defs;
  LiVESWidget *save_rte_defs;
  LiVESWidget *vj_reset;
  LiVESWidget *vj_realize;
  LiVESWidget *vj_mode;
  LiVESWidget *show_devopts;
  LiVESWidget *dev_dabg;
  LiVESWidget *dev_timing;
  LiVESWidget *dev_caching;
  LiVESWidget *mt_menu;
  LiVESWidget *troubleshoot;
  LiVESWidget *expl_missing;
  LiVESWidget *export_custom_rfx;
  LiVESWidget *delete_custom_rfx;
  LiVESWidget *edit_test_rfx;
  LiVESWidget *rename_test_rfx;
  LiVESWidget *delete_test_rfx;
  LiVESWidget *promote_test_rfx;

  /// for the fileselection preview
  LiVESWidget *fs_playarea;
  LiVESWidget *fs_playalign;
  LiVESWidget *fs_playframe;
  LiVESWidget *fs_playimg;

  /// for the framedraw special widget - TODO - use a sub-struct
  LiVESWidget *framedraw; ///< the eventbox
  LiVESWidget *framedraw_reset; ///< the 'redraw' button
  LiVESWidget *framedraw_preview; ///< the 'redraw' button
  LiVESWidget *framedraw_spinbutton; ///< the frame number button
  LiVESWidget *framedraw_scale; ///< the slider
  LiVESWidget *framedraw_maskbox; ///< box for opacity controls
  LiVESWidget *framedraw_opscale; ///< opacity
  LiVESWidget *framedraw_cbutton; ///< colour for mask
  LiVESWidget *fd_frame; ///< surrounding frame widget

  lives_painter_surface_t *fd_surface;

  weed_plant_t *fd_layer_orig; ///< original layer uneffected
  weed_plant_t *fd_layer; ///< framedraw preview layer

  int framedraw_frame; ///< current displayed frame
  int fd_max_frame; ///< max effected / generated frame

  ulong fd_spin_func; ///< spinbutton for framedraw previews

  LiVESWidget *hbox3;  ///< hbox with start / end spins and selection label (C.E.)

  // bars here -> actually text above bars
  LiVESWidget *vidbar, *laudbar, *raudbar;

  LiVESWidget *spinbutton_end, *spinbutton_start;

  LiVESWidget *sa_button;
  LiVESWidget *sa_hbox;

  LiVESWidget *arrow1;
  LiVESWidget *arrow2;

  lives_cursor_t cursor_style;

  weed_plant_t *filter_map; // the video filter map for rendering
  weed_plant_t *afilter_map; // the audio filter map for renering
  weed_plant_t *audio_event; // event for audio render tracking
  void ** *pchains; // parameter value chains for interpolation

  // frame preview in the separate window
  LiVESWidget *preview_box;
  LiVESWidget *preview_image;
  LiVESWidget *preview_spinbutton;
  LiVESWidget *preview_scale;
  LiVESWidget *preview_hbox;
  int preview_frame;
  ulong preview_spin_func;
  int prv_link;
#define PRV_FREE 0
#define PRV_START 1
#define PRV_END 2
#define PRV_PTR 3
#define PRV_DEFAULT PRV_PTR

  lives_painter_surface_t *si_surface, *ei_surface, *pi_surface;

  LiVESWidget *start_image, *end_image;
  LiVESWidget *playarea;
  LiVESWidget *hseparator;
  LiVESWidget *message_box;
  LiVESWidget *msg_area;
  LiVESWidget *msg_scrollbar;
  LiVESAdjustment *msg_adj;

  lives_painter_surface_t *msg_surface;

  LiVESWidget *clipsmenu;
  LiVESWidget *clipgroups;
  LiVESWidget *cg_submenu;
  LiVESWidget *cg_managegroups;
  LiVESWidget *show_defgroup;
  LiVESWidget *eventbox;
  LiVESWidget *eventbox2;
  lives_painter_surface_t **eb2_psurf;
  LiVESWidget *eventbox3;
  LiVESWidget *eventbox4;
  LiVESWidget *eventbox5;

  // toolbar buttons
  LiVESWidget *t_stopbutton;
  LiVESWidget *t_fullscreen;
  LiVESWidget *t_sepwin;
  LiVESWidget *t_infobutton;

  LiVESWidget *t_slower;
  LiVESWidget *t_faster;
  LiVESWidget *t_forward;
  LiVESWidget *t_back;
  LiVESWidget *t_hide;

  LiVESWidget *toolbar;
  LiVESWidget *tb_hbox;
  LiVESWidget *fs1;
  LiVESWidget *top_vbox;

  LiVESWidget *l0_tb;
  LiVESWidget *l1_tb;
  LiVESWidget *l2_tb;
  LiVESWidget *l3_tb;

  LiVESWidget *lock_audio_checkbutton;
  LiVESWidget *int_audio_checkbutton, *ext_audio_checkbutton;
  LiVESWidget *ext_audio_mon;

  ulong int_audio_func, ext_audio_func, lock_audio_func;

  LiVESWidget *volume_scale;
  LiVESWidget *vol_toolitem;
  LiVESWidget *vol_label;

  // menubar buttons
  LiVESWidget *btoolbar; ///< button toolbar - clip editor
  LiVESWidget *m_sepwinbutton, *m_playbutton, *m_stopbutton, *m_playselbutton, *m_rewindbutton,
              *m_loopbutton, *m_mutebutton;
  LiVESWidget *menu_hbox;
  LiVESWidget *menubar;

  // separate window
  int opwx, opwy;

  // sepwin buttons
  LiVESWidget *preview_controls;
  LiVESWidget *p_playbutton, *p_playselbutton, *p_rewindbutton, *p_loopbutton, *p_mutebutton;
  LiVESWidget *p_mute_img;

  // timer bars
  LiVESWidget *video_draw, *laudio_draw, *raudio_draw;

  int drawsrc;
  lives_painter_surface_t *video_drawable, *laudio_drawable, *raudio_drawable;

  // framecounter
  LiVESWidget *framebar;
  LiVESWidget *framecounter;
  LiVESWidget *spinbutton_pb_fps;
  LiVESWidget *vps_label;
  LiVESWidget *banner;

  LiVESWidget *ldg_menuitem;

  // (sub)menus
  LiVESWidget *files_menu;
  LiVESWidget *edit_menu;
  LiVESWidget *play_menu;
  LiVESWidget *effects_menu;
  LiVESWidget *tools_menu;
  LiVESWidget *audio_menu;
  LiVESWidget *info_menu;
  LiVESWidget *advanced_menu;
  LiVESWidget *vj_menu;
  LiVESWidget *toys_menu;
  LiVESWidget *help_menu;

  // rendered effects
  LiVESWidget *utilities_menu;
  LiVESWidget *utilities_submenu;
  LiVESWidget *rfx_submenu;
  LiVESWidget *gens_menu;
  LiVESWidget *gens_submenu;
  LiVESWidget *run_test_rfx_submenu;
  LiVESWidget *run_test_rfx_menu;
  LiVESWidget *custom_effects_menu;
  LiVESWidget *custom_effects_submenu;
  LiVESWidget *custom_effects_separator;
  LiVESWidget *custom_tools_menu;
  LiVESWidget *custom_tools_submenu;
  LiVESWidget *custom_tools_separator;
  LiVESWidget *custom_gens_menu;
  LiVESWidget *custom_gens_submenu;
  LiVESWidget *custom_utilities_menu;
  LiVESWidget *custom_utilities_submenu;
  LiVESWidget *custom_utilities_separator;
  LiVESWidget *rte_separator;

  LiVESWidget *dsu_widget;

  LiVESWidget *resize_menuitem;
  LiVESWidget *textwidget_focus;

  int num_tracks;
  int *clip_index;

  /// maps frame slots to the presentation values (if >= 0, points to a 'virtual' frame in the source clip, -1 indicates a decoded img. frame
  frames64_t *frame_index;

  // TODO - can this be simplified ?
  boolean close_keep_frames; ///< special value for when generating to clipboard
  boolean only_close; ///< only close clips - do not exit
  boolean no_exit; ///< if TRUE, do not exit after saving set

  volatile boolean is_exiting; ///< set during shutdown (inverse of only_close then) --> status

  ulong pw_scroll_func;
  boolean msg_area_configed;

  lives_proc_thread_t aud_rec_lpt;

  lives_thread_data_t *fg_tdata;
  lives_proc_thread_t def_lpt;
  lives_hook_stack_t **global_hook_stacks;
  LiVESList *all_hstacks;

  /// jack audio player / transport
#ifdef ENABLE_JACK
  boolean jack_trans_poll;
  jack_driver_t *jackd; ///< jack audio playback client
  jack_driver_t *jackd_read; ///< jack audio recorder client
  jack_driver_t *jackd_trans; ///< jack transport client
#define RT_AUDIO
#else
  void *jackd;  ///< dummy
  void *jackd_read; ///< dummy
#endif

  /// pulseaudio player
#ifdef HAVE_PULSE_AUDIO
  pulse_driver_t *pulsed; ///< pulse audio playback device
  pulse_driver_t *pulsed_read; ///< pulse audio recorder device
#define RT_AUDIO
#else
  void *pulsed;
  void *pulsed_read;
#endif

  boolean xrun_active;

  // layouts
  LiVESTextBuffer *layout_textbuffer; ///< stores layout errors
  LiVESList *affected_layouts_map; ///< map of layouts with errors
  LiVESList *current_layouts_map; ///< map of all layouts for set

  /// list of pairs of marks in affected_layouts_map, text between them should be deleted when
  /// stored_layout is deleted
  LiVESList *affected_layout_marks;

  /// immediately (to be) affected layout maps
  LiVESList *xlays;

  /// crash recovery system
  LiVESList *recovery_list;
  char *recovery_file;  ///< the filename of our recover file
  boolean leave_recovery;
  boolean recoverable_layout;
  boolean invalid_clips;
  boolean recovering_files;
  boolean hard_recovery;
  boolean recording_recovered;

  boolean unordered_blocks; ///< are we recording unordered blocks ?

  mt_opts multi_opts; ///< some multitrack options that survive between mt calls

  /// mutices
  pthread_mutex_t abuf_mutex;  ///< used to synch audio buffer request count - shared between audio and video threads
  pthread_mutex_t abuf_frame_mutex;  ///< used to synch audio buffer for generators
  pthread_mutex_t abuf_aux_frame_mutex;  ///< used to synch audio buffer for loopback
  pthread_mutex_t fxd_active_mutex; ///< prevent simultaneous writing to active_dummy by audio and video threads
  pthread_mutex_t event_list_mutex; ///< prevent simultaneous writing to event_list by audio and video threads
  pthread_mutex_t clip_list_mutex; ///< prevent adding/removing to cliplist while another thread could be reading it
  pthread_mutex_t vpp_stream_mutex; ///< prevent from writing audio when stream is closing
  pthread_mutex_t cache_buffer_mutex; ///< sync for jack playback termination
  pthread_mutex_t audio_filewriteend_mutex; ///< sync for ending writing audio to file
  pthread_mutex_t exit_mutex; ///< prevent multiple threads trying to run cleanup
  pthread_mutex_t fbuffer_mutex; /// append / remove with file_buffer list
  pthread_mutex_t alarmlist_mutex; /// single access for updating alarm list
  pthread_mutex_t trcount_mutex; /// transition count mutex
  pthread_mutex_t alock_mutex; /// audio lock / unlock
  pthread_mutex_t all_hstacks_mutex; ///

  pthread_mutex_t fx_key_mutex[FX_KEYS_MAX_VIRTUAL];
  int fx_mutex_nlocks[FX_KEYS_MAX_VIRTUAL];
  uint64_t fx_mutex_tuid[FX_KEYS_MAX_VIRTUAL];

  ///< set for param window updates from OSC or data connections, notifies main thread to do visual updates
  volatile lives_rfx_t *vrfx_update;

  lives_fx_candidate_t
  ///< effects which can have candidates from which a delegate is selected (current examples are: audio_volume, resize)
  fx_candidates[MAX_FX_CANDIDATE_TYPES];

  /// file caches (readonly)
  LiVESList *prefs_cache;  ///< cache of preferences, used during startup phase
  LiVESList *hdrs_cache;  ///< cache of a file header (e.g. header.lives)
  LiVESList *meta_cache;  ///< metadata cache
  LiVESList *gen_cache;  ///< general cache

  FILE *clip_header; /// for writing header.lives values

  LiVESList *file_buffers; ///< list of open files for buffered i/o

  int aud_rec_fd; ///< fd of file we are recording audio to
  double rec_end_time;
  int64_t rec_samples;
  double rec_fps;
  frames_t rec_vid_frames;

  lives_audio_buf_t *alock_abuf; // experimental

  ///< values to be written to the event list concurrent with next video ftame event
  int rec_arate, rec_achans, rec_asamps, rec_signed_endian;

  /// message output settings
  int last_dprint_file;
  boolean no_switch_dprint;
  boolean suppress_dprint; ///< tidy up, e.g. by blocking "switched to file..." and "closed file..." messages

  char *string_constants[NUM_LIVES_STRING_CONSTANTS];
  char *any_string;  ///< localised text saying "Any", for encoder and output format
  char *none_string;  ///< localised text saying "None", for playback plugin name, etc.
  char *recommended_string;  ///< localised text saying "recommended", for encoder and output format, etc.
  char *disabled_string;  ///< localised text saying "disabled !", for playback plugin name, etc.
  char *cl_string; ///< localised text saying "*The current layout*", for layout warnings

  frames_t opening_frames; ///< count of frames so far opened, updated after preview (currently)

  boolean show_procd; ///< override showing of "processing..." dialog

  boolean block_param_updates; ///< block visual param changes from updating real values
  boolean no_interp; ///< block interpolation (for single frame previews)

  ticks_t cevent_tc; ///< timecode of currently processing event

  boolean opening_multi; ///< flag to indicate multiple file selection

  int img_concat_clip;  ///< when opening multiple, image files can get concatenated here (prefs->concat_images)

  /// rendered generators
  boolean gen_to_clipboard;
  boolean is_generating;

  boolean keep_pre; ///< set if previewed frames should be retained as processed frames (for rendered effects / generators)

  /// video plugin
  _vid_playback_plugin *vpp;
  const char *new_vpp;

  /// multi-head support
  lives_mgeometry_t *mgeom;

  /// external control inputs
  boolean ext_cntl[MAX_EXT_CNTL];

#ifdef ALSA_MIDI
  snd_seq_t *seq_handle;
  int alsa_midi_port;
  int alsa_midi_dummy;
#endif

  boolean midi_channel_lock;

  weed_plant_t *rte_textparm; ///< send keyboard input to this parameter (usually NULL)

  int write_abuf; ///< audio buffer number to write to (for multitrack)
  volatile int abufs_to_fill;

  boolean first_shown; // used during setup phase
  boolean lives_shown; // ensures we always show the main window

  /// splash window
  LiVESWidget *splash_window, *splash_label, *splash_progress;

  /// encoder text output
  LiVESIOChannel *iochan;
  LiVESTextView *optextview;

  boolean has_custom_effects, has_custom_tools,  has_custom_gens, has_custom_utilities;
  boolean has_test_effects;
  volatile boolean rfx_loaded;

  boolean debug; ///< debug crashes and asserts
  FILE *debug_log;
  void *debug_ptr;

  char *subt_save_file; ///< name of file to save subtitles to

  LiVESTargetEntry *target_table; ///< drag and drop target table

  int log_fd; /// unused for now

  lives_timeout_t alarms[LIVES_MAX_ALARMS]; ///< reserve 1 for emergency msgs
  lives_alarm_t next_free_alarm;

  /// OSD
  char *urgency_msg;
  char *overlay_msg;

  // stuff specific to audio gens
  volatile int agen_key; ///< which fx key is generating audio [1 based] (or 0 for none)
  volatile boolean agen_needs_reinit;
  uint64_t agen_samps_count; ///< count of samples since init

  boolean aplayer_broken;

  boolean add_clear_ds_button;
  boolean add_clear_ds_adv;
  boolean tried_ds_recover;

  boolean has_session_workdir;

  int ce_frame_width, ce_frame_height;

  lives_render_error_t render_error;

  uint64_t next_ds_warn_level; ///< current disk space warning level for the tempdir

  lives_pconnect_t *pconx; ///< list of out -> in param connections
  lives_cconnect_t *cconx; ///< list of out -> in alpha channel connections

  int sepwin_minwidth, sepwin_minheight;

  boolean gui_hidden;

  uint32_t signal_caught;
  boolean signals_deferred;

  boolean ce_thumbs;
  boolean ce_upd_clip;

#define SCREEN_AREA_NONE		-1
#define SCREEN_AREA_AUTO		0
#define SCREEN_AREA_FOREGROUND		1
#define SCREEN_AREA_BACKGROUND		2
#define SCREEN_AREA_USER_DEFINED1	3

  int n_screen_areas; // number of screen areas
  int active_sa_fx; // active screen area for effects
  int active_sa_clips; // active screen area for clips
  lives_screen_area_t *screen_areas; // array of screen areas

  int active_track_list[MAX_TRACKS];
  boolean primary_src_used[MAX_FILES];
  lives_clipsrc_group_t *track_sources[MAX_TRACKS];
  int old_active_track_list[MAX_TRACKS];

  boolean gen_started_play;

  lives_audio_buf_t *afbuffer; ///< used for buffering / feeding audio to video generators

  volatile lives_audio_buf_t *audio_frame_buffer_aux; ///< used for buffering / feeding to loopback
  lives_audio_buf_t *afb_aux[2]; ///< used for buffering / feeding audio to loopback
  int afbuffer_aux_clients; /// # of registered clients for the aux audio frame buffer
  int afbuffer_aux_clients_read; /// current read count. When this reaches abuffer_clients_aux, we swap the read / write buffers

  pthread_t *libthread;  /// GUI thread for liblives

#define LIVES_SENSE_STATE_UNKNOWN 0
#define LIVES_SENSE_STATE_INSENSITIZED (1 << 0)
#define LIVES_SENSE_STATE_PROC_INSENSITIZED (1 << 1)
#define LIVES_SENSE_STATE_SENSITIZED (1 << 16)
#define LIVES_SENSE_STATE_INTERACTIVE (1 << 31)

#define LIVES_IS_INTERACTIVE ((mainw->sense_state & LIVES_SENSE_STATE_INTERACTIVE) ? TRUE : FALSE)
#define LIVES_IS_SENSITIZED ((mainw->sense_state & LIVES_SENSE_STATE_SENSITIZED) ? TRUE : FALSE)

  uint32_t sense_state;

  char frameblank_path[PATH_MAX];
  char sepimg_path[PATH_MAX];

  uint64_t aud_data_written;

  int crash_possible; // set this to a number before defer_sigint
  const lives_funcdef_t *err_funcdef;

  LiVESPixbuf *scrap_pixbuf; ///< cached image for speeding up rendering
  weed_layer_t *scrap_layer; ///< cached image for speeding up rendering

  boolean gui_much_events;
  volatile boolean do_ctx_update;
  boolean no_context_update; ///< may be set temporarily to block wodget context updates

  weed_plant_t *msg_list;
  weed_plant_t *ref_message; // weak ref
  int n_messages;
  int ref_message_n;
  int mbar_res; /// reserved space for mbar

  ticks_t flush_audio_tc; ///< when rendering, we can use this to force audio to be rendered up to tc; designed for previews

  // main window resizing, no longer very important
  int assumed_width;
  int assumed_height;
#define DEF_IDLE_MAX 1
  int idlemax;

  boolean configured;  // determines whether config_func is inital config or screen resize
  boolean calibrated;

  boolean reconfig; ///< set to TRUE if a monitor / screen size change is detected

  boolean ignore_screen_size; ///< applied during  frame reconfig events

  int def_trans_idx;

  int ds_mon;

#define CHECK_CRIT		(1 << 0)
#define CHECK_WARN		(1 << 1)
#define CHECK_QUOTA		(1 << 2)

  char *version_hash;
  char *old_vhash;

  /// experimental value, primarily for testing
  volatile int uflow_count;

  volatile boolean
  force_show; /// if set to TRUE during playback then a new frame (or possibly the current one) will be displayed ASAP

  /// adaptive quality settings
  ///< a roughly calibrated value that ranges from -64 (lightly loaded) -> +64 (heavily loaded)
  /// (currently only active during playback), i.e higher values represent more machine load
  /// some functions may change their behaviour according to this value if prefs->pbq_adaptive is FALSE then such changes
  /// should be minimal; if TRUE then more profound changes are permitted
#define EFFORT_RANGE_MAXD ((double)64.0)
#define EFFORT_RANGE_MAX 64
#define EFFORT_LIMIT_LOW (EFFORT_RANGE_MAX >> 3)    ///< default 8
#define EFFORT_LIMIT_MED (EFFORT_RANGE_MAX >> 2)  ///< default 32
  int effort;
  boolean lockstats;
  double disk_pressure;

  boolean memok; ///< set to FALSE if a segfault is received, ie. we should assume all memory is corrupted and exit ASAP

  LiVESList *restart_params;

  /// this is not really used yet, but the idea is that in future the clipboard may be reproduced in various
  /// sizes / palettes / gamma functions, so instead of having to transform it each time we can cache various versions
#define MAX_CBSTORES 8
  int ncbstores;
  lives_clip_t *cbstores[8];

  /// caches for start / end / preview images. This avoids having to reload / reread them from the source, which could disrupt playback.
  weed_layer_t *st_fcache, *en_fcache, *pr_fcache;
  /// these are freed when the clip is switched or closed, or when the source frame changes or is updated
  ////////////////////
  boolean add_trash_rb;
  boolean cs_manage;

  boolean dsu_valid;
  int max_textsize;

  lives_permmgr_t *permmgr;

  volatile boolean pretty_colours;

  boolean suppress_layout_warnings;

  volatile lives_proc_thread_t helper_procthreads[N_HLP_PROCTHREADS];

  boolean no_configs;

#define MONITOR_QUOTA (1 << 0)

  uint32_t disk_mon;

  volatile boolean transrend_waiting;
  volatile weed_layer_t *transrend_layer;
  boolean pr_audio;
  double vfade_in_secs, vfade_out_secs;
  lives_colRGBA64_t vfade_in_col, vfade_out_col;
} mainwindow;

/// interface colour settings
extern _palette *palette;

typedef struct {
  ulong ins_frame_function;

  LiVESWidget *merge_dialog;
  LiVESWidget *ins_frame_button;
  LiVESWidget *drop_frame_button;
  LiVESWidget *param_vbox;
  LiVESWidget *spinbutton_loops;

  boolean loop_to_fit;
  boolean align_start;
  boolean ins_frames;

  int *list_to_rfx_index;
  LiVESList *trans_list;
} _merge_opts;

typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *resetbutton;
  LiVESWidget *savebutton;
  lives_rfx_t *rfx;
  int key;
  int mode;
} _fx_dialog;

_merge_opts *merge_opts;

_fx_dialog *fx_dialog[2];

#ifndef IS_MINGW
#define LIVES_SIGKILL SIGKILL
#define LIVES_SIGINT  SIGINT
#define LIVES_SIGPIPE SIGPIPE
#define LIVES_SIGTRAP SIGTRAP
#define LIVES_SIGUSR1 SIGUSR1
#define LIVES_SIGABRT SIGABRT
#define LIVES_SIGSEGV SIGSEGV
#define LIVES_SIGHUP  SIGHUP
#define LIVES_SIGTERM SIGTERM
#define LIVES_SIGQUIT SIGQUIT
#define LIVES_SIGFPE  SIGFPE
#else
#define LIVES_SIGKILL SIGTERM
#define LIVES_SIGINT  SIGINT
#define LIVES_SIGPIPE SIGPIPE
#define LIVES_SIGTRAP SIGTRAP
#define LIVES_SIGUSR1 SIGUSR1
#define LIVES_SIGABRT SIGABRT
#define LIVES_SIGSEGV SIGSEGV
#define LIVES_SIGFPE  SIGSEGV
#define LIVES_SIGHUP  SIGINT
#define LIVES_SIGTERM SIGTERM
#define LIVES_SIGQUIT SIGQUIT
#endif

#ifdef ENABLE_JACK
volatile aserver_message_t jack_message;
volatile aserver_message_t jack_message2;
#endif

#ifdef HAVE_PULSE_AUDIO
volatile aserver_message_t pulse_message;
volatile aserver_message_t pulse_message2;
#endif

#endif // HAS_LIVES_MAINWINDOW_H
