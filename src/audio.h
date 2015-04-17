// audio.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2009
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_AUDIO_H
#define HAS_LIVES_AUDIO_H

#define SAMPLE_MAX_16BIT_P  32767.0f
#define SAMPLE_MAX_16BIT_N  32768.0f
#define SAMPLE_MAX_16BITI  32768

///sign swapping
#define SWAP_U_TO_S 1 ///< unsigned to signed
#define SWAP_S_TO_U 2 ///< signed to unsigned

///endian swapping
#define SWAP_X_TO_L 1  ///< other to local
#define SWAP_L_TO_X 2 ///< local to other


/// defaults for when not specifed
# define DEFAULT_AUDIO_RATE 44100
# define DEFAULT_AUDIO_CHANS 2
# define DEFAULT_AUDIO_SAMPS 16
# define DEFAULT_AUDIO_SIGNED8 (AFORM_UNSIGNED)
# define DEFAULT_AUDIO_SIGNED16 (!AFORM_UNSIGNED)

/// KO time before declaring audio server dead
#define LIVES_ACONNECT_TIMEOUT (10 * U_SEC)

/// TODO ** - make configurable - audio buffer size for rendering
#define MAX_AUDIO_MEM 8*1024*1024

/// chunk size for interpolate/effect cycle
#define RENDER_BLOCK_SIZE 1024

/// size of silent block in bytes
#define SILENCE_BLOCK_SIZE 65536

/// chunk size for audio buffer reads
#define READ_BLOCK_SIZE 4096

/// buffer size for realtime audio
#define XSAMPLES 128000


/////////////////////////////////////
/// asynch msging


#define ASERVER_CMD_PROCESSED 0
#define ASERVER_CMD_FILE_OPEN 1
#define ASERVER_CMD_FILE_CLOSE 2
#define ASERVER_CMD_FILE_SEEK 3

/* message passing structure */
typedef struct _aserver_message_t {
  volatile int command;
  volatile char *data;
  volatile struct _aserver_message_t *next;
} aserver_message_t;



typedef enum {
  LIVES_NOP_OPERATION=0,
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
  weed_timecode_t start_tc;

  double arate;

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

  size_t samp_space; ///< buffer space in samples (* by sizeof(type) to get bytesize) [if interleaf, also * by chans]


  // in or out buffers
  uint8_t **buffer8; ///< sample data in 8 bit format (or NULL)
  short   **buffer16; ///< sample data in 16 bit format (or NULL)
  int32_t **buffer24; ///< sample data in 24 bit format (or NULL)
  int32_t **buffer32; ///< sample data in 32 bit format (or NULL)
  float   **bufferf; ///< sample data in float format (or NULL)

  boolean s8_signed;
  boolean s16_signed;
  boolean s24_signed;
  boolean s32_signed;

  // ring buffer
  size_t samples_filled; ///< number of samples filled (readonly client)
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

  volatile boolean die;  ///< set to TRUE to shut down thread

} lives_audio_buf_t;


//////////////////////////////////////////

typedef enum lives_audio_loop {
  AUDIO_LOOP_NONE,
  AUDIO_LOOP_FORWARD,
  AUDIO_LOOP_PINGPONG
} lives_audio_loop_t;



void sample_silence_dS(float *dst, uint64_t nsamples);

void sample_move_d8_d16(short *dst, uint8_t *src,
                        uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign);

void sample_move_d16_d16(short *dst, short *src,
                         uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_endian, int swap_sign);

void sample_move_d16_d8(uint8_t *dst, short *src,
                        uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign);

void sample_move_d16_float(float *dst, short *src, uint64_t nsamples, uint64_t src_skip, int is_unsigned, boolean rev_endian, float vol);

int64_t sample_move_float_int(void *holding_buff, float **float_buffer, int nsamps, float scale, int chans, int asamps, int usigned,
                              boolean swap_endian, boolean float_interleaved, float vol); ///< returns frames output

int64_t sample_move_abuf_float(float **obuf, int nchans, int nsamps, int out_arate, float vol);

int64_t sample_move_abuf_int16(short *obuf, int nchans, int nsamps, int out_arate);

void sample_move_float_float(float *dst, float *src, uint64_t nsamples, float scale, int dst_skip);

boolean float_deinterleave(float *fbuffer, int nsamps, int nchans);
boolean float_interleave(float *fbuffer, int nsamps, int nchans);

int64_t render_audio_segment(int nfiles, int *from_files, int to_file, double *avels, double *fromtime, weed_timecode_t tc_start,
                             weed_timecode_t tc_end, double *chvol, double opvol_start, double opvol_end, lives_audio_buf_t *obuf);

void aud_fade(int fileno, double startt, double endt, double startv, double endv); ///< fade in/fade out

typedef enum {
  RECA_WINDOW_GRAB,
  RECA_NEW_CLIP,
  RECA_EXISTING,
  RECA_EXTERNAL,
  RECA_GENERATED
} lives_rec_audio_type_t;

#ifdef ENABLE_JACK
void jack_rec_audio_to_clip(int fileno, int oldfileno, lives_rec_audio_type_t rec_type);  ///< record from external source to clip
void jack_rec_audio_end(boolean close_dev, boolean close_fd);
#endif

#ifdef HAVE_PULSE_AUDIO
void pulse_rec_audio_to_clip(int fileno, int oldfileno, lives_rec_audio_type_t rec_type);  ///< record from external source to clip
void pulse_rec_audio_end(boolean close_dev, boolean close_fd);
#endif

void fill_abuffer_from(lives_audio_buf_t *abuf, weed_plant_t *event_list, weed_plant_t *st_event, boolean exact);


boolean resync_audio(int frameno);


lives_audio_track_state_t *get_audio_and_effects_state_at(weed_plant_t *event_list, weed_plant_t *st_event, boolean get_audstate,
    boolean exact);

boolean get_audio_from_plugin(float *fbuffer, int nchans, int arate, int nsamps);
void reinit_audio_gen(void);

void init_jack_audio_buffers(int achans, int arate, boolean exact);
void free_jack_audio_buffers(void);

void init_pulse_audio_buffers(int achans, int arate, boolean exact);
void free_pulse_audio_buffers(void);

void audio_free_fnames(void);

lives_audio_buf_t *audio_cache_init(void);
void audio_cache_end(void);
lives_audio_buf_t *audio_cache_get_buffer(void);

boolean apply_rte_audio_init(void);
void apply_rte_audio_end(boolean del);
boolean apply_rte_audio(int nframes);

void init_audio_frame_buffer(short aplayer);
void free_audio_frame_buffer(lives_audio_buf_t *abuf);
void append_to_audio_bufferf(lives_audio_buf_t *abuf, float *src, uint64_t nsamples, int channum);
void append_to_audio_buffer16(lives_audio_buf_t *abuf, void *src, uint64_t nsamples, int channum);
boolean push_audio_to_channel(weed_plant_t *achan, lives_audio_buf_t *abuf);

boolean start_audio_stream(void);
void stop_audio_stream(void);
void clear_audio_stream(void);
void audio_stream(void *buff, size_t nbytes, int fd);

#endif
