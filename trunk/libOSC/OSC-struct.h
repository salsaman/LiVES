/*
Copyright © 1998. The Regents of the University of California (Regents).
All Rights Reserved.

Written by Matt Wright, The Center for New Music and Audio Technologies,
University of California, Berkeley.

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

The OpenSound Control WWW page is
    http://www.cnmat.berkeley.edu/OpenSoundControl
*/

#ifndef IS_MINGW
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <sys/types.h>


#ifndef OSCH
#define OSCH
#ifndef TRUE
typedef int Boolean;
#define TRUE 1
#define FALSE 0
#endif


/* Fixed byte width types */
typedef int int4;   /* 4 byte int */
typedef struct NetworkReturnAddressStruct_t {
  struct sockaddr_in  cl_addr; /* client information */
  struct sockaddr_in  my_addr; /* us */
  int clilen;
  int sockfd;
  fd_set readfds;
  struct timeval tv;
  int fdmax;
} NetworkReturnAddressStruct;


typedef struct OSCPacketBuffer_struct {
  char *buf;			/* Contents of network packet go here */
  int n;			/* Overall size of packet */
  int refcount;		/* # queued things using memory from this buffer */
  struct OSCPacketBuffer_struct *nextFree;	/* For linked list of free packets */

  Boolean returnAddrOK;       /* Because returnAddr points to memory we need to
				   store future return addresses, we set this
				   field to FALSE in situations where a packet
				   buffer "has no return address" instead of
				   setting returnAddr to 0 */

  void *returnAddr;	/* Addr of client this packet is from */
  /* This was of type NetworkReturnAddressPtr, but the constness
           was making it impossible for me to initialize it.  There's
     probably a better way that I don't understand. */

} OSCPacketBuffer;

struct OSCReceiveMemoryTuner {
  void *(*InitTimeMemoryAllocator)(int numBytes);
  void *(*RealTimeMemoryAllocator)(int numBytes);
  int receiveBufferSize;
  int numReceiveBuffers;
  int numQueuedObjects;
  int numCallbackListNodes;
};

#endif
