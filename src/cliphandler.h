// cliphandler.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _CLIPHANDLER_H_
#define _CLIPHANDLER_H_


#define CLIP_NAME_MAXLEN 256

#define IS_VALID_CLIP(clip) (clip >= 0 && clip <= MAX_FILES && mainw->files[clip])
#define CURRENT_CLIP_IS_VALID IS_VALID_CLIP(mainw->current_file)

#define IS_TEMP_CLIP(clip) (IS_VALID_CLIP(clip) && mainw->files[clip]->clip_type == CLIP_TYPE_TEMP)
#define CURRENT_CLIP_IS_TEMP IS_TEMP_CLIP(mainw->current_file)

#define CLIP_HAS_VIDEO(clip) (IS_VALID_CLIP(clip) ? mainw->files[clip]->frames > 0 : FALSE)
#define CURRENT_CLIP_HAS_VIDEO CLIP_HAS_VIDEO(mainw->current_file)

#define CLIP_HAS_AUDIO(clip) (IS_VALID_CLIP(clip) ? (mainw->files[clip]->achans > 0 && mainw->files[clip]->asampsize > 0) : FALSE)
#define CURRENT_CLIP_HAS_AUDIO CLIP_HAS_AUDIO(mainw->current_file)

#define CLIP_VIDEO_TIME(clip) ((double)(CLIP_HAS_VIDEO(clip) ? mainw->files[clip]->video_time : 0.))

#define CLIP_LEFT_AUDIO_TIME(clip) ((double)(CLIP_HAS_AUDIO(clip) ? mainw->files[clip]->laudio_time : 0.))

#define CLIP_RIGHT_AUDIO_TIME(clip) ((double)(CLIP_HAS_AUDIO(clip) ? (mainw->files[clip]->achans > 1 ? \
								     mainw->files[clip]->raudio_time : 0.) : 0.))

#define CLIP_AUDIO_TIME(clip) ((double)(CLIP_LEFT_AUDIO_TIME(clip) >= CLIP_RIGHT_AUDIO_TIME(clip) \
					? CLIP_LEFT_AUDIO_TIME(clip) : CLIP_RIGHT_AUDIO_TIME(clip)))

#define CLIP_TOTAL_TIME(clip) ((double)(CLIP_VIDEO_TIME(clip) > CLIP_AUDIO_TIME(clip) ? CLIP_VIDEO_TIME(clip) : \
					CLIP_AUDIO_TIME(clip)))

#define IS_NORMAL_CLIP(clip) (IS_VALID_CLIP(clip)			\
			      ? (mainw->files[clip]->clip_type == CLIP_TYPE_DISK \
				 || mainw->files[clip]->clip_type == CLIP_TYPE_FILE \
				 || mainw->files[clip]->clip_type == CLIP_TYPE_NULL_VIDEO) : FALSE)

#define CURRENT_CLIP_IS_NORMAL IS_NORMAL_CLIP(mainw->current_file)
#define CURRENT_CLIP_TOTAL_TIME CLIP_TOTAL_TIME(mainw->current_file)

#define CLIPBOARD_FILE 0

#define clipboard mainw->files[CLIPBOARD_FILE]

#define CURRENT_CLIP_IS_CLIPBOARD (mainw->current_file == CLIPBOARD_FILE)


typedef union _binval {
  uint64_t num;
  const char chars[8];
  size_t size;
} binval;

typedef enum {
  CLIP_TYPE_DISK, ///< imported video, broken into frames
  CLIP_TYPE_FILE, ///< unimported video, not or partially broken in frames
  CLIP_TYPE_GENERATOR, ///< frames from generator plugin
  CLIP_TYPE_NULL_VIDEO, ///< generates blank video frames
  CLIP_TYPE_TEMP, ///< temp type, for internal use only
  CLIP_TYPE_YUV4MPEG, ///< yuv4mpeg stream
  CLIP_TYPE_LIVES2LIVES, ///< type for LiVES to LiVES streaming
  CLIP_TYPE_VIDEODEV,  ///< frames from video device
} lives_clip_type_t;

/// corresponds to one clip in the GUI
typedef struct _lives_clip_t {
  binval binfmt_check, binfmt_version, binfmt_bytes;

  uint64_t unique_id;    ///< this and the handle can be used to uniquely id a file
  char handle[256];

  char md5sum[64];
  char type[64];

  lives_clip_type_t clip_type;
  lives_img_type_t img_type;

  // basic info (saved during backup)
  frames_t frames;  ///< number of video frames
  frames_t start, end;

  double fps;  /// framerate of the clip
  boolean ratio_fps; ///< if the fps was set by a ratio

  int hsize; ///< frame width (horizontal) in pixels (NOT macropixels !)
  int vsize; ///< frame height (vertical) in pixels

  lives_interlace_t interlace; ///< interlace type (if known - none, topfirst, bottomfirst or : see plugins.h)

  int bpp; ///< bits per pixel of the image frames, 24 or 32

  int gamma_type;

  int arps; ///< audio physical sample rate (i.e the "normal" sample rate of the clip when played at 1,0 X velocity)
  int arate; ///< current audio playback rate (varies if the clip rate is changed)
  int achans; ///< number of audio channels (0, 1 or 2)
  int asampsize; ///< audio sample size in bits (8 or 16)
  uint32_t signed_endian; ///< bitfield
  float vol; ///< relative volume level / gain; sizeof array will be equal to achans

  size_t afilesize;
  size_t f_size;

  boolean changed;
  boolean was_in_set;

  /////////////////
  char title[1024], author[1024], comment[1024], keywords[1024];
  ////////////////

  char name[CLIP_NAME_MAXLEN];  ///< the display name
  char file_name[PATH_MAX]; ///< input file
  char save_file_name[PATH_MAX];

  boolean is_untitled, orig_file_name, was_renamed;

  // various times; total time is calculated as the longest of video, laudio and raudio
  double video_time, laudio_time, raudio_time;

  double pointer_time;  ///< pointer time in timeline, + the playback start posn for clipeditor (unless playing the selection)
  double real_pointer_time;  ///< pointer time in timeline, can extend beyond video, for audio

  frames_t frameno, last_frameno;

  char mime_type[256]; ///< not important

  boolean deinterlace; ///< auto deinterlace

  int header_version;
#define LIVES_CLIP_HEADER_VERSION 104

  // uid of decoder plugin
  uint64_t decoder_uid;

  // extended info (not saved)

  //opening/restoring status
  boolean opening, opening_audio, opening_only_audio, opening_loc;
  frames_t opening_frames;
  boolean restoring;
  boolean is_loaded;  ///< should we continue loading if we come back to this clip

  frames_t progress_start, progress_end;

  ///undo
  lives_undo_t undo_action;

  frames_t undo_start, undo_end;
  frames_t insert_start, insert_end;

  char undo_text[32], redo_text[32];

  boolean undoable, redoable;

  // used for storing undo values
  int undo1_int, undo2_int, undo3_int, undo4_int;
  uint32_t undo1_uint;
  double undo1_dbl, undo2_dbl;
  boolean undo1_boolean, undo2_boolean, undo3_boolean;

  int undo_arate; ///< audio playback rate
  int undo_achans;
  int undo_asampsize;
  int undo_arps;
  uint32_t undo_signed_endian;

  int ohsize, ovsize;

  frames_t old_frames; ///< for deordering, etc.

  // used only for insert_silence, holds pre-padding length for undo
  double old_laudio_time, old_raudio_time;

  /////
  // binfmt fields may be added here:
  ///

  ////
  //// end add section ^^^^^^^

  /// binfmt is just a file dump of the struct up to the end of binfmt_end
  char binfmt_rsvd[4096];
  uint64_t binfmt_end; ///< marks the end of anything "interesring" we may want to save via binfmt extension

  /// DO NOT remove or alter any fields before this ^^^^^
  ///////////////////////////////////////////////////////////////////////////
  // fields after here can be removed or changed or added to

  boolean has_binfmt;

  /// index of frames for CLIP_TYPE_FILE
  /// >0 means corresponding frame within original clip
  /// -1 means corresponding image file (equivalent to CLIP_TYPE_DISK)
  /// size must be >= frames, MUST be contiguous in memory
  frames_t *frame_index;
  frames_t *frame_index_back; ///< for undo

  double pb_fps;  ///< current playback rate, may vary from fps, can be 0. or negative

  double img_decode_time;

  char info_file[PATH_MAX]; ///< used for asynch communication with externals

  LiVESWidget *menuentry;
  ulong menuentry_func;
  double freeze_fps; ///< pb_fps for paused / frozen clips
  boolean play_paused;

  lives_direction_t adirection; ///< audio play direction during playback, FORWARD or REVERSE.

  /// don't show preview/pause buttons on processing
  boolean nopreview;

  /// don't show the 'keep' button - e.g. for operations which resize frames
  boolean nokeep;

  // current and last played index frames for internal player
  frames_t saved_frameno;

  char staging_dir[PATH_MAX];

  pthread_mutex_t transform_mutex;

  /////////////////////////////////////////////////////////////
  // see resample.c for new events system

  // events
  resample_event *resample_events;  ///<for block resampler

  weed_plant_t *event_list;
  weed_plant_t *event_list_back;
  weed_plant_t *next_event;

  LiVESList *layout_map;
  ////////////////////////////////////////////////////////////////////////////////////////

  void *ext_src; ///< points to opaque source for non-disk types

#define LIVES_EXT_SRC_UNKNOWN -1
#define LIVES_EXT_SRC_NONE 0
#define LIVES_EXT_SRC_DECODER 1
#define LIVES_EXT_SRC_FILTER 2
#define LIVES_EXT_SRC_FIFO 3
#define LIVES_EXT_SRC_STREAM 4
#define LIVES_EXT_SRC_DEVICE 5
#define LIVES_EXT_SRC_FILE_BUFF 6

  int ext_src_type;

  int n_altsrcs;
  void **alt_srcs;
  int *alt_src_types;

  uint64_t *cache_objects; ///< for future use

  lives_proc_thread_t pumper;

#define IMG_BUFF_SIZE 262144  ///< 256 * 1024 < chunk size for reading images

  volatile off64_t aseek_pos; ///< audio seek posn. (bytes) for when we switch clips

  // decoder data

  frames_t last_vframe_played; /// experimental for player

  /// layout map for the current layout
  frames_t stored_layout_frame; ///M highest value used
  int stored_layout_idx;
  double stored_layout_audio;
  double stored_layout_fps;

  lives_subtitles_t *subt;

  boolean no_proc_sys_errors; ///< skip system error dialogs in processing
  boolean no_proc_read_errors; ///< skip read error dialogs in processing
  boolean no_proc_write_errors; ///< skip write error dialogs in processing

  boolean keep_without_preview; ///< allow keep, even when nopreview is set - TODO use only nopreview and nokeep

  lives_painter_surface_t *laudio_drawable, *raudio_drawable;

  int cb_src; ///< source clip for clipboard; for other clips, may be used to hold some temporary linkage

  boolean needs_update; ///< loaded values were incorrect, update header
  boolean needs_silent_update; ///< needs internal update, we shouldn't concern the user

  boolean checked_for_old_header, has_old_header;

  float **audio_waveform; ///< values for drawing the audio wave
  size_t *aw_sizes; ///< size of each audio_waveform in units of floats (i.e 4 bytes)

  int last_play_sequence;  ///< updated only when FINISHING playing a clip (either by switching or ending playback, better for a/vsync)

  int tcache_height; /// height for thumbnail cache (width is fixed, but if this changes, invalidate)
  frames_t tcache_dubious_from; /// set by clip alterations, frames from here onwards should be freed
  LiVESList *tcache; /// thumbnail cache, list of lives_tcache_entry_t
  boolean checked; /// clip integrity checked on load - to avoid duplicating it

  boolean hidden; // hidden in menu by clip groups

  boolean tsavedone;
} lives_clip_t;

typedef struct {
  /// list of entries in clip thumbnail cache (for multitrack timeline)
  frames_t frame;
  LiVESPixbuf *pixbuf;
} lives_tcache_entry_t;

typedef enum {
  CLIP_DETAILS_BPP,
  CLIP_DETAILS_FPS,
  CLIP_DETAILS_PB_FPS,
  CLIP_DETAILS_WIDTH,
  CLIP_DETAILS_HEIGHT,
  CLIP_DETAILS_UNIQUE_ID,
  CLIP_DETAILS_ARATE,
  CLIP_DETAILS_PB_ARATE,
  CLIP_DETAILS_ACHANS,
  CLIP_DETAILS_ASIGNED,
  CLIP_DETAILS_AENDIAN,
  CLIP_DETAILS_ASAMPS,
  CLIP_DETAILS_FRAMES,
  CLIP_DETAILS_TITLE,
  CLIP_DETAILS_AUTHOR,
  CLIP_DETAILS_COMMENT,
  CLIP_DETAILS_PB_FRAMENO,
  CLIP_DETAILS_FILENAME,
  CLIP_DETAILS_CLIPNAME,
  CLIP_DETAILS_HEADER_VERSION,
  CLIP_DETAILS_KEYWORDS,
  CLIP_DETAILS_INTERLACE,
  CLIP_DETAILS_DECODER_NAME,
  CLIP_DETAILS_GAMMA_TYPE,
  CLIP_DETAILS_MD5SUM, // for future use
  CLIP_DETAILS_CACHE_OBJECTS, // for future use
  CLIP_DETAILS_DECODER_UID,
  CLIP_DETAILS_IMG_TYPE,
  CLIP_DETAILS_RESERVED28,
  CLIP_DETAILS_RESERVED27,
  CLIP_DETAILS_RESERVED26,
  CLIP_DETAILS_RESERVED25,
  CLIP_DETAILS_RESERVED24,
  CLIP_DETAILS_RESERVED23,
  CLIP_DETAILS_RESERVED22,
  CLIP_DETAILS_RESERVED21,
  CLIP_DETAILS_RESERVED20,
  CLIP_DETAILS_RESERVED19,
  CLIP_DETAILS_RESERVED18,
  CLIP_DETAILS_RESERVED17,
  CLIP_DETAILS_RESERVED16,
  CLIP_DETAILS_RESERVED15,
  CLIP_DETAILS_RESERVED14,
  CLIP_DETAILS_RESERVED13,
  CLIP_DETAILS_RESERVED12,
  CLIP_DETAILS_RESERVED11,
  CLIP_DETAILS_RESERVED10,
  CLIP_DETAILS_RESERVED9,
  CLIP_DETAILS_RESERVED8,
  CLIP_DETAILS_RESERVED7,
  CLIP_DETAILS_RESERVED6,
  CLIP_DETAILS_RESERVED5,
  CLIP_DETAILS_RESERVED4,
  CLIP_DETAILS_RESERVED3,
  CLIP_DETAILS_RESERVED2,
  CLIP_DETAILS_RESERVED1,
  CLIP_DETAILS_RESERVED0
} lives_clip_details_t;

typedef enum {
  LIVES_MATCH_UNDEFINED = 0,
  LIVES_MATCH_NEAREST,
  LIVES_MATCH_AT_LEAST,
  LIVES_MATCH_AT_MOST,
  LIVES_MATCH_HIGHEST,
  LIVES_MATCH_LOWEST,
  LIVES_MATCH_CHOICE,
  LIVES_MATCH_SPECIFIED
} lives_size_match_t;

// TODO - these should be requirements for the transform
// changing clip object from not loaded to ready
typedef struct {
  char URI[8192];
  char save_dir[PATH_MAX];
  char fname[PATH_MAX];
  char format[256];
  char ext[16];
  int desired_width;
  int desired_height;
  float desired_fps;  // unused for now
  lives_size_match_t matchsize;
  boolean do_update;
  boolean allownf;
  boolean debug;
  // returned values
  double duration;
  char vidchoice[512];
  char audchoice[512];
  // TODO: add audio bitrate ?, audio_lang, get_sub, sub_format, sub_language, etc.
} lives_remote_clip_request_t;

#define LIVES_LITERAL_EVENT "event"
#define LIVES_LITERAL_FRAMES "frames"

int find_clip_by_uid(uint64_t uid);

char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp);

boolean get_clip_value(int which, lives_clip_details_t, void *retval, size_t maxlen);
boolean save_clip_value(int which, lives_clip_details_t, void *val);
boolean save_clip_values(int which);

void dump_clip_binfmt(int which);
boolean restore_clip_binfmt(int which);
lives_clip_t *clip_forensic(int which, char *binfmtname);

size_t reget_afilesize(int fileno);
off_t reget_afilesize_inner(int fileno);

boolean read_file_details(const char *file_name, boolean only_check_for_audio, boolean open_image);

boolean update_clips_version(int which);

int save_event_frames(int clipno);
void clear_event_frames(int clipno);

// query function //
boolean ignore_clip(int which);

void make_cleanable(int clipno, boolean isit);

void remove_old_headers(int which);
boolean write_headers(int which);
boolean read_headers(int which, const char *dir, const char *file_name);

char *get_clip_dir(int which);

void permit_close(int which);

void migrate_from_staging(int which);

/// intents ////

// aliases for object states
#define CLIP_STATE_NOT_LOADED 	OBJECT_STATE_NULL
#define CLIP_STATE_READY	OBJECT_STATE_NORMAL

// txparams
#define CLIP_PARAM_STAGING_DIR "staging_dir"

lives_intentparams_t *get_txparams_for_clip(int which, lives_intention intent);

#endif
