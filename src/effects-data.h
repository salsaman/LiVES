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
  int okey; ///< okey is 0 based
  int omode;

  int nparams; ///< number of parameters which are linked

  // index to parameters which are to be copied (|params|)
  int *params;

  // number of connections for each param
  int *nconns;

  // each param is mapped to nconns[i] of these
  int *ikey;  ///< ikey is 0 based
  int *imode;
  int *ipnum;
  boolean *autoscale;

  lives_pconnect_t *next;
  
};



/// add a new connection from out_param okey/omode/opnum to in_param ikey/imode/ipnum
void pconx_add_connection(int okey, int omode, int opnum, int ikey, int imode, int ipnum, boolean autoscale);

/// return if ikey/imode/ipnum is mapped to an out param
weed_plant_t *pconx_get_out_param(int ikey, int imode, int ipnum, boolean *autoscale);

// free all connections (and set mainw->pconx to NULL)
void pconx_delete_all();

void pconx_delete(int okey, int omode, int ocnum, int ikey, int imode, int icnum);

void pconx_remap_mode(int key, int omode, int nmode);

// chain any output data into fx key/mode
boolean pconx_chain_data(int key, int mode);

// return list of in keys/modes/params/autoscale
gchar *pconx_list(int okey, int omode, int opnum);

// special version for compound fx internal connections
boolean pconx_chain_data_internal(weed_plant_t *inst);


// alpha channels


// struct for connecting out alphas to in alphas

typedef struct _lives_cconnect_t lives_cconnect_t;


// when an out alpha is mapped/updated, we add it to here

// when unmapped we delete it



struct _lives_cconnect_t {
  int okey; ///< okey is 0 based
  int omode;

  int nchans; ///< number of alpha channels which are linked (|chans|)

  // index to chans which are to be copied
  int *chans;

  // number of connections for each channel
  int *nconns;

  // each param is mapped to nconns[i] of these
  int *ikey;  ///< ikey is 0 based
  int *imode;
  int *icnum;

  lives_cconnect_t *next;
  
};



/// add a new connection from out_chan okey/omode/ocnum to in_chan ikey/imode/icnum
void cconx_add_connection(int okey, int omode, int ocnum, int ikey, int imode, int icnum);

/// return if ikey/imode/ichan is mapped to an out alpha
weed_plant_t *cconx_get_out_chan(int ikey, int imode, int ipnum);

// free all connections (and set mainw->cconx to NULL)
void cconx_delete_all();

void cconx_delete(int okey, int omode, int ocnum, int ikey, int imode, int icnum);

void cconx_remap_mode(int key, int omode, int nmode);

// chain any output data into fx key/mode
boolean cconx_chain_data(int key, int mode);

// return list of in chaannels/modes/params
gchar *cconx_list(int okey, int omode, int ocnum);

boolean cconx_chain_data_internal(weed_plant_t *ichan);


//////////////////////////////////////////////////////////


typedef struct {
  int okey;
  int omode;
  int num_alpha;
  int num_params;
  int ntabs;
  GtkWidget *conx_dialog;

  // per tab part
  GtkWidget **cfxcombo;
  GtkWidget **pfxcombo;
  GtkWidget **pcombo;
  GtkWidget **ccombo;
  GtkWidget **acheck;
  GtkWidget *allcheckc;
  int *ikeys;
  int *imodes;
  int *idx;
  gulong *dpp_func;
  gulong *acheck_func;
} lives_conx_w;


GtkWidget *make_datacon_window(int key, int mode);

