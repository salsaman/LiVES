// liblives.cpp
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

extern "C" {
#include <libOSC/libosc.h>
#include <libOSC/OSC-client.h>
#include "main.h"
#include "lbindings.h"
}


#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <iostream>

#define HAVE_OSC_TT
#include "liblives.hpp"



extern "C" {
  int real_main(int argc, char *argv[], ulong id);

  bool lives_osc_cb_quit(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_play(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_fgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_start(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_saveset(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);

}



static int pad4(int val) {
  return (int)((val+4)/4)*4;
}


static int padup(char **str, int arglen) {
  int newlen = pad4(arglen);
  char *ostr = *str;
  *str = (char *)lives_calloc(1,newlen);
  lives_memcpy(*str, ostr, arglen);
  lives_free(ostr);
  return newlen;
}


static int add_int_arg(char **str, int arglen, int val) {
  int newlen = arglen + 4;
  char *ostr = *str;
  *str = (char *)lives_calloc(1,newlen);
  lives_memcpy(*str, ostr, arglen);
  if (!is_big_endian()) {
    (*str)[arglen] = (unsigned char)((val&0xFF000000)>>3);
    (*str)[arglen+1] = (unsigned char)((val&0x00FF0000)>>2);
    (*str)[arglen+2] = (unsigned char)((val&0x0000FF00)>>1);
    (*str)[arglen+3] = (unsigned char)(val&0x000000FF);
  }
  else {
    lives_memcpy(*str + arglen, &val, 4);
  }
  lives_free(ostr);
  return newlen;
}


static int add_string_arg(char **str, int arglen, const char *val) {
  int newlen = arglen + strlen(val) + 1;
  char *ostr = *str;
  *str = (char *)lives_calloc(1,newlen);
  lives_memcpy(*str, ostr, arglen);
  lives_memcpy(*str + arglen, val, strlen(val));
  lives_free(ostr);
  return newlen;
}


static void *play_thread(void *) {
  int arglen = 1;
  char **vargs=(char **)lives_malloc(sizeof(char *));
  *vargs = strdup(",");
  arglen = padup(vargs, arglen);
  lives_osc_cb_play(NULL, arglen, (const void *)(*vargs), OSCTT_CurrentTime(), NULL);
  lives_free(*vargs);
  return NULL;
}

static volatile bool spinning;
static ulong blocking_id;
static char *private_response;

static bool private_cb(lives::privateInfo *info, void *data) {
  if (info->id == blocking_id) {
    private_response = strdup(info->response);
    spinning = false;
    return false;
  }

  return true;
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


  void livesApp::init(int argc, char *oargv[]) {
    char **argv;
    char progname[] = "lives-exe";
    if (argc < 0) argc=0;
    argc++;

    argv=(char **)malloc(argc * sizeof(char *));
    argv[0]=strdup(progname);

    for (int i=1; i < argc; i++) {
      argv[i]=strdup(oargv[i-1]);
    }

    uint64_t id = lives_random();
    livesAppCtx ctx;

    ctx.id = id;
    ctx.app = this;
    appMgr.push_back(ctx);

    m_id = id;
    real_main(argc, argv, id);
    free(argv);
  }


  livesApp::livesApp() : m_set(this) {
    if (appMgr.empty())
      init(0,NULL);
    else 
      m_id = 0;
  }

  livesApp::livesApp(int argc, char *argv[]) : m_set(this) {
    if (appMgr.empty())
      init(argc,argv);
    else 
      m_id = 0;
  }


  livesApp::~livesApp() {
    if (!m_id) return;

    int arglen = 1;
    char **vargs=(char **)lives_malloc(sizeof(char *));
    *vargs = strdup(",");
    arglen = padup(vargs, arglen);

    // call object destructor callback
    binding_cb (LIVES_NOTIFY_OBJECT_DESTROYED, NULL, (uint64_t)this);
    
    list<closure *>::iterator it = m_closures.begin();
    while (it != m_closures.end()) {
      delete *it;
      m_closures.erase(it++);
    }

    appMgr.clear();

    lives_osc_cb_quit(NULL, arglen, (const void *)(*vargs), OSCTT_CurrentTime(), NULL);
    lives_free(*vargs);
  }


  set livesApp::currentSet() {
    return m_set;
  }

  void livesApp::appendClosure(int msgnum, callback_f func, void *data) {
    closure *cl = new closure;
    cl->object = this;
    cl->msgnum = msgnum;
    cl->func = (callback_f)func;
    cl->data = data;
    m_closures.push_back(cl);
  }

  bool livesApp::addCallback(int msgnum, modeChanged_callback_f func, void *data) {
    if (msgnum != LIVES_NOTIFY_MODE_CHANGED) return false;
    appendClosure(msgnum, (callback_f)func, data);
    return true;
  }


  bool livesApp::addCallback(int msgnum, private_callback_f func, void *data) {
    if (msgnum != LIVES_NOTIFY_PRIVATE) return false;
    appendClosure(msgnum, (callback_f)func, data);
    return true;
  }

  bool livesApp::addCallback(int msgnum, objectDestroyed_callback_f func, void *data) {
    if (msgnum != LIVES_NOTIFY_OBJECT_DESTROYED) return false;
    appendClosure(msgnum, (callback_f)func, data);
    return true;
  }


  void livesApp::play() {
    if (!m_id) return;
    play_thread(NULL);
  }

  bool livesApp::stop() {
    if (!m_id) return FALSE;
    // return false if we are not playing
    return lives_osc_cb_stop(NULL, 0, NULL, OSCTT_CurrentTime(), NULL);
  }


  int livesApp::showInfo(const char *text, bool blocking) {
    if (!m_id) return 0;
    // if blocking wait for response
    if (blocking) {
      spinning = true;
      blocking_id = lives_random();
      addCallback(LIVES_NOTIFY_PRIVATE, private_cb, NULL); 
      idle_show_info(text,blocking,blocking_id);
      while (spinning) usleep(100);
      int ret = atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    idle_show_info(text,blocking,0);
    return 0;
  }

  char *livesApp::chooseFileWithPreview(const char *dirname, const char *title, int preview_type) {
    spinning = true;
    blocking_id = lives_random();
    addCallback(LIVES_NOTIFY_PRIVATE, private_cb, NULL); 
    idle_choose_file_with_preview(dirname,title,preview_type,blocking_id);
    while (spinning) usleep(100);
    char *ret = strdup(private_response);
    lives_free(private_response);
    return ret;
  }


  char *livesApp::chooseFileWithPreview(const char *dirname, int preview_type) {
    return chooseFileWithPreview(dirname,NULL,preview_type);
  }

  clip *livesApp::openFile(const char *fname, double stime, int frames) {
    if (fname == NULL) return NULL;
    spinning = true;
    blocking_id = lives_random();
    addCallback(LIVES_NOTIFY_PRIVATE, private_cb, NULL); 
    idle_open_file(fname, stime, frames, blocking_id);
    while (spinning) usleep(100);
    ulong cid = strtoul(private_response, NULL, 10);
    lives_free(private_response);
    clip *c = NULL;
    if (cid != 0l) {
      c = new clip(cid);
    }
    return c;
  }


  list<closure*> livesApp::closures() {
    return m_closures;
  }


  //////////////// set ////////////////////

  set::set(livesApp *lives) {
    m_lives = lives;
    m_name = NULL;
  }

  set::~set() {
    if (m_name != NULL) lives_free(m_name);

    clipList::iterator it = m_clips.begin();
    while (it != m_clips.end()) {
      delete *it;
      m_clips.erase(it++);
    }
  }

  char *set::name() {
    setName(get_set_name());
    return m_name;
  }

  void set::setName(const char *name) {
    char noname[] = "";
    if (m_name != NULL) {
      if (!strcmp(m_name, name)) return;
      lives_free(m_name);
    }
    if (name == NULL) m_name = strdup(noname);
    else m_name = strdup(name);
  }

  bool set::save(const char *name) {
    save(name, false);
  }


  bool set::save(const char *name, bool force_append) {
    int arglen = 3;
    char **vargs=(char **)lives_malloc(sizeof(char *));
    *vargs = strdup(",si");
    arglen = padup(vargs, arglen);
    arglen = add_string_arg(vargs, arglen, name);
    arglen = add_int_arg(vargs, arglen, force_append);

    spinning = true;
    blocking_id = lives_random();

    m_lives->addCallback(LIVES_NOTIFY_PRIVATE, private_cb, NULL); 

    idle_save_set(name,arglen,(const void *)(*vargs),blocking_id);

    while (spinning) usleep(100);
    lives_free(*vargs);
    bool ret = (bool)(atoi(private_response));
    lives_free(private_response);
    return ret;
  }


  clipList set::cliplist() {
    ulong *ids = get_unique_ids();
    // clear old cliplist

    clipList::iterator it = m_clips.begin();
    while (it != m_clips.end()) {
      delete *it;
      m_clips.erase(it++);
    }

    for (int i=0; ids[i] != 0l; i++) {
      clip *c = new clip(ids[i]);
      m_clips.push_back(c);
    }
    lives_free(ids);
    return m_clips;
  }


  /////////////// clip ////////////////

  clip::clip(uint64_t uid) {
    m_uid = uid;
  }

  int clip::frames() {
    int cnum = cnum_for_uid(m_uid);
    if (mainw->files[cnum]!=NULL) return mainw->files[cnum]->frames;
    return -1;
  }
  
  bool clip::select() {
    bool ret = false;
    int arglen = 2;
    int cnum = cnum_for_uid(m_uid);
    char **vargs=(char **)lives_malloc(sizeof(char *));
    *vargs = strdup(",i");

    if (cnum > 0) {
      arglen = padup(vargs, arglen);
      arglen = add_int_arg(vargs, arglen, cnum);
      ret = lives_osc_cb_fgclip_select(NULL, arglen, (const void *)(*vargs), OSCTT_CurrentTime(), NULL);
    }

    lives_free(vargs);
    return ret;
  }


  namespace prefs {
    const char *currentVideoLoadDir() {
      return mainw->vid_load_dir;
    }


  }

}


void binding_cb (int msgnumber, const char *msgstring, uint64_t id) {
  bool ret;
  lives::livesApp *lapp;

  if (msgnumber == LIVES_NOTIFY_OBJECT_DESTROYED) lapp = (lives::livesApp *)id;
  else lapp = lives::find_instance_for_id(id);

  if (lapp == NULL) return;

  list <lives::closure *> cl = lapp->closures();

  list<lives::closure *>::iterator it = cl.begin();
  while (it != cl.end()) {

    if ((*it)->msgnum == msgnumber) {
      switch (msgnumber) {
      case LIVES_NOTIFY_MODE_CHANGED:
	{
	  lives::modeChangedInfo info;
	  info.mode = atoi(msgstring);
	  lives::modeChanged_callback_f fn = (lives::modeChanged_callback_f)((*it)->func);
	  ret = (fn)((*it)->object, &info, (*it)->data);
	}
	break;
      case LIVES_NOTIFY_OBJECT_DESTROYED:
	{
	  lives::objectDestroyed_callback_f fn = (lives::objectDestroyed_callback_f)((*it)->func);
	  ret = (fn)((*it)->object, (*it)->data);
	}
	break;
      case LIVES_NOTIFY_PRIVATE:
	{
	  // private event type
	  lives::privateInfo info;
	  char **msgtok = lives_strsplit(msgstring, " ", -1);
	  info.id = strtoul(msgtok[0],NULL,10);
	  if (get_token_count(msgstring,' ')==1) info.response=NULL;
	  else info.response = strdup(msgtok[1]);
	  lives_strfreev(msgtok);
	  lives::private_callback_f fn = (lives::private_callback_f)((*it)->func);
	  ret = (fn)(&info, (*it)->data);
	}
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

