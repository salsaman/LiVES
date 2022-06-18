// stringfuncs.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

size_t lives_strlen(const char *s);

int64_t lives_strtol(const char *nptr);
uint64_t lives_strtoul(const char *nptr);

int lives_utf8_strcasecmp(const char *s1, const char *s2);
int lives_utf8_strcmp(const char *s1, const char *s2);

char *lives_strdup_quick(const char *s);

char *lives_strdup_concat(char *str, const char *sep, const char *fmt, ...);

boolean lives_strcmp(const char *st1, const char *st2);

int lives_strcmp_ordered(const char *st1, const char *st2);

char *lives_chomp(char *, boolean multi);
char *lives_strtrim(const char *);

boolean lives_string_ends_with(const char *string, const char *fmt, ...);

size_t get_token_count(const char *string, int delim);
LiVESList *get_token_count_split(char *string, int delim, size_t *ntoks);
LiVESList *get_token_count_split_nth(char *string, int delim, size_t *ntoks, int nt);

char *subst(const char *string, const char *from, const char *to);
char *subst_quote(const char *xstring, const char *quotes, const char *from, const char *to);
char *insert_newlines(const char *text, int maxwidth);

char *remove_trailing_zeroes(double val);

char *lives_ellipsize(char *, size_t maxlen, LiVESEllipsizeMode mode);
char *lives_pad(char *, size_t minlen, int align);
char *lives_pad_ellipsize(char *, size_t fixlen, int padlen, LiVESEllipsizeMode mode);

char *format_tstr(double xtime, int minlim);

char *dir_to_pieces(const char *dirnm);
