/// experimental feature
// transcode.c
// LiVES
// (c) G. Finch 2008 - 2019 <salsaman_lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// fast transcoding via a plugin

#ifdef LIBAV_TRANSCODE

#ifndef HAS_LIVES_TRANSCODE_H
#define HAS_LIVES_TRANSCODE_H

#define TRANSCODE_PLUGIN_NAME "libav_stream"
#define DEF_TRANSCODE_FILENAME "lives-video"

#define TRANSCODE_PARAM_FILENAME "fname"

// stages for internal transcoding
boolean transcode_prep(void);

// if this returns FALSE, transcode_cleanup(mainw->vpp) must be called
char *transcode_get_params(char *fname_def);
void transcode_cleanup(_vid_playback_plugin *vpp);

boolean transcode_clip(int start, int end, boolean internal, char *def_pname);

#endif // HAS_LIVES_TRANSCODE_H

#endif
