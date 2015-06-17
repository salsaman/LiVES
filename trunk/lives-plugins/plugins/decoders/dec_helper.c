// LiVES - decoder helper
// (c) Th. Berger 2015 <loki@lokis-chaos.de>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "decplugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

double get_fps(const char *uri) {
  // use mplayer to get fps if we can...it seems to have some magical way
  FILE *fp;
  int rc;
  double ret = -1.;
  const char *binary;
  char buffer[1024];
  char cmd[1024];

#define FIND_PLAYER(player) \
    rc = system("which " #player); \
    if ( rc == 0 ) { binary = #player; goto found_player; }

  FIND_PLAYER(mplayer)
  FIND_PLAYER(mplayer2)
  FIND_PLAYER(mpv)

  // if we get here, we did not find a player
  goto exit;

found_player:
  snprintf(cmd,1024,"LANGUAGE=en LANG=en %s \"%s\" -identify -frames 0 2>/dev/null | grep ID_VIDEO_FPS",binary,uri);
  fp = popen(cmd,"r");

  fgets(buffer,1024,fp);
  if (!(strncmp(buffer,"ID_VIDEO_FPS=",13))) {
    ret = strtod(buffer+13,NULL);
  }
  pclose(fp);
exit:
  return ret;
}
