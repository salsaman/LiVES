// bundles.h
// LiVES
// (c) G. Finch 2003-2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#ifndef _BUNDLES_H
#define _BUNDLES_H

char *get_short_name(const char *q);
uint64_t get_vflags(const char *q, off_t *offx);
uint32_t get_vtype(const char *q, off_t *offx);
const char *get_vname(const char *q);
boolean get_is_array(const char *q);

lives_bundle_t *create_bundle(bundle_type btype, ...) LIVES_SENTINEL;

lives_bundle_t *create_object_bundle(uint64_t otype, uint64_t subtype);

boolean bundle_has_item(lives_bundle_t *, const char *item);

bundledef_t get_bundledef_from_bundle(lives_bundle_t *);

char *flatten_bundledef(bundledef_t);
char *flatten_bundle(lives_bundle_t *);

uint64_t get_bundledef64sum(bundledef_t bdef, char **flattened);
uint64_t get_bundle64sum(lives_bundle_t *, char **flattened);

boolean set_bundle_value(lives_bundle_t *, const char *name, ...);
boolean set_bundle_val(lives_bundle_t *, const char *name, uint32_t vtype,
                       uint32_t ne, boolean array, ...);

#endif
