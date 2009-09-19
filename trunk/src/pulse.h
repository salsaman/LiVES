// pulse.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2009
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


typedef struct {
  pa_threaded_mainloop *mloop;
  pa_context *con;
  pa_stream *pstream;
  pa_proplist *pa_props;

  int str_idx;

  pa_context_state_t state;

  // app side
  glong in_arate; /* samples(frames) per second */
  gulong in_achans; /* number of input channels(1 is mono, 2 stereo etc..) */
  gulong in_asamps;

  // server side
  glong out_arate; /* samples(frames) per second */
  gulong out_achans; /* number of output channels(1 is mono, 2 stereo etc..) */
  gulong out_asamps;

  gulong out_chans_available;

  int in_signed;
  int in_endian;

  int out_signed;
  int out_endian;

  gulong num_calls; /* count of process_audio() calls */

  audio_buffer_t* aPlayPtr; // data read from file
  lives_audio_loop_t loop;

  guchar* sound_buffer; // transformed data

  float volume[PULSE_MAX_OUTPUT_CHANS]; // amount volume, 1.0 is full volume

  gboolean in_use; /* true if this device is currently in use */
  gboolean mute;

  /* linked list of messages we are sending to the callback process */
  volatile aserver_message_t   *msgq;

  gulong frames_written;

  gboolean is_paused;

  gint64 audio_ticks; // ticks when we did the last seek, used to calculate current ticks from audio

  int fd; /* if >0 we are playing from a file */
  gboolean is_opening; // TRUE if file is opening (audiodump.pcm)
  off_t seek_pos;
  off_t seek_end;
  gboolean usigned;
  gboolean reverse_endian;

  gshort *whentostop; // pointer to mainw->whentostop
  volatile gint *cancelled; // pointer to mainw->cancelled

  /* variables used for trying to restart the connection to pulse */
  gboolean pulsed_died;
  struct timeval last_reconnect_attempt;

  gboolean is_output; // is output FROM host to jack

  gboolean is_silent;

  gint playing_file;

  lives_audio_buf_t **abufs;
  volatile gint read_abuf;

  gulong chunk_size;


} pulse_driver_t;



void lives_pulse_init (void); // init server, mainloop and context

int pulse_audio_init(void);  // init driver vars.
int pulse_audio_read_init(void); // ditto

pulse_driver_t *pulse_get_driver(gboolean is_output); // get driver refs

int pulse_driver_activate(pulse_driver_t *); // connect to server
void pulse_close_client(pulse_driver_t *, gboolean shutdown); //


// utils
volatile aserver_message_t *pulse_get_msgq(pulse_driver_t *); // pull last msg from msgq, or return NULL

gint64 lives_pulse_get_time(pulse_driver_t *); // get time from pa, in 10^-8 seconds
void pulse_audio_seek_frame (pulse_driver_t *, gint frame); // seek to (video) frame
long pulse_audio_seek_bytes (pulse_driver_t *, long bytes); // seek to byte position

void pulse_get_rec_avals(pulse_driver_t *);



#endif
