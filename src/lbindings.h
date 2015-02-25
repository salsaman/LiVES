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


typedef struct {
  ulong id;
  char *fname;
  double stime;
  int frames;
} opfidata;


typedef struct {
  ulong id;
  char *dir;
  char *title;
  int preview_type;
} fprev;


#endif



void idle_show_info(const char *text, boolean blocking, ulong id);
void idle_save_set(const char *name, int arglen, const void *vargs, ulong id);
void idle_choose_file_with_preview(const char *dirname, const char *title, int preview_type, ulong id);
void idle_open_file(const char *fname, double stime, int frames, ulong id);


ulong *get_unique_ids(void);

int cnum_for_uid(ulong uid);


#endif //HAS_LIVES_LBINDINGS_H
