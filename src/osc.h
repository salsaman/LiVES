// osc.h
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2012
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


/* some portions of this file based on libOSC
Written by Adrian Freed, The Center for New Music and Audio Technologies,
University of California, Berkeley.  Copyright (c) 1992,93,94,95,96,97,98,99,2000,01,02,03,04
The Regents of the University of California (Regents).

Permission to use, copy, modify, distribute, and distribute modified versions
of this software and its documentation without fee and without a signed
licensing agreement, is hereby granted, provided that the above copyright
notice, this paragraph and the following two paragraphs appear in all copies,
modifications, and distributions.

IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.


The OSC webpage is http://cnmat.cnmat.berkeley.edu/OpenSoundControl
*/

#ifdef ENABLE_OSC
#ifndef HAS_LIVES_OSC_H
#define HAS_LIVES_OSC_H

#ifndef Boolean
#define Boolean boolean
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <libOSC/libosc.h>
#include <libOSC/OSC-client.h>

#ifdef __cplusplus
}
#endif

typedef struct osc_arg_t {
  int a;
  int b;
  int c;
} osc_arg;

typedef struct lives_osc_t {
  struct OSCAddressSpaceMemoryTuner t;
  struct OSCReceiveMemoryTuner rt;
  struct OSCContainerQueryResponseInfoStruct cqinfo;
  struct OSCMethodQueryResponseInfoStruct ris;
  struct sockaddr_in cl_addr;
  int sockfd;
  int clilen;
  fd_set readfds;
  OSCcontainer container;
  OSCcontainer *leaves;
  OSCPacketBuffer packet;
  osc_arg *osc_args;
} lives_osc;

void lives_osc_free(lives_osc *o);
void lives_osc_dump();


boolean lives_osc_act(OSCbuf *msg);


#endif //HAS_LIVES_OSC_H
#endif //ENABLE_OSC
