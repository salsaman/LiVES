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
#define debug_print(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug_print(a)
#endif

#define ALLOW_UNUSED __attribute__((unused))

#define TEXTLEN 64
#define MAX_ALLOC 100000000ul
#define LIVES_STRUCT_ID 0x4C7C56332D2D3035  /// 1st 8 bytes - L|V3--05 (be) or 50--3V|L (le)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef OVERRIDE_MEMFUNCS
static void *(*calloc_func)(size_t nmemb, size_t size) = calloc;
static void *(*memcpy_func)(void *dest, const void *src, size_t n) = memcpy;
static void *(*memset_func)(void *s, int c, size_t n) = memset;
static void (*free_func)(void *ptr) = free;
#endif

/// copy flags
///< alloc and copy on copy. If bytesize is set that will be the alocated size,
/// if 0 then we do a strdup. Fixed size is ignored for arrays.
#define LIVES_FIELD_FLAG_ALLOC_AND_COPY (1l<<0)

///< if bytesize is 0, field will be set to NULL in lives_struct_copy,
/// if ALLOC_AND_COPY is also set, will be set to empty string
/// if bytesize > 0 and not IS_NULLT_ARRAY
/// then field will will be filled with bytesize zeros
/// dest->field = NULL or memset(dets->field, 0, bytesize)
/// for ARRAYS, the process will be appied to each element in turn
/// however, since a NULL element marks the end of a NULLT_ARRAY,
/// the combination ALLOC_AND_COPY | ZERO_ON_COPY | IS_NULLT_ARRAY may interfere with
/// subsequent copying
#define LIVES_FIELD_FLAG_ZERO_ON_COPY (1l<<1)

// delete flags
///< field wiill be freed in lives_struct_delete
/// free(struct->field)
#define LIVES_FIELD_FLAG_FREE_ON_DELETE (1l<<16)

/// for (i = 0; struct->field[i], i++) free(struct->field[i];
#define LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE (1l<<17) ///< combined with IS_NULLT_ARRAY, frees all elements, combine with FREE_ON_DELETE to free elemnt after

/// flags giving extra info about the field (affects copy and delete)

///< field is a substruct with its own lives_struct_def_t; functions should be called recursively
/// it must be possible to locate the lives_struct_def_t field from the first byte
/// sequence matching LIVES_STRUCT_ID in its identifier field
/// lives_struct_copy(struct->field
/// if this is set, all other flag bits are ignored for the field
#define LIVES_FIELD_FLAG_IS_SUBSTRUCT (1l<<32)

///< field is an array of elements of size bytelen, last element has all bytes set to zero
/// if bytesize is zero, it is an array of NUL terminated char
/// may be combined with ALLOC_AND_COPY, FREE_ON_DELETE, FREE_ALL_ON_DELETE
#define LIVES_FIELD_FLAG_IS_NULLT_ARRAY (1l<<33)

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

typedef void (*lives_field_copy_f)(void *dst_struct, void *src_struct, const char *strct_type,
                                   const char *field_name, void *ptr_to_dst_field,
                                   void *ptr_to_src_field);
// e,g, using field names:
// strct_type *d = (strct_type *)dst_struct;
// strct_type *s = (strct_type *)src_struct;
/// d->*field_name = s->(field_name) + 10;
/// or using anonymous fields:
/// *(int *)dst_field = *(int *)src_field + 10;

typedef void (*lives_field_delete_f)(void *strct, const char *struct_type,
                                     const char *field_name, void *ptr_to_field);
// e,g
// strct_type *mystruct = (strct_type *)strct;
// free(mystryct->(field_name));

typedef void (*lives_field_update_f)(void *strct, const char *struct_type,
                                     const char *field_name, void *ptr_to_field);
// lives_struct_copy will call this (after calling copy_func if that function is defined)
// may be called manually at other times after updating the field to notify a listener

// STRUCTS

typedef struct _lives_special_field {
  /// flags may be  0 to optionally provide info re. the field name, bytesize,
  //(and optionally ptr_to_field)
  uint64_t flags;
  /// must be set when creating the struct
  void *ptr_to_field;
  char name[TEXTLEN]; /// optional unless flags == 0 or any of the functions below are defined.
  size_t bytesize; /// defines the elemnt size for
  lives_field_copy_f copy_func; ///< will be called from lives_struct_copy
  lives_field_delete_f delete_func; ///< called from lives_struct_free
  lives_field_update_f update_func; ///< may be called manually after updating a field value
} lives_special_field_t;

typedef struct _lives_struct_def {
  uint64_t identifier;  /// LIVES_STRUCT_ID / LIVES_STRUCT_SELF_ID
  uint64_t unique_id; /// main and self share sane id

  char structtype[TEXTLEN]; /// type of the struct as string, e.g "lives_struct_def_t"
  void *top; ///< ptr to the start of parent struct itself, typecast to a void *
  size_t structsize; ///< bytesize of parent struct (sizef(struct))
  char last_field[TEXTLEN]; ///< name of last field of struct

  lives_special_field_t **special_fields;  /// may be NULL, else is pointer to NULL terminated array
  lives_special_field_t **self_fields;  /// fields in the struct_def_t struct itself

  void *user_data; /// user_data for instances of struct
} lives_struct_def_t;

#define SELF_STRUCT_TYPE "lives_struct_def_t"

////// FUNCTION BODIES //////

#ifndef IGN_RET
// for getentropy
#define IGN_RET(a) ((void)((a) + 1))
#endif

// builtin delete callback
static void gen_delete(void *strct, const char *strct_type, const char *field_name,
                       void *ptr_to_field) {
  if (!strcmp(strct_type, SELF_STRUCT_TYPE)) {
    if (!strcmp(field_name, "self_fields")) {
      lives_special_field_t **spfields = *((lives_special_field_t ***)ptr_to_field);
      for (int i = 0; spfields[i]; i++)(*free_func)(spfields[i]);
      (*free_func)(spfields);
    }
  }
}

// builtin copy cb
static void gen_copy(void *dst, void *src, const char *strct_type, const char *field_name,
                     void *dst_fld_ptr, void *src_fld_ptr) {
  if (!strcmp(strct_type, SELF_STRUCT_TYPE)) {
    if (!strcmp(field_name, "unique_id")) {
      IGN_RET(getentropy(dst_fld_ptr, 8));
    } else if (!strcmp(field_name, "top")) {
      *(void **)dst_fld_ptr = dst;
    } else if (!strcmp(field_name, "special_fields") || !strcmp(field_name, "self_fields")) {
      off_t offset;
      lives_special_field_t **dspf = *((lives_special_field_t ***)dst_fld_ptr);
      lives_special_field_t **sspf = *((lives_special_field_t ***)src_fld_ptr);
      for (int i = 0; sspf[i]; i++) {
        offset = (char *)sspf[i]->ptr_to_field - (char *)src;
        dspf[i]->ptr_to_field = (char *)dst + offset;
	  // *INDENT-OFF*
	}
      }}
    // *INDENT-ON*
}

//// API FUNCTIONS /////////

static lives_special_field_t *make_special_field(uint64_t flags, void *ptr_to_field,
    const char *name,
    size_t data_size,
    lives_field_copy_f copy_func,
    lives_field_delete_f delete_func,
    lives_field_update_f update_func) {
  lives_special_field_t *specf =
    (lives_special_field_t *)(*calloc_func)(1, sizeof(lives_special_field_t));
  if (specf) {
    specf->flags = flags;
    specf->ptr_to_field = ptr_to_field;
    snprintf(specf->name, TEXTLEN, "%s", name);
    specf->bytesize = data_size;
    specf->copy_func = copy_func;
    specf->delete_func = delete_func;
    specf->update_func = update_func;
  }
  return specf;
}

static void lives_struct_free(lives_struct_def_t *lsd) {
  lives_special_field_t **spfields;
  void *thestruct;
  int alldone = 0;

  if (!lsd) return;

  thestruct = lsd->top;
  spfields = lsd->special_fields;

recurse_free:

  if (spfields) {
    for (int i = 0; spfields[i]; i++) {
      lives_special_field_t *spcf = spfields[i];
      uint64_t flags = spcf->flags;
      void *ptr = spcf->ptr_to_field;

      if (!flags) continue;

      if (flags & LIVES_FIELD_FLAG_IS_SUBSTRUCT) {
        /// some other kind of substruct, find its lsd and call again
        /// TODO
        continue;
      }

      if (flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE) {
        if (!(flags & LIVES_FIELD_FLAG_IS_NULLT_ARRAY)) {
          flags &= ~LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE;
          flags |= LIVES_FIELD_FLAG_FREE_ON_DELETE;
        }
      }
      if (flags & LIVES_FIELD_FLAG_FREE_ALL_ON_DELETE) {
        void **vptr = *((void ***)ptr);
        if (vptr) {
          for (int j = 0; vptr[j]; j++) if (vptr[j])(*free_func)(vptr[j]);
        }
      }
      if (flags & LIVES_FIELD_FLAG_FREE_ON_DELETE) {
        void *vptr = *((void **)ptr);
        if (vptr)(*free_func)(vptr);
      }
    }
    for (int i = 0; spfields[i]; i++) {
      lives_special_field_t *spcf = spfields[i];
      if (spcf->ptr_to_field == spfields) alldone = 1;
      if (spcf->delete_func)(*spcf->delete_func)(lsd->top,
            (spfields == lsd->special_fields)
            ? lsd->structtype : SELF_STRUCT_TYPE,
            spcf->name, spcf->ptr_to_field);
      if (alldone) break;
    }
  }

  if (alldone) {
    (*free_func)(thestruct);
    return;
  }

  // after freeing the struct fields we free the lives_struct_def_t itself
  // this is done using the values contained in selfdata
  if (spfields == lsd->special_fields) {
    spfields = lsd->self_fields;
    goto recurse_free;
  }
}

static void *lives_struct_copy(lives_struct_def_t *lsd) {
  lives_special_field_t **spfields;
  off_t offset = 0;
  void *dst_field;
  void *new_struct = (*calloc_func)(1, lsd->structsize);
  int j;
  if (!new_struct) return NULL;
  debug_print("copying struct of type: %s, %p -> %p, with size %lu\n", lsd->structtype,
              lsd->top, new_struct,
              lsd->structsize);
  (*memcpy_func)(new_struct, lsd->top, lsd->structsize);

  // copy self_fields first:
  spfields = lsd->self_fields;

#ifdef DO_ALIGN_CHECK
  /// check alignment, else none of this will work
  while (offset + 8 < lsd->structsize) {
    uint64_t *u0, *u1;
    dst_field = (void *)((char *)lsd->top + offset);
    u0 = (uint64_t *)dst_field;
    dst_field = (void *)((char *)new_struct + offset);
    u1 = (uint64_t *)dst_field;
    if (*u0 != *u1) {
      debug_print("ALIGNMENT CHECK FAILED, ABORTING !");
      abort();
    }
    offset += 37;
  }
  debug_print("lives_struct_copy: memory alignment checks passed, OK to proceed\n");
#endif

  debug_print("copying lives_stuct_def_t fields first\n");

recurse_copy:

  if (spfields) {
    for (int i = 0; spfields[i]; i++) {
      lives_special_field_t *spcf = spfields[i];
      size_t bsize = spcf->bytesize;

      if (!spcf->flags) continue;
      if (i) debug_print("field done\n\n");
      offset = (char *)spcf->ptr_to_field - (char *)lsd->top;
      dst_field = (void *)((char *)new_struct + offset);

      debug_print("handling field %s at offset %lu with flags 0X%lX\n",
                  spcf->name, offset, spcf->flags);
      if (spcf->flags & LIVES_FIELD_FLAG_IS_SUBSTRUCT) {
        // otherwise we descend into the substruct and locate its lives_struct_def *
        // TODO...
        debug_print("field is another substruct {TODO}\n");
        continue;
      }
      if (!(spcf->flags & LIVES_FIELD_FLAG_IS_NULLT_ARRAY)) {
        if (spcf->flags & LIVES_FIELD_FLAG_ALLOC_AND_COPY) {
          if (bsize) {
            if (bsize > MAX_ALLOC) {
              debug_print("error: memory request too large (%lu > %lu)\n", bsize, MAX_ALLOC);
              continue;
            } else {
              void **vptr = (void **)dst_field;
              debug_print("allocating %lu bytes...", bsize);
              if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
                *vptr = (*calloc_func)(1, bsize);
                debug_print("and set to zero.\n");
                continue;
              } else {
                if (!(*((void **)spcf->ptr_to_field))) {
                  debug_print("value is NULL, not copying\n");
                } else {
                  debug_print("and copying from src to dest.\n");
                  *vptr = (*calloc_func)(1, bsize);
                  (*memcpy_func)(dst_field, spcf->ptr_to_field, bsize);
                }
              }
            }
          } else {
            // strings
            char **cptr = (char **)dst_field;
            if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
              // set the string to an empty string
              // without ALLOC_AND_COPY we set it to NULL instead
              *cptr = strdup("");
              debug_print("string was set to \"\"\n");

            } else {
              if ((*((char **)spcf->ptr_to_field))) {
                *cptr = strdup(*((char **)spcf->ptr_to_field));
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
          continue;
        }
        // non-alloc
        if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
          if (bsize) {
            (*memset_func)(dst_field, 0, bsize);
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
        continue;
      }
      if (spcf->flags & LIVES_FIELD_FLAG_IS_NULLT_ARRAY) {
        int count = 0;
        debug_print("handling array...");
        // copy / create n elements or strings
        if (!bsize) {
          // copy N strings or create empty strings, source field is char **
          char **dptr;
          char **cptr = (*(char ***)spcf->ptr_to_field);
          if (cptr) {
            while (cptr[count]) count++;
            dptr = (char **)(*calloc_func)(count + 1, sizeof(char *));
            for (int j = 0; j < count; j++) {
              // flags tells us what to with each element
              if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
                dptr[j] = strdup("");
              } else {
                if (spcf->flags & LIVES_FIELD_FLAG_ALLOC_AND_COPY) {
                  dptr[j] = strdup(cptr[j]);
                } else dptr[j] = cptr[j];
              }
            }
            dptr[i] = NULL; /// final element must always be NULL
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
          continue;
        } else {
          if (spcf->flags & (LIVES_FIELD_FLAG_ALLOC_AND_COPY
                             | LIVES_FIELD_FLAG_ZERO_ON_COPY)) {
            /// alloc and copy elements of size bsize
            void **dptr;
            void **vptr = (*(void ***)spcf->ptr_to_field);
            if (vptr) {
              count = 0;
              while (vptr[count]) count++;
              dptr = (void **)(*calloc_func)(count + 1, sizeof(void *));
              for (j = 0; j < count; j++) {
                // flags tells us what to with each element
                if (spcf->flags & LIVES_FIELD_FLAG_ZERO_ON_COPY) {
                  dptr[j] = (*calloc_func)(1, bsize);
                } else {
                  dptr[j] = (*calloc_func)(1, bsize);
                  (*memcpy_func)(dptr[j], vptr[j], bsize);
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
            void *oldarea = *((void **)spcf->ptr_to_field), *newarea;
            char *ptr = oldarea;
            if (ptr) {
              for (count = 0;; count++) {
                for (j = 0; j < bsize; j++) if (ptr[j]) break;
                if (j == bsize) break;
                ptr += bsize;
              }
              count++;
              newarea = (*calloc_func)(count, bsize);
              (*memcpy_func)(newarea, spcf->ptr_to_field, count * bsize);
              *((char **)dst_field) = (char *)newarea;
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
	    }}}}
      // *INDENT-ON*

    debug_print("all fields in struct copied\n\n");

    /// call callbacks
    for (int i = 0; spfields[i]; i++) {
      lives_special_field_t *spcf = spfields[i];
      offset = (char *)spcf->ptr_to_field - (char *)lsd->top;
      dst_field = (void *)((char *)new_struct + offset);
      if (spcf->copy_func) {
        debug_print("calling copy_func for %s\n", spcf->name);
        (*spcf->copy_func)(new_struct, lsd->top,
                           (spfields == lsd->special_fields)
                           ? lsd->structtype : SELF_STRUCT_TYPE,
                           spcf->name,  dst_field, spcf->ptr_to_field);
      }
      if (spcf->update_func)(*spcf->update_func)(new_struct,
            (spfields == lsd->special_fields)
            ? lsd->structtype : SELF_STRUCT_TYPE,
            spcf->name, dst_field);
    }
  }

  if (spfields != lsd->special_fields) {
    // after copying structdef, we copy the normal fields
    spfields = lsd->special_fields;
    debug_print("copying normal fields\n");
    goto recurse_copy;
  } debug_print("struct copy done\n\n");
  return new_struct;
}

/// after creating a struct with a lives_struct_def_t field, ths funtion can be called to
/// set up the struct_def_t. Parameters are a pointer to the struct, a pointer to the
/// struct_def_t field, struct type as text, sizeof struct, name of the last field (optional but
/// may be useful for checking if fields have been added), and the number of "special fields"
/// these fields are held in the lives_special_field_t array returned (a NULL final element is
/// also added, not included in the count). Special fields are any which require special methods
/// for copying or deleting). Some flag bits can be set for standard methods, as well as adding
/// callbacks. For fixed size fields, bytesize can be set, or left at zero for nul terminated
/// strinhg.
/// once set up. it is a simple matter to call lives_struct_copy and lives_struct_free.

static lives_special_field_t **make_structdef(void *mystruct, lives_struct_def_t *lsd,
    const char *struct_type,
    size_t struct_size, const char *last_field,
    int nspecial) {
  lives_special_field_t **xspecf;
  if (!lsd->identifier) lsd->identifier = LIVES_STRUCT_ID;
  if (!lsd->unique_id) IGN_RET(getentropy(&lsd->unique_id, 8));
  if (struct_type)
    snprintf(lsd->structtype, TEXTLEN, "%s", struct_type);
  lsd->top = mystruct;
  lsd->structsize = struct_size;
  if (last_field)
    snprintf(lsd->last_field, TEXTLEN, "%s", last_field);
  if (nspecial > 0) {
    lsd->special_fields =
      (lives_special_field_t **)(*calloc_func)(nspecial + 1, sizeof(lives_special_field_t *));
    lsd->special_fields[nspecial] = NULL;
  } else lsd->special_fields = NULL;
  xspecf = lsd->self_fields =
             (lives_special_field_t **)(*calloc_func)(6, sizeof(lives_special_field_t *));

  // set a new random value on copy
  xspecf[0] = make_special_field(0, &lsd->unique_id, "unique_id", 0, gen_copy, NULL, NULL);

  // repoint to copy struct on copy
  xspecf[1] = make_special_field(0, &lsd->top, "top", 0, gen_copy, NULL, NULL);

  // values will be alloced and copied to a copy struct,
  // gen_copy will then adjust the ptr_to_field values
  xspecf[2] = make_special_field(LIVES_FIELD_PTR_ARRAY,
                                 &lsd->special_fields, "special_fields",
                                 sizeof(lives_special_field_t), gen_copy, NULL,
                                 NULL);

  // value will be set to zero after copying
  xspecf[3] = make_special_field(LIVES_FIELD_FLAG_ZERO_ON_COPY,
                                 &lsd->user_data, "user_data", 8, NULL, NULL, NULL);

  // this needs to be last, so that gen_delete can free the values last
  // ideally we would have liked to have put COPY_POINTERS here and
  // put "top" after this with FREE_ON_DELETE set there.
  // will be alloced and copied on copy, gen_copy will adjust ptr_to_field values
  // gen delete will free all as a final action before the entire struct is freed

  xspecf[4] = make_special_field(LIVES_FIELD_FLAG_ALLOC_AND_COPY
                                 | LIVES_FIELD_FLAG_IS_NULLT_ARRAY,
                                 &lsd->self_fields, "self_fields",
                                 sizeof(lives_special_field_t),
                                 gen_copy, gen_delete,
                                 NULL);

  xspecf[5] = NULL;

  lsd->user_data = NULL;
  return lsd->special_fields;
}

///// API ////////////////////

// after creating the struct, call make_structdef passsing a pointer to the struct,
// a pointer to structdef field and the type
static lives_special_field_t **make_structdef(void *mystruct, lives_struct_def_t *lsd,
    const char *struct_type,
    size_t struct_size, const char *last_field,
    int nspecial) ALLOW_UNUSED;

// function to define special fields, array elements returned from make_structdef
// should be assigned via this function
static lives_special_field_t *make_special_field(uint64_t flags, void *ptr_to_field,
    const char *field_name,
    size_t data_size,
    lives_field_copy_f copy_func,
    lives_field_delete_f delete_func,
    lives_field_update_f update_func) ALLOW_UNUSED;

// allocates and returns a copy of strct, calls copy_funcs, fills in lives_sttuct_def for copy
static void *lives_struct_copy(lives_struct_def_t *lsd) ALLOW_UNUSED;

// calls free_funcs, then frees strct
static void lives_struct_free(lives_struct_def_t *lsd) ALLOW_UNUSED;

/////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
#undef DEBUG
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
