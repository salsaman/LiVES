// maths.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _MATHS_H_
#define _MATHS_H_

#ifdef MIN
#undef MIN
#endif
#define MIN(a,b) ((a) <= (b) ? (a) : (b))
#ifdef MAX
#undef MAX
#endif
#define MAX(a,b) ((a) >= (b) ? (a) : (b))

// math macros / functions
#define MASK64_h32	0xFFFFFFFF00000000
#define MASK64_l32	0x00000000FFFFFFFF
#define MASK64_hh16	0xFFFF000000000000
#define MASK64_hl16	0x0000FFFF00000000
#define MASK64_lh16	0x00000000FFFF0000
#define MASK64_ll16	0x0000FFFF00000000

#define MASK64_hhh8	0xFF00000000000000
#define MASK64_hhl8	0x00FF000000000000
#define MASK64_hlh8	0x0000FF0000000000
#define MASK64_hll8	0x000000FF00000000
#define MASK64_lhh8	0x00000000FF000000
#define MASK64_lhl8	0x0000000000FF0000
#define MASK64_llh8	0x000000000000FF00
#define MASK64_lll8	0x00000000000000FF

#define MASK64_01_x8	0x0101010101010101
#define MASK64_80_x8	0x8080808080808080

#define MASK64_x8(n)	0x##n##n##n##n##n##n##n##n

// md5sum ////////////////////////////////////////////////////////////////////////////////////////////////////
#define _A(b,c,d)(d^(b&(c^d)))
#define _B(b,c,d)_A(d,b,c)
#define _C(b,c,d)(b^c^d)
#define _D(b,c,d)(c^(b|~d))
#define A_(w,s)(w=(w<<s)|(w>>(32-s)))
#define B_(g,h,i,j,s,T)do{g+=_A(h,i,j)+(*_X++=*w)+T;++w;A_(g,s);g+=h;}while(0)
#define BX(W,X,Y,Z)B_(A,B,C,D,7,W);B_(B,C,D,A,22,X);B_(C,D,A,B,17,Y);B_(D,A,B,X,12,Z);
#define C_(f,k,s,T,g,h,i,j)do{g+=f(h,i,j)+X[k]+T;A_(g,s);g+=h;}while(0)
#define CA_(M,w,W,x,X,y,Y,z,Z,g,h,i,j)C_(M,w,g,W,A,B,C,D);\
  C_(M,x,h,X,D,A,B,C);C_(M,y,i,Y,C,D,A,B);C_(M,z,j,Z,B,C,D,A);
#define CW(...)CA_(_B,__VA_ARGS__,5,9,14,20)
#define CX(...)CA_(_C,__VA_ARGS__,4,11,16,23)
#define CY(...)CA_(_D,__VA_ARGS__,6,10,15,21)
typedef struct {uint32_t A, B, C, D, t[2], bl; char buf[128];} md5priv;
#define MD5_VALUE_CALCULATION_MAGIC BX(0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee)BX(0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501)BX(0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be)BX(0x6b901122,0xfd987193,0xa679438e,0x49b40821)CW(1,0xf61e2562,6,0xc040b340,11,0x265e5a51,0,0xe9b6c7aa)CW(5,0xd62f105d,10,0x02441453,15,0xd8a1e681,4,0xe7d3fbc8)CW(9,0x21e1cde6,14,0xc33707d6,3,0xf4d50d87,8,0x455a14ed)CW(13,0xa9e3e905,2,0xfcefa3f8,7,0x676f02d9,12,0x8d2a4c8a)CX(5,0xfffa3942,8,0x8771f681,11,0x6d9d6122,14,0xfde5380c)CX(1,0xa4beea44,4,0x4bdecfa9,7,0xf6bb4b60,10,0xbebfbc70)CX(13,0x289b7ec6,0,0xeaa127fa,3,0xd4ef3085,6,0x04881d05)CX(9,0xd9d4d039,12,0xe6db99e5,15,0x1fa27cf8,2,0xc4ac5665)CY(0,0xf4292244,7,0x432aff97,14,0xab9423a7,5,0xfc93a039)CY(12,0x655b59c3,3,0x8f0ccc92,10,0xffeff47d,1,0x85845dd1)CY(8,0x6fa87e4f,15,0xfe2ce6e0,6,0xa3014314,13,0x4e0811a1)CY(4,0xf7537e82,11,0xbd3af235,2,0x2ad7d2bb,9,0xeb86d391);
#define MD5_SIZE 16
// returns array of uint8_t output[16]
uint8_t *tinymd5(void *data, size_t dsize) WARN_UNUSED LIVES_PURE;
uint64_t minimd5(void *data, size_t dsize) LIVES_PURE;
void *lives_md5_sum(const char *filename, int64_t *size);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SHA512_SIZE	64
#define SHA512_BSIZE	128
typedef struct {
  uint64_t H[8], data_len[2];
  uint8_t block_len, block[SHA512_BSIZE], digest[SHA512_SIZE];
} sha512priv;

#define Ch(x,y,z)((x&y)^(~x&z))
#define Maj(x,y,z)((x&y)^(x&z)^(y&z))
#define SHR(n,x)(x>>n)
#define ROTR(n,x)(SHR(n,x)|(x<<(64-n)))
#define SIGMA0(x)(ROTR(28,x)^ROTR(34,x)^ROTR(39,x))
#define SIGMA1(x)(ROTR(14,x)^ROTR(18,x)^ROTR(41,x))
#define sigma0(x)(ROTR(1,x)^ROTR(8,x)^SHR(7,x))
#define sigma1(x)(ROTR(19,x)^ROTR(61,x)^SHR(6,x))
#define squared(a)((a)*(a))

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

float LEFloat_to_BEFloat(float f) LIVES_CONST;
uint64_t lives_10pow(int pow) LIVES_CONST;
double lives_fix(double val, int decimals) LIVES_CONST;

uint32_t get_2pow(uint32_t val) LIVES_CONST;
uint64_t get_2pow_64(uint64_t x)LIVES_CONST;

uint64_t get_near2pow(uint64_t val) LIVES_CONST;

uint32_t get_log2(uint32_t val) LIVES_CONST;
uint32_t get_log2_64(uint64_t x) LIVES_CONST;

int get_onescount_64(uint64_t num);
int get_onescount_32(uint32_t num);
int get_onescount_16(uint16_t num);
int get_onescount_8(uint8_t num);

uint64_t factorial(int n);
uint64_t factorial_div(int n, int m);

double binomial(int ntrials, int nhits, double ps);

#define LN_CONSTVAL 1.4427 // 1 / ln(2)

float get_approx_ln(uint32_t val) LIVES_CONST;
double get_approx_ln64(uint64_t x) LIVES_CONST;

double binomial(int hits, int trials, double phit);

int lcm(int x, int y, int max);

uint64_t nxtval(uint64_t val, uint64_t lim, boolean less);
uint64_t get_satisfactory_value(uint64_t val, uint64_t lim, boolean less);

double gaussian(double x, double a, double m, double s1, double s2);

int hextodec(const char *string);

uint32_t make_bitmask32(int hi, int lo);
uint32_t get_bits32(uint32_t bytes, int hi, int lo);

//flagbits for ann->flags
#define ANN_TRAINED		(1ull << 1)

typedef struct {
  int nweights, nnodes;
  double *weights, *bweights;
  double *biases, *bbiases;
  double *values;
  int nlayers;
  int nrnds;
  int *lcount;
  int ndatapoints;
  LiVESList *last_data;
  LiVESList *tstdata;
  double last_res;
  double maxr, maxrb, maxw, damp;
  uint64_t flags;
  int nvary;
  int no_change_count;
  int generations;
} lives_ann_t;

typedef struct {
  double *inputs;
  double res;
} ann_testdata_t;

lives_ann_t *lives_ann_create(int nlayers, int *lcounts);
double lives_ann_predict_result(lives_ann_t *, ann_testdata_t *realdata);
double lives_ann_evolve(lives_ann_t *);
void lives_ann_init_seed(lives_ann_t *, double maxw);
void lives_ann_set_variance(lives_ann_t *, double maxr, double maxrb, double damp, int nrnds);
void lives_ann_free(lives_ann_t *);

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

const lives_obj_instance_t *maths_object_with_subtype(uint64_t subtype);
lives_object_transform_t *math_transform_for_intent(lives_obj_t *obj, lives_intention intent);

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
