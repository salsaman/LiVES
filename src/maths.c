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
  if (capable->byte_order == LIVES_LITTLE_ENDIAN) swab4(&f, &f, 1);
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

