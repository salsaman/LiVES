// liblives.hpp
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

/** \file liblives.hpp
    Header file for liblives.
 */

#ifndef HAS_LIBLIVES_H
#define HAS_LIBLIVES_H

// defs shared with lbindings.c

/**
   Filechooser hinting types
*/
typedef enum {
  LIVES_FILE_CHOOSER_VIDEO_AUDIO,  ///< file chooser options for single video or audio file
  LIVES_FILE_CHOOSER_AUDIO_ONLY, ///< file chooser options for single audio file
  //  LIVES_FILE_CHOOSER_VIDEO_AUDIO_MULTI, ///< file chooser options for multiple video or audio files
  //LIVES_FILE_CHOOSER_VIDEO_RANGE ///< file chooser options for video range (start time/number of frames)
} lives_filechooser_t;

/**
   LiVES operation mode
*/
typedef enum {
  LIVES_MODE_CLIPEDIT=0, ///< clip editor mode
  LIVES_MODE_MULTITRACK ///< multitrack mode
} lives_mode_t;


#include "osc_notify.h"


typedef enum {
  LIVES_CALLBACK_FRAME_SYNCH = LIVES_OSC_NOTIFY_FRAME_SYNCH, ///< sent when a frame is displayed
  LIVES_CALLBACK_PLAYBACK_STARTED = LIVES_OSC_NOTIFY_PLAYBACK_STARTED,  ///< sent when a/v playback starts or clip is switched
  LIVES_CALLBACK_PLAYBACK_STOPPED = LIVES_OSC_NOTIFY_PLAYBACK_STOPPED, ///< sent when a/v playback ends

  /// sent when a/v playback ends and there is recorded data for 
  /// rendering/previewing
  LIVES_CALLBACK_PLAYBACK_STOPPED_RD = LIVES_OSC_NOTIFY_PLAYBACK_STOPPED_RD,

  LIVES_CALLBACK_RECORD_STARTED = LIVES_OSC_NOTIFY_RECORD_STARTED, ///< sent when record starts (TODO)
  LIVES_CALLBACK_RECORD_STOPPED = LIVES_OSC_NOTIFY_RECORD_STOPPED, ///< sent when record stops (TODO)

  LIVES_CALLBACK_QUIT = LIVES_OSC_NOTIFY_QUIT, ///< sent when app quits

  LIVES_CALLBACK_CLIP_OPENED = LIVES_OSC_NOTIFY_CLIP_OPENED, ///< sent after a clip is opened
  LIVES_CALLBACK_CLIP_CLOSED = LIVES_OSC_NOTIFY_CLIP_CLOSED, ///< sent after a clip is closed

  LIVES_CALLBACK_CLIPSET_OPENED = LIVES_OSC_NOTIFY_CLIPSET_OPENED, ///< sent after a clip set is opened
  LIVES_CALLBACK_CLIPSET_SAVED = LIVES_OSC_NOTIFY_CLIPSET_SAVED, ///< sent after a clip set is closed

  LIVES_CALLBACK_MODE_CHANGED = LIVES_OSC_NOTIFY_MODE_CHANGED,

  LIVES_CALLBACK_OBJECT_DESTROYED = 16384, ///< sent when livesApp object is deleted

  LIVES_CALLBACK_PRIVATE = 32768 ///< for internal use
} lives_callback_t;




////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus

#ifndef DOXYGEN_SKIP

#if defined _WIN32 || defined __CYGWIN__
  #ifdef BUILDING_DLL
    #ifdef __GNUC__
      #define LIVES_DLL_PUBLIC __attribute__ ((dllexport))
    #else
      #define LIVES_DLL_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #else
    #ifdef __GNUC__
      #define LIVES_DLL_PUBLIC __attribute__ ((dllimport))
    #else
      #define LIVES_DLL_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define LIVES_DLL_PUBLIC __attribute__ ((visibility ("default")))
    #define LIVES_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define LIVES_DLL_PUBLIC
    #define LIVES_DLL_LOCAL
  #endif
#endif

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////

#include <list>
#include <string.h>
#include <inttypes.h>

/**
   typedef
*/
typedef unsigned long ulong;


#ifndef DOXYGEN_SKIP
extern "C" {
  void binding_cb (lives_callback_t cb_type, const char *msgstring, uint64_t id);
}
#endif

using namespace std;

/////////////////////////////////////////////////////////////////////////////////////////////////////





/**
   lives namespace.
*/
namespace lives {

  /**
     typedef
  */
  typedef class livesApp livesApp;

#ifndef DOXYGEN_SKIP
  typedef void *(*callback_f)(void *);

  typedef struct {
    ulong id;
    livesApp *object;
    lives_callback_t cb_type;
    callback_f func;
    void *data;
  } closure;

  typedef list<closure *> closureList;
  typedef list<closure *>::iterator closureListIterator;
#endif

  /** \struct
     Data returned in modeChanged callback.
  */
  typedef struct {
    lives_mode_t mode; ///< mode changed to
  } modeChangedInfo;


#ifndef DOXYGEN_SKIP
  typedef struct {
    ulong id;
    char *response;
  } _privateInfo;


  typedef bool (*private_callback_f)(_privateInfo *, void *);
#endif
  /**
     modeChanged_callback_f.
  */
  typedef bool (*modeChanged_callback_f)(livesApp *lives, modeChangedInfo *, void *);


  /**
     objectDestroyed_callback_f.
  */
  typedef bool (*objectDestroyed_callback_f)(livesApp *lives, void *);

  /**
    typedef
  */
  typedef class set set;

  /**
     typedef
  */
  typedef class clip clip;

  /**
     class "clip". 
     Represents a clip which is open in LiVES.
  */
  class LIVES_DLL_PUBLIC clip {
    friend livesApp;
    friend set;

  public:
    /**
       Public constructor. 
       Creates a clip instance which starts off invalid.
    */
    clip();

    /**
       Check if clip is valid. 
       A valid clip may be returned from livesApp::openFile() for example.
       @return true if the clip is valid.
    */
    bool isValid();

    /**
       Number of frames in this clip. 
       If clip is not valid then -1 is returned.
       @return int number of frames, or -1 if clip is not valid.
    */
    int frames();

    bool select();

    /**
       @return true if the two clips have the same unique_id.
    */
    inline bool operator==(const clip& other) {
      return other.m_uid == m_uid;
    }

  protected:
    clip(ulong uid);

  private:
    ulong m_uid;

  };
  


/**
  typedef
*/
  typedef list<clip *> clipList;


/**
  typedef
*/
  typedef list<clip *>::iterator clipListIterator;

  ///// set ////////


  /**
     class "set". 
     Represents a list of clips and/or layouts which are open in LiVES. May be returned from livesApp::currentSet().
  */
  class LIVES_DLL_PUBLIC set {
    friend livesApp;

  public:
    /**
       Creates a set. 
       The set begins as invalid until returned from some method.
       @return an initially invalid set.
    */
    set();

    ~set();

    /**
       Returns whether the set is valid or not.
       @return true if the set is valid (connected to a livesApp instance).
    */
    bool isValid();

    /**
       Returns the current name of the set. 
       If it has not been defined, an empty string is returned. If the set is invalid, NULL is returned.
       @return const char *name, which should not be freed or altered.
    */
    const char *name();

    /**
       Save the set, and close all open clips and layouts. 
       If the set name is empty, the user can choose the name via the GUI. 
       If the name is defined, and it points to a different, existing set, the set will not be saved and false will be returned, 
       unless force_append is set to true, in which case the current clips and layouts will be appended to the other set.
       @param name name to save set as, or "" to let the user choose a name.
       @param force_append set to true to force appending to another existing set.
       @return true if the set was saved.
    */
    bool save(const char *name="", bool force_append=false);

    /**
       Returns a list of all currently opened clips in the currently opened set.
       Be careful, calling cliplist() a second time may invalidate the previous return value and all the clips in it.
       @ return cliplist for use in e.g. size() or for obtaining a clip via "=".
    */
    clipList cliplist();

  protected:
    set(livesApp *lives);
    void setName(const char *setname);

  private:
    livesApp *m_lives;
    char *m_name;
    clipList m_clips;
    //list<layout *>layouts;
  };



  /**
     class "livesApp". 
     Represents a single LiVES application. Note that currently only one such instance can be valid at a time, 
     attempting to create a second concurrent instance will return an invalid instance.
  */
  class LIVES_DLL_PUBLIC livesApp {
    friend set;

  public:
    /**
       Constructor with no arguments.
    */
    livesApp();

    /**
       Constructor with argc, argv arguments.
       argv array is equivalent to commandline options passed to the LiVES application.
       @param argc count of number of arguments
       @param argv[] array of options.
    */
    livesApp(int argc, char *argv[]);

    /**
       Destructor: closes the LiVES application.
       Deletes currentSet(). Deletes any callbacks which were set for this instance.
    */
    ~livesApp();

    /**
       Returns whether the instance is valid or not.
       A valid instance is connected to a running LiVES application.
       @return true if instance is connected to a running LiVES application.
    */
    bool isValid();

    /**
       Returns the loaded set.
       Beware, if the livesApp instance is deleted, this is also deleted.
       @return the current set in LiVES.
    */
    const set& currentSet();

    /**
       Commence playback of video and audio with the currently selected clip.
       If LiVES is already playing, or is busy, nothing will happen.
    */
    void play();

    /**
       Stop playback.
       If LiVES is not playing, nothing happens.
       @return true if playback was stopped.
    */
    bool stop();

    /**
       Remove a previously added callback.
       @param id value previously returned from addCallback.
       @return true if playback was stopped.
    */
    bool removeCallback(ulong id);

    /**
       Add a modeChanged callback.
       @param msgnum must have value LIVES_CALLBACK_MODE_CHANGED.
       @param func function to be called when this signal is received.
       @param data data to be passed to callback function
       @return unsigned long callback_id
    */
    ulong addCallback(lives_callback_t cb_type, modeChanged_callback_f func, void *data);


    /**
       Add an objectDestroyed callback.
       @param msgnum must have value LIVES_CALLBACK_OBJECT_DESTROYED
       @param func function to be called when this signal is received.
       @param data data to be passed to callback function
       @return unsigned long callback_id
    */
    ulong addCallback(lives_callback_t cb_type, objectDestroyed_callback_f func, void *data);


    /**
       Show Info dialog in the LiVES GUI.
       @param text text to be diaplayed in the dialog.
       @param blocking if true then function will block until the user presses "OK"
       @return if blocking, returns the response code from the dialog.
    */
    int showInfo(const char *text, bool blocking=true);

   /**
       Allow the user choose a file via a fileselector.
       After returning, the setting that the user selected for deinterlace may be obtained by calling 
       livesApp::deinterlaceOption(). This value can the be passed into openFile().
       Chooser type will direct the user towards the type of file to choose, however there is no guarantee 
       that a file of the "correct" type will be returned.
       @param dirname directory name to start in (or NULL)
       @param chooser_type must be either LIVES_FILE_CHOOSER_VIDEO_AUDIO or LIVES_FILE_CHOOSER_AUDIO_ONLY.
       @param title title of window to display, or NULL to use a default title.
       @return the name of the file selected.
    */
    char *chooseFileWithPreview(const char *dirname, lives_filechooser_t chooser_type, const char *title=NULL);

   /**
      Open a file and return a clip for it.
      @param fname the full pathname of the file to open
      @param with_audio if true the audio will be loaded as well as the video
      @param stime the time in seconds from which to start loading
      @param frames number of frames to open (0 means all frames)
      @param deinterlace set to true to force deinterlacing
    */
    clip openFile(const char *fname, bool with_audio=true, double stime=0., int frames=0, bool deinterlace=false);

    /**
       Change the interactivity of the GUI application.
       Interctivity is via menus and keyboard accelerators
       @param setting set to true to allow interaction with the GUI.
    */
    void setInteractive(bool setting);

    /**
       Returns whether the GUI app is in interactive mode.
       @return true if GUI interactivity via menus and keyboard accelerators is enabled.
    */
    bool interactive();


    /**
       Returns last setting of deinterlace by user.
       @return value that the user selected during the last filechooser with preview operation.
       This value may be passed into openFile().
    */
    bool deinterlaceOption();


    /**
       For internal use only.
    */
    closureList closures();

  protected:
    LIVES_DLL_LOCAL ulong addCallback(lives_callback_t cb_type, private_callback_f func, void *data);


  private:
    ulong m_id;
    set m_set;
    closureList m_closures;

    bool m_deinterlace;

    LIVES_DLL_LOCAL ulong appendClosure(lives_callback_t cb_type, callback_f func, void *data);
    LIVES_DLL_LOCAL void init(int argc, char *argv[]);

    livesApp(livesApp const&);              // Don't Implement
    void operator=(livesApp const&); // Don't implement


  };

  /**
     Preferences.
  */
  namespace prefs {
    const char *currentVideoLoadDir(); ///< current video load directory.

  }


}

#endif // __cplusplus

#endif //HAS_LIBLIVES_H
