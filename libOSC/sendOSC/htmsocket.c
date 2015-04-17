/*
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

/* htmsocket.c

Adrian Freed
	send parameters to htm servers by udp or UNIX protocol

   Modified 6/6/96 by Matt Wright to understand symbolic host names
   in addition to X.X.X.X addresses.
*/


#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/param.h>

#ifndef IS_MINGW
#include <netinet/in.h>
#include <sys/time.h>

#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/times.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>

#else
#include <winsock2.h>
#include <ws2tcpip.h>


#include <fcntl.h>
#undef _S_IREAD
#undef _S_IWRITE

#define _S_IREAD 256
#define _S_IWRITE 128
int mkstemp(char *tmpl) {
  int ret=-1;
  mktemp(tmpl);
  ret=open(tmpl,O_RDWR|O_BINARY|O_CREAT|O_EXCL|_O_SHORT_LIVED, _S_IREAD|_S_IWRITE);
  return ret;
}

#endif

#if IS_SOLARIS
#include <sys/filio.h>
#endif


#include <ctype.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>

#include <stdlib.h>

#define UNIXDG_PATH "/tmp/htm"
#define UNIXDG_TMP "/tmp/htm.XXXXXX"
#include "htmsocket.h"


typedef struct {
  float srate;

  struct sockaddr_in serv_addr; /* udp socket */
#ifndef IS_MINGW
  struct sockaddr_un userv_addr; /* UNIX socket */
#endif
  int sockfd;		/* socket file descriptor */
  int index, len,uservlen;
  void *addr;
  int id;
} desc;


/* open a socket for HTM communication to given  host on given portnumber */
/* if host is 0 then UNIX protocol is used (i.e. local communication */

void *OpenHTMSocket(char *host, int portnumber) {
  //#ifndef IS_MINGW
  int sockfd;
  struct sockaddr_in  cl_addr;
#ifndef IS_MINGW
  struct sockaddr_un  ucl_addr;
#endif
  desc *o;
  o = malloc(sizeof(*o));
  if (!o)
    return 0;
#ifdef IS_MINGW
  if (!host) host="localhost";
#else
  if (!host) {
    int clilen;
    o->len = sizeof(ucl_addr);
    /*
     * Fill in the structure "userv_addr" with the address of the
     * server that we want to send to.
     */

    memset((char *) &o->userv_addr, 0, sizeof(o->userv_addr));
    o->userv_addr.sun_family = AF_UNIX;
    strcpy(o->userv_addr.sun_path, UNIXDG_PATH);
    sprintf(o->userv_addr.sun_path+strlen(o->userv_addr.sun_path), "%d", portnumber);
    o->uservlen = sizeof(o->userv_addr.sun_family) + strlen(o->userv_addr.sun_path);
    o->addr = &(o->userv_addr);
    /*
     * Open a socket (a UNIX domain datagram socket).
     */

    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) >= 0) {
      /*
       * Bind a local address for us.
       * In the UNIX domain we have to choose our own name (that
       * should be unique).  We'll use mktemp() to create a unique
       * pathname, based on our process id.
       */
      int dummy;

      memset((char *) &ucl_addr, 0, sizeof(ucl_addr));    /* zero out */
      ucl_addr.sun_family = AF_UNIX;
      strcpy(ucl_addr.sun_path, UNIXDG_TMP);

      dummy=mkstemp(ucl_addr.sun_path);
      dummy=dummy;
      clilen = sizeof(ucl_addr.sun_family) + strlen(ucl_addr.sun_path);

      if (bind(sockfd, (struct sockaddr *) &ucl_addr, clilen) < 0) {
        perror("client: can't bind local address");
        close(sockfd);
        sockfd = -1;
      }
    } else
      perror("unable to make socket\n");

  } else {
#endif
  /*
   * Fill in the structure "serv_addr" with the address of the
   * server that we want to send to.
   */
  o->len = sizeof(cl_addr);
  memset((char *)&o->serv_addr, 0, sizeof(o->serv_addr));
  o->serv_addr.sin_family = AF_INET;

  /* MW 6/6/96: Call gethostbyname() instead of inet_addr(),
  so that host can be either an Internet host name (e.g.,
  "les") or an Internet address in standard dot notation
  (e.g., "128.32.122.13") */
  {
    struct hostent *hostsEntry;
    unsigned long address;

    hostsEntry = gethostbyname(host);
    if (hostsEntry == NULL) {
      fprintf(stderr, "Couldn't decipher host name \"%s\"\n",
              host);
      return 0;
    }

    address = *((unsigned long *) hostsEntry->h_addr_list[0]);
    o->serv_addr.sin_addr.s_addr = address;
  }

  /* was: o->serv_addr.sin_addr.s_addr = inet_addr(host); */

  /* End MW changes */

  o->serv_addr.sin_port = htons(portnumber);
  o->addr = &(o->serv_addr);
  /*
   * Open a socket (a UDP domain datagram socket).
   */
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
    memset((char *)&cl_addr, 0, sizeof(cl_addr));
    cl_addr.sin_family = AF_INET;
    cl_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    cl_addr.sin_port = htons(0);

    if (bind(sockfd, (struct sockaddr *) &cl_addr, sizeof(cl_addr)) < 0) {
      perror("could not bind\n");
      close(sockfd);
      sockfd = -1;
    }
  } else {
    perror("unable to make socket\n");
  }
#ifndef IS_MINGW
}
#endif
if (sockfd<0) {
  free(o);
  o = 0;
} else
  o->sockfd = sockfd;
return o;
//#endif
return NULL;
}


#include <errno.h>

static  bool sendudp(const struct sockaddr *sp, int sockfd,int length, int count, void  *b) {
  int rcount;
  if ((rcount=sendto(sockfd, b, count, 0, sp, length)) != count) {
    /*	printf("sockfd %d count %d rcount %dlength %d errno %d\n", sockfd,count,rcount,length,
    			errno); */
    return FALSE;
  }
  return TRUE;
}
bool SendHTMSocket(void *htmsendhandle, int length_in_bytes, void *buffer) {
  //#ifndef IS_MINGW
  desc *o = (desc *)htmsendhandle;
  return sendudp(o->addr, o->sockfd, o->len, length_in_bytes, buffer);
  //#endif
  return FALSE;
}
void CloseHTMSocket(void *htmsendhandle) {
  //#ifndef IS_MINGW
  desc *o = (desc *)htmsendhandle;
  close(o->sockfd);
  free(o);
  //#endif
}
