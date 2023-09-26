// stringfuncs.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

char *lives_string_tolower(const char *);

size_t lives_strlen(const char *);
boolean lives_strlen_atleast(const char *, size_t min);

int64_t lives_strtol(const char *);
uint64_t lives_strtoul(const char *);

int lives_utf8_strcasecmp(const char *, const char *);
int lives_utf8_strcmp(const char *, const char *);

char *lives_strdup_quick(const char *);

// lives_strconcat(char *, ...);

char *lives_strdup_concat(char *, const char *sep, const char *fmt, ...);

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

boolean lives_str_starts_with(const char *st1, const char *st2);
boolean lives_string_ends_with(const char *, const char *fmt, ...);
const char *lives_str_starts_with_skip(const char *st1, const char *st2);

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

char *format_tstr(double xtime, int minlim);

char *dir_to_pieces(const char *dirnm);

char *md5_print(void *md5sum);


// experimental
//size_t lives_strlen128(const char *s);
