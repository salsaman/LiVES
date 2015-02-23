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
#include <pthread.h>

#include <iostream>

#define HAVE_OSC_TT
#include "bindings.hpp"


extern "C" {

  OSCTimeTag OSCTT_CurrentTime(void);
  int real_main(int argc, char *argv[]);

  bool lives_osc_cb_quit(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_play(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_fgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_start(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);

  void idle_show_info(const char *text, bool blocking);


  const char *get_set_name();
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

static void *play_thread(void *) {
  int arglen = 1;
  char *vargs = strdup(",");
  arglen = padup(&vargs, arglen);
  lives_osc_cb_play(NULL, arglen, vargs, OSCTT_CurrentTime(), NULL);
  return NULL;
}



//////////////////////////////////////////////////

namespace lives {

  livesApp::livesApp() {
    char progname[] = "lives-exe";
    char *argv[]={progname};
    real_main(1, argv);
  }

  livesApp::livesApp(int argc, char *argv[]) {
    real_main(argc, argv);
  }


  livesApp::~livesApp() {
    OSCTimeTag t;
    t.seconds = t.fraction = 0;
    lives_osc_cb_quit(NULL, 0, NULL, t, NULL);
  }


  set livesApp::currentSet() {
    m_set.setName(get_set_name());
    return m_set;
  }


  void livesApp::play() {
    // pthread_t playthread;
    //pthread_create(&playthread, NULL, play_thread, NULL);
    play_thread(NULL);
  }

  bool livesApp::stop() {
    // return false if we are not playing
    return lives_osc_cb_stop(NULL, 0, NULL, OSCTT_CurrentTime(), NULL);
  }


  void livesApp::showInfo(const char *text, bool blocking) {
    idle_show_info(text,blocking);
    // TODO - if blocking wait for response

  }

  char *set::name() {
    return m_name;
  }

  void set::setName(const char *name) {
    if (m_name != NULL) free(m_name);
    m_name = strdup(name);
  }

  set::~set() {
    if (m_name != NULL) free(m_name);
  }


  bool clip::select(int cnum) {
    OSCTimeTag t;
    t.seconds = t.fraction = 0;
    int arglen = 2;
    char *vargs = strdup(",i");
    arglen = padup(&vargs, arglen);
    arglen = add_int_arg(&vargs, arglen, cnum);
    bool ret = lives_osc_cb_fgclip_select(NULL, arglen, (const void *)vargs, OSCTT_CurrentTime(), NULL);
    free(vargs);
    return ret;
  }






  bool record::enable() {
    OSCTimeTag t;
    t.seconds = t.fraction = 0;
    return lives_osc_record_start(NULL, 0, NULL, t, NULL);
  }

  bool record::disable() {
    OSCTimeTag t;
    t.seconds = t.fraction = 0;
    int arglen = 0;
    const char *vargs = NULL;
    return lives_osc_record_stop(NULL, arglen, (const void *)vargs, t, NULL);
  }

  bool record::toggle() {
    OSCTimeTag t;
    t.seconds = t.fraction = 0;
    int arglen = 0;
    const char *vargs = NULL;
    return lives_osc_record_toggle(NULL, arglen, (const void *)vargs, t, NULL);
  }



}


void binding_cb (int msgnumber,const char *msgstring) {
  //cout << "GOT CB " << msgnumber << " " << msgstring;
}

