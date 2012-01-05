// htmsocket.c
// LiVES
// (c) G. Finch 2008 - 2012  <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#if defined(__sun)
#include <sys/filio.h>
#endif

#include "main.h"
#include "support.h"

#define UNIXDG_PATH "/tmp/lives"
#define UNIXDG_TMP "/tmp/lives.XXXXXX"
#include "htmsocket.h"                          

typedef struct {
  struct sockaddr_in serv_addr;
  int sockfd;
  int len;
#ifdef __cplusplus
  sockaddr *addr;
#else
  void *addr;
#endif
} desc;


void *OpenHTMSocket(const char *host, int portnumber, gboolean sender) {
  int sockfd;
  struct sockaddr_in cl_addr;
  desc *o;
  struct hostent *hostsEntry;
  uint64_t address=0;

  o = (desc *)malloc(sizeof(desc));
  if (o==NULL) return NULL;

  o->len = sizeof(cl_addr);
  memset((char *)&o->serv_addr, 0, sizeof(o->serv_addr));
  o->serv_addr.sin_family = AF_INET;
  
  if (strcmp(host,"INADDR_ANY")) {
    hostsEntry = gethostbyname(host);
    
    if (hostsEntry == NULL) {
      herror(NULL);
      return NULL;
    }
    
    address = *((uint64_t *) hostsEntry->h_addr_list[0]);
  }

  if (sender) {
    // open sender socket
    o->serv_addr.sin_addr.s_addr = address;
    o->serv_addr.sin_port = htons(portnumber);
  }
  else {
    // open receiver socket
    if (!strcmp(host,"INADDR_ANY")) o->serv_addr.sin_addr.s_addr = INADDR_ANY;
    else o->serv_addr.sin_addr.s_addr = address;
    o->serv_addr.sin_port = htons(0);
  }

#ifdef __cplusplus
  o->addr = (sockaddr *)&(o->serv_addr);
#else
  o->addr = &(o->serv_addr);
#endif

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
    memset((char *)&cl_addr, 0, sizeof(cl_addr));
    cl_addr.sin_family = AF_INET;
    if (sender) {
      // bind on all interfaces, any port
      cl_addr.sin_addr.s_addr = INADDR_ANY;
      cl_addr.sin_port = htons(0);
    }
    else {
      // bind on all interfaces, specified port
      cl_addr.sin_addr.s_addr = INADDR_ANY;
      cl_addr.sin_port = htons(portnumber);
    }
    if (bind(sockfd, (struct sockaddr *) &cl_addr, sizeof(cl_addr)) < 0) {
      g_printerr("could not bind\n");
      close(sockfd);
      sockfd = -1;
    }
  }
  else g_printerr("unable to make socket\n");

  if(sockfd<0) {
    g_free(o);
    o = NULL;
  }
  else {
    int mxsize=1024*1024;
    o->sockfd = sockfd;
    if (!sender) setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void *) &mxsize, sizeof(mxsize));
  }

  if (o!=NULL&&strcmp(host,"INADDR_ANY")) {
    connect(sockfd, o->addr, sizeof(cl_addr));
  }

  return o;
}

static ssize_t getudp(struct sockaddr *sp, int sockfd, int length, size_t count, void  *b, int bfsize) {
  int flags=0;
  ssize_t res;
  int len;

  if (bfsize>0) {
    int xbfsize;
    socklen_t slt=sizeof(xbfsize);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void *) &bfsize, sizeof(bfsize));
    getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void *) &xbfsize, &slt);
    if (xbfsize<bfsize) return -2;
  }

  ioctl(sockfd,FIONREAD,&len);
  if (len==0) return -1;

  do {
    res=recvfrom(sockfd, b, count, flags, sp, (socklen_t *)&length);
    //g_print("res is %d\n",res);
  } while (res==-1);

  return res;
}

static gboolean sendudp(const struct sockaddr *sp, int sockfd, int length, size_t count, void  *b) {
  size_t rcount;
  if ((rcount=sendto(sockfd, b, count, 0, sp, length)) != count) {
    //printf("sockfd %d count %d rcount %dlength %d errno %d\n", sockfd,count,rcount,length,errno);
    return FALSE;
  }
  return TRUE;
}

gboolean lives_stream_out(void *htmsendhandle, size_t length, void *buffer) {
  desc *o = (desc *)(htmsendhandle);
  return sendudp(o->addr, o->sockfd, o->len, length, buffer);
}

ssize_t lives_stream_in(void *htmrecvhandle, size_t length, void *buffer, int bfsize) {
  desc *o = (desc *)htmrecvhandle;
#ifdef __cplusplus
  return getudp((sockaddr *)o->addr, o->sockfd, o->len, length, buffer, bfsize);
#else
  return getudp(o->addr, o->sockfd, o->len, length, buffer, bfsize);
#endif
}

void CloseHTMSocket(void *htmsendhandle) {
  desc *o = (desc *)htmsendhandle;
  close(o->sockfd);
  g_free(o);
}
