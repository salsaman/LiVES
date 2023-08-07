// cliphandler.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef GET_BASE_DEFFNS

#ifndef _CLIPHANDLER_H_

#define CLIPBOARD_FILE 0
#define clipboard mainw->files[CLIPBOARD_FILE]
#define CURRENT_CLIP_IS_CLIPBOARD (mainw->current_file == CLIPBOARD_FILE)

#define CLIP_NAME_MAXLEN 256

#define IS_VALID_CLIP(clip) (mainw && (clip) >= 0 && (clip) <= MAX_FILES && mainw->files[(clip)])
#define CURRENT_CLIP_IS_VALID (mainw && IS_VALID_CLIP(mainw->current_file))

#define RETURN_VALID_CLIP(clip) (IS_VALID_CLIP((clip)) ? mainw->files[(clip)] : NULL)

#define IS_TEMP_CLIP(clip) (IS_VALID_CLIP((clip)) && mainw->files[(clip)]->clip_type == CLIP_TYPE_TEMP)
#define CURRENT_CLIP_IS_TEMP (mainw && IS_TEMP_CLIP(mainw->current_file))

#define CLIP_HAS_VIDEO(clip) (IS_VALID_CLIP((clip)) ? mainw->files[(clip)]->frames > 0 : FALSE)
#define CURRENT_CLIP_HAS_VIDEO (mainw && CLIP_HAS_VIDEO(mainw->current_file))

#define CLIP_HAS_AUDIO(clip) (IS_VALID_CLIP((clip)) ? (mainw->files[(clip)]->achans > 0 \
						       && mainw->files[(clip)]->asampsize > 0) : FALSE)
#define CURRENT_CLIP_HAS_AUDIO (mainw && CLIP_HAS_AUDIO(mainw->current_file))

#define CLIP_VIDEO_TIME(clip) (CLIP_HAS_VIDEO((clip)) ? mainw->files[(clip)]->video_time : 0.)

#define CLIP_LEFT_AUDIO_TIME(clip) (CLIP_HAS_AUDIO((clip)) ? mainw->files[(clip)]->laudio_time : 0.)

#define CLIP_RIGHT_AUDIO_TIME(clip) (CLIP_HAS_AUDIO((clip)) ? (mainw->files[(clip)]->achans > 1 ? \
							       mainw->files[(clip)]->raudio_time : 0.) : 0.)

#define CLIP_AUDIO_TIME(clip) (CLIP_LEFT_AUDIO_TIME((clip)) >= CLIP_RIGHT_AUDIO_TIME((clip)) \
				? CLIP_LEFT_AUDIO_TIME((clip)) : CLIP_RIGHT_AUDIO_TIME((clip)))

#define CLIP_TOTAL_TIME(clip) (CLIP_VIDEO_TIME((clip)) > CLIP_AUDIO_TIME((clip)) ? CLIP_VIDEO_TIME((clip)) : \
			       CLIP_AUDIO_TIME((clip)))

#define IS_PHYSICAL_CLIP(clip) (IS_VALID_CLIP((clip))			\
				? (mainw->files[(clip)]->clip_type == CLIP_TYPE_DISK \
				   || mainw->files[(clip)]->clip_type == CLIP_TYPE_FILE) : FALSE)

#define CURRENT_CLIP_IS_PHYSICAL (mainw && IS_PHYSICAL_CLIP(mainw->current_file))

#define RETURN_PHYSICAL_CLIP(clip) (IS_PHYSICAL_CLIP((clip)) ? mainw->files[(clip)] : NULL)

#define IS_NORMAL_CLIP(clip) (IS_VALID_CLIP((clip))			\
			      ? (IS_PHYSICAL_CLIP((clip))		\
				 || mainw->files[(clip)]->clip_type == CLIP_TYPE_NULL_VIDEO) : FALSE)

#define CURRENT_CLIP_IS_NORMAL (mainw && IS_NORMAL_CLIP(mainw->current_file))

#define RETURN_NORMAL_CLIP(clip) (IS_NORMAL_CLIP((clip)) ? mainw->files[(clip)] : NULL)

#define CURRENT_CLIP_TOTAL_TIME (mainw ? CLIP_TOTAL_TIME(mainw->current_file) : 0.)

#define IS_ASCRAP_CLIP(which) (mainw->ascrap_file == which && IS_VALID_CLIP(which) \
			       && (!get_primary_src(mainw->ascrap_file) \
				   || (get_primary_src(mainw->ascrap_file))->src_type != LIVES_SRC_TYPE_RECORDER))

#define CLIPSWITCH_BLOCKED (!mainw || mainw->go_away || LIVES_MODE_MT || !CURRENT_CLIP_IS_VALID || mainw->preview \
			    || (LIVES_IS_PLAYING && mainw->event_list && !(mainw->record || mainw->record_paused)) \
			    || (mainw->is_processing && cfile->is_loaded) || !mainw->cliplist \
			    || !LIVES_IS_INTERACTIVE || is_transport_locked())

typedef union _binval {
  uint64_t num;
  const char chars[8];
  size_t size;
} binval;

#endif
#endif

#ifndef HAVE_CLIP_T
#define HAVE_CLIP_T

#define IMG_BUFF_SIZE 262144  ///< 256 * 1024 < chunk size for reading images

typedef enum {
  CLIP_TYPE_UNDEFINED,
  CLIP_TYPE_DISK, ///< imported video, broken into frames
  CLIP_TYPE_FILE, ///< unimported video, not or partially broken in frames
  CLIP_TYPE_GENERATOR, ///< frames from generator plugin
  CLIP_TYPE_NULL_VIDEO, ///< generates blank video frames
  CLIP_TYPE_TEMP, ///< temp type, for internal use only
  CLIP_TYPE_YUV4MPEG, ///< yuv4mpeg stream
  CLIP_TYPE_LIVES2LIVES, ///< type for LiVES to LiVES streaming
  CLIP_TYPE_VIDEODEV,  ///< frames from video device
} lives_clip_type_t;

// src types (for clips)

// no source
#define LIVES_SRC_TYPE_UNDEFINED	0

// static source types (actor == NULL)
#define LIVES_SRC_TYPE_IMAGE		1
#define LIVES_SRC_TYPE_BLANK		2

// non static source types

// frame source is a video clip
#define LIVES_SRC_TYPE_DECODER		64

// frame src is a filter instance (generator)
#define LIVES_SRC_TYPE_GENERATOR	65

// source pixel_data is a r/w memory buffer
#define LIVES_SRC_TYPE_MEMBUFF		66

// frame src is a fifo file
#define LIVES_SRC_TYPE_FIFO		67

// frame src is a socket / network connection
#define LIVES_SRC_TYPE_STREAM		68

// frame source is a hardware device (e.g webcam)
#define LIVES_SRC_TYPE_DEVICE		69

// frame src is a file buffer containing multiple frames
// e.g a dump or raw frame data (scrap file)
#define LIVES_SRC_TYPE_FILE_BUFF	70

// grabbed frames, eg from the desktop recorder
// may be similaer to
#define LIVES_SRC_TYPE_RECORDER		71

// frame src is internal (e.g test pattern, nullvideo)
#define LIVES_SRC_TYPE_INTERNAL		256

// layer has no source
#define SRC_STATUS_NOT_SET		0

// source is on standby / idle
#define SRC_STATUS_IDLE    		1

// ready to fetch data
#define SRC_STATUS_READY    		2

// fethcing data / busy
#define SRC_STATUS_RUNNING    		3

// target is out of date
#define SRC_STATUS_EXPIRED		32

// source cannot read any data
#define SRC_STATUS_UNAVAILABLE		64
// bad metadata supplied
#define SRC_STATUS_EINVAL		65
// source can retry getting data
#define SRC_STATUS_EAGAIN		66

// error encountered while loading
#define SRC_STATUS_ERROR		512
// source provider has been removed
#define SRC_STATUS_DELETED		513
// source cannot operate correctly, do not use
#define SRC_STATUS_BROKEN		514

//

#define SRC_PURPOSE_ANY		     	-1

// primary source for the clip, the default
#define SRC_PURPOSE_PRIMARY		0
// clone source for multitracks
#define SRC_PURPOSE_TRACK		1
//
#define SRC_PURPOSE_TRACK_OR_PRIMARY	2

// clone source for pre caching frames, can be switched with primary / track
#define SRC_PURPOSE_PRECACHE		8
// source used for creating thumbnail images
#define SRC_PURPOSE_THUMBNAIL		9

// for srcs used in nodemodel
#define SRC_PURPOSE_MODEL		128

// src is required to provide some frames for the clip, and is in the primary srcgroup
// is hould only be freed when the clip is closed, or under special circumstance - e.g
// to reload the src and clear and error condition
#define SRC_FLAG_NOFREE			(1ull < 0)

// src can only load one layer pixel_data at a time, otherwise multiple layers can be filled in parallel
#define SRC_FLAG_SINGLE			(1ull < 4)

// src can be slow to fill the pixel_data, running the action_func async is recommended
#define SRC_FLAG_ASYNC			(1ull < 8)

typedef lives_result_t (*lives_clipsrc_func_t)(weed_layer_t *layer);

// clip_srcs are obejcts (plugins or functions) which take a layer with size / palette
// frame number defined, and fill the empty pixel_data for the layer
// the clip_src may be internal (eg. blank frame source) where the pixel_data is filled without any external source
// or may be external, in this case data is read from an extern source, decoded, and the placed in pixel_data
//
// clips may have multiple srcs which each map to a set of frame numbers
// (mapping will be done through extension to frame_index - TODO)
//
// the default set (group) of clip_srcs for a clip are known collectively as the primary source
// it is possible to create clones of the primary source and have duplicate source sets
// these can be used to allow parallel access to the sources - examples are thumbnail creators
// duplicate track players, precaching sources
//
// during playback we have a stack of "layers", each layer represents the target of a clip_src
// the layers are orered by "track" index, thus we can bind one or more clip_srcs to each track
// and these clip_srcs will fill the layers in the stack
//
// this means for example, a single clip can be connected to multiple tracks, in this case the clip_srcs would be cloned
// each track getting its own copy. We can also have cloned clip_srcs not associated with any track (for example
// the thumbnail srcs), the precache srcs are special clones becsue they can swap plaes with the primary src
//
// clip_src types are designed to be extendible, all that is necessary is to provide an action_function which
// takes a a layer as input - the layer will have width, height, palettes, clip and idx (frame) number set
// the clip_src simply needs to fill the pixel data im the layer. Some clip_srcs can prodice frames at any size
// or palette, others may have fixed sizes, and one or a set of palttes. Tnhey can also advise if they use linear gamma
// or produce premultiplied alpha. Clip_srcs can also be generator plugins (generating pixel_data in memeory).
// or can connect to fifo files, streams, devices...
//
// we have static clip_srcs - these are shared between all clips (e.g. blank_frame clip_src)
// we also have dynamic clip_srcs which are specific to a single clip (e.g decoder type sources)
// the most common mixture of clip_srcs is combining a decoder src and the static img_decoder src
// cloning a dynamic src creates a copy of the src, cloning a static src just returns the static src
// it is possible to define new clip_srcs by providing an action_func which takes a layer as input and
// fills in the pixel_data.

// clip_srcs can be sync or async. In the case of async, the layer STATUS is used
// to track the layer through queued, loading, lodaded and ready statuses.
// the frame_index for a clip (TODO) maps each frame to a clip_src, and an index value for the src.
// clip_srcs also have a STATUS, most commonly this can be INACTIVE / ACTIVE
//
// Some clip srcs are able to fill layer_pixel data in parallel (e.g blank frame src)
// others can only be used to fill a single layer at a time (e.g decoder srcs)
// to use single srcs in parallel, it is necessary to create a clone.

// when a clip is saved we need to also save the frame_index and the metadata for each clip_src,
// so that when the clip is reloaded, we can recreate the clip srcs
// to aid this we have fields:
//  src_uid (e.g for a decoder src, this would point to a decoder plugin uid)
//  src_type: eg. decoder
//  ext_id - a fingerprint for the ext media (eg. md5sum)
//  ext_URI - the location where the ext_src can be found
//
typedef union {
  char md5sum[MD5_SIZE];
  char cert[64];
} fingerprint_t;


// a clip_source (clipsrc) is some object which can fill the pixel_data for a layer
// a clip can have multiple clipsrcs, and these are arranged into sourc groups (clipsrc_groups)
// almost all clips have a PRIMARY srcgrp, whcih has sufficeint clipsrcs in it to fill pixel data for all
// frame from 1 - frames


// clip srcs have 1 mandatory function - action_funcy(layer, boolean async)
// which should take the layer, and fill its pixel data, adjusitng layer_status
// it should attempt to adhere to the values set in the layer fo the following:
// - width (in macropixels of the target palette), height, palette, (and YUV_clamping, YUV_sampling, YUV-subtype
// if these are set and the palette is YUV) as well as gamma_type
// the source will set the layer leaves to the actual values
// if async is set, the action_function should run in athread, and layer status can be monitored by the caller
// otherwise the function should block untill all the pixel_data is loaded and converted
// the action_func can be a wrapper which reads the detail from the layer, passes them as parameter
// in case of error, NULL pixel_Data is retrun and layer status set to error or invalid
typedef lives_result_t (*clipsrc_action_func_t)(lives_layer_t *, boolean async);

// some clip_srcs ar re entrant / threadsafe, that is, action _Function can be called as many times as required
// in parallel
// if this is not the case, the src must provide a function for creating a copy (clone) of an existing "actor"
typedef void *(*clipsrc_clone_func_t)(void *actor, const char *URI);
// normally, actor would be NULL, and URI would be set. To create a clone, URI is NULL and actor is set
// to create a clone from a sanpshot, actor will be set with snapshot adn URI set with the URI for
// which the snapshot was created

// other functions will exst to int / deinit the src factory

// and a further function to return a snapshot of the current src state,
// to make it possible to reload the src in a future session

typedef struct {
  // unique id for this clip src in srcgrp - clones get differetn values
  uint64_t uid;

  // unique id for this clip src in srcgrp - clones get same value
  uint64_t class_uid;

  ////////////////////////////// template part
  int src_type;

  // for plugins (eg. decodder pllugins, this would be the plugin uid)
  // for filters (e.g. generators, the filter plugin uid)
  uint64_t actor_uid;

  //clipsrc_init_func_t create_func;  // (ext_URI) - return instance of actor

  // if this is non NULL defined, then we need only create a layer with appropriate values
  // size, palette, frame
  // then call action_func(layer);
  clipsrc_action_func_t action_func; // layer - fill layer pixel_data

  //clipsrc_clone_func_t clone_func;  // actor  - create new instance with same URL, actor_UID
  //clipsrc_snapshot_func_t snapshot_func; // create a snapshot of strate for faster reloading
  //clipsrc_destroy_func_t destroy_func; // actor - frees actor

  uint64_t flags;

  //////////////////////////////////

  // pointer to a plugin for this clip_src (for staic srcs, this will be NULL)
  void *actor;

  // if actor is a template, points to the specific inst for the scrgroup
  void *actor_inst;

  // cpal and gaama will be converted first to srcgroup appatent_pal, apparent_gamma

  // current palette in use
  int cpal;

  // current gamm_type for the src
  int gamma_type;

  // external checksum for clip source (e.g md4sum, cert)
  fingerprint_t ext_checksum;

  // URI of external source
  char *ext_URI;

  void *priv; // private data for the source
} lives_clip_src_t;

// we will have an array of these, representing all static clipsrcs
// when creating a clipsrc, we check src_type
// we look this up to see if it is a static clipsrc. If so we create the clipsrc. setting
// details as contained in the template for that src_type
// all fields are copied from template to clipsrc
// the clone
typedef struct {
  int src_type;
  uint64_t actor_uid;
  clipsrc_action_func_t action_func; // layer - fill layer pixel_data
  clipsrc_clone_func_t clone_func;  // actor  - create new instance with same URL, actor_UID
  uint64_t flags;
} lives_clipsrc_template;

// clip_srcs are contained in clipsrc_groups. Clips  can have one or more clipsrc groups
// the default group is the PRIMARY group, but we can ausiliar groups
// each group has an apparent pelette - the group acts like a single entity
// which returns frames 1 -> frames at size hsize X vsize,
// with palette apparent_pal
// within the group there may be any number of clip_srcs - these link to a frame number via the
// frame_index -> idx of clip_src in srcs_list
//
// the primary group is the one used for playback
// each group can only pull a single frame at a time
// to pull a frame, the status must be READY
// then we can point to layer to a lyer to be filled, and set a frame number
// then call load_frema(src_group)
// the frame index will be consulted, and we find which source in the group is responsible for that frame
// and the index of the frame for that src
// now we need to monitor the status until it gets to ready and then the layer can be used
//
// since a group can only fill one layer at a time we can create audiliar groups (for example for thumbnailing)
// we can call clone_group(src_group, purpose) and get a new group. Static clip_srcs in the group
// are copied by referennce, non stastic are cloned.
//
// the width, height, gamma_type and  palette of the layer may be set
// if the size is set, the gorup will resize the frame to that size if possible
// if palette and gamm_type are set, the gorup will try to satisfy these
// if not set, the layer size, palette, gamma will be set depnding on which src of the group is used
// if the layer does set palette and gamma, these are stored for information in apparent_palette, apparent_gamma
// this allows for a consistent visual by setting the next layer to the same values
//
// if load_frame() is called with pixel_data in layer, this will be freed
//
// instead of attaching layer to a srcgrp and calling load_frame(), it is also possible to set the srcgrp for
// a layer and call it that way
typedef struct {
  int n_srcs;
  int track;

  int active_src;

  lives_clip_src_t **srcs;

  union {
    int apparent_pal;
    full_pal_t apparent_pally;
  };

  int apparent_gamma;

  pthread_mutex_t src_mutex;

  pthread_mutex_t refcnt_mutex;

  // refcnt set to 1 on creation
  int refcnt;

  weed_layer_t *layer; // current output layer for group

  int purpose; // the src_group purpose, for locating it
  int status; // group status, same codes as for clip_src
} lives_clipsrc_group_t;

/// corresponds to one clip in the GUI
typedef struct _lives_clip_t {
  binval binfmt_check, binfmt_version, binfmt_bytes;

  uint64_t unique_id;    ///< this and the handle can be used to uniquely id a file
  char handle[256];

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
  lives_interlace_t old_interlace;

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
  uint64_t decoder_uid, old_dec_uid;

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

  double pb_fps;  ///< current playback rate, may vary from fps, can be 0. or negative

  double target_framerate; ///< display rate we are trying to reach, may affect pb_fps
  /////
  // binfmt fields may be added here:
  ///

  ////
  //// end add section ^^^^^^^

  /// binfmt is just a file dump of the struct up to the end of binfmt_end

#define BINFMT_RSVD_BYTES 4096
  char binfmt_rsvd[BINFMT_RSVD_BYTES];
  uint64_t binfmt_end; ///< marks the end of anything "interesring" we may want to save via binfmt extension

  /// DO NOT remove or alter any fields before this ^^^^^
  ///////////////////////////////////////////////////////////////////////////
  // fields after here can be removed or changed or added to

  boolean has_binfmt;

  // TODO - this will change soon. In the future, we can have multiple clip_srcs per clip
  // and the frame index will contain pairs: clip_src | clip_src_idx
  // for now we can consider two clip_srcs: an img_decoder, and a video_decoder
  // positive values will become 1 | idx_val and -values will become 0 | idx_val
  // with idx_val < 0 meaning virtual_frame_number - idx_val
  // thus the only change initially will be to put a 1 before +ve values and a 0 before -ve values
  // - the advantage of this method is that for example we can have clips which pull frames from multiple
  // video clips, simply by creating a clip_src for each clip, however we will need to keep a record of
  // what the clip_src numbers refer to -
  // eg. 0 == img_decoder, 1 == dec_plugin with uid X, targeted at vidclip with URI Y, md5sum Z

  /// index of frames for CLIP_TYPE_FILE
  /// >0 means corresponding frame within original clip (undecoded)
  // e.g if frame_index[0] ie 20, then it means frame 20 (starting from 1) in the encoded source
  // if frame_index[50] is 99, then this is frame 99 in encoded source
  /// -1 means corresponding decoded (image) file (equivalent to CLIP_TYPE_DISK)
  // ie. if frame_index[0] is -1, this corresponds to image 1, if frame_index[100] is -1, then it means image 101
  // the advantage of only using -1 for images is that image files can be renamed / overwtitten to remove gaps
  // and the corresponding section cut from the frame_index without a need to renumber anything within it

  /// these are pointers to buffers large enough to hold frames *
  // at least frames * sizeof(frames_t) and old_frames * sizeof(frames_t) respectively
  // TODO - this will change at soem point - -replaced by an array of struct - clip_src, identifier  fro src
  frames_t *frame_index;
  frames_t *frame_index_back; ///< for undo
  pthread_mutex_t frame_index_mutex;

  // alt_frame_index may be used for temporary remappings, without disturbing the "real" frame_index
  // in this variant, decoded (image) frames are stored as -frame, rather than simply -1
  // this is useful if we want to skip over images without physically removing them
  // this can be used for CLIP_TYPE_DISK as well as CLIP_TYPE_FILE
  // (for the former, all entries will be < 0, for the latter, at least one entry will be > 0)
  frames_t *alt_frame_index;
  frames_t alt_frames; // the number "frames" in the alt_frame_index

  double img_decode_time;

  char info_file[PATH_MAX]; ///< used for asynch communication with externals

  LiVESWidget *menuentry;
  ulong menuentry_func;
  double freeze_fps; ///< pb_fps for paused / frozen clips
  volatile boolean play_paused;

  double fps_scale; // scale factor for transitory adjustments to pb_fps during playback (default is 1.0)

  lives_direction_t adirection; ///< audio play direction during playback, FORWARD or REVERSE.

  /// don't show preview/pause buttons on processing
  boolean nopreview;

  /// don't show the 'keep' button - e.g. for operations which resize frames
  boolean nokeep;

  // current and last played index frames for internal player
  frames_t saved_frameno;

  char staging_dir[PATH_MAX];

  pthread_mutex_t transform_mutex;

  // can be PUSH, PULL or PUSH_PULL
  lives_delivery_t delivery;

  char **frame_md5s[2]; // we have two arrays, 0 == decoded frames,
  // 1 == undecoded frames
  char blank_md5s[10][MD5_SIZE];

  /////////////////////////////////////////////////////////////
  // see resample.c for new events system

  // events
  resample_event *resample_events;  ///<for block resampler

  weed_plant_t *event_list;
  weed_plant_t *event_list_back;
  weed_plant_t *next_event;

  LiVESList *layout_map;
  double lmap_fix_apad;
  ////////////////////////////////////////////////////////////////////////////////////////

  pthread_mutex_t srcgrp_mutex;

  lives_clipsrc_group_t **src_groups;
  int n_src_groups;

  // since clips may have multiple clip_srcs each with their own internal palete, we select one palette
  // to be "representative" of tyhe clip. Just as width and heith are "representative" of the frame size
  // aooarent pal is representative of the palette. This means that whatever clip_src is applied for a frame
  // we convert it to width,height, apparent_pal
  int apparent_pal;

  uint64_t *cache_objects; ///< for future use

  lives_proc_thread_t pumper;

  volatile off64_t aseek_pos; ///< audio seek posn. (bytes) for when we switch clips
  ticks_t async_delta;

  int aplay_fd; /// may point to a buffered file during playback, else -1

  // last frame requested by timer. The base from which we calculate the next frame to play
  // this may differ from last_frameno, which is the last frame actually played
  frames_t last_req_frame;

  frames_t next_frame; // efault 0, can be setr to force player to show
  // a certain frame next, and continuue from there

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

  ticks_t sync_delta; // used for audio sync when switching back to the clip
} lives_clip_t;

#endif

#ifndef _CLIPHANDLER_H_
#define _CLIPHANDLER_H_

typedef struct {
  /// list of entries in clip thumbnail cache (for multitrack timeline)
  frames_t frame;
  LiVESPixbuf *pixbuf;
} lives_tcache_entry_t;

typedef enum {
  CLIP_DETAILS_HEADER_VERSION,
  CLIP_DETAILS_UNIQUE_ID,
  CLIP_DETAILS_CLIPNAME,
  CLIP_DETAILS_FILENAME,
  CLIP_DETAILS_FPS,
  CLIP_DETAILS_WIDTH,
  CLIP_DETAILS_HEIGHT,
  CLIP_DETAILS_ARATE,
  CLIP_DETAILS_ACHANS,
  CLIP_DETAILS_ASIGNED,
  CLIP_DETAILS_AENDIAN,
  CLIP_DETAILS_ASAMPS,
  CLIP_DETAILS_FRAMES,
  CLIP_DETAILS_INTERLACE,
  CLIP_DETAILS_DECODER_NAME, // deprecated
  CLIP_DETAILS_DECODER_UID,
  CLIP_DETAILS_BPP,
  CLIP_DETAILS_IMG_TYPE,
  CLIP_DETAILS_GAMMA_TYPE,
  CLIP_DETAILS_PB_FPS,
  CLIP_DETAILS_PB_ARATE,
  CLIP_DETAILS_PB_FRAMENO,
  CLIP_DETAILS_MD5SUM, // for future use
  CLIP_DETAILS_CACHE_OBJECTS, // for future use
  CLIP_DETAILS_TITLE,
  CLIP_DETAILS_AUTHOR,
  CLIP_DETAILS_COMMENT,
  CLIP_DETAILS_KEYWORDS,
  //
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
  lives_match_t matchsize;
  boolean do_update;
  boolean allownf;
  boolean debug;
  // returned values
  double duration;
  char vidchoice[512];
  char audchoice[512];
  // TODO: add audio bitrate ?, audio_lang, get_sub, sub_format, sub_language, etc.
} lives_remote_clip_request_t;

// mid level API
lives_clip_t *create_cfile(int new_file, const char *handle, boolean is_loaded);
void switch_clip(int type, int newclip, boolean force);
int find_next_clip(int index, int old_file);

// low level API
void switch_to_file(int old_file, int new_file);
void do_quick_switch(int new_file);
boolean switch_audio_clip(int new_file, boolean activate);

int create_nullvideo_clip(const char *handle);
char *get_untitled_name(int number);

#define LIVES_LITERAL_EVENT "event"
#define LIVES_LITERAL_FRAMES "frames"

#define get_frame_md5(clip, frame)					\
  (IS_PHYSICAL_CLIP((clip))						\
   ? (((frame) <= mainw->files[(clip)]->frames				\
       && get_indexed_frame((clip), (frame) + 1) < 0) ?			\
      ((mainw->files[(clip)]->frame_md5s[0]) ?				\
       mainw->files[(clip)]->frame_md5s[0][-get_indexed_frame((clip), (frame) + 1)- 1] : NULL) \
      : (((((get_clip_cdata((clip)) && (frame) <= get_clip_cdata((clip))->nframes))) \
	  && get_indexed_frame((clip), (frame) + 1) >= 0 && mainw->files[(clip)]->frame_md5s[1]) \
	 ? mainw->files[(clip)]->frame_md5s[1][get_indexed_frame((clip), (frame) + 1)] : NULL)): NULL)

// srcgrps

lives_clip_src_t *_clone_clipsrc(lives_clipsrc_group_t *srcgrp, int nclip, lives_clip_src_t *insrc);

// create a clone of primary
lives_clipsrc_group_t *clone_srcgrp(int sclip, int dclip, int track, int purpose);
lives_clip_src_t *add_clip_src(int nclip, int track, int purpose, void *actor, int src_type,
                               uint64_t actor_uid, fingerprint_t *chksum, const char *ext_URI);

lives_clip_src_t *get_clip_src(lives_clipsrc_group_t *, uint64_t actor_uid, int src_type, const char *ext_URI,
                               fingerprint_t *chksum);
lives_clip_src_t *find_src_by_class_uid(lives_clipsrc_group_t *srcgrp, uint64_t class_uid);

lives_clipsrc_group_t *get_srcgrp(int nclip, int track, int purpose);
lives_clipsrc_group_t *get_primary_srcgrp(int nclip);

boolean swap_srcgrps(int nclip, int otrack, int opurpose, int ntrack, int npurpose);

void clip_src_free(int nclip,  lives_clipsrc_group_t *, lives_clip_src_t *);
void clip_srcs_free_all(int nclip, lives_clipsrc_group_t *);
void srcgrp_free(int nclip, lives_clipsrc_group_t *);
void srcgrp_remove(int nclip, int track, int purpose);
void srcgrps_free_all(int nclip);

void *get_primary_actor(lives_clip_t *);
void *get_primary_inst(lives_clip_t *);
int get_primary_src_type(lives_clip_t *);
lives_clip_src_t *get_primary_src(int nclip);
lives_clip_src_t *add_primary_src(int nclip, void *actor, int src_type);
lives_clip_src_t *add_primary_inst(int nclip, void *actor, void *actor_inst, int src_type);
void remove_primary_src(int nclip, int src_type);

void srcgrp_set_apparent(lives_clip_t *, lives_clipsrc_group_t *, full_pal_t pally, int gamma_type);

// clip sources
/* lives_clip_src_t *add_clip_source(int nclip, int track, int purpose, void *source, int src_type); */
/* lives_clip_src_t *get_clip_source(int nclip, int track, int purpose); */
/* void clip_source_remove(int nclip, int track, int purpose); */
/* void clip_source_free(int nclip, lives_clip_src_t *); */
/* void clip_sources_free_all(int nclip); */
/* boolean swap_clip_sources(int nclip, int otrack, int opurpose, int ntrack, int npurpose); */

/* // create union from all clip srcs, teinated with WEED_PALETTE_END */
/* int *combine_src_palettes(int clipno); */

//////////

void init_clipboard(void);

int find_clip_by_uid(uint64_t uid);

char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp);

boolean del_clip_value(int which, lives_clip_details_t what);
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

void set_undoable(const char *what, boolean sensitive);
void set_redoable(const char *what, boolean sensitive);

boolean read_from_infofile(FILE *infofile);

// query function //
boolean check_for_ratio_fps(double fps);

double get_ratio_fps(const char *string);

boolean calc_ratio_fps(double fps, int *numer, int *denom);

boolean ignore_clip(int which);

void make_cleanable(int clipno, boolean isit);

void remove_old_headers(int which);
boolean write_headers(int which);
boolean read_headers(int which, const char *dir, const char *file_name);

char *get_clip_dir(int which);

void permit_close(int which);

char *get_staging_dir_for(int clipno, const lives_intentcap_t *);
void migrate_from_staging(int clipno);
char *use_staging_dir_for(int clipno);

// should we use a decoder to reload ?
boolean should_use_decoder(int clipno);

/// intents ////

// aliases for object states
#define CLIP_STATE_NOT_LOADED 	OBJECT_STATE_EXTERNAL
#define CLIP_STATE_READY	OBJECT_STATE_PREPARED

#define CLIP_ATTR_STAGING_DIR "staging_dir"

void make_object_for_clip(int clipno, lives_intentcap_t *);

#endif
