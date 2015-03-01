// bindings.h
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_LBINDINGS_H
#define HAS_LIVES_LBINDINGS_H

#ifndef __cplusplus 

#ifdef IS_LIBLIVES
void binding_cb (int msgnumber, const char *msgstring, ulong myid);

#endif

#endif

boolean idle_show_info(const char *text, boolean blocking, ulong id);
boolean idle_save_set(const char *name, int arglen, const void *vargs, ulong id);
boolean idle_choose_file_with_preview(const char *dirname, const char *title, int preview_type, ulong id);
boolean idle_open_file(const char *fname, double stime, int frames, ulong id);
boolean idle_set_interactive(boolean setting, ulong id);
boolean idle_choose_set(ulong id);
boolean idle_reload_set(const char *setname, ulong id);
boolean idle_set_pref_bool(int prefidx, boolean val, ulong id);
boolean idle_set_pref_int(int prefidx, int val, ulong id);
boolean idle_switch_clip(int type, int cnum, ulong id);
boolean idle_unmap_effects(ulong id);

ulong *get_unique_ids(void);
int cnum_for_uid(ulong uid);
int get_first_fx_matched(const char *package, const char *fxname, const char *author, int version);


#endif //HAS_LIVES_LBINDINGS_H
