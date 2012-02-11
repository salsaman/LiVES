// effects-data.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// functions for chaining and data passing

#include "main.h"



#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
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
	pconx->okey=g_realloc(pconx->okey,totcons*sizint);
	pconx->omode=g_realloc(pconx->omode,totcons*sizint);
	pconx->opnum=g_realloc(pconx->opnum,totcons*sizint);

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


  pconx->params=g_realloc(pconx->params,pconx->nparams*sizint);
  pconx->params[pconx->nparams-1]=pnum;

  pconx->okey=g_realloc(pconx->okey,pconx->nparams*sizint);
  pconx->okey[pconx->nparams-1]=pnum;

  pconx->omode=g_realloc(pconx->omode,pconx->nparams*sizint);
  pconx->omode[pconx->nparams-1]=pnum;

  pconx->opnum=g_realloc(pconx->opnum,pconx->nparams*sizint);
  pconx->opnum[pconx->nparams-1]=pnum;


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
	  weed_plant_t **inparams=weed_get_plantptr_array(inst,"in_parameters",&error);
	  weed_plant_t *param=inparams[pconx->params[i]];
	  weed_free(inparams);
	  return param;
	}
      }
    }
    pconx=pconx->next;
  }

  return NULL;
}





void fx_chain_data(weed_plant_t *inst, int key, int mode) {
  int error;
  int nparams=weed_leaf_num_elements(inst,"in_parameters");
  weed_plant_t **inparams=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t *oparam;

  register int i;
  
  for (i=0;i<nparams;i++) {
    if ((oparam=pconx_get_out_param(key,mode,i))!=NULL) {
      weed_leaf_copy(inparams[i],"value",oparam,"value");
    }
  }

}
