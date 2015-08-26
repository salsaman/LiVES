// pulse.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifdef HAVE_PULSE_AUDIO

#include <pulse/context.h>
#include <pulse/thread-mainloop.h>
#include <pulse/introspect.h>
#include <pulse/stream.h>
#include <pulse/proplist.h>
#include <pulse/error.h>

#include "audio.h"


#define PULSE_MAX_OUTPUT_CHANS PA_CHANNEL_POSITION_MAX

#define LIVES_PA_BUFF_MAXLEN 16384
#define LIVES_PA_BUFF_TARGET 1024
#define LIVES_PA_BUFF_FRAGSIZE 1024

typedef struct {
  size_t size;
  size_t max_size;
  void *data;
} audio_buffer_t;


typedef struct {
  pa_threaded_mainloop *mloop;
  pa_context *con;
  pa_stream *pstream;
  pa_proplist *pa_props;

  pa_usec_t usec_start;

  int str_idx;

  pa_context_state_t state;

  // app side
  int64_t in_arate; /**< samples(frames) per second */
  uint64_t in_achans; /**< number of input channels(1 is mono, 2 stereo etc..) */
  uint64_t in_asamps;

  // server side
  int64_t out_arate; /**< samples(frames) per second */
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

  float volume[PULSE_MAX_OUTPUT_CHANS]; ///< amount volume, 1.0 is full volume

  boolean in_use; /**< true if this device is currently in use */
  boolean mute;

  /**< linked list of messages we are sending to the callback process */
  volatile aserver_message_t   *msgq;

  volatile uint64_t frames_written;

  boolean is_paused;

  volatile int64_t audio_ticks; ///< ticks when we did the last seek, used to calculate current ticks from audio

  int fd; /**< if >0 we are playing from a lives_clip_t */
  boolean is_opening; ///< TRUE if file is opening (audiodump.pcm)
  volatile off_t seek_pos;
  off_t seek_end;
  boolean usigned;
  boolean reverse_endian;

  lives_whentostop_t *whentostop; ///< pointer to mainw->whentostop
  volatile lives_cancel_t *cancelled; ///< pointer to mainw->cancelled

  /* variables used for trying to restart the connection to pulse */
  boolean pulsed_died;
  struct timeval last_reconnect_attempt;

  boolean is_output; ///< is output FROM host to jack

  int playing_file;

  lives_audio_buf_t **abufs;
  volatile int read_abuf;

  uint64_t chunk_size;

  volatile int astream_fd;

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

boolean pulse_try_reconnect(void);

// utils
volatile aserver_message_t *pulse_get_msgq(pulse_driver_t *); ///< pull last msg from msgq, or return NULL

int64_t pulse_audio_seek_bytes(pulse_driver_t *, int64_t bytes);  ///< seek to byte position

int64_t lives_pulse_get_time(pulse_driver_t *, boolean absolute); ///< get time from pa, in 10^-8 seconds

double lives_pulse_get_pos(pulse_driver_t *);


//////////////////////

boolean pulse_audio_seek_frame(pulse_driver_t *, int frame);  ///< seek to (video) frame

void pulse_get_rec_avals(pulse_driver_t *);



#endif
