// effects-data.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2012 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// functions for chaining and data passing between fx plugins

#include "main.h"

//#define DEBUG_PCONX

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


void pconx_delete_all(void) {
  lives_pconnect_t *pconx=mainw->pconx,*pconx_next;

  pthread_mutex_lock(&mainw->data_mutex);

  while (pconx!=NULL) {
    pconx_next=pconx->next;
    g_free(pconx->params);
    g_free(pconx->nconns);
    g_free(pconx->ikey);
    g_free(pconx->imode);
    g_free(pconx->ipnum);
    g_free(pconx->autoscale);
    g_free(pconx);
    pconx=pconx_next;
  }
  mainw->pconx=NULL;

  pthread_mutex_unlock(&mainw->data_mutex);

}



void pconx_delete(int okey, int omode, int opnum, int ikey, int imode, int ipnum) {
  lives_pconnect_t *pconx=mainw->pconx,*pconx_next,*pconx_prev=NULL;

  register int i,j=0,k;

  int totcons=0,maxcons=0;

  pthread_mutex_lock(&mainw->data_mutex);

  while (pconx!=NULL) {
    pconx_next=pconx->next;
    if (okey==-1||(pconx->okey==okey&&pconx->omode==omode)) {
      if (ikey==-1) {
	// delete entire node
	g_free(pconx->params);
	g_free(pconx->nconns);
	g_free(pconx->ikey);
	g_free(pconx->imode);
	g_free(pconx->ipnum);
	g_free(pconx->autoscale);
	g_free(pconx);
	if (mainw->pconx==pconx) mainw->pconx=pconx_next;
	else pconx_prev->next=pconx_next;
	pthread_mutex_unlock(&mainw->data_mutex);
	return;
      }

      for (i=0;i<pconx->nparams;i++) {
	maxcons+=pconx->nconns[i];
      }

      for (i=0;i<pconx->nparams;i++) {
	totcons+=pconx->nconns[i];

	if (okey!=-1&&pconx->params[i]!=opnum) {
	  j+=totcons;
	  continue;
	}

	for (;j<totcons;j++) {
	  if (pconx->ikey[j]==ikey && pconx->imode[j]==imode && pconx->ipnum[j]==ipnum) {
	    maxcons--;
	    for (k=j;k<maxcons;k++) {
	      pconx->ikey[k]=pconx->ikey[k+1];
	      pconx->imode[k]=pconx->imode[k+1];
	      pconx->ipnum[k]=pconx->ipnum[k+1];
	      pconx->autoscale[k]=pconx->autoscale[k+1];
	    }

	    pconx->ikey=(int *)g_realloc(pconx->ikey,maxcons*sizint);
	    pconx->imode=(int *)g_realloc(pconx->imode,maxcons*sizint);
	    pconx->ipnum=(int *)g_realloc(pconx->ipnum,maxcons*sizint);
	    pconx->autoscale=(int *)g_realloc(pconx->autoscale,maxcons*sizint);

	    pconx->nconns[i]--;

	    if (pconx->nconns[i]==0) {
	      pconx->nparams--;
	      for (k=i;k<pconx->nparams;k++) {
		pconx->nconns[k]=pconx->nconns[k+1];
	      }

	      if (pconx->nparams==0) {
		// delete entire node
		g_free(pconx->params);
		g_free(pconx->nconns);
		g_free(pconx->ikey);
		g_free(pconx->imode);
		g_free(pconx->ipnum);
		g_free(pconx->autoscale);
		g_free(pconx);
		if (mainw->pconx==pconx) {
		  mainw->pconx=pconx_next;
		  pconx=NULL;
		}
		else {
		  pconx=pconx_prev;
		  pconx->next=pconx_next;
		}
	      }
	      else {
		pconx->nconns=(int *)g_realloc(pconx->nconns,pconx->nparams*sizint);
	      }
	    }
	  }
	}
	j+=totcons;
      }
    }
    pconx_prev=pconx;
    pconx=pconx_next;
  }
  pthread_mutex_unlock(&mainw->data_mutex);
}


void pconx_remap_mode(int key, int omode, int nmode) {
  lives_pconnect_t *pconx=mainw->pconx;

  register int i,j,totcons;

  while (pconx!=NULL) {
    if (pconx->okey==key&&pconx->omode==omode) {
      pconx->omode=nmode;
    }
    j=0;
    totcons=0;
    for (i=0;i<pconx->nparams;i++) {
      totcons+=pconx->nconns[i];
      for (;j<totcons;j++) {
	if (pconx->ikey[j]==key && pconx->imode[j]==omode) {
	  pconx->imode[j]=nmode;
	}
      }
    }
    pconx=pconx->next;
  }
}


static lives_pconnect_t *pconx_new (int okey, int omode) {
  lives_pconnect_t *pconx=(lives_pconnect_t *)g_malloc0(sizeof(struct _lives_pconnect_t));
  pconx->next=NULL;
  pconx->okey=okey;
  pconx->omode=omode;
  pconx->nparams=0;
  pconx->nconns=NULL;
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


static lives_pconnect_t *pconx_find (int okey, int omode) {
  //
  lives_pconnect_t *pconx=mainw->pconx;
  while (pconx!=NULL) {
    if (pconx->okey==okey&&pconx->omode==omode) {
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



void pconx_add_connection(int okey, int omode, int opnum, int ikey, int imode, int ipnum, boolean autoscale) {
  lives_pconnect_t *pconx=pconx_find(okey,omode);
  int posn=0,totcons=0;
  register int i,j;

  pthread_mutex_lock(&mainw->data_mutex);

  if (pconx==NULL) {
    // add whole new node
    pconx=pconx_new(okey,omode);
    pconx_append(pconx);
  }
  else {
    // see if already in params
    
    for (i=0;i<pconx->nparams;i++) {
      
      if (pconx->params[i]==opnum) {
	// located !
	// add connection to existing

	// increment nconns for this param
	pconx->nconns[i]++;

	for (j=0;j<pconx->nparams;j++) {
	  if (j<i) {
	    // calc posn
	    posn+=pconx->nconns[j];
	  }
	  totcons+=pconx->nconns[j];
	}

	// if already there, do not add again, just update autoscale
	for (j=posn;j<totcons;j++) {
	  if (pconx->ikey[j]==ikey&&pconx->imode[j]==imode&&pconx->ipnum[j]==ipnum) {
	    pconx->autoscale[j]=autoscale;
	    pthread_mutex_unlock(&mainw->data_mutex);
	    return;
	  }
	}

	// make space for new
	pconx->ikey=(int *)g_realloc(pconx->ikey,totcons*sizint);
	pconx->imode=(int *)g_realloc(pconx->imode,totcons*sizint);
	pconx->ipnum=(int *)g_realloc(pconx->ipnum,totcons*sizint);
	pconx->autoscale=(int *)g_realloc(pconx->autoscale,totcons*sizint);

	// move up 1
	for (j=totcons-1;j>posn;j--) {
	  pconx->ikey[j]=pconx->ikey[j-1];
	  pconx->imode[j]=pconx->imode[j-1];
	  pconx->ipnum[j]=pconx->ipnum[j-1];
	  pconx->autoscale[j]=pconx->autoscale[j-1];
	}

	// insert at posn
	pconx->ikey[posn]=ikey;
	pconx->imode[posn]=imode;
	pconx->ipnum[posn]=ipnum;
	pconx->autoscale[posn]=autoscale;

	pthread_mutex_unlock(&mainw->data_mutex);

	return;
      }
      
    }

    // so, we have data for key/mode but this is a new param to be mapped
    
    for (i=0;i<pconx->nparams;i++) {
      totcons+=pconx->nconns[i];
    }

    totcons++;
    
    pconx->nparams++;
    posn=pconx->nparams;
    
    // make space for new
    pconx->nconns=(int *)g_realloc(pconx->nconns,posn*sizint);
    pconx->params=(int *)g_realloc(pconx->params,posn*sizint);

    pconx->ikey=(int *)g_realloc(pconx->ikey,totcons*sizint);
    pconx->imode=(int *)g_realloc(pconx->imode,totcons*sizint);
    pconx->ipnum=(int *)g_realloc(pconx->ipnum,totcons*sizint);
    pconx->autoscale=(int *)g_realloc(pconx->autoscale,totcons*sizint);
    
    pconx->params[posn-1]=opnum;

    pconx->nconns[posn-1]=1;
    
    posn=totcons-1;
    
    // insert at posn
    pconx->ikey[posn]=ikey;
    pconx->imode[posn]=imode;
    pconx->ipnum[posn]=ipnum;
    pconx->autoscale[posn]=autoscale;

#ifdef DEBUG_PCONX
    g_print("added another pconx from %d %d %d to %d %d %d\n",okey,omode,opnum,ikey,imode,ipnum);
#endif

    pthread_mutex_unlock(&mainw->data_mutex);

    return;

  }

  // add new

  totcons=pconx_get_numcons(pconx)+1;
  pconx->nparams++;

  pconx->nconns=(int *)g_realloc(pconx->params,pconx->nparams*sizint);
  pconx->nconns[pconx->nparams-1]=1;

  pconx->params=(int *)g_realloc(pconx->params,pconx->nparams*sizint);
  pconx->params[pconx->nparams-1]=opnum;

  pconx->ikey=(int *)g_realloc(pconx->ikey,totcons*sizint);
  pconx->ikey[totcons-1]=ikey;

  pconx->imode=(int *)g_realloc(pconx->imode,totcons*sizint);
  pconx->imode[totcons-1]=imode;

  pconx->ipnum=(int *)g_realloc(pconx->ipnum,totcons*sizint);
  pconx->ipnum[totcons-1]=ipnum;

  pconx->autoscale=(int *)g_realloc(pconx->autoscale,totcons*sizint);
  pconx->autoscale[totcons-1]=autoscale;

#ifdef DEBUG_PCONX
  g_print("added new pconx from %d %d %d to %d %d %d\n",okey,omode,opnum,ikey,imode,ipnum);
#endif

  pthread_mutex_unlock(&mainw->data_mutex);

}



weed_plant_t *pconx_get_out_param(int ikey, int imode, int ipnum, int *autoscale) {
  // walk all pconx and find one which has ikey/imode/ipnum as destination
  // then all we need do is copy the "value" leaf

  lives_pconnect_t *pconx=mainw->pconx;

  weed_plant_t *inst;

  int totcons,error;
  register int i,j;

  while (pconx!=NULL) {
    if (mainw->is_rendering) {
      inst=get_new_inst_for_keymode(pconx->okey,pconx->omode);
    }
    else {
      inst=rte_keymode_get_instance(pconx->okey+1,pconx->omode);
    }
    if (inst==NULL) {
      pconx=pconx->next;
      continue;
    }
    if (!weed_plant_has_leaf(inst,"out_parameters")) {
      pconx=pconx->next;
      continue;
    }
    totcons=0;
    j=0;
    for (i=0;i<pconx->nparams;i++) {
      totcons+=pconx->nconns[i];
      for (;j<totcons;j++) {
	if (pconx->ikey[j]==ikey && pconx->imode[j]==imode && pconx->ipnum[j]==ipnum) {
	  weed_plant_t **outparams=weed_get_plantptr_array(inst,"out_parameters",&error);
	  weed_plant_t *param=NULL;
	  if (i<weed_leaf_num_elements(inst,"out_parameters")) {
	    param=outparams[pconx->params[i]];
	    if (autoscale!=NULL) *autoscale=pconx->autoscale[j];
	  }
	  weed_free(outparams);
	  return param;
	}
      }
    }
    pconx=pconx->next;
  }

  return NULL;
}



boolean weed_leaves_differ(weed_plant_t *p1, const char *key1, weed_plant_t *p2, const char *key2) {




  return TRUE;
}



boolean pconx_convert_value_data(weed_plant_t *inst, int pnum, weed_plant_t *dparam, weed_plant_t *sparam, boolean autoscale) {
  // try to convert values of various type, if we succeed, copy the "value" and return TRUE (if changed)
  weed_plant_t *dptmpl;

  int dtype,stype,nsvals,ndvals,error,dflags;
  int copyto,ondvals;

  boolean retval=FALSE;

  register int i;

  // TODO *** - handle autoscale

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


  // TODO var elems with nsvals > ndvals
  // TODO autoscale
  // TODO any -> color
  // TODO string -> string
  // TODO - max/min with > 1 elems



  nsvals=weed_leaf_num_elements(sparam,"value");
  ondvals=ndvals=weed_leaf_num_elements(dparam,"value");
  
  dptmpl=weed_get_plantptr_value(dparam,"template",&error);
  //sptmpl=weed_get_plantptr_value(sparam,"template",&error);

  //  sflags=weed_get_int_value(sptmpl,"flags",&error);
  dflags=weed_get_int_value(dptmpl,"flags",&error);

  dtype=weed_leaf_seed_type(dparam,"value");
  stype=weed_leaf_seed_type(sparam,"value");

  if (ndvals>nsvals) {
    if (!((dflags&WEED_PARAMETER_VARIABLE_ELEMENTS)&&!(dflags&WEED_PARAMETER_ELEMENT_PER_CHANNEL))) return FALSE;
    // TODO !!!
  }

  switch (stype) {
  case WEED_SEED_DOUBLE:
    switch (dtype) {
    case WEED_SEED_DOUBLE:
      {
	double *valsD=weed_get_double_array(sparam,"value",&error);
	double *valsd=weed_get_double_array(dparam,"value",&error);
	
	double maxd=weed_get_double_value(dptmpl,"max",&error);
	double mind=weed_get_double_value(dptmpl,"min",&error);

	for (i=0;i<ndvals;i++) {
	  if (valsd[i]!=valsD[i]) {
	    retval=TRUE;
	    valsd[i]=valsD[i];
	    if (valsd[i]>maxd) valsd[i]=maxd;
	    if (valsd[i]<mind) valsd[i]=mind;
	  }
	}
	if (retval) {

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_double_array(dparam,"value",ndvals,valsd);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(valsD);
	weed_free(valsd);
      }
      return retval;

    case WEED_SEED_STRING:
      {
	char *opstring,*tmp,*bit;
	double *valsd=weed_get_double_array(sparam,"value",&error);
	char **valss,*vals;

	if (ndvals==1) {
	  opstring=g_strdup("");
	  vals=weed_get_string_value(dparam,"value",&error);
	  for (i=0;i<nsvals;i++) {
	    bit=g_strdup_printf("%.4f",valsd[i]);
	    if (strlen(opstring)==0)
	      tmp=g_strconcat (opstring,bit,NULL);
	    else 
	      tmp=g_strconcat (opstring," ",bit,NULL);
	    g_free(bit);
	    g_free(opstring);
	    opstring=tmp;
	  }
	  if (strcmp(vals,opstring)) {
	    pthread_mutex_lock(&mainw->data_mutex);
	    weed_set_string_value(dparam,"value",opstring);
	    pthread_mutex_unlock(&mainw->data_mutex);
	    retval=TRUE;
	  }
	  weed_free(vals);
	  weed_free(valsd);
	  g_free(opstring);
	  return retval;
	}

	valss=weed_get_string_array(dparam,"value",&error);
	for (i=0;i<ndvals;i++) {
	  bit=g_strdup_printf("%.4f",valsd[i]);
	  if (strcmp(valss[i],bit)) {
	    retval=TRUE;
	    weed_free(valss[i]);
	    valss[i]=bit;
	  }
	  else g_free(bit);
	}
	if (!retval) {
	  for (i=0;i<ndvals;i++) weed_free(valss[i]);
	  weed_free(valss);
	  weed_free(valsd);
	  return FALSE;
	}

	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_string_array(dparam,"value",ndvals,valss);
	pthread_mutex_unlock(&mainw->data_mutex);

	for (i=0;i<ndvals;i++) weed_free(valss[i]);
	weed_free(valss);
	weed_free(valsd);
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
	char *opstring,*tmp,*bit;
	int *valsi=weed_get_int_array(sparam,"value",&error);

	char **valss,*vals;

	if (ndvals==1) {
	  opstring=g_strdup("");
	  vals=weed_get_string_value(dparam,"value",&error);
	  for (i=0;i<nsvals;i++) {
	    bit=g_strdup_printf("%d",valsi[i]);
	    if (strlen(opstring)==0)
	      tmp=g_strconcat (opstring,bit,NULL);
	    else 
	      tmp=g_strconcat (opstring," ",bit,NULL);
	    g_free(bit);
	    g_free(opstring);
	    opstring=tmp;
	  }
	  if (strcmp(vals,opstring)) {
	    pthread_mutex_lock(&mainw->data_mutex);
	    weed_set_string_value(dparam,"value",opstring);
	    pthread_mutex_unlock(&mainw->data_mutex);
	    retval=TRUE;
	  }
	  weed_free(vals);
	  weed_free(valsi);
	  g_free(opstring);
	  return retval;
	}

	valss=weed_get_string_array(dparam,"value",&error);
	for (i=0;i<ndvals;i++) {
	  bit=g_strdup_printf("%d",valsi[i]);
	  if (strcmp(valss[i],bit)) {
	    retval=TRUE;
	    weed_free(valss[i]);
	    valss[i]=bit;
	  }
	  else g_free(bit);
	}
	if (!retval) {
	  for (i=0;i<ndvals;i++) weed_free(valss[i]);
	  weed_free(valss);
	  weed_free(valsi);
	  return FALSE;
	}

	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_string_array(dparam,"value",ndvals,valss);
	pthread_mutex_unlock(&mainw->data_mutex);

	for (i=0;i<ndvals;i++) weed_free(valss[i]);
	weed_free(valss);
	weed_free(valsi);
      }
      return retval;
    case WEED_SEED_DOUBLE:
      {
	int *valsi=weed_get_int_array(sparam,"value",&error);
	double * valsd=weed_get_double_array(dparam,"value",&error);
	
	double maxd=weed_get_double_value(dptmpl,"max",&error);
	double mind=weed_get_double_value(dptmpl,"min",&error);

	for (i=0;i<ndvals;i++) {
	  if (valsd[i]!=(double)valsi[i]) {
	    retval=TRUE;
	    valsd[i]=(double)valsi[i];
	    if (valsd[i]>maxd) valsd[i]=maxd;
	    if (valsd[i]<mind) valsd[i]=mind;
	  }
	}
	if (retval) {

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_double_array(dparam,"value",ndvals,valsd);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(valsi);
	weed_free(valsd);
      }
      return retval;

   case WEED_SEED_INT:
      {
	int *valsI=weed_get_int_array(sparam,"value",&error);
	int *valsi=weed_get_int_array(dparam,"value",&error);
	
	int maxi=weed_get_int_value(dptmpl,"max",&error);
	int mini=weed_get_int_value(dptmpl,"min",&error);

	for (i=0;i<ndvals;i++) {
	  if (valsi[i]!=valsI[i]) {
	    retval=TRUE;
	    valsi[i]=valsI[i];
	    if (valsi[i]>maxi) valsi[i]=maxi;
	    if (valsi[i]<mini) valsi[i]=mini;
	  }
	}
	if (retval) {

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_int_array(dparam,"value",ndvals,valsi);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(valsI);
	weed_free(valsi);
      }
      return retval;

    }
    break;

  case WEED_SEED_BOOLEAN:
    switch (dtype) {
    case WEED_SEED_STRING:
      {
	char *opstring,*tmp,*bit;
	int *valsb=weed_get_boolean_array(sparam,"value",&error);

	char **valss,*vals;

	if (ndvals==1) {
	  opstring=g_strdup("");
	  vals=weed_get_string_value(dparam,"value",&error);
	  for (i=0;i<nsvals;i++) {
	    bit=g_strdup_printf("%d",valsb[i]);
	    if (strlen(opstring)==0)
	      tmp=g_strconcat (opstring,bit,NULL);
	    else 
	      tmp=g_strconcat (opstring," ",bit,NULL);
	    g_free(bit);
	    g_free(opstring);
	    opstring=tmp;
	  }
	  if (strcmp(vals,opstring)) {
	    pthread_mutex_lock(&mainw->data_mutex);
	    weed_set_string_value(dparam,"value",opstring);
	    pthread_mutex_unlock(&mainw->data_mutex);
	    retval=TRUE;
	  }
	  weed_free(vals);
	  weed_free(valsb);
	  g_free(opstring);
	  return retval;
	}

	valss=weed_get_string_array(dparam,"value",&error);
	for (i=0;i<ndvals;i++) {
	  bit=g_strdup_printf("%d",valsb[i]);
	  if (strcmp(valss[i],bit)) {
	    retval=TRUE;
	    weed_free(valss[i]);
	    valss[i]=bit;
	  }
	  else g_free(bit);
	}
	if (!retval) {
	  for (i=0;i<ndvals;i++) weed_free(valss[i]);
	  weed_free(valss);
	  weed_free(valsb);
	  return FALSE;
	}

	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_string_array(dparam,"value",ndvals,valss);
	pthread_mutex_unlock(&mainw->data_mutex);

	for (i=0;i<ndvals;i++) weed_free(valss[i]);
	weed_free(valss);
	weed_free(valsb);
      }
      return retval;
    case WEED_SEED_DOUBLE:
      {
	int *valsb=weed_get_boolean_array(sparam,"value",&error);
	double * valsd=weed_get_double_array(dparam,"value",&error);
	
	double maxd=weed_get_double_value(dptmpl,"max",&error);
	double mind=weed_get_double_value(dptmpl,"min",&error);

	for (i=0;i<ndvals;i++) {
	  if (valsd[i]!=(double)valsb[i]) {
	    retval=TRUE;
	    valsd[i]=(double)valsb[i];
	    if (valsd[i]>maxd) valsd[i]=maxd;
	    if (valsd[i]<mind) valsd[i]=mind;
	  }
	}
	if (retval) {

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_double_array(dparam,"value",ndvals,valsd);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(valsb);
	weed_free(valsd);
      }
      return retval;
    case WEED_SEED_INT:
      {
	int *valsb=weed_get_boolean_array(sparam,"value",&error);
	int *valsi=weed_get_int_array(dparam,"value",&error);

	int maxi=weed_get_int_value(dptmpl,"max",&error);
	int mini=weed_get_int_value(dptmpl,"min",&error);
	
	for (i=0;i<ndvals;i++) {
	  if (valsi[i]!=valsb[i]) {
	    retval=TRUE;
	    valsi[i]=valsb[i];
	    if (valsi[i]>maxi) valsi[i]=maxi;
	    if (valsi[i]<mini) valsi[i]=mini;
	  }
	}
	if (retval) {

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_int_array(dparam,"value",ndvals,valsi);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(valsi);
	weed_free(valsb);
      }
      return retval;

    case WEED_SEED_BOOLEAN:
      {
	int *valsB=weed_get_boolean_array(sparam,"value",&error);
	int *valsb=weed_get_boolean_array(dparam,"value",&error);

	for (i=0;i<ndvals;i++) {
	  if (valsb[i]!=valsB[i]) {
	    retval=TRUE;
	    valsb[i]=valsB[i];
	  }
	}
	if (retval) {

	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_boolean_array(dparam,"value",ndvals,valsb);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(valsB);
	weed_free(valsb);
      }
      return retval;
    default:
      break;
    }

    break;

  default:
    break;
    }


  if ((dflags&WEED_PARAMETER_VARIABLE_ELEMENTS)&&!(dflags&WEED_PARAMETER_ELEMENT_PER_CHANNEL)) ndvals=nsvals;

  // TODO !!
  if (dtype==stype && nsvals==ndvals) {
    // values of same type and number, -> simpĺe copy
    
    if (weed_leaves_differ(dparam,"value",sparam,"value")) {
 
      if (dtype==WEED_SEED_INT||dtype==WEED_SEED_DOUBLE) {
	// prevent interpolation during rendering
	if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	  // if we are recording, add this change to our event_list
	  rec_param_change(inst,pnum);
	  copyto=set_copy_to(inst,pnum,FALSE);
	  if (copyto!=-1) rec_param_change(inst,copyto);
	}
      }

      pthread_mutex_lock(&mainw->data_mutex);
      retval=weed_leaf_copy(dparam,"value",sparam,"value");
      pthread_mutex_unlock(&mainw->data_mutex);
    }
    return retval;
  }

  return retval;
}





void pconx_chain_data(int key, int mode) {
  int error;
  int nparams=0;
  int autoscale;
  weed_plant_t **inparams;
  weed_plant_t *oparam;
  weed_plant_t *inst;

  boolean changed;

  int copyto=-1;

  register int i;

  if (mainw->is_rendering) {
    if ((inst=get_new_inst_for_keymode(key,mode))==NULL) {
      return; ///< dest effect is not found
    }
  }
  else {
    if ((inst=rte_keymode_get_instance(key+1,mode))==NULL) {
      return; ///< dest effect is not enabled
    }
  }

  if (weed_plant_has_leaf(inst,"in_parameters")) nparams=weed_leaf_num_elements(inst,"in_parameters");

  if (nparams>0) {
    inparams=weed_get_plantptr_array(inst,"in_parameters",&error);

    for (i=0;i<nparams;i++) {
      if ((oparam=pconx_get_out_param(key,mode,i,&autoscale))!=NULL) {
	//#define DEBUG_PCONX
#ifdef DEBUG_PCONX
  g_print("got pconx to %d %d %d\n",key,mode,i);
#endif
  changed=pconx_convert_value_data(inst,i,inparams[i],oparam,autoscale);

	if (changed) {
	  // only store value if it changed; for int, double or colour, store old value too

	  copyto=set_copy_to(inst,i,TRUE);
	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,i);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }


	  if (fx_dialog[1]!=NULL) {
	    lives_rfx_t *rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(fx_dialog[1]),"rfx");
	    if (!rfx->is_template) {
	      gint keyw=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
	      gint modew=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
	      if (keyw==key&&modew==mode)
		// ask the main thread to update the param window
		mainw->vrfx_update=rfx;
	    }
	  }



	}

      }
    }
    weed_free(inparams);
  }
}











// alpha channs



void cconx_delete_all(void) {
  lives_cconnect_t *cconx=mainw->cconx,*cconx_next;
  while (cconx!=NULL) {
    cconx_next=cconx->next;
    if (cconx->nchans>0) {
      g_free(cconx->chans);
      g_free(cconx->nconns);
      g_free(cconx->ikey);
      g_free(cconx->imode);
      g_free(cconx->icnum);
    }
    g_free(cconx);
    cconx=cconx_next;
  }
  mainw->cconx=NULL;
}




void cconx_delete(int okey, int omode, int ocnum, int ikey, int imode, int icnum) {
  lives_cconnect_t *cconx=mainw->cconx,*cconx_next,*cconx_prev=NULL;

  register int i,j=0,k;

  int totcons=0,maxcons=0;

  while (cconx!=NULL) {
    cconx_next=cconx->next;
    if (okey==-1||(cconx->okey==okey&&cconx->omode==omode)) {
      if (ikey==-1) {
	// delete entire node
	g_free(cconx->chans);
	g_free(cconx->nconns);
	g_free(cconx->ikey);
	g_free(cconx->imode);
	g_free(cconx->icnum);
	g_free(cconx);
	if (mainw->cconx==cconx) mainw->cconx=cconx_next;
	else cconx_prev->next=cconx_next;
	return;
      }

      for (i=0;i<cconx->nchans;i++) {
	maxcons+=cconx->nconns[i];
      }

      for (i=0;i<cconx->nchans;i++) {
	totcons+=cconx->nconns[i];

	if (okey!=-1&&cconx->chans[i]!=ocnum) {
	  j+=totcons;
	  continue;
	}

	for (;j<totcons;j++) {
	  if (cconx->ikey[j]==ikey && cconx->imode[j]==imode && cconx->icnum[j]==icnum) {
	    maxcons--;
	    for (k=j;k<maxcons;k++) {
	      cconx->ikey[k]=cconx->ikey[k+1];
	      cconx->imode[k]=cconx->imode[k+1];
	      cconx->icnum[k]=cconx->icnum[k+1];
	    }

	    cconx->ikey=(int *)g_realloc(cconx->ikey,maxcons*sizint);
	    cconx->imode=(int *)g_realloc(cconx->imode,maxcons*sizint);
	    cconx->icnum=(int *)g_realloc(cconx->icnum,maxcons*sizint);

	    cconx->nconns[i]--;

	    if (cconx->nconns[i]==0) {
	      cconx->nchans--;
	      for (k=i;k<cconx->nchans;k++) {
		cconx->nconns[k]=cconx->nconns[k+1];
	      }

	      if (cconx->nchans==0) {
		// delete entire node
		g_free(cconx->chans);
		g_free(cconx->nconns);
		g_free(cconx->ikey);
		g_free(cconx->imode);
		g_free(cconx->icnum);
		g_free(cconx);
		if (mainw->cconx==cconx) {
		  mainw->cconx=cconx_next;
		  cconx=NULL;
		}
		else {
		  cconx=cconx_prev;
		  cconx->next=cconx_next;
		}
	      }
	      else {
		cconx->nconns=(int *)g_realloc(cconx->nconns,cconx->nchans*sizint);
	      }
	    }
	  }
	}
	j+=totcons;
      }
    }
    cconx_prev=cconx;
    cconx=cconx_next;
  }
}


void cconx_remap_mode(int key, int omode, int nmode) {
  lives_cconnect_t *cconx=mainw->cconx;

  register int i,j,totcons;

  while (cconx!=NULL) {
    if (cconx->okey==key&&cconx->omode==omode) {
      cconx->omode=nmode;
    }
    j=0;
    totcons=0;
    for (i=0;i<cconx->nchans;i++) {
      totcons+=cconx->nconns[i];
      for (;j<totcons;j++) {
	if (cconx->ikey[j]==key && cconx->imode[j]==omode) {
	  cconx->imode[j]=nmode;
	}
      }
    }
    cconx=cconx->next;
  }
}



static lives_cconnect_t *cconx_new (int okey, int omode) {
  lives_cconnect_t *cconx=(lives_cconnect_t *)g_malloc0(sizeof(struct _lives_cconnect_t));
  cconx->next=NULL;
  cconx->okey=okey;
  cconx->omode=omode;
  cconx->nchans=0;
  cconx->nconns=NULL;
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


static lives_cconnect_t *cconx_find (int okey, int omode) {
  lives_cconnect_t *cconx=mainw->cconx;
  while (cconx!=NULL) {
    if (cconx->okey==okey&&cconx->omode==omode) {
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



void cconx_add_connection(int okey, int omode, int ocnum, int ikey, int imode, int icnum) {
  lives_cconnect_t *cconx=cconx_find(okey,omode);
  int posn=0,totcons=0;
  register int i,j;

  if (cconx==NULL) {
    // add whole new node
    cconx=cconx_new(okey,omode);
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

	// if already there, do not add again
	for (j=posn;j<totcons;j++) {
	  if (cconx->ikey[j]==ikey&&cconx->imode[j]==imode&&cconx->icnum[j]==icnum) {
	    return;
	  }
	}

	// make space for new
	cconx->ikey=(int *)g_realloc(cconx->ikey,totcons*sizint);
	cconx->imode=(int *)g_realloc(cconx->imode,totcons*sizint);
	cconx->icnum=(int *)g_realloc(cconx->icnum,totcons*sizint);

	// move up 1
	for (j=totcons-1;j>posn;j--) {
	  cconx->ikey[j]=cconx->ikey[j-1];
	  cconx->imode[j]=cconx->imode[j-1];
	  cconx->icnum[j]=cconx->icnum[j-1];
	}

	// insert at posn
	cconx->ikey[posn]=ikey;
	cconx->imode[posn]=imode;
	cconx->icnum[posn]=icnum;

	return;
      }
      
    }

    // so, we have data for key/mode but this is a new channel to be mapped

    for (i=0;i<cconx->nchans;i++) {
      totcons+=cconx->nconns[i];
    }
    
    totcons++;
    
    cconx->nchans++;
    posn=cconx->nchans;
    
    // make space for new
    cconx->nconns=(int *)g_realloc(cconx->nconns,posn*sizint);
    cconx->chans=(int *)g_realloc(cconx->chans,posn*sizint);

    cconx->ikey=(int *)g_realloc(cconx->ikey,totcons*sizint);
    cconx->imode=(int *)g_realloc(cconx->imode,totcons*sizint);
    cconx->icnum=(int *)g_realloc(cconx->icnum,totcons*sizint);
    
    cconx->chans[posn-1]=ocnum;

    cconx->nconns[posn-1]=1;
    
    posn=totcons-1;
    
    // insert at posn
    cconx->ikey[posn]=ikey;
    cconx->imode[posn]=imode;
    cconx->icnum[posn]=icnum;

#ifdef DEBUG_PCONX
  g_print("added another cconx from %d %d %d to %d %d %d\n",okey,omode,ocnum,ikey,imode,icnum);
#endif

    return;
  }

  // add new

  totcons=cconx_get_numcons(cconx)+1;
  cconx->nchans++;

  cconx->nconns=(int *)g_realloc(cconx->chans,cconx->nchans*sizint);
  cconx->nconns[cconx->nchans-1]=1;

  cconx->chans=(int *)g_realloc(cconx->chans,cconx->nchans*sizint);
  cconx->chans[cconx->nchans-1]=ocnum;

  cconx->ikey=(int *)g_realloc(cconx->ikey,totcons*sizint);
  cconx->ikey[totcons-1]=ikey;

  cconx->imode=(int *)g_realloc(cconx->imode,totcons*sizint);
  cconx->imode[totcons-1]=imode;

  cconx->icnum=(int *)g_realloc(cconx->icnum,totcons*sizint);
  cconx->icnum[totcons-1]=icnum;

#ifdef DEBUG_PCONX
  g_print("added new cconx from %d %d %d to %d %d %d\n",okey,omode,ocnum,ikey,imode,icnum);
#endif

}



weed_plant_t *cconx_get_out_alpha(int ikey, int imode, int icnum) {
  // walk all cconx and find one which has ikey/imode/icnum as destination
  // then all we need do is convert the pixel_data

  lives_cconnect_t *cconx=mainw->cconx;

  weed_plant_t *inst;

  int totcons,error;
  register int i,j;

  while (cconx!=NULL) {
    if (mainw->is_rendering) {
      inst=get_new_inst_for_keymode(cconx->okey,cconx->omode);
    }
    else {
      inst=rte_keymode_get_instance(cconx->okey+1,cconx->omode);
    }
    if (inst==NULL) {
      cconx=cconx->next;
      continue;
    }
    if (!weed_plant_has_leaf(inst,"out_channels")) {
      cconx=cconx->next;
      continue;
    }
    totcons=0;
    j=0;
    for (i=0;i<cconx->nchans;i++) {
      totcons+=cconx->nconns[i];
      for (;j<totcons;j++) {
	if (cconx->ikey[j]==ikey && cconx->imode[j]==imode && cconx->icnum[j]==icnum) {
	  weed_plant_t **outchans=weed_get_plantptr_array(inst,"out_channels",&error);
	  weed_plant_t *channel=NULL;
	  if (cconx->chans[i]<weed_leaf_num_elements(inst,"out_channels")) {
	    channel=outchans[cconx->chans[i]];
	  }
	  weed_free(outchans);
	  return channel;
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
  orow=weed_get_int_value(dchan,"rowstrides",&error);

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

  dpdata=weed_get_voidptr_value(dchan,"pixel_data",&error);

  if (dpdata!=NULL) {
    g_free(dpdata);
    dpdata=NULL;
  }

  weed_set_int_value(dchan,"width",iwidth);
  weed_set_int_value(dchan,"height",iheight);
  weed_set_int_value(dchan,"current_palette",ipal);

  if (pal_ok) {
    weed_set_voidptr_value(dchan,"pixel_data",spdata);
    weed_set_int_value(dchan,"rowstrides",irow);
    
    /// caller - do not free in dchan
    weed_set_boolean_value(dchan,"host_orig_pdata",WEED_TRUE);
    return FALSE;
  }
  create_empty_pixel_data(dchan,FALSE,TRUE);
  dpdata=weed_get_voidptr_value(dchan,"pixel_data",&error);
  
  orow=weed_get_int_value(dchan,"rowstrides",&error);

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

  convert_layer_palette(dchan,opal,0);

  if (needs_reinit) return TRUE;

  return FALSE;
}




boolean cconx_chain_data(int key, int mode) {
  // ret TRUE if we should reinit inst (because of palette change)

  weed_plant_t *ichan,*ochan;
  weed_plant_t *inst;

  gboolean needs_reinit=FALSE;

  register int i=0;

  if (mainw->is_rendering) {
    if ((inst=get_new_inst_for_keymode(key,mode))==NULL) {
      return FALSE; ///< dest effect is not found
    }
  }
  else {
    if ((inst=rte_keymode_get_instance(key+1,mode))==NULL) {
      return FALSE; ///< dest effect is not enabled
    }
  }

  while ((ichan=get_enabled_channel(inst,i,TRUE))!=NULL) {
    if ((ochan=cconx_get_out_alpha(key,mode,i++))!=NULL) {
      if (cconx_convert_pixel_data(ichan,ochan)) needs_reinit=TRUE;
    }
  }
  return needs_reinit;
}
 
