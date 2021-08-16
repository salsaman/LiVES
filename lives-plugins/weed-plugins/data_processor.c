// data_processor.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// generically process out[x] from a combination of in[y], store[z] and arithmetic expressions
//#define DEBUG

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////
// algorithm:
// - preprocess the expression; put * and / in parentheses
// - parse and simplify the equation, e.g a+-b becomes just a-b
// create a tree from the equation, of the form LHS - operator - RHS
//   we start from the lowest level and when reaching an operator, the current tree becomes the LHS
//      and then we start to build the RHS
//   parentheses cause a new sub branch
// then starting from the leaves we evaluate the tree. At the lowest level we substitute symbols with actual numerical
// values, this is then passed up the tree to give the final result.

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define EQS 256
#define EQN 512 ///< must be > EQS

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
  node *newnode = weed_malloc(sizeof(node));
  newnode->left = newnode->right = newnode->parent = NULL;
  newnode->visited = 0;
  newnode->value = strdup(value);
  newnode->varname = NULL;
  return newnode;
}


static node *add_parent(node *xnode, char *value) {
  // add a parent to node; if node is rootnode, newnode becomes the new root
  // node becomes the left child

  node *newnode = new_node(value);
  newnode->left = xnode;
  if (xnode->parent != NULL) {
    if (xnode->parent->left == xnode) xnode->parent->left = newnode;
    else xnode->parent->right = newnode;
  } else rootnode = newnode;
  xnode->parent = newnode;

  return newnode;
}


static node *add_right(node *xnode, char *value) {
  // add node as right child
  node *newnode = new_node(value);
  newnode->parent = xnode;
  xnode->right = newnode;
  return newnode;
}


static void free_all(node *xnode) {
  if (xnode == NULL) return;
  free_all(xnode->left);
  free_all(xnode->right);
  weed_free(xnode->value);
  if (xnode->varname != NULL) weed_free(xnode->varname);
  weed_free(xnode);
}


static double getval(char *what, _sdata *sdata) {
  // convert symbols to numerical (double) values
  int which;

  if (!strncmp(what, "i[", 2)) {
    which = atoi(what + 2);
    if (which >= EQN - EQS) {
      sdata->error = 3;
      return 0.;
    }
    return weed_get_double_value(sdata->params[which], WEED_LEAF_VALUE, NULL);
  }

  if (!strncmp(what, "s[", 2)) {
    which = atoi(what + 2);
    if (which >= NSTORE) {
      sdata->error = 4;
      return 0.;
    }
    return sdata->store[which];
  }
  return strtod(what, NULL);
}


static char *simplify2(node *left, node *right, node *op, _sdata *sdata) {
  // apply an operator to both sides of a node
  double res = 0.;

  char buf[32768];

  if (!strcmp(op->value, "-")) res = getval(left->value, sdata) - getval(right->value, sdata);
  else if (!strcmp(op->value, "*")) res = getval(left->value, sdata) * getval(right->value, sdata);
  else if (!strcmp(op->value, "+")) res = getval(left->value, sdata) + getval(right->value, sdata);
  else if (!strcmp(op->value, "/")) res = getval(left->value, sdata) / getval(right->value, sdata);

  weed_free(op->value);

  if (op->varname != NULL) {
    snprintf(buf, 32768, "%s[%d]", op->varname, (int)res);
    res = getval(buf, sdata);
    free(op->varname);
    op->varname = NULL;
  }

  snprintf(buf, 32768, "%f", res);

  op->value = strdup(buf);

  free_all(left);
  free_all(right);
  op->left = op->right = NULL;
  return op->value;
}


static char *simplify(node *xnode, _sdata *sdata) {
  // parse the tree recursively - we descend the LHS until we reach the bottom
  // calculate the value, then desecend the RHS
  // we visit each node twice, once on the way down, then again on the way up
  char *res = NULL;

  if (!xnode) return NULL;

  if (!xnode->left) {
    xnode->visited = 2;
    return xnode->value;
  }

  if (!xnode->left->left && !xnode->right->left) {
#ifdef DEBUG
    fprintf(stderr, "simplifying %s %s %s = ", xnode->left->value, xnode->value, xnode->right->value);
#endif
    res = simplify2(xnode->left, xnode->right, xnode, sdata);
    if (xnode->left) xnode->left->visited = 2;
    if (xnode->right) xnode->right->visited = 2;

#ifdef DEBUG
    fprintf(stderr, "%s\n", res);
#endif
  } else {
    simplify(xnode->left, sdata);
    simplify(xnode->right, sdata);

    if (!xnode->left->left && !xnode->right->left) {
#ifdef DEBUG
      fprintf(stderr, "simplifying %s %s %s = ", xnode->left->value, xnode->value, xnode->right->value);
#endif
      res = simplify2(xnode->left, xnode->right, xnode, sdata);
      if (xnode->left) xnode->left->visited = 2;
      if (xnode->right) xnode->right->visited = 2;

#ifdef DEBUG
      fprintf(stderr, "%s\n", res);
#endif
    } else {
      if (xnode->visited < 2) {
        xnode->visited = 2;
        simplify(xnode, sdata);
      }
    }
  }

  xnode->visited = 2;

  return res;
}

//#define DEBUG_SYNTAX

static int exp_to_tree(const char *exp) {
  // here we parse an expression (RHS of the equation) and convert it to a tree
  // - if we get parentheses then we call this recursively for the expression in brackets
  // we also do some cleanup of the expression, e.g 1++2 becomes 1+0+2, 1+-2 becomes 1+0-2, etc.
  // spaces are removed
  node *oldroot;
  char buf[1024];
  char *varname = NULL;
  char *parbit, *tmp;

  size_t len = strlen(exp);

  int nstart = -1;
  int plevel = 0, pstart;
  int gotdot = 0;
  int retval;
  int op = 0;
  int i;

  for (i = 0; i < len; i++) {
    switch (exp[i]) {
    case '[':
      if (!varname) {
#ifdef DEBUG_SYNTAX
        printf("pt 1\n");
#endif
        return 1;
      }
      plevel = 2;
      pstart = ++i;

      while (1) {
        if (!strncmp(&exp[i], "]", 1)) break;
        i++;
        if (i > len) {
#ifdef DEBUG_SYNTAX
          printf("pt 2\n");
#endif
          return 1;
        }
      }

      if (i - pstart + 3 > MAX_EXP_LEN) return 5;

      parbit = weed_malloc(i - pstart + 3);
      sprintf(parbit, "0+"); // need at least one operator to hold the varname

      snprintf(parbit + 2, i - pstart + 1, "%s", exp + pstart);

#ifdef DEBUG
      fprintf(stderr, "got subexpression2 %s\n", parbit);
#endif

      oldroot = rootnode;
      rootnode = NULL;

      retval = exp_to_tree(parbit);
      weed_free(parbit);

      if (retval != 0) return retval;

      rootnode->varname = varname;
      varname = NULL;

      if (oldroot) {
        oldroot->right = rootnode;
        if (rootnode) rootnode->parent = oldroot;
        rootnode = oldroot;
      }

      break;

    case '(':
      if (plevel == 1) {
#ifdef DEBUG_SYNTAX
        printf("pt 3\n");
#endif
        return 1;
      }
      if (nstart != -1) {
#ifdef DEBUG_SYNTAX
        printf("pt 4\n");
#endif
        return 1;
      }

      plevel = 2;
      pstart = ++i;

      while (1) {
        if (!strncmp(&exp[i], ")", 1)) plevel--;
        else if (!strncmp(&exp[i], "(", 1)) plevel++;

        if (plevel == 1) break;

        i++;
        if (i > len) {
#ifdef DEBUG_SYNTAX
          printf("pt 5\n");
#endif
          return 1;
        }
      }

      parbit = strndup(exp + pstart, i - pstart);

#ifdef DEBUG
      fprintf(stderr, "got subexpression %s\n", parbit);
#endif

      oldroot = rootnode;
      rootnode = NULL;

      retval = exp_to_tree(parbit);
      weed_free(parbit);

      if (retval != 0) return retval;

      if (oldroot) {
        oldroot->right = rootnode;
        if (rootnode) rootnode->parent = oldroot;
        rootnode = oldroot;
      }
      break;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      if (plevel == 1) {
#ifdef DEBUG_SYNTAX
        printf("pt 6\n");
#endif
        return 1;
      }
      if (varname) {
#ifdef DEBUG_SYNTAX
        printf("pt 7\n");
#endif
        return 1;
      }
      if (nstart == -1) nstart = i;
      break;
    case '.':
      if (plevel == 1) {
#ifdef DEBUG_SYNTAX
        printf("pt 8\n");
#endif
        return 1;
      }
      if (gotdot || varname) {
#ifdef DEBUG_SYNTAX
        printf("pt 9\n");
#endif
        return 1;
      }
      if (nstart == -1) nstart = i;
      gotdot = 1;
      break;
    case '-':
    case '+':
      if (varname) {
#ifdef DEBUG_SYNTAX
        printf("pt 10\n");
#endif
        return 1;
      }
      if (nstart == -1 && plevel == 0) {
        if (len + 2 > MAX_EXP_LEN) return 5;

        tmp = weed_malloc(len + 2);
        snprintf(tmp, i + 1, "%s", exp);

        sprintf(tmp + i, "0");

        // replace "+-" or "-+" with "0-"
        // replace "++" or "--" with "0+ or "
        if ((op == '-' && exp[i] == '+') || (op == '+' && exp[i] == '-') || (op != '+' && op != '-' && exp[i] == '-'))
          sprintf(tmp + i + 1, "-");
        else
          sprintf(tmp + i + 1, "+");
        sprintf(tmp + i + 2, "%s", exp + i + 1);
        len++;
        i--;
        sprintf((char *)exp, "%s", tmp);

        weed_free(tmp);
        op = exp[i];
        break;
      }

    case '*':
    case '/':
      if (varname) {
#ifdef DEBUG_SYNTAX
        printf("pt 11\n");
#endif
        return 1;
      }
      op = exp[i];
      if (plevel == 0) {
        if (nstart == -1) {
#ifdef DEBUG_SYNTAX
          printf("pt 12\n");
#endif
          return 1;
        }

        snprintf(buf, i - nstart + 1, "%s", exp + nstart);

        if (!rootnode) {
          rootnode = new_node(buf);
          snprintf(buf, 2, "%s", &exp[i]);
          add_parent(rootnode, buf);
        } else {
          add_right(rootnode, buf);
          snprintf(buf, 2, "%s", &exp[i]);
          add_parent(rootnode, buf);
        }
      } else {
        snprintf(buf, 2, "%s", &exp[i]);
        add_parent(rootnode, buf);
      }

      nstart = -1;
      gotdot = 0;
      plevel = 0;

      break;
    case 'i':
      if (plevel == 1) {
#ifdef DEBUG_SYNTAX
        printf("pt 13\n");
#endif
        return 1;
      }
      if (varname || nstart != -1) {
#ifdef DEBUG_SYNTAX
        printf("pt 14\n");
#endif
        return 1;
      }
      varname = strdup("i");
      break;
    case 's':
      if (plevel == 1) {
#ifdef DEBUG_SYNTAX
        printf("pt 15\n");
#endif
        return 1;
      }
      if (varname || nstart != -1) {
#ifdef DEBUG_SYNTAX
        printf("pt 16\n");
#endif
        return 1;
      }
      varname = strdup("s");
      break;
    case ' ':
      break;
    default:
      return 1;
    }
  }

  if (nstart == -1) {
#ifdef DEBUG_SYNTAX
    printf("pt 17\n");
#endif
    if (plevel == 0) return 1;
    return 0;
  }

  snprintf(buf, i - nstart + 1, "%s", exp + nstart);

  if (!rootnode) {
    rootnode = new_node(buf);
  } else {
    add_right(rootnode, buf);
  }

  return 0;
}


#ifdef DEBUG
static void print_tree(node *xnode, int visits) {
  //visit tree in infix order

  if (!xnode) return;

  if (!xnode->left) {
    fprintf(stderr, "%s", xnode->value);
    xnode->visited = visits;
    return;
  }

  if (xnode->left->visited < visits) {
    fprintf(stderr, "(");
    print_tree(xnode->left, visits);
  }
  if (xnode->visited < visits) printf("%s", xnode->value);
  xnode->visited = visits;
  if (xnode->right->visited < visits) {
    print_tree(xnode->right, visits);
    fprintf(stderr, ")");
  }
}
#endif


static int preproc(const char *exp) {
  // put parens around *, /
  char tmp[65536];
  char lastop = 0;
  size_t len = strlen(exp);
  int i, nstart = -1, plevel = 0;

  for (i = 0; i < len; i++) {
    switch (exp[i]) {
    case '(':
      nstart = i;
    case '[':
      i += preproc(&exp[i + 1]);
      len = strlen(exp);
      break;
    case ')':
    case ']':
      i++;
      goto preprocdone;
    case '0': case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case 'i': case 's':
      if (nstart == -1) nstart = i;
      break;
    case '+': case '-':
      if (nstart == -1) {
        nstart = i;
        break;
      }
      if (plevel > 0 && (lastop == '*' || lastop == '/')) {
        // close parens
        if (nstart == -1) break;
        sprintf(tmp, "%s", exp);
        sprintf(tmp + i, ")%s", exp + i);
        sprintf((char *)exp, "%s", tmp);
        len++;
        i++;
        plevel--;
      }
      lastop = exp[i];
      nstart = -1;
      break;
    case '*':
    case '/':
      if (lastop == '+' || lastop == '-') {
        // open parens
        sprintf(tmp, "%s", exp);
        sprintf(tmp + nstart, "(%s", exp + nstart);
        sprintf((char *)exp, "%s", tmp);
        len++;
        i++;
        nstart = -1;
        plevel++;
      }
      lastop = exp[i];
      break;
    default:
      break;
    }
  }

preprocdone:

  // close any open parens
  if (plevel > 0) {
    sprintf(tmp, "%s", exp);
    for (i = 0; i < plevel; i++) {
      sprintf(tmp + len + i, ")");
    }
    sprintf((char *)exp, "%s", tmp);
    i = strlen(exp);
  }

  return i;
}


static double evaluate(const char *exp, _sdata *sdata) {
  double res;

  sdata->error = 0;
  rootnode = NULL;

  preproc(exp);

#ifdef DEBUG
  printf("preproc is %s\n", exp);
#endif

  sdata->error = exp_to_tree(exp);
  if (sdata->error > 0) return 0.;

#ifdef DEBUG
  fprintf(stderr, "\nExp is:\n");
  print_tree(rootnode, 2);
  fprintf(stderr, "\n\n");
#endif

  simplify(rootnode, sdata);

#ifdef DEBUG
  fprintf(stderr, "\nSimplified result is:\n");
  print_tree(rootnode, 3);
  fprintf(stderr, "\n\nOK\n");
#endif

  if (sdata->error) return 0.;

  if (!strncmp(rootnode->value, "inf", 3)) {
    sdata->error = 2;
    return 0.;
  }

  res = strtod(rootnode->value, NULL);

  free_all(rootnode);

  return res;
}


//////////////////////////////////////////////////////////////////////////////////

static weed_error_t dataproc_init(weed_plant_t *inst) {
  _sdata *sdata = (_sdata *)weed_calloc(1, sizeof(_sdata));

  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;
  sdata->store = weed_calloc(NSTORE, sizeof(double));

  if (!sdata->store) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  weed_set_instance_data(inst, sdata);

  return WEED_SUCCESS;
}


static weed_error_t dataproc_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  _sdata *sdata = weed_get_instance_data(inst, sdata);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);

  double res = 0.;

  char *ip = NULL, *ptr;

  char exp[MAX_EXP_LEN];

  size_t len;

  int which;

  register int i;

  sdata->params = in_params;

  for (i = EQS; i < EQN; i++) {
    if (ip) weed_free(ip);
    ip = weed_get_string_value(in_params[i], WEED_LEAF_VALUE, NULL);

    if (!strlen(ip)) continue;

    ptr = index(ip, '=');
    if (!ptr) {
      fprintf(stderr, "data_processor ERROR: eqn %d has no '='\n", i - EQS);
      continue;
    }

    if (strncmp(ip, "s", 1) && strncmp(ip, "o", 1)) {
      fprintf(stderr, "data_processor ERROR: eqn %d must set s or o value\n", i - EQS);
      continue;
    }

    if (strncmp(ip + 1, "[", 1) || strncmp(ptr - 1, "]", 1)) {
      fprintf(stderr, "data_processor ERROR: eqn %d has invalid []\n", i - EQS);
      continue;
    }

    if (strlen(ptr + 1) >= MAX_EXP_LEN) {
      fprintf(stderr, "data_processor ERROR: eqn %d has too long expression\n", i - EQS);
      continue;
    }

    len = ptr - ip - 2;

    if (len < 1) {
      fprintf(stderr, "data_processor ERROR: eqn %d has invalid []\n", i - EQS);
      continue;
    }

    if (len >= MAX_EXP_LEN) {
      fprintf(stderr, "data_processor ERROR: eqn %d has too long []\n", i - EQS);
      continue;
    }

    sdata->error = 0;

    snprintf(exp, len, "%s", ip + 2);
    which = (int)evaluate(exp, sdata);

    if (!strncmp(ip, "o[", 2)) {
      if (which >= EQN - EQS) {
        sdata->error = 3;
      }
    }

    else if (!strncmp(ip, "s[", 2)) {
      if (which >= NSTORE) {
        sdata->error = 4;
      }
    }

    if (!sdata->error) {
      sprintf(exp, "%s", ptr + 1);
      res = evaluate(exp, sdata);
    } else sdata->error += 100;

    if (sdata->error) {
      switch (sdata->error) {
      case 1:
        sprintf(exp, "%s", ptr + 1);
        fprintf(stderr, "data_processor ERROR: syntax error in RHS of eqn %d\n%s\n", i - EQS, exp);
        break;
      case 2:
        fprintf(stderr, "data_processor ERROR: division by 0 in RHS of eqn %d\n", i - EQS);
        break;
      case 3:
        fprintf(stderr, "data_processor ERROR: i array out of bounds in RHS of eqn %d\n", i - EQS);
        break;
      case 4:
        fprintf(stderr, "data_processor ERROR: s array out of bounds in RHS of eqn %d\n", i - EQS);
        break;
      case 5:
        fprintf(stderr, "data_processor ERROR: expanded expression too long in RHS of eqn %d\n", i - EQS);
        break;
      case 101:
        snprintf(exp, len, "%s", ip + 2);
        fprintf(stderr, "data_processor ERROR: syntax error in LHS of eqn %d\n%s\n", i - EQS, exp);
        break;
      case 102:
        fprintf(stderr, "data_processor ERROR: division by 0 in LHS of eqn %d\n", i - EQS);
        break;
      case 103:
        fprintf(stderr, "data_processor ERROR: o array out of bounds in LHS of eqn %d\n", i - EQS);
        break;
      case 104:
        fprintf(stderr, "data_processor ERROR: s array out of bounds in LHS of eqn %d\n", i - EQS);
        break;
      case 105:
        fprintf(stderr, "data_processor ERROR: expanded expression too long in LHS of eqn %d\n", i - EQS);
        break;
      default:
        break;
      }
    }

    else {
      if (!strncmp(ip, "s", 1)) {
        sdata->store[which] = res;
      } else {
        weed_set_double_value(out_params[which], WEED_LEAF_VALUE, res);
      }
    }
  }

  if (ip) weed_free(ip);

  weed_free(in_params);
  weed_free(out_params);

  return WEED_SUCCESS;
}


static weed_error_t dataproc_deinit(weed_plant_t *inst) {
  _sdata *sdata = weed_get_instance_data(inst, sdata);
  if (sdata) {
    if (sdata->store) weed_free(sdata->store);
    weed_free(sdata);
  }
  weed_set_instance_data(inst, NULL);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class, *gui;

  weed_plant_t *in_params[EQN + 1];
  weed_plant_t *out_params[EQN - EQS + 1];

  register int i;

  char name[256];
  char name2[256];
  char label[256];
  char desc[1024];

  for (i = 0; i < EQS; i++) {
    snprintf(name, 256, "input%03d", i);
    in_params[i] = weed_float_init(name, "", 0., -1000000000000., 1000000000000.);

    gui = weed_paramtmpl_get_gui(in_params[i]);
    weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
  }

  for (i = EQS; i < EQN; i++) {
    snprintf(name, 256, "equation%03d", i - EQS);
    snprintf(label, 256, "Equation %03d", i - EQS);
    snprintf(name2, 256, "output%03d", i - EQS);
    in_params[i] = weed_text_init(name, label, "");
    out_params[i - EQS] = weed_out_param_float_init_nominmax(name2, 0.);
  }

  in_params[EQN] = NULL;
  out_params[EQN - EQS] = NULL;

  filter_class = weed_filter_class_init("data_processor", "salsaman", 1, 0, NULL,
                                        dataproc_init, dataproc_process, dataproc_deinit, NULL, NULL, in_params, out_params);

  snprintf(desc, 1024,
           "Produce (double) output values o[] from a combination of in values i[], stored values s[],\n"
           "and arithmetic expressions."
           "The LHS of each equation must be either an o element or an s element.\n"
           "The right hand side may be composed of s elements, i elements, o elements and "
           "any valid combination of the symbols + - / * and ( )\n"
           "E.g:\n\n"
           "    o[0]=s[0]-0.5\n    s[0]=i[0]*4.2\n\n"
           "Whitespace in equations is not permitted (PATCHME !).\n"
           "Array subscripts range from 0 to %d\n"
           "The values of the s array are initialized to 0., and retained between processing cycles\n"
           "The values of the i array are updated each cycle from the values of plugin's %d (hidden) input parameters,\n"
           "and the values of the o array after evaluating all the equations are replicated to the corresponding out parameters.\n"
           "Equations are processed in sequence from 000 to %03d on each cycle; empty equation strings are ignored.\n"
           , EQS - 1, EQS, EQN - EQS);

  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);
  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;
