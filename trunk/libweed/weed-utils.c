/* WEED is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   Weed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this source code; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA


   Weed is developed by:

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net


   mainly based on LiViDO, which is developed by:


   Niels Elburg - http://veejay.sf.net

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

   Denis "Jaromil" Rojo - http://freej.dyne.org

   Tom Schouten - http://zwizwa.fartit.com

   Andraz Tori - http://cvs.cinelerra.org

   reviewed with suggestions and contributions from:

   Silvano "Kysucix" Galliani - http://freej.dyne.org

   Kentaro Fukuchi - http://megaui.net/fukuchi

   Jun Iio - http://www.malib.net

   Carlo Prelz - http://www2.fluido.as:8080/

*/

/* (C) Gabriel "Salsaman" Finch, 2005 - 2010 */

#include <string.h>

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#else
#include "weed.h"
#endif


extern weed_leaf_set_f weed_leaf_set;
extern weed_leaf_get_f weed_leaf_get;
extern weed_plant_new_f weed_plant_new;
extern weed_plant_list_leaves_f weed_plant_list_leaves;
extern weed_leaf_num_elements_f weed_leaf_num_elements;
extern weed_leaf_element_size_f weed_leaf_element_size;
extern weed_leaf_seed_type_f weed_leaf_seed_type;
extern weed_leaf_get_flags_f weed_leaf_get_flags;
extern weed_malloc_f weed_malloc;
extern weed_free_f weed_free;
extern weed_memcpy_f weed_memcpy;
extern weed_memset_f weed_memset;



/////////////////////////////////////////////////////////////////

int weed_plant_has_leaf(weed_plant_t *plant, const char *key) {
  if (weed_leaf_get(plant,key,0,NULL)==WEED_ERROR_NOSUCH_LEAF) return WEED_FALSE;
  return WEED_TRUE;
}

/////////////////////////////////////////////////////////////////
// leaf setters

int weed_set_int_value(weed_plant_t *plant, const char *key, int value) {
  // returns a WEED_ERROR
  return weed_leaf_set(plant,key,WEED_SEED_INT,1,&value);
}

int weed_set_double_value(weed_plant_t *plant, const char *key, double value) {
  // returns a WEED_ERROR
  return weed_leaf_set(plant,key,WEED_SEED_DOUBLE,1,&value);
}

int weed_set_boolean_value(weed_plant_t *plant, const char *key, int value) {
  // returns a WEED_ERROR
  return weed_leaf_set(plant,key,WEED_SEED_BOOLEAN,1,&value);
}

int weed_set_int64_value(weed_plant_t *plant, const char *key, int64_t value) {
  // returns a WEED_ERROR
  return weed_leaf_set(plant,key,WEED_SEED_INT64,1,&value);
}

int weed_set_string_value(weed_plant_t *plant, const char *key, const char *value) {
  // returns a WEED_ERROR
  return weed_leaf_set(plant,key,WEED_SEED_STRING,1,&value);
}

int weed_set_plantptr_value(weed_plant_t *plant, const char *key, weed_plant_t *value) {
  // returns a WEED_ERROR
  return weed_leaf_set(plant,key,WEED_SEED_PLANTPTR,1,&value);
}

int weed_set_voidptr_value(weed_plant_t *plant, const char *key, void *value) {
  // returns a WEED_ERROR
  return weed_leaf_set(plant,key,WEED_SEED_VOIDPTR,1,&value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// general leaf getter

inline int weed_get_value(weed_plant_t *plant, const char *key, void *value) {
  // returns a WEED_ERROR
  return weed_leaf_get(plant, key, 0, value);
}

////////////////////////////////////////////////////////////

int weed_get_int_value(weed_plant_t *plant, const char *key, int *error) {
  int retval=0;
  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_INT) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return retval;
  } else *error=weed_get_value(plant,key,&retval);
  return retval;
}

double weed_get_double_value(weed_plant_t *plant, const char *key, int *error) {
  double retval=0.;
  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_DOUBLE) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return retval;
  }
  *error=weed_get_value(plant,key,&retval);
  return retval;
}

int weed_get_boolean_value(weed_plant_t *plant, const char *key, int *error) {
  int retval=0;
  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_BOOLEAN) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return retval;
  }
  *error=weed_get_value(plant,key,&retval);
  return retval;
}

int64_t weed_get_int64_value(weed_plant_t *plant, const char *key, int *error) {
  int64_t retval=0;
  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_INT64) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return retval;
  }
  *error=weed_get_value(plant,key,&retval);
  return retval;
}

char *weed_get_string_value(weed_plant_t *plant, const char *key, int *error) {
  size_t size;
  char *retval=NULL;
  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_STRING) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }
  if ((retval=(char *)weed_malloc((size=weed_leaf_element_size(plant,key,0))+1))==NULL) {
    *error=WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }
  if ((*error=weed_get_value(plant,key,&retval))!=WEED_NO_ERROR) {
    weed_free(retval);
    return NULL;
  }
  weed_memset(retval+size,0,1);
  return retval;
}

void *weed_get_voidptr_value(weed_plant_t *plant, const char *key, int *error) {
  void *retval=NULL;
  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_VOIDPTR) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return retval;
  }
  *error=weed_get_value(plant,key,&retval);
  return retval;
}

weed_plant_t *weed_get_plantptr_value(weed_plant_t *plant, const char *key, int *error) {
  weed_plant_t *retval=NULL;
  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_PLANTPTR) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return retval;
  }
  *error=weed_get_value(plant,key,&retval);
  return retval;
}


////////////////////////////////////////////////////////////

int *weed_get_int_array(weed_plant_t *plant, const char *key, int *error) {
  int i;
  int num_elems;
  int *retval;

  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_INT) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }

  if ((num_elems=weed_leaf_num_elements(plant,key))==0) return NULL;

  if ((retval=(int *)weed_malloc(num_elems*4))==NULL) {
    *error=WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i=0; i<num_elems; i++) {
    if ((*error=weed_leaf_get(plant, key, i, &retval[i]))!=WEED_NO_ERROR) {
      weed_free(retval);
      return NULL;
    }
  }
  return retval;
}

double *weed_get_double_array(weed_plant_t *plant, const char *key, int *error) {
  int i;
  int num_elems;
  double *retval;

  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_DOUBLE) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }
  if ((num_elems=weed_leaf_num_elements(plant,key))==0) return NULL;

  if ((retval=(double *)weed_malloc(num_elems*sizeof(double)))==NULL) {
    *error=WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i=0; i<num_elems; i++) {
    if ((*error=weed_leaf_get(plant, key, i, &retval[i]))!=WEED_NO_ERROR) {
      weed_free(retval);
      return NULL;
    }
  }
  return retval;
}

int *weed_get_boolean_array(weed_plant_t *plant, const char *key, int *error) {
  int i;
  int num_elems;
  int *retval;

  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_BOOLEAN) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }

  if ((num_elems=weed_leaf_num_elements(plant,key))==0) return NULL;

  if ((retval=(int *)weed_malloc(num_elems*4))==NULL) {
    *error=WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i=0; i<num_elems; i++) {
    if ((*error=weed_leaf_get(plant, key, i, &retval[i]))!=WEED_NO_ERROR) {
      weed_free(retval);
      return NULL;
    }
  }
  return retval;
}

int64_t *weed_get_int64_array(weed_plant_t *plant, const char *key, int *error) {
  int i;
  int num_elems;
  int64_t *retval;

  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_INT64) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }

  if ((num_elems=weed_leaf_num_elements(plant,key))==0) return NULL;

  if ((retval=(int64_t *)weed_malloc(num_elems*8))==NULL) {
    *error=WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i=0; i<num_elems; i++) {
    if ((*error=weed_leaf_get(plant, key, i, &retval[i]))!=WEED_NO_ERROR) {
      weed_free(retval);
      return NULL;
    }
  }
  return retval;
}

char **weed_get_string_array(weed_plant_t *plant, const char *key, int *error) {
  int i;
  int num_elems;
  char **retval;
  size_t size;

  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_STRING) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }

  if ((num_elems=weed_leaf_num_elements(plant,key))==0) return NULL;

  if ((retval=(char **)weed_malloc(num_elems*sizeof(char *)))==NULL) {
    *error=WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i=0; i<num_elems; i++) {
    if ((retval[i]=(char *)weed_malloc((size=weed_leaf_element_size(plant,key,i))+1))==NULL) {
      for (--i; i>=0; i--) weed_free(retval[i]);
      *error=WEED_ERROR_MEMORY_ALLOCATION;
      weed_free(retval);
      return NULL;
    }
    if ((*error=weed_leaf_get(plant, key, i, &retval[i]))!=WEED_NO_ERROR) {
      for (--i; i>=0; i--) weed_free(retval[i]);
      weed_free(retval);
      return NULL;
    }
    weed_memset(retval[i]+size,0,1);
  }
  return retval;
}

void **weed_get_voidptr_array(weed_plant_t *plant, const char *key, int *error) {
  int i;
  int num_elems;
  void **retval;

  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_VOIDPTR) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }

  if ((num_elems=weed_leaf_num_elements(plant,key))==0) return NULL;

  if ((retval=(void **)weed_malloc(num_elems*sizeof(void *)))==NULL) {
    *error=WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i=0; i<num_elems; i++) {
    if ((*error=weed_leaf_get(plant, key, i, &retval[i]))!=WEED_NO_ERROR) {
      weed_free(retval);
      return NULL;
    }
  }
  return retval;
}

weed_plant_t **weed_get_plantptr_array(weed_plant_t *plant, const char *key, int *error) {
  int i;
  int num_elems;
  weed_plant_t **retval;

  if (weed_plant_has_leaf(plant,key)&&weed_leaf_seed_type(plant,key)!=WEED_SEED_PLANTPTR) {
    *error=WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }

  if ((num_elems=weed_leaf_num_elements(plant,key))==0) return NULL;

  if ((retval=(weed_plant_t **)weed_malloc(num_elems*sizeof(weed_plant_t *)))==NULL) {
    *error=WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i=0; i<num_elems; i++) {
    if ((*error=weed_leaf_get(plant, key, i, &retval[i]))!=WEED_NO_ERROR) {
      weed_free(retval);
      return NULL;
    }
  }
  return retval;
}

/////////////////////////////////////////////////////

int weed_set_int_array(weed_plant_t *plant, const char *key, int num_elems, int *values) {
  return weed_leaf_set(plant,key,WEED_SEED_INT,num_elems,values);
}

int weed_set_double_array(weed_plant_t *plant, const char *key, int num_elems, double *values) {
  return weed_leaf_set(plant,key,WEED_SEED_DOUBLE,num_elems,values);
}

int weed_set_boolean_array(weed_plant_t *plant, const char *key, int num_elems, int *values) {
  return weed_leaf_set(plant,key,WEED_SEED_BOOLEAN,num_elems,values);
}

int weed_set_int64_array(weed_plant_t *plant, const char *key, int num_elems, int64_t *values) {
  return weed_leaf_set(plant,key,WEED_SEED_INT64,num_elems,values);
}

int weed_set_string_array(weed_plant_t *plant, const char *key, int num_elems, char **values) {
  return weed_leaf_set(plant,key,WEED_SEED_STRING,num_elems,values);
}

int weed_set_voidptr_array(weed_plant_t *plant, const char *key, int num_elems, void **values) {
  return weed_leaf_set(plant,key,WEED_SEED_VOIDPTR,num_elems,values);
}

int weed_set_plantptr_array(weed_plant_t *plant, const char *key, int num_elems, weed_plant_t **values) {
  return weed_leaf_set(plant,key,WEED_SEED_PLANTPTR,num_elems,values);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////


int weed_leaf_copy(weed_plant_t *dst, const char *keyt, weed_plant_t *src, const char *keyf) {
  int num,seed_type,i,error;
  int *datai;
  double *datad;
  char **datac;
  int64_t *datai6;
  void **datav;
  weed_plant_t **datap;

  if (weed_plant_has_leaf(src,keyf)==WEED_FALSE) return WEED_ERROR_NOSUCH_LEAF;

  seed_type=weed_leaf_seed_type(src,keyf);
  num=weed_leaf_num_elements(src,keyf);

  if (num==0) weed_leaf_set(dst,keyt,seed_type,0,NULL);
  else {
    switch (seed_type) {
    case WEED_SEED_INT:
      datai=weed_get_int_array(src,keyf,&error);
      error=weed_set_int_array(dst,keyt,num,datai);
      weed_free(datai);
      break;
    case WEED_SEED_INT64:
      datai6=weed_get_int64_array(src,keyf,&error);
      error=weed_set_int64_array(dst,keyt,num,datai6);
      weed_free(datai6);
      break;
    case WEED_SEED_BOOLEAN:
      datai=weed_get_boolean_array(src,keyf,&error);
      error=weed_set_boolean_array(dst,keyt,num,datai);
      weed_free(datai);
      break;
    case WEED_SEED_DOUBLE:
      datad=weed_get_double_array(src,keyf,&error);
      error=weed_set_double_array(dst,keyt,num,datad);
      weed_free(datad);
      break;
    case WEED_SEED_VOIDPTR:
      datav=weed_get_voidptr_array(src,keyf,&error);
      error=weed_set_voidptr_array(dst,keyt,num,datav);
      weed_free(datav);
      break;
    case WEED_SEED_PLANTPTR:
      datap=weed_get_plantptr_array(src,keyf,&error);
      error=weed_set_plantptr_array(dst,keyt,num,datap);
      weed_free(datap);
      break;
    case WEED_SEED_STRING:
      datac=weed_get_string_array(src,keyf,&error);
      error=weed_set_string_array(dst,keyt,num,datac);
      for (i=0; i<num; i++) {
        weed_free(datac[i]);
      }
      weed_free(datac);
      break;
    }
  }
  return error;
}


weed_plant_t *weed_plant_copy(weed_plant_t *src) {
  int i=0,error;
  char **proplist=weed_plant_list_leaves(src);
  char *prop;
  weed_plant_t *plant=weed_plant_new(weed_get_int_value(src,"type",&error));
  if (plant==NULL) return NULL;

  for (prop=proplist[0]; (prop=proplist[i])!=NULL&&error==WEED_NO_ERROR; i++) {
    if (strcmp(prop,"type")) error=weed_leaf_copy(plant,prop,src,prop);
    weed_free(prop);
  }
  weed_free(proplist);

  if (error==WEED_ERROR_MEMORY_ALLOCATION) {
    //if (plant!=NULL) weed_plant_free(plant); // well, we should free the plant, but the plugins don't have this function...
    return NULL;
  }

  return plant;
}


int weed_get_plant_type(weed_plant_t *plant) {
  int error;
  return weed_get_int_value(plant,"type",&error);
}



