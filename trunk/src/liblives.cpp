// liblives.cpp
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
#include "liblives.hpp"


extern "C" {

  int real_main(int argc, char *argv[], ulong id);

  uint64_t lives_random(void);

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


#include <sys/time.h>

#define SECONDS_FROM_1900_to_1970 2208988800 /* 17 leap years */
#define TWO_TO_THE_32_OVER_ONE_MILLION 4295


static OSCTimeTag OSCTT_CurrentTimex(void) {
  OSCTimeTag tag;
  
  uint64_t result;
  uint32_t usecOffset;
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  result = (unsigned) SECONDS_FROM_1900_to_1970 + 
    (unsigned) tv.tv_sec - 
    (unsigned) 60 * tz.tz_minuteswest +
    (unsigned) (tz.tz_dsttime ? 3600 : 0);

  tag.seconds = result;
    	
  usecOffset = (unsigned) tv.tv_usec * (unsigned) TWO_TO_THE_32_OVER_ONE_MILLION;

  tag.fraction = usecOffset;

  return tag;
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

  static list<livesAppCtx> appMgr;

  static livesApp *find_instance_for_id(ulong id) {
    list<livesAppCtx>::iterator it = appMgr.begin();
    while (it != appMgr.end()) {
      if ((*it).id == id) {
	return (*it).app;
      }
      ++it;
    }
    return NULL;
  }


  livesApp::livesApp() {
    char progname[] = "lives-exe";
    char *argv[]={progname};
    uint64_t id = lives_random();
    livesAppCtx ctx;
    ctx.id = id;
    ctx.app = this;
    appMgr.push_back(ctx);
    m_id = id;
    real_main(1, argv, id);
  }

  livesApp::livesApp(int argc, char *argv[]) {
    // TODO
    char progname[] = "lives-exe";
    argv[0]=progname;
    uint64_t id = lives_random();
    livesAppCtx ctx;
    ctx.id = id;
    ctx.app = this;
    appMgr.push_back(ctx);
    m_id = id;
    real_main(argc, argv, id);
  }


  livesApp::~livesApp() {
    int arglen = 1;
    char *vargs = strdup(",");
    arglen = padup(&vargs, arglen);
    
    list<closure *>::iterator it = m_closures.begin();
    while (it != m_closures.end()) {
      delete *it;
      m_closures.erase(it++);
    }
    lives_osc_cb_quit(NULL, arglen, vargs, OSCTT_CurrentTime(), NULL);
  }


  set livesApp::currentSet() {
    m_set.setName(get_set_name());
    return m_set;
  }


  bool livesApp::addCallback(int msgnum, modeChanged_callback_f func, void *data) {
    if (msgnum != LIVES_NOTIFY_MODE_CHANGED) return false;
    closure *cl = new closure;
    cl->msgnum = msgnum;
    cl->func = (callback_f)func;
    cl->data = data;
    m_closures.push_back(cl);
    return true;
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

  list<closure*> livesApp::closures() {
    return m_closures;
  }





  char *set::name() {
    return m_name;
  }

  void set::setName(const char *name) {
    char noname[] = "";
    if (m_name != NULL) free(m_name);
    if (name == NULL) m_name = strdup(noname);
    else m_name = strdup(name);
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


void binding_cb (int msgnumber, const char *msgstring, uint64_t id) {
  bool ret;
  lives::livesApp *lapp = lives::find_instance_for_id(id);
  list <lives::closure *> cl = lapp->closures();

  list<lives::closure *>::iterator it = cl.begin();
  while (it != cl.end()) {

    if ((*it)->msgnum == msgnumber) {
      switch (msgnumber) {
      case LIVES_NOTIFY_MODE_CHANGED:
	lives::modeChangedInfo info;
	info.mode = atoi(msgstring);
	ret = ((*it)->func)(&info, (*it)->data);
	break;
      default:
	continue;
      }
      if (!ret) {
	cl.erase(it++);
	continue;
      }
    }
    ++it;
  }
}

