// effects-data.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details



// struct for connecting out params to in params

typedef struct _lives_pconnect_t lives_pconnect_t;


// when an out parameter is mapped/updated, we add it to here

// when unmapped we delete it


struct _lives_pconnect_t {
  int ikey; ///< ikey is 0 based
  int imode;

  int nparams; ///< number of parameters which are linked

  // index to parameters which are to be copied (|params|)
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

// chain any output data into fx key/mode
void pconx_chain_data(int key, int mode);



// alpha channels


// struct for connecting out alphas to in alphas

typedef struct _lives_cconnect_t lives_cconnect_t;


// when an out alpha is mapped/updated, we add it to here

// when unmapped we delete it



struct _lives_cconnect_t {
  int ikey; ///< ikey is 0 based
  int imode;

  int nchans; ///< number of alpha channels which are linked (|chans|)

  // index to chans which are to be copied
  int *chans;

  // number of connections for each channel
  int *nconns;

  // each param is mapped to nconns[i] of these
  int *okey;  ///< okey is 0 based
  int *omode;
  int *ocnum;

  lives_cconnect_t *next;
  
};



/// add a new connection from out_param ikey/imode/ipnum to in_param okey/omode/opnum
void cconx_add_connection(int ikey, int imode, int icnum, int okey, int omode, int ocnum);


/// return if okey/omode/ochan is mapped to an out alpha
weed_plant_t *cconx_get_out_chan(int okey, int omode, int opnum);


// chain any output data into fx key/mode
boolean cconx_chain_data(int key, int mode);
