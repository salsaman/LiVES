/// experimental feature
// transcode.c
// LiVES
// (c) G. Finch 2008 - 2017 <salsaman_lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// fast transcoding via a plugin

#ifdef LIBAV_TRANSCODE

#ifndef HAS_LIVES_TRANSCODE_H
#define HAS_LIVES_TRANSCODE_H

#define TRANSCODE_PLUGIN_NAME "libav_stream"
#define DEF_TRANSCODE_FILENAME "lives-video.flv"
#define DEF_TRANSCODE_FILENAME_FOSS "lives-video.ogm"

#define TRANSCODE_PARAM_FILENAME "fname"

#define TRANSCODE_INTENT_BASIC 1
#define TRANSCODE_INTENT_ADVANCED 2

boolean transcode(int start, int end);



#endif // HAS_LIVES_TRANSCODE_H

#endif
