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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2015 */

// special version of libweed with fixed function defs. - for inclusion in other dynamic libs (e.g. liblives)

#ifndef _WEED_VISIBLE_FUNCTIONS_
#define _WEED_VISIBLE_FUNCTIONS_
#endif

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#else
#include "weed.h"
#include "weed-host.h"
#endif

#include <glib.h>

#ifdef CHECK_CALLER
#include <stdio.h>
#endif


#include <string.h> // for malloc, memset, memcpy
#include <stdlib.h> // for free


#if GLIB_CHECK_VERSION(2,14,0) == FALSE
static inline gpointer g_slice_copy(gsize bsize, gconstpointer block) {
  gpointer ret=g_slice_alloc(bsize);
  memcpy(ret,block,bsize);
  return ret;
}
#endif


static inline size_t weed_strlen(const char *string) {
  size_t len=0;
  size_t maxlen=(size_t)-2;
  while (*(string++)!=0&&(len!=maxlen)) len++;
  return len;
}

static inline char *weed_strdup(const char *string) {
  size_t len;
  char *ret=(char *)malloc((len=weed_strlen(string))+1);

  memcpy(ret,string,len+1);
  return ret;
}

static inline char *weed_slice_strdup(const char *string) {
  return (char *)g_slice_copy(weed_strlen(string)+1,string);
}

static inline int weed_strcmp(const char *st1, const char *st2) {
  while (!(*st1==0&&*st2==0)) {
    if (*(st1)==0||*(st2)==0||*(st1++)!=*(st2++)) return 1;
  }
  return 0;
}


static inline int weed_seed_is_ptr(int seed) {
  return (seed!=WEED_SEED_BOOLEAN&&seed!=WEED_SEED_INT&&seed!=WEED_SEED_DOUBLE&&seed!=WEED_SEED_STRING&&seed!=
          WEED_SEED_INT64)?1:0;
}

static inline size_t weed_seed_get_size(int seed, void *value) {
  return weed_seed_is_ptr(seed)?(sizeof(void *)):\
         (seed==WEED_SEED_BOOLEAN||seed==WEED_SEED_INT)?4:\
         (seed==WEED_SEED_DOUBLE)?8:\
         (seed==WEED_SEED_INT64)?8:\
         (seed==WEED_SEED_STRING)?weed_strlen((const char *)value):0;
}

static inline void weed_data_free(weed_data_t **data, int num_elems, int seed_type) {
  register int i;
  for (i=0; i<num_elems; i++) {
    if (!weed_seed_is_ptr(seed_type)||(seed_type==WEED_SEED_STRING&&data[i]->value!=NULL))
      g_slice_free1(data[i]->size,data[i]->value);
    g_slice_free(weed_data_t,data[i]);
  }
  g_slice_free1(num_elems*sizeof(weed_data_t *),data);
}

static inline weed_data_t **weed_data_new(int seed_type, int num_elems, void *value) {
  register int i;
  weed_data_t **data=NULL;
  size_t size;
  char **valuec=(char **)value;
  void **valuev=(void **)value;

  if (num_elems==0) return data;
  if ((data=(weed_data_t **)g_slice_alloc(num_elems*sizeof(weed_data_t *)))==NULL) return ((weed_data_t **)0);
  for (i=0; i<num_elems; i++) {
    if ((data[i]=g_slice_new(weed_data_t))==NULL) {
      weed_data_free(data,--i,seed_type);
      return NULL;
    }
    if (weed_seed_is_ptr(seed_type)) data[i]->value=valuev[i];
    else {
      if (seed_type==WEED_SEED_STRING) {
        size=weed_strlen(valuec[i]);
        if (size>0) data[i]->value=g_slice_copy(size,valuec[i]);
        else data[i]->value=NULL;
        data[i]->size=size;
      } else {
        size=weed_seed_get_size(seed_type,NULL);
        data[i]->value=g_slice_copy(size,(char *)value+i*size);
      }
      if (size>0&&data[i]->value==NULL) { // memory error
        weed_data_free(data,--i,seed_type);
        return NULL;
      }
    }
    if (seed_type!=WEED_SEED_STRING) data[i]->size=weed_seed_get_size(seed_type,data[i]->value);
  }
  return data;
}

static inline weed_leaf_t *weed_find_leaf(weed_plant_t *leaf, const char *key) {
  while (leaf!=NULL) {
    if (!weed_strcmp((char *)leaf->key,(char *)key)) return leaf;
    leaf=leaf->next;
  }
  return NULL;
}

static inline void weed_leaf_free(weed_leaf_t *leaf) {
  weed_data_free(leaf->data,leaf->num_elements,leaf->seed_type);
  g_slice_free1(weed_strlen(leaf->key)+1,(void *)leaf->key);
  g_slice_free(weed_leaf_t,leaf);
}

static inline weed_leaf_t *weed_leaf_new(const char *key, int seed) {
  weed_leaf_t *leaf=g_slice_new(weed_leaf_t);
  if (leaf==NULL) return NULL;
  if ((leaf->key=weed_slice_strdup(key))==NULL) {
    g_slice_free(weed_leaf_t,leaf);
    return NULL;
  }
  leaf->seed_type=seed;
  leaf->data=NULL;
  leaf->next=NULL;
  leaf->num_elements=leaf->flags=0;
  return leaf;
}

static inline void weed_leaf_append(weed_plant_t *leaf, weed_leaf_t *newleaf) {
  weed_leaf_t *leafnext;
  while (leaf!=NULL) {
    if ((leafnext=leaf->next)==NULL) {
      leaf->next=newleaf;
      return;
    }
    leaf=leafnext;
  }
}

void weed_plant_free(weed_plant_t *leaf) {
  weed_leaf_t *leafnext;
  while (leaf!=NULL) {
    leafnext=leaf->next;
    weed_leaf_free(leaf);
    leaf=leafnext;
  }
}


int weed_leaf_delete(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf=plant,*leafnext;
  while (leaf->next!=NULL) {
    if (!weed_strcmp((char *)leaf->next->key,(char *)key)) {
      if (leaf->next->flags&WEED_LEAF_READONLY_HOST) return WEED_ERROR_LEAF_READONLY;
      leafnext=leaf->next;
      leaf->next=leaf->next->next;
      weed_leaf_free(leafnext);
      return WEED_NO_ERROR;
    }
    leaf=leaf->next;
  }
  return WEED_ERROR_NOSUCH_LEAF;
}


int weed_leaf_set_flags(weed_plant_t *plant, const char *key, int flags) {
  weed_leaf_t *leaf=weed_find_leaf(plant, key);
  if (leaf==NULL) return WEED_ERROR_NOSUCH_LEAF;
  leaf->flags=flags;
  return WEED_NO_ERROR;
}

weed_plant_t *weed_plant_new(int plant_type) {
  weed_leaf_t *leaf;
  if ((leaf=weed_leaf_new("type",WEED_SEED_INT))==NULL) return NULL;
  if ((leaf->data=(weed_data_t **)weed_data_new(WEED_SEED_INT,1,&plant_type))==NULL) {
    g_slice_free1(weed_strlen(leaf->key)+1,(void *)leaf->key);
    g_slice_free(weed_leaf_t,leaf);
    return NULL;
  }
  leaf->num_elements=1;
  leaf->next=NULL;
  weed_leaf_set_flags(leaf,"type",WEED_LEAF_READONLY_PLUGIN|WEED_LEAF_READONLY_HOST);
  return leaf;
}

char **weed_plant_list_leaves(weed_plant_t *plant) {
  weed_leaf_t *leaf=plant;
  char **leaflist;
  register int i=1;
  for (; leaf!=NULL; i++) {
    leaf=leaf->next;
  }
  if ((leaflist=(char **)malloc(i*sizeof(char *)))==NULL) return NULL;
  i=0;
  for (leaf=plant; leaf!=NULL; leaf=leaf->next) {
    if ((leaflist[i]=weed_strdup(leaf->key))==NULL) {
      for (--i; i>=0; i--) free(leaflist[i]);
      free(leaflist);
      return NULL;
    }
    i++;
  }
  leaflist[i]=NULL;
  return leaflist;
}

static inline int _weed_leaf_set_caller(weed_plant_t *plant, const char *key, int seed_type, int num_elems,
                                        void *value, int caller) {
#ifdef CHECK_CALLER
  printf("caller was %d\n",caller);
#endif
  weed_data_t **data=NULL;
  weed_leaf_t *leaf=weed_find_leaf(plant,key);
  if (leaf==NULL) {
    if ((leaf=weed_leaf_new(key,seed_type))==NULL) return WEED_ERROR_MEMORY_ALLOCATION;
    weed_leaf_append(plant,leaf);
  } else {
    if ((caller==WEED_CALLER_PLUGIN&&leaf->flags&WEED_LEAF_READONLY_PLUGIN)||
        (caller==WEED_CALLER_HOST&&leaf->flags&WEED_LEAF_READONLY_HOST)) return WEED_ERROR_LEAF_READONLY;
    if (seed_type!=leaf->seed_type) return WEED_ERROR_WRONG_SEED_TYPE;
    weed_data_free(leaf->data,leaf->num_elements,seed_type);
    leaf->data=NULL;
  }
  leaf->num_elements=0;
  if (num_elems>0&&(data=weed_data_new(seed_type,num_elems,value))==NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  leaf->data=data;
  leaf->num_elements=num_elems;
  return WEED_NO_ERROR;
}


int weed_leaf_set(weed_plant_t *plant, const char *key, int seed_type, int num_elems, void *value) {
  // host version
  return _weed_leaf_set_caller(plant,key,seed_type,num_elems,value,WEED_CALLER_HOST);
}


int weed_leaf_set_plugin(weed_plant_t *plant, const char *key, int seed_type, int num_elems, void *value) {
  // plugin version - host should pass this to plugin in host_info
  return _weed_leaf_set_caller(plant,key,seed_type,num_elems,value,WEED_CALLER_PLUGIN);
}


int weed_default_get(weed_plant_t *plant, const char *key, int idx, void *value) {
  // the plugin should use this only when bootstrapping in order to get its memory functions
  // the actual weed_leaf_get

  // here we must assume that the plugin does not yet have its memory functions, so we can only
  // use the standard ones

  // additionally, the prototype of this function must never change

  weed_leaf_t *leaf=weed_find_leaf(plant,key);
  if (leaf==NULL||idx>leaf->num_elements) return WEED_ERROR_NOSUCH_LEAF;
  if (value==NULL) return WEED_NO_ERROR;
  if (weed_seed_is_ptr(leaf->seed_type)) memcpy(value,&leaf->data[idx]->value,sizeof(void *));
  else {
    if (leaf->seed_type==WEED_SEED_STRING) {
      size_t size=leaf->data[idx]->size;
      char **valuecharptrptr=(char **)value;
      if (size>0) memcpy(*valuecharptrptr,leaf->data[idx]->value,size);
      memset(*valuecharptrptr+size,0,1);
    } else memcpy(value,leaf->data[idx]->value,weed_seed_get_size(leaf->seed_type,leaf->data[idx]->value));
  }
  return WEED_NO_ERROR;

}



int weed_leaf_get(weed_plant_t *plant, const char *key, int idx, void *value) {
  weed_leaf_t *leaf=weed_find_leaf(plant,key);
  if (leaf==NULL||idx>leaf->num_elements) return WEED_ERROR_NOSUCH_LEAF;
  if (value==NULL) return WEED_NO_ERROR;
  if (weed_seed_is_ptr(leaf->seed_type)) memcpy(value,&leaf->data[idx]->value,sizeof(void *));
  else {
    if (leaf->seed_type==WEED_SEED_STRING) {
      size_t size=leaf->data[idx]->size;
      char **valuecharptrptr=(char **)value;
      if (size>0) memcpy(*valuecharptrptr,leaf->data[idx]->value,size);
      memset(*valuecharptrptr+size,0,1);
    } else memcpy(value,leaf->data[idx]->value,weed_seed_get_size(leaf->seed_type,leaf->data[idx]->value));
  }
  return WEED_NO_ERROR;
}

int weed_leaf_num_elements(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf=weed_find_leaf(plant, key);
  if (leaf==NULL) return 0;
  return leaf->num_elements;
}

size_t weed_leaf_element_size(weed_plant_t *plant, const char *key, int idx) {
  weed_leaf_t *leaf=weed_find_leaf(plant, key);
  if (leaf==NULL||idx>leaf->num_elements) return 0;
  return leaf->data[idx]->size;
}

int weed_leaf_seed_type(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf=weed_find_leaf(plant, key);
  if (leaf==NULL) return 0;
  return leaf->seed_type;
}

int weed_leaf_get_flags(weed_plant_t *plant, const char *key) {
  weed_leaf_t *leaf=weed_find_leaf(plant, key);
  if (leaf==NULL) return 0;
  return leaf->flags;
}


void weed_free(void *p) {
  free(p);
}

void *weed_memset(void *p, int c, size_t n) {
  return memset(p,c,n);
}

void *weed_malloc(size_t n) {
  return malloc(n);
}

void *weed_memcpy(void *dest, const void *src, size_t n) {
  return memcpy(dest,src,n);
}
