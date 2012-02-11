// effects-data.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details



// struct for connecting out params to in params

typedef struct _lives_pconnect_t lives_pconnect_t;


// when an out parameter is mapped/updated, we add it to here

// when unmapped we delete it

// when fx is instantiated, we map the inst and unmap after destroy


struct _lives_pconnect_t {
  int ikey; ///< ikey is 0 based
  int imode;

  int nparams; ///< number of parameters which are linked

  // index to parameters which are to be copied
  int *params;

  // number of connections for each param
  int *nconns;

  // each param is mapped to nconns[i] of these
  int *okey;  ///< okey is 0 based
  int *omode;
  int *opnum;

  lives_pconnect_t *next;
  
};



/// add a new connection from out_param ikey/imode/ipnum to in_param okey/omode/opnum
void pconx_add_connection(int ikey, int imode, int ipnum, int okey, int omode, int opnum);

/// return if okey/omode/opnum is mapped to an out param
weed_plant_t *pconx_get_out_param(int okey, int omode, int opnum);

// free all connections (and set mainw->pconx to NULL)
void pconx_free_all();
