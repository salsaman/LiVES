// LiVES - decoder plugin header
// (c) G. Finch 2008 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// lsd.h :: implemntation of LiVES Struct Definition (LSD)
// functions for auto copy and auto free of structs

#ifndef __STRUCTDEFS_H__
#define __STRUCTDEFS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define DEBUG

#ifdef DEBUG
#include <stdio.h>
#define debug_print(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug_print(...)
#endif

#ifndef SILENT_ENOMEM
#include <stdio.h>
#define memerr_print(size, name, struct) fprintf(stderr, "WARNING: memory failure allocating " \
						 "%lu bytes for field %s in struct %s", \
						 size, name, struct)
#else
#define memerr_print(a, b, c)
#endif

#ifndef SILENT_FAILURES
#include <stdio.h>
#define baderr_print(...) fprintf(stderr, __VA_ARGS__)
#else
#define baderr_print(...)
#endif

#if defined _GNU_SOURCE
#define ALLOW_UNUSED __attribute__((unused))
#else
#define ALLOW_UNUSED
#endif

#define LSD_TEXTLEN 64
#define LSD_NAMELEN 16
#define MAX_ALLOC 100000000ul
#define LIVES_STRUCT_ID 0x4C7C56332D2D3035  /// 1st 8 bytes - L|V3--05 (be) or 50--3V|L (le)

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifndef OVERRIDE_MEMFUNCS
static void *(*_lsd_calloc)(size_t nmemb, size_t size) = calloc;
static void *(*_lsd_memcpy)(void *dest, const void *src, size_t n) = memcpy;
static void *(*_lsd_memset)(void *s, int c, size_t n) = memset;
static void (*_lsd_free)(void *ptr) = free;
#endif

#ifndef OVERRIDE_STRFUNCS
static int(*_lsd_strcmp)(const char *s1, const char *s2) = strcmp;
static char *(*_lsd_strdup)(const char *s) = strdup;
static void (*_lsd_string_free)(void *ptr) = free;
#endif

#ifdef USE_POSIX_MEMALIGN
#ifndef _MEM_ALIGNMENT_
#define _MEM_ALIGNMENT_ 64 // or whatever power of 2
#endif
static int _lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size) {
  int ret = posix_memalign(memptr, _MEM_ALIGNMENT_, nmemb * size);
  if (!ret && *memptr)(*_lsd_memset)(*memptr, 0, nmemb * size);
  return ret;
}
#else
static int _lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size) {
  return !memptr ? 0 : (!(*memptr = (*_lsd_calloc)(nmemb, size))) ? ENOMEM : 0;
}
#define _MEM_ALIGNMENT_ 0 // irrelevant
#endif

static int (*_lsd_calloc_aligned)(void **memptr, size_t nmemb, size_t size) =
  _lsd_calloc_aligned_;

static char *_lsd_proxy_strdup(char *str) ALLOW_UNUSED;
static char *_lsd_proxy_strdup(char *str) {
  char *ret;
  int i = 0;
  while (str[i++]);
  (*_lsd_calloc_aligned)((void **)&ret, 1, i);
  (*_lsd_memcpy)(ret, str, i);
  return ret;
}

/// AUTONATION FLAGS

/// copy flags
///< alloc and copy on copy. If bytesize is set that will be the alocated size,
/// if 0 then we do a strdup. Fixed size is ignored for arrays.
#define LIVES_FIELD_FLAG_ALLOC_AND_COPY (1l << 0)

///< if bytesize is 0, field will be set to NULL in lives_struct_copy,
/// if ALLOC_AND_COPY is also set, will be set to empty string
/// if bytesize > 0 and not IS_NULLT_ARRAY
/// then field will will be filled with bytesize zeros
/// dest->field = NULL or memset(dets->field, 0, bytesize)
/// for ARRAYS, the process will be appied to each element in turn
/// however, since a NULL element marks the end of a NULLT_ARRAY,
/// the combination ALLOC_AND_COPY | ZERO_ON_COPY | IS_NULLT_ARRAY may interfere with
/// subsequent copying
#define LIVES_FIELD_FLAG_ZERO_ON_COPY (1l << 1)

// delete flags
///< field wiill be freed in lives_struct_delete
/// free(struct->field)
#define LIVES_FIELD_FLAG_FREE_ON_DELETE (1l << 16)

/// for (i = 0; struct->field[i], i++) free(struct->field[i];
#define LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE (1l << 17) ///< combined with IS_NULLT_ARRAY, frees all elements, combine with FREE_ON_DELETE to free elemnt after

/// flags giving extra info about the field (affects copy and delete)

///< field is a substruct with its own lives_struct_def_t; functions should be called recursively
/// it must be possible to locate the lives_struct_def_t field from the first byte
/// sequence matching LIVES_STRUCT_ID in its identifier field
/// lives_struct_copy(struct->field
/// if this is set, all other flag bits are ignored for the field
#define LIVES_FIELD_FLAG_IS_SUBSTRUCT (1l << 32)

///< field is an array of elements of size bytelen, last element has all bytes set to zero
/// if bytesize is zero, it is an array of NUL terminated char
/// may be combined with ALLOC_AND_COPY, FREE_ON_DELETE, FREE_ALL_ON_DELETE
#define LIVES_FIELD_FLAG_IS_NULLT_ARRAY (1l << 33)

// combinations:
// Z : bytesize == 0 :: set any * to NULL
// Z : bytesize > 0 :: memset(bytesize, 0)

// A&C : bytesize == 0 :: strdup
// A&C : bytesize > 0 :: malloc(bytesize), memcpy(bytesize)
// FREE_ON_DELETE recommended (LIVES_FIELD_IS_CHARPTR, LIVES_FIELD_IS_BLOB)

// A&C | Z : bytesize == 0 :: strdup("")  (LIVES_FIELD_TO_EMPTY_STRING)
// A&C | Z : bytesize > 0 :: malloc(bytesize), memset(bytesize, 0)
// FREE_ON_DELETE recommended

// NULLT : bytesize == 0 :: copy string array (strings still point to original strings)
// NULLT : bytesize > 0 :: copy array of elements of size bytesize
// FREE_ON_DELETE recommended (LIVES_FIELD_IS_ARRAY)
// setting FREE_ALL_ON_DELETE may be dangerous, as it would free the original values !

// NULLT + Z : interpreted as NULLT + ACC + Z,
// 		otherwise it would imply copying by reference, then ovewriting memory with 0

// NULLT + A&C : bytesize == 0 :: strdup, multiple strings
// NULLT + A&C : bytesize > 0 :: copy array of pointers to elements of size bytesize
// FREE_ON_DELETE | FREE_ALL_ON_DELETE recommended (LIVES_FIELD_IS_

// NULLT + A&C + Z : bytesize == 0 :: creates an equivalent number of empty strings
// NULLT + A&C + Z : bytesize > 0 :: malloc(bytesize), memset(bytesize, 0), multiple elements
// FREE_ON_DELETE | FREE_ALL_ON_DELETE recommended

#define LIVES_FIELD_CHARPTR (LIVES_FIELD_FLAG_ALLOC_AND_COPY | LIVES_FIELD_FLAG_FREE_ON_DELETE)
#define LIVES_FIELD_BLOB LIVES_FIELD_CHARPTR // with bytesize > 0

#define LIVES_FIELD_ARRAY (LIVES_FIELD_FLAG_IS_NULLT_ARRAY | LIVES_FIELD_FLAG_FREE_ON_DELETE)

#define LIVES_FIELD_PTR_ARRAY (LIVES_FIELD_ARRAY | LIVES_FIELD_FLAG_ALLOC_AND_COPY \
			       | LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE)

// with a bytesize of zero this will cause a string to be set to "" on copy
// without the ALLOC_AND_COPY, the string would be set to NULL
#define LIVES_FIELD_TO_EMPTY_STRING (LIVES_FIELD_FLAG_ALLOC_AND_COPY | LIVES_FIELD_FLAG_ZERO_ON_COPY)

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
typedef void (*lives_field_init_cb)(void *strct, const char *struct_type,
                                    const char *field_name, void *ptr_to_field);

// this is called from lives_struct_copy after all automatic updates are performed
typedef void (*lives_field_copy_cb)(void *dst_struct, void *src_struct, const char *strct_type,
                                    const char *field_name, void *ptr_to_dst_field,
                                    void *ptr_to_src_field);
// e,g, using field names:
// strct_type *d = (strct_type *)dst_struct;
// strct_type *s = (strct_type *)src_struct;
/// d->*field_name = s->(field_name) + 10;
/// or using anonymous fields:
/// *(int *)dst_field = *(int *)src_field + 10;

//called from lives_struct_free before any fields are finalised
typedef void (*lives_field_delete_cb)(void *strct, const char *struct_type,
                                      const char *field_name, void *ptr_to_field);
// e,g
// strct_type *mystruct = (strct_type *)strct;
// free(mystryct->(field_name));

// STRUCTS

typedef struct _lives_special_field {
  /// flags may be  0 to optionally provide info re. the field name, bytesize,
  //(and optionally offset_to_field)
  uint64_t flags;
  /// must be set when creating the struct
  off_t offset_to_field;
  char name[LSD_NAMELEN]; /// optional unless flags == 0 or any of the functions below are defined.
  size_t bytesize; /// defines the elemnt size for
  lives_field_init_cb init_func; ///< will be called from lives_struct_new
  lives_field_copy_cb copy_func; ///< will be called from lives_struct_copy
  lives_field_delete_cb delete_func; ///< called from lives_struct_free
} lives_special_field_t; /// 112 bytes

typedef struct _lives_struct_def {
  uint64_t identifier;  /// default: LIVES_STRUCT_ID
  uint64_t unique_id; /// randomly generted id, unique to each instance

  int32_t refcount; ///< refcount, set to 1 on creation, free unrefs and only frees when 0.
  uint32_t generation; ///< initialized as 1 and incremented on each copy

  void *top; ///< ptr to the start of parent struct itself, typecast to a void *

  char structtype[LSD_TEXTLEN]; /// type of the struct as string, e.g "lives_struct_def_t"
  size_t structsize; ///< bytesize of parent struct (sizef(struct))
  char last_field[LSD_TEXTLEN]; ///< name of last field of struct (informational only)

  lives_struct_new_cb new_struct_callback;  ///< called from lives_struct_new
  void *new_user_data; /// user_data for new_struct_callback

  lives_struct_copied_cb copied_struct_callback;  ///< called from lives_struct_copy
  void *copied_user_data; /// user_data for clone_struct_callback

  lives_struct_destroy_cb destroy_struct_callback; /// called from lives_struct_free if refcount is 0
  void *destroy_user_data; /// user_data for delete_struct_callback

  lives_special_field_t **special_fields;  /// may be NULL, else is pointer to NULL terminated array
  lives_special_field_t **self_fields;  /// fields in the struct_def_t struct itself

  void *class_data; /// user_data, value maintained across clones
  void *user_data; /// user_data for instances of struct, reset on copy
  uint64_t end_id;  /// end marker. == identifier ^ 0xFFFFFFFFFFFFFFFF
} lives_struct_def_t; /// 256 bytes

// it is also possible to create a static struct_def, in which case the following is true
// unique_id is 0, top is NULL, refcount is 0, generation is 0,
// a static version may be copied to struct_def for a struct, this is like a normal copy

#define SELF_STRUCT_TYPE "lives_struct_def_t"

////// FUNCTION BODIES //////

#ifndef IGN_RET
// for getentropy
#define IGN_RET(a) ((void)((a) + 1))
#endif

#ifdef DO_ALIGN_CHECK
// optional check to ensure that src_offset == dst_offset for the same field
// this is done by sampling and comparing both memory areas at various offsets
// (It should be the case anyway, since the compiler needs to know where the fields are
// within the struct)
// Also: both structs are created by alloc / memcpy, without any refence to the struct type
// thus casting either one to struct_type * should produce identical results
// Since all offsets are relative to the start of the struct / substruct
// the alignment of the start itself is not relevant
// Finally, we sample at odd bytesizes, and cast to uint64_t *. If there are any problems
// caused by realignemnt due to recasting from char * -> void * -> uint64_t *
// then they should show up here.
//
// in case of doubt, USE_POSIX_MEMALIGN can be defined, and _MEM_ALIGNMENT_ set to a power of two
// in which case posix_memalign will be used to allocate all memory
//
#define  _OFFS_CHKVAL 37
static void _lsd_memalign_check(void *src, void *dst, size_t structsize);
static void _lsd_memalign_check(void *src, void *dst, size_t structsize) {
  uint64_t *u0, *u1;
  void *field;
  off_t offset = 0;
  structsize -= 8;
  while (offset < structsize) {
    field = (void *)((char *)src + offset);
    u0 = (uint64_t *)field;
    ield = (void *)((char *)dst + offset);
    u1 = (uint64_t *)field;
    if (*u0 != *u1) {
      debug_print("ALIGNMENT CHECK FAILED, ABORTING !");
      abort();
    }
    offset += _OFFS_CHKVAL;
  }
  debug_print("lives_struct_copy: memory alignment checks passed, OK to proceed\n");
}
#endif

static void _lsd_init_copy(void *, void *, const char *, const char *, void *) ALLOW_UNUSED;
static void _lsd_init_copy(void *dst, void *strct, const char *strct_type, const char *field_name,
                           void *ptr_to_field) {
  if (!dst) {
    /// called from init
    if (!strcmp(field_name, "identifier")) {
      *(uint64_t *)ptr_to_field = LIVES_STRUCT_ID;
      return;
    } else if (!strcmp(field_name, "end_id")) {
      *(uint64_t *)ptr_to_field =
        (LIVES_STRUCT_ID ^ 0xFFFFFFFFFFFFFFFF);
      return;
    }
  }
  if (!strcmp(field_name, "unique_id")) {
    IGN_RET(getentropy(ptr_to_field, 8));
  } else if (!strcmp(field_name, "top")) {
    *(void **)ptr_to_field = strct;
  } else if (!strcmp(field_name, "refcount")) {
    *((int *)ptr_to_field) = 1;
  } else if (!strcmp(field_name, "generation")) {
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
                           void *ptr_to_field) {
  // nothing
}

// other internal funcs
static int _lsd_generation_check_lt(lives_struct_def_t *lsd, int gen, int show_error) {
  /// check less than or error (check for static)
  if (lsd) {
    if (lsd->generation < gen) return 1;
    if (show_error)
      baderr_print("Function was called with an lsd-in-struct, but we wanted static lsd\n");
  }
  return 0;
}
static int _lsd_generation_check_gt(lives_struct_def_t *lsd, int gen, int show_error) {
  /// check greater than or error (check for lsd-in-struct)
  if (lsd) {
    if (lsd->generation > gen) return 1;
    if (show_error)
      baderr_print("Function was called with a static lsd, but we wanted lsd-in-struct\n");
  }
  return 0;
}

static lives_special_field_t *_lsd_make_special_field(uint64_t flags, void *top,
    void *ptr_to_field,
    const char *name,
    size_t data_size,
    lives_field_init_cb init_func,
    lives_field_copy_cb copy_func,
    lives_field_delete_cb delete_func) {

  lives_special_field_t *specf;

  if ((*_lsd_calloc_aligned)((void **)&specf, 1, sizeof(lives_special_field_t))) {
    memerr_print(sizeof(lives_special_field_t), name, "?????");
    return NULL;
  }
  specf->flags = flags;
  specf->offset_to_field = (off_t)((char *)ptr_to_field - (char *)top);
  if (name)
    snprintf(specf->name, LSD_NAMELEN, "%s", name);
  specf->bytesize = data_size;
  specf->init_func = init_func;
  specf->copy_func = copy_func;
  specf->delete_func = delete_func;
  return specf;
}

// auto (flagbit) handlers

static int _lsd_auto_delete(void *ptr, uint64_t flags, size_t bsize) {
  if (flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE) {
    if (!(flags & LIVES_FIELD_FLAG_IS_NULLT_ARRAY)) {
      flags &= ~LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE;
      flags |= LIVES_FIELD_FLAG_FREE_ON_DELETE;
    }
  }
  if (flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE) {
    void **vptr = *((void ***)ptr);
    if (vptr) {
      if ((flags & LIVES_FIELD_FLAG_ALLOC_AND_COPY) && !bsize) {
        for (int j = 0; vptr[j]; j++) if (vptr[j])(*_lsd_string_free)(vptr[j]);
      } else {
        for (int j = 0; vptr[j]; j++) if (vptr[j])(*_lsd_free)(vptr[j]);
      }
    }
  }
  if (flags & LIVES_FIELD_FLAG_FREE_ON_DELETE) {
    void *vptr = *((void **)ptr);
    if (vptr) {
      if ((flags & LIVES_FIELD_FLAG_ALLOC_AND_COPY) && !bsize
          && !(flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE)) {
        //g_print("flags !!! %lu %lu\n", flags, sizee);
        (*_lsd_string_free)(vptr);
      } else {
        //g_print("flags %lu %lu\n", flags, sizee);
        (*_lsd_free)(vptr);
      }
    }
  }
  return 0;
}

static void _lsd_auto_copy(void *dst_field, void *src_field, lives_special_field_t *spcf,
                           lives_struct_def_t *lsd) {
  size_t bsize = spcf->bytesize;
  int j;
  if (!(spcf->flags & LIVES_FIELD_FLAG_IS_NULLT_ARRAY)) {
    if (spcf->flags & LIVES_FIELD_FLAG_ALLOC_AND_COPY) {
      if (bsize) {
        if (bsize > MAX_ALLOC) {
          debug_print("error: memory request too large (%lu > %lu)\n", bsize, MAX_ALLOC);
          return;
        } else {
          debug_print("allocating %lu bytes...", bsize);
          if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
            if (!(*_lsd_calloc_aligned)((void **)dst_field, 1, bsize)) {
              debug_print("and set to zero.\n");
            } else {
              memerr_print(bsize, spcf->name, lsd->structtype);
            }
            return;
          } else {
            if (src_field != lsd && !(*((void **)src_field))) {
              debug_print("value is NULL, not copying\n");
            } else {
              if (!(*_lsd_calloc_aligned)((void **)dst_field, 1, bsize)) {
                (*_lsd_memcpy)(*(void **)dst_field, src_field, bsize);
                debug_print("and copying from src to dest.\n");
              } else {
                memerr_print(bsize, spcf->name, lsd->structtype);
                return;
              }
		// *INDENT-OFF*
	      }}}
	  // *INDENT-ON*
      } else {
        // strings
        char **cptr = (char **)dst_field;
        if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
          // set the string to an empty string
          // without ALLOC_AND_COPY we set it to NULL instead
          *cptr = (*_lsd_strdup)("");
          debug_print("string was set to \"\"\n");
        } else {
          if ((*((char **)src_field))) {
            *cptr = (*_lsd_strdup)(*((char **)src_field));
            debug_print("did a strdup from src to dest\n");
#ifdef SHOW_TEXT
            debug_print("%s\n", *cptr);
#endif
          } else {
            debug_print("value is NULL, not copying\n");
          }
        }
      }
      if (!(spcf->flags & LIVES_FIELD_FLAG_FREE_ON_DELETE)) {
        debug_print("WARNING: FREE_ON_DELETE not set\n");
      }
      if (spcf->flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE) {
        debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
      }
      return;
    }
    // non-alloc
    if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
      if (bsize) {
        (*_lsd_memset)(dst_field, 0, bsize);
        debug_print("zeroed %lu bytes\n", bsize);
      } else {
        *((char **)dst_field) = NULL;
        debug_print("set string to NULL\n");
      }
    }
    if ((spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) && !bsize) {
      if (!(spcf->flags & LIVES_FIELD_FLAG_FREE_ON_DELETE)) {
        debug_print("WARNING: FREE_ON_DELETE not set\n");
      }
    } else {
      if (spcf->flags & LIVES_FIELD_FLAG_FREE_ON_DELETE) {
        debug_print("WARNING: FREE_ON_DELETE is set\n");
      }
    }
    if (spcf->flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE) {
      debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
    }
    return;
  }
  if (spcf->flags & LIVES_FIELD_FLAG_IS_NULLT_ARRAY) {
    int count = 0;
    debug_print("handling array...");
    // copy / create n elements or strings
    if (!bsize) {
      // copy N strings or create empty strings, source field is char **
      char **cptr = (*(char ***)src_field), **dptr;
      if (cptr) {
        while (cptr[count]) count++;
        if ((*_lsd_calloc_aligned)((void **)&dptr, count + 1, sizeof(char *))) {
          memerr_print(bsize, spcf->name, lsd->structtype);
          return;
        }
        for (j = 0; j < count; j++) {
          // flags tells us what to with each element
          if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
            dptr[j] = (*_lsd_strdup)("");
          } else {
            if (spcf->flags & LIVES_FIELD_FLAG_ALLOC_AND_COPY) {
              dptr[j] = (*_lsd_strdup)(cptr[j]);
            } else dptr[j] = cptr[j];
          }
        }
        dptr[j] = NULL; /// final element must always be NULL
        (*(char ***)dst_field) = dptr;
      }

      if (!cptr) {
        debug_print("value is NULL, not copying\n");
      } else {
        if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
          debug_print("created %d empty strings (+ terminating NULL)\n", count);
        } else {
          if (spcf->flags & LIVES_FIELD_FLAG_ALLOC_AND_COPY) {
            debug_print("duplicated %d strings (+ terminating NULL)\n", count);
          } else {
            debug_print("copy-by-ref %d strings (+ terminating NULL)\n", count);
          }
        }
      }
      if (!(spcf->flags & LIVES_FIELD_FLAG_FREE_ON_DELETE)) {
        debug_print("WARNING: FREE_ON_DELETE not set\n");
      }
      if (!(spcf->flags & LIVES_FIELD_FLAG_ALLOC_AND_COPY) &&
          !(spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY)) {
        if (spcf->flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE) {
          debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
        }
      } else {
        if (!(spcf->flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE)) {
          debug_print("WARNING: FREE_ALL_ON_DELETE not set\n");
        }
      }
      return;
    } else {
      if (spcf->flags & (LIVES_FIELD_FLAG_ALLOC_AND_COPY
                         | LIVES_FIELD_FLAG_ZERO_ON_COPY)) {
        /// alloc and copy elements of size bsize
        void **vptr = (*(void ***)src_field), **dptr;
        if (vptr) {
          count = 0;
          while (vptr[count]) count++;
          if ((*_lsd_calloc_aligned)((void **)&dptr, count + 1, sizeof(void *))) {
            memerr_print((count + 1) * sizeof(void *), spcf->name, lsd->structtype);
            return;
          }
          for (j = 0; j < count; j++) {
            // flags tells us what to with each element
            if ((*_lsd_calloc_aligned)((void **)&dptr[j], 1, bsize)) {
              memerr_print(bsize, spcf->name, lsd->structtype);
              return;
            } else {
              if (!(spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY)) {
                (*_lsd_memcpy)(dptr[j], vptr[j], bsize);
              }
            }
          }
          dptr[j] = NULL; /// final element must always be NULL
          (*(void **)dst_field) = dptr;
        }
        if (!vptr) {
          debug_print("value is NULL, not copying\n");
        } else {
          if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
            debug_print("created %d pointers to empty elements of size %lu "
                        "(+ terminating NULL)\n", count, bsize);
          } else {
            debug_print("duplicated %d pointers to elements of size %lu "
                        "(+ terminating NULL)\n",
                        count, bsize);
          }
        }
        if (!(spcf->flags & LIVES_FIELD_FLAG_FREE_ON_DELETE)) {
          debug_print("WARNING: FREE_ON_DELETE not set\n");
        }
        if (!(spcf->flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE)) {
          debug_print("WARNING: FREE_ALL_ON_DELETE not set\n");
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
          if ((*_lsd_calloc_aligned)((void **)&newarea, count, bsize)) {
            memerr_print(bsize, spcf->name, lsd->structtype);
            return;
          } else {
            (*_lsd_memcpy)(newarea, src_field, count * bsize);
            *((char **)dst_field) = (char *)newarea;
          }
        }
        if (!ptr) {
          debug_print("value is NULL, not copying\n");
        } else {
          debug_print("- copied %d values of size %ld (including final 0's)\n",
                      count, bsize);
        }
        if (!(spcf->flags & LIVES_FIELD_FLAG_FREE_ON_DELETE)) {
          debug_print("WARNING: FREE_ON_DELETE not set\n");
        }
        if (spcf->flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE) {
          debug_print("WARNING: FREE_ALL_ON_DELETE is set\n");
        }
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*
}

static void _lsd_struct_free(lives_struct_def_t *) ALLOW_UNUSED;
static void _lsd_struct_free(lives_struct_def_t *lsd) {
  lives_special_field_t **spfields, *self_fields_self_fields = NULL;
  uint64_t lsd_flags = 0;
  size_t lsd_size = 0;
  void *src_field;
  void *thestruct = NULL;

  if (!lsd) return;

  if (_lsd_generation_check_lt(lsd, 1, 0)) {
    spfields = lsd->self_fields;
  } else {
    thestruct = lsd->top;

    if (lsd->destroy_struct_callback)
      (*lsd->destroy_struct_callback)(thestruct, lsd->structtype, lsd->destroy_user_data);

    spfields = lsd->special_fields;
  }

recurse_free:
  if (spfields) {
    char *top;
    off_t xoffset = 0;
    if (spfields == lsd->self_fields) {
      xoffset = spfields[0]->offset_to_field;
    }
    for (int i = 0; spfields[i]; i++) {
      lives_special_field_t *spcf = spfields[i];
      if (spfields == lsd->self_fields) {
        if (spfields[0]->bytesize) {
          top = (char *)lsd->top;
          if (!i) {
            lsd_size = spcf->bytesize;
            lsd_flags = spcf->flags;
          } else {
            top = ((char *)lsd->top + xoffset);
            top = (char *)(*((lives_struct_def_t **)top));
          }
        } else top = lsd->top + xoffset;
      } else top = lsd->top;

      src_field = top + spcf->offset_to_field;
      if (spcf->delete_func)(*spcf->delete_func)(lsd->top, SELF_STRUCT_TYPE,
            spcf->name, src_field);
      if (spfields == lsd->special_fields) {
        src_field = (char *)lsd->top + spcf->offset_to_field;
        if (spcf->delete_func)(*spcf->delete_func)(lsd->top, lsd->structtype,
              spcf->name, src_field);
      }
    }

    for (int i = 0; spfields[i]; i++) {
      lives_special_field_t *spcf = spfields[i];
      uint64_t flags = spcf->flags;

      if (spfields == lsd->self_fields) src_field = (char *)lsd + spcf->offset_to_field;
      else src_field = (char *)lsd->top + spcf->offset_to_field;

      if (src_field == &spfields) {
        /// self_fields must be done last, since it is still needed for now
        self_fields_self_fields = spcf;
        continue;
      }

      if (!flags) continue;

      if (flags & LIVES_FIELD_FLAG_IS_SUBSTRUCT) {
        /// some other kind of substruct, find its lsd and call again
        /// TODO
        continue;
      }
      _lsd_auto_delete(src_field, flags, spcf->bytesize);
    }
  }

  // after freeing the struct fields we free the lives_struct_def_t itself
  // this is done using the values contained in selfdata
  if (spfields == lsd->special_fields) {
    spfields = lsd->self_fields;
    goto recurse_free;
  }

  if (self_fields_self_fields) {
    src_field = (void *)((char *)lsd + self_fields_self_fields->offset_to_field);
    _lsd_auto_delete(src_field, self_fields_self_fields->flags, 1);
  }

  if (lsd_flags) {
    src_field = (void *)lsd;
    _lsd_auto_delete(src_field, lsd_flags, lsd_size);
  }

  if (thestruct)(*_lsd_free)(thestruct);
  else (*_lsd_free)(lsd);
}

static void *_lsd_struct_copy(lives_struct_def_t *lsd, void *new_struct) {
  lives_special_field_t **spfields;
  char *top, *dst_field, *src_field;
  void *parent = NULL;
  off_t offset = 0, xoffset;
  spfields = lsd->self_fields;

  if (!new_struct) {
    // copy
    if ((*_lsd_calloc_aligned)((void **)&new_struct, 1, lsd->structsize)) {
      memerr_print(lsd->structsize, "ALL FIELDS", lsd->structtype);
      return NULL;
    }
    parent = lsd->top;
    debug_print("copying struct of type: %s, %p -> %p, with size %lu\n", lsd->structtype,
                parent, new_struct,
                lsd->structsize);
    xoffset = ((char *)lsd - (char *)lsd->top);
    (*_lsd_memcpy)(new_struct, parent, lsd->structsize);
#ifdef DO_ALIGN_CHECK
    _lsd_memalign_check(parent, new_struct, lsd->structsize);
#endif
  } else {
    // init
    xoffset = spfields[0]->offset_to_field;
    if (xoffset < 0) {
      baderr_print("Unable to copy struct of type %s, lives_struct_init must be called first\n",
                   lsd->structtype);
      return NULL;
    }
    debug_print("initing struct %p of type: %s\n", new_struct, lsd->structtype);
  }

  // copy self_fields first:

  debug_print("copying lives_struct_def_t fields first\n");

recurse_copy:

  if (spfields) {
    for (int i = 0; spfields[i]; i++) {
      lives_special_field_t *spcf = spfields[i];

      if (!spcf->flags) continue;
      if (i > 1) debug_print("field done\n\n");

      if (spfields == lsd->self_fields) {
        if (spfields[0]->bytesize) {
          top = (char *)new_struct;
          if (i) {
            top = ((char *)new_struct + xoffset);
            top = (char *)(*((lives_struct_def_t **)top));
          }
        } else top = new_struct + xoffset;
      } else top = new_struct;

      offset = spcf->offset_to_field;
      dst_field = top + offset;

      debug_print("handling field %s at offset %lu from %p with flags 0X%016lX\n",
                  spcf->name, offset, top, spcf->flags);
      if (spcf->flags & LIVES_FIELD_FLAG_IS_SUBSTRUCT) {
        // otherwise we descend into the substruct and locate its lives_struct_def *
        // TODO...
        debug_print("field is another substruct {TODO}\n");
        continue;
      }

      if (spfields == lsd->self_fields) {
        if (!i && spfields[0]->bytesize)
          src_field = (char *)lsd;
        else
          src_field = (char *)lsd + offset;
      } else src_field = (char *)parent + offset;

      _lsd_auto_copy(dst_field, src_field, spcf, lsd);
    }
    // *INDENT-ON*

    debug_print("all fields in struct copied\n\n");

recurse_callbacks:
    /// call callbacks
    if (spfields) {
      for (int i = 0; spfields[i]; i++) {
        lives_special_field_t *spcf = spfields[i];
        if (spfields == lsd->self_fields) {
          if (spfields[0]->bytesize) {
            top = (char *)new_struct;
            if (i) {
              top = ((char *)new_struct + xoffset);
              top = (char *)(*((lives_struct_def_t **)top));
            }
          } else top = new_struct + xoffset;
        } else top = new_struct;
        offset = spcf->offset_to_field;
        dst_field = top + offset;
        if (parent) {
          if (spcf->copy_func) {
            debug_print("calling copy_func for %s\n", spcf->name);
            if (spfields == lsd->self_fields) src_field = (char *)lsd + offset;
            else src_field = (char *)parent + offset;
            (*spcf->copy_func)(new_struct, lsd->top, (spfields == lsd->special_fields)
                               ? lsd->structtype : SELF_STRUCT_TYPE,
                               spcf->name, dst_field, src_field);
          }
        } else {
          if (spcf->init_func) {
            debug_print("calling init_func for %s\n", spcf->name);
            (*spcf->init_func)(new_struct, (spfields == lsd->special_fields)
                               ? lsd->structtype : SELF_STRUCT_TYPE,
                               spcf->name, dst_field);
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-ON*

  if (spfields != lsd->special_fields) {
    // after copying structdef, we copy the normal fields
    spfields = lsd->special_fields;
    if (!parent) {
      debug_print("initing normal fields\n");
      if (spfields) goto recurse_callbacks;
    } else {
      debug_print("copying normal fields\n");
      if (spfields) goto recurse_copy;
    }
  }

  if (parent)
    debug_print("struct copy done\n\n");
  else
    debug_print("struct init done\n\n");

  debug_print("triggering any struct callbacks\n\n");

  if (parent) {
    if (lsd->copied_struct_callback)
      (*lsd->copied_struct_callback)(parent, new_struct, lsd->structtype, lsd->copied_user_data);
  }

  /* if (dst_lsd->new_struct_callback) */
  /*   (*dst_lsd->new_struct_callback)(new_struct, parent, lsd->structtype, dst_lsd->new_user_data); */

  return new_struct;
}


static int _lsd_struct_init(const lives_struct_def_t *lsd, void *thestruct,
                            lives_struct_def_t **lsd_in_structp, int is_ptr) {
  lives_struct_def_t *lsd_in_struct;
  if (!lsd || !thestruct || !lsd_in_structp) return EINVAL;
  if (!is_ptr && !(lsd_in_struct = (lives_struct_def_t *)*lsd_in_structp))
    return EINVAL;
  if (!_lsd_generation_check_lt((lives_struct_def_t *)lsd, 1, 0)) return EINVAL;
  else {
    lives_special_field_t **spfields = lsd->self_fields;
    if (is_ptr)
      spfields[0] = _lsd_make_special_field(LIVES_FIELD_FLAG_ALLOC_AND_COPY, thestruct,
                                            lsd_in_structp, "lsd",
                                            sizeof(lives_struct_def_t), NULL, NULL, NULL);
    else
      spfields[0] = _lsd_make_special_field(0, thestruct, lsd_in_struct, "lsd", 0, NULL, NULL, NULL);
  }
  return 0;
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
  return !lsd ? NULL : lsd->structtype;
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
  return !lsd ? 0 : lsd->structsize;
}
static void lives_struct_set_user_data(lives_struct_def_t *lsd, void *data) {
  if (lsd) lsd->user_data = data;
}
static void *lives_struct_get_class_data(lives_struct_def_t *lsd) {
  return !lsd ? NULL : lsd->class_data;
}
static void lives_struct_set_class_data(lives_struct_def_t *lsd, void *data) {
  if (lsd) lsd->class_data = data;
}

static lives_special_field_t *make_special_field(uint64_t flags, void *thestruct,
    void *ptr_to_field,
    const char *name,
    size_t data_size,
    lives_field_init_cb init_func,
    lives_field_copy_cb copy_func,
    lives_field_delete_cb delete_func) {
  return _lsd_make_special_field(flags, thestruct, ptr_to_field, name, data_size,
                                 init_func, copy_func, delete_func);
}

static int lives_struct_ref(lives_struct_def_t *lsd) {
  return lsd ? ++((lives_struct_def_t *)lsd)->refcount : 0;
}

static int lives_struct_unref(lives_struct_def_t *lsd) {
  if (lsd) {
    if (!lsd->top) {
      baderr_print("Unable to free struct of type %s, lives_struct_init must be called first\n",
                   lsd->structtype);
      return -1;;
    }
    int rc = --(((lives_struct_def_t *)lsd)->refcount);
    if (rc <= 0) {
      if (_lsd_generation_check_lt((lives_struct_def_t *)lsd, 1, 0))
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
  if (!_lsd_generation_check_gt(lsd, 0, 1)) return NULL;
  return _lsd_struct_copy(lsd, NULL);
}

static int lives_struct_init_p(const lives_struct_def_t *lsd, void *thestruct,
                               lives_struct_def_t **lsd_in_struct) {
  /// the difference between this function call and the following is that here we allocate
  // copy and free lsd, and additionally, special_field offsets are measured from the
  // dereferenced field rather than the field itself
  // internally this is signalled by setting the bytesize for lsd->special_fields[0].bytesize
  // to sizeof(lives_struct_def_t *) instead of 0...
  return _lsd_struct_init(lsd, thestruct, lsd_in_struct, 1);
}

static int lives_struct_init(const lives_struct_def_t *lsd, void *thestruct,
                             lives_struct_def_t *lsd_in_struct) {
  // in other cases a bytesize of zero indicates a string, but in this case it is not relevant
  // since flags is also zero. In the unforseen case that flags ever needs setting, then care
  // needs to be taken in callbacks to use memcpy rather than strdup, etc.
  return _lsd_struct_init(lsd, thestruct, &lsd_in_struct, 0);
}

static void *lives_struct_create(const lives_struct_def_t *lsd) {
  void *thestruct, *lsd_in_struct;
  lives_special_field_t **spfields;
  off_t offset;

  if (!lsd) return NULL;
  spfields = lsd->self_fields;
  if (!spfields) return NULL;
  offset = spfields[0]->offset_to_field;
  if (offset < 0) {
    baderr_print("Unable to create struct of type %s, lives_struct_init must be called first\n",
                 lsd->structtype);
    return NULL;
  }
  if ((*_lsd_calloc_aligned)((void **)&thestruct, 1, lsd->structsize)) {
    memerr_print(lsd->structsize, "ALL", lsd->structtype);
    return NULL;
  }

  if (!(spfields[0]->bytesize)) {
    lsd_in_struct = (void *)((char *)thestruct + offset);
    (*_lsd_memcpy)(lsd_in_struct, lsd, sizeof(lives_struct_def_t));
  }
  _lsd_struct_copy((lives_struct_def_t *)lsd, thestruct);
  return thestruct;
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
  // must have same identifier, end_id, structtype, structsize, last_field, class_data
  if (lsd1->structsize == lsd2->structsize
      && lsd1->identifier == lsd2->identifier
      && lsd1->end_id == lsd2->end_id
      && (!(*_lsd_strcmp)(lsd1->structtype, lsd2->structtype))
      && (!(*_lsd_strcmp)(lsd1->class_data, lsd2->class_data))
      && (!(*_lsd_strcmp)(lsd1->last_field, lsd2->last_field))) return 1;
  return 0;
}

/// the process works as follows:
/// - call lsd_create, passing in the name of the struct, size, last field name (optional),
/// and number of "special fields"
/// special fields are any which have init / copy / delete functions
/// If the struct has special_fields then:
/// call make_special_field for each elemnt in lsd->special_fields
/// for this it is necessary to make 1 instance of struct via normal means
/// field values in the struct are irrelevant, the struct is only needed to get the field offsets
/// fpr any special fields
/// finally, call lives_struct_init, passing in lsd, pointer to struct and pointer to lsd field in
/// the struct.
/// This serves to finalise the lsd, and initialize field values in the struct + lsd in struct.
///
///
/// the returned lives_struct_def_t can now be used as a "template" to make new structs
/// lives_struct_new can take a static struct_def and return a new struct
/// (this is just like lives_struct_copy, except that init_func is called on the fields
/// instead of copy_func.)
///

/// NB. once support for substructs is added, it will possible for a single struct to cantain
/// multiple lsd, each with its own size, last_field and special_fields

static const lives_struct_def_t *lsd_create(const char *struct_type, size_t struct_size,
    const char *last_field, int nspecial) {
  lives_special_field_t **xspecf;
  lives_struct_def_t *lsd;

  if ((*_lsd_calloc_aligned)((void **)&lsd, 1, sizeof(lives_struct_def_t))) {
    memerr_print(sizeof(lives_struct_def_t), "LSD template", lsd->structtype);
    return NULL;
  }

  if (struct_type)
    snprintf(lsd->structtype, LSD_TEXTLEN, "%s", struct_type);

  lsd->structsize = struct_size;
  lsd->refcount = 1;

  if (last_field)
    snprintf(lsd->last_field, LSD_TEXTLEN, "%s", last_field);

  if (nspecial > 0) {
    if ((*_lsd_calloc_aligned)((void **)&lsd->special_fields, nspecial + 1,
                               sizeof(lives_special_field_t *))) {
      memerr_print((nspecial + 1) * sizeof(lives_special_field_t *), "lsd.special_fields",
                   SELF_STRUCT_TYPE);
      return NULL;
    }

    lsd->special_fields[nspecial] = NULL;
  }

  if ((*_lsd_calloc_aligned)((void **) & (lsd->self_fields), 11, sizeof(lives_special_field_t *))) {
    memerr_print(8 * sizeof(lives_special_field_t *), "lsd.self_fields", SELF_STRUCT_TYPE);
    return NULL;
  }
  xspecf = lsd->self_fields;

  // xspecf[0] stays as NULL for now

  // set on init
  xspecf[1] = _lsd_make_special_field(0, lsd, &lsd->identifier, "identifier", 0, _lsd_init_cb,
                                      NULL, NULL);

  // set to val
  xspecf[2] = _lsd_make_special_field(0, lsd, &lsd->end_id, "end_id", 0, _lsd_init_cb,
                                      NULL, NULL);

  // set a new random value on init / copy
  xspecf[3] = _lsd_make_special_field(0, lsd, &lsd->unique_id, "unique_id", 0,
                                      _lsd_init_cb, _lsd_copy_cb, NULL);

  // point ot struct on init / copy
  xspecf[4] = _lsd_make_special_field(0, lsd, &lsd->top, "top", 0, _lsd_init_cb, _lsd_copy_cb, NULL);

  // et to 1 on init / copy
  xspecf[5] = _lsd_make_special_field(0, lsd, &lsd->refcount, "refcount", 0,
                                      _lsd_init_cb, _lsd_copy_cb, NULL);

  // set to 1 on init, increment on copy
  xspecf[6] = _lsd_make_special_field(0, lsd, &lsd->generation, "generation", 0,
                                      _lsd_init_cb, _lsd_copy_cb, NULL);

  // values will be alloced and copied to a copy struct,
  xspecf[7] = _lsd_make_special_field(LIVES_FIELD_PTR_ARRAY, lsd,
                                      &lsd->special_fields, "special_fields",
                                      sizeof(lives_special_field_t), NULL, NULL, NULL);

  // value will be set to zero after copying
  xspecf[8] = _lsd_make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY, lsd,
                                      &lsd->user_data, "user_data", 8, NULL, NULL, NULL);

  // in theaory we should put put "top" after this with FREE_ON_DELETE set there.
  // however we cannot free the struct until all its fields are freed.

  xspecf[9] = _lsd_make_special_field(LIVES_FIELD_PTR_ARRAY, lsd,
                                      &lsd->self_fields, "self_fields",
                                      sizeof(lives_special_field_t),
                                      NULL, NULL, NULL);
  // set on init
  xspecf[10] = NULL;

  return lsd;
}

///// API ////////////////////

static int lsd_free(const lives_struct_def_t *) ALLOW_UNUSED;
static int lsd_unref(const lives_struct_def_t *) ALLOW_UNUSED;
static int lsd_ref(const lives_struct_def_t *) ALLOW_UNUSED;
static int lsd_get_refcount(const lives_struct_def_t *) ALLOW_UNUSED;

// returns 1 if both struct defs have  same identifier, end_id, structtype,
// structsize, last_field, class_data (i.e one is copy / instance of another)
// (lives_struct_get_generation can provide more information)
static int lsd_same_family(const lives_struct_def_t *lsd1, lives_struct_def_t *lsd2) ALLOW_UNUSED;

// sets class data which will be copied to all instances from template
// and from instance to copies.
static void lives_struct_set_class_data(lives_struct_def_t *, void *class_data) ALLOW_UNUSED;
static void *lives_struct_get_class_data(lives_struct_def_t *) ALLOW_UNUSED;

static const lives_struct_def_t *lsd_create(const char *struct_type,
    size_t struct_size, const char *last_field,
    int nspecial) ALLOW_UNUSED;

// function to define special fields, array elements returned from make_structdef
// should be assigned via this function
static lives_special_field_t *make_special_field(uint64_t flags, void *sample_struct,
    void *ptr_to_field,
    const char *field_name,
    size_t data_size,
    lives_field_init_cb init_func,
    lives_field_copy_cb copy_func,
    lives_field_delete_cb delete_func) ALLOW_UNUSED;

// Finishes the initialisation of the lsd template (passed as the first parameter)
// a sample instance of the struct should be created (using malloc / calloc, etc)
// and passed as the second parameter. The sample should be freed afterwards using normal free.
// All subsequent instances must be created with lives_struct_create or lives_struct_copy.
// The function returns 0 on success, EINVAL if a paramter is invalid.
// this version should be used when sruct has a field with type (lives_struct_def_t)
static int lives_struct_init(const lives_struct_def_t *, void *thestruct,
                             lives_struct_def_t *) ALLOW_UNUSED;

// as above - this version should be used when lsd is of type (lives_struct_def_t *)
static int lives_struct_init_p(const lives_struct_def_t *, void *thestruct,
                               lives_struct_def_t **) ALLOW_UNUSED;

/// creates a new instance fo struct. lsd can be one returned from create_lsd + lives_struct_init
// or it can be lsd from inside another struct (cast to const)
static void *lives_struct_create(const lives_struct_def_t *) ALLOW_UNUSED;

// allocates and returns a copy of struct, calls copy_funcs, fills in lives_struct_def for copy
// lsd must be within a struct, not a static template
static void *lives_struct_copy(lives_struct_def_t *) ALLOW_UNUSED;

// just calls lives_struct_unref
static int lives_struct_free(lives_struct_def_t *) ALLOW_UNUSED;

// decrements refcount, then if <=0 frees struct. Returns refcount (so value <=0 means struct freed)
// returns -1 if parameter is NULL
static int lives_struct_unref(lives_struct_def_t *) ALLOW_UNUSED;

// increments refcount, returns new value. Returns 0 if paramter is NULL
static int lives_struct_ref(lives_struct_def_t *) ALLOW_UNUSED;

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
/////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
#undef DEBUG
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
