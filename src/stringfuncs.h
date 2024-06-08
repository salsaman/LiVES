// stringfuncs.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _STRINGFUNCS_H
#define _STRINGFUNCS_H

#define VAL_AND_QUOTED(val) val, (const char *)#val

#define lives_strdup(s) lives_strdup_quick(s)

#define lives_strdup_free(a, b) (lives_free_and_return((a)) ? NULL : lives_strdup(b))
#define lives_strdup_printf_free(a, ...) (lives_free_and_return((a)) ? NULL : lives_strdup_printf(__VA_ARGS__))

char *lives_strdup_quick(const char *str);
char *lives_strndup(const char *str, size_t len);

int lives_print_ret(const char *fmt, ...);

char *lives_strndup_printf(const char *fmt, int maxlen, ...) WARN_UNUSED;
char *lives_strdup_printf(const char *fmt, ...) WARN_UNUSED;
char *lives_strdup_vprintf(const char *fmt, va_list ap) WARN_UNUSED;

#define LSPF(fmt, ...) (lives_strdup_printf((fmt), __VA_ARGS__))

char *lives_string_tolower(const char *);

size_t lives_strlen(const char *);
boolean lives_strlen_atleast(const char *, size_t min);

int64_t lives_strtol(const char *);
uint64_t lives_strtoul(const char *);

int lives_utf8_strcasecmp(const char *, const char *);
int lives_utf8_strcmp(const char *, const char *);


char *lives_strdup_concat_sep(char *, const char *sep, const char *fmt, ...);

#define lives_strdup_concat(str, fmt, ...) \
  lives_strdup_concat_sep(str, NULL, fmt, __VA_ARGS__)

char *lives_concat_sep(char *st, const char *sep, char *x);

//frees x; st becomes invalid
char *lives_concat(char *, char *x);

char *lives_strcollate(char **, const char *sep, const char *xnew);

int lives_strappend(const char *, int len, const char *xnew);

const char *lives_strappendf(const char *, int len, const char *fmt, ...);

char *lives_strstop(char *, const char term);

boolean lives_strcmp(const char *, const char *);
boolean lives_strncmp(const char *, const char *, size_t len);

boolean lives_strcmp_free(char *, const char *);

int lives_strcmp_ordered(const char *, const char *);

char *lives_chomp(char *, boolean multi);
char *lives_strtrim(const char *);

boolean lives_str_starts_with(const char *string, const char *start);
boolean lives_str_ends_with(const char *, const char *fmt, ...);
const char *lives_str_starts_with_skip(const char *string, const char *start);

size_t get_token_count(const char *, int delim);
LiVESList *get_token_count_split(char *, int delim, size_t *ntoks);
LiVESList *get_token_count_split_nth(char *, int delim, size_t *ntoks, int nt);

char *subst(const char *string, const char *from, const char *to);
char *subst_quote(const char *xstring, const char *quotes, const char *from, const char *to);
char *insert_newlines(const char *, int maxwidth);

char *remove_trailing_zeroes(double val);

char *lives_ellipsize(char *, size_t maxlen, LiVESEllipsizeMode mode);
char *lives_pad(char *, size_t minlen, int align);
char *lives_pad_ellipsize(char *, size_t fixlen, int padlen, LiVESEllipsizeMode mode);

char *dir_to_pieces(const char *dirnm);

// the following functions use primituves which use glibc malloc / free
// here we define wrappers which copy and free originals
char **lives_strsplit(const char *str, const char *delim, int maxtok);
void lives_strfreev(char **strings);

// lives_strdup  + free()
char *strdup_free(char *);

#define lives_strconcat(str1, ...) strdup_free(_lives_strconcat((str1), __VA_ARGS__))
#define lives_markup_escape_text(str, size) strdup_free(_lives_markup_escape_text((str), (size)))
#define lives_markup_printf_escaped(fmt, ...) strdup_free(_lives_markup_printf_escaped((fmt), __VA_ARGS__))

// experimental
//size_t lives_strlen128(const char *s);

#endif
