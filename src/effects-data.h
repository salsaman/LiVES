// effects-data.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012 (salsaman@xs4all.nl,salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details







// struct for connecting out params to in params
typedef struct {
  int ikey;
  int imode;
  int *okeys;
  int *omodes;
  weed_plant_t **vals;
} lives_pconnect_t;




