// liblives.cpp
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

/** \file liblives.cpp
    liblives interface
 */

#ifndef DOXYGEN_SKIP
extern "C" {
#include <libOSC/libosc.h>
#include <libOSC/OSC-client.h>
#include "main.h"
#include "lbindings.h"
}


#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <iostream>


#include "liblives.hpp"

extern "C" {
  int real_main(int argc, char *argv[], ulong id);

  bool is_big_endian(void);

  bool lives_osc_cb_quit(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_play(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_fgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_start(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_record_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_saveset(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);

}

inline int pad4(int val) {
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


static bool play_thread() {
  int arglen = 1;
  char **vargs=(char **)lives_malloc(sizeof(char *));
  *vargs = strdup(",");
  arglen = padup(vargs, arglen);
  bool ret = lives_osc_cb_play(NULL, arglen, (const void *)(*vargs), OSCTT_CurrentTime(), NULL);
  lives_free(*vargs);
  return ret;
}

static volatile bool spinning;
static ulong msg_id;
static char *private_response;

static bool private_cb(lives::_privateInfo *info, void *data) {
  if (info->id == msg_id) {
    private_response = strdup(info->response);
    spinning = false;
    return false;
  }

  return true;
}

#endif // doxygen_skip

//////////////////////////////////////////////////

namespace lives {

#ifndef DOXYGEN_SKIP
  typedef struct {
    ulong id;
    livesApp *app;
  } livesAppCtx;

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

#endif

  void LiVESString::setEncoding(lives_char_encoding_t enc) {
    m_encoding = enc;
  }

  lives_char_encoding_t LiVESString::encoding() {
    return m_encoding;
  }

  LiVESString LiVESString::toEncoding(lives_char_encoding_t enc) {
    if (enc == LIVES_CHAR_ENCODING_UTF8) {
      if (m_encoding == LIVES_CHAR_ENCODING_LOCAL8BIT) {
	LiVESString str(L2U8(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_UTF8);
	return str;
      }
#ifndef IS_MINGW
      else if (m_encoding == LIVES_CHAR_ENCODING_FILESYSTEM) {
	LiVESString str(F2U8(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_UTF8);
	return str;
      }
#endif
    }
    else if (enc == LIVES_CHAR_ENCODING_FILESYSTEM) {
#ifndef IS_MINGW
      if (m_encoding == LIVES_CHAR_ENCODING_UTF8) {
	LiVESString str(U82F(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_FILESYSTEM);
	return str;
      }
#else
      if (m_encoding == LIVES_CHAR_ENCODING_LOCAL8BIT) {
	LiVESString str(U82L(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_FILESYSTEM);
	return str;
      }
#endif
    }
    else if (enc == LIVES_CHAR_ENCODING_LOCAL8BIT) {
      if (m_encoding == LIVES_CHAR_ENCODING_UTF8) {
	LiVESString str(U82L(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_LOCAL8BIT);
	return str;
      }
#ifndef IS_MINGW
      if (m_encoding == LIVES_CHAR_ENCODING_FILESYSTEM) {
	LiVESString str(F2U8(this->c_str()));
	str.assign(U82L(str.c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_LOCAL8BIT);
	return str;
      }
#endif
    }
    return *this;
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

    ulong id = lives_random();
    livesAppCtx ctx;

    ctx.id = id;
    ctx.app = this;
    appMgr.push_back(ctx);

    m_set = new set(this);
    m_player = new player(this);
    m_effectKeyMap = new effectKeyMap(this);

    real_main(argc, argv, id);
    free(argv);
    m_id = id;
  }


  livesApp::livesApp() : m_id(0l) {
    if (appMgr.empty())
      init(0,NULL);
  }

  livesApp::livesApp(int argc, char *argv[]) : m_id(0l) {
    if (appMgr.empty())
      init(argc,argv);
  }


  livesApp::~livesApp() {
    if (!isValid()) return;

    int arglen = 1;
    char **vargs=(char **)lives_malloc(sizeof(char *));
    *vargs = strdup(",");
    arglen = padup(vargs, arglen);

    // call object destructor callback
    binding_cb (LIVES_CALLBACK_OBJECT_DESTROYED, NULL, (ulong)this);
    
    closureListIterator it = m_closures.begin();
    while (it != m_closures.end()) {
      delete *it;
      it = m_closures.erase(it);
    }

    appMgr.clear();

    lives_osc_cb_quit(NULL, arglen, (const void *)(*vargs), OSCTT_CurrentTime(), NULL);
    lives_free(*vargs);
  }


  bool livesApp::isValid() {
    return m_id != 0l;
  }


  const set& livesApp::currentSet() {
    return *m_set;
  }


  const player& livesApp::currentPlayer() {
    return *m_player;
  }


  ulong livesApp::appendClosure(lives_callback_t cb_type, callback_f func, void *data) {
    closure *cl = new closure;
    cl->id = lives_random();
    cl->object = this;
    cl->cb_type = cb_type;
    cl->func = (callback_f)func;
    cl->data = data;
    m_closures.push_back(cl);
    return cl->id;
  }

  void livesApp::setClosures(closureList cl) {
    m_closures = cl;
  }


  ulong livesApp::addCallback(lives_callback_t cb_type, modeChanged_callback_f func, void *data) {
    if (cb_type != LIVES_CALLBACK_MODE_CHANGED) return 0l;
    return appendClosure(cb_type, (callback_f)func, data);
  }

  ulong livesApp::addCallback(lives_callback_t cb_type, private_callback_f func, void *data) {
    if (cb_type != LIVES_CALLBACK_PRIVATE) return 0l;
    return appendClosure(cb_type, (callback_f)func, data);
  }

  ulong livesApp::addCallback(lives_callback_t cb_type, objectDestroyed_callback_f func, void *data) {
    if (cb_type != LIVES_CALLBACK_OBJECT_DESTROYED) return 0l;
    return appendClosure(cb_type, (callback_f)func, data);
  }

  ulong livesApp::addCallback(lives_callback_t cb_type, appQuit_callback_f func, void *data) {
    if (cb_type != LIVES_CALLBACK_APP_QUIT) return 0l;
    return appendClosure(cb_type, (callback_f)func, data);
  }

  bool livesApp::removeCallback(ulong id) {
    closureListIterator it = m_closures.begin();
    while (it != m_closures.end()) {
      if ((*it)->id == id) {
	delete *it;
	m_closures.erase(it);
	return true;
      }
      ++it;
    }
    return false;
  }


  lives_dialog_response_t livesApp::showInfo(LiVESString text, bool blocking) {
    lives_dialog_response_t ret=LIVES_DIALOG_RESPONSE_INVALID;
    if (!isValid()) return ret;
    // if blocking wait for response
    if (blocking) {
      spinning = true;
      msg_id = lives_random();
      ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
      if (!idle_show_info(text.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(),blocking,msg_id)) {
	spinning = false;
	removeCallback(cbid);
      }
      else {
	while (spinning) usleep(100);
	if (isValid()) {
	  ret = (lives_dialog_response_t)atoi(private_response);
	  lives_free(private_response);
	}
      }
      return ret;
    }
    if (idle_show_info(text.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(),blocking,0))
      return LIVES_DIALOG_RESPONSE_NONE;
    return ret;
  }


  LiVESString livesApp::chooseFileWithPreview(LiVESString dirname, lives_filechooser_t preview_type, LiVESString title) {
    LiVESString emptystr;
    if (!isValid()) return emptystr;
    if (preview_type != LIVES_FILE_CHOOSER_VIDEO_AUDIO && preview_type != LIVES_FILE_CHOOSER_AUDIO_ONLY) return emptystr;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    char *ret = NULL;
    if (!idle_choose_file_with_preview(dirname.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str(),
				       title.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(),
				       preview_type,msg_id)) {
      spinning = false;
      removeCallback(cbid);
    }
    else {
      while (spinning) usleep(100);
      if (isValid()) {
	// last 2 chars are " " and %d (deinterlace choice)
	LiVESString str(private_response, strlen(private_response) - 2, LIVES_CHAR_ENCODING_FILESYSTEM);
	m_deinterlace = (bool)atoi(private_response + strlen(private_response) - 2);
	lives_free(private_response);
	return str;
      }
    }
    return emptystr;
  }


  LiVESString livesApp::chooseSet() {
    LiVESString emptystr;
    if (!isValid()) return emptystr;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    char *ret = NULL;
    if (!idle_choose_set(msg_id)) {
      spinning = false;
      removeCallback(cbid);
    }
    else {
      while (spinning) usleep(100);
      if (isValid()) {
	LiVESString str(private_response, LIVES_CHAR_ENCODING_FILESYSTEM);
	lives_free(private_response);
	return str;
      }
    }
    return emptystr;
  }


  clip livesApp::openFile(LiVESString fname, bool with_audio, double stime, int frames, bool deinterlace) {
    if (!isValid()) return clip(0);
    if (fname.empty()) return clip(0);
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    ulong cid = 0l;
    
    if (!idle_open_file(fname.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str(), stime, frames, msg_id)) {
      spinning = false;
      removeCallback(cbid);
    }
    else {
      while (spinning) usleep(100);
      if (isValid()) {
	cid = strtoul(private_response, NULL, 10);
	lives_free(private_response);
      }
    }
    return clip(cid, this);
  }


  bool livesApp::reloadSet(LiVESString setname) {
    if (!isValid()) return false;
    if (setname.empty()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    if (!idle_reload_set(setname.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str(),msg_id)) {
      spinning = false;
      removeCallback(cbid);
      return false;
    }
    while (spinning) usleep(100);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  bool livesApp::deinterlaceOption() {
    return m_deinterlace;
  }

  lives_interface_mode_t livesApp::mode() {
    if (!isValid()) return LIVES_INTERFACE_MODE_INVALID;
    if (mainw->multitrack != NULL) return LIVES_INTERFACE_MODE_MULTITRACK;
    return LIVES_INTERFACE_MODE_CLIPEDIT;
  }

  lives_status_t livesApp::status() {
    if (!isValid()) return LIVES_STATUS_INVALID;
    if (mainw->go_away) return LIVES_STATUS_NOTREADY;
    if (mainw->playing_file > -1) return LIVES_STATUS_PLAYING;
    if (mainw->is_processing) return LIVES_STATUS_PROCESSING;
    if (mainw->preview) return LIVES_STATUS_PREVIEW;
    return LIVES_STATUS_READY;
  }

  closureList& livesApp::closures() {
    return m_closures;
  }

  void livesApp::invalidate() {
    m_id = 0l;
  }

  bool livesApp::interactive() {
    return mainw->interactive;
  }


  bool livesApp::setInteractive(bool setting) {
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    if (!idle_set_interactive(setting, msg_id)) {
      spinning = false;
      removeCallback(cbid);
    }
    while (spinning) usleep(100);
    if (isValid()) {
      lives_free(private_response);
    }
    return setting;
  }



  const effectKeyMap& livesApp::currentEffectKeyMap() {
    return *m_effectKeyMap;
  }


#ifndef DOXYGEN_SKIP
  bool livesApp::setPref(int prefidx, bool val) {
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    if (!idle_set_pref_bool(prefidx, val, msg_id)) {
      spinning = false;
      removeCallback(cbid);
      return false;
    }
    while (spinning) usleep(100);
    if (isValid()) {
      lives_free(private_response);
    }
    return true;
  }

  bool livesApp::setPref(int prefidx, int val) {
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    if (!idle_set_pref_int(prefidx, val, msg_id)) {
      spinning = false;
      removeCallback(cbid);
      return false;
    }
    while (spinning) usleep(100);
    if (isValid()) {
      lives_free(private_response);
    }
    return true;
  }
#endif

  //////////////// player ////////////////////

  player::player(livesApp *lives) {
    // make shared ptr
    m_lives = lives;
  }


  bool player::isValid() {
    return m_lives->isValid();
  }


  bool player::play() {
    if (!isValid()) return false;
    return play_thread();
  }

  bool player::stop() {
    if (!isValid()) return FALSE;
    // return false if we are not playing
    return lives_osc_cb_stop(NULL, 0, NULL, OSCTT_CurrentTime(), NULL);
  }




  //////////////// set ////////////////////


  set::set(livesApp *lives) {
    // make shared ptr
    m_lives = lives;
  }


  bool set::isValid() {
    return m_lives->isValid();
  }


  LiVESString set::name() {
    if (!isValid()) return LiVESString("");
    return LiVESString(get_set_name(), LIVES_CHAR_ENCODING_UTF8);
  }


  unsigned int set::numClips() {
    if (!isValid()) return 0;
    update_clip_list();
    return m_clips.size();
  }


  clip set::nthClip(unsigned int n) {
    if (!isValid()) return clip(0l);
    update_clip_list();
    if (n >= m_clips.size()) return clip(0l);
    return clip(m_clips[n], m_lives);
  }


  int set::indexOf(clip c) {
    if (!isValid()) return -1;
    if (!c.isValid()) return -1;
    update_clip_list();
    int i;
    for (i = 0; i < m_clips.size(); i++) {
      if (m_clips[i] == c.m_uid) return i;
    }
    return -1;
  }



  bool set::save(LiVESString name, bool force_append) {
    if (!isValid()) return FALSE;
    int arglen = 3;
    char **vargs=(char **)lives_malloc(sizeof(char *));
    const char *cname = name.c_str();
    *vargs = strdup(",si");
    arglen = padup(vargs, arglen);
    arglen = add_string_arg(vargs, arglen, cname);
    arglen = add_int_arg(vargs, arglen, force_append);

    spinning = true;
    msg_id = lives_random();

    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 

    bool ret = false;

    if (!idle_save_set(cname,arglen,(const void *)(*vargs),msg_id)) {
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) usleep(100);
      if (isValid()) {
	ret = (bool)(atoi(private_response));
	lives_free(private_response);
      }
    }
    lives_free(*vargs);
    return ret;
  }


  void set::update_clip_list() {
    // clear old cliplist
    clipListIterator it = m_clips.begin();
    while (it != m_clips.end()) {
      it = m_clips.erase(it);
    }
    if (isValid()) {
      ulong *ids = get_unique_ids();

      for (int i=0; ids[i] != 0l; i++) {
	m_clips.push_back(ids[i]);
      }
      lives_free(ids);
    }
  }


  /////////////// clip ////////////////


  clip::clip() : m_uid(0l) {};

  clip::clip(ulong uid, livesApp *lives) {
    m_uid = uid;
    m_lives = lives;
  }

  bool clip::isValid() {
    return (m_lives->isValid() && cnum_for_uid(m_uid) != -1);
  }

  int clip::frames() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->frames;
    }
    return -1;
  }

  int clip::width() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->hsize;
    }
    return -1;
  }

  int clip::height() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->vsize;
    }
    return -1;
  }

  double clip::fps() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->fps;
    }
    return -1.;
  }

  int clip::audioRate() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->arate;
    }
    return -1;
  }

  int clip::audioChannels() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->achans;
    }
    return -1;
  }

  int clip::audioSampleSize() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->asampsize;
    }
    return -1;
  }

  bool clip::audioSigned() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return !(mainw->files[cnum]->signed_endian & AFORM_UNSIGNED);
    }
    return true;
  }

  lives_endian_t clip::audioEndian() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) {
	if (mainw->files[cnum]->signed_endian & AFORM_BIG_ENDIAN) return LIVES_BIGENDIAN;
      }
    }
    return LIVES_LITTLEENDIAN;
  }

  LiVESString clip::name() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return LiVESString(mainw->files[cnum]->name, LIVES_CHAR_ENCODING_UTF8);
    }
    LiVESString emptystr;
    return emptystr;
  }

  int clip::selectionStart() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->start;
    }
    return -1;
  }

  int clip::selectionEnd() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->end;
    }
    return -1;
  }

  
  bool clip::switchTo() {
    if (!isValid()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    int cnum = cnum_for_uid(m_uid);
    if (!idle_switch_clip(1,cnum, msg_id)) {
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) usleep(100);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  bool clip::setAsBackground() {
    if (!isValid()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    int cnum = cnum_for_uid(m_uid);
    if (!idle_switch_clip(2,cnum, msg_id)) {
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) usleep(100);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }

  //////////////////////////////////////////////

  //// effectKeyMap
  effectKeyMap::effectKeyMap(livesApp *lives) {
    m_lives = lives;
  }


  bool effectKeyMap::isValid() {
    return m_lives->isValid();
  }


  effectKey effectKeyMap::at(int i) {
    return (*this)[i];
  }

  size_t effectKeyMap::size() {
    if (!isValid()) return 0;
    return (size_t) prefs::rteKeysVirtual(*m_lives);
  }

  bool effectKeyMap::clear() {
    if (!isValid()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    if (!idle_unmap_effects(msg_id)) {
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) usleep(100);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  /////////////////////////////////////////////////

  /// effectKey
  effectKey::effectKey(livesApp *lives, int key) {
    m_lives = lives;
    m_key = key;
  }

  bool effectKey::isValid() {
    return m_lives->isValid() && m_key >= 1 && m_key <= prefs::rteKeysVirtual(*(m_lives));
  }
  

  int effectKey::numModes() {
    if (!isValid()) return 0;
    return ::prefs->max_modes_per_key;
  }

  int effectKey::numMappedModes() {
    if (!isValid()) return 0;
    return get_num_mapped_modes_for_key(m_key);
  }


  int effectKey::mode() {
    if (!isValid()) return -1;
    return get_current_mode_for_key(m_key);
  }

  bool effectKey::enabled() {
    if (!isValid()) return false;
    return get_rte_key_is_enabled(m_key);
  }


  int effectKey::setMode(int new_mode) {
    if (!isValid()) return -1;
    if (new_mode < 0 || new_mode >= numMappedModes()) return mode();

    if (new_mode == mode()) return new_mode;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    if (!idle_fx_setmode(m_key, new_mode, msg_id)) {
      spinning = false;
      m_lives->removeCallback(cbid);
      return mode();
    }
    while (spinning) usleep(100);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
    }
    return mode();
  }



  bool effectKey::setEnabled(bool setting) {
    if (!isValid()) return false;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    if (!idle_fx_enable(m_key, setting,  msg_id)) {
      spinning = false;
      m_lives->removeCallback(cbid);
      return enabled();
    }
    while (spinning) usleep(100);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
    }
    return enabled();
  }



  int effectKey::appendMapping(effect fx) {
    if (!isValid()) return -1;
    if (!fx.isValid()) return -1;

    if (fx.m_lives != m_lives) return -1;

    int mode = numMappedModes();
    if (mode == numModes()) return -1;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    if (!idle_map_fx(m_key, mode, fx.m_idx, msg_id)) {
      spinning = false;
      m_lives->removeCallback(cbid);
      return -1;
    }
    while (spinning) usleep(100);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      if (ret) return mode;
    }
    return -1;
  }

  ////////////////////////////////////////////////////////

  effect::effect(livesApp& lives, LiVESString hashname) {
    // TODO
    m_lives = &lives;
  }

  effect::effect(livesApp& lives, const char *package, const char *fxname, const char *author, int version) {
    m_idx = get_first_fx_matched(package, fxname, author, version);
    m_lives = &lives;
  }

  bool effect::isValid() {
    return (m_idx != -1 && m_lives->isValid());
  }


  ////// prefs
  

  namespace prefs {
    LiVESString currentVideoLoadDir(livesApp &lives) {
      LiVESString str(mainw->vid_load_dir, LIVES_CHAR_ENCODING_UTF8);
      return str;
    }

    LiVESString currentAudioDir(livesApp &lives) {
      LiVESString str(mainw->audio_dir, LIVES_CHAR_ENCODING_UTF8);
      return str;
    }

    LiVESString tmpDir(livesApp &lives) {
      LiVESString str(::prefs->tmpdir, LIVES_CHAR_ENCODING_FILESYSTEM);
      return str;
    }

    lives_audio_source_t audioSource(livesApp &lives) {
      if (::prefs->audio_src == AUDIO_SRC_EXT) return LIVES_AUDIO_SOURCE_EXTERNAL;
      return LIVES_AUDIO_SOURCE_INTERNAL;
    }

    bool setAudioSource(livesApp &lives, lives_audio_source_t asrc) {
      return lives.setPref(PREF_REC_EXT_AUDIO, (bool)(asrc==LIVES_AUDIO_SOURCE_EXTERNAL));
    }

    lives_audio_player_t audioPlayer(livesApp &lives) {
      if (::prefs->audio_player == AUD_PLAYER_SOX) return LIVES_AUDIO_PLAYER_SOX;
      if (::prefs->audio_player == AUD_PLAYER_JACK) return LIVES_AUDIO_PLAYER_JACK;
      if (::prefs->audio_player == AUD_PLAYER_PULSE) return LIVES_AUDIO_PLAYER_PULSE;
      if (::prefs->audio_player == AUD_PLAYER_MPLAYER) return LIVES_AUDIO_PLAYER_MPLAYER;
      if (::prefs->audio_player == AUD_PLAYER_MPLAYER2) return LIVES_AUDIO_PLAYER_MPLAYER2;
    }

    int rteKeysVirtual(livesApp &lives) {
      return ::prefs->rte_keys_virtual;
    }

  }


}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef DOXYGEN_SKIP

void binding_cb (lives_callback_t cb_type, const char *msgstring, ulong id) {
  bool ret;
  lives::livesApp *lapp;

  if (cb_type == LIVES_CALLBACK_OBJECT_DESTROYED) lapp = (lives::livesApp *)id;
  else lapp = lives::find_instance_for_id(id);

  if (lapp == NULL) return;

  lives::closureList cl = lapp->closures();

  lives::closureListIterator it = cl.begin();
  while (it != cl.end()) {

    if ((*it)->cb_type == cb_type) {
      switch (cb_type) {
      case LIVES_CALLBACK_MODE_CHANGED:
	{
	  lives::modeChangedInfo info;
	  info.mode = (lives_interface_mode_t)atoi(msgstring);
	  lives::modeChanged_callback_f fn = (lives::modeChanged_callback_f)((*it)->func);
	  ret = (fn)((*it)->object, &info, (*it)->data);
	}
	break;
      case LIVES_CALLBACK_APP_QUIT:
	{
	  // TODO !! test
	  lives::appQuitInfo info;
	  info.signum = atoi(msgstring);
	  lives::appQuit_callback_f fn = (lives::appQuit_callback_f)((*it)->func);
	  lapp->invalidate();
	  ret = (fn)((*it)->object, &info, (*it)->data);
	  spinning = false;
	}
	break;
      case LIVES_CALLBACK_OBJECT_DESTROYED:
	{
	  lives::objectDestroyed_callback_f fn = (lives::objectDestroyed_callback_f)((*it)->func);
	  ret = (fn)((*it)->object, (*it)->data);
	}
	break;
      case LIVES_CALLBACK_PRIVATE:
	{
	  // private event type
	  lives::_privateInfo info;
	  char *endptr;
	  info.id = strtoul(msgstring,&endptr,10);
	  info.response = endptr+1;
	  lives::private_callback_f fn = (lives::private_callback_f)((*it)->func);
	  ret = (fn)(&info, (*it)->data);
	}
	break;
      default:
	continue;
      }
      if (!ret) {
	delete *it;
	it = cl.erase(it);

	// for some really bizarre reason cl is only a reference to m_closures and we have to write it back
	// even though closures() is supposed to be "pass-by-value"...
	lapp->setClosures(cl);
	continue;
      }
    }
    ++it;
  }
}

#endif // doxygen_skip
