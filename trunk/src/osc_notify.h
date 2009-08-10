// osc_notify.h
// LiVES (lives-exe)
// (c) G. Finch 2008
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// this is the start of a system for monitoring LiVES using OSC

// for example, LiVES can be started like: lives -oscstart 9999
// a client can then connect to UDP port 9999, and can ask LiVES to open a notify socket on UDP port 9997
//   sendOSC -host localhost 9999 /lives/open_notify_socket,9997
//
// LiVES will then send messages of the form:
//   msg_number<space>msg_string
// when various events happen. The event types are enumerated below. 
//

#ifndef _HAS_OSC_NOTIFY_H
#define _HAS_OSC_NOTIFY_H

#define LIVES_OSC_NOTIFY_FRAME_SYNCH 1 // sent when a frame is displayed
#define LIVES_OSC_NOTIFY_PLAYBACK_STARTED 2 // sent when a/v playback starts or clip is switched
#define LIVES_OSC_NOTIFY_PLAYBACK_STOPPED 3 // sent when a/v playback ends

#define LIVES_OSC_NOTIFY_QUIT 64 // sent when app quits

#define LIVES_OSC_NOTIFY_CLIP_OPENED 128  // TODO - msg_string starts with new clip number
#define LIVES_OSC_NOTIFY_CLIP_CLOSED 129


#define LIVES_OSC_NOTIFY_CLIPSET_OPENED 256 //msg_string starts with setname
#define LIVES_OSC_NOTIFY_CLIPSET_SAVED 257


#define LIVES_OSC_NOTIFY_SUCCESS 512
#define LIVES_OSC_NOTIFY_FAILED 1024
#define LIVES_OSC_NOTIFY_CANCELLED 2048


#endif
