// data_processor.c
// weed plugin
// (c) G. Finch (salsaman) 2011
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// geenrically process out[x] from a combination of in[a][b], store[z] and arithmetic expressions

#include <stdio.h>

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-plugin.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]={131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

/////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>

#define EQS 256
#define EQN 512

#define NSTORE (EQN-EQS)


typedef struct {
  weed_plant_t **params;
  double *store;
  short error;
} _sdata;


typedef struct _node node; 

struct _node {
  node *left;
  node *right;
  node *parent;
  int visited;
  char *value;
  char *varname;
};

static node *rootnode;


static node *new_node(char *value) {
  node *newnode=weed_malloc(sizeof(node));
  newnode->left=newnode->right=newnode->parent=NULL;
  newnode->visited=0;
  newnode->value=strdup(value);
  newnode->varname=NULL;
  return newnode;
}


static node * add_parent(node *xnode, char *value) {
  // add a parent to node; if node is rootnode, newnode becomes the new root
  // node becomes the left child

  node *newnode=new_node(value);
  newnode->left=xnode;
  if (xnode->parent!=NULL) {
    if (xnode->parent->left==xnode) xnode->parent->left=newnode;
    else xnode->parent->right=newnode;
  }
  else rootnode=newnode;
  xnode->parent=newnode;

  return newnode;
}


static node * add_right(node *xnode, char *value) {
  // add node as right child

  node *newnode=new_node(value);
  newnode->parent=xnode;
  xnode->right=newnode;

  return newnode;
}




static void free_all(node *xnode) {
  if (xnode==NULL) return;
  free_all(xnode->left);
  free_all(xnode->right);
  weed_free(xnode->value);
  if (xnode->varname!=NULL) weed_free(xnode->varname);
  weed_free(xnode);
}


static double getval(char *what, _sdata *sdata) {
  int error,which;

  if (!strncmp(what,"i[",2)) {
    which=atoi(what+2);
    if (which>=EQN-EQS) {
      sdata->error=3;
      return 0.;
    }
    return weed_get_double_value(sdata->params[which],"value",&error);
  }

  if (!strncmp(what,"s[",2)) {
    which=atoi(what+2);
    if (which>=NSTORE) {
      sdata->error=4;
      return 0.;
    }
    return sdata->store[which];
  }

  return strtod(what,NULL);

}



static char *simplify2(node *left, node *right, node *op, _sdata *sdata) {
  double res=0.;

  char buf[32768];

  if (!strcmp(op->value,"-")) res=getval(left->value,sdata)-getval(right->value,sdata);
  else if (!strcmp(op->value,"*")) res=getval(left->value,sdata)*getval(right->value,sdata);
  else if (!strcmp(op->value,"+")) res=getval(left->value,sdata)+getval(right->value,sdata);
  else if (!strcmp(op->value,"/")) res=getval(left->value,sdata)/getval(right->value,sdata);

  weed_free(op->value);

  if (op->varname==NULL) snprintf(buf,32768,"%f",res);
  else snprintf(buf,32768,"%s[%d]",op->varname,(int)res);

  op->value=strdup(buf);

  free_all(left);
  free_all(right);
  op->left=op->right=NULL;
  return op->value;

}




static char *simplify(node *xnode, _sdata *sdata) {
  char *res=NULL;

  if (xnode==NULL) return NULL;

  if (xnode->left==NULL) {
    xnode->visited=2;

    return xnode->value;
  }

  if (xnode->left->left==NULL&&xnode->right->left==NULL) {
#ifdef DEBUG
    fprintf(stderr,"simplifying %s %s %s = ",xnode->left->value,xnode->value,xnode->right->value);
#endif
    res=simplify2(xnode->left,xnode->right,xnode,sdata);
    if (xnode->left!=NULL) xnode->left->visited=2;
    if (xnode->right!=NULL) xnode->right->visited=2;

#ifdef DEBUG
    fprintf(stderr,"%s\n",res);
#endif
  }
  else {
    simplify(xnode->left,sdata);
    simplify(xnode->right,sdata);

    if (xnode->left->left==NULL&&xnode->right->left==NULL) {
#ifdef DEBUG
      fprintf(stderr,"simplifying %s %s %s = ",xnode->left->value,xnode->value,xnode->right->value);
#endif
      res=simplify2(xnode->left,xnode->right,xnode,sdata);
      if (xnode->left!=NULL) xnode->left->visited=2;
      if (xnode->right!=NULL) xnode->right->visited=2;
      
#ifdef DEBUG
      fprintf(stderr,"%s\n",res);
#endif
    }
    else {
      if (xnode->visited<2) {
	xnode->visited=2;
	simplify(xnode,sdata);
      }
    }
  }

  xnode->visited=2;

  return res;
}



static int exp_to_tree(const char *exp) {
  size_t len=strlen(exp);

  int nstart=-1;
  int plevel=0;
  int pstart;
  int gotdot=0;
  int op;

  char buf[1024];

  char *varname=NULL;
  char *parbit;

  node *oldroot,*newnode=NULL;


  register int i;

  for (i=0;i<len;i++) {
    switch (exp[i]) {

    case '[':
      if (varname==NULL) return 1;

      plevel=2;
      pstart=++i;

      while(1) {
	if (!strncmp(&exp[i],"]",1)) break;
	i++;
	if (i>len) return 1;
      }

      parbit=weed_malloc(i-pstart+3);
      sprintf(parbit,"%s","0+");

      snprintf(parbit+2,i-pstart+1,"%s",exp+pstart);

#ifdef DEBUG
      fprintf(stderr,"got subexpression2 %s\n",parbit);
#endif

      oldroot=rootnode;
      rootnode=NULL;

      exp_to_tree(parbit);
      weed_free(parbit);

      rootnode->varname=varname;
      varname=NULL;

      if (oldroot!=NULL) {
	oldroot->right=rootnode;
	if (rootnode!=NULL) rootnode->parent=oldroot;
	rootnode=oldroot;
      }

      break;

    case '(':
      if (plevel==1) return 1;
      if (nstart!=-1) return 1;

      plevel=2;
      pstart=++i;

      while(1) {
	if (!strncmp(&exp[i],")",1)) plevel--;
	else if (!strncmp(&exp[i],"(",1)) plevel++;

	if (plevel==1) break;

	i++;
	if (i>len) return 1;

      }

      parbit=strndup(exp+pstart,i-pstart);

#ifdef DEBUG
      fprintf(stderr,"got subexpression %s\n",parbit);
#endif

      oldroot=rootnode;
      rootnode=NULL;

      exp_to_tree(parbit);
      weed_free(parbit);

      if (oldroot!=NULL) {
	oldroot->right=rootnode;
	if (rootnode!=NULL) rootnode->parent=oldroot;
	rootnode=oldroot;
      }
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (plevel==1) return 1;
      if (varname!=NULL) return 1;
      if (nstart==-1) nstart=i;
      break;
    case '.':
      if (plevel==1) return 1;
      if (gotdot||varname!=NULL) return 1;
      if (nstart==-1) nstart=i;
      gotdot=1;
      break;
    case '+':
    case '-':
    case '*':
    case '/':
      if (varname!=NULL) return 1;
      if (plevel==0) {
	if (nstart==-1) return 1;

	op=exp[i];

	snprintf(buf,i-nstart+1,"%s",exp+nstart);
	
	if (rootnode==NULL) {
	  rootnode=new_node(buf);
	  snprintf(buf,2,"%s",&exp[i]);
	  add_parent(rootnode,buf);
	}
	else {
	  if (op=='+'||op=='-'||!strcmp(rootnode->value,"/")||!strcmp(rootnode->value,"*")) {
	    if (newnode==NULL) add_right(rootnode,buf);
	    else add_right(newnode,buf);
	    snprintf(buf,2,"%s",&exp[i]);
	    add_parent(rootnode,buf);
	    newnode=NULL;
	  }
	  else {
	    if (newnode!=NULL) {
	      newnode=add_right(newnode,buf);
	      snprintf(buf,2,"%s",&exp[i]);
	      newnode=add_parent(newnode,buf);
	    }
	    else {
	      newnode=add_right(rootnode,buf);
	      snprintf(buf,2,"%s",&exp[i]);
	      newnode=add_parent(newnode,buf);
	    }
	  }

	}
      }
      else {
	snprintf(buf,2,"%s",&exp[i]);
	add_parent(rootnode,buf);
      }

      nstart=-1;
      gotdot=0;
      plevel=0;

      break;
    case 'i':
      if (plevel==1) return 1;
      if (varname!=NULL||nstart!=-1) return 1;
      varname=strdup("i");
      break;
    case 's':
      if (plevel==1) return 1;
      if (varname!=NULL||nstart!=-1) return 1;
      varname=strdup("s");
      break;
    case ' ':
      break;
    default:
      return 1;
    }
  }

  if (nstart==-1) {
    if (plevel==0) return 1;
    return 0;
  }

  snprintf(buf,i-nstart+1,"%s",exp+nstart);

  if (rootnode==NULL) {
    rootnode=new_node(buf);
  }
  else {
    if (newnode==NULL) {
      add_right(rootnode,buf);
    }
    else add_right(newnode,buf);
  }

  return 0;
}



#ifdef DEBUG
static void print_tree(node *xnode, int visits) {
  //visit tree in infix order

  if (xnode==NULL) return;

  if (xnode->left==NULL) {
    fprintf(stderr,"%s",xnode->value);
    xnode->visited=visits;
    return;
  }

  if (xnode->left->visited<visits) {
    fprintf(stderr,"(");
    print_tree(xnode->left,visits);
  }
  if (xnode->visited<visits) printf("%s",xnode->value);
  xnode->visited=visits;
  if (xnode->right->visited<visits) {
    print_tree(xnode->right,visits);
    fprintf(stderr,")");
  }
}
#endif


static double evaluate (char *exp, _sdata *sdata) {
  double res;

  sdata->error=0;
  rootnode=NULL;

  if (exp_to_tree(exp)) {
    sdata->error=1;
    return 0.;
  }

#ifdef DEBUG
  fprintf(stderr,"\nExp is:\n");
  print_tree(rootnode,2);
  fprintf(stderr,"\n\n");
#endif
  
  simplify(rootnode,sdata);

#ifdef DEBUG
  fprintf(stderr,"\nSimplified result is:\n");
  print_tree(rootnode,3);
  fprintf(stderr,"\n\n");
#endif

  if (sdata->error) return 0.;

  if (!strncmp(rootnode->value,"inf",3)) {
    sdata->error=2;
    return 0.;
  }

  if (!(strncmp(rootnode->value,"i",1)||!strncmp(rootnode->value,"s",1))) {
    char buf[32768];
#ifdef DEBUG
    fprintf(stderr,"!!!!!\n");
#endif
    res=getval(rootnode->value,sdata);
    snprintf(buf,32768,"%f",res);
#ifdef DEBUG
    fprintf(stderr,"real res is %s\n",buf);
#endif
  }
  else res=strtod(rootnode->value,NULL);

  free_all(rootnode);

  return res;
}


//////////////////////////////////////////////////////////////////////////////////


int dataproc_init(weed_plant_t *inst) {
  register int i;

  _sdata *sdata=(_sdata *)weed_malloc(sizeof(_sdata));

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->store=weed_malloc(NSTORE*sizeof(double));

  if (sdata->store==NULL) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (i=0;i<NSTORE;i++) {
    sdata->store[i]=0.;
  }

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}



int dataproc_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  double res=0.;

  char *ip,*ptr,*ws;

  size_t len;

  int which;

  register int i;

  sdata->params=in_params;

  for (i=EQS;i<EQN;i++) {
    ip=weed_get_string_value(in_params[i],"value",&error);

    if (!strlen(ip)) continue;

    ptr=index(ip,'=');
    if (ptr==NULL) {
      fprintf(stderr,"data_processor ERROR: eqn %d has no '='\n",i-EQS);
      continue;
    }

    if (strncmp(ip,"s",1)&&strncmp(ip,"o",1)) {
      fprintf(stderr,"data_processor ERROR: eqn %d must set s or o value\n",i-EQS);
      continue;
    }

    if (strncmp(ip+1,"[",1)||strncmp(ptr-1,"]",1)) {
      fprintf(stderr,"data_processor ERROR: eqn %d has invalid []\n",i-EQS);
      continue;
    }

    len=ptr-ip-2;

    if (len<1) {
      fprintf(stderr,"data_processor ERROR: eqn %d has invalid []\n",i-EQS);
      continue;
    }

    sdata->error=0;

    ws=weed_malloc(len+1);
    snprintf(ws,len,"%s",ip+2);
    which=(int)evaluate(ws,sdata);

    if (!strncmp(ip,"o[",2)) {
      if (which>=EQN-EQS) {
	sdata->error=3;
      }
    }

    else if (!strncmp(ip,"s[",2)) {
      if (which>=NSTORE) {
	sdata->error=4;
      }
    }

    if (!sdata->error) {
      res=evaluate(ptr+1,sdata);
    }
    else sdata->error+=100;

    if (sdata->error) {
      switch (sdata->error) {
      case 1:
	fprintf(stderr,"data_processor ERROR: syntax error in RHS of eqn %d\n",i-EQS); break;
      case 2:
	fprintf(stderr,"data_processor ERROR: division by 0 in RHS of eqn %d\n",i-EQS); break;
      case 3:
	fprintf(stderr,"data_processor ERROR: i array out of bounds in RHS of eqn %d\n",i-EQS); break;
      case 4:
	fprintf(stderr,"data_processor ERROR: s array out of bounds in RHS of eqn %d\n",i-EQS); break;
      case 101:
	fprintf(stderr,"data_processor ERROR: syntax error in LHS of eqn %d\n%s\n",i-EQS,ws); break;
      case 102:
	fprintf(stderr,"data_processor ERROR: division by 0 in LHS of eqn %d\n",i-EQS); break;
      case 103:
	fprintf(stderr,"data_processor ERROR: o array out of bounds in LHS of eqn %d\n",i-EQS); break;
      case 104:
	fprintf(stderr,"data_processor ERROR: s array out of bounds in LHS of eqn %d\n",i-EQS); break;
      default:
	break;
      }

    }

    else {
      if (!strncmp(ip,"s",1)) {
	sdata->store[which]=res;
      }
      else {
	weed_set_double_value(out_params[which],"value",res);
      }
    }

    weed_free(ws);
    weed_free(ip);
  }

  printf("VALSSS: %f %f %f %f : %f %f\n",weed_get_double_value(in_params[0],"value",&error),weed_get_double_value(in_params[1],"value",&error),weed_get_double_value(in_params[2],"value",&error),weed_get_double_value(in_params[3],"value",&error),weed_get_double_value(out_params[0],"value",&error),weed_get_double_value(out_params[1],"value",&error));

  weed_free(in_params);
  weed_free(out_params);

  return WEED_NO_ERROR;
}


int dataproc_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (sdata!=NULL) {
    if (sdata->store!=NULL) weed_free(sdata->store);
    weed_free(sdata);
  }
  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    weed_plant_t *filter_class,*gui;

    weed_plant_t *in_params[EQN+1];
    weed_plant_t *out_params[EQN-EQS+1];

    register int i;

    char name[256];
    char name2[256];
    char label[256];

    for (i=0;i<EQS;i++) {
      snprintf(name,256,"input%03d",i);
      in_params[i]=weed_float_init(name,"",0.,-1000000000000.,1000000000000.);
      gui=weed_parameter_template_get_gui(in_params[i]);
      weed_set_boolean_value(gui,"hidden",WEED_TRUE);
    }

    for (i=EQS;i<EQN;i++) {
      snprintf(name,256,"equation%03d",i-EQS);
      snprintf(label,256,"Equation %03d",i-EQS);
      snprintf(name2,256,"output%03d",i-EQS);
      in_params[i]=weed_text_init(name,label,"");
      out_params[i-EQS]=weed_out_param_float_init(name2,0.,-1000000000000.,1000000000000.);
    }

    in_params[EQN]=NULL;
    out_params[EQN-EQS]=NULL;

    filter_class=weed_filter_class_init("data_processor","salsaman",1,0,&dataproc_init,&dataproc_process,
					&dataproc_deinit,NULL,NULL,in_params,out_params);


    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

