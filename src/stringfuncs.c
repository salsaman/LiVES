// stringfuncs.c
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

#include <ctype.h>

LIVES_GLOBAL_INLINE char *lives_string_tolower(const char *st) {
  char *lst;
  size_t slen = lives_strlen(st);
  int i = 0;
  lst = lives_malloc(slen + 1);
  for (char *c = (char *)st; *c; c++) lst[i++] = (char)tolower(*c);
  lst[i] = 0;
  return lst;
}


size_t get_token_count(const char *string, int delim) {
  size_t pieces = 1;
  if (!string) return 0;
  if (delim <= 0 || delim > 255) return 1;
  while ((string = strchr(string, delim)) != NULL) {
    pieces++;
    string++;
  }
  return pieces;
}


LiVESList *get_token_count_split_nth(char *string, int delim, size_t *ntoks, int nt) {
  LiVESList *list = NULL;
  size_t pieces = 1;
  char *base = string;
  if (ntoks) *ntoks = 0;
  if (!string) return NULL;
  if (delim <= 0 || delim > 255) return NULL;
  while ((string = strchr(string, delim)) != NULL) {
    *string = 0;
    if (nt >= 0) {
      if (pieces > nt) return lives_list_append(list, (void *)lives_strdup(base));
    } else list = lives_list_append(list, (void *)lives_strdup(base));
    pieces++;
    base = (char *)(++string);
  }
  if (nt >= 0) {
    if (pieces > nt) return lives_list_append(list, (void *)lives_strdup(base));
  } else list = lives_list_append(list, (void *)lives_strdup(base));
  if (ntoks) *ntoks = pieces;
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *get_token_count_split(char *string, int delim, size_t *ntoks) {
  return get_token_count_split_nth(string, delim, ntoks, -1);
}


char *get_nth_token(const char *string, const char *delim, int pnumber) {
  if (!string || !delim || pnumber < 0) return NULL;
  else {
    char *str = lives_strdup(string), *ret;
    LiVESList *list = get_token_count_split_nth(str, delim[0], NULL, pnumber);
    lives_free(str);
    ret = (char *)list->data;
    lives_list_free(list);
    return ret;
  }
}


int lives_utf8_strcasecmp(const char *s1, const char *s2) {
  // ignore case
  if (!s1 || !s2) return (s1 != s2);
  else {
    char *s1u = lives_utf8_casefold(s1, -1);
    char *s2u = lives_utf8_casefold(s2, -1);
    int ret = lives_strcmp(s1u, s2u);
    lives_free(s1u);
    lives_free(s2u);
    return ret;
  }
}


LIVES_GLOBAL_INLINE int lives_utf8_strcmp(const char *s1, const char *s2) {
  return lives_utf8_collate(s1, s2);
}


/// each byte B can be thought of as a signed char, subtracting 1 sets bit 7 if B was <= 0, then AND with ~B clears bit 7 if it
/// was already set (i.e B was < 0), thus bit 7 only remains set if the byte started as 0.
#define hasNulByte(x) (((x) - 0x0101010101010101) & ~(x) & 0x8080808080808080)

// here we use a binary search, using the result from aove we want yo detext which byte is now non-zero
// so we do x & 0xFFFFFFFF00000000, if this is zero then try x & 0xFFFF0000 and so on
// values are in decimal just to look cool and mysterious
#define getnulpos(nulmask) ((nulmask & 2155905152ul) ? ((nulmask & 32896ul) ? ((nulmask & 128ul) ? 0 : 1) : \
							   ((nulmask & 8388608ul) ? 2 : 3)) : (nulmask & 141287244169216ul) ? \
			    ((nulmask & 549755813888ul) ? 4 : 5) : ((nulmask & 36028797018963968ul) ? 6 : 7))

#define getnulpos_be(nulmask) ((nulmask & 9259542121117908992ul) ? ((nulmask & 9259400833873739776ul) ? \
								    ((nulmask & 9223372036854775808ul) ? 0 : 1) : \
								    ((nulmask & 140737488355328ul) ? 2 : 3)) \
			       : (nulmask & 2155872256ul) ? ((nulmask & 2147483648ul) ? 4 : 5) : ((nulmask & 32768ul) ? 6 : 7))

#define getnulpos_ne(nulmask) (capable->hw.byte_order == LIVES_LITTLE_ENDIAN \
			       ? getnulpos(nulmask) : getnulpos_be(nulmask))


#define getnulbyte(nulmask) ((nulmask & 2155905152ul) ? ((nulmask & 32896ul) ? ((nulmask & 128ul) ? 1 : 2) : \
							   ((nulmask & 8388608ul) ? 3 : 4)) : (nulmask & 141287244169216ul) ? \
			    ((nulmask & 549755813888ul) ? 5 : ) : ((nulmask & 36028797018963968ul) ? 6 : 7))


/* #define BITMASKx(n, m)	0x##n#m */
/* #define BITMASK1(n)	BITMASKx(n, ) */
/* #define BITMASK1(n)	BITMASKx(n, ) */
/* #define BITMASK2(n)		BITMASK(n, n) */
/* #define BITMASK2_2(n, m)       	BITMASK(n, m) */
/* #define BITMASK4(n)		BITMASK2(n, n) */
/* #define BITMASK4_2(n, m))	BITMASK2(n, m) */
/* #define BITMASK8(n)		BITMASK4(n, n) */
/* #define BITMASK8_2(n, m)	BITMASK4(n, m) */


LIVES_GLOBAL_INLINE size_t lives_strlen(const char *s) {
  if (!s) return 0;
#ifndef STD_STRINGFUNCS
  else {
    uint64_t *pi = (uint64_t *)s, nulmask;
    if ((void *)pi == (void *)s) {
      while (!(nulmask = hasNulByte(*pi))) pi++;
      return (char *)pi - s + getnulpos_ne(nulmask);
    }
  }
#endif
  return strlen(s);
}


LIVES_GLOBAL_INLINE boolean lives_strlen_atleast(const char *s, size_t min) {
  if (!s) return FALSE;
  for (int i = 0; i < min; i++) if (!s[i]) return FALSE;
  return TRUE;
}


LIVES_GLOBAL_INLINE int64_t lives_strtol(const char *nptr) {
  if (sizeof(long int) == 8) return strtol(nptr, NULL, 10);
  else return strtoll(nptr, NULL, 10);
}


LIVES_GLOBAL_INLINE uint64_t lives_strtoul(const char *nptr) {
  if (sizeof(long int) == 8) return strtoul(nptr, NULL, 10);
  else return strtoull(nptr, NULL, 10);
}


LIVES_GLOBAL_INLINE char *lives_strdup_quick(const char *s) {
  if (!s) return NULL;
#ifndef STD_STRINGFUNCS
  else {
    uint64_t *pi = (uint64_t *)s, nulmask, stlen;
    if (!s) return NULL;
    if ((void *)pi == (void *)s) {
      while (!(nulmask = hasNulByte(*pi))) pi++;
      stlen = (char *)pi - s + 1
              + (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)
              ? getnulpos(nulmask) : getnulpos_be(nulmask);
      return lives_memcpy(lives_malloc(stlen), s, stlen);
    }
  }
#endif
  return lives_strdup(s);
}


/// returns FALSE if strings match
// safer version of strcmp, which can handle NULL strings
LIVES_GLOBAL_INLINE boolean lives_strcmp(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 != st2);
  else {
    return strcmp(st1, st2);
  }
}


// like strcmp, but frees the first param
LIVES_GLOBAL_INLINE boolean lives_strcmp_free(char *st1, const char *st2) {
  boolean bret = lives_strcmp((const char *)st1, st2);
  lives_free(st1);
  return bret;
}


LIVES_GLOBAL_INLINE int lives_strcmp_ordered(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 != st2);
  else {
#ifdef STD_STRINGFUNCS
    return strcmp(st1, st2);
#endif
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if ((void *)ip1 == (void *)st1 && (void *)ip2 == (void *)st2) {
        do {
          d1 = *(ip1++);
          d2 = *(ip2++);
        } while (d1 == d2 && !hasNulByte(d1));
        st1 = (const char *)(--ip1); st2 = (const char *)(--ip2);
      }
      if (*st1 != *st2 || !(*st1)) break;
      st1++; st2++;
    }
  }
  return (*st1 > *st2) - (*st1 < *st2);
}

/// returns FALSE if strings match
LIVES_GLOBAL_INLINE boolean lives_strncmp(const char *st1, const char *st2, size_t len) {
  if (!st1 || !st2) return (st1 != st2);
  else {
#ifdef STD_STRINGFUNCS
    return strncmp(st1, st2, len);
#endif
    size_t xlen = len >> 3;
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if (xlen && (void *)ip1 == (void *)st1 && (void *)ip2 == (void *)st2) {
        do {
          d1 = *(ip1++);
          d2 = *(ip2++);
        } while (d1 == d2 && !hasNulByte(d1) && --xlen);
        if (xlen) {
          if (!hasNulByte(d2)) return TRUE;
          ip1--;
          ip2--;
        }
        st1 = (void *)ip1; st2 = (void *)ip2;
        len -= ((len >> 3) - xlen) << 3;
      }
      if (!(len--)) return FALSE;
      if (*st1 != *(st2++)) return TRUE;
      if (!(*(st1++))) return FALSE;
    }
  }
  return (*st1 != *st2);
}

/// returns TRUE if st1 starts with st2
LIVES_GLOBAL_INLINE boolean lives_str_starts_with(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 == st2);
  else {
    size_t srchlen = lives_strlen(st2);
    if (!lives_strncmp(st1, st2, srchlen)) return TRUE;
    return FALSE;
  }
}

/// if st1 starts with st2, returns the next char, else NULL
LIVES_GLOBAL_INLINE const char *lives_str_starts_with_skip(const char *st1, const char *st2) {
  if (!st1 || !st2) return NULL;
  else {
    size_t srchlen = lives_strlen(st2);
    if (!lives_strncmp(st1, st2, srchlen)) return st1 + srchlen;
    return NULL;
  }
}


#define HASHROOT 5381
LIVES_GLOBAL_INLINE uint32_t lives_string_hash(const char *st) {
  if (st && *st) for (uint32_t hash = HASHROOT;; hash += (hash << 5)
                        + * (st++)) if (!(*st)) return hash;
  return 0;
}

LIVES_GLOBAL_INLINE char *lives_strstop(char *st, const char term) {
  /// truncate st, replacing term with \0
  if (st && term) {
    char *c = index(st, term);
    if (c) *c = 0;
  }
  return st;
}


LIVES_GLOBAL_INLINE char *lives_chomp(char *buff, boolean multi) {
  /// chop off final newline
  /// see also lives_strchomp() which removes all whitespace
  if (buff) {
    int slen, start = -1;
    for (slen = 0; buff[slen]; slen++) {
      if (buff[slen] == '\n') {
        if (start == -1) start = slen;
      } else start = -1;
    }
    if (start >= 0) lives_memset(&buff[multi ? start : --slen], 0, 1);
  }
  return buff;
}


LIVES_GLOBAL_INLINE char *lives_strtrim(const char *buff) {
  /// return string with start and end newlines stripped
  /// see also lives_strstrip() which removes all whitespace
  int i, j;
  if (!buff) return NULL;
  for (i = 0; buff[i] == '\n'; i++);
  for (j = i; buff[j]; j++) if (buff[j] == '\n') break;
  return lives_strndup(buff + i, j - i);
}


#define BSIZE (8)
#define INITSIZE 32

char *subst(const char *xstring, const char *from, const char *to) {
  // return a string with all occurrences of from replaced with to
  // return value should be freed after use
  char *ret = lives_calloc(INITSIZE, BSIZE);
  uint64_t ubuff = 0;
  char *buff;

  const size_t fromlen = strlen(from);
  const size_t tolen = strlen(to);
  const size_t tolim = BSIZE - tolen;

  size_t match = 0;
  size_t xtolen = tolen;
  size_t bufil = 0;
  size_t retfil = 0;
  size_t retsize = INITSIZE;
  size_t retlimit = retsize - BSIZE;

  buff = (char *)&ubuff;

  for (char *cptr = (char *)xstring; *cptr; cptr++) {
    if (*cptr == from[match++]) {
      if (match == fromlen) {
        // got complete match
        match = 0;
        if (bufil > tolim) xtolen = BSIZE - bufil;
        lives_memcpy(buff + bufil, to, xtolen);
        if ((bufil += xtolen) == BSIZE) {
          if (retfil > retlimit) {
            // increase size of return string
            ret = lives_recalloc(ret, retsize * 2, retsize, BSIZE);
            retsize *= 2;
            retlimit = (retsize - 1) *  BSIZE;
          }
          lives_memcpy(ret + retfil, buff, BSIZE);
          retfil += BSIZE;
          bufil = 0;
          if (xtolen < tolen) {
            lives_memcpy(buff, to + xtolen, tolen - xtolen);
            bufil += tolen - xtolen;
            xtolen = tolen;
          }
        }
      }
      continue;
    }

    if (--match > 0) {
      xtolen = match;
      if (bufil > BSIZE - match) xtolen = BSIZE - bufil;
      lives_memcpy(buff + bufil, from, xtolen);
      if ((bufil += xtolen) == BSIZE) {
        if (retfil > retlimit) {
          ret = lives_recalloc(ret, retsize * 2, retsize, BSIZE);
          retsize *= 2;
          retlimit = (retsize - 1) *  BSIZE;
        }
        lives_memcpy(ret + retfil, buff, BSIZE);
        retfil += BSIZE;
        bufil = 0;
        if (xtolen < fromlen) {
          lives_memcpy(buff, from + xtolen, fromlen - xtolen);
          bufil += fromlen - xtolen - match;
          xtolen = tolen;
        }
      }
      match = 0;
    }
    if (match < 0) match = 0;
    buff[bufil] = *cptr;
    if (++bufil == BSIZE) {
      if (retfil > retlimit) {
        ret = lives_recalloc(ret, retsize * 2, retsize, BSIZE);
        retsize *= 2;
        retlimit = (retsize - 1) *  BSIZE;
      }
      lives_memcpy(ret + retfil, buff, BSIZE);
      retfil += BSIZE;
      bufil = 0;
    }
  }

  if (bufil) {
    if (retsize > retlimit) {
      ret = lives_recalloc(ret, retsize + 1, retsize, BSIZE);
      retsize++;
    }
    lives_memcpy(ret + retfil, buff, bufil);
    retfil += bufil;
  }
  if (match) {
    if (retsize > retlimit) {
      ret = lives_recalloc(ret, retsize + 1, retsize, BSIZE);
      retsize++;
    }
    lives_memcpy(ret + retsize, from, match);
    retfil += match;
  }
  ret[retfil++] = 0;
  retsize *= BSIZE;

  if (retsize - retfil > (retsize >> 2)) {
    char *tmp = lives_malloc(retfil);
    lives_memcpy(tmp, ret, retfil);
    lives_free(ret);
    return tmp;
  }
  return ret;
}


char *subst_quote(const char *xstring, const char *quotes, const char *from, const char *to) {
  char *res = subst(xstring, from, to);
  char *res2 = lives_strdup_printf("%s%s%s", quotes, res, quotes);
  lives_free(res);
  return res2;
}


char *insert_newlines(const char *text, int maxwidth) {
  // crude formatting of strings, ensure a newline after every run of maxwidth chars
  wchar_t utfsym;
  char *retstr;
  size_t runlen = 0;
  size_t req_size = 1; // for the terminating \0
  size_t tlen;
  int xtoffs;
  boolean needsnl = FALSE;

  int i;

  if (!text) return NULL;

  if (maxwidth < 1) return lives_strdup("Bad maxwidth, dummy");

  tlen = lives_strlen(text);

  xtoffs = mbtowc(NULL, NULL, 0); // reset read state

  //pass 1, get the required size
  for (i = 0; i < tlen; i += xtoffs) {
    xtoffs = mbtowc(&utfsym, &text[i], 4); // get next utf8 wchar
    if (!xtoffs) break;
    if (xtoffs == -1) {
      LIVES_WARN("mbtowc returned -1");
      return lives_strdup(text);
    }

    if (*(text + i) == '\n') runlen = 0; // is a newline (in any encoding)
    else {
      runlen++;
      if (needsnl) req_size++; ///< we will insert a nl here
    }

    if (runlen == maxwidth) {
      if (i < tlen - 1 && (*(text + i + 1) != '\n')) {
        // needs a newline
        needsnl = TRUE;
        runlen = 0;
      }
    } else needsnl = FALSE;
    req_size += xtoffs;
  }

  xtoffs = mbtowc(NULL, NULL, 0); // reset read state

  retstr = (char *)lives_calloc_align(req_size);
  req_size = 0; // reuse as a ptr to offset in retstr
  runlen = 0;
  needsnl = FALSE;

  //pass 2, copy and insert newlines

  for (i = 0; i < tlen; i += xtoffs) {
    xtoffs = mbtowc(&utfsym, &text[i], 4); // get next utf8 wchar
    if (!xtoffs) break;
    if (*(text + i) == '\n') runlen = 0; // is a newline (in any encoding)
    else {
      runlen++;
      if (needsnl) {
        *(retstr + req_size) = '\n';
        req_size++;
      }
    }

    if (runlen == maxwidth) {
      if (i < tlen - 1 && (*(text + i + 1) != '\n')) {
        // needs a newline
        needsnl = TRUE;
        runlen = 0;
      }
    } else needsnl = FALSE;
    lives_memcpy(retstr + req_size, &utfsym, xtoffs);
    req_size += xtoffs;
  }

  *(retstr + req_size) = 0;

  return retstr;
}

char *remove_trailing_zeroes(double val) {
  int i;
  double xval = val;

  if (val == (int)val) return lives_strdup_printf("%d", (int)val);
  for (i = 0; i <= 16; i++) {
    xval *= 10.;
    if (xval == (int)xval) return lives_strdup_printf("%.*f", i, val);
  }
  return lives_strdup_printf("%.*f", i, val);
}


boolean lives_string_ends_with(const char *string, const char *fmt, ...) {
  char *textx;
  va_list xargs;
  size_t slen, cklen;
  boolean ret = FALSE;

  if (!string) return FALSE;

  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  if (!textx) return FALSE;
  slen = lives_strlen(string);
  cklen = lives_strlen(textx);
  if (cklen == 0 || cklen > slen) {
    lives_free(textx);
    return FALSE;
  }
  if (!lives_strncmp(string + slen - cklen, textx, cklen)) ret = TRUE;
  lives_free(textx);
  return ret;
}


// input length includes terminating NUL

LIVES_GLOBAL_INLINE char *lives_ellipsize(char *txt, size_t maxlen, LiVESEllipsizeMode mode) {
  /// eg. txt = "abcdefgh", maxlen = 6, LIVES_ELLIPSIZE_END  -> txt == "...gh" + NUL
  ///     txt = "abcdefgh", maxlen = 6, LIVES_ELLIPSIZE_START  -> txt == "ab..." + NUL
  ///     txt = "abcdefgh", maxlen = 6, LIVES_ELLIPSIZE_MIDDLE  -> txt == "a...h" + NUL
  // LIVES_ELLIPSIZE_NONE - do not ellipsise
  // return value should be freed, unless txt is returned
  const char ellipsis[4] = "...\0";
  size_t slen = lives_strlen(txt);
  off_t stlen, enlen;
  char *retval = txt;
  if (!maxlen) return NULL;
  if (slen >= maxlen) {
    if (maxlen == 1) return lives_strdup("");
    retval = (char *)lives_malloc(maxlen);
    if (maxlen == 2) return lives_strdup(".");
    if (maxlen == 3) return lives_strdup("..");
    if (maxlen == 4) return lives_strdup("...");
    maxlen -= 4;
    switch (mode) {
    case LIVES_ELLIPSIZE_END:
      lives_memcpy(retval, ellipsis, 3);
      lives_memcpy(retval + 3, txt + slen - maxlen, maxlen + 1);
      break;
    case LIVES_ELLIPSIZE_START:
      lives_memcpy(retval, txt, maxlen);
      lives_memcpy(retval + maxlen, ellipsis, 4);
      break;
    case LIVES_ELLIPSIZE_MIDDLE:
      enlen = maxlen >> 1;
      stlen = maxlen - enlen;
      lives_memcpy(retval, txt, stlen);
      lives_memcpy(retval + stlen, ellipsis, 3);
      lives_memcpy(retval + stlen + 3, txt + slen - enlen, enlen + 1);
      break;
    default: break;
    }
  }
  return retval;
}


LIVES_GLOBAL_INLINE char *lives_pad(char *txt, size_t minlen, int align) {
  // pad with spaces at start and end respectively
  // ealign gives ellipsis pos, palign can be LIVES_ALIGN_START, LIVES_ALIGN_END
  // LIVES_ALIGN_START -> pad end, LIVES_ALIGN_END -> pad start
  // LIVES_ALIGN_CENTER -> pad on both sides
  // LIVES_ALIGN_FILL - do not pad
  size_t slen = lives_strlen(txt);
  char *retval = txt;
  off_t ipos = 0;
  if (align == LIVES_ALIGN_FILL) return txt;
  if (slen < minlen - 1) {
    retval = (char *)lives_malloc(minlen);
    lives_memset(retval, ' ', --minlen);
    retval[minlen] = 0;
    switch (align) {
    case LIVES_ALIGN_END:
      ipos = minlen - slen;
      break;
    case LIVES_ALIGN_CENTER:
      ipos = minlen - slen;
      break;
    default:
      break;
    }
    lives_memcpy(retval + ipos, txt, slen);
  }
  return retval;
}


LIVES_GLOBAL_INLINE char *lives_pad_ellipsize(char *txt, size_t fixlen, int palign,  LiVESEllipsizeMode emode) {
  // if len of txt < fixlen it will be padded, if longer, ellipsised
  // ealign gives ellipsis pos, palign can be LIVES_ALIGN_START, LIVES_ALIGN_END
  // pad with spaces at start and end respectively
  // LIVES_ALIGN_CENTER -> pad on both sides
  // LIVES_ALIGN_FILL - do not pad
  size_t slen = lives_strlen(txt);
  if (slen == fixlen - 1) return txt;
  if (slen >= fixlen) return lives_ellipsize(txt, fixlen, emode);
  return lives_pad(txt, fixlen, palign);
}


char *format_tstr(double xtime, int minlim) {
  // format xtime (secs) as h/min/secs
  // if minlim > 0 then for mins >= minlim we don't show secs.
  char *tstr;
  int hrs = (int64_t)xtime / 3600, min;
  xtime -= hrs * 3600;
  min = (int64_t)xtime / 60;
  xtime -= min * 60;
  if (xtime >= 60.) {
    min++;
    xtime -= 60.;
  }
  if (min >= 60) {
    hrs++;
    min -= 60;
  }
  if (hrs > 0) {
    // TRANSLATORS: h(ours) min(utes)
    if (minlim) tstr = lives_strdup_printf(_("%d h %d min"), hrs, min);
    // TRANSLATORS: h(ours) min(utes) sec(onds)
    else tstr = lives_strdup_printf("%d h %d min %.2f sec", hrs, min, xtime);
  } else {
    if (min > 0) {
      if (minlim) {
        // TRANSLATORS: min(utes)
        if (min >= minlim) tstr = lives_strdup_printf(_("%d min"), min);
        // TRANSLATORS: min(utes) sec(onds)
        else tstr = lives_strdup_printf("%d min %d sec", min, (int)(xtime + .5));
      }
      // TRANSLATORS: min(utes) sec(onds)
      else tstr = lives_strdup_printf("%d min %.2f sec", min, xtime);
    } else {
      if (minlim) tstr = lives_strdup_printf("%d sec", (int)(xtime + .5));
      else tstr = lives_strdup_printf("%.2f sec", xtime);
    }
  }
  return tstr;
}


LIVES_GLOBAL_INLINE char *lives_concat_sep(char *st, const char *sep, char *x) {
  /// nb: lives strconcat
  // uses realloc / memcpy, frees x; st becomes invalid
  char *tmp;
  if (st) {
    size_t s1 = lives_strlen(st), s2 = lives_strlen(x), s3 = lives_strlen(sep);
    tmp = (char *)lives_realloc(st, ++s2 + s1 + s3);
    lives_memcpy(tmp + s1, sep, s3);
    lives_memcpy(tmp + s1 + s3, x, s2);
  } else tmp = lives_strdup(x);
  lives_free(x);
  return tmp;
}

LIVES_GLOBAL_INLINE char *lives_concat(char *st, char *x) {
  /// nb: lives strconcat
  // uses realloc / memcpy, frees x; st becomes invalid
  size_t s1 = lives_strlen(st), s2 = lives_strlen(x);
  char *tmp = (char *)lives_realloc(st, ++s2 + s1);
  lives_memcpy(tmp + s1, x, s2);
  lives_free(x);
  return tmp;
}


LIVES_GLOBAL_INLINE char *lives_strcollate(char **strng, const char *sep, const char *xnew) {
  // appends xnew to *string, if strlen(*string) > 0 appends sep first
  //
  char *tmp = (strng && *strng && **strng) ? *strng : lives_strdup("");
  char *strng2 = lives_strdup_printf("%s%s%s", tmp, (sep && *tmp) ? sep : "", xnew);
  lives_freep((void **)strng);
  return strng2;
}


LIVES_GLOBAL_INLINE int lives_strappend(const char *string, int len, const char *xnew) {
  /// see also: lives_concat()
  size_t sz = lives_strlen(string);
  int newln = lives_snprintf((char *)(string + sz), len - sz, "%s", xnew);
  if (newln > len) newln = len;
  return --newln - sz; // returns strlen(xnew)
}

LIVES_GLOBAL_INLINE const char *lives_strappendf(const char *string, int len, const char *fmt, ...) {
  va_list xargs;
  char *text;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  lives_strappend(string, len, text);
  lives_free(text);
  return string;
}


char *lives_strdup_concat(char *str, const char *sep, const char *fmt, ...) {
  // appends to a, freeing old ptr and returning new string
  // if sep, str and fmted string are all non NULL / non empty, inserts sep after str
  va_list xargs;
  char *tmp, *out;
  va_start(xargs, fmt);
  tmp = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  if (!str || !*str) {
    out = tmp;
  } else {
    out = lives_strdup_printf("%s%s%s", str, (tmp && *tmp && sep) ? sep : "",
                              (tmp && *tmp) ? tmp : "");
    if (tmp) lives_free(tmp);
  }
  if (str) lives_free(str);
  return out;
}


char *dir_to_pieces(const char *dirnm) {
  // rewrite a pathname with multiple // collapsed and then each remaining / translated to '|'
  char *pcs = subst(dirnm, LIVES_DIR_SEP, "|"), *pcs2;
  size_t plen = lives_strlen(pcs), plen2;
  for (; pcs[plen - 1] == '|'; plen--) pcs[plen - 1] = 0;
  while (1) {
    plen2 = plen;
    pcs2 = subst(pcs, "||", "|");
    plen = lives_strlen(pcs2);
    if (plen == plen2) break;
    lives_free(pcs);
    pcs = pcs2;
  }
  return pcs2;
}


char *md5_print(void *md5sum) {
  char *cc = (char *)md5sum;
  char *qq = lives_strdup("0x");
  for (int i = 0; i < 16; i++) qq = lives_strdup_concat(qq, "", "%02x", cc++);
  return qq;
}
