// maths.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _MATHS_H_
#define _MATHS_H_

// math macros / functions

#define squared(a) ((a) * (a))

#define sig(a) ((a) < 0. ? -1.0 : 1.0)

// round to nearest integer
#define ROUND_I(a) ((int)((double)(a) + .5))

// clamp a between 0 and b; both values rounded to nearest int
#define NORMAL_CLAMP(a, b) (ROUND_I((a))  < 0 ? 0 : ROUND_I((a)) > ROUND_I((b)) ? ROUND_I((b)) : ROUND_I((a)))

// clamp a between 1 and b; both values rounded to nearest int; if rounded value of a is <= 0, return rounded b
#define UTIL_CLAMP(a, b) (NORMAL_CLAMP((a), (b)) <= 0 ? ROUND_I((b)) : ROUND_I((a)))

// normal integer clamp
#define INT_CLAMP(i, min, max) ((i) < (min) ? (min) : (i) > (max) ? (max) : (i))

// round a up double / float a to  next multiple of int b
#define CEIL(a, b) ((int)(((double)(a) + (double)(b) - .000000001) / ((double)(b))) * (b))

// round int a up to next multiple of int b, unless a is already a multiple of b
#define ALIGN_CEIL(a, b) (((int)(((a) + (b) - 1.) / (b))) * (b))

// round int a up to next multiple of int b, unless a is already a multiple of b
#define ALIGN_CEIL64(a, b) ((((int64_t)(a) + (int64_t)(b) - 1) / (int64_t)(b)) * (int64_t)(b))

// round float / double a down to nearest multiple of int b
#define FLOOR(a, b) ((int)(((double)(a) - .000000001) / ((double)(b))) * (b))

// floating point division, maintains the sign of the dividend, regardless of the sign of the divisor
#define SIGNED_DIVIDE(a, b) ((a) < 0. ? -fabs((a) / (b)) : fabs((a) / (b)))

// using signed ints, the first part will be 1 iff -a < b, the second iff a > b, equivalent to abs(a) > b
#define ABS_THRESH(a, b) (((a) + (b)) >> 31) | (((b) - (a)) >> 31)

#define myround(n) ((n) >= 0. ? (int)((n) + 0.5) : (int)((n) - 0.5))

float LEFloat_to_BEFloat(float f) GNU_CONST;
uint64_t lives_10pow(int pow) GNU_CONST;
double lives_fix(double val, int decimals) GNU_CONST;
uint32_t get_approx_ln(uint32_t val) GNU_CONST;
uint64_t get_approx_ln64(uint64_t x)GNU_CONST;
uint64_t get_near2pow(uint64_t val) GNU_CONST;

double gaussian(double x, double a, double m, double s1, double s2);

int hextodec(const char *string);

//// statistics ///
#define OBJECT_TYPE_MATH		IMkType("obj.MATH")
#define MATH_OBJECT_SUBTYPE_STATS	IMkType("MATHstat")

enum {
  SUBTYPE_STATS,
};

#define MATH_PARAM_DATA "data"
#define MATH_PARAM_DATA_SIZE "data_size"
#define MATH_PARAM_VALUE "value"

#define MATH_PARAM_RESULT "result"

enum {
  MATH_INTENTION_RUNNING_AVG = 1,
  MATH_INTENTION_DEV_FROM_MEAN
};

const lives_object_template_t *maths_object_with_subtype(uint64_t subtype);
lives_object_transform_t *math_transform_for_intent(lives_object_t *obj, lives_intention intent);

// to init, call twice with newval NULL, 1st call sets nvals from idx, second sets maxsize
// the call with data in newval and idx from 0 - nvals, neval will be replaced withh running avg.
// data is used by the functions and not to be messed with:
//
// static stats_pkt stats = NULL;
// runung_average(NULL, 4, &stats);   // analyze 4 values
// runung_average(NULL, 64, &stats); // use sliding window size 64

// then:
// float fval = 0.5;
// size_t nentries = running_average(&fval, 0, &stats);
// appends value 0.5 to the series for variable 0. and returns the running avg in fval.

size_t running_average(float *newval, int idx, void **data);

#endif
