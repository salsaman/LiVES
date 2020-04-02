// pulse.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2020
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifdef HAVE_PULSE_AUDIO

#include <pulse/context.h>
#include <pulse/thread-mainloop.h>
#include <pulse/introspect.h>
#include <pulse/stream.h>
#include <pulse/proplist.h>
#include <pulse/error.h>

#define PULSE_MAX_OUTPUT_CHANS PA_CHANNEL_POSITION_MAX

#define LIVES_PA_BUFF_MAXLEN 16384
#define LIVES_PA_BUFF_TARGET 4096
#define LIVES_PA_BUFF_MINREQ 2048
#define LIVES_PA_BUFF_FRAGSIZE 4096

#define PA_SAMPSIZE 16
#define PA_ACHANS 2

typedef struct {
  ssize_t size;
  size_t max_size;
  volatile void *data;
} audio_buffer_t;

typedef struct {
  pa_threaded_mainloop *mloop;
  pa_context *con;
  pa_stream *pstream;
  pa_proplist *pa_props;

  volatile pa_usec_t usec_start;
  volatile pa_usec_t extrausec;
  double tscale;

  int str_idx;

  pa_stream_state_t state;

  // app side
  volatile int in_arate; /**< samples(frames) per second */
  uint64_t in_achans; /**< number of input channels(1 is mono, 2 stereo etc..) */
  uint64_t in_asamps;

  // server side
  int out_arate; /**< samples(frames) per second */
  uint64_t out_achans; /**< number of output channels(1 is mono, 2 stereo etc..) */
  uint64_t out_asamps;

  uint64_t out_chans_available;

  int in_signed;
  int in_endian;

  int out_signed;
  int out_endian;

  uint64_t num_calls; /**< count of process_audio() calls */

  audio_buffer_t *aPlayPtr; ///< data read from file
  lives_audio_loop_t loop;

  uint8_t *sound_buffer; ///< transformed data

  pa_cvolume volume;

  //float volume[PULSE_MAX_OUTPUT_CHANS]; ///< amount volume, 1.0 is full volume (currently unused, see volume_linear)

  boolean in_use; /**< true if this device is currently in use */
  boolean mute;

  /**< linked list of messages we are sending to the callback process */
  volatile aserver_message_t   *msgq;

  volatile uint64_t frames_written;

  boolean is_paused;

  int fd; /**< if >0 we are playing from a lives_clip_t */
  off_t seek_pos;
  volatile off_t real_seek_pos;
  off_t seek_end;
  boolean usigned;
  boolean reverse_endian;

  volatile lives_whentostop_t *whentostop; ///< pointer to mainw->whentostop
  volatile lives_cancel_t *cancelled; ///< pointer to mainw->cancelled

  boolean pulsed_died;

  boolean is_output; ///< is output FROM host to jack

  volatile int playing_file;

  lives_audio_buf_t **abufs;
  volatile int read_abuf;

  size_t chunk_size;

  double volume_linear; ///< TODO: use perchannel volume[]

  volatile int astream_fd;

  volatile float abs_maxvol_heard;

  volatile boolean is_corked;
} pulse_driver_t;

// TODO - rationalise names

boolean lives_pulse_init(short startup_phase);  ///< init server, mainloop and context

int pulse_audio_init(void);  ///< init driver vars.
int pulse_audio_read_init(void); // ditto

pulse_driver_t *pulse_get_driver(boolean is_output); ///< get driver refs

int pulse_driver_activate(pulse_driver_t *); ///< connect to server
void pulse_close_client(pulse_driver_t *);

void pulse_shutdown(void); ///< shudown server, mainloop, context

void pulse_aud_pb_ready(int fileno);

size_t pulse_flush_read_data(pulse_driver_t *, int fileno, size_t rbytes, boolean rev_endian, void *data);

void pulse_driver_uncork(pulse_driver_t *);
void pulse_driver_cork(pulse_driver_t *);

boolean pulse_try_reconnect(void);

// utils
volatile aserver_message_t *pulse_get_msgq(pulse_driver_t *); ///< pull last msg from msgq, or return NULL

int64_t pulse_audio_seek_bytes(pulse_driver_t *, int64_t bytes, lives_clip_t *);

boolean pa_time_reset(pulse_driver_t *, int64_t offset);

ticks_t lives_pulse_get_time(pulse_driver_t *); ///< get time from pa, in 10^-8 seconds

double lives_pulse_get_pos(pulse_driver_t *);

size_t pulse_get_buffsize(pulse_driver_t *);


void pa_mloop_lock(void);
void pa_mloop_unlock(void);

//////////////////////

boolean pulse_audio_seek_frame(pulse_driver_t *, double frame);  ///< seek to (video) frame

void pulse_get_rec_avals(pulse_driver_t *);

#endif
