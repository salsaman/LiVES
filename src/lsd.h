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

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

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
#define GNU_MALLOC_SIZE2(argx, argy) __attribute__((alloc_size(argx, argy)))
#define GNU_HOT __attribute__((hot))

#ifndef OVERRIDE_MEMFUNCS
#ifdef USE_POSIX_MEMALIGN
#define GNU_ALIGNED(sizex) __attribute__((assume_aligned(sizex)))
#endif
#endif

#else
#define ALLOW_UNUSED
#define GNU_MALLOC_SIZE2(x, y)
#define GNU_HOT

#ifndef OVERRIDE_MEMFUNCS
#ifdef USE_POSIX_MEMALIGN
#define GNU_ALIGNED(x)
#endif
#endif

#endif

#define LSD_STRUCT_TYPE_LEN 64 ///< max len (bytes) for struct type names
#define LSD_FIELD_NAME_LEN 32 ///< max field name length (bytes)
#define LSD_CLASS_ID_LEN 32 ///< max len for class_id
#define LSD_MAX_ALLOC 65535
#define LIVES_STRUCT_ID 0x4C7C56332D2D3035  /// 1st 8 bytes - L|V3--05 (be) or 50--3V|L (le)

enum {
  lsd_self_field_lsd = 0,
  lsd_self_field_identifier,
  lsd_self_field_unique_id,
  lsd_self_field_refcount,
  lsd_self_field_generation,
  lsd_self_field_top,
  lsd_self_field_special_fields,
  lsd_self_field_self_fields,
  lsd_self_field_user_data,
  lsd_self_field_end_id,
  lsd_n_self_fields,
};

#define LSD_N_SELF_FIELDS			lsd_n_self_fields

#define LSD_FIELD_LSD				"LSD"
#define LSD_FIELD_LSD_IDX			lsd_self_field_lsd

#define LSD_FIELD_IDENTIFIER		       	"identifier"
#define LSD_FIELD_IDENTIFIER_IDX		lsd_self_field_identifier
#define LSD_FIELD_UNIQUE_ID			"unique_id"
#define LSD_FIELD_UNIQUE_ID_IDX			lsd_self_field_unique_id
#define LSD_FIELD_REFCOUNT			"refcount"
#define LSD_FIELD_REFCOUNT_IDX			lsd_self_field_refcount
#define LSD_FIELD_GENERATION			"generation"
#define LSD_FIELD_GENERATION_IDX		lsd_self_field_generation
#define LSD_FIELD_TOP				"top"
#define LSD_FIELD_TOP_IDX			lsd_self_field_top
#define LSD_FIELD_SPECIAL_FIELDS		"spcl_fields"
#define LSD_FIELD_SPECIAL_FIELDS_IDX		lsd_self_field_special_fields
#define LSD_FIELD_SELF_FIELDS			"self_fields"
#define LSD_FIELD_SELF_FIELDS_IDX		lsd_self_field_self_fields
#define LSD_FIELD_USER_DATA			"user_data"
#define LSD_FIELD_USER_DATA_IDX			lsd_self_field_user_data
#define LSD_FIELD_END_ID			"end_id"
#define LSD_FIELD_END_ID_IDX			lsd_self_field_end_id

#define LSD_SUCCESS 1
#define LSD_FAIL 0
#define LSD_ERROR -1

#ifndef OVERRIDE_MEMFUNCS
#ifndef USE_POSIX_MEMALIGN
static void *(*_lsd_calloc)(size_t nmemb, size_t size) = calloc;
#endif
static void *(*_lsd_memcpy)(void *dest, const void *src, size_t n) = memcpy;
static void *(*_lsd_memset)(void *s, int c, size_t n) = memset;
static void (*_lsd_free)(void *ptr) = free;
#endif

#ifndef OVERRIDE_STRFUNCS
static int(*_lsd_strcmp)(const char *s1, const char *s2) = strcmp;
static char *(*_lsd_strdup)(const char *s) = strdup;
static int(*_lsd_snprintf)(char *s, size_t n, const char *format, ...) = snprintf;
static void (*_lsd_string_free)(void *ptr) = free;
#endif

#ifndef OVERRIDE_CALLOC_ALIGNED

#ifdef USE_POSIX_MEMALIGN

#ifndef _MEM_ALIGNMENT_
#define _MEM_ALIGNMENT_ 64 // or whatever power of 2
#endif
static void *_lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size)
GNU_ALIGNED(MEM_ALIGNMENT) GNU_MALLOC_SIZE2(2, 3);

static void *_lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size);
GNU_ALIGNED(MEM_ALIGNMENT) GNU_MALLOC_SIZE2(2, 3) {
  int ret = posix_memalign(memptr, _MEM_ALIGNMENT_, nmemb * size);
  if (!ret && *memptr)(*_lsd_memset)(*memptr, 0, nmemb * size);
  return !ret ? *memptr : NULL;
}
#else // posix
static void *_lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size) GNU_MALLOC_SIZE2(2, 3);
static void *_lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size) {
  return !memptr ? NULL : (!(*memptr = (*_lsd_calloc)(nmemb, size))) ? NULL : *memptr;
}
#endif  // over
#else
// fn overiden
extern void *(*_lsd_calloc_aligned_)(void **memptr, size_t nmemb, size_t size);
#endif //

/// AUTOMATION FLAGS

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
/// field wiill be freed in lives_struct_delete
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

/// field is a substruct with its own lives_struct_def_t; functions may be called depth first
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

// forward decl

typedef struct _lives_struct_def lives_struct_def_t;

// CALLBACK FUNCTION TYPEDEFS

// struct callbacks

// this is called from lives_struct_new after all fields have been initialised via init_cbunc
typedef void (*lives_struct_new_cb)(void *strct, void *parent, const char *strct_type,
                                    void *new_user_data);

// this is called from lives_struct_copy after a copy is made
typedef void (*lives_struct_copied_cb)(void *strct, void *child, const char *strct_type,
                                       void *copied_user_data);

// this is called from lives_struct_free before any fields are freed or delete_func called
typedef void (*lives_struct_destroy_cb)(void *strct, const char *strct_type, void *delete_user_data);

// field callbacks

//called from lives_struct_new
typedef void (*lsd_field_init_cb)(void *strct, const char *struct_type,
                                  const char *field_name, void *ptr_to_field);

// this is called from lives_struct_copy after all automatic updates are performed
typedef void (*lsd_field_copy_cb)(void *dst_struct, void *src_struct, const char *strct_type,
                                  const char *field_name, void *ptr_to_dst_field,
                                  void *ptr_to_src_field);

//called from lives_struct_free before any fields are finalised
typedef void (*lsd_field_delete_cb)(void *strct, const char *struct_type,
                                    const char *field_name, void *ptr_to_field);
// STRUCTS

typedef struct _lsd_special_field {
  /// flags may be  0 to optionally provide info re. the field name, byte_size,
  //(and optionally offset_to_field)
  uint64_t flags;
  /// must be set when creating the struct
  off_t offset_to_field;
  char name[LSD_FIELD_NAME_LEN]; /// optional unless flags == 0 or any of the fns. below are defined.
  size_t byte_size; /// defines the element size for alloc and copy
  lsd_field_init_cb init_func; ///< will be called from lives_struct_new
  lsd_field_copy_cb copy_func; ///< will be called from lives_struct_copy
  lsd_field_delete_cb delete_func; ///< called from lives_struct_free
} lsd_special_field_t; /// 128 bytes

typedef struct _lives_struct_def {
  uint64_t identifier;  /// default: LIVES_STRUCT_ID
  uint64_t unique_id; /// randomly generated id, unique to each instance

  int32_t refcount; ///< refcount, set to 1 on creation, free unrefs and only frees when 0.
  uint32_t generation; ///< initialized as 1 and incremented on each copy

  void *top; ///< ptr to the start of parent struct itself, typecast to a void *

  char struct_type[LSD_STRUCT_TYPE_LEN]; /// type of the struct as string, e.g "lives_struct_def_t"
  size_t struct_size; ///< byte_size of parent struct (sizef(struct))
  char last_field[LSD_FIELD_NAME_LEN]; ///< name of last field of struct (informational only)

  lives_struct_new_cb new_struct_callback;  ///< called from lives_struct_new
  void *new_user_data; /// user_data for new_struct_callback

  lives_struct_copied_cb copied_struct_callback;  ///< called from lives_struct_copy
  void *copied_user_data; /// user_data for clone_struct_callback

  lives_struct_destroy_cb destroy_struct_callback; /// called from lives_struct_free if refcount is 0
  void *destroy_user_data; /// user_data for delete_struct_callback

  int nspecial;
  lsd_special_field_t **special_fields;  /// may be NULL, else is pointer to NULL terminated array
  lsd_special_field_t **self_fields;  /// fields in the struct_def_t struct itself

  char class_id[LSD_CLASS_ID_LEN]; /// user definable class_id, value is copied to any clones
  void *user_data; /// user_data for instances of struct, reset on copy
  uint64_t end_id;  /// end marker. == identifier ^ 0xFFFFFFFFFFFFFFFF
} lives_struct_def_t; /// 256 bytes

// it is also possible to create a static struct_def, in which case the following is true
// unique_id is 0, top is NULL, refcount is 0, generation is 0,
// a static version may be copied to struct_def for a struct, this is like a normal copy

#define SELF_STRUCT_TYPE "lives_struct_def_t"

///// API ////////////////////

/// the process works as follows:
/// 1) call lsd_create(), passing in the (string) name of the struct, size in bytes, last field name (optional),
/// and number of "special fields"
/// (special fields are any which want automatic init / copy / delete functions)
/// the return value is an lsd "handle" to be used in the following steps
///
/// 2) If the struct has special_fields then:
/// call make_special_field() for each element in lsd->special_fields
/// for this it is necessary to make 1 instance of struct via normal means (e.g. malloc())
/// field values in the dummy struct are irrelevant, the struct is only needed to get the offsets
/// of the special fields.
///
/// (it is also possible to change or define the special fields later, either in the tmeplate or
/// in a struct instance, although you cannot add more)
//.
/// 3) finally, call lives_struct_init(), passing in lsd, pointer to the 'dummy' struct and pointer to lsd field in
/// the struct. If the lsd field is a pointer, call lives_struct_init_p() instead, and pass the address
/// of the pointer field.
/// This serves to finalise the lsd, and initialize field values in the struct + lsd in struct.
/// You can then decide whether to keep hold of the lsd template as is, in which case
// the dummy struct, should be freed and not reused - else the template can be created as the
// lsde field in the sample struct, which can then be utilised to make new structs, or copies (clones)
// of itself
// technical:
// Note each time a struct is created or copied, the lsd template creates a replica of
// itself using itself as the template. (self replicating code). This helps to keep the code base
// smaller and more efficient - the biggest complication is having to take pointers to field offests
// and dereference these as necessary. The main thing to note is the "byte_size" of the LSD field
// - if this is zero then the LSD is a static struct in the parent and we measure offsets from
// "top" (pointer to the start of containing struct) + offset to LSD; if this is non zero
/// (sizeof(lives_struct_def_t)) then it is a pointer and we must dereference the field pointer
/// and measure offsets from there instead

///
/// NB. a field of type lives_struct_def_t must be somewhere in the "parent" struct, it can be anywhere
/// inside it, and it can be a static struct, or pointer to struct (it will be automatically allocated
/// copied and freed). This serves as a template for making copies and for freeing the the struct.
///
/// the returned (lives_struct_def_t *)static_lsd from step 3) can now be used as a "template" to make new structs
/// - lives_struct_create(static_lsd) will return a new struct from the template
/// - lives_struct_copy(&struct->lsd) will make a copy of struct, following the special field rules
//  - lives_struct_free(&struct->lsd) will free struct, following special field rules
// other functions include: refcounting, generation values, classID, user_data
// - you can get struct type, size, UID (unique per instance), check for equivalent structs,
/// as well as getting and setting user data per instance

/// NB. once support for substructs is added, it will possible for a single struct to cantain
/// multiple lsd, each with its own size, last_field and special_fields

static int lsd_free(const lives_struct_def_t *) ALLOW_UNUSED;
static int lsd_unref(const lives_struct_def_t *) ALLOW_UNUSED;
static int lsd_ref(const lives_struct_def_t *) ALLOW_UNUSED;
static int lsd_get_refcount(const lives_struct_def_t *) ALLOW_UNUSED;

// returns LSD_SUCCESS if both struct defs have  same identifier, end_id, struct_type,
// struct_size, last_field, class_id (i.e one is copy / instance of another)
// (lives_struct_get_generation can provide more information), otherwise LSD_FAIL
static int lsd_same_family(const lives_struct_def_t *lsd1, lives_struct_def_t *lsd2) ALLOW_UNUSED;

// sets class data which will be copied to all instances from template
// and from instance to copies.
static void lives_struct_set_class_id(lives_struct_def_t *, const char *class_id) ALLOW_UNUSED;

// The returned value must not be freed or written to.
static const char *lives_struct_get_class_id(lives_struct_def_t *) ALLOW_UNUSED;

static const lives_struct_def_t *lsd_create(const char *struct_type,
    size_t struct_size, const char *last_field,
    int nspecial) ALLOW_UNUSED;

// function to define special fields, array elements returned from make_structdef
// should be assigned via this function
static lsd_special_field_t *
make_special_field(uint64_t flags, void *sample_struct, void *ptr_to_field,
                   const char *field_name, size_t data_size,
                   lsd_field_init_cb init_func, lsd_field_copy_cb copy_func,
                   lsd_field_delete_cb delete_func) ALLOW_UNUSED;

// Finishes the initialisation of the lsd template (passed as the first parameter)
// a sample instance of the struct should be created (using malloc / calloc, etc)
// and passed as the second parameter. The sample should be freed afterwards using normal free.
// All subsequent instances must be created with lives_struct_create or lives_struct_copy.
// The function returns LSD_SUCCESS on success, LSD_ERROR if a parameter is invalid.
// this version should be used when sruct has a field with type (lives_struct_def_t)
static int lives_struct_init(const lives_struct_def_t *, void *thestruct,
                             lives_struct_def_t *) ALLOW_UNUSED;

// as above - this version should be used when lsd is of type (lives_struct_def_t *)
static int lives_struct_init_p(const lives_struct_def_t *, void *thestruct,
                               lives_struct_def_t **) ALLOW_UNUSED;

/// creates a new instance of struct. lsd can be one returned from create_lsd + lives_struct_init
// or it can be lsd from inside another struct (cast to const) - the fields will be set to
// the initial state rather than copied over
static void *lives_struct_create(const lives_struct_def_t *) ALLOW_UNUSED;

/// similar to the previous function, in this case the new struct will not be allocated or
// freed. This is intended to be used to "fill in" the fields for a static stuct
// e.g. if it is a non-pointer field inside another struct field in
static void *lives_struct_create_static(const lives_struct_def_t *template,
                                        void *static_mem) ALLOW_UNUSED;

// allocates and returns a copy of struct, calls copy_funcs, fills in lives_struct_def for copy
// lsd must be within a struct, not a static template
static void *lives_struct_copy(lives_struct_def_t *) ALLOW_UNUSED;

// just calls lives_struct_unref
static int lives_struct_free(lives_struct_def_t *) ALLOW_UNUSED;

// decrements refcount, then if <=0 frees struct. Returns refcount (so value <=0 means struct freed)
// returns -1 if parameter is NULL
static int lives_struct_unref(lives_struct_def_t *) ALLOW_UNUSED;

// increments refcount, returns new value. Returns 0 if parameter is NULL
static int lives_struct_ref(lives_struct_def_t *) ALLOW_UNUSED;

// Values returned from the get_* functions below refer to the actual fields in the lsd struct,
// and must not be freed or written to, except via the corresponding API functions

// returns current refcount, or 0 if NULL is passed
static int lives_struct_get_refcount(lives_struct_def_t *) ALLOW_UNUSED;

// set user data for an instance, reset for copies
static void lives_struct_set_user_data(lives_struct_def_t *, void *data) ALLOW_UNUSED;
static void *lives_struct_get_user_data(lives_struct_def_t *) ALLOW_UNUSED;

// returns generation number, 0 for a template, 1 for instance created via lives_struct_new,
// 2 for copy of instance, 3 for copy of copy, etc
static int lives_struct_get_generation(lives_struct_def_t *) ALLOW_UNUSED;

static uint64_t lives_struct_get_uid(lives_struct_def_t *) ALLOW_UNUSED;

static const char *lives_struct_get_type(lives_struct_def_t *) ALLOW_UNUSED;

static const char *lives_struct_get_last_field(lives_struct_def_t *) ALLOW_UNUSED;

static uint64_t lives_struct_get_identifier(lives_struct_def_t *) ALLOW_UNUSED;

static uint64_t lives_struct_get_end_id(lives_struct_def_t *) ALLOW_UNUSED;

static size_t lives_struct_get_size(lives_struct_def_t *) ALLOW_UNUSED;

/*
  // set init_callback for a struct or instance, passed on to copies
  // called after instance is made via lives_struct_new or lives_struct_copy
  // parent struct is NULL for lives_struct_new
  static void lives_struct_set_new_callback(lives_struct_def_t *, void *new_user_data) ALLOW_UNUSED; // TODO

  // set copied callback for a struct or instance, passed on to copies
  // called when copy is made of an instance
  static void lives_struct_set_copied_callback(lives_struct_def_t *, void *copied_user_data) ALLOW_UNUSED; // TODO

  // set destroy callback for a struct or instance, passed on to copies
  // called when instance is about to be destroyed
  static void lives_struct_set_destroy_callback(lives_struct_def_t *, void *destroy_user_data) ALLOW_UNUSED; // TODO
*/

////// FUNCTION BODIES //////

#ifndef LSD_UIDLEN
#define LSD_UIDLEN 8
#endif

#ifndef LSD_RANDFUNC
#ifdef HAVE_GETENTROPY
#define _LSD_IGN_RET(a) ((void)((a) + 1))
#define LSD_RANDFUNC(ptr, size) _LSD_IGN_RET(getentropy(ptr, size))
#else
error("\n\ngetentropy not found, so you need to #define LSD_RANDFUNC(ptr, size)\n"
      "Did you include 'AC_CHECK_FUNCS(getentropy)' in configure.ac ?");
#endif
#endif

static void _lsd_init_copy(void *, void *, const char *, const char *, void *) ALLOW_UNUSED;
static void _lsd_init_copy(void *dst, void *strct, const char *strct_type, const char *field_name,
                           void *ptr_to_field) {

  if (!dst) {
    /// called from init
    if (!(*_lsd_strcmp)(field_name, LSD_FIELD_IDENTIFIER)) {
      *(uint64_t *)ptr_to_field = LIVES_STRUCT_ID;
      return;
    }
    if (!(*_lsd_strcmp)(field_name, LSD_FIELD_END_ID)) {
      *(uint64_t *)ptr_to_field = (LIVES_STRUCT_ID ^ 0xFFFFFFFFFFFFFFFF);
      return;
    }
  }

  if (!(*_lsd_strcmp)(field_name, LSD_FIELD_TOP)) {
    if (dst) *(void **)ptr_to_field = dst;
    else *(void **)ptr_to_field = strct;
  } else if (!(*_lsd_strcmp)(field_name, LSD_FIELD_UNIQUE_ID)) {
    LSD_RANDFUNC(ptr_to_field, LSD_UIDLEN);
  } else if (!(*_lsd_strcmp)(field_name, LSD_FIELD_REFCOUNT)) {
    *((int *)ptr_to_field) = 1;
  } else if (!(*_lsd_strcmp)(field_name, LSD_FIELD_GENERATION)) {
    (*(int *)ptr_to_field)++;
  }
}

// builtin init_cb
static void _lsd_init_cb(void *, const char *, const char *, void *) ALLOW_UNUSED;
static void _lsd_init_cb(void *strct, const char *strct_type, const char *field_name,
                         void *ptr_to_field) {
  _lsd_init_copy(NULL, strct, strct_type, field_name, ptr_to_field);
}

// builtin copy cb
static void _lsd_copy_cb(void *, void *, const char *, const char *, void *, void *) ALLOW_UNUSED;
static void _lsd_copy_cb(void *dst, void *src, const char *strct_type, const char *field_name,
                         void *dst_fld_ptr, void *src_fld_ptr) {
  _lsd_init_copy(dst, src, strct_type, field_name, dst_fld_ptr);
}

// builtin delete_cb
static void _lsd_delete_cb(void *, const char *, const char *, void *) ALLOW_UNUSED;
static void _lsd_delete_cb(void *strct, const char *strct_type, const char *field_name,
                           void *ptr_to_field) {/* nothing */}

// other internal funcs
static int _lsd_generation_check_lt(lives_struct_def_t *lsd, int gen, int show_error) {
  /// check less than or error (check for static)
  if (lsd) {
    if (lsd->generation < gen) return LSD_SUCCESS;
    if (show_error)
      lsd_baderr_print("Function was called with an lsd-in-struct, but we wanted static lsd\n");
  }
  return LSD_FAIL;
}
static int _lsd_generation_check_gt(lives_struct_def_t *lsd, int gen, int show_error) {
  /// check greater than or error (check for lsd-in-struct)
  if (lsd) {
    if (lsd->generation > gen) return LSD_SUCCESS;
    if (show_error)
      lsd_baderr_print("Function was called with a static lsd, but we wanted lsd-in-struct\n");
  }
  return LSD_FAIL;
}

static lsd_special_field_t *
_lsd_make_special_field(uint64_t flags, void *top, void *ptr_to_field,
                        const char *name, size_t data_size,
                        lsd_field_init_cb init_func,
                        lsd_field_copy_cb copy_func,
                        lsd_field_delete_cb delete_func) {
  lsd_special_field_t *specf;

  if (!(*_lsd_calloc_aligned_)((void **)&specf, 1, sizeof(lsd_special_field_t))) {
    lsd_memerr_print(sizeof(lsd_special_field_t), name, "?????");
    return NULL;
  }
  specf->flags = flags;
  specf->offset_to_field = (off_t)((char *)ptr_to_field - (char *)top);
  if (name)(*_lsd_snprintf)(specf->name, LSD_FIELD_NAME_LEN, "%s", name);
  specf->byte_size = data_size;
  specf->init_func = init_func;
  specf->copy_func = copy_func;
  specf->delete_func = delete_func;
  return specf;
}

static void *_lsd_get_field(char *top, int is_self_field,
                            lsd_special_field_t **spfields, int i) {
  // fo init / copy top is new_struct or parent, for delete, top is lsd->top
  lsd_debug_print("calculating offset: for %s number %d, %s, top is %p, ",
                  is_self_field ? "self_field" : "special_field", i, spfields[i]->name, top);
  if (is_self_field) {
    top += spfields[LSD_FIELD_LSD_IDX]->offset_to_field;
    lsd_debug_print("lsd field is at offset %" PRIu64 ", %p\n",
                    spfields[LSD_FIELD_LSD_IDX]->offset_to_field, top);
    if (spfields[LSD_FIELD_LSD_IDX]->byte_size) {
      if (i == LSD_FIELD_LSD_IDX) return top;
      top = *((char **)top);
      lsd_debug_print("\nlsd is a pointer, real top is %p\n", top);
    }
  }
  lsd_debug_print("adding field offset of %" PRIu64 ", final ptr is %p\n",
                  spfields[i]->offset_to_field, top + spfields[i]->offset_to_field);
  return top + spfields[i]->offset_to_field;
}

static lives_struct_def_t *_lsd_get_lsd_in_struct(char *thestruct, const lives_struct_def_t *lsd) {
  // returns a pointer to the lsd field inside the struct
  // if the field is pointer then we dereference the address to get the real pointer
  lsd_special_field_t **spfields = lsd->self_fields;
  lives_struct_def_t *dst_lsd = _lsd_get_field(thestruct, 1, spfields, LSD_FIELD_LSD_IDX);
  if (spfields[LSD_FIELD_LSD_IDX]->byte_size) dst_lsd = *(lives_struct_def_t **)dst_lsd;
  return dst_lsd;
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
      } else {
        for (int j = 0; vptr[j]; j++) if (vptr[j])(*_lsd_free)(vptr[j]);
      }
    }
  }
  if (flags & LSD_FIELD_FLAG_FREE_ON_DELETE) {
    void *vptr = *((void **)ptr);
    if (vptr) {
      if ((flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) && !bsize
          && !(flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE)) {
        //g_print("flags !!! %" PRIu64 " %" PRIu64 "\n", flags, sizee);
        (*_lsd_string_free)(vptr);
      } else {
        //g_print("flags %" PRIu64 " %" PRIu64 "\n", flags, sizee);
        (*_lsd_free)(vptr);
      }
    }
  }
}

static void _lsd_auto_copy(void *dst_field, void *src_field, lsd_special_field_t *spcf,
                           lives_struct_def_t *lsd) GNU_HOT;
static void _lsd_auto_copy(void *dst_field, void *src_field, lsd_special_field_t *spcf,
                           lives_struct_def_t *lsd) {
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
          lsd_debug_print("allocating %" PRIu64 " bytes...", bsize);
          if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
            // alloc and zero
            if ((*_lsd_calloc_aligned_)((void **)dst_field, 1, bsize)) {
              lsd_debug_print("and set to zero.\n");
            } else {
              lsd_memerr_print(bsize, spcf->name, lsd->struct_type);
            }
            return;
          } else {
            // alloc and copy
            if (src_field != lsd && !(*((void **)src_field))) {
              lsd_debug_print("value is NULL, not copying\n");
            } else {
              if ((*_lsd_calloc_aligned_)((void **)dst_field, 1, bsize)) {
                (*_lsd_memcpy)(*(void **)dst_field, src_field, bsize);
                lsd_debug_print("and copying from src to dest.\n");
              } else {
                lsd_memerr_print(bsize, spcf->name, lsd->struct_type);
                return;
              }
		// *INDENT-OFF*
	      }}}
	  // *INDENT-ON*
      } else {
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
#ifdef SHOW_TEXT
            lsd_debug_print("%s\n", *cptr);
#endif
          } else {
            lsd_debug_print("value is NULL, not copying\n");
          }
        }
      }
      if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE)) {
        lsd_debug_print("WARNING: FREE_ON_DELETE not set\n");
      }
      if (spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE) {
        lsd_debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
      }
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
    } else {
      if (spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE) {
        lsd_debug_print("WARNING: FREE_ON_DELETE is set\n");
      }
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
            if (spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) {
              dptr[j] = (*_lsd_strdup)("");
            } else {
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
      } else {
        if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
          lsd_debug_print("created %d empty strings (+ terminating NULL)\n", count);
        } else {
          if (spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) {
            lsd_debug_print("duplicated %d strings (+ terminating NULL)\n", count);
          } else {
            lsd_debug_print("copy-by-ref %d strings (+ terminating NULL)\n", count);
          }
        }
      }
      if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE)) {
        lsd_debug_print("WARNING: FREE_ON_DELETE not set\n");
      }
      if (!(spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) &&
          !(spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY)) {
        if (spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE) {
          lsd_debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
        }
      } else {
        if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE)) {
          lsd_debug_print("WARNING: FREE_ALL_ON_DELETE not set\n");
        }
      }
      return;
    } else {
      /// array of elemnts of bsize
      if (spcf->flags & (LSD_FIELD_FLAG_ALLOC_AND_COPY
                         | LSD_FIELD_FLAG_ZERO_ON_COPY)) {
        /// alloc and copy elements of size bsize
        void **vptr = (*(void ** *)src_field), **dptr;
        if (vptr) {
          count = 0;
          while (vptr[count]) count++;
          if (!(*_lsd_calloc_aligned_)((void **)&dptr, count + 1, sizeof(void *))) {
            lsd_memerr_print((count + 1) * sizeof(void *), spcf->name, lsd->struct_type);
            return;
          }
          for (j = 0; j < count; j++) {
            // flags tells us what to with each element
            if (spcf->flags & LSD_FIELD_FLAG_ALLOC_AND_COPY) {
              if (!(*_lsd_calloc_aligned_)((void **)&dptr[j], 1, bsize)) {
                lsd_memerr_print(bsize, spcf->name, lsd->struct_type);
                return;
              } else {
                if (!(spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY)) {
                  (*_lsd_memcpy)(dptr[j], vptr[j], bsize);
                }
              }
            }
          }
          dptr[j] = NULL; /// final element must always be NULL
          (*(void **)dst_field) = dptr;
        }
        if (!vptr) {
          lsd_debug_print("value is NULL, not copying\n");
        } else {
          if (spcf->flags & LSD_FIELD_FLAG_ZERO_ON_COPY) {
            lsd_debug_print("created %d pointers to empty elements of size %" PRIu64 " "
                            "(+ terminating NULL)\n", count, bsize);
          } else {
            lsd_debug_print("duplicated %d pointers to elements of size %" PRIu64 " "
                            "(+ terminating NULL)\n",
                            count, bsize);
          }
        }
        if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ON_DELETE)) {
          lsd_debug_print("WARNING: FREE_ON_DELETE not set\n");
        }
        if (!(spcf->flags & LSD_FIELD_FLAG_FREE_ALL_ON_DELETE)) {
          lsd_debug_print("WARNING: FREE_ALL_ON_DELETE not set\n");
        }
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
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*
}

static void _lsd_struct_free(lives_struct_def_t *) ALLOW_UNUSED;
static void _lsd_struct_free(lives_struct_def_t *lsd) {
  lsd_special_field_t **spfields, *self_spcf = NULL;
  uint64_t lsd_flags = 0;
  size_t lsd_size = 0;
  void *src_field, *self_fields = NULL;
  void *thestruct = NULL;

  if (!lsd) return;

  if (_lsd_generation_check_lt(lsd, 1, 0) == LSD_SUCCESS) {
    thestruct = lsd;
    spfields = lsd->self_fields;
  } else {
    thestruct = lsd->top;
    if (lsd->destroy_struct_callback)
      (*lsd->destroy_struct_callback)(thestruct, lsd->struct_type, lsd->destroy_user_data);
    spfields = lsd->special_fields;
  }

  while (1) {
    if (spfields) {
      int nfields = (spfields == lsd->self_fields ? LSD_N_SELF_FIELDS : lsd->nspecial);
      for (int i = 0; i < nfields; i++) {
        lsd_special_field_t *spcf = spfields[i];
        if (!spcf) continue;
        src_field = _lsd_get_field(thestruct, thestruct == lsd, spfields, i);
        if (thestruct == lsd) {
          if (!i) {
            lsd_size = spcf->byte_size;
            lsd_flags = spcf->flags;
          }
          if (spcf->delete_func)
            (*spcf->delete_func)(lsd->top, SELF_STRUCT_TYPE, spcf->name, src_field);
        } else {
          if (spcf->delete_func)
            (*spcf->delete_func)(lsd->top, lsd->struct_type, spcf->name, src_field);
        }
      }

      for (int i = 0; i < nfields; i++) {
        lsd_special_field_t *spcf = spfields[i];
        uint64_t flags;
        if (!spcf) continue;
        flags = spcf->flags;
        src_field = _lsd_get_field(thestruct, spfields == lsd->self_fields, spfields, i);
        if (src_field == &lsd->self_fields) {
          /// self_fields must be done last, since it is still needed for now
          self_fields = src_field;
          self_spcf = spcf;
          continue;
        }

        if (!flags) continue;
        if (flags & LSD_FIELD_FLAG_IS_SUBSTRUCT) {
          /// some other kind of substruct, find its lsd and call again
          /// TODO
          continue;
        }
        _lsd_auto_delete(src_field, flags, spcf->byte_size);
      }
    }

    // after freeing the struct fields we free the lives_struct_def_t itself
    // this is done using the values contained in selfdata
    if (spfields == lsd->special_fields) spfields = lsd->self_fields;
    else break;
  }

  if (self_fields) _lsd_auto_delete(self_fields, self_spcf->flags, 1);

  if (lsd_flags) {
    src_field = (void *)lsd;
    _lsd_auto_delete(src_field, lsd_flags, lsd_size);
  }
  if (thestruct) {
    if (!(lsd_flags & LSD_PRIV_NO_DEL_STRUCT)) {
      (*_lsd_free)(thestruct);
    }
  } else (*_lsd_free)(lsd);
}

static void  _lsd_copy_fields(lives_struct_def_t *lsd, lsd_special_field_t **spfields,
                              void *new_struct, void *parent, int nfields) {
  for (int i = 0; i < nfields; i++) {
    lsd_special_field_t *spcf = spfields[i];
    char *dst_field, *src_field;

    if (!spcf || !spcf->flags) continue;

    if (i > 1) lsd_debug_print("field done\n\n");

    dst_field = _lsd_get_field(new_struct, spfields == lsd->self_fields, spfields, i);

    lsd_debug_print("handling field %s with flags 0X%016lX\n", spcf->name, spcf->flags);

    if (spcf->flags & LSD_FIELD_FLAG_IS_SUBSTRUCT) {
      // otherwise we descend into the substruct and locate its lives_struct_def *
      // TODO...
      lsd_debug_print("field is another substruct {TODO}\n");
      continue;
    }

    if (!parent) {
      off_t offset = spfields[LSD_FIELD_LSD_IDX]->offset_to_field;
      size_t byte_size = spfields[LSD_FIELD_LSD_IDX]->byte_size;

      //when creating a new struct, we update src fields temporarily
      //offset_to_field for lsd is only valid for the dest struct, for src it is 0
      //also src is a pointer, not pointer to pointer, so set src byte_size to zero
      spfields[LSD_FIELD_LSD_IDX]->offset_to_field = 0;
      spfields[LSD_FIELD_LSD_IDX]->byte_size = 0;
      src_field = _lsd_get_field((char *)lsd, spfields == lsd->self_fields, spfields, i);
      spfields[LSD_FIELD_LSD_IDX]->offset_to_field = offset;
      spfields[LSD_FIELD_LSD_IDX]->byte_size = byte_size;
    } else {
      src_field = _lsd_get_field(parent, spfields == lsd->self_fields, spfields, i);
      if (!i && spfields == lsd->self_fields && lsd->self_fields[LSD_FIELD_LSD_IDX]->byte_size) src_field = *(char **)src_field;
    }
    _lsd_auto_copy(dst_field, src_field, spcf, lsd);
  }
}

static void _lsd_run_callbacks(lives_struct_def_t *lsd, lsd_special_field_t **spfields,
                               void *new_struct, int is_copy, int nfields) {
  for (int i = 0; i < nfields; i++) {
    lsd_special_field_t *spcf = spfields[i];
    char *dst_field;
    if (!spcf) continue;

    dst_field = _lsd_get_field(new_struct, spfields == lsd->self_fields, spfields, i);
    if (i == LSD_FIELD_LSD_IDX && spfields == lsd->self_fields &&
        lsd->self_fields[i]->byte_size) dst_field = *(char **)dst_field;

    if (!is_copy || (spcf->flags & LSD_FIELD_FLAG_CALL_INIT_FUNC_ON_COPY)) {
      if (spcf->init_func) {
        lsd_debug_print("calling init_func for %s\n", spcf->name);
        (*spcf->init_func)(new_struct, (spfields == lsd->special_fields)
                           ? lsd->struct_type : SELF_STRUCT_TYPE, spcf->name, dst_field);
      }
    }
    if (is_copy) {
      if (spcf->copy_func) {
        char *src_field = (char *)lsd->top + spcf->offset_to_field;
        lsd_debug_print("calling copy_func for %s\n", spcf->name);
        (*spcf->copy_func)(new_struct, lsd->top, (spfields == lsd->special_fields)
                           ? lsd->struct_type : SELF_STRUCT_TYPE, spcf->name, dst_field, src_field);
      }
    }
  }
}

static void *_lsd_struct_copy(lives_struct_def_t *lsd, void *new_struct, int is_copy) {
  lsd_special_field_t **spfields;
  lives_struct_def_t *dst_lsd;
  void *parent = NULL;

  if (!lsd->struct_size) {
    lsd_baderr_print("Cannot create or copy a struct type %s with size 0", lsd->struct_type);
    return NULL;
  }

  spfields = lsd->self_fields;

  if (is_copy) {
    // copy
    if (!(*_lsd_calloc_aligned_)((void **)&new_struct, 1, lsd->struct_size)) {
      lsd_memerr_print(lsd->struct_size, "ALL FIELDS", lsd->struct_type);
      return NULL;
    }

    lsd_debug_print("copying struct of type: %s, %p -> %p, with size %" PRIu64 "\n",
                    lsd->struct_type,
                    parent, new_struct, lsd->struct_size);
    parent = lsd->top; // pointer to struct containing the lsd template
    (*_lsd_memcpy)(new_struct, parent, lsd->struct_size);
  } else {
    lsd_debug_print("initing struct %p of type: %s\n", new_struct, lsd->struct_type);
  }

  // copy self_fields first:
  lsd_debug_print("copying lives_struct_def_t fields first\n");

  if (!is_copy) {
    _lsd_copy_fields(lsd, spfields, new_struct, parent, LSD_N_SELF_FIELDS);
    _lsd_run_callbacks(lsd, spfields, new_struct, 0, LSD_N_SELF_FIELDS);
    lsd_debug_print("all self fields in struct copied\n\n");
  }

  // after copying structdef, we copy the normal fields
  spfields = lsd->special_fields;
  if (spfields) {
    if (!is_copy) {
      lsd_debug_print("initing normal fields\n");
      _lsd_run_callbacks(lsd, spfields, new_struct, 0, lsd->nspecial);
    } else {
      lsd_debug_print("copying normal fields\n");
      _lsd_copy_fields(lsd, spfields, new_struct, parent, lsd->nspecial);
      _lsd_run_callbacks(lsd, spfields, new_struct, 1, lsd->nspecial);
    }
  }

  if (is_copy) lsd_debug_print("struct copy done\n\n");
  else lsd_debug_print("struct init done\n\n");

  lsd_debug_print("triggering any struct callbacks\n\n");

  if (is_copy) {
    if (lsd->copied_struct_callback)
      (*lsd->copied_struct_callback)(parent, new_struct, lsd->struct_type, lsd->copied_user_data);
  }

  dst_lsd = _lsd_get_field(new_struct, 1, lsd->self_fields, LSD_FIELD_LSD_IDX);
  spfields = lsd->self_fields;
  if (spfields[LSD_FIELD_LSD_IDX]->byte_size) dst_lsd = *(lives_struct_def_t **)dst_lsd;
  if (dst_lsd->new_struct_callback)
    (*dst_lsd->new_struct_callback)(new_struct, parent, lsd->struct_type, dst_lsd->new_user_data);

  return new_struct;
}

static int _lsd_struct_init(const lives_struct_def_t *lsd, void *thestruct,
                            lives_struct_def_t **lsd_in_structp, int is_ptr) {
  // all we do here is to find the offset of the lsd field inside struct
  lives_struct_def_t *lsd_in_struct;
  if (!lsd || !thestruct || !lsd_in_structp) return LSD_ERROR;
  if (!is_ptr && !(lsd_in_struct = (lives_struct_def_t *)*lsd_in_structp)) return LSD_ERROR;
  if (_lsd_generation_check_lt((lives_struct_def_t *)lsd, 1, 0) != LSD_SUCCESS) return LSD_ERROR;
  else {
    lsd_special_field_t **spfields = lsd->self_fields;
    if (is_ptr)
      spfields[LSD_FIELD_LSD_IDX] = _lsd_make_special_field(LSD_FIELD_FLAG_ALLOC_AND_COPY, thestruct,
                                    lsd_in_structp, LSD_FIELD_LSD,
                                    sizeof(lives_struct_def_t), NULL, NULL, NULL);
    else
      spfields[LSD_FIELD_LSD_IDX] = _lsd_make_special_field(0, thestruct, lsd_in_struct, LSD_FIELD_LSD, 0, NULL, NULL, NULL);
  }
  return LSD_SUCCESS;
}

static void _lsd_lsd_free(lives_struct_def_t *lsd) {
  _lsd_struct_free(lsd);
}

//// API FUNCTIONS /////////

static int lives_struct_get_generation(lives_struct_def_t *lsd) {
  return !lsd ? -1 : lsd->generation;
}
static uint64_t lives_struct_get_uid(lives_struct_def_t *lsd) {
  return !lsd ? 0 : lsd->unique_id;
}
static const char *lives_struct_get_type(lives_struct_def_t *lsd) {
  return !lsd ? NULL : lsd->struct_type;
}
static const char *lives_struct_get_last_field(lives_struct_def_t *lsd) {
  return !lsd ? NULL : lsd->last_field;
}
static uint64_t lives_struct_get_identifier(lives_struct_def_t *lsd) {
  return !lsd ? 0ul : lsd->identifier;
}
static uint64_t lives_struct_get_end_id(lives_struct_def_t *lsd) {
  return !lsd ? 0ul : lsd->end_id;
}
static void *lives_struct_get_user_data(lives_struct_def_t *lsd) {
  return !lsd ? NULL : lsd->user_data;
}
static size_t lives_struct_get_size(lives_struct_def_t *lsd) {
  return !lsd ? 0 : lsd->struct_size;
}
static void lives_struct_set_user_data(lives_struct_def_t *lsd, void *data) {
  if (lsd) lsd->user_data = data;
}
static const char *lives_struct_get_class_id(lives_struct_def_t *lsd) {
  return !lsd ? NULL : lsd->class_id;
}
static void lives_struct_set_class_id(lives_struct_def_t *lsd, const char *class_id) {
  if (lsd)(*_lsd_snprintf)(lsd->class_id, LSD_CLASS_ID_LEN, "%s", class_id);
}

static lsd_special_field_t *make_special_field(uint64_t flags, void *thestruct,
    void *ptr_to_field,
    const char *name,
    size_t data_size,
    lsd_field_init_cb init_func,
    lsd_field_copy_cb copy_func,
    lsd_field_delete_cb delete_func) {
  return _lsd_make_special_field(flags, thestruct, ptr_to_field, name, data_size,
                                 init_func, copy_func, delete_func);
}

static int lives_struct_ref(lives_struct_def_t *lsd) {
  return lsd ? ++((lives_struct_def_t *)lsd)->refcount : 0;
}

static int lives_struct_unref(lives_struct_def_t *lsd) {
  if (lsd) {
    if (!lsd->top) {
      lsd_baderr_print("Unable to free struct of type %s, lives_struct_init must be called first\n",
                       lsd->struct_type);
      return -1;;
    }
    int rc = --(((lives_struct_def_t *)lsd)->refcount);
    if (rc <= 0) {
      if (_lsd_generation_check_lt((lives_struct_def_t *)lsd, 1, 0) == LSD_SUCCESS)
        _lsd_lsd_free((lives_struct_def_t *)lsd);
      else _lsd_struct_free((lives_struct_def_t *)lsd);
      return rc;
    }
  }
  return 0;
}

static int lives_struct_get_refcount(lives_struct_def_t *lsd) {
  return lsd->refcount;
}

static int lives_struct_free(lives_struct_def_t *lsd) {
  return lives_struct_unref(lsd);
}

static void *lives_struct_copy(lives_struct_def_t *lsd) {
  // alloc and replicate
  if (_lsd_generation_check_gt(lsd, 0, 1) != LSD_SUCCESS) return NULL;
  return _lsd_struct_copy(lsd, NULL, 1);
}

static int lives_struct_init_p(const lives_struct_def_t *lsd, void *thestruct,
                               lives_struct_def_t **lsd_in_struct) {
  /// the difference between this function call and the following is that here we allocate
  // copy and free lsd, and additionally, special_field offsets are measured from the
  // dereferenced field rather than the field itself
  // internally this is signalled by setting the byte_size for lsd->special_fields[0].byte_size
  // to sizeof(lives_struct_def_t *) instead of 0...
  return _lsd_struct_init(lsd, thestruct, lsd_in_struct, 1);
}

static int lives_struct_init(const lives_struct_def_t *lsd, void *thestruct,
                             lives_struct_def_t *lsd_in_struct) {
  // in other cases a byte_size of zero indicates a string, but in this case it is not relevant
  // since flags is also zero. In the unforeseen case that flags ever needs setting, then care
  // needs to be taken in callbacks to use memcpy rather than strdup, etc.
  return _lsd_struct_init(lsd, thestruct, &lsd_in_struct, 0);
}

static void *__lives_struct_create(const lives_struct_def_t *lsd, void *inplace) {
  lives_struct_def_t *lsd_in_struct = NULL;
  lsd_special_field_t **spfields;
  void *thestruct = inplace;

  if (!lsd) return NULL;
  spfields = lsd->self_fields;
  if (!spfields) return NULL;
  if (!spfields[LSD_FIELD_LSD_IDX]) {
    lsd_baderr_print("Unable to create struct of type %s, "
                     "lives_struct_init or lives_struct_init_p must be called "
                     "first\n", lsd->struct_type);
    return NULL;
  }

  if (inplace) {
    lsd_in_struct = _lsd_get_lsd_in_struct(thestruct, lsd);
    if (lsd_in_struct) {
      spfields = lsd_in_struct->self_fields;
      if (inplace == lsd || (spfields && (*(uint64_t *)(spfields[LSD_FIELD_IDENTIFIER_IDX])
                                          == LIVES_STRUCT_ID && (*(uint64_t *)(spfields[LSD_FIELD_END_ID_IDX]))
                                          == (LIVES_STRUCT_ID ^ 0xFFFFFFFFFFFFFFFF)))) {
        lsd_baderr_print("Refusing to init a struct of type %s, "
                         "target appears to be already initialised.\n",
                         lsd->struct_type);
        return NULL;
      }
    }
  }

  if (!thestruct) {
    if (!(*_lsd_calloc_aligned_)((void **)&thestruct, 1, lsd->struct_size)) {
      lsd_memerr_print(lsd->struct_size, "ALL", lsd->struct_type);
      return NULL;
    }
  }
  _lsd_struct_copy((lives_struct_def_t *)lsd, thestruct, 0);

  if (inplace) {
    lsd_in_struct = _lsd_get_lsd_in_struct(thestruct, lsd);
    lsd_special_field_t *lsd_in_lsd_in_struct = lsd_in_struct->self_fields[LSD_FIELD_LSD_IDX];
    lsd_in_lsd_in_struct->flags |= LSD_PRIV_NO_DEL_STRUCT;
  }
  return thestruct;
}

static void *lives_struct_create(const lives_struct_def_t *lsd) {
  // standard create, will produce a new struct of the specified type
  return __lives_struct_create(lsd, NULL);
}

static void *lives_struct_create_static(const lives_struct_def_t *lsd,
                                        void *struct_ptr) {
  // with this version one can create the struct in static memory. for example embedded in
  // another struct, just pass in the address of the allocated struct
  // the the struct will be intialized inplace, on deletion the fields will be deleted as
  // appropriate, but not the stricy itself
  return __lives_struct_create(lsd, struct_ptr);
}

static int lsd_ref(const lives_struct_def_t *lsd) {
  return lives_struct_ref((lives_struct_def_t *)lsd);
}
static int lsd_unref(const lives_struct_def_t *lsd) {
  return lives_struct_unref((lives_struct_def_t *)lsd);
}
static int lsd_get_refcount(const lives_struct_def_t *lsd) {
  return lives_struct_get_refcount((lives_struct_def_t *)lsd);
}
static int lsd_free(const lives_struct_def_t *lsd) {
  return lsd_unref(lsd);
}

static int lsd_same_family(const lives_struct_def_t *lsd1, lives_struct_def_t *lsd2) {
  // must have same identifier, end_id, struct_type, struct_size, last_field, class_id
  if (lsd1->struct_size == lsd2->struct_size
      && lsd1->identifier == lsd2->identifier
      && lsd1->end_id == lsd2->end_id
      && (!(*_lsd_strcmp)(lsd1->struct_type, lsd2->struct_type))
      && (!(*_lsd_strcmp)(lsd1->class_id, lsd2->class_id))
      && (!(*_lsd_strcmp)(lsd1->last_field, lsd2->last_field))) return LSD_SUCCESS;
  return LSD_FAIL;
}


static const lives_struct_def_t *lsd_create(const char *struct_type, size_t struct_size,
    const char *last_field, int nspecial) {
  lsd_special_field_t **xspecf;
  lives_struct_def_t *lsd;
  size_t xsize;

  if (!(*_lsd_calloc_aligned_)((void **)&lsd, 1, sizeof(lives_struct_def_t))) {
    lsd_memerr_print(sizeof(lives_struct_def_t), "LSD template", struct_type);
    return NULL;
  }

  if (struct_type)(*_lsd_snprintf)(lsd->struct_type, LSD_STRUCT_TYPE_LEN, "%s", struct_type);

  lsd->struct_size = struct_size;
  lsd->refcount = 1;
  lsd->nspecial = nspecial;

  if (last_field)(*_lsd_snprintf)(lsd->last_field, LSD_FIELD_NAME_LEN, "%s", last_field);

  if (nspecial > 0) {
    if (!(*_lsd_calloc_aligned_)((void **)&lsd->special_fields, nspecial + 1,
                                 sizeof(lsd_special_field_t *))) {
      lsd_memerr_print((nspecial + 1) * sizeof(lsd_special_field_t *),
                       LSD_FIELD_LSD "." LSD_FIELD_SPECIAL_FIELDS, SELF_STRUCT_TYPE);
      return NULL;
    }
    lsd->special_fields[nspecial] = NULL;
  }

  xsize = LSD_N_SELF_FIELDS * sizeof(lsd_special_field_t) + sizeof(lsd_special_field_t *);
  if (!(*_lsd_calloc_aligned_)((void **) & (lsd->self_fields), 1, xsize)) {
    lsd_memerr_print(xsize, LSD_FIELD_LSD "." LSD_FIELD_SELF_FIELDS, SELF_STRUCT_TYPE);
    if (nspecial)(*_lsd_free)(lsd->special_fields);
    (*_lsd_free)(lsd);
    return NULL;
  }
  xspecf = lsd->self_fields;

  // xspecf[0] ("lsd") stays as NULL for now, this tells us that the struct has not been inited yet.

  // set on init
  xspecf[1] = _lsd_make_special_field(0, lsd, &lsd->identifier, LSD_FIELD_IDENTIFIER, 0, _lsd_init_cb,
                                      NULL, NULL);
  // set a new random value on init / copy
  xspecf[2] = _lsd_make_special_field(0, lsd, &lsd->unique_id, LSD_FIELD_UNIQUE_ID, 0,
                                      _lsd_init_cb, _lsd_copy_cb, NULL);
  // et to 1 on init / copy
  xspecf[3] = _lsd_make_special_field(0, lsd, &lsd->refcount, LSD_FIELD_REFCOUNT, 0,
                                      _lsd_init_cb, _lsd_copy_cb, NULL);
  // set to 1 on init, increment on copy
  xspecf[4] = _lsd_make_special_field(0, lsd, &lsd->generation, LSD_FIELD_GENERATION, 0,
                                      _lsd_init_cb, _lsd_copy_cb, NULL);
  // point to struct on init / copy
  xspecf[5] = _lsd_make_special_field(0, lsd, &lsd->top, LSD_FIELD_TOP, 0, _lsd_init_cb, _lsd_copy_cb, NULL);

  // values will be alloced and copied to a copy struct,
  xspecf[6] = _lsd_make_special_field(LSD_FIELD_PTR_ARRAY, lsd,
                                      &lsd->special_fields, LSD_FIELD_SPECIAL_FIELDS,
                                      sizeof(lsd_special_field_t), NULL, NULL, NULL);
  // NULL terminated array - array allocated / freed elements copied
  xspecf[7] = _lsd_make_special_field(LSD_FIELD_PTR_ARRAY, lsd, &lsd->self_fields, LSD_FIELD_SELF_FIELDS,
                                      sizeof(lsd_special_field_t), NULL, NULL, NULL);
  // value will be set to zero after copying
  xspecf[8] = _lsd_make_special_field(LSD_FIELD_FLAG_ZERO_ON_COPY, lsd,
                                      &lsd->user_data, LSD_FIELD_USER_DATA, 8, NULL, NULL, NULL);
  // set to val
  xspecf[9] = _lsd_make_special_field(0, lsd, &lsd->end_id, LSD_FIELD_END_ID, 0, _lsd_init_cb,
                                      NULL, NULL);

  return lsd;
}

/////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
#undef DEBUG
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
