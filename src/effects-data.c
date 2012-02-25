// effects-data.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// functions for chaining and data passing between fx plugins

#include "main.h"



#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#include <weed/weed-effects.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-effects.h"
#endif


#include "effects.h"


static lives_pconnect_t *pconx_new (int ikey, int imode) {
  lives_pconnect_t *pconx=(lives_pconnect_t *)g_malloc0(sizeof(struct _lives_pconnect_t));
  pconx->next=NULL;
  pconx->ikey=ikey;
  pconx->imode=imode;
  return pconx;
}


static void pconx_append(lives_pconnect_t *pconx) {
 lives_pconnect_t *opconx=mainw->pconx;
 lives_pconnect_t *last_pconx=opconx;

  while (opconx!=NULL) {
    last_pconx=opconx;
    opconx=opconx->next;
  }

  if (last_pconx!=NULL) last_pconx->next=pconx;
  if (mainw->pconx==NULL) mainw->pconx=pconx;
}


static lives_pconnect_t *pconx_find (int ikey, int imode) {
  lives_pconnect_t *pconx=mainw->pconx;
  while (pconx!=NULL) {
    if (pconx->ikey==ikey&&pconx->imode==imode) {
      return pconx;
    }
    pconx=pconx->next;
  }
  return NULL;
}



static int pconx_get_numcons(lives_pconnect_t *pconx) {
  int totcons=0;
  register int j;

  for (j=0;j<pconx->nparams;j++) {
    totcons+=pconx->nconns[j];
  }
  return totcons;
}



void pconx_add_connection(int ikey, int imode, int pnum, int okey, int omode, int opnum) {
  lives_pconnect_t *pconx=pconx_find(ikey,imode);
  int posn=0,totcons=0;
  register int i,j;

  if (pconx==NULL) {
    // add whole new node
    pconx=pconx_new(ikey,imode);
    pconx_append(pconx);
  }
  else {
    // see if already in params
    
    for (i=0;i<pconx->nparams;i++) {
      
      if (pconx->params[i]==pnum) {
	// add connection to existing
	pconx->nconns[i]++;

	for (j=0;j<pconx->nparams;j++) {
	  if (j<i) {
	    // calc posn
	    posn+=pconx->nconns[j];
	  }
	  totcons+=pconx->nconns[j];
	}

	// make space for new
	pconx->okey=(int *)g_realloc(pconx->okey,totcons*sizint);
	pconx->omode=(int *)g_realloc(pconx->omode,totcons*sizint);
	pconx->opnum=(int *)g_realloc(pconx->opnum,totcons*sizint);

	// move up 1
	for (j=totcons-1;j>posn;j--) {
	  pconx->okey[j]=pconx->okey[j-1];
	  pconx->omode[j]=pconx->omode[j-1];
	  pconx->opnum[j]=pconx->opnum[j-1];
	}

	// insert at posn
	pconx->okey[posn]=okey;
	pconx->omode[posn]=omode;
	pconx->opnum[posn]=opnum;

	return;
      }
      
    }
  }

  // add new

  pconx->nparams++;
  totcons=pconx_get_numcons(pconx);

  pconx->nconns=(int *)g_realloc(pconx->params,pconx->nparams*sizint);
  pconx->nconns[pconx->nparams-1]=1;

  pconx->params=(int *)g_realloc(pconx->params,pconx->nparams*sizint);
  pconx->params[pconx->nparams-1]=pnum;

  pconx->okey=(int *)g_realloc(pconx->okey,pconx->nparams*sizint);
  pconx->okey[pconx->nparams-1]=okey;

  pconx->omode=(int *)g_realloc(pconx->omode,pconx->nparams*sizint);
  pconx->omode[pconx->nparams-1]=omode;

  pconx->opnum=(int *)g_realloc(pconx->opnum,pconx->nparams*sizint);
  pconx->opnum[pconx->nparams-1]=opnum;


}



weed_plant_t *pconx_get_out_param(int okey, int omode, int opnum) {
  // walk all pconx and find one which has okey/omode/opnum as output
  // then all we need do is copy the "value" leaf

  lives_pconnect_t *pconx=mainw->pconx;

  weed_plant_t *inst;

  int totcons,error;
  register int i,j;

  while (pconx!=NULL) {
    if ((inst=rte_keymode_get_instance(pconx->ikey+1,pconx->imode))==NULL) return NULL; ///< effect is not enabled
    totcons=0;
    j=0;
    for (i=0;i<pconx->nparams;i++) {
      totcons+=pconx->nconns[i];
      for (;j<totcons;j++) {
	if (pconx->okey[j]==okey && pconx->omode[j]==omode && pconx->opnum[j]==opnum) {

	  if (!weed_plant_has_leaf(inst,"out_parameters")) return NULL;
	  else {
	    weed_plant_t **outparams=weed_get_plantptr_array(inst,"out_parameters",&error);
	    weed_plant_t *param=NULL;
	    if (i<weed_leaf_num_elements(inst,"out_parameters")) {
	      param=outparams[pconx->params[i]];
	    }
	    weed_free(outparams);
	    return param;
	  }
	}
      }
    }
    pconx=pconx->next;
  }

  return NULL;
}



gboolean pconx_convert_value_data(weed_plant_t *dparam, weed_plant_t *sparam) {
  // try to convert values of various type, if we succeed, copy the "value" and return TRUE
  weed_plant_t *dptmpl;

  int dtype,stype,nsvals,ndvals,error,dflags;

  register int i;

  // allowed conversions
  // type -> type

  // bool -> int, bool -> float, bool -> string, (bool -> int64)
  // int -> float, int -> string,  (int -> int64)
  // float -> string
  // (int64 -> string)

  // TODO : int[3]/float[3] -> colourRGB
  //        int[4]/float[4] -> colourRGBA
  //
  // provided we have min/max on output (for scaling)




  nsvals=weed_leaf_num_elements(sparam,"value");
  ndvals=weed_leaf_num_elements(dparam,"value");
  
  dptmpl=weed_get_plantptr_value(dparam,"template",&error);
  //sptmpl=weed_get_plantptr_value(sparam,"template",&error);

  //  sflags=weed_get_int_value(sptmpl,"flags",&error);
  dflags=weed_get_int_value(dptmpl,"flags",&error);

  if (((dtype=weed_leaf_seed_type(dparam,"value"))==(stype=weed_leaf_seed_type(sparam,"value"))) &&
      ((nsvals==ndvals) || (dflags&WEED_PARAMETER_VARIABLE_ELEMENTS))) {
    // values of same type and number, -> simpĺe copy
    weed_leaf_copy(dparam,"value",sparam,"value");
    return TRUE;
  }

  if (!((ndvals==1)||(ndvals==nsvals&&stype==WEED_SEED_INT&&dtype==WEED_SEED_DOUBLE))) return FALSE;

  switch (stype) {
  case WEED_SEED_DOUBLE:
    switch (dtype) {
    case WEED_SEED_STRING:
      {
	char *opstring=g_strdup(""),*tmp,*bit;
	double *valsd=weed_get_double_array(sparam,"value",&error);
	for (i=0;i<nsvals;i++) {
	  bit=g_strdup_printf("%.4f",valsd[i]);
	  if (strlen(opstring)==0) 
	    tmp=g_strconcat (opstring,bit,NULL);
	  else 
	    tmp=g_strconcat (opstring," ",bit,NULL);
	  g_free(opstring);
	  opstring=tmp;
	}
	weed_set_string_value(dparam,"value",opstring);
	weed_free(valsd);
	g_free(opstring);
      }
      return TRUE;
    default:
      break;
    }

    break;

  case WEED_SEED_INT:
    switch (dtype) {
    case WEED_SEED_STRING:
      {
	char *opstring=g_strdup(""),*tmp,*bit;
	int *valsi=weed_get_int_array(sparam,"value",&error);
	for (i=0;i<nsvals;i++) {
	  bit=g_strdup_printf("%d",valsi[i]);
	  if (strlen(opstring)==0) 
	    tmp=g_strconcat (opstring,bit,NULL);
	  else 
	    tmp=g_strconcat (opstring," ",bit,NULL);
	  g_free(opstring);
	  opstring=tmp;
	}
	weed_set_string_value(dparam,"value",opstring);
	weed_free(valsi);
	g_free(opstring);
      }
      return TRUE;
    case WEED_SEED_DOUBLE:
      {
	int *valsi=weed_get_int_array(sparam,"value",&error);
	double * valsd=g_try_malloc(ndvals*sizdbl);
	
	if (valsd==NULL) {
	  LIVES_WARN("Could not assign memory for dest value");
	  return TRUE;
	}
	
	for (i=0;i<nsvals;i++) {
	  valsd[i]=(double)valsi[i];
	}
	weed_set_double_array(dparam,"value",ndvals,valsd);
	weed_free(valsi);
	g_free(valsd);
      }
      return TRUE;

    }
    break;

  case WEED_SEED_BOOLEAN:
    switch (dtype) {
    case WEED_SEED_STRING:
      {
	char *opstring=g_strdup(""),*tmp,*bit;
	int *valsi=weed_get_boolean_array(sparam,"value",&error);
	for (i=0;i<nsvals;i++) {
	  bit=g_strdup_printf("%d",valsi[i]);
	  if (strlen(opstring)==0) 
	    tmp=g_strconcat (opstring,bit,NULL);
	  else 
	    tmp=g_strconcat (opstring," ",bit,NULL);
	  g_free(opstring);
	  opstring=tmp;
	}
	weed_set_string_value(dparam,"value",opstring);
	weed_free(valsi);
	g_free(opstring);
      }
      return TRUE;
    case WEED_SEED_DOUBLE:
      {
	int *valsi=weed_get_boolean_array(sparam,"value",&error);
	double * valsd=g_try_malloc(ndvals*sizdbl);
	
	if (valsd==NULL) {
	  LIVES_WARN("Could not assign memory for dest value");
	  return TRUE;
	}
	
	for (i=0;i<nsvals;i++) {
	  valsd[i]=(double)valsi[i];
	}
	weed_set_double_array(dparam,"value",ndvals,valsd);
	weed_free(valsi);
	g_free(valsd);
      }
      return TRUE;
    case WEED_SEED_INT:
      {
	int *valsi=weed_get_boolean_array(sparam,"value",&error);
	weed_set_int_array(dparam,"value",ndvals,valsi);
	weed_free(valsi);
      }
      return TRUE;
    default:
      break;
    }

    break;

  default:
    break;
    }


  return FALSE;
}





void pconx_chain_data(int key, int mode) {
  int error;
  int nparams=0;
  weed_plant_t **inparams;
  weed_plant_t *oparam;
  weed_plant_t *inst;

  register int i;

  if ((inst=rte_keymode_get_instance(key+1,mode))==NULL) return; ///< effect is not enabled
  if (weed_plant_has_leaf(inst,"in_parameters")) nparams=weed_leaf_num_elements(inst,"in_parameters");

  if (nparams>0) {
    inparams=weed_get_plantptr_array(inst,"in_parameters",&error);

    for (i=0;i<nparams;i++) {
      if ((oparam=pconx_get_out_param(key,mode,i))!=NULL) {
	pconx_convert_value_data(inparams[i],oparam);
      }
    }
    weed_free(inparams);
  }
}
