// effects-data.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#define FX_DATA_WILDCARD -1000000

#define FX_DATA_KEY_SUBTITLES -1
#define FX_DATA_KEY_PLAYBACK_PLUGIN -2

#define EXTRA_PARAMS_OUT 1
#define EXTRA_PARAMS_IN 1

#define FX_DATA_PARAM_ACTIVE -1

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

// free all connections (and set mainw->pconx to NULL)
void pconx_delete_all();

void pconx_delete(int okey, int omode, int ocnum, int ikey, int imode, int icnum);

void pconx_remap_mode(int key, int omode, int nmode);

// chain any output data into fx key/mode
boolean pconx_chain_data(int key, int mode);

// return list of in keys/modes/params/autoscale
char *pconx_list(int okey, int omode, int opnum);

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

// free all connections (and set mainw->cconx to NULL)
void cconx_delete_all();

void cconx_delete(int okey, int omode, int ocnum, int ikey, int imode, int icnum);

void cconx_remap_mode(int key, int omode, int nmode);

// chain any output data into fx key/mode
boolean cconx_chain_data(int key, int mode);

// return list of in chaannels/modes/params
char *cconx_list(int okey, int omode, int ocnum);

boolean cconx_chain_data_internal(weed_plant_t *ichan);


//////////////////////////////////////////////////////////

void override_if_active_input(int hotkey);
void end_override_if_activate_output(int hotkey);

////////////////////////////////////////////////////////////

typedef struct {
  weed_plant_t *filter;

  int okey;
  int omode;
  int num_alpha;
  int num_params;
  int ntabs;

  lives_cconnect_t *cconx;
  lives_pconnect_t *pconx;

  LiVESWidget *conx_dialog;
  LiVESWidget *acbutton;
  LiVESWidget *apbutton;
  LiVESWidget *disconbutton;

  LiVESWidget **clabel;
  LiVESWidget **pclabel;
  LiVESWidget **cfxcombo;
  LiVESWidget **pfxcombo;
  LiVESWidget **pcombo;
  LiVESWidget **ccombo;
  LiVESWidget **acheck;
  LiVESWidget **add_button;
  LiVESWidget **del_button;
  LiVESWidget *allcheckc;
  LiVESWidget *allcheck_label;

  LiVESWidget *tablec;
  LiVESWidget *tablep;

  // table row counts
  int trowsc;
  int trowsp;

  // # dislay rows for each param/channel
  int *dispc;
  int *dispp;

  int *ikeys;
  int *imodes;
  int *idx;

  ulong *dpc_func;
  ulong *dpp_func;
  ulong *acheck_func;

} lives_conx_w;




LiVESWidget *make_datacon_window(int key, int mode);

int pconx_check_connection(weed_plant_t *ofilter, int opnum, int ikey, int imode, int ipnum, boolean setup, weed_plant_t **iparam_ret,
                           int *idx_ret,
                           int *okey, int *omode, int *oopnum);

int cconx_check_connection(int ikey, int imode, int icnum, boolean setup, weed_plant_t **ichan_ret, int *idx_ret, int *okey, int *omode,
                           int *ocnum);



boolean feeds_to_video_filters(int okey, int omode);
boolean feeds_to_audio_filters(int okey, int omode);
