// jack.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2013
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_JACK_H
#define HAS_LIVES_JACK_H

#ifdef ENABLE_JACK




/////////////////////////////////////////////////////////////////
// Transport

#include <jack/jack.h>
#include <jack/transport.h>

boolean lives_jack_init(void);  /** start up server on LiVES init */
boolean lives_jack_poll(void); /** poll function to check transport state */
void lives_jack_end(void);

int lives_start_ready_callback(jack_transport_state_t state, jack_position_t *pos, void *arg);

void jack_pb_start(void);  /** start playback transport master */
void jack_pb_stop(void);  /** pause playback transport master */


////////////////////////////////////////////////////////////////////////////
// Audio

#include "audio.h"

#define JACK_MAX_OUTPUT_PORTS 10
#define JACK_MAX_INPUT_PORTS 10

#define ERR_PORT_NOT_FOUND 10


typedef jack_nframes_t nframes_t;



// let's hope these are well above the standard jack transport states...
#define JackTClosed 1024
#define JackTReset 1025
#define JackTStopped 1026

typedef struct {
  int      dev_idx;                      /**< id of this device ??? */
  int64_t     sample_out_rate;                   /**< samples(frames) per second */
  int64_t     sample_in_rate;                   /**< samples(frames) per second */
  uint64_t    num_input_channels;            /**< number of input channels(1 is mono, 2 stereo etc..) */
  uint64_t    num_output_channels;           /**< number of output channels(1 is mono, 2 stereo etc..) */
  uint64_t    bytes_per_channel;

  uint64_t    num_calls;                     /**< count of process_audio() calls */

  jack_port_t     *output_port[JACK_MAX_OUTPUT_PORTS]; /**< output ports */
  jack_port_t     *input_port[JACK_MAX_INPUT_PORTS]; /**< input ports */
  jack_client_t   *client;                        /**< pointer to jack client */

  char             **jack_port_name;              /**< user given strings for the port names, can be NULL */
  unsigned int     jack_port_name_count;          /**< the number of port names given */
  uint64_t    jack_port_flags;               /**< flags to be passed to jack when opening the output ports */

  lives_audio_loop_t loop;

  jack_transport_state_t state;

  float     volume[JACK_MAX_OUTPUT_PORTS];      ///< amount volume, 1.0 is full volume

  boolean          in_use;                        /**< true if this device is currently in use */
  boolean mute;

  volatile aserver_message_t   *msgq;          /**< linked list of messages we are sending to the callback process */

  off_t seek_pos;
  off_t seek_end;
  boolean usigned;
  boolean reverse_endian;

  lives_whentostop_t *whentostop; ///< pointer to mainw->whentostop
  volatile lives_cancel_t *cancelled; ///< pointer to mainw->cancelled

  /* variables used for trying to restart the connection to jack */
  boolean             jackd_died;                    /**< true if jackd has died and we should try to restart it */
  struct timeval   last_reconnect_attempt;

  boolean play_when_stopped; ///< if we should play audio even when jack transport is stopped
  uint64_t audio_ticks; ///< ticks when we did the last seek, used to calculate current ticks from audio
  uint64_t frames_written;

  int out_chans_available;
  int in_chans_available;

  boolean is_paused;

  boolean is_output; ///< is output FROM host to jack

  boolean is_silent;

  boolean is_active;

  int playing_file;

  volatile float jack_pulse[1024];

  lives_audio_buf_t **abufs;
  volatile int read_abuf;

  volatile int astream_fd;

} jack_driver_t;


#define JACK_MAX_OUTDEVICES 10
#define JACK_MAX_INDEVICES 10


////////////////////////////////////////////////////////////////////////////

jack_driver_t *jack_get_driver(int dev_idx, boolean is_output); ///< get driver

int jack_audio_init(void); ///< init jack for host output
int jack_audio_read_init(void); ///< init jack for host input

int jack_open_device(jack_driver_t *); ///< open device for host output
int jack_open_device_read(jack_driver_t *); ///< open device for host input

int jack_driver_activate(jack_driver_t *);  ///< activate for host playback
int jack_read_driver_activate(jack_driver_t *, boolean autocon);  ///< activate for host recording

void jack_close_device(jack_driver_t *);

boolean jack_try_reconnect(void);

void jack_aud_pb_ready(int fileno);


// utils
volatile aserver_message_t *jack_get_msgq(jack_driver_t *); ///< pull last msg from msgq, or return NULL
uint64_t lives_jack_get_time(jack_driver_t *, boolean absolute); ///< get time from jack, in 10^-8 seconds
boolean jack_audio_seek_frame(jack_driver_t *, int frame);  ///< seek to (video) frame
int64_t jack_audio_seek_bytes(jack_driver_t *, int64_t bytes);  ///< seek to byte position

void jack_get_rec_avals(jack_driver_t *);

uint64_t jack_transport_get_time(void);

double lives_jack_get_pos(jack_driver_t *);


#endif


#endif
