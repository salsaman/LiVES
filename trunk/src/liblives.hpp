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
  LIVES_STATUS_NOTREADY, ///< application is starting up; not ready
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

  LIVES_CALLBACK_MODE_CHANGED = 4096, ///< sent when interface mode changes

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
   Default character encoding
*/
#define LIVES_CHAR_ENCODING_DEFAULT LIVES_CHAR_ENCODING_UTF8

/**
   Dialog response values
*/
typedef enum {
// positive values for custom responses
  LIVES_DIALOG_RESPONSE_INVALID=-1, ///< INVALID response
  LIVES_DIALOG_RESPONSE_NONE=0, ///< Response not obtained
  LIVES_DIALOG_RESPONSE_OK, ///< OK button clicked
  LIVES_DIALOG_RESPONSE_RETRY, ///< Retry button clicked
  LIVES_DIALOG_RESPONSE_ABORT, ///< Abort button clicked
  LIVES_DIALOG_RESPONSE_RESET, ///< Reset button clicked
  LIVES_DIALOG_RESPONSE_SHOW_DETAILS, ///< Show details button clicked
  LIVES_DIALOG_RESPONSE_CANCEL, ///< Cancel button clicked
  LIVES_DIALOG_RESPONSE_ACCEPT, ///< Accept button clicked
  LIVES_DIALOG_RESPONSE_YES, ///< Yes button clicked
  LIVES_DIALOG_RESPONSE_NO ///< No button clicked
} lives_dialog_response_t;


/**
   Audio sources
*/
typedef enum {
  LIVES_AUDIO_SOURCE_INTERNAL, ///< Audio source is internal to LiVES
  LIVES_AUDIO_SOURCE_EXTERNAL ///< Audio source is external to LiVES
} lives_audio_source_t;


/**
   Audio players
*/
typedef enum {
  LIVES_AUDIO_PLAYER_PULSE, ///< Audio playback is through PulseAudio
  LIVES_AUDIO_PLAYER_JACK, ///< Audio playback is thorugh Jack
  LIVES_AUDIO_PLAYER_SOX, ///< Audio playback is through Sox
  LIVES_AUDIO_PLAYER_MPLAYER, ///< Audio playback is through mplayer
  LIVES_AUDIO_PLAYER_MPLAYER2 ///< Audio playback is through mplayer2
} lives_audio_player_t;



/**
   Multitrack insert modes
*/
typedef enum {
  LIVES_INSERT_MODE_NORMAL
} lives_insert_mode_t;



/**
   Multitrack gravity
*/
typedef enum {
  LIVES_GRAVITY_NONE, ///< no gravity
  LIVES_GRAVITY_LEFT, ///< inserted blocks gravitate to the left
  LIVES_GRAVITY_RIGHT ///< inserted blocks gravitate to the right
} lives_gravity_t;



/**
   Player looping modes (bitmap)
*/
typedef enum {
  LIVES_LOOP_MODE_NONE=0, ///< no looping
  LIVES_LOOP_MODE_CONTINUOUS=1, ///< both video and audio loop continuously
  LIVES_LOOP_MODE_FIT_AUDIO=2 ///< video keeps looping until audio playback finishes
} lives_loop_mode_t;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus

#include <vector>
#include <list>
#include <map>

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
  typedef class effectKey effectKey;

  /**
     typedef
  */
  typedef class effectKeyMap effectKeyMap;

  /**
     typedef
  */
  typedef class effect effect;


  /**
     typedef
  */
  typedef class player player;


  /**
     typedef
  */
  typedef class multitrack multitrack;


  /**
     typedef
  */
  typedef class block block;


  /**
     typedef
  */
  typedef class livesString livesString;


  typedef list<livesString> livesStringList;


  ///////////////////////////////////////////////////


  /**
     class "livesString". 
     A subclass of std::string which automatically handles various character encodings.
  */
  class livesString : public std::string {
  public:
    livesString(const string& str="", lives_char_encoding_t e=LIVES_CHAR_ENCODING_DEFAULT) : m_encoding(e), std::string(str) {}
    livesString(const string& str, size_t pos, size_t len = npos, lives_char_encoding_t e=LIVES_CHAR_ENCODING_DEFAULT) : m_encoding(e),
      std::string(str, pos, len) {}
    livesString(const char* s, lives_char_encoding_t e=LIVES_CHAR_ENCODING_DEFAULT) : m_encoding(e), std::string(s) {}
    livesString(const char* s, size_t n, lives_char_encoding_t e=LIVES_CHAR_ENCODING_DEFAULT) : m_encoding(e), std::string(s, n) {}
    livesString(size_t n, char c, lives_char_encoding_t e=LIVES_CHAR_ENCODING_DEFAULT) : m_encoding(e), std::string(n, c) {}
    template <class InputIterator>
    livesString  (InputIterator first, InputIterator last, 
		  lives_char_encoding_t e=LIVES_CHAR_ENCODING_DEFAULT) : m_encoding(e), std::string(first, last) {}

    /**
       Change the character encoding of the string.
       @param enc the character encoding to convert to.
       @return either the same string if no conversion is needed, or a new string if conversion is needed
    */
    livesString toEncoding(lives_char_encoding_t enc);

    /**
       Define the character encoding of the string.
       @param enc the character encoding the string is in.
    */
    void setEncoding(lives_char_encoding_t enc);

    /**
       Return the encoding that the string was declared as.
       @return the character encoding the string is in.
    */
    lives_char_encoding_t encoding();

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
     class "livesApp". 
     Represents a single LiVES application. Note that currently only one such instance can be valid at a time, 
     attempting to create a second concurrent instance will return an invalid instance.
  */
  class livesApp {
    friend set;
    friend clip;
    friend effectKeyMap;
    friend effectKey;
    friend player;
    friend multitrack;
    friend block;

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
       Deletes any callbacks which were set for this instance.
    */
    ~livesApp();

    /**
       Returns whether the instance is valid or not.
       A valid instance is connected to a running LiVES application.
       @return true if instance is connected to a running LiVES application.
    */
    bool isValid();

    /**
       Equivalent to status() == LIVES_STATUS_READY.
       @return true if status() == LIVES_STATUS_READY.
       @see status().
    */
    bool isReady();

    /**
       Equivalent to status() == LIVES_STATUS_PLAYING.
       @return true if status() == LIVES_STATUS_PLAYING.
       @see status().
    */
    bool isPlaying();

    /**
       Returns the current set
       @return the current set.
    */
    const set& getSet();

    /**
       Returns the current effectKeyMap
       @return the current effectKeyMap.
    */
    const effectKeyMap& getEffectKeyMap();

    /**
       Returns the player for this livesApp.
       @return the player for this livesApp.
    */
    const player& getPlayer();

    /**
       Returns the multitrack object for this livesApp.
       @return the multitrack object for this livesApp.
    */
    const multitrack& getMultitrack();

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
       @see removeCallback().
    */
    ulong addCallback(lives_callback_t cb_type, modeChanged_callback_f func, void *data);

    /**
       Add an appQuit callback.
       @param cb_type must have value LIVES_CALLBACK_APP_QUIT
       @param func function to be called when this signal is received.
       @param data data to be passed to callback function
       @return unsigned long callback_id
       @see LIVES_CALLBACK_APP_QUIT
       @see removeCallback().
    */
    ulong addCallback(lives_callback_t cb_type, appQuit_callback_f func, void *data);

    /**
       Add an objectDestroyed callback.
       @param cb_type must have value LIVES_CALLBACK_OBJECT_DESTROYED
       @param func function to be called when this signal is received.
       @param data data to be passed to callback function
       @return unsigned long callback_id
       @see LIVES_CALLBACK_OBJECT_DESTROYED
       @see removeCallback().
    */
    ulong addCallback(lives_callback_t cb_type, objectDestroyed_callback_f func, void *data);

    /**
       Show Info dialog in the LiVES GUI.
       Only has an effect when status() is LIVES_STATUS_READY.
       @param text text to be diaplayed in the dialog.
       @param blocking if true then function will block until the user presses "OK"
       @return if blocking, returns the response code from the dialog.
    */
    lives_dialog_response_t showInfo(livesString text, bool blocking=true);

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
       @see openFile().
    */
    livesString chooseFileWithPreview(livesString dirname, lives_filechooser_t chooser_type, livesString title=livesString(""));

   /**
      Open a file and return a clip for it.
      Only works when status() is LIVES_STATUS_READY, otherwise an invalid clip is returned.
      If the file pointed to cannot be opened as a clip, an invalid clip is returned.
      If interactive() is true, the user may cancel the load, or choose to load only part of the file.
      @param fname the full pathname of the file to open
      @param with_audio if true the audio will be loaded as well as the video
      @param stime the time in seconds from which to start loading
      @param frames number of frames to open (0 means all frames)
      @param deinterlace set to true to force deinterlacing
      @return a clip.
      @see chooseFileWithPreview().
      @see deinterlaceOption().
    */
    clip openFile(livesString fname, bool with_audio=true, double stime=0., int frames=0, bool deinterlace=false);

    /**
       Returns a list of available sets. The list returned depends on the setting of prefs::tmpDir().
       This may be an expensive operation as it requires accessing the underlying filesystem.
       If the set is invalid, an empty list is returned.
       @return a list<livesString> of set names.
       @see reloadSet().
       @see set::save().
    */
    livesStringList availableSets();

   /**
       Allow the user to choose a set to open.
       Only has an effect when status() is LIVES_STATUS_READY, and there are no currently open clips, 
       otherwise returns an empty string.
       If the user cancels, an empty string is returned.
       The valid list of sets to choose from will be equivalent to the list returned by availableSets().
       @return the name of the set selected.
       @see reloadSet().
       @see availableSets().
    */
    livesString chooseSet();

   /**
      Reload an existing clip set.
      Only works when status() is LIVES_STATUS_READY, otherwise false is returned.
      A set may not be accessed concurrently by more than one copy of LiVES.
      The valid list of sets is equivalent to the list returned by availableSets().
      @param setname the name of the set to reload.
      @see chooseSet().
      @see availableSets().
    */
    bool reloadSet(livesString setname);

    /**
       Change the interactivity of the GUI application.
       Interactivity is via menus and keyboard accelerators
       @param setting set to true to allow interaction with the GUI.
       @return the new setting.
       @see interactive().
    */
    bool setInteractive(bool setting);

    /**
       Returns whether the GUI app is in interactive mode.
       @return true if GUI interactivity via menus and keyboard accelerators is enabled.
       @see setInteractive().
    */
    bool interactive();

    /**
       Returns last setting of deinterlace by user.
       @return value that the user selected during the last filechooser with preview operation.
       This value may be passed into openFile().
       @see openFile().
    */
    bool deinterlaceOption();

    /**
       Get the current interface mode of the livesApp.
       If the livesApp is invalid, returns LIVES_INTERFACE_MODE_INVALID.
       @return current mode.
       @see setMode().
    */
    lives_interface_mode_t mode();

    /**
       Set the current interface mode of the livesApp.
       Only works if status() is LIVES_STATUS_READY.
       If the livesApp is invalid, returns LIVES_INTERFACE_MODE_INVALID.
       @return the new mode.
       @see mode().
    */
    lives_interface_mode_t setMode(lives_interface_mode_t mode);//, livesMultitrackSettings settings=NULL);

    /**
       Get the current operational status of the livesApp.
       @return current status.
       @see isReady().
       @see isPlaying().
    */
    lives_status_t status();


    /**
       If status() is LIVES_STATUS_PROCESSING, cancel the current processing if possible.
       @return true if the processing was cancelled.
    */
    bool cancel();



#ifndef DOXYGEN_SKIP
    // For internal use only.
    closureList& closures();
    void invalidate();
    void setClosures(closureList cl);
    
    bool setPref(int prefidx, bool val);
    bool setPref(int prefidx, int val);
#endif

  protected:
    ulong addCallback(lives_callback_t cb_type, private_callback_f func, void *data);

  private:
    ulong m_id;
    closureList m_closures;
    set * m_set;
    player *m_player;
    effectKeyMap * m_effectKeyMap;
    multitrack *m_multitrack;

    bool m_deinterlace;

    ulong appendClosure(lives_callback_t cb_type, callback_f func, void *data);
    void init(int argc, char *argv[]);

    void operator=(livesApp const&); // Don't implement


  };




  /**
     class "clip". 
     Represents a clip which is open in LiVES.
     @see set::nthClip()
     @see livesApp::openFile()
     @see player::foregroundClip()
     @see player::backgroundClip()
  */
  class clip {
    friend livesApp;
    friend set;
    friend block;
    friend multitrack;
    friend player;

  public:

    /**
       Creates a new, invalid clip
    */
    clip();

    /**
       Check if clip is valid. 
       A clip is valid if it is loaded in a valid livesApp instance, and the livesApp::status() is not LIVES_STATUS_NOTREADY. 
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
    double FPS();

    /**
       Framerate (frames per second) that the clip is/will be played back at.
       This may vary from the normal FPS(). During playback it will be equivalent to player::FPS().
       If livesApp::mode() is LIVES_INTERFACE_MODE_MULTITRACK then this will return multitrack::FPS().
       IF the clip is invalid, 0. is returned.
       @return the playback framerate
       @see player::setCurrentFPS().
       @see player::FPS().
    */
    double playbackFPS();

    /**
       Human readable name of the clip.
       If clip is not valid then empty string is returned.
       @return livesString name, or empty string if clip is not valid.
    */
    livesString name();

    /**
       Audio rate for this clip. 
       If the clip is video only, 0 is returned.
       If clip is not valid then -1 is returned.
       Note this is not necessarily the same as the soundcard audio rate which can be obtained via prefs::audioPlayerRate().
       @return int audio rate, or -1 if clip is not valid.
       @see playbackAudioRate()
    */
    int audioRate();

    /**
       The current playback audio rate for this clip, which may differ from audioRate(). 
       If the clip is video only, 0 is returned.
       If clip is not valid then -1 is returned.
       Note this is not necessarily the same as the soundcard audio rate which can be obtained via prefs::audioPlayerRate().
       If livesApp::mode() is LIVES_INTERFACE_MODE_MULTITRACK then this will return multitrack::audioRate().
       @return int playback audio rate, or -1 if clip is not valid.
       @see audiorate().
    */
    int playbackAudioRate();

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
       Select all frames in the clip. if the clip is invalid does nothing.
       Only works is livesApp::status() is LIVES_STATUS_READY or LIVES_STATUS_PLAYING.
       @return true if the operation was successful.
    */
    bool selectAll();

    /**
       Set the selection start frame for the clip. If the new start is > selectionEnd() then selection end will be set to the new start.
       If the clip is invalid there is no effect.
       Only functions if livesApp::status() is LIVES_STATUS_READY or LIVES_STATUS_PLAYING.
       @param start the selection start frame which must be in range 1 <= start <= frames().
       @see setSelectionEnd().
    */
    bool setSelectionStart(unsigned int start);

    /**
       Set the selection end frame for the clip. If the new end is < selectionStart() then selection start will be set to the new end.
       If the clip is invalid there is no effect.
       Only functions if livesApp::status() is LIVES_STATUS_READY or LIVES_STATUS_PLAYING.
       @param end the selection end frame which must be in range 1 <= end <= frames().
       @see setSelectionStart().
    */
    bool setSelectionEnd(unsigned int end);

    /**
       Switch to this clip as the current foreground clip.
       Only works if livesApp::status() is LIVES_STATUS_READY or LIVES_STATUS_PLAYING and livesApp::mode() is LIVES_INTERFACE_MODE_CLIP_EDITOR.
       If clips are switched during playback, the application acts as if livesApp::loopMode() were set to LIVES_LOOP_MODE_CONTINUOUS.
       If the clip is invalid, nothing happens and false is returned.
       @return true if the switch was successful.
       @see player::setForegroundClip()
    */
    bool switchTo();

    /**
       Switch to this clip as the current background clip.
       Only works if livesApp::status() is LIVES_STATUS_READY or LIVES_STATUS_PLAYING and livesApp::mode() is LIVES_INTERFACE_MODE_CLIP_EDITOR.
       If the clip is invalid, nothing happens and false is returned.
       @return true if the switch was successful.
       @see player::setBackgroundClip()
    */
    bool setIsBackground();

    /**
       @return true if the two clips have the same internal id, and belong to the same livesApp.
    */
    inline bool operator==(const clip& other) {
      return other.m_uid == m_uid && m_lives == other.m_lives;
    }

  protected:
    clip(ulong uid, livesApp *lives=NULL);
    ulong m_uid;

  private:
    livesApp *m_lives;

  };
  


#ifndef DOXYGEN_SKIP
  typedef vector<ulong> clipList;
  typedef vector<ulong>::iterator clipListIterator;
#endif

  ///// set ////////


  /**
     class "set". 
     Represents a list of clips and/or layouts which are open in LiVES. May be obtained from livesApp::getSet().
     @see livesApp::getSet()
  */
  class set {
    friend livesApp;

  public:

    /**
       Returns whether the set is valid or not.
       The set is valid if belongs to a valid livesApp, and the livesApp::status() is not LIVES_STATUS_NOTREADY.
       @return true if the set is valid (associated with a valid livesApp instance).
    */
    bool isValid() const;

    /**
       Returns the current name of the set. 
       If it has not been defined, an empty string is returned. If the set is invalid, an empty string is returned.
       @return livesString name.
    */
    livesString name() const;

    /**
       Save the set, and close all open clips and layouts. 
       If the set name is empty, the user can choose the name via the GUI. 
       If the name is defined, and it points to a different, existing set, the set will not be saved and false will be returned, 
       unless force_append is set to true, in which case the current clips and layouts will be appended to the other set.
       Saving a set with a new name is an expensive operation as it requires moving files in the underlying filesystem.
       @param name name to save set as, or empty string to let the user choose a name.
       @param force_append set to true to force appending to another existing set.
       @return true if the set was saved.
    */
    bool save(livesString name, bool force_append=false) const;

    /**
       Returns the number of clips in the set. If the set is invalid, returns 0.
       @return number of clips.
       @see indexOf().
       @see nthClip().
    */
    unsigned int numClips() const;

    /**
       Returns the nth clip in the set. If n  >= numClips(), returns an invalid clip. If the set is invalid, returns an invalid clip.
       @return the nth clip in the set.
       @see indexOf().
       @see numClips().
    */
    clip nthClip(unsigned int n) const;

    /**
       Returns the index of a clip in the currentSet. If the clip is not in the current set, then -1 is returned.
       If the set is invalid or the clip is invalid, returns -1.
       @return the index of the clip in the set.
       @see nthClip().
       @see numClips().
    */
    int indexOf(clip c) const;

    /**
       Returns a list of layout names for this set. If the set is invalid, returns an empty list.
       @return a list of layout names for this set.
       @see livesApp::reloadLayout().
    */
    livesStringList layoutNames(unsigned int n) const;

    /**
       @return true if the two sets belong to the same livesApp.
    */
    inline bool operator==(const set& other) const {
      return other.m_lives == m_lives;
    }


  protected:
    set(livesApp *lives=NULL);
    void setName(const char *setname);

  private:
    livesApp *m_lives;
    clipList m_clips;

    void update_clip_list(void);


  };





  /**
     class "player". 
     Represents a media player associated with a livesApp.
     @see livesApp::getPlayer()
  */
  class player {
    friend livesApp;

  public:

    /**
       Returns whether the player is valid or not.
       A valid player belongs to a valid livesApp, and the livesApp::status() is not LIVES_STATUS_NOTREADY.
       @return true if the player is valid.
    */
    bool isValid() const;

    /**
       Returns true if the livesApp::status() is LIVES_STATUS_PLAYING.
    */
    bool isPlaying() const;

    /**
       Set playback in a detached window.
       If the livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT and prefs::sepWinSticky() is true, the window appears straight away;
       otherwise it appears only when isPlaying() is true.
       @see setFS().
       @see sepWin().
    */
    void setSepWin(bool setting) const;

    /**
       @return true if playback is in a separate window from the main GUI.
       @see setSepWin().
    */
    bool sepWin() const;

    /**
       Set playback fullscreen. Use of setFS() is recommended instead.
       @see setFS().
       @see fullScreen().
    */
    void setFullScreen(bool setting) const;

    /**
       @return true if playback is full screen.
       @see setFullScreen().
    */
    bool fullScreen() const;

    /**
       Combines the functionality of setSepWin() and setFullScreen().
       If the livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT and prefs::sepWinSticky() is true, the window appears straight away,
       but it will only fill the screen when isPlaying() is true; otherwise it appears only when isPlaying() is true.
       @see setSepWin()
       @see setFullScreen()
    */
    void setFS(bool setting) const;

    /**
       Commence playback of video and audio with the currently selected clip.
       Only has an effect when livesApp::status() is LIVES_STATUS_READY.
       @return true if playback was started.
    */
    bool play() const;

    /**
       Stop playback.
       If livesApp::status() is not LIVES_STATUS_PLAYING, nothing happens.
       @return true if playback was stopped.
    */
    bool stop() const;

    /**
       Set the foreground clip for the player. Equivalent to clip::switchTo() except that it only functions when 
       livesApp::status() is LIVES_STATUS_PLAYING and the livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT.
       If the clip is invalid, or isActive() is false, nothing happens and false is returned.
       @param c the clip to set as foreground.
       @return true if the function was successful.
       @see foregroundClip().
       @see setBackgroundClip().
    */
    bool setForegroundClip(clip c) const;

    /**
       Returns the current foreground clip of the player. If isActive() is false, returns an invalid clip.
       If the livesApp::mode() is not LIVES_INTERFACE_MODE_CLIPEDIT, returns an invalid clip.
       @return the current foreground clip.
       @see setForegroundClip().
       @see backgroundClip().
    */
    clip foregroundClip() const;

    /**
       Set the background clip for the player. Equivalent to clip::setIsBackground() except that it only functions when 
       livesApp::status() is LIVES_STATUS_PLAYING and the livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT.
       Only works if there is one or more transition effects active.
       If the clip is invalid, or isActive() is false, nothing happens and false is returned.
       @return true if the function was successful.
       @see backgroundClip().
       @see setForegroundClip().
    */
    bool setBackgroundClip(clip c) const;

    /**
       Returns the current background clip of the player. If isActive() is false, returns an invalid clip.
       If the livesApp::mode() is not LIVES_INTERFACE_MODE_CLIPEDIT, returns an invalid clip.
       @return the current background clip.
       @see setBackgroundClip().
       @see foregroundClip().
    */
    clip backgroundClip() const;

    /**
       Set the current playback start time in seconds (this is also the insertion point in multitrack mode).
       Only works if the livesApp::status() is LIVES_STATUS_READY. If livesApp::mode() is LIVES_INTERFACE_MODE_CLIP_EDITOR, 
       the start time may not be set beyond the end of the current clip (video and audio). 
       The outcome of setting playback beyond the end of video but not of audio and vice-versa depends on the value of loopMode().
       If livesApp::mode() is LIVES_INTERFACE_MODE_MULITRACK, setting the current time may cause the timeline to stretch visually 
       (i.e zoom out). 
       The miminum value is 0.0 in every mode. Values < 0. will be ignored.
       If isValid() is false, nothing happens and 0. is returned.
       @param time the time in seconds to set playback start time to.
       @returns the new playback start time.
       @see playbackTime().
       @see setAudioPlaybackTime().
       @see multitrack::setCurrentTime().
    */
    double setPlaybackTime(double time) const;

    /**
       Return the current clip playback time in seconds. If livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT, then this returns 
       the current playback time for video in the current foreground clip.
       If livesApp::mode() is LIVES_INTERFACE_MODE_MULTITRACK, then this returns the current player time in the multitrack timeline 
       (equivalent to elapsedTime()).
       This function works in livesApp::status() LIVE_STATUS_READY and LIVES_STATUS_PLAYING.
       If isValid() is false, 0. is returned.
       @returns the current clip playback time.
       @see setPlaybackTime().
       @see audioPlaybackTime().
       @see elapsedTime().
       @see multitrack::currentTime().
    */
    double playbackTime() const;
    

    //    double setAudioPlaybackTime(double time) const;
       
    /**
       Return the current clip audio playback time in seconds. If livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT, then this returns 
       the current playback time for audio in the current foreground clip.
       If livesApp::mode() is LIVES_INTERFACE_MODE_MULTITRACK, then this returns the current player time in the multitrack timeline 
       (equivalent to elapsedTime()).
       This function works with livesApp::status() of LIVE_STATUS_READY and LIVES_STATUS_PLAYING.
       If isValid() is false, 0. is returned.
       @returns the current clip audio playback time.
       @see playbackTime().
       @see elapsedTime().
       @see multitrack::currentTime().
    */
    double audioPlaybackTime() const;

    /**
       Return the elapsed time, i.e. total time in seconds since playback began.
       If livesApp::status() is not LIVES_STATUS_PLAYING, 0. is returned.
       @return the time in seconds since playback began.
       @see playbackTime().
       @see audioPlaybackTime().
       @see multitrack::currentTime().
    */
    double elapsedTime() const;

    /**
       Set the current playback framerate in frames per second. Only works if livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT and
       livesApp::status() is LIVES_STATUS_PLAYING.
       Allowed values range from -prefs::maxFPS to +prefs::maxFPS.
       If prefs::audioFollowsVideoChanges() is true, then the audio playback rate will change proportionally.
       If isPlaying() is false, nothing happens and 0. is returned.
       Note, the setting only applies to the current clip; if the clip being played is switched then currentFPS() may change.
       @param fps the framerate to set
       @return the new framerate
       @see currentFPS().
     */
    double setCurrentFPS(double fps) const;

    /**
       Return the current playback framerate in frames per second of the player.
       If isValid() is false, returns 0. If livesApp::status is neither LIVES_STATUS_READY nor LIVES_STATUS_PLAYING, returns 0.
       Otherwise, this is equivalent to foregroundClip::playbackFPS().
       @return the current or potential playback rate in frames per second.
       @see setCurrentFPS().
       @see clip::playbackFPS().
    */
    double currentFPS() const;

    /**
       Return the current audio rate of the player.
       If isValid() is false, returns 0. If livesApp::status is neither LIVES_STATUS_READY nor LIVES_STATUS_PLAYING, returns 0.
       Otherwise, this is equivalent to foregroundClip::playbackAudioRate().
       Note this is not necessarily the same as the soundcard audio rate which can be obtained via prefs::audioPlayerRate().
       @return the current or potential audio rate in Hz.
       @see clip::playbackAudioRate().
    */
    int currentAudioRate() const;

    /**
       Set the loop mode for the player. The value is a bitmap, however LIVES_LOOP_MODE_FIT_AUDIO 
       only has meaning when livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT.
       If isValid() is false, nothing happens.
       @param the desired loop mode
       @return the new loop mode
       @see loopMode().
    */
    lives_loop_mode_t setLoopMode(lives_loop_mode_t loopMode) const;

    /**
       Return the loop mode of the player.
       If isValid() is false, returns LIVES_LOOP_MODE_NONE.
       @return the current loop mode of the player
       @see setLoopMode().
    */
    lives_loop_mode_t loopMode() const;

    /**
       Set ping pong mode. If pingPong is true then rather than looping forward, video and audio will "bounce" 
       forwards and backwards off their end points, provided the mode() is LIVES_INTERFACE_MODE_CLIPEDIT.
       If mode() is LIVES_INTERFACE_MODE_MULTITRACK, then the value is ignored.
       @param the desired value
       @return the new value
       @see pingPong().
       @see loopMode().
    */
    bool setPingPong(bool pingPong) const;

    /**
       Return ping pong mode. If pingPong is true then rather than looping forward, video and audio will "bounce" 
       forwards and backwards off their end points, provided the mode() is LIVES_INTERFACE_MODE_CLIPEDIT.
       If mode() is LIVES_INTERFACE_MODE_MULTITRACK, then the value is ignored.
       If the player is invalid, false is returned.
       @return the current value.
       @see setPingPong().
       @see loopMode().
    */
    bool pingPong() const;

    /**
       Resets the clip::playbackFPS() for the current foreground clip, so it is equal to the clip::FPS().
       Resets the clip::playbackAudioRate so it is equal to the clip::audioRate().
       If possible equalizes the audioPlaybackTime() with the playbackTime() so video and audio are in sync.
       Only works if isPlaying() is true.
       @return true if the method succeeded.
    */
    bool resyncFPS() const;


    /**
       @return true if the two players belong to the same livesApp.
    */
    inline bool operator==(const player& other) const {
      return other.m_lives == m_lives;
    }


  protected:
    player(livesApp *lives=NULL);

  private:
    livesApp *m_lives;

  };

  /////////////////////////////////////////////////////

  /**
     class "effectKey". 
     Represents a single effect key slot. A valid livesApp will have a map of these (effectKeyMapping()) whose size() is equal to 
     prefs::rteKeysVirtual().
     @see effectKeyMap::operator[]
  */
  class effectKey {
    friend effectKeyMap;
  public:
    /**
       Creates a new, invalid effect key
    */
    effectKey();

    /**
       Returns whether the effectKey is valid or not.
       An effect key is valid if it is owned by a valid livesApp, and the livesApp::status() is not LIVES_STATUS_NOTREADY, 
       and the key value is in the range 1 <= key <= prefs::rteKeysVirtual.
       @return true if the effectKey is valid.
    */
    bool isValid();

    /**
       Return the (physical or virtual) key associated with this effectKey.
       Effects (apart from generators) are applied in ascending key order.
       Physical keys (1 - 9) can also be toggled from the keyboard, provided livesApp::interactive() is true.
       If the effectKey is invalid, 0 is returned.
       @return the physical or virtual key associated with this effectKey.
    */
    int key();

    /**
       Return the number of modes for this effectKey slot. Modes run from 0 to numModes() - 1.
       Effects can be mapped to modes in ascending order, but only one mode is the active mode for the effectKey.
       If the effectKey is invalid, 0 will be returned.
       @return the number of modes for this effectKey.
    */
    int numModes();

    /**
       Return the number of mapped modes for this effectKey slot. Modes run from 0 to numModes() - 1.
       When numMappedModes() == numModes() for an effectKey, no more effects may be mapped to it until a mapping is erased.
       If the effectKey is invalid, 0 will be returned.
       @return the number of modes for this effectKey.
    */
    int numMappedModes();

    /**
       Set the current mode this effectKey.
       Only works if the effecKey is valid, a valid effect is mapped to the mode, and livesApp::status() is 
       LIVES_STATUS_PLAYING or LIVES_STATUS_READY.
       @param mode the mode to switch to.
       @return the new mode of the effectKey.
       @see currentMode().
       @see numMappedModes().
    */
    int setCurrentMode(int mode);

    /**
       Get the current mode for this effectKey.
       If the effectKey is invalid, the current mode is -1.
       @return the current mode of the effectKey.
       @see setCurrentMode().
    */
    int currentMode();

    /**
       Enable an effect mapped to this effectKey, mode().
       Only works if the effecKey is valid, a valid effect is mapped to the mode, and livesApp::status() is 
       LIVES_STATUS_PLAYING or LIVES_STATUS_READY.
       @return the new state of the effectKey
       @see enabled().
    */
    bool setEnabled(bool setting);

    /**
       Return a value to indicate whether the effect mapped to this effectKey, mode() is active. 
       If the effectKey is invalid, returns false.
       @return true if the effect mapped at mode() is enabled.
       @see setEnabled().
    */
    bool enabled();

    /**
       Map an effect to the next unused mode for the effectKey.
       Will only work if the livesApp::status() is LIVES_STATUS_PLAYING or LIVES_STATUS_READY.
       The effectKey and the effect must share the same owner livesApp, and both must be valid.
       @return the mode number the effect was mapped to, or -1 if the mapping failed.
       @see removeMapping().
    */
    int appendMapping(effect fx);

    /**
       Remove an effect from being mapped to this key.
       Will only work if the livesApp::status() is LIVES_STATUS_PLAYING or LIVES_STATUS_READY.
       If the effectKey is invalid, or if no effect is mapped to the mode, false is returned and nothing happens.
       If an effect is removed, effects mapped to higher mode numbers on the same effectKey will move down a mode to close the gap.
       Note: the currentMode() does not change unless this is the last mapped effect for this effectKey, 
       so this may cause the currently enabled effect to change.
       @param mode the mode to remove the effect from.
       @return true if an effect was unmapped.
       @see appendMapping().
    */
    bool removeMapping(int mode);

    /**
       Returns the effect mapped to the key at the specified mode.
       If the effectKey is invalid, or if no effect is mapped to the mode, an invalid effect is returned.
       @param mode the specified mode.
       @return the effect mapped to the key at the specified mode.
    */
    effect at(int mode);

    /**
       @return true if the two effectKeys have the same livesApp and key value and belong to the same livesApp
    */
    inline bool operator==(const effectKey& other) {
      return other.m_key == m_key && m_lives == other.m_lives;
    }

  protected:
    effectKey(livesApp *lives, int key);

  private:
    int m_key;
    livesApp *m_lives;

  };



  /**
     class "effectKeyMap". 
     Represents a mapping of effectKey instances to key slots. Real time effects are always applied in order of ascending index value 
     (with the exception of generator effects, which are applied first).
     @see livesApp::getEffectKeyMap()
  */
    class effectKeyMap {
      friend livesApp;
    public:
      /**
	 Returns whether the effectKeyMap is valid or not.
	 A valid effectKeyMap is owned by a valid livesApp, the livesApp::status() is not LIVES_STATUS_NOTREADY, 
	 and the index is 1 <= i <= prefs::rteKeysVirtual(). 
	 @return true if the effectKeyMap is valid.
      */
      bool isValid() const;

      /**
	 Unmap all effects from effectKey mappings, leaving an empty map.
	 Only has an effect when livesApp::status() is LIVES_STATUS_READY.
	 @return true if all effects were unmapped
      */
      bool clear() const;

      /**
	 Returns the ith effect key for this key map.
	 Valid range for i is 1 <= i <= prefs::rteKeysVirtual().
	 For values of i outside this range, an invalid effectKey is returned.
	 @return an effectKey with index i.
	 @see effectKey::operator[]
      */
      effectKey at(int i) const;

      /**
	 Returns the number of key slots (indices) in the effectKeyMap.
	 The valid range of keys is 1 <= key <= size().
	 Equivalent to prefs::rteKeysVirtual(), except that if the effectKeyMap is invalid, returns 0.
	 @return the number of key slots.
	 @see prefs::rteKeysVirtual()
      */
      size_t size() const;

      /**
	 @return true if the two effectKeyMaps have the same livesApp
      */
      inline bool operator==(const effectKeyMap& other) const {
	return other.m_lives == m_lives;
      }

      /**
	 Returns an effect key with index i for this key map. The value of i may be chosen freely, but if it is outside the range 
	 1 <= i <= size() then the effectKey will be considered invalid.
	 @return an effectKey with index i for this key map.
	 @see at()
      */
      inline effectKey operator [] (int i) const {
	return effectKey(m_lives, i);
      }


    protected:
      effectKeyMap(livesApp *lives);

    private:
      livesApp *m_lives;
    };



  /**
     class "effect". 
     Represents a single effect.
  */
  class effect {
    friend effectKey;
  public:
    effect(livesApp& lives, livesString hashname); // TODO


    /**
       Create a new effect from a template. In case of multiple matches, only the first match is returned. 
       In the case of no matches, an invalid effect is returned.
       @param lives a livesApp instance
       @param package a package name (e.g. "frei0r", "LADSPA"), or "" to match any package.
       @param fxname the name of an effect (e.g. "chroma blend") or "" to match any effect name.
       @param author the name of the principle author of the effect (e.g. "jsmith") or "" to match any author.
       @param version the number of a version to match, or 0 to match any version.
       @return an effect.
    */
    effect(livesApp& lives, const char *package, const char *fxname, const char *author="", int version=0);

    /**
       Returns whether the effect is valid or not.
       A valid effect is owned by a valid livesApp, whose livesApp::status() is not LIVES_STATUS_NOTREADY, and which references an existing effect plugin.
       @return true if the effect is valid.
    */
    bool isValid();      

    /**
       @return true if the two effects have the same index and the same livesApp owner
    */
    inline bool operator==(const effect& other) {
      return other.m_idx == m_idx && m_lives == other.m_lives;
    }


  protected:
    effect(livesApp *lives=NULL, int idx=-1);
    livesApp *m_lives;
    int m_idx;

  private:

  };



  /**
     class "block". 
     Represents a single block of frames which forms part of a layout. This is an abstracted level, since layouts are fundamentally formed of "events" which may span multiple blocks.
  */
  class block {
    friend multitrack;

  public:

    /**
       returns whether the block is valid or not. A block may become invalid if it is deleted from a layout for example. 
       Undoing a deletion does not cause a block to become valid again, you need to search for it again by time and track number.
       If the livesApp::mode() is changed from LIVES_INTERFACE_MODE_MULTITRACK, then all existing blocks become invalid.
       @return whether the block is contained in an active multitrack instance.
    */
    bool isValid();

    /**
       Returns a reference to the block on the specified track at the specified time. If no such block exists, returns an invalid block.
       @param track the track number. Values < 0 indicate backing audio tracks.
       @param time the time in seconds on the timeline.
       @return the block on the given track at the given time.
    */
    block(multitrack m, int track, double time);

    /**
       Returns the start time in seconds of the block.
       If the block is invalid, returns -1.
       @return start time in seconds.
    */
    double startTime();
    
    /**
       Returns the duration in seconds of the block.
       If the block is invalid, returns -1.
       @return duration in seconds.
    */
    double length();

    /**
       Returns the clip which is the source of frames for this block.
       If the block is invalid, returns an invalid clip.
       @return the clip which is the source of frames for this block.
    */
    clip clipSource();

    /**
       Returns the track number to which this block is currently attached.
       A track number < 0 indicates a backing audio track.
       If the block is invalid, returns 0.
       @return the current track number for this block.
    */
    int track();


    /**
       Removes a block from the multitrack timeline. Upon being removed, the block becomes invalid.
       Undoing the operation does not cause the block to become valid again, although it can be searched for using the constructor.
       If the block is invalid, nothing happens and false is returned.
       Only works if livesApp::status() is LIVES_STATUS_READY.
       May cause other blocks to move, depending on the setting of multitrack::gravity().
       @returns true if the block was removed.
       @see multitrack::insertBlock().
    */
    bool remove();


    /**
       Move the block to a new track at a new timeline time.
       Depending on the value of multitrack::insertMode(), it may not be possible to do the insertion. 
       In case of failure an invalid block is returned.
       If the current track is a backing audio track, then only audio is inserted; 
       in this case if without_audio is true an invalid block is returned.
       Only works if livesApp::status() is LIVES_STATUS_READY and isActive() is true.
       Note: the actual place where the block ends up, and its final size depends on various factors such as the gravity() setting, 
       the insertMode() setting, and the location of other blocks in the layout.
       The insertion may cause other blocks to relocate.
       If the block is invalid, nothing happens and false is returned.
       @param track the new track to move to.
       @param time the timeline time in seconds to move to.
       @return true if the block could be moved.
    */
    bool moveTo(int track, double time);



  protected:
    block(multitrack *m=NULL, ulong uid=0l);


  private:
    ulong m_uid;
    livesApp *m_lives;

    void invalidate();
  };



  /**
     class "multitrack". 
     Represents the multitrack object in a livesApp.
  */
  class multitrack {
    friend livesApp;
    friend block;

  public:

    /**
       returns whether the multitrack is valid or not. A valid multitrack is one which is owned by a valid livesApp, 
       whose livesApp::status() is not LIVES_STATUS_NOTREADY.
       @return whether the multitrack is valid
    */
    bool isValid() const;

    /**
       returns whether the multitrack is active or not. This is equivent to livesApp::mode() == LIVES_INTERFACE_MODE_MULTITRACK.
       @return whether the multitrack is active or not.
    */
    bool isActive() const;

    /**
       Set the current track if isActive() is true.
       Only works when livesApp::status() is LIVES_STATUS_READY or LIVES_STATUS_PLAYING.
       @param track a value >= 0 represents a video track, a value < 0 represents a backing audio track.
       @return true if the track setting was successful.
       @see currentTrack().
    */
    bool setCurrentTrack(int track) const;

    /**
       If isActive() is true, then this method returns the current active track.
       The active track defines the insertion point for video and audio, along with the currentTime().
       If isActive() is false, or the livesApp::status is not LIVES_STATUS_READY or LIVES_STATUS_PLAYING 
       then the return value is undefined.
       @return the current active track in multitrack mode. A value >= 0 represents a video track, a value < 0 represents a backing audio track.
       @see setCurrentTrack().
    */
    int currentTrack() const;

    /**
       Set the current playback start time in seconds. This is also the insertion point for insertBlock().
       Only works if the livesApp::status() is LIVES_STATUS_READY and isActive() is true.
       Setting the current time may cause the timeline to stretch visually (i.e zoom out). 
       The miminum value is 0.0; values < 0.0 will be ignored.
       This function is synonymous with player::setPlaybackTime().
       @param time the time in seconds to set playback start time to.
       @returns the new playback start time.
       @see currentTime().
    */
    double setCurrentTime(double time) const;

    /**
       Return the current playback time in seconds. If isActive() is true this returns the current player time in the multitrack timeline 
       (equivalent to to player::playbackTime(), and during playback, equivalent to player::elapsedTime() plus a constant offset).
       This function works when livesApp::status() is LIVE_STATUS_READY or LIVES_STATUS_PLAYING.
       If isActive() is false, -1. is returned.
       @returns the current clip playback time.
       @see setCurrentTime().
       @see currentAudioTime().
       @see elapsedTime().
    */
    double currentTime() const;

    /**
       If isActive() is true, then this method returns the label for a track.
       @param track the track number. A value >= 0 represents a video track, a value < 0 represents a backing audio track.
       @return the track label, or empty string if the specified track does not exist.
    */
    livesString trackLabel(int track) const;


    bool setTrackLabel(int track) const;


    lives_gravity_t gravity() const;


    lives_gravity_t setGravity(lives_gravity_t grav) const;


    int numVideoTracks() const;


    int numAudioTracks() const;


    /**
       Return the framerate of the multitrack in frames per second.
       If isActive() is false, returns 0. Otherwise when the livesAPP::status() is LIVES_STATUS_PLAYING, player::FPS() takes this value.
       @return the framerate of the multitrack in frames per second.
    */
    double FPS() const;

    /**
       Insert frames from clip c into currentTrack() at currentTime()
       If ignore_selection_limits is true, then all frames from the clip will be inserted, 
       otherwise (the default) only frames from clip::selectionStart() to clip::selectionEnd() will be used.
       If without_audio is false (the default), audio is also inserted.
       Frames are automatically resampled to fit layout::fps().
       Depending on the insertMode(), it may not be possible to do the insertion. In case of failure an invalid block is returned.
       If the current track is a backing audio track, then only audio is inserted; 
       in this case if without_audio is true an invalid block is returned.
       Only works if livesApp::status() is LIVES_STATUS_READY and isActive() is true.
       Note: the actual place where the block ends up, and its final size depends on various factors such as the gravity() setting, 
       the insertMode() setting, and the location of other blocks in the layout.
       The insertion may cause other blocks to relocate.
       @param c the clip to insert from
       @param ignore_selection_limits if true then all frames from the clip will be inserted
       @param without_audio if false then audio is also inserted
       @return the newly inserted block.
       @see setCurrentTrack().
       @see setCurrentTime().
       @see clip::setSelectionStart().
       @see clip::setSelectionEnd().
       @see setGravity().
       @see block::remove().
    */
    block insertBlock(clip c, bool ignore_selection_limits=false, bool without_audio=false) const;


    /**
       Wipe the current layout, leaving a blank layout.
       If force is false, then the user will have a chance to cancel (if livesApp::interactive() is true), 
       or to save the layout.
       Only works if livesApp::status() is LIVES_STATUS_READY and isActive() is true.
       Otherwise, the layout will not be wiped, and an empty string will be returned.
       @param force set to true to force the layout to be wiped.
       @return the name which the layout was saved to, or empty string if it was not saved.
    */
    livesString wipeLayout(bool force=false) const;

    livesString chooseLayout() const;

    bool reloadLayout(livesString filename) const;

    bool saveLayout(livesString name) const;

    clip render(bool render_audio=true) const;


    bool setAutoTransition(effect autotrans) const;


    bool setAutoTransitionEnabled(bool setting) const;


    /**
       @return true if the two layouts have the same livesApp owner
    */
    inline bool operator==(const multitrack& other) const {
      return m_lives == other.m_lives;
    }


  protected:
    multitrack(livesApp *lives=NULL);
    livesApp *m_lives;


  };






  /**
     Preferences.
  */
  namespace prefs {
    livesString currentVideoLoadDir(livesApp lives); ///< current video load directory.
    ///< @param lives a reference to a livesApp instance

    livesString currentAudioDir(livesApp lives); ///< current audio directory for loading and saving audio.
    ///< @param lives a reference to a livesApp instance

    livesString tmpDir(livesApp lives); ///< Despite the name, this is the working directory for the LiVES application. 
    ///< The valid list of sets is drawn from this directory, for it is here that they are saved and loaded.
    ///< The value can only be set at runtime through the GUI preferences window. Otherwise you can override the default value 
    ///< when the livesApp() is created via argv[] option "-tmpdir", eg: <BR>
    ///< <BR><BLOCKQUOTE><I>
    ///<    char *argv[2]; <BR>
    ///<    argv[0]="-tmpdir"; <BR>
    ///<    argv[1]="/home/user/tempdir/"; <BR>
    ///<    livesApp lives(2, argv); <BR>
    ///< </I></BLOCKQUOTE>
    ///< @param lives a reference to a livesApp instance
    lives_audio_source_t audioSource(livesApp lives); ///< the current audio source

    bool setAudioSource(livesApp lives, lives_audio_source_t asrc); ///< Set the audio source. Only works if livesApp::status() is LIVES_STATUS_READY.
    ///< @param lives a reference to a livesApp instance
    ///< @param asrc the desired audio source
    ///< @return true if the audio source could be changed.

    lives_audio_player_t audioPlayer(livesApp lives); ///< the current audio player
    ///< @param lives a reference to a livesApp instance

    int audioPlayerRate(livesApp lives);

    int rteKeysVirtual(livesApp lives); ///< maximum value for effectKey indices
    ///< @param lives a reference to a livesApp instance

    double maxFPS(livesApp lives);

    bool audioFollowsVideoChanges(livesApp lives);

    bool setAudioFollowsVideoChanges(livesApp lives, bool setting);

    bool sepWinSticky(livesApp lives);

    bool setSepWinSticky(livesApp lives, bool setting);


  }


}

#endif // __cplusplus

#endif //HAS_LIBLIVES_H
