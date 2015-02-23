// bindings.cpp
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

extern "C" {
#include <libOSC/libosc.h>
#include <libOSC/OSC-client.h>
}

#include <string.h>
#include <stdlib.h>

#define HAVE_OSC_TT
#include "bindings.hpp"



extern "C" {
  int real_main(int argc, char *argv[]);
  bool lives_osc_cb_quit(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_fgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_start(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
}


static int pad4(int val) {
  return (int)((val+4)/4)*4;
}


static int padup(char **str, int arglen) {
  int newlen = pad4(arglen);
  char *ostr = *str;
  *str = (char *)calloc(1,newlen);
  memcpy(*str, ostr, arglen);
  free(ostr);
  return newlen;
}

static int add_int_arg(char **str, int arglen, int val) {
  int newlen = arglen + 4;
  char *ostr = *str;
  *str = (char *)calloc(1,newlen);
  if (!is_big_endian()) {
    *str[arglen] = (unsigned char)((val&0xFF000000)>>3);
    *str[arglen+1] = (unsigned char)((val&0x00FF0000)>>2);
    *str[arglen+2] = (unsigned char)((val&0x0000FF00)>>1);
    *str[arglen+3] = (unsigned char)(val&0x000000FF);
  }
  else {
    memcpy(*str + arglen, &val, 4);
  }
  free(ostr);
  return newlen;
}


//////////////////////////////////////////////////

//using namespace lives;

  livesApp::livesApp() {
    char *argv[1]={"lives-exe"};
    real_main(1, argv);
  }

  livesApp::livesApp(int argc, char *argv[]) {
    real_main(argc, argv);
  }


  livesApp::~livesApp() {
    lives_osc_cb_quit(NULL, 0, NULL, OSCTT_CurrentTime(), NULL);
  }


  list<clip *> livesApp::clipList() {

  }


  bool clip::select(int cnum) {
    int arglen = 2;
    char *vargs = strdup(",i");
    arglen = padup(&vargs, arglen);
    arglen = add_int_arg(&vargs, arglen, cnum);
    bool ret = lives_osc_cb_fgclip_select(NULL, arglen, (const void *)vargs, OSCTT_CurrentTime(), NULL);
    free(vargs);
    return ret;
  }




  bool record::enable() {
    return lives_osc_record_start(NULL, 0, NULL, OSCTT_CurrentTime(), NULL);
  }

  bool record::disable() {
    int arglen = 0;
    const char *vargs = NULL;
    return lives_osc_record_stop(NULL, arglen, (const void *)vargs, OSCTT_CurrentTime(), NULL);
  }

  bool record::toggle() {
    int arglen = 0;
    const char *vargs = NULL;
    return lives_osc_record_toggle(NULL, arglen, (const void *)vargs, OSCTT_CurrentTime(), NULL);
  }



