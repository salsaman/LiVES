// data_processor.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// generically process out[x] from a combination of in[y], store[z] and arithmetic expressions
//#define DEBUG
#include <stdio.h>

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define EQS 256
#define EQN 512

#define NSTORE (EQN-EQS)

#define MAX_EXP_LEN 65536

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


static node *add_parent(node *xnode, char *value) {
  // add a parent to node; if node is rootnode, newnode becomes the new root
  // node becomes the left child

  node *newnode=new_node(value);
  newnode->left=xnode;
  if (xnode->parent!=NULL) {
    if (xnode->parent->left==xnode) xnode->parent->left=newnode;
    else xnode->parent->right=newnode;
  } else rootnode=newnode;
  xnode->parent=newnode;

  return newnode;
}


static node *add_right(node *xnode, char *value) {
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

  if (op->varname!=NULL) {
    snprintf(buf,32768,"%s[%d]",op->varname,(int)res);
    res=getval(buf,sdata);
    free(op->varname);
    op->varname=NULL;
  }

  snprintf(buf,32768,"%f",res);

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
  } else {
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
    } else {
      if (xnode->visited<2) {
        xnode->visited=2;
        simplify(xnode,sdata);
      }
    }
  }

  xnode->visited=2;

  return res;
}

//#define DEBUG_SYNTAX

static int exp_to_tree(const char *exp) {
  size_t len=strlen(exp);

  int nstart=-1;
  int plevel=0;
  int pstart;
  int gotdot=0;
  int retval;
  int op=0;

  char buf[1024];

  char *varname=NULL;
  char *parbit;

  char *tmp;

  node *oldroot;

  register int i;

  for (i=0; i<len; i++) {
    switch (exp[i]) {

    case '[':
      if (varname==NULL) {
#ifdef DEBUG_SYNTAX
        printf("pt 1\n");
#endif
        return 1;
      }
      plevel=2;
      pstart=++i;

      while (1) {
        if (!strncmp(&exp[i],"]",1)) break;
        i++;
        if (i>len) {
#ifdef DEBUG_SYNTAX
          printf("pt 2\n");
#endif
          return 1;
        }
      }

      if (i-pstart+3>MAX_EXP_LEN) return 5;

      parbit=weed_malloc(i-pstart+3);
      sprintf(parbit,"0+"); // need at least one operator to hold the varname

      snprintf(parbit+2,i-pstart+1,"%s",exp+pstart);

#ifdef DEBUG
      fprintf(stderr,"got subexpression2 %s\n",parbit);
#endif

      oldroot=rootnode;
      rootnode=NULL;

      retval=exp_to_tree(parbit);
      weed_free(parbit);

      if (retval!=0) return retval;

      rootnode->varname=varname;
      varname=NULL;

      if (oldroot!=NULL) {
        oldroot->right=rootnode;
        if (rootnode!=NULL) rootnode->parent=oldroot;
        rootnode=oldroot;
      }

      break;

    case '(':
      if (plevel==1) {
#ifdef DEBUG_SYNTAX
        printf("pt 3\n");
#endif
        return 1;
      }
      if (nstart!=-1) {
#ifdef DEBUG_SYNTAX
        printf("pt 4\n");
#endif
        return 1;
      }

      plevel=2;
      pstart=++i;

      while (1) {
        if (!strncmp(&exp[i],")",1)) plevel--;
        else if (!strncmp(&exp[i],"(",1)) plevel++;

        if (plevel==1) break;

        i++;
        if (i>len) {
#ifdef DEBUG_SYNTAX
          printf("pt 5\n");
#endif
          return 1;
        }
      }

      parbit=strndup(exp+pstart,i-pstart);

#ifdef DEBUG
      fprintf(stderr,"got subexpression %s\n",parbit);
#endif

      oldroot=rootnode;
      rootnode=NULL;

      retval=exp_to_tree(parbit);
      weed_free(parbit);

      if (retval!=0) return retval;

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
      if (plevel==1) {
#ifdef DEBUG_SYNTAX
        printf("pt 6\n");
#endif
        return 1;
      }
      if (varname!=NULL) {
#ifdef DEBUG_SYNTAX
        printf("pt 7\n");
#endif
        return 1;
      }
      if (nstart==-1) nstart=i;
      break;
    case '.':
      if (plevel==1) {
#ifdef DEBUG_SYNTAX
        printf("pt 8\n");
#endif
        return 1;
      }
      if (gotdot||varname!=NULL) {
#ifdef DEBUG_SYNTAX
        printf("pt 9\n");
#endif
        return 1;
      }
      if (nstart==-1) nstart=i;
      gotdot=1;
      break;
    case '-':
    case '+':
      if (varname!=NULL) {
#ifdef DEBUG_SYNTAX
        printf("pt 10\n");
#endif
        return 1;
      }
      if (nstart==-1&&plevel==0) {
        if (len+2>MAX_EXP_LEN) return 5;

        tmp=weed_malloc(len+2);
        snprintf(tmp,i+1,"%s",exp);

        sprintf(tmp+i,"0");

        // replace "+-" or "-+" with "0-"
        // replace "++" or "--" with "0+ or "
        if ((op=='-'&&exp[i]=='+')||(op=='+'&&exp[i]=='-')||(op!='+'&&op!='-'&&exp[i]=='-'))
          sprintf(tmp+i+1,"-");
        else
          sprintf(tmp+i+1,"+");
        sprintf(tmp+i+2,"%s",exp+i+1);
        len++;
        i--;
        sprintf((char *)exp,"%s",tmp);

        weed_free(tmp);
        op=exp[i];
        break;
      }

    case '*':
    case '/':
      if (varname!=NULL) {
#ifdef DEBUG_SYNTAX
        printf("pt 11\n");
#endif
        return 1;
      }
      op=exp[i];
      if (plevel==0) {
        if (nstart==-1) {
#ifdef DEBUG_SYNTAX
          printf("pt 12\n");
#endif
          return 1;
        }

        snprintf(buf,i-nstart+1,"%s",exp+nstart);

        if (rootnode==NULL) {
          rootnode=new_node(buf);
          snprintf(buf,2,"%s",&exp[i]);
          add_parent(rootnode,buf);
        } else {
          add_right(rootnode,buf);
          snprintf(buf,2,"%s",&exp[i]);
          add_parent(rootnode,buf);
        }
      } else {
        snprintf(buf,2,"%s",&exp[i]);
        add_parent(rootnode,buf);
      }

      nstart=-1;
      gotdot=0;
      plevel=0;

      break;
    case 'i':
      if (plevel==1) {
#ifdef DEBUG_SYNTAX
        printf("pt 13\n");
#endif
        return 1;
      }
      if (varname!=NULL||nstart!=-1) {
#ifdef DEBUG_SYNTAX
        printf("pt 14\n");
#endif
        return 1;
      }
      varname=strdup("i");
      break;
    case 's':
      if (plevel==1) {
#ifdef DEBUG_SYNTAX
        printf("pt 15\n");
#endif
        return 1;
      }
      if (varname!=NULL||nstart!=-1) {
#ifdef DEBUG_SYNTAX
        printf("pt 16\n");
#endif
        return 1;
      }
      varname=strdup("s");
      break;
    case ' ':
      break;
    default:
      return 1;
    }
  }

  if (nstart==-1) {
#ifdef DEBUG_SYNTAX
    printf("pt 17\n");
#endif
    if (plevel==0) return 1;
    return 0;
  }

  snprintf(buf,i-nstart+1,"%s",exp+nstart);

  if (rootnode==NULL) {
    rootnode=new_node(buf);
  } else {
    add_right(rootnode,buf);
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







static int preproc(const char *exp) {
  // put parens around *, /
  char tmp[65536];
  char lastop=0;

  int nstart=-1,plevel=0;
  size_t len=strlen(exp);

  register int i;


  for (i=0; i<len; i++) {
    switch (exp[i]) {
    case '(':
      nstart=i;
    case '[':
      i+=preproc(&exp[i+1]);
      len=strlen(exp);
      break;
    case ')':
    case ']':
      i++;
      goto preprocdone;
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
    case 'i':
    case 's':
      if (nstart==-1) nstart=i;
      break;
    case '+':
    case '-':
      if (nstart==-1) {
        nstart=i;
        break;
      }
      if (plevel>0&&(lastop=='*'||lastop=='/')) {
        // close parens
        if (nstart==-1) break;
        sprintf(tmp,"%s",exp);
        sprintf(tmp+i,")%s",exp+i);
        sprintf((char *)exp,"%s",tmp);
        len++;
        i++;
        plevel--;
      }
      lastop=exp[i];
      nstart=-1;
      break;
    case '*':
    case '/':
      if (lastop=='+'||lastop=='-') {
        // open parens
        sprintf(tmp,"%s",exp);
        sprintf(tmp+nstart,"(%s",exp+nstart);
        sprintf((char *)exp,"%s",tmp);
        len++;
        i++;
        nstart=-1;
        plevel++;
      }
      lastop=exp[i];
      break;
    default:
      break;
    }
  }

preprocdone:

  // close any open parens
  if (plevel>0) {
    sprintf(tmp,"%s",exp);
    for (i=0; i<plevel; i++) {
      sprintf(tmp+len+i,")");
    }
    sprintf((char *)exp,"%s",tmp);
    i=strlen(exp);
  }

  return i;
}




static double evaluate(const char *exp, _sdata *sdata) {
  double res;

  sdata->error=0;
  rootnode=NULL;

  preproc(exp);

#ifdef DEBUG
  printf("preproc is %s\n",exp);
#endif

  sdata->error=exp_to_tree(exp);
  if (sdata->error>0) return 0.;

#ifdef DEBUG
  fprintf(stderr,"\nExp is:\n");
  print_tree(rootnode,2);
  fprintf(stderr,"\n\n");
#endif

  simplify(rootnode,sdata);

#ifdef DEBUG
  fprintf(stderr,"\nSimplified result is:\n");
  print_tree(rootnode,3);
  fprintf(stderr,"\n\nOK\n");
#endif

  if (sdata->error) return 0.;

  if (!strncmp(rootnode->value,"inf",3)) {
    sdata->error=2;
    return 0.;
  }

  res=strtod(rootnode->value,NULL);

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

  for (i=0; i<NSTORE; i++) {
    sdata->store[i]=0.;
  }

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}



int dataproc_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  double res=0.;

  char *ip=NULL,*ptr;

  char exp[MAX_EXP_LEN];

  size_t len;

  int which;

  register int i;

  sdata->params=in_params;

  for (i=EQS; i<EQN; i++) {
    if (ip!=NULL) weed_free(ip);
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

    if (strlen(ptr+1)>=MAX_EXP_LEN) {
      fprintf(stderr,"data_processor ERROR: eqn %d has too long expression\n",i-EQS);
      continue;
    }

    len=ptr-ip-2;

    if (len<1) {
      fprintf(stderr,"data_processor ERROR: eqn %d has invalid []\n",i-EQS);
      continue;
    }

    if (len>=MAX_EXP_LEN) {
      fprintf(stderr,"data_processor ERROR: eqn %d has too long []\n",i-EQS);
      continue;
    }

    sdata->error=0;

    snprintf(exp,len,"%s",ip+2);
    which=(int)evaluate(exp,sdata);

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
      sprintf(exp,"%s",ptr+1);
      res=evaluate(exp,sdata);
    } else sdata->error+=100;

    if (sdata->error) {
      switch (sdata->error) {
      case 1:
        sprintf(exp,"%s",ptr+1);
        fprintf(stderr,"data_processor ERROR: syntax error in RHS of eqn %d\n%s\n",i-EQS,exp);
        break;
      case 2:
        fprintf(stderr,"data_processor ERROR: division by 0 in RHS of eqn %d\n",i-EQS);
        break;
      case 3:
        fprintf(stderr,"data_processor ERROR: i array out of bounds in RHS of eqn %d\n",i-EQS);
        break;
      case 4:
        fprintf(stderr,"data_processor ERROR: s array out of bounds in RHS of eqn %d\n",i-EQS);
        break;
      case 5:
        fprintf(stderr,"data_processor ERROR: expanded expression too long in RHS of eqn %d\n",i-EQS);
        break;
      case 101:
        snprintf(exp,len,"%s",ip+2);
        fprintf(stderr,"data_processor ERROR: syntax error in LHS of eqn %d\n%s\n",i-EQS,exp);
        break;
      case 102:
        fprintf(stderr,"data_processor ERROR: division by 0 in LHS of eqn %d\n",i-EQS);
        break;
      case 103:
        fprintf(stderr,"data_processor ERROR: o array out of bounds in LHS of eqn %d\n",i-EQS);
        break;
      case 104:
        fprintf(stderr,"data_processor ERROR: s array out of bounds in LHS of eqn %d\n",i-EQS);
        break;
      case 105:
        fprintf(stderr,"data_processor ERROR: expanded expression too long in LHS of eqn %d\n",i-EQS);
        break;
      default:
        break;
      }

    }

    else {
      if (!strncmp(ip,"s",1)) {
        sdata->store[which]=res;
      } else {
        weed_set_double_value(out_params[which],"value",res);
      }
    }

  }

  if (ip!=NULL) weed_free(ip);

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




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    weed_plant_t *filter_class,*gui;

    weed_plant_t *in_params[EQN+1];
    weed_plant_t *out_params[EQN-EQS+1];

    register int i;

    char name[256];
    char name2[256];
    char label[256];
    char desc[512];

    for (i=0; i<EQS; i++) {
      snprintf(name,256,"input%03d",i);
      in_params[i]=weed_float_init(name,"",0.,-1000000000000.,1000000000000.);
      gui=weed_parameter_template_get_gui(in_params[i]);
      weed_set_boolean_value(gui,"hidden",WEED_TRUE);
    }

    for (i=EQS; i<EQN; i++) {
      snprintf(name,256,"equation%03d",i-EQS);
      snprintf(label,256,"Equation %03d",i-EQS);
      snprintf(name2,256,"output%03d",i-EQS);
      in_params[i]=weed_text_init(name,label,"");
      out_params[i-EQS]=weed_out_param_float_init_nominmax(name2,0.);
    }

    in_params[EQN]=NULL;
    out_params[EQN-EQS]=NULL;

    filter_class=weed_filter_class_init("data_processor","salsaman",1,0,&dataproc_init,&dataproc_process,
                                        &dataproc_deinit,NULL,NULL,in_params,out_params);

    snprintf(desc,512,
             "Generically process out[x] from a combination of in[y], store[z] and arithmetic expressions.\nE.g:\no[0]=s[0]\ns[0]=i[0]*4\n\nArray subscripts can be from 0 - %d",
             EQS-1);

    weed_set_string_value(filter_class,"description",desc);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

