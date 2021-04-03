// jack.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2017
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_JACK_H
#define HAS_LIVES_JACK_H

#define JACK_URL "http://jackaudio.org"

#ifdef ENABLE_JACK

/////////////////////////////////////////////////////////////////
// Transport

#include <jack/jack.h>
#include <jack/transport.h>
#include <jack/control.h>

typedef enum {
  JACK_CLIENT_TYPE_INVALID,
  JACK_CLIENT_TYPE_TRANSPORT,
  JACK_CLIENT_TYPE_AUDIO_WRITER,
  JACK_CLIENT_TYPE_AUDIO_READER,
  JACK_CLIENT_TYPE_MIDI,
  JACK_CLIENT_TYPE_SLAVE,
  JACK_CLIENT_TYPE_OTHER
} lives_jack_client_type;

// should really be lives_jack_client_t - TODO
typedef struct _lives_jack_driver_t jack_driver_t;

// GUI functions
void jack_srv_startup_config(LiVESWidget *, livespointer type_data);

boolean jack_drivers_config(LiVESWidget *, livespointer ptype);

void jack_server_config(LiVESWidget *, lives_rfx_t *);

void show_jack_status(LiVESButton *, livespointer is_transp);

// connect client or start server
boolean lives_jack_init(lives_jack_client_type client_type, jack_driver_t *jackd);
boolean lives_jack_poll(void); /** poll function to check transport state */
void lives_jack_end(void);

int lives_start_ready_callback(jack_transport_state_t state, jack_position_t *pos, void *arg);

void jack_pb_start(double pbtime);  /** start playback transport master */
void jack_pb_stop(void);  /** pause playback transport master */

////////////////////////////////////////////////////////////////////////////
// Audio

#include "audio.h"

// mapping of jack_opts pref
#define JACK_OPTS_TRANSPORT_CLIENT	(1 << 0)   ///< jack can start/stop
#define JACK_OPTS_TRANSPORT_MASTER	(1 << 1)  ///< transport master (start and stop)
#define JACK_OPTS_START_TSERVER		(1 << 2)     ///< start transport server if unable to connect
#define JACK_OPTS_NOPLAY_WHEN_PAUSED	(1 << 3) ///< do not play audio when transport is paused
#define JACK_OPTS_START_ASERVER		(1 << 4)     ///< start audio server if unable to connect

#define JACK_OPTS_TIMEBASE_START	(1 << 5)    ///< jack sets play start position
#define JACK_OPTS_TIMEBASE_CLIENT	(1 << 6)    ///< full timebase client (position updates)
#define JACK_OPTS_TIMEBASE_MASTER	(1 << 7)   ///< timebase master (not implemented yet)
#define JACK_OPTS_NO_READ_AUTOCON	(1 << 8)   ///< do not auto con. rd clients when playing ext aud
#define JACK_OPTS_TIMEBASE_LSTART	(1 << 9)    ///< LiVES sets play start position

#define JACK_OPTS_ENABLE_TCLIENT      	(1 << 10)     ///< enable transport client (global setting)

// sever is only killed if LiVES started it
// conflicts if both clients want to start same server and vals differ
#define JACK_OPTS_PERM_ASERVER      	(1 << 11)     ///< leave audio srvr running even if we started
#define JACK_OPTS_PERM_TSERVER      	(1 << 12)     ///< leave transport running even if we started it

// only one or other should be set...
#define JACK_OPTS_SETENV_ASERVER      	(1 << 13)     ///< setenv $JACK_DEFAULT_SERVER to aserver_sname
#define JACK_OPTS_SETENV_TSERVER      	(1 << 14)     ///< setenv $JACK_DEFAULT_SERVER to tserver_sname

#define JACK_INFO_TEST_SETUP      	(1 << 29)     ///< -jackserver used
#define JACK_INFO_TEMP_NAMES      	(1 << 30)     ///< -jackserver used
#define JACK_INFO_TEMP_OPTS      	(1 << 31)     ///< -jackopts used

#define JACK_OPTS_OPTS_MASK ((1 << 24) - 1)


#define JACK_MAX_OUTPUT_PORTS 10
#define JACK_MAX_INPUT_PORTS 10

#define ERR_PORT_NOT_FOUND 10

#define JACK_DEFAULT_SERVER "JACK_DEFAULT_SERVER"
#define JACK_DEFAULT_SERVER_NAME "default"

#define JACKD_RC_NAME "jackdrc"

typedef jack_nframes_t nframes_t;

// custom transport states (not very useful, may remove)
#define JackTClosed 1024 // not activated
#define JackTReset 1025
#define JackTStopped 1026

#define STMSGLEN 65536

typedef struct _lives_jack_driver_t {
  int      dev_idx;                      /**< id of this device ??? */
  int     sample_out_rate;                   /**< samples(frames) per second */
  volatile int     sample_in_rate;                   /**< samples(frames) per second */
  uint64_t    num_input_channels;            /**< number of input channels(1 is mono, 2 stereo etc..) */
  uint64_t    num_output_channels;           /**< number of output channels(1 is mono, 2 stereo etc..) */
  uint64_t    bytes_per_channel;

  uint64_t    num_calls;                     /**< count of process_audio() calls */

  jack_port_t     *output_port[JACK_MAX_OUTPUT_PORTS]; /**< output ports */
  jack_port_t     *input_port[JACK_MAX_INPUT_PORTS]; /**< input ports */
  jack_client_t   *client;                        /**< pointer to actual jack client */
  char *client_name; // our name for the client

  boolean started_server; /// TRUE if the server this client is connected to was started by it

  char             **jack_port_name;              /**< user given strings for the port names, can be NULL - unused*/
  unsigned int     jack_port_name_count;          /**< the number of port names given - unused*/
  uint64_t    jack_port_flags;               /**< flags to be passed to jack when opening the output ports - unused*/

  lives_audio_loop_t loop;  ///< playback loop mode

  jack_transport_state_t state;

  float     volume[JACK_MAX_OUTPUT_PORTS];      ///< amount volume, 1.0 is full volume

  boolean          in_use;                        /**< true if this device is currently in use */
  boolean mute;

  volatile aserver_message_t   *msgq;          /**< linked list of messages we are sending to the callback process */

  off_t seek_pos;
  volatile off_t real_seek_pos;
  off_t seek_end;
  boolean usigned;
  boolean reverse_endian;

  volatile lives_whentostop_t *whentostop; ///< pointer to mainw->whentostop
  volatile lives_cancel_t *cancelled; ///< pointer to mainw->cancelled

  /* variables used for trying to restart the connection to jack */
  boolean             jackd_died;                    /**< true if jackd has died and we should try to restart it */

  // TODO - use jack_opts ??
  boolean play_when_stopped; ///< if we should play audio even when jack transport is stopped

  volatile jack_nframes_t nframes_start;
  volatile uint64_t frames_written;

  int out_chans_available;
  int in_chans_available;

  boolean is_paused;

  boolean is_output; ///< is output FROM host to jack

  boolean is_silent;

  boolean is_active;

  int playing_file;   ///< clip number we are playing from or recording to, can differ from video clip

  //volatile float jack_pulse[1024];  // unused

  lives_audio_buf_t **abufs;    // ring buffer of audio buffers to read from / write to
  volatile int read_abuf;

  volatile int astream_fd;

  volatile float abs_maxvol_heard;

  char status_msg[STMSGLEN];
} jack_driver_t;

#define JACK_MAX_OUTDEVICES 10
#define JACK_MAX_INDEVICES 10

////////////////////////////////////////////////////////////////////////////
void jack_dump_metadata(void);

char *jack_parse_script(const char *fname);

jack_driver_t *jack_get_driver(int dev_idx, boolean is_output); ///< get driver

boolean jack_get_cfg_file(boolean is_trans, char **pserver_cfgx);

int jack_audio_init(void); ///< init jack for host outputy
int jack_audio_read_init(void); ///< init jack for host input

boolean jack_create_client_writer(jack_driver_t *); ///< open device for host output
boolean jack_create_client_reader(jack_driver_t *); ///< open device for host input

boolean jack_write_client_activate(jack_driver_t *);  ///< activate for host playback
boolean jack_read_client_activate(jack_driver_t *, boolean autocon);  ///< activate for host recording

void jack_close_client(jack_driver_t *);

boolean jack_try_reconnect(void);

void jack_aud_pb_ready(int fileno);
void jack_pb_end(void);

size_t jack_flush_read_data(size_t rbytes, void *data);

// utils
volatile aserver_message_t *jack_get_msgq(jack_driver_t *); ///< pull last msg from msgq, or return NULL
void jack_time_reset(jack_driver_t *, int64_t offset);
ticks_t lives_jack_get_time(jack_driver_t *); ///< get time from jack, in 10^-8 seconds
boolean jack_audio_seek_frame(jack_driver_t *, double frame);  ///< seek to (video) frame
int64_t jack_audio_seek_bytes(jack_driver_t *, int64_t bytes, lives_clip_t *sfile);  ///< seek to byte position
size_t jack_get_buffsize(jack_driver_t *);

void jack_get_rec_avals(jack_driver_t *);

ticks_t jack_transport_get_current_ticks(void);

double lives_jack_get_pos(jack_driver_t *);

#endif

#endif
