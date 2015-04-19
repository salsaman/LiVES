// liblives.cpp
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

/** \file liblives.cpp
    liblives interface
 */

#ifndef DOXYGEN_SKIP

#include "liblives.hpp"

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <iostream>

extern "C" {
  typedef int Boolean;
#include <libOSC/libosc.h>
#include <libOSC/OSC-client.h>
#include "main.h"
#include "lbindings.h"
#include "effects-weed.h"

  int real_main(int argc, char *argv[], ulong id);

  bool is_big_endian(void);

  bool lives_osc_cb_quit(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);
  bool lives_osc_cb_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, void * ra);

  track_rect *find_block_by_uid(lives_mt *mt, ulong uid);

}


static volatile bool spinning;
static ulong msg_id;
static char *private_response;
static pthread_mutex_t spin_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_done = PTHREAD_COND_INITIALIZER;

static bool private_cb(lives::_privateInfo *info, void *data) {
  if (info->id == msg_id) {
    private_response = strdup(info->response);
    spinning = false;
    pthread_cond_signal(&cond_done);
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

  void livesString::setEncoding(lives_char_encoding_t enc) {
    m_encoding = enc;
  }

  lives_char_encoding_t livesString::encoding() {
    return m_encoding;
  }

  livesString livesString::toEncoding(lives_char_encoding_t enc) {
    if (enc == LIVES_CHAR_ENCODING_UTF8) {
      if (m_encoding == LIVES_CHAR_ENCODING_LOCAL8BIT) {
	livesString str(L2U8(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_UTF8);
	return str;
      }
#ifndef IS_MINGW
      else if (m_encoding == LIVES_CHAR_ENCODING_FILESYSTEM) {
	livesString str(F2U8(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_UTF8);
	return str;
      }
#endif
    }
    else if (enc == LIVES_CHAR_ENCODING_FILESYSTEM) {
#ifndef IS_MINGW
      if (m_encoding == LIVES_CHAR_ENCODING_UTF8) {
	livesString str(U82F(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_FILESYSTEM);
	return str;
      }
#else
      if (m_encoding == LIVES_CHAR_ENCODING_LOCAL8BIT) {
	livesString str(U82L(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_FILESYSTEM);
	return str;
      }
#endif
    }
    else if (enc == LIVES_CHAR_ENCODING_LOCAL8BIT) {
      if (m_encoding == LIVES_CHAR_ENCODING_UTF8) {
	livesString str(U82L(this->c_str()));
	str.setEncoding(LIVES_CHAR_ENCODING_LOCAL8BIT);
	return str;
      }
#ifndef IS_MINGW
      if (m_encoding == LIVES_CHAR_ENCODING_FILESYSTEM) {
	livesString str(F2U8(this->c_str()));
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
    m_multitrack = new multitrack(this);

    m_deinterlace = false;

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
    return this != NULL && m_id != 0l;
  }


  bool livesApp::isPlaying() {
    return status() == LIVES_STATUS_PLAYING;
  }


  bool livesApp::isReady() {
    return status() == LIVES_STATUS_READY;
  }


  const set& livesApp::getSet() {
    return *m_set;
  }


  const player& livesApp::getPlayer() {
    return *m_player;
  }


  const multitrack& livesApp::getMultitrack() {
    return *m_multitrack;
  }


  ulong livesApp::appendClosure(lives_callback_t cb_type, callback_f func, void *data) {
    closure *cl = new closure;
    cl->id = lives_random();
    cl->object = this;
    cl->cb_type = cb_type;
    cl->func = (callback_f)func;
    cl->data = data;
    pthread_mutex_lock(&spin_mutex); // lock mutex so that new callbacks cannot be added yet
    m_closures.push_back(cl);
    pthread_mutex_unlock(&spin_mutex);
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
    pthread_mutex_lock(&spin_mutex); // lock mutex so that new callbacks cannot be added yet
    closureListIterator it = m_closures.begin();
    while (it != m_closures.end()) {
      if ((*it)->id == id) {
	delete *it;
	m_closures.erase(it);
	pthread_mutex_unlock(&spin_mutex);
	return true;
      }
      ++it;
    }
    pthread_mutex_unlock(&spin_mutex);
    return false;
  }


  lives_dialog_response_t livesApp::showInfo(livesString text, bool blocking) {
    lives_dialog_response_t ret=LIVES_DIALOG_RESPONSE_INVALID;
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return ret;
    // if blocking wait for response
    if (blocking) {
      spinning = true;
      msg_id = lives_random();
      ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
      pthread_mutex_lock(&spin_mutex);
      if (!idle_show_info(text.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(),blocking, msg_id)) {
	pthread_mutex_unlock(&spin_mutex);
	spinning = false;
	removeCallback(cbid);
      }
      else {
	while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
	pthread_mutex_unlock(&spin_mutex);
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


  livesString livesApp::chooseFileWithPreview(livesString dirname, lives_filechooser_t preview_type, livesString title) {
    livesString emptystr;
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return emptystr;
    if (preview_type != LIVES_FILE_CHOOSER_VIDEO_AUDIO && preview_type != LIVES_FILE_CHOOSER_AUDIO_ONLY) return emptystr;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_choose_file_with_preview(dirname.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str(),
				       title.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(),
				       preview_type, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	// last 2 chars are " " and %d (deinterlace choice)
	livesString str(private_response, strlen(private_response) - 2, LIVES_CHAR_ENCODING_FILESYSTEM);
	m_deinterlace = (bool)atoi(private_response + strlen(private_response) - 2);
	lives_free(private_response);
	return str;
      }
    }
    return emptystr;
  }


  livesString livesApp::chooseSet() {
    livesString emptystr;
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return emptystr;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_choose_set(msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	livesString str(private_response, LIVES_CHAR_ENCODING_FILESYSTEM);
	lives_free(private_response);
	return str;
      }
    }
    return emptystr;
  }


  livesStringList livesApp::availableSets() {
    livesStringList list;
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return list;
    LiVESList *setlist=get_set_list(::prefs->tmpdir, true), *slist=setlist;
    while (slist != NULL) {
      list.push_back(livesString((const char *)slist->data, LIVES_CHAR_ENCODING_UTF8));
      lives_free(slist->data);
      slist = slist->next;
    }
    lives_list_free(setlist);
    return list;
  }


  clip livesApp::openFile(livesString fname, bool with_audio, double stime, int frames, bool deinterlace) {
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return clip();
    if (fname.empty()) return clip();
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    ulong cid = 0l;
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_open_file(fname.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str(), stime, frames, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	cid = strtoul(private_response, NULL, 10);
	lives_free(private_response);
      }
    }
    return clip(cid, this);
  }


  bool livesApp::reloadSet(livesString setname) {
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_reload_set(setname.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str(), msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
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
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return LIVES_INTERFACE_MODE_INVALID;
    if (m_multitrack->isActive()) return LIVES_INTERFACE_MODE_MULTITRACK;
    return LIVES_INTERFACE_MODE_CLIPEDIT;
  }


  lives_interface_mode_t livesApp::setMode(lives_interface_mode_t newmode) {
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return LIVES_INTERFACE_MODE_INVALID;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_if_mode(newmode, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
      return mode();
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
    }
    return mode();
  }


  lives_status_t livesApp::status() {
    if (!isValid()) return LIVES_STATUS_INVALID;
    if (mainw->go_away) return LIVES_STATUS_NOTREADY;
    if (mainw->is_processing) return LIVES_STATUS_PROCESSING;
    if ((mainw->preview || mainw->event_list != NULL) && mainw->multitrack==NULL) return LIVES_STATUS_PREVIEW;
    if (mainw->playing_file > -1 || mainw->preview) return LIVES_STATUS_PLAYING;
    return LIVES_STATUS_READY;
  }



  bool livesApp::cancel() {
    if (!isValid()) return false;
    if (status() != LIVES_STATUS_PROCESSING) return false;
    bool ret = false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_cancel_proc(msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      ret = (bool)atoi(private_response);
      lives_free(private_response);
    }
    return ret;
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
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return mainw->interactive;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_interactive(setting, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      lives_free(private_response);
    }
    return setting;
  }



  const effectKeyMap& livesApp::getEffectKeyMap() {
    return *m_effectKeyMap;
  }


#ifndef DOXYGEN_SKIP
  bool livesApp::setPref(int prefidx, bool val) {
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_pref_bool(prefidx, val, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      lives_free(private_response);
    }
    return true;
  }

  bool livesApp::setPref(int prefidx, int val) {
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_pref_int(prefidx, val, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      lives_free(private_response);
    }
    return true;
  }

  bool livesApp::setPref(int prefidx, int bitfield, bool val) {
    if (!isValid() || status() == LIVES_STATUS_NOTREADY) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_pref_bitmapped(prefidx, bitfield, val, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
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


  bool player::isValid() const {
    return m_lives != NULL && m_lives->isValid() && m_lives->status() != LIVES_STATUS_NOTREADY;
  }


  bool player::isPlaying() const {
    return isValid() && m_lives->isPlaying();
  }


  bool player::isRecording() const {
    return isValid() && mainw->record;
  }


  bool player::play() const {
    if (!isValid() || !m_lives->isReady()) return false;
    return start_player();
  }

  bool player::stop() const {
    if (!isPlaying()) return false;
    // return false if we are not playing
    return lives_osc_cb_stop(NULL, 0, NULL, OSCTT_CurrentTime(), NULL);
  }


  void player::setSepWin(bool setting) const {
    if (!isValid()) return;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_sepwin(setting, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      lives_free(private_response);
    }
    return;
  }


  void player::setFullScreen(bool setting) const {
    if (!isValid()) return;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_fullscreen(setting, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      lives_free(private_response);
    }
    return;
  }


  bool player::sepWin() const {
    if (!isValid()) return false;
    return mainw->sep_win;
  }


  bool player::fullScreen() const {
    if (!isValid()) return false;
    return mainw->fs;
  }


  bool player::setForegroundClip(clip c) const {
    if (!isPlaying()) return false;
    return c.switchTo();
  }


  bool player::setBackgroundClip(clip c) const {
    if (!isPlaying()) return false;
    return c.setIsBackground();
  }

  clip player::foregroundClip() const {
    if (!isPlaying()) return clip();
    if (m_lives->m_multitrack->isActive()) return clip();
    if (mainw->files[mainw->playing_file] != NULL) return clip(mainw->files[mainw->playing_file]->unique_id, m_lives);
    return clip();
  }

  clip player::backgroundClip() const {
    if (!isPlaying()) return clip();
    if (m_lives->m_multitrack->isActive()) return clip();
    if (mainw->files[mainw->blend_file] != NULL) return clip(mainw->files[mainw->blend_file]->unique_id, m_lives);
    return clip();
  }

  void player::setFS(bool setting) const {
    if (!isValid()) return;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_fullscreen_sepwin(setting, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      lives_free(private_response);
    }
    return;
  }


  double player::videoPlaybackTime(bool background) const {
    if (!isValid()) return 0.;

    if (m_lives->status() == LIVES_STATUS_NOTREADY || m_lives->status() == LIVES_STATUS_PROCESSING) return 0.;

    if (!m_lives->m_multitrack->isActive()) {
      if (mainw->current_file == -1) return 0.;
      if (mainw->playing_file > -1) {
	if (!background) return (cfile->frameno - 1.)/cfile->fps;
	else if (mainw->blend_file != -1 && mainw->blend_file != mainw->current_file && mainw->files[mainw->blend_file] != NULL) {
	  return mainw->files[mainw->blend_file]->frameno;
	}
	else return 0.;
      }
      else return cfile->pointer_time;
    }
    else {
      return lives_ruler_get_value(LIVES_RULER(mainw->multitrack->timeline));
    }
  }


  double player::audioPlaybackTime() const {
    if (!isValid()) return 0.;

    if (m_lives->status() != LIVES_STATUS_NOTREADY || m_lives->status() == LIVES_STATUS_PROCESSING) return 0.;

    if (!m_lives->m_multitrack->isActive()) {
      if (mainw->current_file == -1) return 0.;
      if (mainw->playing_file > -1) return (mainw->aframeno - 1.)/cfile->fps;
      else return cfile->pointer_time;
    }
    else {
      return lives_ruler_get_value(LIVES_RULER(mainw->multitrack->timeline));
    }
  }


  double player::setAudioPlaybackTime(double time) const {
    if (!isValid()) return 0.;
    if (!m_lives->isPlaying()) return 0.;
    if (!is_realtime_aplayer(::prefs->audio_player)) return 0.;
    if (mainw->record && ::prefs->audio_src == AUDIO_SRC_EXT) return 0.;
    if (mainw->multitrack != NULL) return 0.;
    if (time<0. || time > cfile->laudio_time) return 0.;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_current_audio_time(time, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return audioPlaybackTime();
  }


  double player::setPlaybackStartTime(double time) const {
    if (!isValid()) return 0.;
    if (!m_lives->isReady()) return 0.;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_current_time(time, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return videoPlaybackTime();
  }



  int player::setVideoPlaybackFrame(int frame, bool bg) const {
    if (!isValid()) return 0;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_current_frame(frame, bg, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return videoPlaybackTime();
  }


  double player::elapsedTime() const {
    if (!isPlaying()) return 0.;
    return mainw->currticks/U_SEC;
  }


  double player::currentFPS() const {
    if (!isValid()) return 0.;
    if (mainw->current_file == -1 || cfile == NULL) return 0.;
    if (m_lives->status() != LIVES_STATUS_PLAYING && m_lives->status() != LIVES_STATUS_READY) return 0.;
    return cfile->pb_fps;
  }


  double player::setCurrentFPS(double fps) const {
    if (!isPlaying()) return 0.;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_current_fps(fps, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return currentFPS();
  }


  int player::currentAudioRate() const {
    if (!isValid()) return 0.;
    if (m_lives->status() != LIVES_STATUS_PLAYING && m_lives->status() != LIVES_STATUS_READY) return 0.;
    if (mainw->current_file == -1 || cfile == NULL) return 0.;
    return cfile->arps;
  }


  lives_loop_mode_t player::setLoopMode(lives_loop_mode_t mode) const {
    if (!isValid()) return loopMode();
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_loop_mode(mode, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return loopMode();
  }


  lives_loop_mode_t player::loopMode() const {
    unsigned int lmode = LIVES_LOOP_MODE_NONE;
    if (!isValid()) return (lives_loop_mode_t)lmode;

    if (mainw->loop) lmode |= LIVES_LOOP_MODE_FIT_AUDIO;
    if (mainw->loop_cont) lmode |= LIVES_LOOP_MODE_CONTINUOUS;

    return (lives_loop_mode_t)lmode;
  }



  bool player::setPingPong(bool setting) const {
    if (!isValid()) return pingPong();
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_ping_pong(setting, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return pingPong();
  }


  bool player::pingPong() const {
    if (!isValid()) return false;
    return mainw->ping_pong;
  }


  bool player::resyncFPS() const {
    if (!isPlaying()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_resync_fps(msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return true;
  }



  //////////////// set ////////////////////


  set::set(livesApp *lives) {
    m_lives = lives;
  }


  bool set::isValid() const {
    return m_lives != NULL && m_lives->isValid() && m_lives->status() != LIVES_STATUS_NOTREADY;
  }


  livesString set::name() const {
    if (!isValid()) return livesString();
    return livesString(get_set_name(), LIVES_CHAR_ENCODING_UTF8);
  }


  bool set::setName(livesString name) const {
    if (!isValid()) return false;
    if (strlen(mainw->set_name) > 0) return false;
    if (numClips() == 0) return false;
    
    if (!name.empty()) {
      const char *new_set_name = name.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str();
      if (is_legal_set_name(new_set_name, TRUE)) {
	lives_snprintf(mainw->set_name,128,"%s",new_set_name);
	return true;
      }
      return false;
    }

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_set_name(msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  unsigned int set::numClips() const {
    if (!isValid()) return 0;
    (const_cast<set *>(this))->update_clip_list();
    return m_clips.size();
  }


  clip set::nthClip(unsigned int n) const {
    if (!isValid()) return clip();
    (const_cast<set *>(this))->update_clip_list();
    if (n >= m_clips.size()) return clip();
    return clip(m_clips[n], m_lives);
  }


  int set::indexOf(clip c) const {
    if (!isValid()) return -1;
    if (!c.isValid()) return -1;
    (const_cast<set *>(this))->update_clip_list();
    int i;
    for (i = 0; i < m_clips.size(); i++) {
      if (m_clips[i] == c.m_uid) return i;
    }
    return -1;
  }


  bool set::save(livesString name, bool force_append) const {
    if (!isValid()) return FALSE;
    const char *cname = name.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str();

    spinning = true;
    msg_id = lives_random();

    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 

    bool ret = false;

    pthread_mutex_lock(&spin_mutex);
    if (!idle_save_set(cname,force_append, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	ret = (bool)(atoi(private_response));
	lives_free(private_response);
      }
    }
    return ret;
  }


  bool set::save() const {
    return save(name());
  }



  void set::update_clip_list() {
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


  clip::clip() : m_uid(0l), m_lives(NULL) {};

  clip::clip(ulong uid, livesApp *lives) {
    m_uid = uid;
    m_lives = lives;
  }

  bool clip::isValid() {
    return (m_lives != NULL && m_lives->isValid() && m_lives->status() != LIVES_STATUS_NOTREADY && cnum_for_uid(m_uid) != -1);
  }

  int clip::frames() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->frames;
    }
    return 0;
  }

  int clip::width() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->hsize;
    }
    return 0;
  }

  int clip::height() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->vsize;
    }
    return 0;
  }

  double clip::FPS() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->fps;
    }
    return 0.;
  }


  double clip::playbackFPS() {
    if (isValid()) {
      if (!m_lives->m_multitrack->isActive()) {
	int cnum = cnum_for_uid(m_uid);
	if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->pb_fps;
      }
      else return m_lives->m_multitrack->FPS();
    }
    return 0.;
  }


  int clip::audioRate() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->arate;
    }
    return 0;
  }


  int clip::playbackAudioRate() {
    int arps = -1;
    if (isValid()) {
      if (!m_lives->m_multitrack->isActive()) {
	int cnum = cnum_for_uid(m_uid);
	if (cnum > -1 && mainw->files[cnum] != NULL) {
	  arps = mainw->files[cnum]->arps;
	  arps *= mainw->files[cnum]->pb_fps / mainw->files[cnum]->fps;
	}
      }
      else arps = mainw->files[mainw->multitrack->render_file]->arate;
    }
    return arps;
  }


  double clip::audioLength() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->laudio_time;
    }
    return 0.;
  }


  int clip::audioChannels() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->achans;
    }
    return 0;
  }

  int clip::audioSampleSize() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->asampsize;
    }
    return 0;
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

  livesString clip::name() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return livesString(get_menu_name(mainw->files[cnum]), LIVES_CHAR_ENCODING_UTF8);
    }
    return livesString();
  }

  int clip::selectionStart() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->start;
    }
    return 0;
  }

  int clip::selectionEnd() {
    if (isValid()) {
      int cnum = cnum_for_uid(m_uid);
      if (cnum > -1 && mainw->files[cnum] != NULL) return mainw->files[cnum]->end;
    }
    return 0;
  }


  bool clip::selectAll() {
    if (!isValid() || m_lives->status() == LIVES_STATUS_NOTREADY) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    int cnum = cnum_for_uid(m_uid);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_select_all(cnum, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }



  bool clip::setSelectionStart(unsigned int frame) {
    if (!isValid()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    int cnum = cnum_for_uid(m_uid);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_select_start(cnum, frame, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  bool clip::setSelectionEnd(unsigned int frame) {
    if (!isValid()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    int cnum = cnum_for_uid(m_uid);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_select_end(cnum, frame, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }

  
  bool clip::switchTo() {
    if (!isValid()) return false;
    if (m_lives->m_multitrack->isActive()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    int cnum = cnum_for_uid(m_uid);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_switch_clip(1,cnum, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  bool clip::setIsBackground() {
    if (!isValid()) return false;
    if (m_lives->m_multitrack->isActive()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    int cnum = cnum_for_uid(m_uid);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_switch_clip(2,cnum, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
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


  bool effectKeyMap::isValid() const {
    return m_lives != NULL && m_lives->isValid() && m_lives->status() != LIVES_STATUS_NOTREADY;
  }


  effectKey effectKeyMap::at(int i) const {
    return (*this)[i];
  }

  size_t effectKeyMap::size() const {
    if (!isValid()) return 0;
    return (size_t) prefs::rteKeysVirtual(*m_lives);
  }

  bool effectKeyMap::clear() const {
    if (!isValid()) return false;
    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_unmap_effects(msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  /////////////////////////////////////////////////

  /// effectKey
  effectKey::effectKey() {
    m_key = 0;
  }

  effectKey::effectKey(livesApp *lives, int key) {
    m_lives = lives;
    m_key = key;
  }

  bool effectKey::isValid() {
    return m_lives != NULL && m_lives->isValid() && m_lives->status() != LIVES_STATUS_NOTREADY && 
      m_key >= 1 && m_key <= prefs::rteKeysVirtual(*(m_lives));
  }
  
  int effectKey::key() {
    return m_key;
  }


  int effectKey::numModes() {
    if (!isValid()) return 0;
    return ::prefs->max_modes_per_key;
  }

  int effectKey::numMappedModes() {
    if (!isValid()) return 0;
    return get_num_mapped_modes_for_key(m_key);
  }


  int effectKey::currentMode() {
    if (!isValid()) return -1;
    return get_current_mode_for_key(m_key);
  }

  bool effectKey::enabled() {
    if (!isValid()) return false;
    return get_rte_key_is_enabled(m_key);
  }


  int effectKey::setCurrentMode(int new_mode) {
    if (!isValid()) return -1;
    if (new_mode < 0 || new_mode >= numMappedModes()) return currentMode();

    if (new_mode == currentMode()) return currentMode();

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_fx_setmode(m_key, new_mode, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return currentMode();
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
    }
    return currentMode();
  }



  bool effectKey::setEnabled(bool setting) {
    if (!isValid()) return false;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_fx_enable(m_key, setting,  msg_id)) {
      spinning = false;
      m_lives->removeCallback(cbid);
      return enabled();
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);

      // TODO: if it was a generator, wait for playing or error


    }
    return enabled();
  }



  int effectKey::appendMapping(effect fx) {
    if (!isValid()) return -1;
    if (!fx.isValid()) return -1;

    if (fx.m_lives != m_lives) return -1;

    if (!m_lives->isReady() && !m_lives->isPlaying()) return -1;

    int mode = numMappedModes();
    if (mode == numModes()) return -1;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_map_fx(m_key, mode, fx.m_idx, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return -1;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      if (ret) return mode;
    }
    return -1;
  }


  bool effectKey::removeMapping(int mode) {
    if (!isValid()) return false;

    if (!m_lives->isReady() && !m_lives->isPlaying()) return false;

    if (mode >= numMappedModes()) return false;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_unmap_fx(m_key, mode, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  effect effectKey::at(int mode) {
    effect e;
    if (!isValid()) return e;
    int idx = rte_keymode_get_filter_idx(m_key, mode);
    if (idx == -1) return e;
    e = effect(m_lives, idx);
    return e;
  }


  ////////////////////////////////////////////////////////

  effect::effect(livesApp lives, livesString hashname, bool match_full) {
    m_idx = -1;
    m_lives = &lives;
    if (m_lives != NULL && m_lives->isValid() && m_lives->status() != LIVES_STATUS_NOTREADY) {
      m_idx = weed_get_idx_for_hashname(hashname.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(), match_full);
    }
  }

  effect::effect(livesApp lives, livesString package, livesString fxname, livesString author, int version) {
    m_idx = -1;
    m_lives = &lives;
    if (m_lives != NULL && m_lives->isValid() && m_lives->status() != LIVES_STATUS_NOTREADY) {
      m_idx = get_first_fx_matched(package.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(), 
				   fxname.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(), 
				   author.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(), 
				   version);
    }
  }

  bool effect::isValid() {
    return (m_idx != -1 && m_lives != NULL && m_lives->isValid() && m_lives->status() != LIVES_STATUS_NOTREADY);
  }


  effect::effect() : m_lives(NULL), m_idx(-1) {}

  effect::effect(livesApp *lives, int idx) : m_lives(lives), m_idx(idx) {}



  ///////////////////////////////
  //// block

  block::block(multitrack *m, ulong uid) : m_uid(uid) {
    if (m == NULL) m_lives = NULL;
    else m_lives = m->m_lives;
  }


  block::block(multitrack m, int track, double time) {
    m_lives = m.m_lives;
    if (!m.isActive()) m_uid = 0l;
    else {
      track_rect *tr = get_block_from_track_and_time(mainw->multitrack, track, time);
      if (tr == NULL) m_uid = 0l;
      else m_uid = tr->uid;
    }
  }

  bool block::isValid() {
    if (m_lives == NULL || !m_lives->isValid() || !m_lives->m_multitrack->isActive() || m_uid == 0l || 
	find_block_by_uid(mainw->multitrack, m_uid) == NULL) return false;
    return true;
  }

  void block::invalidate() {
    m_uid = 0l;
  }

  double block::startTime() {
    track_rect *tr = find_block_by_uid(mainw->multitrack, m_uid);
    if (tr == NULL) return -1.;
    return (double)get_event_timecode(tr->start_event)/U_SEC;
  }

  double block::length() {
    track_rect *tr = find_block_by_uid(mainw->multitrack, m_uid);
    if (tr == NULL) return -1.;
    return (double)get_event_timecode(tr->end_event)/U_SEC + 1./mainw->multitrack->fps -
      (double)get_event_timecode(tr->start_event)/U_SEC;
  }

  clip block::clipSource() {
    track_rect *tr = find_block_by_uid(mainw->multitrack, m_uid);
    if (tr == NULL) return clip();
    int cnum = get_clip_for_block(tr);
    if (cnum == -1) return clip();
    return clip(mainw->files[cnum]->unique_id, m_lives);
  }

  int block::track() {
    track_rect *tr = find_block_by_uid(mainw->multitrack, m_uid);
    if (tr == NULL) return 0;
    return get_track_for_block(tr);
  }


  bool block::remove() {
    if (!isValid()) return false;
    if (!m_lives->isReady()) return false;

    track_rect *tr = find_block_by_uid(mainw->multitrack, m_uid);
    if (tr == NULL) return false;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_remove_block(m_uid, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret=(bool)atoi(private_response);
      lives_free(private_response);
      if (ret) invalidate();
      return ret;
   }
    return false;
  }


  bool block::moveTo(int track, double time) {
    if (!isValid()) return false;
    if (!m_lives->isReady()) return false;

    track_rect *tr = find_block_by_uid(mainw->multitrack, m_uid);
    if (tr == NULL) return false;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_move_block(m_uid, track, time, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret=(bool)atoi(private_response);
      lives_free(private_response);
      if (ret) invalidate();
      return ret;
   }
    return false;
  }



  ///////////////////////////////////////////////////////////////////
  /// multitrack

  multitrack::multitrack(livesApp *lives) {
    m_lives = lives;
  }

  bool multitrack::isValid() const {
    return m_lives != NULL && m_lives->m_id != 0l && m_lives->status() != LIVES_STATUS_NOTREADY;
  }


  bool multitrack::isActive() const {
    return (isValid() && mainw->multitrack != NULL);
  }


  double multitrack::currentTime() const {
    if (!isActive()) return 0.;
    return m_lives->m_player->videoPlaybackTime();
  }


  double multitrack::setCurrentTime(double time) const {
    if (!isActive() || !m_lives->isReady()) return currentTime();
    return m_lives->m_player->setPlaybackStartTime(time);
  }


  block multitrack::insertBlock(clip c, bool ign_sel, bool with_audio) const {
    if (!isActive()) return block();
    if (!c.isValid()) return block();
    if (!m_lives->isReady()) return block();

    int clipno = cnum_for_uid(c.m_uid);

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL); 
    pthread_mutex_lock(&spin_mutex);
    if (!idle_insert_block(clipno, ign_sel, with_audio, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return block();
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      ulong uid = strtoul(private_response, NULL, 10);
      lives_free(private_response);
      return block(const_cast<multitrack *>(this), uid);
   }
    return block();
  }



  livesString multitrack::wipeLayout(bool force) const {
    livesString emptystr;
    if (!isActive()) return emptystr;
    if (!m_lives->isReady()) return emptystr;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_wipe_layout(force,  msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return emptystr;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      livesString str(private_response, LIVES_CHAR_ENCODING_UTF8);
      lives_free(private_response);
      return str;
    }
    return emptystr;
  }



  livesString multitrack::chooseLayout() const {
    livesString emptystr;
    if (!isActive()) return emptystr;
    if (!m_lives->isReady()) return emptystr;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_choose_layout(msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return emptystr;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      livesString str(private_response, LIVES_CHAR_ENCODING_UTF8);
      lives_free(private_response);
      return str;
    }
    return emptystr;
  }


  livesStringList multitrack::availableLayouts() const {
    livesStringList list;
    if (!isValid()) return list;
    LiVESList *layoutlist=mainw->current_layouts_map;
    while (layoutlist != NULL) {
      char *data=repl_tmpdir((const char *)layoutlist->data, FALSE);
      list.push_back(livesString(data, LIVES_CHAR_ENCODING_FILESYSTEM).toEncoding(LIVES_CHAR_ENCODING_UTF8));
      lives_free(data);
      layoutlist = layoutlist->next;
    }
    return list;
  }


  bool multitrack::reloadLayout(livesString layoutname) const {
    if (!isActive()) return false;
    if (!m_lives->isReady()) return false;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_reload_layout(layoutname.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str(), msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return false;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      bool ret = (bool)atoi(private_response);
      lives_free(private_response);
      return ret;
    }
    return false;
  }


  livesString multitrack::saveLayout(livesString name) const {
    livesString emptystr;
    if (!isActive()) return emptystr;
    if (!m_lives->isReady()) return emptystr;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_save_layout(name.toEncoding(LIVES_CHAR_ENCODING_FILESYSTEM).c_str(), msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return emptystr;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      char *lname = strdup(private_response);
      lives_free(private_response);
      return livesString(lname).toEncoding(LIVES_CHAR_ENCODING_UTF8);
    }
    return emptystr;
  }


  livesString multitrack::saveLayout() const {
    livesString emptystr;
    if (!isActive()) return emptystr;
    if (!m_lives->isReady()) return emptystr;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_save_layout(NULL, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return emptystr;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      char *lname = strdup(private_response);
      lives_free(private_response);
      return livesString(lname).toEncoding(LIVES_CHAR_ENCODING_UTF8);
    }
    return emptystr;
  }


  clip multitrack::render(bool with_audio, bool normalise_audio) const {
    clip c;
    if (!isActive()) return c;
    if (!m_lives->isReady()) return c;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    
    pthread_mutex_lock(&spin_mutex);
    if (!idle_render_layout(with_audio, normalise_audio, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
      return c;
    }
    while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
    pthread_mutex_unlock(&spin_mutex);
    if (isValid()) {
      ulong uid = strtoul(private_response,NULL,10);
      c = clip(uid, m_lives);
      lives_free(private_response);
    }
    return c;
  }


  effect multitrack::autoTransition() const {
    effect e;
    if (!m_lives->isValid() || m_lives->status() == LIVES_STATUS_NOTREADY) return e;
    if (::prefs->atrans_fx == -1) return e;
    e = effect(m_lives, ::prefs->atrans_fx);
    return e;
  }


  bool multitrack::setAutoTransition(effect autotrans) const {
    if (!m_lives->isValid()) return false;
    if (!autotrans.isValid()) return disableAutoTransition();

    // check if is transition
    if (get_transition_param(get_weed_filter(autotrans.m_idx), FALSE) == -1) return false;

    if (m_lives->status() != LIVES_STATUS_READY && m_lives->status() != LIVES_STATUS_PLAYING) return false;
    mt_set_autotrans(autotrans.m_idx);
    return true;
  }


  bool multitrack::disableAutoTransition() const {
    if (!m_lives->isValid()) return false;
    if (m_lives->status() != LIVES_STATUS_READY && m_lives->status() != LIVES_STATUS_PLAYING) return false;
    mt_set_autotrans(-1);
    return true;
  }


  int multitrack::currentTrack() const {
    if (!isActive()) return 0;
    return mainw->multitrack->current_track;
  }


  bool multitrack::setCurrentTrack(int track) const {
    if (m_lives->status() == LIVES_STATUS_PROCESSING) return false;
    if (!isActive()) return false;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_mt_set_track(track, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	bool ret=(bool)(atoi(private_response));
	lives_free(private_response);
	return ret;
      }
    }
    return false;
  }



  livesString multitrack::trackLabel(int track) const {
    livesString emptystr;
    if (!isActive()) return emptystr;

    if (mt_track_is_video(mainw->multitrack, track)) 
      return livesString(get_track_name(mainw->multitrack, track, FALSE), LIVES_CHAR_ENCODING_UTF8); 
    if (mt_track_is_audio(mainw->multitrack, track)) 
      return livesString(get_track_name(mainw->multitrack, track, TRUE), LIVES_CHAR_ENCODING_UTF8); 

    return emptystr;
  }


  double multitrack::FPS() const {
    if (!isActive()) return 0.;
    return mainw->multitrack->fps;
  }



  bool multitrack::setTrackLabel(int track, livesString label) const {
    if (m_lives->status() == LIVES_STATUS_PROCESSING) return false;
    if (!isActive()) return false;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_track_label(track, label.toEncoding(LIVES_CHAR_ENCODING_UTF8).c_str(), msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	bool ret=(bool)(atoi(private_response));
	lives_free(private_response);
	return ret;
      }
    }
    return false;
  }


  lives_gravity_t multitrack::gravity() const {
    if (!isActive()) return LIVES_GRAVITY_NORMAL;
    switch (mainw->multitrack->opts.grav_mode) {
    case GRAV_MODE_LEFT: return LIVES_GRAVITY_LEFT;
    case GRAV_MODE_RIGHT: return LIVES_GRAVITY_RIGHT;
    default: return LIVES_GRAVITY_NORMAL;
    }

  }


  lives_gravity_t multitrack::setGravity(lives_gravity_t grav) const {
    if (m_lives->status() == LIVES_STATUS_PROCESSING) return gravity();
    if (!isActive()) return gravity();

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_gravity((int)grav, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return gravity();
  }


  lives_insert_mode_t multitrack::insertMode() const {
    if (!isActive()) return LIVES_INSERT_MODE_NORMAL;
    switch (mainw->multitrack->opts.insert_mode) {
    default: return LIVES_INSERT_MODE_NORMAL;
    }

  }


  lives_insert_mode_t multitrack::setInsertMode(lives_insert_mode_t mode) const {
    if (m_lives->status() == LIVES_STATUS_PROCESSING) return insertMode();
    if (!isActive()) return insertMode();

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_set_insert_mode((int)mode, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	lives_free(private_response);
      }
    }
    return insertMode();
  }


  int multitrack::numAudioTracks() const {
    if (!isActive()) return 0;
    return mainw->multitrack->opts.back_audio_tracks;
  }


  int multitrack::numVideoTracks() const {
    if (!isActive()) return 0;
    return mainw->multitrack->num_video_tracks;
  }


  int multitrack::addVideoTrack(bool in_front) const {
    if (!isActive()) return -1;
    if (m_lives->isReady()) return -1.;

    spinning = true;
    msg_id = lives_random();
    ulong cbid = m_lives->addCallback(LIVES_CALLBACK_PRIVATE, private_cb, NULL);
    pthread_mutex_lock(&spin_mutex);
    if (!idle_insert_vtrack(in_front, msg_id)) {
      pthread_mutex_unlock(&spin_mutex);
      spinning = false;
      m_lives->removeCallback(cbid);
    }
    else {
      while (spinning) pthread_cond_wait(&cond_done, &spin_mutex);
      pthread_mutex_unlock(&spin_mutex);
      if (isValid()) {
	int tnum = atoi(private_response);
	lives_free(private_response);
	return tnum;
      }
    }
    return -1;
  }


  //////////////////////////////////////////////

  ////// prefs
  

  namespace prefs {
    livesString currentVideoLoadDir(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return livesString();
      return livesString(mainw->vid_load_dir, LIVES_CHAR_ENCODING_UTF8);
    }

    livesString currentAudioDir(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return livesString();
      return livesString(mainw->audio_dir, LIVES_CHAR_ENCODING_UTF8);
    }

    livesString tmpDir(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return livesString();
      return livesString(::prefs->tmpdir, LIVES_CHAR_ENCODING_FILESYSTEM);
    }

    lives_audio_source_t audioSource(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return LIVES_AUDIO_SOURCE_UNKNOWN;
      if (::prefs->audio_src == AUDIO_SRC_EXT) return LIVES_AUDIO_SOURCE_EXTERNAL;
      return LIVES_AUDIO_SOURCE_INTERNAL;
    }

    bool setAudioSource(livesApp lives, lives_audio_source_t asrc) {
      if (!lives.isReady()) return false;
      return lives.setPref(PREF_REC_EXT_AUDIO, (bool)(asrc==LIVES_AUDIO_SOURCE_EXTERNAL));
    }

    lives_audio_player_t audioPlayer(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return LIVES_AUDIO_PLAYER_UNKNOWN;
      if (::prefs->audio_player == AUD_PLAYER_SOX) return LIVES_AUDIO_PLAYER_SOX;
      if (::prefs->audio_player == AUD_PLAYER_JACK) return LIVES_AUDIO_PLAYER_JACK;
      if (::prefs->audio_player == AUD_PLAYER_PULSE) return LIVES_AUDIO_PLAYER_PULSE;
      if (::prefs->audio_player == AUD_PLAYER_MPLAYER) return LIVES_AUDIO_PLAYER_MPLAYER;
      if (::prefs->audio_player == AUD_PLAYER_MPLAYER2) return LIVES_AUDIO_PLAYER_MPLAYER2;
    }

    int audioPlayerRate(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return 0;
#ifdef ENABLE_JACK
      if (::prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL) return mainw->jackd->sample_out_rate;
#endif
#ifdef HAVE_PULSE_AUDIO
      if (::prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL) return mainw->pulsed->out_arate;
#endif
      return 0;
    }

    bool isRealtimeAudioPlayer(lives_audio_player_t player_type) {
      int ptype;
      if (player_type == LIVES_AUDIO_PLAYER_SOX) ptype = AUD_PLAYER_SOX;
      else if (player_type == LIVES_AUDIO_PLAYER_JACK) ptype = AUD_PLAYER_JACK;
      else if (player_type == LIVES_AUDIO_PLAYER_PULSE) ptype = AUD_PLAYER_PULSE;
      else if (player_type == LIVES_AUDIO_PLAYER_MPLAYER) ptype = AUD_PLAYER_MPLAYER;
      else if (player_type == LIVES_AUDIO_PLAYER_MPLAYER2) ptype = AUD_PLAYER_MPLAYER2;
      return is_realtime_aplayer(ptype);
    }

    int rteKeysVirtual(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return 0;
      return ::prefs->rte_keys_virtual;
    }

    double maxFPS(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return 0.;
      return FPS_MAX;
    }

    bool audioFollowsVideoChanges(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return false;
      return ::prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS;
    }

    bool audioFollowsFPSChanges(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return false;
      return ::prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS;
    }

    bool setAudioFollowsVideoChanges(livesApp lives, bool setting) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return false;
      return lives.setPref(PREF_AUDIO_OPTS, AUDIO_OPTS_FOLLOW_CLIPS, setting);
    }

    bool setAudioFollowsFPSChanges(livesApp lives, bool setting) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return false;
      return lives.setPref(PREF_AUDIO_OPTS, AUDIO_OPTS_FOLLOW_FPS, setting);
    }

    bool sepWinSticky(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return false;
      return ::prefs->sepwin_type==SEPWIN_TYPE_STICKY;
    }

    bool setSepWinSticky(livesApp lives, bool setting) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return false;
      return lives.setPref(PREF_SEPWIN_STICKY, setting);
    }

    bool mtExitRender(livesApp lives) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return false;
      return ::prefs->mt_exit_render;
    }

    bool setMtExitRender(livesApp lives, bool setting) {
      if (!lives.isValid() || lives.status() == LIVES_STATUS_NOTREADY) return false;
      return lives.setPref(PREF_MT_EXIT_RENDER, setting);
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

  pthread_mutex_lock(&spin_mutex); // lock mutex so that new callbacks cannot be added yet

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
	lapp->setClosures(cl);
	continue;
      }
    }
    ++it;
  }

  pthread_mutex_unlock(&spin_mutex);

}

#endif // doxygen_skip
