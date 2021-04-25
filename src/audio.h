// audio.h
// LiVES (lives-exe)
// (c) G. Finch (salsaman+lives@gmail.com) 2005 - 2020
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_AUDIO_H
#define HAS_LIVES_AUDIO_H

typedef uint64_t size64_t;
typedef int64_t ssize64_t;

#define AUD_SRC_EXTERNAL (prefs->audio_src == AUDIO_SRC_EXT)
#define AUD_SRC_INTERNAL (prefs->audio_src == AUDIO_SRC_INT)
#define AUD_SRC_REALTIME (get_play_clipno() != -1)
#define AV_CLIPS_EQUAL (get_aplay_clipno() == mainw->playing_file)

#define SAMPLE_MAX_16BIT_P  32767.4999999f
#define SAMPLE_MAX_16BIT_N  32768.0f
#define SAMPLE_MAX_16BITI  32768

///sign swapping
#define SWAP_U_TO_S 1 ///< unsigned to signed
#define SWAP_S_TO_U 2 ///< signed to unsigned

///endian swapping
#define SWAP_X_TO_L 1  ///< other to local
#define SWAP_L_TO_X 2 ///< local to other

/// defaults for when not specified
# define DEFAULT_AUDIO_RATE 44100
# define DEFAULT_AUDIO_CHANS 2
# define DEFAULT_AUDIO_SAMPS 16
# define DEFAULT_AUDIO_SIGNED8 (AFORM_UNSIGNED)
# define DEFAULT_AUDIO_SIGNED16 (!AFORM_UNSIGNED)

// for afbuffer
#define ABUF_ARENA_SIZE 768000

/// max number of player channels
#define MAX_ACHANS 2

// keep first N audio_in filesysten handles open - multitrack only
#define NSTOREDFDS 64

/// TODO ** - make configurable - audio buffer size for rendering
#define MAX_AUDIO_MEM 32 * 1024 * 1024

/// chunk size for interpolate/effect cycle
#define RENDER_BLOCK_SIZE 1024

/// size of silent block in bytes
#define SILENCE_BLOCK_SIZE BUFFER_FILL_BYTES_LARGE

/// buffer size (output samples) for (semi) realtime audio (bytes == XSAMPLES * achans * samp. size)
/// this is shared across prefs->num_rtaudiobufs buffers (default 4)
/// used when we have an event_list (i.e multitrack or previewing a recording in CE)
#define XSAMPLES 393216

#define AUD_WRITE_CHECK 0xFFFFFFFFF4000000 ///< after recording this many bytes we check disk space (default 128MB)

#define WEED_LEAF_HOST_KEEP_ADATA "keep_adata" /// set to WEED_TRUE in layer if doing zero-copy audio processing

/////////////////////////////////////
/// asynch msging

#define ASERVER_CMD_PROCESSED 0
#define ASERVER_CMD_FILE_OPEN 1
#define ASERVER_CMD_FILE_CLOSE 2
#define ASERVER_CMD_FILE_SEEK 3

/* message passing structure */
typedef struct _aserver_message_t {
  volatile int command;
  ticks_t tc;
  volatile char *data;
  volatile struct _aserver_message_t *next;
} aserver_message_t;

typedef enum {
  LIVES_NOP_OPERATION = 0,
  LIVES_READ_OPERATION,
  LIVES_WRITE_OPERATION,
  LIVES_CONVERT_OPERATION
} lives_operation_t;

typedef struct {
  lives_operation_t operation; // read, write, or convert [readonly by server]
  volatile boolean is_ready; // [readwrite all]
  boolean eof; ///< did we read EOF ?  [readonly by client]
  int fileno; // [readonly by server]

  // readonly by server:

  // use one or other
  off_t seek;
  ticks_t start_tc;

  int arate;

  ssize_t bytesize; // file in/out length in bytes [write by server in case of eof]

  boolean in_interleaf;
  boolean out_interleaf;

  int in_achans; ///< channels for _filebuffer side
  int out_achans; ///< channels for buffer* side
  int in_asamps; // set to -val for float
  int out_asamps; // set to -val for float
  int swap_sign;
  int swap_endian;
  double shrink_factor;  ///< resampling ratio

  // this is for the afbuffer (bufferf is an arena)
  int vclients, aclients;
  int vclients_read, aclients_read;
  size_t vclient_readpos, aclient_readpos;
  size_t vclient_readlevel, aclient_readlevel;
  volatile ssize_t write_pos;

  size_t samp_space; ///< buffer space in samples (* by sizeof(type) to get bytesize) [if interleaf, also * by chans]

  boolean sequential; ///< hint that we will read sequentially starting from seek

  // in or out buffers
  uint8_t **buffer8; ///< sample data in 8 bit format (or NULL)
  union {
    short   **buffer16; ///< sample data in 16 bit format (or NULL)
    uint8_t **buffer16_8; ///< sample data in 8 bit format (or NULL)
  };
  int32_t **buffer24; ///< sample data in 24 bit format (or NULL)
  int32_t **buffer32; ///< sample data in 32 bit format (or NULL)
  float   **bufferf; ///< sample data in float format (or NULL)

  // input values
  boolean s8_signed;
  boolean s16_signed;
  boolean s24_signed;
  boolean s32_signed;

  // ring buffer
  volatile size_t samples_filled; ///< number of samples filled (readonly client)
  size_t start_sample; ///< used for reading (readonly server)

  // private fields (used by server)
  uint8_t *_filebuffer; ///< raw data to/from file - can be cast to int16_t
  ssize_t _cbytesize; ///< current _filebuffer bytesize; if this changes we need to realloc _filebuffer
  size_t _csamp_space; ///< current sample buffer size in single channel samples
  int _fd; ///< file descriptor
  int _cfileno; ///< current fileno
  int _cseek;  ///< current seek pos
  int _cachans; ///< current output channels
  int _cin_interleaf;
  int _cout_interleaf;
  int _casamps; ///< current out_asamps
  double _shrink_factor;  ///< resampling ratio

  pthread_mutex_t atomic_mutex; ///<  ensures all buffer info updated together

  volatile boolean die;  ///< set to TRUE to shut down thread
} lives_audio_buf_t;

//////////////////////////////////////////

typedef enum lives_audio_loop {
  AUDIO_LOOP_NONE,
  AUDIO_LOOP_FORWARD,
  AUDIO_LOOP_PINGPONG
} lives_audio_loop_t;

float get_float_audio_val_at_time(int fnum, int afd, double secs, int chnum, int chans) GNU_HOT;
float audiofile_get_maxvol(int fnum, double start, double end, float thresh);

boolean normalise_audio(int fnum, double start, double end, float thresh);

void sample_silence_dS(float *dst, size64_t nsamples);

void sample_silence_stream(int nchans, int64_t nframes);

boolean pad_with_silence(int out_fd, void *buff, off64_t oins_size, int64_t ins_size, int asamps, int aunsigned,
                         boolean big_endian);

void sample_move_float_d16(int16_t *dst, float *src,
                           size64_t nsamples, size_t tbytes, double scale, int nDstChannels,
                           int nSrcChannels, int swap_endian, int swap_sign);

void sample_move_d8_d16(short *dst, uint8_t *src,
                        size64_t nsamples, size_t tbytes, double scale, int nDstChannels, int nSrcChannels, int swap_sign) GNU_HOT;

void sample_move_d16_d16(short *dst, short *src,
                         size64_t nsamples, size_t tbytes, double scale, int nDstChannels, int nSrcChannels, int swap_endian,
                         int swap_sign) GNU_HOT;

void sample_move_d16_d8(uint8_t *dst, short *src,
                        size64_t nsamples, size_t tbytes, double scale, int nDstChannels, int nSrcChannels, int swap_sign) GNU_HOT;

float sample_move_d16_float(float *dst, short *src, size_t nsamples, size_t src_skip, int is_unsigned, boolean rev_endian,
                            float vol) GNU_HOT;

int64_t sample_move_float_int(void *holding_buff, float **float_buffer, int nsamps, double scale, int chans, int asamps,
                              int usigned,
                              boolean swap_endian, boolean float_interleaved, float vol) GNU_HOT; ///< returns frames output

int64_t sample_move_abuf_float(float **obuf, int nchans, int nsamps, int out_arate, float vol) GNU_HOT;

int64_t sample_move_abuf_int16(short *obuf, int nchans, int nsamps, int out_arate) GNU_HOT;

size64_t sample_move_float_float(float *dst, float *src, size64_t in_samples, double scale, int dst_skip, float vol) GNU_HOT;

float float_deinterleave(float *dst, float *src, size64_t in_samples, double scale, int in_chans, float vol) GNU_HOT;
size64_t float_interleave(float *out, float **in, size64_t nsamps, double scale, int nchans, float vol) GNU_HOT;

int64_t render_audio_segment(int nfiles, int *from_files, int to_file, double *avels, double *fromtime, ticks_t tc_start,
                             ticks_t tc_end, double *chvol, double opvol_start, double opvol_end, lives_audio_buf_t *obuf);

void aud_fade(int fileno, double startt, double endt, double startv, double endv); ///< fade in/fade out
boolean adjust_clip_volume(int fileno, float newvol, boolean make_backup);

typedef enum {
  RECA_MONITOR = 0,
  RECA_WINDOW_GRAB,
  RECA_NEW_CLIP,
  RECA_EXISTING,
  RECA_EXTERNAL,
  RECA_GENERATED
} lives_rec_audio_type_t;

#ifdef ENABLE_JACK
void jack_rec_audio_to_clip(int fileno, int oldfileno,
                            lives_rec_audio_type_t rec_type);  ///< record from external source to clip
void jack_rec_audio_end(boolean close_fd);
#endif

#ifdef HAVE_PULSE_AUDIO
void pulse_rec_audio_to_clip(int fileno, int oldfileno,
                             lives_rec_audio_type_t rec_type);  ///< record from external source to clip
void pulse_rec_audio_end(boolean close_fd);
#endif

void fill_abuffer_from(lives_audio_buf_t *abuf, weed_plant_t *event_list, weed_plant_t *st_event, boolean exact);
void wake_audio_thread(void);

int get_aplay_clipno(void);

boolean resync_audio(int clipno, double frameno);
void avsync_force(void);

void freeze_unfreeze_audio(boolean is_frozen);

lives_audio_track_state_t *get_audio_and_effects_state_at(weed_plant_t *event_list, weed_plant_t *st_event,
    weed_timecode_t fill_tc, int what_to_get, boolean exact);

boolean get_audio_from_plugin(float **fbuffer, int nchans, int arate, int nsamps, boolean is_audio_thread);
void reinit_audio_gen(void);

void init_jack_audio_buffers(int achans, int arate, boolean exact);
void free_jack_audio_buffers(void);

void init_pulse_audio_buffers(int achans, int arate, boolean exact);
void free_pulse_audio_buffers(void);

void audio_free_fnames(void);

#define is_realtime_aplayer(ptype) ((ptype == AUD_PLAYER_JACK || ptype == AUD_PLAYER_PULSE || ptype == AUD_PLAYER_NONE))

void preview_aud_vol(void);

lives_audio_buf_t *audio_cache_init(void);
void audio_cache_end(void);
lives_audio_buf_t *audio_cache_get_buffer(void);

boolean apply_rte_audio_init(void);
void apply_rte_audio_end(boolean del);
boolean apply_rte_audio(int64_t nframes);

void init_audio_frame_buffers(short aplayer);
void init_aux_audio_frame_buffers(short aplayer);
void free_audio_frame_buffer(lives_audio_buf_t *abuf);
void append_to_audio_bufferf(float *src, size64_t nsamples, int channum);
void append_to_aux_audio_bufferf(float *src, size64_t nsamples, int channum);
void append_to_audio_buffer16(void *src, size64_t nsamples, int channum);
boolean push_audio_to_channel(weed_plant_t *filter, weed_plant_t *achan, lives_audio_buf_t *abuf, boolean is_vid);
boolean start_audio_stream(void);
void stop_audio_stream(void);
void clear_audio_stream(void);
void audio_stream(void *buff, size_t nbytes, int fd);

char *lives_get_audio_file_name(int fnum);
char *get_audio_file_name(int fnum, boolean opening);

char *get_achannel_name(int totchans, int idx) WARN_UNUSED;
const char *audio_player_get_display_name(const char *aplayer);

lives_cancel_t handle_audio_timeout(void);

lives_audio_track_state_t *audio_frame_to_atstate(weed_plant_t *event, int *ntracks);

#define lives_vol_from_linear(vol) ((float)squared(squared((vol))))
#define lives_vol_to_linear(vol) (sqrtf(sqrtf((vol))))

#endif
