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
#ifdef _LIVES_SOURCE
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


LIVES_GLOBAL_INLINE int get_onescount_8(uint8_t num) {
  int tot = 0;
  for (uint8_t x = 128; x; x >>= 1) if (num & x) tot++;
  return tot;
}


LIVES_GLOBAL_INLINE int get_onescount_16(uint16_t num) {
  return get_onescount_8((uint16_t)((num & 0xFF00) >> 8))
    + get_onescount_8((uint16_t)((num & 0xFF)));
}

LIVES_GLOBAL_INLINE int get_onescount_32(uint32_t num) {
  return get_onescount_16((uint16_t)((num & 0xFFFF0000) >> 16))
    + get_onescount_16((uint16_t)((num & 0xFFFF)));
}

LIVES_GLOBAL_INLINE int get_onescount_64(uint64_t num) {
  return get_onescount_32((uint32_t)((num & 0xFFFFFFFF00000000) >> 32))
    + get_onescount_32((uint32_t)((num & 0xFFFFFFFF)));
}


static LIVES_CONST int get_hex_digit(const char c) {
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
    a = y; y = x; x = a;
  }
  for (int val = x, rem = y - x; val <= max; val += x) {
    if (rem == y) return val;
    rem += x;
    if (rem > y) rem -= y;
  }
  return max;
}



struct _decomp {
  uint64_t value;
  int i, j;
};

struct _decomp_tab {
  uint64_t value;
  int i, j;
  struct _decomp_tab *lower,  *higher;
};

static struct _decomp_tab nxttbl[64][25];
static boolean nxttab_inited = FALSE;

void make_nxttab(void) {
  LiVESList *preplist = NULL, *dccl, *dccl_last = NULL;
  uint64_t val6 = 1ul, val;
  struct _decomp *dcc;
  int max2pow, xi, xj;
  if (nxttab_inited) return;
  for (int j = 0; j < 25; j++) {
    val = val6;
    max2pow = 64 - ((j * 10 + 7) >> 2);
    dccl = preplist;
    for (int i = 0; i < max2pow; i++) {
      dcc = (struct _decomp *)lives_malloc(sizeof(struct _decomp));
      dcc->value = val;
      dcc->i = i;
      dcc->j = j;
      if (!preplist) dccl = preplist = lives_list_append(preplist, dcc);
      else {
        LiVESList *dccl2 = lives_list_append(NULL, (livespointer)dcc);
        for (; dccl; dccl = dccl->next) {
          dcc = (struct _decomp *)dccl->data;
          if (dcc->value > val) break;
          dccl_last = dccl;
        }
        if (!dccl) {
          dccl_last->next = dccl2;
          dccl2->prev = dccl_last;
          dccl2->next = NULL;
          dccl = dccl2;
        } else {
          dccl2->next = dccl;
          dccl2->prev = dccl->prev;
          if (dccl->prev) dccl->prev->next = dccl2;
          else preplist = dccl2;
          dccl->prev = dccl2;
        }
      }
      val *= 2;
    }
    val6 *= 6;
  }
  for (dccl = preplist; dccl; dccl = dccl->next) {
    dcc = (struct _decomp *)dccl->data;
    xi = dcc->i;
    xj = dcc->j;
    nxttbl[xi][xj].value = dcc->value;
    nxttbl[xi][xj].i = xi;
    nxttbl[xi][xj].j = xj;
    if (dccl->prev) {
      dcc = (struct _decomp *)dccl->prev->data;
      nxttbl[xi][xj].lower = &(nxttbl[dcc->i][dcc->j]);
    } else nxttbl[xi][xj].lower = NULL;
    if (dccl->next) {
      dcc = (struct _decomp *)dccl->next->data;
      nxttbl[xi][xj].higher = &(nxttbl[dcc->i][dcc->j]);
    } else nxttbl[xi][xj].higher = NULL;
  }
  lives_list_free_all(&preplist);
  nxttab_inited = TRUE;
}


uint64_t nxtval(uint64_t val, uint64_t lim, boolean less) {
  // to avoid only checking powers of 2, we want some number which is (2 ** i) * (6 ** j)
  // which gives a nice range of results
  uint64_t oval = val;
  int i = 0, j = 0;
  if (!nxttab_inited) make_nxttab();
  /// decompose val into i, j
  /// divide by 6 until val mod 6 is non zero
  if (val & 1) {
    if (less) val--;
    else val++;
  }
  for (; !(val % 6) && val > 0; j++, val /= 6);
  /// divide by 2 until we reach 1; if the result of a division is odd we add or subtract 1
  for (; val > 1; i++, val /= 2) {
    if (val & 1) {
      if (less) val--;
      else val++;
    }
  }
  val = nxttbl[i][j].value;
  if (less) {
    if (val == oval) {
      if (nxttbl[i][j].lower) val = nxttbl[i][j].lower->value;
    } else {
      while (nxttbl[i][j].higher->value < oval) {
        int xi = nxttbl[i][j].higher->i;
        val = nxttbl[i][j].value;
        j = nxttbl[i][j].higher->j;
        i = xi;
      }
    }
    return val > lim ? val : lim;
  }
  if (val == oval) {
    if (nxttbl[i][j].higher) val = nxttbl[i][j].higher->value;
  } else {
    while (nxttbl[i][j].lower && nxttbl[i][j].lower->value > oval) {
      int xi = nxttbl[i][j].lower->i;
      j = nxttbl[i][j].lower->j;
      i = xi;
      val = nxttbl[i][j].value;
    }
  }
  return val < lim ? val : lim;
}



// find a "satisfactory" value (my definition) - for a given valau
// return the nearest nummber which can be expressed as 2 ** i + 6 ** j
// we start making a first estimate. If x is odd we add or subtract one,
// then repeatedly divide by six until ww get a non divisible value.
// Then we take the remainder and keep dividing by 2, adding or subtracting in case we get a remainder 1
// with thees factors as a starting value we consult a lookup tabl and

uint64_t get_satisfactory_value(uint64_t val, uint64_t lim, boolean less) {
  // to avoid only checking powers of 2, we want some number which is (2 ** i) + (6 ** j)
  // which gives a nice range of results
  uint64_t oval = val;
  int i = 0, j = 0;
  if (!nxttab_inited) make_nxttab();
  /// decompose val into i, j
  /// divide by 6 until val mod 6 is non zero
  // add or subtract 1 to make the value even (since we know the final result must be a power of two or of 6)
  if (val & 1) {
    if (less) val--;
    else val++;
  }
  for (; !(val % 6) && val > 0; j++, val /= 6);
  /// divide by 2 until we reach 1; if the result of a division is odd we add or subtract 1
  for (; val > 1; i++, val /= 2) {
    if (val & 1) {
      if (less) val--;
      else val++;
    }
  }
  val = nxttbl[i][j].value;
  if (less) {
    if (val == oval) {
      if (nxttbl[i][j].lower) val = nxttbl[i][j].lower->value;
    } else {
      while (nxttbl[i][j].higher->value < oval) {
        int xi = nxttbl[i][j].higher->i;
        val = nxttbl[i][j].value;
        j = nxttbl[i][j].higher->j;
        i = xi;
      }
    }
    return val > lim ? val : lim;
  }
  if (val == oval) {
    if (nxttbl[i][j].higher) val = nxttbl[i][j].higher->value;
  } else {
    while (nxttbl[i][j].lower && nxttbl[i][j].lower->value > oval) {
      int xi = nxttbl[i][j].lower->i;
      j = nxttbl[i][j].lower->j;
      i = xi;
      val = nxttbl[i][j].value;
    }
  }
  return val < lim ? val : lim;
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
    m = a + b; n = c + d;
    res = (double)m / (double)n;
    if (fabs(res - val) <= limit) break;
    if (res > val) {
      b = m; d = n;
    } else {
      a = m; c = n;
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
   where n and m are integers, and m is close to a multiple of 10.
   so we would start with something like (F - b) / (F * 10. + c)    == 1.

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
    curt += 10.;
  }
}
///////////////////////////////////////////////////////////////////////////////
// MD5SUM functions - adapted from busybox source by Ulrich Drepper <drepper@gnu.ai.mit.edu>
static const uint8_t padding[64] = {0x80, 0};

LIVES_LOCAL_INLINE void lives_md5_start(md5priv *priv) {
  priv->A = 0x67452301; priv->B = 0xefcdab89; priv->C = 0x98badcfe; priv->D = 0x10325476;
  priv->t[0] = priv->t[1] = 0; priv->bl = 0;
}

// puts the final value in ret (a 16 byte array of uint8_t - (here cast to uint32_[4])
LIVES_LOCAL_INLINE void *lives_md5_read(md5priv *priv, void *ret) {
  ((uint32_t *) ret)[0] = priv->A; ((uint32_t *) ret)[1] = priv->B;
  ((uint32_t *) ret)[2] = priv->C; ((uint32_t *) ret)[3] = priv->D;
  return ret;
}

static LIVES_HOT void lives_md5_proc(const void *p, size_t len, md5priv *priv) {
  // process one block of data, values are placed in priv->A / B / C / D
  size_t nw = len / sizeof(uint32_t);
  const uint32_t *w = p, *e = w + nw;
  // backup current values of A, B, C, D - will be resotred after
  uint32_t X[MD5_SIZE], A = priv->A, B = priv->B, C = priv->C, D = priv->D;
  priv->t[0] += len;
  if (priv->t[0] < len) priv->t[1]++;
  while (w < e) {
    uint32_t *_X = X, _A_ = A, _B_ = B, _C_ = C, _D_ = D;
    BX(0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee)BX(0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501);
    BX(0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be)BX(0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821);
    CW(1, 0xf61e2562, 6, 0xc040b340, 11, 0x265e5a51, 0, 0xe9b6c7aa) CW(5, 0xd62f105d, 10, 0x02441453, 15, 0xd8a1e681, 4,
        0xe7d3fbc8);
    CW(9, 0x21e1cde6, 14, 0xc33707d6, 3, 0xf4d50d87, 8, 0x455a14ed) CW(13, 0xa9e3e905, 2, 0xfcefa3f8, 7, 0x676f02d9, 12,
        0x8d2a4c8a);
    CX(5, 0xfffa3942, 8, 0x8771f681, 11, 0x6d9d6122, 14, 0xfde5380c) CX(1, 0xa4beea44, 4, 0x4bdecfa9, 7, 0xf6bb4b60, 10,
        0xbebfbc70);
    CX(13, 0x289b7ec6, 0, 0xeaa127fa, 3, 0xd4ef3085, 6, 0x04881d05) CX(9, 0xd9d4d039, 12, 0xe6db99e5, 15, 0x1fa27cf8, 2,
        0xc4ac5665);
    CY(0, 0xf4292244, 7, 0x432aff97, 14, 0xab9423a7, 5, 0xfc93a039) CY(12, 0x655b59c3, 3, 0x8f0ccc92, 10, 0xffeff47d, 1,
        0x85845dd1);
    CY(8, 0x6fa87e4f, 15, 0xfe2ce6e0, 6, 0xa3014314, 13, 0x4e0811a1) CY(4, 0xf7537e82, 11, 0xbd3af235, 2, 0x2ad7d2bb, 9,
        0xeb86d391);
    A += _A_; B += _B_; C += _C_; D += _D_;
  }
  priv->A = A; priv->B = B; priv->C = C; priv->D = D;
}

static void lives_md5_calc(const void *p, size_t len, md5priv *priv) {
  if (priv->bl) {
    size_t rem = priv->bl, extra = 128 - rem > len ? len : 128 - rem;
    lives_memcpy(&priv->buf[rem], p, extra);
    priv->bl += extra;
    rem += extra;
    if (rem > 64) {
      lives_md5_proc(priv->buf, rem & ~63, priv);
      lives_memcpy(priv->buf, &priv->buf[rem & ~63], rem & 63);
      priv->bl = rem & 63;
    }
    p = (const char *) p + extra;
    len -= extra;
  }

  if (len > 64) {
    lives_md5_proc(p, len & ~63, priv);
    p = (const char *)p + (len & ~63);
    len &= 63;
  }

  if (len > 0) {
    lives_memcpy(priv->buf, p, len);
    priv->bl = len;
  }
}

static void *lives_md5_end(md5priv *priv, void *ret) {
  uint32_t dlen = priv->bl;
  size_t padsize = dlen >= 56 ? 64 + 56 - dlen : 56 - dlen;
  priv->t[0] += dlen;
  if (priv->t[0] < dlen) priv->t[1]++;
  lives_memcpy(&priv->buf[dlen], padding, padsize);
  dlen += padsize;
  *(uint32_t *)&priv->buf[dlen] = priv->t[0] << 3;
  *(uint32_t *)&priv->buf[dlen + 4] = (priv->t[1] << 3) | (priv->t[0] >> 29);
  lives_md5_proc(priv->buf, dlen + 8, priv);
  return lives_md5_read(priv, ret);
}

LIVES_LOCAL_INLINE void *lives_md5_make(const char *p, size_t len, void *ret) {
  md5priv priv;
  lives_md5_start(&priv);
  lives_md5_calc(p, len, &priv);
  return lives_md5_end(&priv, ret);
}


void *lives_md5_sum(const char *filename, int64_t *fsize) {
  char buff[65536];
  uint8_t *md5buf = lives_calloc(1, MD5_SIZE);
  md5priv priv;
  int64_t tot = 0;
  int fd = lives_open_buffered_rdonly(filename);
  if (fd < 0) return NULL;
  lives_md5_start(&priv);
  while (!lives_read_buffered_eof(fd)) {
    ssize_t dsize = lives_read_buffered(fd, buff, 65536, TRUE);
    if (dsize < 0) return NULL;
    lives_md5_calc(buff, dsize, &priv);
    tot += dsize;
  }
  lives_close_buffered(fd);
  if (fsize) *fsize = tot;
  return lives_md5_end(&priv, md5buf);
}


uint8_t *tinymd5(void *data, size_t dsize) {
  uint8_t *md5buf = lives_calloc(1, MD5_SIZE);
  lives_md5_make(data, dsize, md5buf);
  return md5buf;
}


uint64_t minimd5(void *data, size_t dsize) {
  static union md5conv {
    uint64_t U[MD5_SIZE >> 3];
    uint8_t u[MD5_SIZE];
  } res;
  uint64_t ret;
  lives_md5_make(data, dsize, (uint8_t *)&res.u);
  ret = res.U[0] ^ res.U[1];
  return ret;
}


#if LIVES_SH512
LIVES_LOCAL_INLINE void lives_sha512_start(md5priv *priv) {
  priv->H[0] = (const uint64_t)(0x6a09e667f3bcc908);
  priv->H[1] = (const uint64_t)(0xbb67ae8584caa73b);
  priv->H[2] = (const uint64_t)(0x3c6ef372fe94f82b);
  priv->H[3] = (const uint64_t)(0xa54ff53a5f1d36f1);
  priv->H[4] = (const uint64_t)(0x510e527fade682d1);
  priv->H[5] = (const uint64_t)(0x9b05688c2b3e6c1f);
  priv->H[6] = (const uint64_t)(0x1f83d9abfb41bd6b);
  priv->H[7] = (const uint64_t)(0x5be0cd19137e2179);
  priv->block_len = 0;
  priv->data_len[0] = 0;
  priv->data_len[1] = 0;
}

LIVES_LOCAL_INLINE  void lives_sha512_calc(uint64_t H[8], uint8 const data[SHA2_BLOCK_LEN)) {
  int i, t;
  uint64_t a, b, c, d, e, f, g, h, M[16], W[80];
  for (i = 0; i < 16; i++) {
    gint p = i * 8;
    M[i] = ((uint64_t) data[p + 0] << 56) | ((uint64_t) data[p + 1] << 48) |
           ((uint64_t) data[p + 2] << 40) | ((uint64_t) data[p + 3] << 32) |
           ((uint64_t) data[p + 4] << 24) | ((uint64_t) data[p + 5] << 16) |
           ((uint64_t) data[p + 6] <<  8) | ((uint64_t) data[p + 7]);
    for (t = 0; t < 80; t++)
      if (t < 16) W[t] = M[t];
      else W[t] = sigma1(W[t - 2]) + W[t - 7] + sigma0(W[t - 15]) + W[t - 16];

    a = H[0]; b = H[1]; c = H[2]; d = H[3];
    e = H[4]; f = H[5]; g = H[6];   h = H[7];

    for (t = 0; t < 80; t++) {
      uint64_t T1, T2;
      T1 = h + SIGMA1(e) + Ch(e, f, g) + SHA2_K[t] + W[t];
      T2 = SIGMA0(a) + Maj(a, b, c);
      h = g; g = f; f = e; e = d + T1;
      d = c; c = b; b = a; a = T1 + T2;
    }
    H[0] += a; H[1] += b; H[2] += c;  H[3] += d;
    H[4] += e; H[5] += f; H[6] += g; H[7] += h;
  }

  LIVES_LOCAL_INLINE void *lives_sha512_make(const char *p, size_t len, void *ret) {
    sha512priv priv;
    lives_sha512_start(&priv);
    lives_sha512_calc(p, len, &priv);
    return lives_sha512_end(&priv, ret);
  }
#endif

  ///////////////////////////////////////////////////////// end md5sum code /////////////

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
  size_t running_average(float * newval, int idx, void **data) {
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
    return NULL;
  }


  /* const lives_object_template_t *maths_object_with_subtype(uint64_t subtype) { */
  /*   if (!inited) init_templates(); */
  /*   if (subtype == MATH_OBJECT_SUBTYPE_STATS) return &tmpls[SUBTYPE_STATS]; */
  /*   return NULL; */
  /* } */

