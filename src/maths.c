// maths.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

LIVES_GLOBAL_INLINE uint64_t lives_10pow(int pow) {
  uint64_t res = 1;
  for (int i = 0; i < pow; i++) res *= 10;
  return res;
}


LIVES_GLOBAL_INLINE double lives_fix(double val, int decimals) {
#ifdef _GNU_SOURCE
  double factor = exp10((double)decimals);
#else
  double factor = (double)lives_10pow(decimals);
#endif
  if (val >= 0.) return (double)((int)(val * factor + 0.4999999)) / factor;
  return (double)((int)(val * factor - 0.49999999)) / factor;
}


// bell curve with its peak at x = m, spread/standard deviation of s1 to the left of the mean,
// spread of s2 to the right of the mean, and scaling parameter a.
LIVES_GLOBAL_INLINE double gaussian(double x, double a, double m, double s1, double s2) {
  double t = (x - m) / (x < m ? s1 : s2);
  return a * exp(-(t * t) / 2.);
}

LIVES_GLOBAL_INLINE uint32_t get_2pow(uint32_t x) {
  // return largest 2 ** n <= x
  x |= (x >> 1); x |= (x >> 2); x |= (x >> 4); x |= (x >> 8); x |= (x >> 16);
  return (++x) >> 1;
}

LIVES_GLOBAL_INLINE uint64_t get_2pow_64(uint64_t x) {
  x |= (x >> 1); x |= (x >> 2); x |= (x >> 4); x |= (x >> 8); x |= (x >> 16); x |= (x >> 32);
  return (++x) >> 1;
}

#define get_log2_8(x) ((x) >= 128 ? 7 : (x) >= 64 ? 6 : (x) >= 32 ? 5 : (x) >= 16 ? 4 \
		       : (x) >= 8 ? 3 : (x) >= 4 ? 2 : (x) >= 2 ? 1 : 0)

LIVES_LOCAL_INLINE uint16_t get_log2_16(uint16_t x) {
  if (!x) return 0;
  if (x & 0xFF00) return get_log2_8((x & 0xFF00) >> 8) + 8;
  return get_log2_8(x & 0xFF);
}

LIVES_GLOBAL_INLINE uint32_t get_log2(uint32_t x) {
  if (x & 0xFFFF0000) return get_log2_16((x & 0xFFFF0000) >> 16) + 16;
  return get_log2_16(x & 0xFFFF);
}

LIVES_GLOBAL_INLINE uint64_t get_log2_64(uint64_t x) {
  if (x & 0xFFFFFFFF00000000) return get_log2((x & 0xFFFFFFFF00000000) >> 32) + 32;
  return get_log2(x & 0xFFFFFFFF);
}

LIVES_GLOBAL_INLINE float get_approx_ln(uint32_t x) {
  return (float)get_log2(x) / LN_CONSTVAL;
}

LIVES_GLOBAL_INLINE double get_approx_ln64(uint64_t x) {
  return (double)get_log2_64(x) / LN_CONSTVAL;
}

LIVES_GLOBAL_INLINE uint64_t get_near2pow(uint64_t val) {
  uint64_t low = get_2pow(val);
  g_print("low = %lu, val - low = %lu, <<1 = %lu\n", low, val - low, (val - low) << 1);
  if (((val - low) << 1) > val) return low << 1;
  return low;
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

int lcm(int x, int y, int max) {
  // find the lowest common multiple of a and b, if it is > max, return 0
  if (y >= x) {
    if (x == y) return x;
    if (x <= 0 || y > max) return 0;
  } else {
    int a;
    if (y <= 0 || x > max) return 0;
    a = y;
    y = x;
    x = a;
  }
  for (int val = x, rem = y - x; val <= max; val += x) {
    if (rem == y) return val;
    rem += x;
    if (rem > y) rem -= y;
  }
  return max;
}


/* start with the number line 0. / 1. to 1. / 1. (a = 0., b = 1., c = 1., d = 1.) */
/* then: take the fraction (a + c) / (b + d), if this is > val, then this becomes new max */
/* if this is < val, then this becomes new min */
/* if equal->LIMIT val then this is our estimate fraction */
/* else, we take the mid value and check...eg. min is a / b  max is c / d, want (a / b + c / d) / 2 */

/* after timeout (cycles) will return FALSE */

static boolean est_fraction(double val, uint32_t *numer, uint32_t *denom, double limit, int cycles) {
  double res;
  int a = 0, b = 1, c = 1, d = 1, m, n, i;
  for (i = 0; i < cycles; i++) {
    m = a + b;
    n = c + d;
    res = (double)m / (double)n;
    if (fabs(res - val) <= limit) break;
    if (res > val) {
      b = m;
      d = n;
    } else {
      a = m;
      c = n;
    }
  }
  *numer = m;
  *denom = n;
  if (i < cycles) return TRUE;
  return FALSE;
}


/**
   @brief return ratio fps (TRUE) or FALSE
   we want to see if we can express fps as n : m
   where n and m are integers, and m is close to a power of 10.

   step 1: fps' = fps / (fps + 1.)

   step 4: find next power of 10 (curt) above x. mpy by (fps + 1.)
           since fps / (fps + 1.) = x / y, y = x * (fps + 1.) / fps = x / fps'
   step 5: return TRUE and values (fps + 1) * curt : curt / fps'
*/
boolean calc_ratio_fps(double fps, int *numer, int *denom) {
  // inverse of get_ratio_fps
  double res, fpsr, curt = 10., diff;
  uint32_t m, n;

  fpsr = (double)((int)(fps + 1.));
  fps /= fpsr;

  est_fraction(fps, &m, &n, 0.00000001, 10000);

  // now we have our answer, m / n, e.g 999 / 1000 ( * 30. = fps)
  // but we want m to be a power of 10 (and it must be close, within say 1%)
  while (1) {
    diff = (double)m / curt;
    res = (double)m / (double)n;
    if (diff >= 0.99 && diff <= 1.01) {
      if (numer) *numer = (int)(fpsr * curt);
      if (denom) *denom = (int)(curt / res);
      return TRUE;
    }
    if (curt > (double)m) return FALSE;
    curt *= 10.;
  }
}



// statistics

/* static boolean inited = FALSE; */
/* //static const lives_object_template_t tmpls[1]; */

/* static void init_templates(void) { */
/*   lives_object_template_t tmpl; */
/*   if (inited) return; */
/*   inited = TRUE; */
/*   tmpl = tmpls[0]; */
/*   tmpl.uid = tmpl.subtype = SUBTYPE_STATS; */
/*   tmpl.type = OBJECT_TYPE_MATH; */
/* } */


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
      tx->prereqs->reqs->params = (lives_tx_param_t **)lives_calloc(sizeof(lives_tx_param_t *), 4);

      tx->prereqs->reqs->params[0] = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM);
      weed_set_string_value(tx->prereqs->reqs->params[0], WEED_LEAF_NAME, MATH_PARAM_DATA);
      weed_set_int_value(tx->prereqs->reqs->params[0], WEED_LEAF_PARAM_TYPE, WEED_PARAM_UNSPECIFIED);

      tx->prereqs->reqs->params[1] = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM);
      weed_set_string_value(tx->prereqs->reqs->params[1], WEED_LEAF_NAME, MATH_PARAM_DATA_SIZE);
      weed_set_int_value(tx->prereqs->reqs->params[1], WEED_LEAF_PARAM_TYPE, WEED_PARAM_INTEGER);

      tx->prereqs->reqs->params[2] = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM);
      weed_set_string_value(tx->prereqs->reqs->params[2], WEED_LEAF_NAME, MATH_PARAM_VALUE);
      weed_set_int_value(tx->prereqs->reqs->params[2], WEED_LEAF_PARAM_TYPE, WEED_PARAM_FLOAT);

      tx->prereqs->reqs->params[3] = 0;
      return tx;
    }
    return NULL;
  }
  return NULL;
}


/* const lives_object_template_t *maths_object_with_subtype(uint64_t subtype) { */
/*   if (!inited) init_templates(); */
/*   if (subtype == MATH_OBJECT_SUBTYPE_STATS) return &tmpls[SUBTYPE_STATS]; */
/*   return NULL; */
/* } */

