// effects-data.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2019 (salsaman+lives@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// functions for chaining and data passing between fx plugins

//#define DEBUG_PCONX

#include "main.h"
#include "effects.h"
#include "ce_thumbs.h"
#include "omc-learn.h"

static lives_pconnect_t *spconx;
static lives_cconnect_t *scconx;

static boolean do_chan_connected_query(lives_conx_w *, int okey, int omode, int ocnum, boolean is_same_key);
static boolean do_param_connected_query(lives_conx_w *, int okey, int omode, int opnum, boolean is_same_key);
static void do_param_incompatible_error(lives_conx_w *);

static void ptable_row_add_standard_widgets(lives_conx_w *, int idx);
static void ptable_row_add_variable_widgets(lives_conx_w *, int idx, int row, int pidx);

static void ctable_row_add_standard_widgets(lives_conx_w *, int idx);
static void ctable_row_add_variable_widgets(lives_conx_w *, int idx, int row, int cidx);

static void padd_clicked(LiVESWidget *button, livespointer user_data);
static void cadd_clicked(LiVESWidget *button, livespointer user_data);

static void dfxp_changed(LiVESWidget *, livespointer conxwp);

static weed_plant_t *active_dummy = NULL;

static LiVESTreeModel *pmodel;
static LiVESTreeModel *cmodel;

static char *lctext;

#define BW (40. * widget_opts.scale)
#define BH (40. * widget_opts.scale)

#ifdef DEBUG_PCONX
static void dump_connections(void) {
  lives_pconnect_t *pconx = mainw->pconx;
  while (pconx) {
    int p = 0;
    for (int i = 0; i < pconx->nparams; i++) {
      int pnum = pconx->params[i];
      for (int j = 0; j < pconx->nconns[i + 1]; j++) {
        p++;
	// *INDENT-OFF*
      }}
    // *INDENT-ON*
    pconx = pconx->next;
  }
}
#endif

static char *get_param_name(weed_plant_t *param, int pnum, boolean is_in) {
  char *name = weed_get_string_value(param, WEED_LEAF_NAME, NULL);
  if (!*name) {
    lives_free(name);
    name = NULL;
  }
  if (!name) {
    if (is_in) name = lives_strdup_printf(_("In param %d"), pnum);
    else name = lives_strdup_printf(_("Out param %d"), pnum);
  }
  return name;
}


static char *get_chan_name(weed_plant_t *chan, int cnum, boolean is_in) {
  char *name = weed_get_string_value(chan, WEED_LEAF_NAME, NULL);
  if (!*name) {
    lives_free(name);
    name = NULL;
  }
  if (!name) {
    if (is_in) name = lives_strdup_printf(_("In channel %d"), cnum);
    else name = lives_strdup_printf(_("Out channel %d"), cnum);
  }
  return name;
}


static void switch_fx_state(int hotkey) {
  // switch effect state when a connection to ACTIVATE is present
  uint32_t last_grabbable_effect = mainw->last_grabbable_effect;
  // use -hotkey to indicate auto
  rte_key_toggle(-hotkey);
  mainw->last_grabbable_effect = last_grabbable_effect;
}


void override_if_active_input(int hotkey) {
  // if we have a connection to ACTIVATE, allow override if the user changes the state from the kbd
  lives_pconnect_t *pconx = mainw->pconx;

  int totcons;
  int imode = rte_key_getmode(hotkey);

  register int i, j;

  while (pconx) {
    totcons = 0;
    j = 0;
    for (i = 0; i < pconx->nparams; i++) {
      totcons += pconx->nconns[i];
      for (; j < totcons; j++) {
        if (pconx->ikey[j] == hotkey - 1 && pconx->imode[j] == imode && pconx->ipnum[j] == FX_DATA_PARAM_ACTIVE) {
          // out param is "ACTIVATED"
          // abuse "autoscale" for this
          pconx->autoscale[i] = TRUE;
          return;
	  // *INDENT-OFF*
        }}}
    pconx = pconx->next;
    // *INDENT-ON*
  }
}


void end_override_if_activate_output(int hotkey) {
  // if we activate an effect and it is connected to ACTIVATE another effect, end any user override
  lives_pconnect_t *pconx = mainw->pconx;

  int totcons;
  int omode = rte_key_getmode(hotkey);

  int j;

  while (pconx) {
    if (pconx->okey == hotkey && pconx->omode == omode) {
      totcons = 0;
      j = 0;
      for (int i = 0; i < pconx->nparams; i++) {
        totcons += pconx->nconns[i];
        for (; j < totcons; j++) {
          if (pconx->ipnum[j] == FX_DATA_PARAM_ACTIVE) {
            // abuse "autoscale" for this
            pconx->autoscale[j] = FALSE;
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*
    pconx = pconx->next;
  }
}


void pconx_delete_all(void) {
  lives_pconnect_t *pconx = mainw->pconx, *pconx_next;
  int i;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) pthread_mutex_lock(&mainw->fx_mutex[i]);

  while (pconx) {
    pconx_next = pconx->next;
    lives_free(pconx->params); lives_free(pconx->nconns); lives_free(pconx->last_boolval);
    lives_free(pconx->ikey); lives_free(pconx->imode); lives_free(pconx->ipnum);
    lives_free(pconx->autoscale); lives_free(pconx);
    pconx = pconx_next;
  }
  mainw->pconx = NULL;

  for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) pthread_mutex_unlock(&mainw->fx_mutex[i]);
}


static lives_pconnect_t *pconx_new(int okey, int omode) {
  lives_pconnect_t *pconx = (lives_pconnect_t *)lives_calloc(sizeof(struct _lives_pconnect_t), 1);
  pconx->okey = okey;
  pconx->omode = omode;
  return pconx;
}


static lives_pconnect_t *pconx_copy(lives_pconnect_t *spconx) {
  lives_pconnect_t *pconx = NULL, *dpconx, *last_dpconx;

  int totcons = 0;

  int j = 0;

  while (spconx) {
    dpconx = pconx_new(spconx->okey, spconx->omode);
    if (!pconx) pconx = dpconx;
    else last_dpconx->next = dpconx;

    dpconx->nparams = spconx->nparams;

    dpconx->nconns = (int *)lives_malloc(dpconx->nparams * sizint);
    dpconx->params = (int *)lives_malloc(dpconx->nparams * sizint);
    dpconx->last_boolval = (int *)lives_malloc(dpconx->nparams * sizint);

    dpconx->ikey = dpconx->imode = dpconx->ipnum = NULL;
    dpconx->autoscale = NULL;

    j = 0;

    for (int i = 0; i < dpconx->nparams; i++) {
      dpconx->params[i] = spconx->params[i];
      dpconx->nconns[i] = spconx->nconns[i];
      dpconx->last_boolval[i] = spconx->last_boolval[i];
      totcons += dpconx->nconns[i];

      dpconx->ikey = (int *)lives_realloc(dpconx->ikey, totcons * sizint);
      dpconx->imode = (int *)lives_realloc(dpconx->imode, totcons * sizint);
      dpconx->ipnum = (int *)lives_realloc(dpconx->ipnum, totcons * sizint);
      dpconx->autoscale = (boolean *)lives_realloc(dpconx->autoscale, totcons * sizint);

      while (j < totcons) {
        dpconx->ikey[j] = spconx->ikey[j];
        dpconx->imode[j] = spconx->imode[j];
        dpconx->ipnum[j] = spconx->ipnum[j];
        dpconx->autoscale[j] = spconx->autoscale[j];
        j++;
      }
    }

    spconx = spconx->next;
    last_dpconx = dpconx;
  }

  return pconx;
}


char *pconx_list(int okey, int omode, int opnum) {
  char *st1 = lives_strdup(""), *st2;
  lives_pconnect_t *pconx = mainw->pconx;
  int totcons = 0;

  while (pconx) {
    if (pconx->okey == okey && pconx->omode == omode) {
      for (int i = 0; i < pconx->nparams; i++) {
        if (pconx->params[i] == opnum) {
          for (int j = totcons; j < totcons + pconx->nconns[i]; j++) {
            if (!*st1)
              st2 = lives_strdup_printf("%d %d %d %d", pconx->ikey[j] + 1, pconx->imode[j] + 1, pconx->ipnum[j],
                                        pconx->autoscale[j]);
            else
              st2 = lives_strdup_printf("%s %d %d %d %d", st1, pconx->ikey[j] + 1, pconx->imode[j] + 1,
                                        pconx->ipnum[j], pconx->autoscale[j]);
            lives_free(st1);
            st1 = st2;
          }
          return st1;
        }
        totcons += pconx->nconns[i];
      }
      return st1;
    }
    pconx = pconx->next;
  }
  return st1;
}


void pconx_delete(int okey, int omode, int opnum, int ikey, int imode, int ipnum) {
  lives_pconnect_t *pconx = mainw->pconx, *pconx_next, *pconx_prev = NULL;

  int i, j = 0, k;
  int totcons = 0, maxcons = 0;

  if (okey >= 0 && okey != FX_DATA_WILDCARD)
    for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++)
      pthread_mutex_lock(&mainw->fx_mutex[i]);

  while (pconx) {
    pconx_next = pconx->next;

#ifdef DEBUG_PCONX
    g_print("Deletion check, want %d / %d, found %d / %d\n", okey, omode, pconx->okey, pconx->omode);
#endif
    if ((okey == FX_DATA_WILDCARD || pconx->okey == okey) && (omode == FX_DATA_WILDCARD || pconx->omode == omode)) {

#ifdef DEBUG_PCONX
      g_print("GOT MATCH\n");
#endif
      if (opnum == FX_DATA_WILDCARD && ikey == FX_DATA_WILDCARD && imode == FX_DATA_WILDCARD
          && ipnum == FX_DATA_WILDCARD) {
        //g_print("rem all cons from %d %d to any param\n",okey,omode);

        // delete entire node
        lives_free(pconx->params); lives_free(pconx->last_boolval); lives_free(pconx->nconns);
        lives_free(pconx->ikey); lives_free(pconx->imode); lives_free(pconx->ipnum);
        lives_free(pconx->autoscale); lives_free(pconx);
        if (mainw->pconx == pconx) mainw->pconx = pconx_next;
        else pconx_prev->next = pconx_next;
        if (okey >= 0 && okey != FX_DATA_WILDCARD) for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++)
            pthread_mutex_unlock(&mainw->fx_mutex[i]);
        return;
      }

      maxcons = 0;
      totcons = 0;
      j = 0;

      for (i = 0; i < pconx->nparams; i++) {
        maxcons += pconx->nconns[i];
      }

      for (i = 0; pconx && i < pconx->nparams; i++) {
#ifdef DEBUG_PCONX
        g_print("Checking oparams, want %d, got %d with %d connections\n", opnum, pconx->params[i], pconx->nconns[i]);
#endif
        totcons += pconx->nconns[i];

        if (opnum != FX_DATA_WILDCARD && pconx->params[i] != opnum) {
          j = totcons;
          continue;
        }

        for (; j < totcons; j++) {
#ifdef DEBUG_PCONX
          g_print("For inputs, want %d / %d (%d), found %d / %d (%d)\n", ikey, imode, ipnum, pconx->ikey[j], pconx->imode[j],
                  pconx->ipnum[j]);
#endif
          if ((ikey == FX_DATA_WILDCARD || pconx->ikey[j] == ikey)
              && (imode == FX_DATA_WILDCARD || pconx->imode[j] == imode)
              && (ipnum == FX_DATA_WILDCARD
                  || (ipnum == FX_DATA_WILDCARD_KEEP_ACTIVATED && pconx->ipnum[j] != FX_DATA_PARAM_ACTIVE)
                  || pconx->ipnum[j] == ipnum)) {
#ifdef DEBUG_PCONX
            g_print("removing connection to %d / %d param (%d)\n", ikey, imode, ipnum);
#endif
            maxcons--;
            for (k = j; k < maxcons; k++) {
              pconx->ikey[k] = pconx->ikey[k + 1];
              pconx->imode[k] = pconx->imode[k + 1];
              pconx->ipnum[k] = pconx->ipnum[k + 1];
              pconx->autoscale[k] = pconx->autoscale[k + 1];
            }

            pconx->ikey = (int *)lives_realloc(pconx->ikey, maxcons * sizint);
            pconx->imode = (int *)lives_realloc(pconx->imode, maxcons * sizint);
            pconx->ipnum = (int *)lives_realloc(pconx->ipnum, maxcons * sizint);
            pconx->autoscale = (boolean *)lives_realloc(pconx->autoscale, maxcons * sizint);

            pconx->nconns[i]--;

            if (pconx->nconns[i] == 0) {
              pconx->nparams--;
              for (k = i; k < pconx->nparams; k++) {
                pconx->params[k] = pconx->params[k + 1];
                pconx->last_boolval[k] = pconx->last_boolval[k + 1];
                pconx->nconns[k] = pconx->nconns[k + 1];
              }

              if (pconx->nparams == 0) {
                // delete entire node
                lives_free(pconx->params); lives_free(pconx->nconns); lives_free(pconx->ikey);
                lives_free(pconx->imode); lives_free(pconx->ipnum); lives_free(pconx->last_boolval);
                lives_free(pconx->autoscale);
                lives_free(pconx);
                if (mainw->pconx == pconx) {
                  mainw->pconx = pconx_next;
                  pconx = NULL;
                } else {
                  pconx = pconx_prev;
                  pconx->next = pconx_next;
                }
              } else {
                pconx->nconns = (int *)lives_realloc(pconx->nconns, pconx->nparams * sizint);
		// *INDENT-OFF*
	      }}}}}}
      // *INDENT-ON*

    pconx_prev = pconx;
    pconx = pconx_next;
  }
  if (okey >= 0 && okey != FX_DATA_WILDCARD)
    for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) pthread_mutex_unlock(&mainw->fx_mutex[i]);
}


void pconx_remap_mode(int key, int omode, int nmode) {
  lives_pconnect_t *pconx = mainw->pconx;

  register int i, j, totcons;

  while (pconx) {
    if (pconx->okey == key && pconx->omode == omode) {
      pconx->omode = nmode;
    }
    j = 0;
    totcons = 0;
    for (i = 0; i < pconx->nparams; i++) {
      totcons += pconx->nconns[i];
      for (; j < totcons; j++) {
        if (pconx->ikey[j] == key && pconx->imode[j] == omode) {
          pconx->imode[j] = nmode;
	  // *INDENT-OFF*
        }}}
    // *INDENT-ON*
    pconx = pconx->next;
  }
}


static void pconx_append(lives_pconnect_t *pconx) {
  lives_pconnect_t *opconx = mainw->pconx;
  lives_pconnect_t *last_pconx = opconx;

  while (opconx) {
    last_pconx = opconx;
    opconx = opconx->next;
  }

  if (last_pconx) last_pconx->next = pconx;
  if (!mainw->pconx) mainw->pconx = pconx;
}


static lives_pconnect_t *pconx_find(int okey, int omode) {
  //
  lives_pconnect_t *pconx = mainw->pconx;
  while (pconx) {
    if (pconx->okey == okey && pconx->omode == omode) {
      return pconx;
    }
    pconx = pconx->next;
  }
  return NULL;
}


static int pconx_get_numcons(lives_conx_w * conxwp, int pnum) {
  // get displayed number
  int totcons = 0;

  if (pnum != FX_DATA_WILDCARD) return conxwp->dispp[pnum + EXTRA_PARAMS_OUT];

  for (int j = 0; j < conxwp->num_params; j++) {
    totcons += conxwp->dispp[j];
  }

  return totcons;
}


static int pconx_get_nconns(lives_pconnect_t *pconx, int pnum) {
  // get actual number of connections
  int totcons = 0;

  if (!pconx) return 0;

  for (int j = 0; j < pconx->nparams; j++) {
    if (pnum != FX_DATA_WILDCARD) {
      if (pconx->params[j] == pnum) return pconx->nconns[j];
    } else totcons += pconx->nconns[j];
  }
  return totcons;
}


static lives_pconnect_t *pconx_add_connection_private(int okey, int omode, int opnum, int ikey, int imode, int ipnum,
    boolean autoscale) {
  lives_pconnect_t *pconx;
  int posn = 0, totcons = 0;
  int i, j;

  // delete any existing connection to the input param
  pconx_delete(FX_DATA_WILDCARD, FX_DATA_WILDCARD, FX_DATA_WILDCARD, ikey, imode, ipnum);
  pconx = pconx_find(okey, omode);

  if (ikey >= 0) for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) pthread_mutex_lock(&mainw->fx_mutex[i]);

  if (!pconx) {
    // add whole new node
    pconx = pconx_new(okey, omode);
    pconx_append(pconx);
  } else {
    // see if already in params
    for (i = 0; i < pconx->nparams; i++) {
      if (pconx->params[i] == opnum) {
        // located !
        // add connection to existing

        for (j = 0; j < pconx->nparams; j++) {
          if (j < i) {
            // calc posn
            posn += pconx->nconns[j];
          }
          totcons += pconx->nconns[j];
        }

        // if already there, do not add again, just update autoscale
        for (j = posn; j < posn + pconx->nconns[i]; j++) {
          if (pconx->ikey[j] == ikey && pconx->imode[j] == imode && pconx->ipnum[j] == ipnum) {
            pconx->autoscale[j] = autoscale;
            if (ikey >= 0) for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) pthread_mutex_unlock(&mainw->fx_mutex[i]);
            return pconx;
          }

          // add in order key/mode/chan
          if (pconx->ikey[j] > ikey || (pconx->ikey[j] == ikey && pconx->imode[j] > imode) ||
              (pconx->ikey[j] == ikey && pconx->imode[j] == imode && pconx->ipnum[j] > ipnum)) break;
        }

        // increment nconns for this param
        pconx->nconns[i]++;

        totcons++;

        posn = j;

        // make space for new
        pconx->ikey = (int *)lives_realloc(pconx->ikey, totcons * sizint);
        pconx->imode = (int *)lives_realloc(pconx->imode, totcons * sizint);
        pconx->ipnum = (int *)lives_realloc(pconx->ipnum, totcons * sizint);
        pconx->autoscale = (boolean *)lives_realloc(pconx->autoscale, totcons * sizint);

        // move up 1
        for (j = totcons - 1; j > posn; j--) {
          pconx->ikey[j] = pconx->ikey[j - 1];
          pconx->imode[j] = pconx->imode[j - 1];
          pconx->ipnum[j] = pconx->ipnum[j - 1];
          pconx->autoscale[j] = pconx->autoscale[j - 1];
        }

        // insert at posn
        pconx->ikey[posn] = ikey;
        pconx->imode[posn] = imode;
        pconx->ipnum[posn] = ipnum;
        pconx->autoscale[posn] = autoscale;

        if (ikey >= 0) for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) pthread_mutex_unlock(&mainw->fx_mutex[i]);

        return pconx;
      }
    }

    // so, we have data for key/mode but this is a new param to be mapped

    for (i = 0; i < pconx->nparams; i++) {
      totcons += pconx->nconns[i];
    }

    totcons++;

    pconx->nparams++;
    posn = pconx->nparams;

    // make space for new
    pconx->nconns = (int *)lives_realloc(pconx->nconns, posn * sizint);
    pconx->params = (int *)lives_realloc(pconx->params, posn * sizint);
    pconx->last_boolval = (int *)lives_realloc(pconx->last_boolval, posn * sizint);

    pconx->ikey = (int *)lives_realloc(pconx->ikey, totcons * sizint);
    pconx->imode = (int *)lives_realloc(pconx->imode, totcons * sizint);
    pconx->ipnum = (int *)lives_realloc(pconx->ipnum, totcons * sizint);
    pconx->autoscale = (boolean *)lives_realloc(pconx->autoscale, totcons * sizint);

    pconx->params[posn - 1] = opnum;

    pconx->last_boolval[posn - 1] = WEED_FALSE;  // TODO ***: get value from default

    pconx->nconns[posn - 1] = 1;

    posn = totcons - 1;

    // insert at posn
    pconx->ikey[posn] = ikey;
    pconx->imode[posn] = imode;
    pconx->ipnum[posn] = ipnum;
    pconx->autoscale[posn] = autoscale;

#ifdef DEBUG_PCONX
    g_print("added another pconx from %d %d %d to %d %d %d\n", okey, omode, opnum, ikey, imode, ipnum);
    dump_connections();
#endif

    if (ikey >= 0) for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) pthread_mutex_unlock(&mainw->fx_mutex[i]);

    return pconx;
  }

  // add new

  totcons = pconx_get_nconns(pconx, FX_DATA_WILDCARD) + 1;
  pconx->nparams++;

  pconx->nconns = (int *)lives_realloc(pconx->params, pconx->nparams * sizint);
  pconx->nconns[pconx->nparams - 1] = 1;

  pconx->params = (int *)lives_realloc(pconx->params, pconx->nparams * sizint);
  pconx->params[pconx->nparams - 1] = opnum;

  pconx->last_boolval = (int *)lives_realloc(pconx->last_boolval, pconx->nparams * sizint);
  pconx->last_boolval[pconx->nparams - 1] = WEED_FALSE; // TODO: ****

  pconx->ikey = (int *)lives_realloc(pconx->ikey, totcons * sizint);
  pconx->ikey[totcons - 1] = ikey;

  pconx->imode = (int *)lives_realloc(pconx->imode, totcons * sizint);
  pconx->imode[totcons - 1] = imode;

  pconx->ipnum = (int *)lives_realloc(pconx->ipnum, totcons * sizint);
  pconx->ipnum[totcons - 1] = ipnum;

  pconx->autoscale = (boolean *)lives_realloc(pconx->autoscale, totcons * sizint);
  pconx->autoscale[totcons - 1] = autoscale;

#ifdef DEBUG_PCONX
  g_print("added new pconx from %d %d %d to %d %d %d (%d)\n", okey, omode, opnum, ikey, imode, ipnum, autoscale);
#endif

  if (ikey >= 0) for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) pthread_mutex_unlock(&mainw->fx_mutex[i]);

  return pconx;
}


void pconx_add_connection(int okey, int omode, int opnum, int ikey, int imode, int ipnum, boolean autoscale) {
  pconx_add_connection_private(okey, omode, opnum, ikey, imode, ipnum, autoscale);
}


static weed_plant_t *pconx_get_out_param(boolean use_filt, int ikey, int imode, int ipnum, int *okey, int *omode, int *opnum,
    int *autoscale) {
  // walk all pconx and find one which has ikey/imode/ipnum as destination
  // then all we need do is copy the "value" leaf

  // use_filt is TRUE if we should use the filter template (otherwise we use the instance)

  // TODO: OMC

  lives_pconnect_t *pconx = mainw->pconx;
  weed_plant_t *inst = NULL, *filter = NULL;
  int totcons, i, j;

  while (pconx) {
    if (!use_filt) {
      if (mainw->is_rendering) return NULL;
      else {
        inst = rte_keymode_get_instance(pconx->okey + 1, pconx->omode);
      }
      if (inst) filter = weed_instance_get_filter(inst, TRUE); // inst could be NULL if we connected to "Activated"
      else filter = NULL;
    } else {
      inst = NULL;
      filter = rte_keymode_get_filter(pconx->okey + 1, pconx->omode);
      if (!filter) {
        pconx = pconx->next;
        continue;
      }
    }
    totcons = 0;
    j = 0;
    for (i = 0; i < pconx->nparams; i++) {
      totcons += pconx->nconns[i];
      for (; j < totcons; j++) {
        if (pconx->ikey[j] == ikey && pconx->imode[j] == imode && pconx->ipnum[j] == ipnum) {
          weed_plant_t *param = NULL;

          // out param is "ACTIVATED"
          if (pconx->params[i] == FX_DATA_PARAM_ACTIVE) {
            weed_plant_t *gui = weed_instance_get_gui(inst, FALSE);
            pthread_mutex_lock(&mainw->fxd_active_mutex);
            if (!active_dummy) {
              active_dummy = weed_plant_new(WEED_PLANT_PARAMETER);
              weed_set_plantptr_value(active_dummy, WEED_LEAF_TEMPLATE, NULL);
            }

            weed_set_boolean_value(active_dummy, WEED_LEAF_VALUE, inst != NULL
                                   && (!gui || !weed_plant_has_leaf(gui, WEED_LEAF_EASE_OUT)));
            param = active_dummy;
            pthread_mutex_unlock(&mainw->fxd_active_mutex);
          } else {
            if (use_filt) {
              weed_plant_t **outparams = weed_get_plantptr_array(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL);
              if (pconx->params[i] < weed_leaf_num_elements(filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) {
                param = outparams[pconx->params[i]];
              }
              lives_free(outparams);
            } else {
              if (!inst) return NULL;
              param = weed_inst_out_param(inst, pconx->params[i]);
            }
          }
          if (okey) *okey = pconx->okey;
          if (omode) *omode = pconx->omode;
          if (opnum) *opnum = pconx->params[i];
          if (autoscale) *autoscale = pconx->autoscale[j];
          if (inst) weed_instance_unref(inst);
          return param;
        }
      }
    }
    pconx = pconx->next;
    if (inst) weed_instance_unref(inst);
  }

  return NULL;
}


static boolean params_compatible(weed_plant_t *sparam, weed_plant_t *dparam) {
  // allowed conversions
  // type -> type

  // bool -> double, bool -> int, bool -> string, (bool -> int64)
  // int -> double, int -> string,  (int -> int64)
  // double -> string
  // (int64 -> string)

  // int[3x]/double[3x] -> colourRGB
  // int[4x]/double[4x] -> colourRGBA
  //

  weed_plant_t *dptmpl = NULL;

  int dtype = 0, stype = 0;
  int ndvals = 0, nsvals = 0;
  int ptype, dflags = 0;

  if (dparam == active_dummy) {
    dptmpl = NULL;
    dtype = WEED_SEED_BOOLEAN;
    ndvals = 1;
  } else {
    if (WEED_PLANT_IS_PARAMETER(dparam)) {
      dptmpl = weed_get_plantptr_value(dparam, WEED_LEAF_TEMPLATE, NULL);
      dtype = weed_leaf_seed_type(dparam, WEED_LEAF_VALUE);
      ndvals = weed_leaf_num_elements(dparam, WEED_LEAF_VALUE);
    } else {
      dptmpl = dparam;
      dtype = weed_leaf_seed_type(dparam, WEED_LEAF_DEFAULT);
      ndvals = weed_leaf_num_elements(dparam, WEED_LEAF_DEFAULT);
    }
  }

  if (sparam == active_dummy) {
    stype = WEED_SEED_BOOLEAN;
    nsvals = 1;
  } else {
    if (WEED_PLANT_IS_PARAMETER(sparam)) {
      stype = weed_leaf_seed_type(sparam, WEED_LEAF_VALUE);
      nsvals = weed_leaf_num_elements(sparam, WEED_LEAF_VALUE);
    } else {
      stype = weed_leaf_seed_type(sparam, WEED_LEAF_DEFAULT);
      nsvals = weed_leaf_num_elements(sparam, WEED_LEAF_DEFAULT);
    }
  }

  if (dptmpl) {
    ptype = weed_paramtmpl_get_type(dptmpl);
    dflags = weed_paramtmpl_get_flags(dptmpl);

    if (ptype == WEED_PARAM_COLOR) {
      int cspace = weed_get_int_value(dptmpl, WEED_LEAF_COLORSPACE, NULL);
      if (cspace == WEED_COLORSPACE_RGB) {
        if (!(nsvals & 3)) return FALSE;
      } else if (nsvals & 3) return FALSE;
    }
  }

  if (ndvals > nsvals) {
    if (!((dflags & WEED_PARAMETER_VARIABLE_SIZE) && !(dflags & WEED_PARAMETER_VALUE_PER_CHANNEL))) return FALSE;
  }

  if (dtype == stype) return TRUE;

  switch (stype) {
  case WEED_SEED_DOUBLE:
    if (dtype == WEED_SEED_STRING) return TRUE;
    return FALSE;
  case WEED_SEED_INT:
    if (dtype == WEED_SEED_DOUBLE || dtype == WEED_SEED_STRING) return TRUE;
    return FALSE;
  case WEED_SEED_BOOLEAN:
    if (dtype == WEED_SEED_DOUBLE || dtype == WEED_SEED_INT || dtype == WEED_SEED_STRING) return TRUE;
    return FALSE;
  default:
    return FALSE;
  }

  return FALSE;
}


static boolean pconx_convert_value_data(weed_plant_t *inst, int pnum, int key, weed_plant_t *dparam, int okey,
                                        weed_plant_t *sparam, boolean autoscale, boolean is_audio_thread, boolean * toggle_fx) {
  // try to convert values of various type, if we succeed, copy the "value" and return TRUE (if changed)
  weed_plant_t *dptmpl = NULL, *sptmpl;

  double ratio;

  int dtype = 0, stype, nsvals, ndvals = 0;
  int ondvals = 0;
  int nsmin = 0, nsmax = 0;
  int minct = 0, maxct = 0;
  int sminct = 0, smaxct = 0;
  int nmax = 0, nmin = 0;
  boolean retval = FALSE;

  int i;

  if (toggle_fx) *toggle_fx = FALSE;

  if (dparam == sparam && (dparam != active_dummy || !active_dummy)) return FALSE;

  if (sparam == active_dummy) {
    nsvals = 1;
    sptmpl = NULL;
    stype = WEED_SEED_BOOLEAN;
  }

  nsvals = weed_leaf_num_elements(sparam, WEED_LEAF_VALUE);
  if (nsvals == 0) return FALSE;
  sptmpl = weed_param_get_template(sparam);
  stype = weed_leaf_seed_type(sparam, WEED_LEAF_VALUE);

  if (dparam != active_dummy) {
    ondvals = ndvals = weed_leaf_num_elements(dparam, WEED_LEAF_VALUE);
    dptmpl = weed_param_get_template(dparam);
    dtype = weed_leaf_seed_type(dparam, WEED_LEAF_VALUE);
  } else {
    dtype = WEED_SEED_BOOLEAN;
    ondvals = ndvals = 1;
    dptmpl = NULL;
  }

  if (!params_compatible(sparam, dparam)) return FALSE;

  if (ndvals > nsvals) ndvals = nsvals;

  if (dparam != active_dummy && sparam != active_dummy) {// && autoscale) {
    if (weed_plant_has_leaf(sptmpl, WEED_LEAF_MIN) && weed_plant_has_leaf(sptmpl, WEED_LEAF_MAX)) {
      nsmin = weed_leaf_num_elements(sptmpl, WEED_LEAF_MIN);
      nsmax = weed_leaf_num_elements(sptmpl, WEED_LEAF_MAX);
    }
  }

  if (dptmpl && weed_plant_has_leaf(dptmpl, WEED_LEAF_MAX)) {
    nmax = weed_leaf_num_elements(dptmpl, WEED_LEAF_MAX);
    nmin = weed_leaf_num_elements(dptmpl, WEED_LEAF_MIN);
  }

  switch (stype) {
  case WEED_SEED_STRING:
    switch (dtype) {
    case WEED_SEED_STRING: {
      char **valsS, **valss;
      if ((valsS = weed_get_string_array(sparam, WEED_LEAF_VALUE, NULL)) == NULL) return retval;
      if ((valss = weed_get_string_array(dparam, WEED_LEAF_VALUE, NULL)) == NULL) return retval;

      if (ndvals > ondvals) valss = (char **)lives_realloc(valss, ndvals * sizeof(char *));

      for (i = 0; i < ndvals; i++) {
        if (i >= ondvals || strcmp(valss[i], valsS[i])) {
          retval = TRUE;
          if (i < ondvals) lives_free(valss[i]);
          valss[i] = valsS[i];
        } else lives_free(valsS[i]);
      }
      if (!retval) {
        for (i = 0; i < ndvals; i++) lives_free(valss[i]);
        lives_free(valss);
        lives_free(valsS);
        return FALSE;
      }

      weed_set_string_array(dparam, WEED_LEAF_VALUE, ndvals, valss);

      for (i = 0; i < ndvals; i++) lives_free(valss[i]);
      lives_free(valss);
      lives_free(valsS);
    }
    return TRUE;
    default:
      return retval;
    }
  case WEED_SEED_DOUBLE:
    switch (dtype) {
    case WEED_SEED_DOUBLE: {
      double *valsD = weed_get_double_array(sparam, WEED_LEAF_VALUE, NULL);
      double *valsd = weed_get_double_array(dparam, WEED_LEAF_VALUE, NULL);

      double *maxd = weed_get_double_array(dptmpl, WEED_LEAF_MAX, NULL);
      double *mind = weed_get_double_array(dptmpl, WEED_LEAF_MIN, NULL);

      double *mins = NULL, *maxs = NULL;

      if (autoscale) {
        mins = weed_get_double_array(sptmpl, WEED_LEAF_MIN, NULL);
        maxs = weed_get_double_array(sptmpl, WEED_LEAF_MAX, NULL);
      }

      if (ndvals > ondvals) valsd = (double *)lives_realloc(valsd, ndvals * sizeof(double));

      for (i = 0; i < ndvals; i++) {
        if (autoscale) {
          ratio = (valsD[i] - mins[sminct]) / (maxs[smaxct] - mins[sminct]);
          valsD[i] = mind[minct] + (maxd[maxct] - mind[minct]) * ratio;
          if (++smaxct == nsmax) smaxct = 0;
          if (++sminct == nsmin) sminct = 0;

          if (valsD[i] > maxd[maxct]) valsD[i] = maxd[maxct];
          if (valsD[i] < mind[minct]) valsD[i] = mind[minct];
        }

        if (i >= ondvals || valsd[i] != valsD[i]) {
          retval = TRUE;
          valsd[i] = valsD[i];
        }
        if (++maxct == nmax) maxct = 0;
        if (++minct == nmin) minct = 0;
      }

      if (mins) {
        lives_free(mins);
        lives_free(maxs);
      }

      if (retval) {
        if (inst && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
        }

        weed_set_double_array(dparam, WEED_LEAF_VALUE, ndvals, valsd);
      }
      lives_free(maxd); lives_free(mind); lives_free(valsD); lives_free(valsd);
    }
    return retval;

    case WEED_SEED_STRING: {
      char *opstring, *tmp, *bit;
      double *valsd = weed_get_double_array(sparam, WEED_LEAF_VALUE, NULL);
      char **valss, *vals;

      if (ndvals == 1) {
        opstring = lives_strdup("");
        vals = weed_get_string_value(dparam, WEED_LEAF_VALUE, NULL);
        for (i = 0; i < nsvals; i++) {
          bit = lives_strdup_printf("%.4f", valsd[i]);
          if (!*opstring)
            tmp = lives_strconcat(opstring, bit, NULL);
          else
            tmp = lives_strconcat(opstring, " ", bit, NULL);
          lives_free(bit);
          lives_free(opstring);
          opstring = tmp;
        }
        if (strcmp(vals, opstring)) {
          weed_set_string_value(dparam, WEED_LEAF_VALUE, opstring);
          retval = TRUE;
        }
        lives_free(vals); lives_free(valsd); lives_free(opstring);
        return retval;
      }

      valss = weed_get_string_array(dparam, WEED_LEAF_VALUE, NULL);

      if (ndvals > ondvals) valss = (char **)lives_realloc(valsd, ndvals * sizeof(char *));

      for (i = 0; i < ndvals; i++) {
        bit = lives_strdup_printf("%.4f", valsd[i]);
        if (i >= ondvals || strcmp(valss[i], bit)) {
          retval = TRUE;
          if (i < ondvals) lives_free(valss[i]);
          valss[i] = bit;
        } else lives_free(bit);
      }
      if (!retval) {
        for (i = 0; i < ndvals; i++) lives_free(valss[i]);
        lives_free(valss);
        lives_free(valsd);
        return FALSE;
      }

      weed_set_string_array(dparam, WEED_LEAF_VALUE, ndvals, valss);

      for (i = 0; i < ndvals; i++) lives_free(valss[i]);
      lives_free(valss);
      lives_free(valsd);
    }
    return TRUE;
    default: break;
    }

    break;

  case WEED_SEED_INT:
    switch (dtype) {
    case WEED_SEED_STRING: {
      char *opstring, *tmp, *bit;
      int *valsi = weed_get_int_array(sparam, WEED_LEAF_VALUE, NULL);

      char **valss, *vals;

      if (ndvals == 1) {
        opstring = lives_strdup("");
        vals = weed_get_string_value(dparam, WEED_LEAF_VALUE, NULL);
        for (i = 0; i < nsvals; i++) {
          bit = lives_strdup_printf("%d", valsi[i]);
          if (!*opstring)
            tmp = lives_strconcat(opstring, bit, NULL);
          else
            tmp = lives_strconcat(opstring, " ", bit, NULL);
          lives_free(bit);
          lives_free(opstring);
          opstring = tmp;
        }
        if (strcmp(vals, opstring)) {
          weed_set_string_value(dparam, WEED_LEAF_VALUE, opstring);
          retval = TRUE;
        }
        lives_free(vals); lives_free(valsi); lives_free(opstring);
        return retval;
      }

      valss = weed_get_string_array(dparam, WEED_LEAF_VALUE, NULL);

      if (ndvals > ondvals) valss = (char **)lives_realloc(valss, ndvals * sizeof(char *));

      for (i = 0; i < ndvals; i++) {
        bit = lives_strdup_printf("%d", valsi[i]);
        if (i >= ondvals || strcmp(valss[i], bit)) {
          retval = TRUE;
          if (i < ondvals) lives_free(valss[i]);
          valss[i] = bit;
        } else lives_free(bit);
      }
      if (!retval) {
        for (i = 0; i < ndvals; i++) lives_free(valss[i]);
        lives_free(valss); lives_free(valsi);
        return FALSE;
      }

      weed_set_string_array(dparam, WEED_LEAF_VALUE, ndvals, valss);

      for (i = 0; i < ndvals; i++) lives_free(valss[i]);
      lives_free(valss);
      lives_free(valsi);
    }
    return retval;
    case WEED_SEED_DOUBLE: {
      int *valsi = weed_get_int_array(sparam, WEED_LEAF_VALUE, NULL);
      double *valsd = weed_get_double_array(dparam, WEED_LEAF_VALUE, NULL);

      double *maxd = weed_get_double_array(dptmpl, WEED_LEAF_MAX, NULL);
      double *mind = weed_get_double_array(dptmpl, WEED_LEAF_MIN, NULL);
      double vald;

      int *mins = NULL, *maxs = NULL;

      if (autoscale) {
        mins = weed_get_int_array(sptmpl, WEED_LEAF_MIN, NULL);
        maxs = weed_get_int_array(sptmpl, WEED_LEAF_MAX, NULL);
      }

      if (ndvals > ondvals) valsd = (double *)lives_realloc(valsd, ndvals * sizeof(double));

      for (i = 0; i < ndvals; i++) {
        if (autoscale) {
          ratio = (double)(valsi[i] - mins[sminct]) / (double)(maxs[smaxct] - mins[sminct]);
          vald = mind[minct] + (maxd[maxct] - mind[minct]) * ratio;
          if (++smaxct == nsmax) smaxct = 0;
          if (++sminct == nsmin) sminct = 0;

          if (vald > maxd[maxct]) vald = maxd[maxct];
          if (vald < mind[minct]) vald = mind[minct];
        } else vald = (double)valsi[i];

        if (i >= ondvals || valsd[i] != vald) {
          retval = TRUE;
          valsd[i] = vald;
        }
        if (++maxct == nmax) maxct = 0;
        if (++minct == nmin) minct = 0;
      }

      if (mins) {
        lives_free(mins);
        lives_free(maxs);
      }

      if (retval) {
        if (inst && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
        }

        weed_set_double_array(dparam, WEED_LEAF_VALUE, ndvals, valsd);
      }
      lives_free(maxd); lives_free(mind);
      lives_free(valsi); lives_free(valsd);
    }
    return retval;

    case WEED_SEED_INT: {
      int *valsI, *valsi, *maxi, *mini;
      int *mins = NULL, *maxs = NULL;

      if ((valsI = weed_get_int_array(sparam, WEED_LEAF_VALUE, NULL)) == NULL) return retval;
      if ((valsi = weed_get_int_array(dparam, WEED_LEAF_VALUE, NULL)) == NULL) return retval;
      if ((maxi = weed_get_int_array(dptmpl, WEED_LEAF_MAX, NULL)) == NULL) return retval;
      if ((mini = weed_get_int_array(dptmpl, WEED_LEAF_MIN, NULL)) == NULL) return retval;

      if (autoscale) {
        mins = weed_get_int_array(sptmpl, WEED_LEAF_MIN, NULL);
        maxs = weed_get_int_array(sptmpl, WEED_LEAF_MAX, NULL);
      }

      if (ndvals > ondvals) valsi = (int *)lives_realloc(valsi, ndvals * sizeof(int));

      for (i = 0; i < ndvals; i++) {
        if (autoscale) {
          ratio = (double)(valsI[i] - mins[sminct]) / (double)(maxs[smaxct] - mins[sminct]);
          valsI[i] = myround(mini[minct] + (double)(maxi[maxct] - mini[minct]) * ratio);
          if (++smaxct == nsmax) smaxct = 0;
          if (++sminct == nsmin) sminct = 0;

          if (valsI[i] > maxi[maxct]) valsI[i] = maxi[maxct];
          if (valsI[i] < mini[minct]) valsI[i] = mini[minct];
        }

        if (i >= ondvals || valsi[i] != valsI[i]) {
          retval = TRUE;
          valsi[i] = valsI[i];
        }
        if (++maxct == nmax) maxct = 0;
        if (++minct == nmin) minct = 0;
      }

      if (mins) {
        lives_free(mins);
        lives_free(maxs);
      }

      if (retval) {
        if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
        }

        weed_set_int_array(dparam, WEED_LEAF_VALUE, ndvals, valsi);
      }
      lives_free(maxi); lives_free(mini);
      lives_free(valsI); lives_free(valsi);
    }
    return retval;
    }
    break;

  case WEED_SEED_BOOLEAN: {
    int *valsb;
    if (dparam == active_dummy) {
      // ACTIVATE
      if (!autoscale) { // autoscale is now "user override"
        int valb = weed_get_boolean_value(sparam, WEED_LEAF_VALUE, NULL);
        if ((valb == WEED_TRUE && (mainw->rte & (GU641 << (key))) == 0) ||
            (valb == WEED_FALSE && (mainw->rte & (GU641 << (key)))) != 0) {
          if (toggle_fx) *toggle_fx = TRUE;
        }
      }
      return retval;
    }
    valsb = weed_get_boolean_array(sparam, WEED_LEAF_VALUE, NULL);
    switch (dtype) {
    case WEED_SEED_STRING: {
      char *opstring, *tmp, *bit;
      char **valss, *vals;

      if (ndvals == 1) {
        opstring = lives_strdup("");
        vals = weed_get_string_value(dparam, WEED_LEAF_VALUE, NULL);
        for (i = 0; i < nsvals; i++) {
          bit = lives_strdup_printf("%d", valsb[i]);
          if (!*opstring)
            tmp = lives_strconcat(opstring, bit, NULL);
          else
            tmp = lives_strconcat(opstring, " ", bit, NULL);
          lives_free(bit);
          lives_free(opstring);
          opstring = tmp;
        }
        if (strcmp(vals, opstring)) {
          weed_set_string_value(dparam, WEED_LEAF_VALUE, opstring);
          retval = TRUE;
        }
        lives_free(vals); lives_free(valsb); lives_free(opstring);
        return retval;
      }

      valss = weed_get_string_array(dparam, WEED_LEAF_VALUE, NULL);
      if (ndvals > ondvals) valss = (char **)lives_realloc(valss, ndvals * sizeof(char *));

      for (i = 0; i < ndvals; i++) {
        bit = lives_strdup_printf("%d", valsb[i]);
        if (i >= ondvals || strcmp(valss[i], bit)) {
          retval = TRUE;
          if (i < ondvals) lives_free(valss[i]);
          valss[i] = bit;
        } else lives_free(bit);
      }
      if (!retval) {
        for (i = 0; i < ndvals; i++) lives_free(valss[i]);
        lives_free(valss);
        lives_free(valsb);
        return FALSE;
      }

      weed_set_string_array(dparam, WEED_LEAF_VALUE, ndvals, valss);

      for (i = 0; i < ndvals; i++) lives_free(valss[i]);
      lives_free(valss);
      lives_free(valsb);
    }
    return retval;
    case WEED_SEED_DOUBLE: {
      double *valsd = weed_get_double_array(dparam, WEED_LEAF_VALUE, NULL);
      double *maxd = weed_get_double_array(dptmpl, WEED_LEAF_MAX, NULL);
      double *mind = weed_get_double_array(dptmpl, WEED_LEAF_MIN, NULL);
      double vald;

      if (ndvals > ondvals) valsd = (double *)lives_realloc(valsd, ndvals * sizeof(double));

      for (i = 0; i < ndvals; i++) {
        if (autoscale) {
          if (valsb[i] == WEED_TRUE) vald = maxd[maxct];
          else vald = mind[minct];
        } else {
          vald = (double)valsb[i];
          if (vald > maxd[maxct]) vald = maxd[maxct];
          if (vald < mind[minct]) vald = mind[minct];
        }
        if (i >= ondvals || valsd[i] != vald) {
          retval = TRUE;
          valsd[i] = vald;
        }
        if (++maxct == nmax) maxct = 0;
        if (++minct == nmin) minct = 0;
      }
      if (retval) {
        if (inst && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
        }

        weed_set_double_array(dparam, WEED_LEAF_VALUE, ndvals, valsd);
      }
      lives_free(maxd); lives_free(mind);
      lives_free(valsb); lives_free(valsd);
    }
    return retval;
    case WEED_SEED_INT: {
      int *valsi = weed_get_int_array(dparam, WEED_LEAF_VALUE, NULL);
      int *maxi = weed_get_int_array(dptmpl, WEED_LEAF_MAX, NULL);
      int *mini = weed_get_int_array(dptmpl, WEED_LEAF_MIN, NULL);

      if (ndvals > ondvals) valsi = (int *)lives_realloc(valsi, ndvals * sizeof(int));

      for (i = 0; i < ndvals; i++) {
        if (autoscale) {
          if (valsb[i] == WEED_TRUE) valsb[i] = maxi[maxct];
          else valsb[i] = mini[minct];
        } else {
          if (valsb[i] > maxi[maxct]) valsb[i] = maxi[maxct];
          if (valsb[i] < mini[minct]) valsb[i] = mini[maxct];
        }
        if (i >= ondvals || valsi[i] != valsb[i]) {
          retval = TRUE;
          valsi[i] = valsb[i];
        }
        if (++maxct == nmax) maxct = 0;
        if (++minct == nmin) minct = 0;
      }
      if (retval) {
        if (inst && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
        }

        weed_set_int_array(dparam, WEED_LEAF_VALUE, ndvals, valsi);
      }
      lives_free(maxi); lives_free(mini);
      lives_free(valsi); lives_free(valsb);
    }
    return retval;

    case WEED_SEED_BOOLEAN: {
      int *valsB = weed_get_boolean_array(dparam, WEED_LEAF_VALUE, NULL);

      if (ndvals > ondvals) valsB = (int *)lives_realloc(valsB, ndvals * sizeof(int));

      for (i = 0; i < ndvals; i++) {
        if (i >= ondvals || valsB[i] != valsb[i]) {
          retval = TRUE;
          valsB[i] = valsb[i];
        }
      }
      if (retval) {
        if (inst && mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
          // if we are recording, add this change to our event_list
          rec_param_change(inst, pnum);
        }
        weed_set_boolean_array(dparam, WEED_LEAF_VALUE, ndvals, valsB);
      }
      lives_free(valsb);
      lives_free(valsB);
    }
    return retval;
    default:
      lives_free(valsb);
      break;
    }
    break;
  }
  default: break;
  }

  return retval;
}


int pconx_chain_data_omc(weed_plant_t *inst, int okey, int omode) {
  // push anything to connected OMC macros
  lives_pconnect_t *pconx = mainw->pconx;

  char *valstr = NULL;

  int totcons, pidx;
  int actions = 0;

  boolean cbval, lbval;

  int i, j;

  if (!inst) return 0;

  if (mainw->event_list && !mainw->record && !mainw->record_paused) return 0;

  // check for any inkeys == FX_DATA_KEY_OMC_MACRO and matching okey, omode
  // for each match we construct a string and send it to the OMC learner

  while (pconx) {
    if (pconx->okey == okey && pconx->omode == omode) {
      totcons = 0;
      j = 0;
      for (i = 0; i < pconx->nparams; i++) {
        totcons += pconx->nconns[i];
        pidx = pconx->params[i];
        lbval = pconx->last_boolval[i];
        for (; j < totcons; j++) {
          if (pconx->ikey[j] == FX_DATA_KEY_OMC_MACRO) {
            // out param is "ACTIVATED"
            if (pidx >= 0) {
              weed_plant_t *oparam = weed_inst_out_param(inst, pidx);
              switch (weed_leaf_seed_type(oparam, WEED_LEAF_VALUE)) {
              case WEED_SEED_BOOLEAN:
                cbval = weed_get_boolean_value(oparam, WEED_LEAF_VALUE, NULL);
                if (cbval == WEED_TRUE && lbval == WEED_FALSE) {
                  // state changed, so trigger act
                  valstr = lives_strdup_printf("%d", cbval);
                }
                pconx->last_boolval[i] = cbval;
                break;
              default: break;
              }
            } else {
              valstr = lives_strdup_printf("%d", WEED_TRUE);
              pconx->last_boolval[i] = WEED_TRUE;
            }
          }
          if (valstr) {
            // construct string to pass
            char *msgstring = lives_strdup_printf("%d %d %d %s", OMC_INTERNAL, pconx->imode[j], pconx->ipnum[j], valstr);
            lives_free(valstr);
            valstr = NULL;
            omc_process_string(OMC_INTERNAL, (const char *)msgstring, FALSE, NULL);
            actions++;
            lives_free(msgstring);
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*

    pconx = pconx->next;
  }
  return actions;
}


boolean pconx_chain_data(int key, int mode, boolean is_audio_thread) {
  weed_plant_t **inparams = NULL;
  weed_plant_t *oparam, *inparam = NULL;
  weed_plant_t *inst = NULL;

  boolean changed, reinit_inst = FALSE;

  int nparams = 0, start = 0;
  int autoscale;
  int pflags;
  int okey, omode;
  boolean toggle_fx = FALSE;
  int i;

  if (mainw->is_rendering) return FALSE;

  if (key == FX_DATA_KEY_PLAYBACK_PLUGIN) {
    // playback plugin
    if (!mainw->vpp) return FALSE;
    inparams = mainw->vpp->play_params;
    nparams = mainw->vpp->num_play_params;
  } else {
    filter_mutex_lock(key);
    inst = rte_keymode_get_instance(key + 1, mode);
    if (!inst) filter_mutex_unlock(key);
    start = -EXTRA_PARAMS_IN;
  }

  if (inst) {
    if (weed_plant_has_leaf(inst, WEED_LEAF_IN_PARAMETERS))
      inparams = weed_get_plantptr_array_counted(inst, WEED_LEAF_IN_PARAMETERS, &nparams);
  } else {
    if (rte_keymode_get_filter_idx(key + 1, mode) == -1) return FALSE;
  }


  for (i = start; i < nparams; i++) {
    //g_print("NOW at %d %d\n ", key, mode);
    if ((oparam = pconx_get_out_param(FALSE, key, mode, i, &okey, &omode, NULL, &autoscale))) {
      //#define DEBUG_PCONX
#ifdef DEBUG_PCONX
      g_print("got pconx to %d %d %d\n", key, mode, i);
#endif
      if (i == FX_DATA_PARAM_ACTIVE) {
        pthread_mutex_lock(&mainw->fxd_active_mutex);
        if (!active_dummy) {
          active_dummy = weed_plant_new(WEED_PLANT_PARAMETER);
          weed_set_plantptr_value(active_dummy, WEED_LEAF_TEMPLATE, NULL);
        }
        inparam = active_dummy;
        pthread_mutex_unlock(&mainw->fxd_active_mutex);
      } else inparam = inparams[i];

      /// we need to keep these locked for as little time as possible so as not to hang up the video / audio thread
      filter_mutex_lock(okey);
      if (oparam != active_dummy) {
        weed_plant_t *oinst = rte_keymode_get_instance(okey + 1, omode);
        if (!oinst) {
          filter_mutex_unlock(okey);
          if (inst) {
            weed_instance_unref(inst);
            filter_mutex_unlock(key);
          }
          return FALSE;
        }
        weed_instance_unref(oinst);
      }

      changed = pconx_convert_value_data(inst, i, key, key == FX_DATA_KEY_PLAYBACK_PLUGIN
                                         ? (weed_plant_t *)pp_get_param(mainw->vpp->play_params, i)
                                         : inparam, okey, oparam, autoscale, is_audio_thread, &toggle_fx);

      if (toggle_fx) {
        if (is_audio_thread) {
          // in the audio thread, don't activate / dectivate video fx. It could cause an underflow if it takes too long
          // let the video thread handle it
          weed_plant_t *filter = rte_keymode_get_filter(key + 1, rte_key_getmode(key + 1));
          if (!is_pure_audio(filter, FALSE)) {
            if (inst) {
              weed_instance_unref(inst);
              inst = NULL;
              filter_mutex_unlock(key);
            }
            filter_mutex_unlock(okey);
            return FALSE;
          }
        }
        if (inst) {
          weed_instance_unref(inst);
          inst = NULL;
          filter_mutex_unlock(key);
        }
        switch_fx_state(key + 1);
        filter_mutex_unlock(okey);
      } else {
        filter_mutex_unlock(okey);
        if (changed && inst && key > -1) {
          // only store value if it changed; for int, double or colour, store old value too

          //copyto = set_copy_to(inst, i, TRUE);
          if (mainw->record && !mainw->record_paused && LIVES_IS_PLAYING && (prefs->rec_opts & REC_EFFECTS)) {
            // if we are recording, add this change to our event_list
            rec_param_change(inst, i);
            //if (copyto != -1) rec_param_change(inst, copyto);
          }

          pflags = weed_get_int_value(inparams[i], WEED_LEAF_FLAGS, NULL);
          if (pflags & WEED_PARAMETER_REINIT_ON_VALUE_CHANGE) reinit_inst = TRUE;

          if (fx_dialog[1] && !reinit_inst) {
            lives_rfx_t *rfx = fx_dialog[1]->rfx;
            if (!rfx->is_template) {
              int keyw = fx_dialog[1]->key;
              int modew = fx_dialog[1]->mode;
              if (keyw == key && modew == mode)
                // ask the main thread to update the param window
                mainw->vrfx_update = rfx;
            }
          }
          if (mainw->ce_thumbs) ce_thumbs_register_rfx_change(key, mode);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (inst) {
    weed_instance_unref(inst);
    inst = NULL;
    filter_mutex_unlock(key);
  }

  if (key != FX_DATA_KEY_PLAYBACK_PLUGIN && inparams) lives_free(inparams);
  return reinit_inst;
}


boolean pconx_chain_data_internal(weed_plant_t *inst) {
  // special version for compound fx internal connections
  weed_plant_t **in_params;

  boolean autoscale, reinit_inst = FALSE;

  int nparams = 0, pflags, i;

  nparams = num_in_params(inst, FALSE, FALSE);
  if (!nparams) return FALSE;

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  for (i = 0; i < nparams; i++) {
    if (weed_plant_has_leaf(in_params[i], WEED_LEAF_HOST_INTERNAL_CONNECTION)) {
      autoscale = FALSE;
      if (weed_plant_has_leaf(in_params[i], WEED_LEAF_HOST_INTERNAL_CONNECTION_AUTOSCALE) &&
          weed_get_boolean_value(in_params[i],
                                 WEED_LEAF_HOST_INTERNAL_CONNECTION_AUTOSCALE, NULL) == WEED_TRUE) autoscale = TRUE;
      if (pconx_convert_value_data(inst, i, -1, in_params[i], -1, weed_get_plantptr_value(in_params[i],
                                   WEED_LEAF_HOST_INTERNAL_CONNECTION, NULL), autoscale, FALSE, NULL)) {

        pflags = weed_get_int_value(in_params[i], WEED_LEAF_FLAGS, NULL);
        if (pflags & WEED_PARAMETER_REINIT_ON_VALUE_CHANGE) reinit_inst = TRUE;
      }
    }
  }

  lives_free(in_params);
  return reinit_inst;
}

// alpha channs


void cconx_delete_all(void) {
  lives_cconnect_t *cconx = mainw->cconx, *cconx_next;
  while (cconx) {
    cconx_next = cconx->next;
    if (cconx->nchans > 0) {
      lives_free(cconx->chans);
      lives_free(cconx->nconns);
      lives_free(cconx->ikey);
      lives_free(cconx->imode);
      lives_free(cconx->icnum);
    }
    lives_free(cconx);
    cconx = cconx_next;
  }
  mainw->cconx = NULL;
}


static lives_cconnect_t *cconx_new(int okey, int omode) {
  lives_cconnect_t *cconx = (lives_cconnect_t *)lives_calloc(sizeof(struct _lives_cconnect_t), 1);
  cconx->next = NULL;
  cconx->okey = okey;
  cconx->omode = omode;
  cconx->nchans = 0;
  cconx->nconns = NULL;
  return cconx;
}


static lives_cconnect_t *cconx_copy(lives_cconnect_t *scconx) {
  lives_cconnect_t *cconx = NULL, *dcconx, *last_dcconx;

  int totcons = 0;

  register int i, j = 0;

  while (scconx) {
    dcconx = cconx_new(scconx->okey, scconx->omode);
    if (!cconx) cconx = dcconx;
    else last_dcconx->next = dcconx;

    dcconx->nchans = scconx->nchans;

    dcconx->nconns = (int *)lives_malloc(dcconx->nchans * sizint);
    dcconx->chans = (int *)lives_malloc(dcconx->nchans * sizint);

    dcconx->ikey = dcconx->imode = dcconx->icnum = NULL;

    j = 0;

    for (i = 0; i < dcconx->nchans; i++) {
      dcconx->chans[i] = scconx->chans[i];
      dcconx->nconns[i] = scconx->nconns[i];
      totcons += dcconx->nconns[i];

      dcconx->ikey = (int *)lives_realloc(dcconx->ikey, totcons * sizint);
      dcconx->imode = (int *)lives_realloc(dcconx->imode, totcons * sizint);
      dcconx->icnum = (int *)lives_realloc(dcconx->icnum, totcons * sizint);

      while (j < totcons) {
        dcconx->ikey[j] = scconx->ikey[j];
        dcconx->imode[j] = scconx->imode[j];
        dcconx->icnum[j] = scconx->icnum[j];
        j++;
      }
    }

    scconx = scconx->next;
    last_dcconx = dcconx;
  }

  return cconx;
}


char *cconx_list(int okey, int omode, int ocnum) {
  char *st1 = lives_strdup(""), *st2;
  lives_cconnect_t *cconx = mainw->cconx;
  int totcons = 0;

  while (cconx) {
    if (cconx->okey == okey && cconx->omode == omode) {
      for (int i = 0; i < cconx->nchans; i++) {
        if (cconx->chans[i] == ocnum) {
          for (int j = totcons; j < totcons + cconx->nconns[i]; j++) {
            if (!*st1) st2 = lives_strdup_printf("%d %d %d", cconx->ikey[j] + 1, cconx->imode[j] + 1, cconx->icnum[j]);
            else
              st2 = lives_strdup_printf("%s %d %d %d", st1, cconx->ikey[j] + 1, cconx->imode[j] + 1, cconx->icnum[j]);
            lives_free(st1);
            st1 = st2;
          }
          return st1;
        }
        totcons += cconx->nconns[i];
      }
      return st1;
    }
    cconx = cconx->next;
  }
  return st1;
}


void cconx_delete(int okey, int omode, int ocnum, int ikey, int imode, int icnum) {
  lives_cconnect_t *cconx = mainw->cconx, *cconx_next, *cconx_prev = NULL;

  register int i, j = 0, k;

  int totcons = 0, maxcons = 0;

  while (cconx) {
    cconx_next = cconx->next;
    if (okey == FX_DATA_WILDCARD || (cconx->okey == okey && cconx->omode == omode)) {
      if (ikey == FX_DATA_WILDCARD) {
        // delete entire node
        lives_free(cconx->chans); lives_free(cconx->nconns); lives_free(cconx->ikey);
        lives_free(cconx->imode); lives_free(cconx->icnum); lives_free(cconx);
        if (mainw->cconx == cconx) mainw->cconx = cconx_next;
        else cconx_prev->next = cconx_next;
        return;
      }

      maxcons = 0;
      totcons = 0;
      j = 0;

      for (i = 0; i < cconx->nchans; i++) {
        maxcons += cconx->nconns[i];
      }

      for (i = 0; cconx && i < cconx->nchans; i++) {
        totcons += cconx->nconns[i];

        if (okey != FX_DATA_WILDCARD && cconx->chans[i] != ocnum) {
          j = totcons;
          continue;
        }

        for (; j < totcons; j++) {
          if (cconx->ikey[j] == ikey && cconx->imode[j] == imode && (icnum == FX_DATA_WILDCARD || cconx->icnum[j] == icnum)) {
            maxcons--;
            for (k = j; k < maxcons; k++) {
              cconx->ikey[k] = cconx->ikey[k + 1];
              cconx->imode[k] = cconx->imode[k + 1];
              cconx->icnum[k] = cconx->icnum[k + 1];
            }

            cconx->ikey = (int *)lives_realloc(cconx->ikey, maxcons * sizint);
            cconx->imode = (int *)lives_realloc(cconx->imode, maxcons * sizint);
            cconx->icnum = (int *)lives_realloc(cconx->icnum, maxcons * sizint);

            cconx->nconns[i]--;

            if (cconx->nconns[i] == 0) {
              cconx->nchans--;
              for (k = i; k < cconx->nchans; k++) {
                cconx->chans[k] = cconx->chans[k + 1];
                cconx->nconns[k] = cconx->nconns[k + 1];
              }

              if (cconx->nchans == 0) {
                // delete entire node
                lives_free(cconx->chans); lives_free(cconx->nconns); lives_free(cconx->ikey);
                lives_free(cconx->imode); lives_free(cconx->icnum); lives_free(cconx);
                if (mainw->cconx == cconx) {
                  mainw->cconx = cconx_next;
                  cconx = NULL;
                } else {
                  cconx = cconx_prev;
                  cconx->next = cconx_next;
                }
              } else {
                cconx->nconns = (int *)lives_realloc(cconx->nconns, cconx->nchans * sizint);
              }
	      // *INDENT-OFF*
            }}}}}
	  // *INDENT-ON*

    cconx_prev = cconx;
    cconx = cconx_next;
  }
}


void cconx_remap_mode(int key, int omode, int nmode) {
  lives_cconnect_t *cconx = mainw->cconx;

  register int i, j, totcons;

  while (cconx) {
    if (cconx->okey == key && cconx->omode == omode) {
      cconx->omode = nmode;
    }
    j = 0;
    totcons = 0;
    for (i = 0; i < cconx->nchans; i++) {
      totcons += cconx->nconns[i];
      for (; j < totcons; j++) {
        if (cconx->ikey[j] == key && cconx->imode[j] == omode) {
          cconx->imode[j] = nmode;
        }
      }
    }
    cconx = cconx->next;
  }
}


static void cconx_append(lives_cconnect_t *cconx) {
  lives_cconnect_t *occonx = mainw->cconx;
  lives_cconnect_t *last_cconx = occonx;

  while (occonx) {
    last_cconx = occonx;
    occonx = occonx->next;
  }

  if (last_cconx) last_cconx->next = cconx;
  if (!mainw->cconx) mainw->cconx = cconx;
}


static lives_cconnect_t *cconx_find(int okey, int omode) {
  lives_cconnect_t *cconx = mainw->cconx;
  while (cconx) {
    if (cconx->okey == okey && cconx->omode == omode) {
      return cconx;
    }
    cconx = cconx->next;
  }
  return NULL;
}


static int cconx_get_numcons(lives_conx_w * conxwp, int cnum) {
  // get displayed number
  int totcons = 0;

  register int j;

  if (cnum != FX_DATA_WILDCARD) return conxwp->dispc[cnum];

  for (j = 0; j < conxwp->num_alpha; j++) {
    totcons += conxwp->dispc[j];
  }

  return totcons;
}


static int cconx_get_nconns(lives_cconnect_t *cconx, int cnum) {
  // get actual number of connections

  int totcons = 0, j;

  if (!cconx) return 0;

  for (j = 0; j < cconx->nchans; j++) {
    if (cnum != FX_DATA_WILDCARD) {
      if (cconx->chans[j] == cnum) return cconx->nconns[j];
    } else totcons += cconx->nconns[j];
  }
  return totcons;
}


static  lives_cconnect_t *cconx_add_connection_private(int okey, int omode, int ocnum, int ikey, int imode, int icnum) {
  lives_cconnect_t *cconx;
  int posn = 0, totcons = 0, i, j;

  // delete any existing connection to the input channel
  cconx_delete(FX_DATA_WILDCARD, 0, 0, ikey, imode, icnum);
  cconx = cconx_find(okey, omode);

  if (!cconx) {
    // add whole new node
    cconx = cconx_new(okey, omode);
    cconx_append(cconx);
  } else {
    // see if already in chans

    for (i = 0; i < cconx->nchans; i++) {

      if (cconx->chans[i] == ocnum) {
        // add connection to existing

        for (j = 0; j < cconx->nchans; j++) {
          if (j < i) {
            // calc posn
            posn += cconx->nconns[j];
          }
          totcons += cconx->nconns[j];
        }

        // if already there, do not add again
        for (j = posn; j < posn + cconx->nconns[i]; j++) {
          if (cconx->ikey[j] == ikey && cconx->imode[j] == imode && cconx->icnum[j] == icnum) {
            return cconx;
          }

          // add in order key/mode/chan
          if (cconx->ikey[j] > ikey || (cconx->ikey[j] == ikey && cconx->imode[j] > imode) ||
              (cconx->ikey[j] == ikey && cconx->imode[j] == imode && cconx->icnum[j] > icnum)) break;
        }

        posn = j; // we will insert here
        cconx->nconns[i]++;
        totcons++;

        // make space for new
        cconx->ikey = (int *)lives_realloc(cconx->ikey, totcons * sizint);
        cconx->imode = (int *)lives_realloc(cconx->imode, totcons * sizint);
        cconx->icnum = (int *)lives_realloc(cconx->icnum, totcons * sizint);

        // move up 1
        for (j = totcons - 1; j > posn; j--) {
          cconx->ikey[j] = cconx->ikey[j - 1];
          cconx->imode[j] = cconx->imode[j - 1];
          cconx->icnum[j] = cconx->icnum[j - 1];
        }

        // insert at posn
        cconx->ikey[posn] = ikey;
        cconx->imode[posn] = imode;
        cconx->icnum[posn] = icnum;

        return cconx;
      }
    }

    // so, we have data for key/mode but this is a new channel to be mapped

    for (i = 0; i < cconx->nchans; i++) {
      totcons += cconx->nconns[i];
    }

    totcons++;

    cconx->nchans++;
    posn = cconx->nchans;

    // make space for new
    cconx->nconns = (int *)lives_realloc(cconx->nconns, posn * sizint);
    cconx->chans = (int *)lives_realloc(cconx->chans, posn * sizint);

    cconx->ikey = (int *)lives_realloc(cconx->ikey, totcons * sizint);
    cconx->imode = (int *)lives_realloc(cconx->imode, totcons * sizint);
    cconx->icnum = (int *)lives_realloc(cconx->icnum, totcons * sizint);

    cconx->chans[posn - 1] = ocnum;

    cconx->nconns[posn - 1] = 1;

    posn = totcons - 1;

    // insert at posn
    cconx->ikey[posn] = ikey;
    cconx->imode[posn] = imode;
    cconx->icnum[posn] = icnum;

#ifdef DEBUG_CCONX
    g_print("added another cconx from %d %d %d to %d %d %d\n", okey, omode, ocnum, ikey, imode, icnum);
#endif

    return cconx;
  }

  // add new

  totcons = cconx_get_nconns(cconx, FX_DATA_WILDCARD) + 1;
  cconx->nchans++;

  cconx->nconns = (int *)lives_realloc(cconx->chans, cconx->nchans * sizint);
  cconx->nconns[cconx->nchans - 1] = 1;

  cconx->chans = (int *)lives_realloc(cconx->chans, cconx->nchans * sizint);
  cconx->chans[cconx->nchans - 1] = ocnum;

  cconx->ikey = (int *)lives_realloc(cconx->ikey, totcons * sizint);
  cconx->ikey[totcons - 1] = ikey;

  cconx->imode = (int *)lives_realloc(cconx->imode, totcons * sizint);
  cconx->imode[totcons - 1] = imode;

  cconx->icnum = (int *)lives_realloc(cconx->icnum, totcons * sizint);
  cconx->icnum[totcons - 1] = icnum;

#ifdef DEBUG_CCONX
  g_print("added new cconx from %d %d %d to %d %d %d\n", okey, omode, ocnum, ikey, imode, icnum);
#endif
  return cconx;
}


void cconx_add_connection(int okey, int omode, int ocnum, int ikey, int imode, int icnum) {
  cconx_add_connection_private(okey, omode, ocnum, ikey, imode, icnum);
}


static weed_plant_t *cconx_get_out_alpha(boolean use_filt, int ikey, int imode, int icnum, int *okey, int *omode, int *ocnum) {
  // walk all cconx and find one which has ikey/imode/icnum as destination
  // then all we need do is convert the pixel_data

  lives_cconnect_t *cconx = mainw->cconx;
  weed_plant_t *inst = NULL, *filter = NULL, *orig_inst = NULL;
  int totcons, i, j;

  while (cconx) {
    if (!use_filt) {
      if (mainw->is_rendering) {
        inst = get_new_inst_for_keymode(cconx->okey, cconx->omode);
      } else {
        inst = rte_keymode_get_instance(cconx->okey + 1, cconx->omode);
      }
      if (!inst) {
        cconx = cconx->next;
        continue;
      }
      filter = weed_instance_get_filter(inst, TRUE);
    } else {
      inst = NULL;
      filter = rte_keymode_get_filter(cconx->okey + 1, cconx->omode);
      if (!filter) {
        cconx = cconx->next;
        continue;
      }
    }
    if (!weed_plant_has_leaf(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES)) {
      if (inst) weed_instance_unref(inst);
      cconx = cconx->next;
      continue;
    }
    orig_inst = inst;
    totcons = 0;
    j = 0;
    for (i = 0; i < cconx->nchans; i++) {
      totcons += cconx->nconns[i];
      for (; j < totcons; j++) {
        if (cconx->ikey[j] == ikey && cconx->imode[j] == imode && cconx->icnum[j] == icnum) {
          weed_plant_t **outchans;
          weed_plant_t *channel = NULL;
          if (use_filt) {
            outchans = weed_get_plantptr_array(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, NULL);
            if (cconx->chans[i] < weed_leaf_num_elements(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES)) {
              channel = outchans[cconx->chans[i]];
            }
          } else {
            while (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE)) inst = weed_get_plantptr_value(inst,
                  WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
            outchans = weed_get_plantptr_array(inst, WEED_LEAF_OUT_CHANNELS, NULL);
            if (cconx->chans[i] < weed_leaf_num_elements(inst, WEED_LEAF_OUT_CHANNELS)) {
              channel = outchans[cconx->chans[i]];
            }
          }
          lives_free(outchans);
          if (okey) *okey = cconx->okey;
          if (omode) *omode = cconx->omode;
          if (ocnum) *ocnum = cconx->chans[i];
          if (orig_inst) weed_instance_unref(orig_inst);
          return channel;
        }
      }
    }
    cconx = cconx->next;
    weed_instance_unref(inst);
  }

  return NULL;
}


boolean cconx_convert_pixel_data(weed_plant_t *dchan, weed_plant_t *schan) {
  // convert pixel_data by possibly converting the type (palette)

  // return TRUE if we need to reinit the instance (because channel palette changed)

  // we set boolean "host_orig_pdata" if we steal the schan pdata (so do not free....)

  int iwidth, iheight, ipal, irow;
  int owidth, oheight, opal, orow, oflags;
  boolean pal_ok, needs_reinit = FALSE;

  weed_plant_t *dtmpl = weed_get_plantptr_value(dchan, WEED_LEAF_TEMPLATE, NULL);

  uint8_t *spdata, *dpdata;

  int i;

  ipal = weed_get_int_value(schan, WEED_LEAF_CURRENT_PALETTE, NULL);
  if (!weed_palette_is_alpha(ipal)) return FALSE;

  iwidth = weed_get_int_value(schan, WEED_LEAF_WIDTH, NULL);
  iheight = weed_get_int_value(schan, WEED_LEAF_HEIGHT, NULL);
  irow = weed_get_int_value(schan, WEED_LEAF_ROWSTRIDES, NULL);

  owidth = weed_get_int_value(dchan, WEED_LEAF_WIDTH, NULL);
  oheight = weed_get_int_value(dchan, WEED_LEAF_HEIGHT, NULL);
  opal = weed_get_int_value(dchan, WEED_LEAF_CURRENT_PALETTE, NULL);
  orow = weed_get_int_value(dchan, WEED_LEAF_ROWSTRIDES, NULL);

  spdata = (uint8_t *)weed_get_voidptr_value(schan, WEED_LEAF_PIXEL_DATA, NULL);

#ifdef DEBUG_CCONX
  g_print("spd is %p %d %d %d %d %d %d %d %d\n", spdata, ipal, opal, iwidth, owidth, iheight, oheight, irow, orow);
  if (spdata) g_print("spd2 is %p %d\n", spdata, spdata[0]);
#endif
  if (ipal == opal && iwidth == owidth && iheight == oheight && irow == orow) {
    /// everything matches - we can just do a steal
    weed_set_voidptr_value(dchan, WEED_LEAF_PIXEL_DATA, spdata);
    /// caller - do not free in dchan
    weed_set_boolean_value(dchan, WEED_LEAF_HOST_ORIG_PDATA, WEED_TRUE);
    return FALSE;
  }

  /// check first if we can set the in-channel palette to match
  if (ipal == opal) pal_ok = TRUE;
  else {
    /// see if dest chan supports the source chan palette
    int num_palettes = weed_leaf_num_elements(dtmpl, WEED_LEAF_PALETTE_LIST);
    int *palettes = weed_get_int_array(dtmpl, WEED_LEAF_PALETTE_LIST, NULL);
    if (best_palette_match(palettes, num_palettes, ipal) == ipal) pal_ok = TRUE; ///< yes
    else pal_ok = FALSE; ///<no
    oflags = weed_get_int_value(dtmpl, WEED_LEAF_FLAGS, NULL);
    if (ipal != opal && (oflags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)) needs_reinit = TRUE;
    lives_free(palettes);
  }

  weed_layer_pixel_data_free(dchan);

  weed_set_int_value(dchan, WEED_LEAF_WIDTH, iwidth);
  weed_set_int_value(dchan, WEED_LEAF_HEIGHT, iheight);
  weed_set_int_value(dchan, WEED_LEAF_CURRENT_PALETTE, ipal);

  if (pal_ok) {
    weed_set_voidptr_value(dchan, WEED_LEAF_PIXEL_DATA, (void *)spdata);
    weed_set_int_value(dchan, WEED_LEAF_ROWSTRIDES, irow);

    /// caller - do not free in dchan
    weed_set_boolean_value(dchan, WEED_LEAF_HOST_ORIG_PDATA, WEED_TRUE);
#ifdef DEBUG_CCONX
    if (spdata) g_print("spd3 is %p %d\n", spdata, spdata[0]);
#endif
    return FALSE;
  }

  create_empty_pixel_data(dchan, FALSE, TRUE);
  dpdata = (uint8_t *)weed_get_voidptr_value(dchan, WEED_LEAF_PIXEL_DATA, NULL);

  orow = weed_get_int_value(dchan, WEED_LEAF_ROWSTRIDES, NULL);

  if (irow == orow) {
    lives_memcpy((void *)dpdata, (void *)spdata, irow * iheight);
  } else {
    int ipwidth = iwidth * weed_palette_get_bits_per_macropixel(ipal) / 8;
    for (i = 0; i < iheight; i++) {
      lives_memcpy((void *)dpdata, (void *)spdata, ipwidth);
      spdata += irow;
      dpdata += orow;
    }
  }

  convert_layer_palette(dchan, opal, 0);

  if (needs_reinit) return TRUE;

  return FALSE;
}


boolean cconx_chain_data(int key, int mode) {
  // ret TRUE if we should reinit inst (because of palette change)

  weed_plant_t *ichan, *ochan;
  weed_plant_t *inst = NULL;

  boolean needs_reinit = FALSE;

  register int i = 0;

  if (key > -1) {
    if (mainw->is_rendering) {
      if ((inst = get_new_inst_for_keymode(key, mode)) == NULL) {
        return FALSE; ///< dest effect is not found
      }
    } else {
      if ((inst = rte_keymode_get_instance(key + 1, mode)) == NULL) {
        return FALSE; ///< dest effect is not enabled
      }
    }
  } else if (key == FX_DATA_KEY_PLAYBACK_PLUGIN) {
    if (!mainw->vpp || mainw->vpp->num_alpha_chans == 0) return FALSE;
  }

  while ((ichan = (key == FX_DATA_KEY_PLAYBACK_PLUGIN ? (weed_plant_t *)pp_get_chan(mainw->vpp->play_params, i)
                   : get_enabled_channel(inst, i, TRUE))) != NULL) {
    if ((ochan = cconx_get_out_alpha(FALSE, key, mode, i++, NULL, NULL, NULL))) {
      filter_mutex_lock(key);
      if (cconx_convert_pixel_data(ichan, ochan)) needs_reinit = TRUE;
      filter_mutex_unlock(key);
    }
  }
  if (inst) weed_instance_unref(inst);
  return needs_reinit;
}


boolean cconx_chain_data_internal(weed_plant_t *ichan) {
  // special version for compound fx internal connections
  boolean needs_reinit = FALSE;

  if (weed_plant_has_leaf(ichan, WEED_LEAF_HOST_INTERNAL_CONNECTION)) {
    weed_plant_t *ochan = weed_get_plantptr_value(ichan, WEED_LEAF_HOST_INTERNAL_CONNECTION, NULL);
    if (cconx_convert_pixel_data(ichan, ochan)) needs_reinit = TRUE;
  }
  return needs_reinit;
}


boolean feeds_to_video_filters(int okey, int omode) {
  weed_plant_t *filter;
  char **array;
  char *chlist;
  int nparams, niparams;
  int ikey, imode, i, j;

  filter = rte_keymode_get_filter(okey + 1, omode);

  nparams = num_out_params(filter);

  for (i = 0; i < nparams; i++) {
    chlist = pconx_list(okey, omode, i);
    niparams = get_token_count(chlist, ' ') / 4;
    array = lives_strsplit(chlist, " ", niparams * 4);
    for (j = 0; j < niparams; j += 4) {
      ikey = atoi(array[j]);
      imode = atoi(array[j + 1]);
      if (imode != rte_key_getmode(ikey + 1)) continue;
      filter = rte_keymode_get_filter(ikey + 1, imode);
      if (has_video_chans_in(filter, TRUE) || has_video_chans_out(filter, TRUE)) {
        lives_strfreev(array);
        lives_free(chlist);
        return TRUE;
      }
    }
    lives_strfreev(array);
    lives_free(chlist);
  }

  for (i = 0; i < nparams; i++) {
    chlist = cconx_list(okey, omode, i);
    niparams = get_token_count(chlist, ' ') / 3;
    array = lives_strsplit(chlist, " ", niparams * 3);
    for (j = 0; j < niparams; j += 3) {
      ikey = atoi(array[j]);
      imode = atoi(array[j + 1]);
      if (imode != rte_key_getmode(ikey + 1)) continue;
      lives_strfreev(array);
      lives_free(chlist);
      return TRUE;
    }
    lives_strfreev(array);
    lives_free(chlist);
  }

  return FALSE;
}


boolean feeds_to_audio_filters(int okey, int omode) {
  weed_plant_t *filter;
  char **array;
  char *chlist;
  int nparams, niparams;
  int ikey, imode;
  int i, j;

  filter = rte_keymode_get_filter(okey + 1, omode);

  nparams = num_out_params(filter);

  for (i = 0; i < nparams; i++) {
    chlist = pconx_list(okey, omode, i);
    niparams = get_token_count(chlist, ' ') / 4;
    array = lives_strsplit(chlist, " ", niparams * 4);
    for (j = 0; j < niparams; j += 4) {
      ikey = atoi(array[j]);
      if (ikey < 0) continue;
      imode = atoi(array[j + 1]);
      if (imode != rte_key_getmode(ikey + 1)) continue;
      filter = rte_keymode_get_filter(ikey + 1, imode);
      if (has_audio_chans_in(filter, TRUE) || has_audio_chans_out(filter, TRUE)) {
        lives_strfreev(array);
        lives_free(chlist);
        return TRUE;
      }
    }
    lives_strfreev(array);
    lives_free(chlist);
  }

  for (i = 0; i < nparams; i++) {
    chlist = cconx_list(okey, omode, i);
    niparams = get_token_count(chlist, ' ') / 3;
    array = lives_strsplit(chlist, " ", niparams * 3);
    for (j = 0; j < niparams; j += 3) {
      ikey = atoi(array[j]);
      if (ikey < 0) continue;
      imode = atoi(array[j + 1]);
      if (imode != rte_key_getmode(ikey + 1)) continue;
      filter = rte_keymode_get_filter(ikey + 1, imode);
      if (has_audio_chans_in(filter, TRUE) || has_audio_chans_out(filter, TRUE)) {
        lives_strfreev(array);
        lives_free(chlist);
        return TRUE;
      }
    }
    lives_strfreev(array);
    lives_free(chlist);
  }

  return FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// channel/param connection window

enum {
  KEY_COLUMN,
  NAME_COLUMN,
  KEYVAL_COLUMN,
  MODEVAL_COLUMN,
  EFD_NUM_COLUMNS
};


static void disconbutton_clicked(LiVESButton * button, livespointer user_data) {
  // disconnect all channels/params
  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  int totparams, totchans;
  int pidx, pidx_last, cidx, cidx_last;

  register int i;

  totparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = 0; i < totchans; i++) {
    lives_combo_set_active_index(LIVES_COMBO(conxwp->cfxcombo[i]), 0);

    if (i == 0) lives_widget_set_sensitive(conxwp->del_button[i], FALSE);
    else {
      cidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->ccombo[i]), "cidx"));
      cidx_last = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->ccombo[i - 1]), "cidx"));
      lives_widget_set_sensitive(conxwp->del_button[i], cidx == cidx_last);
    }
  }

  for (i = 0; i < totparams; i++) {
    lives_combo_set_active_index(LIVES_COMBO(conxwp->pfxcombo[i]), 0);

    if (i == 0) lives_widget_set_sensitive(conxwp->del_button[i + totchans], FALSE);
    else {
      pidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[i]), "pidx"));
      pidx_last = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[i - 1]), "pidx"));
      lives_widget_set_sensitive(conxwp->del_button[i + totchans], pidx == pidx_last);
    }
  }
}


static void apbutton_clicked(LiVESButton * button, livespointer user_data) {
  // autoconnect each param with a compatible one in the target
  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  LiVESWidget *combo;

  LiVESTreeIter iter;
  LiVESTreeModel *model;
  LiVESTreePath *tpath;

  weed_plant_t **iparams, **oparams;
  weed_plant_t *filter, *param, *oparam;

  int fidx, key, mode, totchans;
  int niparams, ours, addn, stparam;

  int i, k = 1;

  // get filter from last connection from first parameter

  ours = pconx_get_numcons(conxwp, -EXTRA_PARAMS_OUT) + pconx_get_numcons(conxwp, 0) - 1;

  combo = (LiVESWidget *)conxwp->pfxcombo[ours];

  if (!lives_combo_get_active_iter(LIVES_COMBO(combo), &iter)) return;
  model = lives_combo_get_model(LIVES_COMBO(combo));

  lives_tree_model_get(model, &iter, KEYVAL_COLUMN, &key, MODEVAL_COLUMN, &mode, -1);

  if (key == FX_DATA_KEY_OMC_MACRO) {
    // match with OMC macro
    //
    //
    return;
  }

  fidx = rte_keymode_get_filter_idx(key, mode);

  if (fidx == -1) return;

  tpath = lives_tree_model_get_path(model, &iter);
  lives_tree_model_get_iter(model, &iter, tpath);

  // find the receiving filter/instance
  filter = get_weed_filter(fidx);

  iparams = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);
  niparams = weed_leaf_num_elements(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES);

#ifdef COMBO_LIST_LIMIT
  if (niparams + EXTRA_PARAMS_IN > COMBO_LIST_LIMIT) {
    niparams = COMBO_LIST_LIMIT - EXTRA_PARAMS_IN;
  }
#endif

  oparams = weed_get_plantptr_array(rte_keymode_get_filter(conxwp->okey + 1, conxwp->omode),
                                    WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL);

  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  // get first param connected
  stparam = conxwp->idx[totchans + ours];

  if (conxwp->ikeys[totchans + ours] < 0) {
    // first out not connected, we will add this back in
    ours -= pconx_get_numcons(conxwp, 0);
    k = 0;
  } else stparam++;

  // set all pcombo with params
  for (i = stparam; i < niparams; i++) {

    param = iparams[i];

    if (weed_plant_has_leaf(param, WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;

    if (pconx_get_out_param(TRUE, key - 1, mode, i, NULL, NULL, NULL, NULL)) continue;

    oparam = oparams[k];

    if (!params_compatible(oparam, param)) continue;

    addn = pconx_get_numcons(conxwp, k);

    ours += addn;

    combo = conxwp->pcombo[ours];

    if (conxwp->ikeys[ours + totchans] >= 0) {
      // add another if needed
      padd_clicked(conxwp->add_button[ours + totchans], (livespointer)conxwp);
      ours++;
    }

    combo = conxwp->pfxcombo[ours];
    model = lives_combo_get_model(LIVES_COMBO(combo));
    lives_tree_model_get_iter(model, &iter, tpath);
    lives_combo_set_active_iter(LIVES_COMBO(combo), &iter);
    lives_widget_context_update();

    lives_combo_set_active_index(LIVES_COMBO(conxwp->pcombo[ours]), i + EXTRA_PARAMS_IN);

    if (++k >= conxwp->num_params - EXTRA_PARAMS_OUT) break;
  }

  lives_tree_path_free(tpath);

  lives_free(iparams);
  lives_free(oparams);
}


static void acbutton_clicked(LiVESButton * button, livespointer user_data) {
  // autoconnect each channel with a compatible one in the target
  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  LiVESWidget *combo;

  LiVESTreeIter iter;
  LiVESTreeModel *model;
  LiVESTreePath *tpath;

  weed_plant_t **ichans, **ochans;
  weed_plant_t *filter, *chan, *ochan;

  int fidx, key, mode;
  int nichans, nochans, ours, addn, stchan;

  int i, j = 0, k = 1;

  // get filter from last connection from first parameter

  ours = cconx_get_numcons(conxwp, 0) - 1;

  combo = (LiVESWidget *)conxwp->cfxcombo[ours];

  if (!lives_combo_get_active_iter(LIVES_COMBO(combo), &iter)) return;
  model = lives_combo_get_model(LIVES_COMBO(combo));

  lives_tree_model_get(model, &iter, KEYVAL_COLUMN, &key, MODEVAL_COLUMN, &mode, -1);
  fidx = rte_keymode_get_filter_idx(key, mode);

  if (fidx == -1) return;

  tpath = lives_tree_model_get_path(model, &iter);
  lives_tree_model_get_iter(model, &iter, tpath);

  // find the receiving filter/instance
  filter = get_weed_filter(fidx);

  ichans = weed_get_plantptr_array(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, NULL);
  nichans = weed_leaf_num_elements(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES);

  ochans = weed_get_plantptr_array(rte_keymode_get_filter(conxwp->okey + 1, conxwp->omode),
                                   WEED_LEAF_OUT_CHANNEL_TEMPLATES, NULL);
  nochans = weed_leaf_num_elements(rte_keymode_get_filter(conxwp->okey + 1, conxwp->omode),
                                   WEED_LEAF_OUT_CHANNEL_TEMPLATES);

  // get first param connected
  stchan = conxwp->idx[ours];

  if (conxwp->ikeys[ours] < 0) {
    // first out not connected, we will add this back in
    ours -= cconx_get_numcons(conxwp, 0);
    k = 0;
  } else {
    stchan++;

    for (i = 0; i < nichans; i++) {
      j++;
      chan = ichans[i];
      if (!has_alpha_palette(chan, filter)) continue;
      if (i == conxwp->idx[ours]) break;
    }
  }

  // set all ccombo with chans
  for (i = stchan; i < nichans; i++) {

    chan = ichans[i];

    if (!has_alpha_palette(chan, filter)) continue;

    if (cconx_get_out_alpha(TRUE, key - 1, mode, i, NULL, NULL, NULL)) continue;

    ochan = ochans[k];

    if (!has_alpha_palette(ochan, filter)) {
      if (++k >= nochans) break;
      continue;
    }

    addn = cconx_get_numcons(conxwp, k);

    ours += addn;

    combo = conxwp->ccombo[ours];

    if (conxwp->ikeys[ours] >= 0) {
      // add another if needed
      cadd_clicked(conxwp->add_button[ours], (livespointer)conxwp);
      ours++;
    }

    combo = conxwp->cfxcombo[ours];
    model = lives_combo_get_model(LIVES_COMBO(combo));
    lives_tree_model_get_iter(model, &iter, tpath);
    lives_combo_set_active_iter(LIVES_COMBO(combo), &iter);
    lives_widget_context_update();

    lives_combo_set_active_index(LIVES_COMBO(conxwp->ccombo[ours]), j++);

    if (++k >= nochans) break;
  }

  lives_tree_path_free(tpath);

  lives_free(ichans);
  lives_free(ochans);
}


static void padd_clicked(LiVESWidget * button, livespointer user_data) {
  // add another param row below the add button
  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  int totparams, totchans;
  int ours = -1, pidx;
#if LIVES_TABLE_IS_GRID
  int trows;
#else
  LiVESWidget *hbox[5], *hboxb[5], *achbox, *comhbox;
#endif

  int i;

  totparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = 0; i < totparams; i++) {
    if (conxwp->add_button[i + totchans] == button) {
      ours = i;
      break;
    }
  }

  pidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[ours]), "pidx"));

  totparams++;

  conxwp->pclabel = (LiVESWidget **)lives_realloc(conxwp->pclabel, (totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->add_button = (LiVESWidget **)lives_realloc(conxwp->add_button, (totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->del_button = (LiVESWidget **)lives_realloc(conxwp->del_button, (totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->clabel = (LiVESWidget **)lives_realloc(conxwp->clabel, (totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->ikeys = (int *)lives_realloc(conxwp->ikeys, (totchans + totparams) * sizint);
  conxwp->imodes = (int *)lives_realloc(conxwp->imodes, (totchans + totparams) * sizint);
  conxwp->idx = (int *)lives_realloc(conxwp->idx, (totchans + totparams) * sizint);

  conxwp->pfxcombo = (LiVESWidget **)lives_realloc(conxwp->pfxcombo, totparams * sizeof(LiVESWidget *));
  conxwp->pcombo = (LiVESWidget **)lives_realloc(conxwp->pcombo, totparams * sizeof(LiVESWidget *));

  conxwp->dpp_func = (ulong *)lives_realloc(conxwp->dpp_func, totparams * sizeof(ulong));
  conxwp->acheck_func = (ulong *)lives_realloc(conxwp->acheck_func, totparams * sizeof(ulong));

  conxwp->acheck = (LiVESWidget **)lives_realloc(conxwp->acheck, totparams * sizeof(LiVESWidget *));

  conxwp->trowsp++;

  for (i = conxwp->trowsp - 3; i > ours; i--) {
    conxwp->del_button[i + 1] = conxwp->del_button[i];
    conxwp->add_button[i + 1] = conxwp->add_button[i];
    conxwp->clabel[i + 1] = conxwp->clabel[i];
  }

#if !LIVES_TABLE_IS_GRID
  lives_table_resize(LIVES_TABLE(conxwp->tablep), conxwp->trowsp, 7);

  // add parent widgets to new row

  for (i = 0; i < 5; i++) {
    hbox[i] = lives_hbox_new(FALSE, 0);

    lives_table_attach(LIVES_TABLE(conxwp->tablep), hbox[i], i, i + 1, conxwp->trowsp - 1, conxwp->trowsp,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

  }

  ptable_row_add_standard_widgets(conxwp, totparams - 1);
#else
  lives_grid_insert_row(LIVES_GRID(conxwp->tablep), ours + 2);
  trows = conxwp->trowsp;
  conxwp->trowsp = ours + 3;
  ptable_row_add_standard_widgets(conxwp, ours + 1);
  conxwp->trowsp = trows;
#endif

  // subtract 1 from trowsp because of title row
  for (i = conxwp->trowsp - 3; i > ours; i--) {
#if !LIVES_TABLE_IS_GRID

    // reparent widgets from row i to row i+1
    hboxb[0] = lives_widget_get_parent(conxwp->pclabel[i]);
    lives_widget_reparent(conxwp->pclabel[i], hbox[0]);
    hbox[0] = hboxb[0];

    hboxb[2] = lives_widget_get_parent(conxwp->pfxcombo[i]);
    lives_widget_reparent(conxwp->pfxcombo[i], hbox[2]);
    hbox[2] = hboxb[2];

    comhbox = lives_widget_get_parent(conxwp->pcombo[i]);
    hboxb[3] = lives_widget_get_parent(comhbox);
    lives_widget_reparent(comhbox, hbox[3]);
    hbox[3] = hboxb[3];

    if (conxwp->acheck[i]) {
      achbox = lives_widget_get_parent(conxwp->acheck[i]);
      hboxb[4] = lives_widget_get_parent(achbox);
      lives_widget_reparent(achbox, hbox[4]);
      hbox[4] = hboxb[4];
    }

#endif

    conxwp->pclabel[i + 1] = conxwp->pclabel[i];

    conxwp->pfxcombo[i + 1] = conxwp->pfxcombo[i];
    conxwp->pcombo[i + 1] = conxwp->pcombo[i];

    conxwp->acheck[i + 1] = conxwp->acheck[i];
    conxwp->acheck_func[i + 1] = conxwp->acheck_func[i];

    conxwp->ikeys[i + 1] = conxwp->ikeys[i];
    conxwp->imodes[i + 1] = conxwp->imodes[i];
    conxwp->idx[i + 1] = conxwp->idx[i];

    conxwp->dpp_func[i + 1] = conxwp->dpp_func[i];
  }

  ptable_row_add_variable_widgets(conxwp, ours + 1, ours + 2, pidx);

  conxwp->ikeys[ours + 1] = conxwp->imodes[ours + 1] = conxwp->idx[i + 1] = 0;

  conxwp->dispp[pidx + EXTRA_PARAMS_OUT]++;

  lives_widget_set_sensitive(conxwp->del_button[ours + 1], TRUE);
  lives_widget_set_sensitive(conxwp->add_button[ours], FALSE);

  lives_widget_show_all(conxwp->tablep);
}


static void pdel_clicked(LiVESWidget * button, livespointer user_data) {
  //  remove the param row at the del button
  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  int totparams, totchans;
  int ours = -1, pidx;

#if !LIVES_TABLE_IS_GRID
  LiVESWidget *comhbox;
  LiVESWidget *hbox[4], *hboxb[4], *achbox;
  int pidx_next;
#endif

  register int i;

#if !LIVES_TABLE_IS_GRID
  hbox[3] = NULL;
#endif

  totparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = 0; i < totparams; i++) {
    if (conxwp->del_button[i + totchans] == button) {
      ours = i;
      break;
    }
  }

  pidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[ours]), "pidx"));

  lives_combo_set_active_index(LIVES_COMBO(conxwp->pfxcombo[ours]), 0);

  if (conxwp->dispp[pidx + EXTRA_PARAMS_OUT] <= 1) {
    // last one for this param, dont delete, just clear
    lives_widget_set_sensitive(conxwp->del_button[totchans + ours], FALSE);
    return;
  }

  if (ours > 0) {
    if (lives_widget_get_sensitive(conxwp->del_button[totchans + ours - 1])
        && lives_widget_get_sensitive(conxwp->add_button[totchans + ours])) {
      lives_widget_set_sensitive(conxwp->add_button[totchans + ours - 1], TRUE);
    }
  }

  conxwp->dispp[pidx + EXTRA_PARAMS_OUT]--;

  // force callback for pfxcombo before destroying it
  lives_widget_context_update();
  totparams--;

#if !LIVES_TABLE_IS_GRID
  hbox[0] = lives_widget_get_parent(conxwp->pclabel[totchans + ours]);
  hbox[1] = lives_widget_get_parent(conxwp->pfxcombo[ours]);

  comhbox = lives_widget_get_parent(conxwp->pcombo[ours]);
  hbox[2] = lives_widget_get_parent(comhbox);

  lives_widget_destroy(conxwp->pfxcombo[ours]);
  lives_widget_destroy(conxwp->pcombo[ours]);
  lives_widget_destroy(comhbox);
#endif

  conxwp->trowsp--;

  // subtract 1 from trowsp because of title row
  for (i = ours; i < conxwp->trowsp - 1; i++) {
#if !LIVES_TABLE_IS_GRID

    // reparent widgets from row i to row i+1

    hboxb[0] = lives_widget_get_parent(conxwp->pclabel[totchans + i + 1]);

    if (i == ours) {
      pidx_next = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[i + 1]), "pidx"));
      if (pidx_next != pidx) {
        // secondary param
        lives_widget_destroy(conxwp->pclabel[totchans + i]);
        lives_widget_reparent(conxwp->pclabel[totchans + i + 1], hbox[0]);
        conxwp->pclabel[totchans + i] = conxwp->pclabel[totchans + i + 1];
      } else {
        // primary param
        lives_widget_destroy(conxwp->pclabel[totchans + i + 1]);
      }
    } else {
      lives_widget_reparent(conxwp->pclabel[totchans + i + 1], hbox[0]);
      conxwp->pclabel[totchans + i] = conxwp->pclabel[totchans + i + 1];
    }

    hbox[0] = hboxb[0];

    hboxb[1] = lives_widget_get_parent(conxwp->pfxcombo[i + 1]);
    lives_widget_reparent(conxwp->pfxcombo[i + 1], hbox[1]);
    hbox[1] = hboxb[1];

    comhbox = lives_widget_get_parent(conxwp->pcombo[i + 1]);
    hboxb[2] = lives_widget_get_parent(comhbox);
    lives_widget_reparent(comhbox, hbox[2]);
    hbox[2] = hboxb[2];

    if (conxwp->acheck[i]) {
      if (!hbox[3]) {
        achbox = lives_widget_get_parent(conxwp->acheck[i]);
        hbox[3] = lives_widget_get_parent(achbox);
        lives_widget_destroy(achbox);
      }
      achbox = lives_widget_get_parent(conxwp->acheck[i + 1]);
      hboxb[3] = lives_widget_get_parent(achbox);
      lives_widget_reparent(achbox, hbox[3]);
      hbox[3] = hboxb[3];
    }

    lives_widget_set_sensitive(conxwp->del_button[totchans + i],
                               lives_widget_get_sensitive(conxwp->del_button[totchans + i + 1]));

#else
    conxwp->add_button[i] = conxwp->add_button[i + 1];
    conxwp->del_button[i] = conxwp->del_button[i + 1];
    conxwp->clabel[i] = conxwp->clabel[i + 1];
    conxwp->pclabel[i] = conxwp->pclabel[i + 1];

    if (i == ours) {
      widget_opts.mnemonic_label = FALSE;
      lives_label_set_text(LIVES_LABEL(conxwp->pclabel[i + 1]), lives_label_get_text(LIVES_LABEL(conxwp->pclabel[i])));
      widget_opts.mnemonic_label = TRUE;
    }
#endif
    conxwp->pfxcombo[i] = conxwp->pfxcombo[i + 1];
    conxwp->pcombo[i] = conxwp->pcombo[i + 1];

    conxwp->acheck[i] = conxwp->acheck[i + 1];
    conxwp->acheck_func[i] = conxwp->acheck_func[i + 1];

    conxwp->ikeys[i] = conxwp->ikeys[i + 1];
    conxwp->imodes[i] = conxwp->imodes[i + 1];
    conxwp->idx[i] = conxwp->idx[i + 1];

    conxwp->dpp_func[i] = conxwp->dpp_func[i + 1];
  }

#if !LIVES_TABLE_IS_GRID
  lives_widget_destroy(conxwp->clabel[conxwp->trowsp - 1 + totchans]);
  lives_widget_destroy(conxwp->add_button[conxwp->trowsp - 1 + totchans]);
  lives_widget_destroy(conxwp->del_button[conxwp->trowsp - 1 + totchans]);

  // destroy (empty) last row parent widgets
  lives_widget_destroy(hbox[0]);
  lives_widget_destroy(hbox[1]);
  lives_widget_destroy(hbox[2]);
  lives_widget_destroy(hbox[3]);

  lives_table_resize(LIVES_TABLE(conxwp->tablep), conxwp->trowsp, 7);
#else
  lives_grid_remove_row(LIVES_GRID(conxwp->tablep), ours + 1);
#endif

  conxwp->pclabel = (LiVESWidget **)lives_realloc(conxwp->pclabel, (totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->add_button = (LiVESWidget **)lives_realloc(conxwp->add_button, (totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->del_button = (LiVESWidget **)lives_realloc(conxwp->del_button, (totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->clabel = (LiVESWidget **)lives_realloc(conxwp->clabel, (totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->ikeys = (int *)lives_realloc(conxwp->ikeys, (totchans + totparams) * sizint);
  conxwp->imodes = (int *)lives_realloc(conxwp->imodes, (totchans + totparams) * sizint);
  conxwp->idx = (int *)lives_realloc(conxwp->idx, (totchans + totparams) * sizint);

  conxwp->pfxcombo = (LiVESWidget **)lives_realloc(conxwp->pfxcombo, totparams * sizeof(LiVESWidget *));
  conxwp->pcombo = (LiVESWidget **)lives_realloc(conxwp->pcombo, totparams * sizeof(LiVESWidget *));

  conxwp->dpp_func = (ulong *)lives_realloc(conxwp->dpp_func, totparams * sizeof(ulong));
  conxwp->acheck_func = (ulong *)lives_realloc(conxwp->acheck_func, totparams * sizeof(ulong));

  conxwp->acheck = (LiVESWidget **)lives_realloc(conxwp->acheck, totparams * sizeof(LiVESWidget *));
}


static void cadd_clicked(LiVESWidget * button, livespointer user_data) {
  // add another channel row below the add button
  lives_conx_w *conxwp = (lives_conx_w *)user_data;

#if LIVES_TABLE_IS_GRID
  int trows;
#else
  LiVESWidget *hbox[4], *hboxb[4], *comhbox;
#endif

  int totparams, totchans;
  int ours = -1, cidx;

  register int i;

  totparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = 0; i < totchans; i++) {
    if (conxwp->add_button[i] == button) {
      ours = i;
      break;
    }
  }

  cidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->cfxcombo[ours]), "cidx"));

  conxwp->dispc[cidx]++;

  totchans++;

  conxwp->pclabel = (LiVESWidget **)lives_realloc(conxwp->pclabel, (totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->add_button = (LiVESWidget **)lives_realloc(conxwp->add_button, (totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->del_button = (LiVESWidget **)lives_realloc(conxwp->del_button, (totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->clabel = (LiVESWidget **)lives_realloc(conxwp->clabel, (totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->ikeys = (int *)lives_realloc(conxwp->ikeys, (totchans + totparams) * sizint);
  conxwp->imodes = (int *)lives_realloc(conxwp->imodes, (totchans + totparams) * sizint);
  conxwp->idx = (int *)lives_realloc(conxwp->idx, (totchans + totparams) * sizint);

  conxwp->cfxcombo = (LiVESWidget **)lives_realloc(conxwp->cfxcombo, totchans * sizeof(LiVESWidget *));
  conxwp->ccombo = (LiVESWidget **)lives_realloc(conxwp->ccombo, totchans * sizeof(LiVESWidget *));

  conxwp->dpc_func = (ulong *)lives_realloc(conxwp->dpc_func, totchans * sizeof(ulong));

  conxwp->trowsc++;

  for (i = conxwp->trowsc - 3; i > ours; i--) {
    conxwp->del_button[i + 1] = conxwp->del_button[i];
    conxwp->add_button[i + 1] = conxwp->add_button[i];
    conxwp->clabel[i + 1] = conxwp->clabel[i];
  }

#if !LIVES_TABLE_IS_GRID
  lives_table_resize(LIVES_TABLE(conxwp->tablec), conxwp->trowsc, 6);

  // add parent widgets to new row

  for (i = 0; i < 4; i++) {
    hbox[i] = lives_hbox_new(FALSE, 0);

    lives_table_attach(LIVES_TABLE(conxwp->tablec), hbox[i], i, i + 1, conxwp->trowsc - 1, conxwp->trowsc,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);
  }

  ctable_row_add_standard_widgets(conxwp, totchans - 1);
#else
  lives_grid_insert_row(LIVES_GRID(conxwp->tablec), ours + 2);
  trows = conxwp->trowsc;
  conxwp->trowsc = ours + 3;
  ctable_row_add_standard_widgets(conxwp, ours + 1);
  conxwp->trowsc = trows;
#endif

  // subtract 1 from trowsp because of title row
  for (i = conxwp->trowsc - 3; i > ours; i--) {
#if !LIVES_TABLE_IS_GRID

    // reparent widgets from row i to row i+1
    hboxb[0] = lives_widget_get_parent(conxwp->pclabel[i]);
    lives_widget_reparent(conxwp->pclabel[i], hbox[0]);
    hbox[0] = hboxb[0];

    hboxb[2] = lives_widget_get_parent(conxwp->cfxcombo[i]);
    lives_widget_reparent(conxwp->cfxcombo[i], hbox[2]);
    hbox[2] = hboxb[2];

    comhbox = lives_widget_get_parent(conxwp->ccombo[i]);
    hboxb[3] = lives_widget_get_parent(comhbox);
    lives_widget_reparent(comhbox, hbox[3]);
    hbox[3] = hboxb[3];

#endif

    conxwp->pclabel[i + 1] = conxwp->pclabel[i];

    conxwp->cfxcombo[i + 1] = conxwp->cfxcombo[i];
    conxwp->ccombo[i + 1] = conxwp->ccombo[i];

    conxwp->ikeys[i + 1] = conxwp->ikeys[i];
    conxwp->imodes[i + 1] = conxwp->imodes[i];
    conxwp->idx[i + 1] = conxwp->idx[i];

    conxwp->dpc_func[i + 1] = conxwp->dpc_func[i];
  }

  ctable_row_add_variable_widgets(conxwp, ours + 1, ours + 2, cidx);

  conxwp->ikeys[ours + 1] = -1;
  conxwp->imodes[ours + 1] = conxwp->idx[i + 1] = 0;

  conxwp->dispc[cidx]++;

  lives_widget_set_sensitive(conxwp->del_button[ours + 1], TRUE);
  lives_widget_set_sensitive(conxwp->add_button[ours], FALSE);

  lives_widget_show_all(conxwp->tablec);
}


static void cdel_clicked(LiVESWidget * button, livespointer user_data) {
  //  remove the channel  row at the del button
  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  int totparams, totchans;
  int ours = -1, cidx;

#if !LIVES_TABLE_IS_GRID
  LiVESWidget *hbox[3], *comhbox;
  LiVESWidget *hboxb[3];
  int cidx_next;
#endif

  register int i;

  totparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = 0; i < totchans; i++) {
    if (conxwp->del_button[i] == button) {
      ours = i;
      break;
    }
  }

  cidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->ccombo[ours]), "cidx"));

  lives_combo_set_active_index(LIVES_COMBO(conxwp->cfxcombo[ours]), 0);

  if (conxwp->dispc[cidx] <= 1) {
    // last one, dont delete, just clear
    lives_widget_set_sensitive(conxwp->del_button[ours], FALSE);
    return;
  }

  // force callback for cfxcombo before destroying it
  lives_widget_context_update();

  conxwp->dispc[cidx]--;

  totchans--;

#if !LIVES_TABLE_IS_GRID
  hbox[0] = lives_widget_get_parent(conxwp->pclabel[ours]);
  hbox[1] = lives_widget_get_parent(conxwp->cfxcombo[ours]);

  comhbox = lives_widget_get_parent(conxwp->ccombo[ours]);
  hbox[2] = lives_widget_get_parent(comhbox);

  lives_widget_destroy(conxwp->cfxcombo[ours]);
  lives_widget_destroy(conxwp->ccombo[ours]);
  lives_widget_destroy(comhbox);
#endif

  conxwp->trowsc--;

  // subtract 1 from trowsc because of title row
  for (i = ours; i < conxwp->trowsc - 1; i++) {
#if !LIVES_TABLE_IS_GRID

    // reparent widgets from row i to row i+1

    hboxb[0] = lives_widget_get_parent(conxwp->pclabel[i + 1]);

    if (i == ours) {
      cidx_next = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->ccombo[i + 1]), "cidx"));
      if (cidx_next != cidx) {
        // secondary chan
        lives_widget_destroy(conxwp->pclabel[i]);
        lives_widget_reparent(conxwp->pclabel[i + 1], hbox[0]);
        conxwp->pclabel[i] = conxwp->pclabel[i + 1];
      } else {
        // primary chan
        lives_widget_destroy(conxwp->pclabel[i + 1]);
      }
    } else {
      lives_widget_reparent(conxwp->pclabel[i + 1], hbox[0]);
      conxwp->pclabel[i] = conxwp->pclabel[i + 1];
    }

    hbox[0] = hboxb[0];

    hboxb[1] = lives_widget_get_parent(conxwp->cfxcombo[i + 1]);
    lives_widget_reparent(conxwp->cfxcombo[i + 1], hbox[1]);
    hbox[1] = hboxb[1];

    comhbox = lives_widget_get_parent(conxwp->ccombo[i + 1]);
    hboxb[2] = lives_widget_get_parent(comhbox);
    lives_widget_reparent(comhbox, hbox[2]);
    hbox[2] = hboxb[2];

    lives_widget_set_sensitive(conxwp->del_button[i],
                               lives_widget_get_sensitive(conxwp->del_button[i + 1]));

#else
    conxwp->add_button[i] = conxwp->add_button[i + 1];
    conxwp->del_button[i] = conxwp->del_button[i + 1];
    conxwp->clabel[i] = conxwp->clabel[i + 1];
    conxwp->pclabel[i] = conxwp->pclabel[i + 1];

    if (i == ours) {
      widget_opts.mnemonic_label = FALSE;
      lives_label_set_text(LIVES_LABEL(conxwp->pclabel[i + 1]), lives_label_get_text(LIVES_LABEL(conxwp->pclabel[i])));
      widget_opts.mnemonic_label = TRUE;
    }
#endif
    conxwp->cfxcombo[i] = conxwp->cfxcombo[i + 1];
    conxwp->ccombo[i] = conxwp->ccombo[i + 1];

    conxwp->ikeys[i] = conxwp->ikeys[i + 1];
    conxwp->imodes[i] = conxwp->imodes[i + 1];
    conxwp->idx[i] = conxwp->idx[i + 1];

    conxwp->dpc_func[i] = conxwp->dpc_func[i + 1];
  }

#if !LIVES_TABLE_IS_GRID
  lives_widget_destroy(conxwp->clabel[conxwp->trowsc - 1]);
  lives_widget_destroy(conxwp->add_button[conxwp->trowsc - 1]);
  lives_widget_destroy(conxwp->del_button[conxwp->trowsc - 1]);

  // destroy (empty) last row parent widgets
  lives_widget_destroy(hbox[0]);
  lives_widget_destroy(hbox[1]);
  lives_widget_destroy(hbox[2]);

  lives_table_resize(LIVES_TABLE(conxwp->tablec), conxwp->trowsc, 6);
#else
  lives_grid_remove_row(LIVES_GRID(conxwp->tablec), ours + 1);
#endif

  if (ours == totchans) {
    if (lives_widget_get_sensitive(conxwp->del_button[ours - 1]))
      lives_widget_set_sensitive(conxwp->add_button[ours - 1], TRUE);
  }

  conxwp->pclabel = (LiVESWidget **)lives_realloc(conxwp->pclabel, (totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->add_button = (LiVESWidget **)lives_realloc(conxwp->add_button, (totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->del_button = (LiVESWidget **)lives_realloc(conxwp->del_button, (totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->clabel = (LiVESWidget **)lives_realloc(conxwp->clabel, (totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->ikeys = (int *)lives_realloc(conxwp->ikeys, (totchans + totparams) * sizint);
  conxwp->imodes = (int *)lives_realloc(conxwp->imodes, (totchans + totparams) * sizint);
  conxwp->idx = (int *)lives_realloc(conxwp->idx, (totchans + totparams) * sizint);

  conxwp->cfxcombo = (LiVESWidget **)lives_realloc(conxwp->cfxcombo, totchans * sizeof(LiVESWidget *));
  conxwp->ccombo = (LiVESWidget **)lives_realloc(conxwp->ccombo, totchans * sizeof(LiVESWidget *));

  conxwp->dpc_func = (ulong *)lives_realloc(conxwp->dpc_func, totchans * sizeof(ulong));
}


static void dfxc_changed(LiVESWidget * combo, livespointer user_data) {
  lives_conx_w *conxw = (lives_conx_w *)user_data;

  LiVESTreeIter iter;
  LiVESTreeModel *model;

  weed_plant_t **ichans;
  weed_plant_t *filter, *chan;

  LiVESList *clist = NULL;

  char *channame;

  int fidx, cidx, key, mode;
  int nichans, nchans, ours = -1;

  int i, j = 0;

  if (!lives_combo_get_active_iter(LIVES_COMBO(combo), &iter)) return;
  model = lives_combo_get_model(LIVES_COMBO(combo));

  lives_tree_model_get(model, &iter, KEYVAL_COLUMN, &key, MODEVAL_COLUMN, &mode, -1);
  fidx = rte_keymode_get_filter_idx(key, mode);

  nchans = cconx_get_numcons(conxw, FX_DATA_WILDCARD);

  for (i = 0; i < nchans; i++) {
    if (conxw->cfxcombo[i] == combo) {
      ours = i;
      break;
    }
  }

  cidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "cidx"));

  if (fidx == -1) {
    lives_combo_set_active_string(LIVES_COMBO(conxw->ccombo[ours]), "");
    lives_signal_handler_block(conxw->ccombo[ours], conxw->dpc_func[ours]);
    lives_combo_populate(LIVES_COMBO(conxw->ccombo[ours]), NULL);
    lives_signal_handler_unblock(conxw->ccombo[ours], conxw->dpc_func[ours]);
    if (conxw->acbutton && cconx_get_nconns(conxw->cconx, 0) == 0 && cidx == 0)
      lives_widget_set_sensitive(conxw->acbutton, FALSE);
    lives_widget_set_sensitive(conxw->ccombo[ours], FALSE);
    return;
  }

  // find the receiving filter/instance
  filter = get_weed_filter(fidx);

  ichans = weed_get_plantptr_array(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, NULL);
  nichans = weed_leaf_num_elements(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES);

  // populate all ccombo with channels
  for (i = 0; i < nichans; i++) {
    chan = ichans[j++];

    if (!has_alpha_palette(chan, filter)) continue;

    channame = get_chan_name(chan, i, TRUE);
    clist = lives_list_append(clist, channame);
  }

  lives_free(ichans);

  lives_combo_set_active_string(LIVES_COMBO(conxw->ccombo[ours]), "");
  lives_signal_handler_block(conxw->ccombo[ours], conxw->dpc_func[ours]);
  lives_combo_populate(LIVES_COMBO(conxw->ccombo[ours]), clist);
  lives_signal_handler_unblock(conxw->ccombo[ours], conxw->dpc_func[ours]);

  if (cidx == 0) if (conxw->acbutton) lives_widget_set_sensitive(conxw->acbutton, TRUE);
  lives_widget_set_sensitive(conxw->ccombo[ours], TRUE);

  lives_list_free_all(&clist);
}


static void dfxp_changed(LiVESWidget * combo, livespointer user_data) {
  // filter was changed

  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  LiVESTreeIter iter;
  LiVESTreeModel *model;

  weed_plant_t **iparams = NULL;
  weed_plant_t *filter, *param;

  LiVESList *plist = NULL;

  char *paramname;

  char *ptype, *range;
  char *array_type, *text;

  int defelems, pflags, stype;
  int fidx, key, mode, pidx;
  int niparams = 0, nparams;
  int ours = -1, i, j = 0;

  if (!lives_combo_get_active_iter(LIVES_COMBO(combo), &iter)) {
    return;
  }

  nparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = 0; i < nparams; i++) {
    if (conxwp->pfxcombo[i] == combo) {
      ours = i;
      break;
    }
  }

  model = lives_combo_get_model(LIVES_COMBO(combo));

  lives_tree_model_get(model, &iter, KEYVAL_COLUMN, &key, MODEVAL_COLUMN, &mode, -1);

  if (key == FX_DATA_KEY_OMC_MACRO) {
    // match with OMC macro
    int nmatchparams = 0;
    weed_plant_t *ofilter;
    LiVESWidget *fxcombo = conxwp->pcombo[ours];

    // get type for out_param
    pidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(fxcombo), "pidx"));
    ofilter = conxwp->filter;

    if (pidx >= 0) {
      weed_plant_t **oparams = weed_get_plantptr_array(ofilter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL);
      weed_plant_t *oparam = oparams[pidx];
      lives_free(oparams);
      stype = weed_leaf_seed_type(oparam, WEED_LEAF_DEFAULT);
    } else {
      // invent an "ACTIVATED" param
      stype = WEED_SEED_BOOLEAN;
    }

    // check omc_macros[mode]; show all params with matching type

    // TODO...

    // if nothing matched, then

    // if out is boolean, we can link to ACTIVATE, we can choose FALSE -> TRUE (TODO: or TRUE -> FALSE)
    //
    if (nmatchparams == 0 && stype == WEED_SEED_BOOLEAN) {
      text = lives_strdup_printf(_("False to True -> TRIGGER (%s)"), weed_seed_type_to_text(WEED_SEED_BOOLEAN));
      plist = lives_list_append(plist, text);
    }
  } else {
    fidx = rte_keymode_get_filter_idx(key, mode);

    pidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "pidx"));

    if (fidx == -1) {
      // connection deleted
      LiVESWidget *acheck = conxwp->acheck[ours];
      if (acheck) {
        lives_signal_handler_block(acheck, conxwp->acheck_func[ours]);
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acheck), FALSE);
        lives_widget_set_sensitive(acheck, FALSE);
        lives_signal_handler_unblock(acheck, conxwp->acheck_func[ours]);
      }

      lives_combo_set_active_string(LIVES_COMBO(conxwp->pcombo[ours]), "");
      lives_signal_handler_block(conxwp->pcombo[ours], conxwp->dpp_func[ours]);
      lives_combo_populate(LIVES_COMBO(conxwp->pcombo[ours]), NULL);
      lives_signal_handler_unblock(conxwp->pcombo[ours], conxwp->dpp_func[ours]);

      if (conxwp->apbutton && pconx_get_nconns(conxwp->pconx, 0) == 0 && pidx == 0)
        lives_widget_set_sensitive(conxwp->apbutton, FALSE);

      lives_widget_set_sensitive(conxwp->pcombo[ours], FALSE);

      return;
    }

    // find the receiving filter/instance
    filter = get_weed_filter(fidx);

    if (weed_plant_has_leaf(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES)) {
      iparams = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);
      niparams = weed_leaf_num_elements(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES);
#ifdef COMBO_LIST_LIMIT
      if (niparams + EXTRA_PARAMS_IN > COMBO_LIST_LIMIT) {
        niparams = COMBO_LIST_LIMIT - EXTRA_PARAMS_IN;
      }
#endif
    }

    // populate pcombo with all in params
    for (i = -EXTRA_PARAMS_IN; i < niparams; i++) {
      if (i == FX_DATA_PARAM_ACTIVE) {
        ptype = weed_seed_type_to_text(WEED_SEED_BOOLEAN);
        text = lives_strdup_printf(_("ACTIVATE (%s)"), ptype);
      } else {
        param = iparams[j++];

        if (weed_plant_has_leaf(param, WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;

        if (weed_plant_has_leaf(param, WEED_LEAF_GROUP) && weed_get_int_value(param, WEED_LEAF_GROUP, NULL) != 0) continue;

        paramname = get_param_name(param, j - 1, TRUE);

        ptype = weed_seed_type_to_text((stype = weed_leaf_seed_type(param, WEED_LEAF_DEFAULT)));

        pflags = weed_get_int_value(param, WEED_LEAF_FLAGS, NULL);

        if (pflags & WEED_PARAMETER_VARIABLE_SIZE) array_type = lives_strdup("[]");
        else if ((defelems = weed_leaf_num_elements(param, WEED_LEAF_DEFAULT)) > 1)
          array_type = lives_strdup_printf("[%d]", defelems);
        else array_type = lives_strdup("");

        if (weed_plant_has_leaf(param, WEED_LEAF_MAX) && weed_plant_has_leaf(param, WEED_LEAF_MIN)) {
          if (stype == WEED_SEED_INT) {
            range = lives_strdup_printf("Range: %d to %d",
                                        weed_get_int_value(param, WEED_LEAF_MIN, NULL), weed_get_int_value(param,
                                            WEED_LEAF_MAX, NULL));
          } else if (stype == WEED_SEED_DOUBLE) {
            range = lives_strdup_printf("Range: %f to %f",
                                        weed_get_double_value(param, WEED_LEAF_MIN, NULL), weed_get_double_value(param,
                                            WEED_LEAF_MAX, NULL));
          } else range = lives_strdup("");
        } else range = lives_strdup("");

        text = lives_strdup_printf("%s\n (%s%s) %s", paramname, ptype, array_type, range);
        lives_free(paramname);
        lives_free(array_type);
        lives_free(range);
      }
      lives_free(ptype);
      plist = lives_list_append(plist, text);
    }

    if (iparams) lives_free(iparams);
  }

  lives_combo_set_active_string(LIVES_COMBO(conxwp->pcombo[ours]), "");
  lives_signal_handler_block(conxwp->pcombo[ours], conxwp->dpp_func[ours]);
  lives_combo_populate(LIVES_COMBO(conxwp->pcombo[ours]), plist);
  lives_signal_handler_unblock(conxwp->pcombo[ours], conxwp->dpp_func[ours]);

  //lives_combo_set_active_string(LIVES_COMBO(conxwp->pcombo[ours]), "");

  if (!pidx) if (conxwp->apbutton) lives_widget_set_sensitive(conxwp->apbutton, TRUE);

  lives_widget_set_sensitive(conxwp->pcombo[ours], TRUE);

  lives_list_free_all(&plist);
}


int pconx_check_connection(weed_plant_t *ofilter, int opnum, int ikey, int imode, int ipnum, boolean setup,
                           weed_plant_t **iparam_ret, int *idx_ret, int *okey, int *omode, int *oopnum) {
  weed_plant_t **oparams = NULL, **iparams;
  weed_plant_t *oparam, *iparam = NULL;

  int niparams, idx, i, j = 0;

  // TODO :: key FX_DATA_KEY_OMC_MACRO == OMC Macro

  if (opnum >= 0) {
    oparams = weed_get_plantptr_array(ofilter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL);
    oparam = oparams[opnum];
    lives_free(oparams);
  } else {
    // invent an "ACTIVATED" param
    pthread_mutex_lock(&mainw->fxd_active_mutex);
    if (active_dummy && WEED_PLANT_IS_PARAMETER(active_dummy)) {
      weed_plant_free(active_dummy);
      active_dummy = NULL;
    }
    if (!active_dummy) {
      active_dummy = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
      weed_set_boolean_value(active_dummy, WEED_LEAF_DEFAULT, WEED_FALSE);
    }
    oparam = active_dummy;
    pthread_mutex_unlock(&mainw->fxd_active_mutex);
  }

  if (ipnum == FX_DATA_PARAM_ACTIVE) {
    // invent an "ACTIVATE" param
    pthread_mutex_lock(&mainw->fxd_active_mutex);
    if (active_dummy && WEED_PLANT_IS_PARAMETER(active_dummy)) {
      weed_plant_free(active_dummy);
      active_dummy = NULL;
    }
    if (!active_dummy) {
      active_dummy = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
      weed_set_boolean_value(active_dummy, WEED_LEAF_DEFAULT, WEED_FALSE);
    }
    idx = ipnum;
    iparam = active_dummy;
    pthread_mutex_unlock(&mainw->fxd_active_mutex);
  } else {
    // find the receiving filter/instance
    int fidx = rte_keymode_get_filter_idx(ikey, imode);
    weed_plant_t *filter = get_weed_filter(fidx);

    iparams = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);
    niparams = weed_leaf_num_elements(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES);

    for (i = 0; i < niparams; i++) {
      iparam = iparams[i];
      if (weed_plant_has_leaf(iparam, WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;
      if (weed_plant_has_leaf(iparam, WEED_LEAF_GROUP) && weed_get_int_value(iparam, WEED_LEAF_GROUP, NULL) != 0) continue;
      if (j == ipnum) break;
      j++;
    }
    idx = i;
    lives_free(iparams);
  }

  if (iparam_ret) *iparam_ret = iparam;
  if (idx_ret) *idx_ret = idx;

  if (!setup) {
    if (pconx_get_out_param(TRUE, ikey, imode, ipnum, okey, omode, oopnum, NULL)) {
      // dest param already has a connection
      return -1;
    }
  }

  if (!params_compatible(oparam, iparam)) {
    return -2;
  }

  return 0;
}


static void dpp_changed(LiVESWidget * combo, livespointer user_data) {
  // receiver param was set

  // 1) check if compatible

  // 2) maybe enable autoscale

  // 3) set text to just param name
  weed_plant_t *iparam;

  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  LiVESWidget *acheck = NULL;
  LiVESWidget *fxcombo;

  LiVESTreeModel *model;

  LiVESTreeIter iter;

  char *paramname;

  boolean hasone = FALSE;
  boolean setup = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "setup"));

  int nparams, nchans;
  int okey, omode, opnum, ikey;

  int pidx, key, mode, ours = -1, ret;
  int idx = lives_combo_get_active_index(LIVES_COMBO(combo));
  int i, j;

  nparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  nchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = 0; i < nparams; i++) {
    if (conxwp->pcombo[i] == combo) {
      ours = i;
      break;
    }
  }

  pidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "pidx"));

  if (idx == -1) {
    // connection deleted
    if (setup) return;
    for (i = 0; i < nchans; i++) {
      if (lives_combo_get_active_index(LIVES_COMBO(conxwp->ccombo[i])) > -1) {
        hasone = TRUE;
        break;
      }
    }
    if (!hasone) {
      for (i = 0; i < nparams; i++) {
        if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[i]), "idx")) > -1) {
          hasone = TRUE;
          break;
        }
      }
    }

    if (!hasone) lives_widget_set_sensitive(conxwp->disconbutton, FALSE);

    ikey = conxwp->ikeys[nchans + ours];

    if (ikey >= 0) {
      //if (ikey > 0) ikey--;
      pconx_delete(conxwp->okey, conxwp->omode, pidx, ikey, conxwp->imodes[nchans + ours],
                   conxwp->idx[nchans + ours]);

      conxwp->pconx = pconx_find(conxwp->okey, conxwp->omode);
    }
    conxwp->ikeys[nchans + ours] = -1;
    conxwp->imodes[nchans + ours] = 0;
    conxwp->idx[nchans + ours] = 0;

    lives_widget_set_sensitive(conxwp->del_button[nchans + ours], FALSE);
    lives_widget_set_sensitive(conxwp->add_button[nchans + ours], FALSE);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo), "idx", LIVES_INT_TO_POINTER(idx));

    return;
  }

  fxcombo = conxwp->pfxcombo[ours];

  if (!lives_combo_get_active_iter(LIVES_COMBO(fxcombo), &iter)) return;

  model = lives_combo_get_model(LIVES_COMBO(fxcombo));

  lives_tree_model_get(model, &iter, KEYVAL_COLUMN, &key, MODEVAL_COLUMN, &mode, -1);

  idx -= EXTRA_PARAMS_IN;

  //// check if connection may be made
  ret = pconx_check_connection(conxwp->filter, pidx, key, mode, idx, setup, &iparam, &j, &okey, &omode, &opnum);

  if (ret == -2) {
    do_param_incompatible_error(conxwp);
    lives_combo_set_active_string(LIVES_COMBO(combo), "");
    return;
  }
  if (ret == -1) {
    if (!do_param_connected_query(conxwp, okey, omode, opnum, conxwp->okey == okey)) {
      lives_combo_set_active_string(LIVES_COMBO(combo), "");
      return;
    } else {
      pconx_delete(okey, omode, opnum, key, mode, idx);
    }
  }

  ///////////////////////////////////////////////////////////////////////////

  idx += EXTRA_PARAMS_IN;

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo), "idx", LIVES_INT_TO_POINTER(idx));

  acheck = conxwp->acheck[ours];

  if (acheck) {
    boolean hasrange = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(acheck), "available"));

    lives_signal_handler_block(acheck, conxwp->acheck_func[ours]);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acheck), FALSE);
    lives_signal_handler_unblock(acheck, conxwp->acheck_func[ours]);

    if (hasrange) {
      g_print("ach sens 1\n");
      lives_widget_set_sensitive(acheck, TRUE);
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(conxwp->allcheckc))) {
        lives_signal_handler_block(acheck, conxwp->acheck_func[ours]);
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acheck), TRUE);
        lives_signal_handler_unblock(acheck, conxwp->acheck_func[ours]);
      }
    }
  }

  if (iparam == active_dummy) {
    if (key == FX_DATA_KEY_OMC_MACRO) paramname = (_("TRIGGER"));
    else paramname = (_("ACTIVATE"));
  } else paramname = get_param_name(iparam, idx - EXTRA_PARAMS_IN, TRUE);

  lives_signal_handler_block(combo, conxwp->dpp_func[ours]);
  lives_combo_set_active_string(LIVES_COMBO(combo), paramname);
  lives_signal_handler_unblock(combo, conxwp->dpp_func[ours]);

  lives_free(paramname);

  lives_widget_set_sensitive(conxwp->del_button[nchans + ours], TRUE);

  if (setup) return;

  lives_widget_set_sensitive(conxwp->add_button[nchans + ours], TRUE);

  conxwp->pconx = pconx_add_connection_private(conxwp->okey, conxwp->omode, pidx, key - 1, mode, j,
                  acheck ? lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(acheck)) : FALSE);

  conxwp->ikeys[nchans + ours] = key;
  conxwp->imodes[nchans + ours] = mode;
  conxwp->idx[nchans + ours] = j;

  lives_widget_set_sensitive(conxwp->disconbutton, TRUE);
}


int cconx_check_connection(int ikey, int imode, int icnum, boolean setup, weed_plant_t **ichan_ret, int *idx_ret,
                           int *okey, int *omode, int *ocnum) {
  weed_plant_t **ichans;
  weed_plant_t *filter, *ichan = NULL;

  int fidx, idx, nichans, i, j = 0;

  fidx = rte_keymode_get_filter_idx(ikey + 1, imode);

  // find the receiving filter/instance
  filter = get_weed_filter(fidx);

  ichans = weed_get_plantptr_array(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, NULL);
  nichans = weed_leaf_num_elements(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES);

  // find actual in channel number from list of alpha channels
  for (i = 0; i < nichans; i++) {
    ichan = ichans[i];
    if (!has_alpha_palette(ichan, filter)) continue;
    if (j == icnum) break;
    j++;
  }

  lives_free(ichans);

  idx = i;

  if (ichan_ret) *ichan_ret = ichan;
  if (idx_ret) *idx_ret = idx;

  if (!setup) {
    if (cconx_get_out_alpha(TRUE, ikey, imode, i, okey, omode, ocnum)) {
      // dest chan already has a connection
      return -1;
    }
  }
  return 0;
}


static void dpc_changed(LiVESWidget * combo, livespointer user_data) {
  lives_conx_w *conxwp = (lives_conx_w *)user_data;
  weed_plant_t *ichan;
  LiVESTreeModel *model;
  LiVESTreeIter iter;
  LiVESWidget *fxcombo;

  char *channame;

  boolean hasone = FALSE;
  boolean setup = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "setup"));

  int nchans, nparams;
  int key, mode, cidx, ours = -1, ret, j;
  int okey, omode, ocnum;
  int idx = lives_combo_get_active_index(LIVES_COMBO(combo));
  int i;

  nchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  nparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = 0; i < nchans; i++) {
    if (conxwp->ccombo[i] == combo) {
      ours = i;
      break;
    }
  }

  cidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "cidx"));

  if (idx == -1) {
    for (i = 0; i < nchans; i++) {
      if (lives_combo_get_active_index(LIVES_COMBO(conxwp->ccombo[i])) > -1) {
        hasone = TRUE;
        break;
      }
    }
    if (!hasone) for (i = 0; i < nparams; i++) {
        if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[i]), "idx")) > -1) {
          hasone = TRUE;
          break;
        }
      }

    if (!hasone) lives_widget_set_sensitive(conxwp->disconbutton, FALSE);

    if (conxwp->ikeys[ours] >= 0) cconx_delete(conxwp->okey, conxwp->omode, cidx,
          conxwp->ikeys[ours], conxwp->imodes[ours], conxwp->idx[ours]);
    conxwp->cconx = cconx_find(conxwp->okey, conxwp->omode);

    conxwp->ikeys[ours] = -1;
    conxwp->imodes[ours] = 0;
    conxwp->idx[ours] = 0;

    lives_widget_set_sensitive(conxwp->del_button[ours], FALSE);

    return;
  }

  fxcombo = conxwp->cfxcombo[ours];

  if (!lives_combo_get_active_iter(LIVES_COMBO(fxcombo), &iter)) return;

  model = lives_combo_get_model(LIVES_COMBO(fxcombo));
  lives_tree_model_get(model, &iter, KEYVAL_COLUMN, &key, MODEVAL_COLUMN, &mode, -1);

  // check if connection can be made
  ret = cconx_check_connection(key, mode, idx, setup, &ichan, &j, &okey, &omode, &ocnum);

  if (ret == -1) {
    // dest chan already has a connection
    if (!do_chan_connected_query(conxwp, okey, omode, ocnum, conxwp->okey == okey)) {
      lives_combo_set_active_string(LIVES_COMBO(combo), "");
      return;
    } else {
      cconx_delete(okey, omode, ocnum, key, mode, idx);
    }
  }

  lives_signal_handler_block(combo, conxwp->dpc_func[ours]);
  channame = get_chan_name(ichan, idx, TRUE);
  lives_combo_set_active_string(LIVES_COMBO(combo), channame);
  lives_signal_handler_unblock(combo, conxwp->dpc_func[ours]);

  lives_free(channame);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo), "idx", LIVES_INT_TO_POINTER(idx));

  lives_widget_set_sensitive(conxwp->del_button[ours], TRUE);

  if (setup) return;

  conxwp->cconx = cconx_add_connection_private(conxwp->okey, conxwp->omode, cidx, key - 1, mode, j);

  conxwp->ikeys[ours] = key;
  conxwp->imodes[ours] = mode;
  conxwp->idx[ours] = j;

  lives_widget_set_sensitive(conxwp->disconbutton, TRUE);
}


static void on_allcheck_toggled(LiVESToggleButton * button, livespointer user_data) {
  lives_conx_w *conxwp = (lives_conx_w *)user_data;
  boolean on = lives_toggle_button_get_active(button);
  int nparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (int i = EXTRA_PARAMS_OUT; i < nparams; i++) {
    if (conxwp->acheck[i]) {
      if (lives_widget_is_sensitive(conxwp->acheck[i]))
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(conxwp->acheck[i]), on);
    }
  }
}


static void on_acheck_toggled(LiVESToggleButton * acheck, livespointer user_data) {
  lives_conx_w *conxwp = (lives_conx_w *)user_data;
  weed_plant_t **iparams;
  weed_plant_t *param, *filter;
  LiVESTreeModel *model;
  LiVESTreeIter iter;
  LiVESWidget *fxcombo;

  boolean on = lives_toggle_button_get_active(acheck);
  int ours = -1, fidx, key, mode, niparams, nparams, nchans, nx = 0;
  int idx, pidx, i, j = 0;

  if (EXTRA_PARAMS_OUT > 0) nx = pconx_get_numcons(conxwp, -EXTRA_PARAMS_OUT);

  nparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  nchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  for (i = nx; i < nparams; i++) {
    if (conxwp->acheck[i] == (LiVESWidget *)acheck) {
      ours = i;
      break;
    }
  }

  idx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[ours]), "idx"));

  fxcombo = conxwp->pfxcombo[ours];

  model = lives_combo_get_model(LIVES_COMBO(fxcombo));
  lives_combo_get_active_iter(LIVES_COMBO(fxcombo), &iter);

  lives_tree_model_get(model, &iter, KEYVAL_COLUMN, &key, MODEVAL_COLUMN, &mode, -1);

  if (key < 0) return;

  if (key == FX_DATA_KEY_OMC_MACRO) {
    // match with OMC macro


    return;
  } else {
    if (idx >= EXTRA_PARAMS_IN) {
      fidx = rte_keymode_get_filter_idx(key, mode);

      // find the receiving filter/instance
      filter = get_weed_filter(fidx);

      iparams = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);
      niparams = weed_leaf_num_elements(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES);

#ifdef COMBO_LIST_LIMIT
      if (niparams + EXTRA_PARAMS_IN > COMBO_LIST_LIMIT) {
        niparams = COMBO_LIST_LIMIT - EXTRA_PARAMS_IN;
      }
#endif

      for (i = 0; i < niparams; i++) {
        param = iparams[j];
        if (weed_plant_has_leaf(param, WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;
        if (j == idx) break;
        j++;
      }

      j = i;

      lives_free(iparams);
    } else j = idx - EXTRA_PARAMS_IN;
  }

  pidx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(acheck), "pidx"));
  conxwp->pconx = pconx_add_connection_private(conxwp->okey, conxwp->omode, pidx, key - 1, mode, j, on);

  conxwp->ikeys[nchans + ours] = key;
  conxwp->imodes[nchans + ours] = mode;
  conxwp->idx[nchans + ours] = j;
}


static LiVESTreeModel *inparam_fx_model(boolean is_chans, int key) {
  LiVESTreeStore *tstore;
  LiVESTreeIter iter1, iter2;
  weed_plant_t *filter;
  char *fxname, *keystr, *text;
  boolean key_added;
  int idx, nmodes = rte_getmodespk(), i, j;

  tstore = lives_tree_store_new(EFD_NUM_COLUMNS, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_INT,
                                LIVES_COL_TYPE_INT);

  lives_tree_store_append(tstore, &iter1, NULL);   /* Acquire an iterator */
  lives_tree_store_set(tstore, &iter1,
                       KEY_COLUMN, mainw->string_constants[LIVES_STRING_CONSTANT_NONE],
                       NAME_COLUMN, mainw->string_constants[LIVES_STRING_CONSTANT_NONE],
                       KEYVAL_COLUMN, 0, MODEVAL_COLUMN, 0, -1);

  if (!is_chans) {
    // add OMC Macros
    keystr = (_("OMC Macros"));
    lives_tree_store_append(tstore, &iter1, NULL);   /* Acquire an iterator */
    lives_tree_store_set(tstore, &iter1, KEY_COLUMN, keystr, NAME_COLUMN, keystr, KEYVAL_COLUMN, 0, MODEVAL_COLUMN, 0, -1);

    for (i = 0; get_omc_macro(i); i++) {
      if (i != OSC_NOTIFY) continue; // TODO
      text = get_omc_macro(i)->macro_text;
      lives_tree_store_append(tstore, &iter2, &iter1);
      lives_tree_store_set(tstore, &iter2, KEY_COLUMN, text, NAME_COLUMN, text,
                           KEYVAL_COLUMN, FX_DATA_KEY_OMC_MACRO, MODEVAL_COLUMN, i, -1);
    }

    lives_free(keystr);
  }

  // go through all keys
  for (i = 1; i <= FX_KEYS_MAX_VIRTUAL; i++) {
    if (i == key + 1) continue;

    key_added = FALSE;
    keystr = lives_strdup_printf(_("Key slot %d"), i);

    for (j = 0; j < nmodes; j++) {
      if ((idx = rte_keymode_get_filter_idx(i, j)) == -1) continue;

      filter = get_weed_filter(idx);

      if (is_chans)
        if (num_alpha_channels(filter, FALSE) == 0) continue;

      fxname = weed_get_string_value(filter, WEED_LEAF_NAME, NULL);
      text = lives_strdup_printf("(%d,%d) %s", i, j + 1, fxname);

      if (!key_added) {
        // add key
        lives_tree_store_append(tstore, &iter1, NULL);   /* Acquire an iterator */
        lives_tree_store_set(tstore, &iter1, KEY_COLUMN, keystr, NAME_COLUMN, keystr, KEYVAL_COLUMN, 0, MODEVAL_COLUMN, 0, -1);
        key_added = TRUE;
      }
      lives_tree_store_append(tstore, &iter2, &iter1);
      lives_tree_store_set(tstore, &iter2, KEY_COLUMN, text, NAME_COLUMN, text, KEYVAL_COLUMN, i, MODEVAL_COLUMN, j, -1);

      lives_free(fxname);
      lives_free(text);
    }

    lives_free(keystr);
  }

  return (LiVESTreeModel *)tstore;
}


static void ptable_row_add_variable_widgets(lives_conx_w * conxwp, int idx, int row, int pidx) {
  weed_plant_t **oparams, *param;

  LiVESWidget *hbox, *hbox2;
  LiVESWidget *fx_entry;

  boolean hasrange = FALSE;

  int stype, totchans;

  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  hbox = lives_hbox_new(FALSE, 0);

  lives_table_attach(LIVES_TABLE(conxwp->tablep), hbox, 0, 1, row, row + 1,
                     (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                     (LiVESAttachOptions)(0), 0, 0);

  conxwp->pclabel[idx + totchans] = lives_standard_label_new(NULL);
  lives_box_pack_start(LIVES_BOX(hbox), conxwp->pclabel[idx + totchans], FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_hbox_new(FALSE, 0);

  conxwp->pfxcombo[idx] = lives_standard_combo_new_with_model(pmodel, LIVES_BOX(hbox));

  lives_combo_set_entry_text_column(LIVES_COMBO(conxwp->pfxcombo[idx]), NAME_COLUMN);

  lives_table_attach(LIVES_TABLE(conxwp->tablep), hbox, 2, 3, row, row + 1,
                     (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                     (LiVESAttachOptions)(0), 0, 0);

  fx_entry = lives_combo_get_entry(LIVES_COMBO(conxwp->pfxcombo[idx]));
  lives_entry_set_width_chars(LIVES_ENTRY(fx_entry), SHORTER_ENTRY_WIDTH);
  lives_entry_set_text(LIVES_ENTRY(fx_entry), mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
  lives_entry_set_editable(LIVES_ENTRY(fx_entry), FALSE);

  hbox = lives_hbox_new(FALSE, 0);

  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, FALSE, 0);

  conxwp->pcombo[idx] = lives_standard_combo_new("", NULL, LIVES_BOX(hbox2), NULL);
  lives_entry_set_width_chars(LIVES_ENTRY(lives_combo_get_entry(LIVES_COMBO(conxwp->pcombo[idx]))), SHORTER_ENTRY_WIDTH);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[idx]), "idx", LIVES_INT_TO_POINTER(-1));
  lives_widget_set_sensitive(conxwp->pcombo[idx], FALSE);

  lives_table_attach(LIVES_TABLE(conxwp->tablep), hbox, 3, 4, row, row + 1,
                     (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(conxwp->pfxcombo[idx]), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(dfxp_changed), (livespointer)conxwp);

  conxwp->dpp_func[idx] = lives_signal_sync_connect(LIVES_GUI_OBJECT(conxwp->pcombo[idx]), LIVES_WIDGET_CHANGED_SIGNAL,
                          LIVES_GUI_CALLBACK(dpp_changed), (livespointer)conxwp);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(conxwp->pcombo[idx]), "pidx", LIVES_INT_TO_POINTER(pidx));

  if (pidx < 0) {
    conxwp->acheck[idx] = NULL;
  } else {
    if (weed_plant_has_leaf(conxwp->filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES)) {
      oparams = weed_get_plantptr_array(conxwp->filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL);

      param = oparams[pidx];
      stype = weed_leaf_seed_type(param, WEED_LEAF_DEFAULT);

      if (weed_plant_has_leaf(param, WEED_LEAF_MAX) && weed_plant_has_leaf(param, WEED_LEAF_MIN) && (stype == WEED_SEED_INT ||
          stype == WEED_SEED_DOUBLE))
        hasrange = TRUE;

      lives_free(oparams);
    }

    hbox = lives_hbox_new(FALSE, 0);

    lives_table_attach(LIVES_TABLE(conxwp->tablep), hbox, 4, 5, row, row + 1,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    hbox2 = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, FALSE, 0);

    conxwp->acheck[idx] = lives_standard_check_button_new(_("Autoscale"), FALSE, LIVES_BOX(hbox2), NULL);

    lives_widget_set_sensitive(conxwp->acheck[idx], FALSE);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(conxwp->acheck[idx]), "available", LIVES_INT_TO_POINTER(hasrange));

    conxwp->acheck_func[idx] = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(conxwp->acheck[idx]), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_acheck_toggled), (livespointer)conxwp);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(conxwp->acheck[idx]), "pidx", LIVES_INT_TO_POINTER(pidx));
  }
}


static void ctable_row_add_variable_widgets(lives_conx_w * conxwp, int idx, int row, int cidx) {
  LiVESWidget *hbox, *hbox2;
  LiVESWidget *fx_entry;

  hbox = lives_hbox_new(FALSE, 0);

  lives_table_attach(LIVES_TABLE(conxwp->tablec), hbox, 0, 1, row, row + 1,
                     (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                     (LiVESAttachOptions)(0), 0, 0);

  conxwp->pclabel[idx] = lives_standard_label_new(NULL);
  lives_box_pack_start(LIVES_BOX(hbox), conxwp->pclabel[idx], FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_hbox_new(FALSE, 0);

  conxwp->cfxcombo[idx] = lives_standard_combo_new_with_model(cmodel, LIVES_BOX(hbox));

  lives_combo_set_entry_text_column(LIVES_COMBO(conxwp->cfxcombo[idx]), NAME_COLUMN);

  lives_table_attach(LIVES_TABLE(conxwp->tablec), hbox, 2, 3, row, row + 1,
                     (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                     (LiVESAttachOptions)(0), 0, 0);

  fx_entry = lives_combo_get_entry(LIVES_COMBO(conxwp->cfxcombo[idx]));
  lives_entry_set_width_chars(LIVES_ENTRY(fx_entry), SHORTER_ENTRY_WIDTH);
  lives_entry_set_text(LIVES_ENTRY(fx_entry), mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
  lives_entry_set_editable(LIVES_ENTRY(fx_entry), FALSE);

  hbox = lives_hbox_new(FALSE, 0);

  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, FALSE, 0);

  conxwp->ccombo[idx] = lives_standard_combo_new("", NULL, LIVES_BOX(hbox2), NULL);
  lives_entry_set_width_chars(LIVES_ENTRY(lives_combo_get_entry(LIVES_COMBO(conxwp->ccombo[idx]))), SHORTER_ENTRY_WIDTH);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(conxwp->ccombo[idx]), "idx", LIVES_INT_TO_POINTER(-1));
  lives_widget_set_sensitive(conxwp->ccombo[idx], FALSE);

  lives_table_attach(LIVES_TABLE(conxwp->tablec), hbox, 3, 4, row, row + 1,
                     (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(conxwp->cfxcombo[idx]), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(dfxc_changed), (livespointer)conxwp);

  conxwp->dpc_func[idx] = lives_signal_sync_connect(LIVES_GUI_OBJECT(conxwp->ccombo[idx]), LIVES_WIDGET_CHANGED_SIGNAL,
                          LIVES_GUI_CALLBACK(dpc_changed), (livespointer)conxwp);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(conxwp->ccombo[idx]), "cidx", LIVES_INT_TO_POINTER(cidx));
}


static void ptable_row_add_standard_widgets(lives_conx_w * conxwp, int idx) {
  LiVESWidget *hbox;

  hbox = lives_hbox_new(FALSE, 0);

  lives_table_attach(LIVES_TABLE(conxwp->tablep), hbox, 1, 2, conxwp->trowsp - 1, conxwp->trowsp,
                     (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND), (LiVESAttachOptions)(0), 0, 0);

  conxwp->clabel[idx] = lives_standard_label_new(lctext);
  lives_box_pack_start(LIVES_BOX(hbox), conxwp->clabel[idx], FALSE, FALSE, widget_opts.packing_width);

  conxwp->add_button[idx] = lives_standard_button_new_from_stock(LIVES_STOCK_ADD, NULL, BW, BH);
  lives_widget_set_tooltip_text(conxwp->add_button[idx],
                                _("Add another connection for this output parameter"));
  lives_widget_set_sensitive(conxwp->add_button[idx], FALSE);

  lives_table_attach(LIVES_TABLE(conxwp->tablep), conxwp->add_button[idx], 6, 7, conxwp->trowsp - 1,
                     conxwp->trowsp, (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(conxwp->add_button[idx]), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(padd_clicked), (livespointer)conxwp);

  conxwp->del_button[idx] = lives_standard_button_new_from_stock(LIVES_STOCK_REMOVE, NULL, BW, BH);
  lives_widget_set_tooltip_text(conxwp->del_button[idx], _("Delete this connection"));

  hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(hbox), conxwp->del_button[idx], FALSE, FALSE, 0);

  lives_table_attach(LIVES_TABLE(conxwp->tablep), hbox, 5, 6, conxwp->trowsp - 1, conxwp->trowsp,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(conxwp->del_button[idx]), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(pdel_clicked), (livespointer)conxwp);

  lives_widget_set_sensitive(conxwp->del_button[idx], FALSE);
}


static void ctable_row_add_standard_widgets(lives_conx_w * conxwp, int idx) {
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);

  lives_table_attach(LIVES_TABLE(conxwp->tablec), hbox, 1, 2, conxwp->trowsc - 1, conxwp->trowsc,
                     (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND), (LiVESAttachOptions)(0), 0, 0);

  conxwp->clabel[idx] = lives_standard_label_new(lctext);
  lives_box_pack_start(LIVES_BOX(hbox), conxwp->clabel[idx], FALSE, FALSE, widget_opts.packing_width);

  conxwp->add_button[idx] = lives_standard_button_new_from_stock(LIVES_STOCK_ADD, NULL, BW, BH);
  lives_widget_set_tooltip_text(conxwp->add_button[idx], _("Add another connection for this output channel"));

  lives_table_attach(LIVES_TABLE(conxwp->tablec), conxwp->add_button[idx], 5, 6, conxwp->trowsc - 1, conxwp->trowsc,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(conxwp->add_button[idx]), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(cadd_clicked), (livespointer)conxwp);

  conxwp->del_button[idx] = lives_standard_button_new_from_stock(LIVES_STOCK_REMOVE, NULL, BW, BH);
  lives_widget_set_tooltip_text(conxwp->del_button[idx], _("Delete this connection"));

  hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(hbox), conxwp->del_button[idx], FALSE, FALSE, 0);

  lives_table_attach(LIVES_TABLE(conxwp->tablec), hbox, 4, 5, conxwp->trowsc - 1, conxwp->trowsc,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(conxwp->del_button[idx]), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(cdel_clicked), (livespointer)conxwp);

  lives_widget_set_sensitive(conxwp->del_button[idx], FALSE);
}


static LiVESWidget *conx_scroll_new(lives_conx_w * conxwp) {
  weed_plant_t *chan, *param;

  LiVESWidget *label;
  LiVESWidget *top_vbox;
  LiVESWidget *hbox;
  LiVESWidget *scrolledwindow;

  char *channame, *pname, *fname;
  char *ptype, *range;
  char *array_type, *text, *tmp;

  boolean isfirst;

  int defelems, pflags, stype;

  int totchans, totparams, nconns;

  register int i, j = 0, x = 0;

  for (i = 0; i < conxwp->num_alpha; i++) {
    nconns = cconx_get_nconns(conxwp->cconx, i);
    if (nconns == 0) nconns = 1;
    conxwp->dispc[i] = nconns;
  }

  for (i = -EXTRA_PARAMS_OUT; i < conxwp->num_params - EXTRA_PARAMS_OUT; i++) {
    nconns = pconx_get_nconns(conxwp->pconx, i);
    if (nconns == 0) nconns = 1;
    conxwp->dispp[i + EXTRA_PARAMS_OUT] = nconns;
  }

  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);
  totparams = pconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  conxwp->add_button = (LiVESWidget **)lives_malloc((totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->del_button = (LiVESWidget **)lives_malloc((totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->clabel = (LiVESWidget **)lives_malloc((totchans + totparams) * sizeof(LiVESWidget *));
  conxwp->pclabel = (LiVESWidget **)lives_malloc((totchans + totparams) * sizeof(LiVESWidget *));

  conxwp->cfxcombo = conxwp->ccombo = conxwp->pcombo = conxwp->pfxcombo = conxwp->acheck = NULL;
  conxwp->dpp_func = conxwp->dpc_func = conxwp->acheck_func = NULL;

  conxwp->ikeys = (int *)lives_malloc((totchans + totparams) * sizint);
  conxwp->imodes = (int *)lives_malloc((totchans + totparams) * sizint);
  conxwp->idx = (int *)lives_malloc((totchans + totparams) * sizint);

  for (i = 0; i < totchans + totparams; i++) {
    conxwp->ikeys[i] = -1;
    conxwp->imodes[i] = conxwp->idx[i] = 0;
  }

  lctext = (_("Connected to -->"));

  top_vbox = lives_vbox_new(FALSE, 0);

  scrolledwindow = lives_standard_scrolled_window_new(-1, -1, top_vbox);

  conxwp->trowsc = conxwp->trowsp = 0;

  fname = weed_get_string_value(conxwp->filter, WEED_LEAF_NAME, NULL);

  if (conxwp->num_alpha > 0) {
    weed_plant_t **ochans = weed_get_plantptr_array(conxwp->filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, NULL);

    conxwp->dpc_func = (ulong *)lives_malloc(totchans * sizeof(ulong));

    conxwp->cfxcombo = (LiVESWidget **)lives_malloc(totchans * sizeof(LiVESWidget *));

    conxwp->ccombo = (LiVESWidget **)lives_malloc(totchans * sizeof(LiVESWidget *));

    tmp = lives_big_and_bold("%s - Alpha Channel Connections", fname);

    widget_opts.use_markup = TRUE;
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    label = lives_standard_label_new(tmp);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    widget_opts.use_markup = FALSE;
    lives_free(tmp);

    lives_box_pack_start(LIVES_BOX(top_vbox), label, FALSE, FALSE, widget_opts.packing_height);

    conxwp->tablec = lives_table_new(0, 6, FALSE);
    lives_table_set_row_spacings(LIVES_TABLE(conxwp->tablec), widget_opts.packing_height);
    lives_table_set_col_spacings(LIVES_TABLE(conxwp->tablec), widget_opts.packing_width);
    lives_box_pack_start(LIVES_BOX(top_vbox), conxwp->tablec, FALSE, FALSE, widget_opts.packing_height);

    conxwp->trowsc = 1;

    cmodel = inparam_fx_model(TRUE, conxwp->okey);

    for (i = 0; i < conxwp->num_alpha; i++) {
      chan = ochans[j++];

      if (!has_alpha_palette(chan, conxwp->filter)) {
        i--;
        continue;
      }

      lives_table_resize(LIVES_TABLE(conxwp->tablec), ++conxwp->trowsc, 6);

      nconns = conxwp->dispc[i];

      isfirst = TRUE;

      do {
        ctable_row_add_variable_widgets(conxwp, x, conxwp->trowsc - 1, i);
        ctable_row_add_standard_widgets(conxwp, x);

        if (isfirst) {
          channame = get_chan_name(chan, j - 1, FALSE);
          widget_opts.mnemonic_label = FALSE;
          lives_label_set_text(LIVES_LABEL(conxwp->pclabel[x]), channame);
          widget_opts.mnemonic_label = TRUE;
          lives_free(channame);
          isfirst = FALSE;
        }

        x++;

        if (nconns > 1) {
          lives_table_resize(LIVES_TABLE(conxwp->tablec), ++conxwp->trowsc, 6);
        }

      } while (--nconns > 0);
    }

    lives_free(ochans);
  }

  if (conxwp->num_alpha > 0 && conxwp->num_params > 0) {
    add_hsep_to_box(LIVES_BOX(top_vbox));
  }

  if (conxwp->num_params > 0) {
    weed_plant_t **oparams = NULL;

    pmodel = inparam_fx_model(FALSE, conxwp->okey);

    if (weed_plant_has_leaf(conxwp->filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES))
      oparams = weed_get_plantptr_array(conxwp->filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES, NULL);

    conxwp->pfxcombo = (LiVESWidget **)lives_malloc(totparams * sizeof(LiVESWidget *));
    conxwp->pcombo = (LiVESWidget **)lives_malloc(totparams * sizeof(LiVESWidget *));

    conxwp->dpp_func = (ulong *)lives_malloc(totparams * sizeof(ulong));
    conxwp->acheck_func = (ulong *)lives_malloc(totparams * sizeof(ulong));

    conxwp->acheck = (LiVESWidget **)lives_malloc(totparams * sizeof(LiVESWidget *));

    tmp = lives_big_and_bold("%s - Parameter Data Connections", fname);

    widget_opts.use_markup = TRUE;
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    label = lives_standard_label_new(tmp);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    widget_opts.use_markup = FALSE;
    lives_free(tmp);

    lives_box_pack_start(LIVES_BOX(top_vbox), label, FALSE, FALSE, widget_opts.packing_height);

    conxwp->tablep = lives_table_new(1, 7, FALSE);
    lives_table_set_row_spacings(LIVES_TABLE(conxwp->tablep), widget_opts.packing_height);
    lives_table_set_col_spacings(LIVES_TABLE(conxwp->tablep), widget_opts.packing_width);
    lives_box_pack_start(LIVES_BOX(top_vbox), conxwp->tablep, FALSE, FALSE, widget_opts.packing_height);

    conxwp->trowsp = 1;
    x = 0;

    hbox = lives_hbox_new(FALSE, 0);

    lives_table_attach(LIVES_TABLE(conxwp->tablep), hbox, 4, 5, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    conxwp->allcheckc = lives_standard_check_button_new(_("Autoscale All"), TRUE, LIVES_BOX(hbox), NULL);
    conxwp->allcheck_label = widget_opts.last_label;

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(conxwp->allcheckc), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_allcheck_toggled),
                                    (livespointer)conxwp);

    if (EXTRA_PARAMS_OUT > 0) {
      lives_table_resize(LIVES_TABLE(conxwp->tablep), ++conxwp->trowsp, 7);

      nconns = conxwp->dispp[0];

      isfirst = TRUE;

      do {
        // TODO: for OMC, adjust the pmodel for each oparam type

        ptable_row_add_variable_widgets(conxwp, x, conxwp->trowsp - 1, -EXTRA_PARAMS_OUT);
        ptable_row_add_standard_widgets(conxwp, x + totchans);

        //lives_widget_set_sensitive(conxwp->add_button[x + totchans], FALSE);

        if (isfirst) {
          /* TRANSLATORS - as in "Effect ACTIVATED" */
          pname = (_("ACTIVATED"));
          ptype = weed_seed_type_to_text(WEED_SEED_BOOLEAN);

          text = lives_strdup_printf("%s (%s)", pname, ptype);
          widget_opts.mnemonic_label = FALSE;
          lives_label_set_text(LIVES_LABEL(conxwp->pclabel[x + totchans]), text);
          widget_opts.mnemonic_label = TRUE;
          lives_free(text);
          lives_free(pname);
          lives_free(ptype);

          isfirst = FALSE;
        }

        x++;

        if (nconns > 1) {
          lives_table_resize(LIVES_TABLE(conxwp->tablep), ++conxwp->trowsp, 7);
        }
      } while (--nconns > 0);
    }

    for (i = 0; i < conxwp->num_params - EXTRA_PARAMS_OUT; i++) {
      lives_table_resize(LIVES_TABLE(conxwp->tablep), ++conxwp->trowsp, 7);

      nconns = conxwp->dispp[i + EXTRA_PARAMS_OUT];

      isfirst = TRUE;

      do {
        ptable_row_add_variable_widgets(conxwp, x, conxwp->trowsp - 1, i);
        ptable_row_add_standard_widgets(conxwp, x + totchans);

        if (isfirst) {
          param = oparams[i];

          pname = get_param_name(param, i, FALSE);

          ptype = weed_seed_type_to_text((stype = weed_leaf_seed_type(param, WEED_LEAF_DEFAULT)));

          pflags = weed_get_int_value(param, WEED_LEAF_FLAGS, NULL);

          if (pflags & WEED_PARAMETER_VARIABLE_SIZE) array_type = lives_strdup("[]");
          else if ((defelems = weed_leaf_num_elements(param, WEED_LEAF_DEFAULT)) > 1)
            array_type = lives_strdup_printf("[%d]", defelems);
          else array_type = lives_strdup("");

          if (weed_plant_has_leaf(param, WEED_LEAF_MAX) && weed_plant_has_leaf(param, WEED_LEAF_MIN)) {
            if (stype == WEED_SEED_INT) {
              range = lives_strdup_printf("Range: %d to %d", weed_get_int_value(param, WEED_LEAF_MIN, NULL),
                                          weed_get_int_value(param,
                                              WEED_LEAF_MAX,
                                              NULL));
            } else if (stype == WEED_SEED_DOUBLE) {
              range = lives_strdup_printf("Range: %f to %f", weed_get_double_value(param, WEED_LEAF_MIN, NULL),
                                          weed_get_double_value(param,
                                              WEED_LEAF_MAX,
                                              NULL));
            } else range = lives_strdup("");
          } else range = lives_strdup("");

          text = lives_strdup_printf("%s\n (%s%s) %s", pname, ptype, array_type, range);
          widget_opts.mnemonic_label = FALSE;
          lives_label_set_text(LIVES_LABEL(conxwp->pclabel[x + totchans]), text);
          widget_opts.mnemonic_label = TRUE;
          lives_free(text);
          lives_free(pname);
          lives_free(ptype);

          isfirst = FALSE;
        }

        x++;

        if (nconns > 1) {
          lives_table_resize(LIVES_TABLE(conxwp->tablep), ++conxwp->trowsp, 7);
        }
      } while (--nconns > 0);
    }

    lives_free(oparams);
  }

  lives_free(fname);

  return scrolledwindow;
}


static void conxw_cancel_clicked(LiVESWidget * button, livespointer user_data) {
  lives_conx_w *conxwp = (lives_conx_w *)user_data;

  /// TODO - consider the benefits of using LSD
  if (conxwp->pclabel) lives_free(conxwp->pclabel);
  if (conxwp->clabel) lives_free(conxwp->clabel);
  if (conxwp->cfxcombo) lives_free(conxwp->cfxcombo);
  if (conxwp->ccombo) lives_free(conxwp->ccombo);
  if (conxwp->pfxcombo) lives_free(conxwp->pfxcombo);
  if (conxwp->pcombo) lives_free(conxwp->pcombo);
  if (conxwp->acheck) lives_free(conxwp->acheck);
  if (conxwp->add_button) lives_free(conxwp->add_button);
  if (conxwp->del_button) lives_free(conxwp->del_button);
  if (conxwp->dpp_func) lives_free(conxwp->dpp_func);
  if (conxwp->dpc_func) lives_free(conxwp->dpc_func);
  if (conxwp->acheck_func) lives_free(conxwp->acheck_func);

  if (conxwp->dispp) lives_free(conxwp->dispp);
  if (conxwp->dispc) lives_free(conxwp->dispc);

  lives_free(conxwp->ikeys);
  lives_free(conxwp->imodes);
  lives_free(conxwp->idx);

  lives_free(lctext);

  pconx_delete_all();
  cconx_delete_all();

  if (!button) return;

  // restore old values
  mainw->pconx = spconx;
  mainw->cconx = scconx;

  lives_general_button_clicked(LIVES_BUTTON(button), NULL);
}


static void conxw_ok_clicked(LiVESWidget * button, livespointer user_data) {
  lives_cconnect_t *cconx_bak = mainw->cconx;
  lives_pconnect_t *pconx_bak = mainw->pconx;

  // let backup copy be freed
  mainw->pconx = spconx;
  mainw->cconx = scconx;

  conxw_cancel_clicked(NULL, user_data);

  mainw->cconx = cconx_bak;
  mainw->pconx = pconx_bak;

  lives_general_button_clicked(LIVES_BUTTON(button), NULL);
}


static void set_to_keymode_vals(LiVESCombo * combo, int xkey, int xmode) {
  LiVESTreeIter iter, piter;
  LiVESTreeModel *model;

  int key, mode;

  model = lives_combo_get_model(combo);
  if (!lives_tree_model_get_iter_first(model, &piter)) return;

  do {
    if (lives_tree_model_iter_children(model, &iter, &piter)) {
      do {
        lives_tree_model_get(model, &iter, KEYVAL_COLUMN, &key, MODEVAL_COLUMN, &mode, -1);
        if (key == (key > -0 ? xkey + 1 : key) && mode == xmode) goto iter_found;
      } while (lives_tree_model_iter_next(model, &iter));
    }
  } while (lives_tree_model_iter_next(model, &piter));

iter_found:
  lives_combo_set_active_iter(combo, &iter);

}


static boolean show_existing(lives_conx_w * conxwp) {
  lives_cconnect_t *cconx = conxwp->cconx;
  lives_pconnect_t *pconx = conxwp->pconx;

  LiVESWidget *cfxcombo, *ccombo;
  LiVESWidget *pfxcombo, *pcombo;
  LiVESWidget *acheck;

  weed_plant_t **ochans, **ichans;
  weed_plant_t **iparams;
  weed_plant_t *ofilter = conxwp->filter, *filter;
  weed_plant_t *chan, *param;

  int ikey, imode, icnum, ipnum, nichans, niparams;
  int posn = 0, cidx, pidx, totchans = 0;
  int i, j, k, l;

  if (!cconx) goto show_ex_params;

  totchans = cconx_get_numcons(conxwp, FX_DATA_WILDCARD);

  ochans = weed_get_plantptr_array(ofilter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, NULL);

  for (i = 0; i < cconx->nchans; i++) {
    // find the row
    l = 0;

    // total out channel connections (display order) up to here
    for (k = 0; k < i; k++) {
      chan = ochans[j++];
      if (!has_alpha_palette(chan, ofilter)) {
        k--;
        continue;
      }
      l += cconx_get_numcons(conxwp, k);
    }

    cidx = cconx->chans[i];
    for (j = posn; j < posn + cconx->nconns[i]; j++) {
      ikey = cconx->ikey[j];
      imode = cconx->imode[j];

      // row is l
      cfxcombo = conxwp->cfxcombo[l];

      // set it to the value which has ikey/imode
      set_to_keymode_vals(LIVES_COMBO(cfxcombo), ikey, imode);

      // set channel
      ccombo = conxwp->ccombo[l];
      icnum = cconx->icnum[j];

      filter = rte_keymode_get_filter(ikey + 1, imode);
      ichans = weed_get_plantptr_array(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, NULL);
      nichans = weed_leaf_num_elements(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES);

      cidx = 0;

      // find combo list index for ichan
      for (k = 0; k < nichans; k++) {
        chan = ichans[k];
        if (!has_alpha_palette(chan, filter)) continue;
        if (k == icnum) break;
        cidx++;
      }

      lives_free(ichans);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ccombo), "setup", LIVES_INT_TO_POINTER(TRUE));

      lives_signal_handler_block(ccombo, conxwp->dpc_func[l]);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ccombo), "idx", LIVES_INT_TO_POINTER(cidx));
      lives_combo_set_active_index(LIVES_COMBO(ccombo), cidx);
      lives_signal_handler_unblock(ccombo, conxwp->dpc_func[l]);

      conxwp->ikeys[l] = ikey;
      conxwp->imodes[l] = imode;
      conxwp->idx[l] = icnum;

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ccombo), "setup", LIVES_INT_TO_POINTER(FALSE));
      lives_widget_set_sensitive(conxwp->disconbutton, TRUE);
      lives_widget_set_sensitive(ccombo, TRUE);

      l++;
    }

    posn += cconx->nconns[i];
  }

  lives_free(ochans);

show_ex_params:

  if (!pconx) goto show_ex_done;

  posn = 0;

  for (i = 0; i < pconx->nparams; i++) {
    pidx = pconx->params[i];

    // find the row
    l = 0;

    // total out param connections (display order) up to here
    for (k = -EXTRA_PARAMS_OUT; k < pidx; k++) {
      l += pconx_get_numcons(conxwp, k);
    }

    for (j = posn; j < posn + pconx->nconns[i]; j++) {
      ikey = pconx->ikey[j];
      imode = pconx->imode[j];

      // row is l
      pfxcombo = conxwp->pfxcombo[l];

      // set it to the value which has ikey/imode
      set_to_keymode_vals(LIVES_COMBO(pfxcombo), ikey, imode);

      // set parameter
      pcombo = conxwp->pcombo[l];
      acheck = conxwp->acheck[l];

      if (j == posn + pconx->nconns[i] - 1)
        lives_widget_set_sensitive(conxwp->add_button[l], TRUE);

      if (acheck) {
        boolean hasrange = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(acheck), "available"));
        if (hasrange) {
          g_print("ach sens 2\n");
          lives_widget_set_sensitive(acheck, TRUE);
          lives_signal_handler_block(acheck, conxwp->acheck_func[l]);
          lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acheck), pconx->autoscale[j]);
          lives_signal_handler_unblock(acheck, conxwp->acheck_func[l]);
        }
      }

      ipnum = pconx->ipnum[j];

      // TODO ****: OMC

      if (ipnum == FX_DATA_PARAM_ACTIVE) {
        pidx = FX_DATA_PARAM_ACTIVE;
      } else {
        filter = rte_keymode_get_filter(ikey + 1, imode);
        iparams = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, NULL);
        niparams = weed_leaf_num_elements(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES);

#ifdef COMBO_LIST_LIMIT
        if (niparams + EXTRA_PARAMS_IN > COMBO_LIST_LIMIT) {
          niparams = COMBO_LIST_LIMIT - EXTRA_PARAMS_IN;
        }
#endif
        pidx = 0;

        // find combo list index for iparam
        for (k = 0; k < niparams; k++) {
          param = iparams[k];
          if (weed_plant_has_leaf(param, WEED_LEAF_HOST_INTERNAL_CONNECTION)) continue;
          if (k == ipnum) break;
          pidx++;
        }

        lives_free(iparams);
      }

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pcombo), "setup", LIVES_INT_TO_POINTER(TRUE));

      conxwp->ikeys[totchans + l] = ikey;
      conxwp->imodes[totchans + l] = imode;
      conxwp->idx[totchans + l] = ipnum;

      //lives_signal_handler_block(pcombo,conxwp->dpp_func[pidx]);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pcombo), "idx", LIVES_INT_TO_POINTER(pidx + EXTRA_PARAMS_IN));
      lives_combo_set_active_index(LIVES_COMBO(pcombo), pidx + EXTRA_PARAMS_IN);
      //lives_signal_handler_unblock(pcombo,conxwp->dpp_func[pidx]);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pcombo), "setup", LIVES_INT_TO_POINTER(FALSE));

      lives_widget_set_sensitive(conxwp->disconbutton, TRUE);
      lives_widget_set_sensitive(pcombo, TRUE);

      l++;
    }

    posn += pconx->nconns[i];
  }

show_ex_done:

  return FALSE;
}


LiVESWidget *make_datacon_window(int key, int mode) {
  static lives_conx_w conxw;

  LiVESWidget *cbox, *abox;
  LiVESWidget *scrolledwindow;

  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  LiVESAccelGroup *accel_group;

  int winsize_h;
  int winsize_v;

  conxw.filter = rte_keymode_get_filter(key + 1, mode);

  if (!conxw.filter) return NULL;

  conxw.acbutton = conxw.apbutton = NULL;

  conxw.dispp = conxw.dispc = NULL;

  // save unaltered values
  spconx = pconx_copy(mainw->pconx);
  scconx = cconx_copy(mainw->cconx);

  conxw.cconx = cconx_find(key, mode);
  conxw.pconx = pconx_find(key, mode);

  conxw.okey = key;
  conxw.omode = mode;

  conxw.num_alpha = num_alpha_channels(conxw.filter, TRUE);
  conxw.num_params = weed_leaf_num_elements(conxw.filter, WEED_LEAF_OUT_PARAMETER_TEMPLATES);

  conxw.num_params += EXTRA_PARAMS_OUT;

  if (conxw.num_params > 0)
    conxw.dispp = (int *)lives_malloc(conxw.num_params * sizint);

  conxw.ntabs = 0;

  winsize_h = GUI_SCREEN_WIDTH - SCR_WIDTH_SAFETY * 2;
  winsize_v = GUI_SCREEN_HEIGHT - SCR_HEIGHT_SAFETY;

  conxw.conx_dialog = lives_standard_dialog_new(_("Parameter and Alpha Channel Connections"), FALSE, winsize_h, winsize_v);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(conxw.conx_dialog), accel_group);

  if (conxw.num_alpha > 0) {
    conxw.dispc = (int *)lives_malloc(conxw.num_alpha * sizint);

    conxw.acbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(conxw.conx_dialog), NULL, _("Auto Connect Channels"),
                     LIVES_RESPONSE_NONE);
    lives_widget_set_sensitive(conxw.acbutton, FALSE);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(conxw.acbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(acbutton_clicked), (livespointer)&conxw);
  }

  if (conxw.num_params > EXTRA_PARAMS_OUT) {
    conxw.apbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(conxw.conx_dialog), NULL, _("Auto Connect Parameters"),
                     LIVES_RESPONSE_NONE);
    lives_widget_set_sensitive(conxw.apbutton, FALSE);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(conxw.apbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(apbutton_clicked), (livespointer)&conxw);
  }

  conxw.disconbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(conxw.conx_dialog), NULL, _("_Disconnect All"),
                       LIVES_RESPONSE_NONE);
  //lives_container_set_border_width(LIVES_CONTAINER(conxw.disconbutton), widget_opts.border_width);
  //!! dont - causes other buttons to exp. vert in gtk2
  lives_widget_set_sensitive(conxw.disconbutton, FALSE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(conxw.disconbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(disconbutton_clicked), (livespointer)&conxw);

  abox = lives_dialog_get_action_area(LIVES_DIALOG(conxw.conx_dialog));
  if (LIVES_IS_BOX(abox) && (conxw.num_alpha > 0 || conxw.num_params > 0)) add_fill_to_box(LIVES_BOX(abox));

  cbox = lives_dialog_get_content_area(LIVES_DIALOG(conxw.conx_dialog));

  scrolledwindow = conx_scroll_new(&conxw);
  show_existing(&conxw);

  lives_box_pack_start(LIVES_BOX(cbox), scrolledwindow, TRUE, TRUE, 0);

  if (conxw.num_params > EXTRA_PARAMS_OUT) {
    if (conxw.apbutton && pconx_get_nconns(conxw.pconx, 0) > 0)
      lives_widget_set_sensitive(conxw.apbutton, TRUE);
  }
  if (conxw.num_alpha > 0) {
    if (conxw.acbutton && cconx_get_nconns(conxw.cconx, 0) > 0)
      lives_widget_set_sensitive(conxw.acbutton, TRUE);
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(conxw.conx_dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(conxw.conx_dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(conxw_cancel_clicked),
                            (livespointer)&conxw);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(conxw_ok_clicked),
                            (livespointer)&conxw);

  lives_widget_show_all(conxw.conx_dialog);

  if (conxw.num_params == EXTRA_PARAMS_OUT && EXTRA_PARAMS_OUT > 0) {
    lives_widget_set_no_show_all(conxw.allcheckc, TRUE);
    lives_widget_hide(conxw.allcheckc);
    lives_widget_set_no_show_all(conxw.allcheck_label, TRUE);
    lives_widget_hide(conxw.allcheck_label);
  }

  return conxw.conx_dialog;
}


static boolean do_chan_connected_query(lives_conx_w * conxwp, int key, int mode, int cnum, boolean is_same) {
  weed_plant_t *filter = rte_keymode_get_filter(key + 1, mode);
  weed_plant_t *ctmpl, **ochans;
  char *cname, *msg, *fname = weed_filter_get_name(filter);
  boolean resp = FALSE;
  ochans = weed_get_plantptr_array(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, NULL);
  ctmpl = ochans[cnum];
  lives_free(ochans);
  cname = get_chan_name(ctmpl, cnum, TRUE);
  widget_opts.transient = LIVES_WINDOW(conxwp->conx_dialog);
  msg = lives_strdup_printf(_("Input channel is already connected from key %d, mode %d\n\n(%s:%s)\n\n"),
                            key + 1, mode, fname, cname);
  if (!is_same)
    resp = do_yesno_dialogf(_("%sWould you like to replace the existing connection ?\n\n"), msg);

  else
    do_error_dialogf(_("%sYou need to remove this connection first before adding another.\n\n"), msg);
  widget_opts.transient = NULL;
  lives_free(msg); lives_free(cname); lives_free(fname);
  return resp;
}


static boolean do_param_connected_query(lives_conx_w * conxwp, int key, int mode, int pnum, boolean is_same) {
  weed_plant_t *filter = rte_keymode_get_filter(key + 1, mode);
  char *pname, *msg, *fname = weed_filter_get_name(filter);
  boolean resp = FALSE;
  if (pnum >= 0) {
    weed_plant_t *ptmpl = weed_filter_out_paramtmpl(filter, pnum);
    pname = get_param_name(ptmpl, pnum, TRUE);
  } else pname = (_("ACTIVATED"));
  widget_opts.transient = LIVES_WINDOW(conxwp->conx_dialog);
  msg = lives_strdup_printf(_("Input parameter is already connected from key %d, mode %d\n\n(%s:%s)\n\n"),
                            key + 1, mode, fname, pname);
  if (!is_same)
    resp = do_yesno_dialogf(_("%sWould you like to replace the existing connection ?\n\n"), msg);

  else
    do_error_dialogf(_("%sYou need to remove this connection first before adding another.\n\n"), msg);
  widget_opts.transient = NULL;
  lives_free(msg); lives_free(pname); lives_free(fname);
  return resp;
}


static void do_param_incompatible_error(lives_conx_w * conxwp) {
  widget_opts.transient = LIVES_WINDOW(conxwp->conx_dialog);
  do_error_dialog(_("Input and output parameters are not compatible"));
  widget_opts.transient = NULL;
}

