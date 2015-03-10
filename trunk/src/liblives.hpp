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
   Multitrack gravity
*/
typedef enum {
  LIVES_GRAVITY_NONE,
  LIVES_GRAVITY_LEFT,
  LIVES_GRAVITY_RIGHT
} lives_gravity_t;



////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus

#include <vector>
#include <list>
#include <map>

#include <tr1/memory>

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
    livesString(lives_char_encoding_t e=LIVES_CHAR_ENCODING_DEFAULT) : m_encoding(e), std::string() {}
    livesString(const string& str) : std::string(str) {}
    livesString(const string& str, size_t pos, size_t len = npos) : std::string(str, pos,  len) {}
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


    livesStringList availableSets();


   /**
       Allow the user choose set to open.
       Only has an effect when status() is LIVES_STATUS_READY, and there are no currently open clips, 
       otherwise returns an empty string.
       If the user cancels, an empty string is returned.
       The valid list of sets depends on the setting of prefs::tmpDir().
       @return the name of the set selected.
       @see reloadSet().
    */
    livesString chooseSet();

   /**
      Reload an existing clip set.
      Only works when status() is LIVES_STATUS_READY, otherwise false is returned.
      A set may not be accessed concurrently by more than one copy of LiVES.
      The valid list of sets depends on the setting of prefs::tmpDir().
      @param setname the name of the set to reload.
      @see chooseSet().
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
  */
  class clip {
    friend livesApp;
    friend set;
    friend block;
    friend multitrack;

  public:

    /**
       Creates a new, invalid clip
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
       @return livesString name, or empty string if clip is not valid.
    */
    livesString name();

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


    void selectAll();


    bool setSelectionStart(unsigned int start);


    bool setSelectionEnd(unsigned int end);

    /**
       Switch to this clip as the current foreground clip.
       Only works if status() is LIVES_STATUS_READY or LIVES_STATUS_PLAYING and mode() is LIVES_INTERFACE_MODE_CLIP_EDITOR.
       @return true if the switch was successful.
       @see player::setForegroundClip()
    */
    bool switchTo();

    /**
       Switch to this clip as the current background clip.
       Only works if status() is LIVES_STATUS_READY or LIVES_STATUS_PLAYING and mode() is LIVES_INTERFACE_MODE_CLIP_EDITOR.
       @return true if the switch was successful.
       @see player::setBackgroundClip()
    */
    bool setIsBackground();

    /**
       @return true if the two clips have the same unique_id, and belong to the same livesApp.
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
    set();

    set(livesApp *lives);
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
       Returns whether the set is valid or not.
       @return true if the set is valid (associated with a valid livesApp instance).
    */
    bool isValid() const;

    /**
       Set playback in a detached window.
       @see setFS().
       @see sepWin().
    */
    void setSepWin(bool setting) const;

    /**
       Set playback fullscreen.
       @see setFS().
       @see fullscreen().
    */
    void setFullScreen(bool setting) const;


    bool sepWin() const;
    bool fullScreen() const;


    /**
       Combines the functionality of setSepWin() and setFullScreen().
       @see setSepWin()
       @see setFullScreen()
    */
    void setFS(bool setting) const;

    /**
       Commence playback of video and audio with the currently selected clip.
       Only has an effect when status() is LIVES_STATUS_READY.
       @return true if playback was started.
    */
    bool play() const;

    /**
       Stop playback.
       If status() is not LIVES_STATUS_PLAYING, nothing happens.
       @return true if playback was stopped.
    */
    bool stop() const;


    bool setForegroundClip() const; // TODO

    bool setBackgroundClip() const; // TODO

    /**
       Set the current playback start time in seconds (this is also the insertion point in multitrack mode).
       Only works if the livesApp::status() is LIVES_STATUS_READY. If livesApp::mode() is LIVES_INTERFACE_MODE_CLIP_EDITOR, 
       the start time may not be set beyond the end of the current clip (video and audio). 
       If livesApp::mode() is LIVES_INTERFACE_MODE_MULITRACK, setting the current time may cause the timeline to stretch visually 
       (i.e zoom out). 
       The miminum value is 0.0 in every mode. Values < 0. will be ignored.
       @param time the time in seconds to set playback start time to.
       @returns the new playback start time.
       @see currentTime().
    */
    double setPlaybackTime(double time) const;

    /**
       Return the current clip playback time in seconds. If livesApp::mode() is LIVES_INTERFACE_MODE_CLIPEDIT, then this returns 
       the current playback time for video in the current foreground clip.
       If livesApp::mode() is LIVES_INTERFACE_MODE_MULTITRACK, then this returns the current player time in the multitrack timeline 
       (equivalent to elapsedTime()).
       This function works in livesApp::status() LIVE_STATUS_READY and LIVES_STATUS_PLAYING.
       @returns the current clip playback time.
       @see setCurrentTime().
       @see currentAudioTime().
       @see elapsedTime().
    */
    double playbackTime() const;


    double setAudioPlaybackTime(double time) const;


    double audioPlaybackTime() const;


    double elapsedTime() const;


    double setCurrentFps(double fps) const;


    double currentFps() const;




    /**
       @return true if the two players belong to the same livesApp.
    */
    inline bool operator==(const player& other) const {
      return other.m_lives == m_lives;
    }


  protected:
    player();
    player(livesApp *lives);

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
       @return true if the effectKey is valid (associated with a valid livesApp instance).
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
       @see mode().
       @see numMappedModes().
    */
    int setMode(int mode);

    /**
       Get the current mode for this effectKey.
       If the effectKey is invalid, the current mode is -1.
       @return the current mode of the effectKey.
       @see setMode().
    */
    int mode();

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
       @see prefs::rteKeysVirtual()
    */
    int appendMapping(effect fx);


    bool removeMapping(int mode);



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
	 @return true if the effectKeyMap is associated with a valid livesApp instance,
	 and the index is 1 <= i <= prefs::rteKeysVirtual().
      */
      bool isValid() const;

      /**
	 Unmap all effects from effectKey mappings, leaving an empty map.
	 Only has an effect when status() is LIVES_STATUS_READY.
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
    effect();
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
       @return whether the block is contained in the current layout.
    */
    bool isValid();

    /**
       Returns the block on the specified track at the specified time. If no such block exists, returns an invalid block.
       @param track the track number. Values < 0 indicate backing audio tracks.
       @param time the time in seconds on the timeline.
       @return the block on the given track at the given time.
    */
    block(int track, double time);

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
       @return the current track number for this clip.
    */
    int track();



    bool remove();



    bool moveTo(int track, double time);



  protected:
    block(ulong uid);


  private:
    ulong m_uid;

  };



  /**
     class "multitrack". 
     Represents the multitrack object in a livesApp.
  */
  class multitrack {
    friend livesApp;

  public:

    /**
       returns whether the multitrack is valid or not. A valid multitrack is one which is owned by a valid livesApp.
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
       (equivalent to to player::playbackTime(), and during playback, equivalent to player::elapsedTime()).
       This function works when livesApp::status() is LIVE_STATUS_READY or LIVES_STATUS_PLAYING.
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
       Insert frames from clip c into currentTrack() at currentTime()
       If ignore_selection_limits is true, then all frames from the clip will be inserted, 
       otherwise (the default) only frames from clip::selectionStart() to clip::selectionEnd() will be used.
       If without_audio is false (the default), audio is also inserted.
       Frames are automatically resampled to fit layout::fps().
       Depending on the insertion mode, it may not be possible to do the insertion. In case of failure an invalid block is returned.
       If the current track is a backing audio track, then only audio is inserted; 
       in this case if without_audio is true an invalid block is returned.
       Only works if livesApp::status() is LIVES_STATUS_READY and isActive() is true.
       Note: the actual place where the block ends up depends on various factors such as the gravity() setting 
       and the location of other blocks in the layout.
       @param c the clip to insert from
       @param ignore_selection_limits if true then all frames from the clip will be inserted
       @param without_audio if false then audio is also inserted
       @return the newly inserted block.
       @see setCurrentTrack().
       @see setCurrentTime().
       @see clip::setSelectionStart().
       @see clip::setSelectionEnd().
       @see setGravity().
    */
    block insertBlock(clip c, bool ignore_selection_limits=false, bool without_audio=false) const;


    /**
       Wipe the current layout, leaving a blank layout.
       If force is false, then the user will have a chance to cancel (if livesApp::interactive() is true), 
       or to save the layout.
       If isActive() is false, the layout will not be wiped, and an empty string will be returned.
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
    //layout();
    multitrack(livesApp *lives);

  private:
    livesApp *m_lives;

  };






  /**
     Preferences.
  */
  namespace prefs {
    livesString currentVideoLoadDir(livesApp &lives); ///< current video load directory.
    ///< @param lives a reference to a livesApp instance

    livesString currentAudioDir(livesApp &lives); ///< current audio directory for loading and saving audio.
    ///< @param lives a reference to a livesApp instance

    livesString tmpDir(livesApp &lives); ///< Despite the name, this is the working directory for the LiVES application. 
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
    lives_audio_source_t audioSource(livesApp &lives); ///< the current audio source

    bool setAudioSource(livesApp &lives, lives_audio_source_t asrc); ///< Set the audio source. Only works if status() is LIVES_STATUS_READY.
    ///< @param lives a reference to a livesApp instance
    ///< @param asrc the desired audio source
    ///< @return true if the audio source could be changed.

    lives_audio_player_t audioPlayer(livesApp &lives); ///< the current audio player
    ///< @param lives a reference to a livesApp instance

    int rteKeysVirtual(livesApp &lives); ///< maximum value for effectKey indices
    ///< @param lives a reference to a livesApp instance

  }


}

#endif // __cplusplus

#endif //HAS_LIBLIVES_H
