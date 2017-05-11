#include "liblives.hpp"

#include <unistd.h>
#include <iostream>

using namespace std;
using namespace lives;


int main(int argc, char *argv[]) {
  livesApp lives(argc - 1, &argv[1]);

  while (lives.status() != LIVES_STATUS_READY) sleep(1);

  clip c = lives.getSet().nthClip(0);

  lives.setMode(LIVES_INTERFACE_MODE_MULTITRACK);

  multitrack mt = lives.getMultitrack();

  mt.setCurrentTrack(0);

  mt.setCurrentTime(0);

  block b = mt.insertBlock(c);

  cout << "Inserted a block from clip " << c.name() << " in track " << mt.trackLabel(mt.currentTrack())
       << " at time " << b.startTime() << " duration " << b.length() << endl;

  lives.getPlayer().play();

  sleep(1);

  while (lives.isPlaying()) sleep(1);

  mt.wipeLayout(true);

  while (lives.isValid()) sleep(1);

  return 0;

}
