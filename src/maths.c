// maths.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

LIVES_GLOBAL_INLINE uint64_t lives_10pow(int pow) {
  register int i;
  uint64_t res = 1;
  for (i = 0; i < pow; i++) res *= 10;
  return res;
}


LIVES_GLOBAL_INLINE double lives_fix(double val, int decimals) {
  double factor = (double)lives_10pow(decimals);
  if (val >= 0.) return (double)((int)(val * factor + 0.5)) / factor;
  return (double)((int)(val * factor - 0.5)) / factor;
}


// bell curve with its peak at x = m, spread/standard deviation of s1 to the left of the mean,
// spread of s2 to the right of the mean, and scaling parameter a.
LIVES_GLOBAL_INLINE double gaussian(double x, double a, double m, double s1, double s2) {
  double t = (x - m) / (x < m ? s1 : s2);
  return a * exp(-(t * t) / 2.);
}


LIVES_GLOBAL_INLINE uint32_t get_approx_ln(uint32_t x) {
  x |= (x >> 1); x |= (x >> 2); x |= (x >> 4); x |= (x >> 8); x |= (x >> 16);
  return (++x) >> 1;
}

LIVES_GLOBAL_INLINE uint64_t get_approx_ln64(uint64_t x) {
  x |= (x >> 1); x |= (x >> 2); x |= (x >> 4); x |= (x >> 8); x |= (x >> 16); x |= (x >> 32);
  return (++x) >> 1;
}

LIVES_GLOBAL_INLINE uint64_t get_near2pow(uint64_t val) {
  uint64_t low = get_approx_ln64(val), high = low * 2;
  if (high < low || (val - low < high - val)) return low;
  return high;
}


/* convert to/from a big endian 32 bit float for internal use */
LIVES_GLOBAL_INLINE float LEFloat_to_BEFloat(float f) {
  if (capable->hw.byte_order == LIVES_LITTLE_ENDIAN) swab4(&f, &f, 1);
  return f;
}

static int get_hex_digit(const char c) GNU_CONST;

static int get_hex_digit(const char c) {
  switch (c) {
  case 'a': case 'A': return 10;
  case 'b': case 'B': return 11;
  case 'c': case 'C': return 12;
  case 'd': case 'D': return 13;
  case 'e': case 'E': return 14;
  case 'f': case 'F': return 15;
  default: return c - 48;
  }
}

LIVES_GLOBAL_INLINE int hextodec(const char *string) {
  int tot = 0;
  for (char c = *string; c; c = *(++string)) tot = (tot << 4) + get_hex_digit(c);
  return tot;
}


// statistics

static boolean inited = FALSE;
static const lives_object_template_t tmpls[1];

static void init_templates(void) {
  lives_object_template_t tmpl;
  if (inited) return;
  inited = TRUE;
  tmpl = tmpls[0];
  tmpl.uid = tmpl.subtype = SUBTYPE_STATS;
  tmpl.type = OBJECT_TYPE_MATH;
}


typedef struct {
  int nvals, maxsize;
  size_t fill;
  float **res;
} tab_data;

// to init, call twice with newval NULL, 1st call sets nvals from idx, second sets maxsize
// the call with data in newval and idx from 0 - nvals, neval will be replaced withh running avg.
// tab_data is used by the functionand not to be messed with
size_t running_average(float *newval, int idx, void **data) {
  size_t nfill;
  if (!data) return 0;
  else {
    tab_data **tdatap = (tab_data **)data;
    tab_data *tdata = *tdatap;
    float tot = 0.;
    if (!newval) {
      if (!tdata) {
        tdata = (tab_data *)lives_calloc(sizeof(tab_data), 1);
        *tdatap = tdata;
      }
      if (!tdata->nvals) {
        tdata->nvals = idx;
        tdata->res = (float **)lives_calloc(sizeof(float *), tdata->nvals);
      } else {
        tdata->maxsize = idx;
        for (int i = 0; i < tdata->nvals; i++) {
          tdata->res[i] = (float *)lives_calloc(4, tdata->maxsize + 1);
        }
      }
      return 0;
    }
    tot = tdata->res[idx][tdata->maxsize];
    if (tdata->fill > tdata->maxsize - 2) {
      tot -= tdata->res[idx][0];
      lives_memmove(&tdata->res[idx][0], &tdata->res[idx][1], (tdata->maxsize - 1) * 4);
    }
    tot += *newval;
    tdata->res[idx][tdata->fill] = *newval;
    tdata->res[idx][tdata->maxsize] = tot;
    *newval = tot / (tdata->fill + 1);
    nfill = tdata->fill + 1;
    if (idx == tdata->nvals - 1 && tdata->fill < tdata->maxsize - 1) tdata->fill++;
  }
  return nfill;
}


lives_object_transform_t *math_transform_for_intent(lives_object_t *obj, lives_intention intent) {
  if (obj->subtype == MATH_OBJECT_SUBTYPE_STATS) {
    if (intent == MATH_INTENTION_DEV_FROM_MEAN) {
      //lives_req_t *req;
      lives_object_transform_t *tx =
        (lives_object_transform_t *)lives_calloc(sizeof(lives_object_transform_t), 1);
      tx->prereqs = (lives_rules_t *)lives_calloc(sizeof(lives_rules_t), 1);
      tx->prereqs->reqs = (lives_intentparams_t *)lives_calloc(sizeof(lives_intentparams_t), 1);
      tx->prereqs->reqs->params = (lives_obj_param_t **)lives_calloc(sizeof(lives_obj_param_t *), 4);

      tx->prereqs->reqs->params[0] = lives_plant_new(LIVES_WEED_SUBTYPE_OBJ_PARAM);
      weed_set_string_value(tx->prereqs->reqs->params[0], WEED_LEAF_NAME, MATH_PARAM_DATA);
      weed_set_int_value(tx->prereqs->reqs->params[0], WEED_LEAF_PARAM_TYPE, WEED_PARAM_UNSPECIFIED);

      tx->prereqs->reqs->params[1] = lives_plant_new(LIVES_WEED_SUBTYPE_OBJ_PARAM);
      weed_set_string_value(tx->prereqs->reqs->params[1], WEED_LEAF_NAME, MATH_PARAM_DATA_SIZE);
      weed_set_int_value(tx->prereqs->reqs->params[1], WEED_LEAF_PARAM_TYPE, WEED_PARAM_INTEGER);

      tx->prereqs->reqs->params[2] = lives_plant_new(LIVES_WEED_SUBTYPE_OBJ_PARAM);
      weed_set_string_value(tx->prereqs->reqs->params[2], WEED_LEAF_NAME, MATH_PARAM_VALUE);
      weed_set_int_value(tx->prereqs->reqs->params[2], WEED_LEAF_PARAM_TYPE, WEED_PARAM_FLOAT);

      tx->prereqs->reqs->params[3] = 0;
      return tx;
    }
    return NULL;
  }
  return NULL;
}


const lives_object_template_t *maths_object_with_subtype(uint64_t subtype) {
  if (!inited) init_templates();
  if (subtype == MATH_OBJECT_SUBTYPE_STATS) return &tmpls[SUBTYPE_STATS];
  return NULL;
}

