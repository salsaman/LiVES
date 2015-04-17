// osc_notify.h
// LiVES (lives-exe)
// (c) G. Finch 2008 - 2010
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// this is a system for monitoring LiVES using OSC

// for example, LiVES can be started like: lives -oscstart 49999
// a client can then connect to UDP port 49999, and can ask LiVES to open a notify socket on UDP port 49997
//   sendOSC -host localhost 49999 /lives/open_notify_socket,49997
//
// LiVES will then send messages of the form:
//   msg_number|msg_string
// (msg_string may be of 0 length. The message is terminated with \n\0).
// when various events happen. The event types are enumerated below.
//

#ifndef HAS_LIVES_OSC_NOTIFY_H
#define HAS_LIVES_OSC_NOTIFY_H

/** \file osc_notify.h
 */


#ifdef __cplusplus
extern "C" {
#endif

#define LIVES_OSC_NOTIFY_FRAME_SYNCH 1 ///< sent when a frame is displayed
#define LIVES_OSC_NOTIFY_PLAYBACK_STARTED 2 ///< sent when a/v playback starts or clip is switched
#define LIVES_OSC_NOTIFY_PLAYBACK_STOPPED 3 ///< sent when a/v playback ends

/// sent when a/v playback ends and there is recorded data for
/// rendering/previewing
#define LIVES_OSC_NOTIFY_PLAYBACK_STOPPED_RD 4

#define LIVES_OSC_NOTIFY_RECORD_STARTED 32 ///< sent when record starts (TODO)
#define LIVES_OSC_NOTIFY_RECORD_STOPPED 33 ///< sent when record stops (TODO)

#define LIVES_OSC_NOTIFY_QUIT 64 ///< sent when app quits

#define LIVES_OSC_NOTIFY_CLIP_OPENED 128  ///< sent after a clip is opened
#define LIVES_OSC_NOTIFY_CLIP_CLOSED 129 ///< sent after a clip is closed

#define LIVES_OSC_NOTIFY_CLIPSET_OPENED 256 ///< sent after a clip set is opened
#define LIVES_OSC_NOTIFY_CLIPSET_SAVED 257 ///< sent after a clip set is closed

#define LIVES_OSC_NOTIFY_SUCCESS 512  ///< for OSC only (not for C++)
#define LIVES_OSC_NOTIFY_FAILED 1024  ///< for OSC only (not for C++)
#define LIVES_OSC_NOTIFY_CANCELLED 2048 ///< for OSC only (not for C++)

#define LIVES_OSC_NOTIFY_MODE_CHANGED 4096 ///< mode changed to clip editor or to multitrack

// >= 65536 reserved for custom


#ifdef __cplusplus
}
#endif

#endif
