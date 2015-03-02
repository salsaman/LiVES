#include "liblives.hpp"

#include <unistd.h>
#include <iostream>

using namespace std;
using namespace lives;


int main() {
  livesApp lives;

  while (lives.status() != LIVES_STATUS_READY) sleep(1);

  set cset = lives.currentSet();

  if (cset.numClips() < 2) {
    lives.showInfo("We need at least two clips loaded for this demo.\nLet's load some more.");

    while (cset.numClips() < 2) {
      LiVESString fname = lives.chooseFileWithPreview(prefs::currentVideoLoadDir(lives), LIVES_FILE_CHOOSER_VIDEO_AUDIO);
      lives.openFile(fname);
    }

  }

  // now we have 2 or more clips open

  clip clip1 = cset.nthClip(0);
  clip clip2 = cset.nthClip(1);

  // set clip 1 as our foreground, clip 2 as our background
  clip1.switchTo();

  // map chroma blend effect to effectKey 1, mode 0
  effectKeyMap fxmap = lives.currentEffectKeyMap();
  fxmap.clear();

  effect simpleblend(lives, "", "chroma blend", "salsaman");

  fxmap[1].appendMapping(simpleblend);

  // enable the effect and start playing
  fxmap[1].setEnabled(true);

  // must do this AFTER enabling the transition
  clip2.setAsBackground();

  player p = lives.currentPlayer();

  p.play();

  while (lives.isValid()) sleep(1);

  return 0;

}
