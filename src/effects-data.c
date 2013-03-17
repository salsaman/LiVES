// effects-data.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2013 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// functions for chaining and data passing between fx plugins


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

#include "main.h"
#include "effects.h"
#include "support.h"




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


static lives_pconnect_t *pconx_new (int okey, int omode) {
  lives_pconnect_t *pconx=(lives_pconnect_t *)g_malloc0(sizeof(struct _lives_pconnect_t));
  pconx->next=NULL;
  pconx->okey=okey;
  pconx->omode=omode;
  pconx->nparams=0;
  pconx->nconns=NULL;
  return pconx;
}


static lives_pconnect_t *pconx_copy(lives_pconnect_t *spconx) {
  lives_pconnect_t *pconx=NULL,*dpconx,*last_dpconx;

  int totcons=0;

  register int i,j=0;

  while (spconx!=NULL) {
    dpconx=pconx_new(spconx->okey,spconx->omode);
    if (pconx==NULL) pconx=dpconx;
    else last_dpconx->next=dpconx;

    dpconx->nparams=spconx->nparams;

    dpconx->nconns=(int *)g_malloc(dpconx->nparams*sizint);
    dpconx->params=(int *)g_malloc(dpconx->nparams*sizint);

    dpconx->ikey=dpconx->imode=dpconx->ipnum=dpconx->autoscale=NULL;

    j=0;

    for (i=0;i<dpconx->nparams;i++) {
      dpconx->params[i]=spconx->params[i];
      dpconx->nconns[i]=spconx->nconns[i];
      totcons+=dpconx->nconns[i];

      dpconx->ikey=(int *)g_realloc(dpconx->ikey,totcons*sizint);
      dpconx->imode=(int *)g_realloc(dpconx->imode,totcons*sizint);
      dpconx->ipnum=(int *)g_realloc(dpconx->ipnum,totcons*sizint);
      dpconx->autoscale=(int *)g_realloc(dpconx->autoscale,totcons*sizint);

      while (j<totcons) {
	dpconx->ikey[j]=spconx->ikey[j];
	dpconx->imode[j]=spconx->imode[j];
	dpconx->ipnum[j]=spconx->ipnum[j];
	dpconx->autoscale[j]=spconx->autoscale[j];
	j++;
      }
    }

    spconx=spconx->next;
    last_dpconx=dpconx;
  }

  return pconx;
}



gchar *pconx_list(int okey, int omode, int opnum) {
  gchar *st1=g_strdup(""),*st2;
  lives_pconnect_t *pconx=mainw->pconx;

  int totcons=0;

  register int i,j;

  while (pconx!=NULL) {
    if (pconx->okey==okey&&pconx->omode==omode) {
      for (i=0;i<pconx->nparams;i++) {
	if (pconx->params[i]==opnum) {
	  for (j=totcons;j<totcons+pconx->nconns[i];j++) {
	    if (strlen(st1)==0) st2=g_strdup_printf("%d %d %d %d",pconx->ikey[j]+1,pconx->imode[j]+1,pconx->ipnum[j],pconx->autoscale[j]);
	    st2=g_strdup_printf("%s %d %d %d %d",st1,pconx->ikey[j]+1,pconx->imode[j]+1,pconx->ipnum[j],pconx->autoscale[j]);
	    g_free(st1);
	    st1=st2;
	  }
	  return st1;
	}
	totcons+=pconx->nconns[i];
      }
      return st1;
    }
    pconx=pconx->next;
  }
  return st1;
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
	//g_print("rem all cons from %d %d to any param\n",okey,omode); 

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

      for (i=0;pconx!=NULL&&i<pconx->nparams;i++) {
	totcons+=pconx->nconns[i];

	if (okey!=-1&&pconx->params[i]!=opnum) {
	  j+=totcons;
	  continue;
	}

	for (;j<totcons;j++) {
	  if (pconx->ikey[j]==ikey && pconx->imode[j]==imode && (ipnum==-1||pconx->ipnum[j]==ipnum)) {
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
		pconx->params[k]=pconx->params[k+1];
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

  // delete any existing connection to the input param
  pconx_delete(-1,0,0,ikey,imode,ipnum);

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

	for (j=0;j<pconx->nparams;j++) {
	  if (j<i) {
	    // calc posn
	    posn+=pconx->nconns[j];
	  }
	  totcons+=pconx->nconns[j];
	}

	// if already there, do not add again, just update autoscale
	for (j=posn;j<posn+pconx->nconns[i];j++) {
	  if (pconx->ikey[j]==ikey&&pconx->imode[j]==imode&&pconx->ipnum[j]==ipnum) {
	    pconx->autoscale[j]=autoscale;
	    pthread_mutex_unlock(&mainw->data_mutex);
	    return;
	  }

	  // add in order key/mode/chan
	  if (pconx->ikey[j]>ikey||(pconx->ikey[j]==ikey&&pconx->imode[j]>imode)||
	      (pconx->ikey[j]==ikey&&pconx->imode[j]==imode&&pconx->ipnum[j]>ipnum)) break;

	}

	// increment nconns for this param
	pconx->nconns[i]++;

	totcons++;

	posn=j;

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
  g_print("added new pconx from %d %d %d to %d %d %d (%d)\n",okey,omode,opnum,ikey,imode,ipnum,autoscale);
#endif

  pthread_mutex_unlock(&mainw->data_mutex);

}



weed_plant_t *pconx_get_out_param(boolean use_filt, int ikey, int imode, int ipnum, int *autoscale) {
  // walk all pconx and find one which has ikey/imode/ipnum as destination
  // then all we need do is copy the "value" leaf

  lives_pconnect_t *pconx=mainw->pconx;

  weed_plant_t *inst=NULL,*filter=NULL;

  int totcons,error;
  register int i,j;

  while (pconx!=NULL) {
    if (!use_filt) {
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
      filter=weed_instance_get_filter(inst,TRUE);
    }
    else {
      filter=rte_keymode_get_filter(pconx->okey+1,pconx->omode);
      if (filter==NULL) {
	pconx=pconx->next;
	continue;
      }
    }
    if (!weed_plant_has_leaf(filter,"out_parameter_templates")) {
      pconx=pconx->next;
      continue;
    }
    totcons=0;
    j=0;
    for (i=0;i<pconx->nparams;i++) {
      totcons+=pconx->nconns[i];
      for (;j<totcons;j++) {
	if (pconx->ikey[j]==ikey && pconx->imode[j]==imode && pconx->ipnum[j]==ipnum) {
	  weed_plant_t *param=NULL;
	  if (use_filt) {
	    weed_plant_t **outparams=weed_get_plantptr_array(filter,"out_parameter_templates",&error);
	    if (pconx->params[i]<weed_leaf_num_elements(filter,"out_parameter_templates")) {
	      param=outparams[pconx->params[i]];
	    }
	    weed_free(outparams);
	  }
	  else {
	    param=weed_inst_out_param(inst,pconx->params[i]);
	  }
	  if (autoscale!=NULL) *autoscale=pconx->autoscale[j];
	  return param;
	}
      }
    }
    pconx=pconx->next;
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

  int error;

  weed_plant_t *dptmpl;

  int dtype;

  int stype;

  int ndvals;
  int nsvals;

  int dhint;
  int dflags;

  if (WEED_PLANT_IS_PARAMETER(dparam)) {
    dptmpl=weed_get_plantptr_value(dparam,"template",&error);
    dtype=weed_leaf_seed_type(dparam,"value");
    ndvals=weed_leaf_num_elements(dparam,"value");
  }
  else {
    dptmpl=dparam;
    dtype=weed_leaf_seed_type(dparam,"default");
    ndvals=weed_leaf_num_elements(dparam,"default");
  }

  if (WEED_PLANT_IS_PARAMETER(sparam)) {
    stype=weed_leaf_seed_type(sparam,"value");
    nsvals=weed_leaf_num_elements(sparam,"value");
  }
  else {
    stype=weed_leaf_seed_type(sparam,"default");
    nsvals=weed_leaf_num_elements(sparam,"default");
  }

  dhint=weed_get_int_value(dptmpl,"hint",&error);
  dflags=weed_get_int_value(dptmpl,"flags",&error);


  if (dhint==WEED_HINT_COLOR) {
    int cspace=weed_get_int_value(dptmpl,"colorspace",&error);
    if (cspace==WEED_COLORSPACE_RGB&&(nsvals%3!=0)) return FALSE;
    if (nsvals%4!=0) return FALSE;
  }

  if (ndvals>nsvals) {
    if (!((dflags&WEED_PARAMETER_VARIABLE_ELEMENTS)&&!(dflags&WEED_PARAMETER_ELEMENT_PER_CHANNEL))) return FALSE;
  }

  if (dtype==stype) return TRUE;

  switch (stype) {
  case WEED_SEED_DOUBLE:
    if (dtype==WEED_SEED_STRING) return TRUE;
    return FALSE;
  case WEED_SEED_INT:
    if (dtype==WEED_SEED_DOUBLE||dtype==WEED_SEED_STRING) return TRUE;
    return FALSE;
  case WEED_SEED_BOOLEAN:
    if (dtype==WEED_SEED_DOUBLE||dtype==WEED_SEED_INT||dtype==WEED_SEED_STRING) return TRUE;
    return FALSE;
  default:
    return FALSE;
  }

  return FALSE;
}



boolean pconx_convert_value_data(weed_plant_t *inst, int pnum, weed_plant_t *dparam, weed_plant_t *sparam, boolean autoscale) {
  // try to convert values of various type, if we succeed, copy the "value" and return TRUE (if changed)
  weed_plant_t *dptmpl,*sptmpl;

  double ratio;

  int dtype,stype,nsvals,ndvals,error;
  int copyto,ondvals;

  int nsmin=0,nsmax=0;

  int minct=0,maxct=0;
  int sminct=0,smaxct=0;

  int nmax=0,nmin=0;

  boolean retval=FALSE;

  register int i;

  if (dparam==sparam) return FALSE;

  nsvals=weed_leaf_num_elements(sparam,"value");
  ondvals=ndvals=weed_leaf_num_elements(dparam,"value");
  
  dptmpl=weed_get_plantptr_value(dparam,"template",&error);
  sptmpl=weed_get_plantptr_value(sparam,"template",&error);

  dtype=weed_leaf_seed_type(dparam,"value");
  stype=weed_leaf_seed_type(sparam,"value");

  if (!params_compatible(sparam,dparam)) return FALSE;

  if (ndvals>nsvals) ndvals=nsvals;
 
  if (autoscale) {
    if (weed_plant_has_leaf(sptmpl,"min")&&weed_plant_has_leaf(sptmpl,"max")) {
      nsmin=weed_leaf_num_elements(sptmpl,"min");
      nsmax=weed_leaf_num_elements(sptmpl,"max");
    }
    else autoscale=FALSE;
  }

  if (weed_plant_has_leaf(dptmpl,"max")) {
    nmax=weed_leaf_num_elements(dptmpl,"max");
    nmin=weed_leaf_num_elements(dptmpl,"min");
  }

  //g_print("got %d and %d\n",stype,dtype);

  switch (stype) {
  case WEED_SEED_STRING:
    switch (dtype) {
    case WEED_SEED_STRING:
      {
	char **valsS=weed_get_string_array(sparam,"value",&error);
	char **valss=weed_get_string_array(dparam,"value",&error);

	if (ndvals>ondvals) valss=(char **)g_realloc(valss,ndvals*sizeof(char *));

	for (i=0;i<ndvals;i++) {
	  if (i>=ondvals||strcmp(valss[i],valsS[i])) {
	    retval=TRUE;
	    if (i<ondvals) weed_free(valss[i]);
	    valss[i]=valsS[i];
	  }
	  else weed_free(valsS[i]);
	}
	if (!retval) {
	  for (i=0;i<ndvals;i++) weed_free(valss[i]);
	  weed_free(valss);
	  weed_free(valsS);
	  return FALSE;
	}

	pthread_mutex_lock(&mainw->data_mutex);
	weed_set_string_array(dparam,"value",ndvals,valss);
	pthread_mutex_unlock(&mainw->data_mutex);

	for (i=0;i<ndvals;i++) weed_free(valss[i]);
	weed_free(valss);
	weed_free(valsS);
      }
      return TRUE;
  default:
    return retval;
    }
  case WEED_SEED_DOUBLE:
    switch (dtype) {
    case WEED_SEED_DOUBLE:
      {
	double *valsD=weed_get_double_array(sparam,"value",&error);
	double *valsd=weed_get_double_array(dparam,"value",&error);
	
	double *maxd=weed_get_double_array(dptmpl,"max",&error);
	double *mind=weed_get_double_array(dptmpl,"min",&error);

	double *mins=NULL,*maxs=NULL;

	if (autoscale) {
	  mins=weed_get_double_array(sptmpl,"min",&error);
	  maxs=weed_get_double_array(sptmpl,"max",&error);
	}

	if (ndvals>ondvals) valsd=(double *)g_realloc(valsd,ndvals*sizeof(double));

	for (i=0;i<ndvals;i++) {
	  if (autoscale) {
	    ratio=(valsD[i]-mins[sminct])/(maxs[smaxct]-mins[sminct]);
	    valsD[i]=mind[minct]+(maxd[maxct]-mind[minct])*ratio;
	    if (++smaxct==nsmax) smaxct=0;
	    if (++sminct==nsmin) sminct=0;

	    if (valsD[i]>maxd[maxct]) valsD[i]=maxd[maxct];
	    if (valsD[i]<mind[minct]) valsD[i]=mind[minct];
	  }

	  if (i>=ondvals||valsd[i]!=valsD[i]) {
	    retval=TRUE;
	    valsd[i]=valsD[i];
	  }
	  if (++maxct==nmax) maxct=0;
	  if (++minct==nmin) minct=0;
	}

	if (mins!=NULL) {
	  weed_free(mins);
	  weed_free(maxs);
	}

	if (retval) {

	  if (inst!=NULL&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_double_array(dparam,"value",ndvals,valsd);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(maxd);
	weed_free(mind);
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

	if (ndvals>ondvals) valss=(char **)g_realloc(valsd,ndvals*sizeof(char *));

	for (i=0;i<ndvals;i++) {
	  bit=g_strdup_printf("%.4f",valsd[i]);
	  if (i>=ondvals||strcmp(valss[i],bit)) {
	    retval=TRUE;
	    if (i<ondvals) weed_free(valss[i]);
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

	if (ndvals>ondvals) valss=(char **)g_realloc(valss,ndvals*sizeof(char *));

	for (i=0;i<ndvals;i++) {
	  bit=g_strdup_printf("%d",valsi[i]);
	  if (i>=ondvals||strcmp(valss[i],bit)) {
	    retval=TRUE;
	    if (i<ondvals) weed_free(valss[i]);
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
	
	double *maxd=weed_get_double_array(dptmpl,"max",&error);
	double *mind=weed_get_double_array(dptmpl,"min",&error);
	double vald;

	int *mins=NULL,*maxs=NULL;

	if (autoscale) {
	  mins=weed_get_int_array(sptmpl,"min",&error);
	  maxs=weed_get_int_array(sptmpl,"max",&error);
	}

	if (ndvals>ondvals) valsd=(double *)g_realloc(valsd,ndvals*sizeof(double));

	for (i=0;i<ndvals;i++) {
	  if (autoscale) {
	    ratio=(double)(valsi[i]-mins[sminct])/(double)(maxs[smaxct]-mins[sminct]);
	    vald=mind[minct]+(maxd[maxct]-mind[minct])*ratio;
	    if (++smaxct==nsmax) smaxct=0;
	    if (++sminct==nsmin) sminct=0;

	    if (vald>maxd[maxct]) vald=maxd[maxct];
	    if (vald<mind[minct]) vald=mind[minct];
	  }
	  else vald=(double)valsi[i];

	  if (i>=ondvals||valsd[i]!=vald) {
	    retval=TRUE;
	    valsd[i]=vald;
	  }
	  if (++maxct==nmax) maxct=0;
	  if (++minct==nmin) minct=0;
	}

	if (mins!=NULL) {
	  weed_free(mins);
	  weed_free(maxs);
	}

	if (retval) {

	  if (inst!=NULL&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_double_array(dparam,"value",ndvals,valsd);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(maxd);
	weed_free(mind);
	weed_free(valsi);
	weed_free(valsd);
      }
      return retval;

   case WEED_SEED_INT:
      {
	int *valsI=weed_get_int_array(sparam,"value",&error);
	int *valsi=weed_get_int_array(dparam,"value",&error);
	
	int *maxi=weed_get_int_array(dptmpl,"max",&error);
	int *mini=weed_get_int_array(dptmpl,"min",&error);

	int *mins=NULL,*maxs=NULL;

	if (autoscale) {
	  mins=weed_get_int_array(sptmpl,"min",&error);
	  maxs=weed_get_int_array(sptmpl,"max",&error);
	}

	if (ndvals>ondvals) valsi=(int *)g_realloc(valsi,ndvals*sizeof(int));

	for (i=0;i<ndvals;i++) {
	  if (autoscale) {
	    ratio=(double)(valsI[i]-mins[sminct])/(double)(maxs[smaxct]-mins[sminct]);
	    valsI[i]=myround(mini[minct]+(double)(maxi[maxct]-mini[minct])*ratio);
	    if (++smaxct==nsmax) smaxct=0;
	    if (++sminct==nsmin) sminct=0;

	    if (valsI[i]>maxi[maxct]) valsI[i]=maxi[maxct];
	    if (valsI[i]<mini[minct]) valsI[i]=mini[minct];
	  }

	  if (i>=ondvals||valsi[i]!=valsI[i]) {
	    retval=TRUE;
	    valsi[i]=valsI[i];
	  }
	  if (++maxct==nmax) maxct=0;
	  if (++minct==nmin) minct=0;
	}

	if (mins!=NULL) {
	  weed_free(mins);
	  weed_free(maxs);
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
	weed_free(maxi);
	weed_free(mini);
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
	if (ndvals>ondvals) valss=(char **)g_realloc(valss,ndvals*sizeof(char *));

	for (i=0;i<ndvals;i++) {
	  bit=g_strdup_printf("%d",valsb[i]);
	  if (i>=ondvals||strcmp(valss[i],bit)) {
	    retval=TRUE;
	    if (i<ondvals) weed_free(valss[i]);
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
	
	double *maxd=weed_get_double_array(dptmpl,"max",&error);
	double *mind=weed_get_double_array(dptmpl,"min",&error);
	double vald;

	if (ndvals>ondvals) valsd=(double *)g_realloc(valsd,ndvals*sizeof(double));

	for (i=0;i<ndvals;i++) {
	  if (autoscale) {
	    if (valsb[i]==WEED_TRUE) vald=maxd[maxct];
	    else vald=mind[minct];
	  }
	  else {
	    vald=(double)valsb[i];
	    if (vald>maxd[maxct]) vald=maxd[maxct];
	    if (vald<mind[minct]) vald=mind[minct];
	  }
	  if (i>=ondvals||valsd[i]!=vald) {
	    retval=TRUE;
	    valsd[i]=vald;
	  }
	  if (++maxct==nmax) maxct=0;
	  if (++minct==nmin) minct=0;
	}
	if (retval) {

	  if (inst!=NULL&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_double_array(dparam,"value",ndvals,valsd);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(maxd);
	weed_free(mind);
	weed_free(valsb);
	weed_free(valsd);
      }
      return retval;
    case WEED_SEED_INT:
      {
	int *valsb=weed_get_boolean_array(sparam,"value",&error);
	int *valsi=weed_get_int_array(dparam,"value",&error);

	int *maxi=weed_get_int_array(dptmpl,"max",&error);
	int *mini=weed_get_int_array(dptmpl,"min",&error);
	
	if (ndvals>ondvals) valsi=(int *)g_realloc(valsi,ndvals*sizeof(int));

	for (i=0;i<ndvals;i++) {
	  if (autoscale) {
	    if (valsb[i]==WEED_TRUE) valsb[i]=maxi[maxct];
	    else valsb[i]=mini[minct];
	  }
	  else {
	    if (valsb[i]>maxi[maxct]) valsb[i]=maxi[maxct];
	    if (valsb[i]<mini[minct]) valsb[i]=mini[maxct];
	  }
	  if (i>=ondvals||valsi[i]!=valsb[i]) {
	    retval=TRUE;
	    valsi[i]=valsb[i];
	  }
	  if (++maxct==nmax) maxct=0;
	  if (++minct==nmin) minct=0;
	}
	if (retval) {

	  if (inst!=NULL&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,pnum);
	    copyto=set_copy_to(inst,pnum,FALSE);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pthread_mutex_lock(&mainw->data_mutex);
	  weed_set_int_array(dparam,"value",ndvals,valsi);
	  pthread_mutex_unlock(&mainw->data_mutex);
	}
	weed_free(maxi);
	weed_free(mini);
	weed_free(valsi);
	weed_free(valsb);
      }
      return retval;

    case WEED_SEED_BOOLEAN:
      {
	int *valsB=weed_get_boolean_array(sparam,"value",&error);
	int *valsb=weed_get_boolean_array(dparam,"value",&error);

	if (ndvals>ondvals) valsb=(int *)g_realloc(valsb,ndvals*sizeof(int));

	for (i=0;i<ndvals;i++) {
	  if (i>=ondvals||valsb[i]!=valsB[i]) {
	    retval=TRUE;
	    valsb[i]=valsB[i];
	  }
	}
	if (retval) {

	  if (inst!=NULL&&mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
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

  return retval;
}



boolean pconx_chain_data(int key, int mode) {

  weed_plant_t **inparams;
  weed_plant_t *oparam;
  weed_plant_t *inst=NULL;

  boolean changed,reinit_inst=FALSE;

  int error;
  int nparams=0;
  int autoscale;
  int pflags;

  int copyto=-1;

  register int i;

  if (key>-1) {
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
    
    if (weed_plant_has_leaf(inst,"in_parameters")) nparams=weed_leaf_num_elements(inst,"in_parameters");
  }
  else if (key==-2) {
    // playback plugin
    if (mainw->vpp==NULL) return FALSE;
    nparams=mainw->vpp->num_play_params;
  }

  if (nparams>0) {

    if (key==-2) inparams=mainw->vpp->play_params;
    else inparams=weed_get_plantptr_array(inst,"in_parameters",&error);

    for (i=0;i<nparams;i++) {
      if ((oparam=pconx_get_out_param(FALSE,key,mode,i,&autoscale))!=NULL) {
	//	#define DEBUG_PCONX
#ifdef DEBUG_PCONX
	g_print("got pconx from %d %d %d\n",key,mode,i);
#endif
	changed=pconx_convert_value_data(inst,i,key==-2?(weed_plant_t *)pp_get_param(mainw->vpp->play_params,i):inparams[i],oparam,autoscale);

	if (changed&&inst!=NULL&&key>-1) {
	  // only store value if it changed; for int, double or colour, store old value too

	  copyto=set_copy_to(inst,i,TRUE);
	  if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	    // if we are recording, add this change to our event_list
	    rec_param_change(inst,i);
	    if (copyto!=-1) rec_param_change(inst,copyto);
	  }

	  pflags=weed_get_int_value(inparams[i],"flags",&error);
	  if (pflags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE) reinit_inst=TRUE;
	  if (copyto!=-1) {
	    pflags=weed_get_int_value(inparams[copyto],"flags",&error);
	    if (pflags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE) reinit_inst=TRUE;
	  }

	  if (fx_dialog[1]!=NULL&&!reinit_inst) {
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
    if (key!=-2) weed_free(inparams);
  }
  return reinit_inst;
}


boolean pconx_chain_data_internal(weed_plant_t *inst) {
  // special version for compound fx internal connections
  weed_plant_t **in_params;

  boolean autoscale,reinit_inst=FALSE;

  int nparams=0,error,pflags,copyto;

  register int i;

  nparams=num_in_params(inst,FALSE,FALSE);
  if (nparams==0) return FALSE;

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  for (i=0;i<nparams;i++) {
    if (weed_plant_has_leaf(in_params[i],"host_internal_connection")) {
      autoscale=FALSE;
      if (weed_plant_has_leaf(in_params[i],"host_internal_connection_autoscale")&&
	  weed_get_boolean_value(in_params[i],"host_internal_connection_autoscale",&error)==WEED_TRUE) autoscale=TRUE;
      if (pconx_convert_value_data(inst,i,in_params[i],weed_get_plantptr_value(in_params[i],"host_internal_connection",&error),autoscale)) {

	copyto=set_copy_to(inst,i,TRUE);

	pflags=weed_get_int_value(in_params[i],"flags",&error);
	if (pflags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE) reinit_inst=TRUE;
	if (copyto!=-1) {
	  pflags=weed_get_int_value(in_params[copyto],"flags",&error);
	  if (pflags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE) reinit_inst=TRUE;
	}
      }

    }
  }

  weed_free(in_params);
  return reinit_inst;
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


static lives_cconnect_t *cconx_new (int okey, int omode) {
  lives_cconnect_t *cconx=(lives_cconnect_t *)g_malloc0(sizeof(struct _lives_cconnect_t));
  cconx->next=NULL;
  cconx->okey=okey;
  cconx->omode=omode;
  cconx->nchans=0;
  cconx->nconns=NULL;
  return cconx;
}



static lives_cconnect_t *cconx_copy(lives_cconnect_t *scconx) {
  lives_cconnect_t *cconx=NULL,*dcconx,*last_dcconx;

  int totcons=0;

  register int i,j=0;

  while (scconx!=NULL) {
    dcconx=cconx_new(scconx->okey,scconx->omode);
    if (cconx==NULL) cconx=dcconx;
    else last_dcconx->next=dcconx;

    dcconx->nchans=scconx->nchans;
    
    dcconx->nconns=(int *)g_malloc(dcconx->nchans*sizint);
    dcconx->chans=(int *)g_malloc(dcconx->nchans*sizint);

    dcconx->ikey=dcconx->imode=dcconx->icnum=NULL;

    j=0;

    for (i=0;i<dcconx->nchans;i++) {
      dcconx->chans[i]=scconx->chans[i];
      dcconx->nconns[i]=scconx->nconns[i];
      totcons+=dcconx->nconns[i];

      dcconx->ikey=(int *)g_realloc(dcconx->ikey,totcons*sizint);
      dcconx->imode=(int *)g_realloc(dcconx->imode,totcons*sizint);
      dcconx->icnum=(int *)g_realloc(dcconx->icnum,totcons*sizint);

      while (j<totcons) {
	dcconx->ikey[j]=scconx->ikey[j];
	dcconx->imode[j]=scconx->imode[j];
	dcconx->icnum[j]=scconx->icnum[j];
	j++;
      }
    }

    scconx=scconx->next;
    last_dcconx=dcconx;
  }

  return cconx;
}


gchar *cconx_list(int okey, int omode, int ocnum) {
  gchar *st1=g_strdup(""),*st2;
  lives_cconnect_t *cconx=mainw->cconx;

  int totcons=0;

  register int i,j;

  while (cconx!=NULL) {
    if (cconx->okey==okey&&cconx->omode==omode) {
      for (i=0;i<cconx->nchans;i++) {
	if (cconx->chans[i]==ocnum) {
	  for (j=totcons;j<totcons+cconx->nconns[i];j++) {
	    if (strlen(st1)==0) st2=g_strdup_printf("%d %d %d",cconx->ikey[j]+1,cconx->imode[j]+1,cconx->icnum[j]);
	    st2=g_strdup_printf("%s %d %d %d",st1,cconx->ikey[j]+1,cconx->imode[j]+1,cconx->icnum[j]);
	    g_free(st1);
	    st1=st2;
	  }
	  return st1;
	}
	totcons+=cconx->nconns[i];
      }
      return st1;
    }
    cconx=cconx->next;
  }
  return st1;
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

      for (i=0;cconx!=NULL&&i<cconx->nchans;i++) {
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
		cconx->chans[k]=cconx->chans[k+1];
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

  // delete any existing connection to the input channel
  cconx_delete(-1,0,0,ikey,imode,icnum);

  if (cconx==NULL) {
    // add whole new node
    cconx=cconx_new(okey,omode);
    cconx_append(cconx);
  }
  else {
    // see if already in chans
    
    for (i=0;i<cconx->nchans;i++) {
 
      if (cconx->chans[i]==ocnum) {
	// add connection to existing

	for (j=0;j<cconx->nchans;j++) {
	  if (j<i) {
	    // calc posn
	    posn+=cconx->nconns[j];
	  }
	  totcons+=cconx->nconns[j];
	}

	// if already there, do not add again
	for (j=posn;j<posn+cconx->nconns[i];j++) {
	  if (cconx->ikey[j]==ikey&&cconx->imode[j]==imode&&cconx->icnum[j]==icnum) {
	    return;
	  }

	  // add in order key/mode/chan
	  if (cconx->ikey[j]>ikey||(cconx->ikey[j]==ikey&&cconx->imode[j]>imode)||
	      (cconx->ikey[j]==ikey&&cconx->imode[j]==imode&&cconx->icnum[j]>icnum)) break;
	      
	}

	posn=j; // we will insert here
	cconx->nconns[i]++;
	totcons++;

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



weed_plant_t *cconx_get_out_alpha(boolean use_filt, int ikey, int imode, int icnum) {
  // walk all cconx and find one which has ikey/imode/icnum as destination
  // then all we need do is convert the pixel_data

  lives_cconnect_t *cconx=mainw->cconx;

  weed_plant_t *inst=NULL,*filter=NULL;

  int totcons,error;
  register int i,j;

  while (cconx!=NULL) {
    if (!use_filt) {
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

      filter=weed_instance_get_filter(inst,TRUE);
    }
    else {
      filter=rte_keymode_get_filter(cconx->okey+1,cconx->omode);
      if (filter==NULL) {
	cconx=cconx->next;
	continue;
      }
    }
    if (!weed_plant_has_leaf(filter,"out_channel_templates")) {
      cconx=cconx->next;
      continue;
    }
    totcons=0;
    j=0;
    for (i=0;i<cconx->nchans;i++) {
      totcons+=cconx->nconns[i];
      for (;j<totcons;j++) {
	if (cconx->ikey[j]==ikey && cconx->imode[j]==imode && cconx->icnum[j]==icnum) {
	  weed_plant_t **outchans;
	  weed_plant_t *channel=NULL;
	  if (use_filt) {
	    outchans=weed_get_plantptr_array(filter,"out_channel_templates",&error);
	    if (cconx->chans[i]<weed_leaf_num_elements(filter,"out_channel_templates")) {
	      channel=outchans[cconx->chans[i]];
	    }
	  }
	  else {
	    while (weed_plant_has_leaf(inst,"host_next_instance")) inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
	    outchans=weed_get_plantptr_array(inst,"out_channels",&error);
	    if (cconx->chans[i]<weed_leaf_num_elements(inst,"out_channels")) {
	      channel=outchans[cconx->chans[i]];
	    }
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


boolean cconx_convert_pixel_data(weed_plant_t *dchan, weed_plant_t *schan) {
  // convert pixel_data by possibly converting the type (palette)

  // return TRUE if we need to reinit the instance (because channel palette changed)

  // we set boolean "host_orig_pdata" if we steal the schan pdata (so do not free....)

  int error;
  int iwidth,iheight,ipal,irow;
  int owidth,oheight,opal,orow,oflags;
  boolean pal_ok,needs_reinit=FALSE;

  weed_plant_t *dtmpl=weed_get_plantptr_value(dchan,"template",&error);

  uint8_t *spdata,*dpdata;

  register int i;

  ipal=weed_get_int_value(schan,"current_palette",&error);
  if (!weed_palette_is_alpha_palette(ipal)) return FALSE;

  iwidth=weed_get_int_value(schan,"width",&error);
  iheight=weed_get_int_value(schan,"height",&error);
  irow=weed_get_int_value(schan,"rowstrides",&error);

  owidth=weed_get_int_value(dchan,"width",&error);
  oheight=weed_get_int_value(dchan,"height",&error);
  opal=weed_get_int_value(dchan,"current_palette",&error);
  orow=weed_get_int_value(dchan,"rowstrides",&error);

  spdata=(uint8_t *)weed_get_voidptr_value(schan,"pixel_data",&error);

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

  dpdata=(uint8_t *)weed_get_voidptr_value(dchan,"pixel_data",&error);

  if (dpdata!=NULL) {
    g_free(dpdata);
    dpdata=NULL;
  }

  weed_set_int_value(dchan,"width",iwidth);
  weed_set_int_value(dchan,"height",iheight);
  weed_set_int_value(dchan,"current_palette",ipal);

  if (pal_ok) {
    weed_set_voidptr_value(dchan,"pixel_data",(void *)spdata);
    weed_set_int_value(dchan,"rowstrides",irow);
    
    /// caller - do not free in dchan
    weed_set_boolean_value(dchan,"host_orig_pdata",WEED_TRUE);
    return FALSE;
  }
  create_empty_pixel_data(dchan,FALSE,TRUE);
  dpdata=(uint8_t *)weed_get_voidptr_value(dchan,"pixel_data",&error);
  
  orow=weed_get_int_value(dchan,"rowstrides",&error);

  if (irow==orow) {
    memcpy((void *)dpdata,(void *)spdata,irow*iheight);
  }
  else {
    int ipwidth = iwidth * weed_palette_get_bits_per_macropixel(ipal) / 8;
    for (i=0;i<iheight;i++) {
      memcpy((void *)dpdata,(void *)spdata,ipwidth);
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
  weed_plant_t *inst=NULL;

  boolean needs_reinit=FALSE;

  register int i=0;

  if (key>-1) {
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
  }
  else if (key==-2) {
    if (mainw->vpp==NULL||mainw->vpp->num_alpha_chans==0) return FALSE;
  }

  while ((ichan=(key==-2?(weed_plant_t *)pp_get_chan(mainw->vpp->play_params,i):get_enabled_channel(inst,i,TRUE)))!=NULL) {
    if ((ochan=cconx_get_out_alpha(FALSE,key,mode,i++))!=NULL) {
      if (cconx_convert_pixel_data(ichan,ochan)) needs_reinit=TRUE;
    }
  }
  return needs_reinit;
}
 



boolean cconx_chain_data_internal(weed_plant_t *ichan) {
  // special version for compound fx internal connections
  boolean needs_reinit=FALSE;

  if (weed_plant_has_leaf(ichan,"host_internal_connection")) {
    int error;
    weed_plant_t *ochan=weed_get_plantptr_value(ichan,"host_internal_connection",&error);
    if (cconx_convert_pixel_data(ichan,ochan)) needs_reinit=TRUE;
  }
  return needs_reinit;
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// channel/param connection window

// still TODO 

// low priority:

// add a new nb page, limit to one key/mode per page

// show values w. nb pages

// add expanders

static lives_pconnect_t *pconx;
static lives_cconnect_t *cconx;

static GtkWidget *acbutton,*apbutton,*disconbutton;

static void do_chan_connected_error(lives_conx_w *);
static void do_param_connected_error(lives_conx_w *);
static void do_param_incompatible_error(lives_conx_w *);


enum {
  KEY_COLUMN,
  NAME_COLUMN,
  KEYVAL_COLUMN,
  MODEVAL_COLUMN,
  NUM_COLUMNS
};



static void disconbutton_clicked(GtkButton *button, gpointer user_data) {
  // disconnect all channels/params
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  register int i;

  for (i=0;i<conxwp->num_alpha;i++) {
    gtk_combo_box_set_active (LIVES_COMBO(conxwp->cfxcombo[i]),0);
  }

  for (i=0;i<conxwp->num_params;i++) {
    gtk_combo_box_set_active (LIVES_COMBO(conxwp->pfxcombo[i]),0);
  }

}


static void apbutton_clicked(GtkButton *button, gpointer user_data) {
  // autoconnect each param with a compatible one in the target
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  GtkWidget *combo=(GtkWidget *)conxwp->pfxcombo[0];

  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreePath *tpath;

  weed_plant_t **iparams,**oparams;
  weed_plant_t *filter,*param,*oparam;

  int fidx,key,mode;
  int niparams;
  int error;

  register int i,j=0,k=0;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo),&iter)) return;
  model=gtk_combo_box_get_model(GTK_COMBO_BOX(combo));

  gtk_tree_model_get(model,&iter,KEYVAL_COLUMN,&key,MODEVAL_COLUMN,&mode,-1);
  fidx=rte_keymode_get_filter_idx(key,mode);

  if (fidx==-1) return;

  tpath=gtk_tree_model_get_path(model,&iter);
  gtk_tree_model_get_iter(model,&iter,tpath);

  // find the receiving filter/instance
  filter=get_weed_filter(fidx);

  iparams=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  niparams=weed_leaf_num_elements(filter,"in_parameter_templates");

  oparams=weed_get_plantptr_array(rte_keymode_get_filter(conxwp->okey+1,conxwp->omode),"out_parameter_templates",&error);

  // set all pcombo with params
  for (i=0;i<niparams;i++) {

    param=iparams[j++];

    if (weed_plant_has_leaf(param,"host_internal_connection")) {
      i--;
      continue;
    }

    if (pconx_get_out_param(TRUE,key,mode,j-1,NULL)!=NULL) continue;

    oparam=oparams[k];

    if (!params_compatible(oparam,param)) continue;
 
    if (k>0) {
      model=gtk_combo_box_get_model(GTK_COMBO_BOX(conxwp->pfxcombo[k]));
      gtk_tree_model_get_iter(model,&iter,tpath);
      gtk_combo_box_set_active_iter(LIVES_COMBO(conxwp->pfxcombo[k]),&iter);
      lives_widget_context_update();
    }
    gtk_combo_box_set_active(LIVES_COMBO(conxwp->pcombo[k++]),i);

    if (k>=conxwp->num_params) break;
  }

  // TODO - set others to blank ??


  gtk_tree_path_free(tpath);

  weed_free(iparams);
  weed_free(oparams);

}


static void acbutton_clicked(GtkButton *button, gpointer user_data) {
  // autoconnect each channel with a compatible one in the target
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  GtkWidget *combo=(GtkWidget *)conxwp->cfxcombo[0];

  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreePath *tpath;

  weed_plant_t **ichans;
  weed_plant_t *filter,*chan;

  int fidx,key,mode;
  int nichans;
  int error;

  register int i,j=0,k=0;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo),&iter)) return;
  model=gtk_combo_box_get_model(GTK_COMBO_BOX(combo));

  gtk_tree_model_get(model,&iter,KEYVAL_COLUMN,&key,MODEVAL_COLUMN,&mode,-1);
  fidx=rte_keymode_get_filter_idx(key,mode);


  if (fidx==-1) return;

  tpath=gtk_tree_model_get_path(model,&iter);

  // find the receiving filter/instance
  filter=get_weed_filter(fidx);

  ichans=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  nichans=weed_leaf_num_elements(filter,"in_channel_templates");

  // set all ccombo with chans
  for (i=0;i<nichans;i++) {

    chan=ichans[j++];

    if (!has_alpha_palette(chan)) {
      i--;
      continue;
    }

    if (cconx_get_out_alpha(TRUE,key,mode,j-1)!=NULL) continue;

    if (k>0) {
      model=gtk_combo_box_get_model(GTK_COMBO_BOX(conxwp->cfxcombo[k]));
      gtk_tree_model_get_iter(model,&iter,tpath);
      gtk_combo_box_set_active_iter(LIVES_COMBO(conxwp->cfxcombo[k]),&iter);
      lives_widget_context_update();
    }
    gtk_combo_box_set_active(LIVES_COMBO(conxwp->ccombo[k++]),i);
    if (k>=conxwp->num_alpha) break;
  }

  // TODO - set others to blank


  gtk_tree_path_free(tpath);

  weed_free(ichans);
}



static void dfxc_changed(GtkWidget *combo, gpointer user_data) {
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  GtkTreeIter iter;
  GtkTreeModel *model;

  weed_plant_t **ichans;
  weed_plant_t *filter,*chan;

  GList *clist=NULL;

  gchar *channame;

  int fidx,key,mode;
  int nichans;
  int error;
  int ours=-1;

  register int i,j=0;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo),&iter)) return;
  model=gtk_combo_box_get_model(GTK_COMBO_BOX(combo));

  gtk_tree_model_get(model,&iter,KEYVAL_COLUMN,&key,MODEVAL_COLUMN,&mode,-1);
  fidx=rte_keymode_get_filter_idx(key,mode);

  for (i=0;i<conxwp->num_alpha;i++) {
    if (conxwp->cfxcombo[i]==combo) {
      ours=i;
      break;
    }
  }

  if (fidx==-1) {
    gtk_combo_box_set_active (LIVES_COMBO(combo),0);
    lives_combo_populate(LIVES_COMBO(conxwp->ccombo[ours]),NULL);
    lives_combo_set_active_string (LIVES_COMBO(conxwp->ccombo[ours]),"");
    if (ours==0) gtk_widget_set_sensitive(acbutton,FALSE);
    return;
  }

  // find the receiving filter/instance
  filter=get_weed_filter(fidx);

  ichans=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  nichans=weed_leaf_num_elements(filter,"in_channel_templates");

  // populate all ccombo with channels
  for (i=0;i<nichans;i++) {
    chan=ichans[j++];

    if (!has_alpha_palette(chan)) continue;

    channame=weed_get_string_value(chan,"name",&error);
    clist=g_list_append(clist,channame);
  }

  weed_free(ichans);

  lives_combo_populate(LIVES_COMBO(conxwp->ccombo[ours]),clist);
  lives_combo_set_active_string (LIVES_COMBO(conxwp->ccombo[ours]),"");
  
  if (ours==0) gtk_widget_set_sensitive(acbutton,TRUE);

  g_list_free_strings(clist);
  g_list_free(clist);


}



static void dfxp_changed(GtkWidget *combo, gpointer user_data) {
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  GtkTreeIter iter;
  GtkTreeModel *model;

  weed_plant_t **iparams;
  weed_plant_t *filter,*param;

  GList *plist=NULL;

  gchar *paramname;

  gchar *ptype,*range;
  gchar *array_type,*text;

  int defelems,pflags,stype;

  int fidx,key,mode;
  int niparams;
  int error;
  int ours=-1;

  register int i,j=0;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo),&iter)) {
    return;
  }

  for (i=0;i<conxwp->num_params;i++) {
    if (conxwp->pfxcombo[i]==combo) {
      ours=i;
      break;
    }
  }

  model=gtk_combo_box_get_model(GTK_COMBO_BOX(combo));

  gtk_tree_model_get(model,&iter,KEYVAL_COLUMN,&key,MODEVAL_COLUMN,&mode,-1);
  fidx=rte_keymode_get_filter_idx(key,mode);

  if (fidx==-1) {
    GtkWidget *acheck=conxwp->acheck[ours];
    g_signal_handler_block(acheck,conxwp->acheck_func[ours]);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acheck),FALSE);
    gtk_widget_set_sensitive(acheck,FALSE);
    g_signal_handler_unblock(acheck,conxwp->acheck_func[ours]);

    gtk_combo_box_set_active (LIVES_COMBO(combo),0);

    lives_combo_populate(LIVES_COMBO(conxwp->pcombo[ours]),NULL);
    lives_combo_set_active_string (LIVES_COMBO(conxwp->pcombo[ours]),"");

    if (ours==0) gtk_widget_set_sensitive(apbutton,FALSE);
    return;
  }

  // find the receiving filter/instance
  filter=get_weed_filter(fidx);

  iparams=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  niparams=weed_leaf_num_elements(filter,"in_parameter_templates");

  // populate all pcombo with params
  for (i=0;i<niparams;i++) {
    param=iparams[j++];

    if (weed_plant_has_leaf(param,"host_internal_connection")) continue;

    paramname=weed_get_string_value(param,"name",&error);

    ptype=weed_seed_type_to_text((stype=weed_leaf_seed_type(param,"default")));

    pflags=weed_get_int_value(param,"flags",&error);

    if (pflags&WEED_PARAMETER_VARIABLE_ELEMENTS) array_type=g_strdup("[]");
    else if ((defelems=weed_leaf_num_elements(param,"default"))>1) array_type=g_strdup_printf("[%d]",defelems);
    else array_type=g_strdup("");
    
    if (weed_plant_has_leaf(param,"max")&&weed_plant_has_leaf(param,"min")) {
      if (stype==WEED_SEED_INT) {
	range=g_strdup_printf("Range: %d to %d",weed_get_int_value(param,"min",&error),weed_get_int_value(param,"max",&error));
      }
      else if (stype==WEED_SEED_DOUBLE) {
	range=g_strdup_printf("Range: %f to %f",weed_get_double_value(param,"min",&error),weed_get_double_value(param,"max",&error));
      }
      else range=g_strdup("");
    }
    else range=g_strdup("");

    text=g_strdup_printf("%s (%s%s) %s",paramname,ptype,array_type,range);
    
    plist=g_list_append(plist,text);

    weed_free(paramname); g_free(ptype); g_free(array_type); g_free(range);

  }

  weed_free(iparams);

  lives_combo_populate(LIVES_COMBO(conxwp->pcombo[ours]),plist);
  lives_combo_set_active_string (LIVES_COMBO(conxwp->pcombo[ours]),"");

  if (ours==0) gtk_widget_set_sensitive(apbutton,TRUE);

  g_list_free_strings(plist);
  g_list_free(plist);


}



static void dpp_changed(GtkWidget *combo, gpointer user_data) {
  // receiver param was set

  // 1) check if compatible

  // 2) maybe enable autoscale

  // 3) set text to just param name


 
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  weed_plant_t **iparams,**oparams;

  weed_plant_t *param=NULL,*oparam;

  weed_plant_t *filter=rte_keymode_get_filter(conxwp->okey+1,conxwp->omode);

  GtkWidget *acheck;
  GtkWidget *fxcombo;

  GtkTreeModel *model;

  GtkTreeIter iter;

  gchar *paramname;

  int niparams;

  int fidx,key,mode,ours=-1,error;
  gint idx=gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

  boolean hasone=FALSE;

  register int i,j=0;

  for (i=0;i<conxwp->num_params;i++) {
    if (conxwp->pcombo[i]==combo) {
      ours=i;
      break;
    }
  }


  if (idx==-1) {
    for (i=0;i<conxwp->num_alpha;i++) if (gtk_combo_box_get_active(GTK_COMBO_BOX(conxwp->ccombo[i]))>-1) {
	hasone=TRUE;
	break;
      }
    if (!hasone) for (i=0;i<conxwp->num_params;i++) if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conxwp->pcombo[i]),"idx"))>-1) {
	hasone=TRUE;
	break;
      }

    if (!hasone) gtk_widget_set_sensitive(disconbutton,FALSE);

    if (conxwp->ikeys[conxwp->num_alpha+ours]!=0) {
      pconx_delete(conxwp->okey,conxwp->omode,ours,
		   conxwp->ikeys[conxwp->num_alpha+ours]-1,
		   conxwp->imodes[conxwp->num_alpha+ours],
		   conxwp->idx[conxwp->num_alpha+ours]);

    }
    conxwp->ikeys[conxwp->num_alpha+ours]=0;
    conxwp->imodes[conxwp->num_alpha+ours]=0;
    conxwp->idx[conxwp->num_alpha+ours]=0;

    g_object_set_data(G_OBJECT(combo),"idx",GINT_TO_POINTER(idx));

    return;
  }



  fxcombo=conxwp->pfxcombo[ours];

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(fxcombo),&iter)) return;

  model=gtk_combo_box_get_model(GTK_COMBO_BOX(fxcombo));

  gtk_tree_model_get(model,&iter,KEYVAL_COLUMN,&key,MODEVAL_COLUMN,&mode,-1);
  fidx=rte_keymode_get_filter_idx(key,mode);

  oparams=weed_get_plantptr_array(filter,"out_parameter_templates",&error);
  oparam=oparams[ours];

  // find the receiving filter/instance
  filter=get_weed_filter(fidx);

  iparams=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  niparams=weed_leaf_num_elements(filter,"in_parameter_templates");

  for (i=0;i<niparams;i++) {
    param=iparams[i];
    if (weed_plant_has_leaf(param,"host_internal_connection")) continue;
    if (j==idx) break;
    j++;
  }

  weed_free(iparams);
  weed_free(oparams);

  j=i;


  if (!params_compatible(oparam,param)) {
    do_param_incompatible_error(conxwp);
    lives_combo_set_active_string (LIVES_COMBO(combo),"");
    return;

  }

  if (pconx_get_out_param(TRUE,key-1,mode,j,NULL)!=NULL) {
    // dest param already has a connection
    do_param_connected_error(conxwp);
    lives_combo_set_active_string (LIVES_COMBO(combo),"");
    return;
  }

  g_object_set_data(G_OBJECT(combo),"idx",GINT_TO_POINTER(idx));

  acheck=conxwp->acheck[ours];

  g_signal_handler_block(acheck,conxwp->acheck_func[ours]);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acheck),FALSE);
  g_signal_handler_unblock(acheck,conxwp->acheck_func[ours]);

  if (weed_plant_has_leaf(param,"min")&&weed_plant_has_leaf(param,"max")) {
    boolean hasrange=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(acheck),"available"));
    if (hasrange) {
      gtk_widget_set_sensitive(acheck,TRUE);
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(conxwp->allcheckc))) {
	g_signal_handler_block(acheck,conxwp->acheck_func[ours]);
	lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acheck),TRUE);
	g_signal_handler_unblock(acheck,conxwp->acheck_func[ours]);
      }
    }
  }

  paramname=weed_get_string_value(param,"name",&error);

  g_signal_handler_block(combo,conxwp->dpp_func[ours]);
  lives_combo_set_active_string (LIVES_COMBO(combo),paramname);
  g_signal_handler_unblock(combo,conxwp->dpp_func[ours]);

  weed_free(paramname);

  if (conxwp->ikeys[conxwp->num_alpha+ours]!=0) pconx_delete(conxwp->okey,conxwp->omode,ours,
							     conxwp->ikeys[conxwp->num_alpha+ours]-1,
							     conxwp->imodes[conxwp->num_alpha+ours],
							     conxwp->idx[conxwp->num_alpha+ours]);

  pconx_add_connection(conxwp->okey,conxwp->omode,ours,key-1,mode,j,lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(acheck)));

  conxwp->ikeys[conxwp->num_alpha+ours]=key;
  conxwp->imodes[conxwp->num_alpha+ours]=mode;
  conxwp->idx[conxwp->num_alpha+ours]=j;

  gtk_widget_set_sensitive(disconbutton,TRUE);

}


static void dpc_changed(GtkWidget *combo, gpointer user_data) {
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  weed_plant_t **ichans,**ochans;

  weed_plant_t *filter;
  weed_plant_t *chan;

  GtkTreeModel *model;

  GtkTreeIter iter;

  GtkWidget *fxcombo;

  boolean hasone=FALSE;

  int nichans,nochans;

  int key,mode,fidx,ours=-1,error;

  gint idx=gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

  register int i,j=0;

  for (i=0;i<conxwp->num_alpha;i++) {
    if (conxwp->ccombo[i]==combo) {
      ours=i;
      break;
    }
  }

  if (idx==-1) {

    for (i=0;i<conxwp->num_alpha;i++) if (gtk_combo_box_get_active(GTK_COMBO_BOX(conxwp->ccombo[i]))>-1) {
	hasone=TRUE;
	break;
      }
    if (!hasone) for (i=0;i<conxwp->num_params;i++) if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conxwp->pcombo[i]),"idx"))>-1) {
	hasone=TRUE;
	break;
      }

    if (!hasone) gtk_widget_set_sensitive(disconbutton,FALSE);

    if (conxwp->ikeys[ours]!=0) cconx_delete(conxwp->okey,conxwp->omode,ours,
					     conxwp->ikeys[ours]-1,
					     conxwp->imodes[ours],
					     conxwp->idx[ours]);

    conxwp->ikeys[ours]=0;
    conxwp->imodes[ours]=0;
    conxwp->idx[ours]=0;

    return;
  }


  fxcombo=conxwp->cfxcombo[ours];

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(fxcombo),&iter)) return;

  model=gtk_combo_box_get_model(GTK_COMBO_BOX(fxcombo));
  gtk_tree_model_get(model,&iter,KEYVAL_COLUMN,&key,MODEVAL_COLUMN,&mode,-1);
  fidx=rte_keymode_get_filter_idx(key,mode);

  // find the receiving filter/instance
  filter=get_weed_filter(fidx);

  ichans=weed_get_plantptr_array(filter,"in_channel_templates",&error);
  nichans=weed_leaf_num_elements(filter,"in_channel_templates");

  for (i=0;i<nichans;i++) {
    chan=ichans[i];
    if (!has_alpha_palette(chan)) continue;
    if (j==idx) break;
    j++;
  }

  weed_free(ichans);

  j=i;

  if (cconx_get_out_alpha(TRUE,key-1,mode,j)!=NULL) {
    // dest chan already has a connection
    do_chan_connected_error(conxwp);
    lives_combo_set_active_string (LIVES_COMBO(combo),"");
    return;
  }

  if (conxwp->ikeys[ours]!=0) cconx_delete(conxwp->okey,conxwp->omode,ours,
					   conxwp->ikeys[ours]-1,
					   conxwp->imodes[ours],
					   conxwp->idx[ours]);

  filter=rte_keymode_get_filter(conxwp->okey+1,conxwp->omode);
  ochans=weed_get_plantptr_array(filter,"out_channel_templates",&error);
  nochans=weed_leaf_num_elements(filter,"out_channel_templates");

  for (i=0;i<nochans;i++) {
    chan=ochans[i];
    if (!has_alpha_palette(chan)) {
      ours++;
      continue;
    }
    if (i==ours) break;
  }

  weed_free(ochans);


  cconx_add_connection(conxwp->okey,conxwp->omode,ours,key-1,mode,j);

  conxwp->ikeys[ours]=key;
  conxwp->imodes[ours]=mode;
  conxwp->idx[ours]=j;

  gtk_widget_set_sensitive(disconbutton,TRUE);

}



static void on_allcheck_toggled(GtkToggleButton *button, gpointer user_data) {
 lives_conx_w *conxwp=(lives_conx_w *)user_data;
 boolean on=lives_toggle_button_get_active(button);

 register int i;

 for (i=0;i<conxwp->num_params;i++) {
   if (lives_widget_is_sensitive(conxwp->acheck[i])) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(conxwp->acheck[i]),on);
 }

}


static void on_acheck_toggled(GtkToggleButton *acheck, gpointer user_data) {
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  weed_plant_t **iparams;
  weed_plant_t *param,*filter;

  GtkTreeModel *model;

  GtkTreeIter iter;

  GtkWidget *fxcombo;

  boolean on=lives_toggle_button_get_active(acheck);

  int ours=-1,fidx,key,mode,error,niparams;
  gint idx;

  register int i,j=0;

  for (i=0;i<conxwp->num_params;i++) {
    if (conxwp->acheck[i]==(GtkWidget *)acheck) {
      ours=i;
      break;
    }
  }

  idx=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conxwp->pcombo[ours]),"idx"));

  fxcombo=conxwp->pfxcombo[ours];

  model=gtk_combo_box_get_model(GTK_COMBO_BOX(fxcombo));
  gtk_combo_box_get_active_iter(GTK_COMBO_BOX(fxcombo),&iter);

  gtk_tree_model_get(model,&iter,KEYVAL_COLUMN,&key,MODEVAL_COLUMN,&mode,-1);

  if (key==0) return;

  fidx=rte_keymode_get_filter_idx(key,mode);


  // find the receiving filter/instance
  filter=get_weed_filter(fidx);

  iparams=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  niparams=weed_leaf_num_elements(filter,"in_parameter_templates");

  for (i=0;i<niparams;i++) {
    param=iparams[j];
    if (weed_plant_has_leaf(param,"host_internal_connection")) continue;
    if (j==idx) break;
    j++;
  }

  j=i;

  weed_free(iparams);

  if (conxwp->ikeys[conxwp->num_alpha+ours]!=0) pconx_delete(conxwp->okey,conxwp->omode,ours,key-1,mode,j);
  pconx_add_connection(conxwp->okey,conxwp->omode,ours,key-1,mode,j,on);

  conxwp->ikeys[conxwp->num_alpha+ours]=key;
  conxwp->imodes[conxwp->num_alpha+ours]=mode;
  conxwp->idx[conxwp->num_alpha+ours]=j;

}


static LiVESTreeModel *inparam_fx_model (boolean is_chans) {
  GtkTreeStore *tstore;

  GtkTreeIter iter1,iter2;

  weed_plant_t *filter;

  gchar *fxname,*keystr,*text;

  boolean key_added;

  int idx;

  int error;

  int nmodes=rte_getmodespk();

  register int i,j;

  tstore=gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);

  gtk_tree_store_append (tstore, &iter1, NULL);  /* Acquire an iterator */
  gtk_tree_store_set(tstore,&iter1,
		     KEY_COLUMN,mainw->string_constants[LIVES_STRING_CONSTANT_NONE],
		     NAME_COLUMN,mainw->string_constants[LIVES_STRING_CONSTANT_NONE],
		     KEYVAL_COLUMN,0,
		     MODEVAL_COLUMN,0,
		     -1);


  // go through all keys
  for (i=1;i<=FX_KEYS_MAX_VIRTUAL;i++) {
    key_added=FALSE;
    keystr=g_strdup_printf(_("Key slot %d"),i);

    for (j=0;j<nmodes;j++) {

      if ((idx=rte_keymode_get_filter_idx(i,j))==-1) continue;

      filter=get_weed_filter(idx);

      if (!is_chans) {
	if (num_in_params(filter,FALSE,TRUE)==0) continue;
      }
      else 
	if (num_alpha_channels(filter,FALSE)==0) continue;
	
      fxname=weed_get_string_value(filter,"name",&error);
      text=g_strdup_printf("(%d,%d) %s",i,j+1,fxname);

      if (!key_added) {
	// add key
	gtk_tree_store_append (tstore, &iter1, NULL);  /* Acquire an iterator */
	gtk_tree_store_set(tstore,&iter1,KEY_COLUMN,keystr,NAME_COLUMN,keystr,KEYVAL_COLUMN,0,MODEVAL_COLUMN,0,-1);
	key_added=TRUE;
      }
      gtk_tree_store_append (tstore, &iter2, &iter1);
      gtk_tree_store_set(tstore,&iter2,KEY_COLUMN,text,NAME_COLUMN,text,KEYVAL_COLUMN,i,MODEVAL_COLUMN,j,-1);

      weed_free(fxname); weed_free(text);
    }

    g_free(keystr);

  }

  return (LiVESTreeModel *)tstore;
}





static GtkWidget *conx_scroll_new(weed_plant_t *filter, lives_conx_w *conxwp) {
  LiVESTreeModel *model;

  GtkWidget *label;
  GtkWidget *top_vbox;
  GtkWidget *hbox,*hbox2;
  GtkWidget *scrolledwindow;

  GtkWidget *fx_entry;

  weed_plant_t *chan,*param;

  gchar *channame,*pname;
  gchar *lctext;
  gchar *ptype,*range;
  gchar *array_type,*text;

  boolean hasrange;

  int defelems,pflags,stype;

  int error;

  register int i,j=0,x=0;

  conxwp->cfxcombo=conxwp->ccombo=conxwp->pcombo=conxwp->pfxcombo=conxwp->acheck=NULL;
  conxwp->dpp_func=conxwp->dpc_func=conxwp->acheck_func=NULL;

  conxwp->ikeys=(int *)g_malloc((conxwp->num_params+conxwp->num_alpha)*sizint);
  conxwp->imodes=(int *)g_malloc((conxwp->num_params+conxwp->num_alpha)*sizint);
  conxwp->idx=(int *)g_malloc((conxwp->num_params+conxwp->num_alpha)*sizint);

  for (i=0;i<conxwp->num_params+conxwp->num_alpha;i++) conxwp->ikeys[i]=conxwp->imodes[i]=conxwp->idx[i]=0;

  lctext=g_strdup(_("Connected to -->"));

  top_vbox=lives_vbox_new (FALSE, 0);

  scrolledwindow = lives_standard_scrolled_window_new (-1,-1,top_vbox,TRUE);

  if (conxwp->num_alpha>0) {
    weed_plant_t **ochans=weed_get_plantptr_array(filter,"out_channel_templates",&error);

    conxwp->dpc_func=(gulong *)g_malloc(conxwp->num_alpha*sizeof(gulong));

    conxwp->cfxcombo=(GtkWidget **)g_malloc(conxwp->num_alpha*sizeof(GtkWidget *));

    conxwp->ccombo=(GtkWidget **)g_malloc(conxwp->num_alpha*sizeof(GtkWidget *));

    label=lives_standard_label_new(_("Alpha Channel Connections"));
    gtk_box_pack_start (GTK_BOX (top_vbox), label, FALSE, FALSE, widget_opts.packing_height);

    for (i=0;i<conxwp->num_alpha;i++) {
      chan=ochans[j++];

      if (!has_alpha_palette(chan)) continue;

      hbox=lives_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (top_vbox), hbox, FALSE, FALSE, widget_opts.packing_width);

      channame=weed_get_string_value(chan,"name",&error);
      label=lives_standard_label_new(channame);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);
      weed_free(channame);

      label=lives_standard_label_new(lctext);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);


      // create combo entry model
      model=inparam_fx_model(TRUE);

      conxwp->cfxcombo[x] = lives_combo_new_with_model (model);

      lives_combo_set_entry_text_column(LIVES_COMBO(conxwp->cfxcombo[x]),NAME_COLUMN);

      gtk_box_pack_start (GTK_BOX (hbox), conxwp->cfxcombo[x], FALSE, FALSE, widget_opts.packing_width);

      fx_entry = lives_combo_get_entry(LIVES_COMBO(conxwp->cfxcombo[x]));
      gtk_entry_set_text (GTK_ENTRY (fx_entry),mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
      lives_entry_set_editable (LIVES_ENTRY (fx_entry), FALSE);


      conxwp->ccombo[x]=lives_standard_combo_new("",FALSE,NULL,LIVES_BOX(hbox),NULL);

      g_signal_connect(GTK_OBJECT (conxwp->cfxcombo[x]), "changed",
		       G_CALLBACK (dfxc_changed),(gpointer)conxwp);
      

      conxwp->dpc_func[x]=g_signal_connect(GTK_OBJECT (conxwp->ccombo[x]), "changed",
					G_CALLBACK (dpc_changed),(gpointer)conxwp);
      

      x++;
    }

    weed_free(ochans);

  }

  if (conxwp->num_alpha>0&&conxwp->num_params>0) {
    add_hsep_to_box (LIVES_BOX (top_vbox));
  }


  if (conxwp->num_params>0) {
    weed_plant_t **oparams=weed_get_plantptr_array(filter,"out_parameter_templates",&error);

    conxwp->pfxcombo=(GtkWidget **)g_malloc(conxwp->num_params*sizeof(GtkWidget *));
    conxwp->pcombo=(GtkWidget **)g_malloc(conxwp->num_params*sizeof(GtkWidget *));
    conxwp->dpp_func=(gulong *)g_malloc(conxwp->num_params*sizeof(gulong));
    conxwp->acheck_func=(gulong *)g_malloc(conxwp->num_params*sizeof(gulong));

    conxwp->acheck=(GtkWidget **)g_malloc(conxwp->num_params*sizeof(GtkWidget *));

    x=0;

    label=lives_standard_label_new(_("Parameter Data Connections"));
    gtk_box_pack_start (GTK_BOX (top_vbox), label, FALSE, FALSE, widget_opts.packing_height);

    hbox=lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (top_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    hbox2=lives_hbox_new (FALSE, 0);
    gtk_box_pack_end (GTK_BOX (hbox), hbox2, FALSE, FALSE, widget_opts.packing_width);
    conxwp->allcheckc=lives_standard_check_button_new(_("Autoscale All"),FALSE,LIVES_BOX(hbox2),NULL);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(conxwp->allcheckc),TRUE);

    g_signal_connect_after (GTK_OBJECT (conxwp->allcheckc), "toggled",
			    G_CALLBACK (on_allcheck_toggled),
			    (gpointer)conxwp);

    for (i=0;i<conxwp->num_params;i++) {
      hbox=lives_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (top_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

      param=oparams[i];

      pname=weed_get_string_value(param,"name",&error);

      ptype=weed_seed_type_to_text((stype=weed_leaf_seed_type(param,"default")));

      pflags=weed_get_int_value(param,"flags",&error);

      if (pflags&WEED_PARAMETER_VARIABLE_ELEMENTS) array_type=g_strdup("[]");
      else if ((defelems=weed_leaf_num_elements(param,"default"))>1) array_type=g_strdup_printf("[%d]",defelems);
      else array_type=g_strdup("");

      hasrange=FALSE;

      if (weed_plant_has_leaf(param,"max")&&weed_plant_has_leaf(param,"min")) {
	if (stype==WEED_SEED_INT) {
	  range=g_strdup_printf("Range: %d to %d",weed_get_int_value(param,"min",&error),weed_get_int_value(param,"max",&error));
	  hasrange=TRUE;
	}
	else if (stype==WEED_SEED_DOUBLE) {
	  range=g_strdup_printf("Range: %f to %f",weed_get_double_value(param,"min",&error),weed_get_double_value(param,"max",&error));
	  hasrange=TRUE;
	}
	else range=g_strdup("");
      }
      else range=g_strdup("");
      
      text=g_strdup_printf("%s (%s%s) %s",pname,ptype,array_type,range);

      label=lives_standard_label_new(text);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);
      weed_free(pname); g_free(ptype); g_free(array_type); g_free(range); g_free(text);

      label=lives_standard_label_new(lctext);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);

      // create combo entry model
      model=inparam_fx_model(FALSE);

      conxwp->pfxcombo[x] = lives_combo_new_with_model (model);

      lives_combo_set_entry_text_column(LIVES_COMBO(conxwp->pfxcombo[x]),NAME_COLUMN);

      gtk_box_pack_start (GTK_BOX (hbox), conxwp->pfxcombo[x], FALSE, FALSE, widget_opts.packing_width);

      fx_entry = lives_combo_get_entry(LIVES_COMBO(conxwp->pfxcombo[x]));
      gtk_entry_set_text (GTK_ENTRY (fx_entry),mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
      lives_entry_set_editable (LIVES_ENTRY (fx_entry), FALSE);


      conxwp->pcombo[x]=lives_standard_combo_new("",FALSE,NULL,LIVES_BOX(hbox),NULL);
      g_object_set_data(G_OBJECT(conxwp->pcombo[x]),"idx",GINT_TO_POINTER(-1));

      add_fill_to_box(GTK_BOX(hbox));

      conxwp->acheck[x]=lives_standard_check_button_new(_("Autoscale"),FALSE,LIVES_BOX(hbox),NULL);

      gtk_widget_set_sensitive(conxwp->acheck[x],FALSE);
      g_object_set_data(G_OBJECT(conxwp->acheck[x]),"available",GINT_TO_POINTER(hasrange));

      conxwp->acheck_func[x]=g_signal_connect_after (GTK_OBJECT (conxwp->acheck[x]), "toggled",
						     G_CALLBACK (on_acheck_toggled),
						     (gpointer)conxwp);

      g_signal_connect(GTK_OBJECT (conxwp->pfxcombo[x]), "changed",
		       G_CALLBACK (dfxp_changed),(gpointer)conxwp);


      conxwp->dpp_func[x]=g_signal_connect(GTK_OBJECT (conxwp->pcombo[x]), "changed",
					   G_CALLBACK (dpp_changed),(gpointer)conxwp);
      

      x++;
    }

    weed_free(oparams);


  }

  g_free(lctext);

  return scrolledwindow;
}


static void conxw_cancel_clicked(GtkWidget *button, gpointer user_data) {
  lives_conx_w *conxwp=(lives_conx_w *)user_data;

  if (conxwp->cfxcombo!=NULL) g_free(conxwp->cfxcombo);
  if (conxwp->ccombo!=NULL) g_free(conxwp->ccombo);
  if (conxwp->pfxcombo!=NULL) g_free(conxwp->pfxcombo);
  if (conxwp->pcombo!=NULL) g_free(conxwp->pcombo);
  if (conxwp->acheck!=NULL) g_free(conxwp->acheck);
  if (conxwp->dpp_func!=NULL) g_free(conxwp->dpp_func);
  if (conxwp->dpc_func!=NULL) g_free(conxwp->dpp_func);
  if (conxwp->acheck_func!=NULL) g_free(conxwp->acheck_func);

  g_free(conxwp->ikeys);
  g_free(conxwp->imodes);
  g_free(conxwp->idx);

  pconx_delete_all();
  cconx_delete_all();

  if (button==NULL) return;

  // restore old values
  mainw->pconx=pconx;
  mainw->cconx=cconx;

 lives_general_button_clicked(LIVES_BUTTON(button),NULL);
}



static void conxw_ok_clicked(GtkWidget *button, gpointer user_data) {
  lives_cconnect_t *cconx_bak=mainw->cconx;
  lives_pconnect_t *pconx_bak=mainw->pconx;

  mainw->pconx=pconx;
  mainw->cconx=cconx;

  conxw_cancel_clicked(NULL,user_data);

  mainw->cconx=cconx_bak;
  mainw->pconx=pconx_bak;

  lives_general_button_clicked(LIVES_BUTTON(button),NULL);
}


static void set_to_keymode_vals(GtkComboBox *combo, int xkey, int xmode) {
  GtkTreeIter iter,piter;
  GtkTreeModel *model;

  int key,mode;

  model=gtk_combo_box_get_model(combo);
  if (!gtk_tree_model_get_iter_first(model,&piter)) return;

  do {
    if (gtk_tree_model_iter_children(model,&iter,&piter)) {
      do {
	gtk_tree_model_get(model,&iter,KEYVAL_COLUMN,&key,MODEVAL_COLUMN,&mode,-1);
	if (key==xkey+1&&mode==xmode) goto iter_found;
      } while (gtk_tree_model_iter_next(model,&iter));
    }
  } while (gtk_tree_model_iter_next(model,&piter));


iter_found:
  gtk_combo_box_set_active_iter(combo,&iter);

}

static int lastckey,lastcmode;
static int lastpkey,lastpmode;

static boolean show_existing(lives_conx_w *conxwp) {
  lives_cconnect_t *cconx=cconx_find(conxwp->okey,conxwp->omode);
  lives_pconnect_t *pconx=pconx_find(conxwp->okey,conxwp->omode);

  GtkWidget *cfxcombo,*ccombo;
  GtkWidget *pfxcombo,*pcombo;
  GtkWidget *acheck;

  weed_plant_t **ochans,**ichans;
  weed_plant_t **iparams;

  weed_plant_t *ofilter=rte_keymode_get_filter(conxwp->okey+1,conxwp->omode),*filter;

  weed_plant_t *chan,*param;

  int ikey,imode,icnum,error,nochans,ipnum,nichans,niparams;

  int curckey=FX_KEYS_MAX+1,curcmode=0;
  int curpkey=FX_KEYS_MAX+1,curpmode=0;

  int posn=0,cidx,pidx;

  register int i,j,k,l;

  // ASSUME FOR NOW THAT EACH OPARAM/OCHAN is mapped to maybe multiple key/modes, but only a single param/chan witin that
  // TODO - update gui with ---> expander to handle this


  if (cconx==NULL) goto show_ex_params;

  // find lowest key/mode which has not been done
  for (i=0;i<cconx->nchans;i++) {
    for (j=posn;j<posn+cconx->nconns[i];j++) {
      ikey=cconx->ikey[j];
      imode=cconx->imode[j];

      if ((ikey<curckey||(ikey==curckey&&imode<curcmode)) && (ikey>lastckey||(ikey==lastckey&&imode>lastcmode))) {
	curckey=ikey;
	curcmode=imode;
      }

    }
    posn+=cconx->nconns[i];
  }


  // now have curckey and curcmode - set fx and param combos
  lastckey=curckey;
  lastcmode=curcmode;
  posn=0;

  for (i=0;i<cconx->nchans;i++) {
    cidx=cconx->chans[i];
    for (j=posn;j<posn+cconx->nconns[i];j++) {
      ikey=cconx->ikey[j];
      imode=cconx->imode[j];

      if (ikey==curckey&&imode==curcmode) {
	      
	// find the row
	ochans=weed_get_plantptr_array(ofilter,"out_channel_templates",&error);
	nochans=weed_leaf_num_elements(ofilter,"out_channel_templates");
	      
	l=0;
	      
	for (k=0;k<nochans;k++) {
	  chan=ochans[k];
	  if (!has_alpha_palette(chan)) continue;
	  if (k==cidx) break;
	  l++;
	}
	      
	weed_free(ochans);

	// row is l
	cfxcombo=conxwp->cfxcombo[l];

	// set it to the value which has ikey/imode
	set_to_keymode_vals(GTK_COMBO_BOX(cfxcombo),ikey,imode);

	// set channel
	ccombo=conxwp->ccombo[l];
	icnum=cconx->icnum[j];


	filter=rte_keymode_get_filter(ikey+1,imode);
	ichans=weed_get_plantptr_array(filter,"in_channel_templates",&error);
	nichans=weed_leaf_num_elements(filter,"in_channel_templates");
	
	cidx=l;

	l=0;
	
	for (k=0;k<nichans;k++) {
	  chan=ichans[k];
	  if (!has_alpha_palette(chan)) continue;
	  if (k==icnum) break;
	  l++;
	}
	      
	weed_free(ichans);

	g_signal_handler_block(ccombo,conxwp->dpc_func[cidx]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(ccombo),l);
	g_signal_handler_unblock(ccombo,conxwp->dpc_func[cidx]);
	      
	conxwp->ikeys[cidx]=ikey+1;
	conxwp->imodes[cidx]=imode;
	conxwp->idx[cidx]=icnum;

	gtk_widget_set_sensitive(disconbutton,TRUE);
	
	break; // TODO ***
      }
      
      
    }
    posn+=cconx->nconns[i];
  }



 show_ex_params:


  if (pconx==NULL) goto show_ex_done;

  // find lowest key/mode which has not been done
  for (i=0;i<pconx->nparams;i++) {
    for (j=posn;j<posn+pconx->nconns[i];j++) {
      ikey=pconx->ikey[j];
      imode=pconx->imode[j];

      if ((ikey<curpkey||(ikey==curpkey&&imode<curpmode)) && (ikey>lastpkey||(ikey==lastpkey&&imode>lastpmode))) {
	curpkey=ikey;
	curpmode=imode;
      }

    }
    posn+=pconx->nconns[i];
  }


  // now have curpkey and curpmode - set fx and param combos and autoscale
  lastpkey=curpkey;
  lastpmode=curpmode;
  posn=0;

  for (i=0;i<pconx->nparams;i++) {
    pidx=pconx->params[i];
    for (j=posn;j<posn+pconx->nconns[i];j++) {
      ikey=pconx->ikey[j];
      imode=pconx->imode[j];

      if (ikey==curpkey&&imode==curpmode) {
	      
	l=pidx;

	// row is l
	pfxcombo=conxwp->pfxcombo[l];

	// set it to the value which has ikey/imode
	set_to_keymode_vals(GTK_COMBO_BOX(pfxcombo),ikey,imode);

	// set channel
	pcombo=conxwp->pcombo[l];
	acheck=conxwp->acheck[l];

	gtk_widget_set_sensitive(acheck,TRUE);

	g_signal_handler_block(acheck,conxwp->acheck_func[l]);
	lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acheck),pconx->autoscale[j]);
	g_signal_handler_unblock(acheck,conxwp->acheck_func[l]);

	ipnum=pconx->ipnum[j];

	filter=rte_keymode_get_filter(ikey+1,imode);
	iparams=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
	niparams=weed_leaf_num_elements(filter,"in_parameter_templates");
	
	l=0;
	
	for (k=0;k<niparams;k++) {
	  param=iparams[k];
	  if (weed_plant_has_leaf(param,"host_internal_connection")) continue;
	  if (k==ipnum) break;
	  l++;
	}
	      
	weed_free(iparams);

	g_signal_handler_block(pcombo,conxwp->dpp_func[pidx]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pcombo),l);
	g_signal_handler_unblock(pcombo,conxwp->dpp_func[pidx]);

	g_object_set_data(G_OBJECT(pcombo),"idx",GINT_TO_POINTER(l));
	      
	conxwp->ikeys[conxwp->num_alpha+pidx]=ikey+1;
	conxwp->imodes[conxwp->num_alpha+pidx]=imode;
	conxwp->idx[conxwp->num_alpha+pidx]=ipnum;

	gtk_widget_set_sensitive(disconbutton,TRUE);
	
	break; // TODO ***
      }
      
      
    }
    posn+=pconx->nconns[i];
  }

 show_ex_done:

  return FALSE;
}





GtkWidget *make_datacon_window(int key, int mode) {
  weed_plant_t *filter=rte_keymode_get_filter(key+1,mode);

  static lives_conx_w conxw;

  GtkWidget *cbox,*abox;
  GtkWidget *scrolledwindow;

  GtkWidget *cancelbutton;
  GtkWidget *okbutton;

  GtkAccelGroup *accel_group;

  boolean needsanother=TRUE;

  int scr_width,scr_height;

  int winsize_h;
  int winsize_v;

  if (filter==NULL) return NULL;

  acbutton=apbutton=NULL;

  pconx=pconx_copy(mainw->pconx);
  cconx=cconx_copy(mainw->cconx);

  conxw.okey=key;
  conxw.omode=mode;

  conxw.num_alpha=num_alpha_channels(filter,TRUE);
  conxw.num_params=weed_leaf_num_elements(filter,"out_parameter_templates");

  conxw.ntabs=0;

  if (prefs->gui_monitor==0) {
    scr_width=mainw->scr_width;
    scr_height=mainw->scr_height;
  }
  else {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
    scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
  }

  winsize_h=scr_width-200;
  winsize_v=scr_height-200;

  conxw.conx_dialog=lives_standard_dialog_new(_("LiVES: - Parameter and Alpha Channel Connections"),FALSE);
  gtk_widget_set_size_request (conxw.conx_dialog, winsize_h, winsize_v);

  accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (conxw.conx_dialog), accel_group);

  abox = lives_dialog_get_action_area(LIVES_DIALOG(conxw.conx_dialog));

  if (conxw.num_alpha>0) {
    acbutton = gtk_button_new_with_mnemonic (_("Auto Connect Channels"));
    gtk_box_pack_start (GTK_BOX (abox), acbutton, FALSE, FALSE, widget_opts.packing_width);
    gtk_widget_set_sensitive(acbutton,FALSE);

    g_signal_connect (GTK_OBJECT (acbutton), "clicked",
		      G_CALLBACK (acbutton_clicked),
		      (gpointer)&conxw);


  }

  if (conxw.num_params>0) {
    apbutton = gtk_button_new_with_mnemonic (_("Auto Connect Parameters"));
    gtk_box_pack_start (GTK_BOX (abox), apbutton, FALSE, FALSE, widget_opts.packing_width);
    gtk_widget_set_sensitive(apbutton,FALSE);

    g_signal_connect (GTK_OBJECT (apbutton), "clicked",
		      G_CALLBACK (apbutton_clicked),
		      (gpointer)&conxw);

  }

  disconbutton = gtk_button_new_with_mnemonic (_("Disconnect All"));
  gtk_box_pack_start (GTK_BOX (abox), disconbutton, FALSE, FALSE, widget_opts.packing_width);
  gtk_widget_set_sensitive(disconbutton,FALSE);

  g_signal_connect (GTK_OBJECT (disconbutton), "clicked",
		    G_CALLBACK (disconbutton_clicked),
		    (gpointer)&conxw);

  if (conxw.num_alpha>0||conxw.num_params>0) add_fill_to_box(GTK_BOX(abox));

  cbox = lives_dialog_get_content_area(LIVES_DIALOG(conxw.conx_dialog));

  lastckey=lastcmode=lastpkey=lastpmode=-1;

  while (needsanother) {
    scrolledwindow = conx_scroll_new(filter,&conxw);
    needsanother=show_existing(&conxw);
  }

  gtk_box_pack_start (GTK_BOX (cbox), scrolledwindow, TRUE, TRUE, 0);


  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (conxw.conx_dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (conxw.conx_dialog), okbutton, GTK_RESPONSE_OK);

  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default(okbutton);

  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape,  (GdkModifierType)0, (GtkAccelFlags)0);


  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (conxw_cancel_clicked),
		    (gpointer)&conxw);


  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (conxw_ok_clicked),
		    (gpointer)&conxw);


  gtk_widget_show_all (conxw.conx_dialog);

  return conxw.conx_dialog;
}


static void do_chan_connected_error( lives_conx_w *conxwp) {

  do_error_dialog_with_check_transient(_("Input channel is already connected"),TRUE,0,GTK_WINDOW(conxwp->conx_dialog));

}


static void do_param_connected_error( lives_conx_w *conxwp) {

  do_error_dialog_with_check_transient(_("Input parameter is already connected"),TRUE,0,GTK_WINDOW(conxwp->conx_dialog));

}


static void do_param_incompatible_error( lives_conx_w *conxwp) {

  do_error_dialog_with_check_transient(_("Input and output parameters are not compatible"),TRUE,0,GTK_WINDOW(conxwp->conx_dialog));

}
