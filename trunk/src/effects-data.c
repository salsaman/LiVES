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



void pconx_add_connection(int ikey, int imode, int ipnum, int okey, int omode, int opnum) {
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
      
      if (pconx->params[i]==ipnum) {
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
  pconx->params[pconx->nparams-1]=ipnum;

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











// alpha channs






static lives_cconnect_t *cconx_new (int ikey, int imode) {
  lives_cconnect_t *cconx=(lives_cconnect_t *)g_malloc0(sizeof(struct _lives_cconnect_t));
  cconx->next=NULL;
  cconx->ikey=ikey;
  cconx->imode=imode;
  return cconx;
}


static void cconx_append(lives_cconnect_t *cconx) {
 lives_cconnect_t *occonx=mainw->cconx;
 lives_cconnect_t *last_cconx=occonx;

  while (occonx!=NULL) {
    last_cconx=occonx;
    occonx=occonx->next;
  }

  if (last_cconx!=NULL) last_cconx->next=cconx;
  if (mainw->cconx==NULL) mainw->cconx=cconx;
}


static lives_cconnect_t *cconx_find (int ikey, int imode) {
  lives_cconnect_t *cconx=mainw->cconx;
  while (cconx!=NULL) {
    if (cconx->ikey==ikey&&cconx->imode==imode) {
      return cconx;
    }
    cconx=cconx->next;
  }
  return NULL;
}



static int cconx_get_numcons(lives_cconnect_t *cconx) {
  int totcons=0;
  register int j;

  for (j=0;j<cconx->nchans;j++) {
    totcons+=cconx->nconns[j];
  }
  return totcons;
}



void cconx_add_connection(int ikey, int imode, int icnum, int okey, int omode, int ocnum) {
  lives_cconnect_t *cconx=cconx_find(ikey,imode);
  int posn=0,totcons=0;
  register int i,j;

  if (cconx==NULL) {
    // add whole new node
    cconx=cconx_new(ikey,imode);
    cconx_append(cconx);
  }
  else {
    // see if already in chans
    
    for (i=0;i<cconx->nchans;i++) {
      
      if (cconx->chans[i]==icnum) {
	// add connection to existing
	cconx->nconns[i]++;

	for (j=0;j<cconx->nchans;j++) {
	  if (j<i) {
	    // calc posn
	    posn+=cconx->nconns[j];
	  }
	  totcons+=cconx->nconns[j];
	}

	// make space for new
	cconx->okey=(int *)g_realloc(cconx->okey,totcons*sizint);
	cconx->omode=(int *)g_realloc(cconx->omode,totcons*sizint);
	cconx->ocnum=(int *)g_realloc(cconx->ocnum,totcons*sizint);

	// move up 1
	for (j=totcons-1;j>posn;j--) {
	  cconx->okey[j]=cconx->okey[j-1];
	  cconx->omode[j]=cconx->omode[j-1];
	  cconx->ocnum[j]=cconx->ocnum[j-1];
	}

	// insert at posn
	cconx->okey[posn]=okey;
	cconx->omode[posn]=omode;
	cconx->ocnum[posn]=ocnum;

	return;
      }
      
    }
  }

  // add new

  cconx->nchans++;
  totcons=cconx_get_numcons(cconx);

  cconx->nconns=(int *)g_realloc(cconx->chans,cconx->nchans*sizint);
  cconx->nconns[cconx->nchans-1]=1;

  cconx->chans=(int *)g_realloc(cconx->chans,cconx->nchans*sizint);
  cconx->chans[cconx->nchans-1]=icnum;

  cconx->okey=(int *)g_realloc(cconx->okey,cconx->nchans*sizint);
  cconx->okey[cconx->nchans-1]=okey;

  cconx->omode=(int *)g_realloc(cconx->omode,cconx->nchans*sizint);
  cconx->omode[cconx->nchans-1]=omode;

  cconx->ocnum=(int *)g_realloc(cconx->ocnum,cconx->nchans*sizint);
  cconx->ocnum[cconx->nchans-1]=ocnum;


}



weed_plant_t *cconx_get_out_alpha(int okey, int omode, int ocnum) {
  // walk all cconx and find one which has okey/omode/opnum as output
  // then all we need do is convert the pixel_data

  lives_cconnect_t *cconx=mainw->cconx;

  weed_plant_t *inst;

  int totcons,error;
  register int i,j;

  while (cconx!=NULL) {
    if ((inst=rte_keymode_get_instance(cconx->ikey+1,cconx->imode))==NULL) return NULL; ///< effect is not enabled
    totcons=0;
    j=0;
    for (i=0;i<cconx->nchans;i++) {
      totcons+=cconx->nconns[i];
      for (;j<totcons;j++) {
	if (cconx->okey[j]==okey && cconx->omode[j]==omode && cconx->ocnum[j]==ocnum) {

	  if (!weed_plant_has_leaf(inst,"out_channels")) return NULL;
	  else {
	    weed_plant_t **outchans=weed_get_plantptr_array(inst,"out_channels",&error);
	    weed_plant_t *channel=NULL;
	    if (i<weed_leaf_num_elements(inst,"out_channels")) {
	      channel=outchans[cconx->chans[i]];
	    }
	    weed_free(outchans);
	    return channel;
	  }
	}
      }
    }
    cconx=cconx->next;
  }

  return NULL;
}



gboolean cconx_convert_pixel_data(weed_plant_t *dchan, weed_plant_t *schan) {
  // convert pixel_data by possibly converting the type (palette)

  // return TRUE if we need to reinit the instance (because channel palette changed)

  // we set boolean "host_orig_pdata" if we steal the schan pdata (so do not free....)

  int error;
  int iwidth,iheight,ipal,irow;
  int owidth,oheight,opal,orow,oflags;
  gboolean pal_ok,needs_reinit=FALSE;

  weed_plant_t *dtmpl=weed_get_plantptr_value(dchan,"template",&error);

  void *spdata,*dpdata;

  register int i;

   iwidth=weed_get_int_value(schan,"width",&error);
  iheight=weed_get_int_value(schan,"height",&error);
  ipal=weed_get_int_value(schan,"current_palette",&error);
  irow=weed_get_int_value(schan,"rowstrides",&error);

  owidth=weed_get_int_value(dchan,"width",&error);
  oheight=weed_get_int_value(dchan,"height",&error);
  opal=weed_get_int_value(dchan,"current_palette",&error);

  spdata=weed_get_voidptr_value(schan,"pixel_data",&error);

  if (ipal==opal&&iwidth==owidth&&iheight==oheight&&irow==orow) {
    /// everything matches - we can just do a steal
    weed_set_voidptr_value(dchan,"pixel_data",spdata);

    /// caller - do not free in dchan
    weed_set_boolean_value(dchan,"host_orig_pdata",WEED_TRUE);
    return FALSE;
  }


  /// check first if we can set the in-channel palette to match 
  if (ipal==opal) pal_ok=TRUE;
  else {
    /// see if dest chan supports the source chan palette
    int num_palettes=weed_leaf_num_elements(dtmpl,"palette_list");
    int *palettes=weed_get_int_array(dtmpl,"palette_list",&error);
    if (check_weed_palette_list(palettes,num_palettes,ipal)==ipal) pal_ok=TRUE; ///< yes
    else pal_ok=FALSE; ///<no
    oflags=weed_get_int_value(dtmpl,"flags",&error);
    if (ipal!=opal&&(oflags&WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)) needs_reinit=TRUE;
    weed_free(palettes);
  }

  weed_set_int_value(dchan,"width",iwidth);
  weed_set_int_value(dchan,"height",iheight);
  weed_set_int_value(dchan,"current_palette",ipal);

  create_empty_pixel_data(dchan,FALSE,TRUE);

  orow=weed_get_int_value(dchan,"rowstrides",&error);

  dpdata=weed_get_voidptr_value(dchan,"pixel_data",&error);

  if (irow==orow) {
    memcpy(dpdata,spdata,irow*iheight);
  }
  else {
    int ipwidth = iwidth * weed_palette_get_bits_per_macropixel(ipal) / 8;
    for (i=0;i<iheight;i++) {
      memcpy(dpdata,spdata,ipwidth);
      spdata+=irow;
      dpdata+=orow;
    }
  }

  if (!pal_ok) convert_layer_palette(dchan,opal,0);

  if (needs_reinit) return TRUE;

  return FALSE;
}




gboolean cconx_chain_data(int key, int mode) {
  // ret TRUE if we should reinit inst (because of palette change)

  weed_plant_t *ichan,*ochan;
  weed_plant_t *inst;

  register int i=0;

  if ((inst=rte_keymode_get_instance(key+1,mode))==NULL) return FALSE; ///< effect is not enabled

  while ((ichan=get_enabled_channel(inst,i,TRUE))!=NULL) {
    if ((ochan=cconx_get_out_alpha(key,mode,i++))!=NULL) {
       return cconx_convert_pixel_data(ichan,ochan);
    }
  }
  return FALSE;
}
 



