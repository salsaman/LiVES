// bundles.h
// LiVES
// (c) G. Finch 2003-2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _BUNDLES_H
#define _BUNDLES_H

typedef weed_plant_t bundle_t;
typedef bundle_t nirva_bundle_t;

// implementation defaults and overrides
#define NIRVA_BUNDLE_T weed_plant_t
#define IMPL_FUNC_has_item bundle_has_item
///

char *get_short_name(const char *q);
//uint64_t get_vflags(const char *q, off_t *offx);
uint32_t get_vtype(const char *q, off_t *offx);
const char *get_vname(const char *q);
boolean get_is_array(const char *q);

bundle_t *create_bundle_by_type(bundle_type btype, ...) LIVES_SENTINEL;

bundle_t *var_bundle_from_bundledef(bundledef_t bdef, ...);
bundle_t *def_bundle_from_bundledef(bundledef_t bdef);

bundle_t *create_object_bundle(uint64_t otype, uint64_t subtype);

boolean bundle_has_item(bundle_t *, const char *item);

bundledef_t get_bundledef_from_bundle(bundle_t *);

size_t bundle_array_append(bundle_t *bundle, const char *name, int ne, int *tot_elems, void *thing);

char *flatten_bundledef(bundledef_t);
char *flatten_bundle(bundle_t *);

uint64_t get_bundledef64sum(bundledef_t bdef, char **flattened);
uint64_t get_bundle64sum(bundle_t *, char **flattened);

boolean set_bundle_value(bundle_t *, const char *name, ...);
boolean set_bundle_val(bundle_t *, const char *name, uint32_t vtype,
                       uint32_t ne, boolean array, ...);

char *bundle_to_header(bundle_t *, const char *tname);

#endif
