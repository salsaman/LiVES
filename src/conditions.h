// conditions.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _CONDITIONS_H
#define _CONDITIONS_H

typedef allvalues_t *lives_condition;
typedef struct {va_list va;} va_surprise;

#define COND_PFX "COND_"

#define _COND_POPEN  		COND_PFX "POPEN"
#define _COND_PCLOSE  		COND_PFX "PCLOSE"
#define _COND_LIST_BEGIN  	COND_PFX "LIST_BEGIN"
#define _COND_LIST_END  	COND_PFX "LIST_END"

#define $(a) "COND_LOCAL", #a
#define $$(a) "COND_GLOBAL", #a
#define intvar(a) "COND_INT_VAL", a
#define uintvar(a) "COND_UINT_VAL", a
#define int64var(a) "COND_INT64_VAL", a
#define uint64var(a) "COND_UINT64_VAL", a
#define boolvar(vara) "COND_BOOLEAN_VAL", a
#define doublevar(a) "COND_DOUBLE_VAL", a
#define floatvar(a) "COND_FLOAT_VAL", a
#define stringvar(a) "COND_STRING_VAL", a
#define voidptrvar(a) "COND_VOIDPTR_VAL", a
#define funcptrvar(a) "COND_FUNCPTR_VAL", a
#define plantptrvar(a) "COND_PLANTPTR_VAL", a

#define COND(TOKEN) COND_PFX #TOKEN

void lives_conditions_init(void);

#define Xregister_cond_token(token, fmt, p0, p1, ...)			\
  _register_cond_token(token, fmt, p0, p1, #p1 __VA_OPT__(,) __VA_ARGS__)

#define register_cond_token(token, fmt, ...)			\
  Xregister_cond_token(token, fmt, __VA_ARGS__, nofunc, NULL);

typedef struct {
  const char *token; // token text
  char fmt; // internal fmt
  const char *desc; // descriptive text
  const char *funcname;
  lives_funcinst_t *funcinst;
} cond_trans;

// INITIAL state FOR PARTIAL
#define COND_SYNTAX_2 "C", "V", "U", _COND_POPEN, NULL

// initial state fOR FULL
#define COND_SYNTAX_0 _COND_PCLOSE, COND_SYNTAX_2

// state after C, V, PCLOSE or return from 'O'
#define COND_SYNTAX_1 "O", _COND_PCLOSE, NULL

typedef boolean lives_cond_result;

#define LIVES_COND_PASS TRUE
#define LIVES_COND_FAIL FALSE

#define CONDRES_NAME(res) (res) == LIVES_COND_PASS ? "LIVES_COND_PASS (TRUE)" : "LIVES_COND_FAIL (FALSE)"

lives_condition _lives_cond_create(const char *cond_start, ...);
#define lives_cond_create(...) _lives_cond_create(_COND_POPEN __VA_OPT__(,) __VA_ARGS__, _COND_PCLOSE)

lives_cond_result lives_cond_eval_real(lives_condition condition, const char *args_fmt, ...);
#define lives_cond_eval(cond, ...) lives_cond_eval_real(cond __VA_OPT__(,)__VA_ARGS__, 0)

lives_condition lives_cond_copy(lives_condition);
void lives_cond_free(lives_condition);
void lives_cond_desc(lives_condition);

#endif
