// (c) G. Finch 2008 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// lsd.h :: implementation of LiVES Struct Definition (LSD)
// functions for auto copy and auto free of structs

#ifndef __STRUCTDEFS_H__
#define __STRUCTDEFS_H__

#ifdef __cplusplus
extern "C"
{
#endif

  // reminder to self (!)
  // if we have a struct type *s == 0xF00000
  // field n is at offset (char *)s + x == p
  // this is a pointer to the field
  // if we dereference this *p == field n
  // then if p is a pointer, field x is at *p + offset to field.
  // this is a pointer to the field

#include <stdio.h>
#if !defined LSD_OVERRIDE_STRGFUNCS || !defined LSD_OVERRIDE_MEMFUNCS
#include <stdlib.h>
#include <string.h>
#endif
#include <inttypes.h>
#include <stdarg.h>

#include <unistd.h>

  //#define DEBUG

#ifdef DEBUG
#define lsd_debug_print(...) fprintf(stderr, __VA_ARGS__)
#else
#define lsd_debug_print(...)
#endif

#ifndef SILENT_ENOMEM
#define lsd_memerr_print(size, name, struct) fprintf(stderr, "WARNING: memory failure allocating " \
						     "%" PRIu64 " bytes for field %s in struct %s", \
						     size, name, struct)
#else
#define lsd_memerr_print(a, b, c)
#endif

#ifndef SILENT_FAILURES
#define lsd_baderr_print(...) fprintf(stderr, __VA_ARGS__)
#else
#define lsd_baderr_print(...)
#endif

#ifdef ALLOW_UNUSED
#undef ALLOW_UNUSED
#endif

#ifdef __GNUC__
#define ALLOW_UNUSED __attribute__((unused))
#define GNU_HOT __attribute__((hot))
#else
#define ALLOW_UNUSED
#define GNU_HOT
#endif

#define _SELF_STRUCT_STRUCT_ struct _lsd_struct_def

#define LSD_SELF_TYPE lsd_struct_def_t

#define LSD_SELF_STRUCT_TYPE "LSD_SELF_TYPE"

#define LSD_SELF_STRUCT_SIZE (sizeof(LSD_SELF_TYPE))

#define LSD_STRUCT_TYPE_LEN 32 ///< max len (bytes) for struct type names
#define LSD_FIELD_NAME_LEN 32 ///< max field name length (bytes)
#define LSD_CLASS_ID_LEN 64 ///< max len for class_id
#define LSD_MAX_ALLOC ((uint64_t)1048576)
#define LSD_STRUCT_ID 0x4C7C56332D2D3035  /// 1st 8 bytes - L|V3--05 (be) or 50--3V|L (le)

#define LSD_FIELD_LSD				"LSD"

#define LSD_FIELD_IDENTIFIER		       	"identifier"
#define LSD_FIELD_UNIQUE_ID			"unique_id"
#define LSD_FIELD_TOP				"top"
#define LSD_FIELD_SELF_FIELDS			"self_fields"
#define LSD_FIELD_SPECIAL_FIELDS		"special_fields"
#define LSD_FIELD_GENERATION			"generation"
#define LSD_FIELD_REFCOUNT			"refcount"
#define LSD_FIELD_USER_DATA			"user_data"
#define LSD_FIELD_END_ID			"end_id"

#define LSD_SUCCESS 0
#define LSD_FAIL 1
#define LSD_ERROR -1
  
#ifndef LSD_OVERRIDE_MEMFUNCS
  static void *(*_lsd_calloc)(size_t nmemb, size_t size) = calloc;
  static void *(*_lsd_memcpy)(void *dest, const void *src, size_t n) = memcpy;
  static void *(*_lsd_memset)(void *s, int c, size_t n) = memset;
  static void (*_lsd_free)(void *ptr) = free;
#else
  extern void *(*_lsd_calloc)(size_t nmemb, size_t size);
  extern void *(*_lsd_memcpy)(void *dest, const void *src, size_t n);
  extern void *(*_lsd_memset)(void *s, int c, size_t n);
  extern void (*_lsd_free)(void *ptr);
#endif

#ifndef LSD_OVERRIDE_STRFUNCS
  static int(*_lsd_strcmp)(const char *s1, const char *s2) = strcmp;
  static char *(*_lsd_strdup)(const char *s) = strdup;
  static int(*_lsd_snprintf)(char *s, size_t n, const char *format, ...) = snprintf;
  static void (*_lsd_string_free)(void *ptr) = free;
#else
  extern int(*_lsd_strcmp)(const char *s1, const char *s2);
  extern char *(*_lsd_strdup)(const char *s);
  extern int(*_lsd_snprintf)(char *s, size_t n, const char *format, ...);
  extern void (*_lsd_string_free)(void *ptr);
#endif

#ifndef LSD_OVERRIDE_CALLOC_ALIGNED
  static void *_lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size) {
    return !memptr ? NULL : (!(*memptr = (*_lsd_calloc)(nmemb, size))) ? NULL : *memptr;}
#else
  extern void *(*_lsd_calloc_aligned_)(void **memptr, size_t nmemb, size_t size);
#endif

  typedef _SELF_STRUCT_STRUCT_ LSD_SELF_TYPE;

  typedef void(*_lsd_cb_func)(void *);

  //static _lsd_cb_func lsd_null_cb ALLOW_UNUSED;
  static void lsd_null_cb(void *none) {/* dummy cb */};

  typedef struct {
    _lsd_cb_func func;
    void *data;
  } _lsd_cb;

  // CALLBACK FUNCTION TYPEDEFS

  // struct callbacks

  // this is called from lsd_struct_new after all fields have been initialised via init_cbunc
  typedef void (*lsd_struct_new_cb)(void *strct, void *parent, const char *strct_type,
				    void *new_user_data);

  // this is called from lsd_struct_copy after a copy is made
  typedef void (*lsd_struct_copied_cb)(void *strct, void *child, const char *strct_type,
				       void *copied_user_data);

  // this is called from lsd_struct_free before any fields are freed or delete_func called
  typedef void (*lsd_struct_destroy_cb)(void *strct, const char *strct_type, void *delete_user_data);

  // field callbacks

  //called from lsd_struct_new
  typedef void (*lsd_field_init_cb)(void *strct, const char *struct_type,
				    const char *field_name, void *ptr_to_field, void *user_data);

  // this is called from lsd_struct_copy after all automatic updates are performed
  typedef void (*lsd_field_copy_cb)(void *dst_struct, void *src_struct, const char *strct_type,
				    const char *field_name, void *ptr_to_dst_field,
				    void *ptr_to_src_field, void *user_data);

  //called from lsd_struct_free before any fields are finalised
  typedef void (*lsd_field_delete_cb)(void *strct, const char *struct_type,
				      const char *field_name, void *ptr_to_field, void *user_data);
  // STRUCTS
  typedef struct _lsd_special_field lsd_special_field_t;

  struct _lsd_special_field {
    lsd_special_field_t *next;
    /// flags may be  0 to optionally provide info re. the field name, byte_size,
    //(and optionally offset_to_field)
    uint64_t flags;
    /// must be set when creating the struct
    off_t offset_to_field;
    char name[LSD_FIELD_NAME_LEN]; /// optional unless flags == 0 or any of the fns. below are defined.
    size_t byte_size; /// defines the element size for alloc and copy
    // for the LSD field, these hold the struct callbacks
    _lsd_cb init_cb; ///< will be called from lsd_struct_new
    _lsd_cb copy_cb; ///< will be called from lsd_struct_copy
    _lsd_cb delete_cb; ///< called from lsd_struct_free
  };

#define LSD_SPECIAL_FIELD_SIZE (sizeof(lsd_special_field_t))

  typedef _SELF_STRUCT_STRUCT_ {
    uint64_t identifier;  /// default: LSD_STRUCT_ID
    uint64_t unique_id; /// randomly generated id, unique to each instance
    void *top; ///< ptr to the start of parent struct itself, typecast to a void *
    char struct_type[LSD_STRUCT_TYPE_LEN]; /// type of the struct as string, e.g "lsd_struct_def_t"
    size_t struct_size; ///< byte_size of parent struct (sizef(struct))
    lsd_special_field_t *self_fields;  /// fields in the struct_def_t struct itself
    lsd_special_field_t *special_fields;  /// may be NULL, else is pointer to NULL terminated array
    uint64_t owner_uid; ///< can be set to identify the owner, should only be set once, in template
    uint32_t generation; ///< initialized as 1 and incremented on each copy
    char class_id[LSD_CLASS_ID_LEN]; /// user definable class_id, value is copied to any clones
    int32_t refcount; ///< refcount, set to 1 on creation, free unrefs and only frees when 0.
    void *user_data; /// user_data for instances of struct, reset on copy
    uint64_t end_id;  /// end marker. == identifier ^ 0xFFFFFFFFFFFFFFFF
  } LSD_SELF_TYPE; /// 256 bytes

  //////////////////////////////////// USER API ////////////////////////////////////

  /// AUTOMATION FLAGS

#define LSD_FIELD_FLAGS_NONE 0

  ///  - copying flags
  /// alloc and copy on copy. If byte_size is set, that will be the allocated size,
  /// if byte_size is 0, then we do a strdup.
  /// For nullt arrays, this is applied to each non-zero element
#define LSD_FIELD_FLAG_ALLOC_AND_COPY (1ull << 1)

  /// for non arrays:
  /// if byte_size is 0, and ALLOC_AND_COPY is also set
  /// will be set to empty string i.e memset(dest->field, 0, byte_size)
  /// else set to NULL
  ///
  /// if byte_size > 0 then field will will be filled with byte_size zeros
  /// i.e. memset(dest->field, 0, byte_size)
  //
  /// for NULLT_ARRAYS, the process will be applied to each element in turn
  /// for bsize == 0:
  /// - if COPY_AND_ALLOC is NOT set, we do a copy by value
  /// -- setting ZERO_ON_COPY is *dangerous* as all elements will instead be set to NULL
  ///     hence the array length will be lost !
  ///
  /// - if COPY_AND_ALLOC is set:
  /// -- we do a strdup, unless ZERO_ON_COPY is also set in which case we create all empty strings
  ///      (up to the NULL element)
  ///
  /// for bsize > 0:
  /// - if COPY_AND_ALLOC is NOT set, we do a copy by value
  ///    setting ZERO_ON_COPY will result in each value being set to bsize zeroes
  ///
  /// - if COPY_AND_ALLOC is set, we do a copy by reference
  ///    setting ZERO_ON_COPY will result in each value being set to NULL
  ///     hence the array length will be lost !
  ///
#define LSD_FIELD_FLAG_ZERO_ON_COPY (1ull << 2)

  // setting LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY results in init_func being called when a struct is copied
  // in anidentical fashion to when a new struct is created. This allows use of a single callback function to
  // set a default both on creation and when copying. If a copy_func is also defined, then then copy_func will be
  // called as normal as soon as the init_func callback returns. Both functions are called after the struct has been copied
  // and any automatic defualts hav been set

  // added in v1.0.1
#define LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY (1ull << 3)

  // - delete flags
  /// field will be freed in lsd_struct_delete
  /// free(struct->field)
  ///
#define LSD_FIELD_FLAG_FREE_ON_DELETE (1ull << 16)

  /// for (i = 0; struct->field[i], i++) free(struct->field[i];
  /// - combined with IS_NULLT_ARRAY, frees all (non NULL) elements;
  ///   if that flagbit is not set, this one is ignored
  /// combined with FREE_ON_DELETE frees elements + array itself
  /// - advisable only to set this is ALLOC_AND_COPY is also set
#define LSD_FIELD_FLAG_FREE_ALL_ON_DELETE (1ull << 17)

  /// flags giving extra info about the field (affects copy and delete)

  /// field is a substruct with its own lsd_struct_def_t; functions may be called depth first
  /// descending into substructs of substructs. structs can even be substructs of themselves, provided
  /// we have a limit on the depth

  /// - NOT YET IMPLEMENTED !
  // - for this to work we need a pointer or a copy of the lsd template for the substruct, and
  //  there is currently no mechanism for setting this.
#define LSD_FIELD_FLAG_IS_SUBSTRUCT (1ull << 24)

  /// field is an array of elements each of size byte_size, last element has all bytes set to zero
  /// if byte_size is zero, it is an array of 0 terminated char * (strings), with last element NULL
  ///
  /// if byte_size > 0:
  /// - if ALLOC_AND_COPY is NOT set then a simple copy by value is done, stopping at any element with
  ///    bsize zeroes
  ///
  /// - if ALLOC_AND_COPY is set,
  /// bsize defines the element size, and the array is terminated by an element with bsize zeroes

  ///    and we do a simple copy by ref of each element. In this case, setting ZERO_ON_COPY would simply set all
  ///    values to zero, thus it should be handled with caution as the information about the array length would be lost
  ///
  /// may be combined with ALLOC_AND_COPY, FREE_ON_DELETE, FREE_ALL_ON_DELETE
  /// combining with ZERO_ON_COPY is NOT recommended for reasons given above
#define LSD_FIELD_FLAG_IS_NULLT_ARRAY (1ull << 25)

  // combinations:
    // Z : byte_size == 0 :: set any * to NULL
    // Z : byte_size > 0 :: memset(byte_size, 0)

  // A&C : byte_size == 0 :: strdup
  // A&C : byte_size > 0 :: malloc(byte_size), memcpy(byte_size)
  // FREE_ON_DELETE recommended (LSD_FIELD_IS_CHARPTR, LSD_FIELD_IS_BLOB)

  // A&C | Z : byte_size == 0 :: strdup("")  (LSD_FIELD_TO_EMPTY_STRING)
  // A&C | Z : byte_size > 0 :: malloc(byte_size), memset(byte_size, 0)
  // FREE_ON_DELETE recommended

  // NULLT : byte_size == 0 :: copy string array (strings still point to original strings)
  // NULLT : byte_size > 0 :: copy array of elements of size byte_size
  // FREE_ON_DELETE recommended (LSD_FIELD_IS_ARRAY)
  // setting FREE_ALL_ON_DELETE may be dangerous, as it would free the original values !

  // NULLT + Z : interpreted as NULLT + ACC + Z,
  // 		otherwise it would imply copying by reference, then ovewriting memory with 0

  // NULLT + A&C : byte_size == 0 :: strdup, multiple strings
  // NULLT + A&C : byte_size > 0 :: copy array of pointers to elements of size byte_size
  // FREE_ON_DELETE | FREE_ALL_ON_DELETE recommended (LSD_FIELD_IS_

  // NULLT + A&C + Z : byte_size == 0 :: creates an equivalent number of empty strings
  // NULLT + A&C + Z : byte_size > 0 :: malloc(byte_size), memset(byte_size, 0), multiple elements
  // FREE_ON_DELETE | FREE_ALL_ON_DELETE recommended

#define LSD_FIELD_CHARPTR (LSD_FIELD_FLAG_ALLOC_AND_COPY | LSD_FIELD_FLAG_FREE_ON_DELETE)
#define LSD_FIELD_STRING LSD_FIELD_CHARPTR

#define LSD_FIELD_BLOB LSD_FIELD_CHARPTR // with byte_size > 0

  // copy by value, free array on delete
#define LSD_FIELD_ARRAY (LSD_FIELD_FLAG_IS_NULLT_ARRAY | LSD_FIELD_FLAG_FREE_ON_DELETE)

  // copy by value, free array on delete
#define LSD_FIELD_PTR_ARRAY (LSD_FIELD_ARRAY | LSD_FIELD_FLAG_ALLOC_AND_COPY \
			     | LSD_FIELD_FLAG_FREE_ALL_ON_DELETE)

#define LSD_FIELD_STR_ARRAY LSD_FIELD_PTR_ARRAY // with bsize == 0

  // with a byte_size of zero this will cause a string to be set to "" on copy
  // without the ALLOC_AND_COPY, the string would be set to NULL
#define LSD_FIELD_TO_EMPTY_STRING (LSD_FIELD_FLAG_ALLOC_AND_COPY | LSD_FIELD_FLAG_ZERO_ON_COPY)

  // flagbits >= 32 are reserved for  internal flags that should not be messed with
#define LSD_PRIV_NO_DEL_STRUCT (1ull << 40)	// free all fields but not the struct itself

  /// API functions ///////////

#define LSD_CREATE_P(lsd, type) do {type *thestruct; lsd = 0;		\
    if (!(*_lsd_calloc_aligned_)((void **)&thestruct, 1, sizeof(type)))	\
      lsd_memerr_print(sizeof(type), "all fields", #type);		\
    else {lsd = lsd_create_p(#type, thestruct, sizeof(type), &thestruct->lsd); \
      (*_lsd_free)(thestruct);}} while (0);

#define ADD_SPCL_FIELD(field, flags, nfuncs, ...)			\
  lsd_add_special_field(thestruct->lsd, #field, flags, &thestruc->##field, nfuncs, \
			thestruct, __VA_ARGS__, NULL);

#define LSD_CREATE(lsd, type) do {type *thestruct; lsd = 0;		\
    if (!(*_lsd_calloc_aligned_)((void **)&thestruct, 1, sizeof(type)))	\
      {lsd_memerr_print(sizeof(type), "all fields", #type);}		\
    else {lsd = lsd_create(#type, thestruct, sizeof(type), &thestruct->lsd); \
      (*_lsd_free)(thestruct);}} while (0);

  static const lsd_struct_def_t *lsd_create(const char *struct_type, void *mystruct, size_t struct_size,
					    lsd_struct_def_t *lsd_in_struct) ALLOW_UNUSED;

  static const lsd_struct_def_t *lsd_create_p(const char *struct_type, void *mystruct, size_t struct_size,
					      lsd_struct_def_t **lsd_in_struct) ALLOW_UNUSED;

  // function to define special fields
  // if the flags are LSD_FIELD_IS_SUBSTRUCT, the parameter following sample_struct
  // MUST be a pointer to the lsd_struct_def of the substruct, and any further parameters will be ignored.
  //
  // otherwise, the parameter after sample_struct MUST be EITHER:
  // NULL -> any further parameters will be ignored,
  //
  // OR a set of callbacks:
  // init_callback, init_callback_data, copy_callback, copy_callback_data, free_callback, free_callback_data
  // if a callback is not used, then it MUST be set to lsd_null_cb, and the following data parameter omitted
  // all three callbacks MUST be defined.
  //
  static void lsd_add_special_field(lsd_struct_def_t *, const char *field_name, uint64_t flags, void *ptr_to_field,
				size_t data_size, void *sample_struct, ...) ALLOW_UNUSED;

  /// creates a new instance of struct. lsd must  be one returned from create_lsd / lsd_struct_init
  // otherwise you can copy
  static void *lsd_struct_create(const lsd_struct_def_t *) ALLOW_UNUSED;

  // initialise an already allocated struct. The lsd will be allocated / initialised
  // and special fieslds initialised.
  static void *lsd_struct_initialise(const lsd_struct_def_t *, void *astruct) ALLOW_UNUSED;

  // allocates and returns a copy of struct, calls copy_funcs, fills in lsd_struct_def for copy
  // lsd must be within a struct, not a static template
  static void *lsd_struct_copy(lsd_struct_def_t *) ALLOW_UNUSED;

  // this is an alias for lsd_struct_unref. If you want to force free, then set lsd->refcount <= 0 first.
  static int lsd_struct_free(lsd_struct_def_t *) ALLOW_UNUSED;

  // decrements refcount, then if <= 0 frees struct. Returns LSD_SUCCESS (0) if freed, otherwise returns the
  // new refcount.
  // returns LSD_ERROR (-1) if parameter is NULL, or the lsd is badly formed.
  // It is left to the caller to implement mutex locking if needed.
  static int lsd_struct_unref(lsd_struct_def_t *) ALLOW_UNUSED;

  // increments refcount, returns new value. Returns LSD_ERROR (-1) if parameter is NULL
  // It is left to the caller to implement mutex locking if needed.
  static int lsd_struct_ref(lsd_struct_def_t *) ALLOW_UNUSED;

  // Values returned from the get_* functions below refer to the actual fields in the lsd struct,
  // and must not be freed or written to, except via the corresponding API functions

  // returns current refcount, or LSD_ERROR if NULL is passed
  static int lsd_struct_get_refcount(lsd_struct_def_t *) ALLOW_UNUSED;

  // returns LSD_SUCCESS if both struct defs have  same identifier, end_id, struct_type,
  // struct_size, and class_id (i.e one is copy / instance of another)
  // (lsd_struct_get_generation can provide more information), otherwise LSD_FAIL
  static int lsd_same_family(const lsd_struct_def_t *lsd1, lsd_struct_def_t *lsd2) ALLOW_UNUSED;

  // sets class data which will be copied to all instances from template
  // and from instance to copies.
  static void lsd_struct_set_class_id(lsd_struct_def_t *, const char *class_id) ALLOW_UNUSED;

  // The returned value must not be freed or written to.
  static const char *lsd_struct_get_class_id(lsd_struct_def_t *) ALLOW_UNUSED;

  // set owner_uid, but only works for templates, and if the value is currently unset (0)
  // returns the new value. This is passed to all structs created
  static uint64_t lsd_struct_set_owner_uid(const lsd_struct_def_t *, uint64_t owner_uid)
    ALLOW_UNUSED;

  // The returned value must not be freed or written to.
  static uint64_t lsd_struct_get_owner_uid(lsd_struct_def_t *) ALLOW_UNUSED;

  // set user data for an instance, reset for copies
  static void lsd_struct_set_user_data(lsd_struct_def_t *, void *data) ALLOW_UNUSED;
  static void *lsd_struct_get_user_data(lsd_struct_def_t *) ALLOW_UNUSED;

  // returns generation number, 0 for a template, 1 for instance created via lsd_struct_new,
  // 2 for copy of instance, 3 for copy of copy, etc
  // returns LSD_ERROR if the parameter is NULL.
  static int lsd_struct_get_generation(lsd_struct_def_t *) ALLOW_UNUSED;

  static uint64_t lsd_struct_get_uid(lsd_struct_def_t *) ALLOW_UNUSED;

  static const char *lsd_struct_get_type(lsd_struct_def_t *) ALLOW_UNUSED;

  static uint64_t lsd_struct_get_identifier(lsd_struct_def_t *) ALLOW_UNUSED;

  static uint64_t lsd_struct_get_end_id(lsd_struct_def_t *) ALLOW_UNUSED;

  static size_t lsd_struct_get_size(lsd_struct_def_t *) ALLOW_UNUSED;

  /*
  // set init_callback for a struct or instance, passed on to copies
  // called after instance is made via lsd_struct_new or lsd_struct_copy
  // parent struct is NULL for lsd_struct_new
  static void lsd_struct_set_new_callback(lsd_struct_def_t *, void *new_user_data) ALLOW_UNUSED; // TODO

  // set copied callback for a struct or instance, passed on to copies
  // called when copy is made of an instance
  static void lsd_struct_set_copied_callback(lsd_struct_def_t *, void *copied_user_data) ALLOW_UNUSED; // TODO

  // set destroy callback for a struct or instance, passed on to copies
  // called when instance is about to be destroyed
  static void lsd_struct_set_destroy_callback(lsd_struct_def_t *, void *destroy_user_data) ALLOW_UNUSED; // TODO
  */

  ////////////////////////////////// END USER API /////////////////////////////////////////////////////////////

#ifndef LSD_UIDLEN
#define LSD_UIDLEN 8
#endif

#ifndef LSD_RANDFUNC
#if defined _UNISTD_H && defined  _DEFAULT_SOURCE
#define _LSD_IGN_RET(a) ((void)((a) + 1))
#define LSD_RANDFUNC(ptr, size) _LSD_IGN_RET(getentropy(ptr, size))
#else
  #define LSD_RANDFUNC
#error "getentropy not found, so you need to #define LSD_RANDFUNC(ptr, size)"
#error "Did you inlcude 'AC_CHECK_FUNCS(getentropy)' in configure.ac ?"
#endif
#endif

  static void _lsd_init_copy(void *dst_struct, void *src_struct, const char *strct_type, const char *field_name,
			     void *ptr_to_field) {
    // self_fields callbacks. These are run when creating (initng) a template (src_struct == NULL)
    // when initing a new struct, and when copying (src_struct non null, and dst points to new contianer)

    // common
    if (!(*_lsd_strcmp)(field_name, LSD_FIELD_UNIQUE_ID)) {
      LSD_RANDFUNC(ptr_to_field, LSD_UIDLEN);
      return;
    }

    if (!(*_lsd_strcmp)(field_name, LSD_FIELD_REFCOUNT)) {
      *((int *)ptr_to_field) = 1;
      return;
    }

    // copy / init new struct
    if (src_struct) {
      if (!(*_lsd_strcmp)(field_name, LSD_FIELD_TOP)
	  && !(*_lsd_strcmp)(strct_type, LSD_SELF_STRUCT_TYPE))
	(*(void **)ptr_to_field) = dst_struct;
      else if (!(*_lsd_strcmp)(field_name, LSD_FIELD_GENERATION))
	(*(int *)ptr_to_field)++;
      return;
    }

    if (!(*_lsd_strcmp)(field_name, LSD_FIELD_IDENTIFIER))
      *(uint64_t *)ptr_to_field = LSD_STRUCT_ID;
    else if (!(*_lsd_strcmp)(field_name, LSD_FIELD_END_ID))
      *(uint64_t *)ptr_to_field = (LSD_STRUCT_ID ^ 0xFFFFFFFFFFFFFFFF);
  }

  // builtin init_cb
  static void _lsd_init_cb(void *strct, const char *strct_type, const char *field_name,
			   void *ptr_to_field) {
    _lsd_init_copy(strct, NULL, strct_type, field_name, ptr_to_field);
  }

  static void _lsd_list_append(lsd_special_field_t **list, lsd_special_field_t *data) {
    if (!*list) *list = data;
    else {
      lsd_special_field_t *xlist;
      for (xlist = *list; xlist->next; xlist = xlist->next);
      xlist->next = data;
    }
  }

  // builtin copy cb
  static void _lsd_copy_cb(void *dst, void *src, const char *strct_type, const char *field_name,
			   void *dst_fld_ptr, void *src_fld_ptr) {
    _lsd_init_copy(dst, src, strct_type, field_name, dst_fld_ptr);}

  static GNU_HOT void _lsd_list_copy_cb(void *dst, void *src, const char *strct_type, const char *field_name,
				void *dst_fld_ptr, void *src_fld_ptr) {
    lsd_special_field_t **slistp = (lsd_special_field_t **)src_fld_ptr;
    lsd_special_field_t **dlistp = (lsd_special_field_t **)dst_fld_ptr, *dlist = NULL;
    for (lsd_special_field_t *slist = *slistp; slist; slist = slist->next) {
      lsd_special_field_t *spcf;
      if (!(*_lsd_calloc_aligned_)((void **)&spcf, 1, LSD_SPECIAL_FIELD_SIZE)) {
	lsd_memerr_print(LSD_SPECIAL_FIELD_SIZE, spcf->name, strct_type);
	break;
      }
      (*_lsd_memcpy)(spcf, slist, LSD_SPECIAL_FIELD_SIZE);
      if (!dlist) *dlistp = dlist = spcf;
      else {
	dlist->next = spcf;
	dlist = dlist->next;
      }}}

  // builtin delete_cb
  static void _lsd_delete_cb(void *strct, const char *strct_type, const char *field_name,
			     void *ptr_to_field) ALLOW_UNUSED;
  static void _lsd_delete_cb(void *strct, const char *strct_type, const char *field_name,
			     void *ptr_to_field) {/* nothing */}

  static void _lsd_list_free_cb(void *strct, const char *strct_type, const char *field_name,
				  void *ptr_to_field) {
    lsd_special_field_t **slist = (lsd_special_field_t **)ptr_to_field, *slist_next;
    for (; *slist; *slist = slist_next) {
      slist_next = (*slist)->next;
      (*_lsd_free)(*slist);
    }
  }

  static int  _lsd_add_special_field(lsd_special_field_t **list, const char *field_name, uint64_t flags,
				     void *ptr_to_field, size_t data_size, void *sample_struct, va_list vargs,
				     lsd_special_field_t **list_last) {

    _lsd_cb_func cb_func;
    void *cb_data;
    lsd_special_field_t *spcf;

    if (flags & LSD_FIELD_FLAG_IS_SUBSTRUCT) {
      if (flags != LSD_FIELD_FLAG_IS_SUBSTRUCT) return LSD_ERROR;
    }

    if (flags == LSD_FIELD_FLAG_IS_SUBSTRUCT) {
      cb_data = va_arg(vargs, void *);
      if (!cb_data) return LSD_ERROR;
      if (!(*_lsd_calloc_aligned_)((void **)&spcf, 1, LSD_SPECIAL_FIELD_SIZE)) {
	lsd_memerr_print(LSD_SPECIAL_FIELD_SIZE, field_name, "?????");
	return LSD_FAIL;
      }
      spcf->init_cb.data = spcf->copy_cb.data = spcf->delete_cb.data = cb_data;
    }
    else {
      if (!(*_lsd_calloc_aligned_)((void **)&spcf, 1, LSD_SPECIAL_FIELD_SIZE)) {
	lsd_memerr_print(LSD_SPECIAL_FIELD_SIZE, field_name, "?????");
	return LSD_FAIL;
      }
      cb_func = va_arg(vargs, void *);
      if (cb_func){
	if (cb_func != lsd_null_cb) {
	  spcf->init_cb.func = cb_func;
	  if (!list_last) {
	    cb_data = va_arg(vargs, void *);
	    spcf->init_cb.data = cb_data;
	  }
	}
	cb_func = va_arg(vargs, void *);
	if (!cb_func) {
	  (*_lsd_free)(spcf);
	  return LSD_ERROR;
	}
	if (cb_func != lsd_null_cb) {
	  spcf->copy_cb.func = cb_func;
	  if (!list_last) {
	    cb_data = va_arg(vargs, void *);
	    spcf->copy_cb.data = cb_data;
	  }
	}
	cb_func = va_arg(vargs, void *);
	if (!cb_func) {
	  (*_lsd_free)(spcf);
	  return LSD_ERROR;
	}
	if (cb_func != lsd_null_cb) {
	  spcf->delete_cb.func = cb_func;
	  if (!list_last) {
	    cb_data = va_arg(vargs, void *);
	    spcf->delete_cb.data = cb_data;
	  }}}}

    spcf->flags = flags;
    spcf->offset_to_field = ptr_to_field - sample_struct;
    if (field_name)(*_lsd_snprintf)(spcf->name, LSD_FIELD_NAME_LEN, "%s", field_name);
    spcf->byte_size = data_size;
    if (!*list || !list_last) _lsd_list_append(list, spcf);
    else _lsd_list_append(list_last, spcf);
    if (list_last) *list_last = spcf;
    return LSD_SUCCESS;
  }

  static inline void _lsd_add_self_field(lsd_struct_def_t *lsd, const char *field_name, uint64_t flags,
							  void *ptr_to_field, size_t data_size, void *sample_struct,
							  lsd_special_field_t **list_last, ...) {
    va_list vargs;
    va_start(vargs, list_last);
    _lsd_add_special_field(&lsd->self_fields, field_name, flags, ptr_to_field, data_size, sample_struct, vargs, list_last);
    va_end(vargs);
  }

  // auto (flagbit) handlers

  static void _lsd_auto_delete(void *ptr, uint64_t flags, size_t bsize) {
    if (flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE) {
      if (!(flags & LSD_FIELD_FLAG_IS_NULLT_ARRAY)) {
	flags &= ~LSD_FIELD_FLAG_FREE_ALL_ON_DELETE;
	flags |= LSD_FIELD_FLAG_FREE_ON_DELETE;
      }
    }
    if (flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE) {
      void **vptr = *((void ** *)ptr);
      if (vptr) {
	if ((flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) && !bsize) {
	  for (int j = 0; vptr[j]; j++) if (vptr[j])(*_lsd_string_free)(vptr[j]);
	} else for (int j = 0; vptr[j]; j++) if (vptr[j])(*_lsd_free)(vptr[j]);
      }
    }
    if (flags & LSD_FIELD_FLAG_FREE_ON_DELETE) {
      void *vptr = *((void **)ptr);
      if (vptr) {
	//g_print("flags !!! %" PRIu64 " %" PRIu64 "\n", flags, sizee);
	if ((flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) && !bsize
	    && !(flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE))
	  (*_lsd_string_free)(vptr);
	else (*_lsd_free)(vptr);
      }}}

  static void _lsd_auto_copy(void *dst_field, void *src_field, lsd_special_field_t *spcf,
			     lsd_struct_def_t *lsd) GNU_HOT;
  static void _lsd_auto_copy(void *dst_field, void *src_field, lsd_special_field_t *spcf,
			     lsd_struct_def_t *lsd) {
    size_t bsize = spcf->byte_size;
    int j;
    if (!(spcf->flags & LSD_FIELD_FLAG_IS_NULLT_ARRAY)) {
      // non-array
      if (spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) {
	if (bsize) {
	  // non-string
	  if (bsize > LSD_MAX_ALLOC) {
	    lsd_debug_print("error: memory request too large (%" PRIu64 " > %" PRIu64 ")\n", bsize, LSD_MAX_ALLOC);
	    return;
	  } else {
	    lsd_debug_print("allocating %" PRIu64 " bytes at %p (%p)...", bsize, dst_field, *(void **)dst_field);
	    if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
	      // alloc and zero
	      if ((*_lsd_calloc_aligned_)((void **)&dst_field, 1, bsize)) {
		lsd_debug_print("and set to zero.\n");
	      } else lsd_memerr_print(bsize, spcf->name, lsd->struct_type);
	      return;
	    } else {
	      // alloc and copy
	      if (src_field != lsd && !(*((void **)src_field))) {
		lsd_debug_print("value is NULL, not copying\n");
	      } else {
		if ((*_lsd_calloc_aligned_)((void **)dst_field, 1, bsize)) {
		  lsd_debug_print("and copying from src %p to dest %p.\n",
				  *(void **)src_field, *(void **)dst_field);
		  (*_lsd_memcpy)(*(void **)dst_field, src_field, bsize);
		} else {
		  lsd_memerr_print(bsize, spcf->name, lsd->struct_type);
		  return;
		}}}}} else {
	  // string (bsize == 0)
	  char **cptr = (char **)dst_field;
	  if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
	    // set the string to an empty string
	    *cptr = (*_lsd_strdup)("");
	    lsd_debug_print("string was set to \"\"\n");
	  } else {
	    if ((*((char **)src_field))) {
	      *cptr = (*_lsd_strdup)(*((char **)src_field));
	      lsd_debug_print("did a strdup from src to dest\n");
	    } else {
	      *cptr = NULL;
	      lsd_debug_print("value is NULL, not copying\n");
	    }}}

	if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE))
	  lsd_debug_print("WARNING: FREE_ON_DELETE not set\n");
	if (spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE)
	  lsd_debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
	return;
      }
      // non-alloc
      if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
	if (bsize) {
	  (*_lsd_memset)(dst_field, 0, bsize);
	  lsd_debug_print("zeroed %" PRIu64 " bytes\n", bsize);
	} else {
	  *((char **)dst_field) = NULL;
	  lsd_debug_print("set string to NULL\n");
	}
      }
      if ((spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) && !bsize) {
	if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE)) {
	  lsd_debug_print("WARNING: FREE_ON_DELETE not set\n");
	}
      } else if (spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE) {
	lsd_debug_print("WARNING: FREE_ON_DELETE is set\n");
      }
      if (spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE) {
	lsd_debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
      }
      return;
    } else {
      // nullt array
      int count = 0;
      lsd_debug_print("handling array...");
      // copy / create n elements or strings
      if (!bsize) {
	// copy N strings or create empty strings, source field is char **
	char **cptr = (*(char ** *)src_field), **dptr;
	if (cptr) {
	  while (cptr[count]) count++;
	  if (!(*_lsd_calloc_aligned_)((void **)&dptr, count + 1, sizeof(char *))) {
	    lsd_memerr_print(bsize, spcf->name, lsd->struct_type);
	    return;
	  }
	  for (j = 0; j < count; j++) {
	    // flags tells us what to with each element
	    if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
	      if (spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY)
		dptr[j] = (*_lsd_strdup)("");
	      else {
		///set to NULL - but deliver a warning
		lsd_debug_print("WARNING - non-last sttring set to NULL");
		dptr[j] = NULL;
	      }
	    } else {
	      if (spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) {
		dptr[j] = (*_lsd_strdup)(cptr[j]);
	      } else dptr[j] = cptr[j];
	    }
	  }
	  dptr[j] = NULL; /// final element must always be NULL
	  (*(char ** *)dst_field) = dptr;
	}

	if (!cptr) {
	  lsd_debug_print("value is NULL, not copying\n");
	} else if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
	  lsd_debug_print("created %d empty strings (+ terminating NULL)\n", count);
	} else if (spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) {
	  lsd_debug_print("duplicated %d strings (+ terminating NULL)\n", count);
	} else {
	  lsd_debug_print("copy-by-ref %d strings (+ terminating NULL)\n", count);
	}

	if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE)) {
	  lsd_debug_print("WARNING: FREE_ON_DELETE not set\n");
	}
	if (!(spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) &&
	    !(spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY)) {
	  if (spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE) {
	    lsd_debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
	  }
	} else if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE)) {
	  lsd_debug_print("WARNING: FREE_ALL_ON_DELETE not set\n");
	}
	return;
      } else {
	/// array of elements of bsize
	if (spcf->flags & (LSD_FIELD_FLAG_ALLOC_AND_COPY
			   | LSD_FIELD_FLAG_ZERO_ON_COPY)) {
	  /// alloc and copy elements of size bsize
	  void **vptr = (*(void ** *)src_field), **dptr = NULL;
	  if (vptr) {
	    count = 0;
	    while (vptr[count]) count++;
	    if (!(*_lsd_calloc_aligned_)((void **)dptr, count + 1, sizeof(void *))) {
	      lsd_memerr_print((count + 1) * sizeof(void *), spcf->name, lsd->struct_type);
	      return;
	    }
	    for (j = 0; j < count; j++) {
	      // flags tells us what to with each element
	      if (spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) {
		if (!(*_lsd_calloc_aligned_)((void **)&dptr[j], 1, bsize)) {
		  lsd_memerr_print(bsize, spcf->name, lsd->struct_type);
		  return;
		}
		if (!(spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY))
		  (*_lsd_memcpy)(dptr[j], vptr[j], bsize);
	      }
	    }
	    dptr[j] = NULL; /// final element must always be NULL
	    (*(void **)dst_field) = dptr;

	    if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
	      lsd_debug_print("created %d pointers to empty elements of size %" PRIu64 " "
			      "(+ terminating NULL)\n", count, bsize);
	    } else {
	      lsd_debug_print("duplicated %d pointers to elements of size %" PRIu64 " "
			      "(+ terminating NULL)\n", count, bsize);
	    }
	  } else lsd_debug_print("value is NULL, not copying\n");

	  if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE))
	    lsd_debug_print("WARNING: FREE_ON_DELETE not set\n");
	  if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE))
	    lsd_debug_print("WARNING: FREE_ALL_ON_DELETE not set\n");
	} else {
	  // simple array of bsize elements
	  // copy up to and including an element with all 0's
	  void *oldarea = *((void **)src_field), *newarea;
	  char *ptr = oldarea;
	  if (ptr) {
	    for (count = 0;; count++) {
	      for (j = 0; j < bsize; j++) if (ptr[j]) break;
	      if (j == bsize) break;
	      ptr += bsize;
	    }
	    count++;
	    if (!(*_lsd_calloc_aligned_)((void **)&newarea, count, bsize)) {
	      lsd_memerr_print(bsize, spcf->name, lsd->struct_type);
	      return;
	    } else {
	      (*_lsd_memcpy)(newarea, src_field, count * bsize);
	      *((char **)dst_field) = (char *)newarea;
	    }
	  }
	  if (!ptr) {
	    lsd_debug_print("value is NULL, not copying\n");
	  } else {
	    lsd_debug_print("- copied %d values of size %" PRId64 " (including final 0's)\n",
			    count, bsize);
	  }
	  if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE)) {
	    lsd_debug_print("WARNING: FREE_ON_DELETE not set\n");
	  }
	  if (spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE) {
	    lsd_debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
	  }
	}}}
  }

  static void _lsd_struct_free(lsd_struct_def_t *) ALLOW_UNUSED;
  static void _lsd_struct_free(lsd_struct_def_t *lsd) {
    lsd_special_field_t *spcf;
    uint64_t flags = 0;
    size_t lsd_size = 0;
    void *src_field;
    void *top;

    if (!lsd) return;

    if (lsd->top) {
      /* if (lsd->destroy_struct_callback) */
      /* 	(*lsd->destroy_struct_callback)(lsd->top, lsd->struct_type, lsd->destroy_user_data); */

    for (spcf = lsd->special_fields; spcf; spcf = spcf->next) {
	flags = spcf->flags;
	if (flags & LSD_FIELD_FLAG_IS_SUBSTRUCT) {
	  /// some other kind of substruct, find its lsd and call again
	  /// TODO
	  continue;
	}
	src_field = lsd->top + spcf->offset_to_field;

	if (spcf->delete_cb.func) {
	  (*((lsd_field_delete_cb)spcf->delete_cb.func))(lsd->top, lsd->struct_type, spcf->name,
							 src_field, spcf->delete_cb.data);
	}
	if (!flags) continue;
	_lsd_auto_delete(src_field, flags, spcf->byte_size);
      }
    }

    top = (char *)lsd;
    if (lsd->self_fields->byte_size) top = *((char **)top);

    for (spcf = lsd->self_fields->next; spcf; spcf = spcf->next) {
      uint64_t flags;
      flags = spcf->flags;
      if (!flags) continue;
      if (flags & LSD_FIELD_FLAG_IS_SUBSTRUCT) {
	/// some other kind of substruct, find its lsd and call again
	/// TODO
	continue;
      }
      if (spcf->delete_cb.func) {
	(*((lsd_field_delete_cb)spcf->delete_cb.func))(top, LSD_SELF_STRUCT_TYPE,
						       spcf->name, &src_field,
						       spcf->delete_cb.data);
      src_field = top + spcf->offset_to_field;
      _lsd_auto_delete(&src_field, flags, spcf->byte_size);
    }

    spcf = lsd->self_fields;
    lsd_size = spcf->byte_size;
    flags = spcf->flags;
    top = (char *)lsd->top;

    _lsd_auto_delete(spcf, spcf->flags, 1);

    if (lsd_size) (*_lsd_free)(lsd);

    if (!(flags & LSD_PRIV_NO_DEL_STRUCT))
      if (top)(*_lsd_free)(top);
    }
  }

  static void  _lsd_copy_fields(lsd_struct_def_t *lsd, lsd_special_field_t *spfields,
				void *new_struct) {
    for (lsd_special_field_t *spcf = spfields; spcf; spcf = spcf->next) {
      void *dst_field, *src_field;
      if (spcf->flags) {
	if (spfields == lsd->self_fields) {
	  if (spcf == lsd->self_fields) {
	    src_field = (char *)lsd;
	    if (lsd->self_fields->byte_size) {
	      //dst_Field is lsd_struct_def_t **
	      dst_field = (char *)new_struct + spcf->offset_to_field;
	    }
	    else
	      dst_field = (char *)new_struct + spcf->offset_to_field;
	  }
	  else {
	    src_field = (char *)lsd + spcf->offset_to_field;
	    if (lsd->self_fields->byte_size) {
	      dst_field = (char *)((lsd_struct_def_t *)((char *)new_struct + lsd->self_fields->offset_to_field))
		+ spcf->offset_to_field;
	    }
	    else {
	      dst_field = (char *)new_struct + lsd->self_fields->offset_to_field + spcf->offset_to_field;
	    }
	  }
	}
	else {
	  src_field = (char *)lsd->top + spcf->offset_to_field;
	  dst_field = (char *)new_struct + spcf->offset_to_field;
	}

	lsd_debug_print("handling field %s with flags 0X%016lX\n", spcf->name, spcf->flags);

	if (spcf->flags & LSD_FIELD_FLAG_IS_SUBSTRUCT) {
	  // otherwise we descend into the substruct and locate its lsd_struct_def *
	  // TODO...
	  lsd_debug_print("field is another substruct {TODO}\n");
	  continue;
	}
	_lsd_auto_copy(dst_field, src_field, spcf, lsd);
	lsd_debug_print("field done\n\n");
      }
    }
  }

  static void _lsd_run_callbacks(lsd_struct_def_t *lsd, lsd_special_field_t *spfields,
				 void *new_struct, int is_copy) {

    for (lsd_special_field_t *spcf = spfields; spcf; spcf = spcf->next) {
      char *src_field, *dst_field = NULL;
      if (spcf == lsd->self_fields
	  || (is_copy && !(spcf->flags & LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY)
	      && !spcf->copy_cb.func)
	  || ((!is_copy || (spcf->flags & LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY))
	      && !spcf->init_cb.func)) continue;

      if (spfields == lsd->self_fields) {
	if (spcf == lsd->self_fields) continue;
	// ptr to field in lsd
	// this is correct if lsd->top == 0
	src_field = (char *)lsd + spcf->offset_to_field;
	//
	if (is_copy) {
	  if (lsd->self_fields->byte_size) {
	    lsd_struct_def_t *dst_lsd = *((lsd_struct_def_t **)((char *)new_struct
								+ lsd->self_fields->offset_to_field));
	    dst_field = (char *)dst_lsd + spcf->offset_to_field;
	  }
	  else {
	    dst_field = (char *)new_struct + lsd->self_fields->offset_to_field + spcf->offset_to_field;
	  }
	}
      }
      else {
	src_field = (char *)lsd->top + spcf->offset_to_field;
	dst_field = (char *)new_struct + spcf->offset_to_field;
      }

      if (!is_copy || (spcf->flags & LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY)) {
	lsd_debug_print("calling init_func for %s\n", spcf->name);
	(*((lsd_field_init_cb)spcf->init_cb.func))(new_struct, (spfields == lsd->special_fields)
						? lsd->struct_type : LSD_SELF_STRUCT_TYPE, spcf->name,
						   spfields == lsd->self_fields ? src_field : dst_field,
						   spcf->init_cb.data);
      }

      if (is_copy && spcf->copy_cb.func) {
	lsd_debug_print("calling copy_func for %s\n", spcf->name);
	// for self fields, the src_struct is irrelevent, it just needs to be non zero, so we know it is copy
	(*((lsd_field_copy_cb)spcf->copy_cb.func))(new_struct, spfields == lsd->self_fields ? lsd : lsd->top,
						   (spfields == lsd->special_fields)
						   ? lsd->struct_type : LSD_SELF_STRUCT_TYPE,
						   spcf->name, dst_field, src_field, spcf->copy_cb.data);
      }
    }
  }

  static void *_lsd_struct_copy_init(lsd_struct_def_t *lsd, void *new_struct, int is_copy) {
    lsd_special_field_t *spfields;

    if (!lsd) return NULL;
    spfields = lsd->self_fields;

    if (!spfields) {
      lsd_baderr_print("This does not appear to be a valid structure definition. "
		       "Cannot create or copy using it.\n");
      return NULL;
    }
    if (is_copy && !lsd->top) {
      lsd_baderr_print("Unable to copy struct of type %s, "
		       "this is a template which can only be used to create new structs\n",
		       lsd->struct_type);
      return NULL;
    }

    if (!lsd->struct_size) {
      lsd_baderr_print("Cannot create or copy a struct type %s with size 0", lsd->struct_type);
      return NULL;
    }

    if (!new_struct) {
      lsd_debug_print("allocating struct of type: %s, with size %" PRIu64 "\n",
		      lsd->struct_type, lsd->struct_size);
      if (!(*_lsd_calloc_aligned_)((void **)&new_struct, 1, lsd->struct_size)) {
	lsd_memerr_print(lsd->struct_size, "ALL FIELDS", lsd->struct_type);
	return NULL;
      } else lsd_debug_print("initing struct %p of type: %s\n", new_struct, lsd->struct_type);
      if (is_copy) {
	lsd_debug_print("copy struct to struct\n");
	(*_lsd_memcpy)(new_struct, lsd->top, lsd->struct_size);
      }
    }

    // struct allocated
    // now copy the lsd
    lsd_debug_print("copying lsd_struct_def_t fields first\n");

    // run auto copy methods
    _lsd_copy_fields(lsd, spfields, new_struct);

    // run copy callbacks
    _lsd_run_callbacks(lsd, spfields, new_struct, 1);

    // now handle special fields
    spfields = lsd->special_fields;

    if (spfields) {
      if (!is_copy) {
	lsd_debug_print("initing special fields\n");
	_lsd_run_callbacks(lsd, spfields, new_struct, 0);
      } else {
	lsd_debug_print("copying special fields\n");
	_lsd_copy_fields(lsd, spfields, new_struct);
	_lsd_run_callbacks(lsd, spfields, new_struct, 1);
      }
    }

    if (is_copy) {lsd_debug_print("struct copy done\n\n");}
    else {lsd_debug_print("struct init done\n\n");}

    if (is_copy && lsd->self_fields->copy_cb.func)
      (((lsd_struct_copied_cb)lsd->self_fields->copy_cb.func))
	(lsd->top, new_struct, lsd->struct_type, lsd->self_fields->copy_cb.data);

    if (lsd->self_fields->init_cb.func)
      ((lsd_struct_new_cb)lsd->self_fields->init_cb.func)(new_struct, lsd->top, lsd->struct_type,
							  lsd->self_fields->init_cb.data);

    return new_struct;
  }

  static void *__lsd_struct_create(const lsd_struct_def_t *lsd, void *thestruct) {
    return _lsd_struct_copy_init((lsd_struct_def_t *)lsd, thestruct, 0);
  }

  static const lsd_struct_def_t *_lsd_create(const char *struct_type, char *mystruct, size_t struct_size,
					     lsd_struct_def_t **lsd_in_struct, int is_ptr) {
    lsd_struct_def_t *lsd;
    lsd_special_field_t *list_last = NULL;

    if (!struct_type || !mystruct || !struct_size || (is_ptr && !lsd_in_struct)
	|| (!is_ptr && !*lsd_in_struct)) return NULL;

    if (!(*_lsd_calloc_aligned_)((void **)&lsd, 1, LSD_SELF_STRUCT_SIZE)) {
      lsd_memerr_print(LSD_SELF_STRUCT_SIZE, "LSD template", struct_type);
      return NULL;
    }

    if (struct_type)(*_lsd_snprintf)(lsd->struct_type, LSD_STRUCT_TYPE_LEN, "%s", struct_type);

    lsd->struct_size = struct_size;

    // this is synthetic field which holds details of the LSD struct itself
    if (is_ptr) _lsd_add_self_field(lsd, LSD_FIELD_LSD, LSD_FIELD_FLAG_ALLOC_AND_COPY, (char *)lsd_in_struct,
				    LSD_SELF_STRUCT_SIZE, (char *)mystruct, &list_last, NULL);
    else _lsd_add_self_field(lsd, LSD_FIELD_LSD, LSD_FIELD_FLAGS_NONE, (char *)(*lsd_in_struct),
			     0, (char *)mystruct, &list_last, NULL);

    _lsd_add_self_field(lsd, LSD_FIELD_IDENTIFIER, LSD_FIELD_FLAGS_NONE, (char *)&lsd->identifier,
			0, (char *)lsd, &list_last, _lsd_init_cb, lsd_null_cb, lsd_null_cb);
    // set a new random value on init / copy
    _lsd_add_self_field(lsd, LSD_FIELD_UNIQUE_ID, LSD_FIELD_FLAGS_NONE, (char *)&lsd->unique_id,
			0, (char *)lsd, &list_last, _lsd_init_cb, _lsd_copy_cb, lsd_null_cb);
    _lsd_add_self_field(lsd, LSD_FIELD_TOP, LSD_FIELD_FLAGS_NONE, (char *)&lsd->top,
			0, (char *)lsd, &list_last, _lsd_init_cb, _lsd_copy_cb, lsd_null_cb);

    _lsd_add_self_field(lsd, LSD_FIELD_SELF_FIELDS, LSD_FIELD_FLAGS_NONE, (char *)&lsd->self_fields,
			0, (char *)lsd, &list_last, lsd_null_cb, _lsd_list_copy_cb, _lsd_list_free_cb);
    _lsd_add_self_field(lsd, LSD_FIELD_SPECIAL_FIELDS, LSD_FIELD_FLAGS_NONE, (char *)&lsd->special_fields,
			0, (char *)lsd, &list_last, lsd_null_cb, _lsd_list_copy_cb, _lsd_list_free_cb);

    // set to 1 on init, increment on copy
    _lsd_add_self_field(lsd, LSD_FIELD_GENERATION, LSD_FIELD_FLAGS_NONE, (char *)&lsd->generation,
			0, (char *)lsd, &list_last, _lsd_init_cb, _lsd_copy_cb, lsd_null_cb);
    // value will be set to zero after copying
    _lsd_add_self_field(lsd, LSD_FIELD_REFCOUNT, LSD_FIELD_FLAGS_NONE,(char *)&lsd->refcount,
			0, (char *)lsd, &list_last, _lsd_init_cb, _lsd_copy_cb, lsd_null_cb);
    _lsd_add_self_field(lsd, LSD_FIELD_USER_DATA, LSD_FIELD_FLAGS_NONE,	(char *)&lsd->user_data,
			0, (char *)lsd, &list_last, NULL);
    // set to value
    _lsd_add_self_field(lsd, LSD_FIELD_END_ID, LSD_FIELD_FLAGS_NONE, (char *)&lsd->end_id,
			0, (char *)lsd, &list_last, _lsd_init_cb, lsd_null_cb, lsd_null_cb);

    _lsd_run_callbacks(lsd, lsd->self_fields, mystruct, 0);
    return lsd;
  }

  static int lsd_struct_get_generation(lsd_struct_def_t *lsd) {return !lsd ? -1 : lsd->generation;}
  static uint64_t lsd_struct_get_uid(lsd_struct_def_t *lsd) {return !lsd ? 0 : lsd->unique_id;}
  static const char *lsd_struct_get_type(lsd_struct_def_t *lsd) {return !lsd ? NULL : lsd->struct_type;}
  static uint64_t lsd_struct_get_identifier(lsd_struct_def_t *lsd) {return !lsd ? 0ul : lsd->identifier;}
  static uint64_t lsd_struct_get_end_id(lsd_struct_def_t *lsd) {return !lsd ? 0ul : lsd->end_id;}
  static void *lsd_struct_get_user_data(lsd_struct_def_t *lsd) {return !lsd ? NULL : lsd->user_data;}
  static size_t lsd_struct_get_size(lsd_struct_def_t *lsd) {return !lsd ? 0 : lsd->struct_size;}
  static void lsd_struct_set_user_data(lsd_struct_def_t *lsd, void *data) {if (lsd) lsd->user_data = data;}
  static const char *lsd_struct_get_class_id(lsd_struct_def_t *lsd) {return !lsd ? NULL : lsd->class_id;}
  static void lsd_struct_set_class_id(lsd_struct_def_t *lsd, const char *class_id) {
    if (lsd)(*_lsd_snprintf)(lsd->class_id, LSD_CLASS_ID_LEN, "%s", class_id);
  }
  static uint64_t lsd_struct_get_owner_uid(lsd_struct_def_t *lsd) {return !lsd ? 0 : lsd->owner_uid;}
  static uint64_t lsd_struct_set_owner_uid(const lsd_struct_def_t *lsd, uint64_t owner_uid) {
    if (!lsd) return 0;
    if (!lsd->generation && !lsd->owner_uid && !lsd->top) ((lsd_struct_def_t *)lsd)->owner_uid = owner_uid;
    return lsd->owner_uid; }
  static void lsd_add_special_field(lsd_struct_def_t *lsd, const char *field_name, uint64_t flags, void *ptr_to_field,
				size_t data_size, void *sample_struct, ...) {
    va_list vargs; va_start(vargs, sample_struct);
    _lsd_add_special_field(&lsd->special_fields, field_name, flags, (char *)ptr_to_field, data_size,
			   (char *)sample_struct, vargs, NULL); va_end(vargs);}
  static int lsd_struct_ref(lsd_struct_def_t *lsd) {return lsd ? ++((lsd_struct_def_t *)lsd)->refcount : LSD_ERROR;}
  static int lsd_struct_unref(lsd_struct_def_t *lsd) {
    if (lsd) {
      if (!lsd->self_fields) {
	lsd_baderr_print("Invalid LSD, cannot free it.\n");
	return LSD_ERROR;;
      }
      else {
	int rc = --(((lsd_struct_def_t *)lsd)->refcount);
	if (rc <= 0) _lsd_struct_free((lsd_struct_def_t *)lsd);
	return LSD_SUCCESS;
      }
    }
    return LSD_ERROR;
  }
  static int lsd_struct_get_refcount(lsd_struct_def_t *lsd) {return lsd ? lsd->refcount : LSD_ERROR;}
  static int lsd_struct_free(lsd_struct_def_t *lsd) {return lsd_struct_unref(lsd);}
  static void *lsd_struct_copy(lsd_struct_def_t *lsd) {return lsd->top ? _lsd_struct_copy_init(lsd, NULL, 1) : 0;}
  static void *lsd_struct_create(const lsd_struct_def_t *lsd) {return __lsd_struct_create(lsd, NULL);}
  static void *lsd_struct_initialise(const lsd_struct_def_t *lsd, void *thestruct) {
    // initialises an already allocaed struct
    thestruct =  __lsd_struct_create(lsd, thestruct);
    if (thestruct) {
      lsd_struct_def_t *dst_lsd = NULL;
      if (lsd->self_fields->byte_size) {
	lsd_struct_def_t **dst_lsdp = (lsd_struct_def_t **)((char *)thestruct + lsd->self_fields->offset_to_field);
	dst_lsd = *dst_lsdp;
      } else dst_lsd = (lsd_struct_def_t *)((char *)thestruct +  lsd->self_fields->offset_to_field);
      if (dst_lsd && dst_lsd->self_fields)
	dst_lsd->self_fields->flags |= LSD_PRIV_NO_DEL_STRUCT;
    }
    return thestruct;
  }
  static int lsd_same_family(const lsd_struct_def_t *lsd1, lsd_struct_def_t *lsd2) {
    // must have same identifier, end_id, struct_type, struct_size, class_id
    if (lsd1->struct_size == lsd2->struct_size
	&& lsd1->identifier == lsd2->identifier
	&& lsd1->end_id == lsd2->end_id
	&& (!(*_lsd_strcmp)(lsd1->struct_type, lsd2->struct_type))
	&& (!(*_lsd_strcmp)(lsd1->class_id, lsd2->class_id))) return LSD_SUCCESS;
    return LSD_FAIL;}

  static const lsd_struct_def_t *lsd_create(const char *struct_type, void *mystruct, size_t struct_size,
					    lsd_struct_def_t *lsd_in_struct) {
    return _lsd_create(struct_type, mystruct, struct_size, &lsd_in_struct, 0); }

  static const lsd_struct_def_t *lsd_create_p(const char *struct_type, void *mystruct, size_t struct_size,
					      lsd_struct_def_t **lsd_in_struct) {
    return _lsd_create(struct_type, mystruct, struct_size, lsd_in_struct, 1); }

  /////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
#undef DEBUG
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/// the process works as follows
// 1) make 1 instance of struct via normal means (e.g. malloc()) - the contents are irrelevant, it is only
// needed to get the offsets of fields for steps 2 and 3:
//
/// 2) call lsd_create(), passing in the (const char *) name of the struct, the dummy copy (cast to void *),
// sizeof(struct_type) and a pointer to the lsd field in the struct.
// If the lsd in the struct is a pointer, call lsd_create_p() instead. eg
//
//		my_struct_t *mystruct = malloc(sizeof(my_struct_t);
//
//		const lsd_struct_def_t *lsd = lsd_create("my_struct_t", (void *)mystruct, sizeof(mystruct),
//			&(mystruct->lsd));
// or:
// 		const lsd_struct_def_t *lsd = lsd_create_p("my_struct_t", (void *)mystruct, sizeof(mystruct),
//			&(mystruct->lsd));
//
// or using the macros there is no need to malloc the dummy, this will be done for you. Just:
//		const lsd_struct_def_t *lsd = LSD_CREATE(my_struct_t, lsd);
//
//		const lsd_struct_def_t *lsd = LSD_CREATE_P(my_struct_t, lsd);
///
/// 2) If the struct has 'special' fields then for each one, call
//		lsd_add_apecial_field(lsd, flags, mystruct, ptr_to_field, field_name,
//				element size (in case this is relevant), init_cb, copy_cb, delete_cn);
//
// you can also add more special fields later on, this is useful in case callbacks point to static functions
//
// the dummy struct can then be initialised with:
//		lsd_struct_initialise(my_struct, lsd);
// this will create an lsd inside the struct, and initialise any special fields
//
// or else you can just free(dummy_struct)
// you can then do:
//			my_struct_t *new_struct = lsd_struct_create(lsd);
//			my_struct_t *new_struct = lsd_struct_create(mystruct->lsd);
//			my_struct_t *new_truct = lsd_struct_copy(mystruct->lsd);
//			lsd_struct_ref(lsd);
//			lsd_struct_ref(mystruct->lsd);
//			lsd_struct_unref(lsd);
//			lsd_struct_unref(mystruct->lsd);
//			lsd_struct_initialise(lsd, nwe_struct);
//			lsd_struct_initialise(mystruct->lsd, new_struct);
//			lsd_struct_clone(dst_struct->lsd, src_struct->lsd);
//
// Note: each time a struct is created or copied, the lsd template creates a replica of
// itself using itself as the template - self replicating code. This helps to keep the code base
// smaller and more efficient
//
// other functions include: generation values, classID, owner uid
// - you can get struct type, size, UID (unique per instance), check for equivalent structs,
// as well as getting and setting user data per instance

// NB. once support for substructs is added, it will possible for a single struct to contain
