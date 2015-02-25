// bindings.h
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_LBINDINGS_H
#define HAS_LIVES_LBINDINGS_H

#ifndef __cplusplus 

#ifdef IS_LIBLIVES
void binding_cb (int msgnumber, const char *msgstring, uint64_t myid);
void ext_caller_check(int ret);
#endif


typedef struct {
  ulong id;
  char *msg;
} msginfo;


typedef struct {
  ulong id;
  int arglen;
  const void *vargs;
} oscdata;


#endif



void idle_show_info(const char *text, boolean blocking, ulong id);
void idle_save_set(const char *name, int arglen, const void *vargs, ulong id);

ulong *get_unique_ids(void);

int cnum_for_uid(ulong uid);


#endif //HAS_LIVES_LBINDINGS_H
