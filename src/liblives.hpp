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
  LIVES_INTERFACE_MODE_INVALID=-1, ///< livesApp instance is invalid
  LIVES_INTERFACE_MODE_CLIPEDIT, ///< clip editor mode
  LIVES_INTERFACE_MODE_MULTITRACK ///< multitrack mode
} lives_interface_mode_t;


/**
   LiVES operational status
*/
typedef enum {
  LIVES_STATUS_INVALID=-1, ///< livesApp instance is invalid
  LIVES_STATUS_NOTREADY, ///< application is staring up, not ready
  LIVES_STATUS_READY, ///< application is ready for commands
  LIVES_STATUS_PLAYING, ///< application is playing, only player commands will be responded to
  LIVES_STATUS_PROCESSING, ///< application is processing, commands will be ignored
  LIVES_STATUS_PREVIEW ///< user is previewing an operation, commands will be ignored
} lives_status_t;


/**
   Endian values
*/
typedef enum {
  LIVES_LITTLEENDIAN,
  LIVES_BIGENDIAN
} lives_endian_t;


/**
   Callback types
*/
typedef enum {
  LIVES_CALLBACK_FRAME_SYNCH = 1, ///< sent when a frame is displayed
  LIVES_CALLBACK_PLAYBACK_STARTED = 2,  ///< sent when a/v playback starts or clip is switched
  LIVES_CALLBACK_PLAYBACK_STOPPED = 3, ///< sent when a/v playback ends
  /// sent when a/v playback ends and there is recorded data for 
  /// rendering/previewing
  LIVES_CALLBACK_PLAYBACK_STOPPED_RD = 4,

  LIVES_CALLBACK_RECORD_STARTED = 32, ///< sent when record starts (TODO)
  LIVES_CALLBACK_RECORD_STOPPED = 33, ///< sent when record stops (TODO)

  LIVES_CALLBACK_APP_QUIT = 64, ///< sent when app quits

  LIVES_CALLBACK_CLIP_OPENED = 128, ///< sent after a clip is opened
  LIVES_CALLBACK_CLIP_CLOSED = 129, ///< sent after a clip is closed


  LIVES_CALLBACK_CLIPSET_OPENED = 256, ///< sent after a clip set is opened
  LIVES_CALLBACK_CLIPSET_SAVED = 257, ///< sent after a clip set is closed

  LIVES_CALLBACK_MODE_CHANGED = 4096,

  LIVES_CALLBACK_OBJECT_DESTROYED = 16384, ///< sent when livesApp object is deleted

#ifndef DOXYGEN_SKIP
  LIVES_CALLBACK_PRIVATE = 32768 ///< for internal use
#endif
} lives_callback_t;



/**
   Character encoding types
*/
typedef enum {
  LIVES_CHAR_ENCODING_UTF8, ///< UTF-8 char encoding
  LIVES_CHAR_ENCODING_LOCAL8BIT, ///< 8 bit locale file encoding
  LIVES_CHAR_ENCODING_FILESYSTEM, ///< file system encoding (UTF-8 on windows, local8bit on others)
  //LIVES_CHAR_ENCODING_UTF16, ///< UTF-16 char encoding
} lives_char_encoding_t;


/**
   Dialog response values
*/
typedef enum {
// positive values for custom responses
  LIVES_DIALOG_RESPONSE_INVALID=-1,
  LIVES_DIALOG_RESPONSE_NONE=0,
  LIVES_DIALOG_RESPONSE_OK,
  LIVES_DIALOG_RESPONSE_RETRY,
  LIVES_DIALOG_RESPONSE_ABORT,
  LIVES_DIALOG_RESPONSE_RESET,
  LIVES_DIALOG_RESPONSE_SHOW_DETAILS,
  LIVES_DIALOG_RESPONSE_CANCEL,
  LIVES_DIALOG_RESPONSE_ACCEPT,
  LIVES_DIALOG_RESPONSE_YES,
  LIVES_DIALOG_RESPONSE_NO
} lives_dialog_response_t;

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

#include <string>

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

  ////////////////////////////////////////////////////

  /**
     typedef
  */
  typedef class livesApp livesApp;

  /**
    typedef
  */
  typedef class set set;

  /**
     typedef
  */
  typedef class clip clip;

  /**
     typedef
  */
  typedef class LiVESString LiVESString;




  ///////////////////////////////////////////////////


  /**
     class "LiVESString". 
     A string type which handles different character encodings.
  */
  class LiVESString : public std::string {
  public:
    LiVESString() : m_encoding(LIVES_CHAR_ENCODING_UTF8) {};
    LiVESString(const string& str) {};
    LiVESString (const string& str, size_t pos, size_t len = npos);
    LiVESString(const char* s) {};
    LiVESString(const char* s, size_t n) {};
    LiVESString(size_t n, char c) {};
    template <class InputIterator>
    LiVESString  (InputIterator first, InputIterator last);

    /**
       Change the character encoding of the string.
       @param enc the character encoding to convert to.
    */
    void toEncoding(lives_char_encoding_t enc);

  private:
    lives_char_encoding_t m_encoding;
  };

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

  /**
     Struct passed to modeChanged callback.
  */
  typedef struct {
    lives_interface_mode_t mode; ///< mode changed to
  } modeChangedInfo;


  /**
     Struct passed to appQuit callback.
  */
  typedef struct {
    int signum; ///< signal which caused the app to exit, or 0 if the user or script quit normally.
  } appQuitInfo;


#ifndef DOXYGEN_SKIP
  typedef struct {
    ulong id;
    char *response;
  } _privateInfo;


  typedef bool (*private_callback_f)(_privateInfo *, void *);
#endif

  /**
     Type of callback function for LIVES_CALLBACK_MODE_CHANGED.
     @see LIVES_CALLBACK_MODE_CHANGED
     @see livesApp::addCallback(lives_callback_t cb_type, modeChanged_callback_f func, void *data)
  */
  typedef bool (*modeChanged_callback_f)(livesApp *lives, modeChangedInfo *, void *);

  /**
     Type of callback function for LIVES_CALLBACK_APP_QUIT.
     @see LIVES_CALLBACK_APP_QUIT
     @see livesApp::addCallback(lives_callback_t cb_type, appQuit_callback_f func, void *data)
  */
  typedef bool (*appQuit_callback_f)(livesApp *lives, appQuitInfo *, void *);


  /**
     Type of callback function for LIVES_CALLBACK_OBJECT_DESTROYED.
     @see LIVES_CALLBACK_OBJECT_DESTROYED
     @see livesApp::addCallback(lives_callback_t cb_type, objectDestroyed_callback_f func, void *data)
  */
  typedef bool (*objectDestroyed_callback_f)(livesApp *lives, void *);


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
       @see livesApp::openFile().
    */
    clip();

    /**
       Check if clip is valid. 
       @see livesApp::openFile().
       @return true if the clip is valid.
    */
    bool isValid();

    /**
       Number of frames in this clip. 
       If the clip is audio only, 0 is returned.
       If clip is not valid then -1 is returned.
       @return int number of frames, or -1 if clip is not valid.
    */
    int frames();

    /**
       Width of the clip in pixels.
       If the clip is audio only, 0 is returned.
       If clip is not valid then -1 is returned.
       @return int width in pixels, or -1 if clip is not valid.
    */
    int width();

    /**
       Height of the clip in pixels.
       If the clip is audio only, 0 is returned.
       If clip is not valid then -1 is returned.
       @return int height in pixels, or -1 if clip is not valid.
    */
    int height();


    /**
       Framerate (frames per second) of the clip.
       If the clip is audio only, 0.0 is returned.
       If clip is not valid then -1.0 is returned.
       @return double framerate of the clip, or -1.0 if clip is not valid.
    */
    double fps();


    /**
       Human readable name of the clip.
       If clip is not valid then empty string is returned.
       @return LiVESString name, or empty string if clip is not valid.
    */
    LiVESString name();


    /**
       Audio rate for this clip. 
       If the clip is video only, 0 is returned.
       If clip is not valid then -1 is returned.
       @return int audio rate, or -1 if clip is not valid.
    */
    int audioRate();


    /**
       Number of audio channels (eg. left, right) for this clip. 
       If the clip is video only, 0 is returned.
       If clip is not valid then -1 is returned.
       @return int audio channels, or -1 if clip is not valid.
    */
    int audioChannels();


    /**
       Size in bits of audio samples (eg. 8, 16, 32) for this clip. 
       If the clip is video only, 0 is returned.
       If clip is not valid then -1 is returned.
       @return int audio sample size, or -1 if clip is not valid.
    */
    int audioSampleSize();


    /**
       Returns whether the audio is signed (true) or unsigned (false).
       If clip is video only or not valid then the return value is undefined.
       @return bool audio signed.
    */
    bool audioSigned();


    /**
       Returns the endianness of the audio.
       If clip is video only or not valid then the return value is undefined.
       @return bool audio signed.
    */
    lives_endian_t audioEndian();


    /**
       Start of the selected frame region.
       If the clip is audio only, 0 is returned.
       If clip is not valid then -1 is returned.
       @return int frame selection start, or -1 if clip is not valid.
    */
    int selectionStart();


    /**
       End of the selected frame region.
       If the clip is audio only, 0 is returned.
       If clip is not valid then -1 is returned.
       @return int frame selection end, or -1 if clip is not valid.
    */
    int selectionEnd();



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
       If it has not been defined, an empty string is returned. If the set is invalid, an empty string is returned.
       @return LiVESString name.
    */
    LiVESString name();

    /**
       Save the set, and close all open clips and layouts. 
       If the set name is empty, the user can choose the name via the GUI. 
       If the name is defined, and it points to a different, existing set, the set will not be saved and false will be returned, 
       unless force_append is set to true, in which case the current clips and layouts will be appended to the other set.
       @param name name to save set as, or empty string to let the user choose a name.
       @param force_append set to true to force appending to another existing set.
       @return true if the set was saved.
    */
    bool save(LiVESString name, bool force_append=false);

    /**
       Returns a list of all currently opened clips in the currently opened set.
       Be careful, calling cliplist() a second time may invalidate the previous return value and all the clips in it.
       @ return cliplist for use in e.g. size() or for obtaining a clip via copying.
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

    set(set const&);              // Don't Implement
    void operator=(set const&); // Don't implement

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
    set * const currentSet();

    /**
       Commence playback of video and audio with the currently selected clip.
       Only has an effect when status() is LIVES_STATUS_READY.
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
       @param cb_type must have value LIVES_CALLBACK_MODE_CHANGED.
       @param func function to be called when this signal is received.
       @param data data to be passed to callback function
       @return unsigned long callback_id
       @see LIVES_CALLBACK_MODE_CHANGED
    */
    ulong addCallback(lives_callback_t cb_type, modeChanged_callback_f func, void *data);


    /**
       Add an appQuit callback.
       @param cb_type must have value LIVES_CALLBACK_APP_QUIT
       @param func function to be called when this signal is received.
       @param data data to be passed to callback function
       @return unsigned long callback_id
       @see LIVES_CALLBACK_APP_QUIT
    */
    ulong addCallback(lives_callback_t cb_type, appQuit_callback_f func, void *data);

    /**
       Add an objectDestroyed callback.
       @param cb_type must have value LIVES_CALLBACK_OBJECT_DESTROYED
       @param func function to be called when this signal is received.
       @param data data to be passed to callback function
       @return unsigned long callback_id
       @see LIVES_CALLBACK_OBJECT_DESTROYED
    */
    ulong addCallback(lives_callback_t cb_type, objectDestroyed_callback_f func, void *data);


    /**
       Show Info dialog in the LiVES GUI.
       Only has an effect when status() is LIVES_STATUS_READY.
       @param text text to be diaplayed in the dialog.
       @param blocking if true then function will block until the user presses "OK"
       @return if blocking, returns the response code from the dialog.
    */
    lives_dialog_response_t showInfo(LiVESString text, bool blocking=true);

   /**
       Allow the user choose a file via a fileselector.
       Only has an effect when status() is LIVES_STATUS_READY, otherwise returns an empty string.
       After returning, the setting that the user selected for deinterlace may be obtained by calling 
       deinterlaceOption(). This value can the be passed into openFile().
       Chooser type will direct the user towards the type of file to choose, however there is no guarantee 
       that a file of the "correct" type will be returned. If the user cancels then an empty filename will be returned.
       @param dirname directory name to start in (or NULL)
       @param chooser_type must be either LIVES_FILE_CHOOSER_VIDEO_AUDIO or LIVES_FILE_CHOOSER_AUDIO_ONLY.
       @param title title of window to display, or NULL to use a default title.
       @return the name of the file selected.
    */
    LiVESString chooseFileWithPreview(LiVESString dirname, lives_filechooser_t chooser_type, LiVESString title=LiVESString(""));

   /**
      Open a file and return a clip for it.
      Only works when status() is LIVES_STATUS_READY, otherwise an invalid clip is returned.
      @param fname the full pathname of the file to open
      @param with_audio if true the audio will be loaded as well as the video
      @param stime the time in seconds from which to start loading
      @param frames number of frames to open (0 means all frames)
      @param deinterlace set to true to force deinterlacing
    */
    clip openFile(LiVESString fname, bool with_audio=true, double stime=0., int frames=0, bool deinterlace=false);

   /**
       Allow the user choose set to open.
       Only has an effect when status() is LIVES_STATUS_READY, and there are no currently open clips, 
       otherwise returns an empty string.
       If the user cancels, an empty string is returned.
       @return the name of the set selected.
    */
    LiVESString chooseSet();

    /**
       Change the interactivity of the GUI application.
       Interactivity is via menus and keyboard accelerators
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
       Get the current interface mode of LiVES.
       @return current mode.
    */
    lives_interface_mode_t mode();


    /**
       Get the current operational status of LiVES.
       @return current status.
    */
    lives_status_t status();


#ifndef DOXYGEN_SKIP
    // For internal use only.
    LIVES_DLL_LOCAL closureList closures();
    LIVES_DLL_LOCAL void invalidate();
#endif


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
    LiVESString currentVideoLoadDir(); ///< current video load directory.

  }


}

#endif // __cplusplus

#endif //HAS_LIBLIVES_H
