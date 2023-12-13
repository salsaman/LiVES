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
  // remainder currently unused
  JACK_CLIENT_TYPE_MIDI,
  JACK_CLIENT_TYPE_TEMP,
  JACK_CLIENT_TYPE_OTHER,
} lives_jack_client_type;

typedef enum {
  JACK_PORT_TYPE_INVALID,
  JACK_PORT_TYPE_DEF_IN,
  JACK_PORT_TYPE_DEF_OUT,
  JACK_PORT_TYPE_AUX_IN,
  JACK_PORT_TYPE_OTHER,
} lives_jack_port_type;

// should really be lives_jack_client_t - TODO
typedef struct _lives_jack_driver_t jack_driver_t;

const char *jack_get_best_client(lives_jack_port_type type, LiVESList *clients);

const char **jack_get_inports(void);
const char **jack_get_outports(void);

LiVESList *jack_get_inport_clients(void);
LiVESList *jack_get_outport_clients(void);

void jack_conx_exclude(jack_driver_t *jackd_in, jack_driver_t *jackd_out, boolean disc);

// GUI functions

void jack_srv_startup_config(LiVESWidget *, livespointer type_data);

boolean jack_drivers_config(LiVESWidget *, livespointer ptype);

void jack_server_config(LiVESWidget *, lives_rfx_t *);

void show_jack_status(LiVESButton *, livespointer is_transp);

boolean jack_log_errmsg(jack_driver_t *, const char *errtxt);

boolean jack_interop_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod,
                              livespointer statep);

boolean jack_interop_cleanup(lives_obj_t *obj, void *jackd);

// connect client or start server
boolean lives_jack_init(lives_jack_client_type client_type, jack_driver_t *jackd);
boolean lives_jack_poll(void); /** poll function to check transport state */
void lives_jack_end(void);

// should be called with mainw->jackd_trans
void jack_transport_set_master(jack_driver_t *, boolean set);
void jack_transport_update(jack_driver_t *, double pbtime);
void jack_pb_start(jack_driver_t *, double pbtime);  /** start playback transport master */
void jack_pb_stop(jack_driver_t *); /** pause playback transport master */

void jack_transport_make_strict_slave(jack_driver_t *jackd, boolean set);
boolean is_transport_locked(void);

////////////////////////////////////////////////////////////////////////////
// Audio

#include "audio.h"

// mapping of jack_opts pref
#define JACK_OPTS_START_ASERVER		(1 << 4)     ///< start audio server if unable to connect

// transport options
#define JACK_OPTS_ENABLE_TCLIENT      	(1 << 10)     ///< enable transport client (global setting)
#define JACK_OPTS_START_TSERVER		(1 << 2)     ///< start transport server if unable to connect

#define JACK_OPTS_TRANSPORT_SLAVE	(1 << 0)   ///< jack can start/stop
#define JACK_OPTS_TRANSPORT_MASTER	(1 << 1)  ///< LiVES can start / stop (start and stop)

#define JACK_OPTS_TIMEBASE_START	(1 << 5)    ///< jack sets play start position
#define JACK_OPTS_TIMEBASE_LSTART	(1 << 9)    ///< LiVES sets play start position

// only one or other should be set
#define JACK_OPTS_TIMEBASE_SLAVE	(1 << 6)    ///< full timebase slave (position updates)
#define JACK_OPTS_TIMEBASE_MASTER	(1 << 7)    ///< full timebase master (position updates)

#define JACK_OPTS_STRICT_SLAVE		(1 << 3) ///< everything must be done via transport
#define JACK_OPTS_STRICT_MASTER		(1 << 11) ///< forcibly become master

// general options
#define JACK_OPTS_NO_READ_AUTOCON	(1 << 8)    ///< do not auto connect input ports
/// if unset, LiVES will, during playback with external audio, remove any direct connections
/// between its input ports and output ports. They will be restored after playback.

// conflicts if both clients want to start same server and vals differ
#define JACK_OPTS_PERM_ASERVER      	(1 << 16)     ///< leave audio srvr running even if we started
#define JACK_OPTS_PERM_TSERVER      	(1 << 17)     ///< leave transport running even if we started it

// only one or other should be set...
#define JACK_OPTS_SETENV_ASERVER      	(1 << 18)     ///< setenv $JACK_DEFAULT_SERVER from aserver_sname
#define JACK_OPTS_SETENV_TSERVER      	(1 << 19)     ///< setenv $JACK_DEFAULT_SERVER from tserver_sname

#define JACK_OPTS_OPTS_MASK ((1 << 24) - 1)

// bits 24 - 31 reserved for info flags

#define JACK_INFO_TEST_SETUP      	(1 << 29)     ///< setup is in test mode
#define JACK_INFO_TEMP_NAMES      	(1 << 30)     ///< -jackserver startup argument used
#define JACK_INFO_TEMP_OPTS      	(1 << 31)     ///< -jackopts startup argument used

#define JACK_MAX_PORTS 10

#define JACK_MAX_CHANNELS 1024

#define ERR_PORT_NOT_FOUND 10

#define JACK_DEFAULT_SERVER "JACK_DEFAULT_SERVER"
#define JACK_DEFAULT_SERVER_NAME "default"

#define JACK_SYSTEM_CLIENT "system"

#define JACKD_RC_NAME "jackdrc"

// custom transport states (not very useful, may remove)
#define JackTClosed 1024 // not activated
#define JackTReset 1025
#define JackTStopped 1026

#define STMSGLEN 65536

typedef struct _lives_jack_driver_t {
  int dev_idx;                      /**< id of this device ??? */
  lives_obj_instance_t *inst;
  int sample_out_rate;                   /**< samples(frames) per second */
  volatile int sample_in_rate;                   /**< samples(frames) per second */
  uint64_t num_input_channels;            /**< number of input channels(1 is mono, 2 stereo etc..) */
  uint64_t num_output_channels;           /**< number of output channels(1 is mono, 2 stereo etc..) */
  uint64_t bytes_per_channel;

  uint64_t num_calls;                     /**< count of process_audio() calls */

  jack_port_t *output_port[JACK_MAX_PORTS]; /**< output ports */
  jack_port_t *input_port[JACK_MAX_PORTS]; /**< input ports */
  jack_client_t *client;                        /**< pointer to actual jack client */
  char *client_name; // our name for the client

  boolean started_server; /// TRUE if the server this client is connected to was started by it

  uint64_t jack_port_flags;               /**< flags to be passed to jack when opening the output ports - unused*/

  lives_audio_loop_t loop;  ///< playback loop mode

  jack_transport_state_t state;

  float volume[JACK_MAX_PORTS];      ///< amount volume, 1.0 is full volume

  boolean in_use;                        /**< true if this device is currently in use */
  boolean mute;

  volatile aserver_message_t *msgq;          /**< linked list of messages we are sending to the callback process */

  ticks_t last_proc_ticks;

  off_t seek_pos;
  volatile off_t real_seek_pos;
  off_t seek_end;
  boolean usigned;
  boolean reverse_endian;

  volatile lives_whentostop_t *whentostop; ///< pointer to mainw->whentostop
  volatile lives_cancel_t *cancelled; ///< pointer to mainw->cancelled

  /* variables used for trying to restart the connection to jack */
  boolean jackd_died;                    /**< true if jackd has died and we should try to restart it */

  volatile jack_nframes_t nframes_start;
  volatile uint64_t frames_written, frames_read;

  int out_ports_available;
  int in_ports_available;

  boolean is_paused;

  int client_type;

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

#define JACK_MAX_OUTDEVICES 1
#define JACK_MAX_INDEVICES 1

////////////////////////////////////////////////////////////////////////////
void jack_dump_metadata(void);

boolean jack_warn(boolean is_trans, boolean is_con);

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

void jack_aud_pb_ready(jack_driver_t *, int fileno);
void jack_pb_end(void);

// utils
volatile aserver_message_t *jack_get_msgq(jack_driver_t *); ///< pull last msg from msgq, or return NULL
void jack_time_reset(jack_driver_t *, int64_t offset);
ticks_t lives_jack_get_time(jack_driver_t *); ///< get time from jack, in 10^-8 seconds

double lives_jack_get_timing_ratio(jack_driver_t *);

boolean jack_audio_seek_frame(jack_driver_t *, double frame);  ///< seek to (video) frame
boolean jack_audio_seek_frame_velocity(jack_driver_t *jackd, double frame, double vel);

int64_t jack_audio_seek_bytes(jack_driver_t *, int64_t bytes, lives_clip_t *sfile);  ///< seek to byte position

size_t jack_get_buffsize(jack_driver_t *);

void jack_set_avel(jack_driver_t *, int clipno, double ratio);

void jack_get_rec_avals(jack_driver_t *);

ticks_t jack_transport_get_current_ticks(jack_driver_t *);

double lives_jack_get_pos(jack_driver_t *);
off_t lives_jack_get_offset(jack_driver_t *);

void lives_jack_set_client_attributes(jack_driver_t *, int fileno, boolean activate, boolean running);

#endif

#endif
