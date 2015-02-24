// bindings.h
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifdef IS_LIBLIVES
void binding_cb (int msgnumber, const char *msgstring, uint64_t myid);
#endif


void lives_notify(int msgnumber,const char *msgstring);
