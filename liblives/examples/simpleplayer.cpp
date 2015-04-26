
#include "liblives.hpp"

#include <unistd.h>
#include <iostream>

static int ready;

using namespace std;
using namespace lives;


bool modeChangedCb(livesApp *lives, modeChangedInfo *info, void *data) {
  ready = 1;
  return false;
}



int main() {


  livesApp *lives = new livesApp;

  set cset = lives->getSet();

  ready = 0;

  lives->addCallback(LIVES_CALLBACK_MODE_CHANGED, modeChangedCb, NULL);

  while (!ready) sleep(1);

  cout << "Set name is " << cset.name() << endl;

  lives->showInfo("Ready to play ?");

  player p = lives->getPlayer();

  p.play();

  sleep(10);

  p.stop();

  sleep(10);

  delete lives;

}
